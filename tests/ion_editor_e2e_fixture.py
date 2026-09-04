#!/usr/bin/env python3
"""Local OAuth/ion fixture and non-interactive browser for editor E2E tests."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import time
from urllib.parse import parse_qs, urlencode, urlparse
from urllib.request import urlopen


def _jwt() -> str:
    now = int(time.time())
    payload = base64.b64encode(
        json.dumps({"id": 222, "iat": now, "exp": now + 3600}).encode()
    ).decode().rstrip("=")
    return f"fixture-header.{payload}.fixture-signature"


class IonFixtureHandler(BaseHTTPRequestHandler):
    authorization: dict[str, str] = {}
    events_file: Path
    access_token = _jwt()

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def _event(self, name: str, **details: object) -> None:
        with self.events_file.open("a", encoding="utf-8") as output:
            output.write(json.dumps({"event": name, **details}) + "\n")

    def _json(self, body: object, status: int = 200) -> None:
        payload = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def _require_bearer(self) -> bool:
        if self.headers.get("Authorization") != f"Bearer {self.access_token}":
            self._json({"error": "invalid_token"}, 401)
            return False
        return True

    def _read_body(self) -> bytes:
        if self.headers.get("Transfer-Encoding", "").lower() != "chunked":
            return self.rfile.read(int(self.headers.get("Content-Length", "0")))
        chunks: list[bytes] = []
        while True:
            size_line = self.rfile.readline().split(b";", 1)[0].strip()
            size = int(size_line, 16)
            if size == 0:
                self.rfile.readline()
                break
            chunks.append(self.rfile.read(size))
            self.rfile.read(2)
        return b"".join(chunks)

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/appData":
            self._event("app_data")
            self._json({"applicationMode": "cesium-ion", "dataStoreType": "S3"})
            return
        if parsed.path == "/oauth":
            query = {key: values[0] for key, values in parse_qs(parsed.query).items()}
            required = {
                "response_type", "client_id", "scope", "redirect_uri", "state",
                "code_challenge_method", "code_challenge",
            }
            if not required.issubset(query) or query["code_challenge_method"] != "S256":
                self._json({"error": "invalid_authorization_request"}, 400)
                return
            type(self).authorization = query
            self._event(
                "authorize",
                state_length=len(query["state"]),
                challenge_length=len(query["code_challenge"]),
                redirect_uri=query["redirect_uri"],
            )
            separator = "&" if "?" in query["redirect_uri"] else "?"
            location = query["redirect_uri"] + separator + urlencode(
                {"code": "fixture-code", "state": query["state"]}
            )
            self.send_response(302)
            self.send_header("Location", location)
            self.end_headers()
            return
        if parsed.path == "/v1/me":
            if not self._require_bearer():
                return
            self._event("profile")
            self._json({
                "id": 222,
                "username": "fixture-user",
                "email": "fixture@example.invalid",
                "emailVerified": True,
                "scopes": ["assets:list", "assets:read", "profile:read", "tokens:read", "tokens:write"],
                "storage": {"used": 0, "available": 1000, "total": 1000},
            })
            return
        if parsed.path == "/v1/assets":
            if not self._require_bearer():
                return
            self._event("assets")
            self._json({"items": [{
                "id": 101, "name": "Fixture Tileset", "description": "E2E fixture",
                "attribution": "", "type": "3DTILES", "bytes": 42,
                "dateAdded": "2026-01-01T00:00:00Z", "status": "COMPLETE",
                "percentComplete": 100,
            }]})
            return
        if parsed.path == "/v2/tokens":
            if not self._require_bearer():
                return
            self._event("tokens")
            self._json({"items": [{
                "id": "fixture-existing", "name": "Existing fixture token",
                "token": "asset-token-existing", "dateAdded": "", "dateModified": "",
                "dateLastUsed": "", "isDefault": False, "scopes": ["assets:read"],
                "assetIds": [101], "allowedUrls": None,
            }]})
            return
        self._json({"error": "not_found"}, 404)

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        body = self._read_body()
        if parsed.path == "/oauth/token":
            request = json.loads(body)
            authorization = type(self).authorization
            verifier = request.get("code_verifier", "")
            challenge = base64.urlsafe_b64encode(
                hashlib.sha256(verifier.encode()).digest()
            ).decode().rstrip("=")
            valid = (
                request.get("grant_type") == "authorization_code"
                and request.get("code") == "fixture-code"
                and request.get("client_id") == authorization.get("client_id")
                and request.get("redirect_uri") == authorization.get("redirect_uri")
                and challenge == authorization.get("code_challenge")
            )
            self._event("token_exchange", pkce_valid=valid)
            if not valid:
                self._json({"error": "invalid_grant"}, 400)
                return
            self._json({
                "access_token": self.access_token,
                "refresh_token": "fixture-refresh-token",
                "token_type": "bearer",
                "expires_in": 3600,
                "refresh_token_expires_in": 7200,
            })
            return
        if parsed.path == "/v2/tokens":
            if not self._require_bearer():
                return
            request = json.loads(body)
            valid = request.get("name") == "Created by E2E" and request.get("scopes") == ["assets:read"]
            self._event("create_token", request_valid=valid)
            if not valid:
                self._json({"error": "invalid_request"}, 400)
                return
            self._json({
                "id": "fixture-created", "name": request["name"],
                "token": "asset-token-created", "dateAdded": "", "dateModified": "",
                "dateLastUsed": "", "isDefault": False, "scopes": request["scopes"],
                "assetIds": None, "allowedUrls": None,
            })
            return
        self._json({"error": "not_found"}, 404)


def serve(port_file: Path, events_file: Path) -> None:
    IonFixtureHandler.events_file = events_file
    events_file.write_text("", encoding="utf-8")
    server = ThreadingHTTPServer(("127.0.0.1", 0), IonFixtureHandler)
    port_file.write_text(str(server.server_port), encoding="utf-8")
    server.serve_forever()


def browse(url: str) -> None:
    with urlopen(url, timeout=15) as response:
        if response.status != 200:
            raise RuntimeError(f"OAuth callback returned HTTP {response.status}")


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    server = subparsers.add_parser("serve")
    server.add_argument("--port-file", type=Path, required=True)
    server.add_argument("--events-file", type=Path, required=True)
    browser = subparsers.add_parser("browse")
    browser.add_argument("url")
    args = parser.parse_args()
    if args.command == "serve":
        serve(args.port_file, args.events_file)
    else:
        browse(args.url)


if __name__ == "__main__":
    main()

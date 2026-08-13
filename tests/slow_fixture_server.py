#!/usr/bin/env python3
"""Local delayed-content server for deterministic cancellation tests."""

import argparse
import base64
import json
import pathlib
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--geojson-fixture", required=True)
    parser.add_argument("--marker", required=True)
    parser.add_argument("--port-file", required=True)
    arguments = parser.parse_args()

    fixture = pathlib.Path(arguments.fixture).read_bytes()
    geojson_fixture = pathlib.Path(arguments.geojson_fixture).read_bytes()
    credit_logo = base64.b64decode(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII="
    )
    marker = pathlib.Path(arguments.marker)
    marker.parent.mkdir(parents=True, exist_ok=True)
    request_counts: dict[str, int] = {}
    request_counts_lock = threading.Lock()

    def append_marker(value: str) -> None:
        with request_counts_lock:
            with marker.open("a", encoding="utf-8") as marker_file:
                marker_file.write(value + "\n")

    def tileset_definition(content_uri: str) -> bytes:
        return json.dumps(
            {
                "asset": {"version": "1.1"},
                "geometricError": 0,
                "root": {
                    "transform": [
                        1, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        6_378_137, 0, 0, 1,
                    ],
                    "boundingVolume": {
                        "box": [
                            50, 0, 50,
                            100, 0, 0,
                            0, 100, 0,
                            0, 0, 100,
                        ]
                    },
                    "geometricError": 0,
                    "content": {"uri": content_uri},
                },
            }
        ).encode("utf-8")

    def raster_tileset_definition(content_uri: str) -> bytes:
        # Place the fixture's local XY plane tangent to WGS84 at longitude and
        # latitude zero. A radial plane has a zero-height cartographic extent,
        # so it deliberately cannot be mapped to real raster imagery.
        return json.dumps(
            {
                "asset": {"version": "1.1"},
                "geometricError": 0,
                "root": {
                    "transform": [
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        1, 0, 0, 0,
                        6_378_137, 0, 0, 1,
                    ],
                    "boundingVolume": {
                        "box": [
                            50, 50, 0,
                            100, 0, 0,
                            0, 100, 0,
                            0, 0, 100,
                        ]
                    },
                    "geometricError": 0,
                    "content": {"uri": content_uri},
                },
            }
        ).encode("utf-8")

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, format_string: str, *values: object) -> None:
            print(format_string % values, flush=True)

        def _send(
            self,
            content_type: str,
            payload: bytes,
            cache_control: str = "no-store",
        ) -> None:
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", cache_control)
            self.end_headers()
            try:
                self.wfile.write(payload)
            except (BrokenPipeError, ConnectionResetError):
                # Cancellation is the behavior this fixture is meant to test.
                pass

        def _send_failure(
            self,
            status: int,
            payload: bytes,
            retry_after: str | None = None,
        ) -> None:
            self.send_response(status)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            if retry_after is not None:
                self.send_header("Retry-After", retry_after)
            self.end_headers()
            self.wfile.write(payload)

        def _read_request_body(self) -> bytes:
            if self.headers.get("Transfer-Encoding", "").lower() != "chunked":
                content_length = int(self.headers.get("Content-Length", "0"))
                return self.rfile.read(content_length)

            body = bytearray()
            while True:
                size_line = self.rfile.readline().strip().split(b";", 1)[0]
                chunk_size = int(size_line, 16)
                if chunk_size == 0:
                    while self.rfile.readline() not in (b"\r\n", b"\n", b""):
                        pass
                    break
                body.extend(self.rfile.read(chunk_size))
                self.rfile.read(2)
            return bytes(body)

        def do_GET(self) -> None:
            parsed = urlparse(self.path)
            query = parse_qs(parsed.query)
            token = query.get("token", [""])[0]
            with request_counts_lock:
                request_counts[parsed.path] = request_counts.get(parsed.path, 0) + 1
                path_count = request_counts[parsed.path]

            if parsed.path in (
                "/header-tileset.json",
                "/header-content.gltf",
            ):
                supplied_header = self.headers.get(
                    "X-Cesium-Tileset-Test",
                    "",
                )
                if supplied_header == "request-header-secret":
                    header_state = "initial"
                elif supplied_header == "request-header-secret-rotated":
                    header_state = "rotated"
                else:
                    header_state = "missing"
                request_kind = (
                    "root"
                    if parsed.path == "/header-tileset.json"
                    else "content"
                )
                # Record only a classification. The actual header value may be
                # a credential and must never enter diagnostics or test logs.
                append_marker(
                    "tileset-header:" + request_kind + ":" + header_state
                )
                if header_state == "missing":
                    self._send_failure(401, b"required request header missing")
                    return
                if request_kind == "root":
                    self._send(
                        "application/json",
                        tileset_definition("/header-content.gltf"),
                    )
                else:
                    self._send("model/gltf+json", fixture)
                return

            if parsed.path in ("/appData", "/alternate/appData"):
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write("geocoder-app-data\n")
                self._send(
                    "application/json",
                    json.dumps(
                        {
                            "applicationMode": "cesium-ion",
                            "dataStoreType": "S3",
                            "attribution": "Test ion endpoint",
                        }
                    ).encode("utf-8"),
                )
                return
            if parsed.path in (
                "/v1/geocode/search",
                "/v1/geocode/autocomplete",
                "/alternate/v1/geocode/search",
                "/alternate/v1/geocode/autocomplete",
            ):
                query_text = query.get("text", [""])[0]
                provider = query.get("geocoder", ["default"])[0]
                authorization_ok = (
                    self.headers.get("Authorization", "")
                    == "Bearer deterministic-geocoder-token"
                )
                request_kind = parsed.path.rsplit("/", 1)[-1]
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write(
                            "geocoder-request:"
                            + request_kind
                            + ":"
                            + provider
                            + ":"
                            + query_text
                            + ":auth="
                            + str(authorization_ok).lower()
                            + "\n"
                        )
                if query_text == "force-failure":
                    self._send_failure(401, b"geocoder authorization rejected")
                    return
                if query_text == "slow-cancel":
                    time.sleep(0.2)
                self._send(
                    "application/json",
                    json.dumps(
                        {
                            "type": "FeatureCollection",
                            "features": [
                                {
                                    "type": "Feature",
                                    "properties": {"label": "Kojan Harbor"},
                                    "geometry": {
                                        "type": "Point",
                                        "coordinates": [145.125, -37.875],
                                    },
                                },
                                {
                                    "type": "Feature",
                                    "properties": {"label": "Leth Nurae Region"},
                                    "bbox": [10.0, 20.0, 14.0, 24.0],
                                    "geometry": {
                                        "type": "Point",
                                        "coordinates": [12.0, 22.0],
                                    },
                                },
                            ],
                            "attributions": [
                                {
                                    "html": "<span>Visible geocoder credit</span>",
                                    "collapsible": False,
                                },
                                {
                                    "html": "<span>Popover geocoder credit</span>",
                                    "collapsible": True,
                                },
                            ],
                        }
                    ).encode("utf-8"),
                )
                return

            if parsed.path == "/retry-tileset.json":
                if path_count <= 2:
                    self._send_failure(503, b"temporary", retry_after="0")
                    return
                self._send(
                    "application/json",
                    tileset_definition("/retry-content.gltf"),
                )
                return
            if parsed.path == "/retry-content.gltf":
                self._send("model/gltf+json", fixture)
                return
            if parsed.path == "/credit-logo.png":
                self._send("image/png", credit_logo, "public, max-age=3600")
                return
            if parsed.path == "/raster-tileset.json":
                self._send(
                    "application/json",
                    raster_tileset_definition("/retry-content.gltf"),
                )
                return
            if parsed.path.startswith("/raster/") and parsed.path.endswith(".png"):
                with marker.open("a", encoding="utf-8") as marker_file:
                    marker_file.write(
                        "raster:"
                        + parsed.path
                        + ":"
                        + self.headers.get("X-Cesium-Overlay-Test", "")
                        + "\n"
                    )
                self._send("image/png", credit_logo)
                return
            if parsed.path.startswith("/REST/v1/Imagery/Metadata/"):
                style = parsed.path.rsplit("/", 1)[-1]
                key = query.get("key", [""])[0]
                culture = query.get("culture", [""])[0]
                host, port = self.server.server_address
                image_url = (
                    f"http://{host}:{port}/bing/tiles/"
                    "{subdomain}/{quadkey}.png?culture={culture}"
                )
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write(
                            f"bing-metadata:{style}:{key}:{culture}\n"
                        )
                self._send(
                    "application/json",
                    json.dumps(
                        {
                            "resourceSets": [
                                {
                                    "resources": [
                                        {
                                            "imageWidth": 1,
                                            "imageHeight": 1,
                                            "zoomMax": 2,
                                            "imageUrl": image_url,
                                            "imageUrlSubdomains": ["t0"],
                                            "imageryProviders": [
                                                {
                                                    "attribution": "Test Bing imagery",
                                                    "coverageAreas": [
                                                        {
                                                            "bbox": [-85, -180, 85, 180],
                                                            "zoomMin": 1,
                                                            "zoomMax": 3,
                                                        }
                                                    ],
                                                }
                                            ],
                                        }
                                    ]
                                }
                            ]
                        }
                    ).encode("utf-8"),
                )
                return
            if parsed.path.startswith("/bing/tiles/") and parsed.path.endswith(".png"):
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write(
                            "bing-tile:"
                            + parsed.path
                            + ":"
                            + query.get("culture", [""])[0]
                            + "\n"
                        )
                self._send("image/png", credit_logo)
                return
            if parsed.path == "/google/tile/v1/viewport":
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write(
                            "google-viewport:"
                            + query.get("session", [""])[0]
                            + ":"
                            + query.get("key", [""])[0]
                            + "\n"
                        )
                self._send(
                    "application/json",
                    json.dumps(
                        {
                            "maxZoomRects": [
                                {
                                    "maxZoom": 28,
                                    "west": -180,
                                    "south": -85,
                                    "east": 180,
                                    "north": 85,
                                }
                            ],
                            "copyright": "Imagery ©2026 Test Google imagery",
                        }
                    ).encode("utf-8"),
                )
                return
            if parsed.path.startswith("/google/v1/2dtiles/"):
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write(
                            "google-tile:"
                            + parsed.path
                            + ":"
                            + query.get("session", [""])[0]
                            + ":"
                            + query.get("key", [""])[0]
                            + "\n"
                        )
                self._send("image/png", credit_logo)
                return
            if parsed.path in ("/geojson.json", "/slow-geojson.json"):
                with marker.open("a", encoding="utf-8") as marker_file:
                    marker_file.write(
                        "geojson:"
                        + parsed.path
                        + ":"
                        + self.headers.get("X-Cesium-GeoJSON-Test", "")
                        + "\n"
                    )
                if parsed.path == "/slow-geojson.json":
                    time.sleep(0.75)
                self._send("application/geo+json", geojson_fixture)
                return
            if parsed.path == "/disconnect-retry-tileset.json":
                if path_count <= 2:
                    # Exercise a transport exception rather than an HTTP
                    # response. The third identical logical request succeeds.
                    self.close_connection = True
                    try:
                        self.connection.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass
                    self.connection.close()
                    return
                self._send(
                    "application/json",
                    tileset_definition("/retry-content.gltf"),
                )
                return
            if parsed.path == "/permanent-tileset.json":
                self._send_failure(404, b"permanent")
                return
            if parsed.path == "/malformed-tileset.json":
                self._send("application/json", b"{ deliberately malformed")
                return
            if parsed.path == "/malformed-content-tileset.json":
                self._send(
                    "application/json",
                    tileset_definition("/malformed-content.gltf"),
                )
                return
            if parsed.path == "/malformed-content.gltf":
                self._send("model/gltf+json", b"{ deliberately malformed")
                return
            if parsed.path == "/overlay/tilemapresource.xml":
                self._send_failure(404, b"overlay provider missing")
                return
            if parsed.path == "/cache-tileset.json":
                with marker.open("a", encoding="utf-8") as marker_file:
                    marker_file.write("cache-tileset:" + token + "\n")
                content_uri = "/cache-content.gltf?token=" + token
                self._send(
                    "application/json",
                    tileset_definition(content_uri),
                    "public, max-age=3600",
                )
                return
            if parsed.path == "/cache-content.gltf":
                with marker.open("a", encoding="utf-8") as marker_file:
                    marker_file.write("cache-content:" + token + "\n")
                self._send(
                    "model/gltf+json",
                    fixture,
                    "public, max-age=3600",
                )
                return
            if parsed.path == "/slow-tileset.json":
                content_uri = "/slow-content.gltf?token=" + token
                self._send(
                    "application/json",
                    tileset_definition(content_uri),
                )
                return
            if parsed.path == "/slow-content.gltf":
                with marker.open("a", encoding="utf-8") as marker_file:
                    marker_file.write(token + "\n")
                time.sleep(0.75)
                self._send("model/gltf+json", fixture)
                return
            self.send_error(404)

        def do_POST(self) -> None:
            parsed = urlparse(self.path)
            query = parse_qs(parsed.query)
            body = self._read_request_body()
            if parsed.path == "/google/v1/createSession":
                try:
                    session_options = json.loads(body)
                except json.JSONDecodeError:
                    self._send_failure(400, b"invalid session JSON")
                    return
                with request_counts_lock:
                    with marker.open("a", encoding="utf-8") as marker_file:
                        marker_file.write(
                            "google-session:"
                            + query.get("key", [""])[0]
                            + ":"
                            + json.dumps(session_options, sort_keys=True)
                            + "\n"
                        )
                self._send(
                    "application/json",
                    json.dumps(
                        {
                            "session": "deterministic-google-session",
                            "expiry": "4102444800",
                            "tileWidth": 1,
                            "tileHeight": 1,
                            "imageFormat": "png",
                        }
                    ).encode("utf-8"),
                )
                return
            self.send_error(404)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    pathlib.Path(arguments.port_file).write_text(
        str(server.server_address[1]), encoding="utf-8"
    )
    try:
        server.serve_forever()
    finally:
        server.shutdown()
        for thread in threading.enumerate():
            if thread is not threading.current_thread() and not thread.daemon:
                thread.join(timeout=1.0)


if __name__ == "__main__":
    main()

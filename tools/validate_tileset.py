#!/usr/bin/env python3
"""Offline structural validator for local 3D Tiles and referenced glTF data."""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse


class TilesetValidator:
    def __init__(self, maximum_payload_bytes: int = 128 * 1024 * 1024) -> None:
        self.maximum_payload_bytes = maximum_payload_bytes
        self.issues: list[dict[str, Any]] = []
        self._visited_tilesets: set[Path] = set()
        self._active_tilesets: set[Path] = set()
        self._visited_models: set[Path] = set()
        self._recorded_files: set[Path] = set()
        self.summary = {
            "tilesets": 0,
            "tiles": 0,
            "contents": 0,
            "gltf_models": 0,
            "external_tilesets": 0,
            "files": 0,
            "payload_bytes": 0,
        }

    def issue(
        self,
        severity: str,
        code: str,
        location: str,
        message: str,
    ) -> None:
        self.issues.append(
            {
                "severity": severity,
                "code": code,
                "location": location,
                "message": message,
            }
        )

    def validate(self, tileset_path: Path) -> dict[str, Any]:
        root = tileset_path.expanduser().resolve()
        self._validate_tileset_file(root, str(root), external=False)
        errors = sum(issue["severity"] == "error" for issue in self.issues)
        warnings = sum(issue["severity"] == "warning" for issue in self.issues)
        return {
            "schema": "cesium-godot-validation-v1",
            "root": str(root),
            "valid": errors == 0,
            "errors": errors,
            "warnings": warnings,
            "summary": self.summary,
            "issues": self.issues,
        }

    def _record_file(self, path: Path, location: str) -> bool:
        if not path.is_file():
            self.issue("error", "missing_file", location, f"Missing file: {path}")
            return False
        path = path.resolve()
        if path in self._recorded_files:
            return True
        try:
            size = path.stat().st_size
        except OSError as error:
            self.issue("error", "unreadable_file", location, str(error))
            return False
        self._recorded_files.add(path)
        self.summary["files"] += 1
        self.summary["payload_bytes"] += size
        if self.maximum_payload_bytes > 0 and size > self.maximum_payload_bytes:
            self.issue(
                "warning",
                "oversized_payload",
                location,
                f"{path.name} is {size} bytes; limit is {self.maximum_payload_bytes}",
            )
        return True

    def _load_json(self, path: Path, location: str) -> dict[str, Any] | None:
        if not self._record_file(path, location):
            return None
        try:
            value = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            self.issue("error", "invalid_json", location, str(error))
            return None
        if not isinstance(value, dict):
            self.issue("error", "invalid_json_root", location, "JSON root must be an object")
            return None
        return value

    def _validate_tileset_file(
        self,
        path: Path,
        location: str,
        *,
        external: bool,
    ) -> None:
        path = path.resolve()
        if path in self._active_tilesets:
            self.issue(
                "error",
                "external_tileset_cycle",
                location,
                f"External tileset cycle returns to {path}",
            )
            return
        if path in self._visited_tilesets:
            return
        document = self._load_json(path, location)
        if document is None:
            return
        self._visited_tilesets.add(path)
        self._active_tilesets.add(path)
        self.summary["tilesets"] += 1
        if external:
            self.summary["external_tilesets"] += 1
        asset = document.get("asset")
        if not isinstance(asset, dict) or not isinstance(asset.get("version"), str):
            self.issue("error", "missing_asset_version", location, "asset.version is required")
        root = document.get("root")
        if not isinstance(root, dict):
            self.issue("error", "missing_root_tile", location, "root must be a tile object")
        else:
            top_error = self._number(document.get("geometricError"))
            if top_error is None or top_error < 0.0:
                self.issue(
                    "error",
                    "invalid_tileset_geometric_error",
                    location,
                    "Top-level geometricError must be a finite non-negative number",
                )
            self._validate_tile(root, path.parent, f"{location}#/root", None)
        self._active_tilesets.remove(path)

    def _validate_tile(
        self,
        tile: dict[str, Any],
        base_dir: Path,
        location: str,
        parent_error: float | None,
    ) -> None:
        self.summary["tiles"] += 1
        self._validate_bounding_volume(tile.get("boundingVolume"), f"{location}/boundingVolume")
        geometric_error = self._number(tile.get("geometricError"))
        if geometric_error is None or geometric_error < 0.0:
            self.issue(
                "error",
                "invalid_geometric_error",
                f"{location}/geometricError",
                "geometricError must be a finite non-negative number",
            )
            geometric_error = None
        elif parent_error is not None and geometric_error > parent_error + 1e-9:
            self.issue(
                "warning",
                "inconsistent_lod_error",
                f"{location}/geometricError",
                f"Child error {geometric_error} is greater than parent error {parent_error}",
            )
        refine = tile.get("refine")
        if refine is not None and refine not in ("ADD", "REPLACE"):
            self.issue("error", "invalid_refine", f"{location}/refine", "refine must be ADD or REPLACE")
        transform = tile.get("transform")
        if transform is not None and not self._finite_array(transform, 16):
            self.issue(
                "error",
                "invalid_transform",
                f"{location}/transform",
                "transform must contain 16 finite numbers",
            )
        if "viewerRequestVolume" in tile:
            self._validate_bounding_volume(
                tile.get("viewerRequestVolume"),
                f"{location}/viewerRequestVolume",
            )

        content_values: list[Any] = []
        if "content" in tile:
            content_values.append(tile.get("content"))
        contents = tile.get("contents")
        if contents is not None:
            if not isinstance(contents, list):
                self.issue("error", "invalid_contents", f"{location}/contents", "contents must be an array")
            else:
                content_values.extend(contents)
        for index, content in enumerate(content_values):
            content_location = f"{location}/content[{index}]"
            if not isinstance(content, dict):
                self.issue("error", "broken_content_link", content_location, "content must be an object")
                continue
            self.summary["contents"] += 1
            if "boundingVolume" in content:
                self._validate_bounding_volume(
                    content.get("boundingVolume"),
                    f"{content_location}/boundingVolume",
                )
            uri = content.get("uri", content.get("url"))
            if not isinstance(uri, str) or not uri:
                self.issue("error", "missing_content_uri", content_location, "content.uri is required")
                continue
            resolved = self._resolve_local_uri(base_dir, uri, content_location)
            if resolved is not None:
                self._validate_content_file(resolved, content_location)

        children = tile.get("children", [])
        if not isinstance(children, list):
            self.issue("error", "broken_child_link", f"{location}/children", "children must be an array")
            return
        for index, child in enumerate(children):
            child_location = f"{location}/children/{index}"
            if not isinstance(child, dict):
                self.issue("error", "broken_child_link", child_location, "child must be a tile object")
                continue
            self._validate_tile(child, base_dir, child_location, geometric_error)

    def _validate_bounding_volume(self, value: Any, location: str) -> None:
        if not isinstance(value, dict):
            self.issue("error", "missing_bounding_volume", location, "bounding volume must be an object")
            return
        standard = [key for key in ("box", "region", "sphere") if key in value]
        extensions = value.get("extensions")
        has_extension_volume = isinstance(extensions, dict) and any(
            key in extensions
            for key in ("3DTILES_bounding_volume_S2", "3DTILES_bounding_volume_cylinder")
        )
        if len(standard) + int(has_extension_volume) != 1:
            self.issue(
                "error",
                "invalid_bounding_volume",
                location,
                "bounding volume must define exactly one supported shape",
            )
            return
        if not standard:
            return
        shape = standard[0]
        components = value[shape]
        expected = {"box": 12, "region": 6, "sphere": 4}[shape]
        if not self._finite_array(components, expected):
            self.issue(
                "error",
                "invalid_bounding_volume",
                f"{location}/{shape}",
                f"{shape} must contain {expected} finite numbers",
            )
            return
        numbers = [float(component) for component in components]
        if shape == "sphere" and numbers[3] <= 0.0:
            self.issue("error", "invalid_bounding_volume", location, "sphere radius must be positive")
        elif shape == "region":
            if (
                not -math.pi <= numbers[0] <= math.pi
                or not -math.pi <= numbers[2] <= math.pi
                or not -math.pi / 2 <= numbers[1] <= math.pi / 2
                or not -math.pi / 2 <= numbers[3] <= math.pi / 2
                or numbers[1] > numbers[3]
                or numbers[4] > numbers[5]
            ):
                self.issue("error", "invalid_bounding_volume", location, "region ranges/heights are invalid")
        elif shape == "box":
            axes = (numbers[3:6], numbers[6:9], numbers[9:12])
            if any(sum(component * component for component in axis) <= 1e-24 for axis in axes):
                self.issue("error", "invalid_bounding_volume", location, "box half-axes must be non-zero")

    def _resolve_local_uri(self, base_dir: Path, uri: str, location: str) -> Path | None:
        parsed = urlparse(uri)
        if parsed.scheme in ("http", "https"):
            self.issue(
                "warning",
                "remote_content_not_validated",
                location,
                f"Offline validation skipped remote URI: {uri}",
            )
            return None
        if parsed.scheme == "data":
            return None
        if parsed.scheme == "file":
            return Path(unquote(parsed.path)).resolve()
        if parsed.scheme:
            self.issue(
                "warning",
                "unsupported_uri_scheme",
                location,
                f"Offline validation skipped URI scheme {parsed.scheme}",
            )
            return None
        return (base_dir / unquote(parsed.path)).resolve()

    def _validate_content_file(self, path: Path, location: str) -> None:
        suffix = path.suffix.lower()
        if suffix == ".json":
            self._validate_tileset_file(path, location, external=True)
        elif suffix == ".gltf":
            self._validate_gltf_file(path, location)
        elif suffix == ".glb":
            self._validate_glb_file(path, location)
        else:
            self._record_file(path, location)

    def _validate_gltf_file(self, path: Path, location: str) -> None:
        path = path.resolve()
        if path in self._visited_models:
            return
        model = self._load_json(path, location)
        if model is None:
            return
        self._visited_models.add(path)
        self.summary["gltf_models"] += 1
        self._validate_gltf_document(model, path.parent, location)

    def _validate_glb_file(self, path: Path, location: str) -> None:
        path = path.resolve()
        if path in self._visited_models or not self._record_file(path, location):
            return
        self._visited_models.add(path)
        try:
            payload = path.read_bytes()
            if len(payload) < 20:
                raise ValueError("GLB header is truncated")
            magic, version, byte_length = struct.unpack_from("<4sII", payload, 0)
            if magic != b"glTF" or version != 2 or byte_length != len(payload):
                raise ValueError("GLB header magic/version/length is invalid")
            chunk_length, chunk_type = struct.unpack_from("<II", payload, 12)
            if chunk_type != 0x4E4F534A or 20 + chunk_length > len(payload):
                raise ValueError("GLB first chunk is not valid JSON")
            model = json.loads(payload[20 : 20 + chunk_length].decode("utf-8").rstrip("\x00 \t\r\n"))
            if not isinstance(model, dict):
                raise ValueError("GLB JSON root must be an object")
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
            self.issue("error", "invalid_glb", location, str(error))
            return
        self.summary["gltf_models"] += 1
        self._validate_gltf_document(model, path.parent, location)

    def _validate_gltf_document(
        self,
        model: dict[str, Any],
        base_dir: Path,
        location: str,
    ) -> None:
        asset = model.get("asset")
        if not isinstance(asset, dict) or asset.get("version") != "2.0":
            self.issue("error", "invalid_gltf_version", location, "glTF asset.version must be 2.0")
        buffers = model.get("buffers", [])
        if not isinstance(buffers, list):
            self.issue("error", "invalid_gltf_buffers", location, "buffers must be an array")
            buffers = []
        for index, buffer in enumerate(buffers):
            if not isinstance(buffer, dict):
                self.issue("error", "invalid_gltf_buffer", f"{location}#/buffers/{index}", "buffer must be an object")
                continue
            uri = buffer.get("uri")
            if isinstance(uri, str):
                resolved = self._resolve_local_uri(base_dir, uri, f"{location}#/buffers/{index}")
                if resolved is not None:
                    self._record_file(resolved, f"{location}#/buffers/{index}")

        buffer_views = model.get("bufferViews", [])
        if not isinstance(buffer_views, list):
            buffer_views = []
            self.issue("error", "invalid_buffer_views", location, "bufferViews must be an array")
        images = model.get("images", [])
        if not isinstance(images, list):
            images = []
            self.issue("error", "invalid_images", location, "images must be an array")
        for index, image in enumerate(images):
            image_location = f"{location}#/images/{index}"
            if not isinstance(image, dict):
                self.issue("error", "invalid_image", image_location, "image must be an object")
                continue
            uri = image.get("uri")
            if isinstance(uri, str):
                resolved = self._resolve_local_uri(base_dir, uri, image_location)
                if resolved is not None and not self._record_file(resolved, image_location):
                    self.issue("error", "missing_texture", image_location, f"Missing image texture: {resolved}")
            elif not self._valid_index(image.get("bufferView"), len(buffer_views)):
                self.issue("error", "missing_texture", image_location, "image needs a valid uri or bufferView")

        textures = model.get("textures", [])
        if not isinstance(textures, list):
            textures = []
            self.issue("error", "invalid_textures", location, "textures must be an array")
        for index, texture in enumerate(textures):
            if not isinstance(texture, dict) or not self._valid_index(texture.get("source"), len(images)):
                self.issue(
                    "error",
                    "missing_texture",
                    f"{location}#/textures/{index}",
                    "texture.source does not identify an image",
                )

        def validate_texture_info(value: Any, texture_location: str) -> None:
            if not isinstance(value, dict) or not self._valid_index(value.get("index"), len(textures)):
                self.issue("error", "missing_texture", texture_location, "material texture index is invalid")

        materials = model.get("materials", [])
        if not isinstance(materials, list):
            materials = []
            self.issue("error", "invalid_materials", location, "materials must be an array")
        for index, material in enumerate(materials):
            if not isinstance(material, dict):
                continue
            material_location = f"{location}#/materials/{index}"
            pbr = material.get("pbrMetallicRoughness")
            if isinstance(pbr, dict):
                for key in ("baseColorTexture", "metallicRoughnessTexture"):
                    if key in pbr:
                        validate_texture_info(pbr[key], f"{material_location}/{key}")
            for key in ("normalTexture", "occlusionTexture", "emissiveTexture"):
                if key in material:
                    validate_texture_info(material[key], f"{material_location}/{key}")

        meshes = model.get("meshes", [])
        if isinstance(meshes, list):
            for mesh_index, mesh in enumerate(meshes):
                if not isinstance(mesh, dict) or not isinstance(mesh.get("primitives"), list):
                    self.issue(
                        "error",
                        "broken_mesh_link",
                        f"{location}#/meshes/{mesh_index}",
                        "mesh.primitives must be an array",
                    )
                    continue
                for primitive_index, primitive in enumerate(mesh["primitives"]):
                    if not isinstance(primitive, dict):
                        self.issue(
                            "error",
                            "broken_mesh_link",
                            f"{location}#/meshes/{mesh_index}/primitives/{primitive_index}",
                            "primitive must be an object",
                        )
                        continue
                    material_index = primitive.get("material")
                    if material_index is not None and not self._valid_index(material_index, len(materials)):
                        self.issue(
                            "error",
                            "broken_material_link",
                            f"{location}#/meshes/{mesh_index}/primitives/{primitive_index}/material",
                            "primitive material index is invalid",
                        )

    @staticmethod
    def _number(value: Any) -> float | None:
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            return None
        result = float(value)
        return result if math.isfinite(result) else None

    @classmethod
    def _finite_array(cls, value: Any, length: int) -> bool:
        return (
            isinstance(value, list)
            and len(value) == length
            and all(cls._number(component) is not None for component in value)
        )

    @staticmethod
    def _valid_index(value: Any, length: int) -> bool:
        return isinstance(value, int) and not isinstance(value, bool) and 0 <= value < length


def validate_tileset(path: Path, maximum_payload_bytes: int = 128 * 1024 * 1024) -> dict[str, Any]:
    return TilesetValidator(maximum_payload_bytes).validate(path)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Validate a local 3D Tiles tileset and referenced glTF files before runtime."
    )
    parser.add_argument("tileset", type=Path)
    parser.add_argument("--max-payload-bytes", type=int, default=128 * 1024 * 1024)
    parser.add_argument("--json", action="store_true", help="Print the complete JSON report")
    parser.add_argument("--strict-warnings", action="store_true", help="Return failure for warnings")
    parser.add_argument("--write-report", type=Path)
    args = parser.parse_args(argv)
    if args.max_payload_bytes < 0:
        parser.error("--max-payload-bytes must be non-negative")
    report = validate_tileset(args.tileset, args.max_payload_bytes)
    encoded = json.dumps(report, indent=2, sort_keys=True)
    if args.write_report is not None:
        args.write_report.parent.mkdir(parents=True, exist_ok=True)
        args.write_report.write_text(encoded + "\n", encoding="utf-8")
    if args.json:
        print(encoded)
    else:
        print(
            f"3D Tiles validation: errors={report['errors']} warnings={report['warnings']} "
            f"tiles={report['summary']['tiles']} files={report['summary']['files']}"
        )
        for issue in report["issues"]:
            print(
                f"{issue['severity'].upper()} {issue['code']} "
                f"{issue['location']}: {issue['message']}"
            )
    if report["errors"] > 0 or (args.strict_warnings and report["warnings"] > 0):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

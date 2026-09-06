"""Exercise a ZIP-installed addon in a fresh project, then reopen its saved scene.

Use --rendered under a private X server for a runtime screenshot. Output is kept
for inspection; no source addon symlinks or developer import caches are used.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", default="godot4")
    parser.add_argument("--rendered", action="store_true")
    parser.add_argument("--log-dir", type=Path, default=ROOT / "build/test-results/packaged-editor")
    args = parser.parse_args()
    args.log_dir.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix="cesium-packaged-editor-", dir="/var/tmp" if os.name != "nt" else None))
    project = stage / "tests/godot-editor"
    project.mkdir(parents=True)
    archive = stage / "cesium-godot.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for path in (ROOT / "addons/cesium_godot").rglob("*"):
            if path.is_file() and path.suffix not in {".uid", ".import", ".a", ".lib", ".exp"}:
                bundle.write(path, path.relative_to(ROOT))
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(project)
    for name in ["project.godot", "camera_scene.gd", "camera_scene.tscn"]:
        shutil.copy2(ROOT / "tests/godot-editor" / name, project / name)
    shutil.copytree(ROOT / "tests/godot-editor/addons/editor_smoke_probe", project / "addons/editor_smoke_probe")
    shutil.copytree(ROOT / "examples/lifecycle_material_demo/fixtures", stage / "examples/lifecycle_material_demo/fixtures")
    env = os.environ.copy()
    env["XDG_DATA_HOME"] = str(stage / "userdata")
    env["XDG_CACHE_HOME"] = str(stage / "cache")
    if os.name == "nt":
        env["APPDATA"] = str(stage / "userdata")
        env["LOCALAPPDATA"] = str(stage / "cache")

    def run(name, command, marker):
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        (stage / (name + ".log")).write_text(output)
        (args.log_dir / (name + ".log")).write_text(output)
        report = {
            "exit_code": result.returncode,
            "signal": signal.Signals(-result.returncode).name if os.name != "nt" and result.returncode < 0 else None,
            "success_marker_found": marker in output,
            "engine_errors_found": "ERROR:" in output,
        }
        (args.log_dir / (name + ".json")).write_text(json.dumps(report, indent=2))
        if result.returncode or marker not in output or "SCRIPT ERROR:" in output or "ERROR:" in output:
            print(output, flush=True)
            raise RuntimeError(
                f"{name} failed: {json.dumps(report)}; "
                f"inspect {stage / (name + '.log')}"
            )

    run("editor", [args.godot, "--headless", "--editor", "--path", str(project), "--quit-after", "1200", "--", "--save-workflow"], "Cesium editor scene actions and serialization passed")
    # Keep the user help cache but discard this temporary project's import cache.
    # This reproduces opening a fresh ZIP install after another editor project,
    # not merely reopening an already-imported project (which hid the race).
    shutil.rmtree(project / ".godot")
    run("editor-warm", [args.godot, "--headless", "--editor", "--path", str(project), "--quit-after", "1200", "--", "--save-workflow"], "Cesium editor scene actions and serialization passed")
    # This second engine process must deserialize the actual editor-saved scene.
    command = [args.godot, "--path", str(project), "res://saved.tscn", "--quit-after", "6000"]
    if not args.rendered:
        command.append("--headless")
    command += ["--", "--smoke-test"]
    if args.rendered:
        command.append("--capture")
    run("reopened-runtime", command, "Ordinary Camera3D streaming fixture passed")
    print(f"Packaged editor save/reopen/Play passed: {stage}")


if __name__ == "__main__":
    main()

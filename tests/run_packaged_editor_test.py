"""Exercise a ZIP-installed addon in a fresh project, then reopen its saved scene.

Use --rendered under a private X server for a runtime screenshot. Output is kept
for inspection; no source addon symlinks or developer import caches are used.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", default="godot4")
    parser.add_argument("--rendered", action="store_true")
    args = parser.parse_args()
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

    def run(name, command, marker):
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        (stage / (name + ".log")).write_text(output)
        if result.returncode or marker not in output or "SCRIPT ERROR:" in output or "ERROR:" in output:
            print(output, flush=True)
            raise RuntimeError(f"{name} failed; inspect {stage / (name + '.log')}")

    run("editor", [args.godot, "--headless", "--editor", "--path", str(project), "--quit-after", "1200", "--", "--save-workflow"], "Cesium editor scene actions and serialization passed")
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

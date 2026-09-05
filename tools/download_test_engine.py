"""Install an official supported Godot engine into an isolated CI test job."""
import argparse
import json
import os
from pathlib import Path
import platform
import time
import urllib.request
import zipfile


def engine_asset(version, system):
    if system == "Windows":
        return f"Godot_v{version}-stable_win64.exe.zip", f"Godot_v{version}-stable_win64_console.exe"
    if system == "Linux":
        return f"Godot_v{version}-stable_linux.x86_64.zip", f"Godot_v{version}-stable_linux.x86_64"
    raise ValueError(f"No runtime test job is configured for {system}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    lock = json.loads((root / "dependencies.lock.json").read_text())
    if args.version not in lock["godot_compatibility"]["tested_versions"]:
        raise SystemExit("Only locked, tested Godot versions may be downloaded")
    asset, executable = engine_asset(args.version, platform.system())
    destination = root / "build/test-engines" / args.version
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / asset
    url = f"https://github.com/godotengine/godot/releases/download/{args.version}-stable/{asset}"
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=60) as response, archive.open("wb") as output:
                import shutil
                shutil.copyfileobj(response, output)
            break
        except OSError:
            if attempt == 2:
                raise
            time.sleep(2)
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(destination)
    engine = destination / executable
    engine.chmod(engine.stat().st_mode | 0o111)
    print(f"Installed official Godot {args.version}: {engine}")
    if "GITHUB_ENV" in os.environ:
        with open(os.environ["GITHUB_ENV"], "a") as output:
            print(f"GODOT_BIN={engine}", file=output)


if __name__ == "__main__":
    main()

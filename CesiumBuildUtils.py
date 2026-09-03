# This file contains utility functions to build CesiumForGodot in SCons
import subprocess
import os
import shutil
import sys
import json
from pathlib import Path

from SCons.Script import Dir

ROOT_DIR_MODULE = "#modules/cesium_godot"

ROOT_DIR_EXT = "#cesium_godot"

CESIUM_MODULE_DEF = "CESIUM_GD_MODULE"

CESIUM_EXT_DEF = "CESIUM_GD_EXT"

CESIUM_NATIVE_ROOT_ENV = "CESIUM_GODOT_NATIVE_ROOT"

CESIUM_NATIVE_BUILD_ROOT_ENV = "CESIUM_GODOT_NATIVE_BUILD_ROOT"

GODOT_CPP_ROOT_ENV = "CESIUM_GODOT_GODOT_CPP_ROOT"

VCPKG_ROOT_ENV = "CESIUM_GODOT_VCPKG_ROOT"

ALLOW_UNPINNED_ENV = "CESIUM_GODOT_ALLOW_UNPINNED_DEPENDENCIES"

REPOSITORY_ROOT = Path(__file__).resolve().parent

DEPENDENCY_SOURCE_ROOT = REPOSITORY_ROOT / "build" / "dependencies" / "sources"

DEPENDENCY_BUILD_ROOT = REPOSITORY_ROOT / "build" / "dependencies" / "native-build"

DEPENDENCY_VCPKG_INSTALLED_ROOT = (
    REPOSITORY_ROOT / "build" / "dependencies" / "vcpkg-installed"
)

DEPENDENCY_LOCK_PATH = REPOSITORY_ROOT / "dependencies.lock.json"

OS_WIN = "nt"

OS_LINUX = "posix"

# sys.platform value for macOS (os.name returns 'posix' for both Linux and macOS)
PLATFORM_MACOS = "darwin"

PLATFORM_ANDROID = "android"

STATIC_TRIPLET = "x64-windows-static"

RELEASE_CONFIG = "Release"

ezvcpkgFoundPath: str = ""


def _dependency_lock():
    with open(DEPENDENCY_LOCK_PATH, "r", encoding="utf-8") as lock_file:
        return json.load(lock_file)


def _configured_path(environment_name: str, default_path: Path) -> str:
    configured = os.environ.get(environment_name, "").strip()
    if configured:
        return os.path.abspath(os.path.expanduser(configured))
    return str(default_path.resolve())


def get_bindings_root() -> str:
    return _configured_path(GODOT_CPP_ROOT_ENV, DEPENDENCY_SOURCE_ROOT / "godot-cpp")


def get_bindings_sconstruct() -> str:
    return os.path.join(get_bindings_root(), "SConstruct")


def get_godot_api_version() -> str:
    return _dependency_lock()["godot_compatibility"]["api_version"]


def get_target_platform(arguments=None) -> str:
    """Return the requested SCons target, falling back to the native host."""
    if arguments is not None:
        requested = str(arguments.get("platform", "")).strip()
        if requested:
            return requested
    if os.name == OS_WIN:
        return "windows"
    if sys.platform == PLATFORM_MACOS:
        return "macos"
    if os.name == OS_LINUX:
        return "linux"
    raise RuntimeError(f"unsupported build host: {sys.platform}")


def get_target_architecture(arguments=None) -> str:
    if arguments is not None:
        requested = str(arguments.get("arch", "")).strip()
        if requested:
            return requested
    return "arm64" if get_target_platform(arguments) == "macos" else "x86_64"


def get_compile_flags(arguments=None):
    target = get_target_platform(arguments)
    if target == "windows":
        return ["/std:c++20", "/Zc:__cplusplus", "/utf-8", "/bigobj"]
    elif target == "macos":
        return ["-std=c++20", "-fexceptions", "-fPIC"]
    elif target in ("linux", PLATFORM_ANDROID):
        return ["-std=c++20", "-fexceptions", "-fpermissive", "-fPIC"]
    raise RuntimeError(f"unsupported target platform: {target}")


def get_linker_flags(arguments=None):
    if get_target_platform(arguments) == "windows":
        return ["/IGNORE:4217"]
    return []


def is_extension_target(argsDict) -> bool:
    return get_compile_target_definition(argsDict) == CESIUM_EXT_DEF


def get_curl_lib_name(arguments=None) -> str:
    if get_target_platform(arguments) == "windows":
        return "libcurl"
    return "curl"


def generate_precision_symbols(argsDict, env):
    desiredPrecision = argsDict.get("precision")
    if desiredPrecision == "double":
        print("Enabling double-precision Godot ABI symbols")
        env.Append(CPPDEFINES=["REAL_T_IS_DOUBLE"])


def get_compile_target_definition(argsDict) -> str:
    # Get the format (default is extension)
    global currentRootDir
    compileTarget = argsDict.get("compileTarget", CESIUM_EXT_DEF)
    if compileTarget == "module":
        print("[CESIUM] - Compiling Cesium For Godot as an engine module...")
        currentRootDir = ROOT_DIR_MODULE
        return CESIUM_MODULE_DEF
    if compileTarget == "" or compileTarget == "extension":
        print("[CESIUM] - Compiling Cesium For Godot as a GDExtension")
        currentRootDir = ROOT_DIR_EXT
        return CESIUM_EXT_DEF

    print("[CESIUM] - Compile target not recognized, options are: module / extension")
    exit(1)


def link_abseil_libs(env, arguments=None):
    foundLibs: list[SCons.Node.FS.File] = env.Glob(
        f"{find_ezvcpkg_path()}/packages/abseil_{determine_triplet(arguments)}/lib/*absl*.a"
    )

    # Dark magic to strip the lib prefix and the file extension
    foundLibs = [lib.name.replace("lib", "")[:-2] for lib in foundLibs]

    env.Append(LINKFLAGS=["-Wl,--start-group"], LIBS=foundLibs)

    env.Append(LINKFLAGS=["-Wl,--end-group"])

    env.Append(
        LINKFLAGS=["-Wl,--start-group"],
        LIBS=[
            "absl_log_internal_log_sink_set",
            "absl_log_globals",
            "absl_leak_check",
            "absl_log_internal_globals",
            "absl_log_internal_format",
            "absl_base",
            "absl_hash",
            "absl_city",
            "absl_examine_stack",
            "absl_stacktrace",
            "absl_borrowed_fixup_buffer",
            "absl_spinlock_wait",
            "absl_debugging_internal",
            "absl_synchronization",
            "absl_base",
            "absl_malloc_internal",
            "absl_int128",
            "absl_symbolize",
            "absl_kernel_timeout_internal",
            "absl_debugging_internal",
            "absl_demangle_internal",
            "absl_log_sink",
            "absl_demangle_rust",
            "absl_decode_rust_punycode",
            "absl_utf8_for_code_point",
        ],
    )
    env.Append(LINKFLAGS=["-Wl,--end-group"])


def get_native_root_override() -> str:
    """Return the caller-selected Cesium Native checkout, if any."""
    configured_root = os.environ.get(CESIUM_NATIVE_ROOT_ENV, "").strip()
    if configured_root == "":
        return ""
    return os.path.abspath(os.path.expanduser(configured_root))


def _allow_unpinned_dependencies() -> bool:
    return os.environ.get(ALLOW_UNPINNED_ENV, "").strip().lower() in (
        "1",
        "true",
        "yes",
    )


def _bootstrap_dependency(name: str, destination: str) -> None:
    if _allow_unpinned_dependencies():
        print(f"WARNING: dependency lock verification disabled for {name}")
        return
    command = [
        sys.executable,
        str(REPOSITORY_ROOT / "tools" / "bootstrap_dependencies.py"),
        "--root",
        str(REPOSITORY_ROOT),
        "--only",
        name,
    ]
    if name == "godot-cpp":
        command.extend(("--godot-cpp-root", destination))
    elif name == "cesium-native":
        command.extend(("--native-root", destination))
    elif name == "vcpkg":
        command.extend(("--vcpkg-root", destination))
    subprocess.run(command, check=True)


def clone_native_repo_if_needed():
    native_root_override = get_native_root_override()
    if native_root_override != "":
        if not os.path.isdir(native_root_override):
            raise RuntimeError(
                f"{CESIUM_NATIVE_ROOT_ENV} does not name a Cesium Native directory: "
                f"{native_root_override}"
            )
        _bootstrap_dependency("cesium-native", native_root_override)
        print(f"Using locked Cesium Native from {native_root_override}")
        return
    _bootstrap_dependency("cesium-native", get_root_dir_native())


def clone_bindings_repo_if_needed():
    _bootstrap_dependency("godot-cpp", get_bindings_root())


def ensure_vcpkg():
    _bootstrap_dependency("vcpkg", get_vcpkg_root())


def clone_lite_html_if_needed():
    # clone_repo_if_needed(ROOT_DIR_EXT + "/third_party/lite-html", "Lite HTML",
    #                      "https://github.com/litehtml/litehtml.git", "v0.9", "6ca1ab0419e770e6d35a1ef690238773a1dafcee")
    pass


def build_litehtml(arch="arm64"):
    """Build litehtml from source for the given architecture."""
    third_party_dir = scons_to_abs_path(ROOT_DIR_EXT + "/third_party")
    source_dir = os.path.join(third_party_dir, "litehtml-src")
    output_dir = os.path.join(third_party_dir, "litehtml", "macos")

    # Check if already built
    if os.path.exists(os.path.join(output_dir, "liblitehtml.a")):
        print("litehtml already built for macOS, skipping...")
        return

    if not os.path.exists(source_dir):
        print("litehtml source not found at %s" % source_dir, file=sys.stderr)
        return

    print("Building litehtml from source for macOS...")

    build_dir = os.path.join(source_dir, "build-macos")
    os.makedirs(build_dir, exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)

    prev_dir = os.getcwd()
    os.chdir(build_dir)

    # Configure with CMake
    result = subprocess.run([
        "cmake",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_OSX_ARCHITECTURES={arch}",
        "-DLITEHTML_BUILD_TESTING=OFF",
        ".."
    ])

    if result.returncode != 0:
        print("Failed to configure litehtml", file=sys.stderr)
        os.chdir(prev_dir)
        return

    # Build
    result = subprocess.run(["cmake", "--build", ".", "--config", "Release"])

    if result.returncode != 0:
        print("Failed to build litehtml", file=sys.stderr)
        os.chdir(prev_dir)
        return

    # Copy output libraries
    for lib in ["liblitehtml.a", "libgumbo.a"]:
        src = os.path.join(build_dir, lib)
        if not os.path.exists(src):
            # Try in subdirectories
            for root, dirs, files in os.walk(build_dir):
                if lib in files:
                    src = os.path.join(root, lib)
                    break
        if os.path.exists(src):
            shutil.copy2(src, output_dir)
            print(f"Copied {lib} to {output_dir}")

    os.chdir(prev_dir)
    print("litehtml build complete!")


# Configure with CMake
def configure_native(argumentsDict):
    print("Configuring Cesium Native")
    is_extension_target(argumentsDict)
    source_directory = get_root_dir_native()
    build_directory = get_native_build_root(argumentsDict)
    os.makedirs(build_directory, exist_ok=True)
    triplet: str = determine_triplet(argumentsDict)
    subprocess_environment = os.environ.copy()
    subprocess_environment["VCPKG_TRIPLET"] = triplet
    subprocess_environment["EZVCPKG_BASEDIR"] = get_ezvcpkg_base_path()
    subprocess_environment.setdefault("GIT_LFS_SKIP_SMUDGE", "1")
    native_tests_enabled = os.environ.get("CESIUM_GODOT_NATIVE_TESTS", "").strip().lower()
    native_tests = "ON" if native_tests_enabled in ("1", "true", "yes") else "OFF"
    cmake_arguments = [
            "cmake",
            "-S",
            source_directory,
            "-B",
            build_directory,
            # Prevent a visible build tree from silently retaining dependency
            # paths from an older checkout or machine. CMake preserves compiled
            # outputs whose commands remain valid, but regenerates its cache
            # from the exact source and vcpkg roots selected above.
            "--fresh",
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={RELEASE_CONFIG}",
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
            "-DCESIUM_MSVC_STATIC_RUNTIME_ENABLED=%s"
            % ("ON" if get_target_platform(argumentsDict) == "windows" else "OFF"),
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
            f"-DCESIUM_TESTS_ENABLED={native_tests}",
            "-DCESIUM_ENABLE_CLANG_TIDY=OFF",
            "-DVCPKG_TRIPLET=%s" % triplet,
            "-DVCPKG_TARGET_TRIPLET=%s" % triplet,
        ]
    if get_target_platform(argumentsDict) == PLATFORM_ANDROID:
        ndk_root = get_android_ndk_root()
        subprocess_environment["ANDROID_NDK_HOME"] = ndk_root
        subprocess_environment["ANDROID_NDK_ROOT"] = ndk_root
        cmake_arguments.extend(
            (
                "-DCESIUM_USE_EZVCPKG=OFF",
                "-DVCPKG_MANIFEST_MODE=ON",
                "-DCMAKE_TOOLCHAIN_FILE=%s"
                % os.path.join(get_vcpkg_root(), "scripts", "buildsystems", "vcpkg.cmake"),
                "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=%s"
                % os.path.join(ndk_root, "build", "cmake", "android.toolchain.cmake"),
                "-DVCPKG_INSTALLED_DIR=%s"
                % get_vcpkg_installed_root(argumentsDict),
                "-DANDROID_ABI=arm64-v8a",
                "-DANDROID_PLATFORM=android-24",
            )
        )
    result = subprocess.run(
        cmake_arguments,
        env=subprocess_environment,
    )

    if result.returncode != 0:
        errorMsg = "cmake return code: %s" % str(result.returncode)
        print(
            "Error configuring Cesium native, please make sure you have CMake installed and up to date: "
            + errorMsg
        )
        raise RuntimeError(errorMsg)
    print("Configuration completed without any errors!")


def determine_triplet(arguments=None):
    target = get_target_platform(arguments)
    architecture = get_target_architecture(arguments)
    if target == PLATFORM_ANDROID:
        if architecture != "arm64":
            raise RuntimeError(
                f"unsupported Android architecture: {architecture}; only arm64 is supported"
            )
        return "arm64-android"
    if target == "windows":
        return "x64-windows-static"
    if target == "macos":
        return "arm64-osx"
    if target == "linux":
        return "x64-linux"
    raise RuntimeError(f"unsupported target platform: {target}")


def get_android_ndk_root() -> str:
    """Resolve and validate the exact Android NDK locked by this repository."""
    version = _dependency_lock()["toolchain"]["android_ndk"]
    explicit = os.environ.get("ANDROID_NDK_ROOT", "").strip()
    if explicit:
        root = Path(explicit).expanduser().resolve()
    else:
        sdk_root = (
            os.environ.get("ANDROID_HOME", "").strip()
            or os.environ.get("ANDROID_SDK_ROOT", "").strip()
        )
        if not sdk_root:
            raise RuntimeError(
                "Android builds require ANDROID_HOME or ANDROID_NDK_ROOT; "
                f"install NDK {version}"
            )
        root = Path(sdk_root).expanduser().resolve() / "ndk" / version

    toolchain = root / "build" / "cmake" / "android.toolchain.cmake"
    if not toolchain.is_file():
        raise RuntimeError(
            f"locked Android NDK {version} is missing at {root}; "
            "set ANDROID_HOME or ANDROID_NDK_ROOT"
        )
    properties = root / "source.properties"
    if properties.is_file():
        revision = ""
        for line in properties.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("Pkg.Revision"):
                revision = line.partition("=")[2].strip()
                break
        if revision and revision != version:
            raise RuntimeError(
                f"Android NDK at {root} is {revision}, expected locked version {version}"
            )
    return str(root)


def compile_native(argumentsDict):
    shouldBuildArg = argumentsDict.get("buildCesium", "yes")
    shouldBuildArg = str(shouldBuildArg).upper() in ("YES", "TRUE", "1")

    if not shouldBuildArg:
        return

    print("Building Cesium Native, this might take a few minutes...")
    configure_native(argumentsDict)
    print("Compiling Cesium Native...")

    result = build_native(argumentsDict)
    if result.returncode != 0:
        raise RuntimeError("Error building Cesium Native")
    print("Finished building Cesium Native!")


def build_native(arguments=None):
    configured_jobs = os.environ.get("CESIUM_GODOT_BUILD_JOBS", "").strip()
    jobs = configured_jobs or str(max(1, os.cpu_count() or 1))
    return subprocess.run(
        ["cmake", "--build", get_native_build_root(arguments), "--parallel", jobs]
    )


def install_additional_libs(arguments=None):
    print("Installing additional libraries")
    vcpkgPath = find_ezvcpkg_path()
    execExtension = ".exe" if os.name == OS_WIN else ""
    executable = "%s/%s" % (vcpkgPath, "vcpkg" + execExtension)
    triplet = determine_triplet(arguments)
    subprocess_environment = os.environ.copy()
    if get_target_platform(arguments) == PLATFORM_ANDROID:
        ndk_root = get_android_ndk_root()
        subprocess_environment["ANDROID_NDK_HOME"] = ndk_root
        subprocess_environment["ANDROID_NDK_ROOT"] = ndk_root
    installed_root = get_vcpkg_installed_root(arguments)
    # Cesium Native's CMake build uses a private manifest install tree. SCons
    # compiles the standalone GDExtension separately, so it must have the same
    # complete manifest available in the shared include / library tree. Using
    # Native's authoritative manifest prevents clean hosts from depending on
    # packages left behind by an earlier build.
    subprocess.run(
        [
            executable,
            "install",
            f"--x-manifest-root={get_root_dir_native()}",
            f"--x-install-root={installed_root}",
            f"--triplet={triplet}",
        ],
        check=True,
        env=subprocess_environment,
    )
    # These additional direct dependencies are not all top-level entries in
    # Cesium Native's manifest, but the standalone extension links them by name.
    subprocess.run(
        [
            executable,
            "install",
            f"--x-install-root={installed_root}",
            f"uriparser:{triplet}",
            f"ada-url:{triplet}",
            f"abseil:{triplet}",
            f"brotli:{triplet}",
        ],
        check=True,
        env=subprocess_environment,
    )
    if get_target_platform(arguments) == "windows":
        subprocess.run(
            [
                executable,
                "install",
                f"--x-install-root={installed_root}",
                f"curl:{triplet}",
            ],
            check=True,
            env=subprocess_environment,
        )


def find_ezvcpkg_path() -> str:
    global ezvcpkgFoundPath
    if ezvcpkgFoundPath != "":
        return ezvcpkgFoundPath
    ezvcpkgFoundPath = get_vcpkg_root()
    if not os.path.isdir(ezvcpkgFoundPath):
        raise RuntimeError(
            f"locked vcpkg checkout is missing at {ezvcpkgFoundPath}; "
            "build Cesium Native first or set CESIUM_GODOT_VCPKG_ROOT"
        )
    print(f"Found ezvcpkg at {ezvcpkgFoundPath}")
    return ezvcpkgFoundPath


def get_vcpkg_root() -> str:
    configured = os.environ.get(VCPKG_ROOT_ENV, "").strip()
    if configured:
        return os.path.abspath(os.path.expanduser(configured))
    baseline = _dependency_lock()["dependencies"]["vcpkg"]["baseline"]
    return str(
        (REPOSITORY_ROOT / "build" / "dependencies" / "vcpkg" / baseline).resolve()
    )


def get_ezvcpkg_base_path() -> str:
    configured = os.environ.get(VCPKG_ROOT_ENV, "").strip()
    if configured:
        return str(Path(os.path.abspath(os.path.expanduser(configured))).parent)
    return str((REPOSITORY_ROOT / "build" / "dependencies" / "vcpkg").resolve())


def scons_to_abs_path(path: str) -> str:
    return Dir(path).get_abspath()


def find_ezvcpkg_include_path(arguments=None) -> str:
    return f"{get_vcpkg_installed_root(arguments)}/{determine_triplet(arguments)}/include"


def find_ezvcpkg_lib_path(arguments=None) -> str:
    return f"{get_vcpkg_installed_root(arguments)}/{determine_triplet(arguments)}/lib"


def get_vcpkg_installed_root(arguments=None) -> str:
    """Keep Android's manifest state separate from desktop package installs."""
    if get_target_platform(arguments) == PLATFORM_ANDROID:
        return str(DEPENDENCY_VCPKG_INSTALLED_ROOT.resolve())
    return os.path.join(get_vcpkg_root(), "installed")


def get_root_dir() -> str:
    return currentRootDir


def get_root_dir_native() -> str:
    native_root_override = get_native_root_override()
    if native_root_override != "":
        return native_root_override
    return str((DEPENDENCY_SOURCE_ROOT / "cesium-native").resolve())


def get_native_build_root(arguments=None) -> str:
    configured = os.environ.get(CESIUM_NATIVE_BUILD_ROOT_ENV, "").strip()
    if configured:
        return os.path.abspath(os.path.expanduser(configured))
    return str((DEPENDENCY_BUILD_ROOT / f"{determine_triplet(arguments)}-release").resolve())


def get_native_library_config_subdir(arguments=None) -> str:
    """Return Release only for a multi-configuration Native build tree."""
    cache_path = os.path.join(get_native_build_root(arguments), "CMakeCache.txt")
    if not os.path.isfile(cache_path):
        return ""
    with open(cache_path, "r", encoding="utf-8", errors="replace") as cache_file:
        for line in cache_file:
            if line.startswith("CMAKE_CONFIGURATION_TYPES:"):
                configurations = line.partition("=")[2].strip().split(";")
                return RELEASE_CONFIG if RELEASE_CONFIG in configurations else ""
    return ""

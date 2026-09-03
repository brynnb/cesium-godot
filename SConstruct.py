#!/usr/bin/env python
import CesiumBuildUtils as cesium_build_utils

LIB_NAME = "Godot3DTiles"

# Glob source files
sources = Glob("cesium_auxiliars/*.cpp")


def add_source_files(self, p_sources):
    sources.extend(p_sources)


# Clone all the needed projects
if (cesium_build_utils.is_extension_target(ARGUMENTS)):
    cesium_build_utils.clone_bindings_repo_if_needed()

cesium_build_utils.clone_native_repo_if_needed()
cesium_build_utils.ensure_vcpkg()
cesium_build_utils.clone_lite_html_if_needed()

cesium_build_utils.compile_native(ARGUMENTS)

# Build litehtml from source on macOS (no pre-built binaries available)
if cesium_build_utils.get_target_platform(ARGUMENTS) == "macos":
    cesium_build_utils.build_litehtml()

# godot-cpp validates its own command line and would otherwise report this
# repository's two orchestration arguments as unknown. Hide only those values
# while evaluating its SConstruct, then restore them for our build.
cesium_only_arguments = {}
for argument_name in ("compileTarget", "buildCesium"):
    if argument_name in ARGUMENTS:
        cesium_only_arguments[argument_name] = ARGUMENTS.pop(argument_name)
try:
    env = SConscript(
        cesium_build_utils.get_bindings_sconstruct(),
        {"api_version": cesium_build_utils.get_godot_api_version()},
    )
finally:
    ARGUMENTS.update(cesium_only_arguments)

# Godot 4.6's generated bindings contain enough object files to exceed the
# POSIX process argument limit when GNU ar is invoked directly. SCons' TEMPFILE
# command generator writes the archive arguments to a response file only when
# the expanded command needs it; GNU ar accepts that response-file syntax.
if env["platform"] in ("linux", "android"):
    env["ARCOM_RESPONSE"] = env["ARCOM"]
    env["ARCOM"] = "${TEMPFILE(ARCOM_RESPONSE)}"

cesium_build_utils.generate_precision_symbols(ARGUMENTS, env)
env.Append(CXXFLAGS=cesium_build_utils.get_compile_flags(ARGUMENTS))
env.Append(LINKFLAGS=cesium_build_utils.get_linker_flags(ARGUMENTS))

cesium_build_utils.install_additional_libs(ARGUMENTS)

compilationTarget: str = cesium_build_utils.get_compile_target_definition(ARGUMENTS)

env.Append(CPPDEFINES=[compilationTarget])
if env["platform"] == "windows":
    # Prevent Win32's wingdi.h OPAQUE macro from colliding with glTF's
    # Material::AlphaMode::OPAQUE without modifying generated Native headers.
    env.Append(CPPDEFINES=["NOGDI", "NOMINMAX", "WIN32_LEAN_AND_MEAN"])
if env["platform"] != "windows":
    env.Append(CPPDEFINES=["CURL_STATIC_LIB", "SQLITE_STATIC"])
env.__class__.add_source_files = add_source_files

# Append include paths
env.Append(CPPPATH=["testSrc/", "cesium_godot/", "cesium_auxiliars/"])

# Run the SCsub that is under cesium_godot/
SConscript("cesium_godot/SCsub", exports="env")


# Create shared library
if env["platform"] == "macos":
    library = env.SharedLibrary(
        "godot3dtiles/addons/cesium_godot/lib/lib{}{}{}".format(
            LIB_NAME, env["suffix"], env["SHLIBSUFFIX"]
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary(
        "godot3dtiles/addons/cesium_godot/lib/{}{}{}".format(
            LIB_NAME, env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

# Set the default target
Default(library)

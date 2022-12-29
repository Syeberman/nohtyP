"""Creates the build environments for the various compilers nohtyP is built with.
"""

import itertools
import traceback

import SCons.Tool
from site_scons.root_environment import SconscriptLog
from site_scons.utilities import AliasIfNotEmpty


# List these in order of decreasing "preference"; the first variant that matches will be chosen as
# "build" (ie the default target)
# TODO If none of these compilers can be found, default to an "any" compiler (but still version the
# output directory)
# TODO Would love to make this more dynamic, finding any known compiler, and then trimming.
# TODO Visual Studio 2022, 2019, 2017 support
# TODO Clang/LLVM support
# TODO GCC 6 can't be found on AppVeyor's Ubuntu1604
compiler_names = (
    "gcc_12",
    "gcc_11",
    "msvs_158",
    "gcc_10",
    "gcc_9",
    "gcc_8",
    "gcc_7",
    "msvs_140",
    "gcc_6",
    "gcc_5",
    "msvs_120",
    "gcc_49",
    "gcc_48",
    "msvs_110",
    "gcc_47",
    "gcc_46",
    "msvs_100",
    "msvs_90",
)
oses = ("win32", "posix", "darwin")
archs = ("amd64", "x86")
# TODO An "analyze" configuration to use the compiler's static analysis, "lint" to explicitly use
# lint against the compiler's headers
configurations = ("debug", "release", "coverage")

# The native targets ("debug", "release", etc) should each copy files from a single variant
nativeTargetsToCreate = set(("debug", "release", "coverage"))


def MakeCompilerEnvs(rootEnv):
    native_os = rootEnv["HOST_OS"]
    if rootEnv["HOST_ARCH"] == "x86_64":
        native_archs = ("amd64", "x86")
    else:
        native_archs = (rootEnv["HOST_ARCH"],)

    SconscriptLog.write(
        f"""
Native OS: {native_os}
Native Arches: {native_archs}
"""
    )

    for compiler_name in compiler_names:
        compiler = SCons.Tool.Tool(compiler_name)

        for targ_os, targ_arch, configuration in itertools.product(oses, archs, configurations):
            # TODO Support cross-compiling OSes, if possible
            if targ_os != native_os:
                continue

            SconscriptLog.write(f"\n{compiler_name} {targ_os} {targ_arch} {configuration}\n")

            # Skip compilers that can't be found.
            try:
                compilerEnv = rootEnv.Clone(
                    COMPILER=compiler,
                    TARGET_OS=targ_os,
                    TARGET_ARCH=targ_arch,
                    CONFIGURATION=configuration,
                    IS_NATIVE=(targ_os == native_os and targ_arch in native_archs),
                    tools=[compiler, "tar"],
                )
            except (SCons.Errors.UserError, SCons.Errors.StopError):
                traceback.print_exc(file=SconscriptLog)
                continue

            # Maintain a separate directory for intermediate and target files, but don't copy
            # source.
            compilerEnv["VTOP"] = compilerEnv.Dir(
                "#Build/%s/%s_%s_%s"
                % (
                    compiler_name,
                    targ_os,
                    targ_arch,
                    configuration,
                )
            )
            compilerEnv.VariantDir("$VTOP", "#", duplicate=False)

            yield compilerEnv


def AddCompilerEnvAliases(compilerEnv, buildTargets, testTargets, analyzeTargets):
    compiler_name: str = compilerEnv["COMPILER"].name
    targ_os: str = compilerEnv["TARGET_OS"]
    targ_arch: str = compilerEnv["TARGET_ARCH"]
    configuration: str = compilerEnv["CONFIGURATION"]
    is_native: str = compilerEnv["IS_NATIVE"]

    # FIXME Handle this differently?
    if configuration == "coverage":
        buildTargets += testTargets

    if is_native:
        # Pick one variant from each configuration that matches the host system, and copy it
        # to the Build/native directory for easy access. nativeTargetsToCreate tracks which
        # configurations we have yet to pick.
        if configuration in nativeTargetsToCreate:
            nativeTargetsToCreate.remove(configuration)
            if configuration == "release":
                native_dest = "#Build/native"
            else:
                native_dest = "#Build/native/" + configuration
            buildTargets.append(compilerEnv.Install(native_dest, buildTargets))
            testTargets.append(compilerEnv.Install(native_dest, testTargets))
            analyzeTargets.append(compilerEnv.Install(native_dest, analyzeTargets))

            # Most developers only want to build or test with one variant on their host system.
            # These aliases make this easy. A full alias looks like `test:release`. The action
            # (`test`) defaults to `build`. The configuration (`release`) defaults to both
            # `release` and `debug`.
            AliasIfNotEmpty(compilerEnv, configuration, buildTargets)
            if configuration != "coverage":
                AliasIfNotEmpty(compilerEnv, "build:" + configuration, buildTargets)
                AliasIfNotEmpty(compilerEnv, "test:" + configuration, testTargets)
                AliasIfNotEmpty(compilerEnv, "analyze:" + configuration, analyzeTargets)
                AliasIfNotEmpty(compilerEnv, "build", buildTargets)
                AliasIfNotEmpty(compilerEnv, "test", testTargets)
                AliasIfNotEmpty(compilerEnv, "analyze", analyzeTargets)

    # These aliases exist for developers that are looking to test their changes across multiple,
    # or specific, compilers. A full alias looks like `test:gcc_9:win32:x86:release`. The action
    # (`test`) defaults to `build`. The configuration (`release`) defaults to both `release` and
    # `debug`. arch (`x86`) and OS (`win32`) default to all available values for each. `all` can
    # be used in place of `compiler:os:arch:configuration` for all available values for each.
    hierarchy = (compiler_name, targ_os, targ_arch, configuration)
    if configuration == "coverage":
        hierarchyName = ":".join(hierarchy)
        AliasIfNotEmpty(compilerEnv, hierarchyName, buildTargets)
    else:
        AliasIfNotEmpty(compilerEnv, "all", buildTargets)
        AliasIfNotEmpty(compilerEnv, "build:all", buildTargets)
        AliasIfNotEmpty(compilerEnv, "test:all", testTargets)
        AliasIfNotEmpty(compilerEnv, "analyze:all", analyzeTargets)
        for hierarchyDepth in range(1, len(hierarchy) + 1):
            hierarchyName = ":".join(hierarchy[:hierarchyDepth])
            AliasIfNotEmpty(compilerEnv, hierarchyName, buildTargets)
            AliasIfNotEmpty(compilerEnv, "build:" + hierarchyName, buildTargets)
            AliasIfNotEmpty(compilerEnv, "test:" + hierarchyName, testTargets)
            AliasIfNotEmpty(compilerEnv, "analyze:" + hierarchyName, analyzeTargets)

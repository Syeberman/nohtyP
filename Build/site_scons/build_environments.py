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
# TODO Clang/LLVM support
# TODO GCC 6 can't be found on AppVeyor's Ubuntu1604
compiler_names = (
    "gcc_13",  # April 2023
    "gcc_12",  # May 2022
    "msvs_17",  # November 2021
    "gcc_11",  # April 2021
    "gcc_10",  # May 2020
    "gcc_9",  # May 2019
    "msvs_16",  # April 2019
    "gcc_8",  # May 2018
    "gcc_7",  # May 2017
    "msvs_15",  # March 2017
    "gcc_6",  # April 2016
    "msvs_140",  # July 2015
    "gcc_5",  # April 2015
    "gcc_49",  # July 2014
    "msvs_120",  # October 2013
    "gcc_48",  # May 2013
    "msvs_110",  # September 2012
    "gcc_47",  # June 2012
    "gcc_46",  # June 2011
    "msvs_100",  # April 2010
    "msvs_90",  # November 2007
)
oses = ("win32", "posix", "darwin")
archs = ("amd64", "x86")
# TODO An "analyze" configuration to use the compiler's static analysis, "lint" to explicitly use
# lint against the compiler's headers
configurations = ("debug", "release", "coverage")


def _MakeCompilerEnvs(rootEnv):
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

    # The native targets ("debug", "release", etc) should each copy files from a single variant.
    install_targets_to_create = set(("debug", "release", "coverage"))

    for compiler_name in compiler_names:
        compiler = SCons.Tool.Tool(compiler_name)

        for targ_os, targ_arch, configuration in itertools.product(
            oses, archs, configurations
        ):
            # TODO Support cross-compiling OSes, if possible
            if targ_os != native_os:
                continue

            is_native_target = targ_os == native_os and targ_arch in native_archs

            SconscriptLog.write(
                f"\n{compiler_name} {targ_os} {targ_arch} {configuration}\n"
            )

            try:
                compilerEnv = rootEnv.Clone(
                    COMPILER=compiler,
                    TARGET_OS=targ_os,
                    TARGET_ARCH=targ_arch,
                    CONFIGURATION=configuration,
                    IS_NATIVE_TARGET=is_native_target,
                    IS_INSTALL_TARGET=False,  # possibly set to True below
                    tools=[compiler, "tar"],
                )
            except (SCons.Errors.UserError, SCons.Errors.StopError):
                # Skip compilers that can't be found.
                traceback.print_exc(file=SconscriptLog)
                continue

            # Pick one variant from each configuration that matches the host system, and copy it to
            # the Build/native directory for easy access.
            if is_native_target and configuration in install_targets_to_create:
                install_targets_to_create.remove(configuration)
                compilerEnv["IS_INSTALL_TARGET"] = True

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


def _AddCompilerEnvAliases(
    compilerEnv, buildTargets, testTargets, analyzeTargets, coverageTargets
):
    compiler_name: str = compilerEnv["COMPILER"].name
    targ_os: str = compilerEnv["TARGET_OS"]
    targ_arch: str = compilerEnv["TARGET_ARCH"]
    configuration: str = compilerEnv["CONFIGURATION"]
    is_install_target: str = compilerEnv["IS_INSTALL_TARGET"]

    if is_install_target:
        if configuration == "release":
            native_dest = "#Build/native"
        else:
            native_dest = "#Build/native/" + configuration
        buildTargets.append(compilerEnv.Install(native_dest, buildTargets))
        testTargets.append(compilerEnv.Install(native_dest, testTargets))
        analyzeTargets.append(compilerEnv.Install(native_dest, analyzeTargets))
        coverageTargets.append(compilerEnv.Install(native_dest, coverageTargets))

        # Most developers only want to build or test with one variant on their host system.
        # These aliases make this easy. A full alias looks like `test:release`. The action
        # (`test`) defaults to `build`. The configuration (`release`) defaults to both
        # `release` and `debug`.
        if configuration == "coverage":
            AliasIfNotEmpty(compilerEnv, configuration, coverageTargets)
        else:
            AliasIfNotEmpty(compilerEnv, configuration, buildTargets)
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
        AliasIfNotEmpty(compilerEnv, hierarchyName, coverageTargets)
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


def AddBuildTargets(rootEnv, makeTargets):
    for compilerEnv in _MakeCompilerEnvs(rootEnv):
        buildTargets, testTargets, analyzeTargets, coverageTargets = makeTargets(
            compilerEnv
        )
        _AddCompilerEnvAliases(
            compilerEnv, buildTargets, testTargets, analyzeTargets, coverageTargets
        )

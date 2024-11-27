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
configurations = ("debug", "release", "coverage")


def _MakeCompilerEnvs(rootEnv):
    """Returns available compiler environments, starting with the most-preferred compiler."""
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
    # We only need one environment where we run checkapi.
    create_checkapi_target = True
    # We run the compiler's static analysis with the latest installed version of each compiler.
    # TODO Perhaps run the analysis on all archs: 64- and 32-bit might have different warnings.
    compiler_analysis_targets_to_create = set(("gcc", "msvs"))

    for compiler_name in compiler_names:
        compiler_family = compiler_name.partition("_")[0]  # i.e. gcc, msvs
        compiler = SCons.Tool.Tool(compiler_name)

        for target_os, target_arch, configuration in itertools.product(
            oses, archs, configurations
        ):
            # TODO Support cross-compiling OSes, if possible
            if target_os != native_os:
                continue

            is_native_target = target_os == native_os and target_arch in native_archs

            SconscriptLog.write(
                f"\n{compiler_name} {target_os} {target_arch} {configuration}\n"
            )

            try:
                compilerEnv = rootEnv.Clone(
                    COMPILER=compiler,
                    TARGET_OS=target_os,
                    TARGET_ARCH=target_arch,
                    CONFIGURATION=configuration,
                    IS_NATIVE_TARGET=is_native_target,
                    tools=[compiler, "tar"],
                    # The following are possibly set to True only for compilers that can be found.
                    IS_INSTALL_TARGET=False,
                    IS_CHECKAPI_TARGET=False,
                    IS_COMPILER_ANALYSIS_TARGET=False,
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
            if configuration == "release":
                if create_checkapi_target:
                    create_checkapi_target = False
                    compilerEnv["IS_CHECKAPI_TARGET"] = True
                if compiler_family in compiler_analysis_targets_to_create:
                    compiler_analysis_targets_to_create.remove(compiler_family)
                    compilerEnv["IS_COMPILER_ANALYSIS_TARGET"] = True

            # Maintain a separate directory for intermediate and target files, but don't copy
            # source.
            compilerEnv["VTOP"] = compilerEnv.Dir(
                "#Build/%s/%s_%s_%s"
                % (
                    compiler_name,
                    target_os,
                    target_arch,
                    configuration,
                )
            )
            compilerEnv.VariantDir("$VTOP", "#", duplicate=False)

            # Because we install analysis from multiple compilers into a single directory, add the
            # compiler name to the suffix.
            compilerEnv["CANALYSISSUFFIX"] = f".{compiler_name}.analysis.txt"
            compilerEnv["CXXANALYSISSUFFIX"] = "$CANALYSISSUFFIX"

            yield compilerEnv


def _AddCompilerEnvAliases(compilerEnv, targets):
    compiler_name: str = compilerEnv["COMPILER"].name
    target_os: str = compilerEnv["TARGET_OS"]
    target_arch: str = compilerEnv["TARGET_ARCH"]
    configuration: str = compilerEnv["CONFIGURATION"]

    if compilerEnv["IS_INSTALL_TARGET"]:
        if configuration == "release":
            native_dest = "#Build/native"
        else:
            native_dest = "#Build/native/" + configuration
        targets.build.append(compilerEnv.Install(native_dest, targets.build))
        targets.test.append(compilerEnv.Install(native_dest, targets.test))
        targets.coverage.append(compilerEnv.Install(native_dest, targets.coverage))

        # Most developers only want to build or test with one variant on their host system.
        # These aliases make this easy. A full alias looks like `test:release`. The action
        # (`test`) defaults to `build`. The configuration (`release`) defaults to both
        # `release` and `debug`.
        if configuration == "coverage":
            AliasIfNotEmpty(compilerEnv, configuration, targets.coverage)
        else:
            AliasIfNotEmpty(compilerEnv, configuration, targets.build)
            AliasIfNotEmpty(compilerEnv, "build:" + configuration, targets.build)
            AliasIfNotEmpty(compilerEnv, "test:" + configuration, targets.test)
            AliasIfNotEmpty(compilerEnv, "build", targets.build)
            AliasIfNotEmpty(compilerEnv, "test", targets.test)

    # There is one analyze target, which runs checkapi, and possibly the gcc and msvs analysis with
    # the latest version of those compilers in a release configuration.
    targets.analyze.append(
        compilerEnv.Install("#Build/native/analyze", targets.analyze)
    )
    AliasIfNotEmpty(compilerEnv, "analyze", targets.analyze)

    # These aliases exist for developers that are looking to test their changes across multiple,
    # or specific, compilers. A full alias looks like `test:gcc_9:win32:x86:release`. The action
    # (`test`) defaults to `build`. The configuration (`release`) defaults to both `release` and
    # `debug`. arch (`x86`) and OS (`win32`) default to all available values for each. `all` can
    # be used in place of `compiler:os:arch:configuration` for all available values for each.
    hierarchy = (compiler_name, target_os, target_arch, configuration)
    if configuration == "coverage":
        hierarchyName = ":".join(hierarchy)
        AliasIfNotEmpty(compilerEnv, hierarchyName, targets.coverage)
    else:
        AliasIfNotEmpty(compilerEnv, "all", targets.build)
        AliasIfNotEmpty(compilerEnv, "build:all", targets.build)
        AliasIfNotEmpty(compilerEnv, "test:all", targets.test)
        for hierarchyDepth in range(1, len(hierarchy) + 1):
            hierarchyName = ":".join(hierarchy[:hierarchyDepth])
            AliasIfNotEmpty(compilerEnv, hierarchyName, targets.build)
            AliasIfNotEmpty(compilerEnv, "build:" + hierarchyName, targets.build)
            AliasIfNotEmpty(compilerEnv, "test:" + hierarchyName, targets.test)


class BuildTargets:
    """These collect all the build, test, analysis, and coverage targets, including:
    - the C and C# builds of the nohtyP shared library
    - python_test and ypExamples results
    - checkapi.py, PC-lint, expanded compiler warnings
    - targets that copy the above to Build/native
    """

    def __init__(self):
        self.build = []
        self.test = []
        self.coverage = []
        self.analyze = []


def AddBuildTargets(rootEnv, makeTargets):
    for compilerEnv in _MakeCompilerEnvs(rootEnv):
        targets = makeTargets(compilerEnv)
        _AddCompilerEnvAliases(compilerEnv, targets)

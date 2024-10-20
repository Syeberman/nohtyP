"""Provides a Coverage action for gcc (i.e. gcov).
"""

import SCons.Action
import SCons.Builder
import SCons.Defaults
import SCons.Util
from pathlib import Path


# TODO SCons should have an easier way to disable output for a Delete.
quietDelete = SCons.Action.ActionFactory(SCons.Defaults.Delete.actfunc, lambda x, y=0: "")


# Input environment variables (set in build scripts):
#
# COVDATADIR - directory where coverage data files can be found (i.e. the inputs to gcov)
# COVTESTACTION - action that will generate the coverage data to analyze
#
# Other environment variables (can be overridden):
#
# COV - the coverage report tool (i.e. gcov)
# COVNOTEPREFIX/COVNOTESUFFIX - prefix/suffix for coverage "note" files (i.e. .gcno)
# COVDATAPREFIX/COVDATASUFFIX - prefix/suffix for coverage data files (i.e. .gcda)
# COVOUTPREFIX/COVOUTSUFFIX - prefix/suffix for analyzed coverage files (i.e. .gcov)
# COVPREFIX/COVSUFFIX - prefix/suffix for target archive (i.e. .gcov.tar.gz)
# COVFLAGS - list of flags to pass to $COV
# COVCOM - command line used to generate analyzed coverage files


def CoverageDataFiles(env):
    """Finds coverage data files that currently exist on disk."""
    prefix = env.subst("$COVDATAPREFIX")
    suffix = env.subst("$COVDATASUFFIX")
    dataDir = Path(env.subst("$COVDATADIR"))

    # Protect against deleting all files in a directory.
    if not prefix and not suffix:
        raise ValueError(f"Missing COVDATAPREFIX/COVDATASUFFIX")

    return sorted(str(x) for x in dataDir.rglob(f"{prefix}*{suffix}"))


# FIXME Perhaps investigate AddPreAction/AddPostAction.
def CoverageAction(target, source, env):
    archive = target[0]

    # Remove existing data files before running the tests.
    # FIXME Also remove .gcov?
    env.Execute(quietDelete(CoverageDataFiles(env)))

    # Run the tests. This creates new data files. This is an input to the builder.
    if env.Execute("$COVTESTACTION", "$COVTESTACTIONSTR"):
        env.Exit(1)

    # Process the data files into the output format. Note that CoverageDataFiles may return a
    # different list than before.
    # FIXME Write the .gcov file in a temporary directory? Use with --stdout.
    outFiles = []
    for dataFile in CoverageDataFiles(env):
        outFile = env.ReplaceIxes(
            dataFile, "COVDATAPREFIX", "COVDATASUFFIX", "COVOUTPREFIX", "COVOUTSUFFIX"
        )
        if env.Execute(*env.subst(["$COVCOM", "$COVCOMSTR"], target=outFile, source=dataFile)):
            env.Exit(1)
        outFiles.append(outFile)

    # Combine the output files into an archive for later processing.
    if env.Execute(
        *env.subst(
            ["$TAR -zc -f $TARGET $SOURCES", "Archiving coverage results to $TARGET"],
            target=archive,
            source=outFiles,
        )
    ):
        env.Exit(1)


def CoverageEmitter(target, source, env):
    archive, *others = SCons.Util.flatten(target)
    archive = SCons.Util.adjustixes(str(archive), env.subst("$COVPREFIX"), env.subst("$COVSUFFIX"))
    return [archive, *others], source


def generate_CoverageBuilder(env):
    env["BUILDERS"]["Coverage"] = SCons.Builder.Builder(
        action=SCons.Action.Action(CoverageAction, None),
        emitter=CoverageEmitter,
    )

    env["COV"] = "gcov"
    env["COVNOTEPREFIX"] = ""
    env["COVNOTESUFFIX"] = ".gcno"
    env["COVDATAPREFIX"] = ""
    env["COVDATASUFFIX"] = ".gcda"  # Files to clean to reset coverage.
    env["COVOUTPREFIX"] = ""
    env["COVOUTSUFFIX"] = ".gcov"
    env["COVPREFIX"] = ""
    env["COVSUFFIX"] = ".gcov.tar.gz"
    env["COVBRANCHFLAGS"] = [  # Flags to add to COVFLAGS to enable branch coverage.
        # FIXME It's these two --branch-* options that will disable branch coverage.
        "--branch-counts",
        "--branch-probabilities",
    ]
    env["COVFLAGS"] = [
        # FIXME --conditions? Requires gcc with -fcondition-coverage.
        # FIXME --source-prefix?
        "--demangled-names",
        "--preserve-paths",
        # FIXME "Exclude functions matching regex", added in GCC 14: will this ignore noreturn
        # assert_* branches?
        # "--exclude ^munit_errorf_ex$"
    ]
    # FIXME Changes to covcom don't trigger recompilation
    env["COVCOM"] = "$COV $COVFLAGS --stdout $SOURCE > $TARGET"

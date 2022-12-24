"""Provides a Coverage action for gcc (i.e. gcov).
"""

import SCons.Action
import SCons.Builder
import SCons.Defaults
from site_init import RGlob


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


def _CoverageFiles(env, prefix_var, suffix_var):
    prefix = env.subst("$" + prefix_var)
    suffix = env.subst("$" + suffix_var)
    dataDir = env.subst("$COVDATADIR")

    # Protect against deleting all files in a directory.
    if not prefix and not suffix:
        raise ValueError(f"Missing {prefix_var}/{suffix_var}")

    # FIXME Why is RGlob returning duplicates?
    return sorted(set(str(x) for x in RGlob(env, dataDir, f"{prefix}*{suffix}")))


def CoverageNoteFiles(env):
    return _CoverageFiles(env, "COVNOTEPREFIX", "COVNOTESUFFIX")


def CoverageDataFiles(env):
    return _CoverageFiles(env, "COVDATAPREFIX", "COVDATASUFFIX")


# FIXME Rename "out file" to something else?
def CoverageOutFiles(env):
    return _CoverageFiles(env, "COVOUTPREFIX", "COVOUTSUFFIX")


# FIXME Perhaps investigate AddPreAction/AddPostAction.
def CoverageAction(target, source, env):
    archive = target[0]

    # Remove existing data files before running the tests.
    env.Execute(quietDelete(CoverageDataFiles(env)))

    # Run the tests. This creates new data files. This is an input to the builder.
    env.Execute("$COVTESTACTION", "$COVTESTACTIONSTR")

    # Process the data files into the output format. Note that CoverageDataFiles may return a
    # different list than before.
    outFiles = []
    for dataFile in CoverageDataFiles(env):
        outFile = env.ReplaceIxes(
            dataFile, "COVDATAPREFIX", "COVDATASUFFIX", "COVOUTPREFIX", "COVOUTSUFFIX"
        )
        env.Execute(*env.subst(["$COVCOM", "$COVCOMSTR"], target=outFile, source=dataFile))
        outFiles.append(outFile)

    # Combine the output files into an archive for later processing.
    env.Execute(
        *env.subst(
            ["$TAR -zc -f $TARGET $SOURCES", "Archiving coverage results to $TARGET"],
            target=archive,
            source=outFiles,
        )
    )


def CoverageEmitter(target, source, env):
    archive, *others = env.Flatten(target)
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
    env["COVFLAGS"] = [
        "--branch-counts",
        "--branch-probabilities",
        "--demangled-names",
        # "--hash-filenames",
        "--preserve-paths",
    ]
    env["COVCOM"] = "$COV $COVFLAGS --stdout $SOURCE > $TARGET"

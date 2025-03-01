"""Used by the compilers to configure an "Analysis" Builder.
"""

import SCons.Builder
import SCons.Tool

# TODO Contribute this back to SCons

# TODO Could we take the program as input and determine the source files to analyze?
# TODO Can we enable whole-program analysis on top of per-file analysis?

AnalysisSuffix = ".analysis.txt"


def createAnalysisBuilder(env):
    """This is a utility function that creates the Analysis Builders in an Environment if they
    are not there already. Analysis Builders run the environment's compiler in a "static analysis"
    mode that collects the warnings and errors emitted. If the Builder is there already, we return
    the existing one.

    Returns the Analysis builder.
    """

    try:
        analysis = env["BUILDERS"]["Analysis"]
    except KeyError:
        analysis = SCons.Builder.Builder(
            action={},
            emitter={},
            src_builder=["CFile", "CXXFile"],
            source_scanner=SCons.Tool.SourceFileScanner,
            single_source=True,
        )
        env["BUILDERS"]["Analysis"] = analysis

        # Allow for different suffixes for C and C++ files.
        env.SetDefault(CANALYSISSUFFIX=AnalysisSuffix)
        env.SetDefault(CXXANALYSISSUFFIX=AnalysisSuffix)

    return analysis

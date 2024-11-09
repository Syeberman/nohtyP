"""Used by the compilers to configure an "Analysis" Builder.
"""

import SCons.Builder
import SCons.Tool

# TODO Contribute this back to SCons

# FIXME Could we take the program as input and analyze the source files backwards? In other words,
# is there whole-program analysis vs per-file analysis?

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
            # prefix = '',
            suffix=AnalysisSuffix,
            src_builder=["CFile", "CXXFile"],
            source_scanner=SCons.Tool.SourceFileScanner,
            single_source=True,
        )
        env["BUILDERS"]["Analysis"] = analysis

        env.SetDefault(CANALYSISSUFFIX=AnalysisSuffix)
        env.SetDefault(CXXANALYSISSUFFIX=AnalysisSuffix)

    return analysis

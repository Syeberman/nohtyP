"""Provides an Analysis action for generic Posix C++ compilers.
"""

import analysis_builder
import cc_analysis
import SCons.Action
import SCons.Util

# XXX These are internal to SCons and may change in the future...but it's unlikely
cplusplus = __import__("c++", globals(), locals(), [])
CXXSuffixes = cplusplus.CXXSuffixes

# TODO Contribute this back to SCons


def cxx_analysis_emitter(target, source, env):
    suffix = env.subst("$CXXANALYSISSUFFIX")
    target = [
        SCons.Util.adjustixes(str(t), "", suffix, ensure_suffix=False) for t in target
    ]
    return (target, source)


CXXAnalysisAction = SCons.Action.Action("$SACXXCOM", "$SACXXCOMSTR")


def generate_AnalysisBuilder(env):
    analysis = analysis_builder.createAnalysisBuilder(env)

    for suffix in CXXSuffixes:
        analysis.add_action(suffix, CXXAnalysisAction)
        analysis.add_emitter(suffix, cxx_analysis_emitter)

    # SACXX is the static analysis mode for CXX, the C++ compiler (compare with SHCXX et al)
    # XXX We do not add -Wextra, -fanalyzer, or any other warning control flags here. The only
    # configuration SACXXCOM brings is to redirect stderr to TARGET and disable compilation.
    # TODO Should SACXXFLAGS/etc reference CXXFLAGS/etc, like SHCXXFLAGS/etc does?
    env["SACXX"] = "$CXX"
    env["SACXXCOM"] = (
        f"$SACXX -fsyntax-only $SACXXFLAGS $SACCFLAGS $_CCCOMCOM $SOURCES 2> $TARGET"
    )

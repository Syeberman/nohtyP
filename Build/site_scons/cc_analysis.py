"""Provides an Analysis action for generic Posix C compilers.
"""

import analysis_builder
import SCons.Action
import SCons.Util

# XXX These are internal to SCons and may change in the future...but it's unlikely
from SCons.Tool.cc import CSuffixes

# TODO Contribute this back to SCons


def c_analysis_emitter(target, source, env):
    suffix = env.subst("$CANALYSISSUFFIX")
    target = [
        SCons.Util.adjustixes(str(t), "", suffix, ensure_suffix=False) for t in target
    ]
    return (target, source)


CAnalysisAction = SCons.Action.Action("$SACCCOM", "$SACCCOMSTR")


def generate_AnalysisBuilder(env):
    analysis = analysis_builder.createAnalysisBuilder(env)

    for suffix in CSuffixes:
        analysis.add_action(suffix, CAnalysisAction)
        analysis.add_emitter(suffix, c_analysis_emitter)

    # SACC is the static analysis mode for CC, the C compiler (compare with SHCC et al)
    # XXX We do not add -Wextra, -fanalyzer, or any other warning control flags here. The only
    # configuration SACCCOM brings is to redirect stderr to TARGET and disable compilation.
    # TODO Should SACFLAGS/etc reference CFLAGS/etc, like SHCFLAGS/etc does?
    env["SACC"] = "$CC"
    env["SACCCOM"] = (
        f"$SACC -fsyntax-only $SACFLAGS $SACCFLAGS $_CCCOMCOM $SOURCES 2> $TARGET"
    )

"""Provides an Analysis action for the Microsoft Visual Studio compilers.
"""

import os

import analysis_builder
import SCons.Action
import SCons.Util

# XXX These are internal to SCons and may change in the future...but it's unlikely
from SCons.Tool.msvc import CSuffixes, CXXSuffixes, msvc_batch_key

# TODO Contribute this back to SCons


def _analysis_emitter(target, source, env, suffix):
    target = [
        SCons.Util.adjustixes(str(t), "", suffix, ensure_suffix=False) for t in target
    ]
    return (target, source)


def c_analysis_emitter(target, source, env):
    suffix = env.subst("$CANALYSISSUFFIX")
    return _analysis_emitter(target, source, env, suffix)


def cxx_analysis_emitter(target, source, env):
    suffix = env.subst("$CXXANALYSISSUFFIX")
    return _analysis_emitter(target, source, env, suffix)


CAnalysisAction = SCons.Action.Action(
    "$SACCCOM", "$SACCCOMSTR", batch_key=msvc_batch_key, targets="$CHANGED_TARGETS"
)
CXXAnalysisAction = SCons.Action.Action(
    "$SACXXCOM", "$SACXXCOMSTR", batch_key=msvc_batch_key, targets="$CHANGED_TARGETS"
)


def generate_AnalysisBuilder(env):
    analysis = analysis_builder.createAnalysisBuilder(env)

    for suffix in CSuffixes:
        analysis.add_action(suffix, CAnalysisAction)
        analysis.add_emitter(suffix, c_analysis_emitter)

    for suffix in CXXSuffixes:
        analysis.add_action(suffix, CXXAnalysisAction)
        analysis.add_emitter(suffix, cxx_analysis_emitter)

    # SACC is the static analysis mode for CC, the C compiler (compare with SHCC et al)
    # XXX Warnings are written to stdout.
    # FIXME Code analysis also creates a log file named filename.nativecodeanalysis.xml, where
    # filename is the name of the analyzed source file. I've disabled this for now with
    # `/analyze:autolog-`... but should we prefer this?
    # TODO Should SACXXFLAGS/etc reference CXXFLAGS/etc, like SHCXXFLAGS/etc does?
    env["SACC"] = "$CC"
    env["SACCCOM"] = (
        '${TEMPFILE("$SACC /analyze:only /analyze:autolog- $CHANGED_SOURCES $SACFLAGS $SACCFLAGS $_CCCOMCOM > $TARGET","$SACCCOMSTR")}'
    )
    env["SACXX"] = "$CXX"
    env["SACXXCOM"] = (
        '${TEMPFILE("$SACXX /analyze:only /analyze:autolog- $CHANGED_SOURCES $SACXXFLAGS $SACCFLAGS $_CCCOMCOM > $TARGET","$SACXXCOMSTR")}'
    )

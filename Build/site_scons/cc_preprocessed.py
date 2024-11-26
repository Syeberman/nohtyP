"""Provides a Preprocessed action for generic Posix C compilers.
"""

import preprocessed_builder
import SCons.Action
import SCons.Util

# XXX These are internal to SCons and may change in the future...but it's unlikely
from SCons.Tool.cc import CSuffixes

# TODO Contribute this back to SCons


def c_preprocessed_emitter(target, source, env):
    suffix = env.subst("$CPREPROCESSEDSUFFIX")
    target = [
        SCons.Util.adjustixes(str(t), "", suffix, ensure_suffix=False) for t in target
    ]
    return (target, source)


CPreprocessedAction = SCons.Action.Action("$PPCCCOM", "$PPCCCOMSTR")


def generate_PreprocessedBuilder(env):
    preprocessed = preprocessed_builder.createPreprocessedBuilder(env)

    for suffix in CSuffixes:
        preprocessed.add_action(suffix, CPreprocessedAction)
        preprocessed.add_emitter(suffix, c_preprocessed_emitter)

    # PPCC is the preprocessor-only mode for CC, the C compiler (compare with SHCC et al)
    # TODO For SCons: be smart and when passed a preprocessed file, compiler skips certain options?
    # TODO Should PPCFLAGS/etc reference CFLAGS/etc, like SHCFLAGS/etc does?
    env["PPCC"] = "$CC"
    env["PPCCCOM"] = "$PPCC -E -o $TARGET -c $PPCFLAGS $PPCCFLAGS $_CCCOMCOM $SOURCES"

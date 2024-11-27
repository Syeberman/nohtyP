"""Provides a Preprocessed action for generic Posix C++ compilers.
"""

import cc_preprocessed
import preprocessed_builder
import SCons.Action
import SCons.Util

# XXX These are internal to SCons and may change in the future...but it's unlikely
cplusplus = __import__("c++", globals(), locals(), [])
CXXSuffixes = cplusplus.CXXSuffixes

# TODO Contribute this back to SCons


def cxx_preprocessed_emitter(target, source, env):
    suffix = env.subst("$CXXPREPROCESSEDSUFFIX")
    target = [
        SCons.Util.adjustixes(str(t), "", suffix, ensure_suffix=False) for t in target
    ]
    return (target, source)


CXXPreprocessedAction = SCons.Action.Action("$PPCXXCOM", "$PPCXXCOMSTR")


def generate_PreprocessedBuilder(env):
    preprocessed = preprocessed_builder.createPreprocessedBuilder(env)

    for suffix in CXXSuffixes:
        preprocessed.add_action(suffix, CXXPreprocessedAction)
        preprocessed.add_emitter(suffix, cxx_preprocessed_emitter)

    # PPCXX is the preprocessor-only mode for CXX, the C++ compiler (compare with SHCXX et al)
    # TODO For SCons: be smart and when passed a preprocessed file, compiler skips certain options?
    # TODO Should PPCXXFLAGS/etc reference CXXFLAGS/etc, like SHCXXFLAGS/etc does?
    env["PPCXX"] = "$CXX"
    env["PPCXXCOM"] = (
        "$PPCXX -E -o $TARGET -c $PPCXXFLAGS $PPCCFLAGS $_CCCOMCOM $SOURCES"
    )

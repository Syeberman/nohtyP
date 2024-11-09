"""Provides a Preprocessed action for the Microsoft Visual Studio compilers.
"""

import os

import preprocessed_builder
import SCons.Action
import SCons.Util
# XXX These are internal to SCons and may change in the future...but it's unlikely
from SCons.Tool.msvc import CSuffixes, CXXSuffixes, msvc_batch_key

# TODO Contribute this back to SCons


def _preprocessed_emitter(target, source, env, suffix):
    target = [
        SCons.Util.adjustixes(str(t), "", suffix, ensure_suffix=False) for t in target
    ]
    return (target, source)


def c_preprocessed_emitter(target, source, env):
    suffix = env.subst("$CPREPROCESSEDSUFFIX")
    return _preprocessed_emitter(target, source, env, suffix)


def cxx_preprocessed_emitter(target, source, env):
    suffix = env.subst("$CXXPREPROCESSEDSUFFIX")
    return _preprocessed_emitter(target, source, env, suffix)


# XXX Adapted from SCons' msvc_output_flag
def msvc_pp_output_flag(target, source, env, for_signature):
    """
    Returns the correct /Fi flag for batching.

    If batching is disabled or there's only one source file, then we
    return an /Fi string that specifies the target explicitly. Otherwise,
    we return an /Fi string that just specifies the first target's
    directory (where the Visual C/C++ compiler will put the .i files).
    """

    # TODO /Fi is not supported on Visual Studio 9.00 (2008) and earlier
    #   https://msdn.microsoft.com/en-us/library/8z9z0bx6(v=vs.90).aspx

    # Fixing MSVC_BATCH mode. Previous if did not work when MSVC_BATCH
    # was set to False. This new version should work better. Removed
    # len(source)==1 as batch mode can compile only one file
    # (and it also fixed problem with compiling only one changed file
    # with batch mode enabled)
    if not "MSVC_BATCH" in env or env.subst("$MSVC_BATCH") in ("0", "False", "", None):
        return "/Fi$TARGET"
    else:
        # The Visual C/C++ compiler requires a \ at the end of the /Fi
        # option to indicate an output directory. We use os.sep here so
        # that the test(s) for this can be run on non-Windows systems
        # without having a hard-coded backslash mess up command-line
        # argument parsing.
        return "/Fi${TARGET.dir}" + os.sep


CPreprocessedAction = SCons.Action.Action(
    "$PPCCCOM", "$PPCCCOMSTR", batch_key=msvc_batch_key, targets="$CHANGED_TARGETS"
)
CXXPreprocessedAction = SCons.Action.Action(
    "$PPCXXCOM", "$PPCXXCOMSTR", batch_key=msvc_batch_key, targets="$CHANGED_TARGETS"
)


def generate_PreprocessedBuilder(env):
    preprocessed = preprocessed_builder.createPreprocessedBuilder(env)

    for suffix in CSuffixes:
        preprocessed.add_action(suffix, CPreprocessedAction)
        preprocessed.add_emitter(suffix, c_preprocessed_emitter)

    for suffix in CXXSuffixes:
        preprocessed.add_action(suffix, CXXPreprocessedAction)
        preprocessed.add_emitter(suffix, cxx_preprocessed_emitter)

    env["_MSVC_PP_OUTPUT_FLAG"] = msvc_pp_output_flag

    # PPCC is the preprocessor-only mode for CC, the C compiler (compare with SHCC et al)
    # TODO For SCons: be smart and when passed a preprocessed file, compiler skips certain options?
    env["PPCC"] = "$CC"
    env["PPCCFLAGS"] = SCons.Util.CLVar("$CCFLAGS")
    env["PPCFLAGS"] = SCons.Util.CLVar("$CFLAGS")
    env["PPCCCOM"] = (
        '${TEMPFILE("$PPCC /P $_MSVC_PP_OUTPUT_FLAG /c $CHANGED_SOURCES $PPCFLAGS $PPCCFLAGS $_CCCOMCOM","$PPCCCOMSTR")}'
    )
    env["PPCXX"] = "$CXX"
    env["PPCXXFLAGS"] = SCons.Util.CLVar("$CXXFLAGS")
    env["PPCXXCOM"] = (
        '${TEMPFILE("$PPCXX /P $_MSVC_PP_OUTPUT_FLAG /c $CHANGED_SOURCES $PPCXXFLAGS $PPCCFLAGS $_CCCOMCOM","$PPCXXCOMSTR")}'
    )

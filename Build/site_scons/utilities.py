"""Miscellaneous utility functions for the build.
"""

import SCons.Util


def GetSharedLibrary(env, target):
    return env.FindIxes(target, "SHLIBPREFIX", "SHLIBSUFFIX")


def GetImportLibrary(env, target):
    # MacOS uses the dll (.dylib) as the import library.
    return env.FindIxes(target, "LIBPREFIX", "LIBSUFFIX") or GetSharedLibrary(env, target)


def GetProgram(env, target):
    # Only Windows uses a suffix. Everything else we hope it's the first target
    return env.FindIxes(target, "PROGPREFIX", "PROGSUFFIX") or SCons.Util.flatten(target)[0]


# This ensures that targets like "test" will fail if Python 3 isn't available
def AliasIfNotEmpty(env, alias, targets=(), action=()):
    """Creates or updates the alias iff targets or action is not empty. Empty aliases are normally
    successful no-ops when specified as build targets on the command line: this ensures that aliases
    that don't actually do anything will cause build failures.
    """
    targets = SCons.Util.flatten(targets)
    action = SCons.Util.flatten(action)
    if not targets and not action:
        return alias
    return env.Alias(alias, targets, action)


# https://stackoverflow.com/a/73436348
def RGlob(env, root_path, pattern, ondisk=True, source=False, strings=False, exclude=None):
    result_nodes = []
    paths = [root_path]
    while paths:
        path = paths.pop()
        all_nodes = env.Glob(f"{path}/*", ondisk=ondisk, source=source, exclude=exclude)
        # `srcnode()` must be used because `isdir()` doesn't work for entries in variant dirs which
        # haven't been copied yet.
        paths.extend(
            entry
            for entry in all_nodes
            if entry.isdir() or (entry.srcnode() and entry.srcnode().isdir())
        )
        result_nodes.extend(
            env.Glob(
                f"{path}/{pattern}", ondisk=ondisk, source=source, strings=strings, exclude=exclude
            )
        )
    return sorted(result_nodes)

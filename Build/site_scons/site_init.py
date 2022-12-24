"""Global variables from this file are automatically added to the SConScript (i.e. make.scons)
"""


from root_environment import SconscriptLog, RootEnv


# This ensures that targets like "test" will fail if Python 3 isn't available
def AliasIfNotEmpty(alias, targets=(), action=()):
    """Creates or updates the alias iff targets or action is not empty. Empty aliases are normally
    successful no-ops when specified as build targets on the command line: this ensures that aliases
    that don't actually do anything will cause build failures.
    """
    targets = Flatten(targets)
    action = Flatten(action)
    if not targets and not action:
        return alias
    return Alias(alias, targets, action)


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

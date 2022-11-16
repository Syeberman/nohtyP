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

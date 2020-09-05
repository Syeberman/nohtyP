import gcc_common
_generate, _exists = gcc_common.DefineGCCToolFunctions(8, major=8)


# Define new functions so that we remain in any stack traces involving these functions
def generate(*args, **kwargs):
    return _generate(*args, **kwargs)


def exists(*args, **kwargs):
    return _exists(*args, **kwargs)

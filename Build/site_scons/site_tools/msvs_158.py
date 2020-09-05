# Visual Studio 2017

import msvs_common
# FIXME SCons needs to be updated to the new VS version detection scheme.
_generate, _exists = msvs_common.DefineMSVSToolFunctions(15.8, ("15.8", ))


# Define new functions so that we remain in any stack traces involving these functions
def generate(*args, **kwargs):
    return _generate(*args, **kwargs)


def exists(*args, **kwargs):
    return _exists(*args, **kwargs)

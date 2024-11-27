# Visual Studio 2019

import msvs_common

_generate, _exists = msvs_common.DefineMSVSToolFunctions(16.0, ("14.2", ))


# Define new functions so that we remain in any stack traces involving these functions
def generate(*args, **kwargs):
    return _generate(*args, **kwargs)


def exists(*args, **kwargs):
    return _exists(*args, **kwargs)

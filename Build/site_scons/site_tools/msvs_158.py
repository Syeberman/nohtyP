# Visual Studio 2017

import msvs_common
# FIXME It seems MS is using the minor version as more of a build number now, so we should just
# accept any "version 15" Visual Studio we find.
_generate, _exists = msvs_common.DefineMSVSToolFunctions(15.8, ("15.8", ))


# Define new functions so that we remain in any stack traces involving these functions
def generate(*args, **kwargs):
    return _generate(*args, **kwargs)


def exists(*args, **kwargs):
    return _exists(*args, **kwargs)

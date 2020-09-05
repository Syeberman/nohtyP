# Visual Studio 2015

import msvs_common
_generate, _exists = msvs_common.DefineMSVSToolFunctions( 14.0, ("14.0", ) )
# Define new functions so that we remain in any stack traces involving these functions
def generate( *args, **kwargs ): return _generate( *args, **kwargs )
def exists( *args, **kwargs ): return _exists( *args, **kwargs )


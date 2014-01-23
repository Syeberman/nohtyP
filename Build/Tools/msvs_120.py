import msvs_common
_generate, _exists = msvs_common.DefineMSVSToolFunctions( 12.0, ("12.0", "12.0Exp") )
# Define new functions so that we remain in any stack traces involving these functions
def generate( *args, **kwargs ): return _generate( *args, **kwargs )
def exists( *args, **kwargs ): return _exists( *args, **kwargs )


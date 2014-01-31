import gcc_common
_generate, _exists = gcc_common.DefineGCCToolFunctions( 4.08, major=4, minor=8 )
# Define new functions so that we remain in any stack traces involving these functions
def generate( *args, **kwargs ): return _generate( *args, **kwargs )
def exists( *args, **kwargs ): return _exists( *args, **kwargs )


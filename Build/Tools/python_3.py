import python_common
_generate, _exists = python_common.DefinePythonToolFunctions( 0x03000000, "3.x" )
# Define new functions so that we remain in any stack traces involving these functions
def generate( *args, **kwargs ): return _generate( *args, **kwargs )
def exists( *args, **kwargs ): return _exists( *args, **kwargs )


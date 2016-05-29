"""checkapi.py -- Static analysis and information for nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import sys
from parse_header import ParseHeader
from api_warnings import CheckEllipsisFunctions

# TODO fake_libc_include doesn't have proper limits or defines for 32- and 64-bit systems
# (does it matter for our purposes?)


# TODO Checks to implement
#   - every (with exceptions) N or K has a NV or KV
#   - every N or K has a variant that takes a ypObject* in its place
#   - exc is always ypObject ** and used in C, F, L, and E
#   - E functions match their originals, except:
#       - first arg is ypObject*, not ypObject**
#       - ypObject** exc is append to arg list, unless orig returns ypObject* (ie yp_popE)
#   - all X functions return a ypObject *
#   - the count equals the actual arg count (minus exc)
#   - C is used iff it contains a C type (an int?)
#   - F is used iff it contains a float
#   - L doesn't use ypObject *
#   - a whitelist of X functions?
#   - determine whether the function mutates or not based on certain patterns (push, pop, set, del,
#   i*, etc, or first param is iterator, file, etc) and ensure ypObject** is used iff it mutates
#   - the "unadorned" version is the one with the fewest arguments (but not zero)

# TODO nohtyP.c checks to implement (in a separate script)
#   - ensure no reference leaks
#   - for functions that return yp_None for success or exactly one exception type, convert to a
#   boolean (on the assumption that it's quicker to test)

def ReportOnVariants(header, *, print=print):
    """Report the functions that belong to the same group, and which is the unadorned version."""
    for root, funcsIter in sorted(header.IterFunctionRoots()):
        funcs = sorted(funcsIter, key=lambda x: x.postfixes)
        if len(funcs) < 2:
            continue
        # if not any( x.name[-1].isdigit( ) for x in funcs ): continue
        print(root)
        for func in funcs:
            print("    ", func)
        print()


def CheckApi(filepath, *, print=print):
    """Reports potential problems in nohtyP.h (and related files)."""
    header = ParseHeader(filepath)

    warnings = []
    CheckEllipsisFunctions(warnings, header)
    for warning in warnings:
        print(warning)

    ReportOnVariants(header, print=print)


# TODO: Proper command line arguments, configurable output file
if __name__ == "__main__":
    CheckApi(sys.argv[1])

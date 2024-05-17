"""checkapi.py -- Static analysis and information for nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import sys
from parse_header import parse_header, ypHeader
from api_warnings import CheckEllipsisFunctions, CheckInputCounts, CheckSetExcFunctions

# TODO fake_libc_include doesn't have proper limits or defines for 32- and 64-bit systems
# (does it matter for our purposes?)


# TODO Checks to implement
#   - every N or K has a variant that takes a ypObject* in its place
#   - exc is always ypObject ** and used in C, F, L, and E
#   - all X functions return a ypObject *
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


def ReportOnVariants(header: ypHeader, *, print=print):
    """Report the functions that belong to the same group, and which is the unadorned version."""
    for root, funcs in sorted(header.root2funcs.items()):
        if len(funcs) < 2:
            continue

        funcs = sorted(funcs, key=lambda x: x.postfixes)
        print(root)
        for func in funcs:
            print("    ", func)
        print()


def CheckApi(filepath: str, *, print=print):
    """Reports potential problems in nohtyP.h (and related files)."""
    header = parse_header(filepath)

    warnings = []
    CheckEllipsisFunctions(warnings, header)
    CheckInputCounts(warnings, header)
    CheckSetExcFunctions(warnings, header)
    for warning in warnings:
        print(warning)
    print()
    print()

    ReportOnVariants(header, print=print)


# TODO: Proper command line arguments, configurable output file
if __name__ == "__main__":
    CheckApi(sys.argv[1])

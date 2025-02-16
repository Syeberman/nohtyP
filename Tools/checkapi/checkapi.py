"""checkapi.py -- Static analysis and information for nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import sys
from parse_header import parse_header, ypHeader
from api_warnings import (
    CheckVarargFunctions,
    CheckParameterCounts,
    CheckSetExcFunctions,
)

# TODO fake_libc_include doesn't have proper limits or defines for 32- and 64-bit systems
# (does it matter for our purposes?)

# TODO Parse a Python signature and compare to the C function, to ensure consistency.

# TODO Python has only a few keyword-only arguments, in max, min, sorted, int.to_bytes,
# int.from_bytes, and list.sort. Translating this to nohtyP can be tricky. Because max and min
# support variable positional arguments (in Python and nohtyP), the version of these functions with
# a key argument were all given the _key prefix (i.e. yp_max_keyN), even in the 2-argument forms
# that take an iterable (i.e. yp_max_key). Here, the consistency among this group of related
# functions was prioritized over following the preferred scheme.
#
# Another complication is sum. yp_sumN makes sense, but then muddies the water when we want the 1-
# and 2-argument forms that take an iterable. The 2-argument form is currently called yp_sum_start,
# rather than yp_sum2, even though start is not a keyword-only argument. (Perhaps Python should have
# made it one...)
#
# The preferred way to handle keyword arguments is sorted/sort, where these arguments become
# positional (i.e. yp_sorted3). This is only possible where variable positional arguments are not,
# and never will be, used. Which is a difficult assertion to make in places. And you may find
# yourself thinking that yp_sortedN would be very convenient, and it would be, but it's almost as
# convenient to to yp_listN then yp_sort, which is why we don't have a yp_sortedN. So perhaps this
# "preferred way" is unrealistic in most cases. (But then yp_sorted_key_reverse, and the three other
# combinations, is very clunky to maintain, and worse the more kw args you add.)
#
# It'd be nice to have these rules formalized somehow in this script, perhaps in combination with
# that "parse Python signatures" idea, because this is an area that invites inconsistency.

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
    CheckVarargFunctions(warnings, header)
    CheckParameterCounts(warnings, header)
    CheckSetExcFunctions(warnings, header)
    for warning in warnings:
        print(warning)
    print()
    print()

    ReportOnVariants(header, print=print)


# TODO: Proper command line arguments, configurable output file
if __name__ == "__main__":
    CheckApi(sys.argv[1])

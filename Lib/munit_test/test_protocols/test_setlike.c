
#include "munit_test/unittest.h"

// XXX Because Python calls both the protocol and the object "set", I'm using the term "set-like" to
// refer to the protocol where there may be confusion.

// Sets should accept themselves, their pairs, iterators, tuple/list, and frozenset/set as valid
// types for the "x" (i.e. "other iterable") argument.
#define x_types_init(type)                                                              \
    {                                                                                   \
        (type), (type)->pair, fixture_type_iter, fixture_type_tuple, fixture_type_list, \
                fixture_type_frozenset, fixture_type_set, NULL                          \
    }


static MunitResult test_isdisjoint(const MunitParameter params[], fixture_t *fixture)
{
    return MUNIT_ERROR;  // FIXME Fill in
}

// FIXME Move test_remove from test_frozenset here.


static MunitParameterEnum test_setlike_params[] = {
        {param_key_type, param_values_types_set}, {NULL}};

MunitTest test_setlike_tests[] = {TEST(test_isdisjoint, test_setlike_params), {NULL}};


// FIXME The protocol and the object share the same name. Distinction between test_setlike and
// test_frozenset is flimsy. Rename?

extern void test_setlike_initialize(void) {}

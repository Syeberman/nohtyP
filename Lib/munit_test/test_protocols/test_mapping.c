
#include "munit_test/unittest.h"

// Mappings should accept themselves, their pairs, iterators, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// FIXME We can't accept just any old iterator...
// FIXME "Dirty" versions like in test_setlike
#define x_types_init(type)                                                                        \
    {                                                                                             \
        (type), (type)->pair, fixture_type_iter, fixture_type_frozendict, fixture_type_dict, NULL \
    }


static MunitResult test_getitem(const MunitParameter params[], fixture_t *fixture)
{
    return MUNIT_FAIL;
}

static MunitParameterEnum test_mapping_params[] = {
        {param_key_type, param_values_types_mapping}, {NULL}};

MunitTest test_mapping_tests[] = {TEST(test_getitem, test_mapping_params), {NULL}};

extern void test_mapping_initialize(void) {}


#include "munit_test/unittest.h"

// Mappings should accept themselves, their pairs, iterators, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// FIXME We can't accept just any old iterator...
// FIXME "Dirty" versions like in test_setlike
#define x_types_init(type)                                                                        \
    {                                                                                             \
        (type), (type)->pair, fixture_type_iter, fixture_type_frozendict, fixture_type_dict, NULL \
    }


// The test_contains in test_collection checks for the behaviour shared amongst all collections;
// this test_contains considers the behaviour unique to mappings.
static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[4];
    obj_array_fill(keys, type->rand_items);

    // Item is unhashable.
    {
        ypObject *mp = type->newN(N(keys[0], keys[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_obj(yp_contains(mp, unhashable), is, yp_False);
        assert_obj(yp_in(unhashable, mp), is, yp_False);
        assert_obj(yp_not_in(unhashable, mp), is, yp_True);
        yp_decrefN(N(mp, unhashable));
    }

    // An unhashable x should match the equal object in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *mp = type->newN(N(int_1));
        assert_obj(yp_contains(mp, intstore_1), is, yp_True);
        assert_obj(yp_in(intstore_1, mp), is, yp_True);
        assert_obj(yp_not_in(intstore_1, mp), is, yp_False);
        yp_decrefN(N(int_1, intstore_1, mp));
    }

    obj_array_decref(keys);
    return MUNIT_OK;
}

static MunitResult test_getitem(const MunitParameter params[], fixture_t *fixture)
{
    return MUNIT_FAIL;
}

static MunitParameterEnum test_mapping_params[] = {
        {param_key_type, param_values_types_mapping}, {NULL}};

MunitTest test_mapping_tests[] = {
        TEST(test_contains, test_mapping_params), TEST(test_getitem, test_mapping_params), {NULL}};

extern void test_mapping_initialize(void) {}

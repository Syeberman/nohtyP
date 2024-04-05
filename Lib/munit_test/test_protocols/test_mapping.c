
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
    ypObject       *keys[2];
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

    // An unhashable x should match the equal key in mp.
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

// FIXME Here and everywhere, an "original object return" flag, with tests to ensure exact object
// returned.
static MunitResult test_getitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[3];
    ypObject       *values[3];
    ypObject       *unhashable;
    ypObject       *mp;
    ypObject       *empty = type->newK(0);
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);
    unhashable = rand_obj_any_mutable_unique(2, keys);
    mp = type->newK(K(keys[0], values[0], keys[1], values[1]));

    // Basic key.
    ead(value, yp_getitem(mp, keys[0]), assert_obj(value, eq, values[0]));
    ead(value, yp_getitem(mp, keys[1]), assert_obj(value, eq, values[1]));

    // Unknown key.
    assert_raises(yp_getitem(mp, keys[2]), yp_KeyError);

    // Empty mp.
    assert_raises(yp_getitem(empty, keys[0]), yp_KeyError);

    // x is mp.
    // FIXME Similar tests everywhere.
    assert_raises(yp_getitem(mp, mp), yp_KeyError);

    // x is unhashable.
    assert_raises(yp_getitem(mp, unhashable), yp_KeyError);

    // An unhashable x should match the equal key in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *mp_int_1 = type->newK(K(int_1, values[0]));
        ead(value, yp_getitem(mp_int_1, intstore_1), assert_obj(value, eq, values[0]));
        yp_decrefN(N(int_1, intstore_1, mp_int_1));
    }

    // Exception passthrough.
    assert_isexception(yp_getitem(mp, yp_SyntaxError), yp_SyntaxError);

    assert_mapping(mp, keys[0], values[0], keys[1], values[1]);  // mp unchanged

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(unhashable, mp, empty));
    return MUNIT_OK;
}

static MunitResult test_getdefault(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[3];
    ypObject       *values[3];
    ypObject       *unhashable;
    ypObject       *mp;
    ypObject       *empty = type->newK(0);
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);
    unhashable = rand_obj_any_mutable_unique(2, keys);
    mp = type->newK(K(keys[0], values[0], keys[1], values[1]));

    // Basic key.
    ead(value, yp_getdefault(mp, keys[0], values[2]), assert_obj(value, eq, values[0]));
    ead(value, yp_getdefault(mp, keys[1], values[2]), assert_obj(value, eq, values[1]));

    // Unknown key.
    ead(value, yp_getdefault(mp, keys[2], values[2]), assert_obj(value, eq, values[2]));

    // Exception-as-default.
    assert_raises(yp_getdefault(mp, keys[2], yp_SyntaxError), yp_SyntaxError);

    // Empty mp.
    ead(value, yp_getdefault(empty, keys[0], values[2]), assert_obj(value, eq, values[2]));

    // x is mp.
    ead(value, yp_getdefault(mp, mp, values[2]), assert_obj(value, eq, values[2]));

    // x is unhashable.
    ead(value, yp_getdefault(mp, unhashable, values[2]), assert_obj(value, eq, values[2]));

    // An unhashable x should match the equal key in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *mp_int_1 = type->newK(K(int_1, values[0]));
        ead(value, yp_getdefault(mp_int_1, intstore_1, values[2]),
                assert_obj(value, eq, values[0]));
        yp_decrefN(N(int_1, intstore_1, mp_int_1));
    }

    // Exception passthrough.
    assert_isexception(yp_getdefault(mp, yp_SyntaxError, values[2]), yp_SyntaxError);

    assert_mapping(mp, keys[0], values[0], keys[1], values[1]);  // mp unchanged

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(unhashable, mp, empty));
    return MUNIT_OK;
}

static MunitParameterEnum test_mapping_params[] = {
        {param_key_type, param_values_types_mapping}, {NULL}};

MunitTest test_mapping_tests[] = {TEST(test_contains, test_mapping_params),
        TEST(test_getitem, test_mapping_params), TEST(test_getdefault, test_mapping_params),
        {NULL}};

extern void test_mapping_initialize(void) {}

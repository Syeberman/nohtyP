
#include "munit_test/unittest.h"

// FIXME _ypDict_deepcopy
// FIXME frozendict_eq where the hash has been cached (introduce _test_comparisons)
// FIXME The various dict iterators
// FIXME test_objects/test_frozendict

// Mappings should accept themselves, their pairs, iterators, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// FIXME We can't accept just any old iterator...
// FIXME "Dirty" versions like in test_setlike, and "shared key"?
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

    // Previously-deleted key.
    if (type->is_mutable) {
        ypObject *mp_delitem = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp_delitem, keys[1], &exc));
        assert_mapping(mp_delitem, keys[0], values[0]);
        assert_raises(yp_getitem(mp_delitem, keys[1]), yp_KeyError);
        assert_mapping(mp_delitem, keys[0], values[0]);
        yp_decrefN(N(mp_delitem));
    }

    // Empty mp.
    assert_raises(yp_getitem(empty, keys[0]), yp_KeyError);

    // key is mp.
    // FIXME Similar tests everywhere.
    assert_raises(yp_getitem(mp, mp), yp_KeyError);

    // key is unhashable.
    assert_raises(yp_getitem(mp, unhashable), yp_KeyError);

    // An unhashable key should match the equal key in mp.
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

    // Previously-deleted key.
    if (type->is_mutable) {
        ypObject *mp_delitem = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp_delitem, keys[1], &exc));
        assert_mapping(mp_delitem, keys[0], values[0]);
        ead(value, yp_getdefault(mp_delitem, keys[1], values[2]), assert_obj(value, eq, values[2]));
        assert_mapping(mp_delitem, keys[0], values[0]);
        yp_decrefN(N(mp_delitem));
    }

    // Exception-as-default.
    assert_raises(yp_getdefault(mp, keys[2], yp_SyntaxError), yp_SyntaxError);

    // Empty mp.
    ead(value, yp_getdefault(empty, keys[0], values[2]), assert_obj(value, eq, values[2]));

    // key is mp.
    ead(value, yp_getdefault(mp, mp, values[2]), assert_obj(value, eq, values[2]));

    // key is unhashable.
    ead(value, yp_getdefault(mp, unhashable, values[2]), assert_obj(value, eq, values[2]));

    // An unhashable key should match the equal key in mp.
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

static MunitResult test_setitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[3];
    ypObject       *values[3];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support setitem.
    if (!type->is_mutable) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_setitem(mp, keys[2], values[2], &exc), yp_MethodError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_setitem(mp, keys[0], values[2], &exc));
        assert_mapping(mp, keys[0], values[2], keys[1], values[1]);
        yp_decref(mp);
    }

    // Unknown key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_setitem(mp, keys[2], values[2], &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
    }

    // Previously-deleted key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp, keys[1], &exc));
        assert_mapping(mp, keys[0], values[0]);
        assert_not_raises_exc(yp_setitem(mp, keys[1], values[2], &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[2]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        assert_not_raises_exc(yp_setitem(mp, keys[2], values[2], &exc));
        assert_mapping(mp, keys[2], values[2]);
        yp_decref(mp);
    }

    // key is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_setitem(mp, mp, values[2], &exc), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // x is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_setitem(mp, keys[0], mp, &exc));
        assert_mapping(mp, keys[0], mp, keys[1], values[1]);
        assert_not_raises_exc(yp_clear(mp, &exc));  // nohtyP does not yet break circular refs.
        yp_decref(mp);
    }

    // key is unhashable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_raises_exc(yp_setitem(mp, unhashable, values[2], &exc), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, unhashable));
    }

    // Unhashable keys should always cause TypeError, even if that key is already in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *mp = type->newK(K(int_1, values[0]));
        assert_raises_exc(yp_setitem(mp, intstore_1, values[2], &exc), yp_TypeError);
        assert_mapping(mp, int_1, values[0]);
        yp_decrefN(N(int_1, intstore_1, mp));
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_isexception_exc(yp_setitem(mp, yp_SyntaxError, values[2], &exc), yp_SyntaxError);
        assert_isexception_exc(yp_setitem(mp, keys[2], yp_SyntaxError, &exc), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
}

static MunitResult test_delitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[3];
    ypObject       *values[3];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support delitem.
    if (!type->is_mutable) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_delitem(mp, keys[2], &exc), yp_MethodError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp, keys[0], &exc));
        assert_mapping(mp, keys[1], values[1]);
        yp_decref(mp);
    }

    // Unknown key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_delitem(mp, keys[2], &exc), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // Previously-deleted key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp, keys[1], &exc));
        assert_mapping(mp, keys[0], values[0]);
        assert_raises_exc(yp_delitem(mp, keys[1], &exc), yp_KeyError);
        assert_mapping(mp, keys[0], values[0]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        assert_raises_exc(yp_delitem(mp, keys[2], &exc), yp_KeyError);
        assert_len(mp, 0);
        yp_decref(mp);
    }

    // key is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_delitem(mp, mp, &exc), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // key is unhashable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_raises_exc(yp_delitem(mp, unhashable, &exc), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, unhashable));
    }

    // An unhashable key should match the equal key in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *mp = type->newK(K(int_1, values[0]));
        assert_not_raises_exc(yp_delitem(mp, intstore_1, &exc));
        assert_len(mp, 0);
        yp_decrefN(N(int_1, intstore_1, mp));
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_isexception_exc(yp_delitem(mp, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
}

static MunitResult test_popvalue(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[4];
    ypObject       *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support popvalue.
    if (!type->is_mutable) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        assert_raises(yp_popvalue3(mp, keys[2], values[3]), yp_MethodError);
        assert_raises(yp_popvalue2(mp, keys[1]), yp_MethodError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ead(value, yp_popvalue3(mp, keys[2], values[3]), assert_obj(value, eq, values[2]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        ead(value, yp_popvalue2(mp, keys[1]), assert_obj(value, eq, values[1]));
        assert_mapping(mp, keys[0], values[0]);
        yp_decref(mp);
    }

    // Unknown key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ead(value, yp_popvalue3(mp, keys[3], values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue3(mp, keys[3], yp_SyntaxError), yp_SyntaxError);
        assert_raises(yp_popvalue2(mp, keys[3]), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
    }

    // Previously-deleted key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        assert_not_raises_exc(yp_delitem(mp, keys[2], &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        ead(value, yp_popvalue3(mp, keys[2], values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue3(mp, keys[2], yp_SyntaxError), yp_SyntaxError);
        assert_raises(yp_popvalue2(mp, keys[2]), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        ead(value, yp_popvalue3(mp, keys[3], values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue3(mp, keys[3], yp_SyntaxError), yp_SyntaxError);
        assert_raises(yp_popvalue2(mp, keys[3]), yp_KeyError);
        assert_len(mp, 0);
        yp_decref(mp);
    }

    // key is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ead(value, yp_popvalue3(mp, mp, values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue3(mp, mp, yp_SyntaxError), yp_SyntaxError);
        assert_raises(yp_popvalue2(mp, mp), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
    }

    // key is unhashable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        ead(value, yp_popvalue3(mp, unhashable, values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue3(mp, unhashable, yp_SyntaxError), yp_SyntaxError);
        assert_raises(yp_popvalue2(mp, unhashable), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decrefN(N(mp, unhashable));
    }

    // An unhashable key should match the equal key in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *int_2 = yp_intC(2);
        ypObject *intstore_2 = yp_intstoreC(2);
        ypObject *mp = type->newK(K(int_1, values[0], int_2, values[1]));
        ead(value, yp_popvalue3(mp, intstore_1, values[3]), assert_obj(value, eq, values[0]));
        assert_mapping(mp, int_2, values[1]);
        ead(value, yp_popvalue2(mp, intstore_2), assert_obj(value, eq, values[1]));
        assert_len(mp, 0);
        yp_decrefN(N(int_1, intstore_1, int_2, intstore_2, mp));
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        assert_isexception(yp_popvalue3(mp, yp_SyntaxError, values[3]), yp_SyntaxError);
        assert_isexception(yp_popvalue2(mp, yp_SyntaxError), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
}

static MunitResult test_popitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    yp_ssize_t      i;
    ypObject       *keys[6];
    ypObject       *values[6];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support popitem.
    if (!type->is_mutable) {
        ypObject *key, *value;
        ypObject *mp = type->newK(K(keys[0], values[0]));
        yp_popitem(mp, &key, &value);
        assert_raises(key, yp_MethodError);
        assert_obj(value, is, key);
        assert_mapping(mp, keys[0], values[0]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic popitem.
    {
        ypObject *key, *value;
        ypObject *mp = type->newK(K(keys[0], values[0]));
        yp_popitem(mp, &key, &value);
        assert_obj(key, eq, keys[0]);
        assert_obj(value, eq, values[0]);
        assert_len(mp, 0);
        yp_decrefN(N(mp, key, value));
    }

    // Empty mp.
    {
        ypObject *key, *value;
        ypObject *mp = type->newK(0);
        yp_popitem(mp, &key, &value);
        assert_raises(key, yp_KeyError);
        assert_obj(value, is, key);
        assert_len(mp, 0);
        yp_decref(mp);
    }

    // Multiple popitems. Order is arbitrary, so run through a few different items.
    for (i = 0; i < 5; i++) {
        ypObject *key_0 = keys[0 + i];      // borrowed
        ypObject *value_0 = values[0 + i];  // borrowed
        ypObject *key_1 = keys[1 + i];      // borrowed
        ypObject *value_1 = values[1 + i];  // borrowed
        ypObject *mp = type->newK(K(key_0, value_0, key_1, value_1));
        ypObject *first_key, *first_value;
        ypObject *second_key, *second_value;
        ypObject *error_key, *error_value;
        yp_popitem(mp, &first_key, &first_value);
        if (yp_eq(first_key, key_0) == yp_True) {
            assert_obj(first_value, eq, value_0);
            assert_mapping(mp, key_1, value_1);
            yp_popitem(mp, &second_key, &second_value);
            assert_obj(second_key, eq, key_1);
            assert_obj(second_value, eq, value_1);
        } else {
            assert_obj(first_key, eq, key_1);
            assert_obj(first_value, eq, value_1);
            assert_mapping(mp, key_0, value_0);
            yp_popitem(mp, &second_key, &second_value);
            assert_obj(second_key, eq, key_0);
            assert_obj(second_value, eq, value_0);
        }
        assert_len(mp, 0);
        yp_popitem(mp, &error_key, &error_value);
        assert_raises(error_key, yp_KeyError);
        assert_obj(error_value, is, error_key);
        assert_len(mp, 0);
        yp_popitem(mp, &error_key, &error_value);  // Calling again still raises KeyError.
        assert_raises(error_key, yp_KeyError);
        assert_obj(error_value, is, error_key);
        assert_len(mp, 0);
        yp_decrefN(N(mp, first_key, first_value, second_key, second_value));
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
}

static MunitResult test_setdefault(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[3];
    ypObject       *values[3];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support setdefault.
    if (!type->is_mutable) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises(yp_setdefault(mp, keys[0], values[2]), yp_MethodError);
        assert_raises(yp_setdefault(mp, keys[2], values[2]), yp_MethodError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ead(value, yp_setdefault(mp, keys[0], values[2]), assert_obj(value, eq, values[0]));
        ead(value, yp_setdefault(mp, keys[1], values[2]), assert_obj(value, eq, values[1]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // Unknown key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ead(value, yp_setdefault(mp, keys[2], values[2]), assert_obj(value, eq, values[2]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
    }

    // Previously-deleted key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp, keys[1], &exc));
        assert_mapping(mp, keys[0], values[0]);
        ead(value, yp_setdefault(mp, keys[1], values[2]), assert_obj(value, eq, values[2]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[2]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        ead(value, yp_setdefault(mp, keys[2], values[2]), assert_obj(value, eq, values[2]));
        assert_mapping(mp, keys[2], values[2]);
        yp_decref(mp);
    }

    // key is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises(yp_setdefault(mp, mp, values[2]), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // x is mp; key in mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ead(value, yp_setdefault(mp, keys[0], mp), assert_obj(value, eq, values[0]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // x is mp; key not in mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ead(value, yp_setdefault(mp, keys[2], mp), assert_obj(value, eq, mp));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], mp);
        assert_not_raises_exc(yp_clear(mp, &exc));  // nohtyP does not yet break circular refs.
        yp_decref(mp);
    }

    // key is unhashable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_raises(yp_setdefault(mp, unhashable, values[2]), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, unhashable));
    }

    // Unhashable keys should always cause TypeError, even if that key is already in mp.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *mp = type->newK(K(int_1, values[0]));
        assert_raises(yp_setdefault(mp, intstore_1, values[2]), yp_TypeError);
        assert_mapping(mp, int_1, values[0]);
        yp_decrefN(N(int_1, intstore_1, mp));
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_isexception(yp_setdefault(mp, yp_SyntaxError, values[2]), yp_SyntaxError);
        assert_isexception(yp_setdefault(mp, keys[2], yp_SyntaxError), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
}

static MunitResult _test_updateK(fixture_type_t *type,
        void (*any_updateK)(ypObject *, ypObject **, int, ...), int test_unhashables)
{
    ypObject *int_1 = yp_intC(1);
    ypObject *intstore_1 = yp_intstoreC(1);
    ypObject *keys[6];
    ypObject *values[6];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support update.
    if (!type->is_mutable) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(
                any_updateK(mp, &exc, K(keys[1], values[3], keys[2], values[2])), yp_MethodError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic update: keys[0] only in mp, keys[1] in both, keys[2] only in varargs.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(any_updateK(mp, &exc, K(keys[1], values[3], keys[2], values[2])));
        assert_mapping(mp, keys[0], values[0], keys[1], values[3], keys[2], values[2]);
        yp_decref(mp);
    }

    // Previously-deleted key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp, keys[1], &exc));
        assert_mapping(mp, keys[0], values[0]);
        assert_not_raises_exc(any_updateK(mp, &exc, K(keys[1], values[3])));
        assert_mapping(mp, keys[0], values[0], keys[1], values[3]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        assert_not_raises_exc(any_updateK(mp, &exc, K(keys[0], values[0])));
        assert_mapping(mp, keys[0], values[0]);
        yp_decref(mp);
    }

    // varargs is empty.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(any_updateK(mp, &exc, 0));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // Both are empty.
    {
        ypObject *mp = type->newK(0);
        assert_not_raises_exc(any_updateK(mp, &exc, 0));
        assert_len(mp, 0);
        yp_decref(mp);
    }

    // key is mp.
    if (test_unhashables) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(any_updateK(mp, &exc, K(mp, values[3])), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // value is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(any_updateK(mp, &exc, K(keys[1], mp)));
        assert_mapping(mp, keys[0], values[0], keys[1], mp);
        assert_not_raises_exc(yp_clear(mp, &exc));  // nohtyP does not yet break circular refs.
        yp_decref(mp);
    }

    // varargs contains duplicates; last value is retained.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(any_updateK(mp, &exc,
                K(keys[1], values[2], keys[1], values[3], keys[2], values[4], keys[2], values[5])));
        assert_mapping(mp, keys[0], values[0], keys[1], values[3], keys[2], values[5]);
        yp_decref(mp);
    }

    // key is unhashable.
    if (test_unhashables) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_raises_exc(any_updateK(mp, &exc, K(unhashable, values[2])), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, unhashable));
    }

    // Unhashable key rejected even if equal to other hashable key.
    if (test_unhashables) {
        ypObject *mp = type->newK(0);
        assert_raises_exc(
                any_updateK(mp, &exc, K(intstore_1, values[1], int_1, values[2])), yp_TypeError);
        assert_len(mp, 0);
        assert_raises_exc(
                any_updateK(mp, &exc, K(int_1, values[1], intstore_1, values[2])), yp_TypeError);
        assert_mapping(mp, int_1, values[1]);  // Optimization: updateK adds while it iterates.
        yp_decrefN(N(mp));
    }

    // Unhashable keys should always cause TypeError, even if that key is already in mp.
    if (test_unhashables) {
        ypObject *mp = type->newK(K(int_1, values[0]));
        assert_raises_exc(any_updateK(mp, &exc, K(intstore_1, values[2])), yp_TypeError);
        assert_mapping(mp, int_1, values[0]);
        yp_decrefN(N(mp));
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(int_1, intstore_1));
    return MUNIT_OK;
}

static void updateK_to_updateKV(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list args;
    va_start(args, n);
    yp_updateKV(mapping, exc, n, args);
    va_end(args);
}

static MunitResult test_updateK(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    MunitResult     test_result;
    ypObject       *keys[6];
    ypObject       *values[6];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    test_result = _test_updateK(type, yp_updateK, /*test_unhashables=*/TRUE);
    if (test_result != MUNIT_OK) goto tear_down;

    test_result = _test_updateK(type, updateK_to_updateKV, /*test_unhashables=*/TRUE);
    if (test_result != MUNIT_OK) goto tear_down;

    // Remaining tests only apply to mutable objects.
    if (!type->is_mutable) goto tear_down;

    // Optimization: we add directly to mp from the varargs. Unfortunately, if varargs contains
    // an exception mid-way, mp may have already been modified.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_updateK(mp, &exc, K(keys[1], values[3], yp_SyntaxError, values[2])),
                yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[3]);
        assert_raises_exc(
                updateK_to_updateKV(mp, &exc, K(keys[1], values[4], yp_SyntaxError, values[2])),
                yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[4]);
        yp_decref(mp);
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_isexception_exc(yp_updateK(mp, &exc, K(yp_SyntaxError, values[2])), yp_SyntaxError);
        assert_isexception_exc(yp_updateK(mp, &exc, K(keys[2], yp_SyntaxError)), yp_SyntaxError);
        assert_isexception_exc(
                updateK_to_updateKV(mp, &exc, K(yp_SyntaxError, values[2])), yp_SyntaxError);
        assert_isexception_exc(
                updateK_to_updateKV(mp, &exc, K(keys[2], yp_SyntaxError)), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return test_result;
}

static void updateK_to_update_fromfrozendict(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    assert_not_raises(x = yp_frozendictKV(n, args));
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
}

static void updateK_to_update_fromdict(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    assert_not_raises(x = yp_dictKV(n, args));
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
}

static void updateK_to_update_fromiter(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    x = new_itemsKV(fixture_type_iter, n, args);
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
}

static MunitResult test_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    MunitResult     test_result;
    ypObject       *int_1 = yp_intC(1);
    ypObject       *keys[6];
    ypObject       *values[6];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // FIXME integrate with x_types_init?
    test_result = _test_updateK(type, updateK_to_update_fromfrozendict, /*test_unhashables=*/FALSE);
    if (test_result != MUNIT_OK) goto tear_down;

    test_result = _test_updateK(type, updateK_to_update_fromdict, /*test_unhashables=*/FALSE);
    if (test_result != MUNIT_OK) goto tear_down;

    test_result = _test_updateK(type, updateK_to_update_fromiter, /*test_unhashables=*/TRUE);
    if (test_result != MUNIT_OK) goto tear_down;

    // Remaining tests only apply to mutable objects.
    if (!type->is_mutable) goto tear_down;

    // x can be mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_update(mp, mp, &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp));
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1])), x,
            new_itemsK(fixture_type_list, K(keys[1], values[3], keys[2], values[2])),
            yp_update(mp, x, &exc),
            assert_mapping(mp, keys[0], values[0], keys[1], values[3], keys[2], values[2]),
            yp_decref(mp));

    // mp is not modified if the iterator fails at the start.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *x_supplier =
                new_itemsK(fixture_type_list, K(keys[1], values[3], keys[2], values[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(mp, x, &exc), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, x_supplier, x));
    }

    // Optimization: we add directly to mp from the iterator. Unfortunately, if the iterator
    // fails mid-way mp may have already been modified.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *x_supplier =
                new_itemsK(fixture_type_list, K(keys[1], values[3], keys[2], values[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(mp, x, &exc), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[3]);
        yp_decrefN(N(mp, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_update(mp, int_1, &exc), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp));
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(yp_update(mp, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp));
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(int_1));
    return test_result;
}

static MunitParameterEnum test_mapping_params[] = {
        {param_key_type, param_values_types_mapping}, {NULL}};

MunitTest test_mapping_tests[] = {TEST(test_contains, test_mapping_params),
        TEST(test_getitem, test_mapping_params), TEST(test_getdefault, test_mapping_params),
        TEST(test_setitem, test_mapping_params), TEST(test_delitem, test_mapping_params),
        TEST(test_popvalue, test_mapping_params), TEST(test_popitem, test_mapping_params),
        TEST(test_setdefault, test_mapping_params), TEST(test_updateK, test_mapping_params),
        TEST(test_update, test_mapping_params), {NULL}};

extern void test_mapping_initialize(void) {}

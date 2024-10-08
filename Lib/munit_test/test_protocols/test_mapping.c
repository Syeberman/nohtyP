
#include "munit_test/unittest.h"

// Mappings should accept themselves, their pairs, iterators, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// TODO "Shared key" versions, somehow? fixture_type_frozendict_shared, fixture_type_dict_shared
#define x_types_init(type)                                                             \
    {(type), (type)->pair, fixture_type_iter, fixture_type_tuple, fixture_type_list,   \
            fixture_type_frozendict, fixture_type_dict, fixture_type_frozendict_dirty, \
            fixture_type_dict_dirty, NULL}

// Returns true iff type supports comparison operators (eq/etc) with other.
static int type_is_comparable(fixture_type_t *type, fixture_type_t *other)
{
    return type->type == other->type || type->type == other->pair->type;
}


// The test_contains in test_collection checks for the behaviour shared amongst all collections;
// this test_contains considers the behaviour unique to mappings.
static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *keys[2];
    obj_array_fill(keys, type->rand_items);

    // Previously-deleted key.
    if (type->is_mutable) {
        ypObject *self = type->newN(N(keys[0], keys[1]));
        assert_not_raises_exc(yp_delitem(self, keys[0], &exc));
        assert_obj(yp_contains(self, keys[0]), is, yp_False);
        assert_obj(yp_in(keys[0], self), is, yp_False);
        assert_obj(yp_not_in(keys[0], self), is, yp_True);
        yp_decrefN(N(self));
    }

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
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *mp = type->newN(N(pair.hashable));
        assert_obj(yp_contains(mp, pair.unhashable), is, yp_True);
        assert_obj(yp_in(pair.unhashable, mp), is, yp_True);
        assert_obj(yp_not_in(pair.unhashable, mp), is, yp_False);
        yp_decrefN(N(pair.hashable, pair.unhashable, mp));
    }

    obj_array_decref(keys);
    return MUNIT_OK;
}

// expected is the boolean expected to be returned.
static void _test_comparisons_not_supported(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject                   *expected)
{
    ypObject *keys[2];
    ypObject *values[2];
    ypObject *mp;
    ypObject *empty = type->newK(0);
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);
    mp = type->newK(K(keys[0], values[0], keys[1], values[1]));

    ead(x, rand_obj(x_type), assert_obj(any_cmp(mp, x), is, expected));
    ead(x, rand_obj(x_type), assert_obj(any_cmp(empty, x), is, expected));

    if (x_type->is_collection) {
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1])),
                assert_obj(any_cmp(mp, x), is, expected));
        ead(x, new_itemsK(x_type, 0), assert_obj(any_cmp(mp, x), is, expected));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1])),
                assert_obj(any_cmp(empty, x), is, expected));
        ead(x, new_itemsK(x_type, 0), assert_obj(any_cmp(empty, x), is, expected));
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(mp, empty));
}

static void _test_comparisons(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject *x_same, ypObject *x_different)
{
    ypObject *keys[4];
    ypObject *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Non-empty mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));

        // x has the same items.
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1])),
                assert_obj(any_cmp(mp, x), is, x_same));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[0], values[0], keys[1], values[1])),
                assert_obj(any_cmp(mp, x), is, x_same));

        // x has the same keys with different values.
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[1], values[3])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[2], keys[1], values[1])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[2], keys[1], values[3])),
                assert_obj(any_cmp(mp, x), is, x_different));

        // x is empty.
        ead(x, new_itemsK(x_type, 0), assert_obj(any_cmp(mp, x), is, x_different));

        // x is is a proper subset and is not empty.
        ead(x, new_itemsK(x_type, K(keys[0], values[0])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[1], values[1])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[0], values[0])),
                assert_obj(any_cmp(mp, x), is, x_different));

        // x is a proper superset.
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1], keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x,
                new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1], keys[2], values[2],
                                           keys[3], values[3])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x,
                new_itemsK(x_type, K(keys[0], values[0], keys[0], values[0], keys[1], values[1],
                                           keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x,
                new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1], keys[2], values[2],
                                           keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));

        // x overlaps and contains additional items.
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[2], values[2], keys[3], values[3])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[1], values[1], keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[1], values[1], keys[2], values[2], keys[3], values[3])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[0], values[0], keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[2], values[2], keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));

        // x does not overlap and contains additional items.
        ead(x, new_itemsK(x_type, K(keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[2], values[2], keys[3], values[3])),
                assert_obj(any_cmp(mp, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[2], values[2], keys[2], values[2])),
                assert_obj(any_cmp(mp, x), is, x_different));

        // x is mp.
        assert_obj(any_cmp(mp, mp), is, x_same);

        // x contains mp.
        if (!x_type->hashable_items_only || object_is_hashable(mp)) {
            ead(x, new_itemsK(x_type, K(mp, values[0])),
                    assert_obj(any_cmp(mp, x), is, x_different));
        } else {
            assert_raises(new_itemsK(x_type, K(mp, values[0])), yp_TypeError);
        }
        ead(x, new_itemsK(x_type, K(keys[0], mp)), assert_obj(any_cmp(mp, x), is, x_different));

        // Exception passthrough.
        assert_isexception(any_cmp(mp, yp_SyntaxError), yp_SyntaxError);

        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);  // mp unchanged.
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *empty = type->newK(0);

        // Non-empty x.
        ead(x, new_itemsK(x_type, K(keys[0], values[0])),
                assert_obj(any_cmp(empty, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[1], values[1])),
                assert_obj(any_cmp(empty, x), is, x_different));
        ead(x, new_itemsK(x_type, K(keys[0], values[0], keys[0], values[0])),
                assert_obj(any_cmp(empty, x), is, x_different));

        // Empty x.
        ead(x, new_itemsK(x_type, 0), assert_obj(any_cmp(empty, x), is, x_same));

        // x is mp.
        assert_obj(any_cmp(empty, empty), is, x_same);

        // x contains mp.
        if (!x_type->hashable_items_only || !type->is_mutable) {
            ead(x, new_itemsK(x_type, K(empty, values[0])),
                    assert_obj(any_cmp(empty, x), is, x_different));
        } else {
            assert_raises(new_itemsK(x_type, K(empty, values[0])), yp_TypeError);
        }
        ead(x, new_itemsK(x_type, K(keys[0], empty)),
                assert_obj(any_cmp(empty, x), is, x_different));

        // Exception passthrough.
        assert_isexception(any_cmp(empty, yp_SyntaxError), yp_SyntaxError);

        assert_len(empty, 0);  // empty unchanged.
        yp_decref(empty);
    }

    // Implementations may use the cached hash as a quick inequality test. Recall that only
    // immutables can cache their hash, which occurs when yp_hashC is called. Also recall that all
    // contained objects must be hashable, so we need a separate set of hashable values. Because the
    // cached hash is an internal optimization, it should only be used with friendly types.
    if (!type->is_mutable && !x_type->is_mutable && type_is_comparable(type, x_type)) {
        yp_ssize_t i, j;
        ypObject  *h_values[4];  // hashable values
        ypObject  *mp;
        ypObject  *empty = type->newK(0);
        obj_array_fill(h_values, type->rand_items);  // use rand_items to get hashable values
        mp = type->newK(K(keys[0], h_values[0], keys[1], h_values[1]));

        // Run the tests twice: once where mp has not cached the hash, and once where it has.
        for (i = 0; i < 2; i++) {
            ypObject *x_is_same = new_itemsK(x_type, K(keys[0], h_values[0], keys[1], h_values[1]));
            ypObject *x_ne_value =
                    new_itemsK(x_type, K(keys[0], h_values[0], keys[1], h_values[3]));
            ypObject *x_is_empty = new_itemsK(x_type, 0);
            ypObject *x_is_subset = new_itemsK(x_type, K(keys[0], h_values[0]));
            ypObject *x_is_superset = new_itemsK(
                    x_type, K(keys[0], h_values[0], keys[1], h_values[1], keys[2], h_values[2]));
            ypObject *x_is_overlapped =
                    new_itemsK(x_type, K(keys[0], h_values[0], keys[2], h_values[2]));
            ypObject *x_is_not_overlapped = new_itemsK(x_type, K(keys[2], h_values[2]));

            // Run the tests twice: once where x has not cached the hash, and once where it has.
            for (j = 0; j < 2; j++) {
                assert_obj(any_cmp(mp, x_is_same), is, x_same);
                assert_obj(any_cmp(mp, x_ne_value), is, x_different);
                assert_obj(any_cmp(mp, x_is_empty), is, x_different);
                assert_obj(any_cmp(mp, x_is_subset), is, x_different);
                assert_obj(any_cmp(mp, x_is_superset), is, x_different);
                assert_obj(any_cmp(mp, x_is_overlapped), is, x_different);
                assert_obj(any_cmp(mp, x_is_not_overlapped), is, x_different);

                assert_obj(any_cmp(empty, x_is_same), is, x_different);
                assert_obj(any_cmp(empty, x_is_empty), is, x_same);

                // Trigger the hash to be cached on "x" and try again.
                assert_not_raises_exc(yp_hashC(x_is_same, &exc));
                assert_not_raises_exc(yp_hashC(x_ne_value, &exc));
                assert_not_raises_exc(yp_hashC(x_is_empty, &exc));
                assert_not_raises_exc(yp_hashC(x_is_subset, &exc));
                assert_not_raises_exc(yp_hashC(x_is_superset, &exc));
                assert_not_raises_exc(yp_hashC(x_is_overlapped, &exc));
                assert_not_raises_exc(yp_hashC(x_is_not_overlapped, &exc));
            }

            assert_obj(any_cmp(mp, mp), is, x_same);
            assert_obj(any_cmp(empty, empty), is, x_same);

            // Trigger the hash to be cached on "mp" and try again.
            assert_not_raises_exc(yp_hashC(mp, &exc));
            assert_not_raises_exc(yp_hashC(empty, &exc));

            yp_decrefN(N(x_is_same, x_ne_value, x_is_empty, x_is_subset, x_is_superset,
                    x_is_overlapped, x_is_not_overlapped));
        }

        yp_decrefN(N(mp, empty));
        obj_array_decref(h_values);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
}

static MunitResult test_eq(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;

    // eq is only supported for friendly x. All others compare unequal.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_is_comparable(type, (*x_type))) {
            _test_comparisons(type, (*x_type), yp_eq, /*x_same=*/yp_True,
                    /*x_different=*/yp_False);
        } else {
            _test_comparisons_not_supported(type, *x_type, yp_eq, yp_False);
        }
    }

    // _test_comparisons_faulty_iter not called as eq doesn't support iterators.

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_eq, yp_False);
    }

    return MUNIT_OK;
}

static MunitResult test_ne(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;

    // ne is only supported for friendly x. All others compare unequal.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_is_comparable(type, (*x_type))) {
            _test_comparisons(type, (*x_type), yp_ne, /*x_same=*/yp_False,
                    /*x_different=*/yp_True);
        } else {
            _test_comparisons_not_supported(type, *x_type, yp_ne, yp_True);
        }
    }

    // _test_comparisons_faulty_iter not called as ne doesn't support iterators.

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_ne, yp_True);
    }

    return MUNIT_OK;
}

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
    assert_raises(yp_getitem(mp, mp), yp_KeyError);

    // key is unhashable.
    assert_raises(yp_getitem(mp, unhashable), yp_KeyError);

    // An unhashable key should match the equal key in mp.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *mp_pair = type->newK(K(pair.hashable, values[0]));
        ead(value, yp_getitem(mp_pair, pair.unhashable), assert_obj(value, eq, values[0]));
        yp_decrefN(N(pair.hashable, pair.unhashable, mp_pair));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ead(value, yp_getitem(mp, keys[0]), assert_obj(value, is, values[0]));
        ead(value, yp_getitem(mp, keys[1]), assert_obj(value, is, values[1]));
    }

    // Exception passthrough.
    assert_isexception(yp_getitem(mp, yp_SyntaxError), yp_SyntaxError);
    assert_isexception(yp_getitem(empty, yp_SyntaxError), yp_SyntaxError);

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

    // Empty mp.
    ead(value, yp_getdefault(empty, keys[0], values[2]), assert_obj(value, eq, values[2]));

    // key is mp.
    ead(value, yp_getdefault(mp, mp, values[2]), assert_obj(value, eq, values[2]));

    // default is mp.
    ead(value, yp_getdefault(mp, keys[0], mp), assert_obj(value, eq, values[0]));
    ead(value, yp_getdefault(mp, keys[2], mp), assert_obj(value, eq, mp));

    // key is unhashable.
    ead(value, yp_getdefault(mp, unhashable, values[2]), assert_obj(value, eq, values[2]));

    // An unhashable key should match the equal key in mp.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *mp_pair = type->newK(K(pair.hashable, values[0]));
        ead(value, yp_getdefault(mp_pair, pair.unhashable, values[2]),
                assert_obj(value, eq, values[0]));
        yp_decrefN(N(pair.hashable, pair.unhashable, mp_pair));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ead(value, yp_getdefault(mp, keys[0], values[2]), assert_obj(value, eq, values[0]));
        ead(value, yp_getdefault(mp, keys[1], values[2]), assert_obj(value, eq, values[1]));
    }

    // Exception passthrough.
    assert_isexception(yp_getdefault(mp, yp_SyntaxError, values[2]), yp_SyntaxError);
    assert_isexception(yp_getdefault(empty, yp_SyntaxError, values[2]), yp_SyntaxError);
    assert_isexception(yp_getdefault(mp, keys[0], yp_SyntaxError), yp_SyntaxError);
    assert_isexception(yp_getdefault(mp, keys[2], yp_SyntaxError), yp_SyntaxError);
    assert_isexception(yp_getdefault(empty, keys[0], yp_SyntaxError), yp_SyntaxError);

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
        assert_raises_exc(yp_setitem(mp, keys[2], values[2], &exc), yp_TypeError);
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

    // value is mp.
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
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *mp = type->newK(K(pair.hashable, values[0]));
        assert_raises_exc(yp_setitem(mp, pair.unhashable, values[2], &exc), yp_TypeError);
        assert_mapping(mp, pair.hashable, values[0]);
        yp_decrefN(N(pair.hashable, pair.unhashable, mp));
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

static void _test_delitem(
        fixture_type_t *type, void (*any_delitem)(ypObject *, ypObject *, ypObject **), int raises)
{
    ypObject *keys[3];
    ypObject *values[3];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Immutables don't support delitem.
    if (!type->is_mutable) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_raises_exc(any_delitem(mp, keys[2], &exc), yp_TypeError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
        goto tear_down;  // Skip remaining tests.
    }

#define assert_not_found_exc(expression)          \
    do {                                          \
        ypObject *exc = yp_None;                  \
        (expression);                             \
        if (raises) {                             \
            assert_isexception(exc, yp_KeyError); \
        } else {                                  \
            assert_obj(exc, is, yp_None);         \
        }                                         \
    } while (0)

    // Basic key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(any_delitem(mp, keys[0], &exc));
        assert_mapping(mp, keys[1], values[1]);
        yp_decref(mp);
    }

    // Unknown key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_found_exc(any_delitem(mp, keys[2], &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // Previously-deleted key.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_raises_exc(yp_delitem(mp, keys[1], &exc));
        assert_mapping(mp, keys[0], values[0]);
        assert_not_found_exc(any_delitem(mp, keys[1], &exc));
        assert_mapping(mp, keys[0], values[0]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        assert_not_found_exc(any_delitem(mp, keys[2], &exc));
        assert_len(mp, 0);
        yp_decref(mp);
    }

    // key is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_not_found_exc(any_delitem(mp, mp, &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // key is unhashable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_not_found_exc(any_delitem(mp, unhashable, &exc));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, unhashable));
    }

    // An unhashable key should match the equal key in mp.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *mp = type->newK(K(pair.hashable, values[0]));
        assert_not_raises_exc(any_delitem(mp, pair.unhashable, &exc));
        assert_len(mp, 0);
        yp_decrefN(N(pair.hashable, pair.unhashable, mp));
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_isexception_exc(any_delitem(mp, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

#undef assert_not_found_exc

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
}

static MunitResult test_delitem(const MunitParameter params[], fixture_t *fixture)
{
    _test_delitem(fixture->type, yp_delitem, /*raises=*/TRUE);
    return MUNIT_OK;
}

static MunitResult test_dropitem(const MunitParameter params[], fixture_t *fixture)
{
    _test_delitem(fixture->type, yp_dropitem, /*raises=*/FALSE);
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
        assert_raises(yp_popvalue2(mp, keys[2]), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // Empty mp.
    {
        ypObject *mp = type->newK(0);
        ead(value, yp_popvalue3(mp, keys[3], values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue2(mp, keys[3]), yp_KeyError);
        assert_len(mp, 0);
        yp_decref(mp);
    }

    // key is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ead(value, yp_popvalue3(mp, mp, values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue2(mp, mp), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decref(mp);
    }

    // default is mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ead(value, yp_popvalue3(mp, keys[2], mp), assert_obj(value, eq, values[2]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        ead(value, yp_popvalue3(mp, keys[2], mp), assert_obj(value, eq, mp));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // key is unhashable.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        ead(value, yp_popvalue3(mp, unhashable, values[3]), assert_obj(value, eq, values[3]));
        assert_raises(yp_popvalue2(mp, unhashable), yp_KeyError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        yp_decrefN(N(mp, unhashable));
    }

    // An unhashable key should match the equal key in mp.
    {
        hashability_pair_t pair0 = rand_obj_any_hashability_pair();
        hashability_pair_t pair1 = rand_obj_any_hashability_pair();
        ypObject          *mp = type->newK(K(pair0.hashable, values[0], pair1.hashable, values[1]));
        ead(value, yp_popvalue3(mp, pair0.unhashable, values[3]), assert_obj(value, eq, values[0]));
        assert_mapping(mp, pair1.hashable, values[1]);
        ead(value, yp_popvalue2(mp, pair1.unhashable), assert_obj(value, eq, values[1]));
        assert_len(mp, 0);
        yp_decrefN(N(pair0.hashable, pair0.unhashable, pair1.hashable, pair1.unhashable, mp));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ead(value, yp_popvalue3(mp, keys[2], values[3]), assert_obj(value, is, values[2]));
        ead(value, yp_popvalue3(mp, keys[3], values[3]), assert_obj(value, is, values[3]));
        ead(value, yp_popvalue2(mp, keys[1]), assert_obj(value, is, values[1]));
        yp_decref(mp);
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1], keys[2], values[2]));
        ypObject *empty = type->newK(0);
        assert_isexception(yp_popvalue3(mp, yp_SyntaxError, values[3]), yp_SyntaxError);
        assert_isexception(yp_popvalue2(mp, yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_popvalue3(empty, yp_SyntaxError, values[3]), yp_SyntaxError);
        assert_isexception(yp_popvalue2(empty, yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_popvalue3(mp, keys[0], yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_popvalue3(mp, keys[3], yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_popvalue3(empty, keys[0], yp_SyntaxError), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1], keys[2], values[2]);
        assert_len(empty, 0);
        yp_decrefN(N(mp, empty));
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

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *key, *value;
        ypObject *mp = type->newK(K(keys[0], values[0]));
        yp_popitem(mp, &key, &value);
        assert_obj(key, is, keys[0]);
        assert_obj(value, is, values[0]);
        assert_len(mp, 0);
        yp_decrefN(N(mp, key, value));
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

    // value is mp; key in mp.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ead(value, yp_setdefault(mp, keys[0], mp), assert_obj(value, eq, values[0]));
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

    // value is mp; key not in mp.
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
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *mp = type->newK(K(pair.hashable, values[0]));
        assert_raises(yp_setdefault(mp, pair.unhashable, values[2]), yp_TypeError);
        assert_mapping(mp, pair.hashable, values[0]);
        yp_decrefN(N(pair.hashable, pair.unhashable, mp));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ead(value, yp_setdefault(mp, keys[0], values[2]), assert_obj(value, is, values[0]));
        ead(value, yp_setdefault(mp, keys[2], values[2]), assert_obj(value, is, values[2]));
        yp_decref(mp);
    }

    // Exception passthrough.
    {
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        assert_isexception(yp_setdefault(mp, yp_SyntaxError, values[2]), yp_SyntaxError);
        assert_isexception(yp_setdefault(mp, keys[0], yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_setdefault(mp, keys[2], yp_SyntaxError), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
}

static void _test_updateK(fixture_type_t *type,
        void (*any_updateK)(ypObject *, ypObject **, int, ...), int test_unhashables)
{
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *keys[6];
    ypObject          *values[6];
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
                any_updateK(mp, &exc, K(pair.unhashable, values[1], pair.hashable, values[2])),
                yp_TypeError);
        assert_len(mp, 0);
        assert_raises_exc(
                any_updateK(mp, &exc, K(pair.hashable, values[1], pair.unhashable, values[2])),
                yp_TypeError);
        // Optimization: updateK adds while it iterates.
        assert_mapping(mp, pair.hashable, values[1]);
        yp_decrefN(N(mp));
    }

    // Unhashable keys should always cause TypeError, even if that key is already in mp.
    if (test_unhashables) {
        ypObject *mp = type->newK(K(pair.hashable, values[0]));
        assert_raises_exc(any_updateK(mp, &exc, K(pair.unhashable, values[2])), yp_TypeError);
        assert_mapping(mp, pair.hashable, values[0]);
        yp_decrefN(N(mp));
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(pair.hashable, pair.unhashable));
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
    ypObject       *keys[6];
    ypObject       *values[6];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Shared tests.
    _test_updateK(type, yp_updateK, /*test_unhashables=*/TRUE);
    _test_updateK(type, updateK_to_updateKV, /*test_unhashables=*/TRUE);

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
        assert_isexception_exc(yp_updateK(mp, &exc, K(keys[0], yp_SyntaxError)), yp_SyntaxError);
        assert_isexception_exc(yp_updateK(mp, &exc, K(keys[2], yp_SyntaxError)), yp_SyntaxError);
        assert_isexception_exc(
                updateK_to_updateKV(mp, &exc, K(yp_SyntaxError, values[2])), yp_SyntaxError);
        assert_isexception_exc(
                updateK_to_updateKV(mp, &exc, K(keys[0], yp_SyntaxError)), yp_SyntaxError);
        assert_isexception_exc(
                updateK_to_updateKV(mp, &exc, K(keys[2], yp_SyntaxError)), yp_SyntaxError);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decref(mp);
    }

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    return MUNIT_OK;
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

static void updateK_to_update_fromtuple(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    x = new_itemsKV(fixture_type_tuple, n, args);
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
}

static void updateK_to_update_fromlist(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    x = new_itemsKV(fixture_type_list, n, args);
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
}

static void updateK_to_update_fromfrozendict_dirty(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    x = new_itemsKV(fixture_type_frozendict_dirty, n, args);
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
}

static void updateK_to_update_fromdict_dirty(ypObject *mapping, ypObject **exc, int n, ...)
{
    va_list   args;
    ypObject *x;
    va_start(args, n);
    x = new_itemsKV(fixture_type_dict_dirty, n, args);
    va_end(args);
    yp_update(mapping, x, exc);
    yp_decref(x);
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

static MunitResult test_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *not_iterable = rand_obj_any_not_iterable();
    ypObject        *keys[6];
    ypObject        *values[6];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        void (*updateK)(ypObject *, ypObject **, int, ...);
        if ((*x_type) == fixture_type_iter) {
            updateK = updateK_to_update_fromiter;
        } else if ((*x_type) == fixture_type_tuple) {
            updateK = updateK_to_update_fromtuple;
        } else if ((*x_type) == fixture_type_list) {
            updateK = updateK_to_update_fromlist;
        } else if ((*x_type) == fixture_type_frozendict_dirty) {
            updateK = updateK_to_update_fromfrozendict_dirty;
        } else if ((*x_type) == fixture_type_dict_dirty) {
            updateK = updateK_to_update_fromdict_dirty;
        } else if ((*x_type) == fixture_type_frozendict) {
            updateK = updateK_to_update_fromfrozendict;
        } else {
            assert_ptr((*x_type), ==, fixture_type_dict);
            updateK = updateK_to_update_fromdict;
        }

        // Shared tests.
        _test_updateK(type, updateK, /*test_unhashables=*/FALSE);
    }

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
        assert_raises_exc(yp_update(mp, not_iterable, &exc), yp_TypeError);
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
    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static MunitParameterEnum test_mapping_params[] = {
        {param_key_type, param_values_types_mapping}, {NULL}};

MunitTest test_mapping_tests[] = {TEST(test_contains, test_mapping_params),
        TEST(test_eq, test_mapping_params), TEST(test_ne, test_mapping_params),
        TEST(test_getitem, test_mapping_params), TEST(test_getdefault, test_mapping_params),
        TEST(test_setitem, test_mapping_params), TEST(test_delitem, test_mapping_params),
        TEST(test_dropitem, test_mapping_params), TEST(test_popvalue, test_mapping_params),
        TEST(test_popitem, test_mapping_params), TEST(test_setdefault, test_mapping_params),
        TEST(test_updateK, test_mapping_params), TEST(test_update, test_mapping_params), {NULL}};

extern void test_mapping_initialize(void) {}

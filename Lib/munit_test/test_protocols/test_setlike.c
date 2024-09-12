
#include "munit_test/unittest.h"

// XXX Because Python calls both the protocol and the object "set", I'm using the term "set-like" to
// refer to the protocol where there may be confusion.
// TODO Do this in nohtyP.h?

// Sets should accept themselves, their pairs, iterators, frozenset/set, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// TODO Add dict key and item views (when implemented).
// TODO "Shared key" versions, somehow? fixture_type_frozendict_shared, fixture_type_dict_shared
#define x_types_init(type)                                                           \
    {(type), (type)->pair, fixture_type_iter, fixture_type_tuple, fixture_type_list, \
            fixture_type_frozenset, fixture_type_set, fixture_type_frozenset_dirty,  \
            fixture_type_set_dirty, fixture_type_frozendict, fixture_type_dict,      \
            fixture_type_frozendict_dirty, fixture_type_dict_dirty, NULL}

#define friend_types_init(type) {(type), (type)->pair, NULL}

// Returns true iff type supports comparison operators (lt/etc) with other. This does not apply to
// the comparison methods (isdisjoint/etc), as those support any iterable.
static int type_is_comparable(fixture_type_t *type, fixture_type_t *other)
{
    return type->type == other->type || type->type == other->pair->type;
}


// The test_contains in test_collection checks for the behaviour shared amongst all collections;
// this test_contains considers the behaviour unique to set-likes.
static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_obj(yp_contains(so, unhashable), is, yp_False);
        assert_obj(yp_in(unhashable, so), is, yp_False);
        assert_obj(yp_not_in(unhashable, so), is, yp_True);
        yp_decrefN(N(so, unhashable));
    }

    // An unhashable x should match the equal item in so.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *so = type->newN(N(pair.hashable));
        assert_obj(yp_contains(so, pair.unhashable), is, yp_True);
        assert_obj(yp_in(pair.unhashable, so), is, yp_True);
        assert_obj(yp_not_in(pair.unhashable, so), is, yp_False);
        yp_decrefN(N(pair.hashable, pair.unhashable, so));
    }

    obj_array_decref(items);
    return MUNIT_OK;
}

// expected is either the exception which is expected to be raised, or the boolean expected to be
// returned.
static void _test_comparisons_not_supported(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject                   *expected)
{
    ypObject *items[2];
    ypObject *so;
    ypObject *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

#define assert_not_supported(expression)      \
    do {                                      \
        ypObject *result = (expression);      \
        if (yp_isexceptionC(expected)) {      \
            assert_raises(result, expected);  \
        } else {                              \
            assert_obj(result, is, expected); \
        }                                     \
    } while (0)

    ead(x, rand_obj(x_type), assert_not_supported(any_cmp(so, x)));
    ead(x, rand_obj(x_type), assert_not_supported(any_cmp(empty, x)));

    if (x_type->is_collection) {
        ead(x, x_type->newN(N(items[0], items[1])), assert_not_supported(any_cmp(so, x)));
        ead(x, x_type->newN(0), assert_not_supported(any_cmp(so, x)));
        ead(x, x_type->newN(N(items[0], items[1])), assert_not_supported(any_cmp(empty, x)));
        ead(x, x_type->newN(0), assert_not_supported(any_cmp(empty, x)));
    }

#undef assert_not_supported

    obj_array_decref(items);
    yp_decrefN(N(so, empty));
}

static void _test_comparisons(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject *x_same, ypObject *x_empty,
        ypObject *x_subset, ypObject *x_superset, ypObject *x_overlap, ypObject *x_no_overlap,
        ypObject *so_empty, ypObject *both_empty)
{
    ypObject *items[4];
    obj_array_fill(items, type->rand_items);

    // Non-empty so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));

        // x has the same items.
        ead(x, x_type->newN(N(items[0], items[1])), assert_obj(any_cmp(so, x), is, x_same));
        ead(x, x_type->newN(N(items[0], items[0], items[1])),
                assert_obj(any_cmp(so, x), is, x_same));

        // x is empty.
        ead(x, x_type->newN(0), assert_obj(any_cmp(so, x), is, x_empty));

        // x is is a proper subset and is not empty.
        ead(x, x_type->newN(N(items[0])), assert_obj(any_cmp(so, x), is, x_subset));
        ead(x, x_type->newN(N(items[1])), assert_obj(any_cmp(so, x), is, x_subset));
        ead(x, x_type->newN(N(items[0], items[0])), assert_obj(any_cmp(so, x), is, x_subset));

        // x is a proper superset.
        ead(x, x_type->newN(N(items[0], items[1], items[2])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(any_cmp(so, x), is, x_superset));

        // x overlaps and contains additional items.
        ead(x, x_type->newN(N(items[0], items[2])), assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(items[0], items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(items[1], items[2])), assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(items[1], items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(items[0], items[0], items[2])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(items[0], items[2], items[2])),
                assert_obj(any_cmp(so, x), is, x_overlap));

        // x does not overlap and contains additional items.
        ead(x, x_type->newN(N(items[2])), assert_obj(any_cmp(so, x), is, x_no_overlap));
        ead(x, x_type->newN(N(items[2], items[3])), assert_obj(any_cmp(so, x), is, x_no_overlap));
        ead(x, x_type->newN(N(items[2], items[2])), assert_obj(any_cmp(so, x), is, x_no_overlap));

        // x is so.
        assert_obj(any_cmp(so, so), is, x_same);

        // x contains so.
        if (!x_type->hashable_items_only || !type->is_mutable) {
            ead(x, x_type->newN(N(so, items[0], items[1])),
                    assert_obj(any_cmp(so, x), is, x_superset));
        }

        // Exception passthrough.
        assert_isexception(any_cmp(so, yp_SyntaxError), yp_SyntaxError);

        assert_setlike(so, items[0], items[1]);  // so unchanged.
        yp_decref(so);
    }

    // Empty so.
    {
        ypObject *empty = type->newN(0);

        // Non-empty x.
        ead(x, x_type->newN(N(items[0])), assert_obj(any_cmp(empty, x), is, so_empty));
        ead(x, x_type->newN(N(items[0], items[1])), assert_obj(any_cmp(empty, x), is, so_empty));
        ead(x, x_type->newN(N(items[0], items[0])), assert_obj(any_cmp(empty, x), is, so_empty));

        // Empty x.
        ead(x, x_type->newN(0), assert_obj(any_cmp(empty, x), is, both_empty));

        // x is so.
        assert_obj(any_cmp(empty, empty), is, both_empty);

        // x contains so.
        if (!x_type->hashable_items_only || !type->is_mutable) {
            ead(x, x_type->newN(N(empty, items[0], items[1])),
                    assert_obj(any_cmp(empty, x), is, so_empty));
        }

        // Exception passthrough.
        assert_isexception(any_cmp(empty, yp_SyntaxError), yp_SyntaxError);

        assert_len(empty, 0);  // empty unchanged.
        yp_decref(empty);
    }

    // x contains unhashable objects.
    if (!x_type->hashable_items_only) {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *unhashable = rand_obj_any_mutable_unique(1, &pair.hashable);
        ypObject          *so = type->newN(N(items[0], pair.hashable));
        ypObject          *empty = type->newN(0);

        // x has the same items.
        ead(x, x_type->newN(N(items[0], pair.unhashable)), assert_obj(any_cmp(so, x), is, x_same));

        // x is is a proper subset and is not empty.
        ead(x, x_type->newN(N(pair.unhashable)), assert_obj(any_cmp(so, x), is, x_subset));

        // x is a proper superset.
        ead(x, x_type->newN(N(items[0], pair.unhashable, items[1])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], pair.hashable, unhashable)),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], pair.unhashable, unhashable)),
                assert_obj(any_cmp(so, x), is, x_superset));

        // x overlaps and contains additional items.
        ead(x, x_type->newN(N(pair.unhashable, items[1])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(pair.hashable, unhashable)),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(pair.unhashable, unhashable)),
                assert_obj(any_cmp(so, x), is, x_overlap));

        // x does not overlap and contains additional items.
        ead(x, x_type->newN(N(unhashable)), assert_obj(any_cmp(so, x), is, x_no_overlap));

        // so is empty.
        ead(x, x_type->newN(N(unhashable)), assert_obj(any_cmp(empty, x), is, so_empty));

        yp_decrefN(N(pair.hashable, pair.unhashable, unhashable, so, empty));
    }

    // Implementations may use the cached hash as a quick inequality test. Recall that only
    // immutables can cache their hash, which occurs when yp_hashC is called. Because the cached
    // hash is an internal optimization, it should only be used with friendly types.
    if (!type->is_mutable && !x_type->is_mutable && type_is_comparable(type, x_type)) {
        yp_ssize_t i, j;
        ypObject  *so = type->newN(N(items[0], items[1]));
        ypObject  *empty = type->newN(0);

        // Run the tests twice: once where so has not cached the hash, and once where it has.
        for (i = 0; i < 2; i++) {
            ypObject *x_is_same = x_type->newN(N(items[0], items[1]));
            ypObject *x_is_empty = x_type->newN(0);
            ypObject *x_is_subset = x_type->newN(N(items[0]));
            ypObject *x_is_superset = x_type->newN(N(items[0], items[1], items[2]));
            ypObject *x_is_overlapped = x_type->newN(N(items[0], items[2]));
            ypObject *x_is_not_overlapped = x_type->newN(N(items[2]));

            // Run the tests twice: once where x has not cached the hash, and once where it has.
            for (j = 0; j < 2; j++) {
                assert_obj(any_cmp(so, x_is_same), is, x_same);
                assert_obj(any_cmp(so, x_is_empty), is, x_empty);
                assert_obj(any_cmp(so, x_is_subset), is, x_subset);
                assert_obj(any_cmp(so, x_is_superset), is, x_superset);
                assert_obj(any_cmp(so, x_is_overlapped), is, x_overlap);
                assert_obj(any_cmp(so, x_is_not_overlapped), is, x_no_overlap);

                assert_obj(any_cmp(empty, x_is_same), is, so_empty);
                assert_obj(any_cmp(empty, x_is_empty), is, both_empty);

                // Trigger the hash to be cached on "x" and try again.
                assert_not_raises_exc(yp_hashC(x_is_same, &exc));
                assert_not_raises_exc(yp_hashC(x_is_empty, &exc));
                assert_not_raises_exc(yp_hashC(x_is_subset, &exc));
                assert_not_raises_exc(yp_hashC(x_is_superset, &exc));
                assert_not_raises_exc(yp_hashC(x_is_overlapped, &exc));
                assert_not_raises_exc(yp_hashC(x_is_not_overlapped, &exc));
            }

            assert_obj(any_cmp(so, so), is, x_same);
            assert_obj(any_cmp(empty, empty), is, both_empty);

            // Trigger the hash to be cached on "so" and try again.
            assert_not_raises_exc(yp_hashC(so, &exc));
            assert_not_raises_exc(yp_hashC(empty, &exc));

            yp_decrefN(N(x_is_same, x_is_empty, x_is_subset, x_is_superset, x_is_overlapped,
                    x_is_not_overlapped));
        }

        yp_decrefN(N(so, empty));
    }

    obj_array_decref(items);
}

static MunitResult test_isdisjoint(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        _test_comparisons(type, (*x_type), yp_isdisjoint, /*x_same=*/yp_False,
                /*x_empty=*/yp_True, /*x_subset=*/yp_False, /*x_superset=*/yp_False,
                /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_True, /*so_empty=*/yp_True,
                /*both_empty=*/yp_True);
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[2], items[3])), result = yp_isdisjoint(so, x),
                      assert_obj(result, is, yp_True), yp_decref(so));

    // Optimization: early exit if so is empty, even if the iterator will fail.
    {
        ypObject *so = type->newN(0);
        ypObject *x = new_faulty_iter(yp_frozenset_empty, 0, yp_SyntaxError, 2);
        assert_obj(yp_isdisjoint(so, x), is, yp_True);
        yp_decrefN(N(so, x));
    }

    // Optimization: early exit if x yields an item in so, even if the iterator will fail.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_obj(yp_isdisjoint(so, x), is, yp_False);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_isdisjoint, yp_TypeError);
    }

    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_issubset(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        _test_comparisons(type, (*x_type), yp_issubset, /*x_same=*/yp_True,
                /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_True,
                /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_True,
                /*both_empty=*/yp_True);
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[2], items[3])), result = yp_issubset(so, x),
                      assert_obj(result, is, yp_False), yp_decref(so));

    // Optimization: early exit if so is empty, even if the iterator will fail.
    {
        ypObject *so = type->newN(0);
        ypObject *x = new_faulty_iter(yp_frozenset_empty, 0, yp_SyntaxError, 2);
        assert_obj(yp_issubset(so, x), is, yp_True);
        yp_decrefN(N(so, x));
    }

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_issubset, yp_TypeError);
    }

    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_issuperset(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        _test_comparisons(type, (*x_type), yp_issuperset, /*x_same=*/yp_True,
                /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_False,
                /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                /*both_empty=*/yp_True);
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[0], items[1])), result = yp_issuperset(so, x),
                      assert_obj(result, is, yp_True), yp_decref(so));

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_issuperset, yp_TypeError);
    }

    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_lt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;

    // lt is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_is_comparable(type, (*x_type))) {
            _test_comparisons(type, (*x_type), yp_lt, /*x_same=*/yp_False,
                    /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_True,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_True,
                    /*both_empty=*/yp_False);
        } else {
            _test_comparisons_not_supported(type, *x_type, yp_lt, yp_TypeError);
        }
    }

    // _test_comparisons_faulty_iter not called as lt doesn't support iterators.

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_lt, yp_TypeError);
    }

    return MUNIT_OK;
}

static MunitResult test_le(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;

    // le is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_is_comparable(type, (*x_type))) {
            _test_comparisons(type, (*x_type), yp_le, /*x_same=*/yp_True,
                    /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_True,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_True,
                    /*both_empty=*/yp_True);
        } else {
            _test_comparisons_not_supported(type, *x_type, yp_le, yp_TypeError);
        }
    }

    // _test_comparisons_faulty_iter not called as le doesn't support iterators.

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_le, yp_TypeError);
    }

    return MUNIT_OK;
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
                    /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_False,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                    /*both_empty=*/yp_True);
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
                    /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_True,
                    /*x_overlap=*/yp_True, /*x_no_overlap=*/yp_True, /*so_empty=*/yp_True,
                    /*both_empty=*/yp_False);
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

static MunitResult test_ge(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;

    // ge is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_is_comparable(type, (*x_type))) {
            _test_comparisons(type, (*x_type), yp_ge, /*x_same=*/yp_True,
                    /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_False,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                    /*both_empty=*/yp_True);
        } else {
            _test_comparisons_not_supported(type, *x_type, yp_ge, yp_TypeError);
        }
    }

    // _test_comparisons_faulty_iter not called as ge doesn't support iterators.

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_ge, yp_TypeError);
    }

    return MUNIT_OK;
}

static MunitResult test_gt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;

    // gt is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_is_comparable(type, (*x_type))) {
            _test_comparisons(type, (*x_type), yp_gt, /*x_same=*/yp_False,
                    /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_False,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                    /*both_empty=*/yp_False);
        } else {
            _test_comparisons_not_supported(type, *x_type, yp_gt, yp_TypeError);
        }
    }

    // _test_comparisons_faulty_iter not called as gt doesn't support iterators.

    // x is not an iterable.
    for (x_type = fixture_types_not_iterable->types; (*x_type) != NULL; x_type++) {
        _test_comparisons_not_supported(type, *x_type, yp_gt, yp_TypeError);
    }

    return MUNIT_OK;
}

static MunitResult test_union(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t    *friend_types[] = friend_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic union: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1], items[2]);
        assert_setlike(so, items[0], items[1]);  // so unchanged.
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_union(so, so);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only || !type->is_mutable) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], so));
            if (type->is_mutable) {
                assert_raises(yp_union(so, x), yp_TypeError);
            } else {
                ead(result, yp_union(so, x), assert_setlike(result, items[0], items[1], so));
            }
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_union(so, x);
        assert_setlike(result, items[0], items[1], items[2]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises(yp_union(so, x), yp_TypeError);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable items rejected even if equal to other hashable items.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(0);
            ead(x, (*x_type)->newN(N(pair.hashable, pair.unhashable)),
                    assert_raises(yp_union(so, x), yp_TypeError));
            ead(x, (*x_type)->newN(N(pair.unhashable, pair.hashable)),
                    assert_raises(yp_union(so, x), yp_TypeError));
            assert_len(so, 0);
            yp_decrefN(N(so));
        }
    }

    // Unhashable items in x always cause a failure, even if that item is not part of the result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            assert_raises(yp_union(so, x), yp_TypeError);
            yp_decrefN(N(so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_union(so, x),
                      assert_setlike(result, items[0], items[1], items[2]),
                      yp_decrefN(N(so, result)));

    // Optimization: lazy shallow copy of a friendly immutable x when immutable so is empty.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_union(so, x);
        if (!type->is_mutable && !(*x_type)->is_mutable) {
            assert_obj(result, is, x);
        } else {
            assert_obj(result, is_not, x);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: lazy shallow copy of an immutable so when friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_union(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, so);
        } else {
            assert_obj(result, is_not, so);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when immutable so is empty and friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_union(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: lazy shallow copy of an immutable so when x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_union(so, so);
        if (!type->is_mutable) {
            assert_obj(result, is, so);
        } else {
            assert_obj(result, is_not, so);
        }
        yp_decrefN(N(so, result));
    }

    // TODO There could be similar optimizations for all four of these methods where the result
    // is equal to so or the result is empty.

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_union(so, not_iterable), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_union(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    return MUNIT_OK;
}

static MunitResult test_intersection(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t    *friend_types[] = friend_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic intersection: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_intersection(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[1]);
        assert_setlike(so, items[0], items[1]);  // so unchanged.
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_intersection(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_intersection(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_intersection(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_intersection(so, so);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only || !type->is_mutable) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], so));
            ead(result, yp_intersection(so, x), assert_setlike(result, items[1]));
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_intersection(so, x);
        assert_setlike(result, items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            ypObject *result = yp_intersection(so, x);
            assert_setlike(result, items[1]);
            yp_decrefN(N(unhashable, so, x, result));
        }
    }

    // An unhashable item in x should match the equal item in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            ypObject *result = yp_intersection(so, x);
            assert_setlike(result, pair.hashable);
            yp_decrefN(N(so, x, result));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_intersection(so, x),
                      assert_setlike(result, items[1]), yp_decrefN(N(so, result)));

    // Optimization: empty immortal when immutable so is empty.
    // TODO This could apply for any iterable x.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_intersection(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when so is immutable and friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_intersection(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when immutable so is empty and friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_intersection(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: lazy shallow copy of an immutable so when x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_intersection(so, so);
        if (!type->is_mutable) {
            assert_obj(result, is, so);
        } else {
            assert_obj(result, is_not, so);
        }
        yp_decrefN(N(so, result));
    }

    // TODO There could be similar optimizations for all four of these methods where the result
    // is equal to so or the result is empty.

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_intersection(so, not_iterable), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_intersection(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.hashable, pair.unhashable));
    return MUNIT_OK;
}

static MunitResult test_difference(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t    *friend_types[] = friend_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic difference: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_difference(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0]);
        assert_setlike(so, items[0], items[1]);  // so unchanged.
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_difference(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_difference(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_difference(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_difference(so, so);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, result));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only || !type->is_mutable) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], so));
            ead(result, yp_difference(so, x), assert_setlike(result, items[0]));
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_difference(so, x);
        assert_setlike(result, items[0]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            ypObject *result = yp_difference(so, x);
            assert_setlike(result, items[0]);
            yp_decrefN(N(unhashable, so, x, result));
        }
    }

    // An unhashable item in x should match the equal item in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            ypObject *result = yp_difference(so, x);
            assert_len(result, 0);
            yp_decrefN(N(so, x, result));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_difference(so, x),
                      assert_setlike(result, items[0]), yp_decrefN(N(so, result)));

    // Optimization: empty immortal when immutable so is empty.
    // TODO This could apply for any iterable x.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_difference(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: lazy shallow copy of an immutable so when friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_difference(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, so);
        } else {
            assert_obj(result, is_not, so);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when immutable so is empty and friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_difference(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when so is immutable and x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_difference(so, so);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, result));
    }

    // TODO There could be similar optimizations for all four of these methods where the result
    // is equal to so or the result is empty.

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_difference(so, not_iterable), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_difference(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.hashable, pair.unhashable));
    return MUNIT_OK;
}

static MunitResult test_symmetric_difference(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t    *friend_types[] = friend_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic symmetric_difference: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[2]);
        assert_setlike(so, items[0], items[1]);  // so unchanged.
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_setlike(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_symmetric_difference(so, so);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, result));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only || !type->is_mutable) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], so));
            if (type->is_mutable) {
                assert_raises(yp_symmetric_difference(so, x), yp_TypeError);
            } else {
                ead(result, yp_symmetric_difference(so, x), assert_setlike(result, items[0], so));
            }
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates. (This is a particularly interesting test for symmetric_difference.)
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_symmetric_difference(so, x);
        assert_setlike(result, items[0], items[2]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises(yp_symmetric_difference(so, x), yp_TypeError);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable items rejected even if equal to other hashable items.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(0);
            ead(x, (*x_type)->newN(N(pair.hashable, pair.unhashable)),
                    assert_raises(yp_symmetric_difference(so, x), yp_TypeError));
            ead(x, (*x_type)->newN(N(pair.unhashable, pair.hashable)),
                    assert_raises(yp_symmetric_difference(so, x), yp_TypeError));
            yp_decrefN(N(so));
        }
    }

    // Unhashable items in x always cause a failure, even if that item is not part of the result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            assert_raises(yp_symmetric_difference(so, x), yp_TypeError);
            yp_decrefN(N(so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_symmetric_difference(so, x),
                      assert_setlike(result, items[0], items[2]), yp_decrefN(N(so, result)));

    // Optimization: lazy shallow copy of a friendly immutable x when immutable so is empty.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_symmetric_difference(so, x);
        if (!type->is_mutable && !(*x_type)->is_mutable) {
            assert_obj(result, is, x);
        } else {
            assert_obj(result, is_not, x);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: lazy shallow copy of an immutable so when friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_symmetric_difference(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, so);
        } else {
            assert_obj(result, is_not, so);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when immutable so is empty and friendly x is empty.
    // TODO This could apply for any iterable x that doesn't yield a value.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_symmetric_difference(so, x);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, x, result));
    }

    // Optimization: empty immortal when so is immutable and x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_symmetric_difference(so, so);
        if (!type->is_mutable) {
            assert_obj(result, is, yp_frozenset_empty);
        } else {
            assert_obj(result, is_not, yp_frozenset_empty);
        }
        yp_decrefN(N(so, result));
    }

    // TODO There could be similar optimizations for all four of these methods where the result
    // is equal to so or the result is empty.

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_symmetric_difference(so, not_iterable), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_symmetric_difference(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    return MUNIT_OK;
}

static MunitResult test_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_update(so, x, &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_setlike(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x));
    }

    // FIXME previously-delete (and elsewhere)

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_update(so, so, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(so));
            assert_raises_exc(yp_update(so, x, &exc), yp_TypeError);
            assert_setlike(so, items[0], items[1]);
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_setlike(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises_exc(yp_update(so, x, &exc), yp_TypeError);
            assert_setlike(so, items[0], items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable items rejected even if equal to other hashable items.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(0);
            ead(x, (*x_type)->newN(N(pair.unhashable, pair.hashable)),
                    assert_raises_exc(yp_update(so, x, &exc), yp_TypeError));
            assert_len(so, 0);
            ead(x, (*x_type)->newN(N(pair.hashable, pair.unhashable)),
                    assert_raises_exc(yp_update(so, x, &exc), yp_TypeError));
            assert_setlike(so, pair.hashable);  // Optimization: update adds while it iterates.
            yp_decrefN(N(so));
        }
    }

    // Unhashable items in x always cause a failure, even if that item is not part of the result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            assert_raises_exc(yp_update(so, x, &exc), yp_TypeError);
            assert_setlike(so, pair.hashable);
            yp_decrefN(N(so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_update(so, x, &exc),
            assert_setlike(so, items[0], items[1], items[2]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // Optimization: we add directly to so from the iterator. Unfortunately, if the iterator
    // fails mid-way so may have already been modified.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_update(so, not_iterable, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    return MUNIT_OK;
}

static MunitResult test_intersection_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support intersection_update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic intersection_update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_setlike(so, items[1]);
        yp_decrefN(N(so, x));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_intersection_update(so, so, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], so));
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            assert_setlike(so, items[1]);
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_setlike(so, items[1]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            assert_setlike(so, items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // An unhashable item in x should match the equal item in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            assert_setlike(so, pair.hashable);
            yp_decrefN(N(so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_intersection_update(so, x, &exc),
            assert_setlike(so, items[1]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // so is not modified if the iterator fails mid-way: intersection_update needs to yield all
    // items before modifying so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_intersection_update(so, not_iterable, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_intersection_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.hashable, pair.unhashable));
    return MUNIT_OK;
}

static MunitResult test_difference_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support difference_update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic difference_update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_setlike(so, items[0]);
        yp_decrefN(N(so, x));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_difference_update(so, so, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], so));
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            assert_setlike(so, items[0]);
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_setlike(so, items[0]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            assert_setlike(so, items[0]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // An unhashable item in x should match the equal item in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            assert_len(so, 0);
            yp_decrefN(N(so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_difference_update(so, x, &exc),
            assert_setlike(so, items[0]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // Optimization: we discard from so directly from the iterator. Unfortunately, if the iterator
    // fails mid-way so may have already been modified.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_difference_update(so, not_iterable, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_difference_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.hashable, pair.unhashable));
    return MUNIT_OK;
}

static MunitResult test_symmetric_difference_update(
        const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t    *type = fixture->type;
    fixture_type_t    *x_types[] = x_types_init(type);
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support symmetric_difference_update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic symmetric_difference_update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_setlike(so, items[0], items[2]);
        yp_decrefN(N(so, x));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x can be so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, so, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // x contains so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(so));
            assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError);
            assert_setlike(so, items[0], items[1]);
            yp_decrefN(N(so, x));
        }
    }

    // x contains duplicates. (This is a particularly interesting test for symmetric_difference.)
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_setlike(so, items[0], items[2]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError);
            assert_setlike(so, items[0], items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable items rejected even if equal to other hashable items.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(0);
            ead(x, (*x_type)->newN(N(pair.hashable, pair.unhashable)),
                    assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError));
            ead(x, (*x_type)->newN(N(pair.unhashable, pair.hashable)),
                    assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError));
            assert_len(so, 0);
            yp_decrefN(N(so));
        }
    }

    // Unhashable items in x always cause a failure, even if that item is not part of the result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *so = type->newN(N(pair.hashable));
            ypObject *x = (*x_type)->newN(N(pair.unhashable));
            assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError);
            assert_setlike(so, pair.hashable);
            yp_decrefN(N(so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_symmetric_difference_update(so, x, &exc),
            assert_setlike(so, items[0], items[2]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // so is not modified if the iterator fails mid-way: symmetric_difference_update needs to yield
    // all items before modifying so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_symmetric_difference_update(so, not_iterable, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(
                yp_symmetric_difference_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    return MUNIT_OK;
}

static MunitResult test_push(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support push.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_push(so, items[2], &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic push.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_push(so, items[2], &exc));
        assert_setlike(so, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_push(so, items[3], &exc));
        assert_setlike(so, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(so));
    }

    // Item already in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_push(so, items[1], &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_not_raises_exc(yp_push(so, items[0], &exc));
        assert_setlike(so, items[0]);
        yp_decrefN(N(so));
    }

    // x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_push(so, so, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_raises_exc(yp_push(so, unhashable, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // Unhashable items should always cause TypeError, even if that item is already in so.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *so = type->newN(N(pair.hashable));
        assert_raises_exc(yp_push(so, pair.unhashable, &exc), yp_TypeError);
        assert_setlike(so, pair.hashable);
        yp_decrefN(N(pair.hashable, pair.unhashable, so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_push(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_pushunique(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support pushunique.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_pushunique(so, items[2], &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic pushunique.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_pushunique(so, items[2], &exc));
        assert_setlike(so, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_pushunique(so, items[3], &exc));
        assert_setlike(so, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(so));
    }

    // Item already in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_pushunique(so, items[1], &exc), yp_KeyError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_not_raises_exc(yp_pushunique(so, items[0], &exc));
        assert_setlike(so, items[0]);
        yp_decrefN(N(so));
    }

    // x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_pushunique(so, so, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_raises_exc(yp_pushunique(so, unhashable, &exc), yp_TypeError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // Unhashable items should always cause TypeError, even if that item is already in so.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *so = type->newN(N(pair.hashable));
        assert_raises_exc(yp_pushunique(so, pair.unhashable, &exc), yp_TypeError);
        assert_setlike(so, pair.hashable);
        yp_decrefN(N(pair.hashable, pair.unhashable, so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_pushunique(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static void _test_remove(
        fixture_type_t *type, void (*any_remove)(ypObject *, ypObject *, ypObject **), int raises)
{
    ypObject *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support remove.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_remove(so, items[1], &exc), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
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

    // Basic remove.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(any_remove(so, items[1], &exc));
        assert_setlike(so, items[0]);
        assert_not_raises_exc(any_remove(so, items[0], &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Item not in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_found_exc(any_remove(so, items[2], &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_not_found_exc(any_remove(so, items[0], &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // x is so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_found_exc(any_remove(so, so, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_not_found_exc(any_remove(so, unhashable, &exc));
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // An unhashable x should match the equal item in so.
    {
        hashability_pair_t pair = rand_obj_any_hashability_pair();
        ypObject          *so = type->newN(N(pair.hashable));
        assert_not_raises_exc(any_remove(so, pair.unhashable, &exc));
        assert_len(so, 0);
        yp_decrefN(N(pair.hashable, pair.unhashable, so));
    }

    // Remove an item from a set. The hash should equal a "clean" set. This was a bug from the
    // implementation of the yp_HashSet functions.
    {
        ypObject *expected = type->newN(N(items[0]));
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(any_remove(so, items[1], &exc));
        assert_hashC_exc(yp_currenthashC(so, &exc), ==, yp_currenthashC(expected, &exc));

        yp_decrefN(N(expected, so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(any_remove(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

#undef assert_not_found_exc

tear_down:
    obj_array_decref(items);
}

static MunitResult test_remove(const MunitParameter params[], fixture_t *fixture)
{
    _test_remove(fixture->type, yp_remove, /*raises=*/TRUE);
    return MUNIT_OK;
}

static MunitResult test_discard(const MunitParameter params[], fixture_t *fixture)
{
    _test_remove(fixture->type, yp_discard, /*raises=*/FALSE);
    return MUNIT_OK;
}

static MunitResult test_pop(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    yp_ssize_t      i;
    ypObject       *items[6];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support pop.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_pop(so), yp_MethodError);
        assert_setlike(so, items[0], items[1]);
        yp_decref(so);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic pop.
    {
        ypObject *so = type->newN(N(items[0]));
        ead(popped, yp_pop(so), assert_obj(popped, eq, items[0]));
        assert_len(so, 0);
        yp_decref(so);
    }

    // Self is empty.
    {
        ypObject *so = type->newN(0);
        assert_raises(yp_pop(so), yp_KeyError);
        assert_len(so, 0);
        yp_decref(so);
    }

    // Multiple pops. Order is arbitrary, so run through a few different items.
    for (i = 0; i < 5; i++) {
        ypObject *item_0 = items[0 + i];  // borrowed
        ypObject *item_1 = items[1 + i];  // borrowed
        ypObject *so = type->newN(N(item_0, item_1));
        ypObject *first;
        assert_not_raises(first = yp_pop(so));
        if (yp_eq(first, item_0) == yp_True) {
            assert_setlike(so, item_1);
            ead(popped, yp_pop(so), assert_obj(popped, eq, item_1));
        } else {
            assert_obj(first, eq, item_1);
            assert_setlike(so, item_0);
            ead(popped, yp_pop(so), assert_obj(popped, eq, item_0));
        }
        assert_len(so, 0);
        assert_raises(yp_pop(so), yp_KeyError);
        assert_len(so, 0);
        assert_raises(yp_pop(so), yp_KeyError);  // Calling again still raises KeyError.
        assert_len(so, 0);
        yp_decrefN(N(so, first));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *so = type->newN(N(items[0]));
        ead(popped, yp_pop(so), assert_obj(popped, is, items[0]));
        yp_decref(so);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}


// TODO dict key and item views, when implemented, will also support the set protocol.
// TODO range could be considered setlike, but is that useful? (Probably not.)
static MunitParameterEnum test_setlike_params[] = {
        {param_key_type, param_values_types_setlike}, {NULL}};

MunitTest test_setlike_tests[] = {TEST(test_contains, test_setlike_params),
        TEST(test_isdisjoint, test_setlike_params), TEST(test_issubset, test_setlike_params),
        TEST(test_issuperset, test_setlike_params), TEST(test_lt, test_setlike_params),
        TEST(test_le, test_setlike_params), TEST(test_eq, test_setlike_params),
        TEST(test_ne, test_setlike_params), TEST(test_ge, test_setlike_params),
        TEST(test_gt, test_setlike_params), TEST(test_union, test_setlike_params),
        TEST(test_intersection, test_setlike_params), TEST(test_difference, test_setlike_params),
        TEST(test_symmetric_difference, test_setlike_params),
        TEST(test_update, test_setlike_params), TEST(test_intersection_update, test_setlike_params),
        TEST(test_difference_update, test_setlike_params),
        TEST(test_symmetric_difference_update, test_setlike_params),
        TEST(test_push, test_setlike_params), TEST(test_pushunique, test_setlike_params),
        TEST(test_remove, test_setlike_params), TEST(test_discard, test_setlike_params),
        TEST(test_pop, test_setlike_params), {NULL}};

extern void test_setlike_initialize(void) {}

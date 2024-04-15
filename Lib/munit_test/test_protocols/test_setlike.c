
#include "munit_test/unittest.h"

// XXX Because Python calls both the protocol and the object "set", I'm using the term "set-like" to
// refer to the protocol where there may be confusion.
// TODO Do this in nohtyP.h?

// Sets should accept themselves, their pairs, iterators, frozenset/set, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// TODO Should dict really be here? What about dict key and item views (when implemented)?
#define x_types_init(type)                                                                     \
    {                                                                                          \
        (type), (type)->pair, fixture_type_iter, fixture_type_frozenset, fixture_type_set,     \
                fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict, \
                fixture_type_dict, NULL                                                        \
    }

#define friend_types_init()                                                     \
    {                                                                           \
        fixture_type_frozenset, fixture_type_set, fixture_type_frozenset_dirty, \
                fixture_type_set_dirty, NULL                                    \
    }

// Returns true iff type can store unhashable objects.
static int type_stores_unhashables(fixture_type_t *type)
{
    return !type->is_setlike && !type->is_mapping;
}

// Returns true iff type supports comparison operators (lt/etc) with other. This does not apply to
// the comparison methods (isdisjoint/etc), as those support any iterable.
static int type_iscomparable(fixture_type_t *type, fixture_type_t *other)
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

    // An unhashable x should match the equal object in so.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *so = type->newN(N(int_1));
        assert_obj(yp_contains(so, intstore_1), is, yp_True);
        assert_obj(yp_in(intstore_1, so), is, yp_True);
        assert_obj(yp_not_in(intstore_1, so), is, yp_False);
        yp_decrefN(N(int_1, intstore_1, so));
    }

    obj_array_decref(items);
    return MUNIT_OK;
}

// expected is either the exception which is expected to be raised, or the boolean expected to be
// returned.
static MunitResult _test_comparisons_not_supported(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject                          *expected)
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

#undef assert_not_supported

    obj_array_decref(items);
    yp_decrefN(N(so, empty));
    return MUNIT_OK;
}

static MunitResult _test_comparisons(fixture_type_t *type, fixture_type_t *x_type,
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

        // Exception passthrough.
        assert_isexception(any_cmp(so, yp_SyntaxError), yp_SyntaxError);

        assert_set(so, items[0], items[1]);  // so unchanged.
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

        // Exception passthrough.
        assert_isexception(any_cmp(empty, yp_SyntaxError), yp_SyntaxError);

        assert_len(empty, 0);  // empty unchanged.
        yp_decref(empty);
    }

    // x contains unhashable objects.
    if (type_stores_unhashables(x_type)) {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *unhashable = rand_obj_any_mutable_unique(1, &int_1);
        ypObject *so = type->newN(N(items[0], int_1));
        ypObject *empty = type->newN(0);

        // x has the same items.
        ead(x, x_type->newN(N(items[0], intstore_1)), assert_obj(any_cmp(so, x), is, x_same));

        // x is is a proper subset and is not empty.
        ead(x, x_type->newN(N(intstore_1)), assert_obj(any_cmp(so, x), is, x_subset));

        // x is a proper superset.
        ead(x, x_type->newN(N(items[0], intstore_1, items[1])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], int_1, unhashable)),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, x_type->newN(N(items[0], intstore_1, unhashable)),
                assert_obj(any_cmp(so, x), is, x_superset));

        // x overlaps and contains additional items.
        ead(x, x_type->newN(N(intstore_1, items[1])), assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(int_1, unhashable)), assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, x_type->newN(N(intstore_1, unhashable)), assert_obj(any_cmp(so, x), is, x_overlap));

        // x does not overlap and contains additional items.
        ead(x, x_type->newN(N(unhashable)), assert_obj(any_cmp(so, x), is, x_no_overlap));

        // so is empty.
        ead(x, x_type->newN(N(unhashable)), assert_obj(any_cmp(empty, x), is, so_empty));

        yp_decrefN(N(int_1, intstore_1, unhashable, so, empty));
    }

    // Implementations may use the cached hash as a quick inequality test. Recall that only
    // immutables can cache their hash, which occurs when yp_hashC is called. Because the cached
    // hash is an internal optimization, it should only be used with friendly types.
    if (!type->is_mutable && !x_type->is_mutable && type_iscomparable(type, x_type)) {
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
    return MUNIT_OK;
}

static MunitResult test_isdisjoint(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[4];
    MunitResult      test_result;
    obj_array_fill(items, type->rand_items);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        test_result = _test_comparisons(type, (*x_type), yp_isdisjoint, /*x_same=*/yp_False,
                /*x_empty=*/yp_True, /*x_subset=*/yp_False, /*x_superset=*/yp_False,
                /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_True, /*so_empty=*/yp_True,
                /*both_empty=*/yp_True);
        if (test_result != MUNIT_OK) goto tear_down;
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
    test_result =
            _test_comparisons_not_supported(type, fixture_type_int, yp_isdisjoint, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    obj_array_decref(items);
    return test_result;
}

static MunitResult test_issubset(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[4];
    MunitResult      test_result;
    obj_array_fill(items, type->rand_items);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        test_result = _test_comparisons(type, (*x_type), yp_issubset, /*x_same=*/yp_True,
                /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_True,
                /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_True,
                /*both_empty=*/yp_True);
        if (test_result != MUNIT_OK) goto tear_down;
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
    test_result =
            _test_comparisons_not_supported(type, fixture_type_int, yp_issubset, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    obj_array_decref(items);
    return test_result;
}

static MunitResult test_issuperset(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[4];
    MunitResult      test_result;
    obj_array_fill(items, type->rand_items);

    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        test_result = _test_comparisons(type, (*x_type), yp_issuperset, /*x_same=*/yp_True,
                /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_False,
                /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                /*both_empty=*/yp_True);
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[0], items[1])), result = yp_issuperset(so, x),
                      assert_obj(result, is, yp_True), yp_decref(so));

    // x is not an iterable.
    test_result =
            _test_comparisons_not_supported(type, fixture_type_int, yp_issuperset, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    obj_array_decref(items);
    return test_result;
}

static MunitResult test_lt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    MunitResult      test_result;

    // lt is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_iscomparable(type, (*x_type))) {
            test_result = _test_comparisons(type, (*x_type), yp_lt, /*x_same=*/yp_False,
                    /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_True,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_True,
                    /*both_empty=*/yp_False);
        } else {
            test_result = _test_comparisons_not_supported(type, (*x_type), yp_lt, yp_TypeError);
        }
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // _test_comparisons_faulty_iter not called as lt doesn't support iterators.

    // x is not an iterable.
    test_result = _test_comparisons_not_supported(type, fixture_type_int, yp_lt, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    return test_result;
}

static MunitResult test_le(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    MunitResult      test_result;

    // le is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_iscomparable(type, (*x_type))) {
            test_result = _test_comparisons(type, (*x_type), yp_le, /*x_same=*/yp_True,
                    /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_True,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_True,
                    /*both_empty=*/yp_True);
        } else {
            test_result = _test_comparisons_not_supported(type, (*x_type), yp_le, yp_TypeError);
        }
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // _test_comparisons_faulty_iter not called as le doesn't support iterators.

    // x is not an iterable.
    test_result = _test_comparisons_not_supported(type, fixture_type_int, yp_le, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    return test_result;
}

static MunitResult test_eq(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    MunitResult      test_result;

    // eq is only supported for friendly x. All others compare unequal.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_iscomparable(type, (*x_type))) {
            test_result = _test_comparisons(type, (*x_type), yp_eq, /*x_same=*/yp_True,
                    /*x_empty=*/yp_False, /*x_subset=*/yp_False, /*x_superset=*/yp_False,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                    /*both_empty=*/yp_True);
        } else {
            test_result = _test_comparisons_not_supported(type, (*x_type), yp_eq, yp_False);
        }
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // _test_comparisons_faulty_iter not called as eq doesn't support iterators.

    // x is not an iterable.
    test_result = _test_comparisons_not_supported(type, fixture_type_int, yp_eq, yp_False);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    return test_result;
}

static MunitResult test_ne(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    MunitResult      test_result;

    // ne is only supported for friendly x. All others compare unequal.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_iscomparable(type, (*x_type))) {
            test_result = _test_comparisons(type, (*x_type), yp_ne, /*x_same=*/yp_False,
                    /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_True,
                    /*x_overlap=*/yp_True, /*x_no_overlap=*/yp_True, /*so_empty=*/yp_True,
                    /*both_empty=*/yp_False);
        } else {
            test_result = _test_comparisons_not_supported(type, (*x_type), yp_ne, yp_True);
        }
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // _test_comparisons_faulty_iter not called as ne doesn't support iterators.

    // x is not an iterable.
    test_result = _test_comparisons_not_supported(type, fixture_type_int, yp_ne, yp_True);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    return test_result;
}

static MunitResult test_ge(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    MunitResult      test_result;

    // ge is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_iscomparable(type, (*x_type))) {
            test_result = _test_comparisons(type, (*x_type), yp_ge, /*x_same=*/yp_True,
                    /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_False,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                    /*both_empty=*/yp_True);
        } else {
            test_result = _test_comparisons_not_supported(type, (*x_type), yp_ge, yp_TypeError);
        }
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // _test_comparisons_faulty_iter not called as ge doesn't support iterators.

    // x is not an iterable.
    test_result = _test_comparisons_not_supported(type, fixture_type_int, yp_ge, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    return test_result;
}

static MunitResult test_gt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    MunitResult      test_result;

    // gt is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (type_iscomparable(type, (*x_type))) {
            test_result = _test_comparisons(type, (*x_type), yp_gt, /*x_same=*/yp_False,
                    /*x_empty=*/yp_True, /*x_subset=*/yp_True, /*x_superset=*/yp_False,
                    /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_False, /*so_empty=*/yp_False,
                    /*both_empty=*/yp_False);
        } else {
            test_result = _test_comparisons_not_supported(type, (*x_type), yp_gt, yp_TypeError);
        }
        if (test_result != MUNIT_OK) goto tear_down;
    }

    // _test_comparisons_faulty_iter not called as gt doesn't support iterators.

    // x is not an iterable.
    test_result = _test_comparisons_not_supported(type, fixture_type_int, yp_gt, yp_TypeError);
    if (test_result != MUNIT_OK) goto tear_down;

tear_down:
    return test_result;
}

static MunitResult test_union(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = friend_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic union: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1], items[2]);
        assert_set(so, items[0], items[1]);  // so unchanged.
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_union(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
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
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains duplicates
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_union(so, x);
        assert_set(result, items[0], items[1], items[2]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises(yp_union(so, x), yp_TypeError);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable objects in x should always cause a failure, even if that object is not part of the
    // result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            assert_raises(yp_union(so, x), yp_TypeError);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_union(so, x),
                      assert_set(result, items[0], items[1], items[2]), yp_decrefN(N(so, result)));

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
        assert_raises(yp_union(so, int_1), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_union(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_intersection(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = friend_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic intersection: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_intersection(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[1]);
        assert_set(so, items[0], items[1]);  // so unchanged.
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
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains duplicates
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_intersection(so, x);
        assert_set(result, items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            ypObject *result = yp_intersection(so, x);
            assert_set(result, items[1]);
            yp_decrefN(N(unhashable, so, x, result));
        }
    }

    // An unhashable object in x should match the equal object in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            ypObject *result = yp_intersection(so, x);
            assert_set(result, int_1);
            yp_decrefN(N(intstore_1, so, x, result));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_intersection(so, x),
                      assert_set(result, items[1]), yp_decrefN(N(so, result)));

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
        assert_raises(yp_intersection(so, int_1), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_intersection(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_difference(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = friend_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic difference: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_difference(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0]);
        assert_set(so, items[0], items[1]);  // so unchanged.
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
        assert_set(result, items[0], items[1]);
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

    // x contains duplicates
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_difference(so, x);
        assert_set(result, items[0]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            ypObject *result = yp_difference(so, x);
            assert_set(result, items[0]);
            yp_decrefN(N(unhashable, so, x, result));
        }
    }

    // An unhashable object in x should match the equal object in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            ypObject *result = yp_difference(so, x);
            assert_len(result, 0);
            yp_decrefN(N(intstore_1, so, x, result));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_difference(so, x),
                      assert_set(result, items[0]), yp_decrefN(N(so, result)));

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
        assert_raises(yp_difference(so, int_1), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_difference(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_symmetric_difference(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = friend_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic symmetric_difference: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[2]);
        assert_set(so, items[0], items[1]);  // so unchanged.
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_symmetric_difference(so, x);
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
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

    // x contains duplicates. (This is a particularly interesting test for symmetric_difference.)
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        ypObject *result = yp_symmetric_difference(so, x);
        assert_set(result, items[0], items[2]);
        yp_decrefN(N(so, x, result));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises(yp_symmetric_difference(so, x), yp_TypeError);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable objects in x should always cause a failure, even if that object is not part of the
    // result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            assert_raises(yp_symmetric_difference(so, x), yp_TypeError);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *so = type->newN(N(items[0], items[1])); ypObject * result, x,
                      yp_tupleN(N(items[1], items[2])), result = yp_symmetric_difference(so, x),
                      assert_set(result, items[0], items[2]), yp_decrefN(N(so, result)));

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
        assert_raises(yp_symmetric_difference(so, int_1), yp_TypeError);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception(yp_symmetric_difference(so, yp_SyntaxError), yp_SyntaxError);
        yp_decrefN(N(so));
    }

    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_update(so, x, &exc), yp_MethodError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_set(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_set(so, items[0], items[1]);
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_set(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises_exc(yp_update(so, x, &exc), yp_TypeError);
            assert_set(so, items[0], items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable objects in x should always cause a failure, even if that object is not part of the
    // result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            assert_raises_exc(yp_update(so, x, &exc), yp_TypeError);
            assert_set(so, int_1);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_update(so, x, &exc),
            assert_set(so, items[0], items[1], items[2]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // Optimization: we add directly to so from the iterator. Unfortunately, if the iterator
    // fails mid-way so may have already been modified.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_update(so, int_1, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_intersection_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support intersection_update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_MethodError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic intersection_update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_set(so, items[1]);
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_set(so, items[1]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            assert_set(so, items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // An unhashable object in x should match the equal object in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            assert_set(so, int_1);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_intersection_update(so, x, &exc),
            assert_set(so, items[1]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // so is not modified if the iterator fails mid-way: intersection_update needs to yield all
    // items before modifying so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_intersection_update(so, int_1, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_intersection_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_difference_update(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support difference_update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_MethodError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic difference_update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_set(so, items[0]);
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
        assert_set(so, items[0], items[1]);
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

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_set(so, items[0]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            assert_set(so, items[0]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // An unhashable object in x should match the equal object in so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            assert_len(so, 0);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_difference_update(so, x, &exc),
            assert_set(so, items[0]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // Optimization: we discard from so directly from the iterator. Unfortunately, if the iterator
    // fails mid-way so may have already been modified.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_difference_update(so, int_1, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_difference_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
}

static MunitResult test_symmetric_difference_update(
        const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support symmetric_difference_update.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = type->newN(N(items[1], items[2]));
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_MethodError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic symmetric_difference_update: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_set(so, items[0], items[2]);
        yp_decrefN(N(so, x));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_set(so, items[0], items[1]);
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

    // x contains duplicates. (This is a particularly interesting test for symmetric_difference.)
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[1], items[2], items[2]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_set(so, items[0], items[2]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError);
            assert_set(so, items[0], items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Unhashable objects in x should always cause a failure, even if that object is not part of the
    // result.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *intstore_1 = yp_intstoreC(1);
            ypObject *so = type->newN(N(int_1));
            ypObject *x = (*x_type)->newN(N(intstore_1));
            assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError);
            assert_set(so, int_1);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *so = type->newN(N(items[0], items[1])), x,
            yp_tupleN(N(items[1], items[2])), yp_symmetric_difference_update(so, x, &exc),
            assert_set(so, items[0], items[2]), yp_decref(so));

    // so is not modified if the iterator fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // so is not modified if the iterator fails mid-way: symmetric_difference_update needs to yield
    // all items before modifying so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_symmetric_difference_update(so, int_1, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(
                yp_symmetric_difference_update(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(int_1));
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic push.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_push(so, items[2], &exc));
        assert_set(so, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_push(so, items[3], &exc));
        assert_set(so, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(so));
    }

    // Item already in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_push(so, items[1], &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_not_raises_exc(yp_push(so, items[0], &exc));
        assert_set(so, items[0]);
        yp_decrefN(N(so));
    }

    // FIXME x is so (and everywhere)

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_raises_exc(yp_push(so, unhashable, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // Unhashable items should always cause TypeError, even if that item is already in so.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *so = type->newN(N(int_1));
        assert_raises_exc(yp_push(so, intstore_1, &exc), yp_TypeError);
        assert_set(so, int_1);
        yp_decrefN(N(int_1, intstore_1, so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_push(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic pushunique.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_pushunique(so, items[2], &exc));
        assert_set(so, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_pushunique(so, items[3], &exc));
        assert_set(so, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(so));
    }

    // Item already in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_pushunique(so, items[1], &exc), yp_KeyError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_not_raises_exc(yp_pushunique(so, items[0], &exc));
        assert_set(so, items[0]);
        yp_decrefN(N(so));
    }

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_raises_exc(yp_pushunique(so, unhashable, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // Unhashable items should always cause TypeError, even if that item is already in so.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *so = type->newN(N(int_1));
        assert_raises_exc(yp_pushunique(so, intstore_1, &exc), yp_TypeError);
        assert_set(so, int_1);
        yp_decrefN(N(int_1, intstore_1, so));
    }

    // Exception passthrough.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_pushunique(so, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult _test_remove(
        fixture_type_t *type, void (*any_remove)(ypObject *, ypObject *, ypObject **), int raises)
{
    ypObject *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support remove.
    if (!type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_remove(so, items[1], &exc), yp_MethodError);
        assert_set(so, items[0], items[1]);
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
        assert_set(so, items[0]);
        assert_not_raises_exc(any_remove(so, items[0], &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Item not in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_found_exc(any_remove(so, items[2], &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_not_found_exc(any_remove(so, items[0], &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Item is unhashable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_not_found_exc(any_remove(so, unhashable, &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // An unhashable x should match the equal object in so.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *so = type->newN(N(int_1));
        assert_not_raises_exc(any_remove(so, intstore_1, &exc));
        assert_len(so, 0);
        yp_decrefN(N(int_1, intstore_1, so));
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

#undef assert_not_found_exc

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_remove(const MunitParameter params[], fixture_t *fixture)
{
    return _test_remove(fixture->type, yp_remove, /*raises=*/TRUE);
}

static MunitResult test_discard(const MunitParameter params[], fixture_t *fixture)
{
    return _test_remove(fixture->type, yp_discard, /*raises=*/FALSE);
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
        assert_set(so, items[0], items[1]);
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
            assert_set(so, item_1);
            ead(popped, yp_pop(so), assert_obj(popped, eq, item_1));
        } else {
            assert_obj(first, eq, item_1);
            assert_set(so, item_0);
            ead(popped, yp_pop(so), assert_obj(popped, eq, item_0));
        }
        assert_len(so, 0);
        assert_raises(yp_pop(so), yp_KeyError);
        assert_len(so, 0);
        assert_raises(yp_pop(so), yp_KeyError);  // Calling again still raises KeyError.
        assert_len(so, 0);
        yp_decrefN(N(so, first));
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


#include "munit_test/unittest.h"

// XXX Because Python calls both the protocol and the object "set", I'm using the term "set-like" to
// refer to the protocol where there may be confusion.

// Sets should accept themselves, their pairs, iterators, frozenset/set, and frozendict/dict as
// valid types for the "x" (i.e. "other iterable") argument.
// FIXME Should dict really be here?
// TODO Add dict key and item views here, when implemented.
#define x_types_init(type)                                                                 \
    {                                                                                      \
        (type), (type)->pair, fixture_type_iter, fixture_type_frozenset, fixture_type_set, \
                fixture_type_frozendict, fixture_type_dict, NULL                           \
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


// TODO Move to test_container? Will need an `if set or dict` around the unhashable test.
static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic contains (and in and not_in).
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_obj(yp_contains(so, items[1]), is, yp_True);
        assert_obj(yp_in(items[1], so), is, yp_True);
        assert_obj(yp_not_in(items[1], so), is, yp_False);
        yp_decrefN(N(so));
    }

    // Item not in so.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_obj(yp_contains(so, items[2]), is, yp_False);
        assert_obj(yp_in(items[2], so), is, yp_False);
        assert_obj(yp_not_in(items[2], so), is, yp_True);
        yp_decrefN(N(so));
    }

    // so is empty.
    {
        ypObject *so = type->newN(0);
        assert_obj(yp_contains(so, items[0]), is, yp_False);
        assert_obj(yp_in(items[0], so), is, yp_False);
        assert_obj(yp_not_in(items[0], so), is, yp_True);
        yp_decrefN(N(so));
    }

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

    if (yp_isexceptionC(expected)) {
        ead(x, rand_obj(x_type), assert_raises(any_cmp(so, x), expected));
        ead(x, rand_obj(x_type), assert_raises(any_cmp(empty, x), expected));
    } else {
        ead(x, rand_obj(x_type), assert_obj(any_cmp(so, x), is, expected));
        ead(x, rand_obj(x_type), assert_obj(any_cmp(empty, x), is, expected));
    }

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

    obj_array_decref(items);
    return MUNIT_OK;
}

// XXX Ensure you pick an so and x_supplier that will exhaust the iterator.
static MunitResult _test_comparisons_faulty_iter(ypObject *(*any_cmp)(ypObject *, ypObject *),
        ypObject *so, ypObject *x_supplier, ypObject *expected)
{
    // x is an iterator that fails at the start.
    {
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(any_cmp(so, x), yp_SyntaxError);
        yp_decrefN(N(x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(any_cmp(so, x), yp_SyntaxError);
        yp_decrefN(N(x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_obj(any_cmp(so, x), is, expected);
        yp_decrefN(N(x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_obj(any_cmp(so, x), is, expected);
        yp_decrefN(N(x));
    }

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

    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[2], items[3]));
        test_result = _test_comparisons_faulty_iter(yp_isdisjoint, so, x_supplier, yp_True);
        yp_decrefN(N(so, x_supplier));
        if (test_result != MUNIT_OK) goto tear_down;
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

    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[2], items[3]));
        test_result = _test_comparisons_faulty_iter(yp_issubset, so, x_supplier, yp_False);
        yp_decrefN(N(so, x_supplier));
        if (test_result != MUNIT_OK) goto tear_down;
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

    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[0], items[1]));
        test_result = _test_comparisons_faulty_iter(yp_issuperset, so, x_supplier, yp_True);
        yp_decrefN(N(so, x_supplier));
        if (test_result != MUNIT_OK) goto tear_down;
    }

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
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic union: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_unionN(so, N(x));
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1], items[2]);
        // FIXME Assert so is unchanged (here and everywhere)?
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_unionN(so, N(x));
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_unionN(so, N(x));
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_unionN(so, N(x));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_unionN(so, N(so));
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // Multiple x objects.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x0 = (*x_type)->newN(N(items[2]));
        ypObject *x1 = (*x_type)->newN(N(items[3]));
        ypObject *result = yp_unionN(so, N(x0, x1));
        assert_set(result, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(so, x0, x1, result));
    }

    // Zero x objects.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_unionN(so, 0);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains an unhashable object.
    // FIXME Even if the unhashable object equals an item in so, there should be an error (and
    // elsewhere)
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises(yp_unionN(so, N(x)), yp_TypeError);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // Optimization: lazy shallow copy of an immutable so when n is zero.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_unionN(so, 0);
        if (type->is_mutable) {
            assert_obj(so, is_not, result);
        } else {
            assert_obj(so, is, result);
        }
        yp_decrefN(N(so, result));
    }

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_unionN(so, N(x)), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_unionN(so, N(x)), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        ypObject *result = yp_unionN(so, N(x));
        assert_set(result, items[0], items[1], items[2]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        ypObject *result = yp_unionN(so, N(x));
        assert_set(result, items[0], items[1], items[2]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_unionN(so, N(int_1)), yp_TypeError);
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
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic intersection: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_intersectionN(so, N(x));
        assert_type_is(result, type->type);
        assert_set(result, items[1]);
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_intersectionN(so, N(x));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_intersectionN(so, N(x));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_intersectionN(so, N(x));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_intersectionN(so, N(so));
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // Multiple x objects.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x0 = (*x_type)->newN(N(items[0]));
        ypObject *x1 = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_intersectionN(so, N(x0, x1));
        assert_set(result, items[0]);
        yp_decrefN(N(so, x0, x1, result));
    }

    // Zero x objects.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_intersectionN(so, 0);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            // FIXME This fails in Python; should it fail for us?
            ypObject *result = yp_intersectionN(so, N(x));
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
            // FIXME This fails in Python; should it fail for us?
            ypObject *result = yp_intersectionN(so, N(x));
            assert_set(result, int_1);
            yp_decrefN(N(intstore_1, so, x, result));
        }
    }

    // Optimization: lazy shallow copy of an immutable so when n is zero.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_intersectionN(so, 0);
        if (type->is_mutable) {
            assert_obj(so, is_not, result);
        } else {
            assert_obj(so, is, result);
        }
        yp_decrefN(N(so, result));
    }

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_intersectionN(so, N(x)), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_intersectionN(so, N(x)), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        ypObject *result = yp_intersectionN(so, N(x));
        assert_set(result, items[1]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        ypObject *result = yp_intersectionN(so, N(x));
        assert_set(result, items[1]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_intersectionN(so, N(int_1)), yp_TypeError);
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
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Basic difference: items[0] only in so, items[1] in both, items[2] only in x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[1], items[2]));
        ypObject *result = yp_differenceN(so, N(x));
        assert_type_is(result, type->type);
        assert_set(result, items[0]);
        yp_decrefN(N(so, x, result));
    }

    // so is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_differenceN(so, N(x));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_differenceN(so, N(x));
        assert_type_is(result, type->type);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_differenceN(so, N(x));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, x, result));
    }

    // x can be so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_differenceN(so, N(so));
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, result));
    }

    // Multiple x objects.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x0 = (*x_type)->newN(N(items[0]));
        ypObject *x1 = (*x_type)->newN(N(items[1]));
        ypObject *result = yp_differenceN(so, N(x0, x1));
        assert_len(result, 0);
        yp_decrefN(N(so, x0, x1, result));
    }

    // Zero x objects.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_differenceN(so, 0);
        assert_set(result, items[0], items[1]);
        yp_decrefN(N(so, result));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            // FIXME This fails in Python; should it fail for us?
            ypObject *result = yp_differenceN(so, N(x));
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
            // FIXME This fails in Python; should it fail for us?
            ypObject *result = yp_differenceN(so, N(x));
            assert_len(result, 0);
            yp_decrefN(N(intstore_1, so, x, result));
        }
    }

    // Optimization: lazy shallow copy of an immutable so when n is zero.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_differenceN(so, 0);
        if (type->is_mutable) {
            assert_obj(so, is_not, result);
        } else {
            assert_obj(so, is, result);
        }
        yp_decrefN(N(so, result));
    }

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_differenceN(so, N(x)), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_differenceN(so, N(x)), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        ypObject *result = yp_differenceN(so, N(x));
        assert_set(result, items[0]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        ypObject *result = yp_differenceN(so, N(x));
        assert_set(result, items[0]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_differenceN(so, N(int_1)), yp_TypeError);
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
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *result = yp_symmetric_difference(so, so);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(so, result));
    }

    // There is no N version of yp_symmetric_difference, so we always have one x object.

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

    // FIXME if x contains duplicates, those items should only be considered once (i.e.
    // {1}.symmetric_difference([1, 1]) and {1}.symmetric_difference([2, 2])). And similar tests for
    // others?

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_symmetric_difference(so, x), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_symmetric_difference(so, x), yp_SyntaxError);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        ypObject *result = yp_symmetric_difference(so, x);
        assert_set(result, items[0], items[2]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        ypObject *result = yp_symmetric_difference(so, x);
        assert_set(result, items[0], items[2]);
        yp_decrefN(N(so, x_supplier, x, result));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises(yp_symmetric_difference(so, int_1), yp_TypeError);
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
        assert_raises_exc(yp_updateN(so, &exc, N(x)), yp_MethodError);
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
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_update(so, so, &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Multiple x objects.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x0 = (*x_type)->newN(N(items[2]));
        ypObject *x1 = (*x_type)->newN(N(items[3]));
        assert_not_raises_exc(yp_updateN(so, &exc, N(x0, x1)));
        assert_set(so, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(so, x0, x1));
    }

    // Zero x objects.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_updateN(so, &exc, 0));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises_exc(yp_update(so, x, &exc), yp_TypeError);
            // FIXME Tests for updates that fail mid-way?
            assert_set(so, items[0], items[1]);
            yp_decrefN(N(unhashable, so, x));
        }
    }

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_update(so, x, &exc), yp_SyntaxError);
        // FIXME choose x so we can validate it fails mid-way, here and elsewhere.
        // so is updated after each item yielded.
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_not_raises_exc(yp_update(so, x, &exc));
        assert_set(so, items[0], items[1], items[2]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_not_raises_exc(yp_update(so, x, &exc));
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
        assert_raises_exc(yp_intersection_updateN(so, &exc, N(x)), yp_MethodError);
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
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_intersection_update(so, so, &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Multiple x objects.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x0 = (*x_type)->newN(N(items[0]));
        ypObject *x1 = (*x_type)->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_intersection_updateN(so, &exc, N(x0, x1)));
        assert_set(so, items[0]);
        yp_decrefN(N(so, x0, x1));
    }

    // Zero x objects.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_intersection_updateN(so, &exc, 0));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            // FIXME This fails in Python; should it fail for us?
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            // FIXME Tests for intersection_updates that fail mid-way?
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
            // FIXME This fails in Python; should it fail for us?
            assert_not_raises_exc(yp_intersection_update(so, x, &exc));
            assert_set(so, int_1);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_SyntaxError);
        // so is updated after each item yielded.
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_set(so, items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_not_raises_exc(yp_intersection_update(so, x, &exc));
        assert_set(so, items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_intersection_update(so, int_1, &exc), yp_TypeError);
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
        assert_raises_exc(yp_difference_updateN(so, &exc, N(x)), yp_MethodError);
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
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_difference_update(so, so, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Multiple x objects.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x0 = (*x_type)->newN(N(items[0]));
        ypObject *x1 = (*x_type)->newN(N(items[1]));
        assert_not_raises_exc(yp_difference_updateN(so, &exc, N(x0, x1)));
        assert_len(so, 0);
        yp_decrefN(N(so, x0, x1));
    }

    // Zero x objects.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_difference_updateN(so, &exc, 0));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            // FIXME This fails in Python; should it fail for us?
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            // FIXME Tests for difference_updates that fail mid-way?
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
            // FIXME This fails in Python; should it fail for us?
            assert_not_raises_exc(yp_difference_update(so, x, &exc));
            assert_len(so, 0);
            yp_decrefN(N(intstore_1, so, x));
        }
    }

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_difference_update(so, x, &exc), yp_SyntaxError);
        // so is updated after each item yielded.
        assert_set(so, items[0]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
        assert_set(so, items[0]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_not_raises_exc(yp_difference_update(so, x, &exc));
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
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_symmetric_difference_update(so, so, &exc));
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // There is no N version of yp_symmetric_difference_update, so we always have one x object.

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ypObject *so = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[1], unhashable));
            assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_TypeError);
            // FIXME Tests for symmetric_difference_updates that fail mid-way?
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

    // x is an iterator that fails at the start.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_SyntaxError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_SyntaxError);
        // FIXME choose x so we can validate it fails mid-way, here and elsewhere.
        // so is symmetric_difference_updated after each item yielded.  FIXME incorrect!
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_set(so, items[0], items[2]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = yp_tupleN(N(items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_not_raises_exc(yp_symmetric_difference_update(so, x, &exc));
        assert_set(so, items[0], items[2]);
        yp_decrefN(N(so, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_symmetric_difference_update(so, int_1, &exc), yp_TypeError);
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

    // Item is unhashable.
    // FIXME another test: unhashable, but equal to existing, should still fail (elsewhere?)
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_raises_exc(yp_push(so, unhashable, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
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
    // FIXME and test if it equals an item already in so: yp_TypeError should win out
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_raises_exc(yp_pushunique(so, unhashable, &exc), yp_TypeError);
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
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
        // FIXME what if unhashable equals one of the items (here and everywhere)?
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
        assert_not_found_exc(any_remove(so, unhashable, &exc));
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, unhashable));
    }

    // An unhashable object in x should match the equal object in so.
    {
        ypObject *int_1 = yp_intC(1);
        ypObject *intstore_1 = yp_intstoreC(1);
        ypObject *so = type->newN(N(int_1));
        // FIXME This fails in Python; should it fail for us?
        assert_not_raises_exc(any_remove(so, intstore_1, &exc));
        assert_len(so, 0);
        yp_decrefN(N(int_1, intstore_1, so));
    }

    // Remove an item from a set. The hash should equal a "clean" set. This was a bug from the
    // implementation of the yp_HashSet functions.
    {
        ypObject *expected = type->newN(N(items[0]));
        yp_hash_t expected_hash;
        ypObject *so = type->newN(N(items[0], items[1]));
        yp_hash_t so_hash;

        assert_not_raises_exc(any_remove(so, items[1], &exc));

        // FIXME Make an assert_hashes_equal?
        assert_not_raises_exc(expected_hash = yp_currenthashC(expected, &exc));
        assert_not_raises_exc(so_hash = yp_currenthashC(so, &exc));
        assert_hashC(expected_hash, ==, so_hash);

        yp_decrefN(N(expected, so));
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
    ypObject       *items[2];
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

    // FIXME multiple pops (in any order)

    // Self is empty.
    {
        ypObject *so = type->newN(0);
        assert_raises(yp_pop(so), yp_KeyError);
        assert_len(so, 0);
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

/*
FIXME Fix this inconsistently-failing testcase

Build\msvs_120\win32_amd64_release\python_test.log

======================================================================
FAIL: test_find_periodic_pattern (python_test.test_bytes.ByteArrayAsStringTest) (p='', text='')
Cover the special path for periodic patterns.
----------------------------------------------------------------------
Traceback (most recent call last):
  File "C:\projects\nohtyp\Lib\python_test\string_tests.py", line 365, in test_find_periodic_pattern
    self.checkequal(reference_find(p, text),
  File "C:\projects\nohtyp\Lib\python_test\string_tests.py", line 73, in checkequal
    self.assertEqual(
  File "C:\projects\nohtyp\Lib\python_test\yp_unittest\case.py", line 59, in assertEqual
    _unittest.TestCase.assertEqual(self, first, second, msg)
AssertionError: -1 != 0
*/

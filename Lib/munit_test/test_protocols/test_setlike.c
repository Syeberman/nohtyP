
#include "munit_test/unittest.h"

// XXX Because Python calls both the protocol and the object "set", I'm using the term "set-like" to
// refer to the protocol where there may be confusion.

// FIXME For read-only operations, I believe we allow to use non-hashable types, but for operations
// that might modify we should always require hashable types.


// Sets should accept themselves, their pairs, iterators, tuple/list, and frozenset/set as valid
// types for the "x" (i.e. "other iterable") argument.
// FIXME Also dict
// TODO Also dict key and item views, when implemented.
#define x_types_init(type)                                                              \
    {                                                                                   \
        (type), (type)->pair, fixture_type_iter, fixture_type_tuple, fixture_type_list, \
                fixture_type_frozenset, fixture_type_set, NULL                          \
    }


static MunitResult _test_comparisons(fixture_type_t *type, fixture_type_t *x_types[],
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject *x_same, ypObject *x_empty,
        ypObject *x_subset, ypObject *x_superset, ypObject *x_overlap, ypObject *x_no_overlap)
{
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // Typical so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(any_cmp(so, x), is, x_same));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(any_cmp(so, x), is, x_same));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(any_cmp(so, x), is, x_empty));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(any_cmp(so, x), is, x_subset));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(any_cmp(so, x), is, x_subset));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(any_cmp(so, x), is, x_subset));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(any_cmp(so, x), is, x_superset));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(any_cmp(so, x), is, x_superset));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])), assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, (*x_type)->newN(N(items[1], items[2])), assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(any_cmp(so, x), is, x_overlap));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(any_cmp(so, x), is, x_overlap));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(any_cmp(so, x), is, x_no_overlap));
        ead(x, (*x_type)->newN(N(items[2], items[3])),
                assert_obj(any_cmp(so, x), is, x_no_overlap));
        ead(x, (*x_type)->newN(N(items[2], items[2])),
                assert_obj(any_cmp(so, x), is, x_no_overlap));

        // x is so.
        assert_obj(any_cmp(so, so), is, x_same);
    }

    // Empty so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        // FIXME x_superset or x_no_overlap?
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(any_cmp(empty, x), is, x_no_overlap));
        ead(x, (*x_type)->newN(N(items[0], items[1])),
                assert_obj(any_cmp(empty, x), is, x_no_overlap));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(any_cmp(empty, x), is, x_no_overlap));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(any_cmp(empty, x), is, x_no_overlap));

        // x is so.
        assert_obj(any_cmp(empty, empty), is, x_no_overlap);
    }

    // FIXME What if x contains unhashable objects?

    // x is not an iterable.
    assert_raises(any_cmp(so, int_1), yp_TypeError);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
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
    fixture_type_t *type = fixture->type;
    fixture_type_t *x_types[] = x_types_init(type);
    ypObject       *items[4];
    ypObject       *so;
    MunitResult     test_result;
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    test_result = _test_comparisons(type, x_types, yp_isdisjoint, /*x_same=*/yp_False,
            /*x_empty=*/yp_True, /*x_subset=*/yp_False, /*x_superset=*/yp_False,
            /*x_overlap=*/yp_False, /*x_no_overlap=*/yp_True);
    if (test_result != MUNIT_OK) goto tear_down;

    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        test_result = _test_comparisons_faulty_iter(yp_isdisjoint, so, x_supplier, yp_True);
        yp_decrefN(N(x_supplier));
        if (test_result != MUNIT_OK) goto tear_down;
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(so));
    return test_result;
}

static MunitResult test_issubset(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // Basic issubset.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_issubset(so, x), is, yp_True));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_issubset(so, x), is, yp_False));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(yp_issubset(so, x), is, yp_False));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_True));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));

        // x is so.
        assert_obj(yp_issubset(so, so), is, yp_True);
    }

    // Empty so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_issubset(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1])),
                assert_obj(yp_issubset(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(yp_issubset(empty, x), is, yp_True));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_issubset(empty, x), is, yp_True));

        // x is so.
        assert_obj(yp_issubset(empty, empty), is, yp_True);
    }

    // FIXME What if x contains unhashable objects?

    // x is an iterator that fails at the start.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_issubset(so, x), yp_SyntaxError);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_issubset(so, x), yp_SyntaxError);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_obj(yp_issubset(so, x), is, yp_False);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_obj(yp_issubset(so, x), is, yp_False);
        yp_decrefN(N(x_supplier, x));
    }

    // x is not an iterable.
    assert_raises(yp_issubset(so, int_1), yp_TypeError);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

static MunitResult test_lt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = {type, type->pair, NULL};
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // lt is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if ((*x_type) == type || (*x_type) == type->pair) continue;
        ead(x, (*x_type)->newN(0), assert_raises(yp_lt(so, x), yp_TypeError));
        ead(x, (*x_type)->newN(0), assert_raises(yp_lt(empty, x), yp_TypeError));
    }

    // Basic lt.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_lt(so, x), is, yp_False));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_lt(so, x), is, yp_False));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_lt(so, x), is, yp_False));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_lt(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_lt(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_lt(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_lt(so, x), is, yp_True));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_lt(so, x), is, yp_False));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[3])), assert_obj(yp_lt(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[2])), assert_obj(yp_lt(so, x), is, yp_False));

        // x is so.
        assert_obj(yp_lt(so, so), is, yp_False);
    }

    // Empty so.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_lt(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_lt(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_lt(empty, x), is, yp_True));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_lt(empty, x), is, yp_False));

        // x is so.
        assert_obj(yp_lt(empty, empty), is, yp_False);
    }

    // XXX x cannot contain unhashable objects as lt is only supported on friendly types.

    // x is not an iterable.
    assert_raises(yp_lt(so, int_1), yp_TypeError);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

static MunitResult test_le(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = {type, type->pair, NULL};
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // le is only supported for friendly x.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if ((*x_type) == type || (*x_type) == type->pair) continue;
        ead(x, (*x_type)->newN(0), assert_raises(yp_le(so, x), yp_TypeError));
        ead(x, (*x_type)->newN(0), assert_raises(yp_le(empty, x), yp_TypeError));
    }

    // Basic le.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_le(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_le(so, x), is, yp_True));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_le(so, x), is, yp_False));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_le(so, x), is, yp_False));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_le(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_le(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_le(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_le(so, x), is, yp_True));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])), assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2])), assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_le(so, x), is, yp_False));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[3])), assert_obj(yp_le(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[2])), assert_obj(yp_le(so, x), is, yp_False));

        // x is so.
        assert_obj(yp_le(so, so), is, yp_True);
    }

    // Empty so.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_le(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_le(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_le(empty, x), is, yp_True));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_le(empty, x), is, yp_True));

        // x is so.
        assert_obj(yp_le(empty, empty), is, yp_True);
    }

    // XXX x cannot contain unhashable objects as le is only supported on friendly types.

    // x is not an iterable.
    assert_raises(yp_le(so, int_1), yp_TypeError);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

static MunitResult test_eq(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = {type, type->pair, NULL};
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // eq is only supported for friendly x. All others compare unequal.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if ((*x_type) == type || (*x_type) == type->pair) continue;
        ead(x, (*x_type)->newN(0), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(0), assert_obj(yp_eq(empty, x), is, yp_False));
    }

    // Basic eq.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_eq(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_eq(so, x), is, yp_True));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_eq(so, x), is, yp_False));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_eq(so, x), is, yp_False));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_eq(so, x), is, yp_False));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2])), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_eq(so, x), is, yp_False));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[3])), assert_obj(yp_eq(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[2])), assert_obj(yp_eq(so, x), is, yp_False));

        // x is so.
        assert_obj(yp_eq(so, so), is, yp_True);
    }

    // Empty so.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_eq(empty, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_eq(empty, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_eq(empty, x), is, yp_False));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_eq(empty, x), is, yp_True));

        // x is so.
        assert_obj(yp_eq(empty, empty), is, yp_True);
    }

    // XXX x cannot contain unhashable objects as eq is only supported on friendly types.

    // x is not an iterable.
    assert_obj(yp_eq(so, int_1), is, yp_False);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

static MunitResult test_ne(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = {type, type->pair, NULL};
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // ne is only supported for friendly x. All others compare unequal.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if ((*x_type) == type || (*x_type) == type->pair) continue;
        ead(x, (*x_type)->newN(0), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(0), assert_obj(yp_ne(empty, x), is, yp_True));
    }

    // Basic ne.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_ne(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_ne(so, x), is, yp_False));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_ne(so, x), is, yp_True));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_ne(so, x), is, yp_True));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_ne(so, x), is, yp_True));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[1], items[2])), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_ne(so, x), is, yp_True));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[2], items[3])), assert_obj(yp_ne(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[2], items[2])), assert_obj(yp_ne(so, x), is, yp_True));

        // x is so.
        assert_obj(yp_ne(so, so), is, yp_False);
    }

    // Empty so.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_ne(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_ne(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])), assert_obj(yp_ne(empty, x), is, yp_True));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_ne(empty, x), is, yp_False));

        // x is so.
        assert_obj(yp_ne(empty, empty), is, yp_False);
    }

    // XXX x cannot contain unhashable objects as ne is only supported on friendly types.

    // x is not an iterable.
    assert_obj(yp_ne(so, int_1), is, yp_True);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

// FIXME Move test_remove from test_frozenset here.


// TODO dict key and item views, when implemented, will also support the set protocol.
static MunitParameterEnum test_setlike_params[] = {
        {param_key_type, param_values_types_set}, {NULL}};

MunitTest test_setlike_tests[] = {TEST(test_isdisjoint, test_setlike_params),
        TEST(test_issubset, test_setlike_params), TEST(test_lt, test_setlike_params),
        TEST(test_le, test_setlike_params), TEST(test_eq, test_setlike_params),
        TEST(test_ne, test_setlike_params), {NULL}};

// FIXME The protocol and the object share the same name. Distinction between test_setlike and
// test_frozenset is flimsy. Rename?

extern void test_setlike_initialize(void) {}

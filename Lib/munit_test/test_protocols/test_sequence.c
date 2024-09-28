
#include "munit_test/unittest.h"

// TODO Is the behaviour of the comparison methods (yp_lt/etc) part of this interface?

// Sequences should accept themselves, their pairs, iterators, and tuple/list as valid types for the
// "x" (i.e. "other iterable") argument.
#define x_types_init(type) \
    {(type), (type)->pair, fixture_type_iter, fixture_type_tuple, fixture_type_list, NULL}

#define friend_types_init(type) {(type), (type)->pair, NULL}


typedef struct _slice_args_t {
    yp_ssize_t start;
    yp_ssize_t stop;
    yp_ssize_t step;
} slice_args_t;


static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = friend_types_init(type);
    fixture_type_t **x_type;
    ypObject        *not_iterable = rand_obj_any_not_iterable();
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // range stores integers following a pattern, so doesn't support concat.
    if (type->is_patterned) {
        ypObject *sq = rand_obj(type);
        assert_raises(yp_concat(sq, sq), yp_TypeError);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic concatenation.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[2], items[3]));
        ypObject *result = yp_concat(sq, x);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1], items[2], items[3]);
        assert_sequence(sq, items[0], items[1]);  // sq unchanged.
        yp_decrefN(N(sq, x, result));
    }

    // sq is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_concat(sq, x);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1]);
        yp_decrefN(N(sq, x, result));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_concat(sq, x);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1]);
        yp_decrefN(N(sq, x, result));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_concat(sq, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(sq, x, result));
    }

    // x is sq.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_concat(sq, sq);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1], items[0], items[1]);
        yp_decrefN(N(sq, result));
    }

    // x contains sq.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->is_string) {
            ypObject *sq = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(items[2], sq));
            if (type->is_string) {
                assert_raises(yp_concat(sq, x), yp_ValueError, yp_TypeError);
            } else {
                ypObject *result = yp_concat(sq, x);
                assert_type_is(result, type->type);
                assert_sequence(result, items[0], items[1], items[2], sq);
                yp_decref(result);
            }
            yp_decrefN(N(sq, x));
        }
    }

    // Duplicates: 0 is duplicated in sq, 1 in x, and 2 shared between them.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[2], items[0]));
        ypObject *x = (*x_type)->newN(N(items[2], items[1], items[1]));
        ypObject *result = yp_concat(sq, x);
        assert_sequence(result, items[0], items[2], items[0], items[2], items[1], items[1]);
        yp_decrefN(N(sq, x, result));
    }

    // Optimization: lazy shallow copy of an immutable sq when friendly x is empty.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_concat(sq, x);
        if (type->is_mutable) {
            assert_obj(sq, is_not, result);
        } else {
            assert_obj(sq, is, result);
        }
        yp_decrefN(N(sq, x, result));
    }

    // Optimization: lazy shallow copy of a friendly immutable x when immutable sq is empty.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *result = yp_concat(sq, x);
        if (type->is_mutable || (*x_type)->is_mutable) {
            assert_obj(x, is_not, result);
        } else {
            assert_obj(x, is, result);
        }
        yp_decrefN(N(sq, x, result));
    }

    // Optimization: empty immortal when immutable sq is empty and friendly x is empty.
    if (type->falsy != NULL) {
        for (x_type = friend_types; (*x_type) != NULL; x_type++) {
            ypObject *sq = type->newN(0);
            ypObject *x = (*x_type)->newN(0);
            ypObject *result = yp_concat(sq, x);
            assert_obj(result, is, type->falsy);
            yp_decrefN(N(sq, x, result));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject *sq = type->newN(N(items[0], items[1])); ypObject * result, x,
                      type->newN(N(items[2], items[3])), result = yp_concat(sq, x),
                      assert_sequence(result, items[0], items[1], items[2], items[3]),
                      yp_decrefN(N(sq, result)));

    // x is not an iterable.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises(yp_concat(sq, not_iterable), yp_TypeError);
        yp_decrefN(N(sq));
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception(yp_concat(sq, yp_SyntaxError), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq));
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static MunitResult test_repeatC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // range stores integers following a pattern, so doesn't support repeat.
    if (type->is_patterned) {
        ypObject *sq = rand_obj(type);
        assert_raises(yp_repeatC(sq, 2), yp_TypeError);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic repeat.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_repeatC(sq, 2);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1], items[0], items[1]);
        assert_sequence(sq, items[0], items[1]);  // sq unchanged.
        yp_decrefN(N(sq, result));
    }

    // Factor of one.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_repeatC(sq, 1);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1]);
        yp_decrefN(N(sq, result));
    }

    // Factor of zero.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_repeatC(sq, 0);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(sq, result));
    }

    // Negative factor.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_repeatC(sq, -1);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(sq, result));
    }

    // Large factor. (Exercises _ypSequence_repeat_memcpy optimization.)
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_repeatC(sq, 8);
        assert_type_is(result, type->type);
        assert_sequence(result, items[0], items[1], items[0], items[1], items[0], items[1],
                items[0], items[1], items[0], items[1], items[0], items[1], items[0], items[1],
                items[0], items[1]);
        yp_decrefN(N(sq, result));
    }

    // Empty sq.
    {
        ypObject *sq = type->newN(0);
        ypObject *result = yp_repeatC(sq, 2);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(N(sq, result));
    }

    // sq contains duplicates.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[0]));
        ypObject *result = yp_repeatC(sq, 2);
        assert_sequence(result, items[0], items[1], items[0], items[0], items[1], items[0]);
        yp_decrefN(N(sq, result));
    }

    // Optimization: lazy shallow copy of an immutable sq when factor is one.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *result = yp_repeatC(sq, 1);
        if (type->is_mutable) {
            assert_obj(sq, is_not, result);
        } else {
            assert_obj(sq, is, result);
        }
        yp_decrefN(N(sq, result));
    }

    // Optimization: empty immortal when immutable sq is empty.
    if (type->falsy != NULL) {
        ypObject *sq = type->newN(0);
        ypObject *result = yp_repeatC(sq, 2);
        assert_obj(result, is, type->falsy);
        yp_decrefN(N(sq, result));
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static void _test_getindexC(
        fixture_type_t *type, ypObject *(*any_getindexC)(ypObject *, yp_ssize_t))
{
    ypObject *items[2];
    ypObject *sq;
    ypObject *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    sq = type->newN(N(items[0], items[1]));

    // Basic index.
    ead(zero, any_getindexC(sq, 0), assert_obj(zero, eq, items[0]));
    ead(one, any_getindexC(sq, 1), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, any_getindexC(sq, -1), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, any_getindexC(sq, -2), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    assert_raises(any_getindexC(sq, 2), yp_IndexError);
    assert_raises(any_getindexC(sq, -3), yp_IndexError);

    // Previously-deleted index.
    if (type->is_mutable) {
        ypObject *sq_delitem = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_delindexC(sq_delitem, 1, &exc));
        assert_sequence(sq_delitem, items[0]);
        assert_raises(any_getindexC(sq_delitem, 1), yp_IndexError);
        assert_sequence(sq_delitem, items[0]);
        yp_decrefN(N(sq_delitem));
    }

    // Empty sq.
    assert_raises(any_getindexC(empty, 0), yp_IndexError);
    assert_raises(any_getindexC(empty, -1), yp_IndexError);

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    assert_raises(any_getindexC(sq, yp_SLICE_DEFAULT), yp_IndexError);
    assert_raises(any_getindexC(sq, yp_SLICE_LAST), yp_IndexError);

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ead(zero, any_getindexC(sq, 0), assert_obj(zero, is, items[0]));
        ead(one, any_getindexC(sq, 1), assert_obj(one, is, items[1]));
    }

    assert_sequence(sq, items[0], items[1]);  // sq unchanged.

    obj_array_decref(items);
    yp_decrefN(N(sq, empty));
}

static MunitResult test_getindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_getindexC(fixture->type, yp_getindexC);
    return MUNIT_OK;
}

static MunitResult test_getsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[5];
    ypObject       *sq;
    obj_array_fill(items, type->rand_items);
    sq = type->newN(N(items[0], items[1], items[2], items[3], items[4]));

    // Basic slice.
    {
        ypObject *zero_one = yp_getsliceC4(sq, 0, 1, 1);
        ypObject *one_two = yp_getsliceC4(sq, 1, 2, 1);
        assert_type_is(zero_one, type->type);
        assert_sequence(zero_one, items[0]);
        assert_type_is(one_two, type->type);
        assert_sequence(one_two, items[1]);
        assert_sequence(sq, items[0], items[1], items[2], items[3], items[4]);  // sq unchanged.
        yp_decrefN(N(zero_one, one_two));
    }

    // Negative step.
    {
        ypObject *neg_one_neg_two = yp_getsliceC4(sq, -1, -2, -1);
        ypObject *neg_two_neg_three = yp_getsliceC4(sq, -2, -3, -1);
        assert_type_is(neg_one_neg_two, type->type);
        assert_sequence(neg_one_neg_two, items[4]);
        assert_type_is(neg_two_neg_three, type->type);
        assert_sequence(neg_two_neg_three, items[3]);
        yp_decrefN(N(neg_one_neg_two, neg_two_neg_three));
    }

    // Total slice, forward and backward.
    {
        ypObject *forward = yp_getsliceC4(sq, 0, 5, 1);
        ypObject *reverse = yp_getsliceC4(sq, -1, -6, -1);
        assert_type_is(forward, type->type);
        assert_sequence(forward, items[0], items[1], items[2], items[3], items[4]);
        assert_type_is(reverse, type->type);
        assert_sequence(reverse, items[4], items[3], items[2], items[1], items[0]);
        yp_decrefN(N(forward, reverse));
    }

    // Step of 2, -2.
    {
        ypObject *forward = yp_getsliceC4(sq, 0, 5, 2);
        ypObject *reverse = yp_getsliceC4(sq, -1, -6, -2);
        assert_type_is(forward, type->type);
        assert_sequence(forward, items[0], items[2], items[4]);
        assert_type_is(reverse, type->type);
        assert_sequence(reverse, items[4], items[2], items[0]);
        yp_decrefN(N(forward, reverse));
    }

    // Empty slices.
    {
        slice_args_t slices[] = {
                {0, 0, 1},      // typical empty slice
                {5, 99, 1},     // i>=len(s) and k>0 (regardless of j)
                {-6, -99, -1},  // i<-len(s) and k<0 (regardless of j)
                {99, 5, -1},    // j>=len(s) and k<0 (regardless of i)
                {-99, -6, 1},   // j<-len(s) and k>0 (regardless of i)
                {4, 4, 1},      // i=j (regardless of k)
                {1, 0, 1},      // i>j and k>0
                {0, 1, -1},     // i<j and k<0
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            ypObject    *empty = yp_getsliceC4(sq, args.start, args.stop, args.step);
            assert_type_is(empty, type->type);
            assert_len(empty, 0);
            yp_decref(empty);
        }
    }

    // yp_SLICE_DEFAULT.
    ead(i_pos_step, yp_getsliceC4(sq, yp_SLICE_DEFAULT, 2, 1),
            assert_sequence(i_pos_step, items[0], items[1]));
    ead(j_pos_step, yp_getsliceC4(sq, 2, yp_SLICE_DEFAULT, 1),
            assert_sequence(j_pos_step, items[2], items[3], items[4]));
    ead(both_pos_step, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1),
            assert_sequence(both_pos_step, items[0], items[1], items[2], items[3], items[4]));
    ead(i_neg_step, yp_getsliceC4(sq, yp_SLICE_DEFAULT, -3, -1),
            assert_sequence(i_neg_step, items[4], items[3]));
    ead(j_neg_step, yp_getsliceC4(sq, -3, yp_SLICE_DEFAULT, -1),
            assert_sequence(j_neg_step, items[2], items[1], items[0]));
    ead(both_neg_step, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1),
            assert_sequence(both_neg_step, items[4], items[3], items[2], items[1], items[0]));

    // yp_SLICE_LAST.
    ead(i_pos_step, yp_getsliceC4(sq, yp_SLICE_LAST, 5, 1), assert_len(i_pos_step, 0));
    ead(j_pos_step, yp_getsliceC4(sq, 0, yp_SLICE_LAST, 1),
            assert_sequence(j_pos_step, items[0], items[1], items[2], items[3], items[4]));
    ead(both_pos_step, yp_getsliceC4(sq, yp_SLICE_LAST, yp_SLICE_LAST, 1),
            assert_len(both_pos_step, 0));
    ead(i_neg_step, yp_getsliceC4(sq, yp_SLICE_LAST, -6, -1),
            assert_sequence(i_neg_step, items[4], items[3], items[2], items[1], items[0]));
    ead(j_neg_step, yp_getsliceC4(sq, -1, yp_SLICE_LAST, -1), assert_len(j_neg_step, 0));
    ead(both_neg_step, yp_getsliceC4(sq, yp_SLICE_LAST, yp_SLICE_LAST, -1),
            assert_len(both_neg_step, 0));

    // Invalid slices.
    assert_raises(yp_getsliceC4(sq, 0, 1, 0), yp_ValueError);  // step==0
    assert_raises(yp_getsliceC4(sq, 0, 1, -yp_SSIZE_T_MAX - 1),
            yp_SystemLimitationError);  // too-small step

    // Slice contains duplicates.
    if (!type->is_patterned) {
        ypObject *dups = type->newN(N(items[0], items[1], items[2], items[2], items[0]));
        ypObject *slice = yp_getsliceC4(dups, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 2);
        assert_sequence(slice, items[0], items[2], items[0]);
        yp_decrefN(N(dups, slice));
    }

    // Optimization: lazy shallow copy of an immutable sq for total forward slice.
    if (!type->is_mutable) {
        ead(forward, yp_getsliceC4(sq, 0, 5, 1), assert_obj(forward, is, sq));
    }

    // Optimization: empty immortal when slice is empty.
    if (type->falsy != NULL) {
        ead(empty, yp_getsliceC4(sq, 0, 0, 1), assert_obj(empty, is, type->falsy));
    }

    // Python's test_getslice.
    ead(slice, yp_getsliceC4(sq, 0, 0, 1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(sq, 1, 2, 1), assert_sequence(slice, items[1]));
    ead(slice, yp_getsliceC4(sq, -2, -1, 1), assert_sequence(slice, items[3]));
    ead(slice, yp_getsliceC4(sq, -1000, 1000, 1), assert_obj(slice, eq, sq));
    ead(slice, yp_getsliceC4(sq, 1000, -1000, 1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1), assert_obj(slice, eq, sq));
    ead(slice, yp_getsliceC4(sq, 1, yp_SLICE_DEFAULT, 1),
            assert_sequence(slice, items[1], items[2], items[3], items[4]));
    ead(slice, yp_getsliceC4(sq, yp_SLICE_DEFAULT, 3, 1),
            assert_sequence(slice, items[0], items[1], items[2]));

    ead(slice, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 2),
            assert_sequence(slice, items[0], items[2], items[4]));
    ead(slice, yp_getsliceC4(sq, 1, yp_SLICE_DEFAULT, 2),
            assert_sequence(slice, items[1], items[3]));
    ead(slice, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1),
            assert_sequence(slice, items[4], items[3], items[2], items[1], items[0]));
    ead(slice, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -2),
            assert_sequence(slice, items[4], items[2], items[0]));
    ead(slice, yp_getsliceC4(sq, 3, yp_SLICE_DEFAULT, -2),
            assert_sequence(slice, items[3], items[1]));
    ead(slice, yp_getsliceC4(sq, 3, 3, -2), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(sq, 3, 2, -2), assert_sequence(slice, items[3]));
    ead(slice, yp_getsliceC4(sq, 3, 1, -2), assert_sequence(slice, items[3]));
    ead(slice, yp_getsliceC4(sq, 3, 0, -2), assert_sequence(slice, items[3], items[1]));
    ead(slice, yp_getsliceC4(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -100),
            assert_sequence(slice, items[4]));
    ead(slice, yp_getsliceC4(sq, 100, -100, 1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(sq, -100, 100, 1), assert_obj(slice, eq, sq));
    ead(slice, yp_getsliceC4(sq, 100, -100, -1),
            assert_sequence(slice, items[4], items[3], items[2], items[1], items[0]));
    ead(slice, yp_getsliceC4(sq, -100, 100, -1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(sq, -100, 100, 2),
            assert_sequence(slice, items[0], items[2], items[4]));

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *zero_one = yp_getsliceC4(sq, 0, 1, 1);
        ead(zero, yp_getindexC(zero_one, 0), assert_obj(zero, is, items[0]));
        yp_decrefN(N(zero_one));
    }

    assert_sequence(sq, items[0], items[1], items[2], items[3], items[4]);  // sq unchanged.

    yp_decref(sq);
    obj_array_decref(items);
    return MUNIT_OK;
}

static ypObject *getindexC_to_getitem(ypObject *sq, yp_ssize_t i)
{
    ypObject *int_i = yp_intC(i);
    ypObject *result = yp_getitem(sq, int_i);
    yp_decref(int_i);
    return result;
}

static MunitResult test_getitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *intstore_0 = yp_intstoreC(0);
    ypObject       *items[2];
    ypObject       *sq;
    ypObject       *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    sq = type->newN(N(items[0], items[1]));

    // Shared tests.
    _test_getindexC(type, getindexC_to_getitem);

    // intstore.
    ead(zero, yp_getitem(sq, intstore_0), assert_obj(zero, eq, items[0]));

    // Index is sq.
    assert_raises(yp_getitem(sq, sq), yp_TypeError);

    // Exception passthrough.
    assert_isexception(yp_getitem(sq, yp_SyntaxError), yp_SyntaxError);
    assert_isexception(yp_getitem(empty, yp_SyntaxError), yp_SyntaxError);

    assert_sequence(sq, items[0], items[1]);  // sq unchanged.

    obj_array_decref(items);
    yp_decrefN(N(sq, empty, intstore_0));
    return MUNIT_OK;
}

static MunitResult test_getdefault(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *int_0 = yp_intC(0);
    ypObject       *int_1 = yp_intC(1);
    ypObject       *int_2 = yp_intC(2);
    ypObject       *int_neg_1 = yp_intC(-1);
    ypObject       *int_neg_2 = yp_intC(-2);
    ypObject       *int_neg_3 = yp_intC(-3);
    ypObject       *int_SLICE_DEFAULT = yp_intC(yp_SLICE_DEFAULT);
    ypObject       *int_SLICE_LAST = yp_intC(yp_SLICE_LAST);
    ypObject       *intstore_0 = yp_intstoreC(0);
    ypObject       *items[3];
    ypObject       *sq;
    ypObject       *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    sq = type->newN(N(items[0], items[1]));

    // Basic index.
    ead(zero, yp_getdefault(sq, int_0, items[2]), assert_obj(zero, eq, items[0]));
    ead(one, yp_getdefault(sq, int_1, items[2]), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, yp_getdefault(sq, int_neg_1, items[2]), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, yp_getdefault(sq, int_neg_2, items[2]), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    ead(two, yp_getdefault(sq, int_2, items[2]), assert_obj(two, eq, items[2]));
    ead(neg_three, yp_getdefault(sq, int_neg_3, items[2]), assert_obj(neg_three, eq, items[2]));

    // Exception-as-default.
    assert_raises(yp_getdefault(sq, int_2, yp_SyntaxError), yp_SyntaxError);
    assert_raises(yp_getdefault(sq, int_neg_3, yp_SyntaxError), yp_SyntaxError);

    // Previously-deleted index.
    if (type->is_mutable) {
        ypObject *sq_delitem = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_delitem(sq_delitem, int_1, &exc));
        assert_sequence(sq_delitem, items[0]);
        ead(two, yp_getdefault(sq_delitem, int_1, items[2]), assert_obj(two, eq, items[2]));
        assert_sequence(sq_delitem, items[0]);
        yp_decrefN(N(sq_delitem));
    }

    // Empty sq.
    ead(zero, yp_getdefault(empty, int_0, items[2]), assert_obj(zero, eq, items[2]));
    ead(neg_one, yp_getdefault(empty, int_neg_1, items[2]), assert_obj(neg_one, eq, items[2]));

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    ead(slice_default, yp_getdefault(sq, int_SLICE_DEFAULT, items[2]),
            assert_obj(slice_default, eq, items[2]));
    ead(slice_last, yp_getdefault(sq, int_SLICE_LAST, items[2]),
            assert_obj(slice_last, eq, items[2]));

    // intstore.
    ead(zero, yp_getdefault(sq, intstore_0, items[2]), assert_obj(zero, eq, items[0]));

    // Index is sq.
    assert_raises(yp_getdefault(sq, sq, items[2]), yp_TypeError);

    // Default is sq.
    ead(zero, yp_getdefault(sq, int_0, sq), assert_obj(zero, eq, items[0]));
    ead(two, yp_getdefault(sq, int_2, sq), assert_obj(two, eq, sq));

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ead(zero, yp_getdefault(sq, int_0, items[2]), assert_obj(zero, is, items[0]));
        ead(two, yp_getdefault(sq, int_2, items[2]), assert_obj(two, is, items[2]));
    }

    // Exception passthrough.
    assert_isexception(yp_getdefault(sq, yp_SyntaxError, items[2]), yp_SyntaxError);
    assert_isexception(yp_getdefault(empty, yp_SyntaxError, items[2]), yp_SyntaxError);

    assert_sequence(sq, items[0], items[1]);  // sq unchanged.

    obj_array_decref(items);
    yp_decrefN(N(sq, empty, int_0, int_1, int_2, int_neg_1, int_neg_2, int_neg_3, int_SLICE_DEFAULT,
            int_SLICE_LAST, intstore_0));
    return MUNIT_OK;
}

// Shared tests for yp_findC5, yp_indexC5, yp_rfindC5, yp_rindexC5, etc.
static void _test_findC(fixture_type_t *type,
        yp_ssize_t (*any_findC)(ypObject *, ypObject *, ypObject **),
        yp_ssize_t (*any_findC5)(ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, ypObject **),
        int forward, int raises)
{
    ypObject *items[3];
    ypObject *sq;
    ypObject *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    sq = type->newN(N(items[0], items[1]));

#define assert_not_found_exc(expression)            \
    do {                                            \
        ypObject *exc = yp_None;                    \
        assert_ssizeC(expression, ==, -1);          \
        if (raises) {                               \
            assert_isexception(exc, yp_ValueError); \
        } else {                                    \
            assert_obj(exc, is, yp_None);           \
        }                                           \
    } while (0)

    // Basic find.
    assert_ssizeC_exc(any_findC(sq, items[0], &exc), ==, 0);
    assert_ssizeC_exc(any_findC(sq, items[1], &exc), ==, 1);

    // Not in sequence.
    assert_not_found_exc(any_findC(sq, items[2], &exc));

    // Empty sq.
    assert_not_found_exc(any_findC(empty, items[0], &exc));

    // Basic slice.
    assert_ssizeC_exc(any_findC5(sq, items[0], 0, 1, &exc), ==, 0);
    assert_not_found_exc(any_findC5(sq, items[0], 1, 2, &exc));
    assert_not_found_exc(any_findC5(sq, items[1], 0, 1, &exc));
    assert_ssizeC_exc(any_findC5(sq, items[1], 1, 2, &exc), ==, 1);

    // Negative indicies.
    assert_ssizeC_exc(any_findC5(sq, items[0], -2, -1, &exc), ==, 0);
    assert_not_found_exc(any_findC5(sq, items[0], -1, 2, &exc));
    assert_not_found_exc(any_findC5(sq, items[1], -2, -1, &exc));
    assert_ssizeC_exc(any_findC5(sq, items[1], -1, 2, &exc), ==, 1);

    // Total slice.
    assert_ssizeC_exc(any_findC5(sq, items[0], 0, 2, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(sq, items[1], 0, 2, &exc), ==, 1);
    assert_not_found_exc(any_findC5(sq, items[2], 0, 2, &exc));

    // Total slice, negative indicies.
    assert_ssizeC_exc(any_findC5(sq, items[0], -2, 2, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(sq, items[1], -2, 2, &exc), ==, 1);
    assert_not_found_exc(any_findC5(sq, items[2], -2, 2, &exc));

    // Empty slices.
    {
        slice_args_t slices[] = {
                // recall step is always 1 for find
                {0, 0, 1},     // typical empty slice
                {2, 99, 1},    // i>=len(s) and k>0 (regardless of j)
                {-99, -3, 1},  // j<-len(s) and k>0 (regardless of i)
                {2, 2, 1},     // i=j (regardless of k)
                {1, 0, 1},     // i>j and k>0
                {-1, -3, 1},   // reverse total slice...but k is always 1
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            assert_not_found_exc(any_findC5(sq, items[0], args.start, args.stop, &exc));
            assert_not_found_exc(any_findC5(sq, items[1], args.start, args.stop, &exc));
            assert_not_found_exc(any_findC5(sq, items[2], args.start, args.stop, &exc));
        }
    }

    // yp_SLICE_DEFAULT.
    assert_ssizeC_exc(any_findC5(sq, items[0], yp_SLICE_DEFAULT, 1, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(sq, items[0], 0, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(sq, items[0], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(sq, items[1], yp_SLICE_DEFAULT, 2, &exc), ==, 1);
    assert_ssizeC_exc(any_findC5(sq, items[1], 1, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(any_findC5(sq, items[1], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_not_found_exc(any_findC5(sq, items[2], yp_SLICE_DEFAULT, 2, &exc));
    assert_not_found_exc(any_findC5(sq, items[2], 0, yp_SLICE_DEFAULT, &exc));
    assert_not_found_exc(any_findC5(sq, items[2], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc));

    // yp_SLICE_LAST.
    assert_not_found_exc(any_findC5(sq, items[0], yp_SLICE_LAST, 2, &exc));
    assert_ssizeC_exc(any_findC5(sq, items[0], 0, yp_SLICE_LAST, &exc), ==, 0);
    assert_not_found_exc(any_findC5(sq, items[0], yp_SLICE_LAST, yp_SLICE_LAST, &exc));
    assert_not_found_exc(any_findC5(sq, items[1], yp_SLICE_LAST, 2, &exc));
    assert_ssizeC_exc(any_findC5(sq, items[1], 1, yp_SLICE_LAST, &exc), ==, 1);
    assert_not_found_exc(any_findC5(sq, items[1], yp_SLICE_LAST, yp_SLICE_LAST, &exc));
    assert_not_found_exc(any_findC5(sq, items[2], yp_SLICE_LAST, 2, &exc));
    assert_not_found_exc(any_findC5(sq, items[2], 0, yp_SLICE_LAST, &exc));
    assert_not_found_exc(any_findC5(sq, items[2], yp_SLICE_LAST, yp_SLICE_LAST, &exc));

    // x is sq; sq not in sq. Recall `"abc" in "abc"` is True for strings.
    if (type->is_string) {
        assert_ssizeC_exc(any_findC(sq, sq, &exc), ==, 0);
        assert_ssizeC_exc(any_findC5(sq, sq, 0, 3, &exc), ==, 0);
    } else {
        assert_not_found_exc(any_findC(sq, sq, &exc));
        assert_not_found_exc(any_findC5(sq, sq, 0, 3, &exc));
    }

    // x is sq; sq contains sq.
    if (!type->is_string && type->is_mutable) {
        ypObject *sq_sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_insertC(sq_sq, 1, sq_sq, &exc));
        assert_ssizeC_exc(any_findC(sq_sq, sq_sq, &exc), ==, 1);
        assert_ssizeC_exc(any_findC5(sq_sq, sq_sq, 0, 3, &exc), ==, 1);
        assert_not_raises_exc(yp_clear(sq_sq, &exc));  // nohtyP does not yet break circular refs.
        yp_decrefN(N(sq_sq));
    }

    // If duplicates, which one is found depends on the direction. Recall patterned sequences like
    // range don't store duplicates.
    if (!type->is_patterned) {
        ypObject *dups = type->newN(N(items[2], items[2], items[2]));
        assert_ssizeC_exc(any_findC(dups, items[2], &exc), ==, forward ? 0 : 2);
        assert_ssizeC_exc(any_findC5(dups, items[2], 0, 2, &exc), ==, forward ? 0 : 1);    // Basic.
        assert_ssizeC_exc(any_findC5(dups, items[2], 1, 3, &exc), ==, forward ? 1 : 2);    // Basic.
        assert_ssizeC_exc(any_findC5(dups, items[2], -3, -1, &exc), ==, forward ? 0 : 1);  // Neg.
        assert_ssizeC_exc(any_findC5(dups, items[2], -2, 3, &exc), ==, forward ? 1 : 2);   // Neg.
        assert_ssizeC_exc(any_findC5(dups, items[2], 0, 3, &exc), ==, forward ? 0 : 2);    // Total.
        assert_ssizeC_exc(any_findC5(dups, items[2], -3, 3, &exc), ==, forward ? 0 : 2);   // Total.
        yp_decref(dups);
    }

    // For strings, find looks for sub-sequences of items; all other sequences inspect only one item
    // at a time. This is tested more thoroughly in test_string.
    {
        ypObject *sq_0_1_2 = type->newN(N(items[0], items[1], items[2]));
        ypObject *sq_0_1 = type->newN(N(items[0], items[1]));
        assert_obj(sq_0_1, ne, items[2]);  // ensure sq_0_1 isn't actually an item in sq_0_1_2
        if (type->is_string) {
            assert_ssizeC_exc(any_findC(sq_0_1_2, sq_0_1, &exc), ==, 0);
            assert_ssizeC_exc(any_findC5(sq_0_1_2, sq_0_1, 0, 3, &exc), ==, 0);
        } else {
            assert_not_found_exc(any_findC(sq_0_1_2, sq_0_1, &exc));
            assert_not_found_exc(any_findC5(sq_0_1_2, sq_0_1, 0, 3, &exc));
        }
        yp_decrefN(N(sq_0_1_2, sq_0_1));
    }

    // Exception passthrough.
    assert_isexception_exc(any_findC(sq, yp_SyntaxError, &exc), yp_SyntaxError);
    assert_isexception_exc(any_findC5(sq, yp_SyntaxError, 0, 1, &exc), yp_SyntaxError);
    assert_isexception_exc(any_findC5(sq, yp_SyntaxError, 0, 0, &exc), yp_SyntaxError);
    assert_isexception_exc(any_findC(empty, yp_SyntaxError, &exc), yp_SyntaxError);
    assert_isexception_exc(any_findC5(empty, yp_SyntaxError, 0, 1, &exc), yp_SyntaxError);
    assert_isexception_exc(any_findC5(empty, yp_SyntaxError, 0, 0, &exc), yp_SyntaxError);

    assert_sequence(sq, items[0], items[1]);  // sq unchanged.

#undef assert_not_found_exc

    obj_array_decref(items);
    yp_decrefN(N(sq, empty));
}

static MunitResult test_findC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_findC, yp_findC5, /*forward=*/TRUE, /*raises=*/FALSE);
    return MUNIT_OK;
}

static MunitResult test_indexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_indexC, yp_indexC5, /*forward=*/TRUE, /*raises=*/TRUE);
    return MUNIT_OK;
}

static MunitResult test_rfindC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_rfindC, yp_rfindC5, /*forward=*/FALSE, /*raises=*/FALSE);
    return MUNIT_OK;
}

static MunitResult test_rindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_rindexC, yp_rindexC5, /*forward=*/FALSE, /*raises=*/TRUE);
    return MUNIT_OK;
}

static MunitResult test_countC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[3];
    ypObject       *sq;
    ypObject       *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    sq = type->newN(N(items[0], items[1]));

    // Basic count.
    assert_ssizeC_exc(yp_countC(sq, items[0], &exc), ==, 1);
    assert_ssizeC_exc(yp_countC(sq, items[1], &exc), ==, 1);

    // Not in sequence.
    assert_ssizeC_exc(yp_countC(sq, items[2], &exc), ==, 0);

    // Empty sq.
    assert_ssizeC_exc(yp_countC(empty, items[0], &exc), ==, 0);

    // Basic slice.
    assert_ssizeC_exc(yp_countC5(sq, items[0], 0, 1, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[0], 1, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[1], 0, 1, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[1], 1, 2, &exc), ==, 1);

    // Negative indicies.
    assert_ssizeC_exc(yp_countC5(sq, items[0], -2, -1, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[0], -1, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[1], -2, -1, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[1], -1, 2, &exc), ==, 1);

    // Total slice.
    assert_ssizeC_exc(yp_countC5(sq, items[0], 0, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[1], 0, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[2], 0, 2, &exc), ==, 0);

    // Total slice, negative indicies.
    assert_ssizeC_exc(yp_countC5(sq, items[0], -2, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[1], -2, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[2], -2, 2, &exc), ==, 0);

    // Empty slices.
    {
        slice_args_t slices[] = {
                // recall step is always 1 for count
                {0, 0, 1},     // typical empty slice
                {2, 99, 1},    // i>=len(s) and k>0 (regardless of j)
                {-99, -3, 1},  // j<-len(s) and k>0 (regardless of i)
                {2, 2, 1},     // i=j (regardless of k)
                {1, 0, 1},     // i>j and k>0
                {-1, -3, 1},   // reverse total slice...but k is always 1
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            assert_ssizeC_exc(yp_countC5(sq, items[0], args.start, args.stop, &exc), ==, 0);
            assert_ssizeC_exc(yp_countC5(sq, items[1], args.start, args.stop, &exc), ==, 0);
            assert_ssizeC_exc(yp_countC5(sq, items[2], args.start, args.stop, &exc), ==, 0);
        }
    }

    // yp_SLICE_DEFAULT.
    assert_ssizeC_exc(yp_countC5(sq, items[0], yp_SLICE_DEFAULT, 1, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[0], 1, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[0], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[1], yp_SLICE_DEFAULT, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[1], 1, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[1], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[2], yp_SLICE_DEFAULT, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[2], 0, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[2], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 0);

    // yp_SLICE_LAST.
    assert_ssizeC_exc(yp_countC5(sq, items[0], yp_SLICE_LAST, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[0], 0, yp_SLICE_LAST, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[0], yp_SLICE_LAST, yp_SLICE_LAST, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[1], yp_SLICE_LAST, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[1], 1, yp_SLICE_LAST, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(sq, items[1], yp_SLICE_LAST, yp_SLICE_LAST, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[2], yp_SLICE_LAST, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[2], 0, yp_SLICE_LAST, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(sq, items[2], yp_SLICE_LAST, yp_SLICE_LAST, &exc), ==, 0);

    // x is sq; sq not in sq. Recall `"abc" in "abc"` is True for strings.
    assert_ssizeC_exc(yp_countC(sq, sq, &exc), ==, type->is_string ? 1 : 0);
    assert_ssizeC_exc(yp_countC5(sq, sq, 0, 3, &exc), ==, type->is_string ? 1 : 0);

    // x is sq, sq contains sq.
    if (!type->is_string && type->is_mutable) {
        ypObject *sq_sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_insertC(sq_sq, 1, sq_sq, &exc));
        assert_ssizeC_exc(yp_countC(sq_sq, sq_sq, &exc), ==, 1);
        assert_ssizeC_exc(yp_countC5(sq_sq, sq_sq, 0, 3, &exc), ==, 1);
        assert_not_raises_exc(yp_clear(sq_sq, &exc));  // nohtyP does not yet break circular refs.
        yp_decrefN(N(sq_sq));
    }

    // Recall patterned sequences like range don't store duplicates.
    if (!type->is_patterned) {
        ypObject *dups = type->newN(N(items[2], items[2], items[2]));
        assert_ssizeC_exc(yp_countC(dups, items[2], &exc), ==, 3);
        assert_ssizeC_exc(yp_countC5(dups, items[2], 0, 1, &exc), ==, 1);    // Basic.
        assert_ssizeC_exc(yp_countC5(dups, items[2], 0, 2, &exc), ==, 2);    // Basic.
        assert_ssizeC_exc(yp_countC5(dups, items[2], 1, 3, &exc), ==, 2);    // Basic.
        assert_ssizeC_exc(yp_countC5(dups, items[2], -3, -2, &exc), ==, 1);  // Neg.
        assert_ssizeC_exc(yp_countC5(dups, items[2], -3, -1, &exc), ==, 2);  // Neg.
        assert_ssizeC_exc(yp_countC5(dups, items[2], -2, 3, &exc), ==, 2);   // Neg.
        assert_ssizeC_exc(yp_countC5(dups, items[2], 0, 3, &exc), ==, 3);    // Total.
        assert_ssizeC_exc(yp_countC5(dups, items[2], -3, 3, &exc), ==, 3);   // Total.
        yp_decref(dups);
    }

    // For strings, count looks for non-overlapping sub-sequences of items; all other sequences
    // count only one item at a time. This is tested more thoroughly in test_string.
    {
        ypObject *sq_0_1_2 = type->newN(N(items[0], items[1], items[2]));
        ypObject *sq_0_1 = type->newN(N(items[0], items[1]));
        assert_obj(sq_0_1, ne, items[2]);  // ensure sq_0_1 isn't actually an item in sq_0_1_2
        assert_ssizeC_exc(yp_countC(sq_0_1_2, sq_0_1, &exc), ==, type->is_string ? 1 : 0);
        assert_ssizeC_exc(yp_countC5(sq_0_1_2, sq_0_1, 0, 3, &exc), ==, type->is_string ? 1 : 0);
        yp_decrefN(N(sq_0_1_2, sq_0_1));
    }

    // Exception passthrough.
    assert_isexception_exc(yp_countC(sq, yp_SyntaxError, &exc), yp_SyntaxError);
    assert_isexception_exc(yp_countC5(sq, yp_SyntaxError, 0, 1, &exc), yp_SyntaxError);
    assert_isexception_exc(yp_countC5(sq, yp_SyntaxError, 0, 0, &exc), yp_SyntaxError);
    assert_isexception_exc(yp_countC(empty, yp_SyntaxError, &exc), yp_SyntaxError);
    assert_isexception_exc(yp_countC5(empty, yp_SyntaxError, 0, 1, &exc), yp_SyntaxError);
    assert_isexception_exc(yp_countC5(empty, yp_SyntaxError, 0, 0, &exc), yp_SyntaxError);

    assert_sequence(sq, items[0], items[1]);  // sq unchanged.

    obj_array_decref(items);
    yp_decrefN(N(sq, empty));
    return MUNIT_OK;
}

static void _test_setindexC(fixture_type_t *type,
        void (*any_setindexC)(ypObject *, yp_ssize_t, ypObject *, ypObject **))
{
    ypObject *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support setindex.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_setindexC(sq, 0, items[2], &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic index.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(any_setindexC(sq, 0, items[2], &exc));
        assert_sequence(sq, items[2], items[1]);
        assert_not_raises_exc(any_setindexC(sq, 1, items[3], &exc));
        assert_sequence(sq, items[2], items[3]);
        yp_decref(sq);
    }

    // Negative index.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(any_setindexC(sq, -1, items[2], &exc));
        assert_sequence(sq, items[0], items[2]);
        assert_not_raises_exc(any_setindexC(sq, -2, items[3], &exc));
        assert_sequence(sq, items[3], items[2]);
        yp_decref(sq);
    }

    // Out of bounds.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_setindexC(sq, 2, items[2], &exc), yp_IndexError);
        assert_raises_exc(any_setindexC(sq, -3, items[3], &exc), yp_IndexError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Previously-deleted index.
    {
        ypObject *sq_delitem = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_delindexC(sq_delitem, 1, &exc));
        assert_sequence(sq_delitem, items[0]);
        assert_raises_exc(any_setindexC(sq_delitem, 1, items[2], &exc), yp_IndexError);
        assert_sequence(sq_delitem, items[0]);
        yp_decrefN(N(sq_delitem));
    }

    // Empty sq.
    {
        ypObject *empty = type->newN(0);
        assert_raises_exc(any_setindexC(empty, 0, items[2], &exc), yp_IndexError);
        assert_raises_exc(any_setindexC(empty, -1, items[3], &exc), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_setindexC(sq, yp_SLICE_DEFAULT, items[2], &exc), yp_IndexError);
        assert_raises_exc(any_setindexC(sq, yp_SLICE_LAST, items[3], &exc), yp_IndexError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // x is sq. Recall strings are restrictive about the objects they accept.
    // TODO Add similar tests to test_string, where sq is 0, 1, and 2 items.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        if (type->is_string) {
            assert_raises_exc(any_setindexC(sq, 0, sq, &exc), yp_ValueError, yp_TypeError);
            assert_sequence(sq, items[0], items[1]);
        } else {
            assert_not_raises_exc(any_setindexC(sq, 0, sq, &exc));
            assert_sequence(sq, sq, items[1]);
            assert_not_raises_exc(yp_clear(sq, &exc));  // nohtyP does not yet break circular refs.
        }
        yp_decref(sq);
    }

    // Duplicates: 0 is duplicated in sq, 1 shared between them.
    {
        ypObject *sq = type->newN(N(items[0], items[0], items[1], items[2]));
        assert_not_raises_exc(any_setindexC(sq, 3, items[1], &exc));
        assert_sequence(sq, items[0], items[0], items[1], items[1]);
        yp_decref(sq);
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(any_setindexC(sq, 0, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
}

static MunitResult test_setindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_setindexC(fixture->type, yp_setindexC);
    return MUNIT_OK;
}

static MunitResult test_setsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    ypObject        *not_iterable = rand_obj_any_not_iterable();
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[11];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support setslice.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *two = type->newN(N(items[2]));
        assert_raises_exc(yp_setsliceC6(sq, 0, 1, 1, two, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, two));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic slice.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *two = (*x_type)->newN(N(items[2]));
        ypObject *three = (*x_type)->newN(N(items[3]));
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 1, 1, two, &exc));
        assert_sequence(sq, items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, 2, 1, three, &exc));
        assert_sequence(sq, items[2], items[3]);
        yp_decrefN(N(sq, two, three));
    }

    // Negative step.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *two = (*x_type)->newN(N(items[2]));
        ypObject *three = (*x_type)->newN(N(items[3]));
        assert_not_raises_exc(yp_setsliceC6(sq, -1, -2, -1, two, &exc));
        assert_sequence(sq, items[0], items[2]);
        assert_not_raises_exc(yp_setsliceC6(sq, -2, -3, -1, three, &exc));
        assert_sequence(sq, items[3], items[2]);
        yp_decrefN(N(sq, two, three));
    }

    // Total slice, forward and backward.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *four_five = (*x_type)->newN(N(items[4], items[5]));
        ypObject *six_seven = (*x_type)->newN(N(items[6], items[7]));
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 2, 1, four_five, &exc));
        assert_sequence(sq, items[4], items[5]);
        assert_not_raises_exc(yp_setsliceC6(sq, -1, -3, -1, six_seven, &exc));
        assert_sequence(sq, items[7], items[6]);
        yp_decrefN(N(sq, four_five, six_seven));
    }

    // Step of 2, -2.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1], items[2], items[3], items[4]));
        ypObject *five_six_seven = (*x_type)->newN(N(items[5], items[6], items[7]));
        ypObject *eight_nine_ten = (*x_type)->newN(N(items[8], items[9], items[10]));
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 5, 2, five_six_seven, &exc));
        assert_sequence(sq, items[5], items[1], items[6], items[3], items[7]);
        assert_not_raises_exc(yp_setsliceC6(sq, -1, -6, -2, eight_nine_ten, &exc));
        assert_sequence(sq, items[10], items[1], items[9], items[3], items[8]);
        yp_decrefN(N(sq, five_six_seven, eight_nine_ten));
    }

    // Empty slices.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject    *sq = type->newN(N(items[0], items[1]));
        ypObject    *empty = (*x_type)->newN(0);
        slice_args_t slices[] = {
                {0, 0, 1},      // typical empty slice
                {5, 99, 1},     // i>=len(s) and k>0 (regardless of j)
                {-6, -99, -1},  // i<-len(s) and k<0 (regardless of j)
                {99, 5, -1},    // j>=len(s) and k<0 (regardless of i)
                {-99, -6, 1},   // j<-len(s) and k>0 (regardless of i)
                {4, 4, 1},      // i=j (regardless of k)
                {1, 0, 1},      // i>j and k>0
                {0, 1, -1},     // i<j and k<0
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            assert_not_raises_exc(yp_setsliceC6(sq, args.start, args.stop, args.step, empty, &exc));
            assert_sequence(sq, items[0], items[1]);
        }
        yp_decrefN(N(sq, empty));
    }

    // yp_SLICE_DEFAULT.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *two = (*x_type)->newN(N(items[2]));
        ypObject *three = (*x_type)->newN(N(items[3]));
        ypObject *four_five = (*x_type)->newN(N(items[4], items[5]));
        ypObject *six = (*x_type)->newN(N(items[6]));
        ypObject *seven = (*x_type)->newN(N(items[7]));
        ypObject *eight_nine = (*x_type)->newN(N(items[8], items[9]));
        assert_not_raises_exc(yp_setsliceC6(sq, yp_SLICE_DEFAULT, 1, 1, two, &exc));
        assert_sequence(sq, items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, yp_SLICE_DEFAULT, 1, three, &exc));
        assert_sequence(sq, items[2], items[3]);
        assert_not_raises_exc(
                yp_setsliceC6(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1, four_five, &exc));
        assert_sequence(sq, items[4], items[5]);
        assert_not_raises_exc(yp_setsliceC6(sq, yp_SLICE_DEFAULT, -2, -1, six, &exc));
        assert_sequence(sq, items[4], items[6]);
        assert_not_raises_exc(yp_setsliceC6(sq, -2, yp_SLICE_DEFAULT, -1, seven, &exc));
        assert_sequence(sq, items[7], items[6]);
        assert_not_raises_exc(
                yp_setsliceC6(sq, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1, eight_nine, &exc));
        assert_sequence(sq, items[9], items[8]);
        yp_decrefN(N(sq, two, three, four_five, six, seven, eight_nine));
    }

    // yp_SLICE_LAST.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *empty = (*x_type)->newN(0);
        ypObject *two = (*x_type)->newN(N(items[2]));
        ypObject *three_four = (*x_type)->newN(N(items[3], items[4]));
        assert_not_raises_exc(yp_setsliceC6(sq, yp_SLICE_LAST, 2, 1, empty, &exc));
        assert_sequence(sq, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, yp_SLICE_LAST, 1, two, &exc));
        assert_sequence(sq, items[0], items[2]);
        assert_not_raises_exc(yp_setsliceC6(sq, yp_SLICE_LAST, yp_SLICE_LAST, 1, empty, &exc));
        assert_sequence(sq, items[0], items[2]);
        assert_not_raises_exc(yp_setsliceC6(sq, yp_SLICE_LAST, -3, -1, three_four, &exc));
        assert_sequence(sq, items[4], items[3]);
        assert_not_raises_exc(yp_setsliceC6(sq, -1, yp_SLICE_LAST, -1, empty, &exc));
        assert_sequence(sq, items[4], items[3]);
        assert_not_raises_exc(yp_setsliceC6(sq, yp_SLICE_LAST, yp_SLICE_LAST, -1, empty, &exc));
        assert_sequence(sq, items[4], items[3]);
        yp_decrefN(N(sq, empty, two, three_four));
    }

    // Invalid slices.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *empty = (*x_type)->newN(0);
        assert_raises_exc(yp_setsliceC6(sq, 0, 1, 0, empty, &exc), yp_ValueError);  // step==0
        assert_sequence(sq, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(sq, 0, 1, -yp_SSIZE_T_MAX - 1, empty, &exc),
                yp_SystemLimitationError);  // too-small step
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, empty));
    }

    // Regular slices (step==1) can grow and shrink the sequence.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(0);
        ypObject *empty = (*x_type)->newN(0);
        ypObject *zero_one = (*x_type)->newN(N(items[0], items[1]));
        ypObject *two = (*x_type)->newN(N(items[2]));
        ypObject *three = (*x_type)->newN(N(items[3]));
        ypObject *four_five = (*x_type)->newN(N(items[4], items[5]));
        ypObject *six_seven_eight = (*x_type)->newN(N(items[6], items[7], items[8]));
        ypObject *nine = (*x_type)->newN(N(items[9]));
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 0, 1, zero_one, &exc));
        assert_sequence(sq, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 0, 1, empty, &exc));
        assert_sequence(sq, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 0, 1, two, &exc));
        assert_sequence(sq, items[2], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, 2, 1, empty, &exc));
        assert_sequence(sq, items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, 2, 1, three, &exc));
        assert_sequence(sq, items[2], items[3]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, 2, 1, four_five, &exc));
        assert_sequence(sq, items[2], items[4], items[5]);
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 3, 1, six_seven_eight, &exc));
        assert_sequence(sq, items[6], items[7], items[8]);
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 3, 1, nine, &exc));
        assert_sequence(sq, items[9]);
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 1, 1, empty, &exc));
        assert_len(sq, 0);
        yp_decrefN(N(sq, empty, zero_one, two, three, four_five, six_seven_eight, nine));
    }

    // Extended slices (step!=1) can neither grow nor shrink the sequence.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *empty = (*x_type)->newN(0);
        ypObject *two = (*x_type)->newN(N(items[2]));
        ypObject *three_four = (*x_type)->newN(N(items[3], items[4]));
        assert_raises_exc(yp_setsliceC6(sq, 0, 0, 2, two, &exc), yp_ValueError);
        assert_sequence(sq, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(sq, -1, -2, -1, empty, &exc), yp_ValueError);
        assert_sequence(sq, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(sq, -1, -2, -1, three_four, &exc), yp_ValueError);
        assert_sequence(sq, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(sq, -1, -3, -1, empty, &exc), yp_ValueError);
        assert_sequence(sq, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(sq, -1, -3, -1, two, &exc), yp_ValueError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, empty, two, three_four));
    }

    // x is sq.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 0, 1, sq, &exc));
        assert_sequence(sq, items[0], items[1], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 1, 4, 1, sq, &exc));
        assert_sequence(sq, items[0], items[0], items[1], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, 0, 5, 1, sq, &exc));
        assert_sequence(sq, items[0], items[0], items[1], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(sq, -1, -6, -1, sq, &exc));
        assert_sequence(sq, items[1], items[0], items[1], items[0], items[0]);
        yp_decref(sq);
    }

    // x contains sq.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->is_string) {
            ypObject *sq = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(sq, items[2]));
            if (type->is_string) {
                assert_raises_exc(yp_setsliceC6(sq, 0, 1, 1, x, &exc), yp_ValueError, yp_TypeError);
                assert_sequence(sq, items[0], items[1]);
            } else {
                assert_not_raises_exc(yp_setsliceC6(sq, 0, 1, 1, x, &exc));
                assert_sequence(sq, sq, items[2], items[1]);
                assert_not_raises_exc(
                        yp_clear(sq, &exc));  // nohtyP does not yet break circular refs.
            }
            yp_decrefN(N(sq, x));
        }
    }

    // Duplicates: 0 is duplicated in sq, 1 in x, and 2 shared between them.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[2], items[0]));
        ypObject *x = (*x_type)->newN(N(items[2], items[1], items[1]));
        assert_not_raises_exc(yp_setsliceC6(sq, 1, 1, 1, x, &exc));
        assert_sequence(sq, items[0], items[2], items[1], items[1], items[2], items[0]);
        yp_decrefN(N(sq, x));
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *sq = type->newN(N(items[0], items[1])), x,
            type->newN(N(items[2], items[3])), yp_setsliceC6(sq, 0, 2, 1, x, &exc),
            assert_sequence(sq, items[2], items[3]), yp_decref(sq));

    // sq is not modified if the iterator fails at the start.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_setsliceC6(sq, 0, 2, 1, x, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, x_supplier, x));
    }

    // sq is not modified if the iterator fails mid-way: iterators must be converted to sequences
    // _first_, as we need to know how much to shift the data when step==1, or to ensure
    // len(x)==slicelength when step!=1.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_setsliceC6(sq, 0, 2, 1, x, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_setsliceC6(sq, 0, 2, 1, not_iterable, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq));
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_setsliceC6(sq, 0, 1, 1, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static void setindexC_to_setitem(ypObject *sq, yp_ssize_t i, ypObject *x, ypObject **exc)
{
    ypObject *int_i = yp_intC(i);
    yp_setitem(sq, int_i, x, exc);
    yp_decref(int_i);
}

static MunitResult test_setitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *intstore_0 = yp_intstoreC(0);
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Shared tests.
    _test_setindexC(fixture->type, yp_setindexC);

    // Remaining tests only apply to mutable objects.
    if (!type->is_mutable) goto tear_down;

    // intstore.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_setitem(sq, intstore_0, items[2], &exc));
        assert_sequence(sq, items[2], items[1]);
        yp_decref(sq);
    }

    // Index is sq.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_setitem(sq, sq, items[2], &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_setitem(sq, yp_SyntaxError, items[2], &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(intstore_0));
    return MUNIT_OK;
}

static void _test_delindexC(fixture_type_t *type,
        void (*any_delindexC)(ypObject *, yp_ssize_t, ypObject **), int raises)
{
    ypObject *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support delindex.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_delindexC(sq, 0, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

#define assert_not_found_exc(expression)            \
    do {                                            \
        ypObject *exc = yp_None;                    \
        (expression);                               \
        if (raises) {                               \
            assert_isexception(exc, yp_IndexError); \
        } else {                                    \
            assert_obj(exc, is, yp_None);           \
        }                                           \
    } while (0)

    // Basic index.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_not_raises_exc(any_delindexC(sq, 0, &exc));
        assert_sequence(sq, items[1], items[2]);
        assert_not_raises_exc(any_delindexC(sq, 1, &exc));
        assert_sequence(sq, items[1]);
        yp_decref(sq);
    }

    // Negative index.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_not_raises_exc(any_delindexC(sq, -1, &exc));
        assert_sequence(sq, items[0], items[1]);
        assert_not_raises_exc(any_delindexC(sq, -2, &exc));
        assert_sequence(sq, items[1]);
        yp_decref(sq);
    }

    // Out of bounds.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_found_exc(any_delindexC(sq, 2, &exc));
        assert_not_found_exc(any_delindexC(sq, -3, &exc));
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Previously-deleted index.
    {
        ypObject *sq_delitem = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_delindexC(sq_delitem, 1, &exc));
        assert_sequence(sq_delitem, items[0]);
        assert_not_found_exc(any_delindexC(sq_delitem, 1, &exc));
        assert_sequence(sq_delitem, items[0]);
        yp_decrefN(N(sq_delitem));
    }

    // Empty sq.
    {
        ypObject *empty = type->newN(0);
        assert_not_found_exc(any_delindexC(empty, 0, &exc));
        assert_not_found_exc(any_delindexC(empty, -1, &exc));
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_found_exc(any_delindexC(sq, yp_SLICE_DEFAULT, &exc));
        assert_not_found_exc(any_delindexC(sq, yp_SLICE_LAST, &exc));
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

#undef assert_not_found_exc

tear_down:
    obj_array_decref(items);
}

static MunitResult test_delindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_delindexC(fixture->type, yp_delindexC, /*raises=*/TRUE);
    return MUNIT_OK;
}

static MunitResult test_dropindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_delindexC(fixture->type, yp_dropindexC, /*raises=*/FALSE);
    return MUNIT_OK;
}

static MunitResult test_delsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[9];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support delslice.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_delsliceC5(sq, 0, 1, 1, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic slice.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_not_raises_exc(yp_delsliceC5(sq, 0, 1, 1, &exc));
        assert_sequence(sq, items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(sq, 1, 2, 1, &exc));
        assert_sequence(sq, items[1]);
        yp_decref(sq);
    }

    // Negative step.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_not_raises_exc(yp_delsliceC5(sq, -1, -2, -1, &exc));
        assert_sequence(sq, items[0], items[1]);
        assert_not_raises_exc(yp_delsliceC5(sq, -2, -3, -1, &exc));
        assert_sequence(sq, items[1]);
        yp_decref(sq);
    }

    // Total slice, forward and backward.
    {
        ypObject *self1 = type->newN(N(items[0], items[1]));
        ypObject *self2 = type->newN(N(items[2], items[3]));
        assert_not_raises_exc(yp_delsliceC5(self1, 0, 2, 1, &exc));
        assert_len(self1, 0);
        assert_not_raises_exc(yp_delsliceC5(self2, -1, -3, -1, &exc));
        assert_len(self2, 0);
        yp_decrefN(N(self1, self2));
    }

    // Step of 2, -2.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[8]));
        assert_not_raises_exc(yp_delsliceC5(sq, 0, 9, 2, &exc));
        assert_sequence(sq, items[1], items[3], items[5], items[7]);
        assert_not_raises_exc(yp_delsliceC5(sq, -1, -5, -2, &exc));
        assert_sequence(sq, items[1], items[5]);
        yp_decref(sq);
    }

    // Empty slices.
    {
        ypObject    *sq = type->newN(N(items[0], items[1]));
        slice_args_t slices[] = {
                {0, 0, 1},      // typical empty slice
                {5, 99, 1},     // i>=len(s) and k>0 (regardless of j)
                {-6, -99, -1},  // i<-len(s) and k<0 (regardless of j)
                {99, 5, -1},    // j>=len(s) and k<0 (regardless of i)
                {-99, -6, 1},   // j<-len(s) and k>0 (regardless of i)
                {4, 4, 1},      // i=j (regardless of k)
                {1, 0, 1},      // i>j and k>0
                {0, 1, -1},     // i<j and k<0
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            assert_not_raises_exc(yp_delsliceC5(sq, args.start, args.stop, args.step, &exc));
            assert_sequence(sq, items[0], items[1]);
        }
        yp_decref(sq);
    }

    // yp_SLICE_DEFAULT.
    {
        ypObject *self1 = type->newN(N(items[0], items[1], items[2], items[3]));
        ypObject *self2 = type->newN(N(items[4], items[5], items[6], items[7]));
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_DEFAULT, 1, 1, &exc));
        assert_sequence(self1, items[1], items[2], items[3]);
        assert_not_raises_exc(yp_delsliceC5(self1, 2, yp_SLICE_DEFAULT, 1, &exc));
        assert_sequence(self1, items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1, &exc));
        assert_len(self1, 0);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_DEFAULT, -2, -1, &exc));
        assert_sequence(self2, items[4], items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, -3, yp_SLICE_DEFAULT, -1, &exc));
        assert_sequence(self2, items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1, &exc));
        assert_len(self2, 0);
        yp_decrefN(N(self1, self2));
    }

    // yp_SLICE_LAST.
    {
        ypObject *self1 = type->newN(N(items[0], items[1], items[2], items[3]));
        ypObject *self2 = type->newN(N(items[4], items[5], items[6], items[7]));
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_LAST, 4, 1, &exc));
        assert_sequence(self1, items[0], items[1], items[2], items[3]);
        assert_not_raises_exc(yp_delsliceC5(self1, 3, yp_SLICE_LAST, 1, &exc));
        assert_sequence(self1, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_LAST, yp_SLICE_LAST, 1, &exc));
        assert_sequence(self1, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_LAST, -2, -1, &exc));
        assert_sequence(self2, items[4], items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, -1, yp_SLICE_LAST, -1, &exc));
        assert_sequence(self2, items[4], items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_LAST, yp_SLICE_LAST, -1, &exc));
        assert_sequence(self2, items[4], items[5], items[6]);
        yp_decrefN(N(self1, self2));
    }

    // Invalid slices.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_delsliceC5(sq, 0, 1, 0, &exc), yp_ValueError);  // step==0
        assert_sequence(sq, items[0], items[1]);
        assert_raises_exc(yp_delsliceC5(sq, 0, 1, -yp_SSIZE_T_MAX - 1, &exc),
                yp_SystemLimitationError);  // too-small step
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static void _test_delitem(fixture_type_t *type,
        void (*any_delindexC)(ypObject *, yp_ssize_t, ypObject **),
        void (*any_delitem)(ypObject *, ypObject *, ypObject **), int raises)
{
    ypObject *intstore_0 = yp_intstoreC(0);
    ypObject *items[4];
    obj_array_fill(items, type->rand_items);

    // Shared tests.
    _test_delindexC(type, any_delindexC, raises);

    // Remaining tests only apply to mutable objects.
    if (!type->is_mutable) goto tear_down;

    // intstore.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_not_raises_exc(any_delitem(sq, intstore_0, &exc));
        assert_sequence(sq, items[1], items[2]);
        yp_decref(sq);
    }

    // Index is sq.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_raises_exc(any_delitem(sq, sq, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1], items[2]);
        yp_decref(sq);
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(any_delitem(sq, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(intstore_0));
}

static void delindexC_to_delitem(ypObject *sq, yp_ssize_t i, ypObject **exc)
{
    ypObject *int_i = yp_intC(i);
    yp_delitem(sq, int_i, exc);
    yp_decref(int_i);
}

static MunitResult test_delitem(const MunitParameter params[], fixture_t *fixture)
{
    _test_delitem(fixture->type, delindexC_to_delitem, yp_delitem, /*raises=*/TRUE);
    return MUNIT_OK;
}

static void dropindexC_to_dropitem(ypObject *sq, yp_ssize_t i, ypObject **exc)
{
    ypObject *int_i = yp_intC(i);
    yp_dropitem(sq, int_i, exc);
    yp_decref(int_i);
}

static MunitResult test_dropitem(const MunitParameter params[], fixture_t *fixture)
{
    _test_delitem(fixture->type, dropindexC_to_dropitem, yp_dropitem, /*raises=*/FALSE);
    return MUNIT_OK;
}

// Shared tests for yp_append, yp_push. (These are the same operation on sequences.)
static void _test_appendC(
        fixture_type_t *type, void (*any_append)(ypObject *, ypObject *, ypObject **))
{
    ypObject *items[3];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support append.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_append(sq, items[2], &exc), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic append.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(any_append(sq, items[2], &exc));
        assert_sequence(sq, items[0], items[1], items[2]);
        yp_decref(sq);
    }

    // Self is empty.
    {
        ypObject *sq = type->newN(0);
        assert_not_raises_exc(any_append(sq, items[2], &exc));
        assert_sequence(sq, items[2]);
        yp_decref(sq);
    }

    // x is sq. Recall strings are restrictive about the objects they accept.
    // TODO Add similar tests to test_string, where sq is 0, 1, and 2 items.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        if (type->is_string) {
            assert_raises_exc(any_append(sq, sq, &exc), yp_ValueError, yp_TypeError);
            assert_sequence(sq, items[0], items[1]);
        } else {
            assert_not_raises_exc(any_append(sq, sq, &exc));
            assert_sequence(sq, items[0], items[1], sq);
            assert_not_raises_exc(yp_clear(sq, &exc));  // nohtyP does not yet break circular refs.
        }
        yp_decref(sq);
    }

    // Duplicates: 0 is duplicated in sq, 1 shared between them.
    {
        ypObject *sq = type->newN(N(items[0], items[0], items[1], items[2]));
        assert_not_raises_exc(any_append(sq, items[1], &exc));
        assert_sequence(sq, items[0], items[0], items[1], items[2], items[1]);
        yp_decref(sq);
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(any_append(sq, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
}

static MunitResult test_append(const MunitParameter params[], fixture_t *fixture)
{
    _test_appendC(fixture->type, yp_append);
    return MUNIT_OK;
}

static MunitResult test_push(const MunitParameter params[], fixture_t *fixture)
{
    _test_appendC(fixture->type, yp_push);
    return MUNIT_OK;
}

static MunitResult test_extend(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *not_iterable = rand_obj_any_not_iterable();
    ypObject        *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support extend.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *two = type->newN(N(items[2]));
        assert_raises_exc(yp_extend(sq, two, &exc), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, two));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic extend.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(N(items[2], items[3]));
        assert_not_raises_exc(yp_extend(sq, x, &exc));
        assert_sequence(sq, items[0], items[1], items[2], items[3]);
        yp_decrefN(N(sq, x));
    }

    // sq is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(0);
        ypObject *x = (*x_type)->newN(N(items[2], items[3]));
        assert_not_raises_exc(yp_extend(sq, x, &exc));
        assert_sequence(sq, items[2], items[3]);
        yp_decrefN(N(sq, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_extend(sq, x, &exc));
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, x));
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_extend(sq, x, &exc));
        assert_len(sq, 0);
        yp_decrefN(N(sq, x));
    }

    // x is sq.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_extend(sq, sq, &exc));
        assert_sequence(sq, items[0], items[1], items[0], items[1]);
        yp_decref(sq);
    }

    // x contains sq.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        if (!(*x_type)->is_string) {
            ypObject *sq = type->newN(N(items[0], items[1]));
            ypObject *x = (*x_type)->newN(N(sq, items[2]));
            if (type->is_string) {
                assert_raises_exc(yp_extend(sq, x, &exc), yp_ValueError, yp_TypeError);
                assert_sequence(sq, items[0], items[1]);
            } else {
                assert_not_raises_exc(yp_extend(sq, x, &exc));
                assert_sequence(sq, items[0], items[1], sq, items[2]);
                assert_not_raises_exc(
                        yp_clear(sq, &exc));  // nohtyP does not yet break circular refs.
            }
            yp_decrefN(N(sq, x));
        }
    }

    // Duplicates: 0 is duplicated in sq, 1 in x, and 2 shared between them.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *sq = type->newN(N(items[0], items[2], items[0]));
        ypObject *x = (*x_type)->newN(N(items[2], items[1], items[1]));
        assert_not_raises_exc(yp_extend(sq, x, &exc));
        assert_sequence(sq, items[0], items[2], items[0], items[2], items[1], items[1]);
        yp_decrefN(N(sq, x));
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests_exc(ypObject *sq = type->newN(N(items[0], items[1])), x,
            type->newN(N(items[2], items[3])), yp_extend(sq, x, &exc),
            assert_sequence(sq, items[0], items[1], items[2], items[3]), yp_decref(sq));

    // sq is not modified if the iterator fails at the start.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises_exc(yp_extend(sq, x, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq, x_supplier, x));
    }

    // Optimization: we append directly to sq from the iterator. Unfortunately, if the iterator
    // fails mid-way sq will have already been modified.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises_exc(yp_extend(sq, x, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1], items[2]);
        yp_decrefN(N(sq, x_supplier, x));
    }

    // x is not an iterable.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_extend(sq, not_iterable, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq));
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_extend(sq, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static MunitResult test_irepeatC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support irepeat.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_irepeatC(sq, 2, &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic irepeat.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_irepeatC(sq, 2, &exc));
        assert_sequence(sq, items[0], items[1], items[0], items[1]);
        yp_decref(sq);
    }

    // Factor of one.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_irepeatC(sq, 1, &exc));
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Factor of zero.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_irepeatC(sq, 0, &exc));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // Negative factor.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_irepeatC(sq, -1, &exc));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // Large factor. (Exercises _ypSequence_repeat_memcpy optimization.).
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_irepeatC(sq, 8, &exc));
        assert_sequence(sq, items[0], items[1], items[0], items[1], items[0], items[1], items[0],
                items[1], items[0], items[1], items[0], items[1], items[0], items[1], items[0],
                items[1]);
        yp_decref(sq);
    }

    // Empty sq.
    {
        ypObject *sq = type->newN(0);
        assert_not_raises_exc(yp_irepeatC(sq, 2, &exc));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // sq contains duplicates.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[0]));
        assert_not_raises_exc(yp_irepeatC(sq, 2, &exc));
        assert_sequence(sq, items[0], items[1], items[0], items[0], items[1], items[0]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_insertC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support insert.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_insertC(sq, 0, items[2], &exc), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic insert.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_insertC(sq, 0, items[2], &exc));
        assert_sequence(sq, items[2], items[0], items[1]);
        assert_not_raises_exc(yp_insertC(sq, 1, items[3], &exc));
        assert_sequence(sq, items[2], items[3], items[0], items[1]);
        yp_decref(sq);
    }

    // Negative index.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_insertC(sq, -1, items[2], &exc));
        assert_sequence(sq, items[0], items[2], items[1]);
        assert_not_raises_exc(yp_insertC(sq, -2, items[3], &exc));
        assert_sequence(sq, items[0], items[3], items[2], items[1]);
        yp_decref(sq);
    }

    // "Out of bounds": recall s.insert(i, x) is the same as s[i:i] = [x].
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_insertC(sq, 2, items[2], &exc));
        assert_sequence(sq, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_insertC(sq, -4, items[3], &exc));
        assert_sequence(sq, items[3], items[0], items[1], items[2]);
        yp_decref(sq);
    }

    // Empty sq.
    {
        ypObject *self1 = type->newN(0);
        ypObject *self2 = type->newN(0);
        assert_not_raises_exc(yp_insertC(self1, 0, items[2], &exc));
        assert_sequence(self1, items[2]);
        assert_not_raises_exc(yp_insertC(self2, -1, items[3], &exc));
        assert_sequence(self2, items[3]);
        yp_decrefN(N(self1, self2));
    }

    // yp_SLICE_DEFAULT. Recall in slices that yp_SLICE_DEFAULT is equivalent to None (or omitting)
    // in Python. Since s.insert(None, '') raises TypeError, that seems correct here too.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_insertC(sq, yp_SLICE_DEFAULT, items[2], &exc), yp_TypeError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // yp_SLICE_LAST.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_insertC(sq, yp_SLICE_LAST, items[2], &exc));
        assert_sequence(sq, items[0], items[1], items[2]);
        yp_decref(sq);
    }

    // x is sq. Recall strings are restrictive about the objects they accept.
    // TODO Add similar tests to test_string, where sq is 0, 1, and 2 items.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        if (type->is_string) {
            assert_raises_exc(yp_insertC(sq, 0, sq, &exc), yp_ValueError, yp_TypeError);
            assert_sequence(sq, items[0], items[1]);
        } else {
            assert_not_raises_exc(yp_insertC(sq, 0, sq, &exc));
            assert_sequence(sq, sq, items[0], items[1]);
            assert_not_raises_exc(yp_clear(sq, &exc));  // nohtyP does not yet break circular refs.
        }
        yp_decref(sq);
    }

    // Duplicates: 0 is duplicated in sq, 1 shared between them.
    {
        ypObject *sq = type->newN(N(items[0], items[0], items[1], items[2]));
        assert_not_raises_exc(yp_insertC(sq, 0, items[1], &exc));
        assert_sequence(sq, items[1], items[0], items[0], items[1], items[2]);
        yp_decref(sq);
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(yp_insertC(sq, 0, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_popindexC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[4];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support popindex.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises(yp_popindexC(sq, 0), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic popindex.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        ead(popped, yp_popindexC(sq, 0), assert_obj(popped, eq, items[0]));
        assert_sequence(sq, items[1], items[2]);
        ead(popped, yp_popindexC(sq, 1), assert_obj(popped, eq, items[2]));
        assert_sequence(sq, items[1]);
        yp_decref(sq);
    }

    // Negative index.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        ead(popped, yp_popindexC(sq, -1), assert_obj(popped, eq, items[2]));
        assert_sequence(sq, items[0], items[1]);
        ead(popped, yp_popindexC(sq, -2), assert_obj(popped, eq, items[0]));
        assert_sequence(sq, items[1]);
        yp_decref(sq);
    }

    // Out of bounds.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises(yp_popindexC(sq, 2), yp_IndexError);
        assert_raises(yp_popindexC(sq, -3), yp_IndexError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Previously-deleted index.
    {
        ypObject *sq_delitem = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_delindexC(sq_delitem, 1, &exc));
        assert_sequence(sq_delitem, items[0]);
        assert_raises(yp_popindexC(sq_delitem, 1), yp_IndexError);
        assert_sequence(sq_delitem, items[0]);
        yp_decrefN(N(sq_delitem));
    }

    // Empty sq.
    {
        ypObject *empty = type->newN(0);
        assert_raises(yp_popindexC(empty, 0), yp_IndexError);
        assert_raises(yp_popindexC(empty, -1), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises(yp_popindexC(sq, yp_SLICE_DEFAULT), yp_IndexError);
        assert_raises(yp_popindexC(sq, yp_SLICE_LAST), yp_IndexError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // sq contains duplicates.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2], items[0], items[0]));
        ead(popped, yp_popindexC(sq, 0), assert_obj(popped, eq, items[0]));
        assert_sequence(sq, items[1], items[2], items[0], items[0]);
        ead(popped, yp_popindexC(sq, 2), assert_obj(popped, eq, items[0]));
        assert_sequence(sq, items[1], items[2], items[0]);
        ead(popped, yp_popindexC(sq, 1), assert_obj(popped, eq, items[2]));
        assert_sequence(sq, items[1], items[0]);
        yp_decref(sq);
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        ead(popped, yp_popindexC(sq, 0), assert_obj(popped, is, items[0]));
        ead(popped, yp_popindexC(sq, 1), assert_obj(popped, is, items[2]));
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_pop(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support pop.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises(yp_pop(sq), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic pop.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ead(popped, yp_pop(sq), assert_obj(popped, eq, items[1]));
        assert_sequence(sq, items[0]);
        ead(popped, yp_pop(sq), assert_obj(popped, eq, items[0]));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // Self is empty.
    {
        ypObject *sq = type->newN(0);
        assert_raises(yp_pop(sq), yp_IndexError);
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // sq contains duplicates.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[0], items[0]));
        ead(popped, yp_pop(sq), assert_obj(popped, eq, items[0]));
        assert_sequence(sq, items[0], items[1], items[0]);
        ead(popped, yp_pop(sq), assert_obj(popped, eq, items[0]));
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ead(popped, yp_pop(sq), assert_obj(popped, is, items[1]));
        ead(popped, yp_pop(sq), assert_obj(popped, is, items[0]));
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

// Shared tests for yp_remove, yp_discard.
static void _test_remove(
        fixture_type_t *type, void (*any_remove)(ypObject *, ypObject *, ypObject **), int raises)
{
    ypObject *items[3];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support remove.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_remove(sq, items[0], &exc), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

#define assert_not_found_exc(expression)            \
    do {                                            \
        ypObject *exc = yp_None;                    \
        (expression);                               \
        if (raises) {                               \
            assert_isexception(exc, yp_ValueError); \
        } else {                                    \
            assert_obj(exc, is, yp_None);           \
        }                                           \
    } while (0)

    // Basic remove.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(any_remove(sq, items[0], &exc));
        assert_sequence(sq, items[1]);
        assert_not_raises_exc(any_remove(sq, items[1], &exc));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // Not in sequence.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_found_exc(any_remove(sq, items[2], &exc));
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Empty sq.
    {
        ypObject *sq = type->newN(0);
        assert_not_found_exc(any_remove(sq, items[2], &exc));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // x is sq. Recall `"abc" in "abc"` is True for strings.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        if (type->is_string) {
            assert_not_raises_exc(any_remove(sq, sq, &exc));
            assert_len(sq, 0);
        } else {
            assert_not_raises_exc(yp_append(sq, sq, &exc));
            assert_sequence(sq, items[0], items[1], sq);
            assert_not_raises_exc(any_remove(sq, sq, &exc));
            assert_sequence(sq, items[0], items[1]);
            assert_not_found_exc(any_remove(sq, sq, &exc));
            assert_sequence(sq, items[0], items[1]);
        }
        yp_decref(sq);
    }

    // If duplicates, the first one is the one that's removed. (There is no yp_rremove, yet.)
    {
        ypObject *dups = type->newN(N(items[0], items[2], items[1], items[2]));
        assert_not_raises_exc(any_remove(dups, items[2], &exc));
        assert_sequence(dups, items[0], items[1], items[2]);
        yp_decref(dups);
    }

    // For strings, remove looks for sub-sequences of items; all other sequences inspect only one
    // item at a time. This is tested more thoroughly in test_string.
    {
        ypObject *sq_0_1_2 = type->newN(N(items[0], items[1], items[2]));
        ypObject *sq_0_1 = type->newN(N(items[0], items[1]));
        assert_obj(sq_0_1, ne, items[2]);  // ensure sq_0_1 isn't actually an item in sq_0_1_2
        if (type->is_string) {
            assert_not_raises_exc(any_remove(sq_0_1_2, sq_0_1, &exc));
            assert_sequence(sq_0_1_2, items[2]);
        } else {
            assert_not_found_exc(any_remove(sq_0_1_2, sq_0_1, &exc));
            assert_sequence(sq_0_1_2, items[0], items[1], items[2]);
        }
        yp_decrefN(N(sq_0_1_2, sq_0_1));
    }

    // Exception passthrough.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_isexception_exc(any_remove(sq, yp_SyntaxError, &exc), yp_SyntaxError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
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

static MunitResult test_reverse(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[3];
    obj_array_fill(items, type->rand_items);

    // Immutables don't support reverse.
    if (!type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_reverse(sq, &exc), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic reverse.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2]));
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_sequence(sq, items[2], items[1], items[0]);
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_sequence(sq, items[0], items[1], items[2]);
        yp_decref(sq);
    }

    // Reverse sequence of length 2.
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_sequence(sq, items[1], items[0]);
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
    }

    // Reverse sequence of length 1.
    {
        ypObject *sq = type->newN(N(items[0]));
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_sequence(sq, items[0]);
        yp_decref(sq);
    }

    // Self is empty.
    {
        ypObject *sq = type->newN(0);
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_len(sq, 0);
        yp_decref(sq);
    }

    // sq contains duplicates.
    {
        ypObject *sq = type->newN(N(items[0], items[1], items[2], items[1]));
        assert_not_raises_exc(yp_reverse(sq, &exc));
        assert_sequence(sq, items[1], items[2], items[1], items[0]);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_sort(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *int_0 = yp_intC(0);
    ypObject       *int_1 = yp_intC(1);
    ypObject       *int_2 = yp_intC(2);
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Sort is only implemented for list; it's not currently part of the sequence protocol.
    if (type->type != yp_t_list) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_sort(sq, &exc), yp_MethodError);
        assert_sequence(sq, items[0], items[1]);
        yp_decref(sq);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic sort. This is more thoroughly tested in test_tuple.
    // TODO Write test_tuple.
    {
        ypObject *sq = type->newN(N(int_2, int_0, int_1, int_0));
        assert_not_raises_exc(yp_sort(sq, &exc));
        assert_sequence(sq, int_0, int_0, int_1, int_2);
        assert_not_raises_exc(yp_sort4(sq, yp_None, yp_True, &exc));
        assert_sequence(sq, int_2, int_1, int_0, int_0);
        yp_decref(sq);
    }

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(int_0, int_1, int_2));
    return MUNIT_OK;
}


static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params),
        TEST(test_repeatC, test_sequence_params), TEST(test_getindexC, test_sequence_params),
        TEST(test_getsliceC, test_sequence_params), TEST(test_getitem, test_sequence_params),
        TEST(test_getdefault, test_sequence_params), TEST(test_findC, test_sequence_params),
        TEST(test_indexC, test_sequence_params), TEST(test_rfindC, test_sequence_params),
        TEST(test_rindexC, test_sequence_params), TEST(test_countC, test_sequence_params),
        TEST(test_setindexC, test_sequence_params), TEST(test_setsliceC, test_sequence_params),
        TEST(test_setitem, test_sequence_params), TEST(test_delindexC, test_sequence_params),
        TEST(test_dropindexC, test_sequence_params), TEST(test_delsliceC, test_sequence_params),
        TEST(test_delitem, test_sequence_params), TEST(test_dropitem, test_sequence_params),
        TEST(test_append, test_sequence_params), TEST(test_push, test_sequence_params),
        TEST(test_extend, test_sequence_params), TEST(test_irepeatC, test_sequence_params),
        TEST(test_insertC, test_sequence_params), TEST(test_popindexC, test_sequence_params),
        TEST(test_pop, test_sequence_params), TEST(test_remove, test_sequence_params),
        TEST(test_discard, test_sequence_params), TEST(test_reverse, test_sequence_params),
        TEST(test_sort, test_sequence_params), {NULL}};


extern void test_sequence_initialize(void) {}

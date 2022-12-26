
#include "munit_test/unittest.h"


// Sequences should accept themselves, their pairs, and iterators as valid types for the "x" (i.e.
// "other iterable") argument.
// TODO Should x_types also include tuple/list? Is it required that every sequence be compatible
// with tuple/list as an iterable?
#define x_types_init(type)                            \
    {                                                 \
        (type), (type)->pair, fixture_type_iter, NULL \
    }


typedef struct _slice_args_t {
    yp_ssize_t start;
    yp_ssize_t stop;
    yp_ssize_t step;
} slice_args_t;


static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t  *friend_types[] = {type, type->pair, NULL};
    fixture_type_t **x_type;
    ypObject        *items[] = obj_array_init(4, type->rand_item());

    // range stores integers following a pattern, so doesn't support concat.
    if (type->is_patterned) {
        ypObject *self = rand_obj(type);
        assert_raises(yp_concat(self, self), yp_MethodError);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic concatenation.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *x = (*x_type)->newN(2, items[2], items[3]);
        ypObject *result = yp_concat(self, x);
        assert_type_is(result, type->type);
        assert_sequence(result, 4, items[0], items[1], items[2], items[3]);
        yp_decrefN(3, self, x, result);
    }

    // self is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(0);
        ypObject *x = (*x_type)->newN(2, items[0], items[1]);
        ypObject *result = yp_concat(self, x);
        assert_type_is(result, type->type);
        assert_sequence(result, 2, items[0], items[1]);
        yp_decrefN(3, self, x, result);
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_concat(self, x);
        assert_type_is(result, type->type);
        assert_sequence(result, 2, items[0], items[1]);
        yp_decrefN(3, self, x, result);
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_concat(self, x);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(3, self, x, result);
    }

    // Optimization: lazy shallow copy of an immutable self when friendly x is empty.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *x = (*x_type)->newN(0);
        ypObject *result = yp_concat(self, x);
        if (type->is_mutable) {
            assert_obj(self, is_not, result);
        } else {
            assert_obj(self, is, result);
        }
        yp_decrefN(3, self, x, result);
    }

    // Optimization: lazy shallow copy of a friendly immutable x when immutable self is empty.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(0);
        ypObject *x = (*x_type)->newN(2, items[0], items[1]);
        ypObject *result = yp_concat(self, x);
        if (type->is_mutable || (*x_type)->is_mutable) {
            assert_obj(x, is_not, result);
        } else {
            assert_obj(x, is, result);
        }
        yp_decrefN(3, self, x, result);
    }

    // Optimization: empty immortal when immutable self is empty and friendly x is empty.
    if (type->falsy != NULL) {
        for (x_type = friend_types; (*x_type) != NULL; x_type++) {
            ypObject *self = type->newN(0);
            ypObject *x = (*x_type)->newN(0);
            ypObject *result = yp_concat(self, x);
            assert_obj(result, is, type->falsy);
            yp_decrefN(3, self, x, result);
        }
    }

    // FIXME Test x being an iterator that fails at start, mid-way.
    // FIXME Test x being an iterator that lies about lenhint.

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_repeatC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(2, type->rand_item());

    // range stores integers following a pattern, so doesn't support repeat.
    if (type->is_patterned) {
        ypObject *self = rand_obj(type);
        assert_raises(yp_repeatC(self, 2), yp_MethodError);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic repeat.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *result = yp_repeatC(self, 2);
        assert_type_is(result, type->type);
        assert_sequence(result, 4, items[0], items[1], items[0], items[1]);
        yp_decrefN(2, self, result);
    }

    // Factor of one.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *result = yp_repeatC(self, 1);
        assert_type_is(result, type->type);
        assert_sequence(result, 2, items[0], items[1]);
        yp_decrefN(2, self, result);
    }

    // Factor of zero.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *result = yp_repeatC(self, 0);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(2, self, result);
    }

    // Negative factor.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *result = yp_repeatC(self, -1);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(2, self, result);
    }

    // Large factor. (Exercises _ypSequence_repeat_memcpy optimization.)
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *result = yp_repeatC(self, 8);
        assert_type_is(result, type->type);
        assert_sequence(result, 16, items[0], items[1], items[0], items[1], items[0], items[1],
                items[0], items[1], items[0], items[1], items[0], items[1], items[0], items[1],
                items[0], items[1]);
        yp_decrefN(2, self, result);
    }

    // Empty self.
    {
        ypObject *self = type->newN(0);
        ypObject *result = yp_repeatC(self, 2);
        assert_type_is(result, type->type);
        assert_len(result, 0);
        yp_decrefN(2, self, result);
    }

    // Optimization: lazy shallow copy of an immutable self when factor is one.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *result = yp_repeatC(self, 1);
        if (type->is_mutable) {
            assert_obj(self, is_not, result);
        } else {
            assert_obj(self, is, result);
        }
        yp_decrefN(2, self, result);
    }

    // Optimization: empty immortal when immutable self is empty.
    if (type->falsy != NULL) {
        ypObject *self = type->newN(0);
        ypObject *result = yp_repeatC(self, 2);
        assert_obj(result, is, type->falsy);
        yp_decrefN(2, self, result);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_getindexC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(2, type->rand_item());
    ypObject       *self = type->newN(2, items[0], items[1]);
    ypObject       *empty = type->newN(0);

    // Basic index.
    ead(zero, yp_getindexC(self, 0), assert_obj(zero, eq, items[0]));
    ead(one, yp_getindexC(self, 1), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, yp_getindexC(self, -1), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, yp_getindexC(self, -2), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    assert_raises(yp_getindexC(self, 2), yp_IndexError);
    assert_raises(yp_getindexC(self, -3), yp_IndexError);

    // Empty self.
    assert_raises(yp_getindexC(empty, 0), yp_IndexError);
    assert_raises(yp_getindexC(empty, -1), yp_IndexError);

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    assert_raises(yp_getindexC(self, yp_SLICE_DEFAULT), yp_IndexError);
    assert_raises(yp_getindexC(self, yp_SLICE_LAST), yp_IndexError);

    obj_array_decref(items);
    yp_decrefN(2, self, empty);
    return MUNIT_OK;
}

static MunitResult test_getsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[5];
    ypObject       *self;
    obj_array_fill(items, type->rand_items);
    self = type->newN(5, items[0], items[1], items[2], items[3], items[4]);

    // Basic slice.
    {
        ypObject *zero_one = yp_getsliceC4(self, 0, 1, 1);
        ypObject *one_two = yp_getsliceC4(self, 1, 2, 1);
        assert_type_is(zero_one, type->type);
        assert_sequence(zero_one, 1, items[0]);
        assert_type_is(one_two, type->type);
        assert_sequence(one_two, 1, items[1]);
        yp_decrefN(2, zero_one, one_two);
    }

    // Negative step.
    {
        ypObject *neg_one_neg_two = yp_getsliceC4(self, -1, -2, -1);
        ypObject *neg_two_neg_three = yp_getsliceC4(self, -2, -3, -1);
        assert_type_is(neg_one_neg_two, type->type);
        assert_sequence(neg_one_neg_two, 1, items[4]);
        assert_type_is(neg_two_neg_three, type->type);
        assert_sequence(neg_two_neg_three, 1, items[3]);
        yp_decrefN(2, neg_one_neg_two, neg_two_neg_three);
    }

    // Total slice, forward and backward.
    {
        ypObject *forward = yp_getsliceC4(self, 0, 5, 1);
        ypObject *reverse = yp_getsliceC4(self, -1, -6, -1);
        assert_type_is(forward, type->type);
        assert_sequence(forward, 5, items[0], items[1], items[2], items[3], items[4]);
        assert_type_is(reverse, type->type);
        assert_sequence(reverse, 5, items[4], items[3], items[2], items[1], items[0]);
        yp_decrefN(2, forward, reverse);
    }

    // Step of 2, -2.
    {
        ypObject *forward = yp_getsliceC4(self, 0, 5, 2);
        ypObject *reverse = yp_getsliceC4(self, -1, -6, -2);
        assert_type_is(forward, type->type);
        assert_sequence(forward, 3, items[0], items[2], items[4]);
        assert_type_is(reverse, type->type);
        assert_sequence(reverse, 3, items[4], items[2], items[0]);
        yp_decrefN(2, forward, reverse);
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
            ypObject    *empty = yp_getsliceC4(self, args.start, args.stop, args.step);
            assert_type_is(empty, type->type);
            assert_len(empty, 0);
            yp_decref(empty);
        }
    }

    // yp_SLICE_DEFAULT.
    ead(i_pos_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, 2, 1),
            assert_sequence(i_pos_step, 2, items[0], items[1]));
    ead(j_pos_step, yp_getsliceC4(self, 2, yp_SLICE_DEFAULT, 1),
            assert_sequence(j_pos_step, 3, items[2], items[3], items[4]));
    ead(both_pos_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1),
            assert_sequence(both_pos_step, 5, items[0], items[1], items[2], items[3], items[4]));
    ead(i_neg_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, -3, -1),
            assert_sequence(i_neg_step, 2, items[4], items[3]));
    ead(j_neg_step, yp_getsliceC4(self, -3, yp_SLICE_DEFAULT, -1),
            assert_sequence(j_neg_step, 3, items[2], items[1], items[0]));
    ead(both_neg_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1),
            assert_sequence(both_neg_step, 5, items[4], items[3], items[2], items[1], items[0]));

    // yp_SLICE_LAST.
    ead(i_pos_step, yp_getsliceC4(self, yp_SLICE_LAST, 5, 1), assert_len(i_pos_step, 0));
    ead(j_pos_step, yp_getsliceC4(self, 0, yp_SLICE_LAST, 1),
            assert_sequence(j_pos_step, 5, items[0], items[1], items[2], items[3], items[4]));
    ead(both_pos_step, yp_getsliceC4(self, yp_SLICE_LAST, yp_SLICE_LAST, 1),
            assert_len(both_pos_step, 0));
    ead(i_neg_step, yp_getsliceC4(self, yp_SLICE_LAST, -6, -1),
            assert_sequence(i_neg_step, 5, items[4], items[3], items[2], items[1], items[0]));
    ead(j_neg_step, yp_getsliceC4(self, -1, yp_SLICE_LAST, -1), assert_len(j_neg_step, 0));
    ead(both_neg_step, yp_getsliceC4(self, yp_SLICE_LAST, yp_SLICE_LAST, -1),
            assert_len(both_neg_step, 0));

    // Invalid slices.
    assert_raises(yp_getsliceC4(self, 0, 1, 0), yp_ValueError);  // step==0
    assert_raises(yp_getsliceC4(self, 0, 1, -yp_SSIZE_T_MAX - 1),
            yp_SystemLimitationError);  // too-small step

    // Optimization: lazy shallow copy of an immutable self for total forward slice.
    if (!type->is_mutable) {
        ead(forward, yp_getsliceC4(self, 0, 5, 1), assert_obj(forward, is, self));
    }

    // Optimization: empty immortal when slice is empty.
    if (type->falsy != NULL) {
        ead(empty, yp_getsliceC4(self, 0, 0, 1), assert_obj(empty, is, type->falsy));
    }

    // Python's test_getslice.
    ead(slice, yp_getsliceC4(self, 0, 0, 1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(self, 1, 2, 1), assert_sequence(slice, 1, items[1]));
    ead(slice, yp_getsliceC4(self, -2, -1, 1), assert_sequence(slice, 1, items[3]));
    ead(slice, yp_getsliceC4(self, -1000, 1000, 1), assert_obj(slice, eq, self));
    ead(slice, yp_getsliceC4(self, 1000, -1000, 1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1),
            assert_obj(slice, eq, self));
    ead(slice, yp_getsliceC4(self, 1, yp_SLICE_DEFAULT, 1),
            assert_sequence(slice, 4, items[1], items[2], items[3], items[4]));
    ead(slice, yp_getsliceC4(self, yp_SLICE_DEFAULT, 3, 1),
            assert_sequence(slice, 3, items[0], items[1], items[2]));

    ead(slice, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 2),
            assert_sequence(slice, 3, items[0], items[2], items[4]));
    ead(slice, yp_getsliceC4(self, 1, yp_SLICE_DEFAULT, 2),
            assert_sequence(slice, 2, items[1], items[3]));
    ead(slice, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1),
            assert_sequence(slice, 5, items[4], items[3], items[2], items[1], items[0]));
    ead(slice, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -2),
            assert_sequence(slice, 3, items[4], items[2], items[0]));
    ead(slice, yp_getsliceC4(self, 3, yp_SLICE_DEFAULT, -2),
            assert_sequence(slice, 2, items[3], items[1]));
    ead(slice, yp_getsliceC4(self, 3, 3, -2), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(self, 3, 2, -2), assert_sequence(slice, 1, items[3]));
    ead(slice, yp_getsliceC4(self, 3, 1, -2), assert_sequence(slice, 1, items[3]));
    ead(slice, yp_getsliceC4(self, 3, 0, -2), assert_sequence(slice, 2, items[3], items[1]));
    ead(slice, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -100),
            assert_sequence(slice, 1, items[4]));
    ead(slice, yp_getsliceC4(self, 100, -100, 1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(self, -100, 100, 1), assert_obj(slice, eq, self));
    ead(slice, yp_getsliceC4(self, 100, -100, -1),
            assert_sequence(slice, 5, items[4], items[3], items[2], items[1], items[0]));
    ead(slice, yp_getsliceC4(self, -100, 100, -1), assert_len(slice, 0));
    ead(slice, yp_getsliceC4(self, -100, 100, 2),
            assert_sequence(slice, 3, items[0], items[2], items[4]));

    yp_decref(self);
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_getitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(2, type->rand_item());
    ypObject       *self = type->newN(2, items[0], items[1]);
    ypObject       *empty = type->newN(0);

    // Basic index.
    ead(zero, yp_getitem(self, int_0), assert_obj(zero, eq, items[0]));
    ead(one, yp_getitem(self, int_1), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, yp_getitem(self, int_neg_1), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, yp_getitem(self, int_neg_2), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    assert_raises(yp_getitem(self, int_2), yp_IndexError);
    assert_raises(yp_getitem(self, int_neg_3), yp_IndexError);

    // Empty self.
    assert_raises(yp_getitem(empty, int_0), yp_IndexError);
    assert_raises(yp_getitem(empty, int_neg_1), yp_IndexError);

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    assert_raises(yp_getitem(self, int_SLICE_DEFAULT), yp_IndexError);
    assert_raises(yp_getitem(self, int_SLICE_LAST), yp_IndexError);

    // FIXME non-integer indicies?

    obj_array_decref(items);
    yp_decrefN(2, self, empty);
    return MUNIT_OK;
}

// FIXME yp_getdefault isn't listed as part of the sequence protocol in nohtyP.h, but it is.
static MunitResult test_getdefault(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(3, type->rand_item());
    ypObject       *self = type->newN(2, items[0], items[1]);
    ypObject       *empty = type->newN(0);

    // Basic index.
    ead(zero, yp_getdefault(self, int_0, items[2]), assert_obj(zero, eq, items[0]));
    ead(one, yp_getdefault(self, int_1, items[2]), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, yp_getdefault(self, int_neg_1, items[2]), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, yp_getdefault(self, int_neg_2, items[2]), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    ead(two, yp_getdefault(self, int_2, items[2]), assert_obj(two, eq, items[2]));
    ead(neg_three, yp_getdefault(self, int_neg_3, items[2]), assert_obj(neg_three, eq, items[2]));

    // Empty self.
    ead(zero, yp_getdefault(empty, int_0, items[2]), assert_obj(zero, eq, items[2]));
    ead(neg_one, yp_getdefault(empty, int_neg_1, items[2]), assert_obj(neg_one, eq, items[2]));

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    ead(slice_default, yp_getdefault(self, int_SLICE_DEFAULT, items[2]),
            assert_obj(slice_default, eq, items[2]));
    ead(slice_last, yp_getdefault(self, int_SLICE_LAST, items[2]),
            assert_obj(slice_last, eq, items[2]));

    // FIXME non-integer indicies?

    obj_array_decref(items);
    yp_decrefN(2, self, empty);
    return MUNIT_OK;
}

// Shared tests for yp_findC5, yp_indexC5, yp_rfindC5, yp_rindexC5, etc.
static MunitResult _test_findC(fixture_type_t *type,
        yp_ssize_t (*any_findC)(ypObject *, ypObject *, ypObject **),
        yp_ssize_t (*any_findC5)(ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, ypObject **),
        int forward, int raises)
{
    ypObject *items[3];
    ypObject *self;
    ypObject *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    self = type->newN(2, items[0], items[1]);

#define assert_not_found_exc(expression)                    \
    do {                                                    \
        ypObject *exc = yp_None;                            \
        assert_ssizeC(expression, ==, -1);                  \
        if (raises) assert_isexception(exc, yp_ValueError); \
    } while (0)

    // Basic find.
    assert_ssizeC_exc(any_findC(self, items[0], &exc), ==, 0);
    assert_ssizeC_exc(any_findC(self, items[1], &exc), ==, 1);

    // Not in sequence.
    assert_not_found_exc(any_findC(self, items[2], &exc));

    // Empty self.
    assert_not_found_exc(any_findC(empty, items[0], &exc));

    // Basic slice.
    assert_ssizeC_exc(any_findC5(self, items[0], 0, 1, &exc), ==, 0);
    assert_not_found_exc(any_findC5(self, items[0], 1, 2, &exc));
    assert_not_found_exc(any_findC5(self, items[1], 0, 1, &exc));
    assert_ssizeC_exc(any_findC5(self, items[1], 1, 2, &exc), ==, 1);

    // Negative indicies.
    assert_ssizeC_exc(any_findC5(self, items[0], -2, -1, &exc), ==, 0);
    assert_not_found_exc(any_findC5(self, items[0], -1, 2, &exc));
    assert_not_found_exc(any_findC5(self, items[1], -2, -1, &exc));
    assert_ssizeC_exc(any_findC5(self, items[1], -1, 2, &exc), ==, 1);

    // Total slice.
    assert_ssizeC_exc(any_findC5(self, items[0], 0, 2, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(self, items[1], 0, 2, &exc), ==, 1);
    assert_not_found_exc(any_findC5(self, items[2], 0, 2, &exc));

    // Total slice, negative indicies.
    assert_ssizeC_exc(any_findC5(self, items[0], -2, 2, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(self, items[1], -2, 2, &exc), ==, 1);
    assert_not_found_exc(any_findC5(self, items[2], -2, 2, &exc));

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
            assert_not_found_exc(any_findC5(self, items[0], args.start, args.stop, &exc));
            assert_not_found_exc(any_findC5(self, items[1], args.start, args.stop, &exc));
            assert_not_found_exc(any_findC5(self, items[2], args.start, args.stop, &exc));
        }
    }

    // yp_SLICE_DEFAULT.
    assert_ssizeC_exc(any_findC5(self, items[0], yp_SLICE_DEFAULT, 1, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(self, items[0], 0, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(self, items[0], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(any_findC5(self, items[1], yp_SLICE_DEFAULT, 2, &exc), ==, 1);
    assert_ssizeC_exc(any_findC5(self, items[1], 1, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(any_findC5(self, items[1], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_not_found_exc(any_findC5(self, items[2], yp_SLICE_DEFAULT, 2, &exc));
    assert_not_found_exc(any_findC5(self, items[2], 0, yp_SLICE_DEFAULT, &exc));
    assert_not_found_exc(any_findC5(self, items[2], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc));

    // yp_SLICE_LAST.
    assert_not_found_exc(any_findC5(self, items[0], yp_SLICE_LAST, 2, &exc));
    assert_ssizeC_exc(any_findC5(self, items[0], 0, yp_SLICE_LAST, &exc), ==, 0);
    assert_not_found_exc(any_findC5(self, items[0], yp_SLICE_LAST, yp_SLICE_LAST, &exc));
    assert_not_found_exc(any_findC5(self, items[1], yp_SLICE_LAST, 2, &exc));
    assert_ssizeC_exc(any_findC5(self, items[1], 1, yp_SLICE_LAST, &exc), ==, 1);
    assert_not_found_exc(any_findC5(self, items[1], yp_SLICE_LAST, yp_SLICE_LAST, &exc));
    assert_not_found_exc(any_findC5(self, items[2], yp_SLICE_LAST, 2, &exc));
    assert_not_found_exc(any_findC5(self, items[2], 0, yp_SLICE_LAST, &exc));
    assert_not_found_exc(any_findC5(self, items[2], yp_SLICE_LAST, yp_SLICE_LAST, &exc));

    // If multiples, which one is found depends on the direction. Recall patterned sequences like
    // range don't store duplicates.
    if (!type->is_patterned) {
        ypObject *multi = type->newN(3, items[2], items[2], items[2]);
        assert_ssizeC_exc(any_findC(multi, items[2], &exc), ==, forward ? 0 : 2);
        assert_ssizeC_exc(any_findC5(multi, items[2], 0, 2, &exc), ==, forward ? 0 : 1);  // Basic.
        assert_ssizeC_exc(any_findC5(multi, items[2], 1, 3, &exc), ==, forward ? 1 : 2);  // Basic.
        assert_ssizeC_exc(any_findC5(multi, items[2], -3, -1, &exc), ==, forward ? 0 : 1);  // Neg.
        assert_ssizeC_exc(any_findC5(multi, items[2], -2, 3, &exc), ==, forward ? 1 : 2);   // Neg.
        assert_ssizeC_exc(any_findC5(multi, items[2], 0, 3, &exc), ==, forward ? 0 : 2);   // Total.
        assert_ssizeC_exc(any_findC5(multi, items[2], -3, 3, &exc), ==, forward ? 0 : 2);  // Total.
        yp_decref(multi);
    }

    if (type->is_string) {
        // For strings, find looks for sub-sequences of items. This behaviour is tested more
        // thoroughly in test_string.
        ypObject *string = type->newN(3, items[0], items[1], items[2]);
        ypObject *other_0_1 = type->newN(2, items[0], items[1]);

        assert_ssizeC_exc(any_findC(string, other_0_1, &exc), ==, 0);
        assert_ssizeC_exc(any_findC5(string, other_0_1, 0, 3, &exc), ==, 0);

        assert_ssizeC_exc(any_findC(string, string, &exc), ==, 0);
        assert_ssizeC_exc(any_findC5(string, string, 0, 3, &exc), ==, 0);

        yp_decrefN(2, string, other_0_1);

    } else {
        // All other sequences inspect only one item at a time.
        ypObject *seq = type->newN(3, items[0], items[1], items[2]);
        assert_not_found_exc(any_findC(seq, seq, &exc));
        assert_not_found_exc(any_findC5(seq, seq, 0, 3, &exc));
        yp_decref(seq);
    }

#undef assert_not_found_exc

    obj_array_decref(items);
    yp_decrefN(2, self, empty);
    return MUNIT_OK;
}

static MunitResult test_findC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_findC, yp_findC5, /*forward=*/TRUE, /*raises=*/FALSE);
}

static MunitResult test_indexC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_indexC, yp_indexC5, /*forward=*/TRUE, /*raises=*/TRUE);
}

static MunitResult test_rfindC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_rfindC, yp_rfindC5, /*forward=*/FALSE, /*raises=*/FALSE);
}

static MunitResult test_rindexC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_rindexC, yp_rindexC5, /*forward=*/FALSE, /*raises=*/TRUE);
}

static MunitResult test_countC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[3];
    ypObject       *self;
    ypObject       *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    self = type->newN(2, items[0], items[1]);

    // Basic count.
    assert_ssizeC_exc(yp_countC(self, items[0], &exc), ==, 1);
    assert_ssizeC_exc(yp_countC(self, items[1], &exc), ==, 1);

    // Not in sequence.
    assert_ssizeC_exc(yp_countC(self, items[2], &exc), ==, 0);

    // Empty self.
    assert_ssizeC_exc(yp_countC(empty, items[0], &exc), ==, 0);

    // Basic slice.
    assert_ssizeC_exc(yp_countC5(self, items[0], 0, 1, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[0], 1, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[1], 0, 1, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[1], 1, 2, &exc), ==, 1);

    // Negative indicies.
    assert_ssizeC_exc(yp_countC5(self, items[0], -2, -1, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[0], -1, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[1], -2, -1, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[1], -1, 2, &exc), ==, 1);

    // Total slice.
    assert_ssizeC_exc(yp_countC5(self, items[0], 0, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[1], 0, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[2], 0, 2, &exc), ==, 0);

    // Total slice, negative indicies.
    assert_ssizeC_exc(yp_countC5(self, items[0], -2, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[1], -2, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[2], -2, 2, &exc), ==, 0);

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
            assert_ssizeC_exc(yp_countC5(self, items[0], args.start, args.stop, &exc), ==, 0);
            assert_ssizeC_exc(yp_countC5(self, items[1], args.start, args.stop, &exc), ==, 0);
            assert_ssizeC_exc(yp_countC5(self, items[2], args.start, args.stop, &exc), ==, 0);
        }
    }

    // yp_SLICE_DEFAULT.
    assert_ssizeC_exc(yp_countC5(self, items[0], yp_SLICE_DEFAULT, 1, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[0], 1, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[0], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[1], yp_SLICE_DEFAULT, 2, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[1], 1, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[1], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[2], yp_SLICE_DEFAULT, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[2], 0, yp_SLICE_DEFAULT, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[2], yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, &exc), ==, 0);

    // yp_SLICE_LAST.
    assert_ssizeC_exc(yp_countC5(self, items[0], yp_SLICE_LAST, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[0], 0, yp_SLICE_LAST, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[0], yp_SLICE_LAST, yp_SLICE_LAST, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[1], yp_SLICE_LAST, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[1], 1, yp_SLICE_LAST, &exc), ==, 1);
    assert_ssizeC_exc(yp_countC5(self, items[1], yp_SLICE_LAST, yp_SLICE_LAST, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[2], yp_SLICE_LAST, 2, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[2], 0, yp_SLICE_LAST, &exc), ==, 0);
    assert_ssizeC_exc(yp_countC5(self, items[2], yp_SLICE_LAST, yp_SLICE_LAST, &exc), ==, 0);

    // Recall patterned sequences like range don't store duplicates.
    if (!type->is_patterned) {
        ypObject *multi = type->newN(3, items[2], items[2], items[2]);
        assert_ssizeC_exc(yp_countC(multi, items[2], &exc), ==, 3);
        assert_ssizeC_exc(yp_countC5(multi, items[2], 0, 1, &exc), ==, 1);    // Basic.
        assert_ssizeC_exc(yp_countC5(multi, items[2], 0, 2, &exc), ==, 2);    // Basic.
        assert_ssizeC_exc(yp_countC5(multi, items[2], 1, 3, &exc), ==, 2);    // Basic.
        assert_ssizeC_exc(yp_countC5(multi, items[2], -3, -2, &exc), ==, 1);  // Neg.
        assert_ssizeC_exc(yp_countC5(multi, items[2], -3, -1, &exc), ==, 2);  // Neg.
        assert_ssizeC_exc(yp_countC5(multi, items[2], -2, 3, &exc), ==, 2);   // Neg.
        assert_ssizeC_exc(yp_countC5(multi, items[2], 0, 3, &exc), ==, 3);    // Total.
        assert_ssizeC_exc(yp_countC5(multi, items[2], -3, 3, &exc), ==, 3);   // Total.
        yp_decref(multi);
    }

    if (type->is_string) {
        // For strings, count looks for non-overlapping sub-sequences of items. This behaviour is
        // tested more thoroughly in test_string.
        ypObject *string = type->newN(3, items[0], items[1], items[2]);
        ypObject *other_0_1 = type->newN(2, items[0], items[1]);

        assert_ssizeC_exc(yp_countC(string, other_0_1, &exc), ==, 1);
        assert_ssizeC_exc(yp_countC5(string, other_0_1, 0, 3, &exc), ==, 1);

        assert_ssizeC_exc(yp_countC(string, string, &exc), ==, 1);
        assert_ssizeC_exc(yp_countC5(string, string, 0, 3, &exc), ==, 1);

        yp_decrefN(2, string, other_0_1);

    } else {
        // All other sequences count only one item at a time.
        ypObject *seq = type->newN(3, items[0], items[1], items[2]);
        assert_ssizeC_exc(yp_countC(seq, seq, &exc), ==, 0);
        assert_ssizeC_exc(yp_countC5(seq, seq, 0, 3, &exc), ==, 0);
        yp_decref(seq);
    }

    obj_array_decref(items);
    yp_decrefN(2, self, empty);
    return MUNIT_OK;
}

static MunitResult test_setindexC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support setindex.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_setindexC(self, 0, items[2], &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_setindexC(self, 0, items[2], &exc));
        assert_sequence(self, 2, items[2], items[1]);
        assert_not_raises_exc(yp_setindexC(self, 1, items[3], &exc));
        assert_sequence(self, 2, items[2], items[3]);
        yp_decref(self);
    }

    // Negative index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_setindexC(self, -1, items[2], &exc));
        assert_sequence(self, 2, items[0], items[2]);
        assert_not_raises_exc(yp_setindexC(self, -2, items[3], &exc));
        assert_sequence(self, 2, items[3], items[2]);
        yp_decref(self);
    }

    // Out of bounds.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_setindexC(self, 2, items[2], &exc), yp_IndexError);
        assert_raises_exc(yp_setindexC(self, -3, items[3], &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *empty = type->newN(0);
        assert_raises_exc(yp_setindexC(empty, 0, items[2], &exc), yp_IndexError);
        assert_raises_exc(yp_setindexC(empty, -1, items[3], &exc), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_setindexC(self, yp_SLICE_DEFAULT, items[2], &exc), yp_IndexError);
        assert_raises_exc(yp_setindexC(self, yp_SLICE_LAST, items[3], &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_setsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[] = obj_array_init(11, type->rand_item());

    // Immutables don't support setslice.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *two = type->newN(1, items[2]);
        assert_raises_exc(yp_setsliceC6(self, 0, 1, 1, two, &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decrefN(2, self, two);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic slice.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *two = (*x_type)->newN(1, items[2]);
        ypObject *three = (*x_type)->newN(1, items[3]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 1, 1, two, &exc));
        assert_sequence(self, 2, items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, 2, 1, three, &exc));
        assert_sequence(self, 2, items[2], items[3]);
        yp_decrefN(3, self, two, three);
    }

    // Negative step.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *two = (*x_type)->newN(1, items[2]);
        ypObject *three = (*x_type)->newN(1, items[3]);
        assert_not_raises_exc(yp_setsliceC6(self, -1, -2, -1, two, &exc));
        assert_sequence(self, 2, items[0], items[2]);
        assert_not_raises_exc(yp_setsliceC6(self, -2, -3, -1, three, &exc));
        assert_sequence(self, 2, items[3], items[2]);
        yp_decrefN(3, self, two, three);
    }

    // Total slice, forward and backward.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *four_five = (*x_type)->newN(2, items[4], items[5]);
        ypObject *six_seven = (*x_type)->newN(2, items[6], items[7]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 2, 1, four_five, &exc));
        assert_sequence(self, 2, items[4], items[5]);
        assert_not_raises_exc(yp_setsliceC6(self, -1, -3, -1, six_seven, &exc));
        assert_sequence(self, 2, items[7], items[6]);
        yp_decrefN(3, self, four_five, six_seven);
    }

    // Step of 2, -2.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(5, items[0], items[1], items[2], items[3], items[4]);
        ypObject *five_six_seven = (*x_type)->newN(3, items[5], items[6], items[7]);
        ypObject *eight_nine_ten = (*x_type)->newN(3, items[8], items[9], items[10]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 5, 2, five_six_seven, &exc));
        assert_sequence(self, 5, items[5], items[1], items[6], items[3], items[7]);
        assert_not_raises_exc(yp_setsliceC6(self, -1, -6, -2, eight_nine_ten, &exc));
        assert_sequence(self, 5, items[10], items[1], items[9], items[3], items[8]);
        yp_decrefN(3, self, five_six_seven, eight_nine_ten);
    }

    // Empty slices.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject    *self = type->newN(2, items[0], items[1]);
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
            assert_not_raises_exc(
                    yp_setsliceC6(self, args.start, args.stop, args.step, empty, &exc));
            assert_sequence(self, 2, items[0], items[1]);
        }
        yp_decrefN(2, self, empty);
    }

    // yp_SLICE_DEFAULT.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *two = (*x_type)->newN(1, items[2]);
        ypObject *three = (*x_type)->newN(1, items[3]);
        ypObject *four_five = (*x_type)->newN(2, items[4], items[5]);
        ypObject *six = (*x_type)->newN(1, items[6]);
        ypObject *seven = (*x_type)->newN(1, items[7]);
        ypObject *eight_nine = (*x_type)->newN(2, items[8], items[9]);
        assert_not_raises_exc(yp_setsliceC6(self, yp_SLICE_DEFAULT, 1, 1, two, &exc));
        assert_sequence(self, 2, items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, yp_SLICE_DEFAULT, 1, three, &exc));
        assert_sequence(self, 2, items[2], items[3]);
        assert_not_raises_exc(
                yp_setsliceC6(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1, four_five, &exc));
        assert_sequence(self, 2, items[4], items[5]);
        assert_not_raises_exc(yp_setsliceC6(self, yp_SLICE_DEFAULT, -2, -1, six, &exc));
        assert_sequence(self, 2, items[4], items[6]);
        assert_not_raises_exc(yp_setsliceC6(self, -2, yp_SLICE_DEFAULT, -1, seven, &exc));
        assert_sequence(self, 2, items[7], items[6]);
        assert_not_raises_exc(
                yp_setsliceC6(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1, eight_nine, &exc));
        assert_sequence(self, 2, items[9], items[8]);
        yp_decrefN(7, self, two, three, four_five, six, seven, eight_nine);
    }

    // yp_SLICE_LAST.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *empty = (*x_type)->newN(0);
        ypObject *two = (*x_type)->newN(1, items[2]);
        ypObject *three_four = (*x_type)->newN(2, items[3], items[4]);
        assert_not_raises_exc(yp_setsliceC6(self, yp_SLICE_LAST, 2, 1, empty, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, yp_SLICE_LAST, 1, two, &exc));
        assert_sequence(self, 2, items[0], items[2]);
        assert_not_raises_exc(yp_setsliceC6(self, yp_SLICE_LAST, yp_SLICE_LAST, 1, empty, &exc));
        assert_sequence(self, 2, items[0], items[2]);
        assert_not_raises_exc(yp_setsliceC6(self, yp_SLICE_LAST, -3, -1, three_four, &exc));
        assert_sequence(self, 2, items[4], items[3]);
        assert_not_raises_exc(yp_setsliceC6(self, -1, yp_SLICE_LAST, -1, empty, &exc));
        assert_sequence(self, 2, items[4], items[3]);
        assert_not_raises_exc(yp_setsliceC6(self, yp_SLICE_LAST, yp_SLICE_LAST, -1, empty, &exc));
        assert_sequence(self, 2, items[4], items[3]);
        yp_decrefN(4, self, empty, two, three_four);
    }

    // Invalid slices.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *empty = (*x_type)->newN(0);
        assert_raises_exc(yp_setsliceC6(self, 0, 1, 0, empty, &exc), yp_ValueError);  // step==0
        assert_sequence(self, 2, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(self, 0, 1, -yp_SSIZE_T_MAX - 1, empty, &exc),
                yp_SystemLimitationError);  // too-small step
        assert_sequence(self, 2, items[0], items[1]);
        yp_decrefN(2, self, empty);
    }

    // Regular slices (step==1) can grow and shrink the sequence.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(0);
        ypObject *empty = (*x_type)->newN(0);
        ypObject *zero_one = (*x_type)->newN(2, items[0], items[1]);
        ypObject *two = (*x_type)->newN(1, items[2]);
        ypObject *three = (*x_type)->newN(1, items[3]);
        ypObject *four_five = (*x_type)->newN(2, items[4], items[5]);
        ypObject *six_seven_eight = (*x_type)->newN(3, items[6], items[7], items[8]);
        ypObject *nine = (*x_type)->newN(1, items[9]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 0, 1, zero_one, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 0, 1, empty, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 0, 1, two, &exc));
        assert_sequence(self, 3, items[2], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, 2, 1, empty, &exc));
        assert_sequence(self, 2, items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, 2, 1, three, &exc));
        assert_sequence(self, 2, items[2], items[3]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, 2, 1, four_five, &exc));
        assert_sequence(self, 3, items[2], items[4], items[5]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 3, 1, six_seven_eight, &exc));
        assert_sequence(self, 3, items[6], items[7], items[8]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 3, 1, nine, &exc));
        assert_sequence(self, 1, items[9]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 1, 1, empty, &exc));
        assert_len(self, 0);
        yp_decrefN(8, self, empty, zero_one, two, three, four_five, six_seven_eight, nine);
    }

    // Extended slices (step!=1) can neither grow nor shrink the sequence.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *empty = (*x_type)->newN(0);
        ypObject *two = (*x_type)->newN(1, items[2]);
        ypObject *three_four = (*x_type)->newN(2, items[3], items[4]);
        assert_raises_exc(yp_setsliceC6(self, 0, 0, 2, two, &exc), yp_ValueError);
        assert_sequence(self, 2, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(self, -1, -2, -1, empty, &exc), yp_ValueError);
        assert_sequence(self, 2, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(self, -1, -2, -1, three_four, &exc), yp_ValueError);
        assert_sequence(self, 2, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(self, -1, -3, -1, empty, &exc), yp_ValueError);
        assert_sequence(self, 2, items[0], items[1]);
        assert_raises_exc(yp_setsliceC6(self, -1, -3, -1, two, &exc), yp_ValueError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decrefN(4, self, empty, two, three_four);
    }

    // x can be self.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 0, 1, self, &exc));
        assert_sequence(self, 4, items[0], items[1], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 1, 4, 1, self, &exc));
        assert_sequence(self, 5, items[0], items[0], items[1], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, 0, 5, 1, self, &exc));
        assert_sequence(self, 5, items[0], items[0], items[1], items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(self, -1, -6, -1, self, &exc));
        assert_sequence(self, 5, items[1], items[0], items[1], items[0], items[0]);
        yp_decref(self);
    }

    // FIXME Test x being an iterator that fails at start, mid-way.
    // FIXME Test x being an iterator that lies about lenhint.

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_setitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support setitem.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_setitem(self, int_0, items[2], &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_setitem(self, int_0, items[2], &exc));
        assert_sequence(self, 2, items[2], items[1]);
        assert_not_raises_exc(yp_setitem(self, int_1, items[3], &exc));
        assert_sequence(self, 2, items[2], items[3]);
        yp_decref(self);
    }

    // Negative index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_setitem(self, int_neg_1, items[2], &exc));
        assert_sequence(self, 2, items[0], items[2]);
        assert_not_raises_exc(yp_setitem(self, int_neg_2, items[3], &exc));
        assert_sequence(self, 2, items[3], items[2]);
        yp_decref(self);
    }

    // Out of bounds.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_setitem(self, int_2, items[2], &exc), yp_IndexError);
        assert_raises_exc(yp_setitem(self, int_neg_3, items[3], &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *empty = type->newN(0);
        assert_raises_exc(yp_setitem(empty, int_0, items[2], &exc), yp_IndexError);
        assert_raises_exc(yp_setitem(empty, int_neg_1, items[3], &exc), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_setitem(self, int_SLICE_DEFAULT, items[2], &exc), yp_IndexError);
        assert_raises_exc(yp_setitem(self, int_SLICE_LAST, items[3], &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // FIXME non-integer indicies?

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_delindexC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support delindex.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delindexC(self, 0, &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic index.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delindexC(self, 0, &exc));
        assert_sequence(self, 2, items[1], items[2]);
        assert_not_raises_exc(yp_delindexC(self, 1, &exc));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Negative index.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delindexC(self, -1, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        assert_not_raises_exc(yp_delindexC(self, -2, &exc));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Out of bounds.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delindexC(self, 2, &exc), yp_IndexError);
        assert_raises_exc(yp_delindexC(self, -3, &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *empty = type->newN(0);
        assert_raises_exc(yp_delindexC(empty, 0, &exc), yp_IndexError);
        assert_raises_exc(yp_delindexC(empty, -1, &exc), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delindexC(self, yp_SLICE_DEFAULT, &exc), yp_IndexError);
        assert_raises_exc(yp_delindexC(self, yp_SLICE_LAST, &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_delsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(9, type->rand_item());

    // Immutables don't support delslice.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delsliceC5(self, 0, 1, 1, &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic slice.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self, 0, 1, 1, &exc));
        assert_sequence(self, 2, items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self, 1, 2, 1, &exc));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Negative step.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self, -1, -2, -1, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        assert_not_raises_exc(yp_delsliceC5(self, -2, -3, -1, &exc));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Total slice, forward and backward.
    {
        ypObject *self1 = type->newN(2, items[0], items[1]);
        ypObject *self2 = type->newN(2, items[2], items[3]);
        assert_not_raises_exc(yp_delsliceC5(self1, 0, 2, 1, &exc));
        assert_len(self1, 0);
        assert_not_raises_exc(yp_delsliceC5(self2, -1, -3, -1, &exc));
        assert_len(self2, 0);
        yp_decrefN(2, self1, self2);
    }

    // Step of 2, -2.
    {
        ypObject *self = type->newN(9, items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[8]);
        assert_not_raises_exc(yp_delsliceC5(self, 0, 9, 2, &exc));
        assert_sequence(self, 4, items[1], items[3], items[5], items[7]);
        assert_not_raises_exc(yp_delsliceC5(self, -1, -5, -2, &exc));
        assert_sequence(self, 2, items[1], items[5]);
        yp_decref(self);
    }

    // Empty slices.
    {
        ypObject    *self = type->newN(2, items[0], items[1]);
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
            assert_not_raises_exc(yp_delsliceC5(self, args.start, args.stop, args.step, &exc));
            assert_sequence(self, 2, items[0], items[1]);
        }
        yp_decref(self);
    }

    // yp_SLICE_DEFAULT.
    {
        ypObject *self1 = type->newN(4, items[0], items[1], items[2], items[3]);
        ypObject *self2 = type->newN(4, items[4], items[5], items[6], items[7]);
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_DEFAULT, 1, 1, &exc));
        assert_sequence(self1, 3, items[1], items[2], items[3]);
        assert_not_raises_exc(yp_delsliceC5(self1, 2, yp_SLICE_DEFAULT, 1, &exc));
        assert_sequence(self1, 2, items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1, &exc));
        assert_len(self1, 0);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_DEFAULT, -2, -1, &exc));
        assert_sequence(self2, 3, items[4], items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, -3, yp_SLICE_DEFAULT, -1, &exc));
        assert_sequence(self2, 2, items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1, &exc));
        assert_len(self2, 0);
        yp_decrefN(2, self1, self2);
    }

    // yp_SLICE_LAST.
    {
        ypObject *self1 = type->newN(4, items[0], items[1], items[2], items[3]);
        ypObject *self2 = type->newN(4, items[4], items[5], items[6], items[7]);
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_LAST, 4, 1, &exc));
        assert_sequence(self1, 4, items[0], items[1], items[2], items[3]);
        assert_not_raises_exc(yp_delsliceC5(self1, 3, yp_SLICE_LAST, 1, &exc));
        assert_sequence(self1, 3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self1, yp_SLICE_LAST, yp_SLICE_LAST, 1, &exc));
        assert_sequence(self1, 3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_LAST, -2, -1, &exc));
        assert_sequence(self2, 3, items[4], items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, -1, yp_SLICE_LAST, -1, &exc));
        assert_sequence(self2, 3, items[4], items[5], items[6]);
        assert_not_raises_exc(yp_delsliceC5(self2, yp_SLICE_LAST, yp_SLICE_LAST, -1, &exc));
        assert_sequence(self2, 3, items[4], items[5], items[6]);
        yp_decrefN(2, self1, self2);
    }

    // Invalid slices.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delsliceC5(self, 0, 1, 0, &exc), yp_ValueError);  // step==0
        assert_sequence(self, 2, items[0], items[1]);
        assert_raises_exc(yp_delsliceC5(self, 0, 1, -yp_SSIZE_T_MAX - 1, &exc),
                yp_SystemLimitationError);  // too-small step
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_delitemC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support delitem.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delitem(self, int_0, &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic index.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delitem(self, int_0, &exc));
        assert_sequence(self, 2, items[1], items[2]);
        assert_not_raises_exc(yp_delitem(self, int_1, &exc));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Negative index.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_delitem(self, int_neg_1, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        assert_not_raises_exc(yp_delitem(self, int_neg_2, &exc));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Out of bounds.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delitem(self, int_2, &exc), yp_IndexError);
        assert_raises_exc(yp_delitem(self, int_neg_3, &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *empty = type->newN(0);
        assert_raises_exc(yp_delitem(empty, int_0, &exc), yp_IndexError);
        assert_raises_exc(yp_delitem(empty, int_neg_1, &exc), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_delitem(self, int_SLICE_DEFAULT, &exc), yp_IndexError);
        assert_raises_exc(yp_delitem(self, int_SLICE_LAST, &exc), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

// Shared tests for yp_append, yp_push. (These are the same operation on sequences.)
static MunitResult _test_appendC(
        fixture_type_t *type, void (*any_append)(ypObject *, ypObject *, ypObject **))
{
    ypObject *items[] = obj_array_init(3, type->rand_item());

    // Immutables don't support append.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(any_append(self, items[2], &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic append.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(any_append(self, items[2], &exc));
        assert_sequence(self, 3, items[0], items[1], items[2]);
        yp_decref(self);
    }

    // Self is empty.
    {
        ypObject *self = type->newN(0);
        assert_not_raises_exc(any_append(self, items[2], &exc));
        assert_sequence(self, 1, items[2]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_append(const MunitParameter params[], fixture_t *fixture)
{
    return _test_appendC(fixture->type, yp_append);
}

static MunitResult test_push(const MunitParameter params[], fixture_t *fixture)
{
    return _test_appendC(fixture->type, yp_push);
}

static MunitResult test_extend(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support extend.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *two = type->newN(1, items[2]);
        assert_raises_exc(yp_extend(self, two, &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decrefN(2, self, two);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic extend.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *x = (*x_type)->newN(2, items[2], items[3]);
        assert_not_raises_exc(yp_extend(self, x, &exc));
        assert_sequence(self, 4, items[0], items[1], items[2], items[3]);
        yp_decrefN(2, self, x);
    }

    // self is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(0);
        ypObject *x = (*x_type)->newN(2, items[2], items[3]);
        assert_not_raises_exc(yp_extend(self, x, &exc));
        assert_sequence(self, 2, items[2], items[3]);
        yp_decrefN(2, self, x);
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_extend(self, x, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        yp_decrefN(2, self, x);
    }

    // Both are empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(0);
        ypObject *x = (*x_type)->newN(0);
        assert_not_raises_exc(yp_extend(self, x, &exc));
        assert_len(self, 0);
        yp_decrefN(2, self, x);
    }

    // x can be self.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_extend(self, self, &exc));
        assert_sequence(self, 4, items[0], items[1], items[0], items[1]);
        yp_decref(self);
    }

    // FIXME Test x being an iterator that fails at start, mid-way.
    // FIXME Test x being an iterator that lies about lenhint.

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_irepeatC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(2, type->rand_item());

    // Immutables don't support irepeat.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_irepeatC(self, 2, &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic irepeat.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_irepeatC(self, 2, &exc));
        assert_sequence(self, 4, items[0], items[1], items[0], items[1]);
        yp_decref(self);
    }

    // Factor of one.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_irepeatC(self, 1, &exc));
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // Factor of zero.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_irepeatC(self, 0, &exc));
        assert_len(self, 0);
        yp_decref(self);
    }

    // Negative factor.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_irepeatC(self, -1, &exc));
        assert_len(self, 0);
        yp_decref(self);
    }

    // Large factor. (Exercises _ypSequence_repeat_memcpy optimization.).
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_irepeatC(self, 8, &exc));
        assert_sequence(self, 16, items[0], items[1], items[0], items[1], items[0], items[1],
                items[0], items[1], items[0], items[1], items[0], items[1], items[0], items[1],
                items[0], items[1]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *self = type->newN(0);
        assert_not_raises_exc(yp_irepeatC(self, 2, &exc));
        assert_len(self, 0);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_insertC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support insert.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_insertC(self, 0, items[2], &exc), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic insert.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_insertC(self, 0, items[2], &exc));
        assert_sequence(self, 3, items[2], items[0], items[1]);
        assert_not_raises_exc(yp_insertC(self, 1, items[3], &exc));
        assert_sequence(self, 4, items[2], items[3], items[0], items[1]);
        yp_decref(self);
    }

    // Negative index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_insertC(self, -1, items[2], &exc));
        assert_sequence(self, 3, items[0], items[2], items[1]);
        assert_not_raises_exc(yp_insertC(self, -2, items[3], &exc));
        assert_sequence(self, 4, items[0], items[3], items[2], items[1]);
        yp_decref(self);
    }

    // "Out of bounds": recall s.insert(i, x) is the same as s[i:i] = [x].
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_insertC(self, 2, items[2], &exc));
        assert_sequence(self, 3, items[0], items[1], items[2]);
        assert_not_raises_exc(yp_insertC(self, -4, items[3], &exc));
        assert_sequence(self, 4, items[3], items[0], items[1], items[2]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *self1 = type->newN(0);
        ypObject *self2 = type->newN(0);
        assert_not_raises_exc(yp_insertC(self1, 0, items[2], &exc));
        assert_sequence(self1, 1, items[2]);
        assert_not_raises_exc(yp_insertC(self2, -1, items[3], &exc));
        assert_sequence(self2, 1, items[3]);
        yp_decrefN(2, self1, self2);
    }

    // yp_SLICE_DEFAULT. Recall in slices that yp_SLICE_DEFAULT is equivalent to None (or omitting)
    // in Python. Since s.insert(None, '') raises TypeError, that seems correct here too.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises_exc(yp_insertC(self, yp_SLICE_DEFAULT, items[2], &exc), yp_TypeError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_not_raises_exc(yp_insertC(self, yp_SLICE_LAST, items[2], &exc));
        assert_sequence(self, 3, items[0], items[1], items[2]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_popindexC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(4, type->rand_item());

    // Immutables don't support popindex.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises(yp_popindexC(self, 0), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic popindex.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        ead(popped, yp_popindexC(self, 0), assert_obj(popped, eq, items[0]));
        assert_sequence(self, 2, items[1], items[2]);
        ead(popped, yp_popindexC(self, 1), assert_obj(popped, eq, items[2]));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Negative index.
    {
        ypObject *self = type->newN(3, items[0], items[1], items[2]);
        ead(popped, yp_popindexC(self, -1), assert_obj(popped, eq, items[2]));
        assert_sequence(self, 2, items[0], items[1]);
        ead(popped, yp_popindexC(self, -2), assert_obj(popped, eq, items[0]));
        assert_sequence(self, 1, items[1]);
        yp_decref(self);
    }

    // Out of bounds.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises(yp_popindexC(self, 2), yp_IndexError);
        assert_raises(yp_popindexC(self, -3), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

    // Empty self.
    {
        ypObject *empty = type->newN(0);
        assert_raises(yp_popindexC(empty, 0), yp_IndexError);
        assert_raises(yp_popindexC(empty, -1), yp_IndexError);
        assert_len(empty, 0);
        yp_decref(empty);
    }

    // yp_SLICE_DEFAULT, yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises(yp_popindexC(self, yp_SLICE_DEFAULT), yp_IndexError);
        assert_raises(yp_popindexC(self, yp_SLICE_LAST), yp_IndexError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_pop(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[] = obj_array_init(2, type->rand_item());

    // Immutables don't support pop.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        assert_raises(yp_pop(self), yp_MethodError);
        assert_sequence(self, 2, items[0], items[1]);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic pop.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ead(popped, yp_pop(self), assert_obj(popped, eq, items[1]));
        assert_sequence(self, 1, items[0]);
        ead(popped, yp_pop(self), assert_obj(popped, eq, items[0]));
        assert_len(self, 0);
        yp_decref(self);
    }

    // Self is empty.
    {
        ypObject *self = type->newN(0);
        assert_raises(yp_pop(self), yp_IndexError);
        assert_len(self, 0);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
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
        TEST(test_delsliceC, test_sequence_params), TEST(test_delitemC, test_sequence_params),
        TEST(test_append, test_sequence_params), TEST(test_push, test_sequence_params),
        TEST(test_extend, test_sequence_params), TEST(test_irepeatC, test_sequence_params),
        TEST(test_insertC, test_sequence_params), TEST(test_popindexC, test_sequence_params),
        TEST(test_pop, test_sequence_params), {NULL}};


extern void test_sequence_initialize(void) {}

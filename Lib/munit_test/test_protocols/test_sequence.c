
#include "munit_test/unittest.h"


typedef struct _slice_args_t {
    yp_ssize_t start;
    yp_ssize_t stop;
    yp_ssize_t step;
} slice_args_t;


static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    // Sequences should concatenate with themselves, their pairs, and iterators.
    fixture_type_t  *x_types[] = {type, type->pair, fixture_type_iter, NULL};
    fixture_type_t  *friend_types[] = {type, type->pair, NULL};
    fixture_type_t **x_type;
    obj_array_init(items, 4, type->rand_item());

    // range stores integers following a pattern, so doesn't support concat.
    if (type->is_patterned) {
        ypObject *self = rand_obj(type);
        ypObject *result = yp_concat(self, self);
        assert_isexception2(result, yp_MethodError);
        yp_decrefN(2, self, result);
        goto tear_down;  // Skip the remaining tests.
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

tear_down:
    obj_array_fini(items);
    return MUNIT_OK;
}

static MunitResult test_repeatC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    obj_array_init(items, 2, type->rand_item());

    // range stores integers following a pattern, so doesn't support repeat.
    if (type->is_patterned) {
        ypObject *self = rand_obj(type);
        ypObject *result = yp_repeatC(self, 2);
        assert_isexception2(result, yp_MethodError);
        yp_decrefN(2, self, result);
        goto tear_down;  // Skip the remaining tests.
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
    obj_array_fini(items);
    return MUNIT_OK;
}

static MunitResult test_getindexC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    obj_array_init(items, 2, type->rand_item());

    // Basic index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *zero = yp_getindexC(self, 0);
        ypObject *one = yp_getindexC(self, 1);
        assert_obj(zero, eq, items[0]);
        assert_obj(one, eq, items[1]);
        yp_decrefN(3, self, zero, one);
    }

    // Negative index.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *neg_one = yp_getindexC(self, -1);
        ypObject *neg_two = yp_getindexC(self, -2);
        assert_obj(neg_one, eq, items[1]);
        assert_obj(neg_two, eq, items[0]);
        yp_decrefN(3, self, neg_one, neg_two);
    }

    // Out of bounds.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *two = yp_getindexC(self, 2);
        ypObject *neg_three = yp_getindexC(self, -3);
        assert_isexception2(two, yp_IndexError);
        assert_isexception2(neg_three, yp_IndexError);
        yp_decrefN(3, self, two, neg_three);
    }

    // Empty self.
    {
        ypObject *self = type->newN(0);
        ypObject *zero = yp_getindexC(self, 0);
        ypObject *neg_one = yp_getindexC(self, -1);
        assert_isexception2(zero, yp_IndexError);
        assert_isexception2(neg_one, yp_IndexError);
        yp_decrefN(3, self, zero, neg_one);
    }

    obj_array_fini(items);
    return MUNIT_OK;
}

static MunitResult test_getsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    obj_array_init(items, 5, type->rand_item());

    // Basic slice.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *zero_one = yp_getsliceC4(self, 0, 1, 1);
        ypObject *one_two = yp_getsliceC4(self, 1, 2, 1);
        assert_type_is(zero_one, type->type);
        assert_sequence(zero_one, 1, items[0]);
        assert_type_is(one_two, type->type);
        assert_sequence(one_two, 1, items[1]);
        yp_decrefN(3, self, zero_one, one_two);
    }

    // Total slice, forward and backward.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *forward = yp_getsliceC4(self, 0, 2, 1);
        ypObject *reverse = yp_getsliceC4(self, -1, -3, -1);
        assert_type_is(forward, type->type);
        assert_sequence(forward, 2, items[0], items[1]);
        assert_type_is(reverse, type->type);
        assert_sequence(reverse, 2, items[1], items[0]);
        yp_decrefN(3, self, forward, reverse);
    }

    // Step of 2, -2.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *forward = yp_getsliceC4(self, 0, 2, 2);
        ypObject *reverse = yp_getsliceC4(self, -1, -3, -2);
        assert_type_is(forward, type->type);
        assert_sequence(forward, 1, items[0]);
        assert_type_is(reverse, type->type);
        assert_sequence(reverse, 1, items[1]);
        yp_decrefN(3, self, forward, reverse);
    }

    // Empty slices.
    {
        ypObject    *self = type->newN(2, items[0], items[1]);
        slice_args_t slices[] = {
                {0, 0, 1},      // typical empty slice
                {2, 99, 1},     // if i>=len(s) and k>0, the slice is empty, regardless of j
                {-3, -99, -1},  // if i<-len(s) and k<0, the slice is empty, regardless of j
                {99, 2, -1},    // if j>=len(s) and k<0, the slice is empty, regardless of i
                {-99, -3, 1},   // if j<-len(s) and k>0, the slice is empty, regardless of i
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            ypObject    *empty = yp_getsliceC4(self, args.start, args.stop, args.step);
            assert_type_is(empty, type->type);
            assert_len(empty, 0);
            yp_decref(empty);
        }
        yp_decref(self);
    }

    // yp_SLICE_DEFAULT.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *i_pos_step = yp_getsliceC4(self, yp_SLICE_DEFAULT, 2, 1);
        ypObject *j_pos_step = yp_getsliceC4(self, 0, yp_SLICE_DEFAULT, 1);
        ypObject *both_pos_step = yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1);
        ypObject *i_neg_step = yp_getsliceC4(self, yp_SLICE_DEFAULT, -3, -1);
        ypObject *j_neg_step = yp_getsliceC4(self, -1, yp_SLICE_DEFAULT, -1);
        ypObject *both_neg_step = yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1);
        assert_sequence(i_pos_step, 2, items[0], items[1]);
        assert_sequence(j_pos_step, 2, items[0], items[1]);
        assert_sequence(both_pos_step, 2, items[0], items[1]);
        assert_sequence(i_neg_step, 2, items[1], items[0]);
        assert_sequence(j_neg_step, 2, items[1], items[0]);
        assert_sequence(both_neg_step, 2, items[1], items[0]);
        yp_decrefN(7, self, i_pos_step, j_pos_step, both_pos_step, i_neg_step, j_neg_step,
                both_neg_step);
    }

    // yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *i_pos_step = yp_getsliceC4(self, yp_SLICE_LAST, 2, 1);
        ypObject *j_pos_step = yp_getsliceC4(self, 0, yp_SLICE_LAST, 1);
        ypObject *both_pos_step = yp_getsliceC4(self, yp_SLICE_LAST, yp_SLICE_LAST, 1);
        ypObject *i_neg_step = yp_getsliceC4(self, yp_SLICE_LAST, -3, -1);
        ypObject *j_neg_step = yp_getsliceC4(self, -1, yp_SLICE_LAST, -1);
        ypObject *both_neg_step = yp_getsliceC4(self, yp_SLICE_LAST, yp_SLICE_LAST, -1);
        assert_len(i_pos_step, 0);
        assert_sequence(j_pos_step, 2, items[0], items[1]);
        assert_len(both_pos_step, 0);
        assert_sequence(i_neg_step, 2, items[1], items[0]);
        assert_len(j_neg_step, 0);
        assert_len(both_neg_step, 0);
        yp_decrefN(7, self, i_pos_step, j_pos_step, both_pos_step, i_neg_step, j_neg_step,
                both_neg_step);
    }

    // Optimization: lazy shallow copy of an immutable self for total forward slice.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *forward = yp_getsliceC4(self, 0, 2, 1);
        assert_obj(forward, is, self);
        yp_decrefN(2, self, forward);
    }

    // Optimization: empty immortal when slice is empty.
    if (type->falsy != NULL) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ypObject *empty = yp_getsliceC4(self, 0, 0, 1);
        assert_obj(empty, is, type->falsy);
        yp_decrefN(2, self, empty);
    }

// Python's test_getslice. Patterned sequences like range only support newN(2, ...).
// FIXME Add a similar test for test_range.c!
// FIXME Is this ead_slice a good idea? Is this the best way to do it? Playing with macros is
// playing with fire.
#define ead_slice(args, assertion) ead(slice, yp_getsliceC4 args, assert_##assertion)
    if (!type->is_patterned) {
        ypObject *self = type->newN(5, items[0], items[1], items[2], items[3], items[4]);
        ead_slice((self, 0, 0, 1), len(slice, 0));
        ead_slice((self, 1, 2, 1), sequence(slice, 1, items[1]));
        ead_slice((self, -2, -1, 1), sequence(slice, 1, items[3]));
        ead_slice((self, -1000, 1000, 1), obj(slice, eq, self));
        ead_slice((self, 1000, -1000, 1), len(slice, 0));
        ead_slice((self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1), obj(slice, eq, self));
        ead_slice((self, 1, yp_SLICE_DEFAULT, 1),
                sequence(slice, 4, items[1], items[2], items[3], items[4]));
        ead_slice((self, yp_SLICE_DEFAULT, 3, 1), sequence(slice, 3, items[0], items[1], items[2]));

        ead_slice((self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 2),
                sequence(slice, 3, items[0], items[2], items[4]));
        ead_slice((self, 1, yp_SLICE_DEFAULT, 2), sequence(slice, 2, items[1], items[3]));
        ead_slice((self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1),
                sequence(slice, 5, items[4], items[3], items[2], items[1], items[0]));
        ead_slice((self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -2),
                sequence(slice, 3, items[4], items[2], items[0]));
        ead_slice((self, 3, yp_SLICE_DEFAULT, -2), sequence(slice, 2, items[3], items[1]));
        ead_slice((self, 3, 3, -2), len(slice, 0));
        ead_slice((self, 3, 2, -2), sequence(slice, 1, items[3]));
        ead_slice((self, 3, 1, -2), sequence(slice, 1, items[3]));
        ead_slice((self, 3, 0, -2), sequence(slice, 2, items[3], items[1]));
        ead_slice((self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -100), sequence(slice, 1, items[4]));
        ead_slice((self, 100, -100, 1), len(slice, 0));
        ead_slice((self, -100, 100, 1), obj(slice, eq, self));
        ead_slice((self, 100, -100, -1),
                sequence(slice, 5, items[4], items[3], items[2], items[1], items[0]));
        ead_slice((self, -100, 100, -1), len(slice, 0));
        ead_slice((self, -100, 100, 2), sequence(slice, 3, items[0], items[2], items[4]));
        yp_decref(self);
    }
#undef ead_slice

    obj_array_fini(items);
    return MUNIT_OK;
}


static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params),
        TEST(test_repeatC, test_sequence_params), TEST(test_getindexC, test_sequence_params),
        TEST(test_getsliceC, test_sequence_params), {NULL}};

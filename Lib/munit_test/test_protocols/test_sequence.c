
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
    ypObject *self = type->newN(2, items[0], items[1]);
    ypObject *empty = type->newN(0);

    // Basic index.
    ead(zero, yp_getindexC(self, 0), assert_obj(zero, eq, items[0]));
    ead(one, yp_getindexC(self, 1), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, yp_getindexC(self, -1), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, yp_getindexC(self, -2), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    ead(two, yp_getindexC(self, 2), assert_isexception2(two, yp_IndexError));
    ead(neg_three, yp_getindexC(self, -3), assert_isexception2(neg_three, yp_IndexError));

    // Empty self.
    ead(zero, yp_getindexC(empty, 0), assert_isexception2(zero, yp_IndexError));
    ead(neg_one, yp_getindexC(empty, -1), assert_isexception2(neg_one, yp_IndexError));

    obj_array_fini(items);
    yp_decrefN(2, self, empty);
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
        ead(i_pos_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, 2, 1),
                assert_sequence(i_pos_step, 2, items[0], items[1]));
        ead(j_pos_step, yp_getsliceC4(self, 0, yp_SLICE_DEFAULT, 1),
                assert_sequence(j_pos_step, 2, items[0], items[1]));
        ead(both_pos_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1),
                assert_sequence(both_pos_step, 2, items[0], items[1]));
        ead(i_neg_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, -3, -1),
                assert_sequence(i_neg_step, 2, items[1], items[0]));
        ead(j_neg_step, yp_getsliceC4(self, -1, yp_SLICE_DEFAULT, -1),
                assert_sequence(j_neg_step, 2, items[1], items[0]));
        ead(both_neg_step, yp_getsliceC4(self, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1),
                assert_sequence(both_neg_step, 2, items[1], items[0]));
        yp_decref(self);
    }

    // yp_SLICE_LAST.
    {
        ypObject *self = type->newN(2, items[0], items[1]);
        ead(i_pos_step, yp_getsliceC4(self, yp_SLICE_LAST, 2, 1), assert_len(i_pos_step, 0));
        ead(j_pos_step, yp_getsliceC4(self, 0, yp_SLICE_LAST, 1),
                assert_sequence(j_pos_step, 2, items[0], items[1]));
        ead(both_pos_step, yp_getsliceC4(self, yp_SLICE_LAST, yp_SLICE_LAST, 1),
                assert_len(both_pos_step, 0));
        ead(i_neg_step, yp_getsliceC4(self, yp_SLICE_LAST, -3, -1),
                assert_sequence(i_neg_step, 2, items[1], items[0]));
        ead(j_neg_step, yp_getsliceC4(self, -1, yp_SLICE_LAST, -1), assert_len(j_neg_step, 0));
        ead(both_neg_step, yp_getsliceC4(self, yp_SLICE_LAST, yp_SLICE_LAST, -1),
                assert_len(both_neg_step, 0));
        yp_decref(self);
    }

    // Optimization: lazy shallow copy of an immutable self for total forward slice.
    if (!type->is_mutable) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ead(forward, yp_getsliceC4(self, 0, 2, 1), assert_obj(forward, is, self));
        yp_decref(self);
    }

    // Optimization: empty immortal when slice is empty.
    if (type->falsy != NULL) {
        ypObject *self = type->newN(2, items[0], items[1]);
        ead(empty, yp_getsliceC4(self, 0, 0, 1), assert_obj(empty, is, type->falsy));
        yp_decref(self);
    }

    // Python's test_getslice. Patterned sequences like range only support newN(2, ...).
    // FIXME Add a similar test for test_range.c!
    if (!type->is_patterned) {
        ypObject *self = type->newN(5, items[0], items[1], items[2], items[3], items[4]);
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
    }

    obj_array_fini(items);
    return MUNIT_OK;
}

static MunitResult test_getitem(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    obj_array_init(items, 2, type->rand_item());
    ypObject *self = type->newN(2, items[0], items[1]);
    ypObject *empty = type->newN(0);

    // Basic index.
    ead(zero, yp_getitem(self, int_0), assert_obj(zero, eq, items[0]));
    ead(one, yp_getitem(self, int_1), assert_obj(one, eq, items[1]));

    // Negative index.
    ead(neg_one, yp_getitem(self, int_neg_1), assert_obj(neg_one, eq, items[1]));
    ead(neg_two, yp_getitem(self, int_neg_2), assert_obj(neg_two, eq, items[0]));

    // Out of bounds.
    ead(two, yp_getitem(self, int_2), assert_isexception2(two, yp_IndexError));
    ead(neg_three, yp_getitem(self, int_neg_3), assert_isexception2(neg_three, yp_IndexError));

    // Empty self.
    ead(zero, yp_getitem(empty, int_0), assert_isexception2(zero, yp_IndexError));
    ead(neg_one, yp_getitem(empty, int_neg_1), assert_isexception2(neg_one, yp_IndexError));

    obj_array_fini(items);
    yp_decrefN(2, self, empty);
    return MUNIT_OK;
}


static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params),
        TEST(test_repeatC, test_sequence_params), TEST(test_getindexC, test_sequence_params),
        TEST(test_getsliceC, test_sequence_params), {NULL}};


extern void test_sequence_initialize(void) {}

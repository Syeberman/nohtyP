
#include "munit_test/unittest.h"


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
        ypObject *obj = rand_obj(type);
        ypObject *result = yp_concat(obj, obj);
        assert_isexception2(result, yp_MethodError);
        yp_decrefN(2, obj, result);
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


static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params), {NULL}};

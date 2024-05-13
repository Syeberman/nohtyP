
#include "munit_test/unittest.h"


// FIXME yp_iter_items, yp_iter_keys, yp_iter_values

// TODO More test cases are needed here.
static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // miniiters created with a specific number of items should have exact length hints.
    {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = yp_miniiter(x, &mi_state);

        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 2);
        ead(next, yp_miniiter_next(mi, &mi_state), assert_not_raises(next));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 1);
        ead(next, yp_miniiter_next(mi, &mi_state), assert_not_raises(next));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        ead(next, yp_miniiter_next(mi, &mi_state), assert_raises(next, yp_StopIteration));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);

        yp_decrefN(N(x, mi));
    }

    obj_array_decref(items);
    return MUNIT_OK;
}


static MunitParameterEnum test_iterable_params[] = {
        {param_key_type, param_values_types_iterable}, {NULL}};

MunitTest test_iterable_tests[] = {TEST(test_miniiter, test_iterable_params), {NULL}};

extern void test_iterable_initialize(void) {}

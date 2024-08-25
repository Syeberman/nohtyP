
#include "munit_test/unittest.h"


// FIXME yp_iter_items, yp_iter_keys, yp_iter_values

// TODO Ensure yp_iter2/yp_max_keyN/etc properly handles exception passthrough, even in cases where
// one of the arguments would be ignored.

// TODO Generally, for exception passthrough with iters, ensure that no items are yielded from the
// iter if one of the arguments is an exception (e.g. yp_filter where function is an exception,
// yp_max_key, yp_sorted3, yp_zipN, etc). ALTHOUGH! This is different than some optimizations where
// we allow modifications before checking for errors: the object is partially modified in these
// cases, so should iters behave like this? ALTHOUGH! exception passthrough is something we are
// providing specifically so as to allow for code to safely ignore exceptions temporarily, so it
// should probably be treated differently. WHICH MEANS! That perhaps some other tests should be
// rethought so that they don't allow partial updates in exception passthrough cases.


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


#include "munit_test/unittest.h"


// TODO yp_iter_items, yp_iter_keys, yp_iter_values only applies to mapping objects.

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

// TODO Include original_object_return tests.


// TODO More test cases are needed here.
static void _test_miniiter(
        fixture_type_t *type, ypObject *(*any_miniiter)(ypObject *, yp_uint64_t *))
{
    ypObject *items[2];
    obj_array_fill(items, type->rand_items);

    // miniiters created with a specific number of items should have exact length hints.
    {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = any_miniiter(x, &mi_state);

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
}

static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    _test_miniiter(fixture->type, yp_miniiter);
    return MUNIT_OK;
}

static MunitResult test_miniiter_keys(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Only mapping types support yp_miniiter_keys.
    if (!type->is_mapping) {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        assert_raises(yp_miniiter_keys(x, &mi_state), yp_MethodError);
        yp_decref(x);
        goto tear_down;
    }

    // Shared tests.
    _test_miniiter(fixture->type, yp_miniiter_keys);

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_miniiter_values(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Only mapping types support yp_miniiter_values.
    if (!type->is_mapping) {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        assert_raises(yp_miniiter_values(x, &mi_state), yp_MethodError);
        yp_decref(x);
        goto tear_down;
    }

    // Shared tests.
    _test_miniiter(fixture->type, yp_miniiter_values);

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult test_miniiter_items(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Only mapping types support yp_miniiter_items.
    if (!type->is_mapping) {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        assert_raises(yp_miniiter_items(x, &mi_state), yp_MethodError);
        yp_decref(x);
        goto tear_down;
    }

    // Shared tests.
    _test_miniiter(fixture->type, yp_miniiter_items);

tear_down:
    obj_array_decref(items);
    return MUNIT_OK;
}


static MunitParameterEnum test_iterable_params[] = {
        {param_key_type, param_values_types_iterable}, {NULL}};

MunitTest test_iterable_tests[] = {TEST(test_miniiter, test_iterable_params),
        TEST(test_miniiter_keys, test_iterable_params),
        TEST(test_miniiter_values, test_iterable_params),
        TEST(test_miniiter_items, test_iterable_params), {NULL}};

extern void test_iterable_initialize(void) {}

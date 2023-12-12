
#include "munit_test/unittest.h"


// FIXME Constructor tests.

static MunitResult _test_new(fixture_type_t *type, ypObject *(*any_new)(ypObject *), ypObject *(*any_newN)(int, ...)) {
    // FIXME Tests all the constructors.
    return MUNIT_OK;
}

static MunitResult _test_call_type(fixture_type_t *type) {
    // FIXME Tests yp_call on the type object.
    return MUNIT_OK;
}

// frozenset- and set-specific miniiter tests; see test_iterable for more tests.
static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Corrupted states.
    {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = yp_miniiter(x, &mi_state);

        mi_state = (yp_uint64_t)-1;
        ead(next, yp_miniiter_next(mi, &mi_state), assert_raises(next, yp_StopIteration));
        // FIXME Test this state with the other miniiter functions; recall state may be modified!

        yp_decrefN(N(x, mi));
    }

    obj_array_decref(items);
    return MUNIT_OK;
}


char *param_values_test_frozenset[] = {"frozenset", "set", NULL};

static MunitParameterEnum test_frozenset_params[] = {
        {param_key_type, param_values_test_frozenset}, {NULL}};

MunitTest test_frozenset_tests[] = {TEST(test_miniiter, test_frozenset_params), {NULL}};


extern void test_frozenset_initialize(void) {}

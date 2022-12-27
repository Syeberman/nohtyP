
#include "munit_test/unittest.h"


// TODO This could be a protocol test.
static MunitResult test_remove(const MunitParameter params[], fixture_t *fixture)
{
    // Remove an item from a set. The hash should equal a "clean" set. This was a bug from the
    // implementation of the yp_HashSet functions.
    {
        ypObject *expected = yp_frozensetN(N(int_0));
        yp_hash_t expected_hash;
        ypObject *set = yp_setN(N(int_0, int_1));
        yp_hash_t set_hash;

        assert_not_raises_exc(yp_remove(set, int_1, &exc));

        // FIXME Make an assert_hashes_equal?
        assert_not_raises_exc(expected_hash = yp_currenthashC(expected, &exc));
        assert_not_raises_exc(set_hash = yp_currenthashC(set, &exc));
        assert_hashC(expected_hash, ==, set_hash);

        yp_decrefN(N(expected, set));
    }

    return MUNIT_OK;
}


// char *param_values_test_objects_set[] = {"frozenset", "set"};

// static MunitParameterEnum test_objects_set_params[] = {
//         {param_key_type, param_values_test_objects_set}, {NULL}};

MunitTest test_frozenset_tests[] = {TEST(test_remove, NULL), {NULL}};


extern void test_frozenset_initialize(void) {}

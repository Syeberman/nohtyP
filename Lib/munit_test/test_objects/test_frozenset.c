
#include "munit_test/unittest.h"


// TODO This could be a protocol test.
static MunitResult test_remove(const MunitParameter params[], fixture_t *fixture)
{
    // Remove an item from a set. The hash should equal a "clean" set. This was a bug from the
    // implementation of the yp_HashSet functions.
    {
        ypObject *expected = yp_frozensetN(1, yp_i_zero);
        yp_hash_t expected_hash;
        ypObject *set = yp_setN(2, yp_i_zero, yp_i_one);
        yp_hash_t set_hash;

        assert_not_raises_exc(yp_remove(set, yp_i_one, &exc));

        // FIXME Make an assert_hashes_equal?
        assert_not_raises_exc(expected_hash = yp_currenthashC(expected, &exc));
        assert_not_raises_exc(set_hash = yp_currenthashC(set, &exc));
        assert_hashC(expected_hash, set_hash);

        yp_decrefN(2, expected, set);
    }

    return MUNIT_OK;
}


// char *param_values_test_objects_set[] = {"frozenset", "set"};

// static MunitParameterEnum test_objects_set_params[] = {
//         {param_key_type, param_values_test_objects_set}, {NULL}};

MunitTest test_frozenset_tests[] = {TEST(test_remove, NULL), {NULL}};
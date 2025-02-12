#include "munit_test/unittest.h"

// TODO Ensure yp_max_key/etc properly handles exception passthrough, even in cases where
// one of the arguments would be ignored.


static MunitResult test_max_key(const MunitParameter params[], fixture_t *fixture)
{
    // yp_max_key is not yet implemented.
    {
        ypObject *x = rand_obj_any(NULL);
        assert_raises(yp_max_key(x, yp_None), yp_NotImplementedError);
        yp_decrefN(N(x));
    }

    return MUNIT_OK;
}

static MunitResult test_min_key(const MunitParameter params[], fixture_t *fixture)
{
    // yp_min_key is not yet implemented.
    {
        ypObject *x = rand_obj_any(NULL);
        assert_raises(yp_min_key(x, yp_None), yp_NotImplementedError);
        yp_decrefN(N(x));
    }

    return MUNIT_OK;
}

static MunitResult test_max(const MunitParameter params[], fixture_t *fixture)
{
    // yp_max is not yet implemented.
    {
        ypObject *x = rand_obj_any(NULL);
        assert_raises(yp_max(x), yp_NotImplementedError);
        yp_decrefN(N(x));
    }

    return MUNIT_OK;
}

static MunitResult test_min(const MunitParameter params[], fixture_t *fixture)
{
    // yp_min is not yet implemented.
    {
        ypObject *x = rand_obj_any(NULL);
        assert_raises(yp_min(x), yp_NotImplementedError);
        yp_decrefN(N(x));
    }

    return MUNIT_OK;
}

// TODO This should list all comparable types.
#define test_comparable_params NULL

MunitTest test_comparable_tests[] = {TEST(test_max_key, test_comparable_params),
        TEST(test_min_key, test_comparable_params), TEST(test_max, test_comparable_params),
        TEST(test_min, test_comparable_params), {NULL}};

extern void test_comparable_initialize(void) {}

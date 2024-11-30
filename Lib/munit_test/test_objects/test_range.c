
#include "munit_test/unittest.h"


// Shared tests for yp_contains, yp_findC, yp_countC, etc. test_collection and test_sequence check
// for the behaviour shared amongst all sequences; this _test_contains considers the behaviour
// unique to range, namely all the various ways an item can _not_ be in a range. We don't test
// yp_findC5/etc because the interesting case, namely where an item is part of the sequence but not
// the slice, is already tested in test_sequence.
static void _test_contains(fixture_type_t *type, ypObject *(*any_contains)(ypObject *, ypObject *))
{
    // "first" and "last" are named assuming a positive step; flip them for negative steps. last
    // is inclusive, so use last+1 for the end.
    yp_int_t   first = (yp_int_t)((yp_int32_t)munit_rand_uint32());
    ypObject  *i_first = yp_intC(first);
    ypObject  *i_first_p1 = yp_intC(first + 1);
    ypObject  *i_first_m1 = yp_intC(first - 1);
    yp_int_t   multi_step = (yp_int_t)munit_rand_int_range(2, 128);
    yp_ssize_t multi_step_len = (yp_ssize_t)munit_rand_int_range(3, 128);
    yp_int_t   last = first + (multi_step * multi_step_len);
    ypObject  *i_last = yp_intC(last);
    ypObject  *i_last_p1 = yp_intC(last + 1);
    ypObject  *i_last_m1 = yp_intC(last - 1);
    yp_ssize_t multi_step_middle_idx = (yp_ssize_t)munit_rand_int_range(1, (int)multi_step_len);
    yp_int_t   middle = first + (multi_step * multi_step_middle_idx);
    ypObject  *i_middle = yp_intC(middle);
    ypObject  *i_middle_p1 = yp_intC(middle + 1);
    ypObject  *i_middle_m1 = yp_intC(middle - 1);

    // Empty range.
    {
        ypObject *r = yp_rangeC3(first, first, 1);

        assert_obj(any_contains(r, i_first), is, yp_False);
        assert_obj(any_contains(r, i_first_p1), is, yp_False);
        assert_obj(any_contains(r, i_first_m1), is, yp_False);

        yp_decrefN(N(r));
    }

    // One item.
    {
        ypObject *r = yp_rangeC3(first, first + 1, 1);

        assert_obj(any_contains(r, i_first), is, yp_True);
        assert_obj(any_contains(r, i_first_p1), is, yp_False);
        assert_obj(any_contains(r, i_first_m1), is, yp_False);

        yp_decrefN(N(r));
    }

    // Three or more items, step 1.
    {
        ypObject *r = yp_rangeC3(first, last + 1, 1);

        assert_obj(any_contains(r, i_first), is, yp_True);
        assert_obj(any_contains(r, i_first_p1), is, yp_True);
        assert_obj(any_contains(r, i_first_m1), is, yp_False);

        assert_obj(any_contains(r, i_last), is, yp_True);
        assert_obj(any_contains(r, i_last_p1), is, yp_False);
        assert_obj(any_contains(r, i_last_m1), is, yp_True);

        assert_obj(any_contains(r, i_middle), is, yp_True);
        assert_obj(any_contains(r, i_middle_p1), is, yp_True);
        assert_obj(any_contains(r, i_middle_m1), is, yp_True);

        yp_decrefN(N(r));
    }

    // Three or more items, step -1. "first" and "last" are flipped.
    {
        ypObject *r = yp_rangeC3(last, first - 1, -1);

        assert_obj(any_contains(r, i_first), is, yp_True);
        assert_obj(any_contains(r, i_first_p1), is, yp_True);
        assert_obj(any_contains(r, i_first_m1), is, yp_False);

        assert_obj(any_contains(r, i_last), is, yp_True);
        assert_obj(any_contains(r, i_last_p1), is, yp_False);
        assert_obj(any_contains(r, i_last_m1), is, yp_True);

        assert_obj(any_contains(r, i_middle), is, yp_True);
        assert_obj(any_contains(r, i_middle_p1), is, yp_True);
        assert_obj(any_contains(r, i_middle_m1), is, yp_True);

        yp_decrefN(N(r));
    }

    // Three or more items, step >1.
    {
        ypObject *r = yp_rangeC3(first, last + 1, multi_step);

        assert_obj(any_contains(r, i_first), is, yp_True);
        assert_obj(any_contains(r, i_first_p1), is, yp_False);
        assert_obj(any_contains(r, i_first_m1), is, yp_False);

        assert_obj(any_contains(r, i_last), is, yp_True);
        assert_obj(any_contains(r, i_last_p1), is, yp_False);
        assert_obj(any_contains(r, i_last_m1), is, yp_False);

        assert_obj(any_contains(r, i_middle), is, yp_True);
        assert_obj(any_contains(r, i_middle_p1), is, yp_False);
        assert_obj(any_contains(r, i_middle_m1), is, yp_False);

        yp_decrefN(N(r));
    }

    // Three or more items, step <-1. "first" and "last" are flipped.
    {
        ypObject *r = yp_rangeC3(last, first - 1, -multi_step);

        assert_obj(any_contains(r, i_first), is, yp_True);
        assert_obj(any_contains(r, i_first_p1), is, yp_False);
        assert_obj(any_contains(r, i_first_m1), is, yp_False);

        assert_obj(any_contains(r, i_last), is, yp_True);
        assert_obj(any_contains(r, i_last_p1), is, yp_False);
        assert_obj(any_contains(r, i_last_m1), is, yp_False);

        assert_obj(any_contains(r, i_middle), is, yp_True);
        assert_obj(any_contains(r, i_middle_p1), is, yp_False);
        assert_obj(any_contains(r, i_middle_m1), is, yp_False);

        yp_decrefN(N(r));
    }

    yp_decrefN(N(i_first, i_first_p1, i_first_m1, i_last, i_last_p1, i_last_m1, i_middle,
            i_middle_p1, i_middle_m1));
}

static ypObject *in_to_contains(ypObject *r, ypObject *x) { return yp_in(x, r); }

static ypObject *not_in_to_contains(ypObject *r, ypObject *x) { return yp_not(yp_not_in(x, r)); }

static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, yp_contains);
    _test_contains(fixture->type, in_to_contains);
    _test_contains(fixture->type, not_in_to_contains);
    return MUNIT_OK;
}

static ypObject *findC_to_contains3(yp_ssize_t (*any_findC)(ypObject *, ypObject *, ypObject **),
        int raises, ypObject *r, ypObject *x)
{
    ypObject  *exc = yp_None;
    yp_ssize_t result = any_findC(r, x, &exc);
    if (result < 0) {
        assert_ssizeC(result, ==, -1);
        if (raises) {
            assert_isexception(exc, yp_ValueError);
        } else {
            assert_obj(exc, is, yp_None);
        };
        return yp_False;
    } else {
        assert_obj(exc, is, yp_None);
        ead(item, yp_getindexC(r, result), assert_obj(item, eq, x));
        return yp_True;
    }
}

static ypObject *findC_to_contains(ypObject *r, ypObject *x)
{
    return findC_to_contains3(yp_findC, /*raises=*/FALSE, r, x);
}

static MunitResult test_findC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, findC_to_contains);
    return MUNIT_OK;
}

static ypObject *indexC_to_contains(ypObject *r, ypObject *x)
{
    return findC_to_contains3(yp_indexC, /*raises=*/TRUE, r, x);
}

static MunitResult test_indexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, indexC_to_contains);
    return MUNIT_OK;
}

static ypObject *rfindC_to_contains(ypObject *r, ypObject *x)
{
    return findC_to_contains3(yp_rfindC, /*raises=*/FALSE, r, x);
}

static MunitResult test_rfindC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, rfindC_to_contains);
    return MUNIT_OK;
}

static ypObject *rindexC_to_contains(ypObject *r, ypObject *x)
{
    return findC_to_contains3(yp_rindexC, /*raises=*/TRUE, r, x);
}

static MunitResult test_rindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, rindexC_to_contains);
    return MUNIT_OK;
}


MunitTest test_range_tests[] = {TEST(test_contains, NULL), TEST(test_findC, NULL),
        TEST(test_indexC, NULL), TEST(test_rfindC, NULL), TEST(test_rindexC, NULL), {NULL}};


extern void test_range_initialize(void) {}

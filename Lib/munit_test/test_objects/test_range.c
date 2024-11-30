
#include "munit_test/unittest.h"


// Like assert_sequence, but also asserts that obj is a range object.
#define assert_range(obj, ...)                                                                   \
    do {                                                                                         \
        ypObject *_ypmt_RANGE_obj = (obj);                                                       \
        ypObject *_ypmt_RANGE_items[] = {__VA_ARGS__};                                           \
        char     *_ypmt_RANGE_item_strs[] = {STRINGIFY(__VA_ARGS__)};                            \
        _assert_type_is(_ypmt_RANGE_obj, yp_t_range, "%s", "yp_t_range", #obj);                  \
        _assert_sequence(_ypmt_RANGE_obj, _ypmt_RANGE_items, "%s", _ypmt_RANGE_item_strs, #obj); \
    } while (0)


static void _test_rangeC(
        ypObject *(*any_rangeC)(yp_int_t), ypObject *(*any_rangeC3)(yp_int_t, yp_int_t, yp_int_t))
{
    uniqueness_t *uq = uniqueness_new();
    yp_ssize_t    i;
    yp_int_t      itemsC[8];
    yp_int_t      stepC;
    ypObject     *i_zero = yp_intC(0);
    ypObject     *i_one = yp_intC(1);
    ypObject     *items[yp_lengthof_array(itemsC)];
    obj_array_fill(items, uq, fixture_type_range->rand_items);
    for (i = 0; i < yp_lengthof_array(itemsC); i++) itemsC[i] = yp_asintC_not_raises(items[i]);
    stepC = itemsC[1] - itemsC[0];

    // Basic new.
    ead(r, any_rangeC(1), assert_range(r, i_zero));
    ead(r, any_rangeC(2), assert_range(r, i_zero, i_one));
    ead(r, any_rangeC3(0, 1, 1), assert_range(r, i_zero));
    ead(r, any_rangeC3(0, 2, 1), assert_range(r, i_zero, i_one));
    ead(r, any_rangeC3(itemsC[0], itemsC[1], stepC), assert_range(r, items[0]));
    ead(r, any_rangeC3(itemsC[0], itemsC[2], stepC), assert_range(r, items[0], items[1]));

    // Negated slice.
    ead(r, any_rangeC3(1, 0, -1), assert_range(r, i_one));
    ead(r, any_rangeC3(1, -1, -1), assert_range(r, i_one, i_zero));
    ead(r, any_rangeC3(itemsC[2], itemsC[1], -stepC), assert_range(r, items[2]));
    ead(r, any_rangeC3(itemsC[2], itemsC[0], -stepC), assert_range(r, items[2], items[1]));

    // Step multiples.
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC),
            assert_range(r, items[0], items[1], items[2], items[3], items[4], items[5], items[6]));
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC * 2),
            assert_range(r, items[0], items[2], items[4], items[6]));
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC * 3),
            assert_range(r, items[0], items[3], items[6]));
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC * 4), assert_range(r, items[0], items[4]));
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC * 5), assert_range(r, items[0], items[5]));
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC * 6), assert_range(r, items[0], items[6]));
    ead(r, any_rangeC3(itemsC[0], itemsC[7], stepC * 7), assert_range(r, items[0]));

    // Step negative multiples.
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -1),
            assert_range(r, items[7], items[6], items[5], items[4], items[3], items[2], items[1]));
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -2),
            assert_range(r, items[7], items[5], items[3], items[1]));
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -3),
            assert_range(r, items[7], items[4], items[1]));
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -4), assert_range(r, items[7], items[3]));
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -5), assert_range(r, items[7], items[2]));
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -6), assert_range(r, items[7], items[1]));
    ead(r, any_rangeC3(itemsC[7], itemsC[0], stepC * -7), assert_range(r, items[7]));

    // Empty range. Optimization: empty immortal when range is empty.
    assert_type_is(yp_range_empty, yp_t_range);
    assert_len(yp_range_empty, 0);
    assert_obj(any_rangeC(0), is, yp_range_empty);
    assert_obj(any_rangeC(-1), is, yp_range_empty);
    assert_obj(any_rangeC3(0, 0, 1), is, yp_range_empty);
    assert_obj(any_rangeC3(0, -1, 1), is, yp_range_empty);
    assert_obj(any_rangeC3(0, 0, -1), is, yp_range_empty);
    assert_obj(any_rangeC3(-1, 0, -1), is, yp_range_empty);
    assert_obj(any_rangeC3(itemsC[0], itemsC[0], stepC), is, yp_range_empty);
    assert_obj(any_rangeC3(itemsC[1], itemsC[0], stepC), is, yp_range_empty);
    assert_obj(any_rangeC3(itemsC[0], itemsC[0], -stepC), is, yp_range_empty);
    assert_obj(any_rangeC3(itemsC[0], itemsC[1], -stepC), is, yp_range_empty);

    // Invalid ranges.
    assert_raises(any_rangeC3(0, 1, 0), yp_ValueError);  // step==0
    assert_raises(
            any_rangeC3(0, 1, -yp_INT_T_MAX - 1), yp_SystemLimitationError);  // too-small step
    assert_raises(any_rangeC3(yp_INT_T_MIN, yp_INT_T_MAX, 1),
            yp_SystemLimitationError);  // too-large length

    obj_array_decref(items);
    yp_decrefN(N(i_one, i_zero));
    uniqueness_dealloc(uq);
}

static MunitResult test_rangeC(const MunitParameter params[], fixture_t *fixture)
{
    _test_rangeC(yp_rangeC, yp_rangeC3);
    return MUNIT_OK;
}

static ypObject *rangeC_to_call_args_t_range(yp_int_t stop)
{
    ypObject *i_stop;
    ypObject *result;
    assert_not_raises(i_stop = yp_intC(stop));
    result = yp_callN(yp_t_range, 1, i_stop);
    yp_decref(i_stop);
    return result;
}

static ypObject *rangeC3_to_call_args_t_range(yp_int_t start, yp_int_t stop, yp_int_t step)
{
    ypObject *i_start;
    ypObject *i_stop;
    ypObject *i_step;
    ypObject *result;
    assert_not_raises(i_start = yp_intC(start));
    assert_not_raises(i_stop = yp_intC(stop));
    assert_not_raises(i_step = yp_intC(step));
    result = yp_callN(yp_t_range, 3, i_start, i_stop, i_step);
    yp_decrefN(N(i_step, i_stop, i_start));
    return result;
}

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *i_zero = yp_intC(0);
    ypObject *i_one = yp_intC(1);
    ypObject *i_two = yp_intC(2);
    ypObject *f_value = rand_obj(NULL, fixture_type_float);
    ypObject *str_cls = yp_str_frombytesC2(-1, "cls");
    ypObject *str_x = yp_str_frombytesC2(-1, "x");
    ypObject *str_step = yp_str_frombytesC2(-1, "step");
    ypObject *str_rand = rand_obj(NULL, fixture_type_str);

    // Shared tests. Tests one and three arguments.
    _test_rangeC(rangeC_to_call_args_t_range, rangeC3_to_call_args_t_range);

    // Two arguments. Slice defaults to 1. Optimization: empty immortal when range is empty.
    ead(r, yp_callN(yp_t_range, N(i_zero, i_one)), assert_range(r, i_zero));
    ead(r, yp_callN(yp_t_range, N(i_zero, i_two)), assert_range(r, i_zero, i_one));
    assert_obj(yp_callN(yp_t_range, N(i_two, i_zero)), is, yp_range_empty);

    // Bad argument types.
    assert_raises(yp_callN(yp_t_range, N(f_value, i_two, i_one)), yp_TypeError);
    assert_raises(yp_callN(yp_t_range, N(i_zero, f_value, i_one)), yp_TypeError);
    assert_raises(yp_callN(yp_t_range, N(i_zero, i_two, f_value)), yp_TypeError);

    // Invalid arguments.
    {
        ypObject *args_two = yp_tupleN(N(i_zero, i_two));
        ypObject *args_three = yp_tupleN(N(i_zero, i_two, i_one));
        ypObject *args_four = yp_tupleN(N(i_zero, i_two, i_one, i_one));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, yp_t_range, str_x, i_two));
        ypObject *kwargs_step = yp_frozendictK(K(str_step, i_one));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, i_zero));

        assert_raises(yp_callN(yp_t_range, 0), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_range, yp_tuple_empty, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_callN(yp_t_range, N(i_zero, i_two, i_one, i_one)), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_range, args_four, yp_frozendict_empty), yp_TypeError);

        // Keyword arguments are not supported.
        assert_raises(yp_call_stars(yp_t_range, yp_tuple_empty, kwargs_cls), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_range, args_two, kwargs_step), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_range, args_three, kwargs_rand), yp_TypeError);

        yp_decrefN(N(args_two, args_three, args_four, kwargs_cls, kwargs_step, kwargs_rand));
    }

    // Exception passthrough.
    assert_isexception(yp_callN(yp_t_range, N(yp_SyntaxError)), yp_SyntaxError);
    assert_isexception(yp_callN(yp_t_range, N(i_zero, yp_SyntaxError)), yp_SyntaxError);
    assert_isexception(yp_callN(yp_t_range, N(i_zero, i_two, yp_SyntaxError)), yp_SyntaxError);

    yp_decrefN(N(str_rand, str_step, str_x, str_cls, f_value, i_one, i_zero));
    return MUNIT_OK;
}

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

static ypObject *contains_to_in(ypObject *r, ypObject *x) { return yp_in(x, r); }

static ypObject *contains_to_not_in(ypObject *r, ypObject *x) { return yp_not(yp_not_in(x, r)); }

static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, yp_contains);
    _test_contains(fixture->type, contains_to_in);
    _test_contains(fixture->type, contains_to_not_in);
    return MUNIT_OK;
}

static ypObject *_contains_to_findC(yp_ssize_t (*any_findC)(ypObject *, ypObject *, ypObject **),
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

static ypObject *contains_to_findC(ypObject *r, ypObject *x)
{
    return _contains_to_findC(yp_findC, /*raises=*/FALSE, r, x);
}

static MunitResult test_findC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, contains_to_findC);
    return MUNIT_OK;
}

static ypObject *contains_to_indexC(ypObject *r, ypObject *x)
{
    return _contains_to_findC(yp_indexC, /*raises=*/TRUE, r, x);
}

static MunitResult test_indexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, contains_to_indexC);
    return MUNIT_OK;
}

static ypObject *contains_to_rfindC(ypObject *r, ypObject *x)
{
    return _contains_to_findC(yp_rfindC, /*raises=*/FALSE, r, x);
}

static MunitResult test_rfindC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, contains_to_rfindC);
    return MUNIT_OK;
}

static ypObject *contains_to_rindexC(ypObject *r, ypObject *x)
{
    return _contains_to_findC(yp_rindexC, /*raises=*/TRUE, r, x);
}

static MunitResult test_rindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_contains(fixture->type, contains_to_rindexC);
    return MUNIT_OK;
}

static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    obj_array_fill(items, uq, fixture_type_range->rand_items);

    // range_getslice
    {
        ypObject *r = fixture_type_range->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_getsliceC4(r, 0, 1, 1), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(r));
    }

    // yp_rangeC
    malloc_tracker_oom_after(0);
    assert_raises(yp_rangeC(1), yp_MemoryError);
    malloc_tracker_oom_disable();

    // yp_rangeC3
    malloc_tracker_oom_after(0);
    assert_raises(yp_rangeC3(0, 1, 1), yp_MemoryError);
    malloc_tracker_oom_disable();

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}


MunitTest test_range_tests[] = {TEST(test_rangeC, NULL), TEST(test_call_type, NULL),
        TEST(test_contains, NULL), TEST(test_findC, NULL), TEST(test_indexC, NULL),
        TEST(test_rfindC, NULL), TEST(test_rindexC, NULL), TEST(test_oom, NULL), {NULL}};


extern void test_range_initialize(void) {}

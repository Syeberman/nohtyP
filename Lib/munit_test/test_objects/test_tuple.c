
#include "munit_test/unittest.h"


// FIXME tuple_concat needs more coverage (test against all types)

static int is_friend_type(fixture_type_t *type, fixture_type_t *x_type)
{
    return x_type->type == type->type || x_type->type == type->pair->type;
}


static void _test_newN(
        fixture_type_t *type, ypObject *(*any_newN)(int, ...), int test_exception_passthrough)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // Basic newN.
    {
        ypObject *sq = any_newN(N(items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq));
    }

    // n is zero.
    {
        ypObject *sq = any_newN(0);
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // n is negative.
    {
        ypObject *sq = any_newN(-1);
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // Duplicate arguments.
    {
        ypObject *sq = any_newN(N(items[0], items[0], items[1], items[1]));
        assert_type_is(sq, type->type);
        assert_sequence(sq, items[0], items[0], items[1], items[1]);
        yp_decrefN(N(sq));
    }

    // Optimization: empty immortal when n is zero.
    if (type->falsy != NULL) {
        assert_obj(any_newN(0), is, type->falsy);
        assert_obj(any_newN(-1), is, type->falsy);
    }

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_newN(N(yp_SyntaxError)), yp_SyntaxError);
        assert_isexception(any_newN(N(yp_None, yp_SyntaxError)), yp_SyntaxError);
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static void _test_new(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_new)(ypObject *), int               test_exception_passthrough)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, x_type->rand_items);  // Note that we use x_type's items.

    // Basic new. Recall that x_type may not guarantee iteration order.
    {
        ypObject *x = x_type->newN(N(items[0], items[1]));
        ypObject *sq = any_new(x);
        assert_type_is(sq, type->type);
        if (x_type->is_sequence) {
            assert_sequence(sq, items[0], items[1]);
        } else {
            assert_setlike(sq, items[0], items[1]);
        }
        yp_decrefN(N(sq, x));
    }

    // x is empty.
    {
        ypObject *x = x_type->newN(0);
        ypObject *sq = any_new(x);
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq, x));
    }

    // x contains duplicates.
    if (x_type->is_sequence && !x_type->is_patterned) {
        ypObject *x = x_type->newN(N(items[0], items[0], items[1]));
        ypObject *sq = any_new(x);
        assert_sequence(sq, items[0], items[0], items[1]);
        yp_decrefN(N(sq, x));
    }

    // Optimization: lazy shallow copy of a friendly immutable x to immutable sq.
    if (is_friend_type(type, x_type)) {
        ypObject *x = x_type->newN(N(items[0], items[1]));
        ypObject *sq = any_new(x);
        if (type->is_mutable || x_type->is_mutable) {
            assert_obj(sq, is_not, x);
        } else {
            assert_obj(sq, is, x);
        }
        yp_decrefN(N(sq, x));
    }

    // Optimization: empty immortal when x is empty.
    if (type->falsy != NULL) {
        ypObject *x = x_type->newN(0);
        ypObject *sq = any_new(x);
        assert_obj(sq, is, type->falsy);
        yp_decrefN(N(sq, x));
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject * sq, x, yp_tupleN(N(items[0], items[1])), sq = any_new(x),
            assert_sequence(sq, items[0], items[1]), yp_decref(sq));

    // x is not an iterable.
    assert_raises(any_new(not_iterable), yp_TypeError);

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_new(yp_SyntaxError), yp_SyntaxError);
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static ypObject *newN_to_tupleNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_tupleNV(n, args);
    va_end(args);
    return result;
}

static ypObject *newN_to_listNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_listNV(n, args);
    va_end(args);
    return result;
}

static MunitResult test_newN(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN)(int, ...);
    ypObject *(*newN_to_newNV)(int, ...);

    if (type->type == yp_t_tuple) {
        newN = yp_tupleN;
        newN_to_newNV = newN_to_tupleNV;
    } else {
        assert_ptr(type->type, ==, yp_t_list);
        newN = yp_listN;
        newN_to_newNV = newN_to_listNV;
    }

    // Shared tests.
    _test_newN(type, newN, /*test_exception_passthrough=*/TRUE);
    _test_newN(type, newN_to_newNV, /*test_exception_passthrough=*/TRUE);

    return MUNIT_OK;
}

static ypObject *newN_to_tuple(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    // Note that if we used yp_tupleNV here, the "fellow tuple" optimization would mean we wouldn't
    // actually be testing yp_tuple here. So, we use new_iterNV.
    va_start(args, n);
    assert_not_raises(iterable = new_iterNV(n, args));  // new ref
    va_end(args);

    result = yp_tuple(iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *newN_to_list(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = new_iterNV(n, args));  // new ref
    va_end(args);

    result = yp_list(iterable);
    yp_decref(iterable);
    return result;
}

static MunitResult test_new(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t **x_type;
    ypObject *(*newN_to_new)(int, ...);
    ypObject *(*new_)(ypObject *);

    if (type->type == yp_t_tuple) {
        newN_to_new = newN_to_tuple;
        new_ = yp_tuple;
    } else {
        assert_ptr(type->type, ==, yp_t_list);
        newN_to_new = newN_to_list;
        new_ = yp_list;
    }

    // Shared tests.
    _test_newN(type, newN_to_new, /*test_exception_passthrough=*/FALSE);
    for (x_type = fixture_types_iterable->types; (*x_type) != NULL; x_type++) {
        _test_new(type, *x_type, new_, /*test_exception_passthrough=*/TRUE);
    }

    return MUNIT_OK;
}

static ypObject *newN_to_call_t_tuple(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    // Note that if we used yp_tupleNV here, the "fellow tuple" optimization would mean we wouldn't
    // actually be testing yp_t_tuple here. So, we use new_iterNV.
    va_start(args, n);
    assert_not_raises(iterable = new_iterNV(n, args));  // new ref
    va_end(args);

    result = yp_callN(yp_t_tuple, 1, iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *new_to_call_t_tuple(ypObject *iterable)
{
    return yp_callN(yp_t_tuple, 1, iterable);
}

static ypObject *newN_to_call_t_list(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = new_iterNV(n, args));  // new ref
    va_end(args);

    result = yp_callN(yp_t_list, 1, iterable);
    yp_decref(iterable);
    return result;
}
static ypObject *new_to_call_t_list(ypObject *iterable) { return yp_callN(yp_t_list, 1, iterable); }


static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t **x_type;
    ypObject *(*newN_to_call_type)(int, ...);
    ypObject *(*new_to_call_type)(ypObject *);
    uniqueness_t *uq = uniqueness_new();
    ypObject     *str_iterable = yp_str_frombytesC2(-1, "iterable");
    ypObject     *str_cls = yp_str_frombytesC2(-1, "cls");
    ypObject     *str_rand = rand_obj(uq, fixture_type_str);

    if (type->type == yp_t_tuple) {
        newN_to_call_type = newN_to_call_t_tuple;
        new_to_call_type = new_to_call_t_tuple;
    } else {
        assert_ptr(type->type, ==, yp_t_list);
        newN_to_call_type = newN_to_call_t_list;
        new_to_call_type = new_to_call_t_list;
    }

    // Shared tests.
    _test_newN(type, newN_to_call_type, /*test_exception_passthrough=*/FALSE);
    for (x_type = fixture_types_iterable->types; (*x_type) != NULL; x_type++) {
        _test_new(type, *x_type, new_to_call_type, /*test_exception_passthrough=*/TRUE);
    }

    // Zero arguments.
    {
        ypObject *sq = yp_callN(type->type, 0);
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // Optimization: empty immortal with zero arguments.
    if (type->falsy != NULL) {
        assert_obj(yp_callN(type->type, 0), is, type->falsy);
    }

    // Invalid arguments.
    {
        ypObject *args_two = yp_tupleN(N(yp_tuple_empty, yp_tuple_empty));
        ypObject *kwargs_iterable = yp_frozendictK(K(str_iterable, yp_tuple_empty));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, type->type));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, yp_tuple_empty));

        assert_raises(yp_callN(type->type, N(yp_tuple_empty, yp_tuple_empty)), yp_TypeError);
        assert_raises(yp_call_stars(type->type, args_two, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_iterable), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_cls), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_rand), yp_TypeError);

        yp_decrefN(N(kwargs_rand, kwargs_cls, kwargs_iterable, args_two));
    }

    // Exception passthrough.
    assert_isexception(yp_callN(type->type, N(yp_SyntaxError)), yp_SyntaxError);

    yp_decrefN(N(str_iterable, str_cls, str_rand));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static void _test_new_repeatCN(
        fixture_type_t *type, ypObject *(*any_new_repeatCN)(yp_ssize_t, int, ...))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // Basic new_repeatCN.
    {
        ypObject *sq = any_new_repeatCN(2, N(items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_sequence(sq, items[0], items[1], items[0], items[1]);
        yp_decrefN(N(sq));
    }

    // Factor of one.
    {
        ypObject *sq = any_new_repeatCN(1, N(items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_sequence(sq, items[0], items[1]);
        yp_decrefN(N(sq));
    }

    // Factor of zero.
    {
        ypObject *sq = any_new_repeatCN(0, N(items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // Negative factor.
    {
        ypObject *sq = any_new_repeatCN(-1, N(items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // Large factor. (Exercises _ypSequence_repeat_memcpy optimization.)
    {
        ypObject *sq = any_new_repeatCN(8, N(items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_sequence(sq, items[0], items[1], items[0], items[1], items[0], items[1], items[0],
                items[1], items[0], items[1], items[0], items[1], items[0], items[1], items[0],
                items[1]);
        yp_decrefN(N(sq));
    }

    // n is zero.
    {
        ypObject *sq = any_new_repeatCN(2, 0);
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // n is negative.
    {
        ypObject *sq = any_new_repeatCN(2, -1);
        assert_type_is(sq, type->type);
        assert_len(sq, 0);
        yp_decrefN(N(sq));
    }

    // Duplicate arguments.
    {
        ypObject *sq = any_new_repeatCN(2, N(items[0], items[0], items[1]));
        assert_type_is(sq, type->type);
        assert_sequence(sq, items[0], items[0], items[1], items[0], items[0], items[1]);
        yp_decrefN(N(sq));
    }

    // Optimization: empty immortal when either factor or n is zero.
    if (type->falsy != NULL) {
        assert_obj(any_new_repeatCN(0, N(items[0], items[1])), is, type->falsy);
        assert_obj(any_new_repeatCN(-1, N(items[0], items[1])), is, type->falsy);
        assert_obj(any_new_repeatCN(2, 0), is, type->falsy);
        assert_obj(any_new_repeatCN(2, -1), is, type->falsy);
    }

    // Extremely-large factor.
    assert_raises(
            any_new_repeatCN(yp_SSIZE_T_MAX, N(items[0], items[1])), yp_MemorySizeOverflowError);

    // Exception passthrough.
    assert_isexception(any_new_repeatCN(2, N(yp_SyntaxError)), yp_SyntaxError);
    // FIXME assert_isexception(any_new_repeatCN(0, N(yp_SyntaxError)), yp_SyntaxError);
    assert_isexception(any_new_repeatCN(2, N(yp_None, yp_SyntaxError)), yp_SyntaxError);

    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static ypObject *newN_to_tuple_repeatCNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_tuple_repeatCNV(1, n, args);
    va_end(args);
    return result;
}

static ypObject *newN_to_list_repeatCNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_list_repeatCNV(1, n, args);
    va_end(args);
    return result;
}

static ypObject *new_repeatCN_to_tuple_repeatCNV(yp_ssize_t factor, int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_tuple_repeatCNV(factor, n, args);
    va_end(args);
    return result;
}

static ypObject *new_repeatCN_to_list_repeatCNV(yp_ssize_t factor, int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_list_repeatCNV(factor, n, args);
    va_end(args);
    return result;
}

static MunitResult test_repeatCN(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN_to_new_repeatCNV)(int, ...);
    ypObject *(*new_repeatCN)(yp_ssize_t, int, ...);
    ypObject *(*new_repeatCN_to_new_repeatCNV)(yp_ssize_t, int, ...);

    if (type->type == yp_t_tuple) {
        newN_to_new_repeatCNV = newN_to_tuple_repeatCNV;
        new_repeatCN = yp_tuple_repeatCN;
        new_repeatCN_to_new_repeatCNV = new_repeatCN_to_tuple_repeatCNV;
    } else {
        assert_ptr(type->type, ==, yp_t_list);
        newN_to_new_repeatCNV = newN_to_list_repeatCNV;
        new_repeatCN = yp_list_repeatCN;
        new_repeatCN_to_new_repeatCNV = new_repeatCN_to_list_repeatCNV;
    }

    // Shared tests. There's no way to test *_repeatCN with _test_newN, as inserting the argument
    // requires us to use va_list.
    _test_newN(type, newN_to_new_repeatCNV, /*test_exception_passthrough=*/TRUE);
    _test_new_repeatCN(type, new_repeatCN);
    _test_new_repeatCN(type, new_repeatCN_to_new_repeatCNV);

    return MUNIT_OK;
}

// FIXME Ensure yp_sort4 properly handles exception passthrough, even in cases where one of the
// arguments would be ignored (e.g. empty list).

// FIXME yp_itemarrayCX

static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *items[8];
    obj_array_fill(items, uq, type->rand_items);

    // _ypTuple_copy
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_unfrozen_copy(sq), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // TODO _ypTuple_deepcopy, new detached array

    // _ypTuple_push
    if (type->is_mutable) {
        int       i;
        ypObject *exc = yp_None;
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        for (i = 0; i < 32; i++) {
            yp_push(sq, items[i % yp_lengthof_array(items)], &exc);
            if (yp_isexceptionC(exc)) break;
        }
        assert_isexception(exc, yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // _ypTuple_extend_fromtuple
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_tupleN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_extend(sq, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // _ypTuple_extend_fromminiiter
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = new_iterN(N(items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7], items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7], items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7], items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_extend(sq, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // _ypTuple_new_fromminiiter
    {
        ypObject *x = new_iterN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_tuple) {
            assert_raises(yp_tuple(x), yp_MemoryError);
        } else {
            assert_raises(yp_list(x), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // _ypTuple_new_fromiterable, empty list
    if (type->type == yp_t_list) {
        ypObject *x = yp_frozensetN(0);
        malloc_tracker_oom_after(0);
        assert_raises(yp_list(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // _ypTuple_concat_fromtuple
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_tupleN(N(items[2], items[3]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_concat(sq, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // _ypTuple_setslice_grow
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_tupleN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_setsliceC6(sq, 1, 2, 1, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // tuple_concat, copy list
    if (type->type == yp_t_list) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(0);
        malloc_tracker_oom_after(0);
        assert_raises(yp_concat(sq, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x, sq));
    }

    // tuple_concat, new
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = new_iterN(N(items[2], items[3]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_concat(sq, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x, sq));
    }

    // tuple_repeat
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_repeatC(sq, 2), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // tuple_getslice
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_getsliceC4(sq, 0, 1, 1), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // list_irepeat
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_irepeatC(sq, 32, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // list_insert
    if (type->is_mutable) {
        int       i;
        ypObject *exc = yp_None;
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        for (i = 0; i < 32; i++) {
            yp_insertC(sq, -1, items[i % yp_lengthof_array(items)], &exc);
            if (yp_isexceptionC(exc)) break;
        }
        assert_isexception(exc, yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // _ypTupleNV
    {
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_tuple) {
            assert_raises(yp_tupleN(N(items[0], items[1])), yp_MemoryError);
        } else {
            assert_raises(yp_listN(N(items[0], items[1])), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // yp_sorted3, new list
    {
        ypObject *x = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_sorted(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // yp_sorted3, new detached array
    {
        ypObject *x = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(1);  // allow yp_list to succeed
        assert_raises(yp_sorted(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // _ypTuple_repeatCNV
    {
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_tuple) {
            assert_raises(yp_tuple_repeatCN(2, N(items[0], items[1])), yp_MemoryError);
        } else {
            assert_raises(yp_list_repeatCN(2, N(items[0], items[1])), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // list_sort, new detached array
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_sort(sq, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}


char *param_values_test_tuple[] = {"tuple", "list", NULL};

static MunitParameterEnum test_tuple_params[] = {{param_key_type, param_values_test_tuple}, {NULL}};

MunitTest test_tuple_tests[] = {TEST(test_newN, test_tuple_params),
        TEST(test_new, test_tuple_params), TEST(test_call_type, test_tuple_params),
        TEST(test_repeatCN, test_tuple_params), TEST(test_oom, test_tuple_params), {NULL}};


extern void test_tuple_initialize(void) {}

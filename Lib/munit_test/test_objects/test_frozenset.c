
#include "munit_test/unittest.h"


// A copy of ypSetMiState from nohtyP.c, as a union with yp_uint64_t to maintain strict aliasing.
typedef union {
    struct {
        yp_uint32_t keysleft;
        yp_uint32_t index;
    } as_struct;
    yp_uint64_t as_int;
} ypSetMiState;

// Defines an mi_state for use with frozensets/sets, which can be accessed by the variable name,
// declared as "const yp_uint64_t".
#define define_frozenset_mi_state(name, keysleft, index)          \
    ypSetMiState      _##name##_struct = {{(keysleft), (index)}}; \
    const yp_uint64_t name = (_##name##_struct.as_int)


static void _test_newN(
        fixture_type_t *type, ypObject *(*any_newN)(int, ...), int test_exception_passthrough)
{
    uniqueness_t      *uq = uniqueness_new();
    hashability_pair_t pair = rand_obj_any_hashability_pair(uq);
    ypObject          *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // Basic newN.
    {
        ypObject *so = any_newN(N(items[0], items[1]));
        assert_type_is(so, type->yp_type);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // n is zero.
    {
        ypObject *so = any_newN(0);
        assert_type_is(so, type->yp_type);
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // n is negative.
    {
        ypObject *so = any_newN(-1);
        assert_type_is(so, type->yp_type);
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Duplicate arguments.
    {
        ypObject *so = any_newN(N(items[0], items[0], items[1], items[1]));
        assert_type_is(so, type->yp_type);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Unhashable argument.
    {
        ypObject *unhashable = rand_obj_any_mutable(uq);
        assert_raises(any_newN(N(unhashable)), yp_TypeError);
        assert_raises(any_newN(N(items[0], unhashable)), yp_TypeError);
        assert_raises(any_newN(N(items[0], items[1], unhashable)), yp_TypeError);
        yp_decrefN(N(unhashable));
    }

    // Unhashable argument rejected even if equal to other hashable argument.
    assert_raises(any_newN(N(pair.hashable, pair.unhashable)), yp_TypeError);
    assert_raises(any_newN(N(pair.unhashable, pair.hashable)), yp_TypeError);

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
    yp_decrefN(N(pair.unhashable, pair.hashable));
    uniqueness_dealloc(uq);
}

static void _test_new(fixture_type_t *type, peer_type_t *peer, ypObject *(*any_new)(ypObject *),
        int test_exception_passthrough)
{
    fixture_type_t    *x_type = peer->type;
    uniqueness_t      *uq = uniqueness_new();
    hashability_pair_t pair = rand_obj_any_hashability_pair(uq);
    ypObject          *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject          *items[2];
    obj_array_fill(items, uq, peer->rand_items);

    // Basic new.
    {
        ypObject *x = x_type->newN(N(items[0], items[1]));
        ypObject *so = any_new(x);
        assert_type_is(so, type->yp_type);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    {
        ypObject *x = x_type->newN(0);
        ypObject *so = any_new(x);
        assert_type_is(so, type->yp_type);
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x contains duplicates.
    {
        ypObject *x = x_type->newN(N(items[0], items[0], items[1]));
        ypObject *so = any_new(x);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable item.
    if (!x_type->hashable_items_only) {
        ypObject *unhashable = rand_obj_any_mutable(uq);
        ead(x, x_type->newN(N(unhashable)), assert_raises(any_new(x), yp_TypeError));
        ead(x, x_type->newN(N(items[0], unhashable)), assert_raises(any_new(x), yp_TypeError));
        ead(x, x_type->newN(N(items[0], items[1], unhashable)),
                assert_raises(any_new(x), yp_TypeError));
        yp_decrefN(N(unhashable));
    }

    // Unhashable item rejected even if equal to other hashable item.
    if (!x_type->hashable_items_only) {
        ead(x, x_type->newN(N(pair.hashable, pair.unhashable)),
                assert_raises(any_new(x), yp_TypeError));
        ead(x, x_type->newN(N(pair.unhashable, pair.hashable)),
                assert_raises(any_new(x), yp_TypeError));
    }

    // Optimization: lazy shallow copy of a friendly immutable x to immutable so.
    if (are_friend_types(type, x_type)) {
        ypObject *x = x_type->newN(N(items[0], items[1]));
        ypObject *so = any_new(x);
        if (type->is_mutable || x_type->is_mutable) {
            assert_obj(so, is_not, x);
        } else {
            assert_obj(so, is, x);
        }
        yp_decrefN(N(so, x));
    }

    // Optimization: empty immortal when x is empty.
    if (type->falsy != NULL) {
        ypObject *x = x_type->newN(0);
        ypObject *so = any_new(x);
        assert_obj(so, is, type->falsy);
        yp_decrefN(N(so, x));
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject * so, x, yp_tupleN(N(items[0], items[1])), so = any_new(x),
            assert_setlike(so, items[0], items[1]), yp_decref(so));

    // x is not an iterable.
    assert_raises(any_new(not_iterable), yp_TypeError);

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_new(yp_SyntaxError), yp_SyntaxError);
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    uniqueness_dealloc(uq);
}

static ypObject *newN_to_frozensetNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_frozensetNV(n, args);
    va_end(args);
    return result;
}

static ypObject *newN_to_setNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_setNV(n, args);
    va_end(args);
    return result;
}

static MunitResult test_newN(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN)(int, ...);
    ypObject *(*newN_to_newNV)(int, ...);

    if (type->yp_type == yp_t_frozenset) {
        newN = yp_frozensetN;
        newN_to_newNV = newN_to_frozensetNV;
    } else {
        assert_ptr(type->yp_type, ==, yp_t_set);
        newN = yp_setN;
        newN_to_newNV = newN_to_setNV;
    }

    // Shared tests.
    _test_newN(type, newN, /*test_exception_passthrough=*/TRUE);
    _test_newN(type, newN_to_newNV, /*test_exception_passthrough=*/TRUE);

    return MUNIT_OK;
}

static ypObject *newN_to_frozenset(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = yp_tupleNV(n, args));  // new ref
    va_end(args);

    result = yp_frozenset(iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *newN_to_set(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = yp_tupleNV(n, args));  // new ref
    va_end(args);

    result = yp_set(iterable);
    yp_decref(iterable);
    return result;
}

static MunitResult test_new(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN_to_new)(int, ...);
    ypObject *(*new_)(ypObject *);
    peer_type_t *peer;

    if (type->yp_type == yp_t_frozenset) {
        newN_to_new = newN_to_frozenset;
        new_ = yp_frozenset;
    } else {
        assert_ptr(type->yp_type, ==, yp_t_set);
        newN_to_new = newN_to_set;
        new_ = yp_set;
    }

    // Shared tests.
    _test_newN(type, newN_to_new, /*test_exception_passthrough=*/FALSE);
    for (peer = type->peers; peer->type != NULL; peer++) {
        _test_new(type, peer, new_, /*test_exception_passthrough=*/TRUE);
    }

    return MUNIT_OK;
}

static ypObject *newN_to_call_t_frozenset(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = yp_tupleNV(n, args));  // new ref
    va_end(args);

    result = yp_callN(yp_t_frozenset, 1, iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *new_to_call_t_frozenset(ypObject *iterable)
{
    return yp_callN(yp_t_frozenset, 1, iterable);
}

static ypObject *newN_to_call_t_set(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = yp_tupleNV(n, args));  // new ref
    va_end(args);

    result = yp_callN(yp_t_set, 1, iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *new_to_call_t_set(ypObject *iterable) { return yp_callN(yp_t_set, 1, iterable); }

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN_to_call_type)(int, ...);
    ypObject *(*new_to_call_type)(ypObject *);
    peer_type_t  *peer;
    uniqueness_t *uq = uniqueness_new();
    ypObject     *str_iterable = yp_str_frombytesC2(-1, "iterable");
    ypObject     *str_cls = yp_str_frombytesC2(-1, "cls");
    ypObject     *str_rand = rand_obj(uq, fixture_type_str);

    if (type->yp_type == yp_t_frozenset) {
        newN_to_call_type = newN_to_call_t_frozenset;
        new_to_call_type = new_to_call_t_frozenset;
    } else {
        assert_ptr(type->yp_type, ==, yp_t_set);
        newN_to_call_type = newN_to_call_t_set;
        new_to_call_type = new_to_call_t_set;
    }

    // Shared tests.
    _test_newN(type, newN_to_call_type, /*test_exception_passthrough=*/FALSE);
    for (peer = type->peers; peer->type != NULL; peer++) {
        _test_new(type, peer, new_to_call_type, /*test_exception_passthrough=*/TRUE);
    }

    // Zero arguments.
    {
        ypObject *so = yp_callN(type->yp_type, 0);
        assert_type_is(so, type->yp_type);
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Optimization: empty immortal with zero arguments.
    if (type->falsy != NULL) {
        assert_obj(yp_callN(type->yp_type, 0), is, type->falsy);
    }

    // Invalid arguments.
    {
        ypObject *args_two = yp_tupleN(N(yp_frozenset_empty, yp_frozenset_empty));
        ypObject *kwargs_iterable = yp_frozendictK(K(str_iterable, yp_frozenset_empty));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, type->yp_type));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, yp_frozenset_empty));

        assert_raises(
                yp_callN(type->yp_type, N(yp_frozenset_empty, yp_frozenset_empty)), yp_TypeError);
        assert_raises(yp_call_stars(type->yp_type, args_two, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(type->yp_type, yp_tuple_empty, kwargs_iterable), yp_TypeError);
        assert_raises(yp_call_stars(type->yp_type, yp_tuple_empty, kwargs_cls), yp_TypeError);
        assert_raises(yp_call_stars(type->yp_type, yp_tuple_empty, kwargs_rand), yp_TypeError);

        yp_decrefN(N(kwargs_rand, kwargs_cls, kwargs_iterable, args_two));
    }

    // Exception passthrough.
    assert_isexception(yp_callN(type->yp_type, N(yp_SyntaxError)), yp_SyntaxError);

    yp_decrefN(N(str_iterable, str_cls, str_rand));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// frozenset- and set-specific miniiter tests; see test_iterable for more tests.
static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    define_frozenset_mi_state(keysleft_255_index_255, 255, 255);
    define_frozenset_mi_state(keysleft_2_index_0, 2, 0);
    define_frozenset_mi_state(keysleft_1_index_0, 1, 0);
    define_frozenset_mi_state(keysleft_0_index_0, 0, 0);
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // Corrupted states.
    {
        yp_uint64_t bad_states[] = {0uLL, (yp_uint64_t)-1, keysleft_255_index_255};
        yp_uint64_t mi_state;
        yp_ssize_t  i;
        ypObject   *so = type->newN(N(items[0], items[1]));
        ypObject   *mi = yp_miniiter(so, &mi_state);
        munit_assert_uint64(mi_state, ==, keysleft_2_index_0);

        for (i = 0; i < yp_lengthof_array(bad_states); i++) {
            mi_state = bad_states[i];
            assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
            munit_assert_uint64(mi_state, ==, bad_states[i]);  // unchanged

            assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
            munit_assert_uint64(mi_state, ==, bad_states[i]);  // unchanged
        }

        yp_decrefN(N(so, mi));
    }

    // Trigger the `index >= ypSet_ALLOCLEN(so)` case in the loop of *_next. keysleft usually
    // terminates iteration, but if keysleft is corrupted, or if an entry is removed from so, then
    // we need to ensure we stop at alloclen.
    {
        yp_uint64_t mi_state;
        ypObject   *so = type->newN(0);
        ypObject   *mi = yp_miniiter(so, &mi_state);
        munit_assert_uint64(mi_state, ==, keysleft_0_index_0);

        mi_state = keysleft_1_index_0;
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        // keysleft is set to zero to exhaust the iterator.
        munit_assert_uint64(mi_state, ==, keysleft_0_index_0);

        yp_decrefN(N(so, mi));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *items[16];
    obj_array_fill(items, uq, type->rand_items);

    // _ypSet_issubset_withiter
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_listN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_issubset(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_update_fromset
    if (type->is_mutable) {
        ypObject *so = type->newN(0);
        ypObject *x = yp_frozensetN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[8], items[9], items[10], items[11], items[12], items[13],
                items[14], items[15]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_update(so, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_update_fromiter
    if (type->is_mutable) {
        ypObject *so = type->newN(0);
        ypObject *x = yp_listN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[8], items[9], items[10], items[11], items[12], items[13],
                items[14], items[15]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_update(so, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_intersection_update_fromiter
    if (type->is_mutable) {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_listN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_intersection_update(so, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_symmetric_difference_update_fromset
    if (type->is_mutable) {
        ypObject *so = type->newN(0);
        ypObject *x = yp_frozensetN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[8], items[9], items[10], items[11], items[12], items[13],
                items[14], items[15]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_symmetric_difference_update(so, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_union_fromset, copy
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_union(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_union_fromset, resize
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(
                N(items[2], items[3], items[4], items[5], items[6], items[7], items[8], items[9],
                        items[10], items[11], items[12], items[13], items[14], items[15]));
        malloc_tracker_oom_after(1);  // allow _ypSet_copy to succeed
        assert_raises(yp_union(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_union_fromiter, copy
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_listN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_union(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_union_fromiter, resize
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_listN(
                N(items[2], items[3], items[4], items[5], items[6], items[7], items[8], items[9],
                        items[10], items[11], items[12], items[13], items[14], items[15]));
        malloc_tracker_oom_after(1);  // allow _ypSet_copy to succeed
        assert_raises(yp_union(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_intersection_fromset
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_intersection(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_intersection_fromiter
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_listN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_intersection(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_difference_fromset
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_difference(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_difference_fromiter
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_listN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_difference(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_symmetric_difference_fromset, copy
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_symmetric_difference(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSet_symmetric_difference_fromset, resize
    {
        ypObject *so = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(
                N(items[2], items[3], items[4], items[5], items[6], items[7], items[8], items[9],
                        items[10], items[11], items[12], items[13], items[14], items[15]));
        malloc_tracker_oom_after(1);  // allow _ypSet_copy to succeed
        assert_raises(yp_symmetric_difference(so, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(so, x));
    }

    // _ypSetNV, new set
    {
        malloc_tracker_oom_after(0);
        if (type->yp_type == yp_t_frozenset) {
            assert_raises(yp_frozensetN(N(items[0])), yp_MemoryError);
        } else {
            assert_raises(yp_setN(N(items[0])), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // _ypSetNV, new ob_data
    if (type->yp_type == yp_t_set) {
        malloc_tracker_oom_after(1);  // allow _ypSet_new (set) to succeed
        assert_raises(yp_setN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                              items[6], items[7], items[8], items[9], items[10], items[11],
                              items[12], items[13], items[14], items[15])),
                yp_MemoryError);
        malloc_tracker_oom_disable();
    }

    // _ypSet_fromiterable
    {
        ypObject *x = yp_listN(N(items[1], items[2]));
        malloc_tracker_oom_after(0);
        if (type->yp_type == yp_t_frozenset) {
            assert_raises(yp_frozenset(x), yp_MemoryError);
        } else {
            assert_raises(yp_set(x), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}


char *param_values_test_frozenset[] = {"frozenset", "set", "frozenset_dirty", "set_dirty", NULL};

static MunitParameterEnum test_frozenset_params[] = {
        {param_key_type, param_values_test_frozenset}, {NULL}};

MunitTest test_frozenset_tests[] = {TEST(test_newN, test_frozenset_params),
        TEST(test_new, test_frozenset_params), TEST(test_call_type, test_frozenset_params),
        TEST(test_miniiter, test_frozenset_params), TEST(test_oom, test_frozenset_params), {NULL}};


extern void test_frozenset_initialize(void) {}

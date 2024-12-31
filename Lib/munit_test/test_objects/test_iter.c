#include "munit_test/unittest.h"

// test_iterable validates the behaviour for iterators of iterables. This file validates all other
// iterator-related behaviour, in particular generators and yp_iter2.

// FIXME More tests here?

// TODO Ensure yp_max_keyN/etc properly handles exception passthrough, even in cases where
// one of the arguments would be ignored.


#define ypIter_LENHINT_MAX 0x7FFFFFFF  // From nohtyP.c.

typedef struct _count_down_state_t {
    yp_int_t  count;
    ypObject *object;
    void     *pointer;
} count_down_state_t;

static yp_state_decl_t count_down_state_decl = {
        sizeof(count_down_state_t), 1, {yp_offsetof(count_down_state_t, object)}};

static ypObject *count_down_iter_func(ypObject *g, ypObject *value)
{
    count_down_state_t *state;
    yp_ssize_t          size;
    ypObject           *result;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, &size, (void **)&state));
    assert_ssizeC(size, ==, yp_sizeof(*state));

    if (state->count < 0) return yp_StopIteration;
    assert_not_raises(result = yp_intC(state->count));
    state->count--;
    return result;
}

static ypObject *rand_obj_any_iter_func(ypObject *g, ypObject *value) { return rand_obj_any(NULL); }

static ypObject *SyntaxError_iter_func(ypObject *g, ypObject *value) { return yp_SyntaxError; }

// Used as the code for a function. Unconditionally returns None.
static ypObject *None_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray) { return yp_None; }

// Used as the code for a function. Unconditionally throws SyntaxError.
static ypObject *SyntaxError_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return yp_SyntaxError;
}

yp_IMMORTAL_STR_LATIN_1_static(s_star, "*");
yp_IMMORTAL_STR_LATIN_1_static(s_star_args, "*args");
yp_IMMORTAL_STR_LATIN_1_static(s_star_star_kwargs, "**kwargs");
yp_IMMORTAL_STR_LATIN_1_static(s_keyword, "keyword");

yp_IMMORTAL_FUNCTION_static(
        func_required_keyword, None_code, ({yp_CONST_REF(s_star)}, {yp_CONST_REF(s_keyword)}));

yp_IMMORTAL_FUNCTION_static(func_SyntaxError, SyntaxError_code,
        ({yp_CONST_REF(s_star_args)}, {yp_CONST_REF(s_star_star_kwargs)}));


static MunitResult test_generatorC(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    void         *pointer = (void *)(yp_ssize_t)munit_rand_uint32();  // Not a valid pointer!
    ypObject     *items[4];
    obj_array_fill(items, uq, fixture_type_iter->rand_items);

    // Basic generatorC.
    {
        ypObject           *iter;
        count_down_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t          iter_state_size;
        count_down_state_t  state = {2, items[0], pointer};  // items[0] is borrowed.
        yp_generator_decl_t decl = {count_down_iter_func, 3, &state, &count_down_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, yp_sizeof(count_down_state_t));
        assert_ptr(iter_state, !=, &state);  // It must be a copy.
        assert_intC(iter_state->count, ==, 2);
        assert_obj(iter_state->object, is, items[0]);
        assert_ptr(iter_state->pointer, ==, pointer);

        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 3);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_i_two));
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 2);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_i_one));
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 1);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_i_zero));
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        assert_intC(iter_state->count, ==, -1);
        assert_obj(iter_state->object, is, items[0]);
        assert_ptr(iter_state->pointer, ==, pointer);

        yp_decrefN(N(iter));
    }

    // FIXME More tests?

    // NULL object pointers in state are initialized to yp_None.
    {
        ypObject           *iter;
        count_down_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t          iter_state_size;
        count_down_state_t  state = {2, NULL, pointer};  // object is NULL.
        yp_generator_decl_t decl = {count_down_iter_func, 3, &state, &count_down_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, yp_sizeof(count_down_state_t));
        assert_ptr(iter_state, !=, &state);  // It must be a copy.
        assert_intC(iter_state->count, ==, 2);
        assert_obj(iter_state->object, is, yp_None);
        assert_ptr(iter_state->pointer, ==, pointer);

        yp_decrefN(N(iter));
    }

    // A NULL state initializes object pointers to yp_None, pointers to NULL, and values to zero.
    {
        ypObject           *iter;
        count_down_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t          iter_state_size;
        yp_generator_decl_t decl = {count_down_iter_func, 1, NULL, &count_down_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, yp_sizeof(count_down_state_t));
        assert_intC(iter_state->count, ==, 0);
        assert_obj(iter_state->object, is, yp_None);
        assert_null(iter_state->pointer);

        yp_decrefN(N(iter));
    }

    // A NULL state declaration means there is no state.
    {
        ypObject           *iter;
        count_down_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t          iter_state_size;
        count_down_state_t  state = {2, items[0], pointer};  // Ignored.
        yp_generator_decl_t decl = {count_down_iter_func, 3, &state, NULL};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, 0);
        assert_ptr(iter_state, !=, &state);  // It must be a copy. FIXME

        yp_decrefN(N(iter));
    }

    // Large length hints are clamped to ypIter_LENHINT_MAX.
    if (yp_SSIZE_T_MAX > ypIter_LENHINT_MAX) {
        ypObject           *iter;
        yp_generator_decl_t decl = {count_down_iter_func, yp_SSIZE_T_MAX, NULL, NULL};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_intC_exc(yp_length_hintC(iter, &exc), ==, ypIter_LENHINT_MAX);

        yp_decrefN(N(iter));
    }

    // state_decl.size cannot be negative.
    {
        static yp_state_decl_t state_decl = {-1};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // Excessively-large state_decl.size.
    {
        static yp_state_decl_t state_decl = {yp_SSIZE_T_MAX};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_MemorySizeOverflowError);
    }

    // state_decl.offsets_len == -1 (array of objects) is not yet implemented.
    {
        static yp_state_decl_t state_decl = {sizeof(ypObject *), -1};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_NotImplementedError);
    }

    // state_decl.offsets_len must be >= 0 or -1.
    {
        static yp_state_decl_t state_decl = {sizeof(ypObject *), -2};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // state_decl offsets cannot be negative.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), 1, {-1}};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // state_decl objects must be fully contained in state.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *) - 1, 1, {0}};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // state_decl objects must be aligned.
    {
        static yp_state_decl_t state_decl = {1 + yp_sizeof(ypObject *), 1, {1}};
        yp_generator_decl_t    decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_SystemLimitationError);
    }

    // There is a maximum to the state_decl object offsets.
    {
        static yp_state_decl_t state_decl = {
                33 * yp_sizeof(ypObject *), 1, {32 * yp_sizeof(ypObject *)}};
        yp_generator_decl_t decl = {count_down_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_SystemLimitationError);
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// XXX test_iter, aka test_new, is tested in test_iterable.

// FIXME Improve these tests once function supports state (then we can track per-function state).
static void _test_new2(ypObject *(*any_new2)(ypObject *, ypObject *))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_callable = rand_obj_any_not_callable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, fixture_type_iter->rand_items);

    // Basic new.
    {
        ypObject *iter = any_new2(yp_t_tuple, yp_None);
        assert_type_is(iter, yp_t_iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_tuple_empty));
        ead(item, yp_next(iter), assert_obj(item, eq, yp_tuple_empty));
        // FIXME Our test callable will never yield yp_None. We need a stateful callable.
        assert_not_raises_exc(yp_close(iter, &exc));
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    // Iterator is empty.
    {
        ypObject *iter = any_new2(yp_t_tuple, yp_tuple_empty);
        assert_type_is(iter, yp_t_iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    // Callable requires arguments.
    ead(iter, any_new2(yp_func_chr, yp_None), assert_raises(yp_next(iter), yp_TypeError));
    ead(iter, any_new2(yp_t_type, yp_None), assert_raises(yp_next(iter), yp_TypeError));
    ead(iter, any_new2(func_required_keyword, yp_None), assert_raises(yp_next(iter), yp_TypeError));

    // Callable raises an exception.
    ead(iter, any_new2(func_SyntaxError, yp_None), assert_raises(yp_next(iter), yp_SyntaxError));

    // x is not callable.
    assert_raises(any_new2(not_callable, yp_None), yp_TypeError);

    // Exception passthrough.
    assert_isexception(any_new2(yp_SyntaxError, yp_None), yp_SyntaxError);
    assert_isexception(any_new2(yp_t_tuple, yp_SyntaxError), yp_SyntaxError);

    obj_array_decref(items);
    yp_decrefN(N(not_callable));
    uniqueness_dealloc(uq);
}

static MunitResult test_new2(const MunitParameter params[], fixture_t *fixture)
{
    _test_new2(yp_iter2);

    return MUNIT_OK;
}

static ypObject *new2_to_call_t_iter(ypObject *callable, ypObject *sentinel)
{
    return yp_callN(yp_t_iter, 2, callable, sentinel);
}

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *not_callable = rand_obj_any_not_callable(uq);
    ypObject     *str_cls = yp_str_frombytesC2(-1, "cls");
    ypObject     *str_object = yp_str_frombytesC2(-1, "object");
    ypObject     *str_sentinel = yp_str_frombytesC2(-1, "sentinel");
    ypObject     *str_rand = rand_obj(uq, fixture_type_str);

    // XXX Most of the single-argument form is tested in test_iterable.

    // Shared tests.
    _test_new2(new2_to_call_t_iter);

    // Invalid arguments.
    {
        ypObject *args_three = yp_tupleN(N(yp_t_tuple, yp_tuple_empty, yp_tuple_empty));
        ypObject *kwargs_object = yp_frozendictK(K(str_object, yp_t_tuple));
        ypObject *kwargs_sentinel =
                yp_frozendictK(K(str_object, yp_t_tuple, str_sentinel, yp_tuple_empty));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, yp_t_iter, str_object, yp_t_tuple));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, yp_t_tuple));

        assert_raises(yp_callN(yp_t_iter, 0), yp_TypeError);
        assert_raises(
                yp_callN(yp_t_iter, N(yp_t_tuple, yp_tuple_empty, yp_tuple_empty)), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, args_three, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_object), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_sentinel), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_cls), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_rand), yp_TypeError);

        yp_decrefN(N(kwargs_rand, kwargs_cls, kwargs_sentinel, kwargs_object, args_three));
    }

    // x is not an iterable.
    assert_raises(yp_callN(yp_t_iter, N(not_iterable)), yp_TypeError);

    // x is not callable.
    assert_raises(yp_callN(yp_t_iter, N(not_callable, yp_None)), yp_TypeError);

    // Exception passthrough.
    assert_isexception(yp_callN(yp_t_iter, N(yp_SyntaxError)), yp_SyntaxError);
    assert_isexception(yp_callN(yp_t_iter, N(yp_SyntaxError, yp_t_tuple)), yp_SyntaxError);
    assert_isexception(yp_callN(yp_t_iter, N(yp_t_tuple, yp_SyntaxError)), yp_SyntaxError);

    yp_decrefN(N(str_rand, str_sentinel, str_object, str_cls, not_callable, not_iterable));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_bool(const MunitParameter params[], fixture_t *fixture)
{
    // All iters are truthy.
    {
        ypObject *iter = rand_obj(NULL, fixture_type_iter);
        assert_obj(yp_bool(iter), is, yp_True);
        yp_decref(iter);
    }

    return MUNIT_OK;
}

// FIXME yp_send, yp_next, yp_next2, yp_throw, yp_length_hintC. And also in test_iterable?

// FIXME Should this be part of test_iterable??
static MunitResult test_close(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    void         *pointer = (void *)(yp_ssize_t)munit_rand_uint32();  // Not a valid pointer!
    ypObject     *items[4];
    obj_array_fill(items, uq, fixture_type_iter->rand_items);

    // Basic close.
    {
        ypObject           *iter;
        count_down_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t          iter_state_size;
        count_down_state_t  state = {2, items[0], pointer};  // items[0] is borrowed.
        yp_generator_decl_t decl = {count_down_iter_func, 3, &state, &count_down_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ptr(iter_state, !=, &state);  // It must be a copy.
        assert_intC(iter_state->count, ==, 2);
        assert_obj(iter_state->object, is, items[0]);
        assert_ptr(iter_state->pointer, ==, pointer);

        assert_not_raises_exc(yp_close(iter, &exc));

        assert_intC(iter_state->count, ==, 2);
        assert_obj(iter_state->object, is, items[0]);  // ...but it's decref'd.
        assert_ptr(iter_state->pointer, ==, pointer);

        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // FIXME More tests?

    // Unexpectedly-yielded value on close.
    {
        ypObject *iter;
        // rand_obj_any_iter_func returns new objects, even on close.
        yp_generator_decl_t decl = {rand_obj_any_iter_func, 3};
        assert_not_raises(iter = yp_generatorC(&decl));
        assert_raises_exc(yp_close(iter, &exc), yp_RuntimeError);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    // Unexpected exception on close.
    {
        ypObject *iter;
        // SyntaxError_iter_func always raises yp_SyntaxError, even on close.
        yp_generator_decl_t decl = {SyntaxError_iter_func, 3};
        assert_not_raises(iter = yp_generatorC(&decl));
        assert_raises_exc(yp_close(iter, &exc), yp_SyntaxError);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// FIXME test_oom: yp_generatorC alloc, _ypMiIter_fromminiiter, yp_iter2
static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    obj_array_fill(items, uq, fixture_type_iter->rand_items);

    // yp_generatorC
    {
        yp_generator_decl_t decl = {rand_obj_any_iter_func};
        malloc_tracker_oom_after(0);
        assert_raises(yp_generatorC(&decl), yp_MemoryError);
        malloc_tracker_oom_disable();
    }

    // _ypMiIter_fromminiiter
    {
        ypObject *x = yp_tupleN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_iter(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // yp_iter2
    {
        malloc_tracker_oom_after(0);
        assert_raises(yp_iter2(yp_t_tuple, yp_None), yp_MemoryError);
        malloc_tracker_oom_disable();
    }

    // _ypSequence_miniiter_next, getindex alloc fails
    {
        // Pick a value that avoids the preallocated ints, such that range must allocate a new int.
        yp_int32_t i = munit_rand_int_range(1000, INT_MAX);
        ypObject  *x = yp_rangeC3(i, i + 1, 1);
        ypObject  *iter = yp_iter(x);
        malloc_tracker_oom_after(0);
        assert_raises(yp_next(iter), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(iter, x));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}


MunitTest test_iter_tests[] = {TEST(test_generatorC, NULL), TEST(test_new2, NULL),
        TEST(test_call_type, NULL), TEST(test_bool, NULL), TEST(test_close, NULL),
        TEST(test_oom, NULL), {NULL}};


extern void test_iter_initialize(void) {}

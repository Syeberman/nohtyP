#include "munit_test/unittest.h"

// test_iterable validates the behaviour for iterators of iterables. This file validates all other
// iterator-related behaviour, in particular generators and yp_iter2.


#define ypIter_LENHINT_MAX 0x7FFFFFFF  // From nohtyP.c.


typedef struct _count_down_state_t {
    yp_int_t count;
} count_down_state_t;

static yp_state_decl_t count_down_state_decl = {sizeof(count_down_state_t)};

// Used as the func for a generator. Yields ints from count to zero.
static ypObject *count_down_iter_func(ypObject *g, ypObject *value)
{
    count_down_state_t *state;
    yp_ssize_t          size;
    ypObject           *result;
    assert_not_exception(yp_iter_stateCX(g, &size, (void **)&state));
    assert_ssizeC(size, ==, yp_sizeof(*state));

    if (yp_isexceptionC(value)) return value;
    if (state->count < 0) return yp_StopIteration;

    assert_not_raises(result = yp_intC(state->count));
    state->count--;
    return result;
}

typedef struct _sample_types_state_t {
    yp_int_t  integer;
    ypObject *object;
    void     *pointer;
} sample_types_state_t;

static yp_state_decl_t sample_types_state_decl = {
        sizeof(sample_types_state_t), 1, {yp_offsetof(sample_types_state_t, object)}};

typedef struct _scripted_generator_state_items_t {
    ypObject *to_yield;
    ypObject *sent;
} scripted_generator_state_items_t;

typedef struct _scripted_generator_state_t {
    yp_ssize_t                       n;
    scripted_generator_state_items_t items[4];
    yp_ssize_t                       i;
} scripted_generator_state_t;

static yp_state_decl_t scripted_generator_state_decl = {sizeof(scripted_generator_state_t), 6,
        {
                yp_offsetof(scripted_generator_state_t, items[0].to_yield),
                yp_offsetof(scripted_generator_state_t, items[0].sent),
                yp_offsetof(scripted_generator_state_t, items[1].to_yield),
                yp_offsetof(scripted_generator_state_t, items[1].sent),
                yp_offsetof(scripted_generator_state_t, items[2].to_yield),
                yp_offsetof(scripted_generator_state_t, items[2].sent),
                yp_offsetof(scripted_generator_state_t, items[3].to_yield),
                yp_offsetof(scripted_generator_state_t, items[3].sent),
        }};

// Used as the func for a generator. Each time it's invoked, it asserts that value is
// state->items[state->i].sent, and it returns state->items[state->i].to_yield.
static ypObject *scripted_generator_iter_func(ypObject *g, ypObject *value)
{
    yp_ssize_t lengthof_items = yp_lengthof_array_member(scripted_generator_state_t, items);
    scripted_generator_state_items_t *items;
    scripted_generator_state_t       *state;
    yp_ssize_t                        size;
    assert_not_exception(yp_iter_stateCX(g, &size, (void **)&state));
    assert_ssizeC(size, ==, yp_sizeof(*state));

    assert_ssizeC(state->n, <=, lengthof_items);
    assert_ssizeC(state->i, >=, 0);
    assert_ssizeC(state->i, <, state->n);
    items = &(state->items[state->i]);
    state->i++;

    assert_obj(value, is, items->sent);
    return yp_incref(items->to_yield);
}

#define assert_script_completed(g)                                                     \
    do {                                                                               \
        scripted_generator_state_t *_ypMT_script_state;                                \
        yp_ssize_t                  _ypMT_script_size;                                 \
        assert_not_exception(                                                          \
                yp_iter_stateCX(g, &_ypMT_script_size, (void **)&_ypMT_script_state)); \
        assert_ssizeC(_ypMT_script_size, ==, yp_sizeof(*_ypMT_script_state));          \
        assert_ssizeC(_ypMT_script_state->i, ==, _ypMT_script_state->n);               \
    } while (0)

// Used as the func for a generator. Unconditionally returns a random object.
static ypObject *rand_obj_any_iter_func(ypObject *g, ypObject *value) { return rand_obj_any(NULL); }

// Used as the code for a function. Unconditionally returns None.
static ypObject *None_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray) { return yp_None; }

// Used as the code for a function. Unconditionally raises SyntaxError.
static ypObject *SyntaxError_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return yp_SyntaxError;
}


static MunitResult test_generatorC(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    void         *pointer = (void *)(yp_ssize_t)munit_rand_uint32();  // Not a valid pointer!
    ypObject     *items[] = obj_array_init(4, rand_obj_any(uq));

    // Basic generatorC.
    {
        ypObject           *iter;
        count_down_state_t  state = {2};
        yp_generator_decl_t decl = {count_down_iter_func, 3, &state, &count_down_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 3);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_i_two));
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 2);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_i_one));
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 1);
        ead(item, yp_next(iter), assert_obj(item, eq, yp_i_zero));
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Basic state initialization.
    {
        ypObject             *iter;
        sample_types_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t            iter_state_size;
        sample_types_state_t  state = {2, items[0], pointer};  // items[0] is borrowed.
        yp_generator_decl_t   decl = {rand_obj_any_iter_func, 3, &state, &sample_types_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, yp_sizeof(sample_types_state_t));
        assert_not_null(iter_state);
        assert_ptr(iter_state, !=, &state);  // It must be a copy.
        assert_intC(iter_state->integer, ==, 2);
        assert_obj(iter_state->object, is, items[0]);
        assert_ptr(iter_state->pointer, ==, pointer);

        yp_decrefN(N(iter));
    }

    // NULL object pointers in state are initialized to yp_None.
    {
        ypObject             *iter;
        sample_types_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t            iter_state_size;
        sample_types_state_t  state = {2, NULL, pointer};  // object is NULL.
        yp_generator_decl_t   decl = {rand_obj_any_iter_func, 3, &state, &sample_types_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, yp_sizeof(sample_types_state_t));
        assert_not_null(iter_state);
        assert_ptr(iter_state, !=, &state);  // It must be a copy.
        assert_intC(iter_state->integer, ==, 2);
        assert_obj(iter_state->object, is, yp_None);  // Initialized to yp_None.
        assert_ptr(iter_state->pointer, ==, pointer);

        yp_decrefN(N(iter));
    }

    // A NULL state initializes object pointers to yp_None, pointers to NULL, and values to zero.
    {
        ypObject             *iter;
        sample_types_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t            iter_state_size;
        yp_generator_decl_t   decl = {rand_obj_any_iter_func, 1, NULL, &sample_types_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, yp_sizeof(sample_types_state_t));
        assert_not_null(iter_state);
        assert_intC(iter_state->integer, ==, 0);      // Initialized to zero.
        assert_obj(iter_state->object, is, yp_None);  // Initialized to yp_None.
        assert_null(iter_state->pointer);             // Initialized to NULL.

        yp_decrefN(N(iter));
    }

    // A NULL state declaration means there is no state.
    {
        ypObject             *iter;
        sample_types_state_t *iter_state;  // iter's copy of the state.
        yp_ssize_t            iter_state_size;
        sample_types_state_t  state = {2, items[0], pointer};  // Ignored.
        yp_generator_decl_t   decl = {rand_obj_any_iter_func, 3, &state, NULL};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises(yp_iter_stateCX(iter, &iter_state_size, (void **)&iter_state));
        assert_ssizeC(iter_state_size, ==, 0);
        assert_not_null(iter_state);
        assert_ptr(iter_state, !=, &state);

        yp_decrefN(N(iter));
    }

    // Large length hints are clamped to ypIter_LENHINT_MAX.
    if (yp_SSIZE_T_MAX > ypIter_LENHINT_MAX) {
        ypObject           *iter;
        yp_generator_decl_t decl = {rand_obj_any_iter_func, yp_SSIZE_T_MAX, NULL, NULL};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_intC_exc(yp_length_hintC(iter, &exc), ==, ypIter_LENHINT_MAX);

        yp_decrefN(N(iter));
    }

    // state_decl.size cannot be negative.
    {
        static yp_state_decl_t state_decl = {-1};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // Excessively-large state_decl.size.
    {
        static yp_state_decl_t state_decl = {yp_SSIZE_T_MAX};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_MemorySizeOverflowError);
    }

    // state_decl.offsets_len == -1 (array of objects) is not yet implemented.
    {
        static yp_state_decl_t state_decl = {sizeof(ypObject *), -1};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_NotImplementedError);
    }

    // state_decl.offsets_len must be >= 0 or -1.
    {
        static yp_state_decl_t state_decl = {sizeof(ypObject *), -2};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // state_decl offsets cannot be negative.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), 1, {-1}};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // state_decl objects must be fully contained in state.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *) - 1, 1, {0}};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_ValueError);
    }

    // state_decl objects must be aligned.
    {
        static yp_state_decl_t state_decl = {1 + yp_sizeof(ypObject *), 1, {1}};
        yp_generator_decl_t    decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_SystemLimitationError);
    }

    // There is a maximum to the state_decl object offsets.
    {
        static yp_state_decl_t state_decl = {
                33 * yp_sizeof(ypObject *), 1, {32 * yp_sizeof(ypObject *)}};
        yp_generator_decl_t decl = {rand_obj_any_iter_func, 1, NULL, &state_decl};
        assert_raises(yp_generatorC(&decl), yp_SystemLimitationError);
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// XXX test_iter, aka test_new, is tested in test_iterable.

static void _test_new2(ypObject *(*any_new2)(ypObject *, ypObject *))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_callable = rand_obj_any_not_callable(uq);
    ypObject     *s_star = yp_str_frombytesC2(-1, "*");
    ypObject     *s_star_args = yp_str_frombytesC2(-1, "*args");
    ypObject     *s_star_star_kwargs = yp_str_frombytesC2(-1, "**kwargs");
    ypObject     *s_keyword = yp_str_frombytesC2(-1, "keyword");
    ypObject     *items[] = obj_array_init(2, rand_obj_any(uq));

    // Basic new.
    {
        ypObject *iter = any_new2(yp_t_tuple, yp_None);
        assert_type_is(iter, yp_t_iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        ead(item, yp_next(iter), assert_obj(item, is, yp_tuple_empty));
        ead(item, yp_next(iter), assert_obj(item, is, yp_tuple_empty));
        // TODO Our test callable will never yield yp_None. We need a stateful callable. Until then,
        // manually close the iter.
        assert_not_raises_exc(yp_close(iter, &exc));
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    // Iterator is empty.
    {
        define_function2(func_None, None_code);
        ypObject *iter = any_new2(func_None, yp_None);
        assert_type_is(iter, yp_t_iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter, func_None));
    }

    // Callable requires arguments.
    ead(iter, any_new2(yp_func_chr, yp_None), assert_raises(yp_next(iter), yp_TypeError));
    ead(iter, any_new2(yp_t_type, yp_None), assert_raises(yp_next(iter), yp_TypeError));
    {
        define_function(func_required_keyword, None_code, ({s_star}, {s_keyword}));
        ead(iter, any_new2(func_required_keyword, yp_None),
                assert_raises(yp_next(iter), yp_TypeError));
        yp_decrefN(N(func_required_keyword));
    }

    // Callable raises an exception.
    {
        define_function(func_SyntaxError, SyntaxError_code, ({s_star_args}, {s_star_star_kwargs}));
        ead(iter, any_new2(func_SyntaxError, yp_None),
                assert_raises(yp_next(iter), yp_SyntaxError));
        yp_decrefN(N(func_SyntaxError));
    }

    // x is not callable.
    assert_raises(any_new2(not_callable, yp_None), yp_TypeError);

    // Exception passthrough.
    assert_isexception(any_new2(yp_SyntaxError, yp_None), yp_SyntaxError);
    assert_isexception(any_new2(yp_t_tuple, yp_SyntaxError), yp_SyntaxError);

    obj_array_decref(items);
    yp_decrefN(N(s_keyword, s_star_star_kwargs, s_star_args, s_star, not_callable));
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
        ypObject *list_empty = yp_listN(0);
        ypObject *args_three = yp_tupleN(N(yp_t_tuple, list_empty, list_empty));
        ypObject *kwargs_object = yp_frozendictK(K(str_object, yp_t_tuple));
        ypObject *kwargs_sentinel =
                yp_frozendictK(K(str_object, yp_t_tuple, str_sentinel, list_empty));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, yp_t_iter, str_object, yp_t_tuple));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, yp_t_tuple));

        assert_raises(yp_callN(yp_t_iter, 0), yp_TypeError);
        assert_raises(yp_callN(yp_t_iter, N(yp_t_tuple, list_empty, list_empty)), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, args_three, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_object), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_sentinel), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_cls), yp_TypeError);
        assert_raises(yp_call_stars(yp_t_iter, yp_tuple_empty, kwargs_rand), yp_TypeError);

        yp_decrefN(
                N(kwargs_rand, kwargs_cls, kwargs_sentinel, kwargs_object, args_three, list_empty));
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

static void _test_next(ypObject *(*any_next)(ypObject *), int raises)
{
    ypObject     *iter_close_exceptions[] = {yp_StopIteration, yp_GeneratorExit, NULL};
    ypObject     *syntax_genExit[] = {yp_SyntaxError, yp_GeneratorExit, NULL};
    ypObject     *osErr_stopIter_genExit[] = {yp_OSError, yp_StopIteration, yp_GeneratorExit, NULL};
    ypObject    **exception;
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[] = obj_array_init(2, rand_obj_any(uq));

#define assert_exhausted(expression)                       \
    do {                                                   \
        if (raises) {                                      \
            assert_raises((expression), yp_StopIteration); \
        } else {                                           \
            assert_obj((expression), is, yp_None);         \
        }                                                  \
    } while (0)

    // Basic next.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {4,
                {{items[0]}, {items[1]}, {yp_StopIteration}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        ead(item, any_next(iter), assert_obj(item, is, items[0]));
        ead(item, any_next(iter), assert_obj(item, is, items[1]));
        assert_exhausted(any_next(iter));
        assert_script_completed(iter);
        assert_exhausted(any_next(iter));

        yp_decrefN(N(iter));
    }

    // Empty generator.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{yp_StopIteration}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 0, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_exhausted(any_next(iter));
        assert_script_completed(iter);
        assert_exhausted(any_next(iter));

        yp_decrefN(N(iter));
    }

    // Generator raises an exception (that is not yp_StopIteration).
    for (exception = syntax_genExit; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                3, {{items[0]}, {*exception}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator yields a value. The second call raises an unexpected
        // exception. Because of the exception, the iterator is closed, which raises
        // yp_GeneratorExit as expected. Because the iterator is closed, the third call behaves as
        // if the iterator is exhausted.
        ead(item, any_next(iter), assert_obj(item, is, items[0]));
        assert_raises(any_next(iter), *exception);
        assert_script_completed(iter);
        assert_exhausted(any_next(iter));

        yp_decrefN(N(iter));
    }

    // yp_GeneratorExit or yp_StopIteration raised on close.
    for (exception = iter_close_exceptions; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{yp_StopIteration}, {*exception, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 0, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator raises yp_StopIteration. Because it's exhausted, the
        // iterator is closed, which raises an expected exception. Because the iterator is closed,
        // the second call behaves as if the iterator is exhausted.
        assert_exhausted(any_next(iter));
        assert_script_completed(iter);
        assert_exhausted(any_next(iter));

        yp_decrefN(N(iter));
    }

    // Unexpectedly-yielded value on close.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {2, {{yp_StopIteration}, {items[0], yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 0, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator raises yp_StopIteration. Because it's exhausted, the
        // iterator is closed, which unexpectedly returns a value, so close raises yp_RuntimeError.
        // Because the iterator is closed, the second call behaves as if the iterator is exhausted.
        assert_raises(any_next(iter), yp_RuntimeError);
        assert_script_completed(iter);
        assert_exhausted(any_next(iter));

        yp_decrefN(N(iter));
    }

    // Unexpected exception on close.
    for (exception = osErr_stopIter_genExit; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                3, {{items[0]}, {*exception}, {yp_SyntaxError, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator yields a value. The second call raises an exception.
        // Because it's exhausted, the iterator is closed, which raises an unexpected exception,
        // replacing the first exception. Because the iterator is closed, the third call behaves as
        // if the iterator is exhausted.
        ead(item, any_next(iter), assert_obj(item, is, items[0]));
        assert_raises(any_next(iter), yp_SyntaxError);
        assert_script_completed(iter);
        assert_exhausted(any_next(iter));

        yp_decrefN(N(iter));
    }

    // Exception passthrough.
    assert_raises(any_next(yp_SyntaxError), yp_SyntaxError);

#undef assert_exhausted

    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static ypObject *next_to_send(ypObject *iterator) { return yp_send(iterator, yp_None); }

static MunitResult test_send(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *values[] = obj_array_init(4, rand_obj_any(uq));
    ypObject     *items[] = obj_array_init(2, rand_obj_any(uq));

    // Shared tests.
    _test_next(next_to_send, /*raises=*/TRUE);

    // Basic send.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                4, {{items[0], values[0]}, {items[1], values[1]}, {yp_StopIteration, values[2]},
                           {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        ead(item, yp_send(iter, values[0]), assert_obj(item, is, items[0]));
        ead(item, yp_send(iter, values[1]), assert_obj(item, is, items[1]));
        assert_raises(yp_send(iter, values[2]), yp_StopIteration);
        assert_script_completed(iter);
        assert_raises(yp_send(iter, values[3]), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Exception passthrough.
    assert_raises(yp_send(yp_SyntaxError, values[0]), yp_SyntaxError);
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{items[0], values[0]}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_raises(yp_send(iter, yp_SyntaxError), yp_SyntaxError);
        // Exception passthrough does not close the iterator.
        ead(item, yp_send(iter, values[0]), assert_obj(item, is, items[0]));

        yp_decrefN(N(iter));
    }

    obj_array_decref(items);
    obj_array_decref(values);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_next(const MunitParameter params[], fixture_t *fixture)
{
    _test_next(yp_next, /*raises=*/TRUE);
    return MUNIT_OK;
}

static ypObject *next_to_next2(ypObject *iterator) { return yp_next2(iterator, yp_None); }

static MunitResult test_next2(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defaults[] = obj_array_init(4, rand_obj_any(uq));
    ypObject     *items[] = obj_array_init(2, rand_obj_any(uq));

    // Shared tests.
    _test_next(next_to_next2, /*raises=*/FALSE);

    // Basic next2.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {4,
                {{items[0]}, {items[1]}, {yp_StopIteration}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        ead(item, yp_next2(iter, defaults[0]), assert_obj(item, is, items[0]));
        ead(item, yp_next2(iter, defaults[1]), assert_obj(item, is, items[1]));
        ead(item, yp_next2(iter, defaults[2]), assert_obj(item, is, defaults[2]));
        assert_script_completed(iter);
        ead(item, yp_next2(iter, defaults[3]), assert_obj(item, is, defaults[3]));

        yp_decrefN(N(iter));
    }

    // Exception passthrough.
    assert_raises(yp_next2(yp_SyntaxError, defaults[0]), yp_SyntaxError);
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {2, {{items[0]}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_raises(yp_next2(iter, yp_SyntaxError), yp_SyntaxError);
        // Exception passthrough does not close the iterator.
        ead(item, yp_next2(iter, defaults[0]), assert_obj(item, is, items[0]));

        yp_decrefN(N(iter));
    }

    obj_array_decref(items);
    obj_array_decref(defaults);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_throw(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *iter_close_exceptions[] = {yp_StopIteration, yp_GeneratorExit, NULL};
    ypObject *syntax_stopIter_genExit[] = {
            yp_SyntaxError, yp_StopIteration, yp_GeneratorExit, NULL};
    ypObject    **exception;
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[] = obj_array_init(2, rand_obj_any(uq));

    // Basic throw, including yp_StopIteration and yp_GeneratorExit.
    for (exception = syntax_stopIter_genExit; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                3, {{items[0]}, {*exception, *exception}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        ead(item, yp_next(iter), assert_obj(item, is, items[0]));
        assert_raises(yp_throw(iter, *exception), *exception);
        assert_script_completed(iter);
        assert_raises(yp_throw(iter, *exception), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Generator ignores the exception.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                3, {{items[0], yp_SyntaxError}, {yp_StopIteration, yp_SyntaxError},
                           {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        ead(item, yp_throw(iter, yp_SyntaxError), assert_obj(item, is, items[0]));
        assert_raises(yp_throw(iter, yp_SyntaxError), yp_StopIteration);
        assert_script_completed(iter);
        assert_raises(yp_throw(iter, yp_SyntaxError), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Generator raises a different exception.
    for (exception = syntax_stopIter_genExit; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{*exception, yp_OSError}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_raises(yp_throw(iter, yp_OSError), *exception);
        assert_script_completed(iter);
        assert_raises(yp_throw(iter, yp_OSError), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // yp_GeneratorExit or yp_StopIteration raised on close.
    for (exception = iter_close_exceptions; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{yp_SyntaxError, yp_SyntaxError}, {*exception, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator throws an exception. Because of the exception, the
        // iterator is closed, which raises an expected exception. Because the iterator is closed,
        // the second call behaves as if the iterator is exhausted.
        assert_raises(yp_throw(iter, yp_SyntaxError), yp_SyntaxError);
        assert_script_completed(iter);
        assert_raises(yp_throw(iter, yp_SyntaxError), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Unexpectedly-yielded value on close.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{yp_SyntaxError, yp_SyntaxError}, {items[0], yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator throws an exception. Because of the exception, the
        // iterator is closed, which unexpectedly returns a value, so close raises yp_RuntimeError,
        // overriding the first exception. Because the iterator is closed, the second call behaves
        // as if the iterator is exhausted.
        assert_raises(yp_throw(iter, yp_SyntaxError), yp_RuntimeError);
        assert_script_completed(iter);
        assert_raises(yp_throw(iter, yp_SyntaxError), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Unexpected exception on close.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {
                2, {{yp_OSError, yp_OSError}, {yp_SyntaxError, yp_GeneratorExit}}};
        yp_generator_decl_t decl = {
                scripted_generator_iter_func, 1, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        // On the first call, the generator throws an exception. Because of the exception, the
        // iterator is closed, which raises an unexpected exception, overriding the first exception.
        // Because the iterator is closed, the second call behaves as if the iterator is exhausted.
        assert_raises(yp_throw(iter, yp_OSError), yp_SyntaxError);
        assert_script_completed(iter);
        assert_raises(yp_throw(iter, yp_OSError), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // exception is not an exception.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {2, {{items[0]}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_raises(yp_throw(iter, items[1]), yp_TypeError);
        // "Not an exception" does not close the iterator.
        ead(item, yp_next(iter), assert_obj(item, is, items[0]));

        yp_decrefN(N(iter));
    }

    // Exception passthrough.
    assert_raises(yp_throw(yp_SyntaxError, yp_Exception), yp_SyntaxError);

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_close(const MunitParameter params[], fixture_t *fixture)
{
    ypObject     *iter_close_exceptions[] = {yp_StopIteration, yp_GeneratorExit, NULL};
    ypObject    **exception;
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[] = obj_array_init(4, rand_obj_any(uq));

    // Basic close.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {2, {{items[0]}, {yp_GeneratorExit, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 2);
        ead(item, yp_next(iter), assert_obj(item, is, items[0]));

        assert_not_raises_exc(yp_close(iter, &exc));
        assert_script_completed(iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // yp_GeneratorExit or yp_StopIteration raised on close.
    for (exception = iter_close_exceptions; *exception != NULL; exception++) {
        ypObject                  *iter;
        scripted_generator_state_t state = {1, {{*exception, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_not_raises_exc(yp_close(iter, &exc));
        assert_script_completed(iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Unexpectedly-yielded value on close.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {1, {{items[0], yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_raises_exc(yp_close(iter, &exc), yp_RuntimeError);
        assert_script_completed(iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Unexpected exception on close.
    {
        ypObject                  *iter;
        scripted_generator_state_t state = {1, {{yp_SyntaxError, yp_GeneratorExit}}};
        yp_generator_decl_t        decl = {
                scripted_generator_iter_func, 2, &state, &scripted_generator_state_decl};
        assert_not_raises(iter = yp_generatorC(&decl));

        assert_raises_exc(yp_close(iter, &exc), yp_SyntaxError);
        assert_script_completed(iter);
        assert_intC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);

        yp_decrefN(N(iter));
    }

    // Exception passthrough.
    assert_raises_exc(yp_close(yp_SyntaxError, &exc), yp_SyntaxError);

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[] = obj_array_init(2, rand_obj_any(uq));

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
        TEST(test_call_type, NULL), TEST(test_bool, NULL), TEST(test_send, NULL),
        TEST(test_next, NULL), TEST(test_next2, NULL), TEST(test_throw, NULL),
        TEST(test_close, NULL), TEST(test_oom, NULL), {NULL}};


extern void test_iter_initialize(void) {}

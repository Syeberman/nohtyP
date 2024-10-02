
#include "munit_test/unittest.h"


// TODO "Shared key" versions, somehow? fixture_type_frozendict_shared, fixture_type_dict_shared
#define x_types_init()                                                                 \
    {fixture_type_frozenset, fixture_type_set, fixture_type_iter, fixture_type_tuple,  \
            fixture_type_list, fixture_type_frozenset_dirty, fixture_type_set_dirty,   \
            fixture_type_frozendict, fixture_type_dict, fixture_type_frozendict_dirty, \
            fixture_type_dict_dirty, NULL}

#define friend_types_init()                                                  \
    {fixture_type_frozenset, fixture_type_set, fixture_type_frozenset_dirty, \
            fixture_type_set_dirty, NULL}

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
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *items[2];
    obj_array_fill(items, type->rand_items);

    // Basic newN.
    {
        ypObject *so = any_newN(N(items[0], items[1]));
        assert_type_is(so, type->type);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // n is zero.
    {
        ypObject *so = any_newN(0);
        assert_type_is(so, type->type);
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // n is negative.
    {
        ypObject *so = any_newN(-1);
        assert_type_is(so, type->type);
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Duplicate arguments.
    {
        ypObject *so = any_newN(N(items[0], items[0], items[1], items[1]));
        assert_type_is(so, type->type);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Unhashable argument.
    {
        ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
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
}

static void _test_new(
        fixture_type_t *type, ypObject *(*any_new)(ypObject *), int test_exception_passthrough)
{
    fixture_type_t    *x_types[] = x_types_init();
    fixture_type_t    *friend_types[] = friend_types_init();
    fixture_type_t   **x_type;
    hashability_pair_t pair = rand_obj_any_hashability_pair();
    ypObject          *not_iterable = rand_obj_any_not_iterable();
    ypObject          *items[2];
    obj_array_fill(items, type->rand_items);

    // Basic new.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *so = any_new(x);
        assert_type_is(so, type->type);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(0);
        ypObject *so = any_new(x);
        assert_type_is(so, type->type);
        assert_len(so, 0);
        yp_decrefN(N(so, x));
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(N(items[0], items[0], items[1]));
        ypObject *so = any_new(x);
        assert_setlike(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, items);
            ead(x, (*x_type)->newN(N(unhashable)), assert_raises(any_new(x), yp_TypeError));
            ead(x, (*x_type)->newN(N(items[0], unhashable)),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x, (*x_type)->newN(N(items[0], items[1], unhashable)),
                    assert_raises(any_new(x), yp_TypeError));
            yp_decrefN(N(unhashable));
        }
    }

    // Unhashable item rejected even if equal to other hashable item.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (!(*x_type)->hashable_items_only) {
            ead(x, (*x_type)->newN(N(pair.hashable, pair.unhashable)),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x, (*x_type)->newN(N(pair.unhashable, pair.hashable)),
                    assert_raises(any_new(x), yp_TypeError));
        }
    }

    // Optimization: lazy shallow copy of a friendly immutable x to immutable so.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *so = any_new(x);
        if (type->is_mutable || (*x_type)->is_mutable) {
            assert_obj(so, is_not, x);
        } else {
            assert_obj(so, is, x);
        }
        yp_decrefN(N(so, x));
    }

    // Optimization: empty immortal when x is empty.
    if (type->falsy != NULL) {
        for (x_type = x_types; (*x_type) != NULL; x_type++) {
            ypObject *x = (*x_type)->newN(0);
            ypObject *so = any_new(x);
            assert_obj(so, is, type->falsy);
            yp_decrefN(N(so, x));
        }
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

    if (type->type == yp_t_frozenset) {
        newN = yp_frozensetN;
        newN_to_newNV = newN_to_frozensetNV;
    } else {
        assert_ptr(type->type, ==, yp_t_set);
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

    if (type->type == yp_t_frozenset) {
        newN_to_new = newN_to_frozenset;
        new_ = yp_frozenset;
    } else {
        assert_ptr(type->type, ==, yp_t_set);
        newN_to_new = newN_to_set;
        new_ = yp_set;
    }

    // Shared tests.
    _test_newN(type, newN_to_new, /*test_exception_passthrough=*/FALSE);
    _test_new(type, new_, /*test_exception_passthrough=*/TRUE);

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
    ypObject *str_iterable = yp_str_frombytesC2(-1, "iterable");
    ypObject *str_cls = yp_str_frombytesC2(-1, "cls");

    if (type->type == yp_t_frozenset) {
        newN_to_call_type = newN_to_call_t_frozenset;
        new_to_call_type = new_to_call_t_frozenset;
    } else {
        assert_ptr(type->type, ==, yp_t_set);
        newN_to_call_type = newN_to_call_t_set;
        new_to_call_type = new_to_call_t_set;
    }

    // Shared tests.
    _test_newN(type, newN_to_call_type, /*test_exception_passthrough=*/FALSE);
    _test_new(type, new_to_call_type, /*test_exception_passthrough=*/TRUE);

    // Zero arguments.
    {
        ypObject *so = yp_callN(type->type, 0);
        assert_type_is(so, type->type);
        assert_len(so, 0);
        yp_decrefN(N(so));
    }

    // Optimization: empty immortal with zero arguments.
    if (type->falsy != NULL) {
        assert_obj(yp_callN(type->type, 0), is, type->falsy);
    }

    // Invalid arguments.
    {
        ypObject *args_two = yp_tupleN(N(yp_frozenset_empty, yp_frozenset_empty));
        ypObject *kwargs_iterable = yp_frozendictK(K(str_iterable, yp_frozenset_empty));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, type->type));

        assert_raises(
                yp_callN(type->type, N(yp_frozenset_empty, yp_frozenset_empty)), yp_TypeError);
        assert_raises(yp_call_stars(type->type, args_two, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_iterable), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_cls), yp_TypeError);

        yp_decrefN(N(kwargs_cls, kwargs_iterable, args_two));
    }

    // Exception passthrough.
    assert_isexception(yp_callN(type->type, N(yp_SyntaxError)), yp_SyntaxError);

    yp_decrefN(N(str_iterable, str_cls));
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
    ypObject *items[2];
    obj_array_fill(items, type->rand_items);

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
    return MUNIT_OK;
}


char *param_values_test_frozenset[] = {"frozenset", "set", "frozenset_dirty", "set_dirty", NULL};

static MunitParameterEnum test_frozenset_params[] = {
        {param_key_type, param_values_test_frozenset}, {NULL}};

MunitTest test_frozenset_tests[] = {TEST(test_newN, test_frozenset_params),
        TEST(test_new, test_frozenset_params), TEST(test_call_type, test_frozenset_params),
        TEST(test_miniiter, test_frozenset_params), {NULL}};


extern void test_frozenset_initialize(void) {}

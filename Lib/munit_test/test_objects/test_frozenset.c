
#include "munit_test/unittest.h"


#define x_types_init()                                                                   \
    {                                                                                    \
        fixture_type_frozenset, fixture_type_set, fixture_type_iter, fixture_type_tuple, \
                fixture_type_list, fixture_type_frozenset_dirty, fixture_type_set_dirty, \
                fixture_type_frozendict, fixture_type_dict, NULL                         \
    }

#define friend_types_init()                                                     \
    {                                                                           \
        fixture_type_frozenset, fixture_type_set, fixture_type_frozenset_dirty, \
                fixture_type_set_dirty, NULL                                    \
    }

// Returns true iff type can store unhashable objects.
static int type_stores_unhashables(fixture_type_t *type)
{
    return !type->is_setlike && !type->is_mapping;
}


static MunitResult _test_newN(fixture_type_t *type, ypObject *(*any_newN)(int, ...))
{
    ypObject *items[2];
    obj_array_fill(items, type->rand_items);

    // Basic newN.
    {
        ypObject *so = any_newN(N(items[0], items[1]));
        assert_type_is(so, type->type);
        assert_set(so, items[0], items[1]);
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so));
    }

    // Unhashable argument.
    {
        ypObject *unhashable = rand_obj_any_mutable();
        assert_raises(any_newN(N(unhashable)), yp_TypeError);
        assert_raises(any_newN(N(items[0], unhashable)), yp_TypeError);
        assert_raises(any_newN(N(items[0], items[1], unhashable)), yp_TypeError);
        yp_decrefN(N(unhashable));
    }

    // Optimization: empty immortal when n is zero.
    if (type->falsy != NULL) {
        assert_obj(any_newN(0), is, type->falsy);
        assert_obj(any_newN(-1), is, type->falsy);
    }

    // Exception passthrough.
    assert_isexception(any_newN(N(yp_SyntaxError)), yp_SyntaxError);
    assert_isexception(any_newN(N(yp_None, yp_SyntaxError)), yp_SyntaxError);

    obj_array_decref(items);
    return MUNIT_OK;
}

static MunitResult _test_new(fixture_type_t *type, ypObject *(*any_new)(ypObject *))
{
    fixture_type_t  *x_types[] = x_types_init();
    fixture_type_t  *friend_types[] = friend_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[2];
    obj_array_fill(items, type->rand_items);

    // Basic new.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(N(items[0], items[1]));
        ypObject *so = any_new(x);
        assert_type_is(so, type->type);
        assert_set(so, items[0], items[1]);
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
        assert_set(so, items[0], items[1]);
        yp_decrefN(N(so, x));
    }

    // x contains an unhashable object.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable();
            ead(x, (*x_type)->newN(N(unhashable)), assert_raises(any_new(x), yp_TypeError));
            ead(x, (*x_type)->newN(N(items[0], unhashable)),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x, (*x_type)->newN(N(items[0], items[1], unhashable)),
                    assert_raises(any_new(x), yp_TypeError));
            yp_decrefN(N(unhashable));
        }
    }

    // Optimization: lazy shallow copy of an immutable x to immutable so.
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
            assert_set(so, items[0], items[1]), yp_decref(so));

    // x is not an iterable.
    assert_raises(any_new(int_1), yp_TypeError);

    // Exception passthrough.
    assert_isexception(any_new(yp_SyntaxError), yp_SyntaxError);

    obj_array_decref(items);
    yp_decrefN(N(int_1));
    return MUNIT_OK;
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
    MunitResult test_result;

    if (type->type == yp_t_frozenset) {
        newN = yp_frozensetN;
        newN_to_newNV = newN_to_frozensetNV;
    } else {
        assert_ptr(type->type, ==, yp_t_set);
        newN = yp_setN;
        newN_to_newNV = newN_to_setNV;
    }

    test_result = _test_newN(type, newN);
    if (test_result != MUNIT_OK) return test_result;

    test_result = _test_newN(type, newN_to_newNV);
    if (test_result != MUNIT_OK) return test_result;

    return MUNIT_OK;
}

static ypObject *newN_to_frozenset(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = yp_tupleNV(n, args);  // new ref
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
    iterable = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_set(iterable);
    yp_decref(iterable);
    return result;
}

static MunitResult test_new(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN_to_new)(int, ...);
    ypObject *(*new)(ypObject *);
    MunitResult test_result;

    if (type->type == yp_t_frozenset) {
        newN_to_new = newN_to_frozenset;
        new = yp_frozenset;
    } else {
        assert_ptr(type->type, ==, yp_t_set);
        newN_to_new = newN_to_set;
        new = yp_set;
    }

    test_result = _test_newN(type, newN_to_new);
    if (test_result != MUNIT_OK) return test_result;

    test_result = _test_new(type, new);
    if (test_result != MUNIT_OK) return test_result;

    return MUNIT_OK;
}

static ypObject *new_to_call_t_frozenset(ypObject *iterable)
{
    return yp_callN(yp_t_frozenset, 1, iterable);
}

static ypObject *newN_to_call_t_frozenset(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = new_to_call_t_frozenset(iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *new_to_call_t_set(ypObject *iterable) { return yp_callN(yp_t_set, 1, iterable); }

static ypObject *newN_to_call_t_set(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = new_to_call_t_set(iterable);
    yp_decref(iterable);
    return result;
}

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newN_to_call_type)(int, ...);
    ypObject *(*new_to_call_type)(ypObject *);
    ypObject   *str_iterable = yp_str_frombytesC2(-1, "iterable");
    ypObject   *str_cls = yp_str_frombytesC2(-1, "cls");
    MunitResult test_result;

    if (type->type == yp_t_frozenset) {
        newN_to_call_type = newN_to_call_t_frozenset;
        new_to_call_type = new_to_call_t_frozenset;
    } else {
        assert_ptr(type->type, ==, yp_t_set);
        newN_to_call_type = newN_to_call_t_set;
        new_to_call_type = new_to_call_t_set;
    }

    test_result = _test_newN(type, newN_to_call_type);
    if (test_result != MUNIT_OK) goto tear_down;

    test_result = _test_new(type, new_to_call_type);
    if (test_result != MUNIT_OK) goto tear_down;

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
        ypObject *kwargs_iterable = yp_frozendictK(K(str_iterable, yp_frozenset_empty));
        ypObject *kwargs_cls = yp_frozendictK(K(str_cls, type->type));

        assert_raises(
                yp_callN(type->type, N(yp_frozenset_empty, yp_frozenset_empty)), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_iterable), yp_TypeError);
        assert_raises(yp_call_stars(type->type, yp_tuple_empty, kwargs_cls), yp_TypeError);

        yp_decrefN(N(kwargs_cls, kwargs_iterable));
    }

tear_down:
    yp_decrefN(N(str_iterable, str_cls));
    return MUNIT_OK;
}

// frozenset- and set-specific miniiter tests; see test_iterable for more tests.
static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *items[2];
    obj_array_fill(items, type->rand_items);

    // Corrupted states.
    {
        yp_uint64_t mi_state;
        yp_ssize_t  length_hint;
        yp_ssize_t  i;
        yp_uint64_t bad_states[] = {0uLL, (yp_uint64_t)-1, 0x000000FF000000FFuLL};
        ypObject   *so = type->newN(N(items[0], items[1]));
        ypObject   *mi = yp_miniiter(so, &mi_state);

        for (i = 0; i < yp_lengthof_array(bad_states); i++) {
            mi_state = bad_states[i];
            assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
            munit_assert_uint64(mi_state, ==, bad_states[i]);  // unchanged

            assert_not_raises_exc(length_hint = yp_miniiter_length_hintC(mi, &mi_state, &exc));
            assert_ssizeC(length_hint, ==, 0);
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
        munit_assert_uint64(mi_state, ==, 0uLL);

        mi_state = 0x0000000000000001uLL;  // index=0, keysleft=1
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        munit_assert_uint64(mi_state, ==, 0uLL);  // keysleft set to zero to exhaust the iterator

        yp_decrefN(N(so, mi));
    }

    obj_array_decref(items);
    return MUNIT_OK;
}


// FIXME test_newN/etc don't need to test "dirty"
char *param_values_test_frozenset[] = {"frozenset", "set", "frozenset_dirty", "set_dirty", NULL};

static MunitParameterEnum test_frozenset_params[] = {
        {param_key_type, param_values_test_frozenset}, {NULL}};

MunitTest test_frozenset_tests[] = {TEST(test_newN, test_frozenset_params),
        TEST(test_new, test_frozenset_params), TEST(test_call_type, test_frozenset_params),
        TEST(test_miniiter, test_frozenset_params), {NULL}};


extern void test_frozenset_initialize(void) {}

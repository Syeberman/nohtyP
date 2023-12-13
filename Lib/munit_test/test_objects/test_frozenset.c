
#include "munit_test/unittest.h"


// FIXME Constructor tests.

static MunitResult _test_newN(fixture_type_t *type, ypObject *(*any_newN)(int, ...))
{
    // FIXME Tests all the constructors.
    return MUNIT_OK;
}

static MunitResult _test_new(fixture_type_t *type, ypObject *(*any_new)(ypObject *))
{
    // FIXME Tests all the constructors.
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
    if (test_result != MUNIT_OK) return test_result;

    test_result = _test_new(type, new_to_call_type);
    if (test_result != MUNIT_OK) return test_result;

    // FIXME additional tests specific to function objects

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
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = yp_miniiter(x, &mi_state);

        mi_state = (yp_uint64_t)-1;
        ead(next, yp_miniiter_next(mi, &mi_state), assert_raises(next, yp_StopIteration));
        // FIXME Test this state with the other miniiter functions; recall state may be modified!

        yp_decrefN(N(x, mi));
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

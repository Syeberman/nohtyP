
#include "munit_test/unittest.h"

// TODO More tests here.
// TODO Select exceptions randomly.
// TODO exception_func_new_code.


static void _test_isexceptionC2(int (*any_isexceptionC2)(ypObject *, ypObject *))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *objects[] = obj_array_init(2, rand_obj_any(uq));

    // x is not an exception.
    assert_false(any_isexceptionC2(objects[0], yp_BaseException));
    assert_false(any_isexceptionC2(objects[0], yp_Exception));
    assert_false(any_isexceptionC2(objects[0], yp_TypeError));

    // x is a different exception.
    assert_false(any_isexceptionC2(yp_ValueError, yp_TypeError));

    // x is a superexception.
    assert_false(any_isexceptionC2(yp_BaseException, yp_TypeError));
    assert_false(any_isexceptionC2(yp_Exception, yp_TypeError));

    // x is the exact same exception.
    assert_true(any_isexceptionC2(yp_BaseException, yp_BaseException));
    assert_true(any_isexceptionC2(yp_Exception, yp_Exception));
    assert_true(any_isexceptionC2(yp_TypeError, yp_TypeError));

    // x is a subexception.
    assert_true(any_isexceptionC2(yp_TypeError, yp_BaseException));
    assert_true(any_isexceptionC2(yp_TypeError, yp_Exception));

    // exception is not an exception.
    assert_false(any_isexceptionC2(objects[0], objects[1]));
    assert_false(any_isexceptionC2(yp_ValueError, objects[1]));

    obj_array_decref(objects);
    uniqueness_dealloc(uq);
}

static MunitResult test_isexceptionC2(const MunitParameter params[], fixture_t *fixture)
{
    _test_isexceptionC2(yp_isexceptionC2);
    return MUNIT_OK;
}

static int isexceptionC2_to_isexceptionCN(ypObject *x, ypObject *exception)
{
    return yp_isexceptionCN(x, 1, exception);
}

static int isexceptionCN_to_isexceptionCNV(ypObject *x, int n, ...)
{
    va_list args;
    int     result;
    va_start(args, n);
    result = yp_isexceptionCNV(x, n, args);
    va_end(args);
    return result;
}

static int isexceptionC2_to_isexceptionCNV(ypObject *x, ypObject *exception)
{
    return isexceptionCN_to_isexceptionCNV(x, 1, exception);
}

static MunitResult test_isexceptionCN(const MunitParameter params[], fixture_t *fixture)
{
    // Shared tests.
    _test_isexceptionC2(isexceptionC2_to_isexceptionCN);
    _test_isexceptionC2(isexceptionC2_to_isexceptionCNV);

    // TODO Multiple exceptions (n > 1).

    return MUNIT_OK;
}

static int isexceptionC2_to_isexception_arrayC(ypObject *x, ypObject *exception)
{
    ypObject *array[] = {exception};
    return yp_isexception_arrayC(x, 1, array);
}

// yp_isexception_arrayC is a unittest.h function, but it's useful to test it here.
static MunitResult test_isexception_arrayC(const MunitParameter params[], fixture_t *fixture)
{
    // Shared tests.
    _test_isexceptionC2(isexceptionC2_to_isexception_arrayC);

    // TODO Multiple exceptions (n > 1).

    return MUNIT_OK;
}


MunitTest test_exception_tests[] = {TEST(test_isexceptionC2, NULL), TEST(test_isexceptionCN, NULL),
        TEST(test_isexception_arrayC, NULL), {NULL}};


extern void test_exception_initialize(void) {}

#include "munit_test/unittest.h"


// Used as the code for a function. Unconditionally returns intstore zero.
static ypObject *zero_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return yp_intstoreC(0);
}

// Used as the code for a function. Unconditionally returns the first argument.
static ypObject *first_arg_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    assert_ssizeC(n, >, 0);
    return yp_incref(argarray[0]);
}


static void _test_iter(fixture_type_t *type, ypObject *(*any_iter)(ypObject *))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // Basic iter.
    {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *iter = any_iter(x);
        assert_type_is(iter, yp_t_iter);

        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 2);
        assert_not_raises(first = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 1);
        assert_not_raises(second = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);

        if (yp_eq(first, items[0]) == yp_True) {
            assert_obj(second, eq, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, eq, items[1]);
            assert_obj(second, eq, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter, x));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        ypObject *iter = any_iter(x);
        assert_type_is(iter, yp_t_iter);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter, x));
    }

    // Optimization: lazy shallow copy of a fellow iterator.
    if (type == fixture_type_iter) {
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *iter = any_iter(x);
        assert_obj(iter, is, x);
        yp_decrefN(N(iter, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *iter = any_iter(x);

        assert_not_raises(first = yp_next(iter));
        assert_not_raises(second = yp_next(iter));
        assert_raises(yp_next(iter), yp_StopIteration);

        if (first == items[0]) {
            assert_obj(second, is, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, is, items[1]);
            assert_obj(second, is, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter, x));
    }

    // x is not an iterable. yp_iter is yp_TypeError, yp_iter_keys is yp_MethodError.
    assert_raises(any_iter(not_iterable), yp_TypeError, yp_MethodError);

    // Exception passthrough.
    assert_raises(any_iter(yp_SyntaxError), yp_SyntaxError);
    assert_raises(any_iter(yp_StopIteration), yp_StopIteration);
    assert_raises(any_iter(yp_GeneratorExit), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_iter(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *not_iterable = rand_obj_any_not_iterable(NULL);

    _test_iter(fixture->type, yp_iter);

    // x is not an iterable.
    assert_raises(yp_iter(not_iterable), yp_TypeError);

    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static void _test_unpackN(fixture_type_t *type, void (*any_unpackN)(ypObject *, int, ...))
{
    ypObject     *syntax_genExit[] = {yp_SyntaxError, yp_GeneratorExit, NULL};
    ypObject    **exception;
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[3];
    obj_array_fill(items, uq, type->rand_items);

    // Basic unpackN.
    {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[0], items[1]));

        any_unpackN(x, N(&first, &second));
        assert_not_exception(first);
        assert_not_exception(second);

        if (yp_eq(first, items[0]) == yp_True) {
            assert_obj(second, eq, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, eq, items[1]);
            assert_obj(second, eq, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, x));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        // TODO When yp_raise is implemented, make n<1 raise yp_ValueError.
        any_unpackN(x, 0);
        yp_decrefN(N(x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[0], items[1]));

        any_unpackN(x, N(&first, &second));
        assert_not_exception(first);
        assert_not_exception(second);

        if (first == items[0]) {
            assert_obj(second, is, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, is, items[1]);
            assert_obj(second, is, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, x));
    }

    // Too many arguments.
    {
        ypObject *second;
        ypObject *x = type->newN(N(items[0]));
        assert_raises_exc(any_unpackN(x, N(&exc, &second)), yp_ValueError);
        assert_isexception(second, yp_ValueError);
        yp_decrefN(N(x));
    }

    // Too few arguments.
    {
        ypObject *x = type->newN(N(items[0], items[1]));
        assert_raises_exc(any_unpackN(x, N(&exc)), yp_ValueError);
        yp_decrefN(N(x));
    }

    // Iterator exception on the first item. (No yielded values, nothing to discard.)
    // XXX Includes yp_GeneratorExit, to ensure it's not confused with yp_StopIteration.
    for (exception = syntax_genExit; *exception != NULL; exception++) {
        ypObject *second;
        ypObject *x_supplier = type->newN(N(items[0], items[1]));
        ypObject *x = new_faulty_iter(x_supplier, 0, *exception, 2);
        assert_raises_exc(any_unpackN(x, N(&exc, &second)), *exception);
        assert_isexception(second, *exception);
        yp_decrefN(N(x, x_supplier));
    }

    // Iterator exception on the second item. (One yielded value discarded.)
    // XXX Includes yp_GeneratorExit, to ensure it's not confused with yp_StopIteration.
    for (exception = syntax_genExit; *exception != NULL; exception++) {
        ypObject *second;
        ypObject *x_supplier = type->newN(N(items[0], items[1]));
        ypObject *x = new_faulty_iter(x_supplier, 1, *exception, 2);
        assert_raises_exc(any_unpackN(x, N(&exc, &second)), *exception);
        assert_isexception(second, *exception);
        yp_decrefN(N(x, x_supplier));
    }

    // Iterator exception on the n+1 item. (All yielded values discarded.)
    // XXX Includes yp_GeneratorExit, to ensure it's not confused with yp_StopIteration.
    for (exception = syntax_genExit; *exception != NULL; exception++) {
        ypObject *second;
        ypObject *x_supplier = type->newN(N(items[0], items[1]));
        ypObject *x = new_faulty_iter(x_supplier, 2, *exception, 2);
        assert_raises_exc(any_unpackN(x, N(&exc, &second)), *exception);
        assert_isexception(second, *exception);
        yp_decrefN(N(x, x_supplier));
    }

    // Iterator exception after the n+1 item. The "too few arguments" yp_ValueError takes priority
    // over the iterator exception, because we don't exhaust the iterator.
    for (exception = syntax_genExit; *exception != NULL; exception++) {
        ypObject *second;
        ypObject *x_supplier = type->newN(N(items[0], items[1], items[2]));
        ypObject *x = new_faulty_iter(x_supplier, 3, *exception, 3);
        assert_raises_exc(any_unpackN(x, N(&exc, &second)), yp_ValueError);
        assert_isexception(second, yp_ValueError);
        yp_decrefN(N(x, x_supplier));
    }

    // x is not an iterable.
    assert_raises_exc(any_unpackN(not_iterable, N(&exc)), yp_TypeError);

    // Exception passthrough.
    assert_raises_exc(any_unpackN(yp_SyntaxError, N(&exc)), yp_SyntaxError);

    // Bug: yp_StopIteration was mistaken for an exhausted iterator.
    assert_raises_exc(any_unpackN(yp_StopIteration, N(&exc)), yp_StopIteration);
    assert_raises_exc(any_unpackN(yp_GeneratorExit, N(&exc)), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static void unpackN_to_unpackNV(ypObject *value, int n, ...)
{
    va_list args;
    va_start(args, n);
    yp_unpackNV(value, n, args);
    va_end(args);
}

static MunitResult test_unpackN(const MunitParameter params[], fixture_t *fixture)
{
    _test_unpackN(fixture->type, yp_unpackN);
    _test_unpackN(fixture->type, unpackN_to_unpackNV);
    return MUNIT_OK;
}

static MunitResult test_filter(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // yp_filter is not yet implemented.
    {
        ypObject *x = rand_obj(NULL, type);
        assert_raises(yp_filter(yp_None, x), yp_NotImplementedError);
        yp_decrefN(N(x));
    }

    return MUNIT_OK;
}

static MunitResult test_filterfalse(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // yp_filterfalse is not yet implemented.
    {
        ypObject *x = rand_obj(NULL, type);
        assert_raises(yp_filterfalse(yp_None, x), yp_NotImplementedError);
        yp_decrefN(N(x));
    }

    return MUNIT_OK;
}

static void _test_reversed(fixture_type_t *type, ypObject *(*any_reversed)(ypObject *))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // reversed is not supported on iterators, set-likes, and mappings.
    if (type->yp_type == yp_t_iter || type->is_setlike || type->is_mapping) {
        ypObject *x = type->newN(N(items[0], items[1]));
        assert_raises(any_reversed(x), yp_TypeError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic reversed.
    {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *iter = any_reversed(x);
        assert_type_is(iter, yp_t_iter);

        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 2);
        assert_not_raises(first = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 1);
        assert_not_raises(second = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);

        assert_obj(first, eq, items[1]);
        assert_obj(second, eq, items[0]);

        yp_decrefN(N(second, first, iter, x));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        ypObject *iter = any_reversed(x);
        assert_type_is(iter, yp_t_iter);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *iter = any_reversed(x);

        assert_not_raises(first = yp_next(iter));
        assert_not_raises(second = yp_next(iter));
        assert_raises(yp_next(iter), yp_StopIteration);

        assert_obj(first, is, items[1]);
        assert_obj(second, is, items[0]);

        yp_decrefN(N(second, first, iter, x));
    }

    // x is not an iterable.
    assert_raises(any_reversed(not_iterable), yp_TypeError);

    // Exception passthrough.
    assert_raises(any_reversed(yp_SyntaxError), yp_SyntaxError);
    assert_raises(any_reversed(yp_StopIteration), yp_StopIteration);
    assert_raises(any_reversed(yp_GeneratorExit), yp_GeneratorExit);

tear_down:
    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_reversed(const MunitParameter params[], fixture_t *fixture)
{
    _test_reversed(fixture->type, yp_reversed);
    return MUNIT_OK;
}

static ypObject *reversed_to_call_func_reversed(ypObject *obj)
{
    return yp_callN(yp_func_reversed, N(obj));
}

static MunitResult test_func_reversed(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *str_sequence = yp_str_frombytesC2(-1, "sequence");
    ypObject       *str_rand = rand_obj(NULL, fixture_type_str);

    _test_reversed(type, reversed_to_call_func_reversed);

    // Invalid arguments.
    {
        ypObject *x = rand_obj(NULL, type);
        ypObject *list_empty = yp_listN(0);
        ypObject *kwargs_sequence = yp_frozendictK(K(str_sequence, x));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, x));

        assert_raises(yp_callN(yp_func_reversed, 0), yp_TypeError);
        assert_raises(yp_callN(yp_func_reversed, N(x, list_empty)), yp_TypeError);
        assert_raises(
                yp_call_stars(yp_func_reversed, yp_tuple_empty, kwargs_sequence), yp_TypeError);
        assert_raises(yp_call_stars(yp_func_reversed, yp_tuple_empty, kwargs_rand), yp_TypeError);

        yp_decrefN(N(kwargs_rand, kwargs_sequence, list_empty, x));
    }

    yp_decrefN(N(str_rand, str_sequence));
    return MUNIT_OK;
}

// yp_sorted/etc is tested more thoroughly in test_tuple.
static void _test_sorted(fixture_type_t *type, ypObject *(*any_sorted)(ypObject *), int reversed)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_ordered_items);

    // Basic sorted.
    {
        ypObject *x = type->newN(N(items[1], items[0]));
        ypObject *sorted = any_sorted(x);
        assert_type_is(sorted, yp_t_list);
        if (reversed) {
            assert_sequence(sorted, items[1], items[0]);
        } else {
            assert_sequence(sorted, items[0], items[1]);
        }
        yp_decrefN(N(sorted, x));
    }

    // Items already sorted.
    {
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *sorted = any_sorted(x);
        assert_type_is(sorted, yp_t_list);
        if (reversed) {
            assert_sequence(sorted, items[1], items[0]);
        } else {
            assert_sequence(sorted, items[0], items[1]);
        }
        yp_decrefN(N(sorted, x));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        ypObject *sorted = any_sorted(x);
        assert_type_is(sorted, yp_t_list);
        assert_len(sorted, 0);
        yp_decrefN(N(sorted, x));
    }

    // x is not an iterable.
    assert_raises(any_sorted(not_iterable), yp_TypeError);

    // Exception passthrough.
    assert_raises(any_sorted(yp_SyntaxError), yp_SyntaxError);
    assert_raises(any_sorted(yp_StopIteration), yp_StopIteration);
    assert_raises(any_sorted(yp_GeneratorExit), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

// yp_sorted3/etc is tested more thoroughly in test_tuple.
static void _test_sorted3(
        fixture_type_t *type, ypObject *(*any_sorted3)(ypObject *, ypObject *, ypObject *))
{
    ypObject *s_x = yp_str_frombytesC2(-1, "x");
    define_function(key_first_arg, first_arg_code, ({s_x}));
    define_function(key_zero, zero_code, ({s_x}));
    ypObject     *keys_noop[] = {yp_None, key_first_arg, NULL};           // borrowed
    ypObject     *keys_all[] = {yp_None, key_first_arg, key_zero, NULL};  // borrowed
    ypObject    **key;                                                    // borrowed
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_ordered_items);

    // Basic sorted3.
    for (key = keys_noop; *key != NULL; key++) {
        ypObject *x = type->newN(N(items[1], items[0]));
        ypObject *sorted = any_sorted3(x, *key, yp_False);
        assert_type_is(sorted, yp_t_list);
        assert_sequence(sorted, items[0], items[1]);
        yp_decrefN(N(sorted, x));
    }

    // Reversed.
    for (key = keys_noop; *key != NULL; key++) {
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *sorted = any_sorted3(x, *key, yp_True);
        assert_type_is(sorted, yp_t_list);
        assert_sequence(sorted, items[1], items[0]);
        yp_decrefN(N(sorted, x));
    }

    // key_zero: items do not move as their keys are all equal.
    {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newN(N(items[1], items[0]));
        ypObject *sorted = any_sorted3(x, key_zero, yp_False);
        assert_type_is(sorted, yp_t_list);
        yp_unpackN(sorted, N(&first, &second));

        if (yp_eq(first, items[1]) == yp_True) {
            assert_obj(second, eq, items[0]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, eq, items[0]);
            assert_obj(second, eq, items[1]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(sorted, x, second, first));
    }

    // Items already sorted.
    for (key = keys_noop; *key != NULL; key++) {
        ypObject *x = type->newN(N(items[0], items[1]));
        ypObject *sorted = any_sorted3(x, *key, yp_False);
        assert_type_is(sorted, yp_t_list);
        assert_sequence(sorted, items[0], items[1]);
        yp_decrefN(N(sorted, x));
    }

    // Items already reversed.
    for (key = keys_noop; *key != NULL; key++) {
        ypObject *x = type->newN(N(items[1], items[0]));
        ypObject *sorted = any_sorted3(x, *key, yp_True);
        assert_type_is(sorted, yp_t_list);
        assert_sequence(sorted, items[1], items[0]);
        yp_decrefN(N(sorted, x));
    }

    // x is empty.
    for (key = keys_all; *key != NULL; key++) {
        ypObject *x = type->newN(0);
        ypObject *sorted = any_sorted3(x, *key, yp_False);
        assert_type_is(sorted, yp_t_list);
        assert_len(sorted, 0);
        yp_decrefN(N(sorted, x));
    }

    // x is not an iterable.
    assert_raises(any_sorted3(not_iterable, yp_None, yp_False), yp_TypeError);
    assert_raises(any_sorted3(not_iterable, key_zero, yp_True), yp_TypeError);

    // Exception passthrough.
    {
        ypObject *list_empty = yp_listN(0);

        assert_raises(any_sorted3(yp_SyntaxError, yp_None, yp_False), yp_SyntaxError);
        assert_raises(any_sorted3(yp_StopIteration, yp_None, yp_False), yp_StopIteration);
        assert_raises(any_sorted3(yp_GeneratorExit, yp_None, yp_False), yp_GeneratorExit);
        assert_raises(any_sorted3(list_empty, yp_SyntaxError, yp_False), yp_SyntaxError);
        assert_raises(any_sorted3(list_empty, yp_None, yp_SyntaxError), yp_SyntaxError);

        yp_decrefN(N(list_empty));
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable, key_zero, key_first_arg, s_x));
    uniqueness_dealloc(uq);
}

static ypObject *sorted_to_sorted3(ypObject *iterable)
{
    return yp_sorted3(iterable, yp_None, yp_False);
}

static ypObject *sorted_to_sorted3_reverse(ypObject *iterable)
{
    return yp_sorted3(iterable, yp_None, yp_True);
}

static MunitResult test_sorted(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Shared tests.
    _test_sorted(type, yp_sorted, /*reversed=*/FALSE);
    _test_sorted(type, sorted_to_sorted3, /*reversed=*/FALSE);
    _test_sorted(type, sorted_to_sorted3_reverse, /*reversed=*/TRUE);
    _test_sorted3(type, yp_sorted3);

    return MUNIT_OK;
}

// Tests the default arguments for yp_func_sorted.
static ypObject *sorted_to_call_func_sorted(ypObject *obj)
{
    return yp_callN(yp_func_sorted, N(obj));
}

static ypObject *sorted3_to_call_func_sorted3(ypObject *obj, ypObject *key, ypObject *reverse)
{
    ypObject *str_key = yp_str_frombytesC2(-1, "key");
    ypObject *str_reverse = yp_str_frombytesC2(-1, "reverse");
    ypObject *args = yp_tupleN(N(obj));
    ypObject *kwargs = yp_frozendictK(K(str_key, key, str_reverse, reverse));
    ypObject *result = yp_call_stars(yp_func_sorted, args, kwargs);
    yp_decrefN(N(kwargs, args, str_reverse, str_key));
    return result;
}

static ypObject *sorted_to_call_func_sorted3(ypObject *obj)
{
    return sorted3_to_call_func_sorted3(obj, yp_None, yp_False);
}

static ypObject *sorted_to_call_func_sorted3_reverse(ypObject *obj)
{
    return sorted3_to_call_func_sorted3(obj, yp_None, yp_True);
}

static MunitResult test_func_sorted(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *str_iterable = yp_str_frombytesC2(-1, "iterable");
    ypObject       *str_rand = rand_obj(NULL, fixture_type_str);

    // Shared tests.
    _test_sorted(type, sorted_to_call_func_sorted, /*reversed=*/FALSE);
    _test_sorted(type, sorted_to_call_func_sorted3, /*reversed=*/FALSE);
    _test_sorted(type, sorted_to_call_func_sorted3_reverse, /*reversed=*/TRUE);
    _test_sorted3(type, sorted3_to_call_func_sorted3);

    // Invalid arguments.
    {
        ypObject *x = rand_obj(NULL, type);
        ypObject *list_empty = yp_listN(0);
        ypObject *kwargs_iterable = yp_frozendictK(K(str_iterable, x));
        ypObject *kwargs_rand = yp_frozendictK(K(str_rand, x));

        assert_raises(yp_callN(yp_func_sorted, 0), yp_TypeError);
        assert_raises(yp_callN(yp_func_sorted, N(x, yp_None)), yp_TypeError);
        assert_raises(yp_callN(yp_func_sorted, N(x, yp_None, yp_False)), yp_TypeError);
        assert_raises(yp_call_stars(yp_func_sorted, list_empty, kwargs_iterable), yp_TypeError);
        assert_raises(yp_call_stars(yp_func_sorted, list_empty, kwargs_rand), yp_TypeError);

        yp_decrefN(N(kwargs_rand, kwargs_iterable, list_empty, x));
    }

    yp_decrefN(N(str_rand, str_iterable));
    return MUNIT_OK;
}

static void _test_zipN(fixture_type_t *type, ypObject *(*any_zipN)(int, ...))
{
    // yp_zipN is not yet implemented.
    ypObject *x = rand_obj(NULL, type);
    assert_raises(any_zipN(N(x)), yp_NotImplementedError);
    yp_decrefN(N(x));
}

static ypObject *zipN_to_zipNV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_zipNV(n, args);
    va_end(args);
    return result;
}

static MunitResult test_zipN(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    _test_zipN(type, yp_zipN);
    _test_zipN(type, zipN_to_zipNV);

    return MUNIT_OK;
}

// x_two contains items[0] and items[1], any_iter creates the iterator, send_value is the value to
// use in yp_send (which the iterator ignores), and items are the objects yielded.
static void _test_send(fixture_type_t *type, ypObject *x_two, ypObject *(*any_iter)(ypObject *),
        ypObject *send_value, ypObject **items, int test_original_object_return)
{
    // Basic send.
    {
        ypObject *first;
        ypObject *second;
        ypObject *iter = any_iter(x_two);

        assert_not_raises(first = yp_send(iter, send_value));
        assert_not_raises(second = yp_send(iter, send_value));
        assert_raises(yp_send(iter, send_value), yp_StopIteration);

        if (yp_eq(first, items[0]) == yp_True) {
            assert_obj(second, eq, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, eq, items[1]);
            assert_obj(second, eq, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        ypObject *iter = any_iter(x);
        assert_raises(yp_send(iter, send_value), yp_StopIteration);
        yp_decrefN(N(iter, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (test_original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *iter = any_iter(x_two);

        assert_not_raises(first = yp_send(iter, send_value));
        assert_not_raises(second = yp_send(iter, send_value));
        assert_raises(yp_send(iter, send_value), yp_StopIteration);

        if (first == items[0]) {
            assert_obj(second, is, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, is, items[1]);
            assert_obj(second, is, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter));
    }

    // Exception passthrough.
    assert_raises(yp_send(yp_SyntaxError, yp_None), yp_SyntaxError);
    assert_raises(yp_send(yp_StopIteration, yp_None), yp_StopIteration);
    assert_raises(yp_send(yp_GeneratorExit, yp_None), yp_GeneratorExit);
    ead(iter, any_iter(x_two), assert_raises(yp_send(iter, yp_SyntaxError), yp_SyntaxError));
}

static MunitResult test_send(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject       *send_value = rand_obj_any(uq);
    ypObject       *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // yp_send with yp_iter. x is reused so can't be an iter.
    if (type != fixture_type_iter) {
        ypObject *x = type->newN(N(items[0], items[1]));
        _test_send(type, x, yp_iter, send_value, items, type->original_object_return);
        yp_decrefN(N(x));
    }

    // yp_send with yp_iter_keys, yp_iter_values, and yp_iter_items.
    if (type->is_mapping) {
        ypObject *values[2];
        ypObject *pairs[2];  // The key/value pairs.
        ypObject *x;
        obj_array_fill(values, uq, type->rand_values);
        assert_not_raises(pairs[0] = yp_tupleN(2, items[0], values[0]));
        assert_not_raises(pairs[1] = yp_tupleN(2, items[1], values[1]));
        x = type->newK(K(items[0], values[0], items[1], values[1]));

        _test_send(type, x, yp_iter_keys, send_value, items, type->original_object_return);
        _test_send(type, x, yp_iter_values, send_value, values, type->original_object_return);
        _test_send(
                type, x, yp_iter_items, send_value, pairs, /*test_original_object_return=*/FALSE);

        yp_decrefN(N(x));
        obj_array_decref(pairs);
        obj_array_decref(values);
    }

    // x is not an iterable.
    assert_raises(yp_send(not_iterable, send_value), yp_TypeError);

    // Exception passthrough.
    assert_raises(yp_send(yp_SyntaxError, send_value), yp_SyntaxError);
    assert_raises(yp_send(yp_StopIteration, send_value), yp_StopIteration);
    assert_raises(yp_send(yp_GeneratorExit, send_value), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(send_value, not_iterable));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// There is no test_next: yp_next is tested throughout this file.

// x_two contains items[0] and items[1], any_iter creates the iterator, default_ is the value to
// use in yp_next2 (which the iterator ignores), and items are the objects yielded.
static void _test_next2(fixture_type_t *type, ypObject *x_two, ypObject *(*any_iter)(ypObject *),
        ypObject *default_, ypObject **items, int test_original_object_return)
{
    // Basic next2.
    {
        ypObject *first;
        ypObject *second;
        ypObject *iter = any_iter(x_two);

        assert_not_raises(first = yp_next2(iter, default_));
        assert_not_raises(second = yp_next2(iter, default_));
        ead(third, yp_next2(iter, default_), assert_obj(third, is, default_));

        if (yp_eq(first, items[0]) == yp_True) {
            assert_obj(second, eq, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, eq, items[1]);
            assert_obj(second, eq, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        ypObject *iter = any_iter(x);
        ead(first, yp_next2(iter, default_), assert_obj(first, is, default_));
        yp_decrefN(N(iter, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (test_original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *iter = any_iter(x_two);

        assert_not_raises(first = yp_next2(iter, default_));
        assert_not_raises(second = yp_next2(iter, default_));
        ead(third, yp_next2(iter, default_), assert_obj(third, is, default_));

        if (first == items[0]) {
            assert_obj(second, is, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, is, items[1]);
            assert_obj(second, is, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter));
    }

    // Exception passthrough.
    assert_raises(yp_next2(yp_SyntaxError, yp_None), yp_SyntaxError);
    assert_raises(yp_next2(yp_StopIteration, yp_None), yp_StopIteration);
    assert_raises(yp_next2(yp_GeneratorExit, yp_None), yp_GeneratorExit);
    ead(iter, any_iter(x_two), assert_raises(yp_next2(iter, yp_SyntaxError), yp_SyntaxError));
}

static MunitResult test_next2(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject       *default_ = rand_obj_any(uq);
    ypObject       *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // yp_next2 with yp_iter. x is reused so can't be an iter.
    if (type != fixture_type_iter) {
        ypObject *x = type->newN(N(items[0], items[1]));
        _test_next2(type, x, yp_iter, default_, items, type->original_object_return);
        yp_decrefN(N(x));
    }

    // yp_next2 with yp_iter_keys, yp_iter_values, and yp_iter_items.
    if (type->is_mapping) {
        ypObject *values[2];
        ypObject *pairs[2];  // The key/value pairs.
        ypObject *x;
        obj_array_fill(values, uq, type->rand_values);
        assert_not_raises(pairs[0] = yp_tupleN(2, items[0], values[0]));
        assert_not_raises(pairs[1] = yp_tupleN(2, items[1], values[1]));
        x = type->newK(K(items[0], values[0], items[1], values[1]));

        _test_next2(type, x, yp_iter_keys, default_, items, type->original_object_return);
        _test_next2(type, x, yp_iter_values, default_, values, type->original_object_return);
        _test_next2(type, x, yp_iter_items, default_, pairs, /*test_original_object_return=*/FALSE);

        yp_decrefN(N(x));
        obj_array_decref(pairs);
        obj_array_decref(values);
    }

    // x is not an iterable.
    assert_raises(yp_next2(not_iterable, default_), yp_TypeError);

    // Exception passthrough.
    assert_raises(yp_next2(yp_SyntaxError, default_), yp_SyntaxError);
    assert_raises(yp_next2(yp_StopIteration, default_), yp_StopIteration);
    assert_raises(yp_next2(yp_GeneratorExit, default_), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(default_, not_iterable));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_throw(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *syntax_stopIter_genExit[] = {
            yp_SyntaxError, yp_StopIteration, yp_GeneratorExit, NULL};
    ypObject      **exception;
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject       *not_exception = rand_obj_any(uq);
    ypObject       *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // yp_throw with yp_iter. x is reused so can't be an iter.
    if (type != fixture_type_iter) {
        ypObject *x = type->newN(N(items[0], items[1]));

        for (exception = syntax_stopIter_genExit; *exception != NULL; exception++) {
            ead(iter, yp_iter(x), assert_raises(yp_throw(iter, *exception), *exception));
        }

        ead(iter, yp_iter(x), assert_raises(yp_throw(iter, not_exception), yp_TypeError));

        yp_decrefN(N(x));
    }

    // yp_throw with yp_iter_keys, yp_iter_values, and yp_iter_items.
    if (type->is_mapping) {
        ypObject *values[2];
        ypObject *x;
        obj_array_fill(values, uq, type->rand_values);
        x = type->newK(K(items[0], values[0], items[1], values[1]));

        for (exception = syntax_stopIter_genExit; *exception != NULL; exception++) {
            ead(iter, yp_iter_keys(x), assert_raises(yp_throw(iter, *exception), *exception));
            ead(iter, yp_iter_values(x), assert_raises(yp_throw(iter, *exception), *exception));
            ead(iter, yp_iter_items(x), assert_raises(yp_throw(iter, *exception), *exception));
        }

        ead(iter, yp_iter_keys(x), assert_raises(yp_throw(iter, not_exception), yp_TypeError));
        ead(iter, yp_iter_values(x), assert_raises(yp_throw(iter, not_exception), yp_TypeError));
        ead(iter, yp_iter_items(x), assert_raises(yp_throw(iter, not_exception), yp_TypeError));

        yp_decrefN(N(x));
        obj_array_decref(values);
    }

    // x is not an iterable.
    assert_raises(yp_throw(not_iterable, yp_SyntaxError), yp_TypeError);

    // Exception passthrough.
    assert_raises(yp_throw(yp_SyntaxError, yp_Exception), yp_SyntaxError);
    assert_raises(yp_throw(yp_StopIteration, yp_Exception), yp_StopIteration);
    assert_raises(yp_throw(yp_GeneratorExit, yp_Exception), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(not_exception, not_iterable));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// x_three contains three items, and any_iter creates the iterator.
static void _test_close(fixture_type_t *type, ypObject *x_three, ypObject *(*any_iter)(ypObject *))
{
    // Basic close.
    {
        ypObject *iter = any_iter(x_three);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 3);
        ead(first, yp_next(iter), assert_not_exception(first));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 2);
        assert_not_raises_exc(yp_close(iter, &exc));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    // close twice.
    {
        ypObject *iter = any_iter(x_three);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 3);
        assert_not_raises_exc(yp_close(iter, &exc));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_not_raises_exc(yp_close(iter, &exc));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter));
    }

    // x is empty.
    {
        ypObject *x = type->newN(0);
        ypObject *iter = any_iter(x);
        assert_not_raises_exc(yp_close(iter, &exc));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter, x));
    }
}

static MunitResult test_close(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject       *items[3];
    obj_array_fill(items, uq, type->rand_items);

    // yp_close with yp_iter. x is reused so can't be an iter.
    if (type != fixture_type_iter) {
        ypObject *x = type->newN(N(items[0], items[1], items[2]));
        _test_close(type, x, yp_iter);
        yp_decrefN(N(x));
    }

    // yp_close with yp_iter_keys, yp_iter_values, and yp_iter_items.
    if (type->is_mapping) {
        ypObject *values[3];
        ypObject *x;
        obj_array_fill(values, uq, type->rand_values);
        x = type->newK(K(items[0], values[0], items[1], values[1], items[2], values[2]));

        _test_close(type, x, yp_iter_keys);
        _test_close(type, x, yp_iter_values);
        _test_close(type, x, yp_iter_items);

        yp_decrefN(N(x));
        obj_array_decref(values);
    }

    // x is not an iterable.
    assert_raises_exc(yp_close(not_iterable, &exc), yp_MethodError);

    // Exception passthrough.
    assert_raises_exc(yp_close(yp_SyntaxError, &exc), yp_SyntaxError);
    assert_raises_exc(yp_close(yp_StopIteration, &exc), yp_StopIteration);
    assert_raises_exc(yp_close(yp_GeneratorExit, &exc), yp_GeneratorExit);

    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_iter_keys(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *not_iterable = rand_obj_any_not_iterable(NULL);

    // iter_keys is only supported by mappings.
    if (!type->is_mapping) {
        ypObject *x = rand_obj(NULL, type);
        assert_raises(yp_iter_keys(x), yp_MethodError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    // Shared tests.
    _test_iter(type, yp_iter_keys);

    // x is not an iterable.
    assert_raises(yp_iter_keys(not_iterable), yp_MethodError);

tear_down:
    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static void _test_iter_values(fixture_type_t *type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *keys[2];
    ypObject     *values[2];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

    // Basic iter_values.
    {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *iter = yp_iter_values(x);
        assert_type_is(iter, yp_t_iter);

        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 2);
        assert_not_raises(first = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 1);
        assert_not_raises(second = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);

        if (yp_eq(first, values[0]) == yp_True) {  // GCOVR_EXCL_START
            assert_obj(second, eq, values[1]);
        } else {
            assert_obj(first, eq, values[1]);
            assert_obj(second, eq, values[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter, x));
    }

    // x is empty.
    {
        ypObject *x = type->newK(0);
        ypObject *iter = yp_iter_values(x);
        assert_type_is(iter, yp_t_iter);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *iter = yp_iter_values(x);

        assert_not_raises(first = yp_next(iter));
        assert_not_raises(second = yp_next(iter));
        assert_raises(yp_next(iter), yp_StopIteration);

        if (first == values[0]) {  // GCOVR_EXCL_START
            assert_obj(second, is, values[1]);
        } else {
            assert_obj(first, is, values[1]);
            assert_obj(second, is, values[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter, x));
    }

    // x is not an iterable.
    assert_raises(yp_iter_values(not_iterable), yp_MethodError);

    // Exception passthrough.
    assert_raises(yp_iter_values(yp_SyntaxError), yp_SyntaxError);
    assert_raises(yp_iter_values(yp_StopIteration), yp_StopIteration);
    assert_raises(yp_iter_values(yp_GeneratorExit), yp_GeneratorExit);

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_iter_values(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // iter_values is only supported by mappings.
    if (!type->is_mapping) {
        ypObject *x = rand_obj(NULL, type);
        assert_raises(yp_iter_values(x), yp_MethodError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    _test_iter_values(type);

tear_down:
    return MUNIT_OK;
}

static void _test_iter_items(fixture_type_t *type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *keys[2];
    ypObject     *values[2];
    ypObject     *pairs[2];  // The key/value pairs.
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);
    assert_not_raises(pairs[0] = yp_tupleN(2, keys[0], values[0]));
    assert_not_raises(pairs[1] = yp_tupleN(2, keys[1], values[1]));

    // Basic iter_items.
    {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *iter = yp_iter_items(x);
        assert_type_is(iter, yp_t_iter);

        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 2);
        assert_not_raises(first = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 1);
        assert_not_raises(second = yp_next(iter));
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);

        assert_type_is(first, yp_t_tuple);
        assert_type_is(second, yp_t_tuple);
        if (yp_eq(first, pairs[0]) == yp_True) {  // GCOVR_EXCL_START
            assert_obj(second, eq, pairs[1]);
        } else {
            assert_obj(first, eq, pairs[1]);
            assert_obj(second, eq, pairs[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter, x));
    }

    // x is empty.
    {
        ypObject *x = type->newK(0);
        ypObject *iter = yp_iter_items(x);
        assert_type_is(iter, yp_t_iter);
        assert_ssizeC_exc(yp_length_hintC(iter, &exc), ==, 0);
        assert_raises(yp_next(iter), yp_StopIteration);
        yp_decrefN(N(iter, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        ypObject *first;
        ypObject *second;
        ypObject *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *iter = yp_iter_items(x);

        assert_not_raises(first = yp_next(iter));
        assert_not_raises(second = yp_next(iter));
        assert_raises(yp_next(iter), yp_StopIteration);

        if (yp_eq(first, pairs[0]) == yp_True) {  // GCOVR_EXCL_START
            ead(item, yp_getindexC(first, 0), assert_obj(item, is, keys[0]));
            ead(item, yp_getindexC(first, 1), assert_obj(item, is, values[0]));
            ead(item, yp_getindexC(second, 0), assert_obj(item, is, keys[1]));
            ead(item, yp_getindexC(second, 1), assert_obj(item, is, values[1]));
        } else {
            ead(item, yp_getindexC(first, 0), assert_obj(item, is, keys[1]));
            ead(item, yp_getindexC(first, 1), assert_obj(item, is, values[1]));
            ead(item, yp_getindexC(second, 0), assert_obj(item, is, keys[0]));
            ead(item, yp_getindexC(second, 1), assert_obj(item, is, values[0]));
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, iter, x));
    }

    // x is not an iterable.
    assert_raises(yp_iter_items(not_iterable), yp_MethodError);

    // Exception passthrough.
    assert_raises(yp_iter_items(yp_SyntaxError), yp_SyntaxError);
    assert_raises(yp_iter_items(yp_StopIteration), yp_StopIteration);
    assert_raises(yp_iter_items(yp_GeneratorExit), yp_GeneratorExit);

    obj_array_decref(pairs);
    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_iter_items(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // iter_items is only supported by mappings.
    if (!type->is_mapping) {
        ypObject *x = rand_obj(NULL, type);
        assert_raises(yp_iter_items(x), yp_MethodError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    _test_iter_items(type);

tear_down:
    return MUNIT_OK;
}

static ypObject *iter_to_call_t_iter(ypObject *x) { return yp_callN(yp_t_iter, 1, x); }

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *not_iterable = rand_obj_any_not_iterable(NULL);

    _test_iter(fixture->type, iter_to_call_t_iter);

    // x is not an iterable.
    assert_raises(iter_to_call_t_iter(not_iterable), yp_TypeError);

    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static void _test_miniiter(
        fixture_type_t *type, ypObject *(*any_miniiter)(ypObject *, yp_uint64_t *))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[2];
    obj_array_fill(items, uq, type->rand_items);

    // Basic miniiter.
    {
        yp_uint64_t mi_state;
        ypObject   *first;
        ypObject   *second;
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = any_miniiter(x, &mi_state);

        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 2);
        assert_not_raises(first = yp_miniiter_next(mi, &mi_state));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 1);
        assert_not_raises(second = yp_miniiter_next(mi, &mi_state));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);

        if (yp_eq(first, items[0]) == yp_True) {
            assert_obj(second, eq, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, eq, items[1]);
            assert_obj(second, eq, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, mi, x));
    }

    // x is empty.
    {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(0);
        ypObject   *mi = any_miniiter(x, &mi_state);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        yp_decrefN(N(mi, x));
    }

    // Optimization: lazy shallow copy of a fellow iterator.
    if (type == fixture_type_iter) {
        yp_uint64_t mi_state;
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = any_miniiter(x, &mi_state);
        assert_obj(mi, is, x);
        yp_decrefN(N(mi, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        yp_uint64_t mi_state;
        ypObject   *first;
        ypObject   *second;
        ypObject   *x = type->newN(N(items[0], items[1]));
        ypObject   *mi = any_miniiter(x, &mi_state);

        assert_not_raises(first = yp_miniiter_next(mi, &mi_state));
        assert_not_raises(second = yp_miniiter_next(mi, &mi_state));
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);

        if (first == items[0]) {
            assert_obj(second, is, items[1]);
        } else {  // GCOVR_EXCL_START
            // Only set-likes and mappings are allowed to iterate out of order.
            assert_true(type->is_setlike || type->is_mapping);
            assert_obj(first, is, items[1]);
            assert_obj(second, is, items[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, mi, x));
    }

    // x is not an iterable. yp_miniiter is yp_TypeError, yp_miniiter_keys is yp_MethodError.
    {
        yp_uint64_t mi_state;
        assert_raises(any_miniiter(not_iterable, &mi_state), yp_TypeError, yp_MethodError);
    }

    // Exception passthrough.
    {
        yp_uint64_t mi_state;
        assert_raises(any_miniiter(yp_SyntaxError, &mi_state), yp_SyntaxError);
        assert_raises(any_miniiter(yp_StopIteration, &mi_state), yp_StopIteration);
        assert_raises(any_miniiter(yp_GeneratorExit, &mi_state), yp_GeneratorExit);
    }

    obj_array_decref(items);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *not_iterable = rand_obj_any_not_iterable(NULL);

    _test_miniiter(fixture->type, yp_miniiter);

    // x is not an iterable.
    {
        yp_uint64_t mi_state;
        assert_raises(yp_miniiter(not_iterable, &mi_state), yp_TypeError);
    }

    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static MunitResult test_miniiter_keys(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *not_iterable = rand_obj_any_not_iterable(NULL);

    // miniiter_keys is only supported by mappings.
    if (!type->is_mapping) {
        yp_uint64_t mi_state;
        ypObject   *x = rand_obj(NULL, type);
        assert_raises(yp_miniiter_keys(x, &mi_state), yp_MethodError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    _test_miniiter(type, yp_miniiter_keys);

    // x is not an iterable.
    {
        yp_uint64_t mi_state;
        assert_raises(yp_miniiter_keys(not_iterable, &mi_state), yp_MethodError);
    }

tear_down:
    yp_decrefN(N(not_iterable));
    return MUNIT_OK;
}

static void _test_miniiter_values(fixture_type_t *type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *keys[2];
    ypObject     *values[2];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

    // Basic miniiter.
    {
        yp_uint64_t mi_state;
        ypObject   *first;
        ypObject   *second;
        ypObject   *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject   *mi = yp_miniiter_values(x, &mi_state);

        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 2);
        assert_not_raises(first = yp_miniiter_next(mi, &mi_state));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 1);
        assert_not_raises(second = yp_miniiter_next(mi, &mi_state));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);

        if (yp_eq(first, values[0]) == yp_True) {  // GCOVR_EXCL_START
            assert_obj(second, eq, values[1]);
        } else {
            assert_obj(first, eq, values[1]);
            assert_obj(second, eq, values[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, mi, x));
    }

    // x is empty.
    {
        yp_uint64_t mi_state;
        ypObject   *x = type->newK(0);
        ypObject   *mi = yp_miniiter_values(x, &mi_state);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        yp_decrefN(N(mi, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        yp_uint64_t mi_state;
        ypObject   *first;
        ypObject   *second;
        ypObject   *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject   *mi = yp_miniiter_values(x, &mi_state);

        assert_not_raises(first = yp_miniiter_next(mi, &mi_state));
        assert_not_raises(second = yp_miniiter_next(mi, &mi_state));
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);

        if (first == values[0]) {  // GCOVR_EXCL_START
            assert_obj(second, is, values[1]);
        } else {
            assert_obj(first, is, values[1]);
            assert_obj(second, is, values[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first, mi, x));
    }

    // x is not an iterable.
    {
        yp_uint64_t mi_state;
        assert_raises(yp_miniiter_values(not_iterable, &mi_state), yp_MethodError);
    }

    // Exception passthrough.
    {
        yp_uint64_t mi_state;
        assert_raises(yp_miniiter_values(yp_SyntaxError, &mi_state), yp_SyntaxError);
        assert_raises(yp_miniiter_values(yp_StopIteration, &mi_state), yp_StopIteration);
        assert_raises(yp_miniiter_values(yp_GeneratorExit, &mi_state), yp_GeneratorExit);
    }

    obj_array_decref(keys);
    obj_array_decref(values);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_miniiter_values(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // miniiter_values is only supported by mappings.
    if (!type->is_mapping) {
        yp_uint64_t mi_state;
        ypObject   *x = rand_obj(NULL, type);
        assert_raises(yp_miniiter_values(x, &mi_state), yp_MethodError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    _test_miniiter_values(type);

tear_down:
    return MUNIT_OK;
}

static void _test_miniiter_items(fixture_type_t *type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *keys[2];
    ypObject     *values[2];
    ypObject     *pairs[2];  // The key/value pairs.
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);
    assert_not_raises(pairs[0] = yp_tupleN(2, keys[0], values[0]));
    assert_not_raises(pairs[1] = yp_tupleN(2, keys[1], values[1]));

    // Basic miniiter.
    {
        yp_uint64_t mi_state;
        ypObject   *first_key;
        ypObject   *first_value;
        ypObject   *second;
        ypObject   *si_value;
        ypObject   *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject   *mi = yp_miniiter_items(x, &mi_state);

        // yp_miniiter_items_next and yp_miniiter_next can be used interchangeably.
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 2);
        yp_miniiter_items_next(mi, &mi_state, &first_key, &first_value);
        assert_not_exception(first_key);
        assert_not_exception(first_value);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 1);
        assert_not_raises(second = yp_miniiter_next(mi, &mi_state));
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        assert_raises_exc(yp_miniiter_items_next(mi, &mi_state, &exc, &si_value), yp_StopIteration);
        assert_isexception(si_value, yp_StopIteration);
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);

        if (yp_eq(first_key, keys[0]) == yp_True) {  // GCOVR_EXCL_START
            assert_obj(first_value, eq, values[0]);
            assert_obj(second, eq, pairs[1]);
        } else {
            assert_obj(first_key, eq, keys[1]);
            assert_obj(first_value, eq, values[1]);
            assert_obj(second, eq, pairs[0]);
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first_value, first_key, mi, x));
    }

    // x is empty.
    {
        yp_uint64_t mi_state;
        ypObject   *si_value;
        ypObject   *x = type->newK(0);
        ypObject   *mi = yp_miniiter_items(x, &mi_state);
        assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
        assert_raises_exc(yp_miniiter_items_next(mi, &mi_state, &exc, &si_value), yp_StopIteration);
        assert_isexception(si_value, yp_StopIteration);
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        yp_decrefN(N(mi, x));
    }

    // Some types store references to the given objects and, thus, return exactly those objects.
    if (type->original_object_return) {
        yp_uint64_t mi_state;
        ypObject   *first_key;
        ypObject   *first_value;
        ypObject   *second;
        ypObject   *x = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject   *mi = yp_miniiter_items(x, &mi_state);

        // yp_miniiter_items_next and yp_miniiter_next can be used interchangeably.
        yp_miniiter_items_next(mi, &mi_state, &first_key, &first_value);
        assert_not_exception(first_key);
        assert_not_exception(first_value);
        assert_not_raises(second = yp_miniiter_next(mi, &mi_state));
        assert_raises_exc(yp_miniiter_items_next(mi, &mi_state, &exc, &exc), yp_StopIteration);

        if (first_key == keys[0]) {  // GCOVR_EXCL_START
            assert_obj(first_value, is, values[0]);
            ead(item, yp_getindexC(second, 0), assert_obj(item, is, keys[1]));
            ead(item, yp_getindexC(second, 1), assert_obj(item, is, values[1]));
        } else {
            assert_obj(first_key, is, keys[1]);
            assert_obj(first_value, is, values[1]);
            ead(item, yp_getindexC(second, 0), assert_obj(item, is, keys[0]));
            ead(item, yp_getindexC(second, 1), assert_obj(item, is, values[0]));
        }  // GCOVR_EXCL_STOP

        yp_decrefN(N(second, first_value, first_key, mi, x));
    }

    // x is not an iterable.
    {
        yp_uint64_t mi_state;
        assert_raises(yp_miniiter_items(not_iterable, &mi_state), yp_MethodError);
    }

    // Exception passthrough.
    {
        yp_uint64_t mi_state;
        assert_raises(yp_miniiter_items(yp_SyntaxError, &mi_state), yp_SyntaxError);
        assert_raises(yp_miniiter_items(yp_StopIteration, &mi_state), yp_StopIteration);
        assert_raises(yp_miniiter_items(yp_GeneratorExit, &mi_state), yp_GeneratorExit);
    }

    obj_array_decref(pairs);
    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(not_iterable));
    uniqueness_dealloc(uq);
}

static MunitResult test_miniiter_items(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // miniiter_items is only supported by mappings.
    if (!type->is_mapping) {
        yp_uint64_t mi_state;
        ypObject   *x = rand_obj(NULL, type);
        assert_raises(yp_miniiter_items(x, &mi_state), yp_MethodError);
        yp_decrefN(N(x));
        goto tear_down;  // Skip remaining tests.
    }

    _test_miniiter_items(type);

tear_down:
    return MUNIT_OK;
}


static MunitParameterEnum test_iterable_params[] = {
        {param_key_type, param_values_types_iterable}, {NULL}};

MunitTest test_iterable_tests[] = {TEST(test_iter, test_iterable_params),
        TEST(test_unpackN, test_iterable_params), TEST(test_filter, test_iterable_params),
        TEST(test_filterfalse, test_iterable_params), TEST(test_reversed, test_iterable_params),
        TEST(test_func_reversed, test_iterable_params), TEST(test_sorted, test_iterable_params),
        TEST(test_func_sorted, test_iterable_params), TEST(test_zipN, test_iterable_params),
        TEST(test_send, test_iterable_params), TEST(test_next2, test_iterable_params),
        TEST(test_throw, test_iterable_params), TEST(test_close, test_iterable_params),
        TEST(test_iter_keys, test_iterable_params), TEST(test_iter_values, test_iterable_params),
        TEST(test_iter_items, test_iterable_params), TEST(test_call_type, test_iterable_params),
        TEST(test_miniiter, test_iterable_params), TEST(test_miniiter_keys, test_iterable_params),
        TEST(test_miniiter_values, test_iterable_params),
        TEST(test_miniiter_items, test_iterable_params), {NULL}};

extern void test_iterable_initialize(void) {}

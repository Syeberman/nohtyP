
#include "munit_test/unittest.h"


// FIXME "frozendict_dirty", "dict_dirty", shared key?
// FIXME fixture_type_frozenset, fixture_type_set, fixture_type_frozenset_dirty, and
// fixture_type_set_dirty are tricky because they require hashable types but in (key, value) the
// value may not be hashable.
#define x_types_init()                                                                     \
    {                                                                                      \
        fixture_type_frozendict, fixture_type_dict, fixture_type_iter, fixture_type_tuple, \
                fixture_type_list, NULL                                                    \
    }

#define friend_types_init()                              \
    {                                                    \
        fixture_type_frozendict, fixture_type_dict, NULL \
    }

// Returns true iff type can store unhashable objects.
static int type_stores_unhashables(fixture_type_t *type)
{
    return !type->is_setlike && !type->is_mapping;
}


static MunitResult _test_newK(
        fixture_type_t *type, ypObject *(*any_newK)(int, ...), int test_exception_passthrough)
{
    ypObject *int_1 = yp_intC(1);
    ypObject *intstore_1 = yp_intstoreC(1);
    ypObject *keys[4];
    ypObject *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Basic newK.
    {
        ypObject *mp = any_newK(K(keys[0], values[0], keys[1], values[1]));
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp));
    }

    // n is zero.
    {
        ypObject *mp = any_newK(0);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp));
    }

    // n is negative.
    {
        ypObject *mp = any_newK(-1);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp));
    }

    // Duplicate arguments; last value is retained.
    {
        ypObject *mp = any_newK(K(keys[0], values[0], keys[1], values[1], keys[1], values[2]));
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[2]);
        yp_decrefN(N(mp));
    }

    // Unhashable key.
    {
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_raises(any_newK(K(unhashable, values[0])), yp_TypeError);
        assert_raises(any_newK(K(keys[0], values[0], unhashable, values[1])), yp_TypeError);
        assert_raises(any_newK(K(keys[0], values[0], keys[1], values[1], unhashable, values[2])),
                yp_TypeError);
        yp_decrefN(N(unhashable));
    }

    // Unhashable key rejected even if equal to other hashable key.
    assert_raises(any_newK(K(int_1, values[0], intstore_1, values[1])), yp_TypeError);
    assert_raises(any_newK(K(intstore_1, values[0], int_1, values[1])), yp_TypeError);

    // Optimization: empty immortal when n is zero.
    if (type->falsy != NULL) {
        assert_obj(any_newK(0), is, type->falsy);
        assert_obj(any_newK(-1), is, type->falsy);
    }

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_newK(K(yp_SyntaxError, yp_None)), yp_SyntaxError);
        assert_isexception(any_newK(K(yp_None, yp_SyntaxError)), yp_SyntaxError);
        assert_isexception(
                any_newK(K(yp_i_one, yp_i_one, yp_SyntaxError, yp_None)), yp_SyntaxError);
        assert_isexception(
                any_newK(K(yp_i_one, yp_i_one, yp_None, yp_SyntaxError)), yp_SyntaxError);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(intstore_1, int_1));
    return MUNIT_OK;
}

static MunitResult _test_new(
        fixture_type_t *type, ypObject *(*any_new)(ypObject *), int test_exception_passthrough)
{
    fixture_type_t  *x_types[] = x_types_init();
    fixture_type_t  *friend_types[] = friend_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *intstore_1 = yp_intstoreC(1);
    ypObject        *keys[4];
    ypObject        *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Basic new.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = new_itemsK(*x_type, K(keys[0], values[0], keys[1], values[1]));
        ypObject *mp = any_new(x);
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[1]);
        yp_decrefN(N(mp, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(0);
        ypObject *mp = any_new(x);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp, x));
    }

    // x contains duplicates; last value is retained.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x =
                new_itemsK(*x_type, K(keys[0], values[0], keys[1], values[1], keys[0], values[2]));
        ypObject *mp = any_new(x);
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[2], keys[1], values[1]);
        yp_decrefN(N(mp, x));
    }

    // x contains an unhashable key.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
            ead(x, new_itemsK(*x_type, K(unhashable, values[0])),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x, new_itemsK(*x_type, K(keys[0], values[0], unhashable, values[1])),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x,
                    new_itemsK(*x_type,
                            K(keys[0], values[0], keys[1], values[1], unhashable, values[2])),
                    assert_raises(any_new(x), yp_TypeError));
            yp_decrefN(N(unhashable));
        }
    }

    // Unhashable key rejected even if equal to other hashable key.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ead(x, new_itemsK(*x_type, K(int_1, values[0], intstore_1, values[1])),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x, new_itemsK(*x_type, K(intstore_1, values[0], int_1, values[1])),
                    assert_raises(any_new(x), yp_TypeError));
        }
    }

    // Optimization: lazy shallow copy of a friendly immutable x to immutable mp.
    for (x_type = friend_types; (*x_type) != NULL; x_type++) {
        ypObject *x = new_itemsK(*x_type, K(keys[0], values[0], keys[1], values[1]));
        ypObject *mp = any_new(x);
        if (type->is_mutable || (*x_type)->is_mutable) {
            assert_obj(mp, is_not, x);
        } else {
            assert_obj(mp, is, x);
        }
        yp_decrefN(N(mp, x));
    }

    // Optimization: empty immortal when x is empty.
    if (type->falsy != NULL) {
        for (x_type = x_types; (*x_type) != NULL; x_type++) {
            ypObject *x = (*x_type)->newN(0);
            ypObject *mp = any_new(x);
            assert_obj(mp, is, type->falsy);
            yp_decrefN(N(mp, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject * mp, x,
            new_itemsK(fixture_type_tuple, K(keys[0], values[0], keys[1], values[1])),
            mp = any_new(x), assert_mapping(mp, keys[0], values[0], keys[1], values[1]),
            yp_decref(mp));

    // x is not an iterable.
    // FIXME Function to create a random "not iterable".
    assert_raises(any_new(int_1), yp_TypeError);

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_new(yp_SyntaxError), yp_SyntaxError);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(intstore_1, int_1));
    return MUNIT_OK;
}

static ypObject *newK_to_frozendictKV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_frozendictKV(n, args);
    va_end(args);
    return result;
}

static ypObject *newK_to_dictKV(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_dictKV(n, args);
    va_end(args);
    return result;
}

static MunitResult test_newK(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newK)(int, ...);
    ypObject *(*newK_to_newKV)(int, ...);
    MunitResult test_result;

    if (type->type == yp_t_frozendict) {
        newK = yp_frozendictK;
        newK_to_newKV = newK_to_frozendictKV;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        newK = yp_dictK;
        newK_to_newKV = newK_to_dictKV;
    }

    test_result = _test_newK(type, newK, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    test_result = _test_newK(type, newK_to_newKV, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    return MUNIT_OK;
}

static ypObject *newK_to_frozendict(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = new_itemsKV(fixture_type_list, n, args);  // new ref
    va_end(args);

    result = yp_frozendict(iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *newK_to_dict(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = new_itemsKV(fixture_type_list, n, args);  // new ref
    va_end(args);

    result = yp_dict(iterable);
    yp_decref(iterable);
    return result;
}

static MunitResult test_new(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*newK_to_new)(int, ...);
    ypObject *(*new_)(ypObject *);
    MunitResult test_result;

    if (type->type == yp_t_frozendict) {
        newK_to_new = newK_to_frozendict;
        new_ = yp_frozendict;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        newK_to_new = newK_to_dict;
        new_ = yp_dict;
    }

    test_result = _test_newK(type, newK_to_new, /*test_exception_passthrough=*/FALSE);
    if (test_result != MUNIT_OK) return test_result;

    test_result = _test_new(type, new_, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    return MUNIT_OK;
}

static ypObject *newK_to_call_args_t_frozendict(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = new_itemsKV(fixture_type_list, n, args));  // new ref
    va_end(args);

    result = yp_callN(yp_t_frozendict, 1, iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *new_to_call_args_t_frozendict(ypObject *iterable)
{
    return yp_callN(yp_t_frozendict, 1, iterable);
}

static ypObject *newK_to_call_args_t_dict(int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(iterable = new_itemsKV(fixture_type_list, n, args));  // new ref
    va_end(args);

    result = yp_callN(yp_t_dict, 1, iterable);
    yp_decref(iterable);
    return result;
}

static ypObject *new_to_call_args_t_dict(ypObject *iterable)
{
    return yp_callN(yp_t_dict, 1, iterable);
}

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init();
    fixture_type_t **x_type;
    ypObject *(*newK_to_call_args_type)(int, ...);
    ypObject *(*new_to_call_args_type)(ypObject *);
    MunitResult test_result;
    ypObject   *str_rand = rand_obj(fixture_type_str);
    ypObject   *str_cls = yp_str_frombytesC2(-1, "cls");
    ypObject   *str_object = yp_str_frombytesC2(-1, "object");
    ypObject   *keys[4];
    ypObject   *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    if (type->type == yp_t_frozendict) {
        newK_to_call_args_type = newK_to_call_args_t_frozendict;
        new_to_call_args_type = new_to_call_args_t_frozendict;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        newK_to_call_args_type = newK_to_call_args_t_dict;
        new_to_call_args_type = new_to_call_args_t_dict;
    }

    test_result = _test_newK(type, newK_to_call_args_type, /*test_exception_passthrough=*/FALSE);
    if (test_result != MUNIT_OK) goto tear_down;

    test_result = _test_new(type, new_to_call_args_type, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) goto tear_down;

    // Zero arguments.
    {
        ypObject *mp = yp_callN(type->type, 0);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp));
    }

    // Basic keyword arguments.
    {
        // cls and object are positional-only params, so cls and object are valid keyword args.
        ypObject *kwargs =
                yp_dictK(K(str_rand, values[0], str_cls, values[1], str_object, values[2]));
        ypObject *mp = yp_call_stars(type->type, yp_tuple_empty, kwargs);
        assert_type_is(mp, type->type);
        assert_mapping(mp, str_rand, values[0], str_cls, values[1], str_object, values[2]);
        assert_obj(mp, is_not, kwargs);
        assert_mapping(kwargs, str_rand, values[0], str_cls, values[1], str_object, values[2]);
        yp_decrefN(N(mp, kwargs));
    }

    // Keyword arguments and an empty positional argument.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *iterable = (*x_type)->newN(0);
        ypObject *args = yp_listN(N(iterable));
        ypObject *kwargs = yp_dictK(K(str_rand, values[0]));
        ypObject *mp = yp_call_stars(type->type, args, kwargs);
        assert_type_is(mp, type->type);
        assert_mapping(mp, str_rand, values[0]);
        assert_obj(mp, is_not, kwargs);
        assert_mapping(kwargs, str_rand, values[0]);
        yp_decrefN(N(mp, kwargs, args, iterable));
    }

    // The positional argument is merged with the keyword arguments.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *iterable = new_itemsK(*x_type, K(keys[0], values[0]));
        ypObject *args = yp_listN(N(iterable));
        ypObject *kwargs = yp_dictK(K(str_rand, values[1]));
        ypObject *mp = yp_call_stars(type->type, args, kwargs);
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], str_rand, values[1]);
        assert_obj(mp, is_not, kwargs);
        assert_mapping(kwargs, str_rand, values[1]);
        yp_decrefN(N(mp, kwargs, args, iterable));
    }

    // Values from keyword arguments replace values from the positional argument.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *iterable = new_itemsK(*x_type, K(str_rand, values[0]));
        ypObject *args = yp_listN(N(iterable));
        ypObject *kwargs = yp_dictK(K(str_rand, values[1]));
        ypObject *mp = yp_call_stars(type->type, args, kwargs);
        assert_type_is(mp, type->type);
        assert_mapping(mp, str_rand, values[1]);
        assert_obj(mp, is_not, kwargs);
        assert_mapping(kwargs, str_rand, values[1]);
        yp_decrefN(N(mp, kwargs, args, iterable));
    }

    // Bug: frozendict_func_new_code would (attempt to) modify yp_frozendict_empty when given an
    // empty positional argument and keyword arguments.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *iterable = (*x_type)->newN(0);
        ypObject *args = yp_listN(N(iterable));
        ypObject *kwargs = yp_dictK(K(str_rand, values[0]));
        ypObject *mp = yp_call_stars(type->type, args, kwargs);
        assert_len(yp_frozendict_empty, 0);
        yp_decrefN(N(mp, kwargs, args, iterable));
    }

    // Optimization: empty immortal with zero arguments.
    if (type->falsy != NULL) {
        assert_obj(yp_callN(type->type, 0), is, type->falsy);
    }

    // Optimization: lazy shallow copy of frozendict kwargs to frozendict.
    {
        ypObject *kwargs = yp_frozendictK(K(str_rand, values[0]));
        ypObject *mp = yp_call_stars(type->type, yp_tuple_empty, kwargs);
        if (type->is_mutable) {
            assert_obj(mp, is_not, kwargs);
        } else {
            assert_obj(mp, is, kwargs);
        }
        yp_decrefN(N(mp, kwargs));
    }

    // Invalid arguments.
    {
        ypObject *args_two = yp_tupleN(N(yp_frozendict_empty, yp_frozendict_empty));
        assert_raises(
                yp_callN(type->type, N(yp_frozendict_empty, yp_frozendict_empty)), yp_TypeError);
        assert_raises(yp_call_stars(type->type, args_two, yp_frozendict_empty), yp_TypeError);
        yp_decrefN(N(args_two));
    }

    // Exception passthrough.
    assert_isexception(yp_callN(type->type, N(yp_SyntaxError)), yp_SyntaxError);

tear_down:
    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(str_object, str_cls, str_rand));
    return MUNIT_OK;
}

static MunitResult _test_fromkeysN(fixture_type_t *type,
        ypObject *(*any_fromkeysN)(ypObject *, int, ...), int test_exception_passthrough)
{
    ypObject *int_1 = yp_intC(1);
    ypObject *intstore_1 = yp_intstoreC(1);
    ypObject *keys[4];
    ypObject *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Basic fromkeysN.
    {
        ypObject *mp = any_fromkeysN(values[0], N(keys[0], keys[1]));
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[0]);
        yp_decrefN(N(mp));
    }

    // n is zero.
    {
        ypObject *mp = any_fromkeysN(values[0], 0);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp));
    }

    // n is negative.
    {
        ypObject *mp = any_fromkeysN(values[0], -1);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp));
    }

    // FIXME large n......Impossible as you'd have to have that many arguments, so change the
    // code to limit it somehow. (test everywhere)

    // Duplicate keys.
    {
        ypObject *mp = any_fromkeysN(values[0], N(keys[0], keys[1], keys[1], keys[0]));
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[0]);
        yp_decrefN(N(mp));
    }

    // Unhashable key.
    {
        ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
        assert_raises(any_fromkeysN(values[0], N(unhashable)), yp_TypeError);
        assert_raises(any_fromkeysN(values[0], N(keys[0], unhashable)), yp_TypeError);
        assert_raises(any_fromkeysN(values[0], N(keys[0], keys[1], unhashable)), yp_TypeError);
        yp_decrefN(N(unhashable));
    }

    // Unhashable key rejected even if equal to other hashable key.
    assert_raises(any_fromkeysN(values[0], N(int_1, intstore_1)), yp_TypeError);
    assert_raises(any_fromkeysN(values[0], N(intstore_1, int_1)), yp_TypeError);

    // Optimization: empty immortal when n is zero.
    if (type->falsy != NULL) {
        assert_obj(any_fromkeysN(values[0], 0), is, type->falsy);
        assert_obj(any_fromkeysN(values[0], -1), is, type->falsy);
    }

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_fromkeysN(yp_SyntaxError, 0), yp_SyntaxError);
        assert_isexception(any_fromkeysN(yp_SyntaxError, N(yp_None)), yp_SyntaxError);
        assert_isexception(any_fromkeysN(yp_None, N(yp_SyntaxError)), yp_SyntaxError);
        assert_isexception(any_fromkeysN(yp_None, N(yp_None, yp_SyntaxError)), yp_SyntaxError);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(intstore_1, int_1));
    return MUNIT_OK;
}

static MunitResult _test_fromkeys(fixture_type_t *type,
        ypObject *(*any_fromkeys)(ypObject *, ypObject *), int test_exception_passthrough)
{
    fixture_type_t  *x_types[] = x_types_init();
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *intstore_1 = yp_intstoreC(1);
    ypObject        *keys[4];
    ypObject        *values[4];
    obj_array_fill(keys, type->rand_items);
    obj_array_fill(values, type->rand_values);

    // Basic fromkeys.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(N(keys[0], keys[1]));
        ypObject *mp = any_fromkeys(x, values[0]);
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[0]);
        yp_decrefN(N(mp, x));
    }

    // x is empty.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(0);
        ypObject *mp = any_fromkeys(x, values[0]);
        assert_type_is(mp, type->type);
        assert_len(mp, 0);
        yp_decrefN(N(mp, x));
    }

    // x contains duplicates.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        ypObject *x = (*x_type)->newN(N(keys[0], keys[1], keys[0]));
        ypObject *mp = any_fromkeys(x, values[0]);
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[0]);
        yp_decrefN(N(mp, x));
    }

    // x contains an unhashable key.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ypObject *unhashable = rand_obj_any_mutable_unique(2, keys);
            ead(x, (*x_type)->newN(N(unhashable)),
                    assert_raises(any_fromkeys(x, values[0]), yp_TypeError));
            ead(x, (*x_type)->newN(N(keys[0], unhashable)),
                    assert_raises(any_fromkeys(x, values[0]), yp_TypeError));
            ead(x, (*x_type)->newN(N(keys[0], keys[1], unhashable)),
                    assert_raises(any_fromkeys(x, values[0]), yp_TypeError));
            yp_decrefN(N(unhashable));
        }
    }

    // Unhashable key rejected even if equal to other hashable key.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Skip types that cannot store unhashable objects.
        if (type_stores_unhashables(*x_type)) {
            ead(x, (*x_type)->newN(N(int_1, intstore_1)),
                    assert_raises(any_fromkeys(x, values[0]), yp_TypeError));
            ead(x, (*x_type)->newN(N(intstore_1, int_1)),
                    assert_raises(any_fromkeys(x, values[0]), yp_TypeError));
        }
    }

    // Optimization: empty immortal when x is empty.
    if (type->falsy != NULL) {
        for (x_type = x_types; (*x_type) != NULL; x_type++) {
            ypObject *x = (*x_type)->newN(0);
            ypObject *mp = any_fromkeys(x, values[0]);
            assert_obj(mp, is, type->falsy);
            yp_decrefN(N(mp, x));
        }
    }

    // Iterator exceptions and bad length hints.
    faulty_iter_tests(ypObject * mp, x, yp_tupleN(N(keys[0], keys[1])),
            mp = any_fromkeys(x, values[0]),
            assert_mapping(mp, keys[0], values[0], keys[1], values[0]), yp_decref(mp));

    // x is not an iterable.
    assert_raises(any_fromkeys(int_1, values[0]), yp_TypeError);

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_fromkeys(yp_SyntaxError, yp_None), yp_SyntaxError);
        // FIXME Initially this didn't fail because we only checked value for an exception when
        // trying to push to the dict. Add more tests everywhere to ensure that all exceptions are
        // passed through even for paths that may be skipped (i.e. multiple arguments).
        assert_isexception(any_fromkeys(yp_tuple_empty, yp_SyntaxError), yp_SyntaxError);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(intstore_1, int_1));
    return MUNIT_OK;
}

static ypObject *fromkeysN_to_frozendict_fromkeysNV(ypObject *value, int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_frozendict_fromkeysNV(value, n, args);
    va_end(args);
    return result;
}

static ypObject *fromkeysN_to_dict_fromkeysNV(ypObject *value, int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_dict_fromkeysNV(value, n, args);
    va_end(args);
    return result;
}

static MunitResult test_fromkeysN(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*fromkeysN)(ypObject *, int, ...);
    ypObject *(*fromkeysN_to_fromkeysNV)(ypObject *, int, ...);
    MunitResult test_result;

    if (type->type == yp_t_frozendict) {
        fromkeysN = yp_frozendict_fromkeysN;
        fromkeysN_to_fromkeysNV = fromkeysN_to_frozendict_fromkeysNV;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        fromkeysN = yp_dict_fromkeysN;
        fromkeysN_to_fromkeysNV = fromkeysN_to_dict_fromkeysNV;
    }

    test_result = _test_fromkeysN(type, fromkeysN, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    test_result =
            _test_fromkeysN(type, fromkeysN_to_fromkeysNV, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    return MUNIT_OK;
}

static ypObject *fromkeysN_to_frozendict_fromkeys(ypObject *value, int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_frozendict_fromkeys(iterable, value);
    yp_decref(iterable);
    return result;
}

static ypObject *fromkeysN_to_dict_fromkeys(ypObject *value, int n, ...)
{
    va_list   args;
    ypObject *iterable;
    ypObject *result;

    va_start(args, n);
    iterable = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_dict_fromkeys(iterable, value);
    yp_decref(iterable);
    return result;
}

static MunitResult test_fromkeys(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject *(*fromkeysN_to_fromkeys)(ypObject *, int, ...);
    ypObject *(*fromkeys)(ypObject *, ypObject *);
    MunitResult test_result;

    if (type->type == yp_t_frozendict) {
        fromkeysN_to_fromkeys = fromkeysN_to_frozendict_fromkeys;
        fromkeys = yp_frozendict_fromkeys;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        fromkeysN_to_fromkeys = fromkeysN_to_dict_fromkeys;
        fromkeys = yp_dict_fromkeys;
    }

    test_result = _test_fromkeysN(type, fromkeysN_to_fromkeys, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    test_result = _test_fromkeys(type, fromkeys, /*test_exception_passthrough=*/TRUE);
    if (test_result != MUNIT_OK) return test_result;

    return MUNIT_OK;
}


// FIXME "frozendict_dirty", "dict_dirty", shared key?
char *param_values_test_frozendict[] = {"frozendict", "dict", NULL};

static MunitParameterEnum test_frozendict_params[] = {
        {param_key_type, param_values_test_frozendict}, {NULL}};

MunitTest test_frozendict_tests[] = {TEST(test_newK, test_frozendict_params),
        TEST(test_new, test_frozendict_params), TEST(test_call_type, test_frozendict_params),
        TEST(test_fromkeysN, test_frozendict_params), TEST(test_fromkeys, test_frozendict_params),
        {NULL}};


extern void test_frozendict_initialize(void) {}

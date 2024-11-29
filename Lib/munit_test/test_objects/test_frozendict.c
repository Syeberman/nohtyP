
#include "munit_test/unittest.h"


// TODO fixture_type_frozenset, fixture_type_set, fixture_type_frozenset_dirty, and
// fixture_type_set_dirty are tricky because they require hashable types but in (key, value) the
// value may not be hashable.
// TODO "Shared key" versions, somehow? fixture_type_frozendict_shared, fixture_type_dict_shared
#define x_types_init()                                                                  \
    {fixture_type_frozendict, fixture_type_dict, fixture_type_iter, fixture_type_tuple, \
            fixture_type_list, fixture_type_frozendict_dirty, fixture_type_dict_dirty, NULL}

#define friend_types_init()                                                     \
    {fixture_type_frozendict, fixture_type_dict, fixture_type_frozendict_dirty, \
            fixture_type_dict_dirty, NULL}

// A copy of ypDictMiState from nohtyP.c, as a union with yp_uint64_t to maintain strict aliasing.
typedef union {
    struct {
        yp_uint32_t keys : 1;
        yp_uint32_t itemsleft : 31;
        yp_uint32_t values : 1;
        yp_uint32_t index : 31;
    } as_struct;
    yp_uint64_t as_int;
} ypDictMiState;

// Defines an mi_state for use with frozendicts/dicts, which can be accessed by the variable name,
// declared as "const yp_uint64_t".
#define define_frozendict_mi_state(name, keys, values, itemsleft, index)             \
    ypDictMiState     _##name##_struct = {{(keys), (itemsleft), (values), (index)}}; \
    const yp_uint64_t name = (_##name##_struct.as_int)

// Extracts keys from mi_state.
#define frozendict_mi_state_keys(mi_state) (((ypDictMiState *)(&(mi_state)))->as_struct.keys)
// Extracts values from mi_state.
#define frozendict_mi_state_values(mi_state) (((ypDictMiState *)(&(mi_state)))->as_struct.values)
// Extracts itemsleft from mi_state.
#define frozendict_mi_state_itemsleft(mi_state) \
    (((ypDictMiState *)(&(mi_state)))->as_struct.itemsleft)
// Extracts index from mi_state.
#define frozendict_mi_state_index(mi_state) (((ypDictMiState *)(&(mi_state)))->as_struct.index)


static void _test_newK(
        fixture_type_t *type, ypObject *(*any_newK)(int, ...), int test_exception_passthrough)
{
    uniqueness_t      *uq = uniqueness_new();
    hashability_pair_t pair = rand_obj_any_hashability_pair(uq);
    ypObject          *keys[4];
    ypObject          *values[4];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

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
        ypObject *unhashable = rand_obj_any_mutable(uq);
        assert_raises(any_newK(K(unhashable, values[0])), yp_TypeError);
        assert_raises(any_newK(K(keys[0], values[0], unhashable, values[1])), yp_TypeError);
        assert_raises(any_newK(K(keys[0], values[0], keys[1], values[1], unhashable, values[2])),
                yp_TypeError);
        yp_decrefN(N(unhashable));
    }

    // Unhashable key rejected even if equal to other hashable key.
    assert_raises(any_newK(K(pair.hashable, values[0], pair.unhashable, values[1])), yp_TypeError);
    assert_raises(any_newK(K(pair.unhashable, values[0], pair.hashable, values[1])), yp_TypeError);

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
    yp_decrefN(N(pair.unhashable, pair.hashable));
    uniqueness_dealloc(uq);
}

static void _test_new(
        fixture_type_t *type, ypObject *(*any_new)(ypObject *), int test_exception_passthrough)
{
    fixture_type_t    *x_types[] = x_types_init();
    fixture_type_t    *friend_types[] = friend_types_init();
    fixture_type_t   **x_type;
    uniqueness_t      *uq = uniqueness_new();
    hashability_pair_t pair = rand_obj_any_hashability_pair(uq);
    ypObject          *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject          *keys[4];
    ypObject          *values[4];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

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
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable(uq);
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
        if (!(*x_type)->hashable_items_only) {
            ead(x, new_itemsK(*x_type, K(pair.hashable, values[0], pair.unhashable, values[1])),
                    assert_raises(any_new(x), yp_TypeError));
            ead(x, new_itemsK(*x_type, K(pair.unhashable, values[0], pair.hashable, values[1])),
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
    assert_raises(any_new(not_iterable), yp_TypeError);

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_new(yp_SyntaxError), yp_SyntaxError);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    uniqueness_dealloc(uq);
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

    if (type->type == yp_t_frozendict) {
        newK = yp_frozendictK;
        newK_to_newKV = newK_to_frozendictKV;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        newK = yp_dictK;
        newK_to_newKV = newK_to_dictKV;
    }

    // Shared tests.
    _test_newK(type, newK, /*test_exception_passthrough=*/TRUE);
    _test_newK(type, newK_to_newKV, /*test_exception_passthrough=*/TRUE);

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

    if (type->type == yp_t_frozendict) {
        newK_to_new = newK_to_frozendict;
        new_ = yp_frozendict;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        newK_to_new = newK_to_dict;
        new_ = yp_dict;
    }

    // Shared tests.
    _test_newK(type, newK_to_new, /*test_exception_passthrough=*/FALSE);
    _test_new(type, new_, /*test_exception_passthrough=*/TRUE);

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

// For faulty_iter_tests in test_call_type, the faulty iterator needs to be inside an args tuple.
static ypObject *test_call_type_faulty_iter_helper(
        ypObject *type, ypObject *iterable, ypObject *kwargs)
{
    ypObject *args = yp_tupleN(N(iterable));
    ypObject *result = yp_call_stars(type, args, kwargs);
    yp_decref(args);
    return result;
}

static MunitResult test_call_type(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init();
    fixture_type_t **x_type;
    ypObject *(*newK_to_call_args_type)(int, ...);
    ypObject *(*new_to_call_args_type)(ypObject *);
    uniqueness_t *uq = uniqueness_new();
    ypObject     *str_rand = rand_obj(uq, fixture_type_str);
    ypObject     *str_cls = yp_str_frombytesC2(-1, "cls");
    ypObject     *str_object = yp_str_frombytesC2(-1, "object");
    ypObject     *keys[4];
    ypObject     *values[4];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

    if (type->type == yp_t_frozendict) {
        newK_to_call_args_type = newK_to_call_args_t_frozendict;
        new_to_call_args_type = new_to_call_args_t_frozendict;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        newK_to_call_args_type = newK_to_call_args_t_dict;
        new_to_call_args_type = new_to_call_args_t_dict;
    }

    // Shared tests.
    _test_newK(type, newK_to_call_args_type, /*test_exception_passthrough=*/FALSE);
    _test_new(type, new_to_call_args_type, /*test_exception_passthrough=*/TRUE);

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

    // Iterator exceptions and bad length hints when merged with keyword arguments.
    {
        ypObject *kwargs = yp_frozendictK(K(str_rand, values[2]));
        faulty_iter_tests(ypObject * mp, x,
                new_itemsK(fixture_type_tuple, K(keys[0], values[0], keys[1], values[1])),
                mp = test_call_type_faulty_iter_helper(type->type, x, kwargs),
                assert_mapping(mp, keys[0], values[0], keys[1], values[1], str_rand, values[2]),
                yp_decref(mp));
        yp_decref(kwargs);
    }

    // Exception passthrough.
    assert_isexception(yp_callN(type->type, N(yp_SyntaxError)), yp_SyntaxError);

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(str_object, str_cls, str_rand));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static void _test_fromkeysN(fixture_type_t *type, ypObject *(*any_fromkeysN)(ypObject *, int, ...),
        int                                 test_exception_passthrough)
{
    uniqueness_t      *uq = uniqueness_new();
    hashability_pair_t pair = rand_obj_any_hashability_pair(uq);
    ypObject          *keys[4];
    ypObject          *values[4];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

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

    // Duplicate keys.
    {
        ypObject *mp = any_fromkeysN(values[0], N(keys[0], keys[1], keys[1], keys[0]));
        assert_type_is(mp, type->type);
        assert_mapping(mp, keys[0], values[0], keys[1], values[0]);
        yp_decrefN(N(mp));
    }

    // Unhashable key.
    {
        ypObject *unhashable = rand_obj_any_mutable(uq);
        assert_raises(any_fromkeysN(values[0], N(unhashable)), yp_TypeError);
        assert_raises(any_fromkeysN(values[0], N(keys[0], unhashable)), yp_TypeError);
        assert_raises(any_fromkeysN(values[0], N(keys[0], keys[1], unhashable)), yp_TypeError);
        yp_decrefN(N(unhashable));
    }

    // Unhashable key rejected even if equal to other hashable key.
    assert_raises(any_fromkeysN(values[0], N(pair.hashable, pair.unhashable)), yp_TypeError);
    assert_raises(any_fromkeysN(values[0], N(pair.unhashable, pair.hashable)), yp_TypeError);

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
    yp_decrefN(N(pair.unhashable, pair.hashable));
    uniqueness_dealloc(uq);
}

static void _test_fromkeys(fixture_type_t *type, ypObject *(*any_fromkeys)(ypObject *, ypObject *),
        int                                test_exception_passthrough)
{
    fixture_type_t    *x_types[] = x_types_init();
    fixture_type_t   **x_type;
    uniqueness_t      *uq = uniqueness_new();
    hashability_pair_t pair = rand_obj_any_hashability_pair(uq);
    ypObject          *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject          *keys[4];
    ypObject          *values[4];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

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
        if (!(*x_type)->hashable_items_only) {
            ypObject *unhashable = rand_obj_any_mutable(uq);
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
        if (!(*x_type)->hashable_items_only) {
            ead(x, (*x_type)->newN(N(pair.hashable, pair.unhashable)),
                    assert_raises(any_fromkeys(x, values[0]), yp_TypeError));
            ead(x, (*x_type)->newN(N(pair.unhashable, pair.hashable)),
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
    assert_raises(any_fromkeys(not_iterable, values[0]), yp_TypeError);

    // Exception passthrough.
    if (test_exception_passthrough) {
        assert_isexception(any_fromkeys(yp_SyntaxError, yp_None), yp_SyntaxError);
        assert_isexception(any_fromkeys(yp_tuple_empty, yp_SyntaxError), yp_SyntaxError);
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    yp_decrefN(N(not_iterable, pair.unhashable, pair.hashable));
    uniqueness_dealloc(uq);
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

    if (type->type == yp_t_frozendict) {
        fromkeysN = yp_frozendict_fromkeysN;
        fromkeysN_to_fromkeysNV = fromkeysN_to_frozendict_fromkeysNV;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        fromkeysN = yp_dict_fromkeysN;
        fromkeysN_to_fromkeysNV = fromkeysN_to_dict_fromkeysNV;
    }

    // Shared tests.
    _test_fromkeysN(type, fromkeysN, /*test_exception_passthrough=*/TRUE);
    _test_fromkeysN(type, fromkeysN_to_fromkeysNV, /*test_exception_passthrough=*/TRUE);

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

    if (type->type == yp_t_frozendict) {
        fromkeysN_to_fromkeys = fromkeysN_to_frozendict_fromkeys;
        fromkeys = yp_frozendict_fromkeys;
    } else {
        assert_ptr(type->type, ==, yp_t_dict);
        fromkeysN_to_fromkeys = fromkeysN_to_dict_fromkeys;
        fromkeys = yp_dict_fromkeys;
    }

    // Shared tests.
    _test_fromkeysN(type, fromkeysN_to_fromkeys, /*test_exception_passthrough=*/TRUE);
    _test_fromkeys(type, fromkeys, /*test_exception_passthrough=*/TRUE);

    return MUNIT_OK;
}

// frozendict- and dict-specific miniiter tests; see test_iterable for more tests.
static MunitResult test_miniiter(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    define_frozendict_mi_state(k_itemsleft_255_index_255, 1, 0, 255, 255);
    define_frozendict_mi_state(kv_itemsleft_2_index_0, 1, 1, 2, 0);
    define_frozendict_mi_state(k_itemsleft_2_index_0, 1, 0, 2, 0);
    define_frozendict_mi_state(v_itemsleft_2_index_0, 0, 1, 2, 0);
    define_frozendict_mi_state(nokv_itemsleft_2_index_0, 0, 0, 2, 0);  // keys=0, values=0
    define_frozendict_mi_state(k_itemsleft_1_index_0, 1, 0, 1, 0);
    define_frozendict_mi_state(k_itemsleft_0_index_0, 1, 0, 0, 0);
    uniqueness_t *uq = uniqueness_new();
    ypObject     *keys[4];
    ypObject     *values[4];
    obj_array_fill(keys, uq, type->rand_items);
    obj_array_fill(values, uq, type->rand_values);

    // Corrupted states.
    {
        yp_uint64_t mi_state;
        yp_ssize_t  i;
        yp_uint64_t bad_states[] = {0uLL, (yp_uint64_t)-1, k_itemsleft_255_index_255};
        ypObject   *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject   *mi = yp_miniiter(mp, &mi_state);
        munit_assert_uint64(mi_state, ==, k_itemsleft_2_index_0);

        for (i = 0; i < yp_lengthof_array(bad_states); i++) {
            mi_state = bad_states[i];
            assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
            munit_assert_uint64(mi_state, ==, bad_states[i]);  // unchanged

            assert_ssizeC_exc(yp_miniiter_length_hintC(mi, &mi_state, &exc), ==, 0);
            munit_assert_uint64(mi_state, ==, bad_states[i]);  // unchanged
        }

        yp_decrefN(N(mp, mi));
    }

    // Trigger the `index >= ypDict_ALLOCLEN(mp)` case in the loop of *_next. itemsleft usually
    // terminates iteration, but if itemsleft is corrupted, or if an entry is removed from mp, then
    // we need to ensure we stop at alloclen.
    {
        yp_uint64_t mi_state;
        ypObject   *mp = type->newK(0);
        ypObject   *mi = yp_miniiter(mp, &mi_state);
        munit_assert_uint64(mi_state, ==, k_itemsleft_0_index_0);

        mi_state = k_itemsleft_1_index_0;
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        // itemsleft set to zero to exhaust the iterator
        munit_assert_uint64(mi_state, ==, k_itemsleft_0_index_0);

        yp_decrefN(N(mp, mi));
    }

    // yp_miniiter_next requires keys=1 and/or values=1.
    {
        yp_uint64_t mi_state;
        ypObject   *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject   *mi = yp_miniiter(mp, &mi_state);
        munit_assert_uint64(mi_state, ==, k_itemsleft_2_index_0);

        mi_state = nokv_itemsleft_2_index_0;
        assert_raises(yp_miniiter_next(mi, &mi_state), yp_StopIteration);
        // mi_state is changed because the check for `keys || values` is after the index is yielded.
        munit_assert_uint32(frozendict_mi_state_keys(mi_state), ==, 0);
        munit_assert_uint32(frozendict_mi_state_values(mi_state), ==, 0);
        munit_assert_uint32(frozendict_mi_state_itemsleft(mi_state), ==, 1);
        munit_assert_uint32(frozendict_mi_state_index(mi_state), >, 0);

        yp_decrefN(N(mp, mi));
    }

    // yp_miniiter_items_next requires keys=1 and values=1.
    {
        yp_uint64_t mi_state;
        yp_ssize_t  i;
        yp_uint64_t bad_states[] = {
                k_itemsleft_2_index_0, v_itemsleft_2_index_0, nokv_itemsleft_2_index_0};
        ypObject *mp = type->newK(K(keys[0], values[0], keys[1], values[1]));
        ypObject *mi = yp_miniiter_items(mp, &mi_state);
        munit_assert_uint64(mi_state, ==, kv_itemsleft_2_index_0);

        for (i = 0; i < yp_lengthof_array(bad_states); i++) {
            ypObject *value = NULL;
            mi_state = bad_states[i];
            assert_raises_exc(yp_miniiter_items_next(mi, &mi_state, &exc, &value), yp_TypeError);
            assert_isexception(value, yp_TypeError);
            munit_assert_uint64(mi_state, ==, bad_states[i]);  // unchanged
        }
        yp_decrefN(N(mp, mi));
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// FIXME Try some more things with valgrind?
static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *str_keys[] = obj_array_init(16, rand_obj(uq, fixture_type_str));
    ypObject       *keys[16];
    obj_array_fill(keys, uq, type->rand_items);

    // Ensure that the function object has been validated first. _ypFunction_validate_parameters is
    // called once per object, and always allocates a set; this allocation interferes with our
    // malloc_tracker_oom_after allocation counts. Avoid this issue by triggering validation now.
    // XXX Interestingly, munit's forking capability (disabled on Windows) exposes these issues.
    ead(mp, yp_call_stars(type->type, yp_tuple_empty, yp_frozendict_empty),
            assert_not_exception(mp));

    // _ypDict_copy
    {
        ypObject *mp = type->newN(N(keys[0], keys[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_unfrozen_copy(mp), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(mp));
    }

    // _ypDict_update_fromdict, new keyset
    if (type->is_mutable) {
        ypObject *mp = type->newN(N(keys[0], keys[1]));
        ypObject *x = yp_frozendict_fromkeysN(
                yp_None, N(keys[2], keys[3], keys[4], keys[5], keys[6], keys[7], keys[8], keys[9],
                                 keys[10], keys[11], keys[12], keys[13], keys[14], keys[15]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_update(mp, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(mp, x));
    }

    // _ypDict_update_fromdict, resize
    if (type->is_mutable) {
        ypObject *mp = type->newN(N(keys[0], keys[1]));
        ypObject *x = yp_frozendict_fromkeysN(
                yp_None, N(keys[2], keys[3], keys[4], keys[5], keys[6], keys[7], keys[8], keys[9],
                                 keys[10], keys[11], keys[12], keys[13], keys[14], keys[15]));
        malloc_tracker_oom_after(1);  // allow _ypSet_new to succeed
        assert_raises_exc(yp_update(mp, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(mp, x));
    }

    // dict_clear
    if (type->is_mutable) {
        ypObject *mp = type->newN(N(keys[0], keys[1]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_clear(mp, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(mp));
    }

    // dict_setdefault
    if (type->is_mutable) {
        int       i;
        ypObject *result = yp_None;
        ypObject *mp = type->newN(N(keys[0], keys[1]));
        malloc_tracker_oom_after(0);
        for (i = 2; i < yp_lengthof_array(keys); i++) {
            result = yp_setdefault(mp, keys[i], yp_None);
            if (yp_isexceptionC(result)) break;
        }
        assert_isexception(result, yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(mp));
    }

    // frozendict_func_new_code/dict_func_new_code, new keyset
    // XXX The type->type function object must already be validated (see above).
    {
        ypObject *x = yp_dictK(0);
        ypObject *args = yp_tupleN(N(x));
        ypObject *kwargs = yp_frozendict_fromkeysN(yp_None, N(str_keys[0]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_call_stars(type->type, args, kwargs), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(kwargs, args, x));
    }

    // frozendict_func_new_code/dict_func_new_code, new dict
    // XXX The type->type function object must already be validated (see above).
    {
        ypObject *x = yp_dictK(0);
        ypObject *args = yp_tupleN(N(x));
        ypObject *kwargs = yp_frozendict_fromkeysN(yp_None, N(str_keys[0]));
        malloc_tracker_oom_after(1);  // allow _ypDict_new (keyset) to succeed
        assert_raises(yp_call_stars(type->type, args, kwargs), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(kwargs, args, x));
    }

    // frozendict_func_new_code/dict_func_new_code, resize
    // XXX The type->type function object must already be validated (see above).
    {
        ypObject *x = yp_dictK(0);
        ypObject *args = yp_tupleN(N(x));
        ypObject *kwargs = yp_frozendict_fromkeysN(yp_None,
                N(str_keys[0], str_keys[1], str_keys[2], str_keys[3], str_keys[4], str_keys[5],
                        str_keys[6], str_keys[7], str_keys[8], str_keys[9], str_keys[10],
                        str_keys[11], str_keys[12], str_keys[13], str_keys[14], str_keys[15]));
        malloc_tracker_oom_after(2);  // allow _ypDict_new (keyset and dict) to succeed
        assert_raises(yp_call_stars(type->type, args, kwargs), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(kwargs, args, x));
    }

    // _ypDictKV, new keyset
    {
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_frozendict) {
            assert_raises(yp_frozendictK(K(keys[0], yp_None)), yp_MemoryError);
        } else {
            assert_raises(yp_dictK(K(keys[0], yp_None)), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // _ypDictKV, new dict
    {
        malloc_tracker_oom_after(1);  // allow _ypDict_new (keyset) to succeed
        if (type->type == yp_t_frozendict) {
            assert_raises(yp_frozendictK(K(keys[0], yp_None)), yp_MemoryError);
        } else {
            assert_raises(yp_dictK(K(keys[0], yp_None)), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // _ypDict_new_fromiterable
    {
        ypObject *x = new_itemsK(fixture_type_list, K(keys[0], yp_None));
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_frozendict) {
            assert_raises(yp_frozendict(x), yp_MemoryError);
        } else {
            assert_raises(yp_dict(x), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // _ypDict_fromkeysNV
    {
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_frozendict) {
            assert_raises(yp_frozendict_fromkeysN(yp_None, N(keys[0])), yp_MemoryError);
        } else {
            assert_raises(yp_dict_fromkeysN(yp_None, N(keys[0])), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // _ypDict_fromkeys
    {
        ypObject *x = yp_listN(N(keys[0]));
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_frozendict) {
            assert_raises(yp_frozendict_fromkeys(x, yp_None), yp_MemoryError);
        } else {
            assert_raises(yp_dict_fromkeys(x, yp_None), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    obj_array_decref(keys);
    obj_array_decref(str_keys);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}
char *param_values_test_frozendict[] = {
        "frozendict", "dict", "frozendict_dirty", "dict_dirty", NULL};

static MunitParameterEnum test_frozendict_params[] = {
        {param_key_type, param_values_test_frozendict}, {NULL}};

MunitTest test_frozendict_tests[] = {TEST(test_newK, test_frozendict_params),
        TEST(test_new, test_frozendict_params), TEST(test_call_type, test_frozendict_params),
        TEST(test_fromkeysN, test_frozendict_params), TEST(test_fromkeys, test_frozendict_params),
        TEST(test_miniiter, test_frozendict_params), TEST(test_oom, test_frozendict_params),
        {NULL}};


extern void test_frozendict_initialize(void) {}

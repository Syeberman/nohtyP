
#include "munit_test/unittest.h"

// TODO We go to the trouble of having fixture_type_t.rand_items/etc to allow the type to control
// what types of items are stored inside it. But then we use functions like
// rand_obj_any_hashability_pair, etc that could return objects that aren't supported by the types
// under test. If this becomes a problem the tests will fail, but it may become a problem.


extern int yp_isexception_arrayC(ypObject *x, yp_ssize_t n, ypObject **exceptions)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) {
        if (yp_isexceptionC2(x, exceptions[i])) return TRUE;
    }
    return FALSE;
}


// Helper function for assert_setlike. If yp_miniiter_next succeeds, sets *actual to a new reference
// to the next yielded value, *items_i to the index in items of the first equal object, and returns
// true; if there is no equal object in items, *items_i is set to n. If mi is exhausted, *actual and
// *items_i are undefined, and false is returned. If yp_miniiter_next fails, *actual is set to the
// exception, *items_i is undefined, and true is returned.  n is the number of elements in items.
extern int _assert_setlike_helper(ypObject *mi, yp_uint64_t *mi_state, yp_ssize_t n,
        ypObject **items, ypObject **actual, yp_ssize_t *items_i)
{
    ypObject *eq;

    *actual = yp_miniiter_next(mi, mi_state);  // new ref
    if (yp_isexceptionC(*actual)) {
        if (yp_isexceptionC2(*actual, yp_StopIteration)) return FALSE;  // mi is exhausted
        return TRUE;  // yp_miniiter_next failed with an exception
    }

    for ((*items_i) = 0; (*items_i) < n; (*items_i)++) {
        // yp_eq would only fail here if items contained an exception.
        assert_not_raises(eq = yp_eq(*actual, items[*items_i]));
        if (eq == yp_True) break;  // equal item found, *items_i is less than n
    }
    return TRUE;  // if an equal item was not found, *items_i is equal to n
}

// Helper function for assert_mapping. If yp_miniiter_items_next succeeds, sets *actual_key and
// *actual_value to new references to the next yielded pair, *items_ki to the "k-index" in items of
// the first matching key, and returns true; if there is no equal keys in items, *items_ki is set to
// k. If mi is exhausted, *actual_key, *actual_value, and *items_ki are undefined, and false is
// returned. If yp_miniiter_items_next fails, *actual_key is set to the exception, *actual_value and
// *items_ki are undefined, and true is returned.  k is the number of (key, value) pairs in items;
// the index of the matching key is (*items_ki)*2, and the value is (*items_ki)*2+1.
extern int _assert_mapping_helper(ypObject *mi, yp_uint64_t *mi_state, yp_ssize_t k,
        ypObject **items, ypObject **actual_key, ypObject **actual_value, yp_ssize_t *items_ki)
{
    ypObject *eq;

    yp_miniiter_items_next(mi, mi_state, actual_key, actual_value);  // new refs
    if (yp_isexceptionC(*actual_key)) {
        assert_ptr(*actual_value, ==, *actual_key);
        if (yp_isexceptionC2(*actual_key, yp_StopIteration)) return FALSE;  // mi is exhausted
        return TRUE;  // yp_miniiter_items_next failed with an exception
    }
    assert_not_exception(*actual_value);

    for ((*items_ki) = 0; (*items_ki) < k; (*items_ki)++) {
        // yp_eq would only fail here if items contained an exception.
        assert_not_raises(eq = yp_eq(*actual_key, items[(*items_ki) * 2]));
        if (eq == yp_True) break;  // equal key found, *items_ki is less than k
    }
    return TRUE;  // if an equal key was not found, *items_ki is equal to k
}


static int array_contains(yp_ssize_t n, ypObject **array, ypObject *x)
{
    yp_ssize_t i;
    ypObject  *result;
    for (i = 0; i < n; i++) {
        assert_not_raises(result = yp_eq(array[i], x));
        if (result == yp_True) return TRUE;
    }
    return FALSE;
}

static int _array_sort_cmp(const void *_x, const void *_y)
{
    ypObject *x = *((ypObject **)_x);
    ypObject *y = *((ypObject **)_y);
    ypObject *result;
    assert_not_raises(result = yp_lt(x, y));
    if (result == yp_True) return -1;
    assert_not_raises(result = yp_gt(x, y));
    if (result == yp_True) return 1;
    // GCOVR_EXCL_START The arrays we are sorting contain unique elements.
    assert_not_raises(result = yp_eq(x, y));
    assert_obj(result, is, yp_True);
    return 0;
    // GCOVR_EXCL_STOP
}

// Sorts the array of objects in ascending order. The objects must all support total ordering with
// each other.
static void array_sort(yp_ssize_t n, ypObject **array)
{
    qsort(array, (size_t)n, sizeof(ypObject *), _array_sort_cmp);
}


// If something should happen 2 in 23 times: RAND_BOOL_FRACTION(2, 23)
// TODO Better name? Better argument names?
#define RAND_BOOL_FRACTION(numerator, denominator) \
    (munit_rand_int_range(0, denominator - 1) < numerator)

// 1 in 50 objects created are falsy.
#define RAND_OBJ_RETURN_FALSY() (RAND_BOOL_FRACTION(1, 50))

#define RAND_OBJ_DEFAULT_DEPTH (3)


static fixture_type_t fixture_type_type_struct;
static fixture_type_t fixture_type_NoneType_struct;
static fixture_type_t fixture_type_bool_struct;
static fixture_type_t fixture_type_int_struct;
static fixture_type_t fixture_type_intstore_struct;
static fixture_type_t fixture_type_float_struct;
static fixture_type_t fixture_type_floatstore_struct;
static fixture_type_t fixture_type_iter_struct;
static fixture_type_t fixture_type_range_struct;
static fixture_type_t fixture_type_bytes_struct;
static fixture_type_t fixture_type_bytearray_struct;
static fixture_type_t fixture_type_str_struct;
static fixture_type_t fixture_type_chrarray_struct;
static fixture_type_t fixture_type_tuple_struct;
static fixture_type_t fixture_type_list_struct;
static fixture_type_t fixture_type_frozenset_struct;
static fixture_type_t fixture_type_set_struct;
static fixture_type_t fixture_type_frozenset_dirty_struct;
static fixture_type_t fixture_type_set_dirty_struct;
static fixture_type_t fixture_type_frozendict_struct;
static fixture_type_t fixture_type_dict_struct;
static fixture_type_t fixture_type_frozendict_dirty_struct;
static fixture_type_t fixture_type_dict_dirty_struct;
static fixture_type_t fixture_type_function_struct;


// GCOVR_EXCL_START

static ypObject *objobjfunc_error(ypObject *x)
{
    munit_error("unsupported operation");
    return yp_SystemError;
}

static ypObject *objvarargfunc_error(int n, ...)
{
    munit_error("unsupported operation");
    return yp_SystemError;
}

static void rand_objs_func_error(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    munit_error("unsupported operation");
}

// GCOVR_EXCL_STOP


// Returns a random yp_int_t value. Prioritizes zero and small numbers.
static yp_int_t rand_intC(void)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return 0;

    } else if (RAND_BOOL_FRACTION(1, 10)) {
        // 1 in 10 will be large. munit doesn't supply a munit_rand_uint64, so make our own.
        return (yp_int_t)((((yp_uint64_t)munit_rand_uint32()) << 32u) | munit_rand_uint32());

    } else {
        // munit doesn't supply a munit_rand_int32 (signed), so make our own.
        // TODO Make these small values even smaller?
        return (yp_int_t)((yp_int32_t)munit_rand_uint32());
    }
}

// TODO Make large/long values less likely?
static yp_float_t rand_floatCF(void)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return 0.0;
    } else {
        return munit_rand_double();
    }
}

// Populates source with len random ascii bytes.
static void rand_ascii(yp_ssize_t len, yp_uint8_t *source)
{
    yp_ssize_t i;
    for (i = 0; i < len; i++) {
        source[i] = (yp_uint8_t)munit_rand_int_range(0, 0x7F);
    }
}

// Chooses a random element from the array with the given length
#define rand_choice(len, array) ((array)[munit_rand_int_range(0, ((int)(len)) - 1)])

// Chooses a random element from the array. Only call for arrays of fixed size that haven't been
// coerced to pointers.
#define rand_choice_array(array) rand_choice(yp_lengthof_array(array), (array))

static fixture_type_t *rand_choice_fixture_types(fixture_types_t *types)
{
    return rand_choice(types->len, types->types);
}


#define UNIQUENESS_MAX_LEN 100
#define UNIQUENESS_MAX_DUPLICATES 20

typedef struct _uniqueness_t {
    yp_ssize_t duplicates;                // Number of duplicates encountered.
    yp_ssize_t len;                       // Number of objects.
    ypObject  *objs[UNIQUENESS_MAX_LEN];  // Owned references.
} uniqueness_t;

extern uniqueness_t *uniqueness_new(void)
{
    uniqueness_t *uq = malloc(sizeof(uniqueness_t));
    uq->duplicates = 0;
    uq->len = 0;
    return uq;
}

// Our tests rely on having a set of objects that are unique from one another but that are otherwise
// random. When such objects are created, call uniqueness_push_array to ensure they are all
// unique. If any of the n objs are equal to the objects stored in uq, false is returned; otherwise,
// all objs are added to uq and true is returned. This fails the test if the number of duplicates
// encountered becomes too large. To test each object individually, call uniqueness_push.
static int uniqueness_push_array(uniqueness_t *uq, yp_ssize_t n, ypObject **objs)
{
    yp_ssize_t i;

    // In contexts where uniqueness is not required, use NULL for uq.
    if (uq == NULL || n < 1) return TRUE;

    for (i = 0; i < n; i++) {
        if (array_contains(uq->len, uq->objs, objs[i])) {
            uq->duplicates++;
            if (uq->duplicates > UNIQUENESS_MAX_DUPLICATES) {
                munit_error("too many duplicate objects encountered");  // GCOVR_EXCL_LINE
            }
            return FALSE;
        }
    }

    // Increase the size of the object array as necessary.
    assert_ssizeC(uq->len + n, <=, UNIQUENESS_MAX_LEN);

    // TODO Mutable values will change while we have a reference to them. Should we make immutable
    // copies here? Should they be deep copies for any nested mutables?
    for (i = 0; i < n; i++) {
        uq->objs[uq->len] = yp_incref(objs[i]);
        uq->len++;
    }
    return TRUE;
}

// A version of uniqueness_push_array that accepts a single object.
static int uniqueness_push(uniqueness_t *uq, ypObject *obj)
{
    ypObject *array[] = {obj};
    return uniqueness_push_array(uq, 1, array);
}

extern void uniqueness_dealloc(uniqueness_t *uq)
{
    while (uq->len > 0) {
        yp_decref(uq->objs[uq->len - 1]);
        uq->len--;
    }
    free(uq);
}

// A convenience macro to execute statement repeatedly until uniqueness_push indicates a unique
// object was produced, at which point that object is returned.
#define _return_unique(uq, statement)               \
    do {                                            \
        ypObject *obj = (statement); /* new ref */  \
        if (uniqueness_push((uq), obj)) return obj; \
        yp_decref(obj); /* loop until unique */     \
    } while (1)


typedef struct _rand_obj_supplier_memo_t {
    yp_ssize_t depth;          // The maximum depth of objects.
    int        only_hashable;  // If true, only hashable objects are returned.
} rand_obj_supplier_memo_t;

// "Any" may be limited by memo.
static ypObject *rand_obj_any_hashable_memo(const rand_obj_supplier_memo_t *memo)
{
    rand_obj_supplier_memo_t sub_memo = {memo->depth - 1, /*only_hashable=*/TRUE};
    assert_ssizeC(sub_memo.depth, >=, 0);
    return rand_choice_fixture_types(fixture_types_immutable)->_new_rand(&sub_memo);
}

// "Any" may be limited by memo (i.e. we might only return hashable types).
static ypObject *rand_obj_any_memo(const rand_obj_supplier_memo_t *memo)
{
    if (memo->only_hashable) {
        return rand_obj_any_hashable_memo(memo);
    } else {
        rand_obj_supplier_memo_t sub_memo = {memo->depth - 1, /*only_hashable=*/FALSE};
        assert_ssizeC(sub_memo.depth, >=, 0);
        return rand_choice_fixture_types(fixture_types_all)->_new_rand(&sub_memo);
    }
}

// Returns a 2-tuple of a hashable key and any value. Recall that "any" may be limited by memo.
static ypObject *rand_obj_any_keyvalue_memo(const rand_obj_supplier_memo_t *memo)
{
    ypObject *key = rand_obj_any_hashable_memo(memo);
    ypObject *value = rand_obj_any_memo(memo);
    ypObject *result = yp_tupleN(2, key, value);
    yp_decrefN(N(key, value));
    assert_not_exception(result);
    return result;
}

static ypObject *rand_obj_int(uniqueness_t *uq) { _return_unique(uq, yp_intC(rand_intC())); }

// XXX Interesting. 0 is a falsy byte, but '\x00' is not a falsy char.
static ypObject *rand_obj_byte(uniqueness_t *uq)
{
    _return_unique(uq, yp_intC(munit_rand_int_range(0, 255)));
}

// TODO Return more than just latin-1 characters
static ypObject *rand_obj_chr(uniqueness_t *uq)
{
    _return_unique(uq, yp_chrC(munit_rand_int_range(0, 255)));
}

static ypObject *_rand_obj_hashable(fixture_type_t *type)
{
    // Start with depth-1 as we are calling _new_rand ourselves.
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH - 1, /*only_hashable=*/TRUE};
    ypObject                *result = type->_new_rand(&memo);
    assert_false(type->is_mutable);
    return result;
}

static ypObject *_rand_obj(fixture_type_t *type)
{
    // Start with depth-1 as we are calling _new_rand ourselves.
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH - 1, /*only_hashable=*/FALSE};
    return type->_new_rand(&memo);
}

extern ypObject *rand_obj(uniqueness_t *uq, fixture_type_t *type)
{
    // None and bool have limited possible values, making uniqueness impossible.
    if (type == fixture_type_NoneType || type == fixture_type_bool) {
        if (uq != NULL) {
            munit_error("cannot ensure uniqueness for None and bool");  // GCOVR_EXCL_LINE
        }
        return _rand_obj(type);
    } else {
        _return_unique(uq, _rand_obj(type));
    }
}

extern ypObject *rand_obj_any_not_iterable(uniqueness_t *uq)
{
    _return_unique(uq, _rand_obj(rand_choice_fixture_types(fixture_types_not_iterable)));
}

extern ypObject *rand_obj_any_hashable_not_str(uniqueness_t *uq)
{
    _return_unique(
            uq, _rand_obj_hashable(rand_choice_fixture_types(fixture_types_immutable_not_str)));
}

static ypObject *rand_obj_any_hashable_paired(uniqueness_t *uq)
{
    _return_unique(
            uq, _rand_obj_hashable(rand_choice_fixture_types(fixture_types_immutable_paired)));
}

extern hashability_pair_t rand_obj_any_hashability_pair(uniqueness_t *uq)
{
    hashability_pair_t pair;
    pair.hashable = rand_obj_any_hashable_paired(uq);
    assert_not_raises(pair.unhashable = yp_unfrozen_copy(pair.hashable));
    return pair;
}

extern ypObject *rand_obj_any_mutable(uniqueness_t *uq)
{
    _return_unique(uq, _rand_obj(rand_choice_fixture_types(fixture_types_mutable)));
}

extern ypObject *rand_obj_any_hashable(uniqueness_t *uq)
{
    _return_unique(uq, _rand_obj_hashable(rand_choice_fixture_types(fixture_types_immutable)));
}

extern ypObject *rand_obj_any(uniqueness_t *uq)
{
    _return_unique(uq, _rand_obj(rand_choice_fixture_types(fixture_types_all)));
}

static void rand_objs_int(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) array[i] = rand_obj_int(uq);  // new ref
}

static void rand_objs_byte(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) array[i] = rand_obj_byte(uq);  // new ref
}

static void rand_objs_chr(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) array[i] = rand_obj_chr(uq);  // new ref
}

static void rand_objs_any_hashable(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) array[i] = rand_obj_any_hashable(uq);  // new ref
}

static void rand_objs_any(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) array[i] = rand_obj_any(uq);  // new ref
}

static void rand_objs_int_ordered(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    rand_objs_int(uq, n, array);
    array_sort(n, array);
}

static void rand_objs_byte_ordered(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    rand_objs_byte(uq, n, array);
    array_sort(n, array);
}

static void rand_objs_chr_ordered(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    rand_objs_chr(uq, n, array);
    array_sort(n, array);
}

// All objects will be of the same type that supports total ordering.
// TODO There are other types of objects we _could_ support here: tuples of strs fit the criteria,
// ints and floats can be compared, mutable and immutable forms can, etc.
static void rand_objs_any_ordered(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    void (*funcs[])(uniqueness_t *uq, yp_ssize_t n, ypObject **array) = {
            rand_objs_int_ordered, rand_objs_byte_ordered, rand_objs_chr_ordered};
    rand_choice_array(funcs)(uq, n, array);
}


static ypObject *_new_items_listKV(yp_ssize_t n, va_list args)
{
    ypObject *result;
    assert_not_raises(result = yp_listN(0));  // new ref
    for (/*n already initialized*/; n > 0; n--) {
        ypObject *item;
        // XXX va_arg calls must be made on separate lines: https://stackoverflow.com/q/1967659
        ypObject *key = va_arg(args, ypObject *);            // borrowed
        ypObject *value = va_arg(args, ypObject *);          // borrowed
        assert_not_raises(item = yp_tupleN(2, key, value));  // new ref
        assert_not_raises_exc(yp_append(result, item, &exc));
        yp_decref(item);
    }
    return result;
}

extern ypObject *new_itemsKV(fixture_type_t *type, yp_ssize_t n, va_list args)
{
    ypObject *result;
    ypObject *list = _new_items_listKV(n, args);  // new ref
    if (type == fixture_type_list) return list;
    result = type->new_(list);
    yp_decref(list);
    return result;
}

extern ypObject *new_itemsK(fixture_type_t *type, yp_ssize_t n, ...)
{
    ypObject *result;
    va_list   args;
    va_start(args, n);
    result = new_itemsKV(type, n, args);  // new ref
    va_end(args);
    return result;
}


typedef struct _rand_iter_state_t {
    yp_ssize_t               n;
    rand_obj_supplier_func   supplier;
    rand_obj_supplier_memo_t supplier_memo;
} rand_iter_state_t;

static yp_state_decl_t rand_iter_state_decl = {yp_sizeof(rand_iter_state_t)};

static ypObject *rand_iter_func(ypObject *g, ypObject *value)
{
    rand_iter_state_t *state;
    yp_ssize_t         size;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, &size, (void **)&state));
    assert_ssizeC(size, ==, yp_sizeof(*state));

    if (state->n < 1) return yp_StopIteration;
    state->n--;
    return state->supplier(&state->supplier_memo);
}

static ypObject *new_rand_iter3(yp_ssize_t n, rand_obj_supplier_func supplier,
        const rand_obj_supplier_memo_t *supplier_memo)
{
    ypObject           *result;
    rand_iter_state_t   state = {n, supplier};
    yp_generator_decl_t decl = {rand_iter_func, n, &state, &rand_iter_state_decl};
    state.supplier_memo = *supplier_memo;

    result = yp_generatorC(&decl);
    assert_not_exception(result);
    return result;
}

typedef struct _faulty_iter_state_t {
    ypObject  *supplier;   // Sub-iterator supplying values to yield until n reaches zero.
    yp_ssize_t n;          // Raise exception when this reaches zero.
    ypObject  *exception;  // Exception to raise.
} faulty_iter_state_t;

static yp_state_decl_t faulty_iter_state_decl = {yp_sizeof(faulty_iter_state_t), 2,
        {yp_offsetof(faulty_iter_state_t, supplier), yp_offsetof(faulty_iter_state_t, exception)}};

static ypObject *faulty_iter_func(ypObject *g, ypObject *value)
{
    faulty_iter_state_t *state;
    yp_ssize_t           size;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, &size, (void **)&state));
    assert_ssizeC(size, ==, yp_sizeof(*state));

    if (state->n < 1) return state->exception;
    state->n--;
    return yp_next(state->supplier);
}

extern ypObject *new_faulty_iter(
        ypObject *supplier, yp_ssize_t n, ypObject *exception, yp_ssize_t length_hint)
{
    ypObject           *result;
    faulty_iter_state_t state = {yp_iter(supplier) /*new ref*/, n, exception};
    yp_generator_decl_t decl = {faulty_iter_func, length_hint, &state, &faulty_iter_state_decl};
    assert_not_exception(state.supplier);
    assert_isexception(exception, yp_BaseException);

    result = yp_generatorC(&decl);
    yp_decref(state.supplier);  // Recall yp_generatorC makes its own references.
    assert_not_exception(result);
    return result;
}


// Returns a random type object, except invalidated and exception objects.
static ypObject *new_rand_type(const rand_obj_supplier_memo_t *memo)
{
    return rand_choice_fixture_types(fixture_types_all)->type;
}

static fixture_type_t fixture_type_type_struct = {
        "type",                     // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_type_struct,  // pair

        new_rand_type,  // _new_rand

        yp_type,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        TRUE,   // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

// There is only one NoneType object: yp_None.
static ypObject *new_rand_NoneType(const rand_obj_supplier_memo_t *memo) { return yp_None; }

static fixture_type_t fixture_type_NoneType_struct = {
        "NoneType",                     // name
        NULL,                           // type (initialized at runtime)
        NULL,                           // falsy (initialized at runtime, maybe)
        &fixture_type_NoneType_struct,  // pair

        new_rand_NoneType,  // _new_rand

        objobjfunc_error,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_bool(const rand_obj_supplier_memo_t *memo)
{
    if (munit_rand_int_range(0, 1)) {
        return yp_True;
    } else {
        return yp_False;
    }
}

static fixture_type_t fixture_type_bool_struct = {
        "bool",                     // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_bool_struct,  // pair

        new_rand_bool,  // _new_rand

        yp_bool,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_int(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_intC(rand_intC());
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_int_struct = {
        "int",                          // name
        NULL,                           // type (initialized at runtime)
        NULL,                           // falsy (initialized at runtime, maybe)
        &fixture_type_intstore_struct,  // pair

        new_rand_int,  // _new_rand

        yp_int,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_intstore(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_intstoreC(rand_intC());
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_intstore_struct = {
        "intstore",                // name
        NULL,                      // type (initialized at runtime)
        NULL,                      // falsy (initialized at runtime, maybe)
        &fixture_type_int_struct,  // pair

        new_rand_intstore,  // _new_rand

        yp_intstore,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        TRUE,   // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_float(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_floatCF(rand_floatCF());
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_float_struct = {
        "float",                          // name
        NULL,                             // type (initialized at runtime)
        NULL,                             // falsy (initialized at runtime, maybe)
        &fixture_type_floatstore_struct,  // pair

        new_rand_float,  // _new_rand

        yp_float,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_floatstore(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_floatstoreCF(rand_floatCF());
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_floatstore_struct = {
        "floatstore",                // name
        NULL,                        // type (initialized at runtime)
        NULL,                        // falsy (initialized at runtime, maybe)
        &fixture_type_float_struct,  // pair

        new_rand_floatstore,  // _new_rand

        yp_floatstore,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        TRUE,   // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_iter(const rand_obj_supplier_memo_t *memo)
{
    yp_ssize_t n = memo->depth < 1 ? 0 : munit_rand_int_range(0, 16);
    return new_rand_iter3(n, rand_obj_any_memo, memo);
}

static ypObject *newN_iter(int n, ...)
{
    va_list   args;
    ypObject *tuple;
    ypObject *result;

    va_start(args, n);
    tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_iter(tuple);
    yp_decref(tuple);
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_iter_struct = {
        "iter",                     // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_iter_struct,  // pair

        new_rand_iter,  // _new_rand

        yp_iter,  // new_

        newN_iter,      // newN
        rand_objs_any,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_any_ordered,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        FALSE,  // hashable_items_only
};

// TODO Ranges that cover more values, not just 32-bit-ish.
static yp_int_t range_rand_start(void) { return (yp_int_t)((yp_int32_t)munit_rand_uint32()); }
static yp_int_t range_rand_step(void)
{
    yp_int_t step = (yp_int_t)munit_rand_int_range(-128, 128);
    if (step == 0) step = 129;
    return step;
}

static ypObject *new_rand_range(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_range_empty;
    } else {
        yp_int_t  start = range_rand_start();
        yp_int_t  len = (yp_int_t)munit_rand_int_range(1, 256);
        yp_int_t  step = range_rand_step();
        ypObject *result = yp_rangeC3(start, start + (step * len), step);
        assert_not_exception(result);
        return result;
    }
}

// XXX The arguments must follow a valid range pattern, in order. Note that rand_items_range creates
// objects that follow this pattern.
// FIXME We should be using the index forms here that don't accept floats.
static ypObject *newN_range(int n, ...)
{
    va_list   args;
    yp_int_t  start;
    yp_int_t  stop;
    yp_int_t  step;
    int       i;
    ypObject *result;

    if (n < 1) return yp_range_empty;

    va_start(args, n);
    assert_not_raises_exc(start = yp_asintC(va_arg(args, ypObject *), &exc));
    if (n < 2) {
        stop = start + 1;
        step = 1;
        goto args_end;
    }

    assert_not_raises_exc(step = yp_asintC(va_arg(args, ypObject *), &exc) - start);
    assert_intC(step, !=, 0);
    stop = start + (n * step);

    // Ensure the remaining items match the pattern.
    for (i = 2; i < n; i++) {
        assert_intC_exc(yp_asintC(va_arg(args, ypObject *), &exc), ==, start + (i * step));
    }
args_end:
    va_end(args);

    result = yp_rangeC3(start, stop, step);
    assert_not_exception(result);
    return result;
}

// Fills array with n integers that cover a range with a random start and step. Any slice of these
// integers is suitable to pass to newN_range to construct a new range.
static void _rand_items_range(uniqueness_t *uq, yp_ssize_t n, ypObject **array, int ordered)
{
    while (1) {
        yp_ssize_t i;
        yp_int_t   start = range_rand_start();
        yp_int_t   step = range_rand_step();
        if (ordered && step < 0) step = -step;  // rand_ordered_items requires ascending values.

        for (i = 0; i < n; i++) {
            assert_not_raises(array[i] = yp_intC(start + (i * step)));
        }

        // Test all selected integers for uniqueness as a group. If any single integer has a
        // duplicate, discard all integers and try again.
        if (uniqueness_push_array(uq, n, array)) return;
        obj_array_decref2(n, array);
    }
}

static void rand_items_range(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    _rand_items_range(uq, n, array, /*ordered=*/FALSE);
}

static void rand_ordered_items_range(uniqueness_t *uq, yp_ssize_t n, ypObject **array)
{
    _rand_items_range(uq, n, array, /*ordered=*/TRUE);
}

static fixture_type_t fixture_type_range_struct = {
        "range",                     // name
        NULL,                        // type (initialized at runtime)
        NULL,                        // falsy (initialized at runtime, maybe)
        &fixture_type_range_struct,  // pair

        new_rand_range,  // _new_rand

        objobjfunc_error,  // new_

        newN_range,        // newN
        rand_items_range,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_ordered_items_range,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_bytes(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_bytes_empty;
    } else {
        ypObject  *result;
        yp_uint8_t source[16];
        yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
        munit_rand_memory((size_t)len, source);
        result = yp_bytesC(len, source);
        assert_not_exception(result);
        return result;
    }
}

// There is no yp_bytesN, because this is an odd way to construct a bytes.
static ypObject *newN_bytes(int n, ...)
{
    va_list   args;
    ypObject *tuple;
    ypObject *result;

    va_start(args, n);
    tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_bytes(tuple);
    yp_decref(tuple);
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_bytes_struct = {
        "bytes",                         // name
        NULL,                            // type (initialized at runtime)
        NULL,                            // falsy (initialized at runtime, maybe)
        &fixture_type_bytearray_struct,  // pair

        new_rand_bytes,  // _new_rand

        yp_bytes,  // new_

        newN_bytes,      // newN
        rand_objs_byte,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_byte_ordered,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_bytearray(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_bytearray0();
    } else {
        ypObject  *result;
        yp_uint8_t source[16];
        yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
        munit_rand_memory((size_t)len, source);
        result = yp_bytearrayC(len, source);
        assert_not_exception(result);
        return result;
    }
}

// There is no yp_bytearrayN, because this is an odd way to construct a bytearray.
static ypObject *newN_bytearray(int n, ...)
{
    va_list   args;
    ypObject *tuple;
    ypObject *result;

    va_start(args, n);
    tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_bytearray(tuple);
    yp_decref(tuple);
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_bytearray_struct = {
        "bytearray",                 // name
        NULL,                        // type (initialized at runtime)
        NULL,                        // falsy (initialized at runtime, maybe)
        &fixture_type_bytes_struct,  // pair

        new_rand_bytearray,  // _new_rand

        yp_bytearray,  // new_

        newN_bytearray,  // newN
        rand_objs_byte,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_byte_ordered,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

// TODO Return larger characters than just ascii.
static ypObject *new_rand_str(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_str_empty;
    } else {
        ypObject  *result;
        yp_uint8_t source[16];
        yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
        rand_ascii(len, source);
        result = yp_str_frombytesC4(len, source, yp_s_utf_8, yp_s_strict);
        assert_not_exception(result);
        return result;
    }
}

// There is no yp_strN, because this is an odd way to construct a str.
static ypObject *newN_str(int n, ...)
{
    va_list   args;
    ypObject *tuple;
    ypObject *result;

    va_start(args, n);
    tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    // Recall that yp_str isn't a typical container constructor, so we use yp_concat.
    result = yp_concat(yp_str_empty, tuple);
    yp_decref(tuple);
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_str_struct = {
        "str",                          // name
        NULL,                           // type (initialized at runtime)
        NULL,                           // falsy (initialized at runtime, maybe)
        &fixture_type_chrarray_struct,  // pair

        new_rand_str,  // _new_rand

        yp_str,  // new_

        newN_str,       // newN
        rand_objs_chr,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_chr_ordered,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

// TODO Return larger characters than just ascii.
static ypObject *new_rand_chrarray(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_chrarray0();
    } else {
        ypObject  *result;
        yp_uint8_t source[16];
        yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
        rand_ascii(len, source);
        result = yp_chrarray_frombytesC4(len, source, yp_s_utf_8, yp_s_strict);
        assert_not_exception(result);
        return result;
    }
}

// There is no yp_chrarrayN, because this is an odd way to construct a chrarray.
static ypObject *newN_chrarray(int n, ...)
{
    va_list   args;
    ypObject *tuple;
    ypObject *result;

    va_start(args, n);
    tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    // Recall that yp_chrarray isn't a typical container constructor, so we use yp_extend.
    result = yp_chrarray0();
    assert_not_exception(result);
    assert_not_raises_exc(yp_extend(result, tuple, &exc));
    yp_decref(tuple);
    return result;
}

static fixture_type_t fixture_type_chrarray_struct = {
        "chrarray",                // name
        NULL,                      // type (initialized at runtime)
        NULL,                      // falsy (initialized at runtime, maybe)
        &fixture_type_str_struct,  // pair

        new_rand_chrarray,  // _new_rand

        yp_chrarray,  // new_

        newN_chrarray,  // newN
        rand_objs_chr,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_chr_ordered,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_tuple(const rand_obj_supplier_memo_t *memo)
{
    if (memo->depth < 1 || RAND_OBJ_RETURN_FALSY()) {
        return yp_tuple_empty;
    } else {
        yp_ssize_t len = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(len, rand_obj_any_memo, memo);
        ypObject  *result = yp_tuple(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static fixture_type_t fixture_type_tuple_struct = {
        "tuple",                    // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_list_struct,  // pair

        new_rand_tuple,  // _new_rand

        yp_tuple,  // new_

        yp_tupleN,      // newN
        rand_objs_any,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_any_ordered,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_list(const rand_obj_supplier_memo_t *memo)
{
    if (memo->depth < 1 || RAND_OBJ_RETURN_FALSY()) {
        return yp_listN(0);
    } else {
        yp_ssize_t len = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(len, rand_obj_any_memo, memo);
        ypObject  *result = yp_list(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static fixture_type_t fixture_type_list_struct = {
        "list",                      // name
        NULL,                        // type (initialized at runtime)
        NULL,                        // falsy (initialized at runtime, maybe)
        &fixture_type_tuple_struct,  // pair

        new_rand_list,  // _new_rand

        yp_list,  // new_

        yp_listN,       // newN
        rand_objs_any,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_any_ordered,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        FALSE,  // hashable_items_only
};

static ypObject *new_rand_frozenset(const rand_obj_supplier_memo_t *memo)
{
    if (memo->depth < 1 || RAND_OBJ_RETURN_FALSY()) {
        return yp_frozenset_empty;
    } else {
        // n may not be the final length, as duplicates are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_hashable_memo, memo);
        ypObject  *result = yp_frozenset(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static fixture_type_t fixture_type_frozenset_struct = {
        "frozenset",               // name
        NULL,                      // type (initialized at runtime)
        NULL,                      // falsy (initialized at runtime, maybe)
        &fixture_type_set_struct,  // pair

        new_rand_frozenset,  // _new_rand

        yp_frozenset,  // new_

        yp_frozensetN,           // newN
        rand_objs_any_hashable,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

static ypObject *new_rand_set(const rand_obj_supplier_memo_t *memo)
{
    if (memo->depth < 1 || RAND_OBJ_RETURN_FALSY()) {
        return yp_setN(0);
    } else {
        // n may not be the final length, as duplicates are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_hashable_memo, memo);
        ypObject  *result = yp_set(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static fixture_type_t fixture_type_set_struct = {
        "set",                           // name
        NULL,                            // type (initialized at runtime)
        NULL,                            // falsy (initialized at runtime, maybe)
        &fixture_type_frozenset_struct,  // pair

        new_rand_set,  // _new_rand

        yp_set,  // new_

        yp_setN,                 // newN
        rand_objs_any_hashable,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

// Adds and discards a unique object from so such that it will contain a deleted entry.
static void make_set_dirty(ypObject *so)
{
    yp_uint8_t bytes[16];
    ypObject  *item;

    // We could use any object to make the set dirty: the type and value don't affect the final
    // result, only the hash of the object remains after removing it. 16 random bytes is unique
    // enough for UUIDs, so we'll use that. It's unlikely, but possible, that the hash could equal
    // an existing or future item in this set, and we don't know what order the entries in the set
    // will be visited, so coverage results for the tests could conceivably change run-to-run. Also,
    // the set will become clean again if resized, which would affect coverage.
    munit_rand_memory(16, bytes);
    assert_not_raises(item = yp_bytesC(16, bytes));  // new ref

    assert_not_raises_exc(yp_pushunique(so, item, &exc));
    assert_not_raises_exc(yp_remove(so, item, &exc));

    yp_decref(item);
}

static ypObject *new_frozenset_dirty(ypObject *x)
{
    ypObject *result = yp_set(x);  // new ref
    if (yp_isexceptionC(result)) return result;
    make_set_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozenset);
    return result;
}

static ypObject *new_rand_frozenset_dirty(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = new_rand_set(memo);  // new ref
    make_set_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozenset);
    return result;
}

static ypObject *new_frozenset_dirtyN(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(result = yp_setNV(n, args));  // new ref
    va_end(args);

    make_set_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozenset);
    return result;
}

// "Dirty" refers to the frozenset containing a deleted entry (i.e. ypSet_dummy).
static fixture_type_t fixture_type_frozenset_dirty_struct = {
        "frozenset_dirty",               // name
        NULL,                            // type (initialized at runtime)
        NULL,                            // falsy (initialized at runtime, maybe)
        &fixture_type_set_dirty_struct,  // pair

        new_rand_frozenset_dirty,  // _new_rand

        new_frozenset_dirty,  // new_

        new_frozenset_dirtyN,    // newN
        rand_objs_any_hashable,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

static ypObject *new_set_dirty(ypObject *x)
{
    ypObject *result = yp_set(x);  // new ref
    if (yp_isexceptionC(result)) return result;
    make_set_dirty(result);
    return result;
}

static ypObject *new_rand_set_dirty(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = new_rand_set(memo);  // new ref
    make_set_dirty(result);
    return result;
}

static ypObject *new_set_dirtyN(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    assert_not_raises(result = yp_setNV(n, args));  // new ref
    va_end(args);

    make_set_dirty(result);
    return result;
}

// "Dirty" refers to the set containing a deleted entry (i.e. ypSet_dummy).
static fixture_type_t fixture_type_set_dirty_struct = {
        "set_dirty",                           // name
        NULL,                                  // type (initialized at runtime)
        NULL,                                  // falsy (initialized at runtime, maybe)
        &fixture_type_frozenset_dirty_struct,  // pair

        new_rand_set_dirty,  // _new_rand

        new_set_dirty,  // new_

        new_set_dirtyN,          // newN
        rand_objs_any_hashable,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_setlike
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

static ypObject *new_rand_frozendict(const rand_obj_supplier_memo_t *memo)
{
    if (memo->depth < 1 || RAND_OBJ_RETURN_FALSY()) {
        return yp_frozendict_empty;
    } else {
        // n may not be the final length, as duplicate keys are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_keyvalue_memo, memo);
        ypObject  *result = yp_frozendict(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

// Used in newN for mapping types (frozendict and dict). Returns an object suitable for
// yp_frozendict/etc. Values are random and unequal from each other (but not necessarily unequal
// from the keys).
static ypObject *_mapping_new_supplierN(int n, va_list args)
{
    int           i;
    ypObject    **values;
    ypObject     *supplier;
    uniqueness_t *uq = uniqueness_new();

    if (n < 1) return yp_listN(0);

    values = malloc(sizeof(ypObject *) * (size_t)n);
    rand_objs_any(uq, n, values);

    assert_not_raises(supplier = yp_listN(0));  // new ref

    for (i = 0; i < n; i++) {
        ypObject *item;
        assert_not_raises(item = yp_tupleN(2, va_arg(args, ypObject *), values[i]));  // new ref
        assert_not_raises_exc(yp_append(supplier, item, &exc));
        yp_decref(item);
    }

    obj_array_decref2(n, values);
    free(values);
    uniqueness_dealloc(uq);

    return supplier;
}

static ypObject *new_frozendictN(int n, ...)
{
    va_list   args;
    ypObject *supplier;
    ypObject *result;

    if (n < 1) return yp_frozendict_empty;

    va_start(args, n);
    supplier = _mapping_new_supplierN(n, args);  // new ref
    va_end(args);

    assert_not_raises(result = yp_frozendict(supplier));  // new ref
    yp_decref(supplier);
    return result;
}

static fixture_type_t fixture_type_frozendict_struct = {
        "frozendict",               // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_dict_struct,  // pair

        new_rand_frozendict,  // _new_rand

        yp_frozendict,  // new_

        new_frozendictN,         // newN
        rand_objs_any_hashable,  // rand_items

        yp_frozendictK,  // newK
        rand_objs_any,   // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

static ypObject *new_rand_dict(const rand_obj_supplier_memo_t *memo)
{
    if (memo->depth < 1 || RAND_OBJ_RETURN_FALSY()) {
        return yp_dictK(0);
    } else {
        // n may not be the final length, as duplicate keys are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_keyvalue_memo, memo);
        ypObject  *result = yp_dict(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static ypObject *new_dictNV(int n, va_list args)
{
    ypObject *result;
    ypObject *supplier = _mapping_new_supplierN(n, args);  // new ref
    assert_not_raises(result = yp_dict(supplier));         // new ref
    yp_decref(supplier);
    return result;
}

static ypObject *new_dictN(int n, ...)
{
    va_list   args;
    ypObject *result;
    va_start(args, n);
    result = new_dictNV(n, args);  // new ref
    va_end(args);
    return result;
}

static fixture_type_t fixture_type_dict_struct = {
        "dict",                           // name
        NULL,                             // type (initialized at runtime)
        NULL,                             // falsy (initialized at runtime, maybe)
        &fixture_type_frozendict_struct,  // pair

        new_rand_dict,  // _new_rand

        yp_dict,  // new_

        new_dictN,               // newN
        rand_objs_any_hashable,  // rand_items

        yp_dictK,       // newK
        rand_objs_any,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

// Adds and discards a unique object from mp such that it will contain a deleted entry.
static void make_dict_dirty(ypObject *mp)
{
    yp_uint8_t bytes[16];
    ypObject  *item;

    // We could use any object to make the dict dirty: the type and value don't affect the final
    // result, only the hash of the object remains after removing it. 16 random bytes is unique
    // enough for UUIDs, so we'll use that. It's unlikely, but possible, that the hash could equal
    // an existing or future item in this dict, and we don't know what order the entries in the dict
    // will be visited, so coverage results for the tests could conceivably change run-to-run. Also,
    // the dict will become clean again if resized, which would affect coverage.
    munit_rand_memory(16, bytes);
    assert_not_raises(item = yp_bytesC(16, bytes));  // new ref

    assert_obj(item, not_in, mp);
    assert_not_raises_exc(yp_setitem(mp, item, item, &exc));
    assert_not_raises_exc(yp_delitem(mp, item, &exc));

    yp_decref(item);
}

static ypObject *new_frozendict_dirty(ypObject *x)
{
    ypObject *result = yp_dict(x);  // new ref
    if (yp_isexceptionC(result)) return result;
    make_dict_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozendict);
    return result;
}

static ypObject *new_rand_frozendict_dirty(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = new_rand_dict(memo);  // new ref
    make_dict_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozendict);
    return result;
}

static ypObject *new_frozendict_dirtyN(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = new_dictNV(n, args);  // new ref
    va_end(args);

    make_dict_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozendict);
    return result;
}

static ypObject *new_frozendict_dirtyK(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_dictKV(n, args);  // new ref
    va_end(args);

    make_dict_dirty(result);
    assert_not_raises_exc(yp_freeze(result, &exc));
    assert_type_is(result, yp_t_frozendict);
    return result;
}

static fixture_type_t fixture_type_frozendict_dirty_struct = {
        "frozendict_dirty",               // name
        NULL,                             // type (initialized at runtime)
        NULL,                             // falsy (initialized at runtime, maybe)
        &fixture_type_dict_dirty_struct,  // pair

        new_rand_frozendict_dirty,  // _new_rand

        new_frozendict_dirty,  // new_

        new_frozendict_dirtyN,   // newN
        rand_objs_any_hashable,  // rand_items

        new_frozendict_dirtyK,  // newK
        rand_objs_any,          // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

static ypObject *new_dict_dirty(ypObject *x)
{
    ypObject *result = yp_dict(x);  // new ref
    if (yp_isexceptionC(result)) return result;
    make_dict_dirty(result);
    return result;
}

static ypObject *new_rand_dict_dirty(const rand_obj_supplier_memo_t *memo)
{
    ypObject *result = new_rand_dict(memo);  // new ref
    make_dict_dirty(result);
    return result;
}

static ypObject *new_dict_dirtyN(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = new_dictNV(n, args);  // new ref
    va_end(args);

    make_dict_dirty(result);
    return result;
}

static ypObject *new_dict_dirtyK(int n, ...)
{
    va_list   args;
    ypObject *result;

    va_start(args, n);
    result = yp_dictKV(n, args);  // new ref
    va_end(args);

    make_dict_dirty(result);
    return result;
}

static fixture_type_t fixture_type_dict_dirty_struct = {
        "dict_dirty",                           // name
        NULL,                                   // type (initialized at runtime)
        NULL,                                   // falsy (initialized at runtime, maybe)
        &fixture_type_frozendict_dirty_struct,  // pair

        new_rand_dict_dirty,  // _new_rand

        new_dict_dirty,  // new_

        new_dict_dirtyN,         // newN
        rand_objs_any_hashable,  // rand_items

        new_dict_dirtyK,  // newK
        rand_objs_any,    // rand_values

        rand_objs_func_error,  // rand_ordered_items

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
        TRUE,   // original_object_return
        TRUE,   // hashable_items_only
};

static ypObject *new_rand_function_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return rand_obj_any(NULL);
}

// TODO Randomly return a statically-allocated function.
// TODO Randomize the parameters, default values, etc.
static ypObject *new_rand_function(const rand_obj_supplier_memo_t *memo)
{
    yp_parameter_decl_t parameter_decl[] = {{yp_s_star_args}, {yp_s_star_star_kwargs}};
    yp_function_decl_t  decl = {
            new_rand_function_code, 0, yp_lengthof_array(parameter_decl), parameter_decl};
    ypObject *result = yp_functionC(&decl);
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_function_struct = {
        "function",                     // name
        NULL,                           // type (initialized at runtime)
        NULL,                           // falsy (initialized at runtime, maybe)
        &fixture_type_function_struct,  // pair

        new_rand_function,  // _new_rand

        objobjfunc_error,  // new_

        objvarargfunc_error,   // newN
        rand_objs_func_error,  // rand_items

        objvarargfunc_error,   // newK
        rand_objs_func_error,  // rand_values

        rand_objs_func_error,  // rand_ordered_items

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_setlike
        FALSE,  // is_mapping
        TRUE,   // is_callable
        FALSE,  // is_patterned
        FALSE,  // original_object_return
        FALSE,  // hashable_items_only
};

fixture_type_t *fixture_type_type = &fixture_type_type_struct;
fixture_type_t *fixture_type_NoneType = &fixture_type_NoneType_struct;
fixture_type_t *fixture_type_bool = &fixture_type_bool_struct;
fixture_type_t *fixture_type_int = &fixture_type_int_struct;
fixture_type_t *fixture_type_intstore = &fixture_type_intstore_struct;
fixture_type_t *fixture_type_float = &fixture_type_float_struct;
fixture_type_t *fixture_type_floatstore = &fixture_type_floatstore_struct;
fixture_type_t *fixture_type_iter = &fixture_type_iter_struct;
fixture_type_t *fixture_type_range = &fixture_type_range_struct;
fixture_type_t *fixture_type_bytes = &fixture_type_bytes_struct;
fixture_type_t *fixture_type_bytearray = &fixture_type_bytearray_struct;
fixture_type_t *fixture_type_str = &fixture_type_str_struct;
fixture_type_t *fixture_type_chrarray = &fixture_type_chrarray_struct;
fixture_type_t *fixture_type_tuple = &fixture_type_tuple_struct;
fixture_type_t *fixture_type_list = &fixture_type_list_struct;
fixture_type_t *fixture_type_frozenset = &fixture_type_frozenset_struct;
fixture_type_t *fixture_type_set = &fixture_type_set_struct;
fixture_type_t *fixture_type_frozenset_dirty = &fixture_type_frozenset_dirty_struct;
fixture_type_t *fixture_type_set_dirty = &fixture_type_set_dirty_struct;
fixture_type_t *fixture_type_frozendict = &fixture_type_frozendict_struct;
fixture_type_t *fixture_type_dict = &fixture_type_dict_struct;
fixture_type_t *fixture_type_frozendict_dirty = &fixture_type_frozendict_dirty_struct;
fixture_type_t *fixture_type_dict_dirty = &fixture_type_dict_dirty_struct;
fixture_type_t *fixture_type_function = &fixture_type_function_struct;

#define FIXTURE_TYPES_ALL_LEN 24  // Verified in initialize_fixture_types.
static fixture_type_t *fixture_types_all_types[] = {&fixture_type_type_struct,
        &fixture_type_NoneType_struct, &fixture_type_bool_struct, &fixture_type_int_struct,
        &fixture_type_intstore_struct, &fixture_type_float_struct, &fixture_type_floatstore_struct,
        &fixture_type_iter_struct, &fixture_type_range_struct, &fixture_type_bytes_struct,
        &fixture_type_bytearray_struct, &fixture_type_str_struct, &fixture_type_chrarray_struct,
        &fixture_type_tuple_struct, &fixture_type_list_struct, &fixture_type_frozenset_struct,
        &fixture_type_set_struct, &fixture_type_frozenset_dirty_struct,
        &fixture_type_set_dirty_struct, &fixture_type_frozendict_struct, &fixture_type_dict_struct,
        &fixture_type_frozendict_dirty_struct, &fixture_type_dict_dirty_struct,
        &fixture_type_function_struct, NULL};
// param_values_types_all is populated in initialize_fixture_types.
static fixture_types_t fixture_types_all_struct = {FIXTURE_TYPES_ALL_LEN, fixture_types_all_types};
fixture_types_t       *fixture_types_all = &fixture_types_all_struct;
char                  *param_values_types_all[FIXTURE_TYPES_ALL_LEN + 1];

// Defines the type arrays (i.e. fixture_types_mutable and param_values_types_mutable). Also defines
// the filter functions used by FILL_FIXTURE_TYPES_ARRAYS to fill these type arrays. These are
// subsets of fixture_types_all, so will at most hold FIXTURE_TYPES_ALL_LEN elements.
#define DEFINE_FIXTURE_TYPES_ARRAYS(name)                                                     \
    static fixture_type_t *fixture_types_##name##_types[FIXTURE_TYPES_ALL_LEN + 1];           \
    static fixture_types_t fixture_types_##name##_struct = {0, fixture_types_##name##_types}; \
    fixture_types_t       *fixture_types_##name = &fixture_types_##name##_struct;             \
    char                  *param_values_types_##name[FIXTURE_TYPES_ALL_LEN + 1];
#define DEFINE_FIXTURE_TYPES(protocol, not_name)                                                \
    DEFINE_FIXTURE_TYPES_ARRAYS(protocol)                                                       \
    static int fixture_type_is_##protocol(fixture_type_t *type) { return type->is_##protocol; } \
    DEFINE_FIXTURE_TYPES_ARRAYS(not_name)                                                       \
    static int fixture_type_is_##not_name(fixture_type_t *type) { return !type->is_##protocol; }
DEFINE_FIXTURE_TYPES(mutable, immutable);
DEFINE_FIXTURE_TYPES(numeric, not_numeric);
DEFINE_FIXTURE_TYPES(iterable, not_iterable);
DEFINE_FIXTURE_TYPES(collection, not_collection);
DEFINE_FIXTURE_TYPES(sequence, not_sequence);
DEFINE_FIXTURE_TYPES(string, not_string);
DEFINE_FIXTURE_TYPES(setlike, not_setlike);
DEFINE_FIXTURE_TYPES(mapping, not_mapping);
#undef DEFINE_FIXTURE_TYPES
DEFINE_FIXTURE_TYPES_ARRAYS(immutable_not_str);
static int fixture_type_is_immutable_not_str(fixture_type_t *type)
{
    return !type->is_mutable && type != fixture_type_str;
}
DEFINE_FIXTURE_TYPES_ARRAYS(immutable_paired);
static int fixture_type_is_immutable_paired(fixture_type_t *type)
{
    return !type->is_mutable && type->pair != type;
}
#undef DEFINE_FIXTURE_TYPES_ARRAYS

static void fill_fixture_types_arrays(fixture_types_t *fixture_types, char **param_values_types,
        int (*is_of_type)(fixture_type_t *))
{
    fixture_type_t **type;
    for (type = fixture_types_all->types; *type != NULL; type++) {
        if (is_of_type(*type)) {
            fixture_types->types[fixture_types->len] = *type;
            param_values_types[fixture_types->len] = (*type)->name;
            fixture_types->len++;
        }
    }
    fixture_types->types[fixture_types->len] = NULL;
    param_values_types[fixture_types->len] = NULL;
}

static void initialize_fixture_types(void)
{
    // These need to be initialized at runtime because they may be imported from a DLL.
    fixture_type_type->type = yp_t_type;
    fixture_type_NoneType->type = yp_t_NoneType;
    fixture_type_NoneType->falsy = yp_None;
    fixture_type_bool->type = yp_t_bool;
    fixture_type_bool->falsy = yp_False;
    fixture_type_int->type = yp_t_int;
    fixture_type_int->falsy = yp_i_zero;
    fixture_type_intstore->type = yp_t_intstore;
    fixture_type_float->type = yp_t_float;
    // TODO Falsy immortal for float?
    fixture_type_floatstore->type = yp_t_floatstore;
    fixture_type_iter->type = yp_t_iter;
    fixture_type_range->type = yp_t_range;
    fixture_type_range->falsy = yp_range_empty;
    fixture_type_bytes->type = yp_t_bytes;
    fixture_type_bytes->falsy = yp_bytes_empty;
    fixture_type_bytearray->type = yp_t_bytearray;
    fixture_type_str->type = yp_t_str;
    fixture_type_str->falsy = yp_str_empty;
    fixture_type_chrarray->type = yp_t_chrarray;
    fixture_type_tuple->type = yp_t_tuple;
    fixture_type_tuple->falsy = yp_tuple_empty;
    fixture_type_list->type = yp_t_list;
    fixture_type_frozenset->type = yp_t_frozenset;
    fixture_type_frozenset->falsy = yp_frozenset_empty;
    fixture_type_set->type = yp_t_set;
    fixture_type_frozenset_dirty->type = yp_t_frozenset;
    fixture_type_frozenset_dirty->falsy = yp_frozenset_empty;
    fixture_type_set_dirty->type = yp_t_set;
    fixture_type_frozendict->type = yp_t_frozendict;
    fixture_type_frozendict->falsy = yp_frozendict_empty;
    fixture_type_dict->type = yp_t_dict;
    fixture_type_frozendict_dirty->type = yp_t_frozendict;
    fixture_type_frozendict_dirty->falsy = yp_frozendict_empty;
    fixture_type_dict_dirty->type = yp_t_dict;
    fixture_type_function->type = yp_t_function;

    // The fixture_types_* and param_values_types_* arrays above were sized based on
    // FIXTURE_TYPES_ALL_LEN, so make sure that value is correct.
    if (yp_lengthof_array(fixture_types_all_types) != FIXTURE_TYPES_ALL_LEN + 1) {
        fprintf(stderr, "Update FIXTURE_TYPES_ALL_LEN in unittest.h to %" PRIssize "\n",
                yp_lengthof_array(fixture_types_all_types) - 1);
        abort();
    }

    // Fill param_values_types_all. fixture_types_all was initialized statically.
    {
        fixture_type_t **type;
        char           **param_values = param_values_types_all;
        for (type = fixture_types_all->types; *type != NULL; type++) {
            *param_values = (*type)->name;
            param_values++;
        }
        *param_values = NULL;
    }

    // Fill the remaining fixture_types_* and param_values_types_* arrays.
#define FILL_FIXTURE_TYPES_ARRAYS(protocol) \
    fill_fixture_types_arrays(              \
            fixture_types_##protocol, param_values_types_##protocol, fixture_type_is_##protocol);
    FILL_FIXTURE_TYPES_ARRAYS(mutable);
    FILL_FIXTURE_TYPES_ARRAYS(numeric);
    FILL_FIXTURE_TYPES_ARRAYS(iterable);
    FILL_FIXTURE_TYPES_ARRAYS(collection);
    FILL_FIXTURE_TYPES_ARRAYS(sequence);
    FILL_FIXTURE_TYPES_ARRAYS(string);
    FILL_FIXTURE_TYPES_ARRAYS(setlike);
    FILL_FIXTURE_TYPES_ARRAYS(mapping);
    FILL_FIXTURE_TYPES_ARRAYS(immutable);
    FILL_FIXTURE_TYPES_ARRAYS(not_numeric);
    FILL_FIXTURE_TYPES_ARRAYS(not_iterable);
    FILL_FIXTURE_TYPES_ARRAYS(not_collection);
    FILL_FIXTURE_TYPES_ARRAYS(not_sequence);
    FILL_FIXTURE_TYPES_ARRAYS(not_string);
    FILL_FIXTURE_TYPES_ARRAYS(not_setlike);
    FILL_FIXTURE_TYPES_ARRAYS(not_mapping);
    FILL_FIXTURE_TYPES_ARRAYS(immutable_not_str);
    FILL_FIXTURE_TYPES_ARRAYS(immutable_paired);
#undef FILL_FIXTURE_TYPES_ARRAYS
}

// TODO We could speed this up with a frozendict or somesuch.
extern fixture_type_t *fixture_type_from_object(ypObject *object)
{
    fixture_type_t **fixture_type;
    ypObject        *type = yp_type(object);
    for (fixture_type = fixture_types_all->types; (*fixture_type) != NULL; fixture_type++) {
        if ((*fixture_type)->type == type) break;
    }
    yp_decref(type);
    assert_not_null(*fixture_type);
    return *fixture_type;
}

extern int object_is_hashable(ypObject *object)
{
    ypObject *exc = yp_None;
    (void)yp_hashC(object, &exc);
    if (yp_isexceptionC2(exc, yp_TypeError)) return FALSE;
    assert_not_exception(exc);  // unexpected exception
    return TRUE;
}


extern void obj_array_decref2(yp_ssize_t n, ypObject **array)
{
    yp_ssize_t i;
    if (n < 1) return;
    for (i = 0; i < n; i++) {
        if (array[i] != NULL) yp_decref(array[i]);
    }
    memset(array, 0, ((size_t)n) * sizeof(ypObject *));
}


extern yp_ssize_t yp_lenC_not_raises(ypObject *container)
{
    yp_ssize_t result;
    assert_not_raises_exc(result = yp_lenC(container, &exc));
    return result;
}

extern yp_int_t yp_asintC_not_raises(ypObject *number)
{
    yp_int_t result;
    assert_not_raises_exc(result = yp_asintC(number, &exc));
    return result;
}


#define MALLOC_TRACKER_MAX_LEN 4000

// TODO Not currently threadsafe
struct _malloc_tracker_t {
    // Simulate "out of memory" after this many allocations. Set to negative to disable.
    int        oom;
    yp_ssize_t len;
    void      *mallocs[MALLOC_TRACKER_MAX_LEN];
} malloc_tracker = {-1, 0};

static void malloc_tracker_fixture_setup(void)
{
    // Disable simulated OOM states unless the test specifically configures this.
    malloc_tracker.oom = -1;
    // We only track allocations made during the test, to verify they have been freed by the end.
    malloc_tracker.len = 0;
}

static void malloc_tracker_push(void *p)
{
    assert_not_null(p);  // NULL should be handled before push is called.

    // Increase the size of the mallocs array as necessary.
    assert_ssizeC(malloc_tracker.len, <, MALLOC_TRACKER_MAX_LEN);

    // Don't bother deduplicating; that should never happen!
    malloc_tracker.mallocs[malloc_tracker.len] = p;
    malloc_tracker.len++;
}

static void malloc_tracker_pop(void *p)
{
    yp_ssize_t i;
    assert_not_null(p);  // NULL should be handled before pop is called.

    // Find the pointer and set it to NULL. Ignore unknown pointers: we are only concerned with
    // allocations during the test.
    // TODO Report on deep pointers, i.e. that are not deallocated in reverse order.
    for (i = malloc_tracker.len - 1; i >= 0; i--) {
        if (malloc_tracker.mallocs[i] == p) {
            malloc_tracker.mallocs[i] = NULL;
            break;
        }
    }

    // Trim trailing NULL entries from the list.
    // TODO Report on long runs of NULLs, i.e. that are not deallocated in reverse order.
    while (malloc_tracker.len > 0 && malloc_tracker.mallocs[malloc_tracker.len - 1] == NULL) {
        malloc_tracker.len--;
    }
}

extern void malloc_tracker_oom_after(int successful)
{
    munit_assert_int(successful, >=, 0);
    malloc_tracker.oom = successful;
}

extern void malloc_tracker_oom_disable(void) { malloc_tracker.oom = -1; }

// Called before we actually allocate memory. Returns true iff we should simulate an OOM condition.
// Otherwise, updates the OOM counter, which will possibly cause the next allocation to fail.
static int malloc_tracker_oom_fail_alloc(void)
{
    if (malloc_tracker.oom == 0) return TRUE;
    if (malloc_tracker.oom > 0) malloc_tracker.oom--;
    return FALSE;
}

extern void *malloc_tracker_malloc(yp_ssize_t *actual, yp_ssize_t size)
{
    void *p;
    if (malloc_tracker_oom_fail_alloc()) return NULL;
    assert_not_null(p = yp_mem_default_malloc(actual, size));
    malloc_tracker_push(p);
    return p;
}

extern void *malloc_tracker_malloc_resize(
        yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra)
{
    void *newP;
    if (malloc_tracker_oom_fail_alloc()) return NULL;
    assert_not_null(newP = yp_mem_default_malloc_resize(actual, p, size, extra));
    if (newP != p) malloc_tracker_push(newP);
    return newP;
}

extern void malloc_tracker_free(void *p)
{
    yp_mem_default_free(p);
    if (p != NULL) malloc_tracker_pop(p);
}

static void malloc_tracker_fixture_tear_down(void)
{
    if (malloc_tracker.len > 0) {
        munit_errorf("memory leak: %p",  // GCOVR_EXCL_LINE
                malloc_tracker.mallocs[malloc_tracker.len - 1]);
    }
}


char param_key_type[] = "type";

static fixture_type_t *fixture_get_type(const MunitParameter params[])
{
    fixture_type_t **type;
    const char      *type_name = munit_parameters_get(params, param_key_type);
    if (type_name == NULL) return NULL;

    for (type = fixture_types_all->types; *type != NULL; type++) {
        if (strcmp((*type)->name, type_name) == 0) return *type;
    }

    munit_errorf("fixture_get_type: unknown type %s", type_name);  // GCOVR_EXCL_LINE
    return NULL;
}

extern fixture_t *fixture_setup(const MunitParameter params[], void *user_data)
{
    fixture_t *fixture = munit_new(fixture_t);

    fixture->type = fixture_get_type(params);

    malloc_tracker_fixture_setup();

    return fixture;
}

extern void fixture_tear_down(fixture_t *fixture)
{
    malloc_tracker_fixture_tear_down();

    free(fixture);
}


#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
static int crt_report_hook(int reportType, char *message, int *returnValue)
{
    munit_error(message);
    return TRUE;  // Unreachable.
}

// Disable the debugger pop-up on Windows, preferring instead to fail the test.
extern void disable_debugger_popups(void) { _CrtSetReportHook(crt_report_hook); }

#else
extern void disable_debugger_popups(void) {}
#endif


extern void unittest_initialize(void) { initialize_fixture_types(); }

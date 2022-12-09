
#include "munit_test/unittest.h"


// 1 in 100 are falsy FIXME make this a macro (5 in 100, etc)
#define RAND_OBJ_RETURN_FALSY() (munit_rand_int_range(0, 100 - 1) == 0)

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
static fixture_type_t fixture_type_frozendict_struct;
static fixture_type_t fixture_type_dict_struct;
static fixture_type_t fixture_type_function_struct;


static ypObject *objvoidfunc_error(void)
{
    munit_error("unsupported operation");
    return NULL;
}

static ypObject *objvarargfunc_error(int n, ...)
{
    munit_error("unsupported operation");
    return NULL;
}


// Returns a random yp_int_t value. Prioritizes zero and small numbers.
static yp_int_t rand_intC(void)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return 0;

    } else if (munit_rand_int_range(0, 10 - 1) == 0) {
        // 1 in 10 will be large. munit doesn't supply a munit_rand_uint64, so make our own.
        // FIXME Make a macro for "x in y" ratio
        return (yp_int_t)((((yp_uint64_t)munit_rand_uint32()) << 32u) | munit_rand_uint32());

    } else {
        // munit doesn't supply a munit_rand_int32 (signed), so make our own.
        // FIXME make this smaller?
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


typedef struct _rand_obj_supplier_memo_t {
    yp_ssize_t depth;          // The maximum depth of objects: 1 means only "terminal" objects.
    int        only_hashable;  // If true, only hashable objects are returned.
} rand_obj_supplier_memo_t;

// Returns a random object for a type that does not itself require a supplier.
static ypObject *rand_obj_terminal(void)
{
    // FIXME Initialize this using a property of the type, perhaps?
    static fixture_type_t *terminal_types[] = {&fixture_type_type_struct,
            &fixture_type_NoneType_struct, &fixture_type_bool_struct, &fixture_type_int_struct,
            &fixture_type_intstore_struct, &fixture_type_float_struct,
            &fixture_type_floatstore_struct, &fixture_type_range_struct, &fixture_type_bytes_struct,
            &fixture_type_bytearray_struct, &fixture_type_str_struct, &fixture_type_chrarray_struct,
            &fixture_type_function_struct};

    static const rand_obj_supplier_memo_t terminal_memo = {0};  // depth=0 is an error

    fixture_type_t *type = rand_choice_array(terminal_types);
    return type->_new_rand(&terminal_memo);
}

// Returns a random object for a hashable type that does not itself require a supplier.
static ypObject *rand_obj_terminal_hashable(void)
{
    // FIXME Initialize this using a property of the type, perhaps?
    static fixture_type_t *terminal_types[] = {&fixture_type_type_struct,
            &fixture_type_NoneType_struct, &fixture_type_bool_struct, &fixture_type_int_struct,
            &fixture_type_float_struct, &fixture_type_range_struct, &fixture_type_bytes_struct,
            &fixture_type_str_struct, &fixture_type_function_struct};

    static const rand_obj_supplier_memo_t terminal_memo = {0};  // depth=0 is an error

    fixture_type_t *type = rand_choice_array(terminal_types);
    return type->_new_rand(&terminal_memo);
}

// "Any" may be limited by memo (i.e. we might only return "terminal" types).
static yp_ssize_t fixture_types_immutable_len;
static ypObject  *rand_obj_any_hashable1(const rand_obj_supplier_memo_t *memo)
{
    assert_ssize(memo->depth, >, 0);
    if (memo->depth < 2) {
        return rand_obj_terminal_hashable();
    } else {
        rand_obj_supplier_memo_t sub_memo = {memo->depth - 1, /*only_hashable=*/TRUE};
        return rand_choice(fixture_types_immutable_len, fixture_types_immutable)
                ->_new_rand(&sub_memo);
    }
}

// "Any" may be limited by memo (i.e. we might only return hashable types).
static ypObject *rand_obj_any1(const rand_obj_supplier_memo_t *memo)
{
    assert_ssize(memo->depth, >, 0);
    if (memo->only_hashable) {
        return rand_obj_any_hashable1(memo);
    } else if (memo->depth < 2) {
        return rand_obj_terminal();
    } else {
        rand_obj_supplier_memo_t sub_memo = {memo->depth - 1, /*only_hashable=*/FALSE};
        return rand_choice(FIXTURE_TYPES_ALL_LEN, fixture_types_all)->_new_rand(&sub_memo);
    }
}

// Returns a 2-tuple of a hashable key and any value. Recall that "any" may be limited by memo.
static ypObject *rand_obj_any_keyvalue1(const rand_obj_supplier_memo_t *memo)
{
    ypObject *key = rand_obj_any_hashable1(memo);
    ypObject *value = rand_obj_any1(memo);
    ypObject *result = yp_tupleN(2, key, value);
    yp_decrefN(2, key, value);
    assert_not_exception(result);
    return result;
}

// TODO Interesting. 0 is a falsy byte, but '\x00' is not a falsy char.
static ypObject *rand_obj_byte(void) { return yp_intC(munit_rand_int_range(0, 255)); }

// TODO Return more than just latin-1 characters
static ypObject *rand_obj_chr(void) { return yp_chrC(munit_rand_int_range(0, 255)); }

ypObject *rand_obj_hashable(fixture_type_t *type)
{
    // Start with depth-1 as we are calling _new_rand ourselves.
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH - 1, /*only_hashable=*/TRUE};
    ypObject                *result = type->_new_rand(&memo);
    assert_false(type->is_mutable);
    return result;
}

ypObject *rand_obj(fixture_type_t *type)
{
    // Start with depth-1 as we are calling _new_rand ourselves.
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH - 1, /*only_hashable=*/FALSE};
    return type->_new_rand(&memo);
}

ypObject *rand_obj_any_hashable(void)
{
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH, /*only_hashable=*/TRUE};
    return rand_obj_any_hashable1(&memo);
}

ypObject *rand_obj_any(void)
{
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH, /*only_hashable=*/FALSE};
    return rand_obj_any1(&memo);
}


typedef struct _new_rand_iter_state {
    yp_ssize_t               n;
    rand_obj_supplier_t      supplier;
    rand_obj_supplier_memo_t supplier_memo;
} new_rand_iter_state;

static yp_state_decl_t new_rand_iter_state_decl = {yp_sizeof(new_rand_iter_state)};

static ypObject *new_rand_iter_func(ypObject *g, ypObject *value)
{
    new_rand_iter_state *state;
    yp_ssize_t           size;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, (void **)&state, &size));
    assert_ssize(size, ==, yp_sizeof(*state));

    if (state->n < 1) return yp_StopIteration;
    state->n--;
    return state->supplier(&state->supplier_memo);
}

static ypObject *new_rand_iter3(
        yp_ssize_t n, rand_obj_supplier_t supplier, const rand_obj_supplier_memo_t *supplier_memo)
{
    ypObject           *result;
    new_rand_iter_state state = {n, supplier};
    yp_generator_decl_t decl = {new_rand_iter_func, n, &state, &new_rand_iter_state_decl};
    state.supplier_memo = *supplier_memo;

    result = yp_generatorC(&decl);
    assert_not_exception(result);
    return result;
}


// Returns a random type object, except invalidated and exception objects.
static ypObject *new_rand_type(const rand_obj_supplier_memo_t *memo)
{
    return rand_choice(FIXTURE_TYPES_ALL_LEN, fixture_types_all)->type;
}

static fixture_type_t fixture_type_type_struct = {
        "type",                     // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_type_struct,  // pair

        new_rand_type,  // _new_rand

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        TRUE,   // is_callable
        FALSE,  // is_patterned
};

// There is only one NoneType object: yp_None.
static ypObject *new_rand_NoneType(const rand_obj_supplier_memo_t *memo) { return yp_None; }

static fixture_type_t fixture_type_NoneType_struct = {
        "NoneType",                     // name
        NULL,                           // type (initialized at runtime)
        NULL,                           // falsy (initialized at runtime, maybe)
        &fixture_type_NoneType_struct,  // pair

        new_rand_NoneType,  // _new_rand

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        TRUE,   // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        TRUE,   // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_iter(const rand_obj_supplier_memo_t *memo)
{
    yp_ssize_t n = munit_rand_int_range(0, 16);
    return new_rand_iter3(n, rand_obj_any1, memo);
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

        newN_iter,     // newN
        rand_obj_any,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

// TODO Ranges that cover more values, not just 32-bit-ish.
static ypObject *new_rand_range(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_range_empty;
    } else {
        ypObject *result;
        yp_int_t  start = (yp_int_t)((yp_int32_t)munit_rand_uint32());
        yp_int_t  len = (yp_int_t)munit_rand_int_range(1, 256);
        yp_int_t  step = (yp_int_t)munit_rand_int_range(-128, 128);
        if (step == 0) step = 1;  // This makes step=1 more likely.

        result = yp_rangeC3(start, start + (step * len), step);
        assert_not_exception(result);
        return result;
    }
}

// Because range's items must follow a pattern, there's a limit to the objects we can create with
// newN. We can construct a range between any two integers from rand_item_range, so start there.
// This allows newN to work with "any" items, so long as there are two or less.
// XXX Will fail the test if the two numbers are equal, as range only contains unique values.
static ypObject *newN_range(int n, ...)
{
    va_list   args;
    yp_int_t  first;
    yp_int_t  second;
    ypObject *result;

    va_start(args, n);
    if (n > 0) assert_not_raises_exc(first = yp_asintC(va_arg(args, ypObject *), &exc));
    if (n > 1) assert_not_raises_exc(second = yp_asintC(va_arg(args, ypObject *), &exc));
    va_end(args);

    if (n < 1) {
        result = yp_range_empty;
    } else if (n == 1) {
        result = yp_rangeC3(first, first + 1, 1);
    } else if (n == 2) {
        if (first < second) {
            result = yp_rangeC3(first, second + 1, second - first);
        } else if (first == second) {
            munit_errorf("newN_range called with same number twice (%" PRIint ")", first);
        } else {
            result = yp_rangeC3(first, second - 1, second - first);
        }
    } else {
        munit_errorf("newN_range unable to create range containing %d items", n);
    }

    assert_not_exception(result);
    return result;
}

static ypObject *rand_item_range(void)
{
    // Our current implementation has some limitations. step can't be yp_INT_T_MIN, and the length
    // can't be >ypObject_LEN_MAX (31 bits). So just play within a narrower area.
    //
    // There's also a limitation where we want newN_range to act like a sequence, but range does not
    // store duplicates. So keep the lowest byte incrementing to decrease the chance of a collision.
    static yp_int_t low_byte = 0;
    yp_int_t        value = (munit_rand_int_range(-0x3FFFFF, 0x3FFFFF) << 8) | low_byte;
    ypObject       *result = yp_intC(value);
    low_byte = (low_byte + 1) & 0xFF;
    assert_not_exception(result);
    return result;
}

static fixture_type_t fixture_type_range_struct = {
        "range",                     // name
        NULL,                        // type (initialized at runtime)
        NULL,                        // falsy (initialized at runtime, maybe)
        &fixture_type_range_struct,  // pair

        new_rand_range,  // _new_rand

        newN_range,       // newN
        rand_item_range,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_patterned
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
        result = yp_bytesC(source, len);  // FIXME I gotta flip these arguments around!
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

        newN_bytes,     // newN
        rand_obj_byte,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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
        result = yp_bytearrayC(source, len);  // FIXME I gotta flip these arguments around!
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

        newN_bytearray,  // newN
        rand_obj_byte,   // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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
        // FIXME I gotta flip these arguments around!
        result = yp_str_frombytesC4(source, len, yp_s_utf_8, yp_s_strict);
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
    // FIXME Support any iterable with str_concat.
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

        newN_str,      // newN
        rand_obj_chr,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
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
        // FIXME I gotta flip these arguments around!
        result = yp_chrarray_frombytesC4(source, len, yp_s_utf_8, yp_s_strict);
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

        newN_chrarray,  // newN
        rand_obj_chr,   // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_tuple(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_tuple_empty;
    } else {
        yp_ssize_t len = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(len, rand_obj_any1, memo);
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

        yp_tupleN,     // newN
        rand_obj_any,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_list(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_listN(0);
    } else {
        yp_ssize_t len = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(len, rand_obj_any1, memo);
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

        yp_listN,      // newN
        rand_obj_any,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_frozenset(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_frozenset_empty;
    } else {
        // n may not be the final length, as duplicates are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_hashable1, memo);
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

        yp_frozensetN,          // newN
        rand_obj_any_hashable,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_set(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_setN(0);
    } else {
        // n may not be the final length, as duplicates are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_hashable1, memo);
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

        yp_setN,                // newN
        rand_obj_any_hashable,  // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_frozendict(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_frozendict_empty;
    } else {
        // n may not be the final length, as duplicate keys are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_keyvalue1, memo);
        ypObject  *result = yp_frozendict(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static fixture_type_t fixture_type_frozendict_struct = {
        "frozendict",               // name
        NULL,                       // type (initialized at runtime)
        NULL,                       // falsy (initialized at runtime, maybe)
        &fixture_type_dict_struct,  // pair

        new_rand_frozendict,  // _new_rand

        objvarargfunc_error,  // newN
        rand_obj_any,         // rand_item

        yp_frozendictK,         // newK
        rand_obj_any_hashable,  // rand_key
        rand_obj_any,           // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_dict(const rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_dictK(0);
    } else {
        // n may not be the final length, as duplicate keys are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = new_rand_iter3(n, rand_obj_any_keyvalue1, memo);
        ypObject  *result = yp_dict(iter);
        yp_decref(iter);
        assert_not_exception(result);
        return result;
    }
}

static fixture_type_t fixture_type_dict_struct = {
        "dict",                           // name
        NULL,                             // type (initialized at runtime)
        NULL,                             // falsy (initialized at runtime, maybe)
        &fixture_type_frozendict_struct,  // pair

        new_rand_dict,  // _new_rand

        objvarargfunc_error,  // newN
        rand_obj_any,         // rand_item

        yp_dictK,               // newK
        rand_obj_any_hashable,  // rand_key
        rand_obj_any,           // rand_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_patterned
};

static ypObject *new_rand_function_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return rand_obj_any();
}

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

        objvarargfunc_error,  // newN
        objvoidfunc_error,    // rand_item

        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_key
        objvoidfunc_error,    // rand_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        TRUE,   // is_callable
        FALSE,  // is_patterned
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
fixture_type_t *fixture_type_frozendict = &fixture_type_frozendict_struct;
fixture_type_t *fixture_type_dict = &fixture_type_dict_struct;
fixture_type_t *fixture_type_function = &fixture_type_function_struct;

fixture_type_t *fixture_types_all[] = {&fixture_type_type_struct, &fixture_type_NoneType_struct,
        &fixture_type_bool_struct, &fixture_type_int_struct, &fixture_type_intstore_struct,
        &fixture_type_float_struct, &fixture_type_floatstore_struct, &fixture_type_iter_struct,
        &fixture_type_range_struct, &fixture_type_bytes_struct, &fixture_type_bytearray_struct,
        &fixture_type_str_struct, &fixture_type_chrarray_struct, &fixture_type_tuple_struct,
        &fixture_type_list_struct, &fixture_type_frozenset_struct, &fixture_type_set_struct,
        &fixture_type_frozendict_struct, &fixture_type_dict_struct, &fixture_type_function_struct,
        NULL};

STATIC_ASSERT(yp_lengthof_array(fixture_types_all) == FIXTURE_TYPES_ALL_LEN + 1,
        lengthof_fixture_types_all);

// These are subsets of fixture_types_all, so will at most hold that many elements.
fixture_type_t *fixture_types_immutable[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_numeric[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_iterable[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_collection[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_sequence[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_string[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_set[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_mapping[FIXTURE_TYPES_ALL_LEN + 1];

static yp_ssize_t fixture_types_immutable_len = 0;  // Incremented later

// Once again, subsets of fixture_types_all.
char *param_values_types_all[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_numeric[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_iterable[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_collection[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_sequence[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_string[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_set[FIXTURE_TYPES_ALL_LEN + 1];
char *param_values_types_mapping[FIXTURE_TYPES_ALL_LEN + 1];

// The given arrays must be no smaller than fixture_types_all.
static void fill_type_arrays(fixture_type_t **fixture_array, char **param_array, yp_ssize_t offset)
{
    fixture_type_t **types;
    for (types = fixture_types_all; *types != NULL; types++) {
        if (*((int *)(((yp_uint8_t *)*types) + offset))) {
            *fixture_array = *types;
            fixture_array++;
            *param_array = (*types)->name;
            param_array++;
        }
    }
    *fixture_array = NULL;
    *param_array = NULL;
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
    fixture_type_frozendict->type = yp_t_frozendict;
    fixture_type_frozendict->falsy = yp_frozendict_empty;
    fixture_type_dict->type = yp_t_dict;
    fixture_type_function->type = yp_t_function;

    {
        fixture_type_t **types;
        fixture_type_t **immutables = fixture_types_immutable;
        char           **param_values = param_values_types_all;
        for (types = fixture_types_all; *types != NULL; types++) {
            *param_values = (*types)->name;
            param_values++;
            if (!(*types)->is_mutable) {
                *immutables = *types;
                immutables++;
                fixture_types_immutable_len++;
            }
        }
        *immutables = NULL;
        *param_values = NULL;
    }

#define FILL_TYPE_ARRAYS(protocol)                                            \
    fill_type_arrays(fixture_types_##protocol, param_values_types_##protocol, \
            yp_offsetof(fixture_type_t, is_##protocol));
    FILL_TYPE_ARRAYS(numeric);
    FILL_TYPE_ARRAYS(iterable);
    FILL_TYPE_ARRAYS(collection);
    FILL_TYPE_ARRAYS(sequence);
    FILL_TYPE_ARRAYS(string);
    FILL_TYPE_ARRAYS(set);
    FILL_TYPE_ARRAYS(mapping);
#undef FILL_TYPE_ARRAYS
}


yp_IMMORTAL_INT(int_neg_5, -5);
yp_IMMORTAL_INT(int_neg_4, -4);
yp_IMMORTAL_INT(int_neg_3, -3);
yp_IMMORTAL_INT(int_neg_2, -2);
yp_IMMORTAL_INT(int_neg_1, -1);
yp_IMMORTAL_INT(int_0, 0);
yp_IMMORTAL_INT(int_1, 1);
yp_IMMORTAL_INT(int_2, 2);
yp_IMMORTAL_INT(int_3, 3);
yp_IMMORTAL_INT(int_4, 4);
yp_IMMORTAL_INT(int_5, 5);


#define MALLOC_TRACKER_MAX_LEN 1000

// TODO Not currently threadsafe
struct _malloc_tracker_t {
    yp_ssize_t len;
    void      *mallocs[MALLOC_TRACKER_MAX_LEN];
} malloc_tracker = {0};

static void malloc_tracker_fixture_setup(void)
{
    // We only track allocations made during the test, to verify they have been freed by the end.
    malloc_tracker.len = 0;
}

static void malloc_tracker_push(void *p)
{
    assert_not_null(p);  // NULL should be handled before push is called.

    // Increase the size of the mallocs array as necessary.
    assert_ssize(malloc_tracker.len, <, MALLOC_TRACKER_MAX_LEN);

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
    for (i = malloc_tracker.len - 1; i >= 0; i--) {
        if (malloc_tracker.mallocs[i] == p) {
            malloc_tracker.mallocs[i] = NULL;
            break;
        }
    }

    // Trim trailing NULL entries from the list.
    while (malloc_tracker.len > 0 && malloc_tracker.mallocs[malloc_tracker.len - 1] == NULL) {
        malloc_tracker.len--;
    }
}

void *malloc_tracker_malloc(yp_ssize_t *actual, yp_ssize_t size)
{
    void *p = yp_mem_default_malloc(actual, size);
    if (p != NULL) malloc_tracker_push(p);
    return p;
}

void *malloc_tracker_malloc_resize(yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra)
{
    void *newP = yp_mem_default_malloc_resize(actual, p, size, extra);
    if (newP != NULL && newP != p) malloc_tracker_push(newP);
    return newP;
}

void malloc_tracker_free(void *p)
{
    yp_mem_default_free(p);
    if (p != NULL) malloc_tracker_pop(p);
}

static void malloc_tracker_fixture_tear_down(void)
{
    if (malloc_tracker.len > 0) {
        munit_errorf("memory leak: %p", malloc_tracker.mallocs[malloc_tracker.len - 1]);
    }
}


char param_key_type[] = "type";

static fixture_type_t *fixture_get_type(const MunitParameter params[])
{
    fixture_type_t **type;
    const char      *type_name = munit_parameters_get(params, param_key_type);
    if (type_name == NULL) return NULL;

    for (type = fixture_types_all; *type != NULL; type++) {
        if (strcmp((*type)->name, type_name) == 0) return *type;
    }

    munit_errorf("fixture_get_type: unknown type %s", type_name);
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


#include "munit_test/unittest.h"


// 1 in 100 are falsy FIXME make this a macro (5 in 100, etc)
#define RAND_OBJ_RETURN_FALSY() (munit_rand_int_range(0, 100 - 1) == 0)

#define RAND_OBJ_DEFAULT_DEPTH (3)


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

static void voidobjpobjpfunc_error(ypObject **key, ypObject **value)
{
    munit_error("unsupported operation");
    *key = *value = NULL;
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
#define rand_choice(len, array) ((array)[munit_rand_int_range(0, (len)-1)])

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
    static fixture_type_t          *terminal_types[] = {&fixture_type_type, &fixture_type_NoneType,
                     &fixture_type_bool, &fixture_type_int, &fixture_type_intstore, &fixture_type_float,
                     &fixture_type_floatstore, &fixture_type_range, &fixture_type_bytes,
                     &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray,
                     &fixture_type_function};
    static rand_obj_supplier_memo_t terminal_memo = {0};  // depth=0 is an error

    fixture_type_t *type = rand_choice_array(terminal_types);
    return type->new_rand(&terminal_memo);
}

// Returns a random object for a hashable type that does not itself require a supplier.
static ypObject *rand_obj_terminal_hashable(void)
{
    // FIXME Initialize this using a property of the type, perhaps?
    static fixture_type_t          *terminal_types[] = {&fixture_type_type, &fixture_type_NoneType,
                     &fixture_type_bool, &fixture_type_int, &fixture_type_float, &fixture_type_range,
                     &fixture_type_bytes, &fixture_type_str, &fixture_type_function};
    static rand_obj_supplier_memo_t terminal_memo = {0};  // depth=0 is an error

    fixture_type_t *type = rand_choice_array(terminal_types);
    return type->new_rand(&terminal_memo);
}

// "Any" may be limited by memo (i.e. we might only return "terminal" types).
static yp_ssize_t fixture_types_immutable_len;
static ypObject  *rand_obj_any_hashable1(rand_obj_supplier_memo_t *memo)
{
    assert_ssize(memo->depth, >, 0);
    if (memo->depth < 2) {
        return rand_obj_terminal_hashable();
    } else {
        rand_obj_supplier_memo_t sub_memo = {memo->depth - 1, /*only_hashable=*/TRUE};
        return rand_choice(fixture_types_immutable_len, fixture_types_immutable)
                ->new_rand(&sub_memo);
    }
}

// "Any" may be limited by memo (i.e. we might only return hashable types).
static ypObject *rand_obj_any1(rand_obj_supplier_memo_t *memo)
{
    assert_ssize(memo->depth, >, 0);
    if (memo->only_hashable) {
        return rand_obj_any_hashable1(memo);
    } else if (memo->depth < 2) {
        return rand_obj_terminal();
    } else {
        rand_obj_supplier_memo_t sub_memo = {memo->depth - 1, /*only_hashable=*/FALSE};
        return rand_choice(FIXTURE_TYPES_ALL_LEN, fixture_types_all)->new_rand(&sub_memo);
    }
}

// Returns a 2-tuple of a hashable key and any value. Recall that "any" may be limited by memo.
static ypObject *rand_obj_any_keyvalue1(rand_obj_supplier_memo_t *memo)
{
    ypObject *key = rand_obj_any_hashable1(memo);
    ypObject *value = rand_obj_any1(memo);
    ypObject *result = yp_tupleN(2, key, value);
    yp_decrefN(2, key, value);
    assert_not_exception(result);
    return result;
}

ypObject *rand_obj_hashable(fixture_type_t *type)
{
    // Start with depth-1 as we are calling new_rand ourselves.
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH - 1, /*only_hashable=*/TRUE};
    ypObject                *result = type->new_rand(&memo);
    assert_false(type->is_mutable);
    return result;
}

ypObject *rand_obj(fixture_type_t *type)
{
    // Start with depth-1 as we are calling new_rand ourselves.
    rand_obj_supplier_memo_t memo = {RAND_OBJ_DEFAULT_DEPTH - 1, /*only_hashable=*/FALSE};
    return type->new_rand(&memo);
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


typedef struct _rand_obj_iter_state {
    yp_ssize_t          n;
    rand_obj_supplier_t supplier;
    void               *supplier_memo;
} rand_obj_iter_state;

static yp_state_decl_t rand_obj_iter_state_decl = {yp_sizeof(rand_obj_iter_state)};

static ypObject *rand_obj_iter_func(ypObject *g, ypObject *value)
{
    rand_obj_iter_state *state;
    yp_ssize_t           size;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, (void **)&state, &size));
    assert_ssize(size, ==, yp_sizeof(*state));

    if (state->n < 1) return yp_StopIteration;
    state->n--;
    return state->supplier(state->supplier_memo);
}

static ypObject *rand_obj_iter3(yp_ssize_t n, rand_obj_supplier_t supplier, void *supplier_memo)
{
    rand_obj_iter_state state = {n, supplier, supplier_memo};
    yp_generator_decl_t decl = {rand_obj_iter_func, n, &state, &rand_obj_iter_state_decl};
    ypObject           *result = yp_generatorC(&decl);
    assert_not_exception(result);
    return result;
}


// Returns a random type object, except invalidated and exception objects.
static ypObject *rand_obj_type(rand_obj_supplier_memo_t *memo)
{
    return rand_choice(FIXTURE_TYPES_ALL_LEN, fixture_types_all)->type;
}

fixture_type_t fixture_type_type = {
        "type",              // name
        NULL,                // type (initialized at runtime)
        &fixture_type_type,  // pair

        rand_obj_type,        // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        TRUE,   // is_callable
        FALSE,  // is_insertion_ordered
};

// There is only one NoneType object: yp_None.
static ypObject *rand_obj_NoneType(rand_obj_supplier_memo_t *memo) { return yp_None; }

fixture_type_t fixture_type_NoneType = {
        "NoneType",              // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_NoneType,  // pair

        rand_obj_NoneType,    // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_bool(rand_obj_supplier_memo_t *memo)
{
    if (munit_rand_int_range(0, 1)) {
        return yp_True;
    } else {
        return yp_False;
    }
}

fixture_type_t fixture_type_bool = {
        "bool",              // name
        NULL,                // type (initialized at runtime)
        &fixture_type_bool,  // pair

        rand_obj_bool,        // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_int(rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_intC(rand_intC());
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_int = {
        "int",                   // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_intstore,  // pair

        rand_obj_int,         // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_intstore(rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_intstoreC(rand_intC());
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_intstore = {
        "intstore",         // name
        NULL,               // type (initialized at runtime)
        &fixture_type_int,  // pair

        rand_obj_intstore,    // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_float(rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_floatCF(rand_floatCF());
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_float = {
        "float",                   // name
        NULL,                      // type (initialized at runtime)
        &fixture_type_floatstore,  // pair

        rand_obj_float,       // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_floatstore(rand_obj_supplier_memo_t *memo)
{
    ypObject *result = yp_floatstoreCF(rand_floatCF());
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_floatstore = {
        "floatstore",         // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_float,  // pair

        rand_obj_floatstore,  // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        TRUE,   // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_iter(rand_obj_supplier_memo_t *memo)
{
    yp_ssize_t n = munit_rand_int_range(0, 16);
    return rand_obj_iter3(n, rand_obj_any1, memo);
}

fixture_type_t fixture_type_iter = {
        "iter",              // name
        NULL,                // type (initialized at runtime)
        &fixture_type_iter,  // pair

        rand_obj_iter,        // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

// TODO Ranges that cover more values, not just 32-bit-ish.
static ypObject *rand_obj_range(rand_obj_supplier_memo_t *memo)
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

fixture_type_t fixture_type_range = {
        "range",              // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_range,  // pair

        rand_obj_range,       // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_bytes(rand_obj_supplier_memo_t *memo)
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

fixture_type_t fixture_type_bytes = {
        "bytes",                  // name
        NULL,                     // type (initialized at runtime)
        &fixture_type_bytearray,  // pair

        rand_obj_bytes,       // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_insertion_ordered
};

static ypObject *rand_obj_bytearray(rand_obj_supplier_memo_t *memo)
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

fixture_type_t fixture_type_bytearray = {
        "bytearray",          // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_bytes,  // pair

        rand_obj_bytearray,   // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_insertion_ordered
};

// TODO Return larger characters than just ascii.
static ypObject *rand_obj_str(rand_obj_supplier_memo_t *memo)
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

fixture_type_t fixture_type_str = {
        "str",                   // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_chrarray,  // pair

        rand_obj_str,         // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_insertion_ordered
};

// TODO Return larger characters than just ascii.
static ypObject *rand_obj_chrarray(rand_obj_supplier_memo_t *memo)
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

fixture_type_t fixture_type_chrarray = {
        "chrarray",         // name
        NULL,               // type (initialized at runtime)
        &fixture_type_str,  // pair

        rand_obj_chrarray,    // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        TRUE,   // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_insertion_ordered
};

static ypObject *rand_obj_tuple(rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_tuple_empty;
    } else {
        yp_ssize_t len = munit_rand_int_range(1, 16);
        ypObject  *iter = rand_obj_iter3(len, rand_obj_any1, memo);
        ypObject  *result = yp_tuple(iter);
        assert_not_exception(result);
        yp_decref(iter);
        return result;
    }
}

fixture_type_t fixture_type_tuple = {
        "tuple",             // name
        NULL,                // type (initialized at runtime)
        &fixture_type_list,  // pair

        rand_obj_tuple,       // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        rand_obj_any,            // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_insertion_ordered
};

static ypObject *rand_obj_list(rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_listN(0);
    } else {
        yp_ssize_t len = munit_rand_int_range(1, 16);
        ypObject  *iter = rand_obj_iter3(len, rand_obj_any1, memo);
        ypObject  *result = yp_list(iter);
        assert_not_exception(result);
        yp_decref(iter);
        return result;
    }
}

fixture_type_t fixture_type_list = {
        "list",               // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_tuple,  // pair

        rand_obj_list,        // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        rand_obj_any,            // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        TRUE,   // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        TRUE,   // is_insertion_ordered
};

static ypObject *rand_obj_frozenset(rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_frozenset_empty;
    } else {
        // n may not be the final length, as duplicates are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = rand_obj_iter3(n, rand_obj_any_hashable1, memo);
        ypObject  *result = yp_frozenset(iter);
        assert_not_exception(result);
        yp_decref(iter);
        return result;
    }
}

fixture_type_t fixture_type_frozenset = {
        "frozenset",        // name
        NULL,               // type (initialized at runtime)
        &fixture_type_set,  // pair

        rand_obj_frozenset,   // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_set(rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_setN(0);
    } else {
        // n may not be the final length, as duplicates are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = rand_obj_iter3(n, rand_obj_any_hashable1, memo);
        ypObject  *result = yp_set(iter);
        assert_not_exception(result);
        yp_decref(iter);
        return result;
    }
}

fixture_type_t fixture_type_set = {
        "set",                    // name
        NULL,                     // type (initialized at runtime)
        &fixture_type_frozenset,  // pair

        rand_obj_set,         // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        rand_obj_any_hashable,   // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        TRUE,   // is_set
        FALSE,  // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_frozendict(rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_frozendict_empty;
    } else {
        // n may not be the final length, as duplicate keys are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = rand_obj_iter3(n, rand_obj_any_keyvalue1, memo);
        ypObject  *result = yp_frozendict(iter);
        assert_not_exception(result);
        yp_decref(iter);
        return result;
    }
}

fixture_type_t fixture_type_frozendict = {
        "frozendict",        // name
        NULL,                // type (initialized at runtime)
        &fixture_type_dict,  // pair

        rand_obj_frozendict,  // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        rand_obj_any,            // rand_item  // FIXME maybe this one returns 2-tuples?
        voidobjpobjpfunc_error,  // rand_key_value  // FIXME ...and this one...remove?

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_dict(rand_obj_supplier_memo_t *memo)
{
    if (RAND_OBJ_RETURN_FALSY()) {
        return yp_dictK(0);
    } else {
        // n may not be the final length, as duplicate keys are discarded.
        yp_ssize_t n = munit_rand_int_range(1, 16);
        ypObject  *iter = rand_obj_iter3(n, rand_obj_any_keyvalue1, memo);
        ypObject  *result = yp_dict(iter);
        assert_not_exception(result);
        yp_decref(iter);
        return result;
    }
}

fixture_type_t fixture_type_dict = {
        "dict",                    // name
        NULL,                      // type (initialized at runtime)
        &fixture_type_frozendict,  // pair

        rand_obj_dict,        // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        rand_obj_any,            // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        TRUE,   // is_mutable
        FALSE,  // is_numeric
        TRUE,   // is_iterable
        TRUE,   // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        TRUE,   // is_mapping
        FALSE,  // is_callable
        FALSE,  // is_insertion_ordered
};

static ypObject *rand_obj_function_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return rand_obj_any();
}

static ypObject *rand_obj_function(rand_obj_supplier_memo_t *memo)
{
    yp_parameter_decl_t parameter_decl[] = {{yp_s_star_args}, {yp_s_star_star_kwargs}};
    yp_function_decl_t  decl = {
             rand_obj_function_code, 0, yp_lengthof_array(parameter_decl), parameter_decl};
    ypObject *result = yp_functionC(&decl);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_function = {
        "function",              // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_function,  // pair

        rand_obj_function,    // new_rand
        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK

        objvoidfunc_error,       // rand_item
        voidobjpobjpfunc_error,  // rand_key_value

        FALSE,  // is_mutable
        FALSE,  // is_numeric
        FALSE,  // is_iterable
        FALSE,  // is_collection
        FALSE,  // is_sequence
        FALSE,  // is_string
        FALSE,  // is_set
        FALSE,  // is_mapping
        TRUE,   // is_callable
        FALSE,  // is_insertion_ordered
};

fixture_type_t *fixture_types_all[] = {&fixture_type_type, &fixture_type_NoneType,
        &fixture_type_bool, &fixture_type_int, &fixture_type_intstore, &fixture_type_float,
        &fixture_type_floatstore, &fixture_type_iter, &fixture_type_range, &fixture_type_bytes,
        &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray, &fixture_type_tuple,
        &fixture_type_list, &fixture_type_frozenset, &fixture_type_set, &fixture_type_frozendict,
        &fixture_type_dict, &fixture_type_function, NULL};

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
    fixture_type_type.type = yp_t_type;
    fixture_type_NoneType.type = yp_t_NoneType;
    fixture_type_bool.type = yp_t_bool;
    fixture_type_int.type = yp_t_int;
    fixture_type_intstore.type = yp_t_intstore;
    fixture_type_float.type = yp_t_float;
    fixture_type_floatstore.type = yp_t_floatstore;
    fixture_type_iter.type = yp_t_iter;
    fixture_type_range.type = yp_t_range;
    fixture_type_bytes.type = yp_t_bytes;
    fixture_type_bytearray.type = yp_t_bytearray;
    fixture_type_str.type = yp_t_str;
    fixture_type_chrarray.type = yp_t_chrarray;
    fixture_type_tuple.type = yp_t_tuple;
    fixture_type_list.type = yp_t_list;
    fixture_type_frozenset.type = yp_t_frozenset;
    fixture_type_set.type = yp_t_set;
    fixture_type_frozendict.type = yp_t_frozendict;
    fixture_type_dict.type = yp_t_dict;
    fixture_type_function.type = yp_t_function;

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

    return fixture;
}

extern void fixture_tear_down(fixture_t *fixture) { free(fixture); }


extern void unittest_initialize(void) { initialize_fixture_types(); }

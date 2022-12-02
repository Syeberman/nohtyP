
#include "munit_test/unittest.h"


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


// Returns a random yp_int_t value. Large (>32 bit) values are less likely; zero even less likely.
static yp_int_t rand_intC(void)
{
    int large = munit_rand_int_range(0, 10 - 1);  // 1 in 10 will be large.

    if (large) {
        // munit doesn't supply a munit_rand_uint64, so make our own.
        return (yp_int_t)((((yp_uint64_t)munit_rand_uint32()) << 32u) | munit_rand_uint32());
    } else {
        // munit doesn't supply a munit_rand_int32 (signed), so make our own.
        return (yp_int_t)((yp_int32_t)munit_rand_uint32());
    }
}

// TODO Make large/long values less likely?
static yp_float_t rand_floatCF(void) { return munit_rand_double(); }

// Populates source with len random ascii bytes.
static void rand_ascii(yp_ssize_t len, yp_uint8_t *source)
{
    yp_ssize_t i;
    for (i = 0; i < len; i++) {
        source[i] = (yp_uint8_t)munit_rand_int_range(0, 0x7F);
    }
}

// Returns either a falsy or truthy object of type. Falsy objects are less likely.
static ypObject *rand_obj_of(fixture_type_t *type)
{
    int truthy = munit_rand_int_range(0, 10 - 1);  // 1 in 10 will be falsy.

    // NoneType has no truthy values; type has no falsy values.
    if (truthy && type->rand_truthy != objvoidfunc_error) {
        return type->rand_truthy();
    } else if (type->rand_falsy != objvoidfunc_error) {
        return type->rand_falsy();
    } else {
        return type->rand_truthy();
    }
}

static ypObject *rand_obj_any_immutable(void)
{
    int             index = munit_rand_int_range(0, FIXTURE_TYPES_ALL_LEN);
    fixture_type_t *type = fixture_types_all[index];
    // This makes immutables that are part of a pair more likely to be chosen.
    // TODO If we had a table of immutables, this wouldn't be an issue.
    if (type->is_mutable) type = type->pair;
    assert_false(type->is_mutable);
    return rand_obj_of(type);
}

static ypObject *rand_obj_any(void)
{
    // FIXME Ensure max is inclusive.
    int index = munit_rand_int_range(0, FIXTURE_TYPES_ALL_LEN);
    return rand_obj_of(fixture_types_all[index]);
}


// Returns a random type object, except invalidated and exception objects.
static ypObject *rand_obj_type(void)
{
    int index = munit_rand_int_range(0, FIXTURE_TYPES_ALL_LEN);
    return fixture_types_all[index]->type;
}

fixture_type_t fixture_type_type = {
        "type",              // name
        NULL,                // type (initialized at runtime)
        &fixture_type_type,  // pair

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy (all type objects are truthy)
        rand_obj_type,        // rand_truthy

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
static ypObject *rand_obj_NoneType(void) { return yp_None; }

fixture_type_t fixture_type_NoneType = {
        "NoneType",              // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_NoneType,  // pair

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        rand_obj_NoneType,    // rand_falsy
        objvoidfunc_error,    // rand_truthy (recall yp_None is falsy)

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

// There is only one falsy bool: yp_False.
static ypObject *rand_obj_bool_falsy(void) { return yp_False; }

// There is only one truthy bool: yp_True.
static ypObject *rand_obj_bool_truthy(void) { return yp_True; }

fixture_type_t fixture_type_bool = {
        "bool",              // name
        NULL,                // type (initialized at runtime)
        &fixture_type_bool,  // pair

        objvarargfunc_error,   // newN
        objvarargfunc_error,   // newK
        rand_obj_bool_falsy,   // rand_falsy
        rand_obj_bool_truthy,  // rand_truthy

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

static ypObject *rand_obj_int_falsy(void) { return yp_i_zero; }

static ypObject *rand_obj_int_truthy(void)
{
    ypObject *result;
    yp_int_t  value = rand_intC();
    if (value == 0) value = 1;  // This makes 1 very slightly more likely.
    result = yp_intC(value);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_int = {
        "int",                   // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_intstore,  // pair

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        rand_obj_int_falsy,   // rand_falsy
        rand_obj_int_truthy,  // rand_truthy

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

static ypObject *rand_obj_intstore_falsy(void) { return yp_intstoreC(0); }

static ypObject *rand_obj_intstore_truthy(void)
{
    ypObject *result;
    yp_int_t  value = rand_intC();
    if (value == 0) value = 1;  // This makes 1 very slightly more likely.
    result = yp_intstoreC(value);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_intstore = {
        "intstore",         // name
        NULL,               // type (initialized at runtime)
        &fixture_type_int,  // pair

        objvarargfunc_error,       // newN
        objvarargfunc_error,       // newK
        rand_obj_intstore_falsy,   // rand_falsy
        rand_obj_intstore_truthy,  // rand_truthy

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

static ypObject *rand_obj_float_falsy(void) { return yp_floatCF(0.0); }

static ypObject *rand_obj_float_truthy(void)
{
    ypObject  *result;
    yp_float_t value = rand_floatCF();
    if (value == 0.0) value = 1.0;  // This makes 1 very slightly more likely.
    result = yp_floatCF(value);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_float = {
        "float",                   // name
        NULL,                      // type (initialized at runtime)
        &fixture_type_floatstore,  // pair

        objvarargfunc_error,    // newN
        objvarargfunc_error,    // newK
        rand_obj_float_falsy,   // rand_falsy
        rand_obj_float_truthy,  // rand_truthy

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

static ypObject *rand_obj_floatstore_falsy(void) { return yp_floatstoreCF(0.0); }

static ypObject *rand_obj_floatstore_truthy(void)
{
    ypObject  *result;
    yp_float_t value = rand_floatCF();
    if (value == 0.0) value = 1.0;  // This makes 1 very slightly more likely.
    result = yp_floatstoreCF(value);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_floatstore = {
        "floatstore",         // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_float,  // pair

        objvarargfunc_error,         // newN
        objvarargfunc_error,         // newK
        rand_obj_floatstore_falsy,   // rand_falsy
        rand_obj_floatstore_truthy,  // rand_truthy

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

static ypObject *rand_obj_iter_func(ypObject *g, ypObject *value)
{
    yp_ssize_t *n;
    yp_ssize_t  size;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, (void **)&n, &size));
    if (*n < 1) return yp_StopIteration;
    (*n)--;
    return rand_obj_any();
}

static ypObject *rand_obj_iter(void)
{
    // While all iters are truthy, even if they yield no values, other types use us to create their
    // own truthy values, so choose a non-zero n.
    yp_ssize_t          n = munit_rand_int_range(1, 16);
    yp_state_decl_t     state_decl = {yp_sizeof(n)};
    yp_generator_decl_t decl = {rand_obj_iter_func, n, &n, &state_decl};
    ypObject           *result = yp_generatorC(&decl);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_iter = {
        "iter",              // name
        NULL,                // type (initialized at runtime)
        &fixture_type_iter,  // pair

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy (all iter objects are truthy)
        rand_obj_iter,        // rand_truthy

        rand_obj_any,            // rand_item
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

// Could get fancy and attempt to construct a set of arguments that result in an empty range... but
// that's better left for the range tests themselves.
static ypObject *rand_obj_range_falsy(void) { return yp_range_empty; }

// TODO Ranges that cover more values, not just 32-bit-ish.
static ypObject *rand_obj_range_truthy(void)
{
    ypObject *result;
    yp_int_t  start = (yp_int_t)((yp_int32_t)munit_rand_uint32());
    yp_int_t  len = (yp_int_t)munit_rand_int_range(1, 256);
    yp_int_t  step = (yp_int_t)munit_rand_int_range(-128, 128);
    if (step == 0) step = 1;  // This makes step=1 more likely.

    result = yp_rangeC3(start, start + (step * len), step);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_range = {
        "range",              // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_range,  // pair

        objvarargfunc_error,    // newN
        objvarargfunc_error,    // newK
        rand_obj_range_falsy,   // rand_falsy
        rand_obj_range_truthy,  // rand_truthy

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

static ypObject *rand_obj_bytes_falsy(void) { return yp_bytes_empty; }

static ypObject *rand_obj_bytes_truthy(void)
{
    ypObject  *result;
    yp_uint8_t source[16];
    yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
    munit_rand_memory((size_t)len, source);
    result = yp_bytesC(source, len);  // FIXME I gotta flip these arguments around!
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_bytes = {
        "bytes",                  // name
        NULL,                     // type (initialized at runtime)
        &fixture_type_bytearray,  // pair

        objvarargfunc_error,    // newN
        objvarargfunc_error,    // newK
        rand_obj_bytes_falsy,   // rand_falsy
        rand_obj_bytes_truthy,  // rand_truthy

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

static ypObject *rand_obj_bytearray_truthy(void)
{
    ypObject  *result;
    yp_uint8_t source[16];
    yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
    munit_rand_memory((size_t)len, source);
    result = yp_bytearrayC(source, len);  // FIXME I gotta flip these arguments around!
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_bytearray = {
        "bytearray",          // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_bytes,  // pair

        objvarargfunc_error,        // newN
        objvarargfunc_error,        // newK
        yp_bytearray0,              // rand_falsy
        rand_obj_bytearray_truthy,  // rand_truthy

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

static ypObject *rand_obj_str_falsy(void) { return yp_str_empty; }

// TODO Return larger characters than just ascii.
static ypObject *rand_obj_str_truthy(void)
{
    ypObject  *result;
    yp_uint8_t source[16];
    yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
    rand_ascii(len, source);
    // FIXME I gotta flip these arguments around!
    result = yp_str_frombytesC4(source, len, yp_s_utf_8, yp_s_strict);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_str = {
        "str",                   // name
        NULL,                    // type (initialized at runtime)
        &fixture_type_chrarray,  // pair

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        rand_obj_str_falsy,   // rand_falsy
        rand_obj_str_truthy,  // rand_truthy

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
static ypObject *rand_obj_chrarray_truthy(void)
{
    ypObject  *result;
    yp_uint8_t source[16];
    yp_ssize_t len = munit_rand_int_range(1, yp_lengthof_array(source));
    rand_ascii(len, source);
    // FIXME I gotta flip these arguments around!
    result = yp_chrarray_frombytesC4(source, len, yp_s_utf_8, yp_s_strict);
    assert_not_exception(result);
    return result;
}

fixture_type_t fixture_type_chrarray = {
        "chrarray",         // name
        NULL,               // type (initialized at runtime)
        &fixture_type_str,  // pair

        objvarargfunc_error,       // newN
        objvarargfunc_error,       // newK
        yp_chrarray0,              // rand_falsy
        rand_obj_chrarray_truthy,  // rand_truthy

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

static ypObject *rand_obj_tuple_falsy(void) { return yp_tuple_empty; }

static ypObject *rand_obj_tuple_truthy(void)
{
    ypObject *iter = rand_obj_iter();
    ypObject *result = yp_tuple(iter);
    assert_not_exception(result);
    yp_decref(iter);
    return result;
}

fixture_type_t fixture_type_tuple = {
        "tuple",             // name
        NULL,                // type (initialized at runtime)
        &fixture_type_list,  // pair

        objvarargfunc_error,    // newN
        objvarargfunc_error,    // newK
        rand_obj_tuple_falsy,   // rand_falsy
        rand_obj_tuple_truthy,  // rand_truthy

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

static ypObject *rand_obj_list_falsy(void) { return yp_listN(0); }

static ypObject *rand_obj_list_truthy(void)
{
    ypObject *iter = rand_obj_iter();
    ypObject *result = yp_list(iter);
    assert_not_exception(result);
    yp_decref(iter);
    return result;
}

fixture_type_t fixture_type_list = {
        "list",               // name
        NULL,                 // type (initialized at runtime)
        &fixture_type_tuple,  // pair

        objvarargfunc_error,   // newN
        objvarargfunc_error,   // newK
        rand_obj_list_falsy,   // rand_falsy
        rand_obj_list_truthy,  // rand_truthy

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

static ypObject *rand_obj_iter_immutable_func(ypObject *g, ypObject *value)
{
    yp_ssize_t *n;
    yp_ssize_t  size;
    if (yp_isexceptionC(value)) return value;
    assert_not_exception(yp_iter_stateCX(g, (void **)&n, &size));
    if (*n < 1) return yp_StopIteration;
    (*n)--;
    return rand_obj_any_immutable();
}

static ypObject *rand_obj_iter_immutable(void)
{
    yp_ssize_t          n = munit_rand_int_range(1, 16);
    yp_state_decl_t     state_decl = {yp_sizeof(n)};
    yp_generator_decl_t decl = {rand_obj_iter_immutable_func, n, &n, &state_decl};
    ypObject           *result = yp_generatorC(&decl);
    assert_not_exception(result);
    return result;
}

static ypObject *rand_obj_frozenset_falsy(void) { return yp_frozenset_empty; }

static ypObject *rand_obj_frozenset_truthy(void)
{
    // FIXME What we actually need is rand_obj_iter_hashable. tuple is an immutable object, but if
    // it contains mutable objects it's not hashable.
    ypObject *iter = rand_obj_iter_immutable();
    ypObject *result = yp_frozenset(iter);
    assert_not_exception(result);
    yp_decref(iter);
    return result;
}

fixture_type_t fixture_type_frozenset = {
        "frozenset",        // name
        NULL,               // type (initialized at runtime)
        &fixture_type_set,  // pair

        objvarargfunc_error,        // newN
        objvarargfunc_error,        // newK
        rand_obj_frozenset_falsy,   // rand_falsy
        rand_obj_frozenset_truthy,  // rand_truthy

        rand_obj_any_immutable,  // rand_item
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

static ypObject *rand_obj_set_falsy(void) { return yp_setN(0); }

static ypObject *rand_obj_set_truthy(void)
{
    ypObject *iter = rand_obj_iter_immutable();
    ypObject *result = yp_set(iter);
    assert_not_exception(result);
    yp_decref(iter);
    return result;
}

fixture_type_t fixture_type_set = {
        "set",                    // name
        NULL,                     // type (initialized at runtime)
        &fixture_type_frozenset,  // pair

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        rand_obj_set_falsy,   // rand_falsy
        rand_obj_set_truthy,  // rand_truthy

        rand_obj_any_immutable,  // rand_item
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

static ypObject *rand_obj_iter_key_value_func(ypObject *g, ypObject *sent_value)
{
    yp_ssize_t *n;
    yp_ssize_t  size;
    ypObject   *key;
    ypObject   *value;
    ypObject   *tuple;

    if (yp_isexceptionC(sent_value)) return sent_value;
    assert_not_exception(yp_iter_stateCX(g, (void **)&n, &size));
    if (*n < 1) return yp_StopIteration;
    (*n)--;

    key = rand_obj_any_immutable();
    value = rand_obj_any();
    tuple = yp_tupleN(2, key, value);
    assert_not_exception(tuple);
    yp_decrefN(2, key, value);
    return tuple;
}

static ypObject *rand_obj_iter_key_value(void)
{
    yp_ssize_t          n = munit_rand_int_range(1, 16);
    yp_state_decl_t     state_decl = {yp_sizeof(n)};
    yp_generator_decl_t decl = {rand_obj_iter_key_value_func, n, &n, &state_decl};
    ypObject           *result = yp_generatorC(&decl);
    assert_not_exception(result);
    return result;
}

static ypObject *rand_obj_frozendict_falsy(void) { return yp_frozendict_empty; }

static ypObject *rand_obj_frozendict_truthy(void)
{
    ypObject *iter = rand_obj_iter_key_value();
    ypObject *result = yp_frozendict(iter);
    assert_not_exception(result);
    yp_decref(iter);
    return result;
}

fixture_type_t fixture_type_frozendict = {
        "frozendict",        // name
        NULL,                // type (initialized at runtime)
        &fixture_type_dict,  // pair

        objvarargfunc_error,         // newN
        objvarargfunc_error,         // newK
        rand_obj_frozendict_falsy,   // rand_falsy
        rand_obj_frozendict_truthy,  // rand_truthy

        rand_obj_any,            // rand_item  // FIXME maybe this one returns the keys
        voidobjpobjpfunc_error,  // rand_key_value  // FIXME ...and this one returns the values

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

static ypObject *rand_obj_dict_falsy(void) { return yp_dictK(0); }

static ypObject *rand_obj_dict_truthy(void)
{
    ypObject *iter = rand_obj_iter_key_value();
    ypObject *result = yp_dict(iter);
    assert_not_exception(result);
    yp_decref(iter);
    return result;
}

fixture_type_t fixture_type_dict = {
        "dict",                    // name
        NULL,                      // type (initialized at runtime)
        &fixture_type_frozendict,  // pair

        objvarargfunc_error,   // newN
        objvarargfunc_error,   // newK
        rand_obj_dict_falsy,   // rand_falsy
        rand_obj_dict_truthy,  // rand_truthy

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

static ypObject *rand_obj_function(void)
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

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy (all function objects are truthy)
        rand_obj_function,    // rand_truthy

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

fixture_type_t *fixture_types_all[FIXTURE_TYPES_ALL_LEN + 1] = {&fixture_type_type,
        &fixture_type_NoneType, &fixture_type_bool, &fixture_type_int, &fixture_type_intstore,
        &fixture_type_float, &fixture_type_floatstore, &fixture_type_iter, &fixture_type_range,
        &fixture_type_bytes, &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray,
        &fixture_type_tuple, &fixture_type_list, &fixture_type_frozenset, &fixture_type_set,
        &fixture_type_frozendict, &fixture_type_dict, &fixture_type_function, NULL};
// These are subsets of fixture_types_all, so will at most hold that many elements.
fixture_type_t *fixture_types_numeric[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_iterable[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_collection[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_sequence[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_string[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_set[FIXTURE_TYPES_ALL_LEN + 1];
fixture_type_t *fixture_types_mapping[FIXTURE_TYPES_ALL_LEN + 1];

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
        char           **param_array = param_values_types_all;
        for (types = fixture_types_all; *types != NULL; types++) {
            *param_array = (*types)->name;
            param_array++;
        }
        *param_array = NULL;
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

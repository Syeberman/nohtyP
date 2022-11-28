
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


fixture_type_t fixture_type_int = {
        "int",  // name
        NULL,   // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_intstore = {
        "intstore",  // name
        NULL,        // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_float = {
        "float",  // name
        NULL,     // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_floatstore = {
        "floatstore",  // name
        NULL,          // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_iter = {
        "iter",  // name
        NULL,    // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_range = {
        "range",  // name
        NULL,     // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_bytes = {
        "bytes",  // name
        NULL,     // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_bytearray = {
        "bytearray",  // name
        NULL,         // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_str = {
        "str",  // name
        NULL,   // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_chrarray = {
        "chrarray",  // name
        NULL,        // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_tuple = {
        "tuple",  // name
        NULL,     // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_list = {
        "list",  // name
        NULL,    // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_frozenset = {
        "frozenset",  // name
        NULL,         // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_set = {
        "set",  // name
        NULL,   // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_frozendict = {
        "frozendict",  // name
        NULL,          // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
        voidobjpobjpfunc_error,  // rand_key_value

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

fixture_type_t fixture_type_dict = {
        "dict",  // name
        NULL,    // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

static void initialize_fixture_types(void)
{
    // These need to be initialized at runtime because they may be imported from a DLL.
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
}

extern void unittest_initialize(void) { initialize_fixture_types(); }

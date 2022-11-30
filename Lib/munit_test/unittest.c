
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


fixture_type_t fixture_type_type = {
        "type",  // name
        NULL,    // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_NoneType = {
        "NoneType",  // name
        NULL,        // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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

fixture_type_t fixture_type_bool = {
        "bool",  // name
        NULL,    // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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
        yp_bytearray0,        // rand_falsy
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
        yp_chrarray0,         // rand_falsy
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

fixture_type_t fixture_type_function = {
        "function",  // name
        NULL,        // type (initialized at runtime)

        objvarargfunc_error,  // newN
        objvarargfunc_error,  // newK
        objvoidfunc_error,    // rand_falsy
        objvoidfunc_error,    // rand_truthy

        objvoidfunc_error,       // rand_value
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
// These are subsets of fixture_types_all, so will at most hold that many elements.
fixture_type_t *fixture_types_numeric[yp_lengthof_array(fixture_types_all)];
fixture_type_t *fixture_types_iterable[yp_lengthof_array(fixture_types_all)];
fixture_type_t *fixture_types_collection[yp_lengthof_array(fixture_types_all)];
fixture_type_t *fixture_types_sequence[yp_lengthof_array(fixture_types_all)];
fixture_type_t *fixture_types_string[yp_lengthof_array(fixture_types_all)];
fixture_type_t *fixture_types_set[yp_lengthof_array(fixture_types_all)];
fixture_type_t *fixture_types_mapping[yp_lengthof_array(fixture_types_all)];

// Once again, subsets of fixture_types_all.
char *param_values_types_all[yp_lengthof_array(fixture_types_all)];
char *param_values_types_numeric[yp_lengthof_array(fixture_types_all)];
char *param_values_types_iterable[yp_lengthof_array(fixture_types_all)];
char *param_values_types_collection[yp_lengthof_array(fixture_types_all)];
char *param_values_types_sequence[yp_lengthof_array(fixture_types_all)];
char *param_values_types_string[yp_lengthof_array(fixture_types_all)];
char *param_values_types_set[yp_lengthof_array(fixture_types_all)];
char *param_values_types_mapping[yp_lengthof_array(fixture_types_all)];

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

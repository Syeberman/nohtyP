
#include "munit_test/unittest.h"

// _test_callK is a bit too complex for GCC.
#if defined(__GNUC__)
#pragma GCC optimize("no-var-tracking")
#endif


typedef struct _signature_t {
    yp_int32_t          n;
    yp_parameter_decl_t params[8];  // Increase length as necessary.
} signature_t;

// Used as the code for a function. Unconditionally returns None.
static ypObject *None_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray) { return yp_None; }


// Used to represent NULL in the captured argarray. Recall functions are compared by identity, so
// this will not be confused with a valid value.
yp_IMMORTAL_FUNCTION2_static(captured_NULL, None_code);

// As with captured_NULL, represents yp_NameError/etc in the captured argarray.
yp_IMMORTAL_FUNCTION2_static(captured_NameError, None_code);

// Used as the code for a function. Captures all details about the arguments and returns them. NULL
// entries in argarray are replaced with the captured_NULL object, and yp_NameError with
// captured_NameError.
static ypObject *capture_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ssize_t i;
    ypObject  *argarray_ptr = yp_intC((yp_ssize_t)argarray);  // new ref
    ypObject  *result = yp_listN(N(f, argarray_ptr));
    assert_not_exception(argarray_ptr);
    assert_not_exception(result);
    yp_decref(argarray_ptr);

    for (i = 0; i < n; i++) {
        ypObject *arg = argarray[i];
        if (arg == NULL) {
            arg = captured_NULL;
        } else if (arg == yp_NameError) {
            arg = captured_NameError;
        }
        assert_not_exception(arg);
        assert_not_raises_exc(yp_append(result, arg, &exc));
    }

    return result;
}

// Asserts that f, n, and argarray were exactly the given values (compared by ==).
#define assert_captured_is(captured, f_expected, n_expected, argarray_expected)                   \
    do {                                                                                          \
        ypObject  *_ypmt_CAPT_captured = (captured);                                              \
        ypObject  *_ypmt_CAPT_f = yp_getindexC(_ypmt_CAPT_captured, 0); /* new ref */             \
        yp_ssize_t _ypmt_CAPT_len;                                                                \
        ypObject  *_ypmt_CAPT_argarray_obj = yp_getindexC(_ypmt_CAPT_captured, 1); /* new ref */  \
        void      *_ypmt_CAPT_argarray;                                                           \
        ypObject  *_ypmt_CAPT_f_expected = (f_expected);                                          \
        yp_ssize_t _ypmt_CAPT_n_expected = (n_expected);                                          \
        void      *_ypmt_CAPT_argarray_expected = (argarray_expected);                            \
        _assert_not_exception(_ypmt_CAPT_f, "yp_getindexC(%s, 0)", #captured);                    \
        _assert_not_raises_exc(_ypmt_CAPT_len = yp_lenC(_ypmt_CAPT_captured, &exc),               \
                "yp_lenC(%s, &exc)", #captured);                                                  \
        _assert_not_raises_exc(                                                                   \
                _ypmt_CAPT_argarray = (void *)yp_asssizeC(_ypmt_CAPT_argarray_obj, &exc),         \
                "yp_asssizeC(yp_getindexC(%s, 1), &exc)", #captured);                             \
        _assert_obj(                                                                              \
                _ypmt_CAPT_f, is, _ypmt_CAPT_f_expected, "<%s f>", "%s", #captured, #f_expected); \
        _assert_typeC(_ypmt_CAPT_len - 2, ==, _ypmt_CAPT_n_expected, PRIssize, "<%s n>", "%s",    \
                #captured, #n_expected);                                                          \
        _assert_ptr(_ypmt_CAPT_argarray, ==, _ypmt_CAPT_argarray_expected, "<%s argarray>", "%s", \
                #captured, #argarray_expected);                                                   \
        yp_decrefN(N(_ypmt_CAPT_f, _ypmt_CAPT_argarray_obj));                                     \
    } while (0)

// Asserts that f was as expected (compared by identity, aka ==) and that n/argarray contained
// exactly the given items in order (compared by equality). Use captured_NULL for any NULL entries.
#define assert_captured(captured, f_expected, ...)                                                \
    do {                                                                                          \
        ypObject *_ypmt_CAPT_captured = (captured);                                               \
        ypObject *_ypmt_CAPT_f = yp_getindexC(_ypmt_CAPT_captured, 0); /* new ref */              \
        ypObject *_ypmt_CAPT_args =                                                               \
                yp_getsliceC4(_ypmt_CAPT_captured, 2, yp_SLICE_LAST, 1); /* new ref */            \
        ypObject *_ypmt_CAPT_f_expected = (f_expected);                                           \
        ypObject *_ypmt_CAPT_items[] = {__VA_ARGS__};                                             \
        char     *_ypmt_CAPT_item_strs[] = {STRINGIFY(__VA_ARGS__)};                              \
        _assert_not_exception(_ypmt_CAPT_captured, "%s", #captured);                              \
        _assert_not_exception(_ypmt_CAPT_f, "yp_getindexC(%s, 0)", #captured);                    \
        _assert_not_exception(                                                                    \
                _ypmt_CAPT_args, "yp_getsliceC4(%s, 2, yp_SLICE_LAST, 1)", #captured);            \
        _assert_obj(                                                                              \
                _ypmt_CAPT_f, is, _ypmt_CAPT_f_expected, "<%s f>", "%s", #captured, #f_expected); \
        _assert_sequence(                                                                         \
                _ypmt_CAPT_args, _ypmt_CAPT_items, "<%s args>", _ypmt_CAPT_item_strs, #captured); \
        yp_decrefN(N(_ypmt_CAPT_f, _ypmt_CAPT_args));                                             \
    } while (0)

#define assert_captured_arg(captured, i, op, expected)                                           \
    do {                                                                                         \
        ypObject  *_ypmt_CAPT_captured = (captured);                                             \
        yp_ssize_t _ypmt_CAPT_i = (i);                                                           \
        ypObject  *_ypmt_CAPT_expected = (expected);                                             \
        ypObject  *_ypmt_CAPT_arg =                                                              \
                yp_getindexC(_ypmt_CAPT_captured, _ypmt_CAPT_i + 2); /* new ref */               \
        _assert_not_exception(_ypmt_CAPT_captured, "%s", #captured);                             \
        _assert_not_exception(                                                                   \
                _ypmt_CAPT_arg, "yp_getindexC(%s, %" PRIssize ")", #captured, _ypmt_CAPT_i + 2); \
        _assert_obj(_ypmt_CAPT_arg, op, _ypmt_CAPT_expected, "<%s args[%" PRIssize "]>", "%s",   \
                #captured, _ypmt_CAPT_i, #expected);                                             \
        yp_decrefN(N(_ypmt_CAPT_arg));                                                           \
    } while (0)

// Fills array with n parameters; useful to create functions with many parameters. names start with
// name_prefix and end with a number incrementing from zero. rand_obj_any generates the default
// values. array will contain new references that will need to be decremented with
// parameter_decl_array_decref*.
static void parameter_decl_array_fill4(
        uniqueness_t *uq, yp_ssize_t n, yp_parameter_decl_t *array, char *name_prefix)
{
    yp_ssize_t i;
    char       name[32];  // Enough for a 64-bit decimal value (19), \0, and a long prefix (12).

    for (i = 0; i < n; i++) {
        // Recall sprintf_array asserts on buffer overflow.
        sprintf_array(name, "%s%" PRIssize, name_prefix, i);
        assert_not_raises(array[i].name = yp_str_frombytesC2(-1, name));  // new ref
        array[i].default_ = rand_obj_any(uq);                             // new ref
    }
}

// Discards all references in the ypObject * array of length n. Skips NULL elements.
static void parameter_decl_array_decref2(yp_ssize_t n, yp_parameter_decl_t *array)
{
    yp_ssize_t i;
    for (i = 0; i < n; i++) {
        if (array[i].name != NULL) yp_decref(array[i].name);
        if (array[i].default_ != NULL) yp_decref(array[i].default_);
    }
    memset(array, 0, ((size_t)n) * sizeof(yp_parameter_decl_t));
}

// Discards all references in the ypObject * array. Skips NULL elements. Only call for arrays of
// fixed size (uses yp_lengthof_array).
#define parameter_decl_array_decref(array) \
    parameter_decl_array_decref2(yp_lengthof_array(array), (array))


// Declares a variable name of type ypObject * and initializes it with a new reference to a function
// object. The parameters argument must be surrounded by parentheses.
// XXX Older compilers reject an empty parameters argument; use define_function2 instead.
#define define_function(name, code, parameters)                                                     \
    yp_parameter_decl_t _##name##_parameters[] = {UNPACK parameters};                               \
    yp_function_decl_t  _##name##_declaration = {                                                   \
            (code), 0, yp_lengthof_array(_##name##_parameters), _##name##_parameters, NULL, NULL}; \
    ypObject *name = yp_functionC(&_##name##_declaration)

// Equivalent to define_function(name, code, ()).
#define define_function2(name, code)                                             \
    yp_function_decl_t _##name##_declaration = {(code), 0, 0, NULL, NULL, NULL}; \
    ypObject          *name = yp_functionC(&_##name##_declaration)


static MunitResult test_newC(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *int_0 = yp_intC(0);
    ypObject *str_a = yp_str_frombytesC2(-1, "a");
    ypObject *str_b = yp_str_frombytesC2(-1, "b");
    ypObject *str_c = yp_str_frombytesC2(-1, "c");
    ypObject *str_slash = yp_str_frombytesC2(-1, "/");
    ypObject *str_star = yp_str_frombytesC2(-1, "*");
    ypObject *str_star_args = yp_str_frombytesC2(-1, "*args");
    ypObject *str_star_star_kwargs = yp_str_frombytesC2(-1, "**kwargs");
    ypObject *str_empty = yp_str_frombytesC2(-1, "");

    // Valid signatures.
    {
        yp_ssize_t  i;
        signature_t signatures[] = {
                {0},                                                     // def f()
                {1, {{str_a}}},                                          // def f(a)
                {2, {{str_a}, {str_b}}},                                 // def f(a, b)
                {1, {{str_a, int_0}}},                                   // def f(a=0)
                {1, {{str_a, yp_NameError}}},                            // def f(a=<exc>)
                {2, {{str_a, int_0}, {str_b, int_0}}},                   // def f(a=0, b=0)
                {2, {{str_a}, {str_slash}}},                             // def f(a, /)
                {3, {{str_a}, {str_slash}, {str_b}}},                    // def f(a, /, b)
                {3, {{str_a}, {str_b}, {str_slash}}},                    // def f(a, b, /)
                {2, {{str_a, int_0}, {str_slash}}},                      // def f(a=0, /)
                {2, {{str_a, yp_NameError}, {str_slash}}},               // def f(a=<exc>, /)
                {3, {{str_a, int_0}, {str_slash}, {str_b, int_0}}},      // def f(a=0, /, b=0)
                {3, {{str_a, int_0}, {str_b, int_0}, {str_slash}}},      // def f(a=0, b=0, /)
                {2, {{str_star}, {str_a}}},                              // def f(*, a)
                {3, {{str_star}, {str_a}, {str_b}}},                     // def f(*, a, b)
                {3, {{str_star}, {str_a}, {str_b, int_0}}},              // def f(*, a, b=0)
                {3, {{str_a}, {str_star}, {str_b}}},                     // def f(a, *, b)
                {3, {{str_a}, {str_star}, {str_b, int_0}}},              // def f(a, *, b=0)
                {2, {{str_star}, {str_a, int_0}}},                       // def f(*, a=0)
                {2, {{str_star}, {str_a, yp_NameError}}},                // def f(*, a=<exc>)
                {3, {{str_star}, {str_a, int_0}, {str_b}}},              // def f(*, a=0, b)
                {3, {{str_star}, {str_a, int_0}, {str_b, int_0}}},       // def f(*, a=0, b=0)
                {3, {{str_a, int_0}, {str_star}, {str_b}}},              // def f(a=0, *, b)
                {3, {{str_a, int_0}, {str_star}, {str_b, int_0}}},       // def f(a=0, *, b=0)
                {1, {{str_star_args}}},                                  // def f(*args)
                {2, {{str_star_args}, {str_a}}},                         // def f(*args, a)
                {2, {{str_a}, {str_star_args}}},                         // def f(a, *args)
                {3, {{str_star_args}, {str_a}, {str_b}}},                // def f(*args, a, b)
                {3, {{str_star_args}, {str_a}, {str_b, int_0}}},         // def f(*args, a, b=0)
                {3, {{str_a}, {str_star_args}, {str_b}}},                // def f(a, *args, b)
                {3, {{str_a}, {str_star_args}, {str_b, int_0}}},         // def f(a, *args, b=0)
                {2, {{str_star_args}, {str_a, int_0}}},                  // def f(*args, a=0)
                {2, {{str_star_args}, {str_a, yp_NameError}}},           // def f(*args, a=<exc>)
                {2, {{str_a, int_0}, {str_star_args}}},                  // def f(a=0, *args)
                {3, {{str_star_args}, {str_a, int_0}, {str_b}}},         // def f(*args, a=0, b)
                {3, {{str_star_args}, {str_a, int_0}, {str_b, int_0}}},  // def f(*args, a=0, b=0)
                {3, {{str_a, int_0}, {str_star_args}, {str_b}}},         // def f(a=0, *args, b)
                {3, {{str_a, int_0}, {str_star_args}, {str_b, int_0}}},  // def f(a=0, *args, b=0)
                {3, {{str_a, int_0}, {str_b, int_0}, {str_star_args}}},  // def f(a=0, b=0, *args)
                {1, {{str_star_star_kwargs}}},                           // def f(**kwargs)

                {6, {{str_a}, {str_slash}, {str_b}, {str_star}, {str_c},
                            {str_star_star_kwargs}}},  // def f(a, /, b, *, c, **kwargs)
                {6, {{str_a}, {str_slash}, {str_b}, {str_star_args}, {str_c},
                            {str_star_star_kwargs}}},  // def f(a, /, b, *args, c, **kwargs)

                {4, {{str_a}, {str_slash}, {str_star}, {str_b}}},     // def f(a, /, *, b)
                {3, {{str_a}, {str_slash}, {str_star_args}}},         // def f(a, /, *args)
                {3, {{str_a}, {str_slash}, {str_star_star_kwargs}}},  // def f(a, /, **kwargs)
                {3, {{str_star}, {str_a}, {str_star_star_kwargs}}},   // def f(*, a, **kwargs)
                {2, {{str_star_args}, {str_star_star_kwargs}}},       // def f(*args, **kwargs)
        };
        for (i = 0; i < yp_lengthof_array(signatures); i++) {
            signature_t        signature = signatures[i];
            yp_function_decl_t decl = {None_code, 0, signature.n, signature.params, NULL, NULL};
            ypObject          *func = yp_functionC(&decl);
            assert_type_is(func, yp_t_function);
            yp_decref(func);
        }
    }

    // parameters can be NULL if parameters_length is zero.
    {
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, NULL};
        ypObject          *func = yp_functionC(&decl);
        assert_type_is(func, yp_t_function);
        yp_decref(func);
    }

    // flags must be zero.
    {
        yp_function_decl_t decl = {None_code, 0xFFFFFFFFu, 0, NULL, NULL, NULL};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // parameters_len cannot be negative.
    {
        yp_function_decl_t decl = {None_code, 0, -1, NULL, NULL, NULL};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl.size cannot be negative.
    {
        static yp_state_decl_t state_decl = {-1};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // Excessively-large state_decl.size. Once state is implemented this should raise
    // yp_MemorySizeOverflowError.
    {
        static yp_state_decl_t state_decl = {yp_SSIZE_T_MAX};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_NotImplementedError);
    }

    // state_decl.offsets_len == -1 (array of objects) is not yet implemented.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), -1};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_NotImplementedError);
    }

    // state_decl.offsets_len must be >= 0 or -1.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), -2};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl offsets cannot be negative.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), 1, {-1}};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl objects must be fully contained in state.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *) - 1, 1, {0}};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl objects must be aligned.
    {
        static yp_state_decl_t state_decl = {1 + yp_sizeof(ypObject *), 1, {1}};
        yp_function_decl_t     decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_SystemLimitationError);
    }

    // There is a maximum to the state_decl object offsets.
    {
        static yp_state_decl_t state_decl = {
                33 * yp_sizeof(ypObject *), 1, {32 * yp_sizeof(ypObject *)}};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_SystemLimitationError);
    }

    // state is not yet implemented.
    {
        yp_state_decl_t    state_decl = {1};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_NotImplementedError);
    }

    // Invalid signatures.
    {
        yp_ssize_t i;
        // ypObject   *str_1 = yp_str_frombytesC2(-1, "1");
        ypObject *str_star_a = yp_str_frombytesC2(-1, "*a");
        // ypObject   *str_star_1 = yp_str_frombytesC2(-1, "*1");
        ypObject *str_star_star = yp_str_frombytesC2(-1, "**");
        ypObject *str_star_star_a = yp_str_frombytesC2(-1, "**a");
        // ypObject   *str_star_star_1 = yp_str_frombytesC2(-1, "**1");
        signature_t signatures[] = {
                {1, {{str_slash}}},                           // / cannot be first
                {2, {{str_star}, {str_slash}}},               // / cannot be after *
                {3, {{str_a}, {str_star}, {str_slash}}},      // / cannot be after *
                {3, {{str_star}, {str_a}, {str_slash}}},      // / cannot be after *
                {2, {{str_star_args}, {str_slash}}},          // / cannot be after *args
                {2, {{str_star_star_kwargs}, {str_slash}}},   // / cannot be after **kwargs
                {3, {{str_a}, {str_slash}, {str_slash}}},     // At most one /
                {2, {{str_a}, {str_slash, int_0}}},           // / cannot have default
                {1, {{str_star}}},                            // * cannot be last
                {2, {{str_star}, {str_star_star_kwargs}}},    // * cannot be imm. before **kwargs
                {3, {{str_star}, {str_star}, {str_a}}},       // At most one *
                {2, {{str_star}, {str_star_args}}},           // * or *args, not both
                {3, {{str_star_args}, {str_star}, {str_a}}},  // * or *args, not both
                {2, {{str_star, int_0}, {str_a}}},            // * cannot have default
                {2, {{str_star_args}, {str_star_args}}},      // At most one *args
                {1, {{str_star_args, int_0}}},                // *args cannot have default
                {2, {{str_star_star_kwargs}, {str_a}}},       // **kwargs must be last
                {2, {{str_star_star_kwargs}, {str_slash}}},   // **kwargs must be last
                {3, {{str_star_star_kwargs}, {str_star}, {str_a}}},     // **kwargs must be last
                {2, {{str_star_star_kwargs}, {str_star_args}}},         // **kwargs must be last
                {2, {{str_star_star_kwargs}, {str_star_star_kwargs}}},  // At most one **kwargs
                {1, {{str_star_star_kwargs, int_0}}},         // **kwargs cannot have default
                {2, {{str_a, int_0}, {str_b}}},               // Defaults on remaining pos. args
                {3, {{str_a, int_0}, {str_b}, {str_slash}}},  // Defaults on remaining pos. args
                {3, {{str_a, int_0}, {str_slash}, {str_b}}},  // Defaults on remaining pos. args

                // Non-identifiers
                {1, {{str_empty}}},
                // TODO Implement str_isidentifier: {1, {{str_1}}},
                // TODO Implement str_isidentifier: {1, {{str_star_1}}},
                {1, {{str_star_star}}},
                // TODO Implement str_isidentifier: {1, {{str_star_star_1}}},

                // Non-unique names
                {2, {{str_a}, {str_a}}},
                {3, {{str_a}, {str_slash}, {str_a}}},
                {3, {{str_a}, {str_star}, {str_a}}},
                {3, {{str_a}, {str_star_args}, {str_a}}},
                {2, {{str_a}, {str_star_a}}},
                {2, {{str_a}, {str_star_star_a}}},
                {2, {{str_star_a}, {str_a}}},
                {2, {{str_star_a}, {str_star_star_a}}},
        };
        for (i = 0; i < yp_lengthof_array(signatures); i++) {
            signature_t        signature = signatures[i];
            yp_function_decl_t decl = {None_code, 0, signature.n, signature.params, NULL, NULL};
            assert_raises(yp_functionC(&decl), yp_ParameterSyntaxError);
        }
        yp_decrefN(N(str_star_a, str_star_star, str_star_star_a));
    }

    // Parameter names must be strs.
    {
        yp_ssize_t  i;
        ypObject   *bytes_a = yp_encode(str_a);
        ypObject   *bytes_slash = yp_encode(str_slash);
        ypObject   *bytes_star = yp_encode(str_star);
        ypObject   *bytes_star_args = yp_encode(str_star_args);
        ypObject   *bytes_star_star_kwargs = yp_encode(str_star_star_kwargs);
        ypObject   *chrarray_a = yp_chrarray(str_a);
        ypObject   *chrarray_slash = yp_chrarray(str_slash);
        ypObject   *chrarray_star = yp_chrarray(str_star);
        ypObject   *chrarray_star_args = yp_chrarray(str_star_args);
        ypObject   *chrarray_star_star_kwargs = yp_chrarray(str_star_star_kwargs);
        signature_t signatures[] = {
                {1, {{bytes_a}}},                    // name must be a str
                {1, {{chrarray_a}}},                 // name must be a str
                {2, {{str_a}, {bytes_slash}}},       // / must be a str
                {2, {{str_a}, {chrarray_slash}}},    // / must be a str
                {2, {{bytes_star}, {str_a}}},        // * must be a str
                {2, {{chrarray_star}, {str_a}}},     // * must be a str
                {1, {{bytes_star_args}}},            // *args must be a str
                {1, {{chrarray_star_args}}},         // *args must be a str
                {1, {{bytes_star_star_kwargs}}},     // **kwargs must be a str
                {1, {{chrarray_star_star_kwargs}}},  // **kwargs must be a str
        };
        for (i = 0; i < yp_lengthof_array(signatures); i++) {
            signature_t        signature = signatures[i];
            yp_function_decl_t decl = {None_code, 0, signature.n, signature.params, NULL, NULL};
            assert_raises(yp_functionC(&decl), yp_TypeError);
        }
        yp_decrefN(N(bytes_a, bytes_slash, bytes_star, bytes_star_args, bytes_star_star_kwargs,
                chrarray_a, chrarray_slash, chrarray_star, chrarray_star_args,
                chrarray_star_star_kwargs));
    }

    // Exception passthrough.
    {
        define_function(f, None_code, ({yp_OSError}));
        assert_isexception(f, yp_OSError);
    }

    yp_decrefN(N(
            int_0, str_a, str_b, str_c, str_slash, str_star, str_star_args, str_star_star_kwargs));
    return MUNIT_OK;
}

// Tests yp_IMMORTAL_FUNCTION*. As this shares the same parameter-validation code as yp_functionC,
// these tests focus on yp_IMMORTAL_FUNCTION-specific behaviour.
static MunitResult test_new_immortal(const MunitParameter params[], fixture_t *fixture)
{
    yp_IMMORTAL_STR_LATIN_1(str_a, "a");
    yp_IMMORTAL_STR_LATIN_1(str_star, "*");

    // Signatures of immortals are validated by yp_call*.
    {
        // Once an immortal function is marked validated, it is not validated again. So create
        // separate objects for each yp_call*.
        yp_IMMORTAL_FUNCTION2(f_callN, capture_code);
        yp_IMMORTAL_FUNCTION2(f_call_stars, capture_code);
        yp_IMMORTAL_FUNCTION2(f_call_arrayX, capture_code);
        ypObject *f_call_arrayX_args[] = {f_call_arrayX};
        yp_IMMORTAL_FUNCTION(f_a_callN, capture_code, ({yp_CONST_REF(str_a)}));
        yp_IMMORTAL_FUNCTION(f_a_call_stars, capture_code, ({yp_CONST_REF(str_a)}));
        ypObject *f_a_call_stars_args = yp_tupleN(1, yp_i_one);  // new ref
        ypObject *f_a_call_stars_kwargs =
                yp_frozendictK(1, yp_CONST_REF(str_a), yp_i_one);  // new ref
        yp_IMMORTAL_FUNCTION(f_a_call_arrayX, capture_code, ({yp_CONST_REF(str_a)}));
        ypObject *f_a_call_arrayX_args[] = {f_a_call_arrayX, yp_i_one};

        ead(capt, yp_callN(f_callN, 0), assert_captured_is(capt, f_callN, 0, NULL));
        ead(capt, yp_call_stars(f_call_stars, yp_tuple_empty, yp_frozendict_empty),
                assert_captured_is(capt, f_call_stars, 0, NULL));
        ead(capt, yp_call_arrayX(1, f_call_arrayX_args),
                assert_captured_is(capt, f_call_arrayX, 0, NULL));

        ead(capt, yp_callN(f_a_callN, 1, yp_i_one), assert_captured(capt, f_a_callN, yp_i_one));
        ead(capt, yp_call_stars(f_a_call_stars, f_a_call_stars_args, yp_frozendict_empty),
                assert_captured(capt, f_a_call_stars, yp_i_one));
        ead(capt, yp_call_stars(f_a_call_stars, yp_tuple_empty, f_a_call_stars_kwargs),
                assert_captured(capt, f_a_call_stars, yp_i_one));
        ead(capt, yp_call_arrayX(2, f_a_call_arrayX_args),
                assert_captured(capt, f_a_call_arrayX, yp_i_one));

        yp_decrefN(N(f_a_call_stars_args, f_a_call_stars_kwargs));
    }

    // Invalid signatures.
    {
        // Invalid immortal functions are validated every time, even though the result will be the
        // same. Regardless, we don't need separate objects for each call.
        yp_IMMORTAL_FUNCTION(f_star, capture_code, ({yp_CONST_REF(str_star)}));
        ypObject *f_star_call_arrayX_args[] = {f_star};

        assert_raises(yp_callN(f_star, 0), yp_ParameterSyntaxError);
        assert_raises(yp_call_stars(f_star, yp_tuple_empty, yp_frozendict_empty),
                yp_ParameterSyntaxError);
        assert_raises(yp_call_arrayX(1, f_star_call_arrayX_args), yp_ParameterSyntaxError);
    }

    return MUNIT_OK;
}

// Tests common to test_callN, test_call_stars, and callN_to_call_arrayX.
static void _test_callN(ypObject *(*any_callN)(ypObject *, int, ...))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defs[] = obj_array_init(5, rand_obj_any(uq));
    ypObject     *args[] = obj_array_init(33, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    ypObject     *str_b = yp_str_frombytesC2(-1, "b");
    ypObject     *str_slash = yp_str_frombytesC2(-1, "/");
    ypObject     *str_star = yp_str_frombytesC2(-1, "*");
    ypObject     *str_star_args = yp_str_frombytesC2(-1, "*args");
    ypObject     *str_star_star_kwargs = yp_str_frombytesC2(-1, "**kwargs");

    // def f()
    {
        define_function2(f, capture_code);

        ead(capt, any_callN(f, 0), assert_captured_is(capt, f, 0, NULL));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_TypeError, yp_NameError);

        yp_decref(f);
    }

    // def f(a)
    {
        define_function(f, capture_code, ({str_a}));

        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, b)
    {
        define_function(f, capture_code, ({str_a}, {str_b}));

        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0)
    {
        define_function(f, capture_code, ({str_a, defs[0]}));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=yp_NameError)
    {
        define_function(f, capture_code, ({str_a, yp_NameError}));

        // code is called if a default is an exception.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, captured_NameError));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        // code is _not_ called if an argument is an exception.
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], defs[1]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, /)
    {
        define_function(f, capture_code, ({str_a}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have one element.
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, /, b)
    {
        define_function(f, capture_code, ({str_a}, {str_slash}, {str_b}));

        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, b, /)
    {
        define_function(f, capture_code, ({str_a}, {str_b}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have two elements.
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, /)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have one element.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, /, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_slash}, {str_b, defs[1]}));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], captured_NULL, defs[1]));
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], captured_NULL, defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, b=1, /)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have two elements.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], defs[1]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a)
    {
        define_function(f, capture_code, ({str_star}, {str_a}));

        // Being a keyword-only parameter, a cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *, b)
    {
        define_function(f, capture_code, ({str_a}, {str_star}, {str_b}));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *, b=1)
    {
        define_function(f, capture_code, ({str_a}, {str_star}, {str_b, defs[1]}));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], captured_NULL, defs[1]));
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a=0)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}));

        // Being a keyword-only parameter, a cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, captured_NULL, defs[0]));
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a=0, b)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}, {str_b}));

        // Being keyword-only parameters, a and b cannot be set from "callN" positional arguments.
        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_TypeError, yp_NameError);

        yp_decref(f);
    }

    // def f(*, a=0, b=1)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}, {str_b, defs[1]}));

        // Being keyword-only parameters, a and b cannot be set from "callN" positional arguments.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, captured_NULL, defs[0], defs[1]));
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_TypeError, yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *, b)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star}, {str_b}));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star}, {str_b, defs[1]}));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], captured_NULL, defs[1]));
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], captured_NULL, defs[1]));
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args)
    {
        define_function(f, capture_code, ({str_star_args}));
        ypObject *zero = yp_tupleN(N(args[0]));
        ypObject *zero_one = yp_tupleN(N(args[0], args[1]));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, zero));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, zero_one));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decrefN(N(f, zero, zero_one));
    }

    // def f(*args, a)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a}));

        // Being a keyword-only parameter, a cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *args)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}));
        ypObject *one = yp_tupleN(N(args[1]));
        ypObject *one_two = yp_tupleN(N(args[1], args[2]));

        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], one));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], one_two));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, one, one_two));
    }

    // def f(a, *args, b)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}, {str_b}));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *args, b=1)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}, {str_b, defs[1]}));
        ypObject *one = yp_tupleN(N(args[1]));
        ypObject *one_two = yp_tupleN(N(args[1], args[2]));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], one, defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], one_two, defs[1]));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, one, one_two));
    }

    // def f(*args, a=0)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a, defs[0]}));
        ypObject *zero = yp_tupleN(N(args[0]));
        ypObject *zero_one = yp_tupleN(N(args[0], args[1]));

        // Being a keyword-only parameter, a cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, yp_tuple_empty, defs[0]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, zero, defs[0]));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, zero_one, defs[0]));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, zero, zero_one));
    }

    // def f(a=0, *args)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_args}));
        ypObject *one = yp_tupleN(N(args[1]));
        ypObject *one_two = yp_tupleN(N(args[1], args[2]));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], one));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], one_two));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, one, one_two));
    }

    // def f(*args, a=0, b)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a, defs[0]}, {str_b}));

        // Being keyword-only parameters, a and b cannot be set from "callN" positional arguments.
        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a=0, b=1)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a, defs[0]}, {str_b, defs[1]}));
        ypObject *zero = yp_tupleN(N(args[0]));
        ypObject *zero_one = yp_tupleN(N(args[0], args[1]));
        ypObject *zero_one_two = yp_tupleN(N(args[0], args[1], args[2]));

        // Being keyword-only parameters, a and b cannot be set from "callN" positional arguments.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, yp_tuple_empty, defs[0], defs[1]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, zero, defs[0], defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, zero_one, defs[0], defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, zero_one_two, defs[0], defs[1]));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, zero, zero_one, zero_one_two));
    }

    // def f(a=0, *args, b)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_args}, {str_b}));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *args, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_args}, {str_b, defs[1]}));
        ypObject *one = yp_tupleN(N(args[1]));
        ypObject *one_two = yp_tupleN(N(args[1], args[2]));

        // Being a keyword-only parameter, b cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], yp_tuple_empty, defs[1]));
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], one, defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], one_two, defs[1]));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, one, one_two));
    }

    // def f(a=0, b=1, *args)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}, {str_star_args}));
        ypObject *two = yp_tupleN(N(args[2]));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], defs[1], yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], defs[1], yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], args[1], yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], args[1], two));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], args[1], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, two));
    }

    // def f(**kwargs)
    {
        define_function(f, capture_code, ({str_star_star_kwargs}));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, yp_frozendict_empty));
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 0, is, yp_frozendict_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, **kwargs)
    {
        define_function(f, capture_code, ({str_a}, {str_star_star_kwargs}));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], yp_frozendict_empty));
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_frozendict_empty));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, **kwargs)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_star_kwargs}));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], yp_frozendict_empty));
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 1, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_frozendict_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(yp_NameError, args[1])), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, **kwargs)
    {
        define_function(f, capture_code, ({str_star_args}, {str_star_star_kwargs}));
        ypObject *zero = yp_tupleN(N(args[0]));
        ypObject *zero_one = yp_tupleN(N(args[0], args[1]));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, yp_tuple_empty, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, zero, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, zero_one, yp_frozendict_empty));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callN(f, 0), assert_captured_arg(capt, 1, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured_arg(capt, 1, is, yp_frozendict_empty));

        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        yp_decrefN(N(f, zero, zero_one));
    }

    // def f(a, *args, **kwargs)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}, {str_star_star_kwargs}));
        ypObject *one = yp_tupleN(N(args[1]));
        ypObject *one_two = yp_tupleN(N(args[1], args[2]));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], one, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], one_two, yp_frozendict_empty));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 1, is, yp_tuple_empty));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 2, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured_arg(capt, 2, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured_arg(capt, 2, is, yp_frozendict_empty));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, one, one_two));
    }

    // def f(a, /, *args, **kwargs)
    {
        define_function(
                f, capture_code, ({str_a}, {str_slash}, {str_star_args}, {str_star_star_kwargs}));
        ypObject *one = yp_tupleN(N(args[1]));
        ypObject *one_two = yp_tupleN(N(args[1], args[2]));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, N(args[0])),
                assert_captured(
                        capt, f, args[0], captured_NULL, yp_tuple_empty, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], captured_NULL, one, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured(capt, f, args[0], captured_NULL, one_two, yp_frozendict_empty));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 2, is, yp_tuple_empty));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callN(f, N(args[0])), assert_captured_arg(capt, 3, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured_arg(capt, 3, is, yp_frozendict_empty));
        ead(capt, any_callN(f, N(args[0], args[1], args[2])),
                assert_captured_arg(capt, 3, is, yp_frozendict_empty));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);
        assert_raises(any_callN(f, N(args[0], yp_NameError)), yp_NameError);

        yp_decrefN(N(f, one, one_two));
    }

    // f in arguments
    {
        define_function(f, capture_code, ({str_a}));
        ead(capt, any_callN(f, N(f)), assert_captured(capt, f, f));
        yp_decref(f);
    }

    // More than ypFunction_MAX_ARGS_ON_STACK parameters
    {
        // params has ypFunction_MAX_ARGS_ON_STACK + 1 elements. Note params contains new refs.
        yp_parameter_decl_t params[32 + 1] = {{yp_incref(str_a)}};
        yp_ssize_t          params_len = yp_lengthof_array(params);
        yp_function_decl_t  decl = {capture_code, 0, (yp_int32_t)params_len, params, NULL, NULL};
        ypObject           *f;
        parameter_decl_array_fill4(uq, params_len - 1, params + 1, "b");
        assert_not_raises(f = yp_functionC(&decl));

        ead(capt, any_callN(f, N(args[0])),
                assert_captured(capt, f, args[0], params[1].default_, params[2].default_,
                        params[3].default_, params[4].default_, params[5].default_,
                        params[6].default_, params[7].default_, params[8].default_,
                        params[9].default_, params[10].default_, params[11].default_,
                        params[12].default_, params[13].default_, params[14].default_,
                        params[15].default_, params[16].default_, params[17].default_,
                        params[18].default_, params[19].default_, params[20].default_,
                        params[21].default_, params[22].default_, params[23].default_,
                        params[24].default_, params[25].default_, params[26].default_,
                        params[27].default_, params[28].default_, params[29].default_,
                        params[30].default_, params[31].default_, params[32].default_));
        ead(capt,
                any_callN(f,
                        N(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
                                args[8], args[9], args[10], args[11], args[12], args[13], args[14],
                                args[15], args[16], args[17], args[18], args[19], args[20],
                                args[21], args[22], args[23], args[24], args[25], args[26],
                                args[27], args[28], args[29], args[30], args[31], args[32])),
                assert_captured(capt, f, args[0], args[1], args[2], args[3], args[4], args[5],
                        args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13],
                        args[14], args[15], args[16], args[17], args[18], args[19], args[20],
                        args[21], args[22], args[23], args[24], args[25], args[26], args[27],
                        args[28], args[29], args[30], args[31], args[32]));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(yp_NameError)), yp_NameError);

        parameter_decl_array_decref(params);
        yp_decref(f);
    }

    // yp_t_type: pos-only self, pos-only
    {
        ead(result, any_callN(yp_t_type, N(str_a)), assert_obj(result, eq, yp_t_str));
        ead(result, any_callN(yp_t_type, N(yp_t_str)), assert_obj(result, eq, yp_t_type));

        assert_raises(any_callN(yp_t_type, 0), yp_TypeError);
        assert_raises(any_callN(yp_t_type, N(str_a, str_b)), yp_TypeError);
        assert_raises(any_callN(yp_t_type, N(yp_NameError)), yp_NameError);
    }

    // yp_t_bool: pos-only self, pos-only, default
    {
        ead(result, any_callN(yp_t_bool, 0), assert_obj(result, eq, yp_False));
        ead(result, any_callN(yp_t_bool, N(yp_str_empty)), assert_obj(result, eq, yp_False));
        ead(result, any_callN(yp_t_bool, N(str_a)), assert_obj(result, eq, yp_True));

        assert_raises(any_callN(yp_t_bool, N(str_a, str_b)), yp_TypeError);
        assert_raises(any_callN(yp_t_bool, N(yp_NameError)), yp_NameError);
    }

    // yp_t_int: pos-only self, pos-only, pos-or-kw, default
    {
        ypObject *str_two_two = yp_str_frombytesC2(-1, "22");
        ypObject *int_twenty_two = yp_intC(22);
        ypObject *int_three = yp_intC(3);
        ypObject *int_eight = yp_intC(8);

        ead(result, any_callN(yp_t_int, 0), assert_obj(result, eq, yp_i_zero));
        ead(result, any_callN(yp_t_int, N(yp_i_one)), assert_obj(result, eq, yp_i_one));
        ead(result, any_callN(yp_t_int, N(str_two_two)), assert_obj(result, eq, int_twenty_two));
        ead(result, any_callN(yp_t_int, N(str_two_two, yp_None)),
                assert_obj(result, eq, int_twenty_two));
        ead(result, any_callN(yp_t_int, N(str_two_two, int_three)),
                assert_obj(result, eq, int_eight));

        assert_raises(any_callN(yp_t_int, N(str_two_two, int_three, int_three)), yp_TypeError);
        assert_raises(any_callN(yp_t_int, N(yp_NameError)), yp_NameError);

        yp_decrefN(N(str_two_two, int_twenty_two, int_three, int_eight));
    }

    // yp_t_bytes: pos-only self, pos-or-kw, default
    {
        ypObject *bytes_a = yp_bytesC(-1, "a");

        ead(result, any_callN(yp_t_bytes, 0), assert_obj(result, eq, yp_bytes_empty));
        ead(result, any_callN(yp_t_bytes, N(bytes_a)), assert_obj(result, eq, bytes_a));
        ead(result, any_callN(yp_t_bytes, N(str_a, yp_s_utf_8)), assert_obj(result, eq, bytes_a));
        ead(result, any_callN(yp_t_bytes, N(str_a, yp_s_utf_8, yp_s_strict)),
                assert_obj(result, eq, bytes_a));

        assert_raises(
                any_callN(yp_t_bytes, N(str_a, yp_s_utf_8, yp_s_strict, bytes_a)), yp_TypeError);
        assert_raises(any_callN(yp_t_bytes, N(yp_NameError)), yp_NameError);

        yp_decrefN(N(bytes_a));
    }

    // yp_t_frozendict: pos-only self, pos-only, **kwargs, default
    {
        ypObject *tuple_a = yp_tupleN(N(str_a, args[0]));
        ypObject *tuple_tuple_a = yp_tupleN(N(tuple_a));
        ypObject *frozendict_a = yp_frozendictK(K(str_a, args[0]));

        // **kwargs cannot be set from a "callN" positional argument.
        ead(result, any_callN(yp_t_frozendict, 0), assert_obj(result, eq, yp_frozendict_empty));
        ead(result, any_callN(yp_t_frozendict, N(tuple_tuple_a)),
                assert_obj(result, eq, frozendict_a));
        ead(result, any_callN(yp_t_frozendict, N(frozendict_a)),
                assert_obj(result, eq, frozendict_a));

        assert_raises(any_callN(yp_t_frozendict, N(tuple_tuple_a, frozendict_a)), yp_TypeError);
        assert_raises(any_callN(yp_t_frozendict, N(yp_NameError)), yp_NameError);

        yp_decrefN(N(tuple_a, tuple_tuple_a, frozendict_a));
    }

    // yp_t_function: pos-only self, *args, **kwargs (because it's currently not implemented)
    {
        // **kwargs cannot be set from a "callN" positional argument.
        assert_raises(any_callN(yp_t_function, 0), yp_NotImplementedError);
        assert_raises(any_callN(yp_t_function, N(str_a)), yp_NotImplementedError);
        assert_raises(any_callN(yp_t_function, N(str_a, str_b)), yp_NotImplementedError);

        assert_raises(any_callN(yp_t_function, N(yp_NameError)), yp_NameError);
    }

    // Calling an exception.
    {
        assert_raises(any_callN(yp_NameError, 0), yp_NameError);
        assert_raises(any_callN(yp_NameError, N(yp_None)), yp_NameError);
        assert_raises(any_callN(yp_NameError, N(yp_tuple_empty)), yp_NameError);

        // yp_OSError holds no special meaning in the function implementation or these tests.
        assert_raises(any_callN(yp_OSError, 0), yp_OSError);
        assert_raises(any_callN(yp_OSError, N(yp_None)), yp_OSError);
        assert_raises(any_callN(yp_OSError, N(yp_tuple_empty)), yp_OSError);
    }

    // None_code
    {
        define_function2(f, None_code);
        assert_obj(any_callN(f, 0), is, yp_None);
        yp_decref(f);
    }

    obj_array_decref(defs);
    obj_array_decref(args);
    yp_decrefN(N(str_a, str_b, str_slash, str_star, str_star_args, str_star_star_kwargs));
    uniqueness_dealloc(uq);
}

// Tests common to test_callK (not yet implemented) and test_call_stars.
static void _test_callK(ypObject *(*any_callK)(ypObject *, int, ...))
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defs[] = obj_array_init(5, rand_obj_any(uq));
    ypObject     *args[] = obj_array_init(33, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    ypObject     *str_b = yp_str_frombytesC2(-1, "b");
    ypObject     *str_c = yp_str_frombytesC2(-1, "c");
    ypObject     *str_slash = yp_str_frombytesC2(-1, "/");
    ypObject     *str_star = yp_str_frombytesC2(-1, "*");
    ypObject     *str_star_args = yp_str_frombytesC2(-1, "*args");
    ypObject     *str_star_star_kwargs = yp_str_frombytesC2(-1, "**kwargs");
    ypObject     *non_string = rand_obj_any_hashable_not_str(uq);  // argument names must be strings

    // def f()
    {
        define_function2(f, capture_code);

        ead(capt, any_callK(f, 0), assert_captured_is(capt, f, 0, NULL));

        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a)
    {
        define_function(f, capture_code, ({str_a}));

        ead(capt, any_callK(f, K(str_a, args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, b)
    {
        define_function(f, capture_code, ({str_a}, {str_b}));

        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0)
    {
        define_function(f, capture_code, ({str_a, defs[0]}));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0]));
        ead(capt, any_callK(f, K(str_a, args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=yp_NameError)
    {
        define_function(f, capture_code, ({str_a, yp_NameError}));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, captured_NameError));
        ead(capt, any_callK(f, K(str_a, args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0])), assert_captured(capt, f, args[0], defs[1]));
        ead(capt, any_callK(f, K(str_b, args[1])), assert_captured(capt, f, defs[0], args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, /)
    {
        define_function(f, capture_code, ({str_a}, {str_slash}));

        // Being a positional-only parameter, a cannot be set from a "callK" keyword argument.
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError, yp_TypeError);

        yp_decref(f);
    }

    // def f(a=0, /)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_slash}));

        // Being a positional-only parameter, a cannot be set from a "callK" keyword argument. The
        // trailing NULL is trimmed from argarray, so argarray will have one element.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0]));
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);

        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, /, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_slash}, {str_b, defs[1]}));

        // Being a positional-only parameter, a cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], captured_NULL, defs[1]));
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], captured_NULL, args[1]));
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);

        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, b=1, /)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}, {str_slash}));

        // Being positional-only parameters, a and b cannot be set from "callK" keyword arguments.
        // The trailing NULL is trimmed from argarray, so argarray will have two elements.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], defs[1]));
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);

        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a)
    {
        define_function(f, capture_code, ({str_star}, {str_a}));

        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a, b)
    {
        define_function(f, capture_code, ({str_star}, {str_a}, {str_b}));

        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a, b=1)
    {
        define_function(f, capture_code, ({str_star}, {str_a}, {str_b, defs[1]}));

        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0], defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *, b)
    {
        define_function(f, capture_code, ({str_a}, {str_star}, {str_b}));

        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *, b=1)
    {
        define_function(f, capture_code, ({str_a}, {str_star}, {str_b, defs[1]}));

        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], captured_NULL, defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a=0)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, captured_NULL, defs[0]));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0]));

        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a=0, b)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}, {str_b}));

        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, captured_NULL, defs[0], args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*, a=0, b=1)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}, {str_b, defs[1]}));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, captured_NULL, defs[0], defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0], defs[1]));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, captured_NULL, defs[0], args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, captured_NULL, args[0], args[1]));

        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *, b)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star}, {str_b}));

        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], captured_NULL, args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star}, {str_b, defs[1]}));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], captured_NULL, defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], captured_NULL, defs[1]));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], captured_NULL, args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callK(f, K(str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args)
    {
        define_function(f, capture_code, ({str_star_args}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, yp_tuple_empty));
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0]));
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, args[0])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError, str_a, args[0])), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *args)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty));
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, args[0])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError, str_a, args[0])), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a, b)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a}, {str_b}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_c, args[2], str_a, args[0], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a, b=1)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a}, {str_b, defs[1]}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0], defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);
        assert_raises(
                any_callK(f, K(str_c, args[2], str_a, args[0], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *args, b)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}, {str_b}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        assert_raises(
                any_callK(f, K(str_a, args[0], str_c, args[2], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a, *args, b=1)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}, {str_b, defs[1]}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        assert_raises(
                any_callK(f, K(str_a, args[0], str_c, args[2], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a=0)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a, defs[0]}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, yp_tuple_empty, defs[0]));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0]));
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, args[0])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError, str_a, args[0])), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *args)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_args}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty));
        assert_raises(any_callK(f, K(str_a, args[0], str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, args[0])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_b, args[1], str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_b, yp_NameError, str_a, args[0])), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a=0, b)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a, defs[0]}, {str_b}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, defs[0], args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        assert_raises(
                any_callK(f, K(str_c, args[2], str_a, args[0], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(*args, a=0, b=1)
    {
        define_function(f, capture_code, ({str_star_args}, {str_a, defs[0]}, {str_b, defs[1]}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, yp_tuple_empty, defs[0], defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0], defs[1]));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, defs[0], args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, args[0], args[1]));
        assert_raises(
                any_callK(f, K(str_c, args[2], str_a, args[0], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *args, b)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_args}, {str_b}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], yp_tuple_empty, args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        assert_raises(
                any_callK(f, K(str_c, args[2], str_a, args[0], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, *args, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_star_args}, {str_b, defs[1]}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], yp_tuple_empty, defs[1]));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, defs[1]));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], yp_tuple_empty, args[1]));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, args[1]));
        assert_raises(
                any_callK(f, K(str_a, args[0], str_c, args[2], str_b, args[1])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(a=0, b=1, *args)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}, {str_star_args}));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, defs[0], defs[1], yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], defs[1], yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], args[1], yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], args[1], yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], args[1], yp_tuple_empty));
        assert_raises(
                any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])), yp_TypeError);

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError, str_b, args[1])), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);

        yp_decref(f);
    }

    // def f(**kwargs)
    {
        define_function(f, capture_code, ({str_star_star_kwargs}));
        ypObject *zero = yp_frozendictK(K(str_a, args[0]));
        ypObject *one = yp_frozendictK(K(str_b, args[1]));
        ypObject *zero_one = yp_frozendictK(K(str_a, args[0], str_b, args[1]));

        ead(capt, any_callK(f, 0), assert_captured(capt, f, yp_frozendict_empty));
        ead(capt, any_callK(f, K(str_a, args[0])), assert_captured(capt, f, zero));
        ead(capt, any_callK(f, K(str_b, args[1])), assert_captured(capt, f, one));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, zero_one));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, zero_one));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 0, is, yp_frozendict_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(non_string, args[0])), yp_TypeError);

        yp_decrefN(N(f, zero, one, zero_one));
    }

    // def f(a, **kwargs)
    {
        define_function(f, capture_code, ({str_a}, {str_star_star_kwargs}));
        ypObject *one = yp_frozendictK(K(str_b, args[1]));

        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], yp_frozendict_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], one));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], one));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 1, is, yp_frozendict_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], non_string, args[1])), yp_TypeError);

        yp_decrefN(N(f, one));
    }

    // def f(*args, **kwargs)
    {
        define_function(f, capture_code, ({str_star_args}, {str_star_star_kwargs}));
        ypObject *zero = yp_frozendictK(K(str_a, args[0]));
        ypObject *one = yp_frozendictK(K(str_b, args[1]));
        ypObject *zero_one = yp_frozendictK(K(str_a, args[0], str_b, args[1]));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, 0), assert_captured(capt, f, yp_tuple_empty, yp_frozendict_empty));
        ead(capt, any_callK(f, K(str_a, args[0])), assert_captured(capt, f, yp_tuple_empty, zero));
        ead(capt, any_callK(f, K(str_b, args[1])), assert_captured(capt, f, yp_tuple_empty, one));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, yp_tuple_empty, zero_one));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, yp_tuple_empty, zero_one));

        // f(args=0, kwargs=1) sets **kwargs to {"args": 0, "kwargs": 1}.
        {
            ypObject *str_args = yp_str_frombytesC2(-1, "args");
            ypObject *str_kwargs = yp_str_frombytesC2(-1, "kwargs");
            ypObject *expected = yp_frozendictK(K(str_args, args[0], str_kwargs, args[1]));
            ead(capt, any_callK(f, K(str_args, args[0], str_kwargs, args[1])),
                    assert_captured(capt, f, yp_tuple_empty, expected));
            yp_decrefN(N(str_args, str_kwargs, expected));
        }

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 0, is, yp_tuple_empty));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 1, is, yp_frozendict_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(non_string, args[0])), yp_TypeError);

        yp_decrefN(N(f, zero, one, zero_one));
    }

    // def f(a, *args, **kwargs)
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}, {str_star_star_kwargs}));
        ypObject *one = yp_frozendictK(K(str_b, args[1]));
        ypObject *one_two = yp_frozendictK(K(str_b, args[1], str_c, args[2]));

        // *args cannot be set from a "callK" keyword argument.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, yp_frozendict_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], yp_tuple_empty, one));
        ead(capt, any_callK(f, K(str_b, args[1], str_a, args[0])),
                assert_captured(capt, f, args[0], yp_tuple_empty, one));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])),
                assert_captured(capt, f, args[0], yp_tuple_empty, one_two));
        ead(capt, any_callK(f, K(str_c, args[2], str_a, args[0], str_b, args[1])),
                assert_captured(capt, f, args[0], yp_tuple_empty, one_two));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0], str_b, args[1], str_c, args[2])),
                assert_captured_arg(capt, 1, is, yp_tuple_empty));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 2, is, yp_frozendict_empty));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_b, args[1])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], non_string, args[1])), yp_TypeError);

        yp_decrefN(N(f, one, one_two));
    }

    // def f(a=0, /, *args, **kwargs)
    {
        define_function(f, capture_code,
                ({str_a, defs[0]}, {str_slash}, {str_star_args}, {str_star_star_kwargs}));
        ypObject *zero = yp_frozendictK(K(str_a, args[0]));
        ypObject *one = yp_frozendictK(K(str_b, args[1]));
        ypObject *one_two = yp_frozendictK(K(str_b, args[1], str_c, args[2]));

        // *args cannot be set from a "callK" keyword argument. Positional-only parameter names
        // do not conflict with keyword arguments of the same name.
        ead(capt, any_callK(f, 0),
                assert_captured(
                        capt, f, defs[0], captured_NULL, yp_tuple_empty, yp_frozendict_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, defs[0], captured_NULL, yp_tuple_empty, zero));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured(capt, f, defs[0], captured_NULL, yp_tuple_empty, one));
        ead(capt, any_callK(f, K(str_b, args[1], str_c, args[2])),
                assert_captured(capt, f, defs[0], captured_NULL, yp_tuple_empty, one_two));
        ead(capt, any_callK(f, K(str_c, args[2], str_b, args[1])),
                assert_captured(capt, f, defs[0], captured_NULL, yp_tuple_empty, one_two));

        // Optimization: empty *args is always yp_tuple_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));
        ead(capt, any_callK(f, K(str_b, args[1], str_c, args[2])),
                assert_captured_arg(capt, 2, is, yp_tuple_empty));

        // Optimization: empty **kwargs is always yp_frozendict_empty.
        ead(capt, any_callK(f, 0), assert_captured_arg(capt, 3, is, yp_frozendict_empty));

        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(str_a, args[0], str_b, yp_NameError)), yp_NameError);
        assert_raises(any_callK(f, K(non_string, args[0])), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, args[0], non_string, args[1])), yp_TypeError);

        yp_decrefN(N(f, zero, one, one_two));
    }

    // f in arguments
    {
        define_function(f, capture_code, ({str_a}));
        assert_raises(any_callK(f, K(f, f)), yp_TypeError);
        ead(capt, any_callK(f, K(str_a, f)), assert_captured(capt, f, f));
        yp_decref(f);
    }

    // Keyword argument names can be any string, including non-identifiers
    {
        define_function(f, capture_code, ({str_star_star_kwargs}));
        char *strings[] = {
                "",       // empty string
                " ",      // whitespace only
                "0name",  // digit as first character
                "na:me",  // non-alphanumeric character
        };
        int i;

        for (i = 0; i < yp_lengthof_array(strings); i++) {
            ypObject *name = yp_str_frombytesC2(-1, strings[i]);
            ypObject *expected = yp_frozendictK(K(name, args[0]));

            ead(capt, any_callK(f, K(name, args[0])), assert_captured(capt, f, expected));

            yp_decrefN(N(name, expected));
        }

        yp_decrefN(N(f));
    }

    // More than ypFunction_MAX_ARGS_ON_STACK parameters
    {
        // params has ypFunction_MAX_ARGS_ON_STACK + 1 elements. Note params contains new refs.
        yp_parameter_decl_t params[32 + 1] = {{yp_incref(str_a)}};
        yp_ssize_t          params_len = yp_lengthof_array(params);
        yp_function_decl_t  decl = {capture_code, 0, (yp_int32_t)params_len, params, NULL, NULL};
        ypObject           *f;
        parameter_decl_array_fill4(uq, params_len - 1, params + 1, "b");
        assert_not_raises(f = yp_functionC(&decl));

        ead(capt, any_callK(f, K(str_a, args[0])),
                assert_captured(capt, f, args[0], params[1].default_, params[2].default_,
                        params[3].default_, params[4].default_, params[5].default_,
                        params[6].default_, params[7].default_, params[8].default_,
                        params[9].default_, params[10].default_, params[11].default_,
                        params[12].default_, params[13].default_, params[14].default_,
                        params[15].default_, params[16].default_, params[17].default_,
                        params[18].default_, params[19].default_, params[20].default_,
                        params[21].default_, params[22].default_, params[23].default_,
                        params[24].default_, params[25].default_, params[26].default_,
                        params[27].default_, params[28].default_, params[29].default_,
                        params[30].default_, params[31].default_, params[32].default_));
        ead(capt,
                any_callK(f,
                        K(str_a, args[0], params[1].name, args[1], params[2].name, args[2],
                                params[3].name, args[3], params[4].name, args[4], params[5].name,
                                args[5], params[6].name, args[6], params[7].name, args[7],
                                params[8].name, args[8], params[9].name, args[9], params[10].name,
                                args[10], params[11].name, args[11], params[12].name, args[12],
                                params[13].name, args[13], params[14].name, args[14],
                                params[15].name, args[15], params[16].name, args[16],
                                params[17].name, args[17], params[18].name, args[18],
                                params[19].name, args[19], params[20].name, args[20],
                                params[21].name, args[21], params[22].name, args[22],
                                params[23].name, args[23], params[24].name, args[24],
                                params[25].name, args[25], params[26].name, args[26],
                                params[27].name, args[27], params[28].name, args[28],
                                params[29].name, args[29], params[30].name, args[30],
                                params[31].name, args[31], params[32].name, args[32])),
                assert_captured(capt, f, args[0], args[1], args[2], args[3], args[4], args[5],
                        args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13],
                        args[14], args[15], args[16], args[17], args[18], args[19], args[20],
                        args[21], args[22], args[23], args[24], args[25], args[26], args[27],
                        args[28], args[29], args[30], args[31], args[32]));

        assert_raises(any_callK(f, 0), yp_TypeError);
        assert_raises(any_callK(f, K(str_a, yp_NameError)), yp_NameError);

        parameter_decl_array_decref(params);
        yp_decref(f);
    }

    // yp_t_type: pos-only self, pos-only
    {
        ypObject *str_object = yp_str_frombytesC2(-1, "object");

        // Positional-only parameters cannot be set from "callK" keyword arguments.
        assert_raises(any_callK(yp_t_type, 0), yp_TypeError);
        assert_raises(any_callK(yp_t_type, K(str_object, str_a)), yp_TypeError);
        assert_raises(
                any_callK(yp_t_type, K(str_object, yp_NameError)), yp_NameError, yp_TypeError);

        yp_decrefN(N(str_object));
    }

    // yp_t_bool: pos-only self, pos-only, default
    {
        ypObject *str_x = yp_str_frombytesC2(-1, "x");

        // Positional-only parameters cannot be set from "callK" keyword arguments.
        ead(result, any_callK(yp_t_bool, 0), assert_obj(result, eq, yp_False));

        assert_raises(any_callK(yp_t_bool, K(str_x, yp_str_empty)), yp_TypeError);
        assert_raises(any_callK(yp_t_bool, K(str_x, str_a)), yp_TypeError);
        assert_raises(any_callK(yp_t_bool, K(str_x, yp_NameError)), yp_NameError);

        yp_decrefN(N(str_x));
    }

    // yp_t_int: pos-only self, pos-only, pos-or-kw, default
    {
        ypObject *str_x = yp_str_frombytesC2(-1, "x");
        ypObject *str_base = yp_str_frombytesC2(-1, "base");
        ypObject *str_two_two = yp_str_frombytesC2(-1, "22");
        ypObject *int_three = yp_intC(3);

        // Positional-only parameters cannot be set from "callK" keyword arguments. Recall that
        // base requires x to be a bytes/str/etc, which also raises yp_TypeError.
        ead(result, any_callK(yp_t_int, 0), assert_obj(result, eq, yp_i_zero));
        ead(result, any_callK(yp_t_int, K(str_base, yp_None)), assert_obj(result, eq, yp_i_zero));
        assert_raises(any_callK(yp_t_int, K(str_base, int_three)), yp_TypeError);

        assert_raises(any_callK(yp_t_int, K(str_x, str_two_two)), yp_TypeError);
        assert_raises(
                any_callK(yp_t_int, K(str_x, str_two_two, str_base, int_three)), yp_TypeError);
        assert_raises(any_callK(yp_t_int, K(str_x, yp_NameError)), yp_NameError);

        yp_decrefN(N(str_x, str_base, str_two_two, int_three));
    }

    // yp_t_bytes: pos-only self, pos-or-kw, default
    {
        ypObject *str_source = yp_str_frombytesC2(-1, "source");
        ypObject *str_encoding = yp_str_frombytesC2(-1, "encoding");
        ypObject *str_errors = yp_str_frombytesC2(-1, "errors");
        ypObject *bytes_a = yp_bytesC(-1, "a");

        ead(result, any_callK(yp_t_bytes, 0), assert_obj(result, eq, yp_bytes_empty));
        ead(result, any_callK(yp_t_bytes, K(str_source, bytes_a)), assert_obj(result, eq, bytes_a));
        ead(result, any_callK(yp_t_bytes, K(str_source, str_a, str_encoding, yp_s_utf_8)),
                assert_obj(result, eq, bytes_a));
        ead(result,
                any_callK(yp_t_bytes,
                        K(str_source, str_a, str_encoding, yp_s_utf_8, str_errors, yp_s_strict)),
                assert_obj(result, eq, bytes_a));

        assert_raises(any_callK(yp_t_bytes, K(str_source, str_a, str_encoding, yp_s_utf_8,
                                                    str_errors, yp_s_strict, str_a, bytes_a)),
                yp_TypeError);
        assert_raises(any_callK(yp_t_bytes, K(str_source, yp_NameError)), yp_NameError);

        yp_decrefN(N(str_source, str_encoding, str_errors, bytes_a));
    }

    // yp_t_frozendict: pos-only self, pos-only, **kwargs, default
    {
        ypObject *str_object = yp_str_frombytesC2(-1, "object");  // name of pos-only param
        ypObject *frozendict_a = yp_frozendictK(K(str_a, args[0]));
        ypObject *frozendict_object_a = yp_frozendictK(K(str_object, frozendict_a, str_a, args[0]));

        // Positional-only parameter names do not conflict with keyword arguments of the same name.
        ead(result, any_callK(yp_t_frozendict, 0), assert_obj(result, eq, yp_frozendict_empty));
        ead(result, any_callK(yp_t_frozendict, K(str_a, args[0])),
                assert_obj(result, eq, frozendict_a));
        ead(result, any_callK(yp_t_frozendict, K(str_object, frozendict_a, str_a, args[0])),
                assert_obj(result, eq, frozendict_object_a));

        assert_raises(any_callK(yp_t_frozendict, K(str_a, yp_NameError)), yp_NameError);

        yp_decrefN(N(str_object, frozendict_a, frozendict_object_a));
    }

    // yp_t_function: pos-only self, *args, **kwargs (because it's currently not implemented)
    {
        // *args cannot be set from a "callK" keyword argument.
        assert_raises(any_callK(yp_t_function, 0), yp_NotImplementedError);
        assert_raises(any_callK(yp_t_function, K(str_a, args[0])), yp_NotImplementedError);
        assert_raises(any_callK(yp_t_function, K(str_a, args[0], str_b, args[1])),
                yp_NotImplementedError);

        assert_raises(any_callK(yp_t_function, K(str_a, yp_NameError)), yp_NameError);
    }

    // Calling an exception.
    {
        assert_raises(any_callK(yp_NameError, 0), yp_NameError);
        assert_raises(any_callK(yp_NameError, K(str_a, args[0])), yp_NameError);

        // yp_OSError holds no special meaning in the function implementation or these tests.
        assert_raises(any_callK(yp_OSError, 0), yp_OSError);
        assert_raises(any_callK(yp_OSError, K(str_a, args[0])), yp_OSError);
    }

    // None_code
    {
        define_function2(f, None_code);
        assert_obj(any_callK(f, 0), is, yp_None);
        yp_decref(f);
    }

    // TODO PEP 448 talks about the confusion around {'a':0,'a':1} being valid but dict(a=0,a=1)
    // being invalid. Figure out how we should be handling duplicate keys. Regardless, we can't
    // currently test for this as the only "yp_callK" we have right now is yp_call_stars, and since
    // it accepts dicts the deduplication has already occurred.

    obj_array_decref(defs);
    obj_array_decref(args);
    yp_decrefN(N(str_a, str_b, str_c, str_slash, str_star, str_star_args, str_star_star_kwargs,
            non_string));
    uniqueness_dealloc(uq);
}

static MunitResult test_callN(const MunitParameter params[], fixture_t *fixture)
{
    ypObject *str_a = yp_str_frombytesC2(-1, "a");

    // Shared tests.
    _test_callN(yp_callN);

    yp_decrefN(N(str_a));
    return MUNIT_OK;
}

// Accepts the yp_callN signature and instead calls yp_call_stars with a tuple. For use in
// _test_callN. n must be >= 0.
static ypObject *callN_to_call_stars_tuple(ypObject *c, int n, ...)
{
    va_list   args;
    ypObject *as_tuple;
    ypObject *result;

    // If args contains an exception, as_tuple will be that exception. We could assert_not_raises
    // here, but this tests that yp_call_stars fails if args is an exception.
    va_start(args, n);
    as_tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_call_stars(c, as_tuple, yp_frozendict_empty);
    yp_decref(as_tuple);
    return result;
}

// Accepts the yp_callN signature and instead calls yp_call_stars with an iterator. For use in
// _test_callN. n must be >= 0.
static ypObject *callN_to_call_stars_iter(ypObject *c, int n, ...)
{
    va_list   args;
    ypObject *as_tuple;
    ypObject *as_iter;
    ypObject *result;

    // If args contains an exception, as_iter will be that exception. We could assert_not_raises
    // here, but this tests that yp_call_stars fails if args is an exception.
    va_start(args, n);
    as_tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    as_iter = yp_iter(as_tuple);
    result = yp_call_stars(c, as_iter, yp_frozendict_empty);
    yp_decrefN(N(as_tuple, as_iter));
    return result;
}

// Accepts the "yp_callK" signature (not actually implemented, yet) and instead calls yp_call_stars
// with a frozendict. For use in _test_callK. n must be >= 0.
static ypObject *callK_to_call_stars_frozendict(ypObject *c, int n, ...)
{
    va_list   args;
    ypObject *as_frozendict;
    ypObject *result;

    // If args contains an exception, as_frozendict will be that exception. We could
    // assert_not_raises here, but this tests that yp_call_stars fails if kwargs is an exception.
    va_start(args, n);
    as_frozendict = yp_frozendictKV(n, args);  // new ref
    va_end(args);

    result = yp_call_stars(c, yp_tuple_empty, as_frozendict);
    yp_decref(as_frozendict);
    return result;
}

static MunitResult test_call_stars(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defs[] = obj_array_init(2, rand_obj_any(uq));
    ypObject     *args[] = obj_array_init(2, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    ypObject     *str_b = yp_str_frombytesC2(-1, "b");
    ypObject     *str_slash = yp_str_frombytesC2(-1, "/");
    ypObject     *str_star = yp_str_frombytesC2(-1, "*");
    ypObject     *str_star_args = yp_str_frombytesC2(-1, "*args");
    ypObject     *str_star_star_kwargs = yp_str_frombytesC2(-1, "**kwargs");

    // Shared tests.
    _test_callN(callN_to_call_stars_tuple);
    _test_callN(callN_to_call_stars_iter);
    _test_callK(callK_to_call_stars_frozendict);

    // def f(a) cannot be called like f(0, a=0).
    {
        define_function(f, capture_code, ({str_a}));
        ypObject *call_args = yp_tupleN(N(args[0]));
        ypObject *call_kwargs = yp_frozendictK(K(str_a, args[0]));
        assert_raises(yp_call_stars(f, call_args, call_kwargs), yp_TypeError);
        yp_decrefN(N(f, call_args, call_kwargs));
    }

    // def f(a, **kwargs) cannot be called like f(0, a=0).
    {
        define_function(f, capture_code, ({str_a}, {str_star_star_kwargs}));
        ypObject *call_args = yp_tupleN(N(args[0]));
        ypObject *call_kwargs = yp_frozendictK(K(str_a, args[0]));
        assert_raises(yp_call_stars(f, call_args, call_kwargs), yp_TypeError);
        yp_decrefN(N(f, call_args, call_kwargs));
    }

    // def f(a, /, **kwargs) _can_ be called like f(0, a=1) (or f(0, a=0), for that matter).
    {
        define_function(f, capture_code, ({str_a}, {str_slash}, {str_star_star_kwargs}));
        ypObject *call_args = yp_tupleN(N(args[0]));
        ypObject *call_kwargs = yp_frozendictK(K(str_a, args[1]));
        ead(capt, yp_call_stars(f, call_args, call_kwargs),
                assert_captured(capt, f, args[0], captured_NULL, call_kwargs));
        yp_decrefN(N(f, call_args, call_kwargs));
    }

    // def f(a) cannot be called like f(**((a, 0), )) (** must be a mapping)
    {
        define_function(f, capture_code, ({str_a}));
        ypObject *call_kwargs0 = yp_tupleN(N(str_a, args[0]));
        ypObject *call_kwargs = yp_tupleN(N(call_kwargs0));
        assert_raises(yp_call_stars(f, yp_tuple_empty, call_kwargs), yp_TypeError);
        yp_decrefN(N(f, call_kwargs0, call_kwargs));
    }

    // def f(**kwargs) cannot be called like f(**((a, 0), )) (** must be a mapping)
    {
        define_function(f, capture_code, ({str_star_star_kwargs}));
        ypObject *call_kwargs0 = yp_tupleN(N(str_a, args[0]));
        ypObject *call_kwargs = yp_tupleN(N(call_kwargs0));
        assert_raises(yp_call_stars(f, yp_tuple_empty, call_kwargs), yp_TypeError);
        yp_decrefN(N(f, call_kwargs0, call_kwargs));
    }

    // def f(*args, **kwargs) cannot be called like f(**((a, 0), )) (** must be a mapping)
    {
        define_function(f, capture_code, ({str_star_args}, {str_star_star_kwargs}));
        ypObject *call_kwargs0 = yp_tupleN(N(str_a, args[0]));
        ypObject *call_kwargs = yp_tupleN(N(call_kwargs0));
        assert_raises(yp_call_stars(f, yp_tuple_empty, call_kwargs), yp_TypeError);
        yp_decrefN(N(f, call_kwargs0, call_kwargs));
    }

    // f is args or kwargs
    {
        define_function(f, capture_code, ({str_a}));
        assert_raises(yp_call_stars(f, f, yp_frozendict_empty), yp_TypeError);
        assert_raises(yp_call_stars(f, yp_tuple_empty, f), yp_TypeError);
        assert_raises(yp_call_stars(f, f, f), yp_TypeError);
        yp_decrefN(N(f));
    }

    // Ensure the args input to yp_call_stars is not modified.
    {
        define_function(f, capture_code, ({str_a}, {str_star_args}));
        ypObject *call_args = yp_listN(N(args[0], args[1]));
        ypObject *one = yp_tupleN(N(args[1]));

        ead(capt, yp_call_stars(f, call_args, yp_frozendict_empty),
                assert_captured(capt, f, args[0], one));

        // This is very unlikely to fail, but we need this test case for kwargs, so for completeness
        // we'll test args as well.
        assert_sequence(call_args, args[0], args[1]);

        yp_decrefN(N(f, call_args, one));
    }

    // Ensure the kwargs input to yp_call_stars is not modified.
    {
        define_function(f, capture_code, ({str_a}, {str_star_star_kwargs}));
        ypObject *call_kwargs = yp_dictK(K(str_a, args[0], str_b, args[1]));
        ypObject *one = yp_frozendictK(K(str_b, args[1]));

        ead(capt, yp_call_stars(f, yp_tuple_empty, call_kwargs),
                assert_captured(capt, f, args[0], one));

        // If _ypFunction_call_make_var_kwargs neglects to make a copy of call_kwargs, it may
        // accidentally modify it.
        assert_mapping(call_kwargs, str_a, args[0], str_b, args[1]);

        yp_decrefN(N(f, call_kwargs, one));
    }

    // Optimization: the args tuple can sometimes be lazy copied.
    {
        define_function(f_args, capture_code, ({str_star_args}));
        define_function(f_args_a, capture_code, ({str_star_args}, {str_a, defs[0]}));
        define_function(f_args_kwargs, capture_code, ({str_star_args}, {str_star_star_kwargs}));
        ypObject *call_args = yp_tupleN(N(args[0], args[1]));
        ypObject *call_kwargs = yp_frozendictK(K(str_a, args[0]));

        ead(capt, yp_call_stars(f_args, call_args, yp_frozendict_empty),
                assert_captured_arg(capt, 0, is, call_args));
        ead(capt, yp_call_stars(f_args_a, call_args, yp_frozendict_empty),
                assert_captured_arg(capt, 0, is, call_args));
        ead(capt, yp_call_stars(f_args_a, call_args, call_kwargs),
                assert_captured_arg(capt, 0, is, call_args));
        ead(capt, yp_call_stars(f_args_kwargs, call_args, yp_frozendict_empty),
                assert_captured_arg(capt, 0, is, call_args));
        ead(capt, yp_call_stars(f_args_kwargs, call_args, call_kwargs),
                assert_captured_arg(capt, 0, is, call_args));

        // TODO There's currently no callable object (withself) with *args.

        yp_decrefN(N(f_args, f_args_a, f_args_kwargs, call_args, call_kwargs));
    }

    // Optimization: the kwargs frozendict can sometimes be lazy copied.
    {
        define_function(f_kwargs, capture_code, ({str_star_star_kwargs}));
        define_function(f_a_slash_kwargs, capture_code,
                ({str_a, defs[0]}, {str_slash}, {str_star_star_kwargs}));
        define_function(f_b_kwargs, capture_code, ({str_b, defs[1]}, {str_star_star_kwargs}));
        define_function(f_star_b_kwargs, capture_code,
                ({str_star}, {str_b, defs[1]}, {str_star_star_kwargs}));
        define_function(f_args_kwargs, capture_code, ({str_star_args}, {str_star_star_kwargs}));
        ypObject *call_args = yp_tupleN(N(args[0]));
        ypObject *call_kwargs = yp_frozendictK(K(str_a, args[0]));

        ead(capt, yp_call_stars(f_kwargs, yp_tuple_empty, call_kwargs),
                assert_captured_arg(capt, 0, is, call_kwargs));
        ead(capt, yp_call_stars(f_a_slash_kwargs, yp_tuple_empty, call_kwargs),
                assert_captured_arg(capt, 2, is, call_kwargs));
        ead(capt, yp_call_stars(f_a_slash_kwargs, call_args, call_kwargs),
                assert_captured_arg(capt, 2, is, call_kwargs));
        ead(capt, yp_call_stars(f_b_kwargs, yp_tuple_empty, call_kwargs),
                assert_captured_arg(capt, 1, is, call_kwargs));
        ead(capt, yp_call_stars(f_b_kwargs, call_args, call_kwargs),
                assert_captured_arg(capt, 1, is, call_kwargs));
        ead(capt, yp_call_stars(f_star_b_kwargs, yp_tuple_empty, call_kwargs),
                assert_captured_arg(capt, 2, is, call_kwargs));
        ead(capt, yp_call_stars(f_args_kwargs, yp_tuple_empty, call_kwargs),
                assert_captured_arg(capt, 1, is, call_kwargs));
        ead(capt, yp_call_stars(f_args_kwargs, call_args, call_kwargs),
                assert_captured_arg(capt, 1, is, call_kwargs));

        ead(result, yp_call_stars(yp_t_frozendict, yp_tuple_empty, call_kwargs),
                assert_obj(result, is, call_kwargs));

        yp_decrefN(N(f_kwargs, f_a_slash_kwargs, f_b_kwargs, f_star_b_kwargs, f_args_kwargs,
                call_args, call_kwargs));
    }

    obj_array_decref(defs);
    obj_array_decref(args);
    yp_decrefN(N(str_a, str_b, str_slash, str_star, str_star_args, str_star_star_kwargs));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// Accepts the yp_callN signature and instead calls yp_call_arrayX. For use in _test_callN. n must
// be >= 0.
static ypObject *callN_to_call_arrayX(ypObject *c, int n, ...)
{
    va_list   args;
    ypObject *as_array[64];
    int       i;

    assert_ssizeC(yp_lengthof_array(as_array), >=, n + 1);

    va_start(args, n);
    as_array[0] = c;  // the callable is at args[0]
    for (i = 0; i < n; i++) {
        as_array[i + 1] = va_arg(args, ypObject *);  // arguments start at args[1]
    }
    va_end(args);

    return yp_call_arrayX(n + 1, as_array);
}

static MunitResult test_call_arrayX(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *args[] = obj_array_init(2, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    ypObject     *str_b = yp_str_frombytesC2(-1, "b");
    ypObject     *str_slash = yp_str_frombytesC2(-1, "/");

    // Shared tests.
    _test_callN(callN_to_call_arrayX);

    // Optimization: The array can sometimes be used directly as argarray. Based on Python's
    // vectorcall optimization.
    {
        define_function(f_a, capture_code, ({str_a}));
        define_function(f_a_b, capture_code, ({str_a}, {str_b}));
        define_function(f_a_slash, capture_code, ({str_a}, {str_slash}));
        define_function(f_a_b_slash, capture_code, ({str_a}, {str_b}, {str_slash}));
        ypObject *call_f_a[] = {f_a, args[0]};
        ypObject *call_f_a_b[] = {f_a_b, args[0], args[1]};
        ypObject *call_f_a_slash[] = {f_a_slash, args[0]};
        ypObject *call_f_a_b_slash[] = {f_a_b_slash, args[0], args[1]};
        ypObject *call_NoneType[] = {yp_t_NoneType};
        ypObject *call_type[] = {yp_t_type, str_a};
        ypObject *call_iter[] = {yp_t_iter, yp_t_NoneType, yp_None};
        ypObject *call_range[] = {yp_t_range, yp_i_zero, yp_i_two, yp_i_one};

        ead(capt, yp_call_arrayX(2, call_f_a), assert_captured_is(capt, f_a, 1, call_f_a + 1));
        ead(capt, yp_call_arrayX(3, call_f_a_b),
                assert_captured_is(capt, f_a_b, 2, call_f_a_b + 1));
        ead(capt, yp_call_arrayX(2, call_f_a_slash),
                assert_captured_is(capt, f_a_slash, 1, call_f_a_slash + 1));
        ead(capt, yp_call_arrayX(3, call_f_a_b_slash),
                assert_captured_is(capt, f_a_b_slash, 2, call_f_a_b_slash + 1));

        // TODO We need a callable object (withself) that allows us to capture argarray, to truly
        // validate this optimization for such callables.
        ead(result, yp_call_arrayX(1, call_NoneType), assert_obj(result, is, yp_None));
        ead(result, yp_call_arrayX(2, call_type), assert_obj(result, is, yp_t_str));
        ead(result, yp_call_arrayX(3, call_iter), assert_raises(yp_next(result), yp_StopIteration));
        ead(result, yp_call_arrayX(4, call_range), assert_sequence(result, yp_i_zero, yp_i_one));

        yp_decrefN(N(f_a, f_a_b, f_a_slash, f_a_b_slash));
    }

    // n must be >0.
    assert_raises(yp_call_arrayX(0, NULL), yp_TypeError);

    obj_array_decref(args);
    yp_decrefN(N(str_a, str_b, str_slash));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_copy(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defs[] = obj_array_init(1, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    define_function(f, None_code, ({str_a, defs[0]}));

    // All copies are lazy, because all function objects are immutable.
    ead(copy, yp_unfrozen_copy(f), assert_obj(copy, is, f));
    ead(copy, yp_frozen_copy(f), assert_obj(copy, is, f));
    ead(copy, yp_copy(f), assert_obj(copy, is, f));

    obj_array_decref(defs);
    yp_decrefN(N(str_a, f));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_deepcopy(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defs[] = obj_array_init(1, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    define_function(f, None_code, ({str_a, defs[0]}));

    // deepcopy is not yet implemented.
    // TODO In Python, deepcopy on a function is a lazy copy; however, in nohtyP, deepcopy is
    // intended to be a solution for threadsafety by always returning a new object (except, perhaps,
    // for immortals).
    assert_raises(yp_unfrozen_deepcopy(f), yp_NotImplementedError);
    assert_raises(yp_frozen_deepcopy(f), yp_NotImplementedError);
    assert_raises(yp_deepcopy(f), yp_NotImplementedError);

    obj_array_decref(defs);
    yp_decrefN(N(str_a, f));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_bool(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *defs[] = obj_array_init(1, rand_obj_any(uq));
    ypObject     *str_a = yp_str_frombytesC2(-1, "a");
    define_function2(f, None_code);
    define_function(f_a, None_code, ({str_a, defs[0]}));

    // Function objects are always truthy.
    assert_obj(yp_bool(f), is, yp_True);
    assert_obj(yp_bool(f_a), is, yp_True);
    assert_obj(yp_bool(yp_func_hash), is, yp_True);

    obj_array_decref(defs);
    yp_decrefN(N(str_a, f, f_a));
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_hash(const MunitParameter params[], fixture_t *fixture)
{
    define_function2(f1, None_code);
    define_function2(f2, None_code);

    // All function objects are hashable, but they compare by identity only.
    assert_hashC_exc(yp_currenthashC(f1, &exc), !=, yp_currenthashC(f2, &exc));
    assert_hashC_exc(yp_hashC(f1, &exc), !=, yp_hashC(f2, &exc));

    // Function objects can be used as dictionary keys.
    {
        ypObject *mp = yp_dictK(K(f1, yp_i_one));

        assert_not_exception(mp);
        ead(value, yp_getitem(mp, f1), assert_obj(value, is, yp_i_one));

        yp_decrefN(N(mp));
    }

    yp_decrefN(N(f1, f2));
    return MUNIT_OK;
}

static MunitResult test_call_t_function(const MunitParameter params[], fixture_t *fixture)
{
    assert_raises(yp_callN(yp_t_function, 0), yp_NotImplementedError);

    return MUNIT_OK;
}


MunitTest test_function_tests[] = {TEST(test_newC, NULL), TEST(test_new_immortal, NULL),
        TEST(test_callN, NULL), TEST(test_call_stars, NULL), TEST(test_call_arrayX, NULL),
        TEST(test_copy, NULL), TEST(test_deepcopy, NULL), TEST(test_bool, NULL),
        TEST(test_hash, NULL), TEST(test_call_t_function, NULL), {NULL}};


extern void test_function_initialize(void) {}

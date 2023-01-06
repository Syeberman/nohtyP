
#include "munit_test/unittest.h"


typedef struct _signature_t {
    yp_int32_t          n;
    yp_parameter_decl_t params[8];  // Increase length as necessary.
} signature_t;

// Used as the code for a function. Unconditionally returns None.
static ypObject *None_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray) { return yp_None; }


// Used to represent NULL in the captured argarray. Recall functions are compared by identity, so
// this will not be confused with a valid value.
_yp_IMMORTAL_FUNCTION5(static, captured_NULL, None_code, 0, NULL);

// As with captured_NULL, represents yp_NameError/etc in the captured argarray.
_yp_IMMORTAL_FUNCTION5(static, captured_NameError, None_code, 0, NULL);

// Used as the code for a function. Captures all details about the arguments and returns them. NULL
// entries in argarray are replaced with the captured_NULL object.
static ypObject *capture_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ssize_t i;
    ypObject  *argarray_ptr = yp_intC((yp_ssize_t)argarray);  // new ref
    ypObject  *result = yp_listN(N(f, argarray_ptr));
    for (i = 0; i < n; i++) {
        ypObject *arg = argarray[i];
        if (arg == NULL) arg = captured_NULL;
        else if (arg == yp_NameError) arg = captured_NameError;
        assert_not_raises_exc(yp_append(result, arg, &exc));
    }
    yp_decref(argarray_ptr);
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
        _assert_not_exception(_ypmt_CAPT_f, "yp_getindexC(%s, 0)", #captured);                    \
        _assert_not_exception(                                                                    \
                _ypmt_CAPT_args, "yp_getsliceC4(%s, 2, yp_SLICE_LAST, 1)", #captured);            \
        _assert_obj(                                                                              \
                _ypmt_CAPT_f, is, _ypmt_CAPT_f_expected, "<%s f>", "%s", #captured, #f_expected); \
        _assert_sequence(                                                                         \
                _ypmt_CAPT_args, _ypmt_CAPT_items, "<%s args>", _ypmt_CAPT_item_strs, #captured); \
        yp_decrefN(N(_ypmt_CAPT_f, _ypmt_CAPT_args));                                             \
    } while (0)


// Declares a variable name of type ypObject * and initializes it with a new reference to a function
// object. The parameters argument must be surrounded by parentheses.
#define define_function(name, code, parameters)                                                     \
    yp_parameter_decl_t _##name##_parameters[] = {UNPACK parameters};                               \
    yp_function_decl_t  _##name##_declaration = {                                                   \
            (code), 0, yp_lengthof_array(_##name##_parameters), _##name##_parameters, NULL, NULL}; \
    ypObject *name = yp_functionC(&_##name##_declaration)


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
    // FIXME Use define_function somehow?
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
                {3, {{str_star_args}, {str_a}, {str_b}}},                // def f(*args, a, b)
                {3, {{str_a}, {str_star_args}, {str_b}}},                // def f(a, *args, b)
                {2, {{str_star_args}, {str_a, int_0}}},                  // def f(*args, a=0)
                {2, {{str_star_args}, {str_a, yp_NameError}}},           // def f(*args, a=<exc>)
                {3, {{str_star_args}, {str_a, int_0}, {str_b}}},         // def f(*args, a=0, b)
                {3, {{str_star_args}, {str_a, int_0}, {str_b, int_0}}},  // def f(*args, a=0, b=0)
                {3, {{str_a, int_0}, {str_star_args}, {str_b}}},         // def f(a=0, *args, b)
                {3, {{str_a, int_0}, {str_star_args}, {str_b, int_0}}},  // def f(a=0, *args, b=0)
                {1, {{str_star_star_kwargs}}},                           // def f(**kwargs)
                {6, {{str_a}, {str_slash}, {str_b}, {str_star}, {str_c},
                            {str_star_star_kwargs}}},  // def f(a, /, b, *, c, **kwargs)
                {6, {{str_a}, {str_slash}, {str_b}, {str_star_args}, {str_c},
                            {str_star_star_kwargs}}},  // def f(a, /, b, *args, c, **kwargs)
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
        yp_state_decl_t    state_decl = {-1};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl.offsets_len == -1 (array of objects) is not yet implemented.
    {
        yp_state_decl_t    state_decl = {yp_sizeof(ypObject *), -1};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_NotImplementedError);
    }

    // state_decl.offsets_len must be >= 0 or -1.
    {
        yp_state_decl_t    state_decl = {yp_sizeof(ypObject *), -2};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
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
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), 1, {1}};
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
    // FIXME Use define_function somehow?
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
                // FIXME Implement str_isidentifier: {1, {{str_1}}},
                // FIXME Implement str_isidentifier: {1, {{str_star_1}}},
                {1, {{str_star_star}}},
                // FIXME Implement str_isidentifier: {1, {{str_star_star_1}}},

                // Non-unique names
                {2, {{str_a}, {str_a}}},
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
    // FIXME Use define_function somehow?
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

    // Keep the exception if name is an exception.
    {
        define_function(f, None_code, ({yp_OSError}));
        assert_isexception(f, yp_OSError);
    }

    yp_decrefN(N(
            int_0, str_a, str_b, str_c, str_slash, str_star, str_star_args, str_star_star_kwargs));
    return MUNIT_OK;
}

// FIXME test_new_immortal: test the yp_IMMORTAL_FUNCTION/etc "constructors".

// Tests common to test_callN, test_call_stars, and callN_to_call_arrayX.
static MunitResult _test_callN(ypObject *(*any_callN)(ypObject *, int, ...))
{
    ypObject *defs[] = obj_array_init(5, rand_obj_any());
    ypObject *args[] = obj_array_init(5, rand_obj_any());
    ypObject *str_a = yp_str_frombytesC2(-1, "a");
    ypObject *str_b = yp_str_frombytesC2(-1, "b");
    ypObject *str_slash = yp_str_frombytesC2(-1, "/");
    ypObject *str_star = yp_str_frombytesC2(-1, "*");
    ypObject *str_star_args = yp_str_frombytesC2(-1, "*args");
    ypObject *str_star_star_kwargs = yp_str_frombytesC2(-1, "**kwargs");

    // def f()
    {
        define_function(f, capture_code, ());

        ead(capt, any_callN(f, 0), assert_captured_is(capt, f, 0, NULL));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a)
    {
        define_function(f, capture_code, ({str_a}));

        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a, b)
    {
        define_function(f, capture_code, ({str_a}, {str_b}));

        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a=0)
    {
        define_function(f, capture_code, ({str_a, defs[0]}));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a=yp_NameError)
    {
        define_function(f, capture_code, ({str_a, yp_NameError}));

        // It's the responsibility of the function to detect exceptions in argarray.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, captured_NameError));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a=0, b=1)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_b, defs[1]}));

        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0], defs[1]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0], defs[1]));
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a, /)
    {
        define_function(f, capture_code, ({str_a}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have one element.
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a, /, b)
    {
        define_function(f, capture_code, ({str_a}, {str_slash}, {str_b}));

        ead(capt, any_callN(f, N(args[0], args[1])),
                assert_captured(capt, f, args[0], captured_NULL, args[1]));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a, b, /)
    {
        define_function(f, capture_code, ({str_a}, {str_b}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have two elements.
        ead(capt, any_callN(f, N(args[0], args[1])), assert_captured(capt, f, args[0], args[1]));

        assert_raises(any_callN(f, N(args[0])), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1], args[2])), yp_TypeError);

        yp_decref(f);
    }

    // def f(a=0, /)
    {
        define_function(f, capture_code, ({str_a, defs[0]}, {str_slash}));

        // The trailing NULL is trimmed from argarray, so argarray will have one element.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, defs[0]));
        ead(capt, any_callN(f, N(args[0])), assert_captured(capt, f, args[0]));

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

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

        yp_decref(f);
    }

    // def f(*, a)
    {
        define_function(f, capture_code, ({str_star}, {str_a}));

        // Being a keyword-only parameter, a cannot be set from a "callN" positional argument.
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);

        assert_raises(any_callN(f, 0), yp_TypeError);
        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

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

        yp_decref(f);
    }

    // def f(*, a=0)
    {
        define_function(f, capture_code, ({str_star}, {str_a, defs[0]}));

        // Being a keyword-only parameter, a cannot be set from a "callN" positional argument.
        ead(capt, any_callN(f, 0), assert_captured(capt, f, captured_NULL, defs[0]));
        assert_raises(any_callN(f, N(args[0])), yp_TypeError);

        assert_raises(any_callN(f, N(args[0], args[1])), yp_TypeError);

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

        yp_decref(f);
    }

    obj_array_decref(defs);
    obj_array_decref(args);
    yp_decrefN(N(str_a, str_b, str_slash, str_star, str_star_args, str_star_star_kwargs));
    return MUNIT_OK;
}

static MunitResult test_callN(const MunitParameter params[], fixture_t *fixture)
{
    ypObject   *str_a = yp_str_frombytesC2(-1, "a");
    MunitResult result;

    result = _test_callN(yp_callN);
    if (result != MUNIT_OK) goto tear_down;

    // It's the responsibility of the function to detect exceptions in the arguments.
    {
        define_function(f, capture_code, ({str_a}));

        ead(capt, yp_callN(f, N(yp_NameError)), assert_captured(capt, f, captured_NameError));

        yp_decref(f);
    }

tear_down:
    yp_decrefN(N(str_a));
    return result;
}

// Accepts the yp_callN signature and instead calls yp_call_stars. For use in _test_callN. n must be
// >= 0.
static ypObject *callN_to_call_stars(ypObject *c, int n, ...)
{
    va_list   args;
    ypObject *as_tuple;
    ypObject *result;

    va_start(args, n);
    as_tuple = yp_tupleNV(n, args);  // new ref
    va_end(args);

    result = yp_call_stars(c, as_tuple, yp_frozendict_empty);
    yp_decref(as_tuple);
    return result;
}

static MunitResult test_call_stars(const MunitParameter params[], fixture_t *fixture)
{
    MunitResult result;

    result = _test_callN(callN_to_call_stars);
    if (result != MUNIT_OK) goto tear_down;

    // It's the responsibility of the function to detect exceptions in the arguments; however, this
    // is impossible with yp_call_stars, as both tuples and frozendicts reject exceptions.

tear_down:
    return result;
}

// Accepts the yp_callN signature and instead calls yp_call_arrayX. For use in _test_callN. n must
// be >= 0.
static ypObject *callN_to_call_arrayX(ypObject *c, int n, ...)
{
    va_list   args;
    ypObject *as_array[n + 1];
    int       i;

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
    ypObject   *str_a = yp_str_frombytesC2(-1, "a");
    MunitResult result;

    result = _test_callN(callN_to_call_arrayX);
    if (result != MUNIT_OK) goto tear_down;

    // It's the responsibility of the function to detect exceptions in the arguments.
    {
        define_function(f, capture_code, ({str_a}));
        ypObject *args[] = {f, yp_NameError};

        ead(capt, yp_call_arrayX(2, args), assert_captured(capt, f, captured_NameError));

        yp_decref(f);
    }

tear_down:
    yp_decrefN(N(str_a));
    return result;
}


MunitTest test_function_tests[] = {TEST(test_newC, NULL), TEST(test_callN, NULL),
        TEST(test_call_stars, NULL), TEST(test_call_arrayX, NULL), {NULL}};


extern void test_function_initialize(void) {}

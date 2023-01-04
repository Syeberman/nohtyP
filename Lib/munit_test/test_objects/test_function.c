
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

// Used as the code for a function. Captures all details about the arguments and returns them. NULL
// entries in argarray are replaced with the captured_NULL object.
static ypObject *capture_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ssize_t i;
    ypObject  *argarray_ptr = yp_intC((yp_ssize_t)argarray);  // new ref
    ypObject  *result = yp_listN(N(f, argarray_ptr));
    for (i = 0; i < n; i++) {
        ypObject *arg = argarray[i] == NULL ? captured_NULL : argarray[i];
        assert_not_raises_exc(yp_append(result, arg, &exc));
    }
    yp_decref(argarray_ptr);
    return result;
}

#define assert_captured_f(captured, op, expected)                                                 \
    do {                                                                                          \
        ypObject *_ypmt_CAPT_f = yp_getindexC((captured), 0); /* new ref */                       \
        ypObject *_ypmt_CAPT_expected = (expected);                                               \
        _assert_not_exception(_ypmt_CAPT_f, "yp_getindexC(%s, 0)", #captured);                    \
        _assert_obj(_ypmt_CAPT_f, op, _ypmt_CAPT_expected, "<%s f>", "%s", #captured, #expected); \
        yp_decref(_ypmt_CAPT_f);                                                                  \
    } while (0)

#define assert_captured_n(captured, op, expected)                                            \
    do {                                                                                     \
        yp_ssize_t _ypmt_CAPT_len;                                                           \
        yp_ssize_t _ypmt_CAPT_expected = (expected);                                         \
        _assert_not_raises_exc(                                                              \
                _ypmt_CAPT_len = yp_lenC((captured), &exc), "yp_lenC(%s, &exc)", #captured); \
        _assert_typeC(_ypmt_CAPT_len - 2, op, _ypmt_CAPT_expected, PRIssize, "<%s n>", "%s", \
                #captured, #expected);                                                       \
    } while (0)

#define assert_captured_argarray_ptr(captured, op, expected)                                   \
    do {                                                                                       \
        void     *_ypmt_CAPT_ptr;                                                              \
        ypObject *_ypmt_CAPT_ptr_obj = yp_getindexC((captured), 1); /* new ref */              \
        void     *_ypmt_CAPT_expected = (expected);                                            \
        _assert_not_raises_exc(_ypmt_CAPT_ptr = (void *)yp_asssizeC(_ypmt_CAPT_ptr_obj, &exc), \
                "yp_asssizeC(yp_getindexC(%s, 1), &exc)", #captured);                          \
        yp_decref(_ypmt_CAPT_ptr_obj);                                                         \
        _assert_ptr(_ypmt_CAPT_ptr, op, _ypmt_CAPT_expected, "<%s argarray>", "%s", #captured, \
                #expected);                                                                    \
    } while (0)

// Asserts that argarray contained exactly the given items in order. Items are compared by nohtyP
// equality (i.e. yp_eq). Use captured_NULL for any NULL entries. Also validates n.
#define assert_captured_args(captured, ...)                                                       \
    do {                                                                                          \
        ypObject *_ypmt_CAPT_args = yp_getsliceC4((captured), 2, yp_SLICE_LAST, 1); /* new ref */ \
        ypObject *_ypmt_CAPT_items[] = {__VA_ARGS__};                                             \
        char     *_ypmt_CAPT_item_strs[] = {STRINGIFY(__VA_ARGS__)};                              \
        _assert_not_exception(                                                                    \
                _ypmt_CAPT_args, "yp_getsliceC4(%s, 2, yp_SLICE_LAST, 1)", #captured);            \
        _assert_sequence(                                                                         \
                _ypmt_CAPT_args, _ypmt_CAPT_items, "<%s args>", _ypmt_CAPT_item_strs, #captured); \
        yp_decref(_ypmt_CAPT_args);                                                               \
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
    // Valid signatures.
    // FIXME Use define_function somehow?
    {
        yp_ssize_t  i;
        signature_t signatures[] = {
                {0},                                                      // def f()
                {1, {{str_a}}},                                           // def f(a)
                {2, {{str_a}, {str_b}}},                                  // def f(a, b)
                {1, {{str_a, int_0}}},                                    // def f(a=0)
                {1, {{str_a, yp_NameError}}},                             // def f(a=<exc>)
                {2, {{str_a, int_0}, {str_b, int_0}}},                    // def f(a=0, b=0)
                {2, {{str_a}, {yp_s_slash}}},                             // def f(a, /)
                {3, {{str_a}, {yp_s_slash}, {str_b}}},                    // def f(a, /, b)
                {3, {{str_a}, {str_b}, {yp_s_slash}}},                    // def f(a, b, /)
                {2, {{str_a, int_0}, {yp_s_slash}}},                      // def f(a=0, /)
                {2, {{str_a, yp_NameError}, {yp_s_slash}}},               // def f(a=<exc>, /)
                {3, {{str_a, int_0}, {str_b, int_0}, {yp_s_slash}}},      // def f(a=0, b=0, /)
                {3, {{str_a, int_0}, {yp_s_slash}, {str_b, int_0}}},      // def f(a=0, /, b=0)
                {2, {{yp_s_star}, {str_a}}},                              // def f(*, a)
                {3, {{yp_s_star}, {str_a}, {str_b}}},                     // def f(*, a, b)
                {3, {{str_a}, {yp_s_star}, {str_b}}},                     // def f(a, *, b)
                {2, {{yp_s_star}, {str_a, int_0}}},                       // def f(*, a=0)
                {2, {{yp_s_star}, {str_a, yp_NameError}}},                // def f(*, a=<exc>)
                {3, {{yp_s_star}, {str_a, int_0}, {str_b}}},              // def f(*, a=0, b)
                {3, {{yp_s_star}, {str_a, int_0}, {str_b, int_0}}},       // def f(*, a=0, b=0)
                {3, {{str_a, int_0}, {yp_s_star}, {str_b}}},              // def f(a=0, *, b)
                {3, {{str_a, int_0}, {yp_s_star}, {str_b, int_0}}},       // def f(a=0, *, b=0)
                {1, {{yp_s_star_args}}},                                  // def f(*args)
                {2, {{yp_s_star_args}, {str_a}}},                         // def f(*args, a)
                {3, {{yp_s_star_args}, {str_a}, {str_b}}},                // def f(*args, a, b)
                {3, {{str_a}, {yp_s_star_args}, {str_b}}},                // def f(a, *args, b)
                {2, {{yp_s_star_args}, {str_a, int_0}}},                  // def f(*args, a=0)
                {2, {{yp_s_star_args}, {str_a, yp_NameError}}},           // def f(*args, a=<exc>)
                {3, {{yp_s_star_args}, {str_a, int_0}, {str_b}}},         // def f(*args, a=0, b)
                {3, {{yp_s_star_args}, {str_a, int_0}, {str_b, int_0}}},  // def f(*args, a=0, b=0)
                {3, {{str_a, int_0}, {yp_s_star_args}, {str_b}}},         // def f(a=0, *args, b)
                {3, {{str_a, int_0}, {yp_s_star_args}, {str_b, int_0}}},  // def f(a=0, *args, b=0)
                {1, {{yp_s_star_star_kwargs}}},                           // def f(**kwargs)
                {6, {{str_a}, {yp_s_slash}, {str_b}, {yp_s_star}, {str_c},
                            {yp_s_star_star_kwargs}}},  // def f(a, /, b, *, c, **kwargs)
                {6, {{str_a}, {yp_s_slash}, {str_b}, {yp_s_star_args}, {str_c},
                            {yp_s_star_star_kwargs}}},  // def f(a, /, b, *args, c, **kwargs)
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
        yp_IMMORTAL_STR_LATIN_1_static(str_1, "1");
        yp_IMMORTAL_STR_LATIN_1_static(str_star_a, "*a");
        yp_IMMORTAL_STR_LATIN_1_static(str_star_1, "*1");
        yp_IMMORTAL_STR_LATIN_1_static(str_star_star, "**");
        yp_IMMORTAL_STR_LATIN_1_static(str_star_star_a, "**a");
        yp_IMMORTAL_STR_LATIN_1_static(str_star_star_1, "**1");
        signature_t signatures[] = {
                {1, {{yp_s_slash}}},                            // / cannot be first
                {2, {{yp_s_star}, {yp_s_slash}}},               // / cannot be after *
                {3, {{str_a}, {yp_s_star}, {yp_s_slash}}},      // / cannot be after *
                {3, {{yp_s_star}, {str_a}, {yp_s_slash}}},      // / cannot be after *
                {2, {{yp_s_star_args}, {yp_s_slash}}},          // / cannot be after *args
                {2, {{yp_s_star_star_kwargs}, {yp_s_slash}}},   // / cannot be after **kwargs
                {3, {{str_a}, {yp_s_slash}, {yp_s_slash}}},     // At most one /
                {2, {{str_a}, {yp_s_slash, int_0}}},            // / cannot have default
                {1, {{yp_s_star}}},                             // * cannot be last
                {2, {{yp_s_star}, {yp_s_star_star_kwargs}}},    // * cannot be imm. before **kwargs
                {3, {{yp_s_star}, {yp_s_star}, {str_a}}},       // At most one *
                {2, {{yp_s_star}, {yp_s_star_args}}},           // * or *args, not both
                {3, {{yp_s_star_args}, {yp_s_star}, {str_a}}},  // * or *args, not both
                {2, {{yp_s_star, int_0}, {str_a}}},             // * cannot have default
                {2, {{yp_s_star_args}, {yp_s_star_args}}},      // At most one *args
                {1, {{yp_s_star_args, int_0}}},                 // *args cannot have default
                {2, {{yp_s_star_star_kwargs}, {str_a}}},        // **kwargs must be last
                {2, {{yp_s_star_star_kwargs}, {yp_s_slash}}},   // **kwargs must be last
                {3, {{yp_s_star_star_kwargs}, {yp_s_star}, {str_a}}},     // **kwargs must be last
                {2, {{yp_s_star_star_kwargs}, {yp_s_star_args}}},         // **kwargs must be last
                {2, {{yp_s_star_star_kwargs}, {yp_s_star_star_kwargs}}},  // At most one **kwargs
                {1, {{yp_s_star_star_kwargs, int_0}}},         // **kwargs cannot have default
                {2, {{str_a, int_0}, {str_b}}},                // Defaults on remaining pos. args
                {3, {{str_a, int_0}, {str_b}, {yp_s_slash}}},  // Defaults on remaining pos. args
                {3, {{str_a, int_0}, {yp_s_slash}, {str_b}}},  // Defaults on remaining pos. args

                // Non-identifiers
                {1, {{yp_str_empty}}},
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
    }

    // Parameter names must be strs.
    // FIXME Use define_function somehow?
    {
        yp_ssize_t  i;
        ypObject   *chrarray_a = yp_chrarray(str_a);
        ypObject   *chrarray_slash = yp_chrarray(yp_s_slash);
        ypObject   *chrarray_star = yp_chrarray(yp_s_star);
        ypObject   *chrarray_star_args = yp_chrarray(yp_s_star_args);
        ypObject   *chrarray_star_star_kwargs = yp_chrarray(yp_s_star_star_kwargs);
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
        yp_decrefN(N(chrarray_a, chrarray_slash, chrarray_star, chrarray_star_args,
                chrarray_star_star_kwargs));
    }

    // Keep the exception if name is an exception.
    {
        define_function(f, None_code, ({yp_OSError}));
        assert_isexception(f, yp_OSError);
    }

    return MUNIT_OK;
}

// FIXME test_new_immortal: test the yp_IMMORTAL_FUNCTION/etc "constructors".

static MunitResult _test_callN(ypObject *(*any_callN)(ypObject *, int, ...))
{
    {
        define_function(f, capture_code, ());
        ypObject *captured = any_callN(f, 0);
        assert_captured_f(captured, is, f);
        assert_captured_n(captured, ==, 0);
        assert_captured_argarray_ptr(captured, ==, NULL);

        assert_raises(any_callN(f, N(int_0)), yp_TypeError);

        yp_decrefN(N(f, captured));
    }

    return MUNIT_OK;
}

static MunitResult test_callN(const MunitParameter params[], fixture_t *fixture)
{
    return _test_callN(yp_callN);
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
    return _test_callN(callN_to_call_stars);
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
    return _test_callN(callN_to_call_arrayX);
}


MunitTest test_function_tests[] = {TEST(test_newC, NULL), TEST(test_callN, NULL),
        TEST(test_call_stars, NULL), TEST(test_call_arrayX, NULL), {NULL}};


extern void test_function_initialize(void) {}

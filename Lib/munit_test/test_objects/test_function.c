
#include "munit_test/unittest.h"


typedef struct _signature_t {
    yp_int32_t          n;
    yp_parameter_decl_t params[8];  // Increase length as necessary.
} signature_t;

// Used as the code for a function. Unconditionally returns None.
static ypObject *None_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray) { return yp_None; }

// Used as the code for a function. Captures all details about the arguments and returns them.
static ypObject *capture_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ssize_t i;
    ypObject  *argarray_obj = yp_intC((yp_ssize_t)argarray);  // new ref
    ypObject  *result = yp_listN(N(f, argarray_obj));
    for (i = 0; i < n; i++) assert_not_raises_exc(yp_append(result, argarray[i], &exc));
    yp_decref(argarray_obj);
    return result;
}

#define assert_captured_f(capture, op, expected)                                                 \
    do {                                                                                         \
        ypObject *_ypmt_CAPT_f = yp_getindexC((capture), 0); /* new ref */                       \
        ypObject *_ypmt_CAPT_expected = (expected);                                              \
        _assert_not_exception(_ypmt_CAPT_f, "yp_getindexC(%s, 0)", #capture);                    \
        _assert_obj(_ypmt_CAPT_f, op, _ypmt_CAPT_expected, "<%s f>", "%s", #capture, #expected); \
        yp_decref(_ypmt_CAPT_f);                                                                 \
    } while (0)


static MunitResult test_newC(const MunitParameter params[], fixture_t *fixture)
{
    // Valid signatures.
    {
        yp_ssize_t  i;
        signature_t signatures[] = {
                {0, {}},                                     // def f()
                {1, {{str_a}}},                              // def f(a)
                {1, {{str_a, yp_None}}},                     // def f(a=None)
                {1, {{str_a, yp_NameError}}},                // def f(a=<exception>)
                {2, {{str_a}, {yp_s_slash}}},                // def f(a, /)
                {2, {{str_a, yp_None}, {yp_s_slash}}},       // def f(a=None, /)
                {2, {{str_a, yp_NameError}, {yp_s_slash}}},  // def f(a=<exception>, /)
                {2, {{yp_s_star}, {str_a}}},                 // def f(*, a)
                {2, {{yp_s_star}, {str_a, yp_None}}},        // def f(*, a=None)
                {2, {{yp_s_star}, {str_a, yp_NameError}}},   // def f(*, a=<exception>)
                {1, {{yp_s_star_args}}},                     // def f(*args)
                {1, {{yp_s_star_star_kwargs}}},              // def f(**kwargs)
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
        yp_state_decl_t state_decl = {-1};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl.offsets_len == -1 (array of objects) is not yet implemented.
    {
        yp_state_decl_t state_decl = {yp_sizeof(ypObject *), -1};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_NotImplementedError);
    }

    // state_decl.offsets_len must be >= 0 or -1.
    {
        yp_state_decl_t state_decl = {yp_sizeof(ypObject *), -2};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl offsets cannot be negative.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), 1, {-1}};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl objects must be fully contained in state.
    {
        static yp_state_decl_t state_decl = {yp_sizeof(ypObject *), 1, {1}};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_ValueError);
    }

    // state_decl objects must be aligned.
    {
        static yp_state_decl_t state_decl = {1 + yp_sizeof(ypObject *), 1, {1}};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_SystemLimitationError);
    }

    // There is a maximum to the state_decl object offsets.
    {
        static yp_state_decl_t state_decl = {33 * yp_sizeof(ypObject *), 1, {32 * yp_sizeof(ypObject *)}};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_SystemLimitationError);
    }

    // state is not yet implemented.
    {
        yp_state_decl_t state_decl = {1};
        yp_function_decl_t decl = {None_code, 0, 0, NULL, NULL, &state_decl};
        assert_raises(yp_functionC(&decl), yp_NotImplementedError);
    }

    // Invalid signatures.
    {
        yp_ssize_t  i;
        signature_t signatures[] = {
                {1, {{yp_str_empty}}},  // name must be an identifier
                // FIXME name must be a valid Python identifier
                {1, {{yp_s_slash}}},                            // / cannot be the first parameter
                {2, {{yp_s_star}, {yp_s_slash}}},               // / cannot be after *
                {2, {{yp_s_star_args}, {yp_s_slash}}},          // / cannot be after *args
                {2, {{yp_s_star_star_kwargs}, {yp_s_slash}}},   // / cannot be after **kwargs
                {1, {{yp_s_star}}},                             // * cannot be last
                {2, {{yp_s_star}, {yp_s_star_star_kwargs}}},    // * cannot be imm. before **kwargs
                {2, {{yp_s_star}, {yp_s_star_args}}},           // * or *args, not both
                {3, {{yp_s_star_args}, {yp_s_star}, {str_a}}},  // * or *args, not both
                // FIXME *name must be a valid Python identifier
                // FIXME **name must be a valid Python identifier
                // FIXME name must not be repeated
        };
        for (i = 0; i < yp_lengthof_array(signatures); i++) {
            signature_t        signature = signatures[i];
            yp_function_decl_t decl = {None_code, 0, signature.n, signature.params, NULL, NULL};
            assert_raises(yp_functionC(&decl), yp_ParameterSyntaxError);
        }
    }

    // Parameter names must be strs.
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
        signature_t        signature = {1, {{yp_OSError}}};
        yp_function_decl_t decl = {None_code, 0, signature.n, signature.params, NULL, NULL};
        assert_raises(yp_functionC(&decl), yp_OSError);
    }

    return MUNIT_OK;
}


MunitTest test_function_tests[] = {TEST(test_newC, NULL), {NULL}};


extern void test_function_initialize(void) {}

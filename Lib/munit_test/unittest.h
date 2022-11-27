
#ifndef yp_MUNIT_TEST_UNITTEST_H
#define yp_MUNIT_TEST_UNITTEST_H
#ifdef __cplusplus
extern "C" {
#endif

#include "nohtyP.h"

#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"


typedef struct _fixture_t {
    void *_dummy;  // FIXME "C requires that a struct or union has at least one member"
} fixture_t;


#define SUITE_OF_TESTS(name)                             \
    {                                                    \
        "/" #name,                      /* prefix */     \
                name##_tests,           /* tests */      \
                NULL,                   /* suites */     \
                1,                      /* iterations */ \
                MUNIT_SUITE_OPTION_NONE /* options */    \
    }

#define SUITE_OF_SUITES(name)                            \
    {                                                    \
        "/" #name,                      /* prefix */     \
                NULL,                   /* tests */      \
                name##_suites,          /* suites */     \
                1,                      /* iterations */ \
                MUNIT_SUITE_OPTION_NONE /* options */    \
    }


#ifndef TRUE
#define TRUE (1 == 1)
#define FALSE (0 == 1)
#endif

// Defines exactly one of yp_ARCH_32_BIT or yp_ARCH_64_BIT.
#if yp_SSIZE_T_MAX == 0x7FFFFFFFFFFFFFFF
#define yp_ARCH_64_BIT 1
#else
#define yp_ARCH_32_BIT 1
#endif

// Similar to PRId64 defined in inttypes.h, this chooses the appropriate format string depending
// on the compiler.
//  PRIint: for use with yp_int_t
//  PRIssize: for use with yp_ssize_t
#if defined(PRId64)
#define PRIint PRId64
#else
#define PRIint "I64d"
#endif
#if defined(yp_ARCH_32_BIT)
#define PRIssize "d"
#elif defined(__APPLE__)
// The MacOS X 12.3 SDK defines ssize_t as long (see __darwin_ssize_t in the _types.h files).
#define PRIssize "ld"
#else
#define PRIssize PRIint
#endif


// FIXME A suite of tests to ensure these assertions are working. They can be
// MUNIT_TEST_OPTION_TODO, which will fail if they pass.

#define assert_ssize(a, op, b) assert_type(yp_ssize_t, PRIssize, a, op, b)

// FIXME A better error message to list the exception name.
#define _assert_not_exception(obj, obj_str, ...)                                          \
    do {                                                                                  \
        if (yp_isexceptionC(obj)) {                                                       \
            munit_errorf("assertion failed: !yp_isexceptionC(" obj_str ")", __VA_ARGS__); \
        }                                                                                 \
    } while (0)

#define assert_not_exception(obj)                             \
    do {                                                      \
        ypObject *_ypmt_NOT_EXC_obj = (obj);                  \
        _assert_not_exception(_ypmt_NOT_EXC_obj, "%s", #obj); \
    } while (0)

// Called when the test should fail because an exception was returned. Fails with either "assertion
// failed: yp_isexceptionC" (ex yp_eq returns an exception) or "returned non-exception on error" (ex
// yp_eq returns something other than yp_True, yp_False, or an exception).
#define _ypmt_error_exception(exc, non_exc_str, obj_str, ...) \
    do {                                                      \
        _assert_not_exception(exc, obj_str, __VA_ARGS__);     \
        munit_errorf(non_exc_str ": " obj_str, __VA_ARGS__);  \
    } while (0)

#define _assert_not_raises_exc(statement, statement_str, ...)                              \
    do {                                                                                   \
        ypObject *exc = NULL;                                                              \
        statement;                                                                         \
        if (exc != NULL) {                                                                 \
            if (yp_isexceptionC(exc)) {                                                    \
                munit_errorf("assertion failed: " statement_str "; !yp_isexceptionC(exc)", \
                        __VA_ARGS__);                                                      \
            }                                                                              \
            munit_errorf("exc set to a non-exception: " statement_str, __VA_ARGS__);       \
        }                                                                                  \
    } while (0)

// For a function that takes `ypObject **exc`, asserts that it does not raise an exception.
// Statement must include `&exc` for the exception argument, and can include a variable assignment.
// Example:
//
//      assert_not_raises_exc(len = yp_lenC(obj, &exc));
#define assert_not_raises_exc(statement) _assert_not_raises_exc(statement, "%s", #statement)

#define _assert_obj(a, op, b, a_str, b_str, ...)                                                   \
    do {                                                                                           \
        ypObject *_ypmt_OBJ_result = yp_##op(a, b);                                                \
        if (_ypmt_OBJ_result == yp_True) {                                                         \
            /* pass */                                                                             \
        } else if (_ypmt_OBJ_result == yp_False) {                                                 \
            munit_errorf(                                                                          \
                    "assertion failed: yp_" #op "(" a_str ", " b_str ") == yp_True", __VA_ARGS__); \
        } else {                                                                                   \
            _ypmt_error_exception(_ypmt_OBJ_result, "expected yp_True, yp_False, or an exception", \
                    "yp_" #op "(" a_str ", " b_str ")", __VA_ARGS__);                              \
        }                                                                                          \
    } while (0)

#define assert_obj(a, op, b)                                           \
    do {                                                               \
        ypObject *_ypmt_OBJ_a = (a);                                   \
        ypObject *_ypmt_OBJ_b = (b);                                   \
        _assert_obj(_ypmt_OBJ_a, op, _ypmt_OBJ_b, "%s", "%s", #a, #b); \
    } while (0)

#define _assert_len(obj, expected, obj_str, expected_str, ...)                           \
    do {                                                                                 \
        yp_ssize_t _ypmt_LEN_actual;                                                     \
        _assert_not_raises_exc(_ypmt_LEN_actual = yp_lenC(obj, &exc),                    \
                "yp_lenC(" obj_str ", &exc) == " expected_str, __VA_ARGS__);             \
        if (_ypmt_LEN_actual != expected) {                                              \
            munit_errorf("assertion failed: yp_lenC(" obj_str ", &exc) == " expected_str \
                         " (%" PRIssize " == %" PRIssize ")",                            \
                    __VA_ARGS__, _ypmt_LEN_actual, expected);                            \
        }                                                                                \
    } while (0)

#define assert_len(obj, expected)                                                    \
    do {                                                                             \
        ypObject  *_ypmt_LEN_obj = (obj);                                            \
        yp_ssize_t _ypmt_LEN_expected = (expected);                                  \
        _assert_len(_ypmt_LEN_obj, _ypmt_LEN_expected, "%s", "%s", #obj, #expected); \
    } while (0)

// Asserts that obj is a sequence containing exactly the given n items in that order. Validates
// yp_lenC and yp_getindexC.
// FIXME And yp_getitemC? And others?
// FIXME Nicely print item that failed (needs yp_str)
// FIXME How does this cleanup for the teardown method if it fails?
// FIXME Do better than <expected>
#define assert_sequence(obj, n, ...)                                                           \
    do {                                                                                       \
        ypObject  *_ypmt_SEQ_obj = (obj);                                                      \
        yp_ssize_t _ypmt_SEQ_n = (n);                                                          \
        ypObject  *_ypmt_SEQ_items[] = {__VA_ARGS__};                                          \
        yp_ssize_t _ypmt_SEQ_i;                                                                \
        _assert_len(_ypmt_SEQ_obj, _ypmt_SEQ_n, "%s", "%s", #obj, #n);                         \
        for (_ypmt_SEQ_i = 0; _ypmt_SEQ_i < _ypmt_SEQ_n; _ypmt_SEQ_i++) {                      \
            ypObject *_ypmt_SEQ_actual = yp_getindexC(_ypmt_SEQ_obj, _ypmt_SEQ_i);             \
            _assert_obj(_ypmt_SEQ_actual, eq, _ypmt_SEQ_items[_ypmt_SEQ_i],                    \
                    "yp_getindexC(%s, %" PRIssize ")", "%s", #obj, _ypmt_SEQ_i, "<expected>"); \
            yp_decref(_ypmt_SEQ_actual);                                                       \
        }                                                                                      \
    } while (0)

#ifdef __cplusplus
}
#endif
#endif  // yp_MUNIT_TEST_UNITTEST_H

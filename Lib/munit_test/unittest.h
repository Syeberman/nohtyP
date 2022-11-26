
#ifndef yp_MUNIT_TEST_UNITTEST_H
#define yp_MUNIT_TEST_UNITTEST_H
#ifdef __cplusplus
extern "C" {
#endif

#include "nohtyP.h"

#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"


typedef struct _fixture_t {
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


#define assert_ssize(a, op, b) assert_type(yp_ssize_t, PRIssize, a, op, b)

// FIXME A better error message to list the exception name.
#define assert_not_exception(obj) assert_false(yp_isexceptionC(obj))

// For a function that takes `ypObject **exc`, asserts that it does not raise an exception.
// Statement must include `&exc` for the exception argument, and can include a variable assignment.
// Example:
//
//      assert_not_raises_exc(len = yp_lenC(obj, &exc));
#define assert_not_raises_exc(statement)                            \
    do {                                                            \
        ypObject *exc = NULL;                                       \
        statement;                                                  \
        if (exc != NULL) {                                          \
            assert_not_exception(exc);                              \
            munit_error("exc set to a non-exception: " #statement); \
        }                                                           \
    } while (0)

#define assert_len(obj, expected)                                                                 \
    do {                                                                                          \
        yp_ssize_t actual;                                                                        \
        yp_ssize_t _yp_MUNIT_expected = (expected);                                               \
        assert_not_raises_exc(actual = yp_lenC(obj, &exc));                                       \
        if (actual != _yp_MUNIT_expected) {                                                       \
            munit_errorf("assertion failed: yp_lenC(%s, &exc) == %s (%" PRIssize " == %" PRIssize \
                         ")",                                                                     \
                    #obj, #expected, actual, _yp_MUNIT_expected);                                 \
        }                                                                                         \
    } while (0)


#ifdef __cplusplus
}
#endif
#endif  // yp_MUNIT_TEST_UNITTEST_H

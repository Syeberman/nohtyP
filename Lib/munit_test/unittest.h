#ifndef yp_MUNIT_TEST_UNITTEST_H
#define yp_MUNIT_TEST_UNITTEST_H
#ifdef __cplusplus
extern "C" {
#endif

// Older versions of MSVS don't have snprintf, and _snprintf doesn't always write the null
// terminator. For how we use this function (fail on overflow), this is fine.
#if defined(_MSC_VER) && _MSC_VER < 1900
// Disables a warning about using _snprintf over _snprintf_s.
#define _CRT_SECURE_NO_WARNINGS
#define unittest_snprintf _snprintf
#else
#define unittest_snprintf snprintf
#endif

#define yp_FUTURE
#include "nohtyP.h"

#include "munit.h"

#include <stddef.h>
#include <stdio.h>

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

#if defined(_MSC_VER) && _MSC_VER < 1800
// Early versions of the Windows CRT did not support lld.
#define PRIint "I64d"
#elif defined(_WIN32) && defined(__GNUC__) && __GNUC__ < 10
// Early versions of the Windows CRT did not support lld.
#define PRIint "I64d"
#else
#define PRIint "lld"
#endif

#if defined(yp_ARCH_32_BIT)
#define PRIssize "d"
#elif defined(_MSC_VER) && _MSC_VER < 1800
// Early versions of the Windows CRT did not support lld.
#define PRIssize "I64d"
#elif defined(__APPLE__)
// The MacOS X 12.3 SDK defines ssize_t as long (see __darwin_ssize_t in the _types.h files).
#define PRIssize "ld"
#elif defined(PRId64)
#define PRIssize PRId64
#else
#define PRIssize "lld"
#endif

// Work around preprocessing bug in msvs_120 and earlier: https://stackoverflow.com/a/3985071/770500
#define _ESC(...) __VA_ARGS__

// clang-format off
// From https://stackoverflow.com/a/2308651/770500
#define _PP_NARG(prefix, ...) _ESC(_PP_ARG_N(prefix, __VA_ARGS__))
#define _PP_ARG_N(prefix, \
     _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
    _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
    _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
    _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
    _61,_62,_63,_64,_65,_66,  n, ...) prefix##n
#define _PP_RSEQ_N() \
             66,65,64,63,62,61,60, \
    59,58,57,56,55,54,53,52,51,50, \
    49,48,47,46,45,44,43,42,41,40, \
    39,38,37,36,35,34,33,32,31,30, \
    29,28,27,26,25,24,23,22,21,20, \
    19,18,17,16,15,14,13,12,11,10, \
     9, 8, 7, 6, 5, 4, 3, 2, 1, 0

// FIXME Generate these using a script to ensure correctness.
#define _STRINGIFY1(a) #a
#define _STRINGIFY2(a, b) #a, #b
#define _STRINGIFY3(a, b, c) #a, #b, #c
#define _STRINGIFY4(a, b, c, d) #a, #b, #c, #d
#define _STRINGIFY5(a, b, c, d, e) #a, #b, #c, #d, #e
#define _STRINGIFY6(a, b, c, d, e, f) #a, #b, #c, #d, #e, #f
#define _STRINGIFY7(a, b, c, d, e, f, g) #a, #b, #c, #d, #e, #f, #g
#define _STRINGIFY8(a, b, c, d, e, f, g, h) #a, #b, #c, #d, #e, #f, #g, #h
#define _STRINGIFY9(a, b, c, d, e, f, g, h, i) #a, #b, #c, #d, #e, #f, #g, #h, #i
#define _STRINGIFY10(a, b, c, d, e, f, g, h, i, j) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j
#define _STRINGIFY11(a, b, c, d, e, f, g, h, i, j, k) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k
#define _STRINGIFY12(a, b, c, d, e, f, g, h, i, j, k, l) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l
#define _STRINGIFY13(a, b, c, d, e, f, g, h, i, j, k, l, m) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m
#define _STRINGIFY14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n
#define _STRINGIFY15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o
#define _STRINGIFY16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p
#define _STRINGIFY17(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q
#define _STRINGIFY18(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r
#define _STRINGIFY19(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s
#define _STRINGIFY20(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t
#define _STRINGIFY21(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u
#define _STRINGIFY22(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v
#define _STRINGIFY23(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w
#define _STRINGIFY24(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x
#define _STRINGIFY25(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y
#define _STRINGIFY26(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z
#define _STRINGIFY27(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A
#define _STRINGIFY28(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B
#define _STRINGIFY29(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B, #C
#define _STRINGIFY30(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B, #C, #D
#define _STRINGIFY31(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B, #C, #D, #E
#define _STRINGIFY32(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B, #C, #D, #E, #F
#define _STRINGIFY33(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B, #C, #D, #E, #F, #G
#define _STRINGIFY34(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, A, B, C, D, E, F, G, H) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z, #A, #B, #C, #D, #E, #F, #G, #H

// FIXME Generate these using a script to ensure correctness.
#define _COMMA_REPEAT1(x) x
#define _COMMA_REPEAT2(x) x, x
#define _COMMA_REPEAT3(x) x, x, x
#define _COMMA_REPEAT4(x) x, x, x, x
#define _COMMA_REPEAT5(x) x, x, x, x, x
#define _COMMA_REPEAT6(x) x, x, x, x, x, x
#define _COMMA_REPEAT7(x) x, x, x, x, x, x, x
#define _COMMA_REPEAT8(x) x, x, x, x, x, x, x, x
#define _COMMA_REPEAT9(x) x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT10(x) x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT11(x) x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT12(x) x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT13(x) x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT14(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT15(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT16(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT17(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT18(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT19(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT20(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT21(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT22(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT23(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT24(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT25(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT26(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT27(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT28(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT29(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT30(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT31(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT32(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
#define _COMMA_REPEAT33(x) x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x
// clang-format on

// Variadic macro tricks.
#define VA_ARGC(...) (_PP_NARG(, __VA_ARGS__, _PP_RSEQ_N()))
#define N(...) VA_ARGC(__VA_ARGS__), __VA_ARGS__
#define K(...) (VA_ARGC(__VA_ARGS__) / 2), __VA_ARGS__
#define STRINGIFY(...) _ESC(_PP_NARG(_STRINGIFY, __VA_ARGS__, _PP_RSEQ_N())(__VA_ARGS__))
#define UNPACK(...) __VA_ARGS__

// sizeof and offsetof as yp_ssize_t, and sizeof a structure member
#define yp_sizeof(x) ((yp_ssize_t)sizeof(x))
#define yp_offsetof(structType, member) ((yp_ssize_t)offsetof(structType, member))
#define yp_sizeof_member(structType, member) yp_sizeof(((structType *)0)->member)

// Length of an array. Only call for arrays of fixed size that haven't been coerced to pointers.
#define yp_lengthof_array(x) (yp_sizeof(x) / yp_sizeof((x)[0]))

// Length of an array in a structure. Only call for arrays of fixed size.
#define yp_lengthof_array_member(structType, member) yp_lengthof_array(((structType *)0)->member)


// parameters can be NULL.
#define TEST(name, parameters)                                     \
    {                                                              \
            "/" #name,                            /* name */       \
            (MunitTestFunc)(name),                /* test */       \
            (MunitTestSetup)fixture_setup,        /* setup */      \
            (MunitTestTearDown)fixture_tear_down, /* tear_down */  \
            MUNIT_TEST_OPTION_NONE,               /* options */    \
            parameters                            /* parameters */ \
    }

#define SUITE_OF_TESTS(name)                         \
    {                                                \
            "/" #name,              /* prefix */     \
            (name##_tests),         /* tests */      \
            NULL,                   /* suites */     \
            1,                      /* iterations */ \
            MUNIT_SUITE_OPTION_NONE /* options */    \
    }

#define SUITE_OF_SUITES(name)                        \
    {                                                \
            "/" #name,              /* prefix */     \
            NULL,                   /* tests */      \
            (name##_suites),        /* suites */     \
            1,                      /* iterations */ \
            MUNIT_SUITE_OPTION_NONE /* options */    \
    }


// FIXME A suite of tests to ensure these assertions are working. They can be
// MUNIT_TEST_OPTION_TODO, which will fail if they pass.

// A different take on munit_assert_type_full that allows for formatting in the strings for a and b.
// a and b may be evaluated multiple times (assign to a variable first).
#define _assert_typeC(a, op, b, val_pri, a_fmt, b_fmt, ...)                                 \
    do {                                                                                    \
        if (!(a op b)) {                                                                    \
            munit_errorf("assertion failed: " a_fmt " " #op " " b_fmt " (%" val_pri " " #op \
                         " %" val_pri ") ",                                                 \
                    __VA_ARGS__, a, b);                                                     \
        }                                                                                   \
    } while (0)

// a and b may be evaluated multiple times (assign to a variable first).
#define _assert_ptr(a, op, b, a_fmt, b_fmt, ...) \
    _assert_typeC(a, op, b, "p", a_fmt, b_fmt, __VA_ARGS__)

// The munit assertions are sufficient: we do not need control over the formatting of a and b.
#define assert_true(expr) munit_assert_true(expr)
#define assert_false(expr) munit_assert_false(expr)
#define assert_ptr(a, op, b) munit_assert_ptr(a, op, b)
#define assert_null(ptr) munit_assert_null(ptr)
#define assert_not_null(ptr) munit_assert_not_null(ptr)
#define assert_intC(a, op, b) munit_assert_type(yp_int_t, PRIint, a, op, b)
#define assert_ssizeC(a, op, b) munit_assert_type(yp_ssize_t, PRIssize, a, op, b)
#define assert_hashC(a, op, b) munit_assert_type(yp_hash_t, PRIssize, a, op, b)

// FIXME A better error message to list the exception name.
#define _assert_not_exception(obj, obj_fmt, ...)                                          \
    do {                                                                                  \
        if (yp_isexceptionC(obj)) {                                                       \
            munit_errorf("assertion failed: !yp_isexceptionC(" obj_fmt ")", __VA_ARGS__); \
        }                                                                                 \
    } while (0)

// Asserts that obj is not an exception and that no exception has been raised.
#define assert_not_exception(obj)                             \
    do {                                                      \
        ypObject *_ypmt_NOT_EXC_obj = (obj);                  \
        _assert_not_exception(_ypmt_NOT_EXC_obj, "%s", #obj); \
    } while (0)

// Called when the test should fail because an exception was returned. Fails with either "assertion
// failed: yp_isexceptionC" (ex yp_eq returns an exception) or "returned non-exception on error" (ex
// yp_eq returns something other than yp_True, yp_False, or an exception).
#define _ypmt_error_exception(exc, non_exc_fmt, obj_fmt, ...) \
    do {                                                      \
        _assert_not_exception(exc, obj_fmt, __VA_ARGS__);     \
        munit_errorf(non_exc_fmt ": " obj_fmt, __VA_ARGS__);  \
    } while (0)

// statement is only evaluated once.
#define _assert_not_raises(statement, statement_fmt, ...)                        \
    do {                                                                         \
        ypObject *_ypmt_NOT_RAISES_obj = statement;                              \
        _assert_not_exception(_ypmt_NOT_RAISES_obj, statement_fmt, __VA_ARGS__); \
    } while (0)

// Asserts that statement does not raise an exception.
#define assert_not_raises(statement) _assert_not_raises((statement), "%s", #statement)

// statement is only evaluated once.
#define _assert_not_raises_exc(statement, statement_fmt, ...)                              \
    do {                                                                                   \
        ypObject *exc = NULL;                                                              \
        statement;                                                                         \
        if (exc != NULL) {                                                                 \
            if (yp_isexceptionC(exc)) {                                                    \
                munit_errorf("assertion failed: " statement_fmt "; !yp_isexceptionC(exc)", \
                        __VA_ARGS__);                                                      \
            }                                                                              \
            munit_errorf("exc set to a non-exception: " statement_fmt, __VA_ARGS__);       \
        }                                                                                  \
    } while (0)

// For a statement that takes a `ypObject **exc` argument, asserts that it does not raise an
// exception. Statement must include `&exc` for the exception argument, and can include a variable
// assignment. Example:
//
//      assert_not_raises_exc(len = yp_lenC(obj, &exc));
#define assert_not_raises_exc(statement) _assert_not_raises_exc((statement), "%s", #statement)

#define _assert_isexception(obj, n, expected, obj_fmt, expected_fmt, ...)                         \
    do {                                                                                          \
        if (n < 1) {                                                                              \
            munit_error("missing expected exception in assertion");                               \
        }                                                                                         \
        if (!yp_isexceptionC(obj)) {                                                              \
            munit_errorf("assertion failed: yp_isexceptionC(" obj_fmt ") (expected " expected_fmt \
                         ")",                                                                     \
                    __VA_ARGS__);                                                                 \
        }                                                                                         \
        if (!yp_isexception_arrayC(obj, n, expected)) {                                           \
            munit_errorf("assertion failed: yp_isexceptionCN(" obj_fmt ", N(" expected_fmt "))",  \
                    __VA_ARGS__);                                                                 \
        }                                                                                         \
    } while (0)

// Asserts that obj is one of the given exceptions, but that no exception has been raised.
#define assert_isexception(obj, ...)                                                          \
    do {                                                                                      \
        ypObject  *_ypmt_ISEXC_obj = (obj);                                                   \
        ypObject  *_ypmt_ISEXC_expected[] = {__VA_ARGS__};                                    \
        yp_ssize_t _ypmt_ISEXC_n = yp_lengthof_array(_ypmt_ISEXC_expected);                   \
        _assert_isexception(_ypmt_ISEXC_obj, _ypmt_ISEXC_n, _ypmt_ISEXC_expected, "%s", "%s", \
                #obj, #__VA_ARGS__);                                                          \
    } while (0)

// For a statement that takes a `ypObject **exc` argument, asserts that it sets *exc to one of the
// given exceptions, but that no exception has been raised. statement must include `&exc` for the
// exception argument. Example:
//
//      assert_isexception_exc(yp_lenC(yp_SyntaxError, &exc), yp_SyntaxError);
//
// TODO nohtyP does not currently make a distinction between returning and raising an exception, so
// this is currently an alias to assert_raises_exc.
#define assert_isexception_exc assert_raises_exc

// statement is only evaluated once.
#define _assert_raises(statement, n, expected, statement_fmt, expected_fmt, ...)                \
    do {                                                                                        \
        ypObject *_ypmt_RAISES_statement = statement;                                           \
        _assert_isexception(                                                                    \
                _ypmt_RAISES_statement, n, expected, statement_fmt, expected_fmt, __VA_ARGS__); \
    } while (0)

// Asserts that statement raises one of the given exceptions.
#define assert_raises(statement, ...)                                                              \
    do {                                                                                           \
        ypObject  *_ypmt_RAISES_expected[] = {__VA_ARGS__};                                        \
        yp_ssize_t _ypmt_RAISES_n = yp_lengthof_array(_ypmt_RAISES_expected);                      \
        _assert_raises((statement), _ypmt_RAISES_n, _ypmt_RAISES_expected, "%s", "%s", #statement, \
                #__VA_ARGS__);                                                                     \
    } while (0)

// statement is only evaluated once.
#define _assert_raises_exc(statement, n, expected, statement_fmt, expected_fmt, ...) \
    do {                                                                             \
        ypObject *exc = yp_None;                                                     \
        if (n < 1) {                                                                 \
            munit_error("missing expected exception in assertion");                  \
        }                                                                            \
        statement;                                                                   \
        if (!yp_isexceptionC(exc)) {                                                 \
            munit_errorf("assertion failed: " statement_fmt                          \
                         "; yp_isexceptionC(exc) (expected " expected_fmt ")",       \
                    __VA_ARGS__);                                                    \
        }                                                                            \
        if (!yp_isexception_arrayC(exc, n, expected)) {                              \
            munit_errorf("assertion failed: " statement_fmt                          \
                         "; yp_isexceptionCN(exc, N(" expected_fmt "))",             \
                    __VA_ARGS__);                                                    \
        }                                                                            \
    } while (0)

// For a statement that takes a `ypObject **exc` argument, asserts that it raises one of the given
// exceptions. statement must include `&exc` for the exception argument. Example:
//
//      assert_raises_exc(yp_lenC(obj, &exc), yp_MethodError);
#define assert_raises_exc(statement, ...)                                                  \
    do {                                                                                   \
        ypObject  *_ypmt_RAISES_expected[] = {__VA_ARGS__};                                \
        yp_ssize_t _ypmt_RAISES_n = yp_lengthof_array(_ypmt_RAISES_expected);              \
        _assert_raises_exc((statement), _ypmt_RAISES_n, _ypmt_RAISES_expected, "%s", "%s", \
                #statement, #__VA_ARGS__);                                                 \
    } while (0)

// A version of _assert_typeC that ensures a_statement and b_statement do not raise an exception via
// a `ypObject **exc` argument. a_statement and b_statement are only evaluated once.
#define _assert_typeC_exc(T, a_statement, op, b_statement, val_pri, a_fmt, b_fmt, ...)       \
    do {                                                                                     \
        T _ypmt_TYPEC_a;                                                                     \
        T _ypmt_TYPEC_b;                                                                     \
        _assert_not_raises_exc(_ypmt_TYPEC_a = (a_statement); _ypmt_TYPEC_b = (b_statement), \
                               a_fmt " " #op " " b_fmt, __VA_ARGS__);                        \
        _assert_typeC(_ypmt_TYPEC_a, op, _ypmt_TYPEC_b, val_pri, a_fmt, b_fmt, __VA_ARGS__); \
    } while (0)

// Asserts that neither a nor b raise an exception via a `ypObject **exc` argument. a and b must
// evaluate to the appropriate type, and at least one of them must be a function call using `&exc`
// for the exception argument. Example:
//
//      assert_ssizeC_exc(yp_findC(obj, item, &exc), ==, 3);
#define assert_intC_exc(a, op, b) _assert_typeC_exc(yp_int_t, a, op, b, PRIint, "%s", "%s", #a, #b)
#define assert_ssizeC_exc(a, op, b) \
    _assert_typeC_exc(yp_ssize_t, a, op, b, PRIssize, "%s", "%s", #a, #b)
#define assert_hashC_exc(a, op, b) \
    _assert_typeC_exc(yp_hash_t, a, op, b, PRIssize, "%s", "%s", #a, #b)

// A version of _assert_typeC that ensures a_statement raises one of the given exceptions via a
// `ypObject **exc` argument. b_statement must not raise an exception. a_statement and b_statement
// are only evaluated once.
#define _assert_typeC_raises_exc(T, a_statement, op, b_statement, excs, val_pri, a_fmt, b_fmt,    \
        excs_fmt, a_str, b_str, excs_str)                                                         \
    do {                                                                                          \
        ypObject  *_ypmt_TYPEC_excs[] = {UNPACK excs};                                            \
        yp_ssize_t _ypmt_TYPEC_n = yp_lengthof_array(_ypmt_TYPEC_excs);                           \
        T          _ypmt_TYPEC_a;                                                                 \
        T          _ypmt_TYPEC_b;                                                                 \
        _assert_raises_exc(_ypmt_TYPEC_a = (a_statement), _ypmt_TYPEC_n, _ypmt_TYPEC_excs, a_fmt, \
                excs_fmt, a_str, excs_str);                                                       \
        _assert_not_raises_exc(_ypmt_TYPEC_b = (b_statement), b_fmt, b_str);                      \
        _assert_typeC(_ypmt_TYPEC_a, op, _ypmt_TYPEC_b, val_pri, a_fmt, b_fmt, a_str, b_str);     \
    } while (0)

// Asserts that a raises one of the given exceptions via a `ypObject **exc` argument. b must not
// raise an exception. a and b must evaluate to the appropriate type, and a must be a function call
// using `&exc` for the exception argument. Example:
//
//      assert_ssizeC_raises_exc(yp_findC(obj, item, &exc), ==, -1, yp_TypeError);
#define assert_intC_raises_exc(a, op, b, ...) \
    _assert_typeC_raises_exc(                 \
            yp_int_t, a, op, b, (__VA_ARGS__), PRIint, "%s", "%s", "%s", #a, #b, #__VA_ARGS__)
#define assert_floatC_raises_exc(a, op, b, ...) \
    _assert_typeC_raises_exc(                   \
            yp_float_t, a, op, b, (__VA_ARGS__), "lf", "%s", "%s", "%s", #a, #b, #__VA_ARGS__)
#define assert_ssizeC_raises_exc(a, op, b, ...) \
    _assert_typeC_raises_exc(                   \
            yp_ssize_t, a, op, b, (__VA_ARGS__), PRIssize, "%s", "%s", "%s", #a, #b, #__VA_ARGS__)
#define assert_hashC_raises_exc(a, op, b, ...) \
    _assert_typeC_raises_exc(                  \
            yp_hash_t, a, op, b, (__VA_ARGS__), PRIssize, "%s", "%s", "%s", #a, #b, #__VA_ARGS__)

// value is the expected value, either yp_True or yp_False; not_value is the negation of value.
#define _assert_bool(obj, value, not_value, obj_fmt, ...)                                        \
    do {                                                                                         \
        if (obj == value) {                                                                      \
            /* pass */                                                                           \
        } else if (obj == not_value) {                                                           \
            munit_errorf("assertion failed: " obj_fmt " == " #value, __VA_ARGS__);               \
        } else {                                                                                 \
            _ypmt_error_exception(obj, "expected a bool or an exception", obj_fmt, __VA_ARGS__); \
        }                                                                                        \
    } while (0)

#define assert_falsy(obj)                                                         \
    do {                                                                          \
        ypObject *_ypmt_FALSY_result = yp_bool(obj);                              \
        _assert_bool(_ypmt_FALSY_result, yp_False, yp_True, "yp_bool(%s)", #obj); \
    } while (0)

#define assert_truthy(obj)                                                         \
    do {                                                                           \
        ypObject *_ypmt_TRUTHY_result = yp_bool(obj);                              \
        _assert_bool(_ypmt_TRUTHY_result, yp_True, yp_False, "yp_bool(%s)", #obj); \
    } while (0)

// Cheeky little hack to make assert_obj(a, is, b) and assert_obj(a, is_not, b) work.
#define yp_is(a, b) ((a) == (b) ? yp_True : yp_False)
#define yp_is_not(a, b) ((a) != (b) ? yp_True : yp_False)

// TODO Print the values of a and b (needs yp_str)
#define _assert_obj(a, op, b, a_fmt, b_fmt, ...)                                              \
    do {                                                                                      \
        ypObject *_ypmt_OBJ_result = yp_##op(a, b);                                           \
        _assert_bool(_ypmt_OBJ_result, yp_True, yp_False, "yp_" #op "(" a_fmt ", " b_fmt ")", \
                __VA_ARGS__);                                                                 \
    } while (0)

// op can be: is, is_not, lt, le, eq, ne, ge, gt, contains, in, not_in, isdisjoint, issubset,
// issuperset, startswith, endswith.
#define assert_obj(a, op, b)                                           \
    do {                                                               \
        ypObject *_ypmt_OBJ_a = (a);                                   \
        ypObject *_ypmt_OBJ_b = (b);                                   \
        _assert_obj(_ypmt_OBJ_a, op, _ypmt_OBJ_b, "%s", "%s", #a, #b); \
    } while (0)

#define _assert_type_is(obj, expected, obj_fmt, expected_fmt, ...)                           \
    do {                                                                                     \
        ypObject *_ypmt_TYPE_type_obj = yp_type(obj);                                        \
        _assert_ptr(_ypmt_TYPE_type_obj, ==, expected, "yp_type(" obj_fmt ")", expected_fmt, \
                __VA_ARGS__);                                                                \
    } while (0)

// yp_type(obj) == expected; obj is not an exception.
#define assert_type_is(obj, expected)                                                      \
    do {                                                                                   \
        ypObject *_ypmt_TYPE_obj = (obj);                                                  \
        ypObject *_ypmt_TYPE_expected = (expected);                                        \
        _assert_not_exception(_ypmt_TYPE_obj, "%s", #obj);                                 \
        _assert_type_is(_ypmt_TYPE_obj, _ypmt_TYPE_expected, "%s", "%s", #obj, #expected); \
    } while (0)

#define _assert_same_type(a, b, a_fmt, b_fmt, ...)                                  \
    do {                                                                            \
        ypObject *_ypmt_TYPE_type_a = yp_type(a);                                   \
        ypObject *_ypmt_TYPE_type_b = yp_type(b);                                   \
        _assert_ptr(_ypmt_TYPE_type_a, ==, _ypmt_TYPE_type_b, "yp_type(" a_fmt ")", \
                "yp_type(" b_fmt ")", __VA_ARGS__);                                 \
    } while (0)

// yp_type(a) == yp_type(b); neither a nor b are exceptions.
#define assert_same_type(a, b)                                             \
    do {                                                                   \
        ypObject *_ypmt_TYPE_a = (a);                                      \
        ypObject *_ypmt_TYPE_b = (b);                                      \
        _assert_not_exception(_ypmt_TYPE_a, "%s", #a);                     \
        _assert_not_exception(_ypmt_TYPE_b, "%s", #b);                     \
        _assert_same_type(_ypmt_TYPE_a, _ypmt_TYPE_b, "%s", "%s", #a, #b); \
    } while (0)

#define _assert_eq_same_type(a, b, a_fmt, b_fmt, ...)       \
    do {                                                    \
        _assert_obj(a, eq, b, a_fmt, b_fmt, __VA_ARGS__);   \
        _assert_same_type(a, b, a_fmt, b_fmt, __VA_ARGS__); \
    } while (0)

// Asserts that a and b are equal and of the same type.
#define assert_eq_same_type(a, b)                                                   \
    do {                                                                            \
        ypObject *_ypmt_EQ_TYPE_a = (a);                                            \
        ypObject *_ypmt_EQ_TYPE_b = (b);                                            \
        _assert_eq_same_type(_ypmt_EQ_TYPE_a, _ypmt_EQ_TYPE_b, "%s", "%s", #a, #b); \
    } while (0)

// XXX expected must be a yp_ssize_t.
#define _assert_len(obj, expected, obj_fmt, expected_fmt, ...)                           \
    do {                                                                                 \
        yp_ssize_t _ypmt_LEN_actual;                                                     \
        _assert_not_raises_exc(_ypmt_LEN_actual = yp_lenC(obj, &exc),                    \
                "yp_lenC(" obj_fmt ", &exc) == " expected_fmt, __VA_ARGS__);             \
        if (_ypmt_LEN_actual != expected) {                                              \
            munit_errorf("assertion failed: yp_lenC(" obj_fmt ", &exc) == " expected_fmt \
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

// items and item_strs must be arrays. item_strs are not formatted: the variable arguments apply
// only to obj_fmt.
#define _assert_sequence(obj, items, obj_fmt, item_strs, ...)                                   \
    do {                                                                                        \
        yp_ssize_t _ypmt_SEQ_n = yp_lengthof_array(items);                                      \
        yp_ssize_t _ypmt_SEQ_i;                                                                 \
        _assert_len(obj, _ypmt_SEQ_n, obj_fmt, "%" PRIssize, __VA_ARGS__, _ypmt_SEQ_n);         \
        for (_ypmt_SEQ_i = 0; _ypmt_SEQ_i < _ypmt_SEQ_n; _ypmt_SEQ_i++) {                       \
            ypObject *_ypmt_SEQ_actual = yp_getindexC(obj, _ypmt_SEQ_i);                        \
            _assert_not_exception(_ypmt_SEQ_actual, "yp_getindexC(" obj_fmt ", %" PRIssize ")", \
                    __VA_ARGS__, _ypmt_SEQ_i);                                                  \
            _assert_eq_same_type(_ypmt_SEQ_actual, items[_ypmt_SEQ_i],                          \
                    "yp_getindexC(" obj_fmt ", %" PRIssize ")", "%s", __VA_ARGS__, _ypmt_SEQ_i, \
                    item_strs[_ypmt_SEQ_i]);                                                    \
            yp_decref(_ypmt_SEQ_actual);                                                        \
        }                                                                                       \
    } while (0)

// Asserts that obj is a sequence containing exactly the given items in that order. Items are
// compared by nohtyP equality (i.e. yp_eq) and type. Validates yp_lenC and yp_getindexC.
#define assert_sequence(obj, ...)                                                          \
    do {                                                                                   \
        ypObject *_ypmt_SEQ_obj = (obj);                                                   \
        ypObject *_ypmt_SEQ_items[] = {__VA_ARGS__};                                       \
        char     *_ypmt_SEQ_item_strs[] = {STRINGIFY(__VA_ARGS__)};                        \
        _assert_sequence(_ypmt_SEQ_obj, _ypmt_SEQ_items, "%s", _ypmt_SEQ_item_strs, #obj); \
    } while (0)

extern int _assert_setlike_helper(ypObject *mi, yp_uint64_t *mi_state, yp_ssize_t n,
        ypObject **items, ypObject **actual, yp_ssize_t *items_i);

// items and item_strs must be arrays. item_strs are not formatted: the variable arguments apply
// only to obj_fmt.
#define _assert_setlike(obj, items, obj_fmt, item_strs, ...)                                       \
    do {                                                                                           \
        yp_ssize_t  _ypmt_SET_n = yp_lengthof_array(items);                                        \
        int         _ypmt_SET_found[yp_lengthof_array(items)] = {0};                               \
        yp_uint64_t _ypmt_SET_mi_state;                                                            \
        ypObject   *_ypmt_SET_mi;                                                                  \
        ypObject   *_ypmt_SET_actual;                                                              \
        yp_ssize_t  _ypmt_SET_i;                                                                   \
                                                                                                   \
        _assert_len(obj, _ypmt_SET_n, obj_fmt, "%" PRIssize, __VA_ARGS__, _ypmt_SET_n);            \
        _assert_not_raises(_ypmt_SET_mi = yp_miniiter(obj, &_ypmt_SET_mi_state),                   \
                "yp_miniiter(" obj_fmt ", &mi_state)", __VA_ARGS__);                               \
                                                                                                   \
        while (_assert_setlike_helper(_ypmt_SET_mi, &_ypmt_SET_mi_state, _ypmt_SET_n, items,       \
                &_ypmt_SET_actual /*new ref*/, &_ypmt_SET_i)) {                                    \
            _assert_not_exception(_ypmt_SET_actual,                                                \
                    "yp_miniiter_next(yp_miniiter(" obj_fmt ", &mi_state), &mi_state)",            \
                    __VA_ARGS__);                                                                  \
            if (_ypmt_SET_i >= _ypmt_SET_n) {                                                      \
                munit_errorf("unexpected item yielded from " obj_fmt, __VA_ARGS__);                \
            }                                                                                      \
            if (_ypmt_SET_found[_ypmt_SET_i]) {                                                    \
                munit_errorf(                                                                      \
                        "%s yielded twice from " obj_fmt, item_strs[_ypmt_SET_i], __VA_ARGS__);    \
            }                                                                                      \
            _ypmt_SET_found[_ypmt_SET_i] = TRUE;                                                   \
                                                                                                   \
            /* We already know from _assert_setlike_helper that _ypmt_SET_actual equals            \
             * items[_ypmt_SET_i]. Now we need to check that they're the same type. */             \
            _assert_same_type(_ypmt_SET_actual, items[_ypmt_SET_i], "<item in " obj_fmt ">", "%s", \
                    __VA_ARGS__, item_strs[_ypmt_SET_i]);                                          \
                                                                                                   \
            yp_decref(_ypmt_SET_actual);                                                           \
        }                                                                                          \
                                                                                                   \
        for (_ypmt_SET_i = 0; _ypmt_SET_i < _ypmt_SET_n; _ypmt_SET_i++) {                          \
            if (!_ypmt_SET_found[_ypmt_SET_i]) {                                                   \
                munit_errorf("%s not yielded from " obj_fmt, item_strs[_ypmt_SET_i], __VA_ARGS__); \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        yp_decref(_ypmt_SET_mi);                                                                   \
    } while (0)

// Asserts that obj is a set containing exactly the given items, in any order, without duplicates.
// Items are compared by nohtyP equality (i.e. yp_eq) and type. Validates yp_lenC and yp_miniiter.
// TODO Once the order items are yielded is guaranteed, we can make the order important.
#define assert_setlike(obj, ...)                                                          \
    do {                                                                                  \
        ypObject *_ypmt_SET_obj = (obj);                                                  \
        ypObject *_ypmt_SET_items[] = {__VA_ARGS__};                                      \
        char     *_ypmt_SET_item_strs[] = {STRINGIFY(__VA_ARGS__)};                       \
        _assert_setlike(_ypmt_SET_obj, _ypmt_SET_items, "%s", _ypmt_SET_item_strs, #obj); \
    } while (0)

extern int _assert_mapping_helper(ypObject *mi, yp_uint64_t *mi_state, yp_ssize_t n,
        ypObject **items, ypObject **actual_key, ypObject **actual_value, yp_ssize_t *items_i);

// items and item_strs must be arrays. item_strs are not formatted: the variable arguments apply
// only to obj_fmt.
#define _assert_mapping(obj, items, obj_fmt, item_strs, ...)                                       \
    do {                                                                                           \
        yp_ssize_t  _ypmt_MAP_k = yp_lengthof_array(items) / 2;                                    \
        int         _ypmt_MAP_found[yp_lengthof_array(items) / 2] = {0};                           \
        yp_uint64_t _ypmt_MAP_mi_state;                                                            \
        ypObject   *_ypmt_MAP_mi;                                                                  \
        ypObject   *_ypmt_MAP_actual_key;                                                          \
        ypObject   *_ypmt_MAP_actual_value;                                                        \
        yp_ssize_t  _ypmt_MAP_ki;                                                                  \
                                                                                                   \
        if (yp_lengthof_array(items) % 2 != 0) {                                                   \
            munit_error("expected an even number of objects for the (key, value) pairs");          \
        }                                                                                          \
        _assert_len(obj, _ypmt_MAP_k, obj_fmt, "%" PRIssize, __VA_ARGS__, _ypmt_MAP_k);            \
        _assert_not_raises(_ypmt_MAP_mi = yp_miniiter_items(obj, &_ypmt_MAP_mi_state),             \
                "yp_miniiter_items(" obj_fmt ", &mi_state)", __VA_ARGS__);                         \
                                                                                                   \
        while (_assert_mapping_helper(_ypmt_MAP_mi, &_ypmt_MAP_mi_state, _ypmt_MAP_k, items,       \
                &_ypmt_MAP_actual_key /*new ref*/, &_ypmt_MAP_actual_value /*new ref*/,            \
                &_ypmt_MAP_ki)) {                                                                  \
            _assert_not_exception(_ypmt_MAP_actual_key,                                            \
                    "yp_miniiter_items_next(yp_miniiter_items(" obj_fmt                            \
                    ", &mi_state), &mi_state)",                                                    \
                    __VA_ARGS__);                                                                  \
            if (_ypmt_MAP_ki >= _ypmt_MAP_k) {                                                     \
                munit_errorf("unexpected item yielded from " obj_fmt, __VA_ARGS__);                \
            }                                                                                      \
            if (_ypmt_MAP_found[_ypmt_MAP_ki]) {                                                   \
                munit_errorf("%s yielded twice from " obj_fmt, item_strs[_ypmt_MAP_ki * 2],        \
                        __VA_ARGS__);                                                              \
            }                                                                                      \
            _ypmt_MAP_found[_ypmt_MAP_ki] = TRUE;                                                  \
                                                                                                   \
            /* We already know from _assert_mapping_helper that _ypmt_MAP_actual_key equals        \
             * items[_ypmt_MAP_ki * 2]. Now we need to check that they're the same type. */        \
            _assert_same_type(_ypmt_MAP_actual_key, items[_ypmt_MAP_ki * 2],                       \
                    "<key in " obj_fmt ">", "%s", __VA_ARGS__, item_strs[_ypmt_MAP_ki * 2]);       \
            _assert_eq_same_type(_ypmt_MAP_actual_value, items[_ypmt_MAP_ki * 2 + 1],              \
                    "<value " obj_fmt "[%s]>", "%s", __VA_ARGS__, item_strs[_ypmt_MAP_ki * 2],     \
                    item_strs[_ypmt_MAP_ki * 2 + 1]);                                              \
                                                                                                   \
            yp_decref(_ypmt_MAP_actual_key);                                                       \
            yp_decref(_ypmt_MAP_actual_value);                                                     \
        }                                                                                          \
                                                                                                   \
        for (_ypmt_MAP_ki = 0; _ypmt_MAP_ki < _ypmt_MAP_k; _ypmt_MAP_ki++) {                       \
            if (!_ypmt_MAP_found[_ypmt_MAP_ki]) {                                                  \
                munit_errorf(                                                                      \
                        "%s not yielded from " obj_fmt, item_strs[_ypmt_MAP_ki * 2], __VA_ARGS__); \
            }                                                                                      \
        }                                                                                          \
                                                                                                   \
        yp_decref(_ypmt_MAP_mi);                                                                   \
    } while (0)

// Asserts that obj is a mapping containing exactly the given key/value pairs, in any order, without
// duplicate keys. Values are compared by nohtyP equality (i.e. yp_eq) and type. Validates yp_lenC
// and yp_miniiter_items.
#define assert_mapping(obj, ...)                                                          \
    do {                                                                                  \
        ypObject *_ypmt_MAP_obj = (obj);                                                  \
        ypObject *_ypmt_MAP_items[] = {__VA_ARGS__};                                      \
        char     *_ypmt_MAP_item_strs[] = {STRINGIFY(__VA_ARGS__)};                       \
        _assert_mapping(_ypmt_MAP_obj, _ypmt_MAP_items, "%s", _ypmt_MAP_item_strs, #obj); \
    } while (0)

// Asserts that the first n pointer items in array are exactly the given n items in that order.
// Items are compared by C equality (i.e. ==).
#define assert_ptr_array(array, ...)                                                         \
    do {                                                                                     \
        void     **_ypmt_PTR_ARR_array = (void **)(array);                                   \
        void      *_ypmt_PTR_ARR_items[] = {__VA_ARGS__};                                    \
        char      *_ypmt_PTR_ARR_item_strs[] = {STRINGIFY(__VA_ARGS__)};                     \
        yp_ssize_t _ypmt_PTR_ARR_n = yp_lengthof_array(_ypmt_PTR_ARR_items);                 \
        yp_ssize_t _ypmt_PTR_ARR_i;                                                          \
        if (_ypmt_PTR_ARR_n < 1) {                                                           \
            munit_error("missing expected items in assertion");                              \
        }                                                                                    \
        for (_ypmt_PTR_ARR_i = 0; _ypmt_PTR_ARR_i < _ypmt_PTR_ARR_n; _ypmt_PTR_ARR_i++) {    \
            _assert_ptr(_ypmt_PTR_ARR_array[_ypmt_PTR_ARR_i], ==,                            \
                    _ypmt_PTR_ARR_items[_ypmt_PTR_ARR_i], "%s[%" PRIssize "]", "%s", #array, \
                    _ypmt_PTR_ARR_i, _ypmt_PTR_ARR_item_strs[_ypmt_PTR_ARR_i]);              \
        }                                                                                    \
    } while (0)

// Execute, assert, decref: ead. For those very small one-liner tests. Executes the expression, sets
// name to the result, executes the assertion, then discards the result. To be used like:
//
//      ead(result, yp_tupleN(0), assert_len(result, 0));
#define ead(name, expression, assertion) \
    do {                                 \
        ypObject *(name) = (expression); \
        assertion;                       \
        yp_decref(name);                 \
    } while (0)


#define _faulty_iter_test_raises(setup, iter_name, iter_expression, statement, tear_down,         \
        test_name, exc_suffix, expected, statement_str)                                           \
    do {                                                                                          \
        ypObject *iter_name;                                                                      \
        UNPACK    setup;                                                                          \
        iter_name = iter_expression;                                                              \
        _assert_raises##exc_suffix(                                                               \
                statement, 1, expected, "%s /*" test_name "*/", "yp_SyntaxError", statement_str); \
        yp_decref(iter_name);                                                                     \
        UNPACK tear_down;                                                                         \
    } while (0)

// XXX Unfortunately, we don't have a way to inject test_name into the assertion statement.
#define _faulty_iter_test_succeeds(setup, iter_name, iter_expression, statement, assertion, \
        tear_down, test_name, exc_suffix, statement_str)                                    \
    do {                                                                                    \
        ypObject *iter_name;                                                                \
        UNPACK    setup;                                                                    \
        iter_name = iter_expression;                                                        \
        _assert_not_raises##exc_suffix(statement, "%s /*" test_name "*/", statement_str);   \
        UNPACK assertion;                                                                   \
        yp_decref(iter_name);                                                               \
        UNPACK tear_down;                                                                   \
    } while (0)

// XXX yp_SyntaxError is chosen as nohtyP.c neither raises nor catches it.
#define _faulty_iter_tests(setup, iter_name, iter_supplier, statement, assertion, tear_down,      \
        exc_suffix, statement_str)                                                                \
    do {                                                                                          \
        yp_ssize_t _ypmt_FLT_ITR_len = yp_lenC_not_raises(iter_supplier);                         \
        ypObject  *_ypmt_FLT_ITR_expected[] = {yp_SyntaxError};                                   \
        if (_ypmt_FLT_ITR_len < 2) {                                                              \
            munit_error("iter_supplier must contain at least two entries");                       \
        }                                                                                         \
        /* x is an iterator that fails at the start. */                                           \
        _faulty_iter_test_raises(setup, iter_name,                                                \
                new_faulty_iter(iter_supplier, 0, yp_SyntaxError, _ypmt_FLT_ITR_len), statement,  \
                tear_down, "fail_start", exc_suffix, _ypmt_FLT_ITR_expected, statement_str);      \
        /* x is an iterator that fails mid-way. */                                                \
        _faulty_iter_test_raises(setup, iter_name,                                                \
                new_faulty_iter(iter_supplier, 1, yp_SyntaxError, _ypmt_FLT_ITR_len), statement,  \
                tear_down, "fail_mid", exc_suffix, _ypmt_FLT_ITR_expected, statement_str);        \
        /* x is an iterator with a too-small length_hint. */                                      \
        _faulty_iter_test_succeeds(setup, iter_name,                                              \
                new_faulty_iter(iter_supplier, _ypmt_FLT_ITR_len + 1, yp_SyntaxError, 1),         \
                statement, assertion, tear_down, "hint_small", exc_suffix, statement_str);        \
        /* x is an iterator with a too-large length_hint. */                                      \
        _faulty_iter_test_succeeds(setup, iter_name,                                              \
                new_faulty_iter(iter_supplier, _ypmt_FLT_ITR_len + 1, yp_SyntaxError,             \
                        _ypmt_FLT_ITR_len + 100),                                                 \
                statement, assertion, tear_down, "hint_large", exc_suffix, statement_str);        \
        /* x is an iterator with the maximum length_hint. FIXME enable. */                        \
        /*_faulty_iter_test_succeeds(setup, iter_name,                                            \
                new_faulty_iter(                                                                  \
                        iter_supplier, _ypmt_FLT_ITR_len + 1, yp_SyntaxError, yp_SSIZE_T_MAX),    \
                statement, assertion, tear_down, "hint_max", exc_suffix, statement_str);       */ \
    } while (0)

// Executes a series of tests using a "faulty iterator" that either raises an exception during
// iteration or provides a misleading length hint. The faulty iterator is assigned to iter_name and
// yields values from iter_supplier, which is evaluated once but iterated over multiple times;
// iter_supplier must contain at least two entries. statement is executed for each test, and should
// reference iter_name. In tests where statement is expected to succeed, assertion is executed to
// validate the results. setup is executed before each test, and tear_down after; setup can contain
// local variable definitions. To be used like:
//
//     faulty_iter_tests(ypObject * so, x, yp_tupleN(N(items[0], items[1])), so = yp_set(x),
//             assert_setlike(so, items[0], items[1]), yp_decref(so));
#define faulty_iter_tests(setup, iter_name, iter_supplier, statement, assertion, tear_down)        \
    do {                                                                                           \
        ypObject *_ypmt_FLT_ITR_supplier = (iter_supplier);                                        \
        char      _ypmt_FLT_ITR_statement_str[] = #statement;                                      \
        _faulty_iter_tests((setup), (iter_name), _ypmt_FLT_ITR_supplier, (statement), (assertion), \
                (tear_down), , _ypmt_FLT_ITR_statement_str);                                       \
        yp_decref(_ypmt_FLT_ITR_supplier);                                                         \
    } while (0)

// A version of faulty_iter_tests supporting statements that take a `ypObject **exc` argument. To be
// used like:
//
//     faulty_iter_tests_exc(ypObject *sq = type->newN(N(items[0], items[1])), x,
//             type->newN(N(items[2], items[3])), yp_setsliceC6(sq, 0, 2, 1, x, &exc),
//             assert_sequence(sq, items[2], items[3]), yp_decref(sq));
#define faulty_iter_tests_exc(setup, iter_name, iter_supplier, statement, assertion, tear_down)    \
    do {                                                                                           \
        ypObject *_ypmt_FLT_ITR_supplier = (iter_supplier);                                        \
        char      _ypmt_FLT_ITR_statement_str[] = #statement;                                      \
        _faulty_iter_tests((setup), (iter_name), _ypmt_FLT_ITR_supplier, (statement), (assertion), \
                (tear_down), _exc, _ypmt_FLT_ITR_statement_str);                                   \
        yp_decref(_ypmt_FLT_ITR_supplier);                                                         \
    } while (0)


// A version of yp_isexceptionCN that accepts an array.
extern int yp_isexception_arrayC(ypObject *x, yp_ssize_t n, ypObject **exceptions);


// A safe sprintf that asserts on buffer overflow. Only call for arrays of fixed size (uses
// yp_lengthof_array).
#define sprintf_array(array, fmt, ...)                                 \
    do {                                                               \
        yp_ssize_t _ypmt_SPRINTF_len = yp_lengthof_array(array);       \
        yp_ssize_t result = (yp_ssize_t)unittest_snprintf(             \
                (array), (size_t)_ypmt_SPRINTF_len, fmt, __VA_ARGS__); \
        assert_ssizeC(result, >=, 0);                                  \
        assert_ssizeC(result, <, _ypmt_SPRINTF_len);                   \
    } while (0)


// Pretty-prints the object to f, followed by a newline.
extern void pprint(FILE *f, ypObject *obj);


typedef struct _fixture_type_t fixture_type_t;
typedef struct _peer_type_t    peer_type_t;
typedef ypObject *(*objobjfunc)(ypObject *);
typedef ypObject *(*objvarargfunc)(int, ...);
typedef struct _rand_obj_supplier_memo_t rand_obj_supplier_memo_t;
typedef ypObject *(*rand_obj_supplier_func)(const rand_obj_supplier_memo_t *);
typedef struct _uniqueness_t uniqueness_t;
typedef void (*rand_objs_func)(uniqueness_t *, yp_ssize_t, ypObject **);

// Describes a single nohtyP type in the context of these tests. Allows for generic tests to be
// written that apply to an entire classification of types (i.e. test_sequence tests all sequence
// objects). Any methods or arguments here that don't apply to a given type will fail the test. May
// also be used to describe special-purpose objects (i.e. fixture_type_set_dirty is a set containing
// deleted items).
typedef struct _fixture_type_t {
    char           *name;     // The name of the type (i.e. int, bytearray, dict).
    ypObject       *yp_type;  // The type object (i.e. yp_t_float, yp_t_list).
    ypObject       *falsy;    // The falsy immortal for this type, or NULL. (Only immutables.)
    fixture_type_t *pair;     // The other type in this object pair, or points back to this type.

    rand_obj_supplier_func _new_rand;  // Internal: used by rand_obj/etc.

    objobjfunc   new_;   // The object converter, aka the single-argument constructor.
    peer_type_t *peers;  // An array of "peer types" (see peer_type_t). Null-terminated.

    // Functions for iterables, where rand_items returns objects that can be accepted by newN and
    // subsequently yielded by yp_iter. (For mappings, newN creates an object with the given keys
    // and random, unique values.)
    objvarargfunc  newN;        // Creates a iterable for the given items (i.e. yp_tupleN).
    rand_objs_func rand_items;  // Fills an array with n random objects.

    // Functions for mappings, where newK takes key/value pairs, yp_contains operates on keys, and
    // yp_getitem returns values. Use rand_items to create keys (there is no rand_keys). newK is
    // also supported for collections that can store key/value pairs (i.e. iter, tuple, and list).
    objvarargfunc  newK;         // Creates an object to hold the given key/values (i.e. yp_dictK).
    rand_objs_func rand_values;  // Fills an array with n random objects for values.

    // Similar to rand_items, except the objects returned all support ordered comparisons with each
    // other, and the items are returned in ascending order (i.e. i[0] < i[1] < ...).
    rand_objs_func rand_ordered_items;

    // Flags to describe the properties of the type.
    int is_mutable;
    int is_numeric;
    int is_iterable;
    int is_collection;  // TODO nohtyP.h calls this "container", but Python abc is collection
    int is_sequence;
    int is_string;
    int is_setlike;
    int is_mapping;
    int is_callable;
    int is_patterned;            // i.e. range doesn't store values, it stores a pattern.
    int original_object_return;  // A collection that *always* returns the object that was stored.
    int hashable_items_only;     // A collection that *requires* items to be hashable.
} fixture_type_t;

// A "peer type" is one that is similar to "self", such that it is possible under certain conditions
// to convert between the two types, to compare instances of the two types, etc. For example, int
// and float are peers: you can convert one to the other and compare them. As another example, tuple
// is a peer to all iterable types: you can convert any iterable to a tuple, although comparisons
// are not necessarily supported.
//
// Because one of the types may have restrictions on the values it can have or contain, each peer
// type includes information on how to build compatible objects. For example, it is possible to
// convert a tuple to a str, but only if the items are chrs. As another example, a float can be
// losslessly converted to an int, but only if the float has an integral value.
//
// Note that pairs, and the types themselves, are all considered peers, and are all included in
// fixture_type_t.peers. For example, a tuple can be converted to both a list and a tuple.
typedef struct _peer_type_t {
    fixture_type_t *type;         // The peer type.
    rand_objs_func  rand_items;   // As per fixture_type_t.rand_items; NULL if not supported.
    rand_objs_func  rand_values;  // As per fixture_type_t.rand_values; NULL if not supported.
} peer_type_t;

// TODO Versions of each of these that build as the mutable type and then freezes, to test that
// the freezing process still yields a viable object.
extern fixture_type_t *fixture_type_type;
extern fixture_type_t *fixture_type_NoneType;
extern fixture_type_t *fixture_type_bool;
extern fixture_type_t *fixture_type_int;
extern fixture_type_t *fixture_type_intstore;
extern fixture_type_t *fixture_type_float;
extern fixture_type_t *fixture_type_floatstore;
extern fixture_type_t *fixture_type_iter;
extern fixture_type_t *fixture_type_range;
extern fixture_type_t *fixture_type_bytes;
extern fixture_type_t *fixture_type_bytearray;
extern fixture_type_t *fixture_type_str;
extern fixture_type_t *fixture_type_chrarray;
extern fixture_type_t *fixture_type_tuple;
extern fixture_type_t *fixture_type_list;
extern fixture_type_t *fixture_type_frozenset;
extern fixture_type_t *fixture_type_set;
extern fixture_type_t *fixture_type_frozenset_dirty;
extern fixture_type_t *fixture_type_set_dirty;
extern fixture_type_t *fixture_type_frozendict;
extern fixture_type_t *fixture_type_dict;
extern fixture_type_t *fixture_type_frozendict_dirty;
extern fixture_type_t *fixture_type_dict_dirty;
extern fixture_type_t *fixture_type_function;

typedef struct _fixture_t {
    fixture_type_t *type;  // The primary type under test.
} fixture_t;

// Collects related feature types together (i.e. fixture_types_mutable are all the mutable types).
typedef struct _fixture_types_t {
    yp_ssize_t       len;    // The number of types.
    fixture_type_t **types;  // An array of types. Null-terminated.
} fixture_types_t;

// "All", except invalidated and exception.
extern fixture_types_t *fixture_types_all;

extern fixture_types_t *fixture_types_mutable;
extern fixture_types_t *fixture_types_numeric;
extern fixture_types_t *fixture_types_iterable;
extern fixture_types_t *fixture_types_collection;
extern fixture_types_t *fixture_types_sequence;
extern fixture_types_t *fixture_types_string;
extern fixture_types_t *fixture_types_setlike;
extern fixture_types_t *fixture_types_mapping;
extern fixture_types_t *fixture_types_immutable;
extern fixture_types_t *fixture_types_not_numeric;
extern fixture_types_t *fixture_types_not_iterable;
extern fixture_types_t *fixture_types_not_collection;
extern fixture_types_t *fixture_types_not_sequence;
extern fixture_types_t *fixture_types_not_string;
extern fixture_types_t *fixture_types_not_setlike;
extern fixture_types_t *fixture_types_not_mapping;
extern fixture_types_t *fixture_types_immutable_not_str;
extern fixture_types_t *fixture_types_immutable_paired;

// Arrays of MunitParameterEnum values for "type" and similar parameters (i.e. the names of types).
// Can't be included in fixture_types_t because the compiler requires this to be a constant.
extern char *param_values_types_all[];
extern char *param_values_types_mutable[];
extern char *param_values_types_numeric[];
extern char *param_values_types_iterable[];
extern char *param_values_types_collection[];
extern char *param_values_types_sequence[];
extern char *param_values_types_string[];
extern char *param_values_types_setlike[];
extern char *param_values_types_mapping[];
extern char *param_values_types_immutable[];
extern char *param_values_types_not_numeric[];
extern char *param_values_types_not_iterable[];
extern char *param_values_types_not_collection[];
extern char *param_values_types_not_sequence[];
extern char *param_values_types_not_string[];
extern char *param_values_types_not_setlike[];
extern char *param_values_types_not_mapping[];
extern char *param_values_types_immutable_not_str[];
extern char *param_values_types_immutable_paired[];

// Returns the test fixture type that corresponds with the type of the object. object cannot be
// invalidated or an exception.
// TODO Support invalidated and exception types?
extern fixture_type_t *fixture_type_from_object(ypObject *object);

// Returns true iff calling yp_hashC on object succeeds. Will assert on an unexpected exception.
// Recall that calling yp_hashC successfully may cache the hash in object.
extern int object_is_hashable(ypObject *object);


extern char param_key_type[];


// A "uniqueness tracker". Used by functions such as rand_obj_any to ensure that the random objects
// created are unique across all function calls made with that tracker. It is valid to have multiple
// trackers active in a single test: each uniqueness tracker operates independently. Use NULL for
// any uniqueness_t* parameter to disable the uniqueness check for that function call. Trackers are
// allocated with uniqueness_new and freed with uniqueness_dealloc.
typedef struct _uniqueness_t uniqueness_t;

// Returned by rand_obj_any_hashability_pair.
typedef struct _hashability_pair_t {
    ypObject *hashable;
    ypObject *unhashable;
} hashability_pair_t;

// Allocates a new uniqueness tracker.
extern uniqueness_t *uniqueness_new(void);

// Discards all references in the tracker and frees memory. uq cannot be used afterwards.
extern void uniqueness_dealloc(uniqueness_t *uq);

// Returns a random object of any type.
extern ypObject *rand_obj_any(uniqueness_t *uq);

// Returns a random mutable object of any type.
extern ypObject *rand_obj_any_mutable(uniqueness_t *uq);

// Returns a random hashable object of any type.
extern ypObject *rand_obj_any_hashable(uniqueness_t *uq);

// Returns a random hashable object of any type except str.
extern ypObject *rand_obj_any_hashable_not_str(uniqueness_t *uq);

// Returns two random objects of any type that compare equal; one is hashable and the other is not.
// While the objects are equal to each other, they will be unequal to any other object in uq.
extern hashability_pair_t rand_obj_any_hashability_pair(uniqueness_t *uq);

// Returns a random object of any non-iterable type.
extern ypObject *rand_obj_any_not_iterable(uniqueness_t *uq);

// Returns a random object of the given type. uq must be NULL for fixture_type_NoneType and
// fixture_type_bool, as there are too few values for these types to guarantee uniqueness.
extern ypObject *rand_obj(uniqueness_t *uq, fixture_type_t *type);

// Returns an iter yielding the n items in order.
extern ypObject *new_iterN(int n, ...);
extern ypObject *new_iterNV(int n, va_list args);

// Returns an object of the given outer type containing the n (key, value) pairs of the given inner
// type. The pairs are constructed by calling inner->newN, while the object is constructed by
// calling outer->new_ with a list of the pairs.
extern ypObject *new_itemsK(fixture_type_t *outer, fixture_type_t *inner, int n, ...);
extern ypObject *new_itemsKV(fixture_type_t *outer, fixture_type_t *inner, int n, va_list args);

// Returns an iterator that yields values from supplier (an iterable) until n values have been
// yielded, after which the given exception is raised. The iterator is initialized with the given
// length_hint, which may be different than the number of values actually yielded.
extern ypObject *new_faulty_iter(
        ypObject *supplier, yp_ssize_t n, ypObject *exception, yp_ssize_t length_hint);

// Initializes a ypObject* array of length n with values from expression. Expression is evaluated n
// times, however the order of evaluation is undefined. n must be an integer literal. Example:
//
//      uniqueness_t *uq = uniqueness_new();
//      ypObject *items[] = obj_array_init(5, rand_obj_any(uq));
#define obj_array_init(n, expression) {_COMMA_REPEAT##n((expression))}

// Fills the ypObject* array using the given filler. Only call for arrays of fixed size (uses
// yp_lengthof_array). Example:
//
//      uniqueness_t *uq = uniqueness_new();
//      ypObject *items[5];
//      obj_array_fill(items, uq, type->rand_items);
#define obj_array_fill(array, uq, filler) (filler)((uq), yp_lengthof_array(array), (array))

// Discards all references in the ypObject* array of length n. Skips NULL elements.
extern void obj_array_decref2(yp_ssize_t n, ypObject **array);

// Discards all references in the ypObject* array. Skips NULL elements. Only call for arrays of
// fixed size (uses yp_lengthof_array).
#define obj_array_decref(array) obj_array_decref2(yp_lengthof_array(array), (array))


// yp_lenC, asserting an exception is not raised.
yp_ssize_t yp_lenC_not_raises(ypObject *container);

// yp_asintC, asserting an exception is not raised.
// FIXME Most tests should probably be using the "index" version of this, without rounding.
yp_int_t yp_asintC_not_raises(ypObject *number);


// Simulate an out-of-memory condition after the given number of successful allocations. If
// successful is zero, the next allocation will fail. All allocations will fail once the OOM counter
// reaches zero, until it is reset by malloc_tracker_oom_after or malloc_tracker_oom_disable.
extern void malloc_tracker_oom_after(int successful);

// Allow all subsequent allocations to succeed. This can be called at any time, regardless of the
// status of the OOM counter. The OOM counter is automatically disabled at the start of each test.
extern void malloc_tracker_oom_disable(void);


extern void *malloc_tracker_malloc(yp_ssize_t *actual, yp_ssize_t size);
extern void *malloc_tracker_malloc_resize(
        yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra);
extern void malloc_tracker_free(void *p);


extern fixture_t *fixture_setup(const MunitParameter params[], void *user_data);
extern void       fixture_tear_down(fixture_t *fixture);


// Disables debugger pop-ups, which can freeze CI builds.
extern void disable_debugger_popups(void);


extern void unittest_initialize(void);

#define SUITE_OF_TESTS_DECLS(name)   \
    extern MunitTest name##_tests[]; \
    extern void      name##_initialize(void)

#define SUITE_OF_SUITES_DECLS(name)    \
    extern MunitSuite name##_suites[]; \
    extern void       name##_initialize(void)


SUITE_OF_SUITES_DECLS(munit_test);
SUITE_OF_TESTS_DECLS(test_unittest);

SUITE_OF_SUITES_DECLS(test_objects);
SUITE_OF_TESTS_DECLS(test_exception);
SUITE_OF_TESTS_DECLS(test_frozendict);
SUITE_OF_TESTS_DECLS(test_frozenset);
SUITE_OF_TESTS_DECLS(test_function);
SUITE_OF_TESTS_DECLS(test_range);
SUITE_OF_TESTS_DECLS(test_tuple);

SUITE_OF_SUITES_DECLS(test_protocols);
SUITE_OF_TESTS_DECLS(test_all);
SUITE_OF_TESTS_DECLS(test_collection);
SUITE_OF_TESTS_DECLS(test_iterable);
SUITE_OF_TESTS_DECLS(test_mapping);
SUITE_OF_TESTS_DECLS(test_sequence);
SUITE_OF_TESTS_DECLS(test_setlike);
SUITE_OF_TESTS_DECLS(test_string);


#ifdef __cplusplus
}
#endif
#endif  // yp_MUNIT_TEST_UNITTEST_H

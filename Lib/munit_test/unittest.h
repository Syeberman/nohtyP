
#ifndef yp_MUNIT_TEST_UNITTEST_H
#define yp_MUNIT_TEST_UNITTEST_H
#ifdef __cplusplus
extern "C" {
#endif

#include "nohtyP.h"

#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include <stddef.h>


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

#if defined(GCC_VER) && GCC_VER >= 40600
#define STATIC_ASSERT(cond, tag) _Static_assert(cond, #tag)
#else
#define STATIC_ASSERT(cond, tag) typedef char assert_##tag[(cond) ? 1 : -1]
#endif

// Work around a preprocessing bug in msvs_120 and earlier:
// https://stackoverflow.com/a/3985071/770500
#define _ESC(...) __VA_ARGS__

// clang-format off
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

#define _COMMA1(a) a
#define _COMMA2(a, b) a, b
#define _COMMA3(a, b, c) a, b, c
#define _COMMA4(a, b, c, d) a, b, c, d
#define _COMMA5(a, b, c, d, e) a, b, c, d, e
#define _COMMA6(a, b, c, d, e, f) a, b, c, d, e, f
#define _COMMA7(a, b, c, d, e, f, g) a, b, c, d, e, f, g
#define _COMMA8(a, b, c, d, e, f, g, h) a, b, c, d, e, f, g, h
#define _COMMA9(a, b, c, d, e, f, g, h, i) a, b, c, d, e, f, g, h, i
#define _COMMA10(a, b, c, d, e, f, g, h, i, j) a, b, c, d, e, f, g, h, i, j
#define _COMMA11(a, b, c, d, e, f, g, h, i, j, k) a, b, c, d, e, f, g, h, i, j, k
#define _COMMA12(a, b, c, d, e, f, g, h, i, j, k, l) a, b, c, d, e, f, g, h, i, j, k, l
#define _COMMA13(a, b, c, d, e, f, g, h, i, j, k, l, m) a, b, c, d, e, f, g, h, i, j, k, l, m
#define _COMMA14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) a, b, c, d, e, f, g, h, i, j, k, l, m, n
#define _COMMA15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o
#define _COMMA16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p
#define _COMMA17(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q
#define _COMMA18(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r
#define _COMMA19(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s
#define _COMMA20(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t
#define _COMMA21(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u
#define _COMMA22(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v
#define _COMMA23(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w
#define _COMMA24(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x
#define _COMMA25(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y
#define _COMMA26(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z

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
// clang-format on

// Macro redefinitions to ensure the number of arguments equals n. n must be an integer literal.
// Doesn't work for yp_tupleN/etc as those occasionally have zero arguments.
#define yp_increfN(n, ...) (yp_increfN((n), _ESC(_COMMA##n(__VA_ARGS__))))
#define yp_decrefN(n, ...) (yp_decrefN((n), _ESC(_COMMA##n(__VA_ARGS__))))
#define yp_unpackN(n, iterable, ...) (yp_unpackN((n), (iterable), _ESC(_COMMA##n(__VA_ARGS__))))

// sizeof and offsetof as yp_ssize_t, and sizeof a structure member
#define yp_sizeof(x) ((yp_ssize_t)sizeof(x))
#define yp_offsetof(structType, member) ((yp_ssize_t)offsetof(structType, member))
#define yp_sizeof_member(structType, member) yp_sizeof(((structType *)0)->member)

// Length of an array. Only call for arrays of fixed size that haven't been coerced to pointers.
#define yp_lengthof_array(x) (yp_sizeof(x) / yp_sizeof((x)[0]))

// Length of an array in a structure. Only call for arrays of fixed size.
#define yp_lengthof_array_member(structType, member) yp_lengthof_array(((structType *)0)->member)


// parameters can be NULL.
#define TEST(name, parameters)                                         \
    {                                                                  \
        "/" #name,                                    /* name */       \
                (MunitTestFunc)(name),                /* test */       \
                (MunitTestSetup)fixture_setup,        /* setup */      \
                (MunitTestTearDown)fixture_tear_down, /* tear_down */  \
                MUNIT_TEST_OPTION_NONE,               /* options */    \
                parameters                            /* parameters */ \
    }

#define SUITE_OF_TESTS(name)                             \
    {                                                    \
        "/" #name,                      /* prefix */     \
                (name##_tests),         /* tests */      \
                NULL,                   /* suites */     \
                1,                      /* iterations */ \
                MUNIT_SUITE_OPTION_NONE /* options */    \
    }

#define SUITE_OF_SUITES(name)                            \
    {                                                    \
        "/" #name,                      /* prefix */     \
                NULL,                   /* tests */      \
                (name##_suites),        /* suites */     \
                1,                      /* iterations */ \
                MUNIT_SUITE_OPTION_NONE /* options */    \
    }


// FIXME A suite of tests to ensure these assertions are working. They can be
// MUNIT_TEST_OPTION_TODO, which will fail if they pass.

#define assert_ssize(a, op, b) munit_assert_type(yp_ssize_t, PRIssize, a, op, b)
#define assert_hashC(a, b) munit_assert_type(yp_hash_t, PRIssize, a, ==, b)

// FIXME A better error message to list the exception name.
#define _assert_not_exception(obj, obj_fmt, ...)                                          \
    do {                                                                                  \
        if (yp_isexceptionC(obj)) {                                                       \
            munit_errorf("assertion failed: !yp_isexceptionC(" obj_fmt ")", __VA_ARGS__); \
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
#define _ypmt_error_exception(exc, non_exc_fmt, obj_fmt, ...) \
    do {                                                      \
        _assert_not_exception(exc, obj_fmt, __VA_ARGS__);     \
        munit_errorf(non_exc_fmt ": " obj_fmt, __VA_ARGS__);  \
    } while (0)

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

// For a function that takes `ypObject **exc`, asserts that it does not raise an exception.
// Statement must include `&exc` for the exception argument, and can include a variable assignment.
// Example:
//
//      assert_not_raises_exc(len = yp_lenC(obj, &exc));
#define assert_not_raises_exc(statement) _assert_not_raises_exc(statement, "%s", #statement)

#define assert_isexception2(obj, expected)                                               \
    do {                                                                                 \
        ypObject *_ypmt_ISEXC_obj = (obj);                                               \
        ypObject *_ypmt_ISEXC_expected = (expected);                                     \
        if (!yp_isexceptionC2(_ypmt_ISEXC_obj, _ypmt_ISEXC_expected)) {                  \
            munit_errorf("assertion failed: yp_isexceptionC2(%s, %s)", #obj, #expected); \
        }                                                                                \
    } while (0)

// A different take on munit_assert_ptr that gives greater control over the format string.
#define _assert_ptr(a, op, b, a_fmt, b_fmt, ...)                                           \
    do {                                                                                   \
        if (!(a op b)) {                                                                   \
            munit_errorf("assertion failed: " a_fmt " " #op " " b_fmt " (%p " #op " %p) ", \
                    __VA_ARGS__, a, b);                                                    \
        }                                                                                  \
    } while (0)

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

// yp_type(obj) == expected
#define assert_type_is(obj, expected)                                                        \
    do {                                                                                     \
        ypObject *_ypmt_TYPE_obj = (obj);                                                    \
        ypObject *_ypmt_TYPE_expected = (expected);                                          \
        ypObject *_ypmt_TYPE_type_obj = yp_type(_ypmt_TYPE_obj);                             \
        _assert_not_exception(_ypmt_TYPE_type_obj, "yp_type(%s)", #obj);                     \
        _assert_ptr(_ypmt_TYPE_type_obj, ==, _ypmt_TYPE_expected, "yp_type(%s)", "%s", #obj, \
                #expected);                                                                  \
    } while (0)

// XXX expected must be a yp_ssize_t!
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

// Asserts that obj is a sequence containing exactly the given n items in that order. Items are
// compared by nohtyP equality (i.e. yp_eq). Validates yp_lenC and yp_getindexC. n must be an
// integer literal.
//
// XXX This is unable to cleanup _ypmt_SEQ_actual on error, as tear_down is not called on assertion
// failures. As such, test failures may leak some memory.
#define assert_sequence(obj, n, ...)                                               \
    do {                                                                           \
        ypObject  *_ypmt_SEQ_obj = (obj);                                          \
        ypObject  *_ypmt_SEQ_items[] = {__VA_ARGS__};                              \
        char      *_ypmt_SEQ_item_strs[] = {_ESC(_STRINGIFY##n(__VA_ARGS__))};     \
        yp_ssize_t _ypmt_SEQ_i;                                                    \
        _assert_len(_ypmt_SEQ_obj, ((yp_ssize_t)(n)), "%s", #n, #obj);             \
        for (_ypmt_SEQ_i = 0; _ypmt_SEQ_i < (n); _ypmt_SEQ_i++) {                  \
            ypObject *_ypmt_SEQ_actual = yp_getindexC(_ypmt_SEQ_obj, _ypmt_SEQ_i); \
            _assert_obj(_ypmt_SEQ_actual, eq, _ypmt_SEQ_items[_ypmt_SEQ_i],        \
                    "yp_getindexC(%s, %" PRIssize ")", "%s", #obj, _ypmt_SEQ_i,    \
                    _ypmt_SEQ_item_strs[_ypmt_SEQ_i]);                             \
            yp_decref(_ypmt_SEQ_actual);                                           \
        }                                                                          \
    } while (0)

// Asserts that the first n pointer items in array are exactly the given n items in that order.
// Items are compared by C equality (i.e. ==). n must be an integer literal.
#define assert_ptr_array(array, n, ...)                                                      \
    do {                                                                                     \
        void     **_ypmt_PTR_ARR_array = (void **)(array);                                   \
        void      *_ypmt_PTR_ARR_items[] = {__VA_ARGS__};                                    \
        char      *_ypmt_PTR_ARR_item_strs[] = {_ESC(_STRINGIFY##n(__VA_ARGS__))};           \
        yp_ssize_t _ypmt_PTR_ARR_i;                                                          \
        for (_ypmt_PTR_ARR_i = 0; _ypmt_PTR_ARR_i < (n); _ypmt_PTR_ARR_i++) {                \
            _assert_ptr(_ypmt_PTR_ARR_array[_ypmt_PTR_ARR_i], ==,                            \
                    _ypmt_PTR_ARR_items[_ypmt_PTR_ARR_i], "%s[%" PRIssize "]", "%s", #array, \
                    _ypmt_PTR_ARR_i, _ypmt_PTR_ARR_item_strs[_ypmt_PTR_ARR_i]);              \
        }                                                                                    \
    } while (0)

// Execute, assert, decref: ead. For those very small one-liner tests. Executes the statement, sets
// name to the result, executes the assertion, then discards the result. To be used like:
//
//      ead(result, yp_tupleN(0), assert_len(result, 0));
// FIXME Is this a good idea? Is this the best way to do it?
#define ead(name, statement, assertion) \
    do {                                \
        ypObject *(name) = (statement); \
        assertion;                      \
        yp_decref(name);                \
    } while (0)


typedef ypObject *(*objvoidfunc)(void);
typedef ypObject *(*objvarargfunc)(int, ...);
typedef struct _rand_obj_supplier_memo_t rand_obj_supplier_memo_t;
typedef ypObject *(*rand_obj_supplier_t)(const rand_obj_supplier_memo_t *);

// Any methods or arguments here that don't apply to a given type will fail the test.
typedef struct _fixture_type_t fixture_type_t;
typedef struct _fixture_type_t {
    char           *name;   // The name of the type (i.e. int, bytearray, dict).
    ypObject       *type;   // The type object (i.e. yp_t_float, yp_t_list).
    ypObject       *falsy;  // The falsy/empty immortal for this type, or NULL. (Only immutables.)
    fixture_type_t *pair;   // The other type in this object pair, or points back to this type.

    rand_obj_supplier_t _new_rand;  // Call via rand_obj/etc.

    // Functions for non-mapping iterables, where rand_item returns an object that can be yielded by
    // the associated iterator, accepted by newN (if supported), and returned by yp_getitem (if
    // supported).
    objvarargfunc newN;       // Creates a iterable for the given items (i.e. yp_tupleN).
    objvoidfunc   rand_item;  // Creates a random object to store in the iterable.

    // Functions for mappings, where newK takes key/value pairs, yp_contains operates on keys, and
    // yp_getitem returns values.
    objvarargfunc newK;        // Creates an object to hold the given key/values (i.e. yp_dictK).
    objvoidfunc   rand_key;    // Creates a random key to store in the mapping.
    objvoidfunc   rand_value;  // Creates a random value to store in the mapping.

    // Flags to describe the properties of the type.
    int is_mutable;
    int is_numeric;
    int is_iterable;
    int is_collection;  // FIXME nohtyP.h calls this "container", but Python abc is collection
    int is_sequence;
    int is_string;
    int is_set;
    int is_mapping;
    int is_callable;
    int is_patterned;  // i.e. range doesn't store values, it stores a pattern
} fixture_type_t;

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
extern fixture_type_t *fixture_type_frozendict;
extern fixture_type_t *fixture_type_dict;
extern fixture_type_t *fixture_type_function;

typedef struct _fixture_t {
    fixture_type_t *type;  // The primary type under test.
} fixture_t;

// The number of elements in fixture_types_all (ignoring the null terminator).
#define FIXTURE_TYPES_ALL_LEN 20

// "All", except invalidated and exception.
extern fixture_type_t *fixture_types_all[];

extern fixture_type_t *fixture_types_immutable[];
extern fixture_type_t *fixture_types_numeric[];
extern fixture_type_t *fixture_types_iterable[];
extern fixture_type_t *fixture_types_collection[];
extern fixture_type_t *fixture_types_sequence[];
extern fixture_type_t *fixture_types_string[];
extern fixture_type_t *fixture_types_set[];
extern fixture_type_t *fixture_types_mapping[];

// Arrays of MunitParameterEnum values for "type" and similar parameters (i.e. the names of types).
extern char *param_values_types_all[];
extern char *param_values_types_numeric[];
extern char *param_values_types_iterable[];
extern char *param_values_types_collection[];
extern char *param_values_types_sequence[];
extern char *param_values_types_string[];
extern char *param_values_types_set[];
extern char *param_values_types_mapping[];


extern char param_key_type[];


// Returns a random object of any type.
extern ypObject *rand_obj_any(void);

// Returns a random hashable object of any type.
extern ypObject *rand_obj_any_hashable(void);

// Returns a random object of the given type.
extern ypObject *rand_obj(fixture_type_t *type);

// Returns a random hashable object of the given type. type must be immutable.
extern ypObject *rand_obj_hashable(fixture_type_t *type);


// Declares a ypObject * array of length n and populates it by executing expression n times. name
// must be a valid variable name. n must be an integer literal.
// Example:
//
//      obj_array_init(items, 5, type->rand_item());  // Declares ypObject *items[5];
#define obj_array_init(name, n, expression) ypObject *name[] = {_COMMA_REPEAT##n((expression))}

#define _obj_array_fini(array, n)                                                  \
    do {                                                                           \
        yp_ssize_t _ypmt_OBJ_ARR_i;                                                \
        for (_ypmt_OBJ_ARR_i = 0; _ypmt_OBJ_ARR_i < n; _ypmt_OBJ_ARR_i++) {        \
            if (array[_ypmt_OBJ_ARR_i] != NULL) yp_decref(array[_ypmt_OBJ_ARR_i]); \
        }                                                                          \
        memset(array, 0, n * sizeof(ypObject *)); /* FIXME necessary? */           \
    } while (0)

// Discards all references in the ypObject * array. Only call for arrays of fixed size (uses
// yp_lengthof_array).
#define obj_array_fini(name)                                           \
    do {                                                               \
        ypObject **_ypmt_OBJ_ARR_array = (name);                       \
        _obj_array_fini(_ypmt_OBJ_ARR_array, yp_lengthof_array(name)); \
    } while (0)

// Discards all references in the ypObject * array of length n. Prefer obj_array_fini when possible.
#define obj_array_fini2(name, n)                                                           \
    do {                                                                                   \
        ypObject **_ypmt_OBJ_ARR_array = (name); /* FIXME does this hide bounds checks? */ \
        ypObject **_ypmt_OBJ_ARR_n = (n);                                                  \
        _obj_array_fini(_ypmt_OBJ_ARR_array, _ypmt_OBJ_ARR_n);                             \
    } while (0)


extern void *malloc_tracker_malloc(yp_ssize_t *actual, yp_ssize_t size);
extern void *malloc_tracker_malloc_resize(
        yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra);
extern void malloc_tracker_free(void *p);


extern fixture_t *fixture_setup(const MunitParameter params[], void *user_data);
extern void       fixture_tear_down(fixture_t *fixture);


// Disables debugger pop-ups, which can freeze CI builds.
extern void disable_debugger_popups(void);


extern void unittest_initialize(void);


#ifdef __cplusplus
}
#endif
#endif  // yp_MUNIT_TEST_UNITTEST_H

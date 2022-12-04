
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

// clang-format off
#define STRINGIFY1(a) #a
#define STRINGIFY2(a, b) #a, #b
#define STRINGIFY3(a, b, c) #a, #b, #c
#define STRINGIFY4(a, b, c, d) #a, #b, #c, #d
#define STRINGIFY5(a, b, c, d, e) #a, #b, #c, #d, #e
#define STRINGIFY6(a, b, c, d, e, f) #a, #b, #c, #d, #e, #f
#define STRINGIFY7(a, b, c, d, e, f, g) #a, #b, #c, #d, #e, #f, #g
#define STRINGIFY8(a, b, c, d, e, f, g, h) #a, #b, #c, #d, #e, #f, #g, #h
#define STRINGIFY9(a, b, c, d, e, f, g, h, i) #a, #b, #c, #d, #e, #f, #g, #h, #i
#define STRINGIFY10(a, b, c, d, e, f, g, h, i, j) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j
#define STRINGIFY11(a, b, c, d, e, f, g, h, i, j, k) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k
#define STRINGIFY12(a, b, c, d, e, f, g, h, i, j, k, l) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l
#define STRINGIFY13(a, b, c, d, e, f, g, h, i, j, k, l, m) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m
#define STRINGIFY14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n
#define STRINGIFY15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o
#define STRINGIFY16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p
#define STRINGIFY17(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q
#define STRINGIFY18(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r
#define STRINGIFY19(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s
#define STRINGIFY20(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t
#define STRINGIFY21(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u
#define STRINGIFY22(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v
#define STRINGIFY23(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w
#define STRINGIFY24(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x
#define STRINGIFY25(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y
#define STRINGIFY26(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) #a, #b, #c, #d, #e, #f, #g, #h, #i, #j, #k, #l, #m, #n, #o, #p, #q, #r, #s, #t, #u, #v, #w, #x, #y, #z
// clang-format on

// sizeof and offsetof as yp_ssize_t, and sizeof a structure member
#define yp_sizeof(x) ((yp_ssize_t)sizeof(x))
#define yp_offsetof(structType, member) ((yp_ssize_t)offsetof(structType, member))
#define yp_sizeof_member(structType, member) yp_sizeof(((structType *)0)->member)

// Length of an array. Only call for arrays of fixed size that haven't been coerced to pointers.
#define yp_lengthof_array(x) (yp_sizeof(x) / yp_sizeof(x[0]))

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

// TODO Print the values of a and b (needs yp_str)
#define _assert_obj(a, op, b, a_fmt, b_fmt, ...)                                              \
    do {                                                                                      \
        ypObject *_ypmt_OBJ_result = yp_##op(a, b);                                           \
        _assert_bool(_ypmt_OBJ_result, yp_True, yp_False, "yp_" #op "(" a_fmt ", " b_fmt ")", \
                __VA_ARGS__);                                                                 \
    } while (0)

// op can be: lt, le, eq, ne, ge, gt, contains, in, not_in, isdisjoint, issubset, issuperset,
// startswith, endswith.
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
        char      *_ypmt_SEQ_item_strs[] = {STRINGIFY##n(__VA_ARGS__)};            \
        yp_ssize_t _ypmt_SEQ_i;                                                    \
        _assert_len(_ypmt_SEQ_obj, n, "%s", #n, #obj);                             \
        for (_ypmt_SEQ_i = 0; _ypmt_SEQ_i < n; _ypmt_SEQ_i++) {                    \
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
        char      *_ypmt_PTR_ARR_item_strs[] = {STRINGIFY##n(__VA_ARGS__)};                  \
        yp_ssize_t _ypmt_PTR_ARR_i;                                                          \
        for (_ypmt_PTR_ARR_i = 0; _ypmt_PTR_ARR_i < n; _ypmt_PTR_ARR_i++) {                  \
            _assert_ptr(_ypmt_PTR_ARR_array[_ypmt_PTR_ARR_i], ==,                            \
                    _ypmt_PTR_ARR_items[_ypmt_PTR_ARR_i], "%s[%" PRIssize "]", "%s", #array, \
                    _ypmt_PTR_ARR_i, _ypmt_PTR_ARR_item_strs[_ypmt_PTR_ARR_i]);              \
        }                                                                                    \
    } while (0)


typedef ypObject *(*objvoidfunc)(void);
typedef ypObject *(*objvarargfunc)(int, ...);
typedef void (*voidobjpobjpfunc)(ypObject **, ypObject **);
typedef struct _rand_obj_supplier_memo_t rand_obj_supplier_memo_t;
typedef ypObject *(*rand_obj_supplier_t)(rand_obj_supplier_memo_t *);

// Any methods or arguments here that don't apply to a given type will fail the test.
typedef struct _fixture_type_t fixture_type_t;
typedef struct _fixture_type_t {
    char           *name;  // The name of the type (i.e. int, bytearray, dict).
    ypObject       *type;  // The type object (i.e. yp_t_float, yp_t_list).
    fixture_type_t *pair;  // The other type in this object pair, or points back to this type.

    rand_obj_supplier_t new_rand;  // Creates a random object, with visitor supplying sub-objects.
    objvarargfunc newN;  // Creates an object to hold the given values (i.e. yp_tupleN, yp_int where
                         // n<2, yp_dict_fromkeysN where value=yp_None).
    objvarargfunc newK;  // Creates an object to hold the given key/values (i.e. yp_dictK).

    // FIXME getitem doesn't apply to set, so how to describe? And dict.in refers to keys!
    objvoidfunc      rand_item;       // Creates a random value as would be returned by yp_getitem.
    voidobjpobjpfunc rand_key_value;  // Creates a random key/value pair as would be yielded by
                                      // yp_iter_items.

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
    // FIXME Is there a better word? The Python docs just say "Ranges implement all of the common
    // sequence operations except concatenation and repetition".
    // FIXME "is_insertion_ordered" is nice because when dict/set are made ordered, this applies.
    int is_insertion_ordered;  // i.e. range doesn't support concat, repeat, newN, etc
} fixture_type_t;

extern fixture_type_t fixture_type_type;
extern fixture_type_t fixture_type_NoneType;
extern fixture_type_t fixture_type_bool;
extern fixture_type_t fixture_type_int;
extern fixture_type_t fixture_type_intstore;
extern fixture_type_t fixture_type_float;
extern fixture_type_t fixture_type_floatstore;
extern fixture_type_t fixture_type_iter;
extern fixture_type_t fixture_type_range;
extern fixture_type_t fixture_type_bytes;
extern fixture_type_t fixture_type_bytearray;
extern fixture_type_t fixture_type_str;
extern fixture_type_t fixture_type_chrarray;
extern fixture_type_t fixture_type_tuple;
extern fixture_type_t fixture_type_list;
extern fixture_type_t fixture_type_frozenset;
extern fixture_type_t fixture_type_set;
extern fixture_type_t fixture_type_frozendict;
extern fixture_type_t fixture_type_dict;
extern fixture_type_t fixture_type_function;

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


extern fixture_t *fixture_setup(const MunitParameter params[], void *user_data);
extern void       fixture_tear_down(fixture_t *fixture);


extern void unittest_initialize(void);


#ifdef __cplusplus
}
#endif
#endif  // yp_MUNIT_TEST_UNITTEST_H

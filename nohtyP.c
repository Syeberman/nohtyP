/*
 * nohtyP.c - A Python-like API for C, in one .c and one .h
 *      https://github.com/Syeberman/nohtyP   [v0.1.0 $Change$]
 *      Copyright (c) 2001-2020 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/3/license.html
 */

// TODO In python_test, use of sys.maxsize needs to be replaced as appropriate with yp_sys_maxint,
// yp_sys_minint, or yp_sys_maxsize

// TODO Audit the use of leading underscore and ensure consistency

// TODO what do we gain by caching the hash?  We already jump through hoops to use the hash
// stored in the hash table where possible.

// TODO Is there a way to reduce the size of type+refcnt+len+alloclen to 64 bits, without hitting
// potential performance issues?

// TODO Do like Python and have just type+refcnt for non-containers

// TODO Python now has operator.length_hint that accepts a default=0 value to return

// TODO Move all the in-line overflow checks into macros/functions that use platform-efficient
// versions as appropriate (like
// https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html).

// TODO Use NotImplemented, instead of NotImplementedError, as per Python.

// TODO Look for all the places yp_decref, yp_eq, and others might execute arbitrary code that
// might modify the object being iterated over. OR  Add some sort of universal flag to objects to
// prevent modifications.

// TODO Look for places we use a borrowed reference from an object and then call arbitrary code:
// that code could invalidate and thus discard the reference.

// TODO Big, big difference in DLL sizes: 187 KB for MSVS 9.0 64-bit release to 1.6 MB for GCC 8
// 64-bit release. What's using up so much space? Why are GCC release builds larger than debug?
// Because we allow yp_malloc to be customized, can we lazy-load the malloc library? Are there other
// libraries we can trim off?

// TODO Invalidation is the only way immutable objects can be mutated. (Is "the only way" true?)
// However, that breaks various optimizations, like holding a pointer to a tuple's array and looping
// over it. Consider that while invalidating, the rest of the object's memory should still be in a
// consistent state. What if invalidating changed the type, replaced all references with yp_None (to
// break reference cycles, free memory as intended, etc), but otherwise left the memory entirely
// intact. (If we're replacing with yp_None for tuple/etc, perhaps we replace other data with
// zeroes, i.e. strs set to null bytes, ints to zero.)

// TODO yp_range is an easy, small type...but does it need to be part of core nohtyP.c? I think
// everything else has a place here, because they are all foundational to all the other types. But
// I don't want this file ballooning out of control, and I *do* want additional modules (like file,
// codecs, etc) as optional components that can be loaded in. range fits that bill as well.

// TODO Audit where we allocate and free memory. Allocations are explicit in the code, but perhaps
// we aren't always overallocating as well as we could be. Frees are trickier, as there could be
// other places we could free or shrink buffers where maybe we aren't today, and I want nohtyP to be
// consistent in how it retains any overallocations while also not being too wasteful. (Consider, on
// `x.clear()` or `del x[:]` or even `del x[-1]`, if the result is empty should we free the buffer,
// or keep it in case the object grows again? No clear answer, but we should be consistent.)

// TODO Review the work of the Faster CPython project:
//      https://github.com/faster-cpython/ideas/issues/72
//      https://github.com/python/cpython/issues/84297
//      https://github.com/faster-cpython/ideas

// TODO Python is not consistent in their naming of mutating vs returning functions. Compare
// bytearray.remove, which modifies, with bytearray.removeprefix, which returns a new string. (It's
// also not consistent because remove raises an error if it can't be found but removeprefix
// doesn't.) There's the "i" prefix, as in yp_add vs yp_iadd, but this is limited to the numeric
// methods and yp_irepeat (which is so-named as it's __imul__ in Python, so it's kinda also
// numeric). Plus, the "i" prefix is for a+=b, i.e. the magic syntax Python has to say "either
// modify this in place, or set it to the new value, but either works". (But I also don't like the
// "either works" part really, as it can lead to confusion.) There's also list.sort vs sorted (and
// list.reverse/reversed), which I like more but then would it be chrarray.strip vs
// chrarray.stripped? ESL programmers may have trouble here, and we'd have to break from Python's
// str.strip/etc. Really the bulk of the issues are in the str/bytes methods, as those tried hard to
// be non-mutating.

#define yp_FUTURE
#include "nohtyP.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)  // MSVC
#include <Windows.h>
#if _MSC_VER >= 1800
#include <inttypes.h>
#endif
#ifndef va_copy
#define va_copy(d, s) ((d) = (s))
#endif
#endif

#if defined(__GNUC__)  // GCC
#define GCC_VER (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#include <inttypes.h>
#include <stdint.h>
#endif

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

// Similar to PRId64 defined in inttypes.h, this chooses the appropriate format string depending on
// the compiler.
//
// - PRIint: for use with yp_int_t
// - PRIssize: for use with yp_ssize_t
#if defined(_MSC_VER)
#define PRIint "I64d"
#else
#define PRIint "lld"
#endif

#if defined(yp_ARCH_32_BIT)
#define PRIssize "d"
#elif defined(_MSC_VER)
#define PRIssize "I64d"
#elif defined(__APPLE__)
// The MacOS X 12.3 SDK defines ssize_t as long (see __darwin_ssize_t in the _types.h files).
#define PRIssize "ld"
#elif defined(PRId64)
#define PRIssize PRId64
#else
#define PRIssize "lld"
#endif

// Internal define from nohtyP.h
#define yp_UNUSED _yp_UNUSED


/*************************************************************************************************
 * Debug control
 *************************************************************************************************/
#pragma region debug

// yp_DEBUG_LEVEL controls how aggressively nohtyP should debug itself at runtime:
//  - 0: no debugging (default)
//  - 1: yp_ASSERT (minimal debugging)
//  - 10: yp_INFO (print debugging)
//  - 20: yp_DEBUG (extra print debugging)
#ifndef yp_DEBUG_LEVEL
// Check for well-known debug defines; inspired from http://nothings.org/stb/stb_h.html
#if defined(DEBUG) || defined(_DEBUG) || defined(DBG)
#if defined(NDEBUG)
#define yp_DEBUG_LEVEL 0
#else
#define yp_DEBUG_LEVEL 1
#endif
#else
#define yp_DEBUG_LEVEL 0
#endif
#endif

#define yp_ASSERT_ENABLED (yp_DEBUG_LEVEL >= 1)
#define yp_INFO_ENABLED (yp_DEBUG_LEVEL >= 10)
#define yp_DEBUG_ENABLED (yp_DEBUG_LEVEL >= 20)

// From http://stackoverflow.com/questions/5641427
#define _yp_S(x) #x
#define _yp_S_(x) _yp_S(x)
#define _yp_S__LINE__ _yp_S_(__LINE__)

#define _yp_FATAL(s_file, s_line, line_of_code, ...)                                      \
    do {                                                                                  \
        (void)fflush(NULL);                                                               \
        fprintf(stderr, "%s",                                                             \
                "Traceback (most recent call last):\n  File \"" s_file "\", line " s_line \
                "\n    " line_of_code "\n");                                              \
        fprintf(stderr, "FATAL ERROR: " __VA_ARGS__);                                     \
        fprintf(stderr, "\n");                                                            \
        abort();                                                                          \
    } while (0)
#define yp_FATAL(fmt, ...) \
    _yp_FATAL("nohtyP.c", _yp_S__LINE__, "yp_FATAL(" #fmt ", " #__VA_ARGS__ ");", fmt, __VA_ARGS__)
#define yp_FATAL1(msg) _yp_FATAL("nohtyP.c", _yp_S__LINE__, "yp_FATAL1(" #msg ");", msg)

#if yp_ASSERT_ENABLED
// TODO Get really, super-duper fancy and print an actual full stack trace.
#define _yp_ASSERT(expr, s_file, s_line, line_of_code, ...)                \
    do {                                                                   \
        if (!(expr)) _yp_FATAL(s_file, s_line, line_of_code, __VA_ARGS__); \
    } while (0)
#else
#define _yp_ASSERT(...)
#endif
#define yp_ASSERT(expr, ...)                                                               \
    _yp_ASSERT(expr, "nohtyP.c", _yp_S__LINE__, "yp_ASSERT(" #expr ", " #__VA_ARGS__ ");", \
            __VA_ARGS__)
#define yp_ASSERT1(expr) \
    _yp_ASSERT(expr, "nohtyP.c", _yp_S__LINE__, "yp_ASSERT1(" #expr ");", "assertion failed")

// Issues a breakpoint if the debugger is attached, on supported platforms
// TODO Debug only, and use only at the point the error is "raised" (rename to yp_raise?)
#if 0 && defined(_MSC_VER)
static void yp_breakonerr(ypObject *err) {
    if(!IsDebuggerPresent()) return;
    DebugBreak();
}
#else
#define yp_breakonerr(err)
#endif

#if yp_INFO_ENABLED
#define yp_INFO0(fmt)              \
    do {                           \
        (void)fflush(NULL);        \
        fprintf(stderr, fmt "\n"); \
        (void)fflush(NULL);        \
    } while (0)
#define yp_INFO(fmt, ...)                       \
    do {                                        \
        (void)fflush(NULL);                     \
        fprintf(stderr, fmt "\n", __VA_ARGS__); \
        (void)fflush(NULL);                     \
    } while (0)
#else
#define yp_INFO0(fmt)
#define yp_INFO(fmt, ...)
#endif

#if yp_DEBUG_ENABLED
#define yp_DEBUG0 yp_INFO0
#define yp_DEBUG yp_INFO
#else
#define yp_DEBUG0(fmt)
#define yp_DEBUG(fmt, ...)
#endif

// We always perform static asserts: they don't affect runtime
#if defined(GCC_VER) && GCC_VER >= 40600
#define yp_STATIC_ASSERT(cond, tag) _Static_assert(cond, #tag)
#else
#define yp_STATIC_ASSERT(cond, tag) typedef char assert_##tag[(cond) ? 1 : -1]
#endif

#pragma endregion debug


/*************************************************************************************************
 * Static assertions for nohtyP.h
 *************************************************************************************************/
#pragma region assertions

#if defined(yp_ARCH_32_BIT)
#if defined(yp_ARCH_64_BIT)
#error yp_ARCH_32_BIT and yp_ARCH_64_BIT cannot both be defined.
#endif
yp_STATIC_ASSERT(sizeof(void *) == 4, sizeof_pointer);
#elif defined(yp_ARCH_64_BIT)
yp_STATIC_ASSERT(sizeof(void *) == 8, sizeof_pointer);
#else
#error Exactly one of yp_ARCH_32_BIT or yp_ARCH_64_BIT must be defined.
#endif

yp_STATIC_ASSERT(sizeof(yp_int8_t) == 1, sizeof_int8);
yp_STATIC_ASSERT(sizeof(yp_uint8_t) == 1, sizeof_uint8);
yp_STATIC_ASSERT(sizeof(yp_int16_t) == 2, sizeof_int16);
yp_STATIC_ASSERT(sizeof(yp_uint16_t) == 2, sizeof_uint16);
yp_STATIC_ASSERT(sizeof(yp_int32_t) == 4, sizeof_int32);
yp_STATIC_ASSERT(sizeof(yp_uint32_t) == 4, sizeof_uint32);
yp_STATIC_ASSERT(sizeof(yp_int64_t) == 8, sizeof_int64);
yp_STATIC_ASSERT(sizeof(yp_uint64_t) == 8, sizeof_uint64);
yp_STATIC_ASSERT(sizeof(yp_float32_t) == 4, sizeof_float32);
yp_STATIC_ASSERT(sizeof(yp_float64_t) == 8, sizeof_float64);
yp_STATIC_ASSERT(sizeof(yp_ssize_t) == sizeof(size_t), sizeof_ssize);
yp_STATIC_ASSERT(yp_SSIZE_T_MAX == (SIZE_MAX / 2), ssize_max);

#define yp_MAX_ALIGNMENT (8)  // The maximum possible required alignment of any entity

// struct _ypObject must be 8-byte aligned in size, and must not have padding bytes
yp_STATIC_ASSERT(sizeof(struct _ypObject) % yp_MAX_ALIGNMENT == 0, sizeof_ypObject_1);
yp_STATIC_ASSERT(sizeof(struct _ypObject) == 16 + (2 * sizeof(void *)), sizeof_ypObject_2);

yp_STATIC_ASSERT(sizeof("abcd") == 5, sizeof_str_includes_null);

#pragma endregion assertions


/*************************************************************************************************
 * Internal structures and types, and related macros
 *************************************************************************************************/
#pragma region fundamentals

typedef size_t yp_uhash_t;

// ypObject_HEAD defines the initial segment of every ypObject; it must be followed by a semicolon
#define ypObject_HEAD _ypObject_HEAD

// The least-significant bit of the type code specifies if the type is immutable (0) or not
#define ypObject_TYPE_CODE(ob) (((ypObject *)(ob))->ob_type)
#define ypObject_SET_TYPE_CODE(ob, type) (ypObject_TYPE_CODE(ob) = (_yp_ob_type_t)(type))
#define ypObject_TYPE_CODE_IS_MUTABLE(type) ((type)&0x1)
#define ypObject_TYPE_CODE_AS_FROZEN(type) ((type)&0xFE)
#define ypObject_TYPE(ob) (ypTypeTable[ypObject_TYPE_CODE(ob)])
#define ypObject_IS_MUTABLE(ob) (ypObject_TYPE_CODE_IS_MUTABLE(ypObject_TYPE_CODE(ob)))
#define ypObject_REFCNT(ob) (((ypObject *)(ob))->ob_refcnt)

// Type pairs are identified by the immutable type code, as all its methods are supported by the
// immutable version
#define ypObject_TYPE_PAIR_CODE(ob) ypObject_TYPE_CODE_AS_FROZEN(ypObject_TYPE_CODE(ob))

// TODO Need two types of immortals: statically-allocated immortals (so should never be
// freed/invalidated) and overly-incref'd immortals (should be allowed to be invalidated and thus
// free any extra data, although the object itself will never be free'd as we've lost track of the
// refcounts)
#define ypObject_REFCNT_IMMORTAL _ypObject_REFCNT_IMMORTAL

// When a hash of this value is stored in ob_hash, call tp_currenthash
#define ypObject_HASH_INVALID _ypObject_HASH_INVALID

// Set ob_len or ob_alloclen to this value to signal an invalid length
#define ypObject_LEN_INVALID _ypObject_LEN_INVALID

// The largest length that can be stored in ob_len and ob_alloclen
// TODO In the C unit tests, override this value to something very small, to test overflow handling
#define ypObject_LEN_MAX ((yp_ssize_t)0x7FFFFFFF)
yp_STATIC_ASSERT(((_yp_ob_len_t)ypObject_LEN_MAX) == ypObject_LEN_MAX, LEN_MAX_fits_in_ob_len);

// The length, and allocated length, of the object
// XXX Do not set a length >ypObject_LEN_MAX, or a negative length !=ypObject_LEN_INVALID
#define ypObject_CACHED_LEN(ob) ((yp_ssize_t)((ypObject *)(ob))->ob_len)  // negative if invalid
#define ypObject_SET_CACHED_LEN(ob, len) (((ypObject *)(ob))->ob_len = (_yp_ob_len_t)(len))
#define ypObject_ALLOCLEN(ob) ((yp_ssize_t)((ypObject *)(ob))->ob_alloclen)  // negative if invalid
#define ypObject_SET_ALLOCLEN(ob, len) (((ypObject *)(ob))->ob_alloclen = (_yp_ob_len_t)(len))

// Hashes can be cached in the object for easy retrieval
#define ypObject_CACHED_HASH(ob) (((ypObject *)(ob))->ob_hash)  // HASH_INVALID if invalid

// Declares the ob_inline_data array for container object structures
#define yp_INLINE_DATA _yp_INLINE_DATA

// Base "constructor" for immortal objects
#define yp_IMMORTAL_HEAD_INIT _yp_IMMORTAL_HEAD_INIT

// Base "constructor" for immortal type objects
#define yp_TYPE_HEAD_INIT yp_IMMORTAL_HEAD_INIT(ypType_CODE, 0, ypObject_LEN_INVALID, NULL)

// Used in tp_flags
#define ypType_FLAG_IS_MAPPING (1u << 0)
#define ypType_FLAG_IS_CALLABLE (1u << 1)

#define ypObject_IS_MAPPING(ob) ((ypObject_TYPE(ob)->tp_flags & ypType_FLAG_IS_MAPPING) != 0)
#define ypObject_IS_CALLABLE(ob) ((ypObject_TYPE(ob)->tp_flags & ypType_FLAG_IS_CALLABLE) != 0)

// Many object methods follow one of these generic function signatures
typedef ypObject *(*objproc)(ypObject *);
typedef ypObject *(*objobjproc)(ypObject *, ypObject *);
typedef ypObject *(*objobjobjproc)(ypObject *, ypObject *, ypObject *);
typedef ypObject *(*objssizeproc)(ypObject *, yp_ssize_t);
typedef ypObject *(*objssizeobjproc)(ypObject *, yp_ssize_t, ypObject *);
typedef ypObject *(*objsliceproc)(ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t);
typedef ypObject *(*objsliceobjproc)(ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t, ypObject *);
typedef ypObject *(*objvalistproc)(ypObject *, int, va_list);
typedef ypObject *(*objpobjpobjproc)(ypObject *, ypObject **, ypObject **);

// Some functions have rather unique signatures
typedef ypObject *(*visitfunc)(ypObject *, void *);
typedef ypObject *(*traversefunc)(ypObject *, visitfunc, void *);
typedef ypObject *(*hashvisitfunc)(ypObject *, void *, yp_hash_t *);
typedef ypObject *(*hashfunc)(ypObject *, hashvisitfunc, void *, yp_hash_t *);
typedef ypObject *(*miniiterfunc)(ypObject *, yp_uint64_t *);
typedef ypObject *(*miniiter_items_nextfunc)(ypObject *, yp_uint64_t *, ypObject **, ypObject **);
typedef ypObject *(*miniiter_length_hintfunc)(ypObject *, yp_uint64_t *, yp_ssize_t *);
typedef ypObject *(*lenfunc)(ypObject *, yp_ssize_t *);
typedef ypObject *(*countfunc)(ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t *);
typedef enum { yp_FIND_FORWARD, yp_FIND_REVERSE } findfunc_direction;
typedef ypObject *(*findfunc)(
        ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, findfunc_direction, yp_ssize_t *);

// Suite of number methods. A placeholder for now, as the current implementation doesn't allow
// overriding yp_add et al (TODO).
typedef struct {
    objproc _placeholder;
} ypNumberMethods;

typedef struct {
    objobjproc      tp_concat;
    objssizeproc    tp_repeat;
    objssizeobjproc tp_getindex;  // if defval is NULL, raise exception if missing
    objsliceproc    tp_getslice;
    findfunc        tp_find;
    countfunc       tp_count;
    objssizeobjproc tp_setindex;
    objsliceobjproc tp_setslice;
    objssizeproc    tp_delindex;
    objsliceproc    tp_delslice;
    objobjproc      tp_append;
    objobjproc      tp_extend;
    objssizeproc    tp_irepeat;
    objssizeobjproc tp_insert;
    objssizeproc    tp_popindex;
    objproc         tp_reverse;
    objobjobjproc   tp_sort;  // FIXME only implemented for list...remove?
} ypSequenceMethods;

typedef struct {
    objobjproc tp_isdisjoint;
    objobjproc tp_issubset;
    // tp_lt is elsewhere
    objobjproc tp_issuperset;
    // tp_gt is elsewhere
    objvalistproc tp_union;
    objvalistproc tp_intersection;
    objvalistproc tp_difference;
    objobjproc    tp_symmetric_difference;
    // tp_update is elsewhere
    objvalistproc tp_intersection_update;
    objvalistproc tp_difference_update;
    objobjproc    tp_symmetric_difference_update;
    // tp_push (aka tp_set_add) is elsewhere
    objobjproc tp_pushunique;
} ypSetMethods;

typedef struct {
    miniiterfunc            tp_miniiter_keys;
    miniiterfunc            tp_miniiter_values;
    miniiterfunc            tp_miniiter_items;
    miniiter_items_nextfunc tp_miniiter_items_next;
    objproc                 tp_iter_keys;
    objproc                 tp_iter_values;
    objproc                 tp_iter_items;
    objobjobjproc           tp_popvalue;
    objpobjpobjproc         tp_popitem;  // on error, return exception, but leave *key/*value
    objobjobjproc           tp_setdefault;
    objvalistproc           tp_updateK;
} ypMappingMethods;

// FIXME Maybe this isn't "as callable", but defines the callable things associated to this object.
//  - tp_call - returns func obj for when this object is called
//  - tp_call_method(name) - returns the func implementing the method
// ...but extra flags stating if this is a static method, class method, instance method.
//
// OR, maybe tp_call should be moved to the top level.

// Callable objects defer to the function object returned by tp_call to parse the arguments.
typedef struct {
    // FIXME ...doesn't actually call the function, but returns information about how to call
    objpobjpobjproc tp_call;  // on error, return exception, but leave *function/*self
} ypCallableMethods;

// TODO I'm not convinced that the separation of __new__ and __init__ has borne fruit. I searched
// Python's source and didn't see many places where tp_new was called without tp_init. It's
// confusing for new users to know which to use (other languages don't have this distinction). It's
// surprising that __init__ is sometimes called and sometimes not. __new__ and __init__ each have to
// parse/verify their argument lists. Not to mention we aren't currently implementing inheritance
// anyway. I'm going to side-step this issue for now and give the types direct control over what
// happens when they are called with yp_func_new.

// Type objects hold pointers to each type's methods.
typedef struct {
    ypObject_HEAD;
    yp_uint64_t tp_flags;  // flags describing this type (ismapping, iscallable, etc)
    // TODO Fill tp_name and use in DEBUG statements
    // TODO Rename to qualname to follow Python?
    ypObject *tp_name;  // For printing, in format "<module>.<name>"

    // Object fundamentals
    ypObject    *tp_func_new;  // the function called when this type is called
    visitfunc    tp_dealloc;   // use yp_decref_fromdealloc(x, memo) to decref objects
    traversefunc tp_traverse;
    // TODO str, repr have the possibility of recursion; trap & test
    // TODO Instead of tp_str, make this tp_format where yp_str calls yp_format (opposite to Python)
    // TODO Maybe even leverage chrarray and make it tp_formatonto, like Python's readinto but using
    // "onto" as this should append its data. (Python uses language like "Pushes AssertionError onto
    // the stack" and "echoed onto the real sys.stdout".)
    objproc tp_str;
    objproc tp_repr;

    // Freezing, copying, and invalidating
    objproc      tp_freeze;
    objproc      tp_unfrozen_copy;
    objproc      tp_frozen_copy;
    traversefunc tp_unfrozen_deepcopy;
    traversefunc tp_frozen_deepcopy;
    objproc      tp_invalidate;

    // Boolean operations and comparisons
    objproc    tp_bool;
    objobjproc tp_lt;
    objobjproc tp_le;
    objobjproc tp_eq;
    objobjproc tp_ne;
    objobjproc tp_ge;
    objobjproc tp_gt;

    // Generic object operations
    hashfunc tp_currenthash;
    objproc  tp_close;

    // Number operations
    ypNumberMethods *tp_as_number;

    // Iterator operations
    miniiterfunc             tp_miniiter;
    miniiterfunc             tp_miniiter_reversed;
    miniiterfunc             tp_miniiter_next;
    miniiter_length_hintfunc tp_miniiter_length_hint;
    objproc                  tp_iter;
    objproc                  tp_iter_reversed;
    objobjproc tp_send;  // called for both yp_send and yp_throw (i.e. accepts exceptions)

    // Container operations
    objobjproc    tp_contains;
    lenfunc       tp_len;
    objobjproc    tp_push;
    objproc       tp_clear;  // delete references to contained objects
    objproc       tp_pop;
    objobjobjproc tp_remove;      // if onmissing is NULL, raise exception if missing
    objobjobjproc tp_getdefault;  // if defval is NULL, raise exception if missing
    objobjobjproc tp_setitem;
    objobjproc    tp_delitem;
    objvalistproc tp_update;

    // Sequence operations
    ypSequenceMethods *tp_as_sequence;

    // Set operations
    ypSetMethods *tp_as_set;

    // Mapping operations
    ypMappingMethods *tp_as_mapping;

    // Callable operations
    ypCallableMethods *tp_as_callable;
} ypTypeObject;

// The type table is defined at the bottom of this file
static ypTypeObject *ypTypeTable[255];

// Codes for the standard types (for lookup in the type table)
// clang-format off
#define ypInvalidated_CODE          (  0u)
// no mutable ypInvalidated type    (  1u)
#define ypException_CODE            (  2u)
// no mutable ypException type      (  3u)
#define ypType_CODE                 (  4u)
// no mutable ypType type           (  5u)

#define ypNoneType_CODE             (  6u)
// no mutable ypNoneType type       (  7u)
#define ypBool_CODE                 (  8u)
// no mutable ypBool type           (  9u)

#define ypInt_CODE                  ( 10u)
#define ypIntStore_CODE             ( 11u)
#define ypFloat_CODE                ( 12u)
#define ypFloatStore_CODE           ( 13u)

#define ypIter_CODE                 ( 14u)
// no mutable ypIter type           ( 15u)

#define ypBytes_CODE                ( 16u)
#define ypByteArray_CODE            ( 17u)
#define ypStr_CODE                  ( 18u)
#define ypChrArray_CODE             ( 19u)
#define ypTuple_CODE                ( 20u)
#define ypList_CODE                 ( 21u)

#define ypFrozenSet_CODE            ( 22u)
#define ypSet_CODE                  ( 23u)

#define ypFrozenDict_CODE           ( 24u)
#define ypDict_CODE                 ( 25u)

#define ypRange_CODE                ( 26u)
// no mutable ypRange type          ( 27u)

#define ypFunction_CODE             ( 28u)
// no mutable ypFunction type       ( 29u)

// clang-format on

yp_STATIC_ASSERT(_ypInt_CODE == ypInt_CODE, ypInt_CODE_matches);
yp_STATIC_ASSERT(_ypBytes_CODE == ypBytes_CODE, ypBytes_CODE_matches);
yp_STATIC_ASSERT(_ypStr_CODE == ypStr_CODE, ypStr_CODE_matches);
yp_STATIC_ASSERT(_ypFunction_CODE == ypFunction_CODE, ypFunction_CODE_matches);

// Generic versions of the methods above to return errors, usually; every method function pointer
// needs to point to a valid function (as opposed to constantly checking for NULL)
// clang-format off
#define DEFINE_GENERIC_METHODS(name, retval) \
    static ypObject *name ## _objproc(ypObject *x) { return retval; } \
    static ypObject *name ## _objobjproc(ypObject *x, ypObject *y) { return retval; } \
    static ypObject *name ## _objobjobjproc(ypObject *x, ypObject *y, ypObject *z) { return retval; } \
    static ypObject *name ## _objssizeproc(ypObject *x, yp_ssize_t i) { return retval; } \
    static ypObject *name ## _objssizeobjproc(ypObject *x, yp_ssize_t i, ypObject *y) { return retval; } \
    static ypObject *name ## _objsliceproc(ypObject *x, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k) { return retval; } \
    static ypObject *name ## _objsliceobjproc(ypObject *x, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *y) { return retval; } \
    static ypObject *name ## _objvalistproc(ypObject *x, int n, va_list args) { return retval; } \
    static ypObject *name ## _objpobjpobjproc(ypObject *x, ypObject **key, ypObject **value) { return retval; } \
    \
    static ypObject *name ## _visitfunc(ypObject *x, void *memo) { return retval; } \
    static ypObject *name ## _traversefunc(ypObject *x, visitfunc visitor, void *memo) { return retval; } \
    static ypObject *name ## _hashfunc(ypObject *x, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash) { return retval; } \
    static ypObject *name ## _miniiterfunc(ypObject *x, yp_uint64_t *state) { return retval; } \
    static ypObject *name ## _miniiter_items_nextfunc(ypObject *x, yp_uint64_t *state, ypObject **key, ypObject **value) { return retval; } \
    static ypObject *name ## _miniiter_lenhfunc(ypObject *x, yp_uint64_t *state, yp_ssize_t *length_hint) { return retval; } \
    static ypObject *name ## _lenfunc(ypObject *x, yp_ssize_t *len) { return retval; } \
    static ypObject *name ## _countfunc(ypObject *x, ypObject *y, yp_ssize_t i, yp_ssize_t j, yp_ssize_t *count) { return retval; } \
    static ypObject *name ## _findfunc(ypObject *x, ypObject *y, yp_ssize_t i, yp_ssize_t j, findfunc_direction direction, yp_ssize_t *index) { return retval; } \
    \
    static ypNumberMethods yp_UNUSED name ## _NumberMethods[1] = { { \
        *name ## _objproc \
    } }; \
    static ypSequenceMethods yp_UNUSED name ## _SequenceMethods[1] = { { \
        *name ## _objobjproc, \
        *name ## _objssizeproc, \
        *name ## _objssizeobjproc, \
        *name ## _objsliceproc, \
        *name ## _findfunc, \
        *name ## _countfunc, \
        *name ## _objssizeobjproc, \
        *name ## _objsliceobjproc, \
        *name ## _objssizeproc, \
        *name ## _objsliceproc, \
        *name ## _objobjproc, \
        *name ## _objobjproc, \
        *name ## _objssizeproc, \
        *name ## _objssizeobjproc, \
        *name ## _objssizeproc, \
        *name ## _objproc, \
        *name ## _objobjobjproc \
    } }; \
    static ypSetMethods yp_UNUSED name ## _SetMethods[1] = { { \
        *name ## _objobjproc, \
        *name ## _objobjproc, \
        *name ## _objobjproc, \
        *name ## _objvalistproc, \
        *name ## _objvalistproc, \
        *name ## _objvalistproc, \
        *name ## _objobjproc, \
        *name ## _objvalistproc, \
        *name ## _objvalistproc, \
        *name ## _objobjproc, \
        *name ## _objobjproc \
    } }; \
    static ypMappingMethods yp_UNUSED name ## _MappingMethods[1] = { { \
        *name ## _miniiterfunc, \
        *name ## _miniiterfunc, \
        *name ## _miniiterfunc, \
        *name ## _miniiter_items_nextfunc, \
        *name ## _objproc, \
        *name ## _objproc, \
        *name ## _objproc, \
        *name ## _objobjobjproc, \
        *name ## _objpobjpobjproc, \
        *name ## _objobjobjproc, \
        *name ## _objvalistproc, \
    } }; \
    static ypCallableMethods yp_UNUSED name ## _CallableMethods[1] = { { \
        *name ## _objpobjpobjproc \
    } };
// clang-format on

DEFINE_GENERIC_METHODS(MethodError, yp_MethodError);  // for methods the type doesn't support
// TODO A yp_ImmutableTypeError, subexception of yp_TypeError, for methods that are supported only
// by the mutable version. Then, add a debug yp_initialize assert to ensure all type tables uses
// this appropriately.
DEFINE_GENERIC_METHODS(TypeError, yp_TypeError);
DEFINE_GENERIC_METHODS(InvalidatedError, yp_InvalidatedError);  // for Invalidated objects
DEFINE_GENERIC_METHODS(ExceptionMethod, x);  // for exception objects; returns "self"

// For use when an object doesn't support a particular comparison operation
// FIXME Python has NotImplemented singleton instead
extern ypObject *const yp_ComparisonNotImplemented;

static ypObject *NotImplemented_comparefunc(ypObject *x, ypObject *y)
{
    return yp_ComparisonNotImplemented;
}

// For use when an object contains no references to other objects
static ypObject *NoRefs_traversefunc(ypObject *x, visitfunc visitor, void *memo) { return yp_None; }

// list/tuple internals that are shared among other types
#define ypTuple_ARRAY(sq) ((ypObject **)((ypObject *)sq)->ob_data)
#define ypTuple_LEN ypObject_CACHED_LEN

#pragma endregion fundamentals


/*************************************************************************************************
 * Helpful functions and macros
 *************************************************************************************************/
#pragma region utilities

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define MIN3(a, b, c) MIN(MIN(a, b), c)
#define MIN4(a, b, c, d) MIN(MIN(a, b), MIN(c, d))


// Functions that return nohtyP objects return the error object to "raise" it. Use this as
// "return_yp_ERR(yp_ValueError);" to return the error properly.
#define return_yp_ERR(_err)             \
    do {                                \
        ypObject *_yp_ERR_err = (_err); \
        return _yp_ERR_err;             \
    } while (0)

// Functions that return C values take a "ypObject **exc" that is only modified on error and is not
// discarded beforehand; such functions also need to return some C value on error (zero, typically).
// Use this as "return_yp_CEXC_ERR(0, exc, yp_ValueError);" to return the error properly.
#define return_yp_CEXC_ERR(retval, exc, _err) \
    do {                                      \
        ypObject *_yp_ERR_err = (_err);       \
        *(exc) = (_yp_ERR_err);               \
        return retval;                        \
    } while (0)

// Functions without outputs take a "ypObject **exc" that is only modified on error and is not
// discarded beforehand. Use this as "return_yp_EXC_ERR(exc, yp_ValueError);" to return the error
// properly.
#define return_yp_EXC_ERR(exc, _err)    \
    do {                                \
        ypObject *_yp_ERR_err = (_err); \
        *(exc) = (_yp_ERR_err);         \
        return;                         \
    } while (0)

// When an object encounters an unknown type, there are three possible cases:
//  - it's an invalidated object, so return yp_InvalidatedError
//  - it's an exception, so return that exception
//  - it's some other type, so return yp_TypeError
// TODO It'd be nice to remove a comparison from this, as a minor efficiency, but not sure how
// TODO Ensure we are using yp_BAD_TYPE in place of yp_TypeError in all the right places
// clang-format off
#define yp_BAD_TYPE(bad_ob) ( \
    ypObject_TYPE_PAIR_CODE(bad_ob) == ypInvalidated_CODE ? \
        yp_InvalidatedError : \
    ypObject_TYPE_PAIR_CODE(bad_ob) == ypException_CODE ? \
        (bad_ob) : \
    /* else */ \
        yp_TypeError)
// clang-format on
#define return_yp_BAD_TYPE(bad_ob) return_yp_ERR(yp_BAD_TYPE(bad_ob))
#define return_yp_CEXC_BAD_TYPE(retval, exc, bad_ob) \
    return_yp_CEXC_ERR((retval), (exc), yp_BAD_TYPE(bad_ob))
#define return_yp_EXC_BAD_TYPE(exc, bad_ob) return_yp_EXC_ERR((exc), yp_BAD_TYPE(bad_ob))

#define yp_IS_EXCEPTION_C(x) (ypObject_TYPE_PAIR_CODE(x) == ypException_CODE)
int yp_isexceptionC(ypObject *x) { return yp_IS_EXCEPTION_C(x); }

// sizeof and offsetof as yp_ssize_t, and sizeof a structure member
// FIXME Does casting to signed hide errors? Would the compiler warn about too-large sizes?
#define yp_sizeof(x) ((yp_ssize_t)sizeof(x))
#define yp_offsetof(structType, member) ((yp_ssize_t)offsetof(structType, member))
#define yp_sizeof_member(structType, member) yp_sizeof(((structType *)0)->member)

// Length of an array. Only call for arrays of fixed size that haven't been coerced to pointers.
#define yp_lengthof_array(x) (yp_sizeof(x) / yp_sizeof((x)[0]))

// Length of an array in a structure. Only call for arrays of fixed size.
#define yp_lengthof_array_member(structType, member) yp_lengthof_array(((structType *)0)->member)

// Versions of memcmp/etc that don't warn about yp_ssize_t mismatches.
#define yp_memcmp(x, y, l) (memcmp((x), (y), (size_t)(l)))
#define yp_memcpy(d, s, l) (memcpy((d), (s), (size_t)(l)))
#define yp_memmove(d, s, l) (memmove((d), (s), (size_t)(l)))
#define yp_memset(x, c, l) (memset((x), (c), (size_t)(l)))

// XXX Adapted from _Py_SIZE_ROUND_DOWN et al
// Below "a" is a power of 2. Round down size "n" to be a multiple of "a".
#define yp_SIZE_ROUND_DOWN(n, a) ((size_t)(n) & ~(size_t)((a)-1))
// Round up size "n" to be a multiple of "a".
#define yp_SIZE_ROUND_UP(n, a) (((size_t)(n) + (size_t)((a)-1)) & ~(size_t)((a)-1))
yp_STATIC_ASSERT(yp_sizeof(size_t) == yp_sizeof(void *), uintptr_unnecessary);
// Round pointer "p" down to the closest "a"-aligned address <= "p".
#define yp_ALIGN_DOWN(p, a) ((void *)((size_t)(p) & ~(size_t)((a)-1)))
// Round pointer "p" up to the closest "a"-aligned address >= "p".
#define yp_ALIGN_UP(p, a) ((void *)(((size_t)(p) + (size_t)((a)-1)) & ~(size_t)((a)-1)))
// Check if pointer "p" is aligned to "a"-bytes boundary.
#define yp_IS_ALIGNED(p, a) (!((size_t)(p) & (size_t)((a)-1)))

// For N functions (that take variable arguments); to be used as follows:
//      return_yp_V_FUNC(ypObject *, yp_foobarV, (x, n, args), n)
// v_func_args must end in the identifier "args", which is declared internal to the macro.
#define return_yp_V_FUNC(v_func_rettype, v_func, v_func_args, last_fixed) \
    do {                                                                  \
        v_func_rettype retval;                                            \
        va_list        args;                                              \
        va_start(args, last_fixed);                                       \
        retval = v_func v_func_args;                                      \
        va_end(args);                                                     \
        return retval;                                                    \
    } while (0)

// As above, but for functions without a return value
#define return_yp_V_FUNC_void(v_func, v_func_args, last_fixed) \
    do {                                                       \
        va_list args;                                          \
        va_start(args, last_fixed);                            \
        v_func v_func_args;                                    \
        va_end(args);                                          \
        return;                                                \
    } while (0)

// As above, but for "K"-functions
#define return_yp_K_FUNC return_yp_V_FUNC
#define return_yp_K_FUNC_void return_yp_V_FUNC_void

// For when we need to work with unsigned yp_int_t's in the math below; casting to unsigned helps
// avoid undefined behaviour on overflow.
typedef yp_uint64_t _yp_uint_t;
yp_STATIC_ASSERT(yp_sizeof(_yp_uint_t) == yp_sizeof(yp_int_t), sizeof_yp_uint_eq_yp_int);
#define yp_UINT_MATH(x, op, y) ((yp_int_t)(((_yp_uint_t)(x))op((_yp_uint_t)(y))))
#define yp_USIZE_MATH(x, op, y) ((yp_ssize_t)(((size_t)(x))op((size_t)(y))))

#if defined(_MSC_VER)
#define yp_IS_NAN _isnan
#define yp_IS_INFINITY(X) (!_finite(X) && !_isnan(X))
#define yp_IS_FINITE(X) _finite(X)
#elif defined(GCC_VER)
#define yp_IS_NAN isnan
#define yp_IS_INFINITY(X) (!finite(X) && !isnan(X))
#define yp_IS_FINITE(X) finite(X)
#else
#error Need to port Py_IS_NAN et al to nohtyP for this platform
#endif

// Prime multiplier used in string and various other hashes
// XXX Adapted from Python's _PyHASH_MULTIPLIER
#define _ypHASH_MULTIPLIER 1000003UL /* 0xf4243 */

/* Parameters used for the numeric hash implementation.  See notes for
   _Py_HashDouble in Python/pyhash.c.  Numeric hashes are based on
   reduction modulo the prime 2**_PyHASH_BITS - 1. */
// XXX Adapted from Python's pyport.h
#if defined(yp_ARCH_64_BIT)
#define _ypHASH_BITS 61
#else
#define _ypHASH_BITS 31
#endif
#define _ypHASH_MODULUS (((size_t)1 << _ypHASH_BITS) - 1)
#define _ypHASH_INF 314159
#define _ypHASH_IMAG _ypHASH_MULTIPLIER

// Return the hash of the given int; always succeeds
static yp_hash_t yp_HashInt(yp_int_t v)
{
    // TODO int is larger than hash on 32-bit systems, so this truncates data, which we don't
    // want; better is to adapt the long_hash algorithm to this datatype
    yp_hash_t hash = (yp_hash_t)v;
    if (hash == ypObject_HASH_INVALID) hash -= 1;
    return hash;
}

// Return the hash of the given double; always succeeds. inst is the object containing this value
// (recall that NaN is compared by identity).
// XXX Adapted from Python's _Py_HashDouble in pyhash.c.
static yp_hash_t yp_HashPointer(void *p);
static yp_hash_t yp_HashDouble(ypObject *inst, double v)
{
    int        e;
    yp_uhash_t sign;
    double     m;
    yp_uhash_t x, y;

    if (!yp_IS_FINITE(v)) {
        if (yp_IS_INFINITY(v)) {
            return v > 0 ? _ypHASH_INF : -_ypHASH_INF;
        } else {
            return yp_HashPointer(inst);
        }
    }

    m = frexp(v, &e);

    sign = 1;
    if (m < 0) {
        sign = (yp_uhash_t)-1;
        m = -m;
    }

    /* process 28 bits at a time;  this should work well both for binary
       and hexadecimal floating point. */
    x = 0;
    while (m) {
        x = ((x << 28) & _ypHASH_MODULUS) | x >> (_ypHASH_BITS - 28);
        m *= 268435456.0; /* 2**28 */
        e -= 28;
        y = (yp_uhash_t)m; /* pull out integer part */
        m -= (double)y;
        x += y;
        if (x >= _ypHASH_MODULUS) x -= _ypHASH_MODULUS;
    }

    /* adjust for the exponent;  first reduce it modulo _ypHASH_BITS */
    e = e >= 0 ? e % _ypHASH_BITS : _ypHASH_BITS - 1 - ((-1 - e) % _ypHASH_BITS);
    x = ((x << e) & _ypHASH_MODULUS) | x >> (_ypHASH_BITS - e);

    x = x * sign;
    if (x == (yp_uhash_t)ypObject_HASH_INVALID) x -= 1;
    return (yp_hash_t)x;
}

// Return the hash of the given pointer; always succeeds
// XXX Adapted from Python's _Py_HashPointer in pyhash.c.
static yp_hash_t yp_HashPointer(void *p)
{
    yp_hash_t x;
    size_t    y = (size_t)p;
    /* bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
       excessive hash collisions for dicts and sets */
    y = (y >> 4) | (y << (8 * yp_sizeof(void *) - 4));
    x = (yp_hash_t)y;
    if (x == ypObject_HASH_INVALID) x -= 1;
    return x;
}

// Return the hash of the given number of bytes; always succeeds
// XXX Adapted from Python's _Py_HashBytes in pyhash.c.
// TODO Python now uses pysiphash for security.
static yp_hash_t yp_HashBytes(yp_uint8_t *p, yp_ssize_t len)
{
    yp_uhash_t x;
    yp_ssize_t i;

    if (len == 0) return 0;
    x = 0;  // (yp_uhash_t) _yp_HashSecret.prefix;
    x ^= (yp_uhash_t)*p << 7;
    for (i = 0; i < len; i++) {
        x = (_ypHASH_MULTIPLIER * x) ^ (yp_uhash_t)*p++;
    }
    x ^= (yp_uhash_t)len;
    // x ^= (yp_uhash_t) _yp_HashSecret.suffix;
    if (x == (yp_uhash_t)ypObject_HASH_INVALID) {
        x = (yp_uhash_t)(ypObject_HASH_INVALID - 1);
    }
    return (yp_hash_t)x;
}

/* Hash for tuples. This is a slightly simplified version of the xxHash
   non-cryptographic hash:
   - we do not use any parallellism, there is only 1 accumulator.
   - we drop the final mixing since this is just a permutation of the
     output space: it does not help against collisions.
   - at the end, we mangle the length with a single constant.
   For the xxHash specification, see
   https://github.com/Cyan4973/xxHash/blob/master/doc/xxhash_spec.md

   Below are the official constants from the xxHash specification. Optimizing
   compilers should emit a single "rotate" instruction for the
   _PyHASH_XXROTATE() expansion. If that doesn't happen for some important
   platform, the macro could be changed to expand to a platform-specific rotate
   spelling instead.
*/
// XXX Adapted from Python's tuplehash
#ifdef yp_ARCH_64_BIT
#define _ypHASH_XXPRIME_1 ((yp_uhash_t)11400714785074694791ULL)
#define _ypHASH_XXPRIME_2 ((yp_uhash_t)14029467366897019727ULL)
#define _ypHASH_XXPRIME_5 ((yp_uhash_t)2870177450012600261ULL)
#define _ypHASH_XXROTATE(x) ((x << 31) | (x >> 33)) /* Rotate left 31 bits */
#else
#define _ypHASH_XXPRIME_1 ((yp_uhash_t)2654435761UL)
#define _ypHASH_XXPRIME_2 ((yp_uhash_t)2246822519UL)
#define _ypHASH_XXPRIME_5 ((yp_uhash_t)374761393UL)
#define _ypHASH_XXROTATE(x) ((x << 13) | (x >> 19)) /* Rotate left 13 bits */
#endif

typedef struct _yp_HashSequence_state_t {
    size_t     len;
    yp_uhash_t acc;
} yp_HashSequence_state_t;

// XXX Adapted from Python's tuplehash
static void yp_HashSequence_init(yp_HashSequence_state_t *state, yp_ssize_t len)
{
    yp_ASSERT1(len >= 0);
    state->len = (size_t)len;
    state->acc = _ypHASH_XXPRIME_5;
}

static void yp_HashSequence_next(yp_HashSequence_state_t *state, yp_hash_t lane)
{
    yp_uhash_t acc = state->acc;

    yp_ASSERT1(lane != ypObject_HASH_INVALID);
    acc += ((yp_uhash_t)lane) * _ypHASH_XXPRIME_2;
    acc = _ypHASH_XXROTATE(acc);
    acc *= _ypHASH_XXPRIME_1;

    state->acc = acc;
}

static yp_hash_t yp_HashSequence_fini(yp_HashSequence_state_t *state)
{
    yp_uhash_t acc = state->acc;

    /* Add input length, mangled to keep the historical value of hash(()). */
    acc += state->len ^ (_ypHASH_XXPRIME_5 ^ 3527539UL);

    if (acc == (yp_uhash_t)ypObject_HASH_INVALID) {
        return 1546275796;
    } else {
        return (yp_hash_t)acc;
    }
}

typedef struct {
    yp_hash_t se_hash;
    ypObject *se_key;
} ypSet_KeyEntry;

typedef struct _yp_HashSet_state_t {
    yp_ssize_t len;
    yp_uhash_t hash;
} yp_HashSet_state_t;

/* Work to increase the bit dispersion for closely spaced hash values.
   This is important because some use cases have many combinations of a
   small number of elements with nearby hashes so that many distinct
   combinations collapse to only a handful of distinct hash values. */

// XXX Adapted from Python's frozenset_hash
static yp_uhash_t _yp_HashSet_shuffle_bits(yp_uhash_t h)
{
    return ((h ^ 89869747UL) ^ (h << 16)) * 3644798167UL;
}

/* Most of the constants in this hash algorithm are randomly chosen
   large primes with "interesting bit patterns" and that passed tests
   for good collision statistics on a variety of problematic datasets
   including powersets and graph structures (such as David Eppstein's
   graph recipes in Lib/test/test_set.py) */

static void yp_HashSet_init(yp_HashSet_state_t *state, yp_ssize_t len)
{
    yp_ASSERT1(len >= 0);
    state->len = len;
    state->hash = 0;
}

static void yp_HashSet_next(yp_HashSet_state_t *state, yp_hash_t lane)
{
    /* Xor-in shuffled bits from every entry's hash field because xor is
       commutative and a frozenset hash should be independent of order. */

    yp_ASSERT1(lane != ypObject_HASH_INVALID);
    state->hash ^= _yp_HashSet_shuffle_bits((yp_uhash_t)lane);
}

// Combines yp_HashSet_init and yp_HashSet_next into a form optimized for a ypSet_KeyEntry table.
// FIXME If we want to call this multiple times, then `used` should be a parameter (the len from
// init is the total length, but this table may have a different `used` parameter).
static void yp_HashSet_next_table(
        yp_HashSet_state_t *state, ypSet_KeyEntry *table, yp_ssize_t mask, yp_ssize_t fill)
{
    yp_uhash_t      hash = state->hash;
    ypSet_KeyEntry *entry;

    yp_ASSERT1(mask >= 0 && fill >= 0 && state->len >= 0);
    yp_ASSERT1(mask + 1 >= fill && fill >= state->len);

    /* Xor-in shuffled bits from every entry's hash field because xor is
       commutative and a frozenset hash should be independent of order.

       For speed, include null entries and dummy entries and then
       subtract out their effect afterwards so that the final hash
       depends only on active entries.  This allows the code to be
       vectorized by the compiler and it saves the unpredictable
       branches that would arise when trying to exclude null and dummy
       entries on every iteration. */

    for (entry = table; entry <= &table[mask]; entry++) {
        hash ^= _yp_HashSet_shuffle_bits((yp_uhash_t)entry->se_hash);
    }

    /* Remove the effect of an odd number of NULL entries */
    if ((mask + 1 - fill) & 1) hash ^= _yp_HashSet_shuffle_bits(0);

    /* Remove the effect of an odd number of dummy entries */
    if ((fill - state->len) & 1) {
        hash ^= _yp_HashSet_shuffle_bits((yp_uhash_t)ypObject_HASH_INVALID);
    }

    state->hash = hash;
}

static yp_hash_t yp_HashSet_fini(yp_HashSet_state_t *state)
{
    yp_uhash_t hash = state->hash;

    /* Factor in the number of active entries */
    hash ^= ((yp_uhash_t)state->len + 1) * 1927868237UL;

    /* Disperse patterns arising in nested frozensets */
    hash ^= (hash >> 11) ^ (hash >> 25);
    hash = hash * 69069U + 907133923UL;

    /* -1 is reserved as an error code */
    if (hash == (yp_uhash_t)ypObject_HASH_INVALID) {
        return 590923713;
    } else {
        return (yp_hash_t)hash;
    }
}


// Maximum code point of Unicode 6.0: 0x10FFFF (1,114,111)
#define ypStringLib_MAX_UNICODE (0x10FFFFu)

/* This Unicode character will be used as replacement character during
   decoding if the errors argument is set to "replace". Note: the
   Unicode character U+FFFD is the official REPLACEMENT CHARACTER in
   Unicode 3.0. */
#define ypStringLib_UNICODE_REPLACEMENT_CHARACTER (0xFFFDu)


// Macros to work with surrogates
// XXX Adapted from Python's unicodeobject.h
#define ypStringLib_IS_SURROGATE(ch) (0xD800 <= (ch) && (ch) <= 0xDFFF)


// TODO Make this configurable via yp_initialize, and/or dynamically
static yp_ssize_t _yp_recursion_limit = 1000;

#pragma endregion utilities


/*************************************************************************************************
 * Structure definitions for the built-in objects
 *************************************************************************************************/
#pragma region object_structs


typedef yp_int32_t _yp_iter_length_hint_t;
#define ypIter_LENHINT_MAX ((yp_ssize_t)0x7FFFFFFF)

typedef ypObject *(*yp_generator_func_t)(ypObject *iterator, ypObject *value);

// _ypIterObject_HEAD shared with friendly classes below
#define _ypIterObject_HEAD_NO_PADDING      \
    ypObject_HEAD;                         \
    _yp_iter_length_hint_t ob_length_hint; \
    yp_uint32_t            ob_objlocs;     \
    yp_generator_func_t    ob_func /* force semi-colon */
// To ensure that ob_inline_data is aligned properly, we need to pad on some platforms
// TODO If we use ob_len to store the length_hint, yp_lenC would have to always call tp_len, but
// then we could trim 8 bytes off all iterators
#if defined(yp_ARCH_32_BIT)
#define _ypIterObject_HEAD         \
    _ypIterObject_HEAD_NO_PADDING; \
    void *_ob_padding /* force semi-colon */
#else
#define _ypIterObject_HEAD _ypIterObject_HEAD_NO_PADDING
#endif
typedef struct {
    _ypIterObject_HEAD;
    yp_INLINE_DATA(yp_uint8_t);
} ypIterObject;
yp_STATIC_ASSERT(yp_offsetof(ypIterObject, ob_inline_data) % yp_MAX_ALIGNMENT == 0,
        alignof_iter_inline_data);

typedef struct {
    _ypIterObject_HEAD;
    ypObject   *mi;
    yp_uint64_t mi_state;
} ypMiIterObject;

typedef struct {
    _ypIterObject_HEAD;
    ypObject *callable;
    ypObject *sentinel;
} ypCallableIterObject;


// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct {
    ypObject_HEAD;
    ypObject *name;
    ypObject *super;
} ypExceptionObject;


typedef struct _ypBoolObject {
    ypObject_HEAD;
    char value;
} ypBoolObject;


typedef struct _ypIntObject ypIntObject;


// TODO Immortal floats in nohtyP.h
typedef struct {
    ypObject_HEAD;
    yp_float_t value;
} ypFloatObject;


typedef struct _ypStringLibObject ypStringLibObject;


typedef struct {
    ypObject_HEAD;
    yp_INLINE_DATA(ypObject *);
} ypTupleObject;


// ypSet_KeyEntry defined above.
typedef struct {
    ypObject_HEAD;
    yp_ssize_t fill;  // # Active + # Dummy
    yp_INLINE_DATA(ypSet_KeyEntry);
} ypSetObject;


typedef struct {
    ypObject_HEAD;
    ypObject *keyset;
    yp_INLINE_DATA(ypObject *);
} ypDictObject;


typedef struct {
    ypObject_HEAD;
    yp_int_t start;  // all ranges of len < 1 have a start of 0
    yp_int_t step;   // all ranges of len < 2 have a step of 1
} ypRangeObject;


#pragma endregion object_structs


/*************************************************************************************************
 * Common immortals, both internal-only and exported
 *************************************************************************************************/
#pragma region common_immortals

#define yp_CONST_REF _yp_CONST_REF


#define yp_IMMORTAL_INVALIDATED(name)                                                   \
    static struct _ypObject _##name##_struct =                                          \
            _yp_IMMORTAL_HEAD_INIT(ypInvalidated_CODE, 0, _ypObject_LEN_INVALID, NULL); \
    ypObject *const name = yp_CONST_REF(name) /* force semi-colon */

// For use internally as parameter defaults to detect when an argument was not supplied. yp_None
// normally fills this purpose, but some functions need to reject None (i.e. yp_t_bytes must
// reject None for encoding and errors.)
// FIXME Could we just use NameError or some other exception?
// FIXME Unlike Python, make this official, so docstrings show "this parameter is optional, but
// doesn't have a default value".
// FIXME ...or just use yp_None
yp_IMMORTAL_INVALIDATED(yp_Arg_Missing);


static ypObject _yp_None_struct =
        yp_IMMORTAL_HEAD_INIT(ypNoneType_CODE, 0, ypObject_LEN_INVALID, NULL);
ypObject *const yp_None = yp_CONST_REF(yp_None);


// There are exactly two bool objects
// TODO Could initialize ypObject_CACHED_HASH here...in fact our value could be in CACHED_HASH.
static ypBoolObject _yp_True_struct = {yp_IMMORTAL_HEAD_INIT(ypBool_CODE, 0, 0, NULL), 1};
ypObject *const     yp_True = yp_CONST_REF(yp_True);
static ypBoolObject _yp_False_struct = {yp_IMMORTAL_HEAD_INIT(ypBool_CODE, 0, 0, NULL), 0};
ypObject *const     yp_False = yp_CONST_REF(yp_False);


#define _ypInt_PREALLOC_START (-5)
#define _ypInt_PREALLOC_END (257)
static ypIntObject _ypInt_pre_allocated[_ypInt_PREALLOC_END - _ypInt_PREALLOC_START];

#define ypInt_IS_PREALLOC(i) (_ypInt_PREALLOC_START <= i && i < _ypInt_PREALLOC_END)

// XXX Careful! Do not use ypInt_PREALLOC_REF with a value that is not preallocated.
#define ypInt_PREALLOC_REF(i) ((ypObject *)&(_ypInt_pre_allocated[(i)-_ypInt_PREALLOC_START]))

// TODO Rename to yp_int_*?  I'm OK with yp_s_* because strs are going to be used more often and
// will likely have long names already (i.e. they'll be named like the string they represent), but
// there won't be many of these, their names are short, and they're infrequently used.
ypObject *const yp_i_neg_one = ypInt_PREALLOC_REF(-1);
ypObject *const yp_i_zero = ypInt_PREALLOC_REF(0);
ypObject *const yp_i_one = ypInt_PREALLOC_REF(1);
ypObject *const yp_i_two = ypInt_PREALLOC_REF(2);


// Macros on ob_type_flags for string objects (bytes and str), used to index into
// ypStringLib_encs.
#define ypStringLib_ENC_CODE_BYTES _ypStringLib_ENC_CODE_BYTES
#define ypStringLib_ENC_CODE_LATIN_1 _ypStringLib_ENC_CODE_LATIN_1
#define ypStringLib_ENC_CODE_UCS_2 _ypStringLib_ENC_CODE_UCS_2
#define ypStringLib_ENC_CODE_UCS_4 _ypStringLib_ENC_CODE_UCS_4


// Empty bytes can be represented by this, immortal object
static ypStringLibObject _yp_bytes_empty_struct = {{ypBytes_CODE, 0, ypStringLib_ENC_CODE_BYTES,
        ypObject_REFCNT_IMMORTAL, 0, ypObject_LEN_INVALID, ypObject_HASH_INVALID, ""}};
ypObject *const          yp_bytes_empty = yp_CONST_REF(yp_bytes_empty);


// Empty strs can be represented by this, immortal object
static ypStringLibObject _yp_str_empty_struct = {{ypStr_CODE, 0, ypStringLib_ENC_CODE_LATIN_1,
        ypObject_REFCNT_IMMORTAL, 0, ypObject_LEN_INVALID, ypObject_HASH_INVALID, ""}};
ypObject *const          yp_str_empty = yp_CONST_REF(yp_str_empty);

yp_IMMORTAL_STR_LATIN_1(yp_s_ascii, "ascii");
yp_IMMORTAL_STR_LATIN_1(yp_s_latin_1, "latin-1");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_8, "utf-8");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_16, "utf-16");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_16be, "utf-16be");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_16le, "utf-16le");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_32, "utf-32");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_32be, "utf-32be");
yp_IMMORTAL_STR_LATIN_1(yp_s_utf_32le, "utf-32le");
yp_IMMORTAL_STR_LATIN_1(yp_s_ucs_2, "ucs-2");
yp_IMMORTAL_STR_LATIN_1(yp_s_ucs_4, "ucs-4");

yp_IMMORTAL_STR_LATIN_1(yp_s_strict, "strict");
yp_IMMORTAL_STR_LATIN_1(yp_s_replace, "replace");
yp_IMMORTAL_STR_LATIN_1(yp_s_ignore, "ignore");
yp_IMMORTAL_STR_LATIN_1(yp_s_xmlcharrefreplace, "xmlcharrefreplace");
yp_IMMORTAL_STR_LATIN_1(yp_s_backslashreplace, "backslashreplace");
yp_IMMORTAL_STR_LATIN_1(yp_s_surrogateescape, "surrogateescape");
yp_IMMORTAL_STR_LATIN_1(yp_s_surrogatepass, "surrogatepass");

yp_IMMORTAL_STR_LATIN_1(yp_s_slash, "/");
yp_IMMORTAL_STR_LATIN_1(yp_s_star, "*");
yp_IMMORTAL_STR_LATIN_1(yp_s_star_args, "*args");
yp_IMMORTAL_STR_LATIN_1(yp_s_star_star_kwargs, "**kwargs");

// Parameter names of the built-in functions.
yp_IMMORTAL_STR_LATIN_1_static(yp_s_base, "base");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_cls, "cls");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_encoding, "encoding");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_errors, "errors");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_i, "i");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_iterable, "iterable");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_key, "key");
// TODO Rename all obj to object? "object" is a Python keyword argument name (str(object='')). (This
// would break with Python docstrings for some built-ins, but may be where Python is headed...but
// oddly it conflicts with the object() built-in so I would expect obj like cls avoids class.)
yp_IMMORTAL_STR_LATIN_1_static(yp_s_obj, "obj");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_object, "object");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_reverse, "reverse");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_self, "self");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_sentinel, "sentinel");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_sequence, "sequence");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_source, "source");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_step, "step");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_x, "x");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_y, "y");
yp_IMMORTAL_STR_LATIN_1_static(yp_s_z, "z");


// Empty tuples can be represented by this, immortal object
static ypObject     *_yp_tuple_empty_data[1] = {NULL};
static ypTupleObject _yp_tuple_empty_struct = {{ypTuple_CODE, 0, 0, ypObject_REFCNT_IMMORTAL, 0, 0,
        ypObject_HASH_INVALID, _yp_tuple_empty_data}};
ypObject *const      yp_tuple_empty = yp_CONST_REF(yp_tuple_empty);


#define ypSet_ALLOCLEN_MIN ((yp_ssize_t)8)

// Empty frozensets can be represented by this, immortal object
static ypSet_KeyEntry _yp_frozenset_empty_data[ypSet_ALLOCLEN_MIN] = {{0}};
static ypSetObject    _yp_frozenset_empty_struct = {
        {ypFrozenSet_CODE, 0, 0, ypObject_REFCNT_IMMORTAL, 0, ypSet_ALLOCLEN_MIN,
                   ypObject_HASH_INVALID, _yp_frozenset_empty_data},
        0};
ypObject *const yp_frozenset_empty = yp_CONST_REF(yp_frozenset_empty);


// Empty frozendicts can be represented by this, immortal object
static ypObject     _yp_frozendict_empty_data[ypSet_ALLOCLEN_MIN] = {{0}};
static ypDictObject _yp_frozendict_empty_struct = {
        {ypFrozenDict_CODE, 0, 0, ypObject_REFCNT_IMMORTAL, 0, ypSet_ALLOCLEN_MIN,
                ypObject_HASH_INVALID, _yp_frozendict_empty_data},
        yp_CONST_REF(yp_frozenset_empty)};
ypObject *const yp_frozendict_empty = yp_CONST_REF(yp_frozendict_empty);


// Use yp_rangeC(0) as the standard empty object
static ypRangeObject _yp_range_empty_struct = {
        {ypRange_CODE, 0, 0, ypObject_REFCNT_IMMORTAL, 0, 0, ypObject_HASH_INVALID, NULL}, 0, 1};
ypObject *const yp_range_empty = yp_CONST_REF(yp_range_empty);


#pragma endregion common_immortals


/*************************************************************************************************
 * Locale-independent ctype.h-like macros
 * XXX This entire section is adapted from Python's pyctype.c and pyctype.h
 *************************************************************************************************/
#pragma region ctype_like
// clang-format off

#define _yp_CTF_LOWER  0x01
#define _yp_CTF_UPPER  0x02
#define _yp_CTF_ALPHA  (_yp_CTF_LOWER|_yp_CTF_UPPER)
#define _yp_CTF_DIGIT  0x04
#define _yp_CTF_ALNUM  (_yp_CTF_ALPHA|_yp_CTF_DIGIT)
#define _yp_CTF_SPACE  0x08
#define _yp_CTF_XDIGIT 0x10

// Unlike their C counterparts, the following macros are meant only for yp_uint8_t values.
#define yp_ISLOWER(c)  (_yp_ctype_table[c] & _yp_CTF_LOWER)
#define yp_ISUPPER(c)  (_yp_ctype_table[c] & _yp_CTF_UPPER)
#define yp_ISALPHA(c)  (_yp_ctype_table[c] & _yp_CTF_ALPHA)
#define yp_ISDIGIT(c)  (_yp_ctype_table[c] & _yp_CTF_DIGIT)
#define yp_ISXDIGIT(c) (_yp_ctype_table[c] & _yp_CTF_XDIGIT)
#define yp_ISALNUM(c)  (_yp_ctype_table[c] & _yp_CTF_ALNUM)
#define yp_ISSPACE(c)  (_yp_ctype_table[c] & _yp_CTF_SPACE)

#define yp_TOLOWER(c) (_yp_ctype_tolower[c])
#define yp_TOUPPER(c) (_yp_ctype_toupper[c])

// XXX Adapted from Python's pyctype.c and pyctype.h
// TODO In Python, this is a table of unsigned ints, which is unnecessarily large; contribute a fix
const yp_uint8_t _yp_ctype_table[256] = {
    0, // 0x0 '\x00'
    0, // 0x1 '\x01'
    0, // 0x2 '\x02'
    0, // 0x3 '\x03'
    0, // 0x4 '\x04'
    0, // 0x5 '\x05'
    0, // 0x6 '\x06'
    0, // 0x7 '\x07'
    0, // 0x8 '\x08'
    _yp_CTF_SPACE, // 0x9 '\t'
    _yp_CTF_SPACE, // 0xa '\n'
    _yp_CTF_SPACE, // 0xb '\v'
    _yp_CTF_SPACE, // 0xc '\f'
    _yp_CTF_SPACE, // 0xd '\r'
    0, // 0xe '\x0e'
    0, // 0xf '\x0f'
    0, // 0x10 '\x10'
    0, // 0x11 '\x11'
    0, // 0x12 '\x12'
    0, // 0x13 '\x13'
    0, // 0x14 '\x14'
    0, // 0x15 '\x15'
    0, // 0x16 '\x16'
    0, // 0x17 '\x17'
    0, // 0x18 '\x18'
    0, // 0x19 '\x19'
    0, // 0x1a '\x1a'
    0, // 0x1b '\x1b'
    0, // 0x1c '\x1c'
    0, // 0x1d '\x1d'
    0, // 0x1e '\x1e'
    0, // 0x1f '\x1f'
    _yp_CTF_SPACE, // 0x20 ' '
    0, // 0x21 '!'
    0, // 0x22 '"'
    0, // 0x23 '#'
    0, // 0x24 '$'
    0, // 0x25 '%'
    0, // 0x26 '&'
    0, // 0x27 "'"
    0, // 0x28 '('
    0, // 0x29 ')'
    0, // 0x2a '*'
    0, // 0x2b '+'
    0, // 0x2c ','
    0, // 0x2d '-'
    0, // 0x2e '.'
    0, // 0x2f '/'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x30 '0'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x31 '1'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x32 '2'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x33 '3'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x34 '4'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x35 '5'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x36 '6'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x37 '7'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x38 '8'
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, // 0x39 '9'
    0, // 0x3a ':'
    0, // 0x3b ';'
    0, // 0x3c '<'
    0, // 0x3d '='
    0, // 0x3e '>'
    0, // 0x3f '?'
    0, // 0x40 '@'
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, // 0x41 'A'
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, // 0x42 'B'
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, // 0x43 'C'
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, // 0x44 'D'
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, // 0x45 'E'
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, // 0x46 'F'
    _yp_CTF_UPPER, // 0x47 'G'
    _yp_CTF_UPPER, // 0x48 'H'
    _yp_CTF_UPPER, // 0x49 'I'
    _yp_CTF_UPPER, // 0x4a 'J'
    _yp_CTF_UPPER, // 0x4b 'K'
    _yp_CTF_UPPER, // 0x4c 'L'
    _yp_CTF_UPPER, // 0x4d 'M'
    _yp_CTF_UPPER, // 0x4e 'N'
    _yp_CTF_UPPER, // 0x4f 'O'
    _yp_CTF_UPPER, // 0x50 'P'
    _yp_CTF_UPPER, // 0x51 'Q'
    _yp_CTF_UPPER, // 0x52 'R'
    _yp_CTF_UPPER, // 0x53 'S'
    _yp_CTF_UPPER, // 0x54 'T'
    _yp_CTF_UPPER, // 0x55 'U'
    _yp_CTF_UPPER, // 0x56 'V'
    _yp_CTF_UPPER, // 0x57 'W'
    _yp_CTF_UPPER, // 0x58 'X'
    _yp_CTF_UPPER, // 0x59 'Y'
    _yp_CTF_UPPER, // 0x5a 'Z'
    0, // 0x5b '['
    0, // 0x5c '\\'
    0, // 0x5d ']'
    0, // 0x5e '^'
    0, // 0x5f '_'
    0, // 0x60 '`'
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, // 0x61 'a'
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, // 0x62 'b'
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, // 0x63 'c'
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, // 0x64 'd'
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, // 0x65 'e'
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, // 0x66 'f'
    _yp_CTF_LOWER, // 0x67 'g'
    _yp_CTF_LOWER, // 0x68 'h'
    _yp_CTF_LOWER, // 0x69 'i'
    _yp_CTF_LOWER, // 0x6a 'j'
    _yp_CTF_LOWER, // 0x6b 'k'
    _yp_CTF_LOWER, // 0x6c 'l'
    _yp_CTF_LOWER, // 0x6d 'm'
    _yp_CTF_LOWER, // 0x6e 'n'
    _yp_CTF_LOWER, // 0x6f 'o'
    _yp_CTF_LOWER, // 0x70 'p'
    _yp_CTF_LOWER, // 0x71 'q'
    _yp_CTF_LOWER, // 0x72 'r'
    _yp_CTF_LOWER, // 0x73 's'
    _yp_CTF_LOWER, // 0x74 't'
    _yp_CTF_LOWER, // 0x75 'u'
    _yp_CTF_LOWER, // 0x76 'v'
    _yp_CTF_LOWER, // 0x77 'w'
    _yp_CTF_LOWER, // 0x78 'x'
    _yp_CTF_LOWER, // 0x79 'y'
    _yp_CTF_LOWER, // 0x7a 'z'
    0, // 0x7b '{'
    0, // 0x7c '|'
    0, // 0x7d '}'
    0, // 0x7e '~'
    0, // 0x7f '\x7f'
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// XXX Adapted from Python's pyctype.c and pyctype.h
const yp_uint8_t _yp_ctype_tolower[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

// XXX Adapted from Python's pyctype.c and pyctype.h
const yp_uint8_t _yp_ctype_toupper[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

// clang-format on
#pragma endregion ctype_like


/*************************************************************************************************
 * nohtyP memory allocations
 *************************************************************************************************/
#pragma region malloc

// The standard C malloc/realloc are inefficient in a couple of ways:
//  - realloc has the potential to copy large amounts of data between buffers, data which may be
//  partially or completely replaced after the realloc
//  - malloc/realloc may allocate more memory than requested, memory that _could_ be used if we
//  only knew how much there was
// yp_malloc, yp_malloc_resize, and yp_free are the top level functions of nohtyP's memory
// allocation scheme, designed to overcome these inefficiencies. There are several implementations
// of these APIs depending on the target platform; you can also provide your own versions via
// yp_initialize.

// Dummy memory allocation functions that always fail, to ensure yp_initialize is called first
static void *_dummy_yp_malloc(yp_ssize_t *actual, yp_ssize_t size) { return NULL; }
static void *_dummy_yp_malloc_resize(yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra)
{
    return NULL;
}
static void _dummy_yp_free(void *p) {}

// See docs for yp_initialize_parameters_t.yp_malloc in nohtyP.h
static void *(*yp_malloc)(yp_ssize_t *actual, yp_ssize_t size) = _dummy_yp_malloc;

// See docs for yp_initialize_parameters_t.yp_malloc_resize in nohtyP.h
static void *(*yp_malloc_resize)(
        yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra) = _dummy_yp_malloc_resize;

// See docs for yp_initialize_parameters_t.yp_free in nohtyP.h
static void (*yp_free)(void *p) = _dummy_yp_free;

// Microsoft gives a couple options for heaps; let's stick with the standard malloc/free plus
// _msize and _expand
#if defined(_MSC_VER)
#include <malloc.h>
// TODO Be consistent: should output pointers come first, or last, in all the functions? (What
// is our nohtyP style guide for function signatures?)
void *yp_mem_default_malloc(yp_ssize_t *actual, yp_ssize_t size)
{
    void *p;
    yp_ASSERT(size >= 0, "size cannot be negative");
    if (size < 1) size = 1;
    p = malloc((size_t)size);
    if (p == NULL) return NULL;
    *actual = (yp_ssize_t)_msize(p);
    if (*actual < 0) *actual = yp_SSIZE_T_MAX;  // we were given more memory than we can use
    yp_DEBUG("malloc: %p %" PRIssize " bytes", p, *actual);
    return p;
}
void *yp_mem_default_malloc_resize(yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra)
{
    void *newp;
    yp_ASSERT(size >= 0, "size cannot be negative");
    yp_ASSERT(extra >= 0, "extra cannot be negative");
    if (size < 1) size = 1;

    newp = _expand(p, (size_t)size);
    if (newp == NULL) {
        size = yp_USIZE_MATH(size, +, extra);
        if (size < 0) size = yp_SSIZE_T_MAX;  // addition overflowed; clamp to max
        newp = malloc((size_t)size);
        if (newp == NULL) return NULL;
    }
    *actual = (yp_ssize_t)_msize(newp);
    if (*actual < 0) *actual = yp_SSIZE_T_MAX;  // we were given more memory than we can use
    yp_DEBUG("malloc_resize: %p %" PRIssize " bytes  (was %p)", newp, *actual, p);
    return newp;
}
void yp_mem_default_free(void *p)
{
    yp_DEBUG("free: %p", p);
    free(p);
}

// TODO GCC-optimized version

// If all else fails, rely on the standard C malloc/free functions
#else
// Rounds allocations up to a multiple that should be easy for most heaps without wasting space for
// the smallest objects (ie ints); don't call with a negative size
#define _yp_DEFAULT_MALLOC_ROUNDTO (16)
static yp_ssize_t _default_yp_malloc_good_size(yp_ssize_t size)
{
    yp_ssize_t diff;
    if (size <= _yp_DEFAULT_MALLOC_ROUNDTO) return _yp_DEFAULT_MALLOC_ROUNDTO;
    diff = size % _yp_DEFAULT_MALLOC_ROUNDTO;
    if (diff > 0) size += _yp_DEFAULT_MALLOC_ROUNDTO - diff;
    return size;
}
void *yp_mem_default_malloc(yp_ssize_t *actual, yp_ssize_t size)
{
    void *p;
    yp_ASSERT(size >= 0, "size cannot be negative");
    *actual = _default_yp_malloc_good_size(size);
    yp_ASSERT1(*actual >= 0);
    p = malloc((size_t)*actual);
    yp_DEBUG("malloc: %p %" PRIssize " bytes", p, *actual);
    return p;
}
void *yp_mem_default_malloc_resize(yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra)
{
    void *newp;
    yp_ASSERT(size >= 0, "size cannot be negative");
    yp_ASSERT(extra >= 0, "extra cannot be negative");
    size = yp_USIZE_MATH(size, +, extra);
    if (size < 0) size = yp_SSIZE_T_MAX;  // addition overflowed; clamp to max
    *actual = _default_yp_malloc_good_size(size);
    yp_ASSERT1(*actual >= 0);
    newp = malloc((size_t)*actual);
    yp_DEBUG("malloc_resize: %p %" PRIssize " bytes  (was %p)", newp, *actual, p);
    return newp;
}
void yp_mem_default_free(void *p)
{
    yp_DEBUG("free: %p", p);
    free(p);
}
#endif

#pragma endregion malloc


/*************************************************************************************************
 * nohtyP object allocations
 *************************************************************************************************/
#pragma region object_malloc

// TODO Review the names in this section. They're a little wordy, and maybe inconsistent.

// This should be one of exactly two possible values, 1 (the default) or ypObject_REFCNT_IMMORTAL,
// depending on yp_initialize's everything_immortal parameter
static yp_uint32_t _ypMem_starting_refcnt = 1;
yp_STATIC_ASSERT(yp_sizeof(_ypMem_starting_refcnt) == yp_sizeof_member(ypObject, ob_refcnt),
        sizeof_starting_refcnt);

// When calculating the number of required bytes, there is protection at higher levels to ensure
// the multiplication never overflows. However, when calculating extra (or required+extra) bytes,
// if the multiplication overflows we clamp to yp_SSIZE_T_MAX.
static yp_ssize_t _ypMem_calc_extra_size(yp_ssize_t alloclen, yp_ssize_t elemsize)
{
    yp_ASSERT(alloclen >= 0, "alloclen cannot be negative");
    yp_ASSERT(elemsize > 0, "elemsize cannot be <=0");
    // If the multiplication will overflow, return the maximum size
    if (alloclen > yp_SSIZE_T_MAX / elemsize) return yp_SSIZE_T_MAX;
    return alloclen * elemsize;
}

// Returns a malloc'd buffer for fixed, non-container objects, or exception on failure
static ypObject *_ypMem_malloc_fixed(int type, yp_ssize_t sizeof_obStruct)
{
    yp_ssize_t size;
    ypObject  *ob = (ypObject *)yp_malloc(&size, sizeof_obStruct);
    if (ob == NULL) return yp_MemoryError;
    ypObject_SET_ALLOCLEN(ob, ypObject_LEN_INVALID);
    ob->ob_data = NULL;
    ypObject_SET_TYPE_CODE(ob, type);
    ob->ob_refcnt = _ypMem_starting_refcnt;
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = ypObject_LEN_INVALID;
    yp_DEBUG("MALLOC_FIXED: type %d %p", type, ob);
    return ob;
}
#define ypMem_MALLOC_FIXED(obStruct, type) _ypMem_malloc_fixed((type), yp_sizeof(obStruct))

// Returns a malloc'd buffer for a container object holding alloclen elements in-line, or exception
// on failure. The container can neither grow nor shrink after allocation. ob_inline_data in
// obStruct is used to determine the element size and ob_data; ob_len is set to zero. alloclen
// must not be negative and offsetof_inline+(alloclen*elemsize) must not overflow. ob_alloclen may
// be larger than requested, but will never be larger than alloclen_max.
static ypObject *_ypMem_malloc_container_inline(int type, yp_ssize_t alloclen,
        yp_ssize_t alloclen_max, yp_ssize_t offsetof_inline, yp_ssize_t elemsize)
{
    yp_ssize_t size;
    ypObject  *ob;

    yp_ASSERT(alloclen >= 0, "alloclen cannot be negative");
    yp_ASSERT(alloclen <= alloclen_max, "alloclen cannot be larger than maximum");
    yp_ASSERT(alloclen_max <= ypObject_LEN_MAX,
            "alloclen_max cannot be larger than ypObject_LEN_MAX");
    yp_ASSERT(alloclen <= (yp_SSIZE_T_MAX - offsetof_inline) / elemsize,
            "yp_malloc size cannot overflow");

    // Allocate memory, then update alloclen based on actual size of buffer allocated
    size = offsetof_inline + (alloclen * elemsize);
    ob = (ypObject *)yp_malloc(&size, size);
    if (ob == NULL) return yp_MemoryError;
    alloclen = (size - offsetof_inline) / elemsize;  // rounds down
    if (alloclen > alloclen_max) alloclen = alloclen_max;

    ypObject_SET_ALLOCLEN(ob, alloclen);
    ob->ob_data = ((yp_uint8_t *)ob) + offsetof_inline;
    ypObject_SET_TYPE_CODE(ob, type);
    ob->ob_refcnt = _ypMem_starting_refcnt;
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = 0;
    yp_DEBUG("MALLOC_CONTAINER_INLINE: type %d %p alloclen %" PRIssize, type, ob, alloclen);
    return ob;
}
#define ypMem_MALLOC_CONTAINER_INLINE4(obStruct, type, alloclen, alloclen_max, elemsize) \
    _ypMem_malloc_container_inline(                                                      \
            (type), (alloclen), (alloclen_max), yp_offsetof(obStruct, ob_inline_data), (elemsize))
#define ypMem_MALLOC_CONTAINER_INLINE(obStruct, type, alloclen, alloclen_max)    \
    ypMem_MALLOC_CONTAINER_INLINE4(obStruct, (type), (alloclen), (alloclen_max), \
            yp_sizeof_member(obStruct, ob_inline_data[0]))

// XXX Cannot change once objects are allocated!  Since we use this value to compute inlinelen of
// all allocated objects, changing this value will mean computing an incorrect inlinelen.
// TODO Make this configurable via yp_initialize
// TODO 64-bit PyDictObject is 128 bytes...we are larger!
// TODO Static asserts to ensure that certain-sized objects fit with one allocation, then optimize
#if defined(yp_ARCH_32_BIT)
#define _ypMem_ideal_size_DEFAULT ((yp_ssize_t)128)
#else
#define _ypMem_ideal_size_DEFAULT ((yp_ssize_t)256)
#endif
static yp_ssize_t _ypMem_ideal_size = _ypMem_ideal_size_DEFAULT;

// Returns a malloc'd buffer for a container that may grow or shrink in the future, or exception on
// failure. A fixed amount of memory is allocated in-line, as per _ypMem_ideal_size. If this fits
// required elements, it is used, otherwise a separate buffer of required+extra elements is
// allocated. required and extra must not be negative and required*elemsize must not overflow.
// ob_alloclen may be larger than requested, but will never be larger than alloclen_max.
static yp_ssize_t _ypMem_inlinelen_container_variable(
        yp_ssize_t offsetof_inline, yp_ssize_t elemsize);
static ypObject *_ypMem_malloc_container_variable(int type, yp_ssize_t required, yp_ssize_t extra,
        yp_ssize_t alloclen_max, yp_ssize_t offsetof_inline, yp_ssize_t elemsize)
{
    yp_ssize_t size;
    ypObject  *ob;
    yp_ssize_t alloclen;

    yp_ASSERT(required >= 0, "required cannot be negative");
    yp_ASSERT(extra >= 0, "extra cannot be negative");
    yp_ASSERT(required <= alloclen_max, "required cannot be larger than maximum");
    yp_ASSERT(_ypMem_inlinelen_container_variable(offsetof_inline, elemsize) <= alloclen_max,
            "inlinelen is larger than maximum?! (Is _ypMem_ideal_size set too high?)");
    yp_ASSERT(alloclen_max <= ypObject_LEN_MAX,
            "alloclen_max cannot be larger than ypObject_LEN_MAX");
    yp_ASSERT(required <= yp_SSIZE_T_MAX / elemsize, "required yp_malloc size cannot overflow");
    if (extra > alloclen_max - required) extra = alloclen_max - required;

    // Allocate object memory, update alloclen based on actual size of buffer allocated, then see
    // if the data can fit inline or if it needs a separate buffer
    ob = (ypObject *)yp_malloc(&size, MAX(offsetof_inline, _ypMem_ideal_size));
    if (ob == NULL) return yp_MemoryError;
    alloclen = (size - offsetof_inline) / elemsize;  // rounds down
    if (required <= alloclen) {  // a valid check even if alloclen>max, because required<=max
        ob->ob_data = ((yp_uint8_t *)ob) + offsetof_inline;
    } else {
        ob->ob_data = yp_malloc(&size, _ypMem_calc_extra_size(required + extra, elemsize));
        if (ob->ob_data == NULL) {
            yp_free(ob);
            return yp_MemoryError;
        }
        alloclen = size / elemsize;  // rounds down
    }
    if (alloclen > alloclen_max) alloclen = alloclen_max;

    ypObject_SET_ALLOCLEN(ob, alloclen);
    ypObject_SET_TYPE_CODE(ob, type);
    ob->ob_refcnt = _ypMem_starting_refcnt;
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = 0;
    yp_DEBUG("MALLOC_CONTAINER_VARIABLE: type %d %p alloclen %" PRIssize, type, ob, alloclen);
    return ob;
}
#define ypMem_MALLOC_CONTAINER_VARIABLE5(obStruct, type, required, extra, alloclen_max, elemsize) \
    _ypMem_malloc_container_variable((type), (required), (extra), (alloclen_max),                 \
            yp_offsetof(obStruct, ob_inline_data), (elemsize))
#define ypMem_MALLOC_CONTAINER_VARIABLE(obStruct, type, required, extra, alloclen_max)      \
    ypMem_MALLOC_CONTAINER_VARIABLE5(obStruct, (type), (required), (extra), (alloclen_max), \
            yp_sizeof_member(obStruct, ob_inline_data[0]))

// Returns the allocated length of the inline data buffer for the given object, which must have
// been allocated with ypMem_MALLOC_CONTAINER_VARIABLE.
// TODO The calculated inlinelen may be smaller than what our initial allocation actually provided
static yp_ssize_t _ypMem_inlinelen_container_variable(
        yp_ssize_t offsetof_inline, yp_ssize_t elemsize)
{
    yp_ssize_t inlinelen = (_ypMem_ideal_size - offsetof_inline) / elemsize;
    if (inlinelen < 0) return 0;
    return inlinelen;
}
#define ypMem_INLINELEN_CONTAINER_VARIABLE3(ob, obStruct, elemsize) \
    _ypMem_inlinelen_container_variable(yp_offsetof(obStruct, ob_inline_data), (elemsize))
#define ypMem_INLINELEN_CONTAINER_VARIABLE(ob, obStruct) \
    ypMem_INLINELEN_CONTAINER_VARIABLE3(                 \
            (ob), obStruct, yp_sizeof_member(obStruct, ob_inline_data[0]))

// Resizes ob_data, the variable-portion of ob, and returns the previous value of ob_data
// ("oldptr"). There are three possible scenarios:
//  - On error, returns NULL, and ob is not modified
//  - If ob_data can be resized in-place, updates ob_alloclen and returns ob_data; in this case, no
//  memcpy is necessary, as the buffer has not moved
//  - Otherwise, updates ob_alloclen and returns oldptr (which is not freed); in this case, you
//  will need to copy the data from oldptr, then free it with:
//      ypMem_REALLOC_CONTAINER_FREE_OLDPTR(ob, obStruct, oldptr)
// Required is the minimum ob_alloclen required; if required can fit inline, the inline buffer is
// used. extra is a hint as to how much the buffer should be over-allocated, which may be ignored.
// This function will always resize the data, so first check to see if a resize is necessary.
// required and extra must not be negative and required*elemsize must not overflow. ob_alloclen
// may be larger than requested, but will never be larger than alloclen_max. Does not update
// ob_len.
// XXX Unlike realloc, this *never* copies to the new buffer and *never* frees the old buffer.
static void *_ypMem_realloc_container_variable(ypObject *ob, yp_ssize_t required, yp_ssize_t extra,
        yp_ssize_t alloclen_max, yp_ssize_t offsetof_inline, yp_ssize_t elemsize)
{
    void      *newptr;
    void      *oldptr;
    yp_ssize_t size;
    yp_ssize_t extra_size;
    yp_ssize_t alloclen;
    void      *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_ssize_t inlinelen = _ypMem_inlinelen_container_variable(offsetof_inline, elemsize);

    yp_ASSERT(required >= 0, "required cannot be negative");
    yp_ASSERT(extra >= 0, "extra cannot be negative");
    yp_ASSERT(required <= alloclen_max, "required cannot be larger than maximum");
    yp_ASSERT(inlinelen <= alloclen_max,
            "inlinelen is larger than maximum?! (Is _ypMem_ideal_size set too high?)");
    yp_ASSERT(alloclen_max <= ypObject_LEN_MAX,
            "alloclen_max cannot be larger than ypObject_LEN_MAX");
    yp_ASSERT(required <= yp_SSIZE_T_MAX / elemsize, "required yp_malloc size cannot overflow");
    if (extra > alloclen_max - required) extra = alloclen_max - required;

    // If the minimum required allocation can fit inline, then prefer that over a separate buffer
    if (required <= inlinelen) {
        oldptr = ob->ob_data;  // might equal inlineptr
        ob->ob_data = inlineptr;
        ypObject_SET_ALLOCLEN(ob, inlinelen);
        yp_DEBUG("REALLOC_CONTAINER_VARIABLE (to inline): %p alloclen %" PRIssize, ob,
                ypObject_ALLOCLEN(ob));
        return oldptr;
    }

    // If the data is currently inline, it must be moved out into a separate buffer
    if (ob->ob_data == inlineptr) {
        newptr = yp_malloc(&size, _ypMem_calc_extra_size(required + extra, elemsize));
        if (newptr == NULL) return NULL;
        oldptr = ob->ob_data;  // can't possibly equal newptr
        ob->ob_data = newptr;
        alloclen = size / elemsize;  // rounds down
        if (alloclen > alloclen_max) alloclen = alloclen_max;
        ypObject_SET_ALLOCLEN(ob, alloclen);
        yp_DEBUG("REALLOC_CONTAINER_VARIABLE (from inline): %p alloclen %" PRIssize, ob, alloclen);
        return oldptr;
    }

    // Otherwise, let yp_malloc_resize determine if we can expand in-place or need to memcpy
    extra_size = _ypMem_calc_extra_size(extra, elemsize);
    newptr = yp_malloc_resize(&size, ob->ob_data, required * elemsize, extra_size);
    if (newptr == NULL) return NULL;
    oldptr = ob->ob_data;  // might equal newptr
    ob->ob_data = newptr;
    alloclen = size / elemsize;  // rounds down
    if (alloclen > alloclen_max) alloclen = alloclen_max;
    ypObject_SET_ALLOCLEN(ob, alloclen);
    yp_DEBUG("REALLOC_CONTAINER_VARIABLE (malloc_resize): %p alloclen %" PRIssize, ob, alloclen);
    return oldptr;
}
#define ypMem_REALLOC_CONTAINER_VARIABLE5(ob, obStruct, required, extra, alloclen_max, elemsize) \
    _ypMem_realloc_container_variable((ob), (required), (extra), (alloclen_max),                 \
            yp_offsetof(obStruct, ob_inline_data), (elemsize))
#define ypMem_REALLOC_CONTAINER_VARIABLE(ob, obStruct, required, extra, alloclen_max)      \
    ypMem_REALLOC_CONTAINER_VARIABLE5((ob), obStruct, (required), (extra), (alloclen_max), \
            yp_sizeof_member(obStruct, ob_inline_data[0]))

// Allocates a new buffer for ob_data, the variable-portion of ob, and returns the previous value of
// ob_data ("oldptr"). On error, returns NULL, and ob is not modified. Once you have copied the data
// from oldptr, free it with:
//
//      ypMem_REALLOC_CONTAINER_FREE_OLDPTR(ob, obStruct, oldptr)
//
// Required is the minimum ob_alloclen required; if required can fit inline, and ob_data is not
// already the inline buffer, then the inline buffer is used, otherwise a separate buffer of
// required+extra elements is allocated. required and extra must not be negative and
// required*elemsize must not overflow. ob_alloclen may be larger than requested, but will never be
// larger than alloclen_max. Does not update ob_len.
//
// XXX Unlike realloc, this *never* copies to the new buffer and *never* frees the old buffer.
static void *_ypMem_realloc_container_variable_new(ypObject *ob, yp_ssize_t required,
        yp_ssize_t extra, yp_ssize_t alloclen_max, yp_ssize_t offsetof_inline, yp_ssize_t elemsize)
{
    void      *newptr;
    void      *oldptr;
    yp_ssize_t size;
    yp_ssize_t alloclen;
    void      *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_ssize_t inlinelen = _ypMem_inlinelen_container_variable(offsetof_inline, elemsize);

    yp_ASSERT(required >= 0, "required cannot be negative");
    yp_ASSERT(extra >= 0, "extra cannot be negative");
    yp_ASSERT(required <= alloclen_max, "required cannot be larger than maximum");
    yp_ASSERT(inlinelen <= alloclen_max,
            "inlinelen is larger than maximum?! (Is _ypMem_ideal_size set too high?)");
    yp_ASSERT(alloclen_max <= ypObject_LEN_MAX,
            "alloclen_max cannot be larger than ypObject_LEN_MAX");
    yp_ASSERT(required <= yp_SSIZE_T_MAX / elemsize, "required yp_malloc size cannot overflow");
    if (extra > alloclen_max - required) extra = alloclen_max - required;

    // If the minimum required allocation can fit inline, and we are not already inline, then prefer
    // that over a separate buffer.
    if (required <= inlinelen && ob->ob_data != inlineptr) {
        oldptr = ob->ob_data;
        ob->ob_data = inlineptr;
        ypObject_SET_ALLOCLEN(ob, inlinelen);
        yp_DEBUG("REALLOC_CONTAINER_VARIABLE_NEW (to inline): %p alloclen %" PRIssize, ob,
                ypObject_ALLOCLEN(ob));
        return oldptr;
    }

    // Otherwise, allocate a separate buffer.
    newptr = yp_malloc(&size, _ypMem_calc_extra_size(required + extra, elemsize));
    if (newptr == NULL) return NULL;
    oldptr = ob->ob_data;  // can't possibly equal newptr
    ob->ob_data = newptr;
    alloclen = size / elemsize;  // rounds down
    if (alloclen > alloclen_max) alloclen = alloclen_max;
    ypObject_SET_ALLOCLEN(ob, alloclen);
    yp_DEBUG("REALLOC_CONTAINER_VARIABLE_NEW (malloc): %p alloclen %" PRIssize, ob, alloclen);
    return oldptr;
}
#define ypMem_REALLOC_CONTAINER_VARIABLE_NEW5(                                       \
        ob, obStruct, required, extra, alloclen_max, elemsize)                       \
    _ypMem_realloc_container_variable_new((ob), (required), (extra), (alloclen_max), \
            yp_offsetof(obStruct, ob_inline_data), (elemsize))
#define ypMem_REALLOC_CONTAINER_VARIABLE_NEW(ob, obStruct, required, extra, alloclen_max)      \
    ypMem_REALLOC_CONTAINER_VARIABLE_NEW5((ob), obStruct, (required), (extra), (alloclen_max), \
            yp_sizeof_member(obStruct, ob_inline_data[0]))

// Called after a successful ypMem_REALLOC_CONTAINER_VARIABLE* to free the previous value of ob_data
// ("oldptr"). If oldptr is the inline buffer, this is a no-op. Always succeeds.
// XXX Do not call if ob_data was resized in-place (i.e. if ob_data==oldptr)
static void _ypMem_realloc_container_free_oldptr(
        ypObject *ob, void *oldptr, yp_ssize_t offsetof_inline)
{
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_DEBUG("REALLOC_CONTAINER_FREE_OLDPTR: %p", ob);
    yp_ASSERT(oldptr != ob->ob_data, "handle the resized-in-place case separately");
    if (oldptr != inlineptr) yp_free(oldptr);
}
#define ypMem_REALLOC_CONTAINER_FREE_OLDPTR(ob, obStruct, oldptr) \
    _ypMem_realloc_container_free_oldptr((ob), oldptr, yp_offsetof(obStruct, ob_inline_data))

// Resets ob_data to the inline buffer and frees the separate buffer (if there is one). Any
// contained objects must have already been discarded; no memory is copied. Always succeeds.
// TODO Is it actually a good idea to free the buffer on clear? If the object grows again, we will
// need a new buffer, and isn't it a fair assumption that a reused object will be about the same
// size every time? We should have consistency and guidance in the documentation.
static void _ypMem_realloc_container_variable_clear(
        ypObject *ob, yp_ssize_t alloclen_max, yp_ssize_t offsetof_inline, yp_ssize_t elemsize)
{
    void      *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_ssize_t inlinelen = _ypMem_inlinelen_container_variable(offsetof_inline, elemsize);

    yp_ASSERT(inlinelen <= alloclen_max,
            "inlinelen is larger than maximum?! (Is _ypMem_ideal_size set too high?)");

    // Free any separately-allocated buffer
    if (ob->ob_data != inlineptr) {
        yp_free(ob->ob_data);
        ob->ob_data = inlineptr;
        ypObject_SET_ALLOCLEN(ob, inlinelen);
    }
    yp_DEBUG("REALLOC_CONTAINER_VARIABLE_CLEAR: %p alloclen %" PRIssize, ob, ypObject_ALLOCLEN(ob));
}
#define ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR3(ob, obStruct, alloclen_max, elemsize) \
    _ypMem_realloc_container_variable_clear(                                          \
            (ob), (alloclen_max), yp_offsetof(obStruct, ob_inline_data), (elemsize))
#define ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR(ob, obStruct, alloclen_max) \
    ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR3(                               \
            (ob), obStruct, (alloclen_max), yp_sizeof_member(obStruct, ob_inline_data[0]))

// Frees an object allocated with ypMem_MALLOC_FIXED
#define ypMem_FREE_FIXED yp_free

// Frees an object allocated with either ypMem_MALLOC_CONTAINER_* macro
static void _ypMem_free_container(ypObject *ob, yp_ssize_t offsetof_inline)
{
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_DEBUG("FREE_CONTAINER: %p", ob);
    if (ob->ob_data != inlineptr) yp_free(ob->ob_data);
    yp_free(ob);
}
#define ypMem_FREE_CONTAINER(ob, obStruct) \
    _ypMem_free_container(ob, yp_offsetof(obStruct, ob_inline_data))

#pragma endregion object_malloc


/*************************************************************************************************
 * Object Reference Counting
 *************************************************************************************************/
#pragma region references
// FIXME Review this section again

// TODO What if these (and yp_isexceptionC) were macros in the header so they were force-inlined
// by users of the API...even if through a DLL?
ypObject *yp_incref(ypObject *x)
{
    if (ypObject_REFCNT(x) >= ypObject_REFCNT_IMMORTAL) return x;  // no-op
    ypObject_REFCNT(x) += 1;
    yp_DEBUG("incref: type %d %p refcnt %d", ypObject_TYPE_CODE(x), x, ypObject_REFCNT(x));
    return x;
}

void yp_increfN(int n, ...)
{
    va_list args;
    va_start(args, n);
    for (/*n already set*/; n > 0; n--) yp_incref(va_arg(args, ypObject *));
    va_end(args);
}

// FIXME This ypObject_dealloclist support should be available externally as part of nohtyP.h,
// so that custom objects can also delay deallocation until the end of their methods. Will need to
// think hard about the names these should be given, and which parts of the API should be opaque.
typedef struct {
    ypObject *head;
    ypObject *tail;
} ypObject_dealloclist;

#define ypObject_DEALLOCLIST_INIT() \
    {                               \
        NULL, NULL                  \
    }

static void _ypObject_dealloclist_push(ypObject_dealloclist *list, ypObject *x)
{
    yp_ASSERT1(list != NULL);
    yp_ASSERT1(ypObject_REFCNT(x) == 1);

    // We abuse ob_hash to form a linked list of objects to deallocate. This is safe since we
    // alone hold the last remaining reference, and as such no other code should access this field.
    ypObject_CACHED_HASH(x) = (yp_hash_t)NULL;
    if (list->head == NULL) {
        yp_ASSERT1(list->tail == NULL);
        list->head = x;
        list->tail = x;
    } else {
        ypObject_CACHED_HASH(list->tail) = (yp_hash_t)x;
        list->tail = x;
    }
}

static ypObject *_ypObject_dealloclist_pop(ypObject_dealloclist *list)
{
    ypObject *x;

    yp_ASSERT1(list != NULL);

    x = list->head;
    if (x == NULL) {
        yp_ASSERT1(list->tail == NULL);
        return NULL;
    }

    // We abuse ob_hash to form a linked list of objects to deallocate. Because deallocating x
    // will run arbitrary code that may depend on ob_hash, we invalidate ob_hash before returning.
    if (x == list->tail) {
        list->head = NULL;
        list->tail = NULL;
    } else {
        list->head = (ypObject *)ypObject_CACHED_HASH(x);
    }
    ypObject_CACHED_HASH(x) = ypObject_HASH_INVALID;
    yp_ASSERT1(ypObject_REFCNT(x) == 1);
    return x;
}

static ypObject *_ypObject_dealloc(ypObject *x, ypObject_dealloclist *list)
{
    ypObject *result = yp_None;

    yp_ASSERT1(x != NULL);
    yp_ASSERT1(list != NULL);

    while (x != NULL) {
        ypObject *subresult;
        yp_DEBUG("decref (dealloc): type %d %p", ypObject_TYPE_CODE(x), x);
        yp_ASSERT1(ypObject_REFCNT(x) == 1);
        subresult = ypObject_TYPE(x)->tp_dealloc(x, list);

        yp_ASSERT(subresult == yp_None || yp_isexceptionC(subresult),
                "tp_dealloc must return yp_None or an exception");
        if (yp_isexceptionC(subresult)) result = subresult;

        x = _ypObject_dealloclist_pop(list);
    }

    return result;
}

// If x's refcount is >1, discards a reference and returns false; otherwise, returns true and keeps
// x's refcount at 1 (it's up to the caller to deallocate x).
static int _ypObject_decref(ypObject *x, ypObject_dealloclist *list)
{
    if (ypObject_REFCNT(x) >= ypObject_REFCNT_IMMORTAL) return FALSE;  // no-op

    if (ypObject_REFCNT(x) > 1) {
        ypObject_REFCNT(x) -= 1;
        yp_DEBUG("decref: type %d %p refcnt %d", ypObject_TYPE_CODE(x), x, ypObject_REFCNT(x));
        return FALSE;
    }

    yp_ASSERT1(ypObject_REFCNT(x) == 1);
    return TRUE;
}

// FIXME Is there a better name to give this?
static void yp_decref_fromdealloc(ypObject *x, void *memo)
{
    int deallocate = _ypObject_decref(x, NULL);
    if (deallocate) {
        _ypObject_dealloclist_push(memo, x);
    }
}

// TODO A version of decref that returns exceptions. Objects that could fail deallocation need to
// ensure they are in a consistent state when returning...perhaps the deallocation can be attempted
// again and it will skip over the part that failed?  Still need yp_decref to squelch errors, but
// an ASSERT on error is not enough: in testing, an exception would have to be raised in a debug
// build for anybody to notice. Instead, certain types (i.e. file) would need to require that
// the exception-returning version is used. If tp_dealloc took a ypObject**exc that yp_decref set
// to NULL but yp_decrefE didn't, then file could ASSERT on the NULL and the user would know to use
// the exception-returning version.
void yp_decref(ypObject *x)
{
    int deallocate = _ypObject_decref(x, NULL);
    if (deallocate) {
        ypObject_dealloclist dealloclist = ypObject_DEALLOCLIST_INIT();
        ypObject *yp_UNUSED  result = _ypObject_dealloc(x, &dealloclist);
        yp_ASSERT(!yp_isexceptionC(result), "tp_dealloc returned exception %p", result);
    }
}

void yp_decrefN(int n, ...)
{
    va_list args;
    va_start(args, n);
    for (/*n already set*/; n > 0; n--) yp_decref(va_arg(args, ypObject *));
    va_end(args);
}

#pragma endregion references


/*************************************************************************************************
 * ypQuickIter: iterator-like abstraction over va_lists of ypObject*s, and iterables
 * XXX Internal use only!
 *************************************************************************************************/
#pragma region ypQuickIter

// TODO Inspect all uses of va_arg, yp_miniiter_next, and yp_next below and see if we can
// consolidate or simplify some code by using this API

// This API exists to reduce duplication of code where variants of a method accept va_lists of
// ypObject*s, or a general-purpose iterable. This code intends to be a light-weight abstraction
// over all of these, but particularly efficient for va_lists. In particular, it doesn't require
// a temporary tuple to be allocated.
//
// Be very careful how you use this API, as only the bare-minimum safety checks are implemented.

typedef union {
    struct {           // State for ypQuickIter_var_*
        int     n;     // Number of variable arguments remaining
        va_list args;  // Current state of variable arguments ("owned")
    } var;
    struct {                     // State for ypQuickIter_array_*
        yp_ssize_t       i;      // Index of next value to yield
        yp_ssize_t       n;      // Number of objects in array
        ypObject *const *array;  // The tuple or list object being iterated over ("borrowed")
    } array;
    struct {             // State for ypQuickIter_tuple_*
        yp_ssize_t i;    // Index of next value to yield
        ypObject  *obj;  // The tuple or list object being iterated over (borrowed)
    } tuple;
    struct {                    // State for ypQuickIter_mi_*
        yp_uint64_t state;      // Mini iterator state
        ypObject   *iter;       // Mini iterator object (owned)
        ypObject   *to_decref;  // Held on behalf of nextX (owned, discarded by nextX/close)
        yp_ssize_t  len;        // >=0 if length_hint is exact, or use yp_miniiter_length_hint
    } mi;
} ypQuickIter_state;

// The various ypQuickIter_new_* calls either correspond with, or return a pointer to, one of
// these method tables, which is used to manipulate the associated ypQuickIter_state.
typedef struct {
    // Returns a *borrowed* reference to the next yielded value. If the iterator is exhausted,
    // returns NULL. The reference becomes invalid when a new value is yielded or close is called.
    ypObject *(*nextX)(ypQuickIter_state *state);

    // Similar to nextX, but returns a new reference (that will remain valid until decref'ed).
    ypObject *(*next)(ypQuickIter_state *state);

    // Returns a new reference to a tuple containing the remaining values (i.e. for *args), or an
    // empty tuple if the iterator is exhausted. Exhausts the iterator, even on error.
    ypObject *(*remaining_as_tuple)(ypQuickIter_state *state);

    // Returns the number of items left to be yielded. Sets *isexact to true if this is an exact
    // value, or false if this is an estimate. On error, sets *exc, returns zero, and *isexact is
    // undefined.
    // TODO Do like Python, and instead of *isexact accept a default hint that is returned?
    // There's also the idea of a ypObject_MIN_LENHINT...
    // FIXME Instead, return an error, take a pointer to the hint.
    yp_ssize_t (*length_hint)(ypQuickIter_state *state, int *isexact, ypObject **exc);

    // Closes the ypQuickIter. Any further operations on state will be undefined.
    // TODO If any of these close methods raise errors, we'll need to return them
    void (*close)(ypQuickIter_state *state);
} ypQuickIter_methods;


static ypObject *ypQuickIter_var_nextX(ypQuickIter_state *state)
{
    yp_ASSERT(state->var.n >= 0, "state->var.n should not be negative");
    if (state->var.n <= 0) return NULL;
    state->var.n -= 1;
    return va_arg(state->var.args, ypObject *);  // borrowed
}

static ypObject *ypQuickIter_var_next(ypQuickIter_state *state)
{
    ypObject *x = ypQuickIter_var_nextX(state);
    return x == NULL ? NULL : yp_incref(x);
}

static ypObject *ypQuickIter_var_remaining_as_tuple(ypQuickIter_state *state)
{
    ypObject *result = yp_tupleNV(state->var.n, state->var.args);
    state->var.n = 0;  // yes, even on error: we don't know how much yp_tupleNV consumed
    return result;
}

static yp_ssize_t ypQuickIter_var_length_hint(
        ypQuickIter_state *state, int *isexact, ypObject **exc)
{
    yp_ASSERT(state->var.n >= 0, "state->var.n should not be negative");
    *isexact = TRUE;
    return state->var.n;
}

static void ypQuickIter_var_close(ypQuickIter_state *state) { va_end(state->var.args); }

static const ypQuickIter_methods ypQuickIter_var_methods = {
        ypQuickIter_var_nextX,               // nextX
        ypQuickIter_var_next,                // next
        ypQuickIter_var_remaining_as_tuple,  // remaining_as_tuple
        ypQuickIter_var_length_hint,         // length_hint
        ypQuickIter_var_close                // close
};

// Initializes state with the given va_list containing n ypObject*s. Always succeeds. Use
// ypQuickIter_var_methods as the method table. The QuickIter is only valid so long as args is.
static void ypQuickIter_new_fromvar(ypQuickIter_state *state, int n, va_list args)
{
    yp_ASSERT1(n >= 0);
    state->var.n = n;
    va_copy(state->var.args, args);  // FIXME What if we didn't va_copy/va_end?
}


static ypObject *ypQuickIter_array_nextX(ypQuickIter_state *state)
{
    ypObject *x;
    if (state->array.i >= state->array.n) return NULL;
    x = state->array.array[state->array.i];  // borrowed
    state->array.i += 1;
    return x;
}

static ypObject *ypQuickIter_array_next(ypQuickIter_state *state)
{
    ypObject *x = ypQuickIter_array_nextX(state);
    return x == NULL ? NULL : yp_incref(x);
}

static ypObject *yp_tuple_fromarray(yp_ssize_t n, ypObject *const *array);
static ypObject *ypQuickIter_array_remaining_as_tuple(ypQuickIter_state *state)
{
    ypObject *result;

    yp_ASSERT(state->array.i >= 0 && state->array.i <= state->array.n,
            "state->array.i should be in range(n+1)");
    result = yp_tuple_fromarray(
            state->array.n - state->array.i, state->array.array + state->array.i);
    state->array.i = state->array.n;  // yes, even on error
    return result;
}

static yp_ssize_t ypQuickIter_array_length_hint(
        ypQuickIter_state *state, int *isexact, ypObject **exc)
{
    yp_ASSERT(state->array.i >= 0 && state->array.i <= state->array.n,
            "state->array.i should be in range(n+1)");
    *isexact = TRUE;
    return state->array.n - state->array.i;
}

static void ypQuickIter_array_close(ypQuickIter_state *state) {}

static const ypQuickIter_methods ypQuickIter_array_methods = {
        ypQuickIter_array_nextX,               // nextX
        ypQuickIter_array_next,                // next
        ypQuickIter_array_remaining_as_tuple,  // remaining_as_tuple
        ypQuickIter_array_length_hint,         // length_hint
        ypQuickIter_array_close                // close
};

// Initializes state with the given array containing n ypObject*s. Always succeeds. Use
// ypQuickIter_array_methods as the method table. The QuickIter is only valid so long as array is.
static void ypQuickIter_new_fromarray(
        ypQuickIter_state *state, yp_ssize_t n, ypObject *const *array)
{
    yp_ASSERT1(n >= 0);
    state->array.i = 0;
    state->array.n = n;
    state->array.array = array;
}


// TODO A bytes object can return immortal ints in range(256)


// These are implemented in the tuple section
static const ypQuickIter_methods ypQuickIter_tuple_methods;
static void ypQuickIter_new_fromtuple(ypQuickIter_state *state, ypObject *tuple);


// TODO Like tuples, sets and dicts can return borrowed references


static ypObject *ypQuickIter_mi_next(ypQuickIter_state *state)
{
    // TODO What if we were to store tp_miniiter_next? (What if miniiter did?)
    ypObject *x = yp_miniiter_next(state->mi.iter, &(state->mi.state));  // new ref
    if (yp_isexceptionC2(x, yp_StopIteration)) return NULL;
    if (state->mi.len >= 0) state->mi.len -= 1;
    return x;
}

static ypObject *ypQuickIter_mi_nextX(ypQuickIter_state *state)
{
    // This function yields borrowed references; since they are not discarded by the caller, we
    // need to retain these references ourselves and discard them when done.
    ypObject *x = ypQuickIter_mi_next(state);
    if (x == NULL) return NULL;
    yp_decref(state->mi.to_decref);
    state->mi.to_decref = x;
    return x;
}

static ypObject *yp_tuple_fromminiiter(ypObject *mi, yp_uint64_t *mi_state);
static ypObject *ypQuickIter_mi_remaining_as_tuple(ypQuickIter_state *state)
{
    return yp_tuple_fromminiiter(state->mi.iter, &(state->mi.state));  // new ref
}

static yp_ssize_t ypQuickIter_mi_length_hint(ypQuickIter_state *state, int *isexact, ypObject **exc)
{
    if (state->mi.len >= 0) {
        *isexact = TRUE;
        return state->mi.len;
    } else {
        *isexact = FALSE;
        return yp_miniiter_length_hintC(state->mi.iter, &(state->mi.state), exc);
    }
}

static void ypQuickIter_mi_close(ypQuickIter_state *state)
{
    yp_decref(state->mi.to_decref);
    yp_decref(state->mi.iter);
}

static const ypQuickIter_methods ypQuickIter_mi_methods = {
        ypQuickIter_mi_nextX,               // nextX
        ypQuickIter_mi_next,                // next
        ypQuickIter_mi_remaining_as_tuple,  // remaining_as_tuple
        ypQuickIter_mi_length_hint,         // length_hint
        ypQuickIter_mi_close                // close
};


// Initializes state to iterate over the given iterable, sets *methods to the proper method
// table to use, and returns yp_None. On error, returns an exception, and *methods and state are
// undefined. iterable is borrowed by state and must not be freed until methods->close is called.
static ypObject *ypQuickIter_new_fromiterable(
        const ypQuickIter_methods **methods, ypQuickIter_state *state, ypObject *iterable)
{
    if (ypObject_TYPE_PAIR_CODE(iterable) == ypTuple_CODE) {
        // We special-case tuples because we can return borrowed references directly
        *methods = &ypQuickIter_tuple_methods;
        ypQuickIter_new_fromtuple(state, iterable);
        return yp_None;

    } else {
        // We may eventually special-case other types, but for now treat them as generic iterables
        ypObject *exc = yp_None;
        ypObject *mi = yp_miniiter(iterable, &(state->mi.state));
        if (yp_isexceptionC(mi)) return mi;
        *methods = &ypQuickIter_mi_methods;
        state->mi.iter = mi;
        state->mi.to_decref = yp_None;
        state->mi.len = yp_lenC(iterable, &exc);
        if (yp_isexceptionC(exc)) state->mi.len = -1;  // indicates yp_miniiter_length_hintC
        return yp_None;
    }
}

#pragma endregion ypQuickIter


/*************************************************************************************************
 * ypQuickSeq: sequence-like abstraction over va_lists of ypObject*s, and iterables
 * XXX Internal use only!
 *************************************************************************************************/
#pragma region ypQuickSeq

// This API exists to reduce duplication of code where variants of a method accept va_lists of
// ypObject*s, or a general-purpose sequence. This code intends to be a light-weight abstraction
// over all of these, but particularly efficient for va_lists accessed by increasing index. That
// last point is important: using a lower index than previously may incur a performance hit, as
// the va_list will need to be va_end'ed, then va_copy'ed from the original, to get back to
// index 0. If you need random access to the va_list, consider first converting to a tuple.
//
// Additionally, this API can be used in places where an iterable would normally be converted into
// a tuple. Similar to va_lists, set and dict can "index" their elements in increasing order, but
// will incur a performance hit if acccessed out-of-order.
//
// Be very careful how you use this API, as only the bare-minimum safety checks are implemented.

typedef union {
    struct {                   // State for ypQuickSeq_var_*
        yp_ssize_t len;        // Number of ypObject*s in args/orig_args
        yp_ssize_t i;          // Current index
        ypObject  *x;          // Object at index i (borrowed) (invalid if len<1)
        va_list    args;       // Current state of variable arguments ("owned")
        va_list    orig_args;  // The starting point for args ("owned")
    } var;
    ypObject *obj;  // State for ypQuickSeq_tuple_*, etc (borrowed)
    // TODO bytes, str, other seq objs could all have their own state here
    struct {                  // State for ypQuickSeq_seq_*
        ypObject *obj;        // Sequence object (borrowed)
        ypObject *to_decref;  // Held for getindexX (owned, discarded by getindexX/close)
    } seq;
    // TODO sets and dicts can be "indexed" here, in contexts where they would be convereted to a
    // tuple anyway
} ypQuickSeq_state;

// The various ypQuickSeq_new_* calls either correspond with, or return a pointer to, one of
// these method tables, which is used to manipulate the associated ypQuickSeq_state.
typedef struct {
    // Returns a *borrowed* reference to the object at index i, or an exception. If the index is
    // out-of-range, returns NULL; negative indices are not allowed. The borrowed reference
    // becomes invalid when a new value is retrieved or close is called.
    ypObject *(*getindexX)(ypQuickSeq_state *state, yp_ssize_t i);
    // Similar to getindexX, but returns a new reference (that will remain valid until decref'ed).
    ypObject *(*getindex)(ypQuickSeq_state *state, yp_ssize_t i);
    // Returns the total number of elements in the ypQuickSeq. On error, sets *exc and returns
    // zero.
    yp_ssize_t (*len)(ypQuickSeq_state *state, ypObject **exc);
    // Closes the ypQuickSeq. Any further operations on state will be undefined.
    // TODO If any of these close methods raise errors, we'll need to return them
    void (*close)(ypQuickSeq_state *state);
} ypQuickSeq_methods;


static ypObject *ypQuickSeq_var_getindexX(ypQuickSeq_state *state, yp_ssize_t i)
{
    yp_ASSERT(i >= 0, "negative indices not allowed in ypQuickSeq");
    if (i >= state->var.len) return NULL;

    // To go backwards, we have to restart state->var.args. Note that if state->var.len is zero,
    // then we've already returned NULL before reaching this code.
    if (i < state->var.i) {
        va_end(state->var.args);
        va_copy(state->var.args, state->var.orig_args);
        state->var.i = 0;
        state->var.x = va_arg(state->var.args, ypObject *);
    }

    // Advance to the requested index (if we aren't there already) and return it
    for (/*state->var.i already set*/; state->var.i < i; state->var.i += 1) {
        state->var.x = va_arg(state->var.args, ypObject *);
    }
    return state->var.x;
}

static ypObject *ypQuickSeq_var_getindex(ypQuickSeq_state *state, yp_ssize_t i)
{
    ypObject *x = ypQuickSeq_var_getindexX(state, i);
    return x == NULL ? NULL : yp_incref(x);
}

static yp_ssize_t ypQuickSeq_var_len(ypQuickSeq_state *state, ypObject **exc)
{
    return state->var.len;
}

static void ypQuickSeq_var_close(ypQuickSeq_state *state)
{
    va_end(state->var.args);
    va_end(state->var.orig_args);
}

static const ypQuickSeq_methods ypQuickSeq_var_methods = {
        ypQuickSeq_var_getindexX,  // getindexX
        ypQuickSeq_var_getindex,   // getindex
        ypQuickSeq_var_len,        // len
        ypQuickSeq_var_close       // close
};

// Initializes state with the given va_list containing n ypObject*s. Always succeeds. Use
// ypQuickSeq_var_methods as the method table. The QuickSeq is only valid so long as args is.
static void ypQuickSeq_new_fromvar(ypQuickSeq_state *state, int n, va_list args)
{
    state->var.len = MAX(n, 0);
    va_copy(state->var.orig_args, args);  // "owned"
    va_copy(state->var.args, args);       // "owned"
    state->var.i = 0;
    // state->var.x is invalid if n<1
    if (n > 0) {
        state->var.x = va_arg(state->var.args, ypObject *);
    }
}


// TODO A bytes object can return immortal ints in range(256)


static void                     ypQuickSeq_tuple_close(ypQuickSeq_state *state);
static const ypQuickSeq_methods ypQuickSeq_tuple_methods;
static void                     ypQuickSeq_new_fromtuple(ypQuickSeq_state *state, ypObject *tuple);


static ypObject *ypQuickSeq_seq_getindex(ypQuickSeq_state *state, yp_ssize_t i)
{
    ypObject *x;
    yp_ASSERT(i >= 0, "negative indices not allowed in ypQuickSeq");
    x = yp_getindexC(state->seq.obj, i);  // new ref
    if (yp_isexceptionC2(x, yp_IndexError)) return NULL;
    return x;
}

static ypObject *ypQuickSeq_seq_getindexX(ypQuickSeq_state *state, yp_ssize_t i)
{
    // This function returns borrowed references; since they are not discarded by the caller, we
    // need to retain these references ourselves and discard them when done.
    ypObject *x = ypQuickSeq_seq_getindex(state, i);
    if (x == NULL) return NULL;
    yp_decref(state->seq.to_decref);
    state->seq.to_decref = x;
    return x;
}

static yp_ssize_t ypQuickSeq_seq_len(ypQuickSeq_state *state, ypObject **exc)
{
    return yp_lenC(state->seq.obj, exc);  // returns zero on error
}

static void ypQuickSeq_seq_close(ypQuickSeq_state *state) { yp_decref(state->seq.to_decref); }

static const ypQuickSeq_methods ypQuickSeq_seq_methods = {
        ypQuickSeq_seq_getindexX,  // getindexX
        ypQuickSeq_seq_getindex,   // getindex
        ypQuickSeq_seq_len,        // len
        ypQuickSeq_seq_close       // close
};


// Returns true if sequence is a supported built-in sequence object, in which case *methods and
// state are initialized. Cannot fail, but if sequence is not supported this returns false.
// XXX The "built-in" distinction is important because we know that getindex will behave sanely
static int ypQuickSeq_new_fromsequence_builtins(
        const ypQuickSeq_methods **methods, ypQuickSeq_state *state, ypObject *sequence)
{
    if (ypObject_TYPE_PAIR_CODE(sequence) == ypTuple_CODE) {
        *methods = &ypQuickSeq_tuple_methods;
        ypQuickSeq_new_fromtuple(state, sequence);
        return TRUE;
    }
    // TODO support for the other built-in sequences
    return FALSE;
}

// Initializes state with the given sequence, sets *methods to the proper method table to use, and
// returns yp_None. On error, returns an exception, and *methods and state are undefined.
// sequence is borrowed by state and must not be freed until methods->close is called.
static ypObject *ypQuickSeq_new_fromsequence(
        const ypQuickSeq_methods **methods, ypQuickSeq_state *state, ypObject *sequence)
{
    if (ypQuickSeq_new_fromsequence_builtins(methods, state, sequence)) {
        return yp_None;

    } else {
        // We may eventually special-case other types, but for now treat them as generic sequences.
        // All sequences should raise yp_IndexError for yp_SSIZE_T_MAX; all other types should
        // raise yp_TypeError or somesuch.
        ypObject *result = yp_getindexC(sequence, yp_SSIZE_T_MAX);
        if (!yp_isexceptionC2(result, yp_IndexError)) {
            if (yp_isexceptionC(result)) return result;
            yp_decref(result);  // discard unexpectedly-returned value
            return yp_RuntimeError;
        }
        *methods = &ypQuickSeq_seq_methods;
        state->seq.obj = sequence;
        state->seq.to_decref = yp_None;
        return yp_None;
    }
}


// Returns true if iterable is a supported built-in iterable object, in which case *methods and
// state are initialized. Cannot fail, but if iterable is not supported this returns false.
// XXX The "built-in" distinction is important because we know that getindex will behave sanely
static int ypQuickSeq_new_fromiterable_builtins(
        const ypQuickSeq_methods **methods, ypQuickSeq_state *state, ypObject *iterable)
{
    return ypQuickSeq_new_fromsequence_builtins(methods, state, iterable);
    // TODO Like tuples, sets and dicts can return borrowed references and can be supported here
}

#pragma endregion ypQuickSeq


/*************************************************************************************************
 * Dynamic Object State (i.e. yp_state_decl_t)
 *************************************************************************************************/
#pragma region state
// FIXME rename most things here

// Modifies *state and *objlocs to loop through all objects in state.
static ypObject **_ypState_nextobj(ypObject ***state, yp_uint32_t *objlocs)
{
    while (*objlocs) {  // while there are still more objects to be found...
        ypObject **next = (*objlocs) & 0x1u ? *state : NULL;
        *state += 1;
        *objlocs >>= 1;
        if (next != NULL) return next;
    }
    return NULL;
}

// Returns the immortal yp_None, or an exception if visitor returns an exception.
static ypObject *_ypState_traverse(void *state, yp_uint32_t objlocs, visitfunc visitor, void *memo)
{
    while (1) {
        ypObject  *result;
        ypObject **next = _ypState_nextobj((ypObject ***)&state, &objlocs);
        if (next == NULL) return yp_None;

        result = visitor(*next, memo);
        if (yp_isexceptionC(result)) return result;
        // FIXME Should we be discarding result here and elsewhere we call a visitfunc?  visitor
        // is not something we expose currently, but when we do we might get arbitrary values.
    }
}

// objlocs: bit n is 1 if (n*yp_sizeof(ypObject *)) is the offset of an object in state. On error,
// *size and *objlocs are undefined.
static ypObject *_ypState_fromdecl(
        yp_ssize_t *size, yp_uint32_t *objlocs, yp_state_decl_t *state_decl)
{
    yp_ssize_t i;
    yp_ssize_t objoffset;
    yp_ssize_t objloc_index;

    // A NULL declaration means there is no state.
    if (state_decl == NULL) {
        *size = 0;
        *objlocs = 0;
        return yp_None;
    }

    if (state_decl->size < 0) return yp_ValueError;
    *size = state_decl->size;

    // When offsets_len is -1, we interpret state as an array of ypObject*. Fail on all other
    // negative values, so we have space to make other flags in the future.
    if (state_decl->offsets_len == -1) {
        return yp_NotImplementedError;  // FIXME Implement.
    }
    if (state_decl->offsets_len < 0) return yp_ValueError;

    // Determine the location of the objects. There are a few errors the user could make:
    //  - an offset for a ypObject* that is at least partially outside of state
    //  - an unaligned ypObject*, which isn't currently allowed and should never happen
    //  - a larger offset than we can represent with objlocs: a current limitation of nohtyP
    *objlocs = 0x0u;
    for (i = 0; i < state_decl->offsets_len; i++) {
        objoffset = state_decl->offsets[i];
        if (objoffset < 0) return yp_ValueError;
        if (objoffset > state_decl->size - yp_sizeof(ypObject *)) return yp_ValueError;
        if (objoffset % yp_sizeof(ypObject *) != 0) return yp_SystemLimitationError;
        objloc_index = objoffset / yp_sizeof(ypObject *);
        if (objloc_index > 31) return yp_SystemLimitationError;
        *objlocs |= (0x1u << objloc_index);
    }
    return yp_None;
}

static void _ypState_copy(void *dest, void *src, yp_ssize_t size, yp_uint32_t objlocs)
{
    ypObject **objp;

    // A NULL state initializes all objects to yp_None, all other pointers to NULL, and all other
    // values to zero.
    if (src == NULL) {
        yp_memset(dest, 0, size);
        while (1) {
            objp = _ypState_nextobj((ypObject ***)&dest, &objlocs);
            if (objp == NULL) return;
            *objp = yp_None;
        }

    } else {
        yp_memcpy(dest, src, size);
        while (1) {
            objp = _ypState_nextobj((ypObject ***)&dest, &objlocs);
            if (objp == NULL) return;
            // NULL object pointers are initialized to yp_None.
            if (*objp == NULL) {
                *objp = yp_None;
            } else {
                yp_incref(*objp);
            }
        }
    }
}

#pragma endregion state


/*************************************************************************************************
 * Iterators
 *************************************************************************************************/
#pragma region iter

#define ypIter_STATE(i) (((ypObject *)i)->ob_data)
#define ypIter_STATE_SIZE ypObject_ALLOCLEN
#define ypIter_SET_STATE_SIZE ypObject_SET_ALLOCLEN
#define ypIter_LENHINT(i) (((ypIterObject *)i)->ob_length_hint)
// ob_objlocs: bit n is 1 if (n*yp_sizeof(ypObject *)) is the offset of an object in state
#define ypIter_OBJLOCS(i) (((ypIterObject *)i)->ob_objlocs)
#define ypIter_FUNC(i) (((ypIterObject *)i)->ob_func)

// The maximum possible size of an iter's state
#define ypIter_STATE_SIZE_MAX \
    ((yp_ssize_t)MIN(yp_SSIZE_T_MAX - yp_sizeof(ypIterObject), ypObject_LEN_MAX))

// Iterator methods

static ypObject *_iter_send(ypObject *i, ypObject *value)
{
    ypObject           *result;
    yp_generator_func_t func = ypIter_FUNC(i);

    yp_DEBUG("iter_send: func %p, i %p, value %d %p", func, i, ypObject_TYPE_CODE(value), value);
    result = func(i, value);
    yp_DEBUG("iter_send: func %p, i %p, value %d %p, result %d %p", func, i,
            ypObject_TYPE_CODE(value), value, ypObject_TYPE_CODE(result), result);

    return result;
}

static ypObject *iter_traverse(ypObject *i, visitfunc visitor, void *memo)
{
    return _ypState_traverse(ypIter_STATE(i), ypIter_OBJLOCS(i), visitor, memo);
}

static ypObject *iter_bool(ypObject *i) { return yp_True; }

// Decrements the reference count of the visited object
static ypObject *_iter_closing_visitor(ypObject *x, void *memo)
{
    yp_decref(x);
    return yp_None;
}

static ypObject *iter_currenthash(
        ypObject *i, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // Since iters are compared by identity, we can cache our hash
    *hash = ypObject_CACHED_HASH(i) = yp_HashPointer(i);
    return yp_None;
}

static ypObject *_iter_closed_generator(ypObject *i, ypObject *value) { return yp_StopIteration; }
static ypObject *iter_close(ypObject *i)
{
    // Let the generator know we're closing
    ypObject *result = _iter_send(i, yp_GeneratorExit);

    // Close off this iterator
    (void)iter_traverse(i, _iter_closing_visitor, NULL);  // never fails
    ypIter_OBJLOCS(i) = 0x0u;                             // they are all discarded now...
    ypIter_LENHINT(i) = 0;
    ypIter_FUNC(i) = _iter_closed_generator;

    // Handle the returned value from the generator. yp_StopIteration and yp_GeneratorExit are not
    // errors. Any other exception or yielded value _is_ an error, as per Python.
    if (yp_isexceptionCN(result, 2, yp_StopIteration, yp_GeneratorExit)) return yp_None;
    if (yp_isexceptionC(result)) return result;
    yp_decref(result);  // discard unexpectedly-yielded value
    return yp_RuntimeError;
}

// iter objects can be returned from yp_miniiter...they simply ignore *state
static ypObject *iter_miniiter(ypObject *i, yp_uint64_t *state)
{
    *state = 0;  // just in case...
    return yp_incref(i);
}

static ypObject *iter_send(ypObject *i, ypObject *value);
static ypObject *iter_miniiter_next(ypObject *i, yp_uint64_t *state)
{
    return iter_send(i, yp_None);
}

static ypObject *iter_miniiter_length_hint(ypObject *i, yp_uint64_t *state, yp_ssize_t *length_hint)
{
    *length_hint = ypIter_LENHINT(i) < 0 ? 0 : ypIter_LENHINT(i);
    return yp_None;
}

static ypObject *iter_iter(ypObject *i) { return yp_incref(i); }

static ypObject *iter_send(ypObject *i, ypObject *value)
{
    ypObject *result = _iter_send(i, value);

    // As per Python, when a generator raises an exception, it can't continue to yield values, so
    // close it. If iter_close fails just ignore it: result is already set to an exception.
    // TODO Don't hide errors from iter_close; instead, use Python's "while handling this
    // exception, another occurred" style of reporting
    if (yp_isexceptionC(result)) {
        (void)iter_close(i);
        return result;
    }

    ypIter_LENHINT(i) -= 1;
    return result;
}

static ypObject *iter_dealloc(ypObject *i, void *memo)
{
    // FIXME Is there something better we can do to handle errors than just ignore them?
    // TODO iter_close calls yp_decref. Can we get it to call yp_decref_fromdealloc instead?
    (void)iter_close(i);  // ignore errors; discards all references
    ypMem_FREE_CONTAINER(i, ypIterObject);
    return yp_None;
}

// XXX This is a function, not a type, in Python. Should we do the same?
static ypObject *iter_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 3, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_iter);
    if (argarray[2] == yp_Arg_Missing) {
        return yp_iter(argarray[1]);
    } else {
        return yp_iter2(argarray[1], argarray[2]);
    }
}

yp_IMMORTAL_FUNCTION_static(iter_func_new, iter_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_object), NULL},
                {yp_CONST_REF(yp_s_sentinel), yp_CONST_REF(yp_Arg_Missing)},
                {yp_CONST_REF(yp_s_slash), NULL}));

static ypTypeObject ypIter_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(iter_func_new),  // tp_func_new
        iter_dealloc,                 // tp_dealloc
        iter_traverse,                // tp_traverse
        NULL,                         // tp_str
        NULL,                         // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,       // tp_freeze
        MethodError_objproc,       // tp_unfrozen_copy
        MethodError_objproc,       // tp_frozen_copy
        MethodError_traversefunc,  // tp_unfrozen_deepcopy
        MethodError_traversefunc,  // tp_frozen_deepcopy
        MethodError_objproc,       // tp_invalidate

        // Boolean operations and comparisons
        iter_bool,                   // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        NotImplemented_comparefunc,  // tp_eq
        NotImplemented_comparefunc,  // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        iter_currenthash,  // tp_currenthash
        iter_close,        // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        iter_miniiter,              // tp_miniiter
        TypeError_miniiterfunc,     // tp_miniiter_reversed
        iter_miniiter_next,         // tp_miniiter_next
        iter_miniiter_length_hint,  // tp_miniiter_length_hint
        iter_iter,                  // tp_iter
        TypeError_objproc,          // tp_iter_reversed
        iter_send,                  // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

// Public functions

// FIXME Compare against Python's __length_hint__ now that it's official.
yp_ssize_t yp_length_hintC(ypObject *i, ypObject **exc)
{
    yp_ssize_t length_hint;
    // FIXME Make a tp_length_hint slot
    if (ypObject_TYPE_PAIR_CODE(i) != ypIter_CODE) {
        return_yp_CEXC_BAD_TYPE(0, exc, i);
    }
    length_hint = ypIter_LENHINT(i);
    return length_hint < 0 ? 0 : length_hint;
}

ypObject *yp_iter_stateCX(ypObject *i, yp_ssize_t *size, void **state)
{
    if (ypObject_TYPE_PAIR_CODE(i) != ypIter_CODE) {
        *size = 0;
        *state = NULL;
        return_yp_BAD_TYPE(i);
    }
    *size = ypIter_STATE_SIZE(i);
    *state = ypIter_STATE(i);
    return yp_None;
}

// TODO Double-check and test the boundary conditions in this function
// XXX Yes, Python also allows unpacking of non-sequence iterables: a,b,c={1,2,3} is valid
// TODO It seems very suspect to allow unpack to work on non-sequences. Is this important enough to
// break with Python? Then again, Python has ordered dicts by default, and because we share dict/set
// code, our yp_sets will be ordered too. So perhaps this is OK again.
void yp_unpackN(ypObject *iterable, int n, ...)
{
    return_yp_V_FUNC_void(yp_unpackNV, (iterable, n, args), n);
}

void yp_unpackNV(ypObject *iterable, int n, va_list args_orig)
{
    yp_uint64_t mi_state;
    ypObject   *mi;
    va_list     args;
    int         remaining;
    ypObject   *x = yp_None;  // set to None in case n==0
    ypObject  **dest;

    // Set the given n arguments to the values yielded from iterable; if an exception occurs, we
    // will need to restart and discard these values. Remember that if yp_miniiter fails,
    // yp_miniiter_next will return the same exception.
    // TODO Hmmm; let's say iterable was yp_StopIteration for some reason: this code would actually
    // succeed when n=0 even though it should probably fail...we should check the yp_miniiter
    // return (here and elsewhere)
    mi = yp_miniiter(iterable, &mi_state);  // new ref
    va_copy(args, args_orig);
    for (remaining = n; remaining > 0; remaining--) {
        x = yp_miniiter_next(mi, &mi_state);  // new ref
        if (yp_isexceptionC(x)) {
            // If the iterable is too short, raise yp_ValueError
            if (yp_isexceptionC2(x, yp_StopIteration)) x = yp_ValueError;
            break;
        }

        dest = va_arg(args, ypObject **);
        *dest = x;
    }
    va_end(args);

    // If we've been successful so far, then ensure we're at the end of iterable
    if (!yp_isexceptionC(x)) {
        x = yp_miniiter_next(mi, &mi_state);  // new ref
        if (yp_isexceptionC2(x, yp_StopIteration)) {
            x = yp_None;  // success!
        } else if (yp_isexceptionC(x)) {
            // some other exception occurred
        } else {
            // If the iterable is too long, raise yp_ValueError
            yp_decref(x);
            x = yp_ValueError;
        }
    }

    // If an error occurred above, then we need to discard the previously-yielded values and set
    // all dests to the exception; otherwise, we're successful, so return
    if (yp_isexceptionC(x)) {
        va_copy(args, args_orig);
        for (/*n already set*/; n > remaining; n--) {
            dest = va_arg(args, ypObject **);
            yp_decref(*dest);
            *dest = x;
        }
        for (/*n already set*/; n > 0; n--) {
            dest = va_arg(args, ypObject **);
            *dest = x;
        }
        va_end(args);
    }
    yp_decref(mi);
}

// Generator Constructors

ypObject *yp_generatorC(yp_generator_decl_t *declaration)
{
    yp_ssize_t  length_hint = declaration->length_hint;
    yp_ssize_t  state_size;
    yp_uint32_t state_objlocs;
    ypObject   *result;
    ypObject   *i;

    result = _ypState_fromdecl(&state_size, &state_objlocs, declaration->state_decl);
    if (yp_isexceptionC(result)) return result;
    if (state_size > ypIter_STATE_SIZE_MAX) return yp_MemorySizeOverflowError;

    // Allocate the iterator
    i = ypMem_MALLOC_CONTAINER_INLINE(ypIterObject, ypIter_CODE, state_size, ypIter_STATE_SIZE_MAX);
    if (yp_isexceptionC(i)) return i;

    // Set attributes, increment reference counts, and return
    i->ob_len = ypObject_LEN_INVALID;
    if (length_hint > ypIter_LENHINT_MAX) length_hint = ypIter_LENHINT_MAX;
    ypIter_LENHINT(i) = (_yp_iter_length_hint_t)length_hint;
    ypIter_FUNC(i) = declaration->func;
    _ypState_copy(ypIter_STATE(i), declaration->state, state_size, state_objlocs);
    ypIter_SET_STATE_SIZE(i, state_size);
    ypIter_OBJLOCS(i) = state_objlocs;
    return i;
}


// Iter Constructors from Mini Iterator Types
// (Allows full iter objects to be created from types that support the mini iterator protocol)

#define ypMiIter_MI(i) (((ypMiIterObject *)i)->mi)
#define ypMiIter_MI_STATE(i) (((ypMiIterObject *)i)->mi_state)

static ypObject *_ypMiIter_generator(ypObject *i, ypObject *value)
{
    ypObject *mi = ypMiIter_MI(i);
    if (yp_isexceptionC(value)) return value;  // yp_GeneratorExit, in particular
    return ypObject_TYPE(mi)->tp_miniiter_next(mi, &ypMiIter_MI_STATE(i));
}

static ypObject *_ypMiIter_fromminiiter(ypObject *x, miniiterfunc mi_constructor)
{
    ypObject  *i;
    ypObject  *mi;
    ypObject  *result;
    yp_ssize_t length_hint;

    // Allocate the iterator
    i = ypMem_MALLOC_FIXED(ypMiIterObject, ypIter_CODE);
    if (yp_isexceptionC(i)) return i;

    // Call the miniiterator "constructor" and get the length hint
    mi = mi_constructor(x, &ypMiIter_MI_STATE(i));
    if (yp_isexceptionC(mi)) {
        ypMem_FREE_FIXED(i);
        return mi;
    }
    result = ypObject_TYPE(mi)->tp_miniiter_length_hint(mi, &ypMiIter_MI_STATE(i), &length_hint);
    if (yp_isexceptionC(result)) {
        yp_decref(mi);
        ypMem_FREE_FIXED(i);
        return result;
    }

    // Set the attributes and return (mi_state set above)
    i->ob_len = ypObject_LEN_INVALID;
    ypIter_STATE(i) = &(ypMiIter_MI(i));
    ypIter_SET_STATE_SIZE(i, yp_sizeof(ypMiIterObject) - yp_offsetof(ypMiIterObject, mi));
    ypIter_FUNC(i) = _ypMiIter_generator;
    ypIter_OBJLOCS(i) = 0x1u;  // indicates mi at state offset zero
    ypMiIter_MI(i) = mi;
    if (length_hint > ypIter_LENHINT_MAX) length_hint = ypIter_LENHINT_MAX;
    ypIter_LENHINT(i) = (_yp_iter_length_hint_t)length_hint;
    return i;
}

// Assign this to tp_iter for types that support mini iterators
static ypObject *_ypIter_fromminiiter(ypObject *x)
{
    return _ypMiIter_fromminiiter(x, ypObject_TYPE(x)->tp_miniiter);
}

// Assign this to tp_iter_reversed for types that support reversed mini iterators
static ypObject *_ypIter_fromminiiter_rev(ypObject *x)
{
    return _ypMiIter_fromminiiter(x, ypObject_TYPE(x)->tp_miniiter_reversed);
}


// Iter Constructors from Iterator-From-Callable Types
// (Allows full iter objects to be created from types that support the mini iterator protocol)

#define ypCallableIter_CALLABLE(i) (((ypCallableIterObject *)i)->callable)
#define ypCallableIter_SENTINEL(i) (((ypCallableIterObject *)i)->sentinel)

static ypObject *_ypCallableIter_generator(ypObject *i, ypObject *value)
{
    ypObject *argarray[] = {ypCallableIter_CALLABLE(i)};
    ypObject *call_result;
    ypObject *eq_result;

    if (yp_isexceptionC(value)) return value;  // yp_GeneratorExit, in particular

    call_result = yp_call_arrayX(1, argarray);  // new ref
    if (yp_isexceptionC(call_result)) return call_result;

    eq_result = yp_eq(ypCallableIter_SENTINEL(i), call_result);
    if (eq_result == yp_False) return call_result;  // success

    // If we get here, we are returning an error, so discard call_result.
    yp_decref(call_result);
    return yp_isexceptionC(eq_result) ? eq_result : yp_StopIteration;
}

ypObject *yp_iter2(ypObject *callable, ypObject *sentinel)
{
    ypObject *i;

    // Allocate the iterator
    i = ypMem_MALLOC_FIXED(ypCallableIterObject, ypIter_CODE);
    if (yp_isexceptionC(i)) return i;

    // Set the attributes and return
    i->ob_len = ypObject_LEN_INVALID;
    ypIter_STATE(i) = &(ypCallableIter_CALLABLE(i));
    ypIter_SET_STATE_SIZE(
            i, yp_sizeof(ypCallableIterObject) - yp_offsetof(ypCallableIterObject, callable));
    ypIter_FUNC(i) = _ypCallableIter_generator;
    ypIter_OBJLOCS(i) = 0x3u;  // two objects: callable and sentinel
    ypCallableIter_CALLABLE(i) = yp_incref(callable);
    ypCallableIter_SENTINEL(i) = yp_incref(sentinel);
    ypIter_LENHINT(i) = 0;  // it's not known how many values will be yielded
    return i;
}


// Generic Mini Iterator Methods for Sequences

yp_STATIC_ASSERT(yp_sizeof(yp_uint64_t) >= yp_sizeof(yp_ssize_t), ssize_fits_uint64);

static ypObject *_ypSequence_miniiter(ypObject *x, yp_uint64_t *state)
{
    *state = 0;  // start at zero (first element) and count up
    return yp_incref(x);
}

static ypObject *_ypSequence_miniiter_rev(ypObject *x, yp_uint64_t *state)
{
    *state = (yp_uint64_t)-1;  // start at -1 (last element) and count down
    return yp_incref(x);
}

static ypObject *_ypSequence_miniiter_next(ypObject *x, yp_uint64_t *_state)
{
    yp_int64_t *state = (yp_int64_t *)_state;
    ypObject   *result = yp_getindexC(x, (yp_ssize_t)*state);
    if (yp_isexceptionC(result)) {
        return yp_isexceptionC2(result, yp_IndexError) ? yp_StopIteration : result;
    }

    if (*state >= 0) {  // we are counting up from the first element
        *state += 1;
    } else {  // we are counting down from the last element
        *state -= 1;
    }
    return result;
}

// XXX Note that yp_miniiter_length_hintC checks for negative hints and returns zero instead
static ypObject *_ypSequence_miniiter_lenh(
        ypObject *x, yp_uint64_t *_state, yp_ssize_t *length_hint)
{
    yp_int64_t *state = (yp_int64_t *)_state;
    ypObject   *exc = yp_None;
    yp_ssize_t  len = yp_lenC(x, &exc);
    if (yp_isexceptionC(exc)) return exc;
    if (*state >= 0) {
        *length_hint = len - ((yp_ssize_t)*state);
    } else {
        *length_hint = len - ((yp_ssize_t)(-1 - *state));
    }
    return yp_None;
}

#pragma endregion iter


/*************************************************************************************************
 * Freezing, "unfreezing", and invalidating
 *************************************************************************************************/
#pragma region transmute
// TODO Rethink what we delegate to the tp_* functions: we should probably give them more control.

typedef struct _yp_deepcopy_memo_t {
    yp_ssize_t recursion_depth;
    ypObject  *keep_alive;  // Ensure objects live at least as long as their IDs are stored.
    ypObject  *copies;      // Maps original object IDs to their copies.
} yp_deepcopy_memo_t;

extern ypObject *const yp_RecursionLimitError;

// TODO If len==0, replace it with the immortal "zero-version" of the type
//  WAIT! I can't do that, because that won't freeze the original and others might be referencing
//  the original so won't see it as frozen now.
//  SO! Still freeze the original, but then also replace it with the zero-version
static ypObject *_yp_freeze(ypObject *x)
{
    int           oldCode = ypObject_TYPE_CODE(x);
    int           newCode = ypObject_TYPE_CODE_AS_FROZEN(oldCode);
    ypTypeObject *newType;
    ypObject     *exc = yp_None;

    // Check if it's already frozen (no-op) or if it can't be frozen (error)
    if (oldCode == newCode) return yp_None;
    newType = ypTypeTable[newCode];
    yp_ASSERT(newType != NULL, "all types should have an immutable counterpart");

    // Freeze the object, possibly reduce memory usage, etc
    // TODO Support unfreezable objects. Let tp_freeze set the type code as appropriate, then
    // inspect it after to see if it worked. (Or return yp_NotImplemented.) Perhaps return an
    // exception if the top-level freeze doesn't freeze, but in the case of deep freeze allow deeper
    // objects to silently fail to freeze.
    exc = newType->tp_freeze(x);
    if (yp_isexceptionC(exc)) return exc;
    ypObject_SET_TYPE_CODE(x, newCode);
    return exc;
}

void yp_freeze(ypObject *x, ypObject **exc)
{
    ypObject *result = _yp_freeze(x);
    if (yp_isexceptionC(result)) return_yp_EXC_ERR(exc, result);
}

static ypObject *_yp_deepfreeze(ypObject *x, void *_memo)
{
    ypObject *exc = yp_None;
    ypObject *memo = (ypObject *)_memo;
    ypObject *id;
    ypObject *result;

    // Avoid recursion: we only have to visit each object once
    // TODO An easier way to accomplish the same is to inspect ypObject_IS_MUTABLE, since we freeze
    // before going deep.
    // TODO ...and then switch to recursion depth check. In fact, reconsider anywhere we take a
    // "weak reference" to an object as a means of preventing recursion. (And, also, if we do
    // this we need a keepalive like deepcopy does.)
    id = yp_intC((yp_ssize_t)x);
    yp_pushunique(memo, id, &exc);
    yp_decref(id);
    if (yp_isexceptionC(exc)) {
        if (yp_isexceptionC2(exc, yp_KeyError)) return yp_None;  // already in set
        return exc;
    }

    // Freeze current object before going deep
    // XXX tp_traverse must propagate exceptions returned by visitor
    result = _yp_freeze(x);
    if (yp_isexceptionC(result)) return result;
    return ypObject_TYPE(x)->tp_traverse(x, _yp_deepfreeze, memo);
}

// TODO All "deep" operations may try to operate on immortals...but shouldn't all immortals be
// immutable already anyway?
void yp_deepfreeze(ypObject *x, ypObject **exc)
{
    ypObject *memo = yp_setN(0);
    ypObject *result = _yp_deepfreeze(x, memo);
    yp_decref(memo);
    if (yp_isexceptionC(result)) return_yp_EXC_ERR(exc, result);
}

// Returns the existing deep-copied object made from x, or yp_KeyError if no such copy has been made
// yet.
static ypObject *_yp_deepcopy_memo_getitem(void *_memo, ypObject *x)
{
    yp_deepcopy_memo_t *memo = (yp_deepcopy_memo_t *)_memo;
    ypObject           *x_id;
    ypObject           *result;

    // We keep the objects in memo uninitialized until we need them.
    if (memo->copies == NULL) return yp_KeyError;

    // TODO It'd be nice if we didn't have to create this temporary int...
    x_id = yp_intC((yp_ssize_t)x);  // new ref
    result = yp_getitem(memo->copies, x_id);
    yp_decref(x_id);
    return result;
}

// Adds the new deep-copied object made from x to the memo.
static ypObject *_yp_deepcopy_memo_setitem(void *_memo, ypObject *x, ypObject *x_copy)
{
    yp_deepcopy_memo_t *memo = (yp_deepcopy_memo_t *)_memo;
    ypObject           *exc = yp_None;
    ypObject           *x_id;

    // We keep the objects in memo uninitialized until we need them.
    if (memo->keep_alive == NULL) {
        memo->keep_alive = yp_listN(0);
        memo->copies = yp_dictK(0);
    }

    // Ensure x stays alive at least as long as we reference its ID.
    yp_append(memo->keep_alive, x, &exc);
    if (yp_isexceptionC(exc)) return exc;

    // TODO It'd be nice if we didn't have to create this temporary int...
    x_id = yp_intC((yp_ssize_t)x);  // new ref
    if (yp_isexceptionC(x_id)) return x_id;

    yp_setitem(memo->copies, x_id, x_copy, &exc);
    yp_decref(x_id);
    if (yp_isexceptionC(exc)) return exc;
    return yp_None;
}

// Common code for all the deepcopy visitors.
// XXX Remember: deep copies always copy everything except hashable (immutable) immortals...and
// maybe even those should be copied as well...or just those that contain other objects?
// XXX This is contrary to in Python, where if immutables (tuple, frozenset, etc) contain the exact
// same objects, it discards the copy and returns the original object (i.e. {1, 2, 3}).
static ypObject *_yp_deepcopy_visitor(ypObject *x, visitfunc visitor, void *_memo, int freeze)
{
    yp_deepcopy_memo_t *memo = (yp_deepcopy_memo_t *)_memo;
    ypObject           *result;

    yp_DEBUG("deepcopy: %p type %d depth %" PRIssize, x, ypObject_TYPE_CODE(x),
            memo->recursion_depth);

    // Be efficient: reuse existing copies of objects. Also helps avoid recursion!
    result = _yp_deepcopy_memo_getitem(memo, x);
    if (!yp_isexceptionC2(result, yp_KeyError)) {
        // We've either already made a copy of this object, or some (unexpected) error occurred.
        return result;
    }

    // This is a failsafe: ideally, all types use _yp_deepcopy_memo_setitem to avoid recursion.
    yp_ASSERT(memo->recursion_depth >= 0, "recursion_depth can't be negative");
    if (memo->recursion_depth > _yp_recursion_limit) {
        return yp_RecursionLimitError;
    }

    // If we get here, then this is the first time visiting this object. We leave it to the types to
    // call _yp_deepcopy_memo_setitem as appropriate.
    memo->recursion_depth += 1;
    if (freeze) {
        result = ypObject_TYPE(x)->tp_frozen_deepcopy(x, visitor, memo);
    } else {
        result = ypObject_TYPE(x)->tp_unfrozen_deepcopy(x, visitor, memo);
    }
    memo->recursion_depth -= 1;
    return result;
}

static ypObject *_yp_deepcopy(ypObject *x, visitfunc visitor)
{
    yp_deepcopy_memo_t memo = {0};  // Avoid creating unnecessary objects: initialize to NULL.

    ypObject *result = visitor(x, &memo);

    if (memo.keep_alive != NULL) {
        yp_decref(memo.copies);
        yp_decref(memo.keep_alive);
    }

    return result;
}

ypObject *yp_unfrozen_copy(ypObject *x) { return ypObject_TYPE(x)->tp_unfrozen_copy(x); }

// Use this as the visitor for "unfrozen" deep copies (i.e. yp_unfrozen_deepcopy).
static ypObject *_yp_unfrozen_deepcopy_visitor(ypObject *x, void *memo)
{
    return _yp_deepcopy_visitor(x, _yp_unfrozen_deepcopy_visitor, memo, FALSE);
}

ypObject *yp_unfrozen_deepcopy(ypObject *x)
{
    return _yp_deepcopy(x, _yp_unfrozen_deepcopy_visitor);
}

ypObject *yp_frozen_copy(ypObject *x) { return ypObject_TYPE(x)->tp_frozen_copy(x); }

// Use this as the visitor for "frozen" deep copies (i.e. yp_frozen_deepcopy).
static ypObject *_yp_frozen_deepcopy_visitor(ypObject *x, void *memo)
{
    return _yp_deepcopy_visitor(x, _yp_frozen_deepcopy_visitor, memo, TRUE);
}

ypObject *yp_frozen_deepcopy(ypObject *x) { return _yp_deepcopy(x, _yp_frozen_deepcopy_visitor); }

ypObject *yp_copy(ypObject *x)
{
    return ypObject_IS_MUTABLE(x) ? yp_unfrozen_copy(x) : yp_frozen_copy(x);
}

// Use this as the visitor for "same type" deep copies (i.e. yp_deepcopy).
static ypObject *_yp_sametype_deepcopy_visitor(ypObject *x, void *memo)
{
    return _yp_deepcopy_visitor(x, _yp_sametype_deepcopy_visitor, memo, !ypObject_IS_MUTABLE(x));
}

ypObject *yp_deepcopy(ypObject *x) { return _yp_deepcopy(x, _yp_sametype_deepcopy_visitor); }

// TODO CONTAINER_INLINE objects won't release any of their memory on invalidation. This is a
// tradeoff in the interests of reducing individual allocations. Perhaps there should be a limit
// on how large to make CONTAINER_INLINE objects, or perhaps we should try to shrink the
// invalidated object in-place (if supported by the heap).
// TODO Should attempting to invalidate an immortal be an error?
void yp_invalidate(ypObject *x, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_NotImplementedError);  // TODO implement
}

// TODO All "deep" operations may try to operate on immortals, which should not be invalidated.
// Should this be an exception, or should these objects be silently skipped?
void yp_deepinvalidate(ypObject *x, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_NotImplementedError);  // TODO implement
}

#pragma endregion transmute


/*************************************************************************************************
 * Boolean operations, comparisons, and generic object operations
 *************************************************************************************************/
#pragma region comparison

// Returns 1 if the object is yp_True, else 0. b must be a bool or an exception.
#define ypBool_IS_TRUE_C(b) ((b) == yp_True)

// Returns 1 if the object is yp_False, else 0. b must be a bool or an exception.
#define ypBool_IS_FALSE_C(b) ((b) == yp_False)

// Returns yp_True if cond is true (non-zero), else yp_False.
#define ypBool_FROM_C(cond) ((cond) ? yp_True : yp_False)

// yp_not as a macro. b must be a bool or an exception.
// XXX b should be a variable, _not_ an expression, as it's evaluated up to three times
// clang-format off
#define ypBool_NOT(b) ( (b) == yp_True ? yp_False : \
                       ((b) == yp_False ? yp_True : (b)))
// clang-format on

ypObject *yp_not(ypObject *x)
{
    ypObject *result = yp_bool(x);
    return ypBool_NOT(result);
}

ypObject *yp_or(ypObject *x, ypObject *y)
{
    ypObject *b = yp_bool(x);
    if (yp_isexceptionC(b)) return b;
    if (b == yp_False) return yp_incref(y);
    return yp_incref(x);
}

ypObject *yp_orN(int n, ...) { return_yp_V_FUNC(ypObject *, yp_orNV, (n, args), n); }
ypObject *yp_orNV(int n, va_list args)
{
    ypObject *x;
    ypObject *b;
    if (n < 1) return yp_False;
    for (/*n already set*/; n > 1; n--) {
        x = va_arg(args, ypObject *);  // borrowed
        b = yp_bool(x);
        if (yp_isexceptionC(b)) return b;
        if (b == yp_True) return yp_incref(x);
    }
    // If everything else was false, we always return the last object
    return yp_incref(va_arg(args, ypObject *));
}

ypObject *yp_anyN(int n, ...) { return_yp_V_FUNC(ypObject *, yp_anyNV, (n, args), n); }
ypObject *yp_anyNV(int n, va_list args)
{
    for (/*n already set*/; n > 0; n--) {
        ypObject *b = yp_bool(va_arg(args, ypObject *));
        if (b != yp_False) return b;  // exit on yp_True or exception
    }
    return yp_False;
}

ypObject *yp_any(ypObject *iterable)
{
    ypObject   *mi;
    yp_uint64_t mi_state;
    ypObject   *x;
    ypObject   *result = yp_False;

    mi = yp_miniiter(iterable, &mi_state);  // new ref
    while (1) {
        x = yp_miniiter_next(mi, &mi_state);  // new ref
        if (yp_isexceptionC2(x, yp_StopIteration)) break;
        result = yp_bool(x);
        yp_decref(x);
        if (result != yp_False) break;  // exit on yp_True or exception
    }
    yp_decref(mi);
    return result;
}

ypObject *yp_and(ypObject *x, ypObject *y)
{
    ypObject *b = yp_bool(x);
    if (yp_isexceptionC(b)) return b;
    if (b == yp_False) return yp_incref(x);
    return yp_incref(y);
}

ypObject *yp_andN(int n, ...) { return_yp_V_FUNC(ypObject *, yp_andNV, (n, args), n); }
ypObject *yp_andNV(int n, va_list args)
{
    ypObject *x;
    ypObject *b;
    if (n < 1) return yp_True;
    for (/*n already set*/; n > 1; n--) {
        x = va_arg(args, ypObject *);  // borrowed
        b = yp_bool(x);
        if (yp_isexceptionC(b)) return b;
        if (b == yp_False) return yp_incref(x);
    }
    // If everything else was true, we always return the last object
    return yp_incref(va_arg(args, ypObject *));
}

ypObject *yp_allN(int n, ...) { return_yp_V_FUNC(ypObject *, yp_allNV, (n, args), n); }
ypObject *yp_allNV(int n, va_list args)
{
    for (/*n already set*/; n > 0; n--) {
        ypObject *b = yp_bool(va_arg(args, ypObject *));
        if (b != yp_True) return b;  // exit on yp_False or exception
    }
    return yp_True;
}

ypObject *yp_all(ypObject *iterable)
{
    ypObject   *mi;
    yp_uint64_t mi_state;
    ypObject   *x;
    ypObject   *result = yp_True;

    mi = yp_miniiter(iterable, &mi_state);  // new ref
    while (1) {
        x = yp_miniiter_next(mi, &mi_state);  // new ref
        if (yp_isexceptionC2(x, yp_StopIteration)) break;
        result = yp_bool(x);
        yp_decref(x);
        if (result != yp_True) break;  // exit on yp_False or exception
    }
    yp_decref(mi);
    return result;
}

// Defined here are yp_lt, yp_le, yp_eq, yp_ne, yp_ge, and yp_gt
// XXX yp_ComparisonNotImplemented should _never_ be seen outside of comparison functions
// TODO Here and elsewhere, the singleton NotImplemented should be used
// TODO Comparison functions have the possibility of recursion; trap (also, add tests)
#define _ypBool_PUBLIC_CMP_FUNCTION(name, reflection, defval)                         \
    ypObject *yp_##name(ypObject *x, ypObject *y)                                     \
    {                                                                                 \
        ypTypeObject *type = ypObject_TYPE(x);                                        \
        ypObject     *result = type->tp_##name(x, y);                                 \
        yp_ASSERT(result == yp_True || result == yp_False || yp_isexceptionC(result), \
                "tp_" #name " must return yp_True, yp_False, or an exception");       \
        if (result != yp_ComparisonNotImplemented) return result;                     \
        type = ypObject_TYPE(y);                                                      \
        result = type->tp_##reflection(y, x);                                         \
        yp_ASSERT(result == yp_True || result == yp_False || yp_isexceptionC(result), \
                "tp_" #reflection " must return yp_True, yp_False, or an exception"); \
        if (result != yp_ComparisonNotImplemented) return result;                     \
        return (defval);                                                              \
    }
// Recall that tp_lt/etc for exceptions and invalidateds will return the correct exception, so
// yp_TypeError is correct to use here (rather than yp_BAD_TYPE).
_ypBool_PUBLIC_CMP_FUNCTION(lt, gt, yp_TypeError);
_ypBool_PUBLIC_CMP_FUNCTION(le, ge, yp_TypeError);
_ypBool_PUBLIC_CMP_FUNCTION(eq, eq, ypBool_FROM_C(x == y));
_ypBool_PUBLIC_CMP_FUNCTION(ne, ne, ypBool_FROM_C(x != y));
_ypBool_PUBLIC_CMP_FUNCTION(ge, le, yp_TypeError);
_ypBool_PUBLIC_CMP_FUNCTION(gt, lt, yp_TypeError);

// XXX Remember, an immutable container may hold mutable objects; yp_hashC must fail in that case
// TODO Need to decide whether to keep pre-computed hash in ypObject and, if so, if we can remove
// the hash from ypSet's element table
yp_STATIC_ASSERT(ypObject_HASH_INVALID == -1, hash_invalid_is_neg_one);

static ypObject *_yp_hash_visitor(ypObject *x, void *_memo, yp_hash_t *hash)
{
    yp_ssize_t recursion_depth = (yp_ssize_t)_memo;
    ypObject  *result;

    // To get the hash of a mutable value, use yp_currenthashC
    if (ypObject_IS_MUTABLE(x)) {
        *hash = ypObject_HASH_INVALID;
        return_yp_BAD_TYPE(x);
    }

    // If the hash has already been calculated, return it immediately
    if (ypObject_CACHED_HASH(x) != ypObject_HASH_INVALID) {
        *hash = ypObject_CACHED_HASH(x);
        return yp_None;
    }

    // Protect against circular references while calculating the hash
    yp_ASSERT(recursion_depth >= 0, "recursion_depth can't be negative");
    if (recursion_depth > _yp_recursion_limit) {
        *hash = ypObject_HASH_INVALID;
        return yp_RecursionLimitError;
    }

    // TODO Contribute this generic hash caching back to Python?
    result = ypObject_TYPE(x)->tp_currenthash(
            x, _yp_hash_visitor, (void *)(recursion_depth + 1), hash);
    if (yp_isexceptionC(result)) {
        *hash = ypObject_HASH_INVALID;
        return result;
    }
    ypObject_CACHED_HASH(x) = *hash;
    return yp_None;
}
yp_hash_t yp_hashC(ypObject *x, ypObject **exc)
{
    yp_hash_t hash;
    ypObject *result = _yp_hash_visitor(x, (void *)0, &hash);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(ypObject_HASH_INVALID, exc, result);
    return hash;
}

static ypObject *_yp_cachedhash_visitor(ypObject *x, void *_memo, yp_hash_t *hash)
{
    yp_ssize_t recursion_depth = (yp_ssize_t)_memo;
    ypObject  *result;

    // Check cached hash, and recursion depth first
    if (!ypObject_IS_MUTABLE(x) && ypObject_CACHED_HASH(x) != ypObject_HASH_INVALID) {
        *hash = ypObject_CACHED_HASH(x);
        return yp_None;
    }
    yp_ASSERT(recursion_depth >= 0, "recursion_depth can't be negative");
    if (recursion_depth > _yp_recursion_limit) {
        *hash = ypObject_HASH_INVALID;
        return yp_RecursionLimitError;
    }

    result = ypObject_TYPE(x)->tp_currenthash(
            x, _yp_cachedhash_visitor, (void *)(recursion_depth + 1), hash);
    // XXX We can't record the cached hash here: consider that yp_hash on a tuple with mutable
    // objects cannot succeed, but we (ie yp_currenthash) _can_
    return result;
}
yp_hash_t yp_currenthashC(ypObject *x, ypObject **exc)
{
    yp_hash_t hash;
    ypObject *result = _yp_cachedhash_visitor(x, (void *)0, &hash);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(ypObject_HASH_INVALID, exc, result);
    return hash;
}

yp_ssize_t yp_lenC(ypObject *x, ypObject **exc)
{
    // FIXME Rethink these "cached len/hash" optimizations: should we always call the method?
    yp_ssize_t len = ypObject_CACHED_LEN(x);
    ypObject  *result;

    if (len >= 0) return len;
    result = ypObject_TYPE(x)->tp_len(x, &len);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(0, exc, result);
    yp_ASSERT(len >= 0, "tp_len cannot return negative");
    return len;
}

#pragma endregion comparison


/*************************************************************************************************
 * Invalidated Objects
 *************************************************************************************************/
#pragma region invalidated

static ypObject *invalidated_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return yp_NotImplementedError;
}

yp_IMMORTAL_FUNCTION_static(invalidated_func_new, invalidated_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_slash), NULL},
                {yp_CONST_REF(yp_s_star_args), NULL}, {yp_CONST_REF(yp_s_star_star_kwargs), NULL}));

static ypTypeObject ypInvalidated_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(invalidated_func_new),  // tp_func_new
        MethodError_visitfunc,               // tp_dealloc  // FIXME implement
        NoRefs_traversefunc,                 // tp_traverse
        NULL,                                // tp_str
        NULL,                                // tp_repr

        // Freezing, copying, and invalidating
        InvalidatedError_objproc,       // tp_freeze
        InvalidatedError_objproc,       // tp_unfrozen_copy
        InvalidatedError_objproc,       // tp_frozen_copy
        InvalidatedError_traversefunc,  // tp_unfrozen_deepcopy
        InvalidatedError_traversefunc,  // tp_frozen_deepcopy
        InvalidatedError_objproc,       // tp_invalidate

        // Boolean operations and comparisons
        InvalidatedError_objproc,     // tp_bool
        InvalidatedError_objobjproc,  // tp_lt
        InvalidatedError_objobjproc,  // tp_le
        InvalidatedError_objobjproc,  // tp_eq
        InvalidatedError_objobjproc,  // tp_ne
        InvalidatedError_objobjproc,  // tp_ge
        InvalidatedError_objobjproc,  // tp_gt

        // Generic object operations
        InvalidatedError_hashfunc,  // tp_currenthash
        InvalidatedError_objproc,   // tp_close

        // Number operations
        InvalidatedError_NumberMethods,  // tp_as_number

        // Iterator operations
        InvalidatedError_miniiterfunc,       // tp_miniiter
        InvalidatedError_miniiterfunc,       // tp_miniiter_reversed
        InvalidatedError_miniiterfunc,       // tp_miniiter_next
        InvalidatedError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        InvalidatedError_objproc,            // tp_iter
        InvalidatedError_objproc,            // tp_iter_reversed
        InvalidatedError_objobjproc,         // tp_send

        // Container operations
        InvalidatedError_objobjproc,     // tp_contains
        InvalidatedError_lenfunc,        // tp_len
        InvalidatedError_objobjproc,     // tp_push
        InvalidatedError_objproc,        // tp_clear
        InvalidatedError_objproc,        // tp_pop
        InvalidatedError_objobjobjproc,  // tp_remove
        InvalidatedError_objobjobjproc,  // tp_getdefault
        InvalidatedError_objobjobjproc,  // tp_setitem
        InvalidatedError_objobjproc,     // tp_delitem
        InvalidatedError_objvalistproc,  // tp_update

        // Sequence operations
        InvalidatedError_SequenceMethods,  // tp_as_sequence

        // Set operations
        InvalidatedError_SetMethods,  // tp_as_set

        // Mapping operations
        InvalidatedError_MappingMethods,  // tp_as_mapping

        // Callable operations
        InvalidatedError_CallableMethods  // tp_as_callable
};

#pragma endregion invalidated


/*************************************************************************************************
 * Exceptions
 *************************************************************************************************/
#pragma region exception

// TODO A nohtyP.h macro to get exception info as a string, include file/line info of the place
// the macro is checked. Something to make reporting exceptions easier for the user of nohtyP.

#define _ypException_NAME(e) (((ypExceptionObject *)e)->name)
#define _ypException_SUPER(e) (((ypExceptionObject *)e)->super)

static ypObject *exception_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return yp_NotImplementedError;
}

yp_IMMORTAL_FUNCTION_static(exception_func_new, exception_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_slash), NULL},
                {yp_CONST_REF(yp_s_star_args), NULL}, {yp_CONST_REF(yp_s_star_star_kwargs), NULL}));

static ypTypeObject ypException_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(exception_func_new),  // tp_func_new
        MethodError_visitfunc,             // tp_dealloc
        NoRefs_traversefunc,               // tp_traverse
        NULL,                              // tp_str
        NULL,                              // tp_repr

        // Freezing, copying, and invalidating
        ExceptionMethod_objproc,       // tp_freeze
        ExceptionMethod_objproc,       // tp_unfrozen_copy
        ExceptionMethod_objproc,       // tp_frozen_copy
        ExceptionMethod_traversefunc,  // tp_unfrozen_deepcopy
        ExceptionMethod_traversefunc,  // tp_frozen_deepcopy
        ExceptionMethod_objproc,       // tp_invalidate

        // Boolean operations and comparisons
        ExceptionMethod_objproc,     // tp_bool
        ExceptionMethod_objobjproc,  // tp_lt
        ExceptionMethod_objobjproc,  // tp_le
        ExceptionMethod_objobjproc,  // tp_eq
        ExceptionMethod_objobjproc,  // tp_ne
        ExceptionMethod_objobjproc,  // tp_ge
        ExceptionMethod_objobjproc,  // tp_gt

        // Generic object operations
        ExceptionMethod_hashfunc,  // tp_currenthash
        ExceptionMethod_objproc,   // tp_close

        // Number operations
        ExceptionMethod_NumberMethods,  // tp_as_number

        // Iterator operations
        ExceptionMethod_miniiterfunc,       // tp_miniiter
        ExceptionMethod_miniiterfunc,       // tp_miniiter_reversed
        ExceptionMethod_miniiterfunc,       // tp_miniiter_next
        ExceptionMethod_miniiter_lenhfunc,  // tp_miniiter_length_hint
        ExceptionMethod_objproc,            // tp_iter
        ExceptionMethod_objproc,            // tp_iter_reversed
        ExceptionMethod_objobjproc,         // tp_send

        // Container operations
        ExceptionMethod_objobjproc,     // tp_contains
        ExceptionMethod_lenfunc,        // tp_len
        ExceptionMethod_objobjproc,     // tp_push
        ExceptionMethod_objproc,        // tp_clear
        ExceptionMethod_objproc,        // tp_pop
        ExceptionMethod_objobjobjproc,  // tp_remove
        ExceptionMethod_objobjobjproc,  // tp_getdefault
        ExceptionMethod_objobjobjproc,  // tp_setitem
        ExceptionMethod_objobjproc,     // tp_delitem
        ExceptionMethod_objvalistproc,  // tp_update

        // Sequence operations
        ExceptionMethod_SequenceMethods,  // tp_as_sequence

        // Set operations
        ExceptionMethod_SetMethods,  // tp_as_set

        // Mapping operations
        ExceptionMethod_MappingMethods,  // tp_as_mapping

        // Callable operations
        ExceptionMethod_CallableMethods  // tp_as_callable
};

// No constructors for exceptions; all such objects are immortal

// The immortal exception objects; this should match Python's hierarchy:
//  http://docs.python.org/3/library/exceptions.html

// FIXME These exception names (the strings) all start with yp_.
#define _yp_IMMORTAL_EXCEPTION_SUPERPTR(name, superptr)                             \
    yp_IMMORTAL_STR_LATIN_1(name##_name, #name);                                    \
    static ypExceptionObject _##name##_struct = {                                   \
            yp_IMMORTAL_HEAD_INIT(ypException_CODE, 0, ypObject_LEN_INVALID, NULL), \
            yp_CONST_REF(name##_name), (superptr)};                                 \
    ypObject *const name = yp_CONST_REF(name) /* force semi-colon */
#define _yp_IMMORTAL_EXCEPTION(name, super) \
    _yp_IMMORTAL_EXCEPTION_SUPERPTR(name, yp_CONST_REF(super))

// clang-format off
_yp_IMMORTAL_EXCEPTION_SUPERPTR(yp_BaseException, NULL);
  _yp_IMMORTAL_EXCEPTION(yp_SystemExit, yp_BaseException);
  _yp_IMMORTAL_EXCEPTION(yp_KeyboardInterrupt, yp_BaseException);
  _yp_IMMORTAL_EXCEPTION(yp_GeneratorExit, yp_BaseException);

  _yp_IMMORTAL_EXCEPTION(yp_Exception, yp_BaseException);
    _yp_IMMORTAL_EXCEPTION(yp_StopIteration, yp_Exception);

    _yp_IMMORTAL_EXCEPTION(yp_ArithmeticError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_FloatingPointError, yp_ArithmeticError);
      _yp_IMMORTAL_EXCEPTION(yp_OverflowError, yp_ArithmeticError);
      _yp_IMMORTAL_EXCEPTION(yp_ZeroDivisionError, yp_ArithmeticError);

    _yp_IMMORTAL_EXCEPTION(yp_AssertionError, yp_Exception);

    _yp_IMMORTAL_EXCEPTION(yp_AttributeError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_MethodError, yp_AttributeError);

    _yp_IMMORTAL_EXCEPTION(yp_BufferError, yp_Exception);
    _yp_IMMORTAL_EXCEPTION(yp_EOFError, yp_Exception);
    _yp_IMMORTAL_EXCEPTION(yp_ImportError, yp_Exception);

    _yp_IMMORTAL_EXCEPTION(yp_LookupError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_IndexError, yp_LookupError);
      _yp_IMMORTAL_EXCEPTION(yp_KeyError, yp_LookupError);

    _yp_IMMORTAL_EXCEPTION(yp_MemoryError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_MemorySizeOverflowError, yp_MemoryError);

    _yp_IMMORTAL_EXCEPTION(yp_NameError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_UnboundLocalError, yp_NameError);

    _yp_IMMORTAL_EXCEPTION(yp_OSError, yp_Exception);
      // TODO Many subexceptions missing here

    _yp_IMMORTAL_EXCEPTION(yp_ReferenceError, yp_Exception);

    _yp_IMMORTAL_EXCEPTION(yp_RuntimeError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_NotImplementedError, yp_RuntimeError);
        // TODO Are we keeping yp_ComparisonNotImplemented? If so, document
        _yp_IMMORTAL_EXCEPTION(yp_ComparisonNotImplemented, yp_NotImplementedError);
      // TODO Document yp_CircularReferenceError and use
      // (Python raises RuntimeError on "maximum recursion depth exceeded", so this fits)
      _yp_IMMORTAL_EXCEPTION(yp_CircularReferenceError, yp_RuntimeError);
      // TODO Document yp_RecursionLimitError
      _yp_IMMORTAL_EXCEPTION(yp_RecursionLimitError, yp_RuntimeError);

    _yp_IMMORTAL_EXCEPTION(yp_SyntaxError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_ParameterSyntaxError, yp_SyntaxError);

    _yp_IMMORTAL_EXCEPTION(yp_SystemError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_SystemLimitationError, yp_SystemError);

    _yp_IMMORTAL_EXCEPTION(yp_TypeError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_InvalidatedError, yp_TypeError);

    _yp_IMMORTAL_EXCEPTION(yp_ValueError, yp_Exception);
      _yp_IMMORTAL_EXCEPTION(yp_UnicodeError, yp_ValueError);
        _yp_IMMORTAL_EXCEPTION(yp_UnicodeEncodeError, yp_UnicodeError);
        _yp_IMMORTAL_EXCEPTION(yp_UnicodeDecodeError, yp_UnicodeError);
        _yp_IMMORTAL_EXCEPTION(yp_UnicodeTranslateError, yp_UnicodeError);
// clang-format on

// yp_isexceptionC defined above

static int _yp_isexceptionC2(ypObject *x, ypObject *exc)
{
    do {
        if (x == exc) return 1;  // x is a (sub)exception of exc
        x = _ypException_SUPER(x);
    } while (x != NULL);
    return 0;  // neither x nor its superexceptions match exc
}

int yp_isexceptionC2(ypObject *x, ypObject *exc)
{
    if (!yp_IS_EXCEPTION_C(x)) return 0;
    return _yp_isexceptionC2(x, exc);
}

static int _yp_isexceptionCNV(ypObject *x, int n, va_list args)
{
    for (/*n already set*/; n > 0; n--) {
        if (_yp_isexceptionC2(x, va_arg(args, ypObject *))) {
            return TRUE;
        }
    }
    return FALSE;
}

int yp_isexceptionCN(ypObject *x, int n, ...)
{
    va_list args;
    int     result;

    if (!yp_IS_EXCEPTION_C(x)) return 0;

    va_start(args, n);
    result = _yp_isexceptionCNV(x, n, args);
    va_end(args);

    return result;
}

int yp_isexceptionCNV(ypObject *x, int n, va_list args)
{
    if (!yp_IS_EXCEPTION_C(x)) return 0;
    return _yp_isexceptionCNV(x, n, args);
}


// TODO Consider this:
//  switch(yp_switchexceptionCN(x, 2, yp_StopIteration, yp_ValueError)) {
//      case 0: // yp_StopIteration
//          break;
//      case 1: // yp_ValueError
//          break;
//      case 2: // returning n means "any other exception"...i.e. the bare "except" clause of try
//          break;
//      default: // *any* other value means no exception...i.e. the "else" clause of try
//          break;
//  }
// Even get fancy and do:
//  #define yp_SWITCH_EXCEPTION(x, n, ...) switch(yp_switchexceptionCN(x, n, ...))
// Then, even fancier, a form of yp_ELSE_EXCEPT (et al) like yp_ELSE_SWITCH_EXCEPT
// ypAPI int yp_switchexceptionCN(ypObject *x, int n, ...);

#pragma endregion exception


/*************************************************************************************************
 * Types
 *************************************************************************************************/
#pragma region type

static ypObject *type_frozen_copy(ypObject *t) { return yp_incref(t); }

static ypObject *type_frozen_deepcopy(ypObject *t, visitfunc copy_visitor, void *copy_memo)
{
    // TODO This assumes all types are statically allocated. deepcopy is used for thread safety as
    // well, so consider how we should be copying these objects for thread safety. Perhaps all types
    // should be immortal in the "interpreter"?
    return yp_incref(t);
}

static ypObject *type_bool(ypObject *t) { return yp_True; }

static ypObject *type_currenthash(
        ypObject *t, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // Since types are compared by identity, we can cache our hash
    *hash = ypObject_CACHED_HASH(t) = yp_HashPointer(t);
    return yp_None;
}

static ypObject *type_call(ypObject *t, ypObject **function, ypObject **self)
{
    *function = yp_incref(((ypTypeObject *)t)->tp_func_new);
    *self = yp_incref(t);
    return yp_None;
}

static ypObject *type_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_type);
    return yp_incref((ypObject *)ypObject_TYPE(argarray[1]));
}

yp_IMMORTAL_FUNCTION_static(type_func_new, type_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_object), NULL},
                {yp_CONST_REF(yp_s_slash), NULL}));

static ypCallableMethods ypType_as_callable = {
        type_call  // tp_call
};

static ypTypeObject ypType_Type = {
        yp_TYPE_HEAD_INIT,
        ypType_FLAG_IS_CALLABLE,  // tp_flags
        NULL,                     // tp_name

        // Object fundamentals
        yp_CONST_REF(type_func_new),  // tp_func_new
        MethodError_visitfunc,        // tp_dealloc
        NoRefs_traversefunc,          // tp_traverse
        NULL,                         // tp_str
        NULL,                         // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,   // tp_freeze
        type_frozen_copy,      // tp_unfrozen_copy
        type_frozen_copy,      // tp_frozen_copy
        type_frozen_deepcopy,  // tp_unfrozen_deepcopy
        type_frozen_deepcopy,  // tp_frozen_deepcopy
        MethodError_objproc,   // tp_invalidate

        // Boolean operations and comparisons
        type_bool,                   // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        NotImplemented_comparefunc,  // tp_eq
        NotImplemented_comparefunc,  // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        type_currenthash,     // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        &ypType_as_callable  // tp_as_callable
};

#pragma endregion type


/*************************************************************************************************
 * None
 *************************************************************************************************/
#pragma region None

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

static ypObject *nonetype_frozen_copy(ypObject *n) { return yp_None; }

static ypObject *nonetype_frozen_deepcopy(ypObject *n, visitfunc copy_visitor, void *copy_memo)
{
    return yp_None;  // _yp_deepcopy_memo_setitem is not needed here.
}

static ypObject *nonetype_bool(ypObject *n) { return yp_False; }

static ypObject *nonetype_currenthash(
        ypObject *n, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // Since we never contain mutable objects, we can cache our hash
    *hash = ypObject_CACHED_HASH(yp_None) = yp_HashPointer(yp_None);
    return yp_None;
}

static ypObject *nonetype_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 1, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_NoneType);
    return yp_None;
}

yp_IMMORTAL_FUNCTION_static(nonetype_func_new, nonetype_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_slash), NULL}));

static ypTypeObject ypNoneType_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(nonetype_func_new),  // tp_func_new
        MethodError_visitfunc,            // tp_dealloc
        NoRefs_traversefunc,              // tp_traverse
        NULL,                             // tp_str
        NULL,                             // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,       // tp_freeze
        nonetype_frozen_copy,      // tp_unfrozen_copy
        nonetype_frozen_copy,      // tp_frozen_copy
        nonetype_frozen_deepcopy,  // tp_unfrozen_deepcopy
        nonetype_frozen_deepcopy,  // tp_frozen_deepcopy
        MethodError_objproc,       // tp_invalidate

        // Boolean operations and comparisons
        nonetype_bool,               // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        NotImplemented_comparefunc,  // tp_eq
        NotImplemented_comparefunc,  // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        nonetype_currenthash,  // tp_currenthash
        MethodError_objproc,   // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

// No constructors for nonetypes; there is exactly one, immortal object

#pragma endregion None


/*************************************************************************************************
 * Bools
 *************************************************************************************************/
#pragma region bool

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

// TODO Could store in ypObject_CACHED_HASH instead
#define _ypBool_VALUE(b) (((ypBoolObject *)b)->value)

static ypObject *bool_frozen_copy(ypObject *b) { return b; }

static ypObject *bool_frozen_deepcopy(ypObject *b, visitfunc copy_visitor, void *copy_memo)
{
    return b;  // _yp_deepcopy_memo_setitem is not needed here.
}

static ypObject *bool_bool(ypObject *b) { return b; }

// Here be bool_lt, bool_le, bool_eq, bool_ne, bool_ge, bool_gt
#define _ypBool_RELATIVE_CMP_FUNCTION(name, operator)                                      \
    static ypObject *bool_##name(ypObject *b, ypObject *x)                                 \
    {                                                                                      \
        if (ypObject_TYPE_PAIR_CODE(x) != ypBool_CODE) return yp_ComparisonNotImplemented; \
        return ypBool_FROM_C(_ypBool_VALUE(b) operator _ypBool_VALUE(x));                  \
    }
_ypBool_RELATIVE_CMP_FUNCTION(lt, <);
_ypBool_RELATIVE_CMP_FUNCTION(le, <=);
_ypBool_RELATIVE_CMP_FUNCTION(eq, ==);
_ypBool_RELATIVE_CMP_FUNCTION(ne, !=);
_ypBool_RELATIVE_CMP_FUNCTION(ge, >=);
_ypBool_RELATIVE_CMP_FUNCTION(gt, >);

// XXX Adapted from Python's int_hash (now obsolete)
static ypObject *bool_currenthash(
        ypObject *b, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // This must remain consistent with the other numeric types. Since we never contain mutable
    // objects, we can cache our hash.
    *hash = ypObject_CACHED_HASH(b) = yp_HashInt(_ypBool_VALUE(b));  // either 0 or 1
    return yp_None;
}

static ypObject *bool_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_bool);
    return yp_bool(argarray[1]);
}

yp_IMMORTAL_FUNCTION_static(bool_func_new, bool_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_x), yp_CONST_REF(yp_False)},
                {yp_CONST_REF(yp_s_slash), NULL}));

static ypTypeObject ypBool_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(bool_func_new),  // tp_func_new
        MethodError_visitfunc,        // tp_dealloc
        NoRefs_traversefunc,          // tp_traverse
        NULL,                         // tp_str
        NULL,                         // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,   // tp_freeze
        bool_frozen_copy,      // tp_unfrozen_copy
        bool_frozen_copy,      // tp_frozen_copy
        bool_frozen_deepcopy,  // tp_unfrozen_deepcopy
        bool_frozen_deepcopy,  // tp_frozen_deepcopy
        MethodError_objproc,   // tp_invalidate

        // Boolean operations and comparisons
        bool_bool,  // tp_bool
        bool_lt,    // tp_lt
        bool_le,    // tp_le
        bool_eq,    // tp_eq
        bool_ne,    // tp_ne
        bool_ge,    // tp_ge
        bool_gt,    // tp_gt

        // Generic object operations
        bool_currenthash,     // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};


// No constructors for bools; there are exactly two objects, and they are immortal

#pragma endregion bool


/*************************************************************************************************
 * Integers
 *************************************************************************************************/
#pragma region int

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

#define ypInt_VALUE(i) (((ypIntObject *)i)->value)

// Arithmetic code depends on both int and float particulars being defined first
#define ypFloat_VALUE(f) (((ypFloatObject *)f)->value)

// Signatures of some specialized arithmetic functions
typedef yp_int_t (*arithLfunc)(yp_int_t, yp_int_t, ypObject **);
typedef yp_float_t (*arithLFfunc)(yp_float_t, yp_float_t, ypObject **);
typedef void (*iarithCfunc)(ypObject *, yp_int_t, ypObject **);
typedef void (*iarithCFfunc)(ypObject *, yp_float_t, ypObject **);
typedef yp_int_t (*unaryLfunc)(yp_int_t, ypObject **);
typedef yp_float_t (*unaryLFfunc)(yp_float_t, ypObject **);

// Bitwise operations on floats aren't supported, so these functions simply raise yp_TypeError
static void       yp_ilshiftCF(ypObject *x, yp_float_t y, ypObject **exc);
static void       yp_irshiftCF(ypObject *x, yp_float_t y, ypObject **exc);
static void       yp_iampCF(ypObject *x, yp_float_t y, ypObject **exc);
static void       yp_ixorCF(ypObject *x, yp_float_t y, ypObject **exc);
static void       yp_ibarCF(ypObject *x, yp_float_t y, ypObject **exc);
static yp_float_t yp_lshiftLF(yp_float_t x, yp_float_t y, ypObject **exc);
static yp_float_t yp_rshiftLF(yp_float_t x, yp_float_t y, ypObject **exc);
static yp_float_t yp_ampLF(yp_float_t x, yp_float_t y, ypObject **exc);
static yp_float_t yp_xorLF(yp_float_t x, yp_float_t y, ypObject **exc);
static yp_float_t yp_barLF(yp_float_t x, yp_float_t y, ypObject **exc);
static yp_float_t yp_invertLF(yp_float_t x, ypObject **exc);

// Public, immortal objects
yp_IMMORTAL_INT(yp_sys_maxint, yp_INT_T_MAX);
yp_IMMORTAL_INT(yp_sys_minint, yp_INT_T_MIN);

// XXX Adapted from Python's _PyLong_DigitValue
/* Table of digit values for 8-bit string -> integer conversion.
 * '0' maps to 0, ..., '9' maps to 9.
 * 'a' and 'A' map to 10, ..., 'z' and 'Z' map to 35.
 * All other indices map to 37.
 * Note that when converting a base B string, a yp_uint8_t c is a legitimate
 * base B digit iff _ypInt_digit_value[c] < B.
 */
// clang-format off
unsigned char _ypInt_digit_value[256] = {
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  37, 37, 37, 37, 37, 37,
    37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
    37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
};
// clang-format on

static yp_int_t _yp_mulL_posints(yp_int_t x, yp_int_t y);

// Check for the ypInt_IS_PREALLOC optimization before calling.
static ypObject *_ypInt_new(yp_int_t value, int type)
{
    ypObject *i;

    yp_ASSERT1(type != ypInt_CODE || !ypInt_IS_PREALLOC(value));

    i = ypMem_MALLOC_FIXED(ypIntObject, type);
    if (yp_isexceptionC(i)) return i;
    ypInt_VALUE(i) = value;
    yp_DEBUG("_ypInt_new: %p type %d value %" PRIint, i, type, value);
    return i;
}

// XXX Will fail if non-ascii bytes are passed in, so safe to call on latin-1 data
static ypObject *_ypInt_fromascii(
        ypObject *(*allocator)(yp_int_t), const yp_uint8_t *bytes, yp_int_t base)
{
    int      sign;
    yp_int_t result;
    yp_int_t digit;

    // Verify base
    if (base < 0 || base == 1 || base > 36) {
        return yp_ValueError;
    }

    // Skip leading whitespace
    while (1) {
        if (*bytes == '\0') return yp_ValueError;
        if (!yp_ISSPACE(*bytes)) break;
        bytes++;
    }

    // We're pointing to a non-whitespace character; see if there's a sign we should consume
    if (*bytes == '+') {
        sign = 1;
        bytes++;
    } else if (*bytes == '-') {
        sign = -1;
        bytes++;
    } else {
        sign = 1;
    }

    // We could be pointing to anything; determine if any prefix agrees with base
    if (*bytes == '0') {
        // We can safely consume this leading zero in all cases, as it cannot change the value
        bytes++;
        if (*bytes == '\0' || yp_ISSPACE(*bytes)) {
            // We've just parsed the string b"0", b"  0  ", etc; take a shortcut
            result = 0;
            goto endofdigits;
        } else if (*bytes == 'b' || *bytes == 'B') {
            if (base == 0) base = 2;
            if (base == 2) bytes++;  // consume this character only if the base agrees
        } else if (*bytes == 'o' || *bytes == 'O') {
            if (base == 0) base = 8;
            if (base == 8) bytes++;  // (ditto)
        } else if (*bytes == 'x' || *bytes == 'X') {
            if (base == 0) base = 16;
            if (base == 16) bytes++;  // (ditto)
        } else {
            // Leading zeroes are allowed if the value is zero, regardless of base (ie "00000");
            // once again, it's always safe to consume leading zeroes
            while (*bytes == '0') bytes++;
            if (*bytes == '\0' || yp_ISSPACE(*bytes)) {
                result = 0;
                goto endofdigits;
            }
            // We're now pointing to the first non-zero digit
            // For non-zero values, leading zeroes are not allowed when base is zero
            if (base == 0) return yp_ValueError;
        }
    }
    if (base == 0) {
        base = 10;  // a Python literal without a prefix is base 10
    }
    yp_ASSERT1(base >= 2 && base <= 36);

    // We could be pointing to anything; make sure there's at least one, valid digit.
    // _ypInt_digit_value[*bytes]>=base ensures we stop at the null-terminator, whitespace, and
    // invalid characters.
    digit = _ypInt_digit_value[*bytes];
    if (digit >= base) return yp_ValueError;
    bytes++;
    result = digit;
    while (1) {
        digit = _ypInt_digit_value[*bytes];
        if (digit >= base) goto endofdigits;
        bytes++;

        result = _yp_mulL_posints(result, base);
        if (result < 0) {
            // If adding digit would not change the value, then check for the yp_INT_T_MIN
            // case; otherwise, adding digit would definitely overflow
            if (digit == 0) goto checkforintmin;
            return yp_OverflowError;
        }

        result = yp_UINT_MATH(result, +, digit);
        if (result < 0) goto checkforintmin;
    }

endofdigits:
    // Ensure there's only whitespace left in the string, and return the new integer
    while (1) {
        if (*bytes == '\0') break;
        if (!yp_ISSPACE(*bytes)) return yp_ValueError;
        bytes++;
    }
    return allocator(sign * result);

checkforintmin:
    // If we overflowed to exactly yp_INT_T_MIN, and our result is supposed to be negative,
    // and there are no more digits, then we've decoded yp_INT_T_MIN
    if (result == yp_INT_T_MIN && sign < 0 && _ypInt_digit_value[*bytes] >= base) {
        sign = 1;  // result is already negative
        goto endofdigits;
    }
    return yp_OverflowError;
}


// Public Methods

static ypObject *int_dealloc(ypObject *i, void *memo)
{
    ypMem_FREE_FIXED(i);
    return yp_None;
}

static ypObject *int_unfrozen_copy(ypObject *i) { return yp_intstoreC(ypInt_VALUE(i)); }

static ypObject *int_frozen_copy(ypObject *i)
{
    // A shallow copy of an int to an int doesn't require an actual copy
    if (ypObject_TYPE_CODE(i) == ypInt_CODE) return yp_incref(i);
    return yp_intC(ypInt_VALUE(i));
}

// Check for the ypInt_IS_PREALLOC optimization before calling.
static ypObject *_ypInt_deepcopy(int type, ypObject *i, void *copy_memo)
{
    ypObject *i_copy = _ypInt_new(ypInt_VALUE(i), type);
    ypObject *result = _yp_deepcopy_memo_setitem(copy_memo, i, i_copy);
    if (yp_isexceptionC(result)) {
        yp_decref(i_copy);
        return result;
    }
    return i_copy;
}

static ypObject *int_unfrozen_deepcopy(ypObject *i, visitfunc copy_visitor, void *copy_memo)
{
    return _ypInt_deepcopy(ypIntStore_CODE, i, copy_memo);
}

static ypObject *int_frozen_deepcopy(ypObject *i, visitfunc copy_visitor, void *copy_memo)
{
    // We don't need to memoize the preallocated integers.
    if (ypInt_IS_PREALLOC(ypInt_VALUE(i))) {
        return ypInt_PREALLOC_REF(ypInt_VALUE(i));
    } else {
        return _ypInt_deepcopy(ypInt_CODE, i, copy_memo);
    }
}

static ypObject *int_bool(ypObject *i) { return ypBool_FROM_C(ypInt_VALUE(i)); }

// Here be int_lt, int_le, int_eq, int_ne, int_ge, int_gt
#define _ypInt_RELATIVE_CMP_FUNCTION(name, operator)                                      \
    static ypObject *int_##name(ypObject *i, ypObject *x)                                 \
    {                                                                                     \
        if (ypObject_TYPE_PAIR_CODE(x) != ypInt_CODE) return yp_ComparisonNotImplemented; \
        return ypBool_FROM_C(ypInt_VALUE(i) operator ypInt_VALUE(x));                     \
    }
_ypInt_RELATIVE_CMP_FUNCTION(lt, <);
_ypInt_RELATIVE_CMP_FUNCTION(le, <=);
_ypInt_RELATIVE_CMP_FUNCTION(eq, ==);
_ypInt_RELATIVE_CMP_FUNCTION(ne, !=);
_ypInt_RELATIVE_CMP_FUNCTION(ge, >=);
_ypInt_RELATIVE_CMP_FUNCTION(gt, >);

// XXX Adapted from Python's int_hash (now obsolete)
// TODO adapt from long_hash instead, which seems to handle this differently
// TODO Move this to a _yp_HashLong function?
static ypObject *int_currenthash(
        ypObject *i, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // This must remain consistent with the other numeric types
    *hash = yp_HashInt(ypInt_VALUE(i));

    // Since we never contain mutable objects, we can cache our hash
    // TODO Look into where we use ypObject_IS_MUTABLE for custom behaviour and consider
    // specializing the methods
    if (!ypObject_IS_MUTABLE(i)) ypObject_CACHED_HASH(i) = *hash;
    return yp_None;
}

static yp_int_t  yp_index_asintC(ypObject *x, ypObject **exc);
static ypObject *int_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 4, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_int);

    if (argarray[3] == yp_None) {
        return yp_int(argarray[1]);
    } else {
        // FIXME Move to a new yp_int_base (i.e. a version of yp_int_baseC that takes an object)
        ypObject *exc = yp_None;
        yp_int_t  base = yp_index_asintC(argarray[3], &exc);
        if (yp_isexceptionC(exc)) return exc;
        return yp_int_baseC(argarray[1], base);
    }
}

static ypObject *intstore_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 4, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_intstore);

    if (argarray[3] == yp_None) {
        return yp_intstore(argarray[1]);
    } else {
        // FIXME Move to a new yp_int_base (i.e. a version of yp_int_baseC that takes an object)
        ypObject *exc = yp_None;
        yp_int_t  base = yp_index_asintC(argarray[3], &exc);
        if (yp_isexceptionC(exc)) return exc;
        return yp_intstore_baseC(argarray[1], base);
    }
}

// FIXME In Python, None is not applicable for base; document the difference? Or use NoArg?
// FIXME Do we want a way to share the array itself between two functions?
#define _ypInt_FUNC_NEW_PARAMETERS                                                  \
    ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_x), ypInt_PREALLOC_REF(0)}, \
            {yp_CONST_REF(yp_s_slash), NULL}, {yp_CONST_REF(yp_s_base), yp_CONST_REF(yp_None)})
yp_IMMORTAL_FUNCTION_static(int_func_new, int_func_new_code, _ypInt_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(intstore_func_new, intstore_func_new_code, _ypInt_FUNC_NEW_PARAMETERS);

static ypTypeObject ypInt_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(int_func_new),  // tp_func_new
        int_dealloc,                 // tp_dealloc
        NoRefs_traversefunc,         // tp_traverse
        NULL,                        // tp_str
        NULL,                        // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,    // tp_freeze
        int_unfrozen_copy,      // tp_unfrozen_copy
        int_frozen_copy,        // tp_frozen_copy
        int_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        int_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,    // tp_invalidate

        // Boolean operations and comparisons
        int_bool,  // tp_bool
        int_lt,    // tp_lt
        int_le,    // tp_le
        int_eq,    // tp_eq
        int_ne,    // tp_ne
        int_ge,    // tp_ge
        int_gt,    // tp_gt

        // Generic object operations
        int_currenthash,      // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypTypeObject ypIntStore_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(intstore_func_new),  // tp_func_new
        int_dealloc,                      // tp_dealloc
        NoRefs_traversefunc,              // tp_traverse
        NULL,                             // tp_str
        NULL,                             // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,    // tp_freeze
        int_unfrozen_copy,      // tp_unfrozen_copy
        int_frozen_copy,        // tp_frozen_copy
        int_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        int_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,    // tp_invalidate

        // Boolean operations and comparisons
        int_bool,  // tp_bool
        int_lt,    // tp_lt
        int_le,    // tp_le
        int_eq,    // tp_eq
        int_ne,    // tp_ne
        int_ge,    // tp_ge
        int_gt,    // tp_gt

        // Generic object operations
        int_currenthash,      // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

// XXX Adapted from Python 2.7's int_add
yp_int_t yp_addL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    yp_int_t result = yp_UINT_MATH(x, +, y);
    if ((result ^ x) < 0 && (result ^ y) < 0) {
        return_yp_CEXC_ERR(0, exc, yp_OverflowError);
    }
    return result;
}

// XXX Adapted from Python 2.7's int_sub
yp_int_t yp_subL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    yp_int_t result = yp_UINT_MATH(x, -, y);
    if ((result ^ x) < 0 && (result ^ ~y) < 0) {
        return_yp_CEXC_ERR(0, exc, yp_OverflowError);
    }
    return result;
}

// Special case of yp_mulL when one of the operands is yp_INT_T_MIN
static yp_int_t _yp_mulL_minint(yp_int_t y, ypObject **exc)
{
    // When multiplying yp_INT_T_MIN, there are only two values of y that won't overflow
    if (y == 0) return 0;
    if (y == 1) return yp_INT_T_MIN;
    return_yp_CEXC_ERR(0, exc, yp_OverflowError);
}

// Special case of yp_mulL where both x and y are positive, or zero. Returns yp_INT_T_MIN if
// x*y overflows to _exactly_ that value; it will be up to the caller to determine if this is
// a valid result. All other overflows will return a negative number strictly larger than
// yp_INT_T_MIN.
// TODO It would be easier just to check "x > max/y"...how does the performance compare, though?
static yp_int_t _yp_mulL_posints(yp_int_t x, yp_int_t y)
{
    /* Adapted from http://www.fefe.de/intof.html (note this describes unsigned 64-bit numbers,
     * while we are effectively multiplying 63-bit numbers):
     * Split both numbers in two 32-bit parts. Let's write "<< 32" as "* shift" to make this easier
     * to read. Basically if we write the first number as a1 shift + a0 and the second number as
     * b1 shift + b0, then the product is
     *      (a1 shift + a0) * (b1 shift + b0) ==
     *          a1 b1 shift shift + a1 b0 shift + a0 b1 shift + a0 b0
     * The first term is shifted by 64 bits, so if both a1 and b1 are nonzero, we have an overflow
     * right there and can abort. For the second and third part we do the 32x32->64 multiplication
     * we just used to check for overflow for 32-bit numbers. Since we already know that either a1
     * or b1 is zero, we can simply calculate
     *      a=(uint64_t)(a1)*b0+(uint64_t)(a0)*b1;
     * At least one half of this term will be zero, so we can't have an overflow in the addition.
     * Then, if a > 0xffffffff, we have an overflow in the multiplication and can abort. If we got
     * this far, the result is
     *      (a << 32) + (uint64_t)(a0)*b0;
     * We still need to check for overflow in this addition, then we can return the result.
     */
    const yp_int_t num_bits_halved = yp_sizeof(yp_int_t) / 2 * 8;
    const yp_int_t bit_mask_halved = (yp_int_t)((1ull << num_bits_halved) - 1ull);
    yp_int_t       result_hi, result_lo;

    // Split x and y into high and low halves
    yp_int_t x_hi = yp_UINT_MATH(x, >>, num_bits_halved);
    yp_int_t x_lo = yp_UINT_MATH(x, &, bit_mask_halved);
    yp_int_t y_hi = yp_UINT_MATH(y, >>, num_bits_halved);
    yp_int_t y_lo = yp_UINT_MATH(y, &, bit_mask_halved);

    // Determine the intermediate value of the high-part of the result
    if (x_hi == 0) {
        // We can take a shortcut in the case of x and y being small values
        if (y_hi == 0) return yp_UINT_MATH(x_lo, *, y_lo);
        result_hi = yp_UINT_MATH(x_lo, *, y_hi);
    } else if (y_hi == 0) {
        result_hi = yp_UINT_MATH(x_hi, *, y_lo);
    } else {
        return -1;  // overflow
    }

    // Shift the intermediate high-part of the result, checking for overflow
    if (result_hi & ~bit_mask_halved) return -1;  // overflow
    result_hi = yp_UINT_MATH(result_hi, <<, num_bits_halved);
    if (result_hi < 0) {
        // If adding x_lo*y_lo would not change result_hi, then we may be dealing with
        // yp_INT_T_MIN, so return result_hi unchanged; otherwise, we would definitely overflow,
        // so return -1
        if (x_lo == 0 || y_lo == 0) return result_hi;
        return -1;
    }

    // Finally, add the low-part of the result to the high-part, check for overflow, and return;
    // the caller checks for overflow on the final addition
    result_lo = yp_UINT_MATH(x_lo, *, y_lo);
    if (result_lo < 0) return -1;  // overflow
    return yp_UINT_MATH(result_hi, +, result_lo);
}

// Python 2.7's int_mul uses the phrase "close enough", which scares me. I prefer the method
// described at http://www.fefe.de/intof.html, although it requires abs(x) and abs(y), meaning
// we need to handle yp_INT_T_MIN specially because we can't negate it.
yp_int_t yp_mulL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    yp_int_t result, sign = 1;

    if (x == yp_INT_T_MIN) return _yp_mulL_minint(y, exc);
    if (y == yp_INT_T_MIN) return _yp_mulL_minint(x, exc);
    if (x < 0) {
        sign = -sign;
        x = -x;
    }
    if (y < 0) {
        sign = -sign;
        y = -y;
    }

    // If we overflow to exactly yp_INT_T_MIN, and our result is supposed to be negative, then
    // we've calculated yp_INT_T_MIN; all other overflows are errors
    result = _yp_mulL_posints(x, y);
    if (result < 0) {
        if (result == yp_INT_T_MIN && sign < 0) return yp_INT_T_MIN;
        return_yp_CEXC_ERR(0, exc, yp_OverflowError);
    }
    return sign * result;
}

// XXX Operands are fist converted to float, then divided; result always a float
yp_float_t yp_truedivL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    ypObject  *subExc = yp_None;
    yp_float_t x_asfloat, y_asfloat;

    x_asfloat = yp_asfloatL(x, &subExc);
    if (yp_isexceptionC(subExc)) return_yp_CEXC_ERR(0.0, exc, subExc);
    y_asfloat = yp_asfloatL(y, &subExc);
    if (yp_isexceptionC(subExc)) return_yp_CEXC_ERR(0.0, exc, subExc);
    return yp_truedivLF(x_asfloat, y_asfloat, exc);
}

yp_int_t yp_floordivL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    yp_int_t div, mod;
    yp_divmodL(x, y, &div, &mod, exc);
    return div;
}

yp_int_t yp_modL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    yp_int_t div, mod;
    yp_divmodL(x, y, &div, &mod, exc);
    return mod;
}

// XXX Adapted from Python 2.7's i_divmod
void yp_divmodL(yp_int_t x, yp_int_t y, yp_int_t *_div, yp_int_t *_mod, ypObject **exc)
{
    yp_int_t xdivy, xmody;

    if (y == 0) {
        *exc = yp_ZeroDivisionError;
        goto error;
    }
    // (-sys.maxint-1)/-1 is the only overflow case.
    if (y == -1 && x == yp_INT_T_MIN) {
        *exc = yp_OverflowError;
        goto error;
    }

    xdivy = x / y;
    /* xdivy*y can overflow on platforms where x/y gives floor(x/y)
     * for x and y with differing signs. (This is unusual
     * behaviour, and C99 prohibits it, but it's allowed by C89;
     * for an example of overflow, take x = LONG_MIN, y = 5 or x =
     * LONG_MAX, y = -5.)  However, x - xdivy*y is always
     * representable as a long, since it lies strictly between
     * -abs(y) and abs(y). We add casts to avoid intermediate
     * overflow.
     */
    xmody = yp_UINT_MATH(x, -, yp_UINT_MATH(xdivy, *, y));
    /* If the signs of x and y differ, and the remainder is non-0,
     * C89 doesn't define whether xdivy is now the floor or the
     * ceiling of the infinitely precise quotient. We want the floor,
     * and we have it iff the remainder's sign matches y's.
     */
    if (xmody && ((y ^ xmody) < 0)) {  // i.e. and signs differ
        xmody += y;
        --xdivy;
        yp_ASSERT1(xmody && ((y ^ xmody) >= 0));
    }
    *_div = xdivy;
    *_mod = xmody;
    return;

error:
    *_div = 0;
    *_mod = 0;
}

yp_int_t yp_powL(yp_int_t x, yp_int_t y, ypObject **exc) { return yp_powL4(x, y, 0, exc); }

// XXX Adapted from Python 2.7's int_pow
yp_int_t yp_powL4(yp_int_t x, yp_int_t y, yp_int_t z, ypObject **exc)
{
    yp_int_t result, temp, prev;
    if (y < 0) {
        // XXX A negative exponent means a float should be returned, which we can't do here, so
        // this is handled at higher levels
        return_yp_CEXC_ERR(0, exc, yp_ValueError);
    }

    /*
     * XXX: The original exponentiation code stopped looping
     * when temp hit zero; this code will continue onwards
     * unnecessarily, but at least it won't cause any errors.
     * Hopefully the speed improvement from the fast exponentiation
     * will compensate for the slight inefficiency.
     * XXX: Better handling of overflows is desperately needed.
     */
    temp = x;
    result = 1;
    while (y > 0) {
        prev = result;  // Save value for overflow check
        if (y & 1) {
            /*
             * The (unsigned long) cast below ensures that the multiplication
             * is interpreted as an unsigned operation rather than a signed one
             * (C99 6.3.1.8p1), thus avoiding the perils of undefined behaviour
             * from signed arithmetic overflow (C99 6.5p5). See issue #12973.
             */
            result = yp_UINT_MATH(result, *, temp);
            if (temp == 0) break;  // Avoid result / 0
            if (result / temp != prev) {
                return_yp_CEXC_ERR(0, exc, yp_OverflowError);
            }
        }
        y >>= 1;  // Shift exponent down by 1 bit
        if (y == 0) break;
        prev = temp;
        temp = yp_UINT_MATH(temp, *, temp);  // Square the value of temp
        if (prev != 0 && temp / prev != prev) {
            return_yp_CEXC_ERR(0, exc, yp_OverflowError);
        }
        if (z) {
            // If we did a multiplication, perform a modulo
            result = result % z;
            temp = temp % z;
        }
    }
    if (z) {
        yp_int_t div;
        yp_divmodL(result, z, &div, &result, exc);
        return result;
    } else {
        return result;
    }
}

// Verify that this platform sign-extends on right-shifts (assumes compiler uses same rules
// as target processor, which it should)
yp_STATIC_ASSERT((-1LL >> 1) == -1LL, right_shift_sign_extends);

// XXX Adapted from Python 2.7's int_lshift
yp_int_t yp_lshiftL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    yp_int_t result;
    if (y < 0) return_yp_CEXC_ERR(0, exc, yp_ValueError);  // negative shift count
    if (x == 0) return 0;  // 0 can be shifted by 50 million bits for all we care
    if (y >= (yp_sizeof(yp_int_t) * 8)) return_yp_CEXC_ERR(0, exc, yp_OverflowError);
    result = x << y;
    if (x != (result >> y)) return_yp_CEXC_ERR(0, exc, yp_OverflowError);
    return result;
}

// XXX Adapted from Python 2.7's int_rshift
yp_int_t yp_rshiftL(yp_int_t x, yp_int_t y, ypObject **exc)
{
    if (y < 0) return_yp_CEXC_ERR(0, exc, yp_ValueError);  // negative shift count
    if (y >= (yp_sizeof(yp_int_t) * 8)) {
        return x < 0 ? -1 : 0;
    }
    return x >> y;
}

yp_int_t yp_ampL(yp_int_t x, yp_int_t y, ypObject **exc) { return x & y; }

yp_int_t yp_xorL(yp_int_t x, yp_int_t y, ypObject **exc) { return x ^ y; }

yp_int_t yp_barL(yp_int_t x, yp_int_t y, ypObject **exc) { return x | y; }

// XXX Adapted from Python 2.7's int_neg
yp_int_t yp_negL(yp_int_t x, ypObject **exc)
{
    if (x == yp_INT_T_MIN) return_yp_CEXC_ERR(0, exc, yp_OverflowError);
    return -x;
}

yp_int_t yp_posL(yp_int_t x, ypObject **exc) { return x; }

yp_int_t yp_absL(yp_int_t x, ypObject **exc)
{
    if (x < 0) return yp_negL(x, exc);
    return x;
}

yp_int_t yp_invertL(yp_int_t x, ypObject **exc) { return ~x; }

// XXX Overloading of add/etc currently not supported
static void iarithmeticC(
        ypObject *x, yp_int_t y, ypObject **exc, arithLfunc intop, arithLFfunc floatop)
{
    int       x_type = ypObject_TYPE_CODE(x);
    ypObject *subExc = yp_None;

    if (x_type == ypIntStore_CODE) {
        yp_int_t result = intop(ypInt_VALUE(x), y, &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        ypInt_VALUE(x) = result;

    } else if (x_type == ypFloatStore_CODE) {
        yp_float_t y_asfloat;
        yp_float_t result;
        y_asfloat = yp_asfloatL(y, &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        result = floatop(ypFloat_VALUE(x), y_asfloat, &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        ypFloat_VALUE(x) = result;

    } else {
        return_yp_EXC_BAD_TYPE(exc, x);
    }
}

static void iarithmetic(
        ypObject *x, ypObject *y, ypObject **exc, iarithCfunc intop, iarithCFfunc floatop)
{
    int y_pair = ypObject_TYPE_PAIR_CODE(y);

    if (y_pair == ypInt_CODE) {
        intop(x, ypInt_VALUE(y), exc);

    } else if (y_pair == ypFloat_CODE) {
        floatop(x, ypFloat_VALUE(y), exc);

    } else {
        return_yp_EXC_BAD_TYPE(exc, y);
    }
}

static ypObject *arithmetic_intop(yp_int_t x, yp_int_t y, arithLfunc intop, int result_mutable)
{
    ypObject *exc = yp_None;
    yp_int_t  result = intop(x, y, &exc);
    if (yp_isexceptionC(exc)) return exc;
    if (result_mutable) return yp_intstoreC(result);
    return yp_intC(result);
}
static ypObject *arithmetic_floatop(
        yp_float_t x, yp_float_t y, arithLFfunc floatop, int result_mutable)
{
    ypObject  *exc = yp_None;
    yp_float_t result = floatop(x, y, &exc);
    if (yp_isexceptionC(exc)) return exc;
    if (result_mutable) return yp_floatstoreCF(result);
    return yp_floatCF(result);
}
static ypObject *arithmetic(ypObject *x, ypObject *y, arithLfunc intop, arithLFfunc floatop)
{
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);
    int       y_pair = ypObject_TYPE_PAIR_CODE(y);
    int       result_mutable = ypObject_IS_MUTABLE(x);
    ypObject *exc = yp_None;

    // Coerce the numeric operands to a common type
    if (y_pair == ypInt_CODE) {
        if (x_pair == ypInt_CODE) {
            return arithmetic_intop(ypInt_VALUE(x), ypInt_VALUE(y), intop, result_mutable);
        } else if (x_pair == ypFloat_CODE) {
            yp_float_t y_asfloat = yp_asfloatC(y, &exc);
            if (yp_isexceptionC(exc)) return exc;
            return arithmetic_floatop(ypFloat_VALUE(x), y_asfloat, floatop, result_mutable);
        } else {
            return_yp_BAD_TYPE(x);
        }
    } else if (y_pair == ypFloat_CODE) {
        if (x_pair == ypInt_CODE) {
            yp_float_t x_asfloat = yp_asfloatC(x, &exc);
            if (yp_isexceptionC(exc)) return exc;
            return arithmetic_floatop(x_asfloat, ypFloat_VALUE(y), floatop, result_mutable);
        } else if (x_pair == ypFloat_CODE) {
            return arithmetic_floatop(ypFloat_VALUE(x), ypFloat_VALUE(y), floatop, result_mutable);
        } else {
            return_yp_BAD_TYPE(x);
        }
    } else {
        return_yp_BAD_TYPE(y);
    }
}

// Defined here are yp_iaddC (et al), yp_iadd (et al), and yp_add (et al)
#define _ypInt_PUBLIC_ARITH_FUNCTION(name)                      \
    void yp_i##name##C(ypObject *x, yp_int_t y, ypObject **exc) \
    {                                                           \
        iarithmeticC(x, y, exc, yp_##name##L, yp_##name##LF);   \
    }                                                           \
    void yp_i##name(ypObject *x, ypObject *y, ypObject **exc)   \
    {                                                           \
        iarithmetic(x, y, exc, yp_i##name##C, yp_i##name##CF);  \
    }                                                           \
    ypObject *yp_##name(ypObject *x, ypObject *y)               \
    {                                                           \
        return arithmetic(x, y, yp_##name##L, yp_##name##LF);   \
    }
_ypInt_PUBLIC_ARITH_FUNCTION(add);
_ypInt_PUBLIC_ARITH_FUNCTION(sub);
_ypInt_PUBLIC_ARITH_FUNCTION(mul);
// truediv implemented separately, as result is always a float
_ypInt_PUBLIC_ARITH_FUNCTION(floordiv);
_ypInt_PUBLIC_ARITH_FUNCTION(mod);
// TODO if pow has a negative exponent, the result must be a float
_ypInt_PUBLIC_ARITH_FUNCTION(pow);
_ypInt_PUBLIC_ARITH_FUNCTION(lshift);
_ypInt_PUBLIC_ARITH_FUNCTION(rshift);
_ypInt_PUBLIC_ARITH_FUNCTION(amp);
_ypInt_PUBLIC_ARITH_FUNCTION(xor);
_ypInt_PUBLIC_ARITH_FUNCTION(bar);

void yp_itruedivC(ypObject *x, yp_int_t y, ypObject **exc)
{
    int       x_type = ypObject_TYPE_CODE(x);
    ypObject *subExc = yp_None;

    // True division is always a float, so being in-place means x cannot be an int.
    if (x_type == ypFloatStore_CODE) {
        yp_float_t y_asfloat = yp_asfloatL(y, &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        yp_itruedivCF(x, y_asfloat, exc);

    } else {
        return_yp_EXC_BAD_TYPE(exc, x);
    }
}

void yp_itruediv(ypObject *x, ypObject *y, ypObject **exc)
{
    int y_pair = ypObject_TYPE_PAIR_CODE(y);

    if (y_pair == ypInt_CODE) {
        yp_itruedivC(x, ypInt_VALUE(y), exc);

    } else if (y_pair == ypFloat_CODE) {
        yp_itruedivCF(x, ypFloat_VALUE(y), exc);

    } else {
        return_yp_EXC_BAD_TYPE(exc, x);
    }
}

ypObject *yp_truediv(ypObject *x, ypObject *y)
{
    int        x_pair = ypObject_TYPE_PAIR_CODE(x);
    int        y_pair = ypObject_TYPE_PAIR_CODE(y);
    int        result_mutable = ypObject_IS_MUTABLE(x);
    ypObject  *exc = yp_None;
    yp_float_t result;

    // Coerce the numeric operands to a common type
    if (y_pair == ypInt_CODE) {
        if (x_pair == ypInt_CODE) {
            result = yp_truedivL(ypInt_VALUE(x), ypInt_VALUE(y), &exc);
        } else if (x_pair == ypFloat_CODE) {
            yp_float_t y_asfloat = yp_asfloatC(y, &exc);
            if (yp_isexceptionC(exc)) return exc;
            result = yp_truedivLF(ypFloat_VALUE(x), y_asfloat, &exc);
        } else {
            return_yp_BAD_TYPE(x);
        }
    } else if (y_pair == ypFloat_CODE) {
        if (x_pair == ypInt_CODE) {
            yp_float_t x_asfloat = yp_asfloatC(x, &exc);
            if (yp_isexceptionC(exc)) return exc;
            result = yp_truedivLF(x_asfloat, ypFloat_VALUE(y), &exc);
        } else if (x_pair == ypFloat_CODE) {
            result = yp_truedivLF(ypFloat_VALUE(x), ypFloat_VALUE(y), &exc);
        } else {
            return_yp_BAD_TYPE(x);
        }
    } else {
        return_yp_BAD_TYPE(y);
    }
    if (yp_isexceptionC(exc)) return exc;
    if (result_mutable) return yp_floatstoreCF(result);
    return yp_floatCF(result);
}

static ypObject *_yp_divmod_ints(
        yp_int_t x, yp_int_t y, ypObject **div, ypObject **mod, int result_mutable)
{
    ypObject *exc = yp_None;
    ypObject *(*allocator)(yp_int_t) = result_mutable ? yp_intstoreC : yp_intC;
    yp_int_t divC, modC;
    yp_divmodL(x, y, &divC, &modC, &exc);
    if (yp_isexceptionC(exc)) return exc;
    *div = allocator(divC);  // new ref
    if (yp_isexceptionC(*div)) return *div;
    *mod = allocator(modC);  // new ref
    if (yp_isexceptionC(*mod)) {
        yp_decref(*div);
        return *mod;
    }
    return yp_None;
}
static ypObject *_yp_divmod_floats(
        yp_float_t x, yp_float_t y, ypObject **div, ypObject **mod, int result_mutable)
{
    ypObject *exc = yp_None;
    ypObject *(*allocator)(yp_float_t) = result_mutable ? yp_floatstoreCF : yp_floatCF;
    yp_float_t divC, modC;
    yp_divmodLF(x, y, &divC, &modC, &exc);
    if (yp_isexceptionC(exc)) return exc;
    *div = allocator(divC);  // new ref
    if (yp_isexceptionC(*div)) return *div;
    *mod = allocator(modC);  // new ref
    if (yp_isexceptionC(*mod)) {
        yp_decref(*div);
        return *mod;
    }
    return yp_None;
}
static ypObject *_yp_divmod(ypObject *x, ypObject *y, ypObject **div, ypObject **mod)
{
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);
    int       y_pair = ypObject_TYPE_PAIR_CODE(y);
    int       result_mutable = ypObject_IS_MUTABLE(x);
    ypObject *exc = yp_None;

    // Coerce the numeric operands to a common type
    if (y_pair == ypInt_CODE) {
        if (x_pair == ypInt_CODE) {
            return _yp_divmod_ints(ypInt_VALUE(x), ypInt_VALUE(y), div, mod, result_mutable);
        } else if (x_pair == ypFloat_CODE) {
            yp_float_t y_asfloat = yp_asfloatC(y, &exc);
            if (yp_isexceptionC(exc)) return exc;
            return _yp_divmod_floats(ypFloat_VALUE(x), y_asfloat, div, mod, result_mutable);
        } else {
            return_yp_BAD_TYPE(x);
        }
    } else if (y_pair == ypFloat_CODE) {
        if (x_pair == ypInt_CODE) {
            yp_float_t x_asfloat = yp_asfloatC(x, &exc);
            if (yp_isexceptionC(exc)) return exc;
            return _yp_divmod_floats(x_asfloat, ypFloat_VALUE(y), div, mod, result_mutable);
        } else if (x_pair == ypFloat_CODE) {
            return _yp_divmod_floats(ypFloat_VALUE(x), ypFloat_VALUE(y), div, mod, result_mutable);
        } else {
            return_yp_BAD_TYPE(x);
        }
    } else {
        return_yp_BAD_TYPE(y);
    }
}
void yp_divmod(ypObject *x, ypObject *y, ypObject **div, ypObject **mod)
{
    // Ensure both div and mod are set to an exception on error
    ypObject *result = _yp_divmod(x, y, div, mod);
    if (yp_isexceptionC(result)) {
        *div = *mod = result;
    }
}

static void iunaryoperation(ypObject *x, ypObject **exc, unaryLfunc intop, unaryLFfunc floatop)
{
    int       x_type = ypObject_TYPE_CODE(x);
    ypObject *subExc = yp_None;

    if (x_type == ypIntStore_CODE) {
        yp_int_t result = intop(ypInt_VALUE(x), &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        ypInt_VALUE(x) = result;

    } else if (x_type == ypFloatStore_CODE) {
        yp_float_t result = floatop(ypFloat_VALUE(x), &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        ypFloat_VALUE(x) = result;

    } else {
        return_yp_EXC_BAD_TYPE(exc, x);
    }
}

static ypObject *unaryoperation(ypObject *x, unaryLfunc intop, unaryLFfunc floatop)
{
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);
    ypObject *exc = yp_None;

    if (x_pair == ypInt_CODE) {
        yp_int_t result = intop(ypInt_VALUE(x), &exc);
        if (yp_isexceptionC(exc)) return exc;
        if (ypObject_IS_MUTABLE(x)) {
            return yp_intstoreC(result);
        } else {
            if (result == ypInt_VALUE(x)) return yp_incref(x);
            return yp_intC(result);
        }

    } else if (x_pair == ypFloat_CODE) {
        yp_float_t result = floatop(ypFloat_VALUE(x), &exc);
        if (yp_isexceptionC(exc)) return exc;
        if (ypObject_IS_MUTABLE(x)) {
            return yp_floatstoreCF(result);
        } else {
            if (result == ypFloat_VALUE(x)) return yp_incref(x);
            return yp_floatCF(result);
        }

    } else {
        return_yp_BAD_TYPE(x);
    }
}

// Defined here are yp_ineg (et al), and yp_neg (et al)
#define _ypInt_PUBLIC_UNARY_FUNCTION(name)                    \
    void yp_i##name(ypObject *x, ypObject **exc)              \
    {                                                         \
        iunaryoperation(x, exc, yp_##name##L, yp_##name##LF); \
    }                                                         \
    ypObject *yp_##name(ypObject *x) { return unaryoperation(x, yp_##name##L, yp_##name##LF); }
_ypInt_PUBLIC_UNARY_FUNCTION(neg);
_ypInt_PUBLIC_UNARY_FUNCTION(pos);
_ypInt_PUBLIC_UNARY_FUNCTION(abs);
_ypInt_PUBLIC_UNARY_FUNCTION(invert);

// XXX Adapted from Python 2.7's bits_in_ulong
// clang-format off
static const yp_uint8_t _BitLengthTable[32] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
};
// clang-format on

yp_int_t yp_int_bit_lengthC(ypObject *x, ypObject **exc)
{
    yp_int_t x_abs;
    yp_int_t x_bits;

    if (ypObject_TYPE_PAIR_CODE(x) != ypInt_CODE) return_yp_CEXC_BAD_TYPE(0, exc, x);

    x_abs = ypInt_VALUE(x);
    if (x_abs < 0) {
        if (x_abs == yp_INT_T_MIN) return yp_sizeof(yp_int_t) * 8;
        x_abs = -x_abs;
    }

    x_bits = 0;
    while (x_abs >= 32) {
        x_bits += 6;
        x_abs >>= 6;
    }
    x_bits += _BitLengthTable[x_abs];
    return x_bits;
}


// Public constructors

// This pre-allocates an array of immortal ints for yp_intC to return
// clang-format off
static ypIntObject _ypInt_pre_allocated[] = {
    #define _ypInt_PREALLOC(value) \
        { yp_IMMORTAL_HEAD_INIT(ypInt_CODE, 0, ypObject_LEN_INVALID, NULL), (value) }
    _ypInt_PREALLOC(-5),
    _ypInt_PREALLOC(-4),
    _ypInt_PREALLOC(-3),
    _ypInt_PREALLOC(-2),
    _ypInt_PREALLOC(-1),

    // Allocates a range of 2, 4, etc starting at the given multiple of 2, 4, etc
    #define _ypInt_PREALLOC002(v) _ypInt_PREALLOC(   v), _ypInt_PREALLOC(   (v) | 0x01)
    #define _ypInt_PREALLOC004(v) _ypInt_PREALLOC002(v), _ypInt_PREALLOC002((v) | 0x02)
    #define _ypInt_PREALLOC008(v) _ypInt_PREALLOC004(v), _ypInt_PREALLOC004((v) | 0x04)
    #define _ypInt_PREALLOC016(v) _ypInt_PREALLOC008(v), _ypInt_PREALLOC008((v) | 0x08)
    #define _ypInt_PREALLOC032(v) _ypInt_PREALLOC016(v), _ypInt_PREALLOC016((v) | 0x10)
    #define _ypInt_PREALLOC064(v) _ypInt_PREALLOC032(v), _ypInt_PREALLOC032((v) | 0x20)
    #define _ypInt_PREALLOC128(v) _ypInt_PREALLOC064(v), _ypInt_PREALLOC064((v) | 0x40)
    #define _ypInt_PREALLOC256(v) _ypInt_PREALLOC128(v), _ypInt_PREALLOC128((v) | 0x80)
    _ypInt_PREALLOC256(0), // pre-allocates range(256)

    _ypInt_PREALLOC(256),
};
// clang-format on


ypObject *yp_intC(yp_int_t value)
{
    if (ypInt_IS_PREALLOC(value)) {
        return ypInt_PREALLOC_REF(value);
    } else {
        return _ypInt_new(value, ypInt_CODE);
    }
}

ypObject *yp_intstoreC(yp_int_t value) { return _ypInt_new(value, ypIntStore_CODE); }

// base is ignored if x is not a bytes or string
static ypObject *_ypInt(ypObject *(*allocator)(yp_int_t), ypObject *x, yp_int_t base)
{
    ypObject *exc = yp_None;
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypInt_CODE) {
        return allocator(ypInt_VALUE(x));
    } else if (x_pair == ypFloat_CODE) {
        yp_int_t x_asint = yp_asintLF(ypFloat_VALUE(x), &exc);
        if (yp_isexceptionC(exc)) return exc;
        return allocator(x_asint);
    } else if (x_pair == ypBool_CODE) {
        return allocator(ypBool_IS_TRUE_C(x));
    } else if (x_pair == ypBytes_CODE) {
        const yp_uint8_t *bytes;
        ypObject         *result = yp_asbytesCX(x, NULL, &bytes);
        if (yp_isexceptionC(result)) return yp_ValueError;  // contains null bytes
        return _ypInt_fromascii(allocator, bytes, base);
    } else if (x_pair == ypStr_CODE) {
        // TODO Implement decoding
        const yp_uint8_t *encoded;
        ypObject         *encoding;
        ypObject         *result = yp_asencodedCX(x, NULL, &encoded, &encoding);
        if (yp_isexceptionC(result)) return yp_ValueError;  // contains null bytes
        if (encoding != yp_s_latin_1) return yp_NotImplementedError;
        return _ypInt_fromascii(allocator, encoded, base);
    } else {
        return_yp_BAD_TYPE(x);
    }
}
ypObject *yp_int_baseC(ypObject *x, yp_int_t base)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);
    if (x_pair != ypBytes_CODE && x_pair != ypStr_CODE) return_yp_BAD_TYPE(x);
    return _ypInt(yp_intC, x, base);
}
ypObject *yp_intstore_baseC(ypObject *x, yp_int_t base)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);
    if (x_pair != ypBytes_CODE && x_pair != ypStr_CODE) return_yp_BAD_TYPE(x);
    return _ypInt(yp_intstoreC, x, base);
}
ypObject *yp_int(ypObject *x)
{
    if (ypObject_TYPE_CODE(x) == ypInt_CODE) return yp_incref(x);
    return _ypInt(yp_intC, x, 10);
}
ypObject *yp_intstore(ypObject *x) { return _ypInt(yp_intstoreC, x, 10); }

// Public conversion functions

yp_int_t yp_asintC(ypObject *x, ypObject **exc)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypInt_CODE) {
        return ypInt_VALUE(x);
    } else if (x_pair == ypFloat_CODE) {
        return yp_asintLF(ypFloat_VALUE(x), exc);
    } else if (x_pair == ypBool_CODE) {
        return ypBool_IS_TRUE_C(x);
    }
    return_yp_CEXC_BAD_TYPE(0, exc, x);
}

// TODO Make this a public API? (all the yp_index_* functions.)
// TODO Be consistent with Python's __index__.
// Losslessly converts the integral object to a C integer. Raises yp_TypeError if x is not an
// integral; in particular, a float will raise yp_TypeError.
static yp_int_t yp_index_asintC(ypObject *x, ypObject **exc)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypInt_CODE) {
        return ypInt_VALUE(x);
    } else if (x_pair == ypBool_CODE) {
        return ypBool_IS_TRUE_C(x);
    }
    return_yp_CEXC_BAD_TYPE(0, exc, x);
}

// Defines the conversion functions. Overflow checking is done by first truncating the value then
// seeing if it equals the stored value. Note that when yp_asintC raises an exception, it returns
// zero, which can be represented in every integer type, so we won't override any yp_TypeError
// errors.
// TODO review http://blog.reverberate.org/2012/12/testing-for-integer-overflow-in-c-and-c.html
#define _ypInt_PUBLIC_AS_C_FUNCTION(qual, index, name, mask)                              \
    qual yp_##name##_t yp##index##_as##name##C(ypObject *x, ypObject **exc)               \
    {                                                                                     \
        yp_int_t      asint = yp##index##_asintC(x, exc);                                 \
        yp_##name##_t retval = (yp_##name##_t)(asint & (mask));                           \
        if ((yp_int_t)retval != asint) return_yp_CEXC_ERR(retval, exc, yp_OverflowError); \
        return retval;                                                                    \
    }
#define _ypInt_PUBLIC_AS_C_FUNCTIONS(name, mask) \
    _ypInt_PUBLIC_AS_C_FUNCTION(, , name, mask)  \
            _ypInt_PUBLIC_AS_C_FUNCTION(static, _index, name, mask)
// clang-format off
_ypInt_PUBLIC_AS_C_FUNCTIONS(int8,   0xFF);
_ypInt_PUBLIC_AS_C_FUNCTIONS(uint8,  0xFFu);
_ypInt_PUBLIC_AS_C_FUNCTIONS(int16,  0xFFFF);
_ypInt_PUBLIC_AS_C_FUNCTIONS(uint16, 0xFFFFu);
_ypInt_PUBLIC_AS_C_FUNCTIONS(int32,  0xFFFFFFFF);
_ypInt_PUBLIC_AS_C_FUNCTIONS(uint32, 0xFFFFFFFFu);
#if defined(yp_ARCH_32_BIT)
yp_STATIC_ASSERT(yp_sizeof(yp_ssize_t) < yp_sizeof(yp_int_t), sizeof_yp_ssize_lt_yp_int);
_ypInt_PUBLIC_AS_C_FUNCTIONS(ssize,  (yp_ssize_t) 0xFFFFFFFF);
_ypInt_PUBLIC_AS_C_FUNCTIONS(hash,   (yp_hash_t) 0xFFFFFFFF);
#endif
// clang-format on

// The functions below assume/assert that yp_int_t is 64 bits
yp_STATIC_ASSERT(yp_sizeof(yp_int_t) == 8, sizeof_yp_int);

yp_int64_t yp_asint64C(ypObject *x, ypObject **exc) { return yp_asintC(x, exc); }

static yp_int64_t yp_index_asint64C(ypObject *x, ypObject **exc) { return yp_index_asintC(x, exc); }

yp_uint64_t yp_asuint64C(ypObject *x, ypObject **exc)
{
    yp_int_t asint = yp_asintC(x, exc);
    if (asint < 0) return_yp_CEXC_ERR((yp_uint64_t)asint, exc, yp_OverflowError);
    return (yp_uint64_t)asint;
}

static yp_uint64_t yp_index_asuint64C(ypObject *x, ypObject **exc)
{
    yp_int_t asint = yp_index_asintC(x, exc);
    if (asint < 0) return_yp_CEXC_ERR((yp_uint64_t)asint, exc, yp_OverflowError);
    return (yp_uint64_t)asint;
}

#if defined(yp_ARCH_64_BIT)
yp_STATIC_ASSERT(yp_sizeof(yp_ssize_t) == yp_sizeof(yp_int_t), sizeof_yp_ssize_eq_yp_int);

yp_ssize_t yp_asssizeC(ypObject *x, ypObject **exc) { return yp_asintC(x, exc); }

static yp_ssize_t yp_index_asssizeC(ypObject *x, ypObject **exc) { return yp_index_asintC(x, exc); }

yp_hash_t yp_ashashC(ypObject *x, ypObject **exc) { return yp_asintC(x, exc); }

static yp_hash_t yp_index_ashashC(ypObject *x, ypObject **exc) { return yp_index_asintC(x, exc); }
#endif

// TODO Make this a public API?
// Similar to yp_int and yp_asintC, but raises yp_ArithmeticError rather than truncating a float
// toward zero. An important property is that yp_int_exact(x) will equal x.
// XXX Inspired by Python's Decimal.to_integral_exact; yp_ArithmeticError may be replaced with a
// more-specific sub-exception in the future
// FIXME Decimal.to_integral_exact doesn't raise an error: it rounds! Python is tricky as to where
// it is lossy and not, making it hard to come up with a name that is consistent with Python.
// ypObject *yp_int_exact(ypObject *x);
static yp_int_t yp_asint_exactLF(yp_float_t x, ypObject **exc);
static yp_int_t yp_asint_exactC(ypObject *x, ypObject **exc)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypInt_CODE) {
        return ypInt_VALUE(x);
    } else if (x_pair == ypFloat_CODE) {
        return yp_asint_exactLF(ypFloat_VALUE(x), exc);
    } else if (x_pair == ypBool_CODE) {
        return ypBool_IS_TRUE_C(x);
    }
    return_yp_CEXC_BAD_TYPE(0, exc, x);
}

#pragma endregion int


/*************************************************************************************************
 * Floats
 *************************************************************************************************/
#pragma region float

// TODO Python has PyFPE_START_PROTECT; we should be doing the same

// ypFloatObject and ypFloat_VALUE are defined above for use by the int code

static ypObject *_ypFloat_new(yp_float_t value, int type)
{
    ypObject *f = ypMem_MALLOC_FIXED(ypFloatObject, type);
    if (yp_isexceptionC(f)) return f;
    ypFloat_VALUE(f) = value;
    return f;
}

static ypObject *float_dealloc(ypObject *f, void *memo)
{
    ypMem_FREE_FIXED(f);
    return yp_None;
}

static ypObject *float_unfrozen_copy(ypObject *f) { return yp_floatstoreCF(ypFloat_VALUE(f)); }

static ypObject *float_frozen_copy(ypObject *f)
{
    // A shallow copy of a float to a float doesn't require an actual copy
    if (ypObject_TYPE_CODE(f) == ypFloat_CODE) return yp_incref(f);
    return yp_floatCF(ypFloat_VALUE(f));
}

static ypObject *_ypFloat_deepcopy(int type, ypObject *f, void *copy_memo)
{
    ypObject *f_copy = _ypFloat_new(ypFloat_VALUE(f), type);
    ypObject *result = _yp_deepcopy_memo_setitem(copy_memo, f, f_copy);
    if (yp_isexceptionC(result)) {
        yp_decref(f_copy);
        return result;
    }
    return f_copy;
}

static ypObject *float_unfrozen_deepcopy(ypObject *f, visitfunc copy_visitor, void *copy_memo)
{
    return _ypFloat_deepcopy(ypFloatStore_CODE, f, copy_memo);
}

static ypObject *float_frozen_deepcopy(ypObject *f, visitfunc copy_visitor, void *copy_memo)
{
    return _ypFloat_deepcopy(ypFloat_CODE, f, copy_memo);
}

static ypObject *float_bool(ypObject *f) { return ypBool_FROM_C(ypFloat_VALUE(f) != 0.0); }

// Here be float_lt, float_le, float_eq, float_ne, float_ge, float_gt
#define _ypFloat_RELATIVE_CMP_FUNCTION(name, operator)             \
    static ypObject *float_##name(ypObject *f, ypObject *x)        \
    {                                                              \
        yp_float_t x_asfloat;                                      \
        int        x_pair = ypObject_TYPE_PAIR_CODE(x);            \
                                                                   \
        if (x_pair == ypFloat_CODE) {                              \
            x_asfloat = ypFloat_VALUE(x);                          \
        } else if (x_pair == ypInt_CODE) {                         \
            ypObject *exc = yp_None;                               \
            x_asfloat = yp_asfloatL(ypInt_VALUE(x), &exc);         \
            if (yp_isexceptionC(exc)) return exc;                  \
        } else {                                                   \
            return yp_ComparisonNotImplemented;                    \
        }                                                          \
        return ypBool_FROM_C(ypFloat_VALUE(f) operator x_asfloat); \
    }
_ypFloat_RELATIVE_CMP_FUNCTION(lt, <);
_ypFloat_RELATIVE_CMP_FUNCTION(le, <=);
_ypFloat_RELATIVE_CMP_FUNCTION(eq, ==);
_ypFloat_RELATIVE_CMP_FUNCTION(ne, !=);
_ypFloat_RELATIVE_CMP_FUNCTION(ge, >=);
_ypFloat_RELATIVE_CMP_FUNCTION(gt, >);

static ypObject *float_currenthash(
        ypObject *f, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // This must remain consistent with the other numeric types
    *hash = yp_HashDouble(f, ypFloat_VALUE(f));

    // Since we never contain mutable objects, we can cache our hash
    if (!ypObject_IS_MUTABLE(f)) ypObject_CACHED_HASH(f) = *hash;
    return yp_None;
}

static ypObject *float_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_float);
    return yp_float(argarray[1]);
}

static ypObject *floatstore_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_floatstore);
    return yp_floatstore(argarray[1]);
}

#define _ypFloat_FUNC_NEW_PARAMETERS                                                \
    ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_x), ypInt_PREALLOC_REF(0)}, \
            {yp_CONST_REF(yp_s_slash), NULL})
yp_IMMORTAL_FUNCTION_static(float_func_new, float_func_new_code, _ypFloat_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(
        floatstore_func_new, floatstore_func_new_code, _ypFloat_FUNC_NEW_PARAMETERS);

static ypTypeObject ypFloat_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(float_func_new),  // tp_func_new
        float_dealloc,                 // tp_dealloc
        NoRefs_traversefunc,           // tp_traverse
        NULL,                          // tp_str
        NULL,                          // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,      // tp_freeze
        float_unfrozen_copy,      // tp_unfrozen_copy
        float_frozen_copy,        // tp_frozen_copy
        float_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        float_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,      // tp_invalidate

        // Boolean operations and comparisons
        float_bool,  // tp_bool
        float_lt,    // tp_lt
        float_le,    // tp_le
        float_eq,    // tp_eq
        float_ne,    // tp_ne
        float_ge,    // tp_ge
        float_gt,    // tp_gt

        // Generic object operations
        float_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypTypeObject ypFloatStore_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(floatstore_func_new),  // tp_func_new
        float_dealloc,                      // tp_dealloc
        NoRefs_traversefunc,                // tp_traverse
        NULL,                               // tp_str
        NULL,                               // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,      // tp_freeze
        float_unfrozen_copy,      // tp_unfrozen_copy
        float_frozen_copy,        // tp_frozen_copy
        float_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        float_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,      // tp_invalidate

        // Boolean operations and comparisons
        float_bool,  // tp_bool
        float_lt,    // tp_lt
        float_le,    // tp_le
        float_eq,    // tp_eq
        float_ne,    // tp_ne
        float_ge,    // tp_ge
        float_gt,    // tp_gt

        // Generic object operations
        float_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

yp_float_t yp_addLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return x + y;  // TODO overflow check
}

yp_float_t yp_subLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return x - y;  // TODO overflow check
}

yp_float_t yp_mulLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return x * y;  // TODO overflow check
}

yp_float_t yp_truedivLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    if (y == 0.0) return_yp_CEXC_ERR(0.0, exc, yp_ZeroDivisionError);
    return x / y;  // TODO overflow check
}

// XXX Although the return value is a whole number, in Python if one of the operands are floats,
// the result is a float
yp_float_t yp_floordivLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    if (y == 0.0) return_yp_CEXC_ERR(0.0, exc, yp_ZeroDivisionError);
    return_yp_CEXC_ERR(0, exc, yp_NotImplementedError);
}

yp_float_t yp_modLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_NotImplementedError);
}

void yp_divmodLF(yp_float_t x, yp_float_t y, yp_float_t *_div, yp_float_t *_mod, ypObject **exc)
{
    *_div = 0;
    *_mod = 0;
    *exc = y == 0.0 ? yp_ZeroDivisionError : yp_NotImplementedError;
}

yp_float_t yp_powLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return pow(x, y);  // TODO overflow check
}

yp_float_t yp_negLF(yp_float_t x, ypObject **exc)
{
    return -x;  // TODO overflow check
}

yp_float_t yp_posLF(yp_float_t x, ypObject **exc) { return x; }

yp_float_t yp_absLF(yp_float_t x, ypObject **exc)
{
    if (x < 0.0) return yp_negLF(x, exc);
    return x;
}

static void iarithmeticCF(ypObject *x, yp_float_t y, ypObject **exc, arithLFfunc floatop)
{
    int        x_type = ypObject_TYPE_CODE(x);
    ypObject  *subExc = yp_None;
    yp_float_t result;

    // Arithmetic with floats is always a float. We are in-place, so x cannot be an int.
    if (x_type == ypFloatStore_CODE) {
        result = floatop(ypFloat_VALUE(x), y, &subExc);
        if (yp_isexceptionC(subExc)) return_yp_EXC_ERR(exc, subExc);
        ypFloat_VALUE(x) = result;

    } else {
        return_yp_EXC_BAD_TYPE(exc, x);
    }
}

// Defined here are yp_iaddCF (et al)
#define _ypFloat_PUBLIC_ARITH_FUNCTION(name)                       \
    void yp_i##name##CF(ypObject *x, yp_float_t y, ypObject **exc) \
    {                                                              \
        iarithmeticCF(x, y, exc, yp_##name##LF);                   \
    }
_ypFloat_PUBLIC_ARITH_FUNCTION(add);
_ypFloat_PUBLIC_ARITH_FUNCTION(sub);
_ypFloat_PUBLIC_ARITH_FUNCTION(mul);
_ypFloat_PUBLIC_ARITH_FUNCTION(truediv);
_ypFloat_PUBLIC_ARITH_FUNCTION(floordiv);
_ypFloat_PUBLIC_ARITH_FUNCTION(mod);
_ypFloat_PUBLIC_ARITH_FUNCTION(pow);

// Binary operations are not applicable on floats
static void yp_ilshiftCF(ypObject *x, yp_float_t y, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_TypeError);
}
static void yp_irshiftCF(ypObject *x, yp_float_t y, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_TypeError);
}
static void yp_iampCF(ypObject *x, yp_float_t y, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_TypeError);
}
static void yp_ixorCF(ypObject *x, yp_float_t y, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_TypeError);
}
static void yp_ibarCF(ypObject *x, yp_float_t y, ypObject **exc)
{
    return_yp_EXC_ERR(exc, yp_TypeError);
}
static yp_float_t yp_lshiftLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_TypeError);
}
static yp_float_t yp_rshiftLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_TypeError);
}
static yp_float_t yp_ampLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_TypeError);
}
static yp_float_t yp_xorLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_TypeError);
}
static yp_float_t yp_barLF(yp_float_t x, yp_float_t y, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_TypeError);
}
static yp_float_t yp_invertLF(yp_float_t x, ypObject **exc)
{
    return_yp_CEXC_ERR(0.0, exc, yp_TypeError);
}


// Public constructors

ypObject *yp_floatCF(yp_float_t value) { return _ypFloat_new(value, ypFloat_CODE); }

ypObject *yp_floatstoreCF(yp_float_t value) { return _ypFloat_new(value, ypFloatStore_CODE); }

static ypObject *_ypFloat(ypObject *(*allocator)(yp_float_t), ypObject *x)
{
    ypObject  *exc = yp_None;
    yp_float_t x_asfloat = yp_asfloatC(x, &exc);
    if (yp_isexceptionC(exc)) return exc;
    return allocator(x_asfloat);
}
ypObject *yp_float(ypObject *x)
{
    if (ypObject_TYPE_CODE(x) == ypFloat_CODE) return yp_incref(x);
    return _ypFloat(yp_floatCF, x);
}
ypObject *yp_floatstore(ypObject *x) { return _ypFloat(yp_floatstoreCF, x); }

// Public conversion functions

yp_float_t yp_asfloatC(ypObject *x, ypObject **exc)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypInt_CODE) {
        return yp_asfloatL(ypInt_VALUE(x), exc);
    } else if (x_pair == ypFloat_CODE) {
        return ypFloat_VALUE(x);
    } else if (x_pair == ypBool_CODE) {
        return yp_asfloatL(ypBool_IS_TRUE_C(x), exc);
    }
    return_yp_CEXC_BAD_TYPE(0.0, exc, x);
}

yp_float_t yp_asfloatL(yp_int_t x, ypObject **exc)
{
    // TODO Implement this as Python does
    return (yp_float_t)x;
}

// XXX Adapted from Python's float_trunc
yp_int_t yp_asintLF(yp_float_t x, ypObject **exc)
{
    yp_float_t wholepart;  // integral portion of x, rounded toward 0

    (void)modf(x, &wholepart);
    /* Try to get out cheap if this fits in a Python int. The attempt
     * to cast to long must be protected, as C doesn't define what
     * happens if the double is too big to fit in a long. Some rare
     * systems raise an exception then (RISCOS was mentioned as one,
     * and someone using a non-default option on Sun also bumped into
     * that). Note that checking for >= and <= LONG_{MIN,MAX} would
     * still be vulnerable:  if a long has more bits of precision than
     * a double, casting MIN/MAX to double may yield an approximation,
     * and if that's rounded up, then, e.g., wholepart=LONG_MAX+1 would
     * yield true from the C expression wholepart<=LONG_MAX, despite
     * that wholepart is actually greater than LONG_MAX.
     */
    if (yp_INT_T_MIN < wholepart && wholepart < yp_INT_T_MAX) {
        return (yp_int_t)wholepart;
    }
    return_yp_CEXC_ERR(0, exc, yp_OverflowError);
}

// TODO make this public?
// XXX Adapted from Python's float_trunc
static yp_int_t yp_asint_exactLF(yp_float_t x, ypObject **exc)
{
    yp_float_t wholepart;  // integral portion of x, rounded toward 0

    if (modf(x, &wholepart) != 0.0) {  // returns fractional portion of x
        return_yp_CEXC_ERR(0, exc, yp_ArithmeticError);
    }
    if (yp_INT_T_MIN < wholepart && wholepart < yp_INT_T_MAX) {
        return (yp_int_t)wholepart;
    }
    return_yp_CEXC_ERR(0, exc, yp_OverflowError);
}

#pragma endregion float


/*************************************************************************************************
 * Common sequence functions
 *************************************************************************************************/
#pragma region sequence

// Using the given length, adjusts negative indices to positive. Returns false if the adjusted
// index is out-of-bounds, else true.
static int ypSequence_AdjustIndexC(yp_ssize_t length, yp_ssize_t *i)
{
    if (*i < 0) *i += length;
    if (*i < 0 || *i >= length) return FALSE;
    return TRUE;
}

// Asserts that the given indices have been adjusted by ypSlice_AdjustIndicesC. Used by internal
// methods that require adjusted indices. If stop is unknown use yp_SLICE_DEFAULT.
// XXX As we do not have access to the original length, we can't assert that start<=len, etc.
#if yp_ASSERT_ENABLED
#define ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength)                         \
    do {                                                                                        \
        yp_ASSERT((step) != 0 && (step) >= -yp_SSIZE_T_MAX, "invalid step %" PRIssize, (step)); \
        yp_ASSERT((slicelength) >= 0, "invalid slicelength %" PRIssize, (slicelength));         \
        yp_ASSERT((start) >= ((step) < 0 ? -1 : 0), "invalid start %" PRIssize, (start));       \
        if ((stop) != yp_SLICE_DEFAULT) {                                                       \
            yp_ssize_t expected_slicelength;                                                    \
            yp_ASSERT((stop) >= ((step) < 0 ? -1 : 0), "invalid stop %" PRIssize, (stop));      \
            if ((step) < 0) {                                                                   \
                expected_slicelength =                                                          \
                        ((stop) >= (start)) ? 0 : ((stop) - (start) + 1) / (step) + 1;          \
            } else {                                                                            \
                expected_slicelength =                                                          \
                        ((start) >= (stop)) ? 0 : ((stop) - (start)-1) / (step) + 1;            \
            }                                                                                   \
            yp_ASSERT((slicelength) == expected_slicelength,                                    \
                    "invalid slicelength %" PRIssize " (%" PRIssize ":%" PRIssize ":%" PRIssize \
                    ")",                                                                        \
                    (slicelength), (start), (stop), (step));                                    \
        }                                                                                       \
    } while (0)
#else
#define ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength)
#endif

// Using the given length, in-place converts the given start/stop/step values to valid indices, and
// also calculates the length of the slice. Returns yp_ValueError if *step is zero. Recall there are
// no out-of-bounds errors with slices.
// XXX yp_SLICE_DEFAULT is yp_SSIZE_T_MIN, which hopefully nobody will try to use as a valid index.
// yp_SLICE_LAST is yp_SSIZE_T_MAX, which is simply a very large number that is handled the same
// as any value that's greater than length.
// XXX Adapted from PySlice_GetIndicesEx
static ypObject *ypSlice_AdjustIndicesC(yp_ssize_t length, yp_ssize_t *start, yp_ssize_t *stop,
        yp_ssize_t *step, yp_ssize_t *slicelength)
{
    if (*step == 0) return yp_ValueError;
    if (*step < -yp_SSIZE_T_MAX) return yp_SystemLimitationError;  // Ensure *step can be negated.

    if (length < 1) {
        *start = *stop = *slicelength = 0;
        return yp_None;
    }

    // Adjust start
    if (*start == yp_SLICE_DEFAULT) {
        *start = (*step < 0) ? length - 1 : 0;
    } else {
        if (*start < 0) *start += length;
        if (*start < 0) *start = (*step < 0) ? -1 : 0;
        if (*start >= length) *start = (*step < 0) ? length - 1 : length;
    }

    // Adjust stop
    if (*stop == yp_SLICE_DEFAULT) {
        *stop = (*step < 0) ? -1 : length;
    } else {
        if (*stop < 0) *stop += length;
        if (*stop < 0) *stop = (*step < 0) ? -1 : 0;
        if (*stop >= length) *stop = (*step < 0) ? length - 1 : length;
    }

    // Calculate slicelength
    if (*step < 0) {
        if (*stop >= *start) {
            *slicelength = 0;
        } else {
            *slicelength = (*stop - *start + 1) / (*step) + 1;
        }
    } else {
        if (*start >= *stop) {
            *slicelength = 0;
        } else {
            *slicelength = (*stop - *start - 1) / (*step) + 1;
        }
    }

    return yp_None;
}

// Using the given _adjusted_ values, in-place converts the given start/stop/step values to the
// inverse slice, where *step=-(*step). slicelength must be >0 (slicelength==0 is a no-op).
// XXX Adapted from Python's list_ass_subscript
// XXX Check for the empty slice case first
static void ypSlice_InvertIndicesC(
        yp_ssize_t *start, yp_ssize_t *stop, yp_ssize_t *step, yp_ssize_t slicelength)
{
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    ypSlice_ASSERT_ADJUSTED_INDICES(*start, *stop, *step, slicelength);

    if (*step < 0) {
        // This comes direct from list_ass_subscript
        *stop = *start + 1;
        *start = *stop + (*step) * (slicelength - 1) - 1;
    } else {
        // This part I've inferred
        *stop = *start - 1;
        *start = *stop + (*step) * (slicelength - 1) + 1;
    }
    *step = -(*step);
}

// Returns the index of the i'th item in the slice with the given adjusted start/step values. i
// must be in range(slicelength).
#define ypSlice_INDEX(start, step, i) ((start) + (i) * (step))

// Used by tp_repeat et al to perform the necessary memcpy's. data must be allocated to hold
// factor*n_size objects, the bytes to repeat must be in the first n_size bytes of data, and the
// rest of data must not contain any references (they will be overwritten). Cannot fail.
// XXX Handle the "empty" case (factor<1 or n_size<1) before calling this function
static void _ypSequence_repeat_memcpy(void *_data, yp_ssize_t factor, yp_ssize_t n_size)
{
    yp_uint8_t *data = (yp_uint8_t *)_data;
    yp_ssize_t  copied;  // the number of times [:n_size] has been repeated (starts at 1, of course)
    yp_ASSERT(factor > 0 && n_size > 0, "factor and n_size must both be strictly positive");
    yp_ASSERT(factor <= yp_SSIZE_T_MAX / n_size, "factor*n_size too large");
    for (copied = 1; copied * 2 < factor; copied *= 2) {
        yp_memcpy(data + (n_size * copied), data + 0, n_size * copied);
    }
    yp_memcpy(data + (n_size * copied), data + 0,
            n_size * (factor - copied));  // no-op if factor==copied
}

// Used to remove elements from an array containing length elements, each of elemsize bytes. start,
// stop, step, and slicelength must be the _adjusted_ values from ypSlice_AdjustIndicesC. Any
// references in the removed elements must have already been discarded.
// XXX Check for the empty slice and total slice cases first
static void _ypSlice_delslice_memmove(void *array, yp_ssize_t length, yp_ssize_t elemsize,
        yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, yp_ssize_t slicelength)
{
    yp_uint8_t *bytes = array;

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength);
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    yp_ASSERT(slicelength < length, "missed a 'total slice' optimization");

    if (step < 0) ypSlice_InvertIndicesC(&start, &stop, &step, slicelength);

    if (step == 1) {
        // One contiguous section
        yp_memmove(
                bytes + (start * elemsize), bytes + (stop * elemsize), (length - stop) * elemsize);
    } else {
        yp_ssize_t  remaining = slicelength;
        yp_uint8_t *chunk_dst = bytes + (start * elemsize);
        yp_uint8_t *chunk_src = chunk_dst + elemsize;
        yp_ssize_t  chunk_len = (step - 1) * elemsize;
        while (remaining > 1) {
            yp_memmove(chunk_dst, chunk_src, chunk_len);
            chunk_dst += chunk_len;
            chunk_src += chunk_len + elemsize;
            remaining -= 1;
        }
        // The last chunk is likely truncated, and not a full step*elemsize in size
        chunk_len = (bytes + (length * elemsize)) - chunk_src;
        yp_memmove(chunk_dst, chunk_src, chunk_len);
    }
}

static ypObject *_ypSequence_getdefault(ypObject *x, ypObject *key, ypObject *defval)
{
    ypObject     *exc = yp_None;
    ypTypeObject *type = ypObject_TYPE(x);
    yp_ssize_t    index = yp_index_asssizeC(key, &exc);
    if (yp_isexceptionC(exc)) return exc;
    return type->tp_as_sequence->tp_getindex(x, index, defval);
}

static ypObject *_ypSequence_setitem(ypObject *x, ypObject *key, ypObject *value)
{
    ypObject     *exc = yp_None;
    ypTypeObject *type = ypObject_TYPE(x);
    yp_ssize_t    index = yp_index_asssizeC(key, &exc);
    if (yp_isexceptionC(exc)) return exc;
    return type->tp_as_sequence->tp_setindex(x, index, value);
}

static ypObject *_ypSequence_delitem(ypObject *x, ypObject *key)
{
    ypObject     *exc = yp_None;
    ypTypeObject *type = ypObject_TYPE(x);
    yp_ssize_t    index = yp_index_asssizeC(key, &exc);
    if (yp_isexceptionC(exc)) return exc;
    return type->tp_as_sequence->tp_delindex(x, index);
}

#pragma endregion sequence


/*************************************************************************************************
 * In-development API for Codec registry and base classes
 *************************************************************************************************/
#pragma region codecs

// XXX Patterned after the codecs module in Python
// TODO This will eventually be exposed in nohtyP.h; review and improve before this happens
// TODO In general, everything below is geared for Unicode; make it flexible enough for any type of
// codec, as Python does

typedef struct _yp_codecs_error_handler_params_t {
    yp_ssize_t sizeof_struct;  // Set to yp_sizeof(yp_codecs_error_handler_params_t) on allocation

    // Details of the error. All references, va_lists, and pointers are borrowed and should not be
    // replaced.
    // TODO Python's error handlers are flexible enough to take any exception object containing
    // any data. Make sure this, while optimized for Unicode, can do the same.
    ypObject *exc;       // yp_UnicodeEncodeError, *DecodeError, or *TranslateError
    ypObject *encoding;  // The unaliased name of the encoding that raised the error
    struct {             // A to-be-formatted string+args describing the specific codec error
        const yp_uint8_t *format;  // ASCII-encoded printf-style format string
        va_list           args;    // printf-style format arguments
    } reason;
    struct {               // The object/data the codec was attempting to encode/decode
        ypObject *object;  // The source object; NULL if working strictly from raw data
                           // XXX For example, yp_str_frombytesC4 will set this to NULL
        // TODO Function to return ptr or, if it's NULL, a pointer to the data from within object
        struct {              // The source data; ptr and/or type may be NULL iff object isn't
            ypObject *type;   // An indication of the type of data pointed to:
                              //  - str-like: set to encoding name as per yp_asencodedCX
                              //  - bytes-like: set to yp_t_bytes
                              //  - other codecs may set this to other objects
            const void *ptr;  // Pointer to the source data
            yp_ssize_t  len;  // Length of source data
        } data;
    } source;
    yp_ssize_t start;  // The first index of invalid data in source
    yp_ssize_t end;    // The index after the last invalid data in source
} yp_codecs_error_handler_params_t;

// Error handler. Either raise params->exc, or a different error, via *replacement. Otherwise,
// set *replacement to the object that replaces the bad data, and *new_position to the index at
// which to restart encoding/decoding.
// XXX It's possible for *new_position to be less than or even greater than params->end on output
// TODO returning (replacement, new_position) is strictly a Unicode thing: generalize
typedef void (*yp_codecs_error_handler_func_t)(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position);

// Registers the alias as an alternate name for the encoding and returns the immortal yp_None. Both
// alias and encoding are normalized before being registered (lowercased, ' ' and '_' converted to
// '-'). Attempting to register "utf-8" as an alias will raise yp_ValueError; however, there is no
// other protection against using encoding names as aliases. Returns an exception on error.
static ypObject *yp_codecs_register_alias(ypObject *alias, ypObject *encoding);

// Returns a new reference to the normalized, unaliased encoding name.
// TODO I believe this alias stuff is actually part of the encodings module; consider how pedantic
// we want to be before releasing this API
static ypObject *yp_codecs_lookup_alias(ypObject *alias);

// Registers error_handler under the given name, and returns the immortal yp_None. This handler
// will be called on codec errors when name is specified as the errors parameter (see yp_encode3
// for an example). Returns an exception on error.
// TODO Make error_handler a function object.
static ypObject *yp_codecs_register_error(
        ypObject *name, yp_codecs_error_handler_func_t error_handler);

// Returns the error_handler associated with the given name. Raises yp_LookupError if the handler
// cannot be found. Sets *exc and returns the built-in "strict" handler on error (which may not
// be the _registered_ "strict" handler.)
// TODO We protect "utf-8" from being aliased; should we protect "strict"/etc?
static yp_codecs_error_handler_func_t yp_codecs_lookup_errorE(ypObject *name, ypObject **exc);

#pragma endregion codecs


/*************************************************************************************************
 * String manipulation library (for bytes and str)
 *************************************************************************************************/
#pragma region strings

// The prefix ypStringLib_* is used for bytes and str, while ypStr_* is strictly the str type

// All ypStringLib-supporting types must:
//  - Set ob_type_flags to one of the ypStringLib_ENC_* values as appropriate
//  - Always use ob_data, ob_len, and ob_alloclen as the pointer, length, and allocated length
//      - ob_data[ob_len] must be the null character appropriate for ob_data's encoding
//      - ob_len < ob_alloclen <= ypStringLib_ALLOCLEN_MAX (except for immortals where alloclen<0)

#define ypStringLib_ENC_CODE(s) (((ypObject *)(s))->ob_type_flags)
#define ypStringLib_ENC(s) (&(ypStringLib_encs[ypStringLib_ENC_CODE(s)]))
#define ypStringLib_DATA(s) (((ypObject *)s)->ob_data)
#define ypStringLib_LEN ypObject_CACHED_LEN
#define ypStringLib_SET_LEN ypObject_SET_CACHED_LEN
#define ypStringLib_ALLOCLEN ypObject_ALLOCLEN
#define ypStringLib_SET_ALLOCLEN ypObject_SET_ALLOCLEN
#define ypStringLib_INLINE_DATA(s) (((ypStringLibObject *)s)->ob_inline_data)

// The maximum possible alloclen and length of any string object. While Latin-1 could technically
// allow four times as much data as ucs-4, for simplicity we use one maximum length for all
// encodings. (Consider that an element in the largest Latin-1 chrarray could be replaced with a
// ucs-4 character, thus quadrupling its size.)
// XXX On the flip side, this means it's possible to create a string that, when encoded, cannot
// fit in a bytes object, as it'll be larger than LEN_MAX.
#define ypStringLib_ALLOCLEN_MAX                                                                \
    ((yp_ssize_t)MIN3(yp_SSIZE_T_MAX - yp_sizeof(ypStringLibObject),                            \
            (yp_SSIZE_T_MAX - yp_sizeof(ypStringLibObject)) / 4 /* /4 for elemsize of ucs-4 */, \
            ypObject_LEN_MAX))
#define ypStringLib_LEN_MAX (ypStringLib_ALLOCLEN_MAX - 1 /* for null terminator */)

// Returns true if ob is one of the types supported by ypStringLib
#define ypStringLib_TYPE_CHECK(ob) \
    (ypObject_TYPE_PAIR_CODE(ob) == ypStr_CODE || ypObject_TYPE_PAIR_CODE(ob) == ypBytes_CODE)

// Returns the type pair code to use for the given encoding
#define ypStringLib_PAIR_CODE_FROM_ENC(enc) \
    (enc == ypStringLib_enc_bytes ? ypBytes_CODE : ypStr_CODE)

// Debug-only macro to verify that bytes/str instances are stored as we expect
#define ypStringLib_ASSERT_INVARIANTS(s)                                                         \
    do {                                                                                         \
        yp_ASSERT(ypStringLib_TYPE_CHECK(s), "expected a bytes, bytearray, str, or chrarray");   \
        yp_ASSERT(ypStringLib_ALLOCLEN(s) < 0 /*immortals have an invalid alloclen*/ ||          \
                          ypStringLib_ALLOCLEN(s) <= ypStringLib_ALLOCLEN_MAX,                   \
                "bytes/str alloclen larger than ALLOCLEN_MAX");                                  \
        yp_ASSERT(ypStringLib_LEN(s) >= 0 && ypStringLib_LEN(s) <= ypStringLib_LEN_MAX,          \
                "bytes/str len not in range(LEN_MAX+1)");                                        \
        yp_ASSERT(ypStringLib_ALLOCLEN(s) < 0 /*immortals have an invalid alloclen*/ ||          \
                          ypStringLib_LEN(s) <= ypStringLib_ALLOCLEN(s) - 1 /*null terminator*/, \
                "bytes/str len (plus null terminator) larger than alloclen");                    \
        yp_ASSERT(ypStringLib_checkenc(s) == ypStringLib_ENC(s),                                 \
                "str not stored in smallest representation");                                    \
        yp_ASSERT(ypStringLib_ENC(s)->getindexX(ypStringLib_DATA(s), ypStringLib_LEN(s)) == 0,   \
                "bytes/str not internally null-terminated");                                     \
    } while (0)

// Gets a pointer to data[i] for the given sizeshift.
#define ypStringLib_ITEM_PTR(sizeshift, data, i) \
    ((void *)(((yp_uint8_t *)(data)) + ((i) << (sizeshift))))

// Uses memcpy to copy len elements from src[src_i] to dest[dest_i] for the given sizeshift.
// XXX dest and src cannot be the same pointer (the data copied cannot overlap).
#define ypStringLib_MEMCPY(sizeshift, dest, dest_i, src, src_i, len)                        \
    do {                                                                                    \
        yp_ASSERT(dest != src, "cannot memcpy inside an object; use ypStringLib_ELEMMOVE"); \
        yp_memcpy(ypStringLib_ITEM_PTR((sizeshift), (dest), (dest_i)),                      \
                ypStringLib_ITEM_PTR((sizeshift), (src), (src_i)), (len) << (sizeshift));   \
    } while (0)

// Uses memcmp to compare len elements from src[src_i] and dest[dest_i] for the given sizeshift.
#define ypStringLib_MEMCMP(sizeshift, dest, dest_i, src, src_i, len) \
    (yp_memcmp(ypStringLib_ITEM_PTR((sizeshift), (dest), (dest_i)),  \
            ypStringLib_ITEM_PTR((sizeshift), (src), (src_i)), (len) << (sizeshift)))

// Moves the elements from [src:] to the index dest; this can be used when deleting elements, or
// inserting elements (the new space is uninitialized). Assumes enough space is allocated for the
// move. Recall that memmove handles overlap. Also adjusts null terminator.
// XXX Don't forget that s may need to be down-converted after elements are deleted.
#define ypStringLib_ELEMMOVE(s, dest, src)                                                         \
    do {                                                                                           \
        yp_ASSERT(dest != src, "unnecessary memmove");                                             \
        yp_memmove(ypStringLib_ITEM_PTR(                                                           \
                           (ypStringLib_ENC(s)->sizeshift), ypStringLib_DATA(s), (dest)),          \
                ypStringLib_ITEM_PTR((ypStringLib_ENC(s)->sizeshift), ypStringLib_DATA(s), (src)), \
                (ypStringLib_LEN(s) - (src) + 1) << ypStringLib_ENC(s)->sizeshift);                \
    } while (0)

// Gets the ordinal at src[src_i]. src_i must be in range(len): no bounds checking is performed.
// TODO Is there a declaration we could give this (or the definitions) to make them faster?
typedef yp_uint32_t (*ypStringLib_getindexXfunc)(const void *src, yp_ssize_t src_i);

// Sets dest[dest_i] to value. dest_i must be in range(alloclen): no bounds checking is performed.
// XXX dest's encoding must be able to store value
// TODO Is there a declaration we could give this (or the definitions) to make them faster?
typedef void (*ypStringLib_setindexXfunc)(void *dest, yp_ssize_t dest_i, yp_uint32_t value);

// XXX In general for this table, make sure you do not use type codes with the wrong
// ypStringLib_ENC_* value. ypStringLib_ENC_CODE_BYTES should only be used for bytes, and
// vice-versa.
// XXX max_char may be larger than ypStringLib_MAX_UNICODE
typedef struct {
    yp_uint8_t                code;       // The ypStringLib_ENC_CODE_* value of the encoding
    yp_uint8_t                sizeshift;  // len<<sizeshift gives the size in bytes
    yp_ssize_t                elemsize;   // The size (in bytes) of one character
    yp_uint32_t               max_char;   // Largest character value that encoding can store
    ypObject                 *name;       // Immortal str of the encoding name (ie yp_s_latin_1)
    ypStringLib_getindexXfunc getindexX;  // Gets the ordinal at src[src_i]
    ypStringLib_setindexXfunc setindexX;  // Sets dest[dest_i] to value
} ypStringLib_encinfo;
static const ypStringLib_encinfo        ypStringLib_encs[4];
static const ypStringLib_encinfo *const ypStringLib_enc_bytes =
        &(ypStringLib_encs[ypStringLib_ENC_CODE_BYTES]);
static const ypStringLib_encinfo *const ypStringLib_enc_latin_1 =
        &(ypStringLib_encs[ypStringLib_ENC_CODE_LATIN_1]);
static const ypStringLib_encinfo *const ypStringLib_enc_ucs_2 =
        &(ypStringLib_encs[ypStringLib_ENC_CODE_UCS_2]);
static const ypStringLib_encinfo *const ypStringLib_enc_ucs_4 =
        &(ypStringLib_encs[ypStringLib_ENC_CODE_UCS_4]);


// Getters, setters, and copiers for our three internal encodings

// Gets ordinal at src[src_i]
static yp_uint32_t ypStringLib_getindexX_1byte(const void *src, yp_ssize_t src_i)
{
    yp_ASSERT(src_i >= 0, "indices must be >=0");
    return ((yp_uint8_t *)src)[src_i];
}
static yp_uint32_t ypStringLib_getindexX_2bytes(const void *src, yp_ssize_t src_i)
{
    yp_ASSERT(src_i >= 0, "indices must be >=0");
    return ((yp_uint16_t *)src)[src_i];
}
static yp_uint32_t ypStringLib_getindexX_4bytes(const void *src, yp_ssize_t src_i)
{
    yp_ASSERT(src_i >= 0, "indices must be >=0");
    return ((yp_uint32_t *)src)[src_i];
}

// Sets dest[dest_i] to value
static void ypStringLib_setindexX_1byte(void *dest, yp_ssize_t dest_i, yp_uint32_t value)
{
    yp_ASSERT(dest_i >= 0, "indices must be >=0");
    yp_ASSERT(value <= 0xFFu, "value larger than a byte");
    ((yp_uint8_t *)dest)[dest_i] = (yp_uint8_t)(value & 0xFFu);
}
static void ypStringLib_setindexX_2bytes(void *dest, yp_ssize_t dest_i, yp_uint32_t value)
{
    yp_ASSERT(dest_i >= 0, "indices must be >=0");
    yp_ASSERT(value <= 0xFFFFu, "value larger than two bytes");
    ((yp_uint16_t *)dest)[dest_i] = (yp_uint16_t)(value & 0xFFFFu);
}
static void ypStringLib_setindexX_4bytes(void *dest, yp_ssize_t dest_i, yp_uint32_t value)
{
    yp_ASSERT(dest_i >= 0, "indices must be >=0");
    ((yp_uint32_t *)dest)[dest_i] = value;
}

// Because we are converting to a larger encoding, copying in reverse avoids having to copy to a
// temporary buffer.
// TODO Write multiple elements at once and, if possible, read in multiples too?
#define _ypStringLib_INPLACE_UPCONVERT_FUNCTION(name, dest_type, src_type) \
    static void name(void *_data, yp_ssize_t len)                          \
    {                                                                      \
        dest_type      *dest = ((dest_type *)_data) + len - 1;             \
        const src_type *src = ((src_type *)_data) + len - 1;               \
        for (/*len already set*/; len > 0; len--) {                        \
            *dest = *src;                                                  \
            dest--;                                                        \
            src--;                                                         \
        }                                                                  \
    }
_ypStringLib_INPLACE_UPCONVERT_FUNCTION(_ypStringLib_inplace_4from2, yp_uint32_t, yp_uint16_t);
_ypStringLib_INPLACE_UPCONVERT_FUNCTION(_ypStringLib_inplace_4from1, yp_uint32_t, yp_uint8_t);
_ypStringLib_INPLACE_UPCONVERT_FUNCTION(_ypStringLib_inplace_2from1, yp_uint16_t, yp_uint8_t);

// Converts the len characters at data to a larger encoding
// XXX There must be enough room in data to fit the larger characters
// XXX new_sizeshift must be larger than old_sizeshift
static void ypStringLib_inplace_upconvert(
        int new_sizeshift, int old_sizeshift, void *data, yp_ssize_t len)
{
    yp_ASSERT(new_sizeshift > old_sizeshift, "can only upconvert to a larger encoding, of course");

    if (new_sizeshift == 2) {  // ucs-4
        if (old_sizeshift == 1) {
            _ypStringLib_inplace_4from2(data, len);
        } else {
            yp_ASSERT(old_sizeshift == 0, "unexpected old_sizeshift");
            _ypStringLib_inplace_4from1(data, len);
        }
    } else {  // ucs-2
        // If dest was sizeshift 0, then src would be too, and we'd have hit the memcpy case
        yp_ASSERT(new_sizeshift == 1, "unexpected new_sizeshift");
        yp_ASSERT(old_sizeshift == 0, "unexpected old_sizeshift");
        _ypStringLib_inplace_2from1(data, len);
    }
}

// Because we are converting to a smaller encoding, copying in natural order avoids having to copy
// to a temporary buffer.
// TODO Write multiple elements at once and, if possible, read in multiples too?
#define _ypStringLib_INPLACE_DOWNCONVERT_FUNCTION(name, dest_type, src_type)            \
    static void name(void *_data, yp_ssize_t len)                                       \
    {                                                                                   \
        dest_type      *dest = ((dest_type *)_data);                                    \
        const src_type *src = ((src_type *)_data);                                      \
        for (/*len already set*/; len > 0; len--) {                                     \
            *dest = (dest_type)*src;                                                    \
            yp_ASSERT(*dest == *src, "ypStringLib_inplace_downconvert truncated data"); \
            dest++;                                                                     \
            src++;                                                                      \
        }                                                                               \
    }
_ypStringLib_INPLACE_DOWNCONVERT_FUNCTION(_ypStringLib_inplace_1from2, yp_uint8_t, yp_uint16_t);
_ypStringLib_INPLACE_DOWNCONVERT_FUNCTION(_ypStringLib_inplace_1from4, yp_uint8_t, yp_uint32_t);
_ypStringLib_INPLACE_DOWNCONVERT_FUNCTION(_ypStringLib_inplace_2from4, yp_uint16_t, yp_uint32_t);

// Converts the len characters at data to a smaller encoding.
// XXX Ensure data can be losslessly converted to the smaller encoding. Too-large characters will
// be truncated.
// XXX new_sizeshift must be smaller than old_sizeshift
static void ypStringLib_inplace_downconvert(
        int new_sizeshift, int old_sizeshift, void *data, yp_ssize_t len)
{
    yp_ASSERT(
            new_sizeshift < old_sizeshift, "can only downconvert to a smaller encoding, of course");

    if (new_sizeshift == 0) {  // latin-1
        if (old_sizeshift == 1) {
            _ypStringLib_inplace_1from2(data, len);
        } else {
            yp_ASSERT(old_sizeshift == 2, "unexpected old_sizeshift");
            _ypStringLib_inplace_1from4(data, len);
        }
    } else {  // ucs-2
        // If dest was sizeshift 2, then src would be too, and we'd have hit the memcpy case
        yp_ASSERT(new_sizeshift == 1, "unexpected new_sizeshift");
        yp_ASSERT(old_sizeshift == 2, "unexpected old_sizeshift");
        _ypStringLib_inplace_2from4(data, len);
    }
}

// TODO Write multiple elements at once and, if possible, read in multiples too
#define _ypStringLib_ELEMCOPY_UPCONVERT_FUNCTION(name, dest_type, src_type)                     \
    static void name(dest_type *dest, yp_ssize_t dest_i, const src_type *src, yp_ssize_t src_i, \
            yp_ssize_t len)                                                                     \
    {                                                                                           \
        dest += dest_i;                                                                         \
        src += src_i;                                                                           \
        for (/*len already set*/; len > 0; len--) {                                             \
            *dest = *src;                                                                       \
            dest++;                                                                             \
            src++;                                                                              \
        }                                                                                       \
    }
_ypStringLib_ELEMCOPY_UPCONVERT_FUNCTION(_ypStringLib_elemcopy_4from2, yp_uint32_t, yp_uint16_t);
_ypStringLib_ELEMCOPY_UPCONVERT_FUNCTION(_ypStringLib_elemcopy_4from1, yp_uint32_t, yp_uint8_t);
_ypStringLib_ELEMCOPY_UPCONVERT_FUNCTION(_ypStringLib_elemcopy_2from1, yp_uint16_t, yp_uint8_t);

// Copies len elements from src starting at src_i, and places them at dest starting at dest_i.
// To be used in contexts where dest may have a larger encoding than src (i.e. where
// dest_sizeshift>=src_sizeshift).
// XXX Not applicable in contexts where dest may be a smaller encoding. See
// ypStringLib_elemcopy_maybedownconvert for that.
// XXX dest and src cannot be the same pointer (the data copied cannot overlap).
static void ypStringLib_elemcopy_maybeupconvert(int dest_sizeshift, void *dest, yp_ssize_t dest_i,
        int src_sizeshift, const void *src, yp_ssize_t src_i, yp_ssize_t len)
{
    yp_ASSERT(dest_sizeshift >= src_sizeshift, "can't elemcopy to smaller encoding");
    yp_ASSERT(dest != src, "cannot elemcopy inside an object; use ypStringLib_ELEMMOVE");
    yp_ASSERT(dest_i >= 0 && src_i >= 0 && len >= 0, "indices/lengths must be >=0");

    if (dest_sizeshift == src_sizeshift) {
        // Huzzah!  We get to use the nice-and-quick memcpy
        ypStringLib_MEMCPY(dest_sizeshift, dest, dest_i, src, src_i, len);
    } else if (dest_sizeshift == 2) {  // ucs-4
        // If src was also sizeshift 2, then we'd have hit the memcpy case
        if (src_sizeshift == 1) {
            _ypStringLib_elemcopy_4from2(dest, dest_i, src, src_i, len);
        } else {
            yp_ASSERT(src_sizeshift == 0, "unexpected src_sizeshift");
            _ypStringLib_elemcopy_4from1(dest, dest_i, src, src_i, len);
        }
    } else {  // ucs-2
        // If dest was sizeshift 0, then src would be too, and we'd have hit the memcpy case
        yp_ASSERT(dest_sizeshift == 1, "unexpected dest_sizeshift");
        yp_ASSERT(src_sizeshift == 0, "unexpected src_sizeshift");
        _ypStringLib_elemcopy_2from1(dest, dest_i, src, src_i, len);
    }
}

// TODO Write multiple elements at once and, if possible, read in multiples too
#define _ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(name, dest_type, src_type)                   \
    static void name(dest_type *dest, yp_ssize_t dest_i, const src_type *src, yp_ssize_t src_i, \
            yp_ssize_t src_step, yp_ssize_t slicelength)                                        \
    {                                                                                           \
        dest += dest_i;                                                                         \
        src += src_i;                                                                           \
        for (/*slicelength already set*/; slicelength > 0; slicelength--) {                     \
            *dest = (dest_type)*src;                                                            \
            yp_ASSERT(*dest == *src, "ypStringLib_elemcopy_maybedownconvert truncated data");   \
            dest++;                                                                             \
            src += src_step;                                                                    \
        }                                                                                       \
    }
_ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(_ypStringLib_elemcopy_1from1, yp_uint8_t, yp_uint8_t);
_ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(_ypStringLib_elemcopy_1from2, yp_uint8_t, yp_uint16_t);
_ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(_ypStringLib_elemcopy_1from4, yp_uint8_t, yp_uint32_t);
_ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(_ypStringLib_elemcopy_2from2, yp_uint16_t, yp_uint16_t);
_ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(_ypStringLib_elemcopy_2from4, yp_uint16_t, yp_uint32_t);
_ypStringLib_ELEMCOPY_DOWNCONVERT_FUNCTION(_ypStringLib_elemcopy_4from4, yp_uint32_t, yp_uint32_t);

// Copies slicelength elements from src starting at src_i, and places them at dest starting at
// dest_i. To be used in contexts where dest may have a smaller encoding than src (i.e. where
// dest_sizeshift<=src_sizeshift). As getslice is one such context, the src_step parameter allows
// copying non-contiguous, and reversed, characters from src. src_i, src_step, and slicelength must
// be the _adjusted_ values from ypSlice_AdjustIndicesC. Never writes the null-terminator.
// XXX Not applicable in contexts where dest may be a larger encoding. See
// ypStringLib_elemcopy_maybeupconvert for that.
// XXX dest and src cannot be the same pointer (the data copied cannot overlap).
static void ypStringLib_elemcopy_maybedownconvert_getslice(int dest_sizeshift, void *dest,
        yp_ssize_t dest_i, int src_sizeshift, const void *src, yp_ssize_t src_i,
        yp_ssize_t src_step, yp_ssize_t slicelength)
{
    yp_ASSERT(dest_sizeshift >= src_sizeshift, "can't elemcopy to smaller encoding");
    yp_ASSERT(dest != src, "cannot elemcopy inside an object; use ypStringLib_ELEMMOVE");
    yp_ASSERT(dest_i >= 0 && src_i >= 0 && slicelength >= 0, "indices/lengths must be >=0");
    ypSlice_ASSERT_ADJUSTED_INDICES(src_i, (yp_ssize_t)yp_SLICE_DEFAULT, src_step, slicelength);

    if (dest_sizeshift == src_sizeshift && src_step == 1) {
        // Huzzah!  We get to use the nice-and-quick memcpy
        ypStringLib_MEMCPY(dest_sizeshift, dest, dest_i, src, src_i, slicelength);
    } else if (dest_sizeshift == 0) {  // latin-1
        if (src_sizeshift == 0) {
            _ypStringLib_elemcopy_1from1(dest, dest_i, src, src_i, src_step, slicelength);
        } else if (src_sizeshift == 1) {
            _ypStringLib_elemcopy_1from2(dest, dest_i, src, src_i, src_step, slicelength);
        } else {
            yp_ASSERT(src_sizeshift == 2, "unexpected src_sizeshift");
            _ypStringLib_elemcopy_1from4(dest, dest_i, src, src_i, src_step, slicelength);
        }
    } else if (dest_sizeshift == 1) {  // ucs-2
        if (src_sizeshift == 1) {
            _ypStringLib_elemcopy_2from2(dest, dest_i, src, src_i, src_step, slicelength);
        } else {
            yp_ASSERT(src_sizeshift == 2, "unexpected src_sizeshift");
            _ypStringLib_elemcopy_2from4(dest, dest_i, src, src_i, src_step, slicelength);
        }
    } else {
        yp_ASSERT(dest_sizeshift == 2, "unexpected dest_sizeshift");
        yp_ASSERT(src_sizeshift == 2, "unexpected src_sizeshift");
        _ypStringLib_elemcopy_4from4(dest, dest_i, src, src_i, src_step, slicelength);
    }
}

// A version of ypStringLib_elemcopy_maybedownconvert_getslice with an identical signature to
// ypStringLib_elemcopy_maybeupconvert, so their pointers can be used interchangeably.
static void ypStringLib_elemcopy_maybedownconvert(int dest_sizeshift, void *dest, yp_ssize_t dest_i,
        int src_sizeshift, const void *src, yp_ssize_t src_i, yp_ssize_t slicelength)
{
    ypStringLib_elemcopy_maybedownconvert_getslice(
            dest_sizeshift, dest, dest_i, src_sizeshift, src, src_i, 1, slicelength);
}

#define ypStringLib_TYPE_CHECKENC_1FROM2_MASK 0xFF00FF00FF00FF00ULL
yp_STATIC_ASSERT(((_yp_uint_t)ypStringLib_TYPE_CHECKENC_1FROM2_MASK) ==
                         ypStringLib_TYPE_CHECKENC_1FROM2_MASK,
        checkenc_1from2_mask_matches_type);
// XXX Adapted from Python's ascii_decode and STRINGLIB(find_max_char)
static const ypStringLib_encinfo *_ypStringLib_checkenc_contiguous_ucs_2(
        const void *data, yp_ssize_t start, yp_ssize_t slicelength)
{
    const _yp_uint_t  mask = ypStringLib_TYPE_CHECKENC_1FROM2_MASK;
    const yp_uint8_t *p = (yp_uint8_t *)data + start;
    const yp_uint8_t *end = p + (slicelength * 2);
    const yp_uint8_t *aligned_end = yp_ALIGN_DOWN(end, yp_sizeof(_yp_uint_t));
    yp_ASSERT((*(yp_uint8_t *)&mask) == 0,
            "_ypStringLib_checkenc_contiguous_ucs_2 doesn't support big-endian yet");
    yp_ASSERT(yp_IS_ALIGNED(data, 2), "unexpected alignment for ucs-2 data");
    yp_ASSERT1(slicelength > 0);

    // If we don't contain an aligned _yp_uint_t, jump to the end
    if (aligned_end - p < yp_sizeof(_yp_uint_t)) goto final_loop;

    // Read the first few elements until we're aligned
    while (!yp_IS_ALIGNED(p, yp_sizeof(_yp_uint_t))) {
        yp_uint16_t value = *((yp_uint16_t *)p);
        // TODO This won't work on big-endian: will need a mask (for uint) and smallmask (uint16)
        if (value & mask) return ypStringLib_enc_ucs_2;
        p += 2;
    }

    // Now read as many aligned ints as we can
    while (p < aligned_end) {
        _yp_uint_t value = *((_yp_uint_t *)p);
        if (value & mask) return ypStringLib_enc_ucs_2;
        p += yp_sizeof(_yp_uint_t);
    }

// Now read the final, unaligned elements
final_loop:
    while (p < end) {
        yp_uint16_t value = *((yp_uint16_t *)p);
        if (value & mask) return ypStringLib_enc_ucs_2;
        p += 2;
    }
    return ypStringLib_enc_latin_1;
}

#define ypStringLib_TYPE_CHECKENC_1FROM4_MASK 0xFFFFFF00FFFFFF00ULL
yp_STATIC_ASSERT(((_yp_uint_t)ypStringLib_TYPE_CHECKENC_1FROM4_MASK) ==
                         ypStringLib_TYPE_CHECKENC_1FROM4_MASK,
        checkenc_1from4_mask_matches_type);
#define ypStringLib_TYPE_CHECKENC_2FROM4_MASK 0xFFFF0000FFFF0000ULL
yp_STATIC_ASSERT(((_yp_uint_t)ypStringLib_TYPE_CHECKENC_2FROM4_MASK) ==
                         ypStringLib_TYPE_CHECKENC_2FROM4_MASK,
        checkenc_2from4_mask_matches_type);
// Returns true if the ucs-4 string can be encoded in the encoding matching mask. *p will point
// to the location that failed the check, or to *end on success.
// XXX On first look, this shares a lot of code with _ypStringLib_checkenc_contiguous_ucs_2.
// However, the two "unaligned elements" loops are different (16- vs 32-bit reads).
static int _ypStringLib_checkenc_ucs_4(_yp_uint_t mask, const yp_uint8_t **p, const yp_uint8_t *end)
{
    const yp_uint8_t *aligned_end = yp_ALIGN_DOWN(end, yp_sizeof(_yp_uint_t));
    yp_ASSERT((*(yp_uint8_t *)&mask) == 0,
            "_ypStringLib_checkenc_contiguous_ucs_4 doesn't support big-endian yet");

    // If we don't contain an aligned _yp_uint_t, jump to the end
    if (aligned_end - *p < yp_sizeof(_yp_uint_t)) goto final_loop;

    // Read the first few elements until we're aligned
    while (!yp_IS_ALIGNED(*p, yp_sizeof(_yp_uint_t))) {
        yp_uint32_t value = *((yp_uint32_t *)*p);
        // TODO This won't work on big-endian: will need a mask (for uint) and smallmask (uint16)
        if (value & mask) return FALSE;
        *p += 4;
    }

    // Now read as many aligned ints as we can
    while (*p < aligned_end) {
        _yp_uint_t value = *((_yp_uint_t *)*p);
        if (value & mask) return FALSE;
        *p += yp_sizeof(_yp_uint_t);
    }

// Now read the final, unaligned elements
final_loop:
    while (*p < end) {
        yp_uint32_t value = *((yp_uint32_t *)*p);
        if (value & mask) return FALSE;
        *p += 4;
    }
    return TRUE;
}

// Returns the ypStringLib_ENC_* that _should_ be used for the given ucs-4-encoded string
// XXX Adapted from Python's ascii_decode and STRINGLIB(find_max_char)
static const ypStringLib_encinfo *_ypStringLib_checkenc_contiguous_ucs_4(
        const void *data, yp_ssize_t start, yp_ssize_t slicelength)
{
    const yp_uint8_t *p = (yp_uint8_t *)data + start;
    const yp_uint8_t *end = p + (slicelength * 4);
    yp_ASSERT(yp_IS_ALIGNED(data, 4), "unexpected alignment for ucs-4 data");
    yp_ASSERT1(slicelength > 0);

    // If the 1FROM4 mask fails in the middle of the string, we can resume from that point
    // because we know 2FROM4 will match the first half anyway.
    if (_ypStringLib_checkenc_ucs_4(ypStringLib_TYPE_CHECKENC_1FROM4_MASK, &p, end)) {
        return ypStringLib_enc_latin_1;
    }
    if (_ypStringLib_checkenc_ucs_4(ypStringLib_TYPE_CHECKENC_2FROM4_MASK, &p, end)) {
        return ypStringLib_enc_ucs_2;
    }
    return ypStringLib_enc_ucs_4;
}

static const ypStringLib_encinfo *_ypStringLib_checkenc_getslice_ucs_2(
        const yp_uint16_t *data, yp_ssize_t start, yp_ssize_t step, yp_ssize_t slicelength)
{
    yp_ssize_t i;
    yp_ASSERT(yp_IS_ALIGNED(data, 2), "unexpected alignment for ucs-2 data");
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    yp_ASSERT(step > 1, "unexpected step");
    for (i = 0; i < slicelength; i++) {
        if (data[ypSlice_INDEX(start, step, i)] > 0xFFu) return ypStringLib_enc_ucs_2;
    }
    return ypStringLib_enc_latin_1;
}

static const ypStringLib_encinfo *_ypStringLib_checkenc_getslice_ucs_4(
        const yp_uint32_t *data, yp_ssize_t start, yp_ssize_t step, yp_ssize_t slicelength)
{
    yp_ssize_t i;
    yp_ASSERT(yp_IS_ALIGNED(data, 4), "unexpected alignment for ucs-4 data");
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    yp_ASSERT(step > 1, "unexpected step");

    // If the latin-1 check fails in the middle of the string, we can resume from that point
    // because we know the ucs-2 check will match the first half anyway.
    for (i = 0; i < slicelength; i++) {
        if (data[ypSlice_INDEX(start, step, i)] > 0xFFu) goto check_for_ucs_2;
    }
    return ypStringLib_enc_latin_1;

check_for_ucs_2:
    for (/*i already set*/; i < slicelength; i++) {
        if (data[ypSlice_INDEX(start, step, i)] > 0xFFFFu) return ypStringLib_enc_ucs_4;
    }
    return ypStringLib_enc_ucs_2;
}

// Returns the minimal encoding required for the given slice of s's data. start, stop, step, and
// slicelength must be the _adjusted_ values from ypSlice_AdjustIndicesC.
// XXX Check for the empty slice and total slice cases first
static const ypStringLib_encinfo *ypStringLib_checkenc_getslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, yp_ssize_t slicelength)
{
    const ypStringLib_encinfo *enc = ypStringLib_ENC(s);
    const void                *data = ypStringLib_DATA(s);

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength);
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    yp_ASSERT(slicelength < ypStringLib_LEN(s), "missed a 'total slice' optimization");

    if (enc->elemsize == 1) {
        yp_ASSERT(enc == ypStringLib_enc_bytes || enc == ypStringLib_enc_latin_1);
        return enc;  // can't shrink any smaller than one byte
    }

    if (step < 0) ypSlice_InvertIndicesC(&start, &stop, &step, slicelength);

    if (step == 1) {
        if (enc == ypStringLib_enc_ucs_2) {
            return _ypStringLib_checkenc_contiguous_ucs_2(data, start, slicelength);
        } else {
            yp_ASSERT(enc == ypStringLib_enc_ucs_4);
            return _ypStringLib_checkenc_contiguous_ucs_4(data, start, slicelength);
        }
    } else {
        if (enc == ypStringLib_enc_ucs_2) {
            return _ypStringLib_checkenc_getslice_ucs_2(data, start, step, slicelength);
        } else {
            yp_ASSERT(enc == ypStringLib_enc_ucs_4);
            return _ypStringLib_checkenc_getslice_ucs_4(data, start, step, slicelength);
        }
    }
}

// Returns the minimal encoding required for the remaining characters after the slice from s is
// deleted. start, stop, step, and slicelength must be the _adjusted_ values from
// ypSlice_AdjustIndicesC.
// XXX Check for the empty slice and total slice cases first
static const ypStringLib_encinfo *ypStringLib_checkenc_delslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, yp_ssize_t slicelength)
{
    const ypStringLib_encinfo *enc = ypStringLib_ENC(s);
    const void                *data = ypStringLib_DATA(s);
    const ypStringLib_encinfo *(*checkenc_contiguous)(const void *, yp_ssize_t, yp_ssize_t);
    const ypStringLib_encinfo *chunk_enc;
    const ypStringLib_encinfo *max_enc;

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength);
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    yp_ASSERT(slicelength < ypStringLib_LEN(s), "missed a 'total slice' optimization");

    if (enc->elemsize == 1) {
        yp_ASSERT(enc == ypStringLib_enc_bytes || enc == ypStringLib_enc_latin_1);
        return enc;  // can't shrink any smaller than one byte
    }

    if (enc == ypStringLib_enc_ucs_2) {
        checkenc_contiguous = _ypStringLib_checkenc_contiguous_ucs_2;
    } else {
        yp_ASSERT(enc == ypStringLib_enc_ucs_4);
        checkenc_contiguous = _ypStringLib_checkenc_contiguous_ucs_4;
    }

    if (step < 0) ypSlice_InvertIndicesC(&start, &stop, &step, slicelength);

    // Start by assuming the data can be latin-1, then check each chunk to see if it needs a larger
    // encoding. If we encounter a chunk that needs our curent encoding, we can exit early.
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypStr_CODE);
    max_enc = ypStringLib_enc_latin_1;

    // The before-start and after-end chunks are more likely to benefit from the "aligned int"
    // optimization, so start there.
    if (start > 0) {
        chunk_enc = checkenc_contiguous(data, 0, start);
        if (chunk_enc == enc) return chunk_enc;  // early exit
        if (max_enc->sizeshift < chunk_enc->sizeshift) max_enc = chunk_enc;
    }
    if (stop < ypStringLib_LEN(s)) {
        chunk_enc = checkenc_contiguous(data, stop, ypStringLib_LEN(s) - stop);
        if (chunk_enc == enc) return chunk_enc;  // early exit
        if (max_enc->sizeshift < chunk_enc->sizeshift) max_enc = chunk_enc;
    }

    // Now we need to check all the characters between the deleted characters to see what the final
    // encoding would be. Recall that if step==1, there are no "between characters".
    // TODO There's opportunity to optimize this further. The "aligned int" condition in
    // _ypStringLib_checkenc_contiguous_* is evaluated on every iteration, but may be possible to
    // just check on the first iteration. Further, if step is small, that "aligned int" condition is
    // useless, as it will never apply. A small step could instead be checked using a custom bitmask
    // (i.e. if step is 2 (and aligned) then 0xFF00FF00FF00FF00ULL could become
    // 0x0000FF000000FF00ULL). Additionally, _ypStringLib_checkenc_contiguous_ucs_4 always starts
    // off checking for latin-1, which is unnecessary if previous chunks were ucs-2.
    // TODO There's another optimization where, if the largest character we are deleting doesn't
    // require the current encoding, then one of the characters we are keeping must require it. We'd
    // still have to check the entire string anyway if this fails, so perhaps this optimization
    // would only apply for small slicelengths.
    if (step > 1) {
        yp_ssize_t i;
        for (i = 0; i < slicelength; i++) {
            chunk_enc = checkenc_contiguous(data, ypSlice_INDEX(start, step, i) + 1, step - 1);
            if (chunk_enc == enc) return chunk_enc;  // early exit
            if (max_enc->sizeshift < chunk_enc->sizeshift) max_enc = chunk_enc;
        }
    }

    // If we get here, then the remaining string can be shrunk to the maximum encoding.
    return max_enc;
}

// Returns the minimal encoding required for s's data.
static const ypStringLib_encinfo *ypStringLib_checkenc(ypObject *s)
{
    const ypStringLib_encinfo *enc = ypStringLib_ENC(s);
    yp_ssize_t                 len = ypStringLib_LEN(s);
    const void                *data = ypStringLib_DATA(s);

    if (enc->elemsize == 1) {
        yp_ASSERT(enc == ypStringLib_enc_bytes || enc == ypStringLib_enc_latin_1);
        return enc;  // can't shrink any smaller than one byte
    } else if (len < 1) {
        yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypStr_CODE);
        return ypStringLib_enc_latin_1;
    } else if (enc == ypStringLib_enc_ucs_2) {
        return _ypStringLib_checkenc_contiguous_ucs_2(data, 0, len);
    } else {
        yp_ASSERT(enc == ypStringLib_enc_ucs_4);
        return _ypStringLib_checkenc_contiguous_ucs_4(data, 0, len);
    }
}

// As yp_index_asuint8C, but raises yp_ValueError (not yp_OverflowError) when out of range.
static yp_uint8_t _ypBytes_asuint8C(ypObject *x, ypObject **exc)
{
    // Recall that when yp_index_asintC raises an exception, it returns zero.
    yp_int_t   asint = yp_index_asintC(x, exc);
    yp_uint8_t retval = (yp_uint8_t)(asint & 0xFFu);
    if ((yp_int_t)retval != asint) return_yp_CEXC_ERR(retval, exc, yp_ValueError);
    return retval;
}

// Sets *x_asitem to the character that x represents, and *x_enc to the encoding for that character,
// returning yp_None. If x is not of the appropriate type for an item of the target string (i.e. int
// for bytes, str for str, etc), returns yp_TypeError. If the value of x is out of range for the
// target string, returns yp_ValueError. On error, *x_asitem and *x_enc are undefined.
typedef ypObject *(*ypStringLib_asitemCfunc)(
        ypObject *x, yp_uint32_t *x_asitem, const ypStringLib_encinfo **x_enc);

static ypObject *_ypBytes_asitemC(
        ypObject *x, yp_uint32_t *x_asitem, const ypStringLib_encinfo **x_enc)
{
    ypObject *exc = yp_None;
    *x_enc = ypStringLib_enc_bytes;
    *x_asitem = _ypBytes_asuint8C(x, &exc);
    return exc;
}
static ypObject *_ypStr_asitemC(
        ypObject *x, yp_uint32_t *x_asitem, const ypStringLib_encinfo **x_enc)
{
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) {
        return_yp_BAD_TYPE(x);
    } else if (ypStringLib_LEN(x) != 1) {
        // In Python, ord('12') returns TypeError, but that seems at odds with b[0]=300 returning
        // ValueError for a bytearray. ValueError seems like the correct option.
        return yp_ValueError;
    } else {
        *x_enc = ypStringLib_ENC(x);
        *x_asitem = (*x_enc)->getindexX(ypStringLib_DATA(x), 0);
        return yp_None;
    }
}
#define ypStringLib_ASITEM_FUNC(s) \
    (ypObject_TYPE_PAIR_CODE(s) == ypStr_CODE ? _ypStr_asitemC : _ypBytes_asitemC)


// Return a new bytes/bytearray/str/chrarray object that can fit the given requiredLen plus the null
// terminator. If type is immutable and alloclen_fixed is true (indicating the object will never
// grow), the data is placed inline with one allocation. enc_code must agree with type.
// XXX Remember to add the null terminator
// XXX Check for the empty immutable, negative len, and >max len cases first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Overallocate to avoid future resizings
static ypObject *_ypStringLib_new(
        int type, yp_ssize_t requiredLen, int alloclen_fixed, const ypStringLib_encinfo *enc)
{
    ypObject *newS;
    yp_ASSERT(ypObject_TYPE_CODE_AS_FROZEN(type) == ypStringLib_PAIR_CODE_FROM_ENC(enc),
            "incorrect type for encoding");
    yp_ASSERT(requiredLen >= 0, "requiredLen cannot be negative");
    yp_ASSERT(requiredLen <= ypStringLib_LEN_MAX, "requiredLen cannot be >max");
    if (alloclen_fixed && !ypObject_TYPE_CODE_IS_MUTABLE(type)) {
        yp_ASSERT(requiredLen > 0, "missed an empty immutable optimization");
        newS = ypMem_MALLOC_CONTAINER_INLINE4(
                ypStringLibObject, type, requiredLen + 1, ypStringLib_ALLOCLEN_MAX, enc->elemsize);
    } else {
        newS = ypMem_MALLOC_CONTAINER_VARIABLE5(ypStringLibObject, type, requiredLen + 1, 0,
                ypStringLib_ALLOCLEN_MAX, enc->elemsize);
    }
    ypStringLib_ENC_CODE(newS) = enc->code;
    return newS;
}

static ypObject *_ypBytes_new(int type, yp_ssize_t requiredLen, int alloclen_fixed)
{
    return _ypStringLib_new(type, requiredLen, alloclen_fixed, ypStringLib_enc_bytes);
}
static ypObject *_ypStr_new(
        int type, yp_ssize_t requiredLen, int alloclen_fixed, const ypStringLib_encinfo *enc)
{
    yp_ASSERT1(ypObject_TYPE_CODE_AS_FROZEN(type) == ypStr_CODE);
    return _ypStringLib_new(type, requiredLen, alloclen_fixed, enc);
}
static ypObject *_ypStr_new_latin_1(int type, yp_ssize_t requiredLen, int alloclen_fixed)
{
    return _ypStringLib_new(type, requiredLen, alloclen_fixed, ypStringLib_enc_latin_1);
}
static ypObject *_ypStr_new_ucs_2(int type, yp_ssize_t requiredLen, int alloclen_fixed)
{
    return _ypStringLib_new(type, requiredLen, alloclen_fixed, ypStringLib_enc_ucs_2);
}
static ypObject *_ypStr_new_ucs_4(int type, yp_ssize_t requiredLen, int alloclen_fixed)
{
    return _ypStringLib_new(type, requiredLen, alloclen_fixed, ypStringLib_enc_ucs_4);
}

// Returns an empty string of the given type, which may be one of the immortal immutables.
static ypObject *ypStringLib_new_empty(int type)
{
    if (ypObject_TYPE_CODE_IS_MUTABLE(type)) {
        if (type == ypChrArray_CODE) return yp_chrarray0();
        yp_ASSERT1(type == ypByteArray_CODE);
        return yp_bytearray0();
    } else {
        if (type == ypStr_CODE) return yp_str_empty;
        yp_ASSERT1(type == ypBytes_CODE);
        return yp_bytes_empty;
    }
}

// Returns a new copy of s of the given type. If type is immutable and alloclen_fixed is true
// (indicating the object will never grow), the data is placed inline with one allocation.
// XXX Check for the possibility of a lazy shallow copy before calling this function
// XXX Check for the empty immutable case first
static ypObject *ypStringLib_new_copy(int type, ypObject *s, int alloclen_fixed)
{
    yp_ssize_t                 s_len = ypStringLib_LEN(s);
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);
    ypObject                  *copy;

    yp_ASSERT(ypObject_TYPE_CODE_AS_FROZEN(type) == ypObject_TYPE_PAIR_CODE(s),
            "incorrect type for copy");

    copy = _ypStringLib_new(type, s_len, alloclen_fixed, s_enc);
    if (yp_isexceptionC(copy)) return copy;
    ypStringLib_MEMCPY(
            s_enc->sizeshift, ypStringLib_DATA(copy), 0, ypStringLib_DATA(s), 0, (s_len + 1));
    ypStringLib_SET_LEN(copy, s_len);
    ypStringLib_ASSERT_INVARIANTS(copy);
    return copy;
}

// Returns a shallow copy of s of the given type. Optimizes for the lazy shallow copy and
// empty immutable cases.
static ypObject *ypStringLib_copy(int type, ypObject *s)
{
    yp_ASSERT(ypObject_TYPE_CODE_AS_FROZEN(type) == ypObject_TYPE_PAIR_CODE(s),
            "incorrect type for copy");

    if (ypStringLib_LEN(s) < 1) return ypStringLib_new_empty(type);
    if (!ypObject_IS_MUTABLE(s) && ypObject_TYPE_CODE(s) == type) return yp_incref(s);
    return ypStringLib_new_copy(type, s, /*alloclen_fixed=*/TRUE);
}

// If the allocated buffer for s is not large enough to hold requiredLen characters (plus null
// terminator) in the newEnc encoding, it is reallocated. As with ypMem_REALLOC_CONTAINER_VARIABLE,
// returns NULL on error, ypStringLib_DATA if reused/reallocated in-place, and oldptr if a new
// buffer was allocated. Updates ypStringLib_ALLOCLEN to be consistent with newEnc (except on
// error). Does not update enc or len and does not null-terminate. Remember to free oldptr with
// ypMem_REALLOC_CONTAINER_FREE_OLDPTR after copying the data over.
// TODO Is the word "maybe" correct here? If we are always updaing alloclen, isn't that a realloc?
static ypObject *_ypStringLib_maybe_realloc(
        ypObject *s, yp_ssize_t requiredLen, yp_ssize_t extra, const ypStringLib_encinfo *newEnc)
{
    yp_ssize_t haveBytes = ypStringLib_ALLOCLEN(s) << ypStringLib_ENC(s)->sizeshift;
    yp_ssize_t needBytes = (requiredLen + 1) << newEnc->sizeshift;

    yp_ASSERT(requiredLen >= 0, "requiredLen cannot be negative");
    yp_ASSERT(extra >= 0, "extra cannot be negative");
    yp_ASSERT(requiredLen <= ypStringLib_LEN_MAX, "requiredLen cannot be >max");
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypStringLib_PAIR_CODE_FROM_ENC(newEnc),
            "incorrect type for encoding");

    if (haveBytes >= needBytes) {
        ypStringLib_SET_ALLOCLEN(s, haveBytes >> newEnc->sizeshift);
        return ypStringLib_DATA(s);
    } else {
        // ypMem_REALLOC sets alloclen, but does not require it to be correct on input. If it did,
        // we'd need to adjust it to the new encoding first.
        return ypMem_REALLOC_CONTAINER_VARIABLE5(s, ypStringLibObject, requiredLen + 1, extra,
                ypStringLib_ALLOCLEN_MAX, newEnc->elemsize);
    }
}

// Called on push/append, extend, irepeat, and similar functions to increase the alloclen and/or
// elemsize of s to fit requiredLen (plus null terminator). Updates alloclen and enc. Doesn't update
// len and doesn't write the null terminator. newEnc must be the same or larger as currently.
static ypObject *_ypStringLib_grow_onextend(
        ypObject *s, yp_ssize_t requiredLen, yp_ssize_t extra, const ypStringLib_encinfo *newEnc)
{
    const ypStringLib_encinfo *oldEnc = ypStringLib_ENC(s);
    void                      *oldptr;

    yp_ASSERT(requiredLen >= ypStringLib_LEN(s), "requiredLen cannot be <len(s)");
    yp_ASSERT(requiredLen > ypStringLib_ALLOCLEN(s) - 1 || newEnc->elemsize > oldEnc->elemsize,
            "_ypStringLib_grow_onextend called unnecessarily");
    yp_ASSERT(newEnc->elemsize >= oldEnc->elemsize, "can't 'grow' to a smaller encoding");

    // Recall _ypStringLib_maybe_realloc adjusts alloclen for the new encoding.
    oldptr = _ypStringLib_maybe_realloc(s, requiredLen, extra, newEnc);
    if (oldptr == NULL) return yp_MemoryError;

    // alloclen is now updated for the new encoding, but the data may still be in the old encoding
    // and/or may need to be copied over from oldptr.
    if (ypStringLib_DATA(s) == oldptr) {
        if (oldEnc != newEnc) {
            ypStringLib_inplace_upconvert(
                    newEnc->sizeshift, oldEnc->sizeshift, ypStringLib_DATA(s), ypStringLib_LEN(s));
        }
    } else {
        ypStringLib_elemcopy_maybeupconvert(newEnc->sizeshift, ypStringLib_DATA(s), 0,
                oldEnc->sizeshift, oldptr, 0, ypStringLib_LEN(s));
        ypMem_REALLOC_CONTAINER_FREE_OLDPTR(s, ypStringLibObject, oldptr);
    }
    ypStringLib_ENC_CODE(s) = newEnc->code;
    return yp_None;
}

// Appends x to s, updating the length. Never writes the null-terminator.
static ypObject *ypStringLib_push(
        ypObject *s, yp_uint32_t x, const ypStringLib_encinfo *x_enc, yp_ssize_t extra)
{
    yp_ssize_t                 newLen;
    const ypStringLib_encinfo *newEnc;

    // TODO Assert that x_enc matches s?

    if (ypStringLib_LEN(s) > ypStringLib_LEN_MAX - 1) return yp_MemorySizeOverflowError;
    newLen = ypStringLib_LEN(s) + 1;

    newEnc = ypStringLib_ENC(s);
    if (newEnc->sizeshift < x_enc->sizeshift) newEnc = x_enc;

    if (ypStringLib_ALLOCLEN(s) - 1 < newLen || ypStringLib_ENC(s) != newEnc) {
        // Recall _ypStringLib_grow_onextend adjusts alloclen and enc.
        // TODO Overallocate?
        ypObject *result = _ypStringLib_grow_onextend(s, newLen, extra, newEnc);
        if (yp_isexceptionC(result)) return result;
    }

    newEnc->setindexX(ypStringLib_DATA(s), newLen - 1, x);

    ypStringLib_SET_LEN(s, newLen);
    return yp_None;
}

// Extends s with the items yielded from x, updating the length. Never writes the null-terminator.
// XXX Do "s[len(s)]=0" when this returns (even on error)
// XXX In Python, if there's an error in the bytearray.extend iterator, the bytearray is unchanged,
// even if valid values were yielded before the error. But Python is inconsistent: list.extend,
// set.update, and dict.update all contain the values yielded from before the error. And
// bytearray.extend achieves this by allocating a temporary bytearray behind the scenes, which is an
// extra allocation we'd like to avoid. So we are intentially different than Python here.
static ypObject *_ypStringLib_extend_fromiter(ypObject *s, ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject                  *x;
    ypStringLib_asitemCfunc    asitem = ypStringLib_ASITEM_FUNC(s);
    ypObject                  *result = yp_None;
    yp_uint32_t                x_asitem;
    const ypStringLib_encinfo *x_enc;
    yp_ssize_t length_hint = yp_miniiter_length_hintC(mi, mi_state, &result);  // zero on error

    while (1) {
        x = yp_miniiter_next(mi, mi_state);  // new ref
        if (yp_isexceptionC(x)) {
            if (yp_isexceptionC2(x, yp_StopIteration)) break;
            return x;
        }
        result = asitem(x, &x_asitem, &x_enc);
        yp_decref(x);
        if (yp_isexceptionC(result)) return result;
        if (length_hint > 0) length_hint -= 1;

        result = ypStringLib_push(s, x_asitem, x_enc, length_hint);
        if (yp_isexceptionC(result)) return result;
    }

    return yp_None;
}

// Extends s with the items yielded from iterable; always writes the null-terminator
static ypObject *ypStringLib_extend_fromiterable(ypObject *s, ypObject *iterable)
{
    ypObject   *result;
    yp_uint64_t mi_state;
    ypObject   *mi;

    yp_ASSERT(!ypStringLib_TYPE_CHECK(iterable),
            "fellow string passed to ypStringLib_extend_fromiterable");

    mi = yp_miniiter(iterable, &mi_state);  // new ref
    if (yp_isexceptionC(mi)) return mi;
    result = _ypStringLib_extend_fromiter(s, mi, &mi_state);
    ypStringLib_ENC(s)->setindexX(ypStringLib_DATA(s), ypStringLib_LEN(s), 0);
    yp_decref(mi);
    ypStringLib_ASSERT_INVARIANTS(s);
    return result;
}

// Extends s with the contents of x, a fellow string object; always writes the null-terminator. s
// and x can be the same object.
static ypObject *ypStringLib_extend_fromstring(ypObject *s, ypObject *x)
{
    yp_ssize_t                 x_len = ypStringLib_LEN(x);
    const ypStringLib_encinfo *x_enc = ypStringLib_ENC(x);
    yp_ssize_t                 newLen;
    const ypStringLib_encinfo *newEnc;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypObject_TYPE_PAIR_CODE(x),
            "missed a yp_TypeError check");

    if (ypStringLib_LEN(s) > ypStringLib_LEN_MAX - x_len) {
        return yp_MemorySizeOverflowError;
    }
    newLen = ypStringLib_LEN(s) + x_len;

    newEnc = ypStringLib_ENC(s);
    if (newEnc->sizeshift < x_enc->sizeshift) newEnc = x_enc;

    if (ypStringLib_ALLOCLEN(s) - 1 < newLen || ypStringLib_ENC(s) != newEnc) {
        // Recall _ypStringLib_grow_onextend adjusts alloclen and enc.
        // TODO Overallocate?
        ypObject *result = _ypStringLib_grow_onextend(s, newLen, 0, newEnc);
        if (yp_isexceptionC(result)) return result;
    }

    if (s == x) {
        // ypStringLib_elemcopy_maybeupconvert asserts when src and dest are the same object.
        yp_uint8_t *data = ypStringLib_DATA(s);
        yp_ssize_t  lenBytes = ypStringLib_LEN(s) << newEnc->sizeshift;
        yp_memcpy(data + lenBytes, data, lenBytes);
    } else {
        ypStringLib_elemcopy_maybeupconvert(newEnc->sizeshift, ypStringLib_DATA(s),
                ypStringLib_LEN(s), x_enc->sizeshift, ypStringLib_DATA(x), 0, x_len);
    }
    newEnc->setindexX(ypStringLib_DATA(s), newLen, 0);

    ypStringLib_SET_LEN(s, newLen);
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}

// Concatenates s with the items yielded from iterable, returning the new string.
static ypObject *ypStringLib_concat_fromiterable(ypObject *s, ypObject *iterable)
{
    ypObject *newS;
    ypObject *result;

    yp_ASSERT(!ypStringLib_TYPE_CHECK(iterable),
            "fellow string passed to ypStringLib_concat_fromiterable");

    // TODO Allocate newS a little larger to consider lengthhint?
    newS = ypStringLib_new_copy(ypObject_TYPE_CODE(s), s, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(newS)) return newS;

    result = ypStringLib_extend_fromiterable(newS, iterable);
    if (yp_isexceptionC(result)) {
        yp_decref(newS);
        return result;
    }
    return newS;
}

// Concatenates s and x, a fellow string object, retuning the new string. s and x can be the same
// object.
static ypObject *ypStringLib_concat_fromstring(ypObject *s, ypObject *x)
{
    yp_ssize_t                 newS_len;
    int                        newS_enc_code;
    const ypStringLib_encinfo *newS_enc;
    ypObject                  *newS;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypObject_TYPE_PAIR_CODE(x),
            "missed a yp_TypeError check");

    // Optimize the case where s or x are empty
    if (ypStringLib_LEN(x) < 1) return ypStringLib_copy(ypObject_TYPE_CODE(s), s);
    if (ypStringLib_LEN(s) < 1) return ypStringLib_copy(ypObject_TYPE_CODE(s), x);

    if (ypStringLib_LEN(s) > ypStringLib_LEN_MAX - ypStringLib_LEN(x)) {
        return yp_MemorySizeOverflowError;
    }
    newS_len = ypStringLib_LEN(s) + ypStringLib_LEN(x);
    // TODO Some places take the max code, some places look explicitly at elemsize. Be consistent!
    newS_enc_code = MAX(ypStringLib_ENC_CODE(s), ypStringLib_ENC_CODE(x));
    newS_enc = &(ypStringLib_encs[newS_enc_code]);
    newS = _ypStringLib_new(ypObject_TYPE_CODE(s), newS_len, /*alloclen_fixed=*/TRUE, newS_enc);
    if (yp_isexceptionC(newS)) return newS;

    ypStringLib_elemcopy_maybeupconvert(newS_enc->sizeshift, ypStringLib_DATA(newS), 0,
            ypStringLib_ENC(s)->sizeshift, ypStringLib_DATA(s), 0, ypStringLib_LEN(s));
    ypStringLib_elemcopy_maybeupconvert(newS_enc->sizeshift, ypStringLib_DATA(newS),
            ypStringLib_LEN(s), ypStringLib_ENC(x)->sizeshift, ypStringLib_DATA(x), 0,
            ypStringLib_LEN(x) + 1);  // incl null
    ypStringLib_SET_LEN(newS, newS_len);
    ypStringLib_ASSERT_INVARIANTS(newS);
    return newS;
}

static ypObject *ypStringLib_clear(ypObject *s)
{
    const ypStringLib_encinfo *newEnc = ypObject_TYPE_PAIR_CODE(s) == ypBytes_CODE ?
                                                ypStringLib_enc_bytes :
                                                ypStringLib_enc_latin_1;
    yp_ASSERT(ypObject_IS_MUTABLE(s), "clear called on immutable object");
    ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR3(
            s, ypStringLibObject, ypStringLib_ALLOCLEN_MAX, newEnc->elemsize);
    yp_ASSERT(ypStringLib_DATA(s) == ypStringLib_INLINE_DATA(s),
            "ypStringLib_clear didn't allocate inline!");
    yp_ASSERT(ypStringLib_ALLOCLEN(s) >= 1, "bytes/str inlinelen must be at least 1");
    ypStringLib_ENC_CODE(s) = newEnc->code;
    newEnc->setindexX(ypStringLib_DATA(s), 0, 0);
    ypStringLib_SET_LEN(s, 0);
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}

static ypObject *ypStringLib_repeat(ypObject *s, yp_ssize_t factor)
{
    yp_ssize_t                 s_len = ypStringLib_LEN(s);
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);
    yp_ssize_t                 newLen;
    ypObject                  *newS;

    ypStringLib_ASSERT_INVARIANTS(s);

    if (s_len < 1 || factor < 1) return ypStringLib_new_empty(ypObject_TYPE_CODE(s));
    if (factor == 1) return ypStringLib_copy(ypObject_TYPE_CODE(s), s);

    if (factor > ypStringLib_LEN_MAX / s_len) return yp_MemorySizeOverflowError;
    newLen = s_len * factor;
    newS = _ypStringLib_new(ypObject_TYPE_CODE(s), newLen, /*alloclen_fixed=*/TRUE, s_enc);
    if (yp_isexceptionC(newS)) return newS;

    ypStringLib_MEMCPY(s_enc->sizeshift, ypStringLib_DATA(newS), 0, ypStringLib_DATA(s), 0, s_len);
    _ypSequence_repeat_memcpy(ypStringLib_DATA(newS), factor, s_len << s_enc->sizeshift);
    s_enc->setindexX(ypStringLib_DATA(newS), newLen, 0);
    ypStringLib_SET_LEN(newS, newLen);

    ypStringLib_ASSERT_INVARIANTS(newS);
    return newS;
}

static ypObject *_ypStringLib_getslice_total_reversed(ypObject *s)
{
    yp_ssize_t                 s_len = ypStringLib_LEN(s);
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);
    ypObject                  *newS;
    yp_ssize_t                 i;

    yp_ASSERT(s_len > 0, "missed an 'empty slice' case");

    newS = _ypStringLib_new(ypObject_TYPE_CODE(s), s_len, /*alloclen_fixed=*/TRUE, s_enc);
    if (yp_isexceptionC(newS)) return newS;

    for (i = 0; i < s_len; i++) {
        yp_uint32_t s_char = s_enc->getindexX(ypStringLib_DATA(s), i);
        s_enc->setindexX(ypStringLib_DATA(newS), s_len - 1 - i, s_char);
    }
    s_enc->setindexX(ypStringLib_DATA(newS), s_len, '\0');

    ypStringLib_ENC_CODE(newS) = s_enc->code;
    ypStringLib_SET_LEN(newS, s_len);
    ypStringLib_ASSERT_INVARIANTS(newS);
    return newS;
}

static ypObject *ypStringLib_getslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step)
{
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);
    ypObject                  *result;
    const ypStringLib_encinfo *newS_enc;
    yp_ssize_t                 newLen;
    ypObject                  *newS;

    result = ypSlice_AdjustIndicesC(ypStringLib_LEN(s), &start, &stop, &step, &newLen);
    if (yp_isexceptionC(result)) return result;

    if (newLen < 1) return ypStringLib_new_empty(ypObject_TYPE_CODE(s));
    if (newLen >= ypStringLib_LEN(s)) {
        if (step == 1) return ypStringLib_copy(ypObject_TYPE_CODE(s), s);
        yp_ASSERT(step == -1, "unexpected step %" PRIssize " for a total slice", step);
        return _ypStringLib_getslice_total_reversed(s);
    }

    newS_enc = ypStringLib_checkenc_getslice(s, start, stop, step, newLen);
    newS = _ypStringLib_new(ypObject_TYPE_CODE(s), newLen, /*alloclen_fixed=*/TRUE, newS_enc);
    if (yp_isexceptionC(newS)) return newS;

    ypStringLib_elemcopy_maybedownconvert_getslice(newS_enc->sizeshift, ypStringLib_DATA(newS), 0,
            s_enc->sizeshift, ypStringLib_DATA(s), start, step, newLen);
    newS_enc->setindexX(ypStringLib_DATA(newS), newLen, '\0');
    ypStringLib_SET_LEN(newS, newLen);
    ypStringLib_ASSERT_INVARIANTS(newS);
    return newS;
}

// Determines the new encoding for s after the setslice operation.
static const ypStringLib_encinfo *_ypStringLib_setslice_newEnc(ypObject *s, yp_ssize_t start,
        yp_ssize_t stop, yp_ssize_t step, yp_ssize_t slicelength, const ypStringLib_encinfo *x_enc)
{
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength);
    yp_ASSERT(slicelength < ypStringLib_LEN(s), "missed a 'total slice' optimization");
    // TODO Assert that x_enc matches s?

    // The resulting encoding for s will be at least big enough to hold x's characters. It may even
    // be larger than that if the remaining characters in s are larger than x's.
    if (x_enc->sizeshift >= s_enc->sizeshift) {
        // x is the same or larger than any character in s, so that's our encoding.
        return x_enc;
    } else if (slicelength < 1) {
        // No characters are being deleted from s, which is larger than x, so that's our encoding.
        return s_enc;
    } else {
        // Characters are being deleted from s, so take the larger of x and the remaining chars.
        const ypStringLib_encinfo *remainingEnc =
                ypStringLib_checkenc_delslice(s, start, stop, step, slicelength);
        if (x_enc->sizeshift >= remainingEnc->sizeshift) {
            return x_enc;
        } else {
            return remainingEnc;
        }
    }
}

// Called for a setslice where all of s's characters are replaced.
static ypObject *_ypStringLib_setslice_total(ypObject *s, yp_ssize_t step, void *x_data,
        yp_ssize_t x_len, const ypStringLib_encinfo *x_enc)
{
    void *oldptr;

    yp_ASSERT(step == 1 || step == -1, "unexpected step %" PRIssize " for a total slice", step);
    yp_ASSERT(step == 1 || ypStringLib_LEN(s) == x_len,
            "missed an 'extended slices can't grow' check");
    // TODO Assert that x_enc matches s?

    // s may need to be reallocated to fit x. But we can immediately discard the old buffer. Recall
    // _ypStringLib_maybe_realloc adjusts alloclen for the new encoding.
    // TODO Overallocate?
    oldptr = _ypStringLib_maybe_realloc(s, x_len, 0, x_enc);
    if (oldptr == NULL) return yp_MemoryError;
    if (ypStringLib_DATA(s) != oldptr) {
        ypMem_REALLOC_CONTAINER_FREE_OLDPTR(s, ypStringLibObject, oldptr);
    }

    if (step == 1) {
        // XXX Recall that x_data is not necessarily null-terminated!
        ypStringLib_MEMCPY(x_enc->sizeshift, ypStringLib_DATA(s), 0, x_data, 0, x_len);
    } else {
        yp_ssize_t i;
        yp_ASSERT1(step == -1);  // also asserted above
        for (i = 0; i < x_len; i++) {
            yp_uint32_t x_char = x_enc->getindexX(x_data, i);
            x_enc->setindexX(ypStringLib_DATA(s), x_len - 1 - i, x_char);
        }
    }
    x_enc->setindexX(ypStringLib_DATA(s), x_len, '\0');

    ypStringLib_ENC_CODE(s) = x_enc->code;
    ypStringLib_SET_LEN(s, x_len);
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}

// Called for a regular setslice (step==1).
static ypObject *_ypStringLib_setslice_regular(ypObject *s, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t slicelength, void *x_data, yp_ssize_t x_len, const ypStringLib_encinfo *x_enc)
{
    // Note that -len(s)<=growBy<=len(x), so the growBy calculation can't overflow
    yp_ssize_t                 growBy = x_len - slicelength;  // negative means array shrinking
    const ypStringLib_encinfo *oldEnc = ypStringLib_ENC(s);
    const ypStringLib_encinfo *newEnc =
            _ypStringLib_setslice_newEnc(s, start, stop, 1, slicelength, x_enc);
    yp_ssize_t newLen;
    void      *oldptr;

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, (yp_ssize_t)1, slicelength);
    yp_ASSERT(slicelength < ypStringLib_LEN(s), "missed a 'total slice' optimization");
    // TODO Assert that x_enc matches s?

    // Ensure there's enough space allocated
    if (ypStringLib_LEN(s) > ypStringLib_LEN_MAX - growBy) return yp_MemorySizeOverflowError;
    newLen = ypStringLib_LEN(s) + growBy;

    if (oldEnc == newEnc) {
        // Recall _ypStringLib_maybe_realloc adjusts alloclen for the new encoding.
        // TODO Overallocate?
        oldptr = _ypStringLib_maybe_realloc(s, newLen, 0, newEnc);
        if (oldptr == NULL) return yp_MemoryError;

        if (ypStringLib_DATA(s) == oldptr) {
            if (growBy != 0) {
                // elemmove adjusts the null terminator.
                ypStringLib_ELEMMOVE(s, stop + growBy, stop);  // memmove: data overlaps
            }
        } else {
            // The data doesn't overlap, so use memcpy. The second memcpy will include the null
            // terminator (note the +1 to length).
            ypStringLib_MEMCPY(newEnc->sizeshift, ypStringLib_DATA(s), 0, oldptr, 0, start);
            ypStringLib_MEMCPY(newEnc->sizeshift, ypStringLib_DATA(s), stop + growBy, oldptr, stop,
                    ypStringLib_LEN(s) - stop + 1);
            ypMem_REALLOC_CONTAINER_FREE_OLDPTR(s, ypStringLibObject, oldptr);
        }
    } else {
        // TODO A version of ypStringLib_elemcopy that always converts in either direction? Or
        // returns the specific function pointer for converting between the two (so we don't have
        // to evaluate the encodings each time).
        void (*elemcopy)(int, void *, yp_ssize_t, int, const void *, yp_ssize_t, yp_ssize_t) =
                oldEnc->elemsize < newEnc->elemsize ? ypStringLib_elemcopy_maybeupconvert :
                                                      ypStringLib_elemcopy_maybedownconvert;

        // When the encoding is changing, there may be a few corner cases where the buffer can be
        // efficiently reused, but this doesn't seem worth the work right now. Recall
        // ypMem_REALLOC_CONTAINER_VARIABLE_NEW adjusts alloclen for the new encoding.
        // TODO Overallocate?
        oldptr = ypMem_REALLOC_CONTAINER_VARIABLE_NEW5(
                s, ypStringLibObject, newLen + 1, 0, ypStringLib_ALLOCLEN_MAX, newEnc->elemsize);
        if (oldptr == NULL) return yp_MemoryError;

        // The second elemcopy will include the null terminator (note the +1 to length).
        elemcopy(newEnc->sizeshift, ypStringLib_DATA(s), 0, oldEnc->sizeshift, oldptr, 0, start);
        elemcopy(newEnc->sizeshift, ypStringLib_DATA(s), stop + growBy, oldEnc->sizeshift, oldptr,
                stop, ypStringLib_LEN(s) - stop + 1);
        ypMem_REALLOC_CONTAINER_FREE_OLDPTR(s, ypStringLibObject, oldptr);
    }

    // There are now len(x) items starting at s[start] waiting for x's data. Recall that newEnc must
    // be the same or larger as x_enc, so use maybeupconvert.
    ypStringLib_elemcopy_maybeupconvert(
            newEnc->sizeshift, ypStringLib_DATA(s), start, x_enc->sizeshift, x_data, 0, x_len);

    ypStringLib_ENC_CODE(s) = newEnc->code;
    ypStringLib_SET_LEN(s, newLen);
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}

// Called for an extended setslice (step!=1).
static ypObject *_ypStringLib_setslice_extended(ypObject *s, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step, void *x_data, yp_ssize_t x_len, const ypStringLib_encinfo *x_enc)
{
    const ypStringLib_encinfo *oldEnc = ypStringLib_ENC(s);
    const ypStringLib_encinfo *newEnc =
            _ypStringLib_setslice_newEnc(s, start, stop, step, x_len, x_enc);
    yp_ssize_t i;

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, x_len);
    yp_ASSERT(step != 1, "call _ypStringLib_setslice_regular for step == 1");
    yp_ASSERT(x_len < ypStringLib_LEN(s), "missed a 'total slice' optimization");
    // TODO Assert that x_enc matches s?

    // If our encoding is growing, grow it before we copy in the elements of x, to ensure they will
    // fit. Larger encodings may need a larger buffer, so an allocation may be necessary.
    // TODO This will copy over characters that will be immediately replaced by x's characters.
    // A possible optimization here could be skipping those characters. (Then again, a bulk "all
    // characters" operation may be more efficient than skipping them.)
    if (oldEnc->elemsize < newEnc->elemsize) {
        // Recall _ypStringLib_maybe_realloc adjusts alloclen for the new encoding.
        // TODO Overallocate?
        void *oldptr = _ypStringLib_maybe_realloc(s, ypStringLib_LEN(s), 0, newEnc);
        if (oldptr == NULL) return yp_MemoryError;

        // Add one to the lengths below to include the hidden null-terminator.
        if (ypStringLib_DATA(s) == oldptr) {
            ypStringLib_inplace_upconvert(newEnc->sizeshift, oldEnc->sizeshift, ypStringLib_DATA(s),
                    ypStringLib_LEN(s) + 1);
        } else {
            ypStringLib_elemcopy_maybeupconvert(newEnc->sizeshift, ypStringLib_DATA(s), 0,
                    oldEnc->sizeshift, oldptr, 0, ypStringLib_LEN(s) + 1);
            ypMem_REALLOC_CONTAINER_FREE_OLDPTR(s, ypStringLibObject, oldptr);
        }
    }

    // s is now large enough to hold x's characters, so copy them in.
    for (i = 0; i < x_len; i++) {
        yp_uint32_t x_char = x_enc->getindexX(x_data, i);
        newEnc->setindexX(ypStringLib_DATA(s), ypSlice_INDEX(start, step, i), x_char);
    }

    // If our encoding is shrinking, shrink it after we copy in the elements of x, because the
    // characters we just removed from s must be the only characters that required the larger
    // encoding. Smaller encodings can always reuse the same buffer.
    // TODO We copy over x's characters, only to copy them again into the new encoding. A possible
    // optimization could be to allow ypStringLib_inplace_downconvert to silently truncate data
    // (it asserts on debug builds, currently), so we could do the target conversion first. But then
    // we'd still be copying characters unnecessarily, see the related TODO above.
    if (oldEnc->elemsize > newEnc->elemsize) {
        yp_ssize_t newAlloclen = ypStringLib_ALLOCLEN(s) << (oldEnc->sizeshift - newEnc->sizeshift);
        ypStringLib_SET_ALLOCLEN(s, newAlloclen);
        // Add one to the length to include the hidden null-terminator.
        ypStringLib_inplace_downconvert(
                newEnc->sizeshift, oldEnc->sizeshift, ypStringLib_DATA(s), ypStringLib_LEN(s) + 1);
    }

    ypStringLib_ENC_CODE(s) = newEnc->code;
    // The length of s does not change for extended setslices.
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}

static ypObject *ypStringLib_delslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step);
// XXX x is not necessarily null-terminated; for example, bytearray_insert calls us with a pointer
// to a yp_uint8_t.
// XXX s and x must _not_ be the same object (pass a copy of x if so).
static ypObject *ypStringLib_setslice_fromstring7(ypObject *s, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step, void *x_data, yp_ssize_t x_len, const ypStringLib_encinfo *x_enc)
{
    ypObject  *result;
    yp_ssize_t slicelength;

    yp_ASSERT(ypStringLib_DATA(s) != x_data, "make a copy of x when s is x");
    // TODO Assert that x_enc matches s?

    // XXX Python is inconsistent here: `s[::5] = b''` is allowed for bytearray but not for list.
    // I'm breaking from Python and choosing to always make this an error.
    if (step == 1 && x_len < 1) {
        return ypStringLib_delslice(s, start, stop, step);
    }

    result = ypSlice_AdjustIndicesC(ypStringLib_LEN(s), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    // Extended slices (step!=1) cannot change the length of s.
    if (step != 1 && x_len != slicelength) return yp_ValueError;

    // TODO Should we add a special case for extend/etc, OR should extend/etc call setslice?
    if (slicelength >= ypStringLib_LEN(s)) {
        // The "total slice" case is easier because we can ignore the current characters of s.
        return _ypStringLib_setslice_total(s, step, x_data, x_len, x_enc);
    } else if (step == 1) {
        return _ypStringLib_setslice_regular(s, start, stop, slicelength, x_data, x_len, x_enc);
    } else {
        return _ypStringLib_setslice_extended(s, start, stop, step, x_data, x_len, x_enc);
    }
}

// s and x can be the same object (a copy is made internally).
static ypObject *ypStringLib_setslice_fromstring(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, ypObject *x)
{
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypObject_TYPE_PAIR_CODE(x),
            "missed a yp_TypeError check");

    if (s == x) {
        // If x is the same object as s, we need a copy of the data.
        yp_ssize_t                 x_len = ypStringLib_LEN(x);
        const ypStringLib_encinfo *x_enc = ypStringLib_ENC(x);
        yp_ssize_t                 x_lenBytes = x_len << x_enc->sizeshift;
        yp_ssize_t                 size;  // ignored
        ypObject                  *result;

        void *x_copy = yp_malloc(&size, x_lenBytes);
        if (x_copy == NULL) return yp_MemoryError;
        yp_memcpy(x_copy, ypStringLib_DATA(x), x_lenBytes);

        result = ypStringLib_setslice_fromstring7(s, start, stop, step, x_copy, x_len, x_enc);

        yp_free(x_copy);
        return result;
    }

    return ypStringLib_setslice_fromstring7(
            s, start, stop, step, ypStringLib_DATA(x), ypStringLib_LEN(x), ypStringLib_ENC(x));
}

// A version of ypStringLib_delslice that takes adjusted slices.
// XXX Handle the "empty slice" and "total slice" cases first.
static ypObject *_ypStringLib_delslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, yp_ssize_t slicelength)
{
    const ypStringLib_encinfo *oldEnc = ypStringLib_ENC(s);
    const ypStringLib_encinfo *newEnc;

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, step, slicelength);
    yp_ASSERT(slicelength > 0, "missed an 'empty slice' optimization");
    yp_ASSERT(slicelength < ypStringLib_LEN(s), "missed a 'total slice' optimization");

    // Add one to the length to include the hidden null-terminator
    _ypSlice_delslice_memmove(ypStringLib_DATA(s), ypStringLib_LEN(s) + 1, oldEnc->elemsize, start,
            stop, step, slicelength);

    // TODO We move around the remaining characters in _ypSlice_delslice_memmove, only to move them
    // again in ypStringLib_inplace_downconvert. A possible optimization could be a version of
    // ypStringLib_inplace_downconvert that works like _ypSlice_delslice_memmove.
    newEnc = ypStringLib_checkenc(s);
    yp_ASSERT(oldEnc->elemsize >= newEnc->elemsize, "unexpected result from ypStringLib_checkenc");
    if (oldEnc != newEnc) {
        // XXX This could end up keeping 3x as much memory as needed (ucs-4 to latin-1). In general
        // we only reallocate when necessary (i.e. when growing) or easy (i.e. when clearing). But
        // perhaps we should establish a threshold here (and other str places) where if there's too
        // much wasted space we free some memory. (set/dict kinda does the same, but maybe for
        // different reasons.)
        yp_ssize_t newAlloclen = ypStringLib_ALLOCLEN(s) << (oldEnc->sizeshift - newEnc->sizeshift);
        ypStringLib_SET_ALLOCLEN(s, newAlloclen);
        // Add one to the length to include the hidden null-terminator
        ypStringLib_inplace_downconvert(newEnc->sizeshift, ypStringLib_ENC(s)->sizeshift,
                ypStringLib_DATA(s), ypStringLib_LEN(s) + 1);
    }

    ypStringLib_ENC_CODE(s) = newEnc->code;
    ypStringLib_SET_LEN(s, ypStringLib_LEN(s) - slicelength);
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}

static ypObject *ypStringLib_delslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step)
{
    ypObject  *result;
    yp_ssize_t slicelength;

    result = ypSlice_AdjustIndicesC(ypStringLib_LEN(s), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;
    if (slicelength < 1) return yp_None;  // no-op
    if (slicelength >= ypStringLib_LEN(s)) return ypStringLib_clear(s);

    return _ypStringLib_delslice(s, start, stop, step, slicelength);
}

// Helper function for bytes_find and str_find. The string to find (x_data and x_len) must have
// already been coerced into the same encoding as s.
// TODO Replace with the faster find implementation from Python.
static yp_ssize_t _ypStringLib_find(ypObject *s, void *x_data, yp_ssize_t x_len, yp_ssize_t start,
        yp_ssize_t slicelength, findfunc_direction direction)
{
    int         sizeshift = ypStringLib_ENC(s)->sizeshift;
    yp_uint8_t *s_data = ypStringLib_DATA(s);
    yp_ssize_t  stop = start + slicelength;
    yp_ssize_t  step;
    yp_ssize_t  s_rlen;   // Remaining length.
    yp_uint8_t *s_rdata;  // Remaining data.

    ypSlice_ASSERT_ADJUSTED_INDICES(start, stop, (yp_ssize_t)1, slicelength);

    if (direction == yp_FIND_FORWARD) {
        s_rdata = s_data + (start << sizeshift);
        step = 1;
    } else {
        s_rdata = s_data + ((stop - x_len) << sizeshift);
        step = -1;
    }

    s_rlen = stop - start;
    while (s_rlen >= x_len) {
        if (yp_memcmp(s_rdata, x_data, x_len << sizeshift) == 0) {
            return (s_rdata - s_data) >> sizeshift;
        }
        s_rdata += (step << sizeshift);
        s_rlen--;
    }
    return -1;
}

// Helper function for bytes_count and str_count. The string to find (x_data and x_len) must have
// already been coerced into the same encoding as s.
static yp_ssize_t _ypStringLib_count(
        ypObject *s, void *x_data, yp_ssize_t x_len, yp_ssize_t start, yp_ssize_t slicelength)
{
    yp_ssize_t s_rstart;  // Start of remaining slice.
    yp_ssize_t s_rlen;    // Remaining length.
    yp_ssize_t n;

    ypSlice_ASSERT_ADJUSTED_INDICES(start, start + slicelength, (yp_ssize_t)1, slicelength);

    // The empty string "matches" every position, including the end of the slice.
    if (x_len < 1) {
        return slicelength + 1;
    }

    // Do the counting.
    s_rstart = start;
    s_rlen = slicelength;
    n = 0;
    while (s_rlen >= x_len) {
        yp_ssize_t i = _ypStringLib_find(s, x_data, x_len, s_rstart, s_rlen, yp_FIND_FORWARD);
        if (i < 0) break;  // x does not exist in the remainder of s.

        n += 1;
        s_rstart = i + x_len;  // We count non-overlapping substrings.
        s_rlen = slicelength - (s_rstart - start);
    }
    return n;
}

static ypObject *ypStringLib_irepeat(ypObject *s, yp_ssize_t factor)
{
    yp_ssize_t                 s_len = ypStringLib_LEN(s);
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);
    yp_ssize_t                 newLen;

    yp_ASSERT(ypObject_IS_MUTABLE(s), "irepeat called on immutable object");
    if (s_len < 1 || factor == 1) return yp_None;  // no-op
    if (factor < 1) return ypStringLib_clear(s);

    if (factor > ypStringLib_LEN_MAX / s_len) return yp_MemorySizeOverflowError;
    newLen = s_len * factor;
    if (ypStringLib_ALLOCLEN(s) - 1 < newLen) {
        // TODO Overallocate?
        ypObject *result = _ypStringLib_grow_onextend(s, newLen, 0, s_enc);
        if (yp_isexceptionC(result)) return result;
    }

    _ypSequence_repeat_memcpy(ypStringLib_DATA(s), factor, s_len << s_enc->sizeshift);
    s_enc->setindexX(ypStringLib_DATA(s), newLen, 0);

    ypStringLib_SET_LEN(s, newLen);
    ypStringLib_ASSERT_INVARIANTS(s);
    return yp_None;
}


// There are some efficiencies we can exploit if iterable/x is a fellow string object
// TODO Is this really a scenario for which we should be optimizing? How typical is ''.join('')?
static ypObject *_ypStringLib_join_fromstring(ypObject *s, ypObject *x)
{
    yp_ssize_t                 s_len = ypStringLib_LEN(s);
    const ypStringLib_encinfo *s_enc = ypStringLib_ENC(s);
    void                      *x_data = ypStringLib_DATA(x);
    yp_ssize_t                 x_len = ypStringLib_LEN(x);
    const ypStringLib_encinfo *x_enc = ypStringLib_ENC(x);
    yp_ssize_t                 i;
    void                      *result_data;
    yp_ssize_t                 result_len;
    int                        result_enc_code;
    const ypStringLib_encinfo *result_enc;
    ypObject                  *result;

    ypStringLib_ASSERT_INVARIANTS(s);
    ypStringLib_ASSERT_INVARIANTS(x);
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypObject_TYPE_PAIR_CODE(x),
            "missed a yp_TypeError check");

    if (x_len < 1) return ypStringLib_new_empty(ypObject_TYPE_CODE(s));
    if (s_len < 1 || x_len == 1) return ypStringLib_copy(ypObject_TYPE_CODE(s), x);

    // Calculate how long the result is going to be and which encoding we'll use
    if (s_len > (ypStringLib_LEN_MAX - x_len) / (x_len - 1)) {
        return yp_MemorySizeOverflowError;
    }
    result_len = (s_len * (x_len - 1)) + x_len;
    result_enc_code = MAX(ypStringLib_ENC_CODE(s), ypStringLib_ENC_CODE(x));

    // Now we can create the result object...
    result_enc = &(ypStringLib_encs[result_enc_code]);
    result = _ypStringLib_new(
            ypObject_TYPE_CODE(s), result_len, /*alloclen_fixed=*/TRUE, result_enc);
    if (yp_isexceptionC(result)) return result;

    // ...and populate it, remembering to null-terminate and update the length
    result_data = ypStringLib_DATA(result);
    ypStringLib_elemcopy_maybeupconvert(
            result_enc->sizeshift, result_data, 1, s_enc->sizeshift, ypStringLib_DATA(s), 0, s_len);
    _ypSequence_repeat_memcpy(result_data, x_len - 1, (s_len + 1) << result_enc->sizeshift);
    for (i = 0; i < x_len; i++) {
        result_enc->setindexX(result_data, i * (s_len + 1), x_enc->getindexX(x_data, i));
    }
    result_enc->setindexX(result_data, result_len, 0);

    ypStringLib_SET_LEN(result, result_len);
    ypStringLib_ASSERT_INVARIANTS(result);
    return result;
}

// Performs the necessary elemcopy's on behalf of ypStringLib_join, null-terminates, and updates
// result's length. Assumes adequate space has already been allocated, and that type checking on
// seq's elements has already been performed.
static void _ypStringLib_join_elemcopy(
        ypObject *result, ypObject *s, const ypQuickSeq_methods *seq, ypQuickSeq_state *state)
{
    const ypStringLib_encinfo *result_enc = ypStringLib_ENC(result);
    void                      *result_data = ypStringLib_DATA(result);
    yp_ssize_t                 result_len = 0;
    yp_ssize_t                 s_len = ypStringLib_LEN(s);
    yp_ssize_t                 i;

    if (s_len < 1) {
        // The separator is empty, so we just concatenate seq's elements
        for (i = 0; /*stop at NULL*/; i++) {
            ypObject *x = seq->getindexX(state, i);  // borrowed
            if (x == NULL) break;
            yp_ASSERT(ypStringLib_TYPE_CHECK(x), "ypStringLib_join didn't perform type checking");
            ypStringLib_ASSERT_INVARIANTS(x);
            ypStringLib_elemcopy_maybeupconvert(result_enc->sizeshift, result_data, result_len,
                    ypStringLib_ENC(x)->sizeshift, ypStringLib_DATA(x), 0, ypStringLib_LEN(x));
            result_len += ypStringLib_LEN(x);
        }

    } else {
        // We need to insert the separator between seq's elements; recall that we know there is at
        // least one element in seq
        int       s_sizeshift = ypStringLib_ENC(s)->sizeshift;
        void     *s_data = ypStringLib_DATA(s);
        ypObject *x = seq->getindexX(state, 0);  // borrowed
        yp_ASSERT(x != NULL, "_ypStringLib_join_elemcopy passed an empty seq");
        for (i = 1; /*stop at NULL*/; i++) {
            yp_ASSERT(ypStringLib_TYPE_CHECK(x), "ypStringLib_join didn't perform type checking");
            ypStringLib_elemcopy_maybeupconvert(result_enc->sizeshift, result_data, result_len,
                    ypStringLib_ENC(x)->sizeshift, ypStringLib_DATA(x), 0, ypStringLib_LEN(x));
            result_len += ypStringLib_LEN(x);
            x = seq->getindexX(state, i);  // borrowed
            if (x == NULL) break;
            ypStringLib_elemcopy_maybeupconvert(
                    result_enc->sizeshift, result_data, result_len, s_sizeshift, s_data, 0, s_len);
            result_len += s_len;
        }
    }

    // Null-terminate and update the length
    result_enc->setindexX(result_data, result_len, 0);
    ypStringLib_SET_LEN(result, result_len);
}

// XXX The object underlying seq must be guaranteed to return the same object per index. So, to be
// safe, convert any non-built-ins to a tuple.
// TODO Alternatively: join must work with str subclasses, which might override __str__ with funky
// behaviour. So have a version of this that works with "tuple of str" (or "va_list of str"), and
// a separate generic version that works with anything, i.e. any iterable of "any" type (where
// "any" is any str subclass).
static ypObject *ypStringLib_join(
        ypObject *s, const ypQuickSeq_methods *seq, ypQuickSeq_state *state)
{
    ypObject                  *exc = yp_None;
    unsigned                   s_pair = ypObject_TYPE_PAIR_CODE(s);
    yp_ssize_t                 seq_len;
    yp_ssize_t                 i;
    ypObject                  *x;
    yp_ssize_t                 result_len;
    int                        result_enc_code;
    const ypStringLib_encinfo *result_enc;
    ypObject                  *result;

    ypStringLib_ASSERT_INVARIANTS(s);

    seq_len = seq->len(state, &exc);
    if (yp_isexceptionC(exc)) return exc;
    if (seq_len < 1) return ypStringLib_new_empty(ypObject_TYPE_CODE(s));
    if (seq_len == 1) {
        x = seq->getindexX(state, 0);  // borrowed
        if (ypObject_TYPE_PAIR_CODE(x) != s_pair) return_yp_BAD_TYPE(x);
        return ypStringLib_copy(ypObject_TYPE_CODE(s), x);
    }

    // Calculate how long the result is going to be, which encoding we'll use, and ensure seq
    // contains the correct types
    if (ypStringLib_LEN(s) > ypStringLib_LEN_MAX / (seq_len - 1)) {
        return yp_MemorySizeOverflowError;
    }
    result_len = ypStringLib_LEN(s) * (seq_len - 1);
    result_enc_code = ypStringLib_ENC_CODE(s);  // if s is empty the code is Latin-1 or BYTES
    for (i = 0; i < seq_len; i++) {
        x = seq->getindexX(state, i);  // borrowed
        if (ypObject_TYPE_PAIR_CODE(x) != s_pair) return_yp_BAD_TYPE(x);
        if (result_len > ypStringLib_LEN_MAX - ypStringLib_LEN(x)) {
            return yp_MemorySizeOverflowError;
        }
        result_len += ypStringLib_LEN(x);
        if (result_enc_code < ypStringLib_ENC_CODE(x)) {
            result_enc_code = ypStringLib_ENC_CODE(x);
        }
    }

    // It's possible s, and all elements of seq, were empty.
    if (result_len < 1) return ypStringLib_new_empty(ypObject_TYPE_CODE(s));

    // Now we can create the result object and populate it
    result_enc = &(ypStringLib_encs[result_enc_code]);
    result = _ypStringLib_new(
            ypObject_TYPE_CODE(s), result_len, /*alloclen_fixed=*/TRUE, result_enc);
    if (yp_isexceptionC(result)) return result;
    _ypStringLib_join_elemcopy(result, s, seq, state);
    yp_ASSERT(ypStringLib_LEN(result) == result_len,
            "joined result isn't length originally calculated");
    ypStringLib_ASSERT_INVARIANTS(result);
    return result;
}


// Unicode encoding/decoding support functions

// Calls the error handler with appropriate arguments, sets *newPos to the (adjusted) index at
// which encoding should continue, and returns the replacement that should be concatenated onto the
// encoded bytes (using ypStringLib_encode_extend_replacement, perhaps). Returns exception on
// error (*newPos will be undefined).
// XXX Adapted from Python's unicode_encode_call_errorhandler
static ypObject *ypStringLib_encode_call_errorhandler(yp_codecs_error_handler_func_t errorHandler,
        const char *reason, ypObject *encoding, ypObject *source, yp_ssize_t errStart,
        yp_ssize_t errEnd, yp_ssize_t *newPos)
{
    yp_codecs_error_handler_params_t params = {yp_sizeof(yp_codecs_error_handler_params_t)};
    ypObject                        *replacement;
    yp_ssize_t                       source_len = ypStringLib_LEN(source);

    params.exc = yp_UnicodeEncodeError;
    params.encoding = encoding;
    // TODO pass the reason along
    params.source.object = source;
    // TODO We should just give the source object...I'm not sure str support for
    // params.source.data makes sense
    params.source.data.type = ypStringLib_ENC(source)->name;
    params.source.data.len = source_len;
    params.source.data.ptr = ypStringLib_DATA(source);
    params.start = errStart;
    params.end = errEnd;

    replacement = yp_SystemError;  // in case errorhandler forgets to modify replacement
    *newPos = yp_SSIZE_T_MAX;      // ... ditto, for newPos
    errorHandler(&params, &replacement, newPos);  // replacement is a new ref on output
    if (ypObject_TYPE_PAIR_CODE(replacement) != ypBytes_CODE) {
        ypObject *typeErr = yp_BAD_TYPE(replacement);
        yp_decref(replacement);
        return typeErr;
    }

    // XXX Python allows the bytes variable in the exception to be modified and replaced here, but
    // this isn't documented and I'm assuming it's a deprecated feature

    if (*newPos < 0) *newPos = source_len + (*newPos);
    if (newPos < 0 || *newPos > source_len) {
        yp_decref(replacement);
        return yp_IndexError;  // "position %zd from error handler out of bounds"
    }

    return replacement;
}

// future_growth is an upper-bound estimate of the number of additional bytes, not including
// the replacement text, that are expected to be added to encoded. After appending, encoded will
// have at least future_growth space available. Does not null-terminate encoded.
// TODO Rewrite to use ypStringLib_extend_fromstring? (Although, what about future_growth?)
static ypObject *ypStringLib_encode_extend_replacement(
        ypObject *encoded, ypObject *replacement, yp_ssize_t future_growth)
{
    yp_ssize_t  encoded_len;
    yp_ssize_t  replacement_len;
    yp_ssize_t  newLen;
    yp_ssize_t  newAlloclen;
    ypObject   *result;
    yp_uint8_t *encoded_data;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(encoded) == ypBytes_CODE, "encoded must be a bytes");
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(replacement) == ypBytes_CODE, "replacement must be a bytes");
    yp_ASSERT(future_growth >= 0, "future_growth can't be negative");

    encoded_len = ypStringLib_LEN(encoded);
    replacement_len = ypStringLib_LEN(replacement);
    if (replacement_len > ypStringLib_LEN_MAX - encoded_len) {
        return yp_MemorySizeOverflowError;
    }
    newLen = encoded_len + replacement_len;
    if (future_growth > ypStringLib_LEN_MAX - newLen) {
        return yp_MemorySizeOverflowError;
    }
    newAlloclen = newLen + future_growth;
    if (ypStringLib_ALLOCLEN(encoded) < newAlloclen) {
        // Recall _ypStringLib_grow_onextend adjusts alloclen and enc.
        result = _ypStringLib_grow_onextend(encoded, newAlloclen, 0, ypStringLib_enc_bytes);
        if (yp_isexceptionC(result)) return result;
    }

    encoded_data = (yp_uint8_t *)ypStringLib_DATA(encoded);
    yp_memcpy(encoded_data + encoded_len, ypStringLib_DATA(replacement), replacement_len);
    ypStringLib_SET_LEN(encoded, newLen);
    return yp_None;
}

// Calls the error handler with appropriate arguments, sets *newPos to the (adjusted) index at
// which decoding should continue, and returns the replacement that should be concatenated onto the
// decoded string (using ypStringLib_decode_extend_replacement, perhaps). Returns exception on
// error (*newPos will be undefined).
// XXX Adapted from Python's unicode_decode_call_errorhandler_writer
static ypObject *ypStringLib_decode_call_errorhandler(yp_codecs_error_handler_func_t errorHandler,
        const char *reason, ypObject *encoding, const yp_uint8_t *source, yp_ssize_t source_len,
        yp_ssize_t errStart, yp_ssize_t errEnd, yp_ssize_t *newPos)
{
    yp_codecs_error_handler_params_t params = {yp_sizeof(yp_codecs_error_handler_params_t)};
    ypObject                        *replacement;

    params.exc = yp_UnicodeDecodeError;
    params.encoding = encoding;
    // TODO pass the reason along
    // TODO pass the object along, if appropriate?
    params.source.data.type = yp_t_bytes;
    params.source.data.ptr = source;
    params.source.data.len = source_len;
    params.start = errStart;
    params.end = errEnd;

    replacement = yp_SystemError;  // in case errorhandler forgets to modify replacement
    *newPos = yp_SSIZE_T_MAX;      // ...ditto, for newPos
    errorHandler(&params, &replacement, newPos);  // replacement is new ref on output
    if (ypObject_TYPE_PAIR_CODE(replacement) != ypStr_CODE) {
        ypObject *typeErr = yp_BAD_TYPE(replacement);
        yp_decref(replacement);
        return typeErr;
    }

    // XXX Python allows the bytes variable in the exception to be modified and replaced here, but
    // this isn't documented and I'm assuming it's a deprecated feature

    if (*newPos < 0) *newPos = source_len + (*newPos);
    if (newPos < 0 || *newPos > source_len) {
        yp_decref(replacement);
        return yp_IndexError;  // "position %zd from error handler out of bounds"
    }

    return replacement;
}

// future_growth is an upper-bound estimate of the number of additional characters, not including
// the replacement text, that are expected to be added to decoded. After appending, decoded will
// have at least future_growth space available. Does not null-terminate decoded.
// TODO Rewrite to use ypStringLib_extend_fromstring? (Although, what about future_growth?)
static ypObject *ypStringLib_decode_extend_replacement(
        ypObject *decoded, ypObject *replacement, yp_ssize_t future_growth)
{
    yp_ssize_t newLen;
    yp_ssize_t newAlloclen;
    ypObject  *result;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(decoded) == ypStr_CODE, "decoded must be a string");
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(replacement) == ypStr_CODE, "replacement must be a string");
    yp_ASSERT(future_growth >= 0, "future_growth can't be negative");

    if (ypStringLib_LEN(replacement) > ypStringLib_LEN_MAX - ypStringLib_LEN(decoded)) {
        return yp_MemorySizeOverflowError;
    }
    newLen = ypStringLib_LEN(decoded) + ypStringLib_LEN(replacement);
    if (future_growth > ypStringLib_LEN_MAX - newLen) {
        return yp_MemorySizeOverflowError;
    }
    newAlloclen = newLen + future_growth;
    if (ypStringLib_ALLOCLEN(decoded) < newAlloclen ||
            ypStringLib_ENC(decoded)->elemsize < ypStringLib_ENC(replacement)->elemsize) {
        // Recall _ypStringLib_grow_onextend adjusts alloclen and enc.
        result = _ypStringLib_grow_onextend(decoded, newAlloclen, 0, ypStringLib_ENC(replacement));
        if (yp_isexceptionC(result)) return result;
    }

    // Recall we don't need to null-terminate, yet.
    ypStringLib_elemcopy_maybeupconvert(ypStringLib_ENC(decoded)->sizeshift,
            ypStringLib_DATA(decoded), ypStringLib_LEN(decoded),
            ypStringLib_ENC(replacement)->sizeshift, ypStringLib_DATA(replacement), 0,
            ypStringLib_LEN(replacement));

    ypStringLib_SET_LEN(decoded, newLen);
    return yp_None;
}


// UTF-8 encoding and decoding functions

// Returns the number of consecutive ascii bytes starting at start. Valid for ascii, latin-1, and
// utf-8 encodings.
// XXX Adapted from Python's ascii_decode
#define ypStringLib_ASCII_CHAR_MASK 0x8080808080808080ULL
yp_STATIC_ASSERT(((_yp_uint_t)ypStringLib_ASCII_CHAR_MASK) == ypStringLib_ASCII_CHAR_MASK,
        ascii_mask_matches_type);
static yp_ssize_t ypStringLib_count_ascii_bytes(const yp_uint8_t *start, const yp_uint8_t *end)
{
    const yp_uint8_t *p = start;
    const yp_uint8_t *aligned_end = yp_ALIGN_DOWN(end, yp_sizeof(_yp_uint_t));

    // If we don't contain an aligned _yp_uint_t, jump to the end
    if (aligned_end - start < yp_sizeof(_yp_uint_t)) goto final_loop;

    // Read the first few bytes until we're aligned. We can return early because we're reading
    // byte-by-byte.
    while (!yp_IS_ALIGNED(p, yp_sizeof(_yp_uint_t))) {
        if (((yp_uint8_t)*p) & 0x80) return p - start;
        ++p;
    }

    // Now read as many aligned ints as we can. Remember that even though the CHAR_MASK test may
    // fail, there may still be a few ascii bytes, so we still need to jump to the end.
    while (p < aligned_end) {
        _yp_uint_t value = *((_yp_uint_t *)p);
        if (value & ypStringLib_ASCII_CHAR_MASK) break;
        p += yp_sizeof(_yp_uint_t);
    }

// Now read the final, unaligned bytes
final_loop:
    while (p < end) {
        if (((yp_uint8_t)*p) & 0x80) break;
        ++p;
    }
    return p - start;
}

// Returns true iff the byte is a valid utf-8 continuation character (i.e. 10xxxxxx)
#define ypStringLib_UTF_8_IS_CONTINUATION(ch) ((ch) >= 0x80 && (ch) < 0xC0)

// _ypStringLib_decode_utf_8_inner_loop either returns one of these values, or the out-of-range
// character (>0xFF) that was decoded (but not written to dest). The "invalid continuation" values
// (1, 2, and 3) are chosen so that the value gives the number of bytes that must be skipped.
// clang-format off
#define ypStringLib_UTF_8_DATA_END          (0u)    // aka success
#define ypStringLib_UTF_8_INVALID_CONT_1    (1u)
#define ypStringLib_UTF_8_INVALID_CONT_2    (2u)
#define ypStringLib_UTF_8_INVALID_CONT_3    (3u)
#define ypStringLib_UTF_8_INVALID_START     (4u)
#define ypStringLib_UTF_8_INVALID_END       (5u)    // *unexpected* end of data
// clang-format on

// Appends the decoded bytes to dest, updating its length. If a decoding error occurs, *source
// will point to the start of the invalid sequence of bytes, and one of the above error codes will
// be returned. If a character is decoded that is too large to fit in dest's encoding, *source
// will point to the end of the (valid) sequence of bytes, the character will be returned, and
// it will be up to the caller to reallocate dest *and* append the character.
// XXX Don't forget to append the valid-but-too-large character to dest!
// XXX Adapted from Python's STRINGLIB(utf8_decode)
static yp_uint32_t _ypStringLib_decode_utf_8_inner_loop(
        ypObject *dest, const yp_uint8_t **source, const yp_uint8_t *end)
{
    yp_uint32_t               ch;
    const yp_uint8_t         *s = *source;
    int                       dest_sizeshift = ypStringLib_ENC(dest)->sizeshift;
    yp_uint32_t               dest_max_char = ypStringLib_ENC(dest)->max_char;
    ypStringLib_setindexXfunc dest_setindexX = ypStringLib_ENC(dest)->setindexX;
    void                     *dest_data = ypStringLib_DATA(dest);
    yp_ssize_t                dest_len = ypStringLib_LEN(dest);

    while (s < end) {
        // We're going to be decoding at least one character, so make sure there's room
        yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - dest_len >= 1);
        ch = *s;

        if (ch < 0x80) {
            /* Fast path for runs of ASCII characters. Given that common UTF-8
               input will consist of an overwhelming majority of ASCII
               characters, we try to optimize for this case.
            */
            yp_ssize_t ascii_len = ypStringLib_count_ascii_bytes(s, end);
            yp_ASSERT1(ascii_len > 0);
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - dest_len >= ascii_len);
            ypStringLib_elemcopy_maybeupconvert(
                    dest_sizeshift, dest_data, dest_len, 0 /*(ascii sizeshift)*/, s, 0, ascii_len);
            s += ascii_len;
            dest_len += ascii_len;
            continue;
        }

        if (ch < 0xE0) {
            // \xC2\x80-\xDF\xBF -- 0080-07FF
            yp_uint32_t ch2;
            if (ch < 0xC2) {
                /* invalid sequence
                \x80-\xBF -- continuation byte
                \xC0-\xC1 -- fake 0000-007F */
                ch = ypStringLib_UTF_8_INVALID_START;
                goto Return;
            }
            if (end - s < 2) {
                /* unexpected end of data: the caller will decide whether
                   it's an error or not */
                ch = ypStringLib_UTF_8_INVALID_END;
                goto Return;
            }
            ch2 = s[1];
            if (!ypStringLib_UTF_8_IS_CONTINUATION(ch2)) {
                ch = ypStringLib_UTF_8_INVALID_CONT_1;
                goto Return;
            }
            ch = (ch << 6) + ch2 - ((0xC0 << 6) + 0x80);
            yp_ASSERT1((ch > 0x007F) && (ch <= 0x07FF));
            s += 2;
            if (ch > dest_max_char) {
                // Out-of-range
                goto Return;
            }
            dest_setindexX(dest_data, dest_len, ch);
            dest_len += 1;
            continue;
        }

        if (ch < 0xF0) {
            // \xE0\xA0\x80-\xEF\xBF\xBF -- 0800-FFFF
            yp_uint32_t ch2, ch3;
            if (end - s < 3) {
                /* unexpected end of data: the caller will decide whether
                   it's an error or not */
                if (end - s < 2) {
                    ch = ypStringLib_UTF_8_INVALID_END;
                    goto Return;
                }
                ch2 = s[1];
                if (!ypStringLib_UTF_8_IS_CONTINUATION(ch2) ||
                        (ch2 < 0xA0 ? ch == 0xE0 : ch == 0xED)) {
                    // for clarification see comments below
                    ch = ypStringLib_UTF_8_INVALID_CONT_1;
                    goto Return;
                }
                ch = ypStringLib_UTF_8_INVALID_END;
                goto Return;
            }
            ch2 = s[1];
            ch3 = s[2];
            if (!ypStringLib_UTF_8_IS_CONTINUATION(ch2)) {
                ch = ypStringLib_UTF_8_INVALID_CONT_1;
                goto Return;
            }
            if (ch == 0xE0) {
                if (ch2 < 0xA0) {
                    /* invalid sequence
                       \xE0\x80\x80-\xE0\x9F\xBF -- fake 0000-0800 */
                    ch = ypStringLib_UTF_8_INVALID_CONT_1;
                    goto Return;
                }
            } else if (ch == 0xED && ch2 >= 0xA0) {
                /* Decoding UTF-8 sequences in range \xED\xA0\x80-\xED\xBF\xBF
                   will result in surrogates in range D800-DFFF. Surrogates are
                   not valid UTF-8 so they are rejected.
                   See http://www.unicode.org/versions/Unicode5.2.0/ch03.pdf
                   (table 3-7) and http://www.rfc-editor.org/rfc/rfc3629.txt */
                ch = ypStringLib_UTF_8_INVALID_CONT_1;
                goto Return;
            }
            if (!ypStringLib_UTF_8_IS_CONTINUATION(ch3)) {
                ch = ypStringLib_UTF_8_INVALID_CONT_2;
                goto Return;
            }
            ch = (ch << 12) + (ch2 << 6) + ch3 - ((0xE0 << 12) + (0x80 << 6) + 0x80);
            yp_ASSERT1((ch > 0x07FF) && (ch <= 0xFFFF));
            s += 3;
            if (ch > dest_max_char) {
                // Out-of-range
                goto Return;
            }
            dest_setindexX(dest_data, dest_len, ch);
            dest_len += 1;
            continue;
        }

        if (ch < 0xF5) {
            // \xF0\x90\x80\x80-\xF4\x8F\xBF\xBF -- 10000-10FFFF
            yp_uint32_t ch2, ch3, ch4;
            if (end - s < 4) {
                /* unexpected end of data: the caller will decide whether
                   it's an error or not */
                if (end - s < 2) {
                    ch = ypStringLib_UTF_8_INVALID_END;
                    goto Return;
                }
                ch2 = s[1];
                if (!ypStringLib_UTF_8_IS_CONTINUATION(ch2) ||
                        (ch2 < 0x90 ? ch == 0xF0 : ch == 0xF4)) {
                    // for clarification see comments below
                    ch = ypStringLib_UTF_8_INVALID_CONT_1;
                    goto Return;
                }
                if (end - s < 3) {
                    ch = ypStringLib_UTF_8_INVALID_END;
                    goto Return;
                }
                ch3 = s[2];
                if (!ypStringLib_UTF_8_IS_CONTINUATION(ch3)) {
                    ch = ypStringLib_UTF_8_INVALID_CONT_2;
                    goto Return;
                }
                ch = ypStringLib_UTF_8_INVALID_END;
                goto Return;
            }
            ch2 = s[1];
            ch3 = s[2];
            ch4 = s[3];
            if (!ypStringLib_UTF_8_IS_CONTINUATION(ch2)) {
                ch = ypStringLib_UTF_8_INVALID_CONT_1;
                goto Return;
            }
            if (ch == 0xF0) {
                if (ch2 < 0x90) {
                    /* invalid sequence
                       \xF0\x80\x80\x80-\xF0\x8F\xBF\xBF -- fake 0000-FFFF */
                    ch = ypStringLib_UTF_8_INVALID_CONT_1;
                    goto Return;
                }
            } else if (ch == 0xF4 && ch2 >= 0x90) {
                /* invalid sequence
                   \xF4\x90\x80\80- -- 110000- overflow */
                // This is the ypStringLib_MAX_UNICODE case
                ch = ypStringLib_UTF_8_INVALID_CONT_1;
                goto Return;
            }
            if (!ypStringLib_UTF_8_IS_CONTINUATION(ch3)) {
                ch = ypStringLib_UTF_8_INVALID_CONT_2;
                goto Return;
            }
            if (!ypStringLib_UTF_8_IS_CONTINUATION(ch4)) {
                ch = ypStringLib_UTF_8_INVALID_CONT_3;
                goto Return;
            }
            ch = (ch << 18) + (ch2 << 12) + (ch3 << 6) + ch4 -
                 ((0xF0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80);
            yp_ASSERT1((ch > 0xFFFF) && (ch <= 0x10FFFF));
            s += 4;
            if (ch > dest_max_char) {
                // Out-of-range
                goto Return;
            }
            dest_setindexX(dest_data, dest_len, ch);
            dest_len += 1;
            continue;
        }

        ch = ypStringLib_UTF_8_INVALID_START;
        goto Return;
    }

    // No more characters: we've decoded everything
    ch = ypStringLib_UTF_8_DATA_END;

Return:
    *source = s;
    ypStringLib_SET_LEN(dest, dest_len);
    return ch;
}

// Called when _ypStringLib_decode_utf_8_inner_loop can't fit the decoded character ch in the
// current encoding. This will up-convert dest to an encoding that can fit ch, then append ch.
// dest will have space for requiredLen characters plus the null terminator (make sure this
// includes room for ch).
// TODO Rewrite to call ypStringLib_push?
static ypObject *_ypStringLib_decode_utf_8_grow_encoding(
        ypObject *dest, yp_uint32_t ch, yp_ssize_t requiredLen)
{
    const ypStringLib_encinfo *newEnc;
    ypObject                  *result;
    yp_ASSERT(ch > 0xFFu, "only call when _ypStringLib_decode_utf_8_inner_loop can't fit the "
                          "decoded character");
    yp_ASSERT(requiredLen - ypStringLib_LEN(dest) > 0, "not enough room given to write ch");

    newEnc = ch > 0xFFFFu ? ypStringLib_enc_ucs_4 : ypStringLib_enc_ucs_2;
    yp_ASSERT(newEnc->elemsize > ypStringLib_ENC(dest)->elemsize,
            "function called without actually needing to grow the encoding");

    // Recall _ypStringLib_grow_onextend adjusts alloclen and enc.
    result = _ypStringLib_grow_onextend(dest, requiredLen, 0, newEnc);
    if (yp_isexceptionC(result)) return result;

    newEnc->setindexX(ypStringLib_DATA(dest), ypStringLib_LEN(dest), ch);

    ypStringLib_SET_LEN(dest, ypStringLib_LEN(dest) + 1);
    return yp_None;
}

static ypObject *_ypStringLib_decode_utf_8_outer_loop(ypObject *dest, const yp_uint8_t *starts,
        const yp_uint8_t *source, const yp_uint8_t *end, ypObject *errors)
{
    ypObject                      *result;
    yp_codecs_error_handler_func_t errorHandler = NULL;

    while (1) {
        yp_uint32_t ch = _ypStringLib_decode_utf_8_inner_loop(dest, &source, end);

        if (ch == ypStringLib_UTF_8_DATA_END) {
            // That's it, everything's decoded. Null-terminate the object and return.
            yp_ASSERT(source == end,
                    "_ypStringLib_decode_utf_8_inner_loop didn't end at the end...?");
            // TODO See how much wasted space is left here and if we should release some back to
            // the heap
            ypStringLib_ENC(dest)->setindexX(ypStringLib_DATA(dest), ypStringLib_LEN(dest), 0);
            ypStringLib_ASSERT_INVARIANTS(dest);
            return yp_None;

        } else if (ch > 0xFFu) {
            // We successfully decoded a character, but it doesn't fit in dest's current encoding.
            // Update our expectation of how many more characters will be added: it's the
            // number of byes left to decode (remembering source was modified above).
            // XXX This can't overflow because dest_alloclen should be smaller than currently
            yp_ssize_t dest_requiredLen = ypStringLib_LEN(dest) + 1 /*for ch*/ + (end - source);
            result = _ypStringLib_decode_utf_8_grow_encoding(dest, ch, dest_requiredLen);
            if (yp_isexceptionC(result)) return result;

        } else {
            // We've hit a decoding error: call the error handler
            // TODO Test surrogate characters in the start, end, and middle of string, both on
            // encode and decode, and multiple contiguous and non-contiguous surrogates
            ypObject         *replacement;
            yp_ssize_t        newPos;
            const yp_uint8_t *errmsg;
            yp_ssize_t        errEnd;
            yp_ssize_t        errStart = source - starts;
            switch (ch) {
            case ypStringLib_UTF_8_INVALID_END:
                errmsg = "unexpected end of data";
                errEnd = end - starts;
                break;
            case ypStringLib_UTF_8_INVALID_START:
                errmsg = "invalid start byte";
                errEnd = errStart + 1;
                break;
            case ypStringLib_UTF_8_INVALID_CONT_1:
            case ypStringLib_UTF_8_INVALID_CONT_2:
            case ypStringLib_UTF_8_INVALID_CONT_3:
                // These constants are chosen so that their value is the amount of characters
                // to advance
                errmsg = "invalid continuation byte";
                errEnd = errStart + (yp_ssize_t)ch;
                break;
            default:
                return yp_SystemError;
            }

            if (errorHandler == NULL) {
                ypObject *exc = yp_None;
                errorHandler = yp_codecs_lookup_errorE(errors, &exc);
                if (yp_isexceptionC(exc)) return exc;
            }

            replacement = ypStringLib_decode_call_errorhandler(  // new ref
                    errorHandler, errmsg, yp_s_utf_8, starts, end - starts, errStart, errEnd,
                    &newPos);
            if (yp_isexceptionC(replacement)) return replacement;
            source = starts + newPos;

            // We can now update our expectation of how many more characters will be added: it's
            // the number of byes left to decode (remembering source was modified above).
            result = ypStringLib_decode_extend_replacement(dest, replacement, end - source);
            yp_decref(replacement);
            if (yp_isexceptionC(result)) return result;
        }
    }
}


// Called on a null source. Returns a (null-terminated) string of null characters of the given
// length.
static ypObject *_ypStringLib_decode_utf_8_onnull(int type, yp_ssize_t len)
{
    ypObject *newS = _ypStr_new_latin_1(type, len, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newS)) return newS;
    yp_memset(ypStringLib_DATA(newS), 0, len + 1 /*+1 for extra null terminator*/);
    ypStringLib_SET_LEN(newS, len);
    ypStringLib_ASSERT_INVARIANTS(newS);
    return newS;
}

// Called when source starts with at least one ascii character. Returns the decoded string object.
static ypObject *_yp_chrC(int type, yp_int_t i);
static ypObject *_ypStringLib_decode_utf_8_ascii_start(
        int type, yp_ssize_t len, const yp_uint8_t *source, ypObject *errors)
{
    const yp_uint8_t *starts = source;
    const yp_uint8_t *end = source + len;
    yp_ssize_t        leading_ascii;
    ypObject         *dest;
    ypObject         *result;
    yp_ASSERT(len > 0, "zero-length strings should be handled before this function");
    yp_ASSERT(source[0] < 0x80u, "call this only on strings that start with ascii characters");

    // Handle single-char strings separately so we can take advantage of yp_chrC efficiencies
    if (len == 1) {
        return _yp_chrC(type, source[0]);
    }

    // If the string is entirely ASCII characters, we can memcpy and possibly allocate in-line
    leading_ascii = ypStringLib_count_ascii_bytes(source, end);
    yp_ASSERT(leading_ascii > 0 && leading_ascii <= len,
            "unexpected output from ypStringLib_count_ascii_bytes");
    if (leading_ascii == len) {
        // TODO When we have an associated UTF-8 bytes object, we can share the ASCII buffer
        dest = _ypStr_new_latin_1(type, len, /*alloclen_fixed=*/TRUE);
        if (yp_isexceptionC(dest)) return dest;
        yp_memcpy(ypStringLib_DATA(dest), source, len);
        ((yp_uint8_t *)ypStringLib_DATA(dest))[len] = 0;
        ypStringLib_SET_LEN(dest, len);
        ypStringLib_ASSERT_INVARIANTS(dest);
        return dest;
    }

    // Otherwise, it's not entirely ASCII, but we know it starts that way, so copy over the
    // part we know and move on to the main loop
    // XXX Worst case: If source contains mostly 0x80-0xFF bytes then we are allocating twice the
    // required memory here
    dest = _ypStr_new_latin_1(type, len, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(dest)) return dest;
    yp_memcpy(ypStringLib_DATA(dest), source, leading_ascii);
    ypStringLib_SET_LEN(dest, leading_ascii);
    source += leading_ascii;

    result = _ypStringLib_decode_utf_8_outer_loop(dest, starts, source, end, errors);
    if (yp_isexceptionC(result)) {
        yp_decref(dest);
        return result;
    }
    return dest;
}

// Should only be called from  _ypStringLib_decode_utf_8. Using only the inline buffer of dest,
// decode the first "few" characters in *source. As in _ypStringLib_decode_utf_8_inner_loop,
// returns either an error code, or the character that could not be written to dest's inline
// buffer; the latter thus indicates what encoding to start with when allocating the separate
// buffer. *source will point to either the error location, or the byte after the returned
// character; dest may be re-encoded in-place. On input, dest must be empty and latin-1, and
// *source must be larger than dest's inline buffer (otherwise there's no point in the precheck).
static yp_uint32_t _ypStringLib_decode_utf_8_inline_precheck(
        ypObject *dest, const yp_uint8_t **source)
{
    yp_ssize_t dest_maxinline = ypStringLib_ALLOCLEN(dest) - 1;  // -1 for terminator
    // TODO Does it really make sense to use the entire inline buffer for the precheck?  Aren't
    // strings "usually" entirely latin-1, or ucs-2, or ucs-4?  Isn't it enough just to check the
    // first, I dunno, 16 characters?  Doing this won't save on allocations, but it *will* save
    // on the memcpy required to move to a separate buffer.
    // ...But wait, consider an HTML page. It's going to start with a bunch of ASCII, then
    // anything ucs-2 or ucs-4 will be in the middle. What use cases am I optimizing for:
    // converting a whole document, or small strings one-at-a-time?
    const yp_uint8_t *fake_end = (*source) + dest_maxinline;
    yp_uint32_t       ch;
    void             *dest_data;
    yp_ssize_t        dest_len;

    yp_ASSERT(dest_maxinline > 0, "str's inline buffer should fit at least one character");
    ch = _ypStringLib_decode_utf_8_inner_loop(dest, source, fake_end);

    // If inner_loop hit an error, or decoded a utf-4 character, we've done all we can
    if (ch <= 0xFFu || ch >= 0x10000u) {
        return ch;
    }

    // To do anything useful, we need room to upconvert, write ch, then write at least one more
    // character. If we can't, we bail.
    dest_maxinline = (ypStringLib_ALLOCLEN(dest) / 2) - 1;
    dest_len = ypStringLib_LEN(dest);
    // -1 for ch, then -1 to make sure we can detect at least one more character
    if (dest_maxinline - 2 < dest_len) {
        return ch;
    }

    // Convert to ucs-2, and don't forget to write ch!
    dest_data = ypStringLib_DATA(dest);
    _ypStringLib_inplace_2from1(dest_data, dest_len);
    ((yp_uint16_t *)dest_data)[dest_len] = (yp_uint16_t)ch;
    dest_len += 1;
    ypStringLib_SET_LEN(dest, dest_len);
    ypStringLib_ENC_CODE(dest) = ypStringLib_ENC_CODE_UCS_2;
    ypStringLib_SET_ALLOCLEN(dest, ypStringLib_ALLOCLEN(dest) / 2);

    // We are at least ucs-2: see if we are actually ucs-4. Recall that source was modified above.
    fake_end = (*source) + (dest_maxinline - dest_len);
    return _ypStringLib_decode_utf_8_inner_loop(dest, source, fake_end);
}

// Called by ypStringLib_decode_frombytesC_utf_8 in the general case. Returns the decoded string
// object.
static ypObject *_ypStringLib_decode_utf_8(
        int type, yp_ssize_t len, const yp_uint8_t *source, ypObject *errors)
{
    const yp_uint8_t *starts = source;
    const yp_uint8_t *end = source + len;
    ypObject         *dest;
    yp_uint32_t       ch;
    yp_ssize_t        dest_requiredLen;
    ypObject         *result;
    yp_ASSERT(len > 0, "zero-length strings should be handled before _ypStringLib_decode_utf_8");
    yp_ASSERT(len <= ypStringLib_LEN_MAX, "can't decode more than ypStringLib_LEN_MAX bytes");

    // If it doesn't start with any ASCII, then before we allocate a separate buffer to hold the
    // data, run the first few bytes through _ypStringLib_decode_utf_8_inner_loop using the inline
    // buffer, to see if we can tell what element size we _should_ be using
    // TODO Contribute this optimization back to Python?

    dest = _ypStr_new_latin_1(type, 0, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(dest)) return dest;

    // Shortcut: if the inline array can fit all decoded characters anyway, jump to outer loop
    if (len <= ypStringLib_ALLOCLEN(dest) - 1) {  // -1 for terminator
        goto outer_loop;
    }

    ch = _ypStringLib_decode_utf_8_inline_precheck(dest, &source);

    if (ch > 0xFFu) {
        // Success!  We can start off appropriately up-converted.
        // XXX Can't overflow because we've checked len<=MAX, and len is the worst-case num of
        // chars
        dest_requiredLen = ypStringLib_LEN(dest) + 1 /*for ch*/ + (end - source);
        result = _ypStringLib_decode_utf_8_grow_encoding(dest, ch, dest_requiredLen);
        if (yp_isexceptionC(result)) {
            yp_decref(dest);
            return result;
        }

    } else {
        // We've reached the end of what we can decode into the inline buffer, or there was a
        // decoding error. Either way, resize and move on to outer_loop.
        // XXX That's actually a lie: we've reached fake_end, but it's possible we haven't decoded
        // as many characters as we estimated and there's still room in the inline buffer. We
        // _could_ keep adjusting fake_end until there's no more room, but I don't think this is
        // advantageous. Don't the first few characters usually determine the encoding?
        // XXX Can't overflow because we've checked len<=MAX, and len is worst-case num of chars
        dest_requiredLen = ypStringLib_LEN(dest) /*ch isn't a char*/ + (end - source);
        if (dest_requiredLen > ypStringLib_ALLOCLEN(dest) - 1) {
            // Recall _ypStringLib_grow_onextend adjusts alloclen and enc.
            result = _ypStringLib_grow_onextend(dest, dest_requiredLen, 0, ypStringLib_ENC(dest));
            if (yp_isexceptionC(result)) {
                yp_decref(dest);
                return result;
            }
        }
    }

outer_loop:
    result = _ypStringLib_decode_utf_8_outer_loop(dest, starts, source, end, errors);
    if (yp_isexceptionC(result)) {
        yp_decref(dest);
        return result;
    }
    return dest;
}

// Decodes the len bytes of utf-8 at source according to errors, and returns a new string of the
// given type. If source is NULL it is considered as having all null bytes; len cannot be
// negative or greater than ypStringLib_LEN_MAX.
// XXX Allocation-wise, the worst-case through the code would be a completely ucs-4 string, as we'd
// allocate len characters (len*4 bytes) for the decoding, but would only decode len/4 characters
// TODO This is TERRIBLE, because if a string has more than a couple ucs-4 characters, it's
// probably *mostly* ucs-4 characters. Is there a quick way to scan the _entire_ string?  Or can
// we just trim the excess once we reach the end?
// XXX Runtime-wise, the worst-case would probably be a string that starts completely Latin-1 (each
// character is a call to enc->setindexX), followed by a ucs-2 then a ucs-4 character (each
// triggering an upconvert of previously-decoded characters)
// TODO Keep the UTF-8 bytes object associated with the new string, but only if there were no
// decoding errors
static ypObject *ypStringLib_decode_frombytesC_utf_8(
        int type, yp_ssize_t len, const yp_uint8_t *source, ypObject *errors)
{
    yp_ASSERT(ypObject_TYPE_CODE_AS_FROZEN(type) == ypStr_CODE, "incorrect str type");
    yp_ASSERT(len >= 0, "negative len not allowed (do ypBytes_adjust_lenC before "
                        "ypStringLib_decode_frombytesC_*)");
    yp_ASSERT(len <= ypStringLib_LEN_MAX, "can't decode more than ypStringLib_LEN_MAX bytes");

    // Handle the empty-string and string-of-nulls cases first
    if (len < 1) {
        if (type == ypStr_CODE) return yp_str_empty;
        return yp_chrarray0();
    } else if (source == NULL) {
        return _ypStringLib_decode_utf_8_onnull(type, len);
    } else if (source[0] < 0x80u) {
        // We optimize for UTF-8 data that is completely, or at least starts with, ASCII: since
        // ASCII is equivalent to the first 128 ordinals in Unicode, we can just memcpy
        return _ypStringLib_decode_utf_8_ascii_start(type, len, source, errors);
    } else {
        return _ypStringLib_decode_utf_8(type, len, source, errors);
    }
}

// XXX Allocation-wise, the worst-case through the code is a string that starts with a latin-1
// character, causing us to allocate len*2 bytes, then containing only ascii, wasting just a byte
// under half the buffer
// XXX Runtime-wise, the worst-case is a string with completely latin-1 characters
// XXX There's no possibility of an error handler being called, so we can use alloclen_fixed=TRUE
static ypObject *_ypBytesC(int type, yp_ssize_t len, const yp_uint8_t *source);
static ypObject *_ypStringLib_encode_utf_8_fromlatin_1(int type, ypObject *source)
{
    yp_ssize_t const  source_len = ypStringLib_LEN(source);
    yp_uint8_t *const source_data = ypStringLib_DATA(source);
    yp_uint8_t *const source_end = source_data + source_len;
    yp_uint8_t       *s;  // moving source_data pointer
    ypObject         *dest;
    yp_ssize_t        dest_alloclen;
    yp_uint8_t       *dest_data;
    yp_uint8_t       *d;  // moving dest_data pointer

    ypStringLib_ASSERT_INVARIANTS(source);
    yp_ASSERT(ypStringLib_ENC_CODE(source) == ypStringLib_ENC_CODE_LATIN_1,
            "_ypStringLib_encode_utf_8_fromlatin_1 called on wrong str encoding");
    yp_ASSERT(source_len > 0,
            "empty-string case should be handled before _ypStringLib_encode_utf_8_fromlatin_1");

    // We optimize for UTF-8 data that is completely, or almost-completely, ASCII, since ASCII is
    // equivalent to the first 128 ordinals in Unicode
    // TODO If we ever keep immortal len-1 bytes objects around, use them here if source_len==1
    if (source_data[0] < 0x80u) {
        // If the string is entirely ASCII characters, we can memcpy and possibly allocate in-line
        yp_ssize_t leading_ascii = ypStringLib_count_ascii_bytes(source_data, source_end);
        yp_ASSERT1(leading_ascii > 0 && leading_ascii <= ypStringLib_LEN_MAX);
        yp_ASSERT1(leading_ascii <= source_len);
        if (leading_ascii == source_len) {
            // TODO When we have an associated UTF-8 bytes object, we can share the ASCII buffer
            return _ypBytesC(type, source_len, source_data /*alloclen_fixed=TRUE*/);
        }

        // Otherwise, it's not entirely ASCII, but we know it starts that way, so copy over the
        // part we know and move on to the main loop
        // XXX We know that the first leading_ascii characters only need one byte, but the
        // remaining might need up to two
        if (source_len - leading_ascii > (ypStringLib_LEN_MAX - leading_ascii) / 2) {
            return yp_MemorySizeOverflowError;
        }
        dest_alloclen = leading_ascii + (source_len - leading_ascii) * 2;
        dest = _ypBytes_new(type, dest_alloclen, /*alloclen_fixed=*/TRUE);
        if (yp_isexceptionC(dest)) return dest;
        dest_data = ypStringLib_DATA(dest);
        yp_memcpy(dest_data, source_data, leading_ascii);
        s = source_data + leading_ascii;
        d = dest_data + leading_ascii;

        // If it doesn't start with any ASCII...well...not much we can do
    } else {
        if (source_len > ypStringLib_LEN_MAX / 2) return yp_MemorySizeOverflowError;
        dest = _ypBytes_new(type, source_len * 2, /*alloclen_fixed=*/TRUE);
        if (yp_isexceptionC(dest)) return dest;
        s = source_data;
        d = dest_data = ypStringLib_DATA(dest);
    }

    while (s < source_end) {
        yp_uint8_t ch = *s;

        if (ch < 0x80u) {
            /* Fast path for runs of ASCII characters. Given that common UTF-8
               input will consist of an overwhelming majority of ASCII
               characters, we try to optimize for this case.
            */
            // TODO Contribute this optimization back to Python
            yp_ssize_t ascii_len = ypStringLib_count_ascii_bytes(s, source_end);
            yp_ASSERT1(ascii_len > 0);
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - (d - dest_data) >= ascii_len);
            yp_memcpy(d, s, ascii_len);
            d += ascii_len;
            s += ascii_len;

        } else {
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - (d - dest_data) >= 2);
            *d++ = (yp_uint8_t)(0xc0u | (ch >> 6));
            *d++ = (yp_uint8_t)(0x80u | (ch & 0x3fu));
            s += 1;
        }
    }

    // Null-terminate, update length, and return
    // TODO Trim excess memory here (certainly if it can be done in-place)
    *d = 0;
    ypStringLib_SET_LEN(dest, d - dest_data);
    ypStringLib_ASSERT_INVARIANTS(dest);
    return dest;
}

// XXX Allocation-wise, the worst case through the code is a string that contains just one ucs-4
// character, causing us to allocate len*4 bytes, then containing only ascii, wasting just about
// three quarters of the buffer
// XXX Runtime-wise, all paths are pretty similar
// XXX We can't use alloclen_fixed=TRUE here, because the error handler might need to resize
// TODO However, trimming the buffer might be a good idea
static ypObject *_ypStringLib_encode_utf_8(int type, ypObject *source, ypObject *errors)
{
    yp_ssize_t const               source_len = ypStringLib_LEN(source);
    void *const                    source_data = ypStringLib_DATA(source);
    const ypStringLib_encinfo     *source_enc = ypStringLib_ENC(source);
    ypStringLib_getindexXfunc      getindexX = source_enc->getindexX;
    yp_ssize_t                     maxCharSize;
    yp_ssize_t                     i;  // index into source_data
    ypObject                      *dest;
    yp_uint8_t                    *dest_data;
    yp_uint8_t                    *d;  // moving dest_data pointer
    yp_codecs_error_handler_func_t errorHandler = NULL;
    ypObject                      *replacement;
    ypObject                      *result;

    ypStringLib_ASSERT_INVARIANTS(source);
    yp_ASSERT(source_enc != ypStringLib_enc_latin_1,
            "use _ypStringLib_encode_utf_8_fromlatin_1 for latin-1 strings");
    yp_ASSERT(
            source_len > 0, "empty-string case should be handled before _ypStringLib_encode_utf_8");
    maxCharSize = source_enc == ypStringLib_enc_ucs_2 ? 3 : 4;

    if (source_len > ypStringLib_LEN_MAX / maxCharSize) return yp_MemorySizeOverflowError;
    dest = _ypBytes_new(type, source_len * maxCharSize, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(dest)) return dest;
    d = dest_data = ypStringLib_DATA(dest);

    for (i = 0; i < source_len; i++) {
        yp_uint32_t ch = getindexX(source_data, i);

        if (ch < 0x80u) {
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - (d - dest_data) >= 1);
            *d++ = (yp_uint8_t)ch;

        } else if (ch < 0x0800u) {
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - (d - dest_data) >= 2);
            *d++ = (yp_uint8_t)(0xc0u | (ch >> 6));
            *d++ = (yp_uint8_t)(0x80u | (ch & 0x3fu));

        } else if (ypStringLib_IS_SURROGATE(ch)) {
            // TODO We could refactor this like on decode: the function returns the surrogate
            // and we call the errorhandler at a higher level...
            if (errorHandler == NULL) {
                ypObject *exc = yp_None;
                errorHandler = yp_codecs_lookup_errorE(errors, &exc);
                if (yp_isexceptionC(exc)) {
                    yp_decref(dest);
                    return exc;
                }
            }

            // TODO Supply an error reason string
            replacement = ypStringLib_encode_call_errorhandler(
                    errorHandler, NULL, yp_s_utf_8, source, i, i + 1, &i);
            if (yp_isexceptionC(replacement)) {
                yp_decref(dest);
                return replacement;
            }

            // We can now update our expectation of how many more bytes will be added: it's the
            // number of characters left to encode (remembering i was modified above). Remember
            // ypStringLib_encode_extend_replacement needs dest's len set appropriately.
            ypStringLib_SET_LEN(dest, d - dest_data);
            result = ypStringLib_encode_extend_replacement(
                    dest, replacement, (source_len - i) * maxCharSize);
            if (yp_isexceptionC(result)) {
                yp_decref(dest);
                return result;
            }

            // Now that we've concatenated replacement onto dest, update our pointer into dest
            d = dest_data + ypStringLib_LEN(dest);

        } else if (ch < 0x10000u) {
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - (d - dest_data) >= 3);
            *d++ = (yp_uint8_t)(0xe0u | (ch >> 12));
            *d++ = (yp_uint8_t)(0x80u | ((ch >> 6) & 0x3fu));
            *d++ = (yp_uint8_t)(0x80u | (ch & 0x3fu));

        } else {
            yp_ASSERT1(ch <= ypStringLib_MAX_UNICODE);
            yp_ASSERT1(ypStringLib_ALLOCLEN(dest) - 1 - (d - dest_data) >= 4);
            *d++ = (yp_uint8_t)(0xf0u | (ch >> 18));
            *d++ = (yp_uint8_t)(0x80u | ((ch >> 12) & 0x3fu));
            *d++ = (yp_uint8_t)(0x80u | ((ch >> 6) & 0x3fu));
            *d++ = (yp_uint8_t)(0x80u | (ch & 0x3fu));
        }
    }

    // Null-terminate, update length, and return
    *d = 0;
    ypStringLib_SET_LEN(dest, d - dest_data);
    ypStringLib_ASSERT_INVARIANTS(dest);
    return dest;
}

static ypObject *ypStringLib_encode_utf_8(int type, ypObject *source, ypObject *errors)
{
    yp_ASSERT(ypObject_TYPE_CODE_AS_FROZEN(type) == ypBytes_CODE, "incorrect bytes type");

    if (ypStringLib_LEN(source) < 1) {
        if (type == ypBytes_CODE) return yp_bytes_empty;
        return yp_bytearray0();
    }

    if (ypStringLib_ENC_CODE(source) == ypStringLib_ENC_CODE_LATIN_1) {
        return _ypStringLib_encode_utf_8_fromlatin_1(type, source);
    } else {
        return _ypStringLib_encode_utf_8(type, source, errors);
    }
}

#pragma endregion strings


/*************************************************************************************************
 * Codec registry and base classes
 *************************************************************************************************/
#pragma region codec_registry

// XXX Patterned after the codecs module in Python
// TODO codecs.register to register functions for encode/decode ...also codecs.lookup
// TODO Python does maintain a distinction between text encodings and all others; do the same
// TODO A macro in nohtyP.h to get/set from a struct with sizeof_struct ensuring compatibility
// TODO yp_codecs_encode and yp_codecs_decode, which works for any arbitrary obj->obj encoding
// TODO In general, everything below is geared for Unicode; make it flexible enough for anything

static ypStringLib_getindexXfunc _yp_codecs_strenc2getindexX(ypObject *encoding)
{
    if (encoding == yp_s_latin_1) return ypStringLib_getindexX_1byte;
    if (encoding == yp_s_ucs_2) return ypStringLib_getindexX_2bytes;
    if (encoding == yp_s_ucs_4) return ypStringLib_getindexX_4bytes;
    // params->source.data.type must be one of the exact objects above, as per yp_asencodedCX
    return NULL;
}

// Set containing the standard encodings like yp_s_utf_8. Instead of a series of yp_eq calls,
// yp_set_getintern is used to return one of these objects, which is then compared by identity
// (i.e. ptr value). Initialized in _yp_codecs_initialize.
static ypObject *_yp_codecs_standard = NULL;

// Dict mapping normalized aliases to "official" names. Initialized in _yp_codecs_initialize.
// TODO Can we statically-allocate this dict?  Perhaps the standard aliases can fit in the
// inline array, and if it grows past that then we allocate on the heap.
static ypObject *_yp_codecs_alias2encoding = NULL;

// All encoding names and their aliases are lowercased, and ' ' and '_' are converted to '-'
// XXX encoding must be a str/chrarray
// XXX Python is inconsistent with how it normalizes encoding names:
//  - encodings/__init__.py: runs of non-alpha (except '.') to '_', leading/trailing '_' removed
//  - unicodeobject.c: to lower, '_' becomes '-', latin-1 names only
//  - textio.c: encodefuncs array uses "utf-8", etc
//  - codecs.c: to lower, ' ' becomes '_', latin-1 names only
//  - encodings/aliases.py: aliases are "utf_8", etc
// Choosing "utf-8", with underscores and spaces turned to hyphens
static ypObject *_yp_codecs_normalize_encoding_name(ypObject *encoding)
{
    yp_uint8_t *data;
    yp_ssize_t  len;
    yp_ssize_t  i;
    ypObject   *norm;
    yp_uint8_t *norm_data;

    // Only latin-1 names are accepted
    if (ypStringLib_ENC_CODE(encoding) != ypStringLib_ENC_CODE_LATIN_1) return yp_ValueError;

    // encoding may already be normalized, in which case: do nothing
    data = ypStringLib_DATA(encoding);
    len = ypStringLib_LEN(encoding);
    for (i = 0; i < len; i++) {
        if (yp_ISUPPER(data[i])) goto convert;
        if (data[i] == ' ' || data[i] == '_') goto convert;
        // TODO Should we deny non-printable characters, '\t', etc?
    }
    return yp_incref(encoding);

convert:
    // OK, there's characters to convert, starting at i: create a new string to return
    norm = _ypStr_new_latin_1(ypStr_CODE, len, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(norm)) return norm;
    norm_data = ypStringLib_DATA(norm);
    yp_memcpy(norm_data, data, i);
    for (/*i already set*/; i < len; i++) {
        yp_uint8_t ch = yp_TOLOWER(data[i]);
        if (ch == ' ' || ch == '_') {
            norm_data[i] = '-';
        } else {
            norm_data[i] = ch;
        }
    }
    norm_data[len] = 0;
    ypStringLib_SET_LEN(norm, len);
    ypStringLib_ASSERT_INVARIANTS(norm);
    return norm;
}

static ypObject *yp_set_getintern(ypObject *set, ypObject *x);

static ypObject *_yp_codecs_register_alias_norm(ypObject *alias_norm, ypObject *encoding_norm)
{
    // We hard-code shortcuts to utf-8 encoders/decoders all over, so don't pretend the user can
    // redirect utf-8 to a new alias
    ypObject *exc = yp_None;
    ypObject *result = yp_eq(alias_norm, yp_s_utf_8);
    if (result != yp_False) {
        if (yp_isexceptionC(result)) return result;
        return yp_ValueError;
    }
    yp_setitem(_yp_codecs_alias2encoding, alias_norm, encoding_norm, &exc);
    return exc;
}

static ypObject *yp_codecs_register_alias(ypObject *alias, ypObject *encoding)
{
    ypObject *result;
    ypObject *alias_norm = _yp_codecs_normalize_encoding_name(alias);        // new ref
    ypObject *encoding_norm = _yp_codecs_normalize_encoding_name(encoding);  // new ref
    if (yp_isexceptionC(alias_norm)) {
        result = alias_norm;
    } else if (yp_isexceptionC(encoding_norm)) {
        result = encoding_norm;
    } else {
        result = _yp_codecs_register_alias_norm(alias_norm, encoding_norm);
    }
    yp_decrefN(2, encoding_norm, alias_norm);
    return result;
}

static ypObject *yp_codecs_lookup_alias(ypObject *alias)
{
    ypObject *encoding;
    ypObject *alias_norm = _yp_codecs_normalize_encoding_name(alias);  // new ref
    if (yp_isexceptionC(alias_norm)) return alias_norm;
    encoding = yp_getitem(_yp_codecs_alias2encoding, alias_norm);
    yp_decref(alias_norm);
    return encoding;
}


// TODO _yp_codecs_encoding2info
// TODO Can we statically-allocate this dict?  Perhaps the standard encodings can fit in the
// inline array, and if it grows past that then we allocate on the heap.
// TODO static yp_codecs_codec_info_t yp_codecs_lookupE(ypObject *encoding, ypObject **exc)
// TODO deny replacing utf_8 codec with anything else, and give it a fast-path in the code
// TODO Registered encoders/decoders should take a ypObject*typehint that identifies a particular
// type for the return value, if possible, otherwise it's ignored and a "standard" type is returned
// (this way, utf-8 can return a bytearray as required by yp_bytearray3)


// Dict mapping error handler names to their functions. Initialized in _yp_codecs_initialize.
// TODO Can we statically-allocate this dict?  Perhaps the standard error handlers can fit in the
// inline array, and if it grows past that then we allocate on the heap.
static ypObject *_yp_codecs_errors2handler = NULL;

static ypObject *yp_codecs_register_error(
        ypObject *name, yp_codecs_error_handler_func_t error_handler)
{
    ypObject *exc = yp_None;
    ypObject *result = yp_intC((yp_ssize_t)error_handler);
    yp_setitem(_yp_codecs_errors2handler, name, result, &exc);
    yp_decref(result);
    return exc;  // on success or exception
}

static void yp_codecs_strict_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position);
static yp_codecs_error_handler_func_t yp_codecs_lookup_errorE(ypObject *name, ypObject **_exc)
{
    ypObject *exc = yp_None;
    ypObject *result = yp_getitem(_yp_codecs_errors2handler, name);  // new ref

    yp_codecs_error_handler_func_t error_handler =
            (yp_codecs_error_handler_func_t)yp_index_asssizeC(result, &exc);
    yp_decref(result);
    if (yp_isexceptionC(exc)) {
        *_exc = exc;
        return yp_codecs_strict_errors;
    }
    return error_handler;
}

static void yp_codecs_strict_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    if (yp_isexceptionC(params->exc)) {
        *replacement = params->exc;
    } else {
        *replacement = yp_TypeError;
    }
    *new_position = yp_SSIZE_T_MAX;
}

yp_IMMORTAL_STR_LATIN_1(yp_codecs_replace_errors_onencode, "?");
static ypObject *yp_codecs_replace_errors_ondecode = NULL;  // TODO Need yp_IMMORTAL_STR_UCS_2
static void      yp_codecs_replace_errors(
             yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    if (yp_isexceptionC2(params->exc, yp_UnicodeEncodeError)) {
        yp_ssize_t replacement_len = params->end - params->start;
        *replacement = yp_repeatC(yp_codecs_replace_errors_ondecode, replacement_len);
        if (yp_isexceptionC(*replacement)) {
            *new_position = yp_SSIZE_T_MAX;
        } else {
            *new_position = params->end;
        }
        return;

    } else if (yp_isexceptionC2(params->exc, yp_UnicodeDecodeError)) {
        if (yp_codecs_replace_errors_ondecode == NULL) {
            ypObject *result = yp_chrC(ypStringLib_UNICODE_REPLACEMENT_CHARACTER);  // new ref
            if (yp_isexceptionC(result)) {
                *replacement = result;
                *new_position = yp_SSIZE_T_MAX;
                return;
            }
            yp_codecs_replace_errors_ondecode = result;  // stolen ref
        }
        *replacement = yp_incref(yp_codecs_replace_errors_ondecode);
        *new_position = params->end;
        return;

    } else {
        *replacement = yp_TypeError;  // unhandled encoding exception
        *new_position = yp_SSIZE_T_MAX;
        return;
    }
}

static void yp_codecs_ignore_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    if (yp_isexceptionC2(params->exc, yp_UnicodeError)) {
        *replacement = yp_str_empty;
        *new_position = params->end;
    } else {
        // TODO Can we make this a bit more flexible, by returning an empty instance of the
        // (hinted) result type?
        *replacement = yp_TypeError;
        *new_position = yp_SSIZE_T_MAX;
    }
}

static void yp_codecs_xmlcharrefreplace_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    *replacement = yp_NotImplementedError;
    *new_position = yp_SSIZE_T_MAX;
}

static void yp_codecs_backslashreplace_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    *replacement = yp_NotImplementedError;
    *new_position = yp_SSIZE_T_MAX;
}

// Returns true if the three bytes at x _could_ be a utf-8 encoded surrogate, or false if it
// definitely is not
// XXX x must contain at least three bytes
#define _yp_codecs_UTF8_SURROGATE_PRECHECK(x) \
    (((x)[0] & 0xf0u) == 0xe0u && ((x)[1] & 0xc0u) == 0x80u && ((x)[2] & 0xc0u) == 0x80u)
// Decodes the utf-8 characters using the three bytes at x; PRECHECK must have returned true; the
// resulting character may not actually be a surrogate
#define _yp_codecs_UTF8_SURROGATE_DECODE(x) \
    ((((x)[0] & 0x0fu) << 12) + (((x)[1] & 0x3fu) << 6) + ((x)[2] & 0x3fu))

// TODO It'd be nice to share code with surrogatepass...
static void yp_codecs_surrogateescape_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    *replacement = yp_NotImplementedError;
    *new_position = yp_SSIZE_T_MAX;
}

// XXX Adapted from PyCodec_SurrogatePassErrors
static ypObject *_yp_codecs_surrogatepass_errors_onencode(
        ypObject *encoding, yp_codecs_error_handler_params_t *params, yp_ssize_t *new_position)
{
    ypStringLib_getindexXfunc getindexX;
    ypObject                 *replacement;
    yp_uint8_t               *outp;
    yp_ssize_t                i;

    getindexX = _yp_codecs_strenc2getindexX(params->source.data.type);
    if (getindexX == NULL) return yp_TypeError;  // params->source.data.type not a string type

    if (encoding == yp_s_utf_8) {
        yp_ssize_t badEnd;      // index of end of surrogates to replace from source
        yp_ssize_t badLen = 0;  // number of surrogate characters to replace

        // Count the number of consecutive surrogates. Stop at the first non-surrogate, or at the
        // end of the buffer.
        for (i = params->start; i < params->source.data.len; i++) {
            yp_int_t ch = getindexX(params->source.data.ptr, i);
            if (!ypStringLib_IS_SURROGATE(ch)) break;
            badLen += 1;
        }
        if (badLen < 1) return params->exc;  // not a surrogate: raise original error

        if (badLen > ypStringLib_LEN_MAX / 3) return yp_MemorySizeOverflowError;
        replacement = _ypBytes_new(ypBytes_CODE, 3 * badLen, /*alloclen_fixed=*/TRUE);
        if (yp_isexceptionC(replacement)) return replacement;

        badEnd = params->start + badLen;
        outp = (yp_uint8_t *)ypStringLib_DATA(replacement);
        for (i = params->start; i < badEnd; i++) {
            yp_int_t ch = getindexX(params->source.data.ptr, i);
            yp_ASSERT(ypStringLib_IS_SURROGATE(ch), "problem in loop above");  // paranoia
            // TODO This bit of code is repeated: what about shared macros to detect/encode/decode
            // the individual types of characters in a utf-8 string?
            *outp++ = (yp_uint8_t)(0xe0u | (ch >> 12));
            *outp++ = (yp_uint8_t)(0x80u | ((ch >> 6) & 0x3fu));
            *outp++ = (yp_uint8_t)(0x80u | (ch & 0x3fu));
        }
        *outp = 0;  // null-terminate
        ypStringLib_SET_LEN(replacement, 3 * badLen);
        *new_position = badEnd;
        ypStringLib_ASSERT_INVARIANTS(replacement);
        return replacement;

    } else {
        yp_ASSERT(!yp_isexceptionC(encoding),
                "yp_set_getintern exceptions should be caught in yp_codecs_surrogatepass_errors");
        return params->exc;  // unsupported standard encoding: raise original error
    }
}
// XXX Adapted from PyCodec_SurrogatePassErrors
static ypObject *_yp_codecs_surrogatepass_errors_ondecode(
        ypObject *encoding, yp_codecs_error_handler_params_t *params, yp_ssize_t *new_position)
{
    ypObject    *replacement;
    yp_uint8_t  *source_data;
    yp_uint16_t *outp;
    yp_ssize_t   i;
    yp_uint32_t  ch;

    // Of course, our source must be a bytes object
    if (params->source.data.type != yp_t_bytes) return yp_TypeError;
    source_data = (yp_uint8_t *)params->source.data.ptr;

    if (encoding == yp_s_utf_8) {
        // TODO The equivalent Python code assumes null-termination of source, or it might
        // overflow. Contribute a fix back to Python.
        yp_ssize_t badEnd;      // index of end of surrogates to replace from source
        yp_ssize_t repLen = 0;  // number of surrogate characters (once decoded) to replace

        // Count the number of consecutive surrogates. Stop at the first non-surrogate, or at the
        // end of the buffer. All surrogates are 3 bytes long.
        for (i = params->start; params->source.data.len - i >= 3; i += 3) {
            if (!_yp_codecs_UTF8_SURROGATE_PRECHECK(source_data + i)) break;
            ch = _yp_codecs_UTF8_SURROGATE_DECODE(source_data + i);
            if (!ypStringLib_IS_SURROGATE(ch)) break;
            repLen += 1;
        }
        if (repLen < 1) return params->exc;  // not a surrogate: raise original error

        // All surrogates are represented in ucs-2
        // TODO This check will never fail, so long as source.data.len<ypStringLib_LEN_MAX
        if (repLen > ypStringLib_LEN_MAX) return yp_MemorySizeOverflowError;
        replacement = _ypStr_new_ucs_2(ypStr_CODE, repLen, /*alloclen_fixed=*/TRUE);
        if (yp_isexceptionC(replacement)) return replacement;

        badEnd = params->start + (repLen * 3);
        outp = (yp_uint16_t *)ypStringLib_DATA(replacement);
        for (i = params->start; i < badEnd; i += 3) {
            yp_ASSERT(_yp_codecs_UTF8_SURROGATE_PRECHECK(source_data + i),
                    "problem in loop above");  // paranoia
            ch = _yp_codecs_UTF8_SURROGATE_DECODE(source_data + i);
            yp_ASSERT(ypStringLib_IS_SURROGATE(ch), "problem in loop above");  // more paranoia
            *outp++ = (yp_uint16_t)ch;
        }
        *outp = 0;  // null-terminate
        ypStringLib_SET_LEN(replacement, repLen);
        *new_position = badEnd;
        ypStringLib_ASSERT_INVARIANTS(replacement);
        return replacement;

    } else {
        yp_ASSERT(!yp_isexceptionC(encoding),
                "yp_set_getintern exceptions should be caught in yp_codecs_surrogatepass_errors");
        return params->exc;  // unsupported standard encoding: raise original error
    }
}
static void yp_codecs_surrogatepass_errors(
        yp_codecs_error_handler_params_t *params, ypObject **replacement, yp_ssize_t *new_position)
{
    ypObject *encoding;

    if (params->source.data.ptr == NULL || params->source.data.type == NULL) {
        *replacement = yp_ValueError;
        goto onerror;
    }
    // TODO Get the getindexX function here for the data

    encoding = yp_set_getintern(_yp_codecs_standard, params->encoding);  // new ref
    if (yp_isexceptionC(encoding)) {
        if (yp_isexceptionC2(encoding, yp_KeyError)) {
            *replacement = params->exc;  // completely-unknown encoding: raise original error
        } else {
            *replacement = encoding;  // unexpected error in yp_set_getintern
        }
        goto onerror;
    }

    if (yp_isexceptionC2(params->exc, yp_UnicodeEncodeError)) {
        *replacement = _yp_codecs_surrogatepass_errors_onencode(encoding, params, new_position);
    } else if (yp_isexceptionC2(params->exc, yp_UnicodeDecodeError)) {
        *replacement = _yp_codecs_surrogatepass_errors_ondecode(encoding, params, new_position);
    } else {
        *replacement = yp_TypeError;  // unhandled encoding exception
    }
    yp_decref(encoding);
    if (yp_isexceptionC(*replacement)) goto onerror;
    return;

onerror:
    *new_position = yp_SSIZE_T_MAX;
    return;
}

#pragma endregion codec_registry


/*************************************************************************************************
 * Sequence of bytes
 *************************************************************************************************/
#pragma region bytes

typedef ypStringLibObject ypBytesObject;

// XXX Since bytes are likely to be used to store arbitrary structures, make sure our alignment is
// compatible will all data types
yp_STATIC_ASSERT(yp_offsetof(ypBytesObject, ob_inline_data) % yp_MAX_ALIGNMENT == 0,
        alignof_bytes_inline_data);

#define ypBytes_DATA(b) ((yp_uint8_t *)ypStringLib_DATA(b))
#define ypBytes_LEN ypStringLib_LEN
#define ypBytes_SET_LEN ypStringLib_SET_LEN
#define ypBytes_ALLOCLEN ypStringLib_ALLOCLEN
#define ypBytes_INLINE_DATA ypStringLib_INLINE_DATA

// The maximum possible alloclen and len of a bytes
#define ypBytes_ALLOCLEN_MAX ypStringLib_ALLOCLEN_MAX
#define ypBytes_LEN_MAX ypStringLib_LEN_MAX

#define ypBytes_ASSERT_INVARIANTS(b)                                                 \
    do {                                                                             \
        yp_ASSERT(ypObject_TYPE_PAIR_CODE(b) == ypBytes_CODE, "bad type for bytes"); \
        yp_ASSERT(ypStringLib_ENC_CODE(b) == ypStringLib_ENC_CODE_BYTES,             \
                "bad StrLib_ENC for bytes");                                         \
        ypStringLib_ASSERT_INVARIANTS(b);                                            \
    } while (0)

// Moves the bytes from [src:] to the index dest; this can be used when deleting bytes, or
// inserting bytes (the new space is uninitialized). Assumes enough space is allocated for the
// move. Recall that memmove handles overlap. Also adjusts null terminator.
#define ypBytes_ELEMMOVE(b, dest, src) \
    yp_memmove(ypBytes_DATA(b) + (dest), ypBytes_DATA(b) + (src), ypBytes_LEN(b) - (src) + 1);

// When byte arrays are accepted from C, a negative len indicates that strlen(source) should be
// used as the length. This function updates *len accordingly. Returns false if the final value
// of *len would be larger than ypBytes_LEN_MAX, in which case *len is undefined and
// yp_MemorySizeOverflowError should probably be returned.
static int ypBytes_adjust_lenC(yp_ssize_t *len, const yp_uint8_t *source)
{
    if (*len < 0) {
        if (source == NULL) {
            *len = 0;
        } else {
            size_t ulen = strlen((const char *)source);
            if (ulen > ypBytes_LEN_MAX) return FALSE;
            *len = (yp_ssize_t)ulen;
        }
    } else if (*len > ypBytes_LEN_MAX) {
        return FALSE;
    }
    return TRUE;
}

// If x is a bool/int in range(256), store value in storage and set *x_data=storage, *x_len=1. If x
// is a fellow bytes, set *x_data and *x_len. Otherwise, returns an exception. *x_data may or may
// not be null terminated.
static ypObject *_ypBytes_coerce_intorbytes(
        ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len, yp_uint8_t *storage)
{
    ypObject *exc = yp_None;
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);

    // FIXME We should make bools non-numeric, I think, which means removing from here.
    if (x_pair == ypBool_CODE || x_pair == ypInt_CODE) {
        *storage = _ypBytes_asuint8C(x, &exc);
        if (yp_isexceptionC(exc)) return exc;
        *x_data = storage;
        *x_len = 1;
        return yp_None;
    } else if (x_pair == ypBytes_CODE) {
        *x_data = ypBytes_DATA(x);
        *x_len = ypBytes_LEN(x);
        return yp_None;
    } else {
        *x_data = storage;  // better than NULL
        *x_len = 0;
        return_yp_BAD_TYPE(x);
    }
}


// Public Methods

static ypObject *bytes_unfrozen_copy(ypObject *b)
{
    return ypStringLib_new_copy(ypByteArray_CODE, b, /*alloclen_fixed=*/FALSE);
}

static ypObject *bytes_frozen_copy(ypObject *b) { return ypStringLib_copy(ypBytes_CODE, b); }

// XXX Check for the yp_bytes_empty case first
static ypObject *_ypBytes_deepcopy(int type, ypObject *b, void *copy_memo)
{
    ypObject *b_copy = ypStringLib_new_copy(type, b, /*alloclen_fixed=*/TRUE);
    ypObject *result = _yp_deepcopy_memo_setitem(copy_memo, b, b_copy);
    if (yp_isexceptionC(result)) {
        yp_decref(b_copy);
        return result;
    }
    return b_copy;
}

static ypObject *bytes_unfrozen_deepcopy(ypObject *b, visitfunc copy_visitor, void *copy_memo)
{
    return _ypBytes_deepcopy(ypByteArray_CODE, b, copy_memo);
}

static ypObject *bytes_frozen_deepcopy(ypObject *b, visitfunc copy_visitor, void *copy_memo)
{
    if (ypBytes_LEN(b) < 1) return yp_bytes_empty;
    return _ypBytes_deepcopy(ypBytes_CODE, b, copy_memo);
}

static ypObject *bytes_bool(ypObject *b) { return ypBool_FROM_C(ypBytes_LEN(b)); }

static ypObject *bytes_find(ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        findfunc_direction direction, yp_ssize_t *i)
{
    yp_uint8_t *x_data;
    yp_ssize_t  x_len;
    yp_uint8_t  storage;
    yp_ssize_t  step = 1;
    yp_ssize_t  slicelength;
    ypObject   *result;

    result = _ypBytes_coerce_intorbytes(x, &x_data, &x_len, &storage);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT(x_data != NULL, "_ypBytes_coerce_intorbytes unexpectedly returned NULL");

    // XXX Unlike Python, the arguments start and end are always treated as in slice notation.
    // Python behaves peculiarly when end<start in certain edge cases involving empty strings. See
    // https://bugs.python.org/issue24243.
    result = ypSlice_AdjustIndicesC(ypStringLib_LEN(b), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    *i = _ypStringLib_find(b, x_data, x_len, start, slicelength, direction);
    return yp_None;
}

static ypObject *bytes_concat(ypObject *b, ypObject *iterable)
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);

    if (iterable_pair == ypBytes_CODE) {
        return ypStringLib_concat_fromstring(b, iterable);
    } else if (iterable_pair == ypStr_CODE) {
        return yp_TypeError;
    } else {
        return ypStringLib_concat_fromiterable(b, iterable);
    }
}

// TODO Do we want a special-case for yp_intC that goes direct to the prealloc array?
static ypObject *bytes_getindex(ypObject *b, yp_ssize_t i, ypObject *defval)
{
    if (!ypSequence_AdjustIndexC(ypBytes_LEN(b), &i)) {
        if (defval == NULL) return yp_IndexError;
        return yp_incref(defval);
    }
    return yp_intC(ypBytes_DATA(b)[i]);
}

static ypObject *bytearray_setindex(ypObject *b, yp_ssize_t i, ypObject *x)
{
    ypObject  *exc = yp_None;
    yp_uint8_t x_value;

    x_value = _ypBytes_asuint8C(x, &exc);
    if (yp_isexceptionC(exc)) return exc;

    if (!ypSequence_AdjustIndexC(ypBytes_LEN(b), &i)) {
        return yp_IndexError;
    }

    ypBytes_DATA(b)[i] = x_value;
    return yp_None;
}

static ypObject *bytearray_delindex(ypObject *b, yp_ssize_t i)
{
    if (!ypSequence_AdjustIndexC(ypBytes_LEN(b), &i)) {
        return yp_IndexError;
    }

    ypBytes_ELEMMOVE(b, i, i + 1);
    ypBytes_SET_LEN(b, ypBytes_LEN(b) - 1);
    return yp_None;
}

static ypObject *_ypBytes_fromiterable(int type, ypObject *iterable);
static ypObject *bytearray_setslice(
        ypObject *b, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, ypObject *x)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypBytes_CODE) {
        return ypStringLib_setslice_fromstring(b, start, stop, step, x);
    } else if (x_pair == ypInt_CODE || x_pair == ypStr_CODE) {
        return yp_TypeError;
    } else {
        ypObject *result;
        ypObject *x_asbytes = _ypBytes_fromiterable(ypBytes_CODE, x);
        if (yp_isexceptionC(x_asbytes)) return x_asbytes;
        result = ypStringLib_setslice_fromstring(b, start, stop, step, x_asbytes);
        yp_decref(x_asbytes);
        return result;
    }
}

static ypObject *bytearray_extend(ypObject *b, ypObject *iterable)
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);

    if (iterable_pair == ypBytes_CODE) {
        return ypStringLib_extend_fromstring(b, iterable);
    } else if (iterable_pair == ypStr_CODE) {
        return yp_TypeError;
    } else {
        return ypStringLib_extend_fromiterable(b, iterable);
    }
}

static ypObject *bytearray_insert(ypObject *b, yp_ssize_t i, ypObject *x)
{
    ypObject  *exc = yp_None;
    yp_uint8_t x_asbyte;

    // Recall that insert behaves like b[i:i]=[x], but i can't be yp_SLICE_DEFAULT.
    if (i == yp_SLICE_DEFAULT) return yp_TypeError;
    x_asbyte = _ypBytes_asuint8C(x, &exc);
    if (yp_isexceptionC(exc)) return exc;

    // It's possible we will need to reallocate b in order to add the byte. The logic to do this is
    // already in ypStringLib_setslice_fromstring.
    return ypStringLib_setslice_fromstring7(b, i, i, 1, &x_asbyte, 1, ypStringLib_enc_bytes);
}

static ypObject *bytearray_popindex(ypObject *b, yp_ssize_t i)
{
    ypObject *result;

    if (!ypSequence_AdjustIndexC(ypBytes_LEN(b), &i)) {
        return yp_IndexError;
    }

    result = yp_intC(ypBytes_DATA(b)[i]);
    ypBytes_ELEMMOVE(b, i, i + 1);
    ypBytes_SET_LEN(b, ypBytes_LEN(b) - 1);
    ypBytes_ASSERT_INVARIANTS(b);
    return result;
}

// XXX Adapted from Python's reverse_slice.
static ypObject *bytearray_reverse(ypObject *b)
{
    yp_uint8_t *lo = ypBytes_DATA(b);
    yp_uint8_t *hi = lo + ypBytes_LEN(b) - 1;
    while (lo < hi) {
        yp_uint8_t t = *lo;
        *lo = *hi;
        *hi = t;
        lo += 1;
        hi -= 1;
    }
    ypBytes_ASSERT_INVARIANTS(b);
    return yp_None;
}

static ypObject *bytes_contains(ypObject *b, ypObject *x)
{
    ypObject  *result;
    yp_ssize_t i = -1;

    result = bytes_find(b, x, 0, yp_SLICE_LAST, yp_FIND_FORWARD, &i);
    if (yp_isexceptionC(result)) return result;
    return ypBool_FROM_C(i >= 0);
}

static ypObject *bytes_len(ypObject *b, yp_ssize_t *len)
{
    *len = ypBytes_LEN(b);
    return yp_None;
}

static ypObject *bytearray_push(ypObject *b, ypObject *x)
{
    ypObject   *exc = yp_None;
    yp_uint32_t x_asitem;
    ypObject   *result;

    x_asitem = _ypBytes_asuint8C(x, &exc);
    if (yp_isexceptionC(exc)) return exc;

    // TODO Overallocate?
    result = ypStringLib_push(b, x_asitem, ypStringLib_enc_bytes, 0);
    ypBytes_DATA(b)[ypBytes_LEN(b)] = '\0';

    ypBytes_ASSERT_INVARIANTS(b);
    return result;
}

static ypObject *bytearray_pop(ypObject *b)
{
    ypObject *result;

    if (ypBytes_LEN(b) < 1) return yp_IndexError;
    result = yp_intC(ypBytes_DATA(b)[ypBytes_LEN(b) - 1]);
    ypBytes_SET_LEN(b, ypBytes_LEN(b) - 1);
    ypBytes_DATA(b)[ypBytes_LEN(b)] = '\0';
    ypBytes_ASSERT_INVARIANTS(b);
    return result;
}

// onmissing must be an immortal, or NULL.
static ypObject *bytearray_remove(ypObject *b, ypObject *x, ypObject *onmissing)
{
    yp_uint8_t *x_data;
    yp_ssize_t  x_len;
    yp_uint8_t  storage;
    ypObject   *result;
    yp_ssize_t  i;

    result = _ypBytes_coerce_intorbytes(x, &x_data, &x_len, &storage);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT(x_data != NULL, "_ypBytes_coerce_intorbytes unexpectedly returned NULL");

    i = _ypStringLib_find(b, x_data, x_len, 0, ypBytes_LEN(b), yp_FIND_FORWARD);
    if (i < 0) goto missing;

    // We found a match to remove
    ypBytes_ELEMMOVE(b, i, i + x_len);
    ypBytes_SET_LEN(b, ypBytes_LEN(b) - x_len);
    ypBytes_ASSERT_INVARIANTS(b);
    return yp_None;

missing:
    if (onmissing == NULL) return yp_ValueError;
    return onmissing;
}

// TODO allow custom min/max methods?

static ypObject *bytes_count(
        ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t *n)
{
    yp_uint8_t *x_data;
    yp_ssize_t  x_len;
    yp_uint8_t  storage;
    ypObject   *result;
    yp_ssize_t  step = 1;
    yp_ssize_t  slicelength;

    result = _ypBytes_coerce_intorbytes(x, &x_data, &x_len, &storage);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT(x_data != NULL, "_ypBytes_coerce_intorbytes unexpectedly returned NULL");

    // XXX Unlike Python, the arguments start and stop are always treated as in slice notation.
    // Python behaves peculiarly when stop<start in certain edge cases involving empty strings. See
    // https://bugs.python.org/issue24243.
    result = ypSlice_AdjustIndicesC(ypBytes_LEN(b), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    *n = _ypStringLib_count(b, x_data, x_len, start, slicelength);
    return yp_None;
}

static ypObject *bytes_isalnum(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isalpha(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isdecimal(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isdigit(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isidentifier(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_islower(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isnumeric(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isprintable(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isspace(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_isupper(ypObject *b) { return yp_NotImplementedError; }

static ypObject *_bytes_tailmatch(
        ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t end, findfunc_direction direction)
{
    yp_ssize_t step = 1;
    yp_ssize_t slice_len;
    ypObject  *result;
    yp_ssize_t cmp_start;
    int        memcmp_result;

    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return_yp_BAD_TYPE(x);

    // XXX Unlike Python, the arguments start and end are always treated as in slice notation.
    // Python behaves peculiarly when end<start in certain edge cases involving empty strings. See
    // https://bugs.python.org/issue24243.
    result = ypSlice_AdjustIndicesC(ypBytes_LEN(b), &start, &end, &step, &slice_len);
    if (yp_isexceptionC(result)) return result;

    // If the prefix is longer than the slice, the slice can't possibly start with it
    if (ypBytes_LEN(x) > slice_len) return yp_False;

    if (direction == yp_FIND_REVERSE) {
        cmp_start = end - ypBytes_LEN(x);  // endswith
    } else {
        cmp_start = start;  // startswith
    }

    memcmp_result = yp_memcmp(ypBytes_DATA(b) + cmp_start, ypBytes_DATA(x), ypBytes_LEN(x));
    return ypBool_FROM_C(memcmp_result == 0);
}

static ypObject *_bytes_startswith_or_endswith(
        ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t end, findfunc_direction direction)
{
    // FIXME Also support lists?  Python requires a tuple here...
    if (ypObject_TYPE_CODE(x) == ypTuple_CODE) {
        yp_ssize_t i;
        for (i = 0; i < ypTuple_LEN(x); i++) {
            ypObject *result = _bytes_tailmatch(b, ypTuple_ARRAY(x)[i], start, end, direction);
            if (result != yp_False) return result;  // yp_True or an exception
        }
        return yp_False;
    } else {
        return _bytes_tailmatch(b, x, start, end, direction);
    }
}

static ypObject *bytes_startswith(ypObject *b, ypObject *prefix, yp_ssize_t start, yp_ssize_t end)
{
    return _bytes_startswith_or_endswith(b, prefix, start, end, yp_FIND_FORWARD);
}

static ypObject *bytes_endswith(ypObject *b, ypObject *suffix, yp_ssize_t start, yp_ssize_t end)
{
    return _bytes_startswith_or_endswith(b, suffix, start, end, yp_FIND_REVERSE);
}

static ypObject *bytes_lower(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_upper(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_casefold(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_swapcase(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_capitalize(ypObject *b) { return yp_NotImplementedError; }

static ypObject *bytes_ljust(ypObject *b, yp_ssize_t width, yp_int_t ord_fillchar)
{
    return yp_NotImplementedError;
}

static ypObject *bytes_rjust(ypObject *b, yp_ssize_t width, yp_int_t ord_fillchar)
{
    return yp_NotImplementedError;
}

static ypObject *bytes_center(ypObject *b, yp_ssize_t width, yp_int_t ord_fillchar)
{
    return yp_NotImplementedError;
}

static ypObject *bytes_expandtabs(ypObject *b, yp_ssize_t tabsize)
{
    return yp_NotImplementedError;
}

static ypObject *bytes_replace(ypObject *b, ypObject *oldsub, ypObject *newsub, yp_ssize_t count)
{
    if (ypObject_TYPE_PAIR_CODE(oldsub) != ypBytes_CODE) return_yp_BAD_TYPE(oldsub);
    if (ypObject_TYPE_PAIR_CODE(newsub) != ypBytes_CODE) return_yp_BAD_TYPE(newsub);
    if ((ypBytes_LEN(oldsub) < 1 && ypBytes_LEN(newsub) < 1) || count == 0) {
        return ypStringLib_copy(ypObject_TYPE_CODE(b), b);
    }
    return yp_NotImplementedError;
}

static ypObject *bytes_lstrip(ypObject *b, ypObject *chars) { return yp_NotImplementedError; }

static ypObject *bytes_rstrip(ypObject *b, ypObject *chars) { return yp_NotImplementedError; }

static ypObject *bytes_strip(ypObject *b, ypObject *chars) { return yp_NotImplementedError; }

static void bytes_partition(
        ypObject *b, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2)
{
    *part0 = *part1 = *part2 = yp_NotImplementedError;
}

static void bytes_rpartition(
        ypObject *b, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2)
{
    *part0 = *part1 = *part2 = yp_NotImplementedError;
}

static ypObject *bytes_split(ypObject *b, ypObject *sep, yp_ssize_t maxsplit)
{
    return yp_NotImplementedError;
}

static ypObject *bytes_rsplit(ypObject *b, ypObject *sep, yp_ssize_t maxsplit)
{
    return yp_NotImplementedError;
}

static ypObject *bytes_splitlines(ypObject *b, ypObject *keepends)
{
    return yp_NotImplementedError;
}

// Returns -1, 0, or 1 as per memcmp
static int _ypBytes_relative_cmp(ypObject *b, ypObject *x)
{
    yp_ssize_t b_len = ypBytes_LEN(b);
    yp_ssize_t x_len = ypBytes_LEN(x);
    int        cmp = yp_memcmp(ypBytes_DATA(b), ypBytes_DATA(x), MIN(b_len, x_len));
    if (cmp == 0) cmp = b_len < x_len ? -1 : (b_len > x_len);
    return cmp;
}
static ypObject *bytes_lt(ypObject *b, ypObject *x)
{
    if (b == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(_ypBytes_relative_cmp(b, x) < 0);
}
static ypObject *bytes_le(ypObject *b, ypObject *x)
{
    if (b == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(_ypBytes_relative_cmp(b, x) <= 0);
}
static ypObject *bytes_ge(ypObject *b, ypObject *x)
{
    if (b == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(_ypBytes_relative_cmp(b, x) >= 0);
}
static ypObject *bytes_gt(ypObject *b, ypObject *x)
{
    if (b == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(_ypBytes_relative_cmp(b, x) > 0);
}

// Returns true (1) if the two bytes/bytearrays are equal. Size is a quick way to check equality.
// TODO Would the pre-computed hash be a quick check for inequality before the memcmp?
static int _ypBytes_are_equal(ypObject *b, ypObject *x)
{
    yp_ssize_t b_len = ypBytes_LEN(b);
    yp_ssize_t x_len = ypBytes_LEN(x);
    if (b_len != x_len) return 0;
    return yp_memcmp(ypBytes_DATA(b), ypBytes_DATA(x), b_len) == 0;
}
static ypObject *bytes_eq(ypObject *b, ypObject *x)
{
    if (b == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(_ypBytes_are_equal(b, x));
}
static ypObject *bytes_ne(ypObject *b, ypObject *x)
{
    if (b == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypBytes_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(!_ypBytes_are_equal(b, x));
}

// Must work even for mutables; yp_hash handles caching this value and denying its use for mutables
static ypObject *bytes_currenthash(
        ypObject *b, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    *hash = yp_HashBytes(ypBytes_DATA(b), ypBytes_LEN(b));

    // Since we never contain mutable objects, we can cache our hash
    if (!ypObject_IS_MUTABLE(b)) ypObject_CACHED_HASH(b) = *hash;
    return yp_None;
}

static ypObject *bytes_dealloc(ypObject *b, void *memo)
{
    ypMem_FREE_CONTAINER(b, ypBytesObject);
    return yp_None;
}

static ypObject *_ypBytes_encode(int type, ypObject *source, ypObject *encoding, ypObject *errors);
static ypObject *_ypBytes(int type, ypObject *source);
static ypObject *_ypBytes_func_new_code(int type, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 5, "unexpected argarray of length %" PRIssize, n);

    if (argarray[3] != yp_Arg_Missing) {  // TODO ...or just use None?
        ypObject *errors = argarray[4] == yp_Arg_Missing ? yp_s_strict : argarray[4];  // borrowed
        return _ypBytes_encode(type, argarray[2], argarray[3], errors);
    } else if (argarray[4] != yp_Arg_Missing) {
        // Either "string argument without an encoding" or "errors without a string argument".
        return yp_TypeError;
    } else {
        return _ypBytes(type, argarray[2]);
    }
}

static ypObject *bytes_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT1(argarray[0] == yp_t_bytes);
    return _ypBytes_func_new_code(ypBytes_CODE, n, argarray);
}

static ypObject *bytearray_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT1(argarray[0] == yp_t_bytearray);
    return _ypBytes_func_new_code(ypByteArray_CODE, n, argarray);
}

#define _ypBytes_FUNC_NEW_PARAMETERS                                     \
    ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_slash), NULL},   \
            {yp_CONST_REF(yp_s_source), yp_CONST_REF(yp_bytes_empty)},   \
            {yp_CONST_REF(yp_s_encoding), yp_CONST_REF(yp_Arg_Missing)}, \
            {yp_CONST_REF(yp_s_errors), yp_CONST_REF(yp_Arg_Missing)})
yp_IMMORTAL_FUNCTION_static(bytes_func_new, bytes_func_new_code, _ypBytes_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(
        bytearray_func_new, bytearray_func_new_code, _ypBytes_FUNC_NEW_PARAMETERS);

static ypSequenceMethods ypBytes_as_sequence = {
        bytes_concat,                 // tp_concat
        ypStringLib_repeat,           // tp_repeat
        bytes_getindex,               // tp_getindex
        ypStringLib_getslice,         // tp_getslice
        bytes_find,                   // tp_find
        bytes_count,                  // tp_count
        MethodError_objssizeobjproc,  // tp_setindex
        MethodError_objsliceobjproc,  // tp_setslice
        MethodError_objssizeproc,     // tp_delindex
        MethodError_objsliceproc,     // tp_delslice
        MethodError_objobjproc,       // tp_append
        MethodError_objobjproc,       // tp_extend
        MethodError_objssizeproc,     // tp_irepeat
        MethodError_objssizeobjproc,  // tp_insert
        MethodError_objssizeproc,     // tp_popindex
        MethodError_objproc,          // tp_reverse
        MethodError_objobjobjproc     // tp_sort
};

static ypTypeObject ypBytes_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(bytes_func_new),  // tp_func_new
        bytes_dealloc,                 // tp_dealloc
        NoRefs_traversefunc,           // tp_traverse
        NULL,                          // tp_str
        NULL,                          // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,      // tp_freeze
        bytes_unfrozen_copy,      // tp_unfrozen_copy
        bytes_frozen_copy,        // tp_frozen_copy
        bytes_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        bytes_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,      // tp_invalidate

        // Boolean operations and comparisons
        bytes_bool,  // tp_bool
        bytes_lt,    // tp_lt
        bytes_le,    // tp_le
        bytes_eq,    // tp_eq
        bytes_ne,    // tp_ne
        bytes_ge,    // tp_ge
        bytes_gt,    // tp_gt

        // Generic object operations
        bytes_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        bytes_contains,             // tp_contains
        bytes_len,                  // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypBytes_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypSequenceMethods ypByteArray_as_sequence = {
        bytes_concat,              // tp_concat
        ypStringLib_repeat,        // tp_repeat
        bytes_getindex,            // tp_getindex
        ypStringLib_getslice,      // tp_getslice
        bytes_find,                // tp_find
        bytes_count,               // tp_count
        bytearray_setindex,        // tp_setindex
        bytearray_setslice,        // tp_setslice
        bytearray_delindex,        // tp_delindex
        ypStringLib_delslice,      // tp_delslice
        bytearray_push,            // tp_append
        bytearray_extend,          // tp_extend
        ypStringLib_irepeat,       // tp_irepeat
        bytearray_insert,          // tp_insert
        bytearray_popindex,        // tp_popindex
        bytearray_reverse,         // tp_reverse
        MethodError_objobjobjproc  // tp_sort
};

static ypTypeObject ypByteArray_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(bytearray_func_new),  // tp_func_new
        bytes_dealloc,                     // tp_dealloc
        NoRefs_traversefunc,               // tp_traverse
        NULL,                              // tp_str
        NULL,                              // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,      // tp_freeze
        bytes_unfrozen_copy,      // tp_unfrozen_copy
        bytes_frozen_copy,        // tp_frozen_copy
        bytes_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        bytes_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,      // tp_invalidate

        // Boolean operations and comparisons
        bytes_bool,  // tp_bool
        bytes_lt,    // tp_lt
        bytes_le,    // tp_le
        bytes_eq,    // tp_eq
        bytes_ne,    // tp_ne
        bytes_ge,    // tp_ge
        bytes_gt,    // tp_gt

        // Generic object operations
        bytes_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        bytes_contains,             // tp_contains
        bytes_len,                  // tp_len
        bytearray_push,             // tp_push
        ypStringLib_clear,          // tp_clear
        bytearray_pop,              // tp_pop
        bytearray_remove,           // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        _ypSequence_setitem,        // tp_setitem
        _ypSequence_delitem,        // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypByteArray_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypObject *_yp_asbytesCX(ypObject *seq, yp_ssize_t *len, const yp_uint8_t **bytes)
{
    if (ypObject_TYPE_PAIR_CODE(seq) != ypBytes_CODE) return_yp_BAD_TYPE(seq);

    *bytes = ypBytes_DATA(seq);
    if (len == NULL) {
        if ((yp_ssize_t)strlen(*bytes) != ypBytes_LEN(seq)) return yp_TypeError;
    } else {
        *len = ypBytes_LEN(seq);
    }
    return yp_None;
}
ypObject *yp_asbytesCX(ypObject *seq, yp_ssize_t *len, const yp_uint8_t **bytes)
{
    ypObject *result = _yp_asbytesCX(seq, len, bytes);
    if (yp_isexceptionC(result)) {
        if (len != NULL) *len = 0;
        *bytes = NULL;
    }
    return result;
}

// Public constructors

static ypObject *_ypBytesC(int type, yp_ssize_t len, const yp_uint8_t *source)
{
    ypObject *b;
    yp_ASSERT(len >= 0, "negative len not allowed (do ypBytes_adjust_lenC before _ypBytesC*)");

    if (len < 1) {
        if (type == ypBytes_CODE) return yp_bytes_empty;
        return yp_bytearray0();
    }
    b = _ypBytes_new(type, len, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(b)) return b;

    // Initialize the data
    if (source == NULL) {
        yp_memset(ypBytes_DATA(b), 0, len + 1);
    } else {
        yp_memcpy(ypBytes_DATA(b), source, len);
        ypBytes_DATA(b)[len] = '\0';
    }
    ypBytes_SET_LEN(b, len);
    ypBytes_ASSERT_INVARIANTS(b);
    return b;
}
// TODO Be consistent and always put "len" params before the thing they are sizing. This breaks with
// Python but, conceptually at least, you need the length of something before you use it.
ypObject *yp_bytesC(yp_ssize_t len, const yp_uint8_t *source)
{
    if (!ypBytes_adjust_lenC(&len, source)) return yp_MemorySizeOverflowError;
    return _ypBytesC(ypBytes_CODE, len, source);
}
ypObject *yp_bytearrayC(yp_ssize_t len, const yp_uint8_t *source)
{
    if (!ypBytes_adjust_lenC(&len, source)) return yp_MemorySizeOverflowError;
    return _ypBytesC(ypByteArray_CODE, len, source);
}

static ypObject *_ypBytes_encode(int type, ypObject *source, ypObject *encoding, ypObject *errors)
{
    ypObject *result;
    if (ypObject_TYPE_PAIR_CODE(source) != ypStr_CODE) return_yp_BAD_TYPE(source);

    // XXX Not handling errors in yp_eq yet because this is just temporary
    if (yp_eq(encoding, yp_s_utf_8) != yp_True) return yp_NotImplementedError;

    // TODO Python limits this to codecs that identify themselves as text encodings: do the same
    result = ypStringLib_encode_utf_8(type, source, errors);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT(ypObject_TYPE_CODE(result) == type, "text encoding didn't return correct type");
    ypBytes_ASSERT_INVARIANTS(result);
    return result;
}
ypObject *yp_bytes3(ypObject *source, ypObject *encoding, ypObject *errors)
{
    return _ypBytes_encode(ypBytes_CODE, source, encoding, errors);
}
ypObject *yp_bytearray3(ypObject *source, ypObject *encoding, ypObject *errors)
{
    return _ypBytes_encode(ypByteArray_CODE, source, encoding, errors);
}
ypObject *yp_encode3(ypObject *s, ypObject *encoding, ypObject *errors)
{
    return _ypBytes_encode(
            ypObject_IS_MUTABLE(s) ? ypByteArray_CODE : ypBytes_CODE, s, encoding, errors);
}
ypObject *yp_encode(ypObject *s)
{
    if (ypObject_TYPE_PAIR_CODE(s) != ypStr_CODE) return_yp_BAD_TYPE(s);
    return ypStringLib_encode_utf_8(
            ypObject_IS_MUTABLE(s) ? ypByteArray_CODE : ypBytes_CODE, s, yp_s_strict);
}

static ypObject *_ypBytes_fromiterable(int type, ypObject *iterable)
{
    ypObject  *exc = yp_None;
    yp_ssize_t length_hint;
    ypObject  *newB;
    ypObject  *result;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(iterable) != ypBytes_CODE, "call ypStringLib_copy instead");
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(iterable) != ypStr_CODE, "raise yp_TypeError earlier");

    length_hint = yp_lenC(iterable, &exc);
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint: it just means we can't pre-allocate
        length_hint = yp_length_hintC(iterable, &exc);
        if (length_hint > ypBytes_LEN_MAX) length_hint = ypBytes_LEN_MAX;
    } else if (length_hint < 1) {
        // yp_lenC reports an empty iterable, so we can shortcut ypStringLib_extend_fromiterable
        if (type == ypBytes_CODE) return yp_bytes_empty;
        return yp_bytearray0();
    } else if (length_hint > ypBytes_LEN_MAX) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    newB = _ypBytes_new(type, length_hint, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newB)) return newB;
    result = ypStringLib_extend_fromiterable(newB, iterable);
    if (yp_isexceptionC(result)) {
        yp_decref(newB);
        return result;
    }

    // TODO We could avoid allocating for an empty iterable altogether if we get the first value
    // before allocating; is this complication worth the optimization?
    if (type == ypBytes_CODE && ypBytes_LEN(newB) < 1) {
        yp_decref(newB);
        return yp_bytes_empty;
    }

    ypBytes_ASSERT_INVARIANTS(newB);
    return newB;
}
static ypObject *_ypBytes(int type, ypObject *source)
{
    ypObject *exc = yp_None;
    int       source_pair = ypObject_TYPE_PAIR_CODE(source);

    if (source_pair == ypBytes_CODE) {
        return ypStringLib_copy(type, source);
    } else if (source_pair == ypInt_CODE) {
        yp_ssize_t len = yp_index_asssizeC(source, &exc);
        if (yp_isexceptionC(exc)) return exc;
        if (len < 0) return yp_ValueError;
        return _ypBytesC(type, len, NULL);
    } else if (source_pair == ypStr_CODE) {
        return yp_TypeError;
    } else {
        // Treat it as a generic iterable
        return _ypBytes_fromiterable(type, source);
    }
}
ypObject *yp_bytes(ypObject *source) { return _ypBytes(ypBytes_CODE, source); }
ypObject *yp_bytearray(ypObject *source) { return _ypBytes(ypByteArray_CODE, source); }

// There is no yp_bytes0 because we export yp_bytes_empty directly.

ypObject *yp_bytearray0(void)
{
    ypObject *newB = _ypBytes_new(ypByteArray_CODE, 0, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newB)) return newB;
    ypBytes_DATA(newB)[0] = '\0';
    ypBytes_ASSERT_INVARIANTS(newB);
    return newB;
}

#pragma endregion bytes


/*************************************************************************************************
 * Sequence of unicode characters
 *************************************************************************************************/
#pragma region str

// TODO http://www.python.org/dev/peps/pep-0393/ (flexible string representations)

typedef ypStringLibObject ypStrObject;

// XXX str will only ever store Latin-1, ucs-2, or ucs-4 data, so only needs 4-byte alignment
yp_STATIC_ASSERT(yp_offsetof(ypStringLibObject, ob_inline_data) % 4 == 0, alignof_str_inline_data);

// TODO pre-allocate static chrs in, say, range(255), or whatever seems appropriate

#define ypStr_ENC_CODE ypStringLib_ENC_CODE
#define ypStr_ENC ypStringLib_ENC
#define ypStr_DATA ypStringLib_DATA
#define ypStr_LEN ypStringLib_LEN
#define ypStr_SET_LEN ypStringLib_SET_LEN
#define ypStr_ALLOCLEN ypStringLib_ALLOCLEN
#define ypStr_INLINE_DATA ypStringLib_INLINE_DATA

#define ypStr_ALLOCLEN_MAX ypStringLib_ALLOCLEN_MAX
#define ypStr_LEN_MAX ypStringLib_LEN_MAX

#define ypStr_ASSERT_INVARIANTS(s)                                                            \
    do {                                                                                      \
        yp_ASSERT(ypObject_TYPE_PAIR_CODE(s) == ypStr_CODE, "bad type for str");              \
        yp_ASSERT(ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_BYTES, "bad StrLib_ENC for str"); \
        ypStringLib_ASSERT_INVARIANTS(s);                                                     \
    } while (0)

#define ypStr_MEMCMP ypStringLib_MEMCMP
#define ypStr_ELEMMOVE ypStringLib_ELEMMOVE

// If x is a str/chrarray that can be encoded as per enc_code, sets *x_data and *x_len to that
// encoded string; this may be x's own data buffer, or a newly-allocated buffer. If x is a
// str/chrarray that cannot be encoded as per enc_code, sets *x_data=NULL and *x_len=0; depending on
// context, this is not necessarily an error (i.e. yp_contains would return false in this case).
// Otherwise, returns an exception. *x_data may or may not be null terminated. If this function
// succeeds, you will need to call _ypStr_coerce_encoding_free to deallocate appropriately.
static ypObject *_ypStr_coerce_encoding(
        ypObject *x, const ypStringLib_encinfo *enc, void **x_data, yp_ssize_t *x_len)
{
    int        dest_sizeshift = enc->sizeshift;
    int        src_sizeshift;
    yp_ssize_t alloclen;

    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return_yp_BAD_TYPE(x);
    src_sizeshift = ypStr_ENC(x)->sizeshift;

    // Ideal path: no allocations required.
    if (dest_sizeshift == src_sizeshift) {
        *x_data = ypStr_DATA(x);
        *x_len = ypStr_LEN(x);
        return yp_None;
    }

    // Being unable to convert is not always an error: in find, it means the string is not present.
    if (dest_sizeshift < src_sizeshift) {
        *x_data = NULL;
        *x_len = 0;
        return yp_None;
    }

    // Otherwise, we need to allocate a new buffer to hold this data. Don't bother writing the null
    // terminator.
    *x_data = yp_malloc(&alloclen, ypStr_LEN(x) << dest_sizeshift);
    if (*x_data == NULL) return yp_MemoryError;
    ypStringLib_elemcopy_maybeupconvert(
            dest_sizeshift, *x_data, 0, src_sizeshift, ypStr_DATA(x), 0, ypStr_LEN(x));
    *x_len = ypStr_LEN(x);
    return yp_None;
}

// Cleans up after _ypStr_coerce_encoding. Safe to call for x_data=NULL.
static void _ypStr_coerce_encoding_free(ypObject *x, void *x_data)
{
    if (x_data != ypStr_DATA(x) && x_data != NULL) {
        yp_free(x_data);
    }
}


static ypObject *str_unfrozen_copy(ypObject *s)
{
    return ypStringLib_new_copy(ypChrArray_CODE, s, TRUE);
}

static ypObject *str_frozen_copy(ypObject *s) { return ypStringLib_copy(ypStr_CODE, s); }

// XXX Check for the yp_str_empty case first
static ypObject *_ypStr_deepcopy(int type, ypObject *s, void *copy_memo)
{
    ypObject *s_copy = ypStringLib_new_copy(type, s, /*alloclen_fixed=*/TRUE);
    ypObject *result = _yp_deepcopy_memo_setitem(copy_memo, s, s_copy);
    if (yp_isexceptionC(result)) {
        yp_decref(s_copy);
        return result;
    }
    return s_copy;
}

static ypObject *str_unfrozen_deepcopy(ypObject *s, visitfunc copy_visitor, void *copy_memo)
{
    return _ypStr_deepcopy(ypChrArray_CODE, s, copy_memo);
}

static ypObject *str_frozen_deepcopy(ypObject *s, visitfunc copy_visitor, void *copy_memo)
{
    if (ypStr_LEN(s) < 1) return yp_str_empty;
    return _ypStr_deepcopy(ypStr_CODE, s, copy_memo);
}

static ypObject *str_bool(ypObject *s) { return ypBool_FROM_C(ypStr_LEN(s)); }

static ypObject *str_concat(ypObject *s, ypObject *iterable)
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);

    if (iterable_pair == ypStr_CODE) {
        return ypStringLib_concat_fromstring(s, iterable);
    } else if (iterable_pair == ypBytes_CODE) {
        return yp_TypeError;
    } else {
        return ypStringLib_concat_fromiterable(s, iterable);
    }
}

static ypObject *str_getindex(ypObject *s, yp_ssize_t i, ypObject *defval)
{
    if (!ypSequence_AdjustIndexC(ypStr_LEN(s), &i)) {
        if (defval == NULL) return yp_IndexError;
        return yp_incref(defval);
    }
    return yp_chrC(ypStr_ENC(s)->getindexX(ypStr_DATA(s), i));
}

#define str_getslice ypStringLib_getslice

static ypObject *chrarray_setindex(ypObject *s, yp_ssize_t i, ypObject *x)
{
    ypObject                  *result;
    yp_uint32_t                x_asitem;
    const ypStringLib_encinfo *x_enc;

    result = _ypStr_asitemC(x, &x_asitem, &x_enc);
    if (yp_isexceptionC(result)) return result;

    if (!ypSequence_AdjustIndexC(ypStr_LEN(s), &i)) {
        return yp_IndexError;
    }

    // It's possible we will need to either upconvert or downconvert s in order to replace s[i]
    // with x. The logic to do this is already implemented in ypStringLib_setslice_fromstring.
    // TODO Do we want a setslice that asserts the slice arguments are already adjusted?
    return ypStringLib_setslice_fromstring7(s, i, i + 1, 1, &x_asitem, 1, x_enc);
}

static ypObject *chrarray_delindex(ypObject *s, yp_ssize_t i)
{
    if (!ypSequence_AdjustIndexC(ypStr_LEN(s), &i)) {
        return yp_IndexError;
    }

    // It's possible we will need to downconvert s in order to delete s[i]. The logic to do this is
    // already implemented in _ypStringLib_delslice.
    return _ypStringLib_delslice(s, i, i + 1, 1, 1);
}

static ypObject *_ypStr_fromiterable(int type, ypObject *iterable);
static ypObject *chrarray_setslice(
        ypObject *s, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, ypObject *x)
{
    int x_pair = ypObject_TYPE_PAIR_CODE(x);

    if (x_pair == ypStr_CODE) {
        return ypStringLib_setslice_fromstring(s, start, stop, step, x);
    } else if (x_pair == ypBytes_CODE) {
        return yp_TypeError;
    } else {
        ypObject *result;
        ypObject *x_asstr = _ypStr_fromiterable(ypStr_CODE, x);
        if (yp_isexceptionC(x_asstr)) return x_asstr;
        result = ypStringLib_setslice_fromstring(s, start, stop, step, x_asstr);
        yp_decref(x_asstr);
        return result;
    }
}

static ypObject *chrarray_push(ypObject *s, ypObject *x)
{
    ypObject                  *result;
    yp_uint32_t                x_asitem;
    const ypStringLib_encinfo *x_enc;

    result = _ypStr_asitemC(x, &x_asitem, &x_enc);
    if (yp_isexceptionC(result)) return result;

    // TODO Overallocate?
    result = ypStringLib_push(s, x_asitem, x_enc, 0);
    ypStr_ENC(s)->setindexX(ypStr_DATA(s), ypStr_LEN(s), 0);

    ypStr_ASSERT_INVARIANTS(s);
    return result;
}

static ypObject *chrarray_pop(ypObject *s)
{
    const ypStringLib_encinfo *s_enc = ypStr_ENC(s);
    ypObject                  *result;

    if (ypStr_LEN(s) < 1) return yp_IndexError;
    result = yp_chrC(s_enc->getindexX(ypStr_DATA(s), ypStr_LEN(s) - 1));
    ypStr_SET_LEN(s, ypStr_LEN(s) - 1);
    s_enc->setindexX(ypStr_DATA(s), ypStr_LEN(s), 0);
    ypStr_ASSERT_INVARIANTS(s);
    return result;
}

// onmissing must be an immortal, or NULL.
static ypObject *chrarray_remove(ypObject *s, ypObject *x, ypObject *onmissing)
{
    void      *x_data;
    yp_ssize_t x_len;
    ypObject  *result;
    yp_ssize_t i;

    result = _ypStr_coerce_encoding(x, ypStr_ENC(s), &x_data, &x_len);
    if (yp_isexceptionC(result)) return result;

    // If we could not coerce to the target encoding, then it must not be in s.
    if (x_data == NULL) goto missing;

    i = _ypStringLib_find(s, x_data, x_len, 0, ypStr_LEN(s), yp_FIND_FORWARD);
    _ypStr_coerce_encoding_free(x, x_data);
    if (i < 0) goto missing;

    // We found a match to remove.
    ypStr_ELEMMOVE(s, i, i + x_len);
    ypStr_SET_LEN(s, ypStr_LEN(s) - x_len);
    ypStr_ASSERT_INVARIANTS(s);
    return yp_None;

missing:
    if (onmissing == NULL) return yp_ValueError;
    return onmissing;
}

static ypObject *chrarray_extend(ypObject *s, ypObject *iterable)
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);

    if (iterable_pair == ypStr_CODE) {
        return ypStringLib_extend_fromstring(s, iterable);
    } else if (iterable_pair == ypBytes_CODE) {
        return yp_TypeError;
    } else {
        return ypStringLib_extend_fromiterable(s, iterable);
    }
}

static ypObject *chrarray_insert(ypObject *s, yp_ssize_t i, ypObject *x)
{
    ypObject                  *result;
    yp_uint32_t                x_asitem;
    const ypStringLib_encinfo *x_enc;

    // Recall that insert behaves like s[i:i]=[x], but i can't be yp_SLICE_DEFAULT.
    if (i == yp_SLICE_DEFAULT) return yp_TypeError;
    result = _ypStr_asitemC(x, &x_asitem, &x_enc);
    if (yp_isexceptionC(result)) return result;

    // It's possible we will need to either upconvert or downconvert s in order to insert x. The
    // logic to do this is already implemented in ypStringLib_setslice_fromstring.
    return ypStringLib_setslice_fromstring7(s, i, i, 1, &x_asitem, 1, x_enc);
}

static ypObject *chrarray_popindex(ypObject *s, yp_ssize_t i)
{
    const ypStringLib_encinfo *s_enc = ypStr_ENC(s);
    ypObject                  *result;

    if (!ypSequence_AdjustIndexC(ypStr_LEN(s), &i)) {
        return yp_IndexError;
    }

    result = yp_chrC(s_enc->getindexX(ypStr_DATA(s), i));
    ypStr_ELEMMOVE(s, i, i + 1);
    ypStr_SET_LEN(s, ypStr_LEN(s) - 1);
    ypStr_ASSERT_INVARIANTS(s);
    return result;
}

// XXX Adapted from Python's reverse_slice.
static ypObject *chrarray_reverse(ypObject *s)
{
    void                     *s_data = ypStr_DATA(s);
    ypStringLib_getindexXfunc getindexX = ypStr_ENC(s)->getindexX;
    ypStringLib_setindexXfunc setindexX = ypStr_ENC(s)->setindexX;
    yp_ssize_t                lo = 0;
    yp_ssize_t                hi = ypStr_LEN(s) - 1;
    while (lo < hi) {
        yp_uint32_t t = getindexX(s_data, lo);
        setindexX(s_data, lo, getindexX(s_data, hi));
        setindexX(s_data, hi, t);
        lo += 1;
        hi -= 1;
    }
    ypStr_ASSERT_INVARIANTS(s);
    return yp_None;
}

static ypObject *str_find(ypObject *s, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        findfunc_direction direction, yp_ssize_t *i)
{
    void      *x_data;
    yp_ssize_t x_len;
    yp_ssize_t step = 1;
    yp_ssize_t slicelength;
    ypObject  *result;

    result = _ypStr_coerce_encoding(x, ypStr_ENC(s), &x_data, &x_len);
    if (yp_isexceptionC(result)) return result;

    // XXX Unlike Python, the arguments start and end are always treated as in slice notation.
    // Python behaves peculiarly when end<start in certain edge cases involving empty strings. See
    // https://bugs.python.org/issue24243.
    result = ypSlice_AdjustIndicesC(ypStringLib_LEN(s), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    if (x_data == NULL) {
        *i = -1;  // We could not coerce to the target encoding, so it must not be in s.
    } else {
        *i = _ypStringLib_find(s, x_data, x_len, start, slicelength, direction);
    }
    _ypStr_coerce_encoding_free(x, x_data);
    return yp_None;
}

static ypObject *str_count(
        ypObject *s, ypObject *x, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t *n)
{
    void      *x_data;
    yp_ssize_t x_len;
    yp_ssize_t step = 1;
    yp_ssize_t slicelength;
    ypObject  *result;

    result = _ypStr_coerce_encoding(x, ypStr_ENC(s), &x_data, &x_len);
    if (yp_isexceptionC(result)) return result;

    // XXX Unlike Python, the arguments start and end are always treated as in slice notation.
    // Python behaves peculiarly when end<start in certain edge cases involving empty strings. See
    // https://bugs.python.org/issue24243.
    result = ypSlice_AdjustIndicesC(ypStringLib_LEN(s), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    if (x_data == NULL) {
        *n = 0;  // We could not coerce to the target encoding, so it must not be in s.
    } else {
        *n = _ypStringLib_count(s, x_data, x_len, start, slicelength);
    }
    _ypStr_coerce_encoding_free(x, x_data);
    return result;
}

static ypObject *str_contains(ypObject *s, ypObject *x)
{
    ypObject  *result;
    yp_ssize_t i = -1;

    result = str_find(s, x, 0, yp_SLICE_LAST, yp_FIND_FORWARD, &i);
    if (yp_isexceptionC(result)) return result;
    return ypBool_FROM_C(i >= 0);
}

static ypObject *str_len(ypObject *s, yp_ssize_t *len)
{
    *len = ypStr_LEN(s);
    return yp_None;
}

static ypObject *str_isalnum(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isalpha(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isdecimal(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isdigit(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isidentifier(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_islower(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isnumeric(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isprintable(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isspace(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_isupper(ypObject *s) { return yp_NotImplementedError; }

static ypObject *_str_tailmatch(
        ypObject *s, ypObject *x, yp_ssize_t start, yp_ssize_t end, findfunc_direction direction)
{
    yp_ssize_t step = 1;
    yp_ssize_t slice_len;
    ypObject  *result;
    yp_ssize_t cmp_start;

    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return_yp_BAD_TYPE(x);

    // XXX Unlike Python, the arguments start and end are always treated as in slice notation.
    // Python behaves peculiarly when end<start in certain edge cases involving empty strings. See
    // https://bugs.python.org/issue24243.
    result = ypSlice_AdjustIndicesC(ypStr_LEN(s), &start, &end, &step, &slice_len);
    if (yp_isexceptionC(result)) return result;

    // If the prefix is longer than the slice, the slice can't possibly start with it
    if (ypStr_LEN(x) > slice_len) return yp_False;

    if (direction == yp_FIND_REVERSE) {
        cmp_start = end - ypStr_LEN(x);  // endswith
    } else {
        cmp_start = start;  // startswith
    }

    if (ypStr_ENC_CODE(s) == ypStr_ENC_CODE(x)) {
        yp_ssize_t sizeshift = ypStr_ENC(x)->sizeshift;

        int memcmp_result =
                ypStr_MEMCMP(sizeshift, ypStr_DATA(s), cmp_start, ypStr_DATA(x), 0, ypStr_LEN(x));
        return ypBool_FROM_C(memcmp_result == 0);
    } else {
        ypStringLib_getindexXfunc s_getindexX = ypStr_ENC(s)->getindexX;
        ypStringLib_getindexXfunc x_getindexX = ypStr_ENC(x)->getindexX;

        yp_ssize_t i;
        for (i = 0; i < ypStr_LEN(x); i++) {
            if (s_getindexX(s, cmp_start + i) != x_getindexX(x, i)) return yp_False;
        }
        return yp_True;
    }
}

static ypObject *_str_startswith_or_endswith(
        ypObject *s, ypObject *x, yp_ssize_t start, yp_ssize_t end, findfunc_direction direction)
{
    // We're called directly (ypFunction calls str_startswith), so ensure we're called correctly
    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(s) == ypStr_CODE);

    // FIXME Also support lists?  Python requires a tuple here...
    if (ypObject_TYPE_CODE(x) == ypTuple_CODE) {
        yp_ssize_t i;
        for (i = 0; i < ypTuple_LEN(x); i++) {
            ypObject *result = _str_tailmatch(s, ypTuple_ARRAY(x)[i], start, end, direction);
            if (result != yp_False) return result;  // yp_True or an exception
        }
        return yp_False;
    } else {
        return _str_tailmatch(s, x, start, end, direction);
    }
}

static ypObject *str_startswith(ypObject *s, ypObject *prefix, yp_ssize_t start, yp_ssize_t end)
{
    return _str_startswith_or_endswith(s, prefix, start, end, yp_FIND_FORWARD);
}

static ypObject *str_endswith(ypObject *s, ypObject *suffix, yp_ssize_t start, yp_ssize_t end)
{
    return _str_startswith_or_endswith(s, suffix, start, end, yp_FIND_REVERSE);
}

static ypObject *str_lower(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_upper(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_casefold(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_swapcase(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_capitalize(ypObject *s) { return yp_NotImplementedError; }

static ypObject *str_ljust(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar)
{
    return yp_NotImplementedError;
}

static ypObject *str_rjust(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar)
{
    return yp_NotImplementedError;
}

static ypObject *str_center(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar)
{
    return yp_NotImplementedError;
}

static ypObject *str_expandtabs(ypObject *s, yp_ssize_t tabsize) { return yp_NotImplementedError; }

static ypObject *str_replace(ypObject *s, ypObject *oldsub, ypObject *newsub, yp_ssize_t count)
{
    return yp_NotImplementedError;
}

static ypObject *str_lstrip(ypObject *s, ypObject *chars) { return yp_NotImplementedError; }

static ypObject *str_rstrip(ypObject *s, ypObject *chars) { return yp_NotImplementedError; }

static ypObject *str_strip(ypObject *s, ypObject *chars) { return yp_NotImplementedError; }

static void str_partition(
        ypObject *s, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2)
{
    *part0 = *part1 = *part2 = yp_NotImplementedError;
}

static void str_rpartition(
        ypObject *s, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2)
{
    *part0 = *part1 = *part2 = yp_NotImplementedError;
}

static ypObject *str_split(ypObject *s, ypObject *sep, yp_ssize_t maxsplit)
{
    return yp_NotImplementedError;
}

static ypObject *str_rsplit(ypObject *s, ypObject *sep, yp_ssize_t maxsplit)
{
    return yp_NotImplementedError;
}

static ypObject *str_splitlines(ypObject *s, ypObject *keepends) { return yp_NotImplementedError; }


// Returns -1, 0, or 1 as per memcmp
static int _ypStr_relative_cmp(ypObject *s, ypObject *x)
{
    yp_ssize_t s_len = ypStr_LEN(s);
    yp_ssize_t x_len = ypStr_LEN(x);
    int        cmp = yp_memcmp(ypStr_DATA(s), ypStr_DATA(x), MIN(s_len, x_len));
    if (cmp == 0) cmp = s_len < x_len ? -1 : (s_len > x_len);
    return cmp;
}
static ypObject *str_lt(ypObject *s, ypObject *x)
{
    if (s == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return yp_ComparisonNotImplemented;
    // TODO relative comps for ucs-2 and -4
    if (ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    if (ypStr_ENC_CODE(x) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    return ypBool_FROM_C(_ypStr_relative_cmp(s, x) < 0);
}
static ypObject *str_le(ypObject *s, ypObject *x)
{
    if (s == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return yp_ComparisonNotImplemented;
    // TODO relative comps for ucs-2 and -4
    if (ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    if (ypStr_ENC_CODE(x) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    return ypBool_FROM_C(_ypStr_relative_cmp(s, x) <= 0);
}
static ypObject *str_ge(ypObject *s, ypObject *x)
{
    if (s == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return yp_ComparisonNotImplemented;
    // TODO relative comps for ucs-2 and -4
    if (ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    if (ypStr_ENC_CODE(x) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    return ypBool_FROM_C(_ypStr_relative_cmp(s, x) >= 0);
}
static ypObject *str_gt(ypObject *s, ypObject *x)
{
    if (s == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return yp_ComparisonNotImplemented;
    // TODO relative comps for ucs-2 and -4
    if (ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    if (ypStr_ENC_CODE(x) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    return ypBool_FROM_C(_ypStr_relative_cmp(s, x) > 0);
}

// Returns true (1) if the two str/chrarrays are equal. Size is a quick way to check equality.
// TODO Would the pre-computed hash be a quick check for inequality before the memcmp?
static int _ypStr_are_equal(ypObject *s, ypObject *x)
{
    yp_ssize_t s_len = ypStr_LEN(s);
    yp_ssize_t x_len = ypStr_LEN(x);
    if (s_len != x_len) return 0;
    // Recall strs are stored in the smallest encoding that can hold them, so different encodings
    // means differing characters
    if (ypStr_ENC_CODE(s) != ypStr_ENC_CODE(x)) return 0;
    return ypStr_MEMCMP(ypStr_ENC(s)->sizeshift, ypStr_DATA(s), 0, ypStr_DATA(x), 0, s_len) == 0;
}
static ypObject *str_eq(ypObject *s, ypObject *x)
{
    if (s == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(_ypStr_are_equal(s, x));
}
static ypObject *str_ne(ypObject *s, ypObject *x)
{
    if (s == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypStr_CODE) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C(!_ypStr_are_equal(s, x));
}

// Must work even for mutables; yp_hash handles caching this value and denying its use for mutables
// TODO bring this in-line with Python's string hashing; it's currently using bytes hashing
static ypObject *str_currenthash(
        ypObject *s, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    if (ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
    *hash = yp_HashBytes(ypStr_DATA(s), ypStr_LEN(s) << ypStr_ENC(s)->sizeshift);

    // Since we never contain mutable objects, we can cache our hash
    if (!ypObject_IS_MUTABLE(s)) ypObject_CACHED_HASH(s) = *hash;
    return yp_None;
}

static ypObject *str_dealloc(ypObject *s, void *memo)
{
    ypMem_FREE_CONTAINER(s, ypStrObject);
    return yp_None;
}

static ypObject *_ypStr_decode(int type, ypObject *source, ypObject *encoding, ypObject *errors);
static ypObject *_ypStr(int type, ypObject *object);
static ypObject *_ypStr_func_new_code(int type, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 5, "unexpected argarray of length %" PRIssize, n);

    // As object defaults to yp_str_empty, and _ypStr_decode rejects strs, encoding-without-object
    // is an error, just as with bytes (but unlike Python).
    if (argarray[3] != yp_Arg_Missing) {  // TODO ...or just use None?
        ypObject *errors = argarray[4] == yp_Arg_Missing ? yp_s_strict : argarray[4];  // borrowed
        return _ypStr_decode(type, argarray[2], argarray[3], errors);
    } else if (argarray[4] != yp_Arg_Missing) {
        // TODO In Python, sys.getdefaultencoding() is the default. Should we break with Python
        // here? I certainly don't like global variables changing behaviour...
        return _ypStr_decode(type, argarray[2], yp_s_utf_8, argarray[4]);
    } else {
        return _ypStr(type, argarray[2]);
    }
}

static ypObject *str_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT1(argarray[0] == yp_t_str);
    return _ypStr_func_new_code(ypStr_CODE, n, argarray);
}

// XXX Did you know Python has a "mutable string" type? _PyUnicodeWriter.
static ypObject *chrarray_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT1(argarray[0] == yp_t_chrarray);
    return _ypStr_func_new_code(ypChrArray_CODE, n, argarray);
}

#define _ypStr_FUNC_NEW_PARAMETERS                                       \
    ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_slash), NULL},   \
            {yp_CONST_REF(yp_s_object), yp_CONST_REF(yp_str_empty)},     \
            {yp_CONST_REF(yp_s_encoding), yp_CONST_REF(yp_Arg_Missing)}, \
            {yp_CONST_REF(yp_s_errors), yp_CONST_REF(yp_Arg_Missing)})
yp_IMMORTAL_FUNCTION_static(str_func_new, str_func_new_code, _ypStr_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(chrarray_func_new, chrarray_func_new_code, _ypStr_FUNC_NEW_PARAMETERS);

static ypSequenceMethods ypStr_as_sequence = {
        str_concat,                   // tp_concat
        ypStringLib_repeat,           // tp_repeat
        str_getindex,                 // tp_getindex
        str_getslice,                 // tp_getslice
        str_find,                     // tp_find
        str_count,                    // tp_count
        MethodError_objssizeobjproc,  // tp_setindex
        MethodError_objsliceobjproc,  // tp_setslice
        MethodError_objssizeproc,     // tp_delindex
        MethodError_objsliceproc,     // tp_delslice
        MethodError_objobjproc,       // tp_append
        MethodError_objobjproc,       // tp_extend
        MethodError_objssizeproc,     // tp_irepeat
        MethodError_objssizeobjproc,  // tp_insert
        MethodError_objssizeproc,     // tp_popindex
        MethodError_objproc,          // tp_reverse
        MethodError_objobjobjproc     // tp_sort
};

static ypTypeObject ypStr_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(str_func_new),  // tp_func_new
        str_dealloc,                 // tp_dealloc
        NoRefs_traversefunc,         // tp_traverse
        NULL,                        // tp_str
        NULL,                        // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,    // tp_freeze
        str_unfrozen_copy,      // tp_unfrozen_copy
        str_frozen_copy,        // tp_frozen_copy
        str_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        str_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,    // tp_invalidate

        // Boolean operations and comparisons
        str_bool,  // tp_bool
        str_lt,    // tp_lt
        str_le,    // tp_le
        str_eq,    // tp_eq
        str_ne,    // tp_ne
        str_ge,    // tp_ge
        str_gt,    // tp_gt

        // Generic object operations
        str_currenthash,      // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        str_contains,               // tp_contains
        str_len,                    // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypStr_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypSequenceMethods ypChrArray_as_sequence = {
        str_concat,                // tp_concat
        ypStringLib_repeat,        // tp_repeat
        str_getindex,              // tp_getindex
        ypStringLib_getslice,      // tp_getslice
        str_find,                  // tp_find
        str_count,                 // tp_count
        chrarray_setindex,         // tp_setindex
        chrarray_setslice,         // tp_setslice
        chrarray_delindex,         // tp_delindex
        ypStringLib_delslice,      // tp_delslice
        chrarray_push,             // tp_append
        chrarray_extend,           // tp_extend
        ypStringLib_irepeat,       // tp_irepeat
        chrarray_insert,           // tp_insert
        chrarray_popindex,         // tp_popindex
        chrarray_reverse,          // tp_reverse
        MethodError_objobjobjproc  // tp_sort
};

static ypTypeObject ypChrArray_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(chrarray_func_new),  // tp_func_new
        str_dealloc,                      // tp_dealloc
        NoRefs_traversefunc,              // tp_traverse
        NULL,                             // tp_str
        NULL,                             // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,    // tp_freeze
        str_unfrozen_copy,      // tp_unfrozen_copy
        str_frozen_copy,        // tp_frozen_copy
        str_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        str_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,    // tp_invalidate

        // Boolean operations and comparisons
        str_bool,  // tp_bool
        str_lt,    // tp_lt
        str_le,    // tp_le
        str_eq,    // tp_eq
        str_ne,    // tp_ne
        str_ge,    // tp_ge
        str_gt,    // tp_gt

        // Generic object operations
        str_currenthash,      // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        str_contains,               // tp_contains
        str_len,                    // tp_len
        chrarray_push,              // tp_push
        ypStringLib_clear,          // tp_clear
        chrarray_pop,               // tp_pop
        chrarray_remove,            // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        _ypSequence_setitem,        // tp_setitem
        _ypSequence_delitem,        // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypChrArray_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypObject *_yp_asencodedCX(
        ypObject *s, yp_ssize_t *size, const yp_uint8_t **encoded, ypObject **encoding)
{
    if (ypObject_TYPE_PAIR_CODE(s) != ypStr_CODE) return_yp_BAD_TYPE(s);

    ypStr_ASSERT_INVARIANTS(s);
    *encoded = ypStr_DATA(s);
    if (size == NULL) {
        // TODO Support ucs-2 and -4 here
        if (ypStr_ENC_CODE(s) != ypStringLib_ENC_CODE_LATIN_1) return yp_NotImplementedError;
        if ((yp_ssize_t)strlen(*encoded) != ypStr_LEN(s)) return yp_TypeError;
    } else {
        *size = ypStr_LEN(s) << ypStr_ENC(s)->sizeshift;
    }
    *encoding = ypStr_ENC(s)->name;
    return yp_None;
}
ypObject *yp_asencodedCX(
        ypObject *s, yp_ssize_t *size, const yp_uint8_t **encoded, ypObject **encoding)
{
    ypObject *result = _yp_asencodedCX(s, size, encoded, encoding);
    if (yp_isexceptionC(result)) {
        if (size != NULL) *size = 0;
        *encoded = NULL;
        *encoding = result;
    }
    return result;
}

// Public constructors

static ypObject *_ypStr_frombytes(
        int type, yp_ssize_t len, const yp_uint8_t *source, ypObject *encoding, ypObject *errors)
{
    ypObject *result;

    // XXX Not handling errors in yp_eq yet because this is just temporary
    if (yp_eq(encoding, yp_s_utf_8) != yp_True) return yp_NotImplementedError;

    // TODO Python limits this to codecs that identify themselves as text encodings: do the same
    if (!ypBytes_adjust_lenC(&len, source)) return yp_MemorySizeOverflowError;
    result = ypStringLib_decode_frombytesC_utf_8(type, len, source, errors);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT(ypObject_TYPE_CODE(result) == type, "text encoding didn't return correct type");
    ypStr_ASSERT_INVARIANTS(result);
    return result;
}
// TODO Be consistent and always put "len" params before the thing they are sizing. This breaks with
// Python but, conceptually at least, you need the length of something before you use it.
ypObject *yp_str_frombytesC4(
        yp_ssize_t len, const yp_uint8_t *source, ypObject *encoding, ypObject *errors)
{
    return _ypStr_frombytes(ypStr_CODE, len, source, encoding, errors);
}
ypObject *yp_chrarray_frombytesC4(
        yp_ssize_t len, const yp_uint8_t *source, ypObject *encoding, ypObject *errors)
{
    return _ypStr_frombytes(ypChrArray_CODE, len, source, encoding, errors);
}
ypObject *yp_str_frombytesC2(yp_ssize_t len, const yp_uint8_t *source)
{
    if (!ypBytes_adjust_lenC(&len, source)) return yp_MemorySizeOverflowError;
    return ypStringLib_decode_frombytesC_utf_8(ypStr_CODE, len, source, yp_s_strict);
}
ypObject *yp_chrarray_frombytesC2(yp_ssize_t len, const yp_uint8_t *source)
{
    if (!ypBytes_adjust_lenC(&len, source)) return yp_MemorySizeOverflowError;
    return ypStringLib_decode_frombytesC_utf_8(ypChrArray_CODE, len, source, yp_s_strict);
}

static ypObject *_ypStr_decode(int type, ypObject *source, ypObject *encoding, ypObject *errors)
{
    ypObject *result;

    // TODO When we open this up to other types with a buffer interface, make sure we continue
    // to deny str/chrarray as source, as Python does.
    if (ypObject_TYPE_PAIR_CODE(source) != ypBytes_CODE) return_yp_BAD_TYPE(source);

    // XXX Not handling errors in yp_eq yet because this is just temporary
    // TODO Python ignores "unknown encoding/errors" on empty buffer, but I'd rather raise error.
    if (yp_eq(encoding, yp_s_utf_8) != yp_True) return yp_NotImplementedError;

    // TODO Python limits this to codecs that identify themselves as text encodings: do the same
    result = ypStringLib_decode_frombytesC_utf_8(
            type, ypBytes_LEN(source), ypBytes_DATA(source), errors);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT(ypObject_TYPE_CODE(result) == type, "text encoding didn't return correct type");
    ypStr_ASSERT_INVARIANTS(result);
    return result;
}
ypObject *yp_str3(ypObject *source, ypObject *encoding, ypObject *errors)
{
    return _ypStr_decode(ypStr_CODE, source, encoding, errors);
}
ypObject *yp_chrarray3(ypObject *source, ypObject *encoding, ypObject *errors)
{
    return _ypStr_decode(ypChrArray_CODE, source, encoding, errors);
}
ypObject *yp_decode3(ypObject *b, ypObject *encoding, ypObject *errors)
{
    return _ypStr_decode(
            ypObject_IS_MUTABLE(b) ? ypChrArray_CODE : ypStr_CODE, b, encoding, errors);
}
ypObject *yp_decode(ypObject *b)
{
    if (ypObject_TYPE_PAIR_CODE(b) != ypBytes_CODE) return_yp_BAD_TYPE(b);
    return ypStringLib_decode_frombytesC_utf_8(
            ypObject_IS_MUTABLE(b) ? ypChrArray_CODE : ypStr_CODE, ypBytes_LEN(b), ypBytes_DATA(b),
            yp_s_strict);
}

// XXX strs are not typically created from iterables. Whereas bytes() works like tuple() in taking
// an iterable of elements, str() does not work that way (passing an iterable to str() returns the
// string representation of the iterable). This is used in the few contexts where we want a str
// constructor that works like tuple() (chrarray_setslice, in particular).
static ypObject *_ypStr_fromiterable(int type, ypObject *iterable)
{
    ypObject  *exc = yp_None;
    yp_ssize_t length_hint;
    ypObject  *newS;
    ypObject  *result;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(iterable) != ypStr_CODE, "call ypStringLib_copy instead");
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(iterable) != ypBytes_CODE, "raise yp_TypeError earlier");

    // TODO Here and everywhere, rethink this bit of code: how much do we trust yp_lenC?
    length_hint = yp_lenC(iterable, &exc);
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint: it just means we can't pre-allocate
        length_hint = yp_length_hintC(iterable, &exc);
        if (length_hint > ypStr_LEN_MAX) length_hint = ypStr_LEN_MAX;
    } else if (length_hint < 1) {
        // yp_lenC reports an empty iterable, so we can shortcut ypStringLib_extend_fromiterable
        if (type == ypStr_CODE) return yp_str_empty;
        return yp_chrarray0();
    } else if (length_hint > ypStr_LEN_MAX) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    newS = _ypStr_new_latin_1(type, length_hint, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newS)) return newS;
    result = ypStringLib_extend_fromiterable(newS, iterable);
    if (yp_isexceptionC(result)) {
        yp_decref(newS);
        return result;
    }

    // TODO We could avoid allocating for an empty iterable altogether if we get the first value
    // before allocating; is this complication worth the optimization?
    if (type == ypStr_CODE && ypStr_LEN(newS) < 1) {
        yp_decref(newS);
        return yp_str_empty;
    }

    ypStr_ASSERT_INVARIANTS(newS);
    return newS;
}

static ypObject *_ypStr(int type, ypObject *object)
{
    if (ypObject_TYPE_PAIR_CODE(object) == ypStr_CODE) {
        return ypStringLib_copy(type, object);
    }

    return yp_NotImplementedError;
}

ypObject *yp_str(ypObject *object) { return _ypStr(ypStr_CODE, object); }

// XXX Did you know Python has a "mutable string" type? _PyUnicodeWriter.
ypObject *yp_chrarray(ypObject *object) { return _ypStr(ypChrArray_CODE, object); }

// There is no yp_str0 because we export yp_str_empty directly.

ypObject *yp_chrarray0(void)
{
    ypObject *newS = _ypStr_new_latin_1(ypChrArray_CODE, 0, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newS)) return newS;
    ((yp_uint8_t *)ypStr_DATA(newS))[0] = 0;
    ypStr_ASSERT_INVARIANTS(newS);
    return newS;
}

// TODO Like ints, the latin-1 chars are singletons in Python. Do the same.
static ypObject *_yp_chrC(int type, yp_int_t i)
{
    const ypStringLib_encinfo *newS_enc;
    ypObject                  *newS;

    if (i < 0 || i > ypStringLib_MAX_UNICODE) return yp_ValueError;

    // clang-format off
    newS_enc = i > 0xFFFFu ? ypStringLib_enc_ucs_4 :
               i > 0xFFu   ? ypStringLib_enc_ucs_2 :
               ypStringLib_enc_latin_1;
    // clang-format on

    newS = _ypStr_new(type, 1, /*alloclen_fixed=*/TRUE, newS_enc);
    if (yp_isexceptionC(newS)) return newS;
    yp_ASSERT(ypStr_DATA(newS) == ypStr_INLINE_DATA(newS), "yp_chrC didn't allocate inline!");

    // Recall we've already checked that i isn't outside of a 32-bit range (MAX_UNICODE)
    newS_enc->setindexX(ypStr_DATA(newS), 0, (yp_uint32_t)i);
    newS_enc->setindexX(ypStr_DATA(newS), 1, 0);
    ypStr_SET_LEN(newS, 1);
    ypStr_ASSERT_INVARIANTS(newS);
    return newS;
}
ypObject *yp_chrC(yp_int_t i) { return _yp_chrC(ypStr_CODE, i); }

#pragma endregion str


/*************************************************************************************************
 * String (str, bytes, etc) methods
 *************************************************************************************************/
#pragma region string_methods

// XXX Since it's not likely that anything other than str and bytes will need to implement these
// methods, they are left out of the type's method table. This may change in the future.
// TODO Since "this may change in the future", perhaps we should raise yp_MethodError instead.

static const ypStringLib_encinfo ypStringLib_encs[4] = {
        // Indices are encoding codes; elements are constants and methods to work with encoding.
        {
                ypStringLib_ENC_CODE_BYTES,   // code
                0,                            // sizeshift
                1,                            // elemsize
                0xFFu,                        // max_char
                NULL,                         // name
                ypStringLib_getindexX_1byte,  // getindexX
                ypStringLib_setindexX_1byte   // setindexX
        },
        {
                ypStringLib_ENC_CODE_LATIN_1,  // code
                0,                             // sizeshift
                1,                             // elemsize
                0xFFu,                         // max_char
                yp_CONST_REF(yp_s_latin_1),    // name
                ypStringLib_getindexX_1byte,   // getindexX
                ypStringLib_setindexX_1byte    // setindexX
        },
        {
                ypStringLib_ENC_CODE_UCS_2,    // code
                1,                             // sizeshift
                2,                             // elemsize
                0xFFFFu,                       // max_char
                yp_CONST_REF(yp_s_ucs_2),      // name
                ypStringLib_getindexX_2bytes,  // getindexX
                ypStringLib_setindexX_2bytes   // setindexX
        },
        {
                ypStringLib_ENC_CODE_UCS_4,    // code
                2,                             // sizeshift
                4,                             // elemsize
                0xFFFFFFFFu,                   // max_char
                yp_CONST_REF(yp_s_ucs_4),      // name
                ypStringLib_getindexX_4bytes,  // getindexX
                ypStringLib_setindexX_4bytes   // setindexX
        }};

// Assume these are most-likely to be run against str/chrarrays, so put that check first
// TODO Rethink where we split off to a type-specific function, and where we call a generic
// ypStringLib
#define _ypStringLib_REDIRECT1(ob, meth, args)     \
    do {                                           \
        int ob_pair = ypObject_TYPE_PAIR_CODE(ob); \
        if (ob_pair == ypStr_CODE) {               \
            return str_##meth args;                \
        }                                          \
        if (ob_pair == ypBytes_CODE) {             \
            return bytes_##meth args;              \
        }                                          \
        return_yp_BAD_TYPE(ob);                    \
    } while (0)


ypObject *yp_isalnum(ypObject *s) { _ypStringLib_REDIRECT1(s, isalnum, (s)); }

ypObject *yp_isalpha(ypObject *s) { _ypStringLib_REDIRECT1(s, isalpha, (s)); }

ypObject *yp_isdecimal(ypObject *s) { _ypStringLib_REDIRECT1(s, isdecimal, (s)); }

ypObject *yp_isdigit(ypObject *s) { _ypStringLib_REDIRECT1(s, isdigit, (s)); }

ypObject *yp_isidentifier(ypObject *s) { _ypStringLib_REDIRECT1(s, isidentifier, (s)); }

ypObject *yp_islower(ypObject *s) { _ypStringLib_REDIRECT1(s, islower, (s)); }

ypObject *yp_isnumeric(ypObject *s) { _ypStringLib_REDIRECT1(s, isnumeric, (s)); }

ypObject *yp_isprintable(ypObject *s) { _ypStringLib_REDIRECT1(s, isprintable, (s)); }

ypObject *yp_isspace(ypObject *s) { _ypStringLib_REDIRECT1(s, isspace, (s)); }

ypObject *yp_isupper(ypObject *s) { _ypStringLib_REDIRECT1(s, isupper, (s)); }

ypObject *yp_startswithC4(ypObject *s, ypObject *prefix, yp_ssize_t start, yp_ssize_t end)
{
    _ypStringLib_REDIRECT1(s, startswith, (s, prefix, start, end));
}

ypObject *yp_startswith(ypObject *s, ypObject *prefix)
{
    return yp_startswithC4(s, prefix, 0, yp_SLICE_LAST);
}

ypObject *yp_endswithC4(ypObject *s, ypObject *suffix, yp_ssize_t start, yp_ssize_t end)
{
    _ypStringLib_REDIRECT1(s, endswith, (s, suffix, start, end));
}

ypObject *yp_endswith(ypObject *s, ypObject *suffix)
{
    return yp_endswithC4(s, suffix, 0, yp_SLICE_LAST);
}

ypObject *yp_lower(ypObject *s) { _ypStringLib_REDIRECT1(s, lower, (s)); }

ypObject *yp_upper(ypObject *s) { _ypStringLib_REDIRECT1(s, upper, (s)); }

ypObject *yp_casefold(ypObject *s) { _ypStringLib_REDIRECT1(s, casefold, (s)); }

ypObject *yp_swapcase(ypObject *s) { _ypStringLib_REDIRECT1(s, swapcase, (s)); }

ypObject *yp_capitalize(ypObject *s) { _ypStringLib_REDIRECT1(s, capitalize, (s)); }

ypObject *yp_ljustC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar)
{
    _ypStringLib_REDIRECT1(s, ljust, (s, width, ord_fillchar));
}

ypObject *yp_ljustC(ypObject *s, yp_ssize_t width) { return yp_ljustC3(s, width, ' '); }

ypObject *yp_rjustC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar)
{
    _ypStringLib_REDIRECT1(s, rjust, (s, width, ord_fillchar));
}

ypObject *yp_rjustC(ypObject *s, yp_ssize_t width) { return yp_rjustC3(s, width, ' '); }

ypObject *yp_centerC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar)
{
    _ypStringLib_REDIRECT1(s, center, (s, width, ord_fillchar));
}

ypObject *yp_centerC(ypObject *s, yp_ssize_t width) { return yp_centerC3(s, width, ' '); }

ypObject *yp_expandtabsC(ypObject *s, yp_ssize_t tabsize)
{
    _ypStringLib_REDIRECT1(s, expandtabs, (s, tabsize));
}

ypObject *yp_replaceC4(ypObject *s, ypObject *oldsub, ypObject *newsub, yp_ssize_t count)
{
    _ypStringLib_REDIRECT1(s, replace, (s, oldsub, newsub, count));
}

ypObject *yp_replace(ypObject *s, ypObject *oldsub, ypObject *newsub)
{
    return yp_replaceC4(s, oldsub, newsub, -1);
}

ypObject *yp_lstrip2(ypObject *s, ypObject *chars)
{
    _ypStringLib_REDIRECT1(s, lstrip, (s, chars));
}

ypObject *yp_lstrip(ypObject *s) { return yp_lstrip2(s, yp_None); }

ypObject *yp_rstrip2(ypObject *s, ypObject *chars)
{
    _ypStringLib_REDIRECT1(s, rstrip, (s, chars));
}

ypObject *yp_rstrip(ypObject *s) { return yp_rstrip2(s, yp_None); }

ypObject *yp_strip2(ypObject *s, ypObject *chars) { _ypStringLib_REDIRECT1(s, strip, (s, chars)); }

ypObject *yp_strip(ypObject *s) { return yp_strip2(s, yp_None); }

ypObject *yp_join(ypObject *s, ypObject *iterable)
{
    const ypQuickSeq_methods *methods;
    ypQuickSeq_state          state;
    ypObject                 *result;

    if (!ypStringLib_TYPE_CHECK(s)) return_yp_BAD_TYPE(s);
    if (ypStringLib_TYPE_CHECK(iterable)) {
        if (ypObject_TYPE_PAIR_CODE(s) != ypObject_TYPE_PAIR_CODE(iterable)) return yp_TypeError;
        return _ypStringLib_join_fromstring(s, iterable);
    }

    if (ypQuickSeq_new_fromiterable_builtins(&methods, &state, iterable)) {
        result = ypStringLib_join(s, methods, &state);
        methods->close(&state);
    } else {
        // TODO It would be better to handle this without creating a temporary tuple at all,
        // so create a _ypStringLib_join_fromiter instead
        ypObject *temptuple = yp_tuple(iterable);
        if (yp_isexceptionC(temptuple)) return temptuple;
        ypQuickSeq_new_fromtuple(&state, temptuple);
        result = ypStringLib_join(s, &ypQuickSeq_tuple_methods, &state);
        ypQuickSeq_tuple_close(&state);
        yp_decref(temptuple);
    }
    if (yp_isexceptionC(result)) return result;
    ypStringLib_ASSERT_INVARIANTS(result);
    return result;
}

ypObject *yp_joinN(ypObject *s, int n, ...)
{
    return_yp_V_FUNC(ypObject *, yp_joinNV, (s, n, args), n);
}
ypObject *yp_joinNV(ypObject *s, int n, va_list args)
{
    ypQuickSeq_state state;
    ypObject        *result;

    if (!ypStringLib_TYPE_CHECK(s)) return_yp_BAD_TYPE(s);
    ypQuickSeq_new_fromvar(&state, n, args);
    result = ypStringLib_join(s, &ypQuickSeq_var_methods, &state);
    ypQuickSeq_var_close(&state);
    if (yp_isexceptionC(result)) return result;
    ypStringLib_ASSERT_INVARIANTS(result);
    return result;
}

void yp_partition(ypObject *s, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2);

void yp_rpartition(
        ypObject *s, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2);

ypObject *yp_splitC3(ypObject *s, ypObject *sep, yp_ssize_t maxsplit)
{
    _ypStringLib_REDIRECT1(s, split, (s, sep, maxsplit));
}

ypObject *yp_split2(ypObject *s, ypObject *sep) { return yp_splitC3(s, sep, -1); }

ypObject *yp_split(ypObject *s) { return yp_splitC3(s, yp_None, -1); }

// TODO use a direction parameter internally like in find/rfind?
ypObject *yp_rsplitC3(ypObject *s, ypObject *sep, yp_ssize_t maxsplit)
{
    _ypStringLib_REDIRECT1(s, rsplit, (s, sep, maxsplit));
}

ypObject *yp_splitlines2(ypObject *s, ypObject *keepends)
{
    _ypStringLib_REDIRECT1(s, splitlines, (s, keepends));
}

#pragma endregion string_methods


/*************************************************************************************************
 * Sequence of generic items
 *************************************************************************************************/
#pragma region list

// ypTuple_ARRAY is defined above
#define ypTuple_SET_ARRAY(sq, array) (((ypObject *)sq)->ob_data = array)
// ypTuple_LEN is defined above
#define ypTuple_SET_LEN ypObject_SET_CACHED_LEN
#define ypTuple_ALLOCLEN ypObject_ALLOCLEN
#define ypTuple_SET_ALLOCLEN ypObject_SET_ALLOCLEN
#define ypTuple_INLINE_DATA(sq) (((ypTupleObject *)sq)->ob_inline_data)

// The maximum possible alloclen and length of a tuple
#define ypTuple_ALLOCLEN_MAX                                                              \
    ((yp_ssize_t)MIN((yp_SSIZE_T_MAX - yp_sizeof(ypTupleObject)) / yp_sizeof(ypObject *), \
            ypObject_LEN_MAX))
#define ypTuple_LEN_MAX ypTuple_ALLOCLEN_MAX

// Moves the elements from [src:] to the index dest; this can be used when deleting items (they
// must be discarded first), or inserting (the new space is uninitialized). Assumes enough space
// is allocated for the move. Recall that memmove handles overlap.
#define ypTuple_ELEMMOVE(sq, dest, src)                               \
    yp_memmove(ypTuple_ARRAY(sq) + (dest), ypTuple_ARRAY(sq) + (src), \
            (ypTuple_LEN(sq) - (src)) * yp_sizeof(ypObject *));

#define ypTuple_ASSERT_ALLOCLEN(alloclen)                                       \
    do {                                                                        \
        yp_ASSERT(alloclen >= 0, "alloclen cannot be negative");                \
        yp_ASSERT(alloclen <= ypTuple_ALLOCLEN_MAX, "alloclen cannot be >max"); \
    } while (0)

// Return a new tuple/list object with the given alloclen. If type is immutable and
// alloclen_fixed is true (indicating the object will never grow), the data is placed inline
// with one allocation.
// XXX Check for the yp_tuple_empty and ypTuple_ALLOCLEN_MAX cases first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Overallocate to avoid future resizings
static ypObject *_ypTuple_new(int type, yp_ssize_t alloclen, int alloclen_fixed)
{
    ypTuple_ASSERT_ALLOCLEN(alloclen);
    if (alloclen_fixed && type == ypTuple_CODE) {
        yp_ASSERT(alloclen > 0, "missed a yp_tuple_empty optimization");
        return ypMem_MALLOC_CONTAINER_INLINE(
                ypTupleObject, ypTuple_CODE, alloclen, ypTuple_ALLOCLEN_MAX);
    } else {
        return ypMem_MALLOC_CONTAINER_VARIABLE(
                ypTupleObject, type, alloclen, 0, ypTuple_ALLOCLEN_MAX);
    }
}

typedef struct {
    ypObject **array;
    yp_ssize_t len;
    yp_ssize_t alloclen;
} ypTuple_detached;

// Creates a new array of objects, placing it and other metadata in detached so that the array can
// be attached to a tuple/list with _ypTuple_attach_array. This is used by deepcopy and other
// constructors that may expose the object to arbitrary code while it is being created. Returns an
// exception on error, in which case detached is invalid.
// XXX Handle the "empty list" case before calling this function.
// XXX The tuple/list that will attach must be created with ypMem_MALLOC_CONTAINER_VARIABLE.
static ypObject *_ypTuple_new_detached_array(yp_ssize_t alloclen, ypTuple_detached *detached)
{
    yp_ssize_t allocsize;
    ypTuple_ASSERT_ALLOCLEN(alloclen);
    yp_ASSERT(alloclen > 0, "missed an empty tuple/list optimization");

    detached->array = yp_malloc(&allocsize, alloclen * yp_sizeof(ypObject *));
    if (detached->array == NULL) return yp_MemoryError;

    detached->len = 0;
    detached->alloclen = allocsize / yp_sizeof(ypObject *);
    if (detached->alloclen > ypTuple_ALLOCLEN_MAX) detached->alloclen = ypTuple_ALLOCLEN_MAX;

    return yp_None;
}

// Clears sq, placing the array of objects and other metadata in detached so that the array can be
// reattached with _ypTuple_attach_array. This is used by sort and other methods that may execute
// arbitrary code (i.e. yp_lt) while modifying the list, to prevent that arbitrary code from also
// modifying the list. Returns an exception on error, in which case sq is not modified and detached
// is invalid.
// XXX Handle the "empty list" case before calling this function.
// TODO Would a flag on the object to prevent mutations work better? We need it for iterating...
static ypObject *_ypTuple_detach_array(ypObject *sq, ypTuple_detached *detached)
{
    ypObject *result;

    yp_ASSERT1(ypObject_TYPE_CODE(sq) == ypList_CODE);
    yp_ASSERT1(ypObject_CACHED_HASH(sq) == ypObject_HASH_INVALID);
    yp_ASSERT1(ypTuple_LEN(sq) > 0);

    if (ypTuple_ARRAY(sq) == ypTuple_INLINE_DATA(sq)) {
        // If the data is inline, we need to allocate a new buffer
        yp_ASSERT1(ypTuple_ALLOCLEN(sq) == ypMem_INLINELEN_CONTAINER_VARIABLE(sq, ypTupleObject));
        result = _ypTuple_new_detached_array(ypTuple_LEN(sq), detached);
        if (yp_isexceptionC(result)) return result;
        yp_memcpy(detached->array, ypTuple_ARRAY(sq), ypTuple_LEN(sq) * yp_sizeof(ypObject *));
        detached->len = ypTuple_LEN(sq);
    } else {
        // We can just detach the buffer from the object
        detached->array = ypTuple_ARRAY(sq);
        detached->len = ypTuple_LEN(sq);
        detached->alloclen = ypTuple_ALLOCLEN(sq);
    }

    // Now that we've detached the array from sq we can clear it
    ypTuple_SET_ARRAY(sq, ypTuple_INLINE_DATA(sq));
    ypTuple_SET_LEN(sq, 0);
    ypTuple_SET_ALLOCLEN(sq, ypMem_INLINELEN_CONTAINER_VARIABLE(sq, ypTupleObject));

    return yp_None;
}

// Frees detached, reclaiming any memory returned by _ypTuple_detach_array.
static void _ypTuple_free_detached(ypTuple_detached *detached)
{
    for (/*detached->len already set*/; detached->len > 0; detached->len--) {
        yp_decref(detached->array[detached->len - 1]);
    }
    yp_free(detached->array);
}

// Reverses the effect of _ypTuple_detach_array, reattaching the array to sq and freeing detached.
// If it is detected that sq was modified since being detached, sq will be cleared before
// re-attachment, and yp_ValueError will be raised. (If clearing sq fails, which is very unlikely,
// the contents of sq is undefined and a different exception may be raised.) Also works to attach
// arrays created by _ypTuple_new_detached_array.
// XXX sq must have been created with ypMem_MALLOC_CONTAINER_VARIABLE (alloclen_fixed=FALSE);
static ypObject *_ypTuple_attach_array(ypObject *sq, ypTuple_detached *detached)
{
    int              wasModified;
    ypTuple_detached modified;
    ypObject        *result;

    // We don't memcpy in this function. If a large tuple was created with alloclen_fixed=TRUE, we
    // are about to waste all that extra data by discarding ob_alloclen. As such, we assert that
    // sq's inline alloclen is the standard alloclen.
    yp_ASSERT1(ypTuple_ARRAY(sq) != ypTuple_INLINE_DATA(sq) ||
               ypTuple_ALLOCLEN(sq) == ypMem_INLINELEN_CONTAINER_VARIABLE(sq, ypTupleObject));

    // If sq was used in a context that expected a stable hash, we cannot modify it, so discard
    // the detached array.
    // XXX This is a good argument for keeping ob_hash, at least on complex objects.
    if (ypObject_CACHED_HASH(sq) != ypObject_HASH_INVALID) {
        _ypTuple_free_detached(detached);
        return yp_Exception;  // TODO Something more specific.
    }

    // This won't detect if the list was modified and then cleared.
    wasModified = ypTuple_LEN(sq) > 0;
    if (wasModified) {
        result = _ypTuple_detach_array(sq, &modified);
        if (yp_isexceptionC(result)) {
            // This is very unlikely to happen, but if it does, it breaks sort's guarantee that the
            // list will be some permutation of its input state, even on error.
            _ypTuple_free_detached(detached);
            return result;
        }
    } else if (ypTuple_ARRAY(sq) != ypTuple_INLINE_DATA(sq)) {
        // This indicates that the list was modified and then cleared with delindex(). We could make
        // this an error, but that still wouldn't catch modified-then-clear(). So keep quiet.
        yp_free(ypTuple_ARRAY(sq));
    }

    // Attach the array to this object
    ypTuple_SET_ARRAY(sq, detached->array);
    ypTuple_SET_LEN(sq, detached->len);
    ypTuple_SET_ALLOCLEN(sq, detached->alloclen);

    if (wasModified) {
        // Python's list_sort raises ValueError in this case, so we will too
        _ypTuple_free_detached(&modified);
        return yp_ValueError;  // TODO Something more specific?  yp_ConcurrentModification?
    }
    return yp_None;
}

// XXX Check for the "yp_tuple_empty" and ypTuple_ALLOCLEN_MAX cases first.
// XXX array must not contain exceptions.
static ypObject *_ypTuple_new_fromarray(int type, yp_ssize_t n, ypObject *const *array)
{
    yp_ssize_t i;
    ypObject  *sq = _ypTuple_new(type, n, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(sq)) return sq;
    yp_memcpy(ypTuple_ARRAY(sq), array, n * yp_sizeof(ypObject *));
    for (i = 0; i < n; i++) yp_incref(ypTuple_ARRAY(sq)[i]);
    ypTuple_SET_LEN(sq, n);
    return sq;
}

// XXX Check for the "lazy shallow copy" and "yp_tuple_empty" cases first.
static ypObject *_ypTuple_copy(int type, ypObject *x)
{
    yp_ASSERT(type != ypTuple_CODE || ypObject_TYPE_CODE(x) != ypTuple_CODE,
            "missed a lazy shallow copy optimization");
    return _ypTuple_new_fromarray(type, ypTuple_LEN(x), ypTuple_ARRAY(x));
}

// XXX Check for the yp_tuple_empty case first
static ypObject *_ypTuple_deepcopy(int type, ypObject *x, visitfunc copy_visitor, void *copy_memo)
{
    ypObject        *sq;
    ypObject        *result;
    ypTuple_detached detached;
    yp_ssize_t       i;
    ypObject        *item;

    yp_ASSERT(type != ypTuple_CODE || ypTuple_LEN(x) > 0, "missed a yp_tuple_empty optimization");

    // Create with an alloclen of zero; _ypTuple_new_detached_array creates the right size buffer.
    sq = _ypTuple_new(type, 0, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(sq)) return sq;

    // To avoid recursion we need to memoize before populating. This may expose an unfinished object
    // to arbitrary code (i.e. a tuple whose hash will change): _ypTuple_attach_array protects
    // against this.
    result = _yp_deepcopy_memo_setitem(copy_memo, x, sq);
    if (yp_isexceptionC(result)) {
        yp_decref(sq);
        return result;
    }

    // Optimization: if x is empty, we don't need to allocate a detached array.
    if (ypTuple_LEN(x) < 1) return sq;

    // Because we are executing arbitrary code via copy_visitor, we create the array outside of sq,
    // then attach it at the end.
    result = _ypTuple_new_detached_array(ypTuple_LEN(x), &detached);
    if (yp_isexceptionC(result)) {
        yp_decref(sq);
        return result;
    }

    // Update detached.len on each item so _ypTuple_free_detached can clean up mid-copy failures.
    for (i = 0; i < ypTuple_LEN(x); i++) {
        item = copy_visitor(ypTuple_ARRAY(x)[i], copy_memo);
        if (yp_isexceptionC(item)) {
            _ypTuple_free_detached(&detached);
            yp_decref(sq);
            return item;
        }

        detached.array[i] = item;
        detached.len += 1;
    }

    result = _ypTuple_attach_array(sq, &detached);
    if (yp_isexceptionC(result)) {
        yp_decref(sq);
        return result;
    }

    return sq;
}

// Used by tp_repeat et al to perform the necessary memcpy's. sq's array must be allocated
// to hold factor*n objects, the objects to repeat must be in the first n elements of the array,
// and the rest of the array must not contain any references (they will be overwritten). Further,
// factor and n must both be greater than zero. Cannot fail.
// XXX Handle the "empty" case (factor<1 or n<1) before calling this function
#define _ypTuple_repeat_memcpy(sq, factor, n) \
    _ypSequence_repeat_memcpy(ypTuple_ARRAY(sq), (factor), (n)*yp_sizeof(ypObject *))

// Called on push/append, extend, or irepeat to increase the allocated size of the tuple. Does not
// update ypTuple_LEN.
// XXX Check for the "same size" case first
static ypObject *_ypTuple_extend_grow(ypObject *sq, yp_ssize_t required, yp_ssize_t extra)
{
    void *oldptr;
    yp_ASSERT(required > ypTuple_LEN(sq), "required cannot be <=len(sq)");
    yp_ASSERT(required <= ypTuple_LEN_MAX, "required cannot be >max");
    oldptr = ypMem_REALLOC_CONTAINER_VARIABLE(
            sq, ypTupleObject, required, extra, ypTuple_ALLOCLEN_MAX);
    if (oldptr == NULL) return yp_MemoryError;
    if (ypTuple_ARRAY(sq) != oldptr) {
        yp_memcpy(ypTuple_ARRAY(sq), oldptr, ypTuple_LEN(sq) * yp_sizeof(ypObject *));
        ypMem_REALLOC_CONTAINER_FREE_OLDPTR(sq, ypTupleObject, oldptr);
    }
    return yp_None;
}

// growhint is the number of additional items, not including x, that are expected to be added to
// the tuple
static ypObject *_ypTuple_push(ypObject *sq, ypObject *x, yp_ssize_t growhint)
{
    ypObject *result;
    if (ypTuple_LEN(sq) > ypTuple_LEN_MAX - 1) return yp_MemorySizeOverflowError;
    if (ypTuple_ALLOCLEN(sq) < ypTuple_LEN(sq) + 1) {
        result = _ypTuple_extend_grow(sq, ypTuple_LEN(sq) + 1, MAX(growhint, 0));
        if (yp_isexceptionC(result)) return result;
    }
    ypTuple_ARRAY(sq)[ypTuple_LEN(sq)] = yp_incref(x);
    ypTuple_SET_LEN(sq, ypTuple_LEN(sq) + 1);
    return yp_None;
}

// XXX sq and x _may_ be the same object
// XXX Check for the "extend with empty" case first
static ypObject *_ypTuple_extend_fromtuple(ypObject *sq, ypObject *x)
{
    yp_ssize_t i;
    yp_ssize_t newLen;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(x) == ypTuple_CODE);
    yp_ASSERT(ypTuple_LEN(x) > 0, "missed an 'extend with empty' optimization");

    if (ypTuple_LEN(sq) > ypTuple_LEN_MAX - ypTuple_LEN(x)) return yp_MemorySizeOverflowError;
    newLen = ypTuple_LEN(sq) + ypTuple_LEN(x);
    if (ypTuple_ALLOCLEN(sq) < newLen) {
        ypObject *result = _ypTuple_extend_grow(sq, newLen, 0);
        if (yp_isexceptionC(result)) return result;
    }

    yp_memcpy(ypTuple_ARRAY(sq) + ypTuple_LEN(sq), ypTuple_ARRAY(x),
            ypTuple_LEN(x) * yp_sizeof(ypObject *));
    for (i = ypTuple_LEN(sq); i < newLen; i++) yp_incref(ypTuple_ARRAY(sq)[i]);

    ypTuple_SET_LEN(sq, newLen);
    return yp_None;
}

static ypObject *_ypTuple_extend_fromminiiter(
        ypObject *sq, yp_ssize_t length_hint, ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject *x;
    ypObject *result;

    while (1) {
        x = yp_miniiter_next(mi, mi_state);  // new ref
        if (yp_isexceptionC(x)) {
            if (yp_isexceptionC2(x, yp_StopIteration)) break;
            return x;
        }
        length_hint -= 1;  // check for <0 only when we need it in _ypTuple_push
        result = _ypTuple_push(sq, x, length_hint);
        yp_decref(x);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

// XXX Check for the "fellow tuple/list" case _before_ calling this function
static ypObject *_ypTuple_extend_fromiterable(
        ypObject *sq, yp_ssize_t length_hint, ypObject *iterable)
{
    ypObject   *result;
    yp_uint64_t mi_state;
    ypObject   *mi;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(iterable) != ypTuple_CODE,
            "missed a 'fellow tuple/list' optimization");

    mi = yp_miniiter(iterable, &mi_state);  // new ref
    if (yp_isexceptionC(mi)) return mi;
    result = _ypTuple_extend_fromminiiter(sq, length_hint, mi, &mi_state);
    yp_decref(mi);
    return result;
}

// Check for the ypTuple_ALLOCLEN_MAX case first
static ypObject *_ypTuple_new_fromminiiter(
        int type, yp_ssize_t length_hint, ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject *newSq;
    ypObject *result;

    newSq = _ypTuple_new(type, length_hint, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newSq)) return newSq;

    result = _ypTuple_extend_fromminiiter(newSq, length_hint, mi, mi_state);
    if (yp_isexceptionC(result)) {
        yp_decref(newSq);
        return result;
    }

    // TODO We could avoid allocating for an empty iterable altogether if we get the first value
    // before allocating; is this complication worth the optimization?
    if (type == ypTuple_CODE && ypTuple_LEN(newSq) < 1) {
        yp_decref(newSq);
        return yp_tuple_empty;
    }

    return newSq;
}

// XXX Check for the "fellow tuple/list" case _before_ calling this function
static ypObject *_ypTuple_new_fromiterable(int type, ypObject *iterable)
{
    ypObject   *exc = yp_None;
    ypObject   *result;
    yp_ssize_t  length_hint;
    yp_uint64_t mi_state;
    ypObject   *mi;

    yp_ASSERT(ypObject_TYPE_PAIR_CODE(iterable) != ypTuple_CODE,
            "missed a 'fellow tuple/list' optimization");

    length_hint = yp_lenC(iterable, &exc);
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint; it just means we can't pre-allocate
        length_hint = yp_length_hintC(iterable, &exc);
        if (length_hint > ypTuple_ALLOCLEN_MAX) length_hint = ypTuple_ALLOCLEN_MAX;
    } else if (length_hint < 1) {
        // FIXME Should we be trusting len this much?
        // yp_lenC reports an empty iterable, so we can shortcut _ypTuple_new_fromminiiter
        if (type == ypTuple_CODE) return yp_tuple_empty;
        return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
    } else if (length_hint > ypTuple_LEN_MAX) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    mi = yp_miniiter(iterable, &mi_state);  // new ref
    if (yp_isexceptionC(mi)) return mi;
    result = _ypTuple_new_fromminiiter(type, length_hint, mi, &mi_state);
    yp_decref(mi);
    return result;
}

static ypObject *_ypTuple_concat_fromtuple(ypObject *sq, ypObject *x)
{
    int        sq_type = ypObject_TYPE_CODE(sq);
    yp_ssize_t newLen;
    ypObject  *newSq;
    yp_ssize_t i;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(x) == ypTuple_CODE);

    if (ypTuple_LEN(sq) < 1) {
        if (ypTuple_LEN(x) < 1) {
            // The concatenation is empty.
            if (sq_type == ypTuple_CODE) return yp_tuple_empty;
            return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
        } else {
            // The concatenation is equal to x.
            if (sq_type == ypTuple_CODE && ypObject_TYPE_CODE(x) == ypTuple_CODE) {
                return yp_incref(x);
            }
            return _ypTuple_copy(sq_type, x);
        }
    } else if (ypTuple_LEN(x) < 1) {
        // The concatenation is equal to sq.
        if (sq_type == ypTuple_CODE) return yp_incref(sq);
        return _ypTuple_copy(ypList_CODE, sq);
    }

    if (ypTuple_LEN(sq) > ypTuple_LEN_MAX - ypTuple_LEN(x)) return yp_MemorySizeOverflowError;
    newLen = ypTuple_LEN(sq) + ypTuple_LEN(x);

    newSq = _ypTuple_new(sq_type, newLen, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newSq)) return newSq;

    yp_memcpy(ypTuple_ARRAY(newSq), ypTuple_ARRAY(sq), ypTuple_LEN(sq) * yp_sizeof(ypObject *));
    yp_memcpy(ypTuple_ARRAY(newSq) + ypTuple_LEN(sq), ypTuple_ARRAY(x),
            ypTuple_LEN(x) * yp_sizeof(ypObject *));
    for (i = 0; i < newLen; i++) yp_incref(ypTuple_ARRAY(newSq)[i]);

    ypTuple_SET_LEN(newSq, newLen);
    return newSq;
}

// Called by setslice to discard the items at sq[start:stop] and shift the items at sq[stop:] to
// start at sq[stop+growBy]; the pointers at sq[start:stop+growBy] will be uninitialized. sq must
// have enough space allocated for the move. Updates ypTuple_LEN. Cannot fail.
static void _ypTuple_setslice_elemmove(
        ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t growBy)
{
    yp_ssize_t i;
    yp_ASSERT(growBy >= -ypTuple_LEN(sq), "growBy cannot be less than -len(sq)");
    yp_ASSERT(ypTuple_LEN(sq) + growBy <= ypTuple_ALLOCLEN(sq), "must be enough space for move");
    // FIXME What if yp_decref modifies sq?
    for (i = start; i < stop; i++) yp_decref(ypTuple_ARRAY(sq)[i]);
    ypTuple_ELEMMOVE(sq, stop + growBy, stop);
    ypTuple_SET_LEN(sq, ypTuple_LEN(sq) + growBy);
}

// Called on a setslice of step 1 and positive growBy, or an insert. Similar to
// _ypTuple_setslice_elemmove, except sq will grow if it doesn't have enough space allocated. On
// error, sq is not modified.
// XXX start and stop must be adjusted values.
static ypObject *_ypTuple_setslice_grow(
        ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t growBy, yp_ssize_t extra)
{
    yp_ssize_t newLen;
    ypObject **oldptr;
    yp_ssize_t i;

    yp_ASSERT(growBy >= 1, "growBy cannot be less than 1");
    yp_ASSERT(start >= 0 && stop >= 0, "start and stop must be adjusted values");

    // XXX We have to be careful that we do not discard items or otherwise modify sq until failure
    // becomes impossible
    if (ypTuple_LEN(sq) > ypTuple_LEN_MAX - growBy) return yp_MemorySizeOverflowError;
    newLen = ypTuple_LEN(sq) + growBy;
    if (ypTuple_ALLOCLEN(sq) < newLen) {
        oldptr = ypMem_REALLOC_CONTAINER_VARIABLE(
                sq, ypTupleObject, newLen, extra, ypTuple_ALLOCLEN_MAX);
        if (oldptr == NULL) return yp_MemoryError;
        if (ypTuple_ARRAY(sq) == oldptr) {
            _ypTuple_setslice_elemmove(sq, start, stop, growBy);
        } else {
            // The data doesn't overlap, so use memcpy, remembering to discard sq[start:stop]
            // FIXME What if yp_decref modifies sq?
            for (i = start; i < stop; i++) yp_decref(oldptr[i]);
            yp_memcpy(ypTuple_ARRAY(sq), oldptr, start * yp_sizeof(ypObject *));
            yp_memcpy(ypTuple_ARRAY(sq) + stop + growBy, oldptr + stop,
                    (ypTuple_LEN(sq) - stop) * yp_sizeof(ypObject *));
            ypMem_REALLOC_CONTAINER_FREE_OLDPTR(sq, ypTupleObject, oldptr);
            ypTuple_SET_LEN(sq, newLen);
        }
    } else {
        _ypTuple_setslice_elemmove(sq, start, stop, growBy);
    }
    return yp_None;
}

// XXX sq and x must _not_ be the same object (pass a copy of x if so)
static ypObject *list_delslice(ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step);
static ypObject *_ypTuple_setslice_fromtuple(
        ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, ypObject *x)
{
    ypObject  *result;
    yp_ssize_t slicelength;
    yp_ssize_t i;

    yp_ASSERT(sq != x, "make a copy of x when sq is x");

    // XXX Oddly, Python doesn't allow `sq[::-1] = []` on lists. I don't see a strong argument for
    // breaking with Python here.
    if (step == 1 && ypTuple_LEN(x) == 0) return list_delslice(sq, start, stop, step);

    result = ypSlice_AdjustIndicesC(ypTuple_LEN(sq), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    if (step == 1) {
        // Note that -len(sq)<=growBy<=len(x), so the growBy calculation can't overflow
        yp_ssize_t growBy = ypTuple_LEN(x) - slicelength;  // negative means list shrinking
        if (growBy > 0) {
            // TODO Overallocate?
            result = _ypTuple_setslice_grow(sq, start, stop, growBy, 0);
            if (yp_isexceptionC(result)) return result;
        } else {
            // Called even on growBy==0, as we need to discard items
            _ypTuple_setslice_elemmove(sq, start, stop, growBy);
        }

        // There are now len(x) elements starting at sq[start] waiting for x's items
        yp_memcpy(ypTuple_ARRAY(sq) + start, ypTuple_ARRAY(x),
                ypTuple_LEN(x) * yp_sizeof(ypObject *));
        for (i = start; i < start + ypTuple_LEN(x); i++) yp_incref(ypTuple_ARRAY(sq)[i]);
    } else {
        if (ypTuple_LEN(x) != slicelength) return yp_ValueError;
        for (i = 0; i < slicelength; i++) {
            ypObject **dest = ypTuple_ARRAY(sq) + ypSlice_INDEX(start, step, i);
            // FIXME What if yp_decref modifies sq?
            yp_decref(*dest);
            *dest = yp_incref(ypTuple_ARRAY(x)[i]);
        }
    }
    return yp_None;
}

// Public Methods

static ypObject *tuple_concat(ypObject *sq, ypObject *iterable)
{
    int        sq_type = ypObject_TYPE_CODE(sq);
    yp_ssize_t iterable_maxLen = ypTuple_LEN_MAX - ypTuple_LEN(sq);
    ypObject  *exc = yp_None;
    ypObject  *newSq;
    ypObject  *result;
    yp_ssize_t length_hint;

    if (ypObject_TYPE_PAIR_CODE(iterable) == ypTuple_CODE) {
        return _ypTuple_concat_fromtuple(sq, iterable);
    } else if (ypTuple_LEN(sq) < 1) {
        return _ypTuple_new_fromiterable(sq_type, iterable);
    }

    length_hint = yp_lenC(iterable, &exc);
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint; it just means we can't pre-allocate
        length_hint = yp_length_hintC(iterable, &exc);
        if (length_hint > iterable_maxLen) length_hint = iterable_maxLen;
    } else if (length_hint < 1) {
        // yp_lenC reports an empty iterable, so we can shortcut _ypTuple_extend_fromiterable
        if (sq_type == ypTuple_CODE) return yp_incref(sq);
        return _ypTuple_copy(ypList_CODE, sq);
    } else if (length_hint > iterable_maxLen) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    newSq = _ypTuple_new(sq_type, ypTuple_LEN(sq) + length_hint, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newSq)) return newSq;
    result = _ypTuple_extend_fromtuple(newSq, sq);  // TODO We don't need extend's alloclen check
    if (!yp_isexceptionC(result)) {
        result = _ypTuple_extend_fromiterable(newSq, length_hint, iterable);
    }
    if (yp_isexceptionC(result)) {
        yp_decref(newSq);
        return result;
    }

    return newSq;
}

static ypObject *tuple_repeat(ypObject *sq, yp_ssize_t factor)
{
    int        sq_type = ypObject_TYPE_CODE(sq);
    ypObject  *newSq;
    yp_ssize_t i;

    if (sq_type == ypTuple_CODE) {
        // If the result will be an empty tuple, return yp_tuple_empty
        if (ypTuple_LEN(sq) < 1 || factor < 1) return yp_tuple_empty;
        // If the result will be an exact copy, since we're immutable just return self
        if (factor == 1) return yp_incref(sq);
    } else {
        // If the result will be an empty list, return a new, empty list
        if (ypTuple_LEN(sq) < 1 || factor < 1) {
            return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
        }
        // If the result will be an exact copy, let the code below make that copy
    }

    if (factor > ypTuple_LEN_MAX / ypTuple_LEN(sq)) return yp_MemorySizeOverflowError;
    newSq = _ypTuple_new(sq_type, ypTuple_LEN(sq) * factor, /*alloclen_fixed=*/TRUE);  // new ref
    if (yp_isexceptionC(newSq)) return newSq;

    yp_memcpy(ypTuple_ARRAY(newSq), ypTuple_ARRAY(sq), ypTuple_LEN(sq) * yp_sizeof(ypObject *));
    _ypTuple_repeat_memcpy(newSq, factor, ypTuple_LEN(sq));

    ypTuple_SET_LEN(newSq, factor * ypTuple_LEN(sq));
    for (i = 0; i < ypTuple_LEN(newSq); i++) {
        yp_incref(ypTuple_ARRAY(newSq)[i]);
    }
    return newSq;
}

static ypObject *tuple_getindex(ypObject *sq, yp_ssize_t i, ypObject *defval)
{
    if (!ypSequence_AdjustIndexC(ypTuple_LEN(sq), &i)) {
        if (defval == NULL) return yp_IndexError;
        return yp_incref(defval);
    }
    return yp_incref(ypTuple_ARRAY(sq)[i]);
}

static ypObject *tuple_getslice(ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step)
{
    int        sq_type = ypObject_TYPE_CODE(sq);
    ypObject  *result;
    yp_ssize_t newLen;
    ypObject  *newSq;
    yp_ssize_t i;

    result = ypSlice_AdjustIndicesC(ypTuple_LEN(sq), &start, &stop, &step, &newLen);
    if (yp_isexceptionC(result)) return result;

    if (sq_type == ypTuple_CODE) {
        // If the result will be an empty tuple, return yp_tuple_empty
        if (newLen < 1) return yp_tuple_empty;
        // If the result will be an exact copy, since we're immutable just return self
        if (step == 1 && newLen == ypTuple_LEN(sq)) return yp_incref(sq);
    } else {
        // If the result will be an empty list, return a new, empty list
        if (newLen < 1) return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
        // If the result will be an exact copy, let the code below make that copy
    }

    // No need to check ypTuple_LEN_MAX: the slice can't be larger than sq is already
    newSq = _ypTuple_new(sq_type, newLen, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newSq)) return newSq;

    if (step == 1) {
        yp_memcpy(ypTuple_ARRAY(newSq), ypTuple_ARRAY(sq) + start, newLen * yp_sizeof(ypObject *));
        for (i = 0; i < newLen; i++) yp_incref(ypTuple_ARRAY(newSq)[i]);
    } else {
        for (i = 0; i < newLen; i++) {
            ypTuple_ARRAY(newSq)[i] = yp_incref(ypTuple_ARRAY(sq)[ypSlice_INDEX(start, step, i)]);
        }
    }
    ypTuple_SET_LEN(newSq, newLen);
    return newSq;
}

static ypObject *tuple_find(ypObject *sq, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        findfunc_direction direction, yp_ssize_t *index)
{
    ypObject  *result;
    yp_ssize_t step = 1;  // may change to -1
    yp_ssize_t sq_rlen;   // remaining length
    yp_ssize_t i;

    result = ypSlice_AdjustIndicesC(ypTuple_LEN(sq), &start, &stop, &step, &sq_rlen);
    if (yp_isexceptionC(result)) return result;
    if (sq_rlen < 1) goto not_found;

    if (direction == yp_FIND_REVERSE) {
        ypSlice_InvertIndicesC(&start, &stop, &step, sq_rlen);
    }

    for (i = start; sq_rlen > 0; i += step, sq_rlen--) {
        result = yp_eq(x, ypTuple_ARRAY(sq)[i]);
        if (yp_isexceptionC(result)) return result;
        if (ypBool_IS_TRUE_C(result)) {
            *index = i;
            return yp_None;
        }
    }
not_found:
    *index = -1;
    return yp_None;
}

static ypObject *tuple_count(
        ypObject *sq, ypObject *x, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t *count)
{
    ypObject  *result;
    yp_ssize_t step = 1;  // ignored; assumed unchanged by ypSlice_AdjustIndicesC
    yp_ssize_t newLen;    // ignored
    yp_ssize_t i;
    yp_ssize_t n = 0;

    result = ypSlice_AdjustIndicesC(ypTuple_LEN(sq), &start, &stop, &step, &newLen);
    if (yp_isexceptionC(result)) return result;

    for (i = start; i < stop; i++) {
        result = yp_eq(x, ypTuple_ARRAY(sq)[i]);
        if (yp_isexceptionC(result)) return result;
        if (ypBool_IS_TRUE_C(result)) n += 1;
    }
    *count = n;
    return yp_None;
}

static ypObject *list_setindex(ypObject *sq, yp_ssize_t i, ypObject *x)
{
    if (yp_isexceptionC(x)) return x;
    if (!ypSequence_AdjustIndexC(ypTuple_LEN(sq), &i)) {
        return yp_IndexError;
    }
    // FIXME What if yp_decref modifies sq?
    yp_decref(ypTuple_ARRAY(sq)[i]);
    ypTuple_ARRAY(sq)[i] = yp_incref(x);
    return yp_None;
}

static ypObject *list_setslice(
        ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, ypObject *x)
{
    // If x is not a tuple/list, or if it is the same object as sq, then a copy must first be made
    if (ypObject_TYPE_PAIR_CODE(x) == ypTuple_CODE && sq != x) {
        return _ypTuple_setslice_fromtuple(sq, start, stop, step, x);
    } else {
        ypObject *result;
        ypObject *x_astuple = yp_tuple(x);
        if (yp_isexceptionC(x_astuple)) return x_astuple;
        result = _ypTuple_setslice_fromtuple(sq, start, stop, step, x_astuple);
        yp_decref(x_astuple);
        return result;
    }
}

static ypObject *list_popindex(ypObject *sq, yp_ssize_t i)
{
    ypObject *result;

    if (!ypSequence_AdjustIndexC(ypTuple_LEN(sq), &i)) {
        return yp_IndexError;
    }

    result = ypTuple_ARRAY(sq)[i];
    ypTuple_ELEMMOVE(sq, i, i + 1);
    ypTuple_SET_LEN(sq, ypTuple_LEN(sq) - 1);
    return result;
}

// XXX Adapted from Python's reverse_slice
static void _list_reverse_slice(ypObject **lo, ypObject **hi)
{
    hi -= 1;
    while (lo < hi) {
        ypObject *t = *lo;
        *lo = *hi;
        *hi = t;
        lo += 1;
        hi -= 1;
    }
}

static ypObject *list_reverse(ypObject *sq)
{
    ypObject **lo = ypTuple_ARRAY(sq);
    ypObject **hi = lo + ypTuple_LEN(sq);
    _list_reverse_slice(lo, hi);
    return yp_None;
}

static ypObject *list_delindex(ypObject *sq, yp_ssize_t i)
{
    ypObject *result = list_popindex(sq, i);
    if (yp_isexceptionC(result)) return result;
    yp_decref(result);
    return yp_None;
}

static ypObject *list_clear(ypObject *sq);
static ypObject *list_delslice(ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step)
{
    ypObject  *result;
    yp_ssize_t slicelength;
    yp_ssize_t i;

    result = ypSlice_AdjustIndicesC(ypTuple_LEN(sq), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;
    if (slicelength < 1) return yp_None;  // no-op
    if (slicelength >= ypTuple_LEN(sq)) return list_clear(sq);

    // First discard references, then shift the remaining pointers
    // FIXME What if yp_decref modifies sq?
    for (i = 0; i < slicelength; i++) {
        yp_decref(ypTuple_ARRAY(sq)[ypSlice_INDEX(start, step, i)]);
    }
    _ypSlice_delslice_memmove(ypTuple_ARRAY(sq), ypTuple_LEN(sq), yp_sizeof(ypObject *), start,
            stop, step, slicelength);
    ypTuple_SET_LEN(sq, ypTuple_LEN(sq) - slicelength);
    return yp_None;
}

static ypObject *list_extend(ypObject *sq, ypObject *iterable)
{
    if (ypObject_TYPE_PAIR_CODE(iterable) == ypTuple_CODE) {
        if (ypTuple_LEN(iterable) < 1) return yp_None;  // no change
        return _ypTuple_extend_fromtuple(sq, iterable);
    } else {
        ypObject  *exc = yp_None;
        yp_ssize_t length_hint = yp_length_hintC(iterable, &exc);
        return _ypTuple_extend_fromiterable(sq, length_hint, iterable);
    }
}

static ypObject *list_irepeat(ypObject *sq, yp_ssize_t factor)
{
    ypObject  *result;
    yp_ssize_t startLen = ypTuple_LEN(sq);
    yp_ssize_t i;

    if (startLen < 1 || factor == 1) return yp_None;  // no-op
    if (factor < 1) return list_clear(sq);

    if (factor > ypTuple_LEN_MAX / startLen) return yp_MemorySizeOverflowError;
    result = _ypTuple_extend_grow(sq, startLen * factor, 0);
    if (yp_isexceptionC(result)) return result;

    _ypTuple_repeat_memcpy(sq, factor, startLen);

    // Remember that we already have references for [:startLen]
    ypTuple_SET_LEN(sq, ypTuple_LEN(sq) * factor);
    for (i = startLen; i < ypTuple_LEN(sq); i++) {
        yp_incref(ypTuple_ARRAY(sq)[i]);
    }
    return yp_None;
}

// TODO Python's ins1 does an item-by-item copy rather than a memmove. Contribute an optimization
// back to Python.
static ypObject *list_insert(ypObject *sq, yp_ssize_t i, ypObject *x)
{
    ypObject *result;

    // Check for exceptions, then adjust the index. Recall that insert behaves like sq[i:i]=[x], but
    // i can't be yp_SLICE_DEFAULT.
    if (i == yp_SLICE_DEFAULT) return yp_TypeError;
    if (yp_isexceptionC(x)) return x;
    if (i < 0) {
        i += ypTuple_LEN(sq);
        if (i < 0) i = 0;
    } else if (i > ypTuple_LEN(sq)) {
        i = ypTuple_LEN(sq);
    }

    // Make room at i and add x
    // TODO Overallocate
    result = _ypTuple_setslice_grow(sq, i, i, 1, 0);
    if (yp_isexceptionC(result)) return result;
    ypTuple_ARRAY(sq)[i] = yp_incref(x);
    return yp_None;
}

// list_popindex is above

static ypObject *tuple_traverse(ypObject *sq, visitfunc visitor, void *memo)
{
    yp_ssize_t i;
    for (i = 0; i < ypTuple_LEN(sq); i++) {
        ypObject *result = visitor(ypTuple_ARRAY(sq)[i], memo);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *tuple_unfrozen_copy(ypObject *sq) { return _ypTuple_copy(ypList_CODE, sq); }

static ypObject *tuple_frozen_copy(ypObject *sq)
{
    if (ypTuple_LEN(sq) < 1) return yp_tuple_empty;
    // A shallow copy of a tuple to a tuple doesn't require an actual copy
    if (ypObject_TYPE_CODE(sq) == ypTuple_CODE) return yp_incref(sq);
    return _ypTuple_copy(ypTuple_CODE, sq);
}

static ypObject *tuple_unfrozen_deepcopy(ypObject *sq, visitfunc copy_visitor, void *copy_memo)
{
    return _ypTuple_deepcopy(ypList_CODE, sq, copy_visitor, copy_memo);
}

static ypObject *tuple_frozen_deepcopy(ypObject *sq, visitfunc copy_visitor, void *copy_memo)
{
    if (ypTuple_LEN(sq) < 1) return yp_tuple_empty;
    return _ypTuple_deepcopy(ypTuple_CODE, sq, copy_visitor, copy_memo);
}

static ypObject *tuple_bool(ypObject *sq) { return ypBool_FROM_C(ypTuple_LEN(sq)); }

// Sets *i to the index in sq and x of the first differing element, or -1 if the elements are equal
// up to the length of the shortest object. Returns exception on error.
static ypObject *_ypTuple_cmp_first_difference(ypObject *sq, ypObject *x, yp_ssize_t *i)
{
    yp_ssize_t min_len;

    if (sq == x) {
        *i = -1;
        return yp_True;
    }
    if (ypObject_TYPE_PAIR_CODE(x) != ypTuple_CODE) return yp_ComparisonNotImplemented;

    // XXX Python's list implementation has protection against the length being modified during the
    // comparison; that isn't required here, at least not yet
    min_len = MIN(ypTuple_LEN(sq), ypTuple_LEN(x));
    for (*i = 0; *i < min_len; (*i)++) {
        ypObject *result = yp_eq(ypTuple_ARRAY(sq)[*i], ypTuple_ARRAY(x)[*i]);
        if (result != yp_True) return result;  // returns on yp_False or an exception
    }
    *i = -1;
    return yp_True;
}
// Here be tuple_lt, tuple_le, tuple_ge, tuple_gt
#define _ypTuple_RELATIVE_CMP_FUNCTION(name, len_cmp_op)                            \
    static ypObject *tuple_##name(ypObject *sq, ypObject *x)                        \
    {                                                                               \
        yp_ssize_t i = -1;                                                          \
        ypObject  *result = _ypTuple_cmp_first_difference(sq, x, &i);               \
        if (yp_isexceptionC(result)) return result;                                 \
        if (i < 0) return ypBool_FROM_C(ypTuple_LEN(sq) len_cmp_op ypTuple_LEN(x)); \
        return yp_##name(ypTuple_ARRAY(sq)[i], ypTuple_ARRAY(x)[i]);                \
    }
_ypTuple_RELATIVE_CMP_FUNCTION(lt, <);
_ypTuple_RELATIVE_CMP_FUNCTION(le, <=);
_ypTuple_RELATIVE_CMP_FUNCTION(ge, >=);
_ypTuple_RELATIVE_CMP_FUNCTION(gt, >);

// Returns yp_True if the two tuples/lists are equal. Size is a quick way to check equality.
// TODO comparison functions can recurse, just like currenthash...fix!
static ypObject *tuple_eq(ypObject *sq, ypObject *x)
{
    yp_ssize_t sq_len = ypTuple_LEN(sq);
    yp_ssize_t i;

    if (sq == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypTuple_CODE) return yp_ComparisonNotImplemented;
    if (sq_len != ypTuple_LEN(x)) return yp_False;

    // We need to inspect all our items for equality, which could be time-intensive. It's fairly
    // obvious that the pre-computed hash, if available, can save us some time when sq!=x.
    if (ypObject_CACHED_HASH(sq) != ypObject_HASH_INVALID &&
            ypObject_CACHED_HASH(x) != ypObject_HASH_INVALID &&
            ypObject_CACHED_HASH(sq) != ypObject_CACHED_HASH(x)) {
        return yp_False;
    }
    // TODO What if we haven't cached this hash yet, but we could?  Calculating the hash now could
    // speed up future comparisons against these objects. But!  What if we're a tuple of mutable
    // objects...we will then attempt to calculate the hash on every comparison, only to fail. If
    // we had a flag to differentiate "tuple of mutables" with "not yet computed"...crap, that
    // still wouldn't quite work, because what if we freeze those mutables?

    for (i = 0; i < sq_len; i++) {
        ypObject *result = yp_eq(ypTuple_ARRAY(sq)[i], ypTuple_ARRAY(x)[i]);
        if (result != yp_True) return result;  // returns on yp_False or an exception
    }
    return yp_True;
}
static ypObject *tuple_ne(ypObject *sq, ypObject *x)
{
    ypObject *result = tuple_eq(sq, x);
    return ypBool_NOT(result);
}

// XXX Adapted from Python's tuplehash
static ypObject *tuple_currenthash(
        ypObject *sq, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    yp_HashSequence_state_t state;
    yp_ssize_t              i;

    yp_HashSequence_init(&state, ypTuple_LEN(sq));
    for (i = 0; i < ypTuple_LEN(sq); i++) {
        // TODO What if the hash visitor changes sq?
        yp_hash_t lane;
        ypObject *result = hash_visitor(ypTuple_ARRAY(sq)[i], hash_memo, &lane);
        if (yp_isexceptionC(result)) return result;
        yp_HashSequence_next(&state, lane);
    }
    *hash = yp_HashSequence_fini(&state);

    return yp_None;
}

static ypObject *tuple_contains(ypObject *sq, ypObject *x)
{
    yp_ssize_t i;
    for (i = 0; i < ypTuple_LEN(sq); i++) {
        ypObject *result = yp_eq(x, ypTuple_ARRAY(sq)[i]);
        if (result != yp_False) return result;  // yp_True, or an exception
    }
    return yp_False;
}

static ypObject *tuple_len(ypObject *sq, yp_ssize_t *len)
{
    *len = ypTuple_LEN(sq);
    return yp_None;
}

static ypObject *list_push(ypObject *sq, ypObject *x)
{
    // TODO Overallocate via growhint
    return _ypTuple_push(sq, x, 0);
}

static ypObject *list_clear(ypObject *sq)
{
    // XXX yp_decref _could_ run code that requires us to be in a good state, so pop items from the
    // end one-at-a-time
    // TODO If yp_decref **adds** to this list, we'll never stop looping. We could use the detach
    // methods...but if the data is inline then a small buffer is allocated, which isn't great for
    // a clear method.
    while (ypTuple_LEN(sq) > 0) {
        ypTuple_SET_LEN(sq, ypTuple_LEN(sq) - 1);
        yp_decref(ypTuple_ARRAY(sq)[ypTuple_LEN(sq)]);
    }
    ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR(sq, ypTupleObject, ypTuple_ALLOCLEN_MAX);
    yp_ASSERT(ypTuple_ARRAY(sq) == ypTuple_INLINE_DATA(sq), "list_clear didn't allocate inline!");
    return yp_None;
}

static ypObject *list_pop(ypObject *sq)
{
    if (ypTuple_LEN(sq) < 1) return yp_IndexError;
    ypTuple_SET_LEN(sq, ypTuple_LEN(sq) - 1);
    return ypTuple_ARRAY(sq)[ypTuple_LEN(sq)];
}

// onmissing must be an immortal, or NULL
static ypObject *list_remove(ypObject *sq, ypObject *x, ypObject *onmissing)
{
    yp_ssize_t i;
    for (i = 0; i < ypTuple_LEN(sq); i++) {
        ypObject *result = yp_eq(x, ypTuple_ARRAY(sq)[i]);
        if (result == yp_False) continue;
        if (yp_isexceptionC(result)) return result;

        // result must be yp_True, so we found a match to remove
        // FIXME What if yp_decref modifies sq?
        yp_decref(ypTuple_ARRAY(sq)[i]);
        ypTuple_ELEMMOVE(sq, i, i + 1);
        ypTuple_SET_LEN(sq, ypTuple_LEN(sq) - 1);
        return yp_None;
    }
    if (onmissing == NULL) return yp_ValueError;
    return onmissing;
}

static ypObject *tuple_dealloc(ypObject *sq, void *memo)
{
    yp_ssize_t i;
    for (i = 0; i < ypTuple_LEN(sq); i++) {
        yp_decref_fromdealloc(ypTuple_ARRAY(sq)[i], memo);
    }
    ypMem_FREE_CONTAINER(sq, ypTupleObject);
    return yp_None;
}

static ypObject *tuple_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_tuple);
    return yp_tuple(argarray[1]);
}

static ypObject *list_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_list);
    return yp_list(argarray[1]);
}

#define _ypTuple_FUNC_NEW_PARAMETERS                                                              \
    ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_iterable), yp_CONST_REF(yp_tuple_empty)}, \
            {yp_CONST_REF(yp_s_slash), NULL})
yp_IMMORTAL_FUNCTION_static(tuple_func_new, tuple_func_new_code, _ypTuple_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(list_func_new, list_func_new_code, _ypTuple_FUNC_NEW_PARAMETERS);

static ypSequenceMethods ypTuple_as_sequence = {
        tuple_concat,                 // tp_concat
        tuple_repeat,                 // tp_repeat
        tuple_getindex,               // tp_getindex
        tuple_getslice,               // tp_getslice
        tuple_find,                   // tp_find
        tuple_count,                  // tp_count
        MethodError_objssizeobjproc,  // tp_setindex
        MethodError_objsliceobjproc,  // tp_setslice
        MethodError_objssizeproc,     // tp_delindex
        MethodError_objsliceproc,     // tp_delslice
        MethodError_objobjproc,       // tp_append
        MethodError_objobjproc,       // tp_extend
        MethodError_objssizeproc,     // tp_irepeat
        MethodError_objssizeobjproc,  // tp_insert
        MethodError_objssizeproc,     // tp_popindex
        MethodError_objproc,          // tp_reverse
        MethodError_objobjobjproc     // tp_sort
};

static ypTypeObject ypTuple_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(tuple_func_new),  // tp_func_new
        tuple_dealloc,                 // tp_dealloc
        tuple_traverse,                // tp_traverse
        NULL,                          // tp_str
        NULL,                          // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,      // tp_freeze
        tuple_unfrozen_copy,      // tp_unfrozen_copy
        tuple_frozen_copy,        // tp_frozen_copy
        tuple_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        tuple_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,      // tp_invalidate

        // Boolean operations and comparisons
        tuple_bool,  // tp_bool
        tuple_lt,    // tp_lt
        tuple_le,    // tp_le
        tuple_eq,    // tp_eq
        tuple_ne,    // tp_ne
        tuple_ge,    // tp_ge
        tuple_gt,    // tp_gt

        // Generic object operations
        tuple_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        tuple_contains,             // tp_contains
        tuple_len,                  // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypTuple_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypObject *list_sort(ypObject *, ypObject *, ypObject *);

static ypSequenceMethods ypList_as_sequence = {
        tuple_concat,    // tp_concat
        tuple_repeat,    // tp_repeat
        tuple_getindex,  // tp_getindex
        tuple_getslice,  // tp_getslice
        tuple_find,      // tp_find
        tuple_count,     // tp_count
        list_setindex,   // tp_setindex
        list_setslice,   // tp_setslice
        list_delindex,   // tp_delindex
        list_delslice,   // tp_delslice
        list_push,       // tp_append
        list_extend,     // tp_extend
        list_irepeat,    // tp_irepeat
        list_insert,     // tp_insert
        list_popindex,   // tp_popindex
        list_reverse,    // tp_reverse
        list_sort        // tp_sort
};

static ypTypeObject ypList_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(list_func_new),  // tp_func_new
        tuple_dealloc,                // tp_dealloc
        tuple_traverse,               // tp_traverse
        NULL,                         // tp_str
        NULL,                         // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,      // tp_freeze
        tuple_unfrozen_copy,      // tp_unfrozen_copy
        tuple_frozen_copy,        // tp_frozen_copy
        tuple_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        tuple_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,      // tp_invalidate

        // Boolean operations and comparisons
        tuple_bool,  // tp_bool
        tuple_lt,    // tp_lt
        tuple_le,    // tp_le
        tuple_eq,    // tp_eq
        tuple_ne,    // tp_ne
        tuple_ge,    // tp_ge
        tuple_gt,    // tp_gt

        // Generic object operations
        tuple_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        tuple_contains,             // tp_contains
        tuple_len,                  // tp_len
        list_push,                  // tp_push
        list_clear,                 // tp_clear
        list_pop,                   // tp_pop
        list_remove,                // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        _ypSequence_setitem,        // tp_setitem
        _ypSequence_delitem,        // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypList_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

ypObject *yp_itemarrayCX(ypObject *seq, yp_ssize_t *len, ypObject *const **array)
{
    if (ypObject_TYPE_PAIR_CODE(seq) != ypTuple_CODE) {
        *len = 0;
        *array = NULL;
        return_yp_BAD_TYPE(seq);
    }
    *len = ypTuple_LEN(seq);
    *array = ypTuple_ARRAY(seq);
    return yp_None;
}


// Custom ypQuickIter support

static ypObject *ypQuickIter_tuple_nextX(ypQuickIter_state *state)
{
    ypObject *x;
    if (state->tuple.i >= ypTuple_LEN(state->tuple.obj)) return NULL;
    x = ypTuple_ARRAY(state->tuple.obj)[state->tuple.i];  // borrowed
    state->tuple.i += 1;
    return x;
}

static ypObject *ypQuickIter_tuple_next(ypQuickIter_state *state)
{
    ypObject *x = ypQuickIter_tuple_nextX(state);
    return x == NULL ? NULL : yp_incref(x);
}

static ypObject *ypQuickIter_tuple_remaining_as_tuple(ypQuickIter_state *state)
{
    ypObject  *sq = state->tuple.obj;
    yp_ssize_t start = state->tuple.i;

    state->tuple.i = ypTuple_LEN(sq);  // exhaust the QuickIter, even on error

    if (start >= ypTuple_LEN(sq)) {
        return yp_tuple_empty;
    } else if (start < 1 && ypObject_TYPE_CODE(sq) == ypTuple_CODE) {
        return yp_incref(sq);
    } else {
        return _ypTuple_new_fromarray(
                ypTuple_CODE, ypTuple_LEN(sq) - start, ypTuple_ARRAY(sq) + start);
    }
}

static yp_ssize_t ypQuickIter_tuple_length_hint(
        ypQuickIter_state *state, int *isexact, ypObject **exc)
{
    yp_ASSERT(state->tuple.i >= 0 && state->tuple.i <= ypTuple_LEN(state->tuple.obj),
            "state->tuple.i should be in range(len+1)");
    *isexact = TRUE;
    return ypTuple_LEN(state->tuple.obj) - state->tuple.i;
}

static void ypQuickIter_tuple_close(ypQuickIter_state *state)
{
    // No-op. We don't yp_decref because it's a borrowed reference.
}

static const ypQuickIter_methods ypQuickIter_tuple_methods = {
        ypQuickIter_tuple_nextX,               // nextX
        ypQuickIter_tuple_next,                // next
        ypQuickIter_tuple_remaining_as_tuple,  // remaining_as_tuple
        ypQuickIter_tuple_length_hint,         // length_hint
        ypQuickIter_tuple_close                // close
};

// Initializes state with the given tuple. Always succeeds. Use ypQuickIter_tuple_methods as the
// method table. tuple is borrowed by state and most not be freed until methods->close is called.
static void ypQuickIter_new_fromtuple(ypQuickIter_state *state, ypObject *tuple)
{
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(tuple) == ypTuple_CODE, "tuple must be a tuple/list");
    state->tuple.i = 0;
    state->tuple.obj = tuple;
}


// Custom ypQuickSeq support

static ypObject *ypQuickSeq_tuple_getindexX(ypQuickSeq_state *state, yp_ssize_t i)
{
    yp_ASSERT(i >= 0, "negative indices not allowed in ypQuickSeq");
    if (i >= ypTuple_LEN(state->obj)) return NULL;
    return ypTuple_ARRAY(state->obj)[i];
}

static ypObject *ypQuickSeq_tuple_getindex(ypQuickSeq_state *state, yp_ssize_t i)
{
    ypObject *x = ypQuickSeq_tuple_getindexX(state, i);
    return x == NULL ? NULL : yp_incref(x);
}

static yp_ssize_t ypQuickSeq_tuple_len(ypQuickSeq_state *state, ypObject **exc)
{
    return ypTuple_LEN(state->obj);
}

static void ypQuickSeq_tuple_close(ypQuickSeq_state *state)
{
    // No-op. We don't yp_decref because it's a borrowed reference.
}

static const ypQuickSeq_methods ypQuickSeq_tuple_methods = {
        ypQuickSeq_tuple_getindexX,  // getindexX
        ypQuickSeq_tuple_getindex,   // getindex
        ypQuickSeq_tuple_len,        // len
        ypQuickSeq_tuple_close       // close
};

// Initializes state with the given tuple. Always succeeds. Use ypQuickSeq_tuple_methods as the
// method table. tuple is borrowed by state and must not be freed until methods->close is called.
static void ypQuickSeq_new_fromtuple(ypQuickSeq_state *state, ypObject *tuple)
{
    yp_ASSERT(ypObject_TYPE_PAIR_CODE(tuple) == ypTuple_CODE, "tuple must be a tuple/list");
    state->obj = tuple;
}


// Constructors

// XXX Check for the empty tuple/list cases first
static ypObject *_ypTupleNV(int type, int n, va_list args)
{
    int       i;
    ypObject *newSq = _ypTuple_new(type, n, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newSq)) return newSq;

    // Extract the objects from args first; we incref these later, which makes it easier to bail
    for (i = 0; i < n; i++) {
        ypObject *x = va_arg(args, ypObject *);  // borrowed
        if (yp_isexceptionC(x)) {
            yp_decref(newSq);
            return x;
        }
        ypTuple_ARRAY(newSq)[i] = x;
    }

    // Now set the other attributes, increment the reference counts, and return
    ypTuple_SET_LEN(newSq, n);
    for (i = 0; i < n; i++) yp_incref(ypTuple_ARRAY(newSq)[i]);
    return newSq;
}

ypObject *yp_tupleN(int n, ...)
{
    if (n < 1) return yp_tuple_empty;
    return_yp_V_FUNC(ypObject *, _ypTupleNV, (ypTuple_CODE, n, args), n);
}
ypObject *yp_tupleNV(int n, va_list args)
{
    if (n < 1) return yp_tuple_empty;
    return _ypTupleNV(ypTuple_CODE, n, args);
}

ypObject *yp_listN(int n, ...)
{
    if (n < 1) return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
    return_yp_V_FUNC(ypObject *, _ypTupleNV, (ypList_CODE, n, args), n);
}
ypObject *yp_listNV(int n, va_list args)
{
    if (n < 1) return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
    return _ypTupleNV(ypList_CODE, n, args);
}

ypObject *yp_tuple(ypObject *iterable)
{
    if (ypObject_TYPE_PAIR_CODE(iterable) == ypTuple_CODE) {
        if (ypTuple_LEN(iterable) < 1) return yp_tuple_empty;
        if (ypObject_TYPE_CODE(iterable) == ypTuple_CODE) return yp_incref(iterable);
        return _ypTuple_copy(ypTuple_CODE, iterable);
    }
    return _ypTuple_new_fromiterable(ypTuple_CODE, iterable);
}

ypObject *yp_list(ypObject *iterable)
{
    if (ypObject_TYPE_PAIR_CODE(iterable) == ypTuple_CODE) {
        return _ypTuple_copy(ypList_CODE, iterable);
    }
    return _ypTuple_new_fromiterable(ypList_CODE, iterable);
}

// Used by yp_call_arrayX/etc to convert array to a tuple (i.e. for *args).
// TODO: This _could_ be something we expose publically, but I don't want to create array versions
// of all va_list functions, so keep private for now.
static ypObject *yp_tuple_fromarray(yp_ssize_t n, ypObject *const *array)
{
    yp_ssize_t i;

    if (n < 1) return yp_tuple_empty;

    // Make sure we don't create a tuple containing exceptions.
    for (i = 0; i < n; i++) {
        if (yp_isexceptionC(array[i])) return array[i];
    }

    return _ypTuple_new_fromarray(ypTuple_CODE, n, array);
}

// Used by QuickIter to convert iterators into a tuple (i.e. for *args).
// TODO Again, we could make this public, but I'd rather not.
static ypObject *yp_tuple_fromminiiter(ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject  *exc = yp_None;
    yp_ssize_t length_hint = yp_miniiter_length_hintC(mi, mi_state, &exc);  // zero on error
    return _ypTuple_new_fromminiiter(ypTuple_CODE, length_hint, mi, mi_state);
}

ypObject *yp_sorted3(ypObject *iterable, ypObject *key, ypObject *reverse)
{
    ypObject *result;
    ypObject *newSq = yp_list(iterable);
    if (yp_isexceptionC(newSq)) return newSq;

    result = list_sort(newSq, key, reverse);
    if (yp_isexceptionC(result)) {
        yp_decref(newSq);
        return result;
    }

    return newSq;
}

ypObject *yp_sorted(ypObject *iterable) { return yp_sorted3(iterable, yp_None, yp_False); }

static ypObject *_ypTuple_repeatCNV(int type, yp_ssize_t factor, int n, va_list args)
{
    ypObject  *newSq;
    yp_ssize_t i;
    ypObject  *item;

    if (factor < 1 || n < 1) {
        if (type == ypTuple_CODE) return yp_tuple_empty;
        return _ypTuple_new(ypList_CODE, 0, /*alloclen_fixed=*/FALSE);
    }

    if (factor > ypTuple_LEN_MAX / n) return yp_MemorySizeOverflowError;
    newSq = _ypTuple_new(type, factor * n, /*alloclen_fixed=*/TRUE);  // new ref
    if (yp_isexceptionC(newSq)) return newSq;

    // Extract the objects from args first; we incref these later, which makes it easier to bail
    for (i = 0; i < n; i++) {
        item = va_arg(args, ypObject *);  // borrowed
        if (yp_isexceptionC(item)) {
            yp_decref(newSq);
            return item;
        }
        ypTuple_ARRAY(newSq)[i] = item;
    }
    _ypTuple_repeat_memcpy(newSq, factor, n);

    // Now set the other attributes, increment the reference counts, and return
    // TODO Do we want a back-door way to increment the reference counts by n?
    ypTuple_SET_LEN(newSq, factor * n);
    for (i = 0; i < ypTuple_LEN(newSq); i++) {
        yp_incref(ypTuple_ARRAY(newSq)[i]);
    }
    return newSq;
}

ypObject *yp_tuple_repeatCN(yp_ssize_t factor, int n, ...)
{
    return_yp_V_FUNC(ypObject *, _ypTuple_repeatCNV, (ypTuple_CODE, factor, n, args), n);
}
ypObject *yp_tuple_repeatCNV(yp_ssize_t factor, int n, va_list args)
{
    return _ypTuple_repeatCNV(ypTuple_CODE, factor, n, args);
}

ypObject *yp_list_repeatCN(yp_ssize_t factor, int n, ...)
{
    return_yp_V_FUNC(ypObject *, _ypTuple_repeatCNV, (ypList_CODE, factor, n, args), n);
}
ypObject *yp_list_repeatCNV(yp_ssize_t factor, int n, va_list args)
{
    return _ypTuple_repeatCNV(ypList_CODE, factor, n, args);
}

#pragma endregion list


/*************************************************************************************************
 * Timsort - Sorting sequences of generic items
 * XXX This entire section is adapted from Python's listobject.c
 *************************************************************************************************/
#pragma region timsort
// clang-format off

/* Lots of code for an adaptive, stable, natural mergesort.  There are many
 * pieces to this algorithm; read listsort.txt for overviews and details.
 */

/* A sortslice contains a pointer to an array of keys and a pointer to
 * an array of corresponding values.  In other words, keys[i]
 * corresponds with values[i].  If values == NULL, then the keys are
 * also the values.
 *
 * Several convenience routines are provided here, so that keys and
 * values are always moved in sync.
 */

typedef struct {
    ypObject **keys;
    ypObject **values;
} sortslice;

static void
sortslice_copy(sortslice *s1, yp_ssize_t i, sortslice *s2, yp_ssize_t j)
{
    s1->keys[i] = s2->keys[j];
    if (s1->values != NULL)
        s1->values[i] = s2->values[j];
}

static void
sortslice_copy_incr(sortslice *dst, sortslice *src)
{
    *dst->keys++ = *src->keys++;
    if (dst->values != NULL)
        *dst->values++ = *src->values++;
}

static void
sortslice_copy_decr(sortslice *dst, sortslice *src)
{
    *dst->keys-- = *src->keys--;
    if (dst->values != NULL)
        *dst->values-- = *src->values--;
}


static void
sortslice_memcpy(sortslice *s1, yp_ssize_t i, sortslice *s2, yp_ssize_t j,
                 yp_ssize_t n)
{
    yp_memcpy(&s1->keys[i], &s2->keys[j], yp_sizeof(ypObject *) * n);
    if (s1->values != NULL)
        yp_memcpy(&s1->values[i], &s2->values[j], yp_sizeof(ypObject *) * n);
}

static void
sortslice_memmove(sortslice *s1, yp_ssize_t i, sortslice *s2, yp_ssize_t j,
                  yp_ssize_t n)
{
    yp_memmove(&s1->keys[i], &s2->keys[j], yp_sizeof(ypObject *) * n);
    if (s1->values != NULL)
        yp_memmove(&s1->values[i], &s2->values[j], yp_sizeof(ypObject *) * n);
}

static void
sortslice_advance(sortslice *slice, yp_ssize_t n)
{
    slice->keys += n;
    if (slice->values != NULL)
        slice->values += n;
}

/* Comparison function: ms->key_compare, which is set at run-time in
 * listsort_impl to optimize for various special cases.
 * Returns -1 on error, 1 if x < y, 0 if x >= y.
 */

#define ISLT(X, Y) (*(ms->key_compare))(X, Y, ms)

/* Compare X to Y via "<".  Goto "fail" if the comparison raises an
   error.  Else "k" is set to true iff X<Y, and an "if (k)" block is
   started.  It makes more sense in context <wink>.  X and Y are PyObject*s.
*/
#define IFLT(X, Y) if ((k = ISLT(X, Y)) < 0) goto fail;  \
           if (k)

/* The maximum number of entries in a MergeState's pending-runs stack.
 * This is enough to sort arrays of size up to about
 *     32 * phi ** MAX_MERGE_PENDING
 * where phi ~= 1.618.  85 is ridiculouslylarge enough, good for an array
 * with 2**64 elements.
 */
#define MAX_MERGE_PENDING 85

/* When we get into galloping mode, we stay there until both runs win less
 * often than MIN_GALLOP consecutive times.  See listsort.txt for more info.
 */
#define MIN_GALLOP 7

/* Avoid malloc for small temp arrays. */
#define MERGESTATE_TEMP_SIZE 256

/* One MergeState exists on the stack per invocation of mergesort.  It's just
 * a convenient way to pass state around among the helper functions.
 */
struct s_slice {
    sortslice base;
    yp_ssize_t len;
};

typedef struct s_MergeState MergeState;
struct s_MergeState {
    /* This controls when we get *into* galloping mode.  It's initialized
     * to MIN_GALLOP.  merge_lo and merge_hi tend to nudge it higher for
     * random data, and lower for highly structured data.
     */
    yp_ssize_t min_gallop;

    /* 'a' is temp storage to help with merges.  It contains room for
     * alloced entries.
     */
    sortslice a;        /* may point to temparray below */
    yp_ssize_t alloced;

    /* A stack of n pending runs yet to be merged.  Run #i starts at
     * address base[i] and extends for len[i] elements.  It's always
     * true (so long as the indices are in bounds) that
     *
     *     pending[i].base + pending[i].len == pending[i+1].base
     *
     * so we could cut the storage for this, but it's a minor amount,
     * and keeping all the info explicit simplifies the code.
     */
    int n;
    struct s_slice pending[MAX_MERGE_PENDING];

    /* 'a' points to this when possible, rather than muck with malloc. */
    ypObject *temparray[MERGESTATE_TEMP_SIZE];

    /* This is the function we will use to compare two keys,
     * even when none of our special cases apply and we have to use
     * safe_object_compare. */
    int (*key_compare)(ypObject *, ypObject *, MergeState *);

    /* This function is used by unsafe_object_compare to optimize comparisons
     * when we know our list is type-homogeneous but we can't assume anything else.
     * In the pre-sort check it is set equal to Py_TYPE(key)->tp_richcompare */
    ypObject *(*key_lt)(ypObject *, ypObject *);

    /* This function is used by unsafe_tuple_compare to compare the first elements
     * of tuples. It may be set to safe_object_compare, but the idea is that hopefully
     * we can assume more, and use one of the special-case compares. */
    int (*tuple_elem_compare)(ypObject *, ypObject *, MergeState *);

    /* Set on exception. */
    ypObject *exc;
};

/* binarysort is the best method for sorting small arrays: it does
   few compares, but can do data movement quadratic in the number of
   elements.
   [lo, hi) is a contiguous slice of a list, and is sorted via
   binary insertion.  This sort is stable.
   On entry, must have lo <= start <= hi, and that [lo, start) is already
   sorted (pass start == lo if you don't know!).
   If islt() complains return -1, else 0.
   Even in case of error, the output slice will be some permutation of
   the input (nothing is lost or duplicated).
*/
static int
binarysort(MergeState *ms, sortslice lo, ypObject **hi, ypObject **start)
{
    yp_ssize_t k;
    ypObject **l, **p, **r;
    ypObject *pivot;

    yp_ASSERT1(lo.keys <= start && start <= hi);
    /* assert [lo, start) is sorted */
    if (lo.keys == start)
        ++start;
    for (; start < hi; ++start) {
        /* set l to where *start belongs */
        l = lo.keys;
        r = start;
        pivot = *r;
        /* Invariants:
         * pivot >= all in [lo, l).
         * pivot  < all in [r, start).
         * The second is vacuously true at the start.
         */
        yp_ASSERT1(l < r);
        do {
            p = l + ((r - l) >> 1);
            IFLT(pivot, *p)
                r = p;
            else
                l = p+1;
        } while (l < r);
        yp_ASSERT1(l == r);
        /* The invariants still hold, so pivot >= all in [lo, l) and
           pivot < all in [l, start), so pivot belongs at l.  Note
           that if there are elements equal to pivot, l points to the
           first slot after them -- that's why this sort is stable.
           Slide over to make room.
           Caution: using memmove is much slower under MSVC 5;
           we're not usually moving many slots. */
        for (p = start; p > l; --p)
            *p = *(p-1);
        *l = pivot;
        if (lo.values != NULL) {
            yp_ssize_t offset = lo.values - lo.keys;
            p = start + offset;
            pivot = *p;
            l += offset;
            for (p = start + offset; p > l; --p)
                *p = *(p-1);
            *l = pivot;
        }
    }
    return 0;

 fail:
    return -1;
}

/*
Return the length of the run beginning at lo, in the slice [lo, hi).  lo < hi
is required on entry.  "A run" is the longest ascending sequence, with

    lo[0] <= lo[1] <= lo[2] <= ...

or the longest descending sequence, with

    lo[0] > lo[1] > lo[2] > ...

Boolean *descending is set to 0 in the former case, or to 1 in the latter.
For its intended use in a stable mergesort, the strictness of the defn of
"descending" is needed so that the caller can safely reverse a descending
sequence without violating stability (strict > ensures there are no equal
elements to get out of order).

Returns -1 in case of error.
*/
static yp_ssize_t
count_run(MergeState *ms, ypObject **lo, ypObject **hi, int *descending)
{
    yp_ssize_t k;
    yp_ssize_t n;

    yp_ASSERT1(lo < hi);
    *descending = 0;
    ++lo;
    if (lo == hi)
        return 1;

    n = 2;
    IFLT(*lo, *(lo-1)) {
        *descending = 1;
        for (lo = lo+1; lo < hi; ++lo, ++n) {
            IFLT(*lo, *(lo-1))
                ;
            else
                break;
        }
    }
    else {
        for (lo = lo+1; lo < hi; ++lo, ++n) {
            IFLT(*lo, *(lo-1))
                break;
        }
    }

    return n;
fail:
    return -1;
}

/*
Locate the proper position of key in a sorted vector; if the vector contains
an element equal to key, return the position immediately to the left of
the leftmost equal element.  [gallop_right() does the same except returns
the position to the right of the rightmost equal element (if any).]

"a" is a sorted vector with n elements, starting at a[0].  n must be > 0.

"hint" is an index at which to begin the search, 0 <= hint < n.  The closer
hint is to the final result, the faster this runs.

The return value is the int k in 0..n such that

    a[k-1] < key <= a[k]

pretending that *(a-1) is minus infinity and a[n] is plus infinity.  IOW,
key belongs at index k; or, IOW, the first k elements of a should precede
key, and the last n-k should follow key.

Returns -1 on error.  See listsort.txt for info on the method.
*/
static yp_ssize_t
gallop_left(MergeState *ms, ypObject *key, ypObject **a, yp_ssize_t n, yp_ssize_t hint)
{
    yp_ssize_t ofs;
    yp_ssize_t lastofs;
    yp_ssize_t k;

    yp_ASSERT1(key && a && n > 0 && hint >= 0 && hint < n);

    a += hint;
    lastofs = 0;
    ofs = 1;
    IFLT(*a, key) {
        /* a[hint] < key -- gallop right, until
         * a[hint + lastofs] < key <= a[hint + ofs]
         */
        const yp_ssize_t maxofs = n - hint;             /* &a[n-1] is highest */
        while (ofs < maxofs) {
            IFLT(a[ofs], key) {
                lastofs = ofs;
                yp_ASSERT1(ofs <= (yp_SSIZE_T_MAX - 1) / 2);
                ofs = (ofs << 1) + 1;
            }
            else                /* key <= a[hint + ofs] */
                break;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to offsets relative to &a[0]. */
        lastofs += hint;
        ofs += hint;
    }
    else {
        /* key <= a[hint] -- gallop left, until
         * a[hint - ofs] < key <= a[hint - lastofs]
         */
        const yp_ssize_t maxofs = hint + 1;             /* &a[0] is lowest */
        while (ofs < maxofs) {
            IFLT(*(a-ofs), key)
                break;
            /* key <= a[hint - ofs] */
            lastofs = ofs;
            yp_ASSERT1(ofs <= (yp_SSIZE_T_MAX - 1) / 2);
            ofs = (ofs << 1) + 1;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to positive offsets relative to &a[0]. */
        k = lastofs;
        lastofs = hint - ofs;
        ofs = hint - k;
    }
    a -= hint;

    yp_ASSERT1(-1 <= lastofs && lastofs < ofs && ofs <= n);
    /* Now a[lastofs] < key <= a[ofs], so key belongs somewhere to the
     * right of lastofs but no farther right than ofs.  Do a binary
     * search, with invariant a[lastofs-1] < key <= a[ofs].
     */
    ++lastofs;
    while (lastofs < ofs) {
        yp_ssize_t m = lastofs + ((ofs - lastofs) >> 1);

        IFLT(a[m], key)
            lastofs = m+1;              /* a[m] < key */
        else
            ofs = m;                    /* key <= a[m] */
    }
    yp_ASSERT1(lastofs == ofs);             /* so a[ofs-1] < key <= a[ofs] */
    return ofs;

fail:
    return -1;
}

/*
Exactly like gallop_left(), except that if key already exists in a[0:n],
finds the position immediately to the right of the rightmost equal value.

The return value is the int k in 0..n such that

    a[k-1] <= key < a[k]

or -1 if error.

The code duplication is massive, but this is enough different given that
we're sticking to "<" comparisons that it's much harder to follow if
written as one routine with yet another "left or right?" flag.
*/
static yp_ssize_t
gallop_right(MergeState *ms, ypObject *key, ypObject **a, yp_ssize_t n, yp_ssize_t hint)
{
    yp_ssize_t ofs;
    yp_ssize_t lastofs;
    yp_ssize_t k;

    yp_ASSERT1(key && a && n > 0 && hint >= 0 && hint < n);

    a += hint;
    lastofs = 0;
    ofs = 1;
    IFLT(key, *a) {
        /* key < a[hint] -- gallop left, until
         * a[hint - ofs] <= key < a[hint - lastofs]
         */
        const yp_ssize_t maxofs = hint + 1;             /* &a[0] is lowest */
        while (ofs < maxofs) {
            IFLT(key, *(a-ofs)) {
                lastofs = ofs;
                yp_ASSERT1(ofs <= (yp_SSIZE_T_MAX - 1) / 2);
                ofs = (ofs << 1) + 1;
            }
            else                /* a[hint - ofs] <= key */
                break;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to positive offsets relative to &a[0]. */
        k = lastofs;
        lastofs = hint - ofs;
        ofs = hint - k;
    }
    else {
        /* a[hint] <= key -- gallop right, until
         * a[hint + lastofs] <= key < a[hint + ofs]
        */
        const yp_ssize_t maxofs = n - hint;             /* &a[n-1] is highest */
        while (ofs < maxofs) {
            IFLT(key, a[ofs])
                break;
            /* a[hint + ofs] <= key */
            lastofs = ofs;
            yp_ASSERT1(ofs <= (yp_SSIZE_T_MAX - 1) / 2);
            ofs = (ofs << 1) + 1;
        }
        if (ofs > maxofs)
            ofs = maxofs;
        /* Translate back to offsets relative to &a[0]. */
        lastofs += hint;
        ofs += hint;
    }
    a -= hint;

    yp_ASSERT1(-1 <= lastofs && lastofs < ofs && ofs <= n);
    /* Now a[lastofs] <= key < a[ofs], so key belongs somewhere to the
     * right of lastofs but no farther right than ofs.  Do a binary
     * search, with invariant a[lastofs-1] <= key < a[ofs].
     */
    ++lastofs;
    while (lastofs < ofs) {
        yp_ssize_t m = lastofs + ((ofs - lastofs) >> 1);

        IFLT(key, a[m])
            ofs = m;                    /* key < a[m] */
        else
            lastofs = m+1;              /* a[m] <= key */
    }
    yp_ASSERT1(lastofs == ofs);             /* so a[ofs-1] <= key < a[ofs] */
    return ofs;

fail:
    return -1;
}

/* Conceptually a MergeState's constructor. */
static void
merge_init(MergeState *ms, yp_ssize_t list_size, int has_keyfunc)
{
    yp_ASSERT1(ms != NULL);
    if (has_keyfunc) {
        /* The temporary space for merging will need at most half the list
         * size rounded up.  Use the minimum possible space so we can use the
         * rest of temparray for other things.  In particular, if there is
         * enough extra space, listsort() will use it to store the keys.
         */
        ms->alloced = (list_size + 1) / 2;

        /* ms->alloced describes how many keys will be stored at
           ms->temparray, but we also need to store the values.  Hence,
           ms->alloced is capped at half of MERGESTATE_TEMP_SIZE. */
        if (MERGESTATE_TEMP_SIZE / 2 < ms->alloced)
            ms->alloced = MERGESTATE_TEMP_SIZE / 2;
        ms->a.values = &ms->temparray[ms->alloced];
    }
    else {
        ms->alloced = MERGESTATE_TEMP_SIZE;
        ms->a.values = NULL;
    }
    ms->a.keys = ms->temparray;
    ms->n = 0;
    ms->min_gallop = MIN_GALLOP;
    ms->exc = yp_None;
}

/* Free all the temp memory owned by the MergeState.  This must be called
 * when you're done with a MergeState, and may be called before then if
 * you want to free the temp memory early.
 */
static void
merge_freemem(MergeState *ms)
{
    yp_ASSERT1(ms != NULL);
    if (ms->a.keys != ms->temparray)
        yp_free(ms->a.keys);
}

/* Ensure enough temp memory for 'need' array slots is available.
 * Returns 0 on success and -1 if the memory can't be gotten.
 */
static int
merge_getmem(MergeState *ms, yp_ssize_t need)
{
    yp_ssize_t multiplier;
    yp_ssize_t actual;

    yp_ASSERT1(ms != NULL);
    if (need <= ms->alloced)
        return 0;

    multiplier = ms->a.values != NULL ? 2 : 1;

    /* Don't realloc!  That can cost cycles to copy the old data, but
     * we don't care what's in the block.
     */
    // FIXME We can use nohtyP's custom realloc code here
    merge_freemem(ms);
    if (need > yp_SSIZE_T_MAX / yp_sizeof(ypObject*) / multiplier) {
        ms->exc = yp_MemorySizeOverflowError;
        return -1;
    }
    ms->a.keys = (ypObject**)yp_malloc(&actual, multiplier * need * yp_sizeof(ypObject *));
    if (ms->a.keys != NULL) {
        ms->alloced = need;
        if (ms->a.values != NULL)
            ms->a.values = &ms->a.keys[need];
        return 0;
    }
    ms->exc = yp_MemoryError;
    return -1;
}
#define MERGE_GETMEM(MS, NEED) ((NEED) <= (MS)->alloced ? 0 :   \
                                merge_getmem(MS, NEED))

/* Merge the na elements starting at ssa with the nb elements starting at
 * ssb.keys = ssa.keys + na in a stable way, in-place.  na and nb must be > 0.
 * Must also have that ssa.keys[na-1] belongs at the end of the merge, and
 * should have na <= nb.  See listsort.txt for more info.  Return 0 if
 * successful, -1 if error.
 */
static yp_ssize_t
merge_lo(MergeState *ms, sortslice ssa, yp_ssize_t na,
         sortslice ssb, yp_ssize_t nb)
{
    yp_ssize_t k;
    sortslice dest;
    int result = -1;            /* guilty until proved innocent */
    yp_ssize_t min_gallop;

    yp_ASSERT1(ms && ssa.keys && ssb.keys && na > 0 && nb > 0);
    yp_ASSERT1(ssa.keys + na == ssb.keys);
    if (MERGE_GETMEM(ms, na) < 0)
        return -1;
    sortslice_memcpy(&ms->a, 0, &ssa, 0, na);
    dest = ssa;
    ssa = ms->a;

    sortslice_copy_incr(&dest, &ssb);
    --nb;
    if (nb == 0)
        goto Succeed;
    if (na == 1)
        goto CopyB;

    min_gallop = ms->min_gallop;
    for (;;) {
        yp_ssize_t acount = 0;          /* # of times A won in a row */
        yp_ssize_t bcount = 0;          /* # of times B won in a row */

        /* Do the straightforward thing until (if ever) one run
         * appears to win consistently.
         */
        for (;;) {
            yp_ASSERT1(na > 1 && nb > 0);
            k = ISLT(ssb.keys[0], ssa.keys[0]);
            if (k) {
                if (k < 0)
                    goto Fail;
                sortslice_copy_incr(&dest, &ssb);
                ++bcount;
                acount = 0;
                --nb;
                if (nb == 0)
                    goto Succeed;
                if (bcount >= min_gallop)
                    break;
            }
            else {
                sortslice_copy_incr(&dest, &ssa);
                ++acount;
                bcount = 0;
                --na;
                if (na == 1)
                    goto CopyB;
                if (acount >= min_gallop)
                    break;
            }
        }

        /* One run is winning so consistently that galloping may
         * be a huge win.  So try that, and continue galloping until
         * (if ever) neither run appears to be winning consistently
         * anymore.
         */
        ++min_gallop;
        do {
            yp_ASSERT1(na > 1 && nb > 0);
            min_gallop -= min_gallop > 1;
            ms->min_gallop = min_gallop;
            k = gallop_right(ms, ssb.keys[0], ssa.keys, na, 0);
            acount = k;
            if (k) {
                if (k < 0)
                    goto Fail;
                sortslice_memcpy(&dest, 0, &ssa, 0, k);
                sortslice_advance(&dest, k);
                sortslice_advance(&ssa, k);
                na -= k;
                if (na == 1)
                    goto CopyB;
                /* na==0 is impossible now if the comparison
                 * function is consistent, but we can't assume
                 * that it is.
                 */
                if (na == 0)
                    goto Succeed;
            }
            sortslice_copy_incr(&dest, &ssb);
            --nb;
            if (nb == 0)
                goto Succeed;

            k = gallop_left(ms, ssa.keys[0], ssb.keys, nb, 0);
            bcount = k;
            if (k) {
                if (k < 0)
                    goto Fail;
                sortslice_memmove(&dest, 0, &ssb, 0, k);
                sortslice_advance(&dest, k);
                sortslice_advance(&ssb, k);
                nb -= k;
                if (nb == 0)
                    goto Succeed;
            }
            sortslice_copy_incr(&dest, &ssa);
            --na;
            if (na == 1)
                goto CopyB;
        } while (acount >= MIN_GALLOP || bcount >= MIN_GALLOP);
        ++min_gallop;           /* penalize it for leaving galloping mode */
        ms->min_gallop = min_gallop;
    }
Succeed:
    result = 0;
Fail:
    if (na)
        sortslice_memcpy(&dest, 0, &ssa, 0, na);
    return result;
CopyB:
    yp_ASSERT1(na == 1 && nb > 0);
    /* The last element of ssa belongs at the end of the merge. */
    sortslice_memmove(&dest, 0, &ssb, 0, nb);
    sortslice_copy(&dest, nb, &ssa, 0);
    return 0;
}

/* Merge the na elements starting at pa with the nb elements starting at
 * ssb.keys = ssa.keys + na in a stable way, in-place.  na and nb must be > 0.
 * Must also have that ssa.keys[na-1] belongs at the end of the merge, and
 * should have na >= nb.  See listsort.txt for more info.  Return 0 if
 * successful, -1 if error.
 */
static yp_ssize_t
merge_hi(MergeState *ms, sortslice ssa, yp_ssize_t na,
         sortslice ssb, yp_ssize_t nb)
{
    yp_ssize_t k;
    sortslice dest, basea, baseb;
    int result = -1;            /* guilty until proved innocent */
    yp_ssize_t min_gallop;

    yp_ASSERT1(ms && ssa.keys && ssb.keys && na > 0 && nb > 0);
    yp_ASSERT1(ssa.keys + na == ssb.keys);
    if (MERGE_GETMEM(ms, nb) < 0)
        return -1;
    dest = ssb;
    sortslice_advance(&dest, nb-1);
    sortslice_memcpy(&ms->a, 0, &ssb, 0, nb);
    basea = ssa;
    baseb = ms->a;
    ssb.keys = ms->a.keys + nb - 1;
    if (ssb.values != NULL)
        ssb.values = ms->a.values + nb - 1;
    sortslice_advance(&ssa, na - 1);

    sortslice_copy_decr(&dest, &ssa);
    --na;
    if (na == 0)
        goto Succeed;
    if (nb == 1)
        goto CopyA;

    min_gallop = ms->min_gallop;
    for (;;) {
        yp_ssize_t acount = 0;          /* # of times A won in a row */
        yp_ssize_t bcount = 0;          /* # of times B won in a row */

        /* Do the straightforward thing until (if ever) one run
         * appears to win consistently.
         */
        for (;;) {
            yp_ASSERT1(na > 0 && nb > 1);
            k = ISLT(ssb.keys[0], ssa.keys[0]);
            if (k) {
                if (k < 0)
                    goto Fail;
                sortslice_copy_decr(&dest, &ssa);
                ++acount;
                bcount = 0;
                --na;
                if (na == 0)
                    goto Succeed;
                if (acount >= min_gallop)
                    break;
            }
            else {
                sortslice_copy_decr(&dest, &ssb);
                ++bcount;
                acount = 0;
                --nb;
                if (nb == 1)
                    goto CopyA;
                if (bcount >= min_gallop)
                    break;
            }
        }

        /* One run is winning so consistently that galloping may
         * be a huge win.  So try that, and continue galloping until
         * (if ever) neither run appears to be winning consistently
         * anymore.
         */
        ++min_gallop;
        do {
            yp_ASSERT1(na > 0 && nb > 1);
            min_gallop -= min_gallop > 1;
            ms->min_gallop = min_gallop;
            k = gallop_right(ms, ssb.keys[0], basea.keys, na, na-1);
            if (k < 0)
                goto Fail;
            k = na - k;
            acount = k;
            if (k) {
                sortslice_advance(&dest, -k);
                sortslice_advance(&ssa, -k);
                sortslice_memmove(&dest, 1, &ssa, 1, k);
                na -= k;
                if (na == 0)
                    goto Succeed;
            }
            sortslice_copy_decr(&dest, &ssb);
            --nb;
            if (nb == 1)
                goto CopyA;

            k = gallop_left(ms, ssa.keys[0], baseb.keys, nb, nb-1);
            if (k < 0)
                goto Fail;
            k = nb - k;
            bcount = k;
            if (k) {
                sortslice_advance(&dest, -k);
                sortslice_advance(&ssb, -k);
                sortslice_memcpy(&dest, 1, &ssb, 1, k);
                nb -= k;
                if (nb == 1)
                    goto CopyA;
                /* nb==0 is impossible now if the comparison
                 * function is consistent, but we can't assume
                 * that it is.
                 */
                if (nb == 0)
                    goto Succeed;
            }
            sortslice_copy_decr(&dest, &ssa);
            --na;
            if (na == 0)
                goto Succeed;
        } while (acount >= MIN_GALLOP || bcount >= MIN_GALLOP);
        ++min_gallop;           /* penalize it for leaving galloping mode */
        ms->min_gallop = min_gallop;
    }
Succeed:
    result = 0;
Fail:
    if (nb)
        sortslice_memcpy(&dest, -(nb-1), &baseb, 0, nb);
    return result;
CopyA:
    yp_ASSERT1(nb == 1 && na > 0);
    /* The first element of ssb belongs at the front of the merge. */
    sortslice_memmove(&dest, 1-na, &ssa, 1-na, na);
    sortslice_advance(&dest, -na);
    sortslice_advance(&ssa, -na);
    sortslice_copy(&dest, 0, &ssb, 0);
    return 0;
}

/* Merge the two runs at stack indices i and i+1.
 * Returns 0 on success, -1 on error.
 */
static yp_ssize_t
merge_at(MergeState *ms, yp_ssize_t i)
{
    sortslice ssa, ssb;
    yp_ssize_t na, nb;
    yp_ssize_t k;

    yp_ASSERT1(ms != NULL);
    yp_ASSERT1(ms->n >= 2);
    yp_ASSERT1(i >= 0);
    yp_ASSERT1(i == ms->n - 2 || i == ms->n - 3);

    ssa = ms->pending[i].base;
    na = ms->pending[i].len;
    ssb = ms->pending[i+1].base;
    nb = ms->pending[i+1].len;
    yp_ASSERT1(na > 0 && nb > 0);
    yp_ASSERT1(ssa.keys + na == ssb.keys);

    /* Record the length of the combined runs; if i is the 3rd-last
     * run now, also slide over the last run (which isn't involved
     * in this merge).  The current run i+1 goes away in any case.
     */
    ms->pending[i].len = na + nb;
    if (i == ms->n - 3)
        ms->pending[i+1] = ms->pending[i+2];
    --ms->n;

    /* Where does b start in a?  Elements in a before that can be
     * ignored (already in place).
     */
    k = gallop_right(ms, *ssb.keys, ssa.keys, na, 0);
    if (k < 0)
        return -1;
    sortslice_advance(&ssa, k);
    na -= k;
    if (na == 0)
        return 0;

    /* Where does a end in b?  Elements in b after that can be
     * ignored (already in place).
     */
    nb = gallop_left(ms, ssa.keys[na-1], ssb.keys, nb, nb-1);
    if (nb <= 0)
        return nb;

    /* Merge what remains of the runs, using a temp array with
     * min(na, nb) elements.
     */
    if (na <= nb)
        return merge_lo(ms, ssa, na, ssb, nb);
    else
        return merge_hi(ms, ssa, na, ssb, nb);
}

/* Examine the stack of runs waiting to be merged, merging adjacent runs
 * until the stack invariants are re-established:
 *
 * 1. len[-3] > len[-2] + len[-1]
 * 2. len[-2] > len[-1]
 *
 * See listsort.txt for more info.
 *
 * Returns 0 on success, -1 on error.
 */
static int
merge_collapse(MergeState *ms)
{
    struct s_slice *p = ms->pending;

    yp_ASSERT1(ms);
    while (ms->n > 1) {
        yp_ssize_t n = ms->n - 2;
        if ((n > 0 && p[n-1].len <= p[n].len + p[n+1].len) ||
            (n > 1 && p[n-2].len <= p[n-1].len + p[n].len)) {
            if (p[n-1].len < p[n+1].len)
                --n;
            if (merge_at(ms, n) < 0)
                return -1;
        }
        else if (p[n].len <= p[n+1].len) {
            if (merge_at(ms, n) < 0)
                return -1;
        }
        else
            break;
    }
    return 0;
}

/* Regardless of invariants, merge all runs on the stack until only one
 * remains.  This is used at the end of the mergesort.
 *
 * Returns 0 on success, -1 on error.
 */
static int
merge_force_collapse(MergeState *ms)
{
    struct s_slice *p = ms->pending;

    yp_ASSERT1(ms);
    while (ms->n > 1) {
        yp_ssize_t n = ms->n - 2;
        if (n > 0 && p[n-1].len < p[n+1].len)
            --n;
        if (merge_at(ms, n) < 0)
            return -1;
    }
    return 0;
}

/* Compute a good value for the minimum run length; natural runs shorter
 * than this are boosted artificially via binary insertion.
 *
 * If n < 64, return n (it's too small to bother with fancy stuff).
 * Else if n is an exact power of 2, return 32.
 * Else return an int k, 32 <= k <= 64, such that n/k is close to, but
 * strictly less than, an exact power of 2.
 *
 * See listsort.txt for more info.
 */
static yp_ssize_t
merge_compute_minrun(yp_ssize_t n)
{
    yp_ssize_t r = 0;           /* becomes 1 if any 1 bits are shifted off */

    yp_ASSERT1(n >= 0);
    while (n >= 64) {
        r |= n & 1;
        n >>= 1;
    }
    return n + r;
}

static void
reverse_sortslice(sortslice *s, yp_ssize_t n)
{
    _list_reverse_slice(s->keys, &s->keys[n]);
    if (s->values != NULL)
        _list_reverse_slice(s->values, &s->values[n]);
}

/* Here we define custom comparison functions to optimize for the cases one commonly
 * encounters in practice: homogeneous lists, often of one of the basic types. */

/* This struct holds the comparison function and helper functions
 * selected in the pre-sort check. */

/* These are the special case compare functions.
 * ms->key_compare will always point to one of these: */

/* Heterogeneous compare: default, always safe to fall back on. */
static int
safe_object_compare(ypObject *v, ypObject *w, MergeState *ms)
{
    ypObject *result = yp_lt(v, w);
    if (yp_isexceptionC(result)) {
        ms->exc = result;
        return -1;
    }
    return ypBool_IS_TRUE_C(result);
}

// The unsafe_object_compare optimization is disabled because I doubt this provides any benefit in
// nohtyP: yp_lt already contains a lot of these optimizations. (TODO But we could profile...)
#if 0
/* Homogeneous compare: safe for any two comparable objects of the same type.
 * (ms->key_richcompare is set to ob_type->tp_richcompare in the
 *  pre-sort check.)
 */
static int
unsafe_object_compare(ypObject *v, ypObject *w, MergeState *ms)
{
    ypObject *res_obj; int res;
    ypObject *exc = yp_None;

    /* No assumptions, because we check first: */
    if (ypObject_TYPE(v)->tp_lt != ms->key_lt)
        return safe_object_compare(v, w, ms);

    yp_ASSERT1(ms->key_lt != NULL);
    res_obj = (*(ms->key_lt))(v, w);

    if (res_obj == yp_ComparisonNotImplemented) {
        return safe_object_compare(v, w, ms);
    }
    if (yp_isexceptionC(res_obj)) {
        ms->exc = res_obj;
        return -1;
    }

    yp_ASSERT(ypObject_TYPE_CODE(res_obj) == ypBool_CODE,
        "tp_lt must return yp_True, yp_False, or an exception");
    res = ypBool_IS_TRUE_C(res_obj);

    /* Note that we can't assert
     *     res == PyObject_RichCompareBool(v, w, Py_LT);
     * because of evil compare functions like this:
     *     lambda a, b:  int(random.random() * 3) - 1)
     * (which is actually in test_sort.py) */
    return res;
}
#endif

/* Latin string compare: safe for any two latin (one byte per char) strings. */
static int
unsafe_latin_compare(ypObject *v, ypObject *w, MergeState *ms)
{
    yp_ssize_t len;
    int res;

    /* Modified from Objects/unicodeobject.c:unicode_compare, assuming: */
    yp_ASSERT1(ypObject_TYPE_CODE(v) == ypStr_CODE);
    yp_ASSERT1(ypObject_TYPE_CODE(w) == ypStr_CODE);
    yp_ASSERT1(ypStr_ENC_CODE(v) == ypStr_ENC_CODE(w));
    yp_ASSERT1(ypStr_ENC_CODE(v) == ypStringLib_ENC_CODE_LATIN_1);

    len = MIN(ypStringLib_LEN(v), ypStringLib_LEN(w));
    res = yp_memcmp(ypStringLib_DATA(v), ypStringLib_DATA(w), len);

    res = (res != 0 ?
           res < 0 :
           ypStringLib_LEN(v) < ypStringLib_LEN(w));

    yp_ASSERT1(res == safe_object_compare(v, w, ms));;
    return res;
}

/* Bounded int compare: compare any two longs that fit in a single machine word. */
static int
unsafe_int_compare(ypObject *v, ypObject *w, MergeState *ms)
{
    yp_int_t v0, w0; int res;

    /* Modified from Objects/longobject.c:long_compare, assuming: */
    yp_ASSERT1(ypObject_TYPE_CODE(v) == ypInt_CODE);
    yp_ASSERT1(ypObject_TYPE_CODE(w) == ypInt_CODE);

    v0 = ypInt_VALUE(v);
    w0 = ypInt_VALUE(w);

    res = v0 < w0;
    yp_ASSERT1(res == safe_object_compare(v, w, ms));
    return res;
}

/* Float compare: compare any two floats. */
static int
unsafe_float_compare(ypObject *v, ypObject *w, MergeState *ms)
{
    int res;

    /* Modified from Objects/floatobject.c:float_richcompare, assuming: */
    yp_ASSERT1(ypObject_TYPE_CODE(v) == ypFloat_CODE);
    yp_ASSERT1(ypObject_TYPE_CODE(w) == ypFloat_CODE);

    res = ypFloat_VALUE(v) < ypFloat_VALUE(w);
    yp_ASSERT1(res == safe_object_compare(v, w, ms));
    return res;
}

/* Tuple compare: compare *any* two tuples, using
 * ms->tuple_elem_compare to compare the first elements, which is set
 * using the same pre-sort check as we use for ms->key_compare,
 * but run on the list [x[0] for x in L]. This allows us to optimize compares
 * on two levels (as long as [x[0] for x in L] is type-homogeneous.) The idea is
 * that most tuple compares don't involve x[1:]. */
static int
unsafe_tuple_compare(ypObject *v, ypObject *w, MergeState *ms)
{
    yp_ssize_t i, vlen, wlen;
    int k;

    /* Modified from Objects/tupleobject.c:tuplerichcompare, assuming: */
    yp_ASSERT1(ypObject_TYPE_CODE(v) == ypTuple_CODE);
    yp_ASSERT1(ypObject_TYPE_CODE(w) == ypTuple_CODE);
    yp_ASSERT1(ypTuple_LEN(v) > 0);
    yp_ASSERT1(ypTuple_LEN(w) > 0);

    vlen = ypTuple_LEN(v);
    wlen = ypTuple_LEN(w);

    for (i = 0; i < vlen && i < wlen; i++) {
        ypObject *result = yp_eq(ypTuple_ARRAY(v)[i], ypTuple_ARRAY(w)[i]);
        if (yp_isexceptionC(result)) {
            ms->exc = result;
            return -1;
        }
        k = ypBool_IS_TRUE_C(result);
        if (!k)
            break;
    }

    if (i >= vlen || i >= wlen)
        return vlen < wlen;

    if (i == 0)
        return ms->tuple_elem_compare(ypTuple_ARRAY(v)[i], ypTuple_ARRAY(w)[i], ms);
    else
        return safe_object_compare(ypTuple_ARRAY(v)[i], ypTuple_ARRAY(w)[i], ms);
}

/* An adaptive, stable, natural mergesort.  See listsort.txt.
 * Returns Py_None on success, NULL on error.  Even in case of error, the
 * list will be some permutation of its input state (nothing is lost or
 * duplicated).
 */
/*[clinic input]
list.sort

    *
    key as keyfunc: object = None
    reverse: bool(accept={int}) = False

Sort the list in ascending order and return None.

The sort is in-place (i.e. the list itself is modified) and stable (i.e. the
order of two equal elements is maintained).

If a key function is given, apply it once to each list item and sort them,
ascending or descending, according to their function values.

The reverse flag can be set to sort in descending order.
[clinic start generated code]*/

static ypObject *
list_sort(ypObject *self, ypObject *keyfunc, ypObject *_reverse)
/*[clinic end generated code: output=57b9f9c5e23fbe42 input=cb56cd179a713060]*/
{
    MergeState ms;
    yp_ssize_t nremaining;
    yp_ssize_t minrun;
    sortslice lo;
    ypTuple_detached detached;
    ypObject *result;
    int reverse;
    yp_ssize_t i;
    ypObject **keys;

    yp_ASSERT1(self != NULL);
    yp_ASSERT1(ypObject_TYPE_CODE(self) == ypList_CODE);

    // Convert arguments
    {
        ypObject *b = yp_bool(_reverse);
        if (yp_isexceptionC(b)) return b;
        reverse = ypBool_IS_TRUE_C(b);
    }

    if (ypTuple_LEN(self) < 1) {
        return yp_None;
    }

    /* The list is temporarily made empty, so that mutations performed
     * by comparison functions can't affect the slice of memory we're
     * sorting (allowing mutations during sorting is a core-dump
     * factory, since ob_item may change).
     */
    result = _ypTuple_detach_array(self, &detached);
    if (yp_isexceptionC(result)) return result;

    if (keyfunc == yp_None) {
        keys = NULL;
        lo.keys = detached.array;
        lo.values = NULL;
    }
    else {
        if (detached.len < MERGESTATE_TEMP_SIZE/2)
            /* Leverage stack space we allocated but won't otherwise use */
            keys = &ms.temparray[detached.len+1];
        else {
            yp_ssize_t actual;
            keys = yp_malloc(&actual, yp_sizeof(ypObject *) * detached.len);
            if (keys == NULL) {
                result = yp_MemoryError;  // returned by keyfunc_fail
                goto keyfunc_fail;
            }
        }

        for (i = 0; i < detached.len ; i++) {
            ypObject *argarray[] = {keyfunc, detached.array[i]};  // borrowed
            keys[i] = yp_call_arrayX(2, argarray);
            if (yp_isexceptionC(keys[i])) {
                result = keys[i];  // returned by keyfunc_fail
                for (i=i-1 ; i>=0 ; i--)
                    yp_decref(keys[i]);
                if (detached.len >= MERGESTATE_TEMP_SIZE/2)
                    yp_free(keys);
                goto keyfunc_fail;
            }
        }

        lo.keys = keys;
        lo.values = detached.array;
    }


    /* The pre-sort check: here's where we decide which compare function to use.
     * How much optimization is safe? We test for homogeneity with respect to
     * several properties that are expensive to check at compare-time, and
     * set ms appropriately. */
    // TODO This strategy may not play well with transmutations (i.e. invalidation).
    if (detached.len > 1) {
        /* Assume the first element is representative of the whole list. */
        int keys_are_in_tuples = (ypObject_TYPE_CODE(lo.keys[0]) == ypTuple_CODE &&
                                  ypTuple_LEN(lo.keys[0]) > 0);

        int key_type = (keys_are_in_tuples ?
                                  ypObject_TYPE_CODE(ypTuple_ARRAY(lo.keys[0])[0]) :
                                  ypObject_TYPE_CODE(lo.keys[0]));

        int keys_are_all_same_type = 1;
        int strings_are_latin = 1;

        /* Prove that assumption by checking every key. */
        for (i=0; i < detached.len; i++) {
            ypObject *key;

            if (keys_are_in_tuples &&
                !(ypObject_TYPE_CODE(lo.keys[i]) == ypTuple_CODE && ypTuple_LEN(lo.keys[i]) != 0)) {
                keys_are_in_tuples = 0;
                keys_are_all_same_type = 0;
                break;
            }

            /* Note: for lists of tuples, key is the first element of the tuple
             * lo.keys[i], not lo.keys[i] itself! We verify type-homogeneity
             * for lists of tuples in the if-statement directly above. */
            key = (keys_are_in_tuples ?
                             ypTuple_ARRAY(lo.keys[i])[0] :
                             lo.keys[i]);

            if (ypObject_TYPE_CODE(key) != key_type) {
                keys_are_all_same_type = 0;
                /* If keys are in tuple we must loop over the whole list to make
                   sure all items are tuples */
                if (!keys_are_in_tuples) {
                    break;
                }
            }

            if (keys_are_all_same_type) {
                if (key_type == ypStr_CODE &&
                        strings_are_latin &&
                        ypStr_ENC_CODE(key) != ypStringLib_ENC_CODE_LATIN_1) {

                    strings_are_latin = 0;
                }
            }
        }

        /* Choose the best compare, given what we now know about the keys. */
        if (keys_are_all_same_type) {

            if (key_type == ypStr_CODE && strings_are_latin) {
                ms.key_compare = unsafe_latin_compare;
            }
            else if (key_type == ypInt_CODE) {
                ms.key_compare = unsafe_int_compare;
            }
            else if (key_type == ypFloat_CODE) {
                ms.key_compare = unsafe_float_compare;
            }
#if 0
            else if ((ms.key_lt = ypTypeTable[key_type]->tp_lt) != NULL) {
                ms.key_compare = unsafe_object_compare;
            }
#endif
            else {
                ms.key_compare = safe_object_compare;
            }
        }
        else {
            ms.key_compare = safe_object_compare;
        }

        if (keys_are_in_tuples) {
            /* Make sure we're not dealing with tuples of tuples
             * (remember: here, key_type refers list [key[0] for key in keys]) */
            if (key_type == ypTuple_CODE) {
                ms.tuple_elem_compare = safe_object_compare;
            }
            else {
                ms.tuple_elem_compare = ms.key_compare;
            }

            ms.key_compare = unsafe_tuple_compare;
        }
    }
    /* End of pre-sort check: ms is now set properly! */

    merge_init(&ms, detached.len, keys != NULL);

    nremaining = detached.len;
    if (nremaining < 2)
        goto succeed;

    /* Reverse sort stability achieved by initially reversing the list,
    applying a stable forward sort, then reversing the final result. */
    if (reverse) {
        if (keys != NULL)
            _list_reverse_slice(&keys[0], &keys[detached.len]);
        _list_reverse_slice(&detached.array[0], &detached.array[detached.len]);
    }

    /* March over the array once, left to right, finding natural runs,
     * and extending short natural runs to minrun elements.
     */
    minrun = merge_compute_minrun(nremaining);
    do {
        int descending;
        yp_ssize_t n;

        /* Identify next run. */
        n = count_run(&ms, lo.keys, lo.keys + nremaining, &descending);
        if (n < 0)
            goto fail;
        if (descending)
            reverse_sortslice(&lo, n);
        /* If short, extend to min(minrun, nremaining). */
        if (n < minrun) {
            const yp_ssize_t force = nremaining <= minrun ?
                              nremaining : minrun;
            if (binarysort(&ms, lo, lo.keys + force, lo.keys + n) < 0)
                goto fail;
            n = force;
        }
        /* Push run onto pending-runs stack, and maybe merge. */
        yp_ASSERT1(ms.n < MAX_MERGE_PENDING);
        ms.pending[ms.n].base = lo;
        ms.pending[ms.n].len = n;
        ++ms.n;
        if (merge_collapse(&ms) < 0)
            goto fail;
        /* Advance to find next run. */
        sortslice_advance(&lo, n);
        nremaining -= n;
    } while (nremaining);

    if (merge_force_collapse(&ms) < 0)
        goto fail;
    yp_ASSERT1(ms.n == 1);
    yp_ASSERT1(keys == NULL
           ? ms.pending[0].base.keys == detached.array
           : ms.pending[0].base.keys == &keys[0]);
    yp_ASSERT1(ms.pending[0].len == detached.len);
    lo = ms.pending[0].base;

succeed:
    yp_ASSERT1(ms.exc == yp_None);
    result = yp_None;
    goto exit;

fail:
    yp_ASSERT1(yp_isexceptionC(ms.exc));
    result = ms.exc;

exit:
    if (keys != NULL) {
        for (i = 0; i < detached.len; i++)
            yp_decref(keys[i]);
        if (detached.len >= MERGESTATE_TEMP_SIZE/2)
            yp_free(keys);
    }

    if (reverse && detached.len > 1)
        _list_reverse_slice(detached.array, detached.array + detached.len);

    merge_freemem(&ms);

keyfunc_fail:
    {
        ypObject *reattachResult = _ypTuple_attach_array(self, &detached);
        if (yp_isexceptionC(reattachResult)) return reattachResult;
    }

    return result;
}
#undef IFLT
#undef ISLT

// clang-format on
#pragma endregion timsort


/*************************************************************************************************
 * Sets
 *************************************************************************************************/
#pragma region set

// XXX Much of this set/dict implementation is pulled right from Python, so best to read the
// original source for documentation on this implementation

// TODO Many set operations allocate temporary objects on the heap; is there a way to avoid this?

#define ypSet_TABLE(so) ((ypSet_KeyEntry *)((ypObject *)so)->ob_data)
#define ypSet_SET_TABLE(so, value) (((ypObject *)so)->ob_data = (void *)(value))
#define ypSet_LEN ypObject_CACHED_LEN
#define ypSet_SET_LEN ypObject_SET_CACHED_LEN
#define ypSet_FILL(so) (((ypSetObject *)so)->fill)
#define ypSet_ALLOCLEN ypObject_ALLOCLEN
#define ypSet_SET_ALLOCLEN ypObject_SET_ALLOCLEN
#define ypSet_MASK(so) (ypSet_ALLOCLEN(so) - 1)
#define ypSet_INLINE_DATA(so) (((ypSetObject *)so)->ob_inline_data)

#define ypSet_PERTURB_SHIFT (5)

// This tests that, by default, the inline data is enough to hold ypSet_ALLOCLEN_MIN elements
yp_STATIC_ASSERT((_ypMem_ideal_size_DEFAULT - yp_offsetof(ypSetObject, ob_inline_data)) /
                                 yp_sizeof(ypSet_KeyEntry) >=
                         ypSet_ALLOCLEN_MIN,
        ypSet_minsize_inline);

// The threshold at which we resize the set, expressed as a fraction of alloclen (ie 2/3)
#define ypSet_RESIZE_AT_NMR (2)  // numerator
#define ypSet_RESIZE_AT_DNM (3)  // denominator

// We don't want the multiplications in _ypSet_space_remaining, _ypSet_calc_alloclen, and
// _ypSet_resize overflowing, so we impose a separate limit on the maximum len and alloclen for
// sets
// XXX ypSet_ALLOCLEN_MAX may be larger than the true maximum
// TODO We could calculate the exact maximum in yp_initialize...
#define ypSet_ALLOCLEN_MAX                                                                        \
    ((yp_ssize_t)MIN4(yp_SSIZE_T_MAX / ypSet_RESIZE_AT_NMR, yp_SSIZE_T_MAX / ypSet_RESIZE_AT_DNM, \
            (yp_SSIZE_T_MAX - yp_sizeof(ypSetObject)) / yp_sizeof(ypSet_KeyEntry),                \
            ypObject_LEN_MAX))
#define ypSet_LEN_MAX ((yp_ssize_t)(ypSet_ALLOCLEN_MAX * ypSet_RESIZE_AT_NMR) / ypSet_RESIZE_AT_DNM)

// A placeholder to replace deleted entries in the hash table
yp_IMMORTAL_INVALIDATED(ypSet_dummy);

// Returns true if the given ypSet_KeyEntry contains a valid key
#define ypSet_ENTRY_USED(loc) ((loc)->se_key != NULL && (loc)->se_key != ypSet_dummy)
// Returns the index of the given ypSet_KeyEntry in the hash table
#define ypSet_ENTRY_INDEX(so, loc) ((yp_ssize_t)((loc)-ypSet_TABLE(so)))

// set code relies on some of the internals from the dict implementation
#define ypDict_LEN ypObject_CACHED_LEN
#define ypDict_SET_LEN ypObject_SET_CACHED_LEN

// Before adding keys to the set, call this function to determine if a resize is necessary.
// Returns 0 if the set should first be resized, otherwise returns the number of keys that can be
// added before the next resize.
// XXX Adapted from PyDict_SetItem, although our thresholds are slightly different
// TODO If we make this threshold configurable, the assert should be in yp_initialize
yp_STATIC_ASSERT(ypSet_RESIZE_AT_NMR <= yp_SSIZE_T_MAX / ypSet_ALLOCLEN_MAX,
        ypSet_space_remaining_cant_overflow);
static yp_ssize_t _ypSet_space_remaining(ypObject *so)
{
    /* If fill >= 2/3 size, adjust size. Normally, this doubles or
     * quaduples the size, but it's also possible for the dict to shrink
     * (if ma_fill is much larger than se_used, meaning a lot of dict
     * keys have been deleted).
     */
    // XXX ypSet_space_remaining_cant_overflow ensures this can't overflow
    yp_ssize_t retval = (ypSet_ALLOCLEN(so) * ypSet_RESIZE_AT_NMR) / ypSet_RESIZE_AT_DNM;
    retval -= ypSet_FILL(so);
    if (retval <= 0) return 0;  // should resize before adding keys
    return retval;
}

// Returns the alloclen that will fit minused entries, or <1 on error
// XXX Adapted from Python's dictresize
// TODO Can we improve by using some bit-twiddling to get the highest power of 2?
// TODO If we make this threshold configurable, the assert should be in yp_initialize
yp_STATIC_ASSERT(
        ypSet_RESIZE_AT_DNM <= yp_SSIZE_T_MAX / ypSet_LEN_MAX, ypSet_calc_alloclen_cant_overflow);
static yp_ssize_t _ypSet_calc_alloclen(yp_ssize_t minused)
{
    yp_ssize_t minentries;
    yp_ssize_t alloclen;

    yp_ASSERT(minused >= 0, "minused cannot be negative");
    yp_ASSERT(minused <= ypSet_LEN_MAX, "minused cannot be greater than max");

    // XXX ypSet_calc_alloclen_cant_overflow ensures this can't overflow
    minentries = ((minused * ypSet_RESIZE_AT_DNM) / ypSet_RESIZE_AT_NMR) + 1;
    alloclen = ypSet_ALLOCLEN_MIN;
    while (alloclen <= minentries && alloclen > 0) {
        alloclen <<= 1;
    }

    // TODO If we could trust that ypSet_ALLOCLEN_MAX was the true maximum (ie, a power of 2), then
    // we could turn this to an assert, or just remove it: ypSet_LEN_MAX would ensure this is never
    // reached
    if (alloclen > ypSet_ALLOCLEN_MAX) return -1;
    return alloclen;
}

// Returns a new, empty set or frozenset object to hold minused entries
// XXX Check for the yp_frozenset_empty case first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Overallocate to avoid future resizings
static ypObject *_ypSet_new(int type, yp_ssize_t minused, int alloclen_fixed)
{
    ypObject  *so;
    yp_ssize_t alloclen = _ypSet_calc_alloclen(minused);
    if (alloclen < 1) return yp_MemorySizeOverflowError;
    if (alloclen_fixed && type == ypFrozenSet_CODE) {
        // FIXME Dict intentionally creates empty frozensets that are not frozenset_empty. Perhaps
        // it shouldn't? But then again I'm rethinking this whole keyset business...
        // yp_ASSERT(minused > 0, "missed a yp_frozenset_empty optimization");
        so = ypMem_MALLOC_CONTAINER_INLINE(
                ypSetObject, ypFrozenSet_CODE, alloclen, ypSet_ALLOCLEN_MAX);
    } else {
        so = ypMem_MALLOC_CONTAINER_VARIABLE(ypSetObject, type, alloclen, 0, ypSet_ALLOCLEN_MAX);
    }
    if (yp_isexceptionC(so)) return so;
    // XXX alloclen must be a power of 2; it's unlikely we'd be given double the requested memory
    ypSet_SET_ALLOCLEN(so, alloclen);
    ypSet_FILL(so) = 0;
    yp_memset(ypSet_TABLE(so), 0, alloclen * yp_sizeof(ypSet_KeyEntry));
    yp_ASSERT(_ypSet_space_remaining(so) >= minused, "new set doesn't have requested room");
    return so;
}

// XXX Check for the "lazy shallow copy" and "yp_frozenset_empty" cases first
// TODO It's tempting to look into memcpy to copy the tables, although that would mean the copy
// would be just as dirty as the original. But if the original isn't "too dirty"...
static void _ypSet_movekey_clean(ypObject *so, ypObject *key, yp_hash_t hash, ypSet_KeyEntry **ep);
static ypObject *_ypSet_copy(int type, ypObject *x, int alloclen_fixed)
{
    yp_ssize_t      keysleft = ypSet_LEN(x);
    ypSet_KeyEntry *otherkeys = ypSet_TABLE(x);
    ypObject       *so;
    yp_ssize_t      i;
    ypSet_KeyEntry *loc;

    so = _ypSet_new(type, keysleft, alloclen_fixed);
    if (yp_isexceptionC(so)) return so;

    // The set is empty and contains no deleted entries, so we can use _ypSet_movekey_clean
    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&otherkeys[i])) continue;
        keysleft -= 1;
        _ypSet_movekey_clean(so, yp_incref(otherkeys[i].se_key), otherkeys[i].se_hash, &loc);
    }
    return so;
}

// XXX Check for the yp_frozenset_empty case first
// FIXME We're trusting that copy_visitor will behave properly and return an object that has the
// same hash as the original and that is unequal to anything else in the other set...bad assumption!
static ypObject *_ypSet_deepcopy(int type, ypObject *x, visitfunc copy_visitor, void *copy_memo)
{
    // FIXME This will fail if x is modified during the copy: ypSet_LEN and ypSet_TABLE may change!
    yp_ssize_t      keysleft = ypSet_LEN(x);
    ypSet_KeyEntry *otherkeys = ypSet_TABLE(x);
    ypObject       *so;
    yp_ssize_t      i;
    ypObject       *key;
    ypSet_KeyEntry *loc;
    ypObject       *result;

    // XXX Unlike _ypTuple_deepcopy, we don't have to worry about sets that contain themselves,
    // which simplifies this greatly.
    so = _ypSet_new(type, keysleft, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(so)) return so;

    // The set is empty and contains no deleted entries, so we can use _ypSet_movekey_clean.
    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&otherkeys[i])) continue;
        keysleft -= 1;
        key = copy_visitor(otherkeys[i].se_key, copy_memo);
        if (yp_isexceptionC(key)) {
            yp_decref(so);
            return key;
        }
        _ypSet_movekey_clean(so, key, otherkeys[i].se_hash, &loc);
    }

    result = _yp_deepcopy_memo_setitem(copy_memo, x, so);
    if (yp_isexceptionC(result)) {
        yp_decref(so);
        return result;
    }

    return so;
}

// Resizes the set to the smallest size that will hold minused values. If you want to reduce the
// need for future resizes, call with a larger minused. Returns yp_None, or an exception on error.
// TODO Do we want to split minused into required and extra, like in other areas?
yp_STATIC_ASSERT(ypSet_ALLOCLEN_MAX <= yp_SSIZE_T_MAX / yp_sizeof(ypSet_KeyEntry),
        ypSet_resize_cant_overflow);
static ypObject *_ypSet_resize(ypObject *so, yp_ssize_t minused)
{
    yp_ssize_t      newalloclen;
    ypSet_KeyEntry *oldkeys;
    yp_ssize_t      keysleft;
    yp_ssize_t      i;
    ypSet_KeyEntry *loc;

    yp_ASSERT1(so != yp_frozenset_empty);  // ensure we don't modify the "empty" frozenset

    // Always allocate a separate buffer.
    newalloclen = _ypSet_calc_alloclen(minused);
    if (newalloclen < 1) return yp_MemorySizeOverflowError;
    // XXX ypSet_resize_cant_overflow ensures this can't overflow
    oldkeys = ypMem_REALLOC_CONTAINER_VARIABLE_NEW(
            so, ypSetObject, newalloclen, 0, ypSet_ALLOCLEN_MAX);
    if (oldkeys == NULL) return yp_MemoryError;
    yp_memset(ypSet_TABLE(so), 0, newalloclen * yp_sizeof(ypSet_KeyEntry));
    // XXX alloclen must be a power of 2; it's unlikely we'd be given double the requested memory
    ypSet_SET_ALLOCLEN(so, newalloclen);

    // Clear the new table.
    keysleft = ypSet_LEN(so);
    ypSet_SET_LEN(so, 0);
    ypSet_FILL(so) = 0;
    yp_ASSERT(_ypSet_space_remaining(so) >= minused, "resized set doesn't have requested room");

    // Move the keys from the old table before free'ing it.
    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&oldkeys[i])) continue;
        keysleft -= 1;
        _ypSet_movekey_clean(so, oldkeys[i].se_key, oldkeys[i].se_hash, &loc);
    }
    ypMem_REALLOC_CONTAINER_FREE_OLDPTR(so, ypSetObject, oldkeys);
    yp_DEBUG("_ypSet_resize: %p table %p  (was %p)", so, ypSet_TABLE(so), oldkeys);
    return yp_None;
}

// Sets *loc to where the key should go in the table; it may already be there, in fact!  Returns
// yp_None, or an exception on error.
// TODO The dict implementation has a bunch of these for various scenarios; let's keep it simple
// for now, but investigate...
// TODO Update as per http://bugs.python.org/issue18771?
// XXX Adapted from Python's lookdict in dictobject.c
static ypObject *_ypSet_lookkey(
        ypObject *so, ypObject *key, register yp_hash_t hash, ypSet_KeyEntry **loc)
{
    register size_t          i;
    register size_t          perturb;
    register ypSet_KeyEntry *freeslot;
    register size_t          mask = (size_t)ypSet_MASK(so);
    ypSet_KeyEntry          *ep0 = ypSet_TABLE(so);
    register ypSet_KeyEntry *ep;
    register ypObject       *cmp;

    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->se_key == NULL || ep->se_key == key) goto success;

    if (ep->se_key == ypSet_dummy) {
        freeslot = ep;
    } else {
        if (ep->se_hash == hash) {
            // Python has protection here against __eq__ changing this set object; hopefully not a
            // problem in nohtyP
            cmp = yp_eq(ep->se_key, key);
            if (yp_isexceptionC(cmp)) return cmp;
            if (cmp == yp_True) goto success;
        }
        freeslot = NULL;
    }

    // In the loop, se_key == ypSet_dummy is by far (factor of 100s) the least likely
    // outcome, so test for that last
    for (perturb = (size_t)hash;; perturb >>= ypSet_PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->se_key == NULL) {
            if (freeslot != NULL) ep = freeslot;
            goto success;
        }
        if (ep->se_key == key) goto success;
        if (ep->se_hash == hash && ep->se_key != ypSet_dummy) {
            // Same __eq__ protection is here as well in Python
            cmp = yp_eq(ep->se_key, key);
            if (yp_isexceptionC(cmp)) return cmp;
            if (cmp == yp_True) goto success;
        } else if (ep->se_key == ypSet_dummy && freeslot == NULL) {
            freeslot = ep;
        }
    }
    //lint -unreachable
    return yp_SystemError;

success:
    // When the code jumps here, it means ep points to the proper entry
    *loc = ep;
    return yp_None;
}

// Steals key and adds it to the hash table at the given location. loc must not currently be in
// use! Ensure the set is large enough (_ypSet_space_remaining) before adding items.
// XXX Adapted from Python's insertdict in dictobject.c
static void _ypSet_movekey(ypObject *so, ypSet_KeyEntry *loc, ypObject *key, yp_hash_t hash)
{
    yp_ASSERT1(so != yp_frozenset_empty);  // ensure we don't modify the "empty" frozenset

    if (loc->se_key == NULL) ypSet_FILL(so) += 1;
    loc->se_key = key;
    loc->se_hash = hash;
    ypSet_SET_LEN(so, ypSet_LEN(so) + 1);
}

// Steals key and adds it to the *clean* hash table. Only use if the key is known to be absent
// from the table, and the table contains no deleted entries; this is usually known when
// cleaning/resizing/copying a table. Sets *loc to the location at which the key was inserted.
// Ensure the set is large enough (_ypSet_space_remaining) before adding items.
// XXX Adapted from Python's insertdict_clean in dictobject.c
static void _ypSet_movekey_clean(ypObject *so, ypObject *key, yp_hash_t hash, ypSet_KeyEntry **ep)
{
    size_t          i;
    size_t          perturb;
    size_t          mask = (size_t)ypSet_MASK(so);
    ypSet_KeyEntry *ep0 = ypSet_TABLE(so);

    yp_ASSERT1(so != yp_frozenset_empty);  // ensure we don't modify the "empty" frozenset

    i = (size_t)hash & mask;
    (*ep) = &ep0[i];
    for (perturb = (size_t)hash; (*ep)->se_key != NULL; perturb >>= ypSet_PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        (*ep) = &ep0[i & mask];
    }
    ypSet_FILL(so) += 1;
    (*ep)->se_key = key;
    (*ep)->se_hash = hash;
    ypSet_SET_LEN(so, ypSet_LEN(so) + 1);
}

// Removes the key at the given location from the hash table and returns the reference to it (the
// key's reference count is not modified).
static ypObject *_ypSet_removekey(ypObject *so, ypSet_KeyEntry *loc)
{
    ypObject *oldkey = loc->se_key;
    yp_ASSERT1(so != yp_frozenset_empty);  // ensure we don't modify the "empty" frozenset
    loc->se_key = ypSet_dummy;
    loc->se_hash = ypObject_HASH_INVALID;
    ypSet_SET_LEN(so, ypSet_LEN(so) - 1);
    return oldkey;
}

// Adds the key to the hash table. *spaceleft should be initialized from  _ypSet_space_remaining;
// this function then decrements it with each key added, and resets it on every resize. growhint
// is the number of additional items, not including key, that are expected to be added to the set.
// Returns yp_True if so was modified, yp_False if it wasn't due to the key already being in the
// set, or an exception on error.
// XXX Adapted from PyDict_SetItem
static ypObject *_ypSet_push(
        ypObject *so, ypObject *key, yp_ssize_t *spaceleft, yp_ssize_t growhint)
{
    yp_hash_t       hash;
    ypObject       *result = yp_None;
    ypSet_KeyEntry *loc;
    yp_ssize_t      newlen;

    // Look for the appropriate entry in the hash table
    hash = yp_hashC(key, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(so, key, hash, &loc);
    if (yp_isexceptionC(result)) return result;

    // If the key is already in the hash table, then there's nothing to do
    if (ypSet_ENTRY_USED(loc)) return yp_False;

    // Otherwise, we need to add the key, which possibly doesn't involve resizing
    if (*spaceleft >= 1) {
        _ypSet_movekey(so, loc, yp_incref(key), hash);
        *spaceleft -= 1;
        return yp_True;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean. Give mutable objects a bit of room to grow. If adding growhint
    // overflows ypSet_LEN_MAX (or yp_SSIZE_T_MAX), clamp to ypSet_LEN_MAX.
    if (growhint < 0) growhint = 0;
    if (ypSet_LEN(so) > ypSet_LEN_MAX - 1) return yp_MemorySizeOverflowError;
    newlen = yp_USIZE_MATH(ypSet_LEN(so) + 1, +, growhint);
    if (newlen < 0 || newlen > ypSet_LEN_MAX) newlen = ypSet_LEN_MAX;  // addition overflowed
    result = _ypSet_resize(so, newlen);                                // invalidates loc
    if (yp_isexceptionC(result)) return result;

    _ypSet_movekey_clean(so, yp_incref(key), hash, &loc);
    *spaceleft = _ypSet_space_remaining(so);
    return yp_True;
}

// Removes the key from the hash table. The set is not resized. Returns the reference to the
// removed key if so was modified, ypSet_dummy if it wasn't due to the key not being in the
// set, or an exception on error.
static ypObject *_ypSet_pop(ypObject *so, ypObject *key)
{
    yp_hash_t       hash;
    ypObject       *result = yp_None;
    ypSet_KeyEntry *loc;

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC(key, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(so, key, hash, &loc);
    if (yp_isexceptionC(result)) return result;

    // If the key is not in the hash table, then there's nothing to do
    if (!ypSet_ENTRY_USED(loc)) return ypSet_dummy;

    // Otherwise, we need to remove the key
    return _ypSet_removekey(so, loc);  // new ref
}

// XXX Check for the so==x case _before_ calling this function
// TODO This requires that the elements of x are immutable...do we want to support mutables too?
static ypObject *_ypSet_isdisjoint(ypObject *so, ypObject *x)
{
    ypSet_KeyEntry *keys = ypSet_TABLE(so);
    yp_ssize_t      keysleft = ypSet_LEN(so);
    yp_ssize_t      i;
    ypObject       *result;
    ypSet_KeyEntry *loc;

    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&keys[i])) continue;
        keysleft -= 1;
        result = _ypSet_lookkey(x, keys[i].se_key, keys[i].se_hash, &loc);
        if (yp_isexceptionC(result)) return result;
        if (ypSet_ENTRY_USED(loc)) return yp_False;
    }
    return yp_True;
}

// XXX Check for the so==x case _before_ calling this function
// TODO This requires that the elements of x are immutable...do we want to support mutables too?
static ypObject *_ypSet_issubset(ypObject *so, ypObject *x)
{
    ypSet_KeyEntry *keys = ypSet_TABLE(so);
    yp_ssize_t      keysleft = ypSet_LEN(so);
    yp_ssize_t      i;
    ypObject       *result;
    ypSet_KeyEntry *loc;

    if (ypSet_LEN(so) > ypSet_LEN(x)) return yp_False;
    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&keys[i])) continue;
        keysleft -= 1;
        result = _ypSet_lookkey(x, keys[i].se_key, keys[i].se_hash, &loc);
        if (yp_isexceptionC(result)) return result;
        if (!ypSet_ENTRY_USED(loc)) return yp_False;
    }
    return yp_True;
}

// XXX Check for the so==other case _before_ calling this function
static ypObject *_ypSet_update_fromset(ypObject *so, ypObject *other)
{
    yp_ssize_t      keysleft = ypSet_LEN(other);
    ypSet_KeyEntry *otherkeys = ypSet_TABLE(other);
    yp_ssize_t      spaceleft = _ypSet_space_remaining(so);
    ypObject       *result;
    yp_ssize_t      i;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(other) == ypFrozenSet_CODE);
    yp_ASSERT1(so != other);

    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&otherkeys[i])) continue;
        keysleft -= 1;
        // TODO _ypSet_push recalculates hash; consolidate?
        result = _ypSet_push(so, otherkeys[i].se_key, &spaceleft, keysleft);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *_ypSet_update_fromiter(ypObject *so, ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject  *exc = yp_None;
    ypObject  *key;
    ypObject  *result;
    yp_ssize_t spaceleft = _ypSet_space_remaining(so);
    yp_ssize_t length_hint = yp_miniiter_length_hintC(mi, mi_state, &exc);  // zero on error

    while (1) {
        key = yp_miniiter_next(mi, mi_state);  // new ref
        if (yp_isexceptionC(key)) {
            if (yp_isexceptionC2(key, yp_StopIteration)) break;
            return key;
        }
        length_hint -= 1;  // check for <0 only when we need it in _ypSet_push
        result = _ypSet_push(so, key, &spaceleft, length_hint);
        yp_decref(key);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

// Adds the keys yielded from iterable to the set. If the set has enough space to hold all the
// keys, the set is not resized (important, as yp_setN et al pre-allocate the necessary space).
// Requires that iterable's items are immutable; unavoidable as they are to be added to the set.
// XXX Check for the so==iterable case _before_ calling this function
static ypObject *_ypSet_update(ypObject *so, ypObject *iterable)
{
    int         iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);
    ypObject   *mi;
    yp_uint64_t mi_state;
    ypObject   *result;

    // Recall that type pairs are identified by the immutable type code
    if (iterable_pair == ypFrozenSet_CODE) {
        return _ypSet_update_fromset(so, iterable);
    } else {
        mi = yp_miniiter(iterable, &mi_state);  // new ref
        if (yp_isexceptionC(mi)) return mi;
        result = _ypSet_update_fromiter(so, mi, &mi_state);
        yp_decref(mi);
        return result;
    }
}

// XXX Check the so==other case _before_ calling this function
static ypObject *_ypSet_intersection_update_fromset(ypObject *so, ypObject *other)
{
    yp_ssize_t      keysleft = ypSet_LEN(so);
    ypSet_KeyEntry *keys = ypSet_TABLE(so);
    yp_ssize_t      i;
    ypSet_KeyEntry *other_loc;
    ypObject       *result;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(other) == ypFrozenSet_CODE);
    yp_ASSERT1(so != other);

    // Since we're only removing keys from so, it won't be resized, so we can loop over it. We
    // break once so is empty because we aren't expecting any errors from _ypSet_lookkey.
    for (i = 0; keysleft > 0; i++) {
        if (ypSet_LEN(so) < 1) break;
        yp_ASSERT(keys == ypSet_TABLE(so) && i < ypSet_ALLOCLEN(so),
                "removing keys shouldn't resize set");
        if (!ypSet_ENTRY_USED(&keys[i])) continue;
        keysleft -= 1;
        result = _ypSet_lookkey(other, keys[i].se_key, keys[i].se_hash, &other_loc);
        if (yp_isexceptionC(result)) return result;
        if (ypSet_ENTRY_USED(other_loc)) continue;  // if entry used, key is in other
        // FIXME What if yp_decref modifies so?
        yp_decref(_ypSet_removekey(so, &keys[i]));
    }
    return yp_None;
}

// TODO This _allows_ mi to yield mutable values, unlike issubset; standardize
static ypObject *_ypSet_difference_update_fromiter(
        ypObject *so, ypObject *mi, yp_uint64_t *mi_state);
static ypObject *_ypSet_difference_update_fromset(ypObject *so, ypObject *other);
static ypObject *_ypSet_intersection_update_fromiter(
        ypObject *so, ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject *so_toremove;
    ypObject *result;

    // TODO can we do this without creating a copy or, alternatively, would it be better to
    // implement this as ypSet_intersection?
    // Unfortunately, we need to create a short-lived copy of so. It's either that, or convert
    // mi to a set, or come up with a fancy scheme to "mark" items in so to be deleted.
    so_toremove = _ypSet_copy(ypSet_CODE, so, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(so_toremove)) return so_toremove;

    // Remove items from so_toremove that are yielded by mi. so_toremove is then a set
    // containing the keys to remove from so.
    result = _ypSet_difference_update_fromiter(so_toremove, mi, mi_state);
    if (!yp_isexceptionC(result)) {
        result = _ypSet_difference_update_fromset(so, so_toremove);
    }
    yp_decref(so_toremove);
    return result;
}

// Removes the keys not yielded from iterable from the set
// XXX Check for the so==iterable case _before_ calling this function
static ypObject *_ypSet_intersection_update(ypObject *so, ypObject *iterable)
{
    int         iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);
    ypObject   *mi;
    yp_uint64_t mi_state;
    ypObject   *result;

    // Recall that type pairs are identified by the immutable type code
    if (iterable_pair == ypFrozenSet_CODE) {
        return _ypSet_intersection_update_fromset(so, iterable);
    } else {
        mi = yp_miniiter(iterable, &mi_state);  // new ref
        if (yp_isexceptionC(mi)) return mi;
        result = _ypSet_intersection_update_fromiter(so, mi, &mi_state);
        yp_decref(mi);
        return result;
    }
}

// XXX Check for the so==other case _before_ calling this function
static ypObject *_ypSet_difference_update_fromset(ypObject *so, ypObject *other)
{
    yp_ssize_t      keysleft = ypSet_LEN(other);
    ypSet_KeyEntry *otherkeys = ypSet_TABLE(other);
    ypObject       *result;
    yp_ssize_t      i;
    ypSet_KeyEntry *loc;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(other) == ypFrozenSet_CODE);
    yp_ASSERT1(so != other);

    // We break once so is empty because we aren't expecting any errors from _ypSet_lookkey
    for (i = 0; keysleft > 0; i++) {
        if (ypSet_LEN(so) < 1) break;
        if (!ypSet_ENTRY_USED(&otherkeys[i])) continue;
        keysleft -= 1;
        result = _ypSet_lookkey(so, otherkeys[i].se_key, otherkeys[i].se_hash, &loc);
        if (yp_isexceptionC(result)) return result;
        if (!ypSet_ENTRY_USED(loc)) continue;  // if entry not used, key is not in set
        yp_decref(_ypSet_removekey(so, loc));
    }
    return yp_None;
}

// TODO This _allows_ mi to yield mutable values, unlike issubset; standardize
static ypObject *_ypSet_difference_update_fromiter(
        ypObject *so, ypObject *mi, yp_uint64_t *mi_state)
{
    ypObject *result = yp_None;
    ypObject *key;

    // It's tempting to stop once so is empty, but doing so would mask errors in yielded keys
    while (1) {
        key = yp_miniiter_next(mi, mi_state);  // new ref
        if (yp_isexceptionC(key)) {
            if (yp_isexceptionC2(key, yp_StopIteration)) break;
            return key;
        }
        result = _ypSet_pop(so, key);  // new ref
        yp_decref(key);
        if (yp_isexceptionC(result)) return result;
        yp_decref(result);
    }
    return yp_None;
}

// Removes the keys yielded from iterable from the set
// XXX Check for the so==iterable case _before_ calling this function
static ypObject *_ypSet_difference_update(ypObject *so, ypObject *iterable)
{
    int         iterable_pair = ypObject_TYPE_PAIR_CODE(iterable);
    ypObject   *mi;
    yp_uint64_t mi_state;
    ypObject   *result;

    // Recall that type pairs are identified by the immutable type code
    if (iterable_pair == ypFrozenSet_CODE) {
        return _ypSet_difference_update_fromset(so, iterable);
    } else {
        mi = yp_miniiter(iterable, &mi_state);  // new ref
        if (yp_isexceptionC(mi)) return mi;
        result = _ypSet_difference_update_fromiter(so, mi, &mi_state);
        yp_decref(mi);
        return result;
    }
}

// XXX Check for the so==other case _before_ calling this function
static ypObject *_ypSet_symmetric_difference_update_fromset(ypObject *so, ypObject *other)
{
    yp_ssize_t      spaceleft = _ypSet_space_remaining(so);
    yp_ssize_t      keysleft = ypSet_LEN(other);
    ypSet_KeyEntry *otherkeys = ypSet_TABLE(other);
    ypObject       *result;
    yp_ssize_t      i;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(other) == ypFrozenSet_CODE);
    yp_ASSERT1(so != other);

    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&otherkeys[i])) continue;
        keysleft -= 1;

        // First, attempt to remove; if nothing was removed, then add it instead
        // TODO _ypSet_pop and _ypSet_push both call yp_currenthashC; consolidate?
        result = _ypSet_pop(so, otherkeys[i].se_key);
        if (result == ypSet_dummy) {
            result = _ypSet_push(so, otherkeys[i].se_key, &spaceleft, keysleft);  // may resize so
            if (yp_isexceptionC(result)) return result;
        } else if (yp_isexceptionC(result)) {
            return result;
        } else {
            // XXX spaceleft based on alloclen and fill, so doesn't change on deletions
            // FIXME What if yp_decref modifies so?
            yp_decref(result);
        }
    }
    return yp_None;
}


// Public methods

static ypObject *frozenset_traverse(ypObject *so, visitfunc visitor, void *memo)
{
    ypSet_KeyEntry *keys = ypSet_TABLE(so);
    yp_ssize_t      keysleft = ypSet_LEN(so);
    yp_ssize_t      i;
    ypObject       *result;

    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&keys[i])) continue;
        keysleft -= 1;
        result = visitor(keys[i].se_key, memo);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *frozenset_freeze(ypObject *so)
{
    return yp_None;  // no-op, currently
}

static ypObject *frozenset_unfrozen_copy(ypObject *so)
{
    return _ypSet_copy(ypSet_CODE, so, /*alloclen_fixed=*/FALSE);
}

static ypObject *frozenset_frozen_copy(ypObject *so)
{
    if (ypSet_LEN(so) < 1) return yp_frozenset_empty;
    // A shallow copy of a frozenset to a frozenset doesn't require an actual copy
    if (ypObject_TYPE_CODE(so) == ypFrozenSet_CODE) return yp_incref(so);
    return _ypSet_copy(ypFrozenSet_CODE, so, /*alloclen_fixed=*/TRUE);
}

static ypObject *frozenset_unfrozen_deepcopy(ypObject *so, visitfunc copy_visitor, void *copy_memo)
{
    // TODO Similar to the issue with dict keys, we need to use sametype_visitor, because
    // copy_visitor may be unfrozen_visitor which can't be stored in a set!
    return _ypSet_deepcopy(ypSet_CODE, so, copy_visitor, copy_memo);
}

static ypObject *frozenset_frozen_deepcopy(ypObject *so, visitfunc copy_visitor, void *copy_memo)
{
    if (ypSet_LEN(so) < 1) return yp_frozenset_empty;
    return _ypSet_deepcopy(ypFrozenSet_CODE, so, copy_visitor, copy_memo);
}

static ypObject *frozenset_bool(ypObject *so) { return ypBool_FROM_C(ypSet_LEN(so)); }

// XXX Adapted from Python's frozenset_hash
static ypObject *frozenset_currenthash(
        ypObject *so, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    yp_HashSet_state_t state;

    yp_HashSet_init(&state, ypSet_LEN(so));
    yp_HashSet_next_table(&state, ypSet_TABLE(so), ypSet_MASK(so), ypSet_FILL(so));
    *hash = yp_HashSet_fini(&state);

    // Since we never contain mutable objects, we can cache our hash
    if (!ypObject_IS_MUTABLE(so)) ypObject_CACHED_HASH(so) = *hash;
    return yp_None;
}

typedef struct {
    yp_uint32_t keysleft;
    yp_uint32_t index;
} ypSetMiState;
yp_STATIC_ASSERT(ypSet_LEN_MAX <= 0xFFFFFFFFu, len_fits_32_bits);
yp_STATIC_ASSERT(yp_sizeof(yp_uint64_t) >= yp_sizeof(ypSetMiState), ypSetMiState_fits_uint64);

static ypObject *frozenset_miniiter(ypObject *so, yp_uint64_t *_state)
{
    ypSetMiState *state = (ypSetMiState *)_state;
    state->keysleft = (yp_uint32_t)ypSet_LEN(so);
    state->index = 0;
    return yp_incref(so);
}

// XXX We need to be a little suspicious of _state...just in case the caller has changed it
static ypObject *frozenset_miniiter_next(ypObject *so, yp_uint64_t *_state)
{
    ypSetMiState   *state = (ypSetMiState *)_state;
    ypSet_KeyEntry *loc;

    // Find the next entry
    if (state->keysleft < 1) return yp_StopIteration;
    while (1) {
        if (((yp_ssize_t)state->index) >= ypSet_ALLOCLEN(so)) {
            state->keysleft = 0;
            return yp_StopIteration;
        }
        loc = &ypSet_TABLE(so)[state->index];
        state->index += 1;
        if (ypSet_ENTRY_USED(loc)) break;
    }

    // Update state and return the key
    state->keysleft -= 1;
    return yp_incref(loc->se_key);
}

static ypObject *frozenset_miniiter_length_hint(
        ypObject *so, yp_uint64_t *state, yp_ssize_t *length_hint)
{
    *length_hint = (yp_ssize_t)((ypSetMiState *)state)->keysleft;
    return yp_None;
}

static ypObject *frozenset_contains(ypObject *so, ypObject *x)
{
    yp_hash_t       hash;
    ypObject       *result = yp_None;
    ypSet_KeyEntry *loc;

    hash = yp_currenthashC(x, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(so, x, hash, &loc);
    if (yp_isexceptionC(result)) return result;
    return ypBool_FROM_C(ypSet_ENTRY_USED(loc));
}

static ypObject *frozenset_isdisjoint(ypObject *so, ypObject *x)
{
    ypObject *x_asset;
    ypObject *result;

    if (so == x) return ypBool_FROM_C(ypSet_LEN(so) < 1);
    if (ypObject_TYPE_PAIR_CODE(x) == ypFrozenSet_CODE) {
        return _ypSet_isdisjoint(so, x);
    } else {
        // Otherwise, we need to convert x to a set to quickly test if it contains all items
        // FIXME We can make a version of _ypSet_isdisjoint that doesn't reqire a new set! Just
        // iterate over x looking for
        x_asset = yp_frozenset(x);
        if (yp_isexceptionC(x_asset)) return x_asset;
        result = _ypSet_isdisjoint(so, x_asset);
        yp_decref(x_asset);
        return result;
    }
}

static ypObject *frozenset_issubset(ypObject *so, ypObject *x)
{
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);
    ypObject *x_asset;
    ypObject *result;

    // We can take some shortcuts if x is a set or a dict
    if (so == x) return yp_True;
    if (x_pair == ypFrozenSet_CODE) {
        return _ypSet_issubset(so, x);
    } else if (x_pair == ypFrozenDict_CODE) {
        if (ypSet_LEN(so) > ypDict_LEN(x)) return yp_False;
    }

    // Otherwise, we need to convert x to a set to quickly test if it contains all items
    // TODO Can we make a version of _ypSet_issubset that doesn't reqire a new set created?
    x_asset = yp_frozenset(x);
    if (yp_isexceptionC(x_asset)) return x_asset;
    result = _ypSet_issubset(so, x_asset);
    yp_decref(x_asset);
    return result;
}

// Remember that if x.issubset(so), then so.issuperset(x)
static ypObject *frozenset_issuperset(ypObject *so, ypObject *x)
{
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);
    ypObject *x_asset;
    ypObject *result;

    // We can take some shortcuts if x is a set or a dict
    if (so == x) return yp_True;
    if (x_pair == ypFrozenSet_CODE) {
        return _ypSet_issubset(x, so);
    } else if (x_pair == ypFrozenDict_CODE) {
        if (ypDict_LEN(x) > ypSet_LEN(so)) return yp_False;
    }

    // Otherwise, we need to convert x to a set to quickly test if it contains all items
    // TODO Can we make a version of _ypSet_issubset that doesn't reqire a new set created?
    x_asset = yp_frozenset(x);
    if (yp_isexceptionC(x_asset)) return x_asset;
    result = _ypSet_issubset(x_asset, so);
    yp_decref(x_asset);
    return result;
}

static ypObject *frozenset_lt(ypObject *so, ypObject *x)
{
    if (so == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypFrozenSet_CODE) return yp_ComparisonNotImplemented;
    if (ypSet_LEN(so) >= ypSet_LEN(x)) return yp_False;
    return _ypSet_issubset(so, x);
}

static ypObject *frozenset_le(ypObject *so, ypObject *x)
{
    if (so == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypFrozenSet_CODE) return yp_ComparisonNotImplemented;
    return _ypSet_issubset(so, x);
}

// TODO comparison functions can recurse, just like currenthash...fix!
static ypObject *frozenset_eq(ypObject *so, ypObject *x)
{
    if (so == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypFrozenSet_CODE) return yp_ComparisonNotImplemented;
    if (ypSet_LEN(so) != ypSet_LEN(x)) return yp_False;

    // We need to inspect all our items for equality, which could be time-intensive. It's fairly
    // obvious that the pre-computed hash, if available, can save us some time when so!=x.
    if (ypObject_CACHED_HASH(so) != ypObject_HASH_INVALID &&
            ypObject_CACHED_HASH(x) != ypObject_HASH_INVALID &&
            ypObject_CACHED_HASH(so) != ypObject_CACHED_HASH(x)) {
        return yp_False;
    }

    return _ypSet_issubset(so, x);
}

static ypObject *frozenset_ne(ypObject *so, ypObject *x)
{
    ypObject *result = frozenset_eq(so, x);
    return ypBool_NOT(result);
}

static ypObject *frozenset_ge(ypObject *so, ypObject *x)
{
    if (so == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypFrozenSet_CODE) return yp_ComparisonNotImplemented;
    return _ypSet_issubset(x, so);
}

static ypObject *frozenset_gt(ypObject *so, ypObject *x)
{
    if (so == x) return yp_False;
    if (ypObject_TYPE_PAIR_CODE(x) != ypFrozenSet_CODE) return yp_ComparisonNotImplemented;
    if (ypSet_LEN(so) <= ypSet_LEN(x)) return yp_False;
    return _ypSet_issubset(x, so);
}

static ypObject *set_update(ypObject *so, int n, va_list args)
{
    ypObject *result;
    for (/*n already set*/; n > 0; n--) {
        ypObject *x = va_arg(args, ypObject *);  // borrowed
        if (so == x) continue;
        result = _ypSet_update(so, x);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *set_intersection_update(ypObject *so, int n, va_list args)
{
    ypObject *result;
    // It's tempting to stop once so is empty, but doing so would mask errors in args
    for (/*n already set*/; n > 0; n--) {
        ypObject *x = va_arg(args, ypObject *);  // borrowed
        if (so == x) continue;
        result = _ypSet_intersection_update(so, x);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *set_clear(ypObject *so);
static ypObject *set_difference_update(ypObject *so, int n, va_list args)
{
    ypObject *result;
    // It's tempting to stop once so is empty, but doing so would mask errors in args
    for (/*n already set*/; n > 0; n--) {
        ypObject *x = va_arg(args, ypObject *);  // borrowed
        if (so == x) {
            result = set_clear(so);
        } else {
            result = _ypSet_difference_update(so, x);
        }
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *set_symmetric_difference_update(ypObject *so, ypObject *x)
{
    int       x_pair = ypObject_TYPE_PAIR_CODE(x);
    ypObject *result;

    if (so == x) return set_clear(so);

    // Recall that type pairs are identified by the immutable type code
    if (x_pair == ypFrozenSet_CODE) {
        return _ypSet_symmetric_difference_update_fromset(so, x);
    } else {
        // TODO Can we make a version of _ypSet_symmetric_difference_update_fromset that doesn't
        // reqire a new set created?
        ypObject *x_asset = yp_frozenset(x);
        if (yp_isexceptionC(x_asset)) return x_asset;
        result = _ypSet_symmetric_difference_update_fromset(so, x_asset);
        yp_decref(x_asset);
        return result;
    }
}

// XXX We redirect the new-object set methods to the in-place versions. Among other things, this
// helps to avoid duplicating code.
// TODO ...except we are creating objects that we destroy then create new ones, which can probably
// be optimized in certain cases, so rethink these four methods. At the very least, can we avoid
// the yp_freeze?
static ypObject *frozenset_union(ypObject *so, int n, va_list args)
{
    ypObject *exc = yp_None;
    ypObject *result;
    ypObject *newSo;

    if (!ypObject_IS_MUTABLE(so) && n < 1) return yp_incref(so);

    newSo = _ypSet_copy(ypSet_CODE, so, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(newSo)) return newSo;
    result = set_update(newSo, n, args);
    if (yp_isexceptionC(result)) {
        yp_decref(newSo);
        return result;
    }

    if (!ypObject_IS_MUTABLE(so)) yp_freeze(newSo, &exc);
    if (yp_isexceptionC(exc)) {
        yp_decref(newSo);
        return exc;
    }
    return newSo;
}

static ypObject *frozenset_intersection(ypObject *so, int n, va_list args)
{
    ypObject *exc = yp_None;
    ypObject *result;
    ypObject *newSo;

    if (!ypObject_IS_MUTABLE(so) && n < 1) return yp_incref(so);

    newSo = _ypSet_copy(ypSet_CODE, so, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(newSo)) return newSo;
    result = set_intersection_update(newSo, n, args);
    if (yp_isexceptionC(result)) {
        yp_decref(newSo);
        return result;
    }

    if (!ypObject_IS_MUTABLE(so)) yp_freeze(newSo, &exc);
    if (yp_isexceptionC(exc)) {
        yp_decref(newSo);
        return exc;
    }
    return newSo;
}

static ypObject *frozenset_difference(ypObject *so, int n, va_list args)
{
    ypObject *exc = yp_None;
    ypObject *result;
    ypObject *newSo;

    if (!ypObject_IS_MUTABLE(so) && n < 1) return yp_incref(so);

    newSo = _ypSet_copy(ypSet_CODE, so, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(newSo)) return newSo;
    result = set_difference_update(newSo, n, args);
    if (yp_isexceptionC(result)) {
        yp_decref(newSo);
        return result;
    }

    if (!ypObject_IS_MUTABLE(so)) yp_freeze(newSo, &exc);
    if (yp_isexceptionC(exc)) {
        yp_decref(newSo);
        return exc;
    }
    return newSo;
}

static ypObject *frozenset_symmetric_difference(ypObject *so, ypObject *x)
{
    ypObject *exc = yp_None;
    ypObject *result;
    ypObject *newSo;

    newSo = _ypSet_copy(ypSet_CODE, so, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(newSo)) return newSo;
    result = set_symmetric_difference_update(newSo, x);
    if (yp_isexceptionC(result)) {
        yp_decref(newSo);
        return result;
    }

    if (!ypObject_IS_MUTABLE(so)) yp_freeze(newSo, &exc);
    if (yp_isexceptionC(exc)) {
        yp_decref(newSo);
        return exc;
    }
    return newSo;
}

static ypObject *set_pushunique(ypObject *so, ypObject *x)
{
    yp_ssize_t spaceleft = _ypSet_space_remaining(so);
    // TODO Overallocate
    ypObject *result = _ypSet_push(so, x, &spaceleft, 0);
    if (yp_isexceptionC(result)) return result;  // TODO: As usual, what if this is yp_KeyError?
    return result == yp_True ? yp_None : yp_KeyError;
}

static ypObject *set_push(ypObject *so, ypObject *x)
{
    yp_ssize_t spaceleft = _ypSet_space_remaining(so);
    // TODO Overallocate
    ypObject *result = _ypSet_push(so, x, &spaceleft, 0);
    if (yp_isexceptionC(result)) return result;
    return yp_None;
}

static ypObject *set_clear(ypObject *so)
{
    ypSet_KeyEntry *oldkeys = ypSet_TABLE(so);
    yp_ssize_t      keysleft = ypSet_LEN(so);
    yp_ssize_t      i;

    if (ypSet_FILL(so) < 1) return yp_None;

    // Discard the old keys
    // FIXME What if yp_decref modifies so?
    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&oldkeys[i])) continue;
        keysleft -= 1;
        yp_decref(oldkeys[i].se_key);
    }

    // Free memory
    ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR(so, ypSetObject, ypSet_ALLOCLEN_MAX);
    yp_ASSERT(ypSet_TABLE(so) == ypSet_INLINE_DATA(so), "set_clear didn't allocate inline!");
    yp_ASSERT(ypSet_ALLOCLEN(so) >= ypSet_ALLOCLEN_MIN,
            "set inlinelen must be at least ypSet_ALLOCLEN_MIN");

    // Update our attributes and return
    // XXX alloclen must be a power of 2; it's unlikely we'd be given double the requested memory
    ypSet_SET_ALLOCLEN(so, ypSet_ALLOCLEN_MIN);  // we can't make use of the excess anyway
    ypSet_SET_LEN(so, 0);
    ypSet_FILL(so) = 0;
    yp_memset(ypSet_TABLE(so), 0, ypSet_ALLOCLEN_MIN * yp_sizeof(ypSet_KeyEntry));
    return yp_None;
}

// Note the difference between this, which removes an arbitrary key, and _ypSet_pop, which removes
// a specific key
// XXX Adapted from Python's set_pop
static ypObject *set_pop(ypObject *so)
{
    register yp_ssize_t      i = 0;
    register ypSet_KeyEntry *table = ypSet_TABLE(so);
    ypObject                *key;

    if (ypSet_LEN(so) < 1) return yp_KeyError;  // "pop from an empty set"

    /* We abuse the hash field of slot 0 to hold a search finger:
     * If slot 0 has a value, use slot 0.
     * Else slot 0 is being used to hold a search finger,
     * and we use its hash value as the first index to look.
     */
    if (!ypSet_ENTRY_USED(table)) {
        i = table->se_hash;
        /* The hash field may be a real hash value, or it may be a
         * legit search finger, or it may be a once-legit search
         * finger that's out of bounds now because it wrapped around
         * or the table shrunk -- simply make sure it's in bounds now.
         */
        if (i > ypSet_MASK(so) || i < 1) i = 1;  // skip slot 0
        while (!ypSet_ENTRY_USED(table + i)) {
            i++;
            if (i > ypSet_MASK(so)) i = 1;
        }
    }
    key = _ypSet_removekey(so, table + i);
    table->se_hash = i + 1;  // next place to start
    return key;
}

static ypObject *frozenset_len(ypObject *so, yp_ssize_t *len)
{
    *len = ypSet_LEN(so);
    return yp_None;
}

// onmissing must be an immortal, or NULL
static ypObject *set_remove(ypObject *so, ypObject *x, ypObject *onmissing)
{
    ypObject *result = _ypSet_pop(so, x);
    if (result == ypSet_dummy) {
        if (onmissing == NULL) return yp_KeyError;
        return onmissing;
    }
    if (yp_isexceptionC(result)) return result;
    yp_decref(result);
    return yp_None;
}

static ypObject *frozenset_dealloc(ypObject *so, void *memo)
{
    ypSet_KeyEntry *keys = ypSet_TABLE(so);
    yp_ssize_t      keysleft = ypSet_LEN(so);
    yp_ssize_t      i;

    for (i = 0; keysleft > 0; i++) {
        if (!ypSet_ENTRY_USED(&keys[i])) continue;
        keysleft -= 1;
        yp_decref_fromdealloc(keys[i].se_key, memo);
    }
    ypMem_FREE_CONTAINER(so, ypSetObject);
    return yp_None;
}

static ypObject *frozenset_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_frozenset);
    return yp_frozenset(argarray[1]);
}

static ypObject *set_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 2, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_set);
    return yp_set(argarray[1]);
}

#define _ypFrozenSet_FUNC_NEW_PARAMETERS                                     \
    ({yp_CONST_REF(yp_s_cls), NULL},                                         \
            {yp_CONST_REF(yp_s_iterable), yp_CONST_REF(yp_frozenset_empty)}, \
            {yp_CONST_REF(yp_s_slash), NULL})
yp_IMMORTAL_FUNCTION_static(
        frozenset_func_new, frozenset_func_new_code, _ypFrozenSet_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(set_func_new, set_func_new_code, _ypFrozenSet_FUNC_NEW_PARAMETERS);

static ypSetMethods ypFrozenSet_as_set = {
        frozenset_isdisjoint,  // tp_isdisjoint
        frozenset_issubset,    // tp_issubset
        // tp_lt is elsewhere
        frozenset_issuperset,  // tp_issuperset
        // tp_gt is elsewhere
        frozenset_union,                 // tp_union
        frozenset_intersection,          // tp_intersection
        frozenset_difference,            // tp_difference
        frozenset_symmetric_difference,  // tp_symmetric_difference
        MethodError_objvalistproc,       // tp_intersection_update
        MethodError_objvalistproc,       // tp_difference_update
        MethodError_objobjproc,          // tp_symmetric_difference_update
        // tp_push (aka tp_set_ad) is elsewhere
        MethodError_objobjproc  // tp_pushunique
};

static ypTypeObject ypFrozenSet_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(frozenset_func_new),  // tp_func_new
        frozenset_dealloc,                 // tp_dealloc
        frozenset_traverse,                // tp_traverse
        NULL,                              // tp_str
        NULL,                              // tp_repr

        // Freezing, copying, and invalidating
        frozenset_freeze,             // tp_freeze
        frozenset_unfrozen_copy,      // tp_unfrozen_copy
        frozenset_frozen_copy,        // tp_frozen_copy
        frozenset_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        frozenset_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,          // tp_invalidate

        // Boolean operations and comparisons
        frozenset_bool,  // tp_bool
        frozenset_lt,    // tp_lt
        frozenset_le,    // tp_le
        frozenset_eq,    // tp_eq
        frozenset_ne,    // tp_ne
        frozenset_ge,    // tp_ge
        frozenset_gt,    // tp_gt

        // Generic object operations
        frozenset_currenthash,  // tp_currenthash
        MethodError_objproc,    // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        frozenset_miniiter,              // tp_miniiter
        TypeError_miniiterfunc,          // tp_miniiter_reversed
        frozenset_miniiter_next,         // tp_miniiter_next
        frozenset_miniiter_length_hint,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,            // tp_iter
        TypeError_objproc,               // tp_iter_reversed
        TypeError_objobjproc,            // tp_send

        // Container operations
        frozenset_contains,         // tp_contains
        frozenset_len,              // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        &ypFrozenSet_as_set,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypSetMethods ypSet_as_set = {
        frozenset_isdisjoint,  // tp_isdisjoint
        frozenset_issubset,    // tp_issubset
        // tp_lt is elsewhere
        frozenset_issuperset,  // tp_issuperset
        // tp_gt is elsewhere
        frozenset_union,                  // tp_union
        frozenset_intersection,           // tp_intersection
        frozenset_difference,             // tp_difference
        frozenset_symmetric_difference,   // tp_symmetric_difference
        set_intersection_update,          // tp_intersection_update
        set_difference_update,            // tp_difference_update
        set_symmetric_difference_update,  // tp_symmetric_difference_update
        // tp_push (aka tp_set_ad) is elsewhere
        set_pushunique,  // tp_pushunique
};

static ypTypeObject ypSet_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(set_func_new),  // tp_func_new
        frozenset_dealloc,           // tp_dealloc
        frozenset_traverse,          // tp_traverse
        NULL,                        // tp_str
        NULL,                        // tp_repr

        // Freezing, copying, and invalidating
        frozenset_freeze,             // tp_freeze
        frozenset_unfrozen_copy,      // tp_unfrozen_copy
        frozenset_frozen_copy,        // tp_frozen_copy
        frozenset_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        frozenset_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,          // tp_invalidate

        // Boolean operations and comparisons
        frozenset_bool,  // tp_bool
        frozenset_lt,    // tp_lt
        frozenset_le,    // tp_le
        frozenset_eq,    // tp_eq
        frozenset_ne,    // tp_ne
        frozenset_ge,    // tp_ge
        frozenset_gt,    // tp_gt

        // Generic object operations
        frozenset_currenthash,  // tp_currenthash
        MethodError_objproc,    // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        frozenset_miniiter,              // tp_miniiter
        TypeError_miniiterfunc,          // tp_miniiter_reversed
        frozenset_miniiter_next,         // tp_miniiter_next
        frozenset_miniiter_length_hint,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,            // tp_iter
        TypeError_objproc,               // tp_iter_reversed
        TypeError_objobjproc,            // tp_send

        // Container operations
        frozenset_contains,         // tp_contains
        frozenset_len,              // tp_len
        set_push,                   // tp_push
        set_clear,                  // tp_clear
        set_pop,                    // tp_pop
        set_remove,                 // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        set_update,                 // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        &ypSet_as_set,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};


// Public functions

void yp_set_add(ypObject *set, ypObject *x, ypObject **exc)
{
    ypObject *result;
    if (ypObject_TYPE_CODE(set) != ypSet_CODE) return_yp_EXC_BAD_TYPE(exc, set);
    result = set_push(set, x);
    if (yp_isexceptionC(result)) return_yp_EXC_ERR(exc, result);
}

// TODO Calling it yp_set_* implies it only works for sets, so do we need a yp_frozenset_*?  If we
// do, we're dooming people to check the type of the object to find out which function they can
// use...but then what else should we call this?  Do we jump right to yp_getintern?
static ypObject *yp_set_getintern(ypObject *set, ypObject *x)
{
    yp_hash_t       hash;
    ypObject       *result = yp_None;
    ypSet_KeyEntry *loc;

    hash = yp_currenthashC(x, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(set, x, hash, &loc);
    if (yp_isexceptionC(result)) return result;
    if (!ypSet_ENTRY_USED(loc)) return yp_KeyError;
    return yp_incref(loc->se_key);
}

// Constructors

// TODO using ypQuickIter here could merge with _ypSet, removing one of its incref/decrefs
static ypObject *_ypSetNV(int type, int n, va_list args)
{
    yp_ssize_t spaceleft;
    ypObject  *result;
    ypObject  *newSo = _ypSet_new(type, n, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newSo)) return newSo;
    spaceleft = _ypSet_space_remaining(newSo);
    while (n > 0) {
        ypObject *x = va_arg(args, ypObject *);  // borrowed
        if (yp_isexceptionC(x)) {
            yp_decref(newSo);
            return x;
        }
        n -= 1;
        result = _ypSet_push(newSo, x, &spaceleft, n);
        if (yp_isexceptionC(result)) {
            yp_decref(newSo);
            return result;
        }
    }
    return newSo;
}

ypObject *yp_frozensetN(int n, ...)
{
    if (n < 1) return yp_frozenset_empty;
    return_yp_V_FUNC(ypObject *, _ypSetNV, (ypFrozenSet_CODE, n, args), n);
}
ypObject *yp_frozensetNV(int n, va_list args)
{
    if (n < 1) return yp_frozenset_empty;
    return _ypSetNV(ypFrozenSet_CODE, n, args);
}

ypObject *yp_setN(int n, ...)
{
    if (n < 1) return _ypSet_new(ypSet_CODE, 0, /*alloclen_fixed=*/FALSE);
    return_yp_V_FUNC(ypObject *, _ypSetNV, (ypSet_CODE, n, args), n);
}
ypObject *yp_setNV(int n, va_list args)
{
    if (n < 1) return _ypSet_new(ypSet_CODE, 0, /*alloclen_fixed=*/FALSE);
    return _ypSetNV(ypSet_CODE, n, args);
}

// XXX Check for the "fellow frozenset/set" case _before_ calling this function
static ypObject *_ypSet(int type, ypObject *iterable)
{
    ypObject  *exc = yp_None;
    ypObject  *newSo;
    ypObject  *result;
    yp_ssize_t length_hint = yp_lenC(iterable, &exc);
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint; it just means we can't pre-allocate
        length_hint = yp_length_hintC(iterable, &exc);
        if (length_hint > ypSet_LEN_MAX) length_hint = ypSet_LEN_MAX;
    } else if (length_hint < 1) {
        // yp_lenC reports an empty iterable, so we can shortcut _ypSet_update
        if (type == ypFrozenSet_CODE) return yp_frozenset_empty;
        return _ypSet_new(ypSet_CODE, 0, /*alloclen_fixed=*/FALSE);
    } else if (length_hint > ypSet_LEN_MAX) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    newSo = _ypSet_new(type, length_hint, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newSo)) return newSo;
    result = _ypSet_update(newSo, iterable);
    if (yp_isexceptionC(result)) {
        yp_decref(newSo);
        return result;
    }

    // TODO We could avoid allocating for an empty iterable altogether if we get the first value
    // before allocating; is this complication worth the optimization?
    if (type == ypFrozenSet_CODE && ypSet_LEN(newSo) < 1) {
        yp_decref(newSo);
        return yp_frozenset_empty;
    }

    return newSo;
}

ypObject *yp_frozenset(ypObject *iterable)
{
    if (ypObject_TYPE_PAIR_CODE(iterable) == ypFrozenSet_CODE) {
        if (ypSet_LEN(iterable) < 1) return yp_frozenset_empty;
        if (ypObject_TYPE_CODE(iterable) == ypFrozenSet_CODE) return yp_incref(iterable);
        return _ypSet_copy(ypFrozenSet_CODE, iterable, /*alloclen_fixed=*/TRUE);
    }
    return _ypSet(ypFrozenSet_CODE, iterable);
}

ypObject *yp_set(ypObject *iterable)
{
    if (ypObject_TYPE_PAIR_CODE(iterable) == ypFrozenSet_CODE) {
        return _ypSet_copy(ypSet_CODE, iterable, /*alloclen_fixed=*/FALSE);
    }
    return _ypSet(ypSet_CODE, iterable);
}

#pragma endregion set


/*************************************************************************************************
 * Mappings
 *************************************************************************************************/
#pragma region dict

// XXX Much of this set/dict implementation is pulled right from Python, so best to read the
// original source for documentation on this implementation

// XXX keyset requires care!  It is potentially shared among multiple dicts, so we cannot remove
// keys or resize it. It identifies itself as a frozendict, yet we add keys to it, so it is not
// truly immutable. As such, it cannot be exposed outside of the set/dict implementations. On the
// plus side, we can allocate it's data inline (via alloclen_fixed).

// ypDictObject and ypDict_LEN are defined above, for use by the set code
#define ypDict_KEYSET(mp) (((ypDictObject *)mp)->keyset)
#define ypDict_ALLOCLEN(mp) ypSet_ALLOCLEN(ypDict_KEYSET(mp))
#define ypDict_VALUES(mp) ((ypObject **)((ypObject *)mp)->ob_data)
#define ypDict_SET_VALUES(mp, x) (((ypObject *)mp)->ob_data = x)
#define ypDict_INLINE_DATA(mp) (((ypDictObject *)mp)->ob_inline_data)

// XXX The dict's alloclen is always equal to the keyset alloclen, and we don't use the standard
// ypMem_* macros to resize the dict, so we abuse mp->ob_alloclen to be the search finger for
// dict_popitem; if it gets corrupted, it's no big deal, because it's just a place to start
// searching
#define ypDict_POPITEM_FINGER ypObject_ALLOCLEN
#define ypDict_SET_POPITEM_FINGER ypObject_SET_ALLOCLEN

// dicts cannot grow larger than the associated keyset
#define ypDict_ALLOCLEN_MAX ypSet_ALLOCLEN_MAX
#define ypDict_LEN_MAX ypSet_LEN_MAX

// Returns a pointer to the value element corresponding to the given key location
#define ypDict_VALUE_ENTRY(mp, key_loc) \
    (&(ypDict_VALUES(mp)[ypSet_ENTRY_INDEX(ypDict_KEYSET(mp), key_loc)]))

// So long as each entry in the dict's value array is no larger than each entry in the set's
// key/hash array, we can share ypSet_ALLOCLEN_MAX and ypSet_LEN_MAX without fear of overflow
yp_STATIC_ASSERT(
        yp_sizeof(ypObject *) <= yp_sizeof(ypSet_KeyEntry), ypDict_data_not_larger_than_set_data);
yp_STATIC_ASSERT(
        (yp_SSIZE_T_MAX - yp_sizeof(ypDictObject)) / yp_sizeof(ypObject *) >= ypSet_ALLOCLEN_MAX,
        ypDict_alloclen_max_not_smaller_than_set_alloclen_max);

// Returns a new, empty dict or frozendict object to hold minused entries
// XXX Check for the "yp_frozendict_empty" case first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Overallocate to avoid future resizings
static ypObject *_ypDict_new(int type, yp_ssize_t minused, int alloclen_fixed)
{
    ypObject  *keyset;
    yp_ssize_t alloclen;
    ypObject  *mp;

    // We always allocate our keyset's data INLINE
    keyset = _ypSet_new(ypFrozenSet_CODE, minused, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(keyset)) return keyset;
    alloclen = ypSet_ALLOCLEN(keyset);
    if (alloclen_fixed && type == ypFrozenDict_CODE) {
        yp_ASSERT(minused > 0, "missed a yp_frozendict_empty optimization");
        mp = ypMem_MALLOC_CONTAINER_INLINE(
                ypDictObject, ypFrozenDict_CODE, alloclen, ypDict_ALLOCLEN_MAX);
    } else {
        mp = ypMem_MALLOC_CONTAINER_VARIABLE(ypDictObject, type, alloclen, 0, ypDict_ALLOCLEN_MAX);
    }
    if (yp_isexceptionC(mp)) {
        yp_decref(keyset);
        return mp;
    }
    ypDict_KEYSET(mp) = keyset;
    yp_memset(ypDict_VALUES(mp), 0, alloclen * yp_sizeof(ypObject *));
    return mp;
}

// If we are performing a shallow copy, we can share keysets and quickly memcpy the values
// XXX Check for the "lazy shallow copy" and "empty copy" cases first
// TODO Is there a point where the original is so dirty that we'd be better spinning a new keyset?
// TODO All these *_copy methods can also copy the hash over, if type is immutable.
static ypObject *_ypDict_copy(int type, ypObject *x, int alloclen_fixed)
{
    ypObject  *keyset;
    yp_ssize_t alloclen;
    ypObject  *mp;
    ypObject **values;
    yp_ssize_t valuesleft;
    yp_ssize_t i;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(x) == ypFrozenDict_CODE);
    yp_ASSERT(ypDict_LEN(x) > 0, "missed an 'empty copy' optimization");

    // Share the keyset object with our fellow dict
    keyset = ypDict_KEYSET(x);
    alloclen = ypSet_ALLOCLEN(keyset);
    if (alloclen_fixed && type == ypFrozenDict_CODE) {
        mp = ypMem_MALLOC_CONTAINER_INLINE(
                ypDictObject, ypFrozenDict_CODE, alloclen, ypDict_ALLOCLEN_MAX);
    } else {
        mp = ypMem_MALLOC_CONTAINER_VARIABLE(ypDictObject, type, alloclen, 0, ypDict_ALLOCLEN_MAX);
    }
    if (yp_isexceptionC(mp)) return mp;
    ypDict_KEYSET(mp) = yp_incref(keyset);

    // Now copy over the values; since we share keysets, the values all line up in the same place
    valuesleft = ypDict_LEN(x);
    ypDict_SET_LEN(mp, valuesleft);
    values = ypDict_VALUES(mp);
    yp_memcpy(values, ypDict_VALUES(x), alloclen * yp_sizeof(ypObject *));
    for (i = 0; valuesleft > 0; i++) {
        if (values[i] == NULL) continue;
        valuesleft -= 1;
        yp_incref(values[i]);
    }
    return mp;
}

// XXX Check for the yp_frozendict_empty case first
// TODO If x contains quite a lot of waste vis-a-vis unused keys from the keyset, then consider
// either a) optimizing x first, or b) not sharing the keyset of this object
static ypObject *_ypDict_deepcopy(int type, ypObject *x, visitfunc copy_visitor, void *copy_memo)
{
    // TODO We can't use copy_visitor to copy the keys, because it might be
    // _yp_unfrozen_deepcopy_visitor!
    // ...or that just means we need to fail if yp_hash fails on the new key.
    return yp_NotImplementedError;
}

// The tricky bit about resizing dicts is that we need both the old and new keysets and value
// arrays to properly transfer the data; in-place resizes are impossible.
// TODO Do we want to split minused into required and extra, like in other areas?
yp_STATIC_ASSERT(
        ypSet_ALLOCLEN_MAX <= yp_SSIZE_T_MAX / yp_sizeof(ypObject *), ypDict_resize_cant_overflow);
static ypObject *_ypDict_resize(ypObject *mp, yp_ssize_t minused)
{
    ypObject       *newkeyset;
    yp_ssize_t      newalloclen;
    ypSet_KeyEntry *oldkeys;
    ypObject      **oldvalues;
    yp_ssize_t      valuesleft;
    yp_ssize_t      i;
    ypObject       *value;
    ypSet_KeyEntry *newkey_loc;

    yp_ASSERT1(mp != yp_frozendict_empty);  // don't modify the empty frozendict!

    // Always allocate a separate buffer. Remember that mp->ob_alloclen has been repurposed to hold
    // a search finger.
    newkeyset = _ypSet_new(ypFrozenSet_CODE, minused, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newkeyset)) return newkeyset;
    newalloclen = ypSet_ALLOCLEN(newkeyset);
    // XXX ypDict_resize_cant_overflow ensures this can't overflow
    oldvalues = ypMem_REALLOC_CONTAINER_VARIABLE_NEW(
            mp, ypDictObject, newalloclen, 0, ypDict_ALLOCLEN_MAX);
    if (oldvalues == NULL) {
        yp_decref(newkeyset);
        return yp_MemoryError;
    }
    yp_memset(ypDict_VALUES(mp), 0, newalloclen * yp_sizeof(ypObject *));

    // Move the keys and values from the old tables.
    oldkeys = ypSet_TABLE(ypDict_KEYSET(mp));
    valuesleft = ypDict_LEN(mp);
    for (i = 0; valuesleft > 0; i++) {
        value = oldvalues[i];
        if (value == NULL) continue;
        valuesleft -= 1;
        _ypSet_movekey_clean(
                newkeyset, yp_incref(oldkeys[i].se_key), oldkeys[i].se_hash, &newkey_loc);
        ypDict_VALUES(mp)[ypSet_ENTRY_INDEX(newkeyset, newkey_loc)] = oldvalues[i];
    }

    // Free the old tables.
    // FIXME What if yp_decref modifies mp?
    yp_decref(ypDict_KEYSET(mp));
    ypDict_KEYSET(mp) = newkeyset;
    ypMem_REALLOC_CONTAINER_FREE_OLDPTR(mp, ypDictObject, oldvalues);
    yp_DEBUG("_ypDict_resize: %p table %p  (was %p)", mp, ypDict_VALUES(mp), oldvalues);
    return yp_None;
}

// Adds a new key with the given hash at the given key_loc, which may require a resize, and sets
// value appropriately. *key_loc must point to a currently-unused location in the hash table; it
// will be updated if a resize occurs. Otherwise behaves as _ypDict_push.
// XXX Adapted from PyDict_SetItem
// TODO The decision to resize currently depends only on _ypSet_space_remaining, but what if the
// shared keyset contains 5x the keys that we actually use?  That's a large waste in the value
// table. Really, we should have a _ypDict_space_remaining.
static ypObject *_ypDict_push_newkey(ypObject *mp, ypSet_KeyEntry **key_loc, ypObject *key,
        yp_hash_t hash, ypObject *value, yp_ssize_t *spaceleft, yp_ssize_t growhint)
{
    ypObject  *keyset = ypDict_KEYSET(mp);
    ypObject  *result;
    yp_ssize_t newlen;

    yp_ASSERT1(mp != yp_frozendict_empty);  // don't modify the empty frozendict!

    // It's possible we can add the key without resizing
    if (*spaceleft >= 1) {
        _ypSet_movekey(keyset, *key_loc, yp_incref(key), hash);
        *ypDict_VALUE_ENTRY(mp, *key_loc) = yp_incref(value);
        ypDict_SET_LEN(mp, ypDict_LEN(mp) + 1);
        *spaceleft -= 1;
        return yp_True;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean. Give mutable objects a bit of room to grow. If adding growhint
    // overflows ypSet_LEN_MAX (or yp_SSIZE_T_MAX), clamp to ypSet_LEN_MAX.
    if (growhint < 0) growhint = 0;
    if (ypDict_LEN(mp) > ypSet_LEN_MAX - 1) return yp_MemorySizeOverflowError;
    newlen = yp_USIZE_MATH(ypDict_LEN(mp) + 1, +, growhint);
    if (newlen < 0 || newlen > ypSet_LEN_MAX) newlen = ypSet_LEN_MAX;  // addition overflowed
    result = _ypDict_resize(mp, newlen);  // invalidates keyset and *key_loc
    if (yp_isexceptionC(result)) return result;

    keyset = ypDict_KEYSET(mp);
    _ypSet_movekey_clean(keyset, yp_incref(key), hash, key_loc);
    *ypDict_VALUE_ENTRY(mp, *key_loc) = yp_incref(value);
    ypDict_SET_LEN(mp, ypDict_LEN(mp) + 1);
    *spaceleft = _ypSet_space_remaining(keyset);
    return yp_True;
}

// Adds the key/value to the dict. If override is false, returns yp_False and does not modify the
// dict if there is an existing value. *spaceleft should be initialized from
// _ypSet_space_remaining; this function then decrements it with each key added, and resets it on
// every resize. Returns yp_True if mp was modified, yp_False if it wasn't due to existing values
// being preserved (ie override is false), or an exception on error.
// XXX Adapted from PyDict_SetItem
static ypObject *_ypDict_push(ypObject *mp, ypObject *key, ypObject *value, int override,
        yp_ssize_t *spaceleft, yp_ssize_t growhint)
{
    yp_hash_t       hash;
    ypObject       *keyset = ypDict_KEYSET(mp);
    ypSet_KeyEntry *key_loc;
    ypObject       *result = yp_None;
    ypObject      **value_loc;

    yp_ASSERT1(mp != yp_frozendict_empty);  // don't modify the empty frozendict!

    // Look for the appropriate entry in the hash table
    hash = yp_hashC(key, &result);
    if (yp_isexceptionC(result)) return result;  // also verifies key is not an exception
    if (yp_isexceptionC(value)) return value;    // verifies value is not an exception
    result = _ypSet_lookkey(keyset, key, hash, &key_loc);
    if (yp_isexceptionC(result)) return result;

    // If the key is already in the hash table, then we simply need to update the value
    if (ypSet_ENTRY_USED(key_loc)) {
        value_loc = ypDict_VALUE_ENTRY(mp, key_loc);
        if (*value_loc == NULL) {
            *value_loc = yp_incref(value);
            ypDict_SET_LEN(mp, ypDict_LEN(mp) + 1);
        } else {
            if (!override) return yp_False;
            // FIXME What if yp_decref modifies mp?
            yp_decref(*value_loc);
            *value_loc = yp_incref(value);
        }
        return yp_True;
    }

    // Otherwise, we need to add both the key _and_ value, which may involve resizing
    return _ypDict_push_newkey(mp, &key_loc, key, hash, value, spaceleft, growhint);
}

// Removes the value from the dict; the key stays in the keyset, but that's of no concern. The
// dict is not resized. Returns the reference to the removed value if mp was modified, ypSet_dummy
// if it wasn't due to the value not being set, or an exception on error.
static ypObject *_ypDict_pop(ypObject *mp, ypObject *key)
{
    yp_hash_t       hash;
    ypObject       *keyset = ypDict_KEYSET(mp);
    ypSet_KeyEntry *key_loc;
    ypObject       *result = yp_None;
    ypObject      **value_loc;
    ypObject       *oldvalue;

    yp_ASSERT1(mp != yp_frozendict_empty);  // don't modify the empty frozendict!

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC(key, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(keyset, key, hash, &key_loc);
    if (yp_isexceptionC(result)) return result;

    // If the there's no existing value, then there's nothing to do (if the key is not in the set,
    // then *value_loc will be NULL)
    value_loc = ypDict_VALUE_ENTRY(mp, key_loc);
    if (*value_loc == NULL) return ypSet_dummy;

    // Otherwise, we need to remove the value
    oldvalue = *value_loc;
    *value_loc = NULL;
    ypDict_SET_LEN(mp, ypDict_LEN(mp) - 1);
    return oldvalue;  // new ref
}

// Item iterators yield iterators that yield exactly 2 values: key first, then value. This
// returns new references to that pair in *key and *value. Both are set to an exception on error;
// in particular, yp_ValueError is returned if exactly 2 values are not returned.
// XXX Yes, the yielded value can be any iterable, even a set or dict (good luck guessing which
// will be the key, and which the value)
static void _ypDict_iter_items_next(ypObject *itemiter, ypObject **key, ypObject **value)
{
    ypObject *keyvaliter = yp_next(itemiter);  // new ref
    if (yp_isexceptionC(keyvaliter)) {         // including yp_StopIteration
        *key = *value = keyvaliter;
        return;
    }
    yp_unpackN(keyvaliter, 2, key, value);
    yp_decref(keyvaliter);
}

// XXX Check for the mp==other case _before_ calling this function
static ypObject *_ypDict_update_fromdict(ypObject *mp, ypObject *other)
{
    yp_ssize_t spaceleft = _ypSet_space_remaining(ypDict_KEYSET(mp));
    yp_ssize_t valuesleft = ypDict_LEN(other);
    ypObject  *other_keyset = ypDict_KEYSET(other);
    yp_ssize_t i;
    ypObject  *other_value;
    ypObject  *result;

    yp_ASSERT(mp != other, "_ypDict_update_fromdict called with mp==other");
    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(other) == ypFrozenDict_CODE);

    // TODO If mp is empty, then we can clear mp, use other's keyset, and memcpy the array of
    // values.

    for (i = 0; valuesleft > 0; i++) {
        other_value = ypDict_VALUES(other)[i];
        if (other_value == NULL) continue;
        valuesleft -= 1;

        // TODO _ypDict_push will call yp_hashC again, even though we already know the hash
        // TODO yp_hashC may mutate mp, invalidating valuesleft!
        result = _ypDict_push(
                mp, ypSet_TABLE(other_keyset)[i].se_key, other_value, 1, &spaceleft, valuesleft);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *_ypDict_update_fromiter(ypObject *mp, ypObject *itemiter)
{
    ypObject  *exc = yp_None;
    ypObject  *result;
    ypObject  *key;
    ypObject  *value;
    yp_ssize_t spaceleft = _ypSet_space_remaining(ypDict_KEYSET(mp));
    yp_ssize_t length_hint = yp_length_hintC(itemiter, &exc);  // zero on error

    while (1) {
        _ypDict_iter_items_next(itemiter, &key, &value);  // new refs: key, value
        if (yp_isexceptionC(key)) {
            if (yp_isexceptionC2(key, yp_StopIteration)) break;
            return key;
        }
        length_hint -= 1;  // check for <0 only when we need it in _ypDict_push
        result = _ypDict_push(mp, key, value, 1, &spaceleft, length_hint);
        yp_decrefN(2, key, value);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

// Adds the key/value pairs yielded from either yp_iter_items or yp_iter to the dict. If the dict
// has enough space to hold all the items, the dict is not resized (important, as yp_dictK et al
// pre-allocate the necessary space).
// XXX Check for the "fellow frozendict" case before calling this function.
// TODO Could a special (key,value)-handling ypQuickIter consolidate this code or make it quicker?
static ypObject *_ypDict_update_fromiterable(ypObject *mp, ypObject *x)
{
    ypObject *itemiter;
    ypObject *result;

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(x) != ypFrozenDict_CODE);

    // Prefer yp_iter_items over yp_iter if supported.
    // FIXME replace with yp_miniiter_items now that it's supported
    // TODO help(dict.update) states that it only looks for a .keys() method. This is probably
    // better: while keys() requires an extra lookup, that's likely cheaper than creating all those
    // 2-tuples...although, with yp_miniiter_items, we would get the best of both worlds, so perhaps
    // this would be an area to break with Python (although, if/when we ever script, we're back to
    // making 2-tuples...so what do we want to optimize? (but in those cases, we could define the
    // tp_miniiter_items_next to do the .keys()/[key] thing.)).
    itemiter = yp_iter_items(x);                                            // new ref
    if (yp_isexceptionC2(itemiter, yp_MethodError)) itemiter = yp_iter(x);  // new ref
    if (yp_isexceptionC(itemiter)) return itemiter;

    result = _ypDict_update_fromiter(mp, itemiter);

    yp_decref(itemiter);
    return result;
}

// Public methods

static ypObject *frozendict_traverse(ypObject *mp, visitfunc visitor, void *memo)
{
    yp_ssize_t valuesleft = ypDict_LEN(mp);
    yp_ssize_t i;
    ypObject  *value;
    ypObject  *result;

    result = visitor(ypDict_KEYSET(mp), memo);
    if (yp_isexceptionC(result)) return result;

    // TODO visitor may mutate mp, invalidating valuesleft!
    for (i = 0; valuesleft > 0; i++) {
        value = ypDict_VALUES(mp)[i];
        if (value == NULL) continue;
        valuesleft -= 1;
        result = visitor(value, memo);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *frozendict_unfrozen_copy(ypObject *x)
{
    if (ypDict_LEN(x) < 1) return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    return _ypDict_copy(ypDict_CODE, x, /*alloclen_fixed=*/FALSE);
}

static ypObject *frozendict_frozen_copy(ypObject *x)
{
    if (ypDict_LEN(x) < 1) return yp_frozendict_empty;
    // A shallow copy of a frozendict to a frozendict doesn't require an actual copy
    if (ypObject_TYPE_CODE(x) == ypFrozenDict_CODE) return yp_incref(x);
    return _ypDict_copy(ypFrozenDict_CODE, x, /*alloclen_fixed=*/TRUE);
}

static ypObject *frozendict_unfrozen_deepcopy(ypObject *x, visitfunc copy_visitor, void *copy_memo)
{
    return _ypDict_deepcopy(ypDict_CODE, x, copy_visitor, copy_memo);
}

static ypObject *frozendict_frozen_deepcopy(ypObject *x, visitfunc copy_visitor, void *copy_memo)
{
    if (ypDict_LEN(x) < 1) return yp_frozendict_empty;
    return _ypDict_deepcopy(ypFrozenDict_CODE, x, copy_visitor, copy_memo);
}

static ypObject *frozendict_bool(ypObject *mp) { return ypBool_FROM_C(ypDict_LEN(mp)); }

// TODO comparison functions can recurse, just like currenthash...fix!
static ypObject *frozendict_eq(ypObject *mp, ypObject *x)
{
    yp_ssize_t      valuesleft;
    yp_ssize_t      mp_i;
    ypObject       *mp_value;
    ypSet_KeyEntry *mp_key_loc;
    ypSet_KeyEntry *x_key_loc;
    ypObject       *x_value;
    ypObject       *result;

    if (mp == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypFrozenDict_CODE) return yp_ComparisonNotImplemented;
    if (ypDict_LEN(mp) != ypDict_LEN(x)) return yp_False;

    // We need to inspect all our items for equality, which could be time-intensive. It's fairly
    // obvious that the pre-computed hash, if available, can save us some time when mp!=x.
    if (ypObject_CACHED_HASH(mp) != ypObject_HASH_INVALID &&
            ypObject_CACHED_HASH(x) != ypObject_HASH_INVALID &&
            ypObject_CACHED_HASH(mp) != ypObject_CACHED_HASH(x)) {
        return yp_False;
    }

    // TODO yp_eq may mutate mp, invalidating valuesleft!
    valuesleft = ypDict_LEN(mp);
    for (mp_i = 0; valuesleft > 0; mp_i++) {
        mp_value = ypDict_VALUES(mp)[mp_i];
        if (mp_value == NULL) continue;
        valuesleft -= 1;
        mp_key_loc = ypSet_TABLE(ypDict_KEYSET(mp)) + mp_i;

        // If the key is not also in x, mp and x are not equal
        result = _ypSet_lookkey(
                ypDict_KEYSET(x), mp_key_loc->se_key, mp_key_loc->se_hash, &x_key_loc);
        if (yp_isexceptionC(result)) return result;
        x_value = *ypDict_VALUE_ENTRY(x, x_key_loc);
        if (x_value == NULL) return yp_False;

        // If the values are not equal, then neither are mp and x
        result = yp_eq(mp_value, x_value);
        if (result != yp_True) return result;  // yp_False or an exception
    }
    return yp_True;
}

static ypObject *frozendict_ne(ypObject *mp, ypObject *x)
{
    ypObject *result = frozendict_eq(mp, x);
    return ypBool_NOT(result);
}

// Essentially: hash((key, value))
static yp_hash_t _ypDict_currenthash_item(yp_hash_t key_hash, yp_hash_t value_hash)
{
    yp_HashSequence_state_t state;
    yp_HashSequence_init(&state, 2);
    yp_HashSequence_next(&state, key_hash);
    yp_HashSequence_next(&state, value_hash);
    return yp_HashSequence_fini(&state);
}

// Essentially: hash(frozenset(x.items()))
static ypObject *frozendict_currenthash(
        ypObject *mp, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    yp_HashSet_state_t state;
    yp_ssize_t         valuesleft;
    yp_ssize_t         mp_i;
    ypObject          *mp_value;
    ypSet_KeyEntry    *mp_key_loc;
    yp_hash_t          mp_value_hash;
    yp_hash_t          mp_item_hash;
    ypObject          *result;

    yp_HashSet_init(&state, ypDict_LEN(mp));

    // TODO hash_visitor may mutate mp, invalidating valuesleft!
    valuesleft = ypDict_LEN(mp);
    for (mp_i = 0; valuesleft > 0; mp_i++) {
        mp_value = ypDict_VALUES(mp)[mp_i];
        if (mp_value == NULL) continue;
        valuesleft -= 1;
        mp_key_loc = ypSet_TABLE(ypDict_KEYSET(mp)) + mp_i;

        // We have the hash of the key at mp_key_loc->se_hash, but not the hash of the value.
        result = hash_visitor(mp_value, hash_memo, &mp_value_hash);
        if (yp_isexceptionC(result)) return result;

        mp_item_hash = _ypDict_currenthash_item(mp_key_loc->se_hash, mp_value_hash);
        yp_HashSet_next(&state, mp_item_hash);
    }

    *hash = yp_HashSet_fini(&state);

    return yp_None;
}

static ypObject *frozendict_contains(ypObject *mp, ypObject *key)
{
    yp_hash_t       hash;
    ypObject       *keyset = ypDict_KEYSET(mp);
    ypSet_KeyEntry *key_loc;
    ypObject       *result = yp_None;

    hash = yp_currenthashC(key, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(keyset, key, hash, &key_loc);
    if (yp_isexceptionC(result)) return result;
    return ypBool_FROM_C((*ypDict_VALUE_ENTRY(mp, key_loc)) != NULL);
}

static ypObject *frozendict_len(ypObject *mp, yp_ssize_t *len)
{
    *len = ypDict_LEN(mp);
    return yp_None;
}

static ypObject *dict_clear(ypObject *mp)
{
    ypObject  *keyset;
    yp_ssize_t alloclen;
    ypObject **oldvalues = ypDict_VALUES(mp);
    yp_ssize_t valuesleft = ypDict_LEN(mp);
    yp_ssize_t i;

    if (ypDict_LEN(mp) < 1) return yp_None;

    // Create a new keyset
    // TODO Rather than creating a new keyset which we may never need, use yp_frozenset_empty,
    // leaving it to _ypDict_push to allocate a new keyset...BUT this means yp_frozenset_empty
    // needs an alloclen of zero, or else we're going to try adding keys to it.
    keyset = _ypSet_new(ypFrozenSet_CODE, 0, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(keyset)) return keyset;
    alloclen = ypSet_ALLOCLEN(keyset);
    yp_ASSERT(
            alloclen == ypSet_ALLOCLEN_MIN, "expect alloclen of ypSet_ALLOCLEN_MIN for new keyset");

    // Discard the old values
    // TODO yp_decref may mutate mp, invalidating valuesleft!
    for (i = 0; valuesleft > 0; i++) {
        if (oldvalues[i] == NULL) continue;
        valuesleft -= 1;
        yp_decref(oldvalues[i]);
    }

    // Free memory. Remember that mp->ob_alloclen has been repurposed to hold a search finger.
    // FIXME What if yp_decref modifies mp?
    yp_decref(ypDict_KEYSET(mp));
    ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR(mp, ypDictObject, ypDict_ALLOCLEN_MAX);
    yp_ASSERT(ypDict_VALUES(mp) == ypDict_INLINE_DATA(mp), "dict_clear didn't allocate inline!");
    yp_ASSERT(ypObject_ALLOCLEN(mp) >= ypSet_ALLOCLEN_MIN,
            "dict inlinelen must be at least ypSet_ALLOCLEN_MIN");

    // Update our attributes and return
    ypDict_SET_LEN(mp, 0);
    ypDict_KEYSET(mp) = keyset;
    yp_memset(ypDict_VALUES(mp), 0, alloclen * yp_sizeof(ypObject *));
    return yp_None;
}

// A defval of NULL means to raise an error if key is not in dict
static ypObject *frozendict_getdefault(ypObject *mp, ypObject *key, ypObject *defval)
{
    yp_hash_t       hash;
    ypObject       *keyset = ypDict_KEYSET(mp);
    ypSet_KeyEntry *key_loc;
    ypObject       *result = yp_None;
    ypObject       *value;

    // Because we are called directly (i.e. ypFunction), ensure we're called correctly
    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(mp) == ypFrozenDict_CODE);

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC(key, &result);
    if (yp_isexceptionC(result)) return result;
    result = _ypSet_lookkey(keyset, key, hash, &key_loc);
    if (yp_isexceptionC(result)) return result;

    // If the there's no existing value, return defval, otherwise return the value
    value = *ypDict_VALUE_ENTRY(mp, key_loc);
    if (value == NULL) {
        if (defval == NULL) return yp_KeyError;
        return yp_incref(defval);
    } else {
        return yp_incref(value);
    }
}

// yp_None or an exception
static ypObject *dict_setitem(ypObject *mp, ypObject *key, ypObject *value)
{
    yp_ssize_t spaceleft = _ypSet_space_remaining(ypDict_KEYSET(mp));
    // TODO Overallocate
    ypObject *result = _ypDict_push(mp, key, value, 1, &spaceleft, 0);
    if (yp_isexceptionC(result)) return result;
    return yp_None;
}

static ypObject *dict_delitem(ypObject *mp, ypObject *key)
{
    ypObject *result = _ypDict_pop(mp, key);
    if (result == ypSet_dummy) return yp_KeyError;
    if (yp_isexceptionC(result)) return result;
    yp_decref(result);
    return yp_None;
}

static ypObject *dict_popvalue(ypObject *mp, ypObject *key, ypObject *defval)
{
    ypObject *result = _ypDict_pop(mp, key);
    if (result == ypSet_dummy) return yp_incref(defval);
    return result;
}

// Note the difference between this, which removes an arbitrary item, and _ypDict_pop, which
// removes a specific item.
// XXX On error, returns exception, but leaves *key/*value unmodified.
// XXX Adapted from Python's set_pop
static ypObject *dict_popitem(ypObject *mp, ypObject **key, ypObject **value)
{
    register yp_ssize_t i;
    register ypObject **values = ypDict_VALUES(mp);

    if (ypDict_LEN(mp) < 1) return yp_KeyError;  // "pop from an empty dict"

    /* We abuse mp->ob_alloclen to hold a search finger; recall
     * ypDict_ALLOCLEN uses the keyset's ob_alloclen
     */
    i = ypDict_POPITEM_FINGER(mp);
    /* The ob_alloclen may be a real allocation length, or it may
     * be a legit search finger, or it may be a once-legit search
     * finger that's out of bounds now because it wrapped around
     * or the table shrunk -- simply make sure it's in bounds now.
     */
    if (i >= ypDict_ALLOCLEN(mp) || i < 0) i = 0;
    while (values[i] == NULL) {
        i++;
        if (i >= ypDict_ALLOCLEN(mp)) i = 0;
    }
    *key = yp_incref(ypSet_TABLE(ypDict_KEYSET(mp))[i].se_key);
    *value = values[i];
    values[i] = NULL;
    ypDict_SET_LEN(mp, ypDict_LEN(mp) - 1);
    ypDict_SET_POPITEM_FINGER(mp, i + 1);  // next place to start
    return yp_None;
}

static ypObject *dict_setdefault(ypObject *mp, ypObject *key, ypObject *defval)
{
    yp_hash_t       hash;
    ypObject       *keyset = ypDict_KEYSET(mp);
    ypSet_KeyEntry *key_loc;
    ypObject       *result = yp_None;
    ypObject      **value_loc;

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC(key, &result);
    if (yp_isexceptionC(result)) return result;  // returns if key is an exception
    if (yp_isexceptionC(defval)) return defval;  // returns if defval is an exception
    result = _ypSet_lookkey(keyset, key, hash, &key_loc);
    if (yp_isexceptionC(result)) return result;

    // If the there's no existing value, add and return defval, otherwise return the value
    value_loc = ypDict_VALUE_ENTRY(mp, key_loc);
    if (*value_loc == NULL) {
        if (ypSet_ENTRY_USED(key_loc)) {
            *value_loc = yp_incref(defval);
            ypDict_SET_LEN(mp, ypDict_LEN(mp) + 1);
        } else {
            yp_ssize_t spaceleft = _ypSet_space_remaining(keyset);
            // TODO Overallocate
            result = _ypDict_push_newkey(mp, &key_loc, key, hash, defval, &spaceleft, 0);
            // value_loc is no longer valid
            if (yp_isexceptionC(result)) return result;
        }
        return yp_incref(defval);
    } else {
        return yp_incref(*value_loc);
    }
}

static ypObject *dict_updateK(ypObject *mp, int n, va_list args)
{
    yp_ssize_t spaceleft = _ypSet_space_remaining(ypDict_KEYSET(mp));
    ypObject  *result = yp_None;
    ypObject  *key;
    ypObject  *value;

    while (n > 0) {
        key = va_arg(args, ypObject *);    // borrowed
        value = va_arg(args, ypObject *);  // borrowed
        n -= 1;
        result = _ypDict_push(mp, key, value, 1, &spaceleft, n);
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

static ypObject *dict_update(ypObject *mp, int n, va_list args)
{
    ypObject *result;
    for (/*n already set*/; n > 0; n--) {
        ypObject *x = va_arg(args, ypObject *);  // borrowed
        if (ypObject_TYPE_PAIR_CODE(x) == ypFrozenDict_CODE) {
            if (mp == x) continue;
            result = _ypDict_update_fromdict(mp, x);
        } else {
            result = _ypDict_update_fromiterable(mp, x);
        }
        if (yp_isexceptionC(result)) return result;
    }
    return yp_None;
}

typedef struct {
    yp_uint32_t keys : 1;
    yp_uint32_t itemsleft : 31;
    // aligned
    yp_uint32_t values : 1;
    yp_uint32_t index : 31;
} ypDictMiState;
yp_STATIC_ASSERT(ypDict_LEN_MAX <= 0x7FFFFFFFu, len_fits_31_bits);
yp_STATIC_ASSERT(yp_sizeof(yp_uint64_t) >= yp_sizeof(ypDictMiState), ypDictMiState_fits_uint64);
#define ypDictMiState_SET_ITEMSLEFT(state, v) ((state)->itemsleft = (yp_uint32_t)(v)&0x7FFFFFFFu)
#define ypDictMiState_SET_INDEX(state, v) ((state)->index = (yp_uint32_t)(v)&0x7FFFFFFFu)

static ypObject *frozendict_miniiter_items(ypObject *mp, yp_uint64_t *_state)
{
    ypDictMiState *state = (ypDictMiState *)_state;
    state->keys = 1;
    state->values = 1;
    ypDictMiState_SET_ITEMSLEFT(state, ypDict_LEN(mp));
    state->index = 0;
    return yp_incref(mp);
}
static ypObject *frozendict_iter_items(ypObject *x)
{
    return _ypMiIter_fromminiiter(x, frozendict_miniiter_items);
}

static ypObject *frozendict_miniiter_keys(ypObject *mp, yp_uint64_t *_state)
{
    ypDictMiState *state = (ypDictMiState *)_state;
    state->keys = 1;
    state->values = 0;
    ypDictMiState_SET_ITEMSLEFT(state, ypDict_LEN(mp));
    state->index = 0;
    return yp_incref(mp);
}
static ypObject *frozendict_iter_keys(ypObject *x)
{
    return _ypMiIter_fromminiiter(x, frozendict_miniiter_keys);
}

static ypObject *frozendict_miniiter_values(ypObject *mp, yp_uint64_t *_state)
{
    ypDictMiState *state = (ypDictMiState *)_state;
    state->keys = 0;
    state->values = 1;
    ypDictMiState_SET_ITEMSLEFT(state, ypDict_LEN(mp));
    state->index = 0;
    return yp_incref(mp);
}
static ypObject *frozendict_iter_values(ypObject *x)
{
    return _ypMiIter_fromminiiter(x, frozendict_miniiter_values);
}

// Returns the index of the next item to yield, or -1 if exhausted. Updates state.
// XXX We need to be a little suspicious of _state...just in case the caller has changed it
static yp_ssize_t _frozendict_miniiter_next(ypObject *mp, ypDictMiState *state)
{
    yp_ssize_t index = state->index;  // don't forget to write it back
    if (state->itemsleft < 1) return -1;

    // Find the next entry.
    while (1) {
        if (index >= ypDict_ALLOCLEN(mp)) {
            ypDictMiState_SET_INDEX(state, ypDict_ALLOCLEN(mp));
            state->itemsleft = 0;
            return -1;
        }
        if (ypDict_VALUES(mp)[index] != NULL) break;
        index++;
    }

    // Update state and return.
    ypDictMiState_SET_INDEX(state, (index + 1));
    ypDictMiState_SET_ITEMSLEFT(state, (state->itemsleft - 1));
    return index;
}

// XXX We need to be a little suspicious of _state...just in case the caller has changed it
static ypObject *frozendict_miniiter_next(ypObject *mp, yp_uint64_t *_state)
{
    ypDictMiState *state = (ypDictMiState *)_state;
    yp_ssize_t     index;

    index = _frozendict_miniiter_next(mp, state);
    if (index < 0) return yp_StopIteration;

    // Find the requested data.
    if (state->keys) {
        if (state->values) {
            // TODO An internal _yp_tuple2, which trusts it won't be passed exceptions, would be
            // quite efficient here
            return yp_tupleN(
                    2, ypSet_TABLE(ypDict_KEYSET(mp))[index].se_key, ypDict_VALUES(mp)[index]);
        } else {
            return yp_incref(ypSet_TABLE(ypDict_KEYSET(mp))[index].se_key);
        }
    } else {
        if (state->values) {
            return yp_incref(ypDict_VALUES(mp)[index]);
        } else {
            return yp_SystemError;  // should never occur
        }
    }
}

// XXX On error, returns exception, but leaves *key/*value unmodified.
// XXX We need to be a little suspicious of _state...just in case the caller has changed it
static ypObject *frozendict_miniiter_items_next(
        ypObject *mp, yp_uint64_t *_state, ypObject **key, ypObject **value)
{
    ypDictMiState *state = (ypDictMiState *)_state;
    yp_ssize_t     index;

    if (!state->keys || !state->values) return yp_TypeError;  // FIXME ValueError?

    index = _frozendict_miniiter_next(mp, state);
    if (index < 0) return yp_StopIteration;

    // Find the requested data
    *key = yp_incref(ypSet_TABLE(ypDict_KEYSET(mp))[index].se_key);
    *value = yp_incref(ypDict_VALUES(mp)[index]);
    return yp_None;
}

static ypObject *frozendict_miniiter_length_hint(
        ypObject *mp, yp_uint64_t *state, yp_ssize_t *length_hint)
{
    *length_hint = ((ypDictMiState *)state)->itemsleft;
    return yp_None;
}

static ypObject *frozendict_dealloc(ypObject *mp, void *memo)
{
    ypObject **values = ypDict_VALUES(mp);
    yp_ssize_t valuesleft = ypDict_LEN(mp);
    yp_ssize_t i;

    yp_decref_fromdealloc(ypDict_KEYSET(mp), memo);
    for (i = 0; valuesleft > 0; i++) {
        if (values[i] == NULL) continue;
        valuesleft -= 1;
        yp_decref_fromdealloc(values[i], memo);
    }
    ypMem_FREE_CONTAINER(mp, ypDictObject);
    return yp_None;
}

static ypObject *frozendict_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 4, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_frozendict);
    yp_ASSERT1(ypObject_TYPE_CODE(argarray[3]) == ypFrozenDict_CODE);

    if (ypDict_LEN(argarray[3]) < 1) {  // no keyword args
        return yp_frozendict(argarray[1]);
    } else if (argarray[1] == yp_frozendict_empty) {  // the default value
        return yp_incref(argarray[3]);  // **kwargs is always a frozendict, so just return it
    } else {
        // FIXME Need a yp_frozendict that merges multiple objects (yp_frozendictN?)
        return yp_NotImplementedError;
    }
}

static ypObject *dict_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 4, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_dict);
    yp_ASSERT1(ypObject_TYPE_CODE(argarray[3]) == ypFrozenDict_CODE);

    if (ypDict_LEN(argarray[3]) < 1) {  // no keyword args
        return yp_dict(argarray[1]);
    } else if (argarray[1] == yp_frozendict_empty) {  // the default value
        return _ypDict_copy(ypDict_CODE, argarray[3], /*alloclen_fixed=*/FALSE);
    } else {
        // TODO Could improve this by pre-allocating.
        ypObject *result;
        ypObject *mp = yp_dict(argarray[1]);
        if (yp_isexceptionC(mp)) return mp;
        result = _ypDict_update_fromdict(mp, argarray[3]);
        if (yp_isexceptionC(result)) {
            yp_decref(mp);
            return result;
        }
        return mp;
    }
}

#define _ypFrozenDict_FUNC_NEW_PARAMETERS                                   \
    ({yp_CONST_REF(yp_s_cls), NULL},                                        \
            {yp_CONST_REF(yp_s_object), yp_CONST_REF(yp_frozendict_empty)}, \
            {yp_CONST_REF(yp_s_slash), NULL}, {yp_CONST_REF(yp_s_star_star_kwargs), NULL})
yp_IMMORTAL_FUNCTION_static(
        frozendict_func_new, frozendict_func_new_code, _ypFrozenDict_FUNC_NEW_PARAMETERS);
yp_IMMORTAL_FUNCTION_static(dict_func_new, dict_func_new_code, _ypFrozenDict_FUNC_NEW_PARAMETERS);

static ypMappingMethods ypFrozenDict_as_mapping = {
        frozendict_miniiter_keys,        // tp_miniiter_keys
        frozendict_miniiter_values,      // tp_miniiter_values
        frozendict_miniiter_items,       // tp_miniiter_items
        frozendict_miniiter_items_next,  // tp_miniiter_items_next
        frozendict_iter_keys,            // tp_iter_keys
        frozendict_iter_values,          // tp_iter_values
        frozendict_iter_items,           // tp_iter_items
        MethodError_objobjobjproc,       // tp_popvalue
        MethodError_objpobjpobjproc,     // tp_popitem
        MethodError_objobjobjproc,       // tp_setdefault
        MethodError_objvalistproc        // tp_updateK
};

static ypTypeObject ypFrozenDict_Type = {
        yp_TYPE_HEAD_INIT,
        ypType_FLAG_IS_MAPPING,  // tp_flags
        NULL,                    // tp_name

        // Object fundamentals
        yp_CONST_REF(frozendict_func_new),  // tp_func_new
        frozendict_dealloc,                 // tp_dealloc
        frozendict_traverse,                // tp_traverse
        NULL,                               // tp_str
        NULL,                               // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,           // tp_freeze
        frozendict_unfrozen_copy,      // tp_unfrozen_copy
        frozendict_frozen_copy,        // tp_frozen_copy
        frozendict_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        frozendict_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,           // tp_invalidate

        // Boolean operations and comparisons
        frozendict_bool,             // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        frozendict_eq,               // tp_eq
        frozendict_ne,               // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        frozendict_currenthash,  // tp_currenthash
        MethodError_objproc,     // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        frozendict_miniiter_keys,         // tp_miniiter
        TypeError_miniiterfunc,           // tp_miniiter_reversed
        frozendict_miniiter_next,         // tp_miniiter_next
        frozendict_miniiter_length_hint,  // tp_miniiter_length_hint
        frozendict_iter_keys,             // tp_iter
        TypeError_objproc,                // tp_iter_reversed
        TypeError_objobjproc,             // tp_send

        // Container operations
        frozendict_contains,        // tp_contains
        frozendict_len,             // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        frozendict_getdefault,      // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        &ypFrozenDict_as_mapping,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

static ypMappingMethods ypDict_as_mapping = {
        frozendict_miniiter_keys,        // tp_miniiter_keys
        frozendict_miniiter_values,      // tp_miniiter_values
        frozendict_miniiter_items,       // tp_miniiter_items
        frozendict_miniiter_items_next,  // tp_miniiter_items_next
        frozendict_iter_keys,            // tp_iter_keys
        frozendict_iter_values,          // tp_iter_values
        frozendict_iter_items,           // tp_iter_items
        dict_popvalue,                   // tp_popvalue
        dict_popitem,                    // tp_popitem
        dict_setdefault,                 // tp_setdefault
        dict_updateK                     // tp_updateK
};

static ypTypeObject ypDict_Type = {
        yp_TYPE_HEAD_INIT,
        ypType_FLAG_IS_MAPPING,  // tp_flags
        NULL,                    // tp_name

        // Object fundamentals
        yp_CONST_REF(dict_func_new),  // tp_func_new
        frozendict_dealloc,           // tp_dealloc
        frozendict_traverse,          // tp_traverse
        NULL,                         // tp_str
        NULL,                         // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,           // tp_freeze
        frozendict_unfrozen_copy,      // tp_unfrozen_copy
        frozendict_frozen_copy,        // tp_frozen_copy
        frozendict_unfrozen_deepcopy,  // tp_unfrozen_deepcopy
        frozendict_frozen_deepcopy,    // tp_frozen_deepcopy
        MethodError_objproc,           // tp_invalidate

        // Boolean operations and comparisons
        frozendict_bool,             // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        frozendict_eq,               // tp_eq
        frozendict_ne,               // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        frozendict_currenthash,  // tp_currenthash
        MethodError_objproc,     // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        frozendict_miniiter_keys,         // tp_miniiter
        TypeError_miniiterfunc,           // tp_miniiter_reversed
        frozendict_miniiter_next,         // tp_miniiter_next
        frozendict_miniiter_length_hint,  // tp_miniiter_length_hint
        frozendict_iter_keys,             // tp_iter
        TypeError_objproc,                // tp_iter_reversed
        TypeError_objobjproc,             // tp_send

        // Container operations
        frozendict_contains,        // tp_contains
        frozendict_len,             // tp_len
        MethodError_objobjproc,     // tp_push
        dict_clear,                 // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        frozendict_getdefault,      // tp_getdefault
        dict_setitem,               // tp_setitem
        dict_delitem,               // tp_delitem
        dict_update,                // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        &ypDict_as_mapping,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

// Constructors

static ypObject *_ypDictKV(int type, int n, va_list args)
{
    ypObject *newMp;
    ypObject *result;

    newMp = _ypDict_new(type, n, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newMp)) return newMp;
    result = dict_updateK(newMp, n, args);
    if (yp_isexceptionC(result)) {
        yp_decref(newMp);
        return result;
    }
    return newMp;
}

ypObject *yp_frozendictK(int n, ...)
{
    if (n < 1) return yp_frozendict_empty;
    return_yp_V_FUNC(ypObject *, _ypDictKV, (ypFrozenDict_CODE, n, args), n);
}
ypObject *yp_frozendictKV(int n, va_list args)
{
    if (n < 1) return yp_frozendict_empty;
    return _ypDictKV(ypFrozenDict_CODE, n, args);
}

ypObject *yp_dictK(int n, ...)
{
    if (n < 1) return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    return_yp_V_FUNC(ypObject *, _ypDictKV, (ypDict_CODE, n, args), n);
}
ypObject *yp_dictKV(int n, va_list args)
{
    if (n < 1) return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    return _ypDictKV(ypDict_CODE, n, args);
}

// XXX Handle the "fellow frozendict" case _before_ calling this function.
// XXX Always creates a new keyset; if you want to share x's keyset, use _ypDict_copy
static ypObject *_ypDict(int type, ypObject *x)
{
    ypObject  *exc = yp_None;
    ypObject  *newMp;
    ypObject  *result;
    yp_ssize_t length_hint = yp_lenC(x, &exc);

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(x) != ypFrozenDict_CODE);

    // We could just check yp_length_hintC if it returned an "is exact length" flag.
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint; it just means we can't pre-allocate
        length_hint = yp_length_hintC(x, &exc);
        if (length_hint > ypDict_LEN_MAX) length_hint = ypDict_LEN_MAX;
    } else if (length_hint < 1) {
        // yp_lenC reports an empty iterable, so we can shortcut _ypDict_update_fromiterable
        if (type == ypFrozenDict_CODE) return yp_frozenset_empty;
        return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    } else if (length_hint > ypDict_LEN_MAX) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    newMp = _ypDict_new(type, length_hint, /*alloclen_fixed=*/FALSE);
    if (yp_isexceptionC(newMp)) return newMp;
    result = _ypDict_update_fromiterable(newMp, x);
    if (yp_isexceptionC(result)) {
        yp_decref(newMp);
        return result;
    }

    // TODO We could avoid allocating for an empty iterable altogether if we get the first value
    // before allocating; is this complication worth the optimization?
    if (type == ypFrozenDict_CODE && ypDict_LEN(newMp) < 1) {
        yp_decref(newMp);
        return yp_frozendict_empty;
    }

    return newMp;
}

ypObject *yp_frozendict(ypObject *x)
{
    // If x is a fellow dict then perform a copy so we can share keysets
    if (ypObject_TYPE_PAIR_CODE(x) == ypFrozenDict_CODE) {
        if (ypDict_LEN(x) < 1) return yp_frozendict_empty;
        if (ypObject_TYPE_CODE(x) == ypFrozenDict_CODE) return yp_incref(x);
        return _ypDict_copy(ypFrozenDict_CODE, x, /*alloclen_fixed=*/TRUE);
    }
    return _ypDict(ypFrozenDict_CODE, x);
}

ypObject *yp_dict(ypObject *x)
{
    // If x is a fellow dict then perform a copy so we can share keysets
    if (ypObject_TYPE_PAIR_CODE(x) == ypFrozenDict_CODE) {
        if (ypDict_LEN(x) < 1) return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
        return _ypDict_copy(ypDict_CODE, x, /*alloclen_fixed=*/FALSE);
    }
    return _ypDict(ypDict_CODE, x);
}

// TOOD ypQuickIter could consolidate this with _ypDict_fromkeys
static ypObject *_ypDict_fromkeysNV(int type, ypObject *value, int n, va_list args)
{
    yp_ssize_t spaceleft;
    ypObject  *result = yp_None;
    ypObject  *key;
    ypObject  *newMp;

    if (n > ypDict_LEN_MAX) return yp_MemorySizeOverflowError;
    newMp = _ypDict_new(type, n, /*alloclen_fixed=*/TRUE);
    if (yp_isexceptionC(newMp)) return newMp;
    spaceleft = _ypSet_space_remaining(ypDict_KEYSET(newMp));

    while (n > 0) {
        key = va_arg(args, ypObject *);  // borrowed
        n -= 1;
        result = _ypDict_push(newMp, key, value, 1, &spaceleft, n);
        if (yp_isexceptionC(result)) break;
    }
    if (yp_isexceptionC(result)) {
        yp_decref(newMp);
        return result;
    }
    return newMp;
}

ypObject *yp_frozendict_fromkeysN(ypObject *value, int n, ...)
{
    if (n < 1) return yp_frozendict_empty;
    return_yp_V_FUNC(ypObject *, _ypDict_fromkeysNV, (ypFrozenDict_CODE, value, n, args), n);
}
ypObject *yp_frozendict_fromkeysNV(ypObject *value, int n, va_list args)
{
    if (n < 1) return yp_frozendict_empty;
    return _ypDict_fromkeysNV(ypFrozenDict_CODE, value, n, args);
}

ypObject *yp_dict_fromkeysN(ypObject *value, int n, ...)
{
    if (n < 1) return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    return_yp_V_FUNC(ypObject *, _ypDict_fromkeysNV, (ypDict_CODE, value, n, args), n);
}
ypObject *yp_dict_fromkeysNV(ypObject *value, int n, va_list args)
{
    if (n < 1) return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    return _ypDict_fromkeysNV(ypDict_CODE, value, n, args);
}

static ypObject *_ypDict_fromkeys(int type, ypObject *iterable, ypObject *value)
{
    ypObject   *exc = yp_None;
    ypObject   *result = yp_None;
    ypObject   *mi;
    yp_uint64_t mi_state;
    ypObject   *newMp;
    yp_ssize_t  spaceleft;
    ypObject   *key;
    yp_ssize_t  length_hint = yp_lenC(iterable, &exc);
    if (yp_isexceptionC(exc)) {
        // Ignore errors determining length_hint; it just means we can't pre-allocate
        length_hint = yp_length_hintC(iterable, &exc);
        if (length_hint > ypDict_LEN_MAX) length_hint = ypDict_LEN_MAX;
    } else if (length_hint < 1) {
        // yp_lenC reports an empty iterable, so we can shortcut _ypDict_push
        if (type == ypFrozenDict_CODE) return yp_frozendict_empty;
        return _ypDict_new(ypDict_CODE, 0, /*alloclen_fixed=*/FALSE);
    } else if (length_hint > ypDict_LEN_MAX) {
        // yp_lenC reports that we don't have room to add their elements
        return yp_MemorySizeOverflowError;
    }

    mi = yp_miniiter(iterable, &mi_state);  // new ref
    if (yp_isexceptionC(mi)) return mi;

    newMp = _ypDict_new(type, length_hint, /*alloclen_fixed=*/FALSE);  // new ref
    if (yp_isexceptionC(newMp)) {
        yp_decref(mi);
        return newMp;
    }
    spaceleft = _ypSet_space_remaining(ypDict_KEYSET(newMp));

    while (1) {
        key = yp_miniiter_next(mi, &mi_state);  // new ref
        if (yp_isexceptionC(key)) {
            if (yp_isexceptionC2(key, yp_StopIteration)) break;  // end of iterator
            result = key;
            break;
        }
        length_hint -= 1;
        result = _ypDict_push(newMp, key, value, 1, &spaceleft, length_hint);
        yp_decref(key);
        if (yp_isexceptionC(result)) break;
    }
    yp_decref(mi);
    if (yp_isexceptionC(result)) {
        yp_decref(newMp);
        return result;
    }
    return newMp;
}

ypObject *yp_frozendict_fromkeys(ypObject *iterable, ypObject *value)
{
    return _ypDict_fromkeys(ypFrozenDict_CODE, iterable, value);
}
ypObject *yp_dict_fromkeys(ypObject *iterable, ypObject *value)
{
    return _ypDict_fromkeys(ypDict_CODE, iterable, value);
}

#pragma endregion dict


/*************************************************************************************************
 * Immutable "start + i*step" sequence of integers
 *************************************************************************************************/
#pragma region range

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

#define ypRange_START(r) (((ypRangeObject *)r)->start)
#define ypRange_STEP(r) (((ypRangeObject *)r)->step)
#define ypRange_LEN ypObject_CACHED_LEN
#define ypRange_SET_LEN ypObject_SET_CACHED_LEN

// We normalize start and step for small ranges to make comparisons and hashes easier; use this to
// ensure we've done it correctly
#define ypRange_ASSERT_NORMALIZED(r)                           \
    do {                                                       \
        yp_ASSERT(ypRange_LEN(r) > 0 || ypRange_START(r) == 0, \
                "empty range should have start of 0");         \
        yp_ASSERT(ypRange_LEN(r) > 1 || ypRange_STEP(r) == 1,  \
                "0- or 1-range should have step of 1");        \
    } while (0)

// Returns the value at i, assuming i is an adjusted index
#define ypRange_GET_INDEX(r, i) ((yp_int_t)(ypRange_START(r) + (ypRange_STEP(r) * (i))))

// Returns true if the two ranges are equal (assuming r and x both pass ypRange_ASSERT_NORMALIZED)
#define ypRange_ARE_EQUAL(r, x)                                                  \
    (ypRange_LEN(r) == ypRange_LEN(x) && ypRange_START(r) == ypRange_START(x) && \
            ypRange_STEP(r) == ypRange_STEP(x))

// Determines the index in r for the given object x, or -1 if it isn't in the range
// XXX If using *index as an actual index, ensure it doesn't overflow yp_ssize_t
static ypObject *_ypRange_find(ypObject *r, ypObject *x, yp_ssize_t *index)
{
    ypObject *exc = yp_None;
    yp_int_t  r_end;
    yp_int_t  x_offset;
    yp_int_t  x_asint = yp_asint_exactC(x, &exc);
    if (yp_isexceptionC(exc)) {
        // If x isn't an int or float, or can't be exactly converted to an equal int, then it's not
        // contained in this range
        if (yp_isexceptionCN(exc, 2, yp_TypeError, yp_ArithmeticError)) {
            *index = -1;
            return yp_None;
        }
        *index = -1;
        return exc;
    }

    yp_ASSERT(ypRange_LEN(r) <= yp_SSIZE_T_MAX,
            "range.find not supporting range lengths >yp_SSIZE_T_MAX");
    r_end = ypRange_GET_INDEX(r, ypRange_LEN(r));
    if (ypRange_STEP(r) < 0) {
        if (x_asint <= r_end || ypRange_START(r) < x_asint) {
            *index = -1;
            return yp_None;
        }
    } else {
        if (x_asint < ypRange_START(r) || r_end <= x_asint) {
            *index = -1;
            return yp_None;
        }
    }

    x_offset = x_asint - ypRange_START(r);
    if (x_offset % ypRange_STEP(r) == 0) {
        yp_ASSERT((x_offset / ypRange_STEP(r)) <= yp_SSIZE_T_MAX,
                "impossibly, range.find calculated an index >yp_SSIZE_T_MAX (?!)");
        *index = (yp_ssize_t)(x_offset / ypRange_STEP(r));
    } else {
        *index = -1;
    }
    return yp_None;
}

static ypObject *range_frozen_copy(ypObject *r) { return yp_incref(r); }

static ypObject *range_frozen_deepcopy(ypObject *r, visitfunc copy_visitor, void *copy_memo)
{
    ypObject *newR;
    ypObject *result;

    if (ypRange_LEN(r) < 1) return yp_range_empty;

    newR = ypMem_MALLOC_FIXED(ypRangeObject, ypRange_CODE);
    if (yp_isexceptionC(newR)) return newR;
    ypRange_START(newR) = ypRange_START(r);
    ypRange_STEP(newR) = ypRange_STEP(r);
    ypRange_SET_LEN(newR, ypRange_LEN(r));
    ypRange_ASSERT_NORMALIZED(newR);

    result = _yp_deepcopy_memo_setitem(copy_memo, r, newR);
    if (yp_isexceptionC(result)) {
        yp_decref(newR);
        return result;
    }

    return newR;
}

static ypObject *range_bool(ypObject *r) { return ypBool_FROM_C(ypRange_LEN(r)); }

// XXX Using ypSequence_AdjustIndexC assumes we don't have ranges longer than yp_SSIZE_T_MAX
static ypObject *range_getindex(ypObject *r, yp_ssize_t i, ypObject *defval)
{
    if (!ypSequence_AdjustIndexC(ypRange_LEN(r), &i)) {
        if (defval == NULL) return yp_IndexError;
        return yp_incref(defval);
    }
    return yp_intC(ypRange_GET_INDEX(r, i));
}

// XXX Using ypSlice_AdjustIndicesC assumes we don't have ranges longer than yp_SSIZE_T_MAX
static ypObject *range_getslice(ypObject *r, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step)
{
    ypObject  *result;
    yp_ssize_t newR_len;
    ypObject  *newR;

    result = ypSlice_AdjustIndicesC(ypRange_LEN(r), &start, &stop, &step, &newR_len);
    if (yp_isexceptionC(result)) return result;

    if (newR_len < 1) return yp_range_empty;
    if (newR_len >= ypRange_LEN(r) && step == 1) return yp_incref(r);

    newR = ypMem_MALLOC_FIXED(ypRangeObject, ypRange_CODE);
    if (yp_isexceptionC(newR)) return newR;
    ypRange_START(newR) = ypRange_GET_INDEX(r, start);
    if (newR_len < 2) {
        ypRange_STEP(newR) = 1;
    } else {
        ypRange_STEP(newR) = ypRange_STEP(r) * step;
    }
    ypRange_SET_LEN(newR, newR_len);
    ypRange_ASSERT_NORMALIZED(newR);
    return newR;
}

static ypObject *range_contains(ypObject *r, ypObject *x)
{
    yp_ssize_t index;
    ypObject  *result = _ypRange_find(r, x, &index);
    if (yp_isexceptionC(result)) return result;
    return ypBool_FROM_C(index >= 0);
}

// XXX Using ypSlice_AdjustIndicesC assumes we don't have ranges longer than yp_SSIZE_T_MAX
static ypObject *range_find(ypObject *r, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        findfunc_direction direction, yp_ssize_t *_index)
{
    yp_ssize_t step = 1;     // won't actually change
    yp_ssize_t slicelength;  // unnecessary
    yp_ssize_t index;
    ypObject  *result = _ypRange_find(r, x, &index);
    if (yp_isexceptionC(result)) return result;

    result = ypSlice_AdjustIndicesC(ypRange_LEN(r), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    // This assertion assures that index==-1 (ie item not in range) won't be confused
    yp_ASSERT(start >= 0, "ypSlice_AdjustIndicesC returned negative start");
    if (start <= index && index < stop) {
        *_index = index;
    } else {
        *_index = -1;
    }
    return yp_None;
}

// XXX Using ypSlice_AdjustIndicesC assumes we don't have ranges longer than yp_SSIZE_T_MAX
static ypObject *range_count(
        ypObject *r, ypObject *x, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t *count)
{
    yp_ssize_t step = 1;     // won't actually change
    yp_ssize_t slicelength;  // unnecessary
    yp_ssize_t index;
    ypObject  *result = _ypRange_find(r, x, &index);
    if (yp_isexceptionC(result)) return result;

    result = ypSlice_AdjustIndicesC(ypRange_LEN(r), &start, &stop, &step, &slicelength);
    if (yp_isexceptionC(result)) return result;

    // This assertion assures that index==-1 (ie item not in range) won't be confused
    yp_ASSERT(start >= 0, "ypSlice_AdjustIndicesC returned negative start");
    if (start <= index && index < stop) {
        *count = 1;
    } else {
        *count = 0;
    }
    return yp_None;
}

static ypObject *range_len(ypObject *r, yp_ssize_t *len)
{
    *len = ypRange_LEN(r);
    return yp_None;
}

static ypObject *range_eq(ypObject *r, ypObject *x)
{
    if (r == x) return yp_True;
    if (ypObject_TYPE_PAIR_CODE(x) != ypRange_CODE) return yp_ComparisonNotImplemented;
    ypRange_ASSERT_NORMALIZED(r);
    ypRange_ASSERT_NORMALIZED(x);
    return ypBool_FROM_C(ypRange_ARE_EQUAL(r, x));
}
static ypObject *range_ne(ypObject *r, ypObject *x)
{
    ypObject *result = range_eq(r, x);
    return ypBool_NOT(result);
}

/* Hash function for range objects. Rough C equivalent of
   if not len(r):
       return hash((len(r), 0, 1))
   if len(r) == 1:
       return hash((len(r), r.start, 1))
   return hash((len(r), r.start, r.step))
*/
// XXX Where Python uses None for start if len<1, we use 0, and where it uses None for step if
// len<2, we use 1
// XXX Adapted from Python's tuplehash and range_hash
static ypObject *range_currenthash(
        ypObject *r, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    yp_HashSequence_state_t state;
    ypRange_ASSERT_NORMALIZED(r);

    yp_HashSequence_init(&state, 3);
    yp_HashSequence_next(&state, yp_HashInt(ypRange_LEN(r)));
    yp_HashSequence_next(&state, yp_HashInt(ypRange_START(r)));
    yp_HashSequence_next(&state, yp_HashInt(ypRange_STEP(r)));
    // Since we never contain mutable objects, we can cache our hash
    *hash = ypObject_CACHED_HASH(yp_None) = yp_HashSequence_fini(&state);

    return yp_None;
}

static ypObject *range_dealloc(ypObject *r, void *memo)
{
    ypMem_FREE_FIXED(r);
    return yp_None;
}

static ypObject *range_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    ypObject *exc = yp_None;
    yp_int_t  start;
    yp_int_t  stop;
    yp_int_t  step;

    yp_ASSERT(n == 4, "unexpected argarray of length %" PRIssize, n);
    yp_ASSERT1(argarray[0] == yp_t_range);

    if (argarray[2] == yp_Arg_Missing) {
        start = 0;
        stop = yp_index_asintC(argarray[1], &exc);
    } else {
        start = yp_index_asintC(argarray[1], &exc);
        stop = yp_index_asintC(argarray[2], &exc);
    }
    step = yp_index_asintC(argarray[3], &exc);
    if (yp_isexceptionC(exc)) return exc;

    return yp_rangeC3(start, stop, step);
}

// If y is missing, start is 0 and stop is x; otherwise, start is x and stop is y.
yp_IMMORTAL_FUNCTION_static(range_func_new, range_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_x), NULL},
                {yp_CONST_REF(yp_s_y), yp_CONST_REF(yp_Arg_Missing)},
                {yp_CONST_REF(yp_s_step), ypInt_PREALLOC_REF(1)},
                {yp_CONST_REF(yp_s_slash), NULL}));

static ypSequenceMethods ypRange_as_sequence = {
        MethodError_objobjproc,       // tp_concat
        MethodError_objssizeproc,     // tp_repeat
        range_getindex,               // tp_getindex
        range_getslice,               // tp_getslice
        range_find,                   // tp_find
        range_count,                  // tp_count
        MethodError_objssizeobjproc,  // tp_setindex
        MethodError_objsliceobjproc,  // tp_setslice
        MethodError_objssizeproc,     // tp_delindex
        MethodError_objsliceproc,     // tp_delslice
        MethodError_objobjproc,       // tp_append
        MethodError_objobjproc,       // tp_extend
        MethodError_objssizeproc,     // tp_irepeat
        MethodError_objssizeobjproc,  // tp_insert
        MethodError_objssizeproc,     // tp_popindex
        MethodError_objproc,          // tp_reverse
        MethodError_objobjobjproc     // tp_sort
};

static ypTypeObject ypRange_Type = {
        yp_TYPE_HEAD_INIT,
        0,     // tp_flags
        NULL,  // tp_name

        // Object fundamentals
        yp_CONST_REF(range_func_new),  // tp_func_new
        range_dealloc,                 // tp_dealloc
        NoRefs_traversefunc,           // tp_traverse
        NULL,                          // tp_str
        NULL,                          // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,    // tp_freeze
        range_frozen_copy,      // tp_unfrozen_copy
        range_frozen_copy,      // tp_frozen_copy
        range_frozen_deepcopy,  // tp_unfrozen_deepcopy
        range_frozen_deepcopy,  // tp_frozen_deepcopy
        MethodError_objproc,    // tp_invalidate

        // Boolean operations and comparisons
        range_bool,                  // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        range_eq,                    // tp_eq
        range_ne,                    // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        range_currenthash,    // tp_currenthash
        MethodError_objproc,  // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        _ypSequence_miniiter,       // tp_miniiter
        _ypSequence_miniiter_rev,   // tp_miniiter_reversed
        _ypSequence_miniiter_next,  // tp_miniiter_next
        _ypSequence_miniiter_lenh,  // tp_miniiter_length_hint
        _ypIter_fromminiiter,       // tp_iter
        _ypIter_fromminiiter_rev,   // tp_iter_reversed
        TypeError_objobjproc,       // tp_send

        // Container operations
        range_contains,             // tp_contains
        range_len,                  // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        _ypSequence_getdefault,     // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        &ypRange_as_sequence,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        TypeError_CallableMethods  // tp_as_callable
};

// XXX Adapted from Python's get_len_of_range
ypObject *yp_rangeC3(yp_int_t start, yp_int_t stop, yp_int_t step)
{
    _yp_uint_t ulen;
    ypObject  *newR;

    // Denying yp_INT_T_MIN ensures step can be negated
    if (step == 0) return yp_ValueError;
    if (step == yp_INT_T_MIN) return yp_SystemLimitationError;

    /* -------------------------------------------------------------
    If step > 0 and lo >= hi, or step < 0 and lo <= hi, the range is empty.
    Else for step > 0, if n values are in the range, the last one is
    lo + (n-1)*step, which must be <= hi-1.  Rearranging,
    n <= (hi - lo - 1)/step + 1, so taking the floor of the RHS gives
    the proper value.  Since lo < hi in this case, hi-lo-1 >= 0, so
    the RHS is non-negative and so truncation is the same as the
    floor.  Letting M be the largest positive long, the worst case
    for the RHS numerator is hi=M, lo=-M-1, and then
    hi-lo-1 = M-(-M-1)-1 = 2*M.  Therefore unsigned long has enough
    precision to compute the RHS exactly.  The analysis for step < 0
    is similar.
    ---------------------------------------------------------------*/
    // Cast to unsigned to avoid undefined behaviour (a la yp_UINT_MATH)
    if (step < 0) {
        if (stop >= start) return yp_range_empty;
        ulen = ((_yp_uint_t)start) - 1u - ((_yp_uint_t)stop);
        ulen = 1u + ulen / ((_yp_uint_t)-step);
    } else {
        if (start >= stop) return yp_range_empty;
        ulen = ((_yp_uint_t)stop) - 1u - ((_yp_uint_t)start);
        ulen = 1u + ulen / ((_yp_uint_t)step);
    }
    // TODO We could store len in our own _yp_uint_t field, to allow for larger ranges, but a lot
    // of other code would also have to change
    if (ulen > ((_yp_uint_t)ypObject_LEN_MAX)) return yp_SystemLimitationError;
    if (ulen < 1) return yp_range_empty;
    if (ulen < 2) step = 1;  // makes comparisons easier

    newR = ypMem_MALLOC_FIXED(ypRangeObject, ypRange_CODE);
    if (yp_isexceptionC(newR)) return newR;
    ypRange_START(newR) = start;
    ypRange_STEP(newR) = step;
    ypRange_SET_LEN(newR, ulen);
    ypRange_ASSERT_NORMALIZED(newR);
    return newR;
}

ypObject *yp_rangeC(yp_int_t stop) { return yp_rangeC3(0, stop, 1); }

#pragma endregion range


/*************************************************************************************************
 * Functions as objects
 *************************************************************************************************/
#pragma region function

// FIXME be sure I'm using "parameter" and "argument" in the right places
// TODO Inspect and consider where yp_ssize_t is used vs int (as in `int n`)
// TODO Make sub exceptions of yp_TypeError for each type of argument error (perhaps all grouped
// under yp_ArgumentError or yp_CallArgumentError or something).
// FIXME Stay consistent: https://docs.python.org/3/library/inspect.html#inspect.signature

// Caches specific information about the function (in particular its parameter list).
#define ypFunction_FLAG_VALIDATED (1u << 0)            // _ypFunction_validate_parameters succeeded
#define ypFunction_FLAG_HAS_POS_ONLY (1u << 1)         // Has positional-only parameter(s)
#define ypFunction_FLAG_HAS_MULTI_POS_ONLY (1u << 2)   // Has >1 pos-only (requires POS_ONLY)
#define ypFunction_FLAG_HAS_POS_OR_KW (1u << 3)        // Has positional-or-keyword parameter(s)
#define ypFunction_FLAG_HAS_MULTI_POS_OR_KW (1u << 4)  // Has >1 pos-or-kw (requires POS_OR_KW)
#define ypFunction_FLAG_HAS_VAR_POS (1u << 5)          // Has *args parameter
#define ypFunction_FLAG_HAS_KW_ONLY (1u << 6)          // Has keyword-only parameter(s)
#define ypFunction_FLAG_HAS_VAR_KW (1u << 7)           // Has **kwargs parameter

// The ypFunction_FLAGS that summarize the behaviour of the parameters.
#define ypFunction_PARAM_FLAGS                                                    \
    (ypFunction_FLAG_HAS_POS_ONLY | ypFunction_FLAG_HAS_MULTI_POS_ONLY |          \
            ypFunction_FLAG_HAS_POS_OR_KW | ypFunction_FLAG_HAS_MULTI_POS_OR_KW | \
            ypFunction_FLAG_HAS_VAR_POS | ypFunction_FLAG_HAS_KW_ONLY |           \
            ypFunction_FLAG_HAS_VAR_KW)

// True if the function takes no parameters.
#define ypFunction_NO_PARAMETERS(param_flags) ((param_flags) == 0)

// True if the function has parameters and they are all positional-only.
#define ypFunction_HAS_ONLY_POS_ONLY(param_flags) \
    (((param_flags) & ~ypFunction_FLAG_HAS_MULTI_POS_ONLY) == ypFunction_FLAG_HAS_POS_ONLY)

// True if the function has parameters and they are all positional-or-keyword.
#define ypFunction_HAS_ONLY_POS_OR_KW(param_flags) \
    (((param_flags) & ~ypFunction_FLAG_HAS_MULTI_POS_OR_KW) == ypFunction_FLAG_HAS_POS_OR_KW)

// True if we can bypass call_QuickIter and directly populate argarray with the args_len positional
// arguments. This optimization depends on the special handling of parameter lists that end in /: we
// drop the trailing NULL from argarray, such that n is one less than the number of parameters.
#define ypFunction_IS_POSITIONAL_MATCH(param_flags, params_len, args_len)          \
    ((ypFunction_HAS_ONLY_POS_OR_KW(param_flags) && (params_len) == (args_len)) || \
            (ypFunction_HAS_ONLY_POS_ONLY(param_flags) && (params_len)-1 == (args_len)))

// True if function is exactly (*args, **kwargs).
#define ypFunction_IS_VAR_POS_VAR_KW(param_flags) \
    ((param_flags) == (ypFunction_FLAG_HAS_VAR_POS | ypFunction_FLAG_HAS_VAR_KW))

// True if function is exactly (a, *args, **kwargs).
#define ypFunction_IS_PARAM_VAR_POS_VAR_KW(param_flags)                              \
    ((param_flags) == (ypFunction_FLAG_HAS_POS_OR_KW | ypFunction_FLAG_HAS_VAR_POS | \
                              ypFunction_FLAG_HAS_VAR_KW))

// True if function is exactly (a, /, *args, **kwargs).
#define ypFunction_IS_PARAM_SLASH_VAR_POS_VAR_KW(param_flags)                       \
    ((param_flags) == (ypFunction_FLAG_HAS_POS_ONLY | ypFunction_FLAG_HAS_VAR_POS | \
                              ypFunction_FLAG_HAS_VAR_KW))

typedef struct {
    // objlocs: bit n is 1 if (n*yp_sizeof(ypObject *)) is the offset of an object in data
    yp_uint32_t objlocs;
    yp_int32_t  size;
    // Note that we are 8-byte aligned here on both 32- and 64-bit systems
    yp_uint8_t data[];
} ypFunctionState;
yp_STATIC_ASSERT(
        yp_offsetof(ypFunctionState, data) % yp_MAX_ALIGNMENT == 0, alignof_function_state_data);

#define ypFunction_FLAGS(f) (((ypObject *)(f))->ob_type_flags)
#define ypFunction_STATE(f) ((ypFunctionState *)((ypFunctionObject *)(f))->ob_state)
#define ypFunction_SET_STATE(f, state) \
    (((ypFunctionObject *)(f))->ob_state = (ypFunctionState *)(state))
#define ypFunction_PARAMS(f) ((yp_parameter_decl_t *)((ypObject *)(f))->ob_data)
#define ypFunction_PARAMS_LEN ypObject_CACHED_LEN
#define ypFunction_SET_PARAMS_LEN ypObject_SET_CACHED_LEN
#define ypFunction_CODE_FUNC(f) (((ypFunctionObject *)(f))->ob_code)

// The maximum possible size of a function's state
// #define ypFunction_STATE_SIZE_MAX ((yp_ssize_t)0x7FFFFFFF)

// The maximum possible number of parameters for a function
// FIXME This alloclen_max/len_max separation is not useful for most types
#define ypFunction_ALLOCLEN_MAX                                                              \
    ((yp_ssize_t)MIN(                                                                        \
            (yp_SSIZE_T_MAX - yp_sizeof(ypFunctionObject)) / yp_sizeof(yp_parameter_decl_t), \
            ypObject_LEN_MAX))
#define ypFunction_LEN_MAX ypFunction_ALLOCLEN_MAX

// The largest argarray that we will allocate on the stack.
#define ypFunction_MAX_ARGS_ON_STACK 32

// For use internally to detect when a key is missing from a dict.
yp_IMMORTAL_INVALIDATED(ypFunction_key_missing);


// Returns an immortal representing the kind of parameter according to yp_parameter_decl_t.name,
// or an exception. Performs only minimal validation; in particular, this does not validate that the
// parameter name is a proper identifier. Use _ypFunction_validate_parameters to fully validate the
// parameter list.
static ypObject *_ypFunction_parameter_kind(ypObject *name)
{
    yp_ssize_t                name_len;
    const void               *name_data;
    ypStringLib_getindexXfunc getindexX;

    if (ypObject_TYPE_CODE(name) != ypStr_CODE) {
        return_yp_BAD_TYPE(name);
    }

    name_len = ypStr_LEN(name);
    name_data = ypStr_DATA(name);
    getindexX = ypStr_ENC(name)->getindexX;
    if (name_len < 1) {
        return yp_ParameterSyntaxError;
    } else if (name_len == 1) {
        yp_uint32_t ch = getindexX(name_data, 0);
        if (ch == '/') {
            return yp_s_slash;
        } else if (ch == '*') {
            return yp_s_star;
        } else {
            return yp_None;  // just a regular parameter
        }
    } else {
        // TODO Python allows `* args` and `** kwargs`. However, we can probably reject this.
        if (getindexX(name_data, 0) != '*') {
            return yp_None;  // just a regular parameter
        } else if (getindexX(name_data, 1) != '*') {
            return yp_s_star_args;
        } else if (name_len < 3) {
            return yp_ParameterSyntaxError;
        } else {
            return yp_s_star_star_kwargs;
        }
    }
}

// FIXME provide a way for code to trigger this during their initialization, before calling the obj.
static ypObject *_ypFunction_validate_parameters(ypObject *f)
{
    yp_ssize_t params_len = ypFunction_PARAMS_LEN(f);
    yp_ssize_t i;

    int n_positional_only = 0;
    int n_positional_or_keyword = 0;
    int has_var_positional = FALSE;
    int has_keyword_only = FALSE;
    int has_var_keyword = FALSE;

    int remaining_are_keyword_only = FALSE;  // all after * or *args are kw-only
    int must_have_default = FALSE;           // if a parameter has a default, all until * must also

    ypObject *param_names;       // a set used to detect duplicate names
    ypObject *result = yp_None;  // set to exception on error

    yp_ASSERT1(!(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED));  // need only be called once

    if (params_len < 1) {
        ypFunction_FLAGS(f) |= ypFunction_FLAG_VALIDATED;
        return yp_None;
    }

    // FIXME We could give yp_set a hint as to how big this will be.
    param_names = yp_setN(0);  // new ref
    for (i = 0; i < params_len; i++) {
        yp_parameter_decl_t param = ypFunction_PARAMS(f)[i];
        ypObject           *param_kind = _ypFunction_parameter_kind(param.name);
        ypObject           *param_name = NULL;  // actual name, stripping leading * or **

        if (param_kind == yp_s_slash) {
            if (n_positional_or_keyword < 1 || n_positional_only > 0 ||
                    remaining_are_keyword_only) {
                // Invalid: (/), (a, /, /), (*, /), (a, *, /), (*, a, /), (*args, /)
                result = yp_ParameterSyntaxError;
                break;
            } else if (param.default_ != NULL) {
                result = yp_ParameterSyntaxError;
                break;
            }
            // The previous positional-or-keyword arguments were actually positional-only.
            n_positional_only = n_positional_or_keyword;
            n_positional_or_keyword = 0;

        } else if (param_kind == yp_s_star || param_kind == yp_s_star_args) {
            if (remaining_are_keyword_only) {
                // Invalid: (*, *, a), (*, *args), (*args, *, a), (*args, *args)
                result = yp_ParameterSyntaxError;
                break;
            } else if (param.default_ != NULL) {
                result = yp_ParameterSyntaxError;
                break;
            }
            remaining_are_keyword_only = TRUE;
            must_have_default = FALSE;
            if (param_kind == yp_s_star_args) {
                has_var_positional = TRUE;
                param_name = str_getslice(param.name, 1, yp_SLICE_LAST, 1);  // new ref
            }

        } else if (param_kind == yp_s_star_star_kwargs) {
            if (i != params_len - 1) {
                // Invalid: (**kwargs, a), (**kwargs, /), (**kwargs, *, a), (**kwargs, *args),
                // (**kwargs, **kwargs)
                result = yp_ParameterSyntaxError;
                break;
            } else if (param.default_ != NULL) {
                result = yp_ParameterSyntaxError;
                break;
            }
            has_var_keyword = TRUE;
            param_name = str_getslice(param.name, 2, yp_SLICE_LAST, 1);  // new ref

        } else if (param_kind == yp_None) {
            if (remaining_are_keyword_only) {
                has_keyword_only = TRUE;
            } else {
                if (param.default_ != NULL) {
                    must_have_default = TRUE;
                } else if (must_have_default) {
                    result = yp_ParameterSyntaxError;
                    break;
                }
                n_positional_or_keyword += 1;
            }
            param_name = yp_incref(param.name);  // new ref

        } else {
            yp_ASSERT(yp_isexceptionC(param_kind), "unexpected return from parameter_kind");
            result = yp_isexceptionC(param_kind) ? param_kind : yp_SystemError;
            break;
        }

        if (param_name != NULL) {
            // TODO: Implement str_isidentifier, then enable this.
            // result = str_isidentifier(param_name);
            // if (result != yp_True) {
            //     // Invalid: (1), (*1), (**1)
            //     if (result == yp_False) result = yp_ParameterSyntaxError;
            //     yp_ASSERT(yp_isexceptionC(result), "unexpected return from str_isidentifier");
            //     yp_decref(param_name);
            //     break;
            // }

            result = set_pushunique(param_names, param_name);
            if (result != yp_None) {
                // Invalid: (a, a), (a, *a), (a, **a), (*a, a), (*a, **a)
                if (yp_isexceptionC2(result, yp_KeyError)) result = yp_ParameterSyntaxError;
                yp_ASSERT(yp_isexceptionC(result), "unexpected return from set_pushunique");
                yp_decref(param_name);
                break;
            }
            yp_decref(param_name);
        }
    }
    yp_decref(param_names);
    if (result != yp_None) {
        yp_ASSERT(yp_isexceptionC(result), "result set to non-exception");
        return yp_isexceptionC(result) ? result : yp_SystemError;
    }

    if (remaining_are_keyword_only && !has_var_positional && !has_keyword_only) {
        // Invalid: (*), (*, **kwargs) (named arguments must follow bare *)
        return yp_ParameterSyntaxError;
    }

    if (n_positional_only > 0) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_POS_ONLY;
    if (n_positional_only > 1) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_MULTI_POS_ONLY;
    if (n_positional_or_keyword > 0) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_POS_OR_KW;
    if (n_positional_or_keyword > 1) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_MULTI_POS_OR_KW;
    if (has_var_positional) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_VAR_POS;
    if (has_keyword_only) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_KW_ONLY;
    if (has_var_keyword) ypFunction_FLAGS(f) |= ypFunction_FLAG_HAS_VAR_KW;

    ypFunction_FLAGS(f) |= ypFunction_FLAG_VALIDATED;
    return yp_None;
}

// Calls yp_decref on the n objects in argarray, skipping nulls.
static void _ypFunction_call_decref_argarray(yp_ssize_t n, ypObject **argarray)
{
    for (/* n already set*/; n > 0; n--) {
        ypObject *arg = argarray[n - 1];
        if (arg != NULL) yp_decref(arg);  // / and * store NULL in argarray
    }
}

// Helper for _ypFunction_call_QuickIter. Places the positional arguments in argarray. Keeps *n
// up-to-date with the number of references in argarray, so they can be deallocated by
// _ypFunction_call_QuickIter after the call. Sets *slash_index to the position of /, if present (/
// is always consumed by this function).
static ypObject *_ypFunction_call_place_args(ypObject *f, const ypQuickIter_methods *iter,
        ypQuickIter_state *args, yp_ssize_t *n, ypObject **argarray, yp_ssize_t *slash_index)
{
    int       is_positional_only = ypFunction_FLAGS(f) & ypFunction_FLAG_HAS_POS_ONLY;
    ypObject *arg;

    while (*n < ypFunction_PARAMS_LEN(f)) {
        yp_parameter_decl_t param = ypFunction_PARAMS(f)[*n];
        ypObject           *param_kind = _ypFunction_parameter_kind(param.name);

        if (param_kind == yp_s_slash) {
            is_positional_only = FALSE;
            *slash_index = *n;
            argarray[*n] = NULL;  // a placeholder so argarray[i] corresponds to params[i]
            (*n)++;               // consume the / parameter

        } else if (param_kind == yp_s_star) {
            argarray[*n] = NULL;  // a placeholder so argarray[i] corresponds to params[i]
            (*n)++;               // consume the * parameter
            break;                // will ensure no remaining positional arguments

        } else if (param_kind == yp_s_star_args) {
            arg = iter->remaining_as_tuple(args);  // new ref
            if (yp_isexceptionC(arg)) return arg;
            argarray[*n] = arg;
            (*n)++;          // consume the parameter
            return yp_None;  // end of positional arguments

        } else if (param_kind == yp_s_star_star_kwargs) {
            // do *not* consume the **kwargs parameter
            break;  // will ensure no remaining positional arguments

        } else if (param_kind == yp_None) {
            arg = iter->next(args);  // new ref
            if (arg == NULL) {
                if (!is_positional_only) {
                    // Do *not* consume the parameter: it may be in kwargs.
                    return yp_None;  // end of positional arguments
                } else if (param.default_ != NULL) {
                    argarray[*n] = yp_incref(param.default_);  // can be an exception
                    (*n)++;                                    // consume the parameter
                } else {
                    // A positional-only parameter without a matching argument or a default.
                    return yp_TypeError;
                }
            } else if (yp_isexceptionC(arg)) {
                return arg;
            } else {
                argarray[*n] = arg;
                (*n)++;  // consume the parameter
            }

        } else {
            yp_ASSERT(yp_isexceptionC(param_kind), "unexpected return from parameter_kind");
            return yp_isexceptionC(param_kind) ? param_kind : yp_SystemError;
        }
    }

    // Ensure we have exhausted all the positional arguments.
    arg = iter->nextX(args);
    if (arg != NULL) {
        return yp_isexceptionC(arg) ? arg : yp_TypeError;
    }

    return yp_None;
}

// Helper for _ypFunction_call_QuickIter. Returns an exact copy of kwargs as a frozendict, to be
// assigned to the **kwargs parameter. Raises yp_TypeError if kwargs contains non-string keys.
// kwargs must be a mapping object. If kwargs_is_copy is true, kwargs is frozen in-place, saving a
// copy.
static ypObject *_ypFunction_call_copy_var_kwargs(ypObject *kwargs, int kwargs_is_copy)
{
    ypObject   *result;
    ypObject   *mi;
    yp_uint64_t mi_state;

    yp_ASSERT1(ypObject_IS_MAPPING(kwargs));

    if (kwargs_is_copy) {
        yp_ASSERT1(ypObject_TYPE_PAIR_CODE(kwargs) == ypFrozenDict_CODE);
        // TODO Implement frozendict_freeze and freeze kwargs in-place here.
        result = yp_frozendict(kwargs);
        if (yp_isexceptionC(result)) return result;
    } else {
        result = yp_frozendict(kwargs);
        if (yp_isexceptionC(result)) return result;
    }

    // Validate that all the keys are strings.
    // TODO Make a slightly-more-optimized version of frozendict_miniiter_keys for internal use?
    mi = frozendict_miniiter_keys(result, &mi_state);
    while (1) {
        ypObject *key = frozendict_miniiter_next(mi, &mi_state);
        // TODO Allow subclasses of str.
        if (ypObject_TYPE_CODE(key) != ypStr_CODE) {
            if (yp_isexceptionC2(key, yp_StopIteration)) break;

            // An exception happened or the key is not a str: replace the result with an exception.
            yp_decref(result);
            result = yp_BAD_TYPE(key);
            yp_decref(key);
            break;
        }
        yp_decref(key);
    }
    yp_decref(mi);

    return result;
}

// Helper for _ypFunction_call_QuickIter. Determines **kwargs by copying kwargs, dropping keyword
// arguments we've already placed, freezing it, and returning a new reference. slash_index is the
// index of /, or -1 if not present. first_kwarg is the position of the first parameter filled by a
// keyword argument. If kwargs_is_copy is true, kwargs is modified directly, saving a copy.
static ypObject *_ypFunction_call_make_var_kwargs(ypObject *f, yp_ssize_t slash_index,
        yp_ssize_t first_kwarg, yp_ssize_t placed_kwargs, ypObject *kwargs, int kwargs_is_copy)
{
    yp_parameter_decl_t param;
    ypObject           *param_kind;
    ypObject           *result;
    yp_ssize_t          i = 0;
    ypObject           *kwargs_copy;

    yp_ASSERT1(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED);
    yp_ASSERT1(ypFunction_FLAGS(f) & ypFunction_FLAG_HAS_VAR_KW);
    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(kwargs) == ypFrozenDict_CODE);

    // If we consumed all the keyword arguments, then **kwargs will be empty. Recall we can trust
    // that there are no duplicate names in params, so we know there were no arguments that were
    // both positional and keyword.
    if (placed_kwargs >= ypDict_LEN(kwargs)) return yp_frozendict_empty;

    // Skip any positional-only parameters: function (a, /, **kwargs) can be called like (1, a=33).
    if (ypFunction_FLAGS(f) & ypFunction_FLAG_HAS_POS_ONLY) {
        yp_ASSERT1(slash_index >= 0);
        i = slash_index + 1;  // start at the param after /, the first kw param
    }

    // Ensure that there were no arguments that were both positional and keyword. At this point,
    // kwargs may still be a frozendict, so use yp_contains. Without a **kwargs,
    // _ypFunction_call_place_kwargs uses placed_kwargs to detect this case.
    for (/*i already set*/; i < first_kwarg; i++) {
        param = ypFunction_PARAMS(f)[i];
        param_kind = _ypFunction_parameter_kind(param.name);
        if (yp_isexceptionC(param_kind)) return param_kind;
        if (param_kind != yp_None) continue;  // skip *, *args, and **kwargs (/ already skipped)

        result = frozendict_contains(kwargs, param.name);
        if (result == yp_False) continue;
        if (yp_isexceptionC(result)) return result;

        return yp_TypeError;  // a matching keyword argument that was also positional
    }

    // If we did not consume any keyword arguments, then **kwargs will be an exact copy.
    if (placed_kwargs < 1) return _ypFunction_call_copy_var_kwargs(kwargs, kwargs_is_copy);

    // We need to modify then freeze kwargs, so make a copy of it, unless it is *already* a copy.
    // (We coerce user-defined mapping types to dict, which is already an object we can modify.)
    if (kwargs_is_copy) {
        yp_ASSERT1(ypObject_TYPE_CODE(kwargs) == ypDict_CODE);
        kwargs_copy = yp_incref(kwargs);
    } else {
        kwargs_copy = yp_dict(kwargs);
        if (yp_isexceptionC(kwargs_copy)) return kwargs_copy;
    }

    // Remove the keyword arguments we have already placed. Remember to decref kwargs_copy on error.
    for (/*i already set*/; i < ypFunction_PARAMS_LEN(f); i++) {
        param = ypFunction_PARAMS(f)[i];
        param_kind = _ypFunction_parameter_kind(param.name);
        if (yp_isexceptionC(param_kind)) {
            yp_decref(kwargs_copy);
            return param_kind;
        }
        if (param_kind != yp_None) continue;  // skip *, *args, and **kwargs (/ already skipped)

        result = dict_popvalue(kwargs_copy, param.name, ypFunction_key_missing);
        if (yp_isexceptionC(result)) {
            yp_decref(kwargs_copy);
            return result;
        }
        yp_decref(result);
    }

    result = _ypFunction_call_copy_var_kwargs(kwargs_copy, /*kwargs_is_copy=*/TRUE);
    yp_decref(kwargs_copy);
    return result;
}

// Helper for _ypFunction_call_QuickIter. Places the keyword arguments in argarray. Keeps *n
// up-to-date with the number of references in argarray, so they can be deallocated by
// _ypFunction_call_QuickIter after the call. slash_index is the index of /, or -1 if not present.
// kwargs must be a dict/frozendict; additonally, if we have a **kwargs parameter, kwargs must be a
// dict, and it will be modified, frozen, and placed in argarray.
static ypObject *_ypFunction_call_place_kwargs(ypObject *f, yp_ssize_t slash_index,
        ypObject *kwargs, int kwargs_is_copy, yp_ssize_t *n, ypObject **argarray)
{
    ypObject  *arg;
    yp_ssize_t first_kwarg = *n;   // remembers the position of the first param filled by us
    yp_ssize_t placed_kwargs = 0;  // counts how many arguments in kwargs we have consumed

    yp_ASSERT1(ypObject_TYPE_PAIR_CODE(kwargs) == ypFrozenDict_CODE);

    while (*n < ypFunction_PARAMS_LEN(f)) {
        yp_parameter_decl_t param = ypFunction_PARAMS(f)[*n];
        ypObject           *param_kind = _ypFunction_parameter_kind(param.name);

        // / was already consumed by _ypFunction_call_place_args.
        if (param_kind == yp_s_star) {
            argarray[*n] = NULL;  // a placeholder so argarray[i] corresponds to params[i]
            (*n)++;               // consume the * parameter

        } else if (param_kind == yp_s_star_args) {
            argarray[*n] = yp_tuple_empty;  // there must not have been any more positional args
            (*n)++;                         // consume the *args parameter

        } else if (param_kind == yp_s_star_star_kwargs) {
            yp_ASSERT(*n == ypFunction_PARAMS_LEN(f) - 1,
                    "_ypFunction_validate_parameters didn't ensure that **kwargs came last");
            arg = _ypFunction_call_make_var_kwargs(
                    f, slash_index, first_kwarg, placed_kwargs, kwargs, kwargs_is_copy);
            if (yp_isexceptionC(arg)) return arg;
            argarray[*n] = arg;
            (*n)++;          // consume the parameter
            return yp_None;  // **kwarg, if present, is always last

        } else if (param_kind == yp_None) {
            arg = frozendict_getdefault(kwargs, param.name, ypFunction_key_missing);  // new ref
            if (yp_isexceptionC(arg)) return arg;
            if (arg != ypFunction_key_missing) {
                argarray[*n] = arg;
                (*n)++;  // consume the parameter
                placed_kwargs += 1;
            } else if (param.default_ != NULL) {
                argarray[*n] = yp_incref(param.default_);  // can be an exception
                (*n)++;                                    // consume the parameter
            } else {
                // A parameter without a matching argument or a default.
                return yp_TypeError;
            }

        } else {
            yp_ASSERT(param_kind != yp_s_slash,
                    "/ should have been consumed by _ypFunction_call_place_args");
            yp_ASSERT(yp_isexceptionC(param_kind), "unexpected return from parameter_kind");
            return yp_isexceptionC(param_kind) ? param_kind : yp_SystemError;
        }
    }

    yp_ASSERT1(!(ypFunction_FLAGS(f) & ypFunction_FLAG_HAS_VAR_KW));
    if (placed_kwargs != ypDict_LEN(kwargs)) {
        // Unexpected keyword arguments (including non-string keys), or an argument was both
        // positional and keyword.
        return yp_TypeError;
    }

    return yp_None;
}

// Helper for _ypFunction_call_QuickIter. Keeps *n up-to-date with the number of references in
// argarray, so they can be deallocated by _ypFunction_call_QuickIter after the call.
static ypObject *_ypFunction_call_QuickIter_inner(ypObject *f, const ypQuickIter_methods *iter,
        ypQuickIter_state *args, ypObject *kwargs, yp_ssize_t *n, ypObject **argarray)
{
    ypObject  *result;
    yp_ssize_t slash_index = -1;  // index of /, or -1 if not present

    yp_ASSERT1(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED);

    // Positional arguments are placed first.
    result = _ypFunction_call_place_args(f, iter, args, n, argarray, &slash_index);
    if (yp_isexceptionC(result)) return result;
    yp_ASSERT1((ypFunction_FLAGS(f) & ypFunction_FLAG_HAS_POS_ONLY && slash_index >= 0) ||
               slash_index == -1);

    if (ypObject_TYPE_PAIR_CODE(kwargs) == ypFrozenDict_CODE) {
        result = _ypFunction_call_place_kwargs(
                f, slash_index, kwargs, /*kwargs_is_copy=*/FALSE, n, argarray);
        if (yp_isexceptionC(result)) return result;
    } else if (ypObject_IS_MAPPING(kwargs)) {
        // We can't trust user-defined mapping types here (and neither does Python). Consider a
        // case-insensitive mapping {'a': 1} with a function (a, A). Even though the mapping
        // contains just one entry, it would match both arguments.
        ypObject *kwargs_copy = yp_dict(kwargs);
        if (yp_isexceptionC(kwargs_copy)) return kwargs_copy;
        result = _ypFunction_call_place_kwargs(
                f, slash_index, kwargs_copy, /*kwargs_is_copy=*/TRUE, n, argarray);
        yp_decref(kwargs_copy);
        if (yp_isexceptionC(result)) return result;
    } else {
        // Argument after ** must be a mapping.
        // XXX Here, Python just requires PyMapping_Keys() (i.e. .keys()) to be supported.
        return_yp_BAD_TYPE(kwargs);
    }

    yp_ASSERT1(*n == ypFunction_PARAMS_LEN(f));
    if (*n > 0 && argarray[*n - 1] == NULL) {
        // Drop the trailing null. This only happens if / is the last entry, which would mean all
        // params are positional-only.
        yp_ASSERT1(_ypFunction_parameter_kind(ypFunction_PARAMS(f)[*n - 1].name) == yp_s_slash);
        yp_ASSERT1(ypFunction_HAS_ONLY_POS_ONLY(ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS));
        yp_ASSERT1(*n > 1 && argarray[*n - 2] != NULL);
        (*n)--;
    }

    // TODO Protect against misbehaving code that modifies argarray? Anywhere we call code.
    return ypFunction_CODE_FUNC(f)(f, *n, *n > 0 ? argarray : NULL);
}

// If self is not NULL, it is "prepended" as a positional argument.
static ypObject *_ypFunction_call_QuickIter(ypObject *f, ypObject *self,
        const ypQuickIter_methods *iter, ypQuickIter_state *args, ypObject *kwargs)
{
    yp_uint8_t param_flags = ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS;
    yp_ssize_t params_len = ypFunction_PARAMS_LEN(f);
    yp_ssize_t n;
    ypObject  *storage[ypFunction_MAX_ARGS_ON_STACK];
    ypObject **argarray;
    ypObject  *result;

    yp_ASSERT1(ypObject_TYPE_CODE(f) == ypFunction_CODE);
    yp_ASSERT1(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED);

    if (params_len <= yp_lengthof_array(storage)) {
        argarray = storage;
    } else {
        yp_ssize_t allocsize;  // unused
        argarray = yp_malloc(&allocsize, params_len * yp_sizeof(ypObject *));
        if (argarray == NULL) return yp_MemoryError;
    }

    if (self == NULL) {
        n = 0;
        result = _ypFunction_call_QuickIter_inner(f, iter, args, kwargs, &n, argarray);

    } else if (param_flags & (ypFunction_FLAG_HAS_POS_ONLY | ypFunction_FLAG_HAS_POS_OR_KW)) {
        yp_ASSERT1(params_len > 0);
        yp_ASSERT1(_ypFunction_parameter_kind(ypFunction_PARAMS(f)[0].name) == yp_None);
        argarray[0] = yp_incref(self);
        n = 1;
        result = _ypFunction_call_QuickIter_inner(f, iter, args, kwargs, &n, argarray);

    } else if (param_flags & ypFunction_FLAG_HAS_VAR_POS) {
        yp_ASSERT1(params_len > 0);
        yp_ASSERT1(_ypFunction_parameter_kind(ypFunction_PARAMS(f)[0].name) == yp_s_star_args);
        yp_ASSERT(FALSE, "self with (*args, ...) not yet implemented");
        n = 0;
        result = yp_NotImplementedError;

    } else {
        // self is a positional arg, so incompatible with keyword-only or **kwargs parameters.
        n = 0;
        result = yp_TypeError;
    }

    _ypFunction_call_decref_argarray(n, argarray);
    if (argarray != storage) {
        yp_free(argarray);
    }
    return result;
}

// TODO Make a linter to enforce the pattern of name that's allowed to be used in other types. i.e.
// is ypFunction* the signifier that it's used directly in other types? When do I use ypFunction_ vs
// function_?

// Helper for ypFunction_callNV* that fills argarray with coerced args and kwargs. argarray may
// contain other entries, like self and possibly a NULL: they are considered borrowed.
static ypObject *_ypFunction_callNV_tostars(
        ypObject *f, int n, va_list args, ypObject **argarray, yp_ssize_t var_pos_i)
{
    yp_ssize_t var_kw_i = var_pos_i + 1;
    ypObject  *result;

    yp_ASSERT1(n >= 0);
    yp_ASSERT1(var_pos_i <= yp_SSIZE_T_MAX - 2);  // paranoia, as max value of var_pos_i is 2-ish

    argarray[var_pos_i] = yp_tupleNV(n, args);  // new ref
    if (yp_isexceptionC(argarray[var_pos_i])) {
        return argarray[var_pos_i];
    }

    argarray[var_kw_i] = yp_frozendict_empty;

    result = ypFunction_CODE_FUNC(f)(f, var_kw_i + 1, argarray);

    yp_decref(argarray[var_pos_i]);
    return result;
}

static ypObject *ypFunction_callNV(ypObject *f, int n, va_list args)
{
    yp_uint8_t param_flags;
    yp_ssize_t params_len = ypFunction_PARAMS_LEN(f);

    yp_ASSERT1(ypObject_TYPE_CODE(f) == ypFunction_CODE);
    yp_ASSERT1(n >= 0);

    // Function immortals must be validated at runtime.
    if (!(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED)) {
        ypObject *result = _ypFunction_validate_parameters(f);
        if (yp_isexceptionC(result)) return result;
    }
    param_flags = ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS;

    // XXX Resist temptation: only add special cases when it's easy AND common.
    if (ypFunction_NO_PARAMETERS(param_flags)) {
        yp_ASSERT1(params_len < 1);
        if (n > 0) return yp_TypeError;
        return ypFunction_CODE_FUNC(f)(f, 0, NULL);

    } else if (n <= ypFunction_MAX_ARGS_ON_STACK &&
               ypFunction_IS_POSITIONAL_MATCH(param_flags, params_len, n)) {
        int       i;
        ypObject *argarray[ypFunction_MAX_ARGS_ON_STACK];
        for (i = 0; i < n; i++) {
            ypObject *arg = argarray[i] = va_arg(args, ypObject *);  // borrowed
            if (yp_isexceptionC(arg)) return arg;
        }
        return ypFunction_CODE_FUNC(f)(f, n, argarray);

    } else if (ypFunction_IS_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[2];
        return _ypFunction_callNV_tostars(f, n, args, argarray, 0);

    } else {
        ypQuickIter_state state;
        ypObject         *result;
        ypQuickIter_new_fromvar(&state, MAX(n, 0), args);
        result = _ypFunction_call_QuickIter(
                f, NULL, &ypQuickIter_var_methods, &state, yp_frozendict_empty);
        ypQuickIter_var_close(&state);
        return result;
    }
}

// self is "prepended" to the arguments as a positional parameter.
static ypObject *ypFunction_callNV_withself(ypObject *f, ypObject *self, int n_args, va_list args)
{
    yp_uint8_t param_flags;
    yp_ssize_t params_len = ypFunction_PARAMS_LEN(f);
    yp_ssize_t n_actual;

    yp_ASSERT1(ypObject_TYPE_CODE(f) == ypFunction_CODE);
    yp_ASSERT1(!yp_isexceptionC(self));
    yp_ASSERT1(n_args >= 0);

    // Function immortals must be validated at runtime.
    if (!(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED)) {
        ypObject *result = _ypFunction_validate_parameters(f);
        if (yp_isexceptionC(result)) return result;
    }
    param_flags = ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS;

    n_actual = n_args + 1;
    if (n_actual < 0) {
        return yp_MemorySizeOverflowError;
    }

    // XXX Resist temptation: only add special cases when it's easy AND common.
    if (ypFunction_NO_PARAMETERS(param_flags)) {
        return yp_TypeError;

    } else if (n_actual <= ypFunction_MAX_ARGS_ON_STACK &&
               ypFunction_IS_POSITIONAL_MATCH(param_flags, params_len, n_actual)) {
        int       i;
        ypObject *argarray[ypFunction_MAX_ARGS_ON_STACK];
        argarray[0] = self;  // borrowed
        for (i = 0; i < n_args; i++) {
            ypObject *arg = argarray[i + 1] = va_arg(args, ypObject *);  // borrowed
            if (yp_isexceptionC(arg)) return arg;
        }
        return ypFunction_CODE_FUNC(f)(f, n_actual, argarray);

    } else if (ypFunction_IS_PARAM_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[3] = {self};  // only self is borrowed in argarray
        return _ypFunction_callNV_tostars(f, n_args, args, argarray, 1);

    } else if (ypFunction_IS_PARAM_SLASH_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[4] = {self, NULL};  // only self is borrowed in argarray
        return _ypFunction_callNV_tostars(f, n_args, args, argarray, 2);

    } else {
        ypQuickIter_state state;
        ypObject         *result;
        ypQuickIter_new_fromvar(&state, n_args, args);
        result = _ypFunction_call_QuickIter(
                f, self, &ypQuickIter_var_methods, &state, yp_frozendict_empty);
        ypQuickIter_var_close(&state);
        return result;
    }
}

// Helper for ypFunction_call_stars* that fills argarray with coerced args and kwargs. argarray may
// contain other entries, like self and possibly a NULL: they are considered borrowed.
static ypObject *_ypFunction_call_stars_tostars(
        ypObject *f, ypObject *args, ypObject *kwargs, ypObject **argarray, yp_ssize_t var_pos_i)
{
    yp_ssize_t var_kw_i = var_pos_i + 1;
    ypObject  *result;

    yp_ASSERT1(var_pos_i <= yp_SSIZE_T_MAX - 2);  // paranoia, as max value of var_pos_i is 2-ish

    if (!ypObject_IS_MAPPING(kwargs)) {
        // Argument after ** must be a mapping.
        // XXX Here, Python just requires PyMapping_Keys() (i.e. .keys()) to be supported.
        return_yp_BAD_TYPE(kwargs);
    }

    argarray[var_pos_i] = yp_tuple(args);  // new ref
    if (yp_isexceptionC(argarray[var_pos_i])) {
        return argarray[var_pos_i];
    }

    argarray[var_kw_i] =
            _ypFunction_call_copy_var_kwargs(kwargs, /*kwargs_is_copy=*/FALSE);  // new ref
    if (yp_isexceptionC(argarray[var_kw_i])) {
        yp_decref(argarray[var_pos_i]);
        return argarray[var_kw_i];
    }

    result = ypFunction_CODE_FUNC(f)(f, var_kw_i + 1, argarray);

    yp_decref(argarray[var_kw_i]);
    yp_decref(argarray[var_pos_i]);
    return result;
}

// TODO Python has str.format_map because str.format will always create a new dict; what if instead
// functions could be configured to accept any object for args/kwargs and pass through unchanged?
static ypObject *ypFunction_call_stars(ypObject *f, ypObject *args, ypObject *kwargs)
{
    yp_uint8_t param_flags;

    yp_ASSERT1(ypObject_TYPE_CODE(f) == ypFunction_CODE);

    // Function immortals must be validated at runtime.
    if (!(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED)) {
        ypObject *result = _ypFunction_validate_parameters(f);
        if (yp_isexceptionC(result)) return result;
    }
    param_flags = ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS;

    // XXX Resist temptation: only add special cases when it's easy AND common.
    // XXX In particular, don't use args' array directly: it might be deallocated during the call!
    if (ypFunction_IS_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[2];
        return _ypFunction_call_stars_tostars(f, args, kwargs, argarray, 0);

    } else {
        const ypQuickIter_methods *iter;
        ypQuickIter_state          state;
        ypObject                  *result = ypQuickIter_new_fromiterable(&iter, &state, args);
        if (yp_isexceptionC(result)) return result;
        result = _ypFunction_call_QuickIter(f, NULL, iter, &state, kwargs);
        iter->close(&state);
        return result;
    }
}

static ypObject *ypFunction_call_stars_withself(
        ypObject *f, ypObject *self, ypObject *args, ypObject *kwargs)
{
    yp_uint8_t param_flags;

    yp_ASSERT1(ypObject_TYPE_CODE(f) == ypFunction_CODE);
    yp_ASSERT1(!yp_isexceptionC(self));

    // Function immortals must be validated at runtime.
    if (!(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED)) {
        ypObject *result = _ypFunction_validate_parameters(f);
        if (yp_isexceptionC(result)) return result;
    }
    param_flags = ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS;

    // XXX Resist temptation: only add special cases when it's easy AND common.
    // XXX In particular, don't use args' array directly: it might be deallocated during the call!
    if (ypFunction_IS_PARAM_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[3] = {self};  // only self is borrowed in argarray
        return _ypFunction_call_stars_tostars(f, args, kwargs, argarray, 1);

    } else if (ypFunction_IS_PARAM_SLASH_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[4] = {self, NULL};  // only self is borrowed in argarray
        return _ypFunction_call_stars_tostars(f, args, kwargs, argarray, 2);

    } else {
        const ypQuickIter_methods *iter;
        ypQuickIter_state          state;
        ypObject                  *result = ypQuickIter_new_fromiterable(&iter, &state, args);
        if (yp_isexceptionC(result)) return result;
        result = _ypFunction_call_QuickIter(f, self, iter, &state, kwargs);
        iter->close(&state);
        return result;
    }
}

// Helper for ypFunction_call_array* that fills argarray with coerced args and kwargs. argarray may
// contain other entries, like self and possibly a NULL: they are considered borrowed.
static ypObject *_ypFunction_call_array_tostars(
        ypObject *f, yp_ssize_t n, ypObject *const *args, ypObject **argarray, yp_ssize_t var_pos_i)
{
    yp_ssize_t var_kw_i = var_pos_i + 1;
    ypObject  *result;

    yp_ASSERT1(n >= 0);
    yp_ASSERT1(var_pos_i <= yp_SSIZE_T_MAX - 2);  // paranoia, as max value of var_pos_i is 2-ish

    argarray[var_pos_i] = yp_tuple_fromarray(n, args);  // new ref
    if (yp_isexceptionC(argarray[var_pos_i])) {
        return argarray[var_pos_i];
    }

    argarray[var_kw_i] = yp_frozendict_empty;

    result = ypFunction_CODE_FUNC(f)(f, var_kw_i + 1, argarray);

    yp_decref(argarray[var_pos_i]);
    return result;
}

// There is no "withself" version: any self parameter should be included at args[0]. This is the
// optimization that explains yp_call_arrayX's odd signature.
static ypObject *ypFunction_call_array(ypObject *f, yp_ssize_t n, ypObject *const *args)
{
    yp_uint8_t param_flags;
    yp_ssize_t params_len = ypFunction_PARAMS_LEN(f);

    yp_ASSERT1(ypObject_TYPE_CODE(f) == ypFunction_CODE);
    yp_ASSERT1(n >= 0);

    // Function immortals must be validated at runtime.
    if (!(ypFunction_FLAGS(f) & ypFunction_FLAG_VALIDATED)) {
        ypObject *result = _ypFunction_validate_parameters(f);
        if (yp_isexceptionC(result)) return result;
    }
    param_flags = ypFunction_FLAGS(f) & ypFunction_PARAM_FLAGS;

    // XXX Resist temptation: only add special cases when it's easy AND common.
    // XXX We handle all three forms of *VAR_POS_VAR_KW here because we are also "withself".
    if (ypFunction_NO_PARAMETERS(param_flags)) {
        yp_ASSERT1(params_len < 1);
        if (n > 0) return yp_TypeError;
        return ypFunction_CODE_FUNC(f)(f, 0, NULL);

    } else if (ypFunction_IS_POSITIONAL_MATCH(param_flags, params_len, n)) {
        // This is why yp_call_arrayX exists: we can just pass the array on through (although we
        // first have to check for exceptions).
        yp_ssize_t i;
        for (i = 0; i < n; i++) {
            if (yp_isexceptionC(args[i])) return args[i];
        }
        return ypFunction_CODE_FUNC(f)(f, n, args);

    } else if (ypFunction_IS_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[2];
        return _ypFunction_call_array_tostars(f, n, args, argarray, 0);

    } else if (n > 0 && ypFunction_IS_PARAM_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[3] = {args[0]};  // only self (args[0]) is borrowed in argarray
        if (yp_isexceptionC(args[0])) return args[0];
        return _ypFunction_call_array_tostars(f, n - 1, args + 1, argarray, 1);

    } else if (n > 0 && ypFunction_IS_PARAM_SLASH_VAR_POS_VAR_KW(param_flags)) {
        ypObject *argarray[4] = {args[0], NULL};  // only self (args[0]) is borrowed in argarray
        if (yp_isexceptionC(args[0])) return args[0];
        return _ypFunction_call_array_tostars(f, n - 1, args + 1, argarray, 2);

    } else {
        ypQuickIter_state state;
        ypObject         *result;
        ypQuickIter_new_fromarray(&state, MAX(n, 0), args);
        result = _ypFunction_call_QuickIter(
                f, NULL, &ypQuickIter_array_methods, &state, yp_frozendict_empty);
        ypQuickIter_array_close(&state);
        return result;
    }
}


// Function methods

static ypObject *_function_traverse_state(ypObject *f, visitfunc visitor, void *memo)
{
    ypFunctionState *state = ypFunction_STATE(f);
    if (state == NULL) return yp_None;
    return _ypState_traverse(state->data, state->objlocs, visitor, memo);
}

static ypObject *_function_traverse_params(ypObject *f, visitfunc visitor, void *memo)
{
    yp_ssize_t i;
    for (i = 0; i < ypFunction_PARAMS_LEN(f); i++) {
        yp_parameter_decl_t param = ypFunction_PARAMS(f)[i];

        ypObject *result = visitor(param.name, memo);
        if (yp_isexceptionC(result)) return result;

        if (param.default_ != NULL) {
            result = visitor(param.default_, memo);
            if (yp_isexceptionC(result)) return result;
        }
    }
    return yp_None;
}

static ypObject *function_traverse(ypObject *f, visitfunc visitor, void *memo)
{
    ypObject *result = _function_traverse_state(f, visitor, memo);
    if (yp_isexceptionC(result)) return result;
    return _function_traverse_params(f, visitor, memo);
}

static ypObject *function_frozen_copy(ypObject *f) { return yp_incref(f); }

static ypObject *function_bool(ypObject *f) { return yp_True; }

static ypObject *function_currenthash(
        ypObject *f, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash)
{
    // Since types are compared by identity, we can cache our hash.
    *hash = ypObject_CACHED_HASH(f) = yp_HashPointer(f);
    return yp_None;
}

// XXX This shouldn't be directly called: yp_call* shortcuts if it sees callable is a function.
static ypObject *function_call(ypObject *f, ypObject **function, ypObject **self)
{
    *function = yp_incref(f);
    *self = NULL;  // i.e. "no implicit first argument"
    return yp_None;
}

// Decrements the reference count of the visited object
static ypObject *_function_decref_visitor(ypObject *x, void *memo)
{
    // TODO Call yp_decref_fromdealloc instead?
    yp_decref(x);
    return yp_None;
}

static ypObject *function_dealloc(ypObject *f, void *memo)
{
    (void)function_traverse(f, _function_decref_visitor, NULL);  // never fails
    ypMem_FREE_FIXED(f);
    return yp_None;
}

static ypObject *function_func_new_code(ypObject *f, yp_ssize_t n, ypObject *const *argarray)
{
    return yp_NotImplementedError;
}

yp_IMMORTAL_FUNCTION_static(function_func_new, function_func_new_code,
        ({yp_CONST_REF(yp_s_cls), NULL}, {yp_CONST_REF(yp_s_slash), NULL},
                {yp_CONST_REF(yp_s_star_args), NULL}, {yp_CONST_REF(yp_s_star_star_kwargs), NULL}));

static ypCallableMethods ypFunction_as_callable = {
        function_call  // tp_call
};

static ypTypeObject ypFunction_Type = {
        yp_TYPE_HEAD_INIT,
        ypType_FLAG_IS_CALLABLE,  // tp_flags
        NULL,                     // tp_name

        // Object fundamentals
        yp_CONST_REF(function_func_new),  // tp_func_new
        function_dealloc,                 // tp_dealloc
        function_traverse,                // tp_traverse
        NULL,                             // tp_str
        NULL,                             // tp_repr

        // Freezing, copying, and invalidating
        MethodError_objproc,       // tp_freeze
        function_frozen_copy,      // tp_unfrozen_copy
        function_frozen_copy,      // tp_frozen_copy
        MethodError_traversefunc,  // tp_unfrozen_deepcopy
        MethodError_traversefunc,  // tp_frozen_deepcopy
        MethodError_objproc,       // tp_invalidate

        // Boolean operations and comparisons
        function_bool,               // tp_bool
        NotImplemented_comparefunc,  // tp_lt
        NotImplemented_comparefunc,  // tp_le
        NotImplemented_comparefunc,  // tp_eq
        NotImplemented_comparefunc,  // tp_ne
        NotImplemented_comparefunc,  // tp_ge
        NotImplemented_comparefunc,  // tp_gt

        // Generic object operations
        function_currenthash,  // tp_currenthash
        MethodError_objproc,   // tp_close

        // Number operations
        MethodError_NumberMethods,  // tp_as_number

        // Iterator operations
        TypeError_miniiterfunc,         // tp_miniiter
        TypeError_miniiterfunc,         // tp_miniiter_reversed
        MethodError_miniiterfunc,       // tp_miniiter_next  // FIXME Should be type error?
        MethodError_miniiter_lenhfunc,  // tp_miniiter_length_hint
        TypeError_objproc,              // tp_iter
        TypeError_objproc,              // tp_iter_reversed
        TypeError_objobjproc,           // tp_send

        // Container operations
        TypeError_objobjproc,       // tp_contains
        TypeError_lenfunc,          // tp_len
        MethodError_objobjproc,     // tp_push
        MethodError_objproc,        // tp_clear
        MethodError_objproc,        // tp_pop
        MethodError_objobjobjproc,  // tp_remove
        MethodError_objobjobjproc,  // tp_getdefault
        MethodError_objobjobjproc,  // tp_setitem
        MethodError_objobjproc,     // tp_delitem
        MethodError_objvalistproc,  // tp_update

        // Sequence operations
        MethodError_SequenceMethods,  // tp_as_sequence

        // Set operations
        MethodError_SetMethods,  // tp_as_set

        // Mapping operations
        MethodError_MappingMethods,  // tp_as_mapping

        // Callable operations
        &ypFunction_as_callable  // tp_as_callable
};

// Public functions

ypObject *yp_functionC(yp_function_decl_t *declaration)
{
    yp_int32_t           parameters_len = declaration->parameters_len;
    yp_parameter_decl_t *parameters = declaration->parameters;
    yp_ssize_t           state_size;
    yp_uint32_t          state_objlocs;
    ypObject            *result;
    ypObject            *newF;
    yp_ssize_t           i;

    if (declaration->flags != 0) return yp_ValueError;
    if (parameters_len < 0) return yp_ValueError;
    if (parameters_len > ypFunction_LEN_MAX) return yp_MemorySizeOverflowError;

    result = _ypState_fromdecl(&state_size, &state_objlocs, declaration->state_decl);
    if (yp_isexceptionC(result)) return result;
    // FIXME Check state_size for yp_MemorySizeOverflowError
    if (state_size > 0) return yp_NotImplementedError;  // FIXME Support state for functions.

    newF = ypMem_MALLOC_CONTAINER_INLINE(
            ypFunctionObject, ypFunction_CODE, parameters_len, ypFunction_ALLOCLEN_MAX);
    if (yp_isexceptionC(newF)) return newF;
    ypFunction_CODE_FUNC(newF) = declaration->code;
    ypFunction_FLAGS(newF) = 0;
    ypFunction_SET_STATE(newF, NULL);
    // TODO name/qualname, doc, state, module, annotations, ...

    for (i = 0; i < parameters_len; i++) {
        ypObject *default_ = parameters[i].default_;  // borrowed
        ypFunction_PARAMS(newF)[i].name = yp_incref(parameters[i].name);
        ypFunction_PARAMS(newF)[i].default_ = default_ == NULL ? NULL : yp_incref(default_);
    }
    ypFunction_SET_PARAMS_LEN(newF, parameters_len);

    result = _ypFunction_validate_parameters(newF);
    if (yp_isexceptionC(result)) {
        yp_decref(newF);
        return result;
    }

    return newF;
}

// FIXME A convenience function to decref all objects in yp_function_decl_t/yp_def_generator_t/etc,
// but warn that it cannot contain borrowed references (as they would be stolen/decref'ed).

// TODO As in Python, support a "partial" object that wraps a function with some pre-set arguments.

#pragma endregion function


/*************************************************************************************************
 * Common object methods
 *************************************************************************************************/
#pragma region methods

// These are the functions that simply redirect to object methods; more complex public functions
// are found elsewhere.

// args must be surrounded in brackets, to form the function call; as such, must also include ob
#define _yp_REDIRECT1(ob, tp_meth, args)               \
    do {                                               \
        ypTypeObject        *type = ypObject_TYPE(ob); \
        return type->tp_meth args;                     \
    } while (0)

#define _yp_REDIRECT2(ob, tp_suite, suite_meth, args)               \
    do {                                                            \
        ypTypeObject                     *type = ypObject_TYPE(ob); \
        return type->tp_suite->suite_meth args;                     \
    } while (0)

#define _yp_REDIRECT_BOOL1(ob, tp_meth, args)                                         \
    do {                                                                              \
        ypTypeObject *type = ypObject_TYPE(ob);                                       \
        ypObject *result = type->tp_meth args;                                        \
        yp_ASSERT(result == yp_True || result == yp_False || yp_isexceptionC(result), \
                #tp_meth " must return yp_True, yp_False, or an exception");          \
        return result;                                                                \
    } while (0)

#define _yp_REDIRECT_BOOL2(ob, tp_suite, suite_meth, args)                            \
    do {                                                                              \
        ypTypeObject *type = ypObject_TYPE(ob);                                       \
        ypObject *result = type->tp_suite->suite_meth args;                           \
        yp_ASSERT(result == yp_True || result == yp_False || yp_isexceptionC(result), \
                #suite_meth " must return yp_True, yp_False, or an exception");       \
        return result;                                                                \
    } while (0)

#define _yp_REDIRECT_EXC1(ob, tp_meth, args, pExc)   \
    do {                                             \
        ypTypeObject *type = ypObject_TYPE(ob);      \
        ypObject *result = type->tp_meth args;       \
        if (yp_isexceptionC(result)) *pExc = result; \
        return;                                      \
    } while (0)

#define _yp_REDIRECT_EXC2(ob, tp_suite, suite_meth, args, pExc) \
    do {                                                        \
        ypTypeObject *type = ypObject_TYPE(ob);                 \
        ypObject *result = type->tp_suite->suite_meth args;     \
        if (yp_isexceptionC(result)) *pExc = result;            \
        return;                                                 \
    } while (0)

ypObject *yp_bool(ypObject *x) { _yp_REDIRECT_BOOL1(x, tp_bool, (x)); }

ypObject *yp_iter(ypObject *x) { _yp_REDIRECT1(x, tp_iter, (x)); }

static ypObject *_yp_send(ypObject *iterator, ypObject *value)
{
    ypTypeObject *type = ypObject_TYPE(iterator);
    ypObject     *result = type->tp_send(iterator, value);
    return result;
}

ypObject *yp_send(ypObject *iterator, ypObject *value)
{
    if (yp_isexceptionC(value)) {
        return value;
    }
    return _yp_send(iterator, value);
}

ypObject *yp_next(ypObject *iterator) { return _yp_send(iterator, yp_None); }

ypObject *yp_next2(ypObject *iterator, ypObject *defval)
{
    ypObject *result = _yp_send(iterator, yp_None);
    if (yp_isexceptionC2(result, yp_StopIteration)) {
        result = yp_incref(defval);
    }
    return result;
}

ypObject *yp_throw(ypObject *iterator, ypObject *exc)
{
    if (!yp_isexceptionC(exc)) {
        // typeExc may be yp_InvalidatedError or yp_TypeError.
        return_yp_BAD_TYPE(exc);
    }
    return _yp_send(iterator, exc);
}

void yp_close(ypObject *x, ypObject **exc) { _yp_REDIRECT_EXC1(x, tp_close, (x), exc); }

ypObject *yp_reversed(ypObject *x) { _yp_REDIRECT1(x, tp_iter_reversed, (x)); }

ypObject *yp_contains(ypObject *container, ypObject *x)
{
    _yp_REDIRECT_BOOL1(container, tp_contains, (container, x));
}
ypObject *yp_in(ypObject *x, ypObject *container)
{
    _yp_REDIRECT_BOOL1(container, tp_contains, (container, x));
}

ypObject *yp_not_in(ypObject *x, ypObject *container)
{
    ypObject *result = yp_in(x, container);
    return ypBool_NOT(result);
}

void yp_push(ypObject *container, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC1(container, tp_push, (container, x), exc);
}

void yp_clear(ypObject *container, ypObject **exc)
{
    _yp_REDIRECT_EXC1(container, tp_clear, (container), exc);
}

ypObject *yp_pop(ypObject *container) { _yp_REDIRECT1(container, tp_pop, (container)); }

ypObject *yp_concat(ypObject *sequence, ypObject *x)
{
    _yp_REDIRECT2(sequence, tp_as_sequence, tp_concat, (sequence, x));
}

ypObject *yp_repeatC(ypObject *sequence, yp_ssize_t factor)
{
    _yp_REDIRECT2(sequence, tp_as_sequence, tp_repeat, (sequence, factor));
}

ypObject *yp_getindexC(ypObject *sequence, yp_ssize_t i)
{
    _yp_REDIRECT2(sequence, tp_as_sequence, tp_getindex, (sequence, i, NULL));
}

ypObject *yp_getsliceC4(ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k)
{
    _yp_REDIRECT2(sequence, tp_as_sequence, tp_getslice, (sequence, i, j, k));
}

yp_ssize_t yp_findC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc)
{
    yp_ssize_t index;
    ypObject  *result = ypObject_TYPE(sequence)->tp_as_sequence->tp_find(
            sequence, x, i, j, yp_FIND_FORWARD, &index);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(-1, exc, result);
    yp_ASSERT(index >= -1, "tp_find cannot return <-1");
    return index;
}

yp_ssize_t yp_findC(ypObject *sequence, ypObject *x, ypObject **exc)
{
    return yp_findC5(sequence, x, 0, yp_SLICE_LAST, exc);
}

yp_ssize_t yp_indexC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc)
{
    ypObject  *subExc = yp_None;
    yp_ssize_t result = yp_findC5(sequence, x, i, j, &subExc);
    if (yp_isexceptionC(subExc)) return_yp_CEXC_ERR(-1, exc, subExc);
    if (result == -1) return_yp_CEXC_ERR(-1, exc, yp_ValueError);
    return result;
}

yp_ssize_t yp_indexC(ypObject *sequence, ypObject *x, ypObject **exc)
{
    return yp_indexC5(sequence, x, 0, yp_SLICE_LAST, exc);
}

yp_ssize_t yp_rfindC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc)
{
    yp_ssize_t index;
    ypObject  *result = ypObject_TYPE(sequence)->tp_as_sequence->tp_find(
            sequence, x, i, j, yp_FIND_REVERSE, &index);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(-1, exc, result);
    yp_ASSERT(index >= -1, "tp_find cannot return <-1");
    return index;
}

yp_ssize_t yp_rfindC(ypObject *sequence, ypObject *x, ypObject **exc)
{
    return yp_rfindC5(sequence, x, 0, yp_SLICE_LAST, exc);
}

yp_ssize_t yp_rindexC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc)
{
    ypObject  *subExc = yp_None;
    yp_ssize_t result = yp_rfindC5(sequence, x, i, j, &subExc);
    if (yp_isexceptionC(subExc)) return_yp_CEXC_ERR(-1, exc, subExc);
    if (result == -1) return_yp_CEXC_ERR(-1, exc, yp_ValueError);
    return result;
}

yp_ssize_t yp_rindexC(ypObject *sequence, ypObject *x, ypObject **exc)
{
    return yp_rindexC5(sequence, x, 0, yp_SLICE_LAST, exc);
}

yp_ssize_t yp_countC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc)
{
    yp_ssize_t count;
    ypObject *result = ypObject_TYPE(sequence)->tp_as_sequence->tp_count(sequence, x, i, j, &count);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(0, exc, result);
    yp_ASSERT(count >= 0, "tp_count cannot return negative");
    return count;
}

yp_ssize_t yp_countC(ypObject *sequence, ypObject *x, ypObject **exc)
{
    return yp_countC5(sequence, x, 0, yp_SLICE_LAST, exc);
}

void yp_setindexC(ypObject *sequence, yp_ssize_t i, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_setindex, (sequence, i, x), exc);
}

void yp_setsliceC6(
        ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_setslice, (sequence, i, j, k, x), exc);
}

void yp_delindexC(ypObject *sequence, yp_ssize_t i, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_delindex, (sequence, i), exc);
}

void yp_delsliceC5(ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_delslice, (sequence, i, j, k), exc);
}

void yp_append(ypObject *sequence, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_append, (sequence, x), exc);
}

void yp_extend(ypObject *sequence, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_extend, (sequence, x), exc);
}

void yp_irepeatC(ypObject *sequence, yp_ssize_t factor, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_irepeat, (sequence, factor), exc);
}

void yp_insertC(ypObject *sequence, yp_ssize_t i, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_insert, (sequence, i, x), exc);
}

ypObject *yp_popindexC(ypObject *sequence, yp_ssize_t i)
{
    _yp_REDIRECT2(sequence, tp_as_sequence, tp_popindex, (sequence, i));
}

void yp_remove(ypObject *sequence, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC1(sequence, tp_remove, (sequence, x, NULL), exc);
}

void yp_reverse(ypObject *sequence, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_reverse, (sequence), exc);
}

void yp_sort4(ypObject *sequence, ypObject *key, ypObject *reverse, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_sort, (sequence, key, reverse), exc);
}

void yp_sort(ypObject *sequence, ypObject **exc)
{
    _yp_REDIRECT_EXC2(sequence, tp_as_sequence, tp_sort, (sequence, yp_None, yp_False), exc);
}

ypObject *yp_isdisjoint(ypObject *set, ypObject *x)
{
    _yp_REDIRECT_BOOL2(set, tp_as_set, tp_isdisjoint, (set, x));
}

ypObject *yp_issubset(ypObject *set, ypObject *x)
{
    _yp_REDIRECT_BOOL2(set, tp_as_set, tp_issubset, (set, x));
}

ypObject *yp_issuperset(ypObject *set, ypObject *x)
{
    _yp_REDIRECT_BOOL2(set, tp_as_set, tp_issuperset, (set, x));
}

ypObject *yp_unionN(ypObject *set, int n, ...)
{
    return_yp_V_FUNC(ypObject *, yp_unionNV, (set, n, args), n);
}
ypObject *yp_unionNV(ypObject *set, int n, va_list args)
{
    _yp_REDIRECT2(set, tp_as_set, tp_union, (set, n, args));
}

ypObject *yp_intersectionN(ypObject *set, int n, ...)
{
    return_yp_V_FUNC(ypObject *, yp_intersectionNV, (set, n, args), n);
}
ypObject *yp_intersectionNV(ypObject *set, int n, va_list args)
{
    _yp_REDIRECT2(set, tp_as_set, tp_intersection, (set, n, args));
}

ypObject *yp_differenceN(ypObject *set, int n, ...)
{
    return_yp_V_FUNC(ypObject *, yp_differenceNV, (set, n, args), n);
}
ypObject *yp_differenceNV(ypObject *set, int n, va_list args)
{
    _yp_REDIRECT2(set, tp_as_set, tp_difference, (set, n, args));
}

ypObject *yp_symmetric_difference(ypObject *set, ypObject *x)
{
    _yp_REDIRECT2(set, tp_as_set, tp_symmetric_difference, (set, x));
}

void yp_updateN(ypObject *set, ypObject **exc, int n, ...)
{
    return_yp_V_FUNC_void(yp_updateNV, (set, exc, n, args), n);
}
void yp_updateNV(ypObject *set, ypObject **exc, int n, va_list args)
{
    _yp_REDIRECT_EXC1(set, tp_update, (set, n, args), exc);
}

void yp_intersection_updateN(ypObject *set, ypObject **exc, int n, ...)
{
    return_yp_V_FUNC_void(yp_intersection_updateNV, (set, exc, n, args), n);
}
void yp_intersection_updateNV(ypObject *set, ypObject **exc, int n, va_list args)
{
    _yp_REDIRECT_EXC2(set, tp_as_set, tp_intersection_update, (set, n, args), exc);
}

void yp_difference_updateN(ypObject *set, ypObject **exc, int n, ...)
{
    return_yp_V_FUNC_void(yp_difference_updateNV, (set, exc, n, args), n);
}
void yp_difference_updateNV(ypObject *set, ypObject **exc, int n, va_list args)
{
    _yp_REDIRECT_EXC2(set, tp_as_set, tp_difference_update, (set, n, args), exc);
}

void yp_symmetric_difference_update(ypObject *set, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(set, tp_as_set, tp_symmetric_difference_update, (set, x), exc);
}

void yp_pushunique(ypObject *set, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC2(set, tp_as_set, tp_pushunique, (set, x), exc);
}

void yp_discard(ypObject *set, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC1(set, tp_remove, (set, x, yp_None), exc);
}

ypObject *yp_getitem(ypObject *mapping, ypObject *key)
{
    _yp_REDIRECT1(mapping, tp_getdefault, (mapping, key, NULL));
}

void yp_setitem(ypObject *mapping, ypObject *key, ypObject *x, ypObject **exc)
{
    _yp_REDIRECT_EXC1(mapping, tp_setitem, (mapping, key, x), exc);
}

void yp_delitem(ypObject *mapping, ypObject *key, ypObject **exc)
{
    _yp_REDIRECT_EXC1(mapping, tp_delitem, (mapping, key), exc);
}

ypObject *yp_getdefault(ypObject *mapping, ypObject *key, ypObject *defval)
{
    _yp_REDIRECT1(mapping, tp_getdefault, (mapping, key, defval));
}

ypObject *yp_iter_items(ypObject *mapping)
{
    _yp_REDIRECT2(mapping, tp_as_mapping, tp_iter_items, (mapping));
}

ypObject *yp_iter_keys(ypObject *mapping)
{
    _yp_REDIRECT2(mapping, tp_as_mapping, tp_iter_keys, (mapping));
}

ypObject *yp_popvalue3(ypObject *mapping, ypObject *key, ypObject *defval)
{
    _yp_REDIRECT2(mapping, tp_as_mapping, tp_popvalue, (mapping, key, defval));
}

void yp_popitem(ypObject *mapping, ypObject **key, ypObject **value)
{
    ypTypeObject *type = ypObject_TYPE(mapping);
    ypObject     *result = type->tp_as_mapping->tp_popitem(mapping, key, value);
    if (yp_isexceptionC(result)) {
        *key = *value = result;
    }
}

ypObject *yp_setdefault(ypObject *mapping, ypObject *key, ypObject *defval)
{
    ypTypeObject *type = ypObject_TYPE(mapping);
    ypObject     *result = type->tp_as_mapping->tp_setdefault(mapping, key, defval);
    return result;
}

void yp_updateK(ypObject *mapping, ypObject **exc, int n, ...)
{
    return_yp_K_FUNC_void(yp_updateKV, (mapping, exc, n, args), n);
}
void yp_updateKV(ypObject *mapping, ypObject **exc, int n, va_list args)
{
    _yp_REDIRECT_EXC2(mapping, tp_as_mapping, tp_updateK, (mapping, n, args), exc);
}

int yp_iscallableC(ypObject *x) { return ypObject_IS_CALLABLE(x); }

// TODO A version of yp_callN that also accepts keyword arguments. Could have yp_callK that _only_
// accepts keyword arguments, but I'd rather it's combined. A yp_callNK would be like
// yp_callNK(c, n_args, <args>, n_kwargs, <kwargs>), but would be tricky to call correctly (a
// miscount would be disastrous).
ypObject *yp_callN(ypObject *c, int n, ...)
{
    return_yp_V_FUNC(ypObject *, yp_callNV, (c, n, args), n);
}

ypObject *yp_callNV(ypObject *c, int n, va_list args)
{
    if (n < 0) n = 0;

    if (ypObject_TYPE_CODE(c) == ypFunction_CODE) {
        return ypFunction_callNV(c, n, args);
    } else {
        ypObject *f;
        ypObject *self;
        ypObject *result = ypObject_TYPE(c)->tp_as_callable->tp_call(c, &f, &self);  // new refs
        if (yp_isexceptionC(result)) return result;

        if (ypObject_TYPE_CODE(f) != ypFunction_CODE) {
            result = yp_BAD_TYPE(f);
        } else if (self == NULL) {
            result = ypFunction_callNV(f, n, args);
        } else {
            result = ypFunction_callNV_withself(f, self, n, args);
        }

        if (self != NULL) yp_decref(self);
        yp_decref(f);
        return result;
    }
}

ypObject *yp_call_stars(ypObject *c, ypObject *args, ypObject *kwargs)
{
    if (ypObject_TYPE_CODE(c) == ypFunction_CODE) {
        return ypFunction_call_stars(c, args, kwargs);
    } else {
        ypObject *f;
        ypObject *self;
        ypObject *result = ypObject_TYPE(c)->tp_as_callable->tp_call(c, &f, &self);  // new refs
        if (yp_isexceptionC(result)) return result;

        if (ypObject_TYPE_CODE(f) != ypFunction_CODE) {
            result = yp_BAD_TYPE(f);
        } else if (self == NULL) {
            result = ypFunction_call_stars(f, args, kwargs);
        } else {
            result = ypFunction_call_stars_withself(f, self, args, kwargs);
        }

        if (self != NULL) yp_decref(self);
        yp_decref(f);
        return result;
    }
}

// TODO Tempted to have a version of this that also accepts keyword arguments (either via K or a
// dict), but the point of yp_call_arrayX is to have an array we can quickly pass along, and that's
// likely impossible with keyword arguments (the values would have to be in the array, in exactly
// the right order, and we'd have to yp_eq all the kwarg names to confirm that). Python supports
// kwargs in vectorcall, but I don't see the overall benefit.
ypObject *yp_call_arrayX(yp_ssize_t n, ypObject **args)
{
    ypObject *c;

    if (n < 1) return yp_TypeError;
    c = args[0];  // borrowed

    if (ypObject_TYPE_CODE(c) == ypFunction_CODE) {
        return ypFunction_call_array(c, n - 1, args + 1);
    } else {
        ypObject *f;
        ypObject *self;
        ypObject *result = ypObject_TYPE(c)->tp_as_callable->tp_call(c, &f, &self);  // new refs
        if (yp_isexceptionC(result)) return result;

        if (ypObject_TYPE_CODE(f) != ypFunction_CODE) {
            result = yp_BAD_TYPE(f);
        } else if (self == NULL) {
            result = ypFunction_call_array(f, n - 1, args + 1);
        } else if (self == c) {
            result = ypFunction_call_array(f, n, args);
        } else {
            args[0] = self;  // temporarily place self at the start of the array; borrowed
            result = ypFunction_call_array(f, n, args);
            args[0] = c;  // revert our changes to args
        }

        if (self != NULL) yp_decref(self);
        yp_decref(f);
        return result;
    }
}

ypObject *yp_iter_values(ypObject *mapping)
{
    _yp_REDIRECT2(mapping, tp_as_mapping, tp_iter_values, (mapping));
}

// FIXME This would be clearer if mini iterators were opaque structures containing obj and state.
// Less chance to accidentally misuse the object returned.
ypObject *yp_miniiter(ypObject *x, yp_uint64_t *state)
{
    _yp_REDIRECT1(x, tp_miniiter, (x, state));
}

ypObject *yp_miniiter_next(ypObject *mi, yp_uint64_t *state)
{
    _yp_REDIRECT1(mi, tp_miniiter_next, (mi, state));
}

yp_ssize_t yp_miniiter_length_hintC(ypObject *mi, yp_uint64_t *state, ypObject **exc)
{
    yp_ssize_t length_hint = 0;
    ypObject  *result = ypObject_TYPE(mi)->tp_miniiter_length_hint(mi, state, &length_hint);
    if (yp_isexceptionC(result)) return_yp_CEXC_ERR(0, exc, result);
    return length_hint < 0 ? 0 : length_hint;
}

ypObject *yp_miniiter_keys(ypObject *x, yp_uint64_t *state)
{
    _yp_REDIRECT2(x, tp_as_mapping, tp_miniiter_keys, (x, state));
}

ypObject *yp_miniiter_values(ypObject *x, yp_uint64_t *state)
{
    _yp_REDIRECT2(x, tp_as_mapping, tp_miniiter_values, (x, state));
}

ypObject *yp_miniiter_items(ypObject *x, yp_uint64_t *state)
{
    _yp_REDIRECT2(x, tp_as_mapping, tp_miniiter_items, (x, state));
}

void yp_miniiter_items_next(ypObject *mi, yp_uint64_t *state, ypObject **key, ypObject **value)
{
    ypTypeObject *type = ypObject_TYPE(mi);
    ypObject     *result = type->tp_as_mapping->tp_miniiter_items_next(mi, state, key, value);
    if (yp_isexceptionC(result)) {
        *key = *value = result;
    }
}

#pragma endregion methods


/*************************************************************************************************
 * C-to-C container operations
 *************************************************************************************************/
#pragma region c2c_containers

// XXX You could spend *weeks* adding all sorts of nifty combinations to this section: DON'T.
// Let's restrain ourselves and limit them to functions we actually find useful (and can test) in
// our projects that integrate nohtyP.

int yp_o2i_containsC(ypObject *container, yp_int_t xC, ypObject **exc)
{
    ypObject *x = yp_intC(xC);
    ypObject *result = yp_contains(container, x);
    yp_decref(x);
    if (yp_isexceptionC(result)) *exc = result;
    return result == yp_True;  // returns false on yp_False or exception
}

void yp_o2i_pushC(ypObject *container, yp_int_t xC, ypObject **exc)
{
    ypObject *x = yp_intC(xC);
    yp_push(container, x, exc);
    yp_decref(x);
}

yp_int_t yp_o2i_popC(ypObject *container, ypObject **exc)
{
    ypObject *x = yp_pop(container);
    yp_int_t  xC = yp_index_asintC(x, exc);
    yp_decref(x);
    return xC;
}

yp_int_t yp_o2i_getitemC(ypObject *container, ypObject *key, ypObject **exc)
{
    ypObject *x = yp_getitem(container, key);
    yp_int_t  xC = yp_index_asintC(x, exc);
    yp_decref(x);
    return xC;
}

void yp_o2i_setitemC(ypObject *container, ypObject *key, yp_int_t xC, ypObject **exc)
{
    ypObject *x = yp_intC(xC);
    yp_setitem(container, key, x, exc);
    yp_decref(x);
}

ypObject *yp_o2s_getitemCX(ypObject *container, ypObject *key, yp_ssize_t *size,
        const yp_uint8_t **encoded, ypObject **encoding)
{
    ypObject *x;
    ypObject *result;
    int       container_pair = ypObject_TYPE_PAIR_CODE(container);

    // XXX The pointer returned via *encoded is only valid so long as the str/chrarray object
    // remains allocated and isn't modified. As such, limit this function to those containers that
    // we *know* will keep the object allocated (so long as _they_ aren't modified, of course).
    if (container_pair != ypTuple_CODE && container_pair != ypFrozenDict_CODE) {
        return_yp_BAD_TYPE(container);
    }

    x = yp_getitem(container, key);
    result = yp_asencodedCX(x, size, encoded, encoding);
    yp_decref(x);
    return result;
}

void yp_o2s_setitemC5(
        ypObject *container, ypObject *key, yp_ssize_t x_lenC, const yp_uint8_t *xC, ypObject **exc)
{
    ypObject *x = yp_str_frombytesC2(x_lenC, xC);
    yp_setitem(container, key, x, exc);
    yp_decref(x);
}

ypObject *yp_i2o_getitemC(ypObject *container, yp_int_t keyC)
{
    ypObject *key = yp_intC(keyC);
    ypObject *x = yp_getitem(container, key);
    yp_decref(key);
    return x;
}

void yp_i2o_setitemC(ypObject *container, yp_int_t keyC, ypObject *x, ypObject **exc)
{
    ypObject *key = yp_intC(keyC);
    yp_setitem(container, key, x, exc);
    yp_decref(key);
}

yp_int_t yp_i2i_getitemC(ypObject *container, yp_int_t keyC, ypObject **exc)
{
    ypObject *key = yp_intC(keyC);
    yp_int_t  x = yp_o2i_getitemC(container, key, exc);
    yp_decref(key);
    return x;
}

void yp_i2i_setitemC(ypObject *container, yp_int_t keyC, yp_int_t xC, ypObject **exc)
{
    ypObject *key = yp_intC(keyC);
    yp_o2i_setitemC(container, key, xC, exc);
    yp_decref(key);
}

ypObject *yp_i2s_getitemCX(ypObject *container, yp_int_t keyC, yp_ssize_t *size,
        const yp_uint8_t **encoded, ypObject **encoding)
{
    ypObject *key = yp_intC(keyC);
    ypObject *result = yp_o2s_getitemCX(container, key, size, encoded, encoding);
    yp_decref(key);
    return result;
}

void yp_i2s_setitemC5(
        ypObject *container, yp_int_t keyC, yp_ssize_t x_lenC, const yp_uint8_t *xC, ypObject **exc)
{
    ypObject *key = yp_intC(keyC);
    yp_o2s_setitemC5(container, key, x_lenC, xC, exc);
    yp_decref(key);
}

ypObject *yp_s2o_getitemC3(ypObject *container, yp_ssize_t key_lenC, const yp_uint8_t *keyC)
{
    ypObject *key = yp_str_frombytesC2(key_lenC, keyC);
    ypObject *x = yp_getitem(container, key);
    yp_decref(key);
    return x;
}

void yp_s2o_setitemC5(ypObject *container, yp_ssize_t key_lenC, const yp_uint8_t *keyC, ypObject *x,
        ypObject **exc)
{
    ypObject *key = yp_str_frombytesC2(key_lenC, keyC);
    yp_setitem(container, key, x, exc);
    yp_decref(key);
}

yp_int_t yp_s2i_getitemC4(
        ypObject *container, yp_ssize_t key_lenC, const yp_uint8_t *keyC, ypObject **exc)
{
    ypObject *key = yp_str_frombytesC2(key_lenC, keyC);
    yp_int_t  x = yp_o2i_getitemC(container, key, exc);
    yp_decref(key);
    return x;
}

void yp_s2i_setitemC5(ypObject *container, yp_ssize_t key_lenC, const yp_uint8_t *keyC, yp_int_t xC,
        ypObject **exc)
{
    ypObject *key = yp_str_frombytesC2(key_lenC, keyC);
    yp_o2i_setitemC(container, key, xC, exc);
    yp_decref(key);
}

#pragma endregion c2c_containers


/*************************************************************************************************
 * nohtyP functions (not methods) as objects
 *************************************************************************************************/
#pragma region functions_as_objects

static ypObject *yp_func_chr_code(ypObject *c, yp_ssize_t n, ypObject *const *argarray)
{
    ypObject *exc = yp_None;
    yp_int_t  i;

    yp_ASSERT(n == 1, "unexpected argarray of length %" PRIssize, n);

    i = yp_index_asintC(argarray[0], &exc);
    if (yp_isexceptionC(exc)) return exc;

    return yp_chrC(i);
};

yp_IMMORTAL_FUNCTION(
        yp_func_chr, yp_func_chr_code, ({yp_CONST_REF(yp_s_i), NULL}, {yp_CONST_REF(yp_s_slash)}));

static ypObject *yp_func_hash_code(ypObject *c, yp_ssize_t n, ypObject *const *argarray)
{
    ypObject *exc = yp_None;
    yp_hash_t hash;

    yp_ASSERT(n == 1, "unexpected argarray of length %" PRIssize, n);

    hash = yp_hashC(argarray[0], &exc);
    if (yp_isexceptionC(exc)) return exc;

    return yp_intC(hash);
};

yp_IMMORTAL_FUNCTION(yp_func_hash, yp_func_hash_code,
        ({yp_CONST_REF(yp_s_obj), NULL}, {yp_CONST_REF(yp_s_slash)}));

static ypObject *yp_func_iscallable_code(ypObject *c, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 1, "unexpected argarray of length %" PRIssize, n);
    return ypBool_FROM_C(yp_iscallableC(argarray[0]));
};

yp_IMMORTAL_FUNCTION(yp_func_iscallable, yp_func_iscallable_code,
        ({yp_CONST_REF(yp_s_obj), NULL}, {yp_CONST_REF(yp_s_slash)}));

static ypObject *yp_func_len_code(ypObject *c, yp_ssize_t n, ypObject *const *argarray)
{
    ypObject  *exc = yp_None;
    yp_ssize_t len;

    yp_ASSERT(n == 1, "unexpected argarray of length %" PRIssize, n);

    len = yp_lenC(argarray[0], &exc);
    if (yp_isexceptionC(exc)) return exc;

    return yp_intC(len);
};

yp_IMMORTAL_FUNCTION(yp_func_len, yp_func_len_code,
        ({yp_CONST_REF(yp_s_obj), NULL}, {yp_CONST_REF(yp_s_slash)}));

static ypObject *yp_func_reversed_code(ypObject *c, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 1, "unexpected argarray of length %" PRIssize, n);
    return yp_reversed(argarray[0]);
};

yp_IMMORTAL_FUNCTION(yp_func_reversed, yp_func_reversed_code,
        ({yp_CONST_REF(yp_s_sequence), NULL}, {yp_CONST_REF(yp_s_slash)}));

static ypObject *yp_func_sorted_code(ypObject *c, yp_ssize_t n, ypObject *const *argarray)
{
    yp_ASSERT(n == 5, "unexpected argarray of length %" PRIssize, n);
    return yp_sorted3(argarray[0], argarray[3], argarray[4]);
};

yp_IMMORTAL_FUNCTION(yp_func_sorted, yp_func_sorted_code,
        ({yp_CONST_REF(yp_s_iterable), NULL}, {yp_CONST_REF(yp_s_slash)},
                {yp_CONST_REF(yp_s_star), NULL}, {yp_CONST_REF(yp_s_key), yp_CONST_REF(yp_None)},
                {yp_CONST_REF(yp_s_reverse), yp_CONST_REF(yp_False)}));

#pragma endregion functions_as_objects


/*************************************************************************************************
 * The type table, and related public functions and variables
 *************************************************************************************************/
#pragma region type_table
// XXX Make sure this corresponds with ypInvalidated_CODE et al!

// Recall that C helpfully sets missing array elements to NULL
// clang-format off
static ypTypeObject *ypTypeTable[255] = {
    &ypInvalidated_Type,// ypInvalidated_CODE          (  0u)
    &ypInvalidated_Type,//                             (  1u)
    &ypException_Type,  // ypException_CODE            (  2u)
    &ypException_Type,  //                             (  3u)
    &ypType_Type,       // ypType_CODE                 (  4u)
    &ypType_Type,       //                             (  5u)

    &ypNoneType_Type,   // ypNoneType_CODE             (  6u)
    &ypNoneType_Type,   //                             (  7u)
    &ypBool_Type,       // ypBool_CODE                 (  8u)
    &ypBool_Type,       //                             (  9u)

    &ypInt_Type,        // ypInt_CODE                  ( 10u)
    &ypIntStore_Type,   // ypIntStore_CODE             ( 11u)
    &ypFloat_Type,      // ypFloat_CODE                ( 12u)
    &ypFloatStore_Type, // ypFloatStore_CODE           ( 13u)

    &ypIter_Type,       // ypIter_CODE                 ( 14u)
    &ypIter_Type,       //                             ( 15u)

    &ypBytes_Type,      // ypBytes_CODE                ( 16u)
    &ypByteArray_Type,  // ypByteArray_CODE            ( 17u)
    &ypStr_Type,        // ypStr_CODE                  ( 18u)
    &ypChrArray_Type,   // ypChrArray_CODE             ( 19u)
    &ypTuple_Type,      // ypTuple_CODE                ( 20u)
    &ypList_Type,       // ypList_CODE                 ( 21u)

    &ypFrozenSet_Type,  // ypFrozenSet_CODE            ( 22u)
    &ypSet_Type,        // ypSet_CODE                  ( 23u)

    &ypFrozenDict_Type, // ypFrozenDict_CODE           ( 24u)
    &ypDict_Type,       // ypDict_CODE                 ( 25u)

    &ypRange_Type,      // ypRange_CODE                ( 26u)
    &ypRange_Type,      //                             ( 27u)

    &ypFunction_Type,   // ypFunction_CODE             ( 28u)
    &ypFunction_Type,   //                             ( 29u)
};
// clang-format on

// TODO Reconsider the behaviour of exceptions. Python returns `type`. If we ever want to support
// creating instances of exceptions, we should do the same.
ypObject *yp_type(ypObject *object) { return (ypObject *)ypObject_TYPE(object); }

// The immortal type objects
// TODO Rename to yp_type_*? We might want to use the 't' in yp_t_* with tuples....
ypObject *const yp_t_invalidated = (ypObject *)&ypInvalidated_Type;
// FIXME Rename to yp_t_BaseException...or is yp_t_exception closer to a metaclass?
ypObject *const yp_t_exception = (ypObject *)&ypException_Type;
ypObject *const yp_t_type = (ypObject *)&ypType_Type;
ypObject *const yp_t_NoneType = (ypObject *)&ypNoneType_Type;
ypObject *const yp_t_bool = (ypObject *)&ypBool_Type;
ypObject *const yp_t_int = (ypObject *)&ypInt_Type;
ypObject *const yp_t_intstore = (ypObject *)&ypIntStore_Type;
ypObject *const yp_t_float = (ypObject *)&ypFloat_Type;
ypObject *const yp_t_floatstore = (ypObject *)&ypFloatStore_Type;
ypObject *const yp_t_iter = (ypObject *)&ypIter_Type;
ypObject *const yp_t_bytes = (ypObject *)&ypBytes_Type;
ypObject *const yp_t_bytearray = (ypObject *)&ypByteArray_Type;
ypObject *const yp_t_str = (ypObject *)&ypStr_Type;
ypObject *const yp_t_chrarray = (ypObject *)&ypChrArray_Type;
ypObject *const yp_t_tuple = (ypObject *)&ypTuple_Type;
ypObject *const yp_t_list = (ypObject *)&ypList_Type;
ypObject *const yp_t_frozenset = (ypObject *)&ypFrozenSet_Type;
ypObject *const yp_t_set = (ypObject *)&ypSet_Type;
ypObject *const yp_t_frozendict = (ypObject *)&ypFrozenDict_Type;
ypObject *const yp_t_dict = (ypObject *)&ypDict_Type;
ypObject *const yp_t_range = (ypObject *)&ypRange_Type;
ypObject *const yp_t_function = (ypObject *)&ypFunction_Type;

#pragma endregion type_table


/*************************************************************************************************
 * Initialization
 *************************************************************************************************/
#pragma region initialization

// TODO A script to ensure the comments on the line match the structure member
static const yp_initialize_parameters_t _default_initialize = {
        yp_sizeof(yp_initialize_parameters_t),  // sizeof_struct
        yp_mem_default_malloc,                  // yp_malloc
        yp_mem_default_malloc_resize,           // yp_malloc_resize
        yp_mem_default_free,                    // yp_free
        FALSE,                                  // everything_immortal
};

// Helpful macro, for use only by yp_initialize and friends, to retrieve an argument from args.
// Returns the default value if args is too small to hold the argument, or if the expression
// "args->key default_cond" (ie "args->yp_malloc ==NULL") evaluates to true.
// clang-format off
#define _yp_INIT_ARG_END(key) \
    (yp_offsetof(yp_initialize_parameters_t, key) + yp_sizeof_member(yp_initialize_parameters_t, key))
#define yp_INIT_ARG2(key, default_cond) \
    ( args->sizeof_struct < _yp_INIT_ARG_END(key) ? \
        _default_initialize.key : \
      args->key default_cond ? \
        _default_initialize.key : \
      /* else */ \
        args->key \
    )
#define yp_INIT_ARG1(key) \
    ( args->sizeof_struct < _yp_INIT_ARG_END(key) ? \
        _default_initialize.key : \
      /* else */ \
        args->key \
    )
// clang-format on

// Called *exactly* *once* by yp_initialize to set up memory management. Further, setting
// yp_malloc here helps ensure that yp_initialize is called before anything else in the library
// (because otherwise all mallocs result in yp_MemoryError).
static void _ypMem_initialize(const yp_initialize_parameters_t *args)
{
    yp_malloc = yp_INIT_ARG2(yp_malloc, == NULL);
    yp_malloc_resize = yp_INIT_ARG2(yp_malloc_resize, == NULL);
    yp_free = yp_INIT_ARG2(yp_free, == NULL);

    if (yp_INIT_ARG1(everything_immortal)) {
        // All objects will be created immortal
        _ypMem_starting_refcnt = ypObject_REFCNT_IMMORTAL;
    } else {
        _ypMem_starting_refcnt = 1;
    }

    // TODO Config param idea: "minimum" or "average" or "usual" or "preferred" allocation
    // size...something that indicates that malloc handles these sizes particularly well. The
    // number should be small, like 64 bytes or something. This number can be used to decide how
    // much data to put in-line. The call to malloc would use exactly this size when creating an
    // object, and at least this size when allocating extra data. This makes all objects the same
    // size, which will help malloc avoid fragmentation. It also recognizes the fact that each
    // malloc has a certain overhead in memory, so might as well allocate a certain amount to
    // compensate. When invalidating an object, the extra data is freed, but the invalidated
    // object that remains would sit in memory with this size until fully freed, so the extra data
    // is wasted until then, which is why the value should be small. (Actually, not all objects
    // will be this size, as int/float, when they become small objects, will only allocate a
    // fraction of this.)
}

// Called *exactly* *once* by yp_initialize to set up the codecs module. Errors are largely
// ignored: calling code will fail gracefully later on.
static void _yp_codecs_initialize(const yp_initialize_parameters_t *args)
{
    ypObject *exc = yp_None;

    // The set of standard encodings
    // TODO This would be easier to maintain with a "yp_N" macro to count args
    // clang-format off
    _yp_codecs_standard = yp_setN(11,
        yp_s_ascii,     yp_s_latin_1,
        yp_s_utf_8,
        yp_s_utf_16,    yp_s_utf_16be,      yp_s_utf_16le,
        yp_s_utf_32,    yp_s_utf_32be,      yp_s_utf_32le,
        yp_s_ucs_2,     yp_s_ucs_4
    );
    // clang-format on

    // Codec aliases
    // TODO Whether statically- or dynamically-allocated, this dict creation needs a length_hint
    // (yp_dict_fromlength_hint?)
    _yp_codecs_alias2encoding = yp_dictK(0);
#define yp_codecs_init_ADD_ALIAS(alias, name)                            \
    do {                                                                 \
        yp_IMMORTAL_STR_LATIN_1(_alias_obj, alias);                      \
        yp_setitem(_yp_codecs_alias2encoding, _alias_obj, (name), &exc); \
        yp_ASSERT1(!yp_isexceptionC(exc));                               \
    } while (0)
    // clang-format off
    yp_codecs_init_ADD_ALIAS("646",            yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("ansi_x3.4_1968", yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("ansi_x3_4_1968", yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("ansi_x3.4_1986", yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("cp367",          yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("csascii",        yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("ibm367",         yp_s_ascii);
    yp_codecs_init_ADD_ALIAS("iso646_us",      yp_s_ascii);
    // TODO More ascii and other aliases below
    // yp_s_latin_1
    // yp_s_utf_8
    // yp_s_utf_16
    // yp_s_utf_16be
    // yp_s_utf_16le
    // yp_s_utf_32
    // yp_s_utf_32be
    // yp_s_utf_32le
    // yp_s_ucs_2
    // yp_s_ucs_4
    // clang-format on

    // The error-handler registry
    // XXX Oddly, gcc 4.8 doesn't like us creating immortal ints from function pointers
    // ("initializer element is not computable at load time")
    // TODO Whether statically- or dynamically-allocated, this dict creation needs a length_hint
    // (yp_dict_fromlength_hint?)
    _yp_codecs_errors2handler = yp_dictK(0);
#define yp_codecs_init_ADD_ERROR(name, func)                                          \
    do {                                                                              \
        yp_o2i_setitemC(_yp_codecs_errors2handler, (name), (yp_ssize_t)(func), &exc); \
        yp_ASSERT1(!yp_isexceptionC(exc));                                            \
    } while (0)
    // clang-format off
    yp_codecs_init_ADD_ERROR(yp_s_strict,            yp_codecs_strict_errors);
    yp_codecs_init_ADD_ERROR(yp_s_replace,           yp_codecs_replace_errors);
    yp_codecs_init_ADD_ERROR(yp_s_ignore,            yp_codecs_ignore_errors);
    yp_codecs_init_ADD_ERROR(yp_s_xmlcharrefreplace, yp_codecs_xmlcharrefreplace_errors);
    yp_codecs_init_ADD_ERROR(yp_s_backslashreplace,  yp_codecs_backslashreplace_errors);
    yp_codecs_init_ADD_ERROR(yp_s_surrogateescape,   yp_codecs_surrogateescape_errors);
    yp_codecs_init_ADD_ERROR(yp_s_surrogatepass,     yp_codecs_surrogatepass_errors);
    // clang-format on
}

void yp_initialize(const yp_initialize_parameters_t *args)
{
    static int initialized = FALSE;

    // Ensure sizeof_struct was initialized appropriately: the earliest version of this struct
    // contained everything_immortal, so sizeof_struct should be at least that size.
    // TODO Change everything_immortal to a field of bit flags: no sense wasting 32/64 bits on
    // a single boolean!
    if (args != NULL && args->sizeof_struct < _yp_INIT_ARG_END(everything_immortal)) {
        yp_FATAL("yp_initialize_parameters_t.sizeof_struct (%" PRIssize
                 ") smaller than minimum (%" PRIssize ")",
                args->sizeof_struct, _yp_INIT_ARG_END(everything_immortal));
    }

    // yp_initialize can only be called once
    if (initialized) {
        yp_INFO0("yp_initialize called multiple times; only first call is honoured");
        return;
    }
    initialized = TRUE;

    // The caller can pass NULL if it just wants the defaults
    if (args == NULL) args = &_default_initialize;

    // Now initialize the modules one-by-one
    _ypMem_initialize(args);
    _yp_codecs_initialize(args);
}

#pragma endregion initialization

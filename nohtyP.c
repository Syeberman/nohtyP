/*
 * nohtyP.c - A Python-like API for C, in one .c and one .h
 *      http://nohtyp.wordpress.com    [v0.1.0 $Change$]
 *      Copyright © 2001-2013 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/3/license.html
 */

#include "nohtyP.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifdef _MSC_VER
#include <Windows.h>
#endif


/*************************************************************************************************
 * Static assertions for nohtyP.h
 *************************************************************************************************/

#define yp_STATIC_ASSERT( cond, tag ) typedef char assert_ ## tag[ (cond) ? 1 : -1 ]

yp_STATIC_ASSERT( sizeof( yp_int8_t ) == 1, sizeof_int8 );
yp_STATIC_ASSERT( sizeof( yp_uint8_t ) == 1, sizeof_uint8 );
yp_STATIC_ASSERT( sizeof( yp_int16_t ) == 2, sizeof_int16 );
yp_STATIC_ASSERT( sizeof( yp_uint16_t ) == 2, sizeof_uint16 );
yp_STATIC_ASSERT( sizeof( yp_int32_t ) == 4, sizeof_int32 );
yp_STATIC_ASSERT( sizeof( yp_uint32_t ) == 4, sizeof_uint32 );
yp_STATIC_ASSERT( sizeof( yp_int64_t ) == 8, sizeof_int64 );
yp_STATIC_ASSERT( sizeof( yp_uint64_t ) == 8, sizeof_uint64 );
yp_STATIC_ASSERT( sizeof( yp_float32_t ) == 4, sizeof_float32 );
yp_STATIC_ASSERT( sizeof( yp_float64_t ) == 8, sizeof_float64 );
yp_STATIC_ASSERT( sizeof( yp_ssize_t ) == sizeof( size_t ), sizeof_ssize );
#define yp_MAX_ALIGNMENT (8)  // The maximum possible required alignment of any entity

// Temporarily disable "integral constant overflow" warnings for this test
#pragma warning( push )
#pragma warning( disable : 4307 )
yp_STATIC_ASSERT( yp_SSIZE_T_MAX+1 < yp_SSIZE_T_MAX, ssize_max );
yp_STATIC_ASSERT( yp_SSIZE_T_MIN-1 > yp_SSIZE_T_MIN, ssize_min );
#pragma warning( pop )

// TODO assert that sizeof( "abcd" ) == 5 (ie it includes the null-terminator)
// TODO assert that we're little-endian (that type code is first byte)


/*************************************************************************************************
 * Internal structures and types, and related macros
 *************************************************************************************************/

typedef size_t yp_uhash_t;

// ypObject_HEAD defines the initial segment of every ypObject
#define ypObject_HEAD _ypObject_HEAD

// First byte of object structure is the type code; next 3 bytes is reference count.  The
// least-significant bit of the type code specifies if the type is immutable (0) or not.
// XXX Assuming little-endian for now
#define ypObject_MAKE_TYPE_REFCNT _ypObject_MAKE_TYPE_REFCNT
#define ypObject_TYPE_CODE( ob ) \
    ( ((yp_uint8_t *)(ob))[0] )
#define ypObject_SET_TYPE_CODE( ob, type ) \
    ( ((yp_uint8_t *)(ob))[0] = (type) )
#define ypObject_TYPE_CODE_IS_MUTABLE( type ) \
    ( (type) & 0x1u )
#define ypObject_TYPE_CODE_AS_FROZEN( type ) \
    ( (type) & 0xFEu )
#define ypObject_TYPE( ob ) \
    ( ypTypeTable[ypObject_TYPE_CODE( ob )] )
#define ypObject_IS_MUTABLE( ob ) \
    ( ypObject_TYPE_CODE_IS_MUTABLE( ypObject_TYPE_CODE( ob ) ) )
#define ypObject_REFCNT( ob ) \
    ( ((ypObject *)(ob))->ob_type_refcnt >> 8 )

// Type pairs are identified by the immutable type code, as all its methods are supported by the
// immutable version
#define ypObject_TYPE_PAIR_CODE( ob ) \
    ypObject_TYPE_CODE_AS_FROZEN( ypObject_TYPE_CODE( ob ) )

// TODO Need two types of immortals: statically-allocated immortals (so should never be
// freed/invalidated) and overly-incref'd immortals (should be allowed to be invalidated and thus
// free any extra data, although the object itself will never be free'd as we've lost track of the
// refcounts)
#define ypObject_REFCNT_IMMORTAL _ypObject_REFCNT_IMMORTAL

// When a hash of this value is stored in ob_hash, call tp_currenthash
#define ypObject_HASH_INVALID _ypObject_HASH_INVALID

// Signals an invalid length stored in ob_len (so call tp_len) or ob_alloclen
#define ypObject_LEN_INVALID        _ypObject_LEN_INVALID
#define ypObject_ALLOCLEN_INVALID   _ypObject_ALLOCLEN_INVALID

// Lengths and hashes can be cached in the object for easy retrieval
// FIXME trap when setting ob_len et al overflows available space
#define ypObject_CACHED_LEN( ob ) \
    ((yp_ssize_t) (((ypObject *)(ob))->ob_len == ypObject_LEN_INVALID ? -1 : ((ypObject *)(ob))->ob_len ))
#define ypObject_CACHED_HASH( ob ) ( ((ypObject *)(ob))->ob_hash )

// Base "constructor" for immortal objects
// TODO What to set alloclen to?  Does it matter?
#define yp_IMMORTAL_HEAD_INIT _yp_IMMORTAL_HEAD_INIT

// Base "constructor" for immortal type objects
#define yp_TYPE_HEAD_INIT yp_IMMORTAL_HEAD_INIT( ypType_CODE, NULL, 0 )

// Many object methods follow one of these generic function signatures
typedef ypObject *(*objproc)( ypObject * );
typedef ypObject *(*objobjproc)( ypObject *, ypObject * );
typedef ypObject *(*objobjobjproc)( ypObject *, ypObject *, ypObject * );
typedef ypObject *(*objssizeproc)( ypObject *, yp_ssize_t );
typedef ypObject *(*objssizeobjproc)( ypObject *, yp_ssize_t, ypObject * );
typedef ypObject *(*objsliceproc)( ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t );
typedef ypObject *(*objsliceobjproc)( ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t, ypObject * );
typedef ypObject *(*objvalistproc)( ypObject *, int, va_list );

// Some functions have rather unique signatures
typedef ypObject *(*visitfunc)( ypObject *, void * );
typedef ypObject *(*traversefunc)( ypObject *, visitfunc, void * );
typedef ypObject *(*hashvisitfunc)( ypObject *, void *, yp_hash_t * );
typedef ypObject *(*hashfunc)( ypObject *, hashvisitfunc, void *, yp_hash_t * );
typedef ypObject *(*miniiterfunc)( ypObject *, yp_uint64_t * );
typedef ypObject *(*miniiter_lenhintfunc)( ypObject *, yp_uint64_t *, yp_ssize_t * );
typedef ypObject *(*lenfunc)( ypObject *, yp_ssize_t * );
typedef ypObject *(*countfunc)( ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t * );
typedef ypObject *(*findfunc)( ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, yp_ssize_t * );
typedef ypObject *(*sortfunc)( ypObject *, yp_sort_key_func_t, ypObject * );
typedef ypObject *(*popitemfunc)( ypObject *, ypObject **, ypObject ** );

// Suite of number methods.  A placeholder for now, as the current implementation doesn't allow
// overriding yp_add et al (TODO).
typedef struct {
    objproc _placeholder;
} ypNumberMethods;

typedef struct {
    objssizeproc tp_getindex;
    objsliceproc tp_getslice;
    findfunc tp_find;
    countfunc tp_count;
    objssizeobjproc tp_setindex;
    objsliceobjproc tp_setslice;
    objssizeproc tp_delindex;
    objsliceproc tp_delslice;
    objobjproc tp_extend;
    objssizeproc tp_irepeat;
    objssizeobjproc tp_insert;
    objssizeproc tp_popindex;
    objproc tp_reverse;
    sortfunc tp_sort;
} ypSequenceMethods;

typedef struct {
    objobjproc tp_isdisjoint;
    objobjproc tp_issubset;
    // tp_lt is elsewhere
    objobjproc tp_issuperset;
    // tp_gt is elsewhere
    objvalistproc tp_update;
    objvalistproc tp_intersection_update;
    objvalistproc tp_difference_update;
    objobjproc tp_symmetric_difference_update;
    objobjproc tp_pushunique;
} ypSetMethods;

typedef struct {
    miniiterfunc tp_miniiter_items;
    objproc tp_iter_items;
    miniiterfunc tp_miniiter_keys;
    objproc tp_iter_keys;
    objobjobjproc tp_popvalue;
    popitemfunc tp_popitem;
    objobjobjproc tp_setdefault;
    miniiterfunc tp_miniiter_values;
    objproc tp_iter_values;
} ypMappingMethods;

// Type objects hold pointers to each type's methods.
typedef struct {
    ypObject_HEAD
    ypObject *tp_name; /* For printing, in format "<module>.<name>" */
    // TODO store type code here?

    // Object fundamentals
    objproc tp_dealloc;
    traversefunc tp_traverse; /* call function for all accessible objects */
    // TODO str, repr have the possibility of recursion; trap & test
    objproc tp_str;
    objproc tp_repr;

    // Freezing, copying, and invalidating
    objproc tp_freeze;
    traversefunc tp_unfrozen_copy;
    traversefunc tp_frozen_copy;
    objproc tp_invalidate; /* clear, then transmute self to ypInvalidated */

    // Boolean operations and comparisons
    objproc tp_bool;
    objobjproc tp_lt;
    objobjproc tp_le;
    objobjproc tp_eq;
    objobjproc tp_ne;
    objobjproc tp_ge;
    objobjproc tp_gt;

    // Generic object operations
    hashfunc tp_currenthash;
    objproc tp_close;

    // Number operations
    ypNumberMethods *tp_as_number;

    // Iterator operations
    miniiterfunc tp_miniiter;
    miniiterfunc tp_miniiter_reversed;
    miniiterfunc tp_miniiter_next;
    miniiter_lenhintfunc tp_miniiter_lenhint;
    objproc tp_iter;
    objproc tp_iter_reversed;
    objobjproc tp_send;

    // Container operations
    objobjproc tp_contains;
    lenfunc tp_len;
    objobjproc tp_push;
    objproc tp_clear; /* delete references to contained objects */
    objproc tp_pop;
    objobjobjproc tp_remove; /* if onmissing is NULL, raise exception if missing */
    objobjobjproc tp_getdefault; /* if defval is NULL, raise exception if missing */
    objobjobjproc tp_setitem;
    objobjproc tp_delitem;

    // Sequence operations
    ypSequenceMethods *tp_as_sequence;

    // Set operations
    ypSetMethods *tp_as_set;

    // Mapping operations
    ypMappingMethods *tp_as_mapping;
} ypTypeObject;

// The type table is defined at the bottom of this file
static ypTypeObject *ypTypeTable[255];

// Codes for the standard types (for lookup in the type table)
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

#define ypFrozenIter_CODE           ( 14u) // behaves like a closed iter
#define ypIter_CODE                 ( 15u)

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

yp_STATIC_ASSERT( _ypInt_CODE == ypInt_CODE, ypInt_CODE );
yp_STATIC_ASSERT( _ypBytes_CODE == ypBytes_CODE, ypBytes_CODE );
yp_STATIC_ASSERT( _ypStr_CODE == ypStr_CODE, ypStr_CODE );

// Generic versions of the methods above to return errors, usually; every method function pointer
// needs to point to a valid function (as opposed to constantly checking for NULL)
#define DEFINE_GENERIC_METHODS( name, retval ) \
    static ypObject *name ## _objproc( ypObject *x ) { return retval; } \
    static ypObject *name ## _objobjproc( ypObject *x, ypObject *y ) { return retval; } \
    static ypObject *name ## _objobjobjproc( ypObject *x, ypObject *y, ypObject *z ) { return retval; } \
    static ypObject *name ## _objssizeproc( ypObject *x, yp_ssize_t i ) { return retval; } \
    static ypObject *name ## _objssizeobjproc( ypObject *x, yp_ssize_t i, ypObject *y ) { return retval; } \
    static ypObject *name ## _objsliceproc( ypObject *x, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k ) { return retval; } \
    static ypObject *name ## _objsliceobjproc( ypObject *x, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *y ) { return retval; } \
    static ypObject *name ## _objvalistproc( ypObject *x, int n, va_list args ) { return retval; } \
    \
    static ypObject *name ## _visitfunc( ypObject *x, void *memo ) { return retval; } \
    static ypObject *name ## _traversefunc( ypObject *x, visitfunc visitor, void *memo ) { return retval; } \
    static ypObject *name ## _hashfunc( ypObject *x, hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash ) { return retval; } \
    static ypObject *name ## _miniiterfunc( ypObject *x, yp_uint64_t *state ) { return retval; } \
    static ypObject *name ## _miniiter_lenhfunc( ypObject *x, yp_uint64_t *state, yp_ssize_t *lenhint ) { return retval; } \
    static ypObject *name ## _lenfunc( ypObject *x, yp_ssize_t *len ) { return retval; } \
    static ypObject *name ## _countfunc( ypObject *x, ypObject *y, yp_ssize_t i, yp_ssize_t j, yp_ssize_t *count ) { return retval; } \
    static ypObject *name ## _findfunc( ypObject *x, ypObject *y, yp_ssize_t i, yp_ssize_t j, yp_ssize_t *index ) { return retval; } \
    static ypObject *name ## _sortfunc( ypObject *x, yp_sort_key_func_t key, ypObject *reverse ) { return retval; } \
    static ypObject *name ## _popitemfunc( ypObject *x, ypObject **key, ypObject **value ) { return retval; } \
    \
    static ypNumberMethods name ## _NumberMethods[1] = { { \
        *name ## _objproc \
    } }; \
    static ypSequenceMethods name ## _SequenceMethods[1] = { { \
        *name ## _objssizeproc, \
        *name ## _objsliceproc, \
        *name ## _findfunc, \
        *name ## _countfunc, \
        *name ## _objssizeobjproc, \
        *name ## _objsliceobjproc, \
        *name ## _objssizeproc, \
        *name ## _objsliceproc, \
        *name ## _objobjproc, \
        *name ## _objssizeproc, \
        *name ## _objssizeobjproc, \
        *name ## _objssizeproc, \
        *name ## _objproc, \
        *name ## _sortfunc \
    } }; \
    static ypSetMethods name ## _SetMethods[1] = { { \
        *name ## _objobjproc, \
        *name ## _objobjproc, \
        *name ## _objobjproc, \
        *name ## _objvalistproc, \
        *name ## _objvalistproc, \
        *name ## _objvalistproc, \
        *name ## _objobjproc, \
        *name ## _objobjproc \
    } }; \
    static ypMappingMethods name ## _MappingMethods[1] = { { \
        *name ## _miniiterfunc, \
        *name ## _objproc, \
        *name ## _miniiterfunc, \
        *name ## _objproc, \
        *name ## _objobjobjproc, \
        *name ## _popitemfunc, \
        *name ## _objobjobjproc, \
        *name ## _miniiterfunc, \
        *name ## _objproc \
    } };
DEFINE_GENERIC_METHODS( MethodError, yp_MethodError ); // for use in methods the type doesn't support
DEFINE_GENERIC_METHODS( InvalidatedError, yp_InvalidatedError ); // for use by Invalidated objects
DEFINE_GENERIC_METHODS( ExceptionMethod, x ); // for use by exception objects; returns "self"

// For use when an object contains no references to other objects
static ypObject *NoRefs_traversefunc( ypObject *x, visitfunc visitor, void *memo ) { return yp_None; } \


/*************************************************************************************************
 * Helpful functions and macros
 *************************************************************************************************/

#ifndef MIN
#define MIN(a,b)   ((a) < (b) ? (a) : (b))
#define MAX(a,b)   ((a) > (b) ? (a) : (b))
#endif

// Issues a breakpoint if the debugger is attached, on supported platforms
// TODO Debug only
// FIXME Use this only at the point the error is "raised"
#if 0 && defined( _MSC_VER )
static void yp_breakonerr( ypObject *err ) {
    if( !IsDebuggerPresent( ) ) return;
    if( err != yp_TypeError ) return; // FIXME temporary, but shows how to trap a certain type of error
    DebugBreak( );
}
#else
#define yp_breakonerr( err )
#endif

// Functions that return nohtyP objects simply need to return the error object to "raise" it
// Use this as "return_yp_ERR( x, yp_ValueError );" to return the error properly
#define return_yp_ERR( _err ) \
    do { ypObject *_yp_ERR_err = (_err); yp_breakonerr( _yp_ERR_err ); return _yp_ERR_err; } while( 0 )

// Functions that modify their inputs take a "ypObject **x".
// Use this as "yp_INPLACE_ERR( x, yp_ValueError );" to discard x and set it to an exception
#define yp_INPLACE_ERR( ob, _err ) \
    do { ypObject *_yp_ERR_err = (_err); yp_breakonerr( _yp_ERR_err ); yp_decref( *(ob) ); *(ob) = (_yp_ERR_err); } while( 0 )
// Use this as "return_yp_INPLACE_ERR( x, yp_ValueError );" to return the error properly
#define return_yp_INPLACE_ERR( ob, _err ) \
    do { yp_INPLACE_ERR( (ob), (_err) ); return; } while( 0 )

// Functions that return C values take a "ypObject **exc" that are only modified on error and are
// not discarded beforehand; they also need to return a valid C value
#define return_yp_CEXC_ERR( retval, exc, _err ) \
    do { ypObject *_yp_ERR_err = (_err); yp_breakonerr( _yp_ERR_err ); *(exc) = (_yp_ERR_err); return retval; } while( 0 )

// When an object encounters an unknown type, there are three possible cases:
//  - it's an invalidated object, so return yp_InvalidatedError
//  - it's an exception, so return it
//  - it's some other type, so return yp_TypeError
#define yp_BAD_TYPE( bad_ob ) ( \
    ypObject_TYPE_PAIR_CODE( bad_ob ) == ypInvalidated_CODE ? \
        yp_InvalidatedError : \
    ypObject_TYPE_PAIR_CODE( bad_ob ) == ypException_CODE ? \
        (bad_ob) : \
    /* else */ \
        yp_TypeError )
#define return_yp_BAD_TYPE( bad_ob ) \
    return_yp_ERR( yp_BAD_TYPE( bad_ob ) )
#define return_yp_INPLACE_BAD_TYPE( ob, bad_ob ) \
    return_yp_INPLACE_ERR( (ob), yp_BAD_TYPE( bad_ob ) )
#define return_yp_CEXC_BAD_TYPE( retval, exc, bad_ob ) \
    return_yp_CEXC_ERR( (retval), (exc), yp_BAD_TYPE( bad_ob ) )

// Return sizeof for a structure member
#define yp_sizeof_member( structType, member ) \
    sizeof( ((structType *)0)->member )

// For N functions (that take variable arguments); to be used as follows:
//      return_yp_V_FUNC( ypObject *, yp_foobarV, (x, n, args), n )
// v_func_args must end in the identifier "args", which is declared internal to the macro.
#define return_yp_V_FUNC( v_func_rettype, v_func, v_func_args, last_fixed ) \
    do {v_func_rettype retval; \
        va_list args; \
        va_start( args, last_fixed ); \
        retval = v_func v_func_args; \
        va_end( args ); \
        return retval; } while( 0 )

// As above, but for functions without a return value
#define return_yp_V_FUNC_void( v_func, v_func_args, last_fixed ) \
    do {va_list args; \
        va_start( args, last_fixed ); \
        v_func v_func_args; \
        va_end( args ); \
        return; } while( 0 )

// As above, but for "K"-functions
#define return_yp_K_FUNC        return_yp_V_FUNC
#define return_yp_K_FUNC_void   return_yp_V_FUNC_void

// Prime multiplier used in string and various other hashes
// XXX Adapted from Python's _PyHASH_MULTIPLIER
#define _ypHASH_MULTIPLIER 1000003  // 0xf4243

// Return the hash of the given number of bytes; always succeeds
// XXX Adapted from Python's _Py_HashBytes
// FIXME On the Release build, this seems to return inconsistent results
static yp_hash_t yp_HashBytes( yp_uint8_t *p, yp_ssize_t len )
{
    yp_uhash_t x;
    yp_ssize_t i;

    if( len == 0 ) return 0;
    x ^= (yp_uhash_t) *p << 7;
    for (i = 0; i < len; i++) {
        x = (_ypHASH_MULTIPLIER * x) ^ (yp_uhash_t) *p++;
    }
    x ^= (yp_uhash_t) len;
    if (x == ypObject_HASH_INVALID) x -= 1;
    return x;
}

// TODO Make this configurable via yp_initialize
static int _yp_recursion_limit = 1000;

#if 0
#include <stdio.h>
#define DEBUG0( fmt ) do { _flushall( ); printf( fmt "\n" ); _flushall( ); } while( 0 )
#define DEBUG( fmt, ... ) do { _flushall( ); printf( fmt "\n", __VA_ARGS__ ); _flushall( ); } while( 0 )
#else
#define DEBUG0( fmt )
#define DEBUG( fmt, ... )
#endif


/*************************************************************************************************
 * nohtyP memory allocations
 *************************************************************************************************/

// The standard C malloc/realloc are inefficient in a couple of ways:
//  - realloc has the potential to copy large amounts of data between buffers, data which may be
//  partially or completely replaced after the realloc
//  - malloc/realloc may allocate more memory than requested, memory that _could_ be used if we
//  only knew how much there was
// yp_malloc, yp_malloc_resize, and yp_free are the top level functions of nohtyP's memory
// allocation scheme, designed to overcome these inefficiencies.  There are several implementations
// of these APIs depending on the target platform; you can also provide your own versions via
// yp_initialize.

// Dummy memory allocation functions that always fail, to ensure yp_initialize is called first
static void *_dummy_yp_malloc( yp_ssize_t *actual, yp_ssize_t size ) { return NULL; }
static void *_dummy_yp_malloc_resize( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra ) { return NULL; }
static void _dummy_yp_free( void *p ) { }

// Allocates at least size bytes of memory, setting *actual to the actual amount of memory
// allocated, and returning the pointer to the buffer.  On error, returns NULL, and *actual is
// undefined.  This will allocate memory even when size==0.
static void *(*yp_malloc)( yp_ssize_t *actual, yp_ssize_t size ) = _dummy_yp_malloc;

// Resizes the given buffer in-place if possible, returning p; otherwise, allocates a new buffer
// and returns a pointer to it.  This function *never* copies data to the new buffer and *never*
// frees the old buffer: it is up to the caller to do this if a new buffer is returned.  In either
// case, *actual is set to the actual amount of memory allocated by the returned pointer.  The
// resized/new buffer will be at least size bytes; extra is a hint as to how much the buffer should
// be over-allocated.  On error, returns NULL, p is not modified, and *actual is undefined.  This
// will allocate memory even when size==0.
static void *(*yp_malloc_resize)( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra ) = _dummy_yp_malloc_resize;

// Frees memory returned by yp_malloc and yp_malloc_resize.
static void (*yp_free)( void *p );

// Microsoft gives a couple options for heaps; let's stick with the standard malloc/free plus
// _msize and _expand
#ifdef _MSC_VER
#include <malloc.h>
#include <crtdbg.h> // For debugging only
static void *_default_yp_malloc( yp_ssize_t *actual, yp_ssize_t size )
{
    void *p;
    if( size < 1 ) size = 1;
    p = malloc( (size_t) size );
    if( p == NULL ) return NULL;
    *actual = (yp_ssize_t) _msize( p );
    // TODO should we check that *actual is > 0?
    DEBUG( "malloc: 0x%08X %d bytes", p, *actual );
    return p;
}
static void *_default_yp_malloc_resize( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra )
{
    void *newp;
    if( size < 1 ) size = 1;
    if( extra < 0 ) extra = 0;

    newp = _expand( p, (size_t) size );
    if( newp == NULL ) {
        newp = malloc( (size_t) (size+extra) );
        if( newp == NULL ) return NULL;
    }
    *actual = (yp_ssize_t) _msize( newp );
    // TODO should we check that *actual is > 0?
    DEBUG( "malloc_resize: 0x%08X %d bytes  (was 0x%08X)", newp, *actual, p );
    return newp;
}
static void _default_yp_free( void *p ) {
    free( p );
    DEBUG( "free: 0x%08X", p );
}

// If all else fails, rely on the standard C malloc/free functions
#else
// Rounds allocations up to a multiple that should be easy for most heaps without wasting space for
// the smallest objects (ie ints)
#define _yp_DEFAULT_MALLOC_ROUNDTO (16)
static yp_ssize_t _default_yp_malloc_good_size( yp_ssize_t size )
{
    yp_ssize_t diff;
    if( size <= _yp_DEFAULT_MALLOC_ROUNDTO ) return _yp_DEFAULT_MALLOC_ROUNDTO;
    diff = size % _yp_DEFAULT_MALLOC_ROUNDTO;
    if( diff > 0 ) size += _yp_DEFAULT_MALLOC_ROUNDTO - diff;
    return size;
}
static void *_default_yp_malloc( yp_ssize_t *actual, yp_ssize_t size )
{
    *actual = _default_yp_malloc_good_size( size );
    if( *actual < 1 ) return NULL;
    return malloc( *actual );
}
static void *_default_yp_malloc_resize( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra )
{
    if( size < 1 ) size = 1;
    if( extra < 0 ) extra = 0;
    *actual = _default_yp_malloc_good_size( size+extra );
    if( *actual < 1 ) return NULL;
    return malloc( *actual );
}
static void (*_default_yp_free)( void *p ) = free;
#endif


/*************************************************************************************************
 * nohtyP object allocations
 *************************************************************************************************/

// FIXME this section could use another pass to review and clean up docs/code/etc

// Declares the ob_inline_data array for container object structures
#define yp_INLINE_DATA _yp_INLINE_DATA

// Returns a malloc'd buffer for fixed, non-container objects, or exception on failure
static ypObject *_ypMem_malloc_fixed( yp_ssize_t sizeof_obStruct, int type )
{
    yp_ssize_t size;
    ypObject *ob = (ypObject *) yp_malloc( &size, sizeof_obStruct );
    if( ob == NULL ) return yp_MemoryError;
    ob->ob_alloclen = ypObject_ALLOCLEN_INVALID;
    ob->ob_data = NULL;
    ob->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 );
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = ypObject_LEN_INVALID;
    DEBUG( "MALLOC_FIXED: type %d 0x%08X", type, ob );
    return ob;
}
#define ypMem_MALLOC_FIXED( obStruct, type ) _ypMem_malloc_fixed( sizeof( obStruct ), (type) )

// Returns a malloc'd buffer for a container object holding alloclen elements in-line, or exception
// on failure.  The container can neither grow nor shrink after allocation.  ob_inline_data in
// obStruct is used to determine the element size and ob_data; ob_len is set to zero.  alloclen
// cannot be negative; ob_alloclen may be larger than requested.
static ypObject *_ypMem_malloc_container_inline(
        yp_ssize_t offsetof_inline, yp_ssize_t sizeof_elems, int type, yp_ssize_t alloclen )
{
    yp_ssize_t size;
    ypObject *ob;
    if( alloclen >= ypObject_ALLOCLEN_INVALID ) return yp_SystemLimitationError;
    // TODO debug-only assert to ensure alloclen positive

    ob = (ypObject *) yp_malloc( &size, offsetof_inline + (alloclen * sizeof_elems) );
    if( ob == NULL ) return yp_MemoryError;
    // FIXME check for alloclen overflow!
    ob->ob_alloclen = (size - offsetof_inline) / sizeof_elems; // rounds down
    ob->ob_data = ((yp_uint8_t *)ob) + offsetof_inline;
    ob->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 );
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = 0;
    DEBUG( "MALLOC_CONTAINER_INLINE: type %d 0x%08X alloclen %d", type, ob, ob->ob_alloclen );
    return ob;
}
#define ypMem_MALLOC_CONTAINER_INLINE( obStruct, type, alloclen ) \
    _ypMem_malloc_container_inline( offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ), (type), (alloclen) )

// TODO Make this configurable via yp_initialize
// XXX 64-bit PyDictObject is 128 bytes...we are larger!
// XXX Cannot change once objects are allocated
// TODO Make static asserts to ensure that certain-sized objects fit with one allocation
// TODO Ensure this isn't larger than ALOCLEN_INVALID
// TODO Optimize this
#if SIZE_MAX <= 0xFFFFFFFFu // 32-bit (or less) platform
#define _ypMem_ideal_size_DEFAULT (128)
#else
#define _ypMem_ideal_size_DEFAULT (256)
#endif
static yp_ssize_t _ypMem_ideal_size = _ypMem_ideal_size_DEFAULT;

// Returns a malloc'd buffer for a container that may grow or shrink in the future, or exception on
// failure.  A fixed amount of memory is allocated in-line, as per _ypMem_ideal_size.  If this fits
// required elements, it is used, otherwise a separate buffer of required+extra elements is
// allocated.  required and extra cannot be negative; ob_alloclen may be larger than requested.
// FIXME ideal is now extra...fix!
static ypObject *_ypMem_malloc_container_variable(
        yp_ssize_t offsetof_inline, yp_ssize_t sizeof_elems, int type,
        yp_ssize_t required, yp_ssize_t extra )
{
    yp_ssize_t size;
    ypObject *ob;
    if( required >= ypObject_ALLOCLEN_INVALID ) return yp_SystemLimitationError;
    if( required+extra >= ypObject_ALLOCLEN_INVALID ) extra = ypObject_ALLOCLEN_INVALID-1 - required;
    // TODO debug-only asserts to ensure other assumptions

    ob = (ypObject *) yp_malloc( &size, MAX( offsetof_inline, _ypMem_ideal_size ) );
    if( ob == NULL ) return yp_MemoryError;
    // FIXME check for alloclen overflow!
    ob->ob_alloclen = (size - offsetof_inline) / sizeof_elems; // rounds down
    if( required <= ob->ob_alloclen ) {
        ob->ob_data = ((yp_uint8_t *)ob) + offsetof_inline;
    } else {
        ob->ob_data = yp_malloc( &size, (required+extra) * sizeof_elems );
        if( ob->ob_data == NULL ) {
            yp_free( ob );
            return yp_MemoryError;
        }
        // FIXME check for alloclen overflow!
        ob->ob_alloclen = size / sizeof_elems; // rounds down
    }
    ob->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 );
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = 0;
    DEBUG( "MALLOC_CONTAINER_VARIABLE: type %d 0x%08X alloclen %d", type, ob, ob->ob_alloclen );
    return ob;
}
#define ypMem_MALLOC_CONTAINER_VARIABLE( obStruct, type, required, extra ) \
    _ypMem_malloc_container_variable( offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ), (type), (required), (extra) )

// Resizes ob_data, the variable-portion of ob, and returns yp_None.  On error, ob is not modified,
// and an exception is returned.  required is the minimum ob_alloclen required, and the minimum
// number of elements that will be preserved; if required can fit inline, the inline buffer is
// used.  ideal is the ob_alloclen to use if the data does not fit inline and a separate buffer
// must be allocated.  This function will always resize the data, so first check to see if a
// resize is necessary.  Any objects in the truncated section must have already been discarded.
// required cannot be negative and ideal must be no less than required; ob_alloclen may be larger
// than either of these.
// TODO The calculated inlinelen may be smaller than what our initial allocation actually provided
// TODO Let the caller move data around and free the old buffer
static ypObject *_ypMem_realloc_container_variable(
        ypObject *ob, yp_ssize_t offsetof_inline, yp_ssize_t sizeof_elems,
        yp_ssize_t required, yp_ssize_t extra )
{
    void *newptr;
    yp_ssize_t size;
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_ssize_t inlinelen = (_ypMem_ideal_size-offsetof_inline) / sizeof_elems;
    if( inlinelen < 0 ) inlinelen = 0;
    if( required >= ypObject_ALLOCLEN_INVALID ) return yp_SystemLimitationError;
    if( required+extra >= ypObject_ALLOCLEN_INVALID ) extra = ypObject_ALLOCLEN_INVALID-1 - required;
    // TODO debug-only asserts to ensure other assumptions

    // If the minimum required allocation can fit inline, then prefer that over a separate buffer
    if( required <= inlinelen ) {
        // If the data is currently not inline, move it there, then free the other buffer
        if( ob->ob_data != inlineptr ) {
            memcpy( inlineptr, ob->ob_data, required * sizeof_elems );
            yp_free( ob->ob_data );
            ob->ob_data = inlineptr;
        }
        ob->ob_alloclen = inlinelen;
        DEBUG( "REALLOC_CONTAINER_VARIABLE (to inline): 0x%08X alloclen %d", ob, ob->ob_alloclen );
        return yp_None;
    }

    // If the data is currently inline, it must be moved out into a separate buffer
    if( ob->ob_data == inlineptr ) {
        newptr = yp_malloc( &size, (required+extra) * sizeof_elems );
        if( newptr == NULL ) return yp_MemoryError;
        memcpy( newptr, ob->ob_data, inlinelen * sizeof_elems );
        ob->ob_data = newptr;
        // FIXME check for alloclen overflow!
        ob->ob_alloclen = size / sizeof_elems; // rounds down
        DEBUG( "REALLOC_CONTAINER_VARIABLE (from inline): 0x%08X alloclen %d", ob, ob->ob_alloclen );
        return yp_None;
    }

    // Otherwise, let yp_malloc_resize handle moving the data around
    newptr = yp_malloc_resize( &size, ob->ob_data, required * sizeof_elems, extra * sizeof_elems );
    if( newptr == NULL ) return yp_MemoryError;
    memcpy( newptr, ob->ob_data, MIN(ob->ob_alloclen * sizeof_elems, size) );
    yp_free( ob->ob_data );
    ob->ob_data = newptr;
    // FIXME check for alloclen overflow!
    ob->ob_alloclen = size / sizeof_elems; // rounds down
    DEBUG( "REALLOC_CONTAINER_VARIABLE (malloc_resize): 0x%08X alloclen %d", ob, ob->ob_alloclen );
    return yp_None;
}
#define ypMem_REALLOC_CONTAINER_VARIABLE( ob, obStruct, required, ideal ) \
    _ypMem_realloc_container_variable( ob, offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ), (required), (ideal) )

// Frees an object allocated with ypMem_MALLOC_FIXED
// TODO Make this a function so it can have a specific DEBUG statement?
#define ypMem_FREE_FIXED yp_free

// Frees an object allocated with either ypMem_REALLOC_CONTAINER_* macro
static void _ypMem_free_container( ypObject *ob, yp_ssize_t offsetof_inline )
{
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    if( ob->ob_data != inlineptr ) yp_free( ob->ob_data );
    yp_free( ob );
    DEBUG( "FREE_CONTAINER: 0x%08X", ob );
}
#define ypMem_FREE_CONTAINER( ob, obStruct ) \
    _ypMem_free_container( ob, offsetof( obStruct, ob_inline_data ) )


/*************************************************************************************************
 * Object fundamentals
 *************************************************************************************************/

ypObject *yp_incref( ypObject *x )
{
    yp_uint32_t refcnt = ypObject_REFCNT( x );
    if( refcnt >= ypObject_REFCNT_IMMORTAL ) return x; // no-op
    x->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( ypObject_TYPE_CODE( x ), refcnt+1 );
    DEBUG( "incref: 0x%08X refcnt %d", x, ypObject_REFCNT( x ) );
    return x;
}

void yp_increfN( yp_ssize_t n, ... )
{
    va_list args;
    int i;
    va_start( args, n );
    for( i = 0; i < n; i++ ) yp_incref( va_arg( args, ypObject * ) );
    va_end( args );
}

void yp_decref( ypObject *x )
{
    yp_uint32_t refcnt = ypObject_REFCNT( x );
    if( refcnt >= ypObject_REFCNT_IMMORTAL ) return; // no-op

    if( refcnt <= 1 ) {
        // TODO Errors currently ignored...should we log them instead?
        ypObject_TYPE( x )->tp_dealloc( x );
        DEBUG( "decref (dealloc): 0x%08X", x );
    } else {
        x->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( ypObject_TYPE_CODE( x ), refcnt-1 );
        DEBUG( "decref: 0x%08X refcnt %d", x, ypObject_REFCNT( x ) );
    }
}

void yp_decrefN( yp_ssize_t n, ... )
{
    va_list args;
    int i;
    va_start( args, n );
    for( i = 0; i < n; i++ ) yp_decref( va_arg( args, ypObject * ) );
    va_end( args );
}


/*************************************************************************************************
 * Iterators
 *************************************************************************************************/

// _ypIterObject_HEAD shared with ypIterValistObject below
#define _ypIterObject_HEAD \
    ypObject_HEAD \
    yp_int32_t ob_lenhint; \
    yp_uint32_t ob_objlocs; \
    yp_generator_func_t ob_func;
typedef struct {
    _ypIterObject_HEAD
    yp_INLINE_DATA( yp_uint8_t );
} ypIterObject;
// FIXME yp_STATIC_ASSERT( offsetof( ypIterObject, ob_inline_data ) % yp_MAX_ALIGNMENT == 0, alignof_iter_inline_data );

#define ypIter_STATE( i )       ( ((ypObject *)i)->ob_data )
#define ypIter_STATE_SIZE( i )  ( ((ypObject *)i)->ob_alloclen )
// TODO decide where we check for underflow for lenhint
#define ypIter_LENHINT( i )     ( ((ypIterObject *)i)->ob_lenhint )
// ob_objlocs: bit n is 1 if (n*sizeof(ypObject *)) is the offset of an object in state
#define ypIter_OBJLOCS( i )     ( ((ypIterObject *)i)->ob_objlocs )
#define ypIter_FUNC( i )        ( ((ypIterObject *)i)->ob_func )

// Iterator methods

static ypObject *iter_traverse( ypObject *i, visitfunc visitor, void *memo )
{
    yp_uint8_t *p = ypIter_STATE( i );
    yp_uint8_t *p_end = p + ypIter_STATE_SIZE( i );
    yp_uint32_t locs = ypIter_OBJLOCS( i );
    ypObject *result;

    while( locs ) { // while there are still more objects to be found...
        if( locs & 0x1u ) {
            // p is pointing at an object; call visitor
            result = visitor( (ypObject *)p, memo );
            if( yp_isexceptionC( result ) ) return result;
        }
        p += sizeof( ypObject * );
        locs >>= 1;
    }
    return yp_None;
}

// Decrements the reference count of the visited object
static ypObject *_iter_closing_visitor( ypObject *x, void *memo ) {
    yp_decref( x );
    return yp_None;
}

static ypObject *_iter_closed_generator( ypObject *i, ypObject *value ) {
    return yp_StopIteration;
}
static ypObject *iter_close( ypObject *i )
{
    // Let the generator know we're closing
    ypObject *result = ypIter_FUNC( i )( i, yp_GeneratorExit );

    // Close off this iterator
    iter_traverse( i, _iter_closing_visitor, NULL ); // never fails
    ypIter_OBJLOCS( i ) = 0x0u; // they are all discarded now...
    ypIter_LENHINT( i ) = 0;
    ypIter_FUNC( i ) = _iter_closed_generator;

    // Handle the returned value from the generator.  yp_StopIteration and yp_GeneratorExit are not
    // errors.  Any other exception or yielded value _is_ an error, as per Python.
    if( yp_isexceptionCN( result, 2, yp_StopIteration, yp_GeneratorExit ) ) return yp_None;
    if( yp_isexceptionC( result ) ) return result;
    yp_decref( result ); // discard unexpectedly-yielded value
    return yp_RuntimeError;
}

// iter objects can be returned from yp_miniiter...they simply ignore *state
static ypObject *iter_miniiter( ypObject *i, yp_uint64_t *state ) {
    *state = 0; // just in case...
    return yp_incref( i );
}

static ypObject *iter_send( ypObject *i, ypObject *value );
static ypObject *iter_miniiter_next( ypObject *i, yp_uint64_t *state ) {
    return iter_send( i, yp_None );
}

static ypObject *iter_miniiter_lenhint( ypObject *i, yp_uint64_t *state, yp_ssize_t *lenhint ) {
    *lenhint = ypIter_LENHINT( i ) < 0 ? 0 : ypIter_LENHINT( i );
    return yp_None;
}

static ypObject *iter_iter( ypObject *i ) {
    return yp_incref( i );
}

static ypObject *iter_send( ypObject *i, ypObject *value )
{
    // As per Python, when a generator raises an exception, it can't continue to yield values.  If
    // iter_close fails just ignore it: result is already set to an exception.
    ypObject *result = ypIter_FUNC( i )( i, value );
    ypIter_LENHINT( i ) -= 1;
    if( yp_isexceptionC( result ) ) iter_close( i );
    return result;
}

static ypObject *iter_dealloc( ypObject *i ) {
    iter_close( i ); // ignore errors; discards all references
    ypMem_FREE_CONTAINER( i, ypIterObject );
    return yp_None;
}

// A frozen iter behaves like a closed iter; as such, they share the same method table
static ypTypeObject ypIter_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    iter_dealloc,                   // tp_dealloc
    iter_traverse,                  // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    MethodError_objproc,            // tp_bool
    MethodError_objobjproc,         // tp_lt
    MethodError_objobjproc,         // tp_le
    MethodError_objobjproc,         // tp_eq
    MethodError_objobjproc,         // tp_ne
    MethodError_objobjproc,         // tp_ge
    MethodError_objobjproc,         // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    iter_close,                     // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    iter_miniiter,                  // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    iter_miniiter_next,             // tp_miniiter_next
    iter_miniiter_lenhint,          // tp_miniiter_lenhint
    iter_iter,                      // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    iter_send,                      // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

// Public functions

// TODO Python has named this __length_hint__; do same?  http://www.python.org/dev/peps/pep-0424
yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc )
{
    yp_ssize_t lenhint;
    if( ypObject_TYPE_PAIR_CODE( iterator ) != ypFrozenIter_CODE ) {
        return_yp_CEXC_BAD_TYPE( 0, exc, iterator );
    }
    lenhint = ypIter_LENHINT( iterator );
    return lenhint < 0 ? 0 : lenhint;
}

// Hidden (for now) feature: size can be NULL
// TODO what if the requested size was an input that we checked against the size of state
// TODO what if we violated the normal rules and inspected *exc on input: if an exception, return
// as if that error occured
// TODO alternatively/additionally, document that this always succeeds if iterator _is_ one
ypObject *yp_iter_stateX( ypObject *iterator, void **state, yp_ssize_t *size )
{
    if( ypObject_TYPE_PAIR_CODE( iterator ) != ypFrozenIter_CODE ) {
        *state = NULL;
        if( size ) *size = 0;
        return_yp_BAD_TYPE( iterator );
    }
    *state = ypIter_STATE( iterator );
    if( size ) *size = ypIter_STATE_SIZE( iterator );
    return yp_None;
}

// Generator Constructors

// Increments the reference count of the visited object
static ypObject *_iter_constructing_visitor( ypObject *x, void *memo ) {
    yp_incref( x );
    return yp_None;
}

ypObject *yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
        void *state, yp_ssize_t size, int n, ... )
{
    return_yp_V_FUNC( ypObject *, yp_generator_fromstructCV,
            (func, lenhint, state, size, n, args), n );
}
ypObject *yp_generator_fromstructCV( yp_generator_func_t func, yp_ssize_t lenhint,
        void *state, yp_ssize_t size, int n, va_list args )
{
    ypObject *iterator;
    yp_uint32_t objlocs = 0x0u;
    yp_ssize_t objoffset;
    yp_ssize_t objloc_index;

    // TODO if size < 0 return error

    // Determine the location of the objects.  There are a few errors the user could make:
    //  - an offset for a ypObject* that is at least partially outside of state; ignore these
    //  - an unaligned ypObject*, which isn't currently allowed and should never happen
    //  - a larger offset than we can represent with objlocs: a current limitation of nohtyP
    // TODO use yp_divmodL here?
    for( /*n already set*/; n > 0; n-- ) {
        objoffset = va_arg( args, yp_ssize_t );
        if( objoffset < 0 ) continue;
        if( objoffset+sizeof( ypObject * ) > (size_t) size ) continue;
        if( objoffset%sizeof( ypObject * ) != 0 ) return yp_SystemLimitationError;
        objloc_index = objoffset / sizeof( ypObject * );
        if( objloc_index > 31 ) return yp_SystemLimitationError;
        objlocs |= (0x1u << objloc_index);
    }

    // Allocate the iterator
    iterator = ypMem_MALLOC_CONTAINER_INLINE( ypIterObject, ypIter_CODE, size );
    if( yp_isexceptionC( iterator ) ) return iterator;

    // Set attributes, increment reference counts, and return
    iterator->ob_len = ypObject_LEN_INVALID;
    ypIter_LENHINT( iterator ) = lenhint;
    ypIter_FUNC( iterator ) = func;
    memcpy( ypIter_STATE( iterator ), state, size );
    ypIter_OBJLOCS( iterator ) = objlocs;
    iter_traverse( iterator, _iter_constructing_visitor, NULL ); // never fails
    return iterator;
}

// Iter Constructors from Mini Iterator Types
// (Allows full iter objects to be created from types that support the mini iterator protocol)

typedef struct {
    _ypIterObject_HEAD
    ypObject *mi;
    yp_uint64_t mi_state;
} ypMiIterObject;
#define ypMiIter_MI( i )        ( ((ypMiIterObject *)i)->mi )
#define ypMiIter_MI_STATE( i )  ( ((ypMiIterObject *)i)->mi_state )

static ypObject *_ypMiIter_generator( ypObject *i, ypObject *value )
{
    ypObject *mi = ypMiIter_MI( i );
    if( yp_isexceptionC( value ) ) return value; // yp_GeneratorExit, in particular
    return ypObject_TYPE( mi )->tp_miniiter_next( mi, &ypMiIter_MI_STATE( i ) );
}

static ypObject *_ypMiIter_from_miniiter( ypObject *x, miniiterfunc mi_constructor )
{
    ypObject *i;
    ypObject *mi;
    ypObject *result;
    yp_ssize_t lenhint;

    // Allocate the iterator
    i = ypMem_MALLOC_FIXED( ypMiIterObject, ypIter_CODE );
    if( yp_isexceptionC( i ) ) return i;

    // Call the miniiterator "constructor" and get the length hint
    mi = mi_constructor( x, &ypMiIter_MI_STATE( i ) );
    if( yp_isexceptionC( mi ) ) {
        ypMem_FREE_FIXED( i );
        return mi;
    }
    result = ypObject_TYPE( mi )->tp_miniiter_lenhint( mi, &ypMiIter_MI_STATE( i ), &lenhint );
    if( yp_isexceptionC( result ) ) {
        yp_decref( mi );
        ypMem_FREE_FIXED( i );
        return result;
    }

    // Set the attributes and return (mi_state set above)
    i->ob_len = ypObject_LEN_INVALID;
    ypIter_STATE( i ) = &(ypMiIter_MI( i )); // TODO also size?
    ypIter_FUNC( i ) = _ypMiIter_generator;
    ypIter_OBJLOCS( i ) = 0x1u;  // indicates mi at state offset zero
    ypMiIter_MI( i ) = mi;
    ypIter_LENHINT( i ) = lenhint;
    return i;
}

// Assign this to tp_iter for types that support mini iterators
static ypObject *_ypIter_from_miniiter( ypObject *x ) {
    return _ypMiIter_from_miniiter( x, ypObject_TYPE( x )->tp_miniiter );
}

// Assign this to tp_iter_reversed for types that support reversed mini iterators
static ypObject *_ypIter_from_miniiter_rev( ypObject *x ) {
    return _ypMiIter_from_miniiter( x, ypObject_TYPE( x )->tp_miniiter_reversed );
}


// Generic Mini Iterator Methods for Sequences

yp_STATIC_ASSERT( sizeof( yp_uint64_t ) >= sizeof( yp_ssize_t ), ssize_fits_uint64 );

static ypObject *_ypSequence_miniiter( ypObject *x, yp_uint64_t *state ) {
    *state = 0; // start at zero (first element) and count up
    return yp_incref( x );
}

static ypObject *_ypSequence_miniiter_rev( ypObject *x, yp_uint64_t *state ) {
    *state = (yp_uint64_t) -1; // start at -1 (last element) and count down
    return yp_incref( x );
}

// XXX Will change *state even if getindex returns an exception, which could in theory wrap-around
// to a valid index...in theory
static ypObject *_ypSequence_miniiter_next( ypObject *x, yp_uint64_t *_state ) {
    yp_int64_t *state = (yp_int64_t *) _state;
    ypObject *result;
    if( *state >= 0 ) { // we are counting up from the first element
        result = yp_getindexC( x, (*state)++ );
    } else {            // we are counting down from the last element
        result = yp_getindexC( x, (*state)-- );
    }
    return yp_isexceptionC2( result, yp_IndexError ) ? yp_StopIteration : result;
}

// TODO ensure yp_miniiter_lenhintC itself catches hint<0
// TODO needs testing!  This is an otherwise-hidden optimization to pre-allocate objects
static ypObject *_ypSequence_miniiter_lenh( ypObject *x, yp_uint64_t *_state, yp_ssize_t *lenhint )
{
    yp_int64_t *state = (yp_int64_t *) _state;
    ypObject *exc = yp_None;
    yp_ssize_t len = yp_lenC( x, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    if( *state >= 0 ) {
        *lenhint = len - ((yp_ssize_t) *state);
    } else {
        *lenhint = len - ((yp_ssize_t) (-1 - *state));
    }
    return yp_None;
}


/*************************************************************************************************
 * Special (and dangerous) iterators for working with variable arguments of ypObject*s
 *************************************************************************************************/

// XXX Be very careful working with these: only pass them to trusted functions that will not
// attempt to retain a reference to these objects after returning.  These aren't your typical
// objects: they're allocated on the stack.  While this could be dangerous, it also  reduces
// duplicating code between versions that handle va_args and those that handle iterables.

typedef struct {
    _ypIterObject_HEAD
    va_list args;
} _ypIterValistObject;
#define ypIterValist_ARGS( i ) (((_ypIterValistObject *)i)->args)

// The number of arguments is stored in ob_lenhint, which is automatically decremented by yp_next
// after each yielded value.
#define yp_ONSTACK_ITER_VALIST( name, n, args ) \
    _ypIterValistObject _ ## name ## _struct = { \
        _yp_IMMORTAL_HEAD_INIT( ypIter_CODE, NULL, ypObject_LEN_INVALID ), \
        n, 0x0u, _iter_valist_generator, args}; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */

static ypObject *_iter_valist_generator( ypObject *i, ypObject *value )
{
    if( yp_isexceptionC( value ) ) return value;
    if( ypIter_LENHINT( i ) < 1 ) return yp_StopIteration;
    return yp_incref( va_arg( ypIterValist_ARGS( i ), ypObject * ) );
}

// XXX Be _especially_ careful working with this: it always yields the same yp_ONSTACK_ITER_VALIST
// object, which it "revives" for a new pair of arguments every time it's yielded.  Not only are
// you denied retaining references to these objects, you are denied retaining a reference to
// previously-yielded yp_ONSTACK_ITER_VALISTs.
typedef struct {
    _ypIterObject_HEAD
    ypObject *subiter;
} _ypIterKValistObject;
#define ypIterKValist_SUBITER( i ) (((_ypIterKValistObject *)i)->subiter)

// Don't include subiter in ob_objlocs
#define yp_ONSTACK_ITER_KVALIST( name, n, args ) \
    _ypIterValistObject _ ## name ## _subiter_struct = { \
        _yp_IMMORTAL_HEAD_INIT( ypIter_CODE, NULL, ypObject_LEN_INVALID ), \
        0, 0x0u, _iter_closed_generator, args}; \
    _ypIterKValistObject _ ## name ## _struct = { \
        _yp_IMMORTAL_HEAD_INIT( ypIter_CODE, NULL, ypObject_LEN_INVALID ), \
        n, 0x0u, _iter_kvalist_generator, (ypObject *) &_ ## name ## _subiter_struct }; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */

static ypObject *_iter_kvalist_generator( ypObject *i, ypObject *value )
{
    yp_ssize_t left;
    ypObject *subiter;

    if( yp_isexceptionC( value ) ) return value;
    if( ypIter_LENHINT( i ) < 1 ) return yp_StopIteration;

    // Ensure the sub-iterator consumed all the items from va_list it should have
    subiter = ypIterKValist_SUBITER( i );
    for( left = ypIter_LENHINT( subiter ); left > 0; left-- ) {
        va_arg( ypIterValist_ARGS( subiter ), ypObject * );
    }

    // Reset the sub-iterator, and yield it again
    ypIter_LENHINT( subiter ) = 2;
    ypIter_FUNC( subiter ) = _iter_valist_generator;
    return subiter;
}


/*************************************************************************************************
 * Freezing, "unfreezing", and invalidating
 *************************************************************************************************/

// TODO If len==0, replace it with the immortal "zero-version" of the type
//  WAIT! I can't do that, because that won't freeze the original and others might be referencing
//  the original so won't see it as frozen now.
//  SO! Still freeze the original, but then also replace it with the zero-version
// FIXME rethink what we do generically in this function, and what we delegate to tp_freeze, and
// also when we call tp_freeze
static ypObject *_yp_freeze( ypObject *x )
{
    int oldCode = ypObject_TYPE_CODE( x );
    int newCode = ypObject_TYPE_CODE_AS_FROZEN( oldCode );
    ypTypeObject *newType;
    ypObject *exc = yp_None;

    // Check if it's already frozen (no-op) or if it can't be frozen (error)
    if( oldCode == newCode ) return yp_None;
    newType = ypTypeTable[newCode];
    if( newType == NULL ) return yp_TypeError;  // TODO make this never happen: such objects should be closed (or invalidated?) instead

    // Freeze the object, cache the final hash (via yp_hashC), possibly reduce memory usage, etc
    exc = newType->tp_freeze( x );
    if( yp_isexceptionC( exc ) ) return exc;
    ypObject_SET_TYPE_CODE( x, newCode );
    x->ob_hash = _ypObject_HASH_INVALID; // just in case
    yp_hashC( x, &exc );
    return exc;
}

void yp_freeze( ypObject **x )
{
    ypObject *result = _yp_freeze( *x );
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( x, result );
}

static ypObject *_yp_deepfreeze( ypObject *x, void *_memo )
{
    ypObject *memo = (ypObject *) _memo;
    ypObject *id;
    ypObject *result;

    // Avoid recursion: we only have to visit each object once
    id = yp_intC( (yp_int64_t) x );
    result = yp_pushuniqueE( memo, id );
    yp_decref( id );
    if( yp_isexceptionC( result ) ) {
        if( yp_isexceptionC2( result, yp_KeyError ) ) return yp_None; // already in set
        return result;
    }

    // Freeze current object before going deep
    result = _yp_freeze( x );
    if( yp_isexceptionC( result ) ) return result;
    // TODO tp_traverse should return if its visitor returns exception...?  Or should we track
    // additional data in what is currently memo?
    return ypObject_TYPE( x )->tp_traverse( x, _yp_deepfreeze, memo );
}

void yp_deepfreeze( ypObject **x )
{
    ypObject *memo = yp_setN( 0 );
    ypObject *result = _yp_deepfreeze( *x, memo );
    yp_decref( memo );
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( x, result );
}

// Use this, and a memo of NULL, as the visitor for shallow copies
static ypObject *yp_shallowcopy_visitor( ypObject *x, void *memo ) {
    return yp_incref( x );
}

ypObject *yp_unfrozen_copy( ypObject *x ) {
    return ypObject_TYPE( x )->tp_unfrozen_copy( x, yp_shallowcopy_visitor, NULL );
}

// XXX Remember: deep copies always copy everything except hashable (immutable) immortals...and
// maybe even those should be copied as well...or just those that contain other objects
static ypObject *_yp_unfrozen_deepcopy( ypObject *x, void *memo ) {
    // TODO don't forget to discard the new objects on error
    // TODO trap recursion & test
    return yp_NotImplementedError;
}

ypObject *yp_unfrozen_deepcopy( ypObject *x ) {
    ypObject *memo = yp_dictK( 0 );
    ypObject *result = _yp_unfrozen_deepcopy( x, memo );
    yp_decref( memo );
    return result;
}

ypObject *yp_frozen_copy( ypObject *x ) {
    if( !ypObject_IS_MUTABLE( x ) ) return yp_incref( x );
    return ypObject_TYPE( x )->tp_frozen_copy( x, yp_shallowcopy_visitor, NULL );
}

// XXX Remember: deep copies always copy everything except hashable (immutable) immortals...and
// maybe even those should be copied as well...or just those that contain other objects
static ypObject *_yp_frozen_deepcopy( ypObject *x, void *memo ) {
    // TODO trap recursion & test
    return yp_NotImplementedError;
}

ypObject *yp_frozen_deepcopy( ypObject *x ) {
    ypObject *memo = yp_dictK( 0 );
    ypObject *result = _yp_frozen_deepcopy( x, memo );
    yp_decref( memo );
    return result;
}

ypObject *yp_copy( ypObject *x ) {
    return ypObject_IS_MUTABLE( x ) ? yp_unfrozen_copy( x ) : yp_frozen_copy( x );
}

// XXX Remember: deep copies always copy everything except hashable (immutable) immortals...and
// maybe even those should be copied as well...or just those that contain other objects
static ypObject *_yp_deepcopy( ypObject *x, void *memo ) {
    // TODO trap recursion & test
    return yp_NotImplementedError;
}

ypObject *yp_deepcopy( ypObject *x ) {
    ypObject *memo = yp_dictK( 0 );
    ypObject *result = _yp_deepcopy( x, memo );
    yp_decref( memo );
    return result;
}

void yp_invalidate( ypObject **x );

void yp_deepinvalidate( ypObject **x );


/*************************************************************************************************
 * Boolean operations, comparisons, and generic object operations
 *************************************************************************************************/

// Returns 1 if the bool object is true, else 0; only valid on bool objects!  The return can also
// be interpreted as the value of the boolean.
// XXX This assumes that yp_True and yp_False are the only two bools
#define ypBool_IS_TRUE_C( b ) ( (b) == yp_True )

#define ypBool_FROM_C( cond ) ( (cond) ? yp_True : yp_False )

// If you know that b is either yp_True, yp_False, or an exception, use this
// XXX b should be a variable, _not_ an expression, as it's evaluated up to three times
#define ypBool_NOT( b ) ( b == yp_True ? yp_False : \
                         (b == yp_False ? yp_True : b))
ypObject *yp_not( ypObject *x ) {
    ypObject *result = yp_bool( x );
    return ypBool_NOT( result );
}

ypObject *yp_or( ypObject *x, ypObject *y )
{
    ypObject *b = yp_bool( x );
    if( yp_isexceptionC( b ) ) return b;
    if( b == yp_False ) return yp_incref( y );
    return yp_incref( x );
}

ypObject *yp_orN( int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_orV, (n, args), n );
}
ypObject *yp_orV( int n, va_list args )
{
    ypObject *x;
    ypObject *b;
    if( n == 0 ) return yp_False;
    for( /*n already set*/; n > 1; n-- ) {
        x = va_arg( args, ypObject * );
        b = yp_bool( x );
        if( yp_isexceptionC( b ) ) return b;
        if( b == yp_True ) return yp_incref( x );
    }
    // If everything else was false, we always return the last object
    return yp_incref( va_arg( args, ypObject * ) );
}

ypObject *yp_anyN( int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_anyV, (n, args), n );
}
ypObject *yp_anyV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return yp_any( iter_args );
}
// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
ypObject *yp_any( ypObject *iterable )
{
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *x;
    ypObject *result = yp_False;

    mi = yp_miniiter( iterable, &mi_state ); // new ref
    while( 1 ) {
        x = yp_miniiter_next( mi, &mi_state ); // new ref
        if( yp_isexceptionC2( x, yp_StopIteration ) ) break;
        result = yp_bool( x );
        yp_decref( x );
        if( result != yp_False ) break; // exit on yp_True or exception
    }
    yp_decref( mi );
    return result;
}

ypObject *yp_and( ypObject *x, ypObject *y )
{
    ypObject *b = yp_bool( x );
    if( yp_isexceptionC( b ) ) return b;
    if( b == yp_False ) return yp_incref( x );
    return yp_incref( y );
}

ypObject *yp_andN( int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_andV, (n, args), n );
}
ypObject *yp_andV( int n, va_list args )
{
    ypObject *x;
    ypObject *b;
    if( n == 0 ) return yp_True;
    for( /*n already set*/; n > 1; n-- ) {
        x = va_arg( args, ypObject * );
        b = yp_bool( x );
        if( yp_isexceptionC( b ) ) return b;
        if( b == yp_False ) return yp_incref( x );
    }
    // If everything else was true, we always return the last object
    return yp_incref( va_arg( args, ypObject * ) );
}

ypObject *yp_allN( int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_allV, (n, args), n );
}
ypObject *yp_allV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return yp_all( iter_args );
}
// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
ypObject *yp_all( ypObject *iterable )
{
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *x;
    ypObject *result = yp_True;

    mi = yp_miniiter( iterable, &mi_state ); // new ref
    while( 1 ) {
        x = yp_miniiter_next( mi, &mi_state ); // new ref
        if( yp_isexceptionC2( x, yp_StopIteration ) ) break;
        result = yp_bool( x );
        yp_decref( x );
        if( result != yp_True ) break; // exit on yp_False or exception
    }
    yp_decref( mi );
    return result;
}

// Defined here are yp_lt, yp_le, yp_eq, yp_ne, yp_ge, and yp_gt
// XXX yp_ComparisonNotImplemented should _never_ be seen outside of comparison functions
// TODO Comparison functions have the possibility of recursion; trap (also, add tests)
ypObject * const yp_ComparisonNotImplemented;
#define _ypBool_PUBLIC_CMP_FUNCTION( name, reflection, defval ) \
ypObject *yp_ ## name( ypObject *x, ypObject *y ) { \
    ypTypeObject *type = ypObject_TYPE( x ); \
    ypObject *result = type->tp_ ## name( x, y ); \
    if( result != yp_ComparisonNotImplemented ) return result; \
    type = ypObject_TYPE( y ); \
    result = type->tp_ ## reflection( y, x ); \
    if( result != yp_ComparisonNotImplemented ) return result; \
    return defval; \
}
_ypBool_PUBLIC_CMP_FUNCTION( lt, gt, yp_TypeError );
_ypBool_PUBLIC_CMP_FUNCTION( le, ge, yp_TypeError );
_ypBool_PUBLIC_CMP_FUNCTION( eq, eq, ypBool_FROM_C( x == y ) );
_ypBool_PUBLIC_CMP_FUNCTION( ne, ne, ypBool_FROM_C( x != y ) );
_ypBool_PUBLIC_CMP_FUNCTION( ge, le, yp_TypeError );
_ypBool_PUBLIC_CMP_FUNCTION( gt, lt, yp_TypeError );

// XXX Remember, an immutable container may hold mutable objects; yp_hashC must fail in that case
// TODO Need to decide whether to keep pre-computed hash in ypObject and, if so, if we can remove
// the hash from ypSet's element table
// TODO Hash functions (currenthash, mainly) have the possibility of recursion; trap (also: test)
yp_STATIC_ASSERT( ypObject_HASH_INVALID == -1, hash_invalid_is_neg_one );

ypObject * const yp_RecursionLimitError;
static ypObject *_yp_hash_visitor( ypObject *x, void *_memo, yp_hash_t *hash )
{
    int *recursion_depth = (int *) _memo;
    ypObject *result;

    // Check type, cached hash, and recursion depth first
    if( ypObject_IS_MUTABLE( x ) ) return_yp_BAD_TYPE( x );
    if( ypObject_CACHED_HASH( x ) != ypObject_HASH_INVALID ) {
        *hash = ypObject_CACHED_HASH( x );
        return yp_None;
    }
    if( *recursion_depth > _yp_recursion_limit ) return yp_RecursionLimitError;

    *recursion_depth += 1;
    result = ypObject_TYPE( x )->tp_currenthash( x, _yp_hash_visitor, _memo, hash );
    *recursion_depth -= 1;
    if( yp_isexceptionC( result ) ) return result;
    ypObject_CACHED_HASH( x ) = *hash;
    return yp_None;
}
yp_hash_t yp_hashC( ypObject *x, ypObject **exc )
{
    int recursion_depth = 0;
    yp_hash_t hash;
    ypObject *result = _yp_hash_visitor( x, &recursion_depth, &hash );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( ypObject_HASH_INVALID, exc, result );
    return hash;
}

static ypObject *_yp_cachedhash_visitor( ypObject *x, void *_memo, yp_hash_t *hash )
{
    int *recursion_depth = (int *) _memo;
    ypObject *result;

    // Check cached hash, and recursion depth first
    if( !ypObject_IS_MUTABLE( x ) && ypObject_CACHED_HASH( x ) != ypObject_HASH_INVALID ) {
        *hash = ypObject_CACHED_HASH( x );
        return yp_None;
    }
    if( *recursion_depth > _yp_recursion_limit ) return yp_RecursionLimitError;

    *recursion_depth += 1;
    result = ypObject_TYPE( x )->tp_currenthash( x, _yp_cachedhash_visitor, _memo, hash );
    *recursion_depth -= 1;
    return result;
}
yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc )
{
    int recursion_depth = 0;
    yp_hash_t hash;
    ypObject *result = _yp_cachedhash_visitor( x, &recursion_depth, &hash );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( ypObject_HASH_INVALID, exc, result );
    return hash;
}

yp_ssize_t yp_lenC( ypObject *x, ypObject **exc )
{
    yp_ssize_t len = ypObject_CACHED_LEN( x );
    ypObject *result;

    if( len >= 0 ) return len;
    result = ypObject_TYPE( x )->tp_len( x, &len );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( 0, exc, result );
    if( len < 0 ) return_yp_CEXC_ERR( 0, exc, yp_SystemError ); // tp_len should not return <0
    return len;
}


/*************************************************************************************************
 * Invalidated Objects
 *************************************************************************************************/

static ypTypeObject ypInvalidated_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                               // tp_name

    // Object fundamentals
    MethodError_objproc,                // tp_dealloc
    NoRefs_traversefunc,                // tp_traverse
    NULL,                               // tp_str
    NULL,                               // tp_repr

    // Freezing, copying, and invalidating
    InvalidatedError_objproc,           // tp_freeze
    InvalidatedError_traversefunc,      // tp_unfrozen_copy
    InvalidatedError_traversefunc,      // tp_frozen_copy
    InvalidatedError_objproc,           // tp_invalidate

    // Boolean operations and comparisons
    InvalidatedError_objproc,           // tp_bool
    InvalidatedError_objobjproc,        // tp_lt
    InvalidatedError_objobjproc,        // tp_le
    InvalidatedError_objobjproc,        // tp_eq
    InvalidatedError_objobjproc,        // tp_ne
    InvalidatedError_objobjproc,        // tp_ge
    InvalidatedError_objobjproc,        // tp_gt

    // Generic object operations
    InvalidatedError_hashfunc,          // tp_currenthash
    InvalidatedError_objproc,           // tp_close

    // Number operations
    InvalidatedError_NumberMethods,     // tp_as_number

    // Iterator operations
    InvalidatedError_miniiterfunc,      // tp_miniiter
    InvalidatedError_miniiterfunc,      // tp_miniiter_reversed
    InvalidatedError_miniiterfunc,      // tp_miniiter_next
    InvalidatedError_miniiter_lenhfunc, // tp_miniiter_lenhint
    InvalidatedError_objproc,           // tp_iter
    InvalidatedError_objproc,           // tp_iter_reversed
    InvalidatedError_objobjproc,        // tp_send

    // Container operations
    InvalidatedError_objobjproc,        // tp_contains
    InvalidatedError_lenfunc,           // tp_len
    InvalidatedError_objobjproc,        // tp_push
    InvalidatedError_objproc,           // tp_clear
    InvalidatedError_objproc,           // tp_pop
    InvalidatedError_objobjobjproc,     // tp_remove
    InvalidatedError_objobjobjproc,     // tp_getdefault
    InvalidatedError_objobjobjproc,     // tp_setitem
    InvalidatedError_objobjproc,        // tp_delitem

    // Sequence operations
    InvalidatedError_SequenceMethods,   // tp_as_sequence

    // Set operations
    InvalidatedError_SetMethods,        // tp_as_set

    // Mapping operations
    InvalidatedError_MappingMethods     // tp_as_mapping
};


/*************************************************************************************************
 * Exceptions
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct {
    ypObject_HEAD
    ypObject *name;
    ypObject *super;
} ypExceptionObject;
#define _ypException_NAME( e )  ( ((ypExceptionObject *)e)->name )
#define _ypException_SUPER( e ) ( ((ypExceptionObject *)e)->super )

static ypTypeObject ypException_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                               // tp_name

    // Object fundamentals
    MethodError_objproc,                // tp_dealloc
    NoRefs_traversefunc,                // tp_traverse
    NULL,                               // tp_str
    NULL,                               // tp_repr

    // Freezing, copying, and invalidating
    ExceptionMethod_objproc,            // tp_freeze
    ExceptionMethod_traversefunc,       // tp_unfrozen_copy
    ExceptionMethod_traversefunc,       // tp_frozen_copy
    ExceptionMethod_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    ExceptionMethod_objproc,            // tp_bool
    ExceptionMethod_objobjproc,         // tp_lt
    ExceptionMethod_objobjproc,         // tp_le
    ExceptionMethod_objobjproc,         // tp_eq
    ExceptionMethod_objobjproc,         // tp_ne
    ExceptionMethod_objobjproc,         // tp_ge
    ExceptionMethod_objobjproc,         // tp_gt

    // Generic object operations
    ExceptionMethod_hashfunc,           // tp_currenthash
    ExceptionMethod_objproc,            // tp_close

    // Number operations
    ExceptionMethod_NumberMethods,      // tp_as_number

    // Iterator operations
    ExceptionMethod_miniiterfunc,       // tp_miniiter
    ExceptionMethod_miniiterfunc,       // tp_miniiter_reversed
    ExceptionMethod_miniiterfunc,       // tp_miniiter_next
    ExceptionMethod_miniiter_lenhfunc,  // tp_miniiter_lenhint
    ExceptionMethod_objproc,            // tp_iter
    ExceptionMethod_objproc,            // tp_iter_reversed
    ExceptionMethod_objobjproc,         // tp_send

    // Container operations
    ExceptionMethod_objobjproc,         // tp_contains
    ExceptionMethod_lenfunc,            // tp_len
    ExceptionMethod_objobjproc,         // tp_push
    ExceptionMethod_objproc,            // tp_clear
    ExceptionMethod_objproc,            // tp_pop
    ExceptionMethod_objobjobjproc,      // tp_remove
    ExceptionMethod_objobjobjproc,      // tp_getdefault
    ExceptionMethod_objobjobjproc,      // tp_setitem
    ExceptionMethod_objobjproc,         // tp_delitem

    // Sequence operations
    ExceptionMethod_SequenceMethods,    // tp_as_sequence

    // Set operations
    ExceptionMethod_SetMethods,         // tp_as_set

    // Mapping operations
    ExceptionMethod_MappingMethods      // tp_as_mapping
};

// No constructors for exceptions; all such objects are immortal

// The immortal exception objects; this should match Python's hierarchy:
//  http://docs.python.org/3/library/exceptions.html
// TODO replace use of yp_IMMORTAL_BYTES with proper string object

#define _yp_IMMORTAL_EXCEPTION_SUPERPTR( name, superptr ) \
    yp_IMMORTAL_BYTES( name ## _name, #name ); \
    static ypExceptionObject _ ## name ## _struct = { \
        _yp_IMMORTAL_HEAD_INIT( ypException_CODE, NULL, 0 ), \
        (ypObject *) &_ ## name ## _name_struct, superptr }; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */
#define _yp_IMMORTAL_EXCEPTION( name, super ) \
    _yp_IMMORTAL_EXCEPTION_SUPERPTR( name, (ypObject *) &_ ## super ## _struct )

_yp_IMMORTAL_EXCEPTION_SUPERPTR( yp_BaseException, NULL );
  _yp_IMMORTAL_EXCEPTION( yp_SystemExit, yp_BaseException );
  _yp_IMMORTAL_EXCEPTION( yp_KeyboardInterrupt, yp_BaseException );
  _yp_IMMORTAL_EXCEPTION( yp_GeneratorExit, yp_BaseException );

  _yp_IMMORTAL_EXCEPTION( yp_Exception, yp_BaseException );
    _yp_IMMORTAL_EXCEPTION( yp_StopIteration, yp_Exception );

    _yp_IMMORTAL_EXCEPTION( yp_ArithmeticError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_FloatingPointError, yp_ArithmeticError );
      _yp_IMMORTAL_EXCEPTION( yp_OverflowError, yp_ArithmeticError );
      _yp_IMMORTAL_EXCEPTION( yp_ZeroDivisionError, yp_ArithmeticError );

    _yp_IMMORTAL_EXCEPTION( yp_AssertionError, yp_Exception );

    _yp_IMMORTAL_EXCEPTION( yp_AttributeError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_MethodError, yp_AttributeError );

    _yp_IMMORTAL_EXCEPTION( yp_BufferError, yp_Exception );
    _yp_IMMORTAL_EXCEPTION( yp_EOFError, yp_Exception );
    _yp_IMMORTAL_EXCEPTION( yp_ImportError, yp_Exception );

    _yp_IMMORTAL_EXCEPTION( yp_LookupError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_IndexError, yp_LookupError );
      _yp_IMMORTAL_EXCEPTION( yp_KeyError, yp_LookupError );

    _yp_IMMORTAL_EXCEPTION( yp_MemoryError, yp_Exception );

    _yp_IMMORTAL_EXCEPTION( yp_NameError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_UnboundLocalError, yp_NameError );

    _yp_IMMORTAL_EXCEPTION( yp_OSError, yp_Exception );
      // TODO Many subexceptions missing here

    _yp_IMMORTAL_EXCEPTION( yp_ReferenceError, yp_Exception );

    _yp_IMMORTAL_EXCEPTION( yp_RuntimeError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_NotImplementedError, yp_RuntimeError );
        _yp_IMMORTAL_EXCEPTION( yp_ComparisonNotImplemented, yp_NotImplementedError );
      // TODO Document yp_CircularReferenceError and use
      // (Python raises RuntimeError on "maximum recursion depth exceeded", so this fits)
      _yp_IMMORTAL_EXCEPTION( yp_CircularReferenceError, yp_RuntimeError );
      // TODO Same with yp_RecursionLimitError
      _yp_IMMORTAL_EXCEPTION( yp_RecursionLimitError, yp_RuntimeError );

    _yp_IMMORTAL_EXCEPTION( yp_SystemError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_SystemLimitationError, yp_SystemError );

    _yp_IMMORTAL_EXCEPTION( yp_TypeError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_InvalidatedError, yp_TypeError );

    _yp_IMMORTAL_EXCEPTION( yp_ValueError, yp_Exception );
      _yp_IMMORTAL_EXCEPTION( yp_UnicodeError, yp_ValueError );
        _yp_IMMORTAL_EXCEPTION( yp_UnicodeEncodeError, yp_UnicodeError );
        _yp_IMMORTAL_EXCEPTION( yp_UnicodeDecodeError, yp_UnicodeError );
        _yp_IMMORTAL_EXCEPTION( yp_UnicodeTranslateError, yp_UnicodeError );

#define yp_IS_EXCEPTION_C( x ) (ypObject_TYPE_PAIR_CODE( x ) == ypException_CODE)
int yp_isexceptionC( ypObject *x ) {
    return yp_IS_EXCEPTION_C( x );
}
#define yp_isexceptionC yp_IS_EXCEPTION_C // force-inline yp_isexceptionC below

static int _yp_isexceptionC2( ypObject *x, ypObject *exc )
{
    do {
        if( x == exc ) return 1; // x is a (sub)exception of exc
        x = _ypException_SUPER( x );
    } while( x != NULL );
    return 0; // neither x nor its superexceptions match exc
}

int yp_isexceptionC2( ypObject *x, ypObject *exc )
{
    if( !yp_IS_EXCEPTION_C( x ) ) return 0; // not an exception
    return _yp_isexceptionC2( x, exc );
}

int yp_isexceptionCN( ypObject *x, int n, ... )
{
    va_list args;

    if( !yp_IS_EXCEPTION_C( x ) ) return 0;

    va_start( args, n ); // remember va_end
    for( /*n already set*/; n > 0; n-- ) {
        if( _yp_isexceptionC2( x, va_arg( args, ypObject * ) ) ) {
            va_end( args );
            return 1;
        }
    }
    va_end( args );
    return 0;
}


/*************************************************************************************************
 * None
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

// FIXME this needs to happen

ypObject *nonetype_bool( ypObject *n ) {
    return yp_False;
}

static ypTypeObject ypNoneType_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    MethodError_objproc,            // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    nonetype_bool,                  // tp_bool
    MethodError_objobjproc,         // tp_lt
    MethodError_objobjproc,         // tp_le
    MethodError_objobjproc,         // tp_eq
    MethodError_objobjproc,         // tp_ne
    MethodError_objobjproc,         // tp_ge
    MethodError_objobjproc,         // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    MethodError_miniiterfunc,       // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

// No constructors for nonetypes; there is exactly one, immortal object
static ypObject _yp_None_struct = yp_IMMORTAL_HEAD_INIT( ypNoneType_CODE, NULL, 0 );
ypObject * const yp_None = &_yp_None_struct;


/*************************************************************************************************
 * Bools
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct {
    ypObject_HEAD
    char value;
} ypBoolObject;
#define _ypBool_VALUE( b ) ( ((ypBoolObject *)b)->value )

static ypObject *bool_bool( ypObject *b ) {
    return b;
}

// Here be bool_lt, bool_le, bool_eq, bool_ne, bool_ge, bool_gt
#define _ypBool_RELATIVE_CMP_FUNCTION( name, operator ) \
static ypObject *bool_ ## name( ypObject *b, ypObject *x ) { \
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBool_CODE ) return yp_ComparisonNotImplemented; \
    return ypBool_FROM_C( _ypBool_VALUE( b ) operator _ypBool_VALUE( x ) ); \
}
_ypBool_RELATIVE_CMP_FUNCTION( lt, < );
_ypBool_RELATIVE_CMP_FUNCTION( le, <= );
_ypBool_RELATIVE_CMP_FUNCTION( eq, == );
_ypBool_RELATIVE_CMP_FUNCTION( ne, != );
_ypBool_RELATIVE_CMP_FUNCTION( ge, >= );
_ypBool_RELATIVE_CMP_FUNCTION( gt, > );

// XXX Adapted from Python's int_hash (now obsolete)
static ypObject *bool_currenthash( ypObject *b,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash )
{
    // This must remain consistent with the other numeric types
    *hash = (yp_hash_t) _ypBool_VALUE( b ); // either 0 or 1
    return yp_None;
}

static ypTypeObject ypBool_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    MethodError_objproc,            // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    bool_bool,                      // tp_bool
    bool_lt,                        // tp_lt
    bool_le,                        // tp_le
    bool_eq,                        // tp_eq
    bool_ne,                        // tp_ne
    bool_ge,                        // tp_ge
    bool_gt,                        // tp_gt

    // Generic object operations
    bool_currenthash,               // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    MethodError_miniiterfunc,       // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};


// No constructors for bools; there are exactly two objects, and they are immortal

// There are exactly two bool objects
static ypBoolObject _yp_True_struct = {yp_IMMORTAL_HEAD_INIT( ypBool_CODE, NULL, 0 ), 1};
ypObject * const yp_True = (ypObject *) &_yp_True_struct;
static ypBoolObject _yp_False_struct = {yp_IMMORTAL_HEAD_INIT( ypBool_CODE, NULL, 0 ), 0};
ypObject * const yp_False = (ypObject *) &_yp_False_struct;


/*************************************************************************************************
 * Integers
 *************************************************************************************************/

// TODO: pre-allocate a set of immortal ints for range(-5, 255), or whatever seems appropriate

// struct _ypIntObject is declared in nohtyP.h for use by yp_IMMORTAL_INT
// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct _ypIntObject ypIntObject;
#define ypInt_VALUE( i ) ( ((ypIntObject *)i)->value )

// Arithmetic code depends on both int and float particulars being defined first
typedef struct {
    ypObject_HEAD
    yp_float_t value;
} ypFloatObject;
#define ypFloat_VALUE( f ) ( ((ypFloatObject *)f)->value )


static ypObject *int_dealloc( ypObject *i ) {
    ypMem_FREE_FIXED( i );
    return yp_None;
}

static ypObject *int_bool( ypObject *i ) {
    return ypBool_FROM_C( ypInt_VALUE( i ) );
}

// Here be int_lt, int_le, int_eq, int_ne, int_ge, int_gt
#define _ypInt_RELATIVE_CMP_FUNCTION( name, operator ) \
static ypObject *int_ ## name( ypObject *i, ypObject *x ) { \
    if( ypObject_TYPE_PAIR_CODE( x ) != ypInt_CODE ) return yp_ComparisonNotImplemented; \
    return ypBool_FROM_C( ypInt_VALUE( i ) operator ypInt_VALUE( x ) ); \
}
_ypInt_RELATIVE_CMP_FUNCTION( lt, < );
_ypInt_RELATIVE_CMP_FUNCTION( le, <= );
_ypInt_RELATIVE_CMP_FUNCTION( eq, == );
_ypInt_RELATIVE_CMP_FUNCTION( ne, != );
_ypInt_RELATIVE_CMP_FUNCTION( ge, >= );
_ypInt_RELATIVE_CMP_FUNCTION( gt, > );

// XXX Adapted from Python's int_hash (now obsolete)
// TODO adapt from long_hash instead, which seems to handle this differently
static ypObject *int_currenthash( ypObject *i,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash )
{
    // This must remain consistent with the other numeric types
    // FIXME int is larger than hash on 32-bit systems, so this truncates data, which we don't
    // want; better is to adapt the long_hash algorithm to this datatype
    *hash = (yp_hash_t) ypInt_VALUE( i );
    if( *hash == ypObject_HASH_INVALID ) *hash -= 1;
    return yp_None;
}

static ypTypeObject ypInt_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    int_dealloc,                    // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    int_bool,                       // tp_bool
    int_lt,                         // tp_lt
    int_le,                         // tp_le
    int_eq,                         // tp_eq
    int_ne,                         // tp_ne
    int_ge,                         // tp_ge
    int_gt,                         // tp_gt

    // Generic object operations
    int_currenthash,                // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    MethodError_miniiterfunc,       // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypTypeObject ypIntStore_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    int_dealloc,                    // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    int_bool,                       // tp_bool
    int_lt,                         // tp_lt
    int_le,                         // tp_le
    int_eq,                         // tp_eq
    int_ne,                         // tp_ne
    int_ge,                         // tp_ge
    int_gt,                         // tp_gt

    // Generic object operations
    int_currenthash,                // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    MethodError_miniiterfunc,       // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    return x + y; // TODO overflow check
}

// XXX Overloading of add/etc currently not supported
void yp_iaddC( ypObject **x, yp_int_t y )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;

    if( x_pair == ypInt_CODE ) {
        yp_int_t result;
        result = yp_addL( ypInt_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypInt_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_intC( result );
        }
        return;

    } else if( x_pair == ypFloat_CODE ) {
        yp_float_t y_asfloat = yp_asfloatL( y, &exc ); // TODO
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_iaddFC( x, y_asfloat );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, *x );
}

void yp_iadd( ypObject **x, ypObject *y )
{
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    ypObject *exc = yp_None;

    if( y_pair == ypInt_CODE ) {
        yp_iaddC( x, ypInt_VALUE( y ) );
        return;

    } else if( y_pair == ypFloat_CODE ) {
        yp_iaddFC( x, ypFloat_VALUE( y ) );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, y );
}

ypObject *yp_add( ypObject *x, ypObject *y )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    ypObject *result;

    if( x_pair != ypInt_CODE && x_pair != ypFloat_CODE ) return_yp_BAD_TYPE( x );
    if( y_pair != ypInt_CODE && y_pair != ypFloat_CODE ) return_yp_BAD_TYPE( y );

    // All numbers hold their data in-line, so freezing a mutable is not heap-inefficient
    result = yp_unfrozen_copy( x );
    yp_iadd( &result, y );
    if( !ypObject_IS_MUTABLE( x ) ) yp_freeze( &result );
    return result;
}

ypObject *yp_intC( yp_int_t value )
{
    ypObject *i = ypMem_MALLOC_FIXED( ypIntObject, ypInt_CODE );
    if( yp_isexceptionC( i ) ) return i;
    ypInt_VALUE( i ) = value;
    DEBUG( "yp_intC: 0x%08X value %d", i, value );
    return i;
}

ypObject *yp_intstoreC( yp_int_t value )
{
    ypObject *i = ypMem_MALLOC_FIXED( ypIntObject, ypIntStore_CODE );
    if( yp_isexceptionC( i ) ) return i;
    ypInt_VALUE( i ) = value;
    DEBUG( "yp_intstoreC: 0x%08X value %d", i, value );
    return i;
}

yp_int_t yp_asintC( ypObject *x, ypObject **exc )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypInt_CODE ) {
        return ypInt_VALUE( x );
    } else if( x_pair == ypFloat_CODE ) {
        return yp_asintFL( ypFloat_VALUE( x ), exc );
    }
    return_yp_CEXC_BAD_TYPE( 0, exc, x );
}

// Defines the conversion functions.  Overflow checking is done by first truncating the value then
// seeing if it equals the stored value.  Note that when yp_asintC raises an exception, it returns
// zero, which can be represented in every integer type, so we won't override any yp_TypeError
// errors.
// TODO review http://blog.reverberate.org/2012/12/testing-for-integer-overflow-in-c-and-c.html
#define _ypInt_PUBLIC_AS_C_FUNCTION( name ) \
yp_ ## name ## _t yp_as ## name ## C( ypObject *x, ypObject **exc ) { \
    yp_int_t asint = yp_asintC( x, exc ); \
    yp_ ## name ## _t retval = (yp_ ## name ## _t) asint; \
    if( (yp_int_t) retval != asint ) return_yp_CEXC_ERR( retval, exc, yp_OverflowError ); \
    return retval; \
}
_ypInt_PUBLIC_AS_C_FUNCTION( int8 );
_ypInt_PUBLIC_AS_C_FUNCTION( uint8 );
_ypInt_PUBLIC_AS_C_FUNCTION( int16 );
_ypInt_PUBLIC_AS_C_FUNCTION( uint16 );
_ypInt_PUBLIC_AS_C_FUNCTION( int32 );
_ypInt_PUBLIC_AS_C_FUNCTION( uint32 );
#if SIZE_MAX <= 0xFFFFFFFFu // 32-bit (or less) platform
yp_STATIC_ASSERT( sizeof( yp_ssize_t ) < sizeof( yp_int_t ), sizeof_yp_ssize_lt_yp_int );
_ypInt_PUBLIC_AS_C_FUNCTION( ssize );
_ypInt_PUBLIC_AS_C_FUNCTION( hash );
#endif

// The functions below assume/assert that yp_int_t is 64 bits
yp_STATIC_ASSERT( sizeof( yp_int_t ) == 8, sizeof_yp_int );
yp_int64_t yp_asint64C( ypObject *x, ypObject **exc ) {
    return yp_asintC( x, exc );
}
yp_uint64_t yp_asuint64C( ypObject *x, ypObject **exc ) {
    yp_int_t asint = yp_asintC( x, exc );
    if( asint < 0 ) return_yp_CEXC_ERR( (yp_uint64_t) asint, exc, yp_OverflowError );
    return (yp_uint64_t) asint;
}

#if SIZE_MAX > 0xFFFFFFFFu // 64-bit (or more) platform
yp_STATIC_ASSERT( sizeof( yp_ssize_t ) == sizeof( yp_int_t ), sizeof_yp_ssize_eq_yp_int );
yp_ssize_t yp_asssizeC( ypObject *x, ypObject **exc ) {
    return yp_asintC( x, exc );
}
yp_hash_t yp_ashashC( ypObject *x, ypObject **exc ) {
    return yp_asintC( x, exc );
}
#endif


/*************************************************************************************************
 * Floats
 *************************************************************************************************/

// ypFloatObject and ypFloat_VALUE are defined above for use by the int code

yp_float_t yp_addFL( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return x + y; // TODO overflow check
}

void yp_iaddFC( ypObject **x, yp_float_t y )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;
    yp_float_t result;

    if( x_pair == ypFloat_CODE ) {
        result = yp_addFL( ypFloat_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypFloat_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_floatC( result );
        }
        return;

    } else if( x_pair == ypInt_CODE ) {
        yp_float_t x_asfloat = yp_asfloatC( *x, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        result = yp_addFL( ypFloat_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_decref( *x );
        *x = yp_floatC( result );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, *x );
}

ypObject *yp_floatC( yp_float_t value )
{
    ypObject *f = ypMem_MALLOC_FIXED( ypFloatObject, ypFloat_CODE );
    if( yp_isexceptionC( f ) ) return f;
    ypFloat_VALUE( f ) = value;
    return f;
}

ypObject *yp_floatstoreC( yp_float_t value )
{
    ypObject *f = ypMem_MALLOC_FIXED( ypFloatObject, ypFloatStore_CODE );
    if( yp_isexceptionC( f ) ) return f;
    ypFloat_VALUE( f ) = value;
    return f;
}

yp_float_t yp_asfloatC( ypObject *x, ypObject **exc )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypInt_CODE ) {
        return yp_asfloatL( ypInt_VALUE( x ), exc );
    } else if( x_pair == ypFloat_CODE ) {
        return ypFloat_VALUE( x );
    }
    return_yp_CEXC_BAD_TYPE( 0.0, exc, x );
}

yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc )
{
    // TODO Implement this as Python does
    return (yp_float_t) x;
}

yp_int_t yp_asintFL( yp_float_t x, ypObject **exc )
{
    // TODO Implement this as Python does
    return (yp_int_t) x;
}


/*************************************************************************************************
 * Indices and slices
 *************************************************************************************************/

// Using the given length, adjusts negative indicies to positive.  Returns yp_IndexError if the
// adjusted index is out-of-bounds, else yp_None.
static ypObject *ypSequence_AdjustIndexC( yp_ssize_t length, yp_ssize_t *i )
{
    if( *i < 0 ) *i += length;
    if( *i < 0 || *i >= length ) return yp_IndexError;
    return yp_None;
}

// Using the given length, in-place converts the given start/stop/step values to valid indices, and
// also calculates the length of the slice.  Returns yp_ValueError if *step is zero, else yp_None;
// there are no out-of-bounds errors with slices.
// XXX yp_SLICE_DEFAULT is yp_SSIZE_T_MIN, which hopefully nobody will try to use as a valid index.
// yp_SLICE_USELEN is yp_SSIZE_T_MAX, which is simply a very large number that is handled the same
// as any value that's greater than length.
// XXX Adapted from PySlice_GetIndicesEx
static ypObject *ypSlice_AdjustIndicesC( yp_ssize_t length, yp_ssize_t *start, yp_ssize_t *stop,
        yp_ssize_t *step, yp_ssize_t *slicelength )
{
    // Adjust step
    if( *step == 0 ) return yp_ValueError;
    if( *step < -yp_SSIZE_T_MAX ) *step = -yp_SSIZE_T_MAX; // ensure *step can be negated

    // Adjust start
    if( *start == yp_SLICE_DEFAULT ) {
        *start = (*step < 0) ? length-1 : 0;
    } else {
        if( *start < 0 ) *start += length;
        if( *start < 0 ) *start = (*step < 0) ? -1 : 0;
        if( *start >= length ) *start = (*step < 0) ? length-1 : length;
    }

    // Adjust stop
    if( *stop == yp_SLICE_DEFAULT ) {
        *stop = (*step < 0) ? -1 : length;
    } else {
        if( *stop < 0 ) *stop += length;
        if( *stop < 0 ) *stop = (*step < 0) ? -1 : 0;
        if( *stop >= length ) *stop = (*step < 0) ? length-1 : length;
    }

    // Calculate slicelength
    if( (*step < 0 && *stop  >= *start) ||
        (*step > 0 && *start >= *stop )    ) {
        *slicelength = 0;
    } else if( *step < 0 ) {
        *slicelength = (*stop - *start + 1) / (*step) + 1;
    } else {
        *slicelength = (*stop - *start - 1) / (*step) + 1;
    }

    return yp_None;
}

// Using the given _adjusted_ values, in-place converts the given start/stop/step values to the
// inverse slice, where *step=-(*step).  Returns an exception on error.
// Adapted from Python's list_ass_subscript
static ypObject *ypSlice_InvertIndicesC( yp_ssize_t *start, yp_ssize_t *stop, yp_ssize_t *step,
        yp_ssize_t slicelength )
{
    if( slicelength < 1 ) return yp_None; // no-op

    if( *step < 0 ) {
        // This comes direct from list_ass_subscript
        *stop = *start + 1;
        *start = *stop + (*step) * (slicelength - 1) - 1;
    } else {
        // This part I've inferred; TODO verify!
        *stop = *start - 1;
        *start = *stop + (*step) * (slicelength - 1) + 1;
    }
    *step = -(*step);

    return yp_None;
}

// Returns the index of the i'th item in the slice with the given adjusted start/step values.  i
// must be in range(slicelength).
#define ypSlice_INDEX( start, step, i )  ((start) + (i)*(step))


/*************************************************************************************************
 * Sequence of bytes
 *************************************************************************************************/

// TODO ensure it is always null-terminated

// struct _ypBytesObject is declared in nohtyP.h for use by yp_IMMORTAL_BYTES
typedef struct _ypBytesObject ypBytesObject;
yp_STATIC_ASSERT( offsetof( ypBytesObject, ob_inline_data ) % yp_MAX_ALIGNMENT == 0, alignof_bytes_inline_data );

#define ypBytes_DATA( b ) ( (yp_uint8_t *) ((ypObject *)b)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypBytes_LEN( b )  ( ((ypObject *)b)->ob_len )

// TODO _yp_bytes_empty (remember NULL terminator)

// Return a new bytes object with uninitialized data of the given length, or an exception
static ypObject *_yp_bytes_new( yp_ssize_t len )
{
    ypObject *b;
    if( len < 0 ) len = 0; // TODO return a new ref to an immortal b'' object
    b = ypMem_MALLOC_CONTAINER_INLINE( ypBytesObject, ypBytes_CODE, len+1 );
    if( yp_isexceptionC( b ) ) return b;
    ypBytes_DATA( b )[len] = '\0';
    ypBytes_LEN( b ) = len;
    return b;
}

// Return a new bytearray object with uninitialized data of the given length, or an exception
// TODO Over-allocate to avoid future resizings
static ypObject *_yp_bytearray_new( yp_ssize_t len )
{
    ypObject *b;
    if( len < 0 ) len = 0;
    b = ypMem_MALLOC_CONTAINER_VARIABLE( ypBytesObject, ypByteArray_CODE, len+1, 0 );
    if( yp_isexceptionC( b ) ) return b;
    ypBytes_DATA( b )[len] = '\0';
    ypBytes_LEN( b ) = len;
    return b;
}

// Return a new bytes or bytearray object (depending on b) with uninitialzed data of the given
// length, or an exception
static ypObject *_ypBytes_new_sametype( ypObject *b, yp_ssize_t len )
{
    if( ypObject_IS_MUTABLE( b ) ) {
        return _yp_bytearray_new( len );
    } else {
        return _yp_bytes_new( len );
    }
}

// Returns a copy of the bytes or bytearray object; the new object will have the given length. If
// len is less than ypBytes_LEN( b ), data is truncated; if equal, returns an exact copy; if
// greater, extra bytes are uninitialized.
static ypObject *_ypBytes_copy( ypObject *b, yp_ssize_t len )
{
    ypObject *newB = _ypBytes_new_sametype( b, len );
    if( yp_isexceptionC( newB ) ) return newB;
    memcpy( ypBytes_DATA( newB ), ypBytes_DATA( b ), MIN( len, ypBytes_LEN( b ) ) );
    return newB;
}

// Shrinks or grows the bytearray; any new bytes are uninitialized.  Returns yp_None on success,
// exception on error.
// TODO over-allocate as appropriate
static ypObject *_ypBytes_resize( ypObject *b, yp_ssize_t newLen )
{
    ypObject *result = ypMem_REALLOC_CONTAINER_VARIABLE( b, ypBytesObject, newLen+1, 0 );
    if( yp_isexceptionC( result ) ) return result;
    ypBytes_DATA( b )[newLen] = '\0';
    ypBytes_LEN( b ) = newLen;
    return yp_None;
}

// If x is a fellow bytes, set *x_data and *x_len.  Otherwise, set *x_data=NULL and *x_len=0.
// TODO note http://bugs.python.org/issue12170 and ensure we stay consistent
static void _ypBytes_coerce_bytes( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypBytes_CODE ) {
        *x_data = ypBytes_DATA( x );
        *x_len = ypBytes_LEN( x );
        return;
    }

    *x_data = NULL;
    *x_len = 0;
    return;
}

// If x is a bool/int in range(256), store value in storage and set *x_data=storage, *x_len=1.  If
// x is a fellow bytes, set *x_data and *x_len.  Otherwise, set *x_data=NULL and *x_len=0.
// TODO: to be correct, ValueError should be raised when int out of range(256)
static void _ypBytes_coerce_intorbytes( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len,
        yp_uint8_t *storage )
{
    ypObject *result;
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypBool_CODE || x_pair == ypInt_CODE ) {
        *storage = yp_asuint8C( x, &result );
        if( !yp_isexceptionC( result ) ) {
            *x_data = storage;
            *x_len = 1;
            return;
        }
    } else if( x_pair == ypBytes_CODE ) {
        *x_data = ypBytes_DATA( x );
        *x_len = ypBytes_LEN( x );
        return;
    }

    *x_data = NULL;
    *x_len = 0;
    return;
}

// TODO Returns yp_None or an exception

static ypObject *bytes_bool( ypObject *b ) {
    return ypBool_FROM_C( ypBytes_LEN( b ) );
}

// Returns yp_None or an exception
static ypObject *bytes_find( ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *i )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    yp_uint8_t storage;
    ypObject *result;
    yp_ssize_t step = 1;
    yp_ssize_t b_rlen;     // remaining length
    yp_uint8_t *b_rdata;   // remaining data

    _ypBytes_coerce_intorbytes( x, &x_data, &x_len, &storage );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &b_rlen );
    if( yp_isexceptionC( result ) ) return result;
    b_rdata = ypBytes_DATA( b ) + start;

    while( b_rlen >= x_len ) {
        if( memcmp( b_rdata, x_data, x_len ) == 0 ) {
            *i = b_rdata - ypBytes_DATA( b );
            return yp_None;
        }
        b_rdata++; b_rlen--;
    }
    *i = -1;
    return yp_None;
}

// Returns yp_True, yp_False, or an exception
static ypObject *bytes_contains( ypObject *b, ypObject *x )
{
    ypObject *result;
    yp_ssize_t i = -1;

    result = bytes_find( b, x, 0, yp_SLICE_USELEN, &i );
    if( yp_isexceptionC( result ) ) return result;
    return ypBool_FROM_C( i >= 0 );
}

// Returns new reference or an exception
static ypObject *bytes_concat( ypObject *b, ypObject *x )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    ypObject *newB;

    _ypBytes_coerce_bytes( x, &x_data, &x_len );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    newB = _ypBytes_copy( b, ypBytes_LEN( b ) + x_len );
    if( yp_isexceptionC( newB ) ) return newB;

    memcpy( ypBytes_DATA( newB )+ypBytes_LEN( b ), x_data, x_len );
    return newB;
}

// Returns yp_None or an exception
static ypObject *bytearray_extend( ypObject *b, ypObject *x )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    ypObject *result;

    _ypBytes_coerce_bytes( x, &x_data, &x_len );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    result = _ypBytes_resize( b, ypBytes_LEN( b ) + x_len );
    if( yp_isexceptionC( result ) ) return result;

    memcpy( ypBytes_DATA( b )+ypBytes_LEN( b ), x_data, x_len );
    return yp_None;
}

// TODO bytes_repeat
// TODO bytes_irepeat

// Returns new reference or an exception
static ypObject *bytes_getindex( ypObject *b, yp_ssize_t i )
{
    ypObject *result = ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i );
    if( yp_isexceptionC( result ) ) return result;
    return yp_intC( ypBytes_DATA( b )[i] );
}

// Returns yp_None or an exception
static ypObject *bytearray_setindex( ypObject *b, yp_ssize_t i, ypObject *x )
{
    ypObject *result = yp_None;
    yp_uint8_t x_value;

    x_value = yp_asuint8C( x, &result );
    if( yp_isexceptionC( result ) ) return result;

    result = ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i );
    if( yp_isexceptionC( result ) ) return result;

    ypBytes_DATA( b )[i] = x_value;
    return yp_None;
}

// Returns yp_None or an exception
static ypObject *bytearray_delindex( ypObject *b, yp_ssize_t i )
{
    ypObject *result;

    result = ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i );
    if( yp_isexceptionC( result ) ) return result;

    // memmove allows overlap; don't bother reallocating
    memmove( ypBytes_DATA( b )+i, ypBytes_DATA( b )+i+1, ypBytes_LEN( b )-i-1 );
    ypBytes_LEN( b ) -= 1;
    return yp_None;
}

// Returns new reference or an exception
static ypObject *bytes_getslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step )
{
    ypObject *result;
    yp_ssize_t newLen;
    ypObject *newB;

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &newLen );
    if( yp_isexceptionC( result ) ) return result;

    newB = _ypBytes_new_sametype( b, newLen );
    if( yp_isexceptionC( newB ) ) return newB;

    if( step == 1 ) {
        memcpy( ypBytes_DATA( newB ), ypBytes_DATA( b )+start, newLen );
    } else {
        yp_ssize_t i;
        for( i = 0; i < newLen; i++ ) {
            ypBytes_DATA( newB )[i] = ypBytes_DATA( b )[ypSlice_INDEX( start, step, i )];
        }
    }
    return newB;
}

// Returns yp_None or an exception
// TODO handle b == x! (and elsewhere?)
static ypObject *bytearray_delslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step );
static ypObject *bytearray_setslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step, ypObject *x )
{
    ypObject *result;
    yp_ssize_t slicelength;
    yp_uint8_t *x_data;
    yp_ssize_t x_len;

    _ypBytes_coerce_bytes( x, &x_data, &x_len );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );
    if( x_len == 0 ) return bytearray_delslice( b, start, stop, step );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;

    if( step == 1 ) {
        if( x_len > slicelength ) {
            // bytearray is growing
            yp_ssize_t growBy = x_len - slicelength;
            yp_ssize_t oldLen = ypBytes_LEN( b );
            result = _ypBytes_resize( b, oldLen + growBy );
            if( yp_isexceptionC( result ) ) return result;
            // memmove allows overlap
            memmove( ypBytes_DATA( b )+stop+growBy, ypBytes_DATA( b )+stop, oldLen-stop );
        } else if( x_len < slicelength ) {
            // bytearray is shrinking
            yp_ssize_t shrinkBy = slicelength - x_len;
            // memmove allows overlap
            memmove( ypBytes_DATA( b )+stop-shrinkBy, ypBytes_DATA( b )+stop,
                    ypBytes_LEN( b )-stop );
            ypBytes_LEN( b ) -= shrinkBy;
        }
        // There are now x_len bytes starting at b[start] waiting for x_data
        memcpy( ypBytes_DATA( b )+start, x_data, x_len );
    } else {
        yp_ssize_t i;
        if( x_len != slicelength ) return yp_ValueError;
        for( i = 0; i < slicelength; i++ ) {
            ypBytes_DATA( b )[ypSlice_INDEX( start, step, i )] = x_data[i];
        }
    }
    return yp_None;
}

// Returns yp_None or an exception
static ypObject *bytearray_delslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step )
{
    ypObject *result;
    yp_ssize_t slicelength;

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;
    if( slicelength < 1 ) return yp_None; // no-op
    if( step < 0 ) ypSlice_InvertIndicesC( &start, &stop, &step, slicelength );

    if( step == 1 ) {
        // One contiguous section
        memmove( ypBytes_DATA( b )+stop-slicelength, ypBytes_DATA( b )+stop,
                ypBytes_LEN( b )-stop );
        ypBytes_LEN( b ) -= slicelength;
    } else {
        return yp_NotImplementedError; // TODO implement
    }
    return yp_None;
}

// TODO bytes_getitem, setitem, delitem...need generic functions to redirect to getindexC et al

// Returns yp_None or an exception
static ypObject *bytes_len( ypObject *b, yp_ssize_t *len )
{
    *len = ypBytes_LEN( b );
    return yp_None;
}

// TODO allow custom min/max methods?

static ypObject *bytes_count( ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *n )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    yp_uint8_t storage;
    ypObject *result;
    yp_ssize_t step = 1;
    yp_ssize_t b_rlen;     // remaining length
    yp_uint8_t *b_rdata;   // remaining data

    _ypBytes_coerce_intorbytes( x, &x_data, &x_len, &storage );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &b_rlen );
    if( yp_isexceptionC( result ) ) return result;
    b_rdata = ypBytes_DATA( b ) + start;

    *n = 0;
    while( b_rlen >= x_len ) {
        if( memcmp( b_rdata, x_data, x_len ) == 0 ) {
            *n += 1;
            b_rdata += x_len; b_rlen -= x_len;
        } else {
            b_rdata += 1; b_rlen -= 1;
        }
    }
    return yp_None;
}

// Returns -1, 0, or 1 as per memcmp
static int _ypBytes_relative_cmp( ypObject *b, ypObject *x ) {
    yp_ssize_t b_len = ypBytes_LEN( b );
    yp_ssize_t x_len = ypBytes_LEN( x );
    int cmp = memcmp( ypBytes_DATA( b ), ypBytes_DATA( x ), MIN( b_len, x_len ) );
    if( cmp == 0 ) cmp = b_len < x_len ? -1 : (b_len > x_len ? 1 : 0);
    return cmp;
}
static ypObject *bytes_lt( ypObject *b, ypObject *x ) {
    if( b == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypBytes_relative_cmp( b, x ) < 0 );
}
static ypObject *bytes_le( ypObject *b, ypObject *x ) {
    if( b == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypBytes_relative_cmp( b, x ) <= 0 );
}
static ypObject *bytes_ge( ypObject *b, ypObject *x ) {
    if( b == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypBytes_relative_cmp( b, x ) >= 0 );
}
static ypObject *bytes_gt( ypObject *b, ypObject *x ) {
    if( b == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypBytes_relative_cmp( b, x ) > 0 );
}

// Returns true (1) if the two bytes/bytearrays are equal.  Size is a quick way to check equality.
// TODO The pre-computed hash, if any, would also be a quick check
static int _ypBytes_are_equal( ypObject *b, ypObject *x ) {
    yp_ssize_t b_len = ypBytes_LEN( b );
    yp_ssize_t x_len = ypBytes_LEN( x );
    if( b_len != x_len ) return 0;
    return memcmp( ypBytes_DATA( b ), ypBytes_DATA( x ), b_len ) == 0;
}
static ypObject *bytes_eq( ypObject *b, ypObject *x ) {
    if( b == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypBytes_are_equal( b, x ) );
}
static ypObject *bytes_ne( ypObject *b, ypObject *x ) {
    if( b == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( !_ypBytes_are_equal( b, x ) );
}

// Must work even for mutables; yp_hash handles caching this value and denying its use for mutables
static ypObject *bytes_currenthash( ypObject *b,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash ) {
    *hash = yp_HashBytes( ypBytes_DATA( b ), ypBytes_LEN( b ) );
    return yp_None;
}

static ypObject *bytes_dealloc( ypObject *b ) {
    ypMem_FREE_CONTAINER( b, ypBytesObject );
    return yp_None;
}

static ypSequenceMethods ypBytes_as_sequence = {
    bytes_getindex,                 // tp_getindex
    bytes_getslice,                 // tp_getslice
    bytes_find,                     // tp_find
    bytes_count,                    // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_extend
    MethodError_objssizeproc,       // tp_irepeat
    MethodError_objssizeobjproc,    // tp_insert
    MethodError_objssizeproc,       // tp_popindex
    MethodError_objproc,            // tp_reverse
    MethodError_sortfunc            // tp_sort
};

static ypTypeObject ypBytes_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    bytes_dealloc,                  // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    bytes_bool,                     // tp_bool
    bytes_lt,                       // tp_lt
    bytes_le,                       // tp_le
    bytes_eq,                       // tp_eq
    bytes_ne,                       // tp_ne
    bytes_ge,                       // tp_ge
    bytes_gt,                       // tp_gt

    // Generic object operations
    bytes_currenthash,              // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    _ypSequence_miniiter,           // tp_miniiter
    _ypSequence_miniiter_rev,       // tp_miniiter_reversed
    _ypSequence_miniiter_next,      // tp_miniiter_next
    _ypSequence_miniiter_lenh,      // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    _ypIter_from_miniiter_rev,      // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    &ypBytes_as_sequence,           // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSequenceMethods ypByteArray_as_sequence = {
    bytes_getindex,                 // tp_getindex
    bytes_getslice,                 // tp_getslice
    bytes_find,                     // tp_find
    bytes_count,                    // tp_count
    bytearray_setindex,             // tp_setindex
    bytearray_setslice,             // tp_setslice
    bytearray_delindex,             // tp_delindex
    bytearray_delslice,             // tp_delslice
    MethodError_objobjproc,         // tp_extend
    MethodError_objssizeproc,       // tp_irepeat
    MethodError_objssizeobjproc,    // tp_insert
    MethodError_objssizeproc,       // tp_popindex
    MethodError_objproc,            // tp_reverse
    MethodError_sortfunc            // tp_sort
};

static ypTypeObject ypByteArray_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    bytes_dealloc,                  // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    bytes_bool,                     // tp_bool
    bytes_lt,                       // tp_lt
    bytes_le,                       // tp_le
    bytes_eq,                       // tp_eq
    bytes_ne,                       // tp_ne
    bytes_ge,                       // tp_ge
    bytes_gt,                       // tp_gt

    // Generic object operations
    bytes_currenthash,              // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    _ypSequence_miniiter,           // tp_miniiter
    _ypSequence_miniiter_rev,       // tp_miniiter_reversed
    _ypSequence_miniiter_next,      // tp_miniiter_next
    _ypSequence_miniiter_lenh,      // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    _ypIter_from_miniiter_rev,      // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    &ypByteArray_as_sequence,       // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};


// Public constructors

static ypObject *_ypBytesC( ypObject *(*allocator)( yp_ssize_t ),
    const yp_uint8_t *source, yp_ssize_t len )
{
    ypObject *b;

    // Allocate an object of the appropriate size
    if( source == NULL ) {
        if( len < 0 ) len = 0;
    } else {
        if( len < 0 ) len = strlen( (const char *) source );
    }
    b = allocator( len );

    // Initialize the data
    if( source == NULL ) {
        memset( ypBytes_DATA( b ), 0, len );
    } else {
        memcpy( ypBytes_DATA( b ), source, len );
    }
    return b;
}
ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len ) {
    return _ypBytesC( _yp_bytes_new, source, len );
}
ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len ) {
    return _ypBytesC( _yp_bytearray_new, source, len );
}


/*************************************************************************************************
 * Sequence of unicode characters
 *************************************************************************************************/

// TODO http://www.python.org/dev/peps/pep-0393/ (flexible string representations)
// struct _ypStrObject is declared in nohtyP.h for use by yp_IMMORTAL_BYTES
typedef struct _ypStrObject ypStrObject;
yp_STATIC_ASSERT( offsetof( ypStrObject, ob_inline_data ) % yp_MAX_ALIGNMENT == 0, alignof_str_inline_data );

// TODO getindex for bytearray (and byte) returns an immutable integer, so getindex for chrarray
// should return an immutable str-of-len-one, which is also consistent with Python's str

// TODO pre-allocate static chrs in, say, range(255), or whatever seems appropriate

// TODO add checks to ensure invalid Unicode codepoints don't get added

#define ypStr_DATA( s ) ( (yp_uint8_t *) ((ypObject *)s)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypStr_LEN( s )  ( ((ypObject *)s)->ob_len )

// TODO _yp_str_empty (remember NULL terminator)

// Return a new str object with uninitialized data of the given length, or an exception
static ypObject *_yp_str_new( yp_ssize_t len )
{
    ypObject *s;
    if( len < 0 ) len = 0; // TODO return a new ref to an immortal "" object
    s = ypMem_MALLOC_CONTAINER_INLINE( ypStrObject, ypStr_CODE, len+1 );
    if( yp_isexceptionC( s ) ) return s;
    ypStr_DATA( s )[len] = '\0';
    ypStr_LEN( s ) = len;
    return s;
}

// Return a new chrarray object with uninitialized data of the given length, or an exception
// TODO Over-allocate to avoid future resizings
static ypObject *_yp_chrarray_new( yp_ssize_t len )
{
    ypObject *s;
    if( len < 0 ) len = 0;
    s = ypMem_MALLOC_CONTAINER_VARIABLE( ypStrObject, ypChrArray_CODE, len+1, 0 );
    if( yp_isexceptionC( s ) ) return s;
    ypStr_DATA( s )[len] = '\0';
    ypStr_LEN( s ) = len;
    return s;
}


static ypObject *str_bool( ypObject *s ) {
    return ypBool_FROM_C( ypStr_LEN( s ) );
}

// Returns new reference or an exception
static ypObject *str_getindex( ypObject *s, yp_ssize_t i )
{
    ypObject *result = ypSequence_AdjustIndexC( ypStr_LEN( s ), &i );
    if( yp_isexceptionC( result ) ) return result;
    return yp_chrC( ypStr_DATA( s )[i] );
}

// Returns yp_None or an exception
static ypObject *str_len( ypObject *s, yp_ssize_t *len )
{
    *len = ypStr_LEN( s );
    return yp_None;
}

// Returns -1, 0, or 1 as per memcmp
static int _ypStr_relative_cmp( ypObject *s, ypObject *x ) {
    yp_ssize_t b_len = ypStr_LEN( s );
    yp_ssize_t x_len = ypStr_LEN( x );
    int cmp = memcmp( ypStr_DATA( s ), ypStr_DATA( x ), MIN( b_len, x_len ) );
    if( cmp == 0 ) cmp = b_len < x_len ? -1 : (b_len > x_len ? 1 : 0);
    return cmp;
}
static ypObject *str_lt( ypObject *s, ypObject *x ) {
    if( s == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypStr_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypStr_relative_cmp( s, x ) < 0 );
}
static ypObject *str_le( ypObject *s, ypObject *x ) {
    if( s == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypStr_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypStr_relative_cmp( s, x ) <= 0 );
}
static ypObject *str_ge( ypObject *s, ypObject *x ) {
    if( s == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypStr_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypStr_relative_cmp( s, x ) >= 0 );
}
static ypObject *str_gt( ypObject *s, ypObject *x ) {
    if( s == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypStr_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypStr_relative_cmp( s, x ) > 0 );
}

// Returns true (1) if the two str/chrarrays are equal.  Size is a quick way to check equality.
// TODO The pre-computed hash, if any, would also be a quick check
static int _ypStr_are_equal( ypObject *s, ypObject *x ) {
    yp_ssize_t b_len = ypStr_LEN( s );
    yp_ssize_t x_len = ypStr_LEN( x );
    if( b_len != x_len ) return 0;
    return memcmp( ypStr_DATA( s ), ypStr_DATA( x ), b_len ) == 0;
}
static ypObject *str_eq( ypObject *s, ypObject *x ) {
    if( s == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypStr_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( _ypStr_are_equal( s, x ) );
}
static ypObject *str_ne( ypObject *s, ypObject *x ) {
    if( s == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypStr_CODE ) return yp_ComparisonNotImplemented;
    return ypBool_FROM_C( !_ypStr_are_equal( s, x ) );
}

// Must work even for mutables; yp_hash handles caching this value and denying its use for mutables
// FIXME bring this in-line with Python's string hashing; it's currently using bytes hashing
static ypObject *str_currenthash( ypObject *s,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash ) {
    *hash = yp_HashBytes( ypStr_DATA( s ), ypStr_LEN( s ) );
    return yp_None;
}

static ypObject *str_dealloc( ypObject *s ) {
    ypMem_FREE_CONTAINER( s, ypStrObject );
    return yp_None;
}

static ypSequenceMethods ypStr_as_sequence = {
    str_getindex,                   // tp_getindex
    MethodError_objsliceproc,       // tp_getslice
    MethodError_findfunc,           // tp_find
    MethodError_countfunc,          // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_extend
    MethodError_objssizeproc,       // tp_irepeat
    MethodError_objssizeobjproc,    // tp_insert
    MethodError_objssizeproc,       // tp_popindex
    MethodError_objproc,            // tp_reverse
    MethodError_sortfunc            // tp_sort
};

static ypTypeObject ypStr_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    str_dealloc,                    // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    str_bool,                       // tp_bool
    str_lt,                         // tp_lt
    str_le,                         // tp_le
    str_eq,                         // tp_eq
    str_ne,                         // tp_ne
    str_ge,                         // tp_ge
    str_gt,                         // tp_gt

    // Generic object operations
    str_currenthash,                // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    _ypSequence_miniiter,           // tp_miniiter
    _ypSequence_miniiter_rev,       // tp_miniiter_reversed
    _ypSequence_miniiter_next,      // tp_miniiter_next
    _ypSequence_miniiter_lenh,      // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    _ypIter_from_miniiter_rev,      // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    &ypStr_as_sequence,             // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSequenceMethods ypChrArray_as_sequence = {
    str_getindex,                   // tp_getindex
    MethodError_objsliceproc,       // tp_getslice
    MethodError_findfunc,           // tp_find
    MethodError_countfunc,          // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_extend
    MethodError_objssizeproc,       // tp_irepeat
    MethodError_objssizeobjproc,    // tp_insert
    MethodError_objssizeproc,       // tp_popindex
    MethodError_objproc,            // tp_reverse
    MethodError_sortfunc            // tp_sort
};

static ypTypeObject ypChrArray_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    str_dealloc,                    // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    str_bool,                       // tp_bool
    str_lt,                         // tp_lt
    str_le,                         // tp_le
    str_eq,                         // tp_eq
    str_ne,                         // tp_ne
    str_ge,                         // tp_ge
    str_gt,                         // tp_gt

    // Generic object operations
    str_currenthash,                // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    _ypSequence_miniiter,           // tp_miniiter
    _ypSequence_miniiter_rev,       // tp_miniiter_reversed
    _ypSequence_miniiter_next,      // tp_miniiter_next
    _ypSequence_miniiter_lenh,      // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    _ypIter_from_miniiter_rev,      // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    &ypChrArray_as_sequence,        // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};


// Public constructors

// FIXME completely ignoring encoding/errors, and assuming source in latin-1
static ypObject *_ypStrC( ypObject *(*allocator)( yp_ssize_t ),
    const yp_uint8_t *source, yp_ssize_t len )
{
    ypObject *s;

    // Allocate an object of the appropriate size
    if( source == NULL ) {
        if( len < 0 ) len = 0;
    } else {
        if( len < 0 ) len = strlen( (const char *) source );
    }
    s = allocator( len );

    // Initialize the data
    if( source == NULL ) {
        memset( ypStr_DATA( s ), 0, len );
    } else {
        memcpy( ypStr_DATA( s ), source, len );
    }
    return s;
}
ypObject *yp_str_frombytesC( const yp_uint8_t *source, yp_ssize_t len,
        ypObject *encoding, ypObject *errors ) {
    return _ypStrC( _yp_str_new, source, len );
}
ypObject *yp_chrarray_frombytesC( const yp_uint8_t *source, yp_ssize_t len,
        ypObject *encoding, ypObject *errors ) {
    return _ypStrC( _yp_chrarray_new, source, len );
}

ypObject *yp_chrC( yp_int_t i ) {
    yp_uint8_t source[1];

    if( i < 0x00 || i > 0xFF ) return yp_SystemLimitationError;
    source[0] = (yp_uint8_t) i;
    return _ypStrC( _yp_str_new, source, 1 );
}

// Immortal constants

yp_IMMORTAL_STR_LATIN1( yp_s_ascii,     "ascii" );
yp_IMMORTAL_STR_LATIN1( yp_s_latin_1,   "latin_1" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_32,    "utf_32" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_32_be, "utf_32_be" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_32_le, "utf_32_le" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_16,    "utf_16" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_16_be, "utf_16_be" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_16_le, "utf_16_le" );
yp_IMMORTAL_STR_LATIN1( yp_s_utf_8,     "utf_8" );

yp_IMMORTAL_STR_LATIN1( yp_s_strict,    "strict" );
yp_IMMORTAL_STR_LATIN1( yp_s_ignore,    "ignore" );
yp_IMMORTAL_STR_LATIN1( yp_s_replace,   "replace" );


/*************************************************************************************************
 * Sequence of generic items
 *************************************************************************************************/

// TODO Eventually, use timsort, but for now C's qsort should be fine

typedef struct {
    ypObject_HEAD
    yp_INLINE_DATA( ypObject * );
} ypTupleObject;
#define ypTuple_ARRAY( sq ) ( (ypObject **) ((ypObject *)sq)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypTuple_LEN( sq ) ( ((ypObject *)sq)->ob_len )
#define ypTuple_ALLOCLEN( sq ) ( ((ypObject *)sq)->ob_alloclen )

// Empty tuples can be represented by this, immortal object
// TODO Can we use this in more places...anywhere we'd return a possibly-empty tuple?
static ypObject *_yp_tuple_empty_data[1] = {NULL};
static ypTupleObject _yp_tuple_empty_struct = {
    { ypObject_MAKE_TYPE_REFCNT( ypTuple_CODE, ypObject_REFCNT_IMMORTAL ),
    0, 0, ypObject_HASH_INVALID, _yp_tuple_empty_data } };
static ypObject * const _yp_tuple_empty = (ypObject *) &_yp_tuple_empty_struct;

// Moves the elements from [src:] to the index dest; this can be used when deleting items (they
// must be discarded first), or inserting (the new space is uninitialized).  Assumes enough space
// is allocated for the move.  Recall that memmove handles overlap.
#define ypTuple_ELEMMOVE( sq, dest, src ) \
    memmove( ypTuple_ARRAY( sq )+(dest), ypTuple_ARRAY( sq )+(src), \
            (ypTuple_LEN( sq )-(src)) * sizeof( ypObject * ) );

// FIXME in general, we need a way to determine when we can use the _INLINE variant
// Returns a new tuple of len zero, but allocated for alloclen elements
// XXX An alloclen of zero may mean lenhint is unreliable; we may still grow the tuple, so don't
// return _yp_tuple_empty!
static ypObject *_yp_tuple_new( yp_ssize_t alloclen ) {
    return ypMem_MALLOC_CONTAINER_VARIABLE( ypTupleObject, ypTuple_CODE, alloclen, 0 );
}

// Returns a new list of len zero, but allocated for alloclen elements
static ypObject *_yp_list_new( yp_ssize_t alloclen ) {
    return ypMem_MALLOC_CONTAINER_VARIABLE( ypTupleObject, ypList_CODE, alloclen, 0 );
}

// Shrinks or grows the tuple; if shrinking, ensure excess elements are discarded.  Returns
// yp_None on success, exception on error.
// TODO over-allocate as appropriate
static ypObject *_ypTuple_resize( ypObject *sq, yp_ssize_t alloclen )
{
    return ypMem_REALLOC_CONTAINER_VARIABLE( sq, ypTupleObject, alloclen, 0 );
}

// growhint is the number of additional items, not including x, that are expected to be added to
// the tuple
static ypObject *_ypTuple_push( ypObject *sq, ypObject *x, yp_ssize_t growhint )
{
    ypObject *result;
    if( ypTuple_ALLOCLEN( sq ) - ypTuple_LEN( sq ) < 1 ) {
        if( growhint < 0 ) growhint = 0;
        result = _ypTuple_resize( sq, ypTuple_LEN( sq ) + growhint+1 );
        if( yp_isexceptionC( result ) ) return result;
    }
    ypTuple_ARRAY( sq )[ypTuple_LEN( sq )] = yp_incref( x );
    ypTuple_LEN( sq ) += 1;
    return yp_None;
}

static ypObject *_ypTuple_extend_from_iter( ypObject *sq, ypObject *mi, yp_uint64_t *mi_state )
{
    ypObject *exc = yp_None;
    ypObject *x;
    ypObject *result;
    yp_ssize_t lenhint = yp_miniiter_lenhintC( mi, mi_state, &exc ); // zero on error

    while( 1 ) {
        x = yp_miniiter_next( mi, mi_state ); // new ref
        if( yp_isexceptionC( x ) ) {
            if( yp_isexceptionC2( x, yp_StopIteration ) ) break;
            return x;
        }
        lenhint -= 1; // check for <0 only when we need it in _ypTuple_push
        result = _ypTuple_push( sq, x, lenhint );
        yp_decref( x );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *_ypTuple_extend( ypObject *sq, ypObject *iterable )
{
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *result;

    // TODO Will special cases for other lists/tuples save anything?
    mi = yp_miniiter( iterable, &mi_state ); // new ref
    if( yp_isexceptionC( mi ) ) return mi;
    result = _ypTuple_extend_from_iter( sq, mi, &mi_state );
    yp_decref( mi );
    return result;
}

// Public Methods

static ypObject *tuple_getindex( ypObject *sq, yp_ssize_t i )
{
    ypObject *result = ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i );
    if( yp_isexceptionC( result ) ) return result;
    return yp_incref( ypTuple_ARRAY( sq )[i] );
}

static ypObject *tuple_getslice( ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step )
{
    return yp_NotImplementedError;
}

static ypObject *tuple_find( ypObject *sq, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *i )
{
    return yp_NotImplementedError;
}

static ypObject *tuple_count( ypObject *sq, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *n )
{
    return yp_NotImplementedError;
}

static ypObject *list_setindex( ypObject *sq, yp_ssize_t i, ypObject *x )
{
    ypObject *result;
    if( yp_isexceptionC( x ) ) return x;
    result = ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i );
    if( yp_isexceptionC( result ) ) return result;
    yp_decref( ypTuple_ARRAY( sq )[i] );
    ypTuple_ARRAY( sq )[i] = yp_incref( x );
    return yp_None;
}

static ypObject *list_setslice( ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step,
        ypObject *x )
{
    if( yp_isexceptionC( x ) ) return x;
    return yp_NotImplementedError;
}

static ypObject *list_popindex( ypObject *sq, yp_ssize_t i )
{
    ypObject *result = ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i );
    if( yp_isexceptionC( result ) ) return result;

    result = ypTuple_ARRAY( sq )[i];
    ypTuple_ELEMMOVE( sq, i, i+1 );
    ypTuple_LEN( sq ) -= 1;
    return result;
}

static ypObject *list_delindex( ypObject *sq, yp_ssize_t i )
{
    ypObject *result = list_popindex( sq, i );
    if( yp_isexceptionC( result ) ) return result;
    yp_decref( result );
    return yp_None;
}

static ypObject *list_delslice( ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step )
{
    return yp_NotImplementedError;
}

// TODO getitem et al

#define list_extend _ypTuple_extend

static ypObject *list_irepeat( ypObject *sq, yp_ssize_t factor )
{
    return yp_NotImplementedError;
}

static ypObject *list_insert( ypObject *sq, yp_ssize_t i, ypObject *x )
{
    ypObject *result;
    if( yp_isexceptionC( x ) ) return x;
    result = ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i );
    if( yp_isexceptionC( result ) ) return result;

    // Resize if necessary
    // FIXME The resize might have to copy data, _then_ we'll also do the ypTuple_ELEMMOVE, copying
    // large amounts of data twice; optimize (and...are there other areas of the code where this
    // happens?)
    if( ypTuple_ALLOCLEN( sq ) - ypTuple_LEN( sq ) < 1 ) {
        // TODO over-allocate?
        result = _ypTuple_resize( sq, ypTuple_LEN( sq ) + 1 );
        if( yp_isexceptionC( result ) ) return result;
    }

    // Make room at i and add x
    ypTuple_ELEMMOVE( sq, i+1, i );
    ypTuple_ARRAY( sq )[i] = yp_incref( x );
    ypTuple_LEN( sq ) += 1;
    return yp_None;
}

// list_popindex is above

static ypObject *tuple_traverse( ypObject *sq, visitfunc visitor, void *memo )
{
    yp_ssize_t i;
    ypObject *result;
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        result = visitor( ypTuple_ARRAY( sq )[i], memo );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *tuple_bool( ypObject *sq ) {
    return ypBool_FROM_C( ypTuple_LEN( sq ) );
}


// Returns yp_True if the two tuples/lists are equal.  Size is a quick way to check equality.
// TODO The pre-computed hash, if any, would also be a quick check
// FIXME comparison functions can recurse, just like currenthash...fix!
static ypObject *tuple_eq( ypObject *sq, ypObject *x )
{
    yp_ssize_t sq_len = ypBytes_LEN( sq );
    yp_ssize_t x_len  = ypBytes_LEN( x );
    yp_ssize_t i;
    ypObject *result;

    if( sq == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypTuple_CODE ) return yp_ComparisonNotImplemented;
    if( sq_len != x_len ) return yp_False;
    for( i = 0; i < sq_len; i++ ) {
        result = yp_eq( ypTuple_ARRAY( sq )[i], ypTuple_ARRAY( x )[i] );
        if( result != yp_True ) return result; // returns on yp_False or an exception
    }
    return yp_True;
}
static ypObject *tuple_ne( ypObject *sq, ypObject *x ) {
    ypObject *result = tuple_eq( sq, x );
    return ypBool_NOT( result );
}

// XXX Adapted from Python's tuplehash
// TODO Do we want to allow currenthash to work on circular references and, if so, how?
static ypObject *tuple_currenthash( ypObject *sq,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash )
{
    ypObject *result;
    yp_uhash_t x;
    yp_hash_t y;
    yp_ssize_t len = ypTuple_LEN(sq);
    ypObject **p;
    yp_uhash_t mult = _ypHASH_MULTIPLIER;
    x = 0x345678;
    p = ypTuple_ARRAY(sq);
    while (--len >= 0) {
        result = hash_visitor(*p++, hash_memo, &y);
        if (yp_isexceptionC(result)) return result;
        x = (x ^ y) * mult;
        /* the cast might truncate len; that doesn't change hash stability */
        mult += (yp_hash_t)(82520L + len + len);
    }
    x += 97531L;
    if (x == (yp_uhash_t)ypObject_HASH_INVALID) {
        x = (yp_uhash_t)(ypObject_HASH_INVALID - 1);
    }
    *hash = (yp_hash_t)x;
    return yp_None;
}

static ypObject *tuple_contains( ypObject *sq, ypObject *x )
{
    yp_ssize_t i;
    ypObject *result;
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        result = yp_eq( x, ypTuple_ARRAY( sq )[i] );
        if( result != yp_False ) return result; // yp_True, or an exception
    }
    return yp_False;
}

static ypObject *tuple_len( ypObject *sq, yp_ssize_t *len ) {
    *len = ypTuple_LEN( sq );
    return yp_None;
}

static ypObject *list_push( ypObject *sq, ypObject *x ) {
    return _ypTuple_push( sq, x, 0 );
}

static ypObject *list_clear( ypObject *sq )
{
    while( ypTuple_LEN( sq ) > 0 ) {
        yp_decref( ypTuple_ARRAY( sq )[ypTuple_LEN( sq )-1] );
        ypTuple_LEN( sq ) -= 1;
    }
    return yp_None;
}

static ypObject *list_pop( ypObject *sq )
{
    if( ypTuple_LEN( sq ) < 1 ) return yp_IndexError;
    ypTuple_LEN( sq ) -= 1;
    return ypTuple_ARRAY( sq )[ypTuple_LEN( sq )];
}

static ypObject *tuple_dealloc( ypObject *sq )
{
    int i;
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        yp_decref( ypTuple_ARRAY( sq )[i] );
    }
    ypMem_FREE_CONTAINER( sq, ypTupleObject );
    return yp_None;
}

#define list_extend _ypTuple_extend

static ypSequenceMethods ypTuple_as_sequence = {
    tuple_getindex,                 // tp_getindex
    tuple_getslice,                 // tp_getslice
    tuple_find,                     // tp_find
    tuple_count,                    // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_extend
    MethodError_objssizeproc,       // tp_irepeat
    MethodError_objssizeobjproc,    // tp_insert
    MethodError_objssizeproc,       // tp_popindex
    MethodError_objproc,            // tp_reverse
    MethodError_sortfunc            // tp_sort
};

static ypTypeObject ypTuple_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    tuple_dealloc,                  // tp_dealloc
    tuple_traverse,                 // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    tuple_bool,                     // tp_bool
    MethodError_objobjproc,         // tp_lt
    MethodError_objobjproc,         // tp_le
    tuple_eq,                       // tp_eq
    tuple_ne,                       // tp_ne
    MethodError_objobjproc,         // tp_ge
    MethodError_objobjproc,         // tp_gt

    // Generic object operations
    tuple_currenthash,              // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    _ypSequence_miniiter,           // tp_miniiter
    _ypSequence_miniiter_rev,       // tp_miniiter_reversed
    _ypSequence_miniiter_next,      // tp_miniiter_next
    _ypSequence_miniiter_lenh,      // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    _ypIter_from_miniiter_rev,      // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    tuple_contains,                 // tp_contains
    tuple_len,                      // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    &ypTuple_as_sequence,           // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSequenceMethods ypList_as_sequence = {
    tuple_getindex,                 // tp_getindex
    tuple_getslice,                 // tp_getslice
    tuple_find,                     // tp_find
    tuple_count,                    // tp_count
    list_setindex,                  // tp_setindex
    list_setslice,                  // tp_setslice
    list_delindex,                  // tp_delindex
    list_delslice,                  // tp_delslice
    list_extend,                    // tp_extend
    list_irepeat,                   // tp_irepeat
    list_insert,                    // tp_insert
    list_popindex,                  // tp_popindex
    MethodError_objproc,            // tp_reverse
    MethodError_sortfunc            // tp_sort
};

static ypTypeObject ypList_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    tuple_dealloc,                  // tp_dealloc
    tuple_traverse,                 // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    tuple_bool,                     // tp_bool
    MethodError_objobjproc,         // tp_lt
    MethodError_objobjproc,         // tp_le
    tuple_eq,                       // tp_eq
    tuple_ne,                       // tp_ne
    MethodError_objobjproc,         // tp_ge
    MethodError_objobjproc,         // tp_gt

    // Generic object operations
    tuple_currenthash,              // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    _ypSequence_miniiter,           // tp_miniiter
    _ypSequence_miniiter_rev,       // tp_miniiter_reversed
    _ypSequence_miniiter_next,      // tp_miniiter_next
    _ypSequence_miniiter_lenh,      // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    _ypIter_from_miniiter_rev,      // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    tuple_contains,                 // tp_contains
    tuple_len,                      // tp_len
    list_push,                      // tp_push
    list_clear,                     // tp_clear
    list_pop,                       // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    &ypList_as_sequence,            // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

// Constructors

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypTuple( ypObject *(*allocator)( yp_ssize_t ), ypObject *iterable )
{
    ypObject *exc = yp_None;
    ypObject *newSq;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( iterable, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( iterable, &exc );
    } else if( lenhint == 0 && allocator == _yp_tuple_new ) {
        // yp_lenC reports an empty iterable, so we can shortcut frozenset creation
        return _yp_tuple_empty;
    }

    newSq = allocator( lenhint );
    if( yp_isexceptionC( newSq ) ) return newSq;
    result = _ypTuple_extend( newSq, iterable );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSq );
        return result;
    }
    return newSq;
}

ypObject *yp_tupleN( int n, ... ) {
    if( n < 1 ) return _yp_tuple_empty;
    return_yp_V_FUNC( ypObject *, yp_tupleV, (n, args), n );
}
ypObject *yp_tupleV( int n, va_list args ) {
    // TODO Return _yp_tuple_empty on n<1??
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypTuple( _yp_tuple_new, iter_args );
}
ypObject *yp_tuple( ypObject *iterable ) {
    if( ypObject_TYPE_CODE( iterable ) == ypTuple_CODE ) return yp_incref( iterable );
    return _ypTuple( _yp_tuple_new, iterable );
}

ypObject *yp_listN( int n, ... ) {
    if( n == 0 ) return _yp_list_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_listV, (n, args), n );
}
ypObject *yp_listV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypTuple( _yp_list_new, iter_args );
}
ypObject *yp_list( ypObject *iterable ) {
    return _ypTuple( _yp_list_new, iterable );

}


/*************************************************************************************************
 * Sets
 *************************************************************************************************/

// XXX Much of this set/dict implementation is pulled right from Python, so best to read the
// original source for documentation on this implementation

// TODO Many set operations allocate temporary objects on the heap; is there a way to avoid this?

typedef struct {
    yp_hash_t se_hash;
    ypObject *se_key;
} ypSet_KeyEntry;
typedef struct {
    ypObject_HEAD
    yp_ssize_t fill; // # Active + # Dummy
    yp_INLINE_DATA( ypSet_KeyEntry );
} ypSetObject;

#define ypSet_TABLE( so )               ( (ypSet_KeyEntry *) ((ypObject *)so)->ob_data )
#define ypSet_SET_TABLE( so, value )    ( ((ypObject *)so)->ob_data = (void *) (value) )
// TODO what if ob_len is the "invalid" value?
#define ypSet_LEN( so )                 ( ((ypObject *)so)->ob_len )
#define ypSet_FILL( so )                ( ((ypSetObject *)so)->fill )
#define ypSet_ALLOCLEN( so )            ( ((ypObject *)so)->ob_alloclen )
#define ypSet_MASK( so )                ( ypSet_ALLOCLEN( so ) - 1 )
#define ypSet_INLINE_DATA( so )         ( ((ypSetObject *)so)->ob_inline_data )

#define ypSet_PERTURB_SHIFT (5)

// This tests that, by default, the inline data is enough to hold ypSet_MINSIZE elements
#define ypSet_MINSIZE (8)
yp_STATIC_ASSERT( (_ypMem_ideal_size_DEFAULT-offsetof( ypSetObject, ob_inline_data )) / sizeof( ypSet_KeyEntry ) >= ypSet_MINSIZE, ypSet_minsize_inline );

static ypObject _ypSet_dummy = yp_IMMORTAL_HEAD_INIT( ypInvalidated_CODE, NULL, 0 );
static ypObject *ypSet_dummy = &_ypSet_dummy;

// Empty frozensets can be represented by this, immortal object
// TODO Can we use this in more places...anywhere we'd return a possibly-empty frozenset?
static ypSet_KeyEntry _yp_frozenset_empty_data[ypSet_MINSIZE] = {0};
static ypSetObject _yp_frozenset_empty_struct = {
    { ypObject_MAKE_TYPE_REFCNT( ypFrozenSet_CODE, ypObject_REFCNT_IMMORTAL ),
    0, ypSet_MINSIZE, ypObject_HASH_INVALID, _yp_frozenset_empty_data }, 0 };
static ypObject * const _yp_frozenset_empty = (ypObject *) &_yp_frozenset_empty_struct;

// Returns true if the given ypSet_KeyEntry contains a valid key
#define ypSet_ENTRY_USED( loc ) \
    ( (loc)->se_key != NULL && (loc)->se_key != ypSet_dummy )
// Returns the index of the given ypSet_KeyEntry in the hash table
#define ypSet_ENTRY_INDEX( so, loc ) \
    ( (yp_ssize_t) ( (loc) - ypSet_TABLE( so ) ) )

// set code relies on some of the internals from the dict implementation
typedef struct {
    ypObject_HEAD
    ypObject *keyset;
    yp_INLINE_DATA( ypObject * );
} ypDictObject;
#define ypDict_LEN( mp )     ( ((ypObject *)mp)->ob_len )

// Before adding keys to the set, call this function to determine if a resize is necessary.
// Returns 0 if the set should first be resized, otherwise returns the number of keys that can be
// added before the next resize.
// TODO ensure we aren't unnecessarily resizing: if the old and new alloclens will be the same,
// and we don't have dummy entries, then resizing is a waste of effort.
// XXX Adapted from PyDict_SetItem, although our thresholds are slightly different
static yp_ssize_t _ypSet_space_remaining( ypObject *so )
{
    /* If fill >= 2/3 size, adjust size.  Normally, this doubles or
     * quaduples the size, but it's also possible for the dict to shrink
     * (if ma_fill is much larger than se_used, meaning a lot of dict
     * keys have been deleted).
     */
    yp_ssize_t retval = (ypSet_ALLOCLEN( so )*2) / 3;
    retval -= ypSet_FILL( so );
    if( retval <= 0 ) return 0; // should resize before adding keys
    return retval;
}

// Returns the alloclen that will fit minused entries, or <1 on error
// XXX Adapted from Python's dictresize; keep in-sync with _ypSet_space_remaining
// TODO Need to carefully review how expected len becomes minused becomes alloclen; need to also
// review that pre-allocating 6, say, will mean no resizes if 6 are added
static yp_ssize_t _ypSet_calc_alloclen( yp_ssize_t minused )
{
    yp_ssize_t minentries = ((minused * 3) / 2) + 1; // recall we fill to 2/3 size
    yp_ssize_t alloclen;
    for( alloclen = ypSet_MINSIZE;
         alloclen <= minentries && alloclen > 0;
         alloclen <<= 1 );
    return alloclen;
}

// If a resize is necessary and you suspect future growth may occur, call this function to
// determine the minused value to pass to _ypSet_resize.
// TODO Make this configurable via yp_initialize
// XXX Adapted from PyDict_SetItem
static yp_ssize_t _ypSet_calc_resize_minused( yp_ssize_t newlen )
{
    /* Quadrupling the size improves average dictionary sparseness
     * (reducing collisions) at the cost of some memory and iteration
     * speed (which loops over every possible entry).  It also halves
     * the number of expensive resize operations in a growing dictionary.
     *
     * Very large dictionaries (over 50K items) use doubling instead.
     * This may help applications with severe memory constraints.
     */
    return (newlen > 50000 ? 2 : 4) * newlen;
}

// Returns a new, empty frozenset object to hold minused entries
// TODO can use CONTAINER_INLINE if minused is a firm max length for the frozenset
// XXX A minused of zero may mean lenhint is unreliable; we may still grow the frozenset, so don't
// return _yp_frozenset_empty!
static ypObject *_yp_frozenset_new( yp_ssize_t minused )
{
    yp_ssize_t alloclen = _ypSet_calc_alloclen( minused );
    ypObject *so;
    if( alloclen < 1 ) return yp_MemoryError;
    so = ypMem_MALLOC_CONTAINER_VARIABLE( ypSetObject, ypFrozenSet_CODE, alloclen, 0 );
    if( yp_isexceptionC( so ) ) return so;
    ypSet_ALLOCLEN( so ) = alloclen; // we can't make use of the excess anyway
    ypSet_FILL( so ) = 0;
    memset( ypSet_TABLE( so ), 0, alloclen * sizeof( ypSet_KeyEntry ) );
    return so;
}

// Returns a new, empty set object to hold minused entries
static ypObject *_yp_set_new( yp_ssize_t minused )
{
    yp_ssize_t alloclen = _ypSet_calc_alloclen( minused );
    ypObject *so;
    if( alloclen < 1 ) return yp_MemoryError;
    so = ypMem_MALLOC_CONTAINER_VARIABLE( ypSetObject, ypSet_CODE, alloclen, 0 );
    if( yp_isexceptionC( so ) ) return so;
    ypSet_ALLOCLEN( so ) = alloclen; // we can't make use of the excess anyway
    ypSet_FILL( so ) = 0;
    memset( ypSet_TABLE( so ), 0, alloclen * sizeof( ypSet_KeyEntry ) );
    return so;
}

// Sets *loc to where the key should go in the table; it may already be there, in fact!  Returns
// yp_None, or an exception on error.
// TODO The dict implementation has a bunch of these for various scenarios; let's keep it simple
// for now, but investigate...
// XXX Adapted from Python's lookdict in dictobject.c
static ypObject *_ypSet_lookkey( ypObject *so, ypObject *key, register yp_hash_t hash,
        ypSet_KeyEntry **loc )
{
    register size_t i;
    register size_t perturb;
    register ypSet_KeyEntry *freeslot;
    register size_t mask = (size_t) ypSet_MASK( so );
    ypSet_KeyEntry *table = ypSet_TABLE( so );
    ypSet_KeyEntry *ep0 = ypSet_TABLE( so );
    register ypSet_KeyEntry *ep;
    register ypObject *cmp;

    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->se_key == NULL || ep->se_key == key) goto success;

    if (ep->se_key == ypSet_dummy) {
        freeslot = ep;
    } else {
        if (ep->se_hash == hash) {
            // Python has protection here against __eq__ changing this set object; hopefully not a
            // problem in nohtyP
            cmp = yp_eq( ep->se_key, key );
            if( yp_isexceptionC( cmp ) ) return cmp;
            if( cmp == yp_True ) goto success;
        }
        freeslot = NULL;
    }

    // In the loop, se_key == ypSet_dummy is by far (factor of 100s) the least likely
    // outcome, so test for that last
    for (perturb = hash; ; perturb >>= ypSet_PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->se_key == NULL) {
            if( freeslot != NULL ) ep = freeslot;
            goto success;
        }
        if (ep->se_key == key) goto success;
        if (ep->se_hash == hash && ep->se_key != ypSet_dummy) {
            // Same __eq__ protection is here as well in Python
            cmp = yp_eq( ep->se_key, key );
            if( yp_isexceptionC( cmp ) ) return cmp;
            if( cmp == yp_True ) goto success;
        } else if (ep->se_key == ypSet_dummy && freeslot == NULL) {
            freeslot = ep;
        }
    }
    return yp_SystemError; // NOT REACHED

// When the code jumps here, it means ep points to the proper entry
success:
    *loc = ep;
    return yp_None;
}

// Steals key and adds it to the hash table at the given location.  loc must not currently be in
// use! Ensure the set is large enough (_ypSet_space_remaining) before adding items.
// XXX Adapted from Python's insertdict in dictobject.c
static void _ypSet_movekey( ypObject *so, ypSet_KeyEntry *loc, ypObject *key,
        yp_hash_t hash )
{
    if( loc->se_key == NULL ) ypSet_FILL( so ) += 1;
    loc->se_key = key;
    loc->se_hash = hash;
    ypSet_LEN( so ) += 1;
}

// Steals key and adds it to the *clean* hash table.  Only use if the key is known to be absent
// from the table, and the table contains no deleted entries; this is usually known when
// cleaning/resizing/copying a table.  Sets *loc to the location at which the key was inserted.
// Ensure the set is large enough (_ypSet_space_remaining) before adding items.
// XXX Adapted from Python's insertdict_clean in dictobject.c
static void _ypSet_movekey_clean( ypObject *so, ypObject *key, yp_hash_t hash,
        ypSet_KeyEntry **ep )
{
    size_t i;
    size_t perturb;
    size_t mask = (size_t) ypSet_MASK( so );
    ypSet_KeyEntry *ep0 = ypSet_TABLE( so );

    i = hash & mask;
    (*ep) = &ep0[i];
    for (perturb = hash; (*ep)->se_key != NULL; perturb >>= ypSet_PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        (*ep) = &ep0[i & mask];
    }
    ypSet_FILL( so ) += 1;
    (*ep)->se_key = key;
    (*ep)->se_hash = hash;
    ypSet_LEN( so ) += 1;
}

// Removes the key at the given location from the hash table and returns the reference to it (the
// key's reference count is not modified).
static ypObject *_ypSet_removekey( ypObject *so, ypSet_KeyEntry *loc )
{
    ypObject *oldkey = loc->se_key;
    loc->se_key = ypSet_dummy;
    ypSet_LEN( so ) -= 1;
    return oldkey;
}

// Resizes the set to the smallest size that will hold minused values.  If you want to reduce the
// need for future resizes, call with a larger minused; _ypSet_calc_resize_minused is suggested for
// this purpose.  Returns yp_None, or an exception on error.
// TODO ensure we aren't unnecessarily resizing: if the old and new alloclens will be the same,
// and we don't have dummy entries, then resizing is a waste of effort.
static ypObject *_ypSet_resize( ypObject *so, yp_ssize_t minused )
{
    yp_ssize_t newalloclen;
    ypSet_KeyEntry *newkeys;
    yp_ssize_t newsize;
    ypSet_KeyEntry *oldkeys;
    yp_ssize_t keysleft;
    yp_ssize_t i;
    ypSet_KeyEntry *loc;

    // TODO allocate the table in-line, then handle the case where both old and new tables
    // could fit in-line (idea: if currently in-line, then just force that the new array be
    // malloc'd...will need to malloc something anyway)
    newalloclen = _ypSet_calc_alloclen( minused );
    if( newalloclen < 1 ) return yp_MemoryError;
    newkeys = (ypSet_KeyEntry *) yp_malloc( &newsize, newalloclen * sizeof( ypSet_KeyEntry ) );
    if( newkeys == NULL ) return yp_MemoryError;
    memset( newkeys, 0, newalloclen * sizeof( ypSet_KeyEntry ) );

    // Failures are impossible from here on, so swap-in the new table
    oldkeys = ypSet_TABLE( so );
    keysleft = ypSet_LEN( so );
    ypSet_SET_TABLE( so, newkeys );
    ypSet_LEN( so ) = 0;
    ypSet_FILL( so ) = 0;
    ypSet_ALLOCLEN( so ) = newalloclen;

    // Move the keys from the old table before free'ing it
    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &oldkeys[i] ) ) continue;
        keysleft -= 1;
        _ypSet_movekey_clean( so, oldkeys[i].se_key, oldkeys[i].se_hash, &loc );
    }
    if( oldkeys != ypSet_INLINE_DATA( so ) ) yp_free( oldkeys );
    DEBUG( "_ypSet_resize: 0x%08X table 0x%08X  (was 0x%08X)", so, newkeys, oldkeys );
    return yp_None;
}

// Adds the key to the hash table.  *spaceleft should be initialized from  _ypSet_space_remaining;
// this function then decrements it with each key added, and resets it on every resize.  Returns
// yp_True if so was modified, yp_False if it wasn't due to the key already being in the set, or an
// exception on error.
// XXX Adapted from PyDict_SetItem
static ypObject *_ypSet_push( ypObject *so, ypObject *key, yp_ssize_t *spaceleft )
{
    yp_hash_t hash;
    ypObject *result = yp_None;
    ypSet_KeyEntry *loc;
    yp_ssize_t newlen;

    // Look for the appropriate entry in the hash table
    hash = yp_hashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( so, key, hash, &loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the key is already in the hash table, then there's nothing to do
    if( ypSet_ENTRY_USED( loc ) ) return yp_False;

    // Otherwise, we need to add the key, which possibly doesn't involve resizing
    if( *spaceleft >= 1 ) {
        _ypSet_movekey( so, loc, yp_incref( key ), hash );
        *spaceleft -= 1;
        return yp_True;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean.  Give mutable objects a bit of room to grow.
    newlen = ypSet_LEN( so )+1;
    if( ypObject_IS_MUTABLE( so ) ) newlen = _ypSet_calc_resize_minused( newlen );
    result = _ypSet_resize( so, newlen );   // invalidates loc
    if( yp_isexceptionC( result ) ) return result;
    _ypSet_movekey_clean( so, yp_incref( key ), hash, &loc );
    *spaceleft = _ypSet_space_remaining( so );
    return yp_True;
}

// Removes the key from the hash table.  The set is not resized.  Returns the reference to the
// removed key if so was modified, ypSet_dummy if it wasn't due to the key not being in the
// set, or an exception on error.
static ypObject *_ypSet_pop( ypObject *so, ypObject *key )
{
    yp_hash_t hash;
    ypObject *result = yp_None;
    ypSet_KeyEntry *loc;

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( so, key, hash, &loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the key is not in the hash table, then there's nothing to do
    if( !ypSet_ENTRY_USED( loc ) ) return ypSet_dummy;

    // Otherwise, we need to remove the key
    return _ypSet_removekey( so, loc ); // new ref
}

// XXX Check for the so==x case _before_ calling this function
// TODO This requires that the elements of x are immutable...do we want to support mutables too?
static ypObject *_ypSet_isdisjoint( ypObject *so, ypObject *x )
{
    ypSet_KeyEntry *keys = ypSet_TABLE( so );
    yp_ssize_t keysleft = ypSet_LEN( so );
    yp_ssize_t i;
    ypObject *result;
    ypSet_KeyEntry *loc;

    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &keys[i] ) ) continue;
        keysleft -= 1;
        result = _ypSet_lookkey( x, keys[i].se_key, keys[i].se_hash, &loc );
        if( yp_isexceptionC( result ) ) return result;
        if( ypSet_ENTRY_USED( loc ) ) return yp_False;
    }
    return yp_True;
}

// XXX Check for the so==x case _before_ calling this function
// TODO This requires that the elements of x are immutable...do we want to support mutables too?
static ypObject *_ypSet_issubset( ypObject *so, ypObject *x )
{
    ypSet_KeyEntry *keys = ypSet_TABLE( so );
    yp_ssize_t keysleft = ypSet_LEN( so );
    yp_ssize_t i;
    ypObject *result;
    ypSet_KeyEntry *loc;

    if( ypSet_LEN( so ) > ypSet_LEN( x ) ) return yp_False;
    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &keys[i] ) ) continue;
        keysleft -= 1;
        result = _ypSet_lookkey( x, keys[i].se_key, keys[i].se_hash, &loc );
        if( yp_isexceptionC( result ) ) return result;
        if( !ypSet_ENTRY_USED( loc ) ) return yp_False;
    }
    return yp_True;
}

// XXX Check for the so==other case _before_ calling this function
// XXX We're trusting that copy_visitor will behave properly and return an object that has the same
// hash as the original and that is unequal to anything else in the other set
static ypObject *_ypSet_update_from_set( ypObject *so, ypObject *other,
        visitfunc copy_visitor, void *copy_memo )
{
    // TODO resize if necessary; if starting from clean, use _ypSet_movekey_clean, otherwise
    // _ypSet_movekey
    yp_ssize_t keysleft = ypSet_LEN( other );
    ypSet_KeyEntry *otherkeys = ypSet_TABLE( other );
    ypObject *result;
    yp_ssize_t i;
    ypObject *key;
    ypSet_KeyEntry *loc;

    // Resize the set if necessary
    // FIXME instead, wait until we need a resize, then since we're resizing anyway, resize to fit
    // the given lenhint
    if( _ypSet_space_remaining( so ) < keysleft ) {
        result = _ypSet_resize( so, ypSet_LEN( so )+keysleft );
        if( yp_isexceptionC( result ) ) return result;
    }

    // If the set is empty and contains no deleted entries, then we also know that none of the keys
    // in other will be in the set, so we can use _ypSet_movekey_clean
    if( ypSet_LEN( so ) == 0 && ypSet_FILL( so ) == 0 ) {
        for( i = 0; keysleft > 0; i++ ) {
            if( !ypSet_ENTRY_USED( &otherkeys[i] ) ) continue;
            keysleft -= 1;
            key = copy_visitor( otherkeys[i].se_key, copy_memo );
            if( yp_isexceptionC( key ) ) return key;
            _ypSet_movekey_clean( so, key, otherkeys[i].se_hash, &loc );
        }
        return yp_None;
    }

    // Otherwise, we need to skip over duplicate keys and use _ypSet_movekey
    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &otherkeys[i] ) ) continue;
        keysleft -= 1;
        result = _ypSet_lookkey( so, otherkeys[i].se_key, otherkeys[i].se_hash, &loc );
        if( yp_isexceptionC( result ) ) return result;
        if( ypSet_ENTRY_USED( loc ) ) continue; // if the entry is used then key is already in set
        key = copy_visitor( otherkeys[i].se_key, copy_memo );
        if( yp_isexceptionC( key ) ) return key;
        _ypSet_movekey( so, loc, key, otherkeys[i].se_hash );
    }
    return yp_None;
}

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypSet_update_from_iter( ypObject *so, ypObject *mi, yp_uint64_t *mi_state )
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    ypObject *result = yp_None;
    yp_ssize_t lenhint;
    ypObject *key;

    // Use lenhint in the hopes of requiring only one resize
    // FIXME instead, wait until we need a resize, then since we're resizing anyway, resize to fit
    // the given lenhint
    lenhint = yp_miniiter_lenhintC( mi, mi_state, &result );
    if( yp_isexceptionC( result ) ) return result;
    if( spaceleft < lenhint ) {
        result = _ypSet_resize( so, ypSet_LEN( so )+lenhint );
        if( yp_isexceptionC( result ) ) return result;
        spaceleft = _ypSet_space_remaining( so );
    }

    // Add all the yielded keys to the set.  Keep in mind that _ypSet_push may resize the
    // set despite our attempts at pre-allocating, since lenhint _is_ just a hint.
    while( 1 ) {
        key = yp_miniiter_next( mi, mi_state ); // new ref
        if( yp_isexceptionC( key ) ) {
            if( yp_isexceptionC2( key, yp_StopIteration ) ) break;
            return key;
        }
        result = _ypSet_push( so, key, &spaceleft );
        yp_decref( key );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

// Adds the keys yielded from iterable to the set.  If the set has enough space to hold all the
// keys, the set is not resized (important, as yp_setN et al pre-allocate the necessary space).
// Requires that iterables's items are immutable; unavoidable as they are to be added to the set.
// XXX Check for the so==iterable case _before_ calling this function
static ypObject *_ypSet_update( ypObject *so, ypObject *iterable )
{
    // TODO determine when/where/how the set should be resized
    int iterable_pair = ypObject_TYPE_PAIR_CODE( iterable );
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *result;

    // Recall that type pairs are identified by the immutable type code
    if( iterable_pair == ypFrozenSet_CODE ) {
        return _ypSet_update_from_set( so, iterable, yp_shallowcopy_visitor, NULL );
    } else {
        mi = yp_miniiter( iterable, &mi_state ); // new ref
        if( yp_isexceptionC( mi ) ) return mi;
        result = _ypSet_update_from_iter( so, mi, &mi_state );
        yp_decref( mi );
        return result;
    }
}

// XXX Check the so==other case _before_ calling this function
static ypObject *_ypSet_intersection_update_from_set( ypObject *so, ypObject *other )
{
    yp_ssize_t keysleft = ypSet_LEN( so );
    ypSet_KeyEntry *keys = ypSet_TABLE( so );
    yp_ssize_t i;
    ypSet_KeyEntry *other_loc;
    ypObject *result;

    // Since we're only removing keys from so, it won't be resized, so we can loop over it.  We
    // break once so is empty because we aren't expecting any errors from _ypSet_lookkey.
    for( i = 0; keysleft > 0; i++ ) {
        if( ypSet_LEN( so ) < 1 ) break;
        if( !ypSet_ENTRY_USED( &keys[i] ) ) continue;
        keysleft -= 1;
        result = _ypSet_lookkey( other, keys[i].se_key, keys[i].se_hash, &other_loc );
        if( yp_isexceptionC( result ) ) return result;
        if( ypSet_ENTRY_USED( other_loc ) ) continue; // if entry used, key is in other
        yp_decref( _ypSet_removekey( so, &keys[i] ) );
    }
    return yp_None;
}

// FIXME This _allows_ mi to yield mutable values, unlike issubset; standardize
static ypObject *frozenset_unfrozen_copy( ypObject *so, visitfunc copy_visitor, void *copy_memo );
static ypObject *_ypSet_difference_update_from_iter( ypObject *so, ypObject *mi, yp_uint64_t *mi_state );
static ypObject *_ypSet_difference_update_from_set( ypObject *so, ypObject *other );
static ypObject *_ypSet_intersection_update_from_iter(
        ypObject *so, ypObject *mi, yp_uint64_t *mi_state )
{
    ypObject *so_toremove;
    ypObject *result;

    // FIXME can we do this without creating a copy or, alternatively, would it be better to
    // implement this as ypSet_intersection?
    // Unfortunately, we need to create a short-lived copy of so.  It's either that, or convert
    // mi to a set, or come up with a fancy scheme to "mark" items in so to be deleted.
    so_toremove = frozenset_unfrozen_copy( so, yp_shallowcopy_visitor, NULL ); // new ref
    if( yp_isexceptionC( so_toremove ) ) return so_toremove;

    // Remove items from so_toremove that are yielded by mi.  so_toremove is then a set
    // containing the keys to remove from so.
    result = _ypSet_difference_update_from_iter( so_toremove, mi, mi_state );
    if( !yp_isexceptionC( result ) ) {
        result = _ypSet_difference_update_from_set( so, so_toremove );
    }
    yp_decref( so_toremove );
    return result;
}

// Removes the keys not yielded from iterable from the set
// XXX Check for the so==iterable case _before_ calling this function
static ypObject *_ypSet_intersection_update( ypObject *so, ypObject *iterable )
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE( iterable );
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *result;

    // Recall that type pairs are identified by the immutable type code
    if( iterable_pair == ypFrozenSet_CODE ) {
        return _ypSet_intersection_update_from_set( so, iterable );
    } else {
        mi = yp_miniiter( iterable, &mi_state ); // new ref
        if( yp_isexceptionC( mi ) ) return mi;
        result = _ypSet_intersection_update_from_iter( so, mi, &mi_state );
        yp_decref( mi );
        return result;
    }
}

// XXX Check for the so==other case _before_ calling this function
static ypObject *_ypSet_difference_update_from_set( ypObject *so, ypObject *other )
{
    yp_ssize_t keysleft = ypSet_LEN( other );
    ypSet_KeyEntry *otherkeys = ypSet_TABLE( other );
    ypObject *result;
    yp_ssize_t i;
    ypSet_KeyEntry *loc;

    // We break once so is empty because we aren't expecting any errors from _ypSet_lookkey
    for( i = 0; keysleft > 0; i++ ) {
        if( ypSet_LEN( so ) < 1 ) break;
        if( !ypSet_ENTRY_USED( &otherkeys[i] ) ) continue;
        keysleft -= 1;
        result = _ypSet_lookkey( so, otherkeys[i].se_key, otherkeys[i].se_hash, &loc );
        if( yp_isexceptionC( result ) ) return result;
        if( !ypSet_ENTRY_USED( loc ) ) continue; // if entry not used, key is not in set
        yp_decref( _ypSet_removekey( so, loc ) );
    }
    return yp_None;
}

// FIXME This _allows_ mi to yield mutable values, unlike issubset; standardize
static ypObject *_ypSet_difference_update_from_iter(
        ypObject *so, ypObject *mi, yp_uint64_t *mi_state )
{
    ypObject *result = yp_None;
    ypObject *key;

    // It's tempting to stop once so is empty, but doing so would mask errors in yielded keys
    while( 1 ) {
        key = yp_miniiter_next( mi, mi_state ); // new ref
        if( yp_isexceptionC( key ) ) {
            if( yp_isexceptionC2( key, yp_StopIteration ) ) break;
            return key;
        }
        result = _ypSet_pop( so, key ); // new ref
        yp_decref( key );
        if( yp_isexceptionC( result ) ) return result;
        yp_decref( result );
    }
    return yp_None;
}

// Removes the keys yielded from iterable from the set
// XXX Check for the so==iterable case _before_ calling this function
static ypObject *_ypSet_difference_update( ypObject *so, ypObject *iterable )
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE( iterable );
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *result;

    // Recall that type pairs are identified by the immutable type code
    if( iterable_pair == ypFrozenSet_CODE ) {
        return _ypSet_difference_update_from_set( so, iterable );
    } else {
        mi = yp_miniiter( iterable, &mi_state ); // new ref
        if( yp_isexceptionC( mi ) ) return mi;
        result = _ypSet_difference_update_from_iter( so, mi, &mi_state );
        yp_decref( mi );
        return result;
    }
}

// XXX Check for the so==other case _before_ calling this function
static ypObject *_ypSet_symmetric_difference_update_from_set( ypObject *so, ypObject *other )
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    yp_ssize_t keysleft = ypSet_LEN( other );
    ypSet_KeyEntry *otherkeys = ypSet_TABLE( other );
    ypObject *result;
    yp_ssize_t i;

    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &otherkeys[i] ) ) continue;
        keysleft -= 1;

        // First, attempt to remove; if nothing was removed, then add it instead
        // TODO _ypSet_pop and _ypSet_push both call yp_currenthashC; consolidate?
        result = _ypSet_pop( so, otherkeys[i].se_key );
        if( yp_isexceptionC( result ) ) return result;
        if( result == ypSet_dummy ) {
            result = _ypSet_push( so, otherkeys[i].se_key, &spaceleft ); // may resize so
            if( yp_isexceptionC( result ) ) return result;
        } else {
            // XXX spaceleft based on alloclen and fill, so doesn't change on deletions
            yp_decref( result );
        }
    }
    return yp_None;
}


// Public methods

static ypObject *_frozenset_decref_visitor( ypObject *x, void *memo ) {
    yp_decref( x );
    return yp_None;
}

static ypObject *frozenset_traverse( ypObject *so, visitfunc visitor, void *memo )
{
    ypSet_KeyEntry *keys = ypSet_TABLE( so );
    yp_ssize_t keysleft = ypSet_LEN( so );
    yp_ssize_t i;
    ypObject *result;

    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &keys[i] ) ) continue;
        keysleft -= 1;
        result = visitor( keys[i].se_key, memo );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *frozenset_freeze( ypObject *so ) {
    return yp_None; // no-op, currently
}

static ypObject *frozenset_unfrozen_copy( ypObject *so, visitfunc copy_visitor, void *copy_memo )
{
    ypObject *result;
    ypObject *so_copy = _yp_set_new( ypSet_LEN( so ) ); // new ref
    if( yp_isexceptionC( so_copy ) ) return so_copy;
    result = _ypSet_update_from_set( so_copy, so, copy_visitor, copy_memo );
    if( yp_isexceptionC( result ) ) {
        yp_decref( so_copy );
        return result;
    }
    return so_copy;
}

static ypObject *frozenset_frozen_copy( ypObject *so, visitfunc copy_visitor, void *copy_memo )
{
    ypObject *result;
    ypObject *so_copy = _yp_frozenset_new( ypSet_LEN( so ) ); // new ref
    if( yp_isexceptionC( so_copy ) ) return so_copy;
    result = _ypSet_update_from_set( so_copy, so, copy_visitor, copy_memo );
    if( yp_isexceptionC( result ) ) {
        yp_decref( so_copy );
        return result;
    }
    return so_copy;
}

static ypObject *frozenset_bool( ypObject *so ) {
    return ypBool_FROM_C( ypSet_LEN( so ) );
}

// XXX Adapted from Python's frozenset_hash
static ypObject *frozenset_currenthash( ypObject *so,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *_hash )
{
    ypSet_KeyEntry *keys = ypSet_TABLE( so );
    yp_ssize_t keysleft = ypSet_LEN( so );
    yp_ssize_t i;
    yp_uhash_t h, hash = 1927868237U;

    hash *= (yp_uhash_t)ypSet_LEN(so) + 1;
    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &keys[i] ) ) continue;
        keysleft -= 1;
        /* Work to increase the bit dispersion for closely spaced hash
           values.  The is important because some use cases have many
           combinations of a small number of elements with nearby
           hashes so that many distinct combinations collapse to only
           a handful of distinct hash values. */
        h = keys[i].se_hash;
        hash ^= (h ^ (h << 16) ^ 89869747U)  * 3644798167U;
    }
    hash = hash * 69069U + 907133923U;
    if (hash == (yp_uhash_t)ypObject_HASH_INVALID) {
        hash = 590923713U;
    }
    *_hash = (yp_hash_t)hash;
    return yp_None;
}

typedef struct {
    yp_uint32_t keysleft;
    yp_uint32_t index;
} ypSetMiState;
yp_STATIC_ASSERT( ypObject_ALLOCLEN_INVALID <= 0xFFFFFFFFu, alloclen_fits_32_bits );
yp_STATIC_ASSERT( sizeof( yp_uint64_t ) >= sizeof( ypSetMiState ), ypSetMiState_fits_uint64 );

static ypObject *frozenset_miniiter( ypObject *so, yp_uint64_t *_state )
{
    ypSetMiState *state = (ypSetMiState *) _state;
    state->keysleft = ypSet_LEN( so );
    state->index = 0;
    return yp_incref( so );
}

// XXX We need to be a little suspicious of _state...just in case the caller has changed it
static ypObject *frozenset_miniiter_next( ypObject *so, yp_uint64_t *_state )
{
    ypSetMiState *state = (ypSetMiState *) _state;
    ypSet_KeyEntry *loc;

    // Find the next entry
    if( state->keysleft < 1 ) return yp_StopIteration;
    while( 1 ) {
        if( state->index >= ypSet_ALLOCLEN( so ) ) {
            state->keysleft = 0;
            return yp_StopIteration;
        }
        loc = &ypSet_TABLE( so )[state->index];
        state->index += 1;
        if( ypSet_ENTRY_USED( loc ) ) break;
    }

    // Update state and return the key
    state->keysleft -= 1;
    return yp_incref( loc->se_key );
}

static ypObject *frozenset_miniiter_lenhint(
        ypObject *so, yp_uint64_t *state, yp_ssize_t *lenhint )
{
    *lenhint = ((ypSetMiState *) state)->keysleft;
    return yp_None;
}

static ypObject *frozenset_contains( ypObject *so, ypObject *x )
{
    yp_hash_t hash;
    ypObject *result = yp_None;
    ypSet_KeyEntry *loc;

    hash = yp_currenthashC( x, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( so, x, hash, &loc );
    if( yp_isexceptionC( result ) ) return result;
    return ypBool_FROM_C( ypSet_ENTRY_USED( loc ) );
}

static ypObject *frozenset_isdisjoint( ypObject *so, ypObject *x )
{
    ypObject *x_asset;
    ypObject *result;

    if( so == x && ypSet_LEN( so ) > 0 ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) == ypFrozenSet_CODE ) {
        return _ypSet_isdisjoint( so, x );
    } else {
        // Otherwise, we need to convert x to a set to quickly test if it contains all items
        // TODO Can we make a version of _ypSet_isdisjoint that doesn't reqire a new set created?
        x_asset = yp_frozenset( x );
        result = _ypSet_isdisjoint( so, x_asset );
        yp_decref( x_asset );
        return result;
    }
}

static ypObject *frozenset_issubset( ypObject *so, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    ypObject *x_asset;
    ypObject *result;

    // We can take some shortcuts if x is a set or a dict
    if( so == x ) return yp_True;
    if( x_pair == ypFrozenSet_CODE ) {
        return _ypSet_issubset( so, x );
    } else if( x_pair == ypFrozenDict_CODE ) {
        if( ypSet_LEN( so ) > ypDict_LEN( x ) ) return yp_False;
    }

    // Otherwise, we need to convert x to a set to quickly test if it contains all items
    // TODO Can we make a version of _ypSet_issubset that doesn't reqire a new set created?
    x_asset = yp_frozenset( x );
    result = _ypSet_issubset( so, x_asset );
    yp_decref( x_asset );
    return result;
}

// Remember that if x.issubset(so), then so.issuperset(x)
static ypObject *frozenset_issuperset( ypObject *so, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    ypObject *x_asset;
    ypObject *result;

    // We can take some shortcuts if x is a set or a dict
    if( so == x ) return yp_True;
    if( x_pair == ypFrozenSet_CODE ) {
        return _ypSet_issubset( x, so );
    } else if( x_pair == ypFrozenDict_CODE ) {
        if( ypDict_LEN( x ) > ypSet_LEN( so ) ) return yp_False;
    }

    // Otherwise, we need to convert x to a set to quickly test if it contains all items
    // TODO Can we make a version of _ypSet_issubset that doesn't reqire a new set created?
    x_asset = yp_frozenset( x );
    result = _ypSet_issubset( x_asset, so );
    yp_decref( x_asset );
    return result;
}

static ypObject *frozenset_lt( ypObject *so, ypObject *x )
{
    if( so == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypFrozenSet_CODE ) return yp_ComparisonNotImplemented;
    if( ypSet_LEN( so ) >= ypSet_LEN( x ) ) return yp_False;
    return _ypSet_issubset( so, x );
}

static ypObject *frozenset_le( ypObject *so, ypObject *x )
{
    if( so == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypFrozenSet_CODE ) return yp_ComparisonNotImplemented;
    return _ypSet_issubset( so, x );
}

static ypObject *frozenset_eq( ypObject *so, ypObject *x )
{
    if( so == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypFrozenSet_CODE ) return yp_ComparisonNotImplemented;
    if( ypSet_LEN( so ) != ypSet_LEN( x ) ) return yp_False;
    // FIXME Compare stored hashes (they should be equal if so and x are equal)
    return _ypSet_issubset( so, x );
}

static ypObject *frozenset_ne( ypObject *so, ypObject *x )
{
    ypObject *result = frozenset_eq( so, x );
    return ypBool_NOT( result );
}

static ypObject *frozenset_ge( ypObject *so, ypObject *x )
{
    if( so == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypFrozenSet_CODE ) return yp_ComparisonNotImplemented;
    return _ypSet_issubset( x, so );
}

static ypObject *frozenset_gt( ypObject *so, ypObject *x )
{
    if( so == x ) return yp_False;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypFrozenSet_CODE ) return yp_ComparisonNotImplemented;
    if( ypSet_LEN( so ) <= ypSet_LEN( x ) ) return yp_False;
    return _ypSet_issubset( x, so );
}

static ypObject *set_update( ypObject *so, int n, va_list args )
{
    ypObject *result;
    for( /*n already set*/; n > 0; n-- ) {
        ypObject *x = va_arg( args, ypObject * );
        if( so == x ) continue;
        result = _ypSet_update( so, x );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *set_intersection_update( ypObject *so, int n, va_list args )
{
    ypObject *result;
    // It's tempting to stop once so is empty, but doing so would mask errors in args
    for( /*n already set*/; n > 0; n-- ) {
        ypObject *x = va_arg( args, ypObject * );
        if( so == x ) continue;
        result = _ypSet_intersection_update( so, x );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *set_clear( ypObject *so );
static ypObject *set_difference_update( ypObject *so, int n, va_list args )
{
    ypObject *result;
    // It's tempting to stop once so is empty, but doing so would mask errors in args
    for( /*n already set*/; n > 0; n-- ) {
        ypObject *x = va_arg( args, ypObject * );
        if( so == x ) {
            result = set_clear( so );
        } else {
            result = _ypSet_difference_update( so, x );
        }
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *set_symmetric_difference_update( ypObject *so, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    ypObject *result;

    if( so == x ) return set_clear( so );

    // Recall that type pairs are identified by the immutable type code
    if( x_pair == ypFrozenSet_CODE ) {
        return _ypSet_symmetric_difference_update_from_set( so, x );
    } else {
        // TODO Can we make a version of _ypSet_symmetric_difference_update_from_set that doesn't
        // reqire a new set created?
        ypObject *x_asset = yp_frozenset( x );
        if( yp_isexceptionC( x_asset ) ) return x_asset;
        result = _ypSet_symmetric_difference_update_from_set( so, x_asset );
        yp_decref( x_asset );
        return result;
    }
}

static ypObject *set_pushunique( ypObject *so, ypObject *x ) {
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    ypObject *result = _ypSet_push( so, x, &spaceleft );
    if( yp_isexceptionC( result ) ) return result;
    return result == yp_True ? yp_None : yp_KeyError;
}

static ypObject *set_push( ypObject *so, ypObject *x ) {
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    ypObject *result = _ypSet_push( so, x, &spaceleft );
    if( yp_isexceptionC( result ) ) return result;
    return yp_None;
}

static ypObject *set_clear( ypObject *so ) {
    if( ypSet_FILL( so ) < 1 ) return yp_None;
    frozenset_traverse( so, _frozenset_decref_visitor, NULL ); // cannot fail
    // FIXME there's a memcpy in this that we can and should avoid
    ypMem_REALLOC_CONTAINER_VARIABLE( so, ypSetObject, ypSet_MINSIZE, ypSet_MINSIZE );
    ypSet_ALLOCLEN( so ) = ypSet_MINSIZE; // we can't make use of the excess anyway
    ypSet_LEN( so ) = 0;
    ypSet_FILL( so ) = 0;
    memset( ypSet_TABLE( so ), 0, ypSet_MINSIZE * sizeof( ypSet_KeyEntry ) );
    return yp_None;
}

// Note the difference between this, which removes an arbitrary key, and
// _ypSet_pop, which removes a specific key
// XXX Adapted from Python's set_pop
static ypObject *set_pop( ypObject *so ) {
    register yp_ssize_t i = 0;
    register ypSet_KeyEntry *table = ypSet_TABLE( so );
    ypObject *key;

    if( ypSet_LEN( so ) < 1 ) return yp_KeyError; // "pop from an empty set"

    /* We abuse the hash field of slot 0 to hold a search finger:
     * If slot 0 has a value, use slot 0.
     * Else slot 0 is being used to hold a search finger,
     * and we use its hash value as the first index to look.
     */
    if( !ypSet_ENTRY_USED( table ) ) {
        i = table->se_hash;
        /* The hash field may be a real hash value, or it may be a
         * legit search finger, or it may be a once-legit search
         * finger that's out of bounds now because it wrapped around
         * or the table shrunk -- simply make sure it's in bounds now.
         */
        if( i > ypSet_MASK( so ) || i < 1 ) i = 1; /* skip slot 0 */
        while( !ypSet_ENTRY_USED( table+i ) ) {
            i++;
            if( i > ypSet_MASK( so ) ) i = 1;
        }
    }
    key = _ypSet_removekey( so, table+i );
    table->se_hash = i + 1;  /* next place to start */
    return key;
}

static ypObject *frozenset_len( ypObject *so, yp_ssize_t *len ) {
    *len = ypSet_LEN( so );
    return yp_None;
}

// onmissing must be an immortal, or NULL
static ypObject *set_remove( ypObject *so, ypObject *x, ypObject *onmissing )
{
    ypObject *result = _ypSet_pop( so, x );
    if( yp_isexceptionC( result ) ) return result;
    if( result == ypSet_dummy ) {
        if( onmissing == NULL ) return yp_KeyError;
        return onmissing;
    }
    yp_decref( result );
    return yp_None;
}

static ypObject *frozenset_dealloc( ypObject *so )
{
    frozenset_traverse( so, _frozenset_decref_visitor, NULL ); // cannot fail
    ypMem_FREE_CONTAINER( so, ypSetObject );
    return yp_None;
}

static ypSetMethods ypFrozenSet_as_set = {
    frozenset_isdisjoint,           // tp_isdisjoint
    frozenset_issubset,             // tp_issubset
    // tp_lt is elsewhere
    frozenset_issuperset,           // tp_issuperset
    // tp_gt is elsewhere
    MethodError_objvalistproc,      // tp_update
    MethodError_objvalistproc,      // tp_intersection_update
    MethodError_objvalistproc,      // tp_difference_update
    MethodError_objobjproc,         // tp_symmetric_difference_update
    MethodError_objobjproc          // tp_pushunique
};

static ypTypeObject ypFrozenSet_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    frozenset_dealloc,              // tp_dealloc
    frozenset_traverse,             // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    frozenset_freeze,               // tp_freeze
    frozenset_unfrozen_copy,        // tp_unfrozen_copy
    frozenset_frozen_copy,          // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    frozenset_bool,                 // tp_bool
    frozenset_lt,                   // tp_lt
    frozenset_le,                   // tp_le
    frozenset_eq,                   // tp_eq
    frozenset_ne,                   // tp_ne
    frozenset_ge,                   // tp_ge
    frozenset_gt,                   // tp_gt

    // Generic object operations
    frozenset_currenthash,          // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    frozenset_miniiter,             // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    frozenset_miniiter_next,        // tp_miniiter_next
    frozenset_miniiter_lenhint,     // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    frozenset_contains,             // tp_contains
    frozenset_len,                  // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    &ypFrozenSet_as_set,            // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSetMethods ypSet_as_set = {
    frozenset_isdisjoint,           // tp_isdisjoint
    frozenset_issubset,             // tp_issubset
    // tp_lt is elsewhere
    frozenset_issuperset,           // tp_issuperset
    // tp_gt is elsewhere
    set_update,                     // tp_update
    set_intersection_update,        // tp_intersection_update
    set_difference_update,          // tp_difference_update
    set_symmetric_difference_update,// tp_symmetric_difference_update
    set_pushunique,                 // tp_pushunique
};

static ypTypeObject ypSet_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    frozenset_dealloc,              // tp_dealloc
    frozenset_traverse,             // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    frozenset_freeze,               // tp_freeze
    frozenset_unfrozen_copy,        // tp_unfrozen_copy
    frozenset_frozen_copy,          // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    frozenset_bool,                 // tp_bool
    frozenset_lt,                   // tp_lt
    frozenset_le,                   // tp_le
    frozenset_eq,                   // tp_eq
    frozenset_ne,                   // tp_ne
    frozenset_ge,                   // tp_ge
    frozenset_gt,                   // tp_gt

    // Generic object operations
    frozenset_currenthash,          // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    frozenset_miniiter,             // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    frozenset_miniiter_next,        // tp_miniiter_next
    frozenset_miniiter_lenhint,     // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    frozenset_contains,             // tp_contains
    frozenset_len,                  // tp_len
    set_push,                       // tp_push
    set_clear,                      // tp_clear
    set_pop,                        // tp_pop
    set_remove,                     // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    &ypSet_as_set,                  // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};


// Public functions

void yp_set_add( ypObject **set, ypObject *x )
{
    ypObject *result;
    if( ypObject_TYPE_CODE( *set ) != ypSet_CODE ) return_yp_INPLACE_BAD_TYPE( set, *set );
    result = set_push( *set, x );
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( set, result );
}


// Constructors

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypSet( ypObject *(*allocator)( yp_ssize_t ), ypObject *iterable )
{
    ypObject *exc = yp_None;
    ypObject *newSo;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( iterable, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( iterable, &exc );
    } else if( lenhint == 0 && allocator == _yp_frozenset_new ) {
        // yp_lenC reports an empty iterable, so we can shortcut frozenset creation
        return _yp_frozenset_empty;
    }

    newSo = allocator( lenhint );
    if( yp_isexceptionC( newSo ) ) return newSo;
    // TODO make sure _yp_set_update is efficient for pre-sized objects
    result = _ypSet_update( newSo, iterable );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSo );
        return result;
    }
    return newSo;
}

ypObject *yp_frozensetN( int n, ... ) {
    if( n < 1 ) return _yp_frozenset_empty;
    return_yp_V_FUNC( ypObject *, yp_frozensetV, (n, args), n );
}
ypObject *yp_frozensetV( int n, va_list args ) {
    // TODO Return _yp_frozenset_empty on n<1??
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypSet( _yp_frozenset_new, iter_args );
}
ypObject *yp_frozenset( ypObject *iterable ) {
    if( ypObject_TYPE_CODE( iterable ) == ypFrozenSet_CODE ) return yp_incref( iterable );
    return _ypSet( _yp_frozenset_new, iterable );
}

ypObject *yp_setN( int n, ... ) {
    if( n == 0 ) return _yp_set_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_setV, (n, args), n );
}
ypObject *yp_setV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypSet( _yp_set_new, iter_args );
}
ypObject *yp_set( ypObject *iterable ) {
    return _ypSet( _yp_set_new, iterable );
}


/*************************************************************************************************
 * Mappings
 *************************************************************************************************/

// XXX Much of this set/dict implementation is pulled right from Python, so best to read the
// original source for documentation on this implementation

// XXX keyset requires care!  It is potentially shared among multiple dicts, so we cannot remove
// keys or resize it.  It identifies itself as a frozendict, yet we add keys to it, so it is not
// truly immutable.  As such, it cannot be exposed outside of the set/dict implementations.
// TODO investigate how/when the keyset will be shared between dicts
// TODO alloclen will always be the same as keyset.alloclen; repurpose?

// ypDictObject and ypDict_LEN are defined above, for use by the set code
#define ypDict_KEYSET( mp )         ( ((ypDictObject *)mp)->keyset )
#define ypDict_ALLOCLEN( mp )       ypSet_ALLOCLEN( ypDict_KEYSET( mp ) )
#define ypDict_VALUES( mp )         ( (ypObject **) ((ypObject *)mp)->ob_data )
#define ypDict_SET_VALUES( mp, x )  ( ((ypObject *)mp)->ob_data = x )
#define ypDict_INLINE_DATA( mp )    ( ((ypDictObject *)mp)->ob_inline_data )

// Returns the index of the given ypSet_KeyEntry in the hash table
// TODO needed?
#define ypDict_ENTRY_INDEX( mp, loc ) \
    ( ypSet_ENTRY_INDEX( ypDict_KEYSET( mp ), loc ) )

// Returns a pointer to the value element corresponding to the given key location
#define ypDict_VALUE_ENTRY( mp, key_loc ) \
    ( &(ypDict_VALUES( mp )[ypSet_ENTRY_INDEX( ypDict_KEYSET( mp ), key_loc )]) )

// Empty frozendicts can be represented by this, immortal object
// TODO Can we use this in more places...anywhere we'd return a possibly-empty frozendict?
static ypObject _yp_frozendict_empty_data[ypSet_MINSIZE] = {0};
static ypDictObject _yp_frozendict_empty_struct = {
    { ypObject_MAKE_TYPE_REFCNT( ypFrozenDict_CODE, ypObject_REFCNT_IMMORTAL ),
    0, ypSet_MINSIZE, ypObject_HASH_INVALID, _yp_frozendict_empty_data }, 
    (ypObject *) &_yp_frozenset_empty_struct };
static ypObject * const _yp_frozendict_empty = (ypObject *) &_yp_frozendict_empty_struct;

// TODO can use CONTAINER_INLINE if we're sure the frozendict won't grow past minused while being
// created
// XXX A minused of zero may mean lenhint is unreliable; we may still grow the frozendict, so don't
// return _yp_frozendict_empty!
static ypObject *_yp_frozendict_new( yp_ssize_t minused )
{
    ypObject *keyset;
    yp_ssize_t alloclen;
    ypObject *mp;

    keyset = _yp_frozenset_new( minused );
    if( yp_isexceptionC( keyset ) ) return keyset;
    alloclen = ypSet_ALLOCLEN( keyset );
    mp = ypMem_MALLOC_CONTAINER_VARIABLE( ypDictObject, ypFrozenDict_CODE, alloclen, 0 );
    if( yp_isexceptionC( mp ) ) {
        yp_decref( keyset );
        return mp;
    }
    ypDict_KEYSET( mp ) = keyset;
    memset( ypDict_VALUES( mp ), 0, alloclen * sizeof( ypObject * ) );
    return mp;
}

static ypObject *_yp_dict_new( yp_ssize_t minused )
{
    ypObject *keyset;
    yp_ssize_t alloclen;
    ypObject *mp;

    keyset = _yp_frozenset_new( minused );
    if( yp_isexceptionC( keyset ) ) return keyset;
    alloclen = ypSet_ALLOCLEN( keyset );
    mp = ypMem_MALLOC_CONTAINER_VARIABLE( ypDictObject, ypDict_CODE, alloclen, 0 );
    if( yp_isexceptionC( mp ) ) {
        yp_decref( keyset );
        return mp;
    }
    ypDict_KEYSET( mp ) = keyset;
    memset( ypDict_VALUES( mp ), 0, alloclen * sizeof( ypObject * ) );
    return mp;
}

// The tricky bit about resizing dicts is that we need both the old and new keysets and value
// arrays to properly transfer the data, so ypMem_REALLOC_CONTAINER_VARIABLE is no help.
static ypObject *_ypDict_resize( ypObject *mp, yp_ssize_t minused )
{
    ypObject *newkeyset;
    yp_ssize_t newalloclen;
    ypObject **newvalues;
    yp_ssize_t newsize;
    ypSet_KeyEntry *oldkeys;
    ypObject **oldvalues;
    yp_ssize_t valuesleft;
    yp_ssize_t i;
    ypObject *value;
    ypSet_KeyEntry *newkey_loc;

    // TODO allocate the value array in-line, then handle the case where both old and new value
    // arrays could fit in-line (idea: if currently in-line, then just force that the new array be
    // malloc'd...will need to malloc something anyway)
    newkeyset = _yp_frozenset_new( minused );
    if( yp_isexceptionC( newkeyset ) ) return newkeyset;
    newalloclen = ypSet_ALLOCLEN( newkeyset );
    newvalues = (ypObject **) yp_malloc( &newsize, newalloclen * sizeof( ypObject * ) );
    if( newvalues == NULL ) {
        yp_decref( newkeyset );
        return yp_MemoryError;
    }
    memset( newvalues, 0, newalloclen * sizeof( ypObject * ) );

    oldkeys = ypSet_TABLE( ypDict_KEYSET( mp ) );
    oldvalues = ypDict_VALUES( mp );
    valuesleft = ypDict_LEN( mp );
    for( i = 0; valuesleft > 0; i++ ) {
        value = ypDict_VALUES( mp )[i];
        if( value == NULL ) continue;
        _ypSet_movekey_clean( newkeyset, yp_incref( oldkeys[i].se_key ), oldkeys[i].se_hash,
                &newkey_loc );
        newvalues[ypSet_ENTRY_INDEX( newkeyset, newkey_loc )] = oldvalues[i];
        valuesleft -= 1;
    }

    yp_decref( ypDict_KEYSET( mp ) );
    ypDict_KEYSET( mp ) = newkeyset;
    if( oldvalues != ypDict_INLINE_DATA( mp ) ) yp_free( oldvalues );
    ypDict_SET_VALUES( mp, newvalues );
    return yp_None;
}

// Adds the key/value to the dict.  If override is false, returns yp_False and does not modify the
// dict if there is an existing value.  *spaceleft should be initialized from
// _ypSet_space_remaining; this function then decrements it with each key added, and resets it on
// every resize.  Returns yp_True if mp was modified, yp_False if it wasn't due to existing values
// being preserved (ie override is false), or an exception on error.
// XXX Adapted from PyDict_SetItem
// TODO The decision to resize currently depends only on _ypSet_space_remaining, but what if the
// shared keyset contains 5x the keys that we actually use?  That's a large waste in the value
// table.  Really, we should have a _ypDict_space_remaining.
static ypObject *_ypDict_push( ypObject *mp, ypObject *key, ypObject *value, int override,
        yp_ssize_t *spaceleft )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *key_loc;
    ypObject *result = yp_None;
    ypObject **value_loc;
    yp_ssize_t newlen;

    // Look for the appropriate entry in the hash table
    // TODO yp_isexceptionC used internally should be a macro
    hash = yp_hashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( keyset, key, hash, &key_loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the key is already in the hash table, then we simply need to update the value
    if( ypSet_ENTRY_USED( key_loc ) ) {
        value_loc = ypDict_VALUE_ENTRY( mp, key_loc );
        if( *value_loc == NULL ) {
            *value_loc = yp_incref( value );
            ypDict_LEN( mp ) += 1;
        } else {
            if( !override ) return yp_False;
            yp_decref( *value_loc );
            *value_loc = yp_incref( value );
        }
        return yp_True;
    }

    // Otherwise, we need to add the key, which possibly doesn't involve resizing
    // TODO spaceleft
    if( *spaceleft >= 1 ) {
        _ypSet_movekey( keyset, key_loc, yp_incref( key ), hash );
        *ypDict_VALUE_ENTRY( mp, key_loc ) = yp_incref( value );
        ypDict_LEN( mp ) += 1;
        *spaceleft -= 1;
        return yp_True;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean.  Give mutable objects a bit of room to grow.
    newlen = ypDict_LEN( mp )+1;
    if( ypObject_IS_MUTABLE( mp ) ) newlen = _ypSet_calc_resize_minused( newlen );
    result = _ypDict_resize( mp, newlen );  // invalidates keyset and key_loc
    if( yp_isexceptionC( result ) ) return result;

    keyset = ypDict_KEYSET( mp );
    _ypSet_movekey_clean( keyset, yp_incref( key ), hash, &key_loc );
    *ypDict_VALUE_ENTRY( mp, key_loc ) = yp_incref( value );
    ypDict_LEN( mp ) += 1;
    *spaceleft = _ypSet_space_remaining( keyset );
    return yp_True;
}

// Removes the value from the dict; the key stays in the keyset, but that's of no concern.  The
// dict is not resized.  Returns the reference to the removed value if mp was modified, ypSet_dummy
// if it wasn't due to the value not being set, or an exception on error.
static ypObject *_ypDict_pop( ypObject *mp, ypObject *key )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *key_loc;
    ypObject *result = yp_None;
    ypObject **value_loc;
    ypObject *oldvalue;

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( keyset, key, hash, &key_loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the there's no existing value, then there's nothing to do (if the key is not in the set,
    // then *value_loc will be NULL)
    value_loc = ypDict_VALUE_ENTRY( mp, key_loc );
    if( *value_loc == NULL ) return ypSet_dummy;

    // Otherwise, we need to remove the value
    oldvalue = *value_loc;
    *value_loc = NULL;
    ypDict_LEN( mp ) -= 1;
    return oldvalue; // new ref
}

// Item iterators yield iterators that yield exactly 2 values: key first, then value.  This
// returns new references to that pair in *key and *value.  Both are set to an exception on error;
// in particular, yp_ValueError is returned if exactly 2 values are not returned.
// XXX *itemiter may be an yp_ONSTACK_ITER_KVALIST: use carefully
static void _ypDict_iter_items_next( ypObject *itemiter, ypObject **key, ypObject **value )
{
    ypObject *excess;
    ypObject *keyvaliter = yp_next( itemiter ); // new ref
    if( yp_isexceptionC( keyvaliter ) ) { // including yp_StopIteration
        *key = *value = keyvaliter;
        return;
    }

    *key = yp_next( keyvaliter ); // new ref
    if( yp_isexceptionC( *key ) ) {
        if( yp_isexceptionC2( *key, yp_StopIteration ) ) *key = yp_ValueError;
        *value = *key;
        goto Return;
    }

    *value = yp_next( keyvaliter ); // new ref
    if( yp_isexceptionC( *value ) ) {
        if( yp_isexceptionC2( *value, yp_StopIteration ) ) *value = yp_ValueError;
        yp_decref( *key );
        *key = *value;
        goto Return;
    }

    excess = yp_next( keyvaliter ); // new ref, but should be yp_StopIteration
    if( excess != yp_StopIteration ) {
        if( !yp_isexceptionC( excess ) ) {
            yp_decref( excess );
            excess = yp_ValueError;
        }
        yp_decrefN( 2, *key, *value );
        *key = *value = excess;
        goto Return;
    }

Return:
    yp_decref( keyvaliter );
    return;
}

// TODO ...what if the dict we're updating against shares the same keyset?
static ypObject *_ypDict_update_from_dict( ypObject *mp, ypObject *other,
        visitfunc copy_visitor, void *copy_memo )
{
    return yp_NotImplementedError; // TODO
}

// XXX *itemiter may be an yp_ONSTACK_ITER_KVALIST: use carefully
static ypObject *_ypDict_update_from_iter( ypObject *mp, ypObject *itemiter )
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( ypDict_KEYSET( mp ) );
    ypObject *result = yp_None;
    yp_ssize_t lenhint;
    ypObject *key;
    ypObject *value;

    // Use lenhint in the hopes of requiring only one resize
    // FIXME instead, wait until we need a resize, then since we're resizing anyway, resize to fit
    // the given lenhint
    lenhint = yp_iter_lenhintC( itemiter, &result );
    if( yp_isexceptionC( result ) ) return result;
    if( spaceleft < lenhint ) {
        result = _ypDict_resize( mp, ypDict_LEN( mp )+lenhint );
        if( yp_isexceptionC( result ) ) return result;
        spaceleft = _ypSet_space_remaining( ypDict_KEYSET( mp ) );
    }

    // Add all the yielded items to the dict.  Keep in mind that _ypDict_push may resize the
    // dict despite our attempts at pre-allocating, since lenhint _is_ just a hint.
    while( 1 ) {
        _ypDict_iter_items_next( itemiter, &key, &value ); // new refs: key, value
        if( yp_isexceptionC( key ) ) {
            if( yp_isexceptionC2( key, yp_StopIteration ) ) break;
            return key;
        }
        result = _ypDict_push( mp, key, value, 1, &spaceleft );
        yp_decrefN( 2, key, value );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

// Adds the key/value pairs yielded from either yp_iter_items or yp_iter to the dict.  If the dict
// has enough space to hold all the items, the dict is not resized (important, as yp_dictK et al
// pre-allocate the necessary space).
static ypObject *_ypDict_update( ypObject *mp, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    ypObject *itemiter;
    ypObject *result;

    // If x is a fellow dict there are efficiencies we can exploit; otherwise, prefer yp_iter_items
    // over yp_iter if supported.  Recall that type pairs are identified by the immutable type
    // code.
    if( x_pair == ypFrozenDict_CODE ) {
        return _ypDict_update_from_dict( mp, x, yp_shallowcopy_visitor, NULL );
    } else {
        // FIXME replace with yp_miniiter_items once supported
        itemiter = yp_iter_items( x ); // new ref
        if( yp_isexceptionC2( itemiter, yp_MethodError ) ) itemiter = yp_iter( x ); // new ref
        if( yp_isexceptionC( itemiter ) ) return itemiter;
        result = _ypDict_update_from_iter( mp, itemiter );
        yp_decref( itemiter );
        return result;
    }
}

// Public methods

static ypObject *frozendict_traverse( ypObject *mp, visitfunc visitor, void *memo )
{
    yp_ssize_t valuesleft = ypDict_LEN( mp );
    yp_ssize_t i;
    ypObject *value;
    ypObject *result;

    result = visitor( ypDict_KEYSET( mp ), memo );
    if( yp_isexceptionC( result ) ) return result;

    for( i = 0; valuesleft > 0; i++ ) {
        value = ypDict_VALUES( mp )[i];
        if( value == NULL ) continue;
        result = visitor( value, memo );
        if( yp_isexceptionC( result ) ) return result;
        valuesleft -= 1;
    }
    return yp_None;
}

static ypObject *frozendict_bool( ypObject *mp ) {
    return ypBool_FROM_C( ypDict_LEN( mp ) );
}

static ypObject *frozendict_contains( ypObject *mp, ypObject *key )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *key_loc;
    ypObject *result = yp_None;

    hash = yp_currenthashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( keyset, key, hash, &key_loc );
    if( yp_isexceptionC( result ) ) return result;
    return ypBool_FROM_C( (*ypDict_VALUE_ENTRY( mp, key_loc )) != NULL );
}

static ypObject *frozendict_len( ypObject *mp, yp_ssize_t *len ) {
    *len = ypDict_LEN( mp );
    return yp_None;
}

// A defval of None means to raise an error if key is not in dict
static ypObject *frozendict_getdefault( ypObject *mp, ypObject *key, ypObject *defval )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *key_loc;
    ypObject *result = yp_None;
    ypObject *value;

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( keyset, key, hash, &key_loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the there's no existing value, return defval, otherwise return the value
    value = *ypDict_VALUE_ENTRY( mp, key_loc );
    if( value == NULL ) {
        if( defval == NULL ) return yp_KeyError;
        return yp_incref( defval );
    } else {
        return yp_incref( value );
    }
}

// yp_None or an exception
static ypObject *dict_setitem( ypObject *mp, ypObject *key, ypObject *value )
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( ypDict_KEYSET( mp ) );
    ypObject *result = _ypDict_push( mp, key, value, 1, &spaceleft );
    if( yp_isexceptionC( result ) ) return result;
    return yp_None;
}

static ypObject *dict_delitem( ypObject *mp, ypObject *key )
{
    ypObject *result = _ypDict_pop( mp, key );
    if( yp_isexceptionC( result ) ) return result;
    if( result == ypSet_dummy ) return yp_KeyError;
    yp_decref( result );
    return yp_None;
}

typedef struct {
    yp_uint32_t keys : 1;
    yp_uint32_t itemsleft : 31;
    // aligned
    yp_uint32_t values : 1;
    yp_uint32_t index : 31;
} ypDictMiState;
yp_STATIC_ASSERT( ypObject_ALLOCLEN_INVALID <= 0x7FFFFFFFu, alloclen_fits_31_bits );
yp_STATIC_ASSERT( sizeof( yp_uint64_t ) >= sizeof( ypDictMiState ), ypDictMiState_fits_uint64 );

static ypObject *frozendict_miniiter_items( ypObject *mp, yp_uint64_t *_state )
{
    ypDictMiState *state = (ypDictMiState *) _state;
    state->keys = 1;
    state->values = 1;
    state->itemsleft = ypDict_LEN( mp );
    state->index = 0;
    return yp_incref( mp );
}
static ypObject *frozendict_iter_items( ypObject *x ) {
    return _ypMiIter_from_miniiter( x, frozendict_miniiter_items );
}

static ypObject *frozendict_miniiter_keys( ypObject *mp, yp_uint64_t *_state )
{
    ypDictMiState *state = (ypDictMiState *) _state;
    state->keys = 1;
    state->values = 0;
    state->itemsleft = ypDict_LEN( mp );
    state->index = 0;
    return yp_incref( mp );
}
static ypObject *frozendict_iter_keys( ypObject *x ) {
    return _ypMiIter_from_miniiter( x, frozendict_miniiter_keys );
}

static ypObject *frozendict_miniiter_values( ypObject *mp, yp_uint64_t *_state )
{
    ypDictMiState *state = (ypDictMiState *) _state;
    state->keys = 0;
    state->values = 1;
    state->itemsleft = ypDict_LEN( mp );
    state->index = 0;
    return yp_incref( mp );
}
static ypObject *frozendict_iter_values( ypObject *x ) {
    return _ypMiIter_from_miniiter( x, frozendict_miniiter_values );
}

// XXX We need to be a little suspicious of _state...just in case the caller has changed it
static ypObject *frozendict_miniiter_next( ypObject *mp, yp_uint64_t *_state )
{
    ypObject *result;
    ypDictMiState *state = (ypDictMiState *) _state;
    yp_ssize_t index = state->index; // don't forget to write it back
    if( state->itemsleft < 1 ) return yp_StopIteration;

    // Find the next entry
    while( 1 ) {
        if( index >= ypDict_ALLOCLEN( mp ) ) {
            state->index = index;
            state->itemsleft = 0;
            return yp_StopIteration;
        }
        if( ypDict_VALUES( mp )[index] != NULL ) break;
        index++;
    }

    // Find the requested data
    if( state->keys ) {
        if( state->values ) {
            result = yp_tupleN( 2, ypSet_TABLE( ypDict_KEYSET( mp ) )[index].se_key,
                                   ypDict_VALUES( mp )[index] );
        } else {
            result = yp_incref( ypSet_TABLE( ypDict_KEYSET( mp ) )[index].se_key );
        }
    } else {
        if( state->values ) {
            result = yp_incref( ypDict_VALUES( mp )[index] );
        } else {
            result = yp_SystemError; // should never occur
        }
    }
    if( yp_isexceptionC( result ) ) return result;

    // Update state and return
    state->index = index + 1;
    state->itemsleft -= 1;
    return result;
}

static ypObject *frozendict_miniiter_lenhint(
        ypObject *mp, yp_uint64_t *state, yp_ssize_t *lenhint )
{
    *lenhint = ((ypDictMiState *) state)->itemsleft;
    return yp_None;
}

static ypObject *_frozendict_dealloc_visitor( ypObject *x, void *memo ) {
    yp_decref( x );
    return yp_None;
}

static ypObject *frozendict_dealloc( ypObject *mp )
{
    frozendict_traverse( mp, _frozendict_dealloc_visitor, NULL ); // cannot fail
    ypMem_FREE_CONTAINER( mp, ypDictObject );
    return yp_None;
}

static ypMappingMethods ypFrozenDict_as_mapping = {
    frozendict_miniiter_items,      // tp_miniiter_items
    frozendict_iter_items,          // tp_iter_items
    frozendict_miniiter_keys,       // tp_miniiter_keys
    frozendict_iter_keys,           // tp_iter_keys
    MethodError_objobjobjproc,      // tp_popvalue
    MethodError_popitemfunc,        // tp_popitem
    MethodError_objobjobjproc,      // tp_setdefault
    frozendict_miniiter_values,     // tp_miniiter_values
    frozendict_iter_values          // tp_iter_values
};

static ypTypeObject ypFrozenDict_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    frozendict_dealloc,             // tp_dealloc
    frozendict_traverse,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    frozendict_bool,                // tp_bool
    MethodError_objobjproc,         // tp_lt
    MethodError_objobjproc,         // tp_le
    MethodError_objobjproc,         // tp_eq
    MethodError_objobjproc,         // tp_ne
    MethodError_objobjproc,         // tp_ge
    MethodError_objobjproc,         // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    frozendict_miniiter_keys,       // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    frozendict_miniiter_next,       // tp_miniiter_next
    frozendict_miniiter_lenhint,    // tp_miniiter_lenhint
    frozendict_iter_keys,           // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    frozendict_contains,            // tp_contains
    frozendict_len,                 // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    frozendict_getdefault,          // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    &ypFrozenDict_as_mapping        // tp_as_mapping
};

static ypMappingMethods ypDict_as_mapping = {
    frozendict_miniiter_items,      // tp_miniiter_items
    frozendict_iter_items,          // tp_iter_items
    frozendict_miniiter_keys,       // tp_miniiter_keys
    frozendict_iter_keys,           // tp_iter_keys
    MethodError_objobjobjproc,      // tp_popvalue
    MethodError_popitemfunc,        // tp_popitem
    MethodError_objobjobjproc,      // tp_setdefault
    frozendict_miniiter_values,     // tp_miniiter_values
    frozendict_iter_values          // tp_iter_values
};

static ypTypeObject ypDict_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    frozendict_dealloc,             // tp_dealloc
    frozendict_traverse,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    MethodError_traversefunc,       // tp_unfrozen_copy
    MethodError_traversefunc,       // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    frozendict_bool,                // tp_bool
    MethodError_objobjproc,         // tp_lt
    MethodError_objobjproc,         // tp_le
    MethodError_objobjproc,         // tp_eq
    MethodError_objobjproc,         // tp_ne
    MethodError_objobjproc,         // tp_ge
    MethodError_objobjproc,         // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    frozendict_miniiter_keys,       // tp_miniiter
    MethodError_miniiterfunc,       // tp_miniiter_reversed
    frozendict_miniiter_next,       // tp_miniiter_next
    frozendict_miniiter_lenhint,    // tp_miniiter_lenhint
    frozendict_iter_keys,           // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    frozendict_contains,            // tp_contains
    frozendict_len,                 // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    frozendict_getdefault,          // tp_getdefault
    dict_setitem,                   // tp_setitem
    dict_delitem,                   // tp_delitem

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    &ypDict_as_mapping              // tp_as_mapping
};

// Constructors
// XXX x may be an yp_ONSTACK_ITER_KVALIST: use carefully
// TODO if x is a fellow dict, consider sharing its keyset
static ypObject *_ypDict( ypObject *(*allocator)( yp_ssize_t ), ypObject *x )
{
    ypObject *exc = yp_None;
    ypObject *newMp;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( x, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( x, &exc );
    } else if( lenhint == 0 && allocator == _yp_frozendict_new ) {
        // yp_lenC reports an empty iterable, so we can shortcut frozendict creation
        return _yp_frozendict_empty;
    }

    newMp = allocator( lenhint );
    if( yp_isexceptionC( newMp ) ) return newMp;
    // TODO make sure _ypDict_update is efficient for pre-sized objects
    result = _ypDict_update( newMp, x );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newMp );
        return result;
    }
    return newMp;
}

ypObject *yp_frozendictK( int n, ... ) {
    if( n < 1 ) return _yp_frozendict_empty;
    return_yp_V_FUNC( ypObject *, yp_frozendictKV, (n, args), n );
}
ypObject *yp_frozendictKV( int n, va_list args ) {
    // TODO Return _yp_frozendict_empty on n<1??
    yp_ONSTACK_ITER_KVALIST( iter_args, n, args );
    return _ypDict( _yp_frozendict_new, iter_args );
}

ypObject *yp_dictK( int n, ... ) {
    if( n == 0 ) return _yp_dict_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_dictKV, (n, args), n );
}
ypObject *yp_dictKV( int n, va_list args ) {
    yp_ONSTACK_ITER_KVALIST( iter_args, n, args );
    return _ypDict( _yp_dict_new, iter_args );
}

// TODO something akin to yp_ONSTACK_ITER_KVALIST that yields the same value with each key
ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... ) {
    if( n < 1 ) return _yp_frozendict_empty;
    return_yp_V_FUNC( ypObject *, yp_frozendict_fromkeysV, (value, n, args), n );
}
ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args ) {
    // TODO Return _yp_frozendict_empty on n<1??
    return yp_NotImplementedError;
}

ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... ) {
    if( n == 0 ) return _yp_dict_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_dict_fromkeysV, (value, n, args), n );
}
ypObject *yp_dict_fromkeysV( ypObject *value, int n, va_list args ) {
    return yp_NotImplementedError;
}

ypObject *yp_frozendict( ypObject *x ) {
    if( ypObject_TYPE_CODE( x ) == ypFrozenDict_CODE ) return yp_incref( x );
    return _ypDict( _yp_frozendict_new, x );
}
ypObject *yp_dict( ypObject *x ) {
    return _ypDict( _yp_dict_new, x );
}


/*************************************************************************************************
 * Common object methods
 *************************************************************************************************/

// These are the functions that simply redirect to object methods; more complex public functions
// are found elsewhere.

// TODO do/while(0)

// args must be surrounded in brackets, to form the function call; as such, must also include ob
// TODO _return_yp_REDIRECT, etc
#define _yp_REDIRECT1( ob, tp_meth, args ) \
    do {ypTypeObject *type = ypObject_TYPE( ob ); \
        return type->tp_meth args; } while( 0 )

#define _yp_REDIRECT2( ob, tp_suite, suite_meth, args ) \
    do {ypTypeObject *type = ypObject_TYPE( ob ); \
        return type->tp_suite->suite_meth args; } while( 0 )

#define _yp_INPLACE1( pOb, tp_meth, args ) \
    do {ypTypeObject *type = ypObject_TYPE( *pOb ); \
        ypObject *result = type->tp_meth args; \
        if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( pOb, result ); \
        return; } while( 0 )

#define _yp_INPLACE2( pOb, tp_suite, suite_meth, args ) \
    do {ypTypeObject *type = ypObject_TYPE( *pOb ); \
        ypObject *result = type->tp_suite->suite_meth args; \
        if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( pOb, result ); \
        return; } while( 0 )

#define _yp_INPLACE_RETURN1( pOb, tp_meth, args ) \
    do {ypTypeObject *type = ypObject_TYPE( *pOb ); \
        ypObject *result = type->tp_meth args; \
        if( yp_isexceptionC( result ) ) yp_INPLACE_ERR( pOb, result ); \
        return result; } while( 0 )

#define _yp_INPLACE_RETURN2( pOb, tp_suite, suite_meth, args ) \
    do {ypTypeObject *type = ypObject_TYPE( *pOb ); \
        ypObject *result = type->tp_suite->suite_meth args; \
        if( yp_isexceptionC( result ) ) yp_INPLACE_ERR( pOb, result ); \
        return result; } while( 0 )

ypObject *yp_bool( ypObject *x ) {
    _yp_REDIRECT1( x, tp_bool, (x) );
    // TODO Ensure the result is yp_True, yp_False, or an exception
}

ypObject *yp_iter( ypObject *x ) {
    _yp_REDIRECT1( x, tp_iter, (x) );
    // TODO Ensure the result is an iterator or an exception
}

ypObject *yp_send( ypObject *iterator, ypObject *value ) {
    _yp_REDIRECT1( iterator, tp_send, (iterator, value) );
    // TODO should we be discarding *iterator?
}

ypObject *yp_next( ypObject *iterator ) {
    _yp_REDIRECT1( iterator, tp_send, (iterator, yp_None) );
    // TODO should we be discarding *iterator?
}

ypObject *yp_throw( ypObject *iterator, ypObject *exc ) {
    if( !yp_isexceptionC( exc ) ) return yp_TypeError;
    _yp_REDIRECT1( iterator, tp_send, (iterator, exc) );
    // TODO should we be discarding *iterator?
}

ypObject *yp_contains( ypObject *container, ypObject *x ) {
    _yp_REDIRECT1( container, tp_contains, (container, x) );
}
ypObject *yp_in( ypObject *x, ypObject *container ) {
    _yp_REDIRECT1( container, tp_contains, (container, x) );
}

ypObject *yp_not_in( ypObject *x, ypObject *container ) {
    ypObject *result = yp_in( x, container );
    return ypBool_NOT( result );
}

void yp_push( ypObject **container, ypObject *x ) {
    _yp_INPLACE1( container, tp_push, (*container, x) );
}

void yp_clear( ypObject **container ) {
    _yp_INPLACE1( container, tp_clear, (*container) );
}

ypObject *yp_pop( ypObject **container ) {
    _yp_INPLACE_RETURN1( container, tp_pop, (*container) );
}

ypObject *yp_concat( ypObject *sequence, ypObject *x ) {
    return yp_NotImplementedError;
}

ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor ) {
    return yp_NotImplementedError;
}

ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i ) {
    _yp_REDIRECT2( sequence, tp_as_sequence, tp_getindex, (sequence, i) );
}

ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k ) {
    return yp_NotImplementedError;
}

yp_ssize_t yp_findC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc ) {
    return_yp_CEXC_ERR( -1, exc, yp_NotImplementedError );
}

yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_findC4( sequence, x, 0, yp_SLICE_USELEN, exc );
}

yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc ) {
    ypObject *subexc = yp_None;
    yp_ssize_t result = yp_findC4( sequence, x, i, j, &subexc );
    if( yp_isexceptionC( subexc ) ) return_yp_CEXC_ERR( -1, exc, subexc );
    if( result == -1 ) return_yp_CEXC_ERR( -1, exc, yp_ValueError );
    return result;
}

yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_indexC4( sequence, x, 0, yp_SLICE_USELEN, exc );
}

void yp_setindexC( ypObject **sequence, yp_ssize_t i, ypObject *x ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_setindex, (*sequence, i, x) );
}

void yp_setsliceC5( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k,
        ypObject *x ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_setslice, (*sequence, i, j, k, x) );
}

void yp_delindexC( ypObject **sequence, yp_ssize_t i ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_delindex, (*sequence, i) );
}

void yp_delsliceC4( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_delslice, (*sequence, i, j, k) );
}

void yp_append( ypObject **sequence, ypObject *x ) {
    _yp_INPLACE1( sequence, tp_push, (*sequence, x) );
}

void yp_extend( ypObject **sequence, ypObject *x ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_extend, (*sequence, x) );
}

void yp_irepeatC( ypObject **sequence, yp_ssize_t factor ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_irepeat, (*sequence, factor) );
}

void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_insert, (*sequence, i, x) );
}

ypObject *yp_popindexC( ypObject **sequence, yp_ssize_t i ) {
    _yp_INPLACE_RETURN2( sequence, tp_as_sequence, tp_popindex, (*sequence, i) );
}

void yp_remove( ypObject **sequence, ypObject *x ) {
    _yp_INPLACE1( sequence, tp_remove, (*sequence, x, NULL) );
}

void yp_reverse( ypObject **sequence ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_reverse, (*sequence) );
}

void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_sort, (*sequence, key, reverse) );
}

void yp_sort( ypObject **sequence ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_sort, (*sequence, NULL, yp_False) );
}

ypObject *yp_isdisjoint( ypObject *set, ypObject *x ) {
    _yp_REDIRECT2( set, tp_as_set, tp_isdisjoint, (set, x) );
}

ypObject *yp_issubset( ypObject *set, ypObject *x ) {
    _yp_REDIRECT2( set, tp_as_set, tp_issubset, (set, x) );
}

ypObject *yp_issuperset( ypObject *set, ypObject *x ) {
    _yp_REDIRECT2( set, tp_as_set, tp_issuperset, (set, x) );
}

// XXX Freezing a mutable set is a quick operation, so we redirect the new-object set methods to
// the in-place versions.  Among other things, this helps to avoid duplicating code.
// TODO Verify this assumption
ypObject *yp_unionN( ypObject *set, int n, ... ) {
    if( !ypObject_IS_MUTABLE( set ) && n < 1 ) return yp_incref( set );
    return_yp_V_FUNC( ypObject *, yp_unionV, (set, n, args), n );
}
ypObject *yp_unionV( ypObject *set, int n, va_list args ) {
    ypObject *result = yp_unfrozen_copy( set );
    yp_updateV( &result, n, args );
    if( !ypObject_IS_MUTABLE( set ) ) yp_freeze( &result );
    return result;
}

ypObject *yp_intersectionN( ypObject *set, int n, ... ) {
    if( !ypObject_IS_MUTABLE( set ) && n < 1 ) return yp_incref( set );
    return_yp_V_FUNC( ypObject *, yp_intersectionV, (set, n, args), n );
}
ypObject *yp_intersectionV( ypObject *set, int n, va_list args ) {
    ypObject *result = yp_unfrozen_copy( set );
    yp_intersection_updateV( &result, n, args );
    if( !ypObject_IS_MUTABLE( set ) ) yp_freeze( &result );
    return result;
}

ypObject *yp_differenceN( ypObject *set, int n, ... ) {
    if( !ypObject_IS_MUTABLE( set ) && n < 1 ) return yp_incref( set );
    return_yp_V_FUNC( ypObject *, yp_differenceV, (set, n, args), n );
}
ypObject *yp_differenceV( ypObject *set, int n, va_list args ) {
    ypObject *result = yp_unfrozen_copy( set );
    yp_difference_updateV( &result, n, args );
    if( !ypObject_IS_MUTABLE( set ) ) yp_freeze( &result );
    return result;
}

ypObject *yp_symmetric_difference( ypObject *set, ypObject *x ) {
    ypObject *result = yp_unfrozen_copy( set );
    yp_symmetric_difference_update( &result, x );
    if( !ypObject_IS_MUTABLE( set ) ) yp_freeze( &result );
    return result;
}

void yp_updateN( ypObject **set, int n, ... ) {
    return_yp_V_FUNC_void( yp_updateV, (set, n, args), n );
}
void yp_updateV( ypObject **set, int n, va_list args ) {
    _yp_INPLACE2( set, tp_as_set, tp_update, (*set, n, args) );
}

void yp_intersection_updateN( ypObject **set, int n, ... ) {
    return_yp_V_FUNC_void( yp_intersection_updateV, (set, n, args), n );
}
void yp_intersection_updateV( ypObject **set, int n, va_list args ) {
    _yp_INPLACE2( set, tp_as_set, tp_intersection_update, (*set, n, args) );
}

void yp_difference_updateN( ypObject **set, int n, ... ) {
    return_yp_V_FUNC_void( yp_difference_updateV, (set, n, args), n );
}
void yp_difference_updateV( ypObject **set, int n, va_list args ) {
    _yp_INPLACE2( set, tp_as_set, tp_difference_update, (*set, n, args) );
}

void yp_symmetric_difference_update( ypObject **set, ypObject *x ) {
    _yp_INPLACE2( set, tp_as_set, tp_symmetric_difference_update, (*set, x) );
}

ypObject *yp_pushuniqueE( ypObject *set, ypObject *x ) {
    _yp_REDIRECT2( set, tp_as_set, tp_pushunique, (set, x) );
}

void yp_discard( ypObject **set, ypObject *x ) {
    _yp_INPLACE1( set, tp_remove, (*set, x, yp_None) );
}

ypObject *yp_getitem( ypObject *mapping, ypObject *key ) {
    _yp_REDIRECT1( mapping, tp_getdefault, (mapping, key, NULL) );
}

void yp_setitem( ypObject **mapping, ypObject *key, ypObject *x ) {
    _yp_INPLACE1( mapping, tp_setitem, (*mapping, key, x) );
}

void yp_delitem( ypObject **mapping, ypObject *key ) {
    _yp_INPLACE1( mapping, tp_delitem, (*mapping, key) );
}

ypObject *yp_getdefault( ypObject *mapping, ypObject *key, ypObject *defval ) {
    _yp_REDIRECT1( mapping, tp_getdefault, (mapping, key, defval) );
}

ypObject *yp_iter_items( ypObject *mapping ) {
    _yp_REDIRECT2( mapping, tp_as_mapping, tp_iter_items, (mapping) );
}

ypObject *yp_iter_keys( ypObject *mapping ) {
    _yp_REDIRECT2( mapping, tp_as_mapping, tp_iter_keys, (mapping) );
}

void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value ) {
    // XXX tp_popitem should set *key and *value to exceptions on error
    _yp_INPLACE2( mapping, tp_as_mapping, tp_popitem, (*mapping, key, value) );
}

ypObject *yp_setdefault( ypObject **mapping, ypObject *key, ypObject *defval ) {
    ypTypeObject *type = ypObject_TYPE( *mapping );
    ypObject *result = type->tp_as_mapping->tp_setdefault( *mapping, key, defval );
    if( yp_isexceptionC( result ) ) yp_INPLACE_ERR( mapping, result );
    return result;
}

void yp_updateK( ypObject **mapping, int n, ... ) {
    return_yp_K_FUNC_void( yp_updateKV, (mapping, n, args), n );
}
void yp_updateKV( ypObject **mapping, int n, va_list args ) {
    yp_decref( *mapping );
    *mapping = yp_NotImplementedError;
    //_yp_INPLACE2( mapping, tp_as_mapping, tp_updateK, (*mapping, n, args) );
}

ypObject *yp_iter_values( ypObject *mapping ) {
    _yp_REDIRECT2( mapping, tp_as_mapping, tp_iter_values, (mapping) );
}

ypObject *yp_miniiter( ypObject *x, yp_uint64_t *state ) {
    _yp_REDIRECT1( x, tp_miniiter, (x, state) );
}

ypObject *yp_miniiter_next( ypObject *mi, yp_uint64_t *state ) {
    _yp_REDIRECT1( mi, tp_miniiter_next, (mi, state) );
}

yp_ssize_t yp_miniiter_lenhintC( ypObject *mi, yp_uint64_t *state, ypObject **exc ) {
    yp_ssize_t lenhint = 0;
    ypObject *result = ypObject_TYPE( mi )->tp_miniiter_lenhint( mi, state, &lenhint );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( 0, exc, result );
    return lenhint < 0 ? 0 : lenhint;
}


/*************************************************************************************************
 * The type table
 *************************************************************************************************/
// XXX Make sure this corresponds with ypInvalidated_CODE et al!

// Recall that C helpfully sets missing array elements to NULL
static ypTypeObject *ypTypeTable[255] = {
    &ypInvalidated_Type,/* ypInvalidated_CODE          (  0u) */
    &ypInvalidated_Type,/*                             (  1u) */
    &ypException_Type,  /* ypException_CODE            (  2u) */
    &ypException_Type,  /*                             (  3u) */
    NULL,               /* ypType_CODE                 (  4u) */
    NULL,               /*                             (  5u) */

    &ypNoneType_Type,   /* ypNoneType_CODE             (  6u) */
    &ypNoneType_Type,   /*                             (  7u) */
    &ypBool_Type,       /* ypBool_CODE                 (  8u) */
    &ypBool_Type,       /*                             (  9u) */

    &ypInt_Type,        /* ypInt_CODE                  ( 10u) */
    &ypIntStore_Type,   /* ypIntStore_CODE             ( 11u) */
    NULL,               /* ypFloat_CODE                ( 12u) */
    NULL,               /* ypFloatStore_CODE           ( 13u) */

    &ypIter_Type,       /* ypFrozenIter_CODE           ( 14u) */
    &ypIter_Type,       /* ypIter_CODE                 ( 15u) */

    &ypBytes_Type,      /* ypBytes_CODE                ( 16u) */
    &ypByteArray_Type,  /* ypByteArray_CODE            ( 17u) */
    &ypStr_Type,        /* ypStr_CODE                  ( 18u) */
    &ypChrArray_Type,   /* ypChrArray_CODE             ( 19u) */
    &ypTuple_Type,      /* ypTuple_CODE                ( 20u) */
    &ypList_Type,       /* ypList_CODE                 ( 21u) */

    &ypFrozenSet_Type,  /* ypFrozenSet_CODE            ( 22u) */
    &ypSet_Type,        /* ypSet_CODE                  ( 23u) */

    &ypFrozenDict_Type, /* ypFrozenDict_CODE           ( 24u) */
    &ypDict_Type,       /* ypDict_CODE                 ( 25u) */
};


/*************************************************************************************************
 * Initialization
 *************************************************************************************************/

// TODO Make this accept configuration values from the user in a structure
void yp_initialize( void )
{
#ifdef _MSC_VER
    // TODO memory leak detection that should only be enabled for debug builds
    // _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    yp_malloc = _default_yp_malloc;
    yp_malloc_resize = _default_yp_malloc_resize;
    yp_free = _default_yp_free;

    // TODO Config param idea: "minimum" or "average" or "usual" or "preferred" allocation
    // size...something that indicates that malloc handles these sizes particularly well.  The
    // number should be small, like 64 bytes or something.  This number can be used to decide how
    // much data to put in-line.  The call to malloc would use exactly this size when creating an
    // object, and at least this size when allocating extra data.  This makes all objects the same
    // size, which will help malloc avoid fragmentation.  It also recognizes the fact that each
    // malloc has a certain overhead in memory, so might as well allocate a certain amount to
    // compensate.  When invalidating an object, the extra data is freed, but the invalidated
    // object that remains would sit in memory with this size until fully freed, so the extra data
    // is wasted until then, which is why the value should be small.  (Actually, not all objects
    // will be this size, as int/float, when they become small objects, will only allocate a
    // fraction of this.)
}

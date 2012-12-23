/*
 * nohtyP.c - A Python-like API for C, in one .c and one .h
 *      http://nohtyp.wordpress.com
 *      Copyright © 2001-2012 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/py3k/license.html
 */

#include "nohtyP.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>


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
#define yp_TYPE_PAIR_CODE( ob ) \
    ypObject_TYPE_CODE_AS_FROZEN( ypObject_TYPE_CODE( ob ) )

// TODO Need two types of immortals: statically-allocated immortals (so should never be
// freed/invalidated) and overly-incref'd immortals (should be allowed to be invalidated and thus
// free any extra data, although the object itself will never be free'd as we've lost track of the
// refcounts)
#define ypObject_REFCNT_IMMORTAL _ypObject_REFCNT_IMMORTAL

// When a hash of this value is stored in ob_hash, call tp_hash
#define ypObject_HASH_INVALID _ypObject_HASH_INVALID

// Signals an invalid length stored in ob_len (so call tp_len) or ob_alloclen
#define ypObject_LEN_INVALID        _ypObject_LEN_INVALID
#define ypObject_ALLOCLEN_INVALID   _ypObject_ALLOCLEN_INVALID

// Lengths and hashes can be cached in the object for easy retrieval
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
typedef ypObject *(*hashfunc)( ypObject *, yp_hash_t * );
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
    objproc tp_iter_items;
    objproc tp_iter_keys;
    objobjobjproc tp_popvalue;
    popitemfunc tp_popitem;
    objobjobjproc tp_setdefault;
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
    objproc tp_iter;
    objproc tp_iter_reversed;
    objobjproc tp_send;

    // Container operations
    objobjproc tp_contains;
    lenfunc tp_length;
    objobjproc tp_push;
    objproc tp_clear; /* delete references to contained objects */
    objproc tp_pop;
    objobjproc tp_remove; /* TODO some indication that error is due to missing item, to supress on discard */
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

#define ypNone_CODE                 (  6u)
// no mutable ypNone type           (  7u)
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
#define ypCharacterArray_CODE       ( 19u)
#define ypTuple_CODE                ( 20u)
#define ypList_CODE                 ( 21u)

#define ypFrozenSet_CODE            ( 22u)
#define ypSet_CODE                  ( 23u)

#define ypFrozenDict_CODE           ( 24u)
#define ypDict_CODE                 ( 25u)

yp_STATIC_ASSERT( _ypBytes_CODE == ypBytes_CODE, ypBytes_CODE );


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
    static ypObject *name ## _hashfunc( ypObject *x, yp_hash_t *hash ) { return retval; } \
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
        *name ## _objproc, \
        *name ## _objproc, \
        *name ## _objobjobjproc, \
        *name ## _popitemfunc, \
        *name ## _objobjobjproc, \
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

// Functions that modify their inputs take a "ypObject **x"; use this as
// "return_yp_INPLACE_ERR( x, yp_ValueError );" to return the error properly
#define return_yp_INPLACE_ERR( ob, err ) \
    do { yp_decref( *(ob) ); *(ob) = (err); return; } while( 0 )

// Functions that return C values take a "ypObject **exc" that are only modified on error and are
// not discarded beforehand; they also need to return a valid C value
#define return_yp_CEXC_ERR( retval, exc, err ) \
    do { *(exc) = (err); return retval; } while( 0 )

// When an object encounters an unknown type, there are three possible cases:
//  - it's an invalidated object, so return yp_InvalidatedError
//  - it's an exception, so return it
//  - it's some other type, so return yp_TypeError
#define yp_BAD_TYPE( bad_ob ) ( \
    yp_TYPE_PAIR_CODE( bad_ob ) == ypInvalidated_CODE ? \
        yp_InvalidatedError : \
    yp_TYPE_PAIR_CODE( bad_ob ) == ypException_CODE ? \
        (bad_ob) : \
    /* else */ \
        yp_TypeError )
#define return_yp_BAD_TYPE( bad_ob ) \
    do { return yp_BAD_TYPE( bad_ob ); } while( 0 )
#define return_yp_INPLACE_BAD_TYPE( ob, bad_ob ) \
    do { return_yp_INPLACE_ERR( (ob), yp_BAD_TYPE( bad_ob ) ); } while( 0 )
#define return_yp_CEXC_BAD_TYPE( retval, exc, bad_ob ) \
    do { return_yp_CEXC_ERR( (retval), (exc), yp_BAD_TYPE( bad_ob ) ); } while( 0 )

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

// Prime multiplier used in string and various other hashes
#define _ypHASH_MULTIPLIER 1000003  // 0xf4243

// Return the hash of the given number of bytes; always succeeds
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
    if (x == ypObject_HASH_INVALID) x = ypObject_HASH_INVALID-1;
    return x;
}


/*************************************************************************************************
 * nohtyP memory allocations
 *************************************************************************************************/

// Dummy memory allocators to ensure that yp_init is called before anything else
static void *_yp_dummy_malloc( yp_ssize_t size ) { return NULL; }
static void *_yp_dummy_realloc( void *p, yp_ssize_t size ) { return NULL; }
// TODO Windows has an _expand function; can use this to shrink invalidated objects in-place; the
// dummy version can just return the pointer unchanged; this will only be useful in the in-place
// data is large...which it might not be
static void _yp_dummy_free( void *p ) { }

// Default versions of the allocators, using C's malloc/realloc/free
static void *_yp_malloc( yp_ssize_t size ) { return malloc( MAX( size, 1 ) ); }
static void *_yp_realloc( void *p, yp_ssize_t size ) { return realloc( p, MAX( size, 1 ) ); }
#define _yp_free  free

// Allows the allocation functions to be overridden by yp_init
static void *(*yp_malloc)( yp_ssize_t ) = _yp_dummy_malloc;
static void *(*yp_realloc)( void *, yp_ssize_t ) = _yp_dummy_realloc;
static void (*yp_free)( void * ) = _yp_dummy_free;

// Declares the ob_inline_data array for container object structures
#define yp_INLINE_DATA _yp_INLINE_DATA

// Sets ob to a malloc'd buffer for fixed, non-container objects, or yp_MemoryError on failure
#define ypMem_MALLOC_FIXED( ob, obStruct, type ) \
    do { \
        (ob) = (ypObject *) yp_malloc( sizeof( obStruct ) ); \
        if( (ob) == NULL ) { (ob) = yp_MemoryError; break; }; \
        (ob)->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 ); \
        (ob)->ob_hash = ypObject_HASH_INVALID; \
        (ob)->ob_len = ypObject_LEN_INVALID; \
        (ob)->ob_alloclen = ypObject_ALLOCLEN_INVALID; \
        (ob)->ob_data = NULL; \
    } while( 0 )

// TODO check that alloclen isn't >= ALOCLEN_INVALID (or negative?)

// Sets ob to a malloc'd buffer for an immutable container object holding alloclen elements, or
// yp_MemoryError on failure.  ob_inline_data in obStruct is used to determine the element size
// and ob_data; ob_len is set to zero.  alloclen cannot be negative.
#define ypMem_MALLOC_CONTAINER_INLINE( ob, obStruct, type, alloclen ) \
    do { \
        (ob) = (ypObject *) yp_malloc( sizeof( obStruct )/*includes one element*/ + \
                ((alloclen-1) * yp_sizeof_member( obStruct, ob_inline_data[0] )) ); \
        if( (ob) == NULL ) { (ob) = yp_MemoryError; break; }; \
        (ob)->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 ); \
        (ob)->ob_hash = ypObject_HASH_INVALID; \
        (ob)->ob_len = 0; \
        (ob)->ob_alloclen = alloclen; \
        (ob)->ob_data = ((obStruct *)(ob))->ob_inline_data; \
    } while( 0 )

// Sets ob to a malloc'd buffer for a mutable container currently holding alloclen elements, or
// yp_MemoryError on failure.  ob_inline_data in obStruct is used to determine the element size;
// ob_len is set to zero.  alloclen cannot be negative.
// TODO Allocate a set number of elements in-line, regardless of alloclen; similar to Python's
// smalltable stuff in sets, etc, it avoids an extra malloc for small sets of data; this should be
// a parameter to this macro
// TODO But it's up to the types to decide if/when to over-allocate, and if over-allocating need to
// make sure to use the in-line buffer when possible
// TODO Does Python over-allocate like this?
#define ypMem_MALLOC_CONTAINER_VARIABLE( ob, obStruct, type, alloclen ) \
    do { \
        (ob) = (ypObject *) yp_malloc( sizeof( obStruct )/*includes one element*/ - \
                yp_sizeof_member( obStruct, ob_inline_data[0] ) ); \
        if( (ob) == NULL ) { (ob) = yp_MemoryError; break; }; \
        (ob)->ob_data = yp_malloc( \
                (alloclen) * yp_sizeof_member( obStruct, ob_inline_data[0] ) ); \
        if( (ob)->ob_data == NULL ) { free( ob ); (ob) = yp_MemoryError; break; } \
        (ob)->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 ); \
        (ob)->ob_hash = ypObject_HASH_INVALID; \
        (ob)->ob_len = 0; \
        (ob)->ob_alloclen = alloclen; \
    } while( 0 )

// Resizes ob_data, the variable-portion of ob, to the given alloclen, and sets result to yp_None.
// If realloc fails, result is set to yp_MemoryError and ob is not modified.  ob_inline_data in
// obStruct is used to determine the element size; ob_alloclen is set to newAlloclen.
// Any objects in the  truncated section must have already been discarded.  newAlloclen cannot be
// negative.
// TODO If newAlloclen is <= the amount pre-allocated in-line by ypMem_MALLOC_CONTAINER_VARIABLE,
// then use it instead of holding on to the separately-allocated buffer; the amount needs to be a
// parameter to this macro, and it must be the same value as used for MALLOC
// TODO The types need to be responsible for not using this macro if not required (ie checking the
// current alloclen)
#define ypMem_REALLOC_CONTAINER_VARIABLE( result, ob, obStruct, newAlloclen ) \
    do { \
        void *newData = yp_realloc( (ob)->ob_data, \
                (newAlloclen) * yp_sizeof_member( obStruct, ob_inline_data[0] ) ); \
        if( newData == NULL ) { (result) = yp_MemoryError; break; }; \
        (result) = yp_None; \
        (ob)->ob_data = newData; \
        (ob)->ob_alloclen = newAlloclen; \
    } while( 0 )

#define ypMem_FREE_FIXED yp_free

#define ypMem_FREE_CONTAINER( ob, obStruct ) \
    do { \
        if( (ob)->ob_data != ((obStruct *)(ob))->ob_inline_data ) yp_free( (ob)->ob_data ); \
        yp_free( ob ); \
    } while( 0 )


/*************************************************************************************************
 * Object fundamentals
 *************************************************************************************************/

ypObject *yp_incref( ypObject *x )
{
    yp_uint32_t refcnt = ypObject_REFCNT( x );
    if( refcnt >= ypObject_REFCNT_IMMORTAL ) return x; // no-op
    x->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( ypObject_TYPE_CODE( x ), refcnt+1 );
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
        ypObject_TYPE( x )->tp_dealloc( x );
    } else {
        x->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( ypObject_TYPE_CODE( x ), refcnt-1 );
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
    ypObject *result;

    // Check if it's already frozen (no-op) or if it can't be frozen (error)
    if( oldCode == newCode ) return yp_None;
    newType = ypTypeTable[newCode];
    if( newType == NULL ) return yp_TypeError;  // TODO make this never happen: such objects should be closed (or invalidated?) instead

    // Freeze the object, cache the final hash, and possibly reduce memory usage, etc
    ypObject_SET_TYPE_CODE( x, newCode );
    x->ob_hash = _ypObject_HASH_INVALID;
    result = newType->tp_currenthash( x, &x->ob_hash );
    return newType->tp_freeze( x );
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

    // Avoid recursion
    id = yp_intC( (yp_int64_t) x );
    result = yp_pushuniqueE( &memo, id );
    yp_decref( id );
    if( yp_isexceptionC( result ) ) {
        if( result == yp_KeyError ) return yp_None; // already in set
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
static ypObject *_yp_shallowcopy_visitor( ypObject *x, void *memo ) {
    return yp_incref( x );
}

ypObject *yp_unfrozen_copy( ypObject *x ) {
    return ypObject_TYPE( x )->tp_unfrozen_copy( x, _yp_shallowcopy_visitor, NULL );
}

static ypObject *_yp_unfrozen_deepcopy( ypObject *x, void *memo ) {
    // TODO
    // TODO don't forget to discard the new objects on error
    return yp_NotImplementedError;
}

ypObject *yp_unfrozen_deepcopy( ypObject *x ) {
    ypObject *memo = yp_dictK( 0 );
    ypObject *result = _yp_unfrozen_deepcopy( x, memo );
    yp_decref( memo );
    return result;
}

ypObject *yp_frozen_copy( ypObject *x ) {
    return ypObject_TYPE( x )->tp_frozen_copy( x, _yp_shallowcopy_visitor, NULL );
}

static ypObject *_yp_frozen_deepcopy( ypObject *x, void *memo ) {
    // TODO
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

static ypObject *_yp_deepcopy( ypObject *x, void *memo ) {
    // TODO
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


// XXX yp_ComparisonNotImplemented should _never_ be seen outside of comparison functions
ypObject *yp_ComparisonNotImplemented;
#define _ypBool_PUBLIC_CMP_FUNCTION( name, reflection ) \
ypObject *yp_ ## name( ypObject *x, ypObject *y ) { \
    ypTypeObject *type = ypObject_TYPE( x ); \
    ypObject *result = type->tp_ ## name( x, y ); \
    if( result != yp_ComparisonNotImplemented ) return result; \
    type = ypObject_TYPE( y ); \
    result = type->tp_ ## reflection( y, x ); \
    if( result != yp_ComparisonNotImplemented ) return result; \
    return yp_TypeError; \
}
_ypBool_PUBLIC_CMP_FUNCTION( lt, gt );
_ypBool_PUBLIC_CMP_FUNCTION( le, ge );
_ypBool_PUBLIC_CMP_FUNCTION( eq, eq );
_ypBool_PUBLIC_CMP_FUNCTION( ne, ne );
_ypBool_PUBLIC_CMP_FUNCTION( ge, le );
_ypBool_PUBLIC_CMP_FUNCTION( gt, lt );
// TODO #undef _ypBool_PUBLIC_CMP_FUNCTION

yp_STATIC_ASSERT( ypObject_HASH_INVALID == -1, hash_invalid_is_neg_one );
yp_hash_t yp_hashC( ypObject *x, ypObject **exc )
{
    if( ypObject_IS_MUTABLE( x ) ) return_yp_CEXC_BAD_TYPE( ypObject_HASH_INVALID, exc, x );
    return yp_currenthashC( x, exc );
}
yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc )
{
    yp_hash_t hash = ypObject_CACHED_HASH( x );
    ypObject *result;

    if( !ypObject_IS_MUTABLE( x ) && hash != ypObject_HASH_INVALID ) {
        return hash;
    }
    result = ypObject_TYPE( x )->tp_currenthash( x, &hash );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( ypObject_HASH_INVALID, exc, result );
    if( !ypObject_IS_MUTABLE( x ) ) ypObject_CACHED_HASH( x ) = hash;
    return hash;
}

yp_ssize_t yp_lenC( ypObject *x, ypObject **exc )
{
    yp_ssize_t len = ypObject_CACHED_LEN( x );
    ypObject *result;

    if( len >= 0 ) return len;
    result = ypObject_TYPE( x )->tp_length( x, &len );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( 0, exc, result );
    if( len < 0 ) return_yp_CEXC_ERR( 0, exc, yp_SystemError ); // tp_length should not return <0
    return len;
}


/*************************************************************************************************
 * Common object methods
 *************************************************************************************************/

// These are the functions that simply redirect to object methods; more complex public functions
// are found elsewhere.

// TODO do/while(0)

// args must be surrounded in brackets, to form the function call; as such, must also include ob
#define _yp_REDIRECT1( ob, tp_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( ob ); \
    return type->tp_meth args;

#define _yp_REDIRECT2( ob, tp_suite, suite_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( ob ); \
    return type->tp_suite->suite_meth args;

#define _yp_INPLACE1( pOb, tp_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( *pOb ); \
    ypObject *result = type->tp_meth args; \
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( pOb, result ); \
    return;

#define _yp_INPLACE2( pOb, tp_suite, suite_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( *pOb ); \
    ypObject *result = type->tp_suite->suite_meth args; \
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( pOb, result ); \
    return;

ypObject *yp_bool( ypObject *x ) {
    _yp_REDIRECT1( x, tp_bool, (x) );
    // TODO Ensure the result is yp_True, yp_False, or an exception
}

ypObject *yp_iter( ypObject *x ) {
    _yp_REDIRECT1( x, tp_iter, (x) );
    // TODO Ensure the result is an iterator or an exception
}

ypObject *yp_send( ypObject **iterator, ypObject *value ) {
    _yp_REDIRECT1( *iterator, tp_send, (*iterator, value) );
    // TODO should we be discarding *iterator?
}

ypObject *yp_next( ypObject **iterator ) {
    _yp_REDIRECT1( *iterator, tp_send, (*iterator, yp_None) );
    // TODO should we be discarding *iterator?
}

ypObject *yp_throw( ypObject **iterator, ypObject *exc ) {
    if( !yp_isexceptionC( exc ) ) return yp_TypeError;
    {_yp_REDIRECT1( *iterator, tp_send, (*iterator, exc) );}
    // TODO should we be discarding *iterator?
}

// TODO for this and other methods that mutate, check first if it's immutable
void yp_push( ypObject **sequence, ypObject *x ) {
    _yp_INPLACE1( sequence, tp_push, (*sequence, x) )
}

ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i ) {
    _yp_REDIRECT2( sequence, tp_as_sequence, tp_getindex, (sequence, i) );
}

void yp_append( ypObject **sequence, ypObject *x ) {
    _yp_INPLACE1( sequence, tp_push, (*sequence, x) )
}

void yp_extend( ypObject **sequence, ypObject *x ) {
    _yp_INPLACE2( sequence, tp_as_sequence, tp_extend, (*sequence, x) )
}

ypObject *yp_pushuniqueE( ypObject **set, ypObject *x ) {
    _yp_REDIRECT2( *set, tp_as_set, tp_pushunique, (*set, x) );
}

// TODO undef necessary stuff


/*************************************************************************************************
 * Invalidated Objects
 *************************************************************************************************/



/*************************************************************************************************
 * Exceptions
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct {
    ypObject_HEAD
    ypObject *ob_name;
} ypExceptionObject;
#define _ypException_NAME( e ) ( ((ypExceptionObject *)e)->ob_name )

static ypTypeObject ypException_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                               // tp_name

    // Object fundamentals
    NULL,                               // tp_dealloc
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
    ExceptionMethod_objproc,            // tp_iter
    ExceptionMethod_objproc,            // tp_iter_reversed
    ExceptionMethod_objobjproc,         // tp_send

    // Container operations
    ExceptionMethod_objobjproc,         // tp_contains
    ExceptionMethod_lenfunc,            // tp_length
    ExceptionMethod_objobjproc,         // tp_push
    ExceptionMethod_objproc,            // tp_clear
    ExceptionMethod_objproc,            // tp_pop
    ExceptionMethod_objobjproc,         // tp_remove
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

// The immortal exception objects
// TODO implement an exception heirarchy one day
// TODO replace use of yp_IMMORTAL_BYTES with proper string object
#define _yp_IMMORTAL_EXCEPTION( name ) \
    yp_IMMORTAL_BYTES( name ## _name, #name ); \
    static ypExceptionObject _ ## name ## _struct = { _yp_IMMORTAL_HEAD_INIT( \
        ypException_CODE, NULL, 0 ), (ypObject *) &_ ## name ## _name_struct }; \
    ypObject *name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */

_yp_IMMORTAL_EXCEPTION( yp_BaseException );
_yp_IMMORTAL_EXCEPTION( yp_Exception );
_yp_IMMORTAL_EXCEPTION( yp_StopIteration );
_yp_IMMORTAL_EXCEPTION( yp_GeneratorExit );
_yp_IMMORTAL_EXCEPTION( yp_ArithmeticError );
_yp_IMMORTAL_EXCEPTION( yp_LookupError );

_yp_IMMORTAL_EXCEPTION( yp_AssertionError );
_yp_IMMORTAL_EXCEPTION( yp_AttributeError );
_yp_IMMORTAL_EXCEPTION( yp_MethodError ); // "subclass" of yp_AttributeError
_yp_IMMORTAL_EXCEPTION( yp_EOFError );
_yp_IMMORTAL_EXCEPTION( yp_FloatingPointError );
_yp_IMMORTAL_EXCEPTION( yp_EnvironmentError );
_yp_IMMORTAL_EXCEPTION( yp_IOError );
_yp_IMMORTAL_EXCEPTION( yp_OSError );
_yp_IMMORTAL_EXCEPTION( yp_ImportError );
_yp_IMMORTAL_EXCEPTION( yp_IndexError );
_yp_IMMORTAL_EXCEPTION( yp_KeyError );
_yp_IMMORTAL_EXCEPTION( yp_KeyboardInterrupt );
_yp_IMMORTAL_EXCEPTION( yp_MemoryError );
_yp_IMMORTAL_EXCEPTION( yp_NameError );
_yp_IMMORTAL_EXCEPTION( yp_OverflowError );
_yp_IMMORTAL_EXCEPTION( yp_RuntimeError );
_yp_IMMORTAL_EXCEPTION( yp_NotImplementedError );
_yp_IMMORTAL_EXCEPTION( yp_ComparisonNotImplemented ); // "subclass" of yp_NotImplementedError
_yp_IMMORTAL_EXCEPTION( yp_SyntaxError );
_yp_IMMORTAL_EXCEPTION( yp_IndentationError );
_yp_IMMORTAL_EXCEPTION( yp_TabError );
_yp_IMMORTAL_EXCEPTION( yp_ReferenceError );
_yp_IMMORTAL_EXCEPTION( yp_SystemError );
_yp_IMMORTAL_EXCEPTION( yp_SystemExit );
_yp_IMMORTAL_EXCEPTION( yp_TypeError );
_yp_IMMORTAL_EXCEPTION( yp_InvalidatedError ); // "subclass" of yp_TypeError
_yp_IMMORTAL_EXCEPTION( yp_UnboundLocalError );
_yp_IMMORTAL_EXCEPTION( yp_UnicodeError );
_yp_IMMORTAL_EXCEPTION( yp_UnicodeEncodeError );
_yp_IMMORTAL_EXCEPTION( yp_UnicodeDecodeError );
_yp_IMMORTAL_EXCEPTION( yp_UnicodeTranslateError );
_yp_IMMORTAL_EXCEPTION( yp_ValueError );
_yp_IMMORTAL_EXCEPTION( yp_ZeroDivisionError );
_yp_IMMORTAL_EXCEPTION( yp_BufferError );
_yp_IMMORTAL_EXCEPTION( yp_RecursionErrorInst );
// TODO #undef _yp_IMMORTAL_EXCEPTION

// TODO make this a macro, also, for use internally
int yp_isexceptionC( ypObject *x )
{
    return yp_TYPE_PAIR_CODE( x ) == ypException_CODE;
}


/*************************************************************************************************
 * None
 *************************************************************************************************/

// FIXME this needs to happen


/*************************************************************************************************
 * Bools
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

// Returns 1 if the bool object is true, else 0; only valid on bool objects!  The return can also
// be interpreted as the value of the boolean.
// XXX This assumes that yp_True and yp_False are the only two bools
#define ypBool_IS_TRUE_C( b ) ( (b) == yp_True )

#define ypBool_FROM_C( cond ) ( (cond) ? yp_True : yp_False )

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct {
    ypObject_HEAD
    char ob_value;
} ypBoolObject;
#define _ypBool_VALUE( b ) ( ((ypBoolObject *)b)->ob_value )

// TODO methods here

// No constructors for bools; there are exactly two objects, and they are immortal

// There are exactly two bool objects
ypBoolObject _yp_True_struct = {yp_IMMORTAL_HEAD_INIT( ypBool_CODE, NULL, 0 ), 1};
ypObject *yp_True = (ypObject *) &_yp_True_struct;
ypBoolObject _yp_False_struct = {yp_IMMORTAL_HEAD_INIT( ypBool_CODE, NULL, 0 ), 0};
ypObject *yp_False = (ypObject *) &_yp_False_struct;


/*************************************************************************************************
 * Integers
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?
typedef struct {
    ypObject_HEAD
    yp_int_t ob_value;
} ypIntObject;
#define ypInt_VALUE( i ) ( ((ypIntObject *)i)->ob_value )

// Arithmetic code depends on both int and float particulars being defined first
typedef struct {
    ypObject_HEAD
    yp_float_t ob_value;
} ypFloatObject;
#define ypFloat_VALUE( f ) ( ((ypFloatObject *)f)->ob_value )


static ypObject *int_dealloc( ypObject *i ) {
    ypMem_FREE_FIXED( i );
    return yp_None;
}

static ypObject *int_bool( ypObject *i ) {
    return ypBool_FROM_C( ypInt_VALUE( i ) );
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
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_length
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjproc,         // tp_remove
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
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_length
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjproc,         // tp_remove
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
    int x_type = yp_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;

    if( x_type == ypInt_CODE ) {
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

    } else if( x_type == ypFloat_CODE ) {
        yp_float_t y_asfloat = yp_asfloatL( y, &exc ); // TODO
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_iaddFC( x, y_asfloat );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, *x );
}

void yp_iadd( ypObject **x, ypObject *y )
{
    int y_type = yp_TYPE_PAIR_CODE( y );
    ypObject *exc = yp_None;

    if( y_type == ypInt_CODE ) {
        yp_iaddC( x, ypInt_VALUE( y ) );
        return;

    } else if( y_type == ypFloat_CODE ) {
        yp_iaddFC( x, ypFloat_VALUE( y ) );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, y );
}

ypObject *yp_add( ypObject *x, ypObject *y )
{
    int x_type = yp_TYPE_PAIR_CODE( x );
    int y_type = yp_TYPE_PAIR_CODE( y );
    ypObject *result;

    if( x_type != ypInt_CODE && x_type != ypFloat_CODE ) return_yp_BAD_TYPE( x );
    if( y_type != ypInt_CODE && y_type != ypFloat_CODE ) return_yp_BAD_TYPE( y );

    // All numbers hold their data in-line, so freezing a mutable is not heap-inefficient
    result = yp_unfrozen_copy( x );
    yp_iadd( &result, y );
    if( !ypObject_IS_MUTABLE( x ) ) yp_freeze( &result );
    return result;
}

ypObject *yp_intC( yp_int_t value )
{
    ypObject *i;
    ypMem_MALLOC_FIXED( i, ypIntObject, ypInt_CODE );
    if( yp_isexceptionC( i ) ) return i;
    ypInt_VALUE( i ) = value;
    return i;
}

yp_int_t yp_asintC( ypObject *x, ypObject **exc )
{
    int x_type = yp_TYPE_PAIR_CODE( x );

    if( x_type == ypInt_CODE ) {
        return ypInt_VALUE( x );
    } else if( x_type == ypFloat_CODE ) {
        return yp_asintFL( ypFloat_VALUE( x ), exc );
    }
    return_yp_CEXC_BAD_TYPE( 0, exc, x );
}

// Defines the conversion functions.  Overflow checking is done by first truncating the value then
// seeing if it equals the stored value.  Note that when yp_asintC raises an exception, it returns
// zero, which can be represented in every integer type, so we won't override any yp_TypeError 
// errors.
#define _ypInt_PUBLIC_AS_C_FUNCTION( name ) \
yp_ ## name ## _t yp_as ## name ## C( ypObject *x, ypObject **exc ) { \
    yp_int_t asint = yp_asintC( x, exc ); \
    yp_ ## name ## _t retval = (yp_ ## name ## _t) asint; \
    if( retval != asint ) return_yp_CEXC_ERR( retval, exc, yp_OverflowError ); \
    return retval; \
}
_ypInt_PUBLIC_AS_C_FUNCTION( int8 );
_ypInt_PUBLIC_AS_C_FUNCTION( uint8 );
_ypInt_PUBLIC_AS_C_FUNCTION( int16 );
_ypInt_PUBLIC_AS_C_FUNCTION( uint16 );
_ypInt_PUBLIC_AS_C_FUNCTION( int32 );
_ypInt_PUBLIC_AS_C_FUNCTION( uint32 );
// TODO #undef _ypInt_PUBLIC_AS_C_FUNCTION

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
    int x_type = yp_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;
    yp_float_t result;

    if( x_type == ypFloat_CODE ) {
        result = yp_addFL( ypFloat_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypFloat_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_floatC( result );
        }
        return;

    } else if( x_type == ypInt_CODE ) {
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
    ypObject *f;
    ypMem_MALLOC_FIXED( f, ypFloatObject, ypFloat_CODE );
    if( yp_isexceptionC( f ) ) return f;
    ypFloat_VALUE( f ) = value;
    return f;
}

yp_float_t yp_asfloatC( ypObject *x, ypObject **exc )
{
    int x_type = yp_TYPE_PAIR_CODE( x );

    if( x_type == ypInt_CODE ) {
        return yp_asfloatL( ypInt_VALUE( x ), exc );
    } else if( x_type == ypFloat_CODE ) {
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
 * Iterators
 *************************************************************************************************/

// TODO Iterators should have a lenhint "attribute" so that consumers of the iterator can
// pre-allocate; this should be automatically decremented with every yielded value

// _ypIterObject_HEAD shared with ypIterValistObject below
#define _ypIterObject_HEAD \
    ypObject_HEAD \
    yp_generator_func_t ob_func; \
    yp_ssize_t ob_lenhint;
typedef struct {
    _ypIterObject_HEAD
    yp_INLINE_DATA( yp_uint8_t );
} ypIterObject;
#define ypIter_LENHINT( i ) ( ((ypIterObject *)i)->ob_lenhint )


yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc ) 
{
    if( yp_TYPE_PAIR_CODE( iterator ) != ypFrozenIter_CODE ) {
        return_yp_CEXC_BAD_TYPE( 0, exc, iterator );
    }
    // TODO ensure we never return <0
    return ypIter_LENHINT( iterator );
}


/*************************************************************************************************
 * Special (and dangerous) iterator for working with variable arguments of ypObject*s
 *************************************************************************************************/

// XXX Be very careful working with this: only pass it to functions that will call yp_iter_lenhintC
// and yp_next.  It is not your typical object: it's allocated on the stack.  While this could be
// dangerous, it also reduces duplicating code between versions that handle va_args and those that
// handle iterables.

typedef struct {
    _ypIterObject_HEAD
    va_list ob_args;
} _ypIterValistObject;

// The number of arguments is stored in ob_lenhint, which is automatically decremented by yp_next
// on each yielded value.
#define yp_ONSTACK_ITER_VALIST( name, n, args ) \
    _ypIterValistObject _ ## name ## _struct = { \
        _yp_IMMORTAL_HEAD_INIT( ypIter_CODE, NULL, 0 ), _ypIterValist_func, n, args}; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */

static ypObject *_ypIterValist_func( ypObject *_i, ypObject *value )
{
    _ypIterValistObject *i = (_ypIterValistObject *) _i;
    if( i->ob_lenhint < 1 ) return yp_StopIteration;
    return yp_incref( va_arg( i->ob_args, ypObject * ) );
}

// TODO #undef _ypIterObject_HEAD

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
yp_STATIC_ASSERT( offsetof( ypBytesObject, ob_inline_data ) % 8 == 0, bytes_inline_data_alignment );

#define ypBytes_DATA( b ) ( (yp_uint8_t *) ((ypObject *)b)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypBytes_LEN( b )  ( ((ypObject *)b)->ob_len )

// TODO end all bytes with a (hidden) null byte

// Return a new bytes object with uninitialized data of the given length, or an exception
static ypObject *_yp_bytes_new( yp_ssize_t len )
{
    ypObject *b;
    if( len < 0 ) len = 0; // TODO return a new ref to an immortal b'' object
    ypMem_MALLOC_CONTAINER_INLINE( b, ypBytesObject, ypBytes_CODE, len );
    if( yp_isexceptionC( b ) ) return b;
    ypBytes_LEN( b ) = len;
    return b;
}

// Return a new bytearray object with uninitialized data of the given length, or an exception
// TODO Over-allocate to avoid future resizings
static ypObject *_yp_bytearray_new( yp_ssize_t len )
{
    ypObject *b;
    if( len < 0 ) len = 0;
    ypMem_MALLOC_CONTAINER_VARIABLE( b, ypBytesObject, ypByteArray_CODE, len );
    if( yp_isexceptionC( b ) ) return b;
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
static ypObject *_ypBytes_resize( ypObject *b, yp_ssize_t newLen )
{
    ypObject *result;
    ypMem_REALLOC_CONTAINER_VARIABLE( result, b, ypBytesObject, newLen );
    if( yp_isexceptionC( result ) ) return result;
    ypBytes_LEN( b ) = newLen;
    return yp_None;
}

// If x is a fellow bytes, set *x_data and *x_len.  Otherwise, set *x_data=NULL and *x_len=0.
// TODO note http://bugs.python.org/issue12170 and ensure we stay consistent
static void _ypBytes_coerce_bytes( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len )
{
    int x_type = yp_TYPE_PAIR_CODE( x );

    if( x_type == ypBytes_CODE ) {
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
    int x_type = yp_TYPE_PAIR_CODE( x );

    if( x_type == ypBool_CODE || x_type == ypInt_CODE ) {
        *storage = yp_asuint8C( x, &result );
        if( !yp_isexceptionC( result ) ) {
            *x_data = storage;
            *x_len = 1;
            return;
        }
    } else if( x_type == ypBytes_CODE ) {
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


// Comparison methods return yp_True, yp_False, or an exception, and are not called if b==x.  When
// checking for (in)equality, more efficient to check size first
#define _ypBytes_RELATIVE_CMP_FUNCTION( name, operator ) \
static ypObject *bytes_ ## name( ypObject *b, ypObject *x ) { \
    int cmp; \
    yp_ssize_t b_len, x_len; \
    if( yp_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented; \
    b_len = ypBytes_LEN( b ); x_len = ypBytes_LEN( x ); \
    cmp = memcmp( ypBytes_DATA( b ), ypBytes_DATA( x ), MIN( b_len, x_len ) ); \
    if( cmp == 0 ) cmp = b_len < x_len ? -1 : (b_len > x_len ? 1 : 0); \
    return ypBool_FROM_C( cmp operator 0 ); \
}
_ypBytes_RELATIVE_CMP_FUNCTION( lt, < );
_ypBytes_RELATIVE_CMP_FUNCTION( le, <= );
_ypBytes_RELATIVE_CMP_FUNCTION( ge, >= );
_ypBytes_RELATIVE_CMP_FUNCTION( gt, > );
// TODO #undef _ypBytes_RELATIVE_CMP_FUNCTION
#define _ypBytes_EQUALITY_CMP_FUNCTION( name, operator ) \
static ypObject *bytes_ ## name( ypObject *b, ypObject *x ) { \
    int cmp; \
    yp_ssize_t b_len, x_len; \
    if( yp_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return yp_ComparisonNotImplemented; \
    b_len = ypBytes_LEN( b ); x_len = ypBytes_LEN( x ); \
    if( b_len != x_len ) return ypBool_FROM_C( 1 operator 0 ); \
    cmp = memcmp( ypBytes_DATA( b ), ypBytes_DATA( x ), MIN( b_len, x_len ) ); \
    return ypBool_FROM_C( cmp operator 0 ); \
}
_ypBytes_EQUALITY_CMP_FUNCTION( eq, == );
_ypBytes_EQUALITY_CMP_FUNCTION( ne, != );
// TODO #undef _ypBytes_EQUALITY_CMP_FUNCTION

// Must work even for mutables; yp_hash handles caching this value and denying its use for mutables
static yp_hash_t bytes_currenthash( ypObject *b ) {
    return yp_HashBytes( ypBytes_DATA( b ), ypBytes_LEN( b ) );
}

static ypObject *bytes_dealloc( ypObject *x ) {
    ypMem_FREE_CONTAINER( x, ypBytesObject );
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
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_length
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjproc,         // tp_remove
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
    MethodError_objproc,            // tp_iter
    MethodError_objproc,            // tp_iter_reversed
    MethodError_objobjproc,         // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    MethodError_lenfunc,            // tp_length
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjproc,         // tp_remove
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

// TODO undef macros


/*************************************************************************************************
 * Sequence of unicode characters
 *************************************************************************************************/

// TODO http://www.python.org/dev/peps/pep-0393/ (flexible string representations)

ypObject *yp_chrC( yp_int_t i ) {
    return yp_NotImplementedError; // TODO
}


/*************************************************************************************************
 * Sequence of generic items
 *************************************************************************************************/

// TODO Eventually, use timsort, but for now C's qsort should be fine

static ypObject *_yp_tuple_new( yp_ssize_t len )
{
    return yp_NotImplementedError;
}

static ypObject *_yp_list_new( yp_ssize_t len )
{
    return yp_NotImplementedError;
}


// Constructors

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypTuple( ypObject *(*allocator)( yp_ssize_t ), ypObject *iterable ) 
{
    return yp_NotImplementedError;
}

ypObject *yp_tupleN( int n, ... ) {
    if( n == 0 ) return _yp_tuple_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_tupleV, (n, args), n );
}
ypObject *yp_tupleV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypTuple( _yp_tuple_new, iter_args );
}
ypObject *yp_tuple( ypObject *iterable ) {
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

typedef struct {
    yp_hash_t se_hash;
    ypObject *se_key;
} ypSet_KeyEntry;
typedef struct {
    ypObject_HEAD
    yp_ssize_t so_fill; // # Active + # Dummy
    yp_INLINE_DATA( ypSet_KeyEntry );
} ypSetObject;

#define ypSet_TABLE( so ) ( (ypSet_KeyEntry *) ((ypObject *)so)->ob_data )
#define ypSet_SET_TABLE( so, value ) ( ((ypObject *)so)->ob_data = (void *) (value) )
// TODO what if ob_len is the "invalid" value?
#define ypSet_LEN( so )  ( ((ypObject *)so)->ob_len )
#define ypSet_FILL( so )  ( ((ypSetObject *)so)->so_fill )
#define ypSet_ALLOCLEN( so )  ( ((ypObject *)so)->ob_alloclen )
#define ypSet_MASK( so ) ( ypSet_ALLOCLEN( so ) - 1 )

#define ypSet_PERTURB_SHIFT (5)
#define ypSet_MINSIZE (8)
static ypObject _ypSet_dummy = yp_IMMORTAL_HEAD_INIT( ypInvalidated_CODE, NULL, 0 );
static ypObject *ypSet_dummy = &_ypSet_dummy;

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
#define ypDict_KEYSET( mp )  ( ((ypDictObject *)mp)->keyset )

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

// If a resize is necessary and you suspect future growth may occur, call this function to 
// determine the minused value to pass to _ypSet_resize.
// TODO make this limit configurable
// XXX Adapted from PyDict_SetItem
static yp_ssize_t _ypSet_calc_resize_minused( ypObject *so, yp_ssize_t newlen )
{
    if( ypObject_IS_MUTABLE( so ) ) {
        /* Quadrupling the size improves average dictionary sparseness
         * (reducing collisions) at the cost of some memory and iteration
         * speed (which loops over every possible entry).  It also halves
         * the number of expensive resize operations in a growing dictionary.
         *
         * Very large dictionaries (over 50K items) use doubling instead.
         * This may help applications with severe memory constraints.
         */
        return (newlen > 50000 ? 2 : 4) * newlen;
    } else {
        // frozensets don't need extra room to grow; additional keys will never be added
        return newlen;
    }

}

// Returns the alloclen that will fit minused, or <1 on error
// XXX Adapted from Python's dictresize
static yp_ssize_t _ypSet_calc_alloclen( yp_ssize_t minused )
{
    yp_ssize_t alloclen;
    for( alloclen = ypSet_MINSIZE;
         alloclen <= minused && alloclen > 0;
         alloclen <<= 1 );
    return alloclen;
}

static ypObject *_yp_frozenset_new( yp_ssize_t minused )
{
    // TODO
    return yp_NotImplementedError;
}

static ypObject *_yp_set_new( yp_ssize_t minused )
{
    // TODO
    return yp_NotImplementedError;
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
        ypSet_KeyEntry **loc )
{
    size_t i;
    size_t perturb;
    size_t mask = (size_t) ypSet_MASK( so );
    ypSet_KeyEntry *ep0 = ypSet_TABLE( so );

    i = hash & mask;
    (*loc) = &ep0[i];
    for (perturb = hash; (*loc)->se_key != NULL; perturb >>= ypSet_PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        (*loc) = &ep0[i & mask];
    }
    ypSet_FILL( so ) += 1;
    (*loc)->se_key = key;
    (*loc)->se_hash = hash;
    ypSet_LEN( so ) += 1;
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
    ypSet_KeyEntry *oldkeys;
    yp_ssize_t keysleft;
    yp_ssize_t i;
    ypSet_KeyEntry *loc;

    // TODO allocate the table in-line, then handle the case where both old and new tables
    // could fit in-line (idea: if currently in-line, then just force that the new array be
    // malloc'd...will need to malloc something anyway)
    newalloclen = _ypSet_calc_alloclen( minused );
    if( newalloclen < 1 ) return yp_MemoryError;
    newkeys = (ypSet_KeyEntry *) yp_malloc( newalloclen * sizeof( ypSet_KeyEntry * ) );
    if( newkeys == NULL ) return yp_MemoryError;
    memset( newkeys, 0, newalloclen * sizeof( ypSet_KeyEntry * ) );

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
    yp_free( oldkeys );
    return yp_None;
}

// Adds the key to the hash table.  *spaceleft should be initialized from  _ypSet_space_remaining; 
// this function then decrements it with each key added, and resets it on every resize.  Returns 
// yp_None or an exception.
// XXX Adapted from PyDict_SetItem
static ypObject *_ypSet_push( ypObject *so, ypObject *key, yp_ssize_t *spaceleft )
{
    yp_hash_t hash;
    ypObject *result = yp_None;
    ypSet_KeyEntry *loc;

    // Look for the appropriate entry in the hash table
    hash = yp_hashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( so, key, hash, &loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the key is already in the hash table, then there's nothing to do
    if( ypSet_ENTRY_USED( loc ) ) return yp_None;

    // Otherwise, we need to add the key, which possibly doesn't involve resizing
    if( *spaceleft >= 1 ) {
        _ypSet_movekey( so, loc, key, hash );
        *spaceleft -= 1;
        return yp_None;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean.  Remember that _ypSet_resize invalidates loc.
    result = _ypSet_resize( so, _ypSet_calc_resize_minused( so, ypSet_LEN( so )+1 ) );
    if( yp_isexceptionC( result ) ) return result;
    _ypSet_movekey_clean( so, key, hash, &loc );
    *spaceleft = _ypSet_space_remaining( so );
    return yp_None;
}

static ypObject *_ypSet_update_from_set( ypObject *so, ypObject *other ) 
{
    // TODO resize if necessary; if starting from clean, use _ypSet_movekey_clean, otherwise
    // _ypSet_movekey
    yp_ssize_t keysleft = ypSet_LEN( other );
    ypSet_KeyEntry *otherkeys = ypSet_TABLE( other );
    ypObject *result;
    yp_ssize_t i;
    ypSet_KeyEntry *loc;

    // Resize the set if necessary
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
            _ypSet_movekey_clean( so, yp_incref( otherkeys[i].se_key ), 
                    otherkeys[i].se_hash, &loc );
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
        _ypSet_movekey( so, loc, yp_incref( otherkeys[i].se_key ), otherkeys[i].se_hash );
    }
    return yp_None;
}

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypSet_update_from_iter( ypObject *so, ypObject **keyiter ) 
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    ypObject *result = yp_None;
    yp_ssize_t lenhint;
    ypObject *key;

    // Use lenhint in the hopes of requiring only one resize
    lenhint = yp_iter_lenhintC( *keyiter, &result );
    if( yp_isexceptionC( result ) ) return result;
    if( spaceleft < lenhint ) {
        result = _ypSet_resize( so, ypSet_LEN( so )+lenhint );
        if( yp_isexceptionC( result ) ) return result;
        spaceleft = _ypSet_space_remaining( so );
    }

    // Add all the yielded keys to the set.  Keep in mind that _ypSet_push may resize the
    // set despite our attempts at pre-allocating, since lenhint _is_ just a hint.
    while( 1 ) {
        key = yp_next( keyiter ); // new ref
        if( yp_isexceptionC( key ) ) {
            if( key == yp_StopIteration ) break;
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
static ypObject *_ypSet_update( ypObject *so, ypObject *iterable ) 
{
    // TODO determine when/where/how the set should be resized
    int iterable_type = yp_TYPE_PAIR_CODE( iterable );
    ypObject *keyiter;
    ypObject *result;

    // sets and dicts both iterate over a fellow set, so there are efficiencies we can exploit;
    // otherwise, treat it as a generic iterator.  Recall that type pairs are identified by the 
    // immutable type code.
    if( iterable_type == ypFrozenSet_CODE ) {
        return _ypSet_update_from_set( so, iterable );
    } else if( iterable_type == ypFrozenDict_CODE ) {
        return _ypSet_update_from_set( so, ypDict_KEYSET( iterable ) );
    } else {
        keyiter = yp_iter( iterable ); // new ref
        if( yp_isexceptionC( keyiter ) ) return keyiter;
        result = _ypSet_update_from_iter( so, &keyiter );
        yp_decref( keyiter );
        return result;
    }
}


// Public methods

static ypObject *set_push( ypObject *so, ypObject *x ) {
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    return _ypSet_push( so, x, &spaceleft );
}


// Constructors
// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypSet( ypObject *(*allocator)( yp_ssize_t ), ypObject *iterable ) 
{
    ypObject *newSo;
    ypObject *result = yp_None;
    yp_ssize_t lenhint = yp_iter_lenhintC( iterable, &result );
    if( yp_isexceptionC( result ) ) return result;

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
    if( n == 0 ) return _yp_frozenset_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_frozensetV, (n, args), n );
}
ypObject *yp_frozensetV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypSet( _yp_frozenset_new, iter_args );
}
ypObject *yp_frozenset( ypObject *iterable ) {
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

// XXX keyset requires care!  It is potentially shared among multiple dicts, so we cannot remove
// keys or resize it.  It identifies itself as a frozendict, yet we add keys to it, so it is not
// truly immutable.  As such, it cannot be exposed outside of the set/dict implementations.
// TODO alloclen will always be the same as keyset.alloclen; repurpose?
// ypDictObject and ypDict_KEYSET are defined above, for use by the set code
#define ypDict_LEN( mp )              ( ((ypObject *)mp)->ob_len )
#define ypDict_VALUES( mp )           ( (ypObject **) ((ypObject *)mp)->ob_data )
#define ypDict_SET_VALUES( mp, x )    ( ((ypObject *)mp)->ob_data = x )

// Returns the index of the given ypSet_KeyEntry in the hash table
#define ypDict_ENTRY_INDEX( mp, loc ) \
    ( ypSet_ENTRY_INDEX( ypDict_KEYSET( mp ), loc ) )

static ypObject *_yp_frozendict_new( yp_ssize_t minused )
{
    // TODO
    return yp_NotImplementedError;
}

static ypObject *_yp_dict_new( yp_ssize_t minused )
{
    // TODO
    return yp_NotImplementedError;
}

// The tricky bit about resizing dicts is that we need both the old and new keysets and value
// arrays to properly transfer the data, so ypMem_REALLOC_CONTAINER_VARIABLE is no help.
static ypObject *_ypDict_resize( ypObject *mp, yp_ssize_t minused )
{
    ypObject *newkeyset;
    yp_ssize_t newalloclen;
    ypObject **newvalues;
    ypSet_KeyEntry *oldkeys;
    ypObject **oldvalues;
    yp_ssize_t valuesleft;
    yp_ssize_t i;
    ypObject *value;
    ypSet_KeyEntry *loc;

    // TODO allocate the value array in-line, then handle the case where both old and new value
    // arrays could fit in-line (idea: if currently in-line, then just force that the new array be
    // malloc'd...will need to malloc something anyway)
    newkeyset = _yp_frozenset_new( minused );
    if( yp_isexceptionC( newkeyset ) ) return newkeyset;
    newalloclen = ypSet_ALLOCLEN( newkeyset );
    newvalues = (ypObject **) yp_malloc( newalloclen * sizeof( ypObject * ) );
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
        _ypSet_movekey_clean( newkeyset, oldkeys[i].se_key, oldkeys[i].se_hash, &loc );
        newvalues[ypSet_ENTRY_INDEX( newkeyset, loc )] = oldvalues[i];
        valuesleft -= 1;
    }

    yp_decref( ypDict_KEYSET( mp ) );
    ypDict_KEYSET( mp ) = newkeyset;
    yp_free( oldvalues );
    ypDict_SET_VALUES( mp, newvalues );
    return yp_None;
}

// Adds a new value to the value array corresponding to the given hash table location; updates the
// len count.  Replaces any existing value.
static void _ypDict_setvalue( ypObject *mp, ypSet_KeyEntry *loc, ypObject *value )
{
    yp_ssize_t i = ypDict_ENTRY_INDEX( mp, loc );
    ypObject *oldvalue = ypDict_VALUES( mp )[i];
    if( oldvalue == NULL ) {
        ypDict_LEN( mp ) += 1;
    } else {
        yp_decref( oldvalue );
    }
    ypDict_VALUES( mp )[i] = yp_incref( value );
}


// yp_None or an exception
// TODO The decision to resize currently depends only on _ypSet_space_remaining, but what if the
// shared keyset contains 5x the keys that we actually use?  That's a large waste in the value
// table.  Really, we should have a _ypDict_space_remaining.
static ypObject *dict_setitem( ypObject *mp, ypObject *key, ypObject *value )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *loc;
    ypObject *result = yp_None;

    // Look for the appropriate entry in the hash table
    // TODO yp_isexceptionC used internally should be a macro
    hash = yp_hashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypSet_lookkey( keyset, key, hash, &loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the key is already in the hash table, then we simply need to update the value
    if( ypSet_ENTRY_USED( loc ) ) {
        _ypDict_setvalue( mp, loc, value );
        return yp_None;
    }

    // Otherwise, we need to add the key, which possibly doesn't involve resizing
    if( _ypSet_space_remaining( keyset ) >= 1 ) {
        _ypSet_movekey( keyset, loc, yp_incref( key ), hash );
        _ypDict_setvalue( mp, loc, value );
        return yp_None;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean.
    result = _ypDict_resize( mp, ypDict_LEN( mp )+1 );   // invalidates keyset and loc
    if( yp_isexceptionC( result ) ) return result;
    keyset = ypDict_KEYSET( mp );
    _ypSet_movekey_clean( keyset, yp_incref( key ), hash, &loc );
    _ypDict_setvalue( mp, loc, value );
}


// Constructors
ypObject *yp_frozendictK( int n, ... ) {
    if( n == 0 ) return _yp_frozendict_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_frozendictKV, (n, args), n );
}
ypObject *yp_frozendictKV( int n, va_list args ) {
    return yp_NotImplementedError;
}

ypObject *yp_dictK( int n, ... ) {
    if( n == 0 ) return _yp_dict_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_dictKV, (n, args), n );
}
ypObject *yp_dictKV( int n, va_list args ) {
    return yp_NotImplementedError;
}

ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... ) {
    if( n == 0 ) return _yp_frozendict_new( 0 );
    return_yp_V_FUNC( ypObject *, yp_frozendict_fromkeysV, (value, n, args), n );
}
ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args ) {
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
    return yp_NotImplementedError;
}
ypObject *yp_dict( ypObject *x ) {
    return yp_NotImplementedError;
}


/*************************************************************************************************
 * The type table
 *************************************************************************************************/
// XXX Make sure this corresponds with ypInvalidated_TYPE et al!

// Recall that C helpfully sets missing array elements to NULL
static ypTypeObject *ypTypeTable[255] = {
    NULL,               /* ypInvalidated_CODE          (  0u) */
    NULL,               /*                             (  1u) */
    &ypException_Type,  /* ypException_CODE            (  2u) */
    NULL,               /*                             (  3u) */
    NULL,               /* ypType_CODE                 (  4u) */
    NULL,               /*                             (  5u) */

    NULL,               /* ypNone_CODE                 (  6u) */
    NULL,               /*                             (  7u) */
    NULL,               /* ypBool_CODE                 (  8u) */
    NULL,               /*                             (  9u) */

    &ypInt_Type,        /* ypInt_CODE                  ( 10u) */
    &ypIntStore_Type,   /* ypIntStore_CODE             ( 11u) */
    NULL,               /* ypFloat_CODE                ( 12u) */
    NULL,               /* ypFloatStore_CODE           ( 13u) */

    NULL,               /* ypFrozenIter_CODE           ( 14u) */
    NULL,               /* ypIter_CODE                 ( 15u) */

    &ypBytes_Type,      /* ypBytes_CODE                ( 16u) */
    &ypByteArray_Type,  /* ypByteArray_CODE            ( 17u) */
    NULL,               /* ypStr_CODE                  ( 18u) */
    NULL,               /* ypCharacterArray_CODE       ( 19u) */
    NULL,               /* ypTuple_CODE                ( 20u) */
    NULL,               /* ypList_CODE                 ( 21u) */

    NULL,               /* ypFrozenSet_CODE            ( 22u) */
    NULL,               /* ypSet_CODE                  ( 23u) */

    NULL,               /* ypFrozenDict_CODE           ( 24u) */
    NULL,               /* ypDict_CODE                 ( 25u) */
};


/*************************************************************************************************
 * Initialization
 *************************************************************************************************/

// TODO Make this accept configuration values from the user in a structure
void yp_initialize( void )
{
    yp_malloc = _yp_malloc;
    yp_realloc = _yp_realloc;
    yp_free = _yp_free;
    
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


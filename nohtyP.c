/*
 * nohtyP.c - A Python-like API for C, in one .c and one .h
 *      http://bitbucket.org/Syeberman/nohtyp   [v0.1.0 $Change$]
 *      Copyright � 2001-2013 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/3/license.html
 */

#include "nohtyP.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

#ifdef _MSC_VER
#include <Windows.h>
#endif


/*************************************************************************************************
 * Debug control
 *************************************************************************************************/

// yp_DEBUG_LEVEL controls how aggressively nohtyP should debug itself at runtime:
//  - 0: no debugging (default)
//  - 1: yp_assert (minimal debugging)
//  - 20: yp_DEBUG (print debugging)
#ifndef yp_DEBUG_LEVEL
    // Check for well-known debug defines; inspired from http://nothings.org/stb/stb_h.html
    #if defined( DEBUG ) || defined( _DEBUG ) || defined( DBG )
        #if defined( NDEBUG )
            #define yp_DEBUG_LEVEL 0
        #else
            #define yp_DEBUG_LEVEL 1
        #endif
    #else
        #define yp_DEBUG_LEVEL 0
    #endif
#endif

// From http://stackoverflow.com/questions/5641427
#define _yp_S(x) #x
#define _yp_S_(x) _yp_S(x)
#define _yp_S__LINE__ _yp_S_(__LINE__)

#if yp_DEBUG_LEVEL >= 1
#define _yp_ASSERT( file, line, expr, ... ) \
    do { \
        if( !(expr) ) { \
            _flushall( ); \
            fprintf( stderr, "%s", "\n  File " file ", line " line "\n    " #expr "\n" ); \
            fprintf( stderr, "yp_ASSERT: " __VA_ARGS__ ); \
            fprintf( stderr, "\n" ); \
            abort( ); \
        } \
    } while( 0 )
#else
#define _yp_ASSERT( ... )
#endif
#define yp_ASSERT( expr, ... )  _yp_ASSERT( "nohtyP.c", _yp_S__LINE__, expr, __VA_ARGS__ )
#define yp_ASSERT1( expr )      yp_ASSERT( expr, "" )

// Issues a breakpoint if the debugger is attached, on supported platforms
// TODO Debug only, and use only at the point the error is "raised" (rename to yp_raise?)
#if 0 && defined( _MSC_VER )
static void yp_breakonerr( ypObject *err ) {
    if( !IsDebuggerPresent( ) ) return;
    DebugBreak( );
}
#else
#define yp_breakonerr( err )
#endif

#if yp_DEBUG_LEVEL >= 20
#define yp_DEBUG0( fmt ) do { _flushall( ); fprintf( stderr, fmt "\n" ); _flushall( ); } while( 0 )
#define yp_DEBUG( fmt, ... ) do { _flushall( ); fprintf( stderr, fmt "\n", __VA_ARGS__ ); _flushall( ); } while( 0 )
#else
#define yp_DEBUG0( fmt )
#define yp_DEBUG( fmt, ... )
#endif

// We always perform static asserts: they don't affect runtime
#define yp_STATIC_ASSERT( cond, tag ) typedef char assert_ ## tag[ (cond) ? 1 : -1 ]


/*************************************************************************************************
 * Static assertions for nohtyP.h
 *************************************************************************************************/

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

yp_STATIC_ASSERT( sizeof( struct _ypObject ) == 16+(2*sizeof( void * )), sizeof_ypObject );

yp_STATIC_ASSERT( sizeof( "abcd" ) == 5, sizeof_str_includes_null );


/*************************************************************************************************
 * Internal structures and types, and related macros
 *************************************************************************************************/

typedef size_t yp_uhash_t;

// ypObject_HEAD defines the initial segment of every ypObject
#define ypObject_HEAD _ypObject_HEAD

// The least-significant bit of the type code specifies if the type is immutable (0) or not
#define ypObject_TYPE_CODE( ob ) \
    ( ((ypObject *)(ob))->ob_type )
#define ypObject_SET_TYPE_CODE( ob, type ) \
    ( ypObject_TYPE_CODE( ob ) = (type) )
#define ypObject_TYPE_CODE_IS_MUTABLE( type ) \
    ( (type) & 0x1u )
#define ypObject_TYPE_CODE_AS_FROZEN( type ) \
    ( (type) & 0xFEu )
#define ypObject_TYPE( ob ) \
    ( ypTypeTable[ypObject_TYPE_CODE( ob )] )
#define ypObject_IS_MUTABLE( ob ) \
    ( ypObject_TYPE_CODE_IS_MUTABLE( ypObject_TYPE_CODE( ob ) ) )
#define ypObject_REFCNT( ob ) \
    ( ((ypObject *)(ob))->ob_refcnt )

// Type pairs are identified by the immutable type code, as all its methods are supported by the
// immutable version
#define ypObject_TYPE_PAIR_CODE( ob ) \
    ypObject_TYPE_CODE_AS_FROZEN( ypObject_TYPE_CODE( ob ) )

// TODO Need two types of immortals: statically-allocated immortals (so should never be
// freed/invalidated) and overly-incref'd immortals (should be allowed to be invalidated and thus
// free any extra data, although the object itself will never be free'd as we've lost track of the
// refcounts)
#define ypObject_REFCNT_IMMORTAL    _ypObject_REFCNT_IMMORTAL

// When a hash of this value is stored in ob_hash, call tp_currenthash
#define ypObject_HASH_INVALID       _ypObject_HASH_INVALID

// Set ob_len or ob_alloclen to this value to signal an invalid length
#define ypObject_LEN_INVALID        _ypObject_LEN_INVALID

// The largest length that can be stored in ob_len and ob_alloclen
// XXX Among other considerations, we avoid lengths that use the last bit so we can check for
// overflow using "len<0" after these calculations:
//  - len_x + len_y
//  - len_x + len_y + 1 (adding null terminator to bytes/str)
#define ypObject_LEN_MAX            (0x7FFFFFFF)

// Lengths and hashes can be cached in the object for easy retrieval
#define ypObject_CACHED_LEN( ob )   ( ((ypObject *)(ob))->ob_len )  // negative if invalid
#define ypObject_CACHED_HASH( ob )  ( ((ypObject *)(ob))->ob_hash ) // HASH_INVALID if invalid

// Base "constructor" for immortal objects
#define yp_IMMORTAL_HEAD_INIT _yp_IMMORTAL_HEAD_INIT

// Base "constructor" for immortal type objects
#define yp_TYPE_HEAD_INIT yp_IMMORTAL_HEAD_INIT( ypType_CODE, NULL, ypObject_LEN_INVALID )

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
typedef enum { yp_FIND_FORWARD, yp_FIND_REVERSE } findfunc_direction;
typedef ypObject *(*findfunc)( ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, findfunc_direction, yp_ssize_t * );
typedef ypObject *(*sortfunc)( ypObject *, yp_sort_key_func_t, ypObject * );
typedef ypObject *(*popitemfunc)( ypObject *, ypObject **, ypObject ** );

// Suite of number methods.  A placeholder for now, as the current implementation doesn't allow
// overriding yp_add et al (TODO).
typedef struct {
    objproc _placeholder;
} ypNumberMethods;

typedef struct {
    objobjproc tp_concat;
    objssizeproc tp_repeat;
    objssizeobjproc tp_getindex;    /* if defval is NULL, raise exception if missing */
    objsliceproc tp_getslice;
    findfunc tp_find;
    countfunc tp_count;
    objssizeobjproc tp_setindex;
    objsliceobjproc tp_setslice;
    objssizeproc tp_delindex;
    objsliceproc tp_delslice;
    objobjproc tp_append;
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
    objvalistproc tp_union;
    objvalistproc tp_intersection;
    objvalistproc tp_difference;
    objobjproc tp_symmetric_difference;
    // tp_update is elsewhere
    objvalistproc tp_intersection_update;
    objvalistproc tp_difference_update;
    objobjproc tp_symmetric_difference_update;
    // tp_push (aka tp_set_add) is elsewhere
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
    objvalistproc tp_updateK;
    miniiterfunc tp_miniiter_values;
    objproc tp_iter_values;
} ypMappingMethods;

// Type objects hold pointers to each type's methods.
typedef struct {
    ypObject_HEAD
    ypObject *tp_name; /* For printing, in format "<module>.<name>" */

    // Object fundamentals
    objproc tp_dealloc;
    traversefunc tp_traverse; /* call function for all accessible objects; return on exception */
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
    objvalistproc tp_update;

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
    static ypObject *name ## _findfunc( ypObject *x, ypObject *y, yp_ssize_t i, yp_ssize_t j, findfunc_direction direction, yp_ssize_t *index ) { return retval; } \
    static ypObject *name ## _sortfunc( ypObject *x, yp_sort_key_func_t key, ypObject *reverse ) { return retval; } \
    static ypObject *name ## _popitemfunc( ypObject *x, ypObject **key, ypObject **value ) { return retval; } \
    \
    static ypNumberMethods name ## _NumberMethods[1] = { { \
        *name ## _objproc \
    } }; \
    static ypSequenceMethods name ## _SequenceMethods[1] = { { \
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
        *name ## _objvalistproc, \
        *name ## _miniiterfunc, \
        *name ## _objproc \
    } };
DEFINE_GENERIC_METHODS( MethodError, yp_MethodError ); // for use in methods the type doesn't support
DEFINE_GENERIC_METHODS( TypeError, yp_TypeError );
DEFINE_GENERIC_METHODS( InvalidatedError, yp_InvalidatedError ); // for use by Invalidated objects
DEFINE_GENERIC_METHODS( ExceptionMethod, x ); // for use by exception objects; returns "self"

// For use when an object doesn't support a particular comparison operation
extern ypObject * const yp_ComparisonNotImplemented;
static ypObject *NotImplemented_comparefunc( ypObject *x, ypObject *y ) {
    return yp_ComparisonNotImplemented;
}

// For use when an object contains no references to other objects
static ypObject *NoRefs_traversefunc( ypObject *x, visitfunc visitor, void *memo ) { return yp_None; } \


/*************************************************************************************************
 * Helpful functions and macros
 *************************************************************************************************/

#ifndef MIN
#define MIN(a,b)   ((a) < (b) ? (a) : (b))
#define MAX(a,b)   ((a) > (b) ? (a) : (b))
#endif

// Functions that return nohtyP objects simply need to return the error object to "raise" it
// Use this as "return_yp_ERR( x, yp_ValueError );" to return the error properly
#define return_yp_ERR( _err ) \
    do { ypObject *_yp_ERR_err = (_err); return _yp_ERR_err; } while( 0 )

// Functions that modify their inputs take a "ypObject **x".
// Use this as "yp_INPLACE_ERR( x, yp_ValueError );" to discard x and set it to an exception
#define yp_INPLACE_ERR( ob, _err ) \
    do { ypObject *_yp_ERR_err = (_err); yp_decref( *(ob) ); *(ob) = (_yp_ERR_err); } while( 0 )
// Use this as "return_yp_INPLACE_ERR( x, yp_ValueError );" to return the error properly
#define return_yp_INPLACE_ERR( ob, _err ) \
    do { yp_INPLACE_ERR( (ob), (_err) ); return; } while( 0 )

// Functions that return C values take a "ypObject **exc" that are only modified on error and are
// not discarded beforehand; they also need to return a valid C value
#define return_yp_CEXC_ERR( retval, exc, _err ) \
    do { ypObject *_yp_ERR_err = (_err); *(exc) = (_yp_ERR_err); return retval; } while( 0 )

// When an object encounters an unknown type, there are three possible cases:
//  - it's an invalidated object, so return yp_InvalidatedError
//  - it's an exception, so return it
//  - it's some other type, so return yp_TypeError
// TODO It'd be nice to remove a comparison from this, as a minor efficiency, but not sure how
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

#define yp_IS_EXCEPTION_C( x ) (ypObject_TYPE_PAIR_CODE( x ) == ypException_CODE)
int yp_isexceptionC( ypObject *x ) {
    return yp_IS_EXCEPTION_C( x );
}

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

// For when we need to work with unsigned yp_int_t's in the math below; casting to unsigned helps
// avoid undefined behaviour on overflow.
typedef yp_uint64_t _yp_uint_t;
yp_STATIC_ASSERT( sizeof( _yp_uint_t ) == sizeof( yp_int_t ), sizeof_yp_uint_eq_yp_int );
#define yp_UINT_MATH( x, op, y ) ((yp_int_t) (((_yp_uint_t)x) op ((_yp_uint_t)y)))

#ifdef _MSC_VER
#define yp_IS_NAN _isnan
#define yp_IS_INFINITY(X) (!_finite(X) && !_isnan(X))
#define yp_IS_FINITE(X) _finite(X)
#else
#error Need to port Py_IS_NAN et al to nohtyP for this platform
#endif

// Prime multiplier used in string and various other hashes
// XXX Adapted from Python's _PyHASH_MULTIPLIER
#define _ypHASH_MULTIPLIER 1000003  // 0xf4243

// Parameters used for the numeric hash implementation.  Numeric hashes are based on reduction
// modulo the prime 2**_PyHASH_BITS - 1.
// XXX Adapted from Python's pyport.h
#if SIZE_MAX == 0xFFFFFFFFu
#define _ypHASH_BITS 31
#else
#define _ypHASH_BITS 61
#endif
#define _ypHASH_MODULUS (((size_t)1 << _ypHASH_BITS) - 1)
#define _ypHASH_INF 314159
#define _ypHASH_NAN 0

// Return the hash of the given double; always succeeds
// XXX Adapted from Python's _Py_HashDouble
yp_hash_t yp_HashDouble( double v )
{
    int e, sign;
    double m;
    yp_uhash_t x, y;

    if (!yp_IS_FINITE(v)) {
        if (yp_IS_INFINITY(v)) {
            return v > 0 ? _ypHASH_INF : -_ypHASH_INF;
        } else {
            return _ypHASH_NAN;
        }
    }

    m = frexp(v, &e);

    sign = 1;
    if (m < 0) {
        sign = -1;
        m = -m;
    }

    /* process 28 bits at a time;  this should work well both for binary
       and hexadecimal floating point. */
    x = 0;
    while (m) {
        x = ((x << 28) & _ypHASH_MODULUS) | x >> (_ypHASH_BITS - 28);
        m *= 268435456.0;  /* 2**28 */
        e -= 28;
        y = (yp_uhash_t)m;  /* pull out integer part */
        m -= y;
        x += y;
        if (x >= _ypHASH_MODULUS) {
            x -= _ypHASH_MODULUS;
        }
    }

    /* adjust for the exponent;  first reduce it modulo _ypHASH_BITS */
    e = e >= 0 ? e % _ypHASH_BITS : _ypHASH_BITS-1-((-1-e) % _ypHASH_BITS);
    x = ((x << e) & _ypHASH_MODULUS) | x >> (_ypHASH_BITS - e);

    x = x * sign;
    if (x == (yp_uhash_t)ypObject_HASH_INVALID) {
        x = (yp_uhash_t)(ypObject_HASH_INVALID-1);
    }
    return (yp_hash_t)x;
}

// Return the hash of the given pointer; always succeeds
// XXX Adapted from Python's _Py_HashPointer
static yp_hash_t yp_HashPointer( void *p )
{
    yp_hash_t x;
    size_t y = (size_t) p;
    /* bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
       excessive hash collisions for dicts and sets */
    y = (y >> 4) | (y << (8 * sizeof( void * ) - 4));
    x = (yp_hash_t) y;
    if (x == ypObject_HASH_INVALID) x -= 1;
    return x;
}

// Return the hash of the given number of bytes; always succeeds
// XXX Adapted from Python's _Py_HashBytes
static yp_hash_t yp_HashBytes( yp_uint8_t *p, yp_ssize_t len )
{
    yp_uhash_t x;
    yp_ssize_t i;

    if( len == 0 ) return 0;
    x = 0; /*(yp_uhash_t) _yp_HashSecret.prefix;*/
    x ^= (yp_uhash_t) *p << 7;
    for (i = 0; i < len; i++) {
        x = (_ypHASH_MULTIPLIER * x) ^ (yp_uhash_t) *p++;
    }
    x ^= (yp_uhash_t) len;
    /*x ^= (yp_uhash_t) _yp_HashSecret.suffix;*/
    if (x == (yp_uhash_t) ypObject_HASH_INVALID) {
        x = (yp_uhash_t) (ypObject_HASH_INVALID-1);
    }
    return x;
}

// TODO Make this configurable via yp_initialize
static int _yp_recursion_limit = 1000;


/*************************************************************************************************
 * Locale-independent ctype.h-like macros
 * XXX This entire section is adapted from Python's pyctype.c and pyctype.h
 *************************************************************************************************/

#define _yp_CTF_LOWER  0x01
#define _yp_CTF_UPPER  0x02
#define _yp_CTF_ALPHA  (_yp_CTF_LOWER|_yp_CTF_UPPER)
#define _yp_CTF_DIGIT  0x04
#define _yp_CTF_ALNUM  (_yp_CTF_ALPHA|_yp_CTF_DIGIT)
#define _yp_CTF_SPACE  0x08
#define _yp_CTF_XDIGIT 0x10

/* Unlike their C counterparts, the following macros are meant only for yp_uint8_t values. */
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
// TODO In Python, this is a table of unsigned ints, which is unnecessarily large; report and fix
const yp_uint8_t _yp_ctype_table[256] = {
    0, /* 0x0 '\x00' */
    0, /* 0x1 '\x01' */
    0, /* 0x2 '\x02' */
    0, /* 0x3 '\x03' */
    0, /* 0x4 '\x04' */
    0, /* 0x5 '\x05' */
    0, /* 0x6 '\x06' */
    0, /* 0x7 '\x07' */
    0, /* 0x8 '\x08' */
    _yp_CTF_SPACE, /* 0x9 '\t' */
    _yp_CTF_SPACE, /* 0xa '\n' */
    _yp_CTF_SPACE, /* 0xb '\v' */
    _yp_CTF_SPACE, /* 0xc '\f' */
    _yp_CTF_SPACE, /* 0xd '\r' */
    0, /* 0xe '\x0e' */
    0, /* 0xf '\x0f' */
    0, /* 0x10 '\x10' */
    0, /* 0x11 '\x11' */
    0, /* 0x12 '\x12' */
    0, /* 0x13 '\x13' */
    0, /* 0x14 '\x14' */
    0, /* 0x15 '\x15' */
    0, /* 0x16 '\x16' */
    0, /* 0x17 '\x17' */
    0, /* 0x18 '\x18' */
    0, /* 0x19 '\x19' */
    0, /* 0x1a '\x1a' */
    0, /* 0x1b '\x1b' */
    0, /* 0x1c '\x1c' */
    0, /* 0x1d '\x1d' */
    0, /* 0x1e '\x1e' */
    0, /* 0x1f '\x1f' */
    _yp_CTF_SPACE, /* 0x20 ' ' */
    0, /* 0x21 '!' */
    0, /* 0x22 '"' */
    0, /* 0x23 '#' */
    0, /* 0x24 '$' */
    0, /* 0x25 '%' */
    0, /* 0x26 '&' */
    0, /* 0x27 "'" */
    0, /* 0x28 '(' */
    0, /* 0x29 ')' */
    0, /* 0x2a '*' */
    0, /* 0x2b '+' */
    0, /* 0x2c ',' */
    0, /* 0x2d '-' */
    0, /* 0x2e '.' */
    0, /* 0x2f '/' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x30 '0' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x31 '1' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x32 '2' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x33 '3' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x34 '4' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x35 '5' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x36 '6' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x37 '7' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x38 '8' */
    _yp_CTF_DIGIT|_yp_CTF_XDIGIT, /* 0x39 '9' */
    0, /* 0x3a ':' */
    0, /* 0x3b ';' */
    0, /* 0x3c '<' */
    0, /* 0x3d '=' */
    0, /* 0x3e '>' */
    0, /* 0x3f '?' */
    0, /* 0x40 '@' */
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, /* 0x41 'A' */
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, /* 0x42 'B' */
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, /* 0x43 'C' */
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, /* 0x44 'D' */
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, /* 0x45 'E' */
    _yp_CTF_UPPER|_yp_CTF_XDIGIT, /* 0x46 'F' */
    _yp_CTF_UPPER, /* 0x47 'G' */
    _yp_CTF_UPPER, /* 0x48 'H' */
    _yp_CTF_UPPER, /* 0x49 'I' */
    _yp_CTF_UPPER, /* 0x4a 'J' */
    _yp_CTF_UPPER, /* 0x4b 'K' */
    _yp_CTF_UPPER, /* 0x4c 'L' */
    _yp_CTF_UPPER, /* 0x4d 'M' */
    _yp_CTF_UPPER, /* 0x4e 'N' */
    _yp_CTF_UPPER, /* 0x4f 'O' */
    _yp_CTF_UPPER, /* 0x50 'P' */
    _yp_CTF_UPPER, /* 0x51 'Q' */
    _yp_CTF_UPPER, /* 0x52 'R' */
    _yp_CTF_UPPER, /* 0x53 'S' */
    _yp_CTF_UPPER, /* 0x54 'T' */
    _yp_CTF_UPPER, /* 0x55 'U' */
    _yp_CTF_UPPER, /* 0x56 'V' */
    _yp_CTF_UPPER, /* 0x57 'W' */
    _yp_CTF_UPPER, /* 0x58 'X' */
    _yp_CTF_UPPER, /* 0x59 'Y' */
    _yp_CTF_UPPER, /* 0x5a 'Z' */
    0, /* 0x5b '[' */
    0, /* 0x5c '\\' */
    0, /* 0x5d ']' */
    0, /* 0x5e '^' */
    0, /* 0x5f '_' */
    0, /* 0x60 '`' */
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, /* 0x61 'a' */
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, /* 0x62 'b' */
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, /* 0x63 'c' */
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, /* 0x64 'd' */
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, /* 0x65 'e' */
    _yp_CTF_LOWER|_yp_CTF_XDIGIT, /* 0x66 'f' */
    _yp_CTF_LOWER, /* 0x67 'g' */
    _yp_CTF_LOWER, /* 0x68 'h' */
    _yp_CTF_LOWER, /* 0x69 'i' */
    _yp_CTF_LOWER, /* 0x6a 'j' */
    _yp_CTF_LOWER, /* 0x6b 'k' */
    _yp_CTF_LOWER, /* 0x6c 'l' */
    _yp_CTF_LOWER, /* 0x6d 'm' */
    _yp_CTF_LOWER, /* 0x6e 'n' */
    _yp_CTF_LOWER, /* 0x6f 'o' */
    _yp_CTF_LOWER, /* 0x70 'p' */
    _yp_CTF_LOWER, /* 0x71 'q' */
    _yp_CTF_LOWER, /* 0x72 'r' */
    _yp_CTF_LOWER, /* 0x73 's' */
    _yp_CTF_LOWER, /* 0x74 't' */
    _yp_CTF_LOWER, /* 0x75 'u' */
    _yp_CTF_LOWER, /* 0x76 'v' */
    _yp_CTF_LOWER, /* 0x77 'w' */
    _yp_CTF_LOWER, /* 0x78 'x' */
    _yp_CTF_LOWER, /* 0x79 'y' */
    _yp_CTF_LOWER, /* 0x7a 'z' */
    0, /* 0x7b '{' */
    0, /* 0x7c '|' */
    0, /* 0x7d '}' */
    0, /* 0x7e '~' */
    0, /* 0x7f '\x7f' */
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

// See docs for yp_initialize_kwparams.yp_malloc in nohtyP.h
static void *(*yp_malloc)( yp_ssize_t *actual, yp_ssize_t size ) = _dummy_yp_malloc;

// See docs for yp_initialize_kwparams.yp_malloc_resize in nohtyP.h
static void *(*yp_malloc_resize)( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra ) = _dummy_yp_malloc_resize;

// See docs for yp_initialize_kwparams.yp_free in nohtyP.h
static void (*yp_free)( void *p );

// Microsoft gives a couple options for heaps; let's stick with the standard malloc/free plus
// _msize and _expand
#ifdef _MSC_VER
#include <malloc.h>
#include <crtdbg.h> // For debugging only
static void *_default_yp_malloc( yp_ssize_t *actual, yp_ssize_t size )
{
    void *p;
    if( size < 0 ) return NULL; // overflow, likely
    if( size < 1 ) size = 1;
    p = malloc( (size_t) size );
    if( p == NULL ) return NULL;
    *actual = (yp_ssize_t) _msize( p );
    if( *actual < 0 ) *actual = yp_SSIZE_T_MAX; // we were given more memory than we can use
    yp_DEBUG( "malloc: 0x%08X %d bytes", p, *actual );
    return p;
}
static void *_default_yp_malloc_resize( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra )
{
    void *newp;
    if( size < 0 ) return NULL; // overflow, likely
    if( size < 1 ) size = 1;
    if( extra < 0 ) return NULL; // overflow, likely
    if( extra > yp_SSIZE_T_MAX-size ) extra = yp_SSIZE_T_MAX - size;

    newp = _expand( p, (size_t) size );
    if( newp == NULL ) {
        newp = malloc( ((size_t) size) + ((size_t) extra) );
        if( newp == NULL ) return NULL;
    }
    *actual = (yp_ssize_t) _msize( newp );
    if( *actual < 0 ) *actual = yp_SSIZE_T_MAX; // we were given more memory than we can use
    yp_DEBUG( "malloc_resize: 0x%08X %d bytes  (was 0x%08X)", newp, *actual, p );
    return newp;
}
static void _default_yp_free( void *p ) {
    yp_DEBUG( "free: 0x%08X", p );
    free( p );
}

// If all else fails, rely on the standard C malloc/free functions
#else
// Rounds allocations up to a multiple that should be easy for most heaps without wasting space for
// the smallest objects (ie ints); don't call with a negative size
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
    if( size < 0 ) return NULL; // overflow, likely
    *actual = _default_yp_malloc_good_size( size );
    return malloc( *actual );
}
static void *_default_yp_malloc_resize( yp_ssize_t *actual,
        void *p, yp_ssize_t size, yp_ssize_t extra )
{
    if( size < 0 ) return NULL; // overflow, likely
    if( extra < 0 ) return NULL; // overflow, likely
    if( extra > yp_SSIZE_T_MAX-size ) extra = yp_SSIZE_T_MAX - size;
    *actual = _default_yp_malloc_good_size( size+extra );
    return malloc( *actual );
}
static void (*_default_yp_free)( void *p ) = free;
#endif


/*************************************************************************************************
 * nohtyP object allocations
 *************************************************************************************************/

// FIXME this section could use another pass to review and clean up docs/code/etc

// This should be one of exactly two possible values, 1 (the default) or ypObject_REFCNT_IMMORTAL,
// depending on yp_initialize's everything_immortal parameter
static yp_uint32_t _ypMem_starting_refcnt = 1;
yp_STATIC_ASSERT( sizeof( _ypMem_starting_refcnt ) == yp_sizeof_member( ypObject, ob_refcnt ), sizeof_starting_refcnt );

// Declares the ob_inline_data array for container object structures
#define yp_INLINE_DATA _yp_INLINE_DATA

// Returns a malloc'd buffer for fixed, non-container objects, or exception on failure
static ypObject *_ypMem_malloc_fixed( yp_ssize_t sizeof_obStruct, int type )
{
    yp_ssize_t size;
    ypObject *ob = (ypObject *) yp_malloc( &size, sizeof_obStruct );
    if( ob == NULL ) return yp_MemoryError;
    ob->ob_alloclen = ypObject_LEN_INVALID;
    ob->ob_data = NULL;
    ob->ob_type = type;
    ob->ob_refcnt = _ypMem_starting_refcnt;
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = ypObject_LEN_INVALID;
    yp_DEBUG( "MALLOC_FIXED: type %d 0x%08X", type, ob );
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

    yp_ASSERT( alloclen >= 0, "alloclen cannot be negative" );
    if( alloclen > ypObject_LEN_MAX ) return yp_SystemLimitationError;

    // Allocate memory, then update alloclen based on actual size of buffer allocated
    ob = (ypObject *) yp_malloc( &size, offsetof_inline + (alloclen * sizeof_elems) );
    if( ob == NULL ) return yp_MemoryError;
    alloclen = (size - offsetof_inline) / sizeof_elems; // rounds down
    if( alloclen > ypObject_LEN_MAX ) alloclen = ypObject_LEN_MAX;

    ob->ob_alloclen = alloclen;
    ob->ob_data = ((yp_uint8_t *)ob) + offsetof_inline;
    ob->ob_type = type;
    ob->ob_refcnt = _ypMem_starting_refcnt;
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = 0;
    yp_DEBUG( "MALLOC_CONTAINER_INLINE: type %d 0x%08X alloclen %d", type, ob, alloclen );
    return ob;
}
#define ypMem_MALLOC_CONTAINER_INLINE( obStruct, type, alloclen ) \
    _ypMem_malloc_container_inline( offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ), (type), (alloclen) )

// TODO Make this configurable via yp_initialize
// XXX 64-bit PyDictObject is 128 bytes...we are larger!
// XXX Cannot change once objects are allocated
// TODO Static asserts to ensure that certain-sized objects fit with one allocation, then optimize
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
static ypObject *_ypMem_malloc_container_variable(
        yp_ssize_t offsetof_inline, yp_ssize_t sizeof_elems, int type,
        yp_ssize_t required, yp_ssize_t extra )
{
    yp_ssize_t size;
    ypObject *ob;
    yp_ssize_t alloclen;

    yp_ASSERT( required >= 0, "required cannot be negative" );
    yp_ASSERT( extra >= 0, "extra cannot be negative" );
    if( required > ypObject_LEN_MAX ) return yp_SystemLimitationError;
    if( required+extra > ypObject_LEN_MAX ) extra = ypObject_LEN_MAX - required;

    // Allocate object memory, update alloclen based on actual size of buffer allocated, then see
    // if the data can fit inline or if it needs a separate buffer
    ob = (ypObject *) yp_malloc( &size, MAX( offsetof_inline, _ypMem_ideal_size ) );
    if( ob == NULL ) return yp_MemoryError;
    alloclen = (size - offsetof_inline) / sizeof_elems; // rounds down
    if( required <= alloclen ) { // a valid check even if alloclen>max, because required<=max
        ob->ob_data = ((yp_uint8_t *)ob) + offsetof_inline;
    } else {
        ob->ob_data = yp_malloc( &size, (required+extra) * sizeof_elems );
        if( ob->ob_data == NULL ) {
            yp_free( ob );
            return yp_MemoryError;
        }
        alloclen = size / sizeof_elems; // rounds down
    }
    if( alloclen > ypObject_LEN_MAX ) alloclen = ypObject_LEN_MAX;

    ob->ob_alloclen = alloclen;
    ob->ob_type = type;
    ob->ob_refcnt = _ypMem_starting_refcnt;
    ob->ob_hash = ypObject_HASH_INVALID;
    ob->ob_len = 0;
    yp_DEBUG( "MALLOC_CONTAINER_VARIABLE: type %d 0x%08X alloclen %d", type, ob, alloclen );
    return ob;
}
#define ypMem_MALLOC_CONTAINER_VARIABLE( obStruct, type, required, extra ) \
    _ypMem_malloc_container_variable( offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ), (type), (required), (extra) )

// Resizes ob_data, the variable-portion of ob, and returns yp_None.  On error, ob is not modified,
// and an exception is returned.  required is the minimum ob_alloclen required, and the minimum
// number of elements that will be preserved; if required can fit inline, the inline buffer is
// used.  extra is a hint as to how much the buffer should be over-allocated, which may be ignored.
// This function will always resize the data, so first check to see if a resize is necessary.  Any
// objects in the truncated section must have already been discarded.  required and extra cannot
// be negative; ob_alloclen may be larger than requested.  Does not update ob_len.
// TODO The calculated inlinelen may be smaller than what our initial allocation actually provided
// TODO Let the caller move data around and free the old buffer
static ypObject *_ypMem_realloc_container_variable(
        ypObject *ob, yp_ssize_t offsetof_inline, yp_ssize_t sizeof_elems,
        yp_ssize_t required, yp_ssize_t extra )
{
    void *newptr;
    yp_ssize_t size;
    yp_ssize_t alloclen;
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_ssize_t inlinelen = (_ypMem_ideal_size-offsetof_inline) / sizeof_elems;
    if( inlinelen < 0 ) inlinelen = 0;

    yp_ASSERT( required >= 0, "required cannot be negative" );
    yp_ASSERT( extra >= 0, "extra cannot be negative" );
    if( required > ypObject_LEN_MAX ) return yp_SystemLimitationError;
    if( extra > ypObject_LEN_MAX-required ) extra = ypObject_LEN_MAX - required;

    // If the minimum required allocation can fit inline, then prefer that over a separate buffer
    if( required <= inlinelen ) {
        // If the data is currently not inline, move it there, then free the other buffer
        if( ob->ob_data != inlineptr ) {
            // TODO check for overflow?!  (should be impossible here because sizes are small)
            memcpy( inlineptr, ob->ob_data, required * sizeof_elems );
            yp_free( ob->ob_data );
            ob->ob_data = inlineptr;
            ob->ob_alloclen = inlinelen;
        }
        yp_DEBUG( "REALLOC_CONTAINER_VARIABLE (to inline): 0x%08X alloclen %d", ob, ob->ob_alloclen );
        return yp_None;
    }

    // If the data is currently inline, it must be moved out into a separate buffer
    if( ob->ob_data == inlineptr ) {
        // TODO check for overflow?!
        newptr = yp_malloc( &size, (required+extra) * sizeof_elems );
        if( newptr == NULL ) return yp_MemoryError;
        memcpy( newptr, ob->ob_data, inlinelen * sizeof_elems );
        ob->ob_data = newptr;
        alloclen = size / sizeof_elems; // rounds down
        if( alloclen > ypObject_LEN_MAX ) alloclen = ypObject_LEN_MAX;
        ob->ob_alloclen = alloclen;
        yp_DEBUG( "REALLOC_CONTAINER_VARIABLE (from inline): 0x%08X alloclen %d", ob, alloclen );
        return yp_None;
    }

    // Otherwise, let yp_malloc_resize determine if we can expand in-place or need to memcpy
    // TODO check for overflow?!
    newptr = yp_malloc_resize( &size, ob->ob_data, required * sizeof_elems, extra * sizeof_elems );
    if( newptr == NULL ) return yp_MemoryError;
    if( newptr != ob->ob_data ) {
        memcpy( newptr, ob->ob_data, MIN(ob->ob_alloclen * sizeof_elems, size) );
        yp_free( ob->ob_data );
        ob->ob_data = newptr;
    }
    alloclen = size / sizeof_elems; // rounds down
    if( alloclen > ypObject_LEN_MAX ) alloclen = ypObject_LEN_MAX;
    ob->ob_alloclen = alloclen;
    yp_DEBUG( "REALLOC_CONTAINER_VARIABLE (malloc_resize): 0x%08X alloclen %d", ob, alloclen );
    return yp_None;
}
#define ypMem_REALLOC_CONTAINER_VARIABLE( ob, obStruct, required, extra ) \
    _ypMem_realloc_container_variable( ob, offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ), (required), (extra) )

// Equivalent, but optimized, version of:
//  ypMem_REALLOC_CONTAINER_VARIABLE( ob, obStruct, 0, 0 )
// Will always reset ob_data to the inline buffer, freeing the separate buffer (if there is one).
// Any contained objects must have already been discarded; no memory is copied.  Always succeeds.
// TODO The calculated inlinelen may be smaller than what our initial allocation actually provided
static void _ypMem_realloc_container_variable_clear(
        ypObject *ob, yp_ssize_t offsetof_inline, yp_ssize_t sizeof_elems )
{
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_ssize_t inlinelen = (_ypMem_ideal_size-offsetof_inline) / sizeof_elems;
    if( inlinelen < 0 ) inlinelen = 0;

    // Free any separately-allocated buffer
    if( ob->ob_data != inlineptr ) {
        yp_free( ob->ob_data );
        ob->ob_data = inlineptr;
        ob->ob_alloclen = inlinelen;
    }
    yp_DEBUG( "REALLOC_CONTAINER_VARIABLE_CLEAR: 0x%08X alloclen %d", ob, ob->ob_alloclen );
}
#define ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR( ob, obStruct ) \
    _ypMem_realloc_container_variable_clear( ob, offsetof( obStruct, ob_inline_data ), \
            yp_sizeof_member( obStruct, ob_inline_data[0] ) )

// Frees an object allocated with ypMem_MALLOC_FIXED
#define ypMem_FREE_FIXED yp_free

// Frees an object allocated with either ypMem_REALLOC_CONTAINER_* macro
static void _ypMem_free_container( ypObject *ob, yp_ssize_t offsetof_inline )
{
    void *inlineptr = ((yp_uint8_t *)ob) + offsetof_inline;
    yp_DEBUG( "FREE_CONTAINER: 0x%08X", ob );
    if( ob->ob_data != inlineptr ) yp_free( ob->ob_data );
    yp_free( ob );
}
#define ypMem_FREE_CONTAINER( ob, obStruct ) \
    _ypMem_free_container( ob, offsetof( obStruct, ob_inline_data ) )


/*************************************************************************************************
 * Object fundamentals
 *************************************************************************************************/

// TODO What if these (and yp_isexceptionC) were macros in the header so they were force-inlined
// by users of the API...even if through a DLL?
ypObject *yp_incref( ypObject *x )
{
    if( ypObject_REFCNT( x ) >= ypObject_REFCNT_IMMORTAL ) return x; // no-op
    ypObject_REFCNT( x ) += 1;
    yp_DEBUG( "incref: 0x%08X refcnt %d", x, ypObject_REFCNT( x ) );
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
    if( ypObject_REFCNT( x ) >= ypObject_REFCNT_IMMORTAL ) return; // no-op

    if( ypObject_REFCNT( x ) <= 1 ) {
        ypObject *result;
        yp_DEBUG( "decref (dealloc): 0x%08X", x );
        result = ypObject_TYPE( x )->tp_dealloc( x );
        yp_ASSERT( !yp_isexceptionC( result ), "tp_dealloc returned exception" );
    } else {
        ypObject_REFCNT( x ) -= 1;
        yp_DEBUG( "decref: 0x%08X refcnt %d", x, ypObject_REFCNT( x ) );
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

// _ypIterObject_HEAD shared with friendly classes below
#define _ypIterObject_HEAD_NO_PADDING \
    ypObject_HEAD \
    yp_int32_t ob_lenhint; \
    yp_uint32_t ob_objlocs; \
    yp_generator_func_t ob_func;
// To ensure that ob_inline_data is aligned properly, we need to pad on some platforms
// TODO If we use ob_len to store the lenhint, yp_lenC would have to always call tp_len, but then
// we could trim 8 bytes off all iterators
#if SIZE_MAX == 0xFFFFFFFFu // 32-bit (or less) platform
#define _ypIterObject_HEAD \
    _ypIterObject_HEAD_NO_PADDING \
    void *_ob_padding;
#else
#define _ypIterObject_HEAD _ypIterObject_HEAD_NO_PADDING
#endif
typedef struct {
    _ypIterObject_HEAD
    yp_INLINE_DATA( yp_uint8_t );
} ypIterObject;
yp_STATIC_ASSERT( offsetof( ypIterObject, ob_inline_data ) % yp_MAX_ALIGNMENT == 0, alignof_iter_inline_data );

#define ypIter_STATE( i )       ( ((ypObject *)i)->ob_data )
#define ypIter_STATE_SIZE( i )  ( ((ypObject *)i)->ob_alloclen )
#define ypIter_LENHINT( i )     ( ((ypIterObject *)i)->ob_lenhint )
// ob_objlocs: bit n is 1 if (n*sizeof(ypObject *)) is the offset of an object in state
#define ypIter_OBJLOCS( i )     ( ((ypIterObject *)i)->ob_objlocs )
#define ypIter_FUNC( i )        ( ((ypIterObject *)i)->ob_func )

// Iterator methods

static ypObject *iter_traverse( ypObject *i, visitfunc visitor, void *memo )
{
    ypObject **p = (ypObject **) ypIter_STATE( i );
    yp_uint32_t locs = ypIter_OBJLOCS( i );
    ypObject *result;

    while( locs ) { // while there are still more objects to be found...
        if( locs & 0x1u ) {
            result = visitor( *p, memo );
            if( yp_isexceptionC( result ) ) return result;
        }
        p += 1;
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
    NotImplemented_comparefunc,     // tp_lt
    NotImplemented_comparefunc,     // tp_le
    NotImplemented_comparefunc,     // tp_eq
    NotImplemented_comparefunc,     // tp_ne
    NotImplemented_comparefunc,     // tp_ge
    NotImplemented_comparefunc,     // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    iter_close,                     // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    iter_miniiter,                  // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    iter_miniiter_next,             // tp_miniiter_next
    iter_miniiter_lenhint,          // tp_miniiter_lenhint
    iter_iter,                      // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    iter_send,                      // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

// Public functions

yp_ssize_t yp_iter_lenhintC( ypObject *i, ypObject **exc )
{
    yp_ssize_t lenhint;
    if( ypObject_TYPE_PAIR_CODE( i ) != ypFrozenIter_CODE ) {
        return_yp_CEXC_BAD_TYPE( 0, exc, i );
    }
    lenhint = ypIter_LENHINT( i );
    return lenhint < 0 ? 0 : lenhint;
}

// TODO what if the requested size was an input that we checked against the size of state
ypObject *yp_iter_stateCX( ypObject *i, void **state, yp_ssize_t *size )
{
    if( ypObject_TYPE_PAIR_CODE( i ) != ypFrozenIter_CODE ) {
        *state = NULL;
        *size = 0;
        return_yp_BAD_TYPE( i );
    }
    *state = ypIter_STATE( i );
    *size = ypIter_STATE_SIZE( i );
    return yp_None;
}

// TODO Double-check and test the boundary conditions in this function
// XXX iterable may be an yp_ONSTACK_ITER_*: use carefully
// XXX Yes, Python also allows unpacking of non-sequence iterables: a,b,c={1,2,3} is valid
void yp_unpackN( ypObject *iterable, int n, ... )
{
    yp_uint64_t mi_state;
    ypObject *mi;
    va_list args;
    int remaining;
    ypObject *x = yp_None; // set to None in case n==0
    ypObject **dest;

    // Set the given n arguments to the values yielded from iterable; if an exception occurs, we
    // will need to restart and discard these values.  Remember that if yp_miniiter fails,
    // yp_miniiter_next will return the same exception.
    // TODO Hmmm; let's say iterable was yp_StopIteration for some reason: this code would actually
    // succeed when n=0 even though it should probably fail...we should check the yp_miniiter
    // return (here and elsewhere)
    mi = yp_miniiter( iterable, &mi_state ); // new ref
    va_start( args, n );
    for( remaining = n; remaining > 0; remaining-- ) {
        x = yp_miniiter_next( mi, &mi_state ); // new ref
        if( yp_isexceptionC( x ) ) {
            // If the iterable is too short, raise yp_ValueError
            if( yp_isexceptionC2( x, yp_StopIteration ) ) x = yp_ValueError;
            break;
        }

        dest = va_arg( args, ypObject ** );
        *dest = x;
    }
    va_end( args );

    // If we've been successful so far, then ensure we're at the end of iterable
    if( !yp_isexceptionC( x ) ) {
        x = yp_miniiter_next( mi, &mi_state ); // new ref
        if( yp_isexceptionC2( x, yp_StopIteration ) ) {
            x = yp_None; // success!
        } else if( yp_isexceptionC( x ) ) {
            // some other exception occured
        } else {
            // If the iterable is too long, raise yp_ValueError
            yp_decref( x );
            x = yp_ValueError;
        }
    }

    // If an error occured above, then we need to discard the previously-yielded values and set
    // all dests to the exception; otherwise, we're successful, so return
    if( yp_isexceptionC( x ) ) {
        va_start( args, n );
        for( /*n already set*/; n > remaining; n-- ) {
            dest = va_arg( args, ypObject ** );
            yp_decref( *dest );
            *dest = x;
        }
        for( /*n already set*/; n > 0; n-- ) {
            dest = va_arg( args, ypObject ** );
            *dest = x;
        }
        va_end( args );
    }
    yp_decref( mi );
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
    return_yp_V_FUNC( ypObject *, yp_generator_fromstructCNV,
            (func, lenhint, state, size, n, args), n );
}
ypObject *yp_generator_fromstructCNV( yp_generator_func_t func, yp_ssize_t lenhint,
        void *state, yp_ssize_t size, int n, va_list args )
{
    ypObject *i;
    yp_uint32_t objlocs = 0x0u;
    yp_ssize_t objoffset;
    yp_ssize_t objloc_index;

    if( size < 0 ) return yp_ValueError;

    // Determine the location of the objects.  There are a few errors the user could make:
    //  - an offset for a ypObject* that is at least partially outside of state; ignore these
    //  - an unaligned ypObject*, which isn't currently allowed and should never happen
    //  - a larger offset than we can represent with objlocs: a current limitation of nohtyP
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
    i = ypMem_MALLOC_CONTAINER_INLINE( ypIterObject, ypIter_CODE, size );
    if( yp_isexceptionC( i ) ) return i;

    // Set attributes, increment reference counts, and return
    i->ob_len = ypObject_LEN_INVALID;
    ypIter_LENHINT( i ) = lenhint;
    ypIter_FUNC( i ) = func;
    memcpy( ypIter_STATE( i ), state, size );
    ypIter_STATE_SIZE( i ) = size;
    ypIter_OBJLOCS( i ) = objlocs;
    iter_traverse( i, _iter_constructing_visitor, NULL ); // never fails
    return i;
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
    ypIter_STATE( i ) = &(ypMiIter_MI( i ));
    ypIter_STATE_SIZE( i ) = sizeof( ypMiIterObject ) - offsetof( ypMiIterObject, mi );
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

// XXX Note that yp_miniiter_lenhintC checks for negative hints and returns zero instead
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
// objects: they're allocated on the stack.  While this could be dangerous, it also reduces
// duplicating code between versions that handle va_args and those that handle iterables.
// TODO Is there a way to improve this using miniiters and a yp_iter_valist immortal object?
#define ypIterValist_ARGS( i ) (*((va_list *) ((ypObject *)i)->ob_data))

// The number of arguments is stored in ob_lenhint, which is automatically decremented by yp_next
// after each yielded value.
#define yp_ONSTACK_ITER_VALIST( name, n, args ) \
    ypIterObject _ ## name ## _struct = { \
        yp_IMMORTAL_HEAD_INIT( ypIter_CODE, &(args), ypObject_LEN_INVALID ), \
        n, 0x0u, _iter_valist_generator}; \
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
// TODO Is there a way to improve this using miniiters and a yp_iter_kvalist immortal object?
#define ypIterKValist_SUBITER( i ) (*((ypObject **) ((ypObject *)i)->ob_data))

// Don't include subiter in ob_objlocs
#define yp_ONSTACK_ITER_KVALIST( name, n, args ) \
    ypIterObject _ ## name ## _subiter_struct = { \
        yp_IMMORTAL_HEAD_INIT( ypIter_CODE, &(args), ypObject_LEN_INVALID ), \
        0, 0x0u, _iter_closed_generator, args}; \
    ypObject * const _ ## name ## _subiter = (ypObject *) &_ ## name ## _subiter_struct; \
    ypIterObject _ ## name ## _struct = { \
        yp_IMMORTAL_HEAD_INIT( ypIter_CODE, &_ ## name ## _subiter, ypObject_LEN_INVALID ), \
        n, 0x0u, _iter_kvalist_generator}; \
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
    yp_ASSERT( newType != NULL, "all types should have an immutable counterpart" );

    // Freeze the object, cache the final hash (via yp_hashC), possibly reduce memory usage, etc
    exc = newType->tp_freeze( x );
    if( yp_isexceptionC( exc ) ) return exc;
    ypObject_SET_TYPE_CODE( x, newCode );
    x->ob_hash = ypObject_HASH_INVALID; // just in case
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
    ypObject *exc = yp_None;
    ypObject *memo = (ypObject *) _memo;
    ypObject *id;
    ypObject *result;

    // Avoid recursion: we only have to visit each object once
    id = yp_intC( (yp_ssize_t) x );
    yp_pushuniqueE( memo, id, &exc );
    yp_decref( id );
    if( yp_isexceptionC( exc ) ) {
        if( yp_isexceptionC2( exc, yp_KeyError ) ) return yp_None; // already in set
        return exc;
    }

    // Freeze current object before going deep
    // XXX tp_traverse must propagate exceptions returned by visitor
    result = _yp_freeze( x );
    if( yp_isexceptionC( result ) ) return result;
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
// TODO It'd be nice to share code with yp_deepcopy_visitor
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
// TODO It'd be nice to share code with yp_deepcopy_visitor
static ypObject *_yp_frozen_deepcopy( ypObject *x, void *memo ) {
    // TODO don't forget to discard the new objects on error
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

// Use this as the visitor for deep copies (copying exactly the same types)
// XXX Remember: deep copies always copy everything except hashable (immutable) immortals...and
// maybe even those should be copied as well...or just those that contain other objects
static ypObject *yp_deepcopy_visitor( ypObject *x, void *_memo ) {
    ypObject **memo = (ypObject **) _memo;
    ypObject *id;
    ypObject *result;

    // Avoid recursion: we only make one copy of each object
    id = yp_intC( (yp_ssize_t) x ); // new ref
    result = yp_getitem( *memo, id );
    if( !yp_isexceptionC( result ) ) {
        yp_decref( id );
        return result; // we've already made a copy of this object; return it
    } else if( !yp_isexceptionC2( result, yp_KeyError ) ) {
        yp_decref( id );
        return result; // some (unexpected) error occured
    }

    // If we get here, then this is the first time visiting this object
    if( ypObject_IS_MUTABLE( x ) ) {
        result = ypObject_TYPE( x )->tp_unfrozen_copy( x, yp_deepcopy_visitor, memo );
    } else {
        result = ypObject_TYPE( x )->tp_frozen_copy( x, yp_deepcopy_visitor, memo );
    }
    if( yp_isexceptionC( result ) ) {
        yp_decref( id );
        return result;
    }
    yp_setitem( memo, id, result );
    yp_decref( id );
    if( yp_isexceptionC( *memo ) ) return *memo;
    return result;
}

ypObject *yp_deepcopy( ypObject *x ) {
    ypObject *memo = yp_dictK( 0 );
    ypObject *result = yp_deepcopy_visitor( x, &memo );
    yp_decref( memo );
    return result;
}

// TODO CONTAINER_INLINE objects won't release any of their memory on invalidation.  This is a 
// tradeoff in the interests of reducing individual allocations.  Perhaps there should be a limit
// on how large to make CONTAINER_INLINE objects, or perhaps we should try to shrink the
// invalidated object in-place (if supported by the heap).
void yp_invalidate( ypObject **x )
{
    // TODO implement
}

void yp_deepinvalidate( ypObject **x )
{
    // TODO implement
}


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
#define ypBool_NOT( b ) ( (b) == yp_True ? yp_False : \
                         ((b) == yp_False ? yp_True : (b)))

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
    return_yp_V_FUNC( ypObject *, yp_orNV, (n, args), n );
}
ypObject *yp_orNV( int n, va_list args )
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
    return_yp_V_FUNC( ypObject *, yp_anyNV, (n, args), n );
}
ypObject *yp_anyNV( int n, va_list args ) {
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
    return_yp_V_FUNC( ypObject *, yp_andNV, (n, args), n );
}
ypObject *yp_andNV( int n, va_list args )
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
    return_yp_V_FUNC( ypObject *, yp_allNV, (n, args), n );
}
ypObject *yp_allNV( int n, va_list args ) {
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
extern ypObject * const yp_ComparisonNotImplemented;
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
yp_STATIC_ASSERT( ypObject_HASH_INVALID == -1, hash_invalid_is_neg_one );

extern ypObject * const yp_RecursionLimitError;
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
    // XXX We can't record the cached hash here: consider that yp_hash on a tuple with mutable
    // objects cannot succeed, but we (ie yp_currenthash) _can_
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
    yp_ASSERT( len >= 0, "tp_len cannot return negative" );
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
    InvalidatedError_objvalistproc,     // tp_update

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
    ExceptionMethod_objvalistproc,      // tp_update

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

#define _yp_IMMORTAL_EXCEPTION_SUPERPTR( name, superptr ) \
    yp_IMMORTAL_STR_LATIN1( name ## _name, #name ); \
    static ypExceptionObject _ ## name ## _struct = { \
        yp_IMMORTAL_HEAD_INIT( ypException_CODE, NULL, 0 ), \
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

// yp_isexceptionC defined above

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
 * Types
 *************************************************************************************************/

static ypObject *type_frozen_copy( ypObject *t, visitfunc copy_visitor, void *copy_memo ) {
    return yp_incref( t );
}

static ypObject *type_bool( ypObject *t ) {
    return yp_True;
}

static ypTypeObject ypType_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    MethodError_objproc,            // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    type_frozen_copy,               // tp_unfrozen_copy
    type_frozen_copy,               // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    type_bool,                      // tp_bool
    NotImplemented_comparefunc,     // tp_lt
    NotImplemented_comparefunc,     // tp_le
    NotImplemented_comparefunc,     // tp_eq
    NotImplemented_comparefunc,     // tp_ne
    NotImplemented_comparefunc,     // tp_ge
    NotImplemented_comparefunc,     // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};


/*************************************************************************************************
 * None
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

static ypObject *nonetype_frozen_copy( ypObject *n, visitfunc copy_visitor, void *copy_memo ) {
    return yp_None;
}

ypObject *nonetype_bool( ypObject *n ) {
    return yp_False;
}

static ypObject *nonetype_currenthash( ypObject *n,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash ) {
    // Since we never contain mutable objects, we can cache our hash
    *hash = ypObject_CACHED_HASH( yp_None ) = yp_HashPointer( yp_None );
    return yp_None;
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
    nonetype_frozen_copy,           // tp_unfrozen_copy
    nonetype_frozen_copy,           // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    nonetype_bool,                  // tp_bool
    NotImplemented_comparefunc,     // tp_lt
    NotImplemented_comparefunc,     // tp_le
    NotImplemented_comparefunc,     // tp_eq
    NotImplemented_comparefunc,     // tp_ne
    NotImplemented_comparefunc,     // tp_ge
    NotImplemented_comparefunc,     // tp_gt

    // Generic object operations
    nonetype_currenthash,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

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

typedef struct {
    ypObject_HEAD
    char value;
} ypBoolObject;
#define _ypBool_VALUE( b ) ( ((ypBoolObject *)b)->value )

static ypObject *bool_frozen_copy( ypObject *b, visitfunc copy_visitor, void *copy_memo ) {
    return b;
}

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
    bool_frozen_copy,               // tp_unfrozen_copy
    bool_frozen_copy,               // tp_frozen_copy
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
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

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

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

// struct _ypIntObject is declared in nohtyP.h for use by yp_IMMORTAL_INT
typedef struct _ypIntObject ypIntObject;
#define ypInt_VALUE( i ) ( ((ypIntObject *)i)->value )

// Arithmetic code depends on both int and float particulars being defined first
typedef struct {
    ypObject_HEAD
    yp_float_t value;
} ypFloatObject;
#define ypFloat_VALUE( f ) ( ((ypFloatObject *)f)->value )

// Signatures of some specialized arithmetic functions
typedef yp_int_t (*arithLfunc)( yp_int_t, yp_int_t, ypObject ** );
typedef yp_float_t (*arithLFfunc)( yp_float_t, yp_float_t, ypObject ** );
typedef void (*iarithCfunc)( ypObject **, yp_int_t );
typedef void (*iarithCFfunc)( ypObject **, yp_float_t );
typedef yp_int_t (*unaryLfunc)( yp_int_t, ypObject ** );
typedef yp_float_t (*unaryLFfunc)( yp_float_t, ypObject ** );

// Bitwise operations on floats aren't supported, so these functions simply raise yp_TypeError
static void yp_ilshiftCF( ypObject **x, yp_float_t y );
static void yp_irshiftCF( ypObject **x, yp_float_t y );
static void yp_iampCF( ypObject **x, yp_float_t y );
static void yp_ixorCF( ypObject **x, yp_float_t y );
static void yp_ibarCF( ypObject **x, yp_float_t y );
static yp_float_t yp_lshiftLF( yp_float_t x, yp_float_t y, ypObject **exc );
static yp_float_t yp_rshiftLF( yp_float_t x, yp_float_t y, ypObject **exc );
static yp_float_t yp_ampLF( yp_float_t x, yp_float_t y, ypObject **exc );
static yp_float_t yp_xorLF( yp_float_t x, yp_float_t y, ypObject **exc );
static yp_float_t yp_barLF( yp_float_t x, yp_float_t y, ypObject **exc );
static yp_float_t yp_invertLF( yp_float_t x, ypObject **exc );

// Public, immortal objects
yp_IMMORTAL_INT( yp_sys_maxint, yp_INT_T_MAX );
yp_IMMORTAL_INT( yp_sys_minint, yp_INT_T_MIN );

// XXX Adapted from Python's _PyLong_DigitValue
/* Table of digit values for 8-bit string -> integer conversion.
 * '0' maps to 0, ..., '9' maps to 9.
 * 'a' and 'A' map to 10, ..., 'z' and 'Z' map to 35.
 * All other indices map to 37.
 * Note that when converting a base B string, a yp_uint8_t c is a legitimate
 * base B digit iff _ypInt_digit_value[c] < B.
 */
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

// XXX Will fail if non-ascii bytes are passed in, so safe to call on latin-1 data
static yp_int_t _yp_mulL_posints( yp_int_t x, yp_int_t y );
static ypObject *_ypInt_from_ascii( ypObject *(*allocator)( yp_int_t ), yp_uint8_t *bytes,
        yp_int_t base )
{
    int sign;
    yp_int_t result;
    yp_int_t digit;

    // Skip leading whitespace
    while( 1 ) {
        if( *bytes == '\0' ) return yp_ValueError;
        if( !yp_ISSPACE( *bytes ) ) break;
        bytes++;
    }

    // We're pointing to a non-whitespace character; see if there's a sign we should consume
    if( *bytes == '+' ) {
        sign = 1;
        bytes++;
    } else if( *bytes == '-' ) {
        sign = -1;
        bytes++;
    } else {
        sign = 1;
    }

    // We could be pointing to anything; determine if any prefix agrees with base
    if( *bytes == '0' ) {
        // We can safely consume this leading zero in all cases, as it cannot change the value
        bytes++;
        if( *bytes == '\0' || yp_ISSPACE( *bytes ) ) {
            // We've just parsed the string b"0", b"  0  ", etc; take a shortcut
            result = 0;
            goto endofdigits;
        } else if( *bytes == 'b' || *bytes == 'B' ) {
            if( base == 0 ) base = 2;
            if( base == 2 ) bytes++; // consume this character only if the base agrees
        } else if( *bytes == 'o' || *bytes == 'O' ) {
            if( base == 0 ) base = 8;
            if( base == 8 ) bytes++; // (ditto)
        } else if( *bytes == 'x' || *bytes == 'X' ) {
            if( base == 0 ) base = 16;
            if( base == 16 ) bytes++; // (ditto)
        } else {
            // Leading zeroes are allowed if the value is zero, regardless of base (ie "00000");
            // once again, it's always safe to consume leading zeroes
            while( *bytes == '0' ) bytes++;
            if( *bytes == '\0' || yp_ISSPACE( *bytes ) ) {
                result = 0;
                goto endofdigits;
            }
            // We're now pointing to the first non-zero digit
            // For non-zero values, leading zeroes are not allowed when base is zero
            if( base == 0 ) return yp_ValueError;
        }
    }
    if( base == 0 ) {
        base = 10; // a Python literal without a prefix is base 10
    } else if( base < 2 || base > 36 ) {
        return yp_ValueError;
    }

    // We could be pointing to anything; make sure there's at least one, valid digit.
    // _ypInt_digit_value[*bytes]>=base ensures we stop at the null-terminator, whitespace, and
    // invalid characters.
    digit = _ypInt_digit_value[*bytes];
    if( digit >= base ) return yp_ValueError;
    bytes++;
    result = digit;
    while( 1 ) {
        digit = _ypInt_digit_value[*bytes];
        if( digit >= base ) goto endofdigits;
        bytes++;

        result = _yp_mulL_posints( result, base );
        if( result < 0 ) {
            // If adding digit would not change the value, then check for the yp_INT_T_MIN
            // case; otherwise, adding digit would definitely overflow
            if( digit == 0 ) goto checkforintmin;
            return yp_OverflowError;
        }

        result = yp_UINT_MATH( result, +, digit );
        if( result < 0 ) goto checkforintmin;
    }

    // Ensure there's only whitespace left in the string, and return the new integer
endofdigits:
    while( 1 ) {
        if( *bytes == '\0' ) break;
        if( !yp_ISSPACE( *bytes ) ) return yp_ValueError;
        bytes++;
    }
    return allocator( sign * result );

checkforintmin:
    // If we overflowed to exactly yp_INT_T_MIN, and our result is supposed to be negative,
    // and there are no more digits, then we've decoded yp_INT_T_MIN
    if( result == yp_INT_T_MIN && sign < 0 && _ypInt_digit_value[*bytes] >= base ) {
        sign = 1; // result is already negative
        goto endofdigits;
    }
    return yp_OverflowError;
}


// Public Methods

static ypObject *int_dealloc( ypObject *i ) {
    ypMem_FREE_FIXED( i );
    return yp_None;
}

static ypObject *int_unfrozen_copy( ypObject *i, visitfunc copy_visitor, void *copy_memo ) {
    return yp_intstoreC( ypInt_VALUE( i ) );
}

static ypObject *int_frozen_copy( ypObject *i, visitfunc copy_visitor, void *copy_memo ) {
    return yp_intC( ypInt_VALUE( i ) );
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
// TODO Move this to a _yp_HashLong function?
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
    int_unfrozen_copy,              // tp_unfrozen_copy
    int_frozen_copy,                // tp_frozen_copy
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
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

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
    int_unfrozen_copy,              // tp_unfrozen_copy
    int_frozen_copy,                // tp_frozen_copy
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
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

// XXX Adapted from Python 2.7's int_add
yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    yp_int_t result = yp_UINT_MATH( x, +, y );
    if( (result^x) < 0 && (result^y) < 0 ) {
        return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
    }
    return result;
}

// XXX Adapted from Python 2.7's int_sub
yp_int_t yp_subL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    yp_int_t result = yp_UINT_MATH( x, -, y );
    if( (result^x) < 0 && (result^~y) < 0 ) {
        return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
    }
    return result;
}

// Special case of yp_mulL when one of the operands is yp_INT_T_MIN
static yp_int_t _yp_mulL_minint( yp_int_t y, ypObject **exc )
{
    // When multiplying yp_INT_T_MIN, there are only two values of y that won't overflow
    if( y == 0 ) return 0;
    if( y == 1 ) return yp_INT_T_MIN;
    return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
}

// Special case of yp_mulL where both x and y are positive, or zero.  Returns yp_INT_T_MIN if
// x*y overflows to _exactly_ that value; it will be up to the caller to determine if this is
// a valid result.  All other overflows will return a negative number strictly larger than
// yp_INT_T_MIN.
static yp_int_t _yp_mulL_posints( yp_int_t x, yp_int_t y )
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
    const yp_int_t num_bits_halved = sizeof( yp_int_t ) / 2 * 8;
    const yp_int_t bit_mask_halved = (1ull << num_bits_halved) - 1ull;
    yp_int_t result_hi, result_lo;

    // Split x and y into high and low halves
    yp_int_t x_hi = yp_UINT_MATH( x, >>, num_bits_halved );
    yp_int_t x_lo = yp_UINT_MATH( x, &, bit_mask_halved );
    yp_int_t y_hi = yp_UINT_MATH( y, >>, num_bits_halved );
    yp_int_t y_lo = yp_UINT_MATH( y, &, bit_mask_halved );

    // Determine the intermediate value of the high-part of the result
    if( x_hi == 0 ) {
        // We can take a shortcut in the case of x and y being small values
        if( y_hi == 0 ) return yp_UINT_MATH( x_lo, *, y_lo );
        result_hi = yp_UINT_MATH( x_lo, *, y_hi );
    } else if( y_hi == 0 ) {
        result_hi = yp_UINT_MATH( x_hi, *, y_lo );
    } else {
        return -1; // overflow
    }

    // Shift the intermediate high-part of the result, checking for overflow
    if( result_hi & ~bit_mask_halved ) return -1; // overflow
    result_hi = yp_UINT_MATH( result_hi, <<, num_bits_halved );
    if( result_hi < 0 ) {
        // If adding x_lo*y_lo would not change result_hi, then we may be dealing with
        // yp_INT_T_MIN, so return result_hi unchanged; otherwise, we would definitely overflow,
        // so return -1
        if( x_lo == 0 || y_lo == 0 ) return result_hi;
        return -1;
    }

    // Finally, add the low-part of the result to the high-part, check for overflow, and return;
    // the caller checks for overflow on the final addition
    result_lo = yp_UINT_MATH( x_lo, *, y_lo );
    if( result_lo < 0 ) return -1; // overflow
    return yp_UINT_MATH( result_hi, +, result_lo );
}

// Python 2.7's int_mul uses the phrase "close enough", which scares me.  I prefer the method
// described at http://www.fefe.de/intof.html, although it requires abs(x) and abs(y), meaning
// we need to handle yp_INT_T_MIN specially because we can't negate it.
yp_int_t yp_mulL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    yp_int_t result, sign = 1;

    if( x == yp_INT_T_MIN ) return _yp_mulL_minint( y, exc );
    if( y == yp_INT_T_MIN ) return _yp_mulL_minint( x, exc );
    if( x < 0 ) { sign = -sign; x = -x; }
    if( y < 0 ) { sign = -sign; y = -y; }

    // If we overflow to exactly yp_INT_T_MIN, and our result is supposed to be negative, then
    // we've calculated yp_INT_T_MIN; all other overflows are errors
    result = _yp_mulL_posints( x, y );
    if( result < 0 ) {
        if( result == yp_INT_T_MIN && sign < 0 ) return yp_INT_T_MIN;
        return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
    }
    return sign * result;
}

// XXX Operands are fist converted to float, then divided; result always a float
yp_float_t yp_truedivL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    ypObject *subexc = yp_None;
    yp_float_t x_asfloat, y_asfloat;

    x_asfloat = yp_asfloatL( x, &subexc );
    if( yp_isexceptionC( subexc ) ) return_yp_CEXC_ERR( 0.0, exc, subexc );
    y_asfloat = yp_asfloatL( y, &subexc );
    if( yp_isexceptionC( subexc ) ) return_yp_CEXC_ERR( 0.0, exc, subexc );
    return yp_truedivLF( x_asfloat, y_asfloat, exc );
}

yp_int_t yp_floordivL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    yp_int_t div, mod;
    yp_divmodL( x, y, &div, &mod, exc );
    return div;
}

yp_int_t yp_modL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    yp_int_t div, mod;
    yp_divmodL( x, y, &div, &mod, exc );
    return mod;
}

// XXX Adapted from Python 2.7's i_divmod
void yp_divmodL( yp_int_t x, yp_int_t y, yp_int_t *_div, yp_int_t *_mod, ypObject **exc )
{
    yp_int_t xdivy, xmody;

    if (y == 0) {
        *exc = yp_ZeroDivisionError;
        goto error;
    }
    /* (-sys.maxint-1)/-1 is the only overflow case. */
    if (y == -1 && x == yp_INT_T_MIN) {
        *exc = yp_OverflowError;
        goto error;
    }

    xdivy = x / y;
    /* xdiv*y can overflow on platforms where x/y gives floor(x/y)
     * for x and y with differing signs. (This is unusual
     * behaviour, and C99 prohibits it, but it's allowed by C89;
     * for an example of overflow, take x = LONG_MIN, y = 5 or x =
     * LONG_MAX, y = -5.)  However, x - xdivy*y is always
     * representable as a long, since it lies strictly between
     * -abs(y) and abs(y).  We add casts to avoid intermediate
     * overflow.
     */
    xmody = yp_UINT_MATH( x, -, yp_UINT_MATH( xdivy, *, y ) );
    /* If the signs of x and y differ, and the remainder is non-0,
     * C89 doesn't define whether xdivy is now the floor or the
     * ceiling of the infinitely precise quotient.  We want the floor,
     * and we have it iff the remainder's sign matches y's.
     */
    if (xmody && ((y ^ xmody) < 0) /* i.e. and signs differ */) {
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

yp_int_t yp_powL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    return yp_powL3( x, y, 0, exc );
}

// XXX Adapted from Python 2.7's int_pow
yp_int_t yp_powL3( yp_int_t x, yp_int_t y, yp_int_t z, ypObject **exc )
{
    yp_int_t result, temp, prev;
    if (y < 0) {
        // XXX A negative exponent means a float should be returned, which we can't do here, so
        // this is handled at higher levels
        return_yp_CEXC_ERR( 0, exc, yp_ValueError );
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
        prev = result;              /* Save value for overflow check */
        if (y & 1) {
            /*
             * The (unsigned long) cast below ensures that the multiplication
             * is interpreted as an unsigned operation rather than a signed one
             * (C99 6.3.1.8p1), thus avoiding the perils of undefined behaviour
             * from signed arithmetic overflow (C99 6.5p5).  See issue #12973.
             */
            result = yp_UINT_MATH(result, *, temp);
            if (temp == 0)
                break; /* Avoid result / 0 */
            if (result / temp != prev) {
                return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
            }
        }
        y >>= 1;               /* Shift exponent down by 1 bit */
        if (y==0) break;
        prev = temp;
        temp = yp_UINT_MATH(temp, *, temp);  /* Square the value of temp */
        if (prev != 0 && temp / prev != prev) {
            return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
        }
        if (z) {
            /* If we did a multiplication, perform a modulo */
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
yp_STATIC_ASSERT( (-1ll >> 1) == -1ll, right_shift_sign_extends );

// XXX Adapted from Python 2.7's int_lshift
yp_int_t yp_lshiftL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    yp_int_t result;
    if( y < 0 ) return_yp_CEXC_ERR( 0, exc, yp_ValueError ); // negative shift count
    if( x == 0 ) return x; // 0 can be shifted by 50 million bits for all we care
    if( y >= (sizeof( yp_int_t ) * 8) ) return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
    result = x << y;
    if( x != (result >> y) ) return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
    return result;
}

// XXX Adapted from Python 2.7's int_rshift
yp_int_t yp_rshiftL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    if( y < 0 ) return_yp_CEXC_ERR( 0, exc, yp_ValueError ); // negative shift count
    if( y >= (sizeof( yp_int_t ) * 8) ) {
        return x < 0 ? -1 : 0;
    }
    return x >> y;
}

yp_int_t yp_ampL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    return x & y;
}

yp_int_t yp_xorL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    return x ^ y;
}

yp_int_t yp_barL( yp_int_t x, yp_int_t y, ypObject **exc )
{
    return x | y;
}

// XXX Adapted from Python 2.7's int_neg
yp_int_t yp_negL( yp_int_t x, ypObject **exc )
{
    if( x == yp_INT_T_MIN ) return_yp_CEXC_ERR( 0, exc, yp_OverflowError );
    return -x;
}

yp_int_t yp_posL( yp_int_t x, ypObject **exc )
{
    return x;
}

yp_int_t yp_absL( yp_int_t x, ypObject **exc )
{
    if( x < 0 ) return yp_negL( x, exc );
    return x;
}

yp_int_t yp_invertL( yp_int_t x, ypObject **exc )
{
    return ~x;
}

// XXX Overloading of add/etc currently not supported
static void iarithmeticC( ypObject **x, yp_int_t y, arithLfunc intop, iarithCFfunc floatop )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;

    if( x_pair == ypInt_CODE ) {
        yp_int_t result = intop( ypInt_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypInt_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_intC( result );
        }
        return;

    } else if( x_pair == ypFloat_CODE ) {
        yp_float_t y_asfloat = yp_asfloatL( y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        floatop( x, y_asfloat );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, *x );
}

static void iarithmetic( ypObject **x, ypObject *y, iarithCfunc intop, iarithCFfunc floatop )
{
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    ypObject *exc = yp_None;

    if( y_pair == ypInt_CODE ) {
        intop( x, ypInt_VALUE( y ) );
        return;

    } else if( y_pair == ypFloat_CODE ) {
        floatop( x, ypFloat_VALUE( y ) );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, y );
}

static ypObject *arithmetic_intop( yp_int_t x, yp_int_t y, arithLfunc intop,
        int result_mutable )
{
    ypObject *exc = yp_None;
    yp_int_t result = intop( x, y, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    if( result_mutable ) return yp_intstoreC( result );
    return yp_intC( result );
}
static ypObject *arithmetic_floatop( yp_float_t x, yp_float_t y, arithLFfunc floatop,
        int result_mutable )
{
    ypObject *exc = yp_None;
    yp_float_t result = floatop( x, y, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    if( result_mutable ) return yp_floatstoreCF( result );
    return yp_floatCF( result );
}
static ypObject *arithmetic( ypObject *x, ypObject *y, arithLfunc intop, arithLFfunc floatop )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    int result_mutable = ypObject_IS_MUTABLE( x );
    ypObject *exc = yp_None;

    // Coerce the numeric operands to a common type
    if( y_pair == ypInt_CODE ) {
        if( x_pair == ypInt_CODE ) {
            return arithmetic_intop( ypInt_VALUE( x ), ypInt_VALUE( y ), intop, result_mutable );
        } else if( x_pair == ypFloat_CODE ) {
            yp_float_t y_asfloat = yp_asfloatC( y, &exc );
            if( yp_isexceptionC( exc ) ) return exc;
            return arithmetic_floatop( ypFloat_VALUE( x ), y_asfloat, floatop, result_mutable );
        } else {
            return_yp_BAD_TYPE( x );
        }
    } else if( y_pair == ypFloat_CODE ) {
        if( x_pair == ypInt_CODE ) {
            yp_float_t x_asfloat = yp_asfloatC( x, &exc );
            if( yp_isexceptionC( exc ) ) return exc;
            return arithmetic_floatop( x_asfloat, ypFloat_VALUE( y ), floatop, result_mutable );
        } else if( x_pair == ypFloat_CODE ) {
            return arithmetic_floatop( ypFloat_VALUE( x ), ypFloat_VALUE( y ), floatop,
                    result_mutable );
        } else {
            return_yp_BAD_TYPE( x );
        }
    } else {
        return_yp_BAD_TYPE( y );
    }
}

// Defined here are yp_iaddC (et al), yp_iadd (et al), and yp_add (et al)
#define _ypInt_PUBLIC_ARITH_FUNCTION( name ) \
    void yp_i ## name ## C( ypObject **x, yp_int_t y ) { \
        iarithmeticC( x, y, yp_ ## name ## L, yp_i ## name ## CF ); \
    } \
    void yp_i ## name( ypObject **x, ypObject *y ) { \
        iarithmetic( x, y, yp_i ## name ## C, yp_i ## name ## CF ); \
    } \
    ypObject *yp_ ## name( ypObject *x, ypObject *y ) { \
        return arithmetic( x, y, yp_ ## name ## L, yp_ ## name ## LF ); \
    }
_ypInt_PUBLIC_ARITH_FUNCTION( add );
_ypInt_PUBLIC_ARITH_FUNCTION( sub );
_ypInt_PUBLIC_ARITH_FUNCTION( mul );
// truediv implemented separately, as result is always a float
_ypInt_PUBLIC_ARITH_FUNCTION( floordiv );
_ypInt_PUBLIC_ARITH_FUNCTION( mod );
// FIXME if pow has a negative exponent, the result must be a float
_ypInt_PUBLIC_ARITH_FUNCTION( pow );
_ypInt_PUBLIC_ARITH_FUNCTION( lshift );
_ypInt_PUBLIC_ARITH_FUNCTION( rshift );
_ypInt_PUBLIC_ARITH_FUNCTION( amp );
_ypInt_PUBLIC_ARITH_FUNCTION( xor );
_ypInt_PUBLIC_ARITH_FUNCTION( bar );

void yp_itruedivC( ypObject **x, yp_int_t y )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;

    if( x_pair == ypInt_CODE ) {
        yp_float_t result = yp_truedivL( ypInt_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_decref( *x );
        *x = yp_floatCF( result );
        return;

    } else if( x_pair == ypFloat_CODE ) {
        yp_float_t y_asfloat = yp_asfloatL( y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_itruedivCF( x, y_asfloat );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, *x );
}

void yp_itruediv( ypObject **x, ypObject *y )
{
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    ypObject *exc = yp_None;

    if( y_pair == ypInt_CODE ) {
        yp_itruedivC( x, ypInt_VALUE( y ) );
        return;

    } else if( y_pair == ypFloat_CODE ) {
        yp_itruedivCF( x, ypFloat_VALUE( y ) );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, y );
}

ypObject *yp_truediv( ypObject *x, ypObject *y )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    int result_mutable = ypObject_IS_MUTABLE( x );
    ypObject *exc = yp_None;
    yp_float_t result;

    // Coerce the numeric operands to a common type
    if( y_pair == ypInt_CODE ) {
        if( x_pair == ypInt_CODE ) {
            result = yp_truedivL( ypInt_VALUE( x ), ypInt_VALUE( y ), &exc );
        } else if( x_pair == ypFloat_CODE ) {
            yp_float_t y_asfloat = yp_asfloatC( y, &exc );
            if( yp_isexceptionC( exc ) ) return exc;
            result = yp_truedivLF( ypFloat_VALUE( x ), y_asfloat, &exc );
        } else {
            return_yp_BAD_TYPE( x );
        }
    } else if( y_pair == ypFloat_CODE ) {
        if( x_pair == ypInt_CODE ) {
            yp_float_t x_asfloat = yp_asfloatC( x, &exc );
            if( yp_isexceptionC( exc ) ) return exc;
            result = yp_truedivLF( x_asfloat, ypFloat_VALUE( y ), &exc );
        } else if( x_pair == ypFloat_CODE ) {
            result = yp_truedivLF( ypFloat_VALUE( x ), ypFloat_VALUE( y ), &exc );
        } else {
            return_yp_BAD_TYPE( x );
        }
    } else {
        return_yp_BAD_TYPE( y );
    }
    if( yp_isexceptionC( exc ) ) return exc;
    if( result_mutable ) return yp_floatstoreCF( result );
    return yp_floatCF( result );
}

static ypObject *_yp_divmod_ints( yp_int_t x, yp_int_t y, ypObject **div, ypObject **mod,
        int result_mutable )
{
    ypObject *exc = yp_None;
    ypObject *(*allocator)( yp_int_t ) = result_mutable ? yp_intstoreC : yp_intC;
    yp_int_t divC, modC;
    yp_divmodL( x, y, &divC, &modC, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    *div = allocator( divC ); // new ref
    if( yp_isexceptionC( *div ) ) return *div;
    *mod = allocator( modC ); // new ref
    if( yp_isexceptionC( *mod ) ) {
        yp_decref( *div );
        return *mod;
    }
    return yp_None;
}
static ypObject *_yp_divmod_floats( yp_float_t x, yp_float_t y, ypObject **div, ypObject **mod,
        int result_mutable  )
{
    ypObject *exc = yp_None;
    ypObject *(*allocator)( yp_float_t ) = result_mutable ? yp_floatstoreCF : yp_floatCF;
    yp_float_t divC, modC;
    yp_divmodLF( x, y, &divC, &modC, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    *div = allocator( divC ); // new ref
    if( yp_isexceptionC( *div ) ) return *div;
    *mod = allocator( modC ); // new ref
    if( yp_isexceptionC( *mod ) ) {
        yp_decref( *div );
        return *mod;
    }
    return yp_None;
}
static ypObject *_yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    int y_pair = ypObject_TYPE_PAIR_CODE( y );
    int result_mutable = ypObject_IS_MUTABLE( x );
    ypObject *exc = yp_None;

    // Coerce the numeric operands to a common type
    if( y_pair == ypInt_CODE ) {
        if( x_pair == ypInt_CODE ) {
            return _yp_divmod_ints( ypInt_VALUE( x ), ypInt_VALUE( y ), div, mod, result_mutable );
        } else if( x_pair == ypFloat_CODE ) {
            yp_float_t y_asfloat = yp_asfloatC( y, &exc );
            if( yp_isexceptionC( exc ) ) return exc;
            return _yp_divmod_floats( ypFloat_VALUE( x ), y_asfloat, div, mod, result_mutable );
        } else {
            return_yp_BAD_TYPE( x );
        }
    } else if( y_pair == ypFloat_CODE ) {
        if( x_pair == ypInt_CODE ) {
            yp_float_t x_asfloat = yp_asfloatC( x, &exc );
            if( yp_isexceptionC( exc ) ) return exc;
            return _yp_divmod_floats( x_asfloat, ypFloat_VALUE( y ), div, mod, result_mutable );
        } else if( x_pair == ypFloat_CODE ) {
            return _yp_divmod_floats( ypFloat_VALUE( x ), ypFloat_VALUE( y ), div, mod,
                    result_mutable );
        } else {
            return_yp_BAD_TYPE( x );
        }
    } else {
        return_yp_BAD_TYPE( y );
    }
}
void yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod ) {
    // Ensure both div and mod are set to an exception on error
    ypObject *result = _yp_divmod( x, y, div, mod );
    if( yp_isexceptionC( result ) ) {
        *div = *mod = result;
    }
}

static void iunaryoperation( ypObject **x, unaryLfunc intop, unaryLFfunc floatop )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;

    if( x_pair == ypInt_CODE ) {
        yp_int_t result = intop( ypInt_VALUE( *x ), &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( *x ) ) {
            ypInt_VALUE( *x ) = result;
        } else {
            if( result == ypInt_VALUE( *x ) ) return;
            yp_decref( *x );
            *x = yp_intC( result );
        }
        return;

    } else if( x_pair == ypFloat_CODE ) {
        yp_float_t result = floatop( ypFloat_VALUE( *x ), &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( *x ) ) {
            ypFloat_VALUE( *x ) = result;
        } else {
            if( result == ypFloat_VALUE( *x ) ) return;
            yp_decref( *x );
            *x = yp_floatCF( result );
        }
        return;

    } else {
        return_yp_INPLACE_BAD_TYPE( x, *x );
    }
}

static ypObject *unaryoperation( ypObject *x, unaryLfunc intop, unaryLFfunc floatop )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    ypObject *exc = yp_None;

    if( x_pair == ypInt_CODE ) {
        yp_int_t result = intop( ypInt_VALUE( x ), &exc );
        if( yp_isexceptionC( exc ) ) return exc;
        if( ypObject_IS_MUTABLE( x ) ) {
            return yp_intstoreC( result );
        } else {
            if( result == ypInt_VALUE( x ) ) return yp_incref( x );
            return yp_intC( result );
        }

    } else if( x_pair == ypFloat_CODE ) {
        yp_float_t result = floatop( ypFloat_VALUE( x ), &exc );
        if( yp_isexceptionC( exc ) ) return exc;
        if( ypObject_IS_MUTABLE( x ) ) {
            return yp_floatstoreCF( result );
        } else {
            if( result == ypFloat_VALUE( x ) ) return yp_incref( x );
            return yp_floatCF( result );
        }

    } else {
        return_yp_BAD_TYPE( x );
    }
}

// Defined here are yp_ineg (et al), and yp_neg (et al)
#define _ypInt_PUBLIC_UNARY_FUNCTION( name ) \
    void yp_i ## name( ypObject **x ) { \
        iunaryoperation( x, yp_ ## name ## L, yp_ ## name ## LF ); \
    } \
    ypObject *yp_ ## name( ypObject *x ) { \
        return unaryoperation( x, yp_ ## name ## L, yp_ ## name ## LF ); \
    }
_ypInt_PUBLIC_UNARY_FUNCTION( neg );
_ypInt_PUBLIC_UNARY_FUNCTION( pos );
_ypInt_PUBLIC_UNARY_FUNCTION( abs );
_ypInt_PUBLIC_UNARY_FUNCTION( invert );

// XXX Adapted from Python 2.7's bits_in_ulong
static const yp_uint8_t _BitLengthTable[32] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
};
yp_int_t yp_int_bit_lengthC( ypObject *x, ypObject **exc )
{
    yp_int_t x_abs;
    yp_int_t x_bits;

    if( ypObject_TYPE_PAIR_CODE( x ) != ypInt_CODE ) return_yp_CEXC_BAD_TYPE( 0, exc, x );

    x_abs = ypInt_VALUE( x );
    if( x_abs < 0 ) {
        if( x_abs == yp_INT_T_MIN ) return sizeof( yp_int_t ) * 8;
        x_abs = -x_abs;
    }

    x_bits = 0;
    while( x_abs >= 32 ) {
        x_bits += 6;
        x_abs >>= 6;
    }
    x_bits += _BitLengthTable[x_abs];
    return x_bits;
}


// Public constructors

// This pre-allocates an array of immortal ints for yp_intC to return
#define _ypInt_PREALLOC_START (-5)
#define _ypInt_PREALLOC_END   (257)
static ypIntObject _ypInt_pre_allocated[] = {
    #define _ypInt_PREALLOC( value ) \
        { yp_IMMORTAL_HEAD_INIT( ypInt_CODE, NULL, ypObject_LEN_INVALID ), (value) }
    _ypInt_PREALLOC( -5 ),
    _ypInt_PREALLOC( -4 ),
    _ypInt_PREALLOC( -3 ),
    _ypInt_PREALLOC( -2 ),
    _ypInt_PREALLOC( -1 ),

    // Allocates a range of 2, 4, etc starting at the given multiple of 2, 4, etc
    #define _ypInt_PREALLOC002( v ) _ypInt_PREALLOC(    v ), _ypInt_PREALLOC(    (v) | 0x01 )
    #define _ypInt_PREALLOC004( v ) _ypInt_PREALLOC002( v ), _ypInt_PREALLOC002( (v) | 0x02 )
    #define _ypInt_PREALLOC008( v ) _ypInt_PREALLOC004( v ), _ypInt_PREALLOC004( (v) | 0x04 )
    #define _ypInt_PREALLOC016( v ) _ypInt_PREALLOC008( v ), _ypInt_PREALLOC008( (v) | 0x08 )
    #define _ypInt_PREALLOC032( v ) _ypInt_PREALLOC016( v ), _ypInt_PREALLOC016( (v) | 0x10 )
    #define _ypInt_PREALLOC064( v ) _ypInt_PREALLOC032( v ), _ypInt_PREALLOC032( (v) | 0x20 )
    #define _ypInt_PREALLOC128( v ) _ypInt_PREALLOC064( v ), _ypInt_PREALLOC064( (v) | 0x40 )
    #define _ypInt_PREALLOC256( v ) _ypInt_PREALLOC128( v ), _ypInt_PREALLOC128( (v) | 0x80 )
    _ypInt_PREALLOC256( 0 ), // pre-allocates range( 256 )

    _ypInt_PREALLOC( 256 ),
};

ypObject * const yp_i_neg_one = (ypObject *) &(_ypInt_pre_allocated[-1 - _ypInt_PREALLOC_START]);
ypObject * const yp_i_zero    = (ypObject *) &(_ypInt_pre_allocated[ 0 - _ypInt_PREALLOC_START]);
ypObject * const yp_i_one     = (ypObject *) &(_ypInt_pre_allocated[ 1 - _ypInt_PREALLOC_START]);
ypObject * const yp_i_two     = (ypObject *) &(_ypInt_pre_allocated[ 2 - _ypInt_PREALLOC_START]);

ypObject *yp_intC( yp_int_t value )
{
    if( _ypInt_PREALLOC_START <= value && value < _ypInt_PREALLOC_END ) {
        return (ypObject *) &(_ypInt_pre_allocated[value - _ypInt_PREALLOC_START]);
    } else {
        ypObject *i = ypMem_MALLOC_FIXED( ypIntObject, ypInt_CODE );
        if( yp_isexceptionC( i ) ) return i;
        ypInt_VALUE( i ) = value;
        yp_DEBUG( "yp_intC: 0x%08X value %d", i, value );
        return i;
    }
}

ypObject *yp_intstoreC( yp_int_t value )
{
    ypObject *i = ypMem_MALLOC_FIXED( ypIntObject, ypIntStore_CODE );
    if( yp_isexceptionC( i ) ) return i;
    ypInt_VALUE( i ) = value;
    yp_DEBUG( "yp_intstoreC: 0x%08X value %d", i, value );
    return i;
}

// base is ignored if x is not a bytes or string
static ypObject *_ypInt( ypObject *(*allocator)( yp_int_t ), ypObject *x, yp_int_t base )
{
    ypObject *exc = yp_None;
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypInt_CODE ) {
        return allocator( ypInt_VALUE( x ) );
    } else if( x_pair == ypFloat_CODE ) {
        yp_int_t x_asint = yp_asintLF( ypFloat_VALUE( x ), &exc );
        if( yp_isexceptionC( exc ) ) return exc;
        return allocator( x_asint );
    } else if( x_pair == ypBool_CODE ) {
        return allocator( ypBool_IS_TRUE_C( x ) );
    } else if( x_pair == ypBytes_CODE ) {
        yp_uint8_t *bytes;
        ypObject *result = yp_asbytesCX( x, &bytes, NULL );
        if( yp_isexceptionC( result ) ) return yp_ValueError; // contains null bytes
        return _ypInt_from_ascii( allocator, bytes, base );
    } else if( x_pair == ypStr_CODE ) {
        // TODO Implement decoding
        yp_uint8_t *encoded;
        ypObject *encoding;
        ypObject *result = yp_asencodedCX( x, &encoded, NULL, &encoding );
        if( yp_isexceptionC( result ) ) return yp_ValueError; // contains null bytes
        if( encoding != yp_s_latin_1 ) return yp_NotImplementedError;
        return _ypInt_from_ascii( allocator, encoded, base );
    } else {
        return_yp_BAD_TYPE( x );
    }
}
ypObject *yp_int_baseC( ypObject *x, yp_int_t base ) {
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    if( x_pair != ypBytes_CODE && x_pair != ypStr_CODE ) return_yp_BAD_TYPE( x );
    return _ypInt( yp_intC, x, base );
}
ypObject *yp_intstore_baseC( ypObject *x, yp_int_t base ) {
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    if( x_pair != ypBytes_CODE && x_pair != ypStr_CODE ) return_yp_BAD_TYPE( x );
    return _ypInt( yp_intstoreC, x, base );
}
ypObject *yp_int( ypObject *x ) {
    if( ypObject_TYPE_CODE( x ) == ypInt_CODE ) return yp_incref( x );
    return _ypInt( yp_intC, x, 10 );
}
ypObject *yp_intstore( ypObject *x ) {
    return _ypInt( yp_intstoreC, x, 10 );
}

// Public conversion functions

yp_int_t yp_asintC( ypObject *x, ypObject **exc )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypInt_CODE ) {
        return ypInt_VALUE( x );
    } else if( x_pair == ypFloat_CODE ) {
        return yp_asintLF( ypFloat_VALUE( x ), exc );
    } else if( x_pair == ypBool_CODE ) {
        return ypBool_IS_TRUE_C( x );
    }
    return_yp_CEXC_BAD_TYPE( 0, exc, x );
}

// Defines the conversion functions.  Overflow checking is done by first truncating the value then
// seeing if it equals the stored value.  Note that when yp_asintC raises an exception, it returns
// zero, which can be represented in every integer type, so we won't override any yp_TypeError
// errors.
// TODO review http://blog.reverberate.org/2012/12/testing-for-integer-overflow-in-c-and-c.html
#define _ypInt_PUBLIC_AS_C_FUNCTION( name, mask ) \
yp_ ## name ## _t yp_as ## name ## C( ypObject *x, ypObject **exc ) { \
    yp_int_t asint = yp_asintC( x, exc ); \
    yp_ ## name ## _t retval = (yp_ ## name ## _t) (asint & (mask)); \
    if( (yp_int_t) retval != asint ) return_yp_CEXC_ERR( retval, exc, yp_OverflowError ); \
    return retval; \
}
_ypInt_PUBLIC_AS_C_FUNCTION( int8,   0xFF );
_ypInt_PUBLIC_AS_C_FUNCTION( uint8,  0xFFu );
_ypInt_PUBLIC_AS_C_FUNCTION( int16,  0xFFFF );
_ypInt_PUBLIC_AS_C_FUNCTION( uint16, 0xFFFFu );
_ypInt_PUBLIC_AS_C_FUNCTION( int32,  0xFFFFFFFF );
_ypInt_PUBLIC_AS_C_FUNCTION( uint32, 0xFFFFFFFFu );
#if SIZE_MAX <= 0xFFFFFFFFu // 32-bit (or less) platform
yp_STATIC_ASSERT( sizeof( yp_ssize_t ) < sizeof( yp_int_t ), sizeof_yp_ssize_lt_yp_int );
_ypInt_PUBLIC_AS_C_FUNCTION( ssize,  (yp_ssize_t) SIZE_MAX );
_ypInt_PUBLIC_AS_C_FUNCTION( hash,   (yp_hash_t) SIZE_MAX );
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

// TODO Python has PyFPE_START_PROTECT; we should be doing the same

// ypFloatObject and ypFloat_VALUE are defined above for use by the int code

static ypObject *float_dealloc( ypObject *f ) {
    ypMem_FREE_FIXED( f );
    return yp_None;
}

static ypObject *float_unfrozen_copy( ypObject *f, visitfunc copy_visitor, void *copy_memo ) {
    return yp_floatstoreCF( ypFloat_VALUE( f ) );
}

static ypObject *float_frozen_copy( ypObject *f, visitfunc copy_visitor, void *copy_memo ) {
    return yp_floatCF( ypFloat_VALUE( f ) );
}

static ypObject *float_bool( ypObject *f ) {
    return ypBool_FROM_C( ypFloat_VALUE( f ) != 0.0 );
}

// Here be float_lt, float_le, float_eq, float_ne, float_ge, float_gt
#define _ypFloat_RELATIVE_CMP_FUNCTION( name, operator ) \
    static ypObject *float_ ## name( ypObject *f, ypObject *x ) { \
        yp_float_t x_asfloat; \
        int x_pair = ypObject_TYPE_PAIR_CODE( x ); \
        \
        if( x_pair == ypFloat_CODE ) { \
            x_asfloat = ypFloat_VALUE( x ); \
        } else if( x_pair == ypInt_CODE ) { \
            ypObject *exc = yp_None; \
            x_asfloat = yp_asfloatL( ypInt_VALUE( x ), &exc ); \
            if( yp_isexceptionC( exc ) ) return exc; \
        } else { \
            return yp_ComparisonNotImplemented; \
        } \
        return ypBool_FROM_C( ypFloat_VALUE( f ) operator x_asfloat ); \
    }
_ypFloat_RELATIVE_CMP_FUNCTION( lt, < );
_ypFloat_RELATIVE_CMP_FUNCTION( le, <= );
_ypFloat_RELATIVE_CMP_FUNCTION( eq, == );
_ypFloat_RELATIVE_CMP_FUNCTION( ne, != );
_ypFloat_RELATIVE_CMP_FUNCTION( ge, >= );
_ypFloat_RELATIVE_CMP_FUNCTION( gt, > );

static ypObject *float_currenthash( ypObject *f,
        hashvisitfunc hash_visitor, void *hash_memo, yp_hash_t *hash )
{
    // This must remain consistent with the other numeric types
    *hash = yp_HashDouble( ypFloat_VALUE( f ) );
    // Since we never contain mutable objects, we can cache our hash
    if( !ypObject_IS_MUTABLE( f ) ) ypObject_CACHED_HASH( f ) = *hash;
    return yp_None;
}

static ypTypeObject ypFloat_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    float_dealloc,                  // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    float_unfrozen_copy,            // tp_unfrozen_copy
    float_frozen_copy,              // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    float_bool,                     // tp_bool
    float_lt,                       // tp_lt
    float_le,                       // tp_le
    float_eq,                       // tp_eq
    float_ne,                       // tp_ne
    float_ge,                       // tp_ge
    float_gt,                       // tp_gt

    // Generic object operations
    float_currenthash,              // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypTypeObject ypFloatStore_Type = {
    yp_TYPE_HEAD_INIT,
    NULL,                           // tp_name

    // Object fundamentals
    float_dealloc,                  // tp_dealloc
    NoRefs_traversefunc,            // tp_traverse
    NULL,                           // tp_str
    NULL,                           // tp_repr

    // Freezing, copying, and invalidating
    MethodError_objproc,            // tp_freeze
    float_unfrozen_copy,            // tp_unfrozen_copy
    float_frozen_copy,              // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    float_bool,                     // tp_bool
    float_lt,                       // tp_lt
    float_le,                       // tp_le
    float_eq,                       // tp_eq
    float_ne,                       // tp_ne
    float_ge,                       // tp_ge
    float_gt,                       // tp_gt

    // Generic object operations
    float_currenthash,              // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    TypeError_miniiterfunc,         // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    MethodError_miniiterfunc,       // tp_miniiter_next
    MethodError_miniiter_lenhfunc,  // tp_miniiter_lenhint
    TypeError_objproc,              // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    TypeError_lenfunc,              // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    MethodError_objobjobjproc,      // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

yp_float_t yp_addLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return x + y; // TODO overflow check
}

yp_float_t yp_subLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return x - y; // TODO overflow check
}

yp_float_t yp_mulLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return x * y; // TODO overflow check
}

yp_float_t yp_truedivLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return x / y; // TODO overflow check
}

// XXX Although the return value is a whole number, in Python if one of the operands are floats,
// the result is a float
yp_float_t yp_floordivLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return_yp_CEXC_ERR( 0, exc, yp_NotImplementedError );
}

yp_float_t yp_modLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return_yp_CEXC_ERR( 0.0, exc, yp_NotImplementedError );
}

void yp_divmodLF( yp_float_t x, yp_float_t y, yp_float_t *_div, yp_float_t *_mod, ypObject **exc )
{
    *exc = yp_NotImplementedError;
}

yp_float_t yp_powLF( yp_float_t x, yp_float_t y, ypObject **exc )
{
    return pow( x, y ); // TODO overflow check
}

yp_float_t yp_negLF( yp_float_t x, ypObject **exc )
{
    return -x; // TODO overflow check
}

yp_float_t yp_posLF( yp_float_t x, ypObject **exc )
{
    return x;
}

yp_float_t yp_absLF( yp_float_t x, ypObject **exc )
{
    if( x < 0.0 ) return yp_negLF( x, exc );
    return x;
}

static void iarithmeticCF( ypObject **x, yp_float_t y, arithLFfunc floatop )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( *x );
    ypObject *exc = yp_None;
    yp_float_t result;

    if( x_pair == ypFloat_CODE ) {
        result = floatop( ypFloat_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypFloat_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_floatCF( result );
        }
        return;

    } else if( x_pair == ypInt_CODE ) {
        yp_float_t x_asfloat = yp_asfloatC( *x, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        result = floatop( ypFloat_VALUE( *x ), y, &exc );
        if( yp_isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_decref( *x );
        *x = yp_floatCF( result );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( x, *x );
}

// Defined here are yp_iaddCF (et al)
#define _ypFloat_PUBLIC_ARITH_FUNCTION( name ) \
    void yp_i ## name ## CF( ypObject **x, yp_float_t y ) { \
        iarithmeticCF( x, y, yp_ ## name ## LF ); \
    }
_ypFloat_PUBLIC_ARITH_FUNCTION( add );
_ypFloat_PUBLIC_ARITH_FUNCTION( sub );
_ypFloat_PUBLIC_ARITH_FUNCTION( mul );
_ypFloat_PUBLIC_ARITH_FUNCTION( truediv );
_ypFloat_PUBLIC_ARITH_FUNCTION( floordiv );
_ypFloat_PUBLIC_ARITH_FUNCTION( mod );
_ypFloat_PUBLIC_ARITH_FUNCTION( pow );

// Binary operations are not applicable on floats
static void yp_ilshiftCF( ypObject **x, yp_float_t y ) {
    return_yp_INPLACE_ERR( x, yp_TypeError );
}
static void yp_irshiftCF( ypObject **x, yp_float_t y ) {
    return_yp_INPLACE_ERR( x, yp_TypeError );
}
static void yp_iampCF( ypObject **x, yp_float_t y ) {
    return_yp_INPLACE_ERR( x, yp_TypeError );
}
static void yp_ixorCF( ypObject **x, yp_float_t y ) {
    return_yp_INPLACE_ERR( x, yp_TypeError );
}
static void yp_ibarCF( ypObject **x, yp_float_t y ) {
    return_yp_INPLACE_ERR( x, yp_TypeError );
}
static yp_float_t yp_lshiftLF( yp_float_t x, yp_float_t y, ypObject **exc ) {
    return_yp_CEXC_ERR( 0.0, exc, yp_TypeError );
}
static yp_float_t yp_rshiftLF( yp_float_t x, yp_float_t y, ypObject **exc ) {
    return_yp_CEXC_ERR( 0.0, exc, yp_TypeError );
}
static yp_float_t yp_ampLF( yp_float_t x, yp_float_t y, ypObject **exc ) {
    return_yp_CEXC_ERR( 0.0, exc, yp_TypeError );
}
static yp_float_t yp_xorLF( yp_float_t x, yp_float_t y, ypObject **exc ) {
    return_yp_CEXC_ERR( 0.0, exc, yp_TypeError );
}
static yp_float_t yp_barLF( yp_float_t x, yp_float_t y, ypObject **exc ) {
    return_yp_CEXC_ERR( 0.0, exc, yp_TypeError );
}
static yp_float_t yp_invertLF( yp_float_t x, ypObject **exc ) {
    return_yp_CEXC_ERR( 0.0, exc, yp_TypeError );
}


// Public constructors

ypObject *yp_floatCF( yp_float_t value )
{
    ypObject *f = ypMem_MALLOC_FIXED( ypFloatObject, ypFloat_CODE );
    if( yp_isexceptionC( f ) ) return f;
    ypFloat_VALUE( f ) = value;
    return f;
}

ypObject *yp_floatstoreCF( yp_float_t value )
{
    ypObject *f = ypMem_MALLOC_FIXED( ypFloatObject, ypFloatStore_CODE );
    if( yp_isexceptionC( f ) ) return f;
    ypFloat_VALUE( f ) = value;
    return f;
}

static ypObject *_ypFloat( ypObject *(*allocator)( yp_float_t ), ypObject *x )
{
    ypObject *exc = yp_None;
    yp_float_t x_asfloat = yp_asfloatC( x, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    return allocator( x_asfloat );
}
ypObject *yp_float( ypObject *x ) {
    if( ypObject_TYPE_CODE( x ) == ypFloat_CODE ) return yp_incref( x );
    return _ypFloat( yp_floatCF, x );
}
ypObject *yp_floatstore( ypObject *x ) {
    return _ypFloat( yp_floatstoreCF, x );
}

// Public conversion functions

yp_float_t yp_asfloatC( ypObject *x, ypObject **exc )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypInt_CODE ) {
        return yp_asfloatL( ypInt_VALUE( x ), exc );
    } else if( x_pair == ypFloat_CODE ) {
        return ypFloat_VALUE( x );
    } else if( x_pair == ypBool_CODE ) {
        return yp_asfloatL( ypBool_IS_TRUE_C( x ), exc );
    }
    return_yp_CEXC_BAD_TYPE( 0.0, exc, x );
}

yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc )
{
    // TODO Implement this as Python does
    return (yp_float_t) x;
}

yp_int_t yp_asintLF( yp_float_t x, ypObject **exc )
{
    // TODO Implement this as Python does
    return (yp_int_t) x;
}


/*************************************************************************************************
 * Common sequence functions
 *************************************************************************************************/

// Using the given length, adjusts negative indicies to positive.  Returns false if the adjusted 
// index is out-of-bounds, else true.
static int ypSequence_AdjustIndexC( yp_ssize_t length, yp_ssize_t *i )
{
    if( *i < 0 ) *i += length;
    if( *i < 0 || *i >= length ) return FALSE;
    return TRUE;
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
// inverse slice, where *step=-(*step).  slicelength must be >0 (slicelength==0 is a no-op).
// Adapted from Python's list_ass_subscript
static void _ypSlice_InvertIndicesC( yp_ssize_t *start, yp_ssize_t *stop, yp_ssize_t *step,
        yp_ssize_t slicelength )
{
    yp_ASSERT( slicelength > 0, "slicelen must be >0" );
    if( *step < 0 ) {
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

// Returns the index of the i'th item in the slice with the given adjusted start/step values.  i
// must be in range(slicelength).
#define ypSlice_INDEX( start, step, i )  ((start) + (i)*(step))

// Used to remove elements from an array containing length elements, each of elemsize bytes.
// start, stop, step, and slicelength must be the _adjusted_ values from ypSlice_AdjustIndicesC,
// and slicelength must be >0 (slicelength==0 is a no-op).  Any references in the removed elements
// must have already been discarded.
static void _ypSlice_delslice_memmove( void *array, yp_ssize_t length, yp_ssize_t elemsize,
        yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, yp_ssize_t slicelength )
{
    yp_uint8_t *bytes = array;

    if( step < 0 ) _ypSlice_InvertIndicesC( &start, &stop, &step, slicelength );

    if( step == 1 ) {
        // One contiguous section
        memmove( bytes+(start*elemsize), bytes+(stop*elemsize), (length-stop)*elemsize );
    } else {
        yp_ssize_t remaining = slicelength;
        yp_uint8_t *chunk_dst = bytes + (start * elemsize);
        yp_uint8_t *chunk_src = chunk_dst + elemsize;
        yp_ssize_t  chunk_len = (step - 1) * elemsize;
        while( remaining > 1 ) {
            memmove( chunk_dst, chunk_src, chunk_len );
            chunk_dst += chunk_len;
            chunk_src += chunk_len + elemsize;
            remaining -= 1;
        }
        // The last chunk is likely truncated, and not a full step*elemsize in size
        chunk_len = (bytes + (length * elemsize)) - chunk_src;
        memmove( chunk_dst, chunk_src, chunk_len );
    }
}

static ypObject *_ypSequence_getdefault( ypObject *x, ypObject *key, ypObject *defval ) {
    ypObject *exc = yp_None;
    ypTypeObject *type = ypObject_TYPE( x );
    yp_ssize_t index = yp_asssizeC( key, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    return type->tp_as_sequence->tp_getindex( x, index, defval );
}

static ypObject *_ypSequence_setitem( ypObject *x, ypObject *key, ypObject *value ) {
    ypObject *exc = yp_None;
    ypTypeObject *type = ypObject_TYPE( x );
    yp_ssize_t index = yp_asssizeC( key, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    return type->tp_as_sequence->tp_setindex( x, index, value );
}

static ypObject *_ypSequence_delitem( ypObject *x, ypObject *key ) {
    ypObject *exc = yp_None;
    ypTypeObject *type = ypObject_TYPE( x );
    yp_ssize_t index = yp_asssizeC( key, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    return type->tp_as_sequence->tp_delindex( x, index );
}


/*************************************************************************************************
 * Sequence of bytes
 *************************************************************************************************/

// struct _ypBytesObject is declared in nohtyP.h for use by yp_IMMORTAL_BYTES
typedef struct _ypBytesObject ypBytesObject;
yp_STATIC_ASSERT( offsetof( ypBytesObject, ob_inline_data ) % yp_MAX_ALIGNMENT == 0, alignof_bytes_inline_data );

#define ypBytes_DATA( b )           ( (yp_uint8_t *) ((ypObject *)b)->ob_data )
#define ypBytes_LEN( b )            ( ((ypObject *)b)->ob_len )
#define ypBytes_ALLOCLEN( b )       ( ((ypObject *)b)->ob_alloclen )
#define ypBytes_INLINE_DATA( b )    ( ((ypBytesObject *)b)->ob_inline_data )

// Empty bytes can be represented by this, immortal object
static ypBytesObject _yp_bytes_empty_struct = {
    { ypBytes_CODE, ypObject_REFCNT_IMMORTAL,
    0, 0, ypObject_HASH_INVALID, "" } };
static ypObject * const _yp_bytes_empty = (ypObject *) &_yp_bytes_empty_struct;

// Moves the bytes from [src:] to the index dest; this can be used when deleting bytes, or
// inserting bytes (the new space is uninitialized).  Assumes enough space is allocated for the
// move.  Recall that memmove handles overlap.  Also adjusts null terminator.
#define ypBytes_ELEMMOVE( b, dest, src ) \
    memmove( ypBytes_DATA( b )+(dest), ypBytes_DATA( b )+(src), ypBytes_LEN( b )-(src)+1 );

// Return a new bytes/bytearray object with the given alloclen.  If type is immutable and
// alloclen_fixed is true (indicating the object will never grow), the data is placed inline with
// one allocation.
// XXX Remember that alloclen should account for the null terminator; also remember to add that
// null terminator
// XXX Check for the _yp_bytes_empty case first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Extend alloclen_fixed to other areas
// TODO Over-allocate to avoid future resizings
static ypObject *_ypBytes_new( int type, yp_ssize_t alloclen, int alloclen_fixed )
{
    if( type == ypBytes_CODE && alloclen_fixed ) {
        return ypMem_MALLOC_CONTAINER_INLINE( ypBytesObject, type, alloclen );
    } else {
        return ypMem_MALLOC_CONTAINER_VARIABLE( ypBytesObject, type, alloclen, 0 );
    }
}

// XXX Check for the possiblity of a lazy shallow copy before calling this function
// XXX Check for the _yp_bytes_empty case first
static ypObject *_ypBytes_copy( int type, ypObject *b, int alloclen_fixed )
{
    ypObject *copy = _ypBytes_new( type, ypBytes_LEN( b )+1, alloclen_fixed );
    if( yp_isexceptionC( copy ) ) return copy;
    memcpy( ypBytes_DATA( copy ), ypBytes_DATA( b ), ypBytes_LEN( b )+1 );
    ypBytes_LEN( copy ) = ypBytes_LEN( b );
    return copy;
}

// Shrinks or grows the bytearray; if shrinking, ensure length is updated
// XXX Remember that required should account for the null terminator
// XXX Check alloclen first to avoid unnecessary resizes
// TODO Split this into grow and shrink functions (here and elsewhere) that check alloclen
static ypObject *_ypBytes_resize( ypObject *b, yp_ssize_t required, yp_ssize_t extra )
{
    return ypMem_REALLOC_CONTAINER_VARIABLE( b, ypBytesObject, required, extra );
}

// As yp_asuint8C, but raises yp_ValueError when value out of range and yp_TypeError if not an int
static yp_uint8_t _ypBytes_asuint8C( ypObject *x, ypObject **exc ) {
    yp_int_t asint;
    yp_uint8_t retval;

    if( ypObject_TYPE_PAIR_CODE( x ) != ypInt_CODE ) return_yp_CEXC_BAD_TYPE( 0, exc, x );
    asint = yp_asintC( x, exc );
    retval = (yp_uint8_t) (asint & 0xFFu);
    if( (yp_int_t) retval != asint ) return_yp_CEXC_ERR( retval, exc, yp_ValueError );
    return retval;
}

// If x is a bool/int in range(256), store value in storage and set *x_data=storage, *x_len=1.  If
// x is a fellow bytes, set *x_data and *x_len.  Otherwise, returns an exception.
static ypObject *_ypBytes_coerce_intorbytes( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len,
        yp_uint8_t *storage )
{
    ypObject *exc = yp_None;
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    if( x_pair == ypBool_CODE || x_pair == ypInt_CODE ) {
        *storage = _ypBytes_asuint8C( x, &exc );
        if( yp_isexceptionC( exc ) ) return exc;
        *x_data = storage;
        *x_len = 1;
        return yp_None;
    } else if( x_pair == ypBytes_CODE ) {
        *x_data = ypBytes_DATA( x );
        *x_len = ypBytes_LEN( x );
        return yp_None;
    } else {
        return_yp_BAD_TYPE( x );
    }
}

// Used by tp_repeat et al to perform the necessary memcpy's.  b's array must be allocated
// to hold (factor*n)+1 bytes, and the bytes to repeat must be in the first n elements of the array.
// Further, factor and n must both be greater than zero.  Null-terminates the result.  Cannot fail.
static void _ypBytes_repeat_memcpy( ypObject *b, size_t factor, size_t n )
{
    yp_uint8_t *data = ypBytes_DATA( b );
    size_t copied; // the number of times [:n] has been repeated (starts at 1, of course)
    for( copied = 1; copied*2 < factor; copied *= 2 ) {
        memcpy( data+(n*copied), data+0, n*copied );
    }
    memcpy( data+(n*copied), data+0, n*(factor-copied) ); // no-op if factor==copied
    data[factor*n] = '\0';
}

// Extends b with the contents of x, a fellow byte object; always writes the null-terminator
// TODO over-allocate as appropriate
static ypObject *_ypBytes_extend_from_bytes( ypObject *b, ypObject *x )
{
    ypObject *result;
    // TODO check for overflow?!
    yp_ssize_t newLen = ypBytes_LEN( b ) + ypBytes_LEN( x );
    if( ypBytes_ALLOCLEN( b ) < newLen+1 ) {
        result = _ypBytes_resize( b, newLen+1, 0 );
        if( yp_isexceptionC( result ) ) return result;
    }
    memcpy( ypBytes_DATA( b )+ypBytes_LEN( b ), ypBytes_DATA( x ), ypBytes_LEN( x ) );
    ypBytes_DATA( b )[newLen] = '\0';
    ypBytes_LEN( b ) = newLen;
    return yp_None;
}

// Extends b with the items yielded from x; never writes the null-terminator, and only updates
// length once the iterator is exhausted
// XXX Do "b[len(b)]=0" when this returns (even on error)
static ypObject *_ypBytes_extend_from_iter( ypObject *b, ypObject *mi, yp_uint64_t *mi_state )
{
    ypObject *exc = yp_None;
    ypObject *x;
    yp_uint8_t x_asbyte;
    yp_ssize_t lenhint = yp_miniiter_lenhintC( mi, mi_state, &exc ); // zero on error
    yp_ssize_t newLen = ypBytes_LEN( b );
    ypObject *result;

    while( 1 ) {
        x = yp_miniiter_next( mi, mi_state ); // new ref
        if( yp_isexceptionC( x ) ) {
            if( yp_isexceptionC2( x, yp_StopIteration ) ) break;
            return x;
        }
        x_asbyte = _ypBytes_asuint8C( x, &exc );
        yp_decref( x );
        if( yp_isexceptionC( exc ) ) return exc;

        lenhint -= 1; // check for <0 only when we need it
        // TODO check for overflow?!
        newLen += 1;
        if( ypBytes_ALLOCLEN( b ) < newLen+1 ) {
            if( lenhint < 0 ) lenhint = 0;
            result = _ypBytes_resize( b, newLen+1, lenhint );
            if( yp_isexceptionC( result ) ) return result;
        }
        ypBytes_DATA( b )[newLen-1] = x_asbyte;
    }

    // Modifying len here allows us to bail easily above, relying on the calling code to replace
    // the null terminator at the right position
    ypBytes_LEN( b ) = newLen;
    return yp_None;
}

// Extends b with the contents of x; always writes the null-terminator
static ypObject *_ypBytes_extend( ypObject *b, ypObject *iterable )
{
    int iterable_pair = ypObject_TYPE_PAIR_CODE( iterable );

    if( iterable_pair == ypBytes_CODE ) {
        return _ypBytes_extend_from_bytes( b, iterable );
    } else if( iterable_pair == ypStr_CODE ) {
        return yp_TypeError;
    } else {
        ypObject *result;
        yp_uint64_t mi_state;
        ypObject *mi = yp_miniiter( iterable, &mi_state ); // new ref
        if( yp_isexceptionC( mi ) ) return mi;
        result = _ypBytes_extend_from_iter( b, mi, &mi_state );
        ypBytes_DATA( b )[ypBytes_LEN( b )] = '\0'; // up to us to add null-terminator
        yp_decref( mi );
        return result;
    }
}

// XXX b and x must _not_ be the same object (pass a copy of x if so)
static ypObject *bytearray_delslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step );
static ypObject *_ypBytes_setslice_from_bytes( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step, ypObject *x )
{
    ypObject *result;
    yp_ssize_t slicelength;

    yp_ASSERT( b != x, "make a copy of x when b is x" );
    if( step == 1 && ypBytes_LEN( x ) == 0 ) return bytearray_delslice( b, start, stop, step );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;

    if( step == 1 ) {
        // TODO check for overflow?!
        yp_ssize_t growBy = ypBytes_LEN( x ) - slicelength; // negative means array shrinking
        yp_ssize_t newLen = ypBytes_LEN( b ) + growBy;

        // Ensure there's enough space allocated; after this, failure is impossible
        // FIXME The resize might have to copy data, _then_ we'll also do the ypBytes_ELEMMOVE,
        // copying large amounts of data twice; optimize
        // TODO Over-allocate
        if( growBy > 0 ) {
            if( ypBytes_ALLOCLEN( b ) < newLen+1 ) {
                result = _ypBytes_resize( b, newLen+1, 0 );
                if( yp_isexceptionC( result ) ) return result;
            }
        }

        // Adjust remaining items and null terminator
        ypBytes_ELEMMOVE( b, stop+growBy, stop );

        // There are now len(x) bytes starting at b[start] waiting for x's data
        memcpy( ypBytes_DATA( b )+start, ypBytes_DATA( x ), ypBytes_LEN( x ) );
        ypBytes_LEN( b ) = newLen;
    } else {
        yp_ssize_t i;
        if( ypBytes_LEN( x ) != slicelength ) return yp_ValueError;
        for( i = 0; i < slicelength; i++ ) {
            ypBytes_DATA( b )[ypSlice_INDEX( start, step, i )] = ypBytes_DATA( x )[i];
        }
    }
    return yp_None;
}


// Public Methods

static ypObject *bytes_unfrozen_copy( ypObject *b, visitfunc copy_visitor, void *copy_memo ) {
    return _ypBytes_copy( ypByteArray_CODE, b, /*alloclen_fixed=*/TRUE );
}

static ypObject *bytes_frozen_copy( ypObject *b, visitfunc copy_visitor, void *copy_memo )
{
    if( ypBytes_LEN( b ) < 1 ) return _yp_bytes_empty;
    // A shallow copy of a bytes to a bytes doesn't require an actual copy
    if( copy_visitor == yp_shallowcopy_visitor && ypObject_TYPE_CODE( b ) == ypBytes_CODE ) {
        return yp_incref( b );
    }
    return _ypBytes_copy( ypBytes_CODE, b, /*alloclen_fixed=*/TRUE );
}

static ypObject *bytes_bool( ypObject *b ) {
    return ypBool_FROM_C( ypBytes_LEN( b ) );
}

// Returns yp_None or an exception
static ypObject *bytes_find( ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        findfunc_direction direction, yp_ssize_t *i )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    yp_uint8_t storage;
    ypObject *result;
    yp_ssize_t step = 1;    // may change to -1
    yp_ssize_t b_rlen;      // remaining length
    yp_uint8_t *b_rdata;    // remaining data

    result = _ypBytes_coerce_intorbytes( x, &x_data, &x_len, &storage );
    if( yp_isexceptionC( result ) ) return result;

    // XXX start and stop are _almost_ like slice notation, except in the case of len(x)==0
    // and (unadjusted) start>len(b).  While we're at it, we can short-cut a b_rlen==0 case.
    if( start >= ypBytes_LEN( b ) ) {
        if( x_len < 1 ) {
            *i = (start == ypBytes_LEN( b ) ? ypBytes_LEN( b ) : -1);
        } else {
            *i = -1;
        }
        return yp_None;
    }

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &b_rlen );
    if( yp_isexceptionC( result ) ) return result;
    if( direction == yp_FIND_FORWARD ) {
        b_rdata = ypBytes_DATA( b ) + start;
        // step is already 1
    } else {
        b_rdata = ypBytes_DATA( b ) + stop - x_len;
        step = -1;
    }

    while( b_rlen >= x_len ) {
        if( memcmp( b_rdata, x_data, x_len ) == 0 ) {
            *i = b_rdata - ypBytes_DATA( b );
            return yp_None;
        }
        b_rdata += step;
        b_rlen--;
    }
    *i = -1;
    return yp_None;
}

static ypObject *bytes_concat( ypObject *b, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    yp_ssize_t newLen;
    ypObject *newB;

    if( x_pair != ypBytes_CODE ) return_yp_BAD_TYPE( x );

    newLen = ypBytes_LEN( b ) + ypBytes_LEN( x );
    if( newLen < 1 && ypObject_TYPE_CODE( b ) == ypBytes_CODE ) return _yp_bytes_empty;
    newB = _ypBytes_new( ypObject_TYPE_CODE( b ), newLen+1, /*alloclen_fixed=*/TRUE );
    if( yp_isexceptionC( newB ) ) return newB;

    memcpy( ypBytes_DATA( newB ), ypBytes_DATA( b ), ypBytes_LEN( b ) );
    memcpy( ypBytes_DATA( newB )+ypBytes_LEN( b ), ypBytes_DATA( x ), ypBytes_LEN( x ) );
    ypBytes_DATA( newB )[newLen] = '\0';
    ypBytes_LEN( newB ) = newLen;
    return newB;
}

static ypObject *bytes_repeat( ypObject *b, yp_ssize_t factor )
{
    int b_type = ypObject_TYPE_CODE( b );
    yp_ssize_t newLen;
    ypObject *newB;

    if( b_type == ypBytes_CODE ) {
        // If the result will be an empty array, return _yp_bytes_empty
        if( ypBytes_LEN( b ) < 1 || factor < 1 ) return _yp_bytes_empty;
        // If the result will be an exact copy, since we're immutable just return self
        if( factor == 1 ) return yp_incref( b );
    } else {
        // If the result will be an empty array, return a new, empty array
        if( ypBytes_LEN( b ) < 1 || factor < 1 ) {
            return yp_bytearrayC( NULL, 0 );
        }
        // If the result will be an exact copy, let the code below make that copy
    }

    if( factor > yp_SSIZE_T_MAX / ypBytes_LEN( b ) ) return yp_MemoryError;
    newLen = ypBytes_LEN( b ) * factor;
    newB = _ypBytes_new( b_type, newLen+1, /*alloclen_fixed=*/TRUE ); // new ref
    if( yp_isexceptionC( newB ) ) return newB;

    memcpy( ypBytes_DATA( newB ), ypBytes_DATA( b ), ypBytes_LEN( b ) );
    _ypBytes_repeat_memcpy( newB, factor, ypBytes_LEN( b ) );
    ypBytes_LEN( newB ) = newLen;
    return newB;
}

static ypObject *bytes_getindex( ypObject *b, yp_ssize_t i, ypObject *defval )
{
    if( !ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i ) ) {
        if( defval == NULL ) return yp_IndexError;
        return yp_incref( defval );
    }
    return yp_intC( ypBytes_DATA( b )[i] );
}

static ypObject *bytearray_setindex( ypObject *b, yp_ssize_t i, ypObject *x )
{
    ypObject *exc = yp_None;
    yp_uint8_t x_value;

    x_value = _ypBytes_asuint8C( x, &exc );
    if( yp_isexceptionC( exc ) ) return exc;

    if( !ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i ) ) {
        return yp_IndexError;
    }

    ypBytes_DATA( b )[i] = x_value;
    return yp_None;
}

static ypObject *bytearray_delindex( ypObject *b, yp_ssize_t i )
{
    if( !ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i ) ) {
        return yp_IndexError;
    }

    ypBytes_ELEMMOVE( b, i, i+1 );
    ypBytes_LEN( b ) -= 1;
    return yp_None;
}

static ypObject *bytes_getslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step )
{
    ypObject *result;
    yp_ssize_t newLen;
    ypObject *newB;

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &newLen );
    if( yp_isexceptionC( result ) ) return result;

    if( newLen < 1 && ypObject_TYPE_CODE( b ) == ypBytes_CODE ) return _yp_bytes_empty;
    newB = _ypBytes_new( ypObject_TYPE_CODE( b ), newLen+1, /*alloclen_fixed=*/TRUE );
    if( yp_isexceptionC( newB ) ) return newB;

    if( step == 1 ) {
        memcpy( ypBytes_DATA( newB ), ypBytes_DATA( b )+start, newLen );
    } else {
        yp_ssize_t i;
        for( i = 0; i < newLen; i++ ) {
            ypBytes_DATA( newB )[i] = ypBytes_DATA( b )[ypSlice_INDEX( start, step, i )];
        }
    }
    ypBytes_DATA( newB )[newLen] = '\0';
    ypBytes_LEN( newB ) = newLen;
    return newB;
}

static ypObject *bytearray_setslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );

    // If x is not a fellow bytes, or if it is the same object as b, then a copy must be made
    if( x_pair == ypBytes_CODE && b != x ) {
        return _ypBytes_setslice_from_bytes( b, start, stop, step, x );
    } else if( x_pair == ypInt_CODE || x_pair == ypStr_CODE ) {
        return yp_TypeError;
    } else {
        ypObject *result;
        ypObject *x_asbytes = yp_bytes( x );
        if( yp_isexceptionC( x_asbytes ) ) return x_asbytes;
        result = _ypBytes_setslice_from_bytes( b, start, stop, step, x_asbytes );
        yp_decref( x_asbytes );
        return result;
    }
}

static ypObject *bytearray_delslice( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step )
{
    ypObject *result;
    yp_ssize_t slicelength;

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;
    if( slicelength < 1 ) return yp_None; // no-op
    // Add one to the length to include the hidden null-terminator
    _ypSlice_delslice_memmove( ypBytes_DATA( b ), ypBytes_LEN( b )+1, 1,
            start, stop, step, slicelength );
    ypBytes_LEN( b ) -= slicelength;
    return yp_None;
}

#define bytearray_extend _ypBytes_extend

static ypObject *bytearray_clear( ypObject *b );
static ypObject *bytearray_irepeat( ypObject *b, yp_ssize_t factor )
{
    ypObject *result;
    yp_ssize_t newLen;

    if( ypBytes_LEN( b ) < 1 || factor == 1 ) return yp_None; // no-op
    if( factor < 1 ) return bytearray_clear( b );

    if( factor > yp_SSIZE_T_MAX / ypBytes_LEN( b ) ) return yp_MemoryError;
    newLen = ypBytes_LEN( b ) * factor;
    result = _ypBytes_resize( b, newLen+1, 0 );
    if( yp_isexceptionC( result ) ) return result;

    _ypBytes_repeat_memcpy( b, factor, ypBytes_LEN( b ) );
    ypBytes_LEN( b ) = newLen;
    return yp_None;
}

static ypObject *bytearray_insert( ypObject *b, yp_ssize_t i, ypObject *x )
{
    ypObject *exc = yp_None;
    // TODO check for overflow?!
    yp_ssize_t newLen = ypBytes_LEN( b ) + 1;
    yp_uint8_t x_asbyte;

    // Check for exceptions, then adjust the index (noting it should behave like b[i:i]=[x])
    x_asbyte = _ypBytes_asuint8C( x, &exc );
    if( yp_isexceptionC( exc ) ) return exc;
    if( i < 0 ) {
        i += ypBytes_LEN( b );
        if( i < 0 ) i = 0;
    } else if( i > ypBytes_LEN( b ) ) {
        i = ypBytes_LEN( b );
    }

    // Resize if necessary
    // FIXME The resize might have to copy data, _then_ we'll also do the ypBytes_ELEMMOVE, copying
    // large amounts of data twice; optimize (and...are there other areas of the code where this
    // happens?)
    if( ypBytes_ALLOCLEN( b ) < newLen+1 ) {
        // TODO over-allocate
        ypObject *result = _ypBytes_resize( b, newLen+1, 0 );
        if( yp_isexceptionC( result ) ) return result;
    }

    // Make room at i and add x_asbyte
    ypBytes_ELEMMOVE( b, i+1, i );
    ypBytes_DATA( b )[i] = x_asbyte;
    ypBytes_LEN( b ) = newLen;
    return yp_None;
}

static ypObject *bytearray_popindex( ypObject *b, yp_ssize_t i )
{
    ypObject *result;
    
    if( !ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i ) ) {
        return yp_IndexError;
    }

    result = yp_intC( ypBytes_DATA( b )[i] );
    ypBytes_ELEMMOVE( b, i, i+1 );
    ypBytes_LEN( b ) -= 1;
    return result;
}

// XXX Adapted from Python's reverse_slice
static ypObject *bytearray_reverse( ypObject *b )
{
    yp_uint8_t *lo = ypBytes_DATA( b );
    yp_uint8_t *hi = lo + ypBytes_LEN( b ) - 1;
    while( lo < hi ) {
        yp_uint8_t t = *lo;
        *lo = *hi;
        *hi = t;
        lo += 1;
        hi -= 1;
    }
    return yp_None;
}

static ypObject *bytes_contains( ypObject *b, ypObject *x )
{
    ypObject *result;
    yp_ssize_t i = -1;

    result = bytes_find( b, x, 0, yp_SLICE_USELEN, yp_FIND_FORWARD, &i );
    if( yp_isexceptionC( result ) ) return result;
    return ypBool_FROM_C( i >= 0 );
}

static ypObject *bytes_len( ypObject *b, yp_ssize_t *len )
{
    *len = ypBytes_LEN( b );
    return yp_None;
}

static ypObject *bytearray_push( ypObject *b, ypObject *x )
{
    return bytearray_insert( b, yp_SLICE_USELEN, x );
}

static ypObject *bytearray_clear( ypObject *b )
{
    ypObject *result = _ypBytes_resize( b, 0+1, 0 );
    if( yp_isexceptionC( result ) ) return result;
    yp_ASSERT( ypBytes_DATA( b ) == ypBytes_INLINE_DATA( b ), "bytearray_clear didn't allocate inline!" ); 
    ypBytes_DATA( b )[0] = '\0';
    ypBytes_LEN( b ) = 0;
    return yp_None;
}

static ypObject *bytearray_pop( ypObject *b )
{
    ypObject *result;

    if( ypBytes_LEN( b ) < 1 ) return yp_IndexError;
    result = yp_intC( ypBytes_DATA( b )[ypBytes_LEN( b )-1] );
    ypBytes_LEN( b ) -= 1;
    ypBytes_DATA( b )[ypBytes_LEN( b )] = '\0';
    return result;
}

// onmissing must be an immortal, or NULL
static ypObject *bytearray_remove( ypObject *b, ypObject *x, ypObject *onmissing )
{
    ypObject *exc = yp_None;
    yp_uint8_t x_asbyte;
    yp_ssize_t i;

    x_asbyte = _ypBytes_asuint8C( x, &exc );
    if( yp_isexceptionC( exc ) ) return exc;

    for( i = 0; i < ypBytes_LEN( b ); i++ ) {
        if( x_asbyte != ypBytes_DATA( b )[i] ) continue;

        // We found a match to remove
        ypBytes_ELEMMOVE( b, i, i+1 );
        ypBytes_LEN( b ) -= 1;
        return yp_None;
    }
    if( onmissing == NULL ) return yp_ValueError;
    return onmissing;
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

    result = _ypBytes_coerce_intorbytes( x, &x_data, &x_len, &storage );
    if( yp_isexceptionC( result ) ) return result;

    // XXX start and stop are _almost_ like slice notation, except in the case of len(x)==0
    // and (unadjusted) start>len(b).  While we're at it, we can short-cut a b_rlen==0 case.
    if( start >= ypBytes_LEN( b ) ) {
        if( x_len < 1 ) {
            *n = (start == ypBytes_LEN( b ) ? 1 : 0);
        } else {
            *n = 0;
        }
        return yp_None;
    }

    // Adjust the indicies, then check for the empty array case: it "matches" every byte position,
    // including the end of the slice, unless the unadjusted start value is larger than len(b)
    // (which is handled above)
    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &b_rlen );
    if( yp_isexceptionC( result ) ) return result;
    if( x_len < 1 ) {
        *n = b_rlen + 1;
        return yp_None;
    }

    // Do the counting
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
// TODO Would the pre-computed hash be a quick check for inequality before the memcmp?
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
    // Since we never contain mutable objects, we can cache our hash
    if( !ypObject_IS_MUTABLE( b ) ) ypObject_CACHED_HASH( b ) = *hash;
    return yp_None;
}

static ypObject *bytes_dealloc( ypObject *b ) {
    ypMem_FREE_CONTAINER( b, ypBytesObject );
    return yp_None;
}

static ypSequenceMethods ypBytes_as_sequence = {
    bytes_concat,                   // tp_concat
    bytes_repeat,                   // tp_repeat
    bytes_getindex,                 // tp_getindex
    bytes_getslice,                 // tp_getslice
    bytes_find,                     // tp_find
    bytes_count,                    // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_append
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
    bytes_unfrozen_copy,            // tp_unfrozen_copy
    bytes_frozen_copy,              // tp_frozen_copy
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
    TypeError_objobjproc,           // tp_send

    // Container operations
    bytes_contains,                 // tp_contains
    bytes_len,                      // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    _ypSequence_getdefault,         // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    &ypBytes_as_sequence,           // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSequenceMethods ypByteArray_as_sequence = {
    bytes_concat,                   // tp_concat
    bytes_repeat,                   // tp_repeat
    bytes_getindex,                 // tp_getindex
    bytes_getslice,                 // tp_getslice
    bytes_find,                     // tp_find
    bytes_count,                    // tp_count
    bytearray_setindex,             // tp_setindex
    bytearray_setslice,             // tp_setslice
    bytearray_delindex,             // tp_delindex
    bytearray_delslice,             // tp_delslice
    bytearray_push,                 // tp_append
    bytearray_extend,               // tp_extend
    bytearray_irepeat,              // tp_irepeat
    bytearray_insert,               // tp_insert
    bytearray_popindex,             // tp_popindex
    bytearray_reverse,              // tp_reverse
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
    bytes_unfrozen_copy,            // tp_unfrozen_copy
    bytes_frozen_copy,              // tp_frozen_copy
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
    TypeError_objobjproc,           // tp_send

    // Container operations
    bytes_contains,                 // tp_contains
    bytes_len,                      // tp_len
    bytearray_push,                 // tp_push
    bytearray_clear,                // tp_clear
    bytearray_pop,                  // tp_pop
    bytearray_remove,               // tp_remove
    _ypSequence_getdefault,         // tp_getdefault
    _ypSequence_setitem,            // tp_setitem
    _ypSequence_delitem,            // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    &ypByteArray_as_sequence,       // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypObject *_yp_asbytesCX( ypObject *seq, const yp_uint8_t * *bytes, yp_ssize_t *len )
{
    if( ypObject_TYPE_PAIR_CODE( seq ) != ypBytes_CODE ) return_yp_BAD_TYPE( seq );
    *bytes = ypBytes_DATA( seq );
    if( len == NULL ) {
        if( (yp_ssize_t) strlen( *bytes ) != ypBytes_LEN( seq ) ) return yp_TypeError;
    } else {
        *len = ypBytes_LEN( seq );
    }
    return yp_None;
}
ypObject *yp_asbytesCX( ypObject *seq, const yp_uint8_t * *bytes, yp_ssize_t *len ) {
    ypObject *result = _yp_asbytesCX( seq, bytes, len );
    if( yp_isexceptionC( result ) ) {
        *bytes = NULL;
        if( len != NULL ) *len = 0;
    }
    return result;
}

// Public constructors

static ypObject *_ypBytesC( int type, const yp_uint8_t *source, yp_ssize_t len )
{
    ypObject *b;

    // Allocate an object of the appropriate size
    if( source == NULL ) {
        if( len < 0 ) len = 0;
    } else {
        if( len < 0 ) len = strlen( (const char *) source );
    }
    if( len < 1 && type == ypBytes_CODE ) return _yp_bytes_empty;
    b = _ypBytes_new( type, len+1, /*alloclen_fixed=*/TRUE );

    // Initialize the data
    if( source == NULL ) {
        memset( ypBytes_DATA( b ), 0, len+1 );
    } else {
        memcpy( ypBytes_DATA( b ), source, len );
        ypBytes_DATA( b )[len] = '\0';
    }
    ypBytes_LEN( b ) = len;
    return b;
}
ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len ) {
    return _ypBytesC( ypBytes_CODE, source, len );
}
ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len ) {
    return _ypBytesC( ypByteArray_CODE, source, len );
}

static ypObject *_ypBytes( int type, ypObject *source )
{
    ypObject *exc = yp_None;
    int source_pair = ypObject_TYPE_PAIR_CODE( source );

    if( source_pair == ypBytes_CODE ) {
        if( type == ypBytes_CODE ) {
            if( ypBytes_LEN( source ) < 1 ) return _yp_bytes_empty;
            if( ypObject_TYPE_CODE( source ) == ypBytes_CODE ) return yp_incref( source );
        }
        return _ypBytes_copy( type, source, /*alloclen_fixed=*/TRUE );
    } else if( source_pair == ypInt_CODE ) {
        yp_ssize_t len = yp_asssizeC( source, &exc );
        if( yp_isexceptionC( exc ) ) return exc;
        if( len < 0 ) return yp_ValueError;
        return _ypBytesC( type, NULL, len );
    } else {
        // Treat it as a generic iterator
        ypObject *newB;
        ypObject *result;
        yp_ssize_t lenhint = yp_lenC( source, &exc );
        if( yp_isexceptionC( exc ) ) {
            // Ignore errors determining lenhint; it just means we can't pre-allocate
            lenhint = yp_iter_lenhintC( source, &exc );
        } else if( lenhint == 0 ) {
            // yp_lenC reports an empty iterable, so we can shortcut _ypBytes_extend
            if( type == ypBytes_CODE ) return _yp_bytes_empty;
            newB = _ypBytes_new( type, 0+1, /*alloclen_fixed=*/TRUE );
            ypBytes_DATA( newB )[0] = '\0';
            return newB;
        }

        newB = _ypBytes_new( type, lenhint+1, /*alloclen_fixed=*/FALSE );
        if( yp_isexceptionC( newB ) ) return newB;
        result = _ypBytes_extend( newB, source );
        if( yp_isexceptionC( result ) ) {
            yp_decref( newB );
            return result;
        }
        return newB;
    }
}
ypObject *yp_bytes( ypObject *source ) {
    return _ypBytes( ypBytes_CODE, source );
}
ypObject *yp_bytearray( ypObject *source ) {
    return _ypBytes( ypByteArray_CODE, source );
}


/*************************************************************************************************
 * Sequence of unicode characters
 *************************************************************************************************/

// TODO http://www.python.org/dev/peps/pep-0393/ (flexible string representations)

// struct _ypStrObject is declared in nohtyP.h for use by yp_IMMORTAL_STR_LATIN1 et al
typedef struct _ypStrObject ypStrObject;
yp_STATIC_ASSERT( offsetof( ypStrObject, ob_inline_data ) % yp_MAX_ALIGNMENT == 0, alignof_str_inline_data );

// TODO pre-allocate static chrs in, say, range(255), or whatever seems appropriate

#define ypStr_DATA( s )         ( (yp_uint8_t *) ((ypObject *)s)->ob_data )
#define ypStr_LEN( s )          ( ((ypObject *)s)->ob_len )
#define ypStr_INLINE_DATA( s )  ( ((ypStrObject *)s)->ob_inline_data )

// Empty strs can be represented by this, immortal object
static ypStrObject _yp_str_empty_struct = {
    { ypStr_CODE, ypObject_REFCNT_IMMORTAL,
    0, 0, ypObject_HASH_INVALID, "" } };
static ypObject * const _yp_str_empty = (ypObject *) &_yp_str_empty_struct;

// Return a new str/chrarray object with the given alloclen.  If type is immutable and
// alloclen_fixed is true (indicating the object will never grow), the data is placed inline with
// one allocation.
// XXX Remember that alloclen should account for the null terminator; also remember to add that
// null terminator
// XXX Check for the _yp_str_empty case first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Extend alloclen_fixed to other areas
// TODO Over-allocate to avoid future resizings
static ypObject *_ypStr_new( int type, yp_ssize_t alloclen, int alloclen_fixed )
{
    if( type == ypStr_CODE && alloclen_fixed ) {
        return ypMem_MALLOC_CONTAINER_INLINE( ypStrObject, type, alloclen );
    } else {
        return ypMem_MALLOC_CONTAINER_VARIABLE( ypStrObject, type, alloclen, 0 );
    }
}

// XXX Check for the possiblity of a lazy shallow copy before calling this function
// XXX Check for the _yp_str_empty case first
static ypObject *_ypStr_copy( int type, ypObject *s, int alloclen_fixed )
{
    ypObject *copy = _ypStr_new( type, ypStr_LEN( s )+1, alloclen_fixed );
    if( yp_isexceptionC( copy ) ) return copy;
    memcpy( ypStr_DATA( copy ), ypStr_DATA( s ), ypStr_LEN( s )+1 );
    ypStr_LEN( copy ) = ypStr_LEN( s );
    return copy;
}

static ypObject *str_unfrozen_copy( ypObject *s, visitfunc copy_visitor, void *copy_memo ) {
    return _ypStr_copy( ypChrArray_CODE, s, TRUE );
}

static ypObject *str_frozen_copy( ypObject *s, visitfunc copy_visitor, void *copy_memo )
{
    if( ypStr_LEN( s ) < 1 ) return _yp_str_empty;
    // A shallow copy of a str to a str doesn't require an actual copy
    if( copy_visitor == yp_shallowcopy_visitor && ypObject_TYPE_CODE( s ) == ypStr_CODE ) {
        return yp_incref( s );
    }
    return _ypStr_copy( ypStr_CODE, s, TRUE );
}

static ypObject *str_bool( ypObject *s ) {
    return ypBool_FROM_C( ypStr_LEN( s ) );
}

static ypObject *str_concat( ypObject *s, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    yp_ssize_t newLen;
    ypObject *newS;

    if( x_pair != ypStr_CODE ) return_yp_BAD_TYPE( x );

    newLen = ypStr_LEN( s ) + ypStr_LEN( x );
    if( newLen < 1 && ypObject_TYPE_CODE( s ) == ypStr_CODE ) return _yp_str_empty;
    newS = _ypStr_new( ypObject_TYPE_CODE( s ), newLen+1, /*alloclen_fixed=*/TRUE );
    if( yp_isexceptionC( newS ) ) return newS;

    memcpy( ypStr_DATA( newS ), ypStr_DATA( s ), ypStr_LEN( s ) );
    memcpy( ypStr_DATA( newS )+ypStr_LEN( s ), ypStr_DATA( x ), ypStr_LEN( x ) );
    ypStr_DATA( newS )[newLen] = '\0';
    ypStr_LEN( newS ) = newLen;
    return newS;
}

static ypObject *str_getindex( ypObject *s, yp_ssize_t i, ypObject *defval )
{
    if( !ypSequence_AdjustIndexC( ypStr_LEN( s ), &i ) ) {
        if( defval == NULL ) return yp_IndexError;
        return yp_incref( defval );
    }
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
    yp_ssize_t s_len = ypStr_LEN( s );
    yp_ssize_t x_len = ypStr_LEN( x );
    int cmp = memcmp( ypStr_DATA( s ), ypStr_DATA( x ), MIN( s_len, x_len ) );
    if( cmp == 0 ) cmp = s_len < x_len ? -1 : (s_len > x_len ? 1 : 0);
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
// TODO Would the pre-computed hash be a quick check for inequality before the memcmp?
static int _ypStr_are_equal( ypObject *s, ypObject *x ) {
    yp_ssize_t s_len = ypStr_LEN( s );
    yp_ssize_t x_len = ypStr_LEN( x );
    if( s_len != x_len ) return 0;
    return memcmp( ypStr_DATA( s ), ypStr_DATA( x ), s_len ) == 0;
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
    // Since we never contain mutable objects, we can cache our hash
    if( !ypObject_IS_MUTABLE( s ) ) ypObject_CACHED_HASH( s ) = *hash;
    return yp_None;
}

static ypObject *str_dealloc( ypObject *s ) {
    ypMem_FREE_CONTAINER( s, ypStrObject );
    return yp_None;
}

static ypSequenceMethods ypStr_as_sequence = {
    str_concat,                     // tp_concat
    MethodError_objssizeproc,       // tp_repeat
    str_getindex,                   // tp_getindex
    MethodError_objsliceproc,       // tp_getslice
    MethodError_findfunc,           // tp_find
    MethodError_countfunc,          // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_append
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
    str_unfrozen_copy,              // tp_unfrozen_copy
    str_frozen_copy,                // tp_frozen_copy
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
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    str_len,                        // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    _ypSequence_getdefault,         // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    &ypStr_as_sequence,             // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSequenceMethods ypChrArray_as_sequence = {
    str_concat,                     // tp_concat
    MethodError_objssizeproc,       // tp_repeat
    str_getindex,                   // tp_getindex
    MethodError_objsliceproc,       // tp_getslice
    MethodError_findfunc,           // tp_find
    MethodError_countfunc,          // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_append
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
    str_unfrozen_copy,              // tp_unfrozen_copy
    str_frozen_copy,                // tp_frozen_copy
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
    TypeError_objobjproc,           // tp_send

    // Container operations
    MethodError_objobjproc,         // tp_contains
    str_len,                        // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    _ypSequence_getdefault,         // tp_getdefault
    _ypSequence_setitem,            // tp_setitem
    _ypSequence_delitem,            // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    &ypChrArray_as_sequence,        // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypObject *_yp_asencodedCX( ypObject *seq, const yp_uint8_t * *encoded, yp_ssize_t *size,
        ypObject * *encoding )
{
    if( ypObject_TYPE_PAIR_CODE( seq ) != ypStr_CODE ) return_yp_BAD_TYPE( seq );
    *encoded = ypStr_DATA( seq );
    if( size == NULL ) {
        if( (yp_ssize_t) strlen( *encoded ) != ypStr_LEN( seq ) ) return yp_TypeError;
    } else {
        *size = ypStr_LEN( seq );
    }
    *encoding = yp_s_latin_1;
    return yp_None;
}
ypObject *yp_asencodedCX( ypObject *seq, const yp_uint8_t * *encoded, yp_ssize_t *size,
        ypObject * *encoding )
{
    ypObject *result = _yp_asencodedCX( seq, encoded, size, encoding );
    if( yp_isexceptionC( result ) ) {
        *encoded = NULL;
        if( size != NULL ) *size = 0;
        *encoding = result;
    }
    return result;
}

// Public constructors

// FIXME completely ignoring encoding/errors, and assuming source is latin-1 (when we should be
// assuming it's utf-8 by default)
static ypObject *_ypStrC( int type, const yp_uint8_t *source, yp_ssize_t len )
{
    ypObject *s;

    // Allocate an object of the appropriate size
    if( source == NULL ) {
        if( len < 0 ) len = 0;
    } else {
        if( len < 0 ) len = strlen( (const char *) source );
    }
    if( len < 1 && type == ypStr_CODE ) return _yp_str_empty;
    s = _ypStr_new( type, len+1, /*alloclen_fixed=*/TRUE );

    // Initialize the data
    if( source == NULL ) {
        memset( ypStr_DATA( s ), 0, len );
    } else {
        memcpy( ypStr_DATA( s ), source, len );
        ypStr_DATA( s )[len] = '\0';
    }
    ypStr_LEN( s ) = len;
    return s;
}
ypObject *yp_str_frombytesC4( const yp_uint8_t *source, yp_ssize_t len,
        ypObject *encoding, ypObject *errors ) {
    if( encoding != yp_s_latin_1 ) return yp_NotImplementedError;
    return _ypStrC( ypStr_CODE, source, len );
}
ypObject *yp_chrarray_frombytesC4( const yp_uint8_t *source, yp_ssize_t len,
        ypObject *encoding, ypObject *errors ) {
    if( encoding != yp_s_latin_1 ) return yp_NotImplementedError;
    return _ypStrC( ypChrArray_CODE, source, len );
}
ypObject *yp_str_frombytesC2( const yp_uint8_t *source, yp_ssize_t len ) {
    return _ypStrC( ypStr_CODE, source, len );
}
ypObject *yp_chrarray_frombytesC2( const yp_uint8_t *source, yp_ssize_t len ) {
    return _ypStrC( ypChrArray_CODE, source, len );
}

ypObject *yp_chrC( yp_int_t i ) {
    yp_uint8_t source[1];

    if( i < 0x00 || i > 0xFF ) return yp_SystemLimitationError;
    source[0] = (yp_uint8_t) i;
    return _ypStrC( ypStr_CODE, source, 1 );
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

typedef struct {
    ypObject_HEAD
    yp_INLINE_DATA( ypObject * );
} ypTupleObject;
#define ypTuple_ARRAY( sq )         ( (ypObject **) ((ypObject *)sq)->ob_data )
#define ypTuple_LEN( sq )           ( ((ypObject *)sq)->ob_len )
#define ypTuple_ALLOCLEN( sq )      ( ((ypObject *)sq)->ob_alloclen )
#define ypTuple_INLINE_DATA( sq )   ( ((ypTupleObject *)sq)->ob_inline_data )

// Empty tuples can be represented by this, immortal object
// TODO Can we use this in more places...anywhere we'd return a possibly-empty tuple?
static ypObject *_yp_tuple_empty_data[1] = {NULL};
static ypTupleObject _yp_tuple_empty_struct = {
    { ypTuple_CODE, ypObject_REFCNT_IMMORTAL,
    0, 0, ypObject_HASH_INVALID, _yp_tuple_empty_data } };
static ypObject * const _yp_tuple_empty = (ypObject *) &_yp_tuple_empty_struct;

// Moves the elements from [src:] to the index dest; this can be used when deleting items (they
// must be discarded first), or inserting (the new space is uninitialized).  Assumes enough space
// is allocated for the move.  Recall that memmove handles overlap.
#define ypTuple_ELEMMOVE( sq, dest, src ) \
    memmove( ypTuple_ARRAY( sq )+(dest), ypTuple_ARRAY( sq )+(src), \
            (ypTuple_LEN( sq )-(src)) * sizeof( ypObject * ) );


// Return a new tuple/list object with the given alloclen.  If type is immutable and
// alloclen_fixed is true (indicating the object will never grow), the data is placed inline
// with one allocation.
// XXX Check for the _yp_tuple_empty case first
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO Over-allocate to avoid future resizings
static ypObject *_ypTuple_new( int type, yp_ssize_t alloclen, int alloclen_fixed ) {
    if( type == ypTuple_CODE && alloclen_fixed ) {
        return ypMem_MALLOC_CONTAINER_INLINE( ypTupleObject, ypTuple_CODE, alloclen );
    } else {
        return ypMem_MALLOC_CONTAINER_VARIABLE( ypTupleObject, type, alloclen, 0 );
    }
}

// XXX Check for the "lazy shallow copy" and "_yp_tuple_empty" cases first
static ypObject *_ypTuple_copy_shallow( int type, ypObject *x, int alloclen_fixed )
{
    yp_ssize_t i;
    ypObject *sq = _ypTuple_new( type, ypTuple_LEN( x ), alloclen_fixed );
    if( yp_isexceptionC( sq ) ) return sq;
    memcpy( ypTuple_ARRAY( sq ), ypTuple_ARRAY( x ), ypTuple_LEN( x )*sizeof( ypObject * ) );
    for( i = 0; i < ypTuple_LEN( x ); i++ ) yp_incref( ypTuple_ARRAY( sq )[i] );
    ypTuple_LEN( sq ) = ypTuple_LEN( x );
    return sq;
}

// Will perform a lazy shallow copy if copy_visitor is yp_shallowcopy_visitor
// XXX Check for the _yp_tuple_empty case first
static ypObject *_ypTuple_copy( int type, ypObject *x, visitfunc copy_visitor, void *copy_memo, 
        int alloclen_fixed )
{
    ypObject *sq;
    yp_ssize_t i;
    ypObject *item;

    if( copy_visitor == yp_shallowcopy_visitor ) {
        // A shallow copy of a tuple to a tuple doesn't require an actual copy
        if( type == ypTuple_CODE && ypObject_TYPE_CODE( x ) == ypTuple_CODE ) {
            return yp_incref( x );
        }
        return _ypTuple_copy_shallow( type, x, alloclen_fixed );
    }

    sq = _ypTuple_new( type, ypTuple_LEN( x ), alloclen_fixed );
    if( yp_isexceptionC( sq ) ) return sq;

    // Update sq's len on each item so yp_decref can clean up after mid-copy failures
    for( i = 0; i < ypTuple_LEN( x ); i++ ) {
        item = copy_visitor( ypTuple_ARRAY( x )[i], copy_memo );
        if( yp_isexceptionC( item ) ) {
            yp_decref( sq );
            return item;
        }
        ypTuple_ARRAY( sq )[i] = item;
        ypTuple_LEN( sq ) += 1;
    }
    return sq;
}

// Shrinks or grows the tuple; if shrinking, ensure excess elements are discarded before calling.
// Returns yp_None on success, exception on error.
// TODO Create two functions: one that shrinks and one that grows
static ypObject *_ypTuple_resize( ypObject *sq, yp_ssize_t required, yp_ssize_t extra )
{
    return ypMem_REALLOC_CONTAINER_VARIABLE( sq, ypTupleObject, required, extra );
}

// Used by tp_repeat et al to perform the necessary memcpy's.  sq's array must be allocated
// to hold factor*n objects, the objects to repeat must be in the first n elements of the array,
// and the rest of the array must not contain any references (they will be overwritten).  Further,
// factor and n must both be greater than zero.  Cannot fail.
static void _ypTuple_repeat_memcpy( ypObject *sq, size_t factor, size_t n )
{
    ypObject **array = ypTuple_ARRAY( sq );
    size_t copied; // the number of times [:n] has been repeated (starts at 1, of course)
    size_t n_size = n * sizeof( ypObject * );
    for( copied = 1; copied*2 < factor; copied *= 2 ) {
        memcpy( array+(n*copied), array+0, n_size*copied );
    }
    memcpy( array+(n*copied), array+0, n_size*(factor-copied) ); // no-op if factor==copied
}

// growhint is the number of additional items, not including x, that are expected to be added to
// the tuple
static ypObject *_ypTuple_push( ypObject *sq, ypObject *x, yp_ssize_t growhint )
{
    ypObject *result;
    if( ypTuple_ALLOCLEN( sq ) - ypTuple_LEN( sq ) < 1 ) {
        if( growhint < 0 ) growhint = 0;
        // TODO check for overflow?!
        result = _ypTuple_resize( sq, ypTuple_LEN( sq )+1, growhint );
        if( yp_isexceptionC( result ) ) return result;
    }
    ypTuple_ARRAY( sq )[ypTuple_LEN( sq )] = yp_incref( x );
    ypTuple_LEN( sq ) += 1;
    return yp_None;
}

// XXX sq and x _may_ be the same object
static ypObject *_ypTuple_extend_from_tuple( ypObject *sq, ypObject *x )
{
    yp_ssize_t i;
    // TODO check for overflow?!
    yp_ssize_t newLen = ypTuple_LEN( sq ) + ypTuple_LEN( x );
    if( ypTuple_ALLOCLEN( sq ) < newLen ) {
        ypObject *result = _ypTuple_resize( sq, newLen, 0 );
        if( yp_isexceptionC( result ) ) return result;
    }
    memcpy( ypTuple_ARRAY( sq )+ypTuple_LEN( sq ), ypTuple_ARRAY( x ),
            ypTuple_LEN( x )*sizeof( ypObject * ) );
    for( i = ypTuple_LEN( sq ); i < newLen; i++ ) yp_incref( ypTuple_ARRAY( sq )[i] );
    ypTuple_LEN( sq ) = newLen;
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
    if( ypObject_TYPE_PAIR_CODE( iterable ) == ypTuple_CODE ) {
        return _ypTuple_extend_from_tuple( sq, iterable );
    } else {
        ypObject *result;
        yp_uint64_t mi_state;
        ypObject *mi = yp_miniiter( iterable, &mi_state ); // new ref
        if( yp_isexceptionC( mi ) ) return mi;
        result = _ypTuple_extend_from_iter( sq, mi, &mi_state );
        yp_decref( mi );
        return result;
    }
}

// XXX sq and x must _not_ be the same object (pass a copy of x if so)
static ypObject *list_delslice( ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step );
static ypObject *_ypTuple_setslice_from_tuple( ypObject *sq,
        yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step, ypObject *x )
{
    ypObject *result;
    yp_ssize_t slicelength;
    yp_ssize_t i;

    yp_ASSERT( sq != x, "make a copy of x when sq is x" );
    if( step == 1 && ypTuple_LEN( x ) == 0 ) return list_delslice( sq, start, stop, step );

    result = ypSlice_AdjustIndicesC( ypTuple_LEN( sq ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;

    if( step == 1 ) {
        // TODO check for overflow?!
        yp_ssize_t growBy = ypTuple_LEN( x ) - slicelength; // negative means list shrinking
        yp_ssize_t newLen = ypTuple_LEN( sq ) + growBy;

        // Ensure there's enough space allocated; after this, failure is impossible
        // FIXME The resize might have to copy data, _then_ we'll also do the ypTuple_ELEMMOVE,
        // copying large amounts of data twice; optimize
        // TODO Over-allocate
        if( growBy > 0 ) {
            if( ypTuple_ALLOCLEN( sq ) < newLen ) {
                result = _ypTuple_resize( sq, newLen, 0 );
                if( yp_isexceptionC( result ) ) return result;
            }
        }

        // Discard items in target area, then adjust remaining items
        for( i = start; i < stop; i++ ) yp_decref( ypTuple_ARRAY( sq )[i] );
        ypTuple_ELEMMOVE( sq, stop+growBy, stop );

        // There are now len(x) elements starting at sq[start] waiting for x's items
        memcpy( ypTuple_ARRAY( sq )+start, ypTuple_ARRAY( x ),
            ypTuple_LEN( x )*sizeof( ypObject * ) );
        for( i = start; i < start+ypTuple_LEN( x ); i++ ) yp_incref( ypTuple_ARRAY( sq )[i] );
        ypTuple_LEN( sq ) = newLen;
    } else {
        if( ypTuple_LEN( x ) != slicelength ) return yp_ValueError;
        for( i = 0; i < slicelength; i++ ) {
            ypObject **dest = ypTuple_ARRAY( sq ) + ypSlice_INDEX( start, step, i );
            yp_decref( *dest );
            *dest = yp_incref( ypTuple_ARRAY( x )[i] );
        }
    }
    return yp_None;
}

// Public Methods

static ypObject *tuple_concat( ypObject *sq, ypObject *iterable )
{
    ypObject *exc = yp_None;
    ypObject *newSq;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( iterable, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( iterable, &exc );
    } else if( lenhint == 0 ) {
        // yp_lenC reports an empty iterable, so we can shortcut _ypTuple_extend
        if( ypObject_TYPE_CODE( sq ) == ypTuple_CODE ) return yp_incref( sq );
        return _ypTuple_copy_shallow( ypList_CODE, sq, /*alloclen_fixed=*/TRUE );
    }

    newSq = _ypTuple_new( ypObject_TYPE_CODE( sq ), ypTuple_LEN( sq )+lenhint, 
            /*alloclen_fixed=*/FALSE );
    if( yp_isexceptionC( newSq ) ) return newSq;
    result = _ypTuple_extend_from_tuple( newSq, sq );
    if( !yp_isexceptionC( result ) ) {
        result = _ypTuple_extend( newSq, iterable );
    }
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSq );
        return result;
    }
    return newSq;
}

static ypObject *tuple_repeat( ypObject *sq, yp_ssize_t factor )
{
    int sq_type = ypObject_TYPE_CODE( sq );
    ypObject *newSq;
    yp_ssize_t i;

    if( sq_type == ypTuple_CODE ) {
        // If the result will be an empty tuple, return _yp_tuple_empty
        if( ypTuple_LEN( sq ) < 1 || factor < 1 ) return _yp_tuple_empty;
        // If the result will be an exact copy, since we're immutable just return self
        if( factor == 1 ) return yp_incref( sq );
    } else {
        // If the result will be an empty list, return a new, empty list
        if( ypTuple_LEN( sq ) < 1 || factor < 1 ) {
            return _ypTuple_new( ypList_CODE, 0, /*alloclen_fixed=*/TRUE );
        }
        // If the result will be an exact copy, let the code below make that copy
    }

    if( factor > yp_SSIZE_T_MAX / ypTuple_LEN( sq ) ) return yp_MemoryError;
    newSq = _ypTuple_new( sq_type, ypTuple_LEN( sq )*factor, /*alloclen_fixed=*/TRUE ); // new ref
    if( yp_isexceptionC( newSq ) ) return newSq;

    memcpy( ypTuple_ARRAY( newSq ), ypTuple_ARRAY( sq ), ypTuple_LEN( sq )*sizeof( ypObject * ) );
    _ypTuple_repeat_memcpy( newSq, factor, ypTuple_LEN( sq ) );

    ypTuple_LEN( newSq ) = factor*ypTuple_LEN( sq );
    for( i = 0; i < ypTuple_LEN( newSq ); i++ ) {
        yp_incref( ypTuple_ARRAY( newSq )[i] );
    }
    return newSq;
}

static ypObject *tuple_getindex( ypObject *sq, yp_ssize_t i, ypObject *defval )
{
    if( !ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i ) ) {
        if( defval == NULL ) return yp_IndexError;
        return yp_incref( defval );
    }
    return yp_incref( ypTuple_ARRAY( sq )[i] );
}

static ypObject *tuple_getslice( ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step )
{
    int sq_type = ypObject_TYPE_CODE( sq );
    ypObject *result;
    yp_ssize_t newLen;
    ypObject *newSq;
    yp_ssize_t i;

    result = ypSlice_AdjustIndicesC( ypTuple_LEN( sq ), &start, &stop, &step, &newLen );
    if( yp_isexceptionC( result ) ) return result;

    if( sq_type == ypTuple_CODE ) {
        // If the result will be an empty tuple, return _yp_tuple_empty
        if( newLen < 1 ) return _yp_tuple_empty;
        // If the result will be an exact copy, since we're immutable just return self
        if( step == 1 && newLen == ypTuple_LEN( sq ) ) return yp_incref( sq );
    } else {
        // If the result will be an empty list, return a new, empty list
        if( newLen < 1 ) return _ypTuple_new( ypList_CODE, 0, /*alloclen_fixed=*/TRUE );
        // If the result will be an exact copy, let the code below make that copy
    }

    newSq = _ypTuple_new( sq_type, newLen, /*alloclen_fixed=*/TRUE );
    if( yp_isexceptionC( newSq ) ) return newSq;

    if( step == 1 ) {
        memcpy( ypTuple_ARRAY( newSq ), ypTuple_ARRAY( sq )+start, newLen * sizeof( ypObject * ) );
        for( i = 0; i < newLen; i++ ) yp_incref( ypTuple_ARRAY( newSq )[i] );
    } else {
        for( i = 0; i < newLen; i++ ) {
            ypTuple_ARRAY( newSq )[i] =
                yp_incref( ypTuple_ARRAY( sq )[ypSlice_INDEX( start, step, i )] );
        }
    }
    ypTuple_LEN( newSq ) = newLen;
    return newSq;
}

static ypObject *tuple_find( ypObject *sq, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        findfunc_direction direction, yp_ssize_t *index )
{
    ypObject *result;
    yp_ssize_t step = 1;    // may change to -1
    yp_ssize_t sq_rlen;     // remaining length
    yp_ssize_t i;

    result = ypSlice_AdjustIndicesC( ypTuple_LEN( sq ), &start, &stop, &step, &sq_rlen );
    if( yp_isexceptionC( result ) ) return result;
    if( direction == yp_FIND_REVERSE ) {
        _ypSlice_InvertIndicesC( &start, &stop, &step, sq_rlen );
    }

    for( i = start; sq_rlen > 0; i += step, sq_rlen-- ) {
        ypObject *result = yp_eq( x, ypTuple_ARRAY( sq )[i] );
        if( yp_isexceptionC( result ) ) return result;
        if( ypBool_IS_TRUE_C( result ) ) {
            *index = i;
            return yp_None;
        }
    }
    *index = -1;
    return yp_None;
}

static ypObject *tuple_count( ypObject *sq, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *count )
{
    ypObject *result;
    yp_ssize_t step = 1; // ignored; assumed unchanged by ypSlice_AdjustIndicesC
    yp_ssize_t newLen; // ignored
    yp_ssize_t i;
    yp_ssize_t n = 0;

    result = ypSlice_AdjustIndicesC( ypTuple_LEN( sq ), &start, &stop, &step, &newLen );
    if( yp_isexceptionC( result ) ) return result;

    for( i = start; i < stop; i++ ) {
        ypObject *result = yp_eq( x, ypTuple_ARRAY( sq )[i] );
        if( yp_isexceptionC( result ) ) return result;
        if( ypBool_IS_TRUE_C( result ) ) n += 1;
    }
    *count = n;
    return yp_None;
}

static ypObject *list_setindex( ypObject *sq, yp_ssize_t i, ypObject *x )
{
    if( yp_isexceptionC( x ) ) return x;
    if( !ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i ) ) {
        return yp_IndexError;
    }
    yp_decref( ypTuple_ARRAY( sq )[i] );
    ypTuple_ARRAY( sq )[i] = yp_incref( x );
    return yp_None;
}

static ypObject *list_setslice( ypObject *sq, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step,
        ypObject *x )
{
    // If x is not a tuple/list, or if it is the same object as sq, then a copy must first be made
    if( ypObject_TYPE_PAIR_CODE( x ) == ypTuple_CODE && sq != x ) {
        return _ypTuple_setslice_from_tuple( sq, start, stop, step, x );
    } else {
        ypObject *result;
        ypObject *x_astuple = yp_tuple( x );
        if( yp_isexceptionC( x_astuple ) ) return x_astuple;
        result = _ypTuple_setslice_from_tuple( sq, start, stop, step, x_astuple );
        yp_decref( x_astuple );
        return result;
    }
}

static ypObject *list_popindex( ypObject *sq, yp_ssize_t i )
{
    ypObject *result;
    
    if( !ypSequence_AdjustIndexC( ypTuple_LEN( sq ), &i ) ) {
        return yp_IndexError;
    }

    result = ypTuple_ARRAY( sq )[i];
    ypTuple_ELEMMOVE( sq, i, i+1 );
    ypTuple_LEN( sq ) -= 1;
    return result;
}

// XXX Adapted from Python's reverse_slice
static ypObject *list_reverse( ypObject *sq )
{
    ypObject **lo = ypTuple_ARRAY( sq );
    ypObject **hi = lo + ypTuple_LEN( sq ) - 1;
    while( lo < hi ) {
        ypObject *t = *lo;
        *lo = *hi;
        *hi = t;
        lo += 1;
        hi -= 1;
    }
    return yp_None;
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
    ypObject *result;
    yp_ssize_t slicelength;
    yp_ssize_t i;

    result = ypSlice_AdjustIndicesC( ypTuple_LEN( sq ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;
    if( slicelength < 1 ) return yp_None; // no-op

    // First discard references, then shift the remaining pointers
    for( i = 0; i < slicelength; i++ ) {
        yp_decref( ypTuple_ARRAY( sq )[ypSlice_INDEX( start, step, i )] );
    }
    _ypSlice_delslice_memmove( ypTuple_ARRAY( sq ), ypTuple_LEN( sq ), sizeof( ypObject * ),
            start, stop, step, slicelength );
    ypTuple_LEN( sq ) -= slicelength;
    return yp_None;
}

#define list_extend _ypTuple_extend

static ypObject *list_clear( ypObject *sq );
static ypObject *list_irepeat( ypObject *sq, yp_ssize_t factor )
{
    ypObject *result;
    yp_ssize_t startLen = ypTuple_LEN( sq );
    yp_ssize_t i;

    if( startLen < 1 || factor == 1 ) return yp_None; // no-op
    if( factor < 1 ) return list_clear( sq );

    if( factor > yp_SSIZE_T_MAX / startLen ) return yp_MemoryError;
    result = _ypTuple_resize( sq, startLen * factor, 0 );
    if( yp_isexceptionC( result ) ) return result;

    _ypTuple_repeat_memcpy( sq, factor, startLen );

    // Remember that we already have references for [:startLen]
    ypTuple_LEN( sq ) *= factor;
    for( i = startLen; i < ypTuple_LEN( sq ); i++ ) {
        yp_incref( ypTuple_ARRAY( sq )[i] );
    }
    return yp_None;
}

// TODO Python's ins1 does an item-by-item copy rather than a memmove...inefficient? ...fix?
static ypObject *list_insert( ypObject *sq, yp_ssize_t i, ypObject *x )
{
    // Check for exceptions, then adjust the index (noting it should behave like sq[i:i]=[x])
    if( yp_isexceptionC( x ) ) return x;
    if( i < 0 ) {
        i += ypTuple_LEN( sq );
        if( i < 0 ) i = 0;
    } else if( i > ypTuple_LEN( sq ) ) {
        i = ypTuple_LEN( sq );
    }

    // Resize if necessary
    // FIXME The resize might have to copy data, _then_ we'll also do the ypTuple_ELEMMOVE, copying
    // large amounts of data twice; optimize (and...are there other areas of the code where this
    // happens?)
    if( ypTuple_ALLOCLEN( sq ) - ypTuple_LEN( sq ) < 1 ) {
        // TODO over-allocate
        // TODO check for overflow?!
        ypObject *result = _ypTuple_resize( sq, ypTuple_LEN( sq ) + 1, 0 );
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
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        ypObject *result = visitor( ypTuple_ARRAY( sq )[i], memo );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *tuple_unfrozen_copy( ypObject *sq, visitfunc copy_visitor, void *copy_memo ) {
    return _ypTuple_copy( ypList_CODE, sq, copy_visitor, copy_memo, /*alloclen_fixed=*/TRUE );
}

static ypObject *tuple_frozen_copy( ypObject *sq, visitfunc copy_visitor, void *copy_memo ) {
    if( ypTuple_LEN( sq ) < 1 ) return _yp_tuple_empty;
    return _ypTuple_copy( ypTuple_CODE, sq, copy_visitor, copy_memo, /*alloclen_fixed=*/TRUE );
}

static ypObject *tuple_bool( ypObject *sq ) {
    return ypBool_FROM_C( ypTuple_LEN( sq ) );
}

// Sets *i to the index in sq and x of the first differing element, or -1 if the elements are equal
// up to the length of the shortest object.  Returns exception on error.
static ypObject *_ypTuple_cmp_first_difference( ypObject *sq, ypObject *x, yp_ssize_t *i )
{
    yp_ssize_t min_len;

    if( sq == x ) {
        *i = -1;
        return yp_True;
    }
    if( ypObject_TYPE_PAIR_CODE( x ) != ypTuple_CODE ) return yp_ComparisonNotImplemented;

    // XXX Python's list implementation has protection against the length being modified during the
    // comparison; that isn't required here, at least not yet
    min_len = MIN( ypTuple_LEN( sq ), ypTuple_LEN( x ) );
    for( *i = 0; *i < min_len; (*i)++ ) {
        ypObject *result = yp_eq( ypTuple_ARRAY( sq )[*i], ypTuple_ARRAY( x )[*i] );
        if( result != yp_True ) return result; // returns on yp_False or an exception
    }
    *i = -1;
    return yp_True;
}
// Here be tuple_lt, tuple_le, tuple_ge, tuple_gt
#define _ypTuple_RELATIVE_CMP_FUNCTION( name, len_cmp_op ) \
    static ypObject *tuple_ ## name( ypObject *sq, ypObject *x ) { \
        yp_ssize_t i = -1; \
        ypObject *result = _ypTuple_cmp_first_difference( sq, x, &i ); \
        if( yp_isexceptionC( result ) ) return result; \
        if( i < 0 ) return ypBool_FROM_C( ypTuple_LEN( sq ) len_cmp_op ypTuple_LEN( x ) ); \
        return yp_ ## name( ypTuple_ARRAY( sq )[i], ypTuple_ARRAY( x )[i] ); \
    }
_ypTuple_RELATIVE_CMP_FUNCTION( lt, < );
_ypTuple_RELATIVE_CMP_FUNCTION( le, <= );
_ypTuple_RELATIVE_CMP_FUNCTION( ge, >= );
_ypTuple_RELATIVE_CMP_FUNCTION( gt, > );

// Returns yp_True if the two tuples/lists are equal.  Size is a quick way to check equality.
// FIXME comparison functions can recurse, just like currenthash...fix!
static ypObject *tuple_eq( ypObject *sq, ypObject *x )
{
    yp_ssize_t sq_len = ypTuple_LEN( sq );
    yp_ssize_t i;

    if( sq == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypTuple_CODE ) return yp_ComparisonNotImplemented;
    if( sq_len != ypTuple_LEN( x ) ) return yp_False;

    // We need to inspect all our items for equality, which could be time-intensive.  It's fairly 
    // obvious that the pre-computed hash, if available, can save us some time when sq!=x.
    if( ypObject_CACHED_HASH( sq ) != ypObject_HASH_INVALID && 
        ypObject_CACHED_HASH( x ) != ypObject_HASH_INVALID && 
        ypObject_CACHED_HASH( sq ) != ypObject_CACHED_HASH( x ) ) return yp_False;
    // TODO What if we haven't cached this hash yet, but we could?  Calculating the hash now could 
    // speed up future comparisons against these objects.  But!  What if we're a tuple of mutable
    // objects...we will then attempt to calculate the hash on every comparison, only to fail.  If
    // we had a flag to differentiate "tuple of mutables" with "not yet computed"...crap, that
    // still wouldn't quite work, because what if we freeze those mutables?

    for( i = 0; i < sq_len; i++ ) {
        ypObject *result = yp_eq( ypTuple_ARRAY( sq )[i], ypTuple_ARRAY( x )[i] );
        if( result != yp_True ) return result; // returns on yp_False or an exception
    }
    return yp_True;
}
static ypObject *tuple_ne( ypObject *sq, ypObject *x ) {
    ypObject *result = tuple_eq( sq, x );
    return ypBool_NOT( result );
}

// XXX Adapted from Python's tuplehash
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
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        ypObject *result = yp_eq( x, ypTuple_ARRAY( sq )[i] );
        if( result != yp_False ) return result; // yp_True, or an exception
    }
    return yp_False;
}

static ypObject *tuple_len( ypObject *sq, yp_ssize_t *len ) {
    *len = ypTuple_LEN( sq );
    return yp_None;
}

static ypObject *list_push( ypObject *sq, ypObject *x ) {
    // TODO over-allocate via growhint
    return _ypTuple_push( sq, x, 0 );
}

static ypObject *list_clear( ypObject *sq )
{
    // XXX yp_decref _could_ run code that requires us to be in a good state, so pop items from the
    // end one-at-a-time
    while( ypTuple_LEN( sq ) > 0 ) {
        ypTuple_LEN( sq ) -= 1;
        yp_decref( ypTuple_ARRAY( sq )[ypTuple_LEN( sq )] );
    }
    ypMem_REALLOC_CONTAINER_VARIABLE_CLEAR( sq, ypTupleObject );
    yp_ASSERT( ypTuple_ARRAY( sq ) == ypTuple_INLINE_DATA( sq ), "list_clear didn't allocate inline!" ); 
    return yp_None;
}

static ypObject *list_pop( ypObject *sq )
{
    if( ypTuple_LEN( sq ) < 1 ) return yp_IndexError;
    ypTuple_LEN( sq ) -= 1;
    return ypTuple_ARRAY( sq )[ypTuple_LEN( sq )];
}

// onmissing must be an immortal, or NULL
static ypObject *list_remove( ypObject *sq, ypObject *x, ypObject *onmissing )
{
    yp_ssize_t i;
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        ypObject *result = yp_eq( x, ypTuple_ARRAY( sq )[i] );
        if( result == yp_False ) continue;
        if( yp_isexceptionC( result ) ) return result;

        // result must be yp_True, so we found a match to remove
        yp_decref( ypTuple_ARRAY( sq )[i] );
        ypTuple_ELEMMOVE( sq, i, i+1 );
        ypTuple_LEN( sq ) -= 1;
        return yp_None;
    }
    if( onmissing == NULL ) return yp_ValueError;
    return onmissing;
}

static ypObject *tuple_dealloc( ypObject *sq )
{
    yp_ssize_t i;
    for( i = 0; i < ypTuple_LEN( sq ); i++ ) {
        yp_decref( ypTuple_ARRAY( sq )[i] );
    }
    ypMem_FREE_CONTAINER( sq, ypTupleObject );
    return yp_None;
}

static ypSequenceMethods ypTuple_as_sequence = {
    tuple_concat,                   // tp_concat
    tuple_repeat,                   // tp_repeat
    tuple_getindex,                 // tp_getindex
    tuple_getslice,                 // tp_getslice
    tuple_find,                     // tp_find
    tuple_count,                    // tp_count
    MethodError_objssizeobjproc,    // tp_setindex
    MethodError_objsliceobjproc,    // tp_setslice
    MethodError_objssizeproc,       // tp_delindex
    MethodError_objsliceproc,       // tp_delslice
    MethodError_objobjproc,         // tp_append
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
    tuple_unfrozen_copy,            // tp_unfrozen_copy
    tuple_frozen_copy,              // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    tuple_bool,                     // tp_bool
    tuple_lt,                       // tp_lt
    tuple_le,                       // tp_le
    tuple_eq,                       // tp_eq
    tuple_ne,                       // tp_ne
    tuple_ge,                       // tp_ge
    tuple_gt,                       // tp_gt

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
    TypeError_objobjproc,           // tp_send

    // Container operations
    tuple_contains,                 // tp_contains
    tuple_len,                      // tp_len
    MethodError_objobjproc,         // tp_push
    MethodError_objproc,            // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    _ypSequence_getdefault,         // tp_getdefault
    MethodError_objobjobjproc,      // tp_setitem
    MethodError_objobjproc,         // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    &ypTuple_as_sequence,           // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

static ypSequenceMethods ypList_as_sequence = {
    tuple_concat,                   // tp_concat
    tuple_repeat,                   // tp_repeat
    tuple_getindex,                 // tp_getindex
    tuple_getslice,                 // tp_getslice
    tuple_find,                     // tp_find
    tuple_count,                    // tp_count
    list_setindex,                  // tp_setindex
    list_setslice,                  // tp_setslice
    list_delindex,                  // tp_delindex
    list_delslice,                  // tp_delslice
    list_push,                      // tp_append
    list_extend,                    // tp_extend
    list_irepeat,                   // tp_irepeat
    list_insert,                    // tp_insert
    list_popindex,                  // tp_popindex
    list_reverse,                   // tp_reverse
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
    tuple_unfrozen_copy,            // tp_unfrozen_copy
    tuple_frozen_copy,              // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    tuple_bool,                     // tp_bool
    tuple_lt,                       // tp_lt
    tuple_le,                       // tp_le
    tuple_eq,                       // tp_eq
    tuple_ne,                       // tp_ne
    tuple_ge,                       // tp_ge
    tuple_gt,                       // tp_gt

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
    TypeError_objobjproc,           // tp_send

    // Container operations
    tuple_contains,                 // tp_contains
    tuple_len,                      // tp_len
    list_push,                      // tp_push
    list_clear,                     // tp_clear
    list_pop,                       // tp_pop
    list_remove,                    // tp_remove
    _ypSequence_getdefault,         // tp_getdefault
    _ypSequence_setitem,            // tp_setitem
    _ypSequence_delitem,            // tp_delitem
    MethodError_objvalistproc,      // tp_update

    // Sequence operations
    &ypList_as_sequence,            // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    MethodError_MappingMethods      // tp_as_mapping
};

// Constructors

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
// XXX Check for the "fellow tuple/list" case _before_ calling this function
static ypObject *_ypTuple( int type, ypObject *iterable )
{
    ypObject *exc = yp_None;
    ypObject *newSq;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( iterable, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( iterable, &exc );
    } else if( lenhint == 0 ) {
        // yp_lenC reports an empty iterable, so we can shortcut _ypTuple_extend
        if( type == ypTuple_CODE ) return _yp_tuple_empty;
        return _ypTuple_new( ypList_CODE, 0, /*alloclen_fixed=*/TRUE );
    }

    newSq = _ypTuple_new( type, lenhint, /*alloclen_fixed=*/FALSE );
    if( yp_isexceptionC( newSq ) ) return newSq;
    result = _ypTuple_extend( newSq, iterable );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSq );
        return result;
    }
    return newSq;
}

static ypObject *_yp_tupleNV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypTuple( ypTuple_CODE, iter_args );
}
ypObject *yp_tupleN( int n, ... ) {
    if( n < 1 ) return _yp_tuple_empty;
    return_yp_V_FUNC( ypObject *, _yp_tupleNV, (n, args), n );
}
ypObject *yp_tupleNV( int n, va_list args ) {
    if( n < 1 ) return _yp_tuple_empty;
    return _yp_tupleNV( n, args );
}
ypObject *yp_tuple( ypObject *iterable ) {
    if( ypObject_TYPE_PAIR_CODE( iterable ) == ypTuple_CODE ) {
        if( ypTuple_LEN( iterable ) < 1 ) return _yp_tuple_empty;
        if( ypObject_TYPE_CODE( iterable ) == ypTuple_CODE ) return yp_incref( iterable );
        return _ypTuple_copy_shallow( ypTuple_CODE, iterable, /*alloclen_fixed=*/TRUE );
    }
    return _ypTuple( ypTuple_CODE, iterable );
}

ypObject *yp_listN( int n, ... ) {
    if( n < 1 ) return _ypTuple_new( ypList_CODE, 0, /*alloclen_fixed=*/TRUE );
    return_yp_V_FUNC( ypObject *, yp_listNV, (n, args), n );
}
ypObject *yp_listNV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypTuple( ypList_CODE, iter_args );
}
ypObject *yp_list( ypObject *iterable ) {
    if( ypObject_TYPE_PAIR_CODE( iterable ) == ypTuple_CODE ) {
        return _ypTuple_copy_shallow( ypList_CODE, iterable, /*alloclen_fixed=*/TRUE );
    }
    return _ypTuple( ypList_CODE, iterable );
}

static ypObject *_ypTuple_repeatCNV( int type, yp_ssize_t factor, int n, va_list args )
{
    ypObject *newSq;
    yp_ssize_t i;
    ypObject *item;

    if( factor < 1 || n < 1 ) {
        if( type == ypTuple_CODE ) return _yp_tuple_empty;
        return _ypTuple_new( ypList_CODE, 0, /*alloclen_fixed=*/TRUE );
    }

    if( factor > yp_SSIZE_T_MAX / n ) return yp_MemoryError;
    newSq = _ypTuple_new( type, factor*n, /*alloclen_fixed=*/TRUE ); // new ref
    if( yp_isexceptionC( newSq ) ) return newSq;

    // Extract the objects from args first; we incref these later, which makes it easier to bail
    for( i = 0; i < n; i++ ) {
        item = va_arg( args, ypObject * );
        if( yp_isexceptionC( item ) ) {
            yp_decref( newSq );
            return item;
        }
        ypTuple_ARRAY( newSq )[i] = item;
    }
    _ypTuple_repeat_memcpy( newSq, factor, n );

    // Now set the other attributes, increment the reference counts, and return
    // TODO Do we want a back-door way to increment the reference counts by n?
    ypTuple_LEN( newSq ) = factor*n;
    for( i = 0; i < ypTuple_LEN( newSq ); i++ ) {
        yp_incref( ypTuple_ARRAY( newSq )[i] );
    }
    return newSq;
}

ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... ) {
    return_yp_V_FUNC( ypObject *, _ypTuple_repeatCNV, (ypTuple_CODE, factor, n, args), n );
}
ypObject *yp_tuple_repeatCNV( yp_ssize_t factor, int n, va_list args ) {
    return _ypTuple_repeatCNV( ypTuple_CODE, factor, n, args );
}

ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... ) {
    return_yp_V_FUNC( ypObject *, _ypTuple_repeatCNV, (ypList_CODE, factor, n, args), n );
}
ypObject *yp_list_repeatCNV( yp_ssize_t factor, int n, va_list args ) {
    return _ypTuple_repeatCNV( ypList_CODE, factor, n, args );
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
#define ypSet_LEN( so )                 ( ((ypObject *)so)->ob_len )
#define ypSet_FILL( so )                ( ((ypSetObject *)so)->fill )
#define ypSet_ALLOCLEN( so )            ( ((ypObject *)so)->ob_alloclen )
#define ypSet_MASK( so )                ( ypSet_ALLOCLEN( so ) - 1 )
#define ypSet_INLINE_DATA( so )         ( ((ypSetObject *)so)->ob_inline_data )

#define ypSet_PERTURB_SHIFT (5)

// This tests that, by default, the inline data is enough to hold ypSet_MINSIZE elements
#define ypSet_MINSIZE (8)
yp_STATIC_ASSERT( (_ypMem_ideal_size_DEFAULT-offsetof( ypSetObject, ob_inline_data )) / sizeof( ypSet_KeyEntry ) >= ypSet_MINSIZE, ypSet_minsize_inline );

// The threshold at which we resize the set, expressed as a fraction of alloclen (ie 2/3)
#define ypSet_RESIZE_AT_NMR (2) // numerator
#define ypSet_RESIZE_AT_DNM (3) // denominator

// We don't want the multiplications in _ypSet_space_remaining and _ypSet_calc_alloclen
// overflowing, so we impose a separate limit on the maximum len and alloclen for sets
#define ypSet_LEN_MAX   (MIN( MIN( yp_SSIZE_T_MAX/ypSet_RESIZE_AT_NMR, \
                                   yp_SSIZE_T_MAX/ypSet_RESIZE_AT_DNM), \
                                   ypObject_LEN_MAX ))

// A placeholder to replace deleted entries in the hash table
static ypObject _ypSet_dummy = yp_IMMORTAL_HEAD_INIT( ypInvalidated_CODE, NULL, 0 );
static ypObject *ypSet_dummy = &_ypSet_dummy;

// Empty frozensets can be represented by this, immortal object
static ypSet_KeyEntry _yp_frozenset_empty_data[ypSet_MINSIZE] = {0};
static ypSetObject _yp_frozenset_empty_struct = {
    { ypFrozenSet_CODE, ypObject_REFCNT_IMMORTAL,
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
// XXX Adapted from PyDict_SetItem, although our thresholds are slightly different
// TODO If we make this threshold configurable, the assert should be in yp_initialize
yp_STATIC_ASSERT( ypSet_RESIZE_AT_NMR <= yp_SSIZE_T_MAX / ypSet_LEN_MAX, ypSet_space_remaining_cant_overflow );
static yp_ssize_t _ypSet_space_remaining( ypObject *so )
{
    /* If fill >= 2/3 size, adjust size.  Normally, this doubles or
     * quaduples the size, but it's also possible for the dict to shrink
     * (if ma_fill is much larger than se_used, meaning a lot of dict
     * keys have been deleted).
     */
    // XXX The static assert above ensures this multiplication can't overflow
    yp_ssize_t retval = (ypSet_ALLOCLEN( so )*ypSet_RESIZE_AT_NMR) / ypSet_RESIZE_AT_DNM;
    retval -= ypSet_FILL( so );
    if( retval <= 0 ) return 0; // should resize before adding keys
    return retval;
}

// Returns the alloclen that will fit minused entries, or <1 on error
// XXX Adapted from Python's dictresize
// TODO Can we improve by using some bit-twiddling to get the highest power of 2?
// TODO If we make this threshold configurable, the assert should be in yp_initialize
yp_STATIC_ASSERT( ypSet_RESIZE_AT_DNM <= yp_SSIZE_T_MAX / ypSet_LEN_MAX, ypSet_calc_alloclen_cant_overflow );
static yp_ssize_t _ypSet_calc_alloclen( yp_ssize_t minused )
{
    yp_ssize_t minentries;
    yp_ssize_t alloclen;

    yp_ASSERT( minused >= 0, "minused cannot be negative" );
    if( minused > ypSet_LEN_MAX ) return -1;
    // XXX The static assert and ypSet_LEN_MAX check above ensure this can't overflow
    minentries = ((minused * ypSet_RESIZE_AT_DNM) / ypSet_RESIZE_AT_NMR) + 1;
    for( alloclen = ypSet_MINSIZE;
         alloclen <= minentries && alloclen > 0;
         alloclen <<= 1 );
    if( alloclen > ypSet_LEN_MAX ) return -1;
    return alloclen;
}

// Returns a new, empty set or frozenset object to hold minused entries
// XXX Check for the _yp_frozenset_empty first
// TODO Extend alloclen_fixed here
// TODO Put protection in place to detect when INLINE objects attempt to be resized
// TODO can use CONTAINER_INLINE if minused is a firm max length for the frozenset
// TODO Over-allocate to avoid future resizings
static ypObject *_ypSet_new( int type, yp_ssize_t minused )
{
    ypObject *so;
    yp_ssize_t alloclen = _ypSet_calc_alloclen( minused );
    if( alloclen < 1 ) return yp_SystemLimitationError;
    so = ypMem_MALLOC_CONTAINER_VARIABLE( ypSetObject, type, alloclen, 0 );
    if( yp_isexceptionC( so ) ) return so;
    // FIXME But wait, what if _ypMem_ideal_size allows us to be _larger_ than minsize?
    ypSet_ALLOCLEN( so ) = alloclen; // we can't make use of the excess anyway
    ypSet_FILL( so ) = 0;
    memset( ypSet_TABLE( so ), 0, alloclen * sizeof( ypSet_KeyEntry ) );
    yp_ASSERT( _ypSet_space_remaining( so ) >= minused, "new set doesn't have requested room" );
    return so;
}

// TODO Ensure shallow copies are efficient
// TODO Shallow copies of frozensets to frozensets can just return incref( x )...where to handle?
static void _ypSet_movekey_clean( ypObject *so, ypObject *key, yp_hash_t hash,
        ypSet_KeyEntry **ep );
static ypObject *_ypSet_copy( int type, ypObject *x, visitfunc copy_visitor, void *copy_memo )
{
    yp_ssize_t keysleft = ypSet_LEN( x );
    ypSet_KeyEntry *otherkeys = ypSet_TABLE( x );
    ypObject *so;
    yp_ssize_t i;
    ypObject *key;
    ypSet_KeyEntry *loc;

    so = _ypSet_new( type, keysleft );
    if( yp_isexceptionC( so ) ) return so;

    // The set is empty and contains no deleted entries, so we can use _ypSet_movekey_clean
    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &otherkeys[i] ) ) continue;
        keysleft -= 1;
        key = copy_visitor( otherkeys[i].se_key, copy_memo );
        if( yp_isexceptionC( key ) ) {
            yp_decref( so );
            return key;
        }
        _ypSet_movekey_clean( so, key, otherkeys[i].se_hash, &loc );
    }
    return so;
}

// Resizes the set to the smallest size that will hold minused values.  If you want to reduce the
// need for future resizes, call with a larger minused.  Returns yp_None, or an exception on error.
// TODO ensure we aren't unnecessarily resizing: if the old and new alloclens will be the same,
// and we don't have dummy entries, then resizing is a waste of effort.
// TODO Do we want to split minused into required and extra, like in other areas?
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
    if( newalloclen < 1 ) return yp_SystemLimitationError;
    // TODO check for overflow?!
    newkeys = (ypSet_KeyEntry *) yp_malloc( &newsize, newalloclen * sizeof( ypSet_KeyEntry ) );
    if( newkeys == NULL ) return yp_MemoryError;
    memset( newkeys, 0, newalloclen * sizeof( ypSet_KeyEntry ) );

    // Failures are impossible from here on, so swap-in the new table
    oldkeys = ypSet_TABLE( so );
    keysleft = ypSet_LEN( so );
    ypSet_SET_TABLE( so, newkeys );
    ypSet_LEN( so ) = 0;
    ypSet_FILL( so ) = 0;
    // FIXME But wait, what if _ypMem_ideal_size allows us to be _larger_ than newalloclen?
    ypSet_ALLOCLEN( so ) = newalloclen;
    yp_ASSERT( _ypSet_space_remaining( so ) >= minused, "resized set doesn't have requested room" );

    // Move the keys from the old table before free'ing it
    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &oldkeys[i] ) ) continue;
        keysleft -= 1;
        _ypSet_movekey_clean( so, oldkeys[i].se_key, oldkeys[i].se_hash, &loc );
    }
    if( oldkeys != ypSet_INLINE_DATA( so ) ) yp_free( oldkeys );
    yp_DEBUG( "_ypSet_resize: 0x%08X table 0x%08X  (was 0x%08X)", so, newkeys, oldkeys );
    return yp_None;
}

// Discards all references; called before clearing or deleting the set
// TODO yp_decref _could_ run code that requires us to be in a good state...but
// this isn't as easy as for a tuple.  We may be forced to use the safer _ypSet_pop.
static void _ypSet_discard_key_refs( ypObject *so )
{
    ypSet_KeyEntry *keys = ypSet_TABLE( so );
    yp_ssize_t keysleft = ypSet_LEN( so );
    yp_ssize_t i;

    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &keys[i] ) ) continue;
        keysleft -= 1;
        yp_decref( keys[i].se_key );
    }
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

// Adds the key to the hash table.  *spaceleft should be initialized from  _ypSet_space_remaining;
// this function then decrements it with each key added, and resets it on every resize.  growhint
// is the number of additional items, not including key, that are expected to be added to the set.
// Returns yp_True if so was modified, yp_False if it wasn't due to the key already being in the
// set, or an exception on error.
// XXX Adapted from PyDict_SetItem
static ypObject *_ypSet_push( ypObject *so, ypObject *key, yp_ssize_t *spaceleft,
        yp_ssize_t growhint )
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
    if( growhint < 0 ) growhint = 0;
    // TODO check for overflow?
    newlen = ypSet_LEN( so )+1 + growhint;
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
static ypObject *_ypSet_update_from_set( ypObject *so, ypObject *other )
{
    yp_ssize_t keysleft = ypSet_LEN( other );
    ypSet_KeyEntry *otherkeys = ypSet_TABLE( other );
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    ypObject *result;
    yp_ssize_t i;

    for( i = 0; keysleft > 0; i++ ) {
        if( !ypSet_ENTRY_USED( &otherkeys[i] ) ) continue;
        keysleft -= 1;
        // TODO _ypSet_push recalculates hash; consolidate?
        result = _ypSet_push( so, otherkeys[i].se_key, &spaceleft, keysleft );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

// XXX iterable may be an yp_ONSTACK_ITER_VALIST: use carefully
static ypObject *_ypSet_update_from_iter( ypObject *so, ypObject *mi, yp_uint64_t *mi_state )
{
    ypObject *exc = yp_None;
    ypObject *key;
    ypObject *result;
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    yp_ssize_t lenhint = yp_miniiter_lenhintC( mi, mi_state, &exc ); // zero on error

    while( 1 ) {
        key = yp_miniiter_next( mi, mi_state ); // new ref
        if( yp_isexceptionC( key ) ) {
            if( yp_isexceptionC2( key, yp_StopIteration ) ) break;
            return key;
        }
        lenhint -= 1; // check for <0 only when we need it in _ypSet_push
        result = _ypSet_push( so, key, &spaceleft, lenhint );
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
        return _ypSet_update_from_set( so, iterable );
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
            result = _ypSet_push( so, otherkeys[i].se_key, &spaceleft, keysleft ); // may resize so
            if( yp_isexceptionC( result ) ) return result;
        } else {
            // XXX spaceleft based on alloclen and fill, so doesn't change on deletions
            yp_decref( result );
        }
    }
    return yp_None;
}


// Public methods

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

// TODO Make shallow copies (copy_visitor == yp_shallowcopy_visitor) efficient (unless there's a
// lot of wasted space)
static ypObject *frozenset_unfrozen_copy( ypObject *so, visitfunc copy_visitor, void *copy_memo ) {
    return _ypSet_copy( ypSet_CODE, so, copy_visitor, copy_memo );
}

// TODO Make shallow copies (copy_visitor == yp_shallowcopy_visitor) efficient (unless there's a
// lot of wasted space)
static ypObject *frozenset_frozen_copy( ypObject *so, visitfunc copy_visitor, void *copy_memo ) {
    return _ypSet_copy( ypFrozenSet_CODE, so, copy_visitor, copy_memo );
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
yp_STATIC_ASSERT( ypSet_LEN_MAX <= 0xFFFFFFFFu, len_fits_32_bits );
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
        if( ((yp_ssize_t) state->index) >= ypSet_ALLOCLEN( so ) ) {
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

    if( so == x ) return ypBool_FROM_C( ypSet_LEN( so ) < 1 );
    if( ypObject_TYPE_PAIR_CODE( x ) == ypFrozenSet_CODE ) {
        return _ypSet_isdisjoint( so, x );
    } else {
        // Otherwise, we need to convert x to a set to quickly test if it contains all items
        // TODO Can we make a version of _ypSet_isdisjoint that doesn't reqire a new set created?
        x_asset = yp_frozenset( x );
        if( yp_isexceptionC( x_asset ) ) return x_asset;
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
    if( yp_isexceptionC( x_asset ) ) return x_asset;
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
    if( yp_isexceptionC( x_asset ) ) return x_asset;
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

// FIXME comparison functions can recurse, just like currenthash...fix!
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

// XXX We redirect the new-object set methods to the in-place versions.  Among other things, this
// helps to avoid duplicating code.
// FIXME ...except we are creating objects that we destroy then create new ones, which can probably
// be optimized in certain cases, so rethink these four methods.  At the very least, can we avoid
// the yp_freeze?
static ypObject *frozenset_union( ypObject *so, int n, va_list args )
{
    ypObject *result;
    ypObject *newSo;

    if( !ypObject_IS_MUTABLE( so ) && n < 1 ) return yp_incref( so );

    newSo = frozenset_unfrozen_copy( so, yp_shallowcopy_visitor, NULL );
    if( yp_isexceptionC( newSo ) ) return newSo;
    result = set_update( newSo, n, args );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSo );
        return result;
    }
    if( !ypObject_IS_MUTABLE( so ) ) yp_freeze( &newSo );
    return newSo;
}

static ypObject *frozenset_intersection( ypObject *so, int n, va_list args )
{
    ypObject *result;
    ypObject *newSo;

    if( !ypObject_IS_MUTABLE( so ) && n < 1 ) return yp_incref( so );

    newSo = frozenset_unfrozen_copy( so, yp_shallowcopy_visitor, NULL );
    if( yp_isexceptionC( newSo ) ) return newSo;
    result = set_intersection_update( newSo, n, args );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSo );
        return result;
    }
    if( !ypObject_IS_MUTABLE( so ) ) yp_freeze( &newSo );
    return newSo;
}

static ypObject *frozenset_difference( ypObject *so, int n, va_list args )
{
    ypObject *result;
    ypObject *newSo;

    if( !ypObject_IS_MUTABLE( so ) && n < 1 ) return yp_incref( so );

    newSo = frozenset_unfrozen_copy( so, yp_shallowcopy_visitor, NULL );
    if( yp_isexceptionC( newSo ) ) return newSo;
    result = set_difference_update( newSo, n, args );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSo );
        return result;
    }
    if( !ypObject_IS_MUTABLE( so ) ) yp_freeze( &newSo );
    return newSo;
}

static ypObject *frozenset_symmetric_difference( ypObject *so, ypObject *x )
{
    ypObject *result;
    ypObject *newSo;

    newSo = frozenset_unfrozen_copy( so, yp_shallowcopy_visitor, NULL );
    if( yp_isexceptionC( newSo ) ) return newSo;
    result = set_symmetric_difference_update( newSo, x );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSo );
        return result;
    }
    if( !ypObject_IS_MUTABLE( so ) ) yp_freeze( &newSo );
    return newSo;
}

static ypObject *set_pushunique( ypObject *so, ypObject *x ) {
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    // TODO Adapt PyDict_SetItem logic via growhint
    ypObject *result = _ypSet_push( so, x, &spaceleft, 0 );
    if( yp_isexceptionC( result ) ) return result;
    return result == yp_True ? yp_None : yp_KeyError;
}

static ypObject *set_push( ypObject *so, ypObject *x ) {
    yp_ssize_t spaceleft = _ypSet_space_remaining( so );
    // TODO Adapt PyDict_SetItem logic via growhint
    ypObject *result = _ypSet_push( so, x, &spaceleft, 0 );
    if( yp_isexceptionC( result ) ) return result;
    return yp_None;
}

static ypObject *set_clear( ypObject *so ) {
    if( ypSet_FILL( so ) < 1 ) return yp_None;
    _ypSet_discard_key_refs( so );
    // FIXME there's a memcpy in this that we can and should avoid
    ypMem_REALLOC_CONTAINER_VARIABLE( so, ypSetObject, ypSet_MINSIZE, 0 );
    yp_ASSERT( ypSet_TABLE( so ) == ypSet_INLINE_DATA( so ), "set_clear didn't allocate inline!" ); 
    // XXX if the realloc fails, we are still pointing at valid, if over-sized, memory
    // FIXME But wait, what if _ypMem_ideal_size allows us to be _larger_ than minsize?
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
    _ypSet_discard_key_refs( so );
    ypMem_FREE_CONTAINER( so, ypSetObject );
    return yp_None;
}

static ypSetMethods ypFrozenSet_as_set = {
    frozenset_isdisjoint,           // tp_isdisjoint
    frozenset_issubset,             // tp_issubset
    // tp_lt is elsewhere
    frozenset_issuperset,           // tp_issuperset
    // tp_gt is elsewhere
    frozenset_union,                // tp_union
    frozenset_intersection,         // tp_intersection
    frozenset_difference,           // tp_difference
    frozenset_symmetric_difference, // tp_symmetric_difference
    MethodError_objvalistproc,      // tp_intersection_update
    MethodError_objvalistproc,      // tp_difference_update
    MethodError_objobjproc,         // tp_symmetric_difference_update
    // tp_push (aka tp_set_ad) is elsewhere
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
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    frozenset_miniiter_next,        // tp_miniiter_next
    frozenset_miniiter_lenhint,     // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

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
    MethodError_objvalistproc,      // tp_update

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
    frozenset_union,                // tp_union
    frozenset_intersection,         // tp_intersection
    frozenset_difference,           // tp_difference
    frozenset_symmetric_difference, // tp_symmetric_difference
    set_intersection_update,        // tp_intersection_update
    set_difference_update,          // tp_difference_update
    set_symmetric_difference_update,// tp_symmetric_difference_update
    // tp_push (aka tp_set_ad) is elsewhere
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
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    frozenset_miniiter_next,        // tp_miniiter_next
    frozenset_miniiter_lenhint,     // tp_miniiter_lenhint
    _ypIter_from_miniiter,          // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

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
    set_update,                     // tp_update

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
// TODO Do an efficient shallow copy if iterable is a fellow set (in yp_frozenset/yp_set)
static ypObject *_ypSet( int type, ypObject *iterable )
{
    ypObject *exc = yp_None;
    ypObject *newSo;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( iterable, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( iterable, &exc );
    } else if( lenhint == 0 && type == ypFrozenSet_CODE ) {
        // yp_lenC reports an empty iterable, so we can shortcut frozenset creation
        return _yp_frozenset_empty;
    }

    newSo = _ypSet_new( type, lenhint );
    if( yp_isexceptionC( newSo ) ) return newSo;
    // TODO make sure _yp_set_update is efficient for pre-sized objects
    result = _ypSet_update( newSo, iterable );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newSo );
        return result;
    }
    return newSo;
}

static ypObject *_yp_frozensetNV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypSet( ypFrozenSet_CODE, iter_args );
}
ypObject *yp_frozensetN( int n, ... ) {
    if( n < 1 ) return _yp_frozenset_empty;
    return_yp_V_FUNC( ypObject *, _yp_frozensetNV, (n, args), n );
}
ypObject *yp_frozensetNV( int n, va_list args ) {
    if( n < 1 ) return _yp_frozenset_empty;
    return _yp_frozensetNV( n, args );
}
ypObject *yp_frozenset( ypObject *iterable ) {
    if( ypObject_TYPE_CODE( iterable ) == ypFrozenSet_CODE ) return yp_incref( iterable );
    return _ypSet( ypFrozenSet_CODE, iterable );
}

static ypObject *_yp_setNV( int n, va_list args ) {
    yp_ONSTACK_ITER_VALIST( iter_args, n, args );
    return _ypSet( ypSet_CODE, iter_args );
}
ypObject *yp_setN( int n, ... ) {
    if( n < 1 ) return _ypSet_new( ypSet_CODE, 0 );
    return_yp_V_FUNC( ypObject *, _yp_setNV, (n, args), n );
}
ypObject *yp_setNV( int n, va_list args ) {
    if( n < 1 ) return _ypSet_new( ypSet_CODE, 0 );
    return _yp_setNV( n, args );
}
ypObject *yp_set( ypObject *iterable ) {
    return _ypSet( ypSet_CODE, iterable );
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

// ypDictObject and ypDict_LEN are defined above, for use by the set code
#define ypDict_KEYSET( mp )         ( ((ypDictObject *)mp)->keyset )
#define ypDict_ALLOCLEN( mp )       ypSet_ALLOCLEN( ypDict_KEYSET( mp ) )
#define ypDict_VALUES( mp )         ( (ypObject **) ((ypObject *)mp)->ob_data )
#define ypDict_SET_VALUES( mp, x )  ( ((ypObject *)mp)->ob_data = x )
#define ypDict_INLINE_DATA( mp )    ( ((ypDictObject *)mp)->ob_inline_data )

// XXX The dict's alloclen is always equal to the keyset alloclen, and we don't use the standard
// ypMem_* macros to resize the dict, so we repurpose mp->ob_alloclen to be the search finger for
// dict_popitem; if it gets corrupted, it's no big deal, because it's just a place to start
// searching
#define ypDict_POPITEM_FINGER( mp ) ( ((ypObject *)mp)->ob_alloclen )

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
    { ypFrozenDict_CODE, ypObject_REFCNT_IMMORTAL,
    0, ypSet_MINSIZE, ypObject_HASH_INVALID, _yp_frozendict_empty_data },
    (ypObject *) &_yp_frozenset_empty_struct };
static ypObject * const _yp_frozendict_empty = (ypObject *) &_yp_frozendict_empty_struct;

// TODO can use CONTAINER_INLINE if we're sure the frozendict won't grow past minused while being
// created
// XXX A minused of zero may mean lenhint is unreliable; we may still grow the frozendict, so don't
// return _yp_frozendict_empty!
static ypObject *_ypDict_new( int type, yp_ssize_t minused )
{
    ypObject *keyset;
    yp_ssize_t alloclen;
    ypObject *mp;

    keyset = _ypSet_new( ypFrozenSet_CODE, minused );
    if( yp_isexceptionC( keyset ) ) return keyset;
    alloclen = ypSet_ALLOCLEN( keyset );
    mp = ypMem_MALLOC_CONTAINER_VARIABLE( ypDictObject, type, alloclen, 0 );
    if( yp_isexceptionC( mp ) ) {
        yp_decref( keyset );
        return mp;
    }
    ypDict_KEYSET( mp ) = keyset;
    memset( ypDict_VALUES( mp ), 0, alloclen * sizeof( ypObject * ) );
    return mp;
}

// Discards all value references; called before clearing or deleting the dict
static void _ypDict_discard_value_refs( ypObject *mp )
{
    ypObject **values = ypDict_VALUES( mp );
    yp_ssize_t valuesleft = ypDict_LEN( mp );
    yp_ssize_t i;

    for( i = 0; valuesleft > 0; i++ ) {
        if( values[i] == NULL ) continue;
        valuesleft -= 1;
        yp_decref( values[i] );
    }
}

// XXX Check for the "lazy shallow copy" and "_yp_frozendict_empty" cases first
static ypObject *_ypDict_copy_shallow( int type, ypObject *x )
{
    ypObject *keyset;
    yp_ssize_t alloclen;
    ypObject *mp;
    ypObject **values;
    yp_ssize_t valuesleft;
    yp_ssize_t i;

    // Share the keyset object with our fellow dict
    keyset = ypDict_KEYSET( x );
    alloclen = ypSet_ALLOCLEN( keyset );
    mp = ypMem_MALLOC_CONTAINER_VARIABLE( ypDictObject, type, alloclen, 0 );
    if( yp_isexceptionC( mp ) ) return mp;
    ypDict_KEYSET( mp ) = yp_incref( keyset );

    // Now copy over the values; since we share keysets, the values all line up in the same place
    valuesleft = ypDict_LEN( mp ) = ypDict_LEN( x );
    values = ypDict_VALUES( mp );
    memcpy( values, ypDict_VALUES( x ), alloclen * sizeof( ypObject * ) );
    for( i = 0; valuesleft > 0; i++ ) {
        if( values[i] == NULL ) continue;
        valuesleft -= 1;
        yp_incref( values[i] );
    }
    return mp;
}

// Will perform a lazy shallow copy if copy_visitor is yp_shallowcopy_visitor
// XXX Check for the _yp_frozendict_empty case first
// TODO If x contains quite a lot of waste vis-a-vis unused keys from the keyset, then consider
// either a) optimizing x first, or b) not sharing the keyset of this object
static ypObject *_ypDict_copy( int type, ypObject *x, visitfunc copy_visitor, void *copy_memo )
{
    // If we are performing a shallow copy, we can share keysets and quickly memcpy the values
    if( copy_visitor == yp_shallowcopy_visitor ) {
        // A shallow copy of a frozendict to a frozendict doesn't require an actual copy
        if( type == ypFrozenDict_CODE && ypObject_TYPE_CODE( x ) == ypFrozenDict_CODE ) {
            return yp_incref( x );
        }
        return _ypDict_copy_shallow( type, x );
    }

    // Otherwise, copying takes a bit more effort
    return yp_NotImplementedError;
}

// The tricky bit about resizing dicts is that we need both the old and new keysets and value
// arrays to properly transfer the data, so ypMem_REALLOC_CONTAINER_VARIABLE is no help.
// XXX If ever this is rewritten to use the ypMem_* macros, remember that mp->ob_alloclen is _not_
// the allocation length: it has been repurposed (see ypDict_POPITEM_FINGER)
// TODO Do we want to split minused into required and extra, like in other areas?
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
    newkeyset = _ypSet_new( ypFrozenSet_CODE, minused );
    if( yp_isexceptionC( newkeyset ) ) return newkeyset;
    newalloclen = ypSet_ALLOCLEN( newkeyset );
    // TODO check for overflow?!
    newvalues = (ypObject **) yp_malloc( &newsize, newalloclen * sizeof( ypObject * ) );
    if( newvalues == NULL ) {
        yp_decref( newkeyset );
        return yp_MemoryError;
    }
    // TODO check for overflow?!
    memset( newvalues, 0, newalloclen * sizeof( ypObject * ) );

    oldkeys = ypSet_TABLE( ypDict_KEYSET( mp ) );
    oldvalues = ypDict_VALUES( mp );
    valuesleft = ypDict_LEN( mp );
    for( i = 0; valuesleft > 0; i++ ) {
        value = ypDict_VALUES( mp )[i];
        if( value == NULL ) continue;
        valuesleft -= 1;
        _ypSet_movekey_clean( newkeyset, yp_incref( oldkeys[i].se_key ), oldkeys[i].se_hash,
                &newkey_loc );
        newvalues[ypSet_ENTRY_INDEX( newkeyset, newkey_loc )] = oldvalues[i];
    }

    yp_decref( ypDict_KEYSET( mp ) );
    ypDict_KEYSET( mp ) = newkeyset;
    if( oldvalues != ypDict_INLINE_DATA( mp ) ) yp_free( oldvalues );
    ypDict_SET_VALUES( mp, newvalues );
    return yp_None;
}

// Adds a new key with the given hash at the given key_loc, which may require a resize, and sets
// value appropriately.  *key_loc must point to a currently-unused location in the hash table; it
// will be updated if a resize occurs.  Otherwise behaves as _ypDict_push.
// XXX Adapted from PyDict_SetItem
// TODO The decision to resize currently depends only on _ypSet_space_remaining, but what if the
// shared keyset contains 5x the keys that we actually use?  That's a large waste in the value
// table.  Really, we should have a _ypDict_space_remaining.
static ypObject *_ypDict_push_newkey( ypObject *mp, ypSet_KeyEntry **key_loc, ypObject *key,
        yp_hash_t hash, ypObject *value, yp_ssize_t *spaceleft, yp_ssize_t growhint )
{
    ypObject *keyset = ypDict_KEYSET( mp );
    ypObject *result;
    yp_ssize_t newlen;

    // It's possible we can add the key without resizing
    if( *spaceleft >= 1 ) {
        _ypSet_movekey( keyset, *key_loc, yp_incref( key ), hash );
        *ypDict_VALUE_ENTRY( mp, *key_loc ) = yp_incref( value );
        ypDict_LEN( mp ) += 1;
        *spaceleft -= 1;
        return yp_True;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_movekey_clean.  Give mutable objects a bit of room to grow.
    if( growhint < 0 ) growhint = 0;
    // FIXME check for newlen overflow?
    newlen = ypDict_LEN( mp )+1 + growhint;
    result = _ypDict_resize( mp, newlen );  // invalidates keyset and *key_loc
    if( yp_isexceptionC( result ) ) return result;

    keyset = ypDict_KEYSET( mp );
    _ypSet_movekey_clean( keyset, yp_incref( key ), hash, key_loc );
    *ypDict_VALUE_ENTRY( mp, *key_loc ) = yp_incref( value );
    ypDict_LEN( mp ) += 1;
    *spaceleft = _ypSet_space_remaining( keyset );
    return yp_True;
}

// Adds the key/value to the dict.  If override is false, returns yp_False and does not modify the
// dict if there is an existing value.  *spaceleft should be initialized from
// _ypSet_space_remaining; this function then decrements it with each key added, and resets it on
// every resize.  Returns yp_True if mp was modified, yp_False if it wasn't due to existing values
// being preserved (ie override is false), or an exception on error.
// XXX Adapted from PyDict_SetItem
static ypObject *_ypDict_push( ypObject *mp, ypObject *key, ypObject *value, int override,
        yp_ssize_t *spaceleft, yp_ssize_t growhint )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *key_loc;
    ypObject *result = yp_None;
    ypObject **value_loc;

    // Look for the appropriate entry in the hash table
    // TODO yp_isexceptionC used internally should be a macro
    hash = yp_hashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;  // also verifies key is not an exception
    if( yp_isexceptionC( value ) ) return value;    // verifies value is not an exception
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

    // Otherwise, we need to add both the key _and_ value, which may involve resizing
    return _ypDict_push_newkey( mp, &key_loc, key, hash, value, spaceleft, growhint );
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
// XXX Yes, the yielded value can be any iterable, even a set or dict (good luck guessing which
// will be the key, and which the value)
// XXX itemiter may be an yp_ONSTACK_ITER_KVALIST: use carefully
static void _ypDict_iter_items_next( ypObject *itemiter, ypObject **key, ypObject **value )
{
    ypObject *keyvaliter = yp_next( itemiter ); // new ref
    if( yp_isexceptionC( keyvaliter ) ) { // including yp_StopIteration
        *key = *value = keyvaliter;
        return;
    }
    yp_unpackN( keyvaliter, 2, key, value );
}

// XXX Check for the mp==other case _before_ calling this function
// XXX We're trusting that copy_visitor will behave properly and return an object that has the same
// hash as the original and that is unequal to anything else in the other set
static ypObject *_ypDict_update_from_dict( ypObject *mp, ypObject *other )
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( ypDict_KEYSET( mp ) );
    yp_ssize_t valuesleft = ypDict_LEN( other );
    ypObject *other_keyset = ypDict_KEYSET( other );
    yp_ssize_t i;
    ypObject *other_value;
    ypObject *result;

    for( i = 0; valuesleft > 0; i++ ) {
        other_value = ypDict_VALUES( other )[i];
        if( other_value == NULL ) continue;
        valuesleft -= 1;

        // TODO _ypDict_push will call yp_hashC again, even though we already know the hash
        result = _ypDict_push( mp, ypSet_TABLE( other_keyset )[i].se_key, other_value, 1,
                &spaceleft, valuesleft );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

// XXX *itemiter may be an yp_ONSTACK_ITER_KVALIST: use carefully
static ypObject *_ypDict_update_from_iter( ypObject *mp, ypObject *itemiter )
{
    ypObject *exc = yp_None;
    ypObject *result;
    ypObject *key;
    ypObject *value;
    yp_ssize_t spaceleft = _ypSet_space_remaining( ypDict_KEYSET( mp ) );
    yp_ssize_t lenhint = yp_iter_lenhintC( itemiter, &exc ); // zero on error

    while( 1 ) {
        _ypDict_iter_items_next( itemiter, &key, &value ); // new refs: key, value
        if( yp_isexceptionC( key ) ) {
            if( yp_isexceptionC2( key, yp_StopIteration ) ) break;
            return key;
        }
        lenhint -= 1; // check for <0 only when we need it in _ypDict_push
        result = _ypDict_push( mp, key, value, 1, &spaceleft, lenhint );
        yp_decrefN( 2, key, value );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

// Adds the key/value pairs yielded from either yp_iter_items or yp_iter to the dict.  If the dict
// has enough space to hold all the items, the dict is not resized (important, as yp_dictK et al
// pre-allocate the necessary space).
// XXX Check for the mp==x case _before_ calling this function
static ypObject *_ypDict_update( ypObject *mp, ypObject *x )
{
    int x_pair = ypObject_TYPE_PAIR_CODE( x );
    ypObject *itemiter;
    ypObject *result;

    // If x is a fellow dict there are efficiencies we can exploit; otherwise, prefer yp_iter_items
    // over yp_iter if supported.  Recall that type pairs are identified by the immutable type
    // code.
    if( x_pair == ypFrozenDict_CODE ) {
        return _ypDict_update_from_dict( mp, x );
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

static ypObject *_frozendict_decref_visitor( ypObject *x, void *memo ) {
    yp_decref( x );
    return yp_None;
}

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
        valuesleft -= 1;
        result = visitor( value, memo );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *frozendict_unfrozen_copy( ypObject *x, visitfunc copy_visitor, void *copy_memo ) {
    return _ypDict_copy( ypDict_CODE, x, copy_visitor, copy_memo );
}

static ypObject *frozendict_frozen_copy( ypObject *x, visitfunc copy_visitor, void *copy_memo ) {
    if( ypDict_LEN( x ) < 1 ) return _yp_frozendict_empty;
    return _ypDict_copy( ypFrozenDict_CODE, x, copy_visitor, copy_memo );
}

static ypObject *frozendict_bool( ypObject *mp ) {
    return ypBool_FROM_C( ypDict_LEN( mp ) );
}

// FIXME comparison functions can recurse, just like currenthash...fix!
static ypObject *frozendict_eq( ypObject *mp, ypObject *x )
{
    yp_ssize_t valuesleft;
    yp_ssize_t mp_i;
    ypObject *mp_value;
    ypSet_KeyEntry *mp_key_loc;
    ypSet_KeyEntry *x_key_loc;
    ypObject *x_value;
    ypObject *result;

    if( mp == x ) return yp_True;
    if( ypObject_TYPE_PAIR_CODE( x ) != ypFrozenDict_CODE ) return yp_ComparisonNotImplemented;
    if( ypDict_LEN( mp ) != ypDict_LEN( x ) ) return yp_False;
    // FIXME Compare stored hashes (they should be equal if mp and x are equal)

    valuesleft = ypDict_LEN( mp );
    for( mp_i = 0; valuesleft > 0; mp_i++ ) {
        mp_value = ypDict_VALUES( mp )[mp_i];
        if( mp_value == NULL ) continue;
        valuesleft -= 1;
        mp_key_loc = ypSet_TABLE( ypDict_KEYSET( mp ) ) + mp_i;

        // If the key is not also in x, mp and x are not equal
        result = _ypSet_lookkey( ypDict_KEYSET( x ),
                mp_key_loc->se_key, mp_key_loc->se_hash, &x_key_loc );
        if( yp_isexceptionC( result ) ) return result;
        x_value = *ypDict_VALUE_ENTRY( x, x_key_loc );
        if( x_value == NULL ) return yp_False;

        // If the values are not equal, than neither are mp and x
        result = yp_eq( mp_value, x_value );
        if( result != yp_True ) return result; // yp_False or an exception
    }
    return yp_True;
}

static ypObject *frozendict_ne( ypObject *mp, ypObject *x ) {
    ypObject *result = frozendict_eq( mp, x );
    return ypBool_NOT( result );
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

static ypObject *dict_clear( ypObject *mp ) {
    ypObject *keyset;
    yp_ssize_t alloclen;
    if( ypDict_LEN( mp ) < 1 ) return yp_None;

    keyset = _ypSet_new( ypFrozenSet_CODE, 0 );
    if( yp_isexceptionC( keyset ) ) return keyset;
    alloclen = ypSet_ALLOCLEN( keyset );

    yp_decref( ypDict_KEYSET( mp ) );
    _ypDict_discard_value_refs( mp );
    // FIXME there's a memcpy in this that we can and should avoid
    ypMem_REALLOC_CONTAINER_VARIABLE( mp, ypDictObject, alloclen, 0 );
    yp_ASSERT( ypDict_VALUES( mp ) == ypDict_INLINE_DATA( mp ), "dict_clear didn't allocate inline!" ); 
    // XXX if the realloc fails, we are still pointing at valid, if over-sized, memory
    ypDict_LEN( mp ) = 0;
    ypDict_KEYSET( mp ) = keyset;
    memset( ypDict_VALUES( mp ), 0, alloclen * sizeof( ypObject * ) );
    return yp_None;
}

// A defval of NULL means to raise an error if key is not in dict
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
    // TODO Adapt PyDict_SetItem logic via growhint
    ypObject *result = _ypDict_push( mp, key, value, 1, &spaceleft, 0 );
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

static ypObject *dict_popvalue( ypObject *mp, ypObject *key, ypObject *defval )
{
    ypObject *result = _ypDict_pop( mp, key );
    if( result == ypSet_dummy ) return yp_incref( defval );
    return result;
}

// Note the difference between this, which removes an arbitrary item, and
// _ypDict_pop, which removes a specific item
// XXX Make sure not to leave references in *key and *value on error
// XXX Adapted from Python's set_pop
static ypObject *dict_popitem( ypObject *mp, ypObject **key, ypObject **value )
{
    register yp_ssize_t i;
    register ypObject **values = ypDict_VALUES( mp );

    if( ypDict_LEN( mp ) < 1 ) return yp_KeyError; // "pop from an empty dict"

    /* We abuse mp->ob_alloclen to hold a search finger; recall
     * ypDict_ALLOCLEN uses the keyset's ob_alloclen
     */
    i = ypDict_POPITEM_FINGER( mp );
    /* The ob_alloclen may be a real allocation length, or it may
     * be a legit search finger, or it may be a once-legit search
     * finger that's out of bounds now because it wrapped around
     * or the table shrunk -- simply make sure it's in bounds now.
     */
    if( i >= ypDict_ALLOCLEN( mp ) || i < 0 ) i = 0;
    while( values[i] == NULL ) {
        i++;
        if( i >= ypDict_ALLOCLEN( mp ) ) i = 0;
    }
    *key = yp_incref( ypSet_TABLE( ypDict_KEYSET( mp ) )[i].se_key );
    *value = values[i];
    values[i] = NULL;
    ypDict_LEN( mp ) -= 1;
    ypDict_POPITEM_FINGER( mp ) = i + 1;  /* next place to start */
    return yp_None;
}

static ypObject *dict_setdefault( ypObject *mp, ypObject *key, ypObject *defval )
{
    yp_hash_t hash;
    ypObject *keyset = ypDict_KEYSET( mp );
    ypSet_KeyEntry *key_loc;
    ypObject *result = yp_None;
    ypObject **value_loc;

    // Look for the appropriate entry in the hash table; note that key can be a mutable object,
    // because we are not adding it to the set
    hash = yp_currenthashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;  // returns if key is an exception
    if( yp_isexceptionC( defval ) ) return defval;  // returns if defval is an exception
    result = _ypSet_lookkey( keyset, key, hash, &key_loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the there's no existing value, add and return defval, otherwise return the value
    value_loc = ypDict_VALUE_ENTRY( mp, key_loc );
    if( *value_loc == NULL ) {
        if( ypSet_ENTRY_USED( key_loc ) ) {
            *value_loc = yp_incref( defval );
            ypDict_LEN( mp ) += 1;
        } else {
            yp_ssize_t spaceleft = _ypSet_space_remaining( keyset );
            // TODO Adapt PyDict_SetItem logic via growhint
            result = _ypDict_push_newkey( mp, &key_loc, key, hash, defval, &spaceleft, 0 );
            // value_loc is no longer valid
            if( yp_isexceptionC( result ) ) return result;
        }
        return yp_incref( defval );
    } else {
        return yp_incref( *value_loc );
    }
}

// TODO Investigate if this is better than using yp_ONSTACK_ITER_KVALIST and, if so, update other
// "K" functions similarly (I'm pretty sure it is)
// FIXME instead, wait until we need a resize, then since we're resizing anyway, resize to fit
// the given n
static ypObject *dict_updateK( ypObject *mp, int n, va_list args )
{
    yp_ssize_t spaceleft = _ypSet_space_remaining( ypDict_KEYSET( mp ) );
    ypObject *result = yp_None;
    ypObject *key;
    ypObject *value;

    while( n > 0 ) {
        key = va_arg( args, ypObject * ); // borrowed
        value = va_arg( args, ypObject * ); // borrowed
        n -= 1;
        result = _ypDict_push( mp, key, value, 1, &spaceleft, n );
        if( yp_isexceptionC( result ) ) return result;
    }
    return yp_None;
}

static ypObject *dict_update( ypObject *mp, int n, va_list args )
{
    ypObject *result;
    for( /*n already set*/; n > 0; n-- ) {
        ypObject *x = va_arg( args, ypObject * );
        if( mp == x ) continue;
        result = _ypDict_update( mp, x );
        if( yp_isexceptionC( result ) ) return result;
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
yp_STATIC_ASSERT( ypSet_LEN_MAX <= 0x7FFFFFFFu, len_fits_31_bits );
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
    yp_decref( ypDict_KEYSET( mp ) );
    _ypDict_discard_value_refs( mp );
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
    MethodError_objvalistproc,      // tp_updateK
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
    frozendict_unfrozen_copy,       // tp_unfrozen_copy
    frozendict_frozen_copy,         // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    frozendict_bool,                // tp_bool
    NotImplemented_comparefunc,     // tp_lt
    NotImplemented_comparefunc,     // tp_le
    frozendict_eq,                  // tp_eq
    frozendict_ne,                  // tp_ne
    NotImplemented_comparefunc,     // tp_ge
    NotImplemented_comparefunc,     // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    frozendict_miniiter_keys,       // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    frozendict_miniiter_next,       // tp_miniiter_next
    frozendict_miniiter_lenhint,    // tp_miniiter_lenhint
    frozendict_iter_keys,           // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

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
    MethodError_objvalistproc,      // tp_update

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
    dict_popvalue,                  // tp_popvalue
    dict_popitem,                   // tp_popitem
    dict_setdefault,                // tp_setdefault
    dict_updateK,                   // tp_updateK
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
    frozendict_unfrozen_copy,       // tp_unfrozen_copy
    frozendict_frozen_copy,         // tp_frozen_copy
    MethodError_objproc,            // tp_invalidate

    // Boolean operations and comparisons
    frozendict_bool,                // tp_bool
    NotImplemented_comparefunc,     // tp_lt
    NotImplemented_comparefunc,     // tp_le
    frozendict_eq,                  // tp_eq
    frozendict_ne,                  // tp_ne
    NotImplemented_comparefunc,     // tp_ge
    NotImplemented_comparefunc,     // tp_gt

    // Generic object operations
    MethodError_hashfunc,           // tp_currenthash
    MethodError_objproc,            // tp_close

    // Number operations
    MethodError_NumberMethods,      // tp_as_number

    // Iterator operations
    frozendict_miniiter_keys,       // tp_miniiter
    TypeError_miniiterfunc,         // tp_miniiter_reversed
    frozendict_miniiter_next,       // tp_miniiter_next
    frozendict_miniiter_lenhint,    // tp_miniiter_lenhint
    frozendict_iter_keys,           // tp_iter
    TypeError_objproc,              // tp_iter_reversed
    TypeError_objobjproc,           // tp_send

    // Container operations
    frozendict_contains,            // tp_contains
    frozendict_len,                 // tp_len
    MethodError_objobjproc,         // tp_push
    dict_clear,                     // tp_clear
    MethodError_objproc,            // tp_pop
    MethodError_objobjobjproc,      // tp_remove
    frozendict_getdefault,          // tp_getdefault
    dict_setitem,                   // tp_setitem
    dict_delitem,                   // tp_delitem
    dict_update,                    // tp_update

    // Sequence operations
    MethodError_SequenceMethods,    // tp_as_sequence

    // Set operations
    MethodError_SetMethods,         // tp_as_set

    // Mapping operations
    &ypDict_as_mapping              // tp_as_mapping
};

// Constructors
// XXX x may be an yp_ONSTACK_ITER_KVALIST: use carefully
// XXX Always creates a new keyset; if you want to share x's keyset, use _ypDict_copy_shallow
// TODO Perhaps this should be broken up, and a _ypDict_update_from_valist created
static ypObject *_ypDict( int type, ypObject *x )
{
    ypObject *exc = yp_None;
    ypObject *newMp;
    ypObject *result;
    yp_ssize_t lenhint = yp_lenC( x, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( x, &exc );
    } else if( lenhint == 0 && type == ypFrozenDict_CODE ) {
        // yp_lenC reports an empty iterable, so we can shortcut frozendict creation
        return _yp_frozendict_empty;
    }

    newMp = _ypDict_new( type, lenhint );
    if( yp_isexceptionC( newMp ) ) return newMp;
    // TODO make sure _ypDict_update is efficient for pre-sized objects
    result = _ypDict_update( newMp, x );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newMp );
        return result;
    }
    return newMp;
}

static ypObject *_yp_frozendictKV( int n, va_list args ) {
    yp_ONSTACK_ITER_KVALIST( iter_args, n, args );
    return _ypDict( ypFrozenDict_CODE, iter_args );
}
ypObject *yp_frozendictK( int n, ... ) {
    if( n < 1 ) return _yp_frozendict_empty;
    return_yp_V_FUNC( ypObject *, _yp_frozendictKV, (n, args), n );
}
ypObject *yp_frozendictKV( int n, va_list args ) {
    if( n < 1 ) return _yp_frozendict_empty;
    return _yp_frozendictKV( n, args );
}
ypObject *yp_frozendict( ypObject *x ) {
    // If x is a fellow dict then perform a copy so we can share keysets
    if( ypObject_TYPE_PAIR_CODE( x ) == ypFrozenDict_CODE ) {
        if( ypDict_LEN( x ) < 1 ) return _yp_frozendict_empty;
        if( ypObject_TYPE_CODE( x ) == ypFrozenDict_CODE ) return yp_incref( x );
        return _ypDict_copy_shallow( ypFrozenDict_CODE, x );
    }
    return _ypDict( ypFrozenDict_CODE, x );
}

static ypObject *_yp_dictKV( int n, va_list args ) {
    yp_ONSTACK_ITER_KVALIST( iter_args, n, args );
    return _ypDict( ypDict_CODE, iter_args );
}
ypObject *yp_dictK( int n, ... ) {
    if( n < 1 ) return _ypDict_new( ypDict_CODE, 0 );
    return_yp_V_FUNC( ypObject *, _yp_dictKV, (n, args), n );
}
ypObject *yp_dictKV( int n, va_list args ) {
    if( n < 1 ) return _ypDict_new( ypDict_CODE, 0 );
    return _yp_dictKV( n, args );
}
ypObject *yp_dict( ypObject *x ) {
    // If x is a fellow dict then perform a copy so we can share keysets
    if( ypObject_TYPE_PAIR_CODE( x ) == ypFrozenDict_CODE ) {
        return _ypDict_copy_shallow( ypDict_CODE, x );
    }
    return _ypDict( ypDict_CODE, x );
}

static ypObject *_ypDict_fromkeysNV( int type, ypObject *value, int n, va_list args )
{
    yp_ssize_t spaceleft;
    ypObject *result = yp_None;
    ypObject *key;
    ypObject *newMp;

    newMp = _ypDict_new( type, n );
    if( yp_isexceptionC( newMp ) ) return newMp;
    spaceleft = _ypSet_space_remaining( ypDict_KEYSET( newMp ) );

    while( n > 0 ) {
        key = va_arg( args, ypObject * ); // borrowed
        n -= 1;
        result = _ypDict_push( newMp, key, value, 1, &spaceleft, n );
        if( yp_isexceptionC( result ) ) break;
    }
    if( yp_isexceptionC( result ) ) {
        yp_decref( newMp );
        return result;
    }
    return newMp;
}

ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... ) {
    if( n < 1 ) return _yp_frozendict_empty;
    return_yp_V_FUNC( ypObject *, _ypDict_fromkeysNV, (ypFrozenDict_CODE, value, n, args), n );
}
ypObject *yp_frozendict_fromkeysNV( ypObject *value, int n, va_list args ) {
    if( n < 1 ) return _yp_frozendict_empty;
    return _ypDict_fromkeysNV( ypFrozenDict_CODE, value, n, args );
}

ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... ) {
    if( n < 1 ) return _ypDict_new( ypDict_CODE, 0 );
    return_yp_V_FUNC( ypObject *, _ypDict_fromkeysNV, (ypDict_CODE, value, n, args), n );
}
ypObject *yp_dict_fromkeysNV( ypObject *value, int n, va_list args ) {
    if( n < 1 ) return _ypDict_new( ypDict_CODE, 0 );
    return _ypDict_fromkeysNV( ypDict_CODE, value, n, args );
}


static ypObject *_ypDict_fromkeys( int type, ypObject *iterable, ypObject *value )
{
    ypObject *exc = yp_None;
    ypObject *result = yp_None;
    ypObject *mi;
    yp_uint64_t mi_state;
    ypObject *newMp;
    yp_ssize_t spaceleft;
    ypObject *key;
    yp_ssize_t lenhint = yp_lenC( iterable, &exc );
    if( yp_isexceptionC( exc ) ) {
        // Ignore errors determining lenhint; it just means we can't pre-allocate
        lenhint = yp_iter_lenhintC( iterable, &exc );
    } else if( lenhint == 0 && type == ypFrozenDict_CODE ) {
        // yp_lenC reports an empty iterable, so we can shortcut frozendict creation
        return _yp_frozendict_empty;
    }

    mi = yp_miniiter( iterable, &mi_state ); // new ref
    if( yp_isexceptionC( mi ) ) return mi;

    newMp = _ypDict_new( type, lenhint ); // new ref
    if( yp_isexceptionC( newMp ) ) {
        yp_decref( mi );
        return newMp;
    }
    spaceleft = _ypSet_space_remaining( ypDict_KEYSET( newMp ) );

    while( 1 ) {
        key = yp_miniiter_next( mi, &mi_state ); // new ref
        if( yp_isexceptionC( key ) ) {
            if( yp_isexceptionC2( key, yp_StopIteration ) ) break; // end of iterator
            result = key;
            break;
        }
        lenhint -= 1;
        result = _ypDict_push( newMp, key, value, 1, &spaceleft, lenhint );
        yp_decref( key );
        if( yp_isexceptionC( result ) ) break;
    }
    yp_decref( mi );
    if( yp_isexceptionC( result ) ) {
        yp_decref( newMp );
        return result;
    }
    return newMp;
}

ypObject *yp_frozendict_fromkeys( ypObject *iterable, ypObject *value ) {
    return _ypDict_fromkeys( ypFrozenDict_CODE, iterable, value );
}
ypObject *yp_dict_fromkeys( ypObject *iterable, ypObject *value ) {
    return _ypDict_fromkeys( ypDict_CODE, iterable, value );
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

#define _yp_REDIRECT_EXC1( ob, tp_meth, args, pExc ) \
    do {ypTypeObject *type = ypObject_TYPE( ob ); \
        ypObject *result = type->tp_meth args; \
        if( yp_isexceptionC( result ) ) *pExc = result; \
        return; } while( 0 )

#define _yp_REDIRECT_EXC2( ob, tp_suite, suite_meth, args, pExc ) \
    do {ypTypeObject *type = ypObject_TYPE( ob ); \
        ypObject *result = type->tp_suite->suite_meth args; \
        if( yp_isexceptionC( result ) ) *pExc = result; \
        return; } while( 0 )

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
}

ypObject *yp_next( ypObject *iterator ) {
    _yp_REDIRECT1( iterator, tp_send, (iterator, yp_None) );
}

ypObject *yp_throw( ypObject *iterator, ypObject *exc ) {
    if( !yp_isexceptionC( exc ) ) return yp_TypeError;
    _yp_REDIRECT1( iterator, tp_send, (iterator, exc) );
}

ypObject *yp_reversed( ypObject *x ) {
    _yp_REDIRECT1( x, tp_iter_reversed, (x) );
    // TODO Ensure the result is an iterator or an exception
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
    _yp_REDIRECT2( sequence, tp_as_sequence, tp_concat, (sequence, x) );
}

ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor ) {
    _yp_REDIRECT2( sequence, tp_as_sequence, tp_repeat, (sequence, factor) );
}

ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i ) {
    _yp_REDIRECT2( sequence, tp_as_sequence, tp_getindex, (sequence, i, NULL) );
}

ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k ) {
    _yp_REDIRECT2( sequence, tp_as_sequence, tp_getslice, (sequence, i, j, k) );
}

yp_ssize_t yp_findC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc )
{
    yp_ssize_t index;
    ypObject *result = ypObject_TYPE( sequence )->tp_as_sequence->tp_find(
            sequence, x, i, j, yp_FIND_FORWARD, &index );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( -1, exc, result );
    yp_ASSERT( index >= -1, "tp_find cannot return <-1" );
    return index;
}

yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_findC4( sequence, x, 0, yp_SLICE_USELEN, exc );
}

yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc )
{
    ypObject *subexc = yp_None;
    yp_ssize_t result = yp_findC4( sequence, x, i, j, &subexc );
    if( yp_isexceptionC( subexc ) ) return_yp_CEXC_ERR( -1, exc, subexc );
    if( result == -1 ) return_yp_CEXC_ERR( -1, exc, yp_ValueError );
    return result;
}

yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_indexC4( sequence, x, 0, yp_SLICE_USELEN, exc );
}

yp_ssize_t yp_rfindC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc )
{
    yp_ssize_t index;
    ypObject *result = ypObject_TYPE( sequence )->tp_as_sequence->tp_find(
            sequence, x, i, j, yp_FIND_REVERSE, &index );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( -1, exc, result );
    yp_ASSERT( index >= -1, "tp_find cannot return <-1" );
    return index;
}

yp_ssize_t yp_rfindC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_rfindC4( sequence, x, 0, yp_SLICE_USELEN, exc );
}

yp_ssize_t yp_rindexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc )
{
    ypObject *subexc = yp_None;
    yp_ssize_t result = yp_rfindC4( sequence, x, i, j, &subexc );
    if( yp_isexceptionC( subexc ) ) return_yp_CEXC_ERR( -1, exc, subexc );
    if( result == -1 ) return_yp_CEXC_ERR( -1, exc, yp_ValueError );
    return result;
}

yp_ssize_t yp_rindexC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_rindexC4( sequence, x, 0, yp_SLICE_USELEN, exc );
}

yp_ssize_t yp_countC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc )
{
    yp_ssize_t count;
    ypObject *result = ypObject_TYPE( sequence )->tp_as_sequence->tp_count(
            sequence, x, i, j, &count );
    if( yp_isexceptionC( result ) ) return_yp_CEXC_ERR( 0, exc, result );
    yp_ASSERT( count >= 0, "tp_count cannot return negative" );
    return count;
}

yp_ssize_t yp_countC( ypObject *sequence, ypObject *x, ypObject **exc ) {
    return yp_countC4( sequence, x, 0, yp_SLICE_USELEN, exc );
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
    _yp_INPLACE2( sequence, tp_as_sequence, tp_append, (*sequence, x) );
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

ypObject *yp_unionN( ypObject *set, int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_unionNV, (set, n, args), n );
}
ypObject *yp_unionNV( ypObject *set, int n, va_list args ) {
    _yp_REDIRECT2( set, tp_as_set, tp_union, (set, n, args) );
}

ypObject *yp_intersectionN( ypObject *set, int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_intersectionNV, (set, n, args), n );
}
ypObject *yp_intersectionNV( ypObject *set, int n, va_list args ) {
    _yp_REDIRECT2( set, tp_as_set, tp_intersection, (set, n, args) );
}

ypObject *yp_differenceN( ypObject *set, int n, ... ) {
    return_yp_V_FUNC( ypObject *, yp_differenceNV, (set, n, args), n );
}
ypObject *yp_differenceNV( ypObject *set, int n, va_list args ) {
    _yp_REDIRECT2( set, tp_as_set, tp_difference, (set, n, args) );
}

ypObject *yp_symmetric_difference( ypObject *set, ypObject *x ) {
    _yp_REDIRECT2( set, tp_as_set, tp_symmetric_difference, (set, x) );
}

void yp_updateN( ypObject **set, int n, ... ) {
    return_yp_V_FUNC_void( yp_updateNV, (set, n, args), n );
}
void yp_updateNV( ypObject **set, int n, va_list args ) {
    _yp_INPLACE1( set, tp_update, (*set, n, args) );
}

void yp_intersection_updateN( ypObject **set, int n, ... ) {
    return_yp_V_FUNC_void( yp_intersection_updateNV, (set, n, args), n );
}
void yp_intersection_updateNV( ypObject **set, int n, va_list args ) {
    _yp_INPLACE2( set, tp_as_set, tp_intersection_update, (*set, n, args) );
}

void yp_difference_updateN( ypObject **set, int n, ... ) {
    return_yp_V_FUNC_void( yp_difference_updateNV, (set, n, args), n );
}
void yp_difference_updateNV( ypObject **set, int n, va_list args ) {
    _yp_INPLACE2( set, tp_as_set, tp_difference_update, (*set, n, args) );
}

void yp_symmetric_difference_update( ypObject **set, ypObject *x ) {
    _yp_INPLACE2( set, tp_as_set, tp_symmetric_difference_update, (*set, x) );
}

void yp_pushuniqueE( ypObject *set, ypObject *x, ypObject **exc ) {
    _yp_REDIRECT_EXC2( set, tp_as_set, tp_pushunique, (set, x), exc );
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

ypObject *yp_popvalue3( ypObject **mapping, ypObject *key, ypObject *defval ) {
    _yp_INPLACE_RETURN2( mapping, tp_as_mapping, tp_popvalue, (*mapping, key, defval) );
}

void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value ) {
    ypTypeObject *type = ypObject_TYPE( *mapping );
    ypObject *result = type->tp_as_mapping->tp_popitem( *mapping, key, value );
    if( yp_isexceptionC( result ) ) {
        *key = *value = result;
        yp_INPLACE_ERR( mapping, result );
    }
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
    _yp_INPLACE2( mapping, tp_as_mapping, tp_updateK, (*mapping, n, args) );
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
 * C-to-C container operations
 *************************************************************************************************/

// XXX You could spend *weeks* adding all sorts of nifty combinations to this section: DON'T.
// Let's restrain ourselves and limit them to functions we actually find useful (and can test) in
// our projects that integrate nohtyP.

int yp_o2i_containsC( ypObject *container, yp_int_t xC, ypObject **exc ) {
    ypObject *x = yp_intC( xC );
    ypObject *result = yp_contains( container, x );
    yp_decref( x );
    if( yp_isexceptionC( result ) ) *exc = result;
    return result == yp_True; // returns false on yp_False or exception
}

void yp_o2i_pushC( ypObject **container, yp_int_t xC ) {
    ypObject *x = yp_intC( xC );
    yp_push( container, x );
    yp_decref( x );
}

yp_int_t yp_o2i_popC( ypObject **container, ypObject **exc ) {
    ypObject *x = yp_pop( container );
    yp_int_t xC = yp_asintC( x, exc );
    yp_decref( x );
    return xC;
}

yp_int_t yp_o2i_getitemC( ypObject *container, ypObject *key, ypObject **exc ) {
    ypObject *x = yp_getitem( container, key );
    yp_int_t xC = yp_asintC( x, exc );
    yp_decref( x );
    return xC;
}

void yp_o2i_setitemC( ypObject **container, ypObject *key, yp_int_t xC ) {
    ypObject *x = yp_intC( xC );
    yp_setitem( container, key, x );
    yp_decref( x );
}

ypObject *yp_o2s_getitemCX( ypObject *container, ypObject *key, const yp_uint8_t * *encoded,
        yp_ssize_t *size, ypObject * *encoding )
{
    ypObject *x;
    ypObject *result;
    int container_pair = ypObject_TYPE_PAIR_CODE( container );

    // XXX The pointer returned via *encoded is only valid so long as the str/chrarray object
    // remains allocated and isn't modified.  As such, limit this function to those containers that
    // we *know* will keep the object allocated (so long as _they_ aren't modified, of course).
    if( container_pair != ypTuple_CODE && container_pair != ypFrozenDict_CODE ) {
        return_yp_BAD_TYPE( container );
    }

    x = yp_getitem( container, key );
    result = yp_asencodedCX( x, encoded, size, encoding );
    yp_decref( x );
    return result;
}

void yp_o2s_setitemC4( ypObject **container, ypObject *key,
        const yp_uint8_t *xC, yp_ssize_t x_lenC )
{
    ypObject *x = yp_str_frombytesC2( xC, x_lenC );
    yp_setitem( container, key, x );
    yp_decref( x );
}

ypObject *yp_i2o_getitemC( ypObject *container, yp_int_t keyC ) {
    ypObject *key = yp_intC( keyC );
    ypObject *x = yp_getitem( container, key );
    yp_decref( key );
    return x;
}

void yp_i2o_setitemC( ypObject **container, yp_int_t keyC, ypObject *x ) {
    ypObject *key = yp_intC( keyC );
    yp_setitem( container, key, x );
    yp_decref( key );
}

yp_int_t yp_i2i_getitemC( ypObject *container, yp_int_t keyC, ypObject **exc ) {
    ypObject *key = yp_intC( keyC );
    yp_int_t x = yp_o2i_getitemC( container, key, exc );
    yp_decref( key );
    return x;
}

void yp_i2i_setitemC( ypObject **container, yp_int_t keyC, yp_int_t xC ) {
    ypObject *key = yp_intC( keyC );
    yp_o2i_setitemC( container, key, xC );
    yp_decref( key );
}

ypObject *yp_i2s_getitemCX( ypObject *container, yp_int_t keyC, const yp_uint8_t * *encoded,
        yp_ssize_t *size, ypObject * *encoding )
{
    ypObject *key = yp_intC( keyC );
    ypObject *result = yp_o2s_getitemCX( container, key, encoded, size, encoding );
    yp_decref( key );
    return result;
}

void yp_i2s_setitemC4( ypObject **container, yp_int_t keyC,
        const yp_uint8_t *xC, yp_ssize_t x_lenC )
{
    ypObject *key = yp_intC( keyC );
    yp_o2s_setitemC4( container, key, xC, x_lenC );
    yp_decref( key );
}

ypObject *yp_s2o_getitemC3( ypObject *container, const yp_uint8_t *keyC, yp_ssize_t key_lenC ) {
    ypObject *key = yp_str_frombytesC2( keyC, key_lenC );
    ypObject *x = yp_getitem( container, key );
    yp_decref( key );
    return x;
}

void yp_s2o_setitemC4( ypObject **container, const yp_uint8_t *keyC, yp_ssize_t key_lenC,
        ypObject *x )
{
    ypObject *key = yp_str_frombytesC2( keyC, key_lenC );
    yp_setitem( container, key, x );
    yp_decref( key );
}

yp_int_t yp_s2i_getitemC3( ypObject *container, const yp_uint8_t *keyC, yp_ssize_t key_lenC,
        ypObject **exc )
{
    ypObject *key = yp_str_frombytesC2( keyC, key_lenC );
    yp_int_t x = yp_o2i_getitemC( container, key, exc );
    yp_decref( key );
    return x;
}

void yp_s2i_setitemC4( ypObject **container, const yp_uint8_t *keyC, yp_ssize_t key_lenC,
        yp_int_t xC )
{
    ypObject *key = yp_str_frombytesC2( keyC, key_lenC );
    yp_o2i_setitemC( container, key, xC );
    yp_decref( key );
}


/*************************************************************************************************
 * The type table, and related public functions and variables
 *************************************************************************************************/
// XXX Make sure this corresponds with ypInvalidated_CODE et al!

// Recall that C helpfully sets missing array elements to NULL
static ypTypeObject *ypTypeTable[255] = {
    &ypInvalidated_Type,/* ypInvalidated_CODE          (  0u) */
    &ypInvalidated_Type,/*                             (  1u) */
    &ypException_Type,  /* ypException_CODE            (  2u) */
    &ypException_Type,  /*                             (  3u) */
    &ypType_Type,       /* ypType_CODE                 (  4u) */
    &ypType_Type,       /*                             (  5u) */

    &ypNoneType_Type,   /* ypNoneType_CODE             (  6u) */
    &ypNoneType_Type,   /*                             (  7u) */
    &ypBool_Type,       /* ypBool_CODE                 (  8u) */
    &ypBool_Type,       /*                             (  9u) */

    &ypInt_Type,        /* ypInt_CODE                  ( 10u) */
    &ypIntStore_Type,   /* ypIntStore_CODE             ( 11u) */
    &ypFloat_Type,      /* ypFloat_CODE                ( 12u) */
    &ypFloatStore_Type, /* ypFloatStore_CODE           ( 13u) */

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

ypObject *yp_type( ypObject *object ) {
    return (ypObject *) ypObject_TYPE( object );
}

// The immortal type objects
ypObject * const yp_type_invalidated = (ypObject *) &ypInvalidated_Type;
ypObject * const yp_type_exception = (ypObject *) &ypException_Type;
ypObject * const yp_type_type = (ypObject *) &ypType_Type;
ypObject * const yp_type_NoneType = (ypObject *) &ypNoneType_Type;
ypObject * const yp_type_bool = (ypObject *) &ypBool_Type;
ypObject * const yp_type_int = (ypObject *) &ypInt_Type;
ypObject * const yp_type_intstore = (ypObject *) &ypIntStore_Type;
ypObject * const yp_type_float = (ypObject *) &ypFloat_Type;
ypObject * const yp_type_floatstore = (ypObject *) &ypFloatStore_Type;
ypObject * const yp_type_iter = (ypObject *) &ypIter_Type;
ypObject * const yp_type_bytes = (ypObject *) &ypBytes_Type;
ypObject * const yp_type_bytearray = (ypObject *) &ypByteArray_Type;
ypObject * const yp_type_str = (ypObject *) &ypStr_Type;
ypObject * const yp_type_chrarray = (ypObject *) &ypChrArray_Type;
ypObject * const yp_type_tuple = (ypObject *) &ypTuple_Type;
ypObject * const yp_type_list = (ypObject *) &ypList_Type;
ypObject * const yp_type_frozenset = (ypObject *) &ypFrozenSet_Type;
ypObject * const yp_type_set = (ypObject *) &ypSet_Type;
ypObject * const yp_type_frozendict = (ypObject *) &ypFrozenDict_Type;
ypObject * const yp_type_dict = (ypObject *) &ypDict_Type;


/*************************************************************************************************
 * Initialization
 *************************************************************************************************/

static const yp_initialize_kwparams _default_initialize = {
    sizeof( yp_initialize_kwparams ),
    _default_yp_malloc,
    _default_yp_malloc_resize,
    _default_yp_free,
    /*everything_immortal=*/FALSE
};

// Helpful macro, for use only by yp_initialize, to retrieve a parameter from kwparams.  Returns
// the default value if kwparams is too small to hold the parameter, or if the expression
// "kwparams->key default_cond" (ie "kwparams->yp_malloc ==NULL") evaluates to true.
#define _yp_INIT_PARAM_END( key ) \
    (offsetof( yp_initialize_kwparams, key ) + yp_sizeof_member( yp_initialize_kwparams, key ))
#define yp_INIT_PARAM2( key, default_cond ) \
    ( kwparams->struct_size < _yp_INIT_PARAM_END( key ) ? \
        _default_initialize.key \
      : kwparams->key default_cond ? \
        _default_initialize.key \
      : /*else*/ \
        kwparams->key \
    )
#define yp_INIT_PARAM1( key ) \
    ( kwparams->struct_size < _yp_INIT_PARAM_END( key ) ? \
        _default_initialize.key \
      : /*else*/ \
        kwparams->key \
    )

void yp_initialize( const yp_initialize_kwparams *kwparams )
{
    static int initialized = FALSE;

#ifdef _MSC_VER
    // TODO memory leak detection that should only be enabled for debug builds
    // _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // yp_initialize can only be called once
    if( initialized ) return;
    initialized = TRUE;

    if( kwparams == NULL ) kwparams = &_default_initialize;

    yp_malloc           = yp_INIT_PARAM2( yp_malloc,         ==NULL );
    yp_malloc_resize    = yp_INIT_PARAM2( yp_malloc_resize,  ==NULL );
    yp_free             = yp_INIT_PARAM2( yp_free,           ==NULL );

    if( yp_INIT_PARAM1( everything_immortal ) ) {
        // All objects will be created immortal
        _ypMem_starting_refcnt = ypObject_REFCNT_IMMORTAL;
    } else {
        _ypMem_starting_refcnt = 1;
    }

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


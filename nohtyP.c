/*
 * nohtyP.c - A Python-like API for C, in one .c and one .h
 *      Public domain?  PSF?  dunno
 *      http://nohtyp.wordpress.com/
 *      TODO Python's license
 *
 * TODO
 */

#include "nohtyP.h"


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

// TODO assert that sizeof( "abcd" ) == 5 (ie it includes the null-terminator)


/*************************************************************************************************
 * Internal structures and types, and related macros
 *************************************************************************************************/

// ypObject_HEAD defines the initial segment of every ypObject
#define ypObject_HEAD \
    ypObject ob_base;

// First byte of object structure is the type code; next 3 bytes is reference count.  The
// least-significant bit of the type code specifies if the type is immutable (0) or not.
// XXX Assuming little-endian for now
#define ypObject_MAKE_TYPE_REFCNT( type, refcnt ) \
    ( ((type) & 0xFFu) | (((refcnt) & 0xFFFFFFu) << 8) )
#define ypObject_TYPE_CODE( ob ) \
    ( ((yp_uint8_t *)(ob))[0] )
#define ypObject_SET_TYPE_CODE( ob, type ) \
    ( ((yp_uint8_t *)(ob))[0] = (type) )
#define ypObject_TYPE_CODE_IS_MUTABLE( type ) \
    ( (type) & 0x1u )
#define ypObject_TYPE_CODE_AS_FROZEN( type ) \
    ( (type) | 0x1u )
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


// A refcnt of this value means the object is immortal
// TODO Need two types of immortals: statically-allocated immortals (so should never be
// freed/invalidated) and overly-incref'd immortals (should be allowed to be invalidated and thus
// free any extra data, although the object itself will never be free'd as we've lost track of the
// refcounts)
#define ypObject_REFCNT_IMMORTAL (0xFFFFFFu)

// When a hash of this value is stored in ob_hash, call tp_hash (which may then update cache)
#define ypObject_HASH_INVALID ((yp_hash_t) -1)

// Signals an invalid length stored in ob_len (so call tp_len) or ob_alloclen
#define ypObject_LEN_INVALID        (0xFFFFu)
#define ypObject_ALLOCLEN_INVALID   (0xFFFFu)

// Creates (likely immutable) immortals 
// TODO What to set alloclen to?  Does it matter?
#define yp_IMMORTAL_HEAD_INIT( type, data, len ) \
    { ypObject_MAKE_TYPE_REFCNT( type, ypObject_REFCNT_IMMORTAL ), \
      ypObject_HASH_INVALID, len, 0, data },




typedef struct {
    binaryfunc sq_concat;
    ssizeargfunc sq_repeat;

    binaryfunc sq_extend;
    ssizeargfunc sq_irepeat;
    // TODO append et al
} ypSequenceMethods;






// TODO all of them
    ssizeargfunc sq_item;
    ssizeargfunc sq_set_item;
    objobjproc sq_contains;


typedef struct {
    ypObject_HEAD
    ypObject *tp_name; /* For printing, in format "<module>.<name>" */
    // TODO store type code here?

    // Object fundamentals
    destructor tp_dealloc;
    traverseproc tp_traverse; /* call function for all accessible objects */
    reprfunc tp_str;
    reprfunc tp_repr;

    // Freezing, copying, and invalidating
    inquiry tp_freeze;
    inquiry tp_unfrozen_copy;
    inquiry tp_frozen_copy;
    inquiry tp_invalidate; /* clear, then transmute self to ypInvalidated */

    // Boolean operations and comparisons
    inquiry tp_bool;
    objobjproc tp_lt;
    objobjproc tp_le;
    objobjproc tp_eq;
    objobjproc tp_ne;
    objobjproc tp_ge;
    objobjproc tp_gt;

    // Generic object operations
    hashfunc tp_currenthash;
    inquiry tp_close;

    // Number operations
    // TODO just leave ypNumberMethods as a dummy for now
    ypNumberMethods *tp_as_number;

    // Iterator operations
    inquiry tp_iter;
    inquiry tp_iter_reversed;
    objobjproc tp_send;

    // Container operations 
    objobjproc tp_contains;
    lenfunc tp_length;
    objobjproc tp_push;
    inquiry tp_clear; /* delete references to contained objects */
    inquiry tp_pop;
    objobjobjproc tp_getdefault; /* if defval is NULL, raise exception if missing */
    objobjobjproc tp_setitem;
    objobjproc tp_delitem;

    // Sequence operations
    // TODO in a method suite: ypSequenceMethods *tp_as_sequence;
    objintproc tp_repeat;
    objintproc tp_getindex;
    objsliceproc tp_getslice;
    findfunc tp_find;
    countfunc tp_count;
    objintobjproc tp_setindex;
    objsliceobjproc tp_setslice;
    objintproc yp_delindex;
    objsliceproc yp_delslice;
    objobjproc tp_extend;
    objintobjproc tp_insert;
    objintproc tp_popindex;
    objobjproc tp_remove;
    inquiry tp_reverse;
    sortfunc tp_sort;

    // Set operations
    // TODO in a method suite
    objobjproc yp_isdisjoint;
    objobjproc yp_issubset;
    // yp_lt is elsewhere
    objobjproc yp_issuperset;
    // yp_gt is elsewhere
    updatefunc yp_update;
    intersectionfunc yp_intersection_update;
    differencefunc yp_difference_update;
    symmdifferencefunc yp_symmetric_difference_update;

    // Mapping operations
    // TODO in a method suite: ypMappingMethods *tp_as_mapping;
    inquiry tp_iter_items;
    inquiry tp_iter_keys;
    objobjobjproc tp_popvalue;
    popitemfunc tp_popitem;
    objobjobjproc tp_setdefault;
    inquiry tp_iter_values;
} ypTypeObject;

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

// no immutable ypIter type         ( 14u)
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


/*************************************************************************************************
 * Helpful functions and macros
 *************************************************************************************************/

// Functions that modify their inputs take a "ypObject **x"; use this as
// "return_yp_INPLACE_ERR( x, yp_ValueError );" to return the error properly
#define return_yp_INPLACE_ERR( ob, err ) \
    do { yp_decref( *(ob) ); *(ob) = (err); return; } while( 0 )

// When an object encounters an unknown type, there are three possible cases:
//  - it's an invalidated object, so return yp_InvalidatedError
//  - it's an exception, so return it
//  - it's some other type, so return yp_TypeError
#define return_yp_BAD_TYPE( bad_ob ) \
    do { switch( yp_TYPE_PAIR_CODE( bad_ob ) ) { \
            case ypInvalidated_CODE: return yp_InvalidatedError; \
            case ypException_CODE: return (bad_ob); \
            default: return yp_TypeError; } } while( 0 )
#define return_yp_INPLACE_BAD_TYPE( ob, bad_ob ) \
    do { switch( yp_TYPE_PAIR_CODE( bad_ob ) ) { \
            case ypInvalidated_CODE: return_yp_INPLACE_ERR( (ob), yp_InvalidatedError ); \
            case ypException_CODE: return_yp_INPLACE_ERR( (ob), bad_ob ); \
            default: return_yp_INPLACE_ERR( (ob), yp_TypeError ); } } while( 0 )


// Return sizeof for a structure member
#define yp_sizeof_member( structType, member ) \
    sizeof( ((structType *)0)->member )

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
static void *(*yp_realloc)( void *, yp_ssize_t ) = _yp_dummy_relloc;
static void (*yp_free)( void * ) = _yp_dummy_free;

// Declares the ob_inline_data array for container object structures
#define yp_INLINE_DATA( elemType ) \
    elemType ob_inline_data[1]

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
        void *newData = yp_realloc( \
                (newAlloclen) * yp_sizeof_member( obStruct, ob_inline_data[0] ) ); \
        if( newData == NULL ) { (result) = yp_MemoryError; break; }; \
        (result) = yp_None; \
        (ob)->ob_data = newData; \
        (ob)->ob_alloclen = newAlloclen; \
    } while( 0 )


/* XXX Here's a crazy idea that is likely unfounded, premature optimization, but it's an idea that
 * might come in handy later if we're stuck with a restrictive version of malloc.
 *
 * When ints/floats become proper small objects, they'll be 16 bytes in size, which some malloc's
 * might over-allocate up to 64 bytes or more.  Just wasted space
 *
 * So, make a "small object container" object, that will never be seen externally, but that these
 * small objects can allocate memory from.  They would maintain a reference to the container
 * (bumping the size to 24 bytes...this might be the deal breaker).  On dealloc for the int/etc, it
 * decrefs its reference to the container.  The container would itself be a small object; it only
 * stores the refcount and frees itself when it hits zero.
 *
 * Internal to the container, memory is allocated stackwise, and only freed when all small objects
 * are dead.  Kinda like regions; kinda also like obstacks, except nothing is popped off the stack
 * until the whole stack is discarded.
 *
 * ...OR!  Don't do this as objects, but wrap around the busted malloc so that it does this itself.
 */

/*************************************************************************************************
 * Object fundamentals
 *************************************************************************************************/

ypObject *yp_incref( ypObject *x )
{
    yp_uint32_t refcnt = ypObject_REFCNT( x );
    if( refcnt == ypObject_REFCNT_IMMORTAL ) return x; // no-op
    x->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( ypObject_TYPE( x ), refcnt+1 );
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
    if( refcnt == ypObject_REFCNT_IMMORTAL ) return x; // no-op

    if( refcnt <= 1 ) {
        yp_TYPE( x )->tp_dealloc( x );
    } else {
        x->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( ypObject_TYPE( x ), refcnt-1 );
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
ypObject *_yp_freeze( ypObject *x )
{
    int oldCode = ypObject_TYPE_CODE( *x );
    int newCode = ypObject_TYPE_CODE_AS_FROZEN( oldCode );
    ypTypeObject *newType;
    ypObject *result;

    // Check if it's already frozen (no-op) or if it can't be frozen (error)
    if( oldCode == newCode ) return yp_None;
    newType = ypTypeTable[newCode];
    if( newType == NULL ) return yp_TypeError;  // TODO return a new, invalidated object

    // Freeze the object, cache the final hash, and possibly reduce memory usage, etc
    ypObject_SET_TYPE_CODE( *x, newCode );
    (*x)->ob_hash = newType->tp_current_hash( *x ); // TODO rename?
    return newType->tp_after_freeze( *x );
}

void yp_freeze( ypObject **x )
{
    ypObject *result = _yp_freeze( *x );
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( x, result );
}

ypObject *_yp_deepfreeze( ypObject *x, ypObject *memo )
{
    ypObject *id;
    ypObject *result;

    // Avoid recursion
    // TODO yp_addunique returns key error if object already in set
    id = yp_intC( (yp_int64_t) x );
    result = yp_addunique( memo, id );
    yp_decref( id );
    if( result == yp_KeyError ) return yp_None;
    if( yp_isexceptionC( result ) ) return result;

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

ypObject *yp_unfrozen_copy( ypObject *x );

ypObject *yp_unfrozen_deepcopy( ypObject *x );

ypObject *yp_frozen_copy( ypObject *x );

ypObject *yp_frozen_deepcopy( ypObject *x );

void yp_invalidate( ypObject **x );

void yp_deepinvalidate( ypObject **x );


/*************************************************************************************************
 * Boolean operations
 *************************************************************************************************/

// TODO


/*************************************************************************************************
 * Public object interface
 *************************************************************************************************/

// TODO should I be filling the type tables completely with function pointers that just return
// ypMethodError, to avoid having to write NULL checks?
// TODO do/while(0)

// args must be surrounded in brackets, to form the function call; as such, must also include ob
#define _yp_REDIRECT1( ob, tp_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( ob ); \
    if( type->tp_meth == NULL ) return ypMethodError; \
    return type->tp_meth args;

#define _yp_REDIRECT2( ob, tp_suite, suite_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( ob ); \
    if( type->tp_suite == NULL || type->tp_suite->suite_meth == NULL ) return ypMethodError; \
    return type->tp_suite->suite_meth args;

#define _yp_INPLACE1( pOb, tp_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( *pOb ); \
    ypObject *result; \
    if( type->tp_meth == NULL ) result = ypMethodError; \
    else result = type->tp_meth args; \
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( pOb, result ); \
    return;

#define _yp_INPLACE2( pOb, tp_suite, suite_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( *pOb ); \
    ypObject *result; \
    if( type->tp_suite == NULL || type->tp_suite->suite_meth == NULL ) result = ypMethodError; \
    else result = type->tp_suite->suite_meth args; \
    if( yp_isexceptionC( result ) ) return_yp_INPLACE_ERR( pOb, result ); \
    return;

void yp_append( ypObject **s, ypObject *x ) {
    _yp_INPLACE2( s, tp_as_sequence, sq_append, (*s, x) )
}





// TODO undef necessary stuff

/*************************************************************************************************
 * Invalidated Objects
 *************************************************************************************************/



/*************************************************************************************************
 * Exceptions
 *************************************************************************************************/




/*************************************************************************************************
 * Bools
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

// Returns 1 if the bool object is true, else 0; only valid on bool objects!  The return can also
// be interpreted as the value of the boolean.
// XXX This assumes that yp_True and yp_False are the only two bools
#define ypBool_IS_TRUE_C( b ) ( ((ypBoolObject *)b) == yp_True )

#define ypBool_FROM_C( cond ) ( (cond) ? yp_True : yp_False )


/*************************************************************************************************
 * Integers
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?


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
        if( isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypInt_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_intC( result );
        }
        return;

    } else if( x_type == ypFloat_CODE ) {
        yp_float_t y_asfloat = yp_asfloatL( y, &exc ); // TODO
        if( isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_iaddFC( x, y_asfloat );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( *x, x );
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

    return_yp_INPLACE_BAD_TYPE( *x, y );
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



/*************************************************************************************************
 * Floats
 *************************************************************************************************/

// TODO: A "ypSmallObject" type for type codes < 8, say, to avoid wasting space for bool/int/float?

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
        if( isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        if( ypObject_IS_MUTABLE( x ) ) {
            ypFloat_VALUE( x ) = result;
        } else {
            yp_decref( *x );
            *x = yp_floatC( result );
        }
        return;

    } else if( x_type == ypInt_CODE ) {
        yp_float_t x_asfloat = yp_asfloatC( *x, &exc );
        if( isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        result = yp_addFL( ypFloat_VALUE( *x ), y, &exc );
        if( isexceptionC( exc ) ) return_yp_INPLACE_ERR( x, exc );
        yp_decref( *x );
        *x = yp_floatC( result );
        return;
    }

    return_yp_INPLACE_BAD_TYPE( *x, x );
}


/*************************************************************************************************
 * Iterators
 *************************************************************************************************/

// TODO Iterators should have a lenhint "attribute" so that consumers of the iterator can
// pre-allocate; this should be automatically decremented with every yielded value


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

typedef struct {
    ypObject_HEAD
    yp_INLINE_DATA( yp_uint8_t );
} ypBytesObject;
yp_STATIC_ASSERT( offsetof( ypBytesObject, ob_inline_data ) % 8 == 0, bytes_inline_data_alignment );

#define ypBytes_DATA( b ) ( (yp_uint8_t *) ((ypBytesObject *)b)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypBytes_LEN( b )  ( ((ypObject *)b)->ob_len )

// Return a new bytes object with uninitialized data of the given length, or an exception
static ypObject *_yp_bytes_new( yp_ssize_t len )
{
    ypObject *b;
    if( len < 0 ) len = 0; // TODO return a new ref to an immortal b'' object
    ypMem_MALLOC_CONTAINER_INLINE( b, ypBytesObject, ypBytes_CODE, len );
    if( isexceptionC( b ) ) return b;
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
    if( isexceptionC( b ) ) return b;
    ypBytes_LEN( b ) = len;
    return b;
}

// Return a new bytes or bytearray object (depending on b) with uninitialzed data of the given
// length, or an exception
static ypObject *_yp_bytes_new_sametype( ypObject *b, yp_ssize_t len )
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
static ypObject *_yp_bytes_copy( ypObject *b, yp_ssize_t len )
{
    ypObject *newB = _yp_bytes_new_sametype( b, len );
    if( isexceptionC( newB ) ) return newB;
    memcpy( ypBytes_DATA( newB ), ypBytes_DATA( b ), MIN( len, ypBytes_LEN( b ) ) );
    return newB;
}

// Shrinks or grows the bytearray; any new bytes are uninitialized.  Returns yp_None on success,
// exception on error.
static ypObject *_yp_bytearray_resize( ypObject *b, yp_ssize_t newLen )
{
    ypObject *result;
    ypMem_REALLOC_CONTAINER_VARIABLE( result, *b, ypBytesObject, newLen );
    if( yp_isexceptionC( result ) ) return result;
    ypBytes_LEN( b ) = len;
    return yp_None;
}

// If x is a fellow bytes, set *x_data and *x_len.  Otherwise, set *x_data=NULL and *x_len=0.
// TODO note http://bugs.python.org/issue12170 and ensure we stay consistent
static void _bytes_coerce_bytes( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len )
{
    ypObject *result;
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
static void _bytes_coerce_intorbytes( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len,
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

// Returns yp_None or an exception
static ypObject *bytes_findC4( ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *i )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    yp_uint8_t storage;
    ypObject *result;
    yp_ssize_t b_rlen;     // remaining length
    yp_uint8_t *b_rdata;   // remaining data

    _bytes_coerce_intorbytes( x, &x_data, &x_len, &storage );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, 1, &b_rlen );
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

    result = bytes_findC4( b, x, 0, yp_SLICE_USELEN, &i );
    if( yp_isexceptionC( result ) ) return result;
    return ypBool_FROM_C( i >= 0 );
}

// Returns new reference or an exception
static ypObject *bytes_concat( ypObject *b, ypObject *x )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    ypObject *newB;

    _bytes_coerce_bytes( x, &x_data, &x_len );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    newB = _yp_bytes_copy( b, ypBytes_LEN( b ) + x_len );
    if( isexceptionC( newB ) ) return newB;

    memcpy( ypBytes_DATA( newB )+ypBytes_LEN( b ), x_data, x_len );
    return newB;
}

// Returns yp_None or an exception
static ypObject *bytearray_extend( ypObject *b, ypObject *x )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    yp_uint8_t storage;
    ypObject *result;

    _bytes_coerce( x, &x_data, &x_len, &storage );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    result = _yp_bytearray_resize( b, ypBytes_LEN( b ) + x_len );
    if( yp_isexceptionC( result ) ) return result;

    memcpy( ypBytes_DATA( b )+ypBytes_LEN( b ), x_data, x_len );
    return yp_None;
}

// TODO bytes_repeat
// TODO bytes_irepeat

// Returns new reference or an exception
static ypObject *bytes_getindexC( ypObject *b, yp_ssize_t i )
{
    ypObject *result = ypSequence_AdjustIndexC( ypBytes_LEN( b ), &i );
    if( yp_isexceptionC( result ) ) return result;
    return yp_intC( ypBytes_DATA( b )[i] );
}

// Returns yp_None or an exception
static ypObject *bytearray_setindexC( ypObject *b, yp_ssize_t i, ypObject *x )
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
static ypObject *bytearray_delindexC( ypObject *b, yp_ssize_t i )
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
static ypObject *bytes_getsliceC( ypObject *b, yp_ssize_t start, yp_ssize_t stop, yp_ssize_t step )
{
    ypObject *result;
    yp_ssize_t newLen;
    ypObject *newB;

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &newLen );
    if( yp_isexceptionC( result ) ) return result;

    newB = _yp_bytes_new_sametype( b, newLen );
    if( isexceptionC( newB ) ) return newB;

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
static ypObject *bytearray_setsliceC( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t step, ypObject *x )
{
    ypObject *result;
    yp_ssize_t slicelength;
    yp_uint8_t *x_data;
    yp_ssize_t x_len;

    _bytes_coerce_bytes( x, &x_data, &x_len );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );
    if( x_len == 0 ) return bytearray_delsliceC( b, start, stop, step );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, &step, &slicelength );
    if( yp_isexceptionC( result ) ) return result;

    if( step == 1 ) {
        if( x_len > slicelength ) {
            // bytearray is growing
            yp_ssize_t growBy = x_len - slicelength;
            yp_ssize_t oldLen = ypBytes_LEN( b );
            result = _yp_bytearray_resize( b, oldLen + growBy );
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
static ypObject *bytearray_delsliceC( ypObject *b, yp_ssize_t start, yp_ssize_t stop,
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
static ypObject *bytes_lenC( ypObject *b, yp_ssize_t *len )
{
    *len = ypBytes_LEN( b );
    return yp_None;
}

// TODO allow custom min/max methods?

static ypObject *bytes_count3C( ypObject *b, ypObject *x, yp_ssize_t start, yp_ssize_t stop,
        yp_ssize_t *n )
{
    yp_uint8_t *x_data;
    yp_ssize_t x_len;
    yp_uint8_t storage;
    ypObject *result;
    yp_ssize_t b_rlen;     // remaining length
    yp_uint8_t *b_rdata;   // remaining data

    _bytes_coerce_intorbytes( x, &x_data, &x_len, &storage );
    if( x_data == NULL ) return_yp_BAD_TYPE( x );

    result = ypSlice_AdjustIndicesC( ypBytes_LEN( b ), &start, &stop, 1, &b_rlen );
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
#define _ypBytes_RELATIVE_CMP_BODY( name, operator ) \
static ypObject *bytes_ ## name( ypObject *b, ypObject *x ) { \
    int cmp; \
    yp_ssize_t b_len, x_len; \
    if( yp_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return_yp_BAD_TYPE( x ); \
    b_len = ypBytes_LEN( b ); x_len = ypBytes_LEN( x ); \
    cmp = memcmp( ypBytes_DATA( b ), ypBytes_DATA( x ), MIN( b_len, x_len ) ); \
    if( cmp == 0 ) cmp = b_len < x_len ? -1 : (b_len > x_len ? 1 : 0); \
    return ypBool_FROM_C( cmp operator 0 ); \
}
_ypBytes_RELATIVE_CMP_BODY( lt, < );
_ypBytes_RELATIVE_CMP_BODY( le, <= );
_ypBytes_RELATIVE_CMP_BODY( ge, >= );
_ypBytes_RELATIVE_CMP_BODY( gt, > );
#undef _ypBytes_RELATIVE_CMP_BODY
#define _ypBytes_EQUALITY_CMP_BODY( name, operator ) \
static ypObject *bytes_ ## name( ypObject *b, ypObject *x ) { \
    int cmp; \
    yp_ssize_t b_len, x_len; \
    if( yp_TYPE_PAIR_CODE( x ) != ypBytes_CODE ) return_yp_BAD_TYPE( x ); \
    b_len = ypBytes_LEN( b ); x_len = ypBytes_LEN( x ); \
    if( b_len != x_len ) return ypBool_FROM_C( 1 operator 0 ); \
    cmp = memcmp( ypBytes_DATA( b ), ypBytes_DATA( x ), MIN( b_len, x_len ) ); \
    return ypBool_FROM_C( cmp operator 0 ); \
}
_ypBytes_EQUALITY_CMP_BODY( eq, == );
_ypBytes_EQUALITY_CMP_BODY( ne, != );
#undef _ypBytes_EQUALITY_CMP_BODY

// Must work even for mutables; yp_hash handles caching this value and denying its use for mutables
static yp_hash_t bytes_current_hash( ypObject *b ) {
    return yp_HashBytes( ypBytes_DATA( b ), ypBytes_LEN( b ) );
}

// TODO undef macros


/*************************************************************************************************
 * Sequence of unicode characters
 *************************************************************************************************/

// TODO http://www.python.org/dev/peps/pep-0393/ (flexible string representations)


/*************************************************************************************************
 * Sequence of generic items
 *************************************************************************************************/

// TODO Eventually, use timsort, but for now C's qsort should be fine


/*************************************************************************************************
 * Sets
 *************************************************************************************************/
// TODO Instead, and following http://www.python.org/dev/peps/pep-0412/, dict should treat set as a
// separate object, with "friendly" (but yp-internal) methods to handle resizes, etc.  This allows
// the set to be returned for yp_keys; it allows multiple dicts to share the same key dict, thus
// reducing memory; because Python essentially does this, we'll have tests written to ensure 
// correctness; it further-simplifies the implementation of dict.
// http://nohtyp.wordpress.com/2012/09/19/dicts-as-sets/

// XXX Much of this set/dict implementation is pulled right from Python, so best to read the
// original source for documentation on this implementation
// TODO make sure I'm using FrozenSet in the right places

typedef struct {
    yp_hash_t se_hash;
    ypObject *se_key;
} ypFrozenSet_KeyEntry;
typedef struct {
    ypObject_HEAD
    yp_ssize_t so_fill; // # Active + # Dummy
    yp_INLINE_DATA( ypFrozenSet_KeyEntry );
} yFrozenSetObject;

#define ypFrozenSet_TABLE( so ) ( (ypFrozenSet_KeyEntry *) ((ypObject *)so)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypFrozenSet_LEN( so )  ( ((ypObject *)so)->ob_len )
#define ypFrozenSet_FILL( so )  ( ((yFrozenSetObject *)so)->so_fill )
#define ypFrozenSet_ALLOCLEN( so )  ( ((ypObject *)so)->ob_alloclen )
#define ypFrozenSet_MASK( so ) ( ypFrozenSet_ALLOCLEN( so ) - 1 )

#define ypFrozenSet_PERTURB_SHIFT (5)
static ypObject _ypFrozenSet_dummy = yp_IMMORTAL_HEAD_INIT( ypInvalidated_CODE, NULL, 0 );
static ypObject *ypFrozenSet_dummy = &_ypFrozenSet_dummy;

// Returns true if the given ypFrozenSet_KeyEntry contains a valid key
#define ypFrozenSet_ENTRY_USED( loc ) \
    ( (loc)->se_key != NULL && (loc)->se_key != ypFrozenSet_dummy )
// Returns the index of the given ypFrozenSet_KeyEntry in the hash table
#define ypFrozenSet_ENTRY_INDEX( so, loc ) \
    ( (yp_ssize_t) ( (loc) - ypFrozenSet_TABLE( so ) ) )



static ypObject *_ypFrozenSet_new( yp_ssize_t minused )
{
    // TODO
}



// Sets *loc to where the key should go in the table; it may already be there, in fact!  Returns
// yp_None, or an exception on error.
// TODO The dict implementation has a bunch of these for various scenarios; let's keep it simple
// for now, but investigate...
// XXX Adapted from Python's lookdict in dictobject.c
static ypObject *_ypFrozenSet_lookkey( ypObject *so, ypObject *key, register yp_hash_t hash, 
        ypFrozenSet_KeyEntry **loc )
{
    register size_t i;
    register size_t perturb;
    register ypFrozenSet_KeyEntry *freeslot;
    register size_t mask = (size_t) ypFrozenSet_MASK( so );
    ypFrozenSet_KeyEntry *table = ypFrozenSet_TABLE( so );
    ypFrozenSet_KeyEntry *ep0 = ypFrozenSet_TABLE( so );
    register ypFrozenSet_KeyEntry *ep;
    register ypObject *cmp;

    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->se_key == NULL || ep->se_key == key) goto success;

    if (ep->se_key == ypFrozenSet_dummy) {
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

    // In the loop, se_key == ypFrozenSet_dummy is by far (factor of 100s) the least likely 
    // outcome, so test for that last
    for (perturb = hash; ; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->se_key == NULL) {
            if( freeslot != NULL ) ep = freeslot;
            goto success;
        }
        if (ep->se_key == key) goto success;
        if (ep->se_hash == hash && ep->se_key != ypFrozenSet_dummy) {
            // Same __eq__ protection is here as well in Python
            cmp = yp_eq( ep->se_key, key );
            if( yp_isexceptionC( cmp ) ) return cmp;
            if( cmp == yp_True ) goto success;
        } else if (ep->se_key == ypFrozenSet_dummy && freeslot == NULL) {
            freeslot = ep;
        }
    }
    return yp_SystemError; // NOT REACHED

// When the code jumps here, it means ep points to the proper entry
success:
    *loc = ep;
    return yp_None;
}

// Adds a new key to the hash table at the given location; updates the fill and len counts.  loc 
// must not currently be in use!
// XXX Adapted from Python's insertdict in dictobject.c
static ypObject *_ypSet_addkey( ypObject *so, ypFrozenSet_KeyEntry *loc, ypObject *key,
        yp_hash_t hash )
{
    if( loc->se_key == NULL ) ypFrozenSet_FILL( so ) += 1;
    loc->se_key = yp_incref( key );
    loc->se_hash = hash;
    ypFrozenSet_LEN( so ) += 1;
}

// Internal routine used while cleaning/resizing/copying a table: the key is known to be absent
// from the table, the table contains no deleted entries, and there's no need to incref the key.
// Sets *loc to the location at which the key was inserted.
// XXX Adapted from Python's insertdict_clean in dictobject.c
static void _ypSet_resize_insertkey( ypObject *so, ypObject *key, yp_hash_t hash,
        ypFrozenSet_KeyEntry **loc )
{
    size_t i;
    size_t perturb;
    size_t mask = (size_t) ypFrozenSet_MASK( so );
    ypFrozenSet_KeyEntry *ep0 = ypFrozenSet_TABLE( so );

    i = hash & mask;
    (*loc) = &ep0[i];
    for (perturb = hash; (*loc)->se_key != NULL; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        (*loc) = &ep0[i & mask];
    }
    ypFrozenSet_FILL( so ) += 1;
    (*loc)->se_key = key;
    (*loc)->se_hash = hash;
    ypFrozenSet_LEN( so ) += 1;
}

// Before adding numnew keys to the set, call this function to determine if a resize is necessary.
// Returns -1 if the set doesn't require a resize, else the new minused value to pass to
// _ypFrozenSet_resize.  If adding one key, it's recommended to first check if the key already
// exists in the set before checking if it should be resized; if adding multiple, just assume that
// none of the keys exist in the set currently.
// TODO ensure we aren't unnecessarily resizing: if the old and new alloclens will be the same,
// and we don't have dummy entries, then resizing is a waste of effort.
// XXX Adapted from PyDict_SetItem
static yp_ssize_t _ypSet_shouldresize( ypObject *so, yp_ssize_t numnew )
{
    yp_ssize_t newfill = ypFrozenSet_FILL( so ) + numnew;

    /* If fill >= 2/3 size, adjust size.  Normally, this doubles or
     * quaduples the size, but it's also possible for the dict to shrink
     * (if ma_fill is much larger than se_used, meaning a lot of dict
     * keys have been deleted).
     *
     * Quadrupling the size improves average dictionary sparseness
     * (reducing collisions) at the cost of some memory and iteration
     * speed (which loops over every possible entry).  It also halves
     * the number of expensive resize operations in a growing dictionary.
     *
     * Very large dictionaries (over 50K items) use doubling instead.
     * This may help applications with severe memory constraints.
     */
    // TODO make this limit configurable
    if( newfill*3 >= ypFrozenSet_ALLOCLEN( so )*2 ) {
        yp_ssize_t newlen = ypFrozenSet_LEN( so ) + numnew;
        return (newlen > 50000 ? 2 : 4) * newlen;
    } else {
        return -1;
    }
}



/*************************************************************************************************
 * Mappings
 *************************************************************************************************/

// XXX keyset requires care!  It is potentially shared among multiple dicts, so we cannot remove
// keys or resize it.  It identifies itself as a frozendict, yet we add keys to it, so it is not
// truly immutable.  As such, it cannot be exposed outside of the set/dict implementations.
// TODO alloclen will always be the same as keyset.alloclen; repurpose?
typedef struct {
    ypObject_HEAD
    ypObject *keyset;
    yp_INLINE_DATA( ypObject * );
} ypFrozenDictObject;

#define ypFrozenDict_VALUES( mp ) ( (ypObject *) ((ypObject *)mp)->ob_data )
#define ypFrozenDict_KEYSET( mp ) ( ((ypFrozenDict *)mp)->keyset )

// Returns the index of the given ypFrozenSet_KeyEntry in the hash table
#define ypFrozenDict_ENTRY_INDEX( mp, loc ) \
    ( ypFrozenSet_ENTRY_INDEX( ypFrozenDict_KEYSET( mp ), loc ) )

// The tricky bit about resizing dicts is that we need both the old and new keysets and value 
// arrays to properly transfer the data, so ypMem_REALLOC_CONTAINER_VARIABLE is no help.
static ypObject *_ypDict_resize( ypObject *mp, yp_ssize_t minused )
{
    ypObject *newkeyset;
    yp_ssize_t newalloclen;
    ypObject **newvalues;
    ypFrozenSet_KeyEntry *oldkeys;
    ypObject *oldvalues;
    yp_ssize_t valuesleft;
    yp_ssize_t i;
    ypObject *value;
    ypFrozenSet_KeyEntry **loc;

    // TODO allocate the value array in-line, then handle the case where both old and new value
    // arrays could fit in-line (idea: if currently in-line, then just force that the new array be
    // malloc'd...will need to malloc something anyway)
    newkeyset = _ypFrozenSet_new( minused );
    if( isexceptionC( newkeyset ) ) return newkeyset;
    newalloclen = ypFrozenSet_ALLOCLEN( newkeyset );
    newvalues = yp_malloc( newalloclen * sizeof( ypObject * ) );
    if( newvalues == NULL ) {
        yp_decref( newkeyset );
        return yp_MemoryError;
    }
    memset( newvalues, 0, newalloclen * sizeof( ypObject * ) );

    oldkeys = ypFrozenSet_TABLE( ypFrozenDict_KEYSET( mp ) );
    oldvalues = ypFrozenDict_VALUES( mp );
    valuesleft = ypFrozenDict_LEN( mp );
    for( i = 0; valuesleft > 0; i++ ) {
        value = ypFrozenDict_VALUES( mp )[i];
        if( value == NULL ) continue;
        _ypSet_resize_insertkey( newkeyset, yp_incref( oldkeytable[i].se_key ), 
                oldkeytable[i].se_hash, &loc );
        newvalues[ypFrozenSet_ENTRY_INDEX( newkeyset, loc )] = oldvalues[i];
        valuesleft -= 1;
    }

    yp_decref( ypFrozenDict_KEYSET( mp ) );
    ypFrozenDict_KEYSET( mp ) = newkeyset;
    yp_free( oldvalues );
    ypFrozenDict_VALUES( mp ) = newvalues;
    return yp_None;
}

// Adds a new value to the value array corresponding to the given hash table location; updates the
// len count.  Replaces any existing value.
static void _ypDict_setvalue( ypObject *mp, ypFrozenSet_KeyEntry *loc, ypObject *value )
{
    yp_ssize_t i = ypFrozenDict_ENTRY_INDEX( mp, loc );
    ypObject *oldvalue = ypFrozenDict_VALUES( mp )[i];
    if( oldvalue == NULL ) {
        ypFrozenDict_LEN( mp ) += 1;
    } else {
        yp_decref( oldvalue );
    }
    ypFrozenDict_VALUES( mp )[i] = yp_incref( value );
}


// yp_None or an exception
// TODO The decision to resize currently depends only on _ypSet_shouldresize, but what if the
// shared keyset contains 5x the keys that we actually use?  That's a large waste in the value
// table.  Really, we should have a _ypDict_shouldresize.
static ypObject *dict_setitem( ypObject *mp, ypObject *key, ypObject *value )
{
    yp_hash_t hash;
    ypObject *keyset = ypFrozenDict_KEYSET( mp );
    ypFrozenSet_KeyEntry *loc;
    ypObject *result = yp_None;

    // Look for the appropriate entry in the hash table
    // TODO yp_isexceptionC used internally should be a macro
    hash = yp_hashC( key, &result );
    if( yp_isexceptionC( result ) ) return result;
    result = _ypFrozenSet_lookkey( keyset, key, hash, &loc );
    if( yp_isexceptionC( result ) ) return result;

    // If the key is already in the hash table, then we simply need to update the value
    if( ypFrozenSet_ENTRY_USED( loc ) ) {
        _ypDict_setvalue( mp, loc, value );
        return yp_None;
    }

    // Otherwise, we need to add the key, which possibly doesn't involve resizing
    newminused = _ypSet_shouldresize( keyset, 1 );
    if( newminused < 0 ) {
        _ypSet_addkey( keyset, loc, key, hash );
        _ypDict_setvalue( mp, loc, value );
        return yp_None;
    }

    // Otherwise, we need to resize the table to add the key; on the bright side, we can use the
    // fast _ypSet_resize_insertkey.
    result = _ypDict_resize( mp, newminused );   // invalidates keyset and loc
    if( yp_isexceptionC( result ) ) return result;
    keyset = ypFrozenDict_KEYSET( mp );
    _ypSet_resize_insertkey( keyset, yp_incref( key ), hash, &loc );
    _ypDict_setvalue( mp, loc, value );
}



/*************************************************************************************************
 * The type table
 *************************************************************************************************/
// XXX Make sure this corresponds with ypInvalidated_TYPE et al!




/*************************************************************************************************
 * Initialization
 *************************************************************************************************/

// Currently a no-op
void yp_initialize( void )
{
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


/*
 * nohtyP.c - A Python-like API for C, in one .c and one .h
 *      Public domain?  PSF?  dunno
 *      http://nohtyp.wordpress.com/
 *      TODO Python's license
 *
 * TODO
 */

#include "nohtyP.h"

// TODO yp_int32_t, yp_uint32_t

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
#define ypObject_REFCNT_IMMORTAL (0xFFFFFFu)

// When a hash of this value is stored in ob_hash, call tp_hash (which may then update cache)
#define ypObject_HASH_NOT_CACHED (0xFFFFFFFFu)

// Signals an invalid length stored in ob_len (so call tp_len) or ob_alloclen
#define ypObject_LEN_INVALID        (0xFFFFu)
#define ypObject_ALLOCLEN_INVALID   (0xFFFFu)

// "CONSTants" are immutable immortals
#define ypObject_HEAD_INIT_CONST( type, len ) \
    { ypObject_MAKE_TYPE_REFCNT( type, ypObject_REFCNT_IMMORTAL ), \
      ypObject_HASH_NOT_CACHED, len, NULL },



typedef struct {
    binaryfunc nb_add;
    binaryfunc nb_subtract;
    binaryfunc nb_multiply;
    binaryfunc nb_remainder;
    binaryfunc nb_divmod;
    ternaryfunc nb_power;
    unaryfunc nb_negative;
    unaryfunc nb_positive;
    unaryfunc nb_absolute;
    unaryfunc nb_invert;
    binaryfunc nb_lshift;
    binaryfunc nb_rshift;
    binaryfunc nb_and;
    binaryfunc nb_xor;
    binaryfunc nb_or;
    binaryfunc nb_floor_divide;
    binaryfunc nb_true_divide;

    unaryfunc nb_int;
    unaryfunc nb_float;
    unaryfunc nb_index;
} ypNumberMethods;

typedef struct {
    binaryfunc nb_iadd;
    binaryfunc nb_isubtract;
    binaryfunc nb_imultiply;
    binaryfunc nb_iremainder;
    // TODO? binaryfunc nb_idivmod;
    ternaryfunc nb_ipower;
    unaryfunc nb_inegative;
    unaryfunc nb_ipositive;
    unaryfunc nb_iabsolute;
    unaryfunc nb_iinvert;
    binaryfunc nb_ilshift;
    binaryfunc nb_irshift;
    binaryfunc nb_iand;
    binaryfunc nb_ixor;
    binaryfunc nb_ior;
    binaryfunc nb_ifloor_divide;
    binaryfunc nb_itrue_divide;
} ypMutableNumberMethods;


typedef struct {
    binaryfunc sq_concat;
    ssizeargfunc sq_repeat;
} ypSequenceMethods;

typedef struct {
    binaryfunc sq_iconcat;
    ssizeargfunc sq_irepeat;
    // TODO iappend et al
} ypMutableSequenceMethods;






// TODO all of them
    inquiry nb_bool;
    ssizeargfunc sq_item;
    ssizeargfunc sq_set_item;
    objobjproc sq_contains;


typedef struct {
    ypObject_HEAD
    ypObject *tp_name; /* For printing, in format "<module>.<name>" */
    yp_ssize_t tp_basicsize, tp_itemsize; /* For allocation */ // TODO remove?

    // TODO store type code here?

    destructor tp_dealloc;
    hashfunc tp_hash;
    lenfunc tp_length;

    inquiry tp_clear; /* delete references to contained objects */
    inquiry tp_invalidate; /* clear, then transmute self to ypInvalidated */
    traverseproc tp_traverse; /* call function for all accessible objects */




    printfunc tp_print;

    reprfunc tp_str;
    reprfunc tp_repr;

    /* Method suites for standard classes */
    ypNumberMethods *tp_as_number;
    ypMutableNumberMethods *tp_as_mutable_number;
    ypSequenceMethods *tp_as_sequence;
    ypMutableSequenceMethods *tp_as_mutable_sequence;
    ypMappingMethods *tp_as_mapping;
    ypMutableMappingMethods *tp_as_mutable_mapping;

    ypObject *tp_doc; /* Documentation string */

    /* rich comparisons */
    ypCompareMethods *tp_comparisons;

    /* Iterators */
    getiterfunc tp_iter;
    iternextfunc tp_iternext;

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
// TODO what to call a mutalbe bool (  9u)

#define ypInt_CODE                  ( 10u)
// TODO what to call a mutable int  ( 11u)
#define ypFloat_CODE                ( 12u)
// TODO what to call a mutable float( 13u)

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
 * Helpful functions and macros for the type code
 *************************************************************************************************/

// Declares the ob_inline_data array for container object structures
#define yp_INLINE_DATA( elemType ) \
    elemType ob_inline_data[1]

// Return sizeof for a structure member
#define _yp_sizeof_member( structType, member ) \
    sizeof( ((structType *)0)->member )

// Sets ptr to a malloc'd buffer for fixed, non-container objects, or NULL on failure
#define ypMem_MALLOC_FIXED( ptr, obStruct, type ) \
    do { \
        (ptr) = (ypObject *) malloc( sizeof( obStruct ) ); \
        if( ptr == NULL ) break; \
        (ptr)->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 ); \
        (ptr)->ob_hash = ypObject_HASH_NOT_CACHED; \
        (ptr)->ob_len = ypObject_LEN_INVALID; \
        (ptr)->ob_alloclen = ypObject_ALLOCLEN_INVALID; \
        (ptr)->ob_data = NULL; \
    } while( 0 )

// Sets ptr to a malloc'd buffer for an immutable container object holding alloclen elements, or
// NULL on failure; ob_inline_data in obStruct is used to determine the element size and ob_data;
// ob_len is set to zero
#define ypMem_MALLOC_CONTAINER_INLINE( ptr, obStruct, type, alloclen ) \
    do { \
        (ptr) = (ypObject *) malloc( sizeof( obStruct )/*includes one element*/ + \
                ((alloclen-1) * _yp_sizeof_member( obStruct, ob_inline_data[0] )) ); \
        if( ptr == NULL ) break; \
        (ptr)->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 ); \
        (ptr)->ob_hash = ypObject_HASH_NOT_CACHED; \
        (ptr)->ob_len = 0; \
        (ptr)->ob_alloclen = alloclen; \
        (ptr)->ob_data = ((obStruct *)(ptr))->ob_inline_data; \
    } while( 0 )

// Sets ptr to a malloc'd buffer for a mutable container currently holding alloclen elements, or
// NULL on failure; ob_inline_data in obStruct is used to determine the element size; ob_len is set
// to zero
#define ypMem_MALLOC_CONTAINER_VARIABLE( ptr, obStruct, type, alloclen ) \
    do { \
        (ptr) = (ypObject *) malloc( sizeof( obStruct )/*includes one element*/ - \
                _yp_sizeof_member( obStruct, ob_inline_data[0] ) ); \
        if( ptr == NULL ) break; \
        (ptr)->ob_data = malloc( (alloclen) * _yp_sizeof_member( obStruct, ob_inline_data[0] ) ); \
        if( (ptr)->ob_data == NULL ) { free( ptr ); break; } \
        (ptr)->ob_type_refcnt = ypObject_MAKE_TYPE_REFCNT( type, 1 ); \
        (ptr)->ob_hash = ypObject_HASH_NOT_CACHED; \
        (ptr)->ob_len = 0; \
        (ptr)->ob_alloclen = alloclen; \
    } while( 0 )

#undef _yp_sizeof_member

// Functions that modify their inputs take a "ypObject **x"; use this as 
// "return_yp_INPLACE_ERR( *x, yp_TypeError );" to return the error properly
#define return_yp_INPLACE_ERR( ob, err ) \
    do { yp_decref( ob ); (ob) = (err); return; } while( 0 )




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


ypObject *_yp_freeze( ypObject *x )
{
    int oldCode = ypObject_TYPE_CODE( *x );
    int newCode = ypObject_TYPE_CODE_AS_FROZEN( oldCode );
    ypTypeObject *newType;
    ypObject *result;

    // Check if it's already frozen (no-op) or if it can't be frozen (error)
    if( oldCode == newCode ) return NULL;
    newType = ypTypeTable[newCode];
    if( newType == NULL ) return yp_TypeError;

    // Freeze the object, ensure hash will be recalculated, and possibly reduce memory usage, etc
    ypObject_SET_TYPE_CODE( *x, newCode );
    (*x)->ob_hash = ypObject_HASH_NOT_CACHED;
    return newType->tp_after_freeze( *x );
}

void yp_freeze( ypObject **x )
{
    ypObject *result = _yp_freeze( *x );
    if( result != NULL ) return_yp_INPLACE_ERR( *x, result );
}

ypObject *_yp_deepfreeze( ypObject *x, ypObject *memo )
{
    ypObject *id;
    ypObject *result;
    
    // Avoid recursion
    // TODO yp_add_unique returns key error if object already in set
    id = yp_intC( (yp_int64_t) x );
    result = yp_add_unique( memo, id );
    yp_decref( id );
    if( result == yp_KeyError ) return NULL; // success
    if( yp_isexceptionC( result ) ) return result;

    // Freeze current object before going deep
    result = _yp_freeze( x );
    if( result != NULL ) return result;
    // TODO tp_traverse should return if its visitor returns non-NULL...?  Or should we track
    // additional data in what is currently memo?
    return ypObject_TYPE( x )->tp_traverse( x, _yp_deepfreeze, memo );
}

void yp_deepfreeze( ypObject **x )
{
    ypObject *memo = yp_setN( 0 );
    ypObject *result = _yp_deepfreeze( *x, memo );
    yp_decref( memo );
    if( result != NULL ) return_yp_INPLACE_ERR( *x, result );
}

// Returns a new reference to a mutable shallow copy of x, or yp_MemoryError.
ypObject *yp_unfrozen_copy( ypObject *x );

// Returns a new reference to a mutable deep copy of x, or yp_MemoryError.
ypObject *yp_unfrozen_deepcopy( ypObject *x );

ypObject *yp_frozen_copy( ypObject *x );

ypObject *yp_frozen_deepcopy( ypObject *x );

// Steals x, discards all referenced objects, deallocates _some_ memory, transmutes it to
// the ypInvalidated type (rendering the object useless), and returns a new reference to x.
// If x is immortal or already invalidated this is a no-op.
void yp_invalidate( ypObject **x );

// Steals and invalidates x and, recursively, all referenced objects, returning a new reference.
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

// args must be surrounded in brackets, to form the function call; as such, must also include ob
#define _yp_REDIRECT1( ob, tp_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( ob ); \
    if( type->tp_meth == NULL ) return ypMethodError; \
    return type->tp_meth args;

#define _yp_REDIRECT2( ob, tp_suite, suite_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( ob ); \
    if( type->tp_suite == NULL || type->tp_suite->suite_meth == NULL ) return ypMethodError; \
    return type->tp_suite->suite_meth args;

// TODO In-place methods need a way to signal that the given object should not be replaced; could
// either return the object (without having incref'd, although any other reference would need to
// be, which is inconsistent!) or can return NULL...let's go with: "NULL, or a new reference"
#define _yp_INPLACE1( pOb, tp_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( *pOb ); \
    ypObject *result; \
    if( type->tp_meth == NULL ) result = ypMethodError; \
    else result = type->tp_meth args; \
    if( result != NULL ) return_yp_INPLACE_ERR( *pOb, result ); \
    return;

#define _yp_INPLACE2( pOb, tp_suite, suite_meth, args ) \
    ypTypeObject *type = ypObject_TYPE( *pOb ); \
    ypObject *result; \
    if( type->tp_suite == NULL || type->tp_suite->suite_meth == NULL ) result = ypMethodError; \
    else result = type->tp_suite->suite_meth args; \
    if( result != NULL ) return_yp_INPLACE_ERR( *pOb, result ); \
    return;

void yp_append( ypObject **s, ypObject *x ) {
    _yp_INPLACE2( s, tp_as_mutable_sequence, sq_iappend, (*s, x) )
}




// TODO undef necessary stuff


/*************************************************************************************************
 * Bools
 *************************************************************************************************/

// Returns 1 if the bool object is true, else 0; only valid on bool objects!  The return can also
// be interpreted as the value of the boolean.
// XXX This assumes that yp_True and yp_False are the only two bools
#define ypBool_IS_TRUE_C( b ) ( ((ypBoolObject *)b) == yp_True )


/*************************************************************************************************
 * Integers
 *************************************************************************************************/

static ypObject *yp_int_iadd( ypObject *x, ypObject *y ) 
{
    x->value += y->value; // TODO overflow check, etc
}

static ypObject *yp_int_add( ypObject *x, ypObject *y ) 
{
    // TODO check type first; no sense making the same copy
    ypObject *new = yp_unfrozen_copy( x );
    return _yp_int_add( new, y );
}


/*************************************************************************************************
 * Floats
 *************************************************************************************************/


/*************************************************************************************************
 * Iterators
 *************************************************************************************************/


/*************************************************************************************************
 * Sequence of bytes
 *************************************************************************************************/

typedef struct {
    ypObject_HEAD
    yp_INLINE_DATA( yp_uint8_t );
} ypBytesObject;

#define ypBytes_DATA( b ) ( (yp_uint8_t *) ((ypBytesObject *)b)->ob_data )
// TODO what if ob_len is the "invalid" value?
#define ypBytes_LEN( b )  ( ((ypObject *)b)->ob_len )

// Return a new bytes object with uninitialized data of the given length, or an exception
static ypObject *_yp_bytes_new( yp_ssize_t len )
{
    ypObject *b;
    ypMem_MALLOC_CONTAINER_INLINE( b, ypBytesObject, ypBytes_CODE, len );
    if( b == NULL ) return yp_MemoryError;
    b->ob_len = len;
    return b;
}

// Return a new bytes object with uninitialized data of the given length, or an exception
// TODO Over-allocate to avoid future resizings
static ypObject *_yp_bytearray_new( yp_ssize_t len )
{
    ypObject *b;
    ypMem_MALLOC_CONTAINER_VARIABLE( b, ypBytesObject, ypByteArray_CODE, len );
    if( b == NULL ) return yp_MemoryError;
    b->ob_len = len;
    return b;
}



// TODO resize bytearray



// If x is an exception, return immediately.  If x is a bool/int in range(256), store value in
// storage and set *x_data=storage, x_len=1.  If x is a fellow bytes, set *x_data and x_len.
// Returns NULL on success, exception on error.
static ypObject *_bytes_coerce( ypObject *x, yp_uint8_t **x_data, yp_ssize_t *x_len, 
        yp_uint8_t *storage )
{
    ypObject *result;
    int x_type = yp_TYPE_PAIR_CODE( x );
    if( x_type == ypException_CODE ) return x;

    if( x_type == yp_Bool_CODE || x_type == ypInt_CODE ) {
        result = yp_as_uint8C( x, storage );
        if( yp_isexceptionC( result ) ) return result;
        *x_data = storage;
        *x_len = 1;
    } else if( x_type == ypBytes_CODE ) {
        *x_data = ypBytes_DATA( x );
        *x_len = ypBytes_LEN( x );
    }
    return NULL; // success
}

static ypObject *bytes_contains( ypObject *b, ypObject *x )
{
    ypObject *result;
    yp_uint8_t *x_data;
    yp_ssize_t x_len;        
    yp_uint8_t storage;
    yp_uint8_t *b_rdata;   // remaining data
    yp_ssize_t b_rlen;     // remaining length
    
    result = _bytes_coerce( x, &x_data, &x_len, &storage );
    if( result != NULL ) return result;

    b_rdata = ypBytes_DATA( b );
    b_rlen = ypBytes_LEN( b );
    while( b_rlen >= x_len ) {
        if( memcmp( b_rdata, x_data, x_len ) == 0 ) return yp_True;
        b_rdata++; b_rlen--;
    }
    return yp_False;   
}

static ypObject *bytearray_iconcat( ypObject *b, ypObject *x )
{
    ypObject *result;
    yp_uint8_t *x_data;
    yp_ssize_t x_len;        
    yp_uint8_t storage;
    yp_uint8_t *b_rdata;   // remaining data
    yp_ssize_t b_rlen;     // remaining length
    
    result = _bytes_coerce( x, &x_data, &x_len, &storage );
    if( result != NULL ) return result;

    // TODO
    b_rdata = ypBytes_DATA( b );
    b_rlen = ypBytes_LEN( b );
    while( b_rlen >= x_len ) {
        if( memcmp( b_rdata, x_data, x_len ) == 0 ) return yp_True;
        b_rdata++; b_rlen--;
    }
    return yp_False;   
}

static ypObject *bytes_concat( ypObject *b, ypObject *x )
{
    // TODO freeze if b is frozen
}





// TODO undef macros


/*************************************************************************************************
 * Sequence of unicode characters
 *************************************************************************************************/


/*************************************************************************************************
 * Sequence of generic items
 *************************************************************************************************/





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
}


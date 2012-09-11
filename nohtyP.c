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
    ( ((ypObject *)(ob))->ob_type_refnt >> 8 )

// A refcnt of this value means the object is immortal
#define ypObject_REFCNT_IMMORTAL (0xFFFFFFu)

// When a hash of this value is stored in ob_alloclen_hash, call tp_hash (which may then update
// cache)
#define ypObject_HASH_NOT_CACHED (0xFFFFFFFFu)

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


typedef struct _typeobject {
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

#ifdef COUNT_ALLOCS
    /* these must be last and never explicitly initialized */
    yp_ssize_t tp_allocs;
    yp_ssize_t tp_frees;
    yp_ssize_t tp_maxalloc;
    struct _typeobject *tp_prev;
    struct _typeobject *tp_next;
#endif
} ypTypeObject;
#endif

// Codes for the standard types (for lookup in the type table)
#define ypInvalidated_TYPE          (  0u)
// no mutable ypInvalidated type    (  1u)
#define ypException_TYPE            (  2u)
// no mutable ypException type      (  3u)
#define ypType_TYPE                 (  4u)
// no mutable ypType type           (  5u)

#define ypNone_TYPE                 (  6u)
// no mutable ypNone type           (  7u)
#define ypBool_TYPE                 (  8u)
// no mutable ypBool type           (  9u)

#define ypInt_TYPE                  ( 10u)
// TODO what to call a mutable int  ( 11u)
#define ypFloat_TYPE                ( 12u)
// TODO what to call a mutable float( 13u)

// no immutable ypIter type         ( 14u)
#define ypIter_TYPE                 ( 15u)

#define ypBytes_TYPE                ( 16u)
#define ypByteArray_TYPE            ( 17u)
#define ypStr_TYPE                  ( 18u)
#define ypCharacterArray_TYPE       ( 19u)
#define ypTuple_TYPE                ( 20u)
#define ypList_TYPE                 ( 21u)

#define ypFrozenSet_TYPE            ( 22u)
#define ypSet_TYPE                  ( 23u)

#define ypFrozenDict_TYPE           ( 24u)
#define ypDict_TYPE                 ( 25u)


/*************************************************************************************************
 * Helpful functions and macros for the type methods/functions
 *************************************************************************************************/

// Functions that modify their inputs take a "ypObject **x"; use this as 
// "return_yp_INPLACE_ERR( *x, yp_TypeError );" to return the error properly
#define return_yp_INPLACE_ERR( ob, err ) \
    do { yp_decref( ob ); (ob) = (err); return; } while( 0 )



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

    // Freeze the object, discarding alloclen and possibly reducing memory usage, etc
    ypObject_SET_TYPE_CODE( *x, newCode );
    (*x)->ob_alloclen_hash = ypObject_HASH_NOT_CACHED;
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


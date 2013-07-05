/*
 * nohtyP.h - A Python-like API for C, in one .c and one .h
 *      http://bitbucket.org/Syeberman/nohtyp   [v0.1.0 $Change$]
 *      Copyright © 2001-2013 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/3/license.html
 *
 * The goal of nohtyP is to enable Python-like code to be written in C.  It is patterned after
 * Python's built-in API, then adjusted for expected usage patterns.  It also borrows ideas from
 * Python's own C API.  To be as portable as possible, it is written in one .c and one .h file and
 * is tested against multiple compilers.  The documentation below is complete, but brief; more
 * detailed documentation can be found at http://docs.python.org/3/.
 *
 * Most functions borrow inputs, create their own references, and output new references.  Errors
 * are handled in one of four ways.  Functions that return objects simply return an appropriate
 * exception object on error:
 *      value = yp_getitem( dict, key );
 *      if( yp_isexceptionC( value ) ) printf( "unknown key" );
 * Functions that modify objects accept a ypObject* by reference and replace that object with an
 * exception on error, discarding the original reference:
 *      yp_setitem( &dict, key, value );
 *      if( yp_isexceptionC( dict ) ) printf( "unhashable key, dict discarded" );
 * If you don't want the modified object discarded on error, use the 'E' version, which returns an
 * exception on error, otherwise yp_None:
 *      result = yp_setitemE( dict, key, value );
 *      if( yp_isexceptionC( result ) ) printf( "unhashable key, dict not modified" );
 * Finally, functions that return C values accept a ypObject** that is set to the exception; it is
 * set _only_ on error, and existing values are not discarded, so the variable should first be
 * initialized to an immortal:
 *      ypObject *result = yp_None;
 *      len = yp_lenC( x, &result );
 *      if( yp_isexceptionC( result ) ) printf( "x isn't a container" );
 * Exception objects are immortal, allowing you to return immediately, without having to call
 * yp_decref, if an error occurs.  Unless explicitly documented as "always succeeds", _any_
 * function can return an exception.
 *
 * It is possible to string together function calls without checking for errors in-between.  When
 * an exception object is used as input to a function, it is immediately returned.  This allows you
 * to check for errors only at the end of a block of code:
 *      newdict = yp_dictK( 0 );            // newdict might be yp_MemoryError
 *      value = yp_getitem( olddict, key ); // value could be yp_KeyError
 *      yp_iaddC( &value, 5 );              // possibly replaces value with yp_TypeError
 *      yp_setitem( &newdict, key, value ); // if value is an exception, newdict will be too
 *      yp_decref( value );                 // a no-op if value is an exception
 *      if( yp_isexceptionC( newdict ) ) abort( );
 *
 * This API is threadsafe so long as no objects are modified while being accessed by multiple
 * threads; this includes updating reference counts, so immutables are not inherently threadsafe!
 * One strategy to ensure safety is to deep copy objects before exchanging between threads.
 * Sharing immutable, immortal objects is always safe.
 *
 * Certain functions are given postfixes to highlight their unique behaviour:
 *  C - C native types are accepted and returned where appropriate
 *  F - A version of "C" that accepts floats in place of ints
 *  L - Library routines that operate strictly on C types
 *  N - n variable positional arguments follow
 *  K - n key/value arguments follow (for a total of n*2 arguments)
 *  V - A version of "N" or "K" that accepts a va_list in place of ...
 *  E - Errors modifying an object do not discard the object (yp_None/exception returned instead)
 *  D - Discard after use (ie yp_IFd)
 *  X - Direct access to internal memory or borrowed objects; tread carefully!
 *  # (number) - A function with # inputs that otherwise shares the same name as another function
 */


/*
 * Header Prerequisites
 */

#ifndef yp_NOHTYP_H
#define yp_NOHTYP_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdarg.h>
#include <limits.h>

#ifdef yp_ENABLE_SHARED
#ifdef yp_BUILD_CORE
#define ypAPI extern __declspec( dllexport )
#else
#define ypAPI extern __declspec( dllimport )
#endif
#else
#define ypAPI extern
#endif


/*
 * Initialization
 */

// Must be called once before any other function; subsequent calls are a no-op.  If a fatal error
// occurs abort() is called.  kwparams can be NULL to accept all defaults; further documentation
// on these parameters can be found below.
typedef struct _yp_initialize_kwparams yp_initialize_kwparams;
ypAPI void yp_initialize( const yp_initialize_kwparams *kwparams );


/*
 * Object Fundamentals
 */

// All objects maintain a reference count; when that count reaches zero, the object is deallocated.
// Certain objects are immortal and, thus, never deallocated; examples include yp_None, yp_True,
// yp_False, exceptions, and so forth.

// nohtyP objects are only accessed through pointers to ypObject.
typedef struct _ypObject ypObject;

// The immortal, null-reference object.
ypAPI ypObject * const yp_None;

// Increments the reference count of x, returning it as a convenience.  Always succeeds; if x is
// immortal this is a no-op.
ypAPI ypObject *yp_incref( ypObject *x );

// A convenience function to increment the references of n objects.
ypAPI void yp_increfN( int n, ... );
ypAPI void yp_increfV( int n, va_list args );

// Decrements the reference count of x, deallocating it if the count reaches zero.  Always
// succeeds; if x is immortal this is a no-op.
ypAPI void yp_decref( ypObject *x );

// A convenience function to decrement the references of n objects.
ypAPI void yp_decrefN( int n, ... );
ypAPI void yp_decrefV( int n, va_list args );

// Returns true (non-zero) if x is an exception, else false.  Always succeeds.
ypAPI int yp_isexceptionC( ypObject *x );


/*
 * Freezing, "Unfreezing", and Invalidating
 */

// All objects support a one-way freeze function that makes them immutable, an unfrozen_copy
// function that returns a mutable copy, and a one-way invalidate function that renders the object
// useless; there are also deep variants to these functions.  Supplying an invalidated object to a
// function results in yp_InvalidatedError.  Freezing and invalidating are two examples of object
// transmutation, where the type of the object is converted to a different type.  Unlike Python,
// most objects are copied in memory, even immutables, as copying is one method for maintaining
// threadsafety.

// Transmutes *x to its associated immutable type.  If *x is already immutable this is a no-op.
ypAPI void yp_freeze( ypObject **x );

// Freezes *x and, recursively, all contained objects.
ypAPI void yp_deepfreeze( ypObject **x );

// Returns a new reference to a mutable shallow copy of x.  If x has no associated mutable type an
// immutable copy is returned.  May also return yp_MemoryError.
ypAPI ypObject *yp_unfrozen_copy( ypObject *x );

// Creates a mutable copy of x and, recursively, all contained objects, returning a new reference.
ypAPI ypObject *yp_unfrozen_deepcopy( ypObject *x );

// Returns a new reference to an immutable shallow copy of x.  If x has no associated immutable
// type a new, invalidated object is returned.  May also return yp_MemoryError.
ypAPI ypObject *yp_frozen_copy( ypObject *x );

// Creates an immutable copy of x and, recursively, all contained objects, returning a new
// reference.
ypAPI ypObject *yp_frozen_deepcopy( ypObject *x );

// Returns a new reference to an exact, shallow copy of x.  May also return yp_MemoryError.
ypAPI ypObject *yp_copy( ypObject *x );

// Creates an exact copy of x and, recursively, all contained objects, returning a new reference.
ypAPI ypObject *yp_deepcopy( ypObject *x );

// Discards all contained objects in *x, deallocates _some_ memory, and transmutes it to the
// ypInvalidated type (rendering the object useless).  If *x is immortal or already invalidated
// this is a no-op; immutable objects _can_ be invalidated.
ypAPI void yp_invalidate( ypObject **x );

// Invalidates *x and, recursively, all contained objects.
ypAPI void yp_deepinvalidate( ypObject **x );


/*
 * Boolean Operations and Comparisons
 */

// There are exactly two boolean values, both immortal: yp_True and yp_False.
ypAPI ypObject * const yp_True;
ypAPI ypObject * const yp_False;

// Returns the immortal yp_False if the object should be considered false (yp_None, a number equal
// to zero, or a container of zero length), otherwise yp_True.
ypAPI ypObject *yp_bool( ypObject *x );

// Returns the immortal yp_True if x is considered false, otherwise yp_False.
ypAPI ypObject *yp_not( ypObject *x );

// Returns a *new* reference to y if x is false, otherwise to x.  Unlike Python, both arguments
// are always evaluated.  You may find yp_anyN more convenient, as it returns an immortal.
ypAPI ypObject *yp_or( ypObject *x, ypObject *y );

// A convenience function to "or" n objects.  Returns yp_False if n is zero, and the first object
// if n is one.  Returns a *new* reference; you may find yp_anyN more convenient, as it returns an
// immortal.
ypAPI ypObject *yp_orN( int n, ... );
ypAPI ypObject *yp_orV( int n, va_list args );

// Equivalent to yp_bool( yp_orN( n, ... ) ).  (Returns an immortal.)
ypAPI ypObject *yp_anyN( int n, ... );
ypAPI ypObject *yp_anyV( int n, va_list args );

// Returns the immortal yp_True if any element of iterable is true; if the iterable is empty,
// returns yp_False.  Stops iterating at the first true element.
ypAPI ypObject *yp_any( ypObject *iterable );

// Returns a *new* reference to x if x is false, otherwise to y.  Unlike Python, both
// arguments are always evaluated.  You may find yp_allN more convenient, as it returns an
// immortal.
ypAPI ypObject *yp_and( ypObject *x, ypObject *y );

// A convenience function to "and" n objects.  Returns yp_True if n is zero, and the first object
// if n is one.  Returns a *new* reference; you may find yp_allN more convenient, as it returns an
// immortal.
ypAPI ypObject *yp_andN( int n, ... );
ypAPI ypObject *yp_andV( int n, va_list args );

// Equivalent to yp_bool( yp_andN( n, ... ) ).  (Returns an immortal.)
ypAPI ypObject *yp_allN( int n, ... );
ypAPI ypObject *yp_allV( int n, va_list args );

// Returns the immortal yp_True if all elements of iterable are true or the iterable is empty.
// Stops iterating at the first false element.
ypAPI ypObject *yp_all( ypObject *iterable );

// Implements the "less than" (x<y), "less than or equal" (x<=y), "equal" (x==y), "not equal"
// (x!=y), "greater than or equal" (x>=y), and "greater than" (x>y) comparisons.  Returns the
// immortal yp_True if the condition is true, otherwise yp_False.
ypAPI ypObject *yp_lt( ypObject *x, ypObject *y );
ypAPI ypObject *yp_le( ypObject *x, ypObject *y );
ypAPI ypObject *yp_eq( ypObject *x, ypObject *y );
ypAPI ypObject *yp_ne( ypObject *x, ypObject *y );
ypAPI ypObject *yp_ge( ypObject *x, ypObject *y );
ypAPI ypObject *yp_gt( ypObject *x, ypObject *y );

// You may also be interested in yp_IF and yp_WHILE for working with boolean operations; see below.


/*
 * nohtyP's C types
 */

// Fixed-size numeric C types
typedef signed char         yp_int8_t;
typedef unsigned char       yp_uint8_t;
typedef short               yp_int16_t;
typedef unsigned short      yp_uint16_t;
#if UINT_MAX == 0xFFFFFFFFu
typedef int                 yp_int32_t;
typedef unsigned int        yp_uint32_t;
#else
typedef long                yp_int32_t;
typedef unsigned long       yp_uint32_t;
#endif
typedef long long           yp_int64_t;
typedef unsigned long long  yp_uint64_t;
typedef float               yp_float32_t;
typedef double              yp_float64_t;
#if SIZE_MAX == 0xFFFFFFFFu
typedef yp_int32_t          yp_ssize_t;
#else
typedef yp_int64_t          yp_ssize_t;
#endif
typedef yp_ssize_t          yp_hash_t;
#define yp_SSIZE_T_MAX ((yp_ssize_t) (SIZE_MAX / 2))
#define yp_SSIZE_T_MIN (-yp_SSIZE_T_MAX - 1)

// C types used to represent the numeric objects within nohtyP
typedef yp_int64_t      yp_int_t;
#define yp_INT_T_MAX LLONG_MAX
#define yp_INT_T_MIN LLONG_MIN
typedef yp_float64_t    yp_float_t;

// The signature of a function that can be wrapped up in a generator, called by yp_send and
// similar functions.  self is the iterator object; use yp_iter_stateX to retrieve any state
// variables.  value is the object that is sent into the function by yp_send; it may also be
// yp_GeneratorExit if yp_close is called, or another exception.  The return value must be a new
// reference, yp_StopIteration if the generator is exhausted, or another exception.  The generator
// will be closed if it returns an exception.
typedef ypObject *(*yp_generator_func_t)( ypObject *self, ypObject *value );


/*
 * Constructors
 */

// Unlike Python, most nohtyP types have both mutable and immutable versions.  An "intstore" is a
// mutable int (it "stores" an int); similar for floatstore.  The mutable str is called a
// "chrarray", while a "frozendict" is an immutable dict.  There are no useful immutable types for
// iters or files: attempting to freeze such types will close them.

// Returns a new reference to an int/intstore with the given value.
ypAPI ypObject *yp_intC( yp_int_t value );
ypAPI ypObject *yp_intstoreC( yp_int_t value );

#ifdef yp_FUTURE // FIXME support Unicode strings
// Returns a new reference to an int/intstore interpreting the C string as an integer literal with
// the given base.  Base zero means to infer the base according to Python's syntax.
ypAPI ypObject *yp_int_strC( const char *string, int base );
ypAPI ypObject *yp_intstore_strC( const char *string, int base );
#endif

// Returns a new reference to a float/floatstore with the given value.
ypAPI ypObject *yp_floatC( yp_float_t value );
ypAPI ypObject *yp_floatstoreC( yp_float_t value );

#ifdef yp_FUTURE // FIXME support Unicode strings
// Returns a new reference to a float/floatstore interpreting the string as a Python
// floating-point literal.
ypAPI ypObject *yp_float_strC( const char *string );
ypAPI ypObject *yp_floatstore_strC( const char *string );
#endif

// Returns a new reference to an iterator for object x.  It is usually unsafe to modify an object
// being iterated over.
ypAPI ypObject *yp_iter( ypObject *x );

// Returns a new reference to a generator-iterator object using the given func.  The function will
// be passed the given n objects as state on each call (the objects will form an array).  lenhint
// is a clue to consumers of the generator how many items will be yielded; use zero if this is not
// known.
ypAPI ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );
ypAPI ypObject *yp_generatorCV( yp_generator_func_t func, yp_ssize_t lenhint, int n, va_list args );

// Similar to yp_generatorCN, but accepts an arbitrary structure (or array) of the given
// size which will be copied into the iterator and maintained as state.  If state contains any
// objects, their offsets must be given as the variable arguments; new references to these objects
// will be created.  (Note that these objects cannot be contained in a union.)
ypAPI ypObject *yp_generator_fromstructCN( yp_generator_func_t func, yp_ssize_t lenhint,
        void *state, yp_ssize_t size, int n, ... );
ypAPI ypObject *yp_generator_fromstructCV( yp_generator_func_t func, yp_ssize_t lenhint,
        void *state, yp_ssize_t size, int n, va_list args );

// Returns a new reference to a range object. yp_rangeC is equivalent to yp_rangeC3( 0, stop, 1 ).
ypAPI ypObject *yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
ypAPI ypObject *yp_rangeC( yp_int_t stop );

// Returns a new reference to a bytes/bytearray, copying the first len bytes from source.  If
// source is NULL it is considered as having all null bytes; if len is negative source is
// considered null terminated (and, therefore, will not contain the null byte).
//  Ex: pre-allocate a bytearray of length 50: yp_bytearrayC( NULL, 50 )
ypAPI ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len );
ypAPI ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len );

// Returns a new reference to a str/chrarray decoded from the given bytes.  source and len are as
// in yp_bytesC.  The Python-equivalent default for encoding is yp_s_utf_8 (compatible with an
// ascii-encoded source), while for errors it is yp_s_strict.  Equivalent to:
//  yp_str3( yp_bytesC( source, len ), encoding, errors )
ypAPI ypObject *yp_str_frombytesC( const yp_uint8_t *source, yp_ssize_t len,
        ypObject *encoding, ypObject *errors );
ypAPI ypObject *yp_chrarray_frombytesC( const yp_uint8_t *source, yp_ssize_t len,
        ypObject *encoding, ypObject *errors );

// Equivalent to yp_str3( yp_bytesC( source, len ), yp_s_utf_8, yp_s_strict ).
ypAPI ypObject *yp_str_frombytesC2( const yp_uint8_t *source, yp_ssize_t len );
ypAPI ypObject *yp_chrarray_frombytesC2( const yp_uint8_t *source, yp_ssize_t len );

// Returns a new reference to a str/chrarray decoded from the given bytes or bytearray object.  The
// Python-equivalent default for encoding is yp_s_utf_8, while for errors it is yp_s_strict.
ypAPI ypObject *yp_str3( ypObject *object, ypObject *encoding, ypObject *errors );
ypAPI ypObject *yp_chrarray3( ypObject *object, ypObject *encoding, ypObject *errors );

// Returns a new reference to the "informal" or nicely-printable string representation of object,
// as a str/chrarray.
ypAPI ypObject *yp_str( ypObject *object );
ypAPI ypObject *yp_chrarray( ypObject *object );

// Returns a new reference to an empty str/chrarray.
ypAPI ypObject *yp_str0( void );
ypAPI ypObject *yp_chrarray0( void );

// Returns a new reference to the str representing a character whose Unicode codepoint is the
// integer i.
ypAPI ypObject *yp_chrC( yp_int_t i );

// Returns a new reference to a tuple/list of length n containing the given objects.
ypAPI ypObject *yp_tupleN( int n, ... );
ypAPI ypObject *yp_tupleV( int n, va_list args );
ypAPI ypObject *yp_listN( int n, ... );
ypAPI ypObject *yp_listV( int n, va_list args );

// Returns a new reference to a tuple/list made from factor shallow copies of yp_tupleN( n, ... )
// concatenated.  Equivalent to "factor * (obj0, obj1, ...)" in Python.
//  Ex: pre-allocate a list of length 99: yp_list_repeatCN( 99, 1, yp_None )
//  Ex: an 8-tuple containing alternating bools: yp_tuple_repeatCN( 4, 2, yp_False, yp_True )
ypAPI ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
ypAPI ypObject *yp_tuple_repeatCV( yp_ssize_t factor, int n, va_list args );
ypAPI ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );
ypAPI ypObject *yp_list_repeatCV( yp_ssize_t factor, int n, va_list args );

// Returns a new reference to a tuple/list whose elements come from iterable.
ypAPI ypObject *yp_tuple( ypObject *iterable );
ypAPI ypObject *yp_list( ypObject *iterable );

// Returns a new reference to a sorted list from the items in iterable.  key is a function that
// returns new or immortal references that are used as comparison keys; to compare the elements
// directly, use NULL.  If reverse is true, the list elements are sorted as if each comparison were
// reversed.
typedef ypObject *(*yp_sort_key_func_t)( ypObject *x );
ypAPI ypObject *yp_sorted3( ypObject *iterable, yp_sort_key_func_t key, ypObject *reverse );

// Equivalent to yp_sorted3( iterable, NULL, yp_False ).
ypAPI ypObject *yp_sorted( ypObject *iterable );

// Returns a new reference to a frozenset/set containing the given n objects; the length will be n,
// unless there are duplicate objects.
ypAPI ypObject *yp_frozensetN( int n, ... );
ypAPI ypObject *yp_frozensetV( int n, va_list args );
ypAPI ypObject *yp_setN( int n, ... );
ypAPI ypObject *yp_setV( int n, va_list args );

// Returns a new reference to a frozenset/set whose elements come from iterable.
ypAPI ypObject *yp_frozenset( ypObject *iterable );
ypAPI ypObject *yp_set( ypObject *iterable );

// Returns a new reference to a frozendict/dict containing the given n key/value pairs (for a total
// of 2*n objects); the length will be n, unless there are duplicate keys, in which case the last
// value will be retained.
//  Ex: yp_dictK( 3, key0, value0, key1, value1, key2, value2 )
ypAPI ypObject *yp_frozendictK( int n, ... );
ypAPI ypObject *yp_frozendictKV( int n, va_list args );
ypAPI ypObject *yp_dictK( int n, ... );
ypAPI ypObject *yp_dictKV( int n, va_list args );

// Returns a new reference to a frozendict/dict containing the given n keys all set to value; the
// length will be n, unless there are duplicate keys.  The Python-equivalent default of value is 
// yp_None.  Note that, unlike Python, value is the _first_ argument.
//  Ex: pre-allocate a dict with 3 keys: yp_dict_fromkeysN( yp_None, 3, key0, key1, key2 )
ypAPI ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
ypAPI ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args );
ypAPI ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
ypAPI ypObject *yp_dict_fromkeysV( ypObject *value, int n, va_list args );

// Returns a new reference to a frozendict/dict containing the keys from iterable all set to value.
// The Python-equivalent default of value is yp_None.
ypAPI ypObject *yp_frozendict_fromkeys( ypObject *iterable, ypObject *value );
ypAPI ypObject *yp_dict_fromkeys( ypObject *iterable, ypObject *value );

// Returns a new reference to a frozendict/dict whose key-value pairs come from x.  x can be a
// mapping object (that supports yp_iter_items), or an iterable that yields exactly two items at a
// time (ie (key, value)).  If a given key is seen more than once, the last value yielded is
// retained.
ypAPI ypObject *yp_frozendict( ypObject *x );
ypAPI ypObject *yp_dict( ypObject *x );

// XXX The file type will be added in a future version


/*
 * Generic Object Operations
 */

// Returns the hash value of x; x must be immutable.  Returns -1 and sets *exc on error.
ypAPI yp_hash_t yp_hashC( ypObject *x, ypObject **exc );

// Returns the _current_ hash value of x; if x is mutable, this value may change between calls.
// Returns -1 and sets *exc on error.  (Unlike Python, this can calculate the hash value of mutable
// types.)
ypAPI yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc );


/*
 * Iterator Operations
 */

// As per Python, an "iterator" is an object that implements yp_next, while an "iterable" is an
// object that implements yp_iter.  Examples of iterables include bytes, str, tuple, set, and dict.

// Unlike other functions that modify their inputs, yp_send et al do not discard iterator on error.
// Instead, if an error occurs in one yp_send, subsequent calls will raise yp_StopIteration.

// "Sends" a value into iterator and returns a new reference to the next yielded value,
// yp_StopIteration if the iterator is exhausted, or another exception.  The value may be ignored
// by the iterator.  If value is an exception this behaves like yp_throw.
ypAPI ypObject *yp_send( ypObject *iterator, ypObject *value );

// Equivalent to yp_send( iterator, yp_None ).
ypAPI ypObject *yp_next( ypObject *iterator );

// Similar to yp_next, but returns a new reference to defval when the iterator is exhausted.
// defval _can_ be an exception; the Python-equivalent "default" of defval is yp_StopIteration.
ypAPI ypObject *yp_next2( ypObject *iterator, ypObject *defval );

// "Throws" an exception into iterator and returns a new reference to the next yielded value,
// yp_StopIteration if the iterator is exhausted, or another exception.  exc _must_ be an
// exception.
ypAPI ypObject *yp_throw( ypObject *iterator, ypObject *exc );

// Returns a hint as to how many items are left to be yielded.  The accuracy of this hint depends
// on the underlying type: most containers know their lengths exactly, but some generators may not.
// A hint of zero could mean that the iterator is exhausted, that the length is unknown, or that
// the iterator will yield infinite values.  Returns zero and sets *exc on error.
ypAPI yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc );

// Typically only called from within yp_generator_func_t functions.  Sets *state and *size to the
// internal generator state buffer and its size in bytes, and returns the immortal yp_None.  The
// structure and initial values of *state are determined by the call to the generator constructor;
// the function cannot change the size after creation, and any ypObject*s in *state should be
// considered *borrowed* (it is safe to replace them with new references).  Sets *state to NULL,
// *size to zero, and returns an exception on error.
ypAPI ypObject *yp_iter_stateX( ypObject *iterator, void **state, yp_ssize_t *size );

// "Closes" the iterator by calling yp_throw( iterator, yp_GeneratorExit ).  If yp_StopIteration or
// yp_GeneratorExit is returned by yp_throw, *iterator is not discarded, otherwise *iterator is
// replaced with an exception.  The behaviour of this function for other types, in particular
// files, is documented elsewhere.
ypAPI void yp_close( ypObject **iterator );

// Sets the given n ypObject**s to new references for the values yielded from iterable.  Iterable
// must yield exactly n objects, or else a yp_ValueError is raised.  Sets all n ypObject**s to the
// same exception on error.  There is no 'V' version of this function.
ypAPI void yp_unpackN( ypObject *iterable, int n, ... );

// Returns a new reference to an iterator that yields values from iterable for which function
// returns true.  The given function must return new or immortal references, as each returned
// value will be discarded; to inspect the elements directly, use NULL.
typedef ypObject *(*yp_filter_function_t)( ypObject *x );
ypAPI ypObject *yp_filter( yp_filter_function_t function, ypObject *iterable );

// Similar to yp_filter, but yields values for which function returns false.
ypAPI ypObject *yp_filterfalse( yp_filter_function_t function, ypObject *iterable );

// Returns a new reference to the largest/smallest of the given n objects.  key is a function that
// returns new or immortal references that are used as comparison keys; to compare the elements
// directly, use NULL.
ypAPI ypObject *yp_max_keyN( yp_sort_key_func_t key, int n, ... );
ypAPI ypObject *yp_max_keyV( yp_sort_key_func_t key, int n, va_list args );
ypAPI ypObject *yp_min_keyN( yp_sort_key_func_t key, int n, ... );
ypAPI ypObject *yp_min_keyV( yp_sort_key_func_t key, int n, va_list args );

// Equivalent to yp_max_keyN( NULL, n, ... ) and yp_min_keyN( NULL, n, ... ).
ypAPI ypObject *yp_maxN( int n, ... );
ypAPI ypObject *yp_maxV( int n, va_list args );
ypAPI ypObject *yp_minN( int n, ... );
ypAPI ypObject *yp_minV( int n, va_list args );

// Returns a new reference to the largest/smallest element in iterable.  key is as in yp_max_keyN.
ypAPI ypObject *yp_max_key( ypObject *iterable, yp_sort_key_func_t key );
ypAPI ypObject *yp_min_key( ypObject *iterable, yp_sort_key_func_t key );

// Equivalent to yp_max_key( iterable, NULL ) and yp_min_key( iterable, NULL ).
ypAPI ypObject *yp_max( ypObject *iterable );
ypAPI ypObject *yp_min( ypObject *iterable );

// Returns a new reference to an iterator that yields the elements of seq in reverse order.
ypAPI ypObject *yp_reversed( ypObject *seq );

// Returns a new reference to an iterator that aggregates elements from each of the n iterables.
ypAPI ypObject *yp_zipN( int n, ... );
ypAPI ypObject *yp_zipV( int n, va_list args );

// You may also be interested in yp_FOR for working with iterables; see below.


/*
 * Container Operations
 */

// These methods are supported by bytes, str, tuple, frozenset, and frozendict (and their mutable
// counterparts, of course).

// Returns the immortal yp_True if an item of container is equal to x, else yp_False.
ypAPI ypObject *yp_contains( ypObject *container, ypObject *x );
ypAPI ypObject *yp_in( ypObject *x, ypObject *container );

// Returns the immortal yp_False if an item of container is equal to x, else yp_True.
ypAPI ypObject *yp_not_in( ypObject *x, ypObject *container );

// Returns the length of container.  Returns zero and sets *exc on error.
ypAPI yp_ssize_t yp_lenC( ypObject *container, ypObject **exc );

// Adds an item to *container.  On error, *container is discarded and set to an exception.  The
// relation between yp_push and yp_pop depends on the type: x may be the first or last item popped,
// or items may be popped in arbitrary order.
ypAPI void yp_push( ypObject **container, ypObject *x );

// Removes all items from *container.  On error, *container is discarded and set to an exception.
ypAPI void yp_clear( ypObject **container );

// Removes an item from *container and returns a new reference to it.  On error, *container is
// discarded and set to an exception _and_ an exception is returned.  (Not supported on dicts; use
// yp_popvalue or yp_popitem instead.)
ypAPI ypObject *yp_pop( ypObject **container );


/*
 * Sequence Operations
 */

// These methods are supported by bytes, str, and tuple (and their mutable counterparts, of
// course).  They are _not_ supported by frozenset and frozendict because those types do not store
// their elements in any particular order.

// Returns a new reference to the concatenation of sequence and x.
ypAPI ypObject *yp_concat( ypObject *sequence, ypObject *x );

// Returns a new reference to factor shallow copies of sequence, concatenated.
ypAPI ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor );

// Returns a new reference to the i-th item of sequence, origin zero.  Negative indicies are
// handled as in Python.
ypAPI ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i );

// Returns a new reference to the slice of sequence from i to j with step k.  The Python-equivalent
// "defaults" for i and j are yp_SLICE_DEFAULT, while for k it is 1.
ypAPI ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

// Equivalent to yp_getindexC( sequence, yp_asssizeC( key, &exc ) ).
ypAPI ypObject *yp_getitem( ypObject *sequence, ypObject *key );

// Returns the lowest index in sequence where x is found, such that x is contained in the slice
// sequence[i:j], or -1 if x is not found.  Returns -1 and sets *exc on error; *exc is _not_ set
// if x is simply not found.  As in Python, types such as tuples inspect only one item at a time,
// while types such as strs look for a particular sub-sequence of items.
ypAPI yp_ssize_t yp_findC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc );

// Equivalent to yp_findC4( sequence, x, 0, yp_SLICE_USELEN, exc ).
ypAPI yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc );

// Similar to yp_findC4 and yp_findC, except sets *exc to yp_ValueError if x is not found.
ypAPI yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc );
ypAPI yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc );

// Returns the total number of non-overlapping occurences of x in sequence.  Returns 0 and sets
// *exc on error.
ypAPI yp_ssize_t yp_countC( ypObject *sequence, ypObject *x, ypObject **exc );

// Sets the i-th item of *sequence, origin zero, to x.  Negative indicies are handled as in
// Python.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_setindexC( ypObject **sequence, yp_ssize_t i, ypObject *x );

// Sets the slice of *sequence, from i to j with step k, to x.  The Python-equivalent "defaults"
// for i and j are yp_SLICE_DEFAULT, while for k it is 1.  On error, *sequence is discarded and
// set to an exception.
ypAPI void yp_setsliceC5( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k,
        ypObject *x );

// Equivalent to yp_setindexC( sequence, yp_asssizeC( key, &exc ), x ).
ypAPI void yp_setitem( ypObject **sequence, ypObject *key, ypObject *x );

// Removes the i-th item from *sequence, origin zero.  Negative indicies are handled as in Python.
// On error, *sequence is discarded and set to an exception.
ypAPI void yp_delindexC( ypObject **sequence, yp_ssize_t i );

// Removes the elements of the slice from *sequence, from i to j with step k.  The Python-
// equivalent "defaults" for i and j are yp_SLICE_DEFAULT, while for k it is 1.  On error,
// *sequence is discarded and set to an exception.
ypAPI void yp_delsliceC4( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

// Equivalent to yp_delindexC( sequence, yp_asssizeC( key, &exc ) ).
ypAPI void yp_delitem( ypObject **sequence, ypObject *key );

// Appends x to the end of *sequence.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_append( ypObject **sequence, ypObject *x );
ypAPI void yp_push( ypObject **sequence, ypObject *x );

// Appends the contents of t to the end of *sequence.  On error, *sequence is discarded and set to
// an exception.
ypAPI void yp_extend( ypObject **sequence, ypObject *t );

// Appends the contents of *sequence to itself factor-1 times; if factor is zero *sequence is
// cleared.  Equivalent to seq*=factor for lists in Python.  On error, *sequence is discarded and
// set to an exception.
ypAPI void yp_irepeatC( ypObject **sequence, yp_ssize_t factor );

// Inserts x into *sequence at the index given by i; existing elements are shifted to make room.
// On error, *sequence is discarded and set to an exception.
ypAPI void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x );

// Removes the i-th item from *sequence and returns it.  The Python-equivalent "default" for i is
// -1.  On error, *sequence is discarded and set to an exception _and_ an exception is returned.
ypAPI ypObject *yp_popindexC( ypObject **sequence, yp_ssize_t i );

// Equivalent to yp_popindexC( sequence, -1 ).  Note that for sequences, yp_push and yp_pop
// together implement a stack (last in, first out).
ypAPI ypObject *yp_pop( ypObject **sequence );

// Removes the first item from *sequence that equals x.  Raises yp_ValueError if x is not
// contained in *sequence.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_remove( ypObject **sequence, ypObject *x );

// Removes the first item from *sequence that equals x, if one is present.  On error, *sequence
// is discarded and set to an exception.
ypAPI void yp_discard( ypObject **sequence, ypObject *x );

// Reverses the items of *sequence in-place.  On error, *sequence is discarded and set to an
// exception.
ypAPI void yp_reverse( ypObject **sequence );

// Sorts the items of *sequence in-place.  key is a function that returns new or immortal
// references that are used as comparison keys; to compare the elements directly, use NULL.  If
// reverse is true, the list elements are sorted as if each comparison were reversed.  On error,
// *sequence is discarded and set to an exception.
ypAPI void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse );

// Equivalent to yp_sort3( sequence, NULL, yp_False ).
ypAPI void yp_sort( ypObject **sequence );

// When given to a slice-like start/stop C argument, signals that the default "end" value be used
// for the argument; which end depends on the sign of step.  If you know the sign of step, you may
// prefer 0 and ypSlice_USELEN instead.
//  Ex: The nohtyP equivalent of "[::a]" is "yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, a"
#define yp_SLICE_DEFAULT yp_SSIZE_T_MIN

// When given to a slice-like start/stop C argument, signals that yp_lenC should be used; in other
// words, it signals that the slice should start/stop at the end of the sequence.
//  Ex: The nohtyP equivalent of "[:]" is "0, yp_SLICE_USELEN, 1"
#define yp_SLICE_USELEN  yp_SSIZE_T_MAX


/*
 * Set Operations
 */

// Returns the immortal yp_True if set has no elements in common with x, else yp_False.
ypAPI ypObject *yp_isdisjoint( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in set is in x, else yp_False.
ypAPI ypObject *yp_issubset( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in set is in x and x has additional elements, else
// yp_False.
ypAPI ypObject *yp_lt( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in x is in set, else yp_False.
ypAPI ypObject *yp_issuperset( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in x is in set and set has additional elements,
// else yp_False.
ypAPI ypObject *yp_gt( ypObject *set, ypObject *x );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// from set and all n objects.
ypAPI ypObject *yp_unionN( ypObject *set, int n, ... );
ypAPI ypObject *yp_unionV( ypObject *set, int n, va_list args );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// common to the set and all n objects.
ypAPI ypObject *yp_intersectionN( ypObject *set, int n, ... );
ypAPI ypObject *yp_intersectionV( ypObject *set, int n, va_list args );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// from set that are not in the n objects.
ypAPI ypObject *yp_differenceN( ypObject *set, int n, ... );
ypAPI ypObject *yp_differenceV( ypObject *set, int n, va_list args );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// in either set or x but not both.
ypAPI ypObject *yp_symmetric_difference( ypObject *set, ypObject *x );

// Add the elements from the n objects to *set.  On error, *set is discarded and set to an
// exception.
ypAPI void yp_updateN( ypObject **set, int n, ... );
ypAPI void yp_updateV( ypObject **set, int n, va_list args );

// Removes elements from *set that are not contained in all n objects.  On error, *set is discarded
// and set to an exception.
ypAPI void yp_intersection_updateN( ypObject **set, int n, ... );
ypAPI void yp_intersection_updateV( ypObject **set, int n, va_list args );

// Removes elements from *set that are contained in any of the n objects.  On error, *set is
// discarded and set to an exception.
ypAPI void yp_difference_updateN( ypObject **set, int n, ... );
ypAPI void yp_difference_updateV( ypObject **set, int n, va_list args );

// Removes elements from *set that are contained in x, and adds elements from x not contained in
// *set.  On error, *set is discarded and set to an exception.
ypAPI void yp_symmetric_difference_update( ypObject **set, ypObject *x );

// Adds element x to *set.  On error, *set is discarded and set to an exception.  While Python
// calls this method add, yp_add is already used for "a+b", so these two equivalent aliases are
// provided instead.
ypAPI void yp_push( ypObject **set, ypObject *x );
ypAPI void yp_set_add( ypObject **set, ypObject *x );

// If x is already contained in set, returns yp_KeyError; otherwise, adds x to set and returns
// the immortal yp_None.  Returns an exception on error; set is never discarded.
ypAPI ypObject *yp_pushuniqueE( ypObject *set, ypObject *x );

// Removes element x from *set.  Raises yp_KeyError if x is not contained in *set.  On error,
// *set is discarded and set to an exception.
ypAPI void yp_remove( ypObject **set, ypObject *x );

// Removes element x from *set if it is present.  On error, *set is discarded and set to an
// exception.
ypAPI void yp_discard( ypObject **set, ypObject *x );

// Removes an arbitrary item from *set and returns a new reference to it.  On error, *set is
// discarded and set to an exception _and_ an exception is returned.  You cannot use the order
// of yp_push calls on sets to determine the order of yp_pop'ped elements.
ypAPI ypObject *yp_pop( ypObject **set );


/*
 * Mapping Operations
 */

// frozendicts and dicts are both mapping objects.

// Returns a new reference to the value of mapping with the given key.  Returns yp_KeyError if key
// is not in the map.
ypAPI ypObject *yp_getitem( ypObject *mapping, ypObject *key );

// Adds or replaces the value of *mapping with the given key, setting it to x.  On error, *mapping
// is discarded and set to an exception.
ypAPI void yp_setitem( ypObject **mapping, ypObject *key, ypObject *x );

// Removes the item with the given key from *mapping.  Raises yp_KeyError if key is not in
// *mapping.  On error, *mapping is discarded and set to an exception.
ypAPI void yp_delitem( ypObject **mapping, ypObject *key );

// As in Python, yp_contains, yp_in, yp_not_in, and yp_iter operate solely on a mapping's keys.

// Similar to yp_getitem, but returns a new reference to defval if key is not in the map.  defval
// _can_ be an exception; the Python-equivalent "default" for defval is yp_None.
ypAPI ypObject *yp_getdefault( ypObject *mapping, ypObject *key, ypObject *defval );

// Returns a new reference to an iterator that yields mapping's (key, value) pairs as 2-tuples.
ypAPI ypObject *yp_iter_items( ypObject *mapping );

// Returns a new reference to an iterator that yields mapping's keys.
ypAPI ypObject *yp_iter_keys( ypObject *mapping );

// If key is in mapping, remove it and return a new reference to its value, else return a new
// reference to defval.  defval _can_ be an exception: if key is in mapping the method succeeds,
// otherwise the method fails with the specified exception.  The Python-equivalent "default" of
// defval is yp_KeyError.  On error, *mapping is discarded and set to an exception (defval,
// perhaps) _and_ that exception is returned.  Note that yp_push and yp_pop are not applicable for
// mapping objects.
ypAPI ypObject *yp_popvalue3( ypObject **mapping, ypObject *key, ypObject *defval );

// Removes an arbitrary item from *mapping and returns new references to its *key and *value.  If
// mapping is empty yp_KeyError is raised.  On error, *mapping is discarded and set to an exception
// _and_ both *key and *value are set to exceptions.
ypAPI void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value );

// Similar to yp_getitem, but returns a new reference to defval _and_ adds it to *mapping if key is
// not in the map.  The Python-equivalent "default" for defval is yp_None; as defval may be added
// to the map, it cannot be an exception.  On error, *mapping is discarded and set to an exception
// _and_ an exception is returned.
ypAPI ypObject *yp_setdefault( ypObject **mapping, ypObject *key, ypObject *defval );

// Add the given n key/value pairs (for a total of 2*n objects) to *mapping, overwriting existing
// keys.  If a given key is seen more than once, the last value is retained.  On error, *mapping is
// discarded and set to an exception.
ypAPI void yp_updateK( ypObject **mapping, int n, ... );
ypAPI void yp_updateKV( ypObject **mapping, int n, va_list args );

// Add the elements from the n objects to *mapping.  Each object is handled as per yp_dict.  On
// error, *mapping is discarded and set to an exception.
ypAPI void yp_updateN( ypObject **mapping, int n, ... );
ypAPI void yp_updateV( ypObject **mapping, int n, va_list args );

// Returns a new reference to an iterator that yields mapping's values.
ypAPI ypObject *yp_iter_values( ypObject *mapping );


/*
 * Bytes & String Operations
 */

// Individual elements of bytes and bytearrays are ints, so yp_getindexC will always return
// immutable ints for these types, and will only accept ints for yp_setindexC; single-byte bytes
// and bytearray objects _are_ accepted for certain operations like yp_contains.  Contrast this to
// strs and chrarrays, whose elements are strictly immutable, single-character strs.  Slicing an
// object returns an object of the same type, so yp_getsliceC4 on a chrarray object returns
// another chrarray object, and so forth.

// Immortal strs representing common encodings, for convience with yp_str_frombytesC et al.
ypAPI ypObject * const yp_s_ascii;     // "ascii"
ypAPI ypObject * const yp_s_latin_1;   // "latin_1"
ypAPI ypObject * const yp_s_utf_32;    // "utf_32"
ypAPI ypObject * const yp_s_utf_32_be; // "utf_32_be"
ypAPI ypObject * const yp_s_utf_32_le; // "utf_32_le"
ypAPI ypObject * const yp_s_utf_16;    // "utf_16"
ypAPI ypObject * const yp_s_utf_16_be; // "utf_16_be"
ypAPI ypObject * const yp_s_utf_16_le; // "utf_16_le"
ypAPI ypObject * const yp_s_utf_8;     // "utf_8"

// Immortal strs representing common string decode error handling schemes, for convience with
// yp_str_frombytesC et al.
ypAPI ypObject * const yp_s_strict;    // "strict"
ypAPI ypObject * const yp_s_ignore;    // "ignore"
ypAPI ypObject * const yp_s_replace;   // "replace"

// XXX Additional bytes- and str-specific methods will be added in a future version


/*
 * Numeric Operations
 */

// The numeric types include ints and floats (and their mutable counterparts, of course).

// Each of these functions return new reference(s) to the result of the given numeric operation;
// for example, yp_add returns the result of adding x and y together.  If the given operands do not
// support the operation, yp_TypeError is returned.  Additional notes:
//  - yp_divmod returns two objects via *div and *mod; on error, they are both set to an exception
//  - If z is yp_None, yp_pow3 returns x to the power y, otherwise x to the power y modulo z
//  - To avoid confusion with the logical operators of the same name, yp_amp implements bitwise
//  and, while yp_bar implements bitwise or
//  - Unlike Python, non-numeric types do not (currently) overload these operators
ypAPI ypObject *yp_add( ypObject *x, ypObject *y );
ypAPI ypObject *yp_sub( ypObject *x, ypObject *y );
ypAPI ypObject *yp_mul( ypObject *x, ypObject *y );
ypAPI ypObject *yp_truediv( ypObject *x, ypObject *y );
ypAPI ypObject *yp_floordiv( ypObject *x, ypObject *y );
ypAPI ypObject *yp_mod( ypObject *x, ypObject *y );
ypAPI void yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod );
ypAPI ypObject *yp_pow( ypObject *x, ypObject *y );
ypAPI ypObject *yp_pow3( ypObject *x, ypObject *y, ypObject *z );
ypAPI ypObject *yp_lshift( ypObject *x, ypObject *y );
ypAPI ypObject *yp_rshift( ypObject *x, ypObject *y );
ypAPI ypObject *yp_amp( ypObject *x, ypObject *y );
ypAPI ypObject *yp_xor( ypObject *x, ypObject *y );
ypAPI ypObject *yp_bar( ypObject *x, ypObject *y );
ypAPI ypObject *yp_neg( ypObject *x );
ypAPI ypObject *yp_pos( ypObject *x );
ypAPI ypObject *yp_abs( ypObject *x );
ypAPI ypObject *yp_invert( ypObject *x );

// In-place versions of the above; if the object *x can be modified to hold the result, it is,
// otherwise *x is discarded and replaced with the result.  If *x is immutable on input, an
// immutable object is returned, otherwise a mutable object is returned.  On error, *x is
// discarded and set to an exception.
ypAPI void yp_iadd( ypObject **x, ypObject *y );
ypAPI void yp_isub( ypObject **x, ypObject *y );
ypAPI void yp_imul( ypObject **x, ypObject *y );
ypAPI void yp_itruediv( ypObject **x, ypObject *y );
ypAPI void yp_ifloordiv( ypObject **x, ypObject *y );
ypAPI void yp_imod( ypObject **x, ypObject *y );
ypAPI void yp_ipow( ypObject **x, ypObject *y );
ypAPI void yp_ipow3( ypObject **x, ypObject *y, ypObject *z );
ypAPI void yp_ilshift( ypObject **x, ypObject *y );
ypAPI void yp_irshift( ypObject **x, ypObject *y );
ypAPI void yp_iamp( ypObject **x, ypObject *y );
ypAPI void yp_ixor( ypObject **x, ypObject *y );
ypAPI void yp_ibar( ypObject **x, ypObject *y );
ypAPI void yp_ineg( ypObject **x );
ypAPI void yp_ipos( ypObject **x );
ypAPI void yp_iabs( ypObject **x );
ypAPI void yp_iinvert( ypObject **x );

// Versions of yp_iadd et al that accept a C integer as the second argument.  Remember that *x may
// be discarded and replaced with the result.
ypAPI void yp_iaddC( ypObject **x, yp_int_t y );
ypAPI void yp_isubC( ypObject **x, yp_int_t y );
ypAPI void yp_imulC( ypObject **x, yp_int_t y );
ypAPI void yp_itruedivC( ypObject **x, yp_int_t y );
ypAPI void yp_ifloordivC( ypObject **x, yp_int_t y );
ypAPI void yp_imodC( ypObject **x, yp_int_t y );
ypAPI void yp_ipowC( ypObject **x, yp_int_t y );
ypAPI void yp_ipowC3( ypObject **x, yp_int_t y, yp_int_t z );
ypAPI void yp_ilshiftC( ypObject **x, yp_int_t y );
ypAPI void yp_irshiftC( ypObject **x, yp_int_t y );
ypAPI void yp_iampC( ypObject **x, yp_int_t y );
ypAPI void yp_ixorC( ypObject **x, yp_int_t y );
ypAPI void yp_ibarC( ypObject **x, yp_int_t y );

// Versions of yp_iadd et al that accept a C floating-point as the second argument.  Remember that
// *x may be discarded and replaced with the result.
ypAPI void yp_iaddFC( ypObject **x, yp_float_t y );
ypAPI void yp_isubFC( ypObject **x, yp_float_t y );
ypAPI void yp_imulFC( ypObject **x, yp_float_t y );
ypAPI void yp_itruedivFC( ypObject **x, yp_float_t y );
ypAPI void yp_ifloordivFC( ypObject **x, yp_float_t y );
ypAPI void yp_imodFC( ypObject **x, yp_float_t y );
ypAPI void yp_ipowFC( ypObject **x, yp_float_t y );
ypAPI void yp_ipowFC3( ypObject **x, yp_float_t y, yp_float_t z );
ypAPI void yp_ilshiftFC( ypObject **x, yp_float_t y );
ypAPI void yp_irshiftFC( ypObject **x, yp_float_t y );
ypAPI void yp_iampFC( ypObject **x, yp_float_t y );
ypAPI void yp_ixorFC( ypObject **x, yp_float_t y );
ypAPI void yp_ibarFC( ypObject **x, yp_float_t y );

// Library routines for nohtyP integer operations on C types.  Returns a reasonable value and sets
// *exc on error; "reasonable" usually means "truncated".
ypAPI yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_subL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_mulL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_truedivL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_floordivL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_modL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI void yp_divmodL( yp_int_t x, yp_int_t y, yp_int_t *div, yp_int_t *mod, ypObject **exc );
ypAPI yp_int_t yp_powL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_powL3( yp_int_t x, yp_int_t y, yp_int_t z, ypObject **exc );
ypAPI yp_int_t yp_lshiftL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_rshiftL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_ampL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_xorL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_barL( yp_int_t x, yp_int_t y, ypObject **exc );
ypAPI yp_int_t yp_negL( yp_int_t x, ypObject **exc );
ypAPI yp_int_t yp_posL( yp_int_t x, ypObject **exc );
ypAPI yp_int_t yp_absL( yp_int_t x, ypObject **exc );
ypAPI yp_int_t yp_invertL( yp_int_t x, ypObject **exc );

// Library routines for nohtyP floating-point operations on C types.  Returns a reasonable value
// and sets *exc on error; "reasonable" usually means "truncated".
ypAPI yp_float_t yp_addFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_subFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_mulFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_truedivFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_floordivFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_modFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI void yp_divmodFL( yp_float_t x, yp_float_t y,
        yp_float_t *div, yp_float_t *mod, ypObject **exc );
ypAPI yp_float_t yp_powFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_powFL3( yp_float_t x, yp_float_t y, yp_float_t z, ypObject **exc );
ypAPI yp_float_t yp_lshiftFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_rshiftFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_ampFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_xorFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_barFL( yp_float_t x, yp_float_t y, ypObject **exc );
ypAPI yp_float_t yp_negFL( yp_float_t x, ypObject **exc );
ypAPI yp_float_t yp_posFL( yp_float_t x, ypObject **exc );
ypAPI yp_float_t yp_absFL( yp_float_t x, ypObject **exc );
ypAPI yp_float_t yp_invertFL( yp_float_t x, ypObject **exc );

// Conversion routines from C types or objects to C types.  Returns a reasonable value and sets
// *exc on error; "reasonable" usually means "truncated".  Converting a float to an int truncates
// toward zero but is not an error.
ypAPI yp_int_t yp_asintC( ypObject *x, ypObject **exc );
ypAPI yp_int8_t yp_asint8C( ypObject *x, ypObject **exc );
ypAPI yp_uint8_t yp_asuint8C( ypObject *x, ypObject **exc );
ypAPI yp_int16_t yp_asint16C( ypObject *x, ypObject **exc );
ypAPI yp_uint16_t yp_asuint16C( ypObject *x, ypObject **exc );
ypAPI yp_int32_t yp_asint32C( ypObject *x, ypObject **exc );
ypAPI yp_uint32_t yp_asuint32C( ypObject *x, ypObject **exc );
ypAPI yp_int64_t yp_asint64C( ypObject *x, ypObject **exc );
ypAPI yp_uint64_t yp_asuint64C( ypObject *x, ypObject **exc );
ypAPI yp_float_t yp_asfloatC( ypObject *x, ypObject **exc );
ypAPI yp_float32_t yp_asfloat32C( ypObject *x, ypObject **exc );
ypAPI yp_float64_t yp_asfloat64C( ypObject *x, ypObject **exc );
ypAPI yp_ssize_t yp_asssizeC( ypObject *x, ypObject **exc );
ypAPI yp_hash_t yp_ashashC( ypObject *x, ypObject **exc );
ypAPI yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc );
ypAPI yp_int_t yp_asintFL( yp_float_t x, ypObject **exc );

// Return a new reference to x rounded to ndigits after the decimal point.
ypAPI ypObject *yp_roundC( ypObject *x, int ndigits );

// Sums the n given objects and returns the total.
ypAPI ypObject *yp_sumN( int n, ... );
ypAPI ypObject *yp_sumV( int n, va_list args );

// Sums the items of iterable and returns the total.
ypAPI ypObject *yp_sum( ypObject *iterable );

// The maximum and minimum integer values, as immortal objects.
ypAPI ypObject * const yp_sys_maxint;
ypAPI ypObject * const yp_sys_minint;

// TODO yp_i_zero, yp_i_one, etc?


/*
 * Type Operations
 */

// Return the immortal type of object.  If object is an exception, yp_type_exception is returned;
// if it is invalidated, yp_type_invalidated is returned.
ypAPI ypObject *yp_type( ypObject *object );

// The immortal type objects
ypAPI ypObject * const yp_type_invalidated;
ypAPI ypObject * const yp_type_exception;
ypAPI ypObject * const yp_type_type;
ypAPI ypObject * const yp_type_NoneType;
ypAPI ypObject * const yp_type_bool;
ypAPI ypObject * const yp_type_int;
ypAPI ypObject * const yp_type_intstore;
ypAPI ypObject * const yp_type_float;
ypAPI ypObject * const yp_type_floatstore;
ypAPI ypObject * const yp_type_iter;
ypAPI ypObject * const yp_type_bytes;
ypAPI ypObject * const yp_type_bytearray;
ypAPI ypObject * const yp_type_str;
ypAPI ypObject * const yp_type_chrarray;
ypAPI ypObject * const yp_type_tuple;
ypAPI ypObject * const yp_type_list;
ypAPI ypObject * const yp_type_frozenset;
ypAPI ypObject * const yp_type_set;
ypAPI ypObject * const yp_type_frozendict;
ypAPI ypObject * const yp_type_dict;


/*
 * Mini Iterators
 */

// yp_iter usually returns a newly-allocated object.  Many types can function as "mini iterators"
// which can be used to avoid having to allocate/deallocate a separate iterator object.  The
// objects returned by yp_miniiter must be used in a very specific manner: you should only call
// yp_miniiter_* and yp_decref on them (yp_incref is also safe, but is usually unnecessary).
// The behaviour of any other functions is undefined and may or may not raise exceptions.
// For convenience, yp_miniiter is always supported if yp_iter is, and vice-versa; as a
// consequence, yp_miniiter _may_ actually allocate a new object, although this is rare.  Example:
//      yp_uint64_t mi_state;
//      ypObject *mi = yp_miniiter( list, &mi_state );
//      while( 1 ) {
//          ypObject *item = yp_miniiter_next( mi, &mi_state );
//          if( yp_isexceptionC2( item, yp_StopIteration ) ) break;
//          // ... operate on item ...
//          yp_decref( item );
//      }
//      yp_decref( mi );

// TODO mini iterators for yp_iter_values, yp_iter_items, etc

// Returns a new reference to a mini iterator for object x and initializes *state to the iterator's
// starting state.  *state is opaque: you must *not* modify it directly.  It is usually unsafe to
// modify an object being iterated over.
ypAPI ypObject *yp_miniiter( ypObject *x, yp_uint64_t *state );

// Returns a new reference to the next yielded value from the mini iterator, yp_StopIteration if
// the iterator is exhausted, or another exception.  state must point to the same location used in
// the yp_miniiter call that returned mi; *state will be modified.
ypAPI ypObject *yp_miniiter_next( ypObject *mi, yp_uint64_t *state );

// Returns a hint as to how many items are left to be yielded.  See yp_iter_lenhintC for additional
// information.  Returns zero and sets *exc on error.
ypAPI yp_ssize_t yp_miniiter_lenhintC( ypObject *mi, yp_uint64_t *state, ypObject **exc );


/*
 * C-to-C Container Operations
 */

// When working with containers, it can be convenient to perform operations using C types rather
// than dealing with reference counting and short-lived objects.  These functions provide shortcuts
// for common operations when dealing with containers.  Keep in mind, though, that many of these
// functions create short-lived objects, so excessive use may impact execution time.

// For functions that deal with strings, if encoding is missing yp_s_utf_8 (which is compatible
// with ascii) is assumed, while if errors is missing yp_s_strict is assumed.  yp_*_containsC
// returns false and sets *exc on exception.

// Operations on containers that map objects to integers
ypAPI int yp_o2i_containsC( ypObject *container, yp_int_t x, ypObject **exc );
ypAPI void yp_o2i_pushC( ypObject **container, yp_int_t x );
ypAPI yp_int_t yp_o2i_popC( ypObject **container, ypObject **exc );
ypAPI yp_int_t yp_o2i_getitemC( ypObject *container, ypObject *key, ypObject **exc );
ypAPI void yp_o2i_setitemC( ypObject **container, ypObject *key, yp_int_t x );

// Operations on containers that map objects to strings
// yp_o2s_getitemCX is documented below, and must be used carefully!
ypAPI void yp_o2s_setitemC4( ypObject **container, ypObject *key,
        const yp_uint8_t *x, yp_ssize_t x_len );

// Operations on containers that map integers to objects.  Note that if the container is known
// at compile-time to be a list, then yp_getindexC et al are better choices.
ypAPI ypObject *yp_i2o_getitemC( ypObject *container, yp_int_t key );
ypAPI void yp_i2o_setitemC( ypObject **container, yp_int_t key, ypObject *x );

// Operations on containers that map strings to objects.  Note that if the value of the string is
// known at compile-time, as in:
//      value = yp_s2o_getitemC3( o, "mykey", -1 );
// it is more-efficient to use yp_IMMORTAL_STR_LATIN1 (also compatible with ascii), as in:
//      yp_IMMORTAL_STR_LATIN1( s_mykey, "mykey" );
//      value = yp_getitem( o, s_mykey );
ypAPI ypObject *yp_s2o_getitemC3( ypObject *container, const yp_uint8_t *key, yp_ssize_t key_len );
ypAPI void yp_s2o_setitemC4( ypObject **container, const yp_uint8_t *key, yp_ssize_t key_len,
        ypObject *x );

// Operations on containers that map integers to integers
ypAPI yp_int_t yp_i2i_getitemC( ypObject *container, yp_int_t key, ypObject **exc );
ypAPI void yp_i2i_setitemC( ypObject **container, yp_int_t key, yp_int_t x );

// Operations on containers that map strings to integers
ypAPI yp_int_t yp_s2i_getitemC3( ypObject *container, const yp_uint8_t *key, yp_ssize_t key_len,
        ypObject **exc );
ypAPI void yp_s2i_setitemC4( ypObject **container, const yp_uint8_t *key, yp_ssize_t key_len,
        yp_int_t x );


/*
 * Immortal "Constructors"
 */

// Defines an immortal int constant at compile-time, which can be accessed by the variable name,
// which is of type "ypObject * const".  value is a C integer literal.  To be used as:
//      yp_IMMORTAL_INT( name, value );

// Defines an immortal bytes constant at compile-time, which can be accessed by the variable name,
// which is of type "ypObject * const".  value is a C string literal that can contain null bytes.
// The length is calculated while compiling; the hash will be calculated the first time it is
// accessed.  To be used as:
//      yp_IMMORTAL_BYTES( name, value );

// Defines an immortal str constant at compile-time, which can be accessed by the variable name,
// which is of type "ypObject * const".  value is a latin-1 encoded C string literal that can
// contain null characters.  The length is calculated while compiling; the hash will be calculated
// the first time it is accessed.  Note that latin-1 is compatible with an ascii-encoded value.
//      yp_IMMORTAL_STR_LATIN1( name, value );


/*
 * Exceptions
 */

// All exception objects are immortal and, as such, do not need to be yp_decref'ed if returned.

// The exception objects that have direct Python counterparts.
ypAPI ypObject * const yp_BaseException;
ypAPI ypObject * const yp_Exception;
ypAPI ypObject * const yp_StopIteration;
ypAPI ypObject * const yp_GeneratorExit;
ypAPI ypObject * const yp_ArithmeticError;
ypAPI ypObject * const yp_LookupError;
ypAPI ypObject * const yp_AssertionError;
ypAPI ypObject * const yp_AttributeError;
ypAPI ypObject * const yp_EOFError;
ypAPI ypObject * const yp_FloatingPointError;
ypAPI ypObject * const yp_OSError;
ypAPI ypObject * const yp_ImportError;
ypAPI ypObject * const yp_IndexError;
ypAPI ypObject * const yp_KeyError;
ypAPI ypObject * const yp_KeyboardInterrupt;
ypAPI ypObject * const yp_MemoryError;
ypAPI ypObject * const yp_NameError;
ypAPI ypObject * const yp_OverflowError;
ypAPI ypObject * const yp_RuntimeError;
ypAPI ypObject * const yp_NotImplementedError;
ypAPI ypObject * const yp_ReferenceError;
ypAPI ypObject * const yp_SystemError;
ypAPI ypObject * const yp_SystemExit;
ypAPI ypObject * const yp_TypeError;
ypAPI ypObject * const yp_UnboundLocalError;
ypAPI ypObject * const yp_UnicodeError;
ypAPI ypObject * const yp_UnicodeEncodeError;
ypAPI ypObject * const yp_UnicodeDecodeError;
ypAPI ypObject * const yp_UnicodeTranslateError;
ypAPI ypObject * const yp_ValueError;
ypAPI ypObject * const yp_ZeroDivisionError;
ypAPI ypObject * const yp_BufferError;

// Raised when the object does not support the given method; subexception of yp_AttributeError
ypAPI ypObject * const yp_MethodError;
// Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError
ypAPI ypObject * const yp_SystemLimitationError;
// Raised when an invalidated object is passed to a function; subexception of yp_TypeError
ypAPI ypObject * const yp_InvalidatedError;

// Returns true (non-zero) if x is an exception that matches exc, else false.  This takes into
// account the exception heirarchy, so is the preferred method of testing for specific exceptions.
// Always succeeds.
ypAPI int yp_isexceptionC2( ypObject *x, ypObject *exc );

// A convenience function to compare x against n possible exceptions.  Returns false if n is zero.
ypAPI int yp_isexceptionCN( ypObject *x, int n, ... );


/*
 * Initialization Parameters
 */

// yp_initialize accepts a number of parameters to customize nohtyP behaviour
// TODO Actually provide "a number of parameters"
typedef struct _yp_initialize_kwparams {
    yp_ssize_t struct_size;     // set to sizeof( yp_initialize_kwparams )
} yp_initialize_kwparams;


/*
 * Direct Object Memory Access
 */

// XXX The "X" in these names is a reminder that the function is returning internal memory, and
// as such should be used with caution.

// For sequences that store their elements as an array of bytes (bytes and bytearray), sets *bytes
// to the beginning of that array, *len to the length of the sequence, and returns the immortal
// yp_None.  *bytes will point into internal object memory which MUST NOT be modified; furthermore,
// the sequence itself must not be modified while using the array.  As a special case, if len is
// NULL, the sequence must not contain null bytes and *bytes will point to a null-terminated array.
// Sets *bytes to NULL, *len to zero (if len is not NULL), and returns an exception on error.
ypAPI ypObject *yp_asbytesCX( ypObject *seq, const yp_uint8_t * *bytes, yp_ssize_t *len );

// str and chrarrays internally store their Unicode characters in particular encodings, usually
// depending on the contents of the string.  This function sets *encoded to the beginning of that
// data, *size to the number of bytes in encoded, and *encoding to the immortal str representing
// the encoding used (yp_s_latin_1, perhaps).  *encoding will point into internal object memory
// which MUST NOT be modified; furthermore, the string itself must not be modified while using the
// array.  As a special case, if size is NULL, the string must not contain null characters and
// *encoded will point to a null-terminated string.  On error, sets *encoded to NULL, *size to
// zero (if size is not NULL), *encoding to the exception, and returns the exception.
ypAPI ypObject *yp_asencodedCX( ypObject *seq, const yp_uint8_t * *encoded, yp_ssize_t *size,
        ypObject * *encoding );

// For sequences that store their elements as an array of pointers to ypObjects (list and tuple),
// sets *array to the beginning of that array, *len to the length of the sequence, and returns the
// immortal yp_None.  *array will point into internal object memory, so they are *borrowed*
// references and MUST NOT be replaced; furthermore, the sequence itself must not be modified while
// using the array.  Sets *array to NULL, *len to zero, and returns an exception on error.
ypAPI ypObject *yp_itemarrayX( ypObject *seq, ypObject * const * *array, yp_ssize_t *len );

// For tuples, lists, dicts, and frozendicts, this is equivalent to:
//  yp_asencodedCX( yp_getitem( container, key ), encoded, size, encoding )
// For all other types, this raises yp_TypeError, and sets the outputs accordingly.  *encoding 
// will point into internal object memory which MUST NOT be modified; furthermore, the string
// itself must neither be modified nor removed from the container while using the array.
ypAPI ypObject *yp_o2s_getitemCX( ypObject *container, ypObject *key, const yp_uint8_t * *encoded,
        yp_ssize_t *size, ypObject * *encoding );


/*
 * Optional Macros
 *
 * These macros may make working with nohtyP easier, but are not required.  They are best described
 * by the nohtyP examples, but are documented below.  The implementations of these macros are
 * considered internal; you'll find them near the end of this header.
 */

#ifdef yp_FUTURE
// yp_IF: A series of macros to emulate an if/elif/else with exception handling.  To be used
// strictly as follows (including braces):
//      yp_IF( condition1 ) {
//        // branch1
//      } yp_ELIF( condition2 ) {   // optional; multiple yp_ELIFs allowed
//          // branch2
//      } yp_ELSE {                 // optional
//          // else-branch
//      } yp_ELSE_EXCEPT_AS( e ) {  // optional; can also use yp_ELSE_EXCEPT
//          // exception-branch
//      } yp_ENDIF
// C's return statement works as you'd expect.
// As in Python, a condition is only evaluated if previous conditions evaluated false and did not
// raise an exception, the exception-branch is executed if any evaluated condition raises an
// exception, and the exception variable is only set if an exception occurs.  Unlike Python,
// exceptions in the chosen branch do not trigger the exception-branch, and the exception variable
// is not cleared at the end of the exception-branch.  If a condition creates a new reference that
// must be discarded, use yp_IFd and/or yp_ELIFd ("d" stands for "discard" or "decref"):
//      yp_IFd( yp_getitem( a, key ) )
#endif

#ifdef yp_FUTURE
// yp_WHILE: A series of macros to emulate a while/else with exception handling.  To be used
// strictly as follows (including braces):
//      yp_WHILE( condition ) {
//          // suite
//      } yp_WHILE_ELSE {           // optional
//          // else-suite
//      } yp_WHILE_EXCEPT_AS( e ) { // optional; can also use yp_WHILE_EXCEPT
//          // exception-suite
//      } yp_ENDWHILE
// C's break, continue, and return statements work as you'd expect.
// As in Python, the condition is evaluated multiple times until:
//  - it evaluates to false, in which case the else-suite is executed
//  - a break statement, in which case neither the else- nor exception-suites are executed
//  - an exception occurs in condition, in which case e is set to the exception and the
//  exception-suite is executed
// Unlike Python, exceptions in the suites do not trigger the exception-suite, and the exception
// variable is not cleared at the end of the exception-suite.
// If condition creates a new reference that must be discarded, use yp_WHILEd ("d" stands for
// "discard" or "decref"):
//      yp_WHILEd( yp_getindexC( a, -1 ) )
#endif

#ifdef yp_FUTURE
// yp_FOR: A series of macros to emulate a for/else with exception handling.  To be used strictly
// as follows (including braces):
//      yp_FOR( x, expression ) {
//          // suite
//      } yp_FOR_ELSE {             // optional
//          // else-suite
//      } yp_FOR_EXCEPT_AS( e ) {   // optional; can also use yp_FOR_EXCEPT
//          // exception-suite
//      } yp_ENDFOR
// C's break and continue statements work as you'd expect.  XXX However, using C's return statement
// directly in a yp_FOR will cause reference leaks, so do not use it.
// TODO Come up with a viable alternative for the return statement (the trouble is if you want to
// return a new reference to x...it'll be decref'd before you return it)
// As in Python, the expression is evaluated once to create an iterator, then the suite is executed
// once with each successfully-yielded value assigned to x (which can be reassigned within the
// suite).  This occurs until:
//  - the iterator returns yp_StopIteration, in which case else-suite is executed (but *not* the
//  exception-suite)
//  - a break statement, in which case neither the else- nor exception-suites are executed
//  - an exception occurs in expression or the iterator, in which case e is set to the exception
//  and the exception-suite is executed
// Unlike Python, there is no automatic tuple unpacking, exceptions in the suites do not trigger
// the exception-suite, and the exception variable is not cleared at the end of the
// exception-suite.
// Be careful with references.  While the internal iterator object is automatically discarded, if
// the expression itself creates a new reference, use yp_FORd ("d" stands for "discard" or
// "decref"):
//      yp_FORd( x, yp_tupleN( a, b, c ) )
// Also, the yielded values assigned to x are borrowed: they are automatically discarded at the end
// of every suite.  If you want to retain a reference to them, you'll need to call yp_incref
// yourself:
//      yp_FOR( x, values ) {
//          if( yp_ge( x, minValue ) ) {
//              yp_incref( x );
//              break;
//          }
//      } yp_ENDFOR
// FIXME re-think the auto-decref of yielded values by yp_ENDFOR...quite common to loop to try to
// find a value, having to incref it all the time can be cumbersome.  Might also be a good time to
// rethink other borrowed values in this api.
#endif

#ifdef yp_FUTURE
// yp: A set of macros to make nohtyP function calls look more like Python operators and method
// calls.  Best explained with examples:
//  a.append( b )           --> yp( a,append, b )               --> yp_append( a, b )
//  a + b                   --> yp( a, add, b )                 --> yp_add( a, b )
// For methods that take no arguments, use yp0 (unlike elsewhere, the postfix counts the number of
// arguments to the equivalent Python method):
//  a.isspace( )            --> yp0( a,isspace )                --> yp_isspace( a )
// If variadic macros are supported by your compiler, yp can take multiple arguments:
//  a.setdefault( b, c )    --> yp( &a,setdefault, b, c )       --> yp_setdefault( &a, b, c )
//  a.startswith( b, 2, 7 ) --> yp( a,startswith4, b, 2, 7 )    --> yp_startswith4( a, b, 2, 7 )
// If variadic macros are not supported, use yp2, yp3, etc (note how the 4 in startswith4 is 1 plus
// the 3 in yp3):
//  a.setdefault( b, c )    --> yp2( &a,setdefault, b, c )      --> yp_setdefault( &a, b, c )
//  a.startswith( b, 2, 7 ) --> yp3( a,startswith4, b, 2, 7 )   --> yp_startswith4( a, b, 2, 7 )
#endif

// TODO A macro to get exception info as a string, include file/line info of the place the macro is
// checked


/*
 * Internals  XXX Do not use directly!
 */

// This structure is likely to change in future versions; it should only exist in-memory
// TODO what do we gain by caching the hash?  We already jump through hoops to use the hash
// stored in the hash table where possible.
// TODO Is there a way to reduce the size of type+refcnt+len+alloclen to 64 bits, without hitting
// potential performance issues?
// TODO Do like Python and have just type+refcnt for non-containers
struct _ypObject {
    yp_uint32_t ob_type;        // type code
    yp_uint32_t ob_refcnt;      // reference count
    yp_int32_t  ob_len;         // length of object
    yp_int32_t  ob_alloclen;    // allocated length
    yp_hash_t   ob_hash;        // cached hash for immutables
    void *      ob_data;        // pointer to object data
    // Note that we are 8-byte aligned here on both 32- and 64-bit systems
};

// ypObject_HEAD defines the initial segment of every ypObject
#define _ypObject_HEAD \
    ypObject ob_base;
// Declares the ob_inline_data array for container object structures
#define _yp_INLINE_DATA( elemType ) \
    elemType ob_inline_data[1]

// These structures are likely to change in future versions; they should only exist in-memory
struct _ypIntObject {
    _ypObject_HEAD
    yp_int_t value;
};
struct _ypBytesObject {
    _ypObject_HEAD
    _yp_INLINE_DATA( yp_uint8_t );
};
struct _ypStrObject {
    _ypObject_HEAD
    _yp_INLINE_DATA( yp_uint8_t );
};

// Set ob_refcnt to this value for immortal objects
#define _ypObject_REFCNT_IMMORTAL   (0x7FFFFFFFu)
// Set ob_hash to this value for uninitialized hashes (tp_hash will be called and ob_hash updated)
#define _ypObject_HASH_INVALID      ((yp_hash_t) -1)

// These type codes must match those in nohtyP.c
#define _ypInt_CODE                  ( 10u)
#define _ypBytes_CODE                ( 16u)
#define _ypStr_CODE                  ( 18u)

// "Constructors" for immortal objects; implementation considered "internal", documentation above
#define _yp_IMMORTAL_HEAD_INIT( type, data, len ) \
    { type, _ypObject_REFCNT_IMMORTAL, \
      len, 0, _ypObject_HASH_INVALID, data }
#define yp_IMMORTAL_INT( name, value ) \
    static struct _ypIntObject _ ## name ## _struct = { _yp_IMMORTAL_HEAD_INIT( \
        _ypInt_CODE, NULL, 0 ), (value) }; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */
#define yp_IMMORTAL_BYTES( name, value ) \
    static const char _ ## name ## _data[] = value; \
    static struct _ypBytesObject _ ## name ## _struct = { _yp_IMMORTAL_HEAD_INIT( \
        _ypBytes_CODE, (void *) _ ## name ## _data, sizeof( _ ## name ## _data )-1 ) }; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */
#define yp_IMMORTAL_STR_LATIN1( name, value ) \
    static const char _ ## name ## _data[] = value; \
    static struct _ypStrObject _ ## name ## _struct = { _yp_IMMORTAL_HEAD_INIT( \
        _ypStr_CODE, (void *) _ ## name ## _data, sizeof( _ ## name ## _data )-1 ) }; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */
// TODO yp_IMMORTAL_TUPLE


#ifdef yp_FUTURE
// The implementation of yp_IF is considered "internal"; see above for documentation
#define yp_IF( expression ) { \
    ypObject *_yp_IF_expr; \
    ypObject *_yp_IF_cond; \
    { \
        _yp_IF_expr = (expression); \
        _yp_IF_cond = yp_bool( _yp_IF_expr ); \
        if( _yp_IF_cond == yp_True )
#define yp_IFd( expression ) { \
    ypObject *_yp_IF_expr; \
    ypObject *_yp_IF_cond; \
    { \
        _yp_IF_expr = (expression); \
        _yp_IF_cond = yp_bool( _yp_IF_expr ); \
        yp_decref( _yp_IF_expr ); \
        if( _yp_IF_cond == yp_True )
#define yp_ELIF( expression ) \
    } if( _yp_IF_cond == yp_False ) { \
        _yp_IF_expr = (expression); \
        _yp_IF_cond = yp_bool( _yp_IF_expr ); \
        if( _yp_IF_cond == yp_True )
#define yp_ELIFd( expression ) \
    } if( _yp_IF_cond == yp_False ) { \
        _yp_IF_expr = (expression); \
        _yp_IF_cond = yp_bool( _yp_IF_expr ); \
        yp_decref( _yp_IF_expr ); \
        if( _yp_IF_cond == yp_True )
#define yp_ELSE \
    } if( _yp_IF_cond == yp_False ) {
#define yp_ELSE_EXCEPT \
    } if( yp_isexceptionC( _yp_IF_cond ) ) {
#define yp_ELSE_EXCEPT_AS( target ) \
    } if( yp_isexceptionC( _yp_IF_cond ) ) { \
        target = _yp_IF_cond;
#define yp_ENDIF \
    } }
#endif

#ifdef yp_FUTURE
// The implementation of yp_WHILE is considered "internal"; see above for documentation
#define yp_WHILE( expression ) { \
    ypObject *_yp_WHILE_expr; \
    ypObject *_yp_WHILE_cond; \
    while( _yp_WHILE_expr = (expression), \
           _yp_WHILE_cond = yp_bool( _yp_WHILE_expr ), \
           _yp_WHILE_cond == yp_True )
#define yp_WHILEd( expression ) { \
    ypObject *_yp_WHILE_expr; \
    ypObject *_yp_WHILE_cond; \
    while( _yp_WHILE_expr = (expression), \
           _yp_WHILE_cond = yp_bool( _yp_WHILE_expr ), \
           yp_decref( _yp_WHILE_expr ), \
           _yp_WHILE_cond == yp_True )
#define yp_WHILE_ELSE \
    if( _yp_WHILE_cond == yp_False )
#define yp_WHILE_EXCEPT \
    if( yp_isexceptionC( _yp_WHILE_cond ) )
#define yp_WHILE_EXCEPT_AS( target ) \
    if( yp_isexceptionC( _yp_WHILE_cond ) && (target = _yp_WHILE_cond) )
#define yp_ENDWHILE \
    }
#endif

#ifdef yp_FUTURE
// The implementation of yp_FOR is considered "internal"; see above for documentation
// TODO Can we update this for yp_miniiter?
#define yp_FOR( target, expression ) { \
    ypObject *_yp_FOR_expr = (expression); \
    ypObject *_yp_FOR_iter = yp_iter( _yp_FOR_expr ); \
    ypObject *_yp_FOR_item; \
    for( _yp_FOR_item = yp_next( _yp_FOR_iter ); \
         !yp_isexceptionC( _yp_FOR_item ) && (target = _yp_FOR_item); \
         yp_decref( _yp_FOR_item ), _yp_FOR_item = yp_next( _yp_FOR_iter ) )
#define yp_FORd( target, expression ) { \
    ypObject *_yp_FOR_expr = (expression); \
    ypObject *_yp_FOR_iter = yp_iter( _yp_FOR_expr ); \
    ypObject *_yp_FOR_item; \
    yp_decref( _yp_FOR_expr ); \
    for( _yp_FOR_item = yp_next( _yp_FOR_iter ); \
         !yp_isexceptionC( _yp_FOR_item ) && (target = _yp_FOR_item); \
         yp_decref( _yp_FOR_item ), _yp_FOR_item = yp_next( _yp_FOR_iter ) )
#define yp_FOR_ELSE \
    if( yp_isexceptionC2( _yp_FOR_item, yp_StopIteration ) )
#define yp_FOR_EXCEPT \
    if( yp_isexceptionC( _yp_FOR_item ) && \
        !yp_isexceptionC2( _yp_FOR_item, yp_StopIteration ) )
#define yp_FOR_EXCEPT_AS( target ) \
    if( yp_isexceptionC( _yp_FOR_item ) && \
        !yp_isexceptionC2( _yp_FOR_item, yp_StopIteration ) && \
        (target = _yp_FOR_item) )
#define yp_ENDFOR \
    yp_decref( _yp_FOR_item ); \
    yp_decref( _yp_FOR_iter ); \
}
#endif

#ifdef yp_FUTURE
// The implementation of "yp" is considered "internal"; see above for documentation
#define yp0( self, method )         yp_ ## method( self )
#define yp1( self, method, a1 )     yp_ ## method( self, a1 )
#ifdef yp_NO_VARIADIC_MACROS
#define yp yp1
#else
#define yp( self, method, ... )     yp_ ## method( self, _VA_ARGS_ )
#endif
#define yp2( self, method, a1, a2 ) yp_ ## method( self, a1, a2 )
#define yp3( self, method, a1, a2, a3 ) yp_ ## method( self, a1, a2, a3 )
#define yp4( self, method, a1, a2, a3, a4 ) yp_ ## method( self, a1, a2, a3, a4 )
#endif

#ifdef __cplusplus
}
#endif
#endif // yp_NOHTYP_H


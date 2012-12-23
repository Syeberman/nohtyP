/*
 * nohtyP.h - A Python-like API for C, in one .c and one .h
 *      http://nohtyp.wordpress.com
 *      Copyright © 2001-2012 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/py3k/license.html
 *
 * The goal of nohtyP is to enable Python-like code to be written in C.  It is patterned after
 * Python's built-in API, then adjusted for expected usage patterns.  It also borrows ideas from
 * Python's own C API.  To be as portable as possible, it is written in one .c and one .h file and
 * attempts to rely strictly on standard C.
 *
 * Most functions borrow inputs, create their own references, and output new references.  One
 * exception: those that modify objects steal a reference to the modified object and return a
 * new reference; this may not be the same object, particularly if an exception occurs.  (You can
 * always call yp_incref first to ensure the object is not deallocated.)
 *
 * When an error occurs in a function, it returns an exception object (after ensuring all objects
 * are left in a consistent state).  If the function is modifying an object, that object is
 * discarded and replaced with the exception.  If an exception object is used for any input into a
 * function, it is returned before any modifications occur.  Exception objects are immortal, so it
 * isn't necessary to call yp_decref on them.  As a result of these rules, it is possible to
 * string together multiple function calls and only check if an exeption occured at the end:
 *      yp_IMMORTAL_BYTES( sep, ", " );
 *      yp_IMMORTAL_BYTES( fmt, "(%s)\n" );
 *      ypObject *sepList = yp_join( sep, list );
 *      // if yp_join failed, sepList could be an exception
 *      ypObject *result = yp_format( fmt, sepList );
 *      yp_decref( sepList );
 *      if( yp_isexception( result ) ) exit( -1 );
 * Unless explicitly documented as "always succeeds", _any_ function can return an exception.
 *
 * This API is threadsafe so long as no objects are modified while being accessed by multiple
 * threads; this includes updating reference counts, so immutables are not inherently threadsafe!
 * One strategy to ensure safety is to deep copy objects before exchanging between threads.
 * Sharing immutable, immortal objects is always safe.
 *
 * Other important postfixes:
 *  C - C native types are accepted and returned where appropriate
 *  F - A version of "C" that accepts floats in place of ints
 *  L - Library routines that operate strictly on C types
 *  N - n variable positional arguments follow
 *  K - n key/value arguments follow (for a total of n*2 arguments)
 *  V - A version of "N" or "K" that accepts a va_list in place of ...
 *  E - Errors modifying an object do not discard the object (exceptions are returned instead)
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

#define ypAPI __declspec( dllexport )


/*
 * Initialization
 */

// Must be called once before any other function; subsequent calls are a no-op.  If a fatal error
// occurs abort() is called.
ypAPI void yp_initialize( void );


/*
 * Object Fundamentals
 */

// All objects maintain a reference count; when that count reaches zero, the object is deallocated.
// Certain objects are immortal and, thus, never deallocated; examples include yp_None, yp_True,
// yp_False, exceptions, and so forth.

// nohtyP objects are only accessed through pointers to ypObject.
typedef struct _ypObject ypObject;

// The null-reference object.
ypAPI ypObject *yp_None;

// Increments the reference count of x, returning it as a convenience.  Always succeeds; if x is
// immortal this is a no-op.
ypAPI ypObject *yp_incref( ypObject *x );

// A convenience function to increment the references of n objects.
void yp_increfN( int n, ... );
void yp_increfV( int n, va_list args );

// Decrements the reference count of x, deallocating it if the count reaches zero.  Always
// succeeds; if x is immortal this is a no-op.
void yp_decref( ypObject *x );

// A convenience function to decrement the references of n objects.
void yp_decrefN( int n, ... );
void yp_decrefV( int n, va_list args );

// Returns true (non-zero) if x is an exception, else false.  Always succeeds.
int yp_isexceptionC( ypObject *x );


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
void yp_freeze( ypObject **x );

// Freezes *x and, recursively, all contained objects.
void yp_deepfreeze( ypObject **x );

// Returns a new reference to a mutable shallow copy of x.  If x has no associated mutable type an
// immutable copy is returned.  May also return yp_MemoryError.
ypObject *yp_unfrozen_copy( ypObject *x );

// Creates a mutable copy of x and, recursively, all contained objects, returning a new reference.
ypObject *yp_unfrozen_deepcopy( ypObject *x );

// Returns a new reference to an immutable shallow copy of x.  If x has no associated immutable
// type a new, invalidated object is returned.  May also return yp_MemoryError.
ypObject *yp_frozen_copy( ypObject *x );

// Creates an immutable copy of x and, recursively, all contained objects, returning a new
// reference.
ypObject *yp_frozen_deepcopy( ypObject *x );

// Returns a new reference to an exact, shallow copy of x.  May also return yp_MemoryError.
ypObject *yp_copy( ypObject *x );

// Creates an exact copy of x and, recursively, all contained objects, returning a new reference.
ypObject *yp_deepcopy( ypObject *x );

// Discards all contained objects in *x, deallocates _some_ memory, and transmutes it to the
// ypInvalidated type (rendering the object useless).  If *x is immortal or already invalidated
// this is a no-op; immutable objects _can_ be invalidated.
void yp_invalidate( ypObject **x );

// Invalidates *x and, recursively, all contained objects.
void yp_deepinvalidate( ypObject **x );


/*
 * Boolean Operations and Comparisons
 */

// There are exactly two boolean values: yp_True and yp_False.
ypObject *yp_True;
ypObject *yp_False;

// Returns the immortal yp_False if the object should be considered false (yp_None, a number equal
// to zero, or a container of zero length), otherwise yp_True.
ypObject *yp_bool( ypObject *x );

// Returns the immortal yp_True if x is considered false, otherwise yp_False.
ypObject *yp_not( ypObject *x );

// Returns a *new* reference to y if x is false, otherwise to x.  Unlike Python, both arguments
// are always evaluated.  You may find yp_anyN more convenient, as it returns an immortal.
ypObject *yp_or( ypObject *x, ypObject *y );

// A convenience function to "or" n objects.  Returns yp_False if n is zero, and the first object
// if n is one.  Returns a *new* reference; you may find yp_anyN more convenient, as it returns an
// immortal.
ypObject *yp_orN( int n, ... );
ypObject *yp_orV( int n, va_list args );

// Equivalent to yp_bool( yp_orN( n, ... ) ).  (Returns an immortal.)
ypObject *yp_anyN( int n, ... );
ypObject *yp_anyV( int n, va_list args );

// Returns the immortal yp_True if any element of iterable is true; if the iterable is empty,
// returns yp_False.  Stops iterating at the first true element.
ypObject *yp_any( ypObject *iterable );

// Returns a *new* reference to x if x is false, otherwise to y.  Unlike Python, both
// arguments are always evaluated.  You may find yp_allN more convenient, as it returns an 
// immortal.
ypObject *yp_and( ypObject *x, ypObject *y );

// A convenience function to "and" n objects.  Returns yp_True if n is zero, and the first object
// if n is one.  Returns a *new* reference; you may find yp_allN more convenient, as it returns an
// immortal.
ypObject *yp_andN( int n, ... );
ypObject *yp_andV( int n, va_list args );

// Equivalent to yp_bool( yp_andN( n, ... ) ).  (Returns an immortal.)
ypObject *yp_allN( int n, ... );
ypObject *yp_allV( int n, va_list args );

// Returns the immortal yp_True if all elements of iterable are true or the iterable is empty.
// Stops iterating at the first false element.
ypObject *yp_all( ypObject *iterable );

// Implements the "less than" (x<y), "less than or equal" (x<=y), "equal" (x==y), "not equal"
// (x!=y), "greater than or equal" (x>=y), and "greater than" (x>y) comparisons.  Returns the
// immortal yp_True if the condition is true, otherwise yp_False.
ypObject *yp_lt( ypObject *x, ypObject *y );
ypObject *yp_le( ypObject *x, ypObject *y );
ypObject *yp_eq( ypObject *x, ypObject *y );
ypObject *yp_ne( ypObject *x, ypObject *y );
ypObject *yp_ge( ypObject *x, ypObject *y );
ypObject *yp_gt( ypObject *x, ypObject *y );

// You may also be interested in yp_IF and yp_WHILE for working with boolean operations; see below.


/*
 * nohtyP's C types
 */

// Fixed-size numeric C types
typedef signed char	        yp_int8_t;
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
typedef yp_float64_t    yp_float_t;

// The signature of a function that can be wrapped up in a generator, called by yp_send and
// similar functions.  self is the iterator object; use yp_iter_stateX to retrieve any state
// variables.  value is the object that is sent into the function by yp_send; it may also be
// yp_GeneratorExit if yp_close is called, or another exception.  The return value must be a new
// reference, yp_StopIteration if the generator is exhausted, or another exception.
typedef ypObject *(*yp_generator_func_t)( ypObject *self, ypObject *value );


/*
 * Constructors
 */

// Unlike Python, most nohtyP types have both mutable and immutable versions.  An "intstore" is a
// mutable int (it "stores" an int); similar for floatstore.  The mutable str is called a
// "characterarray", while a "frozendict" is an immutable dict.  There are no useful immutable 
// types for iters or files: attempting to freeze such types will close them.

// Returns a new reference to an int/intstore with the given value.
ypObject *yp_intC( yp_int_t value );
ypObject *yp_intstoreC( yp_int_t value );

// Returns a new reference to an int/intstore interpreting the C string as an integer literal with
// the given base.  Base zero means to infer the base according to Python's syntax.
ypObject *yp_int_strC( const char *string, int base );
ypObject *yp_intstore_strC( const char *string, int base );

// Returns a new reference to a float/floatstore with the given value.
ypObject *yp_floatC( yp_float_t value );
ypObject *yp_floatstoreC( yp_float_t value );

// Returns a new reference to a float/floatstore interpreting the string as a Python
// floating-point literal.
ypObject *yp_float_strC( const char *string );
ypObject *yp_floatstore_strC( const char *string );

// Returns a new reference to an iterator for object x.
ypObject *yp_iter( ypObject *x );

// Returns a new reference to a generator-iterator object using the given func.  The function will
// be passed the given n objects as state on each call (the objects will form an array).  lenhint
// is a clue to consumers of the generator how many items will be yielded; use zero if this is not
// known.
ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );
ypObject *yp_generatorCV( yp_generator_func_t func, yp_ssize_t lenhint, int n, va_list args );

// Returns a new reference to a range object. yp_rangeC is equivalent to yp_rangeC3( 0, stop, 1 ).
ypObject *yp_rangeC3( yp_int_t start, yp_int_t stop, yp_int_t step );
ypObject *yp_rangeC( yp_int_t stop );

// Returns a new reference to a bytes/bytearray, copying the first len bytes from source.  If
// source is NULL it is considered as having all null bytes; if len is negative source is
// considered null terminated (and, therefore, will not contain the null byte).
//  Ex: pre-allocate a bytearray of length 50: yp_bytearrayC( NULL, 50 )
ypObject *yp_bytesC( const yp_uint8_t *source, yp_ssize_t len );
ypObject *yp_bytearrayC( const yp_uint8_t *source, yp_ssize_t len );

// XXX The str/characterarray types will be added in a future version

// Returns a new reference to the str representing a character whose Unicode codepoint is the
// integer i.
ypObject *yp_chrC( yp_int_t i );

// Returns a new reference to a tuple/list of length n containing the given objects.
ypObject *yp_tupleN( int n, ... );
ypObject *yp_tupleV( int n, va_list args );
ypObject *yp_listN( int n, ... );
ypObject *yp_listV( int n, va_list args );

// Returns a new reference to a tuple/list made from factor shallow copies of yp_tupleN( n, ... )
// concatenated.  Equivalent to "factor * (obj0, obj1, ...)" in Python.
//  Ex: pre-allocate a list of length 99: yp_list_repeatCN( 99, 1, yp_None )
//  Ex: an 8-tuple containing alternating bools: yp_tuple_repeatCN( 4, 2, yp_False, yp_True )
ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
ypObject *yp_tuple_repeatCV( yp_ssize_t factor, int n, va_list args );
ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );
ypObject *yp_list_repeatCV( yp_ssize_t factor, int n, va_list args );

// Returns a new reference to a tuple/list whose elements come from iterable.
ypObject *yp_tuple( ypObject *iterable );
ypObject *yp_list( ypObject *iterable );

// Returns a new reference to a sorted list from the items in iterable.  key is a function that
// returns new or immortal references that are used as comparison keys; to compare the elements
// directly, use NULL.  If reverse is true, the list elements are sorted as if each comparison were
// reversed.
typedef ypObject *(*yp_sort_key_func_t)( ypObject *x );
ypObject *yp_sorted3( ypObject *iterable, yp_sort_key_func_t key, ypObject *reverse );

// Equivalent to yp_sorted3( iterable, NULL, yp_False ).
ypObject *yp_sorted( ypObject *iterable );

// Returns a new reference to a frozenset/set containing the given n objects; the length will be n,
// unless there are duplicate objects.
ypObject *yp_frozensetN( int n, ... );
ypObject *yp_frozensetV( int n, va_list args );
ypObject *yp_setN( int n, ... );
ypObject *yp_setV( int n, va_list args );

// Returns a new reference to a frozenset/set whose elements come from iterable.
ypObject *yp_frozenset( ypObject *iterable );
ypObject *yp_set( ypObject *iterable );

// Returns a new reference to a frozendict/dict containing the given n key/value pairs (for a total
// of 2*n objects); the length will be n, unless there are duplicate keys.
//  Ex: yp_dictK( 3, key0, value0, key1, value1, key2, value2 )
ypObject *yp_frozendictK( int n, ... );
ypObject *yp_frozendictKV( int n, va_list args );
ypObject *yp_dictK( int n, ... );
ypObject *yp_dictKV( int n, va_list args );

// Returns a new reference to a frozendict/dict containing the given n keys all set to value; the
// length will be n, unless there are duplicate keys.
//  Ex: pre-allocate a dict with 3 keys: yp_dict_fromkeysN( yp_None, 3, key0, key1, key2 )
ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
ypObject *yp_frozendict_fromkeysV( ypObject *value, int n, va_list args );
ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );
ypObject *yp_dict_fromkeysV( ypObject *value, int n, va_list args );

// Returns a new reference to a frozendict/dict whose key-value pairs come from x.  x can be a
// mapping object, or an iterable that yields exactly two items at a time (ie (key, value)).
ypObject *yp_frozendict( ypObject *x );
ypObject *yp_dict( ypObject *x );

// XXX The file type will be added in a future version


/*
 * Generic Object Operations
 */

// Returns the hash value of x; x must be immutable.  Returns -1 and sets *exc on error.
yp_hash_t yp_hashC( ypObject *x, ypObject **exc );

// Returns the _current_ hash value of x; if x is mutable, this value may change between calls.
// Returns -1 and sets *exc on error.  (Unlike Python, this can calculate the hash value of mutable
// types.)
yp_hash_t yp_currenthashC( ypObject *x, ypObject **exc );


/*
 * Iterator Operations
 */

// As per Python, an "iterator" is an object that implements yp_next, while an "iterable" is an
// object that implements yp_iter.

// TODO Because these functions don't (currently) discard *iterator, should they have prefix E?

// "Sends" a value into *iterator and returns a new reference to the next yielded value,
// yp_StopIteration if the iterator is exhausted, or another exception.  The value may be ignored
// by the iterator.  If value is an exception this behaves like yp_throw.
ypObject *yp_send( ypObject **iterator, ypObject *value );

// Equivalent to yp_send( iterator, yp_None ).
ypObject *yp_next( ypObject **iterator );

// Similar to yp_next, but returns a new reference to defval when the iterator is exhausted.  The
// Python-equivalent "default" of defval is yp_StopIteration.
ypObject *yp_next2( ypObject **iterator, ypObject *defval );

// "Throws" an exception into *iterator and returns a new reference to the next yielded
// value, yp_StopIteration if the iterator is exhausted, or another exception.  exc _must_ be an
// exception.
ypObject *yp_throw( ypObject **iterator, ypObject *exc );

// Returns a hint as to how many items are left to be yielded.  The accuracy of this hint depends 
// on the underlying type: most containers know their lengths exactly, but some generators may not.
// A hint of zero could mean that the iterator is exhausted, that the length is unknown, or that
// the iterator will yield infinite values.  Returns zero and sets *exc on error.
yp_ssize_t yp_iter_lenhintC( ypObject *iterator, ypObject **exc );

// Typically only called from within yp_generator_func_t functions.  Returns the generator state
// and its size in bytes.  The structure and initial values of *state are determined by the call
// to the generator constructor; the function cannot change the size after creation, and any
// ypObject*s in *state should be considered *borrowed* (it is safe to replace them with new 
// references).  Sets *state to NULL, *size to zero, and *exc to an exception on error.
void yp_iter_stateX( ypObject *iterator, void **state, yp_ssize_t *size, ypObject **exc );

// "Closes" the iterator by calling yp_throw( iterator, yp_GeneratorExit ).  If yp_StopIteration or
// yp_GeneratorExit is returned by yp_throw, *iterator is not discarded, otherwise *iterator is
// replaced with an exception.  The behaviour of this function for other types, in particular
// files, is documented elsewhere.
void yp_close( ypObject **iterator );

// Returns a new reference to an iterator that yields values from iterable for which function
// returns true.  The given function must return new or immortal references, as each returned
// value will be discarded; to inspect the elements directly, use NULL.
typedef ypObject *(*yp_filter_function_t)( ypObject *x );
ypObject *yp_filter( yp_filter_function_t function, ypObject *iterable );

// Similar to yp_filter, but yields values for which function returns false.
ypObject *yp_filterfalse( yp_filter_function_t function, ypObject *iterable );

// Returns a new reference to the largest/smallest of the given n objects.  key is a function that
// returns new or immortal references that are used as comparison keys; to compare the elements
// directly, use NULL.
ypObject *yp_max_keyN( yp_sort_key_func_t key, int n, ... );
ypObject *yp_max_keyV( yp_sort_key_func_t key, int n, va_list args );
ypObject *yp_min_keyN( yp_sort_key_func_t key, int n, ... );
ypObject *yp_min_keyV( yp_sort_key_func_t key, int n, va_list args );

// Equivalent to yp_max_keyN( NULL, n, ... ) and yp_min_keyN( NULL, n, ... ).
ypObject *yp_maxN( int n, ... );
ypObject *yp_maxV( int n, va_list args );
ypObject *yp_minN( int n, ... );
ypObject *yp_minV( int n, va_list args );

// Returns a new reference to the largest/smallest element in iterable.  key is as in yp_max_keyN.
ypObject *yp_max_key( ypObject *iterable, yp_sort_key_func_t key );
ypObject *yp_min_key( ypObject *iterable, yp_sort_key_func_t key );

// Equivalent to yp_max_key( iterable, NULL ) and yp_min_key( iterable, NULL ).
ypObject *yp_max( ypObject *iterable );
ypObject *yp_min( ypObject *iterable );

// Returns a new reference to an iterator that yields the elements of seq in reverse order.
ypObject *yp_reversed( ypObject *seq );

// Returns a new reference to an iterator that aggregates elements from each of the n iterables.
ypObject *yp_zipN( int n, ... );
ypObject *yp_zipV( int n, va_list args );

// You may also be interested in yp_FOR for working with iterables; see below.


/*
 * Container Operations
 */

// Returns the immortal yp_True if an item of container is equal to x, else yp_False.
ypObject *yp_contains( ypObject *container, ypObject *x );
ypObject *yp_in( ypObject *x, ypObject *container );

// Returns the immortal yp_False if an item of container is equal to x, else yp_True.
ypObject *yp_not_in( ypObject *x, ypObject *container );

// Returns the length of container.  Returns zero and sets *exc on error.
yp_ssize_t yp_lenC( ypObject *container, ypObject **exc );

// Adds an item to *container.  On error, *container is discarded and set to an exception.  The 
// relation between yp_push and yp_pop depends on the type: x may be the first or last item popped,
// or items may be popped in arbitrary order.
void yp_push( ypObject **container, ypObject *x );

// Removes all items from *container.  On error, *container is discarded and set to an exception.
void yp_clear( ypObject **container );

// Removes an item from *container and returns a new reference to it.  On error, *container is 
// discarded and set to an exception _and_ an exception is returned.  (Not supported on dicts; use 
// yp_popvalue or yp_popitem instead.)
ypObject *yp_pop( ypObject **container );


/*
 * Sequence Operations
 */

// Returns a new reference to the concatenation of sequence and x.
ypObject *yp_concat( ypObject *sequence, ypObject *x );

// Returns a new reference to factor shallow copies of sequence, concatenated.
ypObject *yp_repeatC( ypObject *sequence, yp_ssize_t factor );

// Returns a new reference to the i-th item of sequence, origin zero.  Negative indicies are
// handled as in Python.
ypObject *yp_getindexC( ypObject *sequence, yp_ssize_t i );

// Returns a new reference to the slice of sequence from i to j with step k.  The Python-equivalent
// "defaults" for i and j are yp_SLICE_DEFAULT, while for k it is 1.
ypObject *yp_getsliceC4( ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

// Equivalent to yp_getindexC( sequence, yp_asssizeC( key, &exc ) ).
ypObject *yp_getitem( ypObject *sequence, ypObject *key );

// Returns the lowest index in sequence where x is found, such that x is contained in the slice
// sequence[i:j], or -1 if x is not found.  Returns -1 and sets *exc on error; *exc is _not_ set
// if x is simply not found.  As in Python, types such as tuples inspect only one item at a time,
// while types such as strs look for a particular sub-sequence of items.
yp_ssize_t yp_findC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc );

// Equivalent to yp_findC4( sequence, x, 0, yp_SLICE_USELEN, exc ).
yp_ssize_t yp_findC( ypObject *sequence, ypObject *x, ypObject **exc );

// Similar to yp_findC4 and yp_findC, except sets *exc to yp_ValueError if x is not found.
yp_ssize_t yp_indexC4( ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
        ypObject **exc );
yp_ssize_t yp_indexC( ypObject *sequence, ypObject *x, ypObject **exc );

// Returns the total number of non-overlapping occurences of x in sequence.  Returns 0 and sets
// *exc on error.
yp_ssize_t yp_countC( ypObject *sequence, ypObject *x, ypObject **exc );

// Sets the i-th item of *sequence, origin zero, to x.  Negative indicies are handled as in 
// Python.  On error, *sequence is discarded and set to an exception.
void yp_setindexC( ypObject **sequence, yp_ssize_t i, ypObject *x );

// Sets the slice of *sequence, from i to j with step k, to x.  The Python-equivalent "defaults"
// for i and j are yp_SLICE_DEFAULT, while for k it is 1.  On error, *sequence is discarded and 
// set to an exception.
void yp_setsliceC5( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *x );

// Equivalent to yp_setindexC( sequence, yp_asssizeC( key, &exc ), x ).
void yp_setitem( ypObject **sequence, ypObject *key, ypObject *x );

// Removes the i-th item from *sequence, origin zero.  Negative indicies are handled as in Python.
// On error, *sequence is discarded and set to an exception.
void yp_delindexC( ypObject **sequence, yp_ssize_t i );

// Removes the elements of the slice from *sequence, from i to j with step k.  The Python-
// equivalent "defaults" for i and j are yp_SLICE_DEFAULT, while for k it is 1.  On error, 
// *sequence is discarded and set to an exception.
void yp_delsliceC4( ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k );

// Equivalent to yp_delindexC( sequence, yp_asssizeC( key, &exc ) ).
void yp_delitem( ypObject **sequence, ypObject *key );

// Appends x to the end of *sequence.  On error, *sequence is discarded and set to an exception.
void yp_append( ypObject **sequence, ypObject *x );
void yp_push( ypObject **sequence, ypObject *x );

// Appends the contents of t to the end of *sequence.  On error, *sequence is discarded and set to
// an exception.
void yp_extend( ypObject **sequence, ypObject *t );

// Appends the contents of *sequence to itself factor-1 times; if factor is zero *sequence is
// cleared.  Equivalent to seq*=factor for lists in Python.  On error, *sequence is discarded and
// set to an exception.
void yp_irepeatC( ypObject **sequence, yp_ssize_t factor );

// Inserts x into *sequence at the index given by i; existing elements are shifted to make room.
// On error, *sequence is discarded and set to an exception.
void yp_insertC( ypObject **sequence, yp_ssize_t i, ypObject *x );

// Removes the i-th item from *sequence and returns it.  The Python-equivalent "default" for i is
// -1.  On error, *sequence is discarded and set to an exception _and_ an exception is returned.
ypObject *yp_popindexC2( ypObject **sequence, yp_ssize_t i );

// Equivalent to yp_popindexC( sequence, -1 ).  Note that for sequences, yp_push and yp_pop
// together implement a stack (last in, first out).
ypObject *yp_pop( ypObject **sequence );

// Removes the first item from *sequence that equals x.  On error, *sequence is discarded and set 
// to an exception.
void yp_remove( ypObject **sequence, ypObject *x );

// Reverses the items of *sequence in-place.  On error, *sequence is discarded and set to an 
// exception.
void yp_reverse( ypObject **sequence );

// Sorts the items of *sequence in-place.  key is a function that returns new or immortal 
// references that are used as comparison keys; to compare the elements directly, use NULL.  If 
// reverse is true, the list elements are sorted as if each comparison were reversed.  On error, 
// *sequence is discarded and set to an exception.
void yp_sort3( ypObject **sequence, yp_sort_key_func_t key, ypObject *reverse );

// Equivalent to yp_sort3( sequence, NULL, yp_False ).
void yp_sort( ypObject **sequence );

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
ypObject *yp_isdisjoint( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in set is in x, else yp_False.
ypObject *yp_issubset( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in set is in x and x has additional elements, else
// yp_False.
ypObject *yp_lt( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in x is in set, else yp_False.
ypObject *yp_issuperset( ypObject *set, ypObject *x );

// Returns the immortal yp_True if every element in x is in set and set has additional elements, 
// else yp_False.
ypObject *yp_gt( ypObject *set, ypObject *x );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// from set and all n objects.
ypObject *yp_unionN( ypObject *set, int n, ... );
ypObject *yp_unionV( ypObject *set, int n, va_list args );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// common to the set and all n objects.
ypObject *yp_intersectionN( ypObject *set, int n, ... );
ypObject *yp_intersectionV( ypObject *set, int n, va_list args );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// from set that are not in the n objects.
ypObject *yp_differenceN( ypObject *set, int n, ... );
ypObject *yp_differenceV( ypObject *set, int n, va_list args );

// Returns a new reference to a set (or frozenset if set is immutable) containing all the elements
// in either set or x but not both.
ypObject *yp_symmetric_difference( ypObject *set, ypObject *x );

// Add the elements from the n objects to *set.  On error, *set is discarded and set to an 
// exception.
void yp_updateN( ypObject **set, int n, ... );
void yp_updateV( ypObject **set, int n, va_list args );

// Removes elements from *set that are not contained in all n objects.  On error, *set is discarded
// and set to an exception.
void yp_intersection_updateN( ypObject **set, int n, ... );
void yp_intersection_updateV( ypObject **set, int n, va_list args );

// Removes elements from *set that are contained in any of the n objects.  On error, *set is 
// discarded and set to an exception.
void yp_difference_updateN( ypObject **set, int n, ... );
void yp_difference_updateV( ypObject **set, int n, va_list args );

// Removes elements from *set that are contained in x, and adds elements from x not contained in
// *set.  On error, *set is discarded and set to an exception.
void yp_symmetric_difference_update( ypObject **set, ypObject *x );

// Adds element x to *set.  On error, *set is discarded and set to an exception.  While Python calls
// this method add, yp_add is already used for "a+b", so these two equivalent aliases are provided 
// instead.
void yp_push( ypObject **set, ypObject *x );
void yp_set_add( ypObject **set, ypObject *x );

// If x is already contained in *set, returns yp_KeyError; otherwise, adds x to *set and returns
// the immortal yp_None.  Returns an exception on error; *set is never discarded.
ypObject *yp_pushuniqueE( ypObject **set, ypObject *x );

// Removes element x from *set.  Raises yp_KeyError if x is not contained in *set.  On error, 
// *set is discarded and set to an exception.
void yp_remove( ypObject **set, ypObject *x );

// Removes element x from *set if it is present.  On error, *set is discarded and set to an 
// exception.
void yp_discard( ypObject **set, ypObject *x );

// Removes an arbitrary item from *set and returns a new reference to it.  On error, *set is 
// discarded and set to an exception _and_ an exception is returned.  You cannot use the order
// of yp_push calls on sets to determine the order of yp_pop'ped elements.
ypObject *yp_pop( ypObject **set );


/*
 * Mapping Operations
 */

// Returns a new reference to the value of mapping with the given key.  Returns yp_KeyError if key 
// is not in the map.
ypObject *yp_getitem( ypObject *mapping, ypObject *key );

// Adds or replaces the value of *mapping with the given key, setting it to x.  On error, *mapping 
// is discarded and set to an exception.
void yp_setitem( ypObject **mapping, ypObject *key, ypObject *x );

// Removes the item with the given key from *mapping.  Raises yp_KeyError if key is not in 
// *mapping.  On error, *mapping is discarded and set to an exception.
void yp_delitem( ypObject **mapping, ypObject *key );

// As in Python, yp_contains, yp_in, yp_not_in, and yp_iter operate solely on a mapping's keys.

// Similar to yp_getitem, but returns a new reference to defval if key is not in the map.  The
// Python-equivalent "default" for defval is yp_None.
ypObject *yp_getdefault3( ypObject *mapping, ypObject *key, ypObject *defval );

// Returns a new reference to an iterator that yields mapping's (key, value) pairs as 2-tuples.
ypObject *yp_iter_items( ypObject *mapping );

// Returns a new reference to an iterator that yields mapping's keys.
ypObject *yp_iter_keys( ypObject *mapping );

// If key is in mapping, remove it and return a new reference to its value, else return a new
// reference to defval.  The Python-equivalent "default" of defval is yp_KeyError.  On error, 
// *mapping is discarded and set to an exception _and_ an exception is returned.  Note that yp_push
// and yp_pop are not applicable for mapping objects.
ypObject *yp_popvalue3( ypObject **mapping, ypObject *key, ypObject *defval );

// Removes an arbitrary item from *mapping and returns new references to its *key and *value.  If
// mapping is empty yp_KeyError is raised.  On error, *mapping is discarded and set to an exception
// _and_ both *key and *value are set to exceptions.
void yp_popitem( ypObject **mapping, ypObject **key, ypObject **value );

// Similar to yp_getitem, but returns a new reference to defval _and_ adds it to *mapping if key is
// not in the map.  The Python-equivalent "default" for defval is yp_None; defval _must_ _not_ be
// an exception.
ypObject *yp_setdefault3( ypObject *mapping, ypObject *key, ypObject *defval );

// TODO Complete
// XXX yp_updateN is _not_ applicable for dicts, unless the objects are allowed to be those that
// yp_dict accepts
// Also: yp_updateK (kw args), yp_update (accepting those that yp_dict accepts, but just one)

// Returns a new reference to an iterator that yields mapping's values.
ypObject *yp_iter_values( ypObject *mapping );


/*
 * Bytes & String Operations
 */

// XXX bytes- and str-specific methods will be added in a future version


/*
 * Numeric Operations
 */

// Each of these functions return new reference(s) to the result of the given numeric operation;
// for example, yp_add returns the result of adding x and y together.  If the given operands do not
// support the operation, yp_TypeError is returned.  Additional notes:
//  - yp_divmod returns two objects via *div and *mod; on error, they are both set to an exception
//  - If z is yp_None, yp_pow returns x to the power y, otherwise x to the power y modulo z
//  - To avoid confusion with the logical operators of the same name, yp_amp implements bitwise
//  and, while yp_bar implements bitwise or
//  - Unlike Python, non-numeric types do not (currently) overload these operators
ypObject *yp_add( ypObject *x, ypObject *y );
ypObject *yp_sub( ypObject *x, ypObject *y );
ypObject *yp_mul( ypObject *x, ypObject *y );
ypObject *yp_truediv( ypObject *x, ypObject *y );
ypObject *yp_floordiv( ypObject *x, ypObject *y );
ypObject *yp_mod( ypObject *x, ypObject *y );
void yp_divmod( ypObject *x, ypObject *y, ypObject **div, ypObject **mod );
ypObject *yp_pow( ypObject *x, ypObject *y, ypObject *z );
ypObject *yp_lshift( ypObject *x, ypObject *y );
ypObject *yp_rshift( ypObject *x, ypObject *y );
ypObject *yp_amp( ypObject *x, ypObject *y );
ypObject *yp_xor( ypObject *x, ypObject *y );
ypObject *yp_bar( ypObject *x, ypObject *y );
ypObject *yp_neg( ypObject *x );
ypObject *yp_pos( ypObject *x );
ypObject *yp_abs( ypObject *x );
ypObject *yp_invert( ypObject *x );

// In-place versions of the above; if the object *x can be modified to hold the result, it is,
// otherwise *x is discarded and replaced with the result.  If *x is immutable on input, an
// immutable object is returned, otherwise a mutable object is returned.  On error, *x is
// discarded and set to an exception.
// TODO Throw error if x is immutable?
void yp_iadd( ypObject **x, ypObject *y );
void yp_isub( ypObject **x, ypObject *y );
void yp_imul( ypObject **x, ypObject *y );
void yp_itruediv( ypObject **x, ypObject *y );
void yp_ifloordiv( ypObject **x, ypObject *y );
void yp_imod( ypObject **x, ypObject *y );
void yp_ipow( ypObject **x, ypObject *y, ypObject *z );
void yp_ilshift( ypObject **x, ypObject *y );
void yp_irshift( ypObject **x, ypObject *y );
void yp_iamp( ypObject **x, ypObject *y );
void yp_ixor( ypObject **x, ypObject *y );
void yp_ibar( ypObject **x, ypObject *y );
void yp_ineg( ypObject **x );
void yp_ipos( ypObject **x );
void yp_iabs( ypObject **x );
void yp_iinvert( ypObject **x );

// Versions of yp_iadd et al that accept a C integer as the second argument.  Remember that *x may
// be discarded and replaced with the result.
void yp_iaddC( ypObject **x, yp_int_t y );
// TODO: et al

// Versions of yp_iadd et al that accept a C floating-point as the second argument.  Remember that
// *x may be discarded and replaced with the result.
void yp_iaddFC( ypObject **x, yp_float_t y );
// TODO: et al

// Library routines for nohtyP integer operations on C types.  Returns a reasonable value and sets
// *exc on error; "reasonable" usually means "truncated".
yp_int_t yp_addL( yp_int_t x, yp_int_t y, ypObject **exc );
// TODO: et al

// Library routines for nohtyP floating-point operations on C types.  Returns a reasonable value
// and sets *exc on error; "reasonable" usually means "truncated".
yp_float_t yp_addFL( yp_float_t x, yp_float_t y, ypObject **exc );
// TODO: et al

// Conversion routines from C types or objects to C types.  Returns a reasonable value and sets
// *exc on error; "reasonable" usually means "truncated".  Converting a float to an int truncates
// toward zero but is not an error.
yp_int_t yp_asintC( ypObject *x, ypObject **exc );
yp_int8_t yp_asint8C( ypObject *x, ypObject **exc );
yp_uint8_t yp_asuint8C( ypObject *x, ypObject **exc );
yp_int16_t yp_asint16C( ypObject *x, ypObject **exc );
yp_uint16_t yp_asuint16C( ypObject *x, ypObject **exc );
yp_int32_t yp_asint32C( ypObject *x, ypObject **exc );
yp_uint32_t yp_asuint32C( ypObject *x, ypObject **exc );
yp_int64_t yp_asint64C( ypObject *x, ypObject **exc );
yp_uint64_t yp_asuint64C( ypObject *x, ypObject **exc );
yp_float_t yp_asfloatC( ypObject *x, ypObject **exc );
yp_float32_t yp_asfloat32C( ypObject *x, ypObject **exc );
yp_float64_t yp_asfloat64C( ypObject *x, ypObject **exc );
yp_float_t yp_asfloatL( yp_int_t x, ypObject **exc );
yp_int_t yp_asintFL( yp_float_t x, ypObject **exc );

// Return a new reference to x rounded to ndigits after the decimal point.
ypObject *yp_roundC( ypObject *x, int ndigits );

// Sums the n given objects and returns the total.
ypObject *yp_sumN( int n, ... );
ypObject *yp_sumV( int n, va_list args );

// Sums the items of iterable and returns the total.
ypObject *yp_sum( ypObject *iterable );


/*
 * Immortal "Constructors"
 */

// Defines an immortal bytes constant at compile-time, which can be accessed by the variable name,
// which is of type "static ypObject * const".  value is a C string literal that can contain null
// bytes.  The length is calculated while compiling; the hash will be calculated the first time it
// is accessed.  To be used as:
//      yp_IMMORTAL_BYTES( name, value );

// TODO Complete

/*
 * Exceptions
 */
ypAPI ypObject * yp_BaseException;
ypAPI ypObject * yp_Exception;
ypAPI ypObject * yp_StopIteration;
ypAPI ypObject * yp_GeneratorExit;
ypAPI ypObject * yp_ArithmeticError;
ypAPI ypObject * yp_LookupError;

ypAPI ypObject * yp_AssertionError;
ypAPI ypObject * yp_AttributeError;
ypAPI ypObject * yp_MethodError; // method lookup failure; "subclass" of yp_AttributeError
ypAPI ypObject * yp_EOFError;
ypAPI ypObject * yp_FloatingPointError;
ypAPI ypObject * yp_EnvironmentError;
ypAPI ypObject * yp_IOError;
ypAPI ypObject * yp_OSError;
ypAPI ypObject * yp_ImportError;
ypAPI ypObject * yp_IndexError;
ypAPI ypObject * yp_KeyError;
ypAPI ypObject * yp_KeyboardInterrupt;
ypAPI ypObject * yp_MemoryError;
ypAPI ypObject * yp_NameError;
ypAPI ypObject * yp_OverflowError;
ypAPI ypObject * yp_RuntimeError;
ypAPI ypObject * yp_NotImplementedError;
ypAPI ypObject * yp_SyntaxError;
ypAPI ypObject * yp_IndentationError;
ypAPI ypObject * yp_TabError;
ypAPI ypObject * yp_ReferenceError;
ypAPI ypObject * yp_SystemError;
ypAPI ypObject * yp_SystemExit;
ypAPI ypObject * yp_TypeError;
ypAPI ypObject * yp_InvalidatedError; // operation on invalidated object; "subclass" of yp_TypeError
ypAPI ypObject * yp_UnboundLocalError;
ypAPI ypObject * yp_UnicodeError;
ypAPI ypObject * yp_UnicodeEncodeError;
ypAPI ypObject * yp_UnicodeDecodeError;
ypAPI ypObject * yp_UnicodeTranslateError;
ypAPI ypObject * yp_ValueError;
ypAPI ypObject * yp_ZeroDivisionError;
ypAPI ypObject * yp_BufferError;
ypAPI ypObject * yp_RecursionErrorInst;


/*
 * Direct Object Memory Access
 */

// XXX The "X" in these names is a reminder that the function is returning internal memory, and
// as such should be used with caution.

// For sequences that store their elements as an array of pointers to ypObjects (list and tuple),
// returns a pointer to the beginning of that array, and sets len to the length of the sequence.
// The returned value points into internal object memory, so they are *borrowed* references and
// MUST NOT be modified; furthermore, the sequence itself must not be modified while using the
// array.  Returns NULL and sets len to -1 on error.
// TODO return an exception object...be consistent among X functions
ypObject const * *yp_itemarrayX( ypObject *seq, yp_ssize_t *len );

// TODO Similar X functions for the other types; some of these could allow modifications


/*
 * Optional Macros
 *
 * These macros may make working with nohtyP easier, but are not required.  They are best described
 * by the nohtyP examples, but are documented below.  The implementations of these macros are
 * considered internal; you'll find them near the end of this header.
 */

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
// As in Python, a condition is only evaluated if previous conditions evaluated false and did not
// raise an exception, the exception-branch is executed if any evaluated condition raises an
// exception, and the exception variable is only set if an exception occurs.  Unlike Python,
// exceptions in the chosen branch do not trigger the exception-branch, and the exception variable
// is not cleared at the end of the exception-branch.
// If a condition creates a new reference that must be discarded, use yp_IFd and/or yp_ELIFd ("d"
// stands for "discard" or "decref"):
//      yp_IFd( yp_getitem( a, key ) )

// yp_WHILE: A series of macros to emulate a while/else with exception handling.  To be used
// strictly as follows (including braces):
//      yp_WHILE( condition ) {
//          // suite
//      } yp_WHILE_ELSE {           // optional
//          // else-suite
//      } yp_WHILE_EXCEPT_AS( e ) { // optional; can also use yp_WHILE_EXCEPT
//          // exception-suite
//      } yp_ENDWHILE
// C's break and continue statements work as you'd expect.  As in Python, the condition is
// evaluated multiple times until:
//  - it evaluates to false, in which case the else-suite is executed
//  - a break statement, in which case neither the else- nor exception-suites are executed
//  - an exception occurs in condition, in which case e is set to the exception and the
//  exception-suite is executed
// Unlike Python, exceptions in the suites do not trigger the exception-suite, and the exception
// variable is not cleared at the end of the exception-suite.
// If condition creates a new reference that must be discarded, use yp_WHILEd ("d" stands for
// "discard" or "decref"):
//      yp_WHILEd( yp_getindexC( a, -1 ) )
// TODO if yp_WHILE_EXCEPT_AS declared the exception variable internally, then it would disappear once
// outside of the block and thus behave more like Python (here and elsewhere)

// yp_FOR: A series of macros to emulate a for/else with exception handling.  To be used strictly
// as follows (including braces):
//      yp_FOR( x, expression ) {
//          // suite
//      } yp_FOR_ELSE {             // optional
//          // else-suite
//      } yp_FOR_EXCEPT_AS( e ) {   // optional; can also use yp_FOR_EXCEPT
//          // exception-suite
//      } yp_ENDFOR
// C's break and continue statements work as you'd expect.  As in Python, the expression is
// evaluated once to create an iterator, then the suite is executed once with each successfully-
// yielded value assigned to x (which can be reassigned within the suite).  This occurs until:
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

// yp: A set of macros to make nohtyP function calls look more like Python operators and method
// calls.  Best explained with examples:
//  a.append( b )           --> yp( a,append, b )               --> yp_append( a, b )
//  a + b                   --> yp( a, add, b )                 --> yp_add( a, b )
// For methods that take no arguments, use yp0 (unlike elsewhere, the postfix counts the number of
// arguments to the equivalent Python method):
//  a.isspace( )            --> yp0( a,isspace )                --> yp_isspace( a )
// If variadic macros are supported by your compiler, yp can take multiple arguments:
//  a.setdefault( b, c )    --> yp( a,setdefault, b, c )        --> yp_setdefault( a, b, c )
//  a.startswith( b, 2, 7 ) --> yp( a,startswith4, b, 2, 7 )    --> yp_startswith4( a, b, 2, 7 )
// If variadic macros are not supported, use yp2, yp3, etc (note how the 4 in startswith4 is 1 plus
// the 3 in yp3):
//  a.setdefault( b, c )    --> yp2( a,setdefault, b, c )       --> yp_setdefault( a, b, c )
//  a.startswith( b, 2, 7 ) --> yp3( a,startswith4, b, 2, 7 )   --> yp_startswith4( a, b, 2, 7 )

// A macro to get exception info as a string, include file/line info of the place the macro is
// checked

// TODO In places where functions have "defaults" and their names include their arg counts (ie
// yp_getdefault3), eventually pick which possible version of the function is the "primary" and
// drop the number for that version (ie make it yp_getdefault).


/*
 * Internals  XXX Do not use directly!
 */

// This structure is likely to change in future versions; it should only exist in-memory
struct _ypObject {
    yp_uint32_t ob_type_refcnt; // first byte type code, remainder ref count
    yp_hash_t   ob_hash;        // cached hash for immutables
    yp_uint16_t ob_len;         // length of object
    yp_uint16_t ob_alloclen;    // allocated length
    void *      ob_data;        // pointer to object data
    // Note that we are 8-byte aligned here on both 32- and 64-bit systems
};

// ypObject_HEAD defines the initial segment of every ypObject
#define _ypObject_HEAD \
    ypObject ob_base;
// Declares the ob_inline_data array for container object structures
#define _yp_INLINE_DATA( elemType ) \
    elemType ob_inline_data[1]

// This structure is likely to change in future versions; it should only exist in-memory
struct _ypBytesObject {
    _ypObject_HEAD
    _yp_INLINE_DATA( yp_uint8_t );
};

// A refcnt of this value means the object is immortal
#define _ypObject_REFCNT_IMMORTAL (0xFFFFFFu)
// When a hash of this value is stored in ob_hash, call tp_hash (which may then update cache)
#define _ypObject_HASH_INVALID ((yp_hash_t) -1)
// Signals an invalid length stored in ob_len (so call tp_len) or ob_alloclen
#define _ypObject_LEN_INVALID        (0xFFFFu)
#define _ypObject_ALLOCLEN_INVALID   (0xFFFFu)

// First byte of object structure is the type code; next 3 bytes is reference count
#define _ypObject_MAKE_TYPE_REFCNT( type, refcnt ) \
    ( ((type) & 0xFFu) | (((refcnt) & 0xFFFFFFu) << 8) )

// These type codes must match those in nohtyP.c
#define _ypBytes_CODE                ( 16u)

// "Constructors" for immortal objects; implementation considered "internal", documentation above
#define _yp_IMMORTAL_HEAD_INIT( type, data, len ) \
    { _ypObject_MAKE_TYPE_REFCNT( type, _ypObject_REFCNT_IMMORTAL ), \
      _ypObject_HASH_INVALID, len, 0, data }
#define yp_IMMORTAL_BYTES( name, value ) \
    static const char _ ## name ## _data[] = value; \
    static struct _ypBytesObject _ ## name ## _struct = { _yp_IMMORTAL_HEAD_INIT( \
        _ypBytes_CODE, (void *) _ ## name ## _data, sizeof( _ ## name ## _data )-1 ) }; \
    ypObject * const name = (ypObject *) &_ ## name ## _struct /* force use of semi-colon */
// TODO yp_IMMORTAL_TUPLE, if useful

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
// TODO define the target internally?
#define yp_ELSE_EXCEPT_AS( target ) \
    } if( yp_isexceptionC( _yp_IF_cond ) ) { \
        target = _yp_IF_cond;
#define yp_ENDIF \
    } }

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

// The implementation of yp_FOR is considered "internal"; see above for documentation
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
    if( _yp_FOR_item == yp_StopIteration )
#define yp_FOR_EXCEPT \
    if( yp_isexceptionC( _yp_FOR_item ) && \
        _yp_FOR_item != yp_StopIteration )
#define yp_FOR_EXCEPT_AS( target ) \
    if( yp_isexceptionC( _yp_FOR_item ) && \
        _yp_FOR_item != yp_StopIteration && \
        (target = _yp_FOR_item) )
#define yp_ENDFOR \
    yp_decref( _yp_FOR_item ); \
    yp_decref( _yp_FOR_iter ); \
    }

// The implementation of "yp" is considered "internal"; see above for documentation
#define yp0( self, method )         yp_ ## method( self )
#define yp1( self, method, a1 )     yp_ ## method( self, a1 )
#ifdef yp_NO_VARIADIC_MACROS // TODO Rename?
#define yp yp1
#else
#define yp( self, method, ... )     yp_ ## method( self, _VA_ARGS_ )
#endif
#define yp2( self, method, a1, a2 ) yp_ ## method( self, a1, a2 )
#define yp3( self, method, a1, a2, a3 ) yp_ ## method( self, a1, a2, a3 )
#define yp4( self, method, a1, a2, a3, a4 ) yp_ ## method( self, a1, a2, a3, a4 )


#ifdef __cplusplus
}
#endif
#endif // yp_NOHTYP_H

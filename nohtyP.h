/*
 * nohtyP.h - A Python-like API for C, in one .c and one .h
 *      http://bitbucket.org/Syeberman/nohtyp   [v0.1.0 $Change$]
 *      Copyright (c) 2001-2020 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/3/license.html
 *
 * The goal of nohtyP is to enable Python-like code to be written in C.  It is patterned after
 * Python's built-in API, then adjusted for expected usage patterns.  It also borrows ideas from
 * Python's own C API.  To be as portable as possible, it is written in one .c and one .h file and
 * is tested against multiple compilers.  The documentation below is complete, but brief; more
 * detailed documentation can be found at http://docs.python.org/3/.
 *
 * Most functions borrow inputs, create their own references, and output new references.  Errors
 * are handled in one of three ways.  Functions that return objects simply return an appropriate
 * exception object on error:
 *
 *      value = yp_getitem(dict, key);
 *      if(yp_isexceptionC(value)) printf("unknown key");
 *
 * Functions that modify objects accept a ypObject* by reference and replace that object with an
 * exception on error, discarding the original reference:
 *
 *      yp_setitem(&dict, key, value);
 *      if(yp_isexceptionC(dict)) printf("unhashable key, dict discarded");
 *
 * If you don't want the modified object discarded on error, use the 'E' version of the function.
 * Such functions accept a ypObject** that is set to the exception; it is set _only_ on error,
 * and existing values are not discarded, so the variable should first be initialized to an
 * immortal like yp_None:
 *
 *      ypObject *exc = yp_None;
 *      yp_setitemE(dict, key, value, &exc);
 *      if(yp_isexceptionC(exc)) printf("unhashable key, dict not modified");
 *
 * This scheme is also used for functions that return C values:
 *
 *      ypObject *exc = yp_None;
 *      len = yp_lenC(x, &exc);
 *      if(yp_isexceptionC(exc)) printf("x isn't a container");
 *
 * Unless explicitly documented as "always succeeds", _any_ function can return an exception.
 *
 * These error handling methods are designed for a specific purpose: to allow combining multiple
 * function calls without checking for errors in-between.  When an exception object is used as
 * input to a function, it is immediately returned, allowing you to check for errors only at the
 * end of a block of code:
 *
 *      newdict = yp_dictK(0);            // newdict might be yp_MemoryError
 *      value = yp_getitem(olddict, key); // value could be yp_KeyError
 *      yp_iaddC(&value, 5);              // possibly replaces value with yp_TypeError
 *      yp_setitem(&newdict, key, value); // if value is an exception, newdict will be too
 *      yp_decref(value);                 // a no-op if value is an exception
 *      if(yp_isexceptionC(newdict)) abort();
 *
 * Similarly, for 'E' and 'C' functions:
 *
 *      ypObject *exc = yp_None;                // ensure exc is initialized to an immortal
 *      value = yp_getitem(dict, key);          // value could be yp_KeyError
 *      lenC = yp_lenC(obj, &exc);              // possibly sets exc to yp_TypeError
 *      yp_setindexE(obj, lenC/2, value, &exc); // if value is an exception, exc will be too
 *      yp_decref(value);                       // a no-op if value is an exception
 *      if(yp_isexceptionC(exc)) abort();
 *
 * This API is threadsafe so long as no objects are modified while being accessed by multiple
 * threads; this includes modifying reference counts, so don't assume immutables are threadsafe!
 * One strategy to ensure safety is to deep copy objects before exchanging between threads.
 * Sharing immutable, immortal objects is always safe.
 *
 * Certain functions are given postfixes to highlight their unique behaviour:
 *
 *  C - C native types are accepted and returned where appropriate
 *  L - Library routines that operate strictly on C types
 *  F - A version of "C" or "L" that accepts floats in place of ints
 *  N - n variable positional arguments follow
 *  K - n key/value arguments follow (for a total of n*2 arguments)
 *  V - A version of "N" or "K" that accepts a va_list in place of ...
 *  E - Errors modifying an object do not discard the object: instead, errors set *exc
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
#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>

// To link to nohtyP statically, simply add nohtyP.c to your project: no special defines are
// required.  To link to nohtyP dynamically, first build nohtyP.c as a shared library with
// yp_ENABLE_SHARED and yp_BUILD_CORE, then include nohtyP.h with yp_ENABLE_SHARED.
#ifdef yp_ENABLE_SHARED
#if defined(_WIN32)
#ifdef yp_BUILD_CORE
#define ypAPI extern __declspec(dllexport)
#else
#define ypAPI extern __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define ypAPI extern __attribute__((visibility("default")))
#endif
#endif
#ifndef ypAPI
#define ypAPI extern
#endif


/*
 * Initialization
 */

typedef struct _yp_initialize_parameters_t yp_initialize_parameters_t;

// Must be called once before any other function; subsequent calls are a no-op.  If a fatal error
// occurs abort() is called.  args can be NULL to accept all defaults; further documentation
// on these parameters can be found below.
ypAPI void yp_initialize(const yp_initialize_parameters_t *args);


/*
 * Object Fundamentals
 */

// All objects maintain a reference count; when that count reaches zero, the object is deallocated.
// Certain objects are immortal and, thus, never deallocated; examples include yp_None, yp_True,
// yp_False, exceptions, and so forth.

// nohtyP objects are only accessed through pointers to ypObject.
typedef struct _ypObject ypObject;

// The immortal, null-reference object.
ypAPI ypObject *const yp_None;

// Increments the reference count of x, returning it as a convenience.  Always succeeds; if x is
// immortal this is a no-op.
ypAPI ypObject *yp_incref(ypObject *x);

// A convenience function to increment the references of n objects.
ypAPI void yp_increfN(int n, ...);
ypAPI void yp_increfNV(int n, va_list args);

// Decrements the reference count of x, deallocating it if the count reaches zero.  Always
// succeeds; if x is immortal this is a no-op.
ypAPI void yp_decref(ypObject *x);

// A convenience function to decrement the references of n objects.
ypAPI void yp_decrefN(int n, ...);
ypAPI void yp_decrefNV(int n, va_list args);

// Returns true (non-zero) if x is an exception, else false.  Always succeeds.
ypAPI int yp_isexceptionC(ypObject *x);


/*
 * nohtyP's C types
 */

// Fixed-size numeric C types
typedef signed char    yp_int8_t;
typedef unsigned char  yp_uint8_t;
typedef short          yp_int16_t;
typedef unsigned short yp_uint16_t;
#if UINT_MAX == 0xFFFFFFFFu
typedef int          yp_int32_t;
typedef unsigned int yp_uint32_t;
#else
typedef long          yp_int32_t;
typedef unsigned long yp_uint32_t;
#endif
typedef long long          yp_int64_t;
typedef unsigned long long yp_uint64_t;
typedef float              yp_float32_t;
typedef double             yp_float64_t;

// Size- and length-related C types
#if defined(SSIZE_MAX)
typedef ssize_t yp_ssize_t;
#define yp_SSIZE_T_MAX SSIZE_MAX
#elif SIZE_MAX == 0xFFFFFFFFu
typedef yp_int32_t    yp_ssize_t;
#define yp_SSIZE_T_MAX (0x7FFFFFFF)
#else
typedef yp_int64_t yp_ssize_t;
#define yp_SSIZE_T_MAX (0x7FFFFFFFFFFFFFFFLL)
#endif
#define yp_SSIZE_T_MIN (-yp_SSIZE_T_MAX - 1)
typedef yp_ssize_t yp_hash_t;

// C types used to represent the numeric objects within nohtyP
typedef yp_int64_t yp_int_t;
#define yp_INT_T_MAX LLONG_MAX
#define yp_INT_T_MIN LLONG_MIN
typedef yp_float64_t yp_float_t;


/*
 * Constructors
 */

// Unlike Python, most nohtyP types have both mutable and immutable versions.  An "intstore" is a
// mutable int (it "stores" an int); similar for floatstore.  The mutable str is called a
// "chrarray", while a "frozendict" is an immutable dict.

// Returns a new reference to an int/intstore with the given value.
ypAPI ypObject *yp_intC(yp_int_t value);
ypAPI ypObject *yp_intstoreC(yp_int_t value);

// Returns a new reference to an int/intstore with the given str, chrarray, bytes, or bytearray
// object interpreted as an integer literal in radix base.  The Python-equivalent default for base
// is 10; a base of 0 means to interpret exactly as a Python code literal.
ypAPI ypObject *yp_int_baseC(ypObject *x, yp_int_t base);
ypAPI ypObject *yp_intstore_baseC(ypObject *x, yp_int_t base);

// Returns a new reference to an int/intstore.  If x is a number, it is converted to an int;
// floating-point numbers are truncated towards zero.  Otherwise, x must be a str, chrarray, bytes,
// or bytearray object, and this is equivalent to yp_int_baseC(x, 10).
ypAPI ypObject *yp_int(ypObject *x);
ypAPI ypObject *yp_intstore(ypObject *x);

// Returns a new reference to a float/floatstore with the given value.
ypAPI ypObject *yp_floatCF(yp_float_t value);
ypAPI ypObject *yp_floatstoreCF(yp_float_t value);

// Returns a new reference to a float/floatstore.  If x is a number, it is converted to a float.
// Otherwise, x must be a str, chrarray, bytes, or bytearray object, which will be interpreted as a
// Python floating-point literal.  In either case, if the resulting value is outside the range of
// a float then yp_OverflowError is returned.
ypAPI ypObject *yp_float(ypObject *x);
ypAPI ypObject *yp_floatstore(ypObject *x);

// Returns a new reference to an iterator for object x.  It is usually unwise to modify an object
// being iterated over.
ypAPI ypObject *yp_iter(ypObject *x);

// Returns a new reference to a generator iterator object using the given func.  The given n objects
// will be made available to func via yp_iter_stateCX as an array; new references to these objects
// will be created, and those references will be discarded when the iterator is deallocated.
// length_hint is a clue to consumers of the iterator how many items will be yielded; use zero if
// this is not known.  Further documentation for yp_generator_func_t can be found below.
typedef ypObject *(*yp_generator_func_t)(ypObject *iterator, ypObject *value);
ypAPI ypObject *yp_generatorCN(yp_generator_func_t func, yp_ssize_t length_hint, int n, ...);
ypAPI ypObject *yp_generatorCNV(
        yp_generator_func_t func, yp_ssize_t length_hint, int n, va_list args);

// Similar to yp_generatorCN, but accepts an arbitrary structure (or array) of the given size which
// will be copied into the iterator and made available to func via yp_iter_stateCX.  If state
// contains any objects, their offsets must be given as the variable arguments; new references to
// these objects will be created, and those references will be discarded when the iterator is
// deallocated.
//  Ex: typedef struct { int x; int y; ypObject *obj1; ypObject *obj2 } mystruct;
//      ypObject *obj1 = yp_dictK(0);
//      mystruct state = { 20, 40, obj1, yp_False };
//      gen = yp_generator_fromstructCN(mygenfunc, 0, &state, sizeof(mystruct),
//                  2, offsetof(mystruct, obj1), offsetof(mystruct, obj2));
//      yp_decref(obj1);  // the iterator has its own reference, so discard ours
// Objects in state cannot be part of a union, because nohtyP cannot know which union member is the
// "active" one.  If state contains objects in an array you must list _each_ _individual_ offset.
// state _can_ contain exceptions.
ypAPI ypObject *yp_generator_fromstructCN(
        yp_generator_func_t func, yp_ssize_t length_hint, void *state, yp_ssize_t size, int n, ...);
ypAPI ypObject *yp_generator_fromstructCNV(yp_generator_func_t func, yp_ssize_t length_hint,
        void *state, yp_ssize_t size, int n, va_list args);

// Returns a new reference to a range object. yp_rangeC is equivalent to yp_rangeC3(0, stop, 1).
ypAPI ypObject *yp_rangeC3(yp_int_t start, yp_int_t stop, yp_int_t step);
ypAPI ypObject *yp_rangeC(yp_int_t stop);

// Returns a new reference to a bytes/bytearray, copying the first len bytes from source.  If
// source is NULL it is considered as having all null bytes; if len is negative source is
// considered null terminated (and, therefore, will not contain the null byte).
//  Ex: pre-allocate a bytearray of length 50: yp_bytearrayC(NULL, 50)
// FIXME Shouldn't len go first?! Are we consistent with (len, array) everywhere?
ypAPI ypObject *yp_bytesC(const yp_uint8_t *source, yp_ssize_t len);
ypAPI ypObject *yp_bytearrayC(const yp_uint8_t *source, yp_ssize_t len);

// Returns a new reference to a bytes/bytearray encoded from the given str or chrarray object.  The
// Python-equivalent default for encoding is yp_s_utf_8, while for errors it is yp_s_strict.
ypAPI ypObject *yp_bytes3(ypObject *source, ypObject *encoding, ypObject *errors);
ypAPI ypObject *yp_bytearray3(ypObject *source, ypObject *encoding, ypObject *errors);

// Returns a new reference to a bytes/bytearray object, depending on the type of source:
//  - integer: the array will have that size and be initialized with null bytes
//  - iterable: it must be an iterable of integers in range(256) used to initialize the array (this
//  includes bytes and bytearray objects)
// Raises yp_TypeError if source is a str/chrarray; instead, use yp_bytes3/yp_bytearray3.
ypAPI ypObject *yp_bytes(ypObject *source);
ypAPI ypObject *yp_bytearray(ypObject *source);

// Returns a new reference to an empty bytes/bytearray.
ypAPI ypObject *yp_bytes0(void);
ypAPI ypObject *yp_bytearray0(void);

// Returns a new reference to a str/chrarray decoded from the given bytes.  source and len are as
// in yp_bytesC.  The Python-equivalent default for encoding is yp_s_utf_8 (compatible with an
// ascii-encoded source), while for errors it is yp_s_strict.  Equivalent to:
//  yp_str3(yp_bytesC(source, len), encoding, errors)
// FIXME Shouldn't len go first?!
ypAPI ypObject *yp_str_frombytesC4(
        const yp_uint8_t *source, yp_ssize_t len, ypObject *encoding, ypObject *errors);
ypAPI ypObject *yp_chrarray_frombytesC4(
        const yp_uint8_t *source, yp_ssize_t len, ypObject *encoding, ypObject *errors);

// Equivalent to yp_str3(yp_bytesC(source, len), yp_s_utf_8, yp_s_strict). Note that in Python,
// omitting encoding and errors would normally return the string representation of the bytes object
// ("b'Zoot!'"), however this constructor decodes it ("Zoot!").
ypAPI ypObject *yp_str_frombytesC2(const yp_uint8_t *source, yp_ssize_t len);
ypAPI ypObject *yp_chrarray_frombytesC2(const yp_uint8_t *source, yp_ssize_t len);

// Returns a new reference to a str/chrarray decoded from the given bytes or bytearray object.  The
// Python-equivalent default for encoding is yp_s_utf_8, while for errors it is yp_s_strict.
ypAPI ypObject *yp_str3(ypObject *source, ypObject *encoding, ypObject *errors);
ypAPI ypObject *yp_chrarray3(ypObject *source, ypObject *encoding, ypObject *errors);

// Returns a new reference to the "informal" or nicely-printable string representation of object,
// as a str/chrarray.  As in Python, passing a bytes object to this constructor returns the string
// representation ("b'Zoot!'"); to decode the bytes, use yp_str3.
ypAPI ypObject *yp_str(ypObject *object);
ypAPI ypObject *yp_chrarray(ypObject *object);

// Returns a new reference to an empty str/chrarray.
ypAPI ypObject *yp_str0(void);
ypAPI ypObject *yp_chrarray0(void);

// Returns a new reference to the str representing a character whose Unicode codepoint is the
// integer i.
ypAPI ypObject *yp_chrC(yp_int_t i);

// Returns a new reference to a tuple/list of length n containing the given objects.
ypAPI ypObject *yp_tupleN(int n, ...);
ypAPI ypObject *yp_tupleNV(int n, va_list args);
ypAPI ypObject *yp_listN(int n, ...);
ypAPI ypObject *yp_listNV(int n, va_list args);

// Returns a new reference to a tuple/list made from factor shallow-copies of yp_tupleN(n, ...)
// concatenated; the length will be factor*n.  Equivalent to "factor * (obj0, obj1, ...)" in
// Python.
//  Ex: pre-allocate a list of length 99: yp_list_repeatCN(99, 1, yp_None)
//  Ex: an 8-tuple containing alternating bools: yp_tuple_repeatCN(4, 2, yp_False, yp_True)
ypAPI ypObject *yp_tuple_repeatCN(yp_ssize_t factor, int n, ...);
ypAPI ypObject *yp_tuple_repeatCNV(yp_ssize_t factor, int n, va_list args);
ypAPI ypObject *yp_list_repeatCN(yp_ssize_t factor, int n, ...);
ypAPI ypObject *yp_list_repeatCNV(yp_ssize_t factor, int n, va_list args);

// Returns a new reference to a tuple/list whose elements come from iterable.
ypAPI ypObject *yp_tuple(ypObject *iterable);
ypAPI ypObject *yp_list(ypObject *iterable);

// Returns a new reference to a sorted list from the items in iterable.  key is a one-argument
// function used to extract a comparison key from each element in iterable; to compare the elements
// directly, use yp_None.  If reverse is true, the list elements are sorted as if each comparison
// were reversed.
ypAPI ypObject *yp_sorted3(ypObject *iterable, ypObject *key, ypObject *reverse);

// Equivalent to yp_sorted3(iterable, yp_None, yp_False).
ypAPI ypObject *yp_sorted(ypObject *iterable);

// Returns a new reference to a frozenset/set containing the given n objects; the length will be n,
// unless there are duplicate objects.
ypAPI ypObject *yp_frozensetN(int n, ...);
ypAPI ypObject *yp_frozensetNV(int n, va_list args);
ypAPI ypObject *yp_setN(int n, ...);
ypAPI ypObject *yp_setNV(int n, va_list args);

// Returns a new reference to a frozenset/set whose elements come from iterable.
ypAPI ypObject *yp_frozenset(ypObject *iterable);
ypAPI ypObject *yp_set(ypObject *iterable);

// Returns a new reference to a frozendict/dict containing the given n key/value pairs (for a total
// of 2*n objects); the length will be n, unless there are duplicate keys, in which case the last
// value will be retained.
//  Ex: yp_dictK(3, key0, value0, key1, value1, key2, value2)
ypAPI ypObject *yp_frozendictK(int n, ...);
ypAPI ypObject *yp_frozendictKV(int n, va_list args);
ypAPI ypObject *yp_dictK(int n, ...);
ypAPI ypObject *yp_dictKV(int n, va_list args);

// Returns a new reference to a frozendict/dict containing the given n keys all set to value; the
// length will be n, unless there are duplicate keys.  The Python-equivalent default of value is
// yp_None.  Note that, unlike Python, value is the _first_ argument.
//  Ex: pre-allocate a dict with 3 keys: yp_dict_fromkeysN(yp_None, 3, key0, key1, key2)
ypAPI ypObject *yp_frozendict_fromkeysN(ypObject *value, int n, ...);
ypAPI ypObject *yp_frozendict_fromkeysNV(ypObject *value, int n, va_list args);
ypAPI ypObject *yp_dict_fromkeysN(ypObject *value, int n, ...);
ypAPI ypObject *yp_dict_fromkeysNV(ypObject *value, int n, va_list args);

// Returns a new reference to a frozendict/dict containing the keys from iterable all set to value.
// The Python-equivalent default of value is yp_None.
ypAPI ypObject *yp_frozendict_fromkeys(ypObject *iterable, ypObject *value);
ypAPI ypObject *yp_dict_fromkeys(ypObject *iterable, ypObject *value);

// Returns a new reference to a frozendict/dict whose key-value pairs come from x.  x can be a
// mapping object (that supports yp_iter_items), or an iterable that yields exactly two items at a
// time (ie (key, value)).  If a given key is seen more than once, the last value yielded is
// retained.
ypAPI ypObject *yp_frozendict(ypObject *x);
ypAPI ypObject *yp_dict(ypObject *x);

// FIXME Move these forward defs to one location?
typedef struct _yp_function_definition_t yp_function_definition_t;

// FIXME Rewrite docs.
// Returns a new reference to a function object whose implementation comes from code and whose
// inputs are described by parameters.  Raises yp_FIXME if code contains no implementations, and
// yp_ParameterSyntaxError if parameters fails validation.  Further documentation for
// yp_function_definition_t and yp_function_parameters_t can be found below.
// FIXME Should parameters==NULL be a shortcut for something?  No parameters?  `*args, **kwargs`?
// FIXME What about code==NULL?
// FIXME Really think hard about which inputs to bury in typedefs and which to surface as C params.
// For example, should code and parameters be in the same struct?
// FIXME Should this be yp_functionC? yp_function should make a function object out of other
// objects, right...? (We need a linter to ensure correct use of C suffix.)
ypAPI ypObject *yp_function(yp_function_definition_t *definition);

// FIXME Rewrite docs.
// Returns a new reference to a generator iterator object using the given code.  The given n objects
// will be made available to code via yp_iter_stateCX as an array; new references to these objects
// will be created, and those references will be discarded when the iterator is deallocated.
// length_hint is a clue to consumers of the iterator how many items will be yielded; use zero if
// this is not known.  Further documentation for yp_generator_func_t can be found below.
// FIXME A version of yp_iter_stateCX for functions
// FIXME Rename yp_generator to match?
// FIXME Is the C suffix applicable here?
// FIXME Really think hard about the symmetry between these and yp_generator*.  Perhaps they should
// all use a standard set of typedefs...i.e. remove some C params and make them structs.
// FIXME Generator names this _fromstate...be consistent
ypAPI ypObject *yp_function_withstateCN(yp_function_definition_t *definition, int n, ...);
ypAPI ypObject *yp_function_withstateCNV(yp_function_definition_t *definition, int n, va_list args);

// FIXME Rewrite docs.
// Similar to yp_functionCN, but accepts an arbitrary structure (or array) of the given size which
// will be copied into the iterator and made available to code via yp_iter_stateCX.  If state
// contains any objects, their offsets must be given as the variable arguments; new references to
// these objects will be created, and those references will be discarded when the iterator is
// deallocated.
//  Ex: typedef struct { int x; int y; ypObject *obj1; ypObject *obj2 } mystruct;
//      ypObject *obj1 = yp_dictK(0);
//      mystruct state = { 20, 40, obj1, yp_False };
//      gen = yp_function_fromstructCN(mygenfunc, 0, &state, sizeof(mystruct),
//                  2, offsetof(mystruct, obj1), offsetof(mystruct, obj2));
//      yp_decref(obj1);  // the iterator has its own reference, so discard ours
// Objects in state cannot be part of a union, because nohtyP cannot know which union member is the
// "active" one.  If state contains objects in an array you must list _each_ _individual_ offset.
// state _can_ contain exceptions.
// FIXME Is the C suffix applicable here?
// FIXME Combine size, n, and args into a single structure that can be allocated statically
// (here and in generators). A structure's definition is static, so why not its description?
ypAPI ypObject *yp_function_withstatestructCN(
        yp_function_definition_t *definition, void *state, yp_ssize_t size, int n, ...);
ypAPI ypObject *yp_function_withstatestructCNV(
        yp_function_definition_t *definition, void *state, yp_ssize_t size, int n, va_list args);


// FIXME What about creating a function object that always returns a particular value?
// FIXME Partial functions, created from other functions?
// FIXME Statically-allocated immortal functions a la yp_IMMORTAL_STR_LATIN_1. Although, how to
// catch errors in the function definition (i.e. required parameters after default parameters)?


// XXX The file type will be added in a future version


/*
 * Boolean Operations and Comparisons
 */

// Unlike Python, bools do not support arithmetic.

// There are exactly two boolean values, both immortal: yp_True and yp_False.
ypAPI ypObject *const yp_True;
ypAPI ypObject *const yp_False;

// Returns the immortal yp_False if the object should be considered false (yp_None, a number equal
// to zero, or a container of zero length), otherwise yp_True.
ypAPI ypObject *yp_bool(ypObject *x);

// Returns the immortal yp_True if x is considered false, otherwise yp_False.
ypAPI ypObject *yp_not(ypObject *x);

// Returns a *new* reference to y if x is false, otherwise to x.  Unlike Python, both arguments are
// always evaluated (there is no short-circuiting).  You may find yp_anyN more convenient, as it
// returns an immortal.
ypAPI ypObject *yp_or(ypObject *x, ypObject *y);

// A convenience function to "or" n objects, returning a *new* reference.  Returns yp_False if n is
// zero, and the first object if n is one.  Unlike Python, all arguments are always evaluated (there
// is no short-circuiting).  You may find yp_anyN more convenient, as it returns an immortal.
ypAPI ypObject *yp_orN(int n, ...);
ypAPI ypObject *yp_orNV(int n, va_list args);

// Equivalent to yp_bool(yp_orN(n, ...)).  (Returns an immortal.)
ypAPI ypObject *yp_anyN(int n, ...);
ypAPI ypObject *yp_anyNV(int n, va_list args);

// Returns the immortal yp_True if any element of iterable is true; if the iterable is empty,
// returns yp_False.  Stops iterating at the first true element.
ypAPI ypObject *yp_any(ypObject *iterable);

// Returns a *new* reference to x if x is false, otherwise to y.  Unlike Python, both arguments are
// always evaluated (there is no short-circuiting).  You may find yp_allN more convenient, as it
// returns an immortal.
ypAPI ypObject *yp_and(ypObject *x, ypObject *y);

// A convenience function to "and" n objects, returning a *new* reference.  Returns yp_True if n is
// zero, and the first object if n is one.  Unlike Python, all arguments are always evaluated (there
// is no short-circuiting).  You may find yp_allN more convenient, as it returns an immortal.
ypAPI ypObject *yp_andN(int n, ...);
ypAPI ypObject *yp_andNV(int n, va_list args);

// Equivalent to yp_bool(yp_andN(n, ...)).  (Returns an immortal.)
ypAPI ypObject *yp_allN(int n, ...);
ypAPI ypObject *yp_allNV(int n, va_list args);

// Returns the immortal yp_True if all elements of iterable are true or the iterable is empty.
// Stops iterating at the first false element.
ypAPI ypObject *yp_all(ypObject *iterable);

// Implements the "less than" (x<y), "less than or equal" (x<=y), "equal" (x==y), "not equal"
// (x!=y), "greater than or equal" (x>=y), and "greater than" (x>y) comparisons.  Returns the
// immortal yp_True if the condition is true, otherwise yp_False.
ypAPI ypObject *yp_lt(ypObject *x, ypObject *y);
ypAPI ypObject *yp_le(ypObject *x, ypObject *y);
ypAPI ypObject *yp_eq(ypObject *x, ypObject *y);
ypAPI ypObject *yp_ne(ypObject *x, ypObject *y);
ypAPI ypObject *yp_ge(ypObject *x, ypObject *y);
ypAPI ypObject *yp_gt(ypObject *x, ypObject *y);

// You may also be interested in yp_IF and yp_WHILE for working with boolean operations; see below.


/*
 * Generic Object Operations
 */

// Returns the hash value of x; x must be immutable and must not contain mutable objects.  Returns
// -1 and sets *exc on error.
ypAPI yp_hash_t yp_hashC(ypObject *x, ypObject **exc);

// Returns the _current_ hash value of x; this value may change between calls. Returns -1 and sets
// *exc on error.  This is able to calculate the hash value of mutable objects and objects
// containing mutable objects that would otherwise fail with yp_hashC.
ypAPI yp_hash_t yp_currenthashC(ypObject *x, ypObject **exc);


/*
 * Iterator Operations
 */

// An "iterator" is an object that implements yp_next, while an "iterable" is an object that
// implements yp_iter.  Examples of iterables include range, bytes, str, tuple, set, and dict;
// examples of iterators include files and generator iterators.  It is usually unwise to modify an
// object being iterated over.

// FIXME Now that iterators are considered "immutable", should we discard it on _next/etc? i.e.
// should we turn ypObject **iterator into just ypObject *iterator?

// "Sends" a value into *iterator and returns a new reference to the next yielded value, or an
// exception.  The value may be ignored by the iterator.  value cannot be an exception.  When the
// iterator is exhausted yp_StopIteration is returned; on any other error, *iterator is discarded
// and set to an exception, _and_ an exception is returned.
ypAPI ypObject *yp_send(ypObject **iterator, ypObject *value);

// Equivalent to yp_send(iterator, yp_None).  Typically used on iterators that ignore the value.
ypAPI ypObject *yp_next(ypObject **iterator);

// Similar to yp_next, but when the iterator is exhausted a new reference to defval is returned.
// defval _can_ be an exception: if it is, then exhaustion is treated as an error as per yp_send
// (including possibly discarding *iterator).
ypAPI ypObject *yp_next2(ypObject **iterator, ypObject *defval);

// "Sends" an exception into *iterator and returns a new reference to the next yielded value, or an
// exception.  The iterator may ignore exc, or it may return it or any other exception.  If exc is
// not an exception, yp_TypeError is raised.  When the iterator is exhausted yp_StopIteration is
// returned; on any other error, *iterator is discarded and set to an exception, _and_ an exception
// is returned.
ypAPI ypObject *yp_throw(ypObject **iterator, ypObject *exc);

// Returns a hint as to how many items are left to be yielded.  The accuracy of this hint depends
// on the underlying type: most containers know their lengths exactly, but some generators may not.
// A hint of zero could mean that the iterator is exhausted, that the length is unknown, or that
// the iterator will yield infinite values.  Returns zero and sets *exc on error.
// FIXME Compare against Python's __length_hint__ now that it's official.
ypAPI yp_ssize_t yp_length_hintC(ypObject *iterator, ypObject **exc);

// "Closes" the iterator by calling yp_throw(iterator, yp_GeneratorExit).  If yp_StopIteration or
// yp_GeneratorExit is returned by yp_throw, *iterator is not discarded; on any other error,
// *iterator is discarded and set to an exception.  The behaviour of this method for other
// types, in particular files, is documented elsewhere.
ypAPI void yp_close(ypObject **iterator);

// Sets the given n ypObject**s to new references for the values yielded from iterable.  Iterable
// must yield exactly n objects, or else a yp_ValueError is raised.  Sets all n ypObject**s to the
// same exception on error.
ypAPI void yp_unpackN(ypObject *iterable, int n, ...);
ypAPI void yp_unpackNV(ypObject *iterable, int n, va_list args);

// Returns a new reference to an iterator that yields values from iterable for which function
// returns true; to compare the elements directly, set function to yp_None.
ypAPI ypObject *yp_filter(ypObject *function, ypObject *iterable);

// Similar to yp_filter, but yields values for which function returns false.
ypAPI ypObject *yp_filterfalse(ypObject *function, ypObject *iterable);

// Returns a new reference to the largest/smallest of the given n objects.  key is a one-argument
// function used to extract a comparison key from each element in iterable; to compare the elements
// directly, use yp_None.
ypAPI ypObject *yp_max_keyN(ypObject *key, int n, ...);
ypAPI ypObject *yp_max_keyNV(ypObject *key, int n, va_list args);
ypAPI ypObject *yp_min_keyN(ypObject *key, int n, ...);
ypAPI ypObject *yp_min_keyNV(ypObject *key, int n, va_list args);

// Equivalent to yp_max_keyN(yp_None, n, ...) and yp_min_keyN(yp_None, n, ...).
ypAPI ypObject *yp_maxN(int n, ...);
ypAPI ypObject *yp_maxNV(int n, va_list args);
ypAPI ypObject *yp_minN(int n, ...);
ypAPI ypObject *yp_minNV(int n, va_list args);

// Returns a new reference to the largest/smallest element in iterable.  key is as in yp_max_keyN.
ypAPI ypObject *yp_max_key(ypObject *iterable, ypObject *key);
ypAPI ypObject *yp_min_key(ypObject *iterable, ypObject *key);

// Equivalent to yp_max_key(iterable, yp_None) and yp_min_key(iterable, yp_None).
ypAPI ypObject *yp_max(ypObject *iterable);
ypAPI ypObject *yp_min(ypObject *iterable);

// Returns a new reference to an iterator that yields the elements of sequence in reverse order.
ypAPI ypObject *yp_reversed(ypObject *sequence);

// Returns a new reference to an iterator that aggregates elements from each of the n iterables.
ypAPI ypObject *yp_zipN(int n, ...);
ypAPI ypObject *yp_zipNV(int n, va_list args);

// The signature of a function that can be wrapped up in a generator iterator.  Called by yp_send,
// yp_throw, and similar methods.  iterator is the iterator object; use yp_iter_stateCX to retrieve
// any state variables.  value is the object that is sent into the function by yp_send, or the
// exception sent in by yp_throw; it may also be yp_GeneratorExit if yp_close is called, or another
// exception.  The return value must be a new or immortal reference, yp_StopIteration if the
// iterator is exhausted, or another exception.  The iterator will be closed with yp_close if it
// returns an exception (resulting in one last call with yp_GeneratorExit).
typedef ypObject *(*yp_generator_func_t)(ypObject *iterator, ypObject *value);

// You may also be interested in yp_FOR for working with iterables; see below.


/*
 * Container Operations
 */

// These methods are supported by range, bytes, str, tuple, frozenset, and frozendict (and their
// mutable counterparts, of course).

// Returns the immortal yp_True if an item of container is equal to x, else yp_False.
ypAPI ypObject *yp_contains(ypObject *container, ypObject *x);
ypAPI ypObject *yp_in(ypObject *x, ypObject *container);

// Returns the immortal yp_False if an item of container is equal to x, else yp_True.
ypAPI ypObject *yp_not_in(ypObject *x, ypObject *container);

// Returns the length of container.  Returns zero and sets *exc on error.
ypAPI yp_ssize_t yp_lenC(ypObject *container, ypObject **exc);

// Adds an item to *container.  On error, *container is discarded and set to an exception.  The
// relation between yp_push and yp_pop depends on the type: x may be the first or last item popped,
// or items may be popped in arbitrary order.
ypAPI void yp_push(ypObject **container, ypObject *x);

// Removes all items from *container.  On error, *container is discarded and set to an exception.
ypAPI void yp_clear(ypObject **container);

// Removes an item from *container and returns a new reference to it.  On error, *container is
// discarded and set to an exception _and_ an exception is returned.  (Not supported on dicts; use
// yp_popvalue3 or yp_popitem instead.)
ypAPI ypObject *yp_pop(ypObject **container);


/*
 * Sequence Operations
 */

// These methods are supported by bytes, str, and tuple (and their mutable counterparts, of
// course).  Most methods are also supported by range; notable exceptions are yp_concat and
// yp_repeatC.  They are _not_ supported by frozenset and frozendict because those types do not
// store their elements in any particular order.

// Sequences are indexed origin zero.  Negative indices are relative to the end of the sequence: in
// effect, when i is negative it is substituted with len(s)+i, and len(s)+j for negative j.  The
// slice of s from i to j with step k is the sequence of items with indices i, i+k, i+2*k, i+3*k
// and so on, stopping when j is reached (but never including j); k cannot be zero.  A single index
// outside of range(-len(s),len(s)) raises a yp_IndexError, but in a slice such an index gets
// clamped to the bounds of the sequence.  See yp_SLICE_DEFAULT and yp_SLICE_USELEN below for more
// information.

// Returns a new reference to the concatenation of sequence and x.
ypAPI ypObject *yp_concat(ypObject *sequence, ypObject *x);

// Returns a new reference to factor shallow-copies of sequence, concatenated.
ypAPI ypObject *yp_repeatC(ypObject *sequence, yp_ssize_t factor);

// Returns a new reference to the i-th item of sequence.
ypAPI ypObject *yp_getindexC(ypObject *sequence, yp_ssize_t i);

// Returns a new reference to the slice of sequence from i to j with step k.  The Python-equivalent
// "defaults" for i and j are yp_SLICE_DEFAULT, while for k it is 1.
ypAPI ypObject *yp_getsliceC4(ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k);

// Equivalent to yp_getindexC(sequence, yp_asssizeC(key, &exc)).
ypAPI ypObject *yp_getitem(ypObject *sequence, ypObject *key);

// Returns the lowest index in sequence where x is found, such that x is contained in the slice
// sequence[i:j], or -1 if x is not found.  Returns -1 and sets *exc on error; *exc is _not_ set
// if x is simply not found.  Types such as tuples inspect only one item at a time, while types
// such as strs look for a particular sub-sequence of items.
ypAPI yp_ssize_t yp_findC4(
        ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc);

// Equivalent to yp_findC4(sequence, x, 0, yp_SLICE_USELEN, exc).
ypAPI yp_ssize_t yp_findC(ypObject *sequence, ypObject *x, ypObject **exc);

// Similar to yp_findC4 and yp_findC, except sets *exc to yp_ValueError if x is not found.
ypAPI yp_ssize_t yp_indexC4(
        ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc);
ypAPI yp_ssize_t yp_indexC(ypObject *sequence, ypObject *x, ypObject **exc);

// Similar to yp_findC4, yp_indexC4, etc, except returns the _highest_ index (it starts
// searching "from the right" or "in reverse".)
ypAPI yp_ssize_t yp_rfindC4(
        ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc);
ypAPI yp_ssize_t yp_rfindC(ypObject *sequence, ypObject *x, ypObject **exc);
ypAPI yp_ssize_t yp_rindexC4(
        ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc);
ypAPI yp_ssize_t yp_rindexC(ypObject *sequence, ypObject *x, ypObject **exc);

// Returns the total number of non-overlapping occurrences of x in sequence[i:j].  Returns 0 and
// sets *exc on error.  Types such as tuples inspect only one item at a time, while types such as
// strs look for a particular sub-sequence of items.
ypAPI yp_ssize_t yp_countC4(
        ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j, ypObject **exc);

// Equivalent to yp_countC4(sequence, x, 0, yp_SLICE_USELEN, exc).
ypAPI yp_ssize_t yp_countC(ypObject *sequence, ypObject *x, ypObject **exc);

// Sets the i-th item of *sequence to x.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_setindexC(ypObject **sequence, yp_ssize_t i, ypObject *x);

// Sets the slice of *sequence, from i to j with step k, to x.  The Python-equivalent "defaults"
// for i and j are yp_SLICE_DEFAULT, while for k it is 1.  On error, *sequence is discarded and
// set to an exception.
ypAPI void yp_setsliceC5(
        ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *x);

// Equivalent to yp_setindexC(sequence, yp_asssizeC(key, &exc), x).
ypAPI void yp_setitem(ypObject **sequence, ypObject *key, ypObject *x);

// Removes the i-th item from *sequence.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_delindexC(ypObject **sequence, yp_ssize_t i);

// Removes the elements of the slice from *sequence, from i to j with step k.  The Python-
// equivalent "defaults" for i and j are yp_SLICE_DEFAULT, while for k it is 1.  On error,
// *sequence is discarded and set to an exception.
ypAPI void yp_delsliceC4(ypObject **sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k);

// Equivalent to yp_delindexC(sequence, yp_asssizeC(key, &exc)).
ypAPI void yp_delitem(ypObject **sequence, ypObject *key);

// Appends x to the end of *sequence.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_append(ypObject **sequence, ypObject *x);
ypAPI void yp_push(ypObject **sequence, ypObject *x);

// Appends the contents of t to the end of *sequence.  On error, *sequence is discarded and set to
// an exception.
ypAPI void yp_extend(ypObject **sequence, ypObject *t);

// Appends the contents of *sequence to itself factor-1 times; if factor is zero *sequence is
// cleared.  Equivalent to seq*=factor for lists in Python.  On error, *sequence is discarded and
// set to an exception.
ypAPI void yp_irepeatC(ypObject **sequence, yp_ssize_t factor);

// Inserts x into *sequence at the index given by i; existing elements are shifted to make room.
// On error, *sequence is discarded and set to an exception.
ypAPI void yp_insertC(ypObject **sequence, yp_ssize_t i, ypObject *x);

// Removes the i-th item from *sequence and returns it.  The Python-equivalent "default" for i is
// -1.  On error, *sequence is discarded and set to an exception _and_ an exception is returned.
ypAPI ypObject *yp_popindexC(ypObject **sequence, yp_ssize_t i);

// Equivalent to yp_popindexC(sequence, -1).  Note that for sequences, yp_push and yp_pop
// together implement a stack (last in, first out).
ypAPI ypObject *yp_pop(ypObject **sequence);

// Removes the first item from *sequence that equals x.  Raises yp_ValueError if x is not
// contained in *sequence.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_remove(ypObject **sequence, ypObject *x);

// Removes the first item from *sequence that equals x.  Does _not_ raise an exception if x is not
// contained in *sequence.  On error, *sequence is discarded and set to an exception.
ypAPI void yp_discard(ypObject **sequence, ypObject *x);

// Reverses the items of *sequence in-place.  On error, *sequence is discarded and set to an
// exception.
ypAPI void yp_reverse(ypObject **sequence);

// Sorts the items of *sequence in-place.  key is a one-argument function used to extract a
// comparison key from each element in iterable; to compare the elements directly, use yp_None.  If
// reverse is true, the list elements are sorted as if each comparison were reversed.  On error,
// *sequence is discarded and set to an exception.
ypAPI void yp_sort3(ypObject **sequence, ypObject *key, ypObject *reverse);

// Equivalent to yp_sort3(sequence, yp_None, yp_False).
ypAPI void yp_sort(ypObject **sequence);

// When given to a slice-like start/stop C argument, signals that the default "end" value be
// substituted for the argument.  Which end depends on the sign of step:
//  - positive step: 0 substituted for start, len(s) substituted for stop
//  - negative step: len(s)-1 substituted for start, -1 substituted for stop
//  Ex: The nohtyP equivalent of "[::a]" is "yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, a"
#define yp_SLICE_DEFAULT yp_SSIZE_T_MIN

// When given to a slice-like start/stop C argument, signals that len(s) should be substituted for
// the argument.  In other words, it signals that the slice should start/stop at the end of the
// sequence.
//  Ex: The nohtyP equivalent of "[:]" is "0, yp_SLICE_USELEN, 1"
#define yp_SLICE_USELEN yp_SSIZE_T_MAX

// When an index in a slice is outside of range(-len(s),len(s)), it gets clamped to the bounds of
// the sequence.  Specifically:
//  - if i>=len(s) and k>0, or i<-len(s) and k<0, the slice is empty, regardless of j
//  - if i>=len(s) and k<0, the (reversed) slice starts with the last element
//  - if i<-len(s) and k>0, the slice starts with the first element
//  - if j>=len(s) and k<0, or j<-len(s) and k>0, the slice is empty, regardless of i
//  - if j>=len(s) and k>0, the slice ends after the last element
//  - if j<-len(s) and k<0, the (reversed) slice ends after the first element


/*
 * Set Operations
 */

// Returns the immortal yp_True if set has no elements in common with x, else yp_False.
ypAPI ypObject *yp_isdisjoint(ypObject *set, ypObject *x);

// Returns the immortal yp_True if every element in set is in x, else yp_False.
ypAPI ypObject *yp_issubset(ypObject *set, ypObject *x);

// Returns the immortal yp_True if every element in set is in x and x has additional elements, else
// yp_False.
ypAPI ypObject *yp_lt(ypObject *set, ypObject *x);

// Returns the immortal yp_True if every element in x is in set, else yp_False.
ypAPI ypObject *yp_issuperset(ypObject *set, ypObject *x);

// Returns the immortal yp_True if every element in x is in set and set has additional elements,
// else yp_False.
ypAPI ypObject *yp_gt(ypObject *set, ypObject *x);

// Returns a new reference to an object of the same type as set containing all the elements from set
// and all n objects.
ypAPI ypObject *yp_unionN(ypObject *set, int n, ...);
ypAPI ypObject *yp_unionNV(ypObject *set, int n, va_list args);

// Returns a new reference to an object of the same type as set containing all the elements common
// to the set and all n objects.
ypAPI ypObject *yp_intersectionN(ypObject *set, int n, ...);
ypAPI ypObject *yp_intersectionNV(ypObject *set, int n, va_list args);

// Returns a new reference to an object of the same type as set containing all the elements from set
// that are not in the n objects.
ypAPI ypObject *yp_differenceN(ypObject *set, int n, ...);
ypAPI ypObject *yp_differenceNV(ypObject *set, int n, va_list args);

// Returns a new reference to an object of the same type as set containing all the elements in
// either set or x but not both.
ypAPI ypObject *yp_symmetric_difference(ypObject *set, ypObject *x);

// Add the elements from the n objects to *set.  On error, *set is discarded and set to an
// exception.
ypAPI void yp_updateN(ypObject **set, int n, ...);
ypAPI void yp_updateNV(ypObject **set, int n, va_list args);

// Removes elements from *set that are not contained in all n objects.  On error, *set is discarded
// and set to an exception.
ypAPI void yp_intersection_updateN(ypObject **set, int n, ...);
ypAPI void yp_intersection_updateNV(ypObject **set, int n, va_list args);

// Removes elements from *set that are contained in any of the n objects.  On error, *set is
// discarded and set to an exception.
ypAPI void yp_difference_updateN(ypObject **set, int n, ...);
ypAPI void yp_difference_updateNV(ypObject **set, int n, va_list args);

// Removes elements from *set that are contained in x, and adds elements from x not contained in
// *set.  On error, *set is discarded and set to an exception.
ypAPI void yp_symmetric_difference_update(ypObject **set, ypObject *x);

// Adds element x to *set.  On error, *set is discarded and set to an exception.  While Python
// calls this method add, yp_add is already used for "a+b", so these two equivalent aliases are
// provided instead.
ypAPI void yp_push(ypObject **set, ypObject *x);
ypAPI void yp_set_add(ypObject **set, ypObject *x);

// If x is already contained in set, raises yp_KeyError; otherwise, adds x to set.  Sets *exc
// on error (set is never discarded).
ypAPI void yp_pushuniqueE(ypObject *set, ypObject *x, ypObject **exc);

// Removes element x from *set.  Raises yp_KeyError if x is not contained in *set.  On error,
// *set is discarded and set to an exception.
ypAPI void yp_remove(ypObject **set, ypObject *x);

// Removes element x from *set.  Does _not_ raise an exception if x is not contained in *set.  On
// error, *set is discarded and set to an exception.
ypAPI void yp_discard(ypObject **set, ypObject *x);

// Removes an arbitrary item from *set and returns a new reference to it.  On error, *set is
// discarded and set to an exception _and_ an exception is returned.  You cannot use the order
// of yp_push calls on sets to determine the order of yp_pop'ped elements.
ypAPI ypObject *yp_pop(ypObject **set);


/*
 * Mapping Operations
 */

// frozendicts and dicts are both mapping objects.  Note that yp_contains, yp_in, yp_not_in,
// and yp_iter operate solely on a mapping's keys.

// Returns a new reference to the value of mapping with the given key.  Returns yp_KeyError if key
// is not in the map.
ypAPI ypObject *yp_getitem(ypObject *mapping, ypObject *key);

// Similar to yp_getitem, but returns a new reference to defval if key is not in the map.  defval
// _can_ be an exception; the Python-equivalent "default" for defval is yp_None.
ypAPI ypObject *yp_getdefault(ypObject *mapping, ypObject *key, ypObject *defval);

// Returns a new reference to an iterator that yields mapping's (key, value) pairs as 2-tuples.
ypAPI ypObject *yp_iter_items(ypObject *mapping);

// Returns a new reference to an iterator that yields mapping's keys.
ypAPI ypObject *yp_iter_keys(ypObject *mapping);

// Returns a new reference to an iterator that yields mapping's values.
ypAPI ypObject *yp_iter_values(ypObject *mapping);

// Adds or replaces the value of *mapping with the given key, setting it to x.  On error, *mapping
// is discarded and set to an exception.
ypAPI void yp_setitem(ypObject **mapping, ypObject *key, ypObject *x);

// Removes the item with the given key from *mapping.  Raises yp_KeyError if key is not in
// *mapping.  On error, *mapping is discarded and set to an exception.
ypAPI void yp_delitem(ypObject **mapping, ypObject *key);

// If key is in mapping, remove it and return a new reference to its value, else return a new
// reference to defval.  defval _can_ be an exception: if it is, then a missing key is treated as
// an error (including discarding *mapping).  The Python-equivalent "default" of defval is
// yp_KeyError.  On error, *mapping is discarded and set to an exception _and_ that exception is
// returned.  Note that yp_push and yp_pop are not applicable for mapping objects.
ypAPI ypObject *yp_popvalue3(ypObject **mapping, ypObject *key, ypObject *defval);

// Equivalent to yp_popvalue3(mapping, key, yp_KeyError).
ypAPI ypObject *yp_popvalue2(ypObject **mapping, ypObject *key);

// Removes an arbitrary item from *mapping and returns new references to its *key and *value.  If
// mapping is empty yp_KeyError is raised.  On error, *mapping is discarded and set to an exception
// _and_ both *key and *value are set to exceptions.
ypAPI void yp_popitem(ypObject **mapping, ypObject **key, ypObject **value);

// Similar to yp_getitem, but returns a new reference to defval _and_ adds it to *mapping if key is
// not in the map.  defval cannot be an exception.  The Python-equivalent "default" for defval is
// yp_None.  On error, *mapping is discarded and set to an exception _and_ an exception is
// returned.
ypAPI ypObject *yp_setdefault(ypObject **mapping, ypObject *key, ypObject *defval);

// Add the given n key/value pairs (for a total of 2*n objects) to *mapping, overwriting existing
// keys.  If a given key is seen more than once, the last value is retained.  On error, *mapping is
// discarded and set to an exception.
ypAPI void yp_updateK(ypObject **mapping, int n, ...);
ypAPI void yp_updateKV(ypObject **mapping, int n, va_list args);

// Add the elements from the n objects to *mapping.  Each object is handled as per yp_dict.  On
// error, *mapping is discarded and set to an exception.
ypAPI void yp_updateN(ypObject **mapping, int n, ...);
ypAPI void yp_updateNV(ypObject **mapping, int n, va_list args);


/*
 * String Operations
 */

// These methods are supported by bytes and str (and their mutable counterparts, of course).
// Individual elements of bytes and bytearrays are ints, so yp_getindexC will always return ints
// for these types, and will only accept ints for yp_setindexC.  The individual elements of strs and
// chrarrays are single-character strs.  Using bytes/bytearray arguments on a str/chrarray method,
// or str/chrarray arguments on a bytes/bytearray method, raises yp_TypeError (unless otherwise
// documented).

// Slicing an object always returns an object of the same type, so yp_getsliceC4 on a bytearray
// will return a bytearray, while a slice of a str is another str, and so forth.

// Unlike Python, the arguments start/end (yp_startswithC4 et al) and i/j (yp_findC4 et al) are
// always treated as in slice notation.  Python behaves peculiarly when end<start in certain edge
// cases involving empty strings (compare "foo"[5:0].startswith("") to "foo".startswith("", 5, 0)).

// Immortal strs representing common encodings, for convience with yp_str_frombytesC4 et al.
ypAPI ypObject *const yp_s_ascii;     // "ascii"
ypAPI ypObject *const yp_s_latin_1;   // "latin-1"
ypAPI ypObject *const yp_s_utf_8;     // "utf-8"
ypAPI ypObject *const yp_s_utf_16;    // "utf-16"
ypAPI ypObject *const yp_s_utf_16be;  // "utf-16be"
ypAPI ypObject *const yp_s_utf_16le;  // "utf-16le"
ypAPI ypObject *const yp_s_utf_32;    // "utf-32"
ypAPI ypObject *const yp_s_utf_32be;  // "utf-32be"
ypAPI ypObject *const yp_s_utf_32le;  // "utf-32le"
ypAPI ypObject *const yp_s_ucs_2;     // "ucs-2"
ypAPI ypObject *const yp_s_ucs_4;     // "ucs-4"

// Immortal strs representing common string encoding error-handling schemes, for convience with
// yp_str_frombytesC4 et al.
ypAPI ypObject *const yp_s_strict;             // "strict"
ypAPI ypObject *const yp_s_replace;            // "replace"
ypAPI ypObject *const yp_s_ignore;             // "ignore"
ypAPI ypObject *const yp_s_xmlcharrefreplace;  // "xmlcharrefreplace"
ypAPI ypObject *const yp_s_backslashreplace;   // "backslashreplace"
ypAPI ypObject *const yp_s_surrogateescape;    // "surrogateescape"
ypAPI ypObject *const yp_s_surrogatepass;      // "surrogatepass"

// Returns the immortal yp_True if all characters in s are alphanumeric and there is at least one
// character, otherwise yp_False.  A character is alphanumeric if one of the following returns
// yp_True: yp_isalpha, yp_isdecimal, yp_isdigit, or yp_isnumeric.
ypAPI ypObject *yp_isalnum(ypObject *s);

// Returns the immortal yp_True if all characters in s are alphabetic and there is at least one
// character, otherwise yp_False.  Alphabetic characters are those characters defined in the
// Unicode character database as "Letter".  Note that this is different from the "Alphabetic"
// property defined in the Unicode Standard.
ypAPI ypObject *yp_isalpha(ypObject *s);

// Returns the immortal yp_True if all characters in s are decimal characters and there is at least
// one character, otherwise yp_False.  Decimal characters are those from general category "Nd".
ypAPI ypObject *yp_isdecimal(ypObject *s);

// Returns the immortal yp_True if all characters in s are digits and there is at least one
// character, otherwise yp_False.  Digit characters are those that have the property value
// Numeric_Type=Digit or Numeric_Type=Decimal.
ypAPI ypObject *yp_isdigit(ypObject *s);

// Returns the immortal yp_True if s is a valid identifier according to the Python language
// definition, otherwise yp_False.
ypAPI ypObject *yp_isidentifier(ypObject *s);

// Returns the immortal yp_True if all cased characters in s are lowercase and there is at least
// one cased character, otherwise yp_False.  Cased characters are those with a general category
// property of "Lu", "Ll", or "Lt".
ypAPI ypObject *yp_islower(ypObject *s);

// Returns the immortal yp_True if all characters in s are numeric characters and there is at
// least one character, otherwise yp_False.  Numeric characters are those that have the Unicode
// numeric value property.
ypAPI ypObject *yp_isnumeric(ypObject *s);

// Returns the immortal yp_True if all characters in s are printable or s is empty, otherwise
// yp_False.  Nonprintable characters are those characters defined in the Unicode character
// database as "Other" or "Separator", excepting space (0x20) which is considered printable.
ypAPI ypObject *yp_isprintable(ypObject *s);

// Returns the immortal yp_True if there are only whitespace characters in s and there is at least
// one character, otherwise yp_False.  Whitespace characters are those characters defined in the
// Unicode character database as "Other" or "Separator" and those with bidirectional property
// being one of "WS", "B", or "S".
ypAPI ypObject *yp_isspace(ypObject *s);

// Returns the immortal yp_True if all cased characters in s are uppercase and there is at least
// one cased character, otherwise yp_False.  Cased characters are those with a general category
// property of "Lu", "Ll", or "Lt".
ypAPI ypObject *yp_isupper(ypObject *s);

// Returns the immortal yp_True if s[start:end] starts with the specified prefix, otherwise
// yp_False.  prefix can also be a tuple of prefix strings for which to look.  If a prefix string
// is empty, returns yp_True.  yp_startswithC considers the entire string (as if start is 0 and end
// is yp_SLICE_USELEN).
// FIXME There's nothing "C" about yp_startswithC. (A linter should check "C" functions.)
ypAPI ypObject *yp_startswithC4(ypObject *s, ypObject *prefix, yp_ssize_t start, yp_ssize_t end);
ypAPI ypObject *yp_startswithC(ypObject *s, ypObject *prefix);

// Similar to yp_startswithC4, except looks for the given suffix(es) at the end of s[start:end].
ypAPI ypObject *yp_endswithC4(ypObject *s, ypObject *suffix, yp_ssize_t start, yp_ssize_t end);
ypAPI ypObject *yp_endswithC(ypObject *s, ypObject *suffix);

// Returns a new reference to a lowercased copy of s.  The lowercasing algorithm is described in
// section 3.13 of the Unicode Standard.
ypAPI ypObject *yp_lower(ypObject *s);

// Returns a new reference to an uppercased copy of s.  The uppercasing algorithm is described in
// section 3.13 of the Unicode Standard.
ypAPI ypObject *yp_upper(ypObject *s);

// Returns a new reference to a "casefolded" copy of s, for use in caseless matching.  The
// casefolding algorithm is described in section 3.13 of the Unicode Standard.
ypAPI ypObject *yp_casefold(ypObject *s);

// Returns a new reference to a copy of s with uppercase characters converted to lowercase and
// vice versa.
ypAPI ypObject *yp_swapcase(ypObject *s);

// Returns a new reference to a copy of s with its first character capitalized and the rest
// lowercased.
ypAPI ypObject *yp_capitalize(ypObject *s);

// Returns a new reference to s left-justified in a string of length width.  Padding is done using
// the specified ord_fillchar for yp_ljustC3, or a space for yp_ljustC.  A copy of s is returned if
// width is less than or equal to its length.
ypAPI ypObject *yp_ljustC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar);
ypAPI ypObject *yp_ljustC(ypObject *s, yp_ssize_t width);

// Similar to yp_ljustC3, except s is right-justified.
ypAPI ypObject *yp_rjustC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar);
ypAPI ypObject *yp_rjustC(ypObject *s, yp_ssize_t width);

// Similar to yp_ljustC3, except s is centered.
ypAPI ypObject *yp_centerC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar);
ypAPI ypObject *yp_centerC(ypObject *s, yp_ssize_t width);

// Returns a new reference to s where all tab characters are replaced by one or more spaces,
// depending on the current column and the given tabsize.  Newline and return characters reset the
// column to zero; all other characters increment the column by one regardless of how the character
// is represented when printed.  The Python-equivalent "default" for tabsize is 8.
ypAPI ypObject *yp_expandtabsC(ypObject *s, yp_ssize_t tabsize);

// Returns a new reference to a copy of s with count occurrences of substring oldsub replaced by
// newsub.  For yp_replace, or if count is -1, all occurrences are replaced.
ypAPI ypObject *yp_replaceC4(ypObject *s, ypObject *oldsub, ypObject *newsub, yp_ssize_t count);
ypAPI ypObject *yp_replace(ypObject *s, ypObject *oldsub, ypObject *newsub);

// Returns a new reference to a copy of s with leading characters removed.  The chars argument is a
// string specifying the set of characters to be removed; for yp_lstrip, or if chars is yp_None,
// this defaults to removing whitespace.
ypAPI ypObject *yp_lstrip2(ypObject *s, ypObject *chars);
ypAPI ypObject *yp_lstrip(ypObject *s);

// Similar to yp_lstrip2, except trailing characters are removed.
ypAPI ypObject *yp_rstrip2(ypObject *s, ypObject *chars);
ypAPI ypObject *yp_rstrip(ypObject *s);

// Similar to yp_lstrip2, except both leading and trailing characters are removed.
ypAPI ypObject *yp_strip2(ypObject *s, ypObject *chars);
ypAPI ypObject *yp_strip(ypObject *s);

// Returns a new reference to the concatenation of the strings in iterable, using s as the
// separator between elements.  Raises yp_TypeError if there are any non-string values.
ypAPI ypObject *yp_join(ypObject *s, ypObject *iterable);

// Equivalent to yp_join(s, yp_tupleN(n, ...)).
ypAPI ypObject *yp_joinN(ypObject *s, int n, ...);
ypAPI ypObject *yp_joinNV(ypObject *s, int n, va_list args);

// Splits s at the first occurrence of sep and returns new references to 3 objects: *part0 is the
// part before the separator, *part1 the separator itself, and *part2 the part after.  If the
// separator is not found, *part0 is a copy of s, and *part1 and *part2 are empty strings.  Sets
// all 3 ypObject**s to the same exception on error.
ypAPI void yp_partition(
        ypObject *s, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2);

// Similar to yp_partition, except s is split at the last occurrence of sep, and if the separator
// is not found then *part0 and *part1 are empty strings, and *part2 is a copy of s.
ypAPI void yp_rpartition(
        ypObject *s, ypObject *sep, ypObject **part0, ypObject **part1, ypObject **part2);

// Returns a new reference to a list of words in the string, using sep as the delimiter string.
// For yp_splitC3, only performs the leftmost splits up to maxsplit; for yp_split2, or if maxsplit
// is -1, there is no limit on the number of splits made.  If sep is yp_None this behaves as
// yp_split, otherwise consecutive delimiters are not grouped together and are deemed to delimit
// empty strings.
//  Ex: yp_split2("1,,2", ",") returns ["1", "", "2"]
ypAPI ypObject *yp_splitC3(ypObject *s, ypObject *sep, yp_ssize_t maxsplit);
ypAPI ypObject *yp_split2(ypObject *s, ypObject *sep);

// Similar to yp_splitC3, except a different splitting algorithm is used.  Runs of consecutive
// whitespace are regarded as a single separator and the result will contain no empty strings at
// the start or end if the string has leading or trailing whitespace.
//  Ex: yp_split(" 1  2   3  ") returns ["1", "2", "3"]
ypAPI ypObject *yp_split(ypObject *s);

// Similar to yp_splitC3, except only performs the rightmost splits up to maxsplit.
//  Ex: yp_rsplitC3("  1  2   3  ", yp_None, 1) returns ["  1  2", "3"]
ypAPI ypObject *yp_rsplitC3(ypObject *s, ypObject *sep, yp_ssize_t maxsplit);

// Returns a new reference to a list of lines in the string, breaking at line boundaries.  Python's
// "universal newlines" approach is used to split lines.  Line breaks are not included in the
// resulting list unless keepends is true.
ypAPI ypObject *yp_splitlines2(ypObject *s, ypObject *keepends);

// Returns a new reference to an encoded version of s (a str/chrarray) as a bytes/bytearray object.
// For yp_encode, encoding is yp_s_utf_8 and errors is yp_s_strict.
ypAPI ypObject *yp_encode3(ypObject *s, ypObject *encoding, ypObject *errors);
ypAPI ypObject *yp_encode(ypObject *s);

// Returns a new reference to an decoded version of b (a bytes/bytearray) as a str/chrarray object.
// For yp_decode, encoding is yp_s_utf_8 and errors is yp_s_strict.
ypAPI ypObject *yp_decode3(ypObject *b, ypObject *encoding, ypObject *errors);
ypAPI ypObject *yp_decode(ypObject *b);


/*
 * String Formatting Operations
 */

// The syntax of format strings can be found in Python's documentation:
//  https://docs.python.org/3/library/string.html#format-string-syntax

// Returns a new reference to the result of the string formatting operation.  s can contain literal
// text or replacement fields delimited by braces ("{" and "}").  Each replacement field contains
// the numeric index of a positional argument.
ypAPI ypObject *yp_formatN(ypObject *s, int n, ...);
ypAPI ypObject *yp_formatNV(ypObject *s, int n, va_list args);

// Similar to yp_formatN, except each replacement field contains the name of one of the n key/value
// pairs (for a total of 2*n objects).
ypAPI ypObject *yp_formatK(ypObject *s, int n, ...);
ypAPI ypObject *yp_formatKV(ypObject *s, int n, va_list args);

// Similar to yp_formatN, except each replacement field can contain either the numeric index of an
// item in sequence, or the name of a key from mapping.  Either sequence or mapping can be yp_None
// to ignore that particular argument.
// TODO Reconsider use of yp_None: perhaps expose yp_tuple_empty and yp_frozendict_empty
ypAPI ypObject *yp_format_seq_map(ypObject *s, ypObject *sequence, ypObject *mapping);

// Convenience methods for yp_format_seq_map(s, sequence, yp_None) and
// yp_format_seq_map(s, yp_None, mapping), respectively.
ypAPI ypObject *yp_format_seq(ypObject *s, ypObject *sequence);
ypAPI ypObject *yp_format_map(ypObject *s, ypObject *mapping);


/*
 * Callable Operations
 */
// XXX This section is a work-in-progress

// C functions can be wrapped up into function objects and called.  It's also possible to call
// certain other objects: for example, calling a type object constructs an object of that type.

// XXX Yes, the proper name for this is `function`

// Returns true (non-zero) if x appears callable, else false.  If this returns true, it is still
// possible that a call fails, but if it is false, calling x will always raise yp_TypeError.
// Always succeeds; if x is an exception false is returned.
// TODO Python just calls this "callable()"
ypAPI int yp_iscallableC(ypObject *x);

// Calls c with n positional arguments, returning the result of the call (which may be a new
// reference or an exception).  Returns yp_TypeError if c is not callable.
ypAPI ypObject *yp_callN(ypObject *c, int n, ...);
ypAPI ypObject *yp_callNV(ypObject *c, int n, va_list args);

// Similar to yp_callN, except the arguments are stored in an array. args cannot be modified until
// yp_call_array returns (FIXME do I need to say this?).
// FIXME Document the optimization here?
ypAPI ypObject *yp_call_array(ypObject *c, yp_ssize_t n, ypObject *const *args);

// Calls c with positional arguments from args and keyword arguments from kwargs, returning the
// result of the call (which may be a new reference or an exception).  Returns yp_TypeError if c is
// not callable.  Equivalent to c(*args, **kwargs) in Python (hence the name "stars").
// TODO Expose yp_tuple_empty and yp_frozendict_empty.
ypAPI ypObject *yp_call_stars(ypObject *c, ypObject *args, ypObject *kwargs);

// FIXME Stay consistent: https://docs.python.org/3/library/inspect.html#inspect.signature


// Describes one parameter of a function object.
typedef struct _yp_function_parameter_t {
    // The name of the parameter as a str (perhaps created via yp_IMMORTAL_STR_LATIN_1).  The name
    // must be a valid Python identifier.
    // FIXME Also verify that the name is a valid Python identifier.
    // FIXME Also /, *, *args, or **kwargs...so "name" may not be strictly correct.

    // Name can be /, in which case the preceeding parameters are positional-only. / cannot be
    // first. If / is in the middle, the corresponding argarray element will be NULL. If / is last,
    // it is not included in argarray: thus, n will be one less than the number of parameters.

    // FIXME test /-at-end behaviour thoroughly.

    // Name can be *, in which case subsequent parameters are keyword-only. If * is first or in the
    // middle, the corresponding argarray element will be NULL. * cannot be last.
    ypObject *name;

    // The default value for the parameter, or NULL if there is no default value.
    ypObject *default_;

    // FIXME Annotation...or at least a placeholder for that (if we are inlined, we can't grow).
} yp_function_parameter_t;

// The signature of one or more C functions that can be wrapped up in a function object.
// TODO Rename?  Maybe impl?  Or funcC?
// TODO Rename other func_t to code_t or funcC_t?  i.e. Does "code" now mean a C function?
typedef struct _yp_function_definition_t {
    // FIXME valueerror if sizeof isn't a minimum?
    // FIXME ...or just remove, because I don't track this for other immortals.
    //     yp_ssize_t sizeof_struct;  // Set to sizeof(yp_function_definition_t) on allocation

    // FIXME Flags to describe what's next in this struct (is it NV, stars, bytecode? Are there
    // annotations?)

    // Called by the yp_call* methods. c is the callable (i.e. the first argument to yp_call*).
    // argarray is an array of n arguments, where argarray[i] corresponds to parameters[i]. The
    // return value must be a new or immortal reference, or an exception.

    // FIXME Specify what happens to keyword-only, *args, and **kwargs.
    // FIXME use yp_function_stateCX to retrieve any state variables
    // FIXME "if n is zero, argarray is NULL"
    // FIXME argarray is read-only: do not modify.
    // FIXME "If exceptions were passed to yp_call*, argarray _may_ contain those exceptions." This
    // is an optimization so that chains of function calls aren't continuously checking.
    ypObject *(*code)(ypObject *c, yp_ssize_t n, ypObject *const *argarray);

    // FIXME doc, name/qualname, state, return annotation, module....

    // TODO Insert new parameters above (flags imply where the offset is)
    // FIXME We need a length here, but in the object that length is stored in ob_len....
    yp_function_parameter_t parameters[];
} yp_function_definition_t;

// TODO Yup, python allows any iterable for f(*arg) calls, just as it does for unpacking.
// TODO In Python, when calling a function, * can be any iterable and ** any mapping.  However,
// when the arguments are passed to `def a(*p, **k)`, p is _always_ a tuple and k _always_ a dict.
// TODO However, unlike Python, use a frozendict for kwargs.

// TODO yp_function_fromstructCN, or maybe yp_def_fromstructCN?

ypAPI ypObject *const yp_func_chr;
ypAPI ypObject *const yp_func_hash;
ypAPI ypObject *const yp_func_len;
ypAPI ypObject *const yp_func_reversed;


/*
 * Numeric Operations
 */

// The numeric types include ints and floats (and their mutable counterparts, of course).

// Each of these methods return new reference(s) to the result of the given numeric operation;
// for example, yp_add returns the result of adding x and y together.  If the given operands do not
// support the operation, yp_TypeError is returned.  Additional notes:
//  - yp_divmod returns two objects via *div and *mod; on error, they are both set to an exception
//  - If z is yp_None, yp_pow3 returns x to the power y, otherwise x to the power y modulo z
//  - To avoid confusion with the logical operators of the same name, yp_amp implements bitwise
//  and, while yp_bar implements bitwise or
//  - Bitwise operations (lshift/rshift/amp/xor/bar/invert) are only applicable to integers
//  - Unlike Python, non-numeric types do not (currently) overload these operators
ypAPI ypObject *yp_add(ypObject *x, ypObject *y);
ypAPI ypObject *yp_sub(ypObject *x, ypObject *y);
ypAPI ypObject *yp_mul(ypObject *x, ypObject *y);
ypAPI ypObject *yp_truediv(ypObject *x, ypObject *y);
ypAPI ypObject *yp_floordiv(ypObject *x, ypObject *y);
ypAPI ypObject *yp_mod(ypObject *x, ypObject *y);
ypAPI void      yp_divmod(ypObject *x, ypObject *y, ypObject **div, ypObject **mod);
ypAPI ypObject *yp_pow(ypObject *x, ypObject *y);
ypAPI ypObject *yp_pow3(ypObject *x, ypObject *y, ypObject *z);
ypAPI ypObject *yp_neg(ypObject *x);
ypAPI ypObject *yp_pos(ypObject *x);
ypAPI ypObject *yp_abs(ypObject *x);
ypAPI ypObject *yp_lshift(ypObject *x, ypObject *y);
ypAPI ypObject *yp_rshift(ypObject *x, ypObject *y);
ypAPI ypObject *yp_amp(ypObject *x, ypObject *y);
ypAPI ypObject *yp_xor(ypObject *x, ypObject *y);
ypAPI ypObject *yp_bar(ypObject *x, ypObject *y);
ypAPI ypObject *yp_invert(ypObject *x);

// In-place versions of the above; if the object *x can be modified to hold the result, it is,
// otherwise *x is discarded and replaced with the result.  If *x is immutable on input, an
// immutable object is returned, otherwise a mutable object is returned.  On error, *x is
// discarded and set to an exception.
ypAPI void yp_iadd(ypObject **x, ypObject *y);
ypAPI void yp_isub(ypObject **x, ypObject *y);
ypAPI void yp_imul(ypObject **x, ypObject *y);
ypAPI void yp_itruediv(ypObject **x, ypObject *y);
ypAPI void yp_ifloordiv(ypObject **x, ypObject *y);
ypAPI void yp_imod(ypObject **x, ypObject *y);
ypAPI void yp_ipow(ypObject **x, ypObject *y);
ypAPI void yp_ipow3(ypObject **x, ypObject *y, ypObject *z);
ypAPI void yp_ineg(ypObject **x);
ypAPI void yp_ipos(ypObject **x);
ypAPI void yp_iabs(ypObject **x);
ypAPI void yp_ilshift(ypObject **x, ypObject *y);
ypAPI void yp_irshift(ypObject **x, ypObject *y);
ypAPI void yp_iamp(ypObject **x, ypObject *y);
ypAPI void yp_ixor(ypObject **x, ypObject *y);
ypAPI void yp_ibar(ypObject **x, ypObject *y);
ypAPI void yp_iinvert(ypObject **x);

// Versions of yp_iadd et al that accept a C integer as the second argument.  Remember that *x may
// be discarded and replaced with the result.
ypAPI void yp_iaddC(ypObject **x, yp_int_t y);
ypAPI void yp_isubC(ypObject **x, yp_int_t y);
ypAPI void yp_imulC(ypObject **x, yp_int_t y);
ypAPI void yp_itruedivC(ypObject **x, yp_int_t y);
ypAPI void yp_ifloordivC(ypObject **x, yp_int_t y);
ypAPI void yp_imodC(ypObject **x, yp_int_t y);
ypAPI void yp_ipowC(ypObject **x, yp_int_t y);
ypAPI void yp_ipowC3(ypObject **x, yp_int_t y, yp_int_t z);
ypAPI void yp_ilshiftC(ypObject **x, yp_int_t y);
ypAPI void yp_irshiftC(ypObject **x, yp_int_t y);
ypAPI void yp_iampC(ypObject **x, yp_int_t y);
ypAPI void yp_ixorC(ypObject **x, yp_int_t y);
ypAPI void yp_ibarC(ypObject **x, yp_int_t y);

// Versions of yp_iadd et al that accept a C floating-point as the second argument.  Remember that
// *x may be discarded and replaced with the result.
ypAPI void yp_iaddCF(ypObject **x, yp_float_t y);
ypAPI void yp_isubCF(ypObject **x, yp_float_t y);
ypAPI void yp_imulCF(ypObject **x, yp_float_t y);
ypAPI void yp_itruedivCF(ypObject **x, yp_float_t y);
ypAPI void yp_ifloordivCF(ypObject **x, yp_float_t y);
ypAPI void yp_imodCF(ypObject **x, yp_float_t y);
ypAPI void yp_ipowCF(ypObject **x, yp_float_t y);

// Library routines for nohtyP integer operations on C types.  Returns zero and sets *exc on error.
// Additional notes:
//  - yp_truedivL returns a floating-point number
//  - If z is 0, yp_powL3 returns x to the power y, otherwise x to the power y modulo z
//  - If y is negative, yp_powL and yp_powL3 raise yp_ValueError, as the result should be a
//  floating-point number; use yp_powLF for negative exponents instead
ypAPI yp_int_t   yp_addL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_subL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_mulL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_float_t yp_truedivL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_floordivL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_modL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI void       yp_divmodL(yp_int_t x, yp_int_t y, yp_int_t *div, yp_int_t *mod, ypObject **exc);
ypAPI yp_int_t   yp_powL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_powL3(yp_int_t x, yp_int_t y, yp_int_t z, ypObject **exc);
ypAPI yp_int_t   yp_negL(yp_int_t x, ypObject **exc);
ypAPI yp_int_t   yp_posL(yp_int_t x, ypObject **exc);
ypAPI yp_int_t   yp_absL(yp_int_t x, ypObject **exc);
ypAPI yp_int_t   yp_lshiftL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_rshiftL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_ampL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_xorL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_barL(yp_int_t x, yp_int_t y, ypObject **exc);
ypAPI yp_int_t   yp_invertL(yp_int_t x, ypObject **exc);

// Library routines for nohtyP floating-point operations on C types.  Returns zero and sets *exc
// on error.
ypAPI yp_float_t yp_addLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI yp_float_t yp_subLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI yp_float_t yp_mulLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI yp_float_t yp_truedivLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI yp_float_t yp_floordivLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI yp_float_t yp_modLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI void       yp_divmodLF(
              yp_float_t x, yp_float_t y, yp_float_t *div, yp_float_t *mod, ypObject **exc);
ypAPI yp_float_t yp_powLF(yp_float_t x, yp_float_t y, ypObject **exc);
ypAPI yp_float_t yp_negLF(yp_float_t x, ypObject **exc);
ypAPI yp_float_t yp_posLF(yp_float_t x, ypObject **exc);
ypAPI yp_float_t yp_absLF(yp_float_t x, ypObject **exc);

// Conversion routines from C types or objects to C types.  Returns a reasonable value and sets
// *exc on error; "reasonable" usually means "truncated".  Converting a float to an int truncates
// toward zero but is not an error.
ypAPI yp_int_t     yp_asintC(ypObject *x, ypObject **exc);
ypAPI yp_int8_t    yp_asint8C(ypObject *x, ypObject **exc);
ypAPI yp_uint8_t   yp_asuint8C(ypObject *x, ypObject **exc);
ypAPI yp_int16_t   yp_asint16C(ypObject *x, ypObject **exc);
ypAPI yp_uint16_t  yp_asuint16C(ypObject *x, ypObject **exc);
ypAPI yp_int32_t   yp_asint32C(ypObject *x, ypObject **exc);
ypAPI yp_uint32_t  yp_asuint32C(ypObject *x, ypObject **exc);
ypAPI yp_int64_t   yp_asint64C(ypObject *x, ypObject **exc);
ypAPI yp_uint64_t  yp_asuint64C(ypObject *x, ypObject **exc);
ypAPI yp_float_t   yp_asfloatC(ypObject *x, ypObject **exc);
ypAPI yp_float32_t yp_asfloat32C(ypObject *x, ypObject **exc);
ypAPI yp_float64_t yp_asfloat64C(ypObject *x, ypObject **exc);
ypAPI yp_ssize_t   yp_asssizeC(ypObject *x, ypObject **exc);
ypAPI yp_hash_t    yp_ashashC(ypObject *x, ypObject **exc);
ypAPI yp_float_t   yp_asfloatL(yp_int_t x, ypObject **exc);
ypAPI yp_int_t     yp_asintLF(yp_float_t x, ypObject **exc);

// Return a new reference to x rounded to ndigits after the decimal point.
ypAPI ypObject *yp_roundC(ypObject *x, int ndigits);

// Sums the n given objects and returns the total.
ypAPI ypObject *yp_sumN(int n, ...);
ypAPI ypObject *yp_sumNV(int n, va_list args);

// Sums the items of iterable and returns the total.
ypAPI ypObject *yp_sum(ypObject *iterable);

// Return the number of bits necessary to represent an integer in binary, excluding the sign and
// leading zeroes.  Returns zero and sets *exc on error.
ypAPI yp_int_t yp_int_bit_lengthC(ypObject *x, ypObject **exc);

// The maximum and minimum integer values, as immortal objects.
ypAPI ypObject *const yp_sys_maxint;
ypAPI ypObject *const yp_sys_minint;

// Immortal ints representing common values, for convenience.
// TODO Rename to yp_int_*?  I'm OK with yp_s_* because strs are going to be used more often and
// will likely have long names already (i.e. they'll be named like the string they represent), but
// there won't be many of these, their names are short, and they're infrequently used.
ypAPI ypObject *const yp_i_neg_one;
ypAPI ypObject *const yp_i_zero;
ypAPI ypObject *const yp_i_one;
ypAPI ypObject *const yp_i_two;


/*
 * Freezing, "Unfreezing", and Invalidating
 */

// All objects support a one-way freeze method that makes them immutable, an unfrozen_copy method
// that returns a mutable copy, and a one-way invalidate method that renders the object useless;
// there are also deep variants to these methods. Supplying an invalidated object to a function
// raises yp_InvalidatedError. Invalidating an object stored in a container may cause some
// operations on that container to raise yp_InvalidatedError. Freezing and invalidating are two
// examples of object transmutation, where the type of the object is converted to a different type.
// Unlike Python, most objects are copied in memory, even immutables, as copying is one strategy to
// maintain threadsafety.

// Transmutes *x to its associated immutable type.  If *x is already immutable this is a no-op.
ypAPI void yp_freeze(ypObject **x);

// Freezes *x and, recursively, all contained objects.
ypAPI void yp_deepfreeze(ypObject **x);

// Returns a new reference to a mutable shallow copy of x.  If x has no associated mutable type an
// immutable copy is returned.
ypAPI ypObject *yp_unfrozen_copy(ypObject *x);

// Creates a mutable copy of x and, recursively, all contained objects, returning a new reference.
ypAPI ypObject *yp_unfrozen_deepcopy(ypObject *x);

// Returns a new reference to an immutable shallow copy of x.  If x has no associated immutable
// type a new, invalidated object is returned.
ypAPI ypObject *yp_frozen_copy(ypObject *x);

// Creates an immutable copy of x and, recursively, all contained objects, returning a new
// reference.
ypAPI ypObject *yp_frozen_deepcopy(ypObject *x);

// Returns a new reference to an exact, shallow copy of x.
ypAPI ypObject *yp_copy(ypObject *x);

// Creates an exact copy of x and, recursively, all contained objects, returning a new reference.
ypAPI ypObject *yp_deepcopy(ypObject *x);

// Discards all contained objects in *x, deallocates _some_ memory, and transmutes it to the
// ypInvalidated type (rendering the object useless).  If *x is immortal or already invalidated
// this is a no-op; immutable objects _can_ be invalidated.
ypAPI void yp_invalidate(ypObject **x);

// Invalidates *x and, recursively, all contained objects.  As nohtyP does not currently detect
// reference cycles during garbage collection, this is an effective way to break cycles and free
// memory.
ypAPI void yp_deepinvalidate(ypObject **x);


/*
 * Type Operations
 */

// Returns a new reference to the type of object.  If object is an exception, yp_t_exception is
// returned; if it is invalidated, yp_t_invalidated is returned.
// TODO Reconsider the behaviour of exceptions.  Python returns `type`.  If we ever want to support
// creating instances of exceptions, we should do the same.
ypAPI ypObject *yp_type(ypObject *object);

// The immortal type objects.  Calling a type object (i.e. yp_call_TODO) constructs an object of
// that type.
// FIXME yp_t_* could also be for tuples. Perhaps change to yp_type_*?
ypAPI ypObject *const yp_t_invalidated;
ypAPI ypObject *const yp_t_exception;  // FIXME Rename to yp_t_BaseException?
ypAPI ypObject *const yp_t_type;
ypAPI ypObject *const yp_t_NoneType;
ypAPI ypObject *const yp_t_bool;
ypAPI ypObject *const yp_t_int;
ypAPI ypObject *const yp_t_intstore;
ypAPI ypObject *const yp_t_float;
ypAPI ypObject *const yp_t_floatstore;
ypAPI ypObject *const yp_t_iter;
ypAPI ypObject *const yp_t_bytes;
ypAPI ypObject *const yp_t_bytearray;
ypAPI ypObject *const yp_t_str;
ypAPI ypObject *const yp_t_chrarray;
ypAPI ypObject *const yp_t_tuple;
ypAPI ypObject *const yp_t_list;
ypAPI ypObject *const yp_t_frozenset;
ypAPI ypObject *const yp_t_set;
ypAPI ypObject *const yp_t_frozendict;
ypAPI ypObject *const yp_t_dict;
ypAPI ypObject *const yp_t_range;
ypAPI ypObject *const yp_t_function;


/*
 * Mini Iterators
 */

// The "mini iterator" API is an alternative to the standard iterator API (yp_iter et al) that can
// in most cases avoid allocating a separate iterator object.  This makes it the preferred API to
// loop over an iterable, although there are restrictions: the mini iterator returned by yp_miniiter
// is opaque and can only be used with the yp_miniiter_* methods and yp_decref.  So, as an example,
// if you need to store an iterator in a list, stick with yp_iter.
//
// The restrictions on how mini iterators are used means you'll typically write the following:
//      yp_uint64_t mi_state;
//      ypObject *mi = yp_miniiter(list, &mi_state);
//      while(1) {
//          ypObject *item = yp_miniiter_next(&mi, &mi_state);
//          if(yp_isexceptionC2(item, yp_StopIteration)) break;
//          // ... operate on item ...
//          yp_decref(item);
//      }
//      yp_decref(mi);
//
// All iterables can be used with yp_miniiter, even if they do not specifically support the mini
// iterator protocol.  Additionally, types that do support the protocol may yet allocate a separate
// object under certain conditions (if a yp_uint64_t is too small to hold the necessary state,
// perhaps).  It is for this reason that mini iterator objects be treated as opaque: the type of the
// returned object may not be consistent.  These restrictions on using mini iterators are not
// enforced: the behaviour of a mini iterator with any other other function is undefined and may or
// may not raise an exception.

// FIXME Would this be clearer if mini iterators were opaque structures containing obj and state?
// Yes, yes it would.

// FIXME Now that iterators are considered "immutable", should we discard it on _next/etc? i.e.
// should we turn ypObject **iterator into just ypObject *iterator?

// Returns a new reference to an opaque mini iterator for object x and initializes *state to the
// iterator's starting state.  *state is also opaque: you must *not* modify it directly.  It is
// usually unwise to modify an object being iterated over.
ypAPI ypObject *yp_miniiter(ypObject *x, yp_uint64_t *state);

// Returns a new reference to the next yielded value from the mini iterator, or an exception.
// state must point to the same data returned by the previous yp_miniiter* call.  When the mini
// iterator is exhausted yp_StopIteration is returned; on any other error, *mi is discarded and set
// to an exception, _and_ an exception is returned.
ypAPI ypObject *yp_miniiter_next(ypObject **mi, yp_uint64_t *state);

// Returns a hint as to how many items are left to be yielded.  See yp_length_hintC for
// additional information.  state must point to the same data returned by the previous yp_miniiter*
// call.  Returns zero and sets *exc on error.
ypAPI yp_ssize_t yp_miniiter_length_hintC(ypObject *mi, yp_uint64_t *state, ypObject **exc);

// Mini iterator versions of yp_iter_keys and yp_iter_values.  Otherwise behaves as yp_miniiter.
ypAPI ypObject *yp_miniiter_keys(ypObject *x, yp_uint64_t *state);
ypAPI ypObject *yp_miniiter_values(ypObject *x, yp_uint64_t *state);

// A mini iterator version of yp_iter_items.  Otherwise behaves as yp_miniiter.  While
// yp_miniiter_next can be used, yp_miniiter_items_next is recommended to avoid allocating a new
// tuple for each key/value pair.
ypAPI ypObject *yp_miniiter_items(ypObject *x, yp_uint64_t *state);

// Returns new references to the next yielded key/value pair from the mini iterator, or an
// exception.  state must point to the same data returned by the previous yp_miniiter* call.  Only
// applicable for mini iterators returned by yp_miniiter_items, otherwise yp_TypeError is raised.
// When the mini iterator is exhausted both *key and *value are set to yp_StopIteration; on any
// other error, *mi is discarded and set to an exception, _and_ both *key and *value are set to
// exceptions.
ypAPI void yp_miniiter_items_next(
        ypObject **mi, yp_uint64_t *state, ypObject **key, ypObject **value);


/*
 * C-to-C Container Operations
 */

// When working with containers, it can be convenient to perform operations using C types rather
// than dealing with reference counting.  These functions provide shortcuts for common operations
// when dealing with containers.  Keep in mind, though, that many of these functions create
// short-lived objects internally, so excessive use may impact execution time.

// For functions that deal with strs, if encoding is missing yp_s_utf_8 (which is compatible
// with ascii) is assumed, while if errors is missing yp_s_strict is assumed.  yp_*_containsC
// returns false and sets *exc on exception.

// Operations on containers that map objects to integers
ypAPI int      yp_o2i_containsC(ypObject *container, yp_int_t x, ypObject **exc);
ypAPI void     yp_o2i_pushC(ypObject **container, yp_int_t x);
ypAPI yp_int_t yp_o2i_popC(ypObject **container, ypObject **exc);
ypAPI yp_int_t yp_o2i_getitemC(ypObject *container, ypObject *key, ypObject **exc);
ypAPI void     yp_o2i_setitemC(ypObject **container, ypObject *key, yp_int_t x);

// Operations on containers that map objects to strs
// yp_o2s_getitemCX is documented below, as it must be used carefully.
ypAPI void yp_o2s_setitemC4(
        ypObject **container, ypObject *key, const yp_uint8_t *x, yp_ssize_t x_len);

// Operations on containers that map integers to objects.  Note that if the container is known
// at compile-time to be a sequence, then yp_getindexC et al are better choices.
ypAPI ypObject *yp_i2o_getitemC(ypObject *container, yp_int_t key);
ypAPI void      yp_i2o_setitemC(ypObject **container, yp_int_t key, ypObject *x);

// Operations on containers that map integers to integers
ypAPI yp_int_t yp_i2i_getitemC(ypObject *container, yp_int_t key, ypObject **exc);
ypAPI void     yp_i2i_setitemC(ypObject **container, yp_int_t key, yp_int_t x);

// Operations on containers that map integers to strs
// yp_i2s_getitemCX is documented below, as it must be used carefully.
// FIXME yp_uint8_t* is confusing; make a yp_char_t, or yp_str_t (a pointer typedef...but we avoid
// those)? Or stick with C-standard char, warts and all.
ypAPI void yp_i2s_setitemC4(
        ypObject **container, yp_int_t key, const yp_uint8_t *x, yp_ssize_t x_len);

// Operations on containers that map strs to objects.  Note that if the value of the str is
// known at compile-time, as in:
//      value = yp_s2o_getitemC3(o, "mykey", -1);
// it is more-efficient to use yp_IMMORTAL_STR_LATIN_1 (also compatible with ascii), as in:
//      yp_IMMORTAL_STR_LATIN_1(s_mykey, "mykey");
//      value = yp_getitem(o, s_mykey);
ypAPI ypObject *yp_s2o_getitemC3(ypObject *container, const yp_uint8_t *key, yp_ssize_t key_len);
ypAPI void      yp_s2o_setitemC4(
             ypObject **container, const yp_uint8_t *key, yp_ssize_t key_len, ypObject *x);

// Operations on containers that map strs to integers
ypAPI yp_int_t yp_s2i_getitemC3(
        ypObject *container, const yp_uint8_t *key, yp_ssize_t key_len, ypObject **exc);
ypAPI void yp_s2i_setitemC4(
        ypObject **container, const yp_uint8_t *key, yp_ssize_t key_len, yp_int_t x);


/*
 * Immortal "Constructor" Macros
 */

// Defines an immortal int object at compile-time, which can be accessed by the variable name,
// which is of type "ypObject * const".  value is a (constant) yp_int_t.  To be used as:
//      yp_IMMORTAL_INT(name, value);

// Defines an immortal bytes object at compile-time, which can be accessed by the variable name,
// which is of type "ypObject * const".  value is a C string literal that can contain null bytes.
// The length is calculated while compiling; the hash will be calculated the first time it is
// accessed.  To be used as:
//      yp_IMMORTAL_BYTES(name, value);

// Defines an immortal str constant at compile-time, which can be accessed by the variable name,
// which is of type "ypObject * const".  value is a latin-1 encoded C string literal that can
// contain null characters.  The length is calculated while compiling; the hash will be calculated
// the first time it is accessed.  Note that this also accepts an ascii-encoded C string literal,
// as ascii is a subset of latin-1.
//      yp_IMMORTAL_STR_LATIN_1(name, value);

// The default immortal "constructor" macros declare variables as "ypObject * const".  This means
// immortals defined outside of a function will be extern.  It also means you should *not*
// use these macros in a function, as the variable will be "deallocated" when the function returns,
// and immortals should never be deallocated.  The following macros work as above, except the
// variables are declared as "static ypObject * const".
// FIXME Rename yp_IMMORTAL_INT to yp_IMMORTAL_INT_extern for clarity and consistency.
//      yp_IMMORTAL_INT_static(name, value);
//      yp_IMMORTAL_BYTES_static(name, value);
//      yp_IMMORTAL_STR_LATIN_1_static(name, value);


/*
 * Exceptions
 */

// All exception objects are immortal and, as such, do not need to be yp_decref'ed; this allows you
// to return immediately if yp_isexceptionC returns true.

// The exception objects that have direct Python counterparts.
ypAPI ypObject *const yp_BaseException;
ypAPI ypObject *const yp_Exception;
ypAPI ypObject *const yp_StopIteration;
ypAPI ypObject *const yp_GeneratorExit;
ypAPI ypObject *const yp_ArithmeticError;
ypAPI ypObject *const yp_LookupError;
ypAPI ypObject *const yp_AssertionError;
ypAPI ypObject *const yp_AttributeError;
ypAPI ypObject *const yp_EOFError;
ypAPI ypObject *const yp_FloatingPointError;
ypAPI ypObject *const yp_OSError;
ypAPI ypObject *const yp_ImportError;
ypAPI ypObject *const yp_IndexError;
ypAPI ypObject *const yp_KeyError;
ypAPI ypObject *const yp_KeyboardInterrupt;
ypAPI ypObject *const yp_MemoryError;
ypAPI ypObject *const yp_NameError;
ypAPI ypObject *const yp_OverflowError;
ypAPI ypObject *const yp_RuntimeError;
ypAPI ypObject *const yp_NotImplementedError;
ypAPI ypObject *const yp_ReferenceError;
ypAPI ypObject *const yp_SystemError;
ypAPI ypObject *const yp_SystemExit;
ypAPI ypObject *const yp_TypeError;
ypAPI ypObject *const yp_UnboundLocalError;
ypAPI ypObject *const yp_UnicodeError;
ypAPI ypObject *const yp_UnicodeEncodeError;
ypAPI ypObject *const yp_UnicodeDecodeError;
ypAPI ypObject *const yp_UnicodeTranslateError;
ypAPI ypObject *const yp_ValueError;
ypAPI ypObject *const yp_ZeroDivisionError;
ypAPI ypObject *const yp_BufferError;

// Raised when the object does not support the given method; subexception of yp_AttributeError.
ypAPI ypObject *const yp_MethodError;
// Raised when an allocation size calculation overflows; subexception of yp_MemoryError.
ypAPI ypObject *const yp_MemorySizeOverflowError;
// Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError.
ypAPI ypObject *const yp_SystemLimitationError;
// Raised when an invalidated object is passed to a function; subexception of yp_TypeError.
ypAPI ypObject *const yp_InvalidatedError;

// Returns true (non-zero) if x is an exception that matches exc, else false.  This takes into
// account the exception heirarchy, so is the preferred way to test for specific exceptions.  Always
// succeeds.
ypAPI int yp_isexceptionC2(ypObject *x, ypObject *exc);

// A convenience function to compare x against n possible exceptions.  Returns false if n is zero.
ypAPI int yp_isexceptionCN(ypObject *x, int n, ...);
ypAPI int yp_isexceptionCNV(ypObject *x, int n, va_list args);


/*
 * Mutability and Hashability
 */

// In nohtyP, just as in Python, an object is considered immutable if the portion of data involved
// in calculating its hash value does not change after creation. However, it is permissable for that
// object to contain additional data that _can_ change after creation.
//
// The clearest example of this is the reference count, which nohtyP stores in the object itself.
// While reference counts are mutable, they are not involved in the calculation of any hash value,
// making it permissible to change the reference count of an immutable object.
//
// This is why types such as iterators, functions, and files are considered immutable, despite
// containing mostly mutable data: their hash is based solely on their identity, the only portion of
// data that doesn't change.


/*
 * Initialization Parameters
 */

// yp_initialize accepts a number of parameters to customize nohtyP behaviour.
// XXX Offsets will not change between versions: members from this struct will never be deleted,
// only deprecated.
typedef struct _yp_initialize_parameters_t {
    yp_ssize_t sizeof_struct;  // Set to sizeof(yp_initialize_parameters_t) on allocation

    // yp_malloc, yp_malloc_resize, and yp_free allow you to specify custom memory allocation APIs.
    // It is recommended to set these to NULL to use nohtyP's internal defaults.  Any functions you
    // supply should behave exactly as documented, and you are encouraged to run the full suite of
    // tests with your API.  (See _default_yp_malloc et al in nohtyP.c for examples.)

    // Allocates at least size bytes of memory, setting *actual to the actual amount of memory
    // allocated, and returning the pointer to the buffer.  On error, returns NULL, and *actual is
    // undefined.  This must succeed when size==0; the behaviour is undefined when size<0.
    // XXX It's recommended that negative sizes abort in debug builds to catch overflow errors.
    void *(*yp_malloc)(yp_ssize_t *actual, yp_ssize_t size);

    // Resizes the given buffer in-place if possible, otherwise allocates a new buffer.  There are
    // three possible scenarios:
    //  - On error, returns NULL, p is not freed, and *actual is undefined
    //  - On successful in-place resize, returns p, and *actual is the amount of memory now
    //  allocated by p
    //  - Otherwise, returns a pointer to the new buffer, p is not freed, and *actual is the amount
    //  of memory allocated to the new buffer; nohtyP will then copy the data and call yp_free(p)
    // The resized/new buffer will be at least size bytes; extra is a hint as to how much the
    // buffer should be over-allocated, which may be ignored.  This must succeed when size==0 or
    // extra==0; the behaviour is undefined when size<0 or extra<0.
    // XXX Unlike realloc, this *never* copies to the new buffer and *never* frees the old buffer.
    // XXX It's recommended that negative sizes abort in debug builds to catch overflow errors.
    void *(*yp_malloc_resize)(yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra);

    // Frees memory returned by yp_malloc and yp_malloc_resize.  May abort on error.
    void (*yp_free)(void *p);

    // Setting everything_immortal to true forces all allocated objects to be immortal, effectively
    // disabling yp_incref and yp_decref.  When false, the default and recommended option, objects
    // are deallocated when their reference count reaches zero.  Setting this option will leak
    // considerable amounts of memory, but if the number of objects allocated by your program is
    // bounded you may notice a small performance improvement.
    // XXX Use this option carefully, and profile to ensure it actually provides a benefit!
    int everything_immortal;

} yp_initialize_parameters_t;


/*
 * Direct Object Memory Access
 */

// XXX The "X" in these names is a reminder that the function is returning internal memory, and
// as such should be used with caution.

// Typically only called from within yp_generator_func_t functions.  Sets *state and *size to the
// internal iterator state buffer and its size in bytes, and returns the immortal yp_None.  The
// structure and initial values of *state are determined by the call to iterator's constructor; the
// function cannot change the size after creation, and any ypObject*s in *state should be considered
// *borrowed* (it is safe to replace them with new or immortal references).  Sets *state to NULL,
// *size to zero, and returns an exception on error.
ypAPI ypObject *yp_iter_stateCX(ypObject *iterator, void **state, yp_ssize_t *size);

// For sequences that store their elements as an array of bytes (bytes and bytearray), sets *bytes
// to the beginning of that array, *len to the length of the sequence, and returns the immortal
// yp_None.  *bytes will point into internal object memory which MUST NOT be modified; furthermore,
// the sequence itself must not be modified while using the array.  As a special case, if len is
// NULL, the sequence must not contain null bytes and *bytes will point to a null-terminated array.
// Sets *bytes to NULL, *len to zero (if len is not NULL), and returns an exception on error.
ypAPI ypObject *yp_asbytesCX(ypObject *seq, const yp_uint8_t **bytes, yp_ssize_t *len);

// str and chrarray internally store their Unicode characters in particular encodings, usually
// depending on the contents of the string.  This function sets *encoded to the beginning of that
// data, *size to the number of bytes in encoded, and *encoding to the immortal str representing
// the encoding used (yp_s_latin_1, perhaps).  *encoded will point into internal object memory
// which MUST NOT be modified; furthermore, the string itself must not be modified while using the
// array.  As a special case, if size is NULL, the string must not contain null characters and
// *encoded will point to a null-terminated string.  On error, sets *encoded to NULL, *size to
// zero (if size is not NULL), *encoding to the exception, and returns the exception.
ypAPI ypObject *yp_asencodedCX(
        ypObject *seq, const yp_uint8_t **encoded, yp_ssize_t *size, ypObject **encoding);

// For sequences that store their elements as an array of pointers to ypObjects (list and tuple),
// sets *array to the beginning of that array, *len to the length of the sequence, and returns the
// immortal yp_None.  *array will point into internal object memory, so they are *borrowed*
// references and MUST NOT be replaced; furthermore, the sequence itself must not be modified while
// using the array.  Sets *array to NULL, *len to zero, and returns an exception on error.
ypAPI ypObject *yp_itemarrayCX(ypObject *seq, ypObject *const **array, yp_ssize_t *len);

// For tuples, lists, dicts, and frozendicts, this is equivalent to:
//  yp_asencodedCX(yp_getitem(container, key), encoded, size, encoding)
// For all other types, this raises yp_TypeError, and sets the outputs accordingly.  *encoded
// will point into internal object memory which MUST NOT be modified; furthermore, the str
// itself must neither be modified nor removed from the container while using the array.
ypAPI ypObject *yp_o2s_getitemCX(ypObject *container, ypObject *key, const yp_uint8_t **encoded,
        yp_ssize_t *size, ypObject **encoding);

// Similar to yp_o2s_getitemCX, except key is a yp_int_t.  *encoded will point into internal object
// memory which MUST NOT be modified; furthermore, the str itself must neither be modified nor
// removed from the container while using the array.
ypAPI ypObject *yp_i2s_getitemCX(ypObject *container, yp_int_t key, const yp_uint8_t **encoded,
        yp_ssize_t *size, ypObject **encoding);


/*
 * Optional Macros
 *
 * These macros may make working with nohtyP easier, but are not required.  They are best described
 * by the examples in ypExamples.c, but are documented below.
 */

#ifdef yp_FUTURE
// yp_IF: A series of macros to emulate an if/elif/else with exception handling.  To be used
// strictly as follows (including braces):
//      yp_IF(condition1) {
//          // branch1
//      } yp_ELIF(condition2) {   // optional; multiple yp_ELIFs allowed
//          // branch2
//      } yp_ELSE {               // optional
//          // else-branch
//      } yp_ELSE_EXCEPT_AS(e) {  // optional; can also use yp_ELSE_EXCEPT
//          // exception-branch
//      } yp_ENDIF
// C's return statement works as you'd expect.  yp_ELSE_EXCEPT_AS defines the exception target
// variable.
//
// As in Python, a condition is only evaluated if previous conditions evaluated false and did not
// raise an exception, the exception-branch is executed if any evaluated condition raises an
// exception, and the exception target is only available inside exception-branch.  Unlike Python,
// exceptions in the chosen branch do not trigger the exception-branch.  If a condition creates a
// new reference that must be discarded, use yp_IFd and/or yp_ELIFd ("d" stands for "discard" or
// "decref"):
//      yp_IFd(yp_getitem(a, key))
#endif

#ifdef yp_FUTURE
// yp_WHILE: A series of macros to emulate a while/else with exception handling.  To be used
// strictly as follows (including braces):
//      yp_WHILE(condition) {
//          // suite
//      } yp_WHILE_ELSE {         // optional
//          // else-suite
//      } yp_WHILE_EXCEPT_AS(e) { // optional; can also use yp_WHILE_EXCEPT
//          // exception-suite
//      } yp_ENDWHILE
// C's break, continue, and return statements work as you'd expect.  yp_WHILE_EXCEPT_AS defines the
// exception target variable.
//
// As in Python, the condition is evaluated multiple times until:
//  - it evaluates to false, in which case the else-suite is executed
//  - a break statement, in which case neither the else- nor exception-suites are executed
//  - an exception occurs in condition, in which case the exception target (which is only available
//  inside exception-suite) is set to the exception and the exception-suite is executed
// Unlike Python, exceptions in the suites do not trigger the exception-suite.  If condition
// creates a new reference that must be discarded, use yp_WHILEd ("d" stands for "discard" or
// "decref"):
//      yp_WHILEd(yp_getindexC(a, -1))
#endif

#ifdef yp_FUTURE
// yp_FOR: A series of macros to emulate a for/else with exception handling.  To be used strictly
// as follows (including braces):
//      ypObject *x = yp_NameError; // initialize to any new or immortal reference
//      yp_FOR(x, expression) {
//          // suite
//      } yp_FOR_ELSE {             // optional
//          // else-suite
//      } yp_FOR_EXCEPT_AS(e) {     // optional; can also use yp_FOR_EXCEPT
//          // exception-suite
//      } yp_ENDFOR
// C's break and continue statements work as you'd expect; however, use yp_FOR_return instead of
// return to avoid leaking a reference.  yp_FOR_EXCEPT_AS defines the exception target variable.
//
// As in Python, the expression is evaluated once to create an iterator, then the suite is executed
// once for each successfully-yielded value, which is assigned to the target variable.  This
// occurs until:
//  - the iterator raises yp_StopIteration, in which case else-suite is executed (but *not* the
//  exception-suite)
//  - a break statement, in which case neither the else- nor exception-suites are executed
//  - an exception occurs in expression or the iterator, in which case the exception target (which
//  is only available inside exception-suite) is set to the exception and the exception-suite is
//  executed
// Unlike Python, there can only be one target (no automatic tuple unpacking), and exceptions in
// the suites do not trigger the exception-suite.  If expression creates a new reference that must
// be discarded, use yp_FORd ("d" stands for "discard" or "decref"):
//      yp_FORd(x, yp_tupleN(3, a, b, c))
//
// Before a new reference is assigned to the target variable, the previous reference in the target
// is automatically discarded.  As such:
//  - if the iterator yields no values, the target's value does not change
//  - when the loop completes, the target will retain a reference to the last successfully-yielded
//  value; this includes the else- and exception-suites
//  - the target *must* have a value before the loop; initializing to yp_NameError mimics Python's
//  behaviour when the iterator yields no values
//  - if you assign a new value to the target, remember to discard the previous reference yourself;
//  also, this new value will be discarded on subsequently-yielded values
//  - if you want to retain a reference to a yielded value before moving to the next one, create
//  a new reference (via yp_incref, perhaps)
#endif

#ifdef yp_FUTURE
// yp: A set of macros to make nohtyP function calls look more like Python operators and method
// calls.  Best explained with examples:
//  a.append(b)           --> yp(a,append, b)               --> yp_append(a, b)
//  a + b                 --> yp(a, add, b)                 --> yp_add(a, b)
// For methods that take no arguments, use yp1 (the '1' counts the object a, but not the method):
//  a.isspace()           --> yp1(a,isspace)                --> yp_isspace(a)
// If variadic macros are supported by your compiler, yp can take multiple arguments:
//  a.setdefault(b, c)    --> yp(&a,setdefault, b, c)       --> yp_setdefault(&a, b, c)
//  a.startswith(b, 2, 7) --> yp(a,startswith4, b, 2, 7)    --> yp_startswith4(a, b, 2, 7)
// If variadic macros are not supported, use yp3, yp4, etc (and define yp_NO_VARIADIC_MACROS):
//  a.setdefault(b, c)    --> yp3(&a,setdefault, b, c)      --> yp_setdefault(&a, b, c)
//  a.startswith(b, 2, 7) --> yp4(a,startswith4, b, 2, 7)   --> yp_startswith4(a, b, 2, 7)
#endif


/*
 * Internals  XXX Do not use directly!
 */

// This structure is likely to change in future versions; it should only exist in-memory
// XXX dicts abuse ob_alloclen to hold a search finger for popitem
// XXX The dealloc list (i.e. yp_decref) abuses ob_hash to point to the next object to dealloc
typedef yp_int32_t _yp_ob_len_t;
struct _ypObject {
    yp_uint16_t  ob_type;        // type code
    yp_uint8_t   ob_flags;       // type-independent flags
    yp_uint8_t   ob_type_flags;  // type-specific flags
    yp_uint32_t  ob_refcnt;      // reference count
    _yp_ob_len_t ob_len;         // length of object
    _yp_ob_len_t ob_alloclen;    // allocated length
    yp_hash_t    ob_hash;        // cached hash for immutables
    void *       ob_data;        // pointer to object data
    // Note that we are 8-byte aligned here on both 32- and 64-bit systems
};

// ypObject_HEAD defines the initial segment of every ypObject; it must be followed by a semicolon
#define _ypObject_HEAD ypObject ob_base /* force semi-colon */
// Declares the ob_inline_data array for container object structures
#define _yp_INLINE_DATA(elemType) elemType ob_inline_data[]

// These structures are likely to change in future versions; they should only exist in-memory
struct _ypIntObject {
    _ypObject_HEAD;
    // TODO If sizeof(yp_int_t)==sizeof(yp_hash_t), we _could_ use ob_hash instead of value,
    // except:
    //  - ob_hash is currently only supposed to be for immutable values
    //  - Value -1 gets mapped to hash -2, which if cached would *change the value* of "-1"
    //  - We could _not_ make this optimization for floats...
    // So, this may not be a great way to reduce the size of these simpler types.
    yp_int_t value;
};
struct _ypBytesObject {
    _ypObject_HEAD;
    _yp_INLINE_DATA(yp_uint8_t);
};
struct _ypStrObject {
    _ypObject_HEAD;
    ypObject *utf_8;
    _yp_INLINE_DATA(yp_uint8_t);
};

// Set ob_refcnt to this value for immortal objects
#define _ypObject_REFCNT_IMMORTAL (0x7FFFFFFFu)
// Set ob_hash to this value for uninitialized hashes (tp_hash will be called and ob_hash updated)
// TODO Instead, make an ob_flag to state if ob_hash is valid or invalid, then could store -1 as
// a valid cached hash and int could use ob_hash to store the value
#define _ypObject_HASH_INVALID ((yp_hash_t)-1)
// Set ob_len or ob_alloclen to this value to signal an invalid length
#define _ypObject_LEN_INVALID ((yp_ssize_t)-1)
// Macros on ob_type_flags for string objects (bytes and str)
#define _ypStringLib_ENC_BYTES (0u)
#define _ypStringLib_ENC_LATIN_1 (1u)
#define _ypStringLib_ENC_UCS_2 (2u)
#define _ypStringLib_ENC_UCS_4 (3u)

// These type codes must match those in nohtyP.c
#define _ypInt_CODE (10u)
#define _ypBytes_CODE (16u)
#define _ypStr_CODE (18u)
#define _ypFunction_CODE (28u)

// "Constructors" for immortal objects; implementation considered "internal", documentation above
#define _yp_IMMORTAL_HEAD_INIT(type, type_flags, data, len)                         \
    {                                                                               \
        type, 0, type_flags, _ypObject_REFCNT_IMMORTAL, len, _ypObject_LEN_INVALID, \
                _ypObject_HASH_INVALID, data                                        \
    }
#define _yp_IMMORTAL_INT(qual, name, value)                                                \
    static struct _ypIntObject _##name##_struct = {                                        \
            _yp_IMMORTAL_HEAD_INIT(_ypInt_CODE, 0, NULL, _ypObject_LEN_INVALID), (value)}; \
    qual ypObject *const name = (ypObject *)&_##name##_struct /* force semi-colon */
#define _yp_IMMORTAL_BYTES(qual, name, value)                                              \
    static const char            _##name##_data[] = value;                                 \
    static struct _ypBytesObject _##name##_struct = {_yp_IMMORTAL_HEAD_INIT(_ypBytes_CODE, \
            _ypStringLib_ENC_BYTES, (void *)_##name##_data, sizeof(_##name##_data) - 1)};  \
    qual ypObject *const name = (ypObject *)&_##name##_struct /* force semi-colon */
// FIXME If we populate name->utf_8 on immortals, we are leaking memory. Either don't, or
// pre-allocate an immortal bytes that we populate later?
#define _yp_IMMORTAL_STR_LATIN_1(qual, name, value)                                               \
    static const char          _##name##_data[] = value;                                          \
    static struct _ypStrObject _##name##_struct = {                                               \
            _yp_IMMORTAL_HEAD_INIT(_ypStr_CODE, _ypStringLib_ENC_LATIN_1, (void *)_##name##_data, \
                    sizeof(_##name##_data) - 1),                                                  \
            NULL};                                                                                \
    qual ypObject *const name = (ypObject *)&_##name##_struct /* force semi-colon */
// TODO yp_IMMORTAL_TUPLE

// FIXME Instead of _yp_NOQUAL, should we force extern? We really don't want yp_IMMORTAL_* placed
// on the stack... And maybe flip around so static is default and _extern is option (as per Python).
#define _yp_NOQUAL  // Used in place of static or extern for qual
#define yp_IMMORTAL_INT(name, value) _yp_IMMORTAL_INT(_yp_NOQUAL, name, value)
#define yp_IMMORTAL_BYTES(name, value) _yp_IMMORTAL_BYTES(_yp_NOQUAL, name, value)
#define yp_IMMORTAL_STR_LATIN_1(name, value) _yp_IMMORTAL_STR_LATIN_1(_yp_NOQUAL, name, value)

#define yp_IMMORTAL_INT_static(name, value) _yp_IMMORTAL_INT(static, name, value)
#define yp_IMMORTAL_BYTES_static(name, value) _yp_IMMORTAL_BYTES(static, name, value)
#define yp_IMMORTAL_STR_LATIN_1_static(name, value) _yp_IMMORTAL_STR_LATIN_1(static, name, value)

#ifdef yp_FUTURE
// The implementation of yp_IF is considered "internal"; see above for documentation
// clang-format off
#define _yp_IF(expression, decref_expression) { \
    ypObject *_yp_IF_expr; \
    ypObject *_yp_IF_cond; \
    { \
        _yp_IF_expr = (expression); \
        _yp_IF_cond = yp_bool(_yp_IF_expr); \
        decref_expression; \
        if(_yp_IF_cond == yp_True) {
#define yp_IF(expression)     _yp_IF(expression, ((void)0))
#define yp_IFd(expression)    _yp_IF(expression, yp_decref(_yp_IF_expr))
#define _yp_ELIF(expression, decref_expression) \
    } } if(_yp_IF_cond == yp_False) { \
        _yp_IF_expr = (expression); \
        _yp_IF_cond = yp_bool(_yp_IF_expr); \
        decref_expression; \
        if(_yp_IF_cond == yp_True) {
#define yp_ELIF(expression)   _yp_ELIF(expression, ((void)0))
#define yp_ELIFd(expression)  _yp_ELIF(expression, yp_decref(_yp_IF_expr))
#define yp_ELSE \
    } } if(_yp_IF_cond == yp_False) { {
#define yp_ELSE_EXCEPT \
    } } if(yp_isexceptionC(_yp_IF_cond)) { {
#define yp_ELSE_EXCEPT_AS(target) \
    yp_ELSE_EXCEPT \
        ypObject *target = _yp_IF_cond;
#define yp_ENDIF \
    } } \
}
// clang-format on
#endif

#ifdef yp_FUTURE
// The implementation of yp_WHILE is considered "internal"; see above for documentation
// clang-format off
#define _yp_WHILE(expression, decref_expression) { \
    ypObject *_yp_WHILE_expr; \
    ypObject *_yp_WHILE_cond; \
    while(_yp_WHILE_expr = (expression), \
           _yp_WHILE_cond = yp_bool(_yp_WHILE_expr), \
           decref_expression, \
           _yp_WHILE_cond == yp_True) {
#define yp_WHILE(expression)  _yp_WHILE(expression, ((void)0))
#define yp_WHILEd(expression) _yp_WHILE(expression, yp_decref(_yp_WHILE_expr))
#define yp_WHILE_ELSE \
    } if(_yp_WHILE_cond == yp_False) {
#define yp_WHILE_EXCEPT \
    } if(yp_isexceptionC(_yp_WHILE_cond)) {
#define yp_WHILE_EXCEPT_AS(target) \
    yp_WHILE_EXCEPT \
        ypObject *target = _yp_WHILE_cond;
#define yp_ENDWHILE \
    } \
}
// clang-format on
#endif

#ifdef yp_FUTURE
// The implementation of yp_FOR is considered "internal"; see above for documentation
// clang-format off
#define _yp_FOR(target, expression, decref_expression) { \
    yp_uint64_t _yp_FOR_state; \
    ypObject *_yp_FOR_item; \
    int _yp_FOR_iter_exhausted = 0; \
    ypObject *_yp_FOR_expr = (expression); \
    ypObject *_yp_FOR_iter = yp_miniiter(_yp_FOR_expr, &_yp_FOR_state); \
    decref_expression; \
    while(1) { \
        _yp_FOR_item = yp_miniiter_next(&_yp_FOR_iter, &_yp_FOR_state); \
        if(yp_isexceptionC(_yp_FOR_item)) { \
            if(yp_isexceptionC(_yp_FOR_iter)) break; \
            if(yp_isexceptionC2(_yp_FOR_item, yp_StopIteration)) _yp_FOR_iter_exhausted = 1; \
            break; \
        } \
        yp_decref(target); target = _yp_FOR_item; {
#define yp_FOR(target, expression)    _yp_FOR(target, expression, ((void)0))
#define yp_FORd(target, expression)   _yp_FOR(target, expression, yp_decref(_yp_FOR_expr))
#define yp_FOR_return(expression) \
    return yp_decref(_yp_FOR_iter), (expression)
#define yp_FOR_ELSE \
    } } if(_yp_FOR_iter_exhausted) { {
#define yp_FOR_EXCEPT \
    } } if(!_yp_FOR_iter_exhausted && yp_isexceptionC(_yp_FOR_item)) { {
#define yp_FOR_EXCEPT_AS(target) \
    yp_FOR_EXCEPT \
        ypObject *target = _yp_FOR_item;
#define yp_ENDFOR \
    } } \
    yp_decref(_yp_FOR_iter); \
}
// clang-format on
#endif

#ifdef yp_FUTURE
// The implementation of "yp" is considered "internal"; see above for documentation
#define yp1(self, method) yp_##method(self)
#define yp2(self, method, a1) yp_##method(self, a1)
#ifdef yp_NO_VARIADIC_MACROS
#define yp yp2
#else
#define yp(self, method, ...) yp_##method(self, __VA_ARGS__)
#endif
#define yp3(self, method, a1, a2) yp_##method(self, a1, a2)
#define yp4(self, method, a1, a2, a3) yp_##method(self, a1, a2, a3)
#define yp5(self, method, a1, a2, a3, a4) yp_##method(self, a1, a2, a3, a4)
#endif

#ifdef __cplusplus
}
#endif
#endif  // yp_NOHTYP_H

/*
 * nohtyP.h - A Python-like API for C, in one .c and one .h
 *      Public domain?  PSF?  dunno
 *      http://nohtyp.wordpress.com/
 *      TODO Python's license
 *
 * The goal of nohtyP is to enable Python-like code to be written in C.  It is patterned after
 * Python's built-in API, then adjusted for expected usage patterns.  It also borrows ideas from 
 * Python's own C API.  To be as portable as possible, it is written in one .c and one .h file and
 * attempts to rely strictly on standard C.
 *
 * Most functions borrow inputs, create their own references, and output new references.  One 
 * exception: those that modify objects steal a reference to the modified object and return a
 * new reference; this may not be the same object, particularly if an exception occurs.  (You can
 * always call yp_incref first to ensure the object is not deallocated.)  Another exception:
 * yp_or and yp_and return borrowed references, as this simplifies the most-common use case.
 *
 * When an error occurs in a function, it returns an exception object (after ensuring all objects
 * are left in a consistent state), even if no exceptions are mentioned in the documentation.  
 * If the  function has stolen a reference, that reference is discarded.  If an exception object
 * is used for any input into a function, it is returned before any modifications occur.  
 * Exception objects are immortal, so it isn't necessary to call yp_decref on them.  As a result
 * of these rules, it is possible to string together multiple function calls and only check if an
 * exeption occured at the end: 
 *      yp_IMMORTAL_BYTES( sep, ", " ); 
 *      yp_IMMORTAL_BYTES( fmt, "(%s)\n" ); 
 *      ypObject *sepList = yp_join( sep, list );
 *      // if yp_join failed, sepList could be an exception 
 *      ypObject *result = yp_format( fmt, sepList ); 
 *      yp_decref( sepList ); 
 *      if( yp_isexception( result ) ) exit( -1 ); 
 *
 * This API is threadsafe so long as no objects are modified while being accessed by multiple
 * threads; this includes updating reference counts, so immutables are not inherently threadsafe!
 * One strategy to ensure safety is to deep copy objects before exchanging between threads.  
 * Sharing immutable, immortal objects is always safe.
 *
 * The boundary between C types and ypObjects is an important one.  Functions that accept C types
 * and return objects end in "C".  Functions that accept objects and return C types end in "_asC",
 * unless "C" would be unambiguous; for example, yp_isexceptionC and yp_lenC must accept only 
 * objects for input.  Finally, functions where the inputs and outputs are, except for the error
 * indicator, all C types, are nohtyP library routines and thus end in "L".
 *
 * Other important postfixes:
 *  D - discard after use (ie yp_IFd)
 *  N - n variable positional arguments follow
 *  K - n key/value arguments follow (for a total of n*2 arguments)
 *  X - direct access to internal memory; tread carefully!
 */


/*
 * Initialization
 */

// Must be called once before any other function; subsequent calls are a no-op.  If a fatal error
// occurs abort() is called.
void yp_initialize( void );


/*
 * Object Fundamentals
 */

// All objects maintain a reference count; when that count reaches zero, the object is deallocated.
// Certain objects are immortal and, thus, never deallocated; examples include yp_None, yp_True, 
// yp_False, exceptions, and so forth.

// nohtyP objects are only accessed through pointers to ypObject.
typedef struct _ypObject ypObject;

// Increments the reference count of x, returning it as a convenience.  Always succeeds; if x is
// immortal this is a no-op.
ypObject *yp_incref( ypObject *x );

// A convenience function to increment the references of n objects.
void yp_increfN( int n, ... );

// Decrements the reference count of x, deallocating it if the count reaches zero.  Always 
// succeeds; if x is immortal this is a no-op.
void yp_decref( ypObject *x );

// A convenience function to decrement the references of n objects.
void yp_decrefN( int n, ... );


/*
 * Freezing, "Unfreezing", and Invalidating
 */

// Most objects support a one-way freeze function that makes them immutable, an unfrozen_copy
// function that returns a mutable copy, and a one-way invalidate function that renders the object
// useless; there are also deep variants to these functions.  Supplying an invalidated object to a
// function results in yp_InvalidatedError.  Freezing and invalidating are two examples of object
// transmutation, where the type of the object is converted to a different type.  Unlike Python,
// most objects are copied in memory, even immutables, as copying is one method for maintaining
// threadsafety.

// Steals x, transmutes it to its associated immutable type, and returns a new reference to it.
// If x is already immutable or has been invalidated this is a no-op.  If x can't be frozen
// a new, invalidated object is returned (rarely occurs: most types _can_ be frozen). 
void yp_freeze( ypObject **x );

// Steals and freezes x and, recursively, all contained objects, returning a new reference.
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

// Steals x, discards all contained objects, deallocates _some_ memory, transmutes it to
// the ypInvalidated type (rendering the object useless), and returns a new reference to x.
// If x is immortal or already invalidated this is a no-op; immutable objects _can_ be invalidated.
void yp_invalidate( ypObject **x );

// Steals and invalidates x and, recursively, all contained objects, returning a new reference.
void yp_deepinvalidate( ypObject **x );


/*
 * Boolean Operations and Comparisons
 */

// TODO comparison/boolean operations and error handling and yp_IF

// Returns the immortal yp_False if the object should be considered false (yp_None, a number equal
// to zero, or a container of zero length), otherwise yp_True or an exception.
ypObject *yp_bool( ypObject *x );

// Returns the immortal yp_True if x is considered false, otherwise yp_False or an exception.
ypObject *yp_not( ypObject *x );

// Returns a *borrowed* reference to y if x is false, otherwise to x.  Unlike  Python, both
// arguments are always evaluated.
ypObject *yp_or( ypObject *x, ypObject *y );

// A convenience function to "or" n objects.  Returns yp_False if n is zero, and the first object 
// if n is one.  Returns a *borrowed* reference.
ypObject *yp_orN( int n, ... );

// Equivalent to yp_bool( yp_orN( n, ... ) ).
ypObject *yp_anyN( int n, ... );

// Returns a *borrowed* reference to x if x is false, otherwise to y.  Unlike Python, both 
// arguments are always evaluated.
ypObject *yp_and( ypObject *x, ypObject *y );

// A convenience function to "and" n objects.  Returns yp_True if n is zero, and the first object
// if n is one.  Returns a *borrowed* reference.
ypObject *yp_andN( int n, ... );

// Equivalent to yp_bool( yp_andN( n, ... ) ).
ypObject *yp_allN( int n, ... );

// Implements the "less than" (x<y), "less than or equal" (x<=y), "equal" (x==y), "not equal"
// (x!=y), "greater than or equal" (x>=y), and "greater than" (x>y) comparisons.  Returns the
// immortal yp_True if the condition is true, otherwise yp_False or an exception.
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

// TODO yp_int32_t, yp_uint32_t; yp_int_t is the type used to represent ints, yp_float_t the same
// (so is actually double).

// The signature of a function that can be wrapped up in a generator, called by yp_send and
// similar functions.  State is an array of len *borrowed* ypObject*s that hold the current state;
// the function cannot change the number of objects, but it can discard them and replace with new
// references.  value is an object that is "sent" into the function by yp_send; it may also be 
// yp_GeneratorExit if yp_close is called, or another exception.  The return value must be a new 
// reference, yp_StopIteration if the generator is exhausted, or another exception.
typedef ypObject *(*yp_generator_func_t)( ypObject **state, yp_ssize_t len, ypObject *value );


/*
 * Constructors
 */

// Unlike Python, most nohtyP types have both mutable and immutable versions.  An "intstore" is a
// mutable int (it "stores" an int); similar for floatstore.  The mutable str is called a
// "characterarray", while a "frozendict" is an immutable dict.  There is no immutable iter or file
// type.

// Returns a new reference to an int/intstore with the given value.
ypObject *yp_intC( yp_int_t value );
ypObject *yp_intstoreC( yp_int_t value );

// Returns a new reference to an int/intstore interpreting the C string as an integer literal with
// the given base.  Base zero means to infer the base according to Python's syntax.
ypObject *yp_int_strC( char *string, int base );
ypObject *yp_intstore_strC( char *string, int base );

// Returns a new reference to a float/floatstore with the given value.
ypObject *yp_floatC( yp_float_t value );
ypObject *yp_floatstoreC( yp_float_t value );

// Returns a new reference to a float/floatstore interpreting the string as a Python 
// floating-point literal.
ypObject *yp_float_strC( char *string );
ypObject *yp_floatstore_strC( char *string );

// Returns a new reference to an iterator for object x.
ypObject *yp_iter( ypObject *x );

// Returns a new reference to a generator-iterator object using the given func.  The function will
// be passed the given n objects as state on each call.  lenhint is a clue to consumers of the 
// generator how many items will be yielded; use zero if this is not known.
ypObject *yp_generatorCN( yp_generator_func_t func, yp_ssize_t lenhint, int n, ... );

// Returns a new reference to a bytes/bytearray, copying the first len bytes from source.  If 
// source is NULL it is considered as having all null bytes; if len is negative source is 
// considered null terminated (and, therefore, will not contain the null byte).
//  Ex: pre-allocate a bytearray of length 50: yp_bytearrayC( NULL, 50 )
ypObject *yp_bytesC( yp_uint8_t *source, yp_ssize_t len );
ypObject *yp_bytearrayC( yp_uint8_t *source, yp_ssize_t len );

// Creates an immortal bytes constant at compile-time, which can be accessed by the variable name,
// which is of type "static ypObject * const".  value is a C string literal that can contain null
// bytes.  The length is calculated while compiling; the hash will be calculated the first time it
// is accessed.  To be used as:
//      yp_IMMORTAL_BYTES( name, value ); 

// XXX The str/characterarray types will be added in a future version

// Returns a new reference to a tuple/list of length n containing the given objects.
ypObject *yp_tupleN( int n, ... );
ypObject *yp_listN( int n, ... );

// Returns a new reference to a tuple/list made from factor shallow copies of yp_tupleN( n, ... )
// concatenated.  Equivalent to "factor * (obj0, obj1, ...)" in Python.
//  Ex: pre-allocate a list of length 99: yp_list_repeatCN( 99, 1, yp_None )
//  Ex: an 8-tuple containing alternating bools: yp_tuple_repeatCN( 4, 2, yp_False, yp_True )
ypObject *yp_tuple_repeatCN( yp_ssize_t factor, int n, ... );
ypObject *yp_list_repeatCN( yp_ssize_t factor, int n, ... );

// Returns a new reference to a tuple/list whose elements come from iterable.
ypObject *yp_tuple( ypObject *iterable );
ypObject *yp_list( ypObject *iterable );

// Returns a new reference to a frozenset/set containing the given n objects; the length will be n,
// unless there are duplicate objects.
ypObject *yp_frozensetN( int n, ... );
ypObject *yp_setN( int n, ... );

// Returns a new reference to a frozenset/set whose elements come from iterable.
ypObject *yp_frozenset( ypObject *iterable );
ypObject *yp_set( ypObject *iterable );

// Returns a new reference to a frozendict/dict containing the given n key/value pairs (for a total
// of 2*n objects); the length will be n, unless there are duplicate keys.
//  Ex: yp_dictK( 3, key0, value0, key1, value1, key2, value2 )
ypObject *yp_frozendictK( int n, ... );
ypObject *yp_dictK( int n, ... );

// Returns a new reference to a frozendict/dict containing the given n keys all set to value; the
// length will be n, unless there are duplicate keys.
//  Ex: pre-allocate a dict with 3 keys: yp_dict_fromkeysN( yp_None, 3, key0, key1, key2 )
ypObject *yp_frozendict_fromkeysN( ypObject *value, int n, ... );
ypObject *yp_dict_fromkeysN( ypObject *value, int n, ... );

// XXX The file type will be added in a future version


// TODO be explicit about which functions return immortal objects


/*
 * Generic Object Operations
 */
yp_hash
yp_isinstanceC
yp_len
yp_type



/*
 * Numeric Operations
 */
yp_abs
yp_divmod
yp_hex
yp_oct
yp_pow
yp_round

// TODO versions of the above that steal their arguments, so that operations can be chained
// together. (yp_addD)
// TODO inplace versions of above, for the mutable int type
// TODO bitwise and/or need new names, so as not to conflict with yp_and/yp_or...yp_amp/yp_bar?

// TODO document uint yp_addC( uint, uint, ypObject *error ) (how to name?!)


/*
 * Iterator Operations
 */

yp_all
yp_any
yp_enumerate
yp_filter
yp_map // ?! do I need to wrap C functions?
yp_max // or numeric?
yp_min
yp_next
yp_range
yp_reversed
yp_sorted // or is this a seqence operation?
yp_sum // or numeric?
yp_zip

// You may also be interested in yp_FOR for working with iterables; see below


/*
 * Sequence Operations
 */

// When given to a slice-like start/stop argument, signals that the slice should start/stop at the
// end of the sequence.  Use zero to identify the start of the sequence.
//  Ex: a complete slice in Python is "[:]"; in nohtyP, it's "0, ypSlice_END, 1"
#define ypSlice_END  yp_SSIZE_T_MAX

// TODO bad idea?
// For sequences that store their elements as an array of pointers to ypObjects (list and tuple),
// returns a pointer to the beginning of that array, and sets len to the length of the sequence.
// The returned value points into internal object memory, so they are *borrowed* references and
// MUST NOT be modified; furthermore, the sequence itself must not be modified while using the
// array.  Returns NULL and sets len to -1 on error.
// 'X' means the function is dealing with internal data
ypObject const * *yp_itemarrayX( ypObject *seq, yp_ssize_t *len );
// TODO similar magic for bytes/etc, although writing to bytearray is OK

/*
 * Set Operations
 */

/*
 * Mapping Operations
 */

/*
 * Bytes & String Operations
 */
yp_chr // also yp_chrC could be useful
yp_ord




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
//      yp_FORd( yp_tupleN( a, b, c ) )
// Also, the yielded values assigned to x are borrowed: they are automatically discarded at the end
// of every suite.  If you want to retain a reference to them, you'll need to call yp_incref
// yourself:
//      yp_FORd( x, values ) {
//          if( yp_ge( x, minValue ) ) {
//              yp_incref( x );
//              break;
//          }
//      } yp_ENDFOR

// yp: A set of macros to make nohtyP function calls look more like Python operators and method
// calls.  Best explained with examples:
//  a.append( b )           --> yp( a,append, b )               --> yp_append( a, b )
//  a + b                   --> yp( a, add, b )                 --> yp_add( a, b )
// For methods that take no arguments, use yp0:
//  a.isspace( )            --> yp0( a,isspace )                --> yp_isspace( a )
// If variadic macros are supported by your compiler, yp can take multiple arguments:
//  a.setdefault( b, c )    --> yp( a,setdefault, b, c )        --> yp_setdefault( a, b, c )
//  a.startswith( b, 2, 7 ) --> yp( a,startswith3, b, 2, 7 )    --> yp_startswith3( a, b, 2, 7 )
// If variadic macros are not supported, use yp2, yp3, etc:
//  a.setdefault( b, c )    --> yp2( a,setdefault, b, c )       --> yp_setdefault( a, b, c )
//  a.startswith( b, 2, 7 ) --> yp3( a,startswith3, b, 2, 7 )   --> yp_startswith3( a, b, 2, 7 )

// A macro to get exception info as a string, include file/line info of the place the macro is
// checked



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
};


// "Constructors" for immortal objects; implementation considered "internal", documentation above
#define yp_IMMORTAL_BYTES( name, value ) \
    static const char _ ## name ## _data[] = value; \
    static ypObject _ ## name ## _struct = yp_IMMORTAL_HEAD_INIT( \
            ypBytes_CODE, _ ## name ## _data, sizeof( _ ## name ## _data )-1 ); \
    static ypObject * const name = &_ ## name ## _struct /* force use of semi-colon */
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
#ifdef yp_NO_VARIADIC_MACROS // FIXME rename or something
#define yp yp1
#else
#define yp( self, method, ... )     yp_ ## method( self, _VAR_ARGS_ )
#endif
#define yp2( self, method, a1, a2 ) yp_ ## method( self, a1, a2 )
#define yp3( self, method, a1, a2, a3 ) yp_ ## method( self, a1, a2, a3 )
#define yp4( self, method, a1, a2, a3, a4 ) yp_ ## method( self, a1, a2, a3, a4 )



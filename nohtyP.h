/*
 * nohtyP.h - A Python-like API for C, in one .c and one .h
 *      Public domain?  PSF?  dunno
 *      http://nohtyp.wordpress.com/
 *      TODO Python's license
 *
 * The goal of nohtyP is to enable Python-like code to be written in C.  It is patterned after
 * Python's built-in API, then adjusted for C's lack of exceptions, manual reference counting, and
 * expected usage patterns.  It also borrows ideas from Python's own C API.  To be as portable as
 * possible, it is written in one .c and one .h file and attempts to rely strictly on standard C
 * functions.
 *
 * TODO document that bytes support format and other Python3 str-only funcs
 *
 * All objects maintain a reference count; when that count reaches zero, the object is deallocated.
 * An invalidated object can still have references to it, although any attempt to use that object
 * would result in an exception.  Objects can be declared immortal and, thus, never deallocated;
 * attempting to invalidate an immortal is a no-op.  Examples of immortals include yp_None,
 * yp_True, yp_False, exceptions, yp_CONST_BYTES, and so forth.
 *
 * Most functions borrow inputs, create their own references, and output new references.
 * One exception: those that modify objects steal a reference to the modified object and return a
 * new reference; this may not be the same object, particularly if an exception occurs.  (You can
 * always call yp_incref or yp_copy to compensate for a stolen reference.)  Another exception:
 * yp_or and yp_and return borrowed references, as this simplifies the most-common use case.
 *
 * TODO instead of steal/return, take in a PyObject**, modifying it in-place on error, and that
 * way the syntax/compiler helps us remember that this is special.  Can still return
 * None/exception, which could be handy.
 *
 * TODO in the docs, be explicit about which functions return immortal objects
 *
 * Most objects support a one-way freeze function that makes them immutable, an unfrozen_copy
 * function that returns mutable copy, and a one-way invalidate function that discards all 
 * references and renders the object useless (even immutable objects); there are also deep
 * variants to these functions.  If an invalidated object is supplied to a function, 
 * yp_InvalidatedError is returned.  Freezing and invalidating are two examples of object 
 * transmutation, where the type of the object is converted to a different type.
 *
 * freeze: http://www.python.org/dev/peps/pep-0351/
 * frozendict: http://www.python.org/dev/peps/pep-0416/ *
 *
 * When an error occurs in a function, it returns an exception object (after ensuring all objects
 * are left in a consistent state), even if no exceptions are mentioned in the documentation.  If
 * the function has stolen a reference, that reference is discarded.  If an exception object is
 * used for any input into a function, it is returned before any modifications occur; note that
 * containers cannot store exception objects.  As a result of these rules, it is possible to
 * string together multiple function calls and only check if an exeption occured at the end: 
 *      yp_CONST_BYTES( sep, ", " ); 
 *      yp_CONST_BYTES( fmt, "(%s)\n" ); 
 *      ypObject *sepList = yp_join( sep, list );
 *      // if yp_join failed, sepList could be an exception 
 *      ypObject *result = yp_format( fmt, sepList ); 
 *      yp_decref( sepList ); 
 *      if( yp_isexception( result ) ) exit( -1 ); 
 * Additionally, exception objects are immortal, so it is safe to ignore them for functions that
 * only return immortal objects (such as yp_None).
 *
 * TODO comparison/boolean operations and error handling and yp_IF
 *
 * This API is threadsafe so long as no objects are modified while being accessed by multiple
 * threads, including changing reference counts.  One strategy to ensure this is to deep copy any
 * object, even immutable ones, before exchanging between threads.  Sharing immutable, immortal
 * objects is always safe.
 *
 * Postfixes:
 *  C - to/from C types
 *  D - discard after use (ie yp_IFd)
 *  N - n variable positional arguments follow
 *  K - n keyword arguments follow
 *  X - direct access to internal memory: tread carefully!
 */


/*
 * Initialization
 */

// Must be called once before any other function; subsequent calls are a no-op.  If a fatal error
// occurs abort() is called.
void yp_initialize( void );


/*
 * Reference counting
 */

// Increments the reference count of x, returning it as a convenience.  Always succeeds; if x is
// immortal this is a no-op.
ypObject *yp_incref( ypObject *x );

// A convenience function to increment the references of n objects.
void yp_increfN( yp_ssize_t n, ... );

// Decrements the reference count of x, deallocating it if the count reaches zero.  Always 
// succeeds; if x is immortal this is a no-op.
void yp_decref( ypObject *x );

// A convenience function to decrement the references of n objects.
void yp_decrefN( yp_ssize_t n, ... );


/*
 * Freezing, "unfreezing", and invalidating
 */

// Steals x, transmutes it to its associated immutable type, and returns a new reference to it.
// If x is already immutable or has been invalidated this is a no-op; if x can't be frozen
ypObject *yp_freeze( ypObject *x );

// Steals and freezes x and, recursively, all referenced objects, returning a new reference.
ypObject *yp_deepfreeze( ypObject *x );

// TODO a bit in the ypObject header to track recursion

// Returns a new reference to a mutable shallow copy of x, or yp_MemoryError.
ypObject *yp_unfrozen_copy( ypObject *x );

// Returns a new reference to a mutable deep copy of x, or yp_MemoryError.
ypObject *yp_unfrozen_deepcopy( ypObject *x );

ypObject *yp_frozen_copy( ypObject *x );

ypObject *yp_frozen_deepcopy( ypObject *x );

// Steals x, discards all referenced objects, deallocates _some_ memory, transmutes it to
// the ypInvalidated type (rendering the object useless), and returns a new reference to x.
// If x is immortal or already invalidated this is a no-op.
ypObject *yp_invalidate( ypObject *x );

// Steals and invalidates x and, recursively, all referenced objects, returning a new reference.
ypObject *yp_deepinvalidate( ypObject *x );


/*
 * Boolean operations
 */

// Returns yp_False if the object should be considered false (yp_None, a number equal to zero, or
// a container of zero length), otherwise yp_True.
ypObject *yp_bool( ypObject *x );

// Returns a borrowed reference to y if x is false, otherwise to x.  Unlike  Python, both
// arguments are always evaluated.
ypObject *yp_or( ypObject *x, ypObject *y );

// A convenience function to "or" n objects.  Similar to yp_any, returns yp_False if n is zero, 
// and the first object if n is one.
ypObject *yp_orN( yp_ssize_t n, ... );

// Returns a borrowed reference to x if x is false, otherwise to y.  Unlike Python, both arguments
// are always evaluated.
ypObject *yp_and( ypObject *x, ypObject *y );

// A convenience function to "and" n objects.  Similar to yp_all, returns yp_True if n is zero,
// and the first object if n is one.
ypObject *yp_andN( yp_ssize_t n, ... );

// Returns yp_True if x is considered false, otherwise yp_False.
ypObject *yp_not( ypObject *x );

// A series of optional macros to emulate an if/elif/else with exception handling.  This simplifies
// the use of functions that return true, false, or an exception.  To be used strictly as follows:
//  yp_IF( cond1 ) {
//      // ...
//  } yp_ELIF( cond2 ) {        // optional; multiple yp_ELIFs allowed
//      // ...
//  } yp_ELSE {                 // optional
//      // ...
//  } yp_ELSE_EXCEPT_AS( e ) {  // optional; can also use yp_ELSE_EXCEPT
//      // ...
//  } yp_ENDIF
// As in Python, a condition is only evaluated if previous conditions evaluated false and did not
// raise an exception, the yp_ELSE_EXCEPT_AS branch is executed if any evaluated condition raises
// an exception, and the exception variable is only set if an exception occurs.  Unlike Python, 
// exceptions in the chosen branch do not trigger the yp_ELSE_EXCEPT_AS branch, and the exception
// variable is not cleared at the end of the branch.
// TODO what to do about references generated by the expressions?
#define yp_IF( expression ) { \
    ypObject *_yp_IF_cond; \
    if( (_yp_IF_cond = yp_bool( expression )) == yp_True )
#define yp_ELIF( expression ) \
    if( _yp_IF_cond == yp_False ) \
      if( (_yp_IF_cond = yp_bool( expression )) == yp_True )
#define yp_ELSE \
    if( _yp_IF_cond == yp_False )
#define yp_ELSE_EXCEPT \
    if( yp_isexceptionC( _yp_IF_cond ) )
#define yp_ELSE_EXCEPT_AS( target ) \
    if( yp_isexceptionC( _yp_IF_cond ) && (target = _yp_IF_cond) )
#define yp_ENDIF \
    }

// A series of optional macros to emulate a while/else with exception handling.
//  yp_WHILE( expression ) {
//      // ...
//  } yp_WHILE_ELSE {
//      // ...
//  } yp_WHILE_EXCEPT_AS( e ) {
//      // ...
//  }
// Expression is evaluated multiple times until it evaluates to false (or an exception)
// TODO what to do about references generated by the expression?
#define yp_WHILE( expression ) { \
    ypObject *_yp_WHILE_cond; \
    while( (_yp_WHILE_cond = yp_bool( expression )) == yp_True )
#define yp_WHILE_ELSE \
    if( _yp_WHILE_cond == yp_False )
#define yp_WHILE_EXCEPT \
    if( yp_isexceptionC( _yp_WHILE_cond ) )
#define yp_WHILE_EXCEPT_AS( target ) \
    if( yp_isexceptionC( _yp_WHILE_cond ) && (target = _yp_WHILE_cond) )
#define yp_ENDWHILE \
    }



/*
 * Comparisons
 */




/*
 * Constructors
 */

// Returns a new mutable bytearray, copying the first len bytes from source.  If source is NULL it
// is considered as having all null bytes; if len is negative source is considered null terminated
// (and, therefore, will not contain the null byte).
ypObject *yp_bytearrayC( unsigned char *source, yp_ssize_t len );

// Equivalent to yp_freeze( yp_bytearrayC( source, len ) );.
ypObject *yp_bytesC( unsigned char *source, yp_ssize_t len );

// Returns a new mutable dict of n items.  There must be n pairs of objects, with the first 
// object in each pair being the key and the second the value (for a total of n*2 objects).
ypObject *yp_dictN( yp_ssize_t n, ... );
// TODO should there be a prefix to indicate this "automatically unpacked" structure idea?
// like with divmod returning two objects via output pointer parameters?

// Returns a new mutable, empty dict, likely to be populated with yp_setitem.  lenhint is the
// expected number of items in the final dict, or negative if this is not known.
ypObject *yp_dict_emptyC( yp_ssize_t lenhint );

// TODO frozendict?

// TODO unfrozen float?
ypObject *yp_float


ypObject *yp_set
ypObject *yp_frozenset

// TODO unfrozen int?
ypObject *yp_intC( long long value );


ypObject *yp_listN( yp_ssize_t n, ... );

// Returns a new list made of factor shallow copies of yp_listN( n, ... ) concatenated.  Equivalent
// to "factor * [obj1, obj2, ...]" in Python.
ypObject *yp_list_repeatCN( yp_ssize_t factor, yp_ssize_t n, ... );
ypObject *yp_list
ypObject *yp_tuple

// TODO files (ie open)?

// TODO unfrozen str?  chararray?
ypObject *yp_str




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


/*
 * Iterator Operations
 */
ypObject *yp_iter
// XXX no frozen iter

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


// A series of optional macros to emulate a for/else with exception handling.  This simplifies the
// use of iterators that may return exceptions.  To be used strictly as follows:
//  yp_FOR( x, expression ) {
//      // ...
//  } yp_FOR_ELSE {             // optional
//      // ...
//  } yp_FOR_EXCEPT_AS( e ) {   // optional; can also use yp_FOR_EXCEPT
//      // ...
//  } yp_ENDFOR
// As in Python, an iterator is created from the expression, the for suite is executed once for each
// yielded value, 
// the target is not assigned if the expression or iterator fails 
// break and continue work as expected
// Unlike Python, multiple targets are not allowed (no automatic tuple unpacking)
// The iterator reference and previous items are automatically discarded, but not for the
// expression or the final item (so that it can be used afterwards)
// the 
// itself.
// TODO what to do about references generated by the expression?
#define yp_FOR( target, expression ) { \
    ypObject *_yp_FOR_iter = yp_iter( expression ); \
    ypObject *_yp_FOR_item; \
    for( _yp_FOR_item = yp_next( _yp_FOR_iter ); \
         !yp_isexceptionC( _yp_FOR_item ) && (yp_decref( target ), target = _yp_FOR_item); \
         _yp_FOR_item = yp_next( _yp_FOR_iter ) )
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
    yp_decref( _yp_FOR_iter ); \
    }


/*
 * Sequence Operations
 */

// TODO bad idea?
// For sequences that store their items as an array of pointers to ypObjects (list and tuple),
// returns a pointer to the beginning of that array, and sets len to the length of the sequence.
// The returned value points into internal object memory, so they are borrowed references and
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









// A function to create a list of yp_Nones, which can be replaced in turn
// by the real values; avoids resizing the list constantly when the length
// is known beforehand.




// A macro to get exception info as a string, include file/line info.



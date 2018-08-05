/*
 * ypExamples.c - Contains a number of nohtyP examples
 *      http://bitbucket.org/Syeberman/nohtyp   [v0.1.0 $Change$]
 *      Copyright © 2001-2013 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/3/license.html
 */

#define yp_FUTURE // FIXME where to put this?
#include "nohtyP.h"
#include <stdio.h>
#include <process.h> // FIXME for system

#ifdef _MSC_VER
#include <crtdbg.h> // For debugging only
#endif


// FIXME track mallocs/inrefs/decrefs/frees and ensure no memory leaks

/*
 * Supporting functions and macros
 * (You can skip this section)
 */

typedef int bool;
#define false 0
#define true 1

#define EXAMPLE( group, name ) do { ypExamples_example_name = #group "_" #name; printf( "\nExample %s\n", ypExamples_example_name ); _flushall( );} while(0);
#define PrintError( fmt, ... ) fprintf( stderr, "  line %d: " fmt "\n", __LINE__, __VA_ARGS__ )
#define Expect( cond, fmt, ... ) do { if( cond ) break; ypExamples_result++; PrintError( fmt, __VA_ARGS__ ); } while(0)
// TODO include exception name
#define ExpectTrue( act ) Expect( act, #act " is not true", 0 )
#define ExpectUnreachable( ) Expect( false, "unexpectedly reachable code", 0 )
#define ExpectEqual( exp, act ) Expect( (exp) == (act), #exp " != " #act, 0 )
#define ExpectNoException( obj ) Expect( !yp_isexceptionC( obj ), "unexpected exception (0x%08X)", (obj) )

int ypExamples( void )
{
    int ypExamples_result = 0; // return value from ypExamples: number of failures
    char *ypExamples_example_name = ""; // current example name TODO needed?


/*
 * Immortals
 */

/* An immortal object is one that does not maintain a reference count and, thus, is never
 * deallocated; while safe to do so, it is not necessary to call yp_incref or yp_decref on these
 * objects.  yp_None, yp_True, and yp_False are all built-in immortals, as are all exception
 * objects.  When a function is documented like "Returns an immortal...", you know you can safely
 * discard the result without first yp_decref'ing it. */

/* It is also possible to define your own, usually immutable, immortal objects.  The following code
 * defines a series of immortal bytes objects, for use in the examples below.  These macros can be
 * used to define global and function-local "ypObject * const" variables. */
yp_IMMORTAL_BYTES( b_apple, "apple" );              // ypObject * const b_apple = <...>;
yp_IMMORTAL_BYTES( b_orange, "orange" );
yp_IMMORTAL_BYTES( b_pear, "pear" );
yp_IMMORTAL_BYTES( b_banana, "banana" );
yp_IMMORTAL_BYTES( b_crabgrass, "crabgrass" );
yp_IMMORTAL_BYTES( b_abracadabra, "abracadabra" );
yp_IMMORTAL_BYTES( b_alacazam, "alacazam" );
yp_IMMORTAL_BYTES( b_jack, "jack" );
yp_IMMORTAL_BYTES( b_sape, "sape" );
yp_IMMORTAL_BYTES( b_guido, "guido" );
yp_IMMORTAL_BYTES( b_irv, "irv" );


/*
 * Building lists
 */

/* Exceptions propagate: when given as an input they are returned immediately.  This example shows
 * that when building a list, you are free to ignore exceptions until the end. */
EXAMPLE( BuildingLists, DiscardOnException )
{
    ypObject *list = yp_listN( 0 );
    int i;
    ypObject *item;
    ypObject *exc = yp_None;

    for( i = 0; i < 5; i++ ) {
        // If yp_intC fails, item set to an exception
        item = yp_intC( i );

        // If item or list is an exception, or yp_append fails, list discarded and replaced with
        // an exception
        yp_append( &list, item );

        // yp_append creates its own reference to item, so we need to discard ours; this is
        // safe even if item is an exception, as exceptions are immortal
        yp_decref( item );
    }

    // If any of the yp_listN, yp_intC, or yp_append calls failed, list would now be that
    // exception
    ExpectNoException( list );
    ExpectEqual( 5, yp_lenC( list, &exc ) );

    // Clean-up
    yp_decref( list );
}

/* Sometimes you don't want the list you're building to be discarded on error.  As demonstrated
 * below, avoiding this is simple. */
EXAMPLE( BuildingLists, KeepOnException )
{
    ypObject *list = yp_listN( 0 );
    ypObject *item;
    ypObject *working;
    ypObject *exc = yp_None;
    int i;

    for( i = 0; i < 5; i++ ) {
        // When yp_chrC fails (on values greater than 0xFF), item will be set to exception
        item = yp_chrC( 0xFE+i );  // FIXME but it won't fail, as it supports unicode...
        if( i > 1 ) ExpectEqual( yp_ValueError, item );

        // As yp_append will discard list if item is an exception, we need to pass it a separate
        // reference in a separate variable
        working = yp_incref( list );
        yp_append( &working, item );
        if( i > 1 ) ExpectEqual( yp_ValueError, working );
        yp_decref( working );

        // append creates its own reference to item, so we need to discard ours
        yp_decref( item );
    }

    // Although the last three yp_chrC calls failed, the first two worked
    ExpectNoException( list );
    ExpectEqual( 2, yp_lenC( list, &exc ) );

    // Clean-up
    yp_decref( list );
}

// TODO more BuildingLists examples


/*
 * Sets
 */

/* Taken from Python's tutorial */
EXAMPLE( Sets, BriefDemonstration )
{
    /* Defining immortal int objects is usually not necessary, as the nohtyP functions that often
     * deal with integers (ie yp_getitem) have versions that accept C integers directly (ie
     * yp_getindexC).  int objects are needed for this example because we use them to check for
     * their presence inside a set. */
    yp_IMMORTAL_INT( i_a, 'a' );
    yp_IMMORTAL_INT( i_b, 'b' );
    yp_IMMORTAL_INT( i_z, 'z' );
    yp_IMMORTAL_INT( i_q, 'q' );

    /* yp_lenC and similar functions only modify exc on error; since they do not discard existing
     * values, exc should be initialized to yp_None or another immortal. */
    ypObject *exc = yp_None;

    {
        /* Sets can be initialized using a variable-number of object arguments */
        ypObject *basket = yp_setN( 6, b_apple, b_orange, b_apple, b_pear, b_orange, b_banana );

        /* Duplicates are removed from sets: of the 6 objects supplied to yp_setN, only 4 are
         * unique */
        ExpectEqual( 4, yp_lenC( basket, &exc ) );

        /* Sets support fast membership testing.  Also of note: there are exactly two bool objects,
         * yp_True and yp_False, so comparing equality of object pointers is perfectly valid. */
        ExpectEqual( yp_True, yp_in( b_orange, basket ) );
        ExpectEqual( yp_False, yp_in( b_crabgrass, basket ) );

        /* Don't forget to clean-up the new reference returned by yp_setN! */
        yp_decref( basket );
    }

    {
        /* Sets can also be initialized from other iterable objects; recall that the elements of a
         * bytes object are ints. */
        ypObject *a = yp_set( b_abracadabra );
        ypObject *b = yp_set( b_alacazam );
        ypObject *result;

        /* Unique letters in a: {'a', 'r', 'b', 'c', 'd'} */
        ExpectEqual( 5, yp_lenC( a, &exc ) );
        ExpectEqual( yp_True, yp_in( i_a, a ) );
        ExpectEqual( yp_False, yp_in( i_z, a ) );

        /* Letters in a but not in b: {'r', 'd', 'b'}
         * If you forget that yp_differenceN returns a new reference, you'll leak some memory */
        result = yp_differenceN( a, 1, b );
        ExpectEqual( 3, yp_lenC( result, &exc ) );
        ExpectEqual( yp_True, yp_in( i_b, result ) );
        ExpectEqual( yp_False, yp_in( i_a, result ) );
        yp_decref( result );

        /* Letters in either a or b: {'a', 'c', 'r', 'd', 'b', 'm', 'z', 'l'} */
        result = yp_unionN( a, 1, b );
        ExpectEqual( 8, yp_lenC( result, &exc ) );
        ExpectEqual( yp_True, yp_in( i_z, result ) );
        ExpectEqual( yp_False, yp_in( i_q, result ) );
        yp_decref( result );

        /* Letters in both a and b: {'a', 'c'} */
        result = yp_intersectionN( a, 1, b );
        ExpectEqual( 2, yp_lenC( result, &exc ) );
        ExpectEqual( yp_True, yp_in( i_a, result ) );
        ExpectEqual( yp_False, yp_in( i_b, result ) );
        yp_decref( result );

        /* Letters in a or b but not both: {'r', 'd', 'b', 'm', 'z', 'l'} */
        result = yp_symmetric_difference( a, b );
        ExpectEqual( 6, yp_lenC( result, &exc ) );
        ExpectEqual( yp_True, yp_in( i_z, result ) );
        ExpectEqual( yp_False, yp_in( i_a, result ) );
        yp_decref( result );

        /* You can clean-up multiple references with a single command */
        yp_decrefN( 2, a, b );
    }

    /* Immortals, including yp_None, don't need to be decref'd */
    ExpectEqual( yp_None, exc );
}


/*
 * Dictionaries
 */

/* Taken from Python's tutorial */
EXAMPLE( Dictionaries, SmallExample )
{
    /* Defining immortal int objects is usually not necessary; they are used in this example
     * as the values of a dict, although they could just as easily be mortal ints.  (Of course,
     * if you _do_ use immortal ints in your code, you'd be wise to give them better names than
     * "i_4098".) */
    yp_IMMORTAL_INT( i_4098, 4098 );
    yp_IMMORTAL_INT( i_4139, 4139 );
    yp_IMMORTAL_INT( i_4127, 4127 );

    /* yp_lenC and similar functions only modify exc on error; since they do not discard existing
     * values, exc should be initialized to yp_None or another immortal. */
    ypObject *exc = yp_None;
    ypObject *result;

    /* The 'K' in "yp_dictK" is a reminder that the variable arguments are n key/value pairs,
     * for a total of 2n arguments; as in Python, a key comes immediately before its value */
    ypObject *tel = yp_dictK( 2, b_jack, i_4098, b_sape, i_4139 );
    ExpectEqual( 2, yp_lenC( tel, &exc ) );

    /* Adding a key to the dict: {'sape': 4139, 'guido': 4127, 'jack': 4098} */
    yp_setitem( &tel, b_guido, i_4127 );
    ExpectEqual( 3, yp_lenC( tel, &exc ) );

    /* Retrieving a key from the dict returns a new reference that you must remember to
     * discard; commenting the calls that yield new references can help in this. */
    result = yp_getitem( tel, b_jack ); // new ref
    ExpectEqual( 4098, yp_asintC( result, &exc ) );
    yp_decref( result );

    /* Discarding a key from the dict: {'guido': 4127, 'jack': 4098} */
    yp_delitem( &tel, b_sape );
    ExpectEqual( 2, yp_lenC( tel, &exc ) );

    /* Adding another key: {'guido': 4127, 'irv': 4127, 'jack': 4098} */
    yp_setitem( &tel, b_irv, i_4127 );
    ExpectEqual( 3, yp_lenC( tel, &exc ) );

    /* The default iterator for dictionaries yields its keys, so yp_list results in:
     *  ['irv', 'guido', 'jack'] */
    result = yp_list( tel ); // new ref
    ExpectEqual( 3, yp_lenC( result, &exc ) );
    ExpectEqual( yp_True, yp_in( b_irv, result ) );
    ExpectEqual( yp_False, yp_in( i_4127, result ) );
    yp_decref( result );

    /* yp_in looks to see if the key is in the dictionary */
    ExpectEqual( yp_True, yp_in( b_guido, tel ) );
    ExpectEqual( yp_False, yp_not_in( b_jack, tel ) );
    ExpectEqual( yp_False, yp_in( i_4127, tel ) );

    /* Clean-up */
    yp_decref( tel );
}


/*
 * Conditional statements
 */

/* yp_ELIF allows for chaining conditions, just as in Python.  However, yp_ELSE_EXCEPT_AS doesn't
 * work in quite the same way... */
EXAMPLE( ConditionalStatements, ELIF )
{
    ypObject *cond1 = yp_bytesC( NULL, 0 );
    ypObject *cond2 = yp_intC( 5 );
    bool branch2_taken = false;

    yp_IF( cond1 ) {
        ExpectUnreachable( );  // Will not be executed, yp_bytesC( NULL, 0 ) is false
    } yp_ELIF( cond2 ) {
        // Will be executed, yp_intC( 5 ) is true; yp_getindexC fails, but unlike Python this
        // doesn't trigger yp_ELSE_EXCEPT_AS
        ExpectEqual( yp_IndexError, yp_getindexC( cond1, 20 ) );
        branch2_taken = true;
    } yp_ELSE {
        ExpectUnreachable( );  // Will not be executed, as cond2 was true
    } yp_ELSE_EXCEPT_AS( e ) {
        // Executed on error in cond1 or cond2, with e defined and set to exception object;
        // unlike Python, this is *not* executed when yp_getindexC fails
        ExpectUnreachable( );
    } yp_ENDIF
    // If you forget ENDIF, you'll get a "missing '}'" compile error

    // Ensure branch 2 was taken above, then clean up
    ExpectTrue( branch2_taken );
    yp_decrefN( 2, cond1, cond2 );
}

/* Sometimes you'll generate a new, mortal reference in your condition statement that needs to be
 * discarded: yp_IFd handles this (the 'd' is for "discard"). */
EXAMPLE( ConditionalStatements, IFd )
{
    ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"
    bool branch1_taken = false;

    // yp_getindexC returns a new reference that must be discarded, hence the use of yp_IFd
    yp_IFd( yp_getindexC( bytes, 0 ) ) {
        // Will be executed, as yp_getindexC( bytes, 0 ) is "A" which is true
        branch1_taken = true;
    } yp_ELIFd( yp_getindexC( bytes, 20 ) ) {  // Not executed since the first branch was taken
        ExpectUnreachable( );
    } yp_ELSE_EXCEPT {
        // Although the second yp_getindexC would cause an exception, since it is not executed
        // this branch won't be executed either
        ExpectUnreachable( );
    } yp_ENDIF

    // Ensure branch 1 was taken above, then clean-up
    ExpectTrue( branch1_taken );
    yp_decref( bytes );
}

/* Here's how you handle an exception occuring in one of your condition statements. */
EXAMPLE( ConditionalStatements, EXCEPT_AS )
{
    ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"
    bool except_taken = false;

    // yp_getindexC will return yp_IndexError as 20 is not valid
    yp_IFd( yp_getindexC( bytes, 20 ) ) {
        ExpectUnreachable( );  // Not executed since yp_getindexC failed
    } yp_ELSE_EXCEPT_AS( e ) {
        ExpectEqual( yp_IndexError, e );  // e gets the exception returned by yp_getindexC
        except_taken = true;
    } yp_ENDIF
    // e is still available for use, if you so desire, but you can ignore it as it's immortal

    // Ensure the right branch was taken, then clean-up
    ExpectTrue( except_taken );
    yp_decref( bytes );
}


/*
 * Loop statements
 */

/* yp_FOR has an else clause, just like Python. */
EXAMPLE( LoopStatements, FOR_ELSE )
{
    ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"
    ypObject *byte = yp_NameError;
    bool else_taken = false;

    yp_FOR( byte, bytes ) {
        // A new value is assigned to byte every time through the loop; previous values in byte are
        // automatically discarded
        ExpectEqual( yp_type( byte ), yp_t_int );
    } yp_FOR_ELSE {
        // Just like in Python, if yp_FOR loops over all values successfully, then the else clause
        // is executed
        else_taken = true;
    } yp_FOR_EXCEPT_AS( e ) {
        // Just like the other yp_*_EXCEPT_AS macros, it's only executed on error in bytes
        ExpectUnreachable( );
    } yp_ENDFOR
    // If you forget ENDFOR, you'll get a "missing '}'" compile error

    // yp_FOR assigns new references to byte, so don't forget to discard it when completed
    yp_decref( byte );

    // Ensure the right branch was taken, then clean-up
    ExpectTrue( else_taken );
    yp_decref( bytes );
}


// TODO In ypExamples, show why there is no dict constructor version that directly accepts strings
// (...because it's so easy to create immortals for bytes/str)


    // End of ypExamples
    system("pause"); // FIXME why isn't VC++ pausing?
    return ypExamples_result;
}

int main( int argc, char *argv[], char *envp[] )
{
    // Enable memory leak detection, if supported by the heap
#ifdef _MSC_VER
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    yp_initialize( NULL );
    return ypExamples( ); // TODO complete
}

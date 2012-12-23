/*
 * ypExamples.c - Contains a number of nohtyP examples and recipies
 *      http://nohtyp.wordpress.com
 *      Copyright © 2001-2012 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/py3k/license.html
 */

#include "nohtyP.h"
#include <stdio.h>
#include <process.h> // FIXME for system

// FIXME track mallocs/inrefs/decrefs/frees and ensure no memory leaks

/*
 * Supporting functions and macros
 * (You can skip this section)
 */

typedef int bool;
#define false 0
#define true 1

#define RECIPIE( name ) do { ypRecipies_recipie_name = #name; printf( "\nRecipie %s\n", ypRecipies_recipie_name ); _flushall( );} while(0);
#define PrintError( fmt, ... ) fprintf( stderr, "  line %d: " fmt "\n", __LINE__, __VA_ARGS__ )
#define Expect( cond, fmt, ... ) do { if( cond ) break; ypRecipies_result++; PrintError( fmt, __VA_ARGS__ ); } while(0)
// TODO include exception name
#define ExpectTrue( act ) Expect( act, #act " is not true", 0 )
#define ExpectUnreachable( ) Expect( false, "unexpectedly reachable code", 0 )
#define ExpectEqual( exp, act ) Expect( (exp) == (act), #exp " != " #act, 0 )
#define ExpectNoException( obj ) Expect( !yp_isexceptionC( obj ), "unexpected exception (0x%08X)", (obj) )

int ypRecipies( void )
{
    int ypRecipies_result = 0; // return value from ypRecipies: number of failures
    char *ypRecipies_recipie_name = ""; // current recipie name TODO needed?


/*
 * Building lists
 */

/* Exceptions propagate: when given as an input they are returned immediately.  This recipie shows
 * that when building a list, you are free to ignore exceptions until the end. */
RECIPIE( BuildingLists_DiscardOnException )
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
RECIPIE( BuildingLists_KeepOnException )
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
 * Conditional statements
 */

/* yp_ELIF allows for chaining conditions, just as in Python.  However, yp_ELSE_EXCEPT_AS doesn't
 * work in quite the same way... */
// FIXME looks like yp_IF isn't executing _any_ of the conditions
RECIPIE( ConditionalStatements_ELIF )
{
    ypObject *cond1 = yp_bytesC( NULL, 0 );
    ypObject *cond2 = yp_intC( 5 );
    bool branch2_taken = false;
    ypObject *e = yp_None; // only set if exception occurs FIXME do we want this??

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
        // Executed on error in cond1 or cond2, with e set to exception object; unlike Python,
        // this is *not* executed when yp_getindexC fails
        ExpectUnreachable( );
    } yp_ENDIF
    // If you forget ENDIF, you'll get a "missing '}'" compile error
    
    // Ensure branch 2 was taken above, then clean up
    ExpectTrue( branch2_taken );
    yp_decrefN( 2, cond1, cond2 );
}

/* Sometimes you'll generate a new, mortal reference in your condition statement that needs to be
 * discarded: yp_IFd handles this (the 'd' is for "discard"). */
RECIPIE( ConditionalStatements_IFd )
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
RECIPIE( ConditionalStatements_EXCEPT_AS )
{
    ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"
    ypObject *e = yp_None; // good to initialize this if you plan to use it after yp_ENDIF
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

// TODO In ypExamples, show why there is no dict constructor version that directly accepts strings
// (...because it's so easy to create immortals for bytes/str)


    // End of ypRecipies
    system("pause"); // FIXME why isn't VC++ pausing?
    return ypRecipies_result;
} 

int main(int argc, char *argv[], char *envp[])
{
    yp_initialize( );
    return ypRecipies( ); // TODO complete
}

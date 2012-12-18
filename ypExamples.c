/*
 * ypExamples.c - Contains a number of nohtyP examples and recipies
 *      http://nohtyp.wordpress.com
 *      Copyright © 2001-2012 Python Software Foundation; All Rights Reserved
 *      License: http://docs.python.org/py3k/license.html
 */

#include "nohtyP.h"
#include <stdio.h>

// FIXME track mallocs/inrefs/decrefs/frees and ensure no memory leaks

/*
 * Supporting functions and macros
 *
 * (Recipies currently implemented as blocks inside of a single "ypRecipies" function)
 */

#define RECIPIE( name ) do { ypRecipies_recipie_name = #name; printf( "\nRecipie %s\n", ypRecipies_recipie_name ); _flushall( );} while(0);
#define RecipiePrintError( fmt, ... ) fprintf( stderr, "Recipie %s, line %d: " fmt "\n", ypRecipies_recipie_name, __LINE__, __VA_ARGS__ )
#define RecipieExpect( cond, fmt, ... ) do { if( cond ) break; ypRecipies_result++; RecipiePrintError( fmt, __VA_ARGS__ ); } while(0)
// TODO include exception name
#define ExpectUnreachable( ) RecipieExpect( 0, "unexpectedly reachable code", 0 )
#define ExpectEqual( exp, act ) RecipieExpect( (exp) == (act), #exp " != " #act, 0 )
#define ExpectNoException( obj ) RecipieExpect( !yp_isexceptionC( obj ), "unexpected exception (0x%08X)", (obj) )

int ypRecipies( void )
{
    int ypRecipies_result = 0; // return value from ypRecipies: number of failures
    char *ypRecipies_recipie_name = ""; // current recipie name


/*
 * Building lists
 */

/* Build list( range( 5 ) ), discarding it on any error */
RECIPIE( BuildingLists_DiscardOnException )
{
    ypObject *list = yp_listN( 0 );
    ypObject *item;
    int i;

    for( i = 0; i < 5; i++ ) {
        // If intC fails, item set to an exception
        item = yp_intC( i );
        
        // If item or list is an exception, or append fails, list discarded and replaced with an
        // exception
        yp_append( &list, item );
        
        // append creates its own reference to item, so we need to discard ours; this is
        // safe even if item is an exception, as exceptions are immortal
        yp_decref( item );
    }
    
    // Because exceptions propagate in the loop above, we need only check for them at the end
    ExpectNoException( list );

    // Clean-up
    yp_decref( list );
}

/* Build list( "\xFE\xFF" ), retaining the list on errors */
RECIPIE( BuildingLists_KeepOnException )
{
    ypObject *list = yp_listN( 0 );
    ypObject *item;
    ypObject *working;
    ypObject *exc = yp_None;
    int i;

    for( i = 0; i < 5; i++ ) {
        // When chrC fails (on values greater than 0xFF), item will be set to exception
        item = yp_chrC( 0xFE+i );
        if( i < 2 ) ExpectNoException( item );
        
        // As append will discard list if item is an exception, we need to pass append a separate 
        // reference
        working = yp_incref( list );
        yp_append( &working, item );
        yp_decref( working );
        
        // append creates its own reference to item, so we need to discard ours
        yp_decref( item );
    }

    // Although the last three chrC calls failed, the first two worked, so our length should be 2
    ExpectNoException( list );
    ExpectEqual( 2, yp_lenC( list, &exc ) );

    // Clean-up
    yp_decref( list );
}

// TODO more BuildingLists examples


/*
 * Conditional statements
 */

/* Successful if/elif/else, with exception handling */
RECIPIE( ConditionalStatements_ELIF )
{
    ypObject *cond1 = yp_bytesC( NULL, 0 );
    ypObject *cond2 = yp_intC( 5 );
    ypObject *e = yp_None; // only set if exception occurs FIXME do we want this??

    yp_IF( cond1 ) {
        ExpectUnreachable( );  // Will not be executed, b"" is false
    } yp_ELIF( cond2 ) {
        // Will be executed, int( 5 ) is true; yp_getindexC will return yp_MethodError
        yp_getindexC( cond2, 20 );
    } yp_ELSE {
        ExpectUnreachable( );  // Will not be executed, as cond2 was true
    } yp_ELSE_EXCEPT_AS( e ) {
        // Executed on error in cond1 or cond2, with e set to exception object
        // *Not* executed when getitem, or any other branch, causes an exception
        ExpectUnreachable( );
    } yp_ENDIF
    // If you forget ENDIF, you'll get a "missing '}'" compile error
    
    // Clean-up
    yp_decrefN( 2, cond1, cond2 );
}

/* Successful if/elif, with exception handling */
RECIPIE( ConditionalStatements_IFd )
{
    ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"

    // yp_getindexC returns a new reference that must be discarded, hence the 'd' in yp_IFd
    yp_IFd( yp_getindexC( bytes, 0 ) ) {
        // Will be executed, as bytes[0] is "A" which is true
    } yp_ELIFd( yp_getindexC( bytes, 20 ) ) {
        ExpectUnreachable( );  // Not executed since the first branch was taken
    } yp_ELSE_EXCEPT {
        // Although the second getitem would cause an exception, since it is not executed this
        // branch won't be executed either
        ExpectUnreachable( );
    } yp_ENDIF
    
    // Clean-up
    yp_decref( bytes );
}

/* Exception in if statement */
RECIPIE( ConditionalStatements_EXCEPT_AS )
{
    ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"
    ypObject *e = yp_None; // good to initialize this if you plan to use it after yp_ENDIF

    // yp_getindexC will return yp_IndexError as 20 is not valid
    yp_IFd( yp_getindexC( bytes, 20 ) ) {
        // Not executed since getitem failed
        ExpectUnreachable( );
    } yp_ELSE_EXCEPT_AS( e ) {
        // This branch is executed instead
        ExpectEqual( yp_IndexError, e );
    } yp_ENDIF
    // e is still available for use, if you so desire, but you can ignore it as it's immortal
    
    // Clean-up
    yp_decref( bytes );
}

// TODO In ypExamples, show why there is no dict constructor version that directly accepts strings
// (...because it's so easy to create immortals for bytes/str)


    // End of ypRecipies
    _getch( ); // FIXME why isn't VC++ pausing?
    return ypRecipies_result;
} 

int main(int argc, char *argv[], char *envp[])
{
    yp_initialize( );
    return ypRecipies( ); // TODO complete
}

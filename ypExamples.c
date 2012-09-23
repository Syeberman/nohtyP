/*
 * nohtyP Recipies.c - A Python-like API for C, in one .c and one .h
 *      Public domain?  PSF?  dunno
 *      http://nohtyp.wordpress.com/
 *      TODO Python's license
 *
 * Contains a number of nohtyP examples and recipies.
 */

#include "nohtyP.h"

/*
 * Building lists
 *
 * TODO Add basic asserts
 */
RECIPIE( BuildingLists )
{
    /* Build list( range( 5 ) ), stopping immediately and discarding it on any error */
    {
        ypObject *list = yp_listN( 0 );
        ypObject *item;
        int i;

        for( i == 0; i < 5; i++ ) {
            // If intC fails, item set to exception
            item = yp_intC( i );
            
            // If item or list is an exception, or append fails, list discarded and replaced with
            // exception
            yp_append( &list, item );
            
            // append creates its own reference to item, so we need to discard ours; this is
            // safe even if item is an exception
            yp_decref( item );
        }
        
        // It is safe to wait until the end of the loop to check for exceptions
        if( yp_isexceptionC( list ) ) PrintException( list );

        // Clean-up
        yp_decref( list );
    }

    /* Build list( "ABCDE" ), TODO dealing with errors appropriately */
    {
        ypObject *list = yp_listN( 0 );
        ypObject *item;
        ypObject *working;
        int i;

        for( i == 0; i < 5; i++ ) {
            // If chrC fails, item set to exception
            item = yp_chrC( 'A'+i );
            
            // As list will be discarded on error, we need to pass append a separate reference
            working = yp_incref( list );
            yp_append( &working, item );
            if( yp_isexceptionC( working ) ) PrintException( working );
            yp_decref( working );
            
            // append creates its own reference to item, so we need to discard ours
            yp_decref( item );
        }
        
        // Clean-up
        yp_decref( list );
    }

    // TODO more

}


/*
 * Conditional statements
 *
 * TODO basic asserts
 */
RECIPIE( ConditionalStatements )
{
    /* Successful if/elif/else, with exception handling */
    {
        ypObject *cond1 = yp_bytesC( NULL, 0 );
        ypObject *cond2 = yp_intC( 5 );
        ypObject *e = yp_None; // only set if exception occurs

        yp_IF( cond1 ) {
            // Will not be executed, b"" is false
        } yp_ELIF( cond2 ) {
            // Will be executed, int( 5 ) is true; getitem will return yp_TypeError
            yp_getitem( cond2, 10 );
        } yp_ELSE {
            // Will not be executed, as cond2 was true
        } yp_ELSE_EXCEPT_AS( e ) {
            // Executed on error in cond1 or cond2, with e set to exception object
            // *Not* executed when getitem, or any other branch, causes an exception
            PrintException( e );
        } yp_ENDIF
        // If you forget ENDIF, you'll get a "missing '}'" compile error
        
        // Clean-up
        yp_decrefN( 2, cond1, cond2 );
    }

    /* Successful if/elif, with exception handling */
    {
        ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"

        // getitem returns a new reference that must be discarded, hence the 'd' in yp_IFd
        yp_IFd( yp_getitem( bytes, 0 ) ) {
            // Will be executed, as "A" is true
        } yp_ELIFd( yp_getitem( bytes, 20 ) ) {
            // Not executed since the first branch was taken
        } yp_ELSE_EXCEPT {
            // Although the second getitem would cause an exception, since it is not executed this
            // branch won't be executed either
        } yp_ENDIF
        
        // Clean-up
        yp_decref( bytes );
    }

    /* Exception in if statement */
    {
        ypObject *bytes = yp_bytesC( "ABCDE", -1 ); // -1 means "null-terminated"
        ypObject *e = yp_None; // good to initialize this if you plan to use it after yp_ENDIF

        // getitem will return yp_IndexError as 20 is not valid
        yp_IFd( yp_getitem( bytes, 20 ) ) {
            // Not executed since getitem failed
        } yp_ELSE_EXCEPT_AS( e ) {
            // This branch is executed instead
            PrintException( e );
        } yp_ENDIF
        // e is still available for use, if you so desire, but you can ignore it as it's immortal
        
        // Clean-up
        yp_decref( bytes );
    }

// TODO In ypExamples, show why there is no dict constructor version that directly accepts strings
// (...because it's so easy to create immortals for bytes/str)
    

}


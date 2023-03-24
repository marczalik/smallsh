/*****************************************************************************
 * Author: Marc Zalik
 * Date: 2022-11-12
 * Filename: UTL_smallsh.c
 *
 * This file contains the source code for utility functions used in smallsh.
 * These functions support:
 *      - Debug printing
 *      - Flushed printing to stdout
 *      - Opening files
 ****************************************************************************/

/*********************************************
 *                INCLUDES
 ********************************************/

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*********************************************
 *                DEFINES
 ********************************************/

#define FILE_PERM       (0644)

/*********************************************
 *           FUNCTION PROTOTYPES
 ********************************************/


/*********************************************
 *            MODULE VARIABLES
 ********************************************/


/*********************************************
 *               FUNCTIONS
 ********************************************/

/*****************************************************************************
 *
 * NAME
 *      UTL_DebugPrint
 *
 * DESCRIPTION
 *      This function takes a variable argument formatted print string and 
 *      only prints it to stdout if compiled with the DEBUG macro.
 *  
 ****************************************************************************/

int UTL_DebugPrint
    (
    char               *stmt,           /* The format string to print out   */
    ...                                 /* Variable number of arguments     */
    )
{
    /* Appease compiler warning */
    if( stmt )
    {
#ifdef DEBUG
        /* Set up variadic arguments and pass to vprintf. */
        va_list args;
        va_start( args, stmt );

        vprintf( stmt, args );
        fflush( NULL );

        va_end( args );
#endif
    }

    return( EXIT_SUCCESS );

} /* end - UTL_DebugPrint() */


/*****************************************************************************
 *
 * NAME
 *      UTL_FlushedPrintOut
 *
 * DESCRIPTION
 *      This function takes a variable argument formatted print string, prints
 *      it to stdout, and then fflushes the buffers.
 *  
 ****************************************************************************/

int UTL_FlushedPrintOut
    (
    char               *stmt,           /* The format string to print out   */
    ...                                 /* Variable number of arguments     */
    )
{
    /* Set up variadic arguments and pass to vprintf. */
    va_list args;
    va_start( args, stmt );

    vprintf( stmt, args );
    fflush( NULL );

    va_end( args );

    return( EXIT_SUCCESS );

} /* end - UTL_FlushedPrintOut() */

/*****************************************************************************
 *
 * NAME
 *      UTL_OpenFile
 *
 * DESCRIPTION
 *      Given a filename, attempts to open the file for reading or writing. 
 *      Exits with failure if file could not be opened.
 *  
 ****************************************************************************/

int UTL_OpenFile
    (
    const char         *fileName,       /* Name of the file to be opened    */
    bool                isReadOnly,     /* Open in read only mode           */
    bool                isWriteOnly     /* Open in write only mode          */
    )
{
    /******************************
    *  LOCAL VARIABLES
    ******************************/
    int         fd;
    int         flag;

    /******************************
    *  INITIALIZE VARIABLES
    ******************************/
    fd          = -1;
    flag        = 0;


    /* Set flags for Read Only mode. */
    if( isReadOnly )
    {
        flag = O_RDONLY;
    }
    /* Set flags for Write Only mode. */
    else if( isWriteOnly )
    {
        flag = O_WRONLY | O_CREAT | O_TRUNC;
    }

    /* Open the given file. */
    if( ( fd = open( fileName, flag, FILE_PERM ) ) == -1 )
    {
        err( errno, "Failed to open file %s.\n", fileName );
    }

    return( fd );

} /* end - UTL_OpenFile() */

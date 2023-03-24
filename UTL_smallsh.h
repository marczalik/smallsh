/*****************************************************************************
 * Author: Marc Zalik
 * Date: 2022-11-12
 * Filename: UTL_smallsh.h
 *
 * This file contains the public header information for UTL_smallsh.c.
 ****************************************************************************/

/*********************************************
 *              INCLUDE GUARD
 ********************************************/

#ifndef _UTL_SMALLSH_H_
#define _UTL_SMALLSH_H_

/*********************************************
 *                INCLUDES
 ********************************************/


/*********************************************
 *                DEFINES
 ********************************************/

#define TRUE    (true)
#define FALSE   (false)

#define OPEN_FILE_READ( _fp )   UTL_OpenFile( _fp, TRUE, FALSE )
#define OPEN_FILE_WRITE( _fp )  UTL_OpenFile( _fp, FALSE, TRUE )

/*********************************************
 *                TYPE DEFS
 ********************************************/


/*********************************************
 *           FUNCTION PROTOTYPES          
 ********************************************/

int UTL_DebugPrint
        (
        char               *stmt,           /* The format string to print out   */
        ...                                 /* Variable number of arguments     */
        );

int UTL_FlushedPrintOut
        (
        char               *stmt,           /* The format string to print out   */
        ...                                 /* Variable number of arguments     */
        );

int UTL_OpenFile
        (
        const char         *fileName,       /* Name of the file to be opened    */
        bool                isReadOnly,     /* Open in read only mode           */
        bool                isWriteOnly     /* Open in write only mode          */
        );

#endif

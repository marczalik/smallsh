/*****************************************************************************
 * Author: Marc Zalik
 * Date: 2022-10-22
 * Filename: smallsh.h
 *
 * This file contains the public header information for smallsh.c.
 ****************************************************************************/

/*********************************************
 *              INCLUDE GUARD
 ********************************************/

#ifndef _SMALLSH_H_
#define _SMALLSH_H_

/*********************************************
 *                INCLUDES
 ********************************************/

#include <stdbool.h>
#include <unistd.h>

/*********************************************
 *                DEFINES
 ********************************************/

#define BUF_SIZE        (2049)
#define MAX_ARGS        (512)
#define MAX_CHILDREN    (10)

/* Arguments */
#define PID_VAR         ("$$")
#define INPUT           ("<")
#define OUTPUT          (">")
#define BACKGROUND      ("&")

/* Commands */
#define COMMENT         ("#")
#define COMMENT_LEN     (1)
#define CD_CMD          ("cd")
#define CD_LEN          (3)
#define EXIT_CMD        ("exit")
#define EXIT_LEN        (5)
#define STATUS_CMD      ("status")
#define STATUS_LEN      (7)
#define HOME_ENV        ("HOME")

/* Background redirection */
#define DEV_NULL        ("/dev/null")

/*********************************************
 *                TYPE DEFS
 ********************************************/

/* Contains info necessary for parsing and running command. */
typedef struct cmdStruct
{
    char   *args[MAX_ARGS];
    char   *input;
    char   *output;
    bool    isRedirectInput;
    bool    isRedirectOutput;
    bool    isBackground;
} cmdStruct;

/*********************************************
 *           FUNCTION PROTOTYPES          
 ********************************************/

#endif

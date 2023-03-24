/*****************************************************************************
 * Author: Marc Zalik
 * Date: 2022-10-22
 * Filename: smallsh.c
 *
 * This file contains the source code for a small interactive shell. The shell
 * features 3 built-in commands: cd, status, and exit. All other commands are
 * run in child processes using exec. Commands can be run in background mode
 * by entering '&' as the last argument. Input and output redirection is
 * supported using '<' and '>', respectively. Child foreground processes can
 * be terminated using SIGINT. Foreground-only mode can be turned on and off
 * by using SIGTSTP.
 ****************************************************************************/


/*********************************************
 *                INCLUDES
 ********************************************/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "smallsh.h"
#include "UTL_smallsh.h"

/*********************************************
 *                DEFINES
 ********************************************/


/*********************************************
 *           FUNCTION PROTOTYPES
 ********************************************/

static int      AddChildPid
                    (
                    pid_t              pid              /* The format string to print out   */
                    );

static char    *ExpandPID
                    (
                    char               *arg             /* The string to expand             */
                    );

static int      ExternalCommand
                    (
                    cmdStruct          *command         /* The command to run as a child    */
                    );

static int      FreeArgs
                    (
                    cmdStruct          *command         /* The command whose args to free   */ 
                    );

static int      GetInput
                    (
                    char               *buf,            /* The buffer where to store input  */ 
                    size_t              n               /* The size of the buffer           */ 
                    );

static void     HandleSIGCHLD
                    (
                    int                 signum          /* The signal number                */ 
                    );

static void     HandleSIGTSTP
                    (
                    int                 signum          /* The signal number                */ 
                    );

static int      MyChangeDir
                    (
                    cmdStruct          *command         /* The command containing the
                                                            necessary info for CD            */ 
                    );
                    
static int      MyExit
                    (
                    cmdStruct          *command         /* The command containing exit      */ 
                    );

static int      MyStatus
                    (
                    void
                    );

static int      ParseCommand
                    (
                    char               *buf,            /* The buffer after user input rec'd*/ 
                    cmdStruct          *command         /* The command struct to fill up    */ 
                    );

static int      ReapZombies
                    (
                    void
                    );

static int      RemoveChildPid
                    (
                    pid_t               pid             /* The pid to remove from bg array  */ 
                    );

static int      RunCommand
                    (
                    cmdStruct          *command         /* The command to run               */ 
                    );

static int      SetSignalHandlers
                    (
                    void
                    );

/*********************************************
 *            MODULE VARIABLES
 ********************************************/

static pid_t                        childPids[ MAX_CHILDREN ];
static int                          childStatus                 = 0;
/* flags used by signal handlers */
static volatile sig_atomic_t        backgroundIgnoreSet         = FALSE;
static volatile sig_atomic_t        backgroundUnignoreSet       = FALSE;
static volatile sig_atomic_t        childTerminated             = FALSE;
static volatile sig_atomic_t        foregroundChild             = FALSE;
static volatile sig_atomic_t        ignoreBackground            = FALSE;

/*********************************************
 *               FUNCTIONS
 ********************************************/

/*****************************************************************************
 *
 * NAME
 *      main
 *
 * DESCRIPTION
 *      This function runs the main loop of the smallsh program. It sets up
 *      the signal handlers prior to entering the loop body, which continually
 *      resets the command data structures, gets new user input, parses the
 *      input, runs the command, and then checks for any zombie child
 *      processes.
 *  
 ****************************************************************************/

int main(void)
{
    /******************************
    *  LOCAL VARIABLES
    ******************************/
    char            buf[ BUF_SIZE ];
    cmdStruct       newCmd;

    /* Install signal handlers */
    SetSignalHandlers();

    for(;;)
    {
        /* Clear out previous command */
        memset( &newCmd, 0, sizeof( newCmd ) );
        memset( buf, '\0', sizeof( buf ) );

        /* Read input */
        GetInput( buf, sizeof( buf ) );

        /* Process input */
        ParseCommand( buf, &newCmd );

        /* Run command */
        RunCommand( &newCmd );

        /* Free args */
        FreeArgs( &newCmd );

        /* Clean up dead children. This flag is set by the SIGCHLD handler. */
        if( childTerminated == TRUE )
        {
            ReapZombies();
        }
    }

    exit( EXIT_SUCCESS );

} /* end - main() */


/*****************************************************************************
 *
 * NAME
 *      AddChildPid
 *
 * DESCRIPTION
 *      This function adds the given pid to the static array of child pids.
 *
 * NOTES
 *      Assumes a statically-allocated array of size MAX_CHILDREN.
 *  
 ****************************************************************************/

static int AddChildPid
    (
    pid_t              pid              /* The format string to print out   */
    )
{
    /*-------------------------------------------------------------
     * Find the first empty entry in the child pids array and store
     * the given pid there.
     *-----------------------------------------------------------*/
    for( size_t i = 0; i < MAX_CHILDREN; i++ )
    {
        if( childPids[ i ] == 0 )
        {
            UTL_DebugPrint( "Adding pid %d\n", pid );
            childPids[ i ] = pid;
            break;
        }
    }

    return( EXIT_SUCCESS );
}


/*****************************************************************************
 *
 * NAME
 *      ExpandPID
 *
 * DESCRIPTION
 *      Given a pointer to a string, this function will heap-allocate space
 *      for a new string in which all instances of the '$$' variable has been
 *      expanded to the current process's pid.
 *
 * NOTES
 *      The memory returned by this function must be freed by the caller.     
 *  
 ****************************************************************************/

static char *ExpandPID
    (
    char               *arg             /* The string to expand             */
    )
{
    /******************************
    *  LOCAL VARIABLES
    ******************************/
    size_t      argIdx;
    long        n;
    char       *lp_newArg;
    size_t      newArgIdx;
    pid_t       pid;
    char        pidStr[18];
    long        pidLen;

    /******************************
    *  INITIALIZE VARIABLES
    ******************************/
    argIdx      = 0;
    n           = strlen( arg );
    newArgIdx   = 0;
    pid         = getpid();
    /* Convert pid into string */
    snprintf( pidStr, sizeof( pidStr ), "%d", pid );
    pidLen      = strlen( pidStr );

    /*----------------------------------------------------------------
     * Allocate maximum possible memory, equal to number of times $$
     * can show up in the argument string. Initialize to 0's to
     * implicitly add NULL terminator to end of new argument string.
     *--------------------------------------------------------------*/
    lp_newArg   = calloc( ( n / 2 ) * pidLen + 1, sizeof( char ) );
    if( lp_newArg == NULL )
    {
        fprintf( stderr, "Failed to allocate space for arg %s\n", arg );
        exit( EXIT_FAILURE );
    }

    UTL_DebugPrint( "lp_newArg: %x\n", lp_newArg );
    
    /*-----------------------------------------------------------------
     * Loop until end of current argument. Strtok adds NULL terminator
     * to end of token, so guaranteed to hit '\0'.
     *---------------------------------------------------------------*/
    while( arg[argIdx] != '\0' )
    {
        UTL_DebugPrint( "argIdx and argIdx+1: %c\t%c\n", arg[argIdx], arg[argIdx+1] );
        if( arg[argIdx] == '$' && arg[argIdx+1] == '$' )
        {
            /* Replace pid variables with pid string */
            strncat( lp_newArg, pidStr, pidLen );
            /* Skip forward 1 extra spot to pass second $ */
            argIdx += 1;
            /*---------------------------------------------------
             * Skip forward in new string by size of pid string,
             * excluding NULL terminator.
             *-------------------------------------------------*/
            newArgIdx += pidLen;
        }
        else
        {
            /* Copy non-pid variables to new string */
            lp_newArg[newArgIdx] = arg[argIdx];
            newArgIdx += 1;
        }

        /* Update loop counter for next iteration */
        argIdx += 1;
    }

    UTL_DebugPrint( " new arg: %s\n", lp_newArg );

    return( lp_newArg );

} /* end - ExpandPID() */


/*****************************************************************************
 *
 * NAME
 *      ExternalCommand
 *
 * DESCRIPTION
 *      This function forks a child process, sets it up according to the
 *      specification of the command struct, and then calls exec() on the
 *      given external (non-built-in) command. If exec fails, the child
 *      process will exit with an exit status. The parent process will either
 *      wait for the child to terminate (foreground) or store the child's pid
 *      and continue on to processing the next command (background).
 *  
 ****************************************************************************/

static int ExternalCommand
    (
    cmdStruct          *command         /* The command to run as a child    */
    )
{
    /******************************
    *  LOCAL VARIABLES
    ******************************/
    int         inputFd;
    int         outputFd;
    
    /******************************
    *  INITIALIZE VARIABLES
    ******************************/
    inputFd     = -1;
    outputFd    = -1;

    pid_t pid = fork();

    switch( pid )
    {
        /* Could not create child process */
        case -1:
            fprintf( stderr, "Failed to fork child process\n" );
            break;
        /* Child process drops in here */
        case 0:
            UTL_DebugPrint( "Child Process: %d\n", pid );

            /* Reset SIGCHLD to default handler */
            struct sigaction SIGCHLD_action = {0};
            SIGCHLD_action.sa_handler = SIG_DFL;
            sigaction( SIGCHLD, &SIGCHLD_action, NULL );
            
            /* Ignore SIGTSTP signals */
            struct sigaction SIGTSTP_action = {0};
            SIGTSTP_action.sa_handler = SIG_IGN;
            sigaction( SIGTSTP, &SIGTSTP_action, NULL );

            /* Handle redirects */
            if( command->isRedirectOutput == TRUE )
            {
                UTL_DebugPrint( "Redirecting stdout to %s\n", command->output );
                outputFd = OPEN_FILE_WRITE( command->output );
                dup2( outputFd, STDOUT_FILENO );
            }
            if( command->isRedirectInput == TRUE )
            {
                UTL_DebugPrint( "Redirecting stdin from %s\n", command->input );
                inputFd = OPEN_FILE_READ( command->input );
                dup2( inputFd, STDIN_FILENO );
            }
            /* Stop ignoring SIGINT for foreground processes */
            if( command->isBackground == FALSE )
            {
                struct sigaction SIGINT_action = {0};
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction( SIGINT, &SIGINT_action, NULL );
            }
            /*--------------------------------------------------------------- 
             * Background processes that aren't otherwise redirecting their
             * inputs or outputs will have their inputs/outputs redirected
             * from/to dev/null, respectively.
             *-------------------------------------------------------------*/
            else
            {
                if( command->isRedirectOutput == FALSE )
                {
                    outputFd = OPEN_FILE_WRITE( DEV_NULL );
                    dup2( outputFd, STDOUT_FILENO );
                }
                if( command->isRedirectInput == FALSE )
                {
                    inputFd = OPEN_FILE_READ( DEV_NULL );
                    dup2( inputFd, STDIN_FILENO );
                }
            }
            
            /* exec only returns if it fails. Report failure, clean up, and exit. */
            if( execvp( command->args[0], command->args ) == -1 )
            {
                UTL_FlushedPrintOut( "%s: no such file or directory\n", command->args[0] );
            }

            /* Close open file descriptors */
            if( outputFd != -1 )
            {
                close( outputFd );
            }
            if( inputFd != -1 )
            {
                close( inputFd );
            }

            exit( EXIT_FAILURE );
            break;
        /* Parent process drops in here */
        default:
            UTL_DebugPrint( "Parent Process: %d\n", pid );

            /* Add child to list of child pids. */
            AddChildPid( pid );

            /* Wait and block until foreground process completes. */
            if( command->isBackground == FALSE )
            {
                /*----------------------------------------------------------------- 
                 * If SIGTSTP signal arrives while waiting for foreground process,
                 * need to know to print out message that we're changed FG/BG modes
                 * before next user input. This flag is used by the SIGTSTP handler
                 * to make that determination.
                 *---------------------------------------------------------------*/
                foregroundChild = TRUE;
                waitpid( pid, &childStatus, 0 );
                /* Remove child pid from pids array. */
                RemoveChildPid( pid );
                foregroundChild = FALSE;

                /* If child was terminated by a signal, display the signal to the user. */
                if( WIFSIGNALED( childStatus ) )
                {
                    UTL_FlushedPrintOut( "terminated by signal %d\n", WTERMSIG( childStatus ) );
                }
                /* If a child terminated normally with a non-0 status, get that status. */
                else if( childStatus != 0 )
                {
                    childStatus = WIFEXITED( childStatus );
                }
            }
            /* Inform user of background process's pid. */
            else
            {
                UTL_FlushedPrintOut( "background pid is %d\n", pid );
            }
            break;
    }

    return( EXIT_SUCCESS );

} /* end - ExternalCommand() */


/*****************************************************************************
 *
 * NAME
 *      FreeArgs
 *
 * DESCRIPTION
 *      This function frees all of the heap-allocated memory stored in the
 *      arguments array of a command struct.
 *
 * NOTES
 *      Assumes that the arguments array is a statically-allocated array of
 *      size MAX_ARGS.
 *  
 ****************************************************************************/

static int FreeArgs
    (
    cmdStruct          *command         /* The command whose args to free   */ 
    )
{
    /* Loop through command's args array and free each one. */
    for( size_t i = 0; i < MAX_ARGS; i++ )
    {
        /* Skip NULLS. */
        if( command->args[ i ] != NULL )
        {
            UTL_DebugPrint( "Freeing %s\n", command->args[ i ] );
            free( command->args[ i ] );
        }
    }

    return( EXIT_SUCCESS );

} /* end - FreeArgs() */


/*****************************************************************************
 *
 * NAME
 *      GetInput
 *
 * DESCRIPTION
 *      This function prompts the user for input using fgets().
 *
 * NOTES
 *      Any trailing newline is not removed from the string captured in this
 *      function.
 *  
 ****************************************************************************/

static int GetInput
    (
    char               *buf,            /* The buffer where to store input  */ 
    size_t              n               /* The size of the buffer           */ 
    )
{
    /*-----------------------------------------------------------------------
     * Print informative messages to user when SIGTSTP signal is dispatched
     * while a foreground process is running as soon as said process ends.
     *---------------------------------------------------------------------*/
    if( backgroundIgnoreSet )
    {
        UTL_FlushedPrintOut( "\nEntering foreground-only mode (& is now ignored)\n" );
        backgroundIgnoreSet = FALSE;
    }
    else if( backgroundUnignoreSet )
    {
        UTL_FlushedPrintOut( "\nExiting foreground-only mode\n" );
        backgroundUnignoreSet = FALSE;
    }

    /* Print user input prompt. */
    UTL_FlushedPrintOut( ": " );
    /* Read line from stdin */
    fgets( buf, n, stdin );

    return( EXIT_SUCCESS );

} /* end - GetInput() */


/*****************************************************************************
 *
 * NAME
 *      HandleSIGCHLD
 *
 * DESCRIPTION
 *      This is the signal handler for SIGCHLD signals for the parent process.
 *      It sets a global flag whenever a child process terminates. The flag
 *      is checked in main() and if set the parent process will reap any
 *      zombie child processes.
 *  
 ****************************************************************************/

static void HandleSIGCHLD
    (
    int                 signum          /* The signal number                */ 
    )
{
    /* Set flag that will be checked at the end of each loop in main. */
    childTerminated = TRUE;

    /* Appease compiler warning. */
    if( signum )
    {
        ;
    }
} /* end - HandleSIGCHLD() */


/*****************************************************************************
 *
 * NAME
 *      HandleSIGTSTP
 *
 * DESCRIPTION
 *      This is the signal handler for SIGTSTP. It checks whether we are
 *      currently in background-only mode before toggling the mode. It also
 *      checks whether we are currently waiting for a foreground child
 *      process to finish, in which case a flag is set so that the user is
 *      prompted of the mode change the next time the user is prompted for
 *      input.
 *  
 ****************************************************************************/

static void HandleSIGTSTP
    (
    int                 signum          /* The signal number                */ 
    )
{
    /* Currently in background mode. Set to foreground mode and tell user. */
    if( ignoreBackground == FALSE )
    {
        ignoreBackground = TRUE;

        /*-----------------------------------------------------
         * Currently waiting on foreground proces to complete.
         * Delay message until next user prompt.
         *---------------------------------------------------*/
        if( foregroundChild == TRUE )
        {
            backgroundIgnoreSet = TRUE;
        }
        else
        {
            write( STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n: ", 52 );
        }
    }
    /* Currently in foreground mode. Set to background mode and tell user. */
    else
    {
        ignoreBackground = FALSE;

        /*-----------------------------------------------------
         * Currently waiting on foreground proces to complete.
         * Delay message until next user prompt.
         *---------------------------------------------------*/
        if( foregroundChild == TRUE )
        {
            backgroundUnignoreSet = TRUE;
        }
        else
        {
            write( STDOUT_FILENO, "\nExiting foreground-only mode\n: ", 32 );
        }
       
    }

    /* Appease compiler warning. */
    if( signum )
    {
        ;
    }

} /* end - HandleSIGTSTP() */

/*****************************************************************************
 *
 * NAME
 *      MyChangeDir
 *
 * DESCRIPTION
 *      This function performs the built-in 'cd' command. If no argument is
 *      supplied, this will change the current directory to the vlaue of the
 *      HOME environment variable. Otherwise, it will attempt to change the
 *      directory to the supplied argument.
 *  
 ****************************************************************************/

static int MyChangeDir
    (
    cmdStruct          *command         /* The command containing the
                                           necessary info for CD            */ 
    )
{
    /* No directory given, use HOME environment variable instead. */
    if( command-> args[ 1 ] == NULL )
    {
        if( chdir( getenv( HOME_ENV ) ) == -1 )
        {
            fprintf( stderr, "Failed to change directory to HOME environment variable.\n" );
        }
    }
    /* Attempt to CD to directory provided. */
    else if( chdir( command->args[ 1 ] ) == -1 )
    {
        UTL_FlushedPrintOut( "Invalid path %s\n", command->args[ 1 ] );
    }

    return( EXIT_SUCCESS );

} /* end - MyChangeDir() */


/*****************************************************************************
 *
 * NAME
 *      MyExit
 *
 * DESCRIPTION
 *      This function performs the built-in 'exit' command. It will free all
 *      heap-allocated memory for the current command, terminate all open
 *      child processes, and then exit.
 *  
 ****************************************************************************/

static int MyExit
    (
    cmdStruct          *command         /* The command containing exit      */ 
    )
{
    /* Free all args in the command struct. */
    FreeArgs( command );

    /* Terminate all ongoing child processes. */
    for( size_t i = 0; i < MAX_CHILDREN; i++ )
    {
        if( childPids[ i ] != 0 )
        {
            /* Send signal to terminate child. */
            kill( childPids[ i ], SIGTERM );
            /* Wait for child to terminate. */
            waitpid( childPids[ i ], &childStatus, 0 );
        }
    }

    exit( EXIT_SUCCESS );

} /* end - MyExit() */


/*****************************************************************************
 *
 * NAME
 *      MyStatus
 *
 * DESCRIPTION
 *      This function performs the built-in 'status' command by printing out
 *      the exit status of the last terminated child process.
 *  
 ****************************************************************************/

static int MyStatus(void)
{
    /* Inform user of last non-built-in command's exit status. */
    UTL_FlushedPrintOut( "exit status %d\n", childStatus );

    return( EXIT_SUCCESS );

} /* end - MyStatus() */


/*****************************************************************************
 *
 * NAME
 *      ParseCommand
 *
 * DESCRIPTION
 *      This function takes user input and parses them into its constituent
 *      parts, filling up the command struct as appropriate. The string is
 *      tokenized on spaces and all tokens are expanded for any instances of 
 *      '$$'. The resulting heap-allocated string is then checked for its
 *      type. The command and most arguments are stored in the command
 *      struct's arg array except for the following:
 *          - Input and output symbols
 *          - The argument immediately following an input/output symbol
 *          - Background indicator '&' if it is the last argument
 *  
 ****************************************************************************/

static int ParseCommand
    (
    char               *buf,            /* The buffer after user input rec'd*/ 
    cmdStruct          *command         /* The command struct to fill up    */ 
    )
{
    /******************************
    *  LOCAL VARIABLES
    ******************************/
    char       *lp_arg;
    int         argCount;
    bool        inputFlag;
    bool        outputFlag;
    char       *lp_tempArg;

    /******************************
    *  INITIALIZE VARIABLES
    ******************************/
    lp_arg                      = NULL;
    lp_tempArg                  = NULL;
    argCount                    = 0;
    inputFlag                   = FALSE;
    outputFlag                  = FALSE;
    command->isBackground       = FALSE;
    command->isRedirectInput    = FALSE;
    command->isRedirectOutput   = FALSE;

    /* Loop up to MAX_ARGS number of times, filling up command's args array. */
    for( int i = 0; i < MAX_ARGS; i++ )
    {
        /* Tokenize input, ignoring ending newline. */
        lp_arg = strtok( buf, " \n" );
        /* Reached end up input tokens. */
        if( lp_arg == NULL )
        {
            break;
        }

        /* Set buf to NULL as required for subsequent calls to strtok. */
        buf = NULL;

        /* Expand $$ variable. */
        lp_tempArg = ExpandPID( lp_arg );

        /*----------------------------------------------- 
         * Build command struct based on argument value.
         *---------------------------------------------*/

        /*-----------------------------------------------
         * If input indicator was last argument seen,
         * use next argument as input redirection file.
         *---------------------------------------------*/
        if( inputFlag == TRUE )
        {
            command->input = lp_tempArg;
            inputFlag = FALSE;
        }
        /*-----------------------------------------------
         * If output indicator was last argument seen,
         * use next argument as output redirection file.
         *---------------------------------------------*/
        else if( outputFlag == TRUE )
        {
            command->output = lp_tempArg;
            outputFlag = FALSE;
        }
        /* Current argument is input indicator, set flag. */
        else if( strncmp( lp_tempArg, INPUT, 2 ) == 0 )
        {
            command->isRedirectInput = TRUE;
            inputFlag = TRUE;
            free( lp_tempArg );
        }
        /* Current argument is output indicator, set flag. */
        else if( strncmp( lp_tempArg, OUTPUT, 2 ) == 0 )
        {
            command->isRedirectOutput = TRUE;
            outputFlag = TRUE;
            free( lp_tempArg );
        }
        /* All other arguments are stored in argument array. */
        else
        {
            UTL_DebugPrint( "Argument %d is %s\n", i, lp_tempArg );
            command->args[ argCount ] = lp_tempArg;
            argCount++;
        }

    }

    /*----------------------------------------------------------------
     * Background commands only occur when & is the last character and
     * a SIGTSTP signal has not set the shell to ignore background
     * command requests.
     *--------------------------------------------------------------*/
    if( argCount > 0 && command->args[ argCount - 1] && ( strncmp( command->args[ argCount - 1 ], BACKGROUND, 2 ) == 0 ) )
    {
        UTL_DebugPrint( "Removing background arg %s\n", command->args[ argCount - 1 ] );
        if( ignoreBackground == FALSE )
        {
            command->isBackground = TRUE;
        }
        else
        {
            command->isBackground = FALSE;
        }
        free( command->args[ argCount - 1 ] );
        command->args[ argCount - 1 ] = NULL;
    }

    return( EXIT_SUCCESS );

} /* end - ParseCommand() */


/*****************************************************************************
 *
 * NAME
 *      ReapZombies
 *
 * DESCRIPTION
 *      This function will clean up any terminated child processes, updating
 *      the global child status appropriately and removing the child's pid
 *      from the child pid array.
 *  
 ****************************************************************************/

static int ReapZombies(void)
{
    /******************************
    *  LOCAL VARIABLES
    ******************************/
    pid_t       pid;

    /******************************
    *  INITIALIZE VARIABLES
    ******************************/
    pid         = 0;

    
    /* Loop until all zombie children are reaped. */
    while( ( pid = waitpid( -1, &childStatus, WNOHANG ) ) > 0 )
    {
        /* Inform user when background child has terminated. */
        UTL_FlushedPrintOut( "background pid %d is done: ", pid );
        
        /* If child was terminated by signal, inform user. */
        if( WIFSIGNALED( childStatus ) )
        {
            UTL_FlushedPrintOut( "terminated by signal %d\n", WTERMSIG( childStatus ) );
        }
        /* Otherwise, inform user of exit status. */
        else
        {
            UTL_FlushedPrintOut( "exit value %d\n", childStatus );
        }
        
        /* Find and remove child from pids array. */
        RemoveChildPid( pid );
        continue;
    }
    
    /* Reset flag */
    childTerminated = FALSE;

    return( EXIT_SUCCESS );

} /* end - ReapZombies() */


/*****************************************************************************
 *
 * NAME
 *      RemoveChildPid
 *
 * DESCRIPTION
 *      This function removes a given pid from the child pid array.      
 *
 * NOTES
 *      Assumes that the child pid array is statically-allocated and of size
 *      MAX_CHILDREN.
 *  
 ****************************************************************************/

static int RemoveChildPid
    (
    pid_t               pid             /* The pid to remove from bg array  */ 
    )
{
    /* Loop through array of background pids until pid found. */
    for( size_t i = 0; i < MAX_CHILDREN; i++ )
    {
        if( childPids[ i ] == pid )
        {
            UTL_DebugPrint( "Removing pid %d\n", pid );
            /* Clear out array index matching given pid. */
            childPids[ i ] = 0;
            break;
        }
    }

    return( EXIT_SUCCESS );

} /* end - RemoveChildPid() */


/*****************************************************************************
 *
 * NAME
 *      RunCommand
 *
 * DESCRIPTION
 *      This function runs the most recently entered and parsed command.     
 *  
 ****************************************************************************/

static int RunCommand
    (
    cmdStruct          *command         /* The command to run               */ 
    )
{
    /* Don't dereference NULL pointers */
    if( command == NULL )
    {
        fprintf( stderr, "Invalid Command Struct provided.\n" );
        return( EXIT_FAILURE );
    }

    /* Skip blank lines */
    if( command->args[ 0 ] == NULL )
    {
        ;
    }
    /* Skip comments. */
    else if( strncmp( command->args[ 0 ], COMMENT, COMMENT_LEN ) == 0 )
    {
        UTL_DebugPrint( "Comment Detected %s\n", command->args[ 0 ] );
        ;
    }
    /* Run CD command using built-in function. */
    else if( strncmp( command->args[ 0 ], CD_CMD, CD_LEN ) == 0 )
    {
        UTL_DebugPrint( "CD Detected %s\n", command->args[0] );
        MyChangeDir( command );
    }
    /* Run Exit command using built-in function. */
    else if( strncmp( command->args[ 0 ], EXIT_CMD, EXIT_LEN ) == 0 )
    {
        UTL_DebugPrint( "EXIT Detected %s\n", command->args[0] );
        MyExit( command );
    }
    /* Run Status command using built-in function. */
    else if( strncmp( command->args[ 0 ], STATUS_CMD, STATUS_LEN ) == 0 )
    {
        UTL_DebugPrint( "STATUS Detected %s\n", command->args[ 0 ] );
        MyStatus();
    }
    /* Run all other commands by forking a child process and calling exec(). */
    else
    {
        UTL_DebugPrint( "External Command Detected %s\n", command->args[ 0 ] );
        ExternalCommand( command );
    }

    return( EXIT_SUCCESS );

} /* end - RunCommand() */


/*****************************************************************************
 *
 * NAME
 *      SetSignalHandlers
 *
 * DESCRIPTION
 *      This function installs the signal handlers for the parent process. It
 *      will ignore SIGINT signals, and it will install HandleSIGCHLD and 
 *      HandleSIGTSTP.
 *  
 ****************************************************************************/

static int SetSignalHandlers(void)
{
    /* Ignore SIGINT */
    struct sigaction SIGINT_action = {0};

    SIGINT_action.sa_handler = SIG_IGN;
    
    sigaction( SIGINT, &SIGINT_action, NULL );

    /* Set up and register SIGCHLD handler */
    struct sigaction SIGCHLD_action = {0};

    SIGCHLD_action.sa_handler = HandleSIGCHLD;
    sigfillset( &SIGCHLD_action.sa_mask );
    SIGCHLD_action.sa_flags = SA_RESTART;

    sigaction( SIGCHLD, &SIGCHLD_action, NULL );

    /* Set up and register SIGTSTP handler */
    struct sigaction SIGTSTP_action = {0};

    SIGTSTP_action.sa_handler = HandleSIGTSTP;
    sigfillset( &SIGTSTP_action.sa_mask );
    SIGTSTP_action.sa_flags = SA_RESTART;

    sigaction( SIGTSTP, &SIGTSTP_action, NULL );
    
    return( EXIT_SUCCESS );

} /* end - SetSignalHandlers() */

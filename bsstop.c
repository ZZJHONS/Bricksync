/* -----------------------------------------------------------------------------
 *
 * Clean stop on SIGINT/SIGTERM or the Windows console events, see bsstop.h
 * Same zlib license as the rest of BrickSync.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "cpuconfig.h"
#include "cc.h"

#if CC_UNIX
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
#else
 #error Unknown/Unsupported platform!
#endif

#include "bsstop.h"


static volatile sig_atomic_t bsStopFlag = 0;
static volatile sig_atomic_t bsStopSource = 0;
static volatile sig_atomic_t bsStopDone = 0;

enum
{
  BS_STOP_SOURCE_NONE = 0,
  BS_STOP_SOURCE_SIGINT,
  BS_STOP_SOURCE_SIGTERM,
  BS_STOP_SOURCE_CTRLC,
  BS_STOP_SOURCE_CTRLBREAK,
  BS_STOP_SOURCE_CLOSE,
  BS_STOP_SOURCE_LOGOFF,
  BS_STOP_SOURCE_SHUTDOWN
};


int bsStopRequested( void )
{
  return ( bsStopFlag != 0 );
}

const char *bsStopSourceName( void )
{
  switch( bsStopSource )
  {
    case BS_STOP_SOURCE_SIGINT:
      return "SIGINT";
    case BS_STOP_SOURCE_SIGTERM:
      return "SIGTERM";
    case BS_STOP_SOURCE_CTRLC:
      return "Ctrl+C";
    case BS_STOP_SOURCE_CTRLBREAK:
      return "Ctrl+Break";
    case BS_STOP_SOURCE_CLOSE:
      return "console closed";
    case BS_STOP_SOURCE_LOGOFF:
      return "user logoff";
    case BS_STOP_SOURCE_SHUTDOWN:
      return "system shutdown";
    default:
      break;
  }
  return "";
}

void bsStopFinished( void )
{
  bsStopDone = 1;
  return;
}


#if CC_UNIX

static void bsStopSignalHandler( int signo )
{
  static const char message[] = "\nStop requested again, exiting immediately.\n";
  ssize_t written;
  if( bsStopFlag )
  {
    /* Second request: the default action ends the process */
    written = write( STDERR_FILENO, message, sizeof(message) - 1 );
    (void)written;
    signal( signo, SIG_DFL );
    raise( signo );
    return;
  }
  bsStopSource = ( signo == SIGTERM ? BS_STOP_SOURCE_SIGTERM : BS_STOP_SOURCE_SIGINT );
  bsStopFlag = 1;
  return;
}

void bsStopInit( void )
{
  struct sigaction sa;
  memset( &sa, 0, sizeof(struct sigaction) );
  sigemptyset( &sa.sa_mask );
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = bsStopSignalHandler;
  sigaction( SIGINT, &sa, 0 );
  sigaction( SIGTERM, &sa, 0 );
  return;
}

#elif CC_WINDOWS

int bsStopHandleConsoleEvent( unsigned long ctrltype )
{
  int source;
  switch( ctrltype )
  {
    case CTRL_C_EVENT:
      source = BS_STOP_SOURCE_CTRLC;
      break;
    case CTRL_BREAK_EVENT:
      source = BS_STOP_SOURCE_CTRLBREAK;
      break;
    case CTRL_CLOSE_EVENT:
      source = BS_STOP_SOURCE_CLOSE;
      break;
    case CTRL_LOGOFF_EVENT:
      source = BS_STOP_SOURCE_LOGOFF;
      break;
    case CTRL_SHUTDOWN_EVENT:
      source = BS_STOP_SOURCE_SHUTDOWN;
      break;
    default:
      return 0;
  }
  if( bsStopFlag )
  {
    /* Second Ctrl+C or Ctrl+Break: let Windows end the process */
    if( ( source == BS_STOP_SOURCE_CTRLC ) || ( source == BS_STOP_SOURCE_CTRLBREAK ) )
      return 0;
    return 1;
  }
  bsStopSource = source;
  bsStopFlag = 1;
  return 1;
}

/* Runs on a thread of its own, created by Windows */
static BOOL WINAPI bsStopConsoleHandler( DWORD ctrltype )
{
  int waitcount;
  if( !( bsStopHandleConsoleEvent( ctrltype ) ) )
    return FALSE;
  if( ( ctrltype == CTRL_CLOSE_EVENT ) || ( ctrltype == CTRL_LOGOFF_EVENT ) || ( ctrltype == CTRL_SHUTDOWN_EVENT ) )
  {
    /* Windows ends the process when this returns, and after about 5 seconds anyway:
       give the main loop that long to finish and save */
    for( waitcount = 0 ; !( bsStopDone ) && ( waitcount < 45 ) ; waitcount++ )
      Sleep( 100 );
  }
  return TRUE;
}

void bsStopInit( void )
{
  SetConsoleCtrlHandler( bsStopConsoleHandler, TRUE );
  return;
}

#endif

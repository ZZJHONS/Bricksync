/* -----------------------------------------------------------------------------
 *
 * stoptest: the clean-stop handlers of bsstop.c.
 *
 * Unix: SIGTERM and SIGINT set the stop flag and the process lives on (the main loop then exits
 * like "quit"); a second signal ends the process at once.
 * Windows: Ctrl+C, Ctrl+Break and console close set the flag; a second Ctrl+C is left to
 * Windows, which ends the process. The handler's decision is called directly: sending a real
 * console event would also hit the shell running the test.
 *
 * Same zlib license as the rest of BrickSync.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include "cpuconfig.h"
#include "cc.h"

#if CC_UNIX
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/wait.h>
#elif CC_WINDOWS
 #include <windows.h>
#endif

#include "bsstop.h"


static int failures = 0;

static void check( int condition, const char *what )
{
  printf( "%s %s\n", ( condition ? "ok:  " : "FAIL:" ), what );
  fflush( stdout );
  if( !( condition ) )
    failures++;
  return;
}


#if CC_UNIX

/* In a child: first SIGINT sets the flag (reported through the pipe), the second one kills */
static void testSecondSignalKills( void )
{
  int fds[2], status;
  pid_t pid;
  char answer;
  ssize_t got;

  if( pipe( fds ) )
  {
    check( 0, "pipe()" );
    return;
  }
  pid = fork();
  if( pid == 0 )
  {
    close( fds[0] );
    bsStopInit();
    raise( SIGINT );
    answer = ( bsStopRequested() ? '1' : '0' );
    if( write( fds[1], &answer, 1 ) != 1 )
      _exit( 4 );
    raise( SIGINT );
    /* Still alive: the second signal did not end us */
    _exit( 3 );
  }
  close( fds[1] );
  answer = 0;
  got = read( fds[0], &answer, 1 );
  close( fds[0] );
  waitpid( pid, &status, 0 );
  check( ( got == 1 ) && ( answer == '1' ), "child: first SIGINT sets the stop flag, process lives on" );
  check( WIFSIGNALED( status ) && ( WTERMSIG( status ) == SIGINT ), "child: second SIGINT ends the process" );
  return;
}

static void testPlatform( void )
{
  testSecondSignalKills();

  bsStopInit();
  check( !( bsStopRequested() ), "no stop before a signal" );
  check( strcmp( bsStopSourceName(), "" ) == 0, "no source before a signal" );
  raise( SIGTERM );
  check( bsStopRequested(), "SIGTERM sets the stop flag" );
  check( strcmp( bsStopSourceName(), "SIGTERM" ) == 0, "source named SIGTERM" );
  check( 1, "process still running after SIGTERM" );
  return;
}

#elif CC_WINDOWS

static void testPlatform( void )
{
  bsStopInit();
  check( !( bsStopRequested() ), "no stop before an event" );
  check( bsStopHandleConsoleEvent( 99 ) == 0, "unknown event is not ours" );
  check( !( bsStopRequested() ), "unknown event sets nothing" );
  check( bsStopHandleConsoleEvent( CTRL_C_EVENT ) == 1, "Ctrl+C handled" );
  check( bsStopRequested(), "Ctrl+C sets the stop flag" );
  check( strcmp( bsStopSourceName(), "Ctrl+C" ) == 0, "source named Ctrl+C" );
  check( bsStopHandleConsoleEvent( CTRL_C_EVENT ) == 0, "second Ctrl+C left to Windows (ends the process)" );
  check( bsStopHandleConsoleEvent( CTRL_BREAK_EVENT ) == 0, "Ctrl+Break while stopping left to Windows" );
  check( bsStopHandleConsoleEvent( CTRL_CLOSE_EVENT ) == 1, "console close while stopping still handled (the wait)" );
  check( strcmp( bsStopSourceName(), "Ctrl+C" ) == 0, "source stays the first one" );
  return;
}

#endif


int main( void )
{
  testPlatform();
  bsStopFinished();
  if( failures )
  {
    printf( "stoptest: %d FAILED\n", failures );
    return 1;
  }
  printf( "stoptest: all passed\n" );
  return 0;
}

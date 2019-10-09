/* -----------------------------------------------------------------------------
 *
 * Copyright (c) 2014-2019 Alexis Naveros.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "iolog.h"

/* For mkdir() */
#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <wincon.h>
 #include <direct.h>
#else
 #error Unknown/Unsupported platform!
#endif


////


#define IO_LOG_FORCE_FLUSH (1)


////


/*
 * Normally, we would set the stdin file descriptor as non-blocking.
 *
 * And of course, Windows doesn't support such a simple thing, so
 * we have to create a thread just to read input in a non-blocking
 * yet buffered manner. Isn't that great?
 *
 * Yeah, right. (Screw you Windows.)
 */


#define IO_STDIN_BUFFER_SIZE (4096)

/* We use global variables because there's just one stdin */
static int volatile ioStdinReadyFlag;
static mtThread ioStdinThread;
static char ioStdinBuffer[IO_STDIN_BUFFER_SIZE];
static mtMutex ioStdinMutex;
static mtSignal ioStdinSignal;
static mtSignal ioStdinSignalRead;


static void *ioStdinMain( void *value )
{
  for( ; ; )
  {
    if( !( fgets( ioStdinBuffer, IO_STDIN_BUFFER_SIZE, stdin ) ) )
      continue;
    mtMutexLock( &ioStdinMutex );
    ioStdinReadyFlag = 1;
    mtSignalBroadcast( &ioStdinSignalRead );
    for( ; ioStdinReadyFlag ; )
      mtSignalWait( &ioStdinSignal, &ioStdinMutex );
    mtMutexUnlock( &ioStdinMutex );
  }
  return 0;
}


void ioStdinInit()
{
  ioStdinReadyFlag = 0;
  mtThreadCreate( &ioStdinThread, ioStdinMain, 0, 0, 0, 0 );
  mtMutexInit( &ioStdinMutex );
  mtSignalInit( &ioStdinSignal );
  mtSignalInit( &ioStdinSignalRead );
  return;
}


char *ioStdinReadLock( int waitmilliseconds )
{
  mtMutexLock( &ioStdinMutex );
  if( ioStdinReadyFlag )
    return ioStdinBuffer;
  if( waitmilliseconds > 0 )
  {
    mtSignalWaitTimeout( &ioStdinSignalRead, &ioStdinMutex, waitmilliseconds );
    if( ioStdinReadyFlag )
      return ioStdinBuffer;
  }
  mtMutexUnlock( &ioStdinMutex );
  return 0;
}

void ioStdinUnlock()
{
  ioStdinReadyFlag = 0;
  mtSignalBroadcast( &ioStdinSignal );
  mtMutexUnlock( &ioStdinMutex );
  return;
}

/* Returned string must be freed */
char *ioStdinReadAlloc( int waitmilliseconds )
{
  int length;
  char *string, *input;

  input = ioStdinReadLock( waitmilliseconds );
  if( !( input ) )
    return 0;
  length = strlen( input );
  string = malloc( length + 1 );
  memcpy( string, input, length + 1 );
  ioStdinUnlock();
  return string;
}


////


static int ioLogReopen( ioLog *log, struct tm *timeinfo )
{
  char *strpath;
  FILE *file;
  char timebuf[64];

  if( ( log->tm_year == timeinfo->tm_year ) && ( log->tm_month == timeinfo->tm_mon ) && ( log->tm_day == timeinfo->tm_mday ) )
    return 1;
  log->tm_year = timeinfo->tm_year;
  log->tm_month = timeinfo->tm_mon;
  log->tm_day = timeinfo->tm_mday;

  strftime( timebuf, 64, "%Y-%m-%d.txt", timeinfo );
  strpath = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "bricksync-%s", log->logpath, timebuf );
  file = fopen( strpath, "a" );

  if( file )
  {
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", timeinfo );
    fprintf( file, "\n========== Log Start ~ %s ==========\n\n", timebuf );
    if( log->file )
      fclose( log->file );
    log->file = file;
  }
  else
  {
    printf( "LOG ERROR : Failed to open \"%s\" for writing.", strpath );
    if( log->file )
      fprintf( log->file, "LOG ERROR : Failed to open \"%s\" for writing.", strpath );
  }
  free( strpath );

  return ( log->file ? 1 : 0 );
}


int ioLogInit( ioLog *log, char *logpath, int flushinterval )
{
  struct tm timeinfo;

#if CC_UNIX
  mkdir( logpath, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
#elif CC_WINDOWS
  _mkdir( logpath );
#else
 #error Unknown/Unsupported platform!
#endif

  memset( log, 0, sizeof(ioLog) );
  log->logpath = strdup( logpath );
  log->curtime = time( 0 );
  log->flushinterval = flushinterval;
  timeinfo = *( localtime( &log->curtime ) );
  if( !( ioLogReopen( log, &timeinfo ) ) )
    return 0;
#if CC_UNIX
  printf( "\33[0m\33[40m\33[37m" );
#endif

  return 1;
}

void ioLogEnd( ioLog *log )
{
  if( !( log ) )
    return;
  free( log->logpath );
  log->logpath = 0;
  fclose( log->file );
  log->file = 0;
  return;
}

void ioLogFlush( ioLog *log )
{
  fflush( stdout );
  if( log->file )
    fflush( log->file );
  return;
}

void ioLogSetOutputFile( ioLog *log, FILE *outputfile )
{
  log->outputfile = outputfile;
  return;
}


////


enum
{
  IO_CODE_BLACK,
  IO_CODE_DKRED,
  IO_CODE_DKGREEN,
  IO_CODE_BROWN,
  IO_CODE_DKBLUE,
  IO_CODE_DKMAGENTA,
  IO_CODE_DKCYAN,
  IO_CODE_GRAY,
  IO_CODE_DKGRAY,
  IO_CODE_RED,
  IO_CODE_GREEN,
  IO_CODE_YELLOW,
  IO_CODE_BLUE,
  IO_CODE_MAGENTA,
  IO_CODE_CYAN,
  IO_CODE_WHITE,

  IO_CODE_DEFAULT,
  IO_CODE_COUNT
};


static void ioHandleColorCode( ioLog *log, int color )
{
#if CC_UNIX
  char *colorstring;

  printf( "\33[0m\33[40m" );
  colorstring = 0;
  switch( color )
  {
    case IO_CODE_BLACK:
      colorstring = "\33[30m";
      break;
    case IO_CODE_DKRED:
      colorstring = "\33[31m";
      break;
    case IO_CODE_DKGREEN:
      colorstring = "\33[32m";
      break;
    case IO_CODE_BROWN:
      colorstring = "\33[33m";
      break;
    case IO_CODE_DKBLUE:
      colorstring = "\33[34m";
      break;
    case IO_CODE_DKMAGENTA:
      colorstring = "\33[35m";
      break;
    case IO_CODE_DKCYAN:
      colorstring = "\33[36m";
      break;
    case IO_CODE_GRAY:
      colorstring = "\33[37m";
      break;
    case IO_CODE_DKGRAY:
      colorstring = "\33[30;1m";
      break;
    case IO_CODE_RED:
      colorstring = "\33[31;1m";
      break;
    case IO_CODE_GREEN:
      colorstring = "\33[32;1m";
      break;
    case IO_CODE_YELLOW:
      colorstring = "\33[33;1m";
      break;
    case IO_CODE_BLUE:
      colorstring = "\33[34;1m";
      break;
    case IO_CODE_MAGENTA:
      colorstring = "\33[35;1m";
      break;
    case IO_CODE_CYAN:
      colorstring = "\33[36;1m";
      break;
    case IO_CODE_WHITE:
      colorstring = "\33[37;1m";
      break;
    case IO_CODE_DEFAULT:
      colorstring = "\33[37m";
      break;
    default:
      return;
  }
  if( colorstring )
    printf( "%s", colorstring );
#else
  int colormask;
  HANDLE console;
  switch( color )
  {
    case IO_CODE_BLACK:
      colormask = 0;
      break;
    case IO_CODE_DKRED:
      colormask = FOREGROUND_RED;
      break;
    case IO_CODE_DKGREEN:
      colormask = FOREGROUND_GREEN;
      break;
    case IO_CODE_BROWN:
      colormask = FOREGROUND_RED | FOREGROUND_GREEN;
      break;
    case IO_CODE_DKBLUE:
      colormask = FOREGROUND_BLUE;
      break;
    case IO_CODE_DKMAGENTA:
      colormask = FOREGROUND_RED | FOREGROUND_BLUE;
      break;
    case IO_CODE_DKCYAN:
      colormask = FOREGROUND_GREEN | FOREGROUND_BLUE;
      break;
    case IO_CODE_GRAY:
      colormask = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
      break;
    case IO_CODE_DKGRAY:
      colormask = FOREGROUND_INTENSITY;
      break;
    case IO_CODE_RED:
      colormask = FOREGROUND_RED | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_GREEN:
      colormask = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_YELLOW:
      colormask = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_BLUE:
      colormask = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_MAGENTA:
      colormask = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_CYAN:
      colormask = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_WHITE:
      colormask = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
      break;
    case IO_CODE_DEFAULT:
      colormask = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
      break;
    default:
      return;
  }
  console = GetStdHandle( STD_OUTPUT_HANDLE );
  if( console )
    SetConsoleTextAttribute( console, colormask );
#endif

  return;
}


static void ioPrintStdoutString( ioLog *log, char *string )
{
  int writelength, colorcode, endlinecount;
  char *writebase;

  endlinecount = 0;
  if( log )
    endlinecount = log->endlinecount;
  writelength = 0;
  writebase = string;
  for( ; *string ; string++ )
  {
    if( *string != IO_COLOR_ESCAPE )
    {
      /* Track how many consecutive line breaks at the end of stdout */
      endlinecount = ( *string == '\n' ? endlinecount + 1 : 0 );
      writelength++;
    }
    else
    {
      if( writelength )
        printf( "%.*s", (int)writelength, writebase );
      string++;
      writelength = 0;
      if( !( *string ) )
        break;
      colorcode = *string - IO_COLOR_CODE_BASE;
      ioHandleColorCode( log, colorcode );
      writelength = 0;
      writebase = string+1;
    }
  }
  if( writelength )
    printf( "%.*s", (int)writelength, writebase );
  if( log )
    log->endlinecount = endlinecount;
  if( !( endlinecount ) )
    fflush( stdout );

  return;
}


static void ioPrintFileString( ioLog *log, FILE *file, char *string )
{
  int writelength;
  char *writebase;

  writelength = 0;
  writebase = string;
  for( ; *string ; string++ )
  {
    if( *string != IO_COLOR_ESCAPE )
      writelength++;
    else
    {
      if( writelength )
        fprintf( file, "%.*s", (int)writelength, writebase );
      string++;
      writelength = 0;
      if( !( *string ) )
        break;
      writelength = 0;
      writebase = string+1;
    }
  }
  if( writelength )
    fprintf( file, "%.*s", (int)writelength, writebase );

  return;
}


void ioPrintf( ioLog *log, int modemask, char *format, ... )
{
  char *string;
  int strsize, allocsize;
  va_list ap;
  time_t curtime;
  struct tm timeinfo;
  char timebuf[64];

  allocsize = 512;
  string = malloc( allocsize );
  for( ; ; )
  {
    va_start( ap, format );
    strsize = vsnprintf( string, allocsize, format, ap );
    va_end( ap );
#if CC_WINDOWS
    if( strsize == -1 )
      strsize = allocsize << 1;
#endif
    if( strsize < allocsize )
      break;
    allocsize = strsize + 2;
    string = realloc( string, allocsize );
  }

  if( !( modemask & IO_MODEBIT_LOGONLY ) )
  {
    if( ( log ) && ( log->linemarker ) )
    {
#if CC_UNIX
      printf( "\r" );
      printf( "                                        " );
      printf( "\r" );
#elif CC_WINDOWS
      HANDLE console;
      CONSOLE_SCREEN_BUFFER_INFO consoleinfo;
      COORD cursor;
      console = GetStdHandle( STD_OUTPUT_HANDLE );
      if( console )
      {
        if( GetConsoleScreenBufferInfo( console, &consoleinfo ) )
        {
          cursor.X = 0;
          cursor.Y = consoleinfo.dwCursorPosition.Y;
          SetConsoleCursorPosition( console, cursor );
        }
      }
#endif
    }
    ioPrintStdoutString( log, string );
    if( modemask & IO_MODEBIT_FLUSH )
      fflush( stdout );
  }

  if( log )
  {
    curtime = time( 0 );
    if( ( curtime - log->curtime ) >= log->flushinterval )
      modemask |= IO_MODEBIT_FLUSH;
    log->curtime = curtime;
    timeinfo = *( localtime( &log->curtime ) );
    /* Update log file */
    if( !( modemask & IO_MODEBIT_NODATE ) )
      ioLogReopen( log, &timeinfo );
    if( !( modemask & IO_MODEBIT_LOGONLY ) )
    {
      /* Track line marker flag */
      log->linemarker = 0;
      if( modemask & IO_MODEBIT_LINEMARKER )
        log->linemarker = 1;
    }
    /* Write to log file */
    if( ( log->file ) && !( modemask & IO_MODEBIT_NOLOG ) )
    {
      /* Skip leading new lines */
      if( !( modemask & IO_MODEBIT_LOGONLY ) )
        for( ; *format == '\n' ; format++ );
      if( *format )
      {
        if( !( modemask & IO_MODEBIT_NODATE ) )
        {
          strftime( timebuf, 64, "%H:%M:%S", &timeinfo );
          fprintf( log->file, "%s ", timebuf );
        }
        ioPrintFileString( log, log->file, string );
        if( modemask & IO_MODEBIT_FLUSH )
          fflush( log->file );
#if IO_LOG_FORCE_FLUSH
        fflush( log->file );
#endif
      }
    }
    if( ( modemask & IO_MODEBIT_OUTPUTFILE ) && ( log->outputfile ) )
    {
      ioPrintFileString( log, log->outputfile, string );
      if( modemask & IO_MODEBIT_FLUSH )
        fflush( log->outputfile );
#if IO_LOG_FORCE_FLUSH
      fflush( log->outputfile );
#endif
    }
  }

  free( string );

  return;
}



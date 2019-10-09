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
#include "debugtrack.h"


////


#define DEBUG_TRACKER_ENTRY_COUNT (256)

typedef struct
{
  int line;
  char *file;
} debugTrackerEntry;

typedef struct
{
  mtThread thread;
  debugTrackerEntry entry[DEBUG_TRACKER_ENTRY_COUNT];
  int entrycount;
  int64_t activitytime;
} debugTracker;

static int debugTrackerInitFlag = 0;
static int debugTrackerCount = 0;
static int debugTrackerWatchInterval = 0;
static int debugTrackerStallTime = 0;
static debugTracker *debugTrackerList = 0;
static int64_t debugTrackerActivityTime = 0;
static mtMutex debugTrackerMutex;
static mtThread debugTrackerThread;

static void *debugTrackerThreadMain( void *value )
{
  int stallflag, trackerindex, entryindex;
  int64_t activitytime, currenttime;
  FILE *debugfile;
  debugTracker *tracker;
  debugTrackerEntry *entry;

  for( ; ; )
  {
    mtMutexLock( &debugTrackerMutex );
    activitytime = debugTrackerActivityTime;
    mtMutexUnlock( &debugTrackerMutex );
    currenttime = ccGetMillisecondsTime();
    if( currenttime > ( activitytime + debugTrackerStallTime ) )
    {
      printf( "\nWARNING: Watchdog timer kicked in, activity appears stalled since %.3f seconds.\n", (double)( currenttime - activitytime ) / 1000.0 );
      tracker = debugTrackerList;
      for( trackerindex = 0 ; trackerindex < debugTrackerCount ; trackerindex++, tracker++ )
        printf( "Thread %d last activity : %.3f seconds ago.\n", trackerindex, (double)( currenttime - tracker->activitytime ) / 1000.0 );
      printf( "You have encountered a bug and you can help fix it.\n" );
      printf( "Please send the files \"debugtracker.txt\" and the latest log file (from /data/logs/) to alexis@rayforce.net\n" );
      printf( "Thank you!\n" );
      debugfile = fopen( "debugtracker.txt", "a" );
      if( debugfile )
      {
        fprintf( debugfile, "\nWARNING: Watchdog timer kicked in, activity appears stalled since %.3f seconds.\n", (double)( currenttime - activitytime ) / 1000.0 );
        tracker = debugTrackerList;
        for( trackerindex = 0 ; trackerindex < debugTrackerCount ; trackerindex++, tracker++ )
        {
          fprintf( debugfile, "Thread %d last activity : %.3f seconds ago.\n", trackerindex, (double)( currenttime - tracker->activitytime ) / 1000.0 );
          for( entryindex = 0 ; entryindex < tracker->entrycount ; entryindex++ )
          {
            entry = &tracker->entry[entryindex];
            fprintf( debugfile, "Log #%d : file %s, line %d\n", entryindex, entry->file, entry->line );
          }
        }
        fclose( debugfile );
      }
      fflush( 0 );
    }
    ccSleep( debugTrackerWatchInterval );
  }

  return 0;
}


////


int debugTrackerInit( int watchintervalmsecs, int stalltimemsecs )
{
  debugTrackerActivityTime = ccGetMillisecondsTime();
  debugTrackerWatchInterval = watchintervalmsecs;
  debugTrackerStallTime = stalltimemsecs;
  mtMutexInit( &debugTrackerMutex );
  mtThreadCreate( &debugTrackerThread, debugTrackerThreadMain, 0, MT_THREAD_FLAGS_JOINABLE, 0, 0 );
  debugTrackerInitFlag = 1;
  return 1;
}


static debugTracker *debugTrackerAlloc()
{
  debugTracker *tracker;
  debugTrackerList = realloc( debugTrackerList, ( debugTrackerCount + 1 ) * sizeof(debugTracker) );
  tracker = &debugTrackerList[ debugTrackerCount ];
  memset( tracker, 0, sizeof(debugTracker) );
  debugTrackerCount++;
  return tracker;
}


void debugTrackerSet( char *file, int line )
{
  int trackindex;
  mtThread thread;
  debugTracker *tracker;
  debugTrackerEntry *entry;

  if( !( debugTrackerInitFlag ) )
    return;
  thread = mtThreadSelf();
  mtMutexLock( &debugTrackerMutex );
  for( trackindex = 0 ; ; trackindex++ )
  {
    if( trackindex == debugTrackerCount )
    {
      tracker = debugTrackerAlloc();
      tracker->thread = thread;
      break;
    }
    tracker = &debugTrackerList[ trackindex ];
    if( mtThreadCmpEqual( &tracker->thread, &thread ) )
      break;
  }

  if( tracker->entrycount == DEBUG_TRACKER_ENTRY_COUNT )
  {
    memmove( &tracker->entry[0], &tracker->entry[1], (DEBUG_TRACKER_ENTRY_COUNT-1)*sizeof(debugTrackerEntry) );
    tracker->entrycount--;
  }
  entry = &tracker->entry[ tracker->entrycount ];
  entry->file = file;
  entry->line = line;
  tracker->entrycount++;

  tracker->activitytime = ccGetMillisecondsTime();
  mtMutexUnlock( &debugTrackerMutex );
  return;
}


void debugTrackerActivity()
{
  if( !( debugTrackerInitFlag ) )
    return;
  mtMutexLock( &debugTrackerMutex );
  debugTrackerActivityTime = ccGetMillisecondsTime();
  mtMutexUnlock( &debugTrackerMutex );
  return;
}


void debugTrackerClear()
{
  int trackerindex;
  debugTracker *tracker;

  mtMutexLock( &debugTrackerMutex );
  tracker = debugTrackerList;
  for( trackerindex = 0 ; trackerindex < debugTrackerCount ; trackerindex++, tracker++ )
  {
    memmove( &tracker->entry[0], &tracker->entry[tracker->entrycount-1], sizeof(debugTrackerEntry) );
    tracker->entrycount = 1;
  }
  mtMutexUnlock( &debugTrackerMutex );

  return;
}


////


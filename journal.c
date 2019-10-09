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
#include <limits.h>
#include <float.h>


#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "debugtrack.h"
#include "iolog.h"

#include "journal.h"


/* Unix specific */
#if CC_UNIX
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <errno.h>
#endif

#if CC_WINDOWS
 #include <windows.h>
#endif



////


#ifndef ADDRESS
 #define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

static inline int intMin( int x, int y )
{
  return ( x < y ? x : y );
}

static inline int intMax( int x, int y )
{
  return ( x > y ? x : y );
}


////


int journalDirSync( const char *dirpath )
{
#if CC_UNIX
  int fd, retval;
  if( ( fd = open( dirpath, O_RDONLY, 0 ) ) == -1 )
    return -1;
  retval = 1;
  if( ( fsync( fd ) == -1 ) && ( errno == EIO ) )
    retval = 0;
  close( fd );
  return retval;
#else
  return 1;
#endif
}

int journalEntryDirSync( const char *filepath )
{
#if CC_UNIX
  int retval;
  size_t dirlen;
  const char *trail;
  char *dirpath;

  /* Skip trailing slashes */
  for( trail = &filepath[ strlen( filepath ) - 1 ] ; ( trail > filepath ) && ( *trail == '/' ) ; trail-- );
  /* Find the last slash if any */
  for( ; ( trail > filepath ) && ( *trail != '/' ) ; trail-- );

  /* Build dirpath */
  dirlen = 0;
  dirpath = ".";
  if( *trail == '/' )
  {
    dirlen = 1;
    if( trail != filepath )
      dirlen = trail - filepath;
    dirpath = malloc( dirlen + 1 );
    memcpy( dirpath, filepath, dirlen );
    dirpath[dirlen] = 0;
  }

  retval = journalDirSync( dirpath );

  if( dirlen )
    free( dirpath );
  return retval;
#else
  return 1;
#endif
}

/* Rename file with directory synchronization */
int journalRenameSync( char *oldpath, char *newpath, int retryflag )
{
#if CC_WINDOWS
  int attemptindex, attemptcount;
  /* On Windows, file indexing or anti-virus software could be scanning the file and prevent rename() */
  attemptcount = ( retryflag ? 16 : 1 );
  for( attemptindex = 0 ; ; attemptindex++ )
  {
    if( MoveFileEx( oldpath, newpath, MOVEFILE_REPLACE_EXISTING ) )
      break;
    if( attemptindex >= attemptcount )
      return 0;
    ccSleep( 250 );
  }
#else
  if( rename( oldpath, newpath ) )
    return 0;
#endif
#if CC_UNIX
  if( !( journalEntryDirSync( oldpath ) ) )
    printf( "JOURNAL WARNING: Sync parent directory of \"%s\" failed\n", oldpath );
  if( !( journalEntryDirSync( newpath ) ) )
    printf( "JOURNAL WARNING: Sync parent directory of \"%s\" failed\n", newpath );
#endif
  return 1;
}


////


int journalAlloc( journalDef *journal, int entryalloc )
{
  DEBUG_SET_TRACKER();

  journal->entryarray = 0;
  journal->entryalloc = entryalloc;
  journal->entrycount = 0;
  if( entryalloc )
  {
    journal->entryarray = malloc( journal->entryalloc * sizeof(journalEntry) );
    if( !( journal->entryarray ) )
      return 0;
  }
  return 1;
}

void journalAddEntry( journalDef *journal, char *oldpath, char *newpath, int oldallocflag, int newallocflag )
{
  journalEntry *entry;

  DEBUG_SET_TRACKER();

  if( journal->entrycount >= journal->entryalloc )
  {
    journal->entryalloc = intMax( 8, journal->entryalloc << 1 );
    journal->entryarray = realloc( journal->entryarray, journal->entryalloc * sizeof(journalEntry) );
  }
  entry = &journal->entryarray[ journal->entrycount ];
  entry->oldpath = oldpath;
  entry->newpath = newpath;
  entry->allocflags = 0;
  if( oldallocflag )
    entry->allocflags |= 0x1;
  if( newallocflag )
    entry->allocflags |= 0x2;
  journal->entrycount++;
  return;
}

void journalFree( journalDef *journal )
{
  int entryindex;
  journalEntry *entry;

  DEBUG_SET_TRACKER();

  for( entryindex = 0 ; entryindex < journal->entrycount ; entryindex++ )
  {
    entry = &journal->entryarray[entryindex];
    if( entry->allocflags & 0x1 )
      free( entry->oldpath );
    if( entry->allocflags & 0x2 )
      free( entry->newpath );
  }
  free( journal->entryarray );
  journal->entryarray = 0;
  journal->entryalloc = 0;
  journal->entrycount = 0;
  return;
}


////


int journalExecute( char *journalpath, char *tempjournalpath, ioLog *log, journalEntry *entryarray, int entrycount )
{
#if CC_UNIX
  int fd;
#else
  FILE *file;
#endif
  int entryindex, retval;
  ccGrowth growth;
  char *oldpath, *newpath;

  DEBUG_SET_TRACKER();

  ccGrowthInit( &growth, 4096 );
  ccGrowthPrintf( &growth, "%d", entrycount );
  ccGrowthSeek( &growth, growth.offset + 1 );
  for( entryindex = 0 ; entryindex < entrycount ; entryindex++ )
  {
    ccGrowthPrintf( &growth, "%s", entryarray[entryindex].oldpath );
    ccGrowthSeek( &growth, growth.offset + 1 );
    ccGrowthPrintf( &growth, "%s", entryarray[entryindex].newpath );
    ccGrowthSeek( &growth, growth.offset + 1 );
  }

  /* Unix specific */
#if CC_UNIX
  if( ( fd = open( tempjournalpath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR ) ) == -1 )
  {
    ccGrowthFree( &growth );
    ioPrintf( log, IO_MODEBIT_FLUSH, "JOURNAL ERROR: Failed to open/create \"%s\" (%s)\n", tempjournalpath, strerror( errno ) );
    return 0;
  }
  retval = 1;
  if( write( fd, growth.data, growth.offset ) != growth.offset )
    retval = 0;
 #if CC_LINUX
  fdatasync( fd );
 #else
  fsync( fd );
 #endif
  if( close( fd ) != 0 )
    retval = 0;
#else
  if( !( file = fopen( tempjournalpath, "wb" ) ) )
  {
    ccGrowthFree( &growth );
    ioPrintf( log, IO_MODEBIT_FLUSH, "JOURNAL ERROR: Failed to open/create \"%s\" (%s)\n", tempjournalpath, strerror( errno ) );
    return 0;
  }
  retval = 1;
  if( fwrite( growth.data, 1, growth.offset, file ) != growth.offset )
    retval = 0;
  /* fsync()?? */
  if( fclose( file ) != 0 )
    retval = 0;
#endif

  ccGrowthFree( &growth );

  if( retval )
  {
    /* Put the fully written journal in place */
    if( !( journalRenameSync( tempjournalpath, journalpath, 1 ) ) )
    {
      ioPrintf( log, IO_MODEBIT_FLUSH, "JOURNAL ERROR: Failed to rename \"%s\" to \"%s\" (%s)\n", tempjournalpath, journalpath, strerror( errno ) );
      return 0;
    }
    for( entryindex = 0 ; entryindex < entrycount ; entryindex++ )
    {
      oldpath = entryarray[entryindex].oldpath;
      newpath = entryarray[entryindex].newpath;
      if( !( journalRenameSync( oldpath, newpath, 1 ) ) )
      {
        ioPrintf( log, IO_MODEBIT_FLUSH, "JOURNAL ERROR: Failed to rename \"%s\" to \"%s\" (%s)\n", oldpath, newpath, strerror( errno ) );
        retval = 0;
      }
    }
    remove( journalpath );
  }

  return retval;
}


int journalReplay( char *journalpath )
{
  int offset;
  int32_t entryindex, entrycount;
  size_t journalsize, srcsize;
  char *journaldata;
  char *src;
  char *oldpath, *newpath;

  DEBUG_SET_TRACKER();

  journaldata = ccFileLoad( journalpath, 16*1048576, &journalsize );
  if( !( journaldata ) )
    return 1;
  if( journalsize < 5 )
    goto error;
  if( !( ccStrParseInt32( journaldata, &entrycount ) ) )
    goto error;
  src = ADDRESS( journaldata, sizeof(int32_t) + 1 );
  srcsize = journalsize - ( sizeof(int32_t) + 1 );

  for( entryindex = 0 ; entryindex < entrycount ; entryindex++ )
  {
    oldpath = src;
    offset = ccSeqFindChar( src, srcsize, '\0' );
    if( offset < 0 )
      goto error;
    offset++;
    src += offset;
    srcsize -= offset;

    newpath = src;
    offset = ccSeqFindChar( src, srcsize, '\0' );
    if( offset < 0 )
      goto error;
    offset++;
    src += offset;
    srcsize -= offset;

    /* Rename may fail when transaction was partially complete, no big deal */
    journalRenameSync( oldpath, newpath, 0 );
  }

  free( journaldata );
  remove( journalpath );

  return 1;

////

  error:
  free( journaldata );
  return 0;
}



/* -----------------------------------------------------------------------------
 *
 * journaltest: checks BrickSync's rename journal (journal.c), its crash recovery.
 *
 * BrickSync saves a transaction (inventory + order files) as temp files, writes the list of
 * temp -> final renames to a journal, then renames. A crash after the journal is written but
 * before every rename is done is repaired at the next start by journalReplay().
 * - journalExecute() with 1 and 12 entries: every file renamed, journal removed;
 * - a journal left by a "crash" (written, no renames done), with 1 and 12 entries:
 *   journalReplay() finishes every rename and removes the journal;
 * - a damaged journal is refused and left alone.
 * Exit code 0 when every case behaves, 1 otherwise. Built and run by build.sh test / CI.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "iolog.h"
#include "journal.h"


#define JT_JOURNAL "jt-journal.txt"
#define JT_JOURNAL_TEMP "jt-journal.tmp"

static int failures = 0;

static void check( int condition, const char *what )
{
  printf( "%s %s\n", ( condition ? "ok:  " : "FAIL:" ), what );
  if( !( condition ) )
    failures++;
  return;
}

static int fileExists( const char *path )
{
  FILE *file;
  if( !( file = fopen( path, "rb" ) ) )
    return 0;
  fclose( file );
  return 1;
}

static int fileHolds( const char *path, const char *content )
{
  char buffer[256];
  size_t size;
  FILE *file;
  if( !( file = fopen( path, "rb" ) ) )
    return 0;
  size = fread( buffer, 1, sizeof(buffer) - 1, file );
  fclose( file );
  buffer[size] = 0;
  return ( strcmp( buffer, content ) == 0 );
}

static void writeFile( const char *path, const char *content )
{
  FILE *file;
  file = fopen( path, "wb" );
  fputs( content, file );
  fclose( file );
  return;
}

static void entryPaths( int index, char *oldpath, char *newpath, char *content )
{
  sprintf( oldpath, "jt-%02d.tmp", index );
  sprintf( newpath, "jt-%02d.txt", index );
  sprintf( content, "file %d", index );
  return;
}

/* Temp files to rename; finals hold stale content, as an older state file would */
static void prepare( int count )
{
  int index;
  char oldpath[64], newpath[64], content[64];
  for( index = 0 ; index < count ; index++ )
  {
    entryPaths( index, oldpath, newpath, content );
    writeFile( oldpath, content );
    writeFile( newpath, "stale" );
  }
  return;
}

static int allRenamed( int count )
{
  int index;
  char oldpath[64], newpath[64], content[64];
  for( index = 0 ; index < count ; index++ )
  {
    entryPaths( index, oldpath, newpath, content );
    if( fileExists( oldpath ) || !( fileHolds( newpath, content ) ) )
      return 0;
  }
  return 1;
}

static void cleanup( int count )
{
  int index;
  char oldpath[64], newpath[64], content[64];
  for( index = 0 ; index < count ; index++ )
  {
    entryPaths( index, oldpath, newpath, content );
    remove( oldpath );
    remove( newpath );
  }
  remove( JT_JOURNAL );
  remove( JT_JOURNAL_TEMP );
  return;
}

/* The journal exactly as journalExecute() writes it, left behind by a crash before any rename */
static void writeCrashedJournal( int count )
{
  int index;
  char oldpath[64], newpath[64], content[64];
  FILE *file;
  file = fopen( JT_JOURNAL, "wb" );
  fprintf( file, "%d", count );
  fputc( 0, file );
  for( index = 0 ; index < count ; index++ )
  {
    entryPaths( index, oldpath, newpath, content );
    fputs( oldpath, file );
    fputc( 0, file );
    fputs( newpath, file );
    fputc( 0, file );
  }
  fclose( file );
  return;
}

static void testExecute( ioLog *log, int count )
{
  int index;
  char what[128];
  journalDef journal;
  char oldpath[64], newpath[64], content[64];

  cleanup( count );
  prepare( count );
  journalAlloc( &journal, count );
  for( index = 0 ; index < count ; index++ )
  {
    entryPaths( index, oldpath, newpath, content );
    journalAddEntry( &journal, strdup( oldpath ), strdup( newpath ), 1, 1 );
  }
  sprintf( what, "journalExecute() with %d entries returns success", count );
  check( journalExecute( JT_JOURNAL, JT_JOURNAL_TEMP, log, journal.entryarray, journal.entrycount ), what );
  sprintf( what, "journalExecute() with %d entries renamed every file", count );
  check( allRenamed( count ), what );
  sprintf( what, "journalExecute() with %d entries removed the journal", count );
  check( !( fileExists( JT_JOURNAL ) ), what );
  journalFree( &journal );
  cleanup( count );
  return;
}

static void testReplay( int count )
{
  char what[128];

  cleanup( count );
  prepare( count );
  writeCrashedJournal( count );
  sprintf( what, "journalReplay() of a crashed %d-entry journal returns success", count );
  check( journalReplay( JT_JOURNAL ), what );
  sprintf( what, "journalReplay() of a crashed %d-entry journal finished every rename", count );
  check( allRenamed( count ), what );
  sprintf( what, "journalReplay() of a crashed %d-entry journal removed the journal", count );
  check( !( fileExists( JT_JOURNAL ) ), what );
  cleanup( count );
  return;
}

static void testDamaged( void )
{
  FILE *file;
  cleanup( 1 );
  prepare( 1 );
  /* Says 3 entries, holds one and a half */
  file = fopen( JT_JOURNAL, "wb" );
  fwrite( "3\0jt-00.tmp\0jt-00.txt\0jt-01", 1, 27, file );
  fclose( file );
  check( !( journalReplay( JT_JOURNAL ) ), "a damaged journal is refused" );
  check( fileExists( JT_JOURNAL ), "a damaged journal is left in place" );
  cleanup( 1 );
  return;
}


int main( int argc, char **argv )
{
  ioLog log;

  setvbuf( stdout, 0, _IONBF, 0 );
  if( !( ioLogInit( &log, "jt-logs", 0 ) ) )
  {
    printf( "FAIL: could not open the test log\n" );
    return 1;
  }
  check( journalReplay( JT_JOURNAL ), "no journal: replay has nothing to do" );
  testExecute( &log, 1 );
  testExecute( &log, 12 );
  testReplay( 1 );
  testReplay( 12 );
  testDamaged();
  ioLogEnd( &log );
  printf( "%s\n", ( failures ? "JOURNAL TEST FAILED" : "JOURNAL TEST PASSED" ) );
  return ( failures ? 1 : 0 );
}

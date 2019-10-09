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
#include <string.h>
#include <math.h>
#include <time.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"

#include "cryptsha1.h"


/* For mkdir() */
#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <sys/types.h>
 #include <sys/stat.h>
#else
 #error Unknown/Unsupported platform!
#endif


#include "bsxpg.h"


////


#define PRICE_GUIDE_DEBUG (0)


////


static int bsxPgMkDir( char *path )
{
  int retval;
#if CC_UNIX
  retval = mkdir( path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
#elif CC_WINDOWS
  retval = _mkdir( path );
#else
 #error Unknown/Unsupported platform!
#endif
  return ( retval == 0 );
}


/* Returned string must be freed() */
char *bsxPriceGuidePath( char *basepath, char itemtypeid, char *itemid, int itemcolorid, int flags )
{
  char prefix0, prefix1;
  char *path;
  uint32_t sha1hash[SHA1_HASH_DIGEST_BITS/32];
  cryptSha1 sha1context;

  path = 0;
  if( flags & BSX_PRICEGUIDE_FLAGS_BRICKSTORE )
  {
    if( flags & BSX_PRICEGUIDE_FLAGS_MKDIR )
    {
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c", basepath, itemtypeid );
      bsxPgMkDir( path );
      free( path );
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%s", basepath, itemtypeid, itemid );
      bsxPgMkDir( path );
      free( path );
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%s" CC_DIR_SEPARATOR_STRING "%d", basepath, itemtypeid, itemid, itemcolorid );
      bsxPgMkDir( path );
      free( path );
    }
    path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%s" CC_DIR_SEPARATOR_STRING "%d" CC_DIR_SEPARATOR_STRING "priceguide.txt", basepath, itemtypeid, itemid, itemcolorid );
  }
  else if( flags & BSX_PRICEGUIDE_FLAGS_BRICKSTOCK )
  {
    cryptInitSha1( &sha1context );
    cryptDataSha1( &sha1context, (void *)itemid, strlen( itemid ) );
    cryptResultSha1( &sha1context, sha1hash );
    prefix0 = ( sha1hash[0] >> 28 ) & 0xf;
    prefix0 += ( prefix0 < 10 ? '0' : 'a'-10 );
    prefix1 = ( sha1hash[0] >> 24 ) & 0xf;
    prefix1 += ( prefix1 < 10 ? '0' : 'a'-10 );
    if( flags & BSX_PRICEGUIDE_FLAGS_MKDIR )
    {
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c", basepath, itemtypeid );
      bsxPgMkDir( path );
      free( path );
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c", basepath, itemtypeid, prefix0 );
      bsxPgMkDir( path );
      free( path );
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c", basepath, itemtypeid, prefix0, prefix1 );
      bsxPgMkDir( path );
      free( path );
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%s", basepath, itemtypeid, prefix0, prefix1, itemid );
      bsxPgMkDir( path );
      free( path );
      path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%s" CC_DIR_SEPARATOR_STRING "%d", basepath, itemtypeid, prefix0, prefix1, itemid, itemcolorid );
      bsxPgMkDir( path );
      free( path );
    }
    path = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%c" CC_DIR_SEPARATOR_STRING "%s" CC_DIR_SEPARATOR_STRING "%d" CC_DIR_SEPARATOR_STRING "priceguide.txt", basepath, itemtypeid, prefix0, prefix1, itemid, itemcolorid );
  }

  return path;
}


int bsxReadPriceGuide( bsxPriceGuide *pg, char *path, char itemcondition )
{
  int donemask, retvalue;
  int lineoffset;
  char *pgfile, *string;
  size_t pgsize;
#if CC_UNIX
  struct stat statbuf;
#elif CC_WINDOWS
  struct _stat statbuf;
#else
 #error
#endif
  /* PG read values */
  char pgc0, pgc1;
  int pgi0, pgi1;
  float pgf0, pgf1, pgf2, pgf3;

  pgfile = ccFileLoad( path, 1048576, &pgsize );
  if( !( pgfile ) )
  {
#if PRICE_GUIDE_DEBUG
    printf( "ERROR: We failed to read price guide at \"%s\"\n", path );
#endif
    return 0;
  }

  donemask = 0x0;
  memset( pg, 0, sizeof(bsxPriceGuide) );
  string = pgfile;
  lineoffset = 1;
  for( ; lineoffset > 0 ; string += lineoffset )
  {
    string = ccStrNextWord( string );
    if( !( string ) )
      break;

    lineoffset = ccStrFindChar( string, '\n' );
    if( *string == '#' )
      continue;

    if( sscanf( string, "%c %c %d %d %f %f %f %f\n", &pgc0, &pgc1, &pgi0, &pgi1, &pgf0, &pgf1, &pgf2, &pgf3 ) != 8 )
    {
#if PRICE_GUIDE_DEBUG
      printf( "WARNING: Scanf failure in \"%s\"\n", path );
#endif
      continue;
    }

#if PRICE_GUIDE_DEBUG
printf( "  PG Read : %c %c %d %d\n", pgc0, pgc1, pgi0, pgi1 );
#endif

    if( pgc1 != itemcondition )
      continue;
    if( pgc0 == 'P' )
    {
      pg->saleqty = pgi0;
      pg->salecount = pgi1;
      pg->saleminimum = pgf0;
      pg->saleaverage = pgf1;
      pg->saleqtyaverage = pgf2;
      pg->salemaximum = pgf3;
      donemask |= 0x1;
    }
    else if( pgc0 == 'C' )
    {
      pg->stockqty = pgi0;
      pg->stockcount = pgi1;
      pg->stockminimum = pgf0;
      pg->stockaverage = pgf1;
      pg->stockqtyaverage = pgf2;
      pg->stockmaximum = pgf3;
      donemask |= 0x2;
    }
  }

  retvalue = 0;
  if( ( donemask & 0x3 ) == 0x3 )
    retvalue = 1;

#if PRICE_GUIDE_DEBUG
  printf( "Pg Read : %d ( 0x%x )\n", retvalue, donemask );
#endif

#if CC_UNIX
  if( !( stat( path, &statbuf ) ) )
    pg->modtime = (int64_t)statbuf.st_mtime;
#elif CC_WINDOWS
  if( !( _stat( path, &statbuf ) ) )
    pg->modtime = (int64_t)statbuf.st_mtime;
#endif

  free( pgfile );
  return retvalue;
}


int bsxWritePriceGuide( bsxPriceGuide *pgnew, bsxPriceGuide *pgused, char *path, char itemtypeid, char *itemid, int itemcolorid )
{
  char timebuf[256];
  time_t curtime;
  FILE *pgfile;

#if PRICE_GUIDE_DEBUG
printf( "Writing Price Guide for : %c %s %d\n", itemtypeid, itemid, itemcolorid );
#endif

  /* Open price guide cache file */
  pgfile = fopen( path, "w" );
  if( !( pgfile ) )
  {
#if PRICE_GUIDE_DEBUG
    printf( "ERROR: We failed to write price guide at \"%s\"\n", path );
#endif
    return 0;
  }

  fprintf( pgfile, "# Price Guide for part %c:#%s, color #%d\n", itemtypeid, itemid, itemcolorid );
  curtime = time( 0 );
  strftime( timebuf, 256, "%H:%M:%S, %b %d %Y", localtime( &curtime ) );
  fprintf( pgfile, "# Last update: %s\n", timebuf );
  fprintf( pgfile, "#\n" );
  fprintf( pgfile, "A\tN\t0\t0\t0.0\t0.0\t0.0\t0.0\n" );
  fprintf( pgfile, "A\tU\t0\t0\t0.0\t0.0\t0.0\t0.0\n" );
  fprintf( pgfile, "P\tN\t%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", pgnew->saleqty, pgnew->salecount, pgnew->saleminimum, pgnew->saleaverage, pgnew->saleqtyaverage, pgnew->salemaximum );
  fprintf( pgfile, "P\tU\t%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", pgused->saleqty, pgused->salecount, pgused->saleminimum, pgused->saleaverage, pgused->saleqtyaverage, pgused->salemaximum );
  fprintf( pgfile, "C\tN\t%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", pgnew->stockqty, pgnew->stockcount, pgnew->stockminimum, pgnew->stockaverage, pgnew->stockqtyaverage, pgnew->stockmaximum );
  fprintf( pgfile, "C\tU\t%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", pgused->stockqty, pgused->stockcount, pgused->stockminimum, pgused->stockaverage, pgused->stockqtyaverage, pgused->stockmaximum );
  fclose( pgfile );

  return 1;
}


////



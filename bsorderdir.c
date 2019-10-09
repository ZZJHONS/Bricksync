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
#include "mmatomic.h"
#include "mmbitmap.h"
#include "iolog.h"
#include "debugtrack.h"
#include "cpuinfo.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <direct.h>
#else
 #error Unknown/Unsupported platform!
#endif

#include "tcp.h"
#include "tcphttp.h"
#include "oauth.h"
#include "exclperm.h"
#include "journal.h"

#include "bsx.h"
#include "bsxpg.h"
#include "json.h"
#include "bsorder.h"
#include "bricklink.h"
#include "brickowl.h"
#include "colortable.h"
#include "bstranslation.h"
#include "bsevalgrade.h"

#include "bricksync.h"


////


static int bsOrderDirAddEntry( bsOrderDir *dir, char *filepath, time_t filetime )
{
  int stroffset;
  int32_t readint;
  char *str;
  bsOrderDirEntry *entry;
  bsOrderDirEntry *entrylist;

  if( dir->entrycount >= dir->entryalloc )
  {
    dir->entryalloc <<= 1;
    if( dir->entryalloc < 1024 )
      dir->entryalloc = 1024;
    entrylist = realloc( dir->entrylist, dir->entryalloc * sizeof(bsOrderDirEntry) );
    if( !( entrylist ) )
      return 0;
    dir->entrylist = entrylist;
  }
  entry = &dir->entrylist[ dir->entrycount ];
  entry->filepath = filepath;
  entry->filename = filepath;
  entry->filetime = filetime;
  for( ; ; entry->filename += stroffset + 1 )
  {
    stroffset = ccStrFindChar( entry->filename, CC_DIR_SEPARATOR_CHAR );
    if( stroffset < 0 )
      break;
  }
  entry->ordertype = BS_ORDER_DIR_ORDER_TYPE_UNKNOWN;
  if( ccStrCmpSeq( "bricklink", entry->filename, strlen( "bricklink" ) ) )
    entry->ordertype = BS_ORDER_DIR_ORDER_TYPE_BRICKLINK;
  else if( ccStrCmpSeq( "brickowl", entry->filename, strlen( "brickowl" ) ) )
    entry->ordertype = BS_ORDER_DIR_ORDER_TYPE_BRICKOWL;
  entry->orderid = 0;
  stroffset = ccStrFindChar( entry->filename, '-' );
  if( stroffset > 0 )
  {
    str = &entry->filename[ stroffset + 1 ];
    stroffset = ccStrFindChar( str, '.' );
    if( ( stroffset > 0 ) && ( ccSeqParseInt32( str, stroffset, &readint ) ) )
      entry->orderid = (int)readint;
  }
  dir->entrycount++;
  return 1;
}


int bsReadOrderDir( bsContext *context, bsOrderDir *orderdir, time_t ordermintime )
{
  ccDir *dir;
  char *filename;
  char *filepath;
  time_t filetime;

  orderdir->entrylist = 0;
  orderdir->entrycount = 0;
  orderdir->entryalloc = 0;
  dir = ccOpenDir( BS_BRICKLINK_ORDER_DIR );
  if( !( dir ) )
    return 0;
  for( ; ; )
  {
    filename = ccReadDir( dir );
    if( !( filename ) )
      break;
    if( filename[0] == '.' )
      continue;
    filepath = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%s", BS_BRICKLINK_ORDER_DIR, filename );
    if( !( ccFileStat( filepath, 0, &filetime ) ) || ( filetime <= ordermintime ) )
    {
      free( filepath );
      continue;
    }
    if( !( bsOrderDirAddEntry( orderdir, filepath, filetime ) ) )
    {
      free( filepath );
      break;
    }
  }
  ccCloseDir( dir );

  return 1;
}


void bsFreeOrderDir( bsContext *context, bsOrderDir *orderdir )
{
  int entryindex;
  bsOrderDirEntry *entry;
  for( entryindex = 0 ; entryindex < orderdir->entrycount ; entryindex++ )
  {
    entry = &orderdir->entrylist[ entryindex ];
    free( entry->filepath );
  }
  free( orderdir->entrylist );
  orderdir->entrylist = 0;
  orderdir->entrycount = 0;
  orderdir->entryalloc = 0;
  return;
}


static int bsCmpOrderDirFileTime( bsOrderDirEntry *entry0, bsOrderDirEntry *entry1 )
{
  if( entry0->filetime != entry1->filetime )
    return ( entry0->filetime > entry1->filetime );
  else
    return ( strcmp( entry0->filepath, entry1->filepath ) > 0 ? 1 : 0 );
}

#define HSORT_MAIN bsxSortOrderDirFileTime
#define HSORT_CMP bsCmpOrderDirFileTime
#define HSORT_TYPE bsOrderDirEntry
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE

void bsSortOrderDir( bsContext *context, bsOrderDir *orderdir )
{
  bsOrderDirEntry *tmp;
  tmp = malloc( orderdir->entrycount * sizeof(bsOrderDirEntry) );
  bsxSortOrderDirFileTime( orderdir->entrylist, tmp, orderdir->entrycount, (uint32_t)(((uintptr_t)orderdir)>>8) );
  free( tmp );
  return;
}


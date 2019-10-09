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

#include "bricksync.h"


////


/* Flags from 0x10000 and up are reserved for internal use */
#define BS_RESOLVE_FLAGS_INTERNAL_PRINTSUCCESS (0x10000)
#define BS_RESOLVE_FLAGS_INTERNAL_PRINTFAILURE (0x20000)


static void bsBrickOwlReplyLookup( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  int64_t *retboid;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if 0
  ccFileStore( "zzz.txt", data, datasize, 0 );
#endif

  reply->result = resultcode;
  if( ( response ) && ( response->httpcode != 200 ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickOwl HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  retboid = (int64_t *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadLookup( retboid, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


static inline char bsResolveDecideItemType( bsxItem *item, char forceitemtype, int fallbacktypeflag )
{
  char itemtypeid;
  itemtypeid = item->typeid;
  if( forceitemtype )
    itemtypeid = forceitemtype;
  if( fallbacktypeflag )
  {
    if( ( itemtypeid == 'P' ) || ( itemtypeid == 'M' ) )
      itemtypeid = '#';
    else if( itemtypeid == 'S' )
      itemtypeid = 'G';
    else if( itemtypeid == 'G' )
      itemtypeid = 'S';
    else
      itemtypeid = 0;
  }
  return itemtypeid;
}


/* Queue a batch of queries to lookup BOID for lots of inventory, flag to skip items with known bolotid */
static int bsQueueBrickOwlLookupBoid( bsContext *context, bsWorkList *worklist, bsxInventory *inv, int resolveflags, char forceitemtype, int fallbacktypeflag )
{
  int itemindex;
  char itemtypeid;
  bsxItem *item;
  bsQueryReply *reply;
  char *querystring;
  char *itemtypestring;

  DEBUG_SET_TRACKER();

  /* Only queue so many queries over HTTP pipelining */
  for( itemindex = worklist->liststart ; itemindex < inv->itemcount ; itemindex++ )
  {
    if( context->brickowl.querycount >= context->brickowl.pipelinequeuesize )
      break;
    if( mmBitMapDirectGet( &worklist->bitmap, itemindex ) )
      continue;
    mmBitMapDirectSet( &worklist->bitmap, itemindex );
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->boid != -1 )
      continue;
    if( !( resolveflags & BS_RESOLVE_FLAGS_FORCEQUERY ) )
    {
      /* Check if that item was resolved by a previous query */
      item->boid = translationBLIDtoBOID( &context->translationtable, item->typeid, item->id );
      if( item->boid != -1 )
        continue;
    }
    if( !( item->id ) )
      continue;
    if( ( resolveflags & BS_RESOLVE_FLAGS_SKIPBOLOTID ) && ( item->bolotid != -1 ) )
      continue;
    itemtypeid = bsResolveDecideItemType( item, forceitemtype, fallbacktypeflag );
    if( !( itemtypeid ) )
      continue;
    switch( itemtypeid )
    {
      case 'P':
        itemtypestring = "Part";
        if( ( item->name ) && ( ccStrFindStr( item->name, "Sticker for " ) ) )
          itemtypestring = "Sticker";
        if( ( item->id ) && ( ccStrFindStr( item->id, "stk" ) || ccStrFindStr( item->id, "Stk" ) ) )
          itemtypestring = "Sticker";
        break;
      case 'S':
        itemtypestring = "Set";
        break;
      case 'M':
        itemtypestring = "Minifigure";
        break;
      case 'G':
        itemtypestring = "Gear";
        break;
      case 'I':
        itemtypestring = "Instructions";
        break;
      case 'O':
        itemtypestring = "Packaging";
        break;
      case '#':
        itemtypestring = "Minibuild";
        break;
      default:
        itemtypestring = 0;
        break;
    }
    /* There's some stuff we can't query, like books or custom lots, leave them a boid of -1 */
    if( itemtypestring )
    {
      querystring = ccStrAllocPrintf( "GET /v1/catalog/id_lookup?key=%s&id=%s&type=%s&id_type=bl_item_no HTTP/1.1\r\nHost: api.brickowl.com\r\nConnection: Keep-Alive\r\n\r\n", context->brickowl.key, item->id, itemtypestring );
      reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, itemindex, (void *)item, (void *)&item->boid );
      bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY | HTTP_QUERY_FLAGS_PIPELINING, (void *)reply, bsBrickOwlReplyLookup );
      free( querystring );
    }
  }
  worklist->liststart = itemindex;

  return ( context->brickowl.querycount ? 1 : 0 );
}


////


/* Query BrickOwl for all missing BOIDs of inventory, flag to skip items with known bolotid */
static int bsQueryBrickOwlLookupPass( bsContext *context, bsxInventory *inv, int resolveflags, int *lookupcounts, char forceitemtype, int fallbacktypeflag )
{
  int itemindex, misscount;
  char itemtypeid;
  int waitcount, itemlistindex;
  bsQueryReply *reply, *replynext;
  bsxItem *item;
  bsWorkList worklist;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

  misscount = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->boid == -1 )
      misscount++;
  }
  /* In order to avoid pipelining many identical requests (lookup 3001,3001,3001,etc.), sort by color */
  bsxSortInventory( inv, BSX_SORT_COLORID, 0 );

  /* Lookup all BLID->BOID as required for creation of new lots */
  bsTrackerInit( &tracker, context->brickowl.http );
  waitcount = context->brickowl.pipelinequeuesize;
  worklist.liststart = 0;
  mmBitMapInit( &worklist.bitmap, inv->itemcount, 0 );
  for( ; ; )
  {
    /* Queue BOID resolutions for the inventory */
    if( !( tracker.failureflag ) )
      bsQueueBrickOwlLookupBoid( context, &worklist, inv, resolveflags, forceitemtype, fallbacktypeflag );
    if( !( context->brickowl.querycount ) )
      break;
    if( context->brickowl.querycount <= waitcount )
      waitcount = context->brickowl.querycount - 1;

    /* Wait for replies */
    bsWaitBrickOwlQueries( context, waitcount );

    /* Examine all queued replies */
    for( reply = context->replylist.first ; reply ; reply = replynext )
    {
      replynext = reply->list.next;
      item = reply->extpointer;
      itemlistindex = reply->extid;
      bsTrackerAccumResult( context, &tracker, reply->result, BS_TRACKER_ACCUM_FLAGS_CANRETRY );
      if( reply->result != HTTP_RESULT_SUCCESS )
      {
        /* Clear bit to attempt item again */
        mmBitMapDirectClear( &worklist.bitmap, itemlistindex );
        if( itemlistindex < worklist.liststart )
          worklist.liststart = itemlistindex;
      }
      else
      {
        itemtypeid = bsResolveDecideItemType( item, forceitemtype, fallbacktypeflag );
        if( !( itemtypeid ) )
          itemtypeid = '?';
        /* Log our query result */
        if( item->boid != -1 )
        {
          /* Add the BLID<->BOID in the translation database */
          translationTableRegisterEntry( &context->translationtable, item->typeid, item->id, item->boid );
          lookupcounts[0] += item->quantity;
          lookupcounts[1]++;
          if( ( resolveflags & ( BS_RESOLVE_FLAGS_PRINTOUT | BS_RESOLVE_FLAGS_INTERNAL_PRINTSUCCESS ) ) == ( BS_RESOLVE_FLAGS_PRINTOUT | BS_RESOLVE_FLAGS_INTERNAL_PRINTSUCCESS ) )
            ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO "Successfull BOID lookup of " IO_CYAN CC_LLD IO_DEFAULT " for BLID \"" IO_GREEN "%s" IO_DEFAULT "\". Item name (" IO_GREEN "%c" IO_DEFAULT ") : \"" IO_GREEN "%s" IO_DEFAULT "\".\n", (long long)item->boid, item->id, itemtypeid, item->name );
          else
            ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Successfull BOID lookup of "CC_LLD" for BLID \"%s\", item type '%c'.\n", (long long)item->boid, item->id, itemtypeid );
        }
        else
        {
          lookupcounts[2] += item->quantity;
          lookupcounts[3]++;
          if( ( resolveflags & ( BS_RESOLVE_FLAGS_PRINTOUT | BS_RESOLVE_FLAGS_INTERNAL_PRINTFAILURE ) ) == ( BS_RESOLVE_FLAGS_PRINTOUT | BS_RESOLVE_FLAGS_INTERNAL_PRINTFAILURE ) )
            ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO "Failed BOID lookup for BLID \"" IO_YELLOW "%s" IO_DEFAULT "\", this BLID is unknown to BrickOwl. Item name (" IO_YELLOW "%c" IO_DEFAULT ") : \"" IO_YELLOW "%s" IO_DEFAULT "\".\n", item->id, itemtypeid, item->name );
          else
            ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Failed BOID lookup for \"%s\", item type '%c', this BLID is unknown to BrickOwl.\n", item->id, itemtypeid );
        }
      }
      bsFreeReply( context, reply );
    }
  }
  bsxSortInventory( inv, BSX_SORT_ID, 0 );
  mmBitMapFree( &worklist.bitmap );

  return ( tracker.failureflag ? 0 : 1 );
}


/* Query BrickOwl for all missing BOIDs of inventory, flag to skip items with known bolotid */
int bsQueryBrickOwlLookupBoids( bsContext *context, bsxInventory *inv, int resolveflags, int *lookupcounts )
{
  int staticlookupcounts[4], typeidindex;
  static const char typeidlist[] = { 'P', 'M', 'S', 'G', 'I', 'O', '#', 0 };

  if( !( lookupcounts ) )
    lookupcounts = staticlookupcounts;
  lookupcounts[0] = 0;
  lookupcounts[1] = 0;
  lookupcounts[2] = 0;
  lookupcounts[3] = 0;

  if( resolveflags & BS_RESOLVE_FLAGS_TRYALL )
  {
    for( typeidindex = 0 ; typeidlist[ typeidindex ] ; typeidindex++ )
    {
      lookupcounts[2] = 0;
      lookupcounts[3] = 0;
      if( !( bsQueryBrickOwlLookupPass( context, inv, resolveflags | BS_RESOLVE_FLAGS_INTERNAL_PRINTSUCCESS | BS_RESOLVE_FLAGS_INTERNAL_PRINTFAILURE, lookupcounts, typeidlist[ typeidindex ], 0 ) ) )
        return 0;
    }
  }
  else
  {
    if( !( bsQueryBrickOwlLookupPass( context, inv, resolveflags | BS_RESOLVE_FLAGS_INTERNAL_PRINTSUCCESS | BS_RESOLVE_FLAGS_INTERNAL_PRINTFAILURE, lookupcounts, 0, 0 ) ) )
      return 0;
    if( ( lookupcounts[3] ) && ( resolveflags & BS_RESOLVE_FLAGS_TRYFALLBACK ) )
    {
      if( resolveflags & BS_RESOLVE_FLAGS_PRINTOUT )
        ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_NODATE, BSMSG_INFO "Attempting BLID lookup with fallback item types for " IO_YELLOW "%d" IO_DEFAULT " still unresolved lots.\n", lookupcounts[3] );
      else
        ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Attempting BLID lookup with fallback item types for %d still unresolved lots.\n", lookupcounts[3] );
      lookupcounts[2] = 0;
      lookupcounts[3] = 0;
      if( !( bsQueryBrickOwlLookupPass( context, inv, resolveflags | BS_RESOLVE_FLAGS_INTERNAL_PRINTSUCCESS, lookupcounts, 0, 1 ) ) )
        return 0;
    }
  }

  return 1;
}


/* For the inventory, fill up all missing BOIDs from the translation database */
void bsResolveBrickOwlBoids( bsContext *context, bsxInventory *inv, int *lookupcounts )
{
  int itemindex;
  bsxItem *item;

  DEBUG_SET_TRACKER();

  if( lookupcounts )
  {
    lookupcounts[0] = 0;
    lookupcounts[1] = 0;
    lookupcounts[2] = 0;
    lookupcounts[3] = 0;
  }

  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->boid != -1 )
      continue;
    if( !( item->id ) )
      continue;
    item->boid = translationBLIDtoBOID( &context->translationtable, item->typeid, item->id );
    if( lookupcounts )
    {
      if( item->boid != -1 )
      {
        lookupcounts[0] += item->quantity;
        lookupcounts[1]++;
      }
      else if( lookupcounts )
      {
        lookupcounts[2] += item->quantity;
        lookupcounts[3]++;
      }
    }
  }

  return;
}


////



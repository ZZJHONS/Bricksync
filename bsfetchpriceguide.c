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


static const size_t blPriceGuideOffsetTable[2][6] =
{
  {
    offsetof(bsxPriceGuide,salecount), offsetof(bsxPriceGuide,saleqty),
    offsetof(bsxPriceGuide,saleminimum), offsetof(bsxPriceGuide,saleaverage),
    offsetof(bsxPriceGuide,saleqtyaverage), offsetof(bsxPriceGuide,salemaximum)
  },
  {
    offsetof(bsxPriceGuide,stockcount), offsetof(bsxPriceGuide,stockqty),
    offsetof(bsxPriceGuide,stockminimum), offsetof(bsxPriceGuide,stockaverage),
    offsetof(bsxPriceGuide,stockqtyaverage), offsetof(bsxPriceGuide,stockmaximum)
  }
};

static int blHtmlParsePriceGuide( bsxPriceGuide *pgnew, bsxPriceGuide *pgused, char *string, int stringlength )
{
  int section, sub, value;
  int64_t curtime;
  char *sectionstr[2], *substr[2];
  char *valstr;
  void *writeaddr;
  bsxPriceGuide *pg;
  bsxPriceGuide pgdummy;

  DEBUG_SET_TRACKER();

  if( !( pgnew ) )
    pgnew = &pgdummy;
  if( !( pgused ) )
    pgused = &pgdummy;

  memset( pgnew, 0, sizeof(bsxPriceGuide) );
  memset( pgused, 0, sizeof(bsxPriceGuide) );
  sectionstr[0] = ccSeqFindStrSkip( string, stringlength, "Past 6 Months Sales" );
  sectionstr[1] = ccSeqFindStrSkip( string, stringlength, "Current Items for Sale" );
  string[stringlength-1] = 0;
  if( sectionstr[0] )
    sectionstr[0][-1] = 0;
  if( sectionstr[1] )
    sectionstr[1][-1] = 0;

  for( section = 0 ; section < 2 ; section++ )
  {
    /* Get sale numbers out */
    if( !( sectionstr[section] ) )
      continue;
    substr[0] = ccStrFindStrSkip( sectionstr[section], "New" );
    substr[1] = ccStrFindStrSkip( sectionstr[section], "Used" );
    for( sub = 0 ; sub < 2 ; sub++ )
    {
      valstr = substr[sub];
      if( !( valstr ) )
        continue;
      pg = ( sub ? pgused : pgnew );
      for( value = 0 ; value < 2 ; value++ )
      {
        writeaddr = ADDRESS( pg, blPriceGuideOffsetTable[section][value] );
        valstr = ccStrFindStrSkip( valstr, "SIZE=\"2\">&nbsp;" );
        if( !( valstr ) )
          return 0;
        if( sscanf( valstr, "%d", (int *)writeaddr ) != 1 )
          return 0;
      }
      for( ; value < 6 ; value++ )
      {
        writeaddr = ADDRESS( pg, blPriceGuideOffsetTable[section][value] );
        valstr = ccStrFindStrSkip( valstr, "SIZE=\"2\">&nbsp;" );
        if( !( valstr ) )
          return 0;
        if( sscanf( valstr, "$%f", (float *)writeaddr ) != 1 )
          return 0;
      }
    }
  }

  curtime = time( 0 );
  pgnew->modtime = curtime;
  pgused->modtime = curtime;
  return 1;
}


////


typedef struct
{
  bsxInventory *inv;
  void *callbackpointer;
  void (*callback)( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer );
} bsFetchPriceGuideCallback;


static void bsBrickLinkReplyPriceGuide( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsxItem *item;
  bsxPriceGuide pgnew, pgused;
  bsFetchPriceGuideCallback *pgcallback;
  char *pgpath;

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
    bsStoreError( context, "BrickLink Web HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( blHtmlParsePriceGuide( &pgnew, &pgused, response->body, response->bodysize ) )
    {
      item = reply->extpointer;
      /* Save to price guide cache */
      pgpath = bsxPriceGuidePath( context->priceguidepath, item->typeid, item->id, item->colorid, context->priceguideflags | BSX_PRICEGUIDE_FLAGS_MKDIR );
      if( !( bsxWritePriceGuide( &pgnew, &pgused, pgpath, item->typeid, item->id, item->colorid ) ) )
      {
        ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to write price guide cache at \"" IO_MAGENTA "%s" IO_WHITE "\".\n", pgpath );
        reply->result = HTTP_RESULT_PROCESS_ERROR;
      }
      free( pgpath );
      /* Call callback if any */
      pgcallback = (bsFetchPriceGuideCallback *)reply->opaquepointer;
      if( ( pgcallback ) && ( pgcallback->callback ) )
        pgcallback->callback( context, pgcallback->inv, item, ( item->condition == 'N' ? &pgnew : &pgused ), pgcallback->callbackpointer );
    }
    else
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickLink Web PG Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Queue a batch of update queries for lots of the "diff" inventory */
static int bsQueueBrickLinkPriceGuide( bsContext *context, bsWorkList *worklist, bsxInventory *inv, bsFetchPriceGuideCallback *pgcallback )
{
  int itemindex;
  char *querystring;
  bsxItem *item;
  bsQueryReply *reply;

  DEBUG_SET_TRACKER();

  /* Only queue so many queries over HTTP pipelining */
  for( itemindex = worklist->liststart ; itemindex < inv->itemcount ; itemindex++ )
  {
    if( context->bricklink.webquerycount >= context->bricklink.pipelinequeuesize )
      break;
    if( mmBitMapDirectGet( &worklist->bitmap, itemindex ) )
      continue;
    mmBitMapDirectSet( &worklist->bitmap, itemindex );
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( !( item->flags & BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE ) )
      continue;
    reply = bsAllocReply( context, BS_QUERY_TYPE_WEBBRICKLINK, itemindex, (void *)item, (void *)pgcallback );
    querystring = ccStrAllocPrintf( "GET /priceGuide.asp?a=%c&viewType=N&colorID=%d&itemID=%s&viewDec=3 HTTP/1.1\r\nHost: www.bricklink.com\r\nConnection: Keep-Alive\r\n\r\n", item->typeid, item->colorid, item->id );
    httpAddQuery( context->bricklink.webhttp, querystring, strlen( querystring ), HTTP_QUERY_FLAGS_RETRY, (void *)reply, bsBrickLinkReplyPriceGuide );
    free( querystring );
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued price guide query for item \"%s\", color %d\n", ( item->id ? item->id : item->name ), item->colorid );
  }
  worklist->liststart = itemindex;

  return ( context->bricklink.webquerycount ? 1 : 0 );
}


/* Populate the price guide cache with all items flagged BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE */
int bsBrickLinkFetchPriceGuide( bsContext *context, bsxInventory *inv, void *callbackpointer, void (*callback)( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer ) )
{
  int waitcount, itemlistindex;
  bsxItem *item;
  bsQueryReply *reply, *replynext;
  bsWorkList worklist;
  bsTracker tracker;
  bsFetchPriceGuideCallback pgcallback;

  DEBUG_SET_TRACKER();

  /* Prepare optional callback handler */
  pgcallback.inv = inv;
  pgcallback.callbackpointer = callbackpointer;
  pgcallback.callback = callback;

  /* Keep pushing price guide fetches until we are done */
  bsTrackerInit( &tracker, context->bricklink.webhttp );
  waitcount = context->bricklink.pipelinequeuesize;
  worklist.liststart = 0;
  mmBitMapInit( &worklist.bitmap, inv->itemcount, 0 );
  for( ; ; )
  {
    /* Queue updates for the "diff" inventory */
    if( !( tracker.failureflag ) )
      bsQueueBrickLinkPriceGuide( context, &worklist, inv, &pgcallback );
    if( !( context->bricklink.webquerycount ) )
      break;
    if( context->bricklink.webquerycount <= waitcount )
      waitcount = context->bricklink.webquerycount - 1;

    /* Wait for replies */
    bsWaitBrickLinkWebQueries( context, waitcount );

    /* Examine all queued replies */
    for( reply = context->replylist.first ; reply ; reply = replynext )
    {
      replynext = reply->list.next;
      item = reply->extpointer;
      itemlistindex = reply->extid;
      if( reply->result != HTTP_RESULT_SUCCESS )
      {
        if( ( reply->result == HTTP_RESULT_CODE_ERROR ) || ( reply->result == HTTP_RESULT_PARSE_ERROR ) )
          ioPrintf( &context->output, 0, BSMSG_ERROR "Bad reply from server when fetching price guide for item \"" IO_MAGENTA "%s" IO_WHITE "\", color " IO_MAGENTA "%d" IO_WHITE ".\n", ( item->id ? item->id : item->name ), item->colorid );
        else if( reply->result != HTTP_RESULT_PROCESS_ERROR )
        {
          ioPrintf( &context->output, 0, BSMSG_WARNING "Price guide fetching failure, attempting again for item \"" IO_MAGENTA "%s" IO_WHITE "\", color " IO_MAGENTA "%d" IO_WHITE ".\n", ( item->id ? item->id : item->name ), item->colorid );
          /* Clear bit to attempt item again */
          mmBitMapDirectClear( &worklist.bitmap, itemlistindex );
          if( itemlistindex < worklist.liststart )
            worklist.liststart = itemlistindex;
        }
      }
      else
      {
        item->flags &= ~BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE;
        if( !( item->flags & BSX_ITEM_FLAGS_DELETED ) )
          ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Success fetching price guide for item \"%s\", color %d\n", ( item->id ? item->id : item->name ), item->colorid );
      }
      bsTrackerAccumResult( context, &tracker, reply->result, BS_TRACKER_ACCUM_FLAGS_CANRETRY );
      bsFreeReply( context, reply );
    }
  }

  mmBitMapFree( &worklist.bitmap );
  if( !( tracker.failureflag ) )
  {
    /* Flag inventory for minor updates, LotIDs and such */
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
    return 1;
  }
  return 0;
}




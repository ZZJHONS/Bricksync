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
#include "bsevalgrade.h"

#include "bricksync.h"


////


#ifdef ALLFUNCTIONALIGN16
 #define BS_FUNCTION_ALIGN16
#else
 #define BS_FUNCTION_ALIGN16 __attribute__((aligned(16)))
#endif


////


static void bsInvCountUpdates( bsxInventory *inv, int *retcreatecount, int *retupdatecount )
{
  int itemindex, createcount, updatecount;
  bsxItem *item;

  createcount = 0;
  updatecount = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->flags & BSX_ITEM_XFLAGS_TO_CREATE )
      createcount++;
    if( item->flags & BSX_ITEM_XFLAGS_TO_UPDATE )
      updatecount++;
  }
  *retcreatecount = createcount;
  *retupdatecount = updatecount;

  return;
}


////


static void bsBrickLinkReplyCreate( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  int64_t *retlotid;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if 0
  ccFileStore( "zzz.txt", response->body, response->bodysize, 0 );
#endif

  reply->result = resultcode;
  if( ( response ) && ( ( response->httpcode < 200 ) || ( response->httpcode > 299 ) ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickLink HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  retlotid = (int64_t *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( blReadLotID( retlotid, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickLink JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


static void bsBrickLinkReplyUpdate( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if 0
  ccFileStore( "zzz.txt", response->body, response->bodysize, 0 );
#endif

  reply->result = resultcode;
  if( ( response ) && ( ( response->httpcode < 200 ) || ( response->httpcode > 299 ) ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickLink HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( blReadGeneric( (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickLink JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


static void bsBrickLinkApplyDiffCreate( bsContext *context, bsxItem *item, int itemindex )
{
  bsQueryReply *reply;
  char *typestring;
  char *encodedstring;
  ccGrowth postgrowth;

  DEBUG_SET_TRACKER();

  if( !( item->id ) )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "Unknown item ID ( BOID " IO_RED CC_LLD IO_WHITE " ), ignored.\n", (long long)item->boid );
    return;
  }
  switch( item->typeid )
  {
    case 'P':
      typestring = "PART";
      break;
    case 'S':
      typestring = "SET";
      break;
    case 'M':
      typestring = "MINIFIG";
      break;
    case 'B':
      typestring = "BOOK";
      break;
    case 'G':
      typestring = "GEAR";
      break;
    case 'C':
      typestring = "CATALOG";
      break;
    case 'I':
      typestring = "INSTRUCTION";
      break;
    case 'O':
      typestring = "ORIGINAL_BOX";
      break;
    default:
      ioPrintf( &context->output, 0, BSMSG_WARNING "Unknown item type '" IO_RED "%c" IO_WHITE "' for \"" IO_CYAN "%s" IO_WHITE "\" ( " IO_CYAN "%s" IO_WHITE " ), ignored.\n", item->typeid, item->name, item->id );
      return;
  }
  ccGrowthInit( &postgrowth, 1024 );
  ccGrowthPrintf( &postgrowth, "[" );
  ccGrowthPrintf( &postgrowth, "{" );
  ccGrowthPrintf( &postgrowth, "\"item\":{" );
  ccGrowthPrintf( &postgrowth, "\"no\":\"%s\"", item->id );
  ccGrowthPrintf( &postgrowth, ",\"type\":\"%s\"", typestring );
  ccGrowthPrintf( &postgrowth, "}" );
  ccGrowthPrintf( &postgrowth, ",\"color_id\":%d", item->colorid );
  ccGrowthPrintf( &postgrowth, ",\"quantity\":%d", item->quantity );
  ccGrowthPrintf( &postgrowth, ",\"unit_price\":%.3f", item->price );
  ccGrowthPrintf( &postgrowth, ",\"new_or_used\":\"%c\"", item->condition );
  if( item->typeid == 'S' )
    ccGrowthPrintf( &postgrowth, ",\"completeness\":\"%s\"", ( item->condition == 'N' ? "S" : "C" ) );
  /* In JSON, we need to escape the chars '"' and '\' */
  if( item->comments )
  {
    encodedstring = 0;
    if( item->comments )
      encodedstring = jsonEncodeEscapeString( item->comments, strlen( item->comments ), 0 );
    ccGrowthPrintf( &postgrowth, ",\"description\":\"%s\"", ( encodedstring ? encodedstring : "" ) );
    free( encodedstring );
  }
  if( item->remarks )
  {
    encodedstring = 0;
    if( item->remarks )
      encodedstring = jsonEncodeEscapeString( item->remarks, strlen( item->remarks ), 0 );
    ccGrowthPrintf( &postgrowth, ",\"remarks\":\"%s\"", ( encodedstring ? encodedstring : "" ) );
    free( encodedstring );
  }
  ccGrowthPrintf( &postgrowth, ",\"bulk\":%d", ( item->bulk > 0 ? item->bulk : 1 ) );
  ccGrowthPrintf( &postgrowth, ",\"is_retain\":%s", ( context->retainemptylotsflag ? "true" : "false" ) );
  ccGrowthPrintf( &postgrowth, ",\"is_stock_room\":false" );
  if( item->mycost > 0.001 )
    ccGrowthPrintf( &postgrowth, ",\"my_cost\":%.3f", item->mycost );
  if( item->tq1 )
  {
    ccGrowthPrintf( &postgrowth, ",\"tier_quantity1\":%d", item->tq1 );
    ccGrowthPrintf( &postgrowth, ",\"tier_price1\":%.3f", item->tp1 );
    ccGrowthPrintf( &postgrowth, ",\"tier_quantity2\":%d", item->tq2 );
    ccGrowthPrintf( &postgrowth, ",\"tier_price2\":%.3f", item->tp2 );
    ccGrowthPrintf( &postgrowth, ",\"tier_quantity3\":%d", item->tq3 );
    ccGrowthPrintf( &postgrowth, ",\"tier_price3\":%.3f", item->tp3 );
  }
  ccGrowthPrintf( &postgrowth, "}" );
  ccGrowthPrintf( &postgrowth, "]" );

  reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKLINK, itemindex, (void *)item, (void *)&item->lotid );
  bsBrickLinkAddQuery( context, "POST", "/api/store/v1/inventories", 0, postgrowth.data, (void *)reply, bsBrickLinkReplyCreate );
  ccGrowthFree( &postgrowth );

#if 1
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued BrickLink Query: Create item \"%s\", color %d, quantity %d\n", ( item->id ? item->id : item->name ), item->colorid, item->quantity );
#endif

  return;
}

static void bsBrickLinkApplyDiffDelete( bsContext *context, bsxItem *item, int itemindex )
{
  bsQueryReply *reply;
  char *pathstring;

  DEBUG_SET_TRACKER();

  pathstring = ccStrAllocPrintf( "/api/store/v1/inventories/"CC_LLD"", (long long)item->lotid );
  reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKLINK, itemindex, (void *)item, 0 );
  /* Lot is gone forever */
  bsBrickLinkAddQuery( context, "DELETE", pathstring, 0, 0, (void *)reply, bsBrickLinkReplyUpdate );
  free( pathstring );

#if 1
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued BrickLink Query: Delete item \"%s\", LotID "CC_LLD"\n", ( item->id ? item->id : item->name ), (long long)item->lotid );
#endif

  return;
}

static void bsBrickLinkApplyDiffUpdate( bsContext *context, bsxItem *item, int itemindex )
{
  bsQueryReply *reply;
  char *pathstring;
  char *encodedstring;
  ccGrowth postgrowth;

  DEBUG_SET_TRACKER();

  pathstring = ccStrAllocPrintf( "/api/store/v1/inventories/"CC_LLD"", (long long)item->lotid );
  ccGrowthInit( &postgrowth, 512 );
  ccGrowthPrintf( &postgrowth, "{\"inventory_id\":"CC_LLD"", (long long)item->lotid );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
  {
    ccGrowthPrintf( &postgrowth, ",\"quantity\":%d", item->quantity );
    if( !( item->flags & BSX_ITEM_XFLAGS_UPDATE_STOCKROOM ) )
      ccGrowthPrintf( &postgrowth, ",\"is_stock_room\":false" );
  }
  /* In JSON, we need to escape the chars '"' and '\' | After Bricklink made changes on Feb 1st 2024 we need to always update the comment field in API calls even if it is the same as before. */ 
  // if( item->flags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
  // {
    encodedstring = 0;
    if( item->comments )
      encodedstring = jsonEncodeEscapeString( item->comments, strlen( item->comments ), 0 );
    ccGrowthPrintf( &postgrowth, ",\"description\":\"%s\"", ( encodedstring ? encodedstring : "" ) );
    free( encodedstring );
  // }
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
  {
    encodedstring = 0;
    if( item->remarks )
      encodedstring = jsonEncodeEscapeString( item->remarks, strlen( item->remarks ), 0 );
    ccGrowthPrintf( &postgrowth, ",\"remarks\":\"%s\"", ( encodedstring ? encodedstring : "" ) );
    free( encodedstring );
  }
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
    ccGrowthPrintf( &postgrowth, ",\"unit_price\":%.3f", item->price );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_BULK )
    ccGrowthPrintf( &postgrowth, ",\"bulk\":%d", ( item->bulk > 0 ? item->bulk : 1 ) );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_STOCKROOM )
  {
    ccGrowthPrintf( &postgrowth, ",\"is_retain\":true" );
    ccGrowthPrintf( &postgrowth, ",\"is_stock_room\":true" );
  }
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_MYCOST )
    ccGrowthPrintf( &postgrowth, ",\"my_cost\":%.3f", item->mycost );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES )
  {
    ccGrowthPrintf( &postgrowth, ",\"tier_quantity1\":%d", item->tq1 );
    ccGrowthPrintf( &postgrowth, ",\"tier_price1\":%.3f", item->tp1 );
    ccGrowthPrintf( &postgrowth, ",\"tier_quantity2\":%d", item->tq2 );
    ccGrowthPrintf( &postgrowth, ",\"tier_price2\":%.3f", item->tp2 );
    ccGrowthPrintf( &postgrowth, ",\"tier_quantity3\":%d", item->tq3 );
    ccGrowthPrintf( &postgrowth, ",\"tier_price3\":%.3f", item->tp3 );
  }
  ccGrowthPrintf( &postgrowth, "}" );
  reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKLINK, itemindex, (void *)item, 0 );
  bsBrickLinkAddQuery( context, "PUT", pathstring, 0, postgrowth.data, (void *)reply, bsBrickLinkReplyUpdate );
  free( pathstring );
  ccGrowthFree( &postgrowth );

#if 1
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued BrickLink Query: Update item \"%s\", color %d", ( item->id ? item->id : item->name ), item->colorid );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Quantity adjust by %+d", item->quantity );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set comments to \"%s\"", ( item->comments ? item->comments : "" ) );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set remarks to \"%s\"", ( item->remarks ? item->remarks : "" ) );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set price to %.3f", item->price );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_BULK )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set bulk to %d", item->bulk );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_MYCOST )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set mycost to %.3f", item->mycost );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set tierprices to [[%d,%.3f],[%d,%.3f],[%d,%.3f]]", item->tq1, item->tp1, item->tq2, item->tp2, item->tq3, item->tp3 );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_STOCKROOM )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Move to Stockroom" );
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "\n" );
#endif

  return;
}


/* Queue a batch of update queries for lots of the "diff" inventory */
static int bsQueueBrickLinkApplyDiff( bsContext *context, bsWorkList *worklist, bsxInventory *diffinv )
{
  int itemindex, apilimit;
  bsxItem *item;

  DEBUG_SET_TRACKER();

  /* Only queue so many queries over HTTP pipelining */
  for( itemindex = worklist->liststart ; itemindex < diffinv->itemcount ; itemindex++ )
  {
    if( context->bricklink.querycount >= context->bricklink.pipelinequeuesize )
      break;
    if( mmBitMapDirectGet( &worklist->bitmap, itemindex ) )
      continue;
    mmBitMapDirectSet( &worklist->bitmap, itemindex );
    item = &diffinv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;

    /* Verify if we aren't exceeding API count thresholds */
    apilimit = context->bricklink.apicountsyncresume;
    if( item->flags & ( BSX_ITEM_XFLAGS_TO_CREATE | BSX_ITEM_XFLAGS_TO_DELETE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY ) )
      apilimit = context->bricklink.apicountquantitylimit;
    else if( item->flags & ( BSX_ITEM_XFLAGS_UPDATE_COMMENTS | BSX_ITEM_XFLAGS_UPDATE_REMARKS ) )
      apilimit = context->bricklink.apicountnoteslimit;
    else if( item->flags & ( BSX_ITEM_XFLAGS_UPDATE_PRICE ) )
      apilimit = context->bricklink.apicountpricelimit;
    if( context->bricklink.apihistory.total >= apilimit )
    {
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: API pool too low, skipping BrickLink item \"%s\", color %d.\n", ( item->id ? item->id : item->name ), item->colorid );
      continue;
    }

    if( item->flags & BSX_ITEM_XFLAGS_TO_CREATE )
      bsBrickLinkApplyDiffCreate( context, item, itemindex );
    else if( item->flags & BSX_ITEM_XFLAGS_TO_DELETE )
      bsBrickLinkApplyDiffDelete( context, item, itemindex );
    else if( item->flags & BSX_ITEM_XFLAGS_TO_UPDATE )
      bsBrickLinkApplyDiffUpdate( context, item, itemindex );
    else
      bsxRemoveItem( diffinv, item );
  }
  worklist->liststart = itemindex;

  return ( context->bricklink.querycount ? 1 : 0 );
}


/* Query BrickLink, apply updates for whole diff inventory, diffinv is modified */
int BS_FUNCTION_ALIGN16 bsQueryBrickLinkApplyDiff( bsContext *context, bsxInventory *diffinv, int *retryflag )
{
  int waitcount, itemlistindex, accumflags;
  int32_t itemflags, itemdiscardflags;
  char *actionstring;
  bsQueryReply *reply, *replynext;
  bsxItem *item, *stockitem;
  bsWorkList worklist;
  bsTracker tracker;
  time_t lastprogresstime, currenttime;

  DEBUG_SET_TRACKER();

  if( !( diffinv->itemcount ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "No update required for BrickLink, we have a " IO_GREEN "perfect inventory match" IO_DEFAULT ".\n" );
    return 1;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Updating BrickLink inventory, " IO_GREEN "%d" IO_DEFAULT " lots are pending for update.\n", diffinv->itemcount );
  lastprogresstime = time( 0 );

  bsxSortInventory( diffinv, BSX_SORT_UPDATE_PRIORITY, 0 );

  /* Keep pushing BrickLink inventory updates until we are done */
  bsTrackerInit( &tracker, context->bricklink.http );
  waitcount = context->bricklink.pipelinequeuesize;
  worklist.liststart = 0;
  mmBitMapInit( &worklist.bitmap, diffinv->itemcount, 0 );
  for( ; ; )
  {
    /* Queue updates for the "diff" inventory */
    if( !( tracker.failureflag ) )
      bsQueueBrickLinkApplyDiff( context, &worklist, diffinv );
    if( !( context->bricklink.querycount ) )
      break;
    if( context->bricklink.querycount <= waitcount )
      waitcount = context->bricklink.querycount - 1;

    /* Wait for replies */
    bsWaitBrickLinkQueries( context, waitcount );

    /* Examine all queued replies */
    for( reply = context->replylist.first ; reply ; reply = replynext )
    {
      replynext = reply->list.next;
      item = reply->extpointer;
      itemlistindex = reply->extid;
      actionstring = "updating";
      if( item->flags & BSX_ITEM_XFLAGS_TO_CREATE )
        actionstring = "creating";
      else if( item->flags & BSX_ITEM_XFLAGS_TO_DELETE )
        actionstring = "deleting";
      accumflags = BS_TRACKER_ACCUM_FLAGS_CANSYNC;
      if( reply->result != HTTP_RESULT_SUCCESS )
      {
        ioPrintf( &context->output, 0, BSMSG_WARNING "Error %s BrickLink item \"" IO_MAGENTA "%s" IO_WHITE "\", color " IO_MAGENTA "%d" IO_WHITE ".\n", actionstring, ( item->id ? item->id : item->name ), item->colorid );
        if( item->flags & BSX_ITEM_XFLAGS_TO_CREATE )
          ioPrintf( &context->output, 0, BSMSG_WARNING "If the BLID for the item has changed, consider using the command " IO_CYAN "setblid" IO_WHITE " to fix this lot.\n" );
        if( ( ( reply->result == HTTP_RESULT_CODE_ERROR ) || ( reply->result == HTTP_RESULT_PARSE_ERROR ) ) )
        {
          if( item->flags & BSX_ITEM_XFLAGS_TO_UPDATE )
          {
            itemdiscardflags = item->flags;
            if( item->flags & ( BSX_ITEM_XFLAGS_UPDATE_BULK | BSX_ITEM_XFLAGS_UPDATE_MYCOST | BSX_ITEM_XFLAGS_UPDATE_USEDGRADE | BSX_ITEM_XFLAGS_UPDATE_TIERPRICES ) )
            {
              /* Discard some low priority updates */
              item->flags &= ~( BSX_ITEM_XFLAGS_UPDATE_BULK | BSX_ITEM_XFLAGS_UPDATE_MYCOST | BSX_ITEM_XFLAGS_UPDATE_USEDGRADE | BSX_ITEM_XFLAGS_UPDATE_TIERPRICES );
            }
            else if( item->flags & ( BSX_ITEM_XFLAGS_UPDATE_PRICE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS | BSX_ITEM_XFLAGS_UPDATE_REMARKS ) )
            {
              /* Discard some mid priority updates */
              item->flags &= ~( BSX_ITEM_XFLAGS_UPDATE_PRICE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS | BSX_ITEM_XFLAGS_UPDATE_REMARKS );
            }
            itemdiscardflags ^= item->flags;
            if( itemdiscardflags )
            {
              accumflags |= BS_TRACKER_ACCUM_FLAGS_NO_ERROR;
              ioPrintf( &context->output, 0, BSMSG_WARNING "An update was rejected by BrickLink. A further " IO_CYAN "sync" IO_WHITE " will attempt the update again.\n" );
              ioPrintf( &context->output, 0, BSMSG_WARNING "We are dropping the following updates:" IO_YELLOW );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_BULK )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Bulk" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_MYCOST )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " MyCost" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_USEDGRADE )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " UsedGrade" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " TierPrices" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Price" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Comments" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Remarks" );
              ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" );
            }
            if( item->flags & BSX_ITEM_XFLAGS_UPDATEMASK )
              goto tryagain;
          }
          ioPrintf( &context->output, 0, BSMSG_WARNING "The operation was rejected by BrickLink. A further " IO_CYAN "sync" IO_WHITE " will attempt the operation again.\n" );
          bsxRemoveItem( diffinv, item );
          reply->result = HTTP_RESULT_SUCCESS;
        }
        else
        {
          /* Clear bit to attempt item again */
          ioPrintf( &context->output, 0, BSMSG_WARNING "The operation will be attempted again.\n" );
          tryagain:
          mmBitMapDirectClear( &worklist.bitmap, itemlistindex );
          if( itemlistindex < worklist.liststart )
            worklist.liststart = itemlistindex;
        }
      }
      else
      {
        itemflags = item->flags;
        item->flags &= ~( BSX_ITEM_XFLAGS_TO_CREATE | BSX_ITEM_XFLAGS_TO_DELETE | BSX_ITEM_XFLAGS_TO_UPDATE );
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Success %s BrickLink item \"%s\", color %d\n", actionstring, ( item->id ? item->id : item->name ), item->colorid );
        /* Update tracked inventory LotID */
        if( ( itemflags & BSX_ITEM_XFLAGS_TO_CREATE ) && ( item->lotid != -1 ) )
        {
          stockitem = 0;
          if( item->extid != -1 )
            stockitem = bsxFindExtID( context->inventory, item->extid );
#if 0
          if( !( stockitem ) )
            stockitem = bsxFindMatchItem( context->inventory, item );
#else
          if( !( stockitem ) )
            ioPrintf( &context->output, 0, IO_RED "Unexpected situation at %s:%d. Please notify code maintainer.\n", __FILE__, __LINE__ );
#endif
          if( stockitem )
            stockitem->lotid = item->lotid;
        }
        /* Item succesfully updated, mark it out of the 'diff' inventory */
        bsxRemoveItem( diffinv, item );
      }
      bsTrackerAccumResult( context, &tracker, reply->result, accumflags );
      bsFreeReply( context, reply );
    }
    /* Print progress report */
    currenttime = time( 0 );
    if( ( currenttime - lastprogresstime ) >= BS_APPLYDELTA_PROGRESS_PRINT_INTERVAL )
    {
      int createcount, updatecount;
      bsInvCountUpdates( diffinv, &createcount, &updatecount );
      if( ( createcount | updatecount ) )
        ioPrintf( &context->output, 0, BSMSG_INFO "Updating BrickLink inventory, " IO_GREEN "%d" IO_DEFAULT " lots to create, " IO_GREEN "%d" IO_DEFAULT " lots to update.\n", createcount, updatecount );
      lastprogresstime = currenttime;
    }
  }
  mmBitMapFree( &worklist.bitmap );

  if( tracker.failureflag )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "BrickLink update did not complete successfully!\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink flagged " IO_MAGENTA "MUST_SYNC" IO_DEFAULT " for deep synchronization.\n" );
    return 0;
  }

  /* Were all queries successful or we need to check for any error? */
  if( tracker.mustsyncflag )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink service flagged for deep sync, we never received replies for some queries.\n" );
    context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_SYNC;
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
  }

  /* Flag inventory for minor updates, LotIDs and such */
  context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink inventory update has completed.\n" );

  return 1;
}


////


static void bsBrickOwlReplyCreate( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  int64_t *retlotid;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if 0
  ccFileStore( "zzz.txt", response->body, response->bodysize, 0 );
#endif

  reply->result = resultcode;
  if( ( response ) && ( ( response->httpcode < 200 ) || ( response->httpcode > 299 ) ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickOwl HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  retlotid = (int64_t *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadLotID( retlotid, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


static void bsBrickOwlReplyUpdate( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if 0
  ccFileStore( "zzz.txt", response->body, response->bodysize, 0 );
#endif

  reply->result = resultcode;
  if( ( response ) && ( ( response->httpcode < 200 ) || ( response->httpcode > 299 ) ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickOwl HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  return;
}


static void bsBrickOwlApplyDiffCreate( bsContext *context, bsxItem *item, int itemindex )
{
  int bocolor, evalcondition;
  bsQueryReply *reply;
  char *querystring;
  char *conditionstring;
  ccGrowth postgrowth;

  DEBUG_SET_TRACKER();

  bocolor = bsTranslateColorBl2Bo( item->colorid );
  if( bocolor == -1 )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "We failed to translate the BL color %d to a BO color code.\n", item->colorid );
    ioPrintf( &context->output, 0, BSMSG_WARNING "The lot \"%s\" ( %s ) in color \"%s\" will not be uploaded to BrickOwl.\n", item->name, item->id, item->colorname );
    return;
  }

  ccGrowthInit( &postgrowth, 512 );
#if 1
  ccGrowthPrintf( &postgrowth, "key=%s&boid="CC_LLD"", context->brickowl.key, (long long)item->boid );
  if( ( item->typeid == 'P' ) || ( ( bocolor ) && ( item->typeid == 'G' ) ) )
    ccGrowthPrintf( &postgrowth, "&color_id=%d", bocolor );
#else
  ccGrowthPrintf( &postgrowth, "key=%s&boid="CC_LLD"", context->brickowl.key, (long long)item->boid );
  if( ( item->typeid == 'P' ) || ( ( bocolor ) && ( item->typeid == 'G' ) ) )
    ccGrowthPrintf( &postgrowth, "-%d", bocolor );
#endif
  ccGrowthPrintf( &postgrowth, "&external_id="CC_LLD"&quantity=%d&price=%.3f", (long long)item->lotid, item->quantity, item->price );
  if( item->typeid == 'O' )
    conditionstring = ( item->condition == 'U' ? "usedg" : "usedn" );
  else if( item->typeid == 'S' )
  {
    if( item->completeness == 'C' )
      conditionstring = ( item->condition == 'U' ? "usedc" : "newc" );
    else if( item->completeness == 'B' )
      conditionstring = ( item->condition == 'U' ? "usedi" : "newi" );
    else
      conditionstring = ( item->condition == 'U' ? "usedc" : "news" );
  }
  else
  {
    if( item->condition != 'U' )
      conditionstring = "new";
    else
    {
      if( item->usedgrade )
      {
        if( item->usedgrade == 'N' )
          conditionstring = "usedn";
        else if( item->usedgrade == 'G' )
          conditionstring = "usedg";
        else if( item->usedgrade == 'A' )
          conditionstring = "useda";
      }
      else
      {
        evalcondition = bsEvalulatePartCondition( item->comments );
        if( evalcondition == BS_EVAL_CONDITION_LIKE_NEW )
          conditionstring = "usedn";
        else if( evalcondition == BS_EVAL_CONDITION_GOOD )
          conditionstring = "usedg";
        else
          conditionstring = "useda";
      }
    }
  }
  ccGrowthPrintf( &postgrowth, "&condition=%s", conditionstring );
  querystring = ccStrAllocPrintf( "POST /v1/inventory/create HTTP/1.1\r\nHost: api.brickowl.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\n\r\n%s", (int)postgrowth.offset, postgrowth.data );
  reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, itemindex, (void *)item, (void *)&item->bolotid );
  bsBrickOwlAddQuery( context, querystring, 0, (void *)reply, bsBrickOwlReplyCreate );
  free( querystring );
  ccGrowthFree( &postgrowth );

#if 1
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued BrickOwl Query: Create item \"%s\", BOID "CC_LLD", color %d, quantity %d\n", ( item->id ? item->id : item->name ), (long long)item->boid, item->colorid, item->quantity );
#endif

  return;
}


static void bsBrickOwlApplyDiffDelete( bsContext *context, bsxItem *item, int itemindex )
{
  bsQueryReply *reply;
  char *querystring;
  char *poststring;

  DEBUG_SET_TRACKER();

  if( item->bolotid != -1 )
    poststring = ccStrAllocPrintf( "key=%s&lot_id="CC_LLD"&absolute_quantity=0", context->brickowl.key, (long long)item->bolotid );
  else
    poststring = ccStrAllocPrintf( "key=%s&external_id="CC_LLD"&absolute_quantity=0", context->brickowl.key, (long long)item->lotid );
  querystring = ccStrAllocPrintf( "POST /v1/inventory/update HTTP/1.1\r\nHost: api.brickowl.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\n\r\n%s", (int)strlen(poststring), poststring );
  reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, itemindex, (void *)item, 0 );
  bsBrickOwlAddQuery( context, querystring, 0, (void *)reply, bsBrickOwlReplyUpdate );
  free( querystring );
  free( poststring );

#if 1
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued BrickOwl Query: Delete item \"%s\", BOID "CC_LLD", LotID "CC_LLD", OwlLotID "CC_LLD"\n", ( item->id ? item->id : item->name ), (long long)item->boid, (long long)item->lotid, (long long)item->bolotid );
#endif

  return;
}

static void bsBrickOwlApplyDiffUpdate( bsContext *context, bsxItem *item, int itemindex )
{
  bsQueryReply *reply;
  char *querystring;
  char *encodedstring;
  char *conditionstring;
  ccGrowth postgrowth;

  DEBUG_SET_TRACKER();

  ccGrowthInit( &postgrowth, 512 );
  ccGrowthPrintf( &postgrowth, "key=%s", context->brickowl.key );
  if( item->bolotid != -1 )
    ccGrowthPrintf( &postgrowth, "&lot_id="CC_LLD"", (long long)item->bolotid );
  else
    ccGrowthPrintf( &postgrowth, "&external_id="CC_LLD"", (long long)item->lotid );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
    ccGrowthPrintf( &postgrowth, "&relative_quantity=%d", item->quantity );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
  {
    encodedstring = 0;
    if( item->comments )
      encodedstring = oauthPercentEncode( item->comments, strlen( item->comments ), 0 );
    ccGrowthPrintf( &postgrowth, "&public_note=%s", ( encodedstring ? encodedstring : "" ) );
    free( encodedstring );
  }
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
  {
    encodedstring = 0;
    if( item->remarks )
      encodedstring = oauthPercentEncode( item->remarks, strlen( item->remarks ), 0 );
    ccGrowthPrintf( &postgrowth, "&personal_note=%s", ( encodedstring ? encodedstring : "" ) );
    free( encodedstring );
  }
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
    ccGrowthPrintf( &postgrowth, "&price=%.3f", item->price );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_BULK )
    ccGrowthPrintf( &postgrowth, "&bulk_qty=%d", ( item->bulk > 0 ? item->bulk : 1 ) );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_MYCOST )
    ccGrowthPrintf( &postgrowth, "&my_cost=%.3f", item->mycost );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_USEDGRADE )
  {
    conditionstring = 0;
    if( item->usedgrade == 'N' )
      conditionstring = "usedn";
    else if( item->usedgrade == 'G' )
      conditionstring = "usedg";
    else if( item->usedgrade == 'A' )
      conditionstring = "useda";
    if( conditionstring )
      ccGrowthPrintf( &postgrowth, "&condition=%s", conditionstring );
  }
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES )
  {
    if( item->tq1 )
    {
      ccGrowthPrintf( &postgrowth, "&tier_price=%d:%.3f", item->tq1, item->tp1 );
      if( item->tq2 )
      {
        ccGrowthPrintf( &postgrowth, ",%d:%.3f", item->tq2, item->tp2 );
        if( item->tq3 )
          ccGrowthPrintf( &postgrowth, ",%d:%.3f", item->tq3, item->tp3 );
      }
    }
    else
      ccGrowthPrintf( &postgrowth, "&tier_price=remove" );
  }

  querystring = ccStrAllocPrintf( "POST /v1/inventory/update HTTP/1.1\r\nHost: api.brickowl.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\n\r\n%s", (int)postgrowth.offset, postgrowth.data );
  reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, itemindex, (void *)item, 0 );
  bsBrickOwlAddQuery( context, querystring, 0, (void *)reply, bsBrickOwlReplyUpdate );
  free( querystring );
  ccGrowthFree( &postgrowth );

#if 1
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Queued BrickOwl Query: Update item \"%s\", color %d", ( item->id ? item->id : item->name ), item->colorid );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Quantity adjust by %+d", item->quantity );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set comments to \"%s\"", ( item->comments ? item->comments : "" ) );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set remarks to \"%s\"", ( item->remarks ? item->remarks : "" ) );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set price to %.3f", item->price );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_BULK )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set bulk to %d", item->bulk );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_MYCOST )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set mycost to %.3f", item->mycost );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_USEDGRADE )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set usedgrade to %c", item->usedgrade );
  if( item->flags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "; Set tierprices to [[%d,%.3f],[%d,%.3f],[%d,%.3f]]", item->tq1, item->tp1, item->tq2, item->tp2, item->tq3, item->tp3 );
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "\n" );
#endif

  return;
}


/* Queue a batch of update queries for lots of the "diff" inventory */
static int bsQueueBrickOwlApplyDiff( bsContext *context, bsWorkList *worklist, bsxInventory *diffinv )
{
  int itemindex;
  bsxItem *item;

  DEBUG_SET_TRACKER();

  /* Only queue so many queries over HTTP pipelining */
  for( itemindex = worklist->liststart ; itemindex < diffinv->itemcount ; itemindex++ )
  {
    if( context->brickowl.querycount >= context->brickowl.pipelinequeuesize )
      break;
    if( mmBitMapDirectGet( &worklist->bitmap, itemindex ) )
      continue;
    mmBitMapDirectSet( &worklist->bitmap, itemindex );
    item = &diffinv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->flags & BSX_ITEM_XFLAGS_TO_CREATE )
      bsBrickOwlApplyDiffCreate( context, item, itemindex );
    else if( item->flags & BSX_ITEM_XFLAGS_TO_DELETE )
      bsBrickOwlApplyDiffDelete( context, item, itemindex );
    else if( item->flags & BSX_ITEM_XFLAGS_TO_UPDATE )
      bsBrickOwlApplyDiffUpdate( context, item, itemindex );
    else
      bsxRemoveItem( diffinv, item );
  }
  worklist->liststart = itemindex;

  return ( context->brickowl.querycount ? 1 : 0 );
}


static int bsQueryBrickOwlApplyDiffPass( bsContext *context, bsxInventory *diffinv, int *retryflag )
{
  int waitcount, itemlistindex, accumflags;
  int32_t itemflags, updateflags, itemdiscardflags;
  char *actionstring;
  bsQueryReply *reply, *replynext;
  bsxItem *item, *stockitem;
  bsWorkList worklist;
  bsTracker tracker;
  time_t lastprogresstime, currenttime;

  lastprogresstime = time( 0 );

  DEBUG_SET_TRACKER();

  /* Keep pushing BrickOwl inventory updates until we are done */
  bsTrackerInit( &tracker, context->brickowl.http );
  waitcount = context->brickowl.pipelinequeuesize;
  worklist.liststart = 0;
  mmBitMapInit( &worklist.bitmap, diffinv->itemcount, 0 );
  for( ; ; )
  {
    /* Queue updates for the "diff" inventory */
    if( !( tracker.failureflag ) )
      bsQueueBrickOwlApplyDiff( context, &worklist, diffinv );
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
      actionstring = "updating";
      if( item->flags & BSX_ITEM_XFLAGS_TO_CREATE )
        actionstring = "creating";
      else if( item->flags & BSX_ITEM_XFLAGS_TO_DELETE )
        actionstring = "deleting";
      accumflags = BS_TRACKER_ACCUM_FLAGS_CANSYNC;
      if( reply->result != HTTP_RESULT_SUCCESS )
      {
        ioPrintf( &context->output, 0, BSMSG_WARNING "Error %s BrickOwl item \"" IO_MAGENTA "%s" IO_WHITE "\", color " IO_MAGENTA "%d" IO_WHITE ".\n", actionstring, ( item->id ? item->id : item->name ), item->colorid );
        if( ( item->flags & BSX_ITEM_XFLAGS_TO_CREATE ) && ( item->id ) )
          ioPrintf( &context->output, 0, BSMSG_WARNING "It's possible we may have cached an old BOID for this BLID. Try updating the BLID<->BOID translation cache by typing " IO_CYAN "owlqueryblid -f %s" IO_WHITE ".\n", item->id );
        if( ( ( reply->result == HTTP_RESULT_CODE_ERROR ) || ( reply->result == HTTP_RESULT_PARSE_ERROR ) ) )
        {
          if( item->flags & BSX_ITEM_XFLAGS_TO_UPDATE )
          {
            itemdiscardflags = item->flags;
            if( item->flags & ( BSX_ITEM_XFLAGS_UPDATE_BULK | BSX_ITEM_XFLAGS_UPDATE_MYCOST | BSX_ITEM_XFLAGS_UPDATE_USEDGRADE | BSX_ITEM_XFLAGS_UPDATE_TIERPRICES ) )
            {
              /* Discard some low priority updates */
              item->flags &= ~( BSX_ITEM_XFLAGS_UPDATE_BULK | BSX_ITEM_XFLAGS_UPDATE_MYCOST | BSX_ITEM_XFLAGS_UPDATE_USEDGRADE | BSX_ITEM_XFLAGS_UPDATE_TIERPRICES );
            }
            else if( item->flags & ( BSX_ITEM_XFLAGS_UPDATE_PRICE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS | BSX_ITEM_XFLAGS_UPDATE_REMARKS ) )
            {
              /* Discard some mid priority updates */
              item->flags &= ~( BSX_ITEM_XFLAGS_UPDATE_PRICE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS | BSX_ITEM_XFLAGS_UPDATE_REMARKS );
            }
            itemdiscardflags ^= item->flags;
            if( itemdiscardflags )
            {
              accumflags |= BS_TRACKER_ACCUM_FLAGS_NO_ERROR;
              ioPrintf( &context->output, 0, BSMSG_WARNING "An update was rejected by BrickOwl. A further " IO_CYAN "sync" IO_WHITE " will attempt the update again.\n" );
              ioPrintf( &context->output, 0, BSMSG_WARNING "We are dropping the following updates:" IO_YELLOW );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_BULK )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Bulk" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_MYCOST )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " MyCost" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_USEDGRADE )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " UsedGrade" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " TierPrices" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Price" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Comments" );
              if( itemdiscardflags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
                ioPrintf( &context->output, IO_MODEBIT_NODATE, " Remarks" );
              ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" );
            }
            if( item->flags & BSX_ITEM_XFLAGS_UPDATEMASK )
              goto tryagain;
          }
          ioPrintf( &context->output, 0, BSMSG_WARNING "The operation was rejected by BrickOwl. A further " IO_CYAN "sync" IO_WHITE " will attempt the operation again.\n" );
          bsxRemoveItem( diffinv, item );
          reply->result = HTTP_RESULT_SUCCESS;
        }
        else
        {
          /* Clear bit to attempt item again */
          ioPrintf( &context->output, 0, BSMSG_WARNING "The operation will be attempted again.\n" );
          tryagain:
          mmBitMapDirectClear( &worklist.bitmap, itemlistindex );
          if( itemlistindex < worklist.liststart )
            worklist.liststart = itemlistindex;
        }
      }
      else
      {
        itemflags = item->flags;
        item->flags &= ~( BSX_ITEM_XFLAGS_TO_CREATE | BSX_ITEM_XFLAGS_TO_DELETE | BSX_ITEM_XFLAGS_TO_UPDATE );
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Success %s BrickOwl item \"%s\", color %d\n", actionstring, ( item->id ? item->id : item->name ), item->colorid );
        if( itemflags & BSX_ITEM_XFLAGS_TO_CREATE )
        {
          /* Update tracked inventory BoLotID */
          if( item->bolotid != -1 )
          {
            stockitem = 0;
            if( item->extid != -1 )
              stockitem = bsxFindExtID( context->inventory, item->extid );
#if 0
            if( !( stockitem ) )
              stockitem = bsxFindMatchItem( context->inventory, item );
#else
            if( !( stockitem ) )
              ioPrintf( &context->output, 0, IO_RED "Unexpected situation at %s:%d. Please notify code maintainer.\n", __FILE__, __LINE__ );
#endif
            if( stockitem )
              stockitem->bolotid = item->bolotid;
          }
          /* BrickOwl's /create can not set a bunch of fields, we need a second "update" pass for created lots */
          updateflags = 0;
          if( item->comments )
            updateflags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS;
          if( item->remarks )
            updateflags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_REMARKS;
          if( item->bulk )
            updateflags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_BULK;
          if( item->mycost > 0.0001 )
            updateflags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_MYCOST;
          item->flags |= updateflags;
        }
        else
        {
          /* Item succesfully updated, mark it out of the 'diff' inventory */
          bsxRemoveItem( diffinv, item );
        }
      }
      bsTrackerAccumResult( context, &tracker, reply->result, accumflags );
      bsFreeReply( context, reply );
    }
    /* Print progress report */
    currenttime = time( 0 );
    if( ( currenttime - lastprogresstime ) >= BS_APPLYDELTA_PROGRESS_PRINT_INTERVAL )
    {
      int createcount, updatecount;
      bsInvCountUpdates( diffinv, &createcount, &updatecount );
      if( ( createcount | updatecount ) )
        ioPrintf( &context->output, 0, BSMSG_INFO "Updating BrickOwl inventory, " IO_GREEN "%d" IO_DEFAULT " lots to create, " IO_GREEN "%d" IO_DEFAULT " lots to update.\n", createcount, updatecount );
      lastprogresstime = currenttime;
    }
  }

  mmBitMapFree( &worklist.bitmap );
  if( tracker.failureflag )
    return 0;

  /* Were all queries successful or we need to check for any error? */
  if( tracker.mustsyncflag )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl service flagged for deep sync, we never received replies for some queries.\n" );
    context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
  }

  /* Flag inventory for minor updates, LotIDs and such */
  context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
  return 1;
}


/* Query BrickOwl, apply updates for whole diff inventory, diffinv is modified */
int BS_FUNCTION_ALIGN16 bsQueryBrickOwlApplyDiff( bsContext *context, bsxInventory *diffinv, int *retryflag )
{
  DEBUG_SET_TRACKER();

  if( !( diffinv->itemcount ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "No update required for BrickOwl, we have a " IO_GREEN "perfect inventory match" IO_DEFAULT ".\n" );
    return 1;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Updating BrickOwl inventory, " IO_GREEN "%d" IO_DEFAULT " lots are pending for update.\n", diffinv->itemcount );

  if( !( bsQueryBrickOwlApplyDiffPass( context, diffinv, retryflag ) ) )
    goto error;
  /* We have to perform two passes, so that created lots can then be updated with the BoLotID in hand */
  if( !( bsQueryBrickOwlApplyDiffPass( context, diffinv, retryflag ) ) )
    goto error;
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl inventory update has completed.\n" );
  return 1;

  error:
  ioPrintf( &context->output, 0, BSMSG_WARNING "BrickOwl update did not complete successfully!\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl flagged " IO_MAGENTA "MUST_SYNC" IO_DEFAULT " for deep synchronization.\n" );
  return 0;
}


////



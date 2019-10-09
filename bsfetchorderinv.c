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
#include "brickowlinv.h"
#include "colortable.h"
#include "bstranslation.h"

#include "bricksync.h"


////


static void bsBrickLinkReplyOrderInventory( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsxInventory *inv;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if BS_ENABLE_DEBUG_SPECIAL
  if( response->body )
  {
    bsOrder *order;
    order = (bsOrder *)reply->extpointer;
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Saving the raw data of inventory for BrickLink order #"CC_LLD" as \"rawdataorder.txt\".\n", (long long)order->id );
    ccFileStore( "rawdataorder.txt", response->body, response->bodysize, 0 );
  }
#endif

  reply->result = resultcode;
  if( ( response ) && ( response->httpcode != 200 ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickLink HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  /* Parse inventory right here */
  inv = (bsxInventory *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
#if BS_ENABLE_DEBUG_OUTPUT
    bsOrder *order;
    order = (bsOrder *)reply->extpointer;
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Received inventory data for BrickLink order #"CC_LLD", parsing now.\n", (long long)order->id );
#endif
    if( !( blReadInventory( inv, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickLink JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Queue a batch of queries for order inventories with time >= basetimestamp */
int bsQueueBrickLinkFetchOrders( bsContext *context, bsOrderList *bsOrderlist, bsWorkList *worklist, int64_t basetimestamp, int64_t pendingtimestamp )
{
  int orderindex;
  char *pathstring;
  bsOrder *order;
  bsxInventory *inv;
  bsQueryReply *reply;

  DEBUG_SET_TRACKER();

  /* Process each order in the OrderList */
  for( orderindex = worklist->liststart ; orderindex < bsOrderlist->ordercount ; orderindex++ )
  {
    if( context->bricklink.querycount >= context->bricklink.pipelinequeuesize )
      break;
    if( mmBitMapDirectGet( &worklist->bitmap, orderindex ) )
      continue;
    mmBitMapDirectSet( &worklist->bitmap, orderindex );
    order = &bsOrderlist->orderarray[ orderindex ];

    /* Do we process that order? Check timestamp */
    if( order->changedate < ( order->status < BL_ORDER_STATUS_SHIPPED ? pendingtimestamp : basetimestamp ) )
      continue;
    if( order->status == BL_ORDER_STATUS_PURGED )
      continue;

#if BS_ENABLE_DEBUG_OUTPUT
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Queue query to fetch BrickLink order #"CC_LLD".\n", (long long)order->id );
#endif

    inv = bsxNewInventory();
    pathstring = ccStrAllocPrintf( "/api/store/v1/orders/"CC_LLD"/items", (long long)order->id );
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKLINK, orderindex, (void *)order, (void *)inv );
    bsBrickLinkAddQuery( context, "GET", pathstring, 0, 0, (void *)reply, bsBrickLinkReplyOrderInventory );
    free( pathstring );
  }
  worklist->liststart = orderindex;

  return ( context->bricklink.querycount ? 1 : 0 );
}


/* Fetch orders from orderlist >= basetimestamp, call callback for each, callback must not free 'inv' */
int bsFetchBrickLinkOrders( bsContext *context, bsOrderList *orderlist, int64_t basetimestamp, int64_t pendingtimestamp, void *uservalue, int (*processorder)( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue ) )
{
  int waitcount, orderlistindex;
  bsxInventory *inv;
  bsOrder *order;
  bsQueryReply *reply, *replynext;
  bsWorkList worklist;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Fetching inventory of BrickLink orders, refps : "CC_LLD" "CC_LLD".\n", (long long)basetimestamp, (long long)pendingtimestamp );
#endif

  /* Put that loop in a function somewhere? */
  bsTrackerInit( &tracker, context->bricklink.http );
  waitcount = context->bricklink.pipelinequeuesize;
  worklist.liststart = 0;
  mmBitMapInit( &worklist.bitmap, orderlist->ordercount, 0 );
  for( ; ; )
  {
    /* Launch queries to fetch some orders */
    if( !( tracker.failureflag ) )
      bsQueueBrickLinkFetchOrders( context, orderlist, &worklist, basetimestamp, pendingtimestamp );
    if( !( context->bricklink.querycount ) )
      break;
    if( context->bricklink.querycount <= waitcount )
      waitcount = context->bricklink.querycount - 1;

#if BS_ENABLE_DEBUG_SPECIAL
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "We have %d BrickLink API queries in queue.\n", context->bricklink.querycount );
#endif

    /* Wait for replies */
    bsWaitBrickLinkQueries( context, waitcount );

    /* Examine all queued replies */
    for( reply = context->replylist.first ; reply ; reply = replynext )
    {
      replynext = reply->list.next;
      inv = reply->opaquepointer;
      orderlistindex = reply->extid;
      bsTrackerAccumResult( context, &tracker, reply->result, BS_TRACKER_ACCUM_FLAGS_CANRETRY );

#if BS_ENABLE_DEBUG_SPECIAL
      ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Reply result code : %d\n", reply->result );
#endif

      /* If query wasn't successfull, clear bit to ask for it again */
      if( reply->result != HTTP_RESULT_SUCCESS )
      {
        mmBitMapDirectClear( &worklist.bitmap, orderlistindex );
        if( orderlistindex < worklist.liststart )
          worklist.liststart = orderlistindex;
      }
      else
      {
        order = (bsOrder *)reply->extpointer;

#if BS_ENABLE_DEBUG_SPECIAL
        ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "bsFetchBrickLinkOrders(), calling processorder(%p,%p) on order %d\n", order, inv, (int)order->id );
#endif

        if( !( processorder( context, order, inv, uservalue ) ) )
          bsTrackerAccumResult( context, &tracker, HTTP_RESULT_SYSTEM_ERROR, 0 );
      }
      bsxFreeInventory( inv );
      bsFreeReply( context, reply );
    }
  }

  mmBitMapFree( &worklist.bitmap );
  return ( tracker.failureflag ? 0 : 1 );
}


////


static void bsBrickOwlReplyOrderInventory( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsxInventory *inv;

  DEBUG_SET_TRACKER();

  reply = uservalue;
  context = reply->context;

#if 0
  ccFileStore( "zzz.txt", response->body, response->bodysize, 0 );
#endif

  reply->result = resultcode;
  if( ( response ) && ( response->httpcode != 200 ) )
  {
    if( response->httpcode )
      reply->result = HTTP_RESULT_CODE_ERROR;
    bsStoreError( context, "BrickOwl HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  /* Parse inventory right here */
  inv = (bsxInventory *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadOrderInventoryTranslate( inv, context->inventory, &context->translationtable, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}




/* Queue a batch of queries for order inventories with time >= basetimestamp */
int bsQueueBrickOwlFetchOrders( bsContext *context, bsOrderList *bsOrderlist, bsWorkList *worklist, int64_t basetimestamp, int64_t pendingtimestamp )
{
  int orderindex;
  char *querystring;
  bsOrder *order;
  bsxInventory *inv;
  bsQueryReply *reply;

  DEBUG_SET_TRACKER();

  /* Process each order in the OrderList */
  for( orderindex = worklist->liststart ; orderindex < bsOrderlist->ordercount ; orderindex++ )
  {
    if( context->brickowl.querycount >= context->brickowl.pipelinequeuesize )
      break;
    if( mmBitMapDirectGet( &worklist->bitmap, orderindex ) )
      continue;
    mmBitMapDirectSet( &worklist->bitmap, orderindex );
    order = &bsOrderlist->orderarray[ orderindex ];

    /* Do we process that order? Check timestamp */
    if( order->date < ( order->status < BO_ORDER_STATUS_SHIPPED ? pendingtimestamp : basetimestamp ) )
      continue;

#if BS_ENABLE_DEBUG_OUTPUT
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Queue query to fetch BrickOwl order #"CC_LLD".\n", (long long)order->id );
#endif

    inv = bsxNewInventory();
    querystring = ccStrAllocPrintf( "GET /v1/order/items?key=%s&order_id="CC_LLD" HTTP/1.1\r\nHost: api.brickowl.com\r\nConnection: Keep-Alive\r\n\r\n", context->brickowl.key, (long long)order->id );
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, orderindex, (void *)order, (void *)inv );
    bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY, (void *)reply, bsBrickOwlReplyOrderInventory );
    free( querystring );
  }
  worklist->liststart = orderindex;

  return ( context->brickowl.querycount ? 1 : 0 );
}


/* Fetch orders from orderlist >= basetimestamp, call callback for each, callback must not free 'inv' */
int bsFetchBrickOwlOrders( bsContext *context, bsOrderList *orderlist, int64_t basetimestamp, int64_t pendingtimestamp, void *uservalue, int (*processorder)( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue ) )
{
  int waitcount, orderlistindex;
  bsxInventory *inv;
  bsOrder *order;
  bsQueryReply *reply, *replynext;
  bsWorkList worklist;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Fetching inventory of BrickOwl orders, reference timestamps : "CC_LLD" "CC_LLD".\n", (long long)basetimestamp, (long long)pendingtimestamp );
#endif

  /* Put that loop in a function somewhere? */
  bsTrackerInit( &tracker, context->brickowl.http );
  waitcount = context->brickowl.pipelinequeuesize;
  worklist.liststart = 0;
  mmBitMapInit( &worklist.bitmap, orderlist->ordercount, 0 );
  for( ; ; )
  {
    /* Launch queries to fetch some orders */
    if( !( tracker.failureflag ) )
      bsQueueBrickOwlFetchOrders( context, orderlist, &worklist, basetimestamp, pendingtimestamp );
    if( !( context->brickowl.querycount ) )
      break;
    if( context->brickowl.querycount <= waitcount )
      waitcount = context->brickowl.querycount - 1;

#if BS_ENABLE_DEBUG_SPECIAL
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "We have %d BrickOwl API queries in queue.\n", context->brickowl.querycount );
#endif

    /* Wait for replies */
    bsWaitBrickOwlQueries( context, waitcount );

    /* Examine all queued replies */
    for( reply = context->replylist.first ; reply ; reply = replynext )
    {
      replynext = reply->list.next;
      inv = reply->opaquepointer;
      orderlistindex = reply->extid;
      bsTrackerAccumResult( context, &tracker, reply->result, BS_TRACKER_ACCUM_FLAGS_CANRETRY );

#if BS_ENABLE_DEBUG_SPECIAL
      ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Reply result code : %d\n", reply->result );
#endif

      /* If query wasn't successfull, clear bit to ask for it again */
      if( reply->result != HTTP_RESULT_SUCCESS )
      {
        mmBitMapDirectClear( &worklist.bitmap, orderlistindex );
        if( orderlistindex < worklist.liststart )
          worklist.liststart = orderlistindex;
      }
      else
      {
        order = (bsOrder *)reply->extpointer;
        if( !( processorder( context, order, inv, uservalue ) ) )
          bsTrackerAccumResult( context, &tracker, HTTP_RESULT_SYSTEM_ERROR, 0 );
      }
      bsxFreeInventory( inv );
      bsFreeReply( context, reply );
    }
  }

  mmBitMapFree( &worklist.bitmap );
  return ( tracker.failureflag ? 0 : 1 );
}



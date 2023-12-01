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

#include <assert.h>
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
void getFetchTimeStamp(char *fetch_timestamp, int time_length)
{
    // Set the current date and time for printout in the fetch message
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    size_t ret = strftime(fetch_timestamp, time_length, "%Y-%m-%d %H:%M:%S", tm);
    // validate pointer to the time value
    assert(ret);
    return;

}

static void bsBrickLinkReplyOrderList( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsOrderList *orderlist;

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
    bsStoreError( context, "BrickLink HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  /* Parse order list right here */
  orderlist = (bsOrderList *)reply->opaquepointer;
  memset( orderlist, 0, sizeof(bsOrderList) );
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( blReadOrderList( orderlist, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickLink JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Query BrickLink's orderlist */
int bsQueryBickLinkOrderList( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate )
{
  bsQueryReply *reply;
  bsTracker tracker;
  // Set the current date and time for printout in the fetch message
  char bl_fetch_date_time[64];
  getFetchTimeStamp(bl_fetch_date_time, 64);

  DEBUG_SET_TRACKER();

  bsTrackerInit( &tracker, context->bricklink.http );
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Fetching the BrickLink Order List (" IO_CYAN "%s" IO_DEFAULT ")...\n", bl_fetch_date_time);
  for( ; ; )
  {
    /* Add an OrderList query */
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKLINK, 0, 0, (void *)orderlist );
    bsBrickLinkAddQuery( context, "GET", "/api/store/v1/orders", "direction=in", 0, (void *)reply, bsBrickLinkReplyOrderList );
    /* Wait until all queries are processed */
    bsWaitBrickLinkQueries( context, 0 );
    /* Free all queued replies, break on success */
    if( bsTrackerProcessGenericReplies( context, &tracker, 1 ) )
      break;
    if( tracker.failureflag )
      return 0;
  }

  if( ( minimumorderdate | minimumchangedate ) )
    bsOrderListFilterOutDate( context, orderlist, minimumorderdate, minimumchangedate );

  orderlist->infolevel = BS_ORDER_INFOLEVEL_DETAILS;

  return 1;
}


/* Upgrade BrickLink's orderlist */
int bsQueryUpgradeBickLinkOrderList( bsContext *context, bsOrderList *orderlist, int infolevel )
{
  if( infolevel >= BS_ORDER_INFOLEVEL_FULL )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Not yet supported! %s:%d\n", __FILE__, __LINE__ );

    orderlist->infolevel = BS_ORDER_INFOLEVEL_FULL;
  }

  return 1;
}


////


static void bsBrickOwlReplyOrderList( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsOrderList *orderlist;

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

  /* Parse order list right here */
  orderlist = (bsOrderList *)reply->opaquepointer;
  memset( orderlist, 0, sizeof(bsOrderList) );
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadOrderList( orderlist, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


static void bsBrickOwlReplyOrderView( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsOrder *order;

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

  /* Parse order list right here */
  order = (bsOrder *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadOrderView( order, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Query BrickOwl's orderlist */
int bsQueryBickOwlOrderList( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate )
{
  int orderindex;
  bsQueryReply *reply;
  char *querystring;
  bsTracker tracker;
  bsOrder *order;
  // Set the current date and time for printout in the fetch message
  char bo_fetch_date_time[64];
  getFetchTimeStamp(bo_fetch_date_time, 64);

  DEBUG_SET_TRACKER();

  bsTrackerInit( &tracker, context->brickowl.http );
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Fetching the BrickOwl Order List (" IO_CYAN "%s" IO_DEFAULT ")...\n", bo_fetch_date_time);
  for( ; ; )
  {
    /* Add an OrderList query */
    querystring = ccStrAllocPrintf( "GET /v1/order/list?key=%s&order_time=%lld&limit=%d&list_type=store HTTP/1.1\r\nHost: api.brickowl.com\r\nConnection: Keep-Alive\r\n\r\n", context->brickowl.key, (long long)minimumorderdate, BS_FETCH_BRICKOWL_ORDERLIST_MAXIMUM );
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, 0, 0, (void *)orderlist );
    bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY, (void *)reply, bsBrickOwlReplyOrderList );
    free( querystring );
    /* Wait until all queries are processed */
    bsWaitBrickOwlQueries( context, 0 );
    /* Free all queued replies, break on success */
    if( bsTrackerProcessGenericReplies( context, &tracker, 1 ) )
      break;
    if( tracker.failureflag )
      return 0;
  }

  if( ( minimumorderdate | minimumchangedate ) )
    bsOrderListFilterOutDate( context, orderlist, minimumorderdate, minimumchangedate );

  orderlist->infolevel = BS_ORDER_INFOLEVEL_BASE;

  return 1;
}


/* Upgrade BrickLink's orderlist */
int bsQueryUpgradeBickOwlOrderList( bsContext *context, bsOrderList *orderlist, int infolevel )
{
  int orderindex;
  bsQueryReply *reply;
  char *querystring;
  bsTracker tracker;
  bsOrder *order;

  if( ( infolevel >= BS_ORDER_INFOLEVEL_DETAILS ) && ( orderlist->ordercount ) )
  {
#if 0
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Fetching details for " IO_CYAN "%d" IO_DEFAULT " BrickOwl orders...\n", orderlist->ordercount );
#endif
    bsTrackerInit( &tracker, context->brickowl.http );
    for( orderindex = 0 ; orderindex < orderlist->ordercount ; orderindex++ )
    {
      order = &orderlist->orderarray[ orderindex ];
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_FLUSH, "LOG: Fetching details for BrickOwl order #" CC_LLD ".\n", (long long)order->id );
      /* Add an OrderView query */
      querystring = ccStrAllocPrintf( "GET /v1/order/view?key=%s&order_id=" CC_LLD " HTTP/1.1\r\nHost: api.brickowl.com\r\nConnection: Keep-Alive\r\n\r\n", context->brickowl.key, (long long)order->id );
      reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, 0, 0, (void *)order );
      bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY, (void *)reply, bsBrickOwlReplyOrderView );
      free( querystring );
      /* Wait until all queries are processed */
      bsWaitBrickOwlQueries( context, 0 );

      /* Free all queued replies */
      if( !( bsTrackerProcessGenericReplies( context, &tracker, 1 ) ) )
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Failed to fetch details for BrickOwl Order " IO_RED "#" CC_LLD IO_WHITE ", skipping.\n", (long long)order->id );
      if( tracker.failureflag )
      {
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Giving up on fetching BrickOwl order details.\n" );
        break;
      }
    }

    orderlist->infolevel = BS_ORDER_INFOLEVEL_FULL;
  }

  return 1;
}


////


void bsOrderListFilterOutDate( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate )
{
  int srcindex, dstindex;
  bsOrder *order;

  dstindex = 0;
  for( srcindex = 0 ; srcindex < orderlist->ordercount ; srcindex++ )
  {
    order = &orderlist->orderarray[ srcindex ];
    if( ( order->date < minimumorderdate ) || ( order->changedate < minimumchangedate ) )
    {
      bsFreeOrderListEntry( order );
      continue;
    }
    if( srcindex != dstindex )
      orderlist->orderarray[ dstindex ] = *order;
    dstindex++;
  }
  orderlist->ordercount = dstindex;

  return;
}


/* Temporary stuff */
int bsOrderListFilterOutDateConditional( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate, int (*condition)( bsContext *context, bsOrder *order ) )
{
  int srcindex, dstindex, condforcecount;
  int64_t condmindate;
  bsOrder *order;

  condmindate = ( minimumchangedate > minimumorderdate ? minimumchangedate : minimumorderdate );
  if( condmindate >= 14*24*60*60 )
    condmindate -= 14*24*60*60;
  else
    condmindate = 0;

  condforcecount = 0;
  dstindex = 0;
  for( srcindex = 0 ; srcindex < orderlist->ordercount ; srcindex++ )
  {
    order = &orderlist->orderarray[ srcindex ];
    if( ( order->date < minimumorderdate ) || ( order->changedate < minimumchangedate ) )
    {
      if( !( condition( context, order ) ) && ( order->changedate >= condmindate ) )
        condforcecount++;
      else
      {
        bsFreeOrderListEntry( order );
        continue;
      }
    }
    if( srcindex != dstindex )
      orderlist->orderarray[ dstindex ] = *order;
    dstindex++;
  }
  orderlist->ordercount = dstindex;

  return condforcecount;
}


////



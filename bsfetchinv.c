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


static void bsBrickLinkReplyInventory( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsxInventory *inv;

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
    bsStoreError( context, "BrickLink HTTP Error", response->header, response->headerlength, response->body, response->bodysize );
  }
  mmListDualAddLast( &context->replylist, reply, offsetof(bsQueryReply,list) );

  /* Parse inventory right here */
  inv = (bsxInventory *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( blReadInventory( inv, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickLink JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Query the inventory from BrickLink */
bsxInventory *bsQueryBrickLinkInventory( bsContext *context )
{
  bsQueryReply *reply;
  bsxInventory *inv;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

  bsTrackerInit( &tracker, context->bricklink.http );
  inv = bsxNewInventory();
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Fetching the BrickLink Inventory...\n" );
  for( ; ; )
  {
    /* Add an Inventory query */
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKLINK, 0, 0, (void *)inv );
#if 1
    /* Only available inventory */
    bsBrickLinkAddQuery( context, "GET", "/api/store/v1/inventories", "status=Y", 0, (void *)reply, bsBrickLinkReplyInventory );
#else
    /* Available + stockroom? */
    bsBrickLinkAddQuery( context, "GET", "/api/store/v1/inventories", "status=Y%2CS", 0, (void *)reply, bsBrickLinkReplyInventory );
#endif
    /* Wait until all queries are processed */
    bsWaitBrickLinkQueries( context, 0 );
    /* Free all queued replies, break on success */
    if( bsTrackerProcessGenericReplies( context, &tracker, 1 ) )
      break;
    if( tracker.failureflag )
    {
      bsxFreeInventory( inv );
      return 0;
    }
  }

  return inv;
}


/* Query BrickLink inventory and orderlist at the moment the inventory was taken, return 0 on failure */
bsxInventory *bsQueryBrickLinkFullState( bsContext *context, bsOrderList *orderlist )
{
  int trycount;
  bsOrderList orderlistcheck;
  bsxInventory *inv;
  time_t synctime;

  DEBUG_SET_TRACKER();

  /* Get past orders in respect to inventory */
  /* Loop over if order list changed while retrieving inventory */

#if BS_INTERNAL_DEBUG
  if( httpGetQueryQueueCount( context->bricklink.http ) > 0 )
    BS_INTERNAL_ERROR_EXIT();
#endif

  for( trycount = 0 ; ; trycount++ )
  {
    /* Fetch the BrickLink Order List */
    if( !( bsQueryBickLinkOrderList( context, orderlist, 0, 0 ) ) )
      goto errorstep0;

    synctime = time( 0 );

    /* Fetch the BrickLink inventory */
    inv = bsQueryBrickLinkInventory( context );
    if( !( inv ) )
      goto errorstep1;

    /* We don't want to return an order list with a "topdate" matching the current timestamp */
    if( difftime( time( 0 ), synctime ) < 2.5 )
      ccSleep( 2000 );

    /* Fetch the BrickLink Order List again */
    if( !( bsQueryBickLinkOrderList( context, &orderlistcheck, 0, 0 ) ) )
      goto errorstep2;

    /* Do order lists match after the wait? If yes, break; */
    if( ( orderlist->topdate == orderlistcheck.topdate ) && ( orderlist->topdatecount == orderlistcheck.topdatecount ) )
      break;

    if( trycount >= 5 )
      goto errorstep2;

    /* An order arrived while we were fetching the inventory, isn't that neat? Yeah, start over. */
    ioPrintf( &context->output, 0, BSMSG_INFO "An order arrived while we were retrieving the inventory.\n" );
    bsxEmptyInventory( inv );
    blFreeOrderList( &orderlistcheck );
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink inventory has " IO_CYAN "%d" IO_DEFAULT " items in " IO_CYAN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  if( !( inv->itemcount - inv->itemfreecount ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "Is your BrickLink store closed? The inventory of a closed store appears totally empty from the API.\n" );
  blFreeOrderList( &orderlistcheck );
  return inv;

  errorstep2:
  bsxFreeInventory( inv );
  errorstep1:
  blFreeOrderList( orderlist );
  errorstep0:
  return 0;
}


////


/* Handle the reply from BrickOwl to an inventory query */
/* Parse the JSON and build an inventory by matching context's tracked inventory */
static void bsBrickOwlReplyInventory( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  bsxInventory *inv;

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

  /* Parse inventory right here */
  inv = (bsxInventory *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadInventoryTranslate( inv, context->inventory, &context->translationtable, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Query the inventory from BrickOwl */
bsxInventory *bsQueryBrickOwlInventory( bsContext *context )
{
  bsQueryReply *reply;
  bsxInventory *inv;
  char *querystring;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

  bsTrackerInit( &tracker, context->brickowl.http );
  inv = bsxNewInventory();
  for( ; ; )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Fetching the BrickOwl Inventory...\n" );
    /* Add an Inventory query */
    querystring = ccStrAllocPrintf( "GET /v1/inventory/list?key=%s%s HTTP/1.1\r\nHost: api.brickowl.com\r\nConnection: Keep-Alive\r\n\r\n", context->brickowl.key, ( context->brickowl.reuseemptyflag ? "&active_only=0" : "" ) );
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, 0, 0, (void *)inv );
    bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY, (void *)reply, bsBrickOwlReplyInventory );
    free( querystring );
    /* Wait until all queries are processed */
    bsWaitBrickOwlQueries( context, 0 );
    /* Free all queued replies, break on success */
    if( bsTrackerProcessGenericReplies( context, &tracker, 1 ) )
      break;
    if( tracker.failureflag )
    {
      bsxFreeInventory( inv );
      return 0;
    }
  }

  return inv;
}


/* Query BrickOwl diff inventory and orderlist at the moment the inventory was taken, return 0 on failure */
bsxInventory *bsQueryBrickOwlFullState( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate )
{
  int trycount;
  bsOrderList orderlistcheck;
  time_t synctime;
  bsxInventory *inv;

  DEBUG_SET_TRACKER();

  /* Get past orders in respect to inventory */
  /* Loop over if order list changed while retrieving inventory */

#if BS_INTERNAL_DEBUG
  if( httpGetQueryQueueCount( context->brickowl.http ) > 0 )
    BS_INTERNAL_ERROR_EXIT();
#endif

  for( trycount = 0 ; ; trycount++ )
  {
    /* Fetch the BrickOwl Order List */
    if( !( bsQueryBickOwlOrderList( context, orderlist, minimumorderdate, minimumorderdate ) ) )
      goto errorstep0;

    synctime = time( 0 );

    /* Fetch a BrickOwl diffinv */
    inv = bsQueryBrickOwlInventory( context );
    if( !( inv ) )
      goto errorstep1;

    /* We don't want to return an order list with a "topdate" matching the current timestamp */
    if( difftime( time( 0 ), synctime ) < 2.5 )
      ccSleep( 2000 );

    /* Fetch the BrickOwl Order List again */
    if( !( bsQueryBickOwlOrderList( context, &orderlistcheck, minimumorderdate, minimumorderdate ) ) )
      goto errorstep2;

    /* Do order lists match after the wait? If yes, break; */
    if( ( orderlist->topdate == orderlistcheck.topdate ) && ( orderlist->topdatecount == orderlistcheck.topdatecount ) )
      break;

    if( trycount >= 5 )
      goto errorstep2;

    /* An order arrived while we were fetching the inventory, isn't that neat? Yeah, start over. */
    ioPrintf( &context->output, 0, BSMSG_INFO "An order arrived while we were retrieving the inventory.\n" );
    bsxFreeInventory( inv );
    boFreeOrderList( &orderlistcheck );
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl inventory has " IO_CYAN "%d" IO_DEFAULT " items in " IO_CYAN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  boFreeOrderList( &orderlistcheck );
  return inv;

  errorstep2:
  bsxFreeInventory( inv );
  errorstep1:
  boFreeOrderList( orderlist );
  errorstep0:
  return 0;
}


////




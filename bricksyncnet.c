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
#include "antidebug.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <signal.h>
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
#include "bsantidebug.h"


////


bsQueryReply *bsAllocReply( bsContext *context, int type, int64_t extid, void *extpointer, void *opaquepointer )
{
  bsQueryReply *reply;

  DEBUG_SET_TRACKER();

  reply = malloc( sizeof(bsQueryReply) );
  reply->context = context;
  reply->opaquepointer = opaquepointer;
  reply->type = type;
  reply->extid = extid;
  reply->extpointer = extpointer;
  switch( reply->type )
  {
    case BS_QUERY_TYPE_BRICKLINK:
      context->bricklink.querycount++;
      break;
    case BS_QUERY_TYPE_WEBBRICKLINK:
      context->bricklink.webquerycount++;
      break;
    case BS_QUERY_TYPE_BRICKOWL:
      context->brickowl.querycount++;
      break;
    case BS_QUERY_TYPE_OTHER:
      break;
    default:
      BS_INTERNAL_ERROR_EXIT();
  }
  return reply;
}

void bsFreeReply( bsContext *context, bsQueryReply *reply )
{
  DEBUG_SET_TRACKER();

  switch( reply->type )
  {
    case BS_QUERY_TYPE_BRICKLINK:
      context->bricklink.querycount--;
      break;
    case BS_QUERY_TYPE_WEBBRICKLINK:
      context->bricklink.webquerycount--;
      break;
    case BS_QUERY_TYPE_BRICKOWL:
      context->brickowl.querycount--;
      break;
    case BS_QUERY_TYPE_OTHER:
      break;
    default:
      BS_INTERNAL_ERROR_EXIT();
  }
  mmListDualRemove( &context->replylist, reply, offsetof(bsQueryReply,list) );
  free( reply );
  return;
}

void bsBrickLinkAddQuery( bsContext *context, char *methodstring, char *pathstring, char *paramstring, char *bodystring, void *uservalue, void (*querycallback)( void *uservalue, int resultcode, httpResponse *response ) )
{
  char *oauthstring;
  char *querystring;
  oauthQuery query;
  ccGrowth growth;
  time_t oauthtime;

  DEBUG_SET_TRACKER();

  /* The BrickLink server doesn't want us to reuse timestamps. Why? WHYYYY??? */
  oauthtime = time( 0 );
  if( oauthtime <= context->bricklink.oauthtime )
    oauthtime = context->bricklink.oauthtime + 1;
  context->bricklink.oauthtime = oauthtime;

  /* Get OAuth authentification string */
  oauthQueryInit( &query, oauthtime, &context->oauthrand );
  oauthQuerySet( &query, context->bricklink.consumerkey, context->bricklink.consumersecret, context->bricklink.token, context->bricklink.tokensecret );
  querystring = ccStrAllocPrintf( "https://%s%s", BS_BRICKLINK_API_SERVER, pathstring );
  oauthQueryComputeSignature( &query, methodstring, querystring, paramstring );
  free( querystring );
  oauthstring = oauthQueryAuthorizationHeader( &query );
  oauthQueryFree( &query );

  /* Connect and send query */
  ccGrowthInit( &growth, 4096 );
  ccGrowthPrintf( &growth, "%s %s", methodstring, pathstring );
  if( paramstring )
    ccGrowthPrintf( &growth, "?%s", paramstring );
  ccGrowthPrintf( &growth, " HTTP/1.1\r\nHost: %s\r\nConnection: Keep-Alive\r\n%s\r\n", BS_BRICKLINK_API_SERVER, oauthstring );
  if( bodystring )
    ccGrowthPrintf( &growth, "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", (int)strlen( bodystring ), bodystring );
  else
    ccGrowthPrintf( &growth, "\r\n" );

#if BS_HTTP_DEBUG
  ioPrintf( &context->output, 0, "=== Our BrickLink Query Header ===\n" );
  ioPrintf( &context->output, 0, "%s\n", (char *)growth.data );
#endif

  /* Don't specify HTTP_QUERY_FLAGS_RETRY, we can't reuse oauth nonce */
  httpAddQuery( context->bricklink.http, (char *)growth.data, growth.offset, 0, uservalue, querycallback );

  /* Free OAuth string */
  free( oauthstring );

  bsApiHistoryIncrement( context, &context->bricklink.apihistory );
  return;
}


void bsBrickOwlAddQuery( bsContext *context, char *querystring, int httpflags, void *uservalue, void (*querycallback)( void *uservalue, int resultcode, httpResponse *response ) )
{
  DEBUG_SET_TRACKER();

#if BS_HTTP_DEBUG
  ioPrintf( &context->output, 0, "=== Our BrickOwl Query Header ===\n" );
  ioPrintf( &context->output, 0, "%s\n", (char *)querystring );
#endif
  httpAddQuery( context->brickowl.http, querystring, strlen( querystring ), httpflags, uservalue, querycallback );
  bsApiHistoryIncrement( context, &context->brickowl.apihistory );
  return;
}


////


void bsFlushTcpProcessHttp( bsContext *context )
{
  tcpFlush( &context->tcp );
  httpProcess( context->bricklink.http );
  httpProcess( context->brickowl.http );
  httpProcess( context->bricklink.webhttp );
  if( context->checkmessageflag )
    httpProcess( context->bricksyncwebhttp );
  return;
}


////


/* Wait until the count of pending BrickLink queries is <= maxpending */
void bsWaitBrickLinkQueries( bsContext *context, int maxpending )
{
  DEBUG_SET_TRACKER();

#if BS_ENABLE_DEBUG_OUTPUT && 0
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Enter bsWaitBrickLinkQueries().\n" );
#endif
  for( ; ; )
  {
    bsFlushTcpProcessHttp( context );
    if( httpGetQueryQueueCount( context->bricklink.http ) > maxpending )
      tcpWait( &context->tcp, 0 );
    else
      break;
  }
#if BS_ENABLE_DEBUG_OUTPUT && 0
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Exit bsWaitBrickLinkQueries().\n" );
#endif
  return;
}

/* Wait until the count of pending BrickLink queries is <= maxpending */
void bsWaitBrickLinkWebQueries( bsContext *context, int maxpending )
{
  DEBUG_SET_TRACKER();

#if BS_ENABLE_DEBUG_OUTPUT && 0
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Enter bsWaitBrickLinkWebQueries().\n" );
#endif
  for( ; ; )
  {
    bsFlushTcpProcessHttp( context );
    if( httpGetQueryQueueCount( context->bricklink.webhttp ) > maxpending )
      tcpWait( &context->tcp, 0 );
    else
      break;
  }
#if BS_ENABLE_DEBUG_OUTPUT && 0
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Exit bsWaitBrickLinkWebQueries().\n" );
#endif
  return;
}

/* Wait until the count of pending BrickOwl queries is <= maxpending */
void bsWaitBrickOwlQueries( bsContext *context, int maxpending )
{
  DEBUG_SET_TRACKER();

#if BS_ENABLE_DEBUG_OUTPUT && 0
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Enter bsWaitBrickOwlQueries().\n" );
#endif
  for( ; ; )
  {
    bsFlushTcpProcessHttp( context );
    if( httpGetQueryQueueCount( context->brickowl.http ) > maxpending )
      tcpWait( &context->tcp, 0 );
    else
      break;
  }
#if BS_ENABLE_DEBUG_OUTPUT && 0
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Exit bsWaitBrickOwlQueries().\n" );
#endif
  return;
}

/* Wait until the count of pending BrickSync queries is <= maxpending */
void bsWaitBrickSyncWebQueries( bsContext *context, int maxpending )
{
  DEBUG_SET_TRACKER();

  for( ; ; )
  {
    bsFlushTcpProcessHttp( context );
    if( httpGetQueryQueueCount( context->bricksyncwebhttp ) > maxpending )
      tcpWait( &context->tcp, 0 );
    else
      break;
  }
  return;
}


////


#define BS_TRACKER_SUCCESS_TO_ERROR_VALUE (64)
#define BS_TRACKER_SUCCESS_SATURATE (128)

void bsTrackerInit( bsTracker *tracker, httpConnection *http )
{
  DEBUG_SET_TRACKER();

  tracker->http = http;
  tracker->errorcount = 0;
  tracker->successcount = 0;
  tracker->failureflag = 0;
  tracker->mustsyncflag = 0;

  httpGetClearErrorCount( http );
  return;
}

/* Accumulate httpresults, return failure flag */
int bsTrackerAccumResult( bsContext *context, bsTracker *tracker, int httpresult, int accumflags )
{
  DEBUG_SET_TRACKER();

  /* Accumulate count of connection errors */
  tracker->errorcount += httpGetClearErrorCount( tracker->http );

  if( tracker->failureflag )
    return tracker->failureflag;
  switch( httpresult )
  {
    case HTTP_RESULT_SUCCESS:

      DEBUG_TRACKER_ACTIVITY();

      tracker->successcount++;
      if( tracker->successcount > BS_TRACKER_SUCCESS_SATURATE )
        tracker->successcount = BS_TRACKER_SUCCESS_SATURATE;
      if( ( tracker->errorcount > 0 ) && ( tracker->successcount >= BS_TRACKER_SUCCESS_TO_ERROR_VALUE ) )
      {
        tracker->errorcount--;
        tracker->successcount -= BS_TRACKER_SUCCESS_TO_ERROR_VALUE;
      }
      break;
    case HTTP_RESULT_CONNECT_ERROR:
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Failed to connect to server, trying again shortly...\n" );
      break;
    case HTTP_RESULT_TRYAGAIN_ERROR:
    case HTTP_RESULT_NOREPLY_ERROR:
      if( accumflags & BS_TRACKER_ACCUM_FLAGS_CANSYNC )
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "No reply received for query, we will retry and later perform a deep sync.\n" );
      else if( accumflags & BS_TRACKER_ACCUM_FLAGS_CANRETRY )
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "No reply received for query, we will retry.\n" );
      else
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "No reply received for query.\n" );
      tracker->mustsyncflag = 1;
      break;
    /* Problems handling the reply */
    case HTTP_RESULT_BADFORMAT_ERROR:
    case HTTP_RESULT_CODE_ERROR:
    case HTTP_RESULT_PARSE_ERROR:
      if( accumflags & BS_TRACKER_ACCUM_FLAGS_CANRETRY )
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Bad reply from server, trying again shortly...\n" );
      else
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Bad reply from server.\n" );
      if( !( accumflags & BS_TRACKER_ACCUM_FLAGS_NO_ERROR ) )
        tracker->errorcount++;
      break;
    /* Abort, serious error */
    case HTTP_RESULT_PROCESS_ERROR:
      tracker->failureflag = 1;
      break;
    /* Very serious errors */
    case HTTP_RESULT_SYSTEM_ERROR:
    default:
      bsFatalError( context );
      break;
  }

  if( tracker->errorcount >= 2 )
  {
    /* Flag all still pending queries to abort */
    httpAbortQueue( tracker->http );
    /* Abort */
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Too many connection errors, giving up.\n" );
    
    /* Resolve IPs again after too many connection errors */
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INIT "Resolving IP addresses for API and WEB services.\n" );
    context->bricklink.apiaddress = tcpResolveName( BS_BRICKLINK_API_SERVER, 0 );
    if( !( context->bricklink.apiaddress ) )
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKLINK_API_SERVER );
    else
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKLINK_API_SERVER, context->bricklink.apiaddress );
    context->bricklink.webaddress = tcpResolveName( BS_BRICKLINK_WEB_SERVER, 0 );
    if( !( context->bricklink.webaddress ) )
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKLINK_WEB_SERVER );
    else
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKLINK_WEB_SERVER, context->bricklink.webaddress );
    context->brickowl.apiaddress = tcpResolveName( BS_BRICKOWL_API_SERVER, 0 );
    if( !( context->brickowl.apiaddress ) )
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKOWL_API_SERVER );
    else
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKOWL_API_SERVER, context->brickowl.apiaddress );
    
    /* Define HTTP connections to BrickLink and BrickOwl */
    context->bricklink.http = httpOpen( &context->tcp, context->bricklink.apiaddress, 443, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING | HTTP_CONNECTION_FLAGS_SSL );
    context->bricklink.webhttp = httpOpen( &context->tcp, context->bricklink.webaddress, 80, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING );
    context->brickowl.http = httpOpen( &context->tcp, context->brickowl.apiaddress, 443, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING | HTTP_CONNECTION_FLAGS_SSL ); 
    
    tracker->failureflag = 1;
  }

  return tracker->failureflag;
}


/* Process all replies, free(reply) for each, accumulate tracker */
int bsTrackerProcessGenericReplies( bsContext *context, bsTracker *tracker, int allowretryflag )
{
  int successcount;
  bsQueryReply *reply, *replynext;

  DEBUG_SET_TRACKER();

  successcount = 0;
  for( reply = context->replylist.first ; reply ; reply = replynext )
  {
    replynext = reply->list.next;
    bsTrackerAccumResult( context, tracker, reply->result, ( allowretryflag ? BS_TRACKER_ACCUM_FLAGS_CANRETRY : 0 ) );
    if( reply->result == HTTP_RESULT_SUCCESS )
      successcount++;
    /* We don't free any reply->opaquepointer memory or other per-reply fancy processing */
    bsFreeReply( context, reply );
  }

  return successcount;
}


////


static void bsBrickOwlReplyUserDetails( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;
  boUserDetails *userdetails;

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

  /* Parse userdetails right here */
  userdetails = (boUserDetails *)reply->opaquepointer;
  if( ( reply->result == HTTP_RESULT_SUCCESS ) && ( response->body ) )
  {
    if( !( boReadUserDetails( userdetails, (char *)response->body, &context->output ) ) )
    {
      reply->result = HTTP_RESULT_PARSE_ERROR;
      bsStoreError( context, "BrickOwl JSON Parse Error", response->header, response->headerlength, response->body, response->bodysize );
    }
  }

  return;
}


/* Query the user details from BrickOwl */
int bsQueryBrickOwlUserDetails( bsContext *context, boUserDetails *userdetails )
{
  bsQueryReply *reply;
  char *querystring;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

  memset( userdetails, 0, sizeof(boUserDetails) );

  bsTrackerInit( &tracker, context->brickowl.http );
  for( ; ; )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INIT "Fetching BrickOwl user information...\n" );
    /* Add an userdetails query */
    querystring = ccStrAllocPrintf( "GET /v1/token/details?key=%s HTTP/1.1\r\nHost: api.brickowl.com\r\nConnection: Keep-Alive\r\n\r\n", context->brickowl.key );
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, 0, 0, (void *)userdetails );
    bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY, (void *)reply, bsBrickOwlReplyUserDetails );
    free( querystring );
    /* Wait until all queries are processed */
    bsWaitBrickOwlQueries( context, 0 );
    /* Free all queued replies, break on success */
    if( bsTrackerProcessGenericReplies( context, &tracker, 1 ) )
      break;
    if( tracker.failureflag )
      return 0;
  }

  return 1;
}



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
#include "bsbroadcast.h"


////


/* Version check and broadcast message, both from the GitHub repository over verified HTTPS:
 * - the latest release: GET https://api.github.com/repos/<repo>/releases/latest (its tag_name);
 * - the message: GET https://raw.githubusercontent.com/<repo>/master/broadcast/message.json.
 * Only someone who can push to the repository can change what users see. */

static const char *bsMessagePriorityString[] =
{
  [BS_BROADCAST_PRIORITY_NONE] = IO_DEFAULT "NONE" IO_DEFAULT,
  [BS_BROADCAST_PRIORITY_LOW] = IO_GREEN "LOW" IO_DEFAULT,
  [BS_BROADCAST_PRIORITY_MEDIUM] = IO_YELLOW "MEDIUM" IO_DEFAULT,
  [BS_BROADCAST_PRIORITY_HIGH] = IO_RED "HIGH" IO_DEFAULT,
  [BS_BROADCAST_PRIORITY_CRITICAL] = IO_RED "CRITICAL" IO_DEFAULT
};


////


static void bsGitHubReplyLatestRelease( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  char tag[BS_RELEASE_TAG_LENGTH_MAX];

  DEBUG_SET_TRACKER();

  context = uservalue;
  if( resultcode != HTTP_RESULT_SUCCESS )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: TCP HTTP error when fetching the latest BrickSync release : %d\n", resultcode );
    return;
  }
  if( ( response ) && ( response->httpcode != 200 ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: HTTP code when fetching the latest BrickSync release : %d\n", response->httpcode );
    return;
  }
  if( !( response ) || !( response->body ) || !( bsParseLatestRelease( (char *)response->body, &context->output, tag, sizeof(tag) ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Could not read the latest BrickSync release.\n" );
    return;
  }
  snprintf( context->latestreleasetag, sizeof(context->latestreleasetag), "%s", tag );
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Latest BrickSync release is %s\n", context->latestreleasetag );
  return;
}


static void bsGitHubReplyBroadcast( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsBroadcast msg;
  bsBroadcast *stored;

  DEBUG_SET_TRACKER();

  context = uservalue;
  if( resultcode != HTTP_RESULT_SUCCESS )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: TCP HTTP error when fetching BrickSync message : %d\n", resultcode );
    return;
  }
  if( ( response ) && ( response->httpcode != 200 ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: HTTP code when fetching BrickSync message : %d\n", response->httpcode );
    return;
  }
  if( !( response ) || !( response->body ) || !( bsParseBroadcast( (char *)response->body, &context->output, &msg ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: BrickSync message could not be read.\n" );
    return;
  }
  if( msg.timestamp == context->lastmessagetimestamp )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: No update for BrickSync message.\n" );
    return;
  }
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: New BrickSync message received, time stamp " CC_LLD ", priority %d\n", (long long)msg.timestamp, msg.priority );

  /* A message for older builds only does not concern this one */
  if( ( msg.onlybefore[0] ) && !( bsBuildOlderThan( BS_RELEASE_TAG, BS_BUILD_STAMP_STRING, msg.onlybefore ) ) )
    msg.priority = BS_BROADCAST_PRIORITY_NONE;

  if( !( context->message ) )
    context->message = malloc( sizeof(bsBroadcast) );
  stored = context->message;
  memcpy( stored, &msg, sizeof(bsBroadcast) );

  /* Do we have a new message for the user? */
  context->contextflags &= ~BS_CONTEXT_FLAGS_NEW_MESSAGE;
  context->lastmessagetimestamp = stored->timestamp;
  if( stored->priority > BS_BROADCAST_PRIORITY_NONE )
    context->contextflags |= BS_CONTEXT_FLAGS_NEW_MESSAGE;
  else
    stored->text[0] = 0;
  return;
}


static void bsPrintVersionSummary( bsContext *context )
{
  DEBUG_SET_TRACKER();

  if( !( context->latestreleasetag[0] ) )
    return;
  switch( bsReleaseState( BS_RELEASE_TAG, BS_BUILD_STAMP_STRING, context->latestreleasetag ) )
  {
    case BS_RELEASE_STATE_LATEST:
      ioPrintf( &context->output, 0, BSMSG_INFO "We are running the latest release of BrickSync : " IO_GREEN "%s" IO_DEFAULT "\n", context->latestreleasetag );
      break;
    case BS_RELEASE_STATE_NEWER_AVAILABLE:
      ioPrintf( &context->output, 0, BSMSG_WARNING "A new release of BrickSync is available : " IO_GREEN "%s" IO_WHITE " (we are " IO_YELLOW "%s" IO_WHITE ").\n", context->latestreleasetag, ( BS_RELEASE_TAG[0] ? BS_RELEASE_TAG : BS_VERSION_STRING ) );
      ioPrintf( &context->output, 0, BSMSG_INFO "Download it from " IO_CYAN BS_DOWNLOAD_URL IO_DEFAULT "\n" );
      break;
    case BS_RELEASE_STATE_AHEAD:
      ioPrintf( &context->output, 0, BSMSG_INFO "We are running a release newer than the latest one published (" IO_GREEN "%s" IO_DEFAULT ").\n", context->latestreleasetag );
      break;
    case BS_RELEASE_STATE_DEVELOPMENT:
      ioPrintf( &context->output, 0, BSMSG_INFO "This is a development build (" IO_YELLOW BS_VERSION_STRING IO_DEFAULT "); the latest release is " IO_GREEN "%s" IO_DEFAULT ".\n", context->latestreleasetag );
      break;
    default:
      break;
  }
  return;
}


static void bsPrintMessageTime( bsContext *context, char *headerstring )
{
  bsBroadcast *msg;
  time_t msgtime;
  struct tm timeinfo;
  char timebuf[64];
  ccGrowth growth;

  msg = context->message;
  if( ( msg ) && ( msg->timestamp > 0 ) )
  {
    msgtime = (time_t)msg->timestamp;
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)msgtime, 4 );
    timeinfo = *( localtime( &msgtime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    ioPrintf( &context->output, 0, BSMSG_INFO "%s : " IO_CYAN "%s" IO_DEFAULT " (%s ago).\n", headerstring, timebuf, growth.data );
    ccGrowthFree( &growth );
  }
  return;
}


static void bsPrintMessageText( bsContext *context, bsBroadcast *msg )
{
  ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_MAGENTA "=== Message starts ===\n" IO_DEFAULT );
  /* The text was cleaned of control characters when read (bsDecodeJsonText) */
  ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_WHITE "%s" IO_DEFAULT "\n", msg->text );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_MAGENTA "=== Message ends ===\n" IO_DEFAULT );
  return;
}


void bsReadBrickSyncMessage( bsContext *context )
{
  bsBroadcast *msg;

  DEBUG_SET_TRACKER();

  bsPrintVersionSummary( context );
  msg = context->message;
  if( !( msg ) || ( msg->priority == BS_BROADCAST_PRIORITY_NONE ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "No BrickSync broadcast message to display.\n" );
    return;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "Message priority : %s.\n", bsMessagePriorityString[msg->priority] );
  bsPrintMessageTime( context, "Message time stamp" );
  bsPrintMessageText( context, msg );
  return;
}


void bsFetchBrickSyncMessage( bsContext *context )
{
  char *querystring;
  bsBroadcast *msg;

  DEBUG_SET_TRACKER();

  if( !( context->bricksyncwebhttp ) && !( context->githubapihttp ) )
    return;

  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Checking for a new BrickSync release and broadcast message...\n" );

  if( context->githubapihttp )
  {
    querystring = ccStrAllocPrintf( "GET " BS_GITHUB_LATEST_RELEASE_PATH " HTTP/1.1\r\n" BS_USER_AGENT_HEADER "Host: " BS_GITHUB_API_SERVER "\r\nAccept: application/vnd.github+json\r\nConnection: Keep-Alive\r\n\r\n" );
    httpAddQuery( context->githubapihttp, querystring, strlen( querystring ), HTTP_QUERY_FLAGS_RETRY, (void *)context, bsGitHubReplyLatestRelease );
    free( querystring );
  }
  if( context->bricksyncwebhttp )
  {
    querystring = ccStrAllocPrintf( "GET " BS_GITHUB_BROADCAST_PATH " HTTP/1.1\r\n" BS_USER_AGENT_HEADER "Host: " BS_GITHUB_RAW_SERVER "\r\nConnection: Keep-Alive\r\n\r\n" );
    httpAddQuery( context->bricksyncwebhttp, querystring, strlen( querystring ), HTTP_QUERY_FLAGS_RETRY, (void *)context, bsGitHubReplyBroadcast );
    free( querystring );
  }

  /* Wait for both replies */
  bsWaitBrickSyncWebQueries( context, 0 );

  bsPrintVersionSummary( context );
  msg = context->message;
  if( ( msg ) && ( context->contextflags & BS_CONTEXT_FLAGS_NEW_MESSAGE ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "There's a new BrickSync broadcast message with priority %s.\n", bsMessagePriorityString[msg->priority] );
    bsPrintMessageTime( context, "BrickSync broadcast message time stamp" );
    if( msg->priority >= BS_BROADCAST_PRIORITY_HIGH )
      bsPrintMessageText( context, msg );
    else
      ioPrintf( &context->output, 0, BSMSG_INFO "Type \"" IO_CYAN "message" IO_DEFAULT "\" to read the message.\n" );
  }
  else
    ioPrintf( &context->output, 0, BSMSG_INFO "There's no new BrickSync broadcast message.\n" );

  return;
}

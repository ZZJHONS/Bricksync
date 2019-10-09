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


static const uint8_t bsMessageSignature[BS_MESSAGE_SIGNATURE_LENGTH] = { 'B', 'S', 'M', 'S', 'G', '0', '0', '0' };

static const char *bsMessagePriorityString[] =
{
  [BS_MESSAGE_PRIORITY_NONE] = IO_DEFAULT "NONE" IO_DEFAULT,
  [BS_MESSAGE_PRIORITY_LOW] = IO_GREEN "LOW" IO_DEFAULT,
  [BS_MESSAGE_PRIORITY_MEDIUM] = IO_YELLOW "MEDIUM" IO_DEFAULT,
  [BS_MESSAGE_PRIORITY_HIGH] = IO_RED "HIGH" IO_DEFAULT,
  [BS_MESSAGE_PRIORITY_CRITICAL] = IO_RED "CRITICAL" IO_DEFAULT 
};



////


enum
{
  BS_VERSION_REQ_FAIL,
  BS_VERSION_REQ_COMPLY,
  BS_VERSION_REQ_EXCEED
};

static inline int bsMeetVersionRequirement( int major, int minor, int revision )
{
  if( BS_VERSION_MAJOR > major )
    return BS_VERSION_REQ_EXCEED;
  if( BS_VERSION_MAJOR < major )
    return BS_VERSION_REQ_FAIL;
  if( BS_VERSION_MINOR > minor )
    return BS_VERSION_REQ_EXCEED;
  if( BS_VERSION_MINOR < minor )
    return BS_VERSION_REQ_FAIL;
  if( BS_VERSION_REVISION > revision )
    return BS_VERSION_REQ_EXCEED;
  if( BS_VERSION_REVISION < revision )
    return BS_VERSION_REQ_FAIL;
  return BS_VERSION_REQ_COMPLY;
}


static void bsBrickSyncReplyMessage( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsMessage *msg;

  DEBUG_SET_TRACKER();

  context = uservalue;

  /* Validate message */
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
  if( response->bodysize != sizeof(bsMessage) )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: BrickSync message of unexpected length %d != %d\n", (int)response->bodysize, (int)sizeof(bsMessage) );
    return;
  }
  msg = (bsMessage *)response->body;
  if( !( ccMemCmp( msg->signature, (void *)bsMessageSignature, BS_MESSAGE_SIGNATURE_LENGTH ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: BrickSync message signature error.\n" );
    return;
  }
  if( msg->timestamp == context->lastmessagetimestamp )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: No update for BrickSync message.\n" );
    return;
  }
  if( msg->messagepriority >= BS_MESSAGE_PRIORITY_CRITICAL )
    msg->messagepriority = BS_MESSAGE_PRIORITY_CRITICAL;

  /* Copy message */
  if( !( context->message ) )
    context->message = malloc( sizeof(bsMessage) );
  memcpy( context->message, msg, sizeof(bsMessage) );

  msg = context->message;
  msg->message[BS_MESSAGE_LENGTH_MAX-1] = 0;
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: New BrickSync message received, time stamp " CC_LLD ", priority %d\n", (long long)msg->timestamp, (int)msg->messagepriority );

  /* Verify if message applies */
  if( ( msg->messageflags & BS_MESSAGE_FLAGS_REQ_REGISTERED ) && ( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ) )
    msg->messagepriority = BS_MESSAGE_PRIORITY_NONE;
  if( ( msg->messageflags & BS_MESSAGE_FLAGS_REQ_UNREGISTERED ) && !( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ) )
    msg->messagepriority = BS_MESSAGE_PRIORITY_NONE;
  if( ( msg->messageflags & BS_MESSAGE_FLAGS_REQ_NOT_LATESTVERSION ) && ( bsMeetVersionRequirement( msg->latestversionmajor, msg->latestversionminor, msg->latestversionrevision ) >= BS_VERSION_REQ_COMPLY ) )
    msg->messagepriority = BS_MESSAGE_PRIORITY_NONE;
  if( ( msg->messageflags & BS_MESSAGE_FLAGS_REQ_NOT_MINIMUMVERSION ) && ( bsMeetVersionRequirement( msg->minimumversionmajor, msg->minimumversionminor, msg->minimumversionrevision ) >= BS_VERSION_REQ_COMPLY ) )
    msg->messagepriority = BS_MESSAGE_PRIORITY_NONE;
  if( !( msg->message[0] ) )
    msg->messagepriority = BS_MESSAGE_PRIORITY_NONE;

  /* Do we have a new message for the user? */
  context->contextflags &= ~BS_CONTEXT_FLAGS_NEW_MESSAGE;
  context->lastmessagetimestamp = msg->timestamp;
  if( msg->messagepriority > BS_MESSAGE_PRIORITY_NONE )
    context->contextflags |= BS_CONTEXT_FLAGS_NEW_MESSAGE;
  else
    msg->message[0] = 0;

  return;
}


static void bsPrintMessageSummary( bsContext *context )
{
  int versionreq;
  bsMessage *msg;
  char *printstring;

  DEBUG_SET_TRACKER();

  msg = context->message;
  if( msg )
  {
    versionreq = bsMeetVersionRequirement( msg->latestversionmajor, msg->latestversionminor, msg->latestversionrevision );
    if( versionreq >= BS_VERSION_REQ_COMPLY )
    {
      if( versionreq >= BS_VERSION_REQ_EXCEED )
        printstring = "We are running a BrickSync version from the " IO_CYAN "F" IO_RED "U" IO_MAGENTA "T" IO_GREEN "U" IO_CYAN "R" IO_YELLOW "E" IO_DEFAULT "!! Latest version should be";
      else
        printstring = "We are running the latest version of BrickSync";
    }
    else
      printstring = "A new version of BrickSync is available";
    ioPrintf( &context->output, 0, BSMSG_INFO "%s : " IO_GREEN "%d.%d.%d" IO_DEFAULT "\n", printstring, (int)msg->latestversionmajor, (int)msg->latestversionminor, (int)msg->latestversionrevision );
    if( !( bsMeetVersionRequirement( msg->minimumversionmajor, msg->minimumversionminor, msg->minimumversionrevision ) ) )
    {
      ioPrintf( &context->output, 0, BSMSG_WARNING "Your version of BrickSync, " IO_YELLOW "%d.%d.%d" IO_WHITE ", has been flagged as requiring an update!\n", (int)BS_VERSION_MAJOR, (int)BS_VERSION_MINOR, (int)BS_VERSION_REVISION );
      ioPrintf( &context->output, 0, BSMSG_INFO "You can download the latest version from " IO_CYAN "http://www.bricksync.net/" IO_DEFAULT "\n" );
    }
  }
  return;
}


static void bsPrintMessageTime( bsContext *context, char *headerstring )
{
  bsMessage *msg;
  time_t msgtime;
  struct tm timeinfo;
  char timebuf[64];
  ccGrowth growth;

  msg = context->message;
  if( msg )
  {
    msgtime = msg->timestamp;
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)msgtime, 4 );
    timeinfo = *( localtime( &msgtime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    ioPrintf( &context->output, 0, BSMSG_INFO "%s : " IO_CYAN "%s" IO_DEFAULT " (%s ago).\n", headerstring, timebuf, growth.data );
    ccGrowthFree( &growth );
  }

  return;
}


void bsReadBrickSyncMessage( bsContext *context )
{
  bsMessage *msg;
  bsPrintMessageSummary( context );

  DEBUG_SET_TRACKER();

  msg = context->message;
  if( !( msg ) || ( msg->messagepriority == BS_MESSAGE_PRIORITY_NONE ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "No BrickSync broadcast message to display.\n" );
    return;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "Message priority : %s.\n", bsMessagePriorityString[msg->messagepriority] );
  bsPrintMessageTime( context, "Message time stamp" );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_MAGENTA "=== Message starts ===\n" IO_DEFAULT );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_WHITE "%s" IO_DEFAULT "\n", msg->message );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_MAGENTA "=== Message ends ===\n" IO_DEFAULT );

  return;
}


void bsFetchBrickSyncMessage( bsContext *context )
{
  char *querystring;
  char *printstring;
  bsMessage *msg;

  DEBUG_SET_TRACKER();

  if( !( context->bricksyncwebhttp ) )
    return;

  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Fetching the latest BrickSync broadcast message...\n" );

  querystring = ccStrAllocPrintf( "GET /bsmessage HTTP/1.1\r\nHost: " BS_BRICKSYNC_WEB_SERVER "\r\nConnection: Close\r\n\r\n" );
  httpAddQuery( context->bricksyncwebhttp, querystring, strlen( querystring ), HTTP_QUERY_FLAGS_RETRY, (void *)context, bsBrickSyncReplyMessage );
  free( querystring );

  /* Wait for reply */
  bsWaitBrickSyncWebQueries( context, 0 );

  msg = context->message;
  if( msg )
  {
    bsPrintMessageSummary( context );
    if( context->contextflags & BS_CONTEXT_FLAGS_NEW_MESSAGE )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "There's a new BrickSync broadcast message with priority %s.\n", bsMessagePriorityString[msg->messagepriority] );
      bsPrintMessageTime( context, "BrickSync broadcast message time stamp" );
      if( msg->messagepriority >= BS_MESSAGE_PRIORITY_HIGH )
      {
        ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_MAGENTA "=== Message starts ===\n" IO_DEFAULT );
        ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_WHITE "%s" IO_DEFAULT "\n", msg->message );
        ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_MAGENTA "=== Message ends ===\n" IO_DEFAULT );
      }
      else
        ioPrintf( &context->output, 0, BSMSG_INFO "Type \"" IO_CYAN "message" IO_DEFAULT "\" to read the message.\n" );
    }
    else
      ioPrintf( &context->output, 0, BSMSG_INFO "There's no new BrickSync broadcast message.\n" );
  }
  else
    printstring = "No BrickSync broadcast message to display.";

  return;
}



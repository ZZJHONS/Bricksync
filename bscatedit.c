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


static void bsBrickOwlReplyEdit( void *uservalue, int resultcode, httpResponse *response )
{
  bsContext *context;
  bsQueryReply *reply;

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

  return;
}


/* Submit a BrickOwl catalog edit, new BLID for BOID */
static int bsSubmitBrickOwlEdit( bsContext *context, bsxItem *item, char *printfieldname, char *fieldname, char *fieldstring )
{
  bsQueryReply *reply;
  char *poststring;
  char *querystring;
  char *itemtype;
  bsTracker tracker;

  DEBUG_SET_TRACKER();

  if( item->flags & BSX_ITEM_FLAGS_DELETED )
    return 0;
  if( item->boid == -1 )
    return 0;

  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl catalog edition, assigning the %s " IO_GREEN "%s" IO_DEFAULT " to the BOID " IO_CYAN CC_LLD IO_DEFAULT ".\n", printfieldname, fieldstring, (long long)item->boid );

  bsTrackerInit( &tracker, context->brickowl.http );
  for( ; ; )
  {
    poststring = ccStrAllocPrintf( "key=%s&boid=" CC_LLD "&type=%s&value=%s", context->brickowl.key, (long long)item->boid, fieldname, fieldstring );
    querystring = ccStrAllocPrintf( "POST /v1/catalog_edit/basic_edit HTTP/1.1\r\nHost: api.brickowl.com\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\n\r\n%s", (int)strlen( poststring ), poststring );
    reply = bsAllocReply( context, BS_QUERY_TYPE_BRICKOWL, 0, (void *)item, (void *)&item->boid );
    bsBrickOwlAddQuery( context, querystring, HTTP_QUERY_FLAGS_RETRY | HTTP_QUERY_FLAGS_PIPELINING, (void *)reply, bsBrickOwlReplyEdit );
    free( querystring );
    free( poststring );

    bsWaitBrickOwlQueries( context, 0 );

    /* Free all queued replies, break on success */
    if( bsTrackerProcessGenericReplies( context, &tracker, 1 ) )
      break;
    if( tracker.failureflag )
      return 0;
  }

  return 1;
}


/* Submit a BrickOwl catalog edit, new BLID for BOID */
int bsSubmitBrickOwlEditBlid( bsContext *context, bsxItem *item )
{
  int retval;
  if( !( item->id ) )
    return 0;
  retval = bsSubmitBrickOwlEdit( context, item, "BLID", "bl_id", item->id );
  return retval;
}


/* Submit a BrickOwl catalog edit, new length for BOID */
int bsSubmitBrickOwlEditLength( bsContext *context, bsxItem *item, int itemlength )
{
  int retval;
  char *fieldstring;
  fieldstring = ccStrAllocPrintf( "%d", itemlength );
  retval = bsSubmitBrickOwlEdit( context, item, "length", "length", fieldstring );
  free( fieldstring );
  return retval;
}


/* Submit a BrickOwl catalog edit, new width for BOID */
int bsSubmitBrickOwlEditWidth( bsContext *context, bsxItem *item, int itemwidth )
{
  int retval;
  char *fieldstring;
  fieldstring = ccStrAllocPrintf( "%d", itemwidth );
  retval = bsSubmitBrickOwlEdit( context, item, "width", "width", fieldstring );
  free( fieldstring );
  return retval;
}


/* Submit a BrickOwl catalog edit, new height for BOID */
int bsSubmitBrickOwlEditHeight( bsContext *context, bsxItem *item, int itemheight )
{
  int retval;
  char *fieldstring;
  fieldstring = ccStrAllocPrintf( "%d", itemheight );
  retval = bsSubmitBrickOwlEdit( context, item, "height", "height", fieldstring );
  free( fieldstring );
  return retval;
}


/* Submit a BrickOwl catalog edit, new weight for BOID */
int bsSubmitBrickOwlEditWeight( bsContext *context, bsxItem *item, float itemweight )
{
  int retval;
  char *fieldstring;
  fieldstring = ccStrAllocPrintf( "%.3f", itemweight );
  retval = bsSubmitBrickOwlEdit( context, item, "weight", "weight", fieldstring );
  free( fieldstring );
  return retval;
}



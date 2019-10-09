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


/* Find all tab-limited strings, replace tabs with \0 */
static int blTextTabLineFilter( char **paramlist, int paramlistsize, char *string, int stringlength )
{
  int tabcount;
  int offset;

  for( tabcount = 0 ; tabcount < paramlistsize ; tabcount++ )
  {
    paramlist[tabcount] = string;
    offset = ccSeqFindChar( string, stringlength, '\t' );
    if( offset < 0 )
      break;
    string[offset] = 0;
    string += offset + 1;
    stringlength -= offset + 1;
  }

  return tabcount;
}


static int blTextParseItem( bsxInventory *inv, char **paramlist, int paramcount )
{
  char altflag, cntrflag;
  int64_t readint;
  bsxItem *item;
/*
[0] = TypeID
[1] = ID
[2] = Name
[3] = Quantity
[4] = ColorID
[5] = ExtraFlag
[6] = AlternateFlag
[7] = AlternateMatchID
[8] = CounterpartFlag
*/
  if( paramcount < 9 )
    return 0;
  if( strlen( paramlist[0] ) > 1 )
    return 0;
  item = bsxNewItem( inv );
  item->typeid = paramlist[0][0];
  bsxSetItemId( item, paramlist[1], -1 );
  bsxSetItemName( item, paramlist[2], -1 );
  if( !( ccStrParseInt64( paramlist[3], &readint ) ) )
    goto discard;
  bsxSetItemQuantity( inv, item, (int)readint );
  if( !( ccStrParseInt64( paramlist[4], &readint ) ) )
    goto discard;
  item->colorid = (int)readint;
  altflag = paramlist[6][0];
  if( strlen( paramlist[6] ) > 1 )
    goto discard;
  if( !( ccStrParseInt64( paramlist[7], &readint ) ) )
    goto discard;
  item->alternateid = (int)readint;
  cntrflag = paramlist[8][0];
  if( strlen( paramlist[8] ) > 1 )
    goto discard;
  if( cntrflag != 'N' )
    goto discard;
  return 1;

  discard:
  bsxRemoveItem( inv, item );
  return 0;
}


/* Parse tab-delimited BrickLink inventory */
static int blTextParseInventory( bsxInventory *inv, char *string, int stringlength )
{
  int linecount, offset, paramcount;
  char *paramlist[32];

  if( ccSeqFindStr( string, stringlength, "There was a problem processing your request" ) )
    return 0;
  if( ccSeqFindStr( string, stringlength, "Item not found in Catalog" ) )
    return 0;
  for( linecount = 0 ; ; linecount++ )
  {
    offset = ccSeqFindChar( string, stringlength, '\n' );
    if( offset < 0 )
      break;
    string[offset] = 0;
    paramcount = blTextTabLineFilter( paramlist, 32, string, offset );
    blTextParseItem( inv, paramlist, paramcount );
    string += offset + 1;
    stringlength -= offset + 1;
  }
  bsxPackInventory( inv );

  return 1;
}


////


static void bsBrickLinkReplySetInventory( void *uservalue, int resultcode, httpResponse *response )
{
  bsxInventory *inv;
  bsxInventory **retinv;

  retinv = uservalue;
  *retinv = 0;
  if( ( response ) && ( ( response->httpcode < 200 ) || ( response->httpcode > 299 ) ) )
    return;

#if 0
  ccFileStore( "zzz.txt", response->body, response->bodysize, 0 );
#endif

  inv = bsxNewInventory();
  if( !( blTextParseInventory( inv, response->body, response->bodysize ) ) )
  {
    bsxFreeInventory( inv );
    return;
  }
  *retinv = inv;
  return;
}


/* Fetch the inventory for a set ID */
bsxInventory *bsBrickLinkFetchSetInventory( bsContext *context, char itemtypeid, char *itemid )
{
  char *querystring;
  bsxInventory *inv;

  DEBUG_SET_TRACKER();

  inv = 0;
  querystring = ccStrAllocPrintf( "GET /catalogDownload.asp?a=a&viewType=4&itemTypeInv=%c&itemNo=%s&downloadType=T HTTP/1.1\r\nHost: www.bricklink.com\r\nConnection: Keep-Alive\r\n\r\n", itemtypeid, itemid );
  httpAddQuery( context->bricklink.webhttp, querystring, strlen( querystring ), HTTP_QUERY_FLAGS_RETRY, (void *)&inv, bsBrickLinkReplySetInventory );
  for( ; ; )
  {
    bsFlushTcpProcessHttp( context );
    if( httpGetQueryQueueCount( context->bricklink.webhttp ) > 0 )
      tcpWait( &context->tcp, 0 );
    else
      break;
  }
  free( querystring );
  if( inv )
  {
    bsxConsolidateInventoryByMatch( inv );
    if( inv->itemcount == 0 )
    {
      bsxFreeInventory( inv );
      inv = 0;
    }
  }

  return inv;
}



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
#include <string.h>
#include <math.h>
#include <time.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "iolog.h"
#include "debugtrack.h"

#include "bsx.h"
#include "bsxpg.h"
#include "tcp.h"

#include "json.h"
#include "bsorder.h"
#include "bricklink.h"


////


#ifndef ADDRESS
 #define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

static inline int intMin( int x, int y )
{
  return ( x < y ? x : y );
}

static inline int intMax( int x, int y )
{
  return ( x > y ? x : y );
}


#define BL_QUERY_DEBUG (0)

#define BL_PARSER_DEBUG (0)


////


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


typedef struct
{
  tcpContext *tcp;
  tcpLink *bllink;

  size_t recvsize;
  size_t recvalloc;
  void *recvdata;

  int status;
  int readyflag;
  int closedflag;
} blServerQuery;


static void blQueryRecv( void *uservalue, tcpDataBuffer *recvbuf )
{
  blServerQuery *blquery;
  blquery = uservalue;

  /* Append data to our buffer */
  if( ( blquery->recvsize + recvbuf->size ) > blquery->recvalloc )
  {
    blquery->recvalloc = intMax( blquery->recvsize + recvbuf->size, blquery->recvalloc << 1 );
    blquery->recvdata = realloc( blquery->recvdata, blquery->recvalloc );
  }
  memcpy( ADDRESS( blquery->recvdata, blquery->recvsize ), recvbuf->pointer, recvbuf->size );
  blquery->recvsize += recvbuf->size;

#if BL_QUERY_DEBUG
  printf( "\n==== RECV PACKET ( %d ) ====\n", (int)recvbuf->size );
  printf( "%.*s", (int)recvbuf->size, (char *)recvbuf->pointer );
#endif

  /* Free the buffer */
  tcpFreeRecvBuffer( blquery->tcp, blquery->bllink, recvbuf );

  return;
}

static void blQuerySendReady( void *uservalue, size_t sendbuffered )
{
  blServerQuery *blquery;
  blquery = uservalue;
  blquery->readyflag = 1;

#if BL_QUERY_DEBUG
printf( "SendReady\n" );
#endif

  return;
}

static void blQuerySendFinished( void *uservalue )
{
#if 0
  blServerQuery *blquery;
  blquery = uservalue;
#endif

#if BL_QUERY_DEBUG
printf( "SendFinished\n" );
#endif

  return;
}

static void blQueryTimeout( void *uservalue )
{
  blServerQuery *blquery;
  blquery = uservalue;

#if BL_QUERY_DEBUG
printf( "Timeout\n" );
#endif

  tcpClose( blquery->tcp, blquery->bllink );

  return;
}

static void blQueryClosed( void *uservalue )
{
  blServerQuery *blquery;
  blquery = uservalue;

  blquery->closedflag = 1;

#if BL_QUERY_DEBUG
printf( "Closed\n" );
#endif

  return;
}


tcpCallbackSet blQueryIO =
{
  .incoming = 0,
  .recv = blQueryRecv,
  .sendready = blQuerySendReady,
  .sendwait = 0,
  .sendfinished = blQuerySendFinished,
  .timeout = blQueryTimeout,
  .closed = blQueryClosed,
  .wake = 0
};


////


/* Send a single query to server, receive malloc()'ed buffer of reply */
char *blQueryInventory( size_t *retsize, char itemtypeid, char *itemid )
{
  tcpContext tcp;
  tcpLink *bllink;
  blServerQuery blquery;
  tcpDataBuffer *tcpbuffer;
  size_t datalen;

  if( !( tcpInit( &tcp, 1, 0 ) ) )
  {
    printf( "ERROR: Failed to initialize TCP interface.\n" );
    return 0;
  }

  memset( &blquery, 0, sizeof(blServerQuery) );
  bllink = tcpConnect( &tcp, "www.bricklink.com", 80, &blquery, &blQueryIO, 0 );
  if( !( bllink ) )
  {
    printf( "ERROR: Failed to open socket to server.\n" );
    goto error;
  }

  blquery.tcp = &tcp;
  blquery.bllink = bllink;
  blquery.recvsize = 0;
  blquery.recvalloc = 262144;
  blquery.recvdata = malloc( blquery.recvalloc );

  /* Wait until connection is ready */
  for( ; !( blquery.readyflag ) ; )
  {
    tcpWait( &tcp, 0 );
    if( blquery.closedflag )
      goto error;
  }

  /* Send our query */
  tcpbuffer = tcpAllocSendBuffer( &tcp, bllink, 4096 );
  datalen = snprintf( tcpbuffer->pointer, 4096, "GET /catalogDownload.asp?a=a&viewType=4&itemTypeInv=%c&itemNo=%s&downloadType=T HTTP/1.0\nHost: www.bricklink.com\n\n", itemtypeid, itemid );
  tcpQueueSendBuffer( &tcp, bllink, tcpbuffer, datalen );

  /* Wait for whole reply, server will close connection */
  for( ; !( blquery.closedflag ) ; )
    tcpWait( &tcp, 0 );

  tcpClose( &tcp, bllink );
  tcpEnd( &tcp );

  *retsize = blquery.recvsize;
  return blquery.recvdata;

  error:
  if( blquery.recvdata )
    free( blquery.recvdata );
  if( bllink )
    tcpClose( &tcp, bllink );
  tcpEnd( &tcp );
  return 0;
}


////


/* Send a single query to server, receive malloc()'ed buffer of reply */
char *blQueryPriceGuide( size_t *retsize, char itemtypeid, char *itemid, int itemcolorid )
{
  tcpContext tcp;
  tcpLink *bllink;
  blServerQuery blquery;
  tcpDataBuffer *tcpbuffer;
  size_t datalen;

  if( !( tcpInit( &tcp, 1, 0 ) ) )
  {
    printf( "ERROR: Failed to initialize TCP interface.\n" );
    return 0;
  }

  memset( &blquery, 0, sizeof(blServerQuery) );
  bllink = tcpConnect( &tcp, "www.bricklink.com", 80, &blquery, &blQueryIO, 0 );
  if( !( bllink ) )
  {
    printf( "ERROR: Failed to open socket to server.\n" );
    goto error;
  }

  blquery.tcp = &tcp;
  blquery.bllink = bllink;
  blquery.recvsize = 0;
  blquery.recvalloc = 262144;
  blquery.recvdata = malloc( blquery.recvalloc );

  /* Wait until connection is ready */
  for( ; !( blquery.readyflag ) ; )
  {
    tcpWait( &tcp, 0 );
    if( blquery.closedflag )
      goto error;
  }

  /* Send our query */
  tcpbuffer = tcpAllocSendBuffer( &tcp, bllink, 4096 );
  datalen = snprintf( tcpbuffer->pointer, 4096, "GET /priceGuide.asp?a=%c&viewType=N&colorID=%d&itemID=%s HTTP/1.0\nHost: www.bricklink.com\n\n", itemtypeid, itemcolorid, itemid );
  tcpQueueSendBuffer( &tcp, bllink, tcpbuffer, datalen );

  /* Wait for whole reply, server will close connection */
  for( ; !( blquery.closedflag ) ; )
    tcpWait( &tcp, 0 );

  tcpClose( &tcp, bllink );
  tcpEnd( &tcp );

  *retsize = blquery.recvsize;
  return blquery.recvdata;

  error:
  if( blquery.recvdata )
    free( blquery.recvdata );
  if( bllink )
    tcpClose( &tcp, bllink );
  tcpEnd( &tcp );
  return 0;
}


////


/* Get message body from HTTP query */
static char *blHttpGetMessageBody( char *data, size_t httpsize, size_t *retbodysize )
{
  char *header, *body, *string;
  int headerlength, contentlength;
  int32_t readint32;

  if( httpsize < 12 )
    return 0;

  header = data;
  body = ccSeqFindStrSkip( data, httpsize, "\r\n\r\n" );
  if( !( body ) )
  {
    body = ccSeqFindStrSkip( data, httpsize, "\n\n" );
    if( !( body ) )
      return 0;
  }

  body[-1] = 0;
  headerlength = (int)( body - header );
  string = ccSeqFindStrSkip( header, headerlength, "Content-Length:" );
  if( !( string ) )
    return 0;
  string = ccStrNextWord( string );
  if( !( ccStrParseInt32( string, &readint32 ) ) )
    return 0;
  contentlength = readint32;
  if( ( headerlength + contentlength ) < httpsize )
    return 0;
  *retbodysize = contentlength;
  return body;
}


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


static int blTextParseItem( bsxItem *item, char **paramlist, int paramcount )
{
  char altflag, cntrflag;
  int64_t readint;
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
  bsxClearItem( item );
  item->typeid = paramlist[0][0];
  item->id = paramlist[1];
  item->name = paramlist[2];
  if( !( ccStrParseInt64( paramlist[3], &readint ) ) )
    return 0;
  item->quantity = (int)readint;
  if( !( ccStrParseInt64( paramlist[4], &readint ) ) )
    return 0;
  item->colorid = (int)readint;
  /* For now, discard any 'alternate' or 'counterpart' item */
  altflag = paramlist[6][0];
  if( strlen( paramlist[6] ) > 1 )
    return 0;
  if( altflag != 'N' )
    return 0;
  cntrflag = paramlist[8][0];
  if( strlen( paramlist[8] ) > 1 )
    return 0;
  if( cntrflag != 'N' )
    return 0;
  return 1;
}


/* Parse tab-delimited BrickLink inventory */
static int blTextParseInventory( bsxInventory *inv, char *string, int stringlength )
{
  int linecount, offset, paramcount;
  char *paramlist[32];
  bsxItem *item;

  if( ccSeqFindStr( string, stringlength, "Item not found in Catalog" ) )
    return 0;
  item = bsxNewItem( inv );
  for( linecount = 0 ; ; linecount++ )
  {
    offset = ccSeqFindChar( string, stringlength, '\n' );
    if( offset < 0 )
      break;
    string[offset] = 0;
    paramcount = blTextTabLineFilter( paramlist, 32, string, offset );
    if( blTextParseItem( item, paramlist, paramcount ) )
    {
      inv->partcount += item->quantity;
      item = bsxNewItem( inv );
    }
    string += offset + 1;
    stringlength -= offset + 1;
  }
  bsxRemoveItem( inv, item );
  return 1;
}


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


/* Fetch inventory for item type and id */
int blFetchInventory( bsxInventory *inv, char itemtypeid, char *itemid )
{
  char *querydata, *invstring;
  size_t querysize;
  size_t invstringsize;

  querydata = blQueryInventory( &querysize, itemtypeid, itemid );
  if( !( querydata ) )
  {
    printf( "ERROR: Failed to fetch data from server.\n" );
    return 0;
  }
  inv->xmldata = querydata;
  invstring = blHttpGetMessageBody( querydata, querysize, &invstringsize );
  if( !( invstring ) )
  {
    printf( "ERROR: Badly formatted HTTP reply.\n" );
    return 0;
  }

  if( !( blTextParseInventory( inv, invstring, invstringsize ) ) )
  {
    printf( "ERROR: Failed to parse inventory string.\n" );
    return 0;
  }

  return 1;

}


int blFetchPriceGuide( bsxPriceGuide *pgnew, bsxPriceGuide *pgused, char itemtypeid, char *itemid, int itemcolorid )
{
  char *querydata, *pgstring;
  size_t querysize;
  size_t pgstringsize;

  querydata = blQueryPriceGuide( &querysize, itemtypeid, itemid, itemcolorid );
  if( !( querydata ) )
  {
    printf( "ERROR: Failed to fetch data from server.\n" );
    return 0;
  }
  pgstring = blHttpGetMessageBody( querydata, querysize, &pgstringsize );
  if( !( pgstring ) )
  {
    printf( "ERROR: Badly formatted HTTP reply.\n" );
    free( querydata );
    return 0;
  }

  if( !( blHtmlParsePriceGuide( pgnew, pgused, pgstring, pgstringsize ) ) )
  {
    printf( "ERROR: Failed to parse price guide string.\n" );
    free( querydata );
    return 0;
  }

  free( querydata );
  return 1;
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



static time_t blMakeTimeUTC( struct tm *timeinfo )
{
  int leap;
  time_t t;
  static const int monthoffset[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  if( (unsigned)timeinfo->tm_mon >= 12 )
    return (time_t)-1;
  if( timeinfo->tm_year < (1970-1900) )
    return (time_t)-1;
  leap = ( timeinfo->tm_year + 1900 ) - ( timeinfo->tm_mon < 2 );
  leap = ( leap / 4 ) - ( leap / 100 ) + ( leap / 400 ) - ( ( (1970-1) / 4 ) - ( (1970-1) / 100 ) + ( (1970-1) / 400 ) );
  t = ( (time_t)( timeinfo->tm_year - (1970-1900) ) * 365 ) + monthoffset[timeinfo->tm_mon] + ( timeinfo->tm_mday - 1 ) + leap;
  t = ( t * 24 ) + timeinfo->tm_hour;
  t = ( t * 60 ) + timeinfo->tm_min;
  t = ( t * 60 ) + timeinfo->tm_sec;
  return ( t < 0 ? (time_t)-1 : t );
}


static inline int blParseCheckName( char *name, int namelen, const char * const ref )
{
  return ( ( namelen == strlen( ref ) ) && ( ccMemCmpInline( name, (void *)ref, namelen ) ) );
}

static int blParseDateString( char *datestring, time_t *retrawtime )
{
  struct tm timeinfo;
  time_t rawtime;
  memset( &timeinfo, 0, sizeof(struct tm) );
  if( !( sscanf( datestring, "%d-%d-%dT%d:%d:%d.%*dZ", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec ) ) )
    return 0;
  timeinfo.tm_year -= 1900;
  timeinfo.tm_mon--;
  rawtime = blMakeTimeUTC( &timeinfo );
  if( rawtime == (time_t)-1 )
    return 0;
  *retrawtime = rawtime;
  return 1;
}


/*
{"meta":{"description":"OK","message":"OK","code":200},"data":[
{"meta":{"description":"A request has been made with a malformed JSON body.","message":"INVALID_REQUEST_BODY","code":400}}
*/

/* We accepted a '{' */
static int blParseMeta( jsonParser *parser )
{
  char *name;
  int namelen;
  int metacode, descriptionlength, messagelength;
  int64_t readint;
  jsonToken *nametoken;
  jsonToken *valuetoken;
  char *valuestring;
  char *description;
  char *message;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Meta ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  description = 0;
  message = 0;
  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    name = &parser->codestring[ nametoken->offset ];
    namelen = nametoken->length;

    if( blParseCheckName( name, namelen, "description" ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      description = &parser->codestring[ valuetoken->offset ];
      descriptionlength = valuetoken->length;
    }
    else if( blParseCheckName( name, namelen, "message" ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      message = &parser->codestring[ valuetoken->offset ];
      messagelength = valuetoken->length;
    }
    else if( blParseCheckName( name, namelen, "code" ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      metacode = readint;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Meta\n" );
#endif

  if( ( metacode < 200 ) || ( metacode > 299 ) )
  {
    ioPrintf( parser->log, 0, "BL JSON PARSER: Server replied with error code %d.\n", metacode );
    ioPrintf( parser->log, 0, "BL JSON PARSER: Server error message : \"%.*s\".\n", (int)messagelength, message );
    ioPrintf( parser->log, 0, "BL JSON PARSER: Server error description : \"%.*s\".\n", (int)descriptionlength, description );
    parser->errorcount++;
  }

  if( parser->errorcount )
    return 0;
  return 1;
}


/* We accepted a '{' */
static int blParseReply( jsonParser *parser, int (*datahandler)( jsonParser *parser, void *opaquedata ), void *opaquedata, int listflag, int reqdataflag )
{
  char *name;
  int namelen;
  int dataflag;
  jsonToken *nametoken;
  char *valuestring;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Reply ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  dataflag = 0;
  if( jsonTokenAccept( parser, JSON_TOKEN_RBRACE ) )
  {
    ioPrintf( parser->log, 0, "BL JSON PARSER: Error, empty reply.\n" );
    parser->errorcount++;
    return 0;
  }
  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Param : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    name = &parser->codestring[ nametoken->offset ];
    namelen = nametoken->length;

    if( blParseCheckName( name, namelen, "meta" ) )
    {
      if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) ) )
        return 0;
      if( !( blParseMeta( parser ) ) )
        return 0;
      if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACE ) ) )
        return 0;
    }
    else if( blParseCheckName( name, namelen, "data" ) )
    {
      if( listflag )
      {
        if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACKET ) ) )
          return 0;
        if( !( datahandler( parser, opaquedata ) ) )
          return 0;
        if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACKET ) ) )
          return 0;
      }
      else
      {
        if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) ) )
          return 0;
        if( !( datahandler( parser, opaquedata ) ) )
          return 0;
        if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACE ) ) )
          return 0;
      }
      dataflag = 1;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

  if( !( dataflag ) && ( reqdataflag ) )
  {
    ioPrintf( parser->log, 0, "BL JSON PARSER: Error, no \"data\" content found in reply.\n" );
    parser->errorcount++;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Reply\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


////


/* We accepted a '{' */
static int blParseOrderEntryCost( jsonParser *parser, bsOrder *order )
{
  char *name;
  int namelen;
  jsonToken *nametoken;
  jsonToken *valuetoken;
  char *valuestring;
  double readdouble;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Order Cost ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Param : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    name = &parser->codestring[ nametoken->offset ];
    namelen = nametoken->length;

    if( blParseCheckName( name, namelen, "subtotal" ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->subtotal = readdouble;
    }
    else if( blParseCheckName( name, namelen, "grand_total" ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->grandtotal = readdouble;
    }
    else if( blParseCheckName( name, namelen, "shipping" ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->shippingcost = readdouble;
    }
    else if( blParseCheckName( name, namelen, "vat_amount" ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->taxamount = readdouble;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Order Cost\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}

/* We accepted a '{' */
static int blParseOrderEntryDispCost( jsonParser *parser, bsOrder *order )
{
  char *name;
  int namelen;
  jsonToken *nametoken;
  jsonToken *valuetoken;
  char *valuestring;
  double readdouble;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Order Cost ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Param : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    name = &parser->codestring[ nametoken->offset ];
    namelen = nametoken->length;

    if( blParseCheckName( name, namelen, "grand_total" ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->paymentgrandtotal = readdouble;
    }
    else if( blParseCheckName( name, namelen, "currency_code" ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      valuestring = &parser->codestring[ valuetoken->offset ];
      if( valuetoken->length < BS_ORDER_CURRENCY_LENGTH )
      {
        memcpy( order->paymentcurrency, valuestring, valuetoken->length );
        order->paymentcurrency[ valuetoken->length ] = 0;
      }
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Order Cost\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}

static int blParseOrderEntryValue( jsonParser *parser, jsonToken *token, bsOrder *order )
{
  char *name;
  int namelen;
  int64_t readint;
  char *valuestring;
  jsonToken *valuetoken;
  time_t rawtime;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( blParseCheckName( name, namelen, "order_id" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    order->id = readint;
  }
  else if( blParseCheckName( name, namelen, "date_ordered" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( !( blParseDateString( valuestring, &rawtime ) ) )
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, failed to parse date string, offset %d ( %s:%d )\n", (parser->token)->offset, __FILE__, __LINE__ );
      parser->errorcount++;
      return 0;
    }
    order->date = (int64_t)rawtime;
  }
  else if( blParseCheckName( name, namelen, "date_status_changed" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( !( blParseDateString( valuestring, &rawtime ) ) )
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, failed to parse date string, offset %d ( %s:%d )\n", (parser->token)->offset, __FILE__, __LINE__ );
      parser->errorcount++;
      return 0;
    }
    order->changedate = (int64_t)rawtime;
  }
  else if( blParseCheckName( name, namelen, "total_count" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    order->partcount = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "unique_count" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    order->lotcount = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "cost" ) )
  {
    if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) ) )
      return 0;
    if( !( blParseOrderEntryCost( parser, order ) ) )
      return 0;
    jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
  }
  else if( blParseCheckName( name, namelen, "disp_cost" ) )
  {
    if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) ) )
      return 0;
    if( !( blParseOrderEntryDispCost( parser, order ) ) )
      return 0;
    jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
  }
  else if( blParseCheckName( name, namelen, "status" ) )
  {
    /* Compare string */
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( ccStrCmpSeq( "PENDING", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_PENDING;
    else if( ccStrCmpSeq( "UPDATED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_UPDATED;
    else if( ccStrCmpSeq( "PROCESSING", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_PROCESSING;
    else if( ccStrCmpSeq( "READY", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_READY;
    else if( ccStrCmpSeq( "PAID", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_PAID;
    else if( ccStrCmpSeq( "PACKED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_PACKED;
    else if( ccStrCmpSeq( "SHIPPED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_SHIPPED;
    else if( ccStrCmpSeq( "RECEIVED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_RECEIVED;
    else if( ccStrCmpSeq( "COMPLETED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_COMPLETED;
    else if( ccStrCmpSeq( "CANCELLED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_CANCELLED;
    else if( ccStrCmpSeq( "PURGED", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_PURGED;
    else if( ccStrCmpSeq( "NPB", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_NPB;
    else if( ccStrCmpSeq( "NPX", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_NPX;
    else if( ccStrCmpSeq( "NRS", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_NRS;
    else if( ccStrCmpSeq( "NSS", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_NSS;
    else if( ccStrCmpSeq( "OCR", valuestring, valuetoken->length ) )
      readint = BL_ORDER_STATUS_OCR;
    else
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown order status \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
    order->status = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "buyer_name" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      free( order->customer );
      order->customer = malloc( valuetoken->length + 1 );
      memcpy( order->customer, &parser->codestring[ valuetoken->offset ], valuetoken->length );
      order->customer[ valuetoken->length ] = 0;
    }
  }
  else
  {
    if( !( jsonParserSkipValue( parser ) ) )
      return 0;
  }

  return 1;
}

/* We accepted a '{' */
static int blParseOrderListEntry( jsonParser *parser, void *uservalue )
{
  jsonToken *nametoken;
  bsOrder *order;
  bsOrderList *orderlist;

  orderlist = uservalue;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

  orderlist->orderarray = realloc( orderlist->orderarray, ( orderlist->ordercount + 1 ) * sizeof(bsOrder) );
  order = &orderlist->orderarray[ orderlist->ordercount ];
  memset( order, 0, sizeof(bsOrder) );
  orderlist->ordercount++;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Order ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Param : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( blParseOrderEntryValue( parser, nametoken, order ) ) )
      return 0;

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

  /* Any extra order processing */
  order->service = BS_ORDER_SERVICE_BRICKLINK;
  if( order->changedate < order->date )
    order->changedate = order->date;
  if( order->changedate >= orderlist->topdate )
  {
    if( order->changedate == orderlist->topdate )
      orderlist->topdatecount++;
    else
    {
      orderlist->topdate = order->changedate;
      orderlist->topdatecount = 1;
    }
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Order\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* We accepted a '[' */
static int blParseOrderList( jsonParser *parser, void *opaquedata )
{
  return jsonParserListObjects( parser, opaquedata, blParseOrderListEntry, 0 );
}


/* The orderlist returned should be free()'d */
int blReadOrderList( bsOrderList *orderlist, char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  orderlist->orderarray = 0;
  orderlist->ordercount = 0;
  orderlist->topdate = 0;
  orderlist->topdatecount = 0;

  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    blParseReply( &parser, blParseOrderList, (void *)orderlist, 1, 1 );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    blFreeOrderList( orderlist );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


void blFreeOrderList( bsOrderList *orderlist )
{
  DEBUG_SET_TRACKER();

  bsFreeOrderList( orderlist );
  return;
}


////


static int blParseItemValue( jsonParser *parser, jsonToken *token, bsxItem *item )
{
  char *name;
  int namelen;
  int64_t readint;
  jsonToken *valuetoken;
  char *valuestring;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( blParseCheckName( name, namelen, "no" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    bsxSetItemId( item, valuestring, valuetoken->length );
  }
  else if( blParseCheckName( name, namelen, "name" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    bsxSetItemName( item, valuestring, valuetoken->length );
  }
  else if( blParseCheckName( name, namelen, "type" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    /* We clear any allocated name, then set the typename to a static string */
    bsxSetItemTypeName( item, 0, 0 );
    if( ccStrCmpSeq( "PART", valuestring, valuetoken->length ) )
    {
      item->typeid = 'P';
      item->typename = "Part";
    }
    else if( ccStrCmpSeq( "MINIFIG", valuestring, valuetoken->length ) )
    {
      item->typeid = 'M';
      item->typename = "Minifig";
    }
    else if( ccStrCmpSeq( "SET", valuestring, valuetoken->length ) )
    {
      item->typeid = 'S';
      item->typename = "Set";
    }
    else if( ccStrCmpSeq( "BOOK", valuestring, valuetoken->length ) )
    {
      item->typeid = 'B';
      item->typename = "Book";
    }
    else if( ccStrCmpSeq( "GEAR", valuestring, valuetoken->length ) )
    {
      item->typeid = 'G';
      item->typename = "Gear";
    }
    else if( ccStrCmpSeq( "CATALOG", valuestring, valuetoken->length ) )
    {
      item->typeid = 'C';
      item->typename = "Catalog";
    }
    else if( ccStrCmpSeq( "INSTRUCTION", valuestring, valuetoken->length ) )
    {
      item->typeid = 'I';
      item->typename = "Instruction";
    }
    else if( ccStrCmpSeq( "UNSORTED_LOT", valuestring, valuetoken->length ) )
    {
      item->typeid = 'U';
      item->typename = "Unsorted Lot";
    }
    else if( ccStrCmpSeq( "ORIGINAL_BOX", valuestring, valuetoken->length ) )
    {
      item->typeid = 'O';
      item->typename = "Box";
    }
    else
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown item type \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
  }
  else if( blParseCheckName( name, namelen, "categoryID" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->categoryid = (int)readint;
  }
  else
  {
    if( !( jsonParserSkipValue( parser ) ) )
      return 0;
  }

  return 1;
}


/* We accepted a '{' */
static int blParseLotItem( jsonParser *parser, bsxItem *item )
{
  jsonToken *nametoken;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Item ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( blParseItemValue( parser, nametoken, item ) ) )
      return 0;

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Item\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


static int blParseLotValue( jsonParser *parser, jsonToken *token, bsxItem *item )
{
  char *name;
  int namelen;
  int64_t readint;
  double readdouble;
  jsonToken *valuetoken;
  char *valuestring;
  char *decodedstring;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( blParseCheckName( name, namelen, "inventory_id" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->lotid = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "item" ) )
  {
    if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) ) )
      return 0;
    if( !( blParseLotItem( parser, item ) ) )
      return 0;
    jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
  }
  else if( blParseCheckName( name, namelen, "color_id" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->colorid = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "color_name" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    bsxSetItemColorName( item, valuestring, valuetoken->length );
  }
  else if( blParseCheckName( name, namelen, "quantity" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->quantity = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "new_or_used" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( ccStrCmpSeq( "N", valuestring, valuetoken->length ) )
      item->condition = 'N';
    else if( ccStrCmpSeq( "U", valuestring, valuetoken->length ) )
      item->condition = 'U';
    else
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown item condition \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
  }
  else if( blParseCheckName( name, namelen, "completeness" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( ccStrCmpSeq( "C", valuestring, valuetoken->length ) )
      item->completeness = 'C';
    else if( ccStrCmpSeq( "B", valuestring, valuetoken->length ) )
      item->completeness = 'B';
    else if( ccStrCmpSeq( "S", valuestring, valuetoken->length ) )
      item->completeness = 'S';
    else if( ccStrCmpSeq( "X", valuestring, valuetoken->length ) )
      item->completeness = 'S';
    else
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown item completeness \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
  }
  else if( blParseCheckName( name, namelen, "unit_price" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    item->price = readdouble;
    if( item->price == item->saleprice )
      item->saleprice = 0.0;
  }
  else if( blParseCheckName( name, namelen, "unit_price_final" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    item->saleprice = readdouble;
    if( item->price == item->saleprice )
      item->saleprice = 0.0;
  }
  else if( blParseCheckName( name, namelen, "description" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( valuetoken->length )
    {
      decodedstring = jsonDecodeEscapeString( valuestring, valuetoken->length, 0 );
      if( decodedstring )
      {
        bsxSetItemComments( item, decodedstring, strlen( decodedstring ) );
        free( decodedstring );
      }
    }
  }
  else if( blParseCheckName( name, namelen, "remarks" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( valuetoken->length )
    {
      decodedstring = jsonDecodeEscapeString( valuestring, valuetoken->length, 0 );
      if( decodedstring )
      {
        bsxSetItemRemarks( item, decodedstring, strlen( decodedstring ) );
        free( decodedstring );
      }
    }
  }
  else if( blParseCheckName( name, namelen, "bulk" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->bulk = (int)readint;
    if( item->bulk <= 1 )
      item->bulk = 0;
  }
  else if( blParseCheckName( name, namelen, "is_retain" ) )
  {
    if( jsonTokenAccept( parser, JSON_TOKEN_TRUE ) )
      item->stockflags |= BSX_ITEM_STOCKFLAGS_RETAIN;
    else if( jsonTokenAccept( parser, JSON_TOKEN_FALSE ) )
      item->stockflags &= ~BSX_ITEM_STOCKFLAGS_RETAIN;
    else
    {
      valuetoken = parser->token;
      valuestring = &parser->codestring[ valuetoken->offset ];
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown item is_retain \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
  }
  else if( blParseCheckName( name, namelen, "is_stock_room" ) )
  {
    if( jsonTokenAccept( parser, JSON_TOKEN_TRUE ) )
      item->stockflags |= BSX_ITEM_STOCKFLAGS_STOCKROOM;
    else if( jsonTokenAccept( parser, JSON_TOKEN_FALSE ) )
      item->stockflags &= ~BSX_ITEM_STOCKFLAGS_STOCKROOM;
    else
    {
      valuetoken = parser->token;
      valuestring = &parser->codestring[ valuetoken->offset ];
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown item is_stock_room \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
  }
  else if( blParseCheckName( name, namelen, "stock_room_id" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    item->stockflags &= ~( BSX_ITEM_STOCKFLAGS_STOCKROOM_A | BSX_ITEM_STOCKFLAGS_STOCKROOM_B | BSX_ITEM_STOCKFLAGS_STOCKROOM_C );
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( ccStrCmpSeq( "A", valuestring, valuetoken->length ) )
      item->stockflags |= BSX_ITEM_STOCKFLAGS_STOCKROOM_A;
    else if( ccStrCmpSeq( "B", valuestring, valuetoken->length ) )
      item->stockflags |= BSX_ITEM_STOCKFLAGS_STOCKROOM_B;
    else if( ccStrCmpSeq( "C", valuestring, valuetoken->length ) )
      item->stockflags |= BSX_ITEM_STOCKFLAGS_STOCKROOM_C;
    else
    {
      ioPrintf( parser->log, 0, "BL JSON PARSER: Error, unknown item stock_room_id \"%.*s\", offset %d\n", (int)valuetoken->length, valuestring, (parser->token)->offset );
      parser->errorcount++;
      return 0;
    }
  }
  else if( blParseCheckName( name, namelen, "my_cost" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    item->mycost = readdouble;
  }
  else if( blParseCheckName( name, namelen, "sale_rate" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->sale = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "tier_quantity1" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->tq1 = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "tier_quantity2" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->tq2 = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "tier_quantity3" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    item->tq3 = (int)readint;
  }
  else if( blParseCheckName( name, namelen, "tier_price1" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    item->tp1 = readdouble;
  }
  else if( blParseCheckName( name, namelen, "tier_price2" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    item->tp2 = readdouble;
  }
  else if( blParseCheckName( name, namelen, "tier_price3" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    item->tp3 = readdouble;
  }
  else
  {
    if( !( jsonParserSkipValue( parser ) ) )
      return 0;
  }

  return 1;
}


/* We accepted a '{' */
static int blParseLot( jsonParser *parser, void *uservalue )
{
  jsonToken *nametoken;
  bsxItem *item;
  bsxInventory *inv;

  inv = uservalue;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Lot ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  item = bsxNewItem( inv );
  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( blParseLotValue( parser, nametoken, item ) ) )
      return 0;

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

  /* Correct any bad item data */
  bsxVerifyItem( item );

  /* Some extra processing */
  inv->partcount += item->quantity;

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Lot\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* We accepted a '[' */
static int blParseInventory( jsonParser *parser, void *opaquedata )
{
  return jsonParserListObjects( parser, opaquedata, blParseLot, 1 );
}


/* Read order inventory */
int blReadOrderInventory( bsxInventory *inv, char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    blParseReply( &parser, blParseInventory, (void *)inv, 1, 1 );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}



////


/* Read inventory */
int blReadInventory( bsxInventory *inv, char *string, ioLog *log )
{
  DEBUG_SET_TRACKER();

  /* Format about identical to order inventory! */
  /* Code can handle the difference ( [[...]] versus [...] ) just fine */
  return blReadOrderInventory( inv, string, log );
}


////



/* We accepted a '{' */
static int blParseLotID( jsonParser *parser, void *opaquedata )
{
  int64_t *retlotid;
  jsonToken *nametoken;

  retlotid = opaquedata;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Lot ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( blParseCheckName( &parser->codestring[ nametoken->offset ], nametoken->length, "inventory_id" ) )
    {
      if( !( jsonReadInteger( parser, retlotid, 0 ) ) )
        return 0;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Lot\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* Read lotID for a single lot, as reply to a lot creation */
int blReadLotID( int64_t *retlotid, char *string, ioLog *log )
{
  int retval;
  int64_t lotid;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  lotid = -1;
  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    blParseReply( &parser, blParseLotID, (void *)&lotid, 0, 1 );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  if( !( parser.errorcount ) && ( lotid != -1 ) )
    *retlotid = lotid;

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


/* We accepted a '{' */
static int blParseGeneric( jsonParser *parser, void *opaquedata )
{
  if( !( jsonParserSkipObject( parser ) ) )
    return 0;
  return 1;
}


/* Read generic reply, return 1 on success */
int blReadGeneric( char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    blParseReply( &parser, blParseGeneric, (void *)0, 0, 0 );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


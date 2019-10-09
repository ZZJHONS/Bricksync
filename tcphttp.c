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
#include <limits.h>
#include <float.h>

#include "cpuconfig.h"

#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "debugtrack.h"

#include "tcp.h"
#include "tcphttp.h"


////


#define TCPHTTP_DEBUG (0)
#define TCPHTTP_DEBUG_CHUNK (0)
#define TCPHTTP_DEBUG_HEADERS (0)
#define TCPHTTP_NETIO_DEBUG (0)
#define TCPHTTP_RECV_DATA_DEBUG (0)

#if 1
 #define TCPHTTP_DEBUG_PRINTF(...) ccDebugLog( "debug-tcphttp.txt", __VA_ARGS__ )
#else
 #define TCPHTTP_DEBUG_PRINTF(...) printf( __VA_ARGS__ )
#endif


////


/* Always null-terminate the query's data? */
#define HTTP_APPEND_ZERO_BYTE (1)

/* Timeout when idle and when waiting for a reply */
#define HTTP_TIMEOUT_IDLE (45*1000)
#define HTTP_TIMEOUT_WAITING (45*1000)

/* Maximum of pipelined calls on the same socket */
#define HTTP_KEEP_ALIVE_MAX_COUNT (80)

/* Maximum count of retry for a failing query */
#define HTTP_FAILED_RETRY_MAXIMUM (3)


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


////


typedef struct
{
  /* Current status of query, HTTP_QUERY_STATUS_xxx */
  int status;
  /* Query flags */
  int flags;
  /* Buffer for both header and content */
  size_t dataoffset;
  size_t dataalloc;
  void *data;
  /* Keep header string in case we have to resend */
  char *querystring;
  size_t querylength;
  /* HTTP pipelining index */
  int pipelineindex;

  httpResponse response;

  /* User value set by httpAddQuery() call */
  void *uservalue;
  /* User callback */
  void (*querycallback)( void *uservalue, int resultcode, httpResponse *response );
  /* Return status for query callback, HTTP_RESULT_xxx */
  int resultcode;

  /* Size of data left to receive in current chunk, -1 if no chunk active */
  ssize_t chunksize;
  /* Offset where the chunk size string begins */
  size_t chunkoffset;

  /* List of pending queries */
  mmListNode list;
} httpQuery;

/* Flags 0x1|0x2 defined in tcphttp.h */

/* Query reply has no content length */
#define HTTP_QUERY_FLAGS_NOCONTENTLENGTH (0x10000)
/* Query reply is chunked */
#define HTTP_QUERY_FLAGS_CHUNKED (0x20000)
/* Query has been moved from waitlist to sentlist */
#define HTTP_QUERY_FLAGS_SENT (0x40000)
/* Abort pending query, return NOREPLY */
#define HTTP_QUERY_FLAGS_ABORTED (0x80000)

enum
{
  /* tcpConnect() went through */
  HTTP_CONNECTION_STATUS_CONNECTING,
  /* Connection is ready to accept queries */
  HTTP_CONNECTION_STATUS_READY,
  /* Connection wants to wait before sending more data */
  HTTP_CONNECTION_STATUS_WAIT,
  /* Connection must receive all its queries before processing the next one */
  HTTP_CONNECTION_STATUS_FLUSH,
  /* Connection is scheduled for closing */
  HTTP_CONNECTION_STATUS_CLOSING,
  /* Link has been closed, http->link is zero or should soon be set to zero */
  HTTP_CONNECTION_STATUS_CLOSED
};

enum
{
  HTTP_QUERY_STATUS_WAITHEADER,
  HTTP_QUERY_STATUS_WAITCONTENT,
  HTTP_QUERY_STATUS_SKIPLINE,
  HTTP_QUERY_STATUS_SKIPTRAILER,
  HTTP_QUERY_STATUS_FAILED,
  HTTP_QUERY_STATUS_ERROR,
  HTTP_QUERY_STATUS_COMPLETE
};

#define HTTP_CONNECTION_FLAGS_IDLE (0x10000)


////


static void httpConnectionRecv( void *uservalue, tcpDataBuffer *recvbuf )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

  http = uservalue;
  mmListDualAddLast( &http->recvbuflist, recvbuf, offsetof(tcpDataBuffer,userlist) );

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: Recv %d bytes (time %ld)\n", (int)recvbuf->size, (long)time(0) );
#endif
#if TCPHTTP_RECV_DATA_DEBUG
  TCPHTTP_DEBUG_PRINTF( "==== RECV PACKET BEGIN ( %d ) ====\n", (int)recvbuf->size );
  TCPHTTP_DEBUG_PRINTF( "%.*s\n\n", (int)recvbuf->size, (char *)recvbuf->pointer );
  TCPHTTP_DEBUG_PRINTF( "==== RECV PACKET END ( %d ) ====\n", (int)recvbuf->size );
#endif

  return;
}

static void httpConnectionSendReady( void *uservalue, size_t sendbuffered )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

  http = uservalue;
  if( http->status == HTTP_CONNECTION_STATUS_CONNECTING )
  {
    http->status = HTTP_CONNECTION_STATUS_READY;
    http->flags |= HTTP_CONNECTION_FLAGS_IDLE;
    tcpSetTimeout( http->tcp, http->link, http->idletimeout );
  }
  else if( http->status == HTTP_CONNECTION_STATUS_WAIT )
    http->status = HTTP_CONNECTION_STATUS_READY;

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: SendReady (time %ld)\n", (long)time(0) );
#endif

  return;
}

static void httpConnectionSendWait( void *uservalue, size_t sendbuffered )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

  http = uservalue;
  if( http->status == HTTP_CONNECTION_STATUS_READY )
    http->status = HTTP_CONNECTION_STATUS_WAIT;

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: SendWait (time %ld)\n", (long)time(0) );
#endif

  return;
}

static void httpConnectionSendFinished( void *uservalue )
{
  DEBUG_SET_TRACKER();

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: SendFinished (time %ld)\n", (long)time(0) );
#endif

  return;
}

static void httpConnectionTimeout( void *uservalue )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

  http = uservalue;
  if( ( http->querysentlist.first ) || ( http->querywaitlist.first ) )
  {
    http->errorcount++;
    http->retryfailurecount++;
  }
  http->status = HTTP_CONNECTION_STATUS_CLOSING;

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: Timeout (time %ld)\n", (long)time(0) );
#endif

  return;
}

static void httpConnectionClosed( void *uservalue )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

  http = uservalue;
  if( http->status == HTTP_CONNECTION_STATUS_CONNECTING )
  {
    http->errorcount++;
    http->retryfailurecount++;
  }
  http->status = HTTP_CONNECTION_STATUS_CLOSED;

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: Closed (time %ld)\n", (long)time(0) );
#endif

  return;
}


static void httpConnectionWake( void *uservalue )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

#if TCPHTTP_NETIO_DEBUG
  TCPHTTP_DEBUG_PRINTF( "HTTP: Wake (time %ld)\n", (long)time(0) );
#endif

  http = uservalue;
  if( http->wake )
    http->wake( http->tcp, http, http->wakecontext );
  return;
}


tcpCallbackSet httpConnectionIO =
{
  .incoming = 0,
  .recv = httpConnectionRecv,
  .sendready = httpConnectionSendReady,
  .sendwait = httpConnectionSendWait,
  .sendfinished = httpConnectionSendFinished,
  .timeout = httpConnectionTimeout,
  .closed = httpConnectionClosed,
  .wake = httpConnectionWake
};



////////////////////////////////////////////////////////////////////////////////



httpConnection *httpOpen( tcpContext *tcp, char *address, int port, int flags )
{
  httpConnection *http;

  DEBUG_SET_TRACKER();

  http = malloc( sizeof(httpConnection) );
  memset( http, 0, sizeof(httpConnection) );
  http->status = HTTP_CONNECTION_STATUS_CLOSED;
  http->tcp = tcp;
  http->link = 0;
  http->address = malloc( strlen( address ) + 1 );
  strcpy( http->address, address );
  http->port = port;
  http->keepalivemax = 1;
  http->idletimeout = HTTP_TIMEOUT_IDLE;
  http->waitingtimeout = HTTP_TIMEOUT_WAITING;
  http->flags = flags & HTTP_CONNECTION_FLAGS_PUBLICMASK;
  http->serverflags = http->flags & HTTP_CONNECTION_FLAGS_SSL;
  http->errorcount = 0;
  http->retryfailurecount = 0;
  http->queryqueuecount = 0;
  mmListDualInit( &http->querywaitlist );
  mmListDualInit( &http->querysentlist );
  mmListDualInit( &http->recvbuflist );
  return http;
}


int httpAddQuery( httpConnection *http, char *querystring, size_t querylen, int queryflags, void *queryuservalue, void (*querycallback)( void *uservalue, int resultcode, httpResponse *response ) )
{
  httpQuery *query;

  DEBUG_SET_TRACKER();

  query = malloc( sizeof(httpQuery) );
  query->status = HTTP_QUERY_STATUS_WAITHEADER;
  query->flags = queryflags;
  query->dataalloc = 0;
  query->dataoffset = 0;
  query->data = 0;
  query->querystring = 0;
  query->querylength = 0;
  query->pipelineindex = 0;
  query->uservalue = queryuservalue;
  query->querycallback = querycallback;
  query->resultcode = HTTP_RESULT_SUCCESS;
  memset( &query->response, 0, sizeof(httpResponse) );
  mmListDualAddLast( &http->querywaitlist, query, offsetof(httpQuery,list) );
  http->queryqueuecount++;

  /* Store querystring, keep along as we may need it to resend query */
  query->querylength = querylen;
  query->querystring = malloc( query->querylength );
  memcpy( query->querystring, querystring, query->querylength );

#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "TcpHttp: httpAddQuery() called, queue has %d queries\n", http->queryqueuecount );
#endif

  return 1;
}


static void httpCloseLink( httpConnection *http )
{
  tcpDataBuffer *recvbuf, *recvbufnext;

  DEBUG_SET_TRACKER();

  /* Discard pending buffers */
  for( recvbuf = http->recvbuflist.first ; recvbuf ; recvbuf = recvbufnext )
  {
    recvbufnext = recvbuf->userlist.next;
    mmListDualRemove( &http->recvbuflist, recvbuf, offsetof(tcpDataBuffer,userlist) );
    tcpFreeRecvBuffer( http->tcp, http->link, recvbuf );
  }
  /* Disconnect */
  if( http->link )
  {
    tcpClose( http->tcp, http->link );
    http->link = 0;
  }
  http->status = HTTP_CONNECTION_STATUS_CLOSED;
  if( !( http->querysentlist.first ) && !( http->querywaitlist.first ) )
    http->retryfailurecount = 0;

#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "TcpHttp: httpCloseLink() called, query lists : %p %p\n", http->querysentlist.first, http->querywaitlist.first );
#endif

  return;
}


static void httpFreeQuery( httpConnection *http, httpQuery *query )
{
  DEBUG_SET_TRACKER();

  http->queryqueuecount--;
  if( query->querystring )
    free( query->querystring );
  if( query->data )
    free( query->data );
  free( query );
  return;
}

static void httpFinishFreeQuery( httpConnection *http, httpQuery *query )
{
  DEBUG_SET_TRACKER();

#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "TcpHttp: httpFinishFreeQuery() called, status : %d %d\n", query->status, query->response.httpcode );
#endif

  if( ( query->status == HTTP_QUERY_STATUS_FAILED ) || ( query->status == HTTP_QUERY_STATUS_ERROR ) )
    query->querycallback( query->uservalue, query->resultcode, 0 );
  else
  {
    query->response.body = ADDRESS( query->data, query->response.headerlength );
    query->response.bodysize = query->dataoffset - query->response.headerlength;
#if HTTP_APPEND_ZERO_BYTE
    ((char *)query->response.body)[ query->response.bodysize ] = 0;
#endif
    query->querycallback( query->uservalue, HTTP_RESULT_SUCCESS, &query->response );
    /* Reset count of retry failures */
    http->retryfailurecount = 0;
  }
  /* Remove from list and free */
  mmListDualRemove( ( query->flags & HTTP_QUERY_FLAGS_SENT ? &http->querysentlist : &http->querywaitlist ), query, offsetof(httpQuery,list) );
  httpFreeQuery( http, query );
  return;
}

/* Fail query and all that follows in list, all must belong to specified querylist */
static void httpFailQueryList( httpConnection *http, httpQuery *query, mmListDualHead *querylist, int resultcode )
{
  httpQuery *querynext;

  DEBUG_SET_TRACKER();

  for( ; query ; query = querynext )
  {
    querynext = query->list.next;
    /* Notify user that query failed */
    query->status = HTTP_QUERY_STATUS_FAILED;
    query->resultcode = resultcode;
    /* If we would like to try again, but the query was flagged to disallow it */
    if( ( resultcode == HTTP_RESULT_TRYAGAIN_ERROR ) && !( query->flags & HTTP_QUERY_FLAGS_RETRY ) )
      query->resultcode = HTTP_RESULT_NOREPLY_ERROR;

#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "TcpHttp : Fail query %p, flags 0x%x\n", query, query->flags );
#endif

    httpFinishFreeQuery( http, query );
  }
  return;
}


void httpClose( httpConnection *http )
{
  tcpDataBuffer *recvbuf, *recvbufnext;

  DEBUG_SET_TRACKER();

  /* Fail all pending queries */
  httpFailQueryList( http, http->querywaitlist.first, &http->querywaitlist, HTTP_RESULT_CONNECT_ERROR );
  httpFailQueryList( http, http->querysentlist.first, &http->querysentlist, HTTP_RESULT_CONNECT_ERROR );
  for( recvbuf = http->recvbuflist.first ; recvbuf ; recvbuf = recvbufnext )
  {
    recvbufnext = recvbuf->userlist.next;
    mmListDualRemove( &http->recvbuflist, recvbuf, offsetof(tcpDataBuffer,userlist) );
    tcpFreeRecvBuffer( http->tcp, http->link, recvbuf );
  }

#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "==== HTTP GLOBAL CLOSE ====\n" );
#endif

  httpCloseLink( http );

  free( http->address );
  free( http );
  return;
}


void httpSetTimeout( httpConnection *http, int idletimeout, int waitingtimeout )
{
  http->idletimeout = idletimeout;
  http->waitingtimeout = waitingtimeout;
  if( http->link )
    tcpSetTimeout( http->tcp, http->link, ( http->flags & HTTP_CONNECTION_FLAGS_IDLE ? http->idletimeout : http->waitingtimeout ) );
  return;
}


////////////////////////////////////////////////////////////////////////////////



static size_t httpFindHeaderLength( char *data, size_t datasize )
{
  size_t headerlength;
  char *body;

  DEBUG_SET_TRACKER();

  body = ccSeqFindStrSkip( data, datasize, "\r\n\r\n" );
  if( !( body ) )
  {
    body = ccSeqFindStrSkip( data, datasize, "\n\n" );
    if( !( body ) )
      return 0;
  }
  headerlength = (int)( body - data );

  return headerlength;
}


static char *httpParseHeaderLine( httpResponse *response, char *headerline, int headerlength )
{
  int linelength;
  int32_t readint32;
  char *string;

  linelength = ccSeqFindChar( headerline, headerlength, '\n' );
  if( linelength < 0 )
    return 0;
  headerline[linelength] = 0;
  if( ( linelength > 0 ) && ( headerline[linelength-1] == '\r' ) )
    headerline[linelength-1] = 0;
#if TCPHTTP_DEBUG_HEADERS
  TCPHTTP_DEBUG_PRINTF( "PARSE HEADER: \"%s\"\n", headerline );
#endif

  if( ( string = ccStrCmpWordIgnoreCase( headerline, "connection: " ) ) )
  {
    if( ccStrWordCmpWord( string, "close" ) )
      response->keepaliveflag = 0;
    else if( ccStrWordCmpWord( string, "keep-alive" ) )
      response->keepaliveflag = 1;
  }
  else if( ( string = ccStrCmpWordIgnoreCase( headerline, "keep-alive: " ) ) )
  {
    if( linelength > 0 )
    {
      string = ccSeqFindStrIgnoreCaseSkip( string, linelength, "max=" );
      if( string )
      {
        if( ccStrParseInt32( string, &readint32 ) )
          response->keepalivemax = readint32;
      }
    }
  }
  else if( ( string = ccStrCmpWordIgnoreCase( headerline, "transfer-encoding: chunked" ) ) )
    response->chunkedflag = 1;
  else if( ( string = ccStrCmpWordIgnoreCase( headerline, "content-length:" ) ) )
  {
    string = ccStrNextWord( string );
    if( ccStrParseInt32( string, &readint32 ) )
    {
      if( readint32 >= 0 )
        response->contentlength = readint32;
    }
  }
  else if( ( string = ccStrCmpWordIgnoreCase( headerline, "trailer:" ) ) )
    response->trailerflag = 1;
  else if( ( string = ccStrCmpWordIgnoreCase( headerline, "location:" ) ) )
    response->location = string;

  return &headerline[linelength+1];
}


#define HTTP_FIRST_LINE_MIN (8)
#define HTTP_FIRST_LINE_MAX (128)

/* Parse HTTP header, just the parts we care about anyway */
/* Return 1 on success, return 0 for try again with more data, return -1 on error */
static int httpParseHeader( httpResponse *response, char *data, size_t datasize )
{
  int httpmajor, httpminor, linelength, headerlength;
  char *header, *body, *string;
  char *headerline;

  DEBUG_SET_TRACKER();

  header = data;
  headerlength = httpFindHeaderLength( data, datasize );
  if( !( headerlength ) )
    return 0;
  body = &data[ headerlength ];
  data[ headerlength -1 ] = 0;

  linelength = ccSeqFindChar( header, headerlength, '\n' );
  if( linelength >= (HTTP_FIRST_LINE_MAX-1) )
    return -1;
  if( linelength < HTTP_FIRST_LINE_MIN )
    return -1;
  string = ccStrCmpSeqIgnoreCase( header, "HTTP/", 5 );
  if( !( string ) )
    return -1;
  if( sscanf( string, "%d.%d %d", &httpmajor, &httpminor, &response->httpcode ) != 3 )
    return -1;

  response->httpversion = ( httpmajor << 8 ) + httpminor;
  response->header = header;
  response->headerlength = headerlength;
  response->body = body;
  response->bodysize = -1;
  response->keepaliveflag = 0;
  response->keepalivemax = 1048576;
  if( response->httpversion >= ( ( 1 << 8 ) + 1 ) )
    response->keepaliveflag = 1;
  response->chunkedflag = 0;
  response->trailerflag = 0;
  response->contentlength = -1;
  response->location = 0;

  headerline = &header[linelength+1];
  for( ; headerline ; )
    headerline = httpParseHeaderLine( response, headerline, (int)( body - headerline ) );

  if( ( response->contentlength == -1 ) && ( response->keepaliveflag ) && !( response->chunkedflag ) )
    return -1;

#if TCPHTTP_DEBUG_HEADERS
  TCPHTTP_DEBUG_PRINTF( "HTTP: httpParseHeader Success Summary\n" );
  TCPHTTP_DEBUG_PRINTF( "  Code : %d\n", response->httpcode );
  TCPHTTP_DEBUG_PRINTF( "  Keep-Alive : %d\n", response->keepaliveflag );
  TCPHTTP_DEBUG_PRINTF( "  Content-Length : %d\n", response->contentlength );
#endif

  return 1;
}


static ssize_t httpParseReadChunkSize( char *chunkdata )
{
  char c;
  ssize_t chunksize;

  DEBUG_SET_TRACKER();

  chunksize = 0;
  for( ; ; chunkdata++ )
  {
    c = *chunkdata;
    chunksize <<= 4;
    if( ( c >= 'a' ) && ( c <= 'f' ) )
      chunksize += ( c - ('a'-10) );
    else if( ( c >= 'A' ) && ( c <= 'F' ) )
      chunksize += ( c - ('A'-10) );
    else if( ( c >= '0' ) && ( c <= '9' ) )
      chunksize += ( c - '0' );
    else if( ( c == ';' ) || ( c == ' ' ) || ( c == '\r' ) || ( c == '\n' ) )
      break;
    else
      return -1;
  }
  chunksize >>= 4;

#if TCPHTTP_DEBUG_CHUNK
  TCPHTTP_DEBUG_PRINTF( "TcpHttp Chunk : Read Chunk Size as %d bytes\n", (int)chunksize );
#endif

  return chunksize;
}

static int httpParseTrailerEnd( httpQuery *query, char *str, size_t length )
{
  int retval;

  DEBUG_SET_TRACKER();

  retval = 0;
  if( ( length == 1 ) && ( str[0] == '\n' ) )
    retval = 1;
  else if( ( length == 2 ) && ( str[0] == '\r' ) && ( str[1] == '\n' ) )
    retval = 1;
  else
  {
    if( ccSeqFindStrSkip( str, length, "\r\n\r\n" ) )
      retval = 1;
    if( ccSeqFindStrSkip( str, length, "\n\n" ) )
      retval = 1;
  }
  return retval;
}

/* Specifies minimum total buffer size, may allocate more */
static inline int httpAllocData( httpQuery *query, size_t datasize )
{
  DEBUG_SET_TRACKER();

#if HTTP_APPEND_ZERO_BYTE
  if( datasize >= query->dataalloc )
  {
    query->dataalloc = intMax( 4096, intMax( datasize + 1, query->dataalloc << 1 ) );
#else
  if( datasize > query->dataalloc )
  {
    query->dataalloc = intMax( 4096, intMax( datasize, query->dataalloc << 1 ) );
#endif
    query->data = realloc( query->data, query->dataalloc );
    if( !( query->data ) )
      return 0;
  }
  return 1;
}

static inline int httpFindCharSkipClamp( char *seq, int seqlen, char c, int *retfoundflag )
{
  int i;

  DEBUG_SET_TRACKER();

  if( retfoundflag )
    *retfoundflag = 1;
  for( i = 0 ; i < seqlen ; i++ )
  {
    if( seq[i] == c )
      return i+1;
  }
  if( retfoundflag )
    *retfoundflag = 0;
  return seqlen;
}

static int httpParseRecvChunk( httpQuery *query, void **retbufdata, size_t *retbufsize )
{
  int chunkflag;
  size_t bufsize, trailsize;
  ssize_t chunksize, copysize;
  void *bufdata;

  DEBUG_SET_TRACKER();

  bufsize = *retbufsize;
  bufdata = *retbufdata;

  for( ; bufsize ; )
  {
#if TCPHTTP_DEBUG && 0
    TCPHTTP_DEBUG_PRINTF( "============== Chunk Buffer ( size %d )\n", (int)bufsize );
    TCPHTTP_DEBUG_PRINTF( "%.*s\n", (int)bufsize, (char *)bufdata );
    TCPHTTP_DEBUG_PRINTF( "============== Chunk Buffer\n" );
#endif

    if( query->status == HTTP_QUERY_STATUS_SKIPTRAILER )
    {
      /* Find an empty new line */
      copysize = httpFindCharSkipClamp( (char *)bufdata, bufsize, '\n', 0 );
      httpAllocData( query, query->chunkoffset + copysize );
      memcpy( ADDRESS( query->data, query->chunkoffset ), bufdata, copysize );
      query->chunkoffset += copysize;
      bufdata = ADDRESS( bufdata, copysize );
      bufsize -= copysize;
      /* When we find the end of the trailer, mark query as complete and preserve rest of buffer */
      trailsize = query->chunkoffset - query->dataoffset;
      if( trailsize >= 65536 )
      {
        TCPHTTP_DEBUG_PRINTF( "HTTP ERROR: Trailer way too long.\n" );
        query->status = HTTP_QUERY_STATUS_ERROR;
        query->resultcode = HTTP_RESULT_BADFORMAT_ERROR;
        return 0;
      }
#if TCPHTTP_DEBUG_CHUNK
      TCPHTTP_DEBUG_PRINTF( "#### Chunk Trailer %d bytes : \"%.*s\"\n", (int)trailsize, (int)trailsize, (char *)ADDRESS( query->data, query->dataoffset ) );
#endif
      if( httpParseTrailerEnd( query, ADDRESS( query->data, query->dataoffset ), trailsize ) )
      {
#if TCPHTTP_DEBUG_CHUNK
        TCPHTTP_DEBUG_PRINTF( "#### Chunk Trailer Finished\n" );
#endif
        query->status = HTTP_QUERY_STATUS_COMPLETE;
        query->resultcode = HTTP_RESULT_SUCCESS;
        break;
      }
      continue;
    }

    if( !( query->chunksize ) )
    {
      /* Do we need to skip a line after the previous chunk? */
      if( query->status == HTTP_QUERY_STATUS_SKIPLINE )
      {
        copysize = httpFindCharSkipClamp( (char *)bufdata, bufsize, '\n', &chunkflag );
        bufdata = ADDRESS( bufdata, copysize );
        bufsize -= copysize;
        if( !( chunkflag ) )
          break;
#if TCPHTTP_DEBUG_CHUNK
        TCPHTTP_DEBUG_PRINTF( "#### Skipped Line\n" );
#endif
        query->status = HTTP_QUERY_STATUS_WAITCONTENT;
      }
      /* Find '\n' in recvbuf, parse "DEADbeef; ignore this\r\n" */
      copysize = httpFindCharSkipClamp( (char *)bufdata, bufsize, '\n', &chunkflag );
      httpAllocData( query, query->chunkoffset + copysize );
      memcpy( ADDRESS( query->data, query->chunkoffset ), bufdata, copysize );
      query->chunkoffset += copysize;
      bufdata = ADDRESS( bufdata, copysize );
      bufsize -= copysize;
      if( !( chunkflag ) )
        break;
      chunksize = httpParseReadChunkSize( ADDRESS( query->data, query->dataoffset ) );
      if( chunksize < 0 )
        return 0;
      query->chunksize = chunksize;
      query->chunkoffset = query->dataoffset + chunksize;
      if( !( chunksize ) )
      {
#if TCPHTTP_DEBUG_CHUNK
        TCPHTTP_DEBUG_PRINTF( "#### Last Chunk Found\n" );
#endif
        /* Last chunk, handle trailer, next query */
        query->status = HTTP_QUERY_STATUS_SKIPTRAILER;
        query->chunkoffset = query->dataoffset;
        continue;
      }
    }

    copysize = query->chunksize;
    if( copysize > bufsize )
      copysize = bufsize;

    httpAllocData( query, query->dataoffset + copysize );
    memcpy( ADDRESS( query->data, query->dataoffset ), bufdata, copysize );

#if TCPHTTP_DEBUG_CHUNK && 0
    TCPHTTP_DEBUG_PRINTF( "============== Chunk Start\n" );
    TCPHTTP_DEBUG_PRINTF( "%.*s\n", (int)copysize, (char *)bufdata );
    TCPHTTP_DEBUG_PRINTF( "============== Chunk End\n" );
#endif

    query->dataoffset += copysize;
    bufdata = ADDRESS( bufdata, copysize );
    bufsize -= copysize;
    query->chunksize -= copysize;
    if( !( query->chunksize ) )
    {
      query->status = HTTP_QUERY_STATUS_SKIPLINE;
      query->chunkoffset = query->dataoffset;
    }
  }

  /* Either buffer or query exhausted */
  *retbufsize = bufsize;
  *retbufdata = bufdata;
  return 1;
}

static int httpParseRecvData( httpConnection *http )
{
  int parsecode;
  size_t bufsize, copysize, headerbreak;
  httpQuery *query;
  void *bufdata;
  tcpDataBuffer *recvbuf, *recvbufnext;

  DEBUG_SET_TRACKER();

  /* TODO: Why do we refuse to process already received buffers when the connection is closing? */
  if( http->status == HTTP_CONNECTION_STATUS_CLOSING )
    return 1;

  /* Loop through all buffers */
  for( recvbuf = http->recvbuflist.first ; recvbuf ; recvbuf = recvbufnext )
  {
    recvbufnext = recvbuf->userlist.next;
    bufdata = recvbuf->pointer;
    bufsize = recvbuf->size;

#if TCPHTTP_DEBUG
    TCPHTTP_DEBUG_PRINTF( "TcpHttp: Read buffer of %d bytes\n", (int)bufsize );
#endif

#if 0
    TCPHTTP_DEBUG_PRINTF( "=========== RECV BEGIN ===========\n" );
    TCPHTTP_DEBUG_PRINTF( "%.*s\n", (int)bufsize, (char *)bufdata );
    TCPHTTP_DEBUG_PRINTF( "=========== RECV END ===========\n" );
#endif

    /* A single buffer can overlap multiple queries */
    for( ; bufsize ; )
    {
      query = http->querysentlist.first;
      if( !( query ) )
      {
        TCPHTTP_DEBUG_PRINTF( "HTTP ERROR: We have no pending query and the server is sending us data.\n" );
        return 0;
      }

      /* If we haven't started the content yet, get HTTP header */
      if( query->status == HTTP_QUERY_STATUS_WAITHEADER )
      {
        /* Queue stuff in our buffer */
        headerbreak = httpFindHeaderLength( bufdata, bufsize );
        if( !( headerbreak ) )
          headerbreak = bufsize;
        /* Careful about infinitely long headers */
        if( ( query->dataoffset + headerbreak ) >= 65536 )
        {
          TCPHTTP_DEBUG_PRINTF( "HTTP ERROR: Header way too long.\n" );
          query->status = HTTP_QUERY_STATUS_ERROR;
          query->resultcode = HTTP_RESULT_BADFORMAT_ERROR;
          return 0;
        }
        httpAllocData( query, query->dataoffset + headerbreak + 4096 );
        memcpy( ADDRESS( query->data, query->dataoffset ), bufdata, headerbreak );
        query->dataoffset += headerbreak;

        /* Attempt to parse header */
        parsecode = httpParseHeader( &query->response, query->data, query->dataoffset );
        if( parsecode == -1 )
        {
          TCPHTTP_DEBUG_PRINTF( "HTTP ERROR: Parse error in HTTP header sent by server.\n" );
          query->status = HTTP_QUERY_STATUS_ERROR;
          query->resultcode = HTTP_RESULT_BADFORMAT_ERROR;
          httpFinishFreeQuery( http, query );
          return 0;
        }
        else if( parsecode == 0 )
        {
          /* Not enough data to read header yet */
          continue;
        }

        /* Header has been read */
        if( query->response.chunkedflag )
        {
          query->flags |= HTTP_QUERY_FLAGS_CHUNKED;
          query->response.contentlength = 0;
          query->chunkoffset = query->response.headerlength;
          query->chunksize = 0;
        }
        else
        {
          if( query->response.contentlength < 0 )
            query->flags |= HTTP_QUERY_FLAGS_NOCONTENTLENGTH;
          else if( query->response.contentlength >= (1024*1048576) )
          {
            TCPHTTP_DEBUG_PRINTF( "HTTP ERROR: Content length way too long.\n" );
            query->status = HTTP_QUERY_STATUS_ERROR;
            query->resultcode = HTTP_RESULT_BADFORMAT_ERROR;
            return 0;
          }
          query->response.contentlength = query->response.contentlength;
        }
        query->status = HTTP_QUERY_STATUS_WAITCONTENT;

        /* Update keep-alive settings */
        if( query->response.keepaliveflag )
        {
          http->serverflags |= HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING;
          http->keepalivemax = query->pipelineindex + query->response.keepalivemax;
          if( http->keepalivemax < 1 )
            http->keepalivemax = 1;
          if( http->keepalivemax > HTTP_KEEP_ALIVE_MAX_COUNT )
            http->keepalivemax = HTTP_KEEP_ALIVE_MAX_COUNT;
        }
        else
        {
          http->serverflags &= ~( HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING );
          http->keepalivemax = 1;
        }
        http->serverflags &= http->flags;

        /* Allocate content buffer */
        httpAllocData( query, query->response.headerlength + ( ( query->flags & ( HTTP_QUERY_FLAGS_NOCONTENTLENGTH | HTTP_QUERY_FLAGS_CHUNKED ) ) ? 1048576 : query->response.contentlength ) );
        query->dataoffset = query->response.headerlength;
        /* Skip header to get remaining data */
        bufdata = ADDRESS( bufdata, query->response.headerlength );
        bufsize -= query->response.headerlength;
      }

      /* Prepare to copy memory to buffer */
      if( query->flags & HTTP_QUERY_FLAGS_CHUNKED )
      {
        if( !( httpParseRecvChunk( query, &bufdata, &bufsize ) ) )
        {
          TCPHTTP_DEBUG_PRINTF( "HTTP ERROR: Failed to parse chunked transfer-encoding reply.\n" );
          query->status = HTTP_QUERY_STATUS_ERROR;
          query->resultcode = HTTP_RESULT_BADFORMAT_ERROR;
          return 0;
        }
      }
      else
      {
        if( query->flags & HTTP_QUERY_FLAGS_NOCONTENTLENGTH )
        {
          httpAllocData( query, query->dataoffset + bufsize );
          copysize = bufsize;
        }
        else
        {
          copysize = ( query->response.contentlength + query->response.headerlength ) - query->dataoffset;
          if( copysize > bufsize )
            copysize = bufsize;
        }
        /* Copy buffer to content */
        memcpy( ADDRESS( query->data, query->dataoffset ), bufdata, copysize );
        query->dataoffset += copysize;
        bufdata = ADDRESS( bufdata, copysize );
        bufsize -= copysize;
        if( !( query->flags & HTTP_QUERY_FLAGS_NOCONTENTLENGTH ) )
        {
#if TCPHTTP_DEBUG
          TCPHTTP_DEBUG_PRINTF( "TcpHttp : Is query %p complete? Received %d == Total %d ( %d + %d )\n", query, (int)query->dataoffset, (int)query->response.contentlength + (int)query->response.headerlength, (int)query->response.contentlength, (int)query->response.headerlength );
#endif
          if( query->dataoffset == ( query->response.contentlength + query->response.headerlength ) )
          {
            query->status = HTTP_QUERY_STATUS_COMPLETE;
            query->resultcode = HTTP_RESULT_SUCCESS;
          }
        }
      }

      /* Is our query complete? */
      if( query->status == HTTP_QUERY_STATUS_COMPLETE )
      {
#if TCPHTTP_DEBUG
        TCPHTTP_DEBUG_PRINTF( "TcpHttp : Query %p complete, %d bytes\n", query, (int)( query->dataoffset - query->response.headerlength ) );
        TCPHTTP_DEBUG_PRINTF( "==== Query Headers ====\n%.*s\n", (int)query->response.headerlength, (char *)query->data );
#endif
        httpFinishFreeQuery( http, query );

#if TCPHTTP_DEBUG
        TCPHTTP_DEBUG_PRINTF( "TcpHttp : Callback dispatched, %d queries pending ( first %p )\n", http->queryqueuecount, http->querysentlist.first );
#endif
        /* If we have no pending sent queries, clear flush status */
        if( ( http->status == HTTP_CONNECTION_STATUS_FLUSH ) && !( http->querysentlist.first ) )
          http->status = HTTP_CONNECTION_STATUS_READY;

        if( !( http->serverflags & HTTP_CONNECTION_FLAGS_KEEPALIVE ) && ( http->sentquerycount ) )
        {
          /* Connection is not keep-alive, and hasn't yet reconnected : close it and reconnect */
          if( http->status != HTTP_CONNECTION_STATUS_CLOSED )
            http->status = HTTP_CONNECTION_STATUS_CLOSING;
        }
        else if( http->sentquerycount >= http->keepalivemax )
        {
          /* Handle keep-alive max parameter, is it time to close the connection? */
          if( !( http->querysentlist.first ) && ( http->status != HTTP_CONNECTION_STATUS_CLOSED ) )
            http->status = HTTP_CONNECTION_STATUS_CLOSING;
        }
      }
    }

    /* Free TCP buffer */
    mmListDualRemove( &http->recvbuflist, recvbuf, offsetof(tcpDataBuffer,userlist) );
    tcpFreeRecvBuffer( http->tcp, http->link, recvbuf );
  }

  return 1;
}


/* Send some queued queries */
static void httpSendQueries( httpConnection *http )
{
  httpQuery *query, *querynext;
  tcpDataBuffer *tcpbuffer;

  DEBUG_SET_TRACKER();

  /* Process waiting queries */
  for( query = http->querywaitlist.first ; query ; query = querynext )
  {
    /* Does the connection have its buffers full, or is not otherwise ready? */
    if( http->status != HTTP_CONNECTION_STATUS_READY )
      break;
    if( http->sentquerycount >= http->keepalivemax )
      break;
    querynext = query->list.next;

    /* Pipelining index for query since last reconnect */
    query->pipelineindex = http->sentquerycount;
    /* Queue send query */
    tcpbuffer = tcpAllocSendBuffer( http->tcp, http->link, query->querylength );
    memcpy( tcpbuffer->pointer, query->querystring, query->querylength );
    tcpQueueSendBuffer( http->tcp, http->link, tcpbuffer, query->querylength );

#if TCPHTTP_DEBUG
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Query %p sent, %d bytes, pipelining index %d / %d\n", query, (int)query->querylength, query->pipelineindex, http->keepalivemax );
#endif
    /* Put query at the end of sentlist */
    mmListDualRemove( &http->querywaitlist, query, offsetof(httpQuery,list) );
    mmListDualAddLast( &http->querysentlist, query, offsetof(httpQuery,list) );
    query->flags |= HTTP_QUERY_FLAGS_SENT;
    /* Increment sent count since last reconnect */
    http->sentquerycount++;
    /* If we can't pipeline, set flush status and wait */
    if( !( query->flags & HTTP_QUERY_FLAGS_PIPELINING ) || !( http->serverflags & HTTP_CONNECTION_FLAGS_PIPELINING ) )
    {
      http->status = HTTP_CONNECTION_STATUS_FLUSH;
      break;
    }
  }

  /* Adjust link timeout if we are toggling idle state */
  switch( http->status )
  {
    case HTTP_CONNECTION_STATUS_READY:
    case HTTP_CONNECTION_STATUS_WAIT:
    case HTTP_CONNECTION_STATUS_FLUSH:
      if( http->querysentlist.first )
      {
        if( http->flags & HTTP_CONNECTION_FLAGS_IDLE )
        {
          http->flags &= ~HTTP_CONNECTION_FLAGS_IDLE;
          tcpSetTimeout( http->tcp, http->link, http->waitingtimeout );
        }
      }
      else
      {
        if( !( http->flags & HTTP_CONNECTION_FLAGS_IDLE ) )
        {
          http->flags |= HTTP_CONNECTION_FLAGS_IDLE;
          tcpSetTimeout( http->tcp, http->link, http->idletimeout );
        }
      }
      break;
    default:
      break;
  }

  return;
}


/* Attempt to reconnect, return zero is status remains nolink */
static int httpAttemptConnect( httpConnection *http )
{
  httpQuery *query, *querybreak, *querynext;
  void *repeatlist;

#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "TcpHttp : Connect, queries pending : %d\n", http->queryqueuecount );
#endif

  DEBUG_SET_TRACKER();

  http->sentquerycount = 0;

#if TCPHTTP_DEBUG
  if( http->retryfailurecount )
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Connect, Retry Failure Count : %d\n", http->retryfailurecount );
  for( query = http->querysentlist.first ; query ; query = query->list.next )
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Query Sent List : %p\n", query );
  for( query = http->querywaitlist.first ; query ; query = query->list.next )
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Query Wait List : %p\n", query );
#endif

  if( http->retryfailurecount >= HTTP_FAILED_RETRY_MAXIMUM )
    goto connecterror;

  /* Reconnect */
  http->link = tcpConnect( http->tcp, http->address, http->port, http, &httpConnectionIO, http->flags & HTTP_CONNECTION_FLAGS_SSL );
  if( http->link )
  {
    http->status = HTTP_CONNECTION_STATUS_CONNECTING;
    tcpSetTimeout( http->tcp, http->link, http->waitingtimeout );
#if TCPHTTP_DEBUG
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Connecting...\n" );
#endif
  }
  else
  {
    connecterror:
    /* Increment error count */
    http->errorcount++;
    http->retryfailurecount = 0;
    /* Fail all pending queries */
    httpFailQueryList( http, http->querywaitlist.first, &http->querywaitlist, HTTP_RESULT_CONNECT_ERROR );
    httpFailQueryList( http, http->querysentlist.first, &http->querysentlist, HTTP_RESULT_CONNECT_ERROR );
#if TCPHTTP_DEBUG
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Connect failed\n" );
#endif
    return 0;
  }

  /* Check all queries we sent and never received a reply for */
  repeatlist = 0;
  for( query = http->querysentlist.first ; query ; query = querynext )
  {
    querynext = query->list.next;
    /* If we tried too many times to reconnect, abort */
    if( http->retryfailurecount >= HTTP_FAILED_RETRY_MAXIMUM )
      break;
    /* If query doesn't allow retry, stop */
    if( !( query->flags & HTTP_QUERY_FLAGS_RETRY ) )
      break;
    /* If query was aborted, also stop right away */
    if( query->flags & HTTP_QUERY_FLAGS_ABORTED )
      break;

    /* Reset query */
    query->status = HTTP_QUERY_STATUS_WAITHEADER;
    query->dataalloc = 0;
    query->dataoffset = 0;
    if( query->data )
      free( query->data );
    query->data = 0;

#if TCPHTTP_DEBUG
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Queued for retry %p\n", query );
#endif
    /* Build a list of sent queries to try again */
    mmListDualRemove( &http->querysentlist, query, offsetof(httpQuery,list) );
    mmListAdd( &repeatlist, query, offsetof(httpQuery,list) );
  }
  querybreak = query;
  /* We need a temporary list in order to insert queries back in the proper order */
  for( query = repeatlist ; query ; query = querynext )
  {
    querynext = query->list.next;
    /* Repeat query by moving it back to http->querywaitlist */
    mmListRemove( query, offsetof(httpQuery,list) );
    mmListDualAddFirst( &http->querywaitlist, query, offsetof(httpQuery,list) );
  }

  if( querybreak )
  {
    /* The TRYAGAIN error are converted to NOREPLY errors if the queries don't allow resend */
    /* Fail all following queries in sentlist */
    httpFailQueryList( http, querybreak, &http->querysentlist, HTTP_RESULT_TRYAGAIN_ERROR );
    /* Also fail all waiting queries that follow */
    httpFailQueryList( http, http->querywaitlist.first, &http->querywaitlist, HTTP_RESULT_TRYAGAIN_ERROR );
  }

  return ( http->status == HTTP_CONNECTION_STATUS_CONNECTING ? 1 : 0 );
}


/* Update status of http connection and all its queries */
int httpProcess( httpConnection *http )
{
  httpQuery *query;

  DEBUG_SET_TRACKER();

  /* Parse buffered incoming data */
  if( !( httpParseRecvData( http ) ) )
  {
#if TCPHTTP_DEBUG
    TCPHTTP_DEBUG_PRINTF( "TcpHttp : Received data parse error.\n" );
#endif
    /* Some error occured! */
    http->errorcount++;
    /* We will free all queued recv buffers when handling the status CLOSING */
    if( http->status != HTTP_CONNECTION_STATUS_CLOSED )
      http->status = HTTP_CONNECTION_STATUS_CLOSING;
  }

  /* If connection was gracefully closed, finish any pending connection:close query */
  if( http->status == HTTP_CONNECTION_STATUS_CLOSED )
  {
    /* If we have a Connection:Close query with undefined contentlength, process it */
    query = http->querysentlist.first;
    if( query )
    {
      if( query->flags & HTTP_QUERY_FLAGS_NOCONTENTLENGTH )
      {
        query->status = HTTP_QUERY_STATUS_COMPLETE;
        query->resultcode = HTTP_RESULT_SUCCESS;
        httpFinishFreeQuery( http, query );
      }
      else
        http->errorcount++;
    }
  }

  /* Close link if scheduled for closing, due to timeout, error or any other reason */
  if( http->status == HTTP_CONNECTION_STATUS_CLOSING )
    httpCloseLink( http );

  /* Attempt to reconnect if the link is fully closed and we have new pending queries */
  if( http->status == HTTP_CONNECTION_STATUS_CLOSED )
  {
    /* If link is still active, terminate it */
    if( http->link )
      httpCloseLink( http );
    /* Do we want to connect back? */
    if( !( http->queryqueuecount ) )
      return 0;
    if( !( httpAttemptConnect( http ) ) )
      return 0;
  }

  /* Send buffered queries */
  if( http->status == HTTP_CONNECTION_STATUS_READY )
    httpSendQueries( http );

  return 1;
}


int httpGetQueryQueueCount( httpConnection *http )
{
  DEBUG_SET_TRACKER();

  return http->queryqueuecount;
}


void httpAbortQueue( httpConnection *http )
{
  httpQuery *query;

  DEBUG_SET_TRACKER();

  for( query = http->querysentlist.first ; query ; query = query->list.next )
    query->flags |= HTTP_QUERY_FLAGS_ABORTED;
  for( query = http->querywaitlist.first ; query ; query = query->list.next )
    query->flags |= HTTP_QUERY_FLAGS_ABORTED;
  return;
}


int httpGetClearErrorCount( httpConnection *http )
{
  int errorcount;
  errorcount = http->errorcount;
  http->errorcount = 0;
/*
  http->retryfailurecount = 0;
*/
#if TCPHTTP_DEBUG
  TCPHTTP_DEBUG_PRINTF( "TcpHttp: GetClearErrorCount() called, return %d errors.\n", errorcount );
#endif
  return errorcount;
}


int httpGetStatus( httpConnection *http )
{
  int retval;

  DEBUG_SET_TRACKER();

  switch( http->status )
  {
    case HTTP_CONNECTION_STATUS_READY:
    case HTTP_CONNECTION_STATUS_WAIT:
    case HTTP_CONNECTION_STATUS_FLUSH:
      retval = 1;
      break;
    case HTTP_CONNECTION_STATUS_CONNECTING:
    case HTTP_CONNECTION_STATUS_CLOSING:
    case HTTP_CONNECTION_STATUS_CLOSED:
    default:
      retval = 0;
      break;
  }
  return retval;
}


void httpSetWakeCallback( httpConnection *http, void (*wake)( tcpContext *tcp, httpConnection *http, void *wakecontext ), void *wakecontext )
{
  http->wake = wake;
  http->wakecontext = wakecontext;
  return;
}


////



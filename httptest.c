/* -----------------------------------------------------------------------------
 *
 * httptest: the back-off rules of tcphttp.c, offline.
 *
 * - which replies mean "slow down" (429, 502/503/504 and kin, a 5xx with Retry-After);
 * - Retry-After read as delta-seconds and as each HTTP date form;
 * - the wait: Retry-After honoured within limits, otherwise 10 s doubling up to 300 s;
 * - one wait per episode: replies arriving during a back-off do not stack more waits;
 * - the header parser fills response->retryafter.
 *
 * Same zlib license as the rest of BrickSync.
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
#include "tcp.h"
#include "tcphttp.h"


static int failures = 0;

static void check( int condition, const char *what )
{
  printf( "%s %s\n", ( condition ? "ok:  " : "FAIL:" ), what );
  if( !( condition ) )
    failures++;
  return;
}

static void checkint( int value, int expected, const char *what )
{
  char buffer[256];
  snprintf( buffer, sizeof(buffer), "%s (got %d, want %d)", what, value, expected );
  check( value == expected, buffer );
  return;
}


/* "Sun, 06 Nov 1994 08:49:37 GMT", the RFC's own example */
#define T_RFC_EXAMPLE ((time_t)784111777)

static void testRetryAfter( void )
{
  printf( "-- Retry-After values\n" );
  checkint( httpParseRetryAfter( "120", 0 ), 120, "delta-seconds" );
  checkint( httpParseRetryAfter( "  7  ", 0 ), 7, "delta-seconds with spaces" );
  checkint( httpParseRetryAfter( "0", 0 ), 0, "zero seconds" );
  checkint( httpParseRetryAfter( "", 0 ), -1, "empty value" );
  checkint( httpParseRetryAfter( "soon", 0 ), -1, "word" );
  checkint( httpParseRetryAfter( "12abc", 0 ), -1, "number with garbage" );
  checkint( httpParseRetryAfter( "-5", 0 ), -1, "negative" );
  checkint( httpParseRetryAfter( 0, 0 ), -1, "no value" );

  checkint( httpParseRetryAfter( "Sun, 06 Nov 1994 08:49:37 GMT", T_RFC_EXAMPLE - 90 ), 90, "IMF-fixdate, 90 s ahead" );
  checkint( httpParseRetryAfter( "Sunday, 06-Nov-94 08:49:37 GMT", T_RFC_EXAMPLE - 90 ), 90, "RFC 850 date, 90 s ahead" );
  checkint( httpParseRetryAfter( "Sun Nov  6 08:49:37 1994", T_RFC_EXAMPLE - 90 ), 90, "asctime date, 90 s ahead" );
  checkint( httpParseRetryAfter( "Sun, 06 Nov 1994 08:49:37 GMT", T_RFC_EXAMPLE + 500 ), 0, "date in the past: no wait" );
  checkint( httpParseRetryAfter( "Tue, 29 Feb 2028 12:00:00 GMT", (time_t)1835438400 - 3600 ), 3600, "leap day 2028" );
  checkint( httpParseRetryAfter( "Wed, 01 Mar 2000 00:00:00 GMT", (time_t)951868800 - 1 ), 1, "after the 2000 leap day" );
  checkint( httpParseRetryAfter( "Sun, 06 Foo 1994 08:49:37 GMT", T_RFC_EXAMPLE ), -1, "unknown month" );
  checkint( httpParseRetryAfter( "Sun, 06 Nov 1994 GMT", T_RFC_EXAMPLE ), -1, "date without time" );
  checkint( httpParseRetryAfter( "Sun, 36 Nov 1994 08:49:37 GMT", T_RFC_EXAMPLE ), -1, "day out of range" );
  checkint( httpParseRetryAfter( "Sun, 06 Nov 1994 28:49:37 GMT", T_RFC_EXAMPLE ), -1, "hour out of range" );
  return;
}


static void testBackoffReply( void )
{
  printf( "-- Which replies mean slow down\n" );
  check( httpIsBackoffReply( 429, -1 ), "429 Too Many Requests" );
  check( httpIsBackoffReply( 502, -1 ), "502 Bad Gateway" );
  check( httpIsBackoffReply( 503, -1 ), "503 Service Unavailable" );
  check( httpIsBackoffReply( 504, -1 ), "504 Gateway Timeout" );
  check( httpIsBackoffReply( 520, -1 ) && httpIsBackoffReply( 524, -1 ) && httpIsBackoffReply( 529, -1 ), "CDN 520-524, 529" );
  check( httpIsBackoffReply( 500, 30 ), "500 with Retry-After" );
  check( !( httpIsBackoffReply( 500, -1 ) ), "bare 500 stays an error" );
  check( !( httpIsBackoffReply( 501, -1 ) ), "501 stays an error" );
  check( !( httpIsBackoffReply( 200, -1 ) ), "200 is not" );
  check( !( httpIsBackoffReply( 404, -1 ) ), "404 is not" );
  check( !( httpIsBackoffReply( 400, 30 ) ), "400 with Retry-After is not" );
  return;
}


static void testDelay( void )
{
  printf( "-- How long to wait\n" );
  checkint( httpBackoffDelay( 1, -1 ), 10, "episode 1" );
  checkint( httpBackoffDelay( 2, -1 ), 20, "episode 2" );
  checkint( httpBackoffDelay( 3, -1 ), 40, "episode 3" );
  checkint( httpBackoffDelay( 4, -1 ), 80, "episode 4" );
  checkint( httpBackoffDelay( 5, -1 ), 160, "episode 5" );
  checkint( httpBackoffDelay( 6, -1 ), HTTP_BACKOFF_MAX, "episode 6 capped" );
  checkint( httpBackoffDelay( 60, -1 ), HTTP_BACKOFF_MAX, "episode 60 capped" );
  checkint( httpBackoffDelay( 1, 5 ), 5, "Retry-After honoured" );
  checkint( httpBackoffDelay( 4, 5 ), 5, "Retry-After honoured, any episode" );
  checkint( httpBackoffDelay( 1, 0 ), 1, "Retry-After 0: one second" );
  checkint( httpBackoffDelay( 1, 100000 ), HTTP_RETRYAFTER_MAX, "Retry-After capped" );
  return;
}


static void testEpisodes( void )
{
  httpConnection *http;

  printf( "-- Back-off episodes on a connection\n" );
  http = httpOpen( 0, "127.0.0.1", 443, 0 );
  checkint( (int)httpGetBackoffUntil( http ), 0, "new connection: no back-off" );
  checkint( httpGetBackoffCount( http ), 0, "new connection: no episode" );

  httpNoteReplyCode( http, 200, -1, 1000 );
  checkint( (int)httpGetBackoffUntil( http ), 0, "success: still none" );

  httpNoteReplyCode( http, 503, -1, 1000 );
  checkint( (int)httpGetBackoffUntil( http ), 1010, "503: wait 10 s" );
  checkint( httpGetBackoffCount( http ), 1, "503: episode 1" );
  checkint( httpGetBackoffCode( http ), 503, "503: code kept" );

  httpNoteReplyCode( http, 503, -1, 1001 );
  httpNoteReplyCode( http, 503, -1, 1002 );
  checkint( (int)httpGetBackoffUntil( http ), 1010, "more 503 in the same batch: same wait" );
  checkint( httpGetBackoffCount( http ), 1, "more 503 in the same batch: same episode" );

  httpNoteReplyCode( http, 429, 30, 1003 );
  checkint( (int)httpGetBackoffUntil( http ), 1033, "a longer Retry-After in the episode extends the wait" );
  httpNoteReplyCode( http, 429, 2, 1004 );
  checkint( (int)httpGetBackoffUntil( http ), 1033, "a shorter one does not shorten it" );

  httpNoteReplyCode( http, 503, -1, 1040 );
  checkint( httpGetBackoffCount( http ), 2, "503 after the wait: episode 2" );
  checkint( (int)httpGetBackoffUntil( http ), 1060, "episode 2: wait 20 s" );

  httpNoteReplyCode( http, 200, -1, 1061 );
  checkint( httpGetBackoffCount( http ), 0, "a success ends the run" );

  httpNoteReplyCode( http, 503, -1, 1070 );
  checkint( httpGetBackoffCount( http ), 1, "next 503: episode 1 again" );
  checkint( (int)httpGetBackoffUntil( http ), 1080, "episode 1 again: wait 10 s" );

  httpNoteReplyCode( http, 500, -1, 1090 );
  checkint( httpGetBackoffCount( http ), 0, "a bare 500 is not a back-off" );

  httpNoteReplyCode( http, 429, 45, 1100 );
  checkint( (int)httpGetBackoffUntil( http ), 1145, "429 with Retry-After 45" );
  checkint( httpGetBackoffCode( http ), 429, "429: code kept" );

  httpClose( http );
  return;
}


static void parseHeader( const char *text, httpResponse *response, int *retval )
{
  static char buffer[1024];
  memset( response, 0, sizeof(httpResponse) );
  strncpy( buffer, text, sizeof(buffer) - 1 );
  buffer[ sizeof(buffer) - 1 ] = 0;
  *retval = httpParseResponseHeader( response, buffer, strlen( buffer ) );
  return;
}

static void testHeader( void )
{
  httpResponse response;
  int retval;

  printf( "-- Reply header\n" );
  parseHeader( "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nRetry-After: 42\r\n\r\n", &response, &retval );
  checkint( retval, 1, "503 header parsed" );
  checkint( response.httpcode, 503, "503 code" );
  checkint( response.retryafter, 42, "Retry-After: 42" );

  parseHeader( "HTTP/1.1 429 Too Many Requests\r\ncontent-length: 0\r\nretry-after:7\r\n\r\n", &response, &retval );
  checkint( retval, 1, "429 header parsed" );
  checkint( response.retryafter, 7, "lower-case retry-after:7" );

  parseHeader( "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nRetry-After: Fri, 31 Dec 9999 23:59:59 GMT\r\n\r\n", &response, &retval );
  checkint( retval, 1, "header with a date parsed" );
  check( response.retryafter > 1000000, "Retry-After as a far date: a long wait (capped later)" );

  parseHeader( "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}", &response, &retval );
  checkint( retval, 1, "200 header parsed" );
  checkint( response.retryafter, -1, "no Retry-After: -1" );

  parseHeader( "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nRetry-After: whenever\r\n\r\n", &response, &retval );
  checkint( response.retryafter, -1, "unreadable Retry-After: -1" );
  return;
}


int main( void )
{
  testRetryAfter();
  testBackoffReply();
  testDelay();
  testEpisodes();
  testHeader();
  if( failures )
  {
    printf( "httptest: %d FAILED\n", failures );
    return 1;
  }
  printf( "httptest: all passed\n" );
  return 0;
}

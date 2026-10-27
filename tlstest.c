/* -----------------------------------------------------------------------------
 *
 * tlstest: checks that BrickSync's TLS layer (tcp.c) authenticates servers.
 *
 * Opens real TLS connections through tcpConnectSSLHost(), the same call BrickSync uses:
 * - the two marketplace APIs must connect (their certificates chain to a trusted root);
 * - servers with a broken certificate (wrong host, self-signed, expired, unknown root),
 *   and a marketplace address asked for under someone else's name, must be refused.
 * Exit code 0 when every case behaves, 1 otherwise. Built and run by build.sh test / CI.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "tcp.h"


typedef struct
{
  tcpContext *tcp;
  tcpLink *link;
  volatile int readyflag;
  volatile int closedflag;
} tlsProbe;

static void probeRecv( void *uservalue, tcpDataBuffer *recvbuf )
{
  tlsProbe *probe = uservalue;
  tcpFreeRecvBuffer( probe->tcp, probe->link, recvbuf );
  return;
}

static void probeSendReady( void *uservalue, size_t sendbuffered )
{
  tlsProbe *probe = uservalue;
  /* Only called once the TLS handshake, certificate check included, has completed */
  probe->readyflag = 1;
  return;
}

static void probeSendFinished( void *uservalue )
{
  return;
}

static void probeTimeout( void *uservalue )
{
  tlsProbe *probe = uservalue;
  tcpClose( probe->tcp, probe->link );
  return;
}

static void probeClosed( void *uservalue )
{
  tlsProbe *probe = uservalue;
  probe->closedflag = 1;
  return;
}

static tcpCallbackSet probeIO =
{
  .incoming = 0,
  .recv = probeRecv,
  .sendready = probeSendReady,
  .sendwait = 0,
  .sendfinished = probeSendFinished,
  .timeout = probeTimeout,
  .closed = probeClosed,
  .wake = 0
};


/* 1 = handshake completed, 0 = refused or failed, -1 = could not even start (DNS) */
static int probeServer( tcpContext *tcp, const char *connecthost, const char *checkname, const char **verifyerror )
{
  tlsProbe probe;
  char *address;
  int result;

  *verifyerror = 0;
  address = tcpResolveName( (char *)connecthost, 0 );
  if( !( address ) )
    return -1;
  memset( &probe, 0, sizeof(tlsProbe) );
  probe.tcp = tcp;
  probe.link = tcpConnectSSLHost( tcp, address, 443, &probe, &probeIO, 1, checkname );
  free( address );
  if( !( probe.link ) )
    return 0;
  tcpSetTimeout( tcp, probe.link, 15000 );
  for( ; !( probe.readyflag ) && !( probe.closedflag ) ; )
    tcpWait( tcp, 1000 );
  result = probe.readyflag;
  if( !( probe.closedflag ) )
    tcpClose( tcp, probe.link );
  *verifyerror = tcpGetClearSslVerifyError( tcp );
  return result;
}


typedef struct
{
  const char *connecthost;
  const char *checkname;
  int mustconnect;
  /* A refusal must come from the certificate check itself (not just any network failure) */
  int mustbeverifyerror;
} tlsCase;

static const tlsCase tlscases[] =
{
  { "api.brickowl.com", "api.brickowl.com", 1, 0 },
  { "api.bricklink.com", "api.bricklink.com", 1, 0 },
  { "www.bricklink.com", "www.bricklink.com", 1, 0 },
  /* Brick Owl's address asked for as another site: the certificate does not name it */
  { "api.brickowl.com", "www.example.com", 0, 1 },
  { "api.bricklink.com", "api.brickowl.com", 0, 1 },
  { "wrong.host.badssl.com", "wrong.host.badssl.com", 0, 1 },
  { "self-signed.badssl.com", "self-signed.badssl.com", 0, 1 },
  { "expired.badssl.com", "expired.badssl.com", 0, 1 },
  { "untrusted-root.badssl.com", "untrusted-root.badssl.com", 0, 1 },
};


int main( int argc, char **argv )
{
  tcpContext tcp;
  int index, result, failures;
  const char *verifyerror;
  const tlsCase *tc;

  if( !( tcpInit( &tcp, 1, 1 ) ) )
  {
    printf( "FAIL: could not initialize TCP/TLS\n" );
    return 1;
  }
  failures = 0;
  for( index = 0 ; index < (int)( sizeof(tlscases) / sizeof(tlsCase) ) ; index++ )
  {
    tc = &tlscases[index];
    result = probeServer( &tcp, tc->connecthost, tc->checkname, &verifyerror );
    if( result < 0 )
    {
      /* Without DNS there is nothing to learn; a missing marketplace still fails the run */
      printf( "%s: %s as %s (could not resolve)\n", ( tc->mustconnect ? "FAIL" : "SKIP" ), tc->connecthost, tc->checkname );
      failures += tc->mustconnect;
      continue;
    }
    if( tc->mustconnect )
    {
      if( result )
        printf( "ok:   %s connects\n", tc->checkname );
      else
      {
        printf( "FAIL: %s refused (%s)\n", tc->checkname, ( verifyerror ? verifyerror : "no certificate error recorded" ) );
        failures++;
      }
    }
    else if( result )
    {
      printf( "FAIL: %s as %s was ACCEPTED\n", tc->connecthost, tc->checkname );
      failures++;
    }
    else if( tc->mustbeverifyerror && !( verifyerror ) )
    {
      printf( "FAIL: %s as %s refused without a certificate error (network failure?)\n", tc->connecthost, tc->checkname );
      failures++;
    }
    else
      printf( "ok:   %s as %s refused (%s)\n", tc->connecthost, tc->checkname, verifyerror );
  }
  tcpEnd( &tcp );
  printf( "%s\n", ( failures ? "TLS TEST FAILED" : "TLS TEST PASSED" ) );
  return ( failures ? 1 : 0 );
}

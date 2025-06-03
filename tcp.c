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


#if CC_WINDOWS
 #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0501
 #endif
 #include <winsock2.h>
 #include <wchar.h>
 #include <windows.h>
 #include <ws2tcpip.h>
 #include <windowsx.h>
 #ifndef INET_ADDRSTRLEN
  #define INET_ADDRSTRLEN 16
 #endif
 typedef uint32_t in_addr_t;
#else
 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <sys/select.h>
 #include <netinet/in.h>
 #include <netinet/tcp.h>
 #include <arpa/inet.h>
 #include <netdb.h>
 #include <errno.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <pthread.h>
 #include <signal.h>
#endif


#include "tcp.h"


#define TCP_ENABLE_SSL_SUPPORT (1)

#define TCP_DEBUG (0)
#define TCP_DEBUG_EVENTS (0)
#define TCP_DEBUG_PRINT_ERRORS (0)

#if 1
 #define TCP_DEBUG_PRINTF(...) ccDebugLog( "debug-tcp.txt", __VA_ARGS__ )
#else
 #define TCP_DEBUG_PRINTF(...) printf( __VA_ARGS__ )
#endif


////


#if TCP_ENABLE_SSL_SUPPORT
 #include <openssl/ssl.h>
 #include <openssl/err.h>
#endif


#define TCP_BUFFER_DEFAULT_SIZE (262144)
#define TCP_BUFFER_CHUNK_COUNT (16)
#define TCP_BUFFER_SEND_READY_SIZE_TRESHOLD (262144)

#define TCPLINK_FLAGS_LISTEN (0x1)
#define TCPLINK_FLAGS_WANT_RECV (0x2)
#define TCPLINK_FLAGS_WANT_SEND (0x4)
#define TCPLINK_FLAGS_CLOSING (0x8)
/* User called tcpClose() on socket, we want to terminate the link */
#define TCPLINK_FLAGS_TERMINATED (0x10)
#define TCPLINK_FLAGS_TERMINATELIST (0x20)

#define TCPLINK_FLAGS_EVENT_INCOMING (0x40)
#define TCPLINK_FLAGS_EVENT_RECV (0x80)
#define TCPLINK_FLAGS_EVENT_SENDREADY (0x100)
#define TCPLINK_FLAGS_EVENT_SENDFINISHED (0x200)
#define TCPLINK_FLAGS_EVENT_TIMEOUT (0x400)
#define TCPLINK_FLAGS_EVENT_CLOSED (0x800)
#define TCPLINK_FLAGS_EVENT_MASK (TCPLINK_FLAGS_EVENT_INCOMING|TCPLINK_FLAGS_EVENT_RECV|TCPLINK_FLAGS_EVENT_SENDREADY|TCPLINK_FLAGS_EVENT_SENDFINISHED|TCPLINK_FLAGS_EVENT_TIMEOUT|TCPLINK_FLAGS_EVENT_CLOSED)

#define TCPLINK_FLAGS_SSL_NEEDCONNECT (0x1000)
#define TCPLINK_FLAGS_SSL_NEEDACCEPT (0x2000)
#define TCPLINK_FLAGS_SSL_ACTIVE (0x4000)
#define TCPLINK_FLAGS_SSL_LISTEN (0x8000)

#if CC_WINDOWS
 /* A low number is required on Windows since tcpWake does *not* work! */
 #define TCP_DEFAULT_SELECT_TIMEOUT (500)
#else
 #define TCP_DEFAULT_SELECT_TIMEOUT (2000)
#endif

#define TCP_DEFAULT_LINK_TIMEOUT (30*1000)

#define TCP_DEFAULT_CLOSING_TIMEOUT (5000)

static void *tcpThreadWork( void *p );


#define TCP_THREAD_STATE_NONE (0x0)
#define TCP_THREAD_STATE_NORMAL (0x1)
#define TCP_THREAD_STATE_LOCKED (0x2)
#define TCP_THREAD_STATE_MASK_ACTIVE (TCP_THREAD_STATE_NORMAL|TCP_THREAD_STATE_LOCKED)


typedef struct
{
  /* User-friendly public buffer */
  tcpDataBuffer publicbuffer;

  /* Size of data[] */
  size_t bufsize;
  /* If this is a send buffer, data size to send */
  size_t sendsize;
  /* How far are we in the buffer for send()/recv() */
  size_t rwoffset;

  /* Node for linked list of buffers */
  mmListNode list;

  /* Buffer data follows */
  char data[0];
} tcpBuffer;

struct _tcpLink
{
  int64_t time;
  int32_t flags;

#if CC_WINDOWS
  SOCKET socket;
#else
  int socket;
#endif
  struct sockaddr_in sockaddr;
  void *uservalue;
  size_t sendbuffered;
  void *sslconnection;

  /* Timeout in milliseconds */
  int timeoutmsecs;

  tcpCallbackSet *netio;

  mmListDualHead recvlist;
  mmListDualHead userrecvlist;
  mmListDualHead sendlist;
  void *sendpending;

  tcpBuffer *recvlast;

  mmListNode list;
  mmListNode eventlist;
};


////


static void tcpError( char *file, int line, int errorcode )
{
#if CC_WINDOWS
  LPTSTR *s = NULL;
  FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorcode, 0, (void *)&s, 0, NULL );
  TCP_DEBUG_PRINTF( "TCP Error %d at %s:%d : \"%s\".\n", errorcode, file, line, s );
  LocalFree(s);
#else
  TCP_DEBUG_PRINTF( "TCP Error %d at %s:%d : \"%s\".\n", errorcode, file, line, strerror( errorcode ) );
#endif
  return;
}

#if CC_WINDOWS
 #define TCP_ERROR() tcpError(__FILE__,__LINE__,WSAGetLastError())
#else
 #define TCP_ERROR() tcpError(__FILE__,__LINE__,errno)
#endif


////


static int64_t tcpTime( tcpContext *context )
{
  struct timeval lntime;
  gettimeofday( &lntime, 0 );
  return ( (int64_t)( lntime.tv_sec - context->reftime.tv_sec ) * (int64_t)1000 ) + ( (int64_t)( lntime.tv_usec - context->reftime.tv_usec ) / (int64_t)1000 );
}


static tcpLink *tcpLinkAlloc( tcpContext *context )
{
  tcpLink *link;

  DEBUG_SET_TRACKER();

  if( !( link = malloc( sizeof(tcpLink) ) ) )
  {
    TCP_DEBUG_PRINTF( "TCP: Memory allocation failed in %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__ );
    exit( 1 );
  }
  memset( link, 0, sizeof(tcpLink) );
#if CC_WINDOWS
  link->socket = INVALID_SOCKET;
#else
  link->socket = -1;
#endif
  mmListDualInit( &link->recvlist );
  mmListDualInit( &link->userrecvlist );
  mmListDualInit( &link->sendlist );
  link->flags = TCPLINK_FLAGS_WANT_RECV | TCPLINK_FLAGS_WANT_SEND;
  return link;
}

static void tcpEventDebug( tcpContext *context, int line )
{
  tcpLink *link;
  for( link = context->eventlist ; link ; link = link->eventlist.next )
  {
    if( link->flags & TCPLINK_FLAGS_EVENT_MASK )
      continue;
    TCP_DEBUG_PRINTF( "TCP ERROR TCP ERROR TCP ERROR AT LINE %d\n", line );
    #if CC_WINDOWS
    Sleep( 1000 );  // Sleep takes milliseconds
    #else
    sleep( 1 );     // sleep takes seconds
    #endif
  }
  return;
}

static void tcpEventQueueAdd( tcpContext *context, tcpLink *link, int eventflag )
{
  DEBUG_SET_TRACKER();

  if( link->flags & TCPLINK_FLAGS_TERMINATED )
    return;
  if( !( link->flags & TCPLINK_FLAGS_EVENT_MASK ) )
    mmListAdd( &context->eventlist, link, offsetof(tcpLink,eventlist) );
  link->flags |= eventflag;
#if 1
  if( !( eventflag & TCPLINK_FLAGS_EVENT_MASK ) )
    TCP_DEBUG_PRINTF( "TCP: BAD FLAG AS EVENT : 0x%x\n", eventflag );
#endif

#if TCP_DEBUG_EVENTS
  char *eventname;
  eventname = "??";
  if( eventflag & TCPLINK_FLAGS_EVENT_INCOMING )
    eventname = "INCOMING";
  else if( eventflag & TCPLINK_FLAGS_EVENT_RECV )
    eventname = "RECV";
  else if( eventflag & TCPLINK_FLAGS_EVENT_SENDREADY )
    eventname = "SENDREADY";
  else if( eventflag & TCPLINK_FLAGS_EVENT_SENDFINISHED )
    eventname = "SENDFINISHED";
  else if( eventflag & TCPLINK_FLAGS_EVENT_TIMEOUT )
    eventname = "TIMEOUT";
  else if( eventflag & TCPLINK_FLAGS_EVENT_CLOSED )
    eventname = "CLOSED";
  TCP_DEBUG_PRINTF( "TCP: Add event %s, eventflag 0x%x -> linkflags 0x%x\n", eventname, eventflag, (int)link->flags );
#endif

  return;
}

static void tcpEventQueueRemove( tcpContext *context, tcpLink *link )
{
  DEBUG_SET_TRACKER();

#if TCP_DEBUG_EVENTS
  TCP_DEBUG_PRINTF( "TCP: Clear events, linkflags 0x%x\n", (int)link->flags );
#endif

  if( link->flags & TCPLINK_FLAGS_EVENT_MASK )
    mmListRemove( link, offsetof(tcpLink,eventlist) );
  link->flags &= ~TCPLINK_FLAGS_EVENT_MASK;
  return;
}

static void tcpBufferFree( tcpContext *context, tcpBuffer *buf );

static void tcpLinkFree( tcpContext *context, tcpLink *link )
{
  tcpBuffer *buf, *bufnext;

  DEBUG_SET_TRACKER();

  for( buf = link->recvlist.first ; buf ; buf = bufnext )
  {
    bufnext = buf->list.next;
    tcpBufferFree( context, buf );
  }
  for( buf = link->userrecvlist.first ; buf ; buf = bufnext )
  {
    bufnext = buf->list.next;
    tcpBufferFree( context, buf );
  }
  for( buf = link->sendlist.first ; buf ; buf = bufnext )
  {
    bufnext = buf->list.next;
    tcpBufferFree( context, buf );
  }
#if TCP_ENABLE_SSL_SUPPORT
  if( link->sslconnection )
    SSL_free( link->sslconnection );
#endif

#if CC_WINDOWS
  if( link->socket != INVALID_SOCKET )
    closesocket( link->socket );
#else
  if( link->socket != -1 )
    close( link->socket );
#endif
  tcpEventQueueRemove( context, link );
  free( link );
  return;
}


////


#if CC_UNIX

static int tcpCreateWakePipe( tcpContext *context )
{
  context->wakepipe[0] = -1;
  context->wakepipe[1] = -1;
  if( pipe( context->wakepipe ) == -1 )
  {
    TCP_ERROR();
    return 0;
  }
  if( fcntl( context->wakepipe[0], F_SETFL, O_NONBLOCK ) == -1 )
    TCP_ERROR();
  if( fcntl( context->wakepipe[1], F_SETFL, O_NONBLOCK ) == -1 )
    TCP_ERROR();
  return 1;
}

static void tcpDestroyWakePipe( tcpContext *context )
{
  close( context->wakepipe[0] );
  close( context->wakepipe[1] );
  return;
}

#endif

#if CC_WINDOWS

int tcpCreateWakePipe( tcpContext *context )
{
  int reuse, sockflag;
  SOCKET listener;
  socklen_t addrlen;
  union
  {
    struct sockaddr_in inaddr;
    struct sockaddr addr;
  } a;

  listener = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  if( listener == INVALID_SOCKET)
  {
    TCP_ERROR();
    return 0;
  }
  memset( &a, 0, sizeof(a) );
  a.inaddr.sin_family = AF_INET;
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_port = 0; 

  context->wakepipe[0] = INVALID_SOCKET;
  context->wakepipe[1] = INVALID_SOCKET;
  reuse = 1;
  if( setsockopt( listener, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, (socklen_t)sizeof(reuse) ) == -1 )
    goto error;
  if( bind( listener, &a.addr, sizeof(a.inaddr) ) == SOCKET_ERROR )
    goto error;
  addrlen = sizeof(a.inaddr);
  if( getsockname( listener, &a.addr, &addrlen ) == SOCKET_ERROR )
    goto error;
  if( listen( listener, 1 ) == SOCKET_ERROR )
    goto error;
  context->wakepipe[0] = socket( AF_INET, SOCK_STREAM, 0 );
  if( context->wakepipe[0] == INVALID_SOCKET )
    goto error;
  if( connect( context->wakepipe[0], &a.addr, sizeof(a.inaddr) ) == SOCKET_ERROR )
    goto error;
  context->wakepipe[1] = accept( listener, 0, 0 );
  if( context->wakepipe[1] == INVALID_SOCKET )
    goto error;
  sockflag = 1;
  if( ioctlsocket( context->wakepipe[0], FIONBIO, (long *)&sockflag ) == SOCKET_ERROR )
    goto error;
  if( ioctlsocket( context->wakepipe[1], FIONBIO, (long *)&sockflag ) == SOCKET_ERROR )
    goto error;
  closesocket( listener );
  return 1;

  error:
  TCP_ERROR();
  closesocket( listener );
  closesocket( context->wakepipe[0] );
  closesocket( context->wakepipe[1] );
  return 0;
}

static void tcpDestroyWakePipe( tcpContext *context )
{
  closesocket( context->wakepipe[0] );
  closesocket( context->wakepipe[1] );
  return;
}

#endif


////


int tcpInit( tcpContext *context, int threadflag, int sslsupportflag )
{
  DEBUG_SET_TRACKER();

  memset( context, 0, sizeof(tcpContext) );
#if !TCP_ENABLE_SSL_SUPPORT
  if( sslsupportflag )
    return 0;
#endif
#if CC_WINDOWS
  WSADATA wsaData;
  if( WSAStartup( MAKEWORD( 2, 0 ), &wsaData ) )
  {
    if( WSAStartup( MAKEWORD( 1, 1 ), &wsaData ) )
    {
      TCP_DEBUG_PRINTF( "TCP ERROR: Failed to initialize WinSock at %s:%d.\n", __FILE__, __LINE__ );
      return 0;
    }
  }
#endif
  gettimeofday( &context->reftime, 0 );
  mmBlockInit( &context->bufferblock, sizeof(tcpBuffer) + TCP_BUFFER_DEFAULT_SIZE, TCP_BUFFER_CHUNK_COUNT, TCP_BUFFER_CHUNK_COUNT, 0x10 );
  if( !tcpCreateWakePipe( context ) )
    return 0;
  context->cancelflag = 0;
  context->threadstate = ( threadflag ? TCP_THREAD_STATE_NORMAL : TCP_THREAD_STATE_NONE );
  context->eventlist = 0;
  if( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE )
  {
    mtMutexInit( &context->mutex );
    mtSignalInit( &context->signal );
    mtThreadCreate( &context->thread, tcpThreadWork, (void *)context, MT_THREAD_FLAGS_JOINABLE, 0, 0 );
  }
#if TCP_ENABLE_SSL_SUPPORT
  context->sslcontext = 0;
  if( sslsupportflag )
  {
    SSL_load_error_strings();
    SSL_library_init();
    context->sslcontext = SSL_CTX_new( SSLv23_client_method() );
    if( !( context->sslcontext ) )
    {
      TCP_ERROR();
      tcpEnd( context );
      return 0;
    }
  }
#endif
#if CC_UNIX
  signal( SIGPIPE, SIG_IGN );
#endif
  return 1;
}

void tcpEnd( tcpContext *context )
{
  tcpLink *link, *next;
  tcpCallbackSet *netio;

  DEBUG_SET_TRACKER();

  /* Notify all sockets to close */
  if( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE )
    mtMutexLock( &context->mutex );

  for( link = context->linklist ; link ; link = link->list.next )
  {
#if CC_WINDOWS
    shutdown( link->socket, SD_BOTH );
#else
    shutdown( link->socket, SHUT_RDWR );
#endif
    link->flags |= TCPLINK_FLAGS_CLOSING;
  }
  for( link = context->listenlist ; link ; link = link->list.next )
    link->flags |= TCPLINK_FLAGS_CLOSING;

  if( !( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE ) )
    tcpFlush( context );
  else
  {
    /* Wait until all sockets are really closed */
    for( ; ( context->linklist ) || ( context->listenlist ) || ( context->terminatelist ) ; )
    {
      mtMutexUnlock( &context->mutex );
      tcpFlush( context );
      mtMutexLock( &context->mutex );
      tcpWake( context );
      mtSignalWait( &context->signal, &context->mutex );
    }
    context->cancelflag = 1;
    mtMutexUnlock( &context->mutex );
    tcpWake( context );
    mtThreadJoin( &context->thread );
    mtMutexDestroy( &context->mutex );
    mtSignalDestroy( &context->signal );
    tcpDestroyWakePipe( context );
  }

  /* Free any remaining link */
  for( link = context->linklist ; link ; link = next )
  {
    next = link->list.next;
    netio = link->netio;
    netio->closed( link->uservalue );
    mmListRemove( link, offsetof(tcpLink,list) );
    tcpLinkFree( context, link );
  }
  for( link = context->terminatelist ; link ; link = next )
  {
    next = link->list.next;
    mmListRemove( link, offsetof(tcpLink,list) );
    tcpLinkFree( context, link );
  }

#if TCP_ENABLE_SSL_SUPPORT
  if( context->sslcontext )
    SSL_CTX_free( context->sslcontext );
#endif

  mmBlockFreeAll( &context->bufferblock );

#if CC_WINDOWS
  WSACleanup();
#endif

  return;
}


////


#if CC_WINDOWS
static int tcpResolveNameAddr( char *address, int listenflag, in_addr_t *retaddr )
{
  int retval;
  in_addr_t ip;
  struct addrinfo hints;
  struct addrinfo *addrhost;

  DEBUG_SET_TRACKER();

  memset( &hints, 0, sizeof(struct addrinfo) );
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = ( listenflag ? AI_PASSIVE : 0 );

  ip = 0;
  retval = 0;
  if( !( getaddrinfo( address, 0, &hints, &addrhost ) ) )
  {
    ip = ( (struct sockaddr_in *)addrhost->ai_addr )->sin_addr.s_addr;
    retval = 1;
    freeaddrinfo( addrhost );
  }

  *retaddr = ip;
  return retval;
}
#else
static int tcpResolveNameAddr( char *address, int listenflag, in_addr_t *retaddr )
{
  in_addr_t ip;

  DEBUG_SET_TRACKER();

 #if 0
  /* Obsolete method */
  struct hostent *host_ent;
  if( ( ip = inet_addr( address ) ) == INADDR_NONE )
  {
    host_ent = gethostbyname( address );
    if( host_ent == 0 )
      return 0;
  }
  ip = *((in_addr_t *)(host_ent->h_addr_list[0]));
  *retaddr = ip;
  return 1;
 #else
  int retval;
  struct addrinfo hints;
  struct addrinfo *addrhost;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = ( listenflag ? AI_PASSIVE : 0 );

  ip = 0;
  retval = 0;
  if( !( getaddrinfo( address, 0, &hints, &addrhost ) ) )
  {
    ip = ( (struct sockaddr_in *)addrhost->ai_addr )->sin_addr.s_addr;
    retval = 1;
    freeaddrinfo( addrhost );
  }

  *retaddr = ip;
  return retval;
 #endif
}
#endif


char *tcpResolveName( char *address, int listenflag )
{
  in_addr_t addr;
  uint32_t ip;
  char *ipstring;

  DEBUG_SET_TRACKER();

  if( !( tcpResolveNameAddr( address, listenflag, &addr ) ) )
    return 0;
  ip = ntohl( addr );
  ipstring = ccStrAllocPrintf( "%d.%d.%d.%d", (int)((ip>>24)&0xff), (int)((ip>>16)&0xff), (int)((ip>>8)&0xff), (int)((ip>>0)&0xff) );
  return ipstring;
}


////


static tcpBuffer *tcpBufferAllocate( tcpContext *context, size_t size )
{
  tcpBuffer *buf;

  DEBUG_SET_TRACKER();

  if( size > TCP_BUFFER_DEFAULT_SIZE )
  {
    buf = malloc( sizeof(tcpBuffer) + size );
    buf->bufsize = size;
  }
  else
  {
    buf = mmBlockAlloc( &context->bufferblock );
    buf->bufsize = TCP_BUFFER_DEFAULT_SIZE;
  }
  buf->publicbuffer.size = buf->bufsize;
  buf->publicbuffer.pointer = buf->data;
  buf->publicbuffer.useroffset = 0;
  buf->rwoffset = 0;
  return buf;
}

static void tcpBufferFree( tcpContext *context, tcpBuffer *buf )
{
  DEBUG_SET_TRACKER();

  if( buf->bufsize > TCP_BUFFER_DEFAULT_SIZE )
  {
    free( buf );
    return;
  }
  mmBlockFree( &context->bufferblock, buf );
  return;
}

static tcpBuffer *tcpAddRecvBuffer( tcpContext *context, tcpLink *link )
{
  tcpBuffer *buf;

  DEBUG_SET_TRACKER();

  buf = tcpBufferAllocate( context, TCP_BUFFER_DEFAULT_SIZE );
  mmListDualAddLast( &link->recvlist, buf, offsetof(tcpBuffer,list) );
  return buf;
}


tcpLink *tcpConnect( tcpContext *context, char *address, int port, void *uservalue, tcpCallbackSet *netio, int sslflag )
{
  int sockflag;
  in_addr_t ip;
  tcpLink *link;
  struct sockaddr_in sockaddr;
#if CC_WINDOWS
  int wsaerrno;
#endif

  DEBUG_SET_TRACKER();

#if !TCP_ENABLE_SSL_SUPPORT
  if( sslflag )
    return 0;
#endif

  if( ( ip = inet_addr( address ) ) == INADDR_NONE )
  {
    if( !( tcpResolveNameAddr( address, 0, &ip ) ) )
      return 0;
  }

  if( !( link = tcpLinkAlloc( context ) ) )
    return 0;

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );

#if CC_WINDOWS
  if( ( link->socket = socket( AF_INET, SOCK_STREAM, 0 ) ) == INVALID_SOCKET )
#else
  if( ( link->socket = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
#endif
  {
    TCP_ERROR();
    goto error;
  }
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = ip;
  sockaddr.sin_port = htons( port );

#if CC_WINDOWS
  sockflag = 1;
  if( ioctlsocket( link->socket, FIONBIO, (long *)&sockflag ) == SOCKET_ERROR )
  {
    TCP_ERROR();
    goto error;
  }
#else
  if( fcntl( link->socket, F_SETFL, O_NONBLOCK ) == -1 )
  {
    TCP_ERROR();
    goto error;
  }
#endif

  sockflag = 1;
#if CC_WINDOWS
  if( setsockopt( link->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&sockflag, sizeof(int) ) == SOCKET_ERROR )
#else
  if( setsockopt( link->socket, IPPROTO_TCP, TCP_NODELAY, (char *)&sockflag, sizeof(int) ) == -1 )
#endif
  {
    TCP_ERROR();
    goto error;
  }
#if !CC_WINDOWS && defined(TCP_QUICKACK)
    if( setsockopt( link->socket, IPPROTO_TCP, TCP_QUICKACK, (char *)&sockflag, sizeof(int) ) == -1 )
      TCP_ERROR();
#endif

#if CC_WINDOWS
  if( connect( link->socket, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_in) ) == SOCKET_ERROR )
  {
    wsaerrno = WSAGetLastError();
    if( ( wsaerrno != WSAEINPROGRESS ) && ( wsaerrno != WSAEWOULDBLOCK ) )
    {
      TCP_ERROR();
      goto error;
    }
  }
#else
  if( ( connect( link->socket, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_in) ) == -1 ) && ( errno != EINPROGRESS ) )
  {
    TCP_ERROR();
    goto error;
  }
#endif

  memcpy( &(link->sockaddr), &sockaddr, sizeof(struct sockaddr_in) );
  link->time = tcpTime( context );
  link->uservalue = uservalue;
  link->timeoutmsecs = TCP_DEFAULT_LINK_TIMEOUT;
  link->netio = netio;

#if TCP_ENABLE_SSL_SUPPORT
  link->sslconnection = 0;
  if( sslflag )
  {
    link->sslconnection = SSL_new( context->sslcontext );
    if( !( link->sslconnection ) )
      goto error;
    SSL_set_fd( link->sslconnection, link->socket );
    link->flags |= TCPLINK_FLAGS_SSL_NEEDCONNECT;
  }
#endif

  mmListAdd( &context->linklist, link, offsetof(tcpLink,list) );

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );

  /* Wake up the thread that may be waiting on select() */
  tcpWake( context );

  return link;

  error:
  tcpLinkFree( context, link );
  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );
  return 0;
}


tcpLink *tcpListen( tcpContext *context, int port, void *listenvalue, tcpCallbackSet *netio, int sslflag )
{
  int a;
  tcpLink *link;
  struct sockaddr_in sinInterface;

  DEBUG_SET_TRACKER();

  if( !( link = tcpLinkAlloc( context ) ) )
    return 0;

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );

  link->socket = socket( AF_INET, SOCK_STREAM, 0 );
#if CC_WINDOWS
  if( link->socket == INVALID_SOCKET )
#else
  if( link->socket == -1 )
#endif
  {
    TCP_ERROR();
    goto error;
  }
  a = 1;
#if CC_WINDOWS
  if( setsockopt( link->socket, SOL_SOCKET, SO_REUSEADDR, (char *)&a, sizeof(int) ) == SOCKET_ERROR )
#else
  if( setsockopt( link->socket, SOL_SOCKET, SO_REUSEADDR, (char *)&a, sizeof(int) ) == -1 )
#endif
  {
    TCP_ERROR();
    goto error;
  }

  sinInterface.sin_family = AF_INET;
  sinInterface.sin_addr.s_addr = INADDR_ANY;
  sinInterface.sin_port = htons( port );
#if CC_WINDOWS
  if( bind( link->socket, (struct sockaddr *)&sinInterface, sizeof(struct sockaddr_in) ) == SOCKET_ERROR )
#else
  if( bind( link->socket, (struct sockaddr *)&sinInterface, sizeof(struct sockaddr_in) ) == -1 )
#endif
  {
    TCP_ERROR();
    goto error;
  }

#if CC_WINDOWS
  a = 1;
  if( ioctlsocket( link->socket, FIONBIO, (long *)&a ) == SOCKET_ERROR )
  {
    TCP_ERROR();
    goto error;
  }
#else
  if( fcntl( link->socket, F_SETFL, O_NONBLOCK ) == -1 )
  {
    TCP_ERROR();
    goto error;
  }
#endif

#if CC_WINDOWS
  if( listen( link->socket, SOMAXCONN ) == SOCKET_ERROR )
#else
  if( listen( link->socket, SOMAXCONN ) == -1 )
#endif
  {
    TCP_ERROR();
    goto error;
  }

  link->flags |= TCPLINK_FLAGS_LISTEN;
  link->uservalue = listenvalue;
  link->netio = netio;

#if TCP_ENABLE_SSL_SUPPORT
  link->sslconnection = 0;
  if( sslflag )
    link->flags |= TCPLINK_FLAGS_SSL_LISTEN;
#endif

  mmListAdd( &context->listenlist, link, offsetof(tcpLink,list) );

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );

  /* Wake up the thread that may be waiting on select() */
  tcpWake( context );

  return link;

  error:
  tcpLinkFree( context, link );
  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );
  return 0;
}


/* Set timeout for link */
void tcpSetTimeout( tcpContext *context, tcpLink *link, int milliseconds )
{
  int wakeflag;

  DEBUG_SET_TRACKER();

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );
  wakeflag = 0;
  if( milliseconds < link->timeoutmsecs )
    wakeflag = 1;
  link->timeoutmsecs = milliseconds;
  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );
  if( wakeflag )
    tcpWake( context );
  return;
}


void tcpClose( tcpContext *context, tcpLink *link )
{
  DEBUG_SET_TRACKER();

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );

#if TCP_DEBUG
  TCP_DEBUG_PRINTF( "TCP: tcpClose() called\n" );
#endif

  /* TODO: SSL shutdown? */
#if CC_WINDOWS
  shutdown( link->socket, SD_BOTH );
#else
  shutdown( link->socket, SHUT_RDWR );
#endif
  link->flags |= TCPLINK_FLAGS_CLOSING | TCPLINK_FLAGS_TERMINATED;
  tcpEventQueueRemove( context, link );

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );

  return;
}


void tcpFreeRecvBuffer( tcpContext *context, tcpLink *link, tcpDataBuffer *netbuf )
{
  tcpBuffer *buf;

  DEBUG_SET_TRACKER();

  buf = ADDRESS( netbuf, -offsetof(tcpBuffer,publicbuffer) );
  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );

#if 0
  if( buf != link->recvlist.first )
    TCP_DEBUG_PRINTF( "TCP: Logical error warning %s:%d\n", __FILE__, __LINE__ );
#endif

  mmListDualRemove( &link->userrecvlist, buf, offsetof(tcpBuffer,list) );
  tcpBufferFree( context, buf );

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );
  return;
}


/* TODO: Allow buffers where ->pointer belongs to user, not freed automatically */
tcpDataBuffer *tcpAllocSendBuffer( tcpContext *context, tcpLink *link, size_t minsize )
{
  tcpBuffer *buf;

  DEBUG_SET_TRACKER();

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );

  buf = tcpBufferAllocate( context, minsize );
  mmListAdd( &link->sendpending, buf, offsetof(tcpBuffer,list) );

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );
  return &buf->publicbuffer;
}


void tcpQueueSendBuffer( tcpContext *context, tcpLink *link, tcpDataBuffer *netbuf, size_t sendsize )
{
  tcpBuffer *buf;
  tcpCallbackSet *netio;

  DEBUG_SET_TRACKER();

  buf = ADDRESS( netbuf, -offsetof(tcpBuffer,publicbuffer) );
  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexLock( &context->mutex );

  netio = link->netio;
  buf->sendsize = sendsize;
  mmListRemove( buf, offsetof(tcpBuffer,list) );
  mmListDualAddLast( &link->sendlist, buf, offsetof(tcpBuffer,list) );
  link->flags |= TCPLINK_FLAGS_WANT_SEND;
  link->sendbuffered += sendsize;
  if( ( netio->sendwait ) && ( link->sendbuffered >= TCP_BUFFER_SEND_READY_SIZE_TRESHOLD ) )
    netio->sendwait( link->uservalue, link->sendbuffered );

  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
    mtMutexUnlock( &context->mutex );

#if TCP_DEBUG
  TCP_DEBUG_PRINTF( "TCP: tcpQueueSendBuffer() called, sendsize %d bytes\n", (int)sendsize );
#endif

  /* Wake up the thread that may be waiting on select() */
  tcpWake( context );
  return;
}


static inline void tcpPollListen( tcpContext *context )
{
  int sockflag, socket;
#if CC_WINDOWS
  int a, socklen;
#else
  socklen_t socklen;
#endif
  struct sockaddr_in sockaddr;
  tcpLink *linkl, *linklnext;
  tcpLink *link;
#if CC_WINDOWS
  int wsaerrno;
#endif

  DEBUG_SET_TRACKER();

  for( linkl = context->listenlist ; linkl ; linkl = linklnext )
  {
    linklnext = linkl->list.next;
    if( linkl->flags & TCPLINK_FLAGS_CLOSING )
    {
      mmListRemove( linkl, offsetof(tcpLink,list) );
      mmListAdd( &context->terminatelist, linkl, offsetof(tcpLink,list) );
      linkl->flags |= TCPLINK_FLAGS_TERMINATELIST;
      continue;
    }

    socklen = sizeof(struct sockaddr_in);
    socket = accept( linkl->socket, (struct sockaddr *)(&sockaddr), &socklen );
#if CC_WINDOWS
    if( socket == INVALID_SOCKET )
    {
      wsaerrno = WSAGetLastError();
      if( ( wsaerrno == WSAEINPROGRESS ) || ( wsaerrno == WSAEWOULDBLOCK ) )
        continue;
      TCP_ERROR();
      continue;
    }
#else
    if( socket == -1 )
    {
      if( errno == EWOULDBLOCK )
        continue;
      TCP_ERROR();
      continue;
    }
#endif
#if CC_UNIX
    if( socket >= FD_SETSIZE )
    {
      TCP_DEBUG_PRINTF( "TCP: Error, socket >= FD_SETSIZE, %d\n", socket );
      close( socket );
      continue;
    }
#endif

    if( !( link = tcpLinkAlloc( context ) ) )
      continue;

    /* Disable Nagle buffering */
    sockflag = 1;
#if CC_WINDOWS
    if( setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char *)&sockflag, sizeof(int) ) == SOCKET_ERROR )
      TCP_ERROR();
#else
    if( setsockopt( socket, IPPROTO_TCP, TCP_NODELAY, (char *)&sockflag, sizeof(int) ) == -1 )
      TCP_ERROR();
#endif
#if !CC_WINDOWS && defined(TCP_QUICKACK)
    if( setsockopt( socket, IPPROTO_TCP, TCP_QUICKACK, (char *)&sockflag, sizeof(int) ) == -1 )
      TCP_ERROR();
#endif
#if CC_WINDOWS
    sockflag = 1;
    if( ioctlsocket( socket, FIONBIO, (long *)&sockflag ) == SOCKET_ERROR )
      TCP_ERROR();
#else
    if( fcntl( socket, F_SETFL, O_NONBLOCK ) == -1 )
      TCP_ERROR();
#endif

    link->socket = socket;
    memcpy( &(link->sockaddr), &sockaddr, sizeof(struct sockaddr_in) );
    link->time = tcpTime( context );
    link->netio = linkl->netio;

#if TCP_ENABLE_SSL_SUPPORT
    link->sslconnection = 0;
    if( linkl->flags & TCPLINK_FLAGS_SSL_LISTEN )
    {
      link->sslconnection = SSL_new( context->sslcontext );
      if( !( link->sslconnection ) )
      {
#if CC_WINDOWS
        closesocket( socket );
#else
        close( socket );
#endif
        tcpLinkFree( context, link );
        continue;
      }
      SSL_set_fd( link->sslconnection, link->socket );
      link->flags |= TCPLINK_FLAGS_SSL_NEEDACCEPT;
    }
#endif

    mmListAdd( &context->linklist, link, offsetof(tcpLink,list) );

    /* Inherit the listening link's value, until it's updated by the incoming() callback */
    link->uservalue = linkl->uservalue;
    tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_INCOMING );
  }

  return;
}


#define TCP_CODE_DATA (0x1)
#define TCP_CODE_ERROR (0x2)
#define TCP_CODE_COMPLETE (0x4)


static inline int tcpRecv( tcpContext *context, tcpLink *link )
{
  int sslcode, retcode;
#if !CC_WINDOWS && defined(TCP_QUICKACK)
  int sockflag;
#endif
  ssize_t size, maxsize, total;
  tcpBuffer *buf;
#if CC_WINDOWS
  int wsaerrno;
#endif

  DEBUG_SET_TRACKER();

  retcode = 0;
  total = 0;
  buf = mmListDualLast( &link->recvlist, offsetof(tcpBuffer,list) );
  if( !( buf ) )
    buf = tcpAddRecvBuffer( context, link );
  for( ; ; )
  {
    maxsize = buf->bufsize - buf->rwoffset;
#if TCP_ENABLE_SSL_SUPPORT
    if( link->flags & TCPLINK_FLAGS_SSL_ACTIVE )
      size = SSL_read( link->sslconnection, &( buf->data[buf->rwoffset] ), maxsize );
    else
      size = recv( link->socket, &( buf->data[buf->rwoffset] ), maxsize, 0 );
#else
    size = recv( link->socket, &( buf->data[buf->rwoffset] ), maxsize, 0 );
#endif

#if 0
TCP_DEBUG_PRINTF( "%%%%%%%%%%%%%%%%%% RECV %lld START %%%%%%%%%%%%%%%%%%%%\n", (long long)size );
if( size > 0 )
  TCP_DEBUG_PRINTF( "%.*s\n", (int)size, (char *)&( buf->data[buf->rwoffset] ) );
TCP_DEBUG_PRINTF( "%%%%%%%%%%%%%%%%%% RECV %lld END %%%%%%%%%%%%%%%%%%%%\n", (long long)size );
#endif

#if TCP_DEBUG
    if( size > 0 )
      TCP_DEBUG_PRINTF( "TCP: Recv %d bytes\n", (int)size );
#endif

    if( size > 0 )
      retcode |= TCP_CODE_DATA;
    else if( size < 0 )
    {
#if TCP_ENABLE_SSL_SUPPORT
      if( link->flags & TCPLINK_FLAGS_SSL_ACTIVE )
      {
        sslcode = SSL_get_error( link->sslconnection, size );
        if( sslcode == SSL_ERROR_WANT_READ )
          break;
        else if( sslcode == SSL_ERROR_WANT_WRITE )
        {
          link->flags |= TCPLINK_FLAGS_WANT_SEND;
          break;
        }
        else
        {
#if TCP_DEBUG_PRINT_ERRORS
          TCP_DEBUG_PRINTF( "TCP: SSL_read() ERROR: %s\n", ERR_error_string( sslcode, 0 ) );
#endif
          retcode |= TCP_CODE_ERROR;
          break;
        }
      }
      else
#endif
      {
#if CC_WINDOWS
        wsaerrno = WSAGetLastError();
        if( ( wsaerrno == WSAEINPROGRESS ) || ( wsaerrno == WSAEWOULDBLOCK ) )
          break;
#else
        if( errno == EWOULDBLOCK )
          break;
#endif
#if TCP_DEBUG_PRINT_ERRORS
        TCP_DEBUG_PRINTF( "TCP: read() ERROR\n" );
#endif
      }
      retcode |= TCP_CODE_ERROR;
      break;
    }
    else if( size == 0 )
    {
#if TCP_DEBUG_PRINT_ERRORS
      TCP_DEBUG_PRINTF( "TCP: read() zero, socket closed\n" );
#endif
      retcode |= TCP_CODE_ERROR;
      break;
    }

    buf->rwoffset += size;
    total += size;
    /* We need a read() returning <= 0 to detect a closed socket right away */
#if 0
    if( buf->rwoffset < buf->bufsize )
      break;
#endif

    buf = tcpAddRecvBuffer( context, link );
  }

#if !CC_WINDOWS && defined(TCP_QUICKACK)
  if( retcode == TCP_CODE_DATA )
  {
    sockflag = 1;
    if( setsockopt( link->socket, IPPROTO_TCP, TCP_QUICKACK, (char *)&sockflag, sizeof(int) ) == -1 )
      TCP_ERROR();
  }
#endif

  return retcode;
}

static inline int tcpSend( tcpContext *context, tcpLink *link )
{
  int sslcode, retcode;
  ssize_t size, maxsize;
  tcpBuffer *buf, *next;
#if CC_WINDOWS
  int wsaerrno;
#endif

  DEBUG_SET_TRACKER();

  if( !( buf = link->sendlist.first ) )
    return 0;

  retcode = 0;
  for( ; ; buf = next )
  {
    maxsize = buf->sendsize - buf->rwoffset;
#if TCP_ENABLE_SSL_SUPPORT
    if( link->flags & TCPLINK_FLAGS_SSL_ACTIVE )
      size = SSL_write( link->sslconnection, &( buf->data[buf->rwoffset] ), maxsize );
    else
      size = send( link->socket, &( buf->data[buf->rwoffset] ), maxsize, 0 );
#else
    size = send( link->socket, &( buf->data[buf->rwoffset] ), maxsize, 0 );
#endif

#if 0
TCP_DEBUG_PRINTF( "%%%%%%%%%%%%%%%%%% SEND %%%%%%%%%%%%%%%%%%%%\n" );
TCP_DEBUG_PRINTF( "%.*s\n", (int)maxsize, (char *)&( buf->data[buf->rwoffset] ) );
TCP_DEBUG_PRINTF( "%%%%%%%%%%%%%%%%%% SEND %%%%%%%%%%%%%%%%%%%%\n" );
#endif

#if TCP_DEBUG
    if( size > 0 )
      TCP_DEBUG_PRINTF( "TCP: Sent %d bytes\n", (int)size );
#endif

    if( size > 0 )
      retcode |= TCP_CODE_DATA;
    else if( size <= 0 )
    {
#if TCP_ENABLE_SSL_SUPPORT
      if( link->flags & TCPLINK_FLAGS_SSL_ACTIVE )
      {
        sslcode = SSL_get_error( link->sslconnection, size );
        if( ( sslcode == SSL_ERROR_WANT_READ ) || ( sslcode == SSL_ERROR_WANT_WRITE ) )
        {
          retcode |= TCP_CODE_DATA;
          break;
        }
        else
        {
#if TCP_DEBUG_PRINT_ERRORS
          TCP_DEBUG_PRINTF( "TCP: SSL_write ERROR: %s\n", ERR_error_string( sslcode, 0 ) );
#endif
          retcode |= TCP_CODE_ERROR;
          break;
        }
      }
      else
#endif
      {
#if CC_WINDOWS
        wsaerrno = WSAGetLastError();
        if( ( wsaerrno == WSAEINPROGRESS ) || ( wsaerrno == WSAEWOULDBLOCK ) )
          retcode |= TCP_CODE_DATA;
        else
        {
          TCP_ERROR();
          retcode |= TCP_CODE_ERROR;
        }
#else
        if( errno == EWOULDBLOCK )
          retcode |= TCP_CODE_DATA;
        else
        {
          TCP_ERROR();
          retcode |= TCP_CODE_ERROR;
        }
#endif
        break;
      }
    }

    link->sendbuffered -= size;
    buf->rwoffset += size;
    if( buf->rwoffset < buf->sendsize )
      break;

    next = buf->list.next;
    mmListDualRemove( &link->sendlist, buf, offsetof(tcpBuffer,list) );
    tcpBufferFree( context, buf );

    if( !( next ) )
    {
      retcode |= TCP_CODE_COMPLETE;
      break;
    }
  }

  return retcode;
}


#if TCP_ENABLE_SSL_SUPPORT

static int tcpSslHandshake( tcpLink *link )
{
  int sslcode, eventflag;

  DEBUG_SET_TRACKER();

  if( link->flags & TCPLINK_FLAGS_SSL_NEEDCONNECT )
    sslcode = SSL_connect( link->sslconnection );
  else if( link->flags & TCPLINK_FLAGS_SSL_NEEDACCEPT )
    sslcode = SSL_accept( link->sslconnection );
  else
    return 0;

  eventflag = 0;
  if( sslcode == 1 )
  {
    link->flags &= ~( TCPLINK_FLAGS_SSL_NEEDCONNECT | TCPLINK_FLAGS_SSL_NEEDACCEPT );
    link->flags |= TCPLINK_FLAGS_SSL_ACTIVE | TCPLINK_FLAGS_WANT_RECV | TCPLINK_FLAGS_WANT_SEND;
  }
  else
  {
    sslcode = SSL_get_error( link->sslconnection, sslcode );
    link->flags &= ~( TCPLINK_FLAGS_WANT_RECV | TCPLINK_FLAGS_WANT_SEND );
    switch( sslcode )
    {
      case SSL_ERROR_WANT_READ:
        link->flags |= TCPLINK_FLAGS_WANT_RECV;
        break;
      case SSL_ERROR_WANT_WRITE:
        link->flags |= TCPLINK_FLAGS_WANT_SEND;
        break;
      default:
#if TCP_DEBUG_PRINT_ERRORS
        TCP_DEBUG_PRINTF( "TCP: SSL_connect ERROR: %s\n", ERR_error_string( sslcode, 0 ) );
        TCP_ERROR();
#endif
#if CC_WINDOWS
        shutdown( link->socket, SD_BOTH );
#else
        shutdown( link->socket, SHUT_RDWR );
#endif
        link->flags &= ~( TCPLINK_FLAGS_SSL_NEEDCONNECT | TCPLINK_FLAGS_SSL_NEEDACCEPT );
        link->flags |= TCPLINK_FLAGS_CLOSING;
        eventflag = 1;
        break;
    }
  }

  return eventflag;
}

#endif


static int tcpProcess( tcpContext *context, int64_t maxtimeout )
{
  int a, eventflag, tcpcode, wakeflag;
#if CC_UNIX
  int rmax;
#endif
  int64_t msecs, curtime, beftimeout;
  tcpLink *link, *linkl, *next;
  tcpCallbackSet *netio;
  struct timeval timeout;
  fd_set fdRead;
  fd_set fdWrite;
  fd_set fdError;

  DEBUG_SET_TRACKER();

  /* Free all terminated links */
  for( link = context->terminatelist ; link ; link = next )
  {
    next = link->list.next;
    /* Can't free link until user has called tcpClose() */
    if( !( link->flags & TCPLINK_FLAGS_TERMINATED ) )
      continue;
#if TCP_DEBUG
    TCP_DEBUG_PRINTF( "TCP: Terminate link, flags 0x%x\n", (int)link->flags );
#endif
    tcpEventQueueRemove( context, link );
    mmListRemove( link, offsetof(tcpLink,list) );
    tcpLinkFree( context, link );
  }

  if( ( context->cancelflag ) && ( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE ) )
  {
    mtMutexUnlock( &context->mutex );
    mtThreadExit();
  }

  tcpPollListen( context );

  FD_ZERO( &fdRead );
  FD_ZERO( &fdWrite );
  FD_ZERO( &fdError );

  msecs = maxtimeout;
  curtime = tcpTime( context );
  for( link = context->linklist ; link ; link = link->list.next )
  {
    if( !( link->timeoutmsecs ) )
      continue;
    if( link->flags & TCPLINK_FLAGS_EVENT_TIMEOUT )
      continue;
    /* Time remaining before timeout */
    beftimeout = link->timeoutmsecs - ( curtime - link->time );
    if( beftimeout < msecs )
      msecs = beftimeout;
  }
  if( msecs < 0 )
    msecs = 0;

  /* Add wake pipe to select() list */
#if CC_UNIX
  rmax = context->wakepipe[0];
#endif
  FD_SET( context->wakepipe[0], &fdRead );
  /* Add all sockets to the select() list */
  for( linkl = context->listenlist ; linkl ; linkl = linkl->list.next )
  {
#if CC_UNIX
    if( linkl->socket > rmax )
      rmax = linkl->socket;
#endif
    FD_SET( linkl->socket, &fdRead );
  }
  for( link = context->linklist ; link ; link = link->list.next )
  {
#if CC_WINDOWS
    if( link->socket == INVALID_SOCKET )
      continue;
#else
    if( link->socket == -1 )
      continue;
    if( link->socket > rmax )
      rmax = link->socket;
#endif
    FD_SET( link->socket, &fdError );
    if( link->flags & ( TCPLINK_FLAGS_WANT_RECV | TCPLINK_FLAGS_CLOSING ) )
      FD_SET( link->socket, &fdRead );
    if( ( link->flags & ( TCPLINK_FLAGS_WANT_SEND | TCPLINK_FLAGS_CLOSING ) ) == TCPLINK_FLAGS_WANT_SEND )
      FD_SET( link->socket, &fdWrite );
  }

  if( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE )
    mtMutexUnlock( &context->mutex );

  DEBUG_SET_TRACKER();

  /* Compute timeout and process all sockets by select() */
  timeout.tv_usec = ( msecs % 1000 ) * 1000;
  timeout.tv_sec = msecs / 1000;
#if TCP_DEBUG
  TCP_DEBUG_PRINTF( "TCP: Entering select(), %d msecs\n", (int)msecs );
#endif
#if CC_WINDOWS
  if( select( 0, &fdRead, &fdWrite, &fdError, &timeout ) == SOCKET_ERROR )
    TCP_ERROR();
#else
  if( select( rmax + 1, &fdRead, &fdWrite, &fdError, &timeout ) < 0 )
    TCP_ERROR();
#endif
#if TCP_DEBUG
  TCP_DEBUG_PRINTF( "TCP: Exited select()\n" );
#endif

  if( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE )
    mtMutexLock( &context->mutex );

  DEBUG_SET_TRACKER();

  eventflag = 0;
  /* Flush any data in wake up pipe */
  if( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE )
  {
    if( FD_ISSET( context->wakepipe[0], &fdRead ) )
    {
#if CC_UNIX
      while( read( context->wakepipe[0], &a, sizeof(a) ) >= 0 );
#endif
#if CC_WINDOWS
      while( recv( context->wakepipe[0], (char *)&a, sizeof(a), 0 ) >= 0 );
#endif
      eventflag = 1;
    }
  }

  DEBUG_SET_TRACKER();

  /* Process the list of sockets */
  curtime = tcpTime( context );
  for( link = context->linklist ; link ; link = next )
  {
    next = link->list.next;
    netio = link->netio;
    wakeflag = 0;
#if TCP_ENABLE_SSL_SUPPORT
    if( link->flags & ( TCPLINK_FLAGS_SSL_NEEDCONNECT | TCPLINK_FLAGS_SSL_NEEDACCEPT ) )
    {
      if( FD_ISSET( link->socket, &fdRead ) || FD_ISSET( link->socket, &fdWrite ) || FD_ISSET( link->socket, &fdError ) )
      {
        link->time = curtime;
        eventflag |= tcpSslHandshake( link );
        continue;
      }
      goto timeoutcheck;
    }
#endif
    if( FD_ISSET( link->socket, &fdRead ) || FD_ISSET( link->socket, &fdError ) )
    {
      if( ( link->flags & TCPLINK_FLAGS_EVENT_MASK ) == TCPLINK_FLAGS_EVENT_TIMEOUT )
        tcpEventQueueRemove( context, link );
      link->time = curtime;
      tcpcode = tcpRecv( context, link );
      if( tcpcode & TCP_CODE_DATA )
      {
        eventflag = 1;
        wakeflag = 1;
        tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_RECV );
      }
      if( tcpcode & TCP_CODE_ERROR )
      {
        eventflag = 1;
        wakeflag = 1;
        if( !( link->flags & TCPLINK_FLAGS_CLOSING ) )
        {
          /* TODO: SSL version? */
#if CC_WINDOWS
          shutdown( link->socket, SD_BOTH );
#else
          shutdown( link->socket, SHUT_RDWR );
#endif
          link->flags |= TCPLINK_FLAGS_CLOSING;
        }
        else if( !( link->flags & TCPLINK_FLAGS_TERMINATELIST ) )
        {
          /* Remove link from active list, add to terminatelist */
          mmListRemove( link, offsetof(tcpLink,list) );
          mmListAdd( &context->terminatelist, link, offsetof(tcpLink,list) );
          tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_CLOSED );
          link->flags |= TCPLINK_FLAGS_TERMINATELIST;
        }
        goto nextfd;
      }
    }

    if( FD_ISSET( link->socket, &fdWrite ) )
    {
      if( ( link->flags & TCPLINK_FLAGS_EVENT_MASK ) == TCPLINK_FLAGS_EVENT_TIMEOUT )
        tcpEventQueueRemove( context, link );
      link->time = curtime;
      tcpcode = tcpSend( context, link );
      if( !( tcpcode ) )
        link->flags &= ~TCPLINK_FLAGS_WANT_SEND;
      else
      {
        if( tcpcode & TCP_CODE_COMPLETE )
        {
          link->flags &= ~TCPLINK_FLAGS_WANT_SEND;
          tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_SENDFINISHED );
          eventflag = 1;
          wakeflag = 1;
        }
        if( tcpcode & TCP_CODE_ERROR )
        {
          if( !( link->flags & TCPLINK_FLAGS_CLOSING ) )
          {
            /* TODO: SSL version? */
#if CC_WINDOWS
            shutdown( link->socket, SD_BOTH );
#else
            shutdown( link->socket, SHUT_RDWR );
#endif
            link->flags |= TCPLINK_FLAGS_CLOSING;
          }
          eventflag = 1;
          wakeflag = 1;
          goto nextfd;
        }
      }
      if( link->sendbuffered < TCP_BUFFER_SEND_READY_SIZE_TRESHOLD )
      {
        eventflag = 1;
        wakeflag = 1;
        tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_SENDREADY );
      }
    }

#if TCP_ENABLE_SSL_SUPPORT
    timeoutcheck:
#endif

    /* Regular timeout */
/*
TCP_DEBUG_PRINTF( "TIMEOUT CHECK : %d %d\n", (int)( curtime - link->time ), (int)link->timeoutmsecs );
*/
    if( ( ( curtime - link->time ) >= link->timeoutmsecs ) && !( link->flags & TCPLINK_FLAGS_EVENT_TIMEOUT ) )
    {
#if TCP_DEBUG
      TCP_DEBUG_PRINTF( "TCP: Timeout! %d msecs\n", (int)( curtime - link->time ) );
#endif
      tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_TIMEOUT );
      eventflag = 1;
      wakeflag = 1;
    }

    /* Closing force timeout */
    if( ( link->flags & ( TCPLINK_FLAGS_CLOSING | TCPLINK_FLAGS_TERMINATELIST ) ) == TCPLINK_FLAGS_CLOSING )
    {
      if( ( curtime - link->time ) >= TCP_DEFAULT_CLOSING_TIMEOUT )
      {
        /* Remove link from active list, add to terminatelist */
        mmListRemove( link, offsetof(tcpLink,list) );
        mmListAdd( &context->terminatelist, link, offsetof(tcpLink,list) );
        tcpEventQueueAdd( context, link, TCPLINK_FLAGS_EVENT_CLOSED );
        link->flags |= TCPLINK_FLAGS_TERMINATELIST;
        eventflag = 1;
      }
    }

    /* Stuff going on with link, asynchronous notification, tcp lock active */
    if( ( wakeflag ) && ( netio->wake ) )
      netio->wake( link->uservalue );

    nextfd:
    continue;
  }

  return eventflag;
}


/* Background thread's main(), processing all sockets in a loop */
static void *tcpThreadWork( void *p )
{
  tcpContext *context;

  DEBUG_SET_TRACKER();

  context = p;
#if CC_UNIX
  signal( SIGPIPE, SIG_IGN );
#endif
  mtMutexLock( &context->mutex );
  for( ; ; )
  {
    if( tcpProcess( context, TCP_DEFAULT_SELECT_TIMEOUT ) )
      mtSignalBroadcast( &context->signal );
  }
  mtMutexUnlock( &context->mutex );

  return 0;
}


////


/**
 * Flush all pending callbacks for all connections of the TCP interface.
 */
static int tcpFlushCallbacks( tcpContext *context )
{
  int activityflag;
  int64_t curtime;
  tcpLink *link, *next;
  tcpBuffer *buf, *bufnext;
  tcpCallbackSet *netio;

  DEBUG_SET_TRACKER();

  activityflag = 0;
  if( context->threadstate == TCP_THREAD_STATE_NORMAL )
  {
    mtMutexLock( &context->mutex );
    context->threadstate = TCP_THREAD_STATE_LOCKED;
  }

  DEBUG_SET_TRACKER();

  if( context->eventlist )
    activityflag = 1;
  curtime = tcpTime( context );
  for( link = context->eventlist ; link ; link = next )
  {
    next = link->eventlist.next;
    netio = link->netio;
#if TCP_DEBUG_EVENTS
    TCP_DEBUG_PRINTF( "TCP: Process events 0x%x\n", (int)link->flags );
#endif

    if( link->flags & TCPLINK_FLAGS_EVENT_INCOMING )
      link->uservalue = netio->incoming( link, link->uservalue );
    if( link->flags & TCPLINK_FLAGS_EVENT_RECV )
    {
#if TCP_DEBUG_EVENTS
      TCP_DEBUG_PRINTF( "TCP: Event Recv\n" );
#endif
      /* Send all read buffers to user */
      for( buf = link->recvlist.first ; buf ; buf = bufnext )
      {
        bufnext = buf->list.next;
        /* If we find an empty last buffer: exit and keep buffer */
        if( !( buf->rwoffset ) && !( bufnext ) )
          break;
        /* Move to user list, clamp buffer size to actual content, call user's recv() */
        mmListDualRemove( &link->recvlist, buf, offsetof(tcpBuffer,list) );
        mmListDualAddLast( &link->userrecvlist, buf, offsetof(tcpBuffer,list) );
        buf->publicbuffer.size = buf->rwoffset;
        netio->recv( link->uservalue, &buf->publicbuffer );
      }
    }
    if( link->flags & TCPLINK_FLAGS_EVENT_SENDFINISHED )
      netio->sendfinished( link->uservalue );
    if( !( link->flags & TCPLINK_FLAGS_CLOSING ) )
    {
      if( link->flags & TCPLINK_FLAGS_EVENT_SENDREADY )
        netio->sendready( link->uservalue, link->sendbuffered );
      if( ( link->flags & TCPLINK_FLAGS_EVENT_TIMEOUT ) && ( ( curtime - link->time ) >= link->timeoutmsecs ) )
      {
        netio->timeout( link->uservalue );
        link->time = curtime;

        DEBUG_SET_TRACKER();
      }
    }
    if( link->flags & TCPLINK_FLAGS_EVENT_CLOSED )
    {
      netio->closed( link->uservalue );

      DEBUG_SET_TRACKER();
    }
    /* Remove all event flags and remove link from event list */
    tcpEventQueueRemove( context, link );
  }

  DEBUG_SET_TRACKER();

  if( context->threadstate == TCP_THREAD_STATE_LOCKED )
  {
    context->threadstate = TCP_THREAD_STATE_NORMAL;
    mtMutexUnlock( &context->mutex );
  }

  return activityflag;
}


int tcpWait( tcpContext *context, long timeout )
{
  DEBUG_SET_TRACKER();

#if 1
  if( !( timeout ) )
    timeout = 30000;
#endif

  if( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE )
  {
    if( tcpFlushCallbacks( context ) )
      return 1;
    if( context->threadstate == TCP_THREAD_STATE_NORMAL )
      mtMutexLock( &context->mutex );
    if( timeout )
      mtSignalWaitTimeout( &context->signal, &context->mutex, timeout );
    else
      mtSignalWait( &context->signal, &context->mutex );
    if( context->threadstate == TCP_THREAD_STATE_NORMAL )
      mtMutexUnlock( &context->mutex );
  }
  else
  {
    if( tcpFlushCallbacks( context ) )
      return 1;
    tcpProcess( context, timeout );
  }
  return tcpFlushCallbacks( context );
}


int tcpFlush( tcpContext *context )
{
  DEBUG_SET_TRACKER();

  if( context->threadstate == TCP_THREAD_STATE_LOCKED )
    TCP_DEBUG_PRINTF( "TCP: Dead-lock!\nDo not call tcpFlush() from within tcpFlush() callback.\n" );
  /* If we don't have a background thread, go process all buffers */
  if( context->threadstate == TCP_THREAD_STATE_NONE )
    tcpProcess( context, 0 );
  return tcpFlushCallbacks( context );
}


void tcpWake( tcpContext *context )
{
  DEBUG_SET_TRACKER();
#if CC_UNIX
  char c;
  int dummy;
  if( ( context->wakepipe[1] != -1 ) && ( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE ) )
    dummy = write( context->wakepipe[1], &c, 1 );
  dummy = dummy;
#endif
#if CC_WINDOWS
  char c;
  int dummy;
  if( ( context->wakepipe[1] != INVALID_SOCKET ) && ( context->threadstate & TCP_THREAD_STATE_MASK_ACTIVE ) )
    dummy = send( context->wakepipe[1], &c, 1, 0 );
  dummy = dummy;
#endif
  return;
}


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

typedef struct
{
  size_t size;
  void *pointer;

  /* For user's private use */
  /* This is to allow later processing, outside recv() callback, in order to release tcp mutex */
  size_t useroffset;
  mmListNode userlist;
} tcpDataBuffer;


////


#if defined(_WIN64) || defined(__WIN64__) || defined(WIN64) || defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
 #include <winsock2.h>
#endif


typedef struct
{
  struct timeval reftime;
#if defined(_WIN64) || defined(__WIN64__) || defined(WIN64) || defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
  SOCKET wakepipe[2];
#else
  int wakepipe[2];
#endif

  void *linklist;
  void *listenlist;
  void *eventlist;
  void *terminatelist;
  int buffercount;
  mmBlockHead bufferblock;

  mtThread thread;
  mtMutex mutex;
  mtSignal signal;
  volatile int cancelflag;
  int threadstate;

  void *sslcontext;
} tcpContext;

typedef struct
{
  /* Only for listening links, incoming() callback defines the uservalue for the new link */
  void *(*incoming)( void *link, void *listenvalue );
  /* Only for data links, new data received */
  void (*recv)( void *uservalue, tcpDataBuffer *recvbuf );
  /* Only for data links, ready to send more data */
  void (*sendready)( void *uservalue, size_t sendbuffered );
  /* Optional: Only for data links, pause before sending more data */
  void (*sendwait)( void *uservalue, size_t sendbuffered );
  /* Only for data links, all data sent */
  void (*sendfinished)( void *uservalue );
  /* Only for data links, timed out */
  void (*timeout)( void *uservalue );
  /* Only for data links, link terminated */
  void (*closed)( void *uservalue );
  /* Optional: asynchroneous notification of pending event on link */
  void (*wake)( void *uservalue );
} tcpCallbackSet;


typedef struct _tcpLink tcpLink;


////


/* Initialize TCP/IP context */
int tcpInit( tcpContext *context, int threadflag, int sslsupportflag );

/* Close all connections, free resources */
void tcpEnd( tcpContext *context );

/* Resolve domain name, return malloc()ed buffer */
char *tcpResolveName( char *address, int listenflag );


////


/* Open link to server */
tcpLink *tcpConnect( tcpContext *context, char *address, int port, void *uservalue, tcpCallbackSet *netio, int sslflag );

/* Listen for connections */
tcpLink *tcpListen( tcpContext *context, int port, void *uservalue, tcpCallbackSet *netio, int sslflag );

/* Set timeout for link */
void tcpSetTimeout( tcpContext *context, tcpLink *link, int milliseconds );

/* Close link */
void tcpClose( tcpContext *context, tcpLink *link );


////


/* Free read buffers received by recv() */
void tcpFreeRecvBuffer( tcpContext *context, tcpLink *link, tcpDataBuffer *buf );

/* Allocate send buffer */
tcpDataBuffer *tcpAllocSendBuffer( tcpContext *context, tcpLink *link, size_t size );

/* Queue the send buffer with data ready to go, buffer is automatically freed after being sent */
void tcpQueueSendBuffer( tcpContext *context, tcpLink *link, tcpDataBuffer *buf, size_t sendsize );


////


/* Wait on context until something happens, implies a tcpFlush(), return 1 if there was any activity */
int tcpWait( tcpContext *context, long timeout );

/* Wake the thread waiting on context */
void tcpWake( tcpContext *context );

/* Flush all pending callbacks on context, return 1 if there was any activity */
int tcpFlush( tcpContext *context );



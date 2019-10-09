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

typedef struct httpConnection httpConnection;

struct httpConnection
{
  int status;
  /* User desired flags */
  int flags;
  /* Flags given to us by server, masked by user flags */
  int serverflags;
  tcpContext *tcp;
  tcpLink *link;
  char *address;
  int port;
  int keepalivemax;
  /* Connection timeouts */
  int idletimeout;
  int waitingtimeout;

  /* Track error count */
  int errorcount;
  /* Track failed retry count */
  int retryfailurecount;
  /* Query sent count since last reconnect */
  int sentquerycount;
  /* Asynchronous wake from tcp thread */
  void (*wake)( tcpContext *tcp, httpConnection *http, void *wakecontext );
  void *wakecontext;

  int queryqueuecount;
  mmListDualHead querywaitlist;
  mmListDualHead querysentlist;
  mmListDualHead recvbuflist;
};

#define HTTP_CONNECTION_FLAGS_KEEPALIVE (0x1)
#define HTTP_CONNECTION_FLAGS_PIPELINING (0x2)
#define HTTP_CONNECTION_FLAGS_SSL (0x4)

#define HTTP_CONNECTION_FLAGS_PUBLICMASK (0xffff)


/* Retry after reconnecting if link is lost */
#define HTTP_QUERY_FLAGS_RETRY (0x1)
/* Allow pipelining of that query */
#define HTTP_QUERY_FLAGS_PIPELINING (0x2)


typedef struct
{
  int httpcode;
  int httpversion;
  char *header;
  ssize_t headerlength;
  void *body;
  ssize_t bodysize;

  int keepaliveflag;
  int keepalivemax;
  int chunkedflag;
  int trailerflag;
  int contentlength;

  char *location;
} httpResponse;


////


/* Open HTTP connection to server */
httpConnection *httpOpen( tcpContext *tcp, char *address, int port, int flags );

/* Close HTTP connection */
void httpClose( httpConnection *http );

/* Set timeouts for connection in milliseconds */
void httpSetTimeout( httpConnection *http, int idletimeout, int waitingtimeout );

/* Queue a query for connection, querycallback() is called when finished */
int httpAddQuery( httpConnection *http, char *querystring, size_t querylen, int queryflags, void *queryuservalue, void (*querycallback)( void *uservalue, int resultcode, httpResponse *response ) );

/* Write queries, parse received data, call querycallback() for queries as appropriate */
int httpProcess( httpConnection *http );

/* Flag all pending queries to abort */
void httpAbortQueue( httpConnection *http );

/* Get the count of queries on connection, includes pending and completed when querycallback() hasn't been called yet */
int httpGetQueryQueueCount( httpConnection *http );

/* Returns the count of errors and clear it back to zero */
int httpGetClearErrorCount( httpConnection *http );

/* Returns non-zero if connected */
int httpGetStatus( httpConnection *http );

/* Set callback to be called asynchronously, by the tcp thread, when httpProcess() should be called */
void httpSetWakeCallback( httpConnection *http, void (*wake)( tcpContext *tcp, httpConnection *http, void *wakecontext ), void *wakecontext );


////


/* Parameter resultcode of query callback */
enum
{
  /* Query was successfull */
  HTTP_RESULT_SUCCESS = 0,
  /* Failed to connect to server, query was never sent */
  HTTP_RESULT_CONNECT_ERROR,
  /* Query allowed RETRY but the server closed the socket, might have been sent or not */
  HTTP_RESULT_TRYAGAIN_ERROR,
  /* Query disallowed RETRY but the server closed the socket before we had a reply */
  HTTP_RESULT_NOREPLY_ERROR,
  /* Error parsing the server's HTTP reply */
  HTTP_RESULT_BADFORMAT_ERROR,

  /* Error codes not used internally, meant for external usage after further checks */
  /* Bad HTTP code */
  HTTP_RESULT_CODE_ERROR,
  /* Error parsing reply */
  HTTP_RESULT_PARSE_ERROR,
  /* Some mild reply handling error */
  HTTP_RESULT_PROCESS_ERROR,
  /* Some grave reply handling error */
  HTTP_RESULT_SYSTEM_ERROR,

  /* Count of result codes, for an user to define custom ones */
  HTTP_RESULT_COUNT
};



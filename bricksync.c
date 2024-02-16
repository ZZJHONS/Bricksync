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
#include "cpuinfo.h"
#include "antidebug.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <signal.h>
#elif CC_WINDOWS
 #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0501
 #endif
 #include <windows.h>
 #include <wincon.h>
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
#include "bsantidebug.h"


////


/*
== Debug ==
gcc bricksync.c bricksyncconf.c bricksyncnet.c bricksyncinit.c bricksyncinput.c bsantidebug.c bsmathpuzzle.c bsregister.c bsapihistory.c bstranslation.c bsoutputxml.c bspriceguide.c bsmastermode.c bscheck.c bssync.c bsapplydiff.c bsfetchorderinv.c bsresolve.c bsfetchinv.c bsfetchorderlist.c bsfetchset.c bscheckreg.c bsfetchpriceguide.c tcp.c vtlex.c cpuinfo.c antidebug.c mm.c mmhash.c mmbitmap.c cc.c tcphttp.c oauth.c bricklink.c brickowl.c colortable.c json.c bsx.c bsxpg.c journal.c exclperm.c iolog.c crypthash.c cryptsha1.c rand.c bn512.c bn1024.c rsabn.c -g -Wall -o bricksync -lm -lpthread -lssl -lcrypto -DBS_VERSION_BUILDTIME=`date '+%s'`

== Release ==
gcc -std=gnu99 -m64 cpuconf.c cpuinfo.c -O2 -s -o cpuconf
./cpuconf -h
gcc  -Wno-implicit-function-declaration bricksync.c bricksyncconf.c bricksyncnet.c bricksyncinit.c bricksyncinput.c bsantidebug.c bsmathpuzzle.c bsregister.c bsapihistory.c bstranslation.c bsoutputxml.c bspriceguide.c bsmastermode.c bscheck.c bssync.c bsapplydiff.c bsfetchorderinv.c bsresolve.c bsfetchinv.c bsfetchorderlist.c bsfetchset.c bscheckreg.c bsfetchpriceguide.c tcp.c vtlex.c cpuinfo.c antidebug.c mm.c mmhash.c mmbitmap.c cc.c tcphttp.c oauth.c bricklink.c brickowl.c colortable.c json.c bsx.c bsxpg.c journal.c exclperm.c iolog.c crypthash.c cryptsha1.c rand.c bn512.c bn1024.c rsabn.c -O2 -s -fvisibility=hidden -o bricksync -lm -lpthread -lssl -lcrypto -DBS_VERSION_BUILDTIME=`date '+%s'`


wc -l bricksync.* bricksyncconf.* bricksyncnet.* bricksyncinit.* bricksyncinput.* bsantidebug.* bsmathpuzzle.* bsregister.* bsapihistory.* bstranslation.* bsoutputxml.* bspriceguide.* bsmastermode.* bscheck.* bssync.* bsapplydiff.* bsfetchorderinv.* bsresolve.* bsfetchinv.* bsfetchorderlist.* bsfetchset.* bscheckreg.* bsfetchpriceguide.* tcp.* vtlex.* cpuinfo.* antidebug.* mm.* mmhash.* mmbitmap.* cc.* tcphttp.* oauth.* bricklink.* brickowl.* colortable.* json.* bsx.* bsxpg.* journal.* exclperm.* iolog.* crypthash.* cryptsha1.* rand.* bn512.* bn1024.* rsabn.*

LD_PRELOAD=./mmdebug.so ./bricksync 2> log.txt
*/


#if !CC_LINUX
 #undef BS_ENABLE_COREDUMP
 #define BS_ENABLE_COREDUMP (0)
#endif
#if BS_ENABLE_COREDUMP
 #include <sys/time.h>
 #include <sys/resource.h>
#endif


////


/* Raw state file data */
typedef struct
{
  /* 128 bytes */
  int32_t version;
  int32_t flags;
  int64_t blinitdate;
  int64_t bltopdate;
  int64_t boinitdate;
  int64_t botopdate;
  int32_t backupindex;
  int32_t errorindex;
  int64_t bllastcheckdate;
  int64_t bllastsyncdate;
  int64_t bolastcheckdate;
  int64_t bolastsyncdate;
  int32_t puzzlequestiontype;
  int32_t lastrunversion;
  int64_t lastruntime;
  int32_t reserved1[8];
} bsFileStateBase;


typedef struct
{
  bsFileStateBase base;
  bsApiHistory blapihistory;
  bsApiHistory boapihistory;
} bsFileState;


////


void bsFatalError( bsContext *context )
{
  ioLog *output;
  output = 0;
  if( context )
    output = &context->output;
  ioPrintf( output, IO_MODEBIT_FLUSH, BSMSG_ERROR IO_RED "Fatal error encountered." IO_DEFAULT "\n" );
  ioPrintf( output, IO_MODEBIT_FLUSH, BSMSG_ERROR IO_RED "Exiting now." IO_DEFAULT "\n" );
  ioLogEnd( output );
#if CC_WINDOWS || CC_OSX
  printf( "Press Enter to exit...\n" );
  for( ; !( ioStdinReadLock( 0 ) ) ; )
    ccSleep( 100 );
  ioStdinUnlock();
#endif
  exit( 1 );
  return;
}


////


bsContext *bsInit( char *statepath, int *retstateloaded )
{
  int stateloaded, conferrorcount;
  size_t stateloadsize;
  char *cwdret, *sysname;
  bsContext *context;
  bsFileState state;

  DEBUG_SET_TRACKER();

  context = malloc( sizeof(bsContext) );
  memset( context, 0, sizeof(bsContext) );

  /* Get current working directory */
  context->cwd[0] = 0;
#if CC_UNIX
  cwdret = getcwd( context->cwd, BS_CWD_PATH_MAX );
#elif CC_WINDOWS
  cwdret = _getcwd( context->cwd, BS_CWD_PATH_MAX );
#else
 #error Unknown/Unsupported platform!
#endif
  if( !( cwdret ) )
  {
    printf( "ERROR: Work directory path too long\n" );
    printf( "Exiting now.\n" );
    exit( 0 );
  }

  /* Initialize log output */
  if( !( ioLogInit( &context->output, BS_LOG_DIR, 10 ) ) )
  {
    printf( "ERROR: Failed to open log file for writing in \"%s" CC_DIR_SEPARATOR_STRING "%s\".\n", context->cwd, BS_LOG_DIR );
    free( context );
    return 0;
  }
  ioPrintf( &context->output, 0, BSMSG_INIT "Launching BrickSync\n" );
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, BSMSG_INIT "Version %s - Build Date %s %s\n", BS_VERSION_STRING, __DATE__, __TIME__ );
  /* Log operating system name for debugging purposes */
  sysname = ccGetSystemName();
  if( sysname )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, BSMSG_INIT "Operating System : %s\n", sysname );
    free( sysname );
  }

  ioPrintf( &context->output, 0, BSMSG_INIT "Work directory is \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );

#if CC_UNIX
  mkdir( BS_BRICKLINK_ORDER_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
  mkdir( BS_BRICKOWL_ORDER_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
  mkdir( BS_PRICEGUIDE_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
  mkdir( BS_BACKUP_DIR, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
#elif CC_WINDOWS
  _mkdir( BS_BRICKLINK_ORDER_DIR );
  _mkdir( BS_BRICKOWL_ORDER_DIR );
  _mkdir( BS_PRICEGUIDE_DIR );
  _mkdir( BS_BACKUP_DIR );
#else
 #error Unknown/Unsupported platform!
#endif

  /* Default values */
  context->contextflags = 0x0;
  context->stateflags = 0x0;
#if BS_ENABLE_MATHPUZZLE
  context->puzzlesolution.i = 24;
#endif
  context->bricklink.pipelinequeuesize = BS_BRICKLINK_PIPELINED_FETCH;
  context->bricklink.orderinitdate = 0;
  context->bricklink.ordertopdate = 0;
  context->bricklink.syncdelay = BS_SYNC_DELAY_BASE;
  context->bricklink.failinterval = BS_POLL_FAIL_INTERVAL_DEFAULT;
  context->bricklink.pollinterval = BS_POLL_SUCCESS_INTERVAL_DEFAULT;
  context->bricklink.apicountlimit = BS_BRICKLINK_APICOUNT_LIMIT_DEFAULT;
  context->bricklink.apicountpricelimit = BS_BRICKLINK_APICOUNT_PRICELIMIT_DEFAULT;
  context->bricklink.apicountnoteslimit = BS_BRICKLINK_APICOUNT_NOTESLIMIT_DEFAULT;
  context->bricklink.apicountquantitylimit = BS_BRICKLINK_APICOUNT_QUANTITYLIMIT_DEFAULT;
  context->bricklink.apicountsyncresume = BS_BRICKLINK_APICOUNT_SYNCRESUME_DEFAULT;
  context->bricklink.xmluploadindex = 0;
  context->bricklink.xmlupdateindex = 0;
  context->brickowl.pipelinequeuesize = BS_BRICKLINK_PIPELINED_FETCH;
  context->brickowl.orderinitdate = 0;
  context->brickowl.ordertopdate = 0;
  context->brickowl.syncdelay = BS_SYNC_DELAY_BASE;
  context->brickowl.failinterval = BS_POLL_FAIL_INTERVAL_DEFAULT;
  context->brickowl.pollinterval = BS_POLL_SUCCESS_INTERVAL_DEFAULT;
  context->brickowl.reuseemptyflag = 0;
  context->backupindex = 0;
  context->errorindex = 0;
  context->priceguidepath = 0;
  context->priceguideflags = BSX_PRICEGUIDE_FLAGS_BRICKSTOCK;
  context->priceguidecachetime = BS_PRICEGUIDE_CACHETIME_DEFAULT;
  context->retainemptylotsflag = 0;
  context->checkmessageflag = 0;
  context->curtime = time( 0 );
  context->messagetime = 0;
  context->message = 0;
  context->lastmessagetimestamp = 0;
  context->diskspacechecktime = 0;
  context->findordertime = 7 * (24*60*60);
#if BS_ENABLE_MATHPUZZLE
  context->puzzleanswer.i = 8;
#endif
  randSeed( &context->randstate, context->curtime ^ ( (uintptr_t)context ) ^ ( (uintptr_t)&state ) );

  /* Initialize API history */
  memset( &context->bricklink.apihistory, 0, sizeof(bsApiHistory) );
  memset( &context->brickowl.apihistory, 0, sizeof(bsApiHistory) );

  /* Load configuration file */
  ioPrintf( &context->output, 0, BSMSG_INIT "Loading configuration at \"" IO_GREEN "%s" IO_DEFAULT "\".\n", BS_CONFIGURATION_FILE );
  if( !( bsConfLoad( context, BS_CONFIGURATION_FILE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Failed to read configuration file at \"" IO_RED "%s" CC_DIR_SEPARATOR_STRING "%s" IO_WHITE "\".\n", context->cwd, BS_CONFIGURATION_FILE );
    goto error;
  }

  /* Ensure reasonable values */
  if( context->bricklink.pollinterval < BS_POLL_SUCCESS_INTERVAL_MIN )
    context->bricklink.pollinterval = BS_POLL_SUCCESS_INTERVAL_MIN;
  if( context->brickowl.pollinterval < BS_POLL_SUCCESS_INTERVAL_MIN )
    context->brickowl.pollinterval = BS_POLL_SUCCESS_INTERVAL_MIN;
  if( context->bricklink.failinterval < BS_POLL_FAIL_INTERVAL_MIN )
    context->bricklink.failinterval = BS_POLL_FAIL_INTERVAL_MIN;
  if( context->brickowl.failinterval < BS_POLL_FAIL_INTERVAL_MIN )
    context->brickowl.failinterval = BS_POLL_FAIL_INTERVAL_MIN;
  if( !( context->priceguidepath ) )
    context->priceguidepath = BS_PRICEGUIDE_DIR;
  if( context->bricklink.pipelinequeuesize < 1 )
    context->bricklink.pipelinequeuesize = 1;
  else if( context->bricklink.pipelinequeuesize > BS_BRICKLINK_PIPELINED_FETCH_MAX )
    context->bricklink.pipelinequeuesize = BS_BRICKLINK_PIPELINED_FETCH_MAX;
  if( context->brickowl.pipelinequeuesize < 1 )
    context->brickowl.pipelinequeuesize = 1;
  else if( context->brickowl.pipelinequeuesize > BS_BRICKOWL_PIPELINED_FETCH_MAX )
    context->brickowl.pipelinequeuesize = BS_BRICKOWL_PIPELINED_FETCH_MAX;

  /* Verify configuration variables */
  conferrorcount = 0;
  if( !( context->bricklink.consumerkey ) || ( strlen( context->bricklink.consumerkey ) <= 1 ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Configuration variable bricklink.consumerkey is undefined.\n" );
    conferrorcount++;
  }
  if( !( context->bricklink.consumersecret ) || ( strlen( context->bricklink.consumersecret ) <= 1 ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Configuration variable bricklink.consumersecret is undefined.\n" );
    conferrorcount++;
  }
  if( !( context->bricklink.token ) || ( strlen( context->bricklink.token ) <= 1 ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Configuration variable bricklink.token is undefined.\n" );
    conferrorcount++;
  }
  if( !( context->bricklink.tokensecret ) || ( strlen( context->bricklink.tokensecret ) <= 1 ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Configuration variable bricklink.tokensecret is undefined.\n" );
    conferrorcount++;
  }
  if( !( context->brickowl.key ) || ( strlen( context->brickowl.key ) <= 1 ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Configuration variable brickowl.key is undefined.\n\n" );
    conferrorcount++;
  }
  if( conferrorcount )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "You must edit the configuration file \"" IO_MAGENTA "%s" IO_WHITE "\" before running BrickSync.\n", BS_CONFIGURATION_FILE );
    goto error;
  }

  ioPrintf( &context->output, 0, BSMSG_INIT "Configuration loaded.\n" );

  /* Load state file */
  stateloaded = 0;
  stateloadsize = ccFileLoadDirect( BS_STATE_FILE, &state, sizeof(bsFileStateBase), sizeof(bsFileState) );
  if( stateloadsize )
  {
    if( state.base.version == 0x1 )
    {
      context->stateflags = state.base.flags;
      context->bricklink.orderinitdate = state.base.blinitdate;
      context->bricklink.ordertopdate = state.base.bltopdate;
      context->brickowl.orderinitdate = state.base.boinitdate;
      context->brickowl.ordertopdate = state.base.botopdate;
      context->backupindex = state.base.backupindex;
      context->errorindex = state.base.errorindex;
      context->bricklink.lastchecktime = state.base.bllastcheckdate;
      context->bricklink.lastsynctime = state.base.bllastsyncdate;
      context->brickowl.lastchecktime = state.base.bolastcheckdate;
      context->brickowl.lastsynctime = state.base.bolastsyncdate;
#if BS_ENABLE_MATHPUZZLE
      context->puzzlequestiontype = state.base.puzzlequestiontype;
#endif
      context->lastrunversion = state.base.lastrunversion;
      context->lastruntime = state.base.lastruntime;
      /* Promote any MUSTUPDATE to MUSTSYNC */
      if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE )
        context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_SYNC;
      if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE )
        context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
      /* Load api history */
      if( stateloadsize == sizeof(bsFileState) )
      {
        memcpy( &context->bricklink.apihistory, &state.blapihistory, sizeof(bsApiHistory) );
        memcpy( &context->brickowl.apihistory, &state.boapihistory, sizeof(bsApiHistory) );
      }
      stateloaded = 1;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Loaded state file flags : 0x%04x\n", context->stateflags );
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Loaded state time stamps : "CC_LLD" %ld\n", (long long)context->bricklink.ordertopdate, (long long)context->brickowl.ordertopdate );
      /* Incomplete init? */
      if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_INITSYNC )
      {
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Previous BrickSync initialization did not complete, starting from scratch.\n" );
        stateloaded = 0;
      }
    }
    else
    {
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Unknown version in BrickSync state file.\n" );
      goto error;
    }
  }

  /* Update API call count history */
  bsApiHistoryUpdate( context );

  /* Initialize OAuth stuff */
  context->bricklink.oauthtime = 0;
  oauthRand32Seed( &context->oauthrand, (uint32_t)time( 0 ) + (uint32_t)(intptr_t)&context );

  /* Initialize TCP networking */
  if( !( tcpInit( &context->tcp, 1, 1 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to initialize TCP interface.\n" );
    goto error;
  }

  /* Resolve IPs */
#if 0
  context->bricklink.apiaddress = strdup( "54.209.53.59" );
  context->bricklink.webaddress = strdup( "54.208.56.110" );
  context->brickowl.apiaddress = strdup( "178.33.122.183" );
  context->bricksyncwebaddress = strdup( "199.58.80.33" );
#else
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INIT "Resolving IP addresses for API and WEB services.\n" );
  context->bricklink.apiaddress = tcpResolveName( BS_BRICKLINK_API_SERVER, 0 );
  if( !( context->bricklink.apiaddress ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKLINK_API_SERVER );
    goto error;
  }
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKLINK_API_SERVER, context->bricklink.apiaddress );
  context->bricklink.webaddress = tcpResolveName( BS_BRICKLINK_WEB_SERVER, 0 );
  if( !( context->bricklink.webaddress ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKLINK_WEB_SERVER );
    goto error;
  }
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKLINK_WEB_SERVER, context->bricklink.webaddress );
  context->brickowl.apiaddress = tcpResolveName( BS_BRICKOWL_API_SERVER, 0 );
  if( !( context->brickowl.apiaddress ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKOWL_API_SERVER );
    goto error;
  }
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKOWL_API_SERVER, context->brickowl.apiaddress );
  if( context->checkmessageflag )
  {
    context->bricksyncwebaddress = tcpResolveName( BS_BRICKSYNC_WEB_SERVER, 0 );
    if( !( context->bricksyncwebaddress ) )
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Failed to resolve IP address for " IO_RED "%s" IO_WHITE ".\n", BS_BRICKSYNC_WEB_SERVER );
    else
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Resolved %s as %s\n", BS_BRICKSYNC_WEB_SERVER, context->bricksyncwebaddress );
  }
#endif

#if BS_ENABLE_REGISTRATION
  /* TODO: User's registration key? */
  if( bsRegistrationInit( context, context->registrationkey ) )
    context->contextflags |= BS_CONTEXT_FLAGS_REGISTERED;
#endif

  /* Create empty tracked inventory */
  context->inventory = bsxNewInventory();

  /* Define HTTP connections to BrickLink and BrickOwl */
  context->bricklink.http = httpOpen( &context->tcp, context->bricklink.apiaddress, 443, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING | HTTP_CONNECTION_FLAGS_SSL );
  context->bricklink.webhttp = httpOpen( &context->tcp, context->bricklink.webaddress, 80, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING );
  context->brickowl.http = httpOpen( &context->tcp, context->brickowl.apiaddress, 443, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING | HTTP_CONNECTION_FLAGS_SSL );
  if( ( context->checkmessageflag ) && ( context->bricksyncwebaddress ) )
    context->bricksyncwebhttp = httpOpen( &context->tcp, context->bricksyncwebaddress, 80, HTTP_CONNECTION_FLAGS_KEEPALIVE | HTTP_CONNECTION_FLAGS_PIPELINING );

  /* Increase BrickOwl timeout due to absurd times required to download inventory */
  httpSetTimeout( context->brickowl.http, 120*1000, 120*1000 );

  /* Determine next synchronization times */
  context->curtime = time( 0 );
  context->bricklink.checktime = context->curtime;
  context->brickowl.checktime = context->curtime;
  if( !( stateloaded ) )
  {
    context->bricklink.checktime += context->bricklink.pollinterval;
    context->brickowl.checktime += context->brickowl.pollinterval;
  }
  context->bricklink.synctime = context->curtime;
  context->brickowl.synctime = context->curtime;

  /* Duplicate high context flags, anti-debugging obfuscation */
  context->contextflags |= context->contextflags << BS_CONTEXT_FLAGS_DUP_SHIFT;

  /* Initialize query counts to zero */
  context->bricklink.querycount = 0;
  context->bricklink.webquerycount = 0;
  context->brickowl.querycount = 0;

  /* Initialize diff update inventories */
  context->bricklink.diffinv = bsxNewInventory();
  context->brickowl.diffinv = bsxNewInventory();

  /* Head of linked list of completed queries */
  mmListDualInit( &context->replylist );

  /* Initialize BLID <->BOID translation database */
  translationTableInit( &context->translationtable, BS_TRANSLATION_FILE );

  *retstateloaded = stateloaded;
  return context;

  error:
  ioLogEnd( &context->output );
  free( context );
  return 0;
}


////


/* Prepare to update BrickSync state */
int bsSaveState( bsContext *context, journalDef *journal )
{
  bsFileState state;
  journalEntry journalentry;

  DEBUG_SET_TRACKER();

  /* Build state file data */
  memset( &state, 0, sizeof(bsFileState) );
  state.base.version = 0x1;
  state.base.flags = context->stateflags;
  state.base.blinitdate = context->bricklink.orderinitdate;
  state.base.bltopdate = context->bricklink.ordertopdate;
  state.base.boinitdate = context->brickowl.orderinitdate;
  state.base.botopdate = context->brickowl.ordertopdate;
  state.base.backupindex = context->backupindex;
  state.base.errorindex = context->errorindex;
  state.base.bllastcheckdate = context->bricklink.lastchecktime;
  state.base.bllastsyncdate = context->bricklink.lastsynctime;
  state.base.bolastcheckdate = context->brickowl.lastchecktime;
  state.base.bolastsyncdate = context->brickowl.lastsynctime;
#if BS_ENABLE_MATHPUZZLE
  state.base.puzzlequestiontype = context->puzzlequestiontype;
#endif
  state.base.lastrunversion = BS_VERSION_INTEGER;
  state.base.lastruntime = context->curtime;
  memcpy( &state.blapihistory, &context->bricklink.apihistory, sizeof(bsApiHistory) );
  memcpy( &state.boapihistory, &context->brickowl.apihistory, sizeof(bsApiHistory) );
  /* Store temporary file with fsync and record journal entry */
  if( !( ccFileStore( BS_STATE_TEMP_FILE, &state, sizeof(bsFileState), 1 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to write state file as \"" IO_RED "%s" CC_DIR_SEPARATOR_STRING "%s" IO_WHITE "\".\n", context->cwd, BS_STATE_TEMP_FILE );
    return 0;
  }
  /* Add to journal if any, otherwise update straight away */
  if( journal )
    journalAddEntry( journal, BS_STATE_TEMP_FILE, BS_STATE_FILE, 0, 0 );
  else
  {
    journalentry.oldpath = BS_STATE_TEMP_FILE;
    journalentry.newpath = BS_STATE_FILE;
    if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, &journalentry, 1 ) ) )
      return 0;
  }
  context->contextflags &= ~BS_CONTEXT_FLAGS_UPDATED_STATE;
  return 1;
}


int bsSaveInventory( bsContext *context, journalDef *journal )
{
  journalEntry journalentry;

  DEBUG_SET_TRACKER();

  /* Store temporary file with fsync and record journal entry */
  if( !( bsxSaveInventory( BS_INVENTORY_TEMP_FILE, context->inventory, 1, 0 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to write inventory file as \"" IO_RED "%s" CC_DIR_SEPARATOR_STRING "%s" IO_WHITE "\".\n", context->cwd, BS_INVENTORY_TEMP_FILE );
    return 0;
  }
  /* Add to journal if any, otherwise update straight away */
  if( journal )
    journalAddEntry( journal, BS_INVENTORY_TEMP_FILE, BS_INVENTORY_FILE, 0, 0 );
  else
  {
    journalentry.oldpath = BS_INVENTORY_TEMP_FILE;
    journalentry.newpath = BS_INVENTORY_FILE;
    if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, &journalentry, 1 ) ) )
      return 0;
  }
  context->contextflags &= ~BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
  return 1;
}


////


/* Filter out all items with '~' in remarks and repack */
void bsInventoryFilterOutItems( bsContext *context, bsxInventory *inv )
{
  int itemindex;
  bsxItem *item;

  DEBUG_SET_TRACKER();

  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( ( item->typeid == 'U' ) || ( bsInvItemFilterFlag( context, item ) ) )
      bsxRemoveItem( inv, item );
  }
  bsxPackInventory( inv );

  return;
}

/* Generate an unique ExtID for that item */
void bsItemSetUniqueExtID( bsContext *context, bsxInventory *inv, bsxItem *item )
{
  int64_t extid;

  DEBUG_SET_TRACKER();

  if( item->extid != -1 )
    return;
  for( ; ; )
  {
#if CPUCONF_WORD_SIZE >= 64
    extid = randInt( &context->randstate );
#else
    extid = ( (uint64_t)randInt( &context->randstate ) ) + ( (uint64_t)randInt( &context->randstate ) << 32 );
#endif
    if( extid == -1 )
      continue;
    if( !( bsxFindExtID( inv, extid ) ) )
      break;
  }
  item->extid = extid;
  return;
}


////


int bsBrickLinkCanLoadOrder( bsContext *context, bsOrder *order )
{
  int retval;
  char *filepath;

  DEBUG_SET_TRACKER();

  filepath = ccStrAllocPrintf( BS_BRICKLINK_ORDER_PATH, (long long)order->id );
  retval = ccFileExists( filepath );
  free( filepath );

  return retval;
}

bsxInventory *bsBrickLinkLoadOrder( bsContext *context, bsOrder *order )
{
  int loadinvflag, existflag;
  bsxInventory *inv;
  char *filepath;

  DEBUG_SET_TRACKER();

  inv = bsxNewInventory();
  filepath = ccStrAllocPrintf( BS_BRICKLINK_ORDER_PATH, (long long)order->id );
  loadinvflag = bsxLoadInventory( inv, filepath );
  existflag = ccFileExists( filepath );
  if( ( existflag ) && !( loadinvflag ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "BrickLink order file \"" IO_RED "%s" IO_WHITE "\" appears to exist but we failed to load it.\n", filepath );
  free( filepath );
  if( !( loadinvflag ) )
  {
    bsxFreeInventory( inv );
    return 0;
  }
  return inv;
}

int bsBrickLinkSaveOrder( bsContext *context, bsOrder *order, bsxInventory *inv, journalDef *journal )
{
  int retval;
  char *oldpath, *newpath;

  DEBUG_SET_TRACKER();

  bsOrderSetInventoryInfo( inv, order );

  oldpath = ccStrAllocPrintf( BS_BRICKLINK_ORDER_TEMP_PATH, (long long)order->id );
  newpath = ccStrAllocPrintf( BS_BRICKLINK_ORDER_PATH, (long long)order->id );
  journalAddEntry( journal, oldpath, newpath, 1, 1 );
  retval = bsxSaveInventory( oldpath, inv, 1, 0 );
  if( retval )
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Saved BrickLink order " IO_GREEN "#"CC_LLD"" IO_DEFAULT " at \"" IO_CYAN "%s" IO_DEFAULT "\".\n", (long long)order->id, newpath );
  else
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to write BrickLink order to \"" IO_RED "%s" IO_WHITE "\".\n", oldpath );
  return retval;
}

int bsBrickOwlCanLoadOrder( bsContext *context, bsOrder *order )
{
  int retval;
  char *filepath;

  DEBUG_SET_TRACKER();

  filepath = ccStrAllocPrintf( BS_BRICKOWL_ORDER_PATH, (long long)order->id );
  retval = ccFileExists( filepath );
  free( filepath );

  return retval;
}

int bsBrickOwlSaveOrder( bsContext *context, bsOrder *order, bsxInventory *inv, journalDef *journal )
{
  int retval;
  char *oldpath, *newpath;

  DEBUG_SET_TRACKER();

  bsOrderSetInventoryInfo( inv, order );

  oldpath = ccStrAllocPrintf( BS_BRICKOWL_ORDER_TEMP_PATH, (long long)order->id );
  newpath = ccStrAllocPrintf( BS_BRICKOWL_ORDER_PATH, (long long)order->id );
  journalAddEntry( journal, oldpath, newpath, 1, 1 );
  retval = bsxSaveInventory( oldpath, inv, 1, 0 );
  if( retval )
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Saved BrickOwl order " IO_GREEN "#"CC_LLD"" IO_DEFAULT " at \"" IO_CYAN "%s" IO_DEFAULT "\".\n", (long long)order->id, newpath );
  else
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to write BrickOwl order to \"" IO_RED "%s" IO_WHITE "\".\n", oldpath );
  return retval;
}


/* Returned string must be free()'d */
char *bsInventoryBackupPath( bsContext *context, int tempflag )
{
  time_t curtime;
  struct tm timeinfo;
  char timebuf[64];
  char *dirstring, *pathstring;

  DEBUG_SET_TRACKER();

  curtime = time( 0 );
  timeinfo = *( localtime( &curtime ) );
  strftime( timebuf, 64, "%Y-%m-%d", &timeinfo );
  dirstring = ccStrAllocPrintf( BS_BACKUP_DIR CC_DIR_SEPARATOR_STRING "%s", timebuf );
#if CC_UNIX
  mkdir( dirstring, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
#elif CC_WINDOWS
  _mkdir( dirstring );
#else
 #error Unknown/Unsupported platform!
#endif
  pathstring = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%s" "%05d.bsx", dirstring, ( tempflag ? ".tmp" : "" ), context->backupindex );
  if( !( tempflag ) )
    context->backupindex++;
  context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
  free( dirstring );

  return pathstring;
}


/* Save a backup of the tracked inventory, with fsync() and journaling */
int bsStoreBackup( bsContext *context, journalDef *journal )
{
  int retval;
  journalEntry journalentry;
  char *backupoldpath, *backupnewpath;

  DEBUG_SET_TRACKER();

  backupoldpath = bsInventoryBackupPath( context, 1 );
  backupnewpath = bsInventoryBackupPath( context, 0 );
  retval = 1;
  ioPrintf( &context->output, 0, BSMSG_INFO "Saving backup of tracked inventory at \"" IO_MAGENTA "%s" IO_DEFAULT "\".\n", backupnewpath );
  if( !( bsxSaveInventory( backupoldpath, context->inventory, 1, 0 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to write inventory backup to \"" IO_RED "%s" IO_WHITE "\".\n", backupoldpath );
    retval = 0;
    free( backupoldpath );
    free( backupnewpath );
  }
  else
  {
    if( journal )
      journalAddEntry( journal, backupoldpath, backupnewpath, 1, 1 );
    else
    {
      journalentry.oldpath = backupoldpath;
      journalentry.newpath = backupnewpath;
      if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, &journalentry, 1 ) ) )
        retval = 0;
      free( backupoldpath );
      free( backupnewpath );
    }
  }
  return retval;
}


/* Returned string must be free()'d */
char *bsErrorStoragePath( bsContext *context, int tempflag )
{
  time_t curtime;
  struct tm timeinfo;
  char timebuf[64];
  char *dirstring, *pathstring;

  DEBUG_SET_TRACKER();

  curtime = time( 0 );
  timeinfo = *( localtime( &curtime ) );
  strftime( timebuf, 64, "%Y-%m-%d", &timeinfo );
  dirstring = ccStrAllocPrintf( BS_ERROR_DIR "%s", timebuf );
#if CC_UNIX
  mkdir( dirstring, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
#elif CC_WINDOWS
  _mkdir( dirstring );
#else
 #error Unknown/Unsupported platform!
#endif
  pathstring = ccStrAllocPrintf( "%s" CC_DIR_SEPARATOR_STRING "%s%05d.txt", dirstring, ( tempflag ? ".tmp" : "" ), context->errorindex );
  context->errorindex++;
  context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
  free( dirstring );

  return pathstring;
}


int bsStoreError( bsContext *context, char *errortype, char *header, size_t headerlength, void *data, size_t datasize )
{
  int retval;
  char *errorpath;
  ccGrowth growth;

  DEBUG_SET_TRACKER();

  if( !( headerlength | datasize ) )
    return 0;
  ccGrowthInit( &growth, 65536 );
  errorpath = bsErrorStoragePath( context, 0 );
  ioPrintf( &context->output, 0, BSMSG_WARNING "%s - Saving server reply (" IO_CYAN "%d" IO_WHITE "+" IO_CYAN "%d" IO_WHITE " bytes) at path \"" IO_RED "%s" IO_WHITE "\".\n", errortype, (int)headerlength, (int)datasize, errorpath );
  ccGrowthPrintf( &growth, "==== HEADERS ====\n" );
  ccGrowthData( &growth, header, headerlength );
  ccGrowthPrintf( &growth, "\n==== DATA ====\n" );
  ccGrowthData( &growth, data, datasize );
  ccGrowthPrintf( &growth, "\n==== END ====\n" );
  retval = ccFileStore( errorpath, growth.data, growth.offset, 0 );
  if( !( retval ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "Failed to write error file at \"" IO_RED "%s" IO_WHITE "\".\n", errorpath );
  free( errorpath );
  ccGrowthFree( &growth );
  return retval;
}


////


int bsInitBrickLinkBackup( bsContext *context )
{
  int retval;
  bsxInventory *inv;
  journalEntry journalentry;
  char *backupoldpath, *backupnewpath;

  DEBUG_SET_TRACKER();

  if( ( context->lastrunversion ) && ( context->lastrunversion < BS_VERSION_INTEGER_ENCODE(1,3,9) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "This is your first run with BrickSync version >= 1.3.9, which now supports " IO_MAGENTA "tier pricing" IO_WHITE " for the first time!\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "For safety of your data, we will save a backup of the current BrickLink inventory to disk.\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "It is " IO_RED "strongly recommended" IO_WHITE " to type " IO_CYAN "blmaster on" IO_WHITE " followed by " IO_CYAN "blmaster off" IO_WHITE " to import all your " IO_MAGENTA "tier prices" IO_WHITE " into BrickSync right away.\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "\n" );
  }
  else if( ( context->lastruntime ) && ( ( context->curtime - context->lastruntime ) >= BS_INITBACKUP_LASTRUNTIME ) )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "State file is over " IO_YELLOW "%d" IO_WHITE " hours old (" IO_RED "%d" IO_WHITE " hours old).\n", BS_INITBACKUP_LASTRUNTIME / (60*60), (int)( ( context->curtime - context->lastruntime ) / (60*60) ) );
    ioPrintf( &context->output, 0, BSMSG_WARNING "For safety of your data, we will save a backup of the current BrickLink inventory to disk.\n" );
  }
  else
    return 1;

  inv = bsQueryBrickLinkInventory( context );
  if( !( inv ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Failed to retrieve BrickLink inventory.\n" );
    return 0;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink inventory has " IO_CYAN "%d" IO_DEFAULT " items in " IO_CYAN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );

  /* Save to disk */
  backupoldpath = bsInventoryBackupPath( context, 1 );
  backupnewpath = bsInventoryBackupPath( context, 0 );
  retval = 1;
  ioPrintf( &context->output, 0, BSMSG_INFO "Saving backup of BrickLink inventory at \"" IO_MAGENTA "%s" IO_DEFAULT "\".\n", backupnewpath );
  if( !( bsxSaveInventory( backupoldpath, context->inventory, 1, 0 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to write inventory backup to \"" IO_RED "%s" IO_WHITE "\".\n", backupoldpath );
    retval = 0;
  }
  else
  {
    journalentry.oldpath = backupoldpath;
    journalentry.newpath = backupnewpath;
    if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, &journalentry, 1 ) ) )
      retval = 0;
  }
  free( backupoldpath );
  free( backupnewpath );
  bsxFreeInventory( inv );

  return retval;
}


void bsDiskFreeSpaceCheck( bsContext *context )
{
  int64_t freediskspace, diskcheckinterval;

  /* Run disk space check */
  diskcheckinterval = BS_BRICKSYNC_DISKSPACECHECK_INTERVAL;
  freediskspace = ccGetFreeDiskSpace( BS_BACKUP_DIR );
  if( freediskspace >= 0 )
  {
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Free disk space check returned " CC_LLD " bytes.\n", (long long)freediskspace );
    if( freediskspace < BS_DISK_SPACE_CRITICAL )
    {
      char *pruneargv[] = { "prunebackups", "15" };
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Free disk space is critically low, only " IO_RED CC_LLD " MB" IO_WHITE " are available.\n", (long long)( freediskspace / 1048576 ) );
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Pruning old backups automatically now.\n" );
      bsCommandPruneBackups( context, 2, pruneargv );
    }
    else if( freediskspace < BS_DISK_SPACE_WARNING )
    {
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Free disk space is low, only " IO_YELLOW CC_LLD " MB" IO_WHITE " are available.\n", (long long)( freediskspace / 1048576 ) );
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Please consider using the " IO_CYAN "prunebackups" IO_DEFAULT " command.\n\n" );
      diskcheckinterval = BS_BRICKSYNC_DISKSPACECHECK_WARNING_INTERVAL;
    }
  }
  context->diskspacechecktime = context->curtime + diskcheckinterval;

  return;
}


////


#if CC_UNIX

volatile int unixSignalSIGUSR1 = 0;

static void unixSignalHandlerSIGUSR1( int signo, siginfo_t *si, void *data )
{
  unixSignalSIGUSR1 = 1;
  return;
}

static void unixInitSignal()
{
  struct sigaction sa;
  sigemptyset( &sa.sa_mask );
  sa.sa_flags = SA_SIGINFO;
  sa.sa_handler = (void *)unixSignalHandlerSIGUSR1;
  sigaction( SIGUSR1, &sa, 0 );
  return;
}

#endif

#if CC_WINDOWS

static void windowsInitConsole()
{
  HANDLE handle;
  COORD sbmaxsize, sbsize;
  SMALL_RECT displayarea = { 0, 0, 0, 0 };
  HWND consolewindow;
  RECT windowrect;

  handle = GetStdHandle( STD_OUTPUT_HANDLE );
  if( handle )
  {
    sbmaxsize = GetLargestConsoleWindowSize( handle );
    if( ( sbmaxsize.X | sbmaxsize.Y ) )
    {
      sbmaxsize.X = CC_MAX( 80, sbmaxsize.X - 20 );
      sbmaxsize.Y = CC_MAX( 36, sbmaxsize.Y - 10 );
      sbsize.X = sbmaxsize.X;
      sbsize.Y = 256;
      if( SetConsoleScreenBufferSize( handle, sbsize ) )
      {
        displayarea.Left = 0;
        displayarea.Top = 0;
        displayarea.Right = sbmaxsize.X - 1;
        displayarea.Bottom = sbmaxsize.Y - 1;
        SetConsoleWindowInfo( handle, TRUE, &displayarea );
      }
    }
  }
  consolewindow = GetConsoleWindow();
  if( consolewindow )
  {
    if( GetWindowRect( consolewindow, &windowrect ) )
      MoveWindow( consolewindow, 32, 32, windowrect.right - windowrect.left, windowrect.bottom - windowrect.top, 1 );
  }
  return;
}

#endif


int main( int argc, char **argv )
{
  int stateloaded, milliseconds, actionflag, workloop;
  bsContext *context;
  void *exclperm;
  journalDef journal;
  bsxInventory *diffinv;
  cpuInfo cpuinfo;
  boUserDetails userdetails;
#if BS_ENABLE_ANTIDEBUG
  int statusflag;
  int (*volatile antidebuginit)( bsContext *context, cpuInfo *cpuinfo );
  int (*volatile blapplydiff)( bsContext *context, bsxInventory *diffinv, int *retryflag );
  int (*volatile boapplydiff)( bsContext *context, bsxInventory *diffinv, int *retryflag );
#endif

#if CC_UNIX
  printf( "\33[0m\33[40m\33[37m" );
  unixInitSignal();
#endif
#if CC_WINDOWS
  windowsInitConsole();
#endif

  mmInit();
  cpuGetInfo( &cpuinfo );
  /* Set Startup Time */
  bsSetStartupTime();

#if ENABLE_DEBUG_TRACKING
  debugTrackerInit( BS_DEBUG_TRACKER_WATCH_MILLISECONDS, BS_DEBUG_TRACKER_STALL_MILLISECONDS );
#endif

  /* Initiate threaded stdin */
  /* The only way to read from the keyboard with buffering without blocking on Windows. Blarggghhh */
  ioStdinInit();

  DEBUG_SET_TRACKER();

  /* Ahrem, let's introduce ourselves. Hi, I'm BrickSync! */
  printf( "BrickSync %s - Build Date %s %s\n", BS_VERSION_STRING, __DATE__, __TIME__ );
  printf( "Copyright (c) 2014-2016 Alexis Naveros\n" );
  printf( "Contact: Stragus on BrickOwl and BrickLink\n" );
  printf( "Email: alexis@rayforce.net\n" );
  printf( "Coupon Donations: Stragus on BrickOwl and BrickLink\n" );
  printf( "Paypal Donations: alexis@rayforce.net\n" );
  printf( "\n" );

#if BS_ENABLE_COREDUMP
  struct rlimit rlim;
  if( !( getrlimit( RLIMIT_CORE, &rlim ) ) )
  {
    if( rlim.rlim_cur != RLIM_INFINITY )
    {
      rlim.rlim_cur = rlim.rlim_max;
      setrlimit( RLIMIT_CORE, &rlim );
    }
  }
#endif

  DEBUG_SET_TRACKER();

  /* Ensure exclusive execution */
  if( !( exclperm = exclPermStart( BS_LOCK_FILE ) ) )
  {
    if( ccFileExists( BS_LOCK_FILE ) )
      ioPrintf( 0, 0, BSMSG_ERROR "You can not have multiple instances of BrickSync running in the same directory.\n" IO_DEFAULT );
    else
    {
      /* Print some information to let the user knows what's going on */
      char *cwd, *cwdret;
      ioPrintf( 0, 0, BSMSG_ERROR "Failed to write the lock file at \"%s\".\n" IO_DEFAULT, BS_LOCK_FILE );
      cwd = malloc( BS_CWD_PATH_MAX );
#if CC_UNIX
      cwdret = getcwd( cwd, BS_CWD_PATH_MAX );
#elif CC_WINDOWS
      cwdret = _getcwd( cwd, BS_CWD_PATH_MAX );
#else
 #error Unknown/Unsupported platform!
#endif
      if( cwdret )
      {
        ioPrintf( 0, 0, "\nYour current working directory is \"" IO_GREEN "%s" IO_DEFAULT "\".\n", cwd );
        ioPrintf( 0, 0, "Does it correspond to the path where BrickSync was installed?\n" );
#if CC_UNIX
        ioPrintf( 0, 0, "\nA way to run BrickSync with the correct working directory:\n" );
        ioPrintf( 0, 0, "- Open a terminal.\n" );
        ioPrintf( 0, 0, "- Use the cd command to change the path to where BrickSync was installed.\n" );
        ioPrintf( 0, 0, "- Type ./bricksync to launch BrickSync.\n" );
#elif CC_WINDOWS
        ioPrintf( 0, 0, "\nOn Windows, double-clicking an executable should set the working directory properly.\n" );
#endif
        ioPrintf( 0, 0, "\nAlso, depending on your graphical environment:\n" );
        ioPrintf( 0, 0, "- You may be able to set the working directory by creating a shortcut to\n" );
        ioPrintf( 0, 0, "  the BrickSync executable, and editing the shortcut's properties.\n" );
        ioPrintf( 0, 0, IO_WHITE "\nPlease correct this issue and run BrickSync again!\n\n" IO_DEFAULT );
      }
      free( cwd );
    }
    bsFatalError( 0 );
    return 0;
  }

  DEBUG_SET_TRACKER();

  /* Replay the journal and any pending operation */
  journalReplay( BS_JOURNAL_FILE );

#if BS_ENABLE_LIMITS
  bsAntiDebugUnpack();
#endif

  /* Initialize BrickSync context */
  context = bsInit( BS_STATE_FILE, &stateloaded );
  if( !( context ) )
  {
    bsFatalError( context );
    return 0;
  }

  DEBUG_SET_TRACKER();

#if BS_ENABLE_ANTIDEBUG
  antidebuginit = bsAntiDebugInit - BS_ANTIDEBUG_INIT_OFFSET;
  antiDebugHandleSIGILL();
#endif

  if( stateloaded )
  {
    ioPrintf( &context->output, 0, BSMSG_INIT "BrickSync state successfully loaded.\n" );
    /* Attempt to load local inventory from disk */
    if( !( bsxLoadInventory( context->inventory, BS_INVENTORY_FILE ) ) )
    {
      stateloaded = 0;
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "No main inventory file found at \"" IO_RED "%s" CC_DIR_SEPARATOR_STRING "%s" IO_WHITE "\".\n", context->cwd, BS_INVENTORY_FILE );
      ioPrintf( &context->output, 0, BSMSG_INFO "Discarding the state file loaded.\n" );
    }
  }
  else
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "No BrickSync state file found!\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "We will create a new state file as \"" IO_CYAN "%s" CC_DIR_SEPARATOR_STRING "%s" IO_DEFAULT "\".\n", context->cwd, BS_STATE_FILE );
    /* We'll actually delay writing the state file until the initialization is finished */
  }

  if( bsQueryBrickOwlUserDetails( context, &userdetails ) )
  {
    context->username = ccStrDup( userdetails.username );
    context->storename = ccStrDup( userdetails.storename );
    context->storecurrency = ccStrDup( userdetails.storecurrency );
  }
  else
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "Failed to query BrickOwl user details, store name and currency are unknown.\n" );
    context->username = 0;
    context->storename = 0;
    context->storecurrency = ccStrDup( "UnknownCurrency" );
  }

  if( stateloaded )
  {
    /* Print status */
    if( stateloaded )
    {
      char *statusargv[] = { "status", "-s" };
      /* Print status summary */
      ioPrintf( &context->output, IO_MODEBIT_NOLOG, "\n" );
      bsCommandStatus( context, 2, statusargv );
    }
    bsDiskFreeSpaceCheck( context );
  }

  if( !( stateloaded ) )
    context->lastrunversion = BS_VERSION_INTEGER;

  DEBUG_SET_TRACKER();

#if BS_ENABLE_ANTIDEBUG
  antidebuginit += bsAntiDebugInitOffset;
#endif

  /* Clean from scratch initialization */
  if( !( stateloaded ) )
  {
    /* Load BrickLink inventory as our default inventory */
    if( !( bsInitBrickLink( context ) ) )
    {
      bsFatalError( context );
      return 0;
    }
    /* Filter out items with '~' in remarks */
    bsInventoryFilterOutItems( context, context->inventory );
    context->stateflags |= BS_STATE_FLAGS_BRICKOWL_INITSYNC;
    journalAlloc( &journal, 2 );
    if( !( bsxSaveInventory( BS_INVENTORY_TEMP_FILE, context->inventory, 1, 0 ) ) )
    {
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to save inventory file \"" IO_RED "%s" IO_WHITE "\"!\n", BS_INVENTORY_TEMP_FILE );
      bsFatalError( context );
      return 0;
    }
    journalAddEntry( &journal, BS_INVENTORY_TEMP_FILE, BS_INVENTORY_FILE, 0, 0 );
    if( !( bsSaveState( context, &journal ) ) )
    {
      bsFatalError( context );
      return 0;
    }
    if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount ) ) )
    {
      bsFatalError( context );
      return 0;
    }
    journalFree( &journal );
  }

  DEBUG_SET_TRACKER();

#if BS_ENABLE_ANTIDEBUG
  /* Initialize some anti-debugging stuff */
  if( !( antidebuginit( context, &cpuinfo ) ) )
  {
    bsFatalError( context );
    return 0;
  }
  if( cpuinfo.totalcorecount )
    bsAntiDebugMid( context );
  bsAntiDebugEnd( context );
#endif

  DEBUG_SET_TRACKER();

  /* Second part of the initialization */
  if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_INITSYNC )
  {
    if( !( bsInitBrickOwl( context ) ) )
    {
      bsFatalError( context );
      return 0;
    }
    context->stateflags &= ~BS_STATE_FLAGS_BRICKOWL_INITSYNC;
    if( !( bsSaveState( context, 0 ) ) )
    {
      bsFatalError( context );
      return 0;
    }
    ioPrintf( &context->output, 0, BSMSG_INFO "Initialization complete.\n" );
  }

  DEBUG_SET_TRACKER();

#if 1
  /* Fetch BrickSync message if needed */
  if( ( context->checkmessageflag ) && ( context->curtime >= context->messagetime ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_NOLOG, "\n" );
    bsFetchBrickSyncMessage( context );
    context->messagetime = context->curtime + BS_BRICKSYNC_MESSAGE_INTERVAL;
  }
#endif

  /* Fetch any BLID->BOID we might have missing, due to interrupted write to the translation database */
  ioPrintf( &context->output, 0, BSMSG_INFO "Resolving any missing BOID in tracked inventory...\n" );
  if( !( bsQueryBrickOwlLookupBoids( context, context->inventory, BS_RESOLVE_FLAGS_TRYFALLBACK, 0 ) ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "Failed to resolve missing BOIDs, use the " IO_CYAN "owlresolve" IO_WHITE " command to try again.\n" );

  DEBUG_SET_TRACKER();

  /* Save backup of BrickLink inventory if our state is old */
  if( stateloaded )
  {
    if( !( bsInitBrickLinkBackup( context ) ) )
    {
      bsFatalError( context );
      return 0;
    }
  }

#if BS_ENABLE_DEBUG_OUTPUT && ENABLE_DEBUG_TRACKING
  ioPrintf( &context->output, IO_MODEBIT_NOLOG, "\n" );
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_DEBUG "Watchdog thread is active.\n" );
#endif

  /* Disallow negative quantities */
  bsxClampNegativeInventory( context->inventory );

  if( !( stateloaded ) )
  {
    ioPrintf( &context->output, 0, "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "As this appears to be your first time running BrickSync, " IO_RED "please read the Frequently Asked Questions" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Before you add or edit any inventory, read especially the questions " IO_YELLOW "#2" IO_DEFAULT " and " IO_YELLOW "#3" IO_DEFAULT ":\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_RED "http://www.bricksync.net/guidefaq.html\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Thank you.\n" );
  }

  DEBUG_SET_TRACKER();

#if BS_ENABLE_LIMITS
  if( ( (context->inventory)->partcount >= BS_LIMITS_MAX_PARTCOUNT ) && !( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ) )
  {
    ioPrintf( &context->output, 0, "\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "As your inventory size exceeds " IO_RED "%d" IO_WHITE " parts and BrickSync is " IO_RED "unregistered" IO_WHITE ", you'll be asked\n", BS_LIMITS_MAX_PARTCOUNT );
    ioPrintf( &context->output, 0, BSMSG_WARNING "a small mathematical puzzle to " IO_CYAN "check" IO_WHITE " for new orders, and " IO_YELLOW "autocheck" IO_WHITE " will be disabled.\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "Please consider supporting BrickSync, type " IO_CYAN "register" IO_WHITE " for more information. thank you!\n" );
    ccSleep( 3000 );
  }
#endif

  /* Fetch BrickSync message if needed */
  if( ( context->checkmessageflag ) && ( context->curtime >= context->messagetime ) )
  {
    bsFetchBrickSyncMessage( context );
    context->messagetime = context->curtime + BS_BRICKSYNC_MESSAGE_INTERVAL;
  }

  DEBUG_SET_TRACKER();

  /* Good to go! */
  ioPrintf( &context->output, 0, "\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "BrickSync state saved. All systems are operational, Captain!\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Type \"" IO_CYAN "help" IO_DEFAULT "\" for the list of commands.\n" );
  ioPrintf( &context->output, 0, "\n" );

  /* We enter the main loop, yaarrr! */
#if BS_ENABLE_ANTIDEBUG
  statusflag = 0;
#endif
  for( ; ; )
  {
#if BS_ENABLE_ANTIDEBUG
    blapplydiff = bsQueryBrickLinkApplyDiff - BS_ANTIDEBUG_BLAPPLY_OFFSET;
    boapplydiff = bsQueryBrickOwlApplyDiff - BS_ANTIDEBUG_BOAPPLY_OFFSET;
#endif

    DEBUG_TRACKER_CLEAR();
    DEBUG_TRACKER_ACTIVITY();
    DEBUG_SET_TRACKER();

#if BS_ENABLE_LIMITS
    if( ( context->contextflags & BS_CONTEXT_FLAGS_AUTOCHECK_MODE ) && ( (context->inventory)->partcount >= BS_LIMITS_MAX_PARTCOUNT ) && !( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ) )
    {
      ioPrintf( &context->output, 0, BSMSG_WARNING "As your inventory size exceeds " IO_RED "%d" IO_WHITE " parts and BrickSync is " IO_RED "unregistered" IO_WHITE ", " IO_CYAN "autocheck" IO_WHITE " has been disabled.\n" IO_DEFAULT "\n", BS_LIMITS_MAX_PARTCOUNT );
      context->contextflags &= ~BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
    }
#endif

    /* Repack inventory to remove gaps if necessary */
    bsxPackInventory( context->inventory );

    /* Update current time */
    context->curtime = time( 0 );

#if BS_ENABLE_MATHPUZZLE
    context->puzzleanswer.i++;
#endif

    /* Update API call count history */
    bsApiHistoryUpdate( context );

    /* Do we have a delayed state update pending? */
    if( context->contextflags & BS_CONTEXT_FLAGS_UPDATED_STATE )
    {
      if( !( bsSaveState( context, 0 ) ) )
      {
        bsFatalError( context );
        return 0;
      }
    }
    if( context->contextflags & BS_CONTEXT_FLAGS_UPDATED_INVENTORY )
    {
      if( !( bsSaveInventory( context, 0 ) ) )
      {
        bsFatalError( context );
        return 0;
      }
    }

    /* Flush buffered I/O on log file */
    ioLogFlush( &context->output );

#if BS_ENABLE_ANTIDEBUG
    bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

#if CC_UNIX
    if( unixSignalSIGUSR1 )
    {
      context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_CHECK | BS_STATE_FLAGS_BRICKOWL_MUST_CHECK;
      ioPrintf( &context->output, 0, BSMSG_INFO IO_MAGENTA "Signal SIGUSR1 received. Services have been flagged for order checks." IO_DEFAULT "\n" );
      unixSignalSIGUSR1 = 0;
    }
#endif

    /* Fetch BrickSync message if needed */
    if( ( context->checkmessageflag ) && ( context->curtime >= context->messagetime ) )
    {
      bsFetchBrickSyncMessage( context );
      context->messagetime = context->curtime + BS_BRICKSYNC_MESSAGE_INTERVAL;
    }

    /* Run disk space check */
    if( context->curtime >= context->diskspacechecktime )
      bsDiskFreeSpaceCheck( context );

    /* Pretty much all operations are disabled when the master mode is running */
    if( !( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MASTER_MODE ) )
    {
      /* Autocheck mode, timer based order checking */
      if( context->contextflags & BS_CONTEXT_FLAGS_AUTOCHECK_MODE )
      {
        if( context->curtime >= context->bricklink.checktime )
        {
          context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_CHECK;
          context->bricklink.checktime = context->curtime + context->bricklink.failinterval;
        }
        if( context->curtime >= context->brickowl.checktime )
        {
          context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_CHECK;
          context->bricklink.checktime = context->curtime + context->bricklink.failinterval;
        }
      }

#if BS_ENABLE_ANTIDEBUG
      context->antidebugcaps &= 0x3ff;
      context->antidebugvalue &= BS_ANTIDEBUG_CONTEXT_VALUE_MASK;
      context->antidebugtimediff = context->antidebugtime1 - context->antidebugtime0;
#endif

      /* BrickLink and/or BrickOwl order check */
      if( context->stateflags & ( BS_STATE_FLAGS_BRICKLINK_MUST_CHECK | BS_STATE_FLAGS_BRICKOWL_MUST_CHECK ) )
      {
#if BS_ENABLE_REGISTRATION
        if( !( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ) )
          bsCheckGlobal( context );
        else
        {
          void (*checkreg)( bsContext *context );
          checkreg = bsCheckRegistered - BS_REGISTRATION_SECRET_OFFSET;
          checkreg += context->registrationsecretoffset;
          checkreg( context );
        }
#else
        bsCheckGlobal( context );
#endif
      }

#if BS_ENABLE_ANTIDEBUG
      blapplydiff += (uint16_t)ccHash32Int32Inline( context->antidebugpuzzle0 + ( context->antidebugvalue & BS_ANTIDEBUG_CONTEXT_VALUE_MASK ) + ( context->antidebugtimediff & BS_ANTIDEBUG_CONTEXT_TIME_MASK ) );
      boapplydiff += (uint16_t)ccHash32Int32Inline( context->antidebugpuzzle1 + bsAntiDebugBoApplyOffset + ccCountBits32( context->antidebugtimediff >> BS_ANTIDEBUG_CONTEXT_TIME_SHIFT ) );
      /* The two ApplyDiff functions have forced 0x10 bytes alignent, we can detect tempering this way */
      if( ( (uintptr_t)blapplydiff | (uintptr_t)boapplydiff ) & 0xf )
      {

#if 0
        /* DEBUG DEBUG DEBUG */
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "State Check: " CC_LLD " " CC_LLD " " CC_LLD " " CC_LLD " " CC_LLD "\n", (long long)context->antidebugpuzzle0, (long long)context->antidebugpuzzle1, (long long)context->antidebugvalue, (long long)context->antidebugtimediff, (long long)bsAntiDebugBoApplyOffset );
        /* DEBUG DEBUG DEBUG */
#endif

        context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE | BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE;
        break;
      }
#endif

#if BS_ENABLE_ANTIDEBUG
      bsAntiDebugCountReset( context );
#endif

      /* Do we have a partial sync waiting for available API calls? */
      if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_PARTIAL_SYNC )
      {
        if( context->bricklink.apihistory.total < context->bricklink.apicountsyncresume )
        {
          ioPrintf( &context->output, 0, BSMSG_INFO "Resuming a partial BrickLink SYNC suspended due to low reserves of daily API calls.\n" );
          context->stateflags &= ~BS_STATE_FLAGS_BRICKLINK_PARTIAL_SYNC;
          context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_SYNC;
        }
      }
      if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_PARTIAL_SYNC )
      {
        ioPrintf( &context->output, 0, BSMSG_INFO "Resuming a partial BrickOwl SYNC suspended due to low reserves of daily API calls.\n" );
        context->stateflags &= ~BS_STATE_FLAGS_BRICKOWL_PARTIAL_SYNC;
        context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
      }

      /* We want to loop until no ApplyDiff/Sync is running */
      do
      {
        workloop = 0;

        context->curtime = time( 0 );

        DEBUG_TRACKER_ACTIVITY();

        /* BrickLink inventory update, when BL_MUST_SYNC is not pending */
        if( ( context->stateflags & ( BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE | BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ) ) == BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE )
        {
          diffinv = context->bricklink.diffinv;
#if BS_ENABLE_ANTIDEBUG
          if( blapplydiff( context, diffinv, 0 ) )
#else
          if( bsQueryBrickLinkApplyDiff( context, diffinv, 0 ) )
#endif
          {
            /* Success, reset any sync delay */
            context->bricklink.syncdelay = BS_SYNC_DELAY_BASE;
            /* Do we need XML output for an update interrupted by low count of free API calls? */
            bsOutputBrickLinkXML( context, diffinv, 1, 0 );
          }
          else
          {
            /* Increase delay before next sync */
            context->bricklink.syncdelay *= BS_SYNC_DELAY_FAIL_FACTOR;
            if( context->bricklink.syncdelay > BS_SYNC_DELAY_MAX )
              context->bricklink.syncdelay = BS_SYNC_DELAY_MAX;
            /* Must sync */
            context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_SYNC;
          }
          /* Import new LotIDs to BrickOwl's pending update queue */
          bsxImportLotIDs( context->brickowl.diffinv, context->inventory );
          /* Empty the diff inventory, it's either fully applied or we need a deep sync */
          bsxEmptyInventory( context->bricklink.diffinv );
          context->stateflags &= ~BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE;
          /* Save updated state flags */
          if( !( bsSaveState( context, 0 ) ) )
          {
            bsFatalError( context );
            return 0;
          }
        }

        /* BrickOwl inventory update, only when none of BO_MUST_SYNC | BL_MUST_UPDATE | BL_MUST_SYNC is pending */
        if( ( context->stateflags & ( BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE | BS_STATE_FLAGS_BRICKOWL_MUST_SYNC | BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE | BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ) ) == BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE )
        {
          diffinv = context->brickowl.diffinv;
#if BS_ENABLE_ANTIDEBUG
          if( boapplydiff( context, diffinv, 0 ) )
#else
          if( bsQueryBrickOwlApplyDiff( context, diffinv, 0 ) )
#endif
          {
            /* Success, reset any sync delay */
            context->brickowl.syncdelay = BS_SYNC_DELAY_BASE;
          }
          else
          {
            /* Increase delay before next sync */
            context->brickowl.syncdelay *= BS_SYNC_DELAY_FAIL_FACTOR;
            if( context->brickowl.syncdelay > BS_SYNC_DELAY_MAX )
              context->brickowl.syncdelay = BS_SYNC_DELAY_MAX;
            /* Must sync */
            context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
          }
          /* Empty the diff inventory, it's either fully applied or we need a deep sync */
          bsxEmptyInventory( context->brickowl.diffinv );
          context->stateflags &= ~BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE;
          /* Save updated state flags */
          if( !( bsSaveState( context, 0 ) ) )
          {
            bsFatalError( context );
            return 0;
          }
        }

#if BS_ENABLE_ANTIDEBUG
        bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

        /* BrickLink inventory deep sync */
        if( ( context->curtime > context->bricklink.synctime ) && ( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ) )
        {
          bsClearBrickLinkXML( context );
          if( bsSyncBrickLink( context, 0 ) )
          {
            context->stateflags &= ~( BS_STATE_FLAGS_BRICKLINK_MUST_SYNC | BS_STATE_FLAGS_BRICKLINK_PARTIAL_SYNC );
            context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE;
            workloop = 1;
          }
          else
          {
            /* We should delay the next SYNC a little? */
            context->bricklink.syncdelay *= BS_SYNC_DELAY_FAIL_FACTOR;
            if( context->bricklink.syncdelay > BS_SYNC_DELAY_MAX )
              context->bricklink.syncdelay = BS_SYNC_DELAY_MAX;
          }
          context->curtime = time( 0 );
          context->bricklink.synctime = context->curtime + context->bricklink.syncdelay;
          context->bricklink.lastsynctime = context->curtime;
        }

        /* BrickOwl inventory deep sync, only when none of BL_MUST_UPDATE | BL_MUST_SYNC is pending */
        if( ( context->curtime > context->brickowl.synctime ) && ( ( context->stateflags & ( BS_STATE_FLAGS_BRICKOWL_MUST_SYNC | BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE | BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ) ) == BS_STATE_FLAGS_BRICKOWL_MUST_SYNC ) )
        {
          if( bsSyncBrickOwl( context, 0 ) )
          {
            context->stateflags &= ~( BS_STATE_FLAGS_BRICKOWL_MUST_SYNC | BS_STATE_FLAGS_BRICKOWL_PARTIAL_SYNC );
            context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE;
            workloop = 1;
          }
          else
          {
            /* We should delay the next SYNC a little? */
            context->brickowl.syncdelay *= BS_SYNC_DELAY_FAIL_FACTOR;
            if( context->brickowl.syncdelay > BS_SYNC_DELAY_MAX )
              context->brickowl.syncdelay = BS_SYNC_DELAY_MAX;
          }
          context->curtime = time( 0 );
          context->brickowl.synctime = context->curtime + context->brickowl.syncdelay;
          context->brickowl.lastsynctime = context->curtime;
        }

      } while( workloop );

#if BS_ENABLE_ANTIDEBUG
      /* Initialize some anti-debugging stuff */
      if( !( antidebuginit( context, &cpuinfo ) ) )
        return 0;
      if( cpuinfo.totalcorecount )
        bsAntiDebugMid( context );
      bsAntiDebugEnd( context );
#endif

    }

    /* Make sure we have double line breaks before input */
    if( !( context->output.linemarker ) )
    {
      if( context->output.endlinecount < 2 )
        ioPrintf( &context->output, IO_MODEBIT_NOLOG, "\n" );
      if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MASTER_MODE )
        ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " is currently " IO_YELLOW "enabled" IO_DEFAULT ". Type \"" IO_CYAN "blmaster off" IO_DEFAULT "\" to synchronize and resume normal operations.\n" );
      ioPrintf( &context->output, IO_MODEBIT_NOLOG | IO_MODEBIT_LINEMARKER, BSMSG_COMMAND );
    }

    /* Detect new commands to know if we should sleep or not */
    actionflag = 0;

    /* Check user input */
    if( !( bsParseInput( context, &actionflag ) ) )
    {
#if BS_ENABLE_ANTIDEBUG
      statusflag = 1;
#endif
      break;
    }

#if BS_ENABLE_ANTIDEBUG
    bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

    DEBUG_SET_TRACKER();

    /* If we have queries pending, only sleep by waiting for network I/O */
    if( actionflag )
      tcpFlush( &context->tcp );
    else
    {
      milliseconds = 100;
      tcpWait( &context->tcp, milliseconds );
    }
    bsFlushTcpProcessHttp( context );
  }

  bsxFreeInventory( context->inventory );
  bsxFreeInventory( context->bricklink.diffinv );
  bsxFreeInventory( context->brickowl.diffinv );

  translationTableEnd( &context->translationtable );

  httpClose( context->bricklink.http );
  httpClose( context->bricklink.webhttp );
  httpClose( context->brickowl.http );

#if BS_ENABLE_ANTIDEBUG
  if( !( statusflag ) )
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Irregular situation detected. Exiting now for safety.\n" );
#endif
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Exiting now.\n" );

  ioLogEnd( &context->output );
  tcpEnd( &context->tcp );
  exclPermStop( exclperm, BS_LOCK_FILE );

  return 1;
}



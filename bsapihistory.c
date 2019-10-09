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


void bsApiHistoryReset( bsContext *context, bsApiHistory *history )
{
  history->basetime = context->curtime;
  memset( history->count, 0, BS_API_HISTORY_SIZE * sizeof(uint16_t) );
  history->total = 0;
  return;
}


static void bsApiHistoryProcess( bsContext *context, bsApiHistory *history )
{
  int i, historyunit;

  DEBUG_SET_TRACKER();

  historyunit = ( context->curtime - history->basetime ) / BS_API_HISTORY_TIMEUNIT;
  if( !( historyunit ) )
    return;
  if( (unsigned)historyunit >= BS_API_HISTORY_SIZE )
  {
    history->basetime = context->curtime;
    memset( history->count, 0, BS_API_HISTORY_SIZE * sizeof(uint16_t) );
    history->total = 0;
    return;
  }

  for( i = BS_API_HISTORY_SIZE - historyunit ; i < BS_API_HISTORY_SIZE ; i++ )
    history->total -= history->count[i];
  memmove( &history->count[historyunit], &history->count[0], ( BS_API_HISTORY_SIZE - historyunit ) * sizeof(uint16_t) );
  for( i = 0 ; i < historyunit ; i++ )
    history->count[i] = 0;
  history->basetime += historyunit * BS_API_HISTORY_TIMEUNIT;

  /* Hide this deep in function calls, but must be set before check() operations */
#if BS_ENABLE_LIMITS
  context->limitinvhardmaxmask = BS_LIMITS_HARDMAX_PARTCOUNT_MASK;
#endif

  return;
}


void bsApiHistoryIncrement( bsContext *context, bsApiHistory *history )
{
  DEBUG_SET_TRACKER();

  history->count[0]++;
  history->total++;
  context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
  return;
}


void bsApiHistoryUpdate( bsContext *context )
{
  DEBUG_SET_TRACKER();

  bsApiHistoryProcess( context, &context->bricklink.apihistory );
  bsApiHistoryProcess( context, &context->brickowl.apihistory );
  return;
}


int bsApiHistoryCountPeriod( bsApiHistory *history, int64_t seconds )
{
  int i, historyunit, count;

  DEBUG_SET_TRACKER();

  historyunit = ( seconds + (BS_API_HISTORY_TIMEUNIT-1) ) / BS_API_HISTORY_TIMEUNIT;
  if( historyunit >= BS_API_HISTORY_SIZE )
    historyunit = BS_API_HISTORY_SIZE;
  count = 0;
  for( i = 0 ; i < historyunit ; i++ )
    count += history->count[i];
  return count;
}



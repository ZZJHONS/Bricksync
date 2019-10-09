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


void bsEnterBrickLinkMasterMode( bsContext *context )
{
  DEBUG_SET_TRACKER();

  if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "The " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " can't be entered while BrickLink has pending updates.\n" );
    return;
  }
  if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_SYNC )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "The " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " can't be entered while BrickLink has a pending deep synchronization.\n" );
    return;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " is now " IO_YELLOW "enabled" IO_DEFAULT ".\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Services will " IO_RED "not" IO_DEFAULT " be checked for new orders. All synchronization operations are suspended.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "You can now edit the " IO_GREEN "BrickLink inventory" IO_DEFAULT " directly as much as you desire.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "When your changes are complete, type \"" IO_CYAN "blmaster off" IO_DEFAULT "\" to synchronize the changes and resume BrickSync operations.\n" );
  context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MASTER_MODE;
  if( !( bsSaveState( context, 0 ) ) )
  {
    bsFatalError( context );
    return;
  }
  return;
}


void bsExitBrickLinkMasterMode( bsContext *context )
{
  int64_t ordertopdate;
  bsxInventory *inv;
  bsOrderList orderlist;
  journalDef journal;

  DEBUG_SET_TRACKER();

  /* Ask math question if appropriate */
#if BS_ENABLE_MATHPUZZLE
  if( (context->inventory)->partcount >= BS_LIMITS_MAX_PARTCOUNT )
  {
    if( !( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ) )
    {
      context->puzzlebundle = malloc( sizeof(bsPuzzleBundle) );
      context->puzzleprevquestiontype = context->puzzlequestiontype;
      if( !( bsPuzzleAsk( context, context->puzzlebundle ) ) )
        goto puzzledone;
    }
  }
#endif

  ioPrintf( &context->output, 0, BSMSG_INFO "We will " IO_GREEN "disable" IO_DEFAULT " the " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT ".\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "All changes applied to the " IO_GREEN "BrickLink inventory" IO_DEFAULT " will be integrated.\n" );

  DEBUG_SET_TRACKER();

  /* Apply all BrickLink orders and fetch the current inventory */
  for( ; ; )
  {
    /* Fetch and apply new BrickLink orders if necessary */
    if( !( bsCheckBrickLink( context ) ) )
      goto networkerror;
    /* Fetch BrickLink inventory */
    if( !( inv = bsQueryBrickLinkFullState( context, &orderlist ) ) )
      goto networkerror;
    ordertopdate = orderlist.topdate;
    blFreeOrderList( &orderlist );
    /* Make sure we are up-to-date on BrickLink orders */
    if( ordertopdate < context->bricklink.ordertopdate )
      break;
  }

  if( !( inv->itemcount - inv->itemfreecount ) )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "The BrickLink inventory appears absolutely " IO_RED "empty" IO_WHITE ". Please make sure your BrickLink store isn't closed.\n" );
    goto networkerror;
  }

  DEBUG_SET_TRACKER();

  bsInventoryFilterOutItems( context, inv );

  /* Fetch any BLID->BOID we might have missing for new lots */
  ioPrintf( &context->output, 0, BSMSG_INFO "Resolving BOIDs for new inventory.\n" );
  if( !( bsQueryBrickOwlLookupBoids( context, inv, BS_RESOLVE_FLAGS_TRYFALLBACK, 0 ) ) )
  {
    bsxFreeInventory( inv );
    ioPrintf( &context->output, 0, BSMSG_ERROR "Failed to resolve BOIDs.\n" );
    goto networkerror;
  }

  journalAlloc( &journal, 4 );

  /* Save a backup of the tracked inventory, with fsync() and journaling */
  bsStoreBackup( context, &journal );

  /* Acquire the new inventory, flag BrickOwl for MUST_SYNC */
  ioPrintf( &context->output, 0, BSMSG_INFO "We have now acquired the BrickLink inventory as our locally tracked inventory.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " is now " IO_GREEN "disabled" IO_DEFAULT ". The galaxy is at peace...\n" );
  context->stateflags &= ~( BS_STATE_FLAGS_BRICKLINK_MASTER_MODE | BS_STATE_FLAGS_BRICKLINK_MUST_CHECK | BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE | BS_STATE_FLAGS_BRICKOWL_MUST_SYNC );
  context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_CHECK | BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
  context->bricklink.lastsynctime = context->curtime;
  context->brickowl.synctime = context->curtime - 1;
  bsxFreeInventory( context->inventory );
  context->inventory = inv;

  /* Update state, with fsync() and journaling */
  if( !( bsSaveState( context, &journal ) ) )
  {
    bsFatalError( context );
    return;
  }

  DEBUG_SET_TRACKER();

  /* Disallow negative quantities */
  bsxClampNegativeInventory( context->inventory );

  /* Save updated inventory with fsync() and journalling */
  if( !( bsxSaveInventory( BS_INVENTORY_TEMP_FILE, context->inventory, 1, 0 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to save inventory file \"" IO_RED "%s" IO_WHITE "\"!\n", BS_INVENTORY_TEMP_FILE );
    bsFatalError( context );
    return;
  }
  journalAddEntry( &journal, BS_INVENTORY_TEMP_FILE, BS_INVENTORY_FILE, 0, 0 );

  /* Apply all the queued changes: backup, order inventory, inventory, state file */
  if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to execute journal!\n" );
    bsFatalError( context );
    return;
  }
  journalFree( &journal );

  DEBUG_SET_TRACKER();

#if BS_ENABLE_MATHPUZZLE
 #if BS_ENABLE_ANTIDEBUG
  if( !( context->contextflags & ( BS_CONTEXT_FLAGS_DUP_REGISTERED | BS_CONTEXT_FLAGS_DUP_DUMMY0 ) ) )
    context->antidebugtimediff |= ( context->puzzlequestiontype == context->puzzleprevquestiontype );
  else
    context->antidebugpuzzle0 &= context->contextflags & BS_CONTEXT_FLAGS_DUP_DUMMY1;
 #endif
  puzzledone:
  if( context->puzzlebundle )
  {
    free( context->puzzlebundle );
    context->puzzlebundle = 0;
  }
#endif

  return;

////

  networkerror:
  ioPrintf( &context->output, 0, BSMSG_ERROR "Exiting the " IO_MAGENTA "BrickLink Master Mode" IO_WHITE " has been aborted.\n" );
#if BS_ENABLE_MATHPUZZLE
  goto puzzledone;
#endif
  return;
}



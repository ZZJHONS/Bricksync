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
#include "bsantidebug.h"


////


static int bsCheckBrickLinkOrder( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue )
{
  int processflag;
  journalDef journal;
  bsxInventory *diskinv, *diffinv, *modinv;
  bsMergeInvStats stats;

  DEBUG_SET_TRACKER();

  bsxRecomputeTotals( inv );

  /* Load the order from disk if it exists, in order to check for an order update */
  ioPrintf( &context->output, 0, BSMSG_INFO "Received BrickLink Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT ", " IO_CYAN "%d" IO_DEFAULT " items in " IO_CYAN "%d" IO_DEFAULT " lots, sale price of " IO_CYAN "%.2f" IO_DEFAULT " %s, order status is %s.\n", (long long)order->id, inv->partcount, inv->itemcount, inv->totalprice, context->storecurrency, bsGetOrderStatusColorString( order ) );

  /* Consolidate all lots from the order's inventory */
  bsxConsolidateInventoryByLotID( inv );

  diskinv = bsBrickLinkLoadOrder( context, order );
  diffinv = 0;
  processflag = 1;
  if( diskinv )
  {
#if BS_ENABLE_DEBUG_SPECIAL
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Existing disk inventory of %d lots\n", inv->itemcount );
#endif

#if 1
    /* Consolidate all lots from the order's inventory on disk */
    /* TODO: Should not be necessary, this is just in case of old versions of the software without consolidation */
    bsxConsolidateInventoryByLotID( diskinv );
#endif
    /* Compute difference between inventories, don't do anything if there has been no change */
    /* This happens all the time when only the "Status" of the order is changed... */
    diffinv = bsxDiffInventoryByLotID( inv, diskinv );
    if( diffinv->itemcount )
      ioPrintf( &context->output, 0, BSMSG_INFO "Inventory of BrickLink Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT " was updated, delta of " IO_CYAN "%d" IO_DEFAULT " items in " IO_CYAN "%d" IO_DEFAULT " lots.\n", (long long)order->id, diffinv->partcount, diffinv->itemcount );
    else
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "Inventory of BrickLink Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT " is unchanged, " IO_WHITE "taking no further action" IO_DEFAULT ".\n", (long long)order->id );
      processflag = 0;
    }
    bsxFreeInventory( diskinv );
#if BS_ENABLE_DEBUG_SPECIAL
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Diff inventory of %d lots\n", diffinv->itemcount );
#endif
  }
  else if( order->date < context->bricklink.orderinitdate )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "Untracked BrickLink Order " IO_GREEN "#"CC_LLD"" IO_WHITE " has been updated.\n", (long long)order->id );
    ioPrintf( &context->output, 0, BSMSG_INFO "This order was already marked as packed or shipped during BrickSync initialization, we do not have the order's previous inventory on disk.\n" );
    if( order->status == BL_ORDER_STATUS_UPDATED )
      ioPrintf( &context->output, 0, BSMSG_WARNING "The order has an " IO_MAGENTA "UPDATED" IO_WHITE " status, manual corrections might be required!\n" );
    else
      ioPrintf( &context->output, 0, BSMSG_INFO "Given the current order status, we assume the order's inventory is unchanged.\n" );
    processflag = 0;
  }

  /* Store the order's content to a temporary file, operation queued in journal */
  if( processflag )
  {
    journalAlloc( &journal, 4 );
    /* Save a backup of the tracked inventory, with fsync() and journaling */
    bsStoreBackup( context, &journal );
    /* We want to apply the diffinv to the local inventory atomicly, then queue updates to BrickOwl */
    /* Between these two steps, have the state flag BrickOwl as requiring sync */
    /* Only update topdate if we have successfully recovered *all* orders after current timestamp */

    /* Save order on disk */
    if( !( bsBrickLinkSaveOrder( context, order, inv, &journal ) ) )
    {
      bsFatalError( context );
      return 0;
    }

    /* Apply whole order or difference to last update */
    modinv = ( diffinv ? diffinv : inv );
    /* Filter out items with '~' in remarks */
    bsInventoryFilterOutItems( context, modinv );

    /* Subtract the content of the order from inventory, queue update to BrickOwl */
    bsxInvertQuantities( modinv );
    bsMergeInv( context, modinv, &stats, BS_MERGE_FLAGS_UPDATE_BRICKOWL );
    context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE;
    /* Update state, with fsync() and journaling */
    if( !( bsSaveState( context, &journal ) ) )
    {
      bsFatalError( context );
      return 0;
    }

    /* Save updated inventory with fsync() and journalling */
    if( !( bsxSaveInventory( BS_INVENTORY_TEMP_FILE, context->inventory, 1, 0 ) ) )
    {
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to save inventory file \"" IO_RED "%s" IO_WHITE "\"!\n", BS_INVENTORY_TEMP_FILE );
      bsFatalError( context );
      return 0;
    }
    journalAddEntry( &journal, BS_INVENTORY_TEMP_FILE, BS_INVENTORY_FILE, 0, 0 );

    /* Apply all the queued changes: backup, order inventory, inventory, state file */
    if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount ) ) )
    {
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to execute journal!\n" );
      bsFatalError( context );
      return 0;
    }
    journalFree( &journal );

    ioPrintf( &context->output, 0, BSMSG_INFO "Tracked inventory adjusted for BrickLink Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT ".\n", (long long)order->id );
  }
  if( diffinv )
    bsxFreeInventory( diffinv );

  return 1;
}


/* Allow people to fetch missing orders */
#define TEMPORARY_WORKAROUND (0)


int bsCheckBrickLink( bsContext *context )
{
  int processretval;
  int64_t basetimestamp, pendingtimestamp;
  bsOrderList orderlist, orderlistcheck;

  DEBUG_SET_TRACKER();

  /* Find a consistent orderlist with a pause in between, making sure the top timestamp has passed */
  for( ; ; )
  {
    basetimestamp = context->bricklink.ordertopdate;


#if TEMPORARY_WORKAROUND


    int condforce;
    if( !( bsQueryBickLinkOrderList( context, &orderlist, 0, 0 ) ) )
      return 0;
    condforce = bsOrderListFilterOutDateConditional( context, &orderlist, 0, basetimestamp, bsBrickLinkCanLoadOrder );
    if( condforce )
      ioPrintf( &context->output, 0, BSMSG_WARNING "Unusual situation detected, forcing a deep order check due to possible BrickLink API bug.\n" );
    else if( orderlist.topdate < context->bricklink.ordertopdate )
    {
      blFreeOrderList( &orderlist );
      ioPrintf( &context->output, 0, BSMSG_INFO "We are up-to-date on BrickLink orders.\n" );
      return 1;
    }

 #if BS_ENABLE_MATHPUZZLE
    if( !( bsPuzzleVerifyAnswer( &context->puzzlesolution, &context->puzzleanswer ) ) )
      context->antidebugpuzzle0 |= (context->inventory)->partcount & context->limitinvhardmaxmask;
 #endif

    /* We sleep to make sure the orderlist's top timestamp has passed */
    ioPrintf( &context->output, 0, BSMSG_INFO "We encountered an update, confirming changes now.\n" );
    ccSleep( 2000 );

    if( !( bsQueryBickLinkOrderList( context, &orderlistcheck, 0, 0 ) ) )
    {
      blFreeOrderList( &orderlist );
      return 0;
    }
    bsOrderListFilterOutDateConditional( context, &orderlist, 0, basetimestamp, bsBrickLinkCanLoadOrder );


#else


    if( !( bsQueryBickLinkOrderList( context, &orderlist, 0, basetimestamp ) ) )
      return 0;
    if( orderlist.topdate < context->bricklink.ordertopdate )
    {
      blFreeOrderList( &orderlist );
      ioPrintf( &context->output, 0, BSMSG_INFO "We are up-to-date on BrickLink orders.\n" );
      return 1;
    }

 #if BS_ENABLE_MATHPUZZLE
    if( !( bsPuzzleVerifyAnswer( &context->puzzlesolution, &context->puzzleanswer ) ) )
      context->antidebugpuzzle0 |= (context->inventory)->partcount & context->limitinvhardmaxmask;
 #endif

    /* We sleep to make sure the orderlist's top timestamp has passed */
    ioPrintf( &context->output, 0, BSMSG_INFO "We encountered an update, confirming changes now.\n" );
    ccSleep( 2000 );

    if( !( bsQueryBickLinkOrderList( context, &orderlistcheck, 0, basetimestamp ) ) )
    {
      blFreeOrderList( &orderlist );
      return 0;
    }


#endif


#if BS_ENABLE_MATHPUZZLE && BS_ENABLE_ANTIDEBUG
    if( ( context->puzzlebundle ) && !( bsPuzzleVerifyAnswer( &(context->puzzlebundle)->solution, &(context->puzzlebundle)->answer ) ) )
      context->antidebugpuzzle1 += context->antidebugcount0;
#endif

    /* Do order lists match after the wait? If yes, break; */
    if( ( orderlist.topdate == orderlistcheck.topdate ) && ( orderlist.topdatecount == orderlistcheck.topdatecount ) )
      break;

    /* A new order arrived sharing the same top timestamp, free stuff and loop over */
    /* TODO: Try so many times before giving up? */
    blFreeOrderList( &orderlist );
    blFreeOrderList( &orderlistcheck );
  }
  blFreeOrderList( &orderlistcheck );

  /* Fetch more information for the new orders */
  bsQueryUpgradeBickLinkOrderList( context, &orderlist, BS_ORDER_INFOLEVEL_DETAILS );

  /* Fetch the inventory of all updates >= our latest topdate */
  basetimestamp = context->bricklink.ordertopdate;
  pendingtimestamp = context->bricklink.ordertopdate;
  processretval = bsFetchBrickLinkOrders( context, &orderlist, basetimestamp, pendingtimestamp, (void *)0, bsCheckBrickLinkOrder );
  blFreeOrderList( &orderlist );

  /* If all orders were received successfully, update state top date */
  if( processretval )
  {
    if( context->bricklink.ordertopdate <= orderlist.topdate )
    {
      context->bricklink.ordertopdate = orderlist.topdate + 1;
      context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
    }
  }

#if BS_ENABLE_ANTIDEBUG
  bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

  return processretval;
}


////


static int bsCheckBrickOwlOrder( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue )
{
  journalDef journal;
  bsMergeInvStats stats;

  DEBUG_SET_TRACKER();

  bsxRecomputeTotals( inv );

  ioPrintf( &context->output, 0, BSMSG_INFO "Received BrickOwl Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT ", " IO_CYAN "%d" IO_DEFAULT " items in " IO_CYAN "%d" IO_DEFAULT " lots, sale price of " IO_CYAN "%.2f" IO_DEFAULT ".\n", (long long)order->id, inv->partcount, inv->itemcount, inv->totalprice, context->storecurrency );

  if( bsBrickOwlCanLoadOrder( context, order ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT " has already been processed, " IO_WHITE "taking no further action" IO_DEFAULT ".\n", (long long)order->id );
    return 1;
  }

  /* Store the order's content to a temporary file, operation queued in journal */
  journalAlloc( &journal, 4 );
  /* Save a backup of the tracked inventory, with fsync() and journaling */
  bsStoreBackup( context, &journal );

  /* We want to apply the diffinv to the local inventory atomicly, then queue updates to BrickLink */
  /* Between these two steps, have the state flag BrickLink as requiring sync */
  /* Only update topdate if we have successfully recovered *all* orders after current timestamp */

  /* Save order on disk */
  if( !( bsBrickOwlSaveOrder( context, order, inv, &journal ) ) )
  {
    bsFatalError( context );
    return 0;
  }

  /* Filter out items with '~' in remarks */
  bsInventoryFilterOutItems( context, inv );
  /* Subtract the content of the order from inventory, queue update to BrickLink */
  bsxInvertQuantities( inv );
  bsMergeInv( context, inv, &stats, BS_MERGE_FLAGS_UPDATE_BRICKLINK );
  context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE;
  /* Update state, with fsync() and journaling */
  if( !( bsSaveState( context, &journal ) ) )
  {
    bsFatalError( context );
    return 0;
  }

  /* Save updated inventory with fsync() and journalling */
  if( !( bsxSaveInventory( BS_INVENTORY_TEMP_FILE, context->inventory, 1, 0 ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to save inventory file \"" IO_RED "%s" IO_WHITE "\"!\n", BS_INVENTORY_TEMP_FILE );
    bsFatalError( context );
    return 0;
  }
  journalAddEntry( &journal, BS_INVENTORY_TEMP_FILE, BS_INVENTORY_FILE, 0, 0 );

  /* Apply all the queued changes: backup, order inventory, inventory, state file */
  if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to execute journal!\n" );
    bsFatalError( context );
    return 0;
  }
  journalFree( &journal );

  ioPrintf( &context->output, 0, BSMSG_INFO "Tracked inventory adjusted for BrickOwl Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT ".\n", (long long)order->id );

  return 1;
}


int bsCheckBrickOwl( bsContext *context )
{
  int processretval;
  int64_t basetimestamp, pendingtimestamp;
  bsOrderList orderlist, orderlistcheck;

  DEBUG_SET_TRACKER();

  /* Find a consistent orderlist with a pause in between, making sure the top timestamp has passed */
  for( ; ; )
  {
    basetimestamp = context->brickowl.ordertopdate;
    if( !( bsQueryBickOwlOrderList( context, &orderlist, basetimestamp, basetimestamp ) ) )
      return 0;
    if( orderlist.topdate < context->brickowl.ordertopdate )
    {
      boFreeOrderList( &orderlist );
      ioPrintf( &context->output, 0, BSMSG_INFO "We are up-to-date on BrickOwl orders.\n" );
      return 1;
    }

#if BS_ENABLE_MATHPUZZLE
    if( !( bsPuzzleVerifyAnswer( &context->puzzlesolution, &context->puzzleanswer ) ) )
      context->antidebugpuzzle0 |= (context->inventory)->partcount & context->limitinvhardmaxmask;
#endif

    /* We sleep to make sure the orderlist's top timestamp has passed */
    ioPrintf( &context->output, 0, BSMSG_INFO "We encountered an update, confirming changes now.\n" );
    ccSleep( 2000 );

    if( !( bsQueryBickOwlOrderList( context, &orderlistcheck, basetimestamp, basetimestamp ) ) )
    {
      boFreeOrderList( &orderlist );
      return 0;
    }

#if BS_ENABLE_MATHPUZZLE && BS_ENABLE_ANTIDEBUG
    if( ( context->puzzlebundle ) && !( bsPuzzleVerifyAnswer( &(context->puzzlebundle)->solution, &(context->puzzlebundle)->answer ) ) )
      context->antidebugpuzzle1 += context->antidebugcount1;
#endif

    /* Do order lists match after the wait? If yes, break; */
    if( ( orderlist.topdate == orderlistcheck.topdate ) && ( orderlist.topdatecount == orderlistcheck.topdatecount ) )
      break;

    /* A new order arrived sharing the same top timestamp, free stuff and loop over */
    /* TODO: Try so many times before giving up? */
    boFreeOrderList( &orderlist );
    boFreeOrderList( &orderlistcheck );
  }
  boFreeOrderList( &orderlistcheck );

  /* Fetch more information for the new orders */
  bsQueryUpgradeBickOwlOrderList( context, &orderlist, BS_ORDER_INFOLEVEL_DETAILS );

  /* Fetch the inventory of all updates >= our latest topdate */
  basetimestamp = context->brickowl.ordertopdate;
  pendingtimestamp = context->brickowl.ordertopdate;
  processretval = bsFetchBrickOwlOrders( context, &orderlist, basetimestamp, pendingtimestamp, (void *)0, bsCheckBrickOwlOrder );
  boFreeOrderList( &orderlist );

  /* If all orders were received successfully, update state top date */
  if( processretval )
  {
    if( context->brickowl.ordertopdate <= orderlist.topdate )
    {
      context->brickowl.ordertopdate = orderlist.topdate + 1;
      context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
    }
  }

#if BS_ENABLE_ANTIDEBUG
  bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

  return processretval;
}


////


void bsCheckBrickLinkState( bsContext *context )
{
  int successflag;

  DEBUG_SET_TRACKER();

  if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_CHECK )
  {
    successflag = bsCheckBrickLink( context );
    context->curtime = time( 0 );
    context->bricklink.checktime = context->curtime + ( successflag ? context->bricklink.pollinterval : context->bricklink.failinterval );
    context->bricklink.lastchecktime = context->curtime;
    context->stateflags &= ~BS_STATE_FLAGS_BRICKLINK_MUST_CHECK;
    if( ( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ) && ( context->curtime < context->bricklink.synctime ) )
      ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink is flagged MUST_SYNC for deep synchronization in " IO_GREEN "%d" IO_DEFAULT " seconds.\n", (int)( context->bricklink.synctime - context->curtime ) );
  }
  return;
}


void bsCheckBrickOwlState( bsContext *context )
{
  int successflag;

  DEBUG_SET_TRACKER();

  if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_CHECK )
  {
    successflag = bsCheckBrickOwl( context );
    context->curtime = time( 0 );
    context->brickowl.checktime = context->curtime + ( successflag ? context->brickowl.pollinterval : context->brickowl.failinterval );
    context->brickowl.lastchecktime = context->curtime;
    context->stateflags &= ~BS_STATE_FLAGS_BRICKOWL_MUST_CHECK;
    if( ( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_SYNC ) && ( context->curtime < context->brickowl.synctime ) )
      ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl is flagged MUST_SYNC for deep synchronization in " IO_GREEN "%d" IO_DEFAULT " seconds.\n", (int)( context->brickowl.synctime - context->curtime ) );
  }
  return;
}


////


void bsCheckGlobal( bsContext *context )
{
  DEBUG_SET_TRACKER();

#if BS_ENABLE_MATHPUZZLE
  if( (context->inventory)->partcount >= BS_LIMITS_MAX_PARTCOUNT )
  {
    context->puzzlebundle = malloc( sizeof(bsPuzzleBundle) );
    context->puzzleprevquestiontype = context->puzzlequestiontype;
    if( !( bsPuzzleAsk( context, context->puzzlebundle ) ) )
    {
      context->stateflags &= ~( BS_STATE_FLAGS_BRICKLINK_MUST_CHECK | BS_STATE_FLAGS_BRICKOWL_MUST_CHECK );
      goto puzzledone;
    }
  }
#endif

  /* BrickLink order check */
  bsCheckBrickLinkState( context );

  /* BrickOwl order check */
  bsCheckBrickOwlState( context );

#if BS_ENABLE_MATHPUZZLE
 #if BS_ENABLE_ANTIDEBUG
  context->antidebugtimediff |= ( context->puzzlequestiontype == context->puzzleprevquestiontype );
 #endif
  puzzledone:
  if( context->puzzlebundle )
  {
    free( context->puzzlebundle );
    context->puzzlebundle = 0;
  }
#endif
  return;
}


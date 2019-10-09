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


static int bsInitProcessBrickLinkOrder( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue )
{
  journalDef *journal;
  journal = (journalDef *)uservalue;
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Received inventory of BrickLink Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT "\n", (long long)order->id );
  /* Store the order's content to a temporary file, operation queued in journal */
  if( !( bsBrickLinkSaveOrder( context, order, inv, journal ) ) )
    return 0;
  return 1;
}


/* Clean initialization from scratch, run this when we have no state saved */
int bsInitBrickLink( bsContext *context )
{
  int retval;
  int64_t basetimestamp, pendingtimestamp;
  bsOrderList orderlist;
  bsxInventory *inv;
  journalDef journal;

  DEBUG_SET_TRACKER();

  /* Get past orders in respect to inventory */
  bsxEmptyInventory( context->inventory );
  inv = bsQueryBrickLinkFullState( context, &orderlist );
  if( !( inv ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to fetch inventory from BrickLink.\n" );
    return 0;
  }
  bsxFreeInventory( context->inventory );
  context->inventory = inv;

  /* BrickLink inventory is now the tracked inventory */
  if( bsxSaveInventory( BS_INVENTORY_FILE, context->inventory, 0, 0 ) )
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "We saved the BrickLink inventory as our locally tracked inventory.\n" );
  else
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to save inventory file as \"" IO_RED "%s" IO_WHITE "\"!\n", BS_INVENTORY_FILE );
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Make sure we have write access to the path \"" IO_RED "%s" CC_DIR_SEPARATOR_STRING "%s" IO_WHITE "\".\n", context->cwd, BS_INVENTORY_FILE );
    return 0;
  }

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Fetching BrickLink Orders.\n" );
#endif

  /* Fetch orders */
  basetimestamp = orderlist.topdate - BS_INIT_ORDER_FETCH_DATE;
  pendingtimestamp = orderlist.topdate - BS_INIT_ORDER_PENDING_DATE;
  context->bricklink.orderinitdate = basetimestamp;
  journalAlloc( &journal, 64 );
  if( !( bsFetchBrickLinkOrders( context, &orderlist, basetimestamp, pendingtimestamp, (void *)&journal, bsInitProcessBrickLinkOrder ) ) )
    goto error;

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "We have retrieved all non-shipped BrickLink orders, and all orders from the past %d days.\n", (int)(BS_INIT_ORDER_FETCH_DATE/(24*60*60)) );
#endif

  /* Execute the journal's pending rename() operations for all BrickLink orders */
  retval = journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount );

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Pending journal executed.\n" );
#endif

  journalFree( &journal );
  if( !( retval ) )
    goto error2;

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Journal freed.\n" );
#endif

  /* Update context order timestamp */
  /* bsQueryBrickLinkFullState() made sure the order list was at least 1 second old */
  if( context->bricklink.ordertopdate <= orderlist.topdate )
    context->bricklink.ordertopdate = orderlist.topdate + 1;
  blFreeOrderList( &orderlist );

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "BrickLink state of orders updated.\n" );
#endif

  context->bricklink.lastchecktime = context->curtime;
  context->bricklink.lastsynctime = context->curtime;

  return 1;

  error:
  journalFree( &journal );
  error2:
  blFreeOrderList( &orderlist );
  return 0;
}


////


static int bsInitProcessBrickOwlOrder( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue )
{
  journalDef *journal;
  journal = (journalDef *)uservalue;
  ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Received inventory of BrickOwl Order " IO_GREEN "#"CC_LLD"" IO_DEFAULT "\n", (long long)order->id );
  /* Store the order's content to a temporary file, operation queued in journal */
  if( !( bsBrickOwlSaveOrder( context, order, inv, journal ) ) )
    return 0;
  return 1;
}


/* Discover BOID<->BLID matches between BrickOwl inventory and stock by looking up lotIDs and translation database */
static void bsInitMatchBrickOwlInventory( bsContext *context, bsxInventory *stockinv, bsxInventory *inv )
{
  int itemindex;
  bsxItem *item, *stockitem;

  DEBUG_SET_TRACKER();

  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;

    /* If ID is already resolved, nothing to do */
    if( item->id )
      continue;

    stockitem = 0;
    if( item->lotid >= 0 )
    {
      stockitem = bsxFindLotID( stockinv, item->lotid );
      if( stockitem )
      {
        /* Set stock's BOID and register the BLID<->BOID translation */
        stockitem->boid = item->boid;
        translationTableRegisterEntry( &context->translationtable, stockitem->typeid, stockitem->id, item->boid );
      }
    }
  }

  return ;
}


/* Set typeIDs, IDs, lotIDs for inventory */
static void bsInitFinalizeBrickOwlInventory( bsContext *context, bsxInventory *stockinv, bsxInventory *inv )
{
  char bltypeid;
  int itemindex;
  bsxItem *item, *stockitem;
  char blid[24];

  DEBUG_SET_TRACKER();

  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;

    blid[0] = 0;
    bltypeid = translationBOIDtoBLID( &context->translationtable, item->boid, blid, sizeof(blid) );
    if( !( bltypeid ) )
      continue;
    bsxSetItemId( item, blid, strlen(blid) );
    item->typeid = bltypeid;

    /* Try to resolve lotID */
    if( item->lotid == -1 )
    {
      stockitem = bsxFindItem( stockinv, item->typeid, item->id, item->colorid, item->condition );
      if( stockitem )
        item->lotid = stockitem->lotid;
    }
  }

  return;
}


static int bsInitBrickOwlAskPermission( bsContext *context, bsSyncStats *stats, int emptywarning )
{
  int retval, errorcount;
  char *useranswer;

  DEBUG_SET_TRACKER();

  /* Inventory retrieved, now decide what to do */
  useranswer = 0;
  retval = 0;
  for( errorcount = 0 ; ; errorcount++ )
  {
    ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_DEFAULT "\nBrickSync requires your authorization to synchronize the BrickOwl inventory.\n" );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "\nSee the latest log in \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING "%s" IO_DEFAULT "\" for detailled information.\n", context->cwd, BS_LOG_DIR );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "\nOn the BrickOwl inventory, we will:\n" );
    bsSyncPrintSummary( context, stats, 1 );
    if( emptywarning )
    {
      ioPrintf( &context->output, 0, BSMSG_WARNING "We are synchronizing everywhere an " IO_RED "empty" IO_WHITE " inventory. Please make sure this is intended.\n" );
      ioPrintf( &context->output, 0, BSMSG_WARNING "If your BrickLink store is just closed: abort the current operation, open your BrickLink store, and restart BrickSync's initialization.\n" );
    }
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" IO_WHITE "Do you agree to apply these changes to BrickOwl?" IO_DEFAULT " (yes/no) : " );
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "\n" );
    ioLogFlush( &context->output );

    /* Get answer from user */
    if( useranswer )
      free( useranswer );
    for( useranswer = 0 ; !( useranswer ) ; )
    {
      DEBUG_TRACKER_ACTIVITY();
      useranswer = ioStdinReadAlloc( 60 );
    }

    if( ccStrLowCmpWord( useranswer, "no" ) || ccStrLowCmpWord( useranswer, "n" ) || ccStrLowCmpWord( useranswer, "nooo" ) || ccStrLowCmpWord( useranswer, "nevar" ) || ccStrLowCmpWord( useranswer, "abort" ) )
    {
      ioPrintf( &context->output, 0, "\nNo?... You're the Captain, give us the word whenever ready!\n" );
      ioPrintf( &context->output, 0, "Exiting now.\n" );
      break;
    }
    if( ccStrLowCmpWord( useranswer, "yes" ) || ccStrLowCmpWord( useranswer, "y" ) || ccStrLowCmpWord( useranswer, "yeah" ) || ccStrLowCmpWord( useranswer, "yup" ) || ccStrLowCmpWord( useranswer, "yaarr" ) || ccStrLowCmpWord( useranswer, "sure" ) )
    {
      retval = 1;
      break;
    }

    if( errorcount >= 5 )
    {
      ioPrintf( &context->output, 0, "\nI give up. Exiting now.\n" );
      break;
    }
    if( !( errorcount & 0x1 ) )
      ioPrintf( &context->output, 0, "\nI have no idea what you are saying. Express yourself clearly!\n" );
    else
      ioPrintf( &context->output, 0, "\nWhat? English, do you speak it? Or type it, whatever.\n" );
    ccSleep( 2000 );
  }

  free( useranswer );
  return retval;
}


int bsInitBrickOwl( bsContext *context )
{
  int errorcount, retryflag, applydiffsuccess, retval, emptywarning, fetchtrycount;
  bsOrderList orderlist;
  int lookupcounts[4];
  int64_t basetimestamp;
  journalDef journal;
  bsxInventory *inv, *deltainv;
  bsSyncStats deltastats;

  DEBUG_SET_TRACKER();

#if BS_ENABLE_DEBUG_OUTPUT
  ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Beginning BrickOwl Initialization.\n" );
#endif

  /* We want to loop in case ApplyDiff() fails due to a NOREPLY error on a !RETRY query */
  errorcount = 0;
  deltainv = 0;
  for( ; ; errorcount++ )
  {
#if BS_ENABLE_DEBUG_OUTPUT
    ioPrintf( &context->output, IO_MODEBIT_FLUSH | IO_MODEBIT_LOGONLY, BSMSG_DEBUG "Get BrickOwl full state.\n" );
#endif

    /* Get past orders in respect to inventory */
    if( !( inv = bsQueryBrickOwlFullState( context, &orderlist, 0 ) ) )
      return 0;
    for( fetchtrycount = 0 ; orderlist.ordercount == BS_FETCH_BRICKOWL_ORDERLIST_MAXIMUM ; fetchtrycount++ )
    {
      bsxFreeInventory( inv );
      boFreeOrderList( &orderlist );
      basetimestamp = orderlist.topdate + 1;
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "List of past orders exceeds BrickOwl listing limit, attempting to resolve initialization timestamp from " CC_LLD ".\n", (long long)basetimestamp );
      if( !( inv = bsQueryBrickOwlFullState( context, &orderlist, basetimestamp ) ) )
        return 0;
      if( !( orderlist.topdate ) )
        orderlist.topdate = basetimestamp;
      if( fetchtrycount >= 16 )
      {
        ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to resolve the timestamp of the latest BrickOwl order.\n" );
        goto error2;
      }
    }

    /* Resolve some BLID<->BOID translation by matching BrickLink lotIDs */
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Examining LotIDs matches between inventories.\n" );
    bsInitMatchBrickOwlInventory( context, context->inventory, inv );

    /* Update context order timestamp */
    /* bsQueryBrickOwlDiffState() made sure the order list was at least 1 second old */
    context->brickowl.orderinitdate = orderlist.topdate;

    /* Lookup and/or resolve all missing BOIDs from stock inventory */
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Resolving BOIDs for all unknown BLIDs.\n" );
    bsResolveBrickOwlBoids( context, context->inventory, 0 );
    if( !( bsQueryBrickOwlLookupBoids( context, context->inventory, BS_RESOLVE_FLAGS_SKIPBOLOTID | BS_RESOLVE_FLAGS_TRYFALLBACK, lookupcounts ) ) )
      goto error2;
    if( lookupcounts[0] )
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Resolved BOIDs for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", lookupcounts[0], lookupcounts[1] );
    if( lookupcounts[2] )
      ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO "Failed to resolve BOIDs for " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", lookupcounts[2], lookupcounts[3] );

    /* Finalize BrickOwl inventory by resolving missing typeIDs, IDs and lotIDs */
    bsInitFinalizeBrickOwlInventory( context, context->inventory, inv );

    /* Import any missing BrickOwl Lot IDs */
    bsxImportOwlLotIDs( context->inventory, inv );

    /* Init stuff */
    basetimestamp = orderlist.topdate - BS_INIT_ORDER_FETCH_DATE;
    journalAlloc( &journal, 64 );
    context->brickowl.orderinitdate = basetimestamp;
#if 0
    int64_t pendingtimestamp;
    pendingtimestamp = orderlist.topdate - BS_INIT_ORDER_PENDING_DATE;
    /* Fetch orders */
    if( !( bsFetchBrickOwlOrders( context, &orderlist, basetimestamp, pendingtimestamp, (void *)&journal, bsInitProcessBrickOwlOrder ) ) )
      goto error;
#endif

    /* Compute inventory difference, delta = BrickLink - BrickOwl */
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_FLUSH, "LOG: Listing all changes to be pushed to BrickOwl.\n" );
    deltainv = bsSyncComputeDeltaInv( context, context->inventory, inv, &deltastats, BS_SYNC_DELTA_MODE_BRICKOWL );
    bsxFreeInventory( inv );
    inv = 0;

    /* Ask for the user's permission to proceed */
    emptywarning = 0;
    if( !( (context->inventory)->itemcount - (context->inventory)->itemfreecount ) )
      emptywarning = 1;
    if( !( bsInitBrickOwlAskPermission( context, &deltastats, emptywarning ) ) )
      goto error;

    DEBUG_SET_TRACKER();

    /* Push updates matching the computed "deltainv" */
    applydiffsuccess = bsQueryBrickOwlApplyDiff( context, deltainv, &retryflag );
    bsxFreeInventory( deltainv );
    deltainv = 0;
    if( applydiffsuccess )
      break;
    else if( !( retryflag ) && ( errorcount++ >= 3 ) )
      goto error;
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "BrickOwl ApplyDiff() did not complete successfully, starting over.\n" );
    journalFree( &journal );
    boFreeOrderList( &orderlist );
  }

  /* Execute the journal's pending rename() operations for all BrickLink orders */
  retval = journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount );
  journalFree( &journal );
  if( !( retval ) )
    goto error3;
  if( context->brickowl.ordertopdate <= orderlist.topdate )
    context->brickowl.ordertopdate = orderlist.topdate + 1;
  boFreeOrderList( &orderlist );

  context->brickowl.lastchecktime = context->curtime;
  context->brickowl.lastsynctime = context->curtime;

  return 1;

  error:
  journalFree( &journal );
  error2:
  boFreeOrderList( &orderlist );
  error3:
  if( inv )
    bsxFreeInventory( inv );
  if( deltainv )
    bsxFreeInventory( deltainv );
  return 0;
}



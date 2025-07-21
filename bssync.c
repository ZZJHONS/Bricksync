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


static void bsSyncLogItemString( char *buffer, int buffersize, bsxItem *item )
{
  int printsize;

  buffer[0] = 0;
  if( item->id )
  {
    printsize = snprintf( buffer, buffersize, ", ID %s", item->id );
#if CC_WINDOWS
    if( ( printsize == -1 ) || ( printsize == buffersize ) )
    {
      buffer[buffersize-1] = 0;
      return;
    }
#endif
    buffersize -= printsize;
    buffer += printsize;
    if( buffersize <= 0 )
      return;
  }
  if( item->name )
  {
    printsize = snprintf( buffer, buffersize, ", Name \"%s\"", item->name );
#if CC_WINDOWS
    if( ( printsize == -1 ) || ( printsize == buffersize ) )
    {
      buffer[buffersize-1] = 0;
      return;
    }
#endif
    buffersize -= printsize;
    buffer += printsize;
    if( buffersize <= 0 )
      return;
  }
  if( item->condition )
  {
    printsize = snprintf( buffer, buffersize, ", %s", ( item->condition == 'N' ? "New" : "Used" ) );
#if CC_WINDOWS
    if( ( printsize == -1 ) || ( printsize == buffersize ) )
    {
      buffer[buffersize-1] = 0;
      return;
    }
#endif
    buffersize -= printsize;
    buffer += printsize;
    if( buffersize <= 0 )
      return;
  }
  if( item->colorid )
  {
    printsize = snprintf( buffer, buffersize, ", Color %d", item->colorid );
#if CC_WINDOWS
    if( ( printsize == -1 ) || ( printsize == buffersize ) )
    {
      buffer[buffersize-1] = 0;
      return;
    }
#endif
    buffersize -= printsize;
    buffer += printsize;
    if( buffersize <= 0 )
      return;
  }
  if( item->quantity )
  {
    printsize = snprintf( buffer, buffersize, ", Quantity %d", item->quantity );
#if CC_WINDOWS
    if( ( printsize == -1 ) || ( printsize == buffersize ) )
    {
      buffer[buffersize-1] = 0;
      return;
    }
#endif
    buffersize -= printsize;
    buffer += printsize;
    if( buffersize <= 0 )
      return;
  }
  return;
}


////


void bsSyncAddDeltaItem( bsContext *context, bsxInventory *deltainv, bsxItem *stockitem, bsxItem *item, char *itemstringbuffer, bsSyncStats *stats, int deltamode )
{
  int updateflags;
  bsxItem *deltaitem;

  /* Find what needs an update */
  updateflags = 0;
  if( item->quantity != stockitem->quantity )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Adjust quantity by %+d%s\n", stockitem->quantity - item->quantity, itemstringbuffer );
    if( ( context->retainemptylotsflag ) && !( stockitem->quantity ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_STOCKROOM;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Move to Stockroom%s\n", itemstringbuffer );
    }
  }
  if( !( bsInvPriceEqual( item->price, stockitem->price ) ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_PRICE;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set Price from %.3f to %.3f%s\n", item->price, stockitem->price, itemstringbuffer );
  }
  if( !( ccStrCmpEqualTest( item->comments, stockitem->comments ) ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_COMMENTS;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set Comments from \"%s\" to \"%s\"%s\n", item->comments, stockitem->comments, itemstringbuffer );
  }
  if( !( ccStrCmpEqualTest( item->remarks, stockitem->remarks ) ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_REMARKS;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set Remarks from \"%s\" to \"%s\"%s\n", item->remarks, stockitem->remarks, itemstringbuffer );
  }
  if( ( item->bulk != stockitem->bulk ) && ( ( item->bulk >= 2 ) || ( stockitem->bulk >= 2 ) ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_BULK;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set Bulk Quantity from %d to %d%s\n", item->bulk, stockitem->bulk, itemstringbuffer );
  }
  if( ( stockitem->mycost > 0.0001 ) && !( bsInvPriceEqual( item->mycost, stockitem->mycost ) ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_MYCOST;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set MyCost from %.3f to %.3f%s\n", item->mycost, stockitem->mycost, itemstringbuffer );
  }
  if( ( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL ) && ( stockitem->usedgrade ) && ( item->usedgrade != stockitem->usedgrade ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_USEDGRADE;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set UsedGrade from %c to %c%s\n", item->usedgrade, stockitem->usedgrade, itemstringbuffer );
  }
  if( !( bsInvItemTierEqual( item, stockitem ) ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_TIERPRICES;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set TierPrices from [[%d,%.3f],[%d,%.3f],[%d,%.3f]] to [[%d,%.3f],[%d,%.3f],[%d,%.3f]]%s\n", item->tq1, item->tp1, item->tq2, item->tp2, item->tq3, item->tp3, stockitem->tq1, stockitem->tp1, stockitem->tq2, stockitem->tp2, stockitem->tq3, stockitem->tp3, itemstringbuffer );
  }
  if ( ( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL ) && ( stockitem->sale != item->sale ) )
  {
    updateflags |= BSX_ITEM_XFLAGS_UPDATE_SALE_PERCENT;
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set Sale Percent from %d to %d%s\n", item->sale, stockitem->sale, itemstringbuffer );
  }


  /* TODO: Remove this once we made sure we are properly importing OwlLotIDs everywhere! */
#if 1
  /* Update OwlLotIDs if necessary */
  if( ( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL ) && ( item->bolotid != -1 ) && ( item->bolotid != stockitem->bolotid ) )
  {
    stockitem->bolotid = item->bolotid;
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
  }
#endif
  /* TODO: Remove this once we made sure we are properly importing OwlLotIDs everywhere! */


  /* Perfect match */
  if( !( updateflags ) )
  {
    stats->match_partcount += item->quantity;
    stats->match_lotcount++;
    return;
  }

  /* Set delta */
  deltaitem = bsxAddCopyItem( deltainv, stockitem );
  deltaitem->lotid = item->lotid;
  deltaitem->boid = item->boid;
  deltaitem->bolotid = item->bolotid;
  deltaitem->quantity = stockitem->quantity - item->quantity;
  if (updateflags & BSX_ITEM_XFLAGS_UPDATE_SALE_PERCENT)
    deltaitem->sale = stockitem->sale;
  deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | updateflags;
  /* Increment counters */
  if( deltaitem->quantity > 0 )
  {
    stats->missing_partcount += deltaitem->quantity;
    stats->missing_lotcount++;
  }
  else if( deltaitem->quantity < 0 )
  {
    stats->extra_partcount -= deltaitem->quantity;
    stats->extra_lotcount++;
  }
  stats->match_partcount += CC_MIN( stockitem->quantity, item->quantity );
  stats->match_lotcount++;

  /* Track updatedata updates */
  if( updateflags & ( BSX_ITEM_XFLAGS_UPDATE_PRICE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS | BSX_ITEM_XFLAGS_UPDATE_REMARKS | BSX_ITEM_XFLAGS_UPDATE_BULK | BSX_ITEM_XFLAGS_UPDATE_MYCOST | BSX_ITEM_XFLAGS_UPDATE_USEDGRADE | BSX_ITEM_XFLAGS_UPDATE_TIERPRICES | BSX_ITEM_XFLAGS_UPDATE_SALE_PERCENT ) )
  {
    stats->updatedata_partcount += stockitem->quantity;
    stats->updatedata_lotcount++;
  }

  return;
}


/* Compute the delta inventory, changes necessary for "inv" to become "stockinv" */
bsxInventory *bsSyncComputeDeltaInv( bsContext *context, bsxInventory *stockinv, bsxInventory *inv, bsSyncStats *stats, int deltamode )
{
  int itemindex, stockitemindex, updateflags;
  size_t bitindex;
  bsxItem *item, *stockitem, *deltaitem;
  mmBitMap stockmap;
  bsxInventory *deltainv;
  char itemstringbuffer[512];

  DEBUG_SET_TRACKER();

  memset( stats, 0, sizeof(bsSyncStats) );
  deltainv = bsxNewInventory();
  mmBitMapInit( &stockmap, stockinv->itemcount, 0 );
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;

    bsSyncLogItemString( itemstringbuffer, sizeof(itemstringbuffer), item );

    /* Skip items flagged by '~' */
    if( bsInvItemFilterFlag( context, item ) )
    {
      stats->skipflag_partcount += item->quantity;
      stats->skipflag_lotcount++;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Skip item filtered out by '~'%s\n", itemstringbuffer );
      continue;
    }

    /* Skip empty lots, what else can we do? */
    if( !( item->quantity ) )
    {
#if 0
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Skip zero quantity item%s\n", itemstringbuffer );
#endif
      continue;
    }

    /* Skip items with no BOIDs, unknown to BrickOwl */
    if( ( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL ) && ( item->boid == -1 ) )
    {
      stats->skipunknown_partcount += item->quantity;
      stats->skipunknown_lotcount++;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Skip unknown item%s\n", itemstringbuffer );
      continue;
    }

    /* Acquire equivalent item from stock inventory */
    stockitem = 0;
    if( item->lotid >= 0 )
    {
      if( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL )
        stockitem = bsxFindBoidColorConditionLotID( stockinv, item->boid, item->colorid, item->condition, item->lotid );
      else
        stockitem = bsxFindLotID( stockinv, item->lotid );
    }
    /* Resolve by OwlLotID if any, for not-yet-created LotIDs only */
    if( !( stockitem ) && ( item->bolotid >= 0 ) )
    {
      stockitem = bsxFindOwlLotID( stockinv, item->bolotid );
      if( ( stockitem ) && ( stockitem->lotid >= 0 ) )
        stockitem = 0;
    }
#if 0
    /* Do item match for BrickOwl? BrickLink should have exact LotID matches */
    /* NO: We need to recreate lots to update LotID on BrickOwl */
    if( !( stockitem ) && ( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL ) )
      stockitem = bsxFindMatchItem( stockinv, item );
#endif

    /* No match, lot is all by itself */
    if( !( stockitem ) )
    {
      /* Orphan lot to be deleted */
      deltaitem = bsxAddCopyItem( deltainv, item );
      bsxSetItemQuantity( deltainv, deltaitem, -item->quantity );
      if( context->retainemptylotsflag )
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY | BSX_ITEM_XFLAGS_UPDATE_STOCKROOM;
      else
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
      stats->deleteorphan_partcount += item->quantity;
      stats->deleteorphan_lotcount++;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Delete orphan item%s\n", itemstringbuffer );
      continue;
    }

    /* Ensure stockitem has unique ExtID */
    if( stockitem->extid == -1 )
      bsItemSetUniqueExtID( context, stockinv, stockitem );

    /* Catch duplicate lots referencing the same bl_lot_id */
    stockitemindex = bsxGetItemListIndex( stockinv, stockitem );
    if( mmBitMapDirectGet( &stockmap, stockitemindex ) )
    {
      deltaitem = bsxAddCopyItem( deltainv, item );
      bsxSetItemQuantity( deltainv, deltaitem, -item->quantity );
      if( context->retainemptylotsflag )
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY | BSX_ITEM_XFLAGS_UPDATE_STOCKROOM;
      else
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
      stats->deleteduplicate_partcount += item->quantity;
      stats->deleteduplicate_lotcount++;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Delete duplicate BLID item%s\n", itemstringbuffer );
      continue;
    }

    /* Flag stock lot as used */
    mmBitMapDirectSet( &stockmap, stockitemindex );

    /* Add deltaitem */
    bsSyncAddDeltaItem( context, deltainv, stockitem, item, itemstringbuffer, stats, deltamode );
  }

  /* Add stock items that weren't found in the inventory */
  if( stockinv->itemcount )
  {
    for( itemindex = 0 ; ; )
    {
      if( !( mmBitMapFindClear( &stockmap, itemindex, stockinv->itemcount - 1, &bitindex ) ) )
        break;
      itemindex = (int)bitindex;
      mmBitMapDirectSet( &stockmap, itemindex );
      stockitem = &stockinv->itemlist[ itemindex ];
      if( stockitem->flags & BSX_ITEM_FLAGS_DELETED )
        continue;
      bsSyncLogItemString( itemstringbuffer, sizeof(itemstringbuffer), stockitem );

      /* Ensure stockitem has unique ExtID */
      if( stockitem->extid == -1 )
        bsItemSetUniqueExtID( context, stockinv, stockitem );

      if( ( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL ) && ( stockitem->boid == -1 ) )
      {
        stats->skipunknown_partcount += stockitem->quantity;
        stats->skipunknown_lotcount++;
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Skip unknown item%s\n", itemstringbuffer );
        continue;
      }

      if( stockitem->quantity == 0 )
      {
#if 0
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Skip zero quantity item%s\n", itemstringbuffer );
#endif
        continue;
      }

      if( deltamode == BS_SYNC_DELTA_MODE_BRICKOWL )
        item = bsxFindBoidColorConditionLotID( inv, stockitem->boid, stockitem->colorid, stockitem->condition, stockitem->lotid );
      else
        item = bsxFindLotID( inv, stockitem->lotid );

      if( item )
        bsSyncAddDeltaItem( context, deltainv, stockitem, item, itemstringbuffer, stats, deltamode );
      else
      {
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Create new item%s\n", itemstringbuffer );
        stats->missing_partcount += stockitem->quantity;
        stats->missing_lotcount++;
        deltaitem = bsxAddCopyItem( deltainv, stockitem );
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_CREATE;
      }
    }
  }

  mmBitMapFree( &stockmap );

  return deltainv;
}


////


int bsMergeInv( bsContext *context, bsxInventory *inv, bsMergeInvStats *stats, int mergeflags )
{
  int itemindex, deleteflag;
  bsxItem *item, *stockitem, *deltaitem;
  bsxInventory *stockinv;
  char itemstringbuffer[512];

  DEBUG_SET_TRACKER();

  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Listing all changes from merging inventory.\n" );

  memset( stats, 0, sizeof(bsMergeInvStats) );
  stockinv = context->inventory;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->quantity == 0 )
      continue;

    bsSyncLogItemString( itemstringbuffer, sizeof(itemstringbuffer), item );

    stockitem = 0;
    if( item->bolotid >= 0 )
      stockitem = bsxFindOwlLotID( stockinv, item->bolotid );
    if( item->lotid >= 0 )
      stockitem = bsxFindLotID( stockinv, item->lotid );
    if( !( stockitem ) )
      stockitem = bsxFindMatchItem( stockinv, item );
    if( stockitem )
    {
      /* Ensure stockitem has unique ExtID */
      if( stockitem->extid == -1 )
        bsItemSetUniqueExtID( context, stockinv, stockitem );

      /* Add item to inventory */
      deleteflag = 0;
      bsxSetItemQuantity( stockinv, stockitem, stockitem->quantity + item->quantity );
      if( stockitem->quantity <= 0 )
      {
        if( stockitem->quantity < 0 )
        {
          ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_WARNING "Negative quantity clamped to zero%s\n", itemstringbuffer );
          /* Don't try to remove more items than there are, in case lots are retained */
          bsxSetItemQuantity( inv, item, item->quantity - stockitem->quantity );
          bsxSetItemQuantity( stockinv, stockitem, 0 );
        }
        deleteflag = 1;
        if( context->retainemptylotsflag )
          deleteflag = 0;
      }

      if( !( deleteflag ) )
      {
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Adjust quantity by %+d%s\n", item->quantity, itemstringbuffer );
        /* Add item to BrickLink update queue */
        if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKLINK )
        {
          deltaitem = bsxAddCopyItem( context->bricklink.diffinv, stockitem );
          bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, item->quantity );
          deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
          if( !( stockitem->quantity ) )
            deltaitem->flags |= BSX_ITEM_XFLAGS_UPDATE_STOCKROOM;
        }
        /* Add item to BrickOwl update queue */
        if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKOWL )
        {
          if( ( stockitem->boid == -1 ) && ( stockitem->bolotid == -1 ) )
            continue;
          deltaitem = bsxAddCopyItem( context->brickowl.diffinv, stockitem );
          bsxSetItemQuantity( context->brickowl.diffinv, deltaitem, item->quantity );
          deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
        }
        /* Increment stats */
        if( item->quantity > 0 )
        {
          stats->added_partcount += item->quantity;
          stats->added_lotcount++;
        }
        else
        {
          stats->removed_partcount -= item->quantity;
          stats->removed_lotcount++;
        }
      }
      else
      {
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Deleting item with zero quantity%s\n", itemstringbuffer );
        /* Add item to BrickLink delete queue */
        if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKLINK )
        {
          deltaitem = bsxAddCopyItem( context->bricklink.diffinv, stockitem );
          bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, item->quantity );
          deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
        }
        /* Add item to BrickOwl delete queue */
        if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKOWL )
        {
          if( ( stockitem->boid == -1 ) && ( stockitem->bolotid == -1 ) )
            continue;
          deltaitem = bsxAddCopyItem( context->brickowl.diffinv, stockitem );
          bsxSetItemQuantity( context->brickowl.diffinv, deltaitem, item->quantity );
          deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
        }
        /* Delete item */
        bsxRemoveItem( stockinv, stockitem );
        /* Increment stats */
        stats->delete_partcount += stockitem->quantity - item->quantity;
        stats->delete_lotcount++;
      }
    }
    else if( item->quantity > 0 )
    {
      /* Add item to local inventory */
      stockitem = bsxAddCopyItem( stockinv, item );
      /* Remove any LotID information */
      stockitem->lotid = -1;
      stockitem->bolotid = -1;
      /* Ensure stockitem has unique ExtID */
      if( stockitem->extid == -1 )
        bsItemSetUniqueExtID( context, stockinv, stockitem );
      /* Add item to BrickLink update queue */
      if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKLINK )
      {
        deltaitem = bsxAddCopyItem( context->bricklink.diffinv, stockitem );
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_CREATE;
      }
      /* Add item to BrickOwl update queue */
      if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKOWL )
      {
        if( item->boid != -1 )
        {
          deltaitem = bsxAddCopyItem( context->brickowl.diffinv, stockitem );
          deltaitem->flags |= BSX_ITEM_XFLAGS_TO_CREATE;
        }
        else
          ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_WARNING "Unknown BOID%s\n", itemstringbuffer );
      }
      /* Increment stats */
      stats->create_partcount += item->quantity;
      stats->create_lotcount++;
    }
    else if( item->quantity < 0 )
      ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_WARNING "Rejected negative quantity for new lot%s\n", itemstringbuffer );
  }

  return 1;
}


////


int bsMergeLoad( bsContext *context, bsxInventory *inv, bsMergeUpdateStats *stats, int mergeflags )
{
  int itemindex, updateflags;
  bsxItem *item, *stockitem, *deltaitem;
  bsxInventory *stockinv;
  char itemstringbuffer[512];

  DEBUG_SET_TRACKER();

  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Listing all changes from merging inventory.\n" );

  memset( stats, 0, sizeof(bsMergeUpdateStats) );
  stockinv = context->inventory;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;

    bsSyncLogItemString( itemstringbuffer, sizeof(itemstringbuffer), item );

    stockitem = 0;
    if( item->lotid >= 0 )
      stockitem = bsxFindLotID( stockinv, item->lotid );
    if( !( stockitem ) )
      stockitem = bsxFindMatchItem( stockinv, item );
    if( !( stockitem ) )
    {
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Ignoring absent item%s\n", itemstringbuffer );
      stats->missing_partcount += item->quantity;
      stats->missing_lotcount++;
      continue;
    }

    updateflags = 0;
    if( ( mergeflags & BS_MERGE_FLAGS_PRICE ) && !( bsInvPriceEqual( item->price, stockitem->price ) ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_PRICE;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set price from %.3f to %.3f for item%s\n", stockitem->price, item->price, itemstringbuffer );
    }
    if( ( mergeflags & BS_MERGE_FLAGS_COMMENTS ) && !( ccStrCmpEqualTest( item->comments, stockitem->comments ) ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_COMMENTS;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set comments from \"%s\" to \"%s\"%s\n", stockitem->comments, item->comments, itemstringbuffer );
    }
    if( ( mergeflags & BS_MERGE_FLAGS_REMARKS ) && !( ccStrCmpEqualTest( item->remarks, stockitem->remarks ) ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_REMARKS;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set remarks from \"%s\" to \"%s\"%s\n", stockitem->remarks, item->remarks, itemstringbuffer );
    }
    if( ( mergeflags & BS_MERGE_FLAGS_BULK ) && ( item->bulk != stockitem->bulk ) && ( ( item->bulk >= 2 ) || ( stockitem->bulk >= 2 ) ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_BULK;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set bulk quantity from %d to %d%s\n", stockitem->bulk, item->bulk, itemstringbuffer );
    }
    if( ( mergeflags & BS_MERGE_FLAGS_MYCOST ) && ( item->mycost > 0.0001 ) && !( bsInvPriceEqual( item->mycost, stockitem->mycost ) ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_MYCOST;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set mycost from %.3f to %.3f%s\n", stockitem->mycost, item->mycost, itemstringbuffer );
    }
    if( ( mergeflags & BS_MERGE_FLAGS_TIERPRICES ) && !( bsInvItemTierEqual( item, stockitem ) ) )
    {
      updateflags |= BSX_ITEM_XFLAGS_UPDATE_TIERPRICES;
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Set TierPrices from [[%d,%.3f],[%d,%.3f],[%d,%.3f]] to [[%d,%.3f],[%d,%.3f],[%d,%.3f]]%s\n", stockitem->tq1, stockitem->tp1, stockitem->tq2, stockitem->tp2, stockitem->tq3, stockitem->tp3, item->tq1, item->tp1, item->tq2, item->tp2, item->tq3, item->tp3, itemstringbuffer );
    }

    if( !( updateflags ) )
    {
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Skipping non-updated fields for item%s\n", itemstringbuffer );
      stats->match_partcount += stockitem->quantity;
      stats->match_lotcount++;
      continue;
    }

    updateflags |= BSX_ITEM_XFLAGS_TO_UPDATE;
    if( mergeflags & BS_MERGE_FLAGS_COMMENTS )
      bsxSetItemComments( stockitem, item->comments, -1 );
    if( mergeflags & BS_MERGE_FLAGS_REMARKS )
      bsxSetItemRemarks( stockitem, item->remarks, -1 );
    if( mergeflags & BS_MERGE_FLAGS_PRICE )
      stockitem->price = item->price;
    if( mergeflags & BS_MERGE_FLAGS_BULK )
      stockitem->bulk = item->bulk;
    if( mergeflags & BS_MERGE_FLAGS_MYCOST )
      stockitem->mycost = item->mycost;
    if( mergeflags & BS_MERGE_FLAGS_TIERPRICES )
    {
      stockitem->tq1 = item->tq1;
      stockitem->tp1 = item->tp1;
      stockitem->tq2 = item->tq2;
      stockitem->tp2 = item->tp2;
      stockitem->tq3 = item->tq3;
      stockitem->tp3 = item->tp3;
    }

    /* Add item to BrickLink update queue */
    if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKLINK )
    {
      deltaitem = bsxAddCopyItem( context->bricklink.diffinv, stockitem );
      deltaitem->flags |= updateflags;
    }
    /* Add item to BrickOwl update queue */
    if( mergeflags & BS_MERGE_FLAGS_UPDATE_BRICKOWL )
    {
      if( stockitem->boid == -1 )
        continue;
      deltaitem = bsxAddCopyItem( context->brickowl.diffinv, stockitem );
      deltaitem->flags |= updateflags;
    }
    stats->updated_partcount += stockitem->quantity;
    stats->updated_lotcount++;
  }

  return 1;
}


////


int bsSyncBrickLink( bsContext *context, bsSyncStats *stats )
{
  int64_t ordertopdate;
  bsSyncStats unusedstats;
  bsxInventory *inv;
  bsOrderList orderlist;

  DEBUG_SET_TRACKER();

  if( !( stats ) )
    stats = &unusedstats;
  memset( stats, 0, sizeof(bsSyncStats) );

  /* We must be up-to-date on orders to perform a SYNC, otherwise abort */
  if( !( inv = bsQueryBrickLinkFullState( context, &orderlist ) ) )
    return 0;
  if( bsxImportLotIDs( context->inventory, inv ) )
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
  ordertopdate = orderlist.topdate;
  blFreeOrderList( &orderlist );
  if( ordertopdate >= context->bricklink.ordertopdate )
  {
    /* A new order has arrived! ABORT! ABORT!! */
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "INFO: BrickLink SYNC has been interrupted, new orders have to be processed.\n" );
    context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_CHECK;
    bsxFreeInventory( inv );
    return 0;
  }

#if BS_ENABLE_ANTIDEBUG
  bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

  /* Set the new deltainv, ready for an update */
  bsxFreeInventory( context->bricklink.diffinv );
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Listing all changes to be pushed to BrickLink.\n" );
  context->bricklink.diffinv = bsSyncComputeDeltaInv( context, context->inventory, inv, stats, BS_SYNC_DELTA_MODE_BRICKLINK );
  bsxFreeInventory( inv );
  return 1;
}


////


int bsSyncBrickOwl( bsContext *context, bsSyncStats *stats )
{
  int64_t ordertopdate;
  bsSyncStats unusedstats;
  bsxInventory *inv;
  bsOrderList orderlist;

  DEBUG_SET_TRACKER();

  if( !( stats ) )
    stats = &unusedstats;
  memset( stats, 0, sizeof(bsSyncStats) );

  /* We must be up-to-date on orders to perform a SYNC, otherwise abort */
  if( !( inv = bsQueryBrickOwlFullState( context, &orderlist, context->brickowl.ordertopdate ) ) )
    return 0;
  if( bsxImportOwlLotIDs( context->inventory, inv ) )
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_INVENTORY;
  ordertopdate = orderlist.topdate;
  boFreeOrderList( &orderlist );
  if( ordertopdate >= context->brickowl.ordertopdate )
  {
    /* A new order has arrived! ABORT! ABORT!! */
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "INFO: BrickOwl SYNC has been interrupted, new orders have to be processed.\n" );
    context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_CHECK;
    bsxFreeInventory( inv );
    return 0;
  }

#if BS_ENABLE_ANTIDEBUG
  bsAntiDebugCountInv( context, __LINE__ & 0xf );
#endif

  /* Set the new deltainv, ready for an update */
  bsxFreeInventory( context->brickowl.diffinv );
  ioPrintf( &context->output, IO_MODEBIT_LOGONLY, "LOG: Listing all changes to be pushed to BrickOwl.\n" );
  context->brickowl.diffinv = bsSyncComputeDeltaInv( context, context->inventory, inv, stats, BS_SYNC_DELTA_MODE_BRICKOWL );
  bsxFreeInventory( inv );
  return 1;
}


////


void bsSyncPrintSummary( bsContext *context, bsSyncStats *stats, int brickowlflag )
{
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Leave untouched " IO_GREEN "%d" IO_DEFAULT " matching items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats->match_partcount, stats->match_lotcount );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Leave untouched " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots flagged by '" IO_WHITE "~" IO_DEFAULT "'.\n", stats->skipflag_partcount, stats->skipflag_lotcount );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Remove " IO_RED "%d" IO_DEFAULT " orphaned items in " IO_RED "%d" IO_DEFAULT " lots with unknown external IDs.\n", stats->deleteorphan_partcount, stats->deleteorphan_lotcount );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Remove " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots with duplicate external lot IDs.\n", stats->deleteduplicate_partcount, stats->deleteduplicate_lotcount );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Add " IO_GREEN "%d" IO_DEFAULT " missing items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats->missing_partcount, stats->missing_lotcount );
  if( brickowlflag )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Omit " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots due to BLIDs unknown to BrickOwl.\n", stats->skipunknown_partcount, stats->skipunknown_lotcount );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Remove " IO_YELLOW "%d" IO_DEFAULT " extra items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", stats->extra_partcount, stats->extra_lotcount );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "- Update data fields for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats->updatedata_partcount, stats->updatedata_lotcount );
  return;
}



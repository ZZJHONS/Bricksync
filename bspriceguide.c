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


/* Fetch price guide for all items slightly outdated */
int bsProcessInventoryPriceGuide( bsContext *context, bsxInventory *inv, int cachetime, void *callbackpointer, void (*callback)( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer ) )
{
  int retval, itemindex, fetchcount;
  int64_t updatetime;
  char *pgpath;
  bsxItem *item;
  bsxPriceGuide pg;

  DEBUG_SET_TRACKER();

  updatetime = context->curtime - cachetime;

  ioPrintf( &context->output, 0, BSMSG_INFO "Looking up price guide cache for " IO_GREEN "%d" IO_DEFAULT " items.\n", inv->itemcount );
  fetchcount = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    pgpath = bsxPriceGuidePath( context->priceguidepath, item->typeid, item->id, item->colorid, context->priceguideflags );
    if( bsxReadPriceGuide( &pg, pgpath, item->condition ) )
    {
      if( pg.modtime >= updatetime )
      {
        if( callback )
          callback( context, inv, item, &pg, callbackpointer );
      }
      else
        item->flags |= BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE;
    }
    else
      item->flags |= BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE;
    free( pgpath );
    if( item->flags & BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE )
    {
      ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_NODATE, "LOG: Item flagged for price guide update: \"%s\", color %d, condition %s.\n", ( item->id ? item->id : item->name ), item->colorid, ( item->condition ? "New" : "Used" ) );
      fetchcount++;
    }
  }

  retval = 1;
  if( fetchcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Fetching price guide data for " IO_GREEN "%d" IO_DEFAULT " items.\n", fetchcount );
    retval = bsBrickLinkFetchPriceGuide( context, inv, callbackpointer, callback );
  }

  return retval;
}


////


void bsPriceGuideInitState( bsPriceGuideState *pgstate, bsxInventory *inv, int showaltflag, int removealtflag )
{
  int altindex;
  bsPriceGuideAlt *alt;

  DEBUG_SET_TRACKER();

  memset( pgstate, 0, sizeof(bsPriceGuideState) );
  pgstate->inv = inv;
  pgstate->showaltflag = showaltflag;
  pgstate->removealtflag = removealtflag;
  pgstate->altcount = 256;
  pgstate->altlist = malloc( pgstate->altcount * sizeof(bsPriceGuideAlt) );
  for( altindex = 0 ; altindex < pgstate->altcount ; altindex++ )
  {
    alt = &pgstate->altlist[ altindex ];
    alt->itemindex = -1;
  }
  return;
}

void bsPriceGuideFinishState( bsPriceGuideState *pgstate )
{
  DEBUG_SET_TRACKER();

  if( pgstate->altlist )
  {
    free( pgstate->altlist );
    pgstate->altlist = 0;
  }
  if( pgstate->stockvalue > 0.001 )
  {
    pgstate->stockpgq /= pgstate->stockvalue;
    pgstate->stockpgp /= pgstate->stockvalue;
  }
  if( pgstate->newvalue > 0.001 )
  {
    pgstate->newpgq /= pgstate->newvalue;
    pgstate->newpgp /= pgstate->newvalue;
  }
  if( pgstate->totalvalue > 0.001 )
  {
    pgstate->totalpgq /= pgstate->totalvalue;
    pgstate->totalpgp /= pgstate->totalvalue;
  }
  return;
}



static int bsPriceGuideHandleAlt( bsPriceGuideState *pgstate, int altindex, int quantity, double value, double sale, double price, double pgq, double pgp, int stockflag, int itemindex )
{
  double itemvalue, itemsale, itemprice;
  bsxInventory *inv;
  bsxItem *remitem;
  bsPriceGuideAlt *alt;

  DEBUG_SET_TRACKER();

  if( (unsigned)altindex >= pgstate->altcount )
    return 0;
  alt = &pgstate->altlist[ altindex ];

  /* Compare with previous alt */
  if( alt->itemindex >= 0 )
  {
    inv = pgstate->inv;
    if( ( value < 0.0001 ) || ( value >= alt->value ) )
    {
      remitem = &inv->itemlist[itemindex];
      if( pgstate->removealtflag )
        bsxRemoveItem( inv, remitem );
      else
        remitem->status = 'E';
      return 0;
    }
    /* Remove previous alt */
    remitem = &inv->itemlist[alt->itemindex];
    if( pgstate->removealtflag )
      bsxRemoveItem( inv, remitem );
    else
      remitem->status = 'E';
    itemvalue = (double)alt->quantity * (double)alt->value;
    itemsale = (double)alt->quantity * (double)alt->sale;
    itemprice = (double)alt->quantity * (double)alt->price;
    if( alt->stockflag )
    {
      pgstate->stocklots--;
      pgstate->stockcount -= alt->quantity;
      pgstate->stockvalue -= itemvalue;
      pgstate->stocksale -= itemsale;
      pgstate->stockprice -= itemprice;
      pgstate->stockpgq -= itemvalue * alt->pgq;
      pgstate->stockpgp -= itemvalue * alt->pgp;
    }
    else
    {
      pgstate->newlots--;
      pgstate->newcount -= alt->quantity;
      pgstate->newvalue -= itemvalue;
      pgstate->newsale -= itemsale;
      pgstate->newprice -= itemprice;
      pgstate->newpgq -= itemvalue * alt->pgq;
      pgstate->newpgp -= itemvalue * alt->pgp;
    }
    pgstate->totallots--;
    pgstate->totalcount -= alt->quantity;
    pgstate->totalvalue -= itemvalue;
    pgstate->totalsale -= itemsale;
    pgstate->totalprice -= itemprice;
    pgstate->totalpgq -= itemvalue * alt->pgq;
    pgstate->totalpgp -= itemvalue * alt->pgp;
  }

  /* Accept new alt */
  alt->quantity = quantity;
  alt->value = value;
  alt->sale = sale;
  alt->price = price;
  alt->pgq = pgq;
  alt->pgp = pgp;
  alt->stockflag = stockflag;
  alt->itemindex = itemindex;

  return 1;
}

/*
Q : SaleQuantity / StockQuantity
P : StockValue / SaleValue
*/
void bsPriceGuideSumCallback( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer )
{
  int stockflag;
  bsPriceGuideState *pgstate;
  bsxItem *stockitem;
  double pgq, pgp, itemvalue, itemsale, itemprice;
  ccGrowth growth;

  DEBUG_SET_TRACKER();

  pgstate = callbackpointer;

  stockitem = bsxFindMatchItem( context->inventory, item );
  ccGrowthInit( &growth, 512 );
  if( pg->stockqty )
  {
    pgq = (double)pg->saleqty / (double)pg->stockqty;
    ccGrowthPrintf( &growth, "Q:%.2f", pgq );
    pgq = fmin( pgq, 8.0 );
  }
  else
  {
    pgq = 8.0;
    ccGrowthPrintf( &growth, "Q:Inf" );
  }
  pgp = 0.0;
  if( pg->stockqtyaverage > 0.01 )
    pgp = (double)pg->saleqtyaverage / (double)pg->stockqtyaverage;
  ccGrowthPrintf( &growth, " P:%.2f", pgp );
  pgp = fmin( pgp, 3.0 );
  if( item->alternateid )
  {
    if( pgstate->showaltflag )
      ccGrowthPrintf( &growth, " Alt%02d", item->alternateid );
  }
  if( !( stockitem ) )
    ccGrowthPrintf( &growth, " New" );
  bsxSetItemComments( item, growth.data, growth.offset );
  ccGrowthFree( &growth );

  /* Fill fields */
  item->price = ( pg->saleqty ? pg->saleqtyaverage : pg->stockqtyaverage );
  item->origprice = item->price;
  itemvalue = (double)item->quantity * (double)item->price;
  itemsale = (double)item->quantity * (double)pg->stockqtyaverage;
  itemprice = 0.0;
  if( stockitem )
    itemprice = (double)item->quantity * (double)stockitem->price;

  if( stockitem )
  {
    item->origprice = stockitem->price;
    bsxSetItemRemarks( item, stockitem->remarks, -1 );
  }

  stockflag = 0;
  if( ( stockitem ) && ( stockitem->quantity > 0 ) )
    stockflag = 1;

  /* Validate item if alternate */
  if( item->alternateid )
  {
    if( !( bsPriceGuideHandleAlt( pgstate, item->alternateid, item->quantity, item->price, pg->stockqtyaverage, ( stockflag ? stockitem->price : 0.0 ), pgq, pgp, stockflag, bsxGetItemListIndex( inv, item ) ) ) )
      return;
  }

  /* Add to total */
  if( stockflag )
  {
    pgstate->stocklots++;
    pgstate->stockcount += item->quantity;
    pgstate->stockvalue += itemvalue;
    pgstate->stocksale += itemsale;
    pgstate->stockprice += itemprice;
    pgstate->stockpgq += itemvalue * pgq;
    pgstate->stockpgp += itemvalue * pgp;
  }
  else
  {
    pgstate->newlots++;
    pgstate->newcount += item->quantity;
    pgstate->newvalue += itemvalue;
    pgstate->newsale += itemsale;
    pgstate->newprice += itemprice;
    pgstate->newpgq += itemvalue * pgq;
    pgstate->newpgp += itemvalue * pgp;
  }
  pgstate->totallots++;
  pgstate->totalcount += item->quantity;
  pgstate->totalvalue += itemvalue;
  pgstate->totalsale += itemsale;
  pgstate->totalprice += itemprice;
  pgstate->totalpgq += itemvalue * pgq;
  pgstate->totalpgp += itemvalue * pgp;

  return;
}


void bsPriceGuideListRangeCallback( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer )
{
  float *listrange;
  float itemratio;

  listrange = callbackpointer;

  if( !( pg->saleqty ) || ( pg->saleqtyaverage < 0.001 ) )
    return;
  itemratio = (float)item->price / (float)pg->saleqtyaverage;
  if( ( itemratio >= listrange[0] ) && ( itemratio <= listrange[1] ) )
    return;

  ioPrintf( &context->output, 0, BSMSG_INFO "Item \"" IO_CYAN "%s" IO_DEFAULT "\" (" IO_GREEN "%s" IO_DEFAULT "), color \"" IO_CYAN "%s" IO_DEFAULT "\", quantity " IO_CYAN "%d" IO_DEFAULT "; item price " IO_YELLOW "%.3f" IO_DEFAULT ", price guide " IO_CYAN "%.3f" IO_DEFAULT ", price ratio of %s%.3f" IO_DEFAULT ".\n", ( item->name ? item->name : "???" ), ( item->id ? item->id : "???" ), item->colorname, item->quantity, item->price, pg->saleqtyaverage, ( itemratio < listrange[0] ? IO_MAGENTA : IO_RED ), itemratio );

  return;
}


////


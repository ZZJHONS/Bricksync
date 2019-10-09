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

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "iolog.h"
#include "debugtrack.h"

#include "bsx.h"
#include "bsxpg.h"
#include "tcp.h"

#include "json.h"
#include "bsorder.h"
#include "brickowl.h"
#include "colortable.h"
#include "bstranslation.h"


////


typedef struct
{
  bsxInventory *orderinv;
  bsxInventory *stockinv;
  translationTable *translationtable;
  ioLog *log;
} boOrderInvState;

static void boReadBoItemCallback( void *uservalue, boItem *boitem )
{
  int blcolorid, urloffset;
  boOrderInvState *invstate;
  bsxItem *stockitem, *item;
  char blid[24];
  char bltypeid;
  char *decodedstring;

  invstate = (boOrderInvState *)uservalue;
  /* Find the item in stockinv that corresponds to our ID, add to orderinv */
  blcolorid = bsTranslateColorBo2Bl( boitem->bocolorid );
  stockitem = 0;
  blid[0] = 0;
  bltypeid = 0;
  if( boitem->bllotid >= 0 )
    stockitem = bsxFindLotID( invstate->stockinv, boitem->bllotid );
  if( !( stockitem ) && ( invstate->translationtable ) && ( bltypeid = translationBOIDtoBLID( invstate->translationtable, boitem->boid, blid, sizeof(blid) ) ) )
    stockitem = bsxFindItem( invstate->stockinv, bltypeid, blid, blcolorid, boitem->condition );
  if( !( stockitem ) )
  {
    /* Create a new item with sparse information, the bolotid and little else */
    item = bsxNewItem( invstate->orderinv );
    if( blid[0] )
      bsxSetItemId( item, blid, strlen( blid ) );
    if( boitem->name )
      bsxSetItemName( item, boitem->name, boitem->namelen );
    item->typeid = bltypeid;
    item->colorid = blcolorid;
    item->origquantity = 0;
  }
  else
  {
    item = bsxAddCopyItem( invstate->orderinv, stockitem );
    item->origquantity = stockitem->quantity;
  }
  item->colorid = blcolorid;
  item->boid = boitem->boid;
  item->sale = boitem->sale;
  item->bulk = boitem->bulk;
  item->mycost = boitem->mycost;
  item->condition = boitem->condition;
  /* Special fix for box in 'new' condition */
  if( ( item->typeid == 'O' ) && ( boitem->condition == 'U' ) && ( boitem->usedgrade == 'N' ) )
    item->condition = 'N';
  if( ( item->condition == 'U' ) && ( boitem->usedgrade ) )
    item->usedgrade = boitem->usedgrade;
  item->price = boitem->price;
  item->lotid = boitem->bllotid;
  item->bolotid = boitem->bolotid;
  bsxSetItemQuantity( invstate->orderinv, item, boitem->quantity );
  item->tq1 = boitem->tierquantity[0];
  item->tp1 = boitem->tierprice[0];
  item->tq2 = boitem->tierquantity[1];
  item->tp2 = boitem->tierprice[1];
  item->tq3 = boitem->tierquantity[2];
  item->tp3 = boitem->tierprice[2];

  /* Correct any bad item data */
  bsxVerifyItem( item );

  if( boitem->name )
    bsxSetItemName( item, boitem->name, boitem->namelen );
  else if( boitem->url )
  {
    urloffset = ccSeqFindCharLast( boitem->url, boitem->urllen, '/' );
    if( urloffset < 0 )
      urloffset = 0;
    else
      urloffset++;
    bsxSetItemName( item, boitem->url + urloffset, boitem->urllen - urloffset );
  }
  if( ( boitem->publicnote ) && ( boitem->publicnotelen ) )
  {
    decodedstring = jsonDecodeEscapeString( boitem->publicnote, boitem->publicnotelen, 0 );
    if( decodedstring )
    {
      bsxSetItemComments( item, decodedstring, strlen( decodedstring ) );
      free( decodedstring );
    }
    else
      ioPrintf( invstate->log, 0, "WARNING: JSON String decoding of \"%.*s\" failed.\n", (int)boitem->publicnotelen, boitem->publicnote );
  }
  else
    bsxSetItemComments( item, 0, 0 );
  if( ( boitem->personalnote ) && ( boitem->personalnotelen ) )
  {
    decodedstring = jsonDecodeEscapeString( boitem->personalnote, boitem->personalnotelen, 0 );
    if( decodedstring )
    {
      bsxSetItemRemarks( item, decodedstring, strlen( decodedstring ) );
      free( decodedstring );
    }
    else
      ioPrintf( invstate->log, 0, "WARNING: JSON String decoding of \"%.*s\" failed.\n", (int)boitem->personalnotelen, boitem->personalnote );
  }
  else
    bsxSetItemRemarks( item, 0, 0 );

  return;
}


/* Fill up orderinv given stockinv as reference for lot IDs */
int boReadOrderInventoryTranslate( bsxInventory *orderinv, bsxInventory *stockinv, void *translationtable, char *string, ioLog *log )
{
  boOrderInvState invstate;

  DEBUG_SET_TRACKER();

  invstate.orderinv = orderinv;
  invstate.stockinv = stockinv;
  invstate.translationtable = translationtable;
  invstate.log = log;
  return boReadOrderInventory( (void *)&invstate, boReadBoItemCallback, string, log );
}

/* Fill up inv given stockinv as reference for lot IDs */
int boReadInventoryTranslate( bsxInventory *orderinv, bsxInventory *stockinv, void *translationtable, char *string, ioLog *log )
{
  boOrderInvState invstate;

  DEBUG_SET_TRACKER();

  invstate.orderinv = orderinv;
  invstate.stockinv = stockinv;
  invstate.translationtable = translationtable;
  invstate.log = log;
  return boReadInventory( (void *)&invstate, boReadBoItemCallback, string, log );
}




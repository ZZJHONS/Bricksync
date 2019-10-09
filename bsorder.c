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

#include "bsx.h"
#include "bsorder.h"


////


const char *bsOrderServiceName[BS_ORDER_SERVICE_COUNT] =
{
  [BS_ORDER_SERVICE_BRICKLINK] = "BrickLink",
  [BS_ORDER_SERVICE_BRICKOWL] = "BrickOwl"
};

static const char *bsOrderBrickLinkStatusString[BL_ORDER_STATUS_COUNT] =
{
 [BL_ORDER_STATUS_PENDING] = "PENDING",
 [BL_ORDER_STATUS_UPDATED] = "UPDATED",
 [BL_ORDER_STATUS_PROCESSING] = "PROCESSING",
 [BL_ORDER_STATUS_READY] = "READY",
 [BL_ORDER_STATUS_PAID] = "PAID",
 [BL_ORDER_STATUS_PACKED] = "PACKED",
 [BL_ORDER_STATUS_SHIPPED] = "SHIPPED",
 [BL_ORDER_STATUS_RECEIVED] = "RECEIVED",
 [BL_ORDER_STATUS_COMPLETED] = "COMPLETED",
 [BL_ORDER_STATUS_CANCELLED] = "CANCELLED",
 [BL_ORDER_STATUS_PURGED] = "PURGED",
 [BL_ORDER_STATUS_NPB] = "NPB",
 [BL_ORDER_STATUS_NPX] = "NPX",
 [BL_ORDER_STATUS_NRS] = "NRS",
 [BL_ORDER_STATUS_NSS] = "NSS",
 [BL_ORDER_STATUS_OCR] = "OCR"
};

static const char *bsOrderBrickLinkStatusColorString[BL_ORDER_STATUS_COUNT] =
{
 [BL_ORDER_STATUS_PENDING] = IO_GREEN "PENDING" IO_DEFAULT,
 [BL_ORDER_STATUS_UPDATED] = IO_MAGENTA "UPDATED" IO_DEFAULT,
 [BL_ORDER_STATUS_PROCESSING] = IO_GREEN "PROCESSING" IO_DEFAULT,
 [BL_ORDER_STATUS_READY] = IO_GREEN "READY" IO_DEFAULT,
 [BL_ORDER_STATUS_PAID] = IO_CYAN "PAID" IO_DEFAULT,
 [BL_ORDER_STATUS_PACKED] = IO_CYAN "PACKED" IO_DEFAULT,
 [BL_ORDER_STATUS_SHIPPED] = IO_CYAN "SHIPPED" IO_DEFAULT,
 [BL_ORDER_STATUS_RECEIVED] = IO_GREEN "RECEIVED" IO_DEFAULT,
 [BL_ORDER_STATUS_COMPLETED] = IO_GREEN "COMPLETED" IO_DEFAULT,
 [BL_ORDER_STATUS_CANCELLED] = IO_RED "CANCELLED" IO_DEFAULT,
 [BL_ORDER_STATUS_PURGED] = IO_GREEN "PURGED" IO_DEFAULT,
 [BL_ORDER_STATUS_NPB] = IO_RED "NPB" IO_DEFAULT,
 [BL_ORDER_STATUS_NPX] = IO_RED "NPX" IO_DEFAULT,
 [BL_ORDER_STATUS_NRS] = IO_RED "NRS" IO_DEFAULT,
 [BL_ORDER_STATUS_NSS] = IO_RED "NSS" IO_DEFAULT,
 [BL_ORDER_STATUS_OCR] = IO_RED "OCR" IO_DEFAULT
};

static const char *bsOrderBrickOwlStatusString[BL_ORDER_STATUS_COUNT] =
{
 [BO_ORDER_STATUS_PENDING] = "PENDING",
 [BO_ORDER_STATUS_PAYMENTSENT] = "PAYMENTSENT",
 [BO_ORDER_STATUS_PAID] = "PAID",
 [BO_ORDER_STATUS_PROCESSING] = "PROCESSING",
 [BO_ORDER_STATUS_PROCESSED] = "PROCESSED",
 [BO_ORDER_STATUS_SHIPPED] = "SHIPPED",
 [BO_ORDER_STATUS_RECEIVED] = "RECEIVED",
 [BO_ORDER_STATUS_CANCELLED] = "CANCELLED"
};

static const char *bsOrderBrickOwlStatusColorString[BL_ORDER_STATUS_COUNT] =
{
 [BO_ORDER_STATUS_PENDING] = IO_GREEN "PENDING" IO_DEFAULT,
 [BO_ORDER_STATUS_PAYMENTSENT] = IO_GREEN "PAYMENTSENT" IO_DEFAULT,
 [BO_ORDER_STATUS_PAID] = IO_CYAN "PAID" IO_DEFAULT,
 [BO_ORDER_STATUS_PROCESSING] = IO_CYAN "PROCESSING" IO_DEFAULT,
 [BO_ORDER_STATUS_PROCESSED] = IO_CYAN "PROCESSED" IO_DEFAULT,
 [BO_ORDER_STATUS_SHIPPED] = IO_CYAN "SHIPPED" IO_DEFAULT,
 [BO_ORDER_STATUS_RECEIVED] = IO_GREEN "RECEIVED" IO_DEFAULT,
 [BO_ORDER_STATUS_CANCELLED] = IO_RED "CANCELLED" IO_DEFAULT
};


////


const char *bsGetOrderStatusString( bsOrder *order )
{
  const char *statusstring;

  statusstring = "UNKNOWN";
  if( order->service == BS_ORDER_SERVICE_BRICKLINK )
  {
    if( (unsigned)order->status < BL_ORDER_STATUS_COUNT )
      statusstring = bsOrderBrickLinkStatusString[order->status];
  }
  else if( order->service == BS_ORDER_SERVICE_BRICKOWL )
  {
    if( (unsigned)order->status < BO_ORDER_STATUS_COUNT )
      statusstring = bsOrderBrickOwlStatusString[order->status];
  }

  return statusstring;
}


const char *bsGetOrderStatusColorString( bsOrder *order )
{
  const char *statusstring;

  statusstring = IO_RED "UNKNOWN" IO_DEFAULT;
  if( order->service == BS_ORDER_SERVICE_BRICKLINK )
  {
    if( (unsigned)order->status < BL_ORDER_STATUS_COUNT )
      statusstring = bsOrderBrickLinkStatusColorString[order->status];
  }
  else if( order->service == BS_ORDER_SERVICE_BRICKOWL )
  {
    if( (unsigned)order->status < BO_ORDER_STATUS_COUNT )
      statusstring = bsOrderBrickOwlStatusColorString[order->status];
  }

  return statusstring;
}


////


void bsFreeOrderListEntry( bsOrder *order )
{
  if( order->customer )
  {
    free( order->customer );
    order->customer = 0;
  }
  return;
}

void bsFreeOrderList( bsOrderList *orderlist )
{
  int orderindex;
  bsOrder *order;

  for( orderindex = 0 ; orderindex < orderlist->ordercount ; orderindex++ )
  {
    order = &orderlist->orderarray[ orderindex ];
    bsFreeOrderListEntry( order );
  }

  free( orderlist->orderarray );
  orderlist->orderarray = 0;
  orderlist->ordercount = 0;

  return;
}


////


void bsOrderSetInventoryInfo( bsxInventory *inv, bsOrder *order )
{
  char *servicestring;
  if( inv->order.service )
    free( inv->order.service );
  if( inv->order.customer )
    free( inv->order.customer );
  if( inv->order.currency )
    free( inv->order.currency );
  servicestring = 0;
  if( order->service == BS_ORDER_SERVICE_BRICKLINK )
    servicestring = "BrickLink";
  else if( order->service == BS_ORDER_SERVICE_BRICKOWL )
    servicestring = "BrickOwl";
  inv->order.service = ccStrDup( servicestring );
  inv->order.orderid = order->id;
  inv->order.orderdate = order->date;
  inv->order.customer = ccStrDup( order->customer );
  inv->order.subtotal = order->subtotal;
  inv->order.grandtotal = order->grandtotal;
  inv->order.payment = order->paymentgrandtotal;
  inv->order.currency = ccStrDup( order->paymentcurrency );
  inv->orderblockflag = 1;
  return;
}



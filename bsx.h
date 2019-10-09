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

/* Build string with escape chars as required, returned string must be free()'d */
char *xmlEncodeEscapeString( char *string, int length, int *retlength );
/* Build string with decoded escape chars, returned string must be free()'d */
char *xmlDecodeEscapeString( char *string, int length, int *retlength );


////


typedef struct
{
  /* ItemID (string) */
  char *id;
  /* ItemName (string) */
  char *name;
  /* ItemTypeName (string) */
  char *typename;
  /* ColorName (string) */
  char *colorname;
  /* CategoryID (int) */
  int categoryid;
  /* CategoryName (string) */
  char *categoryname;
  /* ItemTypeID (char) : 'P','S','M','B','G','C','I','O','U' */
  char typeid;
  /* Condition (char) : 'N','U' */
  char condition;
  /* Used grade (char) : 'N','G','A' */
  char usedgrade;
  /* Completeness (char) : 'C'(complete),'B'(incomplete),'S'(sealed) */
  char completeness;
  /* Status (char) */
  char status;
  /* ColorID (int) */
  int colorid;
  /* Qty (int) */
  int quantity;
  /* Price (float) */
  float price;
  /* SalePrice (float) */
  float saleprice;
  /* Bulk (int) */
  int bulk;
  /* Sale (int) */
  int sale;
  /* Retain, stockroom */
  int stockflags;
  /* AlternateID (int) */
  int alternateid;
  /* OrigPrice (float) */
  float origprice;
  /* Comments (string) */
  char *comments;
  /* Remarks (string) */
  char *remarks;
  /* OrigQty (int) */
  int origquantity;
  /* MyCost (float) */
  float mycost;
  /* Tier1-quantity */
  int tq1;
  /* Tier1-price */
  float tp1;
  /* Tier2-quantity */
  int tq2;
  /* Tier2-price */
  float tp2;
  /* Tier3-quantity */
  int tq3;
  /* Tier3-price */
  float tp3;
  /* LotID (int64_t) */
  int64_t lotid;

  /* BrickOwl extensions */
  /* BrickOwl ID (int64_t) */
  int64_t boid;
  /* BrickOwl LotID (int64_t) */
  int64_t bolotid;

  /* String allocation flags */
  int32_t flags;

  /* External ID, not read or saved, -1 by default */
  int64_t extid;

} bsxItem;

#define BSX_ITEM_FLAGS_ALLOC_ID (0x1)
#define BSX_ITEM_FLAGS_ALLOC_NAME (0x2)
#define BSX_ITEM_FLAGS_ALLOC_TYPENAME (0x4)
#define BSX_ITEM_FLAGS_ALLOC_COLORNAME (0x8)
#define BSX_ITEM_FLAGS_ALLOC_CATEGORYNAME (0x10)
#define BSX_ITEM_FLAGS_ALLOC_COMMENTS (0x20)
#define BSX_ITEM_FLAGS_ALLOC_REMARKS (0x40)
#define BSX_ITEM_FLAGS_DELETED (0x80)

/* Flags meant for custom usage */
#define BSX_ITEM_XFLAGS_TO_CREATE (0x100)
#define BSX_ITEM_XFLAGS_TO_UPDATE (0x200)
#define BSX_ITEM_XFLAGS_TO_DELETE (0x400)
#define BSX_ITEM_XFLAGS_UPDATE_QUANTITY (0x1000)
#define BSX_ITEM_XFLAGS_UPDATE_COMMENTS (0x2000)
#define BSX_ITEM_XFLAGS_UPDATE_REMARKS (0x4000)
#define BSX_ITEM_XFLAGS_UPDATE_PRICE (0x8000)
#define BSX_ITEM_XFLAGS_UPDATE_BULK (0x100000)
#define BSX_ITEM_XFLAGS_UPDATE_STOCKROOM (0x200000)
#define BSX_ITEM_XFLAGS_UPDATE_MYCOST (0x400000)
#define BSX_ITEM_XFLAGS_UPDATE_USEDGRADE (0x800000)
#define BSX_ITEM_XFLAGS_UPDATE_TIERPRICES (0x1000000)
#define BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE (0x2000000)

/* Mask of all update flags */
#define BSX_ITEM_XFLAGS_UPDATEMASK (BSX_ITEM_XFLAGS_UPDATE_QUANTITY|BSX_ITEM_XFLAGS_UPDATE_COMMENTS|BSX_ITEM_XFLAGS_UPDATE_REMARKS|BSX_ITEM_XFLAGS_UPDATE_PRICE|BSX_ITEM_XFLAGS_UPDATE_BULK|BSX_ITEM_XFLAGS_UPDATE_STOCKROOM|BSX_ITEM_XFLAGS_UPDATE_MYCOST|BSX_ITEM_XFLAGS_UPDATE_USEDGRADE|BSX_ITEM_XFLAGS_UPDATE_TIERPRICES)

#define BSX_ITEM_STOCKFLAGS_RETAIN (0x1)
#define BSX_ITEM_STOCKFLAGS_STOCKROOM (0x2)
#define BSX_ITEM_STOCKFLAGS_STOCKROOM_A (0x10)
#define BSX_ITEM_STOCKFLAGS_STOCKROOM_B (0x20)
#define BSX_ITEM_STOCKFLAGS_STOCKROOM_C (0x40)


typedef struct
{
  char *service;
  int orderid;
  int64_t orderdate;
  char *customer;
  float subtotal;
  float grandtotal;
  float payment;
  char *currency;
} bsxInventoryOrder;

typedef struct
{
  char *xmldata;
  size_t xmlsize;
  int itemcount;
  int itemalloc;
  int itemfreecount;
  bsxItem *itemlist;

  int orderblockflag;
  bsxInventoryOrder order;

  int partcount;
  double totalprice;
  double totalorigprice;
} bsxInventory;


////


bsxInventory *bsxNewInventory();
int bsxLoadInventory( bsxInventory *inv, char *path );
int bsxSaveInventory( char *path, bsxInventory *inv, int fsyncflag, int sortcolumn );
void bsxEmptyInventory( bsxInventory *inv );
void bsxFreeInventory( bsxInventory *inv );

/* If many items were deleted from inventory, repack the list */
void bsxPackInventory( bsxInventory *inv );

/* Clamp negative quantities to zero */
void bsxClampNegativeInventory( bsxInventory *inv );


////


enum
{
  BSX_SORT_ID,
  BSX_SORT_NAME,
  BSX_SORT_TYPENAME,
  BSX_SORT_COLORNAME,
  BSX_SORT_CATEGORYNAME,
  BSX_SORT_COLORID,
  BSX_SORT_QUANTITY,
  BSX_SORT_PRICE,
  BSX_SORT_ORIGPRICE,
  BSX_SORT_COMMENTS,
  BSX_SORT_REMARKS,
  BSX_SORT_LOTID,
  BSX_SORT_ID_COLOR_CONDITION_REMARKS_COMMENTS_QUANTITY,
  BSX_SORT_COLORNAME_NAME_CONDITION,
  BSX_SORT_UPDATE_PRIORITY,
  BSX_SORT_CHECK_LIST_ORDER,

  BSX_SORT_COUNT
};

/* Sort inventory by the field specified */
int bsxSortInventory( bsxInventory *inv, int sortfield, int reverseflag );


/* Find item that matches ID, typeID, colorID and condition */
bsxItem *bsxFindMatchItem( bsxInventory *inv, bsxItem *matchitem );

/* Find the index of the item that matches ID, typeID, colorID and condition */
int bsxFindMatchItemIndex( bsxInventory *inv, bsxItem *matchitem );

/* Find item by typeID, ID, colorID and condition */
bsxItem *bsxFindItem( bsxInventory *inv, char typeid, char *id, int colorid, char condition );

/* Find item by ID, colorID and condition */
bsxItem *bsxFindItemNoType( bsxInventory *inv, char *id, int colorid, char condition );

/* Find item by BOID, colorID and condition */
bsxItem *bsxFindItemBOID( bsxInventory *inv, int64_t boid, int colorid, char condition );

/* Find item that matches LotID */
bsxItem *bsxFindLotID( bsxInventory *inv, int64_t lotid );

/* Find item that matches BOID && color && condition && LotID */
bsxItem *bsxFindBoidColorConditionLotID( bsxInventory *inv, int64_t boid, int colorid, char condition, int64_t lotid );

/* Find item that matches OwlLotID */
bsxItem *bsxFindOwlLotID( bsxInventory *inv, int64_t bolotid );

/* Find item that matches extID */
bsxItem *bsxFindExtID( bsxInventory *inv, int64_t extid );


////


/* Add/Sub 'srcinv' to 'dstinv' with consolidation if items match */
int bsxAddInventory( bsxInventory *dstinv, bsxInventory *srcinv );
int bsxSubInventory( bsxInventory *dstinv, bsxInventory *srcinv );

/* Returns an inventory as ( dstinv - srcinv ) */
bsxInventory *bsxDiffInventory( bsxInventory *dstinv, bsxInventory *srcinv );
bsxInventory *bsxDiffInventoryByLotID( bsxInventory *dstinv, bsxInventory *srcinv );

void bsxInvertQuantities( bsxInventory *inv );

/* Recompute partcount and totalprice */
void bsxRecomputeTotals( bsxInventory *inv );

/* Remove all items with a quantity of zero */
void bsxRemoveEmptyItems( bsxInventory *inv );

/* Merge quantities for matching items */
void bsxConsolidateInventoryByMatch( bsxInventory *inv );

/* Merge quantities for items sharing the same lotID */
void bsxConsolidateInventoryByLotID( bsxInventory *inv );


////


/* Import all LotIDs from inv to stockinv for matching items, return count */
int bsxImportLotIDs( bsxInventory *dstinv, bsxInventory *srcinv );

/* Import all OwlLotIDs from inv to stockinv for matching items, return count */
int bsxImportOwlLotIDs( bsxInventory *dstinv, bsxInventory *srcinv );


////


bsxItem *bsxNewItem( bsxInventory *inv );
bsxItem *bsxAddItem( bsxInventory *inv, bsxItem *itemref );
bsxItem *bsxAddCopyItem( bsxInventory *inv, bsxItem *itemref );
void bsxRemoveItem( bsxInventory *inv, bsxItem *item );

void bsxSetItemId( bsxItem *item, char *id, int len );
void bsxSetItemName( bsxItem *item, char *name, int len );
void bsxSetItemTypeName( bsxItem *item, char *typename, int len );
void bsxSetItemColorName( bsxItem *item, char *colorname, int len );
void bsxSetItemComments( bsxItem *item, char *comments, int len );
void bsxSetItemRemarks( bsxItem *item, char *remarks, int len );

void bsxSetItemQuantity( bsxInventory *inv, bsxItem *item, int quantity );

size_t bsxGetItemListIndex( bsxInventory *inv, bsxItem *item );

void bsxVerifyItem( bsxItem *item );


static inline void bsxClearItem( bsxItem *item )
{
  memset( item, 0, sizeof(bsxItem) );
  item->status = 'I';
  item->condition = 'N';
  item->completeness = 'S';
  item->lotid = -1;
  item->boid = -1;
  item->bolotid = -1;
  item->extid = -1;
  return;
}


////



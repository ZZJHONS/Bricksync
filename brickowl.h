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

/* Read orderlist, return value is count of orders */
int boReadOrderList( bsOrderList *orderlist, char *string, ioLog *log );
void boFreeOrderList( bsOrderList *orderlist );

/* Read order details */
int boReadOrderView( bsOrder *order, char *string, ioLog *log );


typedef struct
{
  int64_t boid;
  int bocolorid;
  int quantity;
  int sale;
  int bulk;
  float mycost;
  char condition;
  char usedgrade;
  int64_t bolotid;
  int64_t bllotid;
  char *extid;
  int extidlen;
  char *name;
  int namelen;
  char *url;
  int urllen;
  float price;
  char *publicnote;
  int publicnotelen;
  char *personalnote;
  int personalnotelen;
  int tierquantity[3];
  float tierprice[3];
} boItem;

#define BO_COLOR_TABLE_RANGE (256)

typedef struct
{
  int bo2bl[BO_COLOR_TABLE_RANGE];
  int bl2bo[BO_COLOR_TABLE_RANGE];
} boColorTable;

#define BO_USER_DETAILS_NAME_LENGTH (256)

typedef struct
{
  char username[BO_USER_DETAILS_NAME_LENGTH];
  char storename[BO_USER_DETAILS_NAME_LENGTH];
  char storecurrency[BS_ORDER_CURRENCY_LENGTH];
} boUserDetails;


/* Read order and call callback for every item found */
int boReadOrderInventory( void *uservalue, void (*callback)( void *uservalue, boItem *boitem ), char *string, ioLog *log );

/* Read inventory and call callback for every item found */
int boReadInventory( void *uservalue, void (*callback)( void *uservalue, boItem *boitem ), char *string, ioLog *log );

/* Read color table */
int boReadColorTable( boColorTable *colortable, char *string, ioLog *log );

/* Read BLID lookup */
int boReadLookup( int64_t *retboid, char *string, ioLog *log );

/* Read lotID for a single lot, as reply to a lot creation */
int boReadLotID( int64_t *retlotid, char *string, ioLog *log );

/* Read user details */
int boReadUserDetails( boUserDetails *details, char *string, ioLog *log );



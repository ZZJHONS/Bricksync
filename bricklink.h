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

/* Send a single query to server, receive malloc()'ed buffer of reply */
char *blQueryInventory( size_t *retsize, char itemtypeid, char *itemid );
char *blQueryPriceGuide( size_t *retsize, char itemtypeid, char *itemid, int itemcolorid );

/* Fetch inventory for item type and id */
int blFetchInventory( bsxInventory *inv, char itemtypeid, char *itemid );
int blFetchPriceGuide( bsxPriceGuide *pgnew, bsxPriceGuide *pgused, char itemtypeid, char *itemid, int itemcolorid );


////


/* Read orderlist, return value is count of orders */
int blReadOrderList( bsOrderList *orderlist, char *string, ioLog *log );
void blFreeOrderList( bsOrderList *orderlist );

/* Read order inventory */
int blReadOrderInventory( bsxInventory *inv, char *string, ioLog *log );

/* Read inventory */
int blReadInventory( bsxInventory *inv, char *string, ioLog *log );

/* Read lotID for a single lot, as reply to a lot creation */
int blReadLotID( int64_t *retlotid, char *string, ioLog *log );

/* Read generic reply, return 1 on success */
int blReadGeneric( char *string, ioLog *log );


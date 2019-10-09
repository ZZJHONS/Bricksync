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

typedef struct
{
  int salecount;
  int saleqty;
  float saleminimum;
  float saleaverage;
  float saleqtyaverage;
  float salemaximum;
  int stockcount;
  int stockqty;
  float stockminimum;
  float stockaverage;
  float stockqtyaverage;
  float stockmaximum;

  int64_t modtime;
} bsxPriceGuide;


/* Returned string must be freed() */
char *bsxPriceGuidePath( char *basepath, char itemtypeid, char *itemid, int itemcolorid, int flags );

#define BSX_PRICEGUIDE_FLAGS_BRICKSTORE (0x1)
#define BSX_PRICEGUIDE_FLAGS_BRICKSTOCK (0x2)
#define BSX_PRICEGUIDE_FLAGS_MKDIR (0x4)


int bsxReadPriceGuide( bsxPriceGuide *pg, char *path, char itemcondition );

int bsxWritePriceGuide( bsxPriceGuide *pgnew, bsxPriceGuide *pgused, char *path, char itemtypeid, char *itemid, int itemcolorid );



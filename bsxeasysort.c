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

#include "bsx.h"


////


int main( int argc, char **argv )
{
  bsxInventory *inv;

  if( argc < 2 )
  {
    printf( "Usage: bsxsort file.bsx inv.bsx\n" );
    return 1;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, argv[1] ) ) )
  {
    printf( "ERROR: Failed to read %s\n", argv[1] );
    return 0;
  }

  bsxSortInventory( inv, BSX_SORT_COLORNAME_NAME_CONDITION, 0 );

  bsxSaveInventory( argv[1], inv, 0, -32 );

  bsxFreeInventory( inv );

  return 1;
}




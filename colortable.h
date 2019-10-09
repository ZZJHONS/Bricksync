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

#define BS_COLOR_INDEX_RANGE (256)

extern int bsColorTranslationBo2Bl[BS_COLOR_INDEX_RANGE];

extern int bsColorTranslationBl2Bo[BS_COLOR_INDEX_RANGE];



static inline int bsTranslateColorBo2Bl( int bocolorid )
{
  if( (unsigned)bocolorid >= BS_COLOR_INDEX_RANGE )
    return -1;
  return bsColorTranslationBo2Bl[ bocolorid ];
}

static inline int bsTranslateColorBl2Bo( int blcolorid )
{
  if( (unsigned)blcolorid >= BS_COLOR_INDEX_RANGE )
    return -1;
  return bsColorTranslationBl2Bo[ blcolorid ];
}



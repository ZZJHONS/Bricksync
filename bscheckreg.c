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


#if BS_ENABLE_REGISTRATION


////


#define BS_CHECKREG_BL_RAW (0x74e6)
#define BS_CHECKREG_BO_RAW (0xd981)

#define BS_CHECKREG_HASH_MASK (0x3ffff)

/* ccHash32Int32() of raw values */
#define BS_CHECKREG_BL_OFFSET (0x1eb97)
#define BS_CHECKREG_BO_OFFSET (0x2fcc3)

#define BS_CHECKREG_ANTIDEBUG_OFFSET0 (372)
#define BS_CHECKREG_ANTIDEBUG_OFFSET1 (588)


////


volatile uint32_t bsCheckRegisteredBlRaw = BS_CHECKREG_BL_RAW;

volatile uint32_t bsCheckRegisteredZeroMask = 0x0;

volatile uint32_t bsCheckRegisteredBoRaw = BS_CHECKREG_BO_RAW;

#if BS_ENABLE_MATHPUZZLE
volatile uint32_t *bsCheckRegAntiDebug0;
volatile uint32_t *bsCheckRegAntiDebug1;
#endif


void bsCheckRegistered( bsContext *context )
{
  int successflag;
  uint32_t bloffset, booffset, zeromask;
  void (*checkbl)( bsContext *context ) = bsCheckBrickLinkState - BS_CHECKREG_BL_OFFSET;
  void (*checkbo)( bsContext *context ) = bsCheckBrickOwlState - BS_CHECKREG_BO_OFFSET;

  bloffset = ccHash32Int32( bsCheckRegisteredBlRaw ) & BS_CHECKREG_HASH_MASK;
  checkbl += bloffset;
  booffset = ccHash32Int32( bsCheckRegisteredBoRaw ) & BS_CHECKREG_HASH_MASK;
  checkbo += booffset;

#if BS_ENABLE_MATHPUZZLE
  zeromask = bsCheckRegisteredZeroMask;
  bsCheckRegAntiDebug0 = ADDRESS( context, offsetof(bsContext,antidebugpuzzle0) - BS_CHECKREG_ANTIDEBUG_OFFSET0 );
  bsCheckRegAntiDebug1 = ADDRESS( context, offsetof(bsContext,antidebugpuzzle1) - BS_CHECKREG_ANTIDEBUG_OFFSET1 );
#endif

  checkbl( context );
  checkbo( context );

#if BS_ENABLE_MATHPUZZLE
  *(uint32_t *)ADDRESS( bsCheckRegAntiDebug0, BS_CHECKREG_ANTIDEBUG_OFFSET0 ) &= zeromask;
  *(uint32_t *)ADDRESS( bsCheckRegAntiDebug1, BS_CHECKREG_ANTIDEBUG_OFFSET1 ) &= zeromask;
#endif

  return;
}


////


#endif


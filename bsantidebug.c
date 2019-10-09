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
#include "cpuinfo.h"
#include "antidebug.h"
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



#if BS_ENABLE_LIMITS


#define X(x) BS_LIMIT_HIDE_CHAR(x)

/* "WARNING: Tracked inventory counts %d items. We are close to BrickSync's maximum of 25000 items for the free version.\nType "register" for more information.\n"; */
char bsInventoryLimitsWarning[] = { X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0xb),X('W'),X('A'),X('R'),X('N'),X('I'),X('N'),X('G'),X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0xf),X(':'),X(' '),X('T'),X('r'),X('a'),X('c'),X('k'),X('e'),X('d'),X(' '),X('i'),X('n'),X('v'),X('e'),X('n'),X('t'),X('o'),X('r'),X('y'),X(' '),X('c'),X('o'),X('u'),X('n'),X('t'),X('s'),X(' '),X('%'),X('d'),X(' '),X('i'),X('t'),X('e'),X('m'),X('s'),X('.'),X(' '),X('W'),X('e'),X(' '),X('a'),X('r'),X('e'),X(' '),X('c'),X('l'),X('o'),X('s'),X('e'),X(' '),X('t'),X('o'),X(' '),X('B'),X('r'),X('i'),X('c'),X('k'),X('S'),X('y'),X('n'),X('c'),X('\''),X('s'),X(' '),X('m'),X('a'),X('x'),X('i'),X('m'),X('u'),X('m'),X(' '),X('o'),X('f'),X(' '),X('2'),X('5'),X('0'),X('0'),X('0'),X('0'),X(' '),X('i'),X('t'),X('e'),X('m'),X('s'),X(' '),X('f'),X('o'),X('r'),X(' '),X('t'),X('h'),X('e'),X(' '),X('f'),X('r'),X('e'),X('e'),X(' '),X('v'),X('e'),X('r'),X('s'),X('i'),X('o'),X('n'),X('.'),X('\n'),X('T'),X('y'),X('p'),X('e'),X(' '),X('"'),X('r'),X('e'),X('g'),X('i'),X('s'),X('t'),X('e'),X('r'),X('"'),X(' '),X('f'),X('o'),X('r'),X(' '),X('m'),X('o'),X('r'),X('e'),X(' '),X('i'),X('n'),X('f'),X('o'),X('r'),X('m'),X('a'),X('t'),X('i'),X('o'),X('n'),X('.'),X('\n'),X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0x10),X(0) };

/* "ERROR: Tracked inventory counts %d items, exceeding BrickSync's maximum of 250000 items for the free version.\n" */
char bsInventoryLimitsError[] = { X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0x9),X('E'),X('R'),X('R'),X('O'),X('R'),X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0xf),X(':'),X(' '),X('T'),X('r'),X('a'),X('c'),X('k'),X('e'),X('d'),X(' '),X('i'),X('n'),X('v'),X('e'),X('n'),X('t'),X('o'),X('r'),X('y'),X(' '),X('c'),X('o'),X('u'),X('n'),X('t'),X('s'),X(' '),X('%'),X('d'),X(' '),X('i'),X('t'),X('e'),X('m'),X('s'),X(','),X(' '),X('e'),X('x'),X('c'),X('e'),X('e'),X('d'),X('i'),X('n'),X('g'),X(' '),X('B'),X('r'),X('i'),X('c'),X('k'),X('S'),X('y'),X('n'),X('c'),X('\''),X('s'),X(' '),X('m'),X('a'),X('x'),X('i'),X('m'),X('u'),X('m'),X(' '),X('o'),X('f'),X(' '),X('2'),X('5'),X('0'),X('0'),X('0'),X('0'),X(' '),X('i'),X('t'),X('e'),X('m'),X('s'),X(' '),X('f'),X('o'),X('r'),X(' '),X('t'),X('h'),X('e'),X(' '),X('f'),X('r'),X('e'),X('e'),X(' '),X('v'),X('e'),X('r'),X('s'),X('i'),X('o'),X('n'),X('.'),X('\n'),X(IO_COLOR_CODE_BASE+0x10),X(0) };

/* "INFO: Registration Code : %s\n" */
char bsInventoryLimitsRegister[] = { X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0xa), X('I'),X('N'),X('F'),X('O'),X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0x10),X(':'),X(' '),X('R'),X('e'),X('g'),X('i'),X('s'),X('t'),X('r'),X('a'),X('t'),X('i'),X('o'),X('n'),X(' '),X('C'),X('o'),X('d'),X('e'),X(' '),X(':'),X(' '),X('%'),X('s'),X('\n'),X(IO_COLOR_ESCAPE),X(IO_COLOR_CODE_BASE+0x10),X(0) };

#undef X


void bsAntiDebugUnpackString( char *string )
{
  for( ; ; string++ )
  {
    *string = BS_LIMIT_UNHIDE_CHAR( *string );
    if( !( *string ) )
      return;
  }
  return;
}

void bsAntiDebugUnpack()
{
  bsAntiDebugUnpackString( bsInventoryLimitsWarning );
  bsAntiDebugUnpackString( bsInventoryLimitsError );
  bsAntiDebugUnpackString( bsInventoryLimitsRegister );
  return;
}


#endif



////



#if BS_ENABLE_ANTIDEBUG


#if BS_ENABLE_ANTIDEBUG_CRASH

 #define ANTIDEBUG_NUMBER 000
 #include "antidebugcrash.h"

#endif

#define ANTIDEBUG_NUMBER 100
#include "antidebugrdtsc.h"

#define ANTIDEBUG_NUMBER 101
#include "antidebugrdtsc.h"


int bsAntiDebugInit( bsContext *context, cpuInfo *cpuinfo )
{
  /* Bit 0x80,0x800 clear, Bit 0x1000 set */
  context->antidebugcaps = 0xa403;

  /* Ensure we can rdtsc, if TSC is not constant, lock thread on specific CPU core */
  if( !( cpuinfo->captsc | cpuinfo->capsse2 ) )
    return 0;

  if( !( cpuinfo->capconstanttsc ) )
    mmThreadBindToCpu( ( mmcontext.cpucount > 1 ? 1 : 0 ) );

  /* Garbage */
  if( cpuinfo->capavx )
  {
    context->antidebugcaps |= cpuinfo->cap3dnow << 4;
    context->antidebugcaps |= cpuinfo->cappclmul << 2;
    if( cpuinfo->cacheunifiedL1 )
      context->antidebugcaps ^= cpuinfo->capxop << 5;
  }

  /* Bit 0x2 clear */
  context->antidebugcaps ^= cpuinfo->capmmx << 1;
  /* Garbage */
  if( cpuinfo->capmmxext )
    context->antidebugcaps |= ( cpuinfo->capmmx ^ cpuinfo->capmmxext ) << 12;
  /* Bit 0x40 set */
  context->antidebugcaps |= ( cpuinfo->capfma3 << 3 ) | ( cpuinfo->capsse << 6 ) | ( cpuinfo->capf16c << 9 ) | ( cpuinfo->caphyperthreading << 10 );
  /* Garbage */
  if( cpuinfo->capsse3 )
    context->antidebugcaps |= ( cpuinfo->capsse ^ cpuinfo->capsse3 ) << 7;

  context->antidebugtime0 = antiDebugRdTsc100();
  /* Garbage */
  if( cpuinfo->capmisalignsse )
    context->antidebugcaps |= ( cpuinfo->capfma4 << 14 ) | ( cpuinfo->cap3dnowext << 15 );
  /* Bit 0x80 clear, Bit 0x40 clear, Bit 0x3 clear, Bit 0x800 set */
  context->antidebugarch = ( cpuinfo->endianness << 7 ) | ( ccCountBits32( cpuinfo->clflushsize ) << 11 );
  return 1;
}


/* This is garbage code to separate anti-debugger chunks of code */
#define CC_SPACECURVE_DIMCOUNT (3)
#define CC_SPACECURVE_DIMSPACING (CC_SPACECURVE_BITS_PER_INT/CC_SPACECURVE_DIMCOUNT)
#define CC_SPACECURVE_DIMSPACING1 (CC_SPACECURVE_DIMSPACING-1)
void ccSpaceCurveUnmap3D( int mapbits, unsigned int curveindex, unsigned int *point )
{
  int bit, rotation, totalbits;
  unsigned int pointindex;
  unsigned int addmask, addbits;
  unsigned int p0, p1, p2;

  totalbits = CC_SPACECURVE_DIMCOUNT * mapbits;
  rotation = 0;
  addmask = 0;
  curveindex ^= ( curveindex ^ ( ( (unsigned int)0x49249249 ) & ( ( ( (unsigned int)2 ) << ( totalbits - CC_SPACECURVE_DIMCOUNT ) ) - 1 ) ) ) >> 1;
  pointindex = 0;
  bit = totalbits - CC_SPACECURVE_DIMCOUNT;
  do
  {
    addbits = ( curveindex >> bit ) & ( ( 1 << CC_SPACECURVE_DIMCOUNT ) - 1 );
    pointindex <<= CC_SPACECURVE_DIMCOUNT;
    pointindex |= addmask ^ ( ( ( addbits << rotation ) | ( addbits >> ( CC_SPACECURVE_DIMCOUNT - rotation ) ) ) & ( ( 1 << CC_SPACECURVE_DIMCOUNT ) - 1 ) );
    addmask = 1 << rotation;
    /* Magic number! */
    rotation = ( 0x641289 >> ( ( ( addbits & ( ( 1 << (CC_SPACECURVE_DIMCOUNT-1) ) - 1 ) ) << 1 ) | ( rotation << 3 ) ) ) & 0x3;
  } while( ( bit -= CC_SPACECURVE_DIMCOUNT ) >= 0 );
  for( bit = CC_SPACECURVE_DIMCOUNT ; bit < totalbits ; bit <<= 1 )
    pointindex ^= pointindex >> bit;

  p0 = pointindex & 0x1;
  p1 = pointindex & 0x2;
  p2 = pointindex & 0x4;
  for( bit = 1 ; bit < mapbits ; bit++ )
  {
    pointindex >>= CC_SPACECURVE_DIMCOUNT;
    p0 |= ( pointindex & 0x1 ) << bit;
    p1 |= ( pointindex & 0x2 ) << bit;
    p2 |= ( pointindex & 0x4 ) << bit;
  }
  point[0] = p0 >> 0;
  point[1] = p1 >> 1;
  point[2] = p2 >> 2;

  return;
}


void bsAntiDebugMid( bsContext *context )
{
#if BS_ENABLE_ANTIDEBUG_CRASH
  context->antidebugvalue = CC_CONCATENATE(antiDebugCrash,000)( context->antidebugcaps, context->antidebugarch );
#else
  context->antidebugvalue = context->antidebugcaps ^ context->antidebugarch;
#endif
  return;
}


/* This is garbage code to separate anti-debugger chunks of code */
#define CC_SPACECURVE_MAPBITS (21)
void ccSpaceCurveUnmap3D21Bits64( uint64_t curveindex, uint64_t *point )
{
  int bit, rotation;
  uint64_t pointindex;
  uint64_t addmask, addbits;
  uint64_t p0, p1, p2;

  rotation = 0;
  addmask = 0;
  curveindex ^= ( curveindex ^ ( ( (uint64_t)0x9249249249249249LL ) & ( ( ( (uint64_t)2 ) << ((CC_SPACECURVE_MAPBITS-1)*CC_SPACECURVE_DIMCOUNT) ) - 1 ) ) ) >> 1;
  pointindex = 0;
  bit = ( CC_SPACECURVE_DIMCOUNT * CC_SPACECURVE_MAPBITS ) - CC_SPACECURVE_DIMCOUNT;
  do
  {
    addbits = ( curveindex >> bit ) & ( ( 1 << CC_SPACECURVE_DIMCOUNT ) - 1 );
    pointindex <<= CC_SPACECURVE_DIMCOUNT;
    pointindex |= addmask ^ ( ( ( addbits << rotation ) | ( addbits >> ( CC_SPACECURVE_DIMCOUNT - rotation ) ) ) & ( ( 1 << CC_SPACECURVE_DIMCOUNT ) - 1 ) );
    addmask = 1 << rotation;
    /* Magic number! */
    rotation = ( 0x641289 >> ( ( ( addbits & ( ( 1 << (CC_SPACECURVE_DIMCOUNT-1) ) - 1 ) ) << 1 ) | ( rotation << 3 ) ) ) & 0x3;
  } while( ( bit -= CC_SPACECURVE_DIMCOUNT ) >= 0 );

  p0 = pointindex & 0x1;
  p1 = pointindex & 0x2;
  p2 = pointindex & 0x4;
  for( bit = 1 ; bit < CC_SPACECURVE_MAPBITS ; bit++ )
  {
    pointindex >>= CC_SPACECURVE_DIMCOUNT;
    p0 |= ( pointindex & 0x1 ) << bit;
    p1 |= ( pointindex & 0x2 ) << bit;
    p2 |= ( pointindex & 0x4 ) << bit;
  }
  point[0] = p0 >> 0;
  point[1] = p1 >> 1;
  point[2] = p2 >> 2;

  return;
}


void bsAntiDebugEnd( bsContext *context )
{
  context->antidebugcaps |= 0x4f;
  context->antidebugtime1 = antiDebugRdTsc101();

  /* antidebugvalue should now be = context->antidebugcaps ^ context->antidebugarch */
  /* context->antidebugtime1 - context->antidebugtime0 should now be a low value */
  /* If any of these two are false, WE ARE BEING DEBUGGED! */

#if 0
  printf( "VALUE : 0x%x 0x%x : 0x%x 0x%x\n", (int)context->antidebugcaps, (int)context->antidebugarch, (int)context->antidebugvalue, (int)( context->antidebugcaps ^ context->antidebugarch ) );
  printf( "VALUE : 0x%x\n", (int)context->antidebugvalue & BS_ANTIDEBUG_CONTEXT_VALUE_MASK );
  printf( "TIME 0x%llx 0x%llx : 0x%llx\n", (long long)context->antidebugtime0, (long long)context->antidebugtime1, (long long)( context->antidebugtime1 - context->antidebugtime0 ) );
#endif

/*
  exit( 0 );
*/

  /* ( ( context->antidebugvalue & BS_ANTIDEBUG_CONTEXT_VALUE_MASK ) == BS_ANTIDEBUG_CONTEXT_VALUE ) guaranteed */
  /* ( ( ( context->antidebugtime1 - context->antidebugtime0 ) & BS_ANTIDEBUG_CONTEXT_TIME_MASK ) == 0 ) guaranteed */
  /* ( context->antidebugtime1 != context->antidebugtime0 ) guaranteed */
  /* ( ccCountBits64( context->antidebugtime1 - context->antidebugtime0 ) != 0 ) guaranteed */
  /* ( ccCountBits32( ( context->antidebugtime1 - context->antidebugtime0 ) >> BS_ANTIDEBUG_CONTEXT_TIME_SHIFT ) == 0 ) guaranteed */



  /* ANTIDEBUG: If conditions fail, fail to compute correct function pointer addresses, leading to crashes later */

  return;
}


////


uint64_t bsAntiDebugCheckCounter = 0;

int32_t bsAntiDebugPadding0[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

volatile uintptr_t bsAntiDebugInitOffset = BS_ANTIDEBUG_INIT_OFFSET;

volatile uint32_t bsAntiDebugBoApplyOffset = BS_ANTIDEBUG_BOAPPLY_RAW;

void __attribute__((noinline)) bsAntiDebugCountGetPointers( bsContext *context, int32_t **antidebugcount0, int32_t **antidebugcount1 )
{
  *antidebugcount0 = ADDRESS( context, offsetof(bsContext,antidebugcount0) + BS_ANTIDEBUGCOUNT_OFFSET0 );
  *antidebugcount1 = ADDRESS( context, offsetof(bsContext,antidebugcount1) + BS_ANTIDEBUGCOUNT_OFFSET1 );
  return;
}


#endif



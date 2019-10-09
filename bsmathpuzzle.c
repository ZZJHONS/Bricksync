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
#include "antidebug.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <signal.h>
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


#if BS_ENABLE_MATHPUZZLE


#define BS_PUZZLE_COLOR_POOL_COUNT (5)

static const char *bsPuzzleColorPool[BS_PUZZLE_COLOR_POOL_COUNT] =
{
  IO_RED,
  IO_GREEN,
  IO_YELLOW,
  IO_MAGENTA,
  IO_CYAN
};

static void bsPuzzleColorString( bsContext *context, ccGrowth *growth, char *string )
{
  int index, lastcolor, color;
  lastcolor = -1;
  for( index = 0 ; string[index] ; index++ )
  {
    do
    {
      color = randInt( &context->randstate ) % BS_PUZZLE_COLOR_POOL_COUNT;
    } while( color == lastcolor );
    ccGrowthPrintf( growth, "%s%c", bsPuzzleColorPool[color], string[index] );
    lastcolor = color;
  }
  return;
}


////


#define BS_PUZZLE_HIDE_PRIME(x) (((x)^0x8f23)+0x295f)
#define BS_PUZZLE_UNHIDE_PRIME(x) (((x)-0x295f)^0x8f23)

#define X(x) BS_PUZZLE_HIDE_PRIME(x)

const int bsPuzzlePrimeTable[] =
{
    X(2),   X(3),   X(5),   X(7),  X(11),  X(13),  X(17),  X(19),  X(23),  X(29),
   X(31),  X(37),  X(41),  X(43),  X(47),  X(53),  X(59),  X(61),  X(67),  X(71),
   X(73),  X(79),  X(83),  X(89),  X(97), X(101), X(103), X(107), X(109), X(113),
  X(127), X(131), X(137), X(139), X(149), X(151), X(157), X(163), X(167), X(173),
  X(179), X(181), X(191), X(193), X(197), X(199), X(211), X(223), X(227), X(229),
  X(233), X(239), X(241), X(251), X(257), X(263), X(269), X(271), X(277), X(281),
  X(283), X(293), X(307), X(311), X(313), X(317), X(331), X(337), X(347), X(349),
  X(353), X(359), X(367), X(373), X(379), X(383), X(389), X(397), X(401), X(409),
  X(419), X(421), X(431), X(433), X(439), X(443), X(449), X(457), X(461), X(463),
  X(467), X(479), X(487), X(491), X(499), X(503), X(509), X(521), X(523), X(541)
};

#undef X


////


void bsPuzzleNextPrime( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int index, base, prime;
  base = 5 + ( randInt( &context->randstate ) & 0x7f );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    base += randInt( &context->randstate ) & 0x7f;
  if( !( randInt( &context->randstate ) & 0x3 ) )
    base += randInt( &context->randstate ) & 0xff;
  for( index = 0 ; ; index++ )
  {
    prime = BS_PUZZLE_UNHIDE_PRIME( bsPuzzlePrimeTable[index] );
    if( base <= prime )
      break;
  }
  if( base == prime )
    base--;
  solution->i = prime;
  ccGrowthPrintf( growth, "What is the first prime number higher than %d?", base );
  context->puzzlesolution.i = prime;
  return;
}


////


void bsPuzzleCountFactors( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int index, base, basework, prime, factorcount;
  base = 5 + ( randInt( &context->randstate ) & 0x1f );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    base += randInt( &context->randstate ) & 0x3f;
  if( !( randInt( &context->randstate ) & 0x3 ) )
    base += randInt( &context->randstate ) & 0x7f;
  basework = base;
  factorcount = 0;
  for( index = 0 ; basework > 1 ; index++ )
  {
    prime = BS_PUZZLE_UNHIDE_PRIME( bsPuzzlePrimeTable[index] );
    for( ; !( basework % prime ) ; )
    {
      basework /= prime;
      factorcount++;
    }
  }
  solution->i = factorcount;
  ccGrowthPrintf( growth, "How many prime factors are there in %d?", base );
  context->puzzlesolution.i = factorcount;
  return;
}


////


void bsPuzzleGreatestCommonDivisor( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int divisor, base0, base1, prime, basemax, gcd, range;
  prime = BS_PUZZLE_UNHIDE_PRIME( bsPuzzlePrimeTable[ randInt( &context->randstate ) & 0x7 ] );
  range = 256 / prime;
  base0 = 5 + ( randInt( &context->randstate ) % range );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    base0 += randInt( &context->randstate ) % range;
  base1 = 5 + ( randInt( &context->randstate ) % range );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    base1 += randInt( &context->randstate ) % range;
  base0 *= prime;
  base1 *= prime;
  basemax = CC_MAX( base0, base1 );
  gcd = 1;
  for( divisor = 2 ; divisor <= basemax ; divisor++ )
  {
    if( !( base0 % divisor ) && !( base1 % divisor ) )
      gcd = divisor;
  }
  context->puzzlesolution.i = gcd;
  ccGrowthPrintf( growth, "What is the greatest common divisor of %d and %d?", base0, base1 );
  solution->i = gcd;
  return;
}


////


void bsPuzzlePowerLastDigit( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int index, base, exponent, lastdigit;
  base = 5 + ( randInt( &context->randstate ) & 0xfff );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    base += randInt( &context->randstate ) & 0xfff;
  exponent = 5 + ( randInt( &context->randstate ) & 0x7 );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    exponent += randInt( &context->randstate ) & 0x7;
  lastdigit = base;
  for( index = 1 ; index < exponent ; index++ )
    lastdigit = ( lastdigit * lastdigit ) % 10;
  context->puzzlesolution.i = lastdigit;
  ccGrowthPrintf( growth, "What is the last decimal digit of %d raised to the power %d?", base, exponent );
  solution->i = lastdigit;
  return;
}


////


void bsPuzzleSolveX( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int value, factor, offset, result;
  value = 5 + ( randInt( &context->randstate ) & 0x1f );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    value += randInt( &context->randstate ) & 0x1f;
  if( !( randInt( &context->randstate ) & 0x1 ) )
    value = -value;
  context->puzzlesolution.i = value;
  factor = 2 + ( randInt( &context->randstate ) & 0x3 );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    factor += randInt( &context->randstate ) & 0x7;
  if( !( randInt( &context->randstate ) & 0x1 ) )
    factor = -factor;
  offset = 2 + ( randInt( &context->randstate ) & 0x7f );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    offset += randInt( &context->randstate ) & 0x7f;
  if( !( randInt( &context->randstate ) & 0x1 ) )
    offset = -offset;
  result = ( factor * value ) + offset;
  solution->i = value;
  ccGrowthPrintf( growth, "If ( %d * x ) %c %d = %d, what is the value of x?", factor, ( offset >= 0 ? '+' : '-' ), abs( offset ), result );
  return;
}


////


void bsPuzzleLineTrace( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int index, pointcount, linecount;
  pointcount = 3 + ( randInt( &context->randstate ) & 0x7 );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    pointcount += randInt( &context->randstate ) & 0x7;
  if( !( randInt( &context->randstate ) & 0x3 ) )
    pointcount += randInt( &context->randstate ) & 0xf;
  linecount = 0;
  for( index = pointcount-1 ; index > 0 ; index-- )
    linecount += index;
  solution->i = linecount;
  ccGrowthPrintf( growth, "If you have %d points on a plane, how many distinct lines can you trace between all points?", pointcount );
  context->puzzlesolution.i = linecount;
  return;
}


////


void bsPuzzleFactorial( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int index, base;
  int64_t factorial;
  base = 2 + ( randInt( &context->randstate ) & 0x3 );
  if( !( randInt( &context->randstate ) & 0x1 ) )
    base += randInt( &context->randstate ) & 0x3;
  if( !( randInt( &context->randstate ) & 0x3 ) )
    base += randInt( &context->randstate ) & 0x7;
  context->puzzlesolution.i = base;
  factorial = 1;
  for( index = base ; index > 0 ; index-- )
    factorial *= index;
  solution->i = base;
  ccGrowthPrintf( growth, "If there are "CC_LLD" different ways to arrange distinct elements in a sequence, how many elements are there?", (long long)factorial );
  return;
}


////


#define BS_PUZZLE_SEQUENCE_LENGTH (8)

void bsPuzzleSequence( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution )
{
  int index, base, indexfactor, indexsquareflag, prevfactor;
  int64_t previous;
  int64_t sequence[BS_PUZZLE_SEQUENCE_LENGTH];
  if( randInt( &context->randstate ) % 6 )
  {
    indexfactor = randInt( &context->randstate ) & 0x1;
    if( randInt( &context->randstate ) & 0x1 )
      indexfactor += randInt( &context->randstate ) & 0x3;
    if( randInt( &context->randstate ) & 0x1 )
      indexfactor = -indexfactor;
    indexsquareflag = 0;
  }
  else
  {
    indexfactor = 0;
    indexsquareflag = 1;
  }
  prevfactor = 0 + ( randInt( &context->randstate ) & 0x1 );
  if( randInt( &context->randstate ) & 0x1 )
    prevfactor += randInt( &context->randstate ) & 0x3;
  if( randInt( &context->randstate ) & 0x1 )
  {
    prevfactor = -prevfactor;
    if( prevfactor == -1 )
      prevfactor = -2;
  }
  base = 0;
  if( ( !( indexfactor ) || !( prevfactor ) ) )
  {
    base = 2 + ( randInt( &context->randstate ) & 0x7 );
    if( ( abs( prevfactor ) < 2 ) && ( randInt( &context->randstate ) & 0x1 ) )
      base += randInt( &context->randstate ) & 0x1f;
  }
  if( randInt( &context->randstate ) & 0x1 )
    base = -base;
  if( ( abs( indexfactor ) < 2 ) && ( !( prevfactor ) || !( base ) ) )
    indexfactor = 2 + ( randInt( &context->randstate ) & 0x7 );
  previous = 0;
  for( index = 0 ; index < BS_PUZZLE_SEQUENCE_LENGTH ; index++ )
  {
    sequence[index] = base + ( index * indexfactor ) + ( prevfactor * previous );
    if( indexsquareflag )
      sequence[index] += index * index;
    previous = sequence[index];
  }
  solution->i = sequence[BS_PUZZLE_SEQUENCE_LENGTH-1];
  ccGrowthPrintf( growth, "What should the next term be in this sequence?  "CC_LLD", "CC_LLD", "CC_LLD", "CC_LLD", "CC_LLD", "CC_LLD", "CC_LLD", ...", (long long)sequence[0], (long long)sequence[1], (long long)sequence[2], (long long)sequence[3], (long long)sequence[4], (long long)sequence[5], (long long)sequence[6] );
  context->puzzlesolution.i = sequence[BS_PUZZLE_SEQUENCE_LENGTH-1];
  return;
}


////


enum
{
  BS_PUZZLE_TYPE_NEXTPRIME,
  BS_PUZZLE_TYPE_COUNTFACTORS,
  BS_PUZZLE_TYPE_GCD,
  BS_PUZZLE_TYPE_POWERLASTDIGIT,
  BS_PUZZLE_TYPE_SOLVEX,
  BS_PUZZLE_TYPE_LINETRACE,
  BS_PUZZLE_TYPE_FACTORIAL,
  BS_PUZZLE_TYPE_SEQUENCE,

  BS_PUZZLE_TYPE_COUNT
};

static void (* const bsPuzzleTable[BS_PUZZLE_TYPE_COUNT])( bsContext *context, ccGrowth *growth, bsPuzzleSolution *solution ) =
{
  bsPuzzleNextPrime,
  bsPuzzleCountFactors,
  bsPuzzleGreatestCommonDivisor,
  bsPuzzlePowerLastDigit,
  bsPuzzleSolveX,
  bsPuzzleLineTrace,
  bsPuzzleFactorial,
  bsPuzzleSequence
};


static int bsPuzzleDecideType( bsContext *context, int previoustype )
{
  int type;
  for( ; ; )
  {
    type = randInt( &context->randstate ) % BS_PUZZLE_TYPE_COUNT;
    if( type != previoustype )
      break;
  }
  return type;
}


static __attribute__((noinline)) int bsPuzzleGetAnswer( bsContext *context, bsPuzzleBundle *puzzlebundle )
{
  int attemptcount, waitcount, retval;
  int64_t readint;
  char *input;

  retval = 0;
  readint = 0;
  input = 0;
  for( attemptcount = 3 ; ; )
  {
    if( input )
      free( input );
    ioPrintf( &context->output, IO_MODEBIT_NOLOG, BSMSG_ANSWER );
    for( waitcount = 0 ; !( input = ioStdinReadAlloc( 2000 ) ) && ( waitcount < 60 ) ; waitcount++ );
    if( !( input ) )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "You are taking too long. BZZZZ.\n" );
      break;
    }
    if( ccStrLowCmpWord( input, "register" ) )
    {
      bsCommandRegister( context, 0, 0 );
      ioPrintf( &context->output, 0, "\n" );
      break;
    }
    if( ccStrLowCmpWord( input, "cancel" ) )
      break;
    if( !( ccStrParseInt64( input, &readint ) ) )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "That answer is not a number. I only understand numbers. BZZZZ.\n" );
      break;
    }
    puzzlebundle->answer.i = readint;
    if( bsPuzzleVerifyAnswer( &puzzlebundle->solution, &puzzlebundle->answer ) )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "Your answer is " IO_GREEN "correct" IO_DEFAULT "! We'll now perform the " IO_CYAN "check" IO_DEFAULT " operation.\n" );
      retval = 1;
      break;
    }
    if( !( --attemptcount ) )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "Your answer is " IO_RED "incorrect" IO_DEFAULT ".\n" );
      ccSleep( 1000 );
      break;
    }
    ioPrintf( &context->output, 0, BSMSG_ERROR "Your answer is " IO_RED "incorrect" IO_WHITE ". You have " IO_MAGENTA "%d" IO_WHITE " attempt%s left.\n", attemptcount, ( attemptcount >= 2 ? "s" : "" ) );
  }
  context->puzzleanswer.i = readint;
  if( input )
    free( input );

  return retval;
}


int bsPuzzleAsk( bsContext *context, bsPuzzleBundle *puzzlebundle )
{
  ccGrowth growth;

  /* Default that to zero here */
  memset( puzzlebundle, 0, sizeof(bsPuzzleBundle) );

  ccGrowthInit( &growth, 512 );
  bsPuzzleColorString( context, &growth, "MATH PUZZLE" );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync is " IO_RED "unregistered" IO_DEFAULT " and your inventory holds more than " IO_RED "%d" IO_DEFAULT " parts, therefore it's %s" IO_DEFAULT " time!\n", BS_LIMITS_MAX_PARTCOUNT, growth.data );
  ccGrowthFree( &growth );
  ioPrintf( &context->output, 0, BSMSG_INFO "You can type " IO_CYAN "register" IO_DEFAULT " for more information, or " IO_MAGENTA "cancel" IO_DEFAULT " to abort the order " IO_CYAN "check" IO_DEFAULT ".\n" );

  context->puzzlequestiontype = bsPuzzleDecideType( context, context->puzzlequestiontype );

  ccGrowthInit( &growth, 512 );
  bsPuzzleTable[context->puzzlequestiontype]( context, &growth, &puzzlebundle->solution );
  ioPrintf( &context->output, 0, BSMSG_QUESTION "%s" IO_DEFAULT "\n", growth.data );
  ccGrowthFree( &growth );

  if( !( bsPuzzleGetAnswer( context, puzzlebundle ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_CYAN "check" IO_DEFAULT " operation has been aborted.\n\n" );
    ccSleep( 1000 );
    return 0;
  }

  return 1;
}


#endif


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
#include <limits.h>
#include <float.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"

#define BN_XP_SUPPORT_128 0
#define BN_XP_SUPPORT_192 0
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 1
#define BN_XP_SUPPORT_1024 1
#include "bn.h"


/*
Big number fixed point two-complement arithmetics, 1024 bits

On 32 bits, this file *MUST* be compiled with -fomit-frame-pointer to free the %ebp register
In fact, don't even think about compiling 32 bits. This is much faster on 64 bits.
*/

#define BN1024_PRINT_REPORT



#if BN_UNIT_BITS == 64
 #define PRINT(p,x) printf( p "%016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx  %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx\n", (long)(x)->unit[8+7], (long)(x)->unit[8+6], (long)(x)->unit[8+5], (long)(x)->unit[8+4], (long)(x)->unit[8+3], (long)(x)->unit[8+2], (long)(x)->unit[8+1], (long)(x)->unit[8+0], (long)(x)->unit[7], (long)(x)->unit[6], (long)(x)->unit[5], (long)(x)->unit[4], (long)(x)->unit[3], (long)(x)->unit[2], (long)(x)->unit[1], (long)(x)->unit[0] )
#else
 #define PRINT(p,x)
#endif



static inline __attribute__((always_inline)) void bn1024Copy( bn1024 *dst, bnUnit *src )
{
  dst->unit[0] = src[0];
  dst->unit[1] = src[1];
  dst->unit[2] = src[2];
  dst->unit[3] = src[3];
  dst->unit[4] = src[4];
  dst->unit[5] = src[5];
  dst->unit[6] = src[6];
  dst->unit[7] = src[7];
  dst->unit[8] = src[8];
  dst->unit[9] = src[9];
  dst->unit[10] = src[10];
  dst->unit[11] = src[11];
  dst->unit[12] = src[12];
  dst->unit[13] = src[13];
  dst->unit[14] = src[14];
  dst->unit[15] = src[15];
#if BN_UNIT_BITS == 32
  dst->unit[16+0] = src[16+0];
  dst->unit[16+1] = src[16+1];
  dst->unit[16+2] = src[16+2];
  dst->unit[16+3] = src[16+3];
  dst->unit[16+4] = src[16+4];
  dst->unit[16+5] = src[16+5];
  dst->unit[16+6] = src[16+6];
  dst->unit[16+7] = src[16+7];
  dst->unit[16+8] = src[16+8];
  dst->unit[16+9] = src[16+9];
  dst->unit[16+10] = src[16+10];
  dst->unit[16+11] = src[16+11];
  dst->unit[16+12] = src[16+12];
  dst->unit[16+13] = src[16+13];
  dst->unit[16+14] = src[16+14];
  dst->unit[16+15] = src[16+15];
#endif
  return;
}

static inline __attribute__((always_inline)) void bn1024CopyShift( bn1024 *dst, bnUnit *src, bnShift shift )
{
  bnShift shiftinv;
  shiftinv = BN_UNIT_BITS - shift;
  dst->unit[0] = ( src[0] >> shift ) | ( src[1] << shiftinv );
  dst->unit[1] = ( src[1] >> shift ) | ( src[2] << shiftinv );
  dst->unit[2] = ( src[2] >> shift ) | ( src[3] << shiftinv );
  dst->unit[3] = ( src[3] >> shift ) | ( src[4] << shiftinv );
  dst->unit[4] = ( src[4] >> shift ) | ( src[5] << shiftinv );
  dst->unit[5] = ( src[5] >> shift ) | ( src[6] << shiftinv );
  dst->unit[6] = ( src[6] >> shift ) | ( src[7] << shiftinv );
  dst->unit[7] = ( src[7] >> shift ) | ( src[8] << shiftinv );
  dst->unit[8] = ( src[8] >> shift ) | ( src[9] << shiftinv );
  dst->unit[9] = ( src[9] >> shift ) | ( src[10] << shiftinv );
  dst->unit[10] = ( src[10] >> shift ) | ( src[11] << shiftinv );
  dst->unit[11] = ( src[11] >> shift ) | ( src[12] << shiftinv );
  dst->unit[12] = ( src[12] >> shift ) | ( src[13] << shiftinv );
  dst->unit[13] = ( src[13] >> shift ) | ( src[14] << shiftinv );
  dst->unit[14] = ( src[14] >> shift ) | ( src[15] << shiftinv );
  dst->unit[15] = ( src[15] >> shift ) | ( src[16] << shiftinv );
#if BN_UNIT_BITS == 32
  dst->unit[16+0] = ( src[16+0] >> shift ) | ( src[16+1] << shiftinv );
  dst->unit[16+1] = ( src[16+1] >> shift ) | ( src[16+2] << shiftinv );
  dst->unit[16+2] = ( src[16+2] >> shift ) | ( src[16+3] << shiftinv );
  dst->unit[16+3] = ( src[16+3] >> shift ) | ( src[16+4] << shiftinv );
  dst->unit[16+4] = ( src[16+4] >> shift ) | ( src[16+5] << shiftinv );
  dst->unit[16+5] = ( src[16+5] >> shift ) | ( src[16+6] << shiftinv );
  dst->unit[16+6] = ( src[16+6] >> shift ) | ( src[16+7] << shiftinv );
  dst->unit[16+7] = ( src[16+7] >> shift ) | ( src[16+8] << shiftinv );
  dst->unit[16+8] = ( src[16+8] >> shift ) | ( src[16+9] << shiftinv );
  dst->unit[16+9] = ( src[16+9] >> shift ) | ( src[16+10] << shiftinv );
  dst->unit[16+10] = ( src[16+10] >> shift ) | ( src[16+11] << shiftinv );
  dst->unit[16+11] = ( src[16+11] >> shift ) | ( src[16+12] << shiftinv );
  dst->unit[16+12] = ( src[16+12] >> shift ) | ( src[16+13] << shiftinv );
  dst->unit[16+13] = ( src[16+13] >> shift ) | ( src[16+14] << shiftinv );
  dst->unit[16+14] = ( src[16+14] >> shift ) | ( src[16+15] << shiftinv );
  dst->unit[16+15] = ( src[16+15] >> shift ) | ( src[16+16] << shiftinv );
#endif
  return;
}

static inline __attribute__((always_inline)) int bn1024HighNonMatch( const bn1024 * const src, bnUnit value )
{
  int high;
  high = 0;
#if BN_UNIT_BITS == 64
  if( src->unit[15] != value )
    high = 15;
  else if( src->unit[14] != value )
    high = 14;
  else if( src->unit[13] != value )
    high = 13;
  else if( src->unit[12] != value )
    high = 12;
  else if( src->unit[11] != value )
    high = 11;
  else if( src->unit[10] != value )
    high = 10;
  else if( src->unit[9] != value )
    high = 9;
  else if( src->unit[8] != value )
    high = 8;
  else if( src->unit[7] != value )
    high = 7;
  else if( src->unit[6] != value )
    high = 6;
  else if( src->unit[5] != value )
    high = 5;
  else if( src->unit[4] != value )
    high = 4;
  else if( src->unit[3] != value )
    high = 3;
  else if( src->unit[2] != value )
    high = 2;
  else if( src->unit[1] != value )
    high = 1;
#else
  if( src->unit[16+15] != value )
    high = 16+15;
  else if( src->unit[16+14] != value )
    high = 16+14;
  else if( src->unit[16+13] != value )
    high = 16+13;
  else if( src->unit[16+12] != value )
    high = 16+12;
  else if( src->unit[16+11] != value )
    high = 16+11;
  else if( src->unit[16+10] != value )
    high = 16+10;
  else if( src->unit[16+9] != value )
    high = 16+9;
  else if( src->unit[16+8] != value )
    high = 16+8;
  else if( src->unit[16+7] != value )
    high = 16+7;
  else if( src->unit[16+6] != value )
    high = 16+6;
  else if( src->unit[16+5] != value )
    high = 16+5;
  else if( src->unit[16+4] != value )
    high = 16+4;
  else if( src->unit[16+3] != value )
    high = 16+3;
  else if( src->unit[16+2] != value )
    high = 16+2;
  else if( src->unit[16+1] != value )
    high = 16+1;
  else if( src->unit[16+0] != value )
    high = 16+0;
  else if( src->unit[15] != value )
    high = 15;
  else if( src->unit[14] != value )
    high = 14;
  else if( src->unit[13] != value )
    high = 13;
  else if( src->unit[12] != value )
    high = 12;
  else if( src->unit[11] != value )
    high = 11;
  else if( src->unit[10] != value )
    high = 10;
  else if( src->unit[9] != value )
    high = 9;
  else if( src->unit[8] != value )
    high = 8;
  else if( src->unit[7] != value )
    high = 7;
  else if( src->unit[6] != value )
    high = 6;
  else if( src->unit[5] != value )
    high = 5;
  else if( src->unit[4] != value )
    high = 4;
  else if( src->unit[3] != value )
    high = 3;
  else if( src->unit[2] != value )
    high = 2;
  else if( src->unit[1] != value )
    high = 1;
#endif
  return high;
}


static inline bnUnit bn1024SignMask( const bn1024 * const src )
{
  return ( (bnUnitSigned)src->unit[BN_INT1024_UNIT_COUNT-1] ) >> (BN_UNIT_BITS-1);
}



////



void bn1024Zero( bn1024 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn1024Set( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src->unit[unitindex];
  return;
}


void bn1024Set32( bn1024 *dst, uint32_t value )
{
  int unitindex;
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn1024Set32Signed( bn1024 *dst, int32_t value )
{
  int unitindex;
  bnUnit u;
  u = ( (bnUnitSigned)value ) >> (BN_UNIT_BITS-1);
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = u;
  return;
}


void bn1024Set32Shl( bn1024 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  shift &= BN_UNIT_BITS-1;
  vl = value;
  vh = 0;
  if( shift )
  {
    vl = vl << shift;
    vh = ( (bnUnit)value ) >> ( BN_UNIT_BITS - shift );
  }
  if( (unsigned)unitindex < BN_INT1024_UNIT_COUNT )
    dst->unit[unitindex] = vl;
  if( (unsigned)++unitindex < BN_INT1024_UNIT_COUNT )
    dst->unit[unitindex] = vh;
  return;
}


void bn1024Set32SignedShl( bn1024 *dst, int32_t value, bnShift shift )
{
  if( value < 0 )
  {
    bn1024Set32Shl( dst, -value, shift );
    bn1024Neg( dst );
  }
  else
    bn1024Set32Shl( dst, value, shift );
  return;
}


#define BN_IEEE754DBL_MANTISSA_BITS (52)
#define BN_IEEE754DBL_EXPONENT_BITS (11)

void bn1024SetDouble( bn1024 *dst, double value, bnShift leftshiftbits )
{
  int unitindex, unitbits, exponent, sign, base, mantissabits;
  uint64_t mantissa;
  union
  {
    double d;
    uint64_t u;
  } dblunion;

  dblunion.d = value;
  sign = dblunion.u >> 63;
  mantissa = dblunion.u & ( ( (uint64_t)1 << BN_IEEE754DBL_MANTISSA_BITS ) - 1 );
  exponent = ( dblunion.u >> BN_IEEE754DBL_MANTISSA_BITS ) & ( ( (uint64_t)1 << BN_IEEE754DBL_EXPONENT_BITS ) - 1 );

  /* Bit-leading mantissa, except for denormals */
  if( exponent )
    mantissa |= (uint64_t)1 << BN_IEEE754DBL_MANTISSA_BITS;
  exponent -= ( 1 << (BN_IEEE754DBL_EXPONENT_BITS-1) ) - 1;

#ifdef BN_SETDBL_DEBUG
{
printf( "Double   : %f\n", value );
printf( "Mantissa : 0x%llx\n", (long long)mantissa );
printf( "         : 0x%llx\n", (long long)1 << (BN_IEEE754DBL_MANTISSA_BITS+1) );
printf( "Exponent : %d\n", exponent );
printf( "Sign     : %d\n", sign );
}
#endif

  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;

  /* Where the mantissa starts */
  base = leftshiftbits + exponent + 1;
  mantissabits = BN_IEEE754DBL_MANTISSA_BITS+1;
  unitindex = base >> BN_UNIT_BIT_SHIFT;
  for( ; mantissabits > 0 ; )
  {
    unitbits = base & (BN_UNIT_BITS-1);
    if( !( unitbits ) )
    {
      unitbits = BN_UNIT_BITS;
      unitindex--;
    }
    base -= unitbits;
    if( (unsigned)unitindex < BN_INT1024_UNIT_COUNT )
    {
      if( mantissabits < unitbits )
        dst->unit[unitindex] = mantissa << ( unitbits - mantissabits );
      else
        dst->unit[unitindex] = mantissa >> ( mantissabits - unitbits );
    }
    mantissabits -= unitbits;
  }

  if( sign )
  {
    for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = ~dst->unit[unitindex];
    bn1024Add32( dst, 1 );
  }

  return;
}


double bn1024GetDouble( bn1024 *dst, bnShift rightshiftbits )
{
  int exponent, sign, msb;
  uint64_t mantissa;
  union
  {
    double d;
    uint64_t u;
  } dblunion;
  bn1024 value;

  sign = 0;
  if( ( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn1024SetNeg( &value, dst );
    sign = 1;
  }
  else
    bn1024Set( &value, dst );

  msb = bn1024GetIndexMSB( &value );
  if( msb < 0 )
    return ( sign ? -0.0 : 0.0 );
  mantissa = bn1024Extract64( &value, msb-64 );
  mantissa >>= 64-BN_IEEE754DBL_MANTISSA_BITS;

  exponent = msb - rightshiftbits;
  exponent += ( 1 << (BN_IEEE754DBL_EXPONENT_BITS-1) ) - 1;
  if( exponent < 0 )
  {
    /* TODO: Handle denormals? */
    return ( sign ? -0.0 : 0.0 );
  }
  else if( exponent >= (1<<BN_IEEE754DBL_EXPONENT_BITS) )
    return ( sign ? -INFINITY : INFINITY );

  dblunion.u = mantissa | ( (uint64_t)exponent << BN_IEEE754DBL_MANTISSA_BITS ) | ( (uint64_t)sign << 63 );
  return dblunion.d;
}


void bn1024SetBit( bn1024 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] |= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


void bn1024ClearBit( bn1024 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] &= ~( (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) ) );
  return;
}


void bn1024FlipBit( bn1024 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] ^= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


////


void bn1024Add32( bn1024 *dst, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "addq %1,0(%0)\n"
    "jnc 1f\n"
    "addq $1,8(%0)\n"
    "jnc 1f\n"
    "addq $1,16(%0)\n"
    "jnc 1f\n"
    "addq $1,24(%0)\n"
    "jnc 1f\n"
    "addq $1,32(%0)\n"
    "jnc 1f\n"
    "addq $1,40(%0)\n"
    "jnc 1f\n"
    "addq $1,48(%0)\n"
    "jnc 1f\n"
    "addq $1,56(%0)\n"
    "jnc 1f\n"
    "addq $1,64(%0)\n"
    "jnc 1f\n"
    "addq $1,72(%0)\n"
    "jnc 1f\n"
    "addq $1,80(%0)\n"
    "jnc 1f\n"
    "addq $1,88(%0)\n"
    "jnc 1f\n"
    "addq $1,96(%0)\n"
    "jnc 1f\n"
    "addq $1,104(%0)\n"
    "jnc 1f\n"
    "addq $1,112(%0)\n"
    "jnc 1f\n"
    "addq $1,120(%0)\n"
    "1:\n"
    :
    : "r" (dst->unit), "r" ((uint64_t)value)
    : "memory" );
#else
  __asm__ __volatile__(
    "addl %1,0(%0)\n"
    "jnc 1f\n"
    "addl $1,4(%0)\n"
    "jnc 1f\n"
    "addl $1,8(%0)\n"
    "jnc 1f\n"
    "addl $1,12(%0)\n"
    "jnc 1f\n"
    "addl $1,16(%0)\n"
    "jnc 1f\n"
    "addl $1,20(%0)\n"
    "jnc 1f\n"
    "addl $1,24(%0)\n"
    "jnc 1f\n"
    "addl $1,28(%0)\n"
    "jnc 1f\n"
    "addl $1,32(%0)\n"
    "jnc 1f\n"
    "addl $1,36(%0)\n"
    "jnc 1f\n"
    "addl $1,40(%0)\n"
    "jnc 1f\n"
    "addl $1,44(%0)\n"
    "jnc 1f\n"
    "addl $1,48(%0)\n"
    "jnc 1f\n"
    "addl $1,52(%0)\n"
    "jnc 1f\n"
    "addl $1,56(%0)\n"
    "jnc 1f\n"
    "addl $1,60(%0)\n"
    "jnc 1f\n"
    "addl $1,64(%0)\n"
    "jnc 1f\n"
    "addl $1,68(%0)\n"
    "jnc 1f\n"
    "addl $1,72(%0)\n"
    "jnc 1f\n"
    "addl $1,76(%0)\n"
    "jnc 1f\n"
    "addl $1,80(%0)\n"
    "jnc 1f\n"
    "addl $1,84(%0)\n"
    "jnc 1f\n"
    "addl $1,88(%0)\n"
    "jnc 1f\n"
    "addl $1,92(%0)\n"
    "jnc 1f\n"
    "addl $1,96(%0)\n"
    "jnc 1f\n"
    "addl $1,100(%0)\n"
    "jnc 1f\n"
    "addl $1,104(%0)\n"
    "jnc 1f\n"
    "addl $1,108(%0)\n"
    "jnc 1f\n"
    "addl $1,112(%0)\n"
    "jnc 1f\n"
    "addl $1,116(%0)\n"
    "jnc 1f\n"
    "addl $1,120(%0)\n"
    "jnc 1f\n"
    "addl $1,124(%0)\n"
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn1024Sub32( bn1024 *dst, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "subq %1,0(%0)\n"
    "jnc 1f\n"
    "subq $1,8(%0)\n"
    "jnc 1f\n"
    "subq $1,16(%0)\n"
    "jnc 1f\n"
    "subq $1,24(%0)\n"
    "jnc 1f\n"
    "subq $1,32(%0)\n"
    "jnc 1f\n"
    "subq $1,40(%0)\n"
    "jnc 1f\n"
    "subq $1,48(%0)\n"
    "jnc 1f\n"
    "subq $1,56(%0)\n"
    "jnc 1f\n"
    "subq $1,64(%0)\n"
    "jnc 1f\n"
    "subq $1,72(%0)\n"
    "jnc 1f\n"
    "subq $1,80(%0)\n"
    "jnc 1f\n"
    "subq $1,88(%0)\n"
    "jnc 1f\n"
    "subq $1,96(%0)\n"
    "jnc 1f\n"
    "subq $1,104(%0)\n"
    "jnc 1f\n"
    "subq $1,112(%0)\n"
    "jnc 1f\n"
    "subq $1,120(%0)\n"
    "1:\n"
    :
    : "r" (dst->unit), "r" ((uint64_t)value)
    : "memory" );
#else
  __asm__ __volatile__(
    "subl %1,0(%0)\n"
    "jnc 1f\n"
    "subl $1,4(%0)\n"
    "jnc 1f\n"
    "subl $1,8(%0)\n"
    "jnc 1f\n"
    "subl $1,12(%0)\n"
    "jnc 1f\n"
    "subl $1,16(%0)\n"
    "jnc 1f\n"
    "subl $1,20(%0)\n"
    "jnc 1f\n"
    "subl $1,24(%0)\n"
    "jnc 1f\n"
    "subl $1,28(%0)\n"
    "jnc 1f\n"
    "subl $1,32(%0)\n"
    "jnc 1f\n"
    "subl $1,36(%0)\n"
    "jnc 1f\n"
    "subl $1,40(%0)\n"
    "jnc 1f\n"
    "subl $1,44(%0)\n"
    "jnc 1f\n"
    "subl $1,48(%0)\n"
    "jnc 1f\n"
    "subl $1,52(%0)\n"
    "jnc 1f\n"
    "subl $1,56(%0)\n"
    "jnc 1f\n"
    "subl $1,60(%0)\n"
    "jnc 1f\n"
    "subl $1,64(%0)\n"
    "jnc 1f\n"
    "subl $1,68(%0)\n"
    "jnc 1f\n"
    "subl $1,72(%0)\n"
    "jnc 1f\n"
    "subl $1,76(%0)\n"
    "jnc 1f\n"
    "subl $1,80(%0)\n"
    "jnc 1f\n"
    "subl $1,84(%0)\n"
    "jnc 1f\n"
    "subl $1,88(%0)\n"
    "jnc 1f\n"
    "subl $1,92(%0)\n"
    "jnc 1f\n"
    "subl $1,96(%0)\n"
    "jnc 1f\n"
    "subl $1,100(%0)\n"
    "jnc 1f\n"
    "subl $1,104(%0)\n"
    "jnc 1f\n"
    "subl $1,108(%0)\n"
    "jnc 1f\n"
    "subl $1,112(%0)\n"
    "jnc 1f\n"
    "subl $1,116(%0)\n"
    "jnc 1f\n"
    "subl $1,120(%0)\n"
    "jnc 1f\n"
    "subl $1,124(%0)\n"
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn1024Add32s( bn1024 *dst, int32_t value )
{
  if( value < 0 )
    bn1024Sub32( dst, -value );
  else
    bn1024Add32( dst, value );
  return;
}


void bn1024Sub32s( bn1024 *dst, int32_t value )
{
  if( value < 0 )
    bn1024Add32( dst, -value );
  else
    bn1024Sub32( dst, value );
  return;
}


void bn1024Add32Shl( bn1024 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bn1024 adder;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    adder.unit[unitindex] = 0;
  vl = value;
  vh = 0;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  shift &= BN_UNIT_BITS-1;
  if( shift )
  {
    vl = vl << shift;
    vh = ( (bnUnit)value ) >> ( BN_UNIT_BITS - shift );
  }
  if( (unsigned)(unitindex+0) < BN_INT1024_UNIT_COUNT )
    adder.unit[unitindex+0] = vl;
  if( (unsigned)(unitindex+1) < BN_INT1024_UNIT_COUNT )
    adder.unit[unitindex+1] = vh;
  bn1024Add( dst, &adder );
  return;
}


void bn1024Sub32Shl( bn1024 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bn1024 adder;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    adder.unit[unitindex] = 0;
  vl = value;
  vh = 0;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  shift &= BN_UNIT_BITS-1;
  if( shift )
  {
    vl = vl << shift;
    vh = ( (bnUnit)value ) >> ( BN_UNIT_BITS - shift );
  }
  if( (unsigned)(unitindex+0) < BN_INT1024_UNIT_COUNT )
    adder.unit[unitindex+0] = vl;
  if( (unsigned)(unitindex+1) < BN_INT1024_UNIT_COUNT )
    adder.unit[unitindex+1] = vh;
  bn1024Sub( dst, &adder );
  return;
}


void bn1024Add( bn1024 *dst, const bn1024 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq %%rax,0(%0)\n"
    "adcq %%rbx,8(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "adcq %%rcx,16(%0)\n"
    "adcq %%rdx,24(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "adcq %%rax,32(%0)\n"
    "adcq %%rbx,40(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "adcq %%rcx,48(%0)\n"
    "adcq %%rdx,56(%0)\n"
    "movq 64(%1),%%rax\n"
    "movq 72(%1),%%rbx\n"
    "adcq %%rax,64(%0)\n"
    "adcq %%rbx,72(%0)\n"
    "movq 80(%1),%%rcx\n"
    "movq 88(%1),%%rdx\n"
    "adcq %%rcx,80(%0)\n"
    "adcq %%rdx,88(%0)\n"
    "movq 96(%1),%%rax\n"
    "movq 104(%1),%%rbx\n"
    "adcq %%rax,96(%0)\n"
    "adcq %%rbx,104(%0)\n"
    "movq 112(%1),%%rcx\n"
    "movq 120(%1),%%rdx\n"
    "adcq %%rcx,112(%0)\n"
    "adcq %%rdx,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "addl %%eax,0(%0)\n"
    "adcl %%ebx,4(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "adcl %%ecx,8(%0)\n"
    "adcl %%edx,12(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "adcl %%eax,16(%0)\n"
    "adcl %%ebx,20(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "adcl %%ecx,24(%0)\n"
    "adcl %%edx,28(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "adcl %%eax,32(%0)\n"
    "adcl %%ebx,36(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "adcl %%ecx,40(%0)\n"
    "adcl %%edx,44(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "adcl %%eax,48(%0)\n"
    "adcl %%ebx,52(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "adcl %%ecx,56(%0)\n"
    "adcl %%edx,60(%0)\n"
    "movl 64(%1),%%eax\n"
    "movl 68(%1),%%ebx\n"
    "adcl %%eax,64(%0)\n"
    "adcl %%ebx,68(%0)\n"
    "movl 72(%1),%%ecx\n"
    "movl 76(%1),%%edx\n"
    "adcl %%ecx,72(%0)\n"
    "adcl %%edx,76(%0)\n"
    "movl 80(%1),%%eax\n"
    "movl 84(%1),%%ebx\n"
    "adcl %%eax,80(%0)\n"
    "adcl %%ebx,84(%0)\n"
    "movl 88(%1),%%ecx\n"
    "movl 92(%1),%%edx\n"
    "adcl %%ecx,88(%0)\n"
    "adcl %%edx,92(%0)\n"
    "movl 96(%1),%%eax\n"
    "movl 100(%1),%%ebx\n"
    "adcl %%eax,96(%0)\n"
    "adcl %%ebx,100(%0)\n"
    "movl 104(%1),%%ecx\n"
    "movl 108(%1),%%edx\n"
    "adcl %%ecx,104(%0)\n"
    "adcl %%edx,108(%0)\n"
    "movl 112(%1),%%eax\n"
    "movl 116(%1),%%ebx\n"
    "adcl %%eax,112(%0)\n"
    "adcl %%ebx,116(%0)\n"
    "movl 120(%1),%%ecx\n"
    "movl 124(%1),%%edx\n"
    "adcl %%ecx,120(%0)\n"
    "adcl %%edx,124(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024SetAdd( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "adcq 16(%2),%%rcx\n"
    "adcq 24(%2),%%rdx\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "adcq 32(%2),%%rax\n"
    "adcq 40(%2),%%rbx\n"
    "movq %%rax,32(%0)\n"
    "movq %%rbx,40(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "adcq 48(%2),%%rcx\n"
    "adcq 56(%2),%%rdx\n"
    "movq %%rcx,48(%0)\n"
    "movq %%rdx,56(%0)\n"
    "movq 64(%1),%%rax\n"
    "movq 72(%1),%%rbx\n"
    "adcq 64(%2),%%rax\n"
    "adcq 72(%2),%%rbx\n"
    "movq %%rax,64(%0)\n"
    "movq %%rbx,72(%0)\n"
    "movq 80(%1),%%rcx\n"
    "movq 88(%1),%%rdx\n"
    "adcq 80(%2),%%rcx\n"
    "adcq 88(%2),%%rdx\n"
    "movq %%rcx,80(%0)\n"
    "movq %%rdx,88(%0)\n"
    "movq 96(%1),%%rax\n"
    "movq 104(%1),%%rbx\n"
    "adcq 96(%2),%%rax\n"
    "adcq 104(%2),%%rbx\n"
    "movq %%rax,96(%0)\n"
    "movq %%rbx,104(%0)\n"
    "movq 112(%1),%%rcx\n"
    "movq 120(%1),%%rdx\n"
    "adcq 112(%2),%%rcx\n"
    "adcq 120(%2),%%rdx\n"
    "movq %%rcx,112(%0)\n"
    "movq %%rdx,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "addl 0(%2),%%eax\n"
    "adcl 4(%2),%%ebx\n"
    "movl %%eax,0(%0)\n"
    "movl %%ebx,4(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "adcl 8(%2),%%ecx\n"
    "adcl 12(%2),%%edx\n"
    "movl %%ecx,8(%0)\n"
    "movl %%edx,12(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "adcl 16(%2),%%eax\n"
    "adcl 20(%2),%%ebx\n"
    "movl %%eax,16(%0)\n"
    "movl %%ebx,20(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "adcl 24(%2),%%ecx\n"
    "adcl 28(%2),%%edx\n"
    "movl %%ecx,24(%0)\n"
    "movl %%edx,28(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "adcl 32(%2),%%eax\n"
    "adcl 36(%2),%%ebx\n"
    "movl %%eax,32(%0)\n"
    "movl %%ebx,36(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "adcl 40(%2),%%ecx\n"
    "adcl 44(%2),%%edx\n"
    "movl %%ecx,40(%0)\n"
    "movl %%edx,44(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "adcl 48(%2),%%eax\n"
    "adcl 52(%2),%%ebx\n"
    "movl %%eax,48(%0)\n"
    "movl %%ebx,52(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "adcl 56(%2),%%ecx\n"
    "adcl 60(%2),%%edx\n"
    "movl %%ecx,56(%0)\n"
    "movl %%edx,60(%0)\n"
    "movl 64(%1),%%eax\n"
    "movl 68(%1),%%ebx\n"
    "adcl 64(%2),%%eax\n"
    "adcl 68(%2),%%ebx\n"
    "movl %%eax,64(%0)\n"
    "movl %%ebx,68(%0)\n"
    "movl 72(%1),%%ecx\n"
    "movl 76(%1),%%edx\n"
    "adcl 72(%2),%%ecx\n"
    "adcl 76(%2),%%edx\n"
    "movl %%ecx,72(%0)\n"
    "movl %%edx,76(%0)\n"
    "movl 80(%1),%%eax\n"
    "movl 84(%1),%%ebx\n"
    "adcl 80(%2),%%eax\n"
    "adcl 84(%2),%%ebx\n"
    "movl %%eax,80(%0)\n"
    "movl %%ebx,84(%0)\n"
    "movl 88(%1),%%ecx\n"
    "movl 92(%1),%%edx\n"
    "adcl 88(%2),%%ecx\n"
    "adcl 92(%2),%%edx\n"
    "movl %%ecx,88(%0)\n"
    "movl %%edx,92(%0)\n"
    "movl 96(%1),%%eax\n"
    "movl 100(%1),%%ebx\n"
    "adcl 96(%2),%%eax\n"
    "adcl 100(%2),%%ebx\n"
    "movl %%eax,96(%0)\n"
    "movl %%ebx,100(%0)\n"
    "movl 104(%1),%%ecx\n"
    "movl 108(%1),%%edx\n"
    "adcl 104(%2),%%ecx\n"
    "adcl 108(%2),%%edx\n"
    "movl %%ecx,104(%0)\n"
    "movl %%edx,108(%0)\n"
    "movl 112(%1),%%eax\n"
    "movl 116(%1),%%ebx\n"
    "adcl 112(%2),%%eax\n"
    "adcl 116(%2),%%ebx\n"
    "movl %%eax,112(%0)\n"
    "movl %%ebx,116(%0)\n"
    "movl 120(%1),%%ecx\n"
    "movl 124(%1),%%edx\n"
    "adcl 120(%2),%%ecx\n"
    "adcl 124(%2),%%edx\n"
    "movl %%ecx,120(%0)\n"
    "movl %%edx,124(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024Sub( bn1024 *dst, const bn1024 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,0(%0)\n"
    "sbbq %%rbx,8(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "sbbq %%rcx,16(%0)\n"
    "sbbq %%rdx,24(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "sbbq %%rax,32(%0)\n"
    "sbbq %%rbx,40(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "sbbq %%rcx,48(%0)\n"
    "sbbq %%rdx,56(%0)\n"
    "movq 64(%1),%%rax\n"
    "movq 72(%1),%%rbx\n"
    "sbbq %%rax,64(%0)\n"
    "sbbq %%rbx,72(%0)\n"
    "movq 80(%1),%%rcx\n"
    "movq 88(%1),%%rdx\n"
    "sbbq %%rcx,80(%0)\n"
    "sbbq %%rdx,88(%0)\n"
    "movq 96(%1),%%rax\n"
    "movq 104(%1),%%rbx\n"
    "sbbq %%rax,96(%0)\n"
    "sbbq %%rbx,104(%0)\n"
    "movq 112(%1),%%rcx\n"
    "movq 120(%1),%%rdx\n"
    "sbbq %%rcx,112(%0)\n"
    "sbbq %%rdx,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,0(%0)\n"
    "sbbl %%ebx,4(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,8(%0)\n"
    "sbbl %%edx,12(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,16(%0)\n"
    "sbbl %%ebx,20(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl %%ecx,24(%0)\n"
    "sbbl %%edx,28(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "sbbl %%eax,32(%0)\n"
    "sbbl %%ebx,36(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "sbbl %%ecx,40(%0)\n"
    "sbbl %%edx,44(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "sbbl %%eax,48(%0)\n"
    "sbbl %%ebx,52(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "sbbl %%ecx,56(%0)\n"
    "sbbl %%edx,60(%0)\n"
    "movl 64(%1),%%eax\n"
    "movl 68(%1),%%ebx\n"
    "sbbl %%eax,64(%0)\n"
    "sbbl %%ebx,68(%0)\n"
    "movl 72(%1),%%ecx\n"
    "movl 76(%1),%%edx\n"
    "sbbl %%ecx,72(%0)\n"
    "sbbl %%edx,76(%0)\n"
    "movl 80(%1),%%eax\n"
    "movl 84(%1),%%ebx\n"
    "sbbl %%eax,80(%0)\n"
    "sbbl %%ebx,84(%0)\n"
    "movl 88(%1),%%ecx\n"
    "movl 92(%1),%%edx\n"
    "sbbl %%ecx,88(%0)\n"
    "sbbl %%edx,92(%0)\n"
    "movl 96(%1),%%eax\n"
    "movl 100(%1),%%ebx\n"
    "sbbl %%eax,96(%0)\n"
    "sbbl %%ebx,100(%0)\n"
    "movl 104(%1),%%ecx\n"
    "movl 108(%1),%%edx\n"
    "sbbl %%ecx,104(%0)\n"
    "sbbl %%edx,108(%0)\n"
    "movl 112(%1),%%eax\n"
    "movl 116(%1),%%ebx\n"
    "sbbl %%eax,112(%0)\n"
    "sbbl %%ebx,116(%0)\n"
    "movl 120(%1),%%ecx\n"
    "movl 124(%1),%%edx\n"
    "sbbl %%ecx,120(%0)\n"
    "sbbl %%edx,124(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024SetSub( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq 0(%2),%%rax\n"
    "sbbq 8(%2),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "sbbq 16(%2),%%rcx\n"
    "sbbq 24(%2),%%rdx\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "sbbq 32(%2),%%rax\n"
    "sbbq 40(%2),%%rbx\n"
    "movq %%rax,32(%0)\n"
    "movq %%rbx,40(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "sbbq 48(%2),%%rcx\n"
    "sbbq 56(%2),%%rdx\n"
    "movq %%rcx,48(%0)\n"
    "movq %%rdx,56(%0)\n"
    "movq 64(%1),%%rax\n"
    "movq 72(%1),%%rbx\n"
    "sbbq 64(%2),%%rax\n"
    "sbbq 72(%2),%%rbx\n"
    "movq %%rax,64(%0)\n"
    "movq %%rbx,72(%0)\n"
    "movq 80(%1),%%rcx\n"
    "movq 88(%1),%%rdx\n"
    "sbbq 80(%2),%%rcx\n"
    "sbbq 88(%2),%%rdx\n"
    "movq %%rcx,80(%0)\n"
    "movq %%rdx,88(%0)\n"
    "movq 96(%1),%%rax\n"
    "movq 104(%1),%%rbx\n"
    "sbbq 96(%2),%%rax\n"
    "sbbq 104(%2),%%rbx\n"
    "movq %%rax,96(%0)\n"
    "movq %%rbx,104(%0)\n"
    "movq 112(%1),%%rcx\n"
    "movq 120(%1),%%rdx\n"
    "sbbq 112(%2),%%rcx\n"
    "sbbq 120(%2),%%rdx\n"
    "movq %%rcx,112(%0)\n"
    "movq %%rdx,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl 0(%2),%%eax\n"
    "sbbl 4(%2),%%ebx\n"
    "movl %%eax,0(%0)\n"
    "movl %%ebx,4(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl 8(%2),%%ecx\n"
    "sbbl 12(%2),%%edx\n"
    "movl %%ecx,8(%0)\n"
    "movl %%edx,12(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl 16(%2),%%eax\n"
    "sbbl 20(%2),%%ebx\n"
    "movl %%eax,16(%0)\n"
    "movl %%ebx,20(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl 24(%2),%%ecx\n"
    "sbbl 28(%2),%%edx\n"
    "movl %%ecx,24(%0)\n"
    "movl %%edx,28(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "sbbl 32(%2),%%eax\n"
    "sbbl 36(%2),%%ebx\n"
    "movl %%eax,32(%0)\n"
    "movl %%ebx,36(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "sbbl 40(%2),%%ecx\n"
    "sbbl 44(%2),%%edx\n"
    "movl %%ecx,40(%0)\n"
    "movl %%edx,44(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "sbbl 48(%2),%%eax\n"
    "sbbl 52(%2),%%ebx\n"
    "movl %%eax,48(%0)\n"
    "movl %%ebx,52(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "sbbl 56(%2),%%ecx\n"
    "sbbl 60(%2),%%edx\n"
    "movl %%ecx,56(%0)\n"
    "movl %%edx,60(%0)\n"
    "movl 64(%1),%%eax\n"
    "movl 68(%1),%%ebx\n"
    "sbbl 64(%2),%%eax\n"
    "sbbl 68(%2),%%ebx\n"
    "movl %%eax,64(%0)\n"
    "movl %%ebx,68(%0)\n"
    "movl 72(%1),%%ecx\n"
    "movl 76(%1),%%edx\n"
    "sbbl 72(%2),%%ecx\n"
    "sbbl 76(%2),%%edx\n"
    "movl %%ecx,72(%0)\n"
    "movl %%edx,76(%0)\n"
    "movl 80(%1),%%eax\n"
    "movl 84(%1),%%ebx\n"
    "sbbl 80(%2),%%eax\n"
    "sbbl 84(%2),%%ebx\n"
    "movl %%eax,80(%0)\n"
    "movl %%ebx,84(%0)\n"
    "movl 88(%1),%%ecx\n"
    "movl 92(%1),%%edx\n"
    "sbbl 88(%2),%%ecx\n"
    "sbbl 92(%2),%%edx\n"
    "movl %%ecx,88(%0)\n"
    "movl %%edx,92(%0)\n"
    "movl 96(%1),%%eax\n"
    "movl 100(%1),%%ebx\n"
    "sbbl 96(%2),%%eax\n"
    "sbbl 100(%2),%%ebx\n"
    "movl %%eax,96(%0)\n"
    "movl %%ebx,100(%0)\n"
    "movl 104(%1),%%ecx\n"
    "movl 108(%1),%%edx\n"
    "sbbl 104(%2),%%ecx\n"
    "sbbl 108(%2),%%edx\n"
    "movl %%ecx,104(%0)\n"
    "movl %%edx,108(%0)\n"
    "movl 112(%1),%%eax\n"
    "movl 116(%1),%%ebx\n"
    "sbbl 112(%2),%%eax\n"
    "sbbl 116(%2),%%ebx\n"
    "movl %%eax,112(%0)\n"
    "movl %%ebx,116(%0)\n"
    "movl 120(%1),%%ecx\n"
    "movl 124(%1),%%edx\n"
    "sbbl 120(%2),%%ecx\n"
    "sbbl 124(%2),%%edx\n"
    "movl %%ecx,120(%0)\n"
    "movl %%edx,124(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024SetAddAdd( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd0, const bn1024 * const srcadd1 )
{
  bn1024SetAdd( dst, src, srcadd0 );
  bn1024Add( dst, srcadd1 );
  return;
}


void bn1024SetAddSub( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd, const bn1024 * const srcsub )
{
  bn1024SetAdd( dst, src, srcadd );
  bn1024Sub( dst, srcsub );
  return;
}


void bn1024SetAddAddSub( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd0, const bn1024 * const srcadd1, const bn1024 * const srcsub )
{
  bn1024SetAdd( dst, src, srcadd0 );
  bn1024Add( dst, srcadd1 );
  bn1024Sub( dst, srcsub );
  return;
}


void bn1024SetAddAddAddSub( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd0, const bn1024 * const srcadd1, const bn1024 * const srcadd2, const bn1024 * const srcsub )
{
  bn1024SetAdd( dst, src, srcadd0 );
  bn1024Add( dst, srcadd1 );
  bn1024Add( dst, srcadd2 );
  bn1024Sub( dst, srcsub );
  return;
}


void bn1024Mul32( bn1024 *dst, const bn1024 * const src, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq %2,%%rax\n"
    "mulq 0(%1)\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 8(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,8(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 16(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,16(%0)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 24(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,24(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 32(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,32(%0)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 40(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,40(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,48(%0)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,56(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 64(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,64(%0)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 72(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,72(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 80(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,80(%0)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 88(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,88(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 96(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,96(%0)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %2,%%rax\n"
    "mulq 104(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,104(%0)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %2,%%rax\n"
    "mulq 112(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,112(%0)\n"

    "movq %2,%%rax\n"
    "mulq 120(%1)\n"
    "addq %%rax,%%rbx\n"
    "movq %%rbx,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" ((uint64_t)value)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl %2,%%eax\n"
    "mull 0(%1)\n"
    "movl %%eax,0(%0)\n"
    "movl %%edx,%%ebx\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 4(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,4(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 8(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,8(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 12(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,12(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 16(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,16(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,20(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,24(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,28(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 32(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,32(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 36(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,36(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 40(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,40(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 44(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,44(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 48(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,48(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 52(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,52(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 56(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,56(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 60(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,60(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 64(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,64(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 68(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,68(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 72(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,72(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 76(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,76(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 80(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,80(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 84(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,84(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 88(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,88(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 92(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,92(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 96(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,96(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 100(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,100(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 104(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,104(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 108(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,108(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 112(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,112(%0)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %2,%%eax\n"
    "mull 116(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,116(%0)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %2,%%eax\n"
    "mull 120(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,120(%0)\n"

    "movl %2,%%eax\n"
    "mull 124(%1)\n"
    "addl %%eax,%%ebx\n"
    "movl %%ebx,124(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024Mul32Signed( bn1024 *dst, const bn1024 * const src, int32_t value )
{
  if( value < 0 )
  {
    bn1024Mul32( dst, src, -value );
    bn1024Neg( dst );
  }
  else
    bn1024Mul32( dst, src, value );
  return;
}


bnUnit bn1024Mul32Check( bn1024 *dst, const bn1024 * const src, uint32_t value )
{
  bnUnit overflow;
  overflow = 0;
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq %3,%%rax\n"
    "mulq 0(%2)\n"
    "movq %%rax,0(%1)\n"
    "movq %%rdx,%%rbx\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,8(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,16(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,24(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,32(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,40(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,48(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,56(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,64(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,72(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,80(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 88(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,88(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 96(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,96(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 104(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,104(%1)\n"

    "xorq %%rbx,%%rbx\n"
    "movq %3,%%rax\n"
    "mulq 112(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "movq %%rcx,112(%1)\n"

    "xorq %%rcx,%%rcx\n"
    "movq %3,%%rax\n"
    "mulq 120(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "movq %%rbx,120(%1)\n"
    "movq %%rcx,%0\n"
    : "=g" (overflow)
    : "r" (dst->unit), "r" (src->unit), "r" ((uint64_t)value)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl %3,%%eax\n"
    "mull 0(%2)\n"
    "movl %%eax,0(%1)\n"
    "movl %%edx,%%ebx\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,4(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,8(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,12(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,16(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,20(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,24(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,28(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,32(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,36(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,40(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,44(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,48(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,52(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,56(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,60(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 64(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,64(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 68(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,68(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 72(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,72(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 76(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,76(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 80(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,80(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 84(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,84(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 88(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,88(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 92(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,92(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 96(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,96(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 100(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,100(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 104(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,104(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 108(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,108(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 112(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,112(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 116(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,116(%1)\n"

    "xorl %%ebx,%%ebx\n"
    "movl %3,%%eax\n"
    "mull 120(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "movl %%ecx,120(%1)\n"

    "xorl %%ecx,%%ecx\n"
    "movl %3,%%eax\n"
    "mull 124(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "movl %%ebx,124(%1)\n"
    "movl %%ecx,%0\n"
    : "=g" (overflow)
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return overflow;
}


bnUnit bn1024Mul32SignedCheck( bn1024 *dst, const bn1024 * const src, int32_t value )
{
  bnUnit ret;
  if( value < 0 )
  {
    ret = bn1024Mul32Check( dst, src, -value );
    bn1024Neg( dst );
  }
  else
    ret = bn1024Mul32Check( dst, src, value );
  return ret;
}


void bn1024Mul( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
#if BN_UNIT_BITS == 64

  __asm__ __volatile__(
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq 0(%2)\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[1] */
    "movq 0(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /* a[1]*b[0] */
    "movq 8(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,8(%0)\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    /* a[0]*b[2] */
    "movq 0(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[0] */
    "movq 16(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[3] */
    "movq 0(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[2] */
    "movq 8(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[1] */
    "movq 16(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[0] */
    "movq 24(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,24(%0)\n"

    /* s4 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[4] */
    "movq 0(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[3] */
    "movq 8(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[2] */
    "movq 16(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[1] */
    "movq 24(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[0] */
    "movq 32(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,32(%0)\n"

    /* s5 */
    "xorq %%rbx,%%rbx\n"
    /* a[0]*b[5] */
    "movq 0(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[4] */
    "movq 8(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[3] */
    "movq 16(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[2] */
    "movq 24(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[4]*b[1] */
    "movq 32(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[0] */
    "movq 40(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,40(%0)\n"

    /* s6 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[6] */
    "movq 0(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[5] */
    "movq 8(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[4] */
    "movq 16(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[3] */
    "movq 24(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[2] */
    "movq 32(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[1] */
    "movq 40(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[0] */
    "movq 48(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,48(%0)\n"

    /* s7 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[7] */
    "movq 0(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[6] */
    "movq 8(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[5] */
    "movq 16(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[4] */
    "movq 24(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[3] */
    "movq 32(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[2] */
    "movq 40(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[1] */
    "movq 48(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[0] */
    "movq 56(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,56(%0)\n"

    /* s8 */
    "xorq %%rbx,%%rbx\n"
    /* a[0]*b[8] */
    "movq 0(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[7] */
    "movq 8(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[6] */
    "movq 16(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[5] */
    "movq 24(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[4]*b[4] */
    "movq 32(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[3] */
    "movq 40(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[6]*b[2] */
    "movq 48(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[7]*b[1] */
    "movq 56(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[8]*b[0] */
    "movq 64(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,64(%0)\n"

    /* s9 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[9] */
    "movq 0(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[8] */
    "movq 8(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[7] */
    "movq 16(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[6] */
    "movq 24(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[5] */
    "movq 32(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[4] */
    "movq 40(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[3] */
    "movq 48(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[2] */
    "movq 56(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[8]*b[1] */
    "movq 64(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[9]*b[0] */
    "movq 72(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,72(%0)\n"

    /* s10 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[10] */
    "movq 0(%1),%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[9] */
    "movq 8(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[8] */
    "movq 16(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[7] */
    "movq 24(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[6] */
    "movq 32(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[5] */
    "movq 40(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[4] */
    "movq 48(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[3] */
    "movq 56(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[8]*b[2] */
    "movq 64(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[9]*b[1] */
    "movq 72(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[10]*b[0] */
    "movq 80(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,80(%0)\n"

    /* s11 */
    "xorq %%rbx,%%rbx\n"
    /* a[0]*b[11] */
    "movq 0(%1),%%rax\n"
    "mulq 88(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[10] */
    "movq 8(%1),%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[9] */
    "movq 16(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[8] */
    "movq 24(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[4]*b[7] */
    "movq 32(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[6] */
    "movq 40(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[6]*b[5] */
    "movq 48(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[7]*b[4] */
    "movq 56(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[8]*b[3] */
    "movq 64(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[9]*b[2] */
    "movq 72(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[10]*b[1] */
    "movq 80(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[11]*b[0] */
    "movq 88(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,88(%0)\n"

    /* s12 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[12] */
    "movq 0(%1),%%rax\n"
    "mulq 96(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[11] */
    "movq 8(%1),%%rax\n"
    "mulq 88(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[10] */
    "movq 16(%1),%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[9] */
    "movq 24(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[8] */
    "movq 32(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[7] */
    "movq 40(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[6] */
    "movq 48(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[5] */
    "movq 56(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[8]*b[4] */
    "movq 64(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[9]*b[3] */
    "movq 72(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[10]*b[2] */
    "movq 80(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[11]*b[1] */
    "movq 88(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[12]*b[0] */
    "movq 96(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,96(%0)\n"

    /* s13 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[13] */
    "movq 0(%1),%%rax\n"
    "mulq 104(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[12] */
    "movq 8(%1),%%rax\n"
    "mulq 96(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[11] */
    "movq 16(%1),%%rax\n"
    "mulq 88(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[10] */
    "movq 24(%1),%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[9] */
    "movq 32(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[8] */
    "movq 40(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[7] */
    "movq 48(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[6] */
    "movq 56(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[8]*b[5] */
    "movq 64(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[9]*b[4] */
    "movq 72(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[10]*b[3] */
    "movq 80(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[11]*b[2] */
    "movq 88(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[12]*b[1] */
    "movq 96(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[13]*b[0] */
    "movq 104(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,104(%0)\n"

    /* s14 */
    /* a[0]*b[14] */
    "movq 0(%1),%%rax\n"
    "mulq 112(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[1]*b[13] */
    "movq 8(%1),%%rax\n"
    "mulq 104(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[2]*b[12] */
    "movq 16(%1),%%rax\n"
    "mulq 96(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[3]*b[11] */
    "movq 24(%1),%%rax\n"
    "mulq 88(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[4]*b[10] */
    "movq 32(%1),%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[5]*b[9] */
    "movq 40(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[6]*b[8] */
    "movq 48(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[7]*b[7] */
    "movq 56(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[8]*b[6] */
    "movq 64(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[9]*b[5] */
    "movq 72(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[10]*b[4] */
    "movq 80(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[11]*b[3] */
    "movq 88(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[12]*b[2] */
    "movq 96(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[13]*b[1] */
    "movq 104(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[14]*b[0] */
    "movq 112(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* add sum */
    "movq %%rcx,112(%0)\n"

    /* s15 */
    /* a[0]*b[15] */
    "movq 0(%1),%%rax\n"
    "mulq 120(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[1]*b[14] */
    "movq 8(%1),%%rax\n"
    "mulq 112(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[2]*b[13] */
    "movq 16(%1),%%rax\n"
    "mulq 104(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[3]*b[12] */
    "movq 24(%1),%%rax\n"
    "mulq 96(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[4]*b[11] */
    "movq 32(%1),%%rax\n"
    "mulq 88(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[5]*b[10] */
    "movq 40(%1),%%rax\n"
    "mulq 80(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[6]*b[9] */
    "movq 48(%1),%%rax\n"
    "mulq 72(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[7]*b[8] */
    "movq 56(%1),%%rax\n"
    "mulq 64(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[8]*b[7] */
    "movq 64(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[9]*b[6] */
    "movq 72(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[10]*b[5] */
    "movq 80(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[11]*b[4] */
    "movq 88(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[12]*b[3] */
    "movq 96(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[13]*b[2] */
    "movq 104(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[14]*b[1] */
    "movq 112(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[15]*b[0] */
    "movq 120(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    /* add sum */
    "movq %%r8,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );

#else

  int unitmask;
  unitmask = ( ((uint64_t)1) << BN_INT1024_UNIT_COUNT ) - 1;
  bn512MulExtended( &dst->unit[0], &src0->unit[0], &src1->unit[0], unitmask );
  bn512AddMulExtended( &dst->unit[BN_INT512_UNIT_COUNT], &src0->unit[0], &src1->unit[BN_INT512_UNIT_COUNT], unitmask >> BN_INT512_UNIT_COUNT_SHIFT, 0 );
  bn512AddMulExtended( &dst->unit[BN_INT512_UNIT_COUNT], &src0->unit[BN_INT512_UNIT_COUNT], &src1->unit[0], unitmask >> BN_INT512_UNIT_COUNT_SHIFT, 0 );

#endif

  return;
}


bnUnit bn1024MulCheck( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int highsum;
  bn1024Mul( dst, src0, src1 );
  highsum  = bn1024HighNonMatch( src0, 0 ) + bn1024HighNonMatch( src1, 0 );
  return ( highsum >= BN_INT1024_UNIT_COUNT );
}


bnUnit bn1024MulSignedCheck( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int highsum;
  bn1024Mul( dst, src0, src1 );
  highsum = bn1024HighNonMatch( src0, bn1024SignMask( src0 ) ) + bn1024HighNonMatch( src1, bn1024SignMask( src1 ) );
  return ( highsum >= BN_INT1024_UNIT_COUNT );
}


void __attribute__ ((noinline)) bn1024MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask )
{
  int unitindex, partmask;
  bnUnit carry;

  /* TODO: Assembly version for 64 bits only? */

  for( unitindex = 0 ; unitindex < 2*BN_INT1024_UNIT_COUNT ; unitindex++ )
    result[unitindex] = 0;
  carry = 0;
  partmask = unitmask & ( ( ((uint64_t)1) << (2*BN_INT512_UNIT_COUNT) ) - 1 );
  if( partmask )
    bn512MulExtended( &result[0], &src0[0], &src1[0], partmask );
  partmask = ( unitmask >> BN_INT512_UNIT_COUNT_SHIFT ) & ( ( ((uint64_t)1) << (2*BN_INT512_UNIT_COUNT) ) - 1 );
  if( partmask )
  {
    carry  = bn512AddMulExtended( &result[BN_INT512_UNIT_COUNT], &src0[0], &src1[BN_INT512_UNIT_COUNT], partmask, carry );
    carry += bn512AddMulExtended( &result[BN_INT512_UNIT_COUNT], &src0[BN_INT512_UNIT_COUNT], &src1[0], partmask, 0 );
  }
  partmask = ( unitmask >> (2*BN_INT512_UNIT_COUNT_SHIFT) ) & ( ( ((uint64_t)1) << (2*BN_INT512_UNIT_COUNT) ) - 1 );
  if( partmask )
    carry = bn512AddMulExtended( &result[2*BN_INT512_UNIT_COUNT], &src0[BN_INT512_UNIT_COUNT], &src1[BN_INT512_UNIT_COUNT], partmask, carry );
  return;
}


static inline void bn1024SubHigh( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,128(%0)\n"
    "sbbq %%rbx,136(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "sbbq %%rcx,144(%0)\n"
    "sbbq %%rdx,152(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "sbbq %%rax,160(%0)\n"
    "sbbq %%rbx,168(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "sbbq %%rcx,176(%0)\n"
    "sbbq %%rdx,184(%0)\n"
    "movq 64(%1),%%rax\n"
    "movq 72(%1),%%rbx\n"
    "sbbq %%rax,192(%0)\n"
    "sbbq %%rbx,200(%0)\n"
    "movq 80(%1),%%rcx\n"
    "movq 88(%1),%%rdx\n"
    "sbbq %%rcx,208(%0)\n"
    "sbbq %%rdx,216(%0)\n"
    "movq 96(%1),%%rax\n"
    "movq 104(%1),%%rbx\n"
    "sbbq %%rax,224(%0)\n"
    "sbbq %%rbx,232(%0)\n"
    "movq 112(%1),%%rcx\n"
    "movq 120(%1),%%rdx\n"
    "sbbq %%rcx,240(%0)\n"
    "sbbq %%rdx,248(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,128(%0)\n"
    "sbbl %%ebx,132(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,136(%0)\n"
    "sbbl %%edx,140(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,144(%0)\n"
    "sbbl %%ebx,148(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl %%ecx,152(%0)\n"
    "sbbl %%edx,156(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "sbbl %%eax,160(%0)\n"
    "sbbl %%ebx,164(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "sbbl %%ecx,168(%0)\n"
    "sbbl %%edx,172(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "sbbl %%eax,176(%0)\n"
    "sbbl %%ebx,180(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "sbbl %%ecx,184(%0)\n"
    "sbbl %%edx,188(%0)\n"
    "movl 64(%1),%%eax\n"
    "movl 68(%1),%%ebx\n"
    "sbbl %%eax,192(%0)\n"
    "sbbl %%ebx,196(%0)\n"
    "movl 72(%1),%%ecx\n"
    "movl 76(%1),%%edx\n"
    "sbbl %%ecx,200(%0)\n"
    "sbbl %%edx,204(%0)\n"
    "movl 80(%1),%%eax\n"
    "movl 84(%1),%%ebx\n"
    "sbbl %%eax,208(%0)\n"
    "sbbl %%ebx,212(%0)\n"
    "movl 88(%1),%%ecx\n"
    "movl 92(%1),%%edx\n"
    "sbbl %%ecx,216(%0)\n"
    "sbbl %%edx,220(%0)\n"
    "movl 96(%1),%%eax\n"
    "movl 100(%1),%%ebx\n"
    "sbbl %%eax,224(%0)\n"
    "sbbl %%ebx,228(%0)\n"
    "movl 104(%1),%%ecx\n"
    "movl 108(%1),%%edx\n"
    "sbbl %%ecx,232(%0)\n"
    "sbbl %%edx,236(%0)\n"
    "movl 112(%1),%%eax\n"
    "movl 116(%1),%%ebx\n"
    "sbbl %%eax,240(%0)\n"
    "sbbl %%ebx,244(%0)\n"
    "movl 120(%1),%%ecx\n"
    "movl 124(%1),%%edx\n"
    "sbbl %%ecx,248(%0)\n"
    "sbbl %%edx,252(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


static inline void bn1024SubHighTwice( bnUnit *result, const bnUnit * const src )
{
  bn1024SubHigh( result, src );
  bn1024SubHigh( result, src );
  return;
}


/* TODO */
/*
bnUnit __attribute__ ((noinline)) bn1024AddMulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask, bnUnit midcarry )
*/

void bn1024MulShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT1024_UNIT_COUNT*2];
  highunit = ( 1024 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );
  bn1024MulExtended( result, src0->unit, src1->unit, unitmask );
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn1024Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn1024Add32( dst, 1 );
  }
  else
  {
    bn1024CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn1024Add32( dst, 1 );
  }
  return;
}


void bn1024MulSignedShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT1024_UNIT_COUNT*2];

  highunit = ( 1024 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn1024MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn1024SubHigh( result, src1->unit );
  if( src1->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn1024SubHigh( result, src0->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn1024Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn1024Add32( dst, 1 );
  }
  else
  {
    bn1024CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn1024Add32( dst, 1 );
  }

  return;
}


bnUnit bn1024MulCheckShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT1024_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 1024 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn1024MulExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn1024HighNonMatch( src0, 0 ) + bn1024HighNonMatch( src1, 0 );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT1024_UNIT_COUNT );
    bn1024Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn1024Add32( dst, 1 );
  }
  else
  {
    overflow = 0;
    if( ( highsum >= offset+BN_INT1024_UNIT_COUNT+1 ) || ( result[offset+BN_INT1024_UNIT_COUNT] >> shift ) )
      overflow = 1;
    bn1024CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn1024Add32( dst, 1 );
  }

  return overflow;
}


bnUnit bn1024MulSignedCheckShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT1024_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 1024 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn1024MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn1024SubHigh( result, src1->unit );
  if( src1->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn1024SubHigh( result, src0->unit );

  highsum = bn1024HighNonMatch( src0, bn1024SignMask( src0 ) ) + bn1024HighNonMatch( src1, bn1024SignMask( src1 ) );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT1024_UNIT_COUNT );
    bn1024Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn1024Add32( dst, 1 );
  }
  else
  {
    if( highsum >= offset+BN_INT1024_UNIT_COUNT+1 )
      overflow = 1;
    else
    {
      overflow = result[offset+BN_INT1024_UNIT_COUNT];
      if( src0->unit[BN_INT1024_UNIT_COUNT-1] ^ src1->unit[BN_INT1024_UNIT_COUNT-1] )
        overflow = ~overflow;
      overflow >>= shift;
    }
    bn1024CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn1024Add32( dst, 1 );
  }

  return overflow;
}


void bn1024SquareShr( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  bn1024MulSignedShr( dst, src, src, shiftbits );
  return;
}



////



void bn1024Div32( bn1024 *dst, uint32_t divisor, uint32_t *rem )
{
  bnShift offset, divisormsb;
  bn1024 div, quotient, dsthalf, qpart, dpart;
  bn1024Set32( &div, divisor );
  bn1024Zero( &quotient );
  divisormsb = bn1024GetIndexMSB( &div );
  for( ; ; )
  {
    if( bn1024CmpLt( dst, &div ) )
      break;
    bn1024Set( &dpart, &div );
    bn1024Set32( &qpart, 1 );
    bn1024SetShr1( &dsthalf, dst );
    offset = bn1024GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn1024Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn1024CmpLe( &dpart, &dsthalf ) )
    {
      bn1024Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn1024Shl( &qpart, &qpart, offset );
    bn1024Sub( dst, &dpart );
    bn1024Add( &quotient, &qpart );
  }
  if( rem )
    *rem = dst->unit[0];
  if( bn1024CmpEq( dst, &div ) )
  {
    if( rem )
      *rem = 0;
    bn1024Add32( &quotient, 1 );
  }
  bn1024Set( dst, &quotient );
  return;
}


void bn1024Div32Signed( bn1024 *dst, int32_t divisor, int32_t *rem )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn1024Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn1024Div32( dst, (uint32_t)divisor, (uint32_t *)rem );
  if( sign )
  {
    bn1024Neg( dst );
    if( rem )
      *rem = -*rem;
  }
  return;
}


void bn1024Div32Round( bn1024 *dst, uint32_t divisor )
{
  uint32_t rem;
  bn1024Div32( dst, divisor, &rem );
  if( ( rem << 1 ) >= divisor )
    bn1024Add32( dst, 1 );
  return;
}


void bn1024Div32RoundSigned( bn1024 *dst, int32_t divisor )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn1024Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn1024Div32Round( dst, (uint32_t)divisor );
  if( sign )
    bn1024Neg( dst );
  return;
}


void bn1024Div( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem )
{
  bnShift offset, remzero, divisormsb;
  bn1024 quotient, dsthalf, qpart, dpart;
  bn1024Zero( &quotient );
  divisormsb = bn1024GetIndexMSB( divisor );
  for( ; ; )
  {
    if( bn1024CmpLt( dst, divisor ) )
      break;
    bn1024Set( &dpart, divisor );
    bn1024Set32( &qpart, 1 );
    bn1024SetShr1( &dsthalf, dst );
    offset = bn1024GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn1024Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn1024CmpLe( &dpart, &dsthalf ) )
    {
      bn1024Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn1024Shl( &qpart, &qpart, offset );
    bn1024Sub( dst, &dpart );
    bn1024Add( &quotient, &qpart );
  }
  remzero = 0;
  if( bn1024CmpEq( dst, divisor ) )
  {
    bn1024Add32( &quotient, 1 );
    remzero = 1;
  }
  if( rem )
  {
    if( remzero )
      bn1024Zero( rem );
    else
      bn1024Set( rem, dst );
  }
  bn1024Set( dst, &quotient );
  return;
}


void bn1024DivSigned( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem )
{
  int sign;
  bn1024 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn1024Neg( dst );
  }
  if( divisor->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn1024SetNeg( &divisorabs, divisor );
    bn1024Div( dst, &divisorabs, rem );
  }
  else
    bn1024Div( dst, divisor, rem );
  if( sign )
  {
    bn1024Neg( dst );
    if( rem )
      bn1024Neg( rem );
  }
  return;
}


void bn1024DivRound( bn1024 *dst, const bn1024 * const divisor )
{
  bn1024 rem;
  bn1024Div( dst, divisor, &rem );
  bn1024Shl1( &rem );
  if( bn1024CmpGe( &rem, divisor ) )
    bn1024Add32( dst, 1 );
  return;
}


void bn1024DivRoundSigned( bn1024 *dst, const bn1024 * const divisor )
{
  int sign;
  bn1024 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn1024Neg( dst );
  }
  if( divisor->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn1024SetNeg( &divisorabs, divisor );
    bn1024DivRound( dst, &divisorabs );
  }
  else
    bn1024DivRound( dst, divisor );
  if( sign )
    bn1024Neg( dst );
  return;
}


void bn1024DivShl( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem, bnShift shift )
{
  bnShift fracshift;
  bn1024 basedst, fracpart, qpart;
  if( !( rem ) )
    rem = &fracpart;
  bn1024Set( &basedst, dst );
  bn1024Div( &basedst, divisor, rem );
  bn1024Shl( dst, &basedst, shift );
  for( fracshift = shift-1 ; fracshift >= 0 ; fracshift-- )
  {
    if( bn1024CmpZero( rem ) )
      break;
    bn1024Shl1( rem );
    if( bn1024CmpGe( rem, divisor ) )
    {
      bn1024Sub( rem, divisor );
      bn1024Set32Shl( &qpart, 1, fracshift );
      bn1024Or( dst, &qpart );
    }
  }
  return;
}


void bn1024DivSignedShl( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem, bnShift shift )
{
  int sign;
  bn1024 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn1024Neg( dst );
  }
  if( divisor->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn1024SetNeg( &divisorabs, divisor );
    bn1024DivShl( dst, &divisorabs, rem, shift );
  }
  else
    bn1024DivShl( dst, divisor, rem, shift );
  if( sign )
  {
    bn1024Neg( dst );
    if( rem )
      bn1024Neg( rem );
  }
  return;
}


void bn1024DivRoundShl( bn1024 *dst, const bn1024 * const divisor, bnShift shift )
{
  bn1024 rem;
  bn1024DivShl( dst, divisor, &rem, shift );
  bn1024Shl1( &rem );
  if( bn1024CmpGe( &rem, divisor ) )
    bn1024Add32( dst, 1 );
  return;
}


void bn1024DivRoundSignedShl( bn1024 *dst, const bn1024 * const divisor, bnShift shift )
{
  int sign;
  bn1024 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn1024Neg( dst );
  }
  if( divisor->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn1024SetNeg( &divisorabs, divisor );
    bn1024DivRoundShl( dst, &divisorabs, shift );
  }
  else
    bn1024DivRoundShl( dst, divisor, shift );
  if( sign )
    bn1024Neg( dst );
  return;
}



////



void bn1024Or( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] |= src->unit[unitindex];
  return;
}


void bn1024SetOr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] | src1->unit[unitindex];
  return;
}


void bn1024Nor( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( dst->unit[unitindex] | src->unit[unitindex] );
  return;
}


void bn1024SetNor( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] | src1->unit[unitindex] );
  return;
}


void bn1024And( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] &= src->unit[unitindex];
  return;
}


void bn1024SetAnd( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] & src1->unit[unitindex];
  return;
}


void bn1024Nand( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] & dst->unit[unitindex] );
  return;
}


void bn1024SetNand( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] & src1->unit[unitindex] );
  return;
}


void bn1024Xor( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] ^= src->unit[unitindex];
  return;
}


void bn1024SetXor( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] ^ src1->unit[unitindex];
  return;
}


void bn1024Nxor( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] ^ dst->unit[unitindex] );
  return;
}


void bn1024SetNxor( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] ^ src1->unit[unitindex] );
  return;
}


void bn1024Not( bn1024 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  return;
}


void bn1024SetNot( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  return;
}


void bn1024Neg( bn1024 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  bn1024Add32( dst, 1 );
  return;
}


void bn1024SetNeg( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  bn1024Add32( dst, 1 );
  return;
}


void bn1024Shl( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[-offset];
  if( !( shift ) )
  {
    limit = offset;
    for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= limit ; unitindex-- )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex >= 0 ; unitindex-- )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = offset;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = BN_INT1024_UNIT_COUNT-1;
    if( offset < BN_INT1024_UNIT_COUNT )
    {
      for( ; unitindex > limit ; unitindex-- )
        dst->unit[unitindex] = ( srcunit[unitindex] << shift ) | ( srcunit[unitindex-1] >> shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] << shift );
      unitindex--;
    }
    for( ; unitindex >= 0 ; unitindex-- )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn1024Shr( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT1024_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = BN_INT1024_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT1024_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift );
      unitindex++;
    }
    for( ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn1024Sal( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  bn1024Shl( dst, src, shiftbits );
  return;
}


void bn1024Sar( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  if( !( src->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn1024Shr( dst, src, shiftbits );
    return;
  }
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT1024_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = -1;
  }
  else
  {
    limit = BN_INT1024_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT1024_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( (bnUnit)-1 << shiftinv );;
      unitindex++;
    }
    for( ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn1024ShrRound( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn1024ExtractBit( src, shiftbits-1 );
  bn1024Shr( dst, src, shiftbits );
  if( bit )
    bn1024Add32( dst, 1 );
  return;
}


void bn1024SarRound( bn1024 *dst, const bn1024 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn1024ExtractBit( src, shiftbits-1 );
  bn1024Sar( dst, src, shiftbits );
  if( bit )
    bn1024Add32( dst, 1 );
  return;
}


void bn1024Shl1( bn1024 *dst )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%0),%%rax\n"
    "movq 8(%0),%%rbx\n"
    "addq %%rax,0(%0)\n"
    "adcq %%rbx,8(%0)\n"
    "movq 16(%0),%%rcx\n"
    "movq 24(%0),%%rdx\n"
    "adcq %%rcx,16(%0)\n"
    "adcq %%rdx,24(%0)\n"
    "movq 32(%0),%%rax\n"
    "movq 40(%0),%%rbx\n"
    "adcq %%rax,32(%0)\n"
    "adcq %%rbx,40(%0)\n"
    "movq 48(%0),%%rcx\n"
    "movq 56(%0),%%rdx\n"
    "adcq %%rcx,48(%0)\n"
    "adcq %%rdx,56(%0)\n"
    "movq 64(%0),%%rax\n"
    "movq 72(%0),%%rbx\n"
    "adcq %%rax,64(%0)\n"
    "adcq %%rbx,72(%0)\n"
    "movq 80(%0),%%rcx\n"
    "movq 88(%0),%%rdx\n"
    "adcq %%rcx,80(%0)\n"
    "adcq %%rdx,88(%0)\n"
    "movq 96(%0),%%rax\n"
    "movq 104(%0),%%rbx\n"
    "adcq %%rax,96(%0)\n"
    "adcq %%rbx,104(%0)\n"
    "movq 112(%0),%%rcx\n"
    "movq 120(%0),%%rdx\n"
    "adcq %%rcx,112(%0)\n"
    "adcq %%rdx,120(%0)\n"
    :
    : "r" (dst->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%0),%%eax\n"
    "movl 4(%0),%%ebx\n"
    "addl %%eax,0(%0)\n"
    "adcl %%ebx,4(%0)\n"
    "movl 8(%0),%%ecx\n"
    "movl 12(%0),%%edx\n"
    "adcl %%ecx,8(%0)\n"
    "adcl %%edx,12(%0)\n"
    "movl 16(%0),%%eax\n"
    "movl 20(%0),%%ebx\n"
    "adcl %%eax,16(%0)\n"
    "adcl %%ebx,20(%0)\n"
    "movl 24(%0),%%ecx\n"
    "movl 28(%0),%%edx\n"
    "adcl %%ecx,24(%0)\n"
    "adcl %%edx,28(%0)\n"
    "movl 32(%0),%%eax\n"
    "movl 36(%0),%%ebx\n"
    "adcl %%eax,32(%0)\n"
    "adcl %%ebx,36(%0)\n"
    "movl 40(%0),%%ecx\n"
    "movl 44(%0),%%edx\n"
    "adcl %%ecx,40(%0)\n"
    "adcl %%edx,44(%0)\n"
    "movl 48(%0),%%eax\n"
    "movl 52(%0),%%ebx\n"
    "adcl %%eax,48(%0)\n"
    "adcl %%ebx,52(%0)\n"
    "movl 56(%0),%%ecx\n"
    "movl 60(%0),%%edx\n"
    "adcl %%ecx,56(%0)\n"
    "adcl %%edx,60(%0)\n"
    "movl 64(%0),%%eax\n"
    "movl 68(%0),%%ebx\n"
    "adcl %%eax,64(%0)\n"
    "adcl %%ebx,68(%0)\n"
    "movl 72(%0),%%ecx\n"
    "movl 76(%0),%%edx\n"
    "adcl %%ecx,72(%0)\n"
    "adcl %%edx,76(%0)\n"
    "movl 80(%0),%%eax\n"
    "movl 84(%0),%%ebx\n"
    "adcl %%eax,80(%0)\n"
    "adcl %%ebx,84(%0)\n"
    "movl 88(%0),%%ecx\n"
    "movl 92(%0),%%edx\n"
    "adcl %%ecx,88(%0)\n"
    "adcl %%edx,92(%0)\n"
    "movl 96(%0),%%eax\n"
    "movl 100(%0),%%ebx\n"
    "adcl %%eax,96(%0)\n"
    "adcl %%ebx,100(%0)\n"
    "movl 104(%0),%%ecx\n"
    "movl 108(%0),%%edx\n"
    "adcl %%ecx,104(%0)\n"
    "adcl %%edx,108(%0)\n"
    "movl 112(%0),%%eax\n"
    "movl 116(%0),%%ebx\n"
    "adcl %%eax,112(%0)\n"
    "adcl %%ebx,116(%0)\n"
    "movl 120(%0),%%ecx\n"
    "movl 124(%0),%%edx\n"
    "adcl %%ecx,120(%0)\n"
    "adcl %%edx,124(%0)\n"
    :
    : "r" (dst->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024SetShl1( bn1024 *dst, const bn1024 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq %%rax,%%rax\n"
    "adcq %%rbx,%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "adcq %%rcx,%%rcx\n"
    "adcq %%rdx,%%rdx\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "adcq %%rax,%%rax\n"
    "adcq %%rbx,%%rbx\n"
    "movq %%rax,32(%0)\n"
    "movq %%rbx,40(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "adcq %%rcx,%%rcx\n"
    "adcq %%rdx,%%rdx\n"
    "movq %%rcx,48(%0)\n"
    "movq %%rdx,56(%0)\n"
    "movq 64(%1),%%rax\n"
    "movq 72(%1),%%rbx\n"
    "adcq %%rax,%%rax\n"
    "adcq %%rbx,%%rbx\n"
    "movq %%rax,64(%0)\n"
    "movq %%rbx,72(%0)\n"
    "movq 80(%1),%%rcx\n"
    "movq 88(%1),%%rdx\n"
    "adcq %%rcx,%%rcx\n"
    "adcq %%rdx,%%rdx\n"
    "movq %%rcx,80(%0)\n"
    "movq %%rdx,88(%0)\n"
    "movq 96(%1),%%rax\n"
    "movq 104(%1),%%rbx\n"
    "adcq %%rax,%%rax\n"
    "adcq %%rbx,%%rbx\n"
    "movq %%rax,96(%0)\n"
    "movq %%rbx,104(%0)\n"
    "movq 112(%1),%%rcx\n"
    "movq 120(%1),%%rdx\n"
    "adcq %%rcx,%%rcx\n"
    "adcq %%rdx,%%rdx\n"
    "movq %%rcx,112(%0)\n"
    "movq %%rdx,120(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "addl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,0(%0)\n"
    "movl %%ebx,4(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,8(%0)\n"
    "movl %%edx,12(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,16(%0)\n"
    "movl %%ebx,20(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,24(%0)\n"
    "movl %%edx,28(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,32(%0)\n"
    "movl %%ebx,36(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,40(%0)\n"
    "movl %%edx,44(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,48(%0)\n"
    "movl %%ebx,52(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,56(%0)\n"
    "movl %%edx,60(%0)\n"
    "movl 64(%1),%%eax\n"
    "movl 68(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,64(%0)\n"
    "movl %%ebx,68(%0)\n"
    "movl 72(%1),%%ecx\n"
    "movl 76(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,72(%0)\n"
    "movl %%edx,76(%0)\n"
    "movl 80(%1),%%eax\n"
    "movl 84(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,80(%0)\n"
    "movl %%ebx,84(%0)\n"
    "movl 88(%1),%%ecx\n"
    "movl 92(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,88(%0)\n"
    "movl %%edx,92(%0)\n"
    "movl 96(%1),%%eax\n"
    "movl 100(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,96(%0)\n"
    "movl %%ebx,100(%0)\n"
    "movl 104(%1),%%ecx\n"
    "movl 108(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,104(%0)\n"
    "movl %%edx,108(%0)\n"
    "movl 112(%1),%%eax\n"
    "movl 116(%1),%%ebx\n"
    "adcl %%eax,%%eax\n"
    "adcl %%ebx,%%ebx\n"
    "movl %%eax,112(%0)\n"
    "movl %%ebx,116(%0)\n"
    "movl 120(%1),%%ecx\n"
    "movl 124(%1),%%edx\n"
    "adcl %%ecx,%%ecx\n"
    "adcl %%edx,%%edx\n"
    "movl %%ecx,120(%0)\n"
    "movl %%edx,124(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn1024Shr1( bn1024 *dst )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 ) | ( dst->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( dst->unit[4] >> 1 ) | ( dst->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( dst->unit[5] >> 1 ) | ( dst->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( dst->unit[6] >> 1 ) | ( dst->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( dst->unit[7] >> 1 ) | ( dst->unit[8] << (BN_UNIT_BITS-1) );;
  dst->unit[8] = ( dst->unit[8] >> 1 ) | ( dst->unit[9] << (BN_UNIT_BITS-1) );
  dst->unit[9] = ( dst->unit[9] >> 1 ) | ( dst->unit[10] << (BN_UNIT_BITS-1) );
  dst->unit[10] = ( dst->unit[10] >> 1 ) | ( dst->unit[11] << (BN_UNIT_BITS-1) );
  dst->unit[11] = ( dst->unit[11] >> 1 ) | ( dst->unit[12] << (BN_UNIT_BITS-1) );
  dst->unit[12] = ( dst->unit[12] >> 1 ) | ( dst->unit[13] << (BN_UNIT_BITS-1) );
  dst->unit[13] = ( dst->unit[13] >> 1 ) | ( dst->unit[14] << (BN_UNIT_BITS-1) );
  dst->unit[14] = ( dst->unit[14] >> 1 ) | ( dst->unit[15] << (BN_UNIT_BITS-1) );
  dst->unit[15] = ( dst->unit[15] >> 1 );
#else
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 ) | ( dst->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( dst->unit[4] >> 1 ) | ( dst->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( dst->unit[5] >> 1 ) | ( dst->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( dst->unit[6] >> 1 ) | ( dst->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( dst->unit[7] >> 1 ) | ( dst->unit[8] << (BN_UNIT_BITS-1) );
  dst->unit[8] = ( dst->unit[8] >> 1 ) | ( dst->unit[9] << (BN_UNIT_BITS-1) );
  dst->unit[9] = ( dst->unit[9] >> 1 ) | ( dst->unit[10] << (BN_UNIT_BITS-1) );
  dst->unit[10] = ( dst->unit[10] >> 1 ) | ( dst->unit[11] << (BN_UNIT_BITS-1) );
  dst->unit[11] = ( dst->unit[11] >> 1 ) | ( dst->unit[12] << (BN_UNIT_BITS-1) );
  dst->unit[12] = ( dst->unit[12] >> 1 ) | ( dst->unit[13] << (BN_UNIT_BITS-1) );
  dst->unit[13] = ( dst->unit[13] >> 1 ) | ( dst->unit[14] << (BN_UNIT_BITS-1) );
  dst->unit[14] = ( dst->unit[14] >> 1 ) | ( dst->unit[15] << (BN_UNIT_BITS-1) );
  dst->unit[15] = ( dst->unit[15] >> 1 ) | ( dst->unit[16] << (BN_UNIT_BITS-1) );
  dst->unit[16] = ( dst->unit[16] >> 1 ) | ( dst->unit[17] << (BN_UNIT_BITS-1) );
  dst->unit[17] = ( dst->unit[17] >> 1 ) | ( dst->unit[18] << (BN_UNIT_BITS-1) );
  dst->unit[18] = ( dst->unit[18] >> 1 ) | ( dst->unit[19] << (BN_UNIT_BITS-1) );
  dst->unit[19] = ( dst->unit[19] >> 1 ) | ( dst->unit[20] << (BN_UNIT_BITS-1) );
  dst->unit[20] = ( dst->unit[20] >> 1 ) | ( dst->unit[21] << (BN_UNIT_BITS-1) );
  dst->unit[21] = ( dst->unit[21] >> 1 ) | ( dst->unit[22] << (BN_UNIT_BITS-1) );
  dst->unit[22] = ( dst->unit[22] >> 1 ) | ( dst->unit[23] << (BN_UNIT_BITS-1) );
  dst->unit[23] = ( dst->unit[23] >> 1 ) | ( dst->unit[24] << (BN_UNIT_BITS-1) );
  dst->unit[24] = ( dst->unit[24] >> 1 ) | ( dst->unit[25] << (BN_UNIT_BITS-1) );
  dst->unit[25] = ( dst->unit[25] >> 1 ) | ( dst->unit[26] << (BN_UNIT_BITS-1) );
  dst->unit[26] = ( dst->unit[26] >> 1 ) | ( dst->unit[27] << (BN_UNIT_BITS-1) );
  dst->unit[27] = ( dst->unit[27] >> 1 ) | ( dst->unit[28] << (BN_UNIT_BITS-1) );
  dst->unit[28] = ( dst->unit[28] >> 1 ) | ( dst->unit[29] << (BN_UNIT_BITS-1) );
  dst->unit[29] = ( dst->unit[29] >> 1 ) | ( dst->unit[30] << (BN_UNIT_BITS-1) );
  dst->unit[30] = ( dst->unit[30] >> 1 ) | ( dst->unit[31] << (BN_UNIT_BITS-1) );
  dst->unit[31] = ( dst->unit[31] >> 1 );
#endif
  return;
}


void bn1024SetShr1( bn1024 *dst, const bn1024 * const src )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 ) | ( src->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( src->unit[4] >> 1 ) | ( src->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( src->unit[5] >> 1 ) | ( src->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( src->unit[6] >> 1 ) | ( src->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( src->unit[7] >> 1 ) | ( src->unit[8] << (BN_UNIT_BITS-1) );
  dst->unit[8] = ( src->unit[8] >> 1 ) | ( src->unit[9] << (BN_UNIT_BITS-1) );
  dst->unit[9] = ( src->unit[9] >> 1 ) | ( src->unit[10] << (BN_UNIT_BITS-1) );
  dst->unit[10] = ( src->unit[10] >> 1 ) | ( src->unit[11] << (BN_UNIT_BITS-1) );
  dst->unit[11] = ( src->unit[11] >> 1 ) | ( src->unit[12] << (BN_UNIT_BITS-1) );
  dst->unit[12] = ( src->unit[12] >> 1 ) | ( src->unit[13] << (BN_UNIT_BITS-1) );
  dst->unit[13] = ( src->unit[13] >> 1 ) | ( src->unit[14] << (BN_UNIT_BITS-1) );
  dst->unit[14] = ( src->unit[14] >> 1 ) | ( src->unit[15] << (BN_UNIT_BITS-1) );
  dst->unit[15] = ( src->unit[15] >> 1 );
#else
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 ) | ( src->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( src->unit[4] >> 1 ) | ( src->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( src->unit[5] >> 1 ) | ( src->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( src->unit[6] >> 1 ) | ( src->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( src->unit[7] >> 1 ) | ( src->unit[8] << (BN_UNIT_BITS-1) );
  dst->unit[8] = ( src->unit[8] >> 1 ) | ( src->unit[9] << (BN_UNIT_BITS-1) );
  dst->unit[9] = ( src->unit[9] >> 1 ) | ( src->unit[10] << (BN_UNIT_BITS-1) );
  dst->unit[10] = ( src->unit[10] >> 1 ) | ( src->unit[11] << (BN_UNIT_BITS-1) );
  dst->unit[11] = ( src->unit[11] >> 1 ) | ( src->unit[12] << (BN_UNIT_BITS-1) );
  dst->unit[12] = ( src->unit[12] >> 1 ) | ( src->unit[13] << (BN_UNIT_BITS-1) );
  dst->unit[13] = ( src->unit[13] >> 1 ) | ( src->unit[14] << (BN_UNIT_BITS-1) );
  dst->unit[14] = ( src->unit[14] >> 1 ) | ( src->unit[15] << (BN_UNIT_BITS-1) );
  dst->unit[15] = ( src->unit[15] >> 1 ) | ( src->unit[16] << (BN_UNIT_BITS-1) );
  dst->unit[16] = ( src->unit[16] >> 1 ) | ( src->unit[17] << (BN_UNIT_BITS-1) );
  dst->unit[17] = ( src->unit[17] >> 1 ) | ( src->unit[18] << (BN_UNIT_BITS-1) );
  dst->unit[18] = ( src->unit[18] >> 1 ) | ( src->unit[19] << (BN_UNIT_BITS-1) );
  dst->unit[19] = ( src->unit[19] >> 1 ) | ( src->unit[20] << (BN_UNIT_BITS-1) );
  dst->unit[20] = ( src->unit[20] >> 1 ) | ( src->unit[21] << (BN_UNIT_BITS-1) );
  dst->unit[21] = ( src->unit[21] >> 1 ) | ( src->unit[22] << (BN_UNIT_BITS-1) );
  dst->unit[22] = ( src->unit[22] >> 1 ) | ( src->unit[23] << (BN_UNIT_BITS-1) );
  dst->unit[23] = ( src->unit[23] >> 1 ) | ( src->unit[24] << (BN_UNIT_BITS-1) );
  dst->unit[24] = ( src->unit[24] >> 1 ) | ( src->unit[25] << (BN_UNIT_BITS-1) );
  dst->unit[25] = ( src->unit[25] >> 1 ) | ( src->unit[26] << (BN_UNIT_BITS-1) );
  dst->unit[26] = ( src->unit[26] >> 1 ) | ( src->unit[27] << (BN_UNIT_BITS-1) );
  dst->unit[27] = ( src->unit[27] >> 1 ) | ( src->unit[28] << (BN_UNIT_BITS-1) );
  dst->unit[28] = ( src->unit[28] >> 1 ) | ( src->unit[29] << (BN_UNIT_BITS-1) );
  dst->unit[29] = ( src->unit[29] >> 1 ) | ( src->unit[30] << (BN_UNIT_BITS-1) );
  dst->unit[30] = ( src->unit[30] >> 1 ) | ( src->unit[31] << (BN_UNIT_BITS-1) );
  dst->unit[31] = ( src->unit[31] >> 1 );
#endif
  return;
}



////



int bn1024CmpZero( const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 0;
  }
  return 1;
}

int bn1024CmpNotZero( const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn1024CmpEq( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn1024CmpNeq( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn1024CmpGt( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn1024CmpGe( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn1024CmpLt( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn1024CmpLe( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn1024CmpSignedGt( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT1024_UNIT_COUNT-1] ^ src->unit[BN_INT1024_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT1024_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn1024CmpSignedGe( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT1024_UNIT_COUNT-1] ^ src->unit[BN_INT1024_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT1024_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn1024CmpSignedLt( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT1024_UNIT_COUNT-1] ^ src->unit[BN_INT1024_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT1024_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn1024CmpSignedLe( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT1024_UNIT_COUNT-1] ^ src->unit[BN_INT1024_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT1024_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn1024CmpPositive( const bn1024 * const src )
{
  return ( src->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) == 0;
}


int bn1024CmpNegative( const bn1024 * const src )
{
  return ( src->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) != 0;
}


int bn1024CmpEqOrZero( bn1024 *dst, const bn1024 * const src )
{
  int unitindex;
  bnUnit accum, diffaccum;
  accum = 0;
  diffaccum = 0;
  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
  {
    diffaccum |= dst->unit[unitindex] - src->unit[unitindex];
    accum |= dst->unit[unitindex];
  }
  return ( ( accum & diffaccum ) == 0 );
}


int bn1024CmpPart( bn1024 *dst, const bn1024 * const src, uint32_t bits )
{
  int unitindex, unitcount, partbits;
  bnUnit unitmask;
  unitcount = ( bits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  partbits = bits & (BN_UNIT_BITS-1);
  unitindex = BN_INT1024_UNIT_COUNT - unitcount;
  if( partbits )
  {
    unitmask = (bnUnit)-1 << (BN_UNIT_BITS-partbits);
    if( ( dst->unit[unitindex] & unitmask ) != ( src->unit[unitindex] & unitmask ) )
      return 0;
    unitindex++;
  }
  for( ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}



////



int bn1024ExtractBit( const bn1024 * const src, int bitoffset )
{
  int unitindex, shift;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  value = 0;
  if( (unsigned)unitindex < BN_INT1024_UNIT_COUNT )
    value = ( src->unit[unitindex] >> shift ) & 0x1;
  return value;
}


uint32_t bn1024Extract32( const bn1024 * const src, int bitoffset )
{
  int unitindex, shift, shiftinv;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT1024_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 32 ) && ( (unsigned)unitindex+1 < BN_INT1024_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
}


uint64_t bn1024Extract64( const bn1024 * const src, int bitoffset )
{
#if BN_UNIT_BITS == 64
  int unitindex, shift, shiftinv;
  uint64_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT1024_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 64 ) && ( (unsigned)unitindex+1 < BN_INT1024_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
#else
  return (uint64_t)bn1024Extract32( src, bitoffset ) | ( (uint64_t)bn1024Extract32( src, bitoffset+32 ) << 32 );
#endif
}


int bn1024GetIndexMSB( const bn1024 * const src )
{
  int unitindex;
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( !( src->unit[unitindex] ) )
      continue;
#if BN_UNIT_BITS == 64
    return ccLog2Int64( src->unit[unitindex] ) + ( unitindex << BN_UNIT_BIT_SHIFT );
#else
    return ccLog2Int32( src->unit[unitindex] ) + ( unitindex << BN_UNIT_BIT_SHIFT );
#endif
  }
  return -1;
}


int bn1024GetIndexMSZ( const bn1024 * const src )
{
  int unitindex;
  bnUnit unit;
  for( unitindex = BN_INT1024_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    unit = ~src->unit[unitindex];
    if( !( unit ) )
      continue;
#if BN_UNIT_BITS == 64
    return ccLog2Int64( unit ) + ( unitindex << BN_UNIT_BIT_SHIFT );
#else
    return ccLog2Int32( unit ) + ( unitindex << BN_UNIT_BIT_SHIFT );
#endif
  }
  return -1;
}




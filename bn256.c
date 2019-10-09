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
#define BN_XP_SUPPORT_256 1
#define BN_XP_SUPPORT_512 0
#define BN_XP_SUPPORT_1024 0
#include "bn.h"


/*
Big number fixed point two-complement arithmetics, 256 bits

On 32 bits, this file *MUST* be compiled with -fomit-frame-pointer to free the %ebp register
In fact, don't even think about compiling 32 bits. This is much faster on 64 bits.
*/

#define BN256_PRINT_REPORT



#if BN_UNIT_BITS == 64
 #define PRINT(p,x) printf( p "%lx %lx %lx %lx\n", (long)(x)->unit[3], (long)(x)->unit[2], (long)(x)->unit[1], (long)(x)->unit[0] )
#else
 #define PRINT(p,x) printf( p "%x %x %x %x %x %x %x %x\n", (x)->unit[7], (x)->unit[6], (x)->unit[5], (x)->unit[4], (x)->unit[3], (x)->unit[2], (x)->unit[1], (x)->unit[0] )
#endif



static inline __attribute__((always_inline)) void bn256Copy( bn256 *dst, bnUnit *src )
{
  dst->unit[0] = src[0];
  dst->unit[1] = src[1];
  dst->unit[2] = src[2];
  dst->unit[3] = src[3];
#if BN_UNIT_BITS == 32
  dst->unit[4] = src[4];
  dst->unit[5] = src[5];
  dst->unit[6] = src[6];
  dst->unit[7] = src[7];
#endif
  return;
}

static inline __attribute__((always_inline)) void bn256CopyShift( bn256 *dst, bnUnit *src, bnShift shift )
{
  bnShift shiftinv;
  shiftinv = BN_UNIT_BITS - shift;
  dst->unit[0] = ( src[0] >> shift ) | ( src[1] << shiftinv );
  dst->unit[1] = ( src[1] >> shift ) | ( src[2] << shiftinv );
  dst->unit[2] = ( src[2] >> shift ) | ( src[3] << shiftinv );
  dst->unit[3] = ( src[3] >> shift ) | ( src[4] << shiftinv );
#if BN_UNIT_BITS == 32
  dst->unit[4] = ( src[4] >> shift ) | ( src[5] << shiftinv );
  dst->unit[5] = ( src[5] >> shift ) | ( src[6] << shiftinv );
  dst->unit[6] = ( src[6] >> shift ) | ( src[7] << shiftinv );
  dst->unit[7] = ( src[7] >> shift ) | ( src[8] << shiftinv );
#endif
  return;
}

static inline __attribute__((always_inline)) int bn256HighNonMatch( const bn256 * const src, bnUnit value )
{
  int high;
  high = 0;
#if BN_UNIT_BITS == 64
  if( src->unit[3] != value )
    high = 3;
  else if( src->unit[2] != value )
    high = 2;
  else if( src->unit[1] != value )
    high = 1;
#else
  if( src->unit[7] != value )
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


static inline bnUnit bn256SignMask( const bn256 * const src )
{
  return ( (bnUnitSigned)src->unit[BN_INT256_UNIT_COUNT-1] ) >> (BN_UNIT_BITS-1);
}



////



void bn256Zero( bn256 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn256Set( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src->unit[unitindex];
  return;
}


void bn256Set32( bn256 *dst, uint32_t value )
{
  int unitindex;
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn256Set32Signed( bn256 *dst, int32_t value )
{
  int unitindex;
  bnUnit u;
  u = ( (bnUnitSigned)value ) >> (BN_UNIT_BITS-1);
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = u;
  return;
}


void bn256Set32Shl( bn256 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
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
  if( (unsigned)unitindex < BN_INT256_UNIT_COUNT )
    dst->unit[unitindex] = vl;
  if( (unsigned)++unitindex < BN_INT256_UNIT_COUNT )
    dst->unit[unitindex] = vh;
  return;
}


void bn256Set32SignedShl( bn256 *dst, int32_t value, bnShift shift )
{
  if( value < 0 )
  {
    bn256Set32Shl( dst, -value, shift );
    bn256Neg( dst );
  }
  else
    bn256Set32Shl( dst, value, shift );
  return;
}


#define BN_IEEE754DBL_MANTISSA_BITS (52)
#define BN_IEEE754DBL_EXPONENT_BITS (11)

void bn256SetDouble( bn256 *dst, double value, bnShift leftshiftbits )
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

  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
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
    if( (unsigned)unitindex < BN_INT256_UNIT_COUNT )
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
    for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = ~dst->unit[unitindex];
    bn256Add32( dst, 1 );
  }

  return;
}


double bn256GetDouble( bn256 *dst, bnShift rightshiftbits )
{
  int exponent, sign, msb;
  uint64_t mantissa;
  union
  {
    double d;
    uint64_t u;
  } dblunion;
  bn256 value;

  sign = 0;
  if( ( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn256SetNeg( &value, dst );
    sign = 1;
  }
  else
    bn256Set( &value, dst );

  msb = bn256GetIndexMSB( &value );
  if( msb < 0 )
    return ( sign ? -0.0 : 0.0 );
  mantissa = bn256Extract64( &value, msb-64 );
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


void bn256Add32( bn256 *dst, uint32_t value )
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn256Sub32( bn256 *dst, uint32_t value )
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn256Add32s( bn256 *dst, int32_t value )
{
  if( value < 0 )
    bn256Sub32( dst, -value );
  else
    bn256Add32( dst, value );
  return;
}


void bn256Sub32s( bn256 *dst, int32_t value )
{
  if( value < 0 )
    bn256Add32( dst, -value );
  else
    bn256Sub32( dst, value );
  return;
}


void bn256Add32Shl( bn256 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  vl = value;
  vh = 0;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  shift &= BN_UNIT_BITS-1;
  if( shift )
  {
    vl = vl << shift;
    vh = ( (bnUnit)value ) >> ( BN_UNIT_BITS - shift );
  }
#if BN_UNIT_BITS == 64
  switch( unitindex )
  {
    case 0:
      __asm__ __volatile__(
        "addq %1,0(%0)\n"
        "adcq %2,8(%0)\n"
        "jnc 1f\n"
        "addq $1,16(%0)\n"
        "jnc 1f\n"
        "addq $1,24(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "addq %1,8(%0)\n"
        "adcq %2,16(%0)\n"
        "jnc 1f\n"
        "addq $1,24(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "addq %1,16(%0)\n"
        "adcq %2,24(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 3:
      __asm__ __volatile__(
        "addq %1,24(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    default:
      break;
  }
#else
  switch( unitindex )
  {
    case 0:
      __asm__ __volatile__(
        "addl %1,0(%0)\n"
        "adcl %2,4(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "addl %1,4(%0)\n"
        "adcl %2,8(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "addl %1,8(%0)\n"
        "adcl %2,12(%0)\n"
        "jnc 1f\n"
        "addl $1,16(%0)\n"
        "jnc 1f\n"
        "addl $1,20(%0)\n"
        "jnc 1f\n"
        "addl $1,24(%0)\n"
        "jnc 1f\n"
        "addl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 3:
      __asm__ __volatile__(
        "addl %1,12(%0)\n"
        "adcl %2,16(%0)\n"
        "jnc 1f\n"
        "addl $1,20(%0)\n"
        "jnc 1f\n"
        "addl $1,24(%0)\n"
        "jnc 1f\n"
        "addl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 4:
      __asm__ __volatile__(
        "addl %1,16(%0)\n"
        "adcl %2,20(%0)\n"
        "jnc 1f\n"
        "addl $1,24(%0)\n"
        "jnc 1f\n"
        "addl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 5:
      __asm__ __volatile__(
        "addl %1,20(%0)\n"
        "adcl %2,24(%0)\n"
        "jnc 1f\n"
        "addl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 6:
      __asm__ __volatile__(
        "addl %1,24(%0)\n"
        "adcl %2,28(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 7:
      __asm__ __volatile__(
        "addl %1,28(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    default:
      break;
  }
#endif
  return;
}


void bn256Sub32Shl( bn256 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  vl = value;
  vh = 0;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  shift &= BN_UNIT_BITS-1;
  if( shift )
  {
    vl = vl << shift;
    vh = ( (bnUnit)value ) >> ( BN_UNIT_BITS - shift );
  }
#if BN_UNIT_BITS == 64
  switch( unitindex )
  {
    case 0:
      __asm__ __volatile__(
        "subq %1,0(%0)\n"
        "sbbq %2,8(%0)\n"
        "jnc 1f\n"
        "subq $1,16(%0)\n"
        "jnc 1f\n"
        "subq $1,24(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "subq %1,8(%0)\n"
        "sbbq %2,16(%0)\n"
        "jnc 1f\n"
        "subq $1,24(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "subq %1,16(%0)\n"
        "sbbq %2,24(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 3:
      __asm__ __volatile__(
        "subq %1,24(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    default:
      break;
  }
#else
  switch( unitindex )
  {
    case 0:
      __asm__ __volatile__(
        "subl %1,0(%0)\n"
        "sbbl %2,4(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "subl %1,4(%0)\n"
        "sbbl %2,8(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "subl %1,8(%0)\n"
        "sbbl %2,12(%0)\n"
        "jnc 1f\n"
        "subl $1,16(%0)\n"
        "jnc 1f\n"
        "subl $1,20(%0)\n"
        "jnc 1f\n"
        "subl $1,24(%0)\n"
        "jnc 1f\n"
        "subl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 3:
      __asm__ __volatile__(
        "subl %1,12(%0)\n"
        "adcl %2,16(%0)\n"
        "jnc 1f\n"
        "subl $1,20(%0)\n"
        "jnc 1f\n"
        "subl $1,24(%0)\n"
        "jnc 1f\n"
        "subl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 4:
      __asm__ __volatile__(
        "subl %1,16(%0)\n"
        "sbbl %2,20(%0)\n"
        "jnc 1f\n"
        "subl $1,24(%0)\n"
        "jnc 1f\n"
        "subl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 5:
      __asm__ __volatile__(
        "subl %1,20(%0)\n"
        "sbbl %2,24(%0)\n"
        "jnc 1f\n"
        "subl $1,28(%0)\n"
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 6:
      __asm__ __volatile__(
        "subl %1,24(%0)\n"
        "sbbl %2,28(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 7:
      __asm__ __volatile__(
        "subl %1,28(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    default:
      break;
  }
#endif
  return;
}


void bn256SetBit( bn256 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] |= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


void bn256ClearBit( bn256 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] &= ~( (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) ) );
  return;
}


void bn256FlipBit( bn256 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] ^= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


////


void bn256Add( bn256 *dst, const bn256 * const src )
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256SetAdd( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256Sub( bn256 *dst, const bn256 * const src )
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256SetSub( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256SetAddAdd( bn256 *dst, const bn256 * const src, const bn256 * const srcadd0, const bn256 * const srcadd1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "adcq 24(%2),%%rdx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "adcq 16(%3),%%rcx\n"
    "adcq 24(%3),%%rdx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  bn256SetAdd( dst, src, srcadd0 );
  bn256Add( dst, srcadd1 );
#endif
  return;
}


void bn256SetAddSub( bn256 *dst, const bn256 * const src, const bn256 * const srcadd, const bn256 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "adcq 24(%2),%%rdx\n"
    "subq 0(%3),%%rax\n"
    "sbbq 8(%3),%%rbx\n"
    "sbbq 16(%3),%%rcx\n"
    "sbbq 24(%3),%%rdx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  bn256SetAdd( dst, src, srcadd );
  bn256Sub( dst, srcsub );
#endif
  return;
}


void bn256SetAddAddSub( bn256 *dst, const bn256 * const src, const bn256 * const srcadd0, const bn256 * const srcadd1, const bn256 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "adcq 24(%2),%%rdx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "adcq 16(%3),%%rcx\n"
    "adcq 24(%3),%%rdx\n"
    "subq 0(%4),%%rax\n"
    "sbbq 8(%4),%%rbx\n"
    "sbbq 16(%4),%%rcx\n"
    "sbbq 24(%4),%%rdx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  bn256SetAdd( dst, src, srcadd0 );
  bn256Add( dst, srcadd1 );
  bn256Sub( dst, srcsub );
#endif
  return;
}


void bn256SetAddAddAddSub( bn256 *dst, const bn256 * const src, const bn256 * const srcadd0, const bn256 * const srcadd1, const bn256 * const srcadd2, const bn256 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "adcq 24(%2),%%rdx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "adcq 16(%3),%%rcx\n"
    "adcq 24(%3),%%rdx\n"
    "addq 0(%4),%%rax\n"
    "adcq 8(%4),%%rbx\n"
    "adcq 16(%4),%%rcx\n"
    "adcq 24(%4),%%rdx\n"
    "subq 0(%5),%%rax\n"
    "sbbq 8(%5),%%rbx\n"
    "sbbq 16(%5),%%rcx\n"
    "sbbq 24(%5),%%rdx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    "movq %%rdx,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit), "r" (srcadd2->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  bn256SetAdd( dst, src, srcadd0 );
  bn256Add( dst, srcadd1 );
  bn256Add( dst, srcadd2 );
  bn256Sub( dst, srcsub );
#endif
  return;
}


void bn256Mul32( bn256 *dst, const bn256 * const src, uint32_t value )
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

    "movq %2,%%rax\n"
    "mulq 24(%1)\n"
    "addq %%rax,%%rbx\n"
    "movq %%rbx,24(%0)\n"
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

    "movl %2,%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ebx\n"
    "movl %%ebx,28(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256Mul32Signed( bn256 *dst, const bn256 * const src, int32_t value )
{
  if( value < 0 )
  {
    bn256Mul32( dst, src, -value );
    bn256Neg( dst );
  }
  else
    bn256Mul32( dst, src, value );
  return;
}


bnUnit bn256Mul32Check( bn256 *dst, const bn256 * const src, uint32_t value )
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
    "movl %%ecx,%0\n"
    : "=g" (overflow)
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return overflow;
}


bnUnit bn256Mul32SignedCheck( bn256 *dst, const bn256 * const src, int32_t value )
{
  bnUnit ret;
  if( value < 0 )
  {
    ret = bn256Mul32Check( dst, src, -value );
    bn256Neg( dst );
  }
  else
    ret = bn256Mul32Check( dst, src, value );
  return ret;
}


void bn256Mul( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
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
    /* a[0]*b[2] */
    "movq 0(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* a[2]*b[0] */
    "movq 16(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"

    /* s3 */
    /* a[0]*b[3] */
    "movq 0(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[1]*b[2] */
    "movq 8(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[2]*b[1] */
    "movq 16(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    /* a[3]*b[0] */
    "movq 24(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    /* add sum */
    "movq %%r8,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );
#else
  __asm__ __volatile__(
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull 0(%2)\n"
    "movl %0,%%edi\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%edx,%%ebx\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    /* a[0]*b[1] */
    "movl 0(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* a[1]*b[0] */
    "movl 4(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,4(%%eax)\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    /* a[0]*b[2] */
    "movl 0(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[1] */
    "movl 4(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[0] */
    "movl 8(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,8(%%eax)\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    /* a[0]*b[3] */
    "movl 0(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[2] */
    "movl 4(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[1] */
    "movl 8(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[0] */
    "movl 12(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,12(%%eax)\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    /* a[0]*b[4] */
    "movl 0(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[3] */
    "movl 4(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[2] */
    "movl 8(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[1] */
    "movl 12(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[0] */
    "movl 16(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,16(%%eax)\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    /* a[0]*b[5] */
    "movl 0(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[4] */
    "movl 4(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[3] */
    "movl 8(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[2] */
    "movl 12(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[1] */
    "movl 16(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[0] */
    "movl 20(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,20(%%eax)\n"

    /* s6 */
    /* a[0]*b[6] */
    "movl 0(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* a[1]*b[5] */
    "movl 4(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* a[2]*b[4] */
    "movl 8(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* a[3]*b[3] */
    "movl 12(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* a[4]*b[2] */
    "movl 16(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* a[5]*b[1] */
    "movl 20(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* a[6]*b[0] */
    "movl 24(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,24(%%eax)\n"

    /* s7 */
    /* a[0]*b[7] */
    "movl 0(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[1]*b[6] */
    "movl 4(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[2]*b[5] */
    "movl 8(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[3]*b[4] */
    "movl 12(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[4]*b[3] */
    "movl 16(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[5]*b[2] */
    "movl 20(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[6]*b[1] */
    "movl 24(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    /* a[7]*b[0] */
    "movl 28(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,28(%%eax)\n"
    :
    : "m" (dst), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return;
}


bnUnit bn256MulCheck( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int highsum;
  bn256Mul( dst, src0, src1 );
  highsum  = bn256HighNonMatch( src0, 0 ) + bn256HighNonMatch( src1, 0 );
  return ( highsum >= BN_INT256_UNIT_COUNT );
}


bnUnit bn256MulSignedCheck( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int highsum;
  bn256Mul( dst, src0, src1 );
  highsum = bn256HighNonMatch( src0, bn256SignMask( src0 ) ) + bn256HighNonMatch( src1, bn256SignMask( src1 ) );
  return ( highsum >= BN_INT256_UNIT_COUNT );
}


void __attribute__ ((noinline)) bn256MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testq $1,%3\n"
    "jz .mulext256s0\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq 0(%2)\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"
    ".mulext256s0:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testq $2,%3\n"
    "jz .mulext256s1\n"
    /* a[0]*b[1] */
    "movq 0(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[0] */
    "movq 8(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,8(%0)\n"
    ".mulext256s1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testq $4,%3\n"
    "jz .mulext256s2\n"
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
    ".mulext256s2:\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    "testq $8,%3\n"
    "jz .mulext256s3\n"
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
    ".mulext256s3:\n"

    /* s4 */
    "xorq %%r8,%%r8\n"
    "testq $0x10,%3\n"
    "jz .mulext256s4\n"
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
    /* add sum */
    "movq %%rbx,32(%0)\n"
    ".mulext256s4:\n"

    /* s5 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x20,%3\n"
    "jz .mulext256s5\n"
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
    /* add sum */
    "movq %%rcx,40(%0)\n"
    ".mulext256s5:\n"

    /* s6 */
    "testq $0xC0,%3\n"
    "jz .mulext256s6\n"
    /* a[3]*b[3] */
    "movq 24(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* add sum */
    "movq %%r8,48(%0)\n"
    "movq %%rbx,56(%0)\n"
    ".mulext256s6:\n"
    :
    : "r" (result), "r" (src0), "r" (src1), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );
#else
  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%3\n"
    "jz .mulext256s0\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull 0(%2)\n"
    "movl %0,%%edi\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%edx,%%ebx\n"
    ".mulext256s0:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    "testl $2,%3\n"
    "jz .mulext256s1\n"
    /* a[0]*b[1] */
    "movl 0(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[0] */
    "movl 4(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,4(%%eax)\n"
    ".mulext256s1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%3\n"
    "jz .mulext256s2\n"
    /* a[0]*b[2] */
    "movl 0(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[1] */
    "movl 4(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[0] */
    "movl 8(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,8(%%eax)\n"
    ".mulext256s2:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%3\n"
    "jz .mulext256s3\n"
    /* a[0]*b[3] */
    "movl 0(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[2] */
    "movl 4(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[1] */
    "movl 8(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[0] */
    "movl 12(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,12(%%eax)\n"
    ".mulext256s3:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%3\n"
    "jz .mulext256s4\n"
    /* a[0]*b[4] */
    "movl 0(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[3] */
    "movl 4(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[2] */
    "movl 8(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[1] */
    "movl 12(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[0] */
    "movl 16(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,16(%%eax)\n"
    ".mulext256s4:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%3\n"
    "jz .mulext256s5\n"
    /* a[0]*b[5] */
    "movl 0(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[4] */
    "movl 4(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[3] */
    "movl 8(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[2] */
    "movl 12(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[1] */
    "movl 16(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[0] */
    "movl 20(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,20(%%eax)\n"
    ".mulext256s5:\n"

    /* s6 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x40,%3\n"
    "jz .mulext256s6\n"
    /* a[0]*b[6] */
    "movl 0(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[5] */
    "movl 4(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[4] */
    "movl 8(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[3] */
    "movl 12(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[2] */
    "movl 16(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[1] */
    "movl 20(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[0] */
    "movl 24(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,24(%%eax)\n"
    ".mulext256s6:\n"

    /* s7 */
    "xorl %%edi,%%edi\n"
    "testl $0x80,%3\n"
    "jz .mulext256s7\n"
    /* a[0]*b[7] */
    "movl 0(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[6] */
    "movl 4(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[5] */
    "movl 8(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[4] */
    "movl 12(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[3] */
    "movl 16(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[2] */
    "movl 20(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[1] */
    "movl 24(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[0] */
    "movl 28(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,28(%%eax)\n"
    ".mulext256s7:\n"

    /* s8 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x100,%3\n"
    "jz .mulext256s8\n"
    /* a[1]*b[7] */
    "movl 4(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[6] */
    "movl 8(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[5] */
    "movl 12(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[4] */
    "movl 16(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[3] */
    "movl 20(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[2] */
    "movl 24(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[1] */
    "movl 28(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,32(%%eax)\n"
    ".mulext256s8:\n"

    /* s9 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x200,%3\n"
    "jz .mulext256s9\n"
    /* a[2]*b[7] */
    "movl 8(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[6] */
    "movl 12(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[5] */
    "movl 16(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[4] */
    "movl 20(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[3] */
    "movl 24(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[2] */
    "movl 28(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,36(%%eax)\n"
    ".mulext256s9:\n"

    /* s10 */
    "xorl %%edi,%%edi\n"
    "testl $0x400,%3\n"
    "jz .mulext256s10\n"
    /* a[3]*b[7] */
    "movl 12(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[6] */
    "movl 16(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[5] */
    "movl 20(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[4] */
    "movl 24(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[3] */
    "movl 28(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,40(%%eax)\n"
    ".mulext256s10:\n"

    /* s11 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x800,%3\n"
    "jz .mulext256s11\n"
    /* a[4]*b[7] */
    "movl 16(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[6] */
    "movl 20(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[5] */
    "movl 24(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[4] */
    "movl 28(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,44(%%eax)\n"
    ".mulext256s11:\n"

    /* s12 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x1000,%3\n"
    "jz .mulext256s12\n"
    /* a[5]*b[7] */
    "movl 20(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[6] */
    "movl 24(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[5] */
    "movl 28(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,48(%%eax)\n"
    ".mulext256s12:\n"

    /* s13 */
    "xorl %%edi,%%edi\n"
    "testl $0x2000,%3\n"
    "jz .mulext256s13\n"
    /* a[6]*b[7] */
    "movl 24(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[6] */
    "movl 28(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,52(%%eax)\n"
    ".mulext256s13:\n"

    /* s14 */
    "testl $0xC000,%3\n"
    "jz .mulext256s14\n"
    /* a[7]*b[7] */
    "movl 28(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,56(%%eax)\n"
    "movl %%edi,60(%%eax)\n"
    ".mulext256s14:\n"
    :
    : "m" (result), "r" (src0), "r" (src1), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return;
}


static inline void bn256SubHigh( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,32(%0)\n"
    "sbbq %%rbx,40(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "sbbq %%rcx,48(%0)\n"
    "sbbq %%rdx,56(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,32(%0)\n"
    "sbbl %%ebx,36(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,40(%0)\n"
    "sbbl %%edx,44(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,48(%0)\n"
    "sbbl %%ebx,52(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl %%ecx,56(%0)\n"
    "sbbl %%edx,60(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


static inline void bn256SubHighTwice( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,32(%0)\n"
    "sbbq %%rbx,40(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "sbbq %%rcx,48(%0)\n"
    "sbbq %%rdx,56(%0)\n"
    "subq %%rax,32(%0)\n"
    "sbbq %%rbx,40(%0)\n"
    "sbbq %%rcx,48(%0)\n"
    "sbbq %%rdx,56(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,32(%0)\n"
    "sbbl %%ebx,36(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,40(%0)\n"
    "sbbl %%edx,44(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,48(%0)\n"
    "sbbl %%ebx,52(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl %%ecx,56(%0)\n"
    "sbbl %%edx,60(%0)\n"
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,32(%0)\n"
    "sbbl %%ebx,36(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,40(%0)\n"
    "sbbl %%edx,44(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,48(%0)\n"
    "sbbl %%ebx,52(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl %%ecx,56(%0)\n"
    "sbbl %%edx,60(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


bnUnit __attribute__ ((noinline)) bn256AddMulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask, bnUnit midcarry )
{
  bnUnit overflow;
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    /* Carry */
    "movq $0,%0\n"

    "xorq %%rbx,%%rbx\n"
    "xorq %%r8,%%r8\n"
    /* s0 */
    "testq $1,%4\n"
    "jz .addmulext256s0\n"
    /* a[0]*b[0] */
    "movq 0(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,0(%1)\n"
    "adcq %%rdx,%%rbx\n"
    ".addmulext256s0:\n"

    "xorq %%rcx,%%rcx\n"
    /* s1 */
    "testq $2,%4\n"
    "jz .addmulext256s1\n"
    /* a[0]*b[1] */
    "movq 0(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    /* a[1]*b[0] */
    "movq 8(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "addq %%rbx,8(%1)\n"
    "adcq $0,%%r8\n"
    "adcq $0,%%rcx\n"
    ".addmulext256s1:\n"

    /* ## Mid carry ## */
    "movq %5,%%rbx\n"
    /* s2 */
    "testq $4,%4\n"
    "jz .addmulext256s2\n"
    /* a[0]*b[2] */
    "movq 0(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[1] */
    "movq 8(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[0] */
    "movq 16(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "addq %%r8,16(%1)\n"
    "adcq $0,%%rcx\n"
    "adcq $0,%%rbx\n"
    ".addmulext256s2:\n"

    "xorq %%r8,%%r8\n"
    /* s3 */
    "testq $8,%4\n"
    "jz .addmulext256s3\n"
    /* a[0]*b[3] */
    "movq 0(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[2] */
    "movq 8(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[1] */
    "movq 16(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[0] */
    "movq 24(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "addq %%rcx,24(%1)\n"
    "adcq $0,%%rbx\n"
    "adcq $0,%%r8\n"
    ".addmulext256s3:\n"

    "xorq %%rcx,%%rcx\n"
    /* s4 */
    "testq $0x10,%4\n"
    "jz .addmulext256s4\n"
    /* a[1]*b[3] */
    "movq 8(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[2] */
    "movq 16(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[1] */
    "movq 24(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "addq %%rbx,32(%1)\n"
    "adcq $0,%%r8\n"
    "adcq $0,%%rcx\n"
    ".addmulext256s4:\n"

    "xorq %%rbx,%%rbx\n"
    /* s5 */
    "testq $0x20,%4\n"
    "jz .addmulext256s5\n"
    /* a[2]*b[3] */
    "movq 16(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[2] */
    "movq 24(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "addq %%r8,40(%1)\n"
    "adcq $0,%%rcx\n"
    "adcq $0,%%rbx\n"
    ".addmulext256s5:\n"

    /* s6 */
    "testq $0xC0,%4\n"
    "jz .addmulext256s6\n"
    /* a[3]*b[3] */
    "movq 24(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%0\n"
    "addq %%rcx,48(%1)\n"
    "adcq %%rbx,56(%1)\n"
    "adcq $0,%0\n"
    ".addmulext256s6:\n"

    : "=m" (overflow)
    : "r" (result), "r" (src0), "r" (src1), "r" (unitmask), "g" (midcarry)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );
#else
  __asm__ __volatile__(
    /* Carry */
    "movl $0,%0\n"

    "xorl %%ebx,%%ebx\n"
    "xorl %%edi,%%edi\n"
    /* s0 */
    "testl $1,%4\n"
    "jz .addmulext256s0\n"
    /* a[0]*b[0] */
    "movl 0(%2),%%eax\n"
    "mull 0(%3)\n"
    "movl %1,%%ecx\n"
    "addl %%eax,0(%%ecx)\n"
    "adcl %%edx,%%ebx\n"
    ".addmulext256s0:\n"

    "xorl %%ecx,%%ecx\n"
    /* s1 */
    "testl $2,%4\n"
    "jz .addmulext256s1\n"
    /* a[0]*b[1] */
    "movl 0(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    /* a[1]*b[0] */
    "movl 4(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ebx,4(%%eax)\n"
    "adcl $0,%%edi\n"
    "adcl $0,%%ecx\n"
    ".addmulext256s1:\n"

    /* ## Mid carry ## */
    "movl %5,%%ebx\n"
    /* s2 */
    "testl $4,%4\n"
    "jz .addmulext256s2\n"
    /* a[0]*b[2] */
    "movl 0(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[1] */
    "movl 4(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[0] */
    "movl 8(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,8(%%eax)\n"
    "adcl $0,%%ecx\n"
    "adcl $0,%%ebx\n"
    ".addmulext256s2:\n"

    "xorl %%edi,%%edi\n"
    /* s3 */
    "testl $8,%4\n"
    "jz .addmulext256s3\n"
    /* a[0]*b[3] */
    "movl 0(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[2] */
    "movl 4(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[1] */
    "movl 8(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[0] */
    "movl 12(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ecx,12(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext256s3:\n"

    "xorl %%ecx,%%ecx\n"
    /* s4 */
    "testl $0x10,%4\n"
    "jz .addmulext256s4\n"
    /* a[0]*b[4] */
    "movl 0(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[3] */
    "movl 4(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[2] */
    "movl 8(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[1] */
    "movl 12(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[0] */
    "movl 16(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ebx,16(%%eax)\n"
    "adcl $0,%%edi\n"
    "adcl $0,%%ecx\n"
    ".addmulext256s4:\n"

    "xorl %%ebx,%%ebx\n"
    /* s5 */
    "testl $0x20,%4\n"
    "jz .addmulext256s5\n"
    /* a[0]*b[5] */
    "movl 0(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[4] */
    "movl 4(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[3] */
    "movl 8(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[2] */
    "movl 12(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[1] */
    "movl 16(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[0] */
    "movl 20(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,20(%%eax)\n"
    "adcl $0,%%ecx\n"
    "adcl $0,%%ebx\n"
    ".addmulext256s5:\n"

    "xorl %%edi,%%edi\n"
    /* s6 */
    "testl $0x40,%4\n"
    "jz .addmulext256s6\n"
    /* a[0]*b[6] */
    "movl 0(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[5] */
    "movl 4(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[4] */
    "movl 8(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[3] */
    "movl 12(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[2] */
    "movl 16(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[1] */
    "movl 20(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[0] */
    "movl 24(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ecx,24(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext256s6:\n"

    "xorl %%ecx,%%ecx\n"
    /* s7 */
    "testl $0x80,%4\n"
    "jz .addmulext256s7\n"
    /* a[0]*b[7] */
    "movl 0(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[6] */
    "movl 4(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[5] */
    "movl 8(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[4] */
    "movl 12(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[3] */
    "movl 16(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[2] */
    "movl 20(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[1] */
    "movl 24(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[0] */
    "movl 28(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ebx,28(%%eax)\n"
    "adcl $0,%%edi\n"
    "adcl $0,%%ecx\n"
    ".addmulext256s7:\n"

    "xorl %%ebx,%%ebx\n"
    /* s8 */
    "testl $0x100,%4\n"
    "jz .addmulext256s8\n"
    /* a[1]*b[7] */
    "movl 4(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[6] */
    "movl 8(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[5] */
    "movl 12(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[4] */
    "movl 16(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[3] */
    "movl 20(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[2] */
    "movl 24(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[1] */
    "movl 28(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,32(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext256s8:\n"

    "xorl %%edi,%%edi\n"
    /* s9 */
    "testl $0x200,%4\n"
    "jz .addmulext256s9\n"
    /* a[2]*b[7] */
    "movl 8(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[6] */
    "movl 12(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[5] */
    "movl 16(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[4] */
    "movl 20(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[3] */
    "movl 24(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[2] */
    "movl 28(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,36(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext256s9:\n"

    "xorl %%edx,%%edx\n"
    /* s10 */
    "testl $0x400,%4\n"
    "jz .addmulext256s10\n"
    /* a[3]*b[7] */
    "movl 12(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[4]*b[6] */
    "movl 16(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[5]*b[5] */
    "movl 20(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[6]*b[4] */
    "movl 24(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[7]*b[3] */
    "movl 28(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ebx,40(%%eax)\n"
    "adcl $0,%%edi\n"
    "adcl $0,%%edx\n"
    ".addmulext256s10:\n"

    "xorl %%ebx,%%ebx\n"
    /* s11 */
    "testl $0x800,%4\n"
    "jz .addmulext256s11\n"
    /* a[4]*b[7] */
    "movl 16(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[6] */
    "movl 20(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[5] */
    "movl 24(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[4] */
    "movl 28(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,44(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext256s11:\n"

    "xorl %%edi,%%edi\n"
    /* s12 */
    "testl $0x1000,%4\n"
    "jz .addmulext256s12\n"
    /* a[5]*b[7] */
    "movl 20(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[6] */
    "movl 24(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[5] */
    "movl 28(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,48(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext256s12:\n"

    "xorl %%edx,%%edx\n"
    /* s13 */
    "testl $0x2000,%4\n"
    "jz .addmulext256s13\n"
    /* a[6]*b[7] */
    "movl 24(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[7]*b[6] */
    "movl 28(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,52(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext256s13:\n"

    /* s14 */
    "testl $0xC000,%4\n"
    "jz .addmulext256s14\n"
    /* a[7]*b[7] */
    "movl 28(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%0\n"
    "movl %1,%%eax\n"
    "addl %%ebx,56(%%eax)\n"
    "adcl %%edi,60(%%eax)\n"
    "adcl $0,%0\n"
    ".addmulext256s14:\n"

    : "=m" (overflow)
    : "m" (result), "r" (src0), "r" (src1), "m" (unitmask), "m" (midcarry)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return overflow;
}


void bn256MulShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT256_UNIT_COUNT*2];

  highunit = ( 256 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn256MulExtended( result, src0->unit, src1->unit, unitmask );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn256Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn256Add32( dst, 1 );
  }
  else
  {
    bn256CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn256Add32( dst, 1 );
  }

  return;
}


void bn256MulSignedShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT256_UNIT_COUNT*2];

  highunit = ( 256 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn256MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn256SubHigh( result, src1->unit );
  if( src1->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn256SubHigh( result, src0->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn256Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn256Add32( dst, 1 );
  }
  else
  {
    bn256CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn256Add32( dst, 1 );
  }

  return;
}


bnUnit bn256MulCheckShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT256_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 256 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn256MulExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn256HighNonMatch( src0, 0 ) + bn256HighNonMatch( src1, 0 );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT256_UNIT_COUNT );
    bn256Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn256Add32( dst, 1 );
  }
  else
  {
    overflow = 0;
    if( ( highsum >= offset+BN_INT256_UNIT_COUNT+1 ) || ( result[offset+BN_INT256_UNIT_COUNT] >> shift ) )
      overflow = 1;
    bn256CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn256Add32( dst, 1 );
  }

  return overflow;
}


bnUnit bn256MulSignedCheckShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT256_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 256 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn256MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn256SubHigh( result, src1->unit );
  if( src1->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn256SubHigh( result, src0->unit );

  highsum = bn256HighNonMatch( src0, bn256SignMask( src0 ) ) + bn256HighNonMatch( src1, bn256SignMask( src1 ) );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT256_UNIT_COUNT );
    bn256Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn256Add32( dst, 1 );
  }
  else
  {
    if( highsum >= offset+BN_INT256_UNIT_COUNT+1 )
      overflow = 1;
    else
    {
      overflow = result[offset+BN_INT256_UNIT_COUNT];
      if( src0->unit[BN_INT256_UNIT_COUNT-1] ^ src1->unit[BN_INT256_UNIT_COUNT-1] )
        overflow = ~overflow;
      overflow >>= shift;
    }
    bn256CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn256Add32( dst, 1 );
  }

  return overflow;
}


void bn256SquareShr( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT256_UNIT_COUNT*2];

  highunit = ( 256 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

#if BN_UNIT_BITS == 64

  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testq $1,%2\n"
    "jz .sqshr256s0\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq %%rax\n"
    "movq %%rax,0(%0)\n"
    "addq %%rdx,%%rbx\n"
    ".sqshr256s0:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testq $2,%2\n"
    "jz .sqshr256s1\n"
    /* a[0]*b[1] */
    "movq 0(%1),%%rax\n"
    "mulq 8(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[0] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,8(%0)\n"
    ".sqshr256s1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testq $4,%2\n"
    "jz .sqshr256s2\n"
    /* a[0]*b[2] */
    "movq 0(%1),%%rax\n"
    "mulq 16(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[0] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"
    ".sqshr256s2:\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    "testq $8,%2\n"
    "jz .sqshr256s3\n"
    /* a[0]*b[3] */
    "movq 0(%1),%%rax\n"
    "mulq 24(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[0] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[2] */
    "movq 8(%1),%%rax\n"
    "mulq 16(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[1] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,24(%0)\n"
    ".sqshr256s3:\n"

    /* s4 */
    "xorq %%r8,%%r8\n"
    "testq $0x10,%2\n"
    "jz .sqshr256s4\n"
    /* a[1]*b[3] */
    "movq 8(%1),%%rax\n"
    "mulq 24(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[1] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[2] */
    "movq 16(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,32(%0)\n"
    ".sqshr256s4:\n"

    /* s5 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x20,%2\n"
    "jz .sqshr256s5\n"
    /* a[2]*b[3] */
    "movq 16(%1),%%rax\n"
    "mulq 24(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[2] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,40(%0)\n"
    ".sqshr256s5:\n"

    /* s6 */
    "testq $0xC0,%2\n"
    "jz .sqshr256s6\n"
    /* a[3]*b[3] */
    "movq 24(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* add sum */
    "movq %%r8,48(%0)\n"
    "movq %%rbx,56(%0)\n"
    ".sqshr256s6:\n"
    :
    : "r" (result), "r" (src), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );

#else

  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%2\n"
    "jz .sqshr256s0\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull %%eax\n"
    "movl %%eax,0(%0)\n"
    "addl %%edx,%%ebx\n"
    ".sqshr256s0:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    "testl $2,%2\n"
    "jz .sqshr256s1\n"
    /* a[0]*b[1] */
    "movl 0(%1),%%eax\n"
    "mull 4(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[0] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %%ebx,4(%0)\n"
    ".sqshr256s1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%2\n"
    "jz .sqshr256s2\n"
    /* a[0]*b[2] */
    "movl 0(%1),%%eax\n"
    "mull 8(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[0] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[1] */
    "movl 4(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %%ecx,8(%0)\n"
    ".sqshr256s2:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%2\n"
    "jz .sqshr256s3\n"
    /* a[0]*b[3] */
    "movl 0(%1),%%eax\n"
    "mull 12(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[0] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[2] */
    "movl 4(%1),%%eax\n"
    "mull 8(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[1] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %%edi,12(%0)\n"
    ".sqshr256s3:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%2\n"
    "jz .sqshr256s4\n"
    /* a[0]*b[4] */
    "movl 0(%1),%%eax\n"
    "mull 16(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[0] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[3] */
    "movl 4(%1),%%eax\n"
    "mull 12(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[1] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[2] */
    "movl 8(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %%ebx,16(%0)\n"
    ".sqshr256s4:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%2\n"
    "jz .sqshr256s5\n"
    /* a[0]*b[5] */
    "movl 0(%1),%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[0] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[4] */
    "movl 4(%1),%%eax\n"
    "mull 16(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[1] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[3] */
    "movl 8(%1),%%eax\n"
    "mull 12(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[2] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %%ecx,20(%0)\n"
    ".sqshr256s5:\n"

    /* s6 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x40,%2\n"
    "jz .sqshr256s6\n"
    /* a[0]*b[6] */
    "movl 0(%1),%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[0] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[5] */
    "movl 4(%1),%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[1] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[4] */
    "movl 8(%1),%%eax\n"
    "mull 16(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[2] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[3] */
    "movl 12(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %%edi,24(%0)\n"
    ".sqshr256s6:\n"

    /* s7 */
    "xorl %%edi,%%edi\n"
    "testl $0x80,%2\n"
    "jz .sqshr256s7\n"
    /* a[0]*b[7] */
    "movl 0(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[0] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[6] */
    "movl 4(%1),%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[1] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[5] */
    "movl 8(%1),%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[2] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[4] */
    "movl 12(%1),%%eax\n"
    "mull 16(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[3] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %%ebx,28(%0)\n"
    ".sqshr256s7:\n"

    /* s8 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x100,%2\n"
    "jz .sqshr256s8\n"
    /* a[1]*b[7] */
    "movl 4(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[1] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[6] */
    "movl 8(%1),%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[2] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[5] */
    "movl 12(%1),%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[3] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[4] */
    "movl 16(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %%ecx,32(%0)\n"
    ".sqshr256s8:\n"

    /* s9 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x200,%2\n"
    "jz .sqshr256s9\n"
    /* a[2]*b[7] */
    "movl 8(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[2] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[6] */
    "movl 12(%1),%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[3] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[5] */
    "movl 16(%1),%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[4] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %%edi,36(%0)\n"
    ".sqshr256s9:\n"

    /* s10 */
    "xorl %%edi,%%edi\n"
    "testl $0x400,%2\n"
    "jz .sqshr256s10\n"
    /* a[3]*b[7] */
    "movl 12(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[3] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[6] */
    "movl 16(%1),%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[4] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[5] */
    "movl 20(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %%ebx,40(%0)\n"
    ".sqshr256s10:\n"

    /* s11 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x800,%2\n"
    "jz .sqshr256s11\n"
    /* a[4]*b[7] */
    "movl 16(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[4] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[6] */
    "movl 20(%1),%%eax\n"
    "mull 24(%1)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[5] */
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %%ecx,44(%0)\n"
    ".sqshr256s11:\n"

    /* s12 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x1000,%2\n"
    "jz .sqshr256s12\n"
    /* a[5]*b[7] */
    "movl 20(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[5] */
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[6] */
    "movl 24(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %%edi,48(%0)\n"
    ".sqshr256s12:\n"

    /* s13 */
    "xorl %%edi,%%edi\n"
    "testl $0x2000,%2\n"
    "jz .sqshr256s13\n"
    /* a[6]*b[7] */
    "movl 24(%1),%%eax\n"
    "mull 28(%1)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[6] */
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %%ebx,52(%0)\n"
    ".sqshr256s13:\n"

    /* s14 */
    "testl $0xC000,%2\n"
    "jz .sqshr256s14\n"
    /* a[7]*b[7] */
    "movl 28(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* add sum */
    "movl %%ecx,56(%0)\n"
    "movl %%edi,60(%0)\n"
    ".sqshr256s14:\n"
    :
    : "r" (result), "r" (src), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );

#endif

  if( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn256SubHighTwice( result, src->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn256Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn256Add32( dst, 1 );
  }
  else
  {
    bn256CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn256Add32( dst, 1 );
  }

  return;
}



////



void bn256Div32( bn256 *dst, uint32_t divisor, uint32_t *rem )
{
  bnShift offset, divisormsb;
  bn256 div, quotient, dsthalf, qpart, dpart;
  bn256Set32( &div, divisor );
  bn256Zero( &quotient );
  divisormsb = bn256GetIndexMSB( &div );
  for( ; ; )
  {
    if( bn256CmpLt( dst, &div ) )
      break;
    bn256Set( &dpart, &div );
    bn256Set32( &qpart, 1 );
    bn256SetShr1( &dsthalf, dst );
    offset = bn256GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn256Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn256CmpLe( &dpart, &dsthalf ) )
    {
      bn256Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn256Shl( &qpart, &qpart, offset );
    bn256Sub( dst, &dpart );
    bn256Add( &quotient, &qpart );
  }
  if( rem )
    *rem = dst->unit[0];
  if( bn256CmpEq( dst, &div ) )
  {
    if( rem )
      *rem = 0;
    bn256Add32( &quotient, 1 );
  }
  bn256Set( dst, &quotient );
  return;
}


void bn256Div32Signed( bn256 *dst, int32_t divisor, int32_t *rem )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn256Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn256Div32( dst, (uint32_t)divisor, (uint32_t *)rem );
  if( sign )
  {
    bn256Neg( dst );
    if( rem )
      *rem = -*rem;
  }
  return;
}


void bn256Div32Round( bn256 *dst, uint32_t divisor )
{
  uint32_t rem;
  bn256Div32( dst, divisor, &rem );
  if( ( rem << 1 ) >= divisor )
    bn256Add32( dst, 1 );
  return;
}


void bn256Div32RoundSigned( bn256 *dst, int32_t divisor )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn256Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn256Div32Round( dst, (uint32_t)divisor );
  if( sign )
    bn256Neg( dst );
  return;
}


void bn256Div( bn256 *dst, const bn256 * const divisor, bn256 *rem )
{
  bnShift offset, remzero, divisormsb;
  bn256 quotient, dsthalf, qpart, dpart;
  bn256Zero( &quotient );
  divisormsb = bn256GetIndexMSB( divisor );
  for( ; ; )
  {
    if( bn256CmpLt( dst, divisor ) )
      break;
    bn256Set( &dpart, divisor );
    bn256Set32( &qpart, 1 );
    bn256SetShr1( &dsthalf, dst );
    offset = bn256GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn256Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn256CmpLe( &dpart, &dsthalf ) )
    {
      bn256Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn256Shl( &qpart, &qpart, offset );
    bn256Sub( dst, &dpart );
    bn256Add( &quotient, &qpart );
  }
  remzero = 0;
  if( bn256CmpEq( dst, divisor ) )
  {
    bn256Add32( &quotient, 1 );
    remzero = 1;
  }
  if( rem )
  {
    if( remzero )
      bn256Zero( rem );
    else
      bn256Set( rem, dst );
  }
  bn256Set( dst, &quotient );
  return;
}


void bn256DivSigned( bn256 *dst, const bn256 * const divisor, bn256 *rem )
{
  int sign;
  bn256 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn256Neg( dst );
  }
  if( divisor->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn256SetNeg( &divisorabs, divisor );
    bn256Div( dst, &divisorabs, rem );
  }
  else
    bn256Div( dst, divisor, rem );
  if( sign )
  {
    bn256Neg( dst );
    if( rem )
      bn256Neg( rem );
  }
  return;
}


void bn256DivRound( bn256 *dst, const bn256 * const divisor )
{
  bn256 rem;
  bn256Div( dst, divisor, &rem );
  bn256Shl1( &rem );
  if( bn256CmpGe( &rem, divisor ) )
    bn256Add32( dst, 1 );
  return;
}


void bn256DivRoundSigned( bn256 *dst, const bn256 * const divisor )
{
  int sign;
  bn256 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn256Neg( dst );
  }
  if( divisor->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn256SetNeg( &divisorabs, divisor );
    bn256DivRound( dst, &divisorabs );
  }
  else
    bn256DivRound( dst, divisor );
  if( sign )
    bn256Neg( dst );
  return;
}


void bn256DivShl( bn256 *dst, const bn256 * const divisor, bn256 *rem, bnShift shift )
{
  bnShift fracshift;
  bn256 basedst, fracpart, qpart;
  if( !( rem ) )
    rem = &fracpart;
  bn256Set( &basedst, dst );
  bn256Div( &basedst, divisor, rem );
  bn256Shl( dst, &basedst, shift );
  for( fracshift = shift-1 ; fracshift >= 0 ; fracshift-- )
  {
    if( bn256CmpZero( rem ) )
      break;
    bn256Shl1( rem );
    if( bn256CmpGe( rem, divisor ) )
    {
      bn256Sub( rem, divisor );
      bn256Set32Shl( &qpart, 1, fracshift );
      bn256Or( dst, &qpart );
    }
  }
  return;
}


void bn256DivSignedShl( bn256 *dst, const bn256 * const divisor, bn256 *rem, bnShift shift )
{
  int sign;
  bn256 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn256Neg( dst );
  }
  if( divisor->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn256SetNeg( &divisorabs, divisor );
    bn256DivShl( dst, &divisorabs, rem, shift );
  }
  else
    bn256DivShl( dst, divisor, rem, shift );
  if( sign )
  {
    bn256Neg( dst );
    if( rem )
      bn256Neg( rem );
  }
  return;
}


void bn256DivRoundShl( bn256 *dst, const bn256 * const divisor, bnShift shift )
{
  bn256 rem;
  bn256DivShl( dst, divisor, &rem, shift );
  bn256Shl1( &rem );
  if( bn256CmpGe( &rem, divisor ) )
    bn256Add32( dst, 1 );
  return;
}


void bn256DivRoundSignedShl( bn256 *dst, const bn256 * const divisor, bnShift shift )
{
  int sign;
  bn256 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn256Neg( dst );
  }
  if( divisor->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn256SetNeg( &divisorabs, divisor );
    bn256DivRoundShl( dst, &divisorabs, shift );
  }
  else
    bn256DivRoundShl( dst, divisor, shift );
  if( sign )
    bn256Neg( dst );
  return;
}



////



void bn256Or( bn256 *dst, const bn256 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%r9\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%r8\n"
    "orq %%rax,0(%0)\n"
    "orq %%r9,8(%0)\n"
    "orq %%rcx,16(%0)\n"
    "orq %%r8,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rcx", "%r8", "%r9", "memory" );
#else
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] |= src->unit[unitindex];
#endif
  return;
}


void bn256SetOr( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%r9\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%r8\n"
    "orq 0(%2),%%rax\n"
    "orq 8(%2),%%r9\n"
    "orq 16(%2),%%rcx\n"
    "orq 24(%2),%%r8\n"
    "movq %%rax,0(%0)\n"
    "movq %%r9,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    "movq %%r8,24(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rcx", "%r8", "%r9", "memory" );
#else
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] | src1->unit[unitindex];
#endif
  return;
}


void bn256Nor( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( dst->unit[unitindex] | src->unit[unitindex] );
  return;
}


void bn256SetNor( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] | src1->unit[unitindex] );
  return;
}


void bn256And( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] &= src->unit[unitindex];
  return;
}


void bn256SetAnd( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] & src1->unit[unitindex];
  return;
}


void bn256Nand( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] & dst->unit[unitindex] );
  return;
}


void bn256SetNand( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] & src1->unit[unitindex] );
  return;
}


void bn256Xor( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] ^= src->unit[unitindex];
  return;
}


void bn256SetXor( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] ^ src1->unit[unitindex];
  return;
}


void bn256Nxor( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] ^ dst->unit[unitindex] );
  return;
}


void bn256SetNxor( bn256 *dst, const bn256 * const src0, const bn256 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] ^ src1->unit[unitindex] );
  return;
}


void bn256Not( bn256 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  return;
}


void bn256SetNot( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  return;
}


void bn256Neg( bn256 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  bn256Add32( dst, 1 );
  return;
}


void bn256SetNeg( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  bn256Add32( dst, 1 );
  return;
}


void bn256Shl( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[-offset];
  if( !( shift ) )
  {
    limit = offset;
    for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= limit ; unitindex-- )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex >= 0 ; unitindex-- )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = offset;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = BN_INT256_UNIT_COUNT-1;
    if( offset < BN_INT256_UNIT_COUNT )
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


void bn256Shr( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT256_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = BN_INT256_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT256_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift );
      unitindex++;
    }
    for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn256Sal( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  bn256Shl( dst, src, shiftbits );
  return;
}


void bn256Sar( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  if( !( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn256Shr( dst, src, shiftbits );
    return;
  }
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT256_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = -1;
  }
  else
  {
    limit = BN_INT256_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT256_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( (bnUnit)-1 << shiftinv );;
      unitindex++;
    }
    for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn256ShrRound( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn256ExtractBit( src, shiftbits-1 );
  bn256Shr( dst, src, shiftbits );
  if( bit )
    bn256Add32( dst, 1 );
  return;
}


void bn256SarRound( bn256 *dst, const bn256 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn256ExtractBit( src, shiftbits-1 );
  bn256Sar( dst, src, shiftbits );
  if( bit )
    bn256Add32( dst, 1 );
  return;
}


void bn256Shl1( bn256 *dst )
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
    :
    : "r" (dst->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256SetShl1( bn256 *dst, const bn256 * const src )
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn256Shr1( bn256 *dst )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 );
#else
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 ) | ( dst->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( dst->unit[4] >> 1 ) | ( dst->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( dst->unit[5] >> 1 ) | ( dst->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( dst->unit[6] >> 1 ) | ( dst->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( dst->unit[7] >> 1 );
#endif
  return;
}


void bn256SetShr1( bn256 *dst, const bn256 * const src )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 );
#else
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 ) | ( src->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( src->unit[4] >> 1 ) | ( src->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( src->unit[5] >> 1 ) | ( src->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( src->unit[6] >> 1 ) | ( src->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( src->unit[7] >> 1 );
#endif
  return;
}



////



int bn256CmpZero( const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 0;
  }
  return 1;
}

int bn256CmpNotZero( const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn256CmpEq( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn256CmpNeq( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn256CmpGt( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn256CmpGe( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn256CmpLt( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn256CmpLe( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn256CmpSignedGt( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT256_UNIT_COUNT-1] ^ src->unit[BN_INT256_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT256_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn256CmpSignedGe( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT256_UNIT_COUNT-1] ^ src->unit[BN_INT256_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT256_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn256CmpSignedLt( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT256_UNIT_COUNT-1] ^ src->unit[BN_INT256_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT256_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn256CmpSignedLe( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT256_UNIT_COUNT-1] ^ src->unit[BN_INT256_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT256_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn256CmpPositive( const bn256 * const src )
{
  return ( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) == 0;
}


int bn256CmpNegative( const bn256 * const src )
{
  return ( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) != 0;
}


int bn256CmpEqOrZero( bn256 *dst, const bn256 * const src )
{
  int unitindex;
  bnUnit accum, diffaccum;
  accum = 0;
  diffaccum = 0;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
  {
    diffaccum |= dst->unit[unitindex] - src->unit[unitindex];
    accum |= dst->unit[unitindex];
  }
  return ( ( accum & diffaccum ) == 0 );
}


int bn256CmpPart( bn256 *dst, const bn256 * const src, uint32_t bits )
{
  int unitindex, unitcount, partbits;
  bnUnit unitmask;
  unitcount = ( bits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  partbits = bits & (BN_UNIT_BITS-1);
  unitindex = BN_INT256_UNIT_COUNT - unitcount;
  if( partbits )
  {
    unitmask = (bnUnit)-1 << (BN_UNIT_BITS-partbits);
    if( ( dst->unit[unitindex] & unitmask ) != ( src->unit[unitindex] & unitmask ) )
      return 0;
    unitindex++;
  }
  for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}



////



int bn256ExtractBit( const bn256 * const src, int bitoffset )
{
  int unitindex, shift;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  value = 0;
  if( (unsigned)unitindex < BN_INT256_UNIT_COUNT )
    value = ( src->unit[unitindex] >> shift ) & 0x1;
  return value;
}


uint32_t bn256Extract32( const bn256 * const src, int bitoffset )
{
  int unitindex, shift, shiftinv;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT256_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 32 ) && ( (unsigned)unitindex+1 < BN_INT256_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
}


uint64_t bn256Extract64( const bn256 * const src, int bitoffset )
{
#if BN_UNIT_BITS == 64
  int unitindex, shift, shiftinv;
  uint64_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT256_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 64 ) && ( (unsigned)unitindex+1 < BN_INT256_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
#else
  return (uint64_t)bn256Extract32( src, bitoffset ) | ( (uint64_t)bn256Extract32( src, bitoffset+32 ) << 32 );
#endif
}


int bn256GetIndexMSB( const bn256 * const src )
{
  int unitindex;
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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


int bn256GetIndexMSZ( const bn256 * const src )
{
  int unitindex;
  bnUnit unit;
  for( unitindex = BN_INT256_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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



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
#define BN_XP_SUPPORT_192 1
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 0
#define BN_XP_SUPPORT_1024 0
#include "bn.h"


/*
Big number fixed point two-complement arithmetics, 192 bits

On 32 bits, this file *MUST* be compiled with -fomit-frame-pointer to free the %ebp register
In fact, don't even think about compiling 32 bits. This is much faster on 64 bits.
*/

#define BN192_PRINT_REPORT



#if BN_UNIT_BITS == 64
 #define PRINT(p,x) printf( p "%lx %lx %lx\n", (long)(x)->unit[2], (long)(x)->unit[1], (long)(x)->unit[0] )
#else
 #define PRINT(p,x) printf( p "%x %x %x %x %x %x\n", (x)->unit[5], (x)->unit[4], (x)->unit[3], (x)->unit[2], (x)->unit[1], (x)->unit[0] )
#endif


static inline __attribute__((always_inline)) void bn192Copy( bn192 *dst, bnUnit *src )
{
  dst->unit[0] = src[0];
  dst->unit[1] = src[1];
  dst->unit[2] = src[2];
#if BN_UNIT_BITS == 32
  dst->unit[3] = src[3];
  dst->unit[4] = src[4];
  dst->unit[5] = src[5];
#endif
  return;
}

static inline __attribute__((always_inline)) void bn192CopyShift( bn192 *dst, bnUnit *src, bnShift shift )
{
  bnShift shiftinv;
  shiftinv = BN_UNIT_BITS - shift;
  dst->unit[0] = ( src[0] >> shift ) | ( src[1] << shiftinv );
  dst->unit[1] = ( src[1] >> shift ) | ( src[2] << shiftinv );
  dst->unit[2] = ( src[2] >> shift ) | ( src[3] << shiftinv );
#if BN_UNIT_BITS == 32
  dst->unit[3] = ( src[3] >> shift ) | ( src[4] << shiftinv );
  dst->unit[4] = ( src[4] >> shift ) | ( src[5] << shiftinv );
  dst->unit[5] = ( src[5] >> shift ) | ( src[6] << shiftinv );
#endif
  return;
}

static inline int bn192HighNonMatch( const bn192 * const src, bnUnit value )
{
  int high;
  high = 0;
#if BN_UNIT_BITS == 64
  if( src->unit[2] != value )
    high = 2;
  else if( src->unit[1] != value )
    high = 1;
#else
  if( src->unit[5] != value )
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


static inline bnUnit bn192SignMask( const bn192 * const src )
{
  return ( (bnUnitSigned)src->unit[BN_INT192_UNIT_COUNT-1] ) >> (BN_UNIT_BITS-1);
}



////



void bn192Zero( bn192 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn192Set( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src->unit[unitindex];
  return;
}


void bn192Set32( bn192 *dst, uint32_t value )
{
  int unitindex;
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn192Set32Signed( bn192 *dst, int32_t value )
{
  int unitindex;
  bnUnit u;
  u = ( (bnUnitSigned)value ) >> (BN_UNIT_BITS-1);
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = u;
  return;
}


void bn192Set32Shl( bn192 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
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
  if( (unsigned)unitindex < BN_INT192_UNIT_COUNT )
    dst->unit[unitindex] = vl;
  if( (unsigned)++unitindex < BN_INT192_UNIT_COUNT )
    dst->unit[unitindex] = vh;
  return;
}


void bn192Set32SignedShl( bn192 *dst, int32_t value, bnShift shift )
{
  if( value < 0 )
  {
    bn192Set32Shl( dst, -value, shift );
    bn192Neg( dst );
  }
  else
    bn192Set32Shl( dst, value, shift );
  return;
}


#define BN_IEEE754DBL_MANTISSA_BITS (52)
#define BN_IEEE754DBL_EXPONENT_BITS (11)

void bn192SetDouble( bn192 *dst, double value, bnShift leftshiftbits )
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

  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
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
    if( (unsigned)unitindex < BN_INT192_UNIT_COUNT )
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
    for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = ~dst->unit[unitindex];
    bn192Add32( dst, 1 );
  }

  return;
}


double bn192GetDouble( bn192 *dst, bnShift rightshiftbits )
{
  int exponent, sign, msb;
  uint64_t mantissa;
  union
  {
    double d;
    uint64_t u;
  } dblunion;
  bn192 value;

  sign = 0;
  if( ( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn192SetNeg( &value, dst );
    sign = 1;
  }
  else
    bn192Set( &value, dst );

  msb = bn192GetIndexMSB( &value );
  if( msb < 0 )
    return ( sign ? -0.0 : 0.0 );
  mantissa = bn192Extract64( &value, msb-64 );
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


void bn192Add32( bn192 *dst, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "addq %1,0(%0)\n"
    "jnc 1f\n"
    "addq $1,8(%0)\n"
    "jnc 1f\n"
    "addq $1,16(%0)\n"
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn192Sub32( bn192 *dst, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "subq %1,0(%0)\n"
    "jnc 1f\n"
    "subq $1,8(%0)\n"
    "jnc 1f\n"
    "subq $1,16(%0)\n"
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn192Add32s( bn192 *dst, int32_t value )
{
  if( value < 0 )
    bn192Sub32( dst, -value );
  else
    bn192Add32( dst, value );
  return;
}


void bn192Sub32s( bn192 *dst, int32_t value )
{
  if( value < 0 )
    bn192Add32( dst, -value );
  else
    bn192Sub32( dst, value );
  return;
}


void bn192Add32Shl( bn192 *dst, uint32_t value, bnShift shift )
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "addq %1,8(%0)\n"
        "adcq %2,16(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "addq %1,16(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 4:
      __asm__ __volatile__(
        "addl %1,16(%0)\n"
        "adcl %2,20(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 5:
      __asm__ __volatile__(
        "addl %1,20(%0)\n"
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


void bn192Sub32Shl( bn192 *dst, uint32_t value, bnShift shift )
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "subq %1,8(%0)\n"
        "sbbq %2,16(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "subq %1,16(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 4:
      __asm__ __volatile__(
        "subl %1,16(%0)\n"
        "sbbl %2,20(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 5:
      __asm__ __volatile__(
        "subl %1,20(%0)\n"
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


void bn192SetBit( bn192 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] |= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


void bn192ClearBit( bn192 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] &= ~( (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) ) );
  return;
}


void bn192FlipBit( bn192 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] ^= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


////


void bn192Add( bn192 *dst, const bn192 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq %%rax,0(%0)\n"
    "adcq %%rbx,8(%0)\n"
    "adcq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192SetAdd( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192Sub( bn192 *dst, const bn192 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "subq %%rax,0(%0)\n"
    "sbbq %%rbx,8(%0)\n"
    "sbbq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192SetSub( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "subq 0(%2),%%rax\n"
    "sbbq 8(%2),%%rbx\n"
    "sbbq 16(%2),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192SetAddAdd( bn192 *dst, const bn192 * const src, const bn192 * const srcadd0, const bn192 * const srcadd1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "adcq 16(%3),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
#else
  bn192SetAdd( dst, src, srcadd0 );
  bn192Add( dst, srcadd1 );
#endif
  return;
}


void bn192SetAddSub( bn192 *dst, const bn192 * const src, const bn192 * const srcadd, const bn192 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "subq 0(%3),%%rax\n"
    "sbbq 8(%3),%%rbx\n"
    "sbbq 16(%3),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
#else
  bn192SetAdd( dst, src, srcadd );
  bn192Sub( dst, srcsub );
#endif
  return;
}


void bn192SetAddAddSub( bn192 *dst, const bn192 * const src, const bn192 * const srcadd0, const bn192 * const srcadd1, const bn192 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "adcq 16(%3),%%rcx\n"
    "subq 0(%4),%%rax\n"
    "sbbq 8(%4),%%rbx\n"
    "sbbq 16(%4),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
#else
  bn192SetAdd( dst, src, srcadd0 );
  bn192Add( dst, srcadd1 );
  bn192Sub( dst, srcsub );
#endif
  return;
}


void bn192SetAddAddAddSub( bn192 *dst, const bn192 * const src, const bn192 * const srcadd0, const bn192 * const srcadd1, const bn192 * const srcadd2, const bn192 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "adcq 16(%2),%%rcx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "adcq 16(%3),%%rcx\n"
    "addq 0(%4),%%rax\n"
    "adcq 8(%4),%%rbx\n"
    "adcq 16(%4),%%rcx\n"
    "subq 0(%5),%%rax\n"
    "sbbq 8(%5),%%rbx\n"
    "sbbq 16(%5),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit), "r" (srcadd2->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
#else
  bn192SetAdd( dst, src, srcadd0 );
  bn192Add( dst, srcadd1 );
  bn192Add( dst, srcadd2 );
  bn192Sub( dst, srcsub );
#endif
  return;
}


void bn192Mul32( bn192 *dst, const bn192 * const src, uint32_t value )
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
    "movq %%rcx,16(%0)\n"
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

    "movl %2,%%eax\n"
    "mull 20(%1)\n"
    "addl %%eax,%%ebx\n"
    "movl %%ebx,20(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192Mul32Signed( bn192 *dst, const bn192 * const src, int32_t value )
{
  if( value < 0 )
  {
    bn192Mul32( dst, src, -value );
    bn192Neg( dst );
  }
  else
    bn192Mul32( dst, src, value );
  return;
}


bnUnit bn192Mul32Check( bn192 *dst, const bn192 * const src, uint32_t value )
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
    "movq %%rbx,%0\n"
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
    "movl %%ecx,%0\n"
    : "=g" (overflow)
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return overflow;
}


bnUnit bn192Mul32SignedCheck( bn192 *dst, const bn192 * const src, int32_t value )
{
  bnUnit ret;
  if( value < 0 )
  {
    ret = bn192Mul32Check( dst, src, -value );
    bn192Neg( dst );
  }
  else
    ret = bn192Mul32Check( dst, src, value );
  return ret;
}


void bn192Mul( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
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
    /* add sum */
    "movq %%rbx,8(%0)\n"

    /* s2 */
    /* a[0]*b[2] */
    "movq 0(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rcx\n"
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    /* a[2]*b[0] */
    "movq 16(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rcx\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
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
    /* a[0]*b[4] */
    "movl 0(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* a[1]*b[3] */
    "movl 4(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* a[2]*b[2] */
    "movl 8(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* a[3]*b[1] */
    "movl 12(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* a[4]*b[0] */
    "movl 16(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,16(%%eax)\n"

    /* s5 */
    /* a[0]*b[5] */
    "movl 0(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    /* a[1]*b[4] */
    "movl 4(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    /* a[2]*b[3] */
    "movl 8(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    /* a[3]*b[2] */
    "movl 12(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    /* a[4]*b[1] */
    "movl 16(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    /* a[5]*b[0] */
    "movl 20(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,20(%%eax)\n"
    :
    : "m" (dst), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return;
}


bnUnit bn192MulCheck( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int highsum;
  bn192Mul( dst, src0, src1 );
  highsum  = bn192HighNonMatch( src0, 0 ) + bn192HighNonMatch( src1, 0 );
  return ( highsum >= BN_INT192_UNIT_COUNT );
}


bnUnit bn192MulSignedCheck( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int highsum;
  bn192Mul( dst, src0, src1 );
  highsum = bn192HighNonMatch( src0, bn192SignMask( src0 ) ) + bn192HighNonMatch( src1, bn192SignMask( src1 ) );
  return ( highsum >= BN_INT192_UNIT_COUNT );
}


void __attribute__ ((noinline)) bn192MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testq $1,%3\n"
    "jz .mulext192s0\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq 0(%2)\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"
    ".mulext192s0:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testq $2,%3\n"
    "jz .mulext192s1\n"
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
    ".mulext192s1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testq $4,%3\n"
    "jz .mulext192s2\n"
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
    ".mulext192s2:\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    "testq $8,%3\n"
    "jz .mulext192s3\n"
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
    /* add sum */
    "movq %%r8,24(%0)\n"
    ".mulext192s3:\n"

    /* s4 */
    "testq $0x30,%3\n"
    "jz .mulext192s4\n"
    /* a[2]*b[2] */
    "movq 16(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /* add sum */
    "movq %%rbx,32(%0)\n"
    "movq %%rcx,40(%0)\n"
    ".mulext192s4:\n"
    :
    : "r" (result), "r" (src0), "r" (src1), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );
#else
  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%3\n"
    "jz .mulext192s0\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull 0(%2)\n"
    "movl %0,%%edi\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%edx,%%ebx\n"
    ".mulext192s0:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    "testl $2,%3\n"
    "jz .mulext192s1\n"
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
    ".mulext192s1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%3\n"
    "jz .mulext192s2\n"
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
    ".mulext192s2:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%3\n"
    "jz .mulext192s3\n"
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
    ".mulext192s3:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%3\n"
    "jz .mulext192s4\n"
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
    ".mulext192s4:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%3\n"
    "jz .mulext192s5\n"
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
    ".mulext192s5:\n"

    /* s6 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x40,%3\n"
    "jz .mulext192s6\n"
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
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,24(%%eax)\n"
    ".mulext192s6:\n"

    /* s7 */
    "xorl %%edi,%%edi\n"
    "testl $0x80,%3\n"
    "jz .mulext192s7\n"
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
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,28(%%eax)\n"
    ".mulext192s7:\n"

    /* s8 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x100,%3\n"
    "jz .mulext192s8\n"
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
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,32(%%eax)\n"
    ".mulext192s8:\n"

    /* s9 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x200,%3\n"
    "jz .mulext192s9\n"
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
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,36(%%eax)\n"
    ".mulext192s9:\n"

    /* s10 */
    "testl $0xC00,%3\n"
    "jz .mulext192s10\n"
    /* a[5]*b[5] */
    "movl 20(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,40(%%eax)\n"
    "movl %%ecx,44(%%eax)\n"
    ".mulext192s10:\n"
    :
    : "m" (result), "r" (src0), "r" (src1), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return;
}


static inline void bn192SubHigh( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "subq %%rax,24(%0)\n"
    "sbbq %%rbx,32(%0)\n"
    "sbbq %%rcx,40(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,24(%0)\n"
    "sbbl %%ebx,28(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,32(%0)\n"
    "sbbl %%edx,36(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,40(%0)\n"
    "sbbl %%ebx,44(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


static inline void bn192SubHighTwice( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "subq %%rax,24(%0)\n"
    "sbbq %%rbx,32(%0)\n"
    "sbbq %%rcx,40(%0)\n"
    "subq %%rax,24(%0)\n"
    "sbbq %%rbx,32(%0)\n"
    "sbbq %%rcx,40(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,24(%0)\n"
    "sbbl %%ebx,28(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,32(%0)\n"
    "sbbl %%edx,36(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,40(%0)\n"
    "sbbl %%ebx,44(%0)\n"
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,24(%0)\n"
    "sbbl %%ebx,28(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,32(%0)\n"
    "sbbl %%edx,36(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,40(%0)\n"
    "sbbl %%ebx,44(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192MulShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT192_UNIT_COUNT*2];

  highunit = ( 192 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn192MulExtended( result, src0->unit, src1->unit, unitmask );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn192Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn192Add32( dst, 1 );
  }
  else
  {
    bn192CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn192Add32( dst, 1 );
  }

  return;
}


void bn192MulSignedShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT192_UNIT_COUNT*2];

  highunit = ( 192 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn192MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn192SubHigh( result, src1->unit );
  if( src1->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn192SubHigh( result, src0->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn192Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn192Add32( dst, 1 );
  }
  else
  {
    bn192CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn192Add32( dst, 1 );
  }

  return;
}


bnUnit bn192MulCheckShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT192_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 192 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn192MulExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn192HighNonMatch( src0, 0 ) + bn192HighNonMatch( src1, 0 );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT192_UNIT_COUNT );
    bn192Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn192Add32( dst, 1 );
  }
  else
  {
    overflow = 0;
    if( ( highsum >= offset+BN_INT192_UNIT_COUNT+1 ) || ( result[offset+BN_INT192_UNIT_COUNT] >> shift ) )
      overflow = 1;
    bn192CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn192Add32( dst, 1 );
  }

  return overflow;
}


bnUnit bn192MulSignedCheckShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT192_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 192 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn192MulExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn192HighNonMatch( src0, bn192SignMask( src0 ) ) + bn192HighNonMatch( src1, bn192SignMask( src1 ) );

  if( src0->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn192SubHigh( result, src1->unit );
  if( src1->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn192SubHigh( result, src0->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT192_UNIT_COUNT );
    bn192Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn192Add32( dst, 1 );
  }
  else
  {
    if( highsum >= offset+BN_INT192_UNIT_COUNT+1 )
      overflow = 1;
    else
    {
      overflow = result[offset+BN_INT192_UNIT_COUNT];
      if( src0->unit[BN_INT192_UNIT_COUNT-1] ^ src1->unit[BN_INT192_UNIT_COUNT-1] )
        overflow = ~overflow;
      overflow >>= shift;
    }
    bn192CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn192Add32( dst, 1 );
  }

  return overflow;
}


void bn192SquareShr( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT192_UNIT_COUNT*2];

  highunit = ( 192 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
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
    "jz .sqshr192s0\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq %%rax\n"
    "movq %%rax,0(%0)\n"
    "addq %%rdx,%%rbx\n"
    ".sqshr192s0:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testq $2,%2\n"
    "jz .sqshr192s1\n"
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
    ".sqshr192s1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testq $4,%2\n"
    "jz .sqshr192s2\n"
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
    ".sqshr192s2:\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    "testq $8,%2\n"
    "jz .sqshr192s3\n"
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
    ".sqshr192s3:\n"

    /* s4 */
    "testq $0x30,%2\n"
    "jz .sqshr192s4\n"
    /* a[2]*b[2] */
    "movq 16(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    /* add sum */
    "movq %%rbx,32(%0)\n"
    "movq %%rcx,40(%0)\n"
    ".sqshr192s4:\n"
    :
    : "r" (result), "r" (src), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );

#else

  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%2\n"
    "jz .sqshr192s0\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull %%eax\n"
    "movl %%eax,0(%0)\n"
    "addl %%edx,%%ebx\n"
    ".sqshr192s0:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    "testl $2,%2\n"
    "jz .sqshr192s1\n"
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
    ".sqshr192s1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%2\n"
    "jz .sqshr192s2\n"
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
    ".sqshr192s2:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%2\n"
    "jz .sqshr192s3\n"
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
    ".sqshr192s3:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%2\n"
    "jz .sqshr192s4\n"
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
    ".sqshr192s4:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%2\n"
    "jz .sqshr192s5\n"
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
    ".sqshr192s5:\n"

    /* s6 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x40,%2\n"
    "jz .sqshr192s6\n"
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
    ".sqshr192s6:\n"

    /* s7 */
    "xorl %%edi,%%edi\n"
    "testl $0x80,%2\n"
    "jz .sqshr192s7\n"
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
    ".sqshr192s7:\n"

    /* s8 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x100,%2\n"
    "jz .sqshr192s8\n"
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
    ".sqshr192s8:\n"

    /* s9 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x200,%2\n"
    "jz .sqshr192s9\n"
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
    ".sqshr192s9:\n"

    /* s10 */
    "testl $0xC00,%2\n"
    "jz .sqshr192s10\n"
    /* a[5]*b[5] */
    "movl 20(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    /* add sum */
    "movl %%ebx,40(%0)\n"
    "movl %%ecx,48(%0)\n"
    ".sqshr192s10:\n"
    :
    : "r" (result), "r" (src), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );

#endif

  if( src->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn192SubHighTwice( result, src->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn192Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn192Add32( dst, 1 );
  }
  else
  {
    bn192CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn192Add32( dst, 1 );
  }

  return;
}



////



void bn192Div32( bn192 *dst, uint32_t divisor, uint32_t *rem )
{
  bnShift offset, divisormsb;
  bn192 div, quotient, dsthalf, qpart, dpart;
  bn192Set32( &div, divisor );
  bn192Zero( &quotient );
  divisormsb = bn192GetIndexMSB( &div );
  for( ; ; )
  {
    if( bn192CmpLt( dst, &div ) )
      break;
    bn192Set( &dpart, &div );
    bn192Set32( &qpart, 1 );
    bn192SetShr1( &dsthalf, dst );
    offset = bn192GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn192Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn192CmpLe( &dpart, &dsthalf ) )
    {
      bn192Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn192Shl( &qpart, &qpart, offset );
    bn192Sub( dst, &dpart );
    bn192Add( &quotient, &qpart );
  }
  if( rem )
    *rem = dst->unit[0];
  if( bn192CmpEq( dst, &div ) )
  {
    if( rem )
      *rem = 0;
    bn192Add32( &quotient, 1 );
  }
  bn192Set( dst, &quotient );
  return;
}


void bn192Div32Signed( bn192 *dst, int32_t divisor, int32_t *rem )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn192Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn192Div32( dst, (uint32_t)divisor, (uint32_t *)rem );
  if( sign )
  {
    bn192Neg( dst );
    if( rem )
      *rem = -*rem;
  }
  return;
}


void bn192Div32Round( bn192 *dst, uint32_t divisor )
{
  uint32_t rem;
  bn192Div32( dst, divisor, &rem );
  if( ( rem << 1 ) >= divisor )
    bn192Add32( dst, 1 );
  return;
}


void bn192Div32RoundSigned( bn192 *dst, int32_t divisor )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn192Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn192Div32Round( dst, (uint32_t)divisor );
  if( sign )
    bn192Neg( dst );
  return;
}


void bn192Div( bn192 *dst, const bn192 * const divisor, bn192 *rem )
{
  bnShift offset, remzero, divisormsb;
  bn192 quotient, dsthalf, qpart, dpart;
  bn192Zero( &quotient );
  divisormsb = bn192GetIndexMSB( divisor );
  for( ; ; )
  {
    if( bn192CmpLt( dst, divisor ) )
      break;
    bn192Set( &dpart, divisor );
    bn192Set32( &qpart, 1 );
    bn192SetShr1( &dsthalf, dst );
    offset = bn192GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn192Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn192CmpLe( &dpart, &dsthalf ) )
    {
      bn192Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn192Shl( &qpart, &qpart, offset );
    bn192Sub( dst, &dpart );
    bn192Add( &quotient, &qpart );
  }
  remzero = 0;
  if( bn192CmpEq( dst, divisor ) )
  {
    bn192Add32( &quotient, 1 );
    remzero = 1;
  }
  if( rem )
  {
    if( remzero )
      bn192Zero( rem );
    else
      bn192Set( rem, dst );
  }
  bn192Set( dst, &quotient );
  return;
}


void bn192DivSigned( bn192 *dst, const bn192 * const divisor, bn192 *rem )
{
  int sign;
  bn192 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn192Neg( dst );
  }
  if( divisor->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn192SetNeg( &divisorabs, divisor );
    bn192Div( dst, &divisorabs, rem );
  }
  else
    bn192Div( dst, divisor, rem );
  if( sign )
  {
    bn192Neg( dst );
    if( rem )
      bn192Neg( rem );
  }
  return;
}


void bn192DivRound( bn192 *dst, const bn192 * const divisor )
{
  bn192 rem;
  bn192Div( dst, divisor, &rem );
  bn192Shl1( &rem );
  if( bn192CmpGe( &rem, divisor ) )
    bn192Add32( dst, 1 );
  return;
}


void bn192DivRoundSigned( bn192 *dst, const bn192 * const divisor )
{
  int sign;
  bn192 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn192Neg( dst );
  }
  if( divisor->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn192SetNeg( &divisorabs, divisor );
    bn192DivRound( dst, &divisorabs );
  }
  else
    bn192DivRound( dst, divisor );
  if( sign )
    bn192Neg( dst );
  return;
}


void bn192DivShl( bn192 *dst, const bn192 * const divisor, bn192 *rem, bnShift shift )
{
  bnShift fracshift;
  bn192 basedst, fracpart, qpart;
  if( !( rem ) )
    rem = &fracpart;
  bn192Set( &basedst, dst );
  bn192Div( &basedst, divisor, rem );
  bn192Shl( dst, &basedst, shift );
  for( fracshift = shift-1 ; fracshift >= 0 ; fracshift-- )
  {
    if( bn192CmpZero( rem ) )
      break;
    bn192Shl1( rem );
    if( bn192CmpGe( rem, divisor ) )
    {
      bn192Sub( rem, divisor );
      bn192Set32Shl( &qpart, 1, fracshift );
      bn192Or( dst, &qpart );
    }
  }
  return;
}


void bn192DivSignedShl( bn192 *dst, const bn192 * const divisor, bn192 *rem, bnShift shift )
{
  int sign;
  bn192 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn192Neg( dst );
  }
  if( divisor->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn192SetNeg( &divisorabs, divisor );
    bn192DivShl( dst, &divisorabs, rem, shift );
  }
  else
    bn192DivShl( dst, divisor, rem, shift );
  if( sign )
  {
    bn192Neg( dst );
    if( rem )
      bn192Neg( rem );
  }
  return;
}


void bn192DivRoundShl( bn192 *dst, const bn192 * const divisor, bnShift shift )
{
  bn192 rem;
  bn192DivShl( dst, divisor, &rem, shift );
  bn192Shl1( &rem );
  if( bn192CmpGe( &rem, divisor ) )
    bn192Add32( dst, 1 );
  return;
}


void bn192DivRoundSignedShl( bn192 *dst, const bn192 * const divisor, bnShift shift )
{
  int sign;
  bn192 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn192Neg( dst );
  }
  if( divisor->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn192SetNeg( &divisorabs, divisor );
    bn192DivRoundShl( dst, &divisorabs, shift );
  }
  else
    bn192DivRoundShl( dst, divisor, shift );
  if( sign )
    bn192Neg( dst );
  return;
}



////



void bn192Or( bn192 *dst, const bn192 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%r8\n"
    "movq 16(%1),%%rcx\n"
    "orq %%rax,0(%0)\n"
    "orq %%r8,8(%0)\n"
    "orq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rcx", "%r8", "memory" );
#else
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] |= src->unit[unitindex];
#endif
  return;
}


void bn192SetOr( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%r8\n"
    "movq 16(%1),%%rcx\n"
    "orq 0(%2),%%rax\n"
    "orq 8(%2),%%r8\n"
    "orq 16(%2),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%r8,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rcx", "%r8", "memory" );
#else
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] | src1->unit[unitindex];
#endif
  return;
}


void bn192Nor( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( dst->unit[unitindex] | src->unit[unitindex] );
  return;
}


void bn192SetNor( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] | src1->unit[unitindex] );
  return;
}


void bn192And( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] &= src->unit[unitindex];
  return;
}


void bn192SetAnd( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] & src1->unit[unitindex];
  return;
}


void bn192Nand( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] & dst->unit[unitindex] );
  return;
}


void bn192SetNand( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] & src1->unit[unitindex] );
  return;
}


void bn192Xor( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] ^= src->unit[unitindex];
  return;
}


void bn192SetXor( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] ^ src1->unit[unitindex];
  return;
}


void bn192Nxor( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] ^ dst->unit[unitindex] );
  return;
}


void bn192SetNxor( bn192 *dst, const bn192 * const src0, const bn192 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] ^ src1->unit[unitindex] );
  return;
}


void bn192Not( bn192 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  return;
}


void bn192SetNot( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  return;
}


void bn192Neg( bn192 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  bn192Add32( dst, 1 );
  return;
}


void bn192SetNeg( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  bn192Add32( dst, 1 );
  return;
}


void bn192Shl( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[-offset];
  if( !( shift ) )
  {
    limit = offset;
    for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= limit ; unitindex-- )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex >= 0 ; unitindex-- )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = offset;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = BN_INT192_UNIT_COUNT-1;
    if( offset < BN_INT192_UNIT_COUNT )
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


void bn192Shr( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT192_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = BN_INT192_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT192_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift );
      unitindex++;
    }
    for( ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn192Sal( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  bn192Shl( dst, src, shiftbits );
  return;
}


void bn192Sar( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  if( !( src->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn192Shr( dst, src, shiftbits );
    return;
  }
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT192_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = -1;
  }
  else
  {
    limit = BN_INT192_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT192_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( (bnUnit)-1 << shiftinv );;
      unitindex++;
    }
    for( ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn192ShrRound( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn192ExtractBit( src, shiftbits-1 );
  bn192Shr( dst, src, shiftbits );
  if( bit )
    bn192Add32( dst, 1 );
  return;
}


void bn192SarRound( bn192 *dst, const bn192 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn192ExtractBit( src, shiftbits-1 );
  bn192Sar( dst, src, shiftbits );
  if( bit )
    bn192Add32( dst, 1 );
  return;
}


void bn192Shl1( bn192 *dst )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%0),%%rax\n"
    "movq 8(%0),%%rbx\n"
    "movq 16(%0),%%rcx\n"
    "addq %%rax,0(%0)\n"
    "adcq %%rbx,8(%0)\n"
    "adcq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
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
    :
    : "r" (dst->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192SetShl1( bn192 *dst, const bn192 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "movq 16(%1),%%rcx\n"
    "addq %%rax,%%rax\n"
    "adcq %%rbx,%%rbx\n"
    "adcq %%rcx,%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    "movq %%rcx,16(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "%rcx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn192Shr1( bn192 *dst )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 );
#else
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 ) | ( dst->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( dst->unit[4] >> 1 ) | ( dst->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( dst->unit[5] >> 1 );
#endif
  return;
}


void bn192SetShr1( bn192 *dst, const bn192 * const src )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 );
#else
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 ) | ( src->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( src->unit[4] >> 1 ) | ( src->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( src->unit[5] >> 1 );
#endif
  return;
}



////



int bn192CmpZero( const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 0;
  }
  return 1;
}

int bn192CmpNotZero( const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn192CmpEq( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn192CmpNeq( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn192CmpGt( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn192CmpGe( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn192CmpLt( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn192CmpLe( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn192CmpSignedGt( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT192_UNIT_COUNT-1] ^ src->unit[BN_INT192_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT192_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn192CmpSignedGe( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT192_UNIT_COUNT-1] ^ src->unit[BN_INT192_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT192_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn192CmpSignedLt( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT192_UNIT_COUNT-1] ^ src->unit[BN_INT192_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT192_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn192CmpSignedLe( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT192_UNIT_COUNT-1] ^ src->unit[BN_INT192_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT192_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn192CmpPositive( const bn192 * const src )
{
  return ( src->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) == 0;
}


int bn192CmpNegative( const bn192 * const src )
{
  return ( src->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) != 0;
}


int bn192CmpEqOrZero( bn192 *dst, const bn192 * const src )
{
  int unitindex;
  bnUnit accum, diffaccum;
  accum = 0;
  diffaccum = 0;
  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
  {
    diffaccum |= dst->unit[unitindex] - src->unit[unitindex];
    accum |= dst->unit[unitindex];
  }
  return ( ( accum & diffaccum ) == 0 );
}


int bn192CmpPart( bn192 *dst, const bn192 * const src, uint32_t bits )
{
  int unitindex, unitcount, partbits;
  bnUnit unitmask;
  unitcount = ( bits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  partbits = bits & (BN_UNIT_BITS-1);
  unitindex = BN_INT192_UNIT_COUNT - unitcount;
  if( partbits )
  {
    unitmask = (bnUnit)-1 << (BN_UNIT_BITS-partbits);
    if( ( dst->unit[unitindex] & unitmask ) != ( src->unit[unitindex] & unitmask ) )
      return 0;
    unitindex++;
  }
  for( ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}



////



int bn192ExtractBit( const bn192 * const src, int bitoffset )
{
  int unitindex, shift;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  value = 0;
  if( (unsigned)unitindex < BN_INT192_UNIT_COUNT )
    value = ( src->unit[unitindex] >> shift ) & 0x1;
  return value;
}


uint32_t bn192Extract32( const bn192 * const src, int bitoffset )
{
  int unitindex, shift, shiftinv;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT192_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 32 ) && ( (unsigned)unitindex+1 < BN_INT192_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
}


uint64_t bn192Extract64( const bn192 * const src, int bitoffset )
{
#if BN_UNIT_BITS == 64
  int unitindex, shift, shiftinv;
  uint64_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT192_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 64 ) && ( (unsigned)unitindex+1 < BN_INT192_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
#else
  return (uint64_t)bn192Extract32( src, bitoffset ) | ( (uint64_t)bn192Extract32( src, bitoffset+32 ) << 32 );
#endif
}


int bn192GetIndexMSB( const bn192 * const src )
{
  int unitindex;
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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


int bn192GetIndexMSZ( const bn192 * const src )
{
  int unitindex;
  bnUnit unit;
  for( unitindex = BN_INT192_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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



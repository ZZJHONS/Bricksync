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

#define BN_XP_SUPPORT_128 1
#define BN_XP_SUPPORT_192 0
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 0
#define BN_XP_SUPPORT_1024 0
#include "bn.h"


/*
Big number fixed point two-complement arithmetics, 128 bits

On 32 bits, this file *MUST* be compiled with -fomit-frame-pointer to free the %ebp register
In fact, don't even think about compiling 32 bits. This is much faster on 64 bits.
*/

#define BN128_PRINT_REPORT
#define BN128_FULL_MUL



#if BN_UNIT_BITS == 64
 #define PRINT(p,x) printf( p "%lx %lx\n", (long)(x)->unit[1], (long)(x)->unit[0] )
#else
 #define PRINT(p,x) printf( p "%x %x %x %x\n", (x)->unit[3], (x)->unit[2], (x)->unit[1], (x)->unit[0] )
#endif



static inline __attribute__((always_inline)) void bn128Copy( bn128 *dst, bnUnit *src )
{
  dst->unit[0] = src[0];
  dst->unit[1] = src[1];
#if BN_UNIT_BITS == 32
  dst->unit[2] = src[2];
  dst->unit[3] = src[3];
#endif
  return;
}

static inline __attribute__((always_inline)) void bn128CopyShift( bn128 *dst, bnUnit *src, bnShift shift )
{
  bnShift shiftinv;
  shiftinv = BN_UNIT_BITS - shift;
  dst->unit[0] = ( src[0] >> shift ) | ( src[1] << shiftinv );
  dst->unit[1] = ( src[1] >> shift ) | ( src[2] << shiftinv );
#if BN_UNIT_BITS == 32
  dst->unit[2] = ( src[2] >> shift ) | ( src[3] << shiftinv );
  dst->unit[3] = ( src[3] >> shift ) | ( src[4] << shiftinv );
#endif
  return;
}


static inline int bn128HighNonMatch( const bn128 * const src, bnUnit value )
{
  int high;
  high = 0;
#if BN_UNIT_BITS == 64
  if( src->unit[1] != value )
    high = 1;
#else
  if( src->unit[3] != value )
    high = 3;
  else if( src->unit[2] != value )
    high = 2;
  else if( src->unit[1] != value )
    high = 1;
#endif
  return high;
}


static inline bnUnit bn128SignMask( const bn128 * const src )
{
  return ( (bnUnitSigned)src->unit[BN_INT128_UNIT_COUNT-1] ) >> (BN_UNIT_BITS-1);
}



////



void bn128Zero( bn128 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn128Set( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src->unit[unitindex];
  return;
}


void bn128Set32( bn128 *dst, uint32_t value )
{
  int unitindex;
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn128Set32Signed( bn128 *dst, int32_t value )
{
  int unitindex;
  bnUnit u;
  u = ( (bnUnitSigned)value ) >> (BN_UNIT_BITS-1);
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = u;
  return;
}


void bn128Set32Shl( bn128 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
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
  if( (unsigned)unitindex < BN_INT128_UNIT_COUNT )
    dst->unit[unitindex] = vl;
  if( (unsigned)++unitindex < BN_INT128_UNIT_COUNT )
    dst->unit[unitindex] = vh;
  return;
}


void bn128Set32SignedShl( bn128 *dst, int32_t value, bnShift shift )
{
  if( value < 0 )
  {
    bn128Set32Shl( dst, -value, shift );
    bn128Neg( dst );
  }
  else
    bn128Set32Shl( dst, value, shift );
  return;
}


#define BN_IEEE754DBL_MANTISSA_BITS (52)
#define BN_IEEE754DBL_EXPONENT_BITS (11)
/*
#define BN_SETDBL_DEBUG
*/

void bn128SetDouble( bn128 *dst, double value, bnShift leftshiftbits )
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

  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
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
    if( (unsigned)unitindex < BN_INT128_UNIT_COUNT )
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
    for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = ~dst->unit[unitindex];
    bn128Add32( dst, 1 );
  }

  return;
}


double bn128GetDouble( bn128 *dst, bnShift rightshiftbits )
{
  int exponent, sign, msb;
  uint64_t mantissa;
  union
  {
    double d;
    uint64_t u;
  } dblunion;
  bn128 value;

  sign = 0;
  if( ( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn128SetNeg( &value, dst );
    sign = 1;
  }
  else
    bn128Set( &value, dst );

  msb = bn128GetIndexMSB( &value );
  if( msb < 0 )
    return ( sign ? -0.0 : 0.0 );
  mantissa = bn128Extract64( &value, msb-64 );
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


void bn128Add32( bn128 *dst, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "addq %1,0(%0)\n"
    "jnc 1f\n"
    "addq $1,8(%0)\n"
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn128Sub32( bn128 *dst, uint32_t value )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "subq %1,0(%0)\n"
    "jnc 1f\n"
    "subq $1,8(%0)\n"
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn128Add32Signed( bn128 *dst, int32_t value )
{
  if( value < 0 )
    bn128Sub32( dst, -value );
  else
    bn128Add32( dst, value );
  return;
}


void bn128Sub32Signed( bn128 *dst, int32_t value )
{
  if( value < 0 )
    bn128Add32( dst, -value );
  else
    bn128Sub32( dst, value );
  return;
}


void bn128Add32Shl( bn128 *dst, uint32_t value, bnShift shift )
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
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "addq %1,8(%0)\n"
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
        "1:\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "addl %1,8(%0)\n"
        "adcl %2,12(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 3:
      __asm__ __volatile__(
        "addl %1,12(%0)\n"
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


void bn128Sub32Shl( bn128 *dst, uint32_t value, bnShift shift )
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
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 1:
      __asm__ __volatile__(
        "subq %1,8(%0)\n"
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
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 2:
      __asm__ __volatile__(
        "subl %1,8(%0)\n"
        "sbbl %2,12(%0)\n"
        :
        : "r" (dst->unit), "r" (vl), "r" (vh)
        : "memory" );
      break;
    case 3:
      __asm__ __volatile__(
        "subl %1,12(%0)\n"
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


void bn128SetBit( bn128 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] |= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


void bn128ClearBit( bn128 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] &= ~( (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) ) );
  return;
}


void bn128FlipBit( bn128 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] ^= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


////


void bn128Add( bn128 *dst, const bn128 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq %%rax,0(%0)\n"
    "adcq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128SetAdd( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128Sub( bn128 *dst, const bn128 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,0(%0)\n"
    "sbbq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128SetSub( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq 0(%2),%%rax\n"
    "sbbq 8(%2),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128SetAddAdd( bn128 *dst, const bn128 * const src, const bn128 * const srcadd0, const bn128 * const srcadd1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit)
    : "%rax", "%rbx", "memory" );
#else
  __asm__ __volatile__(
    "movl %1,%%esi\n"
    "movl %2,%%edi\n"
    "movl 0(%%esi),%%eax\n"
    "movl 4(%%esi),%%ebx\n"
    "movl 8(%%esi),%%ecx\n"
    "movl 12(%%esi),%%edx\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %3,%%esi\n"
    "movl %0,%%edi\n"
    "addl 0(%%esi),%%eax\n"
    "adcl 4(%%esi),%%ebx\n"
    "adcl 8(%%esi),%%ecx\n"
    "adcl 12(%%esi),%%edx\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%ebx,4(%%edi)\n"
    "movl %%ecx,8(%%edi)\n"
    "movl %%edx,12(%%edi)\n"
    :
    : "g" (dst->unit), "g" (src->unit), "g" (srcadd0->unit), "g" (srcadd1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory" );
#endif
  return;
}


void bn128SetAddSub( bn128 *dst, const bn128 * const src, const bn128 * const srcadd, const bn128 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "subq 0(%3),%%rax\n"
    "sbbq 8(%3),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "memory" );
#else
  __asm__ __volatile__(
    "movl %1,%%esi\n"
    "movl %2,%%edi\n"
    "movl 0(%%esi),%%eax\n"
    "movl 4(%%esi),%%ebx\n"
    "movl 8(%%esi),%%ecx\n"
    "movl 12(%%esi),%%edx\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %3,%%esi\n"
    "movl %0,%%edi\n"
    "subl 0(%%esi),%%eax\n"
    "sbbl 4(%%esi),%%ebx\n"
    "sbbl 8(%%esi),%%ecx\n"
    "sbbl 12(%%esi),%%edx\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%ebx,4(%%edi)\n"
    "movl %%ecx,8(%%edi)\n"
    "movl %%edx,12(%%edi)\n"
    :
    : "g" (dst->unit), "g" (src->unit), "g" (srcadd->unit), "g" (srcsub->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory" );
#endif
  return;
}


void bn128SetAddAddSub( bn128 *dst, const bn128 * const src, const bn128 * const srcadd0, const bn128 * const srcadd1, const bn128 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "subq 0(%4),%%rax\n"
    "sbbq 8(%4),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "memory" );
#else
  __asm__ __volatile__(
    "movl %1,%%esi\n"
    "movl %2,%%edi\n"
    "movl 0(%%esi),%%eax\n"
    "movl 4(%%esi),%%ebx\n"
    "movl 8(%%esi),%%ecx\n"
    "movl 12(%%esi),%%edx\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %3,%%edi\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %4,%%esi\n"
    "movl %0,%%edi\n"
    "subl 0(%%esi),%%eax\n"
    "sbbl 4(%%esi),%%ebx\n"
    "sbbl 8(%%esi),%%ecx\n"
    "sbbl 12(%%esi),%%edx\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%ebx,4(%%edi)\n"
    "movl %%ecx,8(%%edi)\n"
    "movl %%edx,12(%%edi)\n"
    :
    : "g" (dst->unit), "g" (src->unit), "g" (srcadd0->unit), "g" (srcadd1->unit), "g" (srcsub->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory" );
#endif
  return;
}


void bn128SetAddAddAddSub( bn128 *dst, const bn128 * const src, const bn128 * const srcadd0, const bn128 * const srcadd1, const bn128 * const srcadd2, const bn128 * const srcsub )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq 0(%2),%%rax\n"
    "adcq 8(%2),%%rbx\n"
    "addq 0(%3),%%rax\n"
    "adcq 8(%3),%%rbx\n"
    "addq 0(%4),%%rax\n"
    "adcq 8(%4),%%rbx\n"
    "subq 0(%5),%%rax\n"
    "sbbq 8(%5),%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (srcadd0->unit), "r" (srcadd1->unit), "r" (srcadd2->unit), "r" (srcsub->unit)
    : "%rax", "%rbx", "memory" );
#else
  __asm__ __volatile__(
    "movl %1,%%esi\n"
    "movl %2,%%edi\n"
    "movl 0(%%esi),%%eax\n"
    "movl 4(%%esi),%%ebx\n"
    "movl 8(%%esi),%%ecx\n"
    "movl 12(%%esi),%%edx\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %3,%%edi\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %4,%%edi\n"
    "addl 0(%%edi),%%eax\n"
    "adcl 4(%%edi),%%ebx\n"
    "adcl 8(%%edi),%%ecx\n"
    "adcl 12(%%edi),%%edx\n"
    "movl %5,%%esi\n"
    "movl %0,%%edi\n"
    "subl 0(%%esi),%%eax\n"
    "sbbl 4(%%esi),%%ebx\n"
    "sbbl 8(%%esi),%%ecx\n"
    "sbbl 12(%%esi),%%edx\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%ebx,4(%%edi)\n"
    "movl %%ecx,8(%%edi)\n"
    "movl %%edx,12(%%edi)\n"
    :
    : "g" (dst->unit), "g" (src->unit), "g" (srcadd0->unit), "g" (srcadd1->unit), "g" (srcadd2->unit), "g" (srcsub->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory" );
#endif
  return;
}


void bn128Mul32( bn128 *dst, const bn128 * const src, uint32_t value )
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
    "movq %%rbx,8(%0)\n"
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

    "movl %2,%%eax\n"
    "mull 12(%1)\n"
    "addl %%eax,%%ebx\n"
    "movl %%ebx,12(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128Mul32Signed( bn128 *dst, const bn128 * const src, int32_t value )
{
  if( value < 0 )
  {
    bn128Mul32( dst, src, -value );
    bn128Neg( dst );
  }
  else
    bn128Mul32( dst, src, value );
  return;
}


bnUnit bn128Mul32Check( bn128 *dst, const bn128 * const src, uint32_t value )
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
    "movl %%ecx,%0\n"
    : "=g" (overflow)
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return overflow;
}


bnUnit bn128Mul32SignedCheck( bn128 *dst, const bn128 * const src, int32_t value )
{
  bnUnit ret;
  if( value < 0 )
  {
    ret = bn128Mul32Check( dst, src, -value );
    bn128Neg( dst );
  }
  else
    ret = bn128Mul32Check( dst, src, value );
  return ret;
}


void bn128Mul( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    /* s0 */
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
    /* a[1]*b[0] */
    "movq 8(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    /* add sum */
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rbx", "%rdx", "memory" );
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
    /* a[0]*b[2] */
    "movl 0(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[1]*b[1] */
    "movl 4(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[2]*b[0] */
    "movl 8(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,8(%%eax)\n"

    /* s3 */
    /* a[0]*b[3] */
    "movl 0(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[1]*b[2] */
    "movl 4(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[2]*b[1] */
    "movl 8(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[3]*b[0] */
    "movl 12(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,12(%%eax)\n"
    :
    : "m" (dst), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return;
}


bnUnit bn128MulCheck( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int highsum;
  bn128Mul( dst, src0, src1 );
  highsum = bn128HighNonMatch( src0, 0 ) + bn128HighNonMatch( src1, 0 );
  return ( highsum >= BN_INT128_UNIT_COUNT );
}


bnUnit bn128MulSignedCheck( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int highsum;
  bn128Mul( dst, src0, src1 );
  highsum = bn128HighNonMatch( src0, bn128SignMask( src0 ) ) + bn128HighNonMatch( src1, bn128SignMask( src1 ) );
  return ( highsum >= BN_INT128_UNIT_COUNT );
}


void bn128MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask )
{
#if BN_UNIT_BITS == 64 && defined(BN128_FULL_MUL)
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
    "adcq $0,%%r8\n"
    /* a[1]*b[0] */
    "movq 8(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,8(%0)\n"

    /* s2 */
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"
    "movq %%r8,24(%0)\n"
    :
    : "r" (result), "r" (src0), "r" (src1)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );
#elif BN_UNIT_BITS == 64
  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testl $1,%3\n"
    "jz 1f\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq 0(%2)\n"
    "movq %%rax,0(%0)\n"
    "addq %%rdx,%%rbx\n"
    "1:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testl $2,%3\n"
    "jz 1f\n"
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
    "1:\n"

    /* s2 */
    "testl $0xC,%3\n"
    "jz 1f\n"
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"
    "movq %%r8,24(%0)\n"
    "1:\n"
    :
    : "r" (result), "r" (src0), "r" (src1), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );
#else
  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%3\n"
    "jz 1f\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull 0(%2)\n"
    "movl %0,%%edi\n"
    "movl %%eax,0(%%edi)\n"
    "addl %%edx,%%ebx\n"
    "1:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    "testl $2,%3\n"
    "jz 1f\n"
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
    "1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%3\n"
    "1:\n"
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
    "1:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%3\n"
    "jz 1f\n"
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
    "1:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%3\n"
    "jz 1f\n"
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
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,16(%%eax)\n"
    "1:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%3\n"
    "jz 1f\n"
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
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,20(%%eax)\n"
    "1:\n"

    /* s6 */
    "testl $0xC0,%3\n"
    "jz 1f\n"
    /* a[3]*b[3] */
    "movl 12(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,24(%%eax)\n"
    "movl %%ebx,28(%%eax)\n"
    "1:\n"
    :
    : "m" (result), "r" (src0), "r" (src1), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return;
}


static inline void bn128SubHigh( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,16(%0)\n"
    "sbbq %%rbx,24(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,16(%0)\n"
    "sbbl %%ebx,20(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,24(%0)\n"
    "sbbl %%edx,28(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


static inline void bn128SubHighTwice( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,16(%0)\n"
    "sbbq %%rbx,24(%0)\n"
    "subq %%rax,16(%0)\n"
    "sbbq %%rbx,24(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,16(%0)\n"
    "sbbl %%ebx,20(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,24(%0)\n"
    "sbbl %%edx,28(%0)\n"
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,16(%0)\n"
    "sbbl %%ebx,20(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,24(%0)\n"
    "sbbl %%edx,28(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128MulSignedExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask )
{
#if BN_UNIT_BITS == 64 && defined(BN128_FULL_MUL)
  __asm__ __volatile__(

    /* s0 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq 0(%2)\n"
    "movq 8(%1),%%r9\n"
    "movq 8(%2),%%r10\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[1] */
    "movq 0(%1),%%rax\n"
    "mulq %%r10\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[0] */
    "movq %%r9,%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    "movq %%rbx,8(%0)\n"

    /* s2 */
    /* a[1]*b[1] */
    "movq %%r9,%%rax\n"
    "mulq %%r10\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"

    /* Sub high [,,%rcx,%r8]  */
    "testq %%r9,%%r9\n"
    "jns 1f\n"
    "subq 0(%2),%%rcx\n"
    "sbbq %%r10,%%r8\n"
    "1:\n"
    "testq %%r10,%%r10\n"
    "jns 1f\n"
    "subq 0(%1),%%rcx\n"
    "sbbq %%r9,%%r8\n"
    "1:\n"

    /* Store high */
    "movq %%rcx,16(%0)\n"
    "movq %%r8,24(%0)\n"
    :
    : "r" (result), "r" (src0), "r" (src1)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9", "%r10", "memory" );
#else
  bn128MulExtended( result, src0, src1, unitmask );
  if( src0[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn128SubHigh( result, src1 );
  if( src1[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
     bn128SubHigh( result, src0 );
#endif
  return;
}


void bn128CopyExtendedShift( bn128 *dst, bnUnit *result, bnShift shiftbits )
{
#if BN_UNIT_BITS == 64 && 0
  __asm__ __volatile__(
    "movq %2,%%rax\n"
    "movl %%eax,%%edx\n"
    "sarl $6,%%eax\n"
    "leaq (%1,%%rax,8),%%rbx\n"
    "andl $63,%%edx\n"
    "movq (%%rbx),%%r8\n"
    "movq 8(%%rbx),%%r9\n"
    "jnz 1f\n"

    "testq %%rax,%%rax\n"
    "je 2f\n"
    "cmpq $0,-8(%%rbx)\n"
    "jns 2f\n"
    "addq $1,%%r8\n"
    "jnc 2f\n"
    "addq $1,%%r9\n"
    "jmp 2f\n"

    ".p2align 4,,15\n"
    "1:\n"
    "movl $64,%%eax\n"
    "movq %%r9,%%r10\n"
    "subl %%edx,%%eax\n"
    "movq 16(%%rbx),%%r11\n"
    "movl %%eax,%%ecx\n"
    "movq %%r8,%%r12\n"
    "shlq %%cl,%%r10\n"
    "shlq %%cl,%%r11\n"
    "movq $1,%%rax\n"
    "movl %%edx,%%ecx\n"
    "shrq %%cl,%%r8\n"
    "shrq %%cl,%%r9\n"
    "subl $1,%%ecx\n"
    "orq %%r10,%%r8\n"
    "orq %%r11,%%r9\n"
    "shlq %%cl,%%rax\n"
    "testq %%rax,%%r12\n"
    "jz 2f\n"
    "addq $1,%%r8\n"
    "jnc 2f\n"
    "addq $1,%%r9\n"

    "2:\n"
    "movq %%r8,(%0)\n"
    "movq %%r9,8(%0)\n"
    :
    : "r" (dst->unit), "r" (result), "r" (shiftbits)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11", "%r12", "memory" );
#else
  bnUnit offset;
  char shift;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn128Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn128Add32( dst, 1 );
  }
  else
  {
    bn128CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn128Add32( dst, 1 );
  }
#endif
  return;
}


void bn128MulShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shiftbits )
{
  bnUnit result[BN_INT128_UNIT_COUNT*2];

#ifdef BN128_FULL_MUL
  bn128MulExtended( result, src0->unit, src1->unit, ~0x0 );
#else
  bnShift highunit, lowunit, unitmask;
  highunit = ( 128 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );
  bn128MulExtended( result, src0->unit, src1->unit, unitmask );
#endif

  bn128CopyExtendedShift( dst, result, shiftbits );

  return;
}


void bn128MulSignedShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shiftbits )
{
  bnUnit result[BN_INT128_UNIT_COUNT*2];

#ifdef BN128_FULL_MUL
  bn128MulSignedExtended( result, src0->unit, src1->unit, ~0x0 );
#else
  bnShift highunit, lowunit, unitmask;
  highunit = ( 128 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );
  bn128MulSignedExtended( result, src0->unit, src1->unit, unitmask );
#endif

  bn128CopyExtendedShift( dst, result, shiftbits );

  return;
}


bnUnit bn128MulCheckShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT128_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 128 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn128MulExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn128HighNonMatch( src0, 0 ) + bn128HighNonMatch( src1, 0 );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT128_UNIT_COUNT );
    bn128Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn128Add32( dst, 1 );
  }
  else
  {
    overflow = 0;
    if( ( highsum >= offset+BN_INT128_UNIT_COUNT+1 ) || ( result[offset+BN_INT128_UNIT_COUNT] >> shift ) )
      overflow = 1;
    bn128CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn128Add32( dst, 1 );
  }

  return overflow;
}


bnUnit bn128MulSignedCheckShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT128_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 128 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn128MulSignedExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn128HighNonMatch( src0, bn128SignMask( src0 ) ) + bn128HighNonMatch( src1, bn128SignMask( src1 ) );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT128_UNIT_COUNT );
    bn128Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn128Add32( dst, 1 );
  }
  else
  {
    if( highsum >= offset+BN_INT128_UNIT_COUNT+1 )
      overflow = 1;
    else
    {
      overflow = result[offset+BN_INT128_UNIT_COUNT];
      if( src0->unit[BN_INT128_UNIT_COUNT-1] ^ src1->unit[BN_INT128_UNIT_COUNT-1] )
        overflow = ~overflow;
      overflow >>= shift;
    }
    bn128CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn128Add32( dst, 1 );
  }

  return overflow;
}


void bn128SquareShr( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  bnUnit result[BN_INT128_UNIT_COUNT*2];

#if BN_UNIT_BITS == 64 && defined(BN128_FULL_MUL)

  __asm__ __volatile__(

    /* s0 */
    "xorq %%rcx,%%rcx\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq %%rax\n"
    "movq 8(%1),%%r9\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    /* a[0]*b[1] */
    "movq 0(%1),%%rax\n"
    "mulq %%r9\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[0] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    "movq %%rbx,8(%0)\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    /* a[1]*b[1] */
    "movq %%r9,%%rax\n"
    "mulq %%r9\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"

    /* Sub high [,,%rcx,%r8]  */
    "testq %%r9,%%r9\n"
    "jns 1f\n"
    "movq 0(%1),%%rax\n"
    "subq %%rax,%%rcx\n"
    "sbbq %%r9,%%r8\n"
    "subq %%rax,%%rcx\n"
    "sbbq %%r9,%%r8\n"
    "1:\n"

    /* Store high */
    "movq %%rcx,16(%0)\n"
    "movq %%r8,24(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "%r9", "memory" );

#elif BN_UNIT_BITS == 64

  bnShift highunit, lowunit, unitmask;

  highunit = ( 128 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testl $1,%2\n"
    "jz 1f\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq %%rax\n"
    "movq %%rax,0(%0)\n"
    "addq %%rdx,%%rbx\n"
    "1:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testl $2,%2\n"
    "jz 1f\n"
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
    "1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testl $0xC,%2\n"
    "jz 1f\n"
    /* a[1]*b[1] */
    "movq 8(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,16(%0)\n"
    "movq %%r8,24(%0)\n"
    "1:\n"
    :
    : "r" (result), "r" (src), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );

  if( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn128SubHighTwice( result, src->unit );

#else

  bnShift highunit, lowunit, unitmask;

  highunit = ( 128 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%2\n"
    "jz 1f\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull %%eax\n"
    "movl %%eax,0(%0)\n"
    "addl %%edx,%%ebx\n"
    "1:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    "testl $2,%2\n"
    "jz 1f\n"
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
    "1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%2\n"
    "jz 1f\n"
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
    "1:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%2\n"
    "jz 1f\n"
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
    "1:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%2\n"
    "jz 1f\n"
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
    "1:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%2\n"
    "jz 1f\n"
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
    "1:\n"

    /* s6 */
    "testl $0xC0,%2\n"
    "jz 1f\n"
    /* a[3]*b[3] */
    "movl 12(%1),%%eax\n"
    "mull %%eax\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    /* add sum */
    "movl %%edi,24(%0)\n"
    "movl %%ebx,32(%0)\n"
    "1:\n"
    :
    : "r" (result), "r" (src), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );

  if( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn128SubHighTwice( result, src->unit );

#endif

  bn128CopyExtendedShift( dst, result, shiftbits );

  return;
}



////



void bn128Div32( bn128 *dst, uint32_t divisor, uint32_t *rem )
{
  bnShift offset, divisormsb;
  bn128 div, quotient, dsthalf, qpart, dpart;
  bn128Set32( &div, divisor );
  bn128Zero( &quotient );
  divisormsb = bn128GetIndexMSB( &div );
  for( ; ; )
  {
    if( bn128CmpLt( dst, &div ) )
      break;
    bn128Set( &dpart, &div );
    bn128Set32( &qpart, 1 );
    bn128SetShr1( &dsthalf, dst );
    offset = bn128GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn128Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn128CmpLe( &dpart, &dsthalf ) )
    {
      bn128Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn128Shl( &qpart, &qpart, offset );
    bn128Sub( dst, &dpart );
    bn128Add( &quotient, &qpart );
  }
  if( rem )
    *rem = dst->unit[0];
  if( bn128CmpEq( dst, &div ) )
  {
    if( rem )
      *rem = 0;
    bn128Add32( &quotient, 1 );
  }
  bn128Set( dst, &quotient );
  return;
}


void bn128Div32Signed( bn128 *dst, int32_t divisor, int32_t *rem )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn128Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn128Div32( dst, (uint32_t)divisor, (uint32_t *)rem );
  if( sign )
  {
    bn128Neg( dst );
    if( rem )
      *rem = -*rem;
  }
  return;
}


void bn128Div32Round( bn128 *dst, uint32_t divisor )
{
  uint32_t rem;
  bn128Div32( dst, divisor, &rem );
  if( ( rem << 1 ) >= divisor )
    bn128Add32( dst, 1 );
  return;
}


void bn128Div32RoundSigned( bn128 *dst, int32_t divisor )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn128Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn128Div32Round( dst, (uint32_t)divisor );
  if( sign )
    bn128Neg( dst );
  return;
}


void bn128Div( bn128 *dst, const bn128 * const divisor, bn128 *rem )
{
  bnShift offset, remzero, divisormsb;
  bn128 quotient, dsthalf, qpart, dpart;
  bn128Zero( &quotient );
  divisormsb = bn128GetIndexMSB( divisor );
  for( ; ; )
  {
    if( bn128CmpLt( dst, divisor ) )
      break;
    bn128Set( &dpart, divisor );
    bn128Set32( &qpart, 1 );
    bn128SetShr1( &dsthalf, dst );
    offset = bn128GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn128Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn128CmpLe( &dpart, &dsthalf ) )
    {
      bn128Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn128Shl( &qpart, &qpart, offset );
    bn128Sub( dst, &dpart );
    bn128Add( &quotient, &qpart );
  }
  remzero = 0;
  if( bn128CmpEq( dst, divisor ) )
  {
    bn128Add32( &quotient, 1 );
    remzero = 1;
  }
  if( rem )
  {
    if( remzero )
      bn128Zero( rem );
    else
      bn128Set( rem, dst );
  }
  bn128Set( dst, &quotient );
  return;
}


void bn128DivSigned( bn128 *dst, const bn128 * const divisor, bn128 *rem )
{
  int sign;
  bn128 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn128Neg( dst );
  }
  if( divisor->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn128SetNeg( &divisorabs, divisor );
    bn128Div( dst, &divisorabs, rem );
  }
  else
    bn128Div( dst, divisor, rem );
  if( sign )
  {
    bn128Neg( dst );
    if( rem )
      bn128Neg( rem );
  }
  return;
}


void bn128DivRound( bn128 *dst, const bn128 * const divisor )
{
  bn128 rem;
  bn128Div( dst, divisor, &rem );
  bn128Shl1( &rem );
  if( bn128CmpGe( &rem, divisor ) )
    bn128Add32( dst, 1 );
  return;
}


void bn128DivRoundSigned( bn128 *dst, const bn128 * const divisor )
{
  int sign;
  bn128 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn128Neg( dst );
  }
  if( divisor->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn128SetNeg( &divisorabs, divisor );
    bn128DivRound( dst, &divisorabs );
  }
  else
    bn128DivRound( dst, divisor );
  if( sign )
    bn128Neg( dst );
  return;
}


void bn128DivShl( bn128 *dst, const bn128 * const divisor, bn128 *rem, bnShift shift )
{
  bnShift fracshift;
  bn128 basedst, fracpart, qpart;
  if( !( rem ) )
    rem = &fracpart;
  bn128Set( &basedst, dst );
  bn128Div( &basedst, divisor, rem );
  bn128Shl( dst, &basedst, shift );
  for( fracshift = shift-1 ; fracshift >= 0 ; fracshift-- )
  {
    if( bn128CmpZero( rem ) )
      break;
    bn128Shl1( rem );
    if( bn128CmpGe( rem, divisor ) )
    {
      bn128Sub( rem, divisor );
      bn128Set32Shl( &qpart, 1, fracshift );
      bn128Or( dst, &qpart );
    }
  }
  return;
}


void bn128DivSignedShl( bn128 *dst, const bn128 * const divisor, bn128 *rem, bnShift shift )
{
  int sign;
  bn128 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn128Neg( dst );
  }
  if( divisor->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn128SetNeg( &divisorabs, divisor );
    bn128DivShl( dst, &divisorabs, rem, shift );
  }
  else
    bn128DivShl( dst, divisor, rem, shift );
  if( sign )
  {
    bn128Neg( dst );
    if( rem )
      bn128Neg( rem );
  }
  return;
}


void bn128DivRoundShl( bn128 *dst, const bn128 * const divisor, bnShift shift )
{
  bn128 rem;
  bn128DivShl( dst, divisor, &rem, shift );
  bn128Shl1( &rem );
  if( bn128CmpGe( &rem, divisor ) )
    bn128Add32( dst, 1 );
  return;
}


void bn128DivRoundSignedShl( bn128 *dst, const bn128 * const divisor, bnShift shift )
{
  int sign;
  bn128 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn128Neg( dst );
  }
  if( divisor->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn128SetNeg( &divisorabs, divisor );
    bn128DivRoundShl( dst, &divisorabs, shift );
  }
  else
    bn128DivRoundShl( dst, divisor, shift );
  if( sign )
    bn128Neg( dst );
  return;
}



////



void bn128Or( bn128 *dst, const bn128 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rcx\n"
    "orq %%rax,0(%0)\n"
    "orq %%rcx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rcx", "memory" );
#else
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] |= src->unit[unitindex];
#endif
  return;
}


void bn128SetOr( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rcx\n"
    "orq 0(%2),%%rax\n"
    "orq 8(%2),%%rcx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rcx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%rax", "%rcx", "memory" );
#else
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] | src1->unit[unitindex];
#endif
  return;
}


void bn128Nor( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( dst->unit[unitindex] | src->unit[unitindex] );
  return;
}


void bn128SetNor( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] | src1->unit[unitindex] );
  return;
}


void bn128And( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] &= src->unit[unitindex];
  return;
}


void bn128SetAnd( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] & src1->unit[unitindex];
  return;
}


void bn128Nand( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] & dst->unit[unitindex] );
  return;
}


void bn128SetNand( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] & src1->unit[unitindex] );
  return;
}


void bn128Xor( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] ^= src->unit[unitindex];
  return;
}


void bn128SetXor( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] ^ src1->unit[unitindex];
  return;
}


void bn128Nxor( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] ^ dst->unit[unitindex] );
  return;
}


void bn128SetNxor( bn128 *dst, const bn128 * const src0, const bn128 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] ^ src1->unit[unitindex] );
  return;
}


void bn128Not( bn128 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  return;
}


void bn128SetNot( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  return;
}


void bn128Neg( bn128 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  bn128Add32( dst, 1 );
  return;
}


void bn128SetNeg( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  bn128Add32( dst, 1 );
  return;
}


void bn128Shl( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[-offset];
  if( !( shift ) )
  {
    limit = offset;
    for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= limit ; unitindex-- )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex >= 0 ; unitindex-- )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = offset;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = BN_INT128_UNIT_COUNT-1;
    if( offset < BN_INT128_UNIT_COUNT )
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


void bn128Shr( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT128_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = BN_INT128_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT128_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift );
      unitindex++;
    }
    for( ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn128Sal( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  bn128Shl( dst, src, shiftbits );
  return;
}


void bn128Sar( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  if( !( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn128Shr( dst, src, shiftbits );
    return;
  }
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT128_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = -1;
  }
  else
  {
    limit = BN_INT128_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT128_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( (bnUnit)-1 << shiftinv );;
      unitindex++;
    }
    for( ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn128ShrRound( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn128ExtractBit( src, shiftbits-1 );
  bn128Shr( dst, src, shiftbits );
  if( bit )
    bn128Add32( dst, 1 );
  return;
}


void bn128SarRound( bn128 *dst, const bn128 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn128ExtractBit( src, shiftbits-1 );
  bn128Sar( dst, src, shiftbits );
  if( bit )
    bn128Add32( dst, 1 );
  return;
}


void bn128Shl1( bn128 *dst )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%0),%%rax\n"
    "movq 8(%0),%%rbx\n"
    "addq %%rax,0(%0)\n"
    "adcq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit)
    : "%rax", "%rbx", "memory" );
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
    :
    : "r" (dst->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128SetShl1( bn128 *dst, const bn128 * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "addq %%rax,%%rax\n"
    "adcq %%rbx,%%rbx\n"
    "movq %%rax,0(%0)\n"
    "movq %%rbx,8(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%rax", "%rbx", "memory" );
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn128Shr1( bn128 *dst )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 );
#else
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 );
#endif
  return;
}


void bn128SetShr1( bn128 *dst, const bn128 * const src )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 );
#else
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 );
#endif
  return;
}



////



int bn128CmpZero( const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 0;
  }
  return 1;
}

int bn128CmpNotZero( const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn128CmpEq( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn128CmpNeq( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn128CmpGt( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn128CmpGe( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn128CmpLt( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn128CmpLe( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn128CmpSignedGt( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT128_UNIT_COUNT-1] ^ src->unit[BN_INT128_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT128_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn128CmpSignedGe( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT128_UNIT_COUNT-1] ^ src->unit[BN_INT128_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT128_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn128CmpSignedLt( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT128_UNIT_COUNT-1] ^ src->unit[BN_INT128_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT128_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn128CmpSignedLe( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT128_UNIT_COUNT-1] ^ src->unit[BN_INT128_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT128_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn128CmpPositive( const bn128 * const src )
{
  return ( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) == 0;
}


int bn128CmpNegative( const bn128 * const src )
{
  return ( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) != 0;
}


int bn128CmpEqOrZero( bn128 *dst, const bn128 * const src )
{
  int unitindex;
  bnUnit accum, diffaccum;
  accum = 0;
  diffaccum = 0;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
  {
    diffaccum |= dst->unit[unitindex] - src->unit[unitindex];
    accum |= dst->unit[unitindex];
  }
  return ( ( accum & diffaccum ) == 0 );
}


int bn128CmpPart( bn128 *dst, const bn128 * const src, uint32_t bits )
{
  int unitindex, unitcount, partbits;
  bnUnit unitmask;
  unitcount = ( bits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  partbits = bits & (BN_UNIT_BITS-1);
  unitindex = BN_INT128_UNIT_COUNT - unitcount;
  if( partbits )
  {
    unitmask = (bnUnit)-1 << (BN_UNIT_BITS-partbits);
    if( ( dst->unit[unitindex] & unitmask ) != ( src->unit[unitindex] & unitmask ) )
      return 0;
    unitindex++;
  }
  for( ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}



////



int bn128ExtractBit( const bn128 * const src, int bitoffset )
{
  int unitindex, shift;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  value = 0;
  if( (unsigned)unitindex < BN_INT128_UNIT_COUNT )
    value = ( src->unit[unitindex] >> shift ) & 0x1;
  return value;
}


uint32_t bn128Extract32( const bn128 * const src, int bitoffset )
{
  int unitindex, shift, shiftinv;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT128_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 32 ) && ( (unsigned)unitindex+1 < BN_INT128_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
}


uint64_t bn128Extract64( const bn128 * const src, int bitoffset )
{
#if BN_UNIT_BITS == 64
  int unitindex, shift, shiftinv;
  uint64_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT128_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 64 ) && ( (unsigned)unitindex+1 < BN_INT128_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
#else
  return (uint64_t)bn128Extract32( src, bitoffset ) | ( (uint64_t)bn128Extract32( src, bitoffset+32 ) << 32 );
#endif
}


int bn128GetIndexMSB( const bn128 * const src )
{
  int unitindex;
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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


int bn128GetIndexMSZ( const bn128 * const src )
{
  int unitindex;
  bnUnit unit;
  for( unitindex = BN_INT128_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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



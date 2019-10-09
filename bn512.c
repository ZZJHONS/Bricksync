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
#define BN_XP_SUPPORT_1024 0
#include "bn.h"


/*
Big number fixed point two-complement arithmetics, 512 bits

On 32 bits, this file *MUST* be compiled with -fomit-frame-pointer to free the %ebp register
In fact, don't even think about compiling 32 bits. This is much faster on 64 bits.
*/

#define BN512_PRINT_REPORT



#if BN_UNIT_BITS == 64
 #define PRINT(p,x) printf( p "%016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx\n", (long)(x)->unit[7], (long)(x)->unit[6], (long)(x)->unit[5], (long)(x)->unit[4], (long)(x)->unit[3], (long)(x)->unit[2], (long)(x)->unit[1], (long)(x)->unit[0] )
#else
 #define PRINT(p,x) printf( p "%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x\n", (x)->unit[15], (x)->unit[14], (x)->unit[13], (x)->unit[12], (x)->unit[11], (x)->unit[10], (x)->unit[9], (x)->unit[8], (x)->unit[7], (x)->unit[6], (x)->unit[5], (x)->unit[4], (x)->unit[3], (x)->unit[2], (x)->unit[1], (x)->unit[0] )
#endif



static inline __attribute__((always_inline)) void bn512Copy( bn512 *dst, bnUnit *src )
{
  dst->unit[0] = src[0];
  dst->unit[1] = src[1];
  dst->unit[2] = src[2];
  dst->unit[3] = src[3];
  dst->unit[4] = src[4];
  dst->unit[5] = src[5];
  dst->unit[6] = src[6];
  dst->unit[7] = src[7];
#if BN_UNIT_BITS == 32
  dst->unit[8] = src[8];
  dst->unit[9] = src[9];
  dst->unit[10] = src[10];
  dst->unit[11] = src[11];
  dst->unit[12] = src[12];
  dst->unit[13] = src[13];
  dst->unit[14] = src[14];
  dst->unit[15] = src[15];
#endif
  return;
}

static inline __attribute__((always_inline)) void bn512CopyShift( bn512 *dst, bnUnit *src, bnShift shift )
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
#if BN_UNIT_BITS == 32
  dst->unit[8] = ( src[8] >> shift ) | ( src[9] << shiftinv );
  dst->unit[9] = ( src[9] >> shift ) | ( src[10] << shiftinv );
  dst->unit[10] = ( src[10] >> shift ) | ( src[11] << shiftinv );
  dst->unit[11] = ( src[11] >> shift ) | ( src[12] << shiftinv );
  dst->unit[12] = ( src[12] >> shift ) | ( src[13] << shiftinv );
  dst->unit[13] = ( src[13] >> shift ) | ( src[14] << shiftinv );
  dst->unit[14] = ( src[14] >> shift ) | ( src[15] << shiftinv );
  dst->unit[15] = ( src[15] >> shift ) | ( src[16] << shiftinv );
#endif
  return;
}

static inline __attribute__((always_inline)) int bn512HighNonMatch( const bn512 * const src, bnUnit value )
{
  int high;
  high = 0;
#if BN_UNIT_BITS == 64
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
#else
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
#endif
  return high;
}


static inline bnUnit bn512SignMask( const bn512 * const src )
{
  return ( (bnUnitSigned)src->unit[BN_INT512_UNIT_COUNT-1] ) >> (BN_UNIT_BITS-1);
}



////



void bn512Zero( bn512 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn512Set( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src->unit[unitindex];
  return;
}


void bn512Set32( bn512 *dst, uint32_t value )
{
  int unitindex;
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;
  return;
}


void bn512Set32Signed( bn512 *dst, int32_t value )
{
  int unitindex;
  bnUnit u;
  u = ( (bnUnitSigned)value ) >> (BN_UNIT_BITS-1);
  dst->unit[0] = value;
  for( unitindex = 1 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = u;
  return;
}


void bn512Set32Shl( bn512 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
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
  if( (unsigned)unitindex < BN_INT512_UNIT_COUNT )
    dst->unit[unitindex] = vl;
  if( (unsigned)++unitindex < BN_INT512_UNIT_COUNT )
    dst->unit[unitindex] = vh;
  return;
}


void bn512Set32SignedShl( bn512 *dst, int32_t value, bnShift shift )
{
  if( value < 0 )
  {
    bn512Set32Shl( dst, -value, shift );
    bn512Neg( dst );
  }
  else
    bn512Set32Shl( dst, value, shift );
  return;
}


#define BN_IEEE754DBL_MANTISSA_BITS (52)
#define BN_IEEE754DBL_EXPONENT_BITS (11)

void bn512SetDouble( bn512 *dst, double value, bnShift leftshiftbits )
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

  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
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
    if( (unsigned)unitindex < BN_INT512_UNIT_COUNT )
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
    for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = ~dst->unit[unitindex];
    bn512Add32( dst, 1 );
  }

  return;
}


double bn512GetDouble( bn512 *dst, bnShift rightshiftbits )
{
  int exponent, sign, msb;
  uint64_t mantissa;
  union
  {
    double d;
    uint64_t u;
  } dblunion;
  bn512 value;

  sign = 0;
  if( ( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn512SetNeg( &value, dst );
    sign = 1;
  }
  else
    bn512Set( &value, dst );

  msb = bn512GetIndexMSB( &value );
  if( msb < 0 )
    return ( sign ? -0.0 : 0.0 );
  mantissa = bn512Extract64( &value, msb-64 );
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


void bn512SetBit( bn512 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] |= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


void bn512ClearBit( bn512 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] &= ~( (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) ) );
  return;
}


void bn512FlipBit( bn512 *dst, bnShift shift )
{
  int unitindex;
  unitindex = shift >> BN_UNIT_BIT_SHIFT;
  dst->unit[unitindex] ^= (bnUnit)1 << ( shift & (BN_UNIT_BITS-1) );
  return;
}


////


void bn512Add32( bn512 *dst, uint32_t value )
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn512Sub32( bn512 *dst, uint32_t value )
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
    "1:\n"
    :
    : "r" (dst->unit), "r" (value)
    : "memory" );
#endif
  return;
}


void bn512Add32s( bn512 *dst, int32_t value )
{
  if( value < 0 )
    bn512Sub32( dst, -value );
  else
    bn512Add32( dst, value );
  return;
}


void bn512Sub32s( bn512 *dst, int32_t value )
{
  if( value < 0 )
    bn512Add32( dst, -value );
  else
    bn512Sub32( dst, value );
  return;
}


void bn512Add32Shl( bn512 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bn512 adder;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
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
  if( (unsigned)(unitindex+0) < BN_INT512_UNIT_COUNT )
    adder.unit[unitindex+0] = vl;
  if( (unsigned)(unitindex+1) < BN_INT512_UNIT_COUNT )
    adder.unit[unitindex+1] = vh;
  bn512Add( dst, &adder );
  return;
}


void bn512Sub32Shl( bn512 *dst, uint32_t value, bnShift shift )
{
  int unitindex;
  bn512 adder;
  bnUnit vl, vh;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
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
  if( (unsigned)(unitindex+0) < BN_INT512_UNIT_COUNT )
    adder.unit[unitindex+0] = vl;
  if( (unsigned)(unitindex+1) < BN_INT512_UNIT_COUNT )
    adder.unit[unitindex+1] = vh;
  bn512Sub( dst, &adder );
  return;
}


void bn512Add( bn512 *dst, const bn512 * const src )
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512SetAdd( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512Sub( bn512 *dst, const bn512 * const src )
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512SetSub( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
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
    :
    : "r" (dst->unit), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512SetAddAdd( bn512 *dst, const bn512 * const src, const bn512 * const srcadd0, const bn512 * const srcadd1 )
{
  bn512SetAdd( dst, src, srcadd0 );
  bn512Add( dst, srcadd1 );
  return;
}


void bn512SetAddSub( bn512 *dst, const bn512 * const src, const bn512 * const srcadd, const bn512 * const srcsub )
{
  bn512SetAdd( dst, src, srcadd );
  bn512Sub( dst, srcsub );
  return;
}


void bn512SetAddAddSub( bn512 *dst, const bn512 * const src, const bn512 * const srcadd0, const bn512 * const srcadd1, const bn512 * const srcsub )
{
  bn512SetAdd( dst, src, srcadd0 );
  bn512Add( dst, srcadd1 );
  bn512Sub( dst, srcsub );
  return;
}


void bn512SetAddAddAddSub( bn512 *dst, const bn512 * const src, const bn512 * const srcadd0, const bn512 * const srcadd1, const bn512 * const srcadd2, const bn512 * const srcsub )
{
  bn512SetAdd( dst, src, srcadd0 );
  bn512Add( dst, srcadd1 );
  bn512Add( dst, srcadd2 );
  bn512Sub( dst, srcsub );
  return;
}


void bn512Mul32( bn512 *dst, const bn512 * const src, uint32_t value )
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

    "movq %2,%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rbx\n"
    "movq %%rbx,56(%0)\n"
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

    "movl %2,%%eax\n"
    "mull 60(%1)\n"
    "addl %%eax,%%ebx\n"
    "movl %%ebx,60(%0)\n"
    :
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512Mul32Signed( bn512 *dst, const bn512 * const src, int32_t value )
{
  if( value < 0 )
  {
    bn512Mul32( dst, src, -value );
    bn512Neg( dst );
  }
  else
    bn512Mul32( dst, src, value );
  return;
}


bnUnit bn512Mul32Check( bn512 *dst, const bn512 * const src, uint32_t value )
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
    "movl %%ecx,%0\n"
    : "=g" (overflow)
    : "r" (dst->unit), "r" (src->unit), "r" (value)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return overflow;
}


bnUnit bn512Mul32SignedCheck( bn512 *dst, const bn512 * const src, int32_t value )
{
  bnUnit ret;
  if( value < 0 )
  {
    ret = bn512Mul32Check( dst, src, -value );
    bn512Neg( dst );
  }
  else
    ret = bn512Mul32Check( dst, src, value );
  return ret;
}


void bn512Mul( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
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
    /* a[0]*b[6] */
    "movq 0(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* a[1]*b[5] */
    "movq 8(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* a[2]*b[4] */
    "movq 16(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* a[3]*b[3] */
    "movq 24(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* a[4]*b[2] */
    "movq 32(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* a[5]*b[1] */
    "movq 40(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* a[6]*b[0] */
    "movq 48(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    /* add sum */
    "movq %%r8,48(%0)\n"

    /* s7 */
    /* a[0]*b[7] */
    "movq 0(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[1]*b[6] */
    "movq 8(%1),%%rax\n"
    "mulq 48(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[2]*b[5] */
    "movq 16(%1),%%rax\n"
    "mulq 40(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[3]*b[4] */
    "movq 24(%1),%%rax\n"
    "mulq 32(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[4]*b[3] */
    "movq 32(%1),%%rax\n"
    "mulq 24(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[5]*b[2] */
    "movq 40(%1),%%rax\n"
    "mulq 16(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[6]*b[1] */
    "movq 48(%1),%%rax\n"
    "mulq 8(%2)\n"
    "addq %%rax,%%rbx\n"
    /* a[7]*b[0] */
    "movq 56(%1),%%rax\n"
    "mulq 0(%2)\n"
    "addq %%rax,%%rbx\n"
    /* add sum */
    "movq %%rbx,56(%0)\n"
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
    "xorl %%ecx,%%ecx\n"
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

    /* s7 */
    "xorl %%edi,%%edi\n"
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

    /* s8 */
    "xorl %%ebx,%%ebx\n"
    /* a[0]*b[8] */
    "movl 0(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
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
    /* a[8]*b[0] */
    "movl 32(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,32(%%eax)\n"

    /* s9 */
    "xorl %%ecx,%%ecx\n"
    /* a[0]*b[9] */
    "movl 0(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[8] */
    "movl 4(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
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
    /* a[8]*b[1] */
    "movl 32(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[0] */
    "movl 36(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,36(%%eax)\n"

    /* s10 */
    "xorl %%edi,%%edi\n"
    /* a[0]*b[10] */
    "movl 0(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[9] */
    "movl 4(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[8] */
    "movl 8(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
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
    /* a[8]*b[2] */
    "movl 32(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[1] */
    "movl 36(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[0] */
    "movl 40(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,40(%%eax)\n"

    /* s11 */
    "xorl %%ebx,%%ebx\n"
    /* a[0]*b[11] */
    "movl 0(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[10] */
    "movl 4(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[9] */
    "movl 8(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[8] */
    "movl 12(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
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
    /* a[8]*b[3] */
    "movl 32(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[2] */
    "movl 36(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[1] */
    "movl 40(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[0] */
    "movl 44(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,44(%%eax)\n"

    /* s12 */
    "xorl %%ecx,%%ecx\n"
    /* a[0]*b[12] */
    "movl 0(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[11] */
    "movl 4(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[10] */
    "movl 8(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[9] */
    "movl 12(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[8] */
    "movl 16(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
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
    /* a[8]*b[4] */
    "movl 32(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[3] */
    "movl 36(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[10]*b[2] */
    "movl 40(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[11]*b[1] */
    "movl 44(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[12]*b[0] */
    "movl 48(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,48(%%eax)\n"

    /* s13 */
    "xorl %%edi,%%edi\n"
    /* a[0]*b[13] */
    "movl 0(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[12] */
    "movl 4(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[11] */
    "movl 8(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[10] */
    "movl 12(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[9] */
    "movl 16(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[8] */
    "movl 20(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
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
    /* a[8]*b[5] */
    "movl 32(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[4] */
    "movl 36(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[3] */
    "movl 40(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[2] */
    "movl 44(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[1] */
    "movl 48(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[0] */
    "movl 52(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,52(%%eax)\n"

    /* s14 */
    /* a[0]*b[14] */
    "movl 0(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[1]*b[13] */
    "movl 4(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[2]*b[12] */
    "movl 8(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[3]*b[11] */
    "movl 12(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[4]*b[10] */
    "movl 16(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[5]*b[9] */
    "movl 20(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[6]*b[8] */
    "movl 24(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[7]*b[7] */
    "movl 28(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[8]*b[6] */
    "movl 32(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[9]*b[5] */
    "movl 36(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[10]*b[4] */
    "movl 40(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[11]*b[3] */
    "movl 44(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[12]*b[2] */
    "movl 48(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[13]*b[1] */
    "movl 52(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[14]*b[0] */
    "movl 56(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,56(%%eax)\n"

    /* s15 */
    /* a[0]*b[15] */
    "movl 0(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[1]*b[14] */
    "movl 4(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[2]*b[13] */
    "movl 8(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[3]*b[12] */
    "movl 12(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[4]*b[11] */
    "movl 16(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[5]*b[10] */
    "movl 20(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[6]*b[9] */
    "movl 24(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[7]*b[8] */
    "movl 28(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[8]*b[7] */
    "movl 32(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[9]*b[6] */
    "movl 36(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[10]*b[5] */
    "movl 40(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[11]*b[4] */
    "movl 44(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[12]*b[3] */
    "movl 48(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[13]*b[2] */
    "movl 52(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[14]*b[1] */
    "movl 56(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    /* a[15]*b[0] */
    "movl 60(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,60(%%eax)\n"

    :
    : "m" (dst), "r" (src0->unit), "r" (src1->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );

#endif

  return;
}


bnUnit bn512MulCheck( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int highsum;
  bn512Mul( dst, src0, src1 );
  highsum  = bn512HighNonMatch( src0, 0 ) + bn512HighNonMatch( src1, 0 );
  return ( highsum >= BN_INT512_UNIT_COUNT );
}


bnUnit bn512MulSignedCheck( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int highsum;
  bn512Mul( dst, src0, src1 );
  highsum = bn512HighNonMatch( src0, bn512SignMask( src0 ) ) + bn512HighNonMatch( src1, bn512SignMask( src1 ) );
  return ( highsum >= BN_INT512_UNIT_COUNT );
}


void __attribute__ ((noinline)) bn512MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask )
{

#if BN_UNIT_BITS == 64

  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testq $1,%3\n"
    "jz .mulext512s0\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq 0(%2)\n"
    "movq %%rax,0(%0)\n"
    "movq %%rdx,%%rbx\n"
    ".mulext512s0:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testq $2,%3\n"
    "jz .mulext512s1\n"
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
    ".mulext512s1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testq $4,%3\n"
    "jz .mulext512s2\n"
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
    ".mulext512s2:\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    "testq $8,%3\n"
    "jz .mulext512s3\n"
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
    ".mulext512s3:\n"

    /* s4 */
    "xorq %%r8,%%r8\n"
    "testq $0x10,%3\n"
    "jz .mulext512s4\n"
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
    ".mulext512s4:\n"

    /* s5 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x20,%3\n"
    "jz .mulext512s5\n"
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
    ".mulext512s5:\n"

    /* s6 */
    "xorq %%rcx,%%rcx\n"
    "testq $0x40,%3\n"
    "jz .mulext512s6\n"
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
    ".mulext512s6:\n"

    /* s7 */
    "xorq %%r8,%%r8\n"
    "testq $0x80,%3\n"
    "jz .mulext512s7\n"
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
    ".mulext512s7:\n"

    /* s8 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x100,%3\n"
    "jz .mulext512s8\n"
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
    /* add sum */
    "movq %%rcx,64(%0)\n"
    ".mulext512s8:\n"

    /* s9 */
    "xorq %%rcx,%%rcx\n"
    "testq $0x200,%3\n"
    "jz .mulext512s9\n"
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
    /* add sum */
    "movq %%r8,72(%0)\n"
    ".mulext512s9:\n"

    /* s10 */
    "xorq %%r8,%%r8\n"
    "testq $0x400,%3\n"
    "jz .mulext512s10\n"
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
    /* add sum */
    "movq %%rbx,80(%0)\n"
    ".mulext512s10:\n"

    /* s11 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x800,%3\n"
    "jz .mulext512s11\n"
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
    /* add sum */
    "movq %%rcx,88(%0)\n"
    ".mulext512s11:\n"

    /* s12 */
    "xorq %%rcx,%%rcx\n"
    "testq $0x1000,%3\n"
    "jz .mulext512s12\n"
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
    /* add sum */
    "movq %%r8,96(%0)\n"
    ".mulext512s12:\n"

    /* s13 */
    "xorq %%r8,%%r8\n"
    "testq $0x2000,%3\n"
    "jz .mulext512s13\n"
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
    /* add sum */
    "movq %%rbx,104(%0)\n"
    ".mulext512s13:\n"

    /* s14 */
    "testq $0xC000,%3\n"
    "jz .mulext512s14\n"
    /* a[7]*b[7] */
    "movq 56(%1),%%rax\n"
    "mulq 56(%2)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* add sum */
    "movq %%rcx,112(%0)\n"
    "movq %%r8,120(%0)\n"
    ".mulext512s14:\n"
    :
    : "r" (result), "r" (src0), "r" (src1), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );

#else

  __asm__ __volatile__(

    "xorl %%ebx,%%ebx\n"
    /* s0 */
    "xorl %%ecx,%%ecx\n"
    "testl $1,%3\n"
    "jz .mulext512s0\n"
    /* a[0]*b[0] */
    "movl 0(%1),%%eax\n"
    "mull 0(%2)\n"
    "movl %0,%%edi\n"
    "movl %%eax,0(%%edi)\n"
    "movl %%edx,%%ebx\n"
    ".mulext512s0:\n"

    /* s1 */
    "xorl %%edi,%%edi\n"
    /* a[0]*b[1] */
    "testl $2,%3\n"
    "jz .mulext512s1\n"
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
    ".mulext512s1:\n"

    /* s2 */
    "xorl %%ebx,%%ebx\n"
    "testl $4,%3\n"
    "jz .mulext512s2\n"
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
    ".mulext512s2:\n"

    /* s3 */
    "xorl %%ecx,%%ecx\n"
    "testl $8,%3\n"
    "jz .mulext512s3\n"
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
    ".mulext512s3:\n"

    /* s4 */
    "xorl %%edi,%%edi\n"
    "testl $0x10,%3\n"
    "jz .mulext512s4\n"
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
    ".mulext512s4:\n"

    /* s5 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20,%3\n"
    "jz .mulext512s5\n"
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
    ".mulext512s5:\n"

    /* s6 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x40,%3\n"
    "jz .mulext512s6\n"
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
    ".mulext512s6:\n"

    /* s7 */
    "xorl %%edi,%%edi\n"
    "testl $0x80,%3\n"
    "jz .mulext512s7\n"
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
    ".mulext512s7:\n"

    /* s8 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x100,%3\n"
    "jz .mulext512s8\n"
    /* a[0]*b[8] */
    "movl 0(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
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
    /* a[8]*b[0] */
    "movl 32(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,32(%%eax)\n"
    ".mulext512s8:\n"

    /* s9 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x200,%3\n"
    "jz .mulext512s9\n"
    /* a[0]*b[9] */
    "movl 0(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[8] */
    "movl 4(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
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
    /* a[8]*b[1] */
    "movl 32(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[0] */
    "movl 36(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,36(%%eax)\n"
    ".mulext512s9:\n"

    /* s10 */
    "xorl %%edi,%%edi\n"
    "testl $0x400,%3\n"
    "jz .mulext512s10\n"
    /* a[0]*b[10] */
    "movl 0(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[9] */
    "movl 4(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[8] */
    "movl 8(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
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
    /* a[8]*b[2] */
    "movl 32(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[1] */
    "movl 36(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[0] */
    "movl 40(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,40(%%eax)\n"
    ".mulext512s10:\n"

    /* s11 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x800,%3\n"
    "jz .mulext512s11\n"
    /* a[0]*b[11] */
    "movl 0(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[10] */
    "movl 4(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[9] */
    "movl 8(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[8] */
    "movl 12(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
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
    /* a[8]*b[3] */
    "movl 32(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[2] */
    "movl 36(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[1] */
    "movl 40(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[0] */
    "movl 44(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,44(%%eax)\n"
    ".mulext512s11:\n"

    /* s12 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x1000,%3\n"
    "jz .mulext512s12\n"
    /* a[0]*b[12] */
    "movl 0(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[11] */
    "movl 4(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[10] */
    "movl 8(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[9] */
    "movl 12(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[8] */
    "movl 16(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
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
    /* a[8]*b[4] */
    "movl 32(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[3] */
    "movl 36(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[10]*b[2] */
    "movl 40(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[11]*b[1] */
    "movl 44(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[12]*b[0] */
    "movl 48(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,48(%%eax)\n"
    ".mulext512s12:\n"

    /* s13 */
    "xorl %%edi,%%edi\n"
    "testl $0x2000,%3\n"
    "jz .mulext512s13\n"
    /* a[0]*b[13] */
    "movl 0(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[12] */
    "movl 4(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[11] */
    "movl 8(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[10] */
    "movl 12(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[9] */
    "movl 16(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[8] */
    "movl 20(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
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
    /* a[8]*b[5] */
    "movl 32(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[4] */
    "movl 36(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[3] */
    "movl 40(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[2] */
    "movl 44(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[1] */
    "movl 48(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[0] */
    "movl 52(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,52(%%eax)\n"
    ".mulext512s13:\n"

    /* s14 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x4000,%3\n"
    "jz .mulext512s14\n"
    /* a[0]*b[14] */
    "movl 0(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[13] */
    "movl 4(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[12] */
    "movl 8(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[11] */
    "movl 12(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[10] */
    "movl 16(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[9] */
    "movl 20(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[8] */
    "movl 24(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[7] */
    "movl 28(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[8]*b[6] */
    "movl 32(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[5] */
    "movl 36(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[4] */
    "movl 40(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[3] */
    "movl 44(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[2] */
    "movl 48(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[1] */
    "movl 52(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[0] */
    "movl 56(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,56(%%eax)\n"
    ".mulext512s14:\n"

    /* s15 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x8000,%3\n"
    "jz .mulext512s15\n"
    /* a[0]*b[15] */
    "movl 0(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[1]*b[14] */
    "movl 4(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[2]*b[13] */
    "movl 8(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[3]*b[12] */
    "movl 12(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[11] */
    "movl 16(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[10] */
    "movl 20(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[9] */
    "movl 24(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[8] */
    "movl 28(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[8]*b[7] */
    "movl 32(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[6] */
    "movl 36(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[10]*b[5] */
    "movl 40(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[11]*b[4] */
    "movl 44(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[12]*b[3] */
    "movl 48(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[13]*b[2] */
    "movl 52(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[14]*b[1] */
    "movl 56(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[15]*b[0] */
    "movl 60(%1),%%eax\n"
    "mull 0(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,60(%%eax)\n"
    ".mulext512s15:\n"

    /* s16 */
    "xorl %%edi,%%edi\n"
    "testl $0x10000,%3\n"
    "jz .mulext512s16\n"
    /* a[1]*b[15] */
    "movl 4(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[14] */
    "movl 8(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[13] */
    "movl 12(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[12] */
    "movl 16(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[11] */
    "movl 20(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[10] */
    "movl 24(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[9] */
    "movl 28(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[8]*b[8] */
    "movl 32(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[7] */
    "movl 36(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[6] */
    "movl 40(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[5] */
    "movl 44(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[4] */
    "movl 48(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[3] */
    "movl 52(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[2] */
    "movl 56(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[1] */
    "movl 60(%1),%%eax\n"
    "mull 4(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,64(%%eax)\n"
    ".mulext512s16:\n"

    /* s17 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x20000,%3\n"
    "jz .mulext512s17\n"
    /* a[2]*b[15] */
    "movl 8(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[14] */
    "movl 12(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[13] */
    "movl 16(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[12] */
    "movl 20(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[11] */
    "movl 24(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[10] */
    "movl 28(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[8]*b[9] */
    "movl 32(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[8] */
    "movl 36(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[7] */
    "movl 40(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[6] */
    "movl 44(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[5] */
    "movl 48(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[4] */
    "movl 52(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[3] */
    "movl 56(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[2] */
    "movl 60(%1),%%eax\n"
    "mull 8(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,68(%%eax)\n"
    ".mulext512s17:\n"

    /* s18 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x40000,%3\n"
    "jz .mulext512s18\n"
    /* a[3]*b[15] */
    "movl 12(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[4]*b[14] */
    "movl 16(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[5]*b[13] */
    "movl 20(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[6]*b[12] */
    "movl 24(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[11] */
    "movl 28(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[8]*b[10] */
    "movl 32(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[9] */
    "movl 36(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[10]*b[8] */
    "movl 40(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[11]*b[7] */
    "movl 44(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[12]*b[6] */
    "movl 48(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[13]*b[5] */
    "movl 52(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[14]*b[4] */
    "movl 56(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[15]*b[3] */
    "movl 60(%1),%%eax\n"
    "mull 12(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,72(%%eax)\n"
    ".mulext512s18:\n"

    /* s19 */
    "xorl %%edi,%%edi\n"
    "testl $0x80000,%3\n"
    "jz .mulext512s19\n"
    /* a[4]*b[15] */
    "movl 16(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[14] */
    "movl 20(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[13] */
    "movl 24(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[12] */
    "movl 28(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[8]*b[11] */
    "movl 32(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[10] */
    "movl 36(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[9] */
    "movl 40(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[8] */
    "movl 44(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[7] */
    "movl 48(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[6] */
    "movl 52(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[5] */
    "movl 56(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[4] */
    "movl 60(%1),%%eax\n"
    "mull 16(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,76(%%eax)\n"
    ".mulext512s19:\n"

    /* s20 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x100000,%3\n"
    "jz .mulext512s20\n"
    /* a[5]*b[15] */
    "movl 20(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[14] */
    "movl 24(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[13] */
    "movl 28(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[8]*b[12] */
    "movl 32(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[11] */
    "movl 36(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[10] */
    "movl 40(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[9] */
    "movl 44(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[8] */
    "movl 48(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[7] */
    "movl 52(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[6] */
    "movl 56(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[5] */
    "movl 60(%1),%%eax\n"
    "mull 20(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,80(%%eax)\n"
    ".mulext512s20:\n"

    /* s21 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x200000,%3\n"
    "jz .mulext512s21\n"
    /* a[6]*b[15] */
    "movl 24(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[7]*b[14] */
    "movl 28(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[8]*b[13] */
    "movl 32(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[9]*b[12] */
    "movl 36(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[10]*b[11] */
    "movl 40(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[11]*b[10] */
    "movl 44(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[12]*b[9] */
    "movl 48(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[13]*b[8] */
    "movl 52(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[14]*b[7] */
    "movl 56(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[15]*b[6] */
    "movl 60(%1),%%eax\n"
    "mull 24(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,84(%%eax)\n"
    ".mulext512s21:\n"

    /* s22 */
    "xorl %%edi,%%edi\n"
    "testl $0x400000,%3\n"
    "jz .mulext512s22\n"
    /* a[7]*b[15] */
    "movl 28(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[8]*b[14] */
    "movl 32(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[13] */
    "movl 36(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[12] */
    "movl 40(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[11] */
    "movl 44(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[10] */
    "movl 48(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[9] */
    "movl 52(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[8] */
    "movl 56(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[7] */
    "movl 60(%1),%%eax\n"
    "mull 28(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,88(%%eax)\n"
    ".mulext512s22:\n"

    /* s23 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x800000,%3\n"
    "jz .mulext512s23\n"
    /* a[8]*b[15] */
    "movl 32(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[14] */
    "movl 36(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[13] */
    "movl 40(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[12] */
    "movl 44(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[11] */
    "movl 48(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[10] */
    "movl 52(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[9] */
    "movl 56(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[8] */
    "movl 60(%1),%%eax\n"
    "mull 32(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,92(%%eax)\n"
    ".mulext512s23:\n"

    /* s24 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x1000000,%3\n"
    "jz .mulext512s24\n"
    /* a[9]*b[15] */
    "movl 36(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[10]*b[14] */
    "movl 40(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[11]*b[13] */
    "movl 44(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[12]*b[12] */
    "movl 48(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[13]*b[11] */
    "movl 52(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[14]*b[10] */
    "movl 56(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[15]*b[9] */
    "movl 60(%1),%%eax\n"
    "mull 36(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,96(%%eax)\n"
    ".mulext512s24:\n"

    /* s25 */
    "xorl %%edi,%%edi\n"
    "testl $0x2000000,%3\n"
    "jz .mulext512s25\n"
    /* a[10]*b[15] */
    "movl 40(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[14] */
    "movl 44(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[13] */
    "movl 48(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[12] */
    "movl 52(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[11] */
    "movl 56(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[10] */
    "movl 60(%1),%%eax\n"
    "mull 40(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,100(%%eax)\n"
    ".mulext512s25:\n"

    /* s26 */
    "xorl %%ebx,%%ebx\n"
    "testl $0x4000000,%3\n"
    "jz .mulext512s26\n"
    /* a[11]*b[15] */
    "movl 44(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[14] */
    "movl 48(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[13] */
    "movl 52(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[12] */
    "movl 56(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[11] */
    "movl 60(%1),%%eax\n"
    "mull 44(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,104(%%eax)\n"
    ".mulext512s26:\n"

    /* s27 */
    "xorl %%ecx,%%ecx\n"
    "testl $0x8000000,%3\n"
    "jz .mulext512s27\n"
    /* a[12]*b[15] */
    "movl 48(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[13]*b[14] */
    "movl 52(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[14]*b[13] */
    "movl 56(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* a[15]*b[12] */
    "movl 60(%1),%%eax\n"
    "mull 48(%2)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%ecx\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,108(%%eax)\n"
    ".mulext512s27:\n"

    /* s28 */
    "xorl %%edi,%%edi\n"
    "testl $0x10000000,%3\n"
    "jz .mulext512s28\n"
    /* a[13]*b[15] */
    "movl 52(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[14] */
    "movl 56(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[13] */
    "movl 60(%1),%%eax\n"
    "mull 52(%2)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ebx,112(%%eax)\n"
    ".mulext512s28:\n"

    /* s29 */
    "testl $0x20000000,%3\n"
    "jz .mulext512s29\n"
    /* a[14]*b[15] */
    "movl 56(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* a[15]*b[14] */
    "movl 60(%1),%%eax\n"
    "mull 56(%2)\n"
    "addl %%eax,%%ecx\n"
    "adcl %%edx,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%ecx,116(%%eax)\n"
    ".mulext512s29:\n"

    /* s30 */
    "testl $0x40000000,%3\n"
    "jz .mulext512s30\n"
    /* a[15]*b[15] */
    "movl 60(%1),%%eax\n"
    "mull 60(%2)\n"
    "addl %%eax,%%edi\n"
    /* add sum */
    "movl %0,%%eax\n"
    "movl %%edi,120(%%eax)\n"
    ".mulext512s30:\n"

    :
    : "m" (result), "r" (src0), "r" (src1), "m" (unitmask)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );

#endif

  return;
}


static inline void bn512SubHigh( bnUnit *result, const bnUnit * const src )
{
#if BN_UNIT_BITS == 64
  __asm__ __volatile__(
    "movq 0(%1),%%rax\n"
    "movq 8(%1),%%rbx\n"
    "subq %%rax,64(%0)\n"
    "sbbq %%rbx,72(%0)\n"
    "movq 16(%1),%%rcx\n"
    "movq 24(%1),%%rdx\n"
    "sbbq %%rcx,80(%0)\n"
    "sbbq %%rdx,88(%0)\n"
    "movq 32(%1),%%rax\n"
    "movq 40(%1),%%rbx\n"
    "sbbq %%rax,96(%0)\n"
    "sbbq %%rbx,104(%0)\n"
    "movq 48(%1),%%rcx\n"
    "movq 56(%1),%%rdx\n"
    "sbbq %%rcx,112(%0)\n"
    "sbbq %%rdx,120(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%rax", "%rbx", "%rcx", "%rdx", "memory" );
#else
  __asm__ __volatile__(
    "movl 0(%1),%%eax\n"
    "movl 4(%1),%%ebx\n"
    "subl %%eax,64(%0)\n"
    "sbbl %%ebx,68(%0)\n"
    "movl 8(%1),%%ecx\n"
    "movl 12(%1),%%edx\n"
    "sbbl %%ecx,72(%0)\n"
    "sbbl %%edx,76(%0)\n"
    "movl 16(%1),%%eax\n"
    "movl 20(%1),%%ebx\n"
    "sbbl %%eax,80(%0)\n"
    "sbbl %%ebx,84(%0)\n"
    "movl 24(%1),%%ecx\n"
    "movl 28(%1),%%edx\n"
    "sbbl %%ecx,88(%0)\n"
    "sbbl %%edx,92(%0)\n"
    "movl 32(%1),%%eax\n"
    "movl 36(%1),%%ebx\n"
    "sbbl %%eax,96(%0)\n"
    "sbbl %%ebx,100(%0)\n"
    "movl 40(%1),%%ecx\n"
    "movl 44(%1),%%edx\n"
    "sbbl %%ecx,104(%0)\n"
    "sbbl %%edx,108(%0)\n"
    "movl 48(%1),%%eax\n"
    "movl 52(%1),%%ebx\n"
    "sbbl %%eax,112(%0)\n"
    "sbbl %%ebx,116(%0)\n"
    "movl 56(%1),%%ecx\n"
    "movl 60(%1),%%edx\n"
    "sbbl %%ecx,120(%0)\n"
    "sbbl %%edx,124(%0)\n"
    :
    : "r" (result), "r" (src)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


static inline void bn512SubHighTwice( bnUnit *result, const bnUnit * const src )
{
  bn512SubHigh( result, src );
  bn512SubHigh( result, src );
  return;
}


bnUnit __attribute__ ((noinline)) bn512AddMulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask, bnUnit midcarry )
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
    "jz .addmulext512s0\n"
    /* a[0]*b[0] */
    "movq 0(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,0(%1)\n"
    "adcq %%rdx,%%rbx\n"
    ".addmulext512s0:\n"

    "xorq %%rcx,%%rcx\n"
    /* s1 */
    "testq $2,%4\n"
    "jz .addmulext512s1\n"
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
    ".addmulext512s1:\n"

    "xorq %%rbx,%%rbx\n"
    /* s2 */
    "testq $4,%4\n"
    "jz .addmulext512s2\n"
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
    ".addmulext512s2:\n"

    "xorq %%r8,%%r8\n"
    /* s3 */
    "testq $8,%4\n"
    "jz .addmulext512s3\n"
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
    ".addmulext512s3:\n"

    "xorq %%rcx,%%rcx\n"
    /* s4 */
    "testq $0x10,%4\n"
    "jz .addmulext512s4\n"
    /* a[0]*b[4] */
    "movq 0(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
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
    /* a[4]*b[0] */
    "movq 32(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "addq %%rbx,32(%1)\n"
    "adcq $0,%%r8\n"
    "adcq $0,%%rcx\n"
    ".addmulext512s4:\n"

    "xorq %%rbx,%%rbx\n"
    /* s5 */
    "testq $0x20,%4\n"
    "jz .addmulext512s5\n"
    /* a[0]*b[5] */
    "movq 0(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[4] */
    "movq 8(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
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
    /* a[4]*b[1] */
    "movq 32(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[0] */
    "movq 40(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "addq %%r8,40(%1)\n"
    "adcq $0,%%rcx\n"
    "adcq $0,%%rbx\n"
    ".addmulext512s5:\n"

    /* ## Mid carry ## */
    "movq %5,%%r8\n"
    /* s6 */
    "testq $40,%4\n"
    "jz .addmulext512s6\n"
    /* a[0]*b[6] */
    "movq 0(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[5] */
    "movq 8(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[4] */
    "movq 16(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[3] */
    "movq 24(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[2] */
    "movq 32(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[1] */
    "movq 40(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[0] */
    "movq 48(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "addq %%rcx,48(%1)\n"
    "adcq $0,%%rbx\n"
    "adcq $0,%%r8\n"
    ".addmulext512s6:\n"

    "xorq %%rcx,%%rcx\n"
    /* s7 */
    "testq $0x80,%4\n"
    "jz .addmulext512s7\n"
    /* a[0]*b[7] */
    "movq 0(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[6] */
    "movq 8(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[5] */
    "movq 16(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[4] */
    "movq 24(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[3] */
    "movq 32(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[2] */
    "movq 40(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[1] */
    "movq 48(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[0] */
    "movq 56(%2),%%rax\n"
    "mulq 0(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "addq %%rbx,56(%1)\n"
    "adcq $0,%%r8\n"
    "adcq $0,%%rcx\n"
    ".addmulext512s7:\n"

    "xorq %%rbx,%%rbx\n"
    /* s8 */
    "testq $0x100,%4\n"
    "jz .addmulext512s8\n"
    /* a[1]*b[7] */
    "movq 8(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[6] */
    "movq 16(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[5] */
    "movq 24(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[4]*b[4] */
    "movq 32(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[3] */
    "movq 40(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[6]*b[2] */
    "movq 48(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[7]*b[1] */
    "movq 56(%2),%%rax\n"
    "mulq 8(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "addq %%r8,64(%1)\n"
    "adcq $0,%%rcx\n"
    "adcq $0,%%rbx\n"
    ".addmulext512s8:\n"

    "xorq %%r8,%%r8\n"
    /* s9 */
    "testq $0x200,%4\n"
    "jz .addmulext512s9\n"
    /* a[2]*b[7] */
    "movq 16(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[6] */
    "movq 24(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[5] */
    "movq 32(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[4] */
    "movq 40(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[3] */
    "movq 48(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[2] */
    "movq 56(%2),%%rax\n"
    "mulq 16(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "addq %%rcx,72(%1)\n"
    "adcq $0,%%rbx\n"
    "adcq $0,%%r8\n"
    ".addmulext512s9:\n"

    "xorq %%rcx,%%rcx\n"
    /* s10 */
    "testq $0x400,%4\n"
    "jz .addmulext512s10\n"
    /* a[3]*b[7] */
    "movq 24(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[6] */
    "movq 32(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[5] */
    "movq 40(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[4] */
    "movq 48(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[3] */
    "movq 56(%2),%%rax\n"
    "mulq 24(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "addq %%rbx,80(%1)\n"
    "adcq $0,%%r8\n"
    "adcq $0,%%rcx\n"
    ".addmulext512s10:\n"

    "xorq %%rbx,%%rbx\n"
    /* s11 */
    "testq $0x800,%4\n"
    "jz .addmulext512s11\n"
    /* a[4]*b[7] */
    "movq 32(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[6] */
    "movq 40(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[6]*b[5] */
    "movq 48(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* a[7]*b[4] */
    "movq 56(%2),%%rax\n"
    "mulq 32(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "addq %%r8,88(%1)\n"
    "adcq $0,%%rcx\n"
    "adcq $0,%%rbx\n"
    ".addmulext512s11:\n"

    "xorq %%r8,%%r8\n"
    /* s12 */
    "testq $0x1000,%4\n"
    "jz .addmulext512s12\n"
    /* a[5]*b[7] */
    "movq 40(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[6] */
    "movq 48(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[5] */
    "movq 56(%2),%%rax\n"
    "mulq 40(%3)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "addq %%rcx,96(%1)\n"
    "adcq $0,%%rbx\n"
    "adcq $0,%%r8\n"
    ".addmulext512s12:\n"

    "xorq %%rcx,%%rcx\n"
    /* s13 */
    "testq $0x2000,%4\n"
    "jz .addmulext512s13\n"
    /* a[6]*b[7] */
    "movq 48(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[6] */
    "movq 56(%2),%%rax\n"
    "mulq 48(%3)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "addq %%rbx,104(%1)\n"
    "adcq $0,%%r8\n"
    "adcq $0,%%rcx\n"
    ".addmulext512s13:\n"

    /* s14 */
    "testq $0xC000,%4\n"
    "jz .addmulext512s14\n"
    /* a[7]*b[7] */
    "movq 56(%2),%%rax\n"
    "mulq 56(%3)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%0\n"
    "addq %%r8,112(%1)\n"
    "adcq %%rcx,120(%1)\n"
    "adcq $0,%0\n"
    ".addmulext512s14:\n"

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
    "jz .addmulext512s0\n"
    /* a[0]*b[0] */
    "movl 0(%2),%%eax\n"
    "mull 0(%3)\n"
    "movl %1,%%ecx\n"
    "addl %%eax,0(%%ecx)\n"
    "adcl %%edx,%%ebx\n"
    ".addmulext512s0:\n"

    "xorl %%ecx,%%ecx\n"
    /* s1 */
    "testl $2,%4\n"
    "jz .addmulext512s1\n"
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
    ".addmulext512s1:\n"

    /* s2 */
    "testl $4,%4\n"
    "jz .addmulext512s2\n"
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
    ".addmulext512s2:\n"

    "xorl %%edi,%%edi\n"
    /* s3 */
    "testl $8,%4\n"
    "jz .addmulext512s3\n"
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
    ".addmulext512s3:\n"

    "xorl %%ecx,%%ecx\n"
    /* s4 */
    "testl $0x10,%4\n"
    "jz .addmulext512s4\n"
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
    ".addmulext512s4:\n"

    "xorl %%ebx,%%ebx\n"
    /* s5 */
    "testl $0x20,%4\n"
    "jz .addmulext512s5\n"
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
    ".addmulext512s5:\n"

    "xorl %%edi,%%edi\n"
    /* s6 */
    "testl $0x40,%4\n"
    "jz .addmulext512s6\n"
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
    ".addmulext512s6:\n"

    "xorl %%ecx,%%ecx\n"
    /* s7 */
    "testl $0x80,%4\n"
    "jz .addmulext512s7\n"
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
    ".addmulext512s7:\n"

    "xorl %%ebx,%%ebx\n"
    /* s8 */
    "testl $0x100,%4\n"
    "jz .addmulext512s8\n"
    /* a[0]*b[8] */
    "movl 0(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
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
    /* a[8]*b[0] */
    "movl 32(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%ecx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,32(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s8:\n"

    "xorl %%edi,%%edi\n"
    /* s9 */
    "testl $0x200,%4\n"
    "jz .addmulext512s9\n"
    /* a[0]*b[9] */
    "movl 0(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[8] */
    "movl 4(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
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
    /* a[8]*b[1] */
    "movl 32(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[0] */
    "movl 36(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,36(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s9:\n"

    "xorl %%edx,%%edx\n"
    /* s10 */
    "testl $0x400,%4\n"
    "jz .addmulext512s10\n"
    /* a[0]*b[10] */
    "movl 0(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[1]*b[9] */
    "movl 4(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[2]*b[8] */
    "movl 8(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
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
    /* a[8]*b[2] */
    "movl 32(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[9]*b[1] */
    "movl 36(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[10]*b[0] */
    "movl 40(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%ebx,40(%%eax)\n"
    "adcl $0,%%edi\n"
    "adcl $0,%%edx\n"
    ".addmulext512s10:\n"

    "xorl %%ebx,%%ebx\n"
    /* s11 */
    "testl $0x800,%4\n"
    "jz .addmulext512s11\n"
    /* a[0]*b[11] */
    "movl 0(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[10] */
    "movl 4(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[9] */
    "movl 8(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[8] */
    "movl 12(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
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
    /* a[8]*b[3] */
    "movl 32(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[2] */
    "movl 36(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[1] */
    "movl 40(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[0] */
    "movl 44(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,44(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s11:\n"

    "xorl %%edi,%%edi\n"
    /* s12 */
    "testl $0x1000,%4\n"
    "jz .addmulext512s12\n"
    /* a[0]*b[12] */
    "movl 20(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[11] */
    "movl 4(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[10] */
    "movl 8(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[9] */
    "movl 12(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[8] */
    "movl 16(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
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
    /* a[8]*b[4] */
    "movl 32(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[3] */
    "movl 36(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[2] */
    "movl 40(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[1] */
    "movl 44(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[0] */
    "movl 48(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,48(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s12:\n"

    "xorl %%edx,%%edx\n"
    /* s13 */
    "testl $0x2000,%4\n"
    "jz .addmulext512s13\n"
    /* a[0]*b[13] */
    "movl 0(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[1]*b[12] */
    "movl 4(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[2]*b[11] */
    "movl 8(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[3]*b[10] */
    "movl 12(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[4]*b[9] */
    "movl 16(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[5]*b[8] */
    "movl 20(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
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
    /* a[8]*b[5] */
    "movl 32(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[9]*b[4] */
    "movl 36(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[10]*b[3] */
    "movl 40(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[11]*b[2] */
    "movl 44(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[12]*b[1] */
    "movl 48(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[13]*b[0] */
    "movl 52(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,52(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s13:\n"

    "xorl %%ebx,%%ebx\n"
    /* s14 */
    "testl $0x4000,%4\n"
    "jz .addmulext512s14\n"
    /* a[0]*b[14] */
    "movl 0(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[1]*b[13] */
    "movl 4(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[2]*b[12] */
    "movl 8(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[11] */
    "movl 12(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[10] */
    "movl 16(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[9] */
    "movl 20(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[8] */
    "movl 24(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[7] */
    "movl 28(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[8]*b[6] */
    "movl 32(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[5] */
    "movl 36(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[4] */
    "movl 40(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[3] */
    "movl 44(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[2] */
    "movl 48(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[1] */
    "movl 52(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[0] */
    "movl 56(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,56(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s14:\n"

    "xorl %%edi,%%edi\n"
    /* s15 */
    "testl $0x8000,%4\n"
    "jz .addmulext512s15\n"
    /* a[0]*b[15] */
    "movl 0(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[1]*b[14] */
    "movl 4(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[2]*b[13] */
    "movl 8(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[3]*b[12] */
    "movl 12(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[11] */
    "movl 16(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[10] */
    "movl 20(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[9] */
    "movl 24(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[8] */
    "movl 28(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[8]*b[7] */
    "movl 32(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[6] */
    "movl 36(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[5] */
    "movl 40(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[4] */
    "movl 44(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[3] */
    "movl 48(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[2] */
    "movl 52(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[1] */
    "movl 56(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[0] */
    "movl 60(%2),%%eax\n"
    "mull 0(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,60(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s15:\n"

    "xorl %%edx,%%edx\n"
    /* s16 */
    "testl $0x10000,%4\n"
    "jz .addmulext512s16\n"
    /* a[1]*b[15] */
    "movl 4(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[2]*b[14] */
    "movl 8(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[3]*b[13] */
    "movl 12(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[4]*b[12] */
    "movl 16(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[5]*b[11] */
    "movl 20(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[6]*b[10] */
    "movl 24(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[7]*b[9] */
    "movl 28(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[8]*b[8] */
    "movl 32(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[9]*b[7] */
    "movl 36(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[10]*b[6] */
    "movl 40(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[11]*b[5] */
    "movl 44(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[12]*b[4] */
    "movl 48(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[13]*b[3] */
    "movl 52(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[14]*b[2] */
    "movl 56(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[15]*b[1] */
    "movl 60(%2),%%eax\n"
    "mull 4(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,64(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s16:\n"

    "xorl %%ebx,%%ebx\n"
    /* s17 */
    "testl $0x20000,%4\n"
    "jz .addmulext512s17\n"
    /* a[2]*b[15] */
    "movl 8(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[3]*b[14] */
    "movl 12(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[4]*b[13] */
    "movl 16(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[5]*b[12] */
    "movl 20(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[11] */
    "movl 24(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[10] */
    "movl 28(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[8]*b[9] */
    "movl 32(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[8] */
    "movl 36(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[7] */
    "movl 40(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[6] */
    "movl 44(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[5] */
    "movl 48(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[4] */
    "movl 52(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[3] */
    "movl 56(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[2] */
    "movl 50(%2),%%eax\n"
    "mull 8(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,68(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s17:\n"

    "xorl %%edi,%%edi\n"
    /* s18 */
    "testl $0x40000,%4\n"
    "jz .addmulext512s18\n"
    /* a[3]*b[15] */
    "movl 12(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[4]*b[14] */
    "movl 16(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[5]*b[13] */
    "movl 20(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[6]*b[12] */
    "movl 24(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[11] */
    "movl 28(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[8]*b[10] */
    "movl 32(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[9] */
    "movl 36(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[8] */
    "movl 40(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[7] */
    "movl 44(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[6] */
    "movl 48(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[5] */
    "movl 52(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[4] */
    "movl 56(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[3] */
    "movl 60(%2),%%eax\n"
    "mull 12(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,72(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s18:\n"

    "xorl %%edx,%%edx\n"
    /* s19 */
    "testl $0x80000,%4\n"
    "jz .addmulext512s19\n"
    /* a[4]*b[15] */
    "movl 16(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[5]*b[14] */
    "movl 20(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[6]*b[13] */
    "movl 24(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[7]*b[12] */
    "movl 28(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[8]*b[11] */
    "movl 32(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[9]*b[10] */
    "movl 36(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[10]*b[9] */
    "movl 40(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[11]*b[8] */
    "movl 44(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[12]*b[7] */
    "movl 48(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[13]*b[6] */
    "movl 52(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[14]*b[5] */
    "movl 56(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[15]*b[4] */
    "movl 60(%2),%%eax\n"
    "mull 16(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,76(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s19:\n"

    "xorl %%ebx,%%ebx\n"
    /* s20 */
    "testl $0x100000,%4\n"
    "jz .addmulext512s20\n"
    /* a[5]*b[15] */
    "movl 20(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[6]*b[14] */
    "movl 24(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[7]*b[13] */
    "movl 28(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[8]*b[12] */
    "movl 32(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[11] */
    "movl 36(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[10] */
    "movl 40(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[9] */
    "movl 44(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[8] */
    "movl 48(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[7] */
    "movl 52(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[6] */
    "movl 56(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[5] */
    "movl 60(%2),%%eax\n"
    "mull 20(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,80(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s20:\n"

    "xorl %%edi,%%edi\n"
    /* s21 */
    "testl $0x200000,%4\n"
    "jz .addmulext512s21\n"
    /* a[6]*b[15] */
    "movl 24(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[7]*b[14] */
    "movl 28(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[8]*b[13] */
    "movl 32(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[9]*b[12] */
    "movl 36(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[11] */
    "movl 40(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[10] */
    "movl 44(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[9] */
    "movl 48(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[8] */
    "movl 52(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[7] */
    "movl 56(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[6] */
    "movl 60(%2),%%eax\n"
    "mull 24(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,84(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s21:\n"

    "xorl %%edx,%%edx\n"
    /* s22 */
    "testl $0x400000,%4\n"
    "jz .addmulext512s22\n"
    /* a[7]*b[15] */
    "movl 28(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[8]*b[14] */
    "movl 32(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[9]*b[13] */
    "movl 36(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[10]*b[12] */
    "movl 40(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[11]*b[11] */
    "movl 44(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[12]*b[10] */
    "movl 48(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[13]*b[9] */
    "movl 52(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[14]*b[8] */
    "movl 56(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[15]*b[7] */
    "movl 60(%2),%%eax\n"
    "mull 28(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,88(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s22:\n"

    "xorl %%ebx,%%ebx\n"
    /* s23 */
    "testl $0x800000,%4\n"
    "jz .addmulext512s23\n"
    /* a[8]*b[15] */
    "movl 32(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[9]*b[14] */
    "movl 36(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[10]*b[13] */
    "movl 40(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[11]*b[12] */
    "movl 44(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[11] */
    "movl 48(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[10] */
    "movl 52(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[9] */
    "movl 56(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[8] */
    "movl 60(%2),%%eax\n"
    "mull 32(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,92(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s23:\n"

    "xorl %%edi,%%edi\n"
    /* s24 */
    "testl $0x1000000,%4\n"
    "jz .addmulext512s24\n"
    /* a[9]*b[15] */
    "movl 36(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[10]*b[14] */
    "movl 40(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[11]*b[13] */
    "movl 44(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[12]*b[12] */
    "movl 48(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[11] */
    "movl 52(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[10] */
    "movl 56(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[9] */
    "movl 60(%2),%%eax\n"
    "mull 36(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,96(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s24:\n"

    "xorl %%edx,%%edx\n"
    /* s25 */
    "testl $0x2000000,%4\n"
    "jz .addmulext512s25\n"
    /* a[10]*b[15] */
    "movl 40(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[11]*b[14] */
    "movl 44(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[12]*b[13] */
    "movl 48(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[13]*b[12] */
    "movl 52(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[14]*b[11] */
    "movl 56(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[15]*b[10] */
    "movl 60(%2),%%eax\n"
    "mull 40(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,100(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s25:\n"

    "xorl %%ebx,%%ebx\n"
    /* s26 */
    "testl $0x4000000,%4\n"
    "jz .addmulext512s26\n"
    /* a[11]*b[15] */
    "movl 44(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[12]*b[14] */
    "movl 48(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[13]*b[13] */
    "movl 52(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[14]*b[12] */
    "movl 56(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[11] */
    "movl 60(%2),%%eax\n"
    "mull 44(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,104(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s26:\n"

    "xorl %%edi,%%edi\n"
    /* s27 */
    "testl $0x8000000,%4\n"
    "jz .addmulext512s27\n"
    /* a[12]*b[15] */
    "movl 48(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[13]*b[14] */
    "movl 52(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[14]*b[13] */
    "movl 56(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* a[15]*b[12] */
    "movl 60(%2),%%eax\n"
    "mull 48(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%%edi\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,108(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s27:\n"

    "xorl %%edx,%%edx\n"
    /* s28 */
    "testl $0x10000000,%4\n"
    "jz .addmulext512s28\n"
    /* a[13]*b[15] */
    "movl 52(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[14]*b[14] */
    "movl 56(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* a[15]*b[13] */
    "movl 60(%2),%%eax\n"
    "mull 52(%3)\n"
    "addl %%eax,%%ebx\n"
    "adcl %%edx,%%edi\n"
    "adcl $0,%%edx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edx,112(%%eax)\n"
    "adcl $0,%%ebx\n"
    "adcl $0,%%edi\n"
    ".addmulext512s28:\n"

    "xorl %%ebx,%%ebx\n"
    /* s29 */
    "testl $0x20000000,%4\n"
    "jz .addmulext512s29\n"
    /* a[14]*b[15] */
    "movl 56(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* a[15]*b[14] */
    "movl 60(%2),%%eax\n"
    "mull 56(%3)\n"
    "addl %%eax,%%edi\n"
    "adcl %%edx,%%edx\n"
    "adcl $0,%%ebx\n"
    /* add sum */
    "movl %1,%%eax\n"
    "addl %%edi,116(%%eax)\n"
    "adcl $0,%%edx\n"
    "adcl $0,%%ebx\n"
    ".addmulext512s29:\n"

    /* s30 */
    "testl $0xC0000000,%4\n"
    "jz .addmulext512s30\n"
    /* a[7]*b[7] */
    "movl 60(%2),%%eax\n"
    "mull 60(%3)\n"
    "addl %%eax,%%edx\n"
    "adcl %%edx,%%ebx\n"
    "adcl $0,%0\n"
    "movl %1,%%eax\n"
    "addl %%edx,120(%%eax)\n"
    "adcl %%ebx,124(%%eax)\n"
    "adcl $0,%0\n"
    ".addmulext512s30:\n"

    : "=m" (overflow)
    : "m" (result), "r" (src0), "r" (src1), "m" (unitmask), "m" (midcarry)
    : "%eax", "%ebx", "%ecx", "%edx", "%edi", "memory" );
#endif
  return overflow;
}


void bn512MulShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT512_UNIT_COUNT*2];
  highunit = ( 512 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );
  bn512MulExtended( result, src0->unit, src1->unit, unitmask );
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn512Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn512Add32( dst, 1 );
  }
  else
  {
    bn512CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn512Add32( dst, 1 );
  }
  return;
}


void bn512MulSignedShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shiftbits )
{
  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT512_UNIT_COUNT*2];

  highunit = ( 512 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn512MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn512SubHigh( result, src1->unit );
  if( src1->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn512SubHigh( result, src0->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn512Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn512Add32( dst, 1 );
  }
  else
  {
    bn512CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn512Add32( dst, 1 );
  }

  return;
}


bnUnit bn512MulCheckShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT512_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 512 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn512MulExtended( result, src0->unit, src1->unit, unitmask );

  highsum = bn512HighNonMatch( src0, 0 ) + bn512HighNonMatch( src1, 0 );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT512_UNIT_COUNT );
    bn512Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn512Add32( dst, 1 );
  }
  else
  {
    overflow = 0;
    if( ( highsum >= offset+BN_INT512_UNIT_COUNT+1 ) || ( result[offset+BN_INT512_UNIT_COUNT] >> shift ) )
      overflow = 1;
    bn512CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn512Add32( dst, 1 );
  }

  return overflow;
}


bnUnit bn512MulSignedCheckShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shiftbits )
{
  bnUnit offset, highunit, lowunit, unitmask, highsum;
  char shift;
  bnUnit result[BN_INT512_UNIT_COUNT*2];
  bnUnit overflow;

  highunit = ( 512 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  bn512MulExtended( result, src0->unit, src1->unit, unitmask );

  if( src0->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn512SubHigh( result, src1->unit );
  if( src1->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn512SubHigh( result, src0->unit );

  highsum = bn512HighNonMatch( src0, bn512SignMask( src0 ) ) + bn512HighNonMatch( src1, bn512SignMask( src1 ) );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    overflow = ( highsum >= offset+BN_INT512_UNIT_COUNT );
    bn512Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn512Add32( dst, 1 );
  }
  else
  {
    if( highsum >= offset+BN_INT512_UNIT_COUNT+1 )
      overflow = 1;
    else
    {
      overflow = result[offset+BN_INT512_UNIT_COUNT];
      if( src0->unit[BN_INT512_UNIT_COUNT-1] ^ src1->unit[BN_INT512_UNIT_COUNT-1] )
        overflow = ~overflow;
      overflow >>= shift;
    }
    bn512CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn512Add32( dst, 1 );
  }

  return overflow;
}


void bn512SquareShr( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{

#if BN_UNIT_BITS == 64

  bnShift offset, highunit, lowunit, unitmask;
  char shift;
  bnUnit result[BN_INT512_UNIT_COUNT*2];

  highunit = ( 512 + shiftbits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  unitmask = ( 1 << highunit ) - 1;
  lowunit = ( ( (int)shiftbits - BN_UNIT_BITS ) >> BN_UNIT_BIT_SHIFT ) - 1;
  if( lowunit > 0 )
    unitmask &= ~( ( 1 << lowunit ) - 1 );

  __asm__ __volatile__(

    "xorq %%rbx,%%rbx\n"
    /* s0 */
    "xorq %%rcx,%%rcx\n"
    "testq $1,%2\n"
    "jz .sqshr512s0\n"
    /* a[0]*b[0] */
    "movq 0(%1),%%rax\n"
    "mulq %%rax\n"
    "movq %%rax,0(%0)\n"
    "addq %%rdx,%%rbx\n"
    ".sqshr512s0:\n"

    /* s1 */
    "xorq %%r8,%%r8\n"
    "testq $2,%2\n"
    "jz .sqshr512s1\n"
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
    ".sqshr512s1:\n"

    /* s2 */
    "xorq %%rbx,%%rbx\n"
    "testq $4,%2\n"
    "jz .sqshr512s2\n"
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
    ".sqshr512s2:\n"

    /* s3 */
    "xorq %%rcx,%%rcx\n"
    "testq $8,%2\n"
    "jz .sqshr512s3\n"
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
    ".sqshr512s3:\n"

    /* s4 */
    "xorq %%r8,%%r8\n"
    "testq $0x10,%2\n"
    "jz .sqshr512s4\n"
    /* a[0]*b[4] */
    "movq 0(%1),%%rax\n"
    "mulq 32(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[0] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
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
    ".sqshr512s4:\n"

    /* s5 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x20,%2\n"
    "jz .sqshr512s5\n"
    /* a[0]*b[5] */
    "movq 0(%1),%%rax\n"
    "mulq 40(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[0] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[1]*b[4] */
    "movq 8(%1),%%rax\n"
    "mulq 32(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[4]*b[1] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
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
    ".sqshr512s5:\n"

    /* s6 */
    "xorq %%rcx,%%rcx\n"
    "testq $0x40,%2\n"
    "jz .sqshr512s6\n"
    /* a[0]*b[6] */
    "movq 0(%1),%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[0] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[1]*b[5] */
    "movq 8(%1),%%rax\n"
    "mulq 40(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[1] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[2]*b[4] */
    "movq 16(%1),%%rax\n"
    "mulq 32(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[2] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[3] */
    "movq 24(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,48(%0)\n"
    ".sqshr512s6:\n"

    /* s7 */
    "xorq %%r8,%%r8\n"
    "testq $0x80,%2\n"
    "jz .sqshr512s7\n"
    /* a[0]*b[7] */
    "movq 0(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[0] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[1]*b[6] */
    "movq 8(%1),%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[1] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[2]*b[5] */
    "movq 16(%1),%%rax\n"
    "mulq 40(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[2] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[3]*b[4] */
    "movq 24(%1),%%rax\n"
    "mulq 32(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[3] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,56(%0)\n"
    ".sqshr512s7:\n"

    /* s8 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x100,%2\n"
    "jz .sqshr512s8\n"
    /* a[1]*b[7] */
    "movq 8(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[7]*b[1] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[2]*b[6] */
    "movq 16(%1),%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[6]*b[2] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[3]*b[5] */
    "movq 24(%1),%%rax\n"
    "mulq 40(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[3] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[4]*b[4] */
    "movq 32(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,64(%0)\n"
    ".sqshr512s8:\n"

    /* s9 */
    "xorq %%rcx,%%rcx\n"
    "testq $0x200,%2\n"
    "jz .sqshr512s9\n"
    /* a[2]*b[7] */
    "movq 16(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[2] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[3]*b[6] */
    "movq 24(%1),%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[3] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[4]*b[5] */
    "movq 32(%1),%%rax\n"
    "mulq 40(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[5]*b[4] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,72(%0)\n"
    ".sqshr512s9:\n"

    /* s10 */
    "xorq %%r8,%%r8\n"
    "testq $0x400,%2\n"
    "jz .sqshr512s10\n"
    /* a[3]*b[7] */
    "movq 24(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[3] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[4]*b[6] */
    "movq 32(%1),%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[6]*b[4] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[5]*b[5] */
    "movq 40(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,80(%0)\n"
    ".sqshr512s10:\n"

    /* s11 */
    "xorq %%rbx,%%rbx\n"
    "testq $0x800,%2\n"
    "jz .sqshr512s11\n"
    /* a[4]*b[7] */
    "movq 32(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[7]*b[4] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[5]*b[6] */
    "movq 40(%1),%%rax\n"
    "mulq 48(%1)\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* a[6]*b[5] */
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    "adcq $0,%%rbx\n"
    /* add sum */
    "movq %%rcx,88(%0)\n"
    ".sqshr512s11:\n"

    /* s12 */
    "xorq %%rcx,%%rcx\n"
    "testq $0x1000,%2\n"
    "jz .sqshr512s12\n"
    /* a[5]*b[7] */
    "movq 40(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[7]*b[5] */
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* a[6]*b[6] */
    "movq 48(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%r8\n"
    "adcq %%rdx,%%rbx\n"
    "adcq $0,%%rcx\n"
    /* add sum */
    "movq %%r8,96(%0)\n"
    ".sqshr512s12:\n"

    /* s13 */
    "xorq %%r8,%%r8\n"
    "testq $0x2000,%2\n"
    "jz .sqshr512s13\n"
    /* a[6]*b[7] */
    "movq 48(%1),%%rax\n"
    "mulq 56(%1)\n"
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* a[7]*b[6] */
    "addq %%rax,%%rbx\n"
    "adcq %%rdx,%%rcx\n"
    "adcq $0,%%r8\n"
    /* add sum */
    "movq %%rbx,104(%0)\n"
    ".sqshr512s13:\n"

    /* s14 */
    "testq $0xC000,%2\n"
    "jz .sqshr512s14\n"
    /* a[7]*b[7] */
    "movq 56(%1),%%rax\n"
    "mulq %%rax\n"
    "addq %%rax,%%rcx\n"
    "adcq %%rdx,%%r8\n"
    /* add sum */
    "movq %%rcx,112(%0)\n"
    "movq %%r8,120(%0)\n"
    ".sqshr512s14:\n"
    :
    : "r" (result), "r" (src), "r" (unitmask)
    : "%rax", "%rbx", "%rcx", "%rdx", "%r8", "memory" );

  if( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    bn512SubHighTwice( result, src->unit );

  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  if( !( shift ) )
  {
    bn512Copy( dst, &result[offset] );
    if( ( offset ) && ( result[offset-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
      bn512Add32( dst, 1 );
  }
  else
  {
    bn512CopyShift( dst, &result[offset], shift );
    if( ( result[offset] & ( (bnUnit)1 << (shift-1) ) ) )
      bn512Add32( dst, 1 );
  }

#else

  bn512MulSignedShr( dst, src, src, shiftbits );

#endif

  return;
}



////



void bn512Div32( bn512 *dst, uint32_t divisor, uint32_t *rem )
{
  bnShift offset, divisormsb;
  bn512 div, quotient, dsthalf, qpart, dpart;
  bn512Set32( &div, divisor );
  bn512Zero( &quotient );
  divisormsb = bn512GetIndexMSB( &div );
  for( ; ; )
  {
    if( bn512CmpLt( dst, &div ) )
      break;
    bn512Set( &dpart, &div );
    bn512Set32( &qpart, 1 );
    bn512SetShr1( &dsthalf, dst );
    offset = bn512GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn512Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn512CmpLe( &dpart, &dsthalf ) )
    {
      bn512Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn512Shl( &qpart, &qpart, offset );
    bn512Sub( dst, &dpart );
    bn512Add( &quotient, &qpart );
  }
  if( rem )
    *rem = dst->unit[0];
  if( bn512CmpEq( dst, &div ) )
  {
    if( rem )
      *rem = 0;
    bn512Add32( &quotient, 1 );
  }
  bn512Set( dst, &quotient );
  return;
}


void bn512Div32Signed( bn512 *dst, int32_t divisor, int32_t *rem )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn512Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn512Div32( dst, (uint32_t)divisor, (uint32_t *)rem );
  if( sign )
  {
    bn512Neg( dst );
    if( rem )
      *rem = -*rem;
  }
  return;
}


void bn512Div32Round( bn512 *dst, uint32_t divisor )
{
  uint32_t rem;
  bn512Div32( dst, divisor, &rem );
  if( ( rem << 1 ) >= divisor )
    bn512Add32( dst, 1 );
  return;
}


void bn512Div32RoundSigned( bn512 *dst, int32_t divisor )
{
  int sign;
  sign = 0;
  if( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn512Neg( dst );
  }
  if( divisor < 0 )
  {
    sign ^= 1;
    divisor = -divisor;
  }
  bn512Div32Round( dst, (uint32_t)divisor );
  if( sign )
    bn512Neg( dst );
  return;
}


void bn512Div( bn512 *dst, const bn512 * const divisor, bn512 *rem )
{
  bnShift offset, remzero, divisormsb;
  bn512 quotient, dsthalf, qpart, dpart;
  bn512Zero( &quotient );
  divisormsb = bn512GetIndexMSB( divisor );
  for( ; ; )
  {
    if( bn512CmpLt( dst, divisor ) )
      break;
    bn512Set( &dpart, divisor );
    bn512Set32( &qpart, 1 );
    bn512SetShr1( &dsthalf, dst );
    offset = bn512GetIndexMSB( &dsthalf ) - divisormsb;
    if( offset > 0 )
      bn512Shl( &dpart, &dpart, offset );
    else
      offset = 0;
    while( bn512CmpLe( &dpart, &dsthalf ) )
    {
      bn512Shl1( &dpart );
      offset++;
    }
    if( offset )
      bn512Shl( &qpart, &qpart, offset );
    bn512Sub( dst, &dpart );
    bn512Add( &quotient, &qpart );
  }
  remzero = 0;
  if( bn512CmpEq( dst, divisor ) )
  {
    bn512Add32( &quotient, 1 );
    remzero = 1;
  }
  if( rem )
  {
    if( remzero )
      bn512Zero( rem );
    else
      bn512Set( rem, dst );
  }
  bn512Set( dst, &quotient );
  return;
}


void bn512DivSigned( bn512 *dst, const bn512 * const divisor, bn512 *rem )
{
  int sign;
  bn512 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn512Neg( dst );
  }
  if( divisor->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn512SetNeg( &divisorabs, divisor );
    bn512Div( dst, &divisorabs, rem );
  }
  else
    bn512Div( dst, divisor, rem );
  if( sign )
  {
    bn512Neg( dst );
    if( rem )
      bn512Neg( rem );
  }
  return;
}


void bn512DivRound( bn512 *dst, const bn512 * const divisor )
{
  bn512 rem;
  bn512Div( dst, divisor, &rem );
  bn512Shl1( &rem );
  if( bn512CmpGe( &rem, divisor ) )
    bn512Add32( dst, 1 );
  return;
}


void bn512DivRoundSigned( bn512 *dst, const bn512 * const divisor )
{
  int sign;
  bn512 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn512Neg( dst );
  }
  if( divisor->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn512SetNeg( &divisorabs, divisor );
    bn512DivRound( dst, &divisorabs );
  }
  else
    bn512DivRound( dst, divisor );
  if( sign )
    bn512Neg( dst );
  return;
}


void bn512DivShl( bn512 *dst, const bn512 * const divisor, bn512 *rem, bnShift shift )
{
  bnShift fracshift;
  bn512 basedst, fracpart, qpart;
  if( !( rem ) )
    rem = &fracpart;
  bn512Set( &basedst, dst );
  bn512Div( &basedst, divisor, rem );
  bn512Shl( dst, &basedst, shift );
  for( fracshift = shift-1 ; fracshift >= 0 ; fracshift-- )
  {
    if( bn512CmpZero( rem ) )
      break;
    bn512Shl1( rem );
    if( bn512CmpGe( rem, divisor ) )
    {
      bn512Sub( rem, divisor );
      bn512Set32Shl( &qpart, 1, fracshift );
      bn512Or( dst, &qpart );
    }
  }
  return;
}


void bn512DivSignedShl( bn512 *dst, const bn512 * const divisor, bn512 *rem, bnShift shift )
{
  int sign;
  bn512 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn512Neg( dst );
  }
  if( divisor->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn512SetNeg( &divisorabs, divisor );
    bn512DivShl( dst, &divisorabs, rem, shift );
  }
  else
    bn512DivShl( dst, divisor, rem, shift );
  if( sign )
  {
    bn512Neg( dst );
    if( rem )
      bn512Neg( rem );
  }
  return;
}


void bn512DivRoundShl( bn512 *dst, const bn512 * const divisor, bnShift shift )
{
  bn512 rem;
  bn512DivShl( dst, divisor, &rem, shift );
  bn512Shl1( &rem );
  if( bn512CmpGe( &rem, divisor ) )
    bn512Add32( dst, 1 );
  return;
}


void bn512DivRoundSignedShl( bn512 *dst, const bn512 * const divisor, bnShift shift )
{
  int sign;
  bn512 divisorabs;
  sign = 0;
  if( dst->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign = 1;
    bn512Neg( dst );
  }
  if( divisor->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
  {
    sign ^= 1;
    bn512SetNeg( &divisorabs, divisor );
    bn512DivRoundShl( dst, &divisorabs, shift );
  }
  else
    bn512DivRoundShl( dst, divisor, shift );
  if( sign )
    bn512Neg( dst );
  return;
}



////



void bn512Or( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] |= src->unit[unitindex];
  return;
}


void bn512SetOr( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] | src1->unit[unitindex];
  return;
}


void bn512Nor( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( dst->unit[unitindex] | src->unit[unitindex] );
  return;
}


void bn512SetNor( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] | src1->unit[unitindex] );
  return;
}


void bn512And( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] &= src->unit[unitindex];
  return;
}


void bn512SetAnd( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] & src1->unit[unitindex];
  return;
}


void bn512Nand( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] & dst->unit[unitindex] );
  return;
}


void bn512SetNand( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] & src1->unit[unitindex] );
  return;
}


void bn512Xor( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] ^= src->unit[unitindex];
  return;
}


void bn512SetXor( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = src0->unit[unitindex] ^ src1->unit[unitindex];
  return;
}


void bn512Nxor( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src->unit[unitindex] ^ dst->unit[unitindex] );
  return;
}


void bn512SetNxor( bn512 *dst, const bn512 * const src0, const bn512 * const src1 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~( src0->unit[unitindex] ^ src1->unit[unitindex] );
  return;
}


void bn512Not( bn512 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  return;
}


void bn512SetNot( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  return;
}


void bn512Neg( bn512 *dst )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~dst->unit[unitindex];
  bn512Add32( dst, 1 );
  return;
}


void bn512SetNeg( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = ~src->unit[unitindex];
  bn512Add32( dst, 1 );
  return;
}


void bn512Shl( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[-offset];
  if( !( shift ) )
  {
    limit = offset;
    for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= limit ; unitindex-- )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex >= 0 ; unitindex-- )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = offset;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = BN_INT512_UNIT_COUNT-1;
    if( offset < BN_INT512_UNIT_COUNT )
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


void bn512Shr( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT512_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  else
  {
    limit = BN_INT512_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT512_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift );
      unitindex++;
    }
    for( ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn512Sal( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{
  bn512Shl( dst, src, shiftbits );
  return;
}


void bn512Sar( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{
  bnShift unitindex, offset, shift, shiftinv, limit;
  const bnUnit *srcunit;
  if( !( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn512Shr( dst, src, shiftbits );
    return;
  }
  shift = shiftbits & (BN_UNIT_BITS-1);
  offset = shiftbits >> BN_UNIT_BIT_SHIFT;
  srcunit = &src->unit[offset];
  if( !( shift ) )
  {
    limit = BN_INT512_UNIT_COUNT - offset;
    for( unitindex = 0 ; unitindex < limit ; unitindex++ )
      dst->unit[unitindex] = srcunit[unitindex];
    for( ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = -1;
  }
  else
  {
    limit = BN_INT512_UNIT_COUNT - offset - 1;
    shiftinv = BN_UNIT_BITS - shift;
    unitindex = 0;
    if( offset < BN_INT512_UNIT_COUNT )
    {
      for( ; unitindex < limit ; unitindex++ )
        dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( srcunit[unitindex+1] << shiftinv );
      dst->unit[unitindex] = ( srcunit[unitindex] >> shift ) | ( (bnUnit)-1 << shiftinv );;
      unitindex++;
    }
    for( ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
      dst->unit[unitindex] = 0;
  }
  return;
}


void bn512ShrRound( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn512ExtractBit( src, shiftbits-1 );
  bn512Shr( dst, src, shiftbits );
  if( bit )
    bn512Add32( dst, 1 );
  return;
}


void bn512SarRound( bn512 *dst, const bn512 * const src, bnShift shiftbits )
{
  int bit;
  bit = bn512ExtractBit( src, shiftbits-1 );
  bn512Sar( dst, src, shiftbits );
  if( bit )
    bn512Add32( dst, 1 );
  return;
}


void bn512Shl1( bn512 *dst )
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
    :
    : "r" (dst->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512SetShl1( bn512 *dst, const bn512 * const src )
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
    :
    : "r" (dst->unit), "r" (src->unit)
    : "%eax", "%ebx", "%ecx", "%edx", "memory" );
#endif
  return;
}


void bn512Shr1( bn512 *dst )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( dst->unit[0] >> 1 ) | ( dst->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( dst->unit[1] >> 1 ) | ( dst->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( dst->unit[2] >> 1 ) | ( dst->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( dst->unit[3] >> 1 ) | ( dst->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( dst->unit[4] >> 1 ) | ( dst->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( dst->unit[5] >> 1 ) | ( dst->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( dst->unit[6] >> 1 ) | ( dst->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( dst->unit[7] >> 1 );
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
  dst->unit[15] = ( dst->unit[15] >> 1 );
#endif
  return;
}


void bn512SetShr1( bn512 *dst, const bn512 * const src )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( src->unit[0] >> 1 ) | ( src->unit[1] << (BN_UNIT_BITS-1) );
  dst->unit[1] = ( src->unit[1] >> 1 ) | ( src->unit[2] << (BN_UNIT_BITS-1) );
  dst->unit[2] = ( src->unit[2] >> 1 ) | ( src->unit[3] << (BN_UNIT_BITS-1) );
  dst->unit[3] = ( src->unit[3] >> 1 ) | ( src->unit[4] << (BN_UNIT_BITS-1) );
  dst->unit[4] = ( src->unit[4] >> 1 ) | ( src->unit[5] << (BN_UNIT_BITS-1) );
  dst->unit[5] = ( src->unit[5] >> 1 ) | ( src->unit[6] << (BN_UNIT_BITS-1) );
  dst->unit[6] = ( src->unit[6] >> 1 ) | ( src->unit[7] << (BN_UNIT_BITS-1) );
  dst->unit[7] = ( src->unit[7] >> 1 );
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
  dst->unit[15] = ( src->unit[15] >> 1 );
#endif
  return;
}



////



int bn512CmpZero( const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 0;
  }
  return 1;
}

int bn512CmpNotZero( const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
  {
    if( src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn512CmpEq( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn512CmpNeq( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 1;
  }
  return 0;
}


int bn512CmpGt( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn512CmpGe( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn512CmpLt( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn512CmpLe( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn512CmpSignedGt( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT512_UNIT_COUNT-1] ^ src->unit[BN_INT512_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT512_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn512CmpSignedGe( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT512_UNIT_COUNT-1] ^ src->unit[BN_INT512_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return src->unit[BN_INT512_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn512CmpSignedLt( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT512_UNIT_COUNT-1] ^ src->unit[BN_INT512_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT512_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 0;
}


int bn512CmpSignedLe( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  if( ( dst->unit[BN_INT512_UNIT_COUNT-1] ^ src->unit[BN_INT512_UNIT_COUNT-1] ) & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    return dst->unit[BN_INT512_UNIT_COUNT-1] >> (BN_UNIT_BITS-1);
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
  {
    if( dst->unit[unitindex] < src->unit[unitindex] )
      return 1;
    if( dst->unit[unitindex] > src->unit[unitindex] )
      return 0;
  }
  return 1;
}


int bn512CmpPositive( const bn512 * const src )
{
  return ( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) == 0;
}


int bn512CmpNegative( const bn512 * const src )
{
  return ( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) != 0;
}


int bn512CmpEqOrZero( bn512 *dst, const bn512 * const src )
{
  int unitindex;
  bnUnit accum, diffaccum;
  accum = 0;
  diffaccum = 0;
  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
  {
    diffaccum |= dst->unit[unitindex] - src->unit[unitindex];
    accum |= dst->unit[unitindex];
  }
  return ( ( accum & diffaccum ) == 0 );
}


int bn512CmpPart( bn512 *dst, const bn512 * const src, uint32_t bits )
{
  int unitindex, unitcount, partbits;
  bnUnit unitmask;
  unitcount = ( bits + (BN_UNIT_BITS-1) ) >> BN_UNIT_BIT_SHIFT;
  partbits = bits & (BN_UNIT_BITS-1);
  unitindex = BN_INT512_UNIT_COUNT - unitcount;
  if( partbits )
  {
    unitmask = (bnUnit)-1 << (BN_UNIT_BITS-partbits);
    if( ( dst->unit[unitindex] & unitmask ) != ( src->unit[unitindex] & unitmask ) )
      return 0;
    unitindex++;
  }
  for( ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
  {
    if( dst->unit[unitindex] != src->unit[unitindex] )
      return 0;
  }
  return 1;
}



////



int bn512ExtractBit( const bn512 * const src, int bitoffset )
{
  int unitindex, shift;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  value = 0;
  if( (unsigned)unitindex < BN_INT512_UNIT_COUNT )
    value = ( src->unit[unitindex] >> shift ) & 0x1;
  return value;
}


uint32_t bn512Extract32( const bn512 * const src, int bitoffset )
{
  int unitindex, shift, shiftinv;
  uint32_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT512_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 32 ) && ( (unsigned)unitindex+1 < BN_INT512_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
}


uint64_t bn512Extract64( const bn512 * const src, int bitoffset )
{
#if BN_UNIT_BITS == 64
  int unitindex, shift, shiftinv;
  uint64_t value;
  unitindex = bitoffset >> BN_UNIT_BIT_SHIFT;
  shift = bitoffset & (BN_UNIT_BITS-1);
  shiftinv = BN_UNIT_BITS - shift;
  value = 0;
  if( (unsigned)unitindex < BN_INT512_UNIT_COUNT )
    value = src->unit[unitindex] >> shift;
  if( ( shiftinv < 64 ) && ( (unsigned)unitindex+1 < BN_INT512_UNIT_COUNT ) )
    value |= src->unit[unitindex+1] << shiftinv;
  return value;
#else
  return (uint64_t)bn512Extract32( src, bitoffset ) | ( (uint64_t)bn512Extract32( src, bitoffset+32 ) << 32 );
#endif
}


int bn512GetIndexMSB( const bn512 * const src )
{
  int unitindex;
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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


int bn512GetIndexMSZ( const bn512 * const src )
{
  int unitindex;
  bnUnit unit;
  for( unitindex = BN_INT512_UNIT_COUNT-1 ; unitindex >= 0 ; unitindex-- )
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



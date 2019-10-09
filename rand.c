/*
   A C-program for MT19937-64 (2004/9/29 version).
   Coded by Takuji Nishimura and Makoto Matsumoto.

   This is a 64-bit version of Mersenne Twister pseudorandom number
   generator.

   Before using, initialize the state by using init_genrand64(seed)
   or init_by_array64(seedarray, arraycount).

   Copyright (C) 2004, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

   3. The names of its contributors may not be used to endorse or promote
    products derived from this software without specific prior written
    permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   References:
   T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
   ACM Transactions on Modeling and
   Computer Simulation 10. (2000) 348--357.
   M. Matsumoto and T. Nishimura,
   ``Mersenne Twister: a 623-dimensionally equidistributed
     uniform pseudorandom number generator''
   ACM Transactions on Modeling and
   Computer Simulation 8. (Jan. 1998) 3--30.

   Any feedback is very welcome.
   http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)
*/


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>


#include "rand.h"


void rand64Seed( rand64State *state, uint64_t seed )
{
  int mti;
  state->mt[0] = seed;
  for( mti = 1 ; mti < RAND64_ARRAYSIZE ; mti++ )
    state->mt[mti] = ( ((uint64_t)6364136223846793005ULL) * ( state->mt[mti-1] ^ ( state->mt[mti-1] >> 62 ) ) + mti );
  state->mti = RAND64_ARRAYSIZE;
  return;
}

void rand64SeedArray( rand64State *state, uint64_t *seedarray, uint64_t arraycount )
{
  uint64_t i, j, k;

  rand64Seed( state, 19650218ULL );
  i = 1;
  j = 0;
  k = ( RAND64_ARRAYSIZE > arraycount ? RAND64_ARRAYSIZE : arraycount );
  for( ; k ; k-- )
  {
    state->mt[i] = ( state->mt[i] ^ ( ( state->mt[i-1] ^ ( state->mt[i-1] >> 62 ) ) * 3935559000370003845ULL ) ) + seedarray[j] + j;
    if( ++i >= RAND64_ARRAYSIZE )
    {
      state->mt[0] = state->mt[RAND64_ARRAYSIZE-1];
      i = 1;
    }
    if( ++j >= arraycount )
      j = 0;
  }
  for( k = RAND64_ARRAYSIZE-1 ; k ; k-- )
  {
    state->mt[i] = ( state->mt[i] ^ ( ( state->mt[i-1] ^ ( state->mt[i-1] >> 62 ) ) * 2862933555777941757ULL ) ) - i;
    if( ++i >= RAND64_ARRAYSIZE )
    {
      state->mt[0] = state->mt[RAND64_ARRAYSIZE-1];
      i = 1;
    }
  }
  state->mt[0] = 1ULL << 63;

  return;
}



uint64_t rand64Int( rand64State *state )
{
  int i;
  uint64_t x;
  static uint64_t mag01[2] = { 0, RAND64_MATRIX };

  if( state->mti >= RAND64_ARRAYSIZE )
  {
    for( i = 0 ; i < RAND64_ARRAYSIZE - RAND64_ARRAYHALF ; i++ )
    {
      x = ( state->mt[i] & RAND64_UPMASK ) | ( state->mt[i+1] & RAND64_LOWMASK );
      state->mt[i] = state->mt[i+RAND64_ARRAYHALF] ^ ( x >> 1 ) ^ mag01[ x & 1 ];
    }
    for( ; i < RAND64_ARRAYSIZE - 1 ; i++ )
    {
      x = ( state->mt[i] & RAND64_UPMASK ) | ( state->mt[i+1] & RAND64_LOWMASK );
      state->mt[i] = state->mt[i+(RAND64_ARRAYHALF-RAND64_ARRAYSIZE)] ^ ( x >> 1 ) ^ mag01[ x & 1 ];
    }
    x = ( state->mt[RAND64_ARRAYSIZE-1] & RAND64_UPMASK ) | ( state->mt[0] & RAND64_LOWMASK );
    state->mt[RAND64_ARRAYSIZE-1] = state->mt[RAND64_ARRAYHALF-1] ^ ( x >> 1 ) ^ mag01[ x & 1 ];
    state->mti = 0;
  }

  x = state->mt[ state->mti++ ];
  x ^= ( x >> 29 ) & ((uint64_t)0x5555555555555555ULL);
  x ^= ( x << 17 ) & ((uint64_t)0x71D67FFFEDA60000ULL);
  x ^= ( x << 37 ) & ((uint64_t)0xFFF7EEE000000000ULL);
  x ^= ( x >> 43 );
  return x;
}

double rand64Double( rand64State *state )
{
  return (double)( rand64Int( state ) >> 11 ) * (double)(1.0/9007199254740991.0);
}

float rand64Float( rand64State *state )
{
  return (float)( rand64Int( state ) >> 11 ) * (float)(1.0/9007199254740991.0);
}

float rand64FloatBell( rand64State *state, float exponent, float lowrange, float highrange )
{
  float f;
  uint64_t i;
  i = rand64Int( state );
  f = (float)( i >> 11 ) * (float)(1.0/9007199254740991.0);
  f = powf( f, exponent );
  if( i & 0x1 )
    f = highrange * ( 1.0 - f );
  else
    f = lowrange * ( f - 1.0 );
  return f;
}

int64_t rand64FloatRound( rand64State *state, float value )
{
  int64_t i;
  float base, disb;
  base = floorf( value );
  disb = value - base;
  i = (int64_t)value;
  if( ( (float)( rand64Int( state ) >> 11 ) * (float)(1.0/9007199254740991.0) ) < disb )
    i++;
  return i;
}



void rand64SourceInit( rand64Source *source, rand64State *state )
{
  source->state = state;
  source->value = rand64Int( source->state );
  source->bitindex = 0;
  return;
}

uint64_t rand64SourceBits( rand64Source *source, int bits )
{
  int freebits;
  uint64_t x;

  freebits = 64 - source->bitindex;
  if( bits < freebits )
  {
    x = source->value >> source->bitindex;
    x &= ( (uint64_t)1 << bits ) - 1;
    source->bitindex += bits;
  }
  else
  {
    x = source->value >> source->bitindex;
    source->bitindex = bits - freebits;
    source->value = rand64Int( source->state );
    x |= ( source->value & ( ( (uint64_t)1 << source->bitindex ) - 1 ) ) << freebits;
  }

  return x;
}




////




void rand32Seed( rand32State *state, uint32_t seed )
{
  int mti;
  state->mt[0] = seed;
  for( mti = 1 ; mti < RAND32_ARRAYSIZE ; mti++ )
    state->mt[mti] = ( ((uint32_t)1812433253UL) * ( state->mt[mti-1] ^ ( state->mt[mti-1] >> 30 ) ) + mti );
  state->mti = RAND32_ARRAYSIZE;
  return;
}

void rand32SeedArray( rand32State *state, uint32_t *seedarray, int arraycount )
{
  int i, j, k;

  rand32Seed( state, 19650218UL );
  i = 1;
  j = 0;
  k = ( RAND32_ARRAYSIZE > arraycount ? RAND32_ARRAYSIZE : arraycount );
  for( ; k; k-- )
  {
    state->mt[i] = ( state->mt[i] ^ ( ( state->mt[i-1] ^ ( state->mt[i-1] >> 30 ) ) * 1664525UL ) ) + seedarray[j] + j;
    if( ++i >= RAND32_ARRAYSIZE )
    {
      state->mt[0] = state->mt[RAND32_ARRAYSIZE-1];
      i = 1;
    }
    if( ++j >= arraycount )
      j = 0;
  }
  for( k = RAND32_ARRAYSIZE-1 ; k ; k-- )
  {
    state->mt[i] = ( state->mt[i] ^ ( ( state->mt[i-1] ^ ( state->mt[i-1] >> 30 ) ) * 1566083941UL ) ) - i;
    if( ++i >= RAND32_ARRAYSIZE )
    {
      state->mt[0] = state->mt[RAND32_ARRAYSIZE-1];
      i=1;
    }
  }
  state->mt[0] = 0x80000000UL;

  return;
}

uint32_t rand32Int( rand32State *state )
{
  int i;
  uint32_t x;
  static uint32_t mag01[2] = { 0x0, RAND32_MATRIX };

  if( state->mti >= RAND32_ARRAYSIZE )
  {
    for( i = 0 ; i < RAND32_ARRAYSIZE - RAND32_ARRAYHALF ; i++ )
    {
      x = ( state->mt[i] & RAND32_UPMASK ) | ( state->mt[i+1] & RAND32_LOWMASK );
      state->mt[i] = state->mt[i+RAND32_ARRAYHALF] ^ ( x >> 1 ) ^ mag01[ x & 0x1 ];
    }
    for( ; i < RAND32_ARRAYSIZE - 1 ; i++ )
    {
      x = ( state->mt[i] & RAND32_UPMASK ) | ( state->mt[i+1] & RAND32_LOWMASK );
      state->mt[i] = state->mt[i+(RAND32_ARRAYHALF-RAND32_ARRAYSIZE)] ^ ( x >> 1 ) ^ mag01[ x & 0x1 ];
    }
    x = ( state->mt[RAND32_ARRAYSIZE-1] & RAND32_UPMASK ) | ( state->mt[0] & RAND32_LOWMASK );
    state->mt[RAND32_ARRAYSIZE-1] = state->mt[RAND32_ARRAYHALF-1] ^ ( x >> 1 ) ^ mag01[ x & 0x1 ];
    state->mti = 0;
  }

  x = state->mt[ state->mti++ ];
  x ^= ( x >> 11 );
  x ^= ( x << 7 ) & ((uint32_t)0x9d2c5680UL);
  x ^= ( x << 15 ) & ((uint32_t)0xefc60000UL);
  x ^= ( x >> 18 );
  return x;
}

double rand32Double( rand32State *state )
{
  return (double)rand32Int( state ) * (double)(1.0/4294967295.0);
}

float rand32Float( rand32State *state )
{
  return (float)rand32Int( state ) * (float)(1.0/4294967295.0);
}

float rand32FloatBell( rand32State *state, float exponent, float lowrange, float highrange )
{
  float f;
  uint32_t i;
  i = rand32Int( state );
  f = (float)( i >> 1 ) * (float)(1.0/2147483647.0);
  f = powf( f, exponent );
  if( i & 0x1 )
    f = highrange * ( 1.0 - f );
  else
    f = lowrange * ( f - 1.0 );
  return f;
}


int32_t rand32FloatRound( rand32State *state, float value )
{
  int32_t i;
  float base, disb;
  base = floorf( value );
  disb = value - base;
  i = (int32_t)value;
  if( ( (float)rand32Int( state ) * (float)(1.0/4294967295.0) ) < disb )
    i++;
  return i;
}



void rand32SourceInit( rand32Source *source, rand32State *state )
{
  source->state = state;
  source->value = rand32Int( source->state );
  source->bitindex = 0;
  return;
}

uint32_t rand32SourceBits( rand32Source *source, int bits )
{
  int freebits;
  uint32_t x;

  freebits = 32 - source->bitindex;
  if( bits < freebits )
  {
    x = source->value >> source->bitindex;
    x &= ( 1 << bits ) - 1;
    source->bitindex += bits;
  }
  else
  {
    x = source->value >> source->bitindex;
    source->bitindex = bits - freebits;
    source->value = rand32Int( source->state );
    x |= ( source->value & ( ( 1 << source->bitindex ) - 1 ) ) << freebits;
  }

  return x;
}




////




int randTrueRandom( void *random, int size )
{
  FILE *file;
  if( !( file = fopen( "/dev/urandom", "rb" ) ) )
    return 0;
  size = fread( random, 1, size, file );
  if( size < 0 )
    size = 0;
  fclose( file );
  return size;
}



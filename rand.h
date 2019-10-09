/*
   A C-program for MT19937-64 (2004/9/29 version).
   Coded by Takuji Nishimura and Makoto Matsumoto.

   This is a 64-bit version of Mersenne Twister pseudorandom number
   generator.

   Before using, initialize the state by using init_genrand64(seed)
   or init_by_array64(init_key, key_length).

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



#define RAND64_ARRAYSIZE 312
#define RAND64_ARRAYHALF 156
#define RAND64_MATRIX ((uint64_t)0xB5026F5AA96619E9ULL)
#define RAND64_UPMASK ((uint64_t)0xFFFFFFFF80000000ULL)
#define RAND64_LOWMASK ((uint64_t)0x7FFFFFFFULL)


typedef struct
{
  uint64_t mt[RAND64_ARRAYSIZE];
  int mti;
} rand64State;

void rand64Seed( rand64State *state, uint64_t seed );
void rand64SeedArray( rand64State *state, uint64_t *seedarray, uint64_t arraycount );
uint64_t rand64Int( rand64State *state );
double rand64Double( rand64State *state );
float rand64Float( rand64State *state );
float rand64FloatBell( rand64State *state, float exponent, float lowrange, float highrange );
int64_t rand64FloatRound( rand64State *state, float value );


////


typedef struct
{
  rand64State *state;
  uint64_t value;
  int bitindex;
} rand64Source;

void rand64SourceInit( rand64Source *source, rand64State *state );
uint64_t rand64SourceBits( rand64Source *source, int bits );



////////



#define RAND32_ARRAYSIZE 624
#define RAND32_ARRAYHALF 397
#define RAND32_MATRIX ((uint32_t)0x9908b0dfUL)
#define RAND32_UPMASK ((uint32_t)0x80000000UL)
#define RAND32_LOWMASK ((uint32_t)0x7fffffffUL)

typedef struct
{
  uint32_t mt[RAND32_ARRAYSIZE];
  int mti;
} rand32State;

void rand32Seed( rand32State *state, uint32_t seed );
void rand32SeedArray( rand32State *state, uint32_t *seedarray, int arraycount );
uint32_t rand32Int( rand32State *state );
double rand32Double( rand32State *state );
float rand32Float( rand32State *state );
float rand32FloatBell( rand32State *state, float exponent, float lowrange, float highrange );
int32_t rand32FloatRound( rand32State *state, float value );


////


typedef struct
{
  rand32State *state;
  uint32_t value;
  int bitindex;
} rand32Source;

void rand32SourceInit( rand32Source *source, rand32State *state );
uint32_t rand32SourceBits( rand32Source *source, int bits );



////////



#if CPUCONF_WORD_SIZE >= 64
 #define RAND_BITS 64
 #define randState rand64State
 #define randSeed(a,b) rand64Seed(a,b)
 #define randSeedArray(a,b,c) rand64SeedArray(a,b,c)
 #define randInt(a) rand64Int(a)
 #define randInt64(a) rand64Int(a)
 #define randFloat(a) rand64Float(a)
 #define randFloatRound(a,b) rand64FloatRound(a,b)
 #define intrand uint64_t
 #define randSource rand64Source
 #define randSourceInit rand64SourceInit
 #define randSourceBits rand64SourceBits
#else
 #define RAND_BITS 32
 #define randState rand32State
 #define randSeed(a,b) rand32Seed(a,b)
 #define randSeedArray(a,b,c) rand32SeedArray(a,b,c)
 #define randInt(a) rand32Int(a)
 #define randInt64(a) ((((uint64_t)rand32Int(a))<<32)|((uint64_t)rand32Int(a)))
 #define randFloat(a) rand32Float(a)
 #define randFloatRound(a,b) rand32FloatRound(a,b)
 #define intrand uint32_t
 #define randSource rand32Source
 #define randSourceInit rand32SourceInit
 #define randSourceBits rand32SourceBits
#endif


int randTrueRandom( void *random, int size );



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


#include "crypthash.h"


static const uint32_t cryptTable[64] =
{
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
  0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
  0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
  0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
  0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
  0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
  0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
  0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
  0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
  0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define Ch(x,y,z) ((x&y)^(~x&z))
#define Maj(x,y,z) ((x&y)^(x&z)^(y&z))
#define S(x,n) (((x)>>((n)&31))|((x)<<(32-((n)&31))))
#define R(x,n) ((x)>>(n))
#define Sigma0(x) (S(x,2)^S(x,13)^S(x,22))
#define Sigma1(x) (S(x,6)^S(x,11)^S(x,25))
#define Gamma0(x) (S(x,7)^S(x,18)^R(x,3))
#define Gamma1(x) (S(x,17)^S(x,19)^R(x,10))

static void cryptCompress( cryptHashState *state )
{
  int i;
  uint32_t s[8], w[64], t0, t1;
  for( i = 0 ; i < 8 ; i++ )
    s[i] = state->state[i];
  for( i = 0 ; i < 16 ; i++ )
    w[i] = ( ( (uint32_t)state->buf[(4*i)+0] ) << 24 ) | ( ( (uint32_t) state->buf[(4*i)+1] ) << 16 ) | ( ( (uint32_t) state->buf[(4*i)+2]) << 8 ) | ( ( (uint32_t) state->buf[(4*i)+3] ) );
  for( i = 16 ; i < 64 ; i++ )
    w[i] = Gamma1(w[i - 2]) + w[i - 7] + Gamma0(w[i - 15]) + w[i - 16];
  for( i = 0 ; i < 64 ; i++ )
  {
    t0 = s[7] + Sigma1( s[4] ) + Ch( s[4], s[5], s[6] ) + cryptTable[i] + w[i];
    t1 = Sigma0( s[0] ) + Maj( s[0], s[1], s[2] );
    s[7] = s[6];
    s[6] = s[5];
    s[5] = s[4];
    s[4] = s[3] + t0;
    s[3] = s[2];
    s[2] = s[1];
    s[1] = s[0];
    s[0] = t0 + t1;
  }
  for( i = 0 ; i < 8 ; i++ )
    state->state[i] += s[i];
  return;
}


void cryptHashInit( cryptHashState *state )
{
  state->curlen = state->length = 0;
  state->state[0] = 0x6A09E667;
  state->state[1] = 0xBB67AE85;
  state->state[2] = 0x3C6EF372;
  state->state[3] = 0xA54FF53A;
  state->state[4] = 0x510E527F;
  state->state[5] = 0x9B05688C;
  state->state[6] = 0x1F83D9AB;
  state->state[7] = 0x5BE0CD19;
  return;
}

void cryptHashData( cryptHashState *state, void *data, int datalength )
{
  uint8_t *d;
  d = data;
  for( ; datalength-- ; )
  {
    state->buf[state->curlen++] = *d++;
    if( state->curlen < 64 )
      continue;
    cryptCompress( state );
    state->length += 512;
    state->curlen = 0;
  }
  return;
}

void cryptHashResult( cryptHashState *state, uint8_t *hash )
{
  int i;
  state->length += state->curlen * 8;
  state->buf[state->curlen++] = 0x80;
  if( state->curlen >= 56 )
  {
    for( ; state->curlen < 64 ; )
      state->buf[state->curlen++] = 0;
    cryptCompress( state );
    state->curlen = 0;
  }
  for( ; state->curlen < 56 ; )
    state->buf[state->curlen++] = 0;
  for( i = 56 ; i < 60 ; i++ )
    state->buf[i] = 0;
  for( i = 60 ; i < 64 ; i++ )
    state->buf[i] = state->length >> ( ( 63 - i ) * 8 );
  cryptCompress( state );
  for( i = 0 ; i < 32 ; i++ )
    hash[i] = state->state[ i >> 2 ] >> ( ( ( 3 - i ) & 3 ) << 3 );
  return;
}


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



#include "cryptsha1.h"

#define SHA1CircularShift(bits,word) (((word)<<(bits))|((word)>>(32-(bits))))


static void SHA1ProcessMessageBlock( cryptSha1 *context )
{
  const uint32_t K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
  int t;
  uint32_t temp;
  uint32_t W[80];
  uint32_t A, B, C, D, E;

  for( t = 0 ; t < 16 ; t++ )
  {
    W[t] = ((uint32_t) context->block[t * 4]) << 24;
    W[t] |= ((uint32_t) context->block[t * 4 + 1]) << 16;
    W[t] |= ((uint32_t) context->block[t * 4 + 2]) << 8;
    W[t] |= ((uint32_t) context->block[t * 4 + 3]);
  }

  for( t = 16 ; t < 80 ; t++ )
    W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

  A = context->hashdigest[0];
  B = context->hashdigest[1];
  C = context->hashdigest[2];
  D = context->hashdigest[3];
  E = context->hashdigest[4];

  for( t = 0 ; t < 20 ; t++ )
  {
    temp = SHA1CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }
  for( t = 20 ; t < 40 ; t++ )
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }
  for( t = 40 ; t < 60 ; t++ )
  {
    temp = SHA1CircularShift(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }
  for( t = 60 ; t < 80 ; t++ )
  {
    temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
    E = D;
    D = C;
    C = SHA1CircularShift(30,B);
    B = A;
    A = temp;
  }

  context->hashdigest[0] += A;
  context->hashdigest[1] += B;
  context->hashdigest[2] += C;
  context->hashdigest[3] += D;
  context->hashdigest[4] += E;
  context->blockindex = 0;
  return;
}


static void SHA1PadMessage(cryptSha1 *context)
{
  if( context->blockindex > 55 )
  {
    context->block[context->blockindex++] = 0x80;
    while( context->blockindex < 64 )
      context->block[context->blockindex++] = 0;
    SHA1ProcessMessageBlock( context );
    while( context->blockindex < 56 )
      context->block[context->blockindex++] = 0;
  }
  else
  {
    context->block[context->blockindex++] = 0x80;
    while( context->blockindex < 56 )
      context->block[context->blockindex++] = 0;
  }
  context->block[56] = ( context->length >> 56 ) & 0xFF;
  context->block[57] = ( context->length >> 48 ) & 0xFF;
  context->block[58] = ( context->length >> 40 ) & 0xFF;
  context->block[59] = ( context->length >> 32 ) & 0xFF;
  context->block[60] = ( context->length >> 24 ) & 0xFF;
  context->block[61] = ( context->length >> 16 ) & 0xFF;
  context->block[62] = ( context->length >> 8 ) & 0xFF;
  context->block[63] = ( context->length ) & 0xFF;
  SHA1ProcessMessageBlock( context );
  return;
}


////


void cryptInitSha1( cryptSha1 *context )
{
  context->length = 0;
  context->blockindex = 0;
  context->hashdigest[0] = 0x67452301;
  context->hashdigest[1] = 0xEFCDAB89;
  context->hashdigest[2] = 0x98BADCFE;
  context->hashdigest[3] = 0x10325476;
  context->hashdigest[4] = 0xC3D2E1F0;
  return;
}

void cryptDataSha1( cryptSha1 *context, const unsigned char *data, size_t length )
{
  if( !( length ) )
    return;
  while( length-- )
  {
    context->block[ context->blockindex++ ] = *data;
    context->length += 8;
    if( context->blockindex == 64 )
      SHA1ProcessMessageBlock(context);
    data++;
  }
  return;
}

void cryptResultSha1( cryptSha1 *context, uint32_t *hashdigest )
{
  SHA1PadMessage( context );
  memcpy( hashdigest, context->hashdigest, 5*sizeof(uint32_t) );
  return;
}




#if 0

int main()
{
  cryptSha1 context;
  uint32_t hashdigest[5];

  cryptInitSha1( &context );
  cryptDataSha1( &context, "3737", 4 );
  cryptResultSha1( &context, hashdigest );

  printf( "RESULT : 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n", context.hashdigest[0], context.hashdigest[1], context.hashdigest[2], context.hashdigest[3], context.hashdigest[4] );

  return 1;
}

#endif



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
#include <time.h>

#include "cpuconfig.h"

#include "cc.h"
#include "ccstr.h"
#include "rand.h"


////


#define BN_XP_BITS 1024
#define BN_XP_SUPPORT_128 0
#define BN_XP_SUPPORT_192 0
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 1
#define BN_XP_SUPPORT_1024 1
#include "bn.h"


////


#include "rsabn.h"


////



/*
gcc -std=gnu99 -m64 cpuconf.c cpuinfo.c -O2 -s -o cpuconf
./cpuconf -h
gcc rsagen.c rsabn.c rand.c cc.c bn128.c bn192.c bn256.c bn512.c bn1024.c bnstdio.c -g -O2 -mtune=nocona -ffast-math -fomit-frame-pointer -msse -msse2 -msse3 -o rsagen -lm -Wall

gcc -std=gnu99 -m32 cpuconf.c cpuinfo.c -O2 -s -o cpuconf
./cpuconf -h
gcc -m32 rsagen.c rsabn.c rand.c cc.c bn128.c bn192.c bn256.c bn512.c bn1024.c bnstdio.c -g -O2 -mtune=nocona -ffast-math -fomit-frame-pointer -msse -msse2 -msse3 -o rsagen -lm -Wall
*/



/* Can not exceed RSA_DATA_BITS-2 */
#define RSA_INPUT_BITS (RSA_DATA_BITS-2)

#define RSA_MSG_LENGTH (4)

#define RSA_PRINT_BUFFER_SIZE (512)


////


void bnprint1024x64( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 1024
  bnXpShrRound( &b, bn, BN_XP_BITS-1024 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%016llx", (long long)bnXpExtract64( &b, i ) );
    i += 64;
    if( i >= 1024 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}

void bnprint1024x32( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 1024
  bnXpShrRound( &b, bn, BN_XP_BITS-1024 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%08x", (int)bnXpExtract32( &b, i ) );
    i += 32;
    if( i >= 1024 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}


////


static rsaType rsaSetRandom( rand32State *randstate )
{
  int i;
  rsaType n;
  for( i = 0 ; i < BN_XP_BITS/BN_UNIT_BITS ; i++ )
  {
#if BN_UNIT_BITS == 64
    n.unit[i] = ( (uint64_t)rand32Int( randstate ) ) | ( ( (uint64_t)rand32Int( randstate ) ) << 32 );
#elif BN_UNIT_BITS == 32
    n.unit[i] = rand32Int( randstate );
#else
 #error
#endif
  }
  return n;
}


int testRsaKeys( rand32State *randstate, rsaType rsakey, rsaType rsainvkey, rsaType rsaproduct )
{
  int i, length;
  rsaType input[RSA_MSG_LENGTH], inputmask;
  rsaType encoded[RSA_MSG_LENGTH];
  rsaType decoded[RSA_MSG_LENGTH];
  char printbuffer[RSA_PRINT_BUFFER_SIZE];

  /* Build some input */
  length = RSA_MSG_LENGTH;
  printf( "\n" );
  printf( "Input: " );
  bnXpSet32Shl( &inputmask, 1, RSA_INPUT_BITS );
  bnXpSub32( &inputmask, 1 );
  for( i = 0; i < length ; i++ )
  {
    input[i] = rsaSetRandom( randstate );
    bnXpAnd( &input[i], &inputmask );
    bnXpPrintHex( &input[i], printbuffer, RSA_PRINT_BUFFER_SIZE, 0, 0, 0 );
    printf( "0x%s,", printbuffer );
  }
  printf( "\n\n" );

  rsaEncrypt( input, encoded, length, rsakey, rsaproduct );

#if 1
  printf( "Encypted: " );
  for( i = 0 ; i < length ; i++ )
  {
    bnXpPrintHex( &encoded[i], printbuffer, RSA_PRINT_BUFFER_SIZE, 0, 0, 0 );
    printf( "0x%s,", printbuffer );
  }
  printf( "\n\n" );
#endif

  rsaEncrypt( encoded, decoded, length, rsainvkey, rsaproduct );

#if 1
  printf( "Decypted: " );
  for( i = 0 ; i < length ; i++ )
  {
    bnXpPrintHex( &decoded[i], printbuffer, RSA_PRINT_BUFFER_SIZE, 0, 0, 0 );
    printf( "0x%s,", printbuffer );
  }
  printf( "\n\n" );
#endif

  /* Verify */
  for( i = 0 ; i < length ; i++ )
  {
    if( bnXpCmpNeq( &input[i], &decoded[i] ) )
      exit( 1 );
  }

#if 1
  printf( "Success!\n\n" );
#endif

  return 0;
}





int main()
{
  int i, j;
  rand32State randstate;
  rsaType rsaproduct;
  rsaType rsakey, rsainvkey;

  rand32Seed( &randstate, time( 0 ) );
  j = ( time( 0 ) + rand32Int( &randstate ) ) & 0xffff;
  for( i = 0 ; i < j ; i++ )
    rand32Int( &randstate );
  j = ( time( 0 ) + rand32Int( &randstate ) ) & 0xffff;
  for( i = 0 ; i < j ; i++ )
    rand32Int( &randstate );

  printf( "Init\n" );
  printf( "\n" );

  /* Generate brand new encryption keys */
  rsaGenKeys( &randstate, RSA_INPUT_BITS, &rsakey, &rsainvkey, &rsaproduct );


  printf( "/* ==== 64 bits ==== */\n" );
  printf( "\n" );
  bnprint1024x64( &rsakey, " bsEncryptionPublicKey = { ", " };\n" );
  printf( "\n" );
  bnprint1024x64( &rsainvkey, " bsEncryptionPrivateKey = { ", " };\n" );
  printf( "\n" );
  bnprint1024x64( &rsaproduct, " bsEncryptionProduct = { ", " };\n" );
  printf( "\n" );
  printf( "\n" );

  printf( "/* ==== 32 bits ==== */\n" );
  printf( "\n" );
  bnprint1024x32( &rsakey, " bsEncryptionPublicKey = { ", " };\n" );
  printf( "\n" );
  bnprint1024x32( &rsainvkey, " bsEncryptionPrivateKey = { ", " };\n" );
  printf( "\n" );
  bnprint1024x32( &rsaproduct, " bsEncryptionProduct = { ", " };\n" );
  printf( "\n" );
  printf( "\n" );


  printf( "/* ==== Testing Keys ==== */\n" );

  testRsaKeys( &randstate, rsakey, rsainvkey, rsaproduct );

  printf( "\n" );
  printf( "Done\n" );

  return 1;
}




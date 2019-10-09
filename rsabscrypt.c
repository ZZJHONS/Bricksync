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
gcc rsabscrypt.c rsabn.c rand.c cc.c bn128.c bn192.c bn256.c bn512.c bn1024.c bnstdio.c -g -O2 -mtune=nocona -ffast-math -fomit-frame-pointer -msse -msse2 -msse3 -o rsabscrypt -lm -Wall

gcc -std=gnu99 -m32 cpuconf.c cpuinfo.c -O2 -s -o cpuconf
./cpuconf -h
gcc -m32 rsabscrypt.c rsabn.c rand.c cc.c bn128.c bn192.c bn256.c bn512.c bn1024.c bnstdio.c -g -O2 -mtune=nocona -ffast-math -fomit-frame-pointer -msse -msse2 -msse3 -o rsabscrypt -lm -Wall
*/

/*
DadsAFOL
INFO: Registration Code: cc7a1120912cc13a3e41c7dede1262ef

*/


/* Fixed equal to our RSA math bit width */
#define BS_ENCRYPTION_SECRET_OFFSET_BITS (24)

/* Multiples of 4 */
#define BS_REGISTRATION_CODE_BYTES (16)
#define BS_REGISTRATION_CODE_BITS (8*BS_REGISTRATION_CODE_BYTES) /* 128 */
#define BS_REGISTRATION_CODE_CHARS (2*BS_REGISTRATION_CODE_BYTES)
#define BS_REGISTRATION_KEY_BYTES (128)
#define BS_REGISTRATION_KEY_BITS (8*BS_REGISTRATION_KEY_BYTES) /* 1024 */
#define BS_REGISTRATION_KEY_CHARS (2*BS_REGISTRATION_KEY_BYTES)


/* Secret offset to be decrypted by registration key */
#define BS_REGISTRATION_SECRET_OFFSET (0x9a6fc)


#if CPUCONF_WORD_SIZE >= 64

static const bn1024 bsEncryptionPublicKey =
{
  .unit =
  {
0x06682574cb8f45b5, 0x3ea0fefd00cea2bf, 0xc9abf335a3e140e0, 0xe41b7ea20b2f610a, 0xe900c2b0793f36a5, 0x5ec4e33430d112be, 0x06a392d6b7a69937, 0x7003e7b696fb6d84, 0x5c18f9a63a54d453, 0xbcabcc49e9fad6a1, 0x2c334308f94fd129, 0x41200681184f76c4, 0x3d17e38c223ec645, 0x05cf3b79918065f7, 0xbb0e2f197559bf53, 0x4037e120b15bba41
  }
};

static const bn1024 bsEncryptionPrivateKey =
{
  .unit =
  {
0x6ed9b1d3e3761f5d, 0x6065850b9725a272, 0xcf138e38ddd7b0df, 0xbbaca4640ab0f96c, 0xf190b6464c077a90, 0x4dcd7b215e06149a, 0x3db23500f2591164, 0x3bfd9bb0a85a900b, 0xd4a3aa6f60833e9d, 0xb0b26c61d7b86ec9, 0x218eaf000c9b7e50, 0xafbde89396a4ca03, 0xf4ef327446a3f2ac, 0xf641d1fc1581212a, 0x24c044e05de934cf, 0x64818af627a3a174
  }
};

static const bn1024 bsEncryptionProduct =
{
  .unit =
  {
0x82697a2d0d1ddeb3, 0x8e836c795885ca4d, 0xbd949b3ab0d5c95e, 0x1fd5e1e528ee1062, 0x529d2f4f7b62d91e, 0x35414cd54307aa8c, 0x8a3970ad86b1012b, 0x932700d92d2fb469, 0x74f8b1fd67029bef, 0x69f07cdf053563aa, 0x0af3e86c730e14ad, 0xce080686a40b2e29, 0x9fb879259197f0d2, 0x0b7319990564b315, 0x7b2d5572d4309cf5, 0x66f42a9e1702dcb5
  }
};

#else

static const bn1024 bsEncryptionPublicKey =
{
  .unit =
  {
0xcb8f45b5, 0x06682574, 0x00cea2bf, 0x3ea0fefd, 0xa3e140e0, 0xc9abf335, 0x0b2f610a, 0xe41b7ea2, 0x793f36a5, 0xe900c2b0, 0x30d112be, 0x5ec4e334, 0xb7a69937, 0x06a392d6, 0x96fb6d84, 0x7003e7b6, 0x3a54d453, 0x5c18f9a6, 0xe9fad6a1, 0xbcabcc49, 0xf94fd129, 0x2c334308, 0x184f76c4, 0x41200681, 0x223ec645, 0x3d17e38c, 0x918065f7, 0x05cf3b79, 0x7559bf53, 0xbb0e2f19, 0xb15bba41, 0x4037e120
  }
};

static const bn1024 bsEncryptionPrivateKey =
{
  .unit =
  {
0xe3761f5d, 0x6ed9b1d3, 0x9725a272, 0x6065850b, 0xddd7b0df, 0xcf138e38, 0x0ab0f96c, 0xbbaca464, 0x4c077a90, 0xf190b646, 0x5e06149a, 0x4dcd7b21, 0xf2591164, 0x3db23500, 0xa85a900b, 0x3bfd9bb0, 0x60833e9d, 0xd4a3aa6f, 0xd7b86ec9, 0xb0b26c61, 0x0c9b7e50, 0x218eaf00, 0x96a4ca03, 0xafbde893, 0x46a3f2ac, 0xf4ef3274, 0x1581212a, 0xf641d1fc, 0x5de934cf, 0x24c044e0, 0x27a3a174, 0x64818af6
  }
};

static const bn1024 bsEncryptionProduct =
{
  .unit =
  {
0x0d1ddeb3, 0x82697a2d, 0x5885ca4d, 0x8e836c79, 0xb0d5c95e, 0xbd949b3a, 0x28ee1062, 0x1fd5e1e5, 0x7b62d91e, 0x529d2f4f, 0x4307aa8c, 0x35414cd5, 0x86b1012b, 0x8a3970ad, 0x2d2fb469, 0x932700d9, 0x67029bef, 0x74f8b1fd, 0x053563aa, 0x69f07cdf, 0x730e14ad, 0x0af3e86c, 0xa40b2e29, 0xce080686, 0x9197f0d2, 0x9fb87925, 0x0564b315, 0x0b731999, 0xd4309cf5, 0x7b2d5572, 0x1702dcb5, 0x66f42a9e
  }
};

#endif


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

void bnprint1024hex( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 1024
  bnXpShrRound( &b, bn, BN_XP_BITS-1024 );
  printf( "%s", prefix );
  for( i = 1024-64 ; i >= 0 ; i -= 64 )
  {
    printf( "%016llx", (long long)bnXpExtract64( &b, i ) );
  }
  printf( "%s", suffix );
#endif
  return;
}


////


int main( int argc, char **argv )
{
  int i, hex, hexindex, codestringlen, unitindex, unitshift;
  char *regcodestring;
  bn1024 regcode, regfullcode, regkey, bnsecretoffset;

  regcodestring = 0;
  if( argc >= 2 )
    regcodestring = argv[1];
  codestringlen = 0;
  if( regcodestring )
    codestringlen = strlen( regcodestring );

  /* Quick check on code length */
  if( codestringlen != BS_REGISTRATION_CODE_CHARS )
  {
    printf( "\n" );
    printf( "Invalid registration code! The code counts %d characters but should be %d characters long.\n", (int)codestringlen, (int)BS_REGISTRATION_CODE_CHARS );
    return 0;
  }

  /* Build registration code */
  memset( &regcode, 0, sizeof(bn1024) );
  for( i = 0 ; i < BS_REGISTRATION_CODE_CHARS ; i++ )
  {
    hex = ccCharHexBase( regcodestring[i] );
    if( hex < 0 )
    {
      printf( "\n" );
      printf( "Invalid registration code! The code's base encoding should be hexadecimal.\n" );
      return 0;
    }
    hexindex = (BS_REGISTRATION_CODE_CHARS-1) - i;
    unitindex = hexindex >> (BN_UNIT_BYTE_SHIFT+1);
    unitshift = ( hexindex << 2 ) & (BN_UNIT_BITS-1);
    regcode.unit[ unitindex ] |= ((bnUnit)hex) << unitshift;
  }

  /* Append secret offset */
  bn1024Set32Shl( &bnsecretoffset, BS_REGISTRATION_SECRET_OFFSET, BS_REGISTRATION_CODE_BITS );
  bn1024SetOr( &regfullcode, &regcode, &bnsecretoffset );



  /* Encrypt */
  rsaEncrypt( &regfullcode, &regkey, 1, bsEncryptionPrivateKey, bsEncryptionProduct );


  bnprint1024x64( &regcode, "Registration Code : { ", " };\n" );
  printf( "\n" );
  bnprint1024x64( &regfullcode, "Registration Full Code : { ", " };\n" );
  printf( "\n" );
  bnprint1024x64( &regkey, "Registration Key : { ", " };\n" );
  printf( "\n" );
  printf( "\n==== REGISTRATION KEY ====\n\n" );
#if 0
  bn1024PrintHexOut( "registrationkey = \"", &regkey, "\";\n" );
#else
  bnprint1024hex( &regkey, "registrationkey = \"", "\";\n" );
#endif
  printf( "\n==== REGISTRATION KEY ====\n\n" );
  printf( "\n" );


////


  bn1024 regverify, bnmask;
  uint32_t secretoffset;


  /* Verification */
  rsaEncrypt( &regkey, &regverify, 1, bsEncryptionPublicKey, bsEncryptionProduct );

  bnprint1024x64( &regverify, "Verification : { ", " };\n" );
  printf( "\n" );

  secretoffset = bn1024Extract32( &regverify, BS_REGISTRATION_CODE_BITS ) & ((1<<BS_ENCRYPTION_SECRET_OFFSET_BITS)-1);

  bn1024Set32Shl( &bnmask, (1<<BS_ENCRYPTION_SECRET_OFFSET_BITS)-1, BS_REGISTRATION_CODE_BITS );
  bn1024Not( &bnmask );
  bn1024And( &regverify, &bnmask );

  bnprint1024x64( &regverify, "Verification : { ", " };\n" );
  printf( "\n" );
  printf( "Secret Offset : 0x%x\n", (int)secretoffset );


  if( bn1024CmpEq( &regverify, &regcode ) )
    printf( "SUCCESS!\n" );
  else
    printf( "\n=== !!! ERROR !!! ERROR !!! ERROR !!! ===\n\n" );

  return 1;
}






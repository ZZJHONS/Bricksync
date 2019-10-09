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

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"
#include "iolog.h"
#include "debugtrack.h"
#include "cpuinfo.h"
#include "antidebug.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <direct.h>
#else
 #error Unknown/Unsupported platform!
#endif

#include "tcp.h"
#include "tcphttp.h"
#include "oauth.h"
#include "exclperm.h"
#include "journal.h"

#include "bsx.h"
#include "bsxpg.h"
#include "json.h"
#include "bsorder.h"
#include "bricklink.h"
#include "brickowl.h"
#include "colortable.h"
#include "bstranslation.h"

#include "bricksync.h"
#include "bsantidebug.h"

#include "crypthash.h"

#define BN_XP_SUPPORT_128 0
#define BN_XP_SUPPORT_192 0
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 0
#define BN_XP_SUPPORT_1024 1
#include "bn.h"

#include "rsabn.h"


////


/* Fixed equal to our RSA math bit width */
#define BS_ENCRYPTION_KEY_BITS (1024)

/* Fixed equal to our RSA math bit width */
#define BS_ENCRYPTION_SECRET_OFFSET_BITS (24)

/* Multiples of 4 */
#define BS_REGISTRATION_CODE_BYTES (16)
#define BS_REGISTRATION_CODE_BITS (8*BS_REGISTRATION_CODE_BYTES) /* 128 */
#define BS_REGISTRATION_CODE_CHARS (2*BS_REGISTRATION_CODE_BYTES)
#define BS_REGISTRATION_KEY_BYTES (128)
#define BS_REGISTRATION_KEY_BITS (8*BS_REGISTRATION_KEY_BYTES) /* 1024 */
#define BS_REGISTRATION_KEY_CHARS (2*BS_REGISTRATION_KEY_BYTES)


/*
Client:
The public encryption key is available right in the source code
User sends his registration code

Server:
Registration key is computed from the private encryption key
The first 128 bits of decrypted registration key : equal to registration code
Next 32 bits : XORed with checksum of 128 bits of registration key -> function pointer offset
*/


////


#if BS_ENABLE_REGISTRATION


////


#if CPUCONF_WORD_SIZE >= 64

static const bn1024 bsEncryptionPublicKey =
{
  .unit =
  {
0x06682574cb8f45b5, 0x3ea0fefd00cea2bf, 0xc9abf335a3e140e0, 0xe41b7ea20b2f610a, 0xe900c2b0793f36a5, 0x5ec4e33430d112be, 0x06a392d6b7a69937, 0x7003e7b696fb6d84, 0x5c18f9a63a54d453, 0xbcabcc49e9fad6a1, 0x2c334308f94fd129, 0x41200681184f76c4, 0x3d17e38c223ec645, 0x05cf3b79918065f7, 0xbb0e2f197559bf53, 0x4037e120b15bba41
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

static const bn1024 bsEncryptionProduct =
{
  .unit =
  {
0x0d1ddeb3, 0x82697a2d, 0x5885ca4d, 0x8e836c79, 0xb0d5c95e, 0xbd949b3a, 0x28ee1062, 0x1fd5e1e5, 0x7b62d91e, 0x529d2f4f, 0x4307aa8c, 0x35414cd5, 0x86b1012b, 0x8a3970ad, 0x2d2fb469, 0x932700d9, 0x67029bef, 0x74f8b1fd, 0x053563aa, 0x69f07cdf, 0x730e14ad, 0x0af3e86c, 0xa40b2e29, 0xce080686, 0x9197f0d2, 0x9fb87925, 0x0564b315, 0x0b731999, 0xd4309cf5, 0x7b2d5572, 0x1702dcb5, 0x66f42a9e
  }
};

#endif


int bsRegistrationInit( bsContext *context, char *regkeystring )
{
  int i, keystringlen, hex, partindex, unitindex, unitshift;
  uint8_t crypthash[CRYPT_HASH_BYTES];
  uint8_t regbincode[BS_REGISTRATION_CODE_BYTES];
  uint32_t secretoffset;
  char *string;
  bn1024 regcode, regkey, regresult, bnmask;
  cryptHashState cryptstate;

  /* Clear stuff */
  context->registrationcode = 0;
  context->registrationsecretoffset = 0;

  /* Sanity check */
  if( !( context->bricklink.consumersecret ) || !( context->bricklink.consumersecret ) || !( context->bricklink.token ) || !( context->bricklink.tokensecret ) || !( context->brickowl.key ) )
    return 0;

  /* Compute registration code */
  cryptHashInit( &cryptstate );
  cryptHashData( &cryptstate, context->bricklink.consumerkey, strlen( context->bricklink.consumerkey ) );
  cryptHashData( &cryptstate, context->bricklink.consumersecret, strlen( context->bricklink.consumersecret ) );
  cryptHashData( &cryptstate, context->brickowl.key, strlen( context->brickowl.key ) );
  cryptHashData( &cryptstate, context->bricklink.token, strlen( context->bricklink.token ) );
  cryptHashData( &cryptstate, context->bricklink.tokensecret, strlen( context->bricklink.tokensecret ) );
  cryptHashResult( &cryptstate, crypthash );

  /* Build registration code */
  memset( regbincode, 0, BS_REGISTRATION_CODE_BYTES );
  for( i = 0 ; i < CRYPT_HASH_BYTES ; i++ )
    regbincode[i%BS_REGISTRATION_CODE_BYTES] ^= crypthash[i];

  /* Build registration code string */
  memset( &regcode, 0, sizeof(bn1024) );
  context->registrationcode = malloc( ( BS_REGISTRATION_CODE_BYTES << 1 ) + 1 );
  string = context->registrationcode;
  for( i = 0 ; i < BS_REGISTRATION_CODE_BYTES ; i++, string += 2 )
  {
    partindex = (BS_REGISTRATION_CODE_BYTES-1) - i;
    unitindex = partindex >> BN_UNIT_BYTE_SHIFT;
    unitshift = ( partindex << 3 ) & (BN_UNIT_BITS-1);
    regcode.unit[ unitindex ] |= ((bnUnit)regbincode[i]) << unitshift;
#if 0
printf( "%d : %d %d : 0x%x : 0x%llx\n", i, unitindex, unitshift, regbincode[i], (long long)regcode.unit[ unitindex ] );
#endif
    string[1] = regbincode[i] & 0xf;
    string[0] = regbincode[i] >> 4;
    string[1] += ( string[1] < 10 ? '0' : 'a'-10 );
    string[0] += ( string[0] < 10 ? '0' : 'a'-10 );
  }
  string[0] = 0;

  /* If no registration key, we are done here */
  keystringlen = 0;
  if( regkeystring )
    keystringlen = strlen( regkeystring );
  if( !( keystringlen ) )
    return 0;

  /* Quick check on key length */
  if( keystringlen != BS_REGISTRATION_KEY_CHARS )
  {
    ioPrintf( &context->output, IO_MODEBIT_NOLOG, "\n" );
    ioPrintf( &context->output, 0, BSMSG_WARNING "Invalid registration key! Your key counts %d characters but it should be %d characters long.\n", (int)keystringlen, (int)BS_REGISTRATION_KEY_CHARS );
    goto badkey;
  }

  /* Build registration key */
  memset( &regkey, 0, sizeof(bn1024) );
  for( i = 0 ; i < BS_REGISTRATION_KEY_CHARS ; i++ )
  {
    hex = ccCharHexBase( regkeystring[i] );
    if( hex < 0 )
    {
      ioPrintf( &context->output, IO_MODEBIT_NOLOG, "\n" );
      ioPrintf( &context->output, 0, BSMSG_WARNING "Invalid registration key! The key should only contain hexadecimal characters.\n" );
      goto badkey;
    }
    partindex = (BS_REGISTRATION_KEY_CHARS-1) - i;
    unitindex = partindex >> (BN_UNIT_BYTE_SHIFT+1);
    unitshift = ( partindex << 2 ) & (BN_UNIT_BITS-1);
    regkey.unit[ unitindex ] |= ((bnUnit)hex) << unitshift;
#if 0
printf( "%d : %d %d : 0x%x : 0x%llx\n", i, unitindex, unitshift, hex, (long long)regkey.unit[ unitindex ] );
#endif
  }

  /* Decrypt */
  rsaEncrypt( &regkey, &regresult, 1, bsEncryptionPublicKey, bsEncryptionProduct );

  /* Extract secret offset and clean up regresult */
  secretoffset = bn1024Extract32( &regresult, BS_REGISTRATION_CODE_BITS ) & ((1<<BS_ENCRYPTION_SECRET_OFFSET_BITS)-1);
  bn1024Set32Shl( &bnmask, (1<<BS_ENCRYPTION_SECRET_OFFSET_BITS)-1, BS_REGISTRATION_CODE_BITS );
  bn1024Not( &bnmask );
  bn1024And( &regresult, &bnmask );

  /* Verify correctness of registration key */
  if( bn1024CmpEq( &regresult, &regcode ) )
    ioPrintf( &context->output, 0, BSMSG_INIT "Your registration key was " IO_GREEN "accepted" IO_DEFAULT ". " IO_WHITE "Thank you for supporting BrickSync!" IO_DEFAULT "\n" );
  else
  {
    ioPrintf( &context->output, 0, BSMSG_INIT "Your registration key was " IO_RED "rejected" IO_DEFAULT ".\n" );
    goto badkey;
  }

  /* Keep secret offset */
  context->registrationsecretoffset = secretoffset;

  return 1;

////

  badkey:
  ioPrintf( &context->output, 0, BSMSG_WARNING "If your inventory holds more than " CC_STRINGIFY(BS_LIMITS_MAX_PARTCOUNT) " items, you will be asked a math question to check for new orders.\n\n" );
  ccSleep( 2000 );
  return 0;
}


////


#endif

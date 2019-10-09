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

#define SHA1_HASH_DIGEST_BITS (160)
#define SHA1_HASH_DIGEST_BYTES (20)

typedef struct
{
  uint32_t hashdigest[5];
  uint64_t length;
  unsigned char block[64];
  int blockindex;
} cryptSha1;


void cryptInitSha1( cryptSha1 *context );
void cryptDataSha1( cryptSha1 *context, const unsigned char *data, size_t length );
void cryptResultSha1( cryptSha1 *context, uint32_t *hashdigest );


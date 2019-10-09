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

typedef struct
{
  uint32_t state[8], length, curlen;
  uint8_t buf[64];
} cryptHashState;

#define CRYPT_HASH_BYTES (32)
#define CRYPT_HASH_BITS (256)

void cryptHashInit( cryptHashState *state );
void cryptHashData( cryptHashState *state, void *data, int datalength );
void cryptHashResult( cryptHashState *state, uint8_t *hash );


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

#define RSA_DATA_BITS (1024)

/* Unsigned data type */
typedef bn1024 rsaType;

/* Signed data type */
typedef bn1024 rsaTypeSigned;


void rsaGenKeys( rand32State *randstate, int inputbitcount, rsaType *retkey, rsaType *retinvkey, rsaType *retproduct );

void rsaEncrypt( rsaType *input, rsaType *output, int length, rsaType rsakey, rsaType rsaproduct );




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

/* Build string with escape chars as required, returned string must be free()'d */
char *xmlEncodeEscapeString( char *string, int length, int *retlength );
/* Build string with decoded escape chars, returned string must be free()'d */
char *xmlDecodeEscapeString( char *string, int length, int *retlength );

/* Parse an XML string for an integer value */
int xmlStrParseInt( char *str, int64_t *retint );
/* Parse an XML string for an float value */
int xmlStrParseFloat( char *str, float *retfloat );


////



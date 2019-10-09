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
  void *blidhashtable;
  void *boidhashtable;
  int storagecount;
  const char *path;
} translationTable;


int translationTableInit( translationTable *table, const char *path );

int translationTableRegisterEntry( translationTable *table, char bltypeid, const char *blid, int64_t boid );

int64_t translationBLIDtoBOID( translationTable *table, char bltypeid, const char *blid );
char translationBOIDtoBLID( translationTable *table, int64_t boid, char *retblid, int blidbuffersize );

int translationTableEnd( translationTable *table );


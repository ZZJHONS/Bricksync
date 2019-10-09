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
#include "mmhash.h"
#include "iolog.h"
#include "debugtrack.h"

#include "bstranslation.h"


////


#define TRANSLATION_DEFAULT_HASH_BITS (12)
#define TRANSLATION_DEFAULT_PAGE_BITS (4)


/* Must be a multiple of 4 */
#define TRANSLATION_BLPACK_LENGTH (16)
#define TRANSLATION_BLID_LENGTH (TRANSLATION_BLPACK_LENGTH-1)
#define TRANSLATION_BLTYPEID_OFFSET (TRANSLATION_BLPACK_LENGTH-1)


typedef struct
{
  char blpack[TRANSLATION_BLPACK_LENGTH];
  int64_t boid;
} translationElement __attribute__ ((aligned(8)));

typedef struct
{
  translationElement element;
  int32_t checksum;
} translationStorage __attribute__ ((aligned(8)));


////


/* Clear the entry so that entryvalid() returns zero */
static void vtHashClearEntryBlid( void *entry )
{
  translationElement *element;
  element = (translationElement *)entry;
  element->blpack[0] = 0;
  return;
}

/* Returns non-zero if the entry is valid and existing */
static int vtHashEntryValidBlid( void *entry )
{
  translationElement *element;
  element = (translationElement *)entry;
  return ( element->blpack[0] ? 1 : 0 );
}

/* Return key for an arbitrary set of user-defined data */
static uint32_t vtHashEntryKeyBlid( void *entry )
{
  translationElement *element;
  element = (translationElement *)entry;
  return ccHash32Data( (void *)element->blpack, TRANSLATION_BLPACK_LENGTH );
}

/* Return MM_HASH_ENTRYCMP* to stop or continue the search */
static int vtHashEntryCmpBlid( void *entry, void *entryref )
{
  translationElement *element, *elementref;
  element = (translationElement *)entry;
  if( !( element->blpack[0] ) )
    return MM_HASH_ENTRYCMP_INVALID;
  elementref = (translationElement *)entryref;
  if( ccMemCmpInline( element->blpack, elementref->blpack, TRANSLATION_BLPACK_LENGTH ) )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

mmHashAccess translationHashAccessBlid =
{
  .clearentry = vtHashClearEntryBlid,
  .entryvalid = vtHashEntryValidBlid,
  .entrykey = vtHashEntryKeyBlid,
  .entrycmp = vtHashEntryCmpBlid,
  .entrylist = 0
};


////


/* Clear the entry so that entryvalid() returns zero */
static void vtHashClearEntryBoid( void *entry )
{
  translationElement *element;
  element = (translationElement *)entry;
  element->boid = -1;
  return;
}

/* Returns non-zero if the entry is valid and existing */
static int vtHashEntryValidBoid( void *entry )
{
  translationElement *element;
  element = (translationElement *)entry;
  return ( element->boid != -1 ? 1 : 0 );
}

/* Return key for an arbitrary set of user-defined data */
static uint32_t vtHashEntryKeyBoid( void *entry )
{
  translationElement *element;
  element = (translationElement *)entry;
  return ccHash32Int32Inline( element->boid );
}

/* Return MM_HASH_ENTRYCMP* to stop or continue the search */
static int vtHashEntryCmpBoid( void *entry, void *entryref )
{
  translationElement *element, *elementref;
  element = (translationElement *)entry;
  if( element->boid == -1 )
    return MM_HASH_ENTRYCMP_INVALID;
  elementref = (translationElement *)entryref;
  if( element->boid == elementref->boid )
    return MM_HASH_ENTRYCMP_FOUND;
  return MM_HASH_ENTRYCMP_SKIP;
}

mmHashAccess translationHashAccessBoid =
{
  .clearentry = vtHashClearEntryBoid,
  .entryvalid = vtHashEntryValidBoid,
  .entrykey = vtHashEntryKeyBoid,
  .entrycmp = vtHashEntryCmpBoid,
  .entrylist = 0
};


////


static int32_t translationCheckSum( translationElement *element )
{
  int index;
  uint32_t checksum;
  uint32_t *data;
  data = (uint32_t *)element;
  checksum = 0;
  for( index = 0 ; index < (sizeof(translationElement)>>2) ; index++ )
  {
    checksum += data[index] & 0xFFFF;
    checksum = ( ( checksum << 16 ) ^ checksum ) ^ ( ( data[index] & 0xFFFF0000 ) >> 5 );
    checksum += checksum >> 11;
  }
  checksum ^= checksum << 3;
  checksum += checksum >> 5;
  checksum ^= checksum << 4;
  checksum += checksum >> 17;
  checksum ^= checksum << 25;
  checksum += checksum >> 6;
  return checksum;
}

static int translationCmpElement( translationElement *element, translationElement *refelement )
{
  int index;
  uint32_t *d0, *d1;
  d0 = (uint32_t *)element;
  d1 = (uint32_t *)refelement;
  for( index = 0 ; index < (sizeof(translationElement)>>2) ; index++ )
  {
    if( d0[index] != d1[index] )
      return 0;
  }
  return 1;
}


static void *translationGrowHashTable( void *hashtable, mmHashAccess *access )
{
  int hashbits;
  size_t hashsize;
  void *newtable;

  DEBUG_SET_TRACKER();

  if( mmHashGetStatus( hashtable, &hashbits ) == MM_HASH_STATUS_MUSTGROW )
  {
    hashbits++;
    hashsize = mmHashRequiredSize( sizeof(translationElement), hashbits, TRANSLATION_DEFAULT_PAGE_BITS );
    newtable = malloc( hashsize );
    mmHashResize( newtable, hashtable, access, hashbits, TRANSLATION_DEFAULT_PAGE_BITS );
    free( hashtable );
    hashtable = newtable;
  }

  return hashtable;
}


static int translationAddEntry( translationTable *table, translationElement *refelement, int *retstorageflag )
{
  int readflag, storageflag;
  translationElement addelement;

  DEBUG_SET_TRACKER();

  storageflag = 0;

  /* Add entry to BLID->BOID hash table */
  addelement = *refelement;
  if( mmHashDirectReadOrAddEntry( table->blidhashtable, &translationHashAccessBlid, &addelement, &readflag ) != MM_HASH_SUCCESS )
    return 0;
  if( readflag )
  {
    if( !( translationCmpElement( refelement, &addelement ) ) )
    {
      mmHashDirectReplaceEntry( table->blidhashtable, &translationHashAccessBlid, refelement, 0 );
      storageflag = 1;
    }
  }
  else
    storageflag = 1;
  table->blidhashtable = translationGrowHashTable( table->blidhashtable, &translationHashAccessBlid );

  /* Add entry to BOID->BLID hash table */
  addelement = *refelement;
  if( mmHashDirectReadOrAddEntry( table->boidhashtable, &translationHashAccessBoid, &addelement, &readflag ) != MM_HASH_SUCCESS )
    return 0;
  if( readflag )
  {
    if( !( translationCmpElement( refelement, &addelement ) ) )
    {
      mmHashDirectReplaceEntry( table->boidhashtable, &translationHashAccessBoid, refelement, 0 );
      storageflag = 1;
    }
  }
  else
    storageflag = 1;
  table->boidhashtable = translationGrowHashTable( table->boidhashtable, &translationHashAccessBoid );

  if( retstorageflag )
    *retstorageflag = storageflag;
  return 1;
}


////


int translationTableInit( translationTable *table, const char *path )
{
  int storageoffset;
  int hashbits;
  size_t hashsize;
  void *filedata;
  size_t filesize;
  translationStorage *storage;

  DEBUG_SET_TRACKER();

  hashbits = TRANSLATION_DEFAULT_HASH_BITS;
  hashsize = mmHashRequiredSize( sizeof(translationElement), hashbits, TRANSLATION_DEFAULT_PAGE_BITS );

  table->blidhashtable = malloc( hashsize );
  mmHashInit( table->blidhashtable, &translationHashAccessBlid, sizeof(translationElement), hashbits, TRANSLATION_DEFAULT_PAGE_BITS, 0x0 );
  table->boidhashtable = malloc( hashsize );
  mmHashInit( table->boidhashtable, &translationHashAccessBoid, sizeof(translationElement), hashbits, TRANSLATION_DEFAULT_PAGE_BITS, 0x0 );
  table->path = path;
  table->storagecount = 0;

  filedata = ccFileLoad( path, 512*1048576, &filesize );
  if( filedata )
  {
    for( storageoffset = 0 ; storageoffset < filesize ; storageoffset += sizeof(translationStorage) )
    {
      storage = ADDRESS( filedata, storageoffset );
      if( storage->element.boid <= 0 )
        break;
      if( !( storage->element.blpack[0] ) )
        break;
      if( storage->checksum != translationCheckSum( &storage->element ) )
        break;
      translationAddEntry( table, &storage->element, 0 );
      table->storagecount++;
    }
    free( filedata );
  }

  return 1;
}


int translationTableRegisterEntry( translationTable *table, char bltypeid, const char *blid, int64_t boid )
{
  int i, storageflag;
  FILE *file;
  translationStorage storageelement;

  DEBUG_SET_TRACKER();

  if( !( bltypeid ) || !( blid[0] ) || ( boid == -1 ) )
    return 0;
  for( i = 0 ; ; i++ )
  {
    if( i == TRANSLATION_BLID_LENGTH )
      return 0;
    if( !( blid[i] ) )
      break;
    storageelement.element.blpack[i] = blid[i];
  }
  for( ; i < TRANSLATION_BLID_LENGTH ; i++ )
    storageelement.element.blpack[i] = 0;
  storageelement.element.blpack[ TRANSLATION_BLTYPEID_OFFSET ] = bltypeid;
  storageelement.element.boid = boid;

  if( !( translationAddEntry( table, &storageelement.element, &storageflag ) ) )
    return 0;
  if( storageflag )
  {
    file = fopen( table->path, "r+b" );
    if( !( file ) )
      file = fopen( table->path, "wb" );
    if( file )
    {
      if( fseek( file, table->storagecount * sizeof(translationStorage), SEEK_SET ) != -1 )
      {
        storageelement.checksum = translationCheckSum( &storageelement.element );
        fwrite( &storageelement, sizeof(translationStorage), 1, file );
        table->storagecount++;
      }
      fclose( file );
    }
  }

  return 1;
}


int64_t translationBLIDtoBOID( translationTable *table, char bltypeid, const char *blid )
{
  int i;
  translationElement refelement;

  DEBUG_SET_TRACKER();

  for( i = 0 ; ; i++ )
  {
    if( !( blid[i] ) )
      break;
    if( i == TRANSLATION_BLID_LENGTH )
      return 0;
    refelement.blpack[i] = blid[i];
  }
  for( ; i < TRANSLATION_BLID_LENGTH ; i++ )
    refelement.blpack[i] = 0;
  refelement.blpack[ TRANSLATION_BLTYPEID_OFFSET ] = bltypeid;

  if( !( mmHashDirectReadEntry( table->blidhashtable, &translationHashAccessBlid, &refelement ) ) )
    return -1;
  return refelement.boid;
}


/* Return typeid, BLID stored in provided buffer */
char translationBOIDtoBLID( translationTable *table, int64_t boid, char *retblid, int blidbuffersize )
{
  int i;
  translationElement refelement;

  DEBUG_SET_TRACKER();

  refelement.boid = boid;

  if( !( mmHashDirectReadEntry( table->boidhashtable, &translationHashAccessBoid, &refelement ) ) )
    return 0;

  if( blidbuffersize <= 0 )
    return 0;
  if( blidbuffersize > TRANSLATION_BLID_LENGTH )
  {
    memcpy( retblid, refelement.blpack, TRANSLATION_BLID_LENGTH );
    retblid[ TRANSLATION_BLID_LENGTH ] = 0;
  }
  else
  {
    for( i = 0 ; i < blidbuffersize ; i++ )
      retblid[i] = refelement.blpack[i];
    retblid[ blidbuffersize - 1 ] = 0;
  }

  return refelement.blpack[ TRANSLATION_BLTYPEID_OFFSET ];
}


int translationTableEnd( translationTable *table )
{
  DEBUG_SET_TRACKER();

  free( table->blidhashtable );
  free( table->boidhashtable );
  memset( table, 0, sizeof(translationTable) );
  return 1;
}



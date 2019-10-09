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

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"


/* Friendlier to cache on SMP systems */
#define BP_BITMAP_PREWRITE_CHECK


/*
TODO
If we don't have atomic instruction support somehow, we need a much better mutex locking mechanism!!
*/


int mmBitMapInit( mmBitMap *bitmap, size_t entrycount, int initvalue )
{
  size_t mapsize, index;
  long value;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
#endif

  mapsize = ( entrycount + CPUCONF_LONG_BITS - 1 ) >> CPUCONF_LONG_BITSHIFT;
  bitmap->map = 0;
  if( mapsize )
  {
    if( !( bitmap->map = malloc( mapsize * sizeof(mmAtomicL) ) ) )
      return 0;
  }
  bitmap->mapsize = mapsize;
  bitmap->entrycount = entrycount;

  map = bitmap->map;
  value = ( initvalue & 0x1 ? ~0x0 : 0x0 );
#ifdef MM_ATOMIC_SUPPORT
  for( index = 0 ; index < mapsize ; index++ )
    mmAtomicWriteL( &map[index], value );
#else
  for( index = 0 ; index < mapsize ; index++ )
    map[index] = value;
  mtMutexInit( &bitmap->mutex );
#endif

  return 1;
}

void mmBitMapReset( mmBitMap *bitmap, int resetvalue )
{
  size_t index;
  long value;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
#endif

  map = bitmap->map;
  value = ( resetvalue & 0x1 ? ~(long)0x0 : (long)0x0 );
#ifdef MM_ATOMIC_SUPPORT
  for( index = 0 ; index < bitmap->mapsize ; index++ )
    mmAtomicWriteL( &map[index], value );
#else
  for( index = 0 ; index < bitmap->mapsize ; index++ )
    map[index] = value;
#endif

  return;
}

void mmBitMapResetRange( mmBitMap *bitmap, int minimumentrycount, int resetvalue )
{
  size_t index, entrycount;
  long value;
#ifdef MM_ATOMIC_SUPPORT
  mmAtomicL *map;
#else
  long *map;
#endif

  map = bitmap->map;
  value = ( resetvalue & 0x1 ? ~(long)0x0 : (long)0x0 );
  entrycount = ( minimumentrycount + CPUCONF_LONG_BITS - 1 ) >> CPUCONF_LONG_BITSHIFT;
#ifdef MM_ATOMIC_SUPPORT
  for( index = 0 ; index < entrycount ; index++ )
    mmAtomicWriteL( &map[index], value );
#else
  for( index = 0 ; index < entrycount ; index++ )
    map[index] = value;
#endif

  return;
}

void mmBitMapFree( mmBitMap *bitmap )
{
  free( bitmap->map );
  bitmap->map = 0;
  bitmap->mapsize = 0;
#ifndef MM_ATOMIC_SUPPORT
  mtMutexDestroy( &bitmap->mutex );
#endif
  return;
}

/* TODO: Yeah... That code was written in one go, maybe I should test if it's working fine, just in case? */
int mmBitMapFindSet( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex )
{
  unsigned long value;
  size_t index, shift, shiftbase, shiftmax, indexlast, shiftlast;

  if( !( bitmap->entrycount ) )
    return 0;

  indexlast = entryindexlast >> CPUCONF_LONG_BITSHIFT;
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );

  /* Leading bits search */
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shiftbase = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  mtMutexLock( &bitmap->mutex );
  value = bitmap->map[index];
#endif
  if( value != (unsigned long)0x0 )
  {
    shiftmax = CPUCONF_LONG_BITS-1;
    if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
      shiftmax = shiftlast;
    value >>= shiftbase;
    for( shift = shiftbase ; shift <= shiftmax ; shift++, value >>= 1 )
    {
      if( !( value & 0x1 ) )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      *retentryindex = entryindex;
      return 1;
    }
  }
  if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
  {
#ifndef MM_ATOMIC_SUPPORT
    mtMutexUnlock( &bitmap->mutex );
#endif
    return 0;
  }

  /* Main search */
  for( ; ; )
  {
    index = ( index + 1 ) % bitmap->mapsize;
    if( index == indexlast )
      break;
#ifdef MM_ATOMIC_SUPPORT
    value = mmAtomicReadL( &bitmap->map[index] );
#else
    value = bitmap->map[index];
#endif
    if( value != (unsigned long)0x0 )
    {
      for( shift = 0 ; ; shift++, value >>= 1 )
      {
        if( !( value & 0x1 ) )
          continue;
        entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
        if( entryindex >= bitmap->entrycount )
          break;
#ifndef MM_ATOMIC_SUPPORT
        mtMutexUnlock( &bitmap->mutex );
#endif
        *retentryindex = entryindex;
        return 1;
      }
    }
  }

  /* Trailing bits search */
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  value = bitmap->map[index];
#endif
  if( value != (unsigned long)0x0 )
  {
    for( shift = 0 ; shift < shiftlast ; shift++, value >>= 1 )
    {
      if( !( value & 0x1 ) )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      *retentryindex = entryindex;
      return 1;
    }
  }
#ifndef MM_ATOMIC_SUPPORT
  mtMutexUnlock( &bitmap->mutex );
#endif

  return 0;
}

int mmBitMapFindClear( mmBitMap *bitmap, size_t entryindex, size_t entryindexlast, size_t *retentryindex )
{
  unsigned long value;
  size_t index, shift, shiftbase, shiftmax, indexlast, shiftlast;

  if( !( bitmap->entrycount ) )
    return 0;

  indexlast = entryindexlast >> CPUCONF_LONG_BITSHIFT;
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );

  /* Leading bits search */
  index = entryindex >> CPUCONF_LONG_BITSHIFT;
  shiftbase = entryindex & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  mtMutexLock( &bitmap->mutex );
  value = bitmap->map[index];
#endif
  if( value != ~(unsigned long)0x0 )
  {
    shiftmax = CPUCONF_LONG_BITS-1;
    if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
      shiftmax = shiftlast;
    value >>= shiftbase;
    for( shift = shiftbase ; shift <= shiftmax ; shift++, value >>= 1 )
    {
      if( value & 0x1 )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      *retentryindex = entryindex;
      return 1;
    }
  }
  if( ( index == indexlast ) && ( shiftlast > shiftbase ) )
  {
#ifndef MM_ATOMIC_SUPPORT
    mtMutexUnlock( &bitmap->mutex );
#endif
    return 0;
  }

  /* Main search */
  for( ; ; )
  {
    index = ( index + 1 ) % bitmap->mapsize;
    if( index == indexlast )
      break;
#ifdef MM_ATOMIC_SUPPORT
    value = mmAtomicReadL( &bitmap->map[index] );
#else
    value = bitmap->map[index];
#endif
    if( value != ~(unsigned long)0x0 )
    {
      for( shift = 0 ; ; shift++, value >>= 1 )
      {
        if( value & 0x1 )
          continue;
        entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
        if( entryindex >= bitmap->entrycount )
          break;
#ifndef MM_ATOMIC_SUPPORT
        mtMutexUnlock( &bitmap->mutex );
#endif
        *retentryindex = entryindex;
        return 1;
      }
    }
  }

  /* Trailing bits search */
  shiftlast = entryindexlast & ( CPUCONF_LONG_BITS - 1 );
#ifdef MM_ATOMIC_SUPPORT
  value = mmAtomicReadL( &bitmap->map[index] );
#else
  value = bitmap->map[index];
#endif
  if( value != ~(unsigned long)0x0 )
  {
    for( shift = 0 ; shift <= shiftlast ; shift++, value >>= 1 )
    {
      if( value & 0x1 )
        continue;
      entryindex = ( index << CPUCONF_LONG_BITSHIFT ) | shift;
      if( entryindex >= bitmap->entrycount )
        break;
#ifndef MM_ATOMIC_SUPPORT
      mtMutexUnlock( &bitmap->mutex );
#endif
      *retentryindex = entryindex;
      return 1;
    }
  }
#ifndef MM_ATOMIC_SUPPORT
  mtMutexUnlock( &bitmap->mutex );
#endif

  return 0;
}



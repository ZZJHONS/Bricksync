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

/*

Templates C style!

#include this whole file with the following definitions set:

#define RSORT_MAIN MyInlinedRadixSort
#define RSORT_RADIX myradixfunction
#define RSORT_TYPE foobar
#define RSORT_RADIXBITS (12)
#define RSORT_BIGGEST_FIRST (1)

#define RSORT_MAXCOUNT (32767)
#define RSORT_TESTFULLBIN (1)

*/


#ifndef RSORT_COPY
 #define RSORT_COPY(d,s) (*(d)=*(s))
 #define CC_RSORT_COPY
#endif


#ifdef RSORT_CONTEXT
 #define RSORT_CONTEXT_PARAM , RSORT_CONTEXT context
 #define RSORT_CONTEXT_PASS context,
 #define RSORT_CONTEXT_PASSLAST , context
#else
 #define RSORT_CONTEXT_PARAM
 #define RSORT_CONTEXT_PASS
 #define RSORT_CONTEXT_PASSLAST
#endif

#if defined(RSORT_MAXCOUNT) && RSORT_MAXCOUNT < 32768
 #define CC_RSORT_COUNT_BITS 16
 #define CC_RSORT_COUNT_TYPE uint16_t
#else
 #define CC_RSORT_COUNT_BITS 32
 #define CC_RSORT_COUNT_TYPE uint32_t
#endif

#if CPU_SSE2_SUPPORT
 #if CC_RSORT_COUNT_BITS == 32
  #define CC_RSORT_RADIXBINCOUNT (((1<<RSORT_RADIXBITS)+15)&~15)
 #else
  #define CC_RSORT_RADIXBINCOUNT (((1<<RSORT_RADIXBITS)+31)&~31)
 #endif
#else
 #define CC_RSORT_RADIXBINCOUNT (1<<RSORT_RADIXBITS)
#endif

#if defined(__GNUC__)
 #define CC_RSORT_ALIGN __attribute__((aligned(16)))
#else
 #define CC_RSORT_ALIGN
#endif


#if CPU_SSE2_SUPPORT

 #define CC_RSORT_MERGE_FUNCX(n) n##PrefixSum
 #define CC_RSORT_MERGE_FUNC(n) HSORT_MERGE_FUNCX(n)

 #if CC_RSORT_COUNT_BITS == 32

  #if RSORT_BIGGEST_FIRST == 0

/* Prefix sum forward 32 bits ~ returns non-zero if all entries landed in the same bin */
static inline int CC_RSORT_MERGE_FUNC(RSORT_MAIN)( uint32_t *prefix, uint32_t *sumtable, const int bincount, int entrycount )
{
  int index;
  __m128i vsum0, vsum1, vprefix0, vprefix1, voffset, vzero, ventrycount;
   #if RSORT_TESTFULLBIN
  __m128i vmaxsum;
   #endif
  vzero = _mm_castps_si128( _mm_setzero_ps() );
  voffset = vzero;
   #if RSORT_TESTFULLBIN
  vmaxsum = vzero;
   #endif
  ventrycount = _mm_set1_epi32( entrycount );
  for( index = 0 ; index < bincount ; index += 8 )
  {
    vsum0 = _mm_load_si128( (void *)&sumtable[index+0] );
    vsum1 = _mm_load_si128( (void *)&sumtable[index+4] );
    vprefix0 = _mm_add_epi32( vsum0, _mm_slli_si128( vsum0, 4 ) ); 
    vprefix1 = _mm_add_epi32( vsum1, _mm_slli_si128( vsum1, 4 ) ); 
    vprefix0 = _mm_add_epi32( vprefix0, _mm_slli_si128( vprefix0, 8 ) ); 
    vprefix1 = _mm_add_epi32( vprefix1, _mm_slli_si128( vprefix1, 8 ) ); 
    vprefix0 = _mm_add_epi32( vprefix0, voffset );
    voffset = _mm_shuffle_epi32( vprefix0, 0xff );
    vprefix1 = _mm_add_epi32( vprefix1, voffset );
   #if RSORT_TESTFULLBIN
    #if CPU_SSE4_1_SUPPORT
    vmaxsum = _mm_max_epi32( vmaxsum, vsum0 );
    vmaxsum = _mm_max_epi32( vmaxsum, vsum1 );
    #else
    vmaxsum = _mm_or_si128( vmaxsum, _mm_cmpeq_epi32( vsum0, ventrycount ) );
    vmaxsum = _mm_or_si128( vmaxsum, _mm_cmpeq_epi32( vsum1, ventrycount ) );
    #endif
   #endif
    _mm_store_si128( (void *)&prefix[index+0], _mm_sub_epi32( vprefix0, vsum0 ) );
    _mm_store_si128( (void *)&prefix[index+4], _mm_sub_epi32( vprefix1, vsum1 ) );
    voffset = _mm_shuffle_epi32( vprefix1, 0xff );
  }
   #if RSORT_TESTFULLBIN
    #if CPU_SSE4_1_SUPPORT
  vmaxsum = _mm_cmpeq_epi16( vmaxsum, ventrycount );
    #endif
  return _mm_movemask_epi8( vmaxsum );
   #else
  return 0;
   #endif
}

  #else

/* Prefix sum backward 32 bits ~ returns non-zero if all entries landed in the same bin */
static inline int CC_RSORT_MERGE_FUNC(RSORT_MAIN)( uint32_t *prefix, uint32_t *sumtable, const int bincount, int entrycount )
{
  int index;
  __m128i vsum0, vsum1, vprefix0, vprefix1, voffset, vzero, ventrycount;
   #if RSORT_TESTFULLBIN
  __m128i vmaxsum;
   #endif
  vzero = _mm_castps_si128( _mm_setzero_ps() );
  voffset = vzero;
   #if RSORT_TESTFULLBIN
  vmaxsum = vzero;
   #endif
  vzero = _mm_castps_si128( _mm_setzero_ps() );
  voffset = vzero;
  ventrycount = _mm_set1_epi32( entrycount );
  for( index = bincount-8 ; index >= 0 ; index -= 8 )
  {
    vsum0 = _mm_load_si128( (void *)&sumtable[index+4] );
    vsum1 = _mm_load_si128( (void *)&sumtable[index+0] );
    vprefix0 = _mm_add_epi32( vsum0, _mm_srli_si128( vsum0, 4 ) ); 
    vprefix1 = _mm_add_epi32( vsum1, _mm_srli_si128( vsum1, 4 ) ); 
    vprefix0 = _mm_add_epi32( vprefix0, _mm_srli_si128( vprefix0, 8 ) ); 
    vprefix1 = _mm_add_epi32( vprefix1, _mm_srli_si128( vprefix1, 8 ) ); 
    vprefix0 = _mm_add_epi32( vprefix0, voffset );
    voffset = _mm_shuffle_epi32( vprefix0, 0x00 );
    vprefix1 = _mm_add_epi32( vprefix1, voffset );
   #if RSORT_TESTFULLBIN
    #if CPU_SSE4_1_SUPPORT
    vmaxsum = _mm_max_epi32( vmaxsum, vsum0 );
    vmaxsum = _mm_max_epi32( vmaxsum, vsum1 );
    #else
    vmaxsum = _mm_or_si128( vmaxsum, _mm_cmpeq_epi32( vsum0, ventrycount ) );
    vmaxsum = _mm_or_si128( vmaxsum, _mm_cmpeq_epi32( vsum1, ventrycount ) );
    #endif
   #endif
    _mm_store_si128( (void *)&prefix[index+4], _mm_sub_epi32( vprefix0, vsum0 ) );
    _mm_store_si128( (void *)&prefix[index+0], _mm_sub_epi32( vprefix1, vsum1 ) );
    voffset = _mm_shuffle_epi32( vprefix1, 0x00 );
  }
   #if RSORT_TESTFULLBIN
    #if CPU_SSE4_1_SUPPORT
  vmaxsum = _mm_cmpeq_epi16( vmaxsum, ventrycount );
    #endif
  return _mm_movemask_epi8( vmaxsum );
   #else
  return 0;
   #endif
}

  #endif

 #elif CC_RSORT_COUNT_BITS == 16

  #if RSORT_BIGGEST_FIRST == 0

/* Prefix sum forward 16 bits ~ returns non-zero if all entries landed in the same bin */
static inline int CC_RSORT_MERGE_FUNC(RSORT_MAIN)( uint16_t *prefix, uint16_t *sumtable, const int bincount, int entrycount )
{
  int index;
  __m128i vsum0, vsum1, vprefix0, vprefix1, voffset, vzero, ventrycount;
   #if RSORT_TESTFULLBIN
  __m128i vmaxsum;
   #endif
   #if CPU_SSSE3_SUPPORT
  __m128i vshufmask;
  vshufmask = _mm_set_epi8( 0x0f,0x0e, 0x0f,0x0e, 0x0f,0x0e, 0x0f,0x0e, 0x0f,0x0e, 0x0f,0x0e, 0x0f,0x0e, 0x0f,0x0e );
   #endif
  vzero = _mm_castps_si128( _mm_setzero_ps() );
  voffset = vzero;
   #if RSORT_TESTFULLBIN
  vmaxsum = vzero;
   #endif
  ventrycount = _mm_set1_epi16( entrycount );
  for( index = 0 ; index < bincount ; index += 16 )
  {
    vsum0 = _mm_load_si128( (void *)&sumtable[index+0] );
    vsum1 = _mm_load_si128( (void *)&sumtable[index+8] );
    vprefix0 = _mm_add_epi16( vsum0, _mm_slli_si128( vsum0, 2 ) ); 
    vprefix1 = _mm_add_epi16( vsum1, _mm_slli_si128( vsum1, 2 ) ); 
    vprefix0 = _mm_add_epi16( vprefix0, _mm_slli_si128( vprefix0, 4 ) ); 
    vprefix1 = _mm_add_epi16( vprefix1, _mm_slli_si128( vprefix1, 4 ) ); 
    vprefix0 = _mm_add_epi16( vprefix0, _mm_slli_si128( vprefix0, 8 ) ); 
    vprefix1 = _mm_add_epi16( vprefix1, _mm_slli_si128( vprefix1, 8 ) ); 
    vprefix0 = _mm_add_epi16( vprefix0, voffset );
   #if CPU_SSSE3_SUPPORT
    voffset = _mm_shuffle_epi8( vprefix0, vshufmask );
   #else
    voffset = _mm_shufflehi_epi16( vprefix0, 0xff );
    voffset = _mm_castps_si128( _mm_movehl_ps( _mm_castsi128_ps( voffset ), _mm_castsi128_ps( voffset ) ) );
   #endif
    vprefix1 = _mm_add_epi16( vprefix1, voffset );
   #if RSORT_TESTFULLBIN
    vmaxsum = _mm_max_epi16( vmaxsum, vsum0 );
    vmaxsum = _mm_max_epi16( vmaxsum, vsum1 );
   #endif
    _mm_store_si128( (void *)&prefix[index+0], _mm_sub_epi16( vprefix0, vsum0 ) );
    _mm_store_si128( (void *)&prefix[index+8], _mm_sub_epi16( vprefix1, vsum1 ) );
   #if CPU_SSSE3_SUPPORT
    voffset = _mm_shuffle_epi8( vprefix1, vshufmask );
   #else
    voffset = _mm_shufflehi_epi16( vprefix1, 0xff );
    voffset = _mm_castps_si128( _mm_movehl_ps( _mm_castsi128_ps( voffset ), _mm_castsi128_ps( voffset ) ) );
   #endif
  }
   #if RSORT_TESTFULLBIN
  vmaxsum = _mm_cmpeq_epi16( vmaxsum, ventrycount );
  return _mm_movemask_epi8( vmaxsum );
   #else
  return 0;
   #endif
}

  #else

/* Prefix sum backward 16 bits ~ returns non-zero if all entries landed in the same bin */
static inline int CC_RSORT_MERGE_FUNC(RSORT_MAIN)( uint16_t *prefix, uint16_t *sumtable, const int bincount, int entrycount )
{
  int index;
  __m128i vsum0, vsum1, vprefix0, vprefix1, voffset, vzero, ventrycount;
   #if RSORT_TESTFULLBIN
  __m128i vmaxsum;
   #endif
   #if CPU_SSSE3_SUPPORT
  __m128i vshufmask;
  vshufmask = _mm_set_epi8( 0x01,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x00 );
   #endif
  vzero = _mm_castps_si128( _mm_setzero_ps() );
  voffset = vzero;
   #if RSORT_TESTFULLBIN
  vmaxsum = vzero;
   #endif
  ventrycount = _mm_set1_epi16( entrycount );
  for( index = bincount-16 ; index >= 0 ; index -= 16 )
  {
    vsum0 = _mm_load_si128( (void *)&sumtable[index+8] );
    vsum1 = _mm_load_si128( (void *)&sumtable[index+0] );
    vprefix0 = _mm_add_epi16( vsum0, _mm_srli_si128( vsum0, 2 ) ); 
    vprefix1 = _mm_add_epi16( vsum1, _mm_srli_si128( vsum1, 2 ) ); 
    vprefix0 = _mm_add_epi16( vprefix0, _mm_srli_si128( vprefix0, 4 ) ); 
    vprefix1 = _mm_add_epi16( vprefix1, _mm_srli_si128( vprefix1, 4 ) ); 
    vprefix0 = _mm_add_epi16( vprefix0, _mm_srli_si128( vprefix0, 8 ) ); 
    vprefix1 = _mm_add_epi16( vprefix1, _mm_srli_si128( vprefix1, 8 ) ); 
    vprefix0 = _mm_add_epi16( vprefix0, voffset );
   #if CPU_SSSE3_SUPPORT
    voffset = _mm_shuffle_epi8( vprefix0, vshufmask );
   #else
    voffset = _mm_shufflelo_epi16( vprefix0, 0x00 );
    voffset = _mm_castps_si128( _mm_movelh_ps( _mm_castsi128_ps( voffset ), _mm_castsi128_ps( voffset ) ) );
   #endif
    vprefix1 = _mm_add_epi16( vprefix1, voffset );
   #if RSORT_TESTFULLBIN
    vmaxsum = _mm_max_epi16( vmaxsum, vsum0 );
    vmaxsum = _mm_max_epi16( vmaxsum, vsum1 );
   #endif
    _mm_store_si128( (void *)&prefix[index+8], _mm_sub_epi16( vprefix0, vsum0 ) );
    _mm_store_si128( (void *)&prefix[index+0], _mm_sub_epi16( vprefix1, vsum1 ) );
   #if CPU_SSSE3_SUPPORT
    voffset = _mm_shuffle_epi8( vprefix1, vshufmask );
   #else
    voffset = _mm_shufflelo_epi16( vprefix1, 0x00 );
    voffset = _mm_castps_si128( _mm_movelh_ps( _mm_castsi128_ps( voffset ), _mm_castsi128_ps( voffset ) ) );
   #endif
  }
   #if RSORT_TESTFULLBIN
  vmaxsum = _mm_cmpeq_epi16( vmaxsum, ventrycount );
  return _mm_movemask_epi8( vmaxsum );
   #else
  return 0;
   #endif
}

  #endif

 #endif


static RSORT_TYPE *RSORT_MAIN( RSORT_TYPE *src, RSORT_TYPE *tmp, int count, int sortbitcount RSORT_CONTEXT_PARAM )
{
  int bitindex, nextbitindex, radix, binflag;
  CC_RSORT_COUNT_TYPE CC_RSORT_ALIGN sumtable[CC_RSORT_RADIXBINCOUNT];
  CC_RSORT_COUNT_TYPE CC_RSORT_ALIGN prefix[CC_RSORT_RADIXBINCOUNT];
  void *clear, *clearend;
  RSORT_TYPE *dst;
  RSORT_TYPE *itemsrc;
  RSORT_TYPE *itemsrcend;
  RSORT_TYPE *itemdst;
  RSORT_TYPE *swap;
  __m128 vzero;

  vzero = _mm_setzero_ps();
  clear = (void *)sumtable;
  clearend = ADDRESS( clear, CC_RSORT_RADIXBINCOUNT * sizeof(CC_RSORT_COUNT_TYPE) );
  for( ; clear < clearend ; clear = ADDRESS( clear, 4*16 ) )
  {
    _mm_store_ps( ADDRESS( clear, 0 ), vzero );
    _mm_store_ps( ADDRESS( clear, 16 ), vzero );
    _mm_store_ps( ADDRESS( clear, 32 ), vzero );
    _mm_store_ps( ADDRESS( clear, 48 ), vzero );
  }
  itemsrc = src;
  itemsrcend = itemsrc + count;
  for( ; itemsrc < itemsrcend ; itemsrc++ )
  {
    radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, 0 );
    sumtable[radix]++;
  }

  dst = tmp;
  for( bitindex = 0 ; bitindex < sortbitcount ; bitindex += RSORT_RADIXBITS )
  {
    binflag = CC_RSORT_MERGE_FUNC(RSORT_MAIN)( prefix, sumtable, CC_RSORT_RADIXBINCOUNT, count );
    nextbitindex = bitindex + RSORT_RADIXBITS;
    itemsrc = src;
    itemsrcend = itemsrc + count;
 #if RSORT_TESTFULLBIN
    if( binflag )
    {
      if( nextbitindex < sortbitcount )
      {
        clear = (void *)sumtable;
        clearend = ADDRESS( clear, CC_RSORT_RADIXBINCOUNT * sizeof(CC_RSORT_COUNT_TYPE) );
        for( ; clear < clearend ; clear = ADDRESS( clear, 4*16 ) )
        {
          _mm_store_ps( ADDRESS( clear, 0 ), vzero );
          _mm_store_ps( ADDRESS( clear, 16 ), vzero );
          _mm_store_ps( ADDRESS( clear, 32 ), vzero );
          _mm_store_ps( ADDRESS( clear, 48 ), vzero );
        }
        for( ; itemsrc < itemsrcend ; itemsrc++ )
        {
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, nextbitindex );
          sumtable[radix]++;
        }
      }
    }
    else
 #endif
    {
      if( nextbitindex < sortbitcount )
      {
        clear = (void *)sumtable;
        clearend = ADDRESS( clear, CC_RSORT_RADIXBINCOUNT * sizeof(CC_RSORT_COUNT_TYPE) );
        for( ; clear < clearend ; clear = ADDRESS( clear, 4*16 ) )
        {
          _mm_store_ps( ADDRESS( clear, 0 ), vzero );
          _mm_store_ps( ADDRESS( clear, 16 ), vzero );
          _mm_store_ps( ADDRESS( clear, 32 ), vzero );
          _mm_store_ps( ADDRESS( clear, 48 ), vzero );
        }
        for( ; itemsrc < itemsrcend ; itemsrc++ )
        {
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, bitindex );
          itemdst = &dst[ prefix[radix]++ ];
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, nextbitindex );
          sumtable[radix]++;
          RSORT_COPY( itemdst, itemsrc );
        }
      }
      else
      {
        for( ; itemsrc < itemsrcend ; itemsrc++ )
        {
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, bitindex );
          itemdst = &dst[ prefix[radix]++ ];
          RSORT_COPY( itemdst, itemsrc );
        }
      }
      swap = src;
      src = dst;
      dst = swap;
    }
  }

  return src;
}


#else


static RSORT_TYPE *RSORT_MAIN( RSORT_TYPE *src, RSORT_TYPE *tmp, int count, int sortbitcount RSORT_CONTEXT_PARAM )
{
  int index, bitindex, nextbitindex, radix, sum;
 #if RSORT_TESTFULLBIN
  int maxsum;
 #endif
  CC_RSORT_COUNT_TYPE CC_RSORT_ALIGN sumtable[CC_RSORT_RADIXBINCOUNT];
  CC_RSORT_COUNT_TYPE CC_RSORT_ALIGN prefix[CC_RSORT_RADIXBINCOUNT];
  RSORT_TYPE *dst;
  RSORT_TYPE *itemsrc;
  RSORT_TYPE *itemsrcend;
  RSORT_TYPE *itemdst;
  RSORT_TYPE *swap;

  memset( sumtable, 0, CC_RSORT_RADIXBINCOUNT * sizeof(CC_RSORT_COUNT_TYPE) );
  itemsrc = src;
  itemsrcend = itemsrc + count;
  for( ; itemsrc < itemsrcend ; itemsrc++ )
  {
    radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, 0 );
    sumtable[radix]++;
  }

  dst = tmp;
  for( bitindex = 0 ; bitindex < sortbitcount ; bitindex += RSORT_RADIXBITS )
  {
 #if RSORT_BIGGEST_FIRST
    sum = 0;
  #if RSORT_TESTFULLBIN
    maxsum = 0;
  #endif
    for( index = CC_RSORT_RADIXBINCOUNT-1 ; index >= 0 ; index-- )
    {
      prefix[index] = sum;
      sum += sumtable[index];
  #if RSORT_TESTFULLBIN
      if( sumtable[index] > maxsum )
        maxsum = sumtable[index];
  #endif
    }
 #else
    sum = 0;
  #if RSORT_TESTFULLBIN
    maxsum = 0;
  #endif
    for( index = 0 ; index < CC_RSORT_RADIXBINCOUNT ; index++ )
    {
      prefix[index] = sum;
      sum += sumtable[index];
  #if RSORT_TESTFULLBIN
      if( sumtable[index] > maxsum )
        maxsum = sumtable[index];
  #endif
    }
 #endif
    nextbitindex = bitindex + RSORT_RADIXBITS;
    itemsrc = src;
    itemsrcend = itemsrc + count;
 #if RSORT_TESTFULLBIN
    if( maxsum == count )
    {
      if( nextbitindex < sortbitcount )
      {
        memset( sumtable, 0, CC_RSORT_RADIXBINCOUNT * sizeof(CC_RSORT_COUNT_TYPE) );
        for( ; itemsrc < itemsrcend ; itemsrc++ )
        {
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, nextbitindex );
          sumtable[radix]++;
        }
      }
    }
    else
 #endif
    {
      if( nextbitindex < sortbitcount )
      {
        memset( sumtable, 0, CC_RSORT_RADIXBINCOUNT * sizeof(CC_RSORT_COUNT_TYPE) );
        for( ; itemsrc < itemsrcend ; itemsrc++ )
        {
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, bitindex );
          itemdst = &dst[ prefix[radix]++ ];
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, nextbitindex );
          sumtable[radix]++;
          RSORT_COPY( itemdst, itemsrc );
        }
      }
      else
      {
        for( ; itemsrc < itemsrcend ; itemsrc++ )
        {
          radix = RSORT_RADIX( RSORT_CONTEXT_PASS itemsrc, bitindex );
          itemdst = &dst[ prefix[radix]++ ];
          RSORT_COPY( itemdst, itemsrc );
        }
      }
      swap = src;
      src = dst;
      dst = swap;
    }
  }

  return src;
}


#endif


#ifdef CC_RSORT_COPY
 #undef RSORT_COPY
 #undef CC_RSORT_COPY
#endif

#undef CC_RSORT_ALIGN
#undef CC_RSORT_COUNT_BITS
#undef CC_RSORT_COUNT_TYPE
#undef CC_RSORT_RADIXBINCOUNT

#undef RSORT_CONTEXT_PARAM
#undef RSORT_CONTEXT_PASS
#undef RSORT_CONTEXT_PASSLAST


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
#include <limits.h>
#include <float.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"

#define BN_XP_SUPPORT_128 1
#define BN_XP_SUPPORT_192 1
#define BN_XP_SUPPORT_256 1
#define BN_XP_SUPPORT_512 1
#define BN_XP_SUPPORT_1024 1
#include "bn.h"



////



int bn128Print( const bn128 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int unitindex;
  bn256 dst256;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT-BN_INT128_UNIT_COUNT ; unitindex++ )
    dst256.unit[unitindex] = 0;
  for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst256.unit[unitindex] = src->unit[unitindex-(BN_INT256_UNIT_COUNT-BN_INT128_UNIT_COUNT)];
  return bn256Print( &dst256, buffer, buffersize, signedflag, rightshiftbits+128, fractiondigits );
}


int bn128PrintHex( const bn128 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[128/4+3];
  bn128 value;
  const static char hexdigit[16] = "0123456789abcdef";

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn128SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn128Set( &value, src );

  /* Write integer part */
  bitcount = bn128GetIndexMSB( &value ) + 1;
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset += 4 )
  {
    digit = bn128Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset -= 4;
    digit = bn128Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


int bn128PrintBin( const bn128 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[128+3];
  bn128 value;

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn128SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn128Set( &value, src );

  /* Write integer part */
  bitcount = bn128GetIndexMSB( &value );
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset++ )
  {
    digit = bn128ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset--;
    digit = bn128ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


void bn128PrintOut( char *prefix, const bn128 * const src, char *suffix )
{
  char buffer[(int)(128.0*0.30102996+3.0)];
  if( prefix )
    printf( "%s", prefix );
  bn128Print( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn128PrintHexOut( char *prefix, const bn128 * const src, char *suffix )
{
  char buffer[128/4+3];
  if( prefix )
    printf( "%s", prefix );
  bn128PrintHex( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn128PrintBinOut( char *prefix, const bn128 * const src, char *suffix )
{
  char buffer[128+3];
  if( prefix )
    printf( "%s", prefix );
  bn128PrintBin( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}



////



int bn128Scan( bn128 *dst, char *str, bnShift leftshiftbits )
{
  int unitindex, negflag, digit, postshift;
  char c;
  bn128 tmp, dec, rem;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;

  /* Integer */
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    bn128Set( &tmp, dst );
    bn128Mul32( dst, &tmp, 10 );
    digit = c - '0';
    if( digit )
      bn128Add32Shl( dst, digit, leftshiftbits );
  }

  /* Fraction */
  postshift = 0;
  if( leftshiftbits > 128-4 )
  {
    postshift = leftshiftbits - (128-4);
    leftshiftbits -= postshift;
  }
  str++;
  bn128Set32( &dec, 10 );
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    digit = c - '0';
    if( digit )
    {
      bn128Set32Shl( &tmp, digit, leftshiftbits );
      /* Todo, accumulate remaining from multiple steps to correct result */
      bn128Div( &tmp, &dec, &rem );
      if( postshift )
        bn128Shl( &tmp, &tmp, postshift );
      bn128Add( dst, &tmp );
      bn128Shl1( &rem );
      if( bn128CmpGt( &rem, &dec ) )
        bn128Add32( dst, 1 );
    }
    while( bn128Mul32Check( &tmp, &dec, 10 ) )
    {
      if( --postshift < 0 )
        goto done;
      bn128Shl1( &dec );
    }
    bn128Set( &dec, &tmp );
  }

  done:
  if( negflag )
    bn128Neg( dst );

  return 1;
}


////


int bn192Print( const bn192 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int unitindex;
  bn256 dst256;
  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT-BN_INT192_UNIT_COUNT ; unitindex++ )
    dst256.unit[unitindex] = 0;
  for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst256.unit[unitindex] = src->unit[unitindex-(BN_INT256_UNIT_COUNT-BN_INT192_UNIT_COUNT)];
  return bn256Print( &dst256, buffer, buffersize, signedflag, rightshiftbits+64, fractiondigits );
}


int bn192PrintHex( const bn192 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[192/4+3];
  bn192 value;
  const static char hexdigit[16] = "0123456789abcdef";

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn192SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn192Set( &value, src );

  /* Write integer part */
  bitcount = bn192GetIndexMSB( &value ) + 1;
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset += 4 )
  {
    digit = bn192Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset -= 4;
    digit = bn192Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


int bn192PrintBin( const bn192 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[192+3];
  bn192 value;

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT192_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn192SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn192Set( &value, src );

  /* Write integer part */
  bitcount = bn192GetIndexMSB( &value );
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset++ )
  {
    digit = bn192ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset--;
    digit = bn192ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


void bn192PrintOut( char *prefix, const bn192 * const src, char *suffix )
{
  char buffer[(int)(192.0*0.30102996+3.0)];
  if( prefix )
    printf( "%s", prefix );
  bn192Print( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn192PrintHexOut( char *prefix, const bn192 * const src, char *suffix )
{
  char buffer[192/4+3];
  if( prefix )
    printf( "%s", prefix );
  bn192PrintHex( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn192PrintBinOut( char *prefix, const bn192 * const src, char *suffix )
{
  char buffer[192+3];
  if( prefix )
    printf( "%s", prefix );
  bn192PrintBin( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}



////



int bn192Scan( bn192 *dst, char *str, bnShift leftshiftbits )
{
  int unitindex, negflag, digit, postshift;
  char c;
  bn192 tmp, dec, rem;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  for( unitindex = 0 ; unitindex < BN_INT192_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;

  /* Integer */
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    bn192Set( &tmp, dst );
    bn192Mul32( dst, &tmp, 10 );
    digit = c - '0';
    if( digit )
      bn192Add32Shl( dst, digit, leftshiftbits );
  }

  /* Fraction */
  postshift = 0;
  if( leftshiftbits > 192-4 )
  {
    postshift = leftshiftbits - (192-4);
    leftshiftbits -= postshift;
  }
  str++;
  bn192Set32( &dec, 10 );
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    digit = c - '0';
    if( digit )
    {
      bn192Set32Shl( &tmp, digit, leftshiftbits );
      /* Todo, accumulate remaining from multiple steps to correct result */
      bn192Div( &tmp, &dec, &rem );
      if( postshift )
        bn192Shl( &tmp, &tmp, postshift );
      bn192Add( dst, &tmp );
      bn192Shl1( &rem );
      if( bn192CmpGt( &rem, &dec ) )
        bn192Add32( dst, 1 );
    }
    while( bn192Mul32Check( &tmp, &dec, 10 ) )
    {
      if( --postshift < 0 )
        goto done;
      bn192Shl1( &dec );
    }
    bn192Set( &dec, &tmp );
  }

  done:
  if( negflag )
    bn192Neg( dst );

  return 1;
}



////



int bn256Print( const bn256 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, j, digitcount, dotdigitcount, totalcount, roundflag, dotshift;
  unsigned int digit;
  uint32_t divrem;
  char digits[256];
  char dotdigits[256];
  bn256 value, part, tmp, ref, dec, rem, dgt, dotlimit;

  totalcount = 0;
  buffersize--;
  if( ( signedflag ) && ( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn256SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn256Set( &value, src );

  /* Write integer part */
  bn256Shr( &part, &value, rightshiftbits );
  digitcount = 0;
  for( i = 0 ; i < 256-1 ; i++ )
  {
    if( bn256CmpZero( &part ) )
      break;
    bn256Div32( &part, 10, &divrem );
    digits[digitcount++] = divrem;
  }

  /* Write fraction part */
  dotshift = 256-1;
  bn256Shl( &part, &value, dotshift - rightshiftbits );
  bn256Set32Shl( &tmp, 1, dotshift );
  bn256Sub32( &tmp, 1 );
  bn256And( &part, &tmp );
  bn256Set32( &dec, 10 );
  roundflag = 0;
  dotdigitcount = 0;
  if( fractiondigits > 255-1 )
    fractiondigits = 255-1;
  bn256Set32( &dotlimit, 10 );
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bn256Set32Shl( &ref, 1, dotshift );
    bn256Div( &ref, &dec, 0 );
    if( bn256CmpLt( &ref, &dotlimit ) )
      break;
    bn256Set( &dgt, &part );
    bn256Div( &dgt, &ref, &rem );
    digit = bn256Extract32( &dgt, 0 );
    if( digit > 9 )
    {
      digit = 9;
      bn256Set32( &dgt, digit );
      roundflag = 1;
    }
    else
    {
      bn256Shl1( &rem );
      roundflag = bn256CmpGt( &rem, &ref );
    }
    dotdigits[dotdigitcount++] = digit;
    if( digit != 0 )
    {
      bn256Mul( &tmp, &dgt, &ref );
      bn256Sub( &part, &tmp );
    }
    bn256Set( &tmp, &dec );
    bn256Mul32( &dec, &tmp, 10 );
  }

  if( roundflag )
  {
    /* Round up, increment all the way up */
    for( i = dotdigitcount-1 ; ; i-- )
    {
      if( i < 0 )
      {
        for( i = 0 ; ; i++ )
        {
          if( i == digitcount )
          {
            digits[digitcount++] = 1;
            break;
          }
          digits[i]++;
          if( digits[i] <= 9 )
            break;
          digits[i] = 0;
        }
        break;
      }
      dotdigits[i]++;
      if( dotdigits[i] <= 9 )
        break;
      dotdigits[i] = 0;
    }
  }

  if( !( digitcount ) )
    digits[digitcount++] = 0;
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  j = digitcount-1;
  for( i = 0 ; i < digitcount ; i++, j-- )
    buffer[j] = digits[i] + '0';
  buffer += digitcount;
  buffersize -= digitcount;
  if( dotdigitcount )
  {
    totalcount += dotdigitcount + 1;
    if( buffersize >= 1 )
    {
      *buffer = '.';
      buffer++;
      buffersize--;
    }
    if( dotdigitcount > buffersize )
      dotdigitcount = buffersize;
    for( i = 0 ; i < dotdigitcount ; i++ )
      buffer[i] = dotdigits[i] + '0';
    buffer += dotdigitcount;
  }
  *buffer = 0;

  return totalcount;
}


int bn256PrintHex( const bn256 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[256/4+3];
  bn256 value;
  const static char hexdigit[16] = "0123456789abcdef";

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn256SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn256Set( &value, src );

  /* Write integer part */
  bitcount = bn256GetIndexMSB( &value ) + 1;
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset += 4 )
  {
    digit = bn256Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset -= 4;
    digit = bn256Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


int bn256PrintBin( const bn256 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[256+3];
  bn256 value;

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT256_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn256SetNot( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn256Set( &value, src );

  /* Write integer part */
  bitcount = bn256GetIndexMSB( &value );
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset++ )
  {
    digit = bn256ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset--;
    digit = bn256ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


void bn256PrintOut( char *prefix, const bn256 * const src, char *suffix )
{
  char buffer[(int)(256.0*0.30102996+3.0)];
  if( prefix )
    printf( "%s", prefix );
  bn256Print( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn256PrintHexOut( char *prefix, const bn256 * const src, char *suffix )
{
  char buffer[256/4+3];
  if( prefix )
    printf( "%s", prefix );
  bn256PrintHex( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn256PrintBinOut( char *prefix, const bn256 * const src, char *suffix )
{
  char buffer[256+3];
  if( prefix )
    printf( "%s", prefix );
  bn256PrintBin( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}



////



int bn256Scan( bn256 *dst, char *str, bnShift leftshiftbits )
{
  int unitindex, negflag, digit, postshift;
  char c;
  bn256 tmp, dec, rem;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  for( unitindex = 0 ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;

  /* Integer */
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    bn256Set( &tmp, dst );
    bn256Mul32( dst, &tmp, 10 );
    digit = c - '0';
    if( digit )
      bn256Add32Shl( dst, digit, leftshiftbits );
  }

  /* Fraction */
  postshift = 0;
  if( leftshiftbits > 256-4 )
  {
    postshift = leftshiftbits - (256-4);
    leftshiftbits -= postshift;
  }
  str++;
  bn256Set32( &dec, 10 );
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    digit = c - '0';
    if( digit )
    {
      bn256Set32Shl( &tmp, digit, leftshiftbits );
      /* Todo, accumulate remaining from multiple steps to correct result */
      bn256Div( &tmp, &dec, &rem );
      if( postshift )
        bn256Shl( &tmp, &tmp, postshift );
      bn256Add( dst, &tmp );
      bn256Shl1( &rem );
      if( bn256CmpGt( &rem, &dec ) )
        bn256Add32( dst, 1 );
    }
    while( bn256Mul32Check( &tmp, &dec, 10 ) )
    {
      if( --postshift < 0 )
        goto done;
      bn256Shl1( &dec );
    }
    bn256Set( &dec, &tmp );
  }

  done:
  if( negflag )
    bn256Neg( dst );

  return 1;
}


////



int bn512Print( const bn512 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, j, digitcount, dotdigitcount, totalcount, roundflag, dotshift;
  unsigned int digit;
  uint32_t divrem;
  char digits[512];
  char dotdigits[512];
  bn512 value, part, tmp, ref, dec, rem, dgt, dotlimit;

  totalcount = 0;
  buffersize--;
  if( ( signedflag ) && ( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn512SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn512Set( &value, src );

  /* Write integer part */
  bn512Shr( &part, &value, rightshiftbits );
  digitcount = 0;
  for( i = 0 ; i < 512-1 ; i++ )
  {
    if( bn512CmpZero( &part ) )
      break;
    bn512Div32( &part, 10, &divrem );
    digits[digitcount++] = divrem;
  }

  /* Write fraction part */
  dotshift = 512-1;
  bn512Shl( &part, &value, dotshift - rightshiftbits );
  bn512Set32Shl( &tmp, 1, dotshift );
  bn512Sub32( &tmp, 1 );
  bn512And( &part, &tmp );
  bn512Set32( &dec, 10 );
  roundflag = 0;
  dotdigitcount = 0;
  if( fractiondigits > 512-1 )
    fractiondigits = 512-1;
  bn512Set32( &dotlimit, 10 );
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bn512Set32Shl( &ref, 1, dotshift );
    bn512Div( &ref, &dec, 0 );
    if( bn512CmpLt( &ref, &dotlimit ) )
      break;
    bn512Set( &dgt, &part );
    bn512Div( &dgt, &ref, &rem );
    digit = bn512Extract32( &dgt, 0 );
    if( digit > 9 )
    {
      digit = 9;
      bn512Set32( &dgt, digit );
      roundflag = 1;
    }
    else
    {
      bn512Shl1( &rem );
      roundflag = bn512CmpGt( &rem, &ref );
    }
    dotdigits[dotdigitcount++] = digit;
    if( digit != 0 )
    {
      bn512Mul( &tmp, &dgt, &ref );
      bn512Sub( &part, &tmp );
    }
    bn512Set( &tmp, &dec );
    bn512Mul32( &dec, &tmp, 10 );
  }

  if( roundflag )
  {
    /* Round up, increment all the way up */
    for( i = dotdigitcount-1 ; ; i-- )
    {
      if( i < 0 )
      {
        for( i = 0 ; ; i++ )
        {
          if( i == digitcount )
          {
            digits[digitcount++] = 1;
            break;
          }
          digits[i]++;
          if( digits[i] <= 9 )
            break;
          digits[i] = 0;
        }
        break;
      }
      dotdigits[i]++;
      if( dotdigits[i] <= 9 )
        break;
      dotdigits[i] = 0;
    }
  }

  if( !( digitcount ) )
    digits[digitcount++] = 0;
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  j = digitcount-1;
  for( i = 0 ; i < digitcount ; i++, j-- )
    buffer[j] = digits[i] + '0';
  buffer += digitcount;
  buffersize -= digitcount;
  if( dotdigitcount )
  {
    totalcount += dotdigitcount + 1;
    if( buffersize >= 1 )
    {
      *buffer = '.';
      buffer++;
      buffersize--;
    }
    if( dotdigitcount > buffersize )
      dotdigitcount = buffersize;
    for( i = 0 ; i < dotdigitcount ; i++ )
      buffer[i] = dotdigits[i] + '0';
    buffer += dotdigitcount;
  }
  *buffer = 0;

  return totalcount;
}


int bn512PrintHex( const bn512 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[512/4+3];
  bn512 value;
  const static char hexdigit[16] = "0123456789abcdef";

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn512SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn512Set( &value, src );

  /* Write integer part */
  bitcount = bn512GetIndexMSB( &value ) + 1;
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset += 4 )
  {
    digit = bn512Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset -= 4;
    digit = bn512Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


int bn512PrintBin( const bn512 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[512+3];
  bn512 value;

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT512_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn512SetNot( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn512Set( &value, src );

  /* Write integer part */
  bitcount = bn512GetIndexMSB( &value );
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset++ )
  {
    digit = bn512ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset--;
    digit = bn512ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


void bn512PrintOut( char *prefix, const bn512 * const src, char *suffix )
{
  char buffer[(int)(512.0*0.30102996+3.0)];
  if( prefix )
    printf( "%s", prefix );
  bn512Print( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn512PrintHexOut( char *prefix, const bn512 * const src, char *suffix )
{
  char buffer[512/4+3];
  if( prefix )
    printf( "%s", prefix );
  bn512PrintHex( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn512PrintBinOut( char *prefix, const bn512 * const src, char *suffix )
{
  char buffer[512+3];
  if( prefix )
    printf( "%s", prefix );
  bn512PrintBin( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}



////



int bn512Scan( bn512 *dst, char *str, bnShift leftshiftbits )
{
  int unitindex, negflag, digit, postshift;
  char c;
  bn512 tmp, dec, rem;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  for( unitindex = 0 ; unitindex < BN_INT512_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;

  /* Integer */
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    bn512Set( &tmp, dst );
    bn512Mul32( dst, &tmp, 10 );
    digit = c - '0';
    if( digit )
      bn512Add32Shl( dst, digit, leftshiftbits );
  }

  /* Fraction */
  postshift = 0;
  if( leftshiftbits > 512-4 )
  {
    postshift = leftshiftbits - (512-4);
    leftshiftbits -= postshift;
  }
  str++;
  bn512Set32( &dec, 10 );
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    digit = c - '0';
    if( digit )
    {
      bn512Set32Shl( &tmp, digit, leftshiftbits );
      /* Todo, accumulate remaining from multiple steps to correct result */
      bn512Div( &tmp, &dec, &rem );
      if( postshift )
        bn512Shl( &tmp, &tmp, postshift );
      bn512Add( dst, &tmp );
      bn512Shl1( &rem );
      if( bn512CmpGt( &rem, &dec ) )
        bn512Add32( dst, 1 );
    }
    while( bn512Mul32Check( &tmp, &dec, 10 ) )
    {
      if( --postshift < 0 )
        goto done;
      bn512Shl1( &dec );
    }
    bn512Set( &dec, &tmp );
  }

  done:
  if( negflag )
    bn512Neg( dst );
  return 1;
}


////


int bn1024Print( const bn1024 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, j, digitcount, dotdigitcount, totalcount, roundflag, dotshift;
  unsigned int digit;
  uint32_t divrem;
  char digits[1024];
  char dotdigits[1024];
  bn1024 value, part, tmp, ref, dec, rem, dgt, dotlimit;

  totalcount = 0;
  buffersize--;
  if( ( signedflag ) && ( src->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn1024SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn1024Set( &value, src );

  /* Write integer part */
  bn1024Shr( &part, &value, rightshiftbits );
  digitcount = 0;
  for( i = 0 ; i < 1024-1 ; i++ )
  {
    if( bn1024CmpZero( &part ) )
      break;
    bn1024Div32( &part, 10, &divrem );
    digits[digitcount++] = divrem;
  }

  /* Write fraction part */
  dotshift = 1024-1;
  bn1024Shl( &part, &value, dotshift - rightshiftbits );
  bn1024Set32Shl( &tmp, 1, dotshift );
  bn1024Sub32( &tmp, 1 );
  bn1024And( &part, &tmp );
  bn1024Set32( &dec, 10 );
  roundflag = 0;
  dotdigitcount = 0;
  if( fractiondigits > 1024-1 )
    fractiondigits = 1024-1;
  bn1024Set32( &dotlimit, 10 );
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bn1024Set32Shl( &ref, 1, dotshift );
    bn1024Div( &ref, &dec, 0 );
    if( bn1024CmpLt( &ref, &dotlimit ) )
      break;
    bn1024Set( &dgt, &part );
    bn1024Div( &dgt, &ref, &rem );
    digit = bn1024Extract32( &dgt, 0 );
    if( digit > 9 )
    {
      digit = 9;
      bn1024Set32( &dgt, digit );
      roundflag = 1;
    }
    else
    {
      bn1024Shl1( &rem );
      roundflag = bn1024CmpGt( &rem, &ref );
    }
    dotdigits[dotdigitcount++] = digit;
    if( digit != 0 )
    {
      bn1024Mul( &tmp, &dgt, &ref );
      bn1024Sub( &part, &tmp );
    }
    bn1024Set( &tmp, &dec );
    bn1024Mul32( &dec, &tmp, 10 );
  }

  if( roundflag )
  {
    /* Round up, increment all the way up */
    for( i = dotdigitcount-1 ; ; i-- )
    {
      if( i < 0 )
      {
        for( i = 0 ; ; i++ )
        {
          if( i == digitcount )
          {
            digits[digitcount++] = 1;
            break;
          }
          digits[i]++;
          if( digits[i] <= 9 )
            break;
          digits[i] = 0;
        }
        break;
      }
      dotdigits[i]++;
      if( dotdigits[i] <= 9 )
        break;
      dotdigits[i] = 0;
    }
  }

  if( !( digitcount ) )
    digits[digitcount++] = 0;
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  j = digitcount-1;
  for( i = 0 ; i < digitcount ; i++, j-- )
    buffer[j] = digits[i] + '0';
  buffer += digitcount;
  buffersize -= digitcount;
  if( dotdigitcount )
  {
    totalcount += dotdigitcount + 1;
    if( buffersize >= 1 )
    {
      *buffer = '.';
      buffer++;
      buffersize--;
    }
    if( dotdigitcount > buffersize )
      dotdigitcount = buffersize;
    for( i = 0 ; i < dotdigitcount ; i++ )
      buffer[i] = dotdigits[i] + '0';
    buffer += dotdigitcount;
  }
  *buffer = 0;

  return totalcount;
}


int bn1024PrintHex( const bn1024 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[1024/4+3];
  bn1024 value;
  const static char hexdigit[16] = "0123456789abcdef";

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn1024SetNeg( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn1024Set( &value, src );

  /* Write integer part */
  bitcount = bn1024GetIndexMSB( &value ) + 1;
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset += 4 )
  {
    digit = bn1024Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset -= 4;
    digit = bn1024Extract32( &value, bitoffset ) & 0xf;
    digits[digitcount++] = hexdigit[ digit ];
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


int bn1024PrintBin( const bn1024 * const src, char *buffer, int buffersize, int signedflag, int rightshiftbits, int fractiondigits )
{
  int i, bitcount, bitoffset, digit, digitcount, totalcount;
  char digits[1024+3];
  bn1024 value;

  totalcount = 0;
  if( ( signedflag ) && ( src->unit[BN_INT1024_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) ) )
  {
    bn1024SetNot( &value, src );
    totalcount++;
    if( buffersize > 0 )
    {
      *buffer++ = '-';
      buffersize--;
    }
  }
  else
    bn1024Set( &value, src );

  /* Write integer part */
  bitcount = bn1024GetIndexMSB( &value );
  digitcount = 0;
  for( bitoffset = rightshiftbits ; bitoffset < bitcount ; bitoffset++ )
  {
    digit = bn1024ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
  }
  if( !( digitcount ) )
    digits[digitcount++] = '0';
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  buffer += digitcount;
  *buffer = 0;
  for( i = 0 ; i < digitcount ; i++ )
  {
    buffer--;
    *buffer = digits[i];
  }
  buffer += digitcount;
  buffersize -= digitcount;

  if( fractiondigits <= 0 )
    return totalcount;

  /* Write fraction part */
  digitcount = 0;
  digits[digitcount++] = '.';
  bitoffset = rightshiftbits;
  for( i = 0 ; i < fractiondigits ; i++ )
  {
    bitoffset--;
    digit = bn1024ExtractBit( &value, bitoffset );
    digits[digitcount++] = '0' + digit;
    if( bitoffset < 0 )
      break;
  }
  totalcount += digitcount;
  if( digitcount > buffersize )
    digitcount = buffersize;
  for( i = 0 ; i < digitcount ; i++ )
    buffer[i] = digits[i];
  buffer[digitcount] = 0;

  return totalcount;
}


void bn1024PrintOut( char *prefix, const bn1024 * const src, char *suffix )
{
  char buffer[(int)(1024.0*0.30102996+3.0)];
  if( prefix )
    printf( "%s", prefix );
  bn1024Print( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn1024PrintHexOut( char *prefix, const bn1024 * const src, char *suffix )
{
  char buffer[1024/4+3];
  if( prefix )
    printf( "%s", prefix );
  bn1024PrintHex( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}


void bn1024PrintBinOut( char *prefix, const bn1024 * const src, char *suffix )
{
  char buffer[1024+3];
  if( prefix )
    printf( "%s", prefix );
  bn1024PrintBin( src, buffer, sizeof(buffer), 1, 0, 0 );
  printf( "%s", buffer );
  if( suffix )
    printf( "%s", suffix );
  return;
}



////



int bn1024Scan( bn1024 *dst, char *str, bnShift leftshiftbits )
{
  int unitindex, negflag, digit, postshift;
  char c;
  bn1024 tmp, dec, rem;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  for( unitindex = 0 ; unitindex < BN_INT1024_UNIT_COUNT ; unitindex++ )
    dst->unit[unitindex] = 0;

  /* Integer */
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    bn1024Set( &tmp, dst );
    bn1024Mul32( dst, &tmp, 10 );
    digit = c - '0';
    if( digit )
      bn1024Add32Shl( dst, digit, leftshiftbits );
  }

  /* Fraction */
  postshift = 0;
  if( leftshiftbits > 1024-4 )
  {
    postshift = leftshiftbits - (1024-4);
    leftshiftbits -= postshift;
  }
  str++;
  bn1024Set32( &dec, 10 );
  for( ; ; str++ )
  {
    c = *str;
    if( c <= ' ' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    digit = c - '0';
    if( digit )
    {
      bn1024Set32Shl( &tmp, digit, leftshiftbits );
      /* Todo, accumulate remaining from multiple steps to correct result */
      bn1024Div( &tmp, &dec, &rem );
      if( postshift )
        bn1024Shl( &tmp, &tmp, postshift );
      bn1024Add( dst, &tmp );
      bn1024Shl1( &rem );
      if( bn1024CmpGt( &rem, &dec ) )
        bn1024Add32( dst, 1 );
    }
    while( bn1024Mul32Check( &tmp, &dec, 10 ) )
    {
      if( --postshift < 0 )
        goto done;
      bn1024Shl1( &dec );
    }
    bn1024Set( &dec, &tmp );
  }

  done:
  if( negflag )
    bn1024Neg( dst );
  return 1;
}



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


#include "bsevalgrade.c"

/*
gcc eval.c cc.c debugtrack.c -s -o eval -lm -lpthread
*/


////


/* Build string with escape chars as required, returned string must be free()'d */
static char *xmlEncodeEscapeString( char *string, int length, int *retlength )
{
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( 5*length + 1 );
  for( dst = dstbase ; length ; length--, string++ )
  {
    c = *string;
    if( c == '&' )
    {
      dst[0] = '&';
      dst[1] = 'a';
      dst[2] = 'm';
      dst[3] = 'p';
      dst[4] = ';';
      dst += 5;
    }
    else if( c == '<' )
    {
      dst[0] = '&';
      dst[1] = 'l';
      dst[2] = 't';
      dst[3] = ';';
      dst += 4;
    }
    else
      *dst++ = c;
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );

  return dstbase;
}


static inline uint32_t jsonDecodeHexBase( char c )
{
  uint32_t hex;
  if( ( c >= '0' ) && ( c <= '9' ) )
    hex = c - '0';
  else if( ( c >= 'A' ) && ( c <= 'F' ) )
    hex = c - ('A'-10);
  else if( ( c >= 'a' ) && ( c <= 'f' ) )
    hex = c - ('a'-10);
  else
    hex = -1;
  return hex;
}


/* Build string with decoded escape chars, returned string must be free()'d */
static char *xmlDecodeEscapeString( char *string, int length, int *retlength )
{
  int utf8length, skip;
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( length + 1 );
  for( dst = dstbase ; length ; length -= skip, string += skip )
  {
    c = *string;
    skip = 1;
    if( c != '&' )
      *dst++ = c;
    else
    {
      if( !( --length ) )
        goto error;
      string++;
      if( ccStrCmpSeq( string, "lt;", 3 ) )
      {
        *dst++ = '<';
        skip = 3;
      }
      else if( ccStrCmpSeq( string, "gt;", 3 ) )
      {
        *dst++ = '>';
        skip = 3;
      }
      else if( ccStrCmpSeq( string, "quot;", 4 ) )
      {
        *dst++ = '"';
        skip = 4;
      }
      else if( ccStrCmpSeq( string, "amp;", 4 ) )
      {
        *dst++ = '&';
        skip = 4;
      }
      else if( ccStrCmpSeq( string, "apos;", 4 ) )
      {
        *dst++ = '\'';
        skip = 4;
      }
      else
      {
        *dst++ = '&';
        skip = 0;
      }
    }
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );
  return dstbase;

  error:
  free( dstbase );
  return 0;
}



////


int main()
{
  char *c, *d, *e;

  c = "New!!1";
  bsEvalulatePartCondition( c );
  c = "Good";
  bsEvalulatePartCondition( c );
  c = "Very good";
  bsEvalulatePartCondition( c );
  c = "Glass Cracked";
  bsEvalulatePartCondition( c );
  c = "Deep gash";
  bsEvalulatePartCondition( c );
  c = "Fading/Extra Used";
  bsEvalulatePartCondition( c );
  c = "Tested";
  bsEvalulatePartCondition( c );
  c = "Tiny scratches";
  bsEvalulatePartCondition( c );
  c = "Few tiny scratches";
  bsEvalulatePartCondition( c );
  c = "Many tiny scratches";
  bsEvalulatePartCondition( c );
  c = "No large scratches";
  bsEvalulatePartCondition( c );
  c = "Some scratches";
  bsEvalulatePartCondition( c );
  c = "Absolutely mint, very new, perfect";
  bsEvalulatePartCondition( c );
  c = "Almost new";
  bsEvalulatePartCondition( c );
  c = "Not quite new";
  bsEvalulatePartCondition( c );
  c = "works perfectly";
  bsEvalulatePartCondition( c );
  c = "Sealed";
  bsEvalulatePartCondition( c );
  c = "Slightly faded";
  bsEvalulatePartCondition( c );
  c = "Some scratches but otherwise new";
  bsEvalulatePartCondition( c );
  c = "The part is actually new, but the decoration is wearing off";
  bsEvalulatePartCondition( c );
  c = "starting to rub off";
  bsEvalulatePartCondition( c );
  c = "The decoration is wearing off";
  bsEvalulatePartCondition( c );
  c = "Heavy play wear";
  bsEvalulatePartCondition( c );
  c = "Stress Marks";
  bsEvalulatePartCondition( c );
  c = "Chewed on";
  bsEvalulatePartCondition( c );
  c = "Bite marks";
  bsEvalulatePartCondition( c );
  c = "Excellent condition, but the glass is missing";
  bsEvalulatePartCondition( c );
  c = "No bite marks";
  bsEvalulatePartCondition( c );
  c = "Deep gash";
  bsEvalulatePartCondition( c );
  c = "This part's condition would scare Cthulhu";
  bsEvalulatePartCondition( c );


/*
  printf( "\n" );

  c = "This is a XML string";
  d = xmlEncodeEscapeString( c, strlen( c ), 0 );
  e = xmlDecodeEscapeString( d, strlen( d ), 0 );
  printf( "Input   : \"%s\"\n", c );
  printf( "Encoded : \"%s\"\n", d );
  printf( "Decoded : \"%s\"\n", e );


  c = "This <p> is <i>Italic</i> stuff";
  d = xmlEncodeEscapeString( c, strlen( c ), 0 );
  e = xmlDecodeEscapeString( d, strlen( d ), 0 );
  printf( "Input   : \"%s\"\n", c );
  printf( "Encoded : \"%s\"\n", d );
  printf( "Decoded : \"%s\"\n", e );


  c = "if( &f < &g )";
  d = xmlEncodeEscapeString( c, strlen( c ), 0 );
  e = xmlDecodeEscapeString( d, strlen( d ), 0 );
  printf( "Input   : \"%s\"\n", c );
  printf( "Encoded : \"%s\"\n", d );
  printf( "Decoded : \"%s\"\n", e );


  c = "Aa &lt; Bb &gt; Cc &amp;";
  d = xmlEncodeEscapeString( c, strlen( c ), 0 );
  e = xmlDecodeEscapeString( d, strlen( d ), 0 );
  printf( "Input   : \"%s\"\n", c );
  printf( "Encoded : \"%s\"\n", d );
  printf( "Decoded : \"%s\"\n", e );
*/


  return;
}


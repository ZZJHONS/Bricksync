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

/* For mkdir() */
#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <sys/types.h>
 #include <sys/stat.h>
#else
 #error Unknown/Unsupported platform!
#endif


#include "xml.h"


////


#define XML_CHAR_IS_DELIMITER(c) (((c)<=' ')||((c)=='<'))

/* Parse an XML string for an integer value */
int xmlStrParseInt( char *str, int64_t *retint )
{
  int negflag;
  char c;
  int64_t workint;

  if( !( str ) )
    return 0;
  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  workint = 0;
  for( ; ; str++ )
  {
    c = *str;
    if( XML_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workint = ( workint * 10 ) + ( c - '0' );
  }

  if( negflag )
    workint = -workint;
  *retint = workint;
  return 1;
}


/* Parse an XML string for an float value */
int xmlStrParseFloat( char *str, float *retfloat )
{
  char c;
  int negflag;
  double workdouble;
  double decfactor;

  if( !( str ) )
    return 0;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  workdouble = 0.0;
  for( ; ; str++ )
  {
    c = *str;
    if( XML_CHAR_IS_DELIMITER( c ) )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workdouble = ( workdouble * 10.0 ) + (double)( c - '0' );
  }

  str++;
  decfactor = 0.1;
  for( ; ; str++ )
  {
    c = *str;
    if( XML_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workdouble += (double)( c - '0' ) * decfactor;
    decfactor *= 0.1;
  }

  done:

  if( negflag )
    workdouble = -workdouble;
  *retfloat = (float)workdouble;

  return 1;
}


////


/* Concatenate suffix to string, and move string pointer to after suffix */
void xmlAppendSuffix( char *suffix, char **string )
{
  for ( ; *suffix ; suffix++ )
  {
    *(*string)++ = *suffix;
  }
}

/* Build string with escape chars as required, returned string must be free()'d */
char *xmlEncodeEscapeString( char *string, int length, int *retlength )
{
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( 6*length + 1 );
  for( dst = dstbase ; length ; length--, string++ )
  {
    c = *string;
    /* Encode predefined entities */
    if( c == '&' )
    {
      xmlAppendSuffix("&amp;", &dst);
    }
    else if( c == '<' )
    {
      xmlAppendSuffix("&lt;", &dst);
    }
    else if( c == '>' )
    {
      xmlAppendSuffix("&gt;", &dst);
    }
    else if( c == '"' )
    {
      xmlAppendSuffix("&quot;", &dst);
    }
    else if( c == '\'' )
    {
      xmlAppendSuffix("&apos;", &dst);
    }
    else
      *dst++ = c;
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );

  return dstbase;
}


/* Check if the prefix matches the given string; otherwise, return 0.
 * If the prefix matches, save the replacement character to the output string before inrementing.
 * Finally, output the number of characters matched from the input string. */
int xmlReplacePrefix( char *string, char *prefix, char replacement, char **output, int *skip)
{
  if ( ccStrCmpSeq( string, prefix, strlen(prefix) ) )
  {
    *(*output)++ = replacement;
    *skip = strlen(prefix);
    return 1;
  }

  return 0;
}

/* Build string with decoded escape chars, returned string must be free()'d */
char *xmlDecodeEscapeString( char *string, int length, int *retlength )
{
  int skip;
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
      /* Decode predefined entities */
      if( xmlReplacePrefix( string, "lt;", '<', &dst, &skip ) ) { }
      else if( xmlReplacePrefix( string, "gt;", '>', &dst, &skip ) ) { }
      else if( xmlReplacePrefix( string, "quot;", '"', &dst, &skip ) ) { }
      else if( xmlReplacePrefix( string, "amp;", '&', &dst, &skip ) ) { }
      else if( xmlReplacePrefix( string, "apos;", '\'', &dst, &skip ) ) { }
      /* Also decode numeric entities */
      else if( xmlReplacePrefix( string, "#039;", '\'', &dst, &skip ) ) { }
      else if( xmlReplacePrefix( string, "#40;", '(', &dst, &skip ) ) { }
      else if( xmlReplacePrefix( string, "#41;", ')', &dst, &skip ) ) { }
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
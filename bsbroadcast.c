/* -----------------------------------------------------------------------------
 *
 * Version check and broadcast message, the parts that need no network (see bsbroadcast.h).
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
#include "iolog.h"
#include "json.h"

#include "bsbroadcast.h"


////


static int bsReadDigits( const char *s, int count, int *ret )
{
  int index, value;
  value = 0;
  for( index = 0 ; index < count ; index++ )
  {
    if( ( s[index] < '0' ) || ( s[index] > '9' ) )
      return 0;
    value = ( value * 10 ) + ( s[index] - '0' );
  }
  *ret = value;
  return 1;
}

static int bsReadNumber( const char **s, int *ret )
{
  int value, count;
  value = 0;
  for( count = 0 ; ( **s >= '0' ) && ( **s <= '9' ) ; count++, (*s)++ )
  {
    if( count >= 4 )
      return 0;
    value = ( value * 10 ) + ( **s - '0' );
  }
  *ret = value;
  return ( count > 0 );
}

int bsReleaseTagParse( const char *tag, bsReleaseTag *ret )
{
  int day, month, year, length;
  const char *s;

  memset( ret, 0, sizeof(bsReleaseTag) );
  if( !( tag ) )
    return 0;
  s = tag;
  if( ( *s == 'v' ) || ( *s == 'V' ) )
    s++;
  if( !( bsReadNumber( &s, &ret->major ) ) || ( *s++ != '.' ) )
    return 0;
  if( !( bsReadNumber( &s, &ret->minor ) ) || ( *s++ != '.' ) )
    return 0;
  if( !( bsReadNumber( &s, &ret->revision ) ) || ( *s++ != '-' ) )
    return 0;
  /* DDMMYY */
  if( !( bsReadDigits( s, 2, &day ) ) || !( bsReadDigits( s + 2, 2, &month ) ) || !( bsReadDigits( s + 4, 2, &year ) ) )
    return 0;
  if( ( day < 1 ) || ( day > 31 ) || ( month < 1 ) || ( month > 12 ) )
    return 0;
  ret->date = ( ( 2000 + year ) * 10000 ) + ( month * 100 ) + day;
  s += 6;
  for( length = 0 ; *s ; s++, length++ )
  {
    if( ( *s < 'a' ) || ( *s > 'z' ) || ( length >= (int)sizeof(ret->suffix) - 1 ) )
      return 0;
    ret->suffix[length] = *s;
  }
  ret->suffix[length] = 0;
  return 1;
}

int bsReleaseTagCompare( const bsReleaseTag *a, const bsReleaseTag *b )
{
  size_t lena, lenb;
  if( a->major != b->major )
    return ( a->major < b->major ? -1 : 1 );
  if( a->minor != b->minor )
    return ( a->minor < b->minor ? -1 : 1 );
  if( a->revision != b->revision )
    return ( a->revision < b->revision ? -1 : 1 );
  if( a->date != b->date )
    return ( a->date < b->date ? -1 : 1 );
  /* Same day: "" < "a" < "b" ... */
  lena = strlen( a->suffix );
  lenb = strlen( b->suffix );
  if( lena != lenb )
    return ( lena < lenb ? -1 : 1 );
  return strcmp( a->suffix, b->suffix );
}

int bsBuildStampDate( const char *stamp )
{
  int date;
  if( !( stamp ) || !( bsReadDigits( stamp, 8, &date ) ) )
    return 0;
  if( ( stamp[8] != 0 ) && ( stamp[8] != '.' ) )
    return 0;
  if( date < 20000101 )
    return 0;
  return date;
}

int bsReleaseState( const char *ourtag, const char *ourstamp, const char *latesttag )
{
  int cmp, ourdate;
  bsReleaseTag latest, ours;

  if( !( bsReleaseTagParse( latesttag, &latest ) ) )
    return BS_RELEASE_STATE_UNKNOWN;
  if( ( ourtag ) && ( ourtag[0] ) && ( bsReleaseTagParse( ourtag, &ours ) ) )
  {
    cmp = bsReleaseTagCompare( &ours, &latest );
    if( cmp < 0 )
      return BS_RELEASE_STATE_NEWER_AVAILABLE;
    return ( cmp == 0 ? BS_RELEASE_STATE_LATEST : BS_RELEASE_STATE_AHEAD );
  }
  /* Not a release build: only its date tells anything */
  ourdate = bsBuildStampDate( ourstamp );
  if( ( ourdate ) && ( ourdate < latest.date ) )
    return BS_RELEASE_STATE_NEWER_AVAILABLE;
  return BS_RELEASE_STATE_DEVELOPMENT;
}

int bsBuildOlderThan( const char *ourtag, const char *ourstamp, const char *tag )
{
  int ourdate;
  bsReleaseTag limit, ours;

  if( !( bsReleaseTagParse( tag, &limit ) ) )
    return 1;
  if( ( ourtag ) && ( ourtag[0] ) && ( bsReleaseTagParse( ourtag, &ours ) ) )
    return ( bsReleaseTagCompare( &ours, &limit ) < 0 );
  ourdate = bsBuildStampDate( ourstamp );
  if( !( ourdate ) )
    return 1;
  return ( ourdate < limit.date );
}


////


static int bsHexValue( char c )
{
  if( ( c >= '0' ) && ( c <= '9' ) )
    return c - '0';
  if( ( c >= 'a' ) && ( c <= 'f' ) )
    return c - 'a' + 10;
  if( ( c >= 'A' ) && ( c <= 'F' ) )
    return c - 'A' + 10;
  return -1;
}

static void bsTextPut( char *dst, int dstsize, int *dstlen, unsigned int c )
{
  /* Control characters (ESC included) never reach the terminal; newline and tab do */
  if( ( ( c < 0x20 ) && ( c != '\n' ) && ( c != '\t' ) ) || ( c == 0x7f ) )
    return;
  if( *dstlen >= dstsize - 1 )
    return;
  dst[(*dstlen)++] = (char)c;
  return;
}

void bsDecodeJsonText( char *dst, int dstsize, const char *src, int srclen )
{
  int index, dstlen, digit, k;
  unsigned int c, code;

  dstlen = 0;
  if( dstsize <= 0 )
    return;
  for( index = 0 ; index < srclen ; index++ )
  {
    c = (unsigned char)src[index];
    if( c != '\\' )
    {
      /* C1 controls written as UTF-8 (U+0080..U+009F, e.g. CSI) are dropped too */
      if( ( c == 0xc2 ) && ( index + 1 < srclen ) && ( (unsigned char)src[index+1] >= 0x80 ) && ( (unsigned char)src[index+1] <= 0x9f ) )
      {
        index++;
        continue;
      }
      bsTextPut( dst, dstsize, &dstlen, c );
      continue;
    }
    if( ++index >= srclen )
      break;
    c = (unsigned char)src[index];
    switch( c )
    {
      case 'n': bsTextPut( dst, dstsize, &dstlen, '\n' ); break;
      case 't': bsTextPut( dst, dstsize, &dstlen, '\t' ); break;
      case 'r': case 'b': case 'f': break;
      case 'u':
        code = 0;
        for( k = 1 ; k <= 4 ; k++ )
        {
          if( ( index + k >= srclen ) || ( ( digit = bsHexValue( src[index+k] ) ) < 0 ) )
            break;
          code = ( code << 4 ) | (unsigned int)digit;
        }
        if( k <= 4 )
          break;
        index += 4;
        /* UTF-8; surrogate halves (characters outside the BMP) become '?' */
        if( code < 0x80 )
          bsTextPut( dst, dstsize, &dstlen, code );
        else if( code <= 0x9f )
        {
          /* C1 control: dropped */
        }
        else if( ( code >= 0xd800 ) && ( code <= 0xdfff ) )
          bsTextPut( dst, dstsize, &dstlen, '?' );
        else if( dstlen + ( code < 0x800 ? 2 : 3 ) < dstsize )
        {
          if( code < 0x800 )
          {
            dst[dstlen++] = (char)( 0xc0 | ( code >> 6 ) );
            dst[dstlen++] = (char)( 0x80 | ( code & 0x3f ) );
          }
          else
          {
            dst[dstlen++] = (char)( 0xe0 | ( code >> 12 ) );
            dst[dstlen++] = (char)( 0x80 | ( ( code >> 6 ) & 0x3f ) );
            dst[dstlen++] = (char)( 0x80 | ( code & 0x3f ) );
          }
        }
        break;
      default:
        /* \" \\ \/ and anything unknown: the character itself */
        bsTextPut( dst, dstsize, &dstlen, c );
        break;
    }
  }
  dst[dstlen] = 0;
  return;
}


////


static int bsTokenIs( jsonParser *parser, jsonToken *token, const char *name )
{
  size_t len;
  len = strlen( name );
  return ( ( token->length == len ) && ( memcmp( &parser->codestring[ token->offset ], name, len ) == 0 ) );
}

/* Walk a flat top-level object; for each key, call handler on the value (a STRING or INTEGER token),
 * anything else is skipped. 1 if the whole object was read */
typedef void (*bsJsonKeyHandler)( jsonParser *parser, jsonToken *name, jsonToken *value, void *uservalue );

static int bsParseObject( char *json, ioLog *log, bsJsonKeyHandler handler, void *uservalue )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;
  jsonToken *name, *value;

  if( !( json ) )
    return 0;
  tokenbuf = jsonLexParse( json, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, json, tokenbuf, log );
  retval = 0;
  if( !( jsonTokenAccept( &parser, JSON_TOKEN_LBRACE ) ) )
    goto done;
  if( jsonTokenAccept( &parser, JSON_TOKEN_RBRACE ) )
  {
    retval = 1;
    goto done;
  }
  for( ; ; )
  {
    if( !( name = jsonTokenAccept( &parser, JSON_TOKEN_STRING ) ) )
      goto done;
    if( !( jsonTokenAccept( &parser, JSON_TOKEN_COLON ) ) )
      goto done;
    if( ( parser.tokentype == JSON_TOKEN_STRING ) || ( parser.tokentype == JSON_TOKEN_INTEGER ) )
    {
      value = parser.token;
      handler( &parser, name, value, uservalue );
      jsonTokenIncrement( &parser );
    }
    else if( !( jsonParserSkipValue( &parser ) ) )
      goto done;
    if( jsonTokenAccept( &parser, JSON_TOKEN_COMMA ) )
      continue;
    if( jsonTokenAccept( &parser, JSON_TOKEN_RBRACE ) )
      break;
    goto done;
  }
  retval = ( parser.errorcount == 0 );
  done:
  jsonLexFree( tokenbuf );
  return retval;
}


typedef struct
{
  char *tag;
  int tagsize;
  int found;
} bsReleaseReader;

static void bsReleaseKey( jsonParser *parser, jsonToken *name, jsonToken *value, void *uservalue )
{
  bsReleaseReader *reader = uservalue;
  if( ( value->type == JSON_TOKEN_STRING ) && bsTokenIs( parser, name, "tag_name" ) )
  {
    bsDecodeJsonText( reader->tag, reader->tagsize, &parser->codestring[ value->offset ], value->length );
    reader->found = 1;
  }
  return;
}

int bsParseLatestRelease( char *json, ioLog *log, char *rettag, int rettagsize )
{
  bsReleaseReader reader;
  bsReleaseTag check;

  rettag[0] = 0;
  reader.tag = rettag;
  reader.tagsize = rettagsize;
  reader.found = 0;
  if( !( bsParseObject( json, log, bsReleaseKey, &reader ) ) || !( reader.found ) )
  {
    rettag[0] = 0;
    return 0;
  }
  /* Only a tag in our format counts */
  if( !( bsReleaseTagParse( rettag, &check ) ) )
  {
    rettag[0] = 0;
    return 0;
  }
  return 1;
}


static const char *bsPriorityNames[BS_BROADCAST_PRIORITY_COUNT] = { "none", "low", "medium", "high", "critical" };

static void bsBroadcastKey( jsonParser *parser, jsonToken *name, jsonToken *value, void *uservalue )
{
  int index;
  int64_t number;
  char buffer[32];
  bsBroadcast *msg = uservalue;

  if( bsTokenIs( parser, name, "timestamp" ) && ( value->type == JSON_TOKEN_INTEGER ) )
  {
    number = 0;
    for( index = 0 ; ( index < value->length ) && ( index < 18 ) ; index++ )
    {
      /* Digits only: a negative or odd number leaves no timestamp */
      if( ( parser->codestring[ value->offset + index ] < '0' ) || ( parser->codestring[ value->offset + index ] > '9' ) )
        return;
      number = ( number * 10 ) + ( parser->codestring[ value->offset + index ] - '0' );
    }
    msg->timestamp = number;
  }
  else if( value->type != JSON_TOKEN_STRING )
    return;
  else if( bsTokenIs( parser, name, "priority" ) )
  {
    bsDecodeJsonText( buffer, sizeof(buffer), &parser->codestring[ value->offset ], value->length );
    for( index = 0 ; index < BS_BROADCAST_PRIORITY_COUNT ; index++ )
    {
      if( ccStrLowCmpWord( buffer, (char *)bsPriorityNames[index] ) )
        msg->priority = index;
    }
  }
  else if( bsTokenIs( parser, name, "only_before" ) )
    bsDecodeJsonText( msg->onlybefore, sizeof(msg->onlybefore), &parser->codestring[ value->offset ], value->length );
  else if( bsTokenIs( parser, name, "message" ) )
    bsDecodeJsonText( msg->text, sizeof(msg->text), &parser->codestring[ value->offset ], value->length );
  return;
}

int bsParseBroadcast( char *json, ioLog *log, bsBroadcast *ret )
{
  memset( ret, 0, sizeof(bsBroadcast) );
  if( !( bsParseObject( json, log, bsBroadcastKey, ret ) ) )
  {
    memset( ret, 0, sizeof(bsBroadcast) );
    return 0;
  }
  if( !( ret->text[0] ) )
    ret->priority = BS_BROADCAST_PRIORITY_NONE;
  return 1;
}

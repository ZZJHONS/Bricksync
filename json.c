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

#include <stdarg.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmhash.h"
#include "iolog.h"

#include "json.h"


/* JSON Lexical Parser */

/*
JSON Syntax Rules
JSON syntax is a subset of the JavaScript object notation syntax:
    Data is in name/value pairs
    Data is separated by commas
    Curly braces hold objects {object}
    Square brackets hold arrays [,,,]
*/


typedef struct
{
  /* Track contextual information */
  char *basestring;
  int linecount;
  ioLog *log;
} jsonLexParser;


#define JSON_DEBUGGING (0)


////


static const unsigned char jsonLexTable[256] =
{
  ['a']=1,['b']=1,['c']=1,['d']=1,['e']=1,['f']=1,['g']=1,['h']=1,['i']=1,['j']=1,['k']=1,['l']=1,['m']=1,['n']=1,['o']=1,['p']=1,['q']=1,['r']=1,['s']=1,['t']=1,['u']=1,['v']=1,['w']=1,['x']=1,['y']=1,['z']=1,
  ['A']=1,['B']=1,['C']=1,['D']=1,['E']=1,['F']=1,['G']=1,['H']=1,['I']=1,['J']=1,['K']=1,['L']=1,['M']=1,['N']=1,['O']=1,['P']=1,['Q']=1,['R']=1,['S']=1,['T']=1,['U']=1,['V']=1,['W']=1,['X']=1,['Y']=1,['Z']=1,
  ['0']=1,['1']=1,['2']=1,['3']=1,['4']=1,['5']=1,['6']=1,['7']=1,['8']=1,['9']=1,['-']=1,
  ['_']=1,
  [ 0 ]=2,['#']=2,[' ']=2,['\t']=2,['\r']=2,['\n']=2,
  ['.']=2,[',']=2,[':']=2,[';']=2,['[']=2,[']']=2,['(']=2,[')']=2,['{']=2,['}']=2,
  ['=']=2,['+']=2,['*']=2,['/']=2,['|']=2,['&']=2,['^']=2,['~']=2,
  ['\"']=2,['\'']=2
};
#define JSON_LEXTABLE_ILLEGAL (0)
#define JSON_LEXTABLE_IDENTIFIER (1)
#define JSON_LEXTABLE_OTHER (2)

static const unsigned char jsonLexTableSpace[256] =
{
  [' ']=1,['\t']=1,['\r']=1,
};

static const unsigned char jsonLexTableSeparator[256] =
{
  ['\0']=1,[' ']=1,['\t']=1,['\r']=1,
};


////


static char *jsonLexSkipSpace( char *string, int *retlineskip )
{
  unsigned char c;
  int lineskip;
  for( lineskip = 0 ; ; string++ )
  {
    c = *string;
    if( !( jsonLexTableSpace[ c ] ) )
    {
      if( c == '\n' )
      {
        lineskip++;
        continue;
      }
      break;
    }
  }
  *retlineskip = lineskip;
  return string;
}


/* Return string length, -1 if bad string */
static int jsonLexIdentifierLength( char *string )
{
  int index;
  unsigned char c, code;
  for( index = 0 ; ; index++ )
  {
    c = string[index];
    code = jsonLexTable[ c ];
    if( code == JSON_LEXTABLE_IDENTIFIER )
      continue;
    else if( code == JSON_LEXTABLE_OTHER )
      break;
    return -1;
  }
  return index;
}

/* Return int/float length, -1 if bad string */
static int jsonLexNumberLength( char *string, int *retfloatflag )
{
  int index, floatflag;
  char c;
  floatflag = 0;
  for( index = 0 ; ; index++ )
  {
    c = string[index];
    if( ( c >= '0' ) && ( c <= '9' ) )
      continue;
    else if( c == '-' )
    {
      if( index )
        return -1;
      else
        continue;
    }
    else if( c == '.' )
    {
      if( floatflag )
        return -1;
      floatflag = 1;
      continue;
    }
    break;
  }
  *retfloatflag = floatflag;
  return index;
}


////


static inline const int jsonLexCmpMem( char *s0, char *s1, int len )
{
  int i;
  for( i = 0 ; i < len ; i++ )
  {
    if( s0[i] != s1[i] )
      return 0;
  }
  return 1;
}


static const int jsonLexScanKeyword( char *string, int len )
{
  int tokentype;
  tokentype = JSON_TOKEN_IDENTIFIER;
  switch( len )
  {
    case 4:
      if( jsonLexCmpMem( string, "true", 4 ) )
        tokentype = JSON_TOKEN_TRUE;
      else if( jsonLexCmpMem( string, "null", 4 ) )
        tokentype = JSON_TOKEN_NULL;
      break;
    case 5:
      if( jsonLexCmpMem( string, "false", 5 ) )
        tokentype = JSON_TOKEN_FALSE;
      break;
    default:
      break;
  }
  return tokentype;
}


static int jsonLexFindStringEnd( char *string )
{
  int index;
  for( index = 0 ; string[index] ; index++ )
  {
    if( string[index] == '\\' )
    {
      if( !( string[++index] ) )
        break;
    }
    else if( string[index] == '\"' )
      return index;
  }
  return -1;
}


////


static char *jsonLexFindToken( jsonLexParser *parser, char *string, jsonToken *token, int *retincrflag )
{
  int tokentype, tokenlen, stringskip, incrflag;
  int floatflag, lineskip, offset;
  unsigned char c, c1, c2, code;

  string = jsonLexSkipSpace( string, &lineskip );
  parser->linecount += lineskip;

  stringskip = 0;
  incrflag = 1;
  c = *string;
  code = jsonLexTable[ c ];
  if( code == JSON_LEXTABLE_IDENTIFIER )
  {
    if( ( ( c >= '0' ) && ( c <= '9' ) ) || ( c == '-' ) )
    {
      /* Immediate number */
      tokenlen = jsonLexNumberLength( string, &floatflag );
      if( tokenlen == -1 )
        goto error;
      tokentype = ( floatflag ? JSON_TOKEN_FLOAT : JSON_TOKEN_INTEGER );
    }
    else
    {
      /* Identifier */
      tokenlen = jsonLexIdentifierLength( string );
      if( tokenlen == -1 )
        goto error;
      /* Recognize reserved keywords */
      tokentype = jsonLexScanKeyword( string, tokenlen );
    }
  }
  else if( code == JSON_LEXTABLE_OTHER )
  {
    /* Potentially a valid token */
    /* Default tokenlen set to 1 */
    tokenlen = 1;
    c1 = 0;
    if( c )
      c1 = string[1];
    switch( c )
    {
      case '\0':
        tokentype = JSON_TOKEN_END;
        break;
      case '.':
        tokentype = JSON_TOKEN_DOT;
        break;
      case ',':
        tokentype = JSON_TOKEN_COMMA;
        break;
      case ':':
        tokentype = JSON_TOKEN_COLON;
        break;
      case ';':
        tokentype = JSON_TOKEN_SEMICOLON;
        break;
      case '(':
        tokentype = JSON_TOKEN_LPAREN;
        break;
      case ')':
        tokentype = JSON_TOKEN_RPAREN;
        break;
      case '{':
        tokentype = JSON_TOKEN_LBRACE;
        break;
      case '}':
        tokentype = JSON_TOKEN_RBRACE;
        break;
      case '[':
        tokentype = JSON_TOKEN_LBRACKET;
        break;
      case ']':
        tokentype = JSON_TOKEN_RBRACKET;
        break;
      case '\'':
        tokentype = JSON_TOKEN_CHAR;
        if( c1 == '\0' )
          goto error;
        c2 = string[2];
        if( c2 != '\'' )
          goto error;
        string++;
        stringskip = 1;
        break;
      case '\"':
        tokentype = JSON_TOKEN_STRING;
        string++;
        offset = jsonLexFindStringEnd( string );
        if( offset == -1 )
          goto error;
        tokenlen = offset;
        stringskip = 1;
        break;
      default:
        goto badchar;
    }
  }
  else
  {
    badchar:
    ioPrintf( parser->log, 0, "JSON LEX: Line %d, offset %d: Bad character 0x%x in string.\n", parser->linecount, (int)( string - parser->basestring ), c );
    return 0;
  }

  token->type = tokentype;
  token->offset = (int)( string - parser->basestring );
  token->length = tokenlen;
  *retincrflag = incrflag;
  return string + ( tokenlen + stringskip );

  error:
  ioPrintf( parser->log, 0, "JSON LEX: Line %d, offset %d: Parse error!\n", parser->linecount, (int)( string - parser->basestring ) );
  return 0;
}


jsonTokenBuffer *jsonLexParse( char *string, ioLog *log )
{
  int incrflag;
  jsonTokenBuffer *buf, *bufnext;
  jsonTokenBuffer *buflist;
  jsonToken *token;
  jsonLexParser parser;

  parser.basestring = string;
  parser.linecount = 0;
  parser.log = log;

  buf = malloc( sizeof(jsonTokenBuffer) );
  buf->tokencount = 0;
  buf->next = 0;
  buflist = buf;

  for( ; ; )
  {
    if( buf->tokencount >= JSON_TOKEN_BUFFER_SIZE )
    {
      bufnext = malloc( sizeof(jsonTokenBuffer) );
      bufnext->tokencount = 0;
      bufnext->next = 0;
      buf->next = bufnext;
      buf = bufnext;
    }
    token = &buf->tokenlist[ buf->tokencount ];

    string = jsonLexFindToken( &parser, string, token, &incrflag );
    if( !( string ) )
    {
      jsonLexFree( buflist );
      return 0;
    }
    buf->tokencount += incrflag;
#if 0
    if( incrflag )
      ioPrintf( parser.log, 0, "Token type %d : %.*s\n", (int)token->type, (int)token->length, &parser.basestring[ token->offset ] );
#endif
    if( token->type == JSON_TOKEN_END )
      break;
  }

  return buflist;
}


void jsonLexFree( jsonTokenBuffer *buf )
{
  jsonTokenBuffer *next;
  for( ; buf ; buf = next )
  {
    next = buf->next;
    free( buf );
  }
  return;
}


////


void jsonTokenInit( jsonParser *parser, char *codestring, jsonTokenBuffer *tokenbuf, ioLog *log )
{
  parser->token = &tokenbuf->tokenlist[0];
  parser->nexttoken = &tokenbuf->tokenlist[1];
  parser->tokentype = (parser->token)->type;
  parser->codestring = codestring;
  parser->tokenbufindex = 2;
  parser->tokenbuf = tokenbuf;
  parser->errorcount = 0;
  parser->depth = 0;
  parser->log = log;
  return;
}

void jsonTokenIncrement( jsonParser *parser )
{
  jsonTokenBuffer *tokenbuf;
  if( parser->tokentype == JSON_TOKEN_END )
    return;
  tokenbuf = parser->tokenbuf;
  if( parser->tokenbufindex == tokenbuf->tokencount )
  {
    tokenbuf = tokenbuf->next;
    parser->tokenbuf = tokenbuf;
    parser->tokenbufindex = 0;
  }
  parser->token = parser->nexttoken;
  parser->nexttoken = &tokenbuf->tokenlist[ parser->tokenbufindex ];
  parser->tokenbufindex++;
  parser->tokentype = (parser->token)->type;
  return;
}


////


void jsonLexDebug( jsonParser *parser )
{
  int tokenindex;
  jsonTokenBuffer *tokenbuf;
  jsonToken *token;
  for( tokenbuf = parser->tokenbuf ; tokenbuf ; tokenbuf = tokenbuf->next )
  {
    ioPrintf( parser->log, 0, "Token Buf : %d tokens\n", tokenbuf->tokencount );
    for( tokenindex = 0 ; tokenindex < tokenbuf->tokencount ; tokenindex++ )
    {
      token = &tokenbuf->tokenlist[ tokenindex ];
      ioPrintf( parser->log, 0, "  Token %d, type %d : %.*s\n", tokenindex, (int)token->type, (int)token->length, &parser->codestring[ token->offset ] );
    }
  }
  return;
}


////


/* We accepted a '{' */
int jsonParserSkipObject( jsonParser *parser )
{
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;
#if JSON_DEBUGGING
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Object ( depth %d )\n", parser->depth );
  parser->depth++;
#endif
  for( ; ; )
  {
#if JSON_DEBUGGING
    token = parser->token;
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if JSON_DEBUGGING
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    switch( parser->tokentype )
    {
      case JSON_TOKEN_INTEGER:
      case JSON_TOKEN_FLOAT:
      case JSON_TOKEN_STRING:
      case JSON_TOKEN_TRUE:
      case JSON_TOKEN_FALSE:
      case JSON_TOKEN_NULL:
        jsonTokenIncrement( parser );
        break;
      case JSON_TOKEN_LBRACE:
        jsonTokenIncrement( parser );
        if( !( jsonParserSkipObject( parser ) ) )
          return 0;
        jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
        break;
      case JSON_TOKEN_LBRACKET:
        jsonTokenIncrement( parser );
        if( !( jsonParserSkipList( parser ) ) )
          return 0;
        jsonTokenExpect( parser, JSON_TOKEN_RBRACKET );
        break;
      default:
        ioPrintf( parser->log, 0, "JSON PARSER: Token %d unexpected, offset %d\n", parser->tokentype, (parser->token)->offset );
        return 0;
    }
    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if JSON_DEBUGGING
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Object\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}

/* We accepted a '[' */
int jsonParserSkipList( jsonParser *parser )
{
  if( parser->tokentype == JSON_TOKEN_RBRACKET )
    return 1;
#if JSON_DEBUGGING
  int i;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering List ( depth %d )\n", parser->depth );
  parser->depth++;
#endif
  for( ; ; )
  {
#if JSON_DEBUGGING
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Elem\n" );
#endif

    switch( parser->tokentype )
    {
      case JSON_TOKEN_INTEGER:
      case JSON_TOKEN_FLOAT:
      case JSON_TOKEN_STRING:
      case JSON_TOKEN_TRUE:
      case JSON_TOKEN_FALSE:
      case JSON_TOKEN_NULL:
        jsonTokenIncrement( parser );
        break;
      case JSON_TOKEN_LBRACE:
        jsonTokenIncrement( parser );
        if( !( jsonParserSkipObject( parser ) ) )
          return 0;
        jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
        break;
      case JSON_TOKEN_LBRACKET:
        jsonTokenIncrement( parser );
        if( !( jsonParserSkipList( parser ) ) )
          return 0;
        jsonTokenExpect( parser, JSON_TOKEN_RBRACKET );
        break;
      default:
        ioPrintf( parser->log, 0, "JSON PARSER: Token %d unexpected, offset %d\n", parser->tokentype, (parser->token)->offset );
        return 0;
    }

#if JSON_DEBUGGING
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Elem Success\n" );
#endif

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if JSON_DEBUGGING
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting List\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}

int jsonParserSkipValue( jsonParser *parser )
{
  switch( parser->tokentype )
  {
    case JSON_TOKEN_INTEGER:
    case JSON_TOKEN_FLOAT:
    case JSON_TOKEN_STRING:
    case JSON_TOKEN_TRUE:
    case JSON_TOKEN_FALSE:
    case JSON_TOKEN_NULL:
      jsonTokenIncrement( parser );
      break;
    case JSON_TOKEN_LBRACE:
      jsonTokenIncrement( parser );
      if( !( jsonParserSkipObject( parser ) ) )
        return 0;
      jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
      break;
    case JSON_TOKEN_LBRACKET:
      jsonTokenIncrement( parser );
      if( !( jsonParserSkipList( parser ) ) )
        return 0;
      jsonTokenExpect( parser, JSON_TOKEN_RBRACKET );
      break;
    default:
      ioPrintf( parser->log, 0, "JSON PARSER: Token %d unexpected, offset %d\n", parser->tokentype, (parser->token)->offset );
      parser->errorcount++;
      return 0;
  }
  return 1;
}


/* We accepted a '[' */
/* Expects [{callback},{callback},{callback}] */
int jsonParserListObjects( jsonParser *parser, void *uservalue, int (*parseobject)( jsonParser *parser, void *uservalue ), int recursiondepth )
{
  if( parser->tokentype == JSON_TOKEN_RBRACKET )
    return 1;

#if JSON_DEBUGGING
  int i;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering List ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  if( ( recursiondepth > 0 ) && jsonTokenAccept( parser, JSON_TOKEN_LBRACKET ) )
  {
    for( ; ; )
    {
      if( !( jsonParserListObjects( parser, uservalue, parseobject, recursiondepth - 1 ) ) )
        return 0;
      jsonTokenExpect( parser, JSON_TOKEN_RBRACKET );
      if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
        break;
      jsonTokenExpect( parser, JSON_TOKEN_LBRACKET );
    }
  }
  else
  {
    for( ; ; )
    {
#if JSON_DEBUGGING
      for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
        ioPrintf( parser->log, 0, "Processing Elem\n" );
#endif
      switch( parser->tokentype )
      {
        case JSON_TOKEN_LBRACE:
          jsonTokenIncrement( parser );
          if( !( parseobject( parser, uservalue ) ) )
            return 0;
          jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
          break;
        default:
          ioPrintf( parser->log, 0, "JSON PARSER: Token %d unexpected, offset %d\n", parser->tokentype, (parser->token)->offset );
          parser->errorcount++;
          return 0;
      }
#if JSON_DEBUGGING
      for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
        ioPrintf( parser->log, 0, "Processing Elem Success\n" );
#endif
      if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
        break;
    }

  }

#if JSON_DEBUGGING
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting List\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


int jsonReadInteger( jsonParser *parser, int64_t *retint, int allowfailflag )
{
  int64_t readint;
  jsonToken *token;
  token = parser->token;
  switch( parser->tokentype )
  {
    case JSON_TOKEN_INTEGER:
    case JSON_TOKEN_STRING:
      if( ccSeqParseInt64( &parser->codestring[ token->offset ], token->length, &readint ) )
        break;
    default:
      if( allowfailflag )
        jsonParserSkipValue( parser );
      else
      {
        ioPrintf( parser->log, 0, "JSON PARSER: Error, integer parse error, offset %d\n", (parser->token)->offset );
        parser->errorcount++;
      }
      return 0;
  }
  jsonTokenIncrement( parser );
  *retint = readint;
  return 1;
}

int jsonReadDouble( jsonParser *parser, double *retdouble )
{
  double readdouble;
  jsonToken *token;
  token = parser->token;
  switch( parser->tokentype )
  {
    case JSON_TOKEN_INTEGER:
    case JSON_TOKEN_FLOAT:
    case JSON_TOKEN_STRING:
      if( ccSeqParseDouble( &parser->codestring[ token->offset ], token->length, &readdouble ) )
        break;
    default:
      ioPrintf( parser->log, 0, "JSON PARSER: Error, float parse error, offset %d\n", (parser->token)->offset );
      parser->errorcount++;
      return 0;
  }
  jsonTokenIncrement( parser );
  *retdouble = readdouble;
  return 1;
}


////



/* Build string with escape chars as required, returned string must be free()'d */
char *jsonEncodeEscapeString( char *string, int length, int *retlength )
{
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( 2*length + 1 );
  for( dst = dstbase ; length ; length--, string++ )
  {
    c = *string;
    if( c == '\\' )
      c = '\\';
    else if( c == '"' )
      c = '"';
    else if( c == '\b' )
      c = 'b';
    else if( c == '\f' )
      c = 'f';
    else if( c == '\n' )
      c = 'n';
    else if( c == '\r' )
      c = 'r';
    else if( c == '\t' )
      c = 't';
    else
    {
      *dst++ = c;
      continue;
    }
    dst[0] = '\\';
    dst[1] = c;
    dst += 2;
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );

  return dstbase;
}



/* Build string with decoded escape chars, returned string must be free()'d */
char *jsonDecodeEscapeString( char *string, int length, int *retlength )
{
  int utf8length;
  char *dst, *dstbase;
  unsigned char c;
  uint32_t unicode;

  dstbase = malloc( length + 1 );
  for( dst = dstbase ; length ; length--, string++ )
  {
    c = *string;
    if( c != '\\' )
      *dst++ = c;
    else
    {
      if( !( --length ) )
        goto error;
      string++;
      c = *string;
      if( c == '\\' )
        *dst++ = '\\';
      else if( c == '"' )
        *dst++ = '"';
      else if( c == '/' )
        *dst++ = '/';
      else if( c == 'b' )
        *dst++ = '\b';
      else if( c == 'f' )
        *dst++ = '\f';
      else if( c == 'n' )
        *dst++ = '\n';
      else if( c == 'r' )
        *dst++ = '\r';
      else if( c == 't' )
        *dst++ = '\t';
      else if( c == 'u' )
      {
        length -= 4;
        if( length < 0 )
          goto error;
        string++;
        unicode  = ccCharHexBase( string[3] );
        unicode |= ccCharHexBase( string[2] ) << 4;
        unicode |= ccCharHexBase( string[1] ) << 8;
        unicode |= ccCharHexBase( string[0] ) << 12;
        utf8length = ccUnicodeToUtf8( dst, unicode );
        if( !( utf8length ) )
          goto error;
        dst += utf8length;
        string += 3;
      }
      else
        goto error;
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



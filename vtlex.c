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

#include "vtlex.h"


/* VT Lexical parser */


typedef struct
{
  /* Track contextual information */
  char *basestring;
  int linecount;
} vtLexParser;


////


static const unsigned char vtLexTable[256] =
{
  ['a']=1,['b']=1,['c']=1,['d']=1,['e']=1,['f']=1,['g']=1,['h']=1,['i']=1,['j']=1,['k']=1,['l']=1,['m']=1,['n']=1,['o']=1,['p']=1,['q']=1,['r']=1,['s']=1,['t']=1,['u']=1,['v']=1,['w']=1,['x']=1,['y']=1,['z']=1,
  ['A']=1,['B']=1,['C']=1,['D']=1,['E']=1,['F']=1,['G']=1,['H']=1,['I']=1,['J']=1,['K']=1,['L']=1,['M']=1,['N']=1,['O']=1,['P']=1,['Q']=1,['R']=1,['S']=1,['T']=1,['U']=1,['V']=1,['W']=1,['X']=1,['Y']=1,['Z']=1,
  ['0']=1,['1']=1,['2']=1,['3']=1,['4']=1,['5']=1,['6']=1,['7']=1,['8']=1,['9']=1,
  ['_']=1,
  [ 0 ]=2,['#']=2,[' ']=2,['\t']=2,['\r']=2,['\n']=2,
  ['.']=2,[',']=2,[':']=2,[';']=2,['[']=2,[']']=2,['(']=2,[')']=2,['{']=2,['}']=2,
  ['=']=2,['+']=2,['-']=2,['*']=2,['/']=2,['|']=2,['&']=2,['^']=2,['~']=2,
  ['\"']=2,['\'']=2
};
#define VT_LEXTABLE_ILLEGAL (0)
#define VT_LEXTABLE_IDENTIFIER (1)
#define VT_LEXTABLE_OTHER (2)

static const unsigned char vtLexTableSpace[256] =
{
  [' ']=1,['\t']=1,['\r']=1,
};

static const unsigned char vtLexTableSeparator[256] =
{
  ['\0']=1,[' ']=1,['\t']=1,['\r']=1,
};


////


static char *vtLexSkipSpace( char *string, int *retlineskip )
{
  unsigned char c;
  int lineskip;
  for( lineskip = 0 ; ; string++ )
  {
    c = *string;
    if( !( vtLexTableSpace[ c ] ) )
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


static int vtLexSkipLine( char *string )
{
  int index;
  unsigned char c;
  for( index = 0 ; ; index++ )
  {
    c = string[index];
    if( ( c == '\n' ) || ( c == '\0' ) )
      break;
  }
  return index;
}


/* Return string length, -1 if bad string */
static int vtLexIdentifierLength( char *string )
{
  int index;
  unsigned char c, code;
  for( index = 0 ; ; index++ )
  {
    c = string[index];
    code = vtLexTable[ c ];
    if( code == VT_LEXTABLE_IDENTIFIER )
      continue;
    else if( code == VT_LEXTABLE_OTHER )
      break;
    return -1;
  }
  return index;
}

/* Return int/float length, -1 if bad string */
static int vtLexNumberLength( char *string, int *retfloatflag )
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


static inline const int vtLexCmpMem( char *s0, char *s1, int len )
{
  int i;
  for( i = 0 ; i < len ; i++ )
  {
    if( s0[i] != s1[i] )
      return 0;
  }
  return 1;
}


static const int vtLexScanKeyword( char *string, int len )
{
  int tokentype;
  tokentype = VT_TOKEN_IDENTIFIER;
  switch( len )
  {
    case 2:
      if( vtLexCmpMem( string, "if", 2 ) )
        tokentype = VT_TOKEN_IF;
      break;
    case 3:
      if( vtLexCmpMem( string, "for", 3 ) )
        tokentype = VT_TOKEN_FOR;
      else if( vtLexCmpMem( string, "new", 3 ) )
        tokentype = VT_TOKEN_FOR;
      break;
    case 4:
      if( vtLexCmpMem( string, "else", 4 ) )
        tokentype = VT_TOKEN_ELSE;
      break;
    case 5:
      if( vtLexCmpMem( string, "while", 5 ) )
        tokentype = VT_TOKEN_WHILE;
      else if( vtLexCmpMem( string, "break", 5 ) )
        tokentype = VT_TOKEN_BREAK;
      break;
    case 6:
      if( vtLexCmpMem( string, "delete", 6 ) )
        tokentype = VT_TOKEN_DELETE;
      else if( vtLexCmpMem( string, "return", 6 ) )
        tokentype = VT_TOKEN_RETURN;
      break;
    case 8:
      if( vtLexCmpMem( string, "continue", 8 ) )
        tokentype = VT_TOKEN_CONTINUE;
      break;
    default:
      break;
  }
  return tokentype;
}


static int vtLexFindChar( char *string, char c )
{
  int index;
  for( index = 0 ; string[index] != c ; index++ )
  {
    if( string[index] == 0 )
      return -1;
  }
  return index;
}


////


char *vtLexFindToken( vtLexParser *parser, char *string, vtToken *token, int *retincrflag )
{
  int tokentype, tokenlen, stringskip, incrflag;
  int floatflag, lineskip, offset;
  unsigned char c, c1, c2, code;

  string = vtLexSkipSpace( string, &lineskip );
  parser->linecount += lineskip;

  stringskip = 0;
  incrflag = 1;
  c = *string;
  code = vtLexTable[ c ];
  if( code == VT_LEXTABLE_IDENTIFIER )
  {
    if( ( c >= '0' ) && ( c <= '9' ) )
    {
      /* Immediate number */
      parsenumber:
      tokenlen = vtLexNumberLength( string, &floatflag );
      if( tokenlen == -1 )
        goto error;
      tokentype = ( floatflag ? VT_TOKEN_FLOAT : VT_TOKEN_INTEGER );
    }
    else
    {
      /* Identifier */
      tokenlen = vtLexIdentifierLength( string );
      if( tokenlen == -1 )
        goto error;
      /* Recognize reserved keywords */
      tokentype = vtLexScanKeyword( string, tokenlen );
    }
  }
  else if( code == VT_LEXTABLE_OTHER )
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
        tokentype = VT_TOKEN_END;
        break;
      case '#':
        tokentype = VT_TOKEN_DOT;
        tokenlen += vtLexSkipLine( string );
        incrflag = 0;
        break;
      case '.':
        tokentype = VT_TOKEN_DOT;
        break;
      case ',':
        tokentype = VT_TOKEN_COMMA;
        break;
      case ':':
        tokentype = VT_TOKEN_COLON;
        break;
      case ';':
        tokentype = VT_TOKEN_SEMICOLON;
        break;
      case '(':
        tokentype = VT_TOKEN_LPAREN;
        break;
      case ')':
        tokentype = VT_TOKEN_RPAREN;
        break;
      case '{':
        tokentype = VT_TOKEN_LBRACE;
        break;
      case '}':
        tokentype = VT_TOKEN_RBRACE;
        break;
      case '[':
        tokentype = VT_TOKEN_LBRACKET;
        break;
      case ']':
        tokentype = VT_TOKEN_RBRACKET;
        break;
      case '\'':
        tokentype = VT_TOKEN_CHAR;
        if( c1 == '\0' )
          goto error;
        c2 = string[2];
        if( c2 != '\'' )
          goto error;
        string++;
        stringskip = 1;
        break;
      case '\"':
        tokentype = VT_TOKEN_STRING;
        string++;
        offset = vtLexFindChar( string, '\"' );
        if( offset == -1 )
          goto error;
        tokenlen = offset;
        stringskip = 1;
        break;
      case '=':
        tokentype = VT_TOKEN_MOV;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_CMPEQ;
          tokenlen = 2;
        }
        break;
      case '+':
        tokentype = VT_TOKEN_ADD;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STADD;
          tokenlen = 2;
        }
        break;
      case '-':
        tokentype = VT_TOKEN_SUB;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STSUB;
          tokenlen = 2;
        }
        else if( ( c1 >= '0' ) && ( c1 <= '9' ) )
          goto parsenumber;
        break;
      case '*':
        tokentype = VT_TOKEN_MUL;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STMUL;
          tokenlen = 2;
        }
        break;
      case '/':
        tokentype = VT_TOKEN_DIV;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STDIV;
          tokenlen = 2;
        }
        else if( c1 == '/' )
        {
          tokenlen += vtLexSkipLine( string );
          incrflag = 0;
        }
        break;
      case '%':
        tokentype = VT_TOKEN_MOD;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STMOD;
          tokenlen = 2;
        }
        break;
      case '|':
        tokentype = VT_TOKEN_OR;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STOR;
          tokenlen = 2;
        }
        break;
      case '^':
        tokentype = VT_TOKEN_XOR;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STXOR;
          tokenlen = 2;
        }
        break;
      case '&':
        tokentype = VT_TOKEN_AND;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_STAND;
          tokenlen = 2;
        }
        break;
      case '~':
        tokentype = VT_TOKEN_NOT;
        break;
      case '<':
        tokentype = VT_TOKEN_CMPLT;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_CMPLE;
          tokenlen = 2;
        }
        break;
      case '>':
        tokentype = VT_TOKEN_CMPGT;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_CMPGE;
          tokenlen = 2;
        }
        break;
      case '!':
        tokentype = VT_TOKEN_LOGNOT;
        if( c1 == '=' )
        {
          tokentype = VT_TOKEN_CMPNE;
          tokenlen = 2;
        }
        break;
      default:
        goto badchar;
    }
  }
  else
  {
    badchar:
    printf( "VT LEX: Line %d: Bad character 0x%x in string.\n", parser->linecount, c );
    return 0;
  }

  token->type = tokentype;
  token->offset = (int)( string - parser->basestring );
  token->length = tokenlen;
  *retincrflag = incrflag;
  return string + ( tokenlen + stringskip );

  error:
  printf( "VT LEX: Line %d: Parse error!\n", parser->linecount );
  return 0;
}


vtTokenBuffer *vtLexParse( char *string )
{
  int incrflag;
  vtTokenBuffer *buf, *bufnext;
  vtTokenBuffer *buflist;
  vtToken *token;
  vtLexParser parser;

  parser.basestring = string;
  parser.linecount = 0;

  buf = malloc( sizeof(vtTokenBuffer) );
  buf->tokencount = 0;
  buf->next = 0;
  buflist = buf;

  for( ; ; )
  {
    if( buf->tokencount >= VT_TOKEN_BUFFER_SIZE )
    {
      bufnext = malloc( sizeof(vtTokenBuffer) );
      bufnext->tokencount = 0;
      bufnext->next = 0;
      buf->next = bufnext;
      buf = bufnext;
    }
    token = &buf->tokenlist[ buf->tokencount ];

    string = vtLexFindToken( &parser, string, token, &incrflag );
    if( !( string ) )
    {
      vtLexFree( buflist );
      return 0;
    }
    buf->tokencount += incrflag;
#if 0
    if( incrflag )
      printf( "Token type %d : %.*s\n", (int)token->type, (int)token->length, &parser.basestring[ token->offset ] );
#endif
    if( token->type == VT_TOKEN_END )
      break;
  }

  return buflist;
}


void vtLexFree( vtTokenBuffer *buf )
{
  vtTokenBuffer *next;
  for( ; buf ; buf = next )
  {
    next = buf->next;
    free( buf );
  }
  return;
}

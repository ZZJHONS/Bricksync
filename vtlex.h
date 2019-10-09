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

enum
{
  VT_TOKEN_END,
  VT_TOKEN_IDENTIFIER, /* a-z,A-Z,0-9,_ */
  VT_TOKEN_INTEGER,
  VT_TOKEN_FLOAT,
  VT_TOKEN_CHAR,
  VT_TOKEN_STRING,

  VT_TOKEN_DOT,         /* "." */
  VT_TOKEN_COMMA,       /* "," */
  VT_TOKEN_COLON,       /* ":" */
  VT_TOKEN_SEMICOLON,   /* ";" */
  VT_TOKEN_LPAREN,      /* "(" */
  VT_TOKEN_RPAREN,      /* ")" */
  VT_TOKEN_LBRACE,      /* "{" */
  VT_TOKEN_RBRACE,      /* "}" */
  VT_TOKEN_LBRACKET,    /* "[" */
  VT_TOKEN_RBRACKET,    /* "]" */

  VT_TOKEN_MOV,    /* "=" */
  VT_TOKEN_ADD,    /* "+" */
  VT_TOKEN_SUB,    /* "-" if not followed by 0..9 */
  VT_TOKEN_MUL,    /* "*" */
  VT_TOKEN_DIV,    /* "/" */
  VT_TOKEN_MOD,    /* "%" */
  VT_TOKEN_OR,     /* "|" */
  VT_TOKEN_XOR,    /* "^" */
  VT_TOKEN_AND,    /* "&" */
  VT_TOKEN_SHL,    /* "<<" */
  VT_TOKEN_SHR,    /* ">>" */
  VT_TOKEN_ROL,    /* "<<<" */
  VT_TOKEN_ROR,    /* ">>>" */
  VT_TOKEN_STADD,  /* "+=" */
  VT_TOKEN_STSUB,  /* "-=" */
  VT_TOKEN_STMUL,  /* "*=" */
  VT_TOKEN_STDIV,  /* "/=" */
  VT_TOKEN_STMOD,  /* "%=" */
  VT_TOKEN_STOR,   /* "|=" */
  VT_TOKEN_STXOR,  /* "^=" */
  VT_TOKEN_STAND,  /* "&=" */
  VT_TOKEN_STSHL,  /* "<<=" */
  VT_TOKEN_STSHR,  /* ">>=" */
  VT_TOKEN_STROL,  /* "<<<=" */
  VT_TOKEN_STROR,  /* ">>>=" */

  VT_TOKEN_NOT,    /* "~" */

  VT_TOKEN_CMPEQ,  /* "==" */
  VT_TOKEN_CMPLT,  /* "<" */
  VT_TOKEN_CMPLE,  /* "<=" */
  VT_TOKEN_CMPGT,  /* ">" */
  VT_TOKEN_CMPGE,  /* ">=" */
  VT_TOKEN_CMPNE,  /* "!=" */

  VT_TOKEN_LOGNOT, /* "!" */
  VT_TOKEN_LOGOR,  /* "||" */
  VT_TOKEN_LOGAND, /* "&&" */

  VT_TOKEN_IF,
  VT_TOKEN_ELSE,
  VT_TOKEN_FOR,
  VT_TOKEN_WHILE,
  VT_TOKEN_RETURN,
  VT_TOKEN_NEW,
  VT_TOKEN_DELETE,
  VT_TOKEN_CONTINUE,
  VT_TOKEN_BREAK,

  VT_TOKEN_COUNT
};


/*
Produces a sequence of tokens for input
*/

#define VT_TOKEN_BUFFER_SIZE (256)

typedef struct
{
  uint16_t type;
  uint16_t length;
  uint32_t offset;
} vtToken;

typedef struct
{
  int tokencount;
  vtToken tokenlist[VT_TOKEN_BUFFER_SIZE];
  void *next;
} vtTokenBuffer;


////


vtTokenBuffer *vtLexParse( char *string );

void vtLexFree( vtTokenBuffer *buf );



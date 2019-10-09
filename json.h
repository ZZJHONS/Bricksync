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
  JSON_TOKEN_END,
  JSON_TOKEN_IDENTIFIER, /* a-z,A-Z,0-9,_ */
  JSON_TOKEN_INTEGER,
  JSON_TOKEN_FLOAT,
  JSON_TOKEN_CHAR,
  JSON_TOKEN_STRING,

  JSON_TOKEN_DOT,         /* "." */
  JSON_TOKEN_COMMA,       /* "," */
  JSON_TOKEN_COLON,       /* ":" */
  JSON_TOKEN_SEMICOLON,   /* ";" */
  JSON_TOKEN_LPAREN,      /* "(" */
  JSON_TOKEN_RPAREN,      /* ")" */
  JSON_TOKEN_LBRACE,      /* "{" */
  JSON_TOKEN_RBRACE,      /* "}" */
  JSON_TOKEN_LBRACKET,    /* "[" */
  JSON_TOKEN_RBRACKET,    /* "]" */

  JSON_TOKEN_TRUE,        /* true */
  JSON_TOKEN_FALSE,       /* false */
  JSON_TOKEN_NULL,        /* null */

  JSON_TOKEN_COUNT
};


#define JSON_TOKEN_BUFFER_SIZE (256)

typedef struct
{
  uint16_t type;
  uint16_t length;
  uint32_t offset;
} jsonToken;

typedef struct
{
  int tokencount;
  jsonToken tokenlist[JSON_TOKEN_BUFFER_SIZE];
  void *next;
} jsonTokenBuffer;



jsonTokenBuffer *jsonLexParse( char *string, ioLog *log );

void jsonLexFree( jsonTokenBuffer *buf );



////



typedef struct
{
  jsonToken *token;
  jsonToken *nexttoken;
  int tokentype;

  char *codestring;
  int tokenbufindex;
  jsonTokenBuffer *tokenbuf;
  int errorcount;

  void *uservalue;
  int depth;
  ioLog *log;

} jsonParser;


void jsonTokenInit( jsonParser *parser, char *codestring, jsonTokenBuffer *tokenbuf, ioLog *log );

void jsonTokenIncrement( jsonParser *parser );

static inline jsonToken *jsonTokenAccept( jsonParser *parser, int tokentype )
{
  jsonToken *token;
  /* Do we accept requested token? */
  if( tokentype != parser->tokentype )
    return 0;
  token = parser->token;
  jsonTokenIncrement( parser );
  return token;
}

static inline jsonToken *jsonTokenExpect( jsonParser *parser, int tokentype )
{
  jsonToken *token;
  /* Do we have expected token? */
  if( tokentype == parser->tokentype )
  {
    token = parser->token;
    jsonTokenIncrement( parser );
    return token;
  }
  printf( "JSON PARSER: Error, token %d when token %d was expected, offset %d\n", (parser->token)->type, tokentype, (parser->token)->offset );
  parser->errorcount++;
  return 0;
}


////


void jsonLexDebug( jsonParser *parser );


/* Skip object, token '{' already accepted */
int jsonParserSkipObject( jsonParser *parser );

/* Skip list, token '[' already accepted */
int jsonParserSkipList( jsonParser *parser );

/* Skip value, tokens 'name:' already accepted */
int jsonParserSkipValue( jsonParser *parser );

/* Expects [{callback},{callback},{callback}], token '[' already accepted */
int jsonParserListObjects( jsonParser *parser, void *uservalue, int (*parseobject)( jsonParser *parser, void *uservalue ), int recursiondepth );

int jsonReadInteger( jsonParser *parser, int64_t *retint, int allowfailflag );
int jsonReadDouble( jsonParser *parser, double *retdouble );


////


/* Build string with escape chars as required, returned string must be free()'d */
char *jsonEncodeEscapeString( char *string, int length, int *retlength );

/* Build string with decoded escape chars, returned string must be free()'d */
char *jsonDecodeEscapeString( char *string, int length, int *retlength );



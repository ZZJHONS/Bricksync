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

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"
#include "iolog.h"
#include "debugtrack.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <direct.h>
#else
 #error Unknown/Unsupported platform!
#endif

#include "tcp.h"
#include "tcphttp.h"
#include "oauth.h"
#include "exclperm.h"
#include "journal.h"

#include "vtlex.h"

#include "bsx.h"
#include "bsxpg.h"
#include "json.h"
#include "bsorder.h"
#include "bricklink.h"
#include "brickowl.h"
#include "colortable.h"
#include "bstranslation.h"

#include "bricksync.h"


////


typedef struct
{
  vtToken *token;
  vtToken *nexttoken;
  int tokentype;

  char *codestring;
  int tokenbufindex;
  vtTokenBuffer *tokenbuf;

  int errorcount;

} bsConfParser;


static void vtTokenInit( bsConfParser *parser, char *codestring, vtTokenBuffer *tokenbuf )
{
  parser->token = &tokenbuf->tokenlist[0];
  parser->nexttoken = &tokenbuf->tokenlist[1];
  parser->tokentype = (parser->token)->type;
  parser->codestring = codestring;
  parser->tokenbufindex = 2;
  parser->tokenbuf = tokenbuf;
  parser->errorcount = 0;
  return;
}

static void vtTokenIncrement( bsConfParser *parser )
{
  vtTokenBuffer *tokenbuf;
  if( parser->tokentype == VT_TOKEN_END )
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

static inline vtToken *vtTokenAccept( bsConfParser *parser, int tokentype )
{
  vtToken *token;
  /* Do we accept requested token? */
  if( tokentype != parser->tokentype )
    return 0;
  token = parser->token;
  vtTokenIncrement( parser );
  return token;
}

static inline vtToken *vtTokenExpect( bsConfParser *parser, int tokentype )
{
  vtToken *token;
  /* Do we have expected token? */
  if( tokentype == parser->tokentype )
  {
    token = parser->token;
    vtTokenIncrement( parser );
    return token;
  }
  parser->errorcount++;
  return 0;
}


////


static int bsConfResolveLine( bsContext *context, bsConfParser *parser, int *retlineoffset )
{
  int tokenoffset, offset, linecount, lineoffset;
  char *codestring;
  vtToken *token;

  token = parser->token;
  tokenoffset = token->offset;
  codestring = parser->codestring;

  linecount = 1;
  lineoffset = 0;
  for( offset = 0 ; offset < tokenoffset ; offset++ )
  {
    if( codestring[offset] == '\n' )
    {
      linecount++;
      lineoffset = 0;
    }
    else
      lineoffset++;
  }

  if( retlineoffset )
    *retlineoffset = lineoffset;
  return linecount;
}


static void bsConfErrorUnknownIdentifier( bsContext *context, bsConfParser *parser, vtToken *token )
{
  int linecount, lineoffset;
  linecount = bsConfResolveLine( context, parser, &lineoffset );
  ioPrintf( &context->output, 0, BSMSG_ERROR "Unknown variable or scope \"" IO_RED "%.*s" IO_WHITE "\" at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", (int)token->length, &parser->codestring[ token->offset ], linecount, lineoffset );
  return;
}

static void bsConfErrorUnknownScopeMember( bsContext *context, bsConfParser *parser, vtToken *token )
{
  int linecount, lineoffset;
  linecount = bsConfResolveLine( context, parser, &lineoffset );
  ioPrintf( &context->output, 0, BSMSG_ERROR "Unknown scope member \"" IO_RED "%.*s" IO_WHITE "\" at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", (int)token->length, &parser->codestring[ token->offset ], linecount, lineoffset );
  return;
}

static void bsConfErrorScopeEntryFailure( bsContext *context, bsConfParser *parser )
{
  int linecount, lineoffset;
  linecount = bsConfResolveLine( context, parser, &lineoffset );
  ioPrintf( &context->output, 0, BSMSG_ERROR "Expected a '.' to enter scope at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
  return;
}

static void bsConfErrorScopeMemberFailure( bsContext *context, bsConfParser *parser )
{
  int linecount, lineoffset;
  linecount = bsConfResolveLine( context, parser, &lineoffset );
  ioPrintf( &context->output, 0, BSMSG_ERROR "Expected an identifier as member of scope at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
  return;
}


static int bsConfReadString( bsContext *context, bsConfParser *parser, char **string )
{
  int linecount, lineoffset;
  vtToken *token;
  token = parser->token;

  if( !( vtTokenExpect( parser, VT_TOKEN_MOV ) ) )
  {
    linecount = bsConfResolveLine( context, parser, &lineoffset );
    ioPrintf( &context->output, 0, BSMSG_ERROR "Expected an assignment operator '=' at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
    return 0;
  }
  if( !( token = vtTokenExpect( parser, VT_TOKEN_STRING ) ) )
  {
    linecount = bsConfResolveLine( context, parser, &lineoffset );
    ioPrintf( &context->output, 0, BSMSG_ERROR "Expected a string litteral for assignment at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
    return 0;
  }
  if( *string )
    free( *string );

  *string = malloc( token->length + 1 );
  memcpy( *string, &parser->codestring[ token->offset ], token->length );
  (*string)[token->length] = 0;
  return 1;
}


static int bsConfReadInteger( bsContext *context, bsConfParser *parser, int *retint )
{
  int linecount, lineoffset;
  int64_t readint;
  vtToken *token;
  token = parser->token;
  if( !( vtTokenExpect( parser, VT_TOKEN_MOV ) ) )
  {
    linecount = bsConfResolveLine( context, parser, &lineoffset );
    ioPrintf( &context->output, 0, BSMSG_ERROR "Expected an assignment operator '=' at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
    return 0;
  }
  if( !( token = vtTokenExpect( parser, VT_TOKEN_INTEGER ) ) )
  {
    linecount = bsConfResolveLine( context, parser, &lineoffset );
    ioPrintf( &context->output, 0, BSMSG_ERROR "Expected an integer for assignment at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
    return 0;
  }
  if( !( ccSeqParseInt64( &parser->codestring[ token->offset ], token->length, &readint ) ) )
  {
    linecount = bsConfResolveLine( context, parser, &lineoffset );
    ioPrintf( &context->output, 0, BSMSG_ERROR "Integer parse error at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
    return 0;
  }
  *retint = (int)readint;
  return 1;
}


////


static int bsConfParse( bsContext *context, bsConfParser *parser )
{
  int linecount, lineoffset, readint;
  char *tokenstring;
  char *readstring;
  vtToken *token;

  DEBUG_SET_TRACKER();

  for( ; ; )
  {
    if( vtTokenAccept( parser, VT_TOKEN_END ) )
      break;
    /* Expect scope.variable = value; */
    if( ( token = vtTokenAccept( parser, VT_TOKEN_IDENTIFIER ) ) )
    {
      tokenstring = &parser->codestring[ token->offset ];
      if( ccStrMatchSeq( "autocheck", tokenstring, token->length ) )
      {
        if( !( bsConfReadInteger( context, parser, &readint ) ) )
          goto error;
        context->contextflags &= ~BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
        if( readint )
          context->contextflags |= BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
      }
      else if( ccStrMatchSeq( "bricklink", tokenstring, token->length ) )
      {
        if( !( vtTokenExpect( parser, VT_TOKEN_DOT ) ) )
        {
          bsConfErrorScopeEntryFailure( context, parser );
          goto error;
        }
        if( !( token = vtTokenExpect( parser, VT_TOKEN_IDENTIFIER ) ) )
        {
          bsConfErrorScopeMemberFailure( context, parser );
          goto error;
        }
        tokenstring = &parser->codestring[ token->offset ];
        if( ccStrMatchSeq( "consumerkey", tokenstring, token->length ) )
        {
          if( !( bsConfReadString( context, parser, &context->bricklink.consumerkey ) ) )
            goto error;
        }
        else if( ccStrMatchSeq( "consumersecret", tokenstring, token->length ) )
        {
          if( !( bsConfReadString( context, parser, &context->bricklink.consumersecret ) ) )
            goto error;
        }
        else if( ccStrMatchSeq( "token", tokenstring, token->length ) )
        {
          if( !( bsConfReadString( context, parser, &context->bricklink.token ) ) )
            goto error;
        }
        else if( ccStrMatchSeq( "tokensecret", tokenstring, token->length ) )
        {
          if( !( bsConfReadString( context, parser, &context->bricklink.tokensecret ) ) )
            goto error;
        }
        else if( ccStrMatchSeq( "pollinterval", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->bricklink.pollinterval = (int)readint;
        }
        else if( ccStrMatchSeq( "failinterval", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->bricklink.failinterval = (int)readint;
        }
        else if( ccStrMatchSeq( "pipelinequeue", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->bricklink.pipelinequeuesize = (int)readint;
        }
        else
        {
          bsConfErrorUnknownScopeMember( context, parser, token );
          goto error;
        }
      }
      else if( ccStrMatchSeq( "brickowl", tokenstring, token->length ) )
      {
        if( !( vtTokenExpect( parser, VT_TOKEN_DOT ) ) )
        {
          bsConfErrorScopeEntryFailure( context, parser );
          goto error;
        }
        if( !( token = vtTokenExpect( parser, VT_TOKEN_IDENTIFIER ) ) )
        {
          bsConfErrorScopeMemberFailure( context, parser );
          goto error;
        }
        tokenstring = &parser->codestring[ token->offset ];
        if( ccStrMatchSeq( "key", tokenstring, token->length ) )
        {
          if( !( bsConfReadString( context, parser, &context->brickowl.key ) ) )
            goto error;
        }
        else if( ccStrMatchSeq( "pollinterval", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->brickowl.pollinterval = (int)readint;
        }
        else if( ccStrMatchSeq( "failinterval", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->brickowl.failinterval = (int)readint;
        }
        else if( ccStrMatchSeq( "pipelinequeue", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->brickowl.pipelinequeuesize = (int)readint;
        }
        else if( ccStrMatchSeq( "reuseempty", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->brickowl.reuseemptyflag = (int)readint;
        }
        else
        {
          bsConfErrorUnknownScopeMember( context, parser, token );
          goto error;
        }
      }
      else if( ccStrMatchSeq( "priceguide", tokenstring, token->length ) )
      {
        if( !( vtTokenExpect( parser, VT_TOKEN_DOT ) ) )
        {
          bsConfErrorScopeEntryFailure( context, parser );
          goto error;
        }
        if( !( token = vtTokenExpect( parser, VT_TOKEN_IDENTIFIER ) ) )
        {
          bsConfErrorScopeMemberFailure( context, parser );
          goto error;
        }
        tokenstring = &parser->codestring[ token->offset ];
        if( ccStrMatchSeq( "cachepath", tokenstring, token->length ) )
        {
          if( !( bsConfReadString( context, parser, &context->priceguidepath ) ) )
            goto error;
        }
        else if( ccStrMatchSeq( "cacheformat", tokenstring, token->length ) )
        {
          readstring = 0;
          if( !( bsConfReadString( context, parser, &readstring ) ) )
            goto error;
          if( ccStrLowCmpWord( readstring, "brickstore" ) )
            context->priceguideflags = BSX_PRICEGUIDE_FLAGS_BRICKSTORE;
          else if( ccStrLowCmpWord( readstring, "brickstock" ) )
            context->priceguideflags = BSX_PRICEGUIDE_FLAGS_BRICKSTOCK;
          else
          {
            linecount = bsConfResolveLine( context, parser, &lineoffset );
            ioPrintf( &context->output, 0, BSMSG_ERROR "Unknown priceguide.cacheformat at " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
            free( readstring );
            goto error;
          }
          free( readstring );
        }
        else if( ccStrMatchSeq( "cachetime", tokenstring, token->length ) )
        {
          if( !( bsConfReadInteger( context, parser, &readint ) ) )
            goto error;
          context->priceguidecachetime = 24*60*60 * (int)readint;
        }
        else
        {
          bsConfErrorUnknownScopeMember( context, parser, token );
          goto error;
        }
      }
      else if( ccStrMatchSeq( "retainemptylots", tokenstring, token->length ) )
      {
        if( !( bsConfReadInteger( context, parser, &readint ) ) )
          goto error;
        context->retainemptylotsflag = (int)readint;
      }
      else if( ccStrMatchSeq( "checkmessage", tokenstring, token->length ) )
      {
        if( !( bsConfReadInteger( context, parser, &readint ) ) )
          goto error;
        //context->checkmessageflag = (int)readint;
        context->checkmessageflag = 0;
      }
      else if( ccStrMatchSeq( "registrationkey", tokenstring, token->length ) )
      {
#if BS_ENABLE_REGISTRATION
        if( !( bsConfReadString( context, parser, &context->registrationkey ) ) )
          goto error;
#else
        char *dummy;
        dummy = 0;
        if( !( bsConfReadString( context, parser, &dummy ) ) )
          goto error;
        if( dummy )
          free( dummy );
#endif
      }
      else
      {
        bsConfErrorUnknownIdentifier( context, parser, token );
        goto error;
      }
    }
    else
    {
      linecount = bsConfResolveLine( context, parser, &lineoffset );
      ioPrintf( &context->output, 0, BSMSG_ERROR "Expected identifier token at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
      goto error;
    }

    if( !( vtTokenExpect( parser, VT_TOKEN_SEMICOLON ) ) )
    {
      linecount = bsConfResolveLine( context, parser, &lineoffset );
      ioPrintf( &context->output, 0, BSMSG_ERROR "Expected ';' to terminate statement at line " IO_CYAN "%d" IO_WHITE ", offset " IO_CYAN "%d" IO_WHITE ".\n", linecount, lineoffset );
      goto error;
    }

    /* Next statement! */
    continue;

    error:
    parser->errorcount++;
    if( parser->errorcount >= 16 )
      break;
    for( ; ; )
    {
      if( vtTokenAccept( parser, VT_TOKEN_END ) )
        return 0;
      if( vtTokenAccept( parser, VT_TOKEN_SEMICOLON ) )
        break;
      vtTokenIncrement( parser );
    }
  }

  return ( parser->errorcount ? 0 : 1 );
}


int bsConfLoad( bsContext *context, char *path )
{
  int retval;
  size_t conflength;
  char *confstring;
  vtTokenBuffer *tokenbuf;
  bsConfParser parser;

  DEBUG_SET_TRACKER();

  confstring = ccFileLoad( path, 1048576, &conflength );
  if( !( confstring ) )
    return 0;

  tokenbuf = vtLexParse( confstring );
  if( !( tokenbuf ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Lexical error while parsing BrickSync configuration at \"" IO_RED "%s" IO_WHITE "\".\n", path );
    free( confstring );
    return 0;
  }

  vtTokenInit( &parser, confstring, tokenbuf );

  retval = bsConfParse( context, &parser );

  vtLexFree( tokenbuf );

  return retval;
}



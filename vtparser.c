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

#include "vtscript.h"
#include "vtlex.h"
#include "vtparser.h"


extern mmHashAccess vtNameHashAccess;


#define VT_PARSER_DEBUG_OP_EMIT (1)


////


static void vtTokenInit( vtParser *parser, char *codestring, vtTokenBuffer *tokenbuf )
{
  vtOpBuffer *opbuf;
  parser->token = &tokenbuf->tokenlist[0];
  parser->nexttoken = &tokenbuf->tokenlist[1];
  parser->tokentype = (parser->token)->type;
  parser->codestring = codestring;
  parser->tokenbufindex = 2;
  parser->tokenbuf = tokenbuf;
  return;
}

static void vtTokenIncrement( vtParser *parser )
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

static inline vtToken *vtTokenAccept( vtParser *parser, int tokentype )
{
  vtToken *token;
  /* Do we accept requested token? */
  if( tokentype != parser->tokentype )
    return 0;
  token = parser->token;
  vtTokenIncrement( parser );
  return token;
}

static inline vtToken *vtTokenExpect( vtParser *parser, int tokentype )
{
  vtToken *token;
  /* Do we have expected token? */
  if( tokentype == parser->tokentype )
  {
    token = parser->token;
    vtTokenIncrement( parser );
    return token;
  }
  printf( "VT PARSER: Error, token %d when token %d was expected\n", (parser->token)->type, tokentype );
  parser->errorcount++;
  return 0;
}


////



static vtOp *vtParserAllocOp( vtParser *parser )
{
  vtOp *op;
  vtOpBuffer *opbuf, *opbufnext;

  opbuf = parser->opbuflast;
  if( opbuf->opcount >= VT_OP_BUFFER_CLAMP )
  {
    opbufnext = malloc( sizeof(vtOpBuffer) );
    opbufnext->opcount = 0;
    opbufnext->next = 0;
    opbuf->next = opbufnext;
    opbuf = opbufnext;
  }
  op = &opbuf->oplist[ opbuf->opcount ];
  opbuf->opcount++;

  return op;
}

static vtMemOffset vtParserReserveMemory( vtParser *parser, int memsize, int alignmask )
{
  vtMemOffset offset;
  offset = ( parser->memsize + alignmask ) & ~alignmask;
  parser->memsize += memsize;
  return offset;
}

static vtMemOffset vtParserReserveStatic( vtParser *parser, int memsize, int alignmask )
{
  vtMemOffset offset;
  offset = ( parser->staticsize + alignmask ) & ~alignmask;
  parser->staticsize += memsize;
  parser->staticmemory = realloc( parser->staticmemory, parser->staticsize );
  return offset;
}

static vtMemOffset vtParserReserveBaseType( vtParser *parser, int basetype )
{
  return vtParserReserveMemory( parser, vtBaseTypeMemSize[ basetype ], vtBaseTypeMemAlign[ basetype ] );
}


////


static inline vtMemOffset vtOpImm32( vtParser *parser, int32_t imm32 )
{
  vtOp *op;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_IMM, VT_BASETYPE_INT32 );
  op->memoffset[0] = vtParserReserveBaseType( parser, VT_BASETYPE_INT32 );
  op->u.imm.storage.i32 = imm32;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: Imm32 at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpImm64( vtParser *parser, int64_t imm64 )
{
  vtOp *op;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_IMM, VT_BASETYPE_INT64 );
  op->memoffset[0] = vtParserReserveBaseType( parser, VT_BASETYPE_INT64 );
  op->u.imm.storage.i64 = imm64;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: Imm64 at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpImmPointer( vtParser *parser, void *p )
{
  vtOp *op;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_IMM, VT_BASETYPE_POINTER );
  op->memoffset[0] = vtParserReserveBaseType( parser, VT_BASETYPE_POINTER );
  op->u.imm.storage.p = p;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: ImmPointer at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpFloat( vtParser *parser, float f )
{
  vtOp *op;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_IMM, VT_BASETYPE_FLOAT );
  op->memoffset[0] = vtParserReserveBaseType( parser, VT_BASETYPE_FLOAT );
  op->u.imm.storage.f = f;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: ImmFloat at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpDouble( vtParser *parser, double d )
{
  vtOp *op;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_IMM, VT_BASETYPE_DOUBLE );
  op->memoffset[0] = vtParserReserveBaseType( parser, VT_BASETYPE_DOUBLE );
  op->u.imm.storage.d = d;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: ImmDouble at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

/* This is a no-op if no cast is necessary */
static inline vtMemOffset vtOpTypeCast( vtParser *parser, vtGenericType *dstgtype, vtMemOffset srcmemoffset, vtGenericType *srcgtype )
{
  vtOp *op;
  if( ( srcgtype->basetype == VT_BASETYPE_POINTER ) || ( dstgtype->basetype == VT_BASETYPE_POINTER ) )
  {
    if( ( srcgtype->basetype == dstgtype->basetype ) && ( srcgtype->matchtype == dstgtype->matchtype ) )
      return srcmemoffset;
    printf( "VT PARSER: Can not cast between different pointer data types\n" );
    parser->errorcount++;
    return 0;
  }
  if( srcgtype->basetype == dstgtype->basetype )
    return srcmemoffset;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_CAST_CHAR + dstgtype->basetype, srcgtype->basetype );
  op->memoffset[0] = vtParserReserveBaseType( parser, dstgtype->basetype );
  op->memoffset[1] = srcmemoffset;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: ImmCast type %d -> %d, to [%d] from [%d]\n", srcgtype->basetype, dstgtype->basetype, op->memoffset[0], op->memoffset[1] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpSysRead( vtParser *parser, vtSysVar *sysvar, vtGenericType *retgtype )
{
  int basetype;
  vtOp *op;
  vtSysType *systype;
  op = vtParserAllocOp( parser );
  systype = sysvar->type;
  basetype = systype->u.typescalar.basetype;
  op->code = VT_OPCODE( VT_OP_SYSREAD, basetype );
  op->memoffset[0] = vtParserReserveBaseType( parser, basetype );
  op->u.sys.var = sysvar;
  op->u.sys.typemember = systype;
  retgtype->basetype = basetype;
  retgtype->matchtype = systype->u.typescalar.matchtype;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: SysRead at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpSysReadMember( vtParser *parser, vtSysVar *sysvar, vtSysMember *sysmember, vtGenericType *retgtype )
{
  int basetype;
  vtOp *op;
  op = vtParserAllocOp( parser );
  basetype = sysmember->basetype;
  op->code = VT_OPCODE( VT_OP_SYSREADMEMBER, basetype );
  op->memoffset[0] = vtParserReserveBaseType( parser, basetype );
  op->u.sys.var = sysvar;
  op->u.sys.typemember = sysmember;
  retgtype->basetype = basetype;
  retgtype->matchtype = sysmember->matchtype;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: SysReadMember at [%d]\n", op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline void vtOpSysWrite( vtParser *parser, vtSysVar *sysvar, vtMemOffset srcmemoffset, vtGenericType *srcgtype )
{
  vtGenericType dstgtype;
  vtOp *op;
  vtSysType *systype;
  systype = sysvar->type;
  dstgtype.basetype = systype->u.typescalar.basetype;
  dstgtype.matchtype = systype->u.typescalar.matchtype;
  srcmemoffset = vtOpTypeCast( parser, &dstgtype, srcmemoffset, srcgtype );
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_SYSWRITE, dstgtype.basetype );
  op->memoffset[0] = srcmemoffset;
  op->u.sys.var = sysvar;
  op->u.sys.typemember = systype;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: SysWrite from [%d]\n", op->memoffset[0] );
#endif
  return;
}

static inline void vtOpSysWriteMember( vtParser *parser, vtSysVar *sysvar, vtSysMember *sysmember, vtMemOffset srcmemoffset, vtGenericType *srcgtype )
{
  vtGenericType dstgtype;
  vtOp *op;
  dstgtype.basetype = sysmember->basetype;
  dstgtype.matchtype = sysmember->matchtype;
  srcmemoffset = vtOpTypeCast( parser, &dstgtype, srcmemoffset, srcgtype );
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( VT_OP_SYSWRITEMEMBER, dstgtype.basetype );
  op->memoffset[0] = srcmemoffset;
  op->u.sys.var = sysvar;
  op->u.sys.typemember = sysmember;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: SysWriteMember from [%d]\n", op->memoffset[0] );
#endif
  return;
}

static inline void vtOpSysGeneric( vtParser *parser, int optype, vtSysVar *sysvar )
{
  vtGenericType dstgtype;
  vtOp *op;
  vtSysType *systype;
  systype = sysvar->type;
  dstgtype.basetype = systype->u.typescalar.basetype;
  dstgtype.matchtype = systype;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( optype, dstgtype.basetype );
  op->u.sys.var = sysvar;
  op->u.sys.typemember = systype;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: SysGeneric %d\n", optype, op->memoffset[0] );
#endif
  return;
}

static inline void vtOpSysCall( vtParser *parser, int optype, vtSysVar *sysvar, vtSysMethod *sysmethod )
{
  vtGenericType dstgtype;
  vtOp *op;
  vtSysType *systype;
  systype = sysvar->type;
  dstgtype.basetype = systype->u.typescalar.basetype;
  dstgtype.matchtype = systype;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( optype, VT_BASETYPE_CHAR );
  op->u.sys.var = sysvar;
  op->u.sys.typemember = sysmethod;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: SysCall %d\n", optype, op->memoffset[0] );
#endif
  return;
}

static inline vtMemOffset vtOpGeneric1( vtParser *parser, int optype, vtMemOffset dstmemoffset, vtGenericType *dstgtype )
{
  vtOp *op;
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( optype, dstgtype->basetype );
  op->memoffset[0] = dstmemoffset;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: Generic1 %d to [%d]\n", optype, op->memoffset[0] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpGenericCast2( vtParser *parser, int optype, vtMemOffset dstmemoffset, vtGenericType *dstgtype, vtMemOffset srcmemoffset, vtGenericType *srcgtype )
{
  vtOp *op;
  srcmemoffset = vtOpTypeCast( parser, dstgtype, srcmemoffset, srcgtype );
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( optype, dstgtype->basetype );
  op->memoffset[0] = dstmemoffset;
  op->memoffset[1] = srcmemoffset;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: Generic2 %d to [%d] from [%d]\n", optype, op->memoffset[0], op->memoffset[1] );
#endif
  return op->memoffset[0];
}

static inline vtMemOffset vtOpSetCmp( vtParser *parser, int optype, vtMemOffset dstmemoffset, vtGenericType *dstgtype, vtMemOffset srcmemoffset, vtGenericType *srcgtype )
{
  vtOp *op;
  srcmemoffset = vtOpTypeCast( parser, dstgtype, srcmemoffset, srcgtype );
  op = vtParserAllocOp( parser );
  op->code = VT_OPCODE( optype, dstgtype->basetype );
  op->memoffset[0] = vtParserReserveBaseType( parser, VT_CMP_BASETYPE );
  op->memoffset[1] = dstmemoffset;
  op->memoffset[2] = srcmemoffset;
#if VT_PARSER_DEBUG_OP_EMIT
  printf( "OpEmit: Cmp %d to [%d], [%d] and [%d]\n", optype, op->memoffset[0], op->memoffset[1], op->memoffset[2] );
#endif
  return op->memoffset[0];
}


////


/*

Scope theory

Scope definition
- Temporary anonymous hierarchical list during compilation of a single chunk of code

Namespace definition
- Permanent named namespace organized hierarchically, 

Both share the same structure called "scope" ; one is anonymous, the other named

*/


////


static vtMemOffset vtParseExpression( vtParser *parser, vtGenericType *retgtype );


static vtMemOffset vtParseImmediateInteger( vtParser *parser, vtToken *token, vtGenericType *retgtype )
{
  int dstbasetype;
  vtMemOffset memoffset;
  int64_t readint;

  readint = 0;
  if( !( ccSeqParseInt64( &parser->codestring[ token->offset ], token->length, &readint ) ) )
    printf( "VT PARSER: Error, immediate integer error ( %s:%d )\n", __FILE__, __LINE__ );

  dstbasetype = retgtype->basetype;
  if( dstbasetype == VT_BASETYPE_FLOAT )
  {
    memoffset = vtOpFloat( parser, (float)readint );
    dstbasetype = VT_BASETYPE_FLOAT;
  }
  else if( dstbasetype == VT_BASETYPE_DOUBLE )
  {
    memoffset = vtOpDouble( parser, (double)readint );
    dstbasetype = VT_BASETYPE_DOUBLE;
  }
  else if( ( dstbasetype == VT_BASETYPE_INT64 ) || ( dstbasetype == VT_BASETYPE_UINT64 ) || ( readint >= ((((int64_t)1)<<31)-1) ) || ( readint < -(((int64_t)1)<<31) ) )
  {
    memoffset = vtOpImm64( parser, (int64_t)readint );
    dstbasetype = VT_BASETYPE_INT64;
  }
  else
  {
    memoffset = vtOpImm32( parser, (int32_t)readint );
    dstbasetype = VT_BASETYPE_INT32;
  }

  retgtype->basetype = dstbasetype;
  return memoffset;
}


static vtMemOffset vtParseImmediateFloat( vtParser *parser, vtToken *token, vtGenericType *retgtype )
{
  int dstbasetype;
  vtMemOffset memoffset;
  double readdouble;

  readdouble = 0.0;
  if( !( ccSeqParseDouble( &parser->codestring[ token->offset ], token->length, &readdouble ) ) )
    printf( "VT PARSER: Error, immediate float error ( %s:%d )\n", __FILE__, __LINE__ );

  dstbasetype = retgtype->basetype;
  if( dstbasetype == VT_BASETYPE_FLOAT )
  {
    memoffset = vtOpFloat( parser, (float)readdouble );
    dstbasetype = VT_BASETYPE_FLOAT;
  }
  else
  {
    memoffset = vtOpDouble( parser, (double)readdouble );
    dstbasetype = VT_BASETYPE_DOUBLE;
  }

  retgtype->basetype = dstbasetype;
  return memoffset;
}


static vtMemOffset vtParseImmediateString( vtParser *parser, vtToken *token, vtGenericType *retgtype )
{
  vtMemOffset staticoffset, memoffset;
  char *staticmem;
  staticoffset = vtParserReserveStatic( parser, token->length + 1, 1 );
  staticmem = ADDRESS( parser->staticmemory, staticoffset );
  memcpy( staticmem, &parser->codestring[ token->offset ], token->length );
  staticmem[ token->length ] = 0;
  memoffset = vtOpImmPointer( parser, (void *)(uintptr_t)staticoffset );
  *retgtype = vtGenericTypeString;
  return memoffset;
}


static vtMemOffset vtParseImmediateChar( vtParser *parser, vtToken *token, vtGenericType *retgtype )
{
  int dstbasetype, readint;
  vtMemOffset memoffset;

  readint = (int)parser->codestring[ token->offset ];

  dstbasetype = retgtype->basetype;
  if( dstbasetype == VT_BASETYPE_FLOAT )
  {
    memoffset = vtOpFloat( parser, (float)readint );
    dstbasetype = VT_BASETYPE_FLOAT;
  }
  else if( dstbasetype == VT_BASETYPE_DOUBLE )
  {
    memoffset = vtOpDouble( parser, (double)readint );
    dstbasetype = VT_BASETYPE_DOUBLE;
  }
  else if( dstbasetype == VT_BASETYPE_INT64 )
  {
    memoffset = vtOpImm64( parser, (int64_t)readint );
    dstbasetype = VT_BASETYPE_INT64;
  }
  else
  {
    memoffset = vtOpImm32( parser, (int32_t)readint );
    dstbasetype = VT_BASETYPE_INT32;
  }

  retgtype->basetype = dstbasetype;
  return memoffset;
}


static int vtResolveIdentifier( vtSystem *system, vtScope *scope, char *tokenname, int tokenlen, int scoperecursionflag, vtName *retname )
{
  retname->name = tokenname;
  retname->namelen = tokenlen;
  /* Check for collision with high priority keywords (data types, etc.) */
  if( mmHashDirectReadEntry( system->sysnamespace.hashtable, &vtNameHashAccess, retname ) )
    return 1;
  /* Search the scope hierarchy */
  for( ; scope ; scope = scope->parentscope )
  {
    if( mmHashDirectReadEntry( scope->namespace.hashtable, &vtNameHashAccess, retname ) )
      return 1;
    if( !( scoperecursionflag ) )
      return 0;
  }
  /* Failed to resolve in any scope */
  return 0;
}

static int vtResolveStructSpace( vtSystem *system, vtSysType *systype, char *tokenname, int tokenlen, vtName *retname )
{
  retname->name = tokenname;
  retname->namelen = tokenlen;
  if( mmHashDirectReadEntry( systype->u.typestruct.namespace.hashtable, &vtNameHashAccess, retname ) )
    return 1;
  /* Failed to resolve in any scope */
  return 0;
}

/* Fully resolve scope hierarchy for identifier */
/* Return parent scope, final token and name if found (namelen==0 otherwise) */
static vtToken *vtResolveScope( vtParser *parser, vtScope **scopeptr, vtName *retname )
{
  char *codestring, *tokenstring;
  vtSystem *system;
  vtScope *scope;
  vtToken *token;

  scope = *scopeptr;
  system = parser->system;
  codestring = parser->codestring;
  for( ; ; )
  {
    if( !( vtTokenExpect( parser, VT_TOKEN_DOT ) ) )
    {
      printf( "VT PARSER: Expected '.' to access scope\n" );
      parser->errorcount++;
      return 0;
    }
    if( !( token = vtTokenAccept( parser, VT_TOKEN_IDENTIFIER ) ) )
    {
      printf( "VT PARSER: Expected identifier, encountered \"%.*s\"\n", (int)token->length, &codestring[ token->offset ] );
      parser->errorcount++;
      return 0;
    }
    tokenstring = &codestring[ token->offset ];
    if( !( vtResolveIdentifier( system, scope, tokenstring, token->length, 0, retname ) ) )
    {
      retname->namelen = 0;
      return token;
    }
    if( retname->class == VT_NAME_CLASS_SCOPE )
      scope = (vtScope *)retname->object;
    else
      break;
  }

  *scopeptr = scope;
  return token;
}

#define VT_NAME_CLASS_ERROR (VT_NAME_CLASS_COUNT)

/* Token is an identifier ; return its class, pointer to abstract object, and optional struct member */
static int vtParseIdentifier( vtParser *parser, vtToken *token, void **retobject, void **retmember )
{
  int nameclass;
  vtName name;
  char *codestring, *tokenstring;
  vtSystem *system;
  vtScope *scope;
  vtSysType *systype;
  vtSysVar *sysvar;
  vtSysMember *sysmember;

  if( parser->errorcount )
    return VT_NAME_CLASS_ERROR;

  system = parser->system;
  scope = &system->globalscope;
  nameclass = VT_NAME_CLASS_ERROR;
  codestring = parser->codestring;

  tokenstring = &codestring[ token->offset ];
  if( !( vtResolveIdentifier( system, parser->currentscope, tokenstring, token->length, 1, &name ) ) )
  {
    printf( "VT PARSER: Unknown identifier \"%.*s\"\n", (int)token->length, tokenstring );
    parser->errorcount++;
    return VT_NAME_CLASS_ERROR;
  }

#if 0
printf( "Identifier \"%.*s\" : class %d, namelen %d, object %p\n", (int)token->length, tokenstring, name.class, name.namelen, name.object );
#endif

  /* If entering scope hierarchy, resolve final identifier */
  if( name.class == VT_NAME_CLASS_SCOPE )
  {
    scope = (vtScope *)name.object;
    if( !( vtResolveScope( parser, &scope, &name ) ) )
      return VT_NAME_CLASS_ERROR;
    if( !( name.namelen ) )
    {
      printf( "VT PARSER: Unknown identifier \"%.*s\" in scope\n", (int)token->length, tokenstring );
      parser->errorcount++;
      return VT_NAME_CLASS_ERROR;
    }
  }

  switch( name.class )
  {
    case VT_NAME_CLASS_SYSTYPE:
      /* Declaration, or cast? */
      nameclass = VT_NAME_CLASS_SYSTYPE;
      systype = (vtSysType *)name.object;
      *retobject = (void *)systype;
      break;
    case VT_NAME_CLASS_SYSVAR:
      /* Accessing a sysvar */
      sysvar = (vtSysVar *)name.object;
      systype = sysvar->type;
#if 0
      /* If variable is array */
      if( vtTokenAccept( parser, VT_TOKEN_LBRACKET ) )
      {
        /* Array indexing, parse expression */
        memoffset = vtParseExpression( parser, &basetype );
        vtTokenExpect( parser, VT_TOKEN_RBRACKET );
      }
#endif
      *retobject = (void *)sysvar;
      if( !( systype->flags & VT_SYSTYPE_FLAGS_STRUCT ) )
        nameclass = VT_NAME_CLASS_SYSVAR;
      else
      {
        if( !( vtTokenExpect( parser, VT_TOKEN_DOT ) ) )
        {
          printf( "VT PARSER: Expected '.' to access struct\n" );
          parser->errorcount++;
          return VT_NAME_CLASS_ERROR;
        }
        if( !( token = vtTokenAccept( parser, VT_TOKEN_IDENTIFIER ) ) )
        {
          printf( "VT PARSER: Expected identifier, encountered \"%.*s\"\n", (int)token->length, &codestring[ token->offset ] );
          parser->errorcount++;
          return VT_NAME_CLASS_ERROR;
        }
        if( !( vtResolveStructSpace( system, systype, &codestring[ token->offset ], token->length, &name ) ) )
        {
          printf( "VT PARSER: Struct has no member or method named \"%.*s\"\n", (int)token->length, &codestring[ token->offset ] );
          parser->errorcount++;
          return VT_NAME_CLASS_ERROR;
        }
        nameclass = name.class;
        *retmember = (void *)name.object;
      }
      break;
    case VT_NAME_CLASS_TYPE:
      /* Declaration, or cast? */
      printf( "VT PARSER: Not supported %s:%d\n", __FILE__, __LINE__ );
      parser->errorcount++;
      break;
    case VT_NAME_CLASS_VARIABLE:
      /* Accessing a variable */
      printf( "VT PARSER: Not supported %s:%d\n", __FILE__, __LINE__ );
      parser->errorcount++;
      break;
    case VT_NAME_CLASS_FUNCTION:
      /* Function call? */
      printf( "VT PARSER: Not supported %s:%d\n", __FILE__, __LINE__ );
      parser->errorcount++;
      break;
    case VT_NAME_CLASS_LABEL:
      /* Goto target? */
      printf( "VT PARSER: Not supported %s:%d\n", __FILE__, __LINE__ );
      parser->errorcount++;
      break;
    case VT_NAME_CLASS_SCOPE:
    case VT_NAME_CLASS_SYSMEMBER:
    case VT_NAME_CLASS_MEMBER:
    default:
      /* Should never happen */
      printf( "VT PARSER: Internal error at %s:%d\n", __FILE__, __LINE__ );
      parser->errorcount++;
      break;
  }

  return nameclass;
}


static int vtParseCallFormat( vtParser *parser, vtCallFormat *format, int retusagemax, vtMemOffset *retoffset )
{
  int argindex, retindex;
  vtGenericType *dstgtype;
  vtGenericType srcgtype;
  vtMemOffset memoffset;
  if( !( vtTokenExpect( parser, VT_TOKEN_LPAREN ) ) )
    return 0;
  for( argindex = 0 ; ; argindex++ )
  {
    if( argindex == format->argcount )
    {
      vtTokenExpect( parser, VT_TOKEN_RPAREN );
      break;
    }
    if( argindex )
      vtTokenExpect( parser, VT_TOKEN_COMMA );
    if( parser->errorcount )
      break;
    dstgtype = &format->argtype[ argindex ];
    srcgtype = *dstgtype;
    memoffset = vtParseExpression( parser, &srcgtype );
    memoffset = vtOpTypeCast( parser, dstgtype, memoffset, &srcgtype );
    vtOpGeneric1( parser, VT_OP_PUSH_ARG, memoffset, dstgtype );
  }
  for( retindex = 0 ; retindex < format->retcount ; retindex++ )
  {
    memoffset = vtParserReserveBaseType( parser, format->rettype[retindex].basetype );
    vtOpGeneric1( parser, VT_OP_PUSH_RET, memoffset, dstgtype );
    /* Need to store memoffset[] for return values, we read from them after call */
    if( retindex < retusagemax )
      retoffset[ retindex ] = memoffset;
  }
  return 1;
}


static vtMemOffset vtParseElement( vtParser *parser, vtGenericType *retgtype )
{
  int nameclass;
  vtMemOffset memoffset;
  vtToken *token;
  vtSysVar *sysvar;
  vtSysMember *sysmember;
  vtSysMethod *sysmethod;
  void *idobject, *idmember;
  vtMemOffset retoffset[VT_RET_COUNT_MAX];

  if( parser->errorcount )
    return 0;

  if( ( token = vtTokenAccept( parser, VT_TOKEN_IDENTIFIER ) ) )
  {
    nameclass = vtParseIdentifier( parser, token, &idobject, &idmember );
    switch( nameclass )
    {
      case VT_NAME_CLASS_SYSVAR:
        sysvar = (vtSysVar *)idobject;
        memoffset = vtOpSysRead( parser, sysvar, retgtype );
        break;
      case VT_NAME_CLASS_SYSMEMBER:
        sysvar = (vtSysVar *)idobject;
        sysmember = (vtSysMember *)idmember;
        memoffset = vtOpSysReadMember( parser, sysvar, sysmember, retgtype );
        break;
      case VT_NAME_CLASS_SYSMETHOD:
        sysvar = (vtSysVar *)idobject;
        sysmethod = (vtSysMethod *)idmember;
        if( !( sysmethod->format.retcount ) )
        {
          printf( "VT PARSER: Method has no return value to use in expression\n", (int)token->length, &parser->codestring[ token->offset ] );
          parser->errorcount++;
          return 0;
        }
        if( !( vtParseCallFormat( parser, &sysmethod->format, VT_RET_COUNT_MAX, retoffset ) ) )
          return 0;
        vtTokenExpect( parser, VT_TOKEN_SEMICOLON );
        vtOpSysCall( parser, VT_OP_CALL_SYSMETHOD, sysvar, sysmethod );
        /* TODO: How do we process all the return values? */
        *retgtype = sysmethod->format.rettype[0];
        return retoffset[0];
      default:
        printf( "VT PARSER: Failed to parse identifier \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
        parser->errorcount++;
        return 0;
    }
  }
  else if( ( token = vtTokenAccept( parser, VT_TOKEN_INTEGER ) ) )
    memoffset = vtParseImmediateInteger( parser, token, retgtype );
  else if( ( token = vtTokenAccept( parser, VT_TOKEN_FLOAT ) ) )
    memoffset = vtParseImmediateFloat( parser, token, retgtype );
  else if( vtTokenAccept( parser, VT_TOKEN_LPAREN ) )
  {
    memoffset = vtParseExpression( parser, retgtype );
    vtTokenExpect( parser, VT_TOKEN_RPAREN );
  }
  else if( ( token = vtTokenAccept( parser, VT_TOKEN_STRING ) ) )
    memoffset = vtParseImmediateString( parser, token, retgtype );
  else if( ( token = vtTokenAccept( parser, VT_TOKEN_CHAR ) ) )
    memoffset = vtParseImmediateChar( parser, token, retgtype );
  else
  {
    printf( "VT PARSER: Error, syntax error ( %s:%d )\n", __FILE__, __LINE__ );
    parser->errorcount++;
  }
  return memoffset;
}


static vtMemOffset vtParseElemMods( vtParser *parser, vtGenericType *retgtype )
{
  int modop;
  vtMemOffset memoffset;
  modop = -2;
  if( vtTokenAccept( parser, VT_TOKEN_ADD ) )
    modop = -1;
  else if( vtTokenAccept( parser, VT_TOKEN_SUB ) )
    modop = VT_OP_NEGATE;
  else if( vtTokenAccept( parser, VT_TOKEN_NOT ) )
    modop = VT_OP_NOT;
  else if( vtTokenAccept( parser, VT_TOKEN_LOGNOT ) )
    modop = VT_OP_LOGNOT;
  if( modop > -2 )
  {
    memoffset = vtParseElemMods( parser, retgtype );
    if( modop >= 0 )
      vtOpGenericCast2( parser, modop, memoffset, retgtype, memoffset, retgtype );
  }
  else
    memoffset = vtParseElement( parser, retgtype );
  return memoffset;
}


/* Term operators: x*y/z */
static vtMemOffset vtParseTerm( vtParser *parser, vtGenericType *retgtype )
{
  vtGenericType dstgtype, srcgtype;
  vtMemOffset dstmemoffset, srcmemoffset;
  int notflag, optype;

  if( parser->errorcount )
    return 0;

  dstgtype = *retgtype;
  dstmemoffset = vtParseElemMods( parser, &dstgtype );

  for( ; ; )
  {
    /* factor * factor / factor ... */
    if( vtTokenAccept( parser, VT_TOKEN_MUL ) )
      optype = VT_OP_MUL;
    else if( vtTokenAccept( parser, VT_TOKEN_DIV ) )
      optype = VT_OP_DIV;
    else if( vtTokenAccept( parser, VT_TOKEN_MOD ) )
      optype = VT_OP_MOD;
    else
      break;
    srcgtype = dstgtype;
    srcmemoffset = vtParseElemMods( parser, &srcgtype );
    /* TODO: Handle type promotion? */
    vtOpGenericCast2( parser, optype, dstmemoffset, &dstgtype, srcmemoffset, &srcgtype );
  }

  *retgtype = dstgtype;
  return dstmemoffset;
}


/* Arithmetic operators: (-)term + term - term */
static vtMemOffset vtParseArithmetic( vtParser *parser, vtGenericType *retgtype )
{
  vtGenericType dstgtype, srcgtype;
  vtMemOffset dstmemoffset, srcmemoffset;
  int optype;

  if( parser->errorcount )
    return 0;

  dstgtype = *retgtype;
  dstmemoffset = vtParseTerm( parser, &dstgtype );

  /* Accumulate any enumeration of terms on dstmemoffset */
  for( ; ; )
  {
    /* term + term - term ... */
    if( vtTokenAccept( parser, VT_TOKEN_ADD ) )
      optype = VT_OP_ADD;
    else if( vtTokenAccept( parser, VT_TOKEN_SUB ) )
      optype = VT_OP_SUB;
    else
      break;
    srcgtype = dstgtype;
    srcmemoffset = vtParseTerm( parser, &srcgtype );
    /* TODO: Handle type promotion? */
    vtOpGenericCast2( parser, optype, dstmemoffset, &dstgtype, srcmemoffset, &srcgtype );
  }

  *retgtype = dstgtype;
  return dstmemoffset;
}


/* Bitwise operators: arithm << arithm >> arithm */
static vtMemOffset vtParseShift( vtParser *parser, vtGenericType *retgtype )
{
  vtGenericType dstgtype, srcgtype;
  vtMemOffset dstmemoffset, srcmemoffset;
  int optype;

  if( parser->errorcount )
    return 0;

  dstgtype = *retgtype;
  dstmemoffset = vtParseArithmetic( parser, &dstgtype );

  /* Accumulate any enumeration of terms on dstmemoffset */
  for( ; ; )
  {
    /* arithm >> arithm << arithm ... */
    if( vtTokenAccept( parser, VT_TOKEN_SHL ) )
      optype = VT_OP_SHL;
    else if( vtTokenAccept( parser, VT_TOKEN_SHR ) )
      optype = VT_OP_SHR;
    else if( vtTokenAccept( parser, VT_TOKEN_ROL ) )
      optype = VT_OP_ROL;
    else if( vtTokenAccept( parser, VT_TOKEN_ROR ) )
      optype = VT_OP_ROR;
    else
      break;
    srcgtype = dstgtype;
    srcmemoffset = vtParseArithmetic( parser, &srcgtype );
    /* TODO: Handle type promotion? */
    vtOpGenericCast2( parser, optype, dstmemoffset, &dstgtype, srcmemoffset, &srcgtype );
  }

  *retgtype = dstgtype;
  return dstmemoffset;
}


/* Bitwise operators: (-)arithm ^ arithm & (-)arithm */
static vtMemOffset vtParseBitwise( vtParser *parser, vtGenericType *retgtype )
{
  vtGenericType dstgtype, srcgtype;
  vtMemOffset dstmemoffset, srcmemoffset;
  int optype;

  if( parser->errorcount )
    return 0;

  dstgtype = *retgtype;
  dstmemoffset = vtParseShift( parser, &dstgtype );

  /* Accumulate any enumeration of terms on dstmemoffset */
  for( ; ; )
  {
    /* arithm ^ arithm | arithm ... */
    if( vtTokenAccept( parser, VT_TOKEN_OR ) )
      optype = VT_OP_OR;
    else if( vtTokenAccept( parser, VT_TOKEN_XOR ) )
      optype = VT_OP_XOR;
    else if( vtTokenAccept( parser, VT_TOKEN_AND ) )
      optype = VT_OP_AND;
    else
      break;
    srcgtype = dstgtype;
    srcmemoffset = vtParseShift( parser, &srcgtype );
    /* TODO: Handle type promotion? */
    vtOpGenericCast2( parser, optype, dstmemoffset, &dstgtype, srcmemoffset, &srcgtype );
  }

  *retgtype = dstgtype;
  return dstmemoffset;
}


/* Comparison operators: bitw < bitw != bitw */
static vtMemOffset vtParseSetCmp( vtParser *parser, vtGenericType *retgtype )
{
  vtGenericType dstgtype, srcgtype;
  vtMemOffset dstmemoffset, srcmemoffset;
  int optype;

  if( parser->errorcount )
    return 0;

  dstgtype = *retgtype;
  dstmemoffset = vtParseBitwise( parser, &dstgtype );

  /* Accumulate any enumeration of terms on dstmemoffset */
  for( ; ; )
  {
    /* bitw >= bitw != bitw < bitw ... */
    if( vtTokenAccept( parser, VT_TOKEN_CMPEQ ) )
      optype = VT_OP_SETEQ;
    else if( vtTokenAccept( parser, VT_TOKEN_CMPLT ) )
      optype = VT_OP_SETLT;
    else if( vtTokenAccept( parser, VT_TOKEN_CMPLE ) )
      optype = VT_OP_SETLE;
    else if( vtTokenAccept( parser, VT_TOKEN_CMPGT ) )
      optype = VT_OP_SETGT;
    else if( vtTokenAccept( parser, VT_TOKEN_CMPGE ) )
      optype = VT_OP_SETGE;
    else if( vtTokenAccept( parser, VT_TOKEN_CMPNE ) )
      optype = VT_OP_SETNE;
    else
      break;
    srcgtype = dstgtype;
    srcmemoffset = vtParseBitwise( parser, &srcgtype );
    /* TODO: Handle type promotion? */
    dstmemoffset = vtOpSetCmp( parser, optype, dstmemoffset, &dstgtype, srcmemoffset, &srcgtype );
    dstgtype.basetype = VT_CMP_BASETYPE;
    dstgtype.matchtype = 0;
  }

  *retgtype = dstgtype;
  return dstmemoffset;
}


/* TODO: Extra step to handle logical && and || as conditional jumps */


/* Expression: Top-level value expression */
static vtMemOffset vtParseExpression( vtParser *parser, vtGenericType *retgtype )
{
  return vtParseSetCmp( parser, retgtype );
}


/* TODO: Like an expression, but handle comparisons as JMP rather than SET */
static vtMemOffset vtParseCondition( vtParser *parser, vtGenericType *retgtype )
{
  return 0;
}


////


static vtSysVar *vtParseSysDeclare( vtParser *parser, vtSysType *systype )
{
  vtName name;
  char *codestring, *tokenstring;
  vtToken *token;
  vtSystem *system;
  vtScope *scope;
  vtSysVar *sysvar;
  vtDeclSysGlobal *decl;

  if( !( token = vtTokenExpect( parser, VT_TOKEN_IDENTIFIER ) ) )
    return 0;
  system = parser->system;
  scope = &system->globalscope;
  codestring = parser->codestring;
  tokenstring = &codestring[ token->offset ];
  if( vtResolveIdentifier( system, parser->currentscope, tokenstring, token->length, 1, &name ) )
  {
    /* If entering scope hierarchy, resolve final identifier */
    if( name.class == VT_NAME_CLASS_SCOPE )
    {
      scope = (vtScope *)name.object;
      if( !( token = vtResolveScope( parser, &scope, &name ) ) )
        return 0;
      if( !( name.namelen ) )
        goto newvar;
    }
    switch( name.class )
    {
      case VT_NAME_CLASS_SYSVAR:
        sysvar = (vtSysVar *)name.object;
        if( sysvar->type == systype )
        {
          /* Variable exists, enable it */
          vtOpSysGeneric( parser, VT_OP_SYSENABLE, sysvar );
          return sysvar;
        }
        else
        {
          printf( "VT PARSER: Variable \"%.*s\" in scope is already declared with a different type\n", (int)token->length, &codestring[ token->offset ] );
          parser->errorcount++;
        }
        break;
      case VT_NAME_CLASS_SYSTYPE:
      default:
        printf( "VT PARSER: Name collision for \"%.*s\" in variable declaration\n", (int)token->length, &codestring[ token->offset ] );
        parser->errorcount++;
        break;
    }
    return 0;
  }

  newvar:
  if( !( sysvar = vtCreateSysVar( system, scope, systype, &codestring[ token->offset ], token->length ) ) )
  {
    printf( "VT PARSER: Failed to declare variable \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
    return 0;
  }
  decl = malloc( sizeof(vtDeclSysGlobal) );
  decl->scope = scope;
  decl->sysvar = sysvar;
  decl->next = 0;
  *parser->declsyslast = decl;
  parser->declsyslast = &decl->next;
  return sysvar;
}


static void vtParseStatement( vtParser *parser )
{
  int nameclass;
  vtGenericType gtype;
  vtMemOffset memoffset;
  vtToken *token;
  vtSysVar *sysvar;
  vtSysType *systype;
  vtSysMember *sysmember;
  vtSysMethod *sysmethod;
  void *idobject, *idmember;
  vtMemOffset retoffset[VT_RET_COUNT_MAX];

  if( parser->errorcount )
    return;

  if( vtTokenAccept( parser, VT_TOKEN_NOT ) )
  {
    if( !( parser->bracedepth ) )
    {
      token = vtTokenExpect( parser, VT_TOKEN_IDENTIFIER );
      if( !( token ) )
        return;
      nameclass = vtParseIdentifier( parser, token, &idobject, &idmember );
      if( nameclass != VT_NAME_CLASS_SYSVAR )
      {
        printf( "VT PARSER: Identifier to be destroyed is not a system variable\n" );
        parser->errorcount++;
        return;
      }
      vtOpSysGeneric( parser, VT_OP_SYSDISABLE, (vtSysVar *)idobject );
    }
    else
    {
      printf( "VT PARSER: Can not destroy system variables in a non-global scope\n" );
      parser->errorcount++;
    }
    vtTokenExpect( parser, VT_TOKEN_SEMICOLON );
  }
  else if( ( token = vtTokenAccept( parser, VT_TOKEN_IDENTIFIER ) ) )
  {
    nameclass = vtParseIdentifier( parser, token, &idobject, &idmember );
    switch( nameclass )
    {
      case VT_NAME_CLASS_SYSTYPE:
        if( !( parser->bracedepth ) )
        {
          if( !( sysvar = vtParseSysDeclare( parser, (vtSysType *)idobject ) ) )
            return;
        }
        else
        {
          printf( "VT PARSER: Can not declare system variables in a non-global scope\n" );
          parser->errorcount++;
          return;
        }
        if( vtTokenAccept( parser, VT_TOKEN_SEMICOLON ) )
          return;
        systype = sysvar->type;
        if( systype->flags & VT_SYSTYPE_FLAGS_STRUCT )
        {
          printf( "VT PARSER: Can't set initial value for a struct-type system variable\n" );
          parser->errorcount++;
          return;
        }
        gtype.basetype = (sysvar->type)->u.typescalar.basetype;
        gtype.matchtype = sysvar->type;
        nameclass = VT_NAME_CLASS_SYSVAR;
        break;
      case VT_NAME_CLASS_SYSVAR:
        sysvar = (vtSysVar *)idobject;
        gtype.basetype = (sysvar->type)->u.typescalar.basetype;
        gtype.matchtype = sysvar->type;
        break;
      case VT_NAME_CLASS_SYSMEMBER:
        sysvar = (vtSysVar *)idobject;
        sysmember = (vtSysMember *)idmember;
        gtype.basetype = sysmember->basetype;
        gtype.matchtype = sysmember;
        break;
      case VT_NAME_CLASS_SYSMETHOD:
        sysvar = (vtSysVar *)idobject;
        sysmethod = (vtSysMethod *)idmember;
        if( !( vtParseCallFormat( parser, &sysmethod->format, 0, 0 ) ) )
          return;
        vtTokenExpect( parser, VT_TOKEN_SEMICOLON );
        vtOpSysCall( parser, VT_OP_CALL_SYSMETHOD, sysvar, sysmethod );
        return;
      default:
        printf( "VT PARSER: Failed to parse identifier \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
        parser->errorcount++;
        return;
    }

    /* Identifier = expression */
    /* TODO: Add support for +=, -=, etc. */
    vtTokenExpect( parser, VT_TOKEN_MOV );
    memoffset = vtParseExpression( parser, &gtype );

    /* Write variable with rvalue */
    switch( nameclass )
    {
      case VT_NAME_CLASS_SYSVAR:
        vtOpSysWrite( parser, sysvar, memoffset, &gtype );
        break;
      case VT_NAME_CLASS_SYSMEMBER:
        vtOpSysWriteMember( parser, sysvar, sysmember, memoffset, &gtype );
        break;
      default:
        printf( "VT PARSER: Internal error at %s:%d\n", __FILE__, __LINE__ );
        parser->errorcount++;
        return;
    }
    vtTokenExpect( parser, VT_TOKEN_SEMICOLON );
  }
  else if( vtTokenAccept( parser, VT_TOKEN_SEMICOLON ) )
  {
    /* Empty statement */
  }
  else if( vtTokenAccept( parser, VT_TOKEN_LBRACE ) )
  {
    /* { foo; bar; } */
    parser->bracedepth++;
    for( ; ; )
    {
      if( vtTokenAccept( parser, VT_TOKEN_RBRACE ) )
        break;
      vtParseStatement( parser );
      if( parser->errorcount )
        break;
    }
    parser->bracedepth--;
  }
  else if( vtTokenAccept( parser, VT_TOKEN_IF ) )
  {
    printf( "VT PARSER: Not supported %s:%d\n", __FILE__, __LINE__ );
    parser->errorcount++;

    parser->bracedepth++;
    /* if ( condition ) */
    vtTokenExpect( parser, VT_TOKEN_LPAREN );
    vtParseCondition( parser, &gtype );
    vtTokenExpect( parser, VT_TOKEN_RPAREN );
    vtParseStatement( parser );
    vtTokenExpect( parser, VT_TOKEN_SEMICOLON );
    /* else */
    if( vtTokenAccept( parser, VT_TOKEN_ELSE ) )
    {
      vtParseStatement( parser );
    }
    parser->bracedepth--;
  }
  else if( vtTokenAccept( parser, VT_TOKEN_WHILE ) )
  {
    printf( "VT PARSER: Not supported %s:%d\n", __FILE__, __LINE__ );
    parser->errorcount++;

    parser->bracedepth++;
    /* while( condition ) */
    vtTokenExpect( parser, VT_TOKEN_LPAREN );
    vtParseCondition( parser, &gtype );
    vtTokenExpect( parser, VT_TOKEN_RPAREN );
    vtParseStatement( parser );
    vtTokenExpect( parser, VT_TOKEN_SEMICOLON );
    parser->bracedepth--;
  }
  else
  {
    printf( "VT PARSER: Error, unexpected token \"%.*s\" ( %s:%d )\n", (int)(parser->token)->length, &parser->codestring[ (parser->token)->offset ], __FILE__, __LINE__ );
    parser->errorcount++;
  }

  return;
}


////


void vtInitParser( vtSystem *system, vtParser *parser )
{
  vtOpBuffer *opbuf;
  parser->system = system;
  parser->token = 0;
  parser->nexttoken = 0;
  parser->tokentype = VT_TOKEN_END;
  parser->codestring = 0;
  parser->tokenbufindex = 0;
  parser->tokenbuf = 0;
  parser->errorcount = 0;
  parser->bracedepth = 0;
  opbuf = malloc( sizeof(vtOpBuffer) );
  opbuf->opcount = 0;
  opbuf->next = 0;
  parser->opbuflist = opbuf;
  parser->opbuflast = opbuf;
  parser->memsize = 0;
  parser->staticsize = 0;
  parser->staticmemory = 0;
  parser->currentscope = &system->globalscope;
  parser->declsyslist = 0;
  parser->declsyslast = &parser->declsyslist;
  return;
}

int vtParseScript( vtParser *parser, char *codestring, vtTokenBuffer *tokenbuf )
{
  vtGenericType gtype;
  vtDeclSysGlobal *decl, *declnext;
  vtTokenInit( parser, codestring, tokenbuf );
  for( ; !( vtTokenAccept( parser, VT_TOKEN_END ) ) ; )
  {
    vtParseStatement( parser );
    if( parser->errorcount )
      break;
  }
  if( parser->errorcount )
  {
    /* TODO: If compilation failed, destroy all global sysvars we just created */
    for( decl = parser->declsyslist ; decl ; decl = declnext )
    {
      declnext = decl->next;
      vtDestroySysVar( parser->system, decl->scope, decl->sysvar );
      free( decl );
    }
    parser->declsyslist = 0;
    vtResetParser( parser );
    return 0;
  }
  gtype.basetype = VT_BASETYPE_INT32;
  gtype.matchtype = 0;
  vtOpGeneric1( parser, VT_OP_RETURN, 0, &gtype );
  return 1;
}

void vtResetParser( vtParser *parser )
{
  vtFreeParser( parser );
  vtInitParser( parser->system, parser );
  return;
}

void vtFreeParser( vtParser *parser )
{
  vtDeclSysGlobal *decl, *declnext;
  vtOpBuffer *opbuf, *opbufnext;
  for( opbuf = parser->opbuflist ; opbuf ; opbuf = opbufnext )
  {
    opbufnext = opbuf->next;
    free( opbuf );
  }
  parser->opbuflist = 0;
  parser->opbuflast = 0;
  if( parser->staticmemory )
  {
    free( parser->staticmemory );
    parser->staticmemory = 0;
  }
  for( decl = parser->declsyslist ; decl ; decl = declnext )
  {
    declnext = decl->next;
    free( decl );
  }
  parser->declsyslist = 0;
  parser->declsyslast = 0;
  return;
}



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

#define VT_OP_PARAM_MAX (3)

typedef struct
{
  vtVarStorage storage;
} vtOpDefImm;

typedef struct
{
  vtSysVar *var;
  void *typemember;
} vtOpDefSys;

typedef uint32_t vtMemOffset;

typedef struct
{
  int code;
  vtMemOffset memoffset[VT_OP_PARAM_MAX];
  int basetype;
  union
  {
    vtOpDefImm imm;
    vtOpDefSys sys;
  } u;
} vtOp;

#define VT_OPCODE(op,basetype) (((op)*VT_BASETYPE_COUNT)+(basetype))


enum
{
  /* Store imm to [memoffset0] */
  VT_OP_IMM,

  /* Cast [memoffset0] from srcbasetype to dstbasetype, store to [memoffset1] */
  VT_OP_CAST_CHAR,
  VT_OP_CAST_INT8,
  VT_OP_CAST_UINT8,
  VT_OP_CAST_INT16,
  VT_OP_CAST_UINT16,
  VT_OP_CAST_INT32,
  VT_OP_CAST_UINT32,
  VT_OP_CAST_INT64,
  VT_OP_CAST_UINT64,
  VT_OP_CAST_FLOAT,
  VT_OP_CAST_DOUBLE,
  VT_OP_CAST_POINTER,

  /* Read system variable, store to [memoffset0] */
  VT_OP_SYSREAD,
  VT_OP_SYSREADMEMBER,
  /* Load [memoffset0], write to system variable */
  VT_OP_SYSWRITE,
  VT_OP_SYSWRITEMEMBER,
  /* Alloc/Free system variables */
  VT_OP_SYSENABLE,
  VT_OP_SYSDISABLE,

  /* Generic ops, input [memoffset1] applied to [memoffset0] */
  VT_OP_MOV,
  VT_OP_ADD,
  VT_OP_SUB,
  VT_OP_NEGATE, /* -foo */
  VT_OP_MUL,
  VT_OP_DIV,
  VT_OP_MOD,
  VT_OP_OR,
  VT_OP_XOR,
  VT_OP_AND,
  VT_OP_NOT, /* ~foo */
  VT_OP_SHL,
  VT_OP_SHR,
  VT_OP_ROL,
  VT_OP_ROR,
  VT_OP_LOGNOT, /* ! */

  /* Comparison ops, store result as integer VT_CMP_BASETYPE */
  VT_OP_SETEQ,
  VT_OP_SETLT,
  VT_OP_SETLE,
  VT_OP_SETGT,
  VT_OP_SETGE,
  VT_OP_SETNE,

  /* Conditional jumps based on comparison */
  VT_OP_JMPEQ,
  VT_OP_JMPLT,
  VT_OP_JMPLE,
  VT_OP_JMPGT,
  VT_OP_JMPGE,
  VT_OP_JMPNE,

  /* Logical flow control, conditional jumps implied */
  VT_OP_LOGOR,  /* || */
  VT_OP_LOGAND, /* && */

  /* Function call, push arg/ret pointers */
  VT_OP_PUSH_ARG,
  VT_OP_PUSH_RET,
  VT_OP_CALL_SYSFUNCTION,
  VT_OP_CALL_SYSMETHOD,

  /* Return from function/method call, or exit if top-level */
  VT_OP_RETURN,

  VT_OP_COUNT
};


/* Op buffer size */
#define VT_OP_BUFFER_SIZE (64)
/* Threshold to stop inserting ops in buffer, leave room for op shuffling */
#define VT_OP_BUFFER_CLAMP (56)

typedef struct
{
  int opcount;
  vtOp oplist[VT_OP_BUFFER_SIZE];
  void *next;
} vtOpBuffer;

typedef struct
{
  vtScope *scope;
  vtSysVar *sysvar;
  void *next;
} vtDeclSysGlobal;

typedef struct
{
  vtSystem *system;
  vtToken *token;
  vtToken *nexttoken;
  int tokentype;

  char *codestring;
  int tokenbufindex;
  vtTokenBuffer *tokenbuf;
  int errorcount;
  int bracedepth;

  void *opbuflist;
  void *opbuflast;

  /* TODO: Differentiate sizes and types, for consolidation and optimization? */
  vtMemOffset memsize;
  vtMemOffset staticsize;
  void *staticmemory;

  /* Track scope hierarchy */
  vtScope *currentscope;

  /* Linked list of global system variable declarations */
  void *declsyslist;
  void **declsyslast;

} vtParser;



////


void vtInitParser( vtSystem *system, vtParser *parser );

int vtParseScript( vtParser *parser, char *codestring, vtTokenBuffer *tokenbuf );

void vtResetParser( vtParser *parser );

void vtFreeParser( vtParser *parser );



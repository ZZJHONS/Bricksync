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
#include "vtexec.h"


////


#define VT_EXEC_DEBUG (1)


#if CPUCONF_FLOAT_SIZE == CPUCONF_CHAR_SIZE
 typedef unsigned char vtFloatInt;
#elif CPUCONF_FLOAT_SIZE == CPUCONF_SHORT_SIZE
 typedef unsigned short vtFloatInt;
#elif CPUCONF_FLOAT_SIZE == CPUCONF_INT_SIZE
 typedef unsigned int vtFloatInt;
#elif CPUCONF_FLOAT_SIZE == CPUCONF_LONG_SIZE
 typedef unsigned long vtFloatInt;
#elif CPUCONF_FLOAT_SIZE == CPUCONF_LONG_LONG_SIZE
 typedef unsigned long long vtFloatInt;
#else
 #error Could not determine integer width equal to float width
#endif

#if CPUCONF_DOUBLE_SIZE == CPUCONF_CHAR_SIZE
 typedef unsigned char vtDoubleInt;
#elif CPUCONF_DOUBLE_SIZE == CPUCONF_SHORT_SIZE
 typedef unsigned short vtDoubleInt;
#elif CPUCONF_DOUBLE_SIZE == CPUCONF_INT_SIZE
 typedef unsigned int vtDoubleInt;
#elif CPUCONF_DOUBLE_SIZE == CPUCONF_LONG_SIZE
 typedef unsigned long vtDoubleInt;
#elif CPUCONF_DOUBLE_SIZE == CPUCONF_LONG_LONG_SIZE
 typedef unsigned long long vtDoubleInt;
#else
 #error Could not determine integer width equal to double width
#endif



int vtExec( vtParser *parser )
{
  int opindex;
  vtMemOffset memoffset;
  void *mem0, *mem1, *mem2;
  vtOpBuffer *opbuf;
  vtOp *op, *opend;
  char *stack;
  vtSysVar *sysvar;
  vtSysType *systype;
  vtSysMember *sysmember;
  vtSysFunction *sysfunction;
  vtSysMethod *sysmethod;
  vtVarStorage storage;
  uintptr_t staticmem;
  /* Function/Method calls */
  int argindex, retindex;
  void *callarg[VT_ARG_COUNT_MAX];
  void *callret[VT_RET_COUNT_MAX];

  /* Temporary storage stuff */
  char cd, cs;
  int8_t i8d, i8s;
  uint8_t u8d, u8s;
  int16_t i16d, i16s;
  uint16_t u16d, u16s;
  int32_t i32d, i32s;
  uint32_t u32d, u32s;
  int64_t i64d, i64s;
  uint64_t u64d, u64s;
  vtFloatInt ifd, ifs;
  vtDoubleInt idd, ids;

  opbuf = parser->opbuflist;
  op = opbuf->oplist;
  opend = &op[ opbuf->opcount ];
  stack = malloc( parser->memsize );
  staticmem = (uintptr_t)parser->staticmemory;

  argindex = 0;
  retindex = 0;

  for( ; ; op++ )
  {
    if( op == opend )
    {
      opbuf = opbuf->next;
      op = opbuf->oplist;
      opend = &op[ opbuf->opcount ];
    }

    mem0 = &stack[ op->memoffset[0] ];
    mem1 = &stack[ op->memoffset[1] ];
    mem2 = &stack[ op->memoffset[2] ];

#if VT_EXEC_DEBUG
printf( "  Exec Opcode %d\n", op->code );
#endif

    /* Execute the opcode */
    switch( op->code )
    {
      /* Immediate */
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_CHAR):
        *((char *)mem0) = op->u.imm.storage.c;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = op->u.imm.storage.i8;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = op->u.imm.storage.i8;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = op->u.imm.storage.i16;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = op->u.imm.storage.i16;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = op->u.imm.storage.i32;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = op->u.imm.storage.i32;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = op->u.imm.storage.i64;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = op->u.imm.storage.i64;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_FLOAT):
        *((float *)mem0) = op->u.imm.storage.f;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = op->u.imm.storage.d;
        break;
      case VT_OPCODE(VT_OP_IMM,VT_BASETYPE_POINTER):
        *((void **)mem0) = ADDRESS( op->u.imm.storage.p, staticmem );
        break;

      /* Cast to char */
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_CHAR):
        *((char *)mem0) = (char)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_INT8):
        *((char *)mem0) = (char)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_UINT8):
        *((char *)mem0) = (char)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_INT16):
        *((char *)mem0) = (char)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_UINT16):
        *((char *)mem0) = (char)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_INT32):
        *((char *)mem0) = (char)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_UINT32):
        *((char *)mem0) = (char)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_INT64):
        *((char *)mem0) = (char)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_UINT64):
        *((char *)mem0) = (char)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_FLOAT):
        *((char *)mem0) = (char)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_CHAR,VT_BASETYPE_DOUBLE):
        *((char *)mem0) = (char)*((double *)mem1);
        break;

      /* Cast to int8 */
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_CHAR):
        *((int8_t *)mem0) = (int8_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = (int8_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_UINT8):
        *((int8_t *)mem0) = (int8_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_INT16):
        *((int8_t *)mem0) = (int8_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_UINT16):
        *((int8_t *)mem0) = (int8_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_INT32):
        *((int8_t *)mem0) = (int8_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_UINT32):
        *((int8_t *)mem0) = (int8_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_INT64):
        *((int8_t *)mem0) = (int8_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_UINT64):
        *((int8_t *)mem0) = (int8_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_FLOAT):
        *((int8_t *)mem0) = (int8_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT8,VT_BASETYPE_DOUBLE):
        *((int8_t *)mem0) = (int8_t)*((double *)mem1);
        break;

      /* Cast to uint8 */
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_CHAR):
        *((uint8_t *)mem0) = (uint8_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_INT8):
        *((uint8_t *)mem0) = (uint8_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = (uint8_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_INT16):
        *((uint8_t *)mem0) = (uint8_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_UINT16):
        *((uint8_t *)mem0) = (uint8_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_INT32):
        *((uint8_t *)mem0) = (uint8_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_UINT32):
        *((uint8_t *)mem0) = (uint8_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_INT64):
        *((uint8_t *)mem0) = (uint8_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_UINT64):
        *((uint8_t *)mem0) = (uint8_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_FLOAT):
        *((uint8_t *)mem0) = (uint8_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT8,VT_BASETYPE_DOUBLE):
        *((uint8_t *)mem0) = (uint8_t)*((double *)mem1);
        break;

      /* Cast to int16 */
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_CHAR):
        *((int16_t *)mem0) = (int16_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_INT8):
        *((int16_t *)mem0) = (int16_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_UINT8):
        *((int16_t *)mem0) = (int16_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = (int16_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_UINT16):
        *((int16_t *)mem0) = (int16_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_INT32):
        *((int16_t *)mem0) = (int16_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_UINT32):
        *((int16_t *)mem0) = (int16_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_INT64):
        *((int16_t *)mem0) = (int16_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_UINT64):
        *((int16_t *)mem0) = (int16_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_FLOAT):
        *((int16_t *)mem0) = (int16_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT16,VT_BASETYPE_DOUBLE):
        *((int16_t *)mem0) = (int16_t)*((double *)mem1);
        break;

      /* Cast to uint16 */
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_CHAR):
        *((uint16_t *)mem0) = (uint16_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_INT8):
        *((uint16_t *)mem0) = (uint16_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_UINT8):
        *((uint16_t *)mem0) = (uint16_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_INT16):
        *((uint16_t *)mem0) = (uint16_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = (uint16_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_INT32):
        *((uint16_t *)mem0) = (uint16_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_UINT32):
        *((uint16_t *)mem0) = (uint16_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_INT64):
        *((uint16_t *)mem0) = (uint16_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_UINT64):
        *((uint16_t *)mem0) = (uint16_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_FLOAT):
        *((uint16_t *)mem0) = (uint16_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT16,VT_BASETYPE_DOUBLE):
        *((uint16_t *)mem0) = (uint16_t)*((double *)mem1);
        break;

      /* Cast to int32 */
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_CHAR):
        *((int32_t *)mem0) = (int32_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_INT8):
        *((int32_t *)mem0) = (int32_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_UINT8):
        *((int32_t *)mem0) = (int32_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_INT16):
        *((int32_t *)mem0) = (int32_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_UINT16):
        *((int32_t *)mem0) = (int32_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = (int32_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_UINT32):
        *((int32_t *)mem0) = (int32_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_INT64):
        *((int32_t *)mem0) = (int32_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_UINT64):
        *((int32_t *)mem0) = (int32_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_FLOAT):
        *((int32_t *)mem0) = (int32_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT32,VT_BASETYPE_DOUBLE):
        *((int32_t *)mem0) = (int32_t)*((double *)mem1);
        break;

      /* Cast to uint32 */
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_CHAR):
        *((uint32_t *)mem0) = (uint32_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_INT8):
        *((uint32_t *)mem0) = (uint32_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_UINT8):
        *((uint32_t *)mem0) = (uint32_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_INT16):
        *((uint32_t *)mem0) = (uint32_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_UINT16):
        *((uint32_t *)mem0) = (uint32_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_INT32):
        *((uint32_t *)mem0) = (uint32_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = (uint32_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_INT64):
        *((uint32_t *)mem0) = (uint32_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_UINT64):
        *((uint32_t *)mem0) = (uint32_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_FLOAT):
        *((uint32_t *)mem0) = (uint32_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT32,VT_BASETYPE_DOUBLE):
        *((uint32_t *)mem0) = (uint32_t)*((double *)mem1);
        break;

      /* Cast to int64 */
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_CHAR):
        *((int64_t *)mem0) = (int64_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_INT8):
        *((int64_t *)mem0) = (int64_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_UINT8):
        *((int64_t *)mem0) = (int64_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_INT16):
        *((int64_t *)mem0) = (int64_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_UINT16):
        *((int64_t *)mem0) = (int64_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_INT32):
        *((int64_t *)mem0) = (int64_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_UINT32):
        *((int64_t *)mem0) = (int64_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = (int64_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_UINT64):
        *((int64_t *)mem0) = (int64_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_FLOAT):
        *((int64_t *)mem0) = (int64_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_INT64,VT_BASETYPE_DOUBLE):
        *((int64_t *)mem0) = (int64_t)*((double *)mem1);
        break;

      /* Cast to uint64 */
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_CHAR):
        *((uint64_t *)mem0) = (uint64_t)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_INT8):
        *((uint64_t *)mem0) = (uint64_t)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_UINT8):
        *((uint64_t *)mem0) = (uint64_t)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_INT16):
        *((uint64_t *)mem0) = (uint64_t)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_UINT16):
        *((uint64_t *)mem0) = (uint64_t)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_INT32):
        *((uint64_t *)mem0) = (uint64_t)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_UINT32):
        *((uint64_t *)mem0) = (uint64_t)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_INT64):
        *((uint64_t *)mem0) = (uint64_t)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = (uint64_t)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_FLOAT):
        *((uint64_t *)mem0) = (uint64_t)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_UINT64,VT_BASETYPE_DOUBLE):
        *((uint64_t *)mem0) = (uint64_t)*((double *)mem1);
        break;

      /* Cast to float */
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_CHAR):
        *((float *)mem0) = (float)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_INT8):
        *((float *)mem0) = (float)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_UINT8):
        *((float *)mem0) = (float)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_INT16):
        *((float *)mem0) = (float)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_UINT16):
        *((float *)mem0) = (float)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_INT32):
        *((float *)mem0) = (float)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_UINT32):
        *((float *)mem0) = (float)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_INT64):
        *((float *)mem0) = (float)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_UINT64):
        *((float *)mem0) = (float)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_FLOAT):
        *((float *)mem0) = (float)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_FLOAT,VT_BASETYPE_DOUBLE):
        *((float *)mem0) = (float)*((double *)mem1);
        break;

      /* Cast to double */
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_CHAR):
        *((double *)mem0) = (double)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_INT8):
        *((double *)mem0) = (double)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_UINT8):
        *((double *)mem0) = (double)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_INT16):
        *((double *)mem0) = (double)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_UINT16):
        *((double *)mem0) = (double)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_INT32):
        *((double *)mem0) = (double)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_UINT32):
        *((double *)mem0) = (double)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_INT64):
        *((double *)mem0) = (double)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_UINT64):
        *((double *)mem0) = (double)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_FLOAT):
        *((double *)mem0) = (double)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_CAST_DOUBLE,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = (double)*((double *)mem1);
        break;

      /* Sysread */
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSREAD,VT_BASETYPE_POINTER):
        sysvar = op->u.sys.var;
        systype = op->u.sys.typemember;
        if( sysvar->flags & VT_SYSVAR_FLAGS_DISABLED )
        {
          printf( "VT EXEC: Runtime error, accessing destroyed system variable\n" );
          return;
        }
        if( !( systype->u.typescalar.read( systype->typevalue, sysvar->storagevalue, 0, mem0 ) ) )
          return;
        break;

      /* Sysreadmember */
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSREADMEMBER,VT_BASETYPE_POINTER):
        sysvar = op->u.sys.var;
        sysmember = op->u.sys.typemember;
        if( sysvar->flags & VT_SYSVAR_FLAGS_DISABLED )
        {
          printf( "VT EXEC: Runtime error, accessing destroyed system variable\n" );
          return;
        }
        if( !( sysmember->read( sysmember->membervalue, sysvar->storagevalue, 0, mem0 ) ) )
          return;
        break;

      /* Syswrite */
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSWRITE,VT_BASETYPE_POINTER):
        sysvar = op->u.sys.var;
        systype = op->u.sys.typemember;
        if( sysvar->flags & VT_SYSVAR_FLAGS_DISABLED )
        {
          printf( "VT EXEC: Runtime error, accessing destroyed system variable\n" );
          return;
        }
        if( !( systype->u.typescalar.write( systype->typevalue, sysvar->storagevalue, 0, mem0 ) ) )
          return;
        break;

      /* Syswritemember */
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSWRITEMEMBER,VT_BASETYPE_POINTER):
        sysvar = op->u.sys.var;
        sysmember = op->u.sys.typemember;
        if( sysvar->flags & VT_SYSVAR_FLAGS_DISABLED )
        {
          printf( "VT EXEC: Runtime error, accessing destroyed system variable\n" );
          return;
        }
        if( !( sysmember->write( sysmember->membervalue, sysvar->storagevalue, 0, mem0 ) ) )
          return;
        break;

      /* Mov */
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_CHAR):
        *((char *)mem0) = *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_FLOAT):
        *((float *)mem0) = *((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = *((double *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOV,VT_BASETYPE_POINTER):
        *((void **)mem0) = *((void **)mem1);
        break;

      /* Add */
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_CHAR):
        *((char *)mem0) += *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_INT8):
        *((int8_t *)mem0) += *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) += *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_INT16):
        *((int16_t *)mem0) += *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) += *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_INT32):
        *((int32_t *)mem0) += *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) += *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_INT64):
        *((int64_t *)mem0) += *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) += *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_FLOAT):
        *((float *)mem0) += *((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_ADD,VT_BASETYPE_DOUBLE):
        *((double *)mem0) += *((double *)mem1);
        break;

      /* Sub */
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_CHAR):
        *((char *)mem0) -= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_INT8):
        *((int8_t *)mem0) -= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) -= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_INT16):
        *((int16_t *)mem0) -= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) -= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_INT32):
        *((int32_t *)mem0) -= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) -= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_INT64):
        *((int64_t *)mem0) -= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) -= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_FLOAT):
        *((float *)mem0) -= *((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_SUB,VT_BASETYPE_DOUBLE):
        *((double *)mem0) -= *((double *)mem1);
        break;

      /* Negate */
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_CHAR):
        *((char *)mem0) = -*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = -*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = -*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = -*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = -*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = -*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = -*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = -*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = -*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_FLOAT):
        *((float *)mem0) = -*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_NEGATE,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = -*((double *)mem1);
        break;

      /* Mul */
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_CHAR):
        *((char *)mem0) *= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_INT8):
        *((int8_t *)mem0) *= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) *= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_INT16):
        *((int16_t *)mem0) *= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) *= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_INT32):
        *((int32_t *)mem0) *= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) *= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_INT64):
        *((int64_t *)mem0) *= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) *= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_FLOAT):
        *((float *)mem0) *= *((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_MUL,VT_BASETYPE_DOUBLE):
        *((double *)mem0) *= *((double *)mem1);
        break;

      /* Div */
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_CHAR):
        *((char *)mem0) /= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_INT8):
        *((int8_t *)mem0) /= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) /= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_INT16):
        *((int16_t *)mem0) /= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) /= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_INT32):
        *((int32_t *)mem0) /= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) /= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_INT64):
        *((int64_t *)mem0) /= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) /= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_FLOAT):
        *((float *)mem0) /= *((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_DIV,VT_BASETYPE_DOUBLE):
        *((double *)mem0) /= *((double *)mem1);
        break;

      /* Mod */
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_CHAR):
        *((char *)mem0) %= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_INT8):
        *((int8_t *)mem0) %= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) %= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_INT16):
        *((int16_t *)mem0) %= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) %= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_INT32):
        *((int32_t *)mem0) %= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) %= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_INT64):
        *((int64_t *)mem0) %= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) %= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_FLOAT):
        *((float *)mem0) = fmodf( *((float *)mem0), *((float *)mem1) );
        break;
      case VT_OPCODE(VT_OP_MOD,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = fmod( *((double *)mem0), *((double *)mem1) );
        break;

      /* Or */
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_CHAR):
        *((char *)mem0) |= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_INT8):
        *((int8_t *)mem0) |= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) |= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_INT16):
        *((int16_t *)mem0) |= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) |= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_INT32):
        *((int32_t *)mem0) |= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) |= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_INT64):
        *((int64_t *)mem0) |= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) |= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_FLOAT):
        *((vtFloatInt *)mem0) |= *((vtFloatInt *)mem1);
        break;
      case VT_OPCODE(VT_OP_OR,VT_BASETYPE_DOUBLE):
        *((vtDoubleInt *)mem0) |= *((vtDoubleInt *)mem1);
        break;

      /* Xor */
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_CHAR):
        *((char *)mem0) ^= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_INT8):
        *((int8_t *)mem0) ^= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) ^= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_INT16):
        *((int16_t *)mem0) ^= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) ^= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_INT32):
        *((int32_t *)mem0) ^= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) ^= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_INT64):
        *((int64_t *)mem0) ^= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) ^= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_FLOAT):
        *((vtFloatInt *)mem0) ^= *((vtFloatInt *)mem1);
        break;
      case VT_OPCODE(VT_OP_XOR,VT_BASETYPE_DOUBLE):
        *((vtDoubleInt *)mem0) ^= *((vtDoubleInt *)mem1);
        break;

      /* And */
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_CHAR):
        *((char *)mem0) &= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_INT8):
        *((int8_t *)mem0) &= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) &= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_INT16):
        *((int16_t *)mem0) &= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) &= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_INT32):
        *((int32_t *)mem0) &= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) &= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_INT64):
        *((int64_t *)mem0) &= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) &= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_FLOAT):
        *((vtFloatInt *)mem0) &= *((vtFloatInt *)mem1);
        break;
      case VT_OPCODE(VT_OP_AND,VT_BASETYPE_DOUBLE):
        *((vtDoubleInt *)mem0) &= *((vtDoubleInt *)mem1);
        break;

      /* Not */
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_CHAR):
        *((char *)mem0) = ~*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = ~*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = ~*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = ~*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = ~*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = ~*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = ~*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = ~*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = ~*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_FLOAT):
        *((vtFloatInt *)mem0) = ~*((vtFloatInt *)mem1);
        break;
      case VT_OPCODE(VT_OP_NOT,VT_BASETYPE_DOUBLE):
        *((vtDoubleInt *)mem0) = ~*((vtDoubleInt *)mem1);
        break;

      /* Shl */
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_CHAR):
        *((char *)mem0) <<= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_INT8):
        *((int8_t *)mem0) <<= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) <<= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_INT16):
        *((int16_t *)mem0) <<= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) <<= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_INT32):
        *((int32_t *)mem0) <<= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) <<= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_INT64):
        *((int64_t *)mem0) <<= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) <<= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_FLOAT):
        *((vtFloatInt *)mem0) <<= *((vtFloatInt *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHL,VT_BASETYPE_DOUBLE):
        *((vtDoubleInt *)mem0) <<= *((vtDoubleInt *)mem1);
        break;

      /* Shr */
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_CHAR):
        *((char *)mem0) >>= *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_INT8):
        *((int8_t *)mem0) >>= *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) >>= *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_INT16):
        *((int16_t *)mem0) >>= *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) >>= *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_INT32):
        *((int32_t *)mem0) >>= *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) >>= *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_INT64):
        *((int64_t *)mem0) >>= *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) >>= *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_FLOAT):
        *((vtFloatInt *)mem0) >>= *((vtFloatInt *)mem1);
        break;
      case VT_OPCODE(VT_OP_SHR,VT_BASETYPE_DOUBLE):
        *((vtDoubleInt *)mem0) >>= *((vtDoubleInt *)mem1);
        break;

      /* Rol */
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_CHAR):
        cd = *((char *)mem0);
        cs = *((char *)mem1);
        *((char *)mem0) = ( cd << cs ) | ( cd >> (CPUCONF_CHAR_BITS-cs) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_INT8):
        i8d = *((int8_t *)mem0);
        i8s = *((int8_t *)mem1);
        *((int8_t *)mem0) = ( i8d << i8s ) | ( i8d >> (8-i8s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_UINT8):
        u8d = *((uint8_t *)mem0);
        u8s = *((uint8_t *)mem1);
        *((uint8_t *)mem0) = ( u8d << u8s ) | ( u8d >> (8-u8s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_INT16):
        i16d = *((int16_t *)mem0);
        i16s = *((int16_t *)mem1);
        *((int16_t *)mem0) = ( i16d << i16s ) | ( i16d >> (16-i16s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_UINT16):
        u16d = *((uint16_t *)mem0);
        u16s = *((uint16_t *)mem1);
        *((uint16_t *)mem0) = ( u16d << u16s ) | ( u16d >> (16-u16s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_INT32):
        i32d = *((int32_t *)mem0);
        i32s = *((int32_t *)mem1);
        *((int32_t *)mem0) = ( i32d << i32s ) | ( i32d >> (32-i32s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_UINT32):
        u32d = *((uint32_t *)mem0);
        u32s = *((uint32_t *)mem1);
        *((uint32_t *)mem0) = ( u32d << u32s ) | ( u32d >> (32-u32s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_INT64):
        i64d = *((int64_t *)mem0);
        i64s = *((int64_t *)mem1);
        *((int64_t *)mem0) = ( i64d << i64s ) | ( i64d >> (64-i64s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_UINT64):
        u64d = *((uint64_t *)mem0);
        u64s = *((uint64_t *)mem1);
        *((uint64_t *)mem0) = ( u64d << u64s ) | ( u64d >> (64-u64s) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_FLOAT):
        ifd = *((vtFloatInt *)mem0);
        ifs = *((vtFloatInt *)mem1);
        *((vtFloatInt *)mem0) = ( ifd << ifs ) | ( ifd >> (CPUCONF_FLOAT_BITS-ifs) );
        break;
      case VT_OPCODE(VT_OP_ROL,VT_BASETYPE_DOUBLE):
        idd = *((vtDoubleInt *)mem0);
        ids = *((vtDoubleInt *)mem1);
        *((vtDoubleInt *)mem0) = ( idd << ids ) | ( idd >> (CPUCONF_DOUBLE_BITS-ids) );
        break;

      /* Ror */
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_CHAR):
        cd = *((char *)mem0);
        cs = *((char *)mem1);
        *((char *)mem0) = ( cd >> cs ) | ( cd << (CPUCONF_CHAR_BITS-cs) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_INT8):
        i8d = *((int8_t *)mem0);
        i8s = *((int8_t *)mem1);
        *((int8_t *)mem0) = ( i8d >> i8s ) | ( i8d << (8-i8s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_UINT8):
        u8d = *((uint8_t *)mem0);
        u8s = *((uint8_t *)mem1);
        *((uint8_t *)mem0) = ( u8d >> u8s ) | ( u8d << (8-u8s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_INT16):
        i16d = *((int16_t *)mem0);
        i16s = *((int16_t *)mem1);
        *((int16_t *)mem0) = ( i16d >> i16s ) | ( i16d << (16-i16s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_UINT16):
        u16d = *((uint16_t *)mem0);
        u16s = *((uint16_t *)mem1);
        *((uint16_t *)mem0) = ( u16d >> u16s ) | ( u16d << (16-u16s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_INT32):
        i32d = *((int32_t *)mem0);
        i32s = *((int32_t *)mem1);
        *((int32_t *)mem0) = ( i32d >> i32s ) | ( i32d << (32-i32s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_UINT32):
        u32d = *((uint32_t *)mem0);
        u32s = *((uint32_t *)mem1);
        *((uint32_t *)mem0) = ( u32d >> u32s ) | ( u32d << (32-u32s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_INT64):
        i64d = *((int64_t *)mem0);
        i64s = *((int64_t *)mem1);
        *((int64_t *)mem0) = ( i64d >> i64s ) | ( i64d << (64-i64s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_UINT64):
        u64d = *((uint64_t *)mem0);
        u64s = *((uint64_t *)mem1);
        *((uint64_t *)mem0) = ( u64d >> u64s ) | ( u64d << (64-u64s) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_FLOAT):
        ifd = *((vtFloatInt *)mem0);
        ifs = *((vtFloatInt *)mem1);
        *((vtFloatInt *)mem0) = ( ifd >> ifs ) | ( ifd << (CPUCONF_FLOAT_BITS-ifs) );
        break;
      case VT_OPCODE(VT_OP_ROR,VT_BASETYPE_DOUBLE):
        idd = *((vtDoubleInt *)mem0);
        ids = *((vtDoubleInt *)mem1);
        *((vtDoubleInt *)mem0) = ( idd >> ids ) | ( idd << (CPUCONF_DOUBLE_BITS-ids) );
        break;

      /* LogNot */
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_CHAR):
        *((char *)mem0) = !*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = !*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = !*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = !*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = !*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = !*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = !*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = !*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = !*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_FLOAT):
        *((float *)mem0) = !*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_LOGNOT,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = !*((double *)mem1);
        break;

      /* Seteq */
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) == *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) == *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) == *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) == *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) == *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) == *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) == *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) == *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) == *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) == *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) == *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETEQ,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) == *((void **)mem2) );
        break;

      /* Setlt */
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) < *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) < *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) < *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) < *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) < *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) < *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) < *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) < *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) < *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) < *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) < *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLT,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) < *((void **)mem2) );
        break;

      /* Setle */
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) <= *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) <= *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) <= *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) <= *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) <= *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) <= *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) <= *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) <= *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) <= *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) <= *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) <= *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETLE,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) <= *((void **)mem2) );
        break;

      /* Setgt */
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) > *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) > *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) > *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) > *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) > *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) > *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) > *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) > *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) > *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) > *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) > *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGT,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) > *((void **)mem2) );
        break;

      /* Setge */
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) >= *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) >= *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) >= *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) >= *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) >= *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) >= *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) >= *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) >= *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) >= *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) >= *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) >= *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETGE,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) >= *((void **)mem2) );
        break;

      /* Setne */
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) != *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) != *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) != *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) != *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) != *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) != *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) != *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) != *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) != *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) != *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) != *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_SETNE,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) != *((void **)mem2) );
        break;

      /* SysEnable */
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSENABLE,VT_BASETYPE_POINTER):
        if( !( vtEnableSysVar( parser->system, op->u.sys.var ) ) )
          printf( "VT EXEC: Runtime error, failed to enable system variable\n" );
        break;

      /* SysDisable */
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSDISABLE,VT_BASETYPE_POINTER):
        if( !( vtDisableSysVar( parser->system, op->u.sys.var ) ) )
          printf( "VT EXEC: Runtime error, failed to disable system variable\n" );
        break;

      /* Push argument for call */
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_PUSH_ARG,VT_BASETYPE_POINTER):
        if( argindex >= VT_OP_PUSH_ARG )
        {
          printf( "VT EXEC: Runtime error, call argument stack overflowed\n" );
          return;
        }
        callarg[argindex++] = mem0;
        break;

      /* Push return for call */
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_PUSH_RET,VT_BASETYPE_POINTER):
        if( retindex >= VT_OP_PUSH_RET )
        {
          printf( "VT EXEC: Runtime error, call return stack overflowed\n" );
          return;
        }
        callret[retindex++] = mem0;
        break;

      /* Call sysfunction */
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_CALL_SYSFUNCTION,VT_BASETYPE_POINTER):
        argindex = 0;
        retindex = 0;
        sysfunction = op->u.sys.typemember;
        argindex = 0;
        retindex = 0;
        sysfunction->call( callarg, callret );
        /* TODO */
        break;

      /* Call sysmethod */
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_CALL_SYSMETHOD,VT_BASETYPE_POINTER):
        sysvar = op->u.sys.var;
        systype = sysvar->type;
        sysmethod = op->u.sys.typemember;
        argindex = 0;
        retindex = 0;
        sysmethod->call( sysvar->storagevalue, callarg, callret );
        break;



////


#if 0
      /* Sysecho */
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_SYSECHO,VT_BASETYPE_POINTER):
        sysvar = op->u.sys.var;
        systype = op->u.sys.typemember;
        if( sysvar->flags & VT_SYSVAR_FLAGS_DISABLED )
        {
          printf( "VT EXEC: Runtime error, accessing destroyed system variable\n" );
          return;
        }
        if( !( systype->u.typescalar.read( systype->typevalue, sysvar->storagevalue, 0, (void *)&storage ) ) )
          return;
        switch( systype->u.typescalar.basetype )
        {
          case VT_BASETYPE_CHAR:
            printf( "SysVar type %d : %d ( '%c' )\n", 
          case VT_BASETYPE_INT8:
          case VT_BASETYPE_UINT8:
          case VT_BASETYPE_INT16:
          case VT_BASETYPE_UINT16:
          case VT_BASETYPE_INT32:
          case VT_BASETYPE_UINT32:
          case VT_BASETYPE_INT64:
          case VT_BASETYPE_UINT64:
          case VT_BASETYPE_FLOAT:
          case VT_BASETYPE_DOUBLE:
          case VT_BASETYPE_POINTER:
        }
        break;
#endif



////

#if 0
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_CHAR):
        *((vtCmpType *)mem0) = (vtCmpType)( *((char *)mem1) == *((char *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int8_t *)mem1) == *((int8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT8):
        *((vtCmpType *)mem0) = (vtCmpType)( *((uint8_t *)mem1) == *((uint8_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) == *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT16):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int16_t *)mem1) == *((int16_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) == *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT32):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int32_t *)mem1) == *((int32_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) == *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT64):
        *((vtCmpType *)mem0) = (vtCmpType)( *((int64_t *)mem1) == *((int64_t *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_FLOAT):
        *((vtCmpType *)mem0) = (vtCmpType)( *((float *)mem1) == *((float *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_DOUBLE):
        *((vtCmpType *)mem0) = (vtCmpType)( *((double *)mem1) == *((double *)mem2) );
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_POINTER):
        *((vtCmpType *)mem0) = (vtCmpType)( *((void **)mem1) == *((void **)mem2) );
        break;
#endif
#if 0
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_CHAR):
        *((www *)mem0) = (www)*((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT8):
        *((www *)mem0) = (www)*((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT8):
        *((www *)mem0) = (www)*((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT16):
        *((www *)mem0) = (www)*((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT16):
        *((www *)mem0) = (www)*((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT32):
        *((www *)mem0) = (www)*((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT32):
        *((www *)mem0) = (www)*((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT64):
        *((www *)mem0) = (www)*((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT64):
        *((www *)mem0) = (www)*((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_FLOAT):
        *((www *)mem0) = (www)*((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_DOUBLE):
        *((www *)mem0) = (www)*((double *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_POINTER):
        *((www *)mem0) = (www)*((void **)mem1);
        break;
#endif

#if 0
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_CHAR):
        *((char *)mem0) = *((char *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT8):
        *((int8_t *)mem0) = *((int8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT8):
        *((uint8_t *)mem0) = *((uint8_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT16):
        *((int16_t *)mem0) = *((int16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT16):
        *((uint16_t *)mem0) = *((uint16_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT32):
        *((int32_t *)mem0) = *((int32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT32):
        *((uint32_t *)mem0) = *((uint32_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_INT64):
        *((int64_t *)mem0) = *((int64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_UINT64):
        *((uint64_t *)mem0) = *((uint64_t *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_FLOAT):
        *((float *)mem0) = *((float *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_DOUBLE):
        *((double *)mem0) = *((double *)mem1);
        break;
      case VT_OPCODE(VT_OP_yyy,VT_BASETYPE_POINTER):
        *((void **)mem0) = *((void **)mem1);
        break;
#endif

////


      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_CHAR):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_INT8):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_UINT8):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_INT16):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_UINT16):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_INT32):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_UINT32):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_INT64):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_UINT64):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_FLOAT):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_DOUBLE):
      case VT_OPCODE(VT_OP_RETURN,VT_BASETYPE_POINTER):
        /* Terminate execution */
        return;
      default:
        printf( "VT EXEC: Runtime error, unsupported instruction! Opcode %d\n", op->code );
        return;
    }

  }

  free( stack );

  return;
}



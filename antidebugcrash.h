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

/*
#define ANTIDEBUG_NUMBER 000
*/

#ifndef ASM_ID_PREFIX
 #ifdef __APPLE__
  #define ASM_ID_PREFIX "_"
 #else
  #define ASM_ID_PREFIX ""
 #endif
#endif


#ifndef ANTIDEBUG_HASH
 #define ANTIDEBUG_HASH(x) (((x^0x9e1f)+((x>>2)^0x37a))^(((x<<3)+0x73fa)+((x^0x4b16)>>5))^((x<<1)+0xa153))
#endif


#define ANTIDEBUG_TMP_RANDOM ANTIDEBUG_HASH(ANTIDEBUG_NUMBER)
#define ANTIDEBUG_TMP_OFFSET (((ANTIDEBUG_TMP_RANDOM)>>1)&0xfff0)

extern void CC_CONCATENATE(antiDebugCrashTarget,ANTIDEBUG_NUMBER)() __asm__( ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugCrashTarget,ANTIDEBUG_NUMBER)) );

const uintptr_t CC_CONCATENATE(antiDebugCrashBase,ANTIDEBUG_NUMBER) __asm__( ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugCrashBase,ANTIDEBUG_NUMBER)) ) = ( (uintptr_t)&(CC_CONCATENATE(antiDebugCrashTarget,ANTIDEBUG_NUMBER)) ) - ANTIDEBUG_TMP_OFFSET;


#if ( ANTIDEBUG_TMP_RANDOM & 0x3 ) == 0x0
 #define ANTIDEBUG_TMP_OP0 "add"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x3 ) == 0x1
 #define ANTIDEBUG_TMP_OP0 "sub"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x3 ) == 0x2
 #define ANTIDEBUG_TMP_OP0 "and"
#else
 #define ANTIDEBUG_TMP_OP0 "or"
#endif

#if ( ANTIDEBUG_TMP_RANDOM & 0x30 ) == 0x00
 #define ANTIDEBUG_TMP_OP1 "add"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x30 ) == 0x10
 #define ANTIDEBUG_TMP_OP1 "sub"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x30 ) == 0x20
 #define ANTIDEBUG_TMP_OP1 "and"
#else
 #define ANTIDEBUG_TMP_OP1 "or"
#endif

#if ( ANTIDEBUG_TMP_RANDOM & 0x300 ) == 0x000
 #define ANTIDEBUG_TMP_REG0 "edx"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x300 ) == 0x100
 #define ANTIDEBUG_TMP_REG0 "esi"
#else
 #define ANTIDEBUG_TMP_REG0 "edi"
#endif

#define ANTIDEBUG_TMP_CRAP0 (((ANTIDEBUG_TMP_RANDOM)>>1)&0x3ff)
#define ANTIDEBUG_TMP_CRAP1 ((((ANTIDEBUG_TMP_RANDOM)>>5)&0xff)<<2)


#if CPUCONF_WORD_SIZE == 64

static inline uint32_t CC_CONCATENATE(antiDebugCrash,ANTIDEBUG_NUMBER)( uint32_t in0, uint32_t in1 )
{
  uint32_t out;
  __asm__ __volatile__(
    "movabsq " ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugCrashBase,ANTIDEBUG_NUMBER)) ", %%rax\n"
    "" ANTIDEBUG_TMP_OP0 "l $" CC_STRINGIFY(ANTIDEBUG_TMP_CRAP0) ", %%" ANTIDEBUG_TMP_REG0 "\n"
    "addq $" CC_STRINGIFY(ANTIDEBUG_TMP_OFFSET) ", %%rax\n"
    ".byte 0xf0\n"
    "" ANTIDEBUG_TMP_OP1 "l $" CC_STRINGIFY(ANTIDEBUG_TMP_CRAP1) ", %%ebx\n"
    "" ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugCrashTarget,ANTIDEBUG_NUMBER)) ":\n"
    : "=b" (out)
    : "0" (in0), "c" (in1)
    : "rax", ANTIDEBUG_TMP_REG0 );
  return out;
}

#elif CPUCONF_WORD_SIZE == 32

static inline uint32_t CC_CONCATENATE(antiDebugCrash,ANTIDEBUG_NUMBER)( uint32_t in0, uint32_t in1 )
{
  uint32_t out;
  __asm__ __volatile__(
    "movl " ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugCrashBase,ANTIDEBUG_NUMBER)) ", %%eax\n"
    "" ANTIDEBUG_TMP_OP0 "l $" CC_STRINGIFY(ANTIDEBUG_TMP_CRAP0) ", %%" ANTIDEBUG_TMP_REG0 "\n"
    "addl $" CC_STRINGIFY(ANTIDEBUG_TMP_OFFSET) ", %%eax\n"
    ".byte 0xf0\n"
    "" ANTIDEBUG_TMP_OP1 "l $" CC_STRINGIFY(ANTIDEBUG_TMP_CRAP1) ", %%ebx\n"
    "" ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugCrashTarget,ANTIDEBUG_NUMBER)) ":\n"
    : "=b" (out)
    : "0" (in0), "c" (in1)
    : "eax", ANTIDEBUG_TMP_REG0 );
  return out;
}

#else

 #error

#endif


#undef ANTIDEBUG_TMP_RANDOM
#undef ANTIDEBUG_TMP_OFFSET
#undef ANTIDEBUG_TMP_CRAP0
#undef ANTIDEBUG_TMP_CRAP1
#undef ANTIDEBUG_TMP_OP0
#undef ANTIDEBUG_TMP_OP1
#undef ANTIDEBUG_TMP_REG0

#undef ANTIDEBUG_NUMBER


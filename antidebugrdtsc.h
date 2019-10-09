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
#define ANTIDEBUG_TMP_OFFSET (((ANTIDEBUG_TMP_RANDOM)>>1)&0x1f0)

extern void CC_CONCATENATE(antiDebugRdTscTarget,ANTIDEBUG_NUMBER)() __asm__( ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugRdTscTarget,ANTIDEBUG_NUMBER)) );

const uintptr_t CC_CONCATENATE(antiDebugRdTscBase,ANTIDEBUG_NUMBER) __asm__( ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugRdTscBase,ANTIDEBUG_NUMBER)) ) = ( (uintptr_t)&(CC_CONCATENATE(antiDebugRdTscTarget,ANTIDEBUG_NUMBER)) ) - ANTIDEBUG_TMP_OFFSET - 2;


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
 #define ANTIDEBUG_TMP_REG0 "ebx"
 #define ANTIDEBUG_TMP_REG1 "esi"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x300 ) == 0x100
 #define ANTIDEBUG_TMP_REG0 "ecx"
 #define ANTIDEBUG_TMP_REG1 "edi"
#elif ( ANTIDEBUG_TMP_RANDOM & 0x300 ) == 0x100
 #define ANTIDEBUG_TMP_REG0 "esi"
 #define ANTIDEBUG_TMP_REG1 "ebx"
#else
 #define ANTIDEBUG_TMP_REG0 "edi"
 #define ANTIDEBUG_TMP_REG1 "ecx"
#endif

#define ANTIDEBUG_TMP_CRAP0 (((ANTIDEBUG_TMP_RANDOM)>>1)&0x3ff)
#define ANTIDEBUG_TMP_CRAP1 ((((ANTIDEBUG_TMP_RANDOM)>>5)&0xff)<<2)



static inline int64_t CC_CONCATENATE(antiDebugRdTsc,ANTIDEBUG_NUMBER)()
{
  uint32_t low, high;
  __asm__ __volatile__(
#if CPUCONF_WORD_SIZE == 64
    "movabsq " ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugRdTscBase,ANTIDEBUG_NUMBER)) ", %%rax\n"
#elif CPUCONF_WORD_SIZE == 32
    "movl " ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugRdTscBase,ANTIDEBUG_NUMBER)) ", %%eax\n"
#else
 #error
#endif
    "" ANTIDEBUG_TMP_OP0 "l $" CC_STRINGIFY(ANTIDEBUG_TMP_CRAP0) ", %%" ANTIDEBUG_TMP_REG0 "\n"
    "" ANTIDEBUG_TMP_OP1 "l $" CC_STRINGIFY(ANTIDEBUG_TMP_CRAP1) ", %%" ANTIDEBUG_TMP_REG1 "\n"
#if CPUCONF_WORD_SIZE == 64
    "addq $" CC_STRINGIFY(ANTIDEBUG_TMP_OFFSET) ", %%rax\n"
    "jmp *%%rax\n"
#elif CPUCONF_WORD_SIZE == 32
    "addl $" CC_STRINGIFY(ANTIDEBUG_TMP_OFFSET) ", %%eax\n"
    "jmp *%%eax\n"
#else
 #error
#endif
    "movl 0x310f4009, %%eax\n"
    ".globl " ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugRdTscTarget,ANTIDEBUG_NUMBER)) "\n"
    "" ASM_ID_PREFIX CC_STRINGIFY(CC_CONCATENATE(antiDebugRdTscTarget,ANTIDEBUG_NUMBER)) ":\n"
    : "=a" (low), "=d" (high)
    :
    : ANTIDEBUG_TMP_REG0, ANTIDEBUG_TMP_REG1
    );
  return ( (uint64_t)high << 32 ) | (uint64_t)low;
}



#undef ANTIDEBUG_TMP_RANDOM
#undef ANTIDEBUG_TMP_OFFSET
#undef ANTIDEBUG_TMP_CRAP0
#undef ANTIDEBUG_TMP_CRAP1
#undef ANTIDEBUG_TMP_OP0
#undef ANTIDEBUG_TMP_OP1
#undef ANTIDEBUG_TMP_REG0
#undef ANTIDEBUG_TMP_REG1

#undef ANTIDEBUG_NUMBER


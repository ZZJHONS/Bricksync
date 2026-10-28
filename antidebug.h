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

/* The anti-debug code is x86 only (rdtsc, x86 registers) and switched off (BS_ENABLE_ANTIDEBUG 0);
 * on other CPUs (Apple Silicon) it is left out so the build compiles. */
#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(__i386__) || defined(__i386) || defined(i386) || defined(_M_X64) || defined(_M_IX86)
 #define ANTIDEBUG_X86 (1)
#endif

#if ANTIDEBUG_X86
static inline int64_t antiDebugRdTsc1()
{
  uint32_t low, high;
  __asm__ __volatile__(
/*
    "rdtsc\n"
*/
    ".byte 0x0f, 0x31\n"
    :
    "=a" (low), "=d" (high)
    );
  return ( (uint64_t)high << 32 ) | (uint64_t)low;
}
#endif



////



void antiDebugHandleSIGILL();



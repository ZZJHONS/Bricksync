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

#ifndef CPUINFO_H
#define CPUINFO_H


typedef struct
{
  uint32_t endianness;
  uint32_t intellevel;
  uint32_t amdlevel;

  uint32_t arch;
  char vendorstring[12+1];
  char identifier[48+1];

  uint32_t vendor;
  uint32_t class;
  uint32_t family;
  uint32_t model;
  uint32_t cacheline;
  uint32_t clflushsize;

  int32_t cachelineL1code, cachesizeL1code, cacheassociativityL1code, cachesharedL1code;
  int32_t cachelineL1data, cachesizeL1data, cacheassociativityL1data, cachesharedL1data;
  int32_t cacheunifiedL1;
  int32_t cachelineL2, cachesizeL2, cacheassociativityL2, cachesharedL2;
  int32_t cachelineL3, cachesizeL3, cacheassociativityL3, cachesharedL3;

  int64_t sysmemory;
  uint32_t socketphysicalcores;
  uint32_t socketlogicalcores;
  uint32_t socketcount;
  uint32_t totalcorecount;
  uint32_t wordsize;
  uint32_t gpregs;
  uint32_t fpregs;


  /* CPU capabilities */
  unsigned int capfpu:1;
  unsigned int capcmov:1;
  unsigned int capclflush:1;
  unsigned int captsc:1;
  unsigned int capmmx:1;
  unsigned int capmmxext:1;
  unsigned int cap3dnow:1;
  unsigned int cap3dnowext:1;
  unsigned int capsse:1;
  unsigned int capsse2:1;
  unsigned int capsse3:1;
  unsigned int capssse3:1;
  unsigned int capsse4p1:1;
  unsigned int capsse4p2:1;
  unsigned int capsse4a:1;
  unsigned int capavx:1;
  unsigned int capavx2:1;
  unsigned int capxop:1;
  unsigned int capfma3:1;
  unsigned int capfma4:1;
  unsigned int capmisalignsse:1;
  unsigned int capavx512f:1;
  unsigned int capavx512dq:1;
  unsigned int capavx512pf:1;
  unsigned int capavx512er:1;
  unsigned int capavx512cd:1;
  unsigned int capavx512bw:1;
  unsigned int capavx512vl:1;
  unsigned int capaes:1;
  unsigned int capsha:1;
  unsigned int cappclmul;
  unsigned int caprdrnd:1;
  unsigned int caprdseed:1;
  unsigned int capcmpxchg16b:1;
  unsigned int cappopcnt:1;
  unsigned int caplzcnt:1;
  unsigned int capmovbe:1;
  unsigned int caprdtscp:1;
  unsigned int capconstanttsc:1;
  unsigned int capf16c:1;
  unsigned int capbmi:1;
  unsigned int capbmi2:1;
  unsigned int captbm:1;
  unsigned int capadx:1;
  unsigned int caphyperthreading:1;
  unsigned int capmwait:1;
  unsigned int caplongmode:1;
  unsigned int capthermalsensor:1;
  unsigned int capclockmodulation:1;
  /* CPU capabilities */

} cpuInfo;


void cpuGetInfo( cpuInfo *cpu );

void cpuGetInfoEnv( cpuInfo *cpu );


////


enum
{
  CPUINFO_LITTLE_ENDIAN,
  CPUINFO_BIG_ENDIAN,
  CPUINFO_MIXED_ENDIAN
};


enum
{
  CPUINFO_ARCH_AMD64,
  CPUINFO_ARCH_IA32,
  CPUINFO_ARCH_UNKNOWN
};


enum
{
  CPUINFO_VENDOR_AMD,
  CPUINFO_VENDOR_INTEL,
  CPUINFO_VENDOR_UNKNOWN
};


enum
{
  CPUINFO_CLASS_STEAMROLLER,
  CPUINFO_CLASS_JAGUAR,
  CPUINFO_CLASS_PILEDRIVER,
  CPUINFO_CLASS_BULLDOZER,
  CPUINFO_CLASS_BOBCAT,
  CPUINFO_CLASS_BARCELONA,
  CPUINFO_CLASS_ATHLON64SSE3,
  CPUINFO_CLASS_ATHLON64,
  CPUINFO_CLASS_ATHLON,
  CPUINFO_CLASS_K6_2,
  CPUINFO_CLASS_K6,

  CPUINFO_CLASS_COREI7AVX2,
  CPUINFO_CLASS_COREI7AVX,
  CPUINFO_CLASS_COREI7,
  CPUINFO_CLASS_CORE2,
  CPUINFO_CLASS_NOCONA,
  CPUINFO_CLASS_PRESCOTT,
  CPUINFO_CLASS_PENTIUM4,
  CPUINFO_CLASS_PENTIUM3,
  CPUINFO_CLASS_PENTIUM2,

  CPUINFO_CLASS_I686,
  CPUINFO_CLASS_I586,

  CPUINFO_CLASS_AMD64GENERIC,
  CPUINFO_CLASS_IA32GENERIC,
  CPUINFO_CLASS_UNKNOWN,

  CPUINFO_CLASS_COUNT
};



#endif /* CPUINFO_H */



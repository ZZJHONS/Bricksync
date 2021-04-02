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
#include <time.h>
#include <limits.h>


#define DEFAULT_HEADER_TEMP_NAME "cpuconfig.temp"
#define DEFAULT_HEADER_NAME "cpuconfig.h"
#define DEFAULT_MAKEFILE_NAME "Makefile.cpuconf"
#define DEFAULT_TARGET_NAME "Makefile"


#include "cpuinfo.h"


////


static const char *cpuClassName[CPUINFO_CLASS_COUNT] =
{
 [CPUINFO_CLASS_STEAMROLLER] = "STEAMROLLER",
 [CPUINFO_CLASS_JAGUAR] = "JAGUAR",
 [CPUINFO_CLASS_PILEDRIVER] = "PILEDRIVER",
 [CPUINFO_CLASS_BULLDOZER] = "BULLDOZER",
 [CPUINFO_CLASS_BOBCAT] = "BOBCAT",
 [CPUINFO_CLASS_BARCELONA] = "BARCELONA",
 [CPUINFO_CLASS_ATHLON64SSE3] = "ATHLON64SSE3",
 [CPUINFO_CLASS_ATHLON64] = "ATHLON64",
 [CPUINFO_CLASS_ATHLON] = "ATHLON",
 [CPUINFO_CLASS_K6_2] = "K6-2",
 [CPUINFO_CLASS_K6] = "K6",
 [CPUINFO_CLASS_COREI7AVX2] = "COREI7AVX2",
 [CPUINFO_CLASS_COREI7AVX] = "COREI7AVX",
 [CPUINFO_CLASS_COREI7] = "COREI7",
 [CPUINFO_CLASS_CORE2] = "CORE2",
 [CPUINFO_CLASS_NOCONA] = "NOCONA",
 [CPUINFO_CLASS_PRESCOTT] = "PRESCOTT",
 [CPUINFO_CLASS_PENTIUM4] = "PENTIUM4",
 [CPUINFO_CLASS_PENTIUM3] = "PENTIUM3",
 [CPUINFO_CLASS_PENTIUM2] = "PENTIUM2",
 [CPUINFO_CLASS_I686] = "I686",
 [CPUINFO_CLASS_I586] = "I586",
 [CPUINFO_CLASS_AMD64GENERIC] = "AMD64GENERIC",
 [CPUINFO_CLASS_IA32GENERIC] = "IA32GENERIC",
 [CPUINFO_CLASS_UNKNOWN] = 0
};


static intptr_t getLog2( intptr_t n )
{
  intptr_t i, j;
  for( i = 1, j = 0 ; i < n ; i <<= 1, j++ );
  return j;
}


void writeCpuInfoHeader( FILE *file, cpuInfo *cpu )
{
  fprintf( file, "#define CPUCONF_CHAR_SIZE (%d)\n", (int)sizeof(char) );
  fprintf( file, "#define CPUCONF_SHORT_SIZE (%d)\n", (int)sizeof(short) );
  fprintf( file, "#define CPUCONF_INT_SIZE (%d)\n", (int)sizeof(int) );
  fprintf( file, "#define CPUCONF_LONG_SIZE (%d)\n", (int)sizeof(long) );
  fprintf( file, "#define CPUCONF_LONG_LONG_SIZE (%d)\n", (int)sizeof(long long) );
  fprintf( file, "#define CPUCONF_INTPTR_SIZE (%d)\n", (int)sizeof(intptr_t) );
  fprintf( file, "#define CPUCONF_POINTER_SIZE (%d)\n", (int)sizeof(void *) );
  fprintf( file, "#define CPUCONF_FLOAT_SIZE (%d)\n", (int)sizeof(float) );
  fprintf( file, "#define CPUCONF_DOUBLE_SIZE (%d)\n", (int)sizeof(double) );
  fprintf( file, "#define CPUCONF_LONG_DOUBLE_SIZE (%d)\n", (int)sizeof(long double) );
  fprintf( file, "\n" );

  fprintf( file, "#define CPUCONF_CHAR_BITS (%d)\n", (int)sizeof(char)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_SHORT_BITS (%d)\n", (int)sizeof(short)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_INT_BITS (%d)\n", (int)sizeof(int)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_LONG_BITS (%d)\n", (int)sizeof(long)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_LONG_LONG_BITS (%d)\n", (int)sizeof(long long)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_INTPTR_BITS (%d)\n", (int)sizeof(intptr_t)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_POINTER_BITS (%d)\n", (int)sizeof(void *)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_FLOAT_BITS (%d)\n", (int)sizeof(float)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_DOUBLE_BITS (%d)\n", (int)sizeof(double)*CHAR_BIT );
  fprintf( file, "#define CPUCONF_LONG_DOUBLE_BITS (%d)\n", (int)sizeof(long double)*CHAR_BIT );
  fprintf( file, "\n" );

  fprintf( file, "#define CPUCONF_CHAR_SIZESHIFT (%d)\n", (int)getLog2( sizeof(char) ) );
  fprintf( file, "#define CPUCONF_SHORT_SIZESHIFT (%d)\n", (int)getLog2( sizeof(short) ) );
  fprintf( file, "#define CPUCONF_INT_SIZESHIFT (%d)\n", (int)getLog2( sizeof(int) ) );
  fprintf( file, "#define CPUCONF_LONG_SIZESHIFT (%d)\n", (int)getLog2( sizeof(long) ) );
  fprintf( file, "#define CPUCONF_LONG_LONG_SIZESHIFT (%d)\n", (int)getLog2( sizeof(long long) ) );
  fprintf( file, "#define CPUCONF_INTPTR_SIZESHIFT (%d)\n", (int)getLog2( sizeof(intptr_t) ) );
  fprintf( file, "#define CPUCONF_POINTER_SIZESHIFT (%d)\n", (int)getLog2( sizeof(void *) ) );
  fprintf( file, "#define CPUCONF_FLOAT_SIZESHIFT (%d)\n", (int)getLog2( sizeof(float) ) );
  fprintf( file, "#define CPUCONF_DOUBLE_SIZESHIFT (%d)\n", (int)getLog2( sizeof(double) ) );
  fprintf( file, "#define CPUCONF_LONG_DOUBLE_SIZESHIFT (%d)\n", (int)getLog2( sizeof(long double) ) );
  fprintf( file, "\n" );

  fprintf( file, "#define CPUCONF_CHAR_BITSHIFT (%d)\n", (int)getLog2( sizeof(char)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_SHORT_BITSHIFT (%d)\n", (int)getLog2( sizeof(short)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_INT_BITSHIFT (%d)\n", (int)getLog2( sizeof(int)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_LONG_BITSHIFT (%d)\n", (int)getLog2( sizeof(long)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_LONG_LONG_BITSHIFT (%d)\n", (int)getLog2( sizeof(long long)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_INTPTR_BITSHIFT (%d)\n", (int)getLog2( sizeof(intptr_t)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_POINTER_BITSHIFT (%d)\n", (int)getLog2( sizeof(void *)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_FLOAT_BITSHIFT (%d)\n", (int)getLog2( sizeof(float)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_DOUBLE_BITSHIFT (%d)\n", (int)getLog2( sizeof(double)*CHAR_BIT ) );
  fprintf( file, "#define CPUCONF_LONG_DOUBLE_BITSHIFT (%d)\n", (int)getLog2( sizeof(long double)*CHAR_BIT ) );
  fprintf( file, "\n" );

  switch( cpu->endianness )
  {
    case CPUINFO_LITTLE_ENDIAN:
      fprintf( file, "#define CPUCONF_LITTLE_ENDIAN\n" );
      break;
    case CPUINFO_BIG_ENDIAN:
      fprintf( file, "#define CPUCONF_BIG_ENDIAN\n" );
      break;
    case CPUINFO_MIXED_ENDIAN:
      fprintf( file, "#define CPUCONF_MIXED_ENDIAN\n" );
    default:
      break;
  }

  switch( cpu->arch )
  {
    case CPUINFO_ARCH_AMD64:
      fprintf( file, "#define CPUCONF_ARCH_AMD64\n" );
      break;
    case CPUINFO_ARCH_IA32:
      fprintf( file, "#define CPUCONF_ARCH_IA32\n" );
      break;
    case CPUINFO_ARCH_UNKNOWN:
    default:
      break;
  }

  if( cpu->vendor == CPUINFO_VENDOR_INTEL )
    fprintf( file, "#define CPUCONF_VENDOR_INTEL\n" );
  else if( cpu->vendor == CPUINFO_VENDOR_AMD )
    fprintf( file, "#define CPUCONF_VENDOR_AMD\n" );

  if( cpu->identifier[0] )
    fprintf( file, "#define CPUCONF_IDENTIFIER \"%s\"\n", cpu->identifier );
  if( cpuClassName[cpu->class] )
    fprintf( file, "#define CPUCONF_CLASS_%s\n", cpuClassName[cpu->class] );
  if( cpu->socketlogicalcores )
    fprintf( file, "#define CPUCONF_SOCKET_LOGICAL_CORES (%d)\n", cpu->socketlogicalcores );
  if( cpu->socketphysicalcores )
    fprintf( file, "#define CPUCONF_SOCKET_PHYSICAL_CORES (%d)\n", cpu->socketphysicalcores );
  if( cpu->socketcount )
    fprintf( file, "#define CPUCONF_SOCKET_COUNT (%d)\n", cpu->socketcount );
  if( cpu->totalcorecount )
    fprintf( file, "#define CPUCONF_TOTAL_CORE_COUNT (%d)\n", cpu->totalcorecount );
  if( cpu->sysmemory )
    fprintf( file, "#define CPUCONF_SYSTEM_MEMORY (%lldLL)\n", (long long)cpu->sysmemory );
  if( cpu->wordsize )
    fprintf( file, "#define CPUCONF_WORD_SIZE (%d)\n", cpu->wordsize );
  if( cpu->cacheline )
    fprintf( file, "#define CPUCONF_CACHE_LINE_SIZE (%d)\n", cpu->cacheline ? cpu->cacheline : 32 );
  if( cpu->cachesizeL1code > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1CODE_SIZE (%d)\n", cpu->cachesizeL1code * 1024 );
  if( cpu->cachelineL1code > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1CODE_LINE (%d)\n", cpu->cachelineL1code );
  if( cpu->cacheassociativityL1code > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1CODE_ASSOCIATIVITY (%d)\n", cpu->cacheassociativityL1code );
  if( cpu->cachesharedL1code > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1CODE_SHARED (%d)\n", cpu->cachesharedL1code );
  if( cpu->cachesizeL1data > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1DATA_SIZE (%d)\n", cpu->cachesizeL1data * 1024 );
  if( cpu->cachelineL1data > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1DATA_LINE (%d)\n", cpu->cachelineL1data );
  if( cpu->cacheassociativityL1data > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1DATA_ASSOCIATIVITY (%d)\n", cpu->cacheassociativityL1data );
  if( cpu->cachesharedL1data > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L1DATA_SHARED (%d)\n", cpu->cachesharedL1data );
  if( ( cpu->cachesizeL1code > 0 ) && ( cpu->cachesizeL1data > 0 ) )
    fprintf( file, "#define CPUCONF_CACHE_L1_UNIFIED_FLAG (%d)\n", cpu->cacheunifiedL1 );
  if( cpu->cachesizeL2 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L2_SIZE (%d)\n", cpu->cachesizeL2 * 1024 );
  if( cpu->cachelineL2 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L2_LINE (%d)\n", cpu->cachelineL2 );
  if( cpu->cacheassociativityL2 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L2_ASSOCIATIVITY (%d)\n", cpu->cacheassociativityL2 );
  if( cpu->cachesharedL2 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L2_SHARED (%d)\n", cpu->cachesharedL2 );
  if( cpu->cachesizeL3 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L3_SIZE (%d)\n", cpu->cachesizeL3 * 1024 );
  if( cpu->cachelineL3 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L3_LINE (%d)\n", cpu->cachelineL3 );
  if( cpu->cacheassociativityL3 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L3_ASSOCIATIVITY (%d)\n", cpu->cacheassociativityL3 );
  if( cpu->cachesharedL3 > 0 )
    fprintf( file, "#define CPUCONF_CACHE_L3_SHARED (%d)\n", cpu->cachesharedL3 );
  fprintf( file, "#define CPUCONF_CAP_GPREGS (%d)\n", cpu->gpregs ? cpu->gpregs : 7 );
  fprintf( file, "#define CPUCONF_CAP_FPREGS (%d)\n", cpu->fpregs ? cpu->fpregs : 8 );
  fprintf( file, "\n" );

  if( cpu->capcmov )
    fprintf( file, "#define CPUCONF_CAP_CMOV\n" );
  if( cpu->capclflush )
    fprintf( file, "#define CPUCONF_CAP_CLFLUSH\n" );
  if( cpu->captsc )
    fprintf( file, "#define CPUCONF_CAP_TSC\n" );
  if( cpu->capmmx )
    fprintf( file, "#define CPUCONF_CAP_MMX\n" );
  if( cpu->capmmxext )
    fprintf( file, "#define CPUCONF_CAP_MMXEXT\n" );
  if( cpu->cap3dnow )
    fprintf( file, "#define CPUCONF_CAP_3DNOW\n" );
  if( cpu->cap3dnowext )
    fprintf( file, "#define CPUCONF_CAP_3DNOWEXT\n" );
  if( cpu->capsse )
    fprintf( file, "#define CPUCONF_CAP_SSE\n" );
  if( cpu->capsse2 )
    fprintf( file, "#define CPUCONF_CAP_SSE2\n" );
  if( cpu->capsse3 )
    fprintf( file, "#define CPUCONF_CAP_SSE3\n" );
  if( cpu->capssse3 )
    fprintf( file, "#define CPUCONF_CAP_SSSE3\n" );
  if( cpu->capsse4p1 )
    fprintf( file, "#define CPUCONF_CAP_SSE4_1\n" );
  if( cpu->capsse4p2 )
    fprintf( file, "#define CPUCONF_CAP_SSE4_2\n" );
  if( cpu->capsse4a )
    fprintf( file, "#define CPUCONF_CAP_SSE4A\n" );
  if( cpu->capavx )
    fprintf( file, "#define CPUCONF_CAP_AVX\n" );
  if( cpu->capavx2 )
    fprintf( file, "#define CPUCONF_CAP_AVX2\n" );
  if( cpu->capxop )
    fprintf( file, "#define CPUCONF_CAP_XOP\n" );
  if( cpu->capfma3 )
    fprintf( file, "#define CPUCONF_CAP_FMA3\n" );
  if( cpu->capfma4 )
    fprintf( file, "#define CPUCONF_CAP_FMA4\n" );
  if( cpu->capmisalignsse )
    fprintf( file, "#define CPUCONF_CAP_MISALIGNSSE\n" );
  if( cpu->capavx512f )
    fprintf( file, "#define CPUCONF_CAP_AVX512F\n" );
  if( cpu->capavx512dq )
    fprintf( file, "#define CPUCONF_CAP_AVX512DQ\n" );
  if( cpu->capavx512pf )
    fprintf( file, "#define CPUCONF_CAP_AVX512PF\n" );
  if( cpu->capavx512er )
    fprintf( file, "#define CPUCONF_CAP_AVX512ER\n" );
  if( cpu->capavx512cd )
    fprintf( file, "#define CPUCONF_CAP_AVX512CD\n" );
  if( cpu->capavx512bw )
    fprintf( file, "#define CPUCONF_CAP_AVX512BW\n" );
  if( cpu->capavx512vl )
    fprintf( file, "#define CPUCONF_CAP_AVX512VL\n" );
  if( cpu->capaes )
    fprintf( file, "#define CPUCONF_CAP_AES\n" );
  if( cpu->capsha )
    fprintf( file, "#define CPUCONF_CAP_SHA\n" );
  if( cpu->cappclmul )
    fprintf( file, "#define CPUCONF_CAP_PCLMUL\n" );
  if( cpu->caprdrnd )
    fprintf( file, "#define CPUCONF_CAP_RDRND\n" );
  if( cpu->caprdseed )
    fprintf( file, "#define CPUCONF_CAP_RDSEED\n" );
  if( cpu->capcmpxchg16b )
    fprintf( file, "#define CPUCONF_CAP_CMPXCHG16B\n" );
  if( cpu->cappopcnt )
    fprintf( file, "#define CPUCONF_CAP_POPCNT\n" );
  if( cpu->caplzcnt )
    fprintf( file, "#define CPUCONF_CAP_LZCNT\n" );
  if( cpu->capmovbe )
    fprintf( file, "#define CPUCONF_CAP_MOVBE\n" );
  if( cpu->caprdtscp )
    fprintf( file, "#define CPUCONF_CAP_RDTSCP\n" );
  if( cpu->capconstanttsc )
    fprintf( file, "#define CPUCONF_CAP_CONSTANTTSC\n" );
  if( cpu->capf16c )
    fprintf( file, "#define CPUCONF_CAP_F16c\n" );
  if( cpu->capbmi )
    fprintf( file, "#define CPUCONF_CAP_BMI\n" );
  if( cpu->capbmi2 )
    fprintf( file, "#define CPUCONF_CAP_BMI2\n" );
  if( cpu->captbm )
    fprintf( file, "#define CPUCONF_CAP_TBM\n" );
  if( cpu->capadx )
    fprintf( file, "#define CPUCONF_CAP_ADX\n" );
  if( cpu->caphyperthreading )
    fprintf( file, "#define CPUCONF_CAP_HYPERTHREADING\n" );
  if( cpu->capmwait )
    fprintf( file, "#define CPUCONF_CAP_MWAIT\n" );
  if( cpu->capthermalsensor )
    fprintf( file, "#define CPUCONF_CAP_THERMALSENSOR\n" );
  if( cpu->capclockmodulation )
    fprintf( file, "#define CPUCONF_CAP_CLOCKMODULATION\n" );
  fprintf( file, "\n" );

/*
  char timebuf[256];
  time_t curtime;
  curtime = time( 0 );
  strftime( timebuf, 256, "%b %d %Y", localtime( &curtime ) );
  fprintf( file, "#define ENVCONF_DATE_STRING \"%s\"\n", timebuf );
  fprintf( file, "\n" );
*/

  return;
}


int writeMakefile( char *makefilename, char *targetname, cpuInfo *cpu, char *makeopts, char *ccoptions )
{
  FILE *file;
  if( !( file = fopen( makefilename, "w" ) ) )
    return 0;
  fprintf( file, "CFLAGS = %s %s\n\n", makeopts, ccoptions );
  fprintf( file, "all :\n" );
  fprintf( file, "\t@make -e -f %s CFLAGS=\"$(CFLAGS)\"\n\n", targetname );
  fclose( file );
  return 1;
}


////


#define CPUINFO_PRINT_ARCH (0x1)
#define CPUINFO_PRINT_VENDOR (0x2)
#define CPUINFO_PRINT_IDENTIFIER (0x4)
#define CPUINFO_PRINT_FAMILY (0x8)
#define CPUINFO_PRINT_MODEL (0x10)
#define CPUINFO_PRINT_CACHELINE (0x20)
#define CPUINFO_PRINT_CORES (0x40)
#define CPUINFO_PRINT_WORDSIZE (0x80)
#define CPUINFO_PRINT_GPREGS (0x100)
#define CPUINFO_PRINT_FPREGS (0x200)
#define CPUINFO_PRINT_MEMORY (0x400)
#define CPUINFO_PRINT_CAPS (0x800)
#define CPUINFO_PRINT_CACHE (0x1000)
#define CPUINFO_PRINT_CCOPTIONS (0x2000)
#define CPUINFO_PRINT_ALL (0xffff)


void cpuPrint( cpuInfo *cpu, int printflags, char *cctarget, char *ccoptions )
{
  if( printflags & CPUINFO_PRINT_ARCH )
  {
    switch( cpu->arch )
    {
      case CPUINFO_ARCH_AMD64:
        printf( "Arch : AMD64\n" );
        break;
      case CPUINFO_ARCH_IA32:
        printf( "Arch : IA32\n" );
        break;
      case CPUINFO_ARCH_UNKNOWN:
      default:
        break;
    }
  }
  if( printflags & CPUINFO_PRINT_VENDOR )
  {
    if( cpu->vendorstring[0] )
      printf( "Vendor : %s\n", cpu->vendorstring );
  }
  if( printflags & CPUINFO_PRINT_IDENTIFIER )
  {
    if( cpu->identifier[0] )
      printf( "Identifier : %s\n", cpu->identifier );
  }
  if( printflags & CPUINFO_PRINT_FAMILY )
  {
    if( cpu->family )
      printf( "Family : %d\n", cpu->family );
  }
  if( printflags & CPUINFO_PRINT_MODEL )
  {
    if( cpu->model )
      printf( "Model  : %d\n", cpu->model );
  }
  if( printflags & CPUINFO_PRINT_CACHELINE )
  {
    if( cpu->cacheline )
      printf( "Cache Line Size  : %d bytes\n", cpu->cacheline );
    if( ( cpu->capclflush ) &&( cpu->clflushsize ) )
      printf( "Cache Flush Size : %d bytes\n", cpu->clflushsize );
  }
  if( printflags & CPUINFO_PRINT_CORES )
  {
    printf( "Processor Layout\n" );
    if( cpu->socketlogicalcores )
      printf( "  Socket Logical Cores  : %d\n", cpu->socketlogicalcores );
    if( cpu->socketphysicalcores )
      printf( "  Socket Physical Cores : %d\n", cpu->socketphysicalcores );
    if( cpu->socketcount )
      printf( "  Socket Count          : %d\n", cpu->socketcount );
    if( cpu->totalcorecount )
      printf( "  Total Cores Count     : %d\n", cpu->totalcorecount );
  }
  if( printflags & CPUINFO_PRINT_WORDSIZE )
  {
    if( cpu->wordsize )
      printf( "Word Size    : %d bits\n", cpu->wordsize );
  }
  if( printflags & CPUINFO_PRINT_GPREGS )
  {
    if( cpu->gpregs )
      printf( "GP Registers : %d\n", cpu->gpregs );
  }
  if( printflags & CPUINFO_PRINT_FPREGS )
  {
    if( cpu->fpregs )
      printf( "FP Registers : %d\n", cpu->fpregs );
  }
  if( printflags & CPUINFO_PRINT_MEMORY )
  {
    if( cpu->sysmemory )
    {
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
      printf( "User-Space Memory : %I64d bytes ( %.2fMb )\n", (long long)cpu->sysmemory, (double)cpu->sysmemory / 1048576.0 );
#else
      printf( "User-Space Memory : %lld bytes ( %.2fMb )\n", (long long)cpu->sysmemory, (double)cpu->sysmemory / 1048576.0 );
#endif
    }
  }
  if( printflags & CPUINFO_PRINT_CAPS )
  {
    printf( "Capabilities :" );
    if( cpu->capcmov )
      printf( " CMOV" );
    if( cpu->capclflush )
      printf( " CLFLUSH" );
    if( cpu->captsc )
      printf( " TSC" );
    if( cpu->capmmx )
      printf( " MMX" );
    if( cpu->capmmxext )
      printf( " MMXEXT" );
    if( cpu->cap3dnow )
      printf( " 3DNow" );
    if( cpu->cap3dnowext )
      printf( " 3DNowExt" );
    if( cpu->capsse )
      printf( " SSE" );
    if( cpu->capsse2 )
      printf( " SSE2" );
    if( cpu->capsse3 )
      printf( " SSE3" );
    if( cpu->capssse3 )
      printf( " SSSE3" );
    if( cpu->capsse4p1 )
      printf( " SSE4.1" );
    if( cpu->capsse4p2 )
      printf( " SSE4.2" );
    if( cpu->capsse4a )
      printf( " SSE4A" );
    if( cpu->capavx )
      printf( " AVX" );
    if( cpu->capavx2 )
      printf( " AVX2" );
    if( cpu->capxop )
      printf( " XOP" );
    if( cpu->capfma3 )
      printf( " FMA3" );
    if( cpu->capfma4 )
      printf( " FMA4" );
    if( cpu->capmisalignsse )
      printf( " MisalignSSE" );
    if( cpu->capavx512f )
      printf( " AVX512F" );
    if( cpu->capavx512dq )
      printf( " AVX512DQ" );
    if( cpu->capavx512pf )
      printf( " AVX512PF" );
    if( cpu->capavx512er )
      printf( " AVX512ER" );
    if( cpu->capavx512cd )
      printf( " AVX512CD" );
    if( cpu->capavx512bw )
      printf( " AVX512BW" );
    if( cpu->capavx512vl )
      printf( " AVX512VL" );
    if( cpu->capaes )
      printf( " AES" );
    if( cpu->capsha )
      printf( " SHA" );
    if( cpu->cappclmul )
      printf( " PCLMUL" );
    if( cpu->caprdrnd )
      printf( " RDRND" );
    if( cpu->caprdseed )
      printf( " RDSEED" );
    if( cpu->capcmpxchg16b )
      printf( " CMPXCHG16B" );
    if( cpu->cappopcnt )
      printf( " POPCNT" );
    if( cpu->caplzcnt )
      printf( " LZCNT" );
    if( cpu->capmovbe )
      printf( " MOVBE" );
    if( cpu->caprdtscp )
      printf( " RDTSCP" );
    if( cpu->capconstanttsc )
      printf( " ConstantTSC" );
    if( cpu->capf16c )
      printf( " F16C" );
    if( cpu->capbmi )
      printf( " BMI" );
    if( cpu->capbmi2 )
      printf( " BMI2" );
    if( cpu->capbmi2 )
      printf( " TBM" );
    if( cpu->capadx )
      printf( " ADX" );
    if( cpu->caphyperthreading )
      printf( " HyperThreading" );
    if( cpu->capmwait )
      printf( " MWAIT" );
    if( cpu->caplongmode )
      printf( " LongMode" );
    if( cpu->capthermalsensor )
      printf( " ThermalSensor" );
    if( cpu->capclockmodulation )
      printf( " ClockModulation" );
    printf( "\n" );
  }

  if( printflags & CPUINFO_PRINT_CACHE )
  {
    if( ( cpu->cachesizeL1code > 0 ) && ( cpu->cachesizeL1data > 0 ) && ( cpu->cacheunifiedL1 ) )
    {
      printf( "L1 Unified Cache Memory\n" );
      printf( "  Cache Size    : %d kb\n", cpu->cachesizeL1code ); 
      printf( "  Line Size     : %d bytes\n", cpu->cachelineL1code );
      printf( "  Associativity : %d way(s)\n", cpu->cacheassociativityL1code );
      printf( "  Shared        : %d core(s)\n", cpu->cachesharedL1code );
    }
    else
    {
      if( cpu->cachesizeL1code > 0 )
      {
        printf( "L1 Code Cache Memory\n" );
        printf( "  Cache Size    : %d kb\n", cpu->cachesizeL1code ); 
        printf( "  Line Size     : %d bytes\n", cpu->cachelineL1code );
        printf( "  Associativity : %d way(s)\n", cpu->cacheassociativityL1code );
        printf( "  Shared        : %d core(s)\n", cpu->cachesharedL1code );
      }
      if( cpu->cachesizeL1data > 0  )
      {
        printf( "L1 Data Cache Memory\n" );
        printf( "  Cache Size    : %d kb\n", cpu->cachesizeL1data ); 
        printf( "  Line Size     : %d bytes\n", cpu->cachelineL1data );
        printf( "  Associativity : %d way(s)\n", cpu->cacheassociativityL1data );
        printf( "  Shared        : %d core(s)\n", cpu->cachesharedL1data );
      }
    }
    if( cpu->cachesizeL2 > 0 )
    {
      printf( "L2 Cache Memory\n" );
      printf( "  Cache Size    : %d kb\n", cpu->cachesizeL2 ); 
      printf( "  Line Size     : %d bytes\n", cpu->cachelineL2 );
      printf( "  Associativity : %d way(s)\n", cpu->cacheassociativityL2 );
      printf( "  Shared        : %d core(s)\n", cpu->cachesharedL2 );
    }
    if( cpu->cachesizeL3 > 0 )
    {
      printf( "L3 Cache Memory\n" );
      printf( "  Cache Size    : %d kb\n", cpu->cachesizeL3 ); 
      printf( "  Line Size     : %d bytes\n", cpu->cachelineL3 );
      printf( "  Associativity : %d way(s)\n", cpu->cacheassociativityL3 );
      printf( "  Shared        : %d core(s)\n", cpu->cachesharedL3 );
    }
  }

  if( printflags & CPUINFO_PRINT_CCOPTIONS )
  {
    if( cctarget[0] )
      printf( "Target Compiler : %s\n", cctarget );
    if( ccoptions[0] )
      printf( "Recommended CC Options : %s\n", ccoptions );
#ifdef __VERSION__
    printf( "Compiler Version : %s\n", __VERSION__ );
#endif
  }

  return;
}


////


typedef struct
{
  char *ccoptions;
  int majorv;
  int minorv;
  void *fallback;
} gccTuneClass;

static gccTuneClass gccTuneTable[] =
{
  [CPUINFO_CLASS_STEAMROLLER] = { "-march=bdver3", 4, 8, &gccTuneTable[CPUINFO_CLASS_PILEDRIVER] },
  [CPUINFO_CLASS_JAGUAR] = { "-march=btver2", 4, 8, &gccTuneTable[CPUINFO_CLASS_BOBCAT] },
  [CPUINFO_CLASS_PILEDRIVER] = { "-march=bdver2", 4, 7, &gccTuneTable[CPUINFO_CLASS_BULLDOZER] },
  [CPUINFO_CLASS_BULLDOZER] = { "-march=bdver1", 4, 6, &gccTuneTable[CPUINFO_CLASS_BARCELONA] },
  [CPUINFO_CLASS_BOBCAT] = { "-march=btver1", 4, 6, &gccTuneTable[CPUINFO_CLASS_ATHLON64SSE3] },
  [CPUINFO_CLASS_BARCELONA] = { "-march=barcelona", 4, 3, &gccTuneTable[CPUINFO_CLASS_ATHLON64SSE3] },
  [CPUINFO_CLASS_ATHLON64SSE3] = { "-march=k8", 3, 4, &gccTuneTable[CPUINFO_CLASS_ATHLON64] },
  [CPUINFO_CLASS_ATHLON64] = { "-march=k8", 3, 4, &gccTuneTable[CPUINFO_CLASS_ATHLON] },
  [CPUINFO_CLASS_ATHLON] = { "-march=athlon", 3, 1, &gccTuneTable[CPUINFO_CLASS_K6_2] },
  [CPUINFO_CLASS_K6_2] = { "-march=k6-2", 3, 1, &gccTuneTable[CPUINFO_CLASS_K6] },
  [CPUINFO_CLASS_K6] = { "-march=k6", 3, 1, &gccTuneTable[CPUINFO_CLASS_I586] },
  [CPUINFO_CLASS_COREI7AVX2] = { "-march=core-avx2", 4, 7, &gccTuneTable[CPUINFO_CLASS_COREI7AVX] },
  [CPUINFO_CLASS_COREI7AVX] = { "-march=corei7-avx", 4, 6, &gccTuneTable[CPUINFO_CLASS_COREI7] },
  [CPUINFO_CLASS_COREI7] = { "-march=corei7", 4, 6, &gccTuneTable[CPUINFO_CLASS_CORE2] },
  [CPUINFO_CLASS_CORE2] = { "-march=core2", 4, 6, &gccTuneTable[CPUINFO_CLASS_NOCONA] },
  [CPUINFO_CLASS_NOCONA] = { "-march=nocona", 3, 4, &gccTuneTable[CPUINFO_CLASS_PRESCOTT] },
  [CPUINFO_CLASS_PRESCOTT] = { "-march=prescott", 3, 4, &gccTuneTable[CPUINFO_CLASS_PENTIUM4] },
  [CPUINFO_CLASS_PENTIUM4] = { "-march=pentium4", 3, 1, &gccTuneTable[CPUINFO_CLASS_PENTIUM3] },
  [CPUINFO_CLASS_PENTIUM3] = { "-march=pentium3", 3, 1, &gccTuneTable[CPUINFO_CLASS_PENTIUM2] },
  [CPUINFO_CLASS_PENTIUM2] = { "-march=pentium2", 3, 1, &gccTuneTable[CPUINFO_CLASS_I686] },
  [CPUINFO_CLASS_I686] = { "-march=i686", 2, 0, &gccTuneTable[CPUINFO_CLASS_I586] },
  [CPUINFO_CLASS_I586] = { "-march=i586", 2, 0, &gccTuneTable[CPUINFO_CLASS_UNKNOWN] },
  [CPUINFO_CLASS_AMD64GENERIC] = { "", 0, 0, &gccTuneTable[CPUINFO_CLASS_UNKNOWN] },
  [CPUINFO_CLASS_IA32GENERIC] = { "", 0, 0, &gccTuneTable[CPUINFO_CLASS_UNKNOWN] },
  [CPUINFO_CLASS_UNKNOWN] = { "", 0, 0, &gccTuneTable[CPUINFO_CLASS_UNKNOWN] }
};


static void cpuFindCcOptions( cpuInfo *cpu, char *cctarget, char *ccoptions )
{
#if defined(__GNUC__)
  char *s;
  int gccmajorv, gccminorv;
  int gccversion;
  int reqversion;
  int optversion;
  gccTuneClass *class;

  class = &gccTuneTable[ cpu->class ];

  gccmajorv = __GNUC__;
  gccminorv = 0;
 #ifdef __GNUC_MINOR__
  gccminorv = __GNUC_MINOR__;
 #endif
  gccversion = ( gccmajorv << 8 ) + gccminorv;
  reqversion = ( class->majorv << 8 ) + class->minorv;

  s = ccoptions;
  s += sprintf( s, "%s", class->ccoptions );
  if( cpu->capmmx )
    s += sprintf( s, " -mmmx" );
  if( cpu->cap3dnow )
    s += sprintf( s, " -m3dnow" );
  if( cpu->capsse )
    s += sprintf( s, " -msse" );
  if( cpu->capsse2 )
    s += sprintf( s, " -msse2" );
  if( cpu->capsse3 )
    s += sprintf( s, " -msse3" );
  if( cpu->capssse3 )
  {
    optversion = ( 4 << 8 ) + 3;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
      s += sprintf( s, " -mssse3" );
  }
  if( cpu->capsse4p1 )
  {
    optversion = ( 4 << 8 ) + 3;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
      s += sprintf( s, " -msse4.1" );
  }
  if( cpu->capsse4p2 )
  {
    optversion = ( 4 << 8 ) + 3;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -msse4.2" );
  }
  if( cpu->capsse4a )
  {
    optversion = ( 4 << 8 ) + 3;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -msse4a" );
  }
  if( cpu->capavx )
  {
    optversion = ( 4 << 8 ) + 4;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mavx" );
  }
  if( cpu->capavx2 )
  {
    optversion = ( 4 << 8 ) + 7;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mavx2" );
  }
  if( cpu->capxop )
  {
    optversion = ( 4 << 8 ) + 6;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mxop" );
  }
  if( cpu->capfma3 )
  {
    optversion = ( 4 << 8 ) + 7;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mfma" );
  }
  if( cpu->capfma4 )
  {
    optversion = ( 4 << 8 ) + 5;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mfma4" );
  }
  if( cpu->capaes )
  {
    optversion = ( 4 << 8 ) + 4;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -maes" );
  }
  if( cpu->cappclmul )
  {
    optversion = ( 4 << 8 ) + 4;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mpclmul" );
  }
  if( cpu->caprdrnd )
  {
    optversion = ( 4 << 8 ) + 7;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mrdrnd" );
  }
  if( cpu->capcmpxchg16b )
  {
    optversion = ( 4 << 8 ) + 6;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mcx16" );
  }
  if( cpu->cappopcnt )
  {
    optversion = ( 4 << 8 ) + 5;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mpopcnt" );
  }
  if( cpu->caplzcnt )
  {
    optversion = ( 4 << 8 ) + 7;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mlzcnt" );
  }
  if( cpu->capmovbe )
  {
    optversion = ( 4 << 8 ) + 5;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mmovbe" );
  }
  if( cpu->capf16c )
  {
    optversion = ( 4 << 8 ) + 7;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mf16c" );
  }
  if( cpu->capbmi )
  {
    optversion = ( 4 << 8 ) + 6;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mbmi" );
  }
  if( cpu->capbmi2 )
  {
    optversion = ( 4 << 8 ) + 7;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mbmi2" );
  }
  if( cpu->captbm )
  {
    optversion = ( 4 << 8 ) + 6;
    if( optversion > reqversion )
      reqversion = optversion;
    if( gccversion >= optversion )
    s += sprintf( s, " -mtbm" );
  }
  if( cpu->arch == CPUINFO_ARCH_AMD64 )
    s += sprintf( s, " -m64" );
  else if( cpu->arch == CPUINFO_ARCH_IA32 )
    s += sprintf( s, " -m32" );
  if( cpu->capavx )
    s += sprintf( s, " -mpreferred-stack-boundary=5" );
  else if( cpu->capsse )
    s += sprintf( s, " -mpreferred-stack-boundary=4" );

  s = cctarget;
  s += sprintf( s, "GCC %d.%d\n", gccversion >> 8, gccversion & 0xff );
  s += sprintf( s, "  GCC best options require %d.%d : ", reqversion >> 8, reqversion & 0xff );
  if( gccversion >= reqversion )
    s += sprintf( s, "Okay!" );
  else
  {
    s += sprintf( s, "FAILED, please upgrade GCC ; downgrading optimization." );
    for( ; gccversion < ( class->majorv << 8 ) + class->minorv ; )
      class = class->fallback;
  }

#else

  strcpy( cctarget, "WARNING, Unknown Compiler" );
  strcpy( ccoptions, "" );

#endif

  return;
}


////


#define MODE_CPUINFO_DETECT (0x1)
#define MODE_WRITE_HEADER (0x2)
#define MODE_WRITE_MAKEFILE (0x4)
#define MODE_QUIET (0x8)


int main( int argc, char *argv[] )
{
  int a, c1, c2;
  int mode;
  int printmask;
  FILE *file, *fileconf;
  cpuInfo cpu;
  char *headertempname;
  char *headername;
  char *makefilename;
  char *targetname;
  char *makeopts;
  char cctarget[1024];
  char ccoptions[1024];

  mode = MODE_CPUINFO_DETECT;
  printmask = CPUINFO_PRINT_ALL;
  headertempname = DEFAULT_HEADER_TEMP_NAME;
  headername = DEFAULT_HEADER_NAME;
  makefilename = DEFAULT_MAKEFILE_NAME;
  targetname = DEFAULT_TARGET_NAME;
  makeopts = "";
  for( a = 1 ; a < argc ; )
  {
    if( !( strcmp( argv[a], "-detect" ) ) )
    {
      mode |= MODE_CPUINFO_DETECT;
      a++;
    }
    else if( !( strcmp( argv[a], "-ccenv" ) ) )
    {
      mode &= ~MODE_CPUINFO_DETECT;
      a++;
    }
    else if( !( strcmp( argv[a], "-h" ) ) )
    {
      mode |= MODE_WRITE_HEADER;
      a++;
    }
    else if( !( strcmp( argv[a], "-m" ) ) )
    {
      mode |= MODE_WRITE_MAKEFILE;
      a++;
    }
    else if( !( strcmp( argv[a], "-q" ) ) )
    {
      mode |= MODE_QUIET;
      a++;
    }
    else if( !( strcmp( argv[a], "-mf" ) ) )
    {
      if( ++a == argc )
      {
        printf( "Error, missing argument for -mf command\n" );
        return 1;
      }
      makefilename = argv[a++];
    }
    else if( !( strcmp( argv[a], "-hf" ) ) )
    {
      if( ++a == argc )
      {
        printf( "Error, missing argument for -hf command\n" );
        return 1;
      }
      headername = argv[a++];
    }
    else if( !( strcmp( argv[a], "-mt" ) ) )
    {
      if( ++a == argc )
      {
        printf( "Error, missing argument for -mt command\n" );
        return 1;
      }
      targetname = argv[a++];
    }
    else if( !( strcmp( argv[a], "-p" ) ) )
    {
      if( ++a == argc )
      {
        printf( "Error, missing argument for -p command\n" );
        return 1;
      }
      printmask = strtol( argv[a++], 0, 0 );
    }
    else if( !( strcmp( argv[a], "-mopts" ) ) )
    {
      if( ++a == argc )
      {
        printf( "Error, missing argument for -mopts command\n" );
        return 1;
      }
      makeopts = argv[a++];
    }
    else
    {
      printf( "Error, unrecognized command : %s\n", argv[a] );
      return 1;
    }
  }


  /* Obtain and print the cpuinfo to the configuration file */
  if( mode & MODE_CPUINFO_DETECT )
  {
    if( !( mode & MODE_QUIET ) )
      printf( "Detecting CPU features...\n\n" );
    cpuGetInfo( &cpu );
  }
  else
  {
    if( !( mode & MODE_QUIET ) )
      printf( "Obtaining target CPU features from the CC environment...\n\n" );
    cpuGetInfoEnv( &cpu );
  }
  cpuFindCcOptions( &cpu, cctarget, ccoptions );
  if( !( mode & MODE_QUIET ) )
  {
    cpuPrint( &cpu, printmask, cctarget, ccoptions );
    printf( "\n" );
  }


  /* If the user wants us to write down a Makefile with the proper CFLAGS, do it */
  if( mode & MODE_WRITE_MAKEFILE )
  {
    if( !( mode & MODE_QUIET ) )
      printf( "Writing a Makefile named \"%s\" forwarding to \"%s\".\n\n", makefilename, targetname );
    writeMakefile( makefilename, targetname, &cpu, makeopts, ccoptions );
  }


  /* If the user wants us to write down a cpuinfo header, do it */
  if( mode & MODE_WRITE_HEADER )
  {
    if( !( file = fopen( headertempname, "w+" ) ) )
    {
      printf( "Could not open file %s for writing\n", headertempname );
      return 1;
    }
    fprintf( file, "/* Automatically generated CPU information header */\n\n" );
    writeCpuInfoHeader( file, &cpu );

    /* If the existing file is up to date, do not replace it */
    if( !( fileconf = fopen( headername, "r" ) ) )
    {
      fclose( file );
      rename( headertempname, headername );
      if( !( mode & MODE_QUIET ) )
        printf( "New %s file generated.\n\n", headername );
    }
    else
    {
      fseek( file, 0, SEEK_SET );
      for( ; ; )
      {
        c1 = fgetc( file );
        c2 = fgetc( fileconf );
        if( c1 != c2 )
          break;
        if( c1 == EOF )
        {
          fclose( fileconf );
          fclose( file );
          remove( headertempname );
          if( !( mode & MODE_QUIET ) )
            printf( "Current %s file is up to date.\n\n", headername );
          return 0;
        }
      }
      fclose( fileconf );
      fclose( file );
      remove( headername );
      rename( headertempname, headername );
      if( !( mode & MODE_QUIET ) )
        printf( "New %s file generated.\n\n", headername );
    }
  }


  return 0;
}


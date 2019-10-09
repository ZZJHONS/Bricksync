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
#include <time.h>
#include <stdarg.h>
#include <signal.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"

#include "env.h"

#include "imgpng.h"


/*
gcc bntest.c bn128.c bn192.c bn256.c bn512.c cc.c -O2 -fomit-frame-pointer -funroll-loops -s -o bn -lm -Wall
*/


////



#define BN_XP_BITS (128)

/*
#define BN_XP_BITS (192)
*/
/*
#define BN_XP_BITS (256)
*/
/*
#define BN_XP_BITS (512)
*/

#include "bn.h"


////


#define FE_FIXED_POINT_OFFSET (BN_XP_BITS-8)


#define FE_INT16_FIXED_SHIFT (BN_XP_BITS-16)


////




#define BN_LOG2_SHIFT (BN_XP_BITS)
static char *bnLog2String = "0.693147180559945309417232121458176568075500134360255254120680009493393621969694715605863326996418687542001481020570685733685520235758130557032670751635075961930727570828371435190307038623891673471123350115364497955239120475172681574932065155524734139525882950453007095326366642654104239157814952043740430385500801944170641671518644712839968171784546957026271631064546150257207402481637773389638550695260668341137273873722928956493547025762652098859693201965058554";

#define BN_LOG1P0625_SHIFT (BN_XP_BITS)
static char *bnLog1p0625String = "0.06062462181643484258060613204042026328620247514472377081451769990871808792215241848864156005182819735034233796490431435665551189891421613616907155920171742353700292475860473831524352722959996641211510644773882214038866739447346579474360877115587676036541264856377193434056335997322031337201382158431227260623923920753299973635981610447290560160748245715335297092661820178500675600344551692297766325578092455814575339390774394478288327005416467645723555082150200";

#define BN_MUL1P0625_SHIFT (BN_XP_BITS-1)

#define BN_DIV1P0625_SHIFT (BN_XP_BITS)

#define BN_EXP1_SHIFT (BN_XP_BITS-2)
static char *bnExp1String = "2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427427466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019011573834187930702154089149934884167509244761460668082264800168477411853742345442437107539077744992069551702761838606261331384583000752044933826560297606737113200709328709127443747047230696977209310141692836819025515108657463772111252389784425056953696770785449969";

#define BN_EXP1INV_SHIFT (BN_XP_BITS)
static char *bnExp1InvString = "0.367879441171442321595523770161460867445811131031767834507836801697461495744899803357147274345919643746627325276843995208246975792790129008626653589494098783092194367377338115048638991125145616344987719978684475957939747302549892495453239366207964810514647520612294223089164926566600365074577283705532853738388106804787611956829893454497350739318599216617433003569937208207102277518021584994233781690715667671762336608230376122915623757209470007040509733425677576";

#define BN_EXP0P125_SHIFT (BN_XP_BITS-1)
static char *bnExp0p125String = "1.133148453066826316829007227811793872565503131745181625912820036078823577880048386513939990794941728573231527015647307565704821045258473399878556402591699526116275928070039798472932034563034065946943537272105787996917050397844900222636324241210507874149907384970375943947807442141724270887360480309232132682376030360884168286810325448480189544878139960290523410565082492730331955162596110339930529578412416511778179411673028743143362740483097971097214133832052452";

#define BN_PI_SHIFT (BN_XP_BITS-2)
static char *bnPiString = "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962";




void bnprint128x64( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 128
  bnXpShrRound( &b, bn, BN_XP_BITS-128 );
  printf( "%s0x%016llx, 0x%016llx%s", prefix, (long long)bnXpExtract64( &b, 0*64 ), (long long)bnXpExtract64( &b, 1*64 ), suffix );
#endif
  return;
}

void bnprint128x32( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 128
  bnXpShrRound( &b, bn, BN_XP_BITS-128 );
  printf( "%s0x%08x, 0x%08x, 0x%08x, 0x%08x%s", prefix, (int)bnXpExtract32( &b, 0*32 ), (int)bnXpExtract32( &b, 1*32 ), (int)bnXpExtract32( &b, 2*32 ), (int)bnXpExtract32( &b, 3*32 ), suffix );
#endif
  return;
}

void bnprint192x64( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 192
  bnXpShrRound( &b, bn, BN_XP_BITS-192 );
  printf( "%s0x%016llx, 0x%016llx, 0x%016llx%s", prefix, (long long)bnXpExtract64( &b, 0*64 ), (long long)bnXpExtract64( &b, 1*64 ), (long long)bnXpExtract64( &b, 2*64 ), suffix );
#endif
  return;
}

void bnprint192x32( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 192
  bnXpShrRound( &b, bn, BN_XP_BITS-192 );
  printf( "%s0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x%s", prefix, (int)bnXpExtract32( &b, 0*32 ), (int)bnXpExtract32( &b, 1*32 ), (int)bnXpExtract32( &b, 2*32 ), (int)bnXpExtract32( &b, 3*32 ), (int)bnXpExtract32( &b, 4*32 ), (int)bnXpExtract32( &b, 5*32 ), suffix );
#endif
  return;
}

void bnprint256x64( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 256
  bnXpShrRound( &b, bn, BN_XP_BITS-256 );
  printf( "%s0x%016llx, 0x%016llx, 0x%016llx, 0x%016llx%s", prefix, (long long)bnXpExtract64( &b, 0*64 ), (long long)bnXpExtract64( &b, 1*64 ), (long long)bnXpExtract64( &b, 2*64 ), (long long)bnXpExtract64( &b, 3*64 ), suffix );
#endif
  return;
}

void bnprint256x32( bnXp *bn, char *prefix, char *suffix )
{
  bnXp b;
#if BN_XP_BITS >= 256
  bnXpShrRound( &b, bn, BN_XP_BITS-256 );
  printf( "%s0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x%s", prefix, (int)bnXpExtract32( &b, 0*32 ), (int)bnXpExtract32( &b, 1*32 ), (int)bnXpExtract32( &b, 2*32 ), (int)bnXpExtract32( &b, 3*32 ), (int)bnXpExtract32( &b, 4*32 ), (int)bnXpExtract32( &b, 5*32 ), (int)bnXpExtract32( &b, 6*32 ), (int)bnXpExtract32( &b, 7*32 ), suffix );
#endif
  return;
}

void bnprint512x64( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 512
  bnXpShrRound( &b, bn, BN_XP_BITS-512 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%016llx", (long long)bnXpExtract64( &b, i ) );
    i += 64;
    if( i >= 512 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}

void bnprint512x32( bnXp *bn, char *prefix, char *suffix )
{
  int i;
  bnXp b;
#if BN_XP_BITS >= 512
  bnXpShrRound( &b, bn, BN_XP_BITS-512 );
  printf( "%s", prefix );
  for( i = 0 ; ; )
  {
    printf( "0x%08x", (int)bnXpExtract32( &b, i ) );
    i += 32;
    if( i >= 512 )
      break;
    printf( ", " );
  }
  printf( "%s", suffix );
#endif
  return;
}




void build()
{
  int i;
  bnXp bn, one, tmp, tmp2;

  printf( "Build\n{\n" );


  bnXpScan( &tmp, bnLog2String, BN_LOG2_SHIFT );
  printf( "  Log2\n" );
  printf( "  In  : %s\n", bnLog2String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );

  bnXpScan( &tmp, bnLog1p0625String, BN_LOG1P0625_SHIFT );
  printf( "  Log1p0625\n" );
  printf( "  In  : %s\n", bnLog1p0625String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );

  bnXpSet32( &tmp, 17 );
  bnXpSet32( &tmp2, 16 );
  bnXpDivShl( &tmp, &tmp2, 0, BN_MUL1P0625_SHIFT );
  printf( "  Mul1p0625\n" );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );

  bnXpSet32( &tmp, 16 );
  bnXpSet32( &tmp2, 17 );
  bnXpDivShl( &tmp, &tmp2, 0, BN_DIV1P0625_SHIFT );
  printf( "  Div1p0625\n" );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );



  bnXpScan( &tmp, bnExp1String, BN_EXP1_SHIFT );
  printf( "  Exp1\n" );
  printf( "  In  : %s\n", bnExp1String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );

  bnXpScan( &tmp, bnExp1InvString, BN_EXP1INV_SHIFT );
  printf( "  Exp1inv\n" );
  printf( "  In  : %s\n", bnExp1InvString );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );


  bnXpScan( &tmp, bnExp0p125String, BN_EXP0P125_SHIFT );
  printf( "  Exp0p125\n" );
  printf( "  In  : %s\n", bnExp0p125String );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );



  bnXpScan( &tmp, bnPiString, BN_PI_SHIFT );
  printf( "  Pi\n" );
  printf( "  In  : %s\n", bnPiString );
  bnprint128x64( &tmp, "   (128,64) : ", "\n" );
  bnprint128x32( &tmp, "   (128,32) : ", "\n" );
  bnprint192x64( &tmp, "   (192,64) : ", "\n" );
  bnprint192x32( &tmp, "   (192,32) : ", "\n" );
  bnprint256x64( &tmp, "   (256,64) : ", "\n" );
  bnprint256x32( &tmp, "   (256,32) : ", "\n" );
  bnprint512x64( &tmp, "   (512,64) : ", "\n" );
  bnprint512x32( &tmp, "   (512,32) : ", "\n" );
  printf( "\n" );



  bnXpSet32Shl( &one, 1, BN_XP_BITS-1 );
#if BN_XP_BITS >= 128
  printf( "  Div Table (128,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint128x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (128,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint128x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 192
  printf( "  Div Table (192,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint192x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (192,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint192x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 256
  printf( "  Div Table (256,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint256x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (256,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint256x32( &bn, "", " } },\n" );
  }
#endif
#if BN_XP_BITS >= 512
  printf( "  Div Table (512,64)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint512x64( &bn, "", " } },\n" );
  }
  printf( "  Div Table (512,32)\n" );
  for( i = 0 ; i < 256 ; i++ )
  {
    bnXpSet( &bn, &one );
    if( i >= 2 )
      bnXpDiv32Round( &bn, i );
    else
      bnXpZero( &bn );
    printf( "  [%d] = { .unit = { ", i );
    bnprint512x32( &bn, "", " } },\n" );
  }
#endif



  printf( "}\n\n" );
  fflush( stdout );


  return;
}



////



void findprimes()
{
  int i, primecount, primealloc;
  bnXp *primelist;
  bnXp target, quotient, rem, maxsearch;
  char buffer[4096];
  uint64_t t0, t1;

  primealloc = 1048576/4;
  primelist = malloc( primealloc * sizeof(bnXp) );

  bnXpSet32( &primelist[0], 2 );
  primecount = 1;
  bnXpSet32( &target, 3 );
  bnXpSqrt( &maxsearch, &target, 0 );

  t0 = mmGetMilisecondsTime();
  for( ; ; )
  {
    for( i = 0 ; ; i++ )
    {
      if( ( i >= primecount ) || ( bnXpCmpGt( &primelist[i], &maxsearch ) ) )
      {
        bnXpSet( &primelist[primecount++], &target );
        if( primecount == primealloc )
          goto done;
        if( !( i & 0xf ) )
          bnXpSqrt( &maxsearch, &target, 0 );
        break;
      }
      bnXpSet( &quotient, &target );
      bnXpDiv( &quotient, &primelist[i], &rem );
      if( bnXpCmpZero( &rem ) )
        break;
    }
    bnXpAdd32( &target, 2 );
    bnXpAdd32( &maxsearch, 2 );
  }

  done:

  t1 = mmGetMilisecondsTime();
  for( i = 0 ; i < primecount ; i++ )
  {
    bnXpPrint( &primelist[i], buffer, 4096, 1, 0, 0 );
    printf( "Prime %d : %s\n", i+1, buffer );
  }
  printf( "Time : %lld msecs\n", (long long)( t1 - t0 ) );
  free( primelist );

  return;
}



////



int main()
{
  int lowbits, i1;
  double f0, f1;
  bnXp h0, h1, h2, h3;
  char buffer[4096];
  char *v0, *v1;





/*
  build();
*/



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "5.934";
  v1 = "2.293";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpDivShl( &h0, &h1, 0, lowbits );
  bnXpPrint( &h0, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.div(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86div(%.3f,%.3f) = %.16f\n", f0, f1, f0/f1 );

  lowbits = BN_XP_BITS / 2;
  v0 = "793035312.459";
  v1 = "0.237";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpDivShl( &h0, &h1, 0, lowbits );
  bnXpPrint( &h0, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.div(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86div(%.3f,%.3f) = %.16f\n", f0, f1, f0/f1 );



  lowbits = 1;
  v0 = "2.5";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );

  lowbits = 2;
  v0 = "285329435347.0";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "43.023";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "0.00000000000000000000934";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSqrt( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sqrt(%.3f) = %s\n", f0, buffer );
  printf( "  x86sqrt(%.3f) = %.16f\n", f0, sqrt(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "95.308";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpLog( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.log(%.3f) = %s\n", f0, buffer );
  printf( "  x86log(%.3f) = %.16f\n", f0, log(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "1.6";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpLog( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.log(%.3f) = %s\n", f0, buffer );
  printf( "  x86log(%.3f) = %.16f\n", f0, log(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "1.7";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpExp( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.exp(%.3f) = %s\n", f0, buffer );
  printf( "  x86exp(%.3f) = %.16f\n", f0, exp(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "-3.8";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpExp( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.exp(%.3f) = %s\n", f0, buffer );
  printf( "  x86exp(%.3f) = %.16f\n", f0, exp(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "2.39";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpExp( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.exp(%.3f) = %s\n", f0, buffer );
  printf( "  x86exp(%.3f) = %.16f\n", f0, exp(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "1.38";
  v1 = "1.72";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "4.0";
  v1 = "3.0";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );

  lowbits = 2;
  v0 = "2.0";
  v1 = "-2.0";
  f0 = atof( v0 );
  f1 = atof( v1 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "0.3";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpCos( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.cos(%.3f) = %s\n", f0, buffer );
  printf( "  x86cos(%.3f) = %.16f\n", f0, cos(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "-6.2";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpCos( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.cos(%.3f) = %s\n", f0, buffer );
  printf( "  x86cos(%.3f) = %.16f\n", f0, cos(f0) );

  lowbits = BN_XP_BITS - 32;
  v0 = "31545.0";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpCos( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.cos(%.3f) = %s\n", f0, buffer );
  printf( "  x86cos(%.3f) = %.16f\n", f0, cos(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "13.6";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSin( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sin(%.3f) = %s\n", f0, buffer );
  printf( "  x86sin(%.3f) = %.16f\n", f0, sin(f0) );

  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "5.7";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpSin( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.sin(%.3f) = %s\n", f0, buffer );
  printf( "  x86sin(%.3f) = %.16f\n", f0, sin(f0) );



  lowbits = FE_FIXED_POINT_OFFSET;
  v0 = "34.7";
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpTan( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.tan(%.3f) = %s\n", f0, buffer );
  printf( "  x86tan(%.3f) = %.16f\n", f0, tan(f0) );

  v0 = "4.9";
  f0 = atof( v0 );
  lowbits = FE_FIXED_POINT_OFFSET;
  bnXpScan( &h0, v0, lowbits );
  bnXpTan( &h1, &h0, lowbits );
  bnXpPrint( &h1, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.tan(%.3f) = %s\n", f0, buffer );
  printf( "  x86tan(%.3f) = %.16f\n", f0, tan(f0) );



  lowbits = 0;
  v0 = "3";
  i1 = 43;
  f0 = atof( v0 );
  bnXpScan( &h0, v0, lowbits );
  bnXpScan( &h1, v1, lowbits );
  bnXpPowInt( &h2, &h0, i1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 0 );
  printf( "Low bits : %d\n", lowbits );
  printf( "  bn.pow(%.3f,%d) = %s\n", f0, i1, buffer );
  printf( "  x86pow(%.3f,%d) = %.16f\n", f0, i1, pow(f0,i1) );






/*
srand( time(0) );
for( ; ; )
{
  h0.unit[0] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[1] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[2] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[3] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[4] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[5] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[6] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h0.unit[7] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[0] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[1] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[2] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[3] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[4] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[5] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[6] = (long)rand() + ( (long)(rand()+rand()) << 32 );
  h1.unit[7] = (long)rand() + ( (long)(rand()+rand()) << 32 );

  bn512Mul( &h2, &h1, &h0 );
  bn512MulShr( &h3, &h1, &h0, 0 );

  bnprint512x64( &h0, "Src 0  :", "\n" );
  bnprint512x64( &h1, "Src 1  :", "\n" );
  bnprint512x64( &h2, "Res A  :", "\n" );
  bnprint512x64( &h3, "Res B  :", "\n" );

  if( bn512CmpNeq( &h2, &h3 ) )
    exit( 0 );

}
*/




  uint64_t t0, t1;
  bnXp k0,k1,k2,k3,k4,k5,k6,kr;

  bnXpSetDouble( &k0, 2.13185, 510 );
  bnXpSetDouble( &k1, -1.9472, 510 );
  bnXpSetDouble( &k2, 0.94387, 510 );
  bnXpSetDouble( &k3, -0.4192, 510 );
  bnXpSetDouble( &k4, 1.84293, 510 );
  bnXpSetDouble( &k5, 0.72641, 510 );

  sleep( 1 );
  t0 = mmGetMilisecondsTime();
  int i;
  for( i = 0 ; i < 40000000 ; i++ )
  {
    bnXpMulSignedShr( &kr, &k0, &k3, 1 );
    bnXpMulSignedShr( &kr, &k1, &k4, 1 );
    bnXpMulSignedShr( &kr, &k2, &k5, 1 );
  }
  t1 = mmGetMilisecondsTime();
  sleep( 1 );

  printf( "Time : %lld msecs for %d muls of %d bits\n", (long long)( t1 - t0 ), 3*40000000, BN_XP_BITS );







/*
int i;
for( i = 0 ; ; i++ )
{
  lowbits = BN_XP_BITS - 32;

  f0 = (double)rand() / (double)RAND_MAX;
  f1 = (double)rand() / (double)RAND_MAX;
  f0 = 8.0 * f0;
  f1 = 8.0 * ( f1 - 0.5 );

  printf( "Low bits : %d\n", lowbits );
  printf( "  x86pow(%.3f,%.3f) = %.16f\n", f0, f1, pow(f0,f1) );

  bnXpSetDouble( &h0, f0, lowbits );
  bnXpSetDouble( &h1, f1, lowbits );
  bnXpPow( &h2, &h0, &h1, lowbits );
  bnXpPrint( &h2, buffer, 4096, 1, lowbits, 2+log10(pow(2,(double)lowbits) ) );
  printf( "  bn.pow(%.3f,%.3f) = %s\n", f0, f1, buffer );
}
*/







/*
  bnUnit result[BN_INT256_UNIT_COUNT*2];
  bnUnit result2[BN_INT256_UNIT_COUNT*2];
  bnUnit src0[BN_INT256_UNIT_COUNT];
  bnUnit src1[BN_INT256_UNIT_COUNT];

srand( time(0) );
for( ; ; )
{
  int i;
  memset( src0, 0, BN_INT256_UNIT_COUNT*sizeof(bnUnit) );
  memset( src1, 0, BN_INT256_UNIT_COUNT*sizeof(bnUnit) );
  memset( result, 0, 2*BN_INT256_UNIT_COUNT*sizeof(bnUnit) );
  memset( result2, 0, 2*BN_INT256_UNIT_COUNT*sizeof(bnUnit) );

  for( i = 0 ; i < BN_INT256_UNIT_COUNT ; i++ )
  {
#if BN_UNIT_BITS == 64
    src0[i] = (long)rand() + ( (long)(rand()+rand()) << 32 );
    src1[i] = (long)rand() + ( (long)(rand()+rand()) << 32 );
#else
    src0[i] = (long)rand() + rand();
    src1[i] = (long)rand() + rand();
#endif
  }

  bn256MulExtended( result, src0, src1, 0xffffffff );
  bn256AddMulExtended( result2, src0, src1, 0xffffffff, 0 );

#if BN_UNIT_BITS == 64
  printf( "Src 0    : %016llx %016llx %016llx %016llx\n", (long long)src0[0], (long long)src0[1], (long long)src0[2], (long long)src0[3] );
  printf( "Src 1    : %016llx %016llx %016llx %016llx\n", (long long)src1[0], (long long)src1[1], (long long)src1[2], (long long)src1[3] );
  printf( "Result 1 : %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", (long long)result[0], (long long)result[1], (long long)result[2], (long long)result[3], (long long)result[4], (long long)result[5], (long long)result[6], (long long)result[7] );
  printf( "Result 2 : %016llx %016llx %016llx %016llx %016llx %016llx %016llx %016llx\n", (long long)result2[0], (long long)result2[1], (long long)result2[2], (long long)result2[3], (long long)result2[4], (long long)result2[5], (long long)result2[6], (long long)result2[7] );
#else
  printf( "Src 0    : %08x %08x %08x %08x\n", (int)src0[0], (int)src0[1], (int)src0[2], (int)src0[3] );
  printf( "Src 1    : %08x %08x %08x %08x\n", (int)src1[0], (int)src1[1], (int)src1[2], (int)src1[3] );
  printf( "Result 1 : %08x %08x %08x %08x %08x %08x %08x %08x\n", (int)result[0], (int)result[1], (int)result[2], (int)result[3], (int)result[4], (int)result[5], (int)result[6], (int)result[7] );
  printf( "Result 2 : %08x %08x %08x %08x %08x %08x %08x %08x\n", (int)result2[0], (int)result2[1], (int)result2[2], (int)result2[3], (int)result2[4], (int)result2[5], (int)result2[6], (int)result2[7] );
#endif

if( ( result[0] != result2[0] ) || ( result[1] != result2[1] ) || ( result[2] != result2[2] ) || ( result[3] != result2[3] ) || ( result[4] != result2[4] ) || ( result[5] != result2[5] ) || ( result[6] != result2[6] ) || ( result[7] != result2[7] ) )
exit(0);

}
*/






/*
  findprimes();
*/



  return 1;
}










/*
def log2(x, tol=1e-13):

    res = 0.0
 
    # Integer part
    while x<1:
        res -= 1
        x *= 2
    while x>=2:
        res += 1
        x /= 2
 
    # Fractional part
    fp = 1.0
    while fp>=tol:
        fp /= 2
        x *= x
        if x >= 2:
            x /= 2
            res += fp

     return res
*/



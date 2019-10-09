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

#include "cpuconfig.h"

#include "cc.h"
#include "ccstr.h"
#include "rand.h"


////


#define BN_XP_BITS 1024
#define BN_XP_SUPPORT_128 0
#define BN_XP_SUPPORT_192 0
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 1
#define BN_XP_SUPPORT_1024 1
#include "bn.h"


////


#include "rsabn.h"


////



/*
gcc rsabn.c rand.c cc.c bn128.c bn192.c bn256.c bn512.c bn1024.c bnstdio.c -g -O2 -mtune=nocona -ffast-math -fomit-frame-pointer -msse -msse2 -msse3 -o rsabn -lm -Wall

gcc -m32 rsabn.c rand.c cc.c bn256.c bn512.c bn1024.c bnstdio.c -g -O2 -mtune=nocona -ffast-math -fomit-frame-pointer -msse -msse2 -msse3 -o rsabn -lm -Wall
*/


#define RSA_DEBUG (0)


/*
Left to implement:
- Chinese remainder algorithm
*/




////



static rsaType rsaModularMultiplication( rsaType a, rsaType b, rsaType mod )
{
  rsaType d, mp2;
  int i;

  bnXpZero( &d );
  bnXpSetShr1( &mp2, &mod );
  if( bnXpCmpGe( &a, &mod ) )
  {
    bnXpSet( &d, &a );
    bnXpDiv( &d, &mod, &a );
  }
  if( bnXpCmpGe( &b, &mod ) )
  {
    bnXpSet( &d, &b );
    bnXpDiv( &d, &mod, &b );
  }
  for( i = 0 ; i < RSA_DATA_BITS ; i++ )
  {
    if( bnXpCmpGt( &d, &mp2 ) )
    {
      bnXpShl1( &d );
      bnXpSub( &d, &mod );
    }
    else
      bnXpShl1( &d );
    /* Check if top bit is set */
    if( bnXpExtractBit( &a, RSA_DATA_BITS-1 ) )
      bnXpAdd( &d, &b );
    if( bnXpCmpGt( &d, &mod ) )
      bnXpSub( &d, &mod );
    bnXpShl1( &a );
  }

  return d;
}


/* Modular exponentiation, (base^exponent)%mod */
static rsaType rsaModularExponentiation( rsaType base, rsaType exponent, rsaType mod )
{
  rsaType x;

  bnXpSet32( &x, 1 );
  for( ; bnXpCmpNotZero( &exponent ) ; bnXpShr1( &exponent ) )
  {
    if( bnXpExtractBit( &exponent, 0 ) )
      x = rsaModularMultiplication( x, base, mod );
    base = rsaModularMultiplication( base, base, mod );
  }
  return x;
}


/* Use to quickly find coprimes */
static rsaType rsaGreatestCommonDivisor( rsaType u, rsaType v )
{
  int shift;
  rsaType temp, retval;
 
  if( bnXpCmpZero( &u ) )
    return v;
  if( bnXpCmpZero( &v ) )
    return u;
 
  /* Let shift := lg K, where K is the greatest power of 2 dividing both u and v. */
  for( shift = 0 ; ; shift++ )
  {
    if( ( bnXpExtractBit( &u, 0 ) | bnXpExtractBit( &v, 0 ) ) & 0x1 )
      break;
    bnXpShr1( &u );
    bnXpShr1( &v );
  }
 
  while( !( bnXpExtractBit( &u, 0 ) ) )
    bnXpShr1( &u );
 
  /* From here on, u is always odd. */
  do
  {
    /* remove all factors of 2 in v -- they are not common */
    /*   note: v is not zero, so while will terminate */
    for( ; !( bnXpExtractBit( &v, 0 ) ) ; )  /* Loop X */
      bnXpShr1( &v );

    /* Now u and v are both odd. Swap if necessary so u <= v,
    then set v = v - u (which is even). For bignums, the
    swapping is just pointer movement, and the subtraction
    can be done in-place. */
    if( bnXpCmpGt( &u, &v ) )
    {
      bnXpSet( &temp, &v );
      bnXpSet( &v, &u );
      bnXpSet( &u, &temp );
    }
    bnXpSub( &v, &u );
  } while( bnXpCmpNotZero( &v ) );
 
  /* restore common factors of 2 */
  bnXpShl( &retval, &u, shift );
  return retval;
}


////


/* Must be a multiple of 4 */
#define RSA_PRIME_BASE_TABLE (512)

#define RSA_PRIME_MILLER_RABIN_PRIME_ROUNDS (32)
#define RSA_PRIME_MILLER_RABIN_RANDOM_ROUNDS (32)

const uint32_t rsaPrimeTable[RSA_PRIME_BASE_TABLE] =
{
    2,   3,   5,   7,  11,  13,  17,  19,  23,  29,
   31,  37,  41,  43,  47,  53,  59,  61,  67,  71,
   73,  79,  83,  89,  97, 101, 103, 107, 109, 113,
  127, 131, 137, 139, 149, 151, 157, 163, 167, 173,
  179, 181, 191, 193, 197, 199, 211, 223, 227, 229,
  233, 239, 241, 251, 257, 263, 269, 271, 277, 281,
  283, 293, 307, 311, 313, 317, 331, 337, 347, 349,
  353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
  419, 421, 431, 433, 439, 443, 449, 457, 461, 463,
  467, 479, 487, 491, 499, 503, 509, 521, 523, 541,
  547, 557, 563, 569, 571, 577, 587, 593, 599, 601,
  607, 613, 617, 619, 631, 641, 643, 647, 653, 659,
  661, 673, 677, 683, 691, 701, 709, 719, 727, 733,
  739, 743, 751, 757, 761, 769, 773, 787, 797, 809,
  811, 821, 823, 827, 829, 839, 853, 857, 859, 863,
  877, 881, 883, 887, 907, 911, 919, 929, 937, 941,
  947, 953, 967, 971, 977, 983, 991, 997,1009,1013,
 1019,1021,1031,1033,1039,1049,1051,1061,1063,1069,
 1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,
 1153,1163,1171,1181,1187,1193,1201,1213,1217,1223,
 1229,1231,1237,1249,1259,1277,1279,1283,1289,1291,
 1297,1301,1303,1307,1319,1321,1327,1361,1367,1373,
 1381,1399,1409,1423,1427,1429,1433,1439,1447,1451,
 1453,1459,1471,1481,1483,1487,1489,1493,1499,1511,
 1523,1531,1543,1549,1553,1559,1567,1571,1579,1583,
 1597,1601,1607,1609,1613,1619,1621,1627,1637,1657,
 1663,1667,1669,1693,1697,1699,1709,1721,1723,1733,
 1741,1747,1753,1759,1777,1783,1787,1789,1801,1811,
 1823,1831,1847,1861,1867,1871,1873,1877,1879,1889,
 1901,1907,1913,1931,1933,1949,1951,1973,1979,1987,
 1993,1997,1999,2003,2011,2017,2027,2029,2039,2053,
 2063,2069,2081,2083,2087,2089,2099,2111,2113,2129,
 2131,2137,2141,2143,2153,2161,2179,2203,2207,2213,
 2221,2237,2239,2243,2251,2267,2269,2273,2281,2287,
 2293,2297,2309,2311,2333,2339,2341,2347,2351,2357,
 2371,2377,2381,2383,2389,2393,2399,2411,2417,2423,
 2437,2441,2447,2459,2467,2473,2477,2503,2521,2531,
 2539,2543,2549,2551,2557,2579,2591,2593,2609,2617,
 2621,2633,2647,2657,2659,2663,2671,2677,2683,2687,
 2689,2693,2699,2707,2711,2713,2719,2729,2731,2741,
 2749,2753,2767,2777,2789,2791,2797,2801,2803,2819,
 2833,2837,2843,2851,2857,2861,2879,2887,2897,2903,
 2909,2917,2927,2939,2953,2957,2963,2969,2971,2999,
 3001,3011,3019,3023,3037,3041,3049,3061,3067,3079,
 3083,3089,3109,3119,3121,3137,3163,3167,3169,3181,
 3187,3191,3203,3209,3217,3221,3229,3251,3253,3257,
 3259,3271,3299,3301,3307,3313,3319,3323,3329,3331,
 3343,3347,3359,3361,3371,3373,3389,3391,3407,3413,
 3433,3449,3457,3461,3463,3467,3469,3491,3499,3511,
 3517,3527,3529,3533,3539,3541,3547,3557,3559,3571,
 3581,3583,3593,3607,3613,3617,3623,3631,3637,3643,
 3659,3671
};


static int MillerRabin( rsaType n, rsaType k )
{
  int s;
  rsaType d, base, exponent, x, one, nmone, sqx;
  if( bnXpCmpEq( &n, &k ) )
    return 1;

  /* Factor n-1 as d 2^s */
  bnXpSet( &d, &n );
  bnXpSub32( &d, 1 );
  for( s = 0 ; !( bnXpExtractBit( &d, 0 ) ); s++ )
    bnXpShr1( &d );

  /* x = ( k^d mod n ) using exponentiation by squaring */
  bnXpSet32( &x, 1 );
  bnXpDiv( &k, &n, &base );
  for( bnXpSet( &exponent, &d ) ; bnXpCmpNotZero( &exponent ) ; bnXpShr1( &exponent ) )
  {
    if( bnXpExtractBit( &exponent, 0 ) )
      x = rsaModularMultiplication( x, base, n );
    base = rsaModularMultiplication( base, base, n );
  }

  /* Verify k^(d 2^[0...s-1]) mod n != 1 */

  bnXpSet32( &one, 1 );
  if( bnXpCmpEq( &x, &one ) )
    return 1;
  bnXpSet( &nmone, &n );
  bnXpSub32( &nmone, 1 );
  if( bnXpCmpEq( &x, &nmone ) )
    return 1;
  for( ; s-- > 1 ; )
  {
    bnXpMul( &sqx, &x, &x );
    bnXpDiv( &sqx, &n, &x );
    if( bnXpCmpEq( &x, &one ) )
      return 0;
    if( bnXpCmpEq( &x, &nmone ) )
      return 1;
  }

  return 0;
}


static rsaType rsaSetRandom( rand32State *randstate )
{
  int i;
  rsaType n;
  for( i = 0 ; i < BN_XP_BITS/BN_UNIT_BITS ; i++ )
  {
#if BN_UNIT_BITS == 64
    n.unit[i] = ( (uint64_t)rand32Int( randstate ) ) | ( ( (uint64_t)rand32Int( randstate ) ) << 32 );
#elif BN_UNIT_BITS == 32
    n.unit[i] = rand32Int( randstate );
#else
 #error
#endif
  }
  return n;
}


int rsaPrimeCheck( rsaType n, rand32State *randstate )
{
  int i;
  uint32_t nrem;
  rsaType randvalue, primecheck, ndiv, tablemax;

  /* Check some small primes */
  bnXpSet32( &tablemax, rsaPrimeTable[RSA_PRIME_BASE_TABLE-1] );
  if( bnXpCmpLt( &n, &tablemax ) )
  {
    for( i = 0; i < RSA_PRIME_BASE_TABLE ; i += 4 )
    {
      if( ( n.unit[0] == rsaPrimeTable[i+0] ) || ( n.unit[0] == rsaPrimeTable[i+1] ) || ( n.unit[0] == rsaPrimeTable[i+2] ) || ( n.unit[0] == rsaPrimeTable[i+3] ) )
        return 1;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+0], &nrem );
      if( !( nrem ) )
        return 0;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+1], &nrem );
      if( !( nrem ) )
        return 0;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+2], &nrem );
      if( !( nrem ) )
        return 0;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+3], &nrem );
      if( !( nrem ) )
        return 0;
    }
  }
  else
  {
    for( i = 0; i < RSA_PRIME_BASE_TABLE ; i += 4 )
    {
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+0], &nrem );
      if( !( nrem ) )
        return 0;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+1], &nrem );
      if( !( nrem ) )
        return 0;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+2], &nrem );
      if( !( nrem ) )
        return 0;
      bnXpSet( &ndiv, &n );
      bnXpDiv32( &ndiv, rsaPrimeTable[i+3], &nrem );
      if( !( nrem ) )
        return 0;
    }
  }

  /* Do a few Miller-Rabin rounds */
  for( i = 0 ; i < RSA_PRIME_MILLER_RABIN_PRIME_ROUNDS ; i++ )
  {
    bnXpSet32( &primecheck, rsaPrimeTable[i] );
    if( !( MillerRabin( n, primecheck ) ) )
      return 0;
  }
  if( randstate )
  {
/*
    rsaType primemask;
    bnXpSet32Shl( &primemask, 1, RSA_DATA_BITS );
    bnXpSub32( &primemask, 1 );
*/
    for( i = 0 ; i < RSA_PRIME_MILLER_RABIN_RANDOM_ROUNDS ; i++ )
    {
      for( ; ; )
      {
        randvalue = rsaSetRandom( randstate );
/*
        bnXpAnd( &randvalue, &primemask );
*/
        bnXpDiv( &randvalue, &n, &primecheck );



        if( bnXpCmpGt( &primecheck, &tablemax ) )
          break;
      }
      if( !( MillerRabin( n, primecheck ) ) )
        return 0;
    }
  }

  return 1;
}



////



static rsaType rsaInverseNumModulusPhi( rsaType a, rsaType hugephi )
{
  rsaTypeSigned b0, t, q, one, tmpq;
  rsaTypeSigned x0, x1, newphi;

  bnXpSet( &b0, &hugephi );
  bnXpSet32( &x0, 0 );
  bnXpSet32( &x1, 1 );
  bnXpSet32( &one, 1 );
  if( bnXpCmpEq( &hugephi, &one ) )
    return one;
  while( bnXpCmpGt( &a, &one ) )
  {
    bnXpSet( &t, &hugephi );
    bnXpSet( &q, &a );
    bnXpDiv( &q, &hugephi, &newphi );
    bnXpSet( &hugephi, &newphi );
    bnXpSet( &a, &t );
    bnXpSet( &t, &x0 );
    bnXpMul( &tmpq, &q, &x0 );
    bnXpSetSub( &x0, &x1, &tmpq );
    bnXpSet( &x1, &t );
  }
  /* Check if top bit is set */
  if( bnXpExtractBit( &x1, RSA_DATA_BITS-1 ) )
    bnXpAdd( &x1, &b0 );

  return x1;
}


static int rsaFindKeyPair( rsaType key, rsaType hugephi, rsaType *retinvkey )
{
  rsaType invkey, phidiv, phirem, gcd, one;

  bnXpSet( &phidiv, &hugephi );
  bnXpDiv( &phidiv, &key, &phirem );
  if( bnXpCmpZero( &phirem ) )
    return 0;
  bnXpSet32( &one, 1 );
  gcd = rsaGreatestCommonDivisor( key, hugephi );
  if( bnXpCmpEq( &gcd, &one ) )
  {
    invkey = rsaInverseNumModulusPhi( key, hugephi );
    if( bnXpCmpNotZero( &invkey ) )
    {
      *retinvkey = invkey;
      return 1;
    }
  }

  return 0;
}


////


/* Public interface */

#define RSA_DEBUG_PRINT_BUFFER_SIZE (512)

#define RSA_PRIME0_BITS ((RSA_DATA_BITS>>1)+0)
#define RSA_PRIME1_BITS ((RSA_DATA_BITS>>1)+0)


void rsaGenKeys( rand32State *randstate, int inputbitcount, rsaType *retkey, rsaType *retinvkey, rsaType *retproduct )
{
  int productbitcount;
  rsaType HugePrime0, HugePrime1;
  rsaType prime0minus1, prime1minus1;
  rsaType primemask, primemin;
  rsaType hugephi;
  rsaType rsaproduct;
  rsaType rsakey, rsainvkey;
  rsaType three;
  bnXpSet32( &three, 3 );
#if RSA_DEBUG
  int64_t t0, t1;
  char printbuffer[RSA_DEBUG_PRINT_BUFFER_SIZE];
#endif

#if RSA_DEBUG
  t0 = ccGetMillisecondsTime();
#endif
  for( ; ; )
  {
    bnXpSet32Shl( &primemask, 1, RSA_PRIME0_BITS );
    bnXpSub32( &primemask, 1 );
    bnXpShr( &primemin, &primemask, 2 );
    if( bnXpCmpLt( &primemin, &three ) )
      bnXpSet( &primemin, &three );
    for( ; ; )
    {
      HugePrime0 = rsaSetRandom( randstate );
      if( !( bnXpExtractBit( &HugePrime0, 0 ) ) )
        bnXpAdd32( &HugePrime0, 1 );
      bnXpAnd( &HugePrime0, &primemask );
      if( bnXpCmpLt( &HugePrime0, &primemin ) )
        continue;
      if( rsaPrimeCheck( HugePrime0, randstate ) )
        break;
    }
    for( ; ; )
    {
      HugePrime1 = rsaSetRandom( randstate );
      if( !( bnXpExtractBit( &HugePrime1, 0 ) ) )
        bnXpAdd32( &HugePrime1, 1 );
      bnXpAnd( &HugePrime1, &primemask );
      if( bnXpCmpLt( &HugePrime1, &primemin ) )
        continue;
      if( rsaPrimeCheck( HugePrime1, randstate ) )
        break;
    }
    if( bnXpCmpEq( &HugePrime0, &HugePrime1 ) )
      continue;
    bnXpMul( &rsaproduct, &HugePrime0, &HugePrime1 );
    productbitcount = bnXpGetIndexMSB( &rsaproduct );

    if( productbitcount < inputbitcount )
      continue;

/*
  printf( "Huge Product : %llu ~ 0x%llx\n", (long long)rsaproduct.unit[0], (long long)rsaproduct.unit[0] );
  printf( "Huge Product Bit Count   : %d / %d\n", productbitcount, RSA_DATA_BITS );
*/

    if( ( productbitcount >= (RSA_DATA_BITS-4) ) && ( productbitcount < (RSA_DATA_BITS-1) ) )
      break;
  }

  bnXpSet( &prime0minus1, &HugePrime0 );
  bnXpSub32( &prime0minus1, 1 );
  bnXpSet( &prime1minus1, &HugePrime1 );
  bnXpSub32( &prime1minus1, 1 );
  bnXpMul( &hugephi, &prime0minus1, &prime1minus1 );

#if RSA_DEBUG
  t1 = ccGetMillisecondsTime();
#endif

#if RSA_DEBUG
  printf( "\n" );
  bnXpPrint( &HugePrime0, printbuffer, RSA_DEBUG_PRINT_BUFFER_SIZE, 0, 0, 0 );
  printf( "HugePrime0 : %s\n", printbuffer );
  bnXpPrint( &HugePrime1, printbuffer, RSA_DEBUG_PRINT_BUFFER_SIZE, 0, 0, 0 );
  printf( "HugePrime1 : %s\n", printbuffer );
  bnXpPrint( &rsaproduct, printbuffer, RSA_DEBUG_PRINT_BUFFER_SIZE, 0, 0, 0 );
  printf( "Huge Product : %s\n", printbuffer );
  printf( "Huge Product Bit Count   : %d / %d\n", productbitcount, RSA_DATA_BITS );
  bnXpPrint( &hugephi, printbuffer, RSA_DEBUG_PRINT_BUFFER_SIZE, 0, 0, 0 );
  printf( "Huge Phi : %s\n", printbuffer );
  printf( "Primes Time : %lld ms\n", (long long)( t1 - t0 ) );
#endif

#if RSA_DEBUG
  t0 = ccGetMillisecondsTime();
#endif
  for( ; ; )
  {
    rsaType rsarand;
    rsarand = rsaSetRandom( randstate );
    bnXpDiv( &rsarand, &hugephi, &rsakey );
    if( !( bnXpExtractBit( &rsakey, 0 ) ) )
    {
      bnXpAdd32( &rsakey, 1 );
      if( bnXpCmpEq( &rsakey, &hugephi ) )
        continue;
    }
    if( bnXpCmpLt( &rsakey, &three ) )
      continue;
    if( bnXpCmpEq( &rsakey, &HugePrime0 ) )
      continue;
    if( bnXpCmpEq( &rsakey, &HugePrime1 ) )
      continue;
    if( rsaFindKeyPair( rsakey, hugephi, &rsainvkey ) )
    {
      if( bnXpCmpNeq( &rsakey, &rsainvkey ) )
        break;
    }
  }
#if RSA_DEBUG
  t1 = ccGetMillisecondsTime();
#endif

#if RSA_DEBUG
  bnXpPrintHex( &rsakey, printbuffer, RSA_DEBUG_PRINT_BUFFER_SIZE, 0, 0, 0 );
  printf( "Key    : 0x%s\n", printbuffer );
  bnXpPrintHex( &rsainvkey, printbuffer, RSA_DEBUG_PRINT_BUFFER_SIZE, 0, 0, 0 );
  printf( "InvKey : 0x%s\n", printbuffer );
  printf( "KeyPair Time : %lld ms\n", (long long)( t1 - t0 ) );
  printf( "\n" );
#endif

  *retkey = rsakey;
  *retinvkey = rsainvkey;
  *retproduct = rsaproduct;
  return;
}


void rsaEncrypt( rsaType *input, rsaType *output, int length, rsaType rsakey, rsaType rsaproduct )
{
  int i;
#if 0
printf( "Encrypt ; Key %llu ; rsaproduct %llu\n\n", (long long)key, (long long)rsaproduct );
#endif
  for( i = 0 ; i < length ; i++ )
    output[i] = rsaModularExponentiation( input[i], rsakey, rsaproduct );

  return;
}


/*
rsaproduct must be bigger than the values being encrypted/decrypted
*/


////

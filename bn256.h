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


#define BN_INT256_UNIT_COUNT (256/BN_UNIT_BITS)
#define BN_INT256_UNIT_COUNT_SHIFT (256>>BN_UNIT_BIT_SHIFT)


typedef struct
{
  bnUnit unit[BN_INT256_UNIT_COUNT];
} bn256;


////


void bn256Zero( bn256 *dst );
void bn256Set( bn256 *dst, const bn256 * const src );
void bn256Set32( bn256 *dst, uint32_t value );
void bn256Set32Signed( bn256 *dst, int32_t value );
void bn256Set32Shl( bn256 *dst, uint32_t value, bnShift shift );
void bn256SetDouble( bn256 *dst, double value, bnShift leftshift );
double bn256GetDouble( bn256 *dst, bnShift rightshift );

void bn256Add32( bn256 *dst, uint32_t value );
void bn256Add32Signed( bn256 *dst, int32_t value );
void bn256Add32Shl( bn256 *dst, uint32_t value, bnShift shift );
void bn256Sub32( bn256 *dst, uint32_t value );
void bn256Sub32Signed( bn256 *dst, int32_t value );
void bn256Sub32Shl( bn256 *dst, uint32_t value, bnShift shift );
void bn256Set32SignedShl( bn256 *dst, int32_t value, bnShift shift );
void bn256Add( bn256 *dst, const bn256 * const src );
void bn256SetAdd( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256Sub( bn256 *dst, const bn256 * const src );
void bn256SetSub( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256SetAddAdd( bn256 *dst, const bn256 * const src, const bn256 * const srcadd0, const bn256 * const srcadd1 );
void bn256SetAddSub( bn256 *dst, const bn256 * const src, const bn256 * const srcadd, const bn256 * const srcsub );
void bn256SetAddAddSub( bn256 *dst, const bn256 * const src, const bn256 * const srcadd0, const bn256 * const srcadd1, const bn256 * const srcsub );
void bn256SetAddAddAddSub( bn256 *dst, const bn256 * const src, const bn256 * const srcadd0, const bn256 * const srcadd1, const bn256 * const srcadd2, const bn256 * const srcsub );

void bn256Mul32( bn256 *dst, const bn256 * const src, uint32_t value );
void bn256Mul32Signed( bn256 *dst, const bn256 * const src, int32_t value );
bnUnit bn256Mul32Check( bn256 *dst, const bn256 * const src, uint32_t value );
bnUnit bn256Mul32SignedCheck( bn256 *dst, const bn256 * const src, int32_t value );
void bn256Mul( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
bnUnit bn256MulCheck( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
bnUnit bn256MulSignedCheck( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256MulShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shift );
void bn256MulSignedShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shift );
bnUnit bn256MulCheckShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shiftbits );
bnUnit bn256MulSignedCheckShr( bn256 *dst, const bn256 * const src0, const bn256 * const src1, bnShift shiftbits );
void bn256SquareShr( bn256 *dst, const bn256 * const src, bnShift shift );

void bn256Div32( bn256 *dst, uint32_t divisor, uint32_t *rem );
void bn256Div32Signed( bn256 *dst, int32_t divisor, int32_t *rem );
void bn256Div32Round( bn256 *dst, uint32_t divisor );
void bn256Div32RoundSigned( bn256 *dst, int32_t divisor );
void bn256Div( bn256 *dst, const bn256 * const divisor, bn256 *rem );
void bn256DivSigned( bn256 *dst, const bn256 * const divisor, bn256 *rem );
void bn256DivRound( bn256 *dst, const bn256 * const divisor );
void bn256DivRoundSigned( bn256 *dst, const bn256 * const divisor );
void bn256DivShl( bn256 *dst, const bn256 * const divisor, bn256 *rem, bnShift shift );
void bn256DivSignedShl( bn256 *dst, const bn256 * const divisor, bn256 *rem, bnShift shift );
void bn256DivRoundShl( bn256 *dst, const bn256 * const divisor, bnShift shift );
void bn256DivRoundSignedShl( bn256 *dst, const bn256 * const divisor, bnShift shift );

void bn256Or( bn256 *dst, const bn256 * const src );
void bn256SetOr( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256Nor( bn256 *dst, const bn256 * const src );
void bn256SetNor( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256And( bn256 *dst, const bn256 * const src );
void bn256SetAnd( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256Nand( bn256 *dst, const bn256 * const src );
void bn256SetNand( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256Xor( bn256 *dst, const bn256 * const src );
void bn256SetXor( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256Nxor( bn256 *dst, const bn256 * const src );
void bn256SetNxor( bn256 *dst, const bn256 * const src0, const bn256 * const src1 );
void bn256Not( bn256 *dst );
void bn256SetNot( bn256 *dst, const bn256 * const src );
void bn256Neg( bn256 *dst );
void bn256SetNeg( bn256 *dst, const bn256 * const src );
void bn256Shl( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Shr( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Sal( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Sar( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256ShrRound( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256SarRound( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Shl1( bn256 *dst );
void bn256SetShl1( bn256 *dst, const bn256 * const src );
void bn256Shr1( bn256 *dst );
void bn256SetShr1( bn256 *dst, const bn256 * const src );

int bn256CmpZero( const bn256 * const src );
int bn256CmpNotZero( const bn256 * const src );
int bn256CmpEq( bn256 *dst, const bn256 * const src );
int bn256CmpNeq( bn256 *dst, const bn256 * const src );
int bn256CmpGt( bn256 *dst, const bn256 * const src );
int bn256CmpGe( bn256 *dst, const bn256 * const src );
int bn256CmpLt( bn256 *dst, const bn256 * const src );
int bn256CmpLe( bn256 *dst, const bn256 * const src );
int bn256CmpSignedGt( bn256 *dst, const bn256 * const src );
int bn256CmpSignedGe( bn256 *dst, const bn256 * const src );
int bn256CmpSignedLt( bn256 *dst, const bn256 * const src );
int bn256CmpSignedLe( bn256 *dst, const bn256 * const src );
int bn256CmpPositive( const bn256 * const src );
int bn256CmpNegative( const bn256 * const src );
int bn256CmpPart( bn256 *dst, const bn256 * const src, uint32_t bits );

int bn256ExtractBit( const bn256 * const src, int bitoffset );
uint32_t bn256Extract32( const bn256 * const src, int bitoffset );
uint64_t bn256Extract64( const bn256 * const src, int bitoffset );
int bn256GetIndexMSB( const bn256 * const src );
int bn256GetIndexMSZ( const bn256 * const src );

int bn256Print( const bn256 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn256PrintHex( const bn256 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn256PrintBin( const bn256 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
void bn256PrintOut( char *prefix, const bn256 * const src, char *suffix );
void bn256PrintHexOut( char *prefix, const bn256 * const src, char *suffix );
void bn256PrintBinOut( char *prefix, const bn256 * const src, char *suffix );
int bn256Scan( bn256 *dst, char *str, bnShift shift );

void bn256Sqrt( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Log( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Exp( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Pow( bn256 *dst, const bn256 * const src, const bn256 * const exponent, bnShift shift );
void bn256PowInt( bn256 *dst, const bn256 * const src, uint32_t exponent, bnShift shift );
void bn256Cos( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Sin( bn256 *dst, const bn256 * const src, bnShift shift );
void bn256Tan( bn256 *dst, const bn256 * const src, bnShift shift );


////


void __attribute__ ((noinline)) bn256MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask );
bnUnit __attribute__ ((noinline)) bn256AddMulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask, bnUnit midcarry );


/*

Missing functions :

abs()
max()
min()
cbrt()
log2()
exp2()
ceil()
floor()
round()

*/



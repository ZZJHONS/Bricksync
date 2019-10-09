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

#define BN_INT128_UNIT_COUNT (128/BN_UNIT_BITS)
#define BN_INT128_UNIT_COUNT_SHIFT (128>>BN_UNIT_BIT_SHIFT)


typedef struct
{
  bnUnit unit[BN_INT128_UNIT_COUNT];
} bn128;


////


void bn128Zero( bn128 *dst );
void bn128Set( bn128 *dst, const bn128 * const src );
void bn128Set32( bn128 *dst, uint32_t value );
void bn128Set32Signed( bn128 *dst, int32_t value );
void bn128Set32Shl( bn128 *dst, uint32_t value, bnShift shift );
void bn128SetDouble( bn128 *dst, double value, bnShift leftshift );
double bn128GetDouble( bn128 *dst, bnShift rightshift );

void bn128Add32( bn128 *dst, uint32_t value );
void bn128Add32Signed( bn128 *dst, int32_t value );
void bn128Add32Shl( bn128 *dst, uint32_t value, bnShift shift );
void bn128Sub32( bn128 *dst, uint32_t value );
void bn128Sub32Signed( bn128 *dst, int32_t value );
void bn128Sub32Shl( bn128 *dst, uint32_t value, bnShift shift );
void bn128Set32SignedShl( bn128 *dst, int32_t value, bnShift shift );
void bn128Add( bn128 *dst, const bn128 * const src );
void bn128SetAdd( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128Sub( bn128 *dst, const bn128 * const src );
void bn128SetSub( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128SetAddAdd( bn128 *dst, const bn128 * const src, const bn128 * const srcadd0, const bn128 * const srcadd1 );
void bn128SetAddSub( bn128 *dst, const bn128 * const src, const bn128 * const srcadd, const bn128 * const srcsub );
void bn128SetAddAddSub( bn128 *dst, const bn128 * const src, const bn128 * const srcadd0, const bn128 * const srcadd1, const bn128 * const srcsub );
void bn128SetAddAddAddSub( bn128 *dst, const bn128 * const src, const bn128 * const srcadd0, const bn128 * const srcadd1, const bn128 * const srcadd2, const bn128 * const srcsub );

void bn128Mul32( bn128 *dst, const bn128 * const src, uint32_t value );
void bn128Mul32Signed( bn128 *dst, const bn128 * const src, int32_t value );
bnUnit bn128Mul32Check( bn128 *dst, const bn128 * const src, uint32_t value );
bnUnit bn128Mul32SignedCheck( bn128 *dst, const bn128 * const src, int32_t value );
void bn128Mul( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
bnUnit bn128MulCheck( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
bnUnit bn128MulSignedCheck( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128MulShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shift );
void bn128MulSignedShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shift );
bnUnit bn128MulCheckShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shift );
bnUnit bn128MulSignedCheckShr( bn128 *dst, const bn128 * const src0, const bn128 * const src1, bnShift shift );
void bn128SquareShr( bn128 *dst, const bn128 * const src, bnShift shift );

void bn128Div32( bn128 *dst, uint32_t divisor, uint32_t *rem );
void bn128Div32Signed( bn128 *dst, int32_t divisor, int32_t *rem );
void bn128Div32Round( bn128 *dst, uint32_t divisor );
void bn128Div32RoundSigned( bn128 *dst, int32_t divisor );
void bn128Div( bn128 *dst, const bn128 * const divisor, bn128 *rem );
void bn128DivSigned( bn128 *dst, const bn128 * const divisor, bn128 *rem );
void bn128DivRound( bn128 *dst, const bn128 * const divisor );
void bn128DivRoundSigned( bn128 *dst, const bn128 * const divisor );
void bn128DivShl( bn128 *dst, const bn128 * const divisor, bn128 *rem, bnShift shift );
void bn128DivSignedShl( bn128 *dst, const bn128 * const divisor, bn128 *rem, bnShift shift );
void bn128DivRoundShl( bn128 *dst, const bn128 * const divisor, bnShift shift );
void bn128DivRoundSignedShl( bn128 *dst, const bn128 * const divisor, bnShift shift );

void bn128Or( bn128 *dst, const bn128 * const src );
void bn128SetOr( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128Nor( bn128 *dst, const bn128 * const src );
void bn128SetNor( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128And( bn128 *dst, const bn128 * const src );
void bn128SetAnd( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128Nand( bn128 *dst, const bn128 * const src );
void bn128SetNand( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128Xor( bn128 *dst, const bn128 * const src );
void bn128SetXor( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128Nxor( bn128 *dst, const bn128 * const src );
void bn128SetNxor( bn128 *dst, const bn128 * const src0, const bn128 * const src1 );
void bn128Not( bn128 *dst );
void bn128SetNot( bn128 *dst, const bn128 * const src );
void bn128Neg( bn128 *dst );
void bn128SetNeg( bn128 *dst, const bn128 * const src );
void bn128Shl( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Shr( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Sal( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Sar( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128ShrRound( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128SarRound( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Shl1( bn128 *dst );
void bn128SetShl1( bn128 *dst, const bn128 * const src );
void bn128Shr1( bn128 *dst );
void bn128SetShr1( bn128 *dst, const bn128 * const src );

int bn128CmpZero( const bn128 * const src );
int bn128CmpNotZero( const bn128 * const src );
int bn128CmpEq( bn128 *dst, const bn128 * const src );
int bn128CmpNeq( bn128 *dst, const bn128 * const src );
int bn128CmpGt( bn128 *dst, const bn128 * const src );
int bn128CmpGe( bn128 *dst, const bn128 * const src );
int bn128CmpLt( bn128 *dst, const bn128 * const src );
int bn128CmpLe( bn128 *dst, const bn128 * const src );
int bn128CmpSignedGt( bn128 *dst, const bn128 * const src );
int bn128CmpSignedGe( bn128 *dst, const bn128 * const src );
int bn128CmpSignedLt( bn128 *dst, const bn128 * const src );
int bn128CmpSignedLe( bn128 *dst, const bn128 * const src );
int bn128CmpPositive( const bn128 * const src );
int bn128CmpNegative( const bn128 * const src );
int bn128CmpPart( bn128 *dst, const bn128 * const src, uint32_t bits );

int bn128ExtractBit( const bn128 * const src, int bitoffset );
uint32_t bn128Extract32( const bn128 * const src, int bitoffset );
uint64_t bn128Extract64( const bn128 * const src, int bitoffset );
int bn128GetIndexMSB( const bn128 * const src );
int bn128GetIndexMSZ( const bn128 * const src );

int bn128Print( const bn128 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn128PrintHex( const bn128 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn128PrintBin( const bn128 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
void bn128PrintOut( char *prefix, const bn128 * const src, char *suffix );
void bn128PrintHexOut( char *prefix, const bn128 * const src, char *suffix );
void bn128PrintBinOut( char *prefix, const bn128 * const src, char *suffix );
int bn128Scan( bn128 *dst, char *str, bnShift shift );

void bn128Sqrt( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Log( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Exp( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Pow( bn128 *dst, const bn128 * const src, const bn128 * const exponent, bnShift shift );
void bn128PowInt( bn128 *dst, const bn128 * const src, uint32_t exponent, bnShift shift );
void bn128Cos( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Sin( bn128 *dst, const bn128 * const src, bnShift shift );
void bn128Tan( bn128 *dst, const bn128 * const src, bnShift shift );


////


void bn128MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask );


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



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

#define BN_INT192_UNIT_COUNT (192/BN_UNIT_BITS)
#define BN_INT192_UNIT_COUNT_SHIFT (192>>BN_UNIT_BIT_SHIFT)


typedef struct
{
  bnUnit unit[BN_INT192_UNIT_COUNT];
} bn192;


////


void bn192Zero( bn192 *dst );
void bn192Set( bn192 *dst, const bn192 * const src );
void bn192Set32( bn192 *dst, uint32_t value );
void bn192Set32Signed( bn192 *dst, int32_t value );
void bn192Set32Shl( bn192 *dst, uint32_t value, bnShift shift );
void bn192SetDouble( bn192 *dst, double value, bnShift leftshift );
double bn192GetDouble( bn192 *dst, bnShift rightshift );

void bn192Add32( bn192 *dst, uint32_t value );
void bn192Add32Signed( bn192 *dst, int32_t value );
void bn192Add32Shl( bn192 *dst, uint32_t value, bnShift shift );
void bn192Sub32( bn192 *dst, uint32_t value );
void bn192Sub32Signed( bn192 *dst, int32_t value );
void bn192Sub32Shl( bn192 *dst, uint32_t value, bnShift shift );
void bn192Set32SignedShl( bn192 *dst, int32_t value, bnShift shift );
void bn192Add( bn192 *dst, const bn192 * const src );
void bn192SetAdd( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192Sub( bn192 *dst, const bn192 * const src );
void bn192SetSub( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192SetAddAdd( bn192 *dst, const bn192 * const src, const bn192 * const srcadd0, const bn192 * const srcadd1 );
void bn192SetAddSub( bn192 *dst, const bn192 * const src, const bn192 * const srcadd, const bn192 * const srcsub );
void bn192SetAddAddSub( bn192 *dst, const bn192 * const src, const bn192 * const srcadd0, const bn192 * const srcadd1, const bn192 * const srcsub );
void bn192SetAddAddAddSub( bn192 *dst, const bn192 * const src, const bn192 * const srcadd0, const bn192 * const srcadd1, const bn192 * const srcadd2, const bn192 * const srcsub );

void bn192Mul32( bn192 *dst, const bn192 * const src, uint32_t value );
void bn192Mul32Signed( bn192 *dst, const bn192 * const src, int32_t value );
bnUnit bn192Mul32Check( bn192 *dst, const bn192 * const src, uint32_t value );
bnUnit bn192Mul32SignedCheck( bn192 *dst, const bn192 * const src, int32_t value );
void bn192Mul( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
bnUnit bn192MulCheck( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
bnUnit bn192MulSignedCheck( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192MulShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shift );
void bn192MulSignedShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shift );
bnUnit bn192MulCheckShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shift );
bnUnit bn192MulSignedCheckShr( bn192 *dst, const bn192 * const src0, const bn192 * const src1, bnShift shift );
void bn192SquareShr( bn192 *dst, const bn192 * const src, bnShift shift );

void bn192Div32( bn192 *dst, uint32_t divisor, uint32_t *rem );
void bn192Div32Signed( bn192 *dst, int32_t divisor, int32_t *rem );
void bn192Div32Round( bn192 *dst, uint32_t divisor );
void bn192Div32RoundSigned( bn192 *dst, int32_t divisor );
void bn192Div( bn192 *dst, const bn192 * const divisor, bn192 *rem );
void bn192DivSigned( bn192 *dst, const bn192 * const divisor, bn192 *rem );
void bn192DivRound( bn192 *dst, const bn192 * const divisor );
void bn192DivRoundSigned( bn192 *dst, const bn192 * const divisor );
void bn192DivShl( bn192 *dst, const bn192 * const divisor, bn192 *rem, bnShift shift );
void bn192DivSignedShl( bn192 *dst, const bn192 * const divisor, bn192 *rem, bnShift shift );
void bn192DivRoundShl( bn192 *dst, const bn192 * const divisor, bnShift shift );
void bn192DivRoundSignedShl( bn192 *dst, const bn192 * const divisor, bnShift shift );

void bn192Or( bn192 *dst, const bn192 * const src );
void bn192SetOr( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192Nor( bn192 *dst, const bn192 * const src );
void bn192SetNor( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192And( bn192 *dst, const bn192 * const src );
void bn192SetAnd( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192Nand( bn192 *dst, const bn192 * const src );
void bn192SetNand( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192Xor( bn192 *dst, const bn192 * const src );
void bn192SetXor( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192Nxor( bn192 *dst, const bn192 * const src );
void bn192SetNxor( bn192 *dst, const bn192 * const src0, const bn192 * const src1 );
void bn192Not( bn192 *dst );
void bn192SetNot( bn192 *dst, const bn192 * const src );
void bn192Neg( bn192 *dst );
void bn192SetNeg( bn192 *dst, const bn192 * const src );
void bn192Shl( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Shr( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Sal( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Sar( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192ShrRound( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192SarRound( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Shl1( bn192 *dst );
void bn192SetShl1( bn192 *dst, const bn192 * const src );
void bn192Shr1( bn192 *dst );
void bn192SetShr1( bn192 *dst, const bn192 * const src );

int bn192CmpZero( const bn192 * const src );
int bn192CmpNotZero( const bn192 * const src );
int bn192CmpEq( bn192 *dst, const bn192 * const src );
int bn192CmpNeq( bn192 *dst, const bn192 * const src );
int bn192CmpGt( bn192 *dst, const bn192 * const src );
int bn192CmpGe( bn192 *dst, const bn192 * const src );
int bn192CmpLt( bn192 *dst, const bn192 * const src );
int bn192CmpLe( bn192 *dst, const bn192 * const src );
int bn192CmpSignedGt( bn192 *dst, const bn192 * const src );
int bn192CmpSignedGe( bn192 *dst, const bn192 * const src );
int bn192CmpSignedLt( bn192 *dst, const bn192 * const src );
int bn192CmpSignedLe( bn192 *dst, const bn192 * const src );
int bn192CmpPositive( const bn192 * const src );
int bn192CmpNegative( const bn192 * const src );
int bn192CmpPart( bn192 *dst, const bn192 * const src, uint32_t bits );

int bn192ExtractBit( const bn192 * const src, int bitoffset );
uint32_t bn192Extract32( const bn192 * const src, int bitoffset );
uint64_t bn192Extract64( const bn192 * const src, int bitoffset );
int bn192GetIndexMSB( const bn192 * const src );
int bn192GetIndexMSZ( const bn192 * const src );

int bn192Print( const bn192 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn192PrintHex( const bn192 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn192PrintBin( const bn192 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
void bn192PrintOut( char *prefix, const bn192 * const src, char *suffix );
void bn192PrintHexOut( char *prefix, const bn192 * const src, char *suffix );
void bn192PrintBinOut( char *prefix, const bn192 * const src, char *suffix );
int bn192Scan( bn192 *dst, char *str, bnShift shift );

void bn192Sqrt( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Log( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Exp( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Pow( bn192 *dst, const bn192 * const src, const bn192 * const exponent, bnShift shift );
void bn192PowInt( bn192 *dst, const bn192 * const src, uint32_t exponent, bnShift shift );
void bn192Cos( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Sin( bn192 *dst, const bn192 * const src, bnShift shift );
void bn192Tan( bn192 *dst, const bn192 * const src, bnShift shift );


////


void __attribute__ ((noinline)) bn192MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask );


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



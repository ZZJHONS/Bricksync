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

#define BN_INT1024_UNIT_COUNT (1024/BN_UNIT_BITS)
#define BN_INT1024_UNIT_COUNT_SHIFT (1024>>BN_UNIT_BIT_SHIFT)


typedef struct
{
  bnUnit unit[BN_INT1024_UNIT_COUNT];
} bn1024;


////


void bn1024Zero( bn1024 *dst );
void bn1024Set( bn1024 *dst, const bn1024 * const src );
void bn1024Set32( bn1024 *dst, uint32_t value );
void bn1024Set32Signed( bn1024 *dst, int32_t value );
void bn1024Set32Shl( bn1024 *dst, uint32_t value, bnShift shift );
void bn1024SetDouble( bn1024 *dst, double value, bnShift leftshift );
double bn1024GetDouble( bn1024 *dst, bnShift rightshift );
void bn1024SetBit( bn1024 *dst, bnShift shift );
void bn1024ClearBit( bn1024 *dst, bnShift shift );
void bn1024FlipBit( bn1024 *dst, bnShift shift );

void bn1024Add32( bn1024 *dst, uint32_t value );
void bn1024Add32Signed( bn1024 *dst, int32_t value );
void bn1024Add32Shl( bn1024 *dst, uint32_t value, bnShift shift );
void bn1024Sub32( bn1024 *dst, uint32_t value );
void bn1024Sub32Signed( bn1024 *dst, int32_t value );
void bn1024Sub32Shl( bn1024 *dst, uint32_t value, bnShift shift );
void bn1024Set32SignedShl( bn1024 *dst, int32_t value, bnShift shift );
void bn1024Add( bn1024 *dst, const bn1024 * const src );
void bn1024SetAdd( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024Sub( bn1024 *dst, const bn1024 * const src );
void bn1024SetSub( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024SetAddAdd( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd0, const bn1024 * const srcadd1 );
void bn1024SetAddSub( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd, const bn1024 * const srcsub );
void bn1024SetAddAddSub( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd0, const bn1024 * const srcadd1, const bn1024 * const srcsub );
void bn1024SetAddAddAddSub( bn1024 *dst, const bn1024 * const src, const bn1024 * const srcadd0, const bn1024 * const srcadd1, const bn1024 * const srcadd2, const bn1024 * const srcsub );

void bn1024Mul32( bn1024 *dst, const bn1024 * const src, uint32_t value );
void bn1024Mul32Signed( bn1024 *dst, const bn1024 * const src, int32_t value );
bnUnit bn1024Mul32Check( bn1024 *dst, const bn1024 * const src, uint32_t value );
bnUnit bn1024Mul32SignedCheck( bn1024 *dst, const bn1024 * const src, int32_t value );
void bn1024Mul( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
bnUnit bn1024MulCheck( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
bnUnit bn1024MulSignedCheck( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024MulShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shift );
void bn1024MulSignedShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shift );
bnUnit bn1024MulCheckShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shiftbits );
bnUnit bn1024MulSignedCheckShr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1, bnShift shiftbits );
void bn1024SquareShr( bn1024 *dst, const bn1024 * const src, bnShift shift );

void bn1024Div32( bn1024 *dst, uint32_t divisor, uint32_t *rem );
void bn1024Div32Signed( bn1024 *dst, int32_t divisor, int32_t *rem );
void bn1024Div32Round( bn1024 *dst, uint32_t divisor );
void bn1024Div32RoundSigned( bn1024 *dst, int32_t divisor );
void bn1024Div( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem );
void bn1024DivSigned( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem );
void bn1024DivRound( bn1024 *dst, const bn1024 * const divisor );
void bn1024DivRoundSigned( bn1024 *dst, const bn1024 * const divisor );
void bn1024DivShl( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem, bnShift shift );
void bn1024DivSignedShl( bn1024 *dst, const bn1024 * const divisor, bn1024 *rem, bnShift shift );
void bn1024DivRoundShl( bn1024 *dst, const bn1024 * const divisor, bnShift shift );
void bn1024DivRoundSignedShl( bn1024 *dst, const bn1024 * const divisor, bnShift shift );

void bn1024Or( bn1024 *dst, const bn1024 * const src );
void bn1024SetOr( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024Nor( bn1024 *dst, const bn1024 * const src );
void bn1024SetNor( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024And( bn1024 *dst, const bn1024 * const src );
void bn1024SetAnd( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024Nand( bn1024 *dst, const bn1024 * const src );
void bn1024SetNand( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024Xor( bn1024 *dst, const bn1024 * const src );
void bn1024SetXor( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024Nxor( bn1024 *dst, const bn1024 * const src );
void bn1024SetNxor( bn1024 *dst, const bn1024 * const src0, const bn1024 * const src1 );
void bn1024Not( bn1024 *dst );
void bn1024SetNot( bn1024 *dst, const bn1024 * const src );
void bn1024Neg( bn1024 *dst );
void bn1024SetNeg( bn1024 *dst, const bn1024 * const src );
void bn1024Shl( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Shr( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Sal( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Sar( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024ShrRound( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024SarRound( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Shl1( bn1024 *dst );
void bn1024SetShl1( bn1024 *dst, const bn1024 * const src );
void bn1024Shr1( bn1024 *dst );
void bn1024SetShr1( bn1024 *dst, const bn1024 * const src );

int bn1024CmpZero( const bn1024 * const src );
int bn1024CmpNotZero( const bn1024 * const src );
int bn1024CmpEq( bn1024 *dst, const bn1024 * const src );
int bn1024CmpNeq( bn1024 *dst, const bn1024 * const src );
int bn1024CmpGt( bn1024 *dst, const bn1024 * const src );
int bn1024CmpGe( bn1024 *dst, const bn1024 * const src );
int bn1024CmpLt( bn1024 *dst, const bn1024 * const src );
int bn1024CmpLe( bn1024 *dst, const bn1024 * const src );
int bn1024CmpSignedGt( bn1024 *dst, const bn1024 * const src );
int bn1024CmpSignedGe( bn1024 *dst, const bn1024 * const src );
int bn1024CmpSignedLt( bn1024 *dst, const bn1024 * const src );
int bn1024CmpSignedLe( bn1024 *dst, const bn1024 * const src );
int bn1024CmpPositive( const bn1024 * const src );
int bn1024CmpNegative( const bn1024 * const src );
int bn1024CmpPart( bn1024 *dst, const bn1024 * const src, uint32_t bits );

int bn1024ExtractBit( const bn1024 * const src, int bitoffset );
uint32_t bn1024Extract32( const bn1024 * const src, int bitoffset );
uint64_t bn1024Extract64( const bn1024 * const src, int bitoffset );
int bn1024GetIndexMSB( const bn1024 * const src );
int bn1024GetIndexMSZ( const bn1024 * const src );

int bn1024Print( const bn1024 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn1024PrintHex( const bn1024 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn1024PrintBin( const bn1024 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
void bn1024PrintOut( char *prefix, const bn1024 * const src, char *suffix );
void bn1024PrintHexOut( char *prefix, const bn1024 * const src, char *suffix );
void bn1024PrintBinOut( char *prefix, const bn1024 * const src, char *suffix );
int bn1024Scan( bn1024 *dst, char *str, bnShift shift );

void bn1024Sqrt( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Log( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Exp( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Pow( bn1024 *dst, const bn1024 * const src, const bn1024 * const exponent, bnShift shift );
void bn1024PowInt( bn1024 *dst, const bn1024 * const src, uint32_t exponent, bnShift shift );
void bn1024Cos( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Sin( bn1024 *dst, const bn1024 * const src, bnShift shift );
void bn1024Tan( bn1024 *dst, const bn1024 * const src, bnShift shift );


////


void __attribute__ ((noinline)) bn1024MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask );


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



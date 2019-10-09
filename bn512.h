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

#define BN_INT512_UNIT_COUNT (512/BN_UNIT_BITS)
#define BN_INT512_UNIT_COUNT_SHIFT (512>>BN_UNIT_BIT_SHIFT)


typedef struct
{
  bnUnit unit[BN_INT512_UNIT_COUNT];
} bn512;


////


void bn512Zero( bn512 *dst );
void bn512Set( bn512 *dst, const bn512 * const src );
void bn512Set32( bn512 *dst, uint32_t value );
void bn512Set32Signed( bn512 *dst, int32_t value );
void bn512Set32Shl( bn512 *dst, uint32_t value, bnShift shift );
void bn512SetDouble( bn512 *dst, double value, bnShift leftshift );
double bn512GetDouble( bn512 *dst, bnShift rightshift );
void bn512SetBit( bn512 *dst, bnShift shift );
void bn512ClearBit( bn512 *dst, bnShift shift );
void bn512FlipBit( bn512 *dst, bnShift shift );

void bn512Add32( bn512 *dst, uint32_t value );
void bn512Add32Signed( bn512 *dst, int32_t value );
void bn512Add32Shl( bn512 *dst, uint32_t value, bnShift shift );
void bn512Sub32( bn512 *dst, uint32_t value );
void bn512Sub32Signed( bn512 *dst, int32_t value );
void bn512Sub32Shl( bn512 *dst, uint32_t value, bnShift shift );
void bn512Set32SignedShl( bn512 *dst, int32_t value, bnShift shift );
void bn512Add( bn512 *dst, const bn512 * const src );
void bn512SetAdd( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512Sub( bn512 *dst, const bn512 * const src );
void bn512SetSub( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512SetAddAdd( bn512 *dst, const bn512 * const src, const bn512 * const srcadd0, const bn512 * const srcadd1 );
void bn512SetAddSub( bn512 *dst, const bn512 * const src, const bn512 * const srcadd, const bn512 * const srcsub );
void bn512SetAddAddSub( bn512 *dst, const bn512 * const src, const bn512 * const srcadd0, const bn512 * const srcadd1, const bn512 * const srcsub );
void bn512SetAddAddAddSub( bn512 *dst, const bn512 * const src, const bn512 * const srcadd0, const bn512 * const srcadd1, const bn512 * const srcadd2, const bn512 * const srcsub );

void bn512Mul32( bn512 *dst, const bn512 * const src, uint32_t value );
void bn512Mul32Signed( bn512 *dst, const bn512 * const src, int32_t value );
bnUnit bn512Mul32Check( bn512 *dst, const bn512 * const src, uint32_t value );
bnUnit bn512Mul32SignedCheck( bn512 *dst, const bn512 * const src, int32_t value );
void bn512Mul( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
bnUnit bn512MulCheck( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
bnUnit bn512MulSignedCheck( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512MulShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shift );
void bn512MulSignedShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shift );
bnUnit bn512MulCheckShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shiftbits );
bnUnit bn512MulSignedCheckShr( bn512 *dst, const bn512 * const src0, const bn512 * const src1, bnShift shiftbits );
void bn512SquareShr( bn512 *dst, const bn512 * const src, bnShift shift );

void bn512Div32( bn512 *dst, uint32_t divisor, uint32_t *rem );
void bn512Div32Signed( bn512 *dst, int32_t divisor, int32_t *rem );
void bn512Div32Round( bn512 *dst, uint32_t divisor );
void bn512Div32RoundSigned( bn512 *dst, int32_t divisor );
void bn512Div( bn512 *dst, const bn512 * const divisor, bn512 *rem );
void bn512DivSigned( bn512 *dst, const bn512 * const divisor, bn512 *rem );
void bn512DivRound( bn512 *dst, const bn512 * const divisor );
void bn512DivRoundSigned( bn512 *dst, const bn512 * const divisor );
void bn512DivShl( bn512 *dst, const bn512 * const divisor, bn512 *rem, bnShift shift );
void bn512DivSignedShl( bn512 *dst, const bn512 * const divisor, bn512 *rem, bnShift shift );
void bn512DivRoundShl( bn512 *dst, const bn512 * const divisor, bnShift shift );
void bn512DivRoundSignedShl( bn512 *dst, const bn512 * const divisor, bnShift shift );

void bn512Or( bn512 *dst, const bn512 * const src );
void bn512SetOr( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512Nor( bn512 *dst, const bn512 * const src );
void bn512SetNor( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512And( bn512 *dst, const bn512 * const src );
void bn512SetAnd( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512Nand( bn512 *dst, const bn512 * const src );
void bn512SetNand( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512Xor( bn512 *dst, const bn512 * const src );
void bn512SetXor( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512Nxor( bn512 *dst, const bn512 * const src );
void bn512SetNxor( bn512 *dst, const bn512 * const src0, const bn512 * const src1 );
void bn512Not( bn512 *dst );
void bn512SetNot( bn512 *dst, const bn512 * const src );
void bn512Neg( bn512 *dst );
void bn512SetNeg( bn512 *dst, const bn512 * const src );
void bn512Shl( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Shr( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Sal( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Sar( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512ShrRound( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512SarRound( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Shl1( bn512 *dst );
void bn512SetShl1( bn512 *dst, const bn512 * const src );
void bn512Shr1( bn512 *dst );
void bn512SetShr1( bn512 *dst, const bn512 * const src );

int bn512CmpZero( const bn512 * const src );
int bn512CmpNotZero( const bn512 * const src );
int bn512CmpEq( bn512 *dst, const bn512 * const src );
int bn512CmpNeq( bn512 *dst, const bn512 * const src );
int bn512CmpGt( bn512 *dst, const bn512 * const src );
int bn512CmpGe( bn512 *dst, const bn512 * const src );
int bn512CmpLt( bn512 *dst, const bn512 * const src );
int bn512CmpLe( bn512 *dst, const bn512 * const src );
int bn512CmpSignedGt( bn512 *dst, const bn512 * const src );
int bn512CmpSignedGe( bn512 *dst, const bn512 * const src );
int bn512CmpSignedLt( bn512 *dst, const bn512 * const src );
int bn512CmpSignedLe( bn512 *dst, const bn512 * const src );
int bn512CmpPositive( const bn512 * const src );
int bn512CmpNegative( const bn512 * const src );
int bn512CmpPart( bn512 *dst, const bn512 * const src, uint32_t bits );

int bn512ExtractBit( const bn512 * const src, int bitoffset );
uint32_t bn512Extract32( const bn512 * const src, int bitoffset );
uint64_t bn512Extract64( const bn512 * const src, int bitoffset );
int bn512GetIndexMSB( const bn512 * const src );
int bn512GetIndexMSZ( const bn512 * const src );

int bn512Print( const bn512 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn512PrintHex( const bn512 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
int bn512PrintBin( const bn512 * const src, char *buffer, int buffersize, int signedflag, int rightshift, int fractiondigits );
void bn512PrintOut( char *prefix, const bn512 * const src, char *suffix );
void bn512PrintHexOut( char *prefix, const bn512 * const src, char *suffix );
void bn512PrintBinOut( char *prefix, const bn512 * const src, char *suffix );
int bn512Scan( bn512 *dst, char *str, bnShift shift );

void bn512Sqrt( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Log( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Exp( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Pow( bn512 *dst, const bn512 * const src, const bn512 * const exponent, bnShift shift );
void bn512PowInt( bn512 *dst, const bn512 * const src, uint32_t exponent, bnShift shift );
void bn512Cos( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Sin( bn512 *dst, const bn512 * const src, bnShift shift );
void bn512Tan( bn512 *dst, const bn512 * const src, bnShift shift );


////


void __attribute__ ((noinline)) bn512MulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask );
bnUnit __attribute__ ((noinline)) bn512AddMulExtended( bnUnit *result, const bnUnit * const src0, const bnUnit * const src1, bnUnit unitmask, bnUnit midcarry );


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



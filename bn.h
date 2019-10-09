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

#ifndef CPUCONF_WORD_SIZE
 #error "cpuconfig.h not included"
#endif


#if CPUCONF_WORD_SIZE >= 64
 #define bnUnit uint64_t
 #define bnShift int64_t
 #define bnUnitSigned int64_t
 #define BN_UNIT_BYTES (8)
 #define BN_UNIT_BITS (64)
 #define BN_UNIT_BYTE_SHIFT (3)
 #define BN_UNIT_BIT_SHIFT (6)
#else
 #define bnUnit uint32_t
 #define bnShift int32_t
 #define bnUnitSigned int32_t
 #define BN_UNIT_BYTES (4)
 #define BN_UNIT_BITS (32)
 #define BN_UNIT_BYTE_SHIFT (2)
 #define BN_UNIT_BIT_SHIFT (5)
#endif


/* It's faster to branch conditionally than add-with-carry on Intel chips, go figure. */
#ifdef CPUCONF_VENDOR_INTEL
 #define BN_BRANCH_OVER_CARRY
#endif


#define BN_UNIT_SIGNBIT_MASK (1ULL<<(BN_UNIT_BITS-1))


#define BN_FASTDIV_MAXIMUM (255)


////


#if !defined(BN_XP_SUPPORT_128) || BN_XP_SUPPORT_128
 #include "bn128.h"
#endif

#if !defined(BN_XP_SUPPORT_192) || BN_XP_SUPPORT_192
 #include "bn192.h"
#endif

#if !defined(BN_XP_SUPPORT_256) || BN_XP_SUPPORT_256
 #include "bn256.h"
#endif

#if !defined(BN_XP_SUPPORT_512) || BN_XP_SUPPORT_512
 #include "bn512.h"
#endif

#if !defined(BN_XP_SUPPORT_1024) || BN_XP_SUPPORT_1024
 #include "bn1024.h"
#endif


////


#if ( !defined(BN_XP_SUPPORT_128) || BN_XP_SUPPORT_128 ) && ( !defined(BN_XP_SUPPORT_256) || BN_XP_SUPPORT_256 )

static inline void bn256Set128( bn256 *dst256, bn128 *src128 )
{
  int unitindex;
  bnUnit signextend;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst256->unit[unitindex] = src128->unit[unitindex];
  signextend = 0;
  if( src128->unit[BN_INT128_UNIT_COUNT-1] & ( (bnUnit)1 << (BN_UNIT_BITS-1) ) )
    signextend = -1;
  for( ; unitindex < BN_INT256_UNIT_COUNT ; unitindex++ )
    dst256->unit[unitindex] = signextend;
  return;
}

static inline void bn128Set256( bn128 *dst128, bn256 *src256 )
{
  int unitindex;
  for( unitindex = 0 ; unitindex < BN_INT128_UNIT_COUNT ; unitindex++ )
    dst128->unit[unitindex] = src256->unit[unitindex];
  return;
}

#endif


////


#if BN_XP_BITS == 128

#define bnXp bn128
#define bnXpZero bn128Zero
#define bnXpSet bn128Set
#define bnXpSet32 bn128Set32
#define bnXpSet32Signed bn128Set32Signed
#define bnXpSet32Shl bn128Set32Shl
#define bnXpSetDouble bn128SetDouble
#define bnXpGetDouble bn128GetDouble

#define bnXpAdd32 bn128Add32
#define bnXpAdd32Signed bn128Add32Signed
#define bnXpAdd32Shl bn128Add32Shl
#define bnXpSub32 bn128Sub32
#define bnXpSub32Signed bn128Sub32Signed
#define bnXpSub32Shl bn128Sub32Shl
#define bnXpAdd bn128Add
#define bnXpSetAdd bn128SetAdd
#define bnXpSub bn128Sub
#define bnXpSetSub bn128SetSub
#define bnXpSetAddAdd bn128SetAddAdd
#define bnXpSetAddSub bn128SetAddSub
#define bnXpSetAddAddSub bn128SetAddAddSub
#define bnXpSetAddAddAddSub bn128SetAddAddAddSub

#define bnXpMul32 bn128Mul32
#define bnXpMul32Signed bn128Mul32Signed
#define bnXpMul32Check bn128Mul32Check
#define bnXpMul32SignedCheck bn128Mul32SignedCheck
#define bnXpMul bn128Mul
#define bnXpMulCheck bn128MulCheck
#define bnXpMulSignedCheck bn128MulSignedCheck
#define bnXpMulShr bn128MulShr
#define bnXpMulSignedShr bn128MulSignedShr
#define bnXpMulCheckShr bn128MulCheckShr
#define bnXpMulSignedCheckShr bn128MulSignedCheckShr
#define bnXpSquareShr bn128SquareShr

#define bnXpDiv32 bn128Div32
#define bnXpDiv32Signed bn128Div32Signed
#define bnXpDiv32Round bn128Div32Round
#define bnXpDiv32RoundSigned bn128Div32RoundSigned
#define bnXpDiv bn128Div
#define bnXpDivSigned bn128DivSigned
#define bnXpDivRound bn128DivRound
#define bnXpDivRoundSigned bn128DivRoundSigned
#define bnXpDivShl bn128DivShl
#define bnXpDivSignedShl bn128DivSignedShl
#define bnXpDivRoundShl bn128DivRoundShl
#define bnXpDivRoundSignedShl bn128DivRoundSignedShl

#define bnXpOr bn128Or
#define bnXpNor bn128Nor
#define bnXpAnd bn128And
#define bnXpNand bn128Nand
#define bnXpNot bn128Not
#define bnXpSetNot bn128SetNot
#define bnXpNeg bn128Neg
#define bnXpSetNeg bn128SetNeg
#define bnXpShl bn128Shl
#define bnXpShr bn128Shr
#define bnXpSal bn128Sal
#define bnXpSar bn128Sar
#define bnXpShrRound bn128ShrRound
#define bnXpSarRound bn128SarRound
#define bnXpShl1 bn128Shl1
#define bnXpSetShl1 bn128SetShl1
#define bnXpShr1 bn128Shr1
#define bnXpSetShr1 bn128SetShr1

#define bnXpCmpZero bn128CmpZero
#define bnXpCmpNotZero bn128CmpNotZero
#define bnXpCmpEq bn128CmpEq
#define bnXpCmpNeq bn128CmpNeq
#define bnXpCmpGt bn128CmpGt
#define bnXpCmpGe bn128CmpGe
#define bnXpCmpLt bn128CmpLt
#define bnXpCmpLe bn128CmpLe
#define bnXpCmpSignedGt bn128CmpSignedGt
#define bnXpCmpSignedGe bn128CmpSignedGe
#define bnXpCmpSignedLt bn128CmpSignedLt
#define bnXpCmpSignedLe bn128CmpSignedLe
#define bnXpCmpPositive bn128CmpPositive
#define bnXpCmpNegative bn128CmpNegative
#define bnXpCmpPart bn128CmpPart

#define bnXpExtractBit bn128ExtractBit
#define bnXpExtract32 bn128Extract32
#define bnXpExtract64 bn128Extract64
#define bnXpGetIndexMSB bn128GetIndexMSB

#define bnXpPrint bn128Print
#define bnXpPrintHex bn128PrintHex
#define bnXpPrintBin bn128PrintBin
#define bnXpPrintOut bn128PrintOut
#define bnXpPrintHexOut bn128PrintHexOut
#define bnXpPrintBinOut bn128PrintBinOut
#define bnXpScan bn128Scan

#define bnXpSqrt bn128Sqrt
#define bnXpLog bn128Log
#define bnXpExp bn128Exp
#define bnXpPow bn128Pow
#define bnXpPowInt bn128PowInt
#define bnXpCos bn128Cos
#define bnXpSin bn128Sin
#define bnXpTan bn128Tan

#elif BN_XP_BITS == 192

#define bnXp bn192
#define bnXpZero bn192Zero
#define bnXpSet bn192Set
#define bnXpSet32 bn192Set32
#define bnXpSet32Signed bn192Set32Signed
#define bnXpSet32Shl bn192Set32Shl
#define bnXpSetDouble bn192SetDouble
#define bnXpGetDouble bn192GetDouble

#define bnXpAdd32 bn192Add32
#define bnXpAdd32Signed bn192Add32Signed
#define bnXpAdd32Shl bn192Add32Shl
#define bnXpSub32 bn192Sub32
#define bnXpSub32Signed bn192Sub32Signed
#define bnXpSub32Shl bn192Sub32Shl
#define bnXpAdd bn192Add
#define bnXpSetAdd bn192SetAdd
#define bnXpSub bn192Sub
#define bnXpSetSub bn192SetSub
#define bnXpSetAddAdd bn192SetAddAdd
#define bnXpSetAddSub bn192SetAddSub
#define bnXpSetAddAddSub bn192SetAddAddSub
#define bnXpSetAddAddAddSub bn192SetAddAddAddSub

#define bnXpMul32 bn192Mul32
#define bnXpMul32Signed bn192Mul32Signed
#define bnXpMul32Check bn192Mul32Check
#define bnXpMul32SignedCheck bn192Mul32SignedCheck
#define bnXpMul bn192Mul
#define bnXpMulCheck bn192MulCheck
#define bnXpMulSignedCheck bn192MulSignedCheck
#define bnXpMulShr bn192MulShr
#define bnXpMulSignedShr bn192MulSignedShr
#define bnXpMulCheckShr bn192MulCheckShr
#define bnXpMulSignedCheckShr bn192MulSignedCheckShr
#define bnXpSquareShr bn192SquareShr

#define bnXpDiv32 bn192Div32
#define bnXpDiv32Signed bn192Div32Signed
#define bnXpDiv32Round bn192Div32Round
#define bnXpDiv32RoundSigned bn192Div32RoundSigned
#define bnXpDiv bn192Div
#define bnXpDivSigned bn192DivSigned
#define bnXpDivRound bn192DivRound
#define bnXpDivRoundSigned bn192DivRoundSigned
#define bnXpDivShl bn192DivShl
#define bnXpDivSignedShl bn192DivSignedShl
#define bnXpDivRoundShl bn192DivRoundShl
#define bnXpDivRoundSignedShl bn192DivRoundSignedShl

#define bnXpOr bn192Or
#define bnXpNor bn192Nor
#define bnXpAnd bn192And
#define bnXpNand bn192Nand
#define bnXpNot bn192Not
#define bnXpSetNot bn192SetNot
#define bnXpNeg bn192Neg
#define bnXpSetNeg bn192SetNeg
#define bnXpShl bn192Shl
#define bnXpShr bn192Shr
#define bnXpSal bn192Sal
#define bnXpSar bn192Sar
#define bnXpShrRound bn192ShrRound
#define bnXpSarRound bn192SarRound
#define bnXpShl1 bn192Shl1
#define bnXpSetShl1 bn192SetShl1
#define bnXpShr1 bn192Shr1
#define bnXpSetShr1 bn192SetShr1

#define bnXpCmpZero bn192CmpZero
#define bnXpCmpNotZero bn192CmpNotZero
#define bnXpCmpEq bn192CmpEq
#define bnXpCmpNeq bn192CmpNeq
#define bnXpCmpGt bn192CmpGt
#define bnXpCmpGe bn192CmpGe
#define bnXpCmpLt bn192CmpLt
#define bnXpCmpLe bn192CmpLe
#define bnXpCmpSignedGt bn192CmpSignedGt
#define bnXpCmpSignedGe bn192CmpSignedGe
#define bnXpCmpSignedLt bn192CmpSignedLt
#define bnXpCmpSignedLe bn192CmpSignedLe
#define bnXpCmpPositive bn192CmpPositive
#define bnXpCmpNegative bn192CmpNegative
#define bnXpCmpPart bn192CmpPart

#define bnXpExtractBit bn192ExtractBit
#define bnXpExtract32 bn192Extract32
#define bnXpExtract64 bn192Extract64
#define bnXpGetIndexMSB bn192GetIndexMSB

#define bnXpPrint bn192Print
#define bnXpPrintHex bn192PrintHex
#define bnXpPrintBin bn192PrintBin
#define bnXpPrintOut bn192PrintOut
#define bnXpPrintHexOut bn192PrintHexOut
#define bnXpPrintBinOut bn192PrintBinOut
#define bnXpScan bn192Scan

#define bnXpSqrt bn192Sqrt
#define bnXpLog bn192Log
#define bnXpExp bn192Exp
#define bnXpPow bn192Pow
#define bnXpPowInt bn192PowInt
#define bnXpCos bn192Cos
#define bnXpSin bn192Sin
#define bnXpTan bn192Tan

#elif BN_XP_BITS == 256

#define bnXp bn256
#define bnXpZero bn256Zero
#define bnXpSet bn256Set
#define bnXpSet32 bn256Set32
#define bnXpSet32Signed bn256Set32Signed
#define bnXpSet32Shl bn256Set32Shl
#define bnXpSetDouble bn256SetDouble
#define bnXpGetDouble bn256GetDouble

#define bnXpAdd32 bn256Add32
#define bnXpAdd32Signed bn256Add32Signed
#define bnXpAdd32Shl bn256Add32Shl
#define bnXpSub32 bn256Sub32
#define bnXpSub32Signed bn256Sub32Signed
#define bnXpSub32Shl bn256Sub32Shl
#define bnXpAdd bn256Add
#define bnXpSetAdd bn256SetAdd
#define bnXpSub bn256Sub
#define bnXpSetSub bn256SetSub
#define bnXpSetAddAdd bn256SetAddAdd
#define bnXpSetAddSub bn256SetAddSub
#define bnXpSetAddAddSub bn256SetAddAddSub
#define bnXpSetAddAddAddSub bn256SetAddAddAddSub

#define bnXpMul32 bn256Mul32
#define bnXpMul32Signed bn256Mul32Signed
#define bnXpMul32Check bn256Mul32Check
#define bnXpMul32SignedCheck bn256Mul32SignedCheck
#define bnXpMul bn256Mul
#define bnXpMulCheck bn256MulCheck
#define bnXpMulSignedCheck bn256MulSignedCheck
#define bnXpMulShr bn256MulShr
#define bnXpMulSignedShr bn256MulSignedShr
#define bnXpMulCheckShr bn256MulCheckShr
#define bnXpMulSignedCheckShr bn256MulSignedCheckShr
#define bnXpSquareShr bn256SquareShr

#define bnXpDiv32 bn256Div32
#define bnXpDiv32Signed bn256Div32Signed
#define bnXpDiv32Round bn256Div32Round
#define bnXpDiv32RoundSigned bn256Div32RoundSigned
#define bnXpDiv bn256Div
#define bnXpDivSigned bn256DivSigned
#define bnXpDivRound bn256DivRound
#define bnXpDivRoundSigned bn256DivRoundSigned
#define bnXpDivShl bn256DivShl
#define bnXpDivSignedShl bn256DivSignedShl
#define bnXpDivRoundShl bn256DivRoundShl
#define bnXpDivRoundSignedShl bn256DivRoundSignedShl

#define bnXpOr bn256Or
#define bnXpNor bn256Nor
#define bnXpAnd bn256And
#define bnXpNand bn256Nand
#define bnXpNot bn256Not
#define bnXpSetNot bn256SetNot
#define bnXpNeg bn256Neg
#define bnXpSetNeg bn256SetNeg
#define bnXpShl bn256Shl
#define bnXpShr bn256Shr
#define bnXpSal bn256Sal
#define bnXpSar bn256Sar
#define bnXpShrRound bn256ShrRound
#define bnXpSarRound bn256SarRound
#define bnXpShl1 bn256Shl1
#define bnXpSetShl1 bn256SetShl1
#define bnXpShr1 bn256Shr1
#define bnXpSetShr1 bn256SetShr1

#define bnXpCmpZero bn256CmpZero
#define bnXpCmpNotZero bn256CmpNotZero
#define bnXpCmpEq bn256CmpEq
#define bnXpCmpNeq bn256CmpNeq
#define bnXpCmpGt bn256CmpGt
#define bnXpCmpGe bn256CmpGe
#define bnXpCmpLt bn256CmpLt
#define bnXpCmpLe bn256CmpLe
#define bnXpCmpSignedGt bn256CmpSignedGt
#define bnXpCmpSignedGe bn256CmpSignedGe
#define bnXpCmpSignedLt bn256CmpSignedLt
#define bnXpCmpSignedLe bn256CmpSignedLe
#define bnXpCmpPositive bn256CmpPositive
#define bnXpCmpNegative bn256CmpNegative
#define bnXpCmpPart bn256CmpPart

#define bnXpExtractBit bn256ExtractBit
#define bnXpExtract32 bn256Extract32
#define bnXpExtract64 bn256Extract64
#define bnXpGetIndexMSB bn256GetIndexMSB

#define bnXpPrint bn256Print
#define bnXpPrintHex bn256PrintHex
#define bnXpPrintBin bn256PrintBin
#define bnXpPrintOut bn256PrintOut
#define bnXpPrintHexOut bn256PrintHexOut
#define bnXpPrintBinOut bn256PrintBinOut
#define bnXpScan bn256Scan

#define bnXpSqrt bn256Sqrt
#define bnXpLog bn256Log
#define bnXpExp bn256Exp
#define bnXpPow bn256Pow
#define bnXpPowInt bn256PowInt
#define bnXpCos bn256Cos
#define bnXpSin bn256Sin
#define bnXpTan bn256Tan

#elif BN_XP_BITS == 512

#define bnXp bn512
#define bnXpZero bn512Zero
#define bnXpSet bn512Set
#define bnXpSet32 bn512Set32
#define bnXpSet32Signed bn512Set32Signed
#define bnXpSet32Shl bn512Set32Shl
#define bnXpSetDouble bn512SetDouble
#define bnXpGetDouble bn512GetDouble

#define bnXpAdd32 bn512Add32
#define bnXpAdd32Signed bn512Add32Signed
#define bnXpAdd32Shl bn512Add32Shl
#define bnXpSub32 bn512Sub32
#define bnXpSub32Signed bn512Sub32Signed
#define bnXpSub32Shl bn512Sub32Shl
#define bnXpAdd bn512Add
#define bnXpSetAdd bn512SetAdd
#define bnXpSub bn512Sub
#define bnXpSetSub bn512SetSub
#define bnXpSetAddAdd bn512SetAddAdd
#define bnXpSetAddSub bn512SetAddSub
#define bnXpSetAddAddSub bn512SetAddAddSub
#define bnXpSetAddAddAddSub bn512SetAddAddAddSub

#define bnXpMul32 bn512Mul32
#define bnXpMul32Signed bn512Mul32Signed
#define bnXpMul32Check bn512Mul32Check
#define bnXpMul32SignedCheck bn512Mul32SignedCheck
#define bnXpMul bn512Mul
#define bnXpMulCheck bn512MulCheck
#define bnXpMulSignedCheck bn512MulSignedCheck
#define bnXpMulShr bn512MulShr
#define bnXpMulSignedShr bn512MulSignedShr
#define bnXpMulCheckShr bn512MulCheckShr
#define bnXpMulSignedCheckShr bn512MulSignedCheckShr
#define bnXpSquareShr bn512SquareShr

#define bnXpDiv32 bn512Div32
#define bnXpDiv32Signed bn512Div32Signed
#define bnXpDiv32Round bn512Div32Round
#define bnXpDiv32RoundSigned bn512Div32RoundSigned
#define bnXpDiv bn512Div
#define bnXpDivSigned bn512DivSigned
#define bnXpDivRound bn512DivRound
#define bnXpDivRoundSigned bn512DivRoundSigned
#define bnXpDivShl bn512DivShl
#define bnXpDivSignedShl bn512DivSignedShl
#define bnXpDivRoundShl bn512DivRoundShl
#define bnXpDivRoundSignedShl bn512DivRoundSignedShl

#define bnXpOr bn512Or
#define bnXpNor bn512Nor
#define bnXpAnd bn512And
#define bnXpNand bn512Nand
#define bnXpNot bn512Not
#define bnXpSetNot bn512SetNot
#define bnXpNeg bn512Neg
#define bnXpSetNeg bn512SetNeg
#define bnXpShl bn512Shl
#define bnXpShr bn512Shr
#define bnXpSal bn512Sal
#define bnXpSar bn512Sar
#define bnXpShrRound bn512ShrRound
#define bnXpSarRound bn512SarRound
#define bnXpShl1 bn512Shl1
#define bnXpSetShl1 bn512SetShl1
#define bnXpShr1 bn512Shr1
#define bnXpSetShr1 bn512SetShr1

#define bnXpCmpZero bn512CmpZero
#define bnXpCmpNotZero bn512CmpNotZero
#define bnXpCmpEq bn512CmpEq
#define bnXpCmpNeq bn512CmpNeq
#define bnXpCmpGt bn512CmpGt
#define bnXpCmpGe bn512CmpGe
#define bnXpCmpLt bn512CmpLt
#define bnXpCmpLe bn512CmpLe
#define bnXpCmpSignedGt bn512CmpSignedGt
#define bnXpCmpSignedGe bn512CmpSignedGe
#define bnXpCmpSignedLt bn512CmpSignedLt
#define bnXpCmpSignedLe bn512CmpSignedLe
#define bnXpCmpPositive bn512CmpPositive
#define bnXpCmpNegative bn512CmpNegative
#define bnXpCmpPart bn512CmpPart

#define bnXpExtractBit bn512ExtractBit
#define bnXpExtract32 bn512Extract32
#define bnXpExtract64 bn512Extract64
#define bnXpGetIndexMSB bn512GetIndexMSB

#define bnXpPrint bn512Print
#define bnXpPrintHex bn512PrintHex
#define bnXpPrintBin bn512PrintBin
#define bnXpPrintOut bn512PrintOut
#define bnXpPrintHexOut bn512PrintHexOut
#define bnXpPrintBinOut bn512PrintBinOut
#define bnXpScan bn512Scan

#define bnXpSqrt bn512Sqrt
#define bnXpLog bn512Log
#define bnXpExp bn512Exp
#define bnXpPow bn512Pow
#define bnXpPowInt bn512PowInt
#define bnXpCos bn512Cos
#define bnXpSin bn512Sin
#define bnXpTan bn512Tan

#elif BN_XP_BITS == 1024

#define bnXp bn1024
#define bnXpZero bn1024Zero
#define bnXpSet bn1024Set
#define bnXpSet32 bn1024Set32
#define bnXpSet32Signed bn1024Set32Signed
#define bnXpSet32Shl bn1024Set32Shl
#define bnXpSetDouble bn1024SetDouble
#define bnXpGetDouble bn1024GetDouble

#define bnXpAdd32 bn1024Add32
#define bnXpAdd32Signed bn1024Add32Signed
#define bnXpAdd32Shl bn1024Add32Shl
#define bnXpSub32 bn1024Sub32
#define bnXpSub32Signed bn1024Sub32Signed
#define bnXpSub32Shl bn1024Sub32Shl
#define bnXpAdd bn1024Add
#define bnXpSetAdd bn1024SetAdd
#define bnXpSub bn1024Sub
#define bnXpSetSub bn1024SetSub
#define bnXpSetAddAdd bn1024SetAddAdd
#define bnXpSetAddSub bn1024SetAddSub
#define bnXpSetAddAddSub bn1024SetAddAddSub
#define bnXpSetAddAddAddSub bn1024SetAddAddAddSub

#define bnXpMul32 bn1024Mul32
#define bnXpMul32Signed bn1024Mul32Signed
#define bnXpMul32Check bn1024Mul32Check
#define bnXpMul32SignedCheck bn1024Mul32SignedCheck
#define bnXpMul bn1024Mul
#define bnXpMulCheck bn1024MulCheck
#define bnXpMulSignedCheck bn1024MulSignedCheck
#define bnXpMulShr bn1024MulShr
#define bnXpMulSignedShr bn1024MulSignedShr
#define bnXpMulCheckShr bn1024MulCheckShr
#define bnXpMulSignedCheckShr bn1024MulSignedCheckShr
#define bnXpSquareShr bn1024SquareShr

#define bnXpDiv32 bn1024Div32
#define bnXpDiv32Signed bn1024Div32Signed
#define bnXpDiv32Round bn1024Div32Round
#define bnXpDiv32RoundSigned bn1024Div32RoundSigned
#define bnXpDiv bn1024Div
#define bnXpDivSigned bn1024DivSigned
#define bnXpDivRound bn1024DivRound
#define bnXpDivRoundSigned bn1024DivRoundSigned
#define bnXpDivShl bn1024DivShl
#define bnXpDivSignedShl bn1024DivSignedShl
#define bnXpDivRoundShl bn1024DivRoundShl
#define bnXpDivRoundSignedShl bn1024DivRoundSignedShl

#define bnXpOr bn1024Or
#define bnXpNor bn1024Nor
#define bnXpAnd bn1024And
#define bnXpNand bn1024Nand
#define bnXpNot bn1024Not
#define bnXpSetNot bn1024SetNot
#define bnXpNeg bn1024Neg
#define bnXpSetNeg bn1024SetNeg
#define bnXpShl bn1024Shl
#define bnXpShr bn1024Shr
#define bnXpSal bn1024Sal
#define bnXpSar bn1024Sar
#define bnXpShrRound bn1024ShrRound
#define bnXpSarRound bn1024SarRound
#define bnXpShl1 bn1024Shl1
#define bnXpSetShl1 bn1024SetShl1
#define bnXpShr1 bn1024Shr1
#define bnXpSetShr1 bn1024SetShr1

#define bnXpCmpZero bn1024CmpZero
#define bnXpCmpNotZero bn1024CmpNotZero
#define bnXpCmpEq bn1024CmpEq
#define bnXpCmpNeq bn1024CmpNeq
#define bnXpCmpGt bn1024CmpGt
#define bnXpCmpGe bn1024CmpGe
#define bnXpCmpLt bn1024CmpLt
#define bnXpCmpLe bn1024CmpLe
#define bnXpCmpSignedGt bn1024CmpSignedGt
#define bnXpCmpSignedGe bn1024CmpSignedGe
#define bnXpCmpSignedLt bn1024CmpSignedLt
#define bnXpCmpSignedLe bn1024CmpSignedLe
#define bnXpCmpPositive bn1024CmpPositive
#define bnXpCmpNegative bn1024CmpNegative
#define bnXpCmpPart bn1024CmpPart

#define bnXpExtractBit bn1024ExtractBit
#define bnXpExtract32 bn1024Extract32
#define bnXpExtract64 bn1024Extract64
#define bnXpGetIndexMSB bn1024GetIndexMSB

#define bnXpPrint bn1024Print
#define bnXpPrintHex bn1024PrintHex
#define bnXpPrintBin bn1024PrintBin
#define bnXpPrintOut bn1024PrintOut
#define bnXpPrintHexOut bn1024PrintHexOut
#define bnXpPrintBinOut bn1024PrintBinOut
#define bnXpScan bn1024Scan

#define bnXpSqrt bn1024Sqrt
#define bnXpLog bn1024Log
#define bnXpExp bn1024Exp
#define bnXpPow bn1024Pow
#define bnXpPowInt bn1024PowInt
#define bnXpCos bn1024Cos
#define bnXpSin bn1024Sin
#define bnXpTan bn1024Tan

#elif defined(BN_XP_BITS)

#error Unsupported BN_XP_BITS

#endif


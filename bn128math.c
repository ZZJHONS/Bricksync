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
#include <limits.h>
#include <float.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"

#define BN_XP_SUPPORT_128 1
#define BN_XP_SUPPORT_192 0
#define BN_XP_SUPPORT_256 0
#define BN_XP_SUPPORT_512 0
#define BN_XP_SUPPORT_1024 0
#include "bn.h"



////



#define BN128_LOG2_SHIFT (128)
#define BN128_LOG1P0625_SHIFT (128)
#define BN128_MUL1P0625_SHIFT (128-1)
#define BN128_DIV1P0625_SHIFT (128)
#define BN128_EXP1_SHIFT (128-2)
#define BN128_EXP1INV_SHIFT (128)
#define BN128_EXP0P125_SHIFT (128-1)
#define BN128_DIV_SHIFT (128-1)
#define BN128_PI_SHIFT (128-2)

#define BN128_DIV_TABLE_SIZE (256)

#define BN128_USE_INVERSE_DIVISION

#if BN_UNIT_BITS == 64

const bn128 bn128InfPos = 
{
 .unit = { 0xffffffffffffffff, 0x7fffffffffffffff }
};

const bn128 bn128InfNeg = 
{
 .unit = { 0x0000000000000000, 0x8000000000000000 }
};

const bn128 bn128Log2 = 
{
 .unit = { 0xc9e3b39803f2f6af, 0xb17217f7d1cf79ab }
};

const bn128 bn128Log1p0625 = 
{
 .unit = { 0xbe64b8b775997898, 0x0f85186008b15330 }
};

const bn128 bn128Mul1p0625 = 
{
 .unit = { 0x0000000000000000, 0x8800000000000000 }
};

const bn128 bn128Div1p0625 = 
{
 .unit = { 0xf0f0f0f0f0f0f0f0, 0xf0f0f0f0f0f0f0f0 }
};

const bn128 bn128Exp1 = 
{
 .unit = { 0xafdc5620273d3cf2, 0xadf85458a2bb4a9a }
};

const bn128 bn128Exp1Inv = 
{
 .unit = { 0xbadec7829054f90e, 0x5e2d58d8b3bcdf1a }
};

const bn128 bn128Exp0p125 = 
{
 .unit = { 0x76b441c27035c6a1, 0x910b022db7ae67ce }
};

const bn128 bn128Pi = 
{
 .unit = { 0xc4c6628b80dc1cd1, 0xc90fdaa22168c234 }
};

const bn128 bn128DivFactor[BN128_DIV_TABLE_SIZE] = 
{
  [0] = { .unit = { 0x0000000000000000, 0x0000000000000000 } },
  [1] = { .unit = { 0x0000000000000000, 0x0000000000000000 } },
  [2] = { .unit = { 0x0000000000000000, 0x4000000000000000 } },
  [3] = { .unit = { 0xaaaaaaaaaaaaaaab, 0x2aaaaaaaaaaaaaaa } },
  [4] = { .unit = { 0x0000000000000000, 0x2000000000000000 } },
  [5] = { .unit = { 0x999999999999999a, 0x1999999999999999 } },
  [6] = { .unit = { 0x5555555555555555, 0x1555555555555555 } },
  [7] = { .unit = { 0x2492492492492492, 0x1249249249249249 } },
  [8] = { .unit = { 0x0000000000000000, 0x1000000000000000 } },
  [9] = { .unit = { 0xe38e38e38e38e38e, 0x0e38e38e38e38e38 } },
  [10] = { .unit = { 0xcccccccccccccccd, 0x0ccccccccccccccc } },
  [11] = { .unit = { 0xba2e8ba2e8ba2e8c, 0x0ba2e8ba2e8ba2e8 } },
  [12] = { .unit = { 0xaaaaaaaaaaaaaaab, 0x0aaaaaaaaaaaaaaa } },
  [13] = { .unit = { 0x9d89d89d89d89d8a, 0x09d89d89d89d89d8 } },
  [14] = { .unit = { 0x9249249249249249, 0x0924924924924924 } },
  [15] = { .unit = { 0x8888888888888889, 0x0888888888888888 } },
  [16] = { .unit = { 0x0000000000000000, 0x0800000000000000 } },
  [17] = { .unit = { 0x8787878787878788, 0x0787878787878787 } },
  [18] = { .unit = { 0x71c71c71c71c71c7, 0x071c71c71c71c71c } },
  [19] = { .unit = { 0xf286bca1af286bca, 0x06bca1af286bca1a } },
  [20] = { .unit = { 0x6666666666666666, 0x0666666666666666 } },
  [21] = { .unit = { 0x6186186186186186, 0x0618618618618618 } },
  [22] = { .unit = { 0x5d1745d1745d1746, 0x05d1745d1745d174 } },
  [23] = { .unit = { 0x21642c8590b21643, 0x0590b21642c8590b } },
  [24] = { .unit = { 0x5555555555555555, 0x0555555555555555 } },
  [25] = { .unit = { 0x51eb851eb851eb85, 0x051eb851eb851eb8 } },
  [26] = { .unit = { 0x4ec4ec4ec4ec4ec5, 0x04ec4ec4ec4ec4ec } },
  [27] = { .unit = { 0xf684bda12f684bda, 0x04bda12f684bda12 } },
  [28] = { .unit = { 0x4924924924924925, 0x0492492492492492 } },
  [29] = { .unit = { 0x69ee58469ee5846a, 0x0469ee58469ee584 } },
  [30] = { .unit = { 0x4444444444444444, 0x0444444444444444 } },
  [31] = { .unit = { 0x4210842108421084, 0x0421084210842108 } },
  [32] = { .unit = { 0x0000000000000000, 0x0400000000000000 } },
  [33] = { .unit = { 0x3e0f83e0f83e0f84, 0x03e0f83e0f83e0f8 } },
  [34] = { .unit = { 0xc3c3c3c3c3c3c3c4, 0x03c3c3c3c3c3c3c3 } },
  [35] = { .unit = { 0x3a83a83a83a83a84, 0x03a83a83a83a83a8 } },
  [36] = { .unit = { 0x38e38e38e38e38e4, 0x038e38e38e38e38e } },
  [37] = { .unit = { 0x2983759f2298375a, 0x03759f22983759f2 } },
  [38] = { .unit = { 0x79435e50d79435e5, 0x035e50d79435e50d } },
  [39] = { .unit = { 0x3483483483483483, 0x0348348348348348 } },
  [40] = { .unit = { 0x3333333333333333, 0x0333333333333333 } },
  [41] = { .unit = { 0x31f3831f3831f383, 0x031f3831f3831f38 } },
  [42] = { .unit = { 0x30c30c30c30c30c3, 0x030c30c30c30c30c } },
  [43] = { .unit = { 0xfa0be82fa0be82fa, 0x02fa0be82fa0be82 } },
  [44] = { .unit = { 0x2e8ba2e8ba2e8ba3, 0x02e8ba2e8ba2e8ba } },
  [45] = { .unit = { 0x2d82d82d82d82d83, 0x02d82d82d82d82d8 } },
  [46] = { .unit = { 0x90b21642c8590b21, 0x02c8590b21642c85 } },
  [47] = { .unit = { 0xc415c9882b931057, 0x02b9310572620ae4 } },
  [48] = { .unit = { 0xaaaaaaaaaaaaaaab, 0x02aaaaaaaaaaaaaa } },
  [49] = { .unit = { 0x05397829cbc14e5e, 0x029cbc14e5e0a72f } },
  [50] = { .unit = { 0x28f5c28f5c28f5c3, 0x028f5c28f5c28f5c } },
  [51] = { .unit = { 0x8282828282828283, 0x0282828282828282 } },
  [52] = { .unit = { 0x2762762762762762, 0x0276276276276276 } },
  [53] = { .unit = { 0xa439f656f1826a44, 0x026a439f656f1826 } },
  [54] = { .unit = { 0x7b425ed097b425ed, 0x025ed097b425ed09 } },
  [55] = { .unit = { 0x253c8253c8253c82, 0x0253c8253c8253c8 } },
  [56] = { .unit = { 0x2492492492492492, 0x0249249249249249 } },
  [57] = { .unit = { 0xfb823ee08fb823ee, 0x023ee08fb823ee08 } },
  [58] = { .unit = { 0x34f72c234f72c235, 0x0234f72c234f72c2 } },
  [59] = { .unit = { 0x8ad8f2fba9386823, 0x022b63cbeea4e1a0 } },
  [60] = { .unit = { 0x2222222222222222, 0x0222222222222222 } },
  [61] = { .unit = { 0x2192e29f79b47582, 0x02192e29f79b4758 } },
  [62] = { .unit = { 0x2108421084210842, 0x0210842108421084 } },
  [63] = { .unit = { 0x2082082082082082, 0x0208208208208208 } },
  [64] = { .unit = { 0x0000000000000000, 0x0200000000000000 } },
  [65] = { .unit = { 0x1f81f81f81f81f82, 0x01f81f81f81f81f8 } },
  [66] = { .unit = { 0x1f07c1f07c1f07c2, 0x01f07c1f07c1f07c } },
  [67] = { .unit = { 0xa07a44c6afc2dd9d, 0x01e9131abf0b7672 } },
  [68] = { .unit = { 0xe1e1e1e1e1e1e1e2, 0x01e1e1e1e1e1e1e1 } },
  [69] = { .unit = { 0x6076b981dae6076c, 0x01dae6076b981dae } },
  [70] = { .unit = { 0x1d41d41d41d41d42, 0x01d41d41d41d41d4 } },
  [71] = { .unit = { 0x12073615a240e6c3, 0x01cd85689039b0ad } },
  [72] = { .unit = { 0x1c71c71c71c71c72, 0x01c71c71c71c71c7 } },
  [73] = { .unit = { 0x0381c0e070381c0e, 0x01c0e070381c0e07 } },
  [74] = { .unit = { 0x14c1bacf914c1bad, 0x01bacf914c1bacf9 } },
  [75] = { .unit = { 0x1b4e81b4e81b4e82, 0x01b4e81b4e81b4e8 } },
  [76] = { .unit = { 0xbca1af286bca1af3, 0x01af286bca1af286 } },
  [77] = { .unit = { 0x1a98ef606a63bd82, 0x01a98ef606a63bd8 } },
  [78] = { .unit = { 0x1a41a41a41a41a42, 0x01a41a41a41a41a4 } },
  [79] = { .unit = { 0xd2a2067b23a5440d, 0x019ec8e951033d91 } },
  [80] = { .unit = { 0x999999999999999a, 0x0199999999999999 } },
  [81] = { .unit = { 0x522c3f35ba781949, 0x01948b0fcd6e9e06 } },
  [82] = { .unit = { 0x18f9c18f9c18f9c2, 0x018f9c18f9c18f9c } },
  [83] = { .unit = { 0x3784a062b2e43db0, 0x018acb90f6bf3a9a } },
  [84] = { .unit = { 0x1861861861861862, 0x0186186186186186 } },
  [85] = { .unit = { 0x8181818181818182, 0x0181818181818181 } },
  [86] = { .unit = { 0x7d05f417d05f417d, 0x017d05f417d05f41 } },
  [87] = { .unit = { 0x78a4c8178a4c8179, 0x0178a4c8178a4c81 } },
  [88] = { .unit = { 0x1745d1745d1745d1, 0x01745d1745d1745d } },
  [89] = { .unit = { 0xe05c0b81702e05c1, 0x01702e05c0b81702 } },
  [90] = { .unit = { 0x16c16c16c16c16c1, 0x016c16c16c16c16c } },
  [91] = { .unit = { 0x1681681681681681, 0x0168168168168168 } },
  [92] = { .unit = { 0xc8590b21642c8591, 0x01642c8590b21642 } },
  [93] = { .unit = { 0x1605816058160581, 0x0160581605816058 } },
  [94] = { .unit = { 0x620ae4c415c9882c, 0x015c9882b9310572 } },
  [95] = { .unit = { 0x308158ed2308158f, 0x0158ed2308158ed2 } },
  [96] = { .unit = { 0x5555555555555555, 0x0155555555555555 } },
  [97] = { .unit = { 0xd07eae2f8151d07f, 0x0151d07eae2f8151 } },
  [98] = { .unit = { 0x829cbc14e5e0a72f, 0x014e5e0a72f05397 } },
  [99] = { .unit = { 0x14afd6a052bf5a81, 0x014afd6a052bf5a8 } },
  [100] = { .unit = { 0x147ae147ae147ae1, 0x0147ae147ae147ae } },
  [101] = { .unit = { 0xe41e6a74981446f8, 0x01446f86562d9fae } },
  [102] = { .unit = { 0x4141414141414141, 0x0141414141414141 } },
  [103] = { .unit = { 0xc45979c95204f88b, 0x013e22cbce4a9027 } },
  [104] = { .unit = { 0x13b13b13b13b13b1, 0x013b13b13b13b13b } },
  [105] = { .unit = { 0x1381381381381381, 0x0138138138138138 } },
  [106] = { .unit = { 0x521cfb2b78c13522, 0x013521cfb2b78c13 } },
  [107] = { .unit = { 0x6e0e5aea77a04c90, 0x01323e34a2b10bf6 } },
  [108] = { .unit = { 0xbda12f684bda12f7, 0x012f684bda12f684 } },
  [109] = { .unit = { 0x4d812c9fb4d812ca, 0x012c9fb4d812c9fb } },
  [110] = { .unit = { 0x129e4129e4129e41, 0x0129e4129e4129e4 } },
  [111] = { .unit = { 0xb88127350b881273, 0x0127350b88127350 } },
  [112] = { .unit = { 0x9249249249249249, 0x0124924924924924 } },
  [113] = { .unit = { 0x21fb78121fb78122, 0x0121fb78121fb781 } },
  [114] = { .unit = { 0x7dc11f7047dc11f7, 0x011f7047dc11f704 } },
  [115] = { .unit = { 0x06ada2811cf06ada, 0x011cf06ada2811cf } },
  [116] = { .unit = { 0x1a7b9611a7b9611a, 0x011a7b9611a7b961 } },
  [117] = { .unit = { 0x1181181181181181, 0x0118118118118118 } },
  [118] = { .unit = { 0x456c797dd49c3411, 0x0115b1e5f75270d0 } },
  [119] = { .unit = { 0x5c81135c81135c81, 0x01135c81135c8113 } },
  [120] = { .unit = { 0x1111111111111111, 0x0111111111111111 } },
  [121] = { .unit = { 0xe26152832c6e043b, 0x010ecf56be69c8fd } },
  [122] = { .unit = { 0x10c9714fbcda3ac1, 0x010c9714fbcda3ac } },
  [123] = { .unit = { 0x10a6810a6810a681, 0x010a6810a6810a68 } },
  [124] = { .unit = { 0x1084210842108421, 0x0108421084210842 } },
  [125] = { .unit = { 0x76c8b4395810624e, 0x010624dd2f1a9fbe } },
  [126] = { .unit = { 0x1041041041041041, 0x0104104104104104 } },
  [127] = { .unit = { 0x0204081020408102, 0x0102040810204081 } },
  [128] = { .unit = { 0x0000000000000000, 0x0100000000000000 } },
  [129] = { .unit = { 0xfe03f80fe03f80fe, 0x00fe03f80fe03f80 } },
  [130] = { .unit = { 0x0fc0fc0fc0fc0fc1, 0x00fc0fc0fc0fc0fc } },
  [131] = { .unit = { 0xbf82ee6986d6f63b, 0x00fa232cf252138a } },
  [132] = { .unit = { 0x0f83e0f83e0f83e1, 0x00f83e0f83e0f83e } },
  [133] = { .unit = { 0xd980f6603d980f66, 0x00f6603d980f6603 } },
  [134] = { .unit = { 0x503d226357e16ece, 0x00f4898d5f85bb39 } },
  [135] = { .unit = { 0x6480f2b9d6480f2c, 0x00f2b9d6480f2b9d } },
  [136] = { .unit = { 0xf0f0f0f0f0f0f0f1, 0x00f0f0f0f0f0f0f0 } },
  [137] = { .unit = { 0x380ef2eb71fc4345, 0x00ef2eb71fc43452 } },
  [138] = { .unit = { 0x303b5cc0ed7303b6, 0x00ed7303b5cc0ed7 } },
  [139] = { .unit = { 0x8bf8a2126ad1f4f3, 0x00ebbdb2a5c1619c } },
  [140] = { .unit = { 0x0ea0ea0ea0ea0ea1, 0x00ea0ea0ea0ea0ea } },
  [141] = { .unit = { 0x96b1edd80e865ac8, 0x00e865ac7b7603a1 } },
  [142] = { .unit = { 0x89039b0ad1207361, 0x00e6c2b4481cd856 } },
  [143] = { .unit = { 0x0e525982af70c881, 0x00e525982af70c88 } },
  [144] = { .unit = { 0x8e38e38e38e38e39, 0x00e38e38e38e38e3 } },
  [145] = { .unit = { 0xe1fc780e1fc780e2, 0x00e1fc780e1fc780 } },
  [146] = { .unit = { 0x81c0e070381c0e07, 0x00e070381c0e0703 } },
  [147] = { .unit = { 0x5713280dee95c4ca, 0x00dee95c4ca037ba } },
  [148] = { .unit = { 0x8a60dd67c8a60dd6, 0x00dd67c8a60dd67c } },
  [149] = { .unit = { 0x579fc90527844b99, 0x00dbeb61eed19c59 } },
  [150] = { .unit = { 0x0da740da740da741, 0x00da740da740da74 } },
  [151] = { .unit = { 0x0d901b2036406c81, 0x00d901b2036406c8 } },
  [152] = { .unit = { 0x5e50d79435e50d79, 0x00d79435e50d7943 } },
  [153] = { .unit = { 0x2b80d62b80d62b81, 0x00d62b80d62b80d6 } },
  [154] = { .unit = { 0x0d4c77b03531dec1, 0x00d4c77b03531dec } },
  [155] = { .unit = { 0x0d3680d3680d3681, 0x00d3680d3680d368 } },
  [156] = { .unit = { 0x0d20d20d20d20d21, 0x00d20d20d20d20d2 } },
  [157] = { .unit = { 0x0b69fcbd2580d0b7, 0x00d0b69fcbd2580d } },
  [158] = { .unit = { 0xe951033d91d2a206, 0x00cf6474a8819ec8 } },
  [159] = { .unit = { 0xe168a7725080ce17, 0x00ce168a7725080c } },
  [160] = { .unit = { 0xcccccccccccccccd, 0x00cccccccccccccc } },
  [161] = { .unit = { 0xe032e1c9f01970e5, 0x00cb8727c065c393 } },
  [162] = { .unit = { 0x29161f9add3c0ca4, 0x00ca4587e6b74f03 } },
  [163] = { .unit = { 0xacc2bf9b7c12d8bc, 0x00c907da4e871146 } },
  [164] = { .unit = { 0x0c7ce0c7ce0c7ce1, 0x00c7ce0c7ce0c7ce } },
  [165] = { .unit = { 0x0c6980c6980c6981, 0x00c6980c6980c698 } },
  [166] = { .unit = { 0x1bc2503159721ed8, 0x00c565c87b5f9d4d } },
  [167] = { .unit = { 0xa58e901886e5f0ac, 0x00c4372f855d824c } },
  [168] = { .unit = { 0x0c30c30c30c30c31, 0x00c30c30c30c30c3 } },
  [169] = { .unit = { 0x4731fcf86d10a9a8, 0x00c1e4bbd595f6e9 } },
  [170] = { .unit = { 0xc0c0c0c0c0c0c0c1, 0x00c0c0c0c0c0c0c0 } },
  [171] = { .unit = { 0xfe80bfa02fe80bfa, 0x00bfa02fe80bfa02 } },
  [172] = { .unit = { 0xbe82fa0be82fa0bf, 0x00be82fa0be82fa0 } },
  [173] = { .unit = { 0xa2c649fd0a5bbee4, 0x00bd69104707661a } },
  [174] = { .unit = { 0xbc52640bc52640bc, 0x00bc52640bc52640 } },
  [175] = { .unit = { 0x0bb3ee721a54d881, 0x00bb3ee721a54d88 } },
  [176] = { .unit = { 0x8ba2e8ba2e8ba2e9, 0x00ba2e8ba2e8ba2e } },
  [177] = { .unit = { 0x2e4850fe8dbd780c, 0x00b92143fa36f5e0 } },
  [178] = { .unit = { 0x702e05c0b81702e0, 0x00b81702e05c0b81 } },
  [179] = { .unit = { 0x58ab9ebfa4782253, 0x00b70fbb5a19be36 } },
  [180] = { .unit = { 0x0b60b60b60b60b61, 0x00b60b60b60b60b6 } },
  [181] = { .unit = { 0x1f1db39fd2bd865d, 0x00b509e68a9b9482 } },
  [182] = { .unit = { 0x0b40b40b40b40b41, 0x00b40b40b40b40b4 } },
  [183] = { .unit = { 0x0b30f63528917c81, 0x00b30f63528917c8 } },
  [184] = { .unit = { 0x642c8590b21642c8, 0x00b21642c8590b21 } },
  [185] = { .unit = { 0x3b80b11fd3b80b12, 0x00b11fd3b80b11fd } },
  [186] = { .unit = { 0x0b02c0b02c0b02c1, 0x00b02c0b02c0b02c } },
  [187] = { .unit = { 0xddc680af3addc681, 0x00af3addc680af3a } },
  [188] = { .unit = { 0x310572620ae4c416, 0x00ae4c415c9882b9 } },
  [189] = { .unit = { 0xb580ad602b580ad6, 0x00ad602b580ad602 } },
  [190] = { .unit = { 0x1840ac7691840ac7, 0x00ac7691840ac769 } },
  [191] = { .unit = { 0x116c90c101571ed4, 0x00ab8f69e28359cd } },
  [192] = { .unit = { 0xaaaaaaaaaaaaaaab, 0x00aaaaaaaaaaaaaa } },
  [193] = { .unit = { 0x37b5b85f80a9c84a, 0x00a9c84a47a07f56 } },
  [194] = { .unit = { 0xe83f5717c0a8e83f, 0x00a8e83f5717c0a8 } },
  [195] = { .unit = { 0x0a80a80a80a80a81, 0x00a80a80a80a80a8 } },
  [196] = { .unit = { 0xc14e5e0a72f05398, 0x00a72f05397829cb } },
  [197] = { .unit = { 0xa7a26fc19fd66a8f, 0x00a655c4392d7b73 } },
  [198] = { .unit = { 0x0a57eb50295fad41, 0x00a57eb50295fad4 } },
  [199] = { .unit = { 0x510b93f09014953a, 0x00a4a9cf1d968337 } },
  [200] = { .unit = { 0x0a3d70a3d70a3d71, 0x00a3d70a3d70a3d7 } },
  [201] = { .unit = { 0xe028c1978feb9f34, 0x00a3065e3fae7cd0 } },
  [202] = { .unit = { 0x720f353a4c0a237c, 0x00a237c32b16cfd7 } },
  [203] = { .unit = { 0x7cd8e80a16b312eb, 0x00a16b312ea8fc37 } },
  [204] = { .unit = { 0xa0a0a0a0a0a0a0a1, 0x00a0a0a0a0a0a0a0 } },
  [205] = { .unit = { 0x09fd809fd809fd81, 0x009fd809fd809fd8 } },
  [206] = { .unit = { 0xe22cbce4a9027c46, 0x009f1165e7254813 } },
  [207] = { .unit = { 0x2027932b48f757cf, 0x009e4cad23dd5f3a } },
  [208] = { .unit = { 0x89d89d89d89d89d9, 0x009d89d89d89d89d } },
  [209] = { .unit = { 0xb8f4f9e027323858, 0x009cc8e160c3fb19 } },
  [210] = { .unit = { 0x09c09c09c09c09c1, 0x009c09c09c09c09c } },
  [211] = { .unit = { 0xa9db9a15d6bfb25a, 0x009b4c6f9ef03a3c } },
  [212] = { .unit = { 0xa90e7d95bc609a91, 0x009a90e7d95bc609 } },
  [213] = { .unit = { 0x0602675c8b6af796, 0x0099d722dabde58f } },
  [214] = { .unit = { 0x37072d753bd02648, 0x00991f1a515885fb } },
  [215] = { .unit = { 0x9868c809868c8098, 0x009868c809868c80 } },
  [216] = { .unit = { 0x5ed097b425ed097b, 0x0097b425ed097b42 } },
  [217] = { .unit = { 0x097012e025c04b81, 0x0097012e025c04b8 } },
  [218] = { .unit = { 0xa6c0964fda6c0965, 0x00964fda6c0964fd } },
  [219] = { .unit = { 0x568095a02568095a, 0x0095a02568095a02 } },
  [220] = { .unit = { 0x094f2094f2094f21, 0x0094f2094f2094f2 } },
  [221] = { .unit = { 0x4580944580944581, 0x0094458094458094 } },
  [222] = { .unit = { 0x5c40939a85c4093a, 0x00939a85c40939a8 } },
  [223] = { .unit = { 0x9c2024bc44e10126, 0x0092f11384049788 } },
  [224] = { .unit = { 0x4924924924924925, 0x0092492492492492 } },
  [225] = { .unit = { 0x091a2b3c4d5e6f81, 0x0091a2b3c4d5e6f8 } },
  [226] = { .unit = { 0x90fdbc090fdbc091, 0x0090fdbc090fdbc0 } },
  [227] = { .unit = { 0x3aa4a6e85132bfb8, 0x00905a38633e06c4 } },
  [228] = { .unit = { 0x3ee08fb823ee08fc, 0x008fb823ee08fb82 } },
  [229] = { .unit = { 0x189808f1779d9fdc, 0x008f1779d9fdc3a2 } },
  [230] = { .unit = { 0x8356d1408e78356d, 0x008e78356d1408e7 } },
  [231] = { .unit = { 0x08dda52023769481, 0x008dda5202376948 } },
  [232] = { .unit = { 0x8d3dcb08d3dcb08d, 0x008d3dcb08d3dcb0 } },
  [233] = { .unit = { 0x2328a70119453809, 0x008ca29c046514e0 } },
  [234] = { .unit = { 0x08c08c08c08c08c1, 0x008c08c08c08c08c } },
  [235] = { .unit = { 0x5a6ac1e808b70345, 0x008b70344a139bc7 } },
  [236] = { .unit = { 0x22b63cbeea4e1a09, 0x008ad8f2fba93868 } },
  [237] = { .unit = { 0x463602290be1c15a, 0x008a42f8705669db } },
  [238] = { .unit = { 0xae4089ae4089ae41, 0x0089ae4089ae4089 } },
  [239] = { .unit = { 0x5055b0bc84d1f101, 0x00891ac73ae9819b } },
  [240] = { .unit = { 0x8888888888888889, 0x0088888888888888 } },
  [241] = { .unit = { 0xf78087f78087f781, 0x0087f78087f78087 } },
  [242] = { .unit = { 0xf130a9419637021e, 0x008767ab5f34e47e } },
  [243] = { .unit = { 0xc60ebfbc937d5dc3, 0x0086d905447a34ac } },
  [244] = { .unit = { 0x0864b8a7de6d1d61, 0x00864b8a7de6d1d6 } },
  [245] = { .unit = { 0x9aa518085bf37613, 0x0085bf37612cee3c } },
  [246] = { .unit = { 0x0853408534085341, 0x0085340853408534 } },
  [247] = { .unit = { 0x9c8084a9f9c8084b, 0x0084a9f9c8084a9f } },
  [248] = { .unit = { 0x0842108421084211, 0x0084210842108421 } },
  [249] = { .unit = { 0x67d6e020e64c1490, 0x00839930523fbe33 } },
  [250] = { .unit = { 0x3b645a1cac083127, 0x0083126e978d4fdf } },
  [251] = { .unit = { 0xa32fefae680828cc, 0x00828cbfbeb9a020 } },
  [252] = { .unit = { 0x0820820820820821, 0x0082082082082082 } },
  [253] = { .unit = { 0x77663297c7560206, 0x0081848da8faf0d2 } },
  [254] = { .unit = { 0x8102040810204081, 0x0081020408102040 } },
  [255] = { .unit = { 0x8080808080808081, 0x0080808080808080 } },
};

#elif BN_UNIT_BITS == 32

const bn128 bn128InfPos = 
{
 .unit = { 0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff }
};

const bn128 bn128InfNeg = 
{
 .unit = { 0x00000000, 0x00000000, 0x00000000, 0x80000000 }
};

const bn128 bn128Log2 = 
{
 .unit = { 0x03f2f6af, 0xc9e3b398, 0xd1cf79ab, 0xb17217f7 }
};

const bn128 bn128Log1p0625 = 
{
 .unit = { 0x75997898, 0xbe64b8b7, 0x08b15330, 0x0f851860 }
};

const bn128 bn128Mul1p0625 = 
{
 .unit = { 0x00000000, 0x00000000, 0x00000000, 0x88000000 }
};

const bn128 bn128Div1p0625 = 
{
 .unit = { 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0, 0xf0f0f0f0 }
};

const bn128 bn128Exp1 = 
{
 .unit = { 0x273d3cf2, 0xafdc5620, 0xa2bb4a9a, 0xadf85458 }
};

const bn128 bn128Exp1Inv = 
{
 .unit = { 0x9054f90e, 0xbadec782, 0xb3bcdf1a, 0x5e2d58d8 }
};

const bn128 bn128Exp0p125 = 
{
 .unit = { 0x7035c6a1, 0x76b441c2, 0xb7ae67ce, 0x910b022d }
};

const bn128 bn128Pi = 
{
 .unit = { 0x80dc1cd1, 0xc4c6628b, 0x2168c234, 0xc90fdaa2 }
};

const bn128 bn128DivFactor[BN128_DIV_TABLE_SIZE] = 
{
  [0] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },
  [1] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } },
  [2] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x40000000 } },
  [3] = { .unit = { 0xaaaaaaab, 0xaaaaaaaa, 0xaaaaaaaa, 0x2aaaaaaa } },
  [4] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x20000000 } },
  [5] = { .unit = { 0x9999999a, 0x99999999, 0x99999999, 0x19999999 } },
  [6] = { .unit = { 0x55555555, 0x55555555, 0x55555555, 0x15555555 } },
  [7] = { .unit = { 0x92492492, 0x24924924, 0x49249249, 0x12492492 } },
  [8] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x10000000 } },
  [9] = { .unit = { 0x8e38e38e, 0xe38e38e3, 0x38e38e38, 0x0e38e38e } },
  [10] = { .unit = { 0xcccccccd, 0xcccccccc, 0xcccccccc, 0x0ccccccc } },
  [11] = { .unit = { 0xe8ba2e8c, 0xba2e8ba2, 0x2e8ba2e8, 0x0ba2e8ba } },
  [12] = { .unit = { 0xaaaaaaab, 0xaaaaaaaa, 0xaaaaaaaa, 0x0aaaaaaa } },
  [13] = { .unit = { 0x89d89d8a, 0x9d89d89d, 0xd89d89d8, 0x09d89d89 } },
  [14] = { .unit = { 0x49249249, 0x92492492, 0x24924924, 0x09249249 } },
  [15] = { .unit = { 0x88888889, 0x88888888, 0x88888888, 0x08888888 } },
  [16] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x08000000 } },
  [17] = { .unit = { 0x87878788, 0x87878787, 0x87878787, 0x07878787 } },
  [18] = { .unit = { 0xc71c71c7, 0x71c71c71, 0x1c71c71c, 0x071c71c7 } },
  [19] = { .unit = { 0xaf286bca, 0xf286bca1, 0x286bca1a, 0x06bca1af } },
  [20] = { .unit = { 0x66666666, 0x66666666, 0x66666666, 0x06666666 } },
  [21] = { .unit = { 0x86186186, 0x61861861, 0x18618618, 0x06186186 } },
  [22] = { .unit = { 0x745d1746, 0x5d1745d1, 0x1745d174, 0x05d1745d } },
  [23] = { .unit = { 0x90b21643, 0x21642c85, 0x42c8590b, 0x0590b216 } },
  [24] = { .unit = { 0x55555555, 0x55555555, 0x55555555, 0x05555555 } },
  [25] = { .unit = { 0xb851eb85, 0x51eb851e, 0xeb851eb8, 0x051eb851 } },
  [26] = { .unit = { 0xc4ec4ec5, 0x4ec4ec4e, 0xec4ec4ec, 0x04ec4ec4 } },
  [27] = { .unit = { 0x2f684bda, 0xf684bda1, 0x684bda12, 0x04bda12f } },
  [28] = { .unit = { 0x24924925, 0x49249249, 0x92492492, 0x04924924 } },
  [29] = { .unit = { 0x9ee5846a, 0x69ee5846, 0x469ee584, 0x0469ee58 } },
  [30] = { .unit = { 0x44444444, 0x44444444, 0x44444444, 0x04444444 } },
  [31] = { .unit = { 0x08421084, 0x42108421, 0x10842108, 0x04210842 } },
  [32] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x04000000 } },
  [33] = { .unit = { 0xf83e0f84, 0x3e0f83e0, 0x0f83e0f8, 0x03e0f83e } },
  [34] = { .unit = { 0xc3c3c3c4, 0xc3c3c3c3, 0xc3c3c3c3, 0x03c3c3c3 } },
  [35] = { .unit = { 0x83a83a84, 0x3a83a83a, 0xa83a83a8, 0x03a83a83 } },
  [36] = { .unit = { 0xe38e38e4, 0x38e38e38, 0x8e38e38e, 0x038e38e3 } },
  [37] = { .unit = { 0x2298375a, 0x2983759f, 0x983759f2, 0x03759f22 } },
  [38] = { .unit = { 0xd79435e5, 0x79435e50, 0x9435e50d, 0x035e50d7 } },
  [39] = { .unit = { 0x83483483, 0x34834834, 0x48348348, 0x03483483 } },
  [40] = { .unit = { 0x33333333, 0x33333333, 0x33333333, 0x03333333 } },
  [41] = { .unit = { 0x3831f383, 0x31f3831f, 0xf3831f38, 0x031f3831 } },
  [42] = { .unit = { 0xc30c30c3, 0x30c30c30, 0x0c30c30c, 0x030c30c3 } },
  [43] = { .unit = { 0xa0be82fa, 0xfa0be82f, 0x2fa0be82, 0x02fa0be8 } },
  [44] = { .unit = { 0xba2e8ba3, 0x2e8ba2e8, 0x8ba2e8ba, 0x02e8ba2e } },
  [45] = { .unit = { 0x82d82d83, 0x2d82d82d, 0xd82d82d8, 0x02d82d82 } },
  [46] = { .unit = { 0xc8590b21, 0x90b21642, 0x21642c85, 0x02c8590b } },
  [47] = { .unit = { 0x2b931057, 0xc415c988, 0x72620ae4, 0x02b93105 } },
  [48] = { .unit = { 0xaaaaaaab, 0xaaaaaaaa, 0xaaaaaaaa, 0x02aaaaaa } },
  [49] = { .unit = { 0xcbc14e5e, 0x05397829, 0xe5e0a72f, 0x029cbc14 } },
  [50] = { .unit = { 0x5c28f5c3, 0x28f5c28f, 0xf5c28f5c, 0x028f5c28 } },
  [51] = { .unit = { 0x82828283, 0x82828282, 0x82828282, 0x02828282 } },
  [52] = { .unit = { 0x62762762, 0x27627627, 0x76276276, 0x02762762 } },
  [53] = { .unit = { 0xf1826a44, 0xa439f656, 0x656f1826, 0x026a439f } },
  [54] = { .unit = { 0x97b425ed, 0x7b425ed0, 0xb425ed09, 0x025ed097 } },
  [55] = { .unit = { 0xc8253c82, 0x253c8253, 0x3c8253c8, 0x0253c825 } },
  [56] = { .unit = { 0x92492492, 0x24924924, 0x49249249, 0x02492492 } },
  [57] = { .unit = { 0x8fb823ee, 0xfb823ee0, 0xb823ee08, 0x023ee08f } },
  [58] = { .unit = { 0x4f72c235, 0x34f72c23, 0x234f72c2, 0x0234f72c } },
  [59] = { .unit = { 0xa9386823, 0x8ad8f2fb, 0xeea4e1a0, 0x022b63cb } },
  [60] = { .unit = { 0x22222222, 0x22222222, 0x22222222, 0x02222222 } },
  [61] = { .unit = { 0x79b47582, 0x2192e29f, 0xf79b4758, 0x02192e29 } },
  [62] = { .unit = { 0x84210842, 0x21084210, 0x08421084, 0x02108421 } },
  [63] = { .unit = { 0x82082082, 0x20820820, 0x08208208, 0x02082082 } },
  [64] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x02000000 } },
  [65] = { .unit = { 0x81f81f82, 0x1f81f81f, 0xf81f81f8, 0x01f81f81 } },
  [66] = { .unit = { 0x7c1f07c2, 0x1f07c1f0, 0x07c1f07c, 0x01f07c1f } },
  [67] = { .unit = { 0xafc2dd9d, 0xa07a44c6, 0xbf0b7672, 0x01e9131a } },
  [68] = { .unit = { 0xe1e1e1e2, 0xe1e1e1e1, 0xe1e1e1e1, 0x01e1e1e1 } },
  [69] = { .unit = { 0xdae6076c, 0x6076b981, 0x6b981dae, 0x01dae607 } },
  [70] = { .unit = { 0x41d41d42, 0x1d41d41d, 0xd41d41d4, 0x01d41d41 } },
  [71] = { .unit = { 0xa240e6c3, 0x12073615, 0x9039b0ad, 0x01cd8568 } },
  [72] = { .unit = { 0x71c71c72, 0x1c71c71c, 0xc71c71c7, 0x01c71c71 } },
  [73] = { .unit = { 0x70381c0e, 0x0381c0e0, 0x381c0e07, 0x01c0e070 } },
  [74] = { .unit = { 0x914c1bad, 0x14c1bacf, 0x4c1bacf9, 0x01bacf91 } },
  [75] = { .unit = { 0xe81b4e82, 0x1b4e81b4, 0x4e81b4e8, 0x01b4e81b } },
  [76] = { .unit = { 0x6bca1af3, 0xbca1af28, 0xca1af286, 0x01af286b } },
  [77] = { .unit = { 0x6a63bd82, 0x1a98ef60, 0x06a63bd8, 0x01a98ef6 } },
  [78] = { .unit = { 0x41a41a42, 0x1a41a41a, 0xa41a41a4, 0x01a41a41 } },
  [79] = { .unit = { 0x23a5440d, 0xd2a2067b, 0x51033d91, 0x019ec8e9 } },
  [80] = { .unit = { 0x9999999a, 0x99999999, 0x99999999, 0x01999999 } },
  [81] = { .unit = { 0xba781949, 0x522c3f35, 0xcd6e9e06, 0x01948b0f } },
  [82] = { .unit = { 0x9c18f9c2, 0x18f9c18f, 0xf9c18f9c, 0x018f9c18 } },
  [83] = { .unit = { 0xb2e43db0, 0x3784a062, 0xf6bf3a9a, 0x018acb90 } },
  [84] = { .unit = { 0x61861862, 0x18618618, 0x86186186, 0x01861861 } },
  [85] = { .unit = { 0x81818182, 0x81818181, 0x81818181, 0x01818181 } },
  [86] = { .unit = { 0xd05f417d, 0x7d05f417, 0x17d05f41, 0x017d05f4 } },
  [87] = { .unit = { 0x8a4c8179, 0x78a4c817, 0x178a4c81, 0x0178a4c8 } },
  [88] = { .unit = { 0x5d1745d1, 0x1745d174, 0x45d1745d, 0x01745d17 } },
  [89] = { .unit = { 0x702e05c1, 0xe05c0b81, 0xc0b81702, 0x01702e05 } },
  [90] = { .unit = { 0xc16c16c1, 0x16c16c16, 0x6c16c16c, 0x016c16c1 } },
  [91] = { .unit = { 0x81681681, 0x16816816, 0x68168168, 0x01681681 } },
  [92] = { .unit = { 0x642c8591, 0xc8590b21, 0x90b21642, 0x01642c85 } },
  [93] = { .unit = { 0x58160581, 0x16058160, 0x05816058, 0x01605816 } },
  [94] = { .unit = { 0x15c9882c, 0x620ae4c4, 0xb9310572, 0x015c9882 } },
  [95] = { .unit = { 0x2308158f, 0x308158ed, 0x08158ed2, 0x0158ed23 } },
  [96] = { .unit = { 0x55555555, 0x55555555, 0x55555555, 0x01555555 } },
  [97] = { .unit = { 0x8151d07f, 0xd07eae2f, 0xae2f8151, 0x0151d07e } },
  [98] = { .unit = { 0xe5e0a72f, 0x829cbc14, 0x72f05397, 0x014e5e0a } },
  [99] = { .unit = { 0x52bf5a81, 0x14afd6a0, 0x052bf5a8, 0x014afd6a } },
  [100] = { .unit = { 0xae147ae1, 0x147ae147, 0x7ae147ae, 0x0147ae14 } },
  [101] = { .unit = { 0x981446f8, 0xe41e6a74, 0x562d9fae, 0x01446f86 } },
  [102] = { .unit = { 0x41414141, 0x41414141, 0x41414141, 0x01414141 } },
  [103] = { .unit = { 0x5204f88b, 0xc45979c9, 0xce4a9027, 0x013e22cb } },
  [104] = { .unit = { 0xb13b13b1, 0x13b13b13, 0x3b13b13b, 0x013b13b1 } },
  [105] = { .unit = { 0x81381381, 0x13813813, 0x38138138, 0x01381381 } },
  [106] = { .unit = { 0x78c13522, 0x521cfb2b, 0xb2b78c13, 0x013521cf } },
  [107] = { .unit = { 0x77a04c90, 0x6e0e5aea, 0xa2b10bf6, 0x01323e34 } },
  [108] = { .unit = { 0x4bda12f7, 0xbda12f68, 0xda12f684, 0x012f684b } },
  [109] = { .unit = { 0xb4d812ca, 0x4d812c9f, 0xd812c9fb, 0x012c9fb4 } },
  [110] = { .unit = { 0xe4129e41, 0x129e4129, 0x9e4129e4, 0x0129e412 } },
  [111] = { .unit = { 0x0b881273, 0xb8812735, 0x88127350, 0x0127350b } },
  [112] = { .unit = { 0x49249249, 0x92492492, 0x24924924, 0x01249249 } },
  [113] = { .unit = { 0x1fb78122, 0x21fb7812, 0x121fb781, 0x0121fb78 } },
  [114] = { .unit = { 0x47dc11f7, 0x7dc11f70, 0xdc11f704, 0x011f7047 } },
  [115] = { .unit = { 0x1cf06ada, 0x06ada281, 0xda2811cf, 0x011cf06a } },
  [116] = { .unit = { 0xa7b9611a, 0x1a7b9611, 0x11a7b961, 0x011a7b96 } },
  [117] = { .unit = { 0x81181181, 0x11811811, 0x18118118, 0x01181181 } },
  [118] = { .unit = { 0xd49c3411, 0x456c797d, 0xf75270d0, 0x0115b1e5 } },
  [119] = { .unit = { 0x81135c81, 0x5c81135c, 0x135c8113, 0x01135c81 } },
  [120] = { .unit = { 0x11111111, 0x11111111, 0x11111111, 0x01111111 } },
  [121] = { .unit = { 0x2c6e043b, 0xe2615283, 0xbe69c8fd, 0x010ecf56 } },
  [122] = { .unit = { 0xbcda3ac1, 0x10c9714f, 0xfbcda3ac, 0x010c9714 } },
  [123] = { .unit = { 0x6810a681, 0x10a6810a, 0xa6810a68, 0x010a6810 } },
  [124] = { .unit = { 0x42108421, 0x10842108, 0x84210842, 0x01084210 } },
  [125] = { .unit = { 0x5810624e, 0x76c8b439, 0x2f1a9fbe, 0x010624dd } },
  [126] = { .unit = { 0x41041041, 0x10410410, 0x04104104, 0x01041041 } },
  [127] = { .unit = { 0x20408102, 0x02040810, 0x10204081, 0x01020408 } },
  [128] = { .unit = { 0x00000000, 0x00000000, 0x00000000, 0x01000000 } },
  [129] = { .unit = { 0xe03f80fe, 0xfe03f80f, 0x0fe03f80, 0x00fe03f8 } },
  [130] = { .unit = { 0xc0fc0fc1, 0x0fc0fc0f, 0xfc0fc0fc, 0x00fc0fc0 } },
  [131] = { .unit = { 0x86d6f63b, 0xbf82ee69, 0xf252138a, 0x00fa232c } },
  [132] = { .unit = { 0x3e0f83e1, 0x0f83e0f8, 0x83e0f83e, 0x00f83e0f } },
  [133] = { .unit = { 0x3d980f66, 0xd980f660, 0x980f6603, 0x00f6603d } },
  [134] = { .unit = { 0x57e16ece, 0x503d2263, 0x5f85bb39, 0x00f4898d } },
  [135] = { .unit = { 0xd6480f2c, 0x6480f2b9, 0x480f2b9d, 0x00f2b9d6 } },
  [136] = { .unit = { 0xf0f0f0f1, 0xf0f0f0f0, 0xf0f0f0f0, 0x00f0f0f0 } },
  [137] = { .unit = { 0x71fc4345, 0x380ef2eb, 0x1fc43452, 0x00ef2eb7 } },
  [138] = { .unit = { 0xed7303b6, 0x303b5cc0, 0xb5cc0ed7, 0x00ed7303 } },
  [139] = { .unit = { 0x6ad1f4f3, 0x8bf8a212, 0xa5c1619c, 0x00ebbdb2 } },
  [140] = { .unit = { 0xa0ea0ea1, 0x0ea0ea0e, 0xea0ea0ea, 0x00ea0ea0 } },
  [141] = { .unit = { 0x0e865ac8, 0x96b1edd8, 0x7b7603a1, 0x00e865ac } },
  [142] = { .unit = { 0xd1207361, 0x89039b0a, 0x481cd856, 0x00e6c2b4 } },
  [143] = { .unit = { 0xaf70c881, 0x0e525982, 0x2af70c88, 0x00e52598 } },
  [144] = { .unit = { 0x38e38e39, 0x8e38e38e, 0xe38e38e3, 0x00e38e38 } },
  [145] = { .unit = { 0x1fc780e2, 0xe1fc780e, 0x0e1fc780, 0x00e1fc78 } },
  [146] = { .unit = { 0x381c0e07, 0x81c0e070, 0x1c0e0703, 0x00e07038 } },
  [147] = { .unit = { 0xee95c4ca, 0x5713280d, 0x4ca037ba, 0x00dee95c } },
  [148] = { .unit = { 0xc8a60dd6, 0x8a60dd67, 0xa60dd67c, 0x00dd67c8 } },
  [149] = { .unit = { 0x27844b99, 0x579fc905, 0xeed19c59, 0x00dbeb61 } },
  [150] = { .unit = { 0x740da741, 0x0da740da, 0xa740da74, 0x00da740d } },
  [151] = { .unit = { 0x36406c81, 0x0d901b20, 0x036406c8, 0x00d901b2 } },
  [152] = { .unit = { 0x35e50d79, 0x5e50d794, 0xe50d7943, 0x00d79435 } },
  [153] = { .unit = { 0x80d62b81, 0x2b80d62b, 0xd62b80d6, 0x00d62b80 } },
  [154] = { .unit = { 0x3531dec1, 0x0d4c77b0, 0x03531dec, 0x00d4c77b } },
  [155] = { .unit = { 0x680d3681, 0x0d3680d3, 0x3680d368, 0x00d3680d } },
  [156] = { .unit = { 0x20d20d21, 0x0d20d20d, 0xd20d20d2, 0x00d20d20 } },
  [157] = { .unit = { 0x2580d0b7, 0x0b69fcbd, 0xcbd2580d, 0x00d0b69f } },
  [158] = { .unit = { 0x91d2a206, 0xe951033d, 0xa8819ec8, 0x00cf6474 } },
  [159] = { .unit = { 0x5080ce17, 0xe168a772, 0x7725080c, 0x00ce168a } },
  [160] = { .unit = { 0xcccccccd, 0xcccccccc, 0xcccccccc, 0x00cccccc } },
  [161] = { .unit = { 0xf01970e5, 0xe032e1c9, 0xc065c393, 0x00cb8727 } },
  [162] = { .unit = { 0xdd3c0ca4, 0x29161f9a, 0xe6b74f03, 0x00ca4587 } },
  [163] = { .unit = { 0x7c12d8bc, 0xacc2bf9b, 0x4e871146, 0x00c907da } },
  [164] = { .unit = { 0xce0c7ce1, 0x0c7ce0c7, 0x7ce0c7ce, 0x00c7ce0c } },
  [165] = { .unit = { 0x980c6981, 0x0c6980c6, 0x6980c698, 0x00c6980c } },
  [166] = { .unit = { 0x59721ed8, 0x1bc25031, 0x7b5f9d4d, 0x00c565c8 } },
  [167] = { .unit = { 0x86e5f0ac, 0xa58e9018, 0x855d824c, 0x00c4372f } },
  [168] = { .unit = { 0x30c30c31, 0x0c30c30c, 0xc30c30c3, 0x00c30c30 } },
  [169] = { .unit = { 0x6d10a9a8, 0x4731fcf8, 0xd595f6e9, 0x00c1e4bb } },
  [170] = { .unit = { 0xc0c0c0c1, 0xc0c0c0c0, 0xc0c0c0c0, 0x00c0c0c0 } },
  [171] = { .unit = { 0x2fe80bfa, 0xfe80bfa0, 0xe80bfa02, 0x00bfa02f } },
  [172] = { .unit = { 0xe82fa0bf, 0xbe82fa0b, 0x0be82fa0, 0x00be82fa } },
  [173] = { .unit = { 0x0a5bbee4, 0xa2c649fd, 0x4707661a, 0x00bd6910 } },
  [174] = { .unit = { 0xc52640bc, 0xbc52640b, 0x0bc52640, 0x00bc5264 } },
  [175] = { .unit = { 0x1a54d881, 0x0bb3ee72, 0x21a54d88, 0x00bb3ee7 } },
  [176] = { .unit = { 0x2e8ba2e9, 0x8ba2e8ba, 0xa2e8ba2e, 0x00ba2e8b } },
  [177] = { .unit = { 0x8dbd780c, 0x2e4850fe, 0xfa36f5e0, 0x00b92143 } },
  [178] = { .unit = { 0xb81702e0, 0x702e05c0, 0xe05c0b81, 0x00b81702 } },
  [179] = { .unit = { 0xa4782253, 0x58ab9ebf, 0x5a19be36, 0x00b70fbb } },
  [180] = { .unit = { 0x60b60b61, 0x0b60b60b, 0xb60b60b6, 0x00b60b60 } },
  [181] = { .unit = { 0xd2bd865d, 0x1f1db39f, 0x8a9b9482, 0x00b509e6 } },
  [182] = { .unit = { 0x40b40b41, 0x0b40b40b, 0xb40b40b4, 0x00b40b40 } },
  [183] = { .unit = { 0x28917c81, 0x0b30f635, 0x528917c8, 0x00b30f63 } },
  [184] = { .unit = { 0xb21642c8, 0x642c8590, 0xc8590b21, 0x00b21642 } },
  [185] = { .unit = { 0xd3b80b12, 0x3b80b11f, 0xb80b11fd, 0x00b11fd3 } },
  [186] = { .unit = { 0x2c0b02c1, 0x0b02c0b0, 0x02c0b02c, 0x00b02c0b } },
  [187] = { .unit = { 0x3addc681, 0xddc680af, 0xc680af3a, 0x00af3add } },
  [188] = { .unit = { 0x0ae4c416, 0x31057262, 0x5c9882b9, 0x00ae4c41 } },
  [189] = { .unit = { 0x2b580ad6, 0xb580ad60, 0x580ad602, 0x00ad602b } },
  [190] = { .unit = { 0x91840ac7, 0x1840ac76, 0x840ac769, 0x00ac7691 } },
  [191] = { .unit = { 0x01571ed4, 0x116c90c1, 0xe28359cd, 0x00ab8f69 } },
  [192] = { .unit = { 0xaaaaaaab, 0xaaaaaaaa, 0xaaaaaaaa, 0x00aaaaaa } },
  [193] = { .unit = { 0x80a9c84a, 0x37b5b85f, 0x47a07f56, 0x00a9c84a } },
  [194] = { .unit = { 0xc0a8e83f, 0xe83f5717, 0x5717c0a8, 0x00a8e83f } },
  [195] = { .unit = { 0x80a80a81, 0x0a80a80a, 0xa80a80a8, 0x00a80a80 } },
  [196] = { .unit = { 0x72f05398, 0xc14e5e0a, 0x397829cb, 0x00a72f05 } },
  [197] = { .unit = { 0x9fd66a8f, 0xa7a26fc1, 0x392d7b73, 0x00a655c4 } },
  [198] = { .unit = { 0x295fad41, 0x0a57eb50, 0x0295fad4, 0x00a57eb5 } },
  [199] = { .unit = { 0x9014953a, 0x510b93f0, 0x1d968337, 0x00a4a9cf } },
  [200] = { .unit = { 0xd70a3d71, 0x0a3d70a3, 0x3d70a3d7, 0x00a3d70a } },
  [201] = { .unit = { 0x8feb9f34, 0xe028c197, 0x3fae7cd0, 0x00a3065e } },
  [202] = { .unit = { 0x4c0a237c, 0x720f353a, 0x2b16cfd7, 0x00a237c3 } },
  [203] = { .unit = { 0x16b312eb, 0x7cd8e80a, 0x2ea8fc37, 0x00a16b31 } },
  [204] = { .unit = { 0xa0a0a0a1, 0xa0a0a0a0, 0xa0a0a0a0, 0x00a0a0a0 } },
  [205] = { .unit = { 0xd809fd81, 0x09fd809f, 0xfd809fd8, 0x009fd809 } },
  [206] = { .unit = { 0xa9027c46, 0xe22cbce4, 0xe7254813, 0x009f1165 } },
  [207] = { .unit = { 0x48f757cf, 0x2027932b, 0x23dd5f3a, 0x009e4cad } },
  [208] = { .unit = { 0xd89d89d9, 0x89d89d89, 0x9d89d89d, 0x009d89d8 } },
  [209] = { .unit = { 0x27323858, 0xb8f4f9e0, 0x60c3fb19, 0x009cc8e1 } },
  [210] = { .unit = { 0xc09c09c1, 0x09c09c09, 0x9c09c09c, 0x009c09c0 } },
  [211] = { .unit = { 0xd6bfb25a, 0xa9db9a15, 0x9ef03a3c, 0x009b4c6f } },
  [212] = { .unit = { 0xbc609a91, 0xa90e7d95, 0xd95bc609, 0x009a90e7 } },
  [213] = { .unit = { 0x8b6af796, 0x0602675c, 0xdabde58f, 0x0099d722 } },
  [214] = { .unit = { 0x3bd02648, 0x37072d75, 0x515885fb, 0x00991f1a } },
  [215] = { .unit = { 0x868c8098, 0x9868c809, 0x09868c80, 0x009868c8 } },
  [216] = { .unit = { 0x25ed097b, 0x5ed097b4, 0xed097b42, 0x0097b425 } },
  [217] = { .unit = { 0x25c04b81, 0x097012e0, 0x025c04b8, 0x0097012e } },
  [218] = { .unit = { 0xda6c0965, 0xa6c0964f, 0x6c0964fd, 0x00964fda } },
  [219] = { .unit = { 0x2568095a, 0x568095a0, 0x68095a02, 0x0095a025 } },
  [220] = { .unit = { 0xf2094f21, 0x094f2094, 0x4f2094f2, 0x0094f209 } },
  [221] = { .unit = { 0x80944581, 0x45809445, 0x94458094, 0x00944580 } },
  [222] = { .unit = { 0x85c4093a, 0x5c40939a, 0xc40939a8, 0x00939a85 } },
  [223] = { .unit = { 0x44e10126, 0x9c2024bc, 0x84049788, 0x0092f113 } },
  [224] = { .unit = { 0x24924925, 0x49249249, 0x92492492, 0x00924924 } },
  [225] = { .unit = { 0x4d5e6f81, 0x091a2b3c, 0xc4d5e6f8, 0x0091a2b3 } },
  [226] = { .unit = { 0x0fdbc091, 0x90fdbc09, 0x090fdbc0, 0x0090fdbc } },
  [227] = { .unit = { 0x5132bfb8, 0x3aa4a6e8, 0x633e06c4, 0x00905a38 } },
  [228] = { .unit = { 0x23ee08fc, 0x3ee08fb8, 0xee08fb82, 0x008fb823 } },
  [229] = { .unit = { 0x779d9fdc, 0x189808f1, 0xd9fdc3a2, 0x008f1779 } },
  [230] = { .unit = { 0x8e78356d, 0x8356d140, 0x6d1408e7, 0x008e7835 } },
  [231] = { .unit = { 0x23769481, 0x08dda520, 0x02376948, 0x008dda52 } },
  [232] = { .unit = { 0xd3dcb08d, 0x8d3dcb08, 0x08d3dcb0, 0x008d3dcb } },
  [233] = { .unit = { 0x19453809, 0x2328a701, 0x046514e0, 0x008ca29c } },
  [234] = { .unit = { 0xc08c08c1, 0x08c08c08, 0x8c08c08c, 0x008c08c0 } },
  [235] = { .unit = { 0x08b70345, 0x5a6ac1e8, 0x4a139bc7, 0x008b7034 } },
  [236] = { .unit = { 0xea4e1a09, 0x22b63cbe, 0xfba93868, 0x008ad8f2 } },
  [237] = { .unit = { 0x0be1c15a, 0x46360229, 0x705669db, 0x008a42f8 } },
  [238] = { .unit = { 0x4089ae41, 0xae4089ae, 0x89ae4089, 0x0089ae40 } },
  [239] = { .unit = { 0x84d1f101, 0x5055b0bc, 0x3ae9819b, 0x00891ac7 } },
  [240] = { .unit = { 0x88888889, 0x88888888, 0x88888888, 0x00888888 } },
  [241] = { .unit = { 0x8087f781, 0xf78087f7, 0x87f78087, 0x0087f780 } },
  [242] = { .unit = { 0x9637021e, 0xf130a941, 0x5f34e47e, 0x008767ab } },
  [243] = { .unit = { 0x937d5dc3, 0xc60ebfbc, 0x447a34ac, 0x0086d905 } },
  [244] = { .unit = { 0xde6d1d61, 0x0864b8a7, 0x7de6d1d6, 0x00864b8a } },
  [245] = { .unit = { 0x5bf37613, 0x9aa51808, 0x612cee3c, 0x0085bf37 } },
  [246] = { .unit = { 0x34085341, 0x08534085, 0x53408534, 0x00853408 } },
  [247] = { .unit = { 0xf9c8084b, 0x9c8084a9, 0xc8084a9f, 0x0084a9f9 } },
  [248] = { .unit = { 0x21084211, 0x08421084, 0x42108421, 0x00842108 } },
  [249] = { .unit = { 0xe64c1490, 0x67d6e020, 0x523fbe33, 0x00839930 } },
  [250] = { .unit = { 0xac083127, 0x3b645a1c, 0x978d4fdf, 0x0083126e } },
  [251] = { .unit = { 0x680828cc, 0xa32fefae, 0xbeb9a020, 0x00828cbf } },
  [252] = { .unit = { 0x20820821, 0x08208208, 0x82082082, 0x00820820 } },
  [253] = { .unit = { 0xc7560206, 0x77663297, 0xa8faf0d2, 0x0081848d } },
  [254] = { .unit = { 0x10204081, 0x81020408, 0x08102040, 0x00810204 } },
  [255] = { .unit = { 0x80808081, 0x80808080, 0x80808080, 0x00808080 } },
};

#endif



////



static inline void bn128Shr2( bn128 *dst )
{
#if BN_UNIT_BITS == 64
  dst->unit[0] = ( dst->unit[0] >> 2 ) | ( dst->unit[1] << (BN_UNIT_BITS-2) );
  dst->unit[1] = ( dst->unit[1] >> 2 );
#else
  dst->unit[0] = ( dst->unit[0] >> 2 ) | ( dst->unit[1] << (BN_UNIT_BITS-2) );
  dst->unit[1] = ( dst->unit[1] >> 2 ) | ( dst->unit[2] << (BN_UNIT_BITS-2) );
  dst->unit[2] = ( dst->unit[2] >> 2 ) | ( dst->unit[3] << (BN_UNIT_BITS-2) );
  dst->unit[3] = ( dst->unit[3] >> 2 );
#endif
  return;
}


void bn128Sqrt( bn128 *dst, const bn128 * const src, bnShift shift )
{
  int msb, oneshift;
  bn128 base, one, tmp;

  bn128Set( &base, src );
  bn128Zero( dst );
  msb = bn128GetIndexMSB( &base );
  oneshift = ( msb & ~0x1 ) | ( shift & 0x1 );
  bn128Zero( &one );
  bn128SetBit( &one, oneshift );
  for( ; bn128CmpGt( &one, &base ) ; )
  {
    oneshift -= 2;
    bn128Shr2( &one );
  }
  for( ; ; )
  {
    bn128SetOr( &tmp, dst, &one );
    bn128Shr1( dst );
    if( bn128CmpGe( &base, &tmp ) )
    {
      bn128Sub( &base, &tmp );
      bn128Or( dst, &one );
    }
    if( oneshift < 2 )
    {
      if( shift < 2 )
        break;
      shift -= 2;
      bn128Shl( dst, dst, 2 );
      bn128Shl( &base, &base, 2 );
    }
    else
    {
      oneshift -= 2;
      bn128Zero( &one );
      bn128SetBit( &one, oneshift );
    }
  }
  if( bn128CmpGt( &base, dst ) )
    bn128Add32( dst, 1 );

  return;
}


void bn128Log( bn128 *dst, const bn128 * const src, bnShift shift )
{
  int divisor;
  bn128 one, limit, base, fsub, fadd, part, bnlog, tmp;
#ifndef BN128_USE_INVERSE_DIVISION
  bn128 factor;
#endif

  bn128Zero( dst );
  if( bn128CmpNegative( src ) )
    return;
  bn128Set( &base, src );
  bn128Set32Shl( &one, 1, shift );
  if( bn128CmpGt( &base, &one ) )
  {
    bn128Set32Shl( &limit, 2, shift-0 );
    if( bn128CmpGt( &base, &limit ) )
    {
      bn128ShrRound( &bnlog, &bn128Log2, BN128_LOG2_SHIFT - shift );
      do
      {
        bn128Add( dst, &bnlog );
        bn128Shr1( &base );
      } while( bn128CmpGt( &base, &limit ) );
    }
    bn128Set32Shl( &limit, 33, shift-5 );
    if( bn128CmpGt( &base, &limit ) )
    {
      bn128ShrRound( &bnlog, &bn128Log1p0625, BN128_LOG1P0625_SHIFT - shift );
      do
      {
        bn128Add( dst, &bnlog );
        bn128MulShr( &tmp, &bn128Div1p0625, &base, BN128_DIV1P0625_SHIFT );
        if( bn128CmpEq( &base, &tmp ) )
          break;
        bn128Set( &base, &tmp );
      } while( bn128CmpGt( &base, &limit ) );
    }
  }
  else
  {
    bn128Set32Shl( &limit, 1, shift-1 );
    if( bn128CmpLt( &base, &limit ) )
    {
      bn128ShrRound( &bnlog, &bn128Log2, BN128_LOG2_SHIFT - shift );
      do
      {
        bn128Sub( dst, &bnlog );
        bn128Shl1( &base );
      } while( bn128CmpLt( &base, &limit ) );
    }
    bn128Set32Shl( &limit, 31, shift-5 );
    if( bn128CmpLt( &base, &limit ) )
    {
      bn128ShrRound( &bnlog, &bn128Log1p0625, BN128_LOG1P0625_SHIFT - shift );
      do
      {
        bn128Sub( dst, &bnlog );
        bn128MulShr( &tmp, &bn128Mul1p0625, &base, BN128_MUL1P0625_SHIFT );
        if( bn128CmpEq( &base, &tmp ) )
          break;
        bn128Set( &base, &tmp );
      } while( bn128CmpLt( &base, &limit ) );
    }
  }

  bn128Sub( &base, &one );
  bn128SquareShr( &fsub, &base, shift );
  bn128Add( dst, &base );
  for( divisor = 2 ; divisor < BN128_DIV_TABLE_SIZE ; divisor += 2 )
  {
#ifdef BN128_USE_INVERSE_DIVISION
    bn128MulSignedShr( &part, &bn128DivFactor[divisor+0], &fsub, BN128_DIV_SHIFT );
#else
    bn128Set( &factor, &one );
    bn128Div32Round( &factor, divisor+0 );
    bn128MulSignedShr( &part, &factor, &fsub, shift );
#endif
    bn128Sub( dst, &part );
    bn128MulSignedShr( &fadd, &fsub, &base, shift );
    if( bn128CmpEqOrZero( &fadd, &fsub ) )
      break;
#ifdef BN128_USE_INVERSE_DIVISION
    bn128MulSignedShr( &part, &bn128DivFactor[divisor+1], &fadd, BN128_DIV_SHIFT );
#else
    bn128Set( &factor, &one );
    bn128Div32Round( &factor, divisor+1 );
    bn128MulSignedShr( &part, &factor, &fadd, shift );
#endif
    bn128Add( dst, &part );
    bn128MulSignedShr( &fsub, &fadd, &base, shift );
    if( bn128CmpEqOrZero( &fsub, &fadd ) )
      break;
  }
#ifdef BN128_PRINT_REPORT
  printf( "Log Iteration Count : %d\n", (divisor-2)/2 );
#endif

  return;
}


void bn128Exp( bn128 *dst, const bn128 * const src, bnShift shift )
{
  int divisor, factorflag;
  bn128 term0, term1, one, factor, tmp, base, limit, bnexp, div8;

  bn128Set32Shl( &one, 1, shift );
  bn128Set( &base, src );

  factorflag = 0;
  bn128Set( &factor, &one );
  if( bn128CmpPositive( &base ) )
  {
    bn128Set( &limit, &one );
    if( bn128CmpGt( &base, &limit ) )
    {
      bn128ShrRound( &bnexp, &bn128Exp1, BN128_EXP1_SHIFT - shift );
      factorflag = 1;
      do
      {
        if( bn128MulCheckShr( &tmp, &factor, &bnexp, shift ) )
        {
          bn128Set( dst, &bn128InfPos );
          return;
        }
        bn128Set( &factor, &tmp );
        bn128Sub( &base, &one );
      } while( bn128CmpGt( &base, &limit ) );
    }
  }
  else
  {
    bn128Zero( &limit );
    if( bn128CmpSignedLt( &base, &limit ) )
    {
      bn128ShrRound( &bnexp, &bn128Exp1Inv, BN128_EXP1INV_SHIFT - shift );
      factorflag = 1;
      do
      {
        bn128MulShr( &tmp, &factor, &bnexp, shift );
        if( bn128CmpZero( &tmp ) )
        {
          bn128CmpZero( dst );
          return;
        }
        bn128Set( &factor, &tmp );
        bn128Add( &base, &one );
      } while( bn128CmpSignedLt( &base, &limit ) );
    }
  }

  bn128Set32Shl( &limit, 1, shift-4 );
  if( bn128CmpSignedGt( &base, &limit ) )
  {
    bn128Set32Shl( &div8, 1, shift-3 );
    if( bn128CmpNotZero( &div8 ) )
    {
      bn128ShrRound( &bnexp, &bn128Exp0p125, BN128_EXP0P125_SHIFT - shift );
      factorflag = 1;
      do
      {
        if( bn128MulCheckShr( &tmp, &factor, &bnexp, shift ) )
        {
          bn128Set( dst, &bn128InfPos );
          return;
        }
        bn128Set( &factor, &tmp );
        bn128Sub( &base, &div8 );
      } while( bn128CmpSignedGt( &base, &limit ) );
    }
  }

  bn128SetAdd( dst, &one, &base );
  bn128Set( &term0, &base );
  for( divisor = 2 ; divisor < BN128_DIV_TABLE_SIZE ; divisor += 2 )
  {
#ifdef BN128_USE_INVERSE_DIVISION
    bn128MulSignedShr( &tmp, &bn128DivFactor[divisor+0], &base, BN128_DIV_SHIFT );
#else
    bn128Set( &tmp, &base );
    bn128Div32RoundSigned( &tmp, divisor+0 );
#endif
    bn128MulSignedShr( &term1, &tmp, &term0, shift );
    if( bn128CmpEqOrZero( &term1, &term0 ) )
      break;
    bn128Add( dst, &term1 );
#ifdef BN128_USE_INVERSE_DIVISION
    bn128MulSignedShr( &tmp, &bn128DivFactor[divisor+1], &base, BN128_DIV_SHIFT );
#else
    bn128Set( &tmp, &base );
    bn128Div32RoundSigned( &tmp, divisor+1 );
#endif
    bn128MulSignedShr( &term0, &tmp, &term1, shift );
    if( bn128CmpEqOrZero( &term0, &term1 ) )
      break;
    bn128Add( dst, &term0 );
  }
  if( factorflag )
  {
    bn128MulSignedShr( &tmp, &factor, dst, shift );
    bn128Set( dst, &tmp );
  }
#ifdef BN128_PRINT_REPORT
  printf( "Exp Iteration Count : %d\n", (divisor-2)/2 );
#endif

  return;
}


void bn128Pow( bn128 *dst, const bn128 * const src, const bn128 * const exponent, bnShift shift )
{
  bn128 tmp, expf;
  bn128Log( &tmp, src, shift );
  bn128MulSignedShr( &expf, &tmp, exponent, shift );
  bn128Exp( dst, &expf, shift );
  return;
}


void bn128PowInt( bn128 *dst, const bn128 * const src, uint32_t exponent, bnShift shift )
{
  uint32_t exlevel, mulflag;
  bn128 tmp, mul;
  if( exponent == 0 )
  {
    bn128Set32Shl( dst, 1, shift );
    return;
  }
  mulflag = 0;
  bn128Set( &mul, src );
  exlevel = 1;
  for( ; ; )
  {
    if( exponent & exlevel )
    {
      if( !( mulflag ) )
      {
        bn128Set( dst, &mul );
        mulflag = 1;
      }
      else
      {
        bn128MulSignedShr( &tmp, dst, &mul, shift );
        bn128Set( dst, &tmp );
      }
      exponent -= exlevel;
      if( !( exponent ) )
        break;
    }
    exlevel <<= 1;
    bn128SquareShr( &tmp, &mul, shift );
    bn128Set( &mul, &tmp );
  }
  return;
}


void bn128Cos( bn128 *dst, const bn128 * const src, bnShift shift )
{
  int divisor, negflag, divproduct, msbdiff;
  bn128 term0, term1, one, tmp, tmp2, base, bn2pi, bnpi, bnhpi, basesq, rem;

  if( bn128CmpPositive( src ) )
    bn128Set( &base, src );
  else
    bn128SetNeg( &base, src );
  bn128ShrRound( &bn2pi, &bn128Pi, BN128_PI_SHIFT-1 - shift );
  if( bn128CmpGt( &base, &bn2pi ) )
  {
    msbdiff = bn128GetIndexMSB( &base ) - bn128GetIndexMSB( &bn2pi );
    if( msbdiff >= 3 )
    {
      bn128Div( &base, &bn2pi, &rem );
      bn128Set( &base, &rem );
    }
    else
    {
      for( ; bn128CmpGt( &base, &bn2pi ) ; )
        bn128Sub( &base, &bn2pi );
    }
  }
  bn128ShrRound( &bnpi, &bn128Pi, BN128_PI_SHIFT+0 - shift );
  if( bn128CmpGt( &base, &bnpi ) )
  {
    bn128SetSub( &tmp, &bn2pi, &base );
    bn128Set( &base, &tmp );
  }
  negflag = 0;
  bn128ShrRound( &bnhpi, &bn128Pi, BN128_PI_SHIFT+1 - shift );
  if( bn128CmpGt( &base, &bnhpi ) )
  {
    bn128SetSub( &tmp, &bnpi, &base );
    bn128Set( &base, &tmp );
    negflag = 1;
  }

  bn128Set32Shl( &one, 1, shift );
  bn128SquareShr( &basesq, &base, shift );
  bn128Set( dst, &one );
  bn128SetShr1( &term0, &basesq );
  bn128Sub( dst, &term0 );
  for( divisor = 4 ; divisor < BN128_DIV_TABLE_SIZE ; divisor += 4 )
  {
#ifdef BN128_USE_INVERSE_DIVISION
    divproduct = (divisor-1) * (divisor+0);
    if( divproduct < BN128_DIV_TABLE_SIZE )
      bn128MulShr( &tmp, &basesq, &bn128DivFactor[divproduct], BN128_DIV_SHIFT );
    else
    {
      bn128MulShr( &tmp2, &bn128DivFactor[divisor-1], &bn128DivFactor[divisor+0], BN128_DIV_SHIFT );
      bn128MulShr( &tmp, &basesq, &tmp2, BN128_DIV_SHIFT );
    }
#else
    bn128Set( &tmp, &basesq );
    bn128Div32Round( &tmp, (divisor-1)*(divisor+0) );
#endif
    bn128MulShr( &term1, &tmp, &term0, shift );
    if( bn128CmpEqOrZero( &term1, &term0 ) )
      break;
    if( bn128CmpZero( &term1 ) )
      break;
    bn128Add( dst, &term1 );
#ifdef BN128_USE_INVERSE_DIVISION
    divproduct = (divisor+1) * (divisor+2);
    if( divproduct < BN128_DIV_TABLE_SIZE )
      bn128MulShr( &tmp, &basesq, &bn128DivFactor[divproduct], BN128_DIV_SHIFT );
    else
    {
      bn128MulShr( &tmp2, &bn128DivFactor[divisor+1], &bn128DivFactor[divisor+2], BN128_DIV_SHIFT );
      bn128MulShr( &tmp, &basesq, &tmp2, BN128_DIV_SHIFT );
    }
#else
    bn128Set( &tmp, &basesq );
    bn128Div32Round( &tmp, (divisor+1)*(divisor+2) );
#endif
    bn128MulShr( &term0, &tmp, &term1, shift );
    if( bn128CmpEqOrZero( &term0, &term1 ) )
      break;
    bn128Sub( dst, &term0 );
  }
  if( negflag )
    bn128Neg( dst );
#ifdef BN128_PRINT_REPORT
  printf( "Cos Iteration Count : %d\n", (divisor-4)/4 );
#endif

  return;
}


void bn128Sin( bn128 *dst, const bn128 * const src, bnShift shift )
{
  bn128 bncos, bnhpi;
  bn128ShrRound( &bnhpi, &bn128Pi, BN128_PI_SHIFT+1 - shift );
  bn128SetSub( &bncos, src, &bnhpi );
  bn128Cos( dst, &bncos, shift );
  return;
}


void bn128Tan( bn128 *dst, const bn128 * const src, bnShift shift )
{
  bn128 bncos;
  bn128Cos( &bncos, src, shift );
  if( bn128CmpNotZero( &bncos ) )
  {
    bn128Sin( dst, src, shift );
    bn128DivRoundSignedShl( dst, &bncos, shift );
  }
  else
    bn128Set( dst, bn128CmpPositive( src ) ? &bn128InfPos : &bn128InfNeg );
  return;
}



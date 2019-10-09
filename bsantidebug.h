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

#if BS_ENABLE_LIMITS


 #define BS_LIMIT_HIDE_CHAR(x) (((x)^0x13)+0xf)
 #define BS_LIMIT_UNHIDE_CHAR(x) (((x)-0xf)^0x13)


extern char bsInventoryLimitsWarning[];
extern char bsInventoryLimitsError[];
extern char bsInventoryLimitsRegister[];


void bsAntiDebugUnpack();


#endif



////



#if BS_ENABLE_ANTIDEBUG


 #define BS_ANTIDEBUG_CONTEXT_VALUE (0x840)
 #define BS_ANTIDEBUG_CONTEXT_VALUE_MASK (0x18c2)

 #define BS_ANTIDEBUG_CONTEXT_TIME_SHIFT (30)
 #define BS_ANTIDEBUG_CONTEXT_TIME_MASK (~((((uint64_t)1)<<BS_ANTIDEBUG_CONTEXT_TIME_SHIFT)-1))

 #define BS_ANTIDEBUG_INIT_OFFSET (0x3d80)

 #define BS_ANTIDEBUG_BLAPPLY_OFFSET (0xfd5) /* ccHash32Int32Inline(BS_ANTIDEBUG_CONTEXT_VALUE|0x0)&0xffff */

 #define BS_ANTIDEBUG_BOAPPLY_RAW (0x9c51)
 #define BS_ANTIDEBUG_BOAPPLY_OFFSET (0x22a9) /* ccHash32Int32Inline(BS_ANTIDEBUG_BOAPPLY_RAW)&0xffff */



int bsAntiDebugInit( bsContext *context, cpuInfo *cpuinfo );

void bsAntiDebugMid( bsContext *context );

void bsAntiDebugEnd( bsContext *context );


////


/* Increment the counter every call, to externally make sure the Checks() are being executed */
/* Important, read back this address through pointer arithmetics, not directly! We must hide access. */
extern uint64_t bsAntiDebugCheckCounter;

extern volatile uintptr_t bsAntiDebugInitOffset;

extern volatile uint32_t bsAntiDebugBoApplyOffset;

void __attribute__((noinline)) bsAntiDebugCountGetPointers( bsContext *context, int32_t **antidebugcount0, int32_t **antidebugcount1 );


#define BS_ANTIDEBUGCOUNT_OFFSET0 (48)
#define BS_ANTIDEBUGCOUNT_OFFSET1 (24)


static inline void __attribute__((always_inline)) bsAntiDebugCountReset( bsContext *context )
{
  int32_t *antidebugcount0;
  int32_t *antidebugcount1;
  bsAntiDebugCountGetPointers( context, &antidebugcount0, &antidebugcount1 );
  *((int32_t *)ADDRESS( antidebugcount0, -BS_ANTIDEBUGCOUNT_OFFSET0 )) = context->antidebugzero;
  *((int32_t *)ADDRESS( antidebugcount1, -BS_ANTIDEBUGCOUNT_OFFSET1 )) = context->antidebugzero;
  return;
}


static inline void __attribute__((always_inline)) bsAntiDebugCountInv( bsContext *context, const int varshift )
{
  int itemindex, partcount, varmask;
  bsxInventory *inv;
  bsxItem *item;
  int32_t * volatile antidebugcount1;

  antidebugcount1 = ADDRESS( context, offsetof(bsContext,antidebugcount1) + BS_ANTIDEBUGCOUNT_OFFSET1 );
  inv = context->inventory;
  item = inv->itemlist;
  partcount = 0;
  varmask = BS_LIMITS_HARDMAX_PARTCOUNT_MASK >> varshift;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    partcount += item->quantity;
    context->antidebugcount0 |= ( partcount >> varshift );
  }
  /* Increment check counter */
  bsAntiDebugCheckCounter += partcount;
  /* These masks should be zero, otherwise the inventory maximum is being exceeded */
  context->antidebugcount0 &= varmask;
  *((int32_t *)ADDRESS( antidebugcount1, -BS_ANTIDEBUGCOUNT_OFFSET1 )) |= ( partcount >> varshift ) & varmask;
  return;
}


#endif



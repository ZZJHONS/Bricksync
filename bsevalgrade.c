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
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cc.h"
#include "ccstr.h"
#include "debugtrack.h"

#include "bsevalgrade.h"


////


#define BS_EVAL_DEBUG (0)


////


#define BS_EVAL_MODIFIER_BASE (0.5)
#define BS_EVAL_INVERTER_OFFSET (0.8)
#define BS_EVAL_THRESHOLD_LIKE_NEW (0.7)
#define BS_EVAL_THRESHOLD_GOOD (0.4)

#define BS_EVAL_MODCLASS_NONE (0x1)
#define BS_EVAL_MODCLASS_SIZE (0x2)
#define BS_EVAL_MODCLASS_QUANTITY (0x4)


typedef struct
{
  int modclass;
  float evalbase, evalsum, modifier, modinv;
} bsEvaluatePartState;

static void bsEvaluateApplyStatement( bsEvaluatePartState *state, float statvalue, float scale )
{
  if( state->modinv < 0.0 )
    statvalue += ( statvalue < 0.0 ? BS_EVAL_INVERTER_OFFSET : -BS_EVAL_INVERTER_OFFSET );
  statvalue *= state->modifier;
  statvalue = fmaxf( fminf( statvalue, 1.0 ), -1.0 );
  state->evalbase += scale * ( 0.5 + (0.5*statvalue) );
  state->evalsum += scale;
  state->modifier = BS_EVAL_MODIFIER_BASE;
  state->modinv = 1.0;
  state->modclass = BS_EVAL_MODCLASS_NONE;
  return;
}

static void bsEvalulateCheckModClass( bsEvaluatePartState *state, int preserveclassflags )
{
  if( !( state->modclass & preserveclassflags ) )
  {
    state->modifier = BS_EVAL_MODIFIER_BASE;
    state->modinv = 1.0;
  }
  return;
}

int bsEvalulatePartCondition( char *comments )
{
  int commentlength, evalcondition, statlen, statlast;
  char *editcomments;
  char *statement, *str;
  char c;
  bsEvaluatePartState state;

  DEBUG_SET_TRACKER();

  if( !( comments ) )
    return BS_EVAL_CONDITION_GOOD;

  commentlength = strlen( comments );
  editcomments = malloc( commentlength + 1 );
  ccStrLowCopy( editcomments, comments, commentlength );
  editcomments[commentlength] = 0;

  state.evalbase = 0.5;
  state.evalsum = 1.0;
  statlast = 0;
  for( statement = editcomments ; !( statlast ) ; )
  {
    for( statlen = 0 ; ; statlen++ )
    {
      c = statement[statlen];
      if( !( c ) )
      {
        statlast = 1;
        break;
      }
      if( ( c >= 'a' ) && ( c <= 'z' ) )
        continue;
      if( ( c >= 'A' ) && ( c <= 'Z' ) )
        continue;
      if( ( c >= '0' ) && ( c <= '9' ) )
        continue;
      if( c == ' ' )
        continue;
      if( c == '\t' )
        continue;
      break;
    }
    statement[statlen] = 0;

    state.modifier = BS_EVAL_MODIFIER_BASE;
    state.modclass = BS_EVAL_MODCLASS_NONE;
    state.modinv = 1.0;
    for( str = statement ; ; )
    {
      str = ccStrNextWord( str );
      if( !( str ) )
        break;

      /* Statements */
      if( ccStrCmpSeq( str, "scratch", strlen( "scratch" ) ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_SIZE | BS_EVAL_MODCLASS_QUANTITY );
        bsEvaluateApplyStatement( &state, -0.75, 4.0 );
      }
      else if( ccStrCmpWord( str, "new" ) || ccStrCmpWord( str, "excellent" ) || ccStrCmpWord( str, "perfect" ) || ccStrCmpWord( str, "perfectly" ) || ccStrCmpWord( str, "mint" ) || ccStrCmpWord( str, "fantastic" ) || ccStrCmpWord( str, "amazing" ) || ccStrCmpWord( str, "great" ) || ccStrCmpWord( str, "sealed" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        state.modifier *= 2.0;
        bsEvaluateApplyStatement( &state, 1.00, 4.0 );
      }
      else if( ccStrCmpWord( str, "good" ) || ccStrCmpWord( str, "nice" ) || ccStrCmpWord( str, "clean" ) || ccStrCmpWord( str, "work" ) || ccStrCmpWord( str, "works" ) || ccStrCmpWord( str, "working" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        bsEvaluateApplyStatement( &state, 0.60, 3.0 );
      }
      else if( ccStrCmpWord( str, "okay" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        bsEvaluateApplyStatement( &state, 0.40, 1.0 );
      }
      else if( ccStrCmpWord( str, "discolored" ) || ccStrCmpWord( str, "faded" ) || ccStrCmpWord( str, "fading" ) || ccStrCmpWord( str, "yellowed" ) || ccStrCmpWord( str, "yellowing" ) || ccStrCmpWord( str, "dented" ) || ccStrCmpWord( str, "tooth" ) || ccStrCmpWord( str, "broke" ) || ccStrCmpWord( str, "broken" ) || ccStrCmpWord( str, "damaged" ) || ccStrCmpWord( str, "cracked" ) || ccStrCmpWord( str, "chewed" ) || ccStrCmpWord( str, "bite" ) || ccStrCmpWord( str, "gash" ) || ccStrCmpWord( str, "gone" ) || ccStrCmpWord( str, "missing" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        state.modifier *= 1.5;
        bsEvaluateApplyStatement( &state, -1.00, 4.0 );
      }
      else if( ccStrCmpWord( str, "worn" ) || ccStrCmpWord( str, "wearing" ) || ccStrCmpWord( str, "wear" ) || ccStrCmpWord( str, "marks" ) || ccStrCmpWord( str, "rub off" ) || ccStrCmpWord( str, "rubbed off" ) || ccStrCmpWord( str, "rubbing off" ) || ccStrCmpWord( str, "wore off" ) || ccStrCmpWord( str, "worn off" ) || ccStrCmpWord( str, "wearing off" ) || ccStrCmpWord( str, "distorted" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        bsEvaluateApplyStatement( &state, -0.80, 3.0 );
      }
      else if( ccStrCmpWord( str, "used" ) || ccStrCmpWord( str, "bad" ) || ccStrCmpWord( str, "poor" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        bsEvaluateApplyStatement( &state, -0.80, 3.0 );
      }
      else if( ccStrCmpWord( str, "acceptable" ) || ccStrCmpWord( str, "stress" ) )
      {
        bsEvalulateCheckModClass( &state, 0 );
        bsEvaluateApplyStatement( &state, -0.60, 3.0 );
      }
      /* state.modifiers */
      else if( ccStrCmpWord( str, "deep" ) || ccStrCmpWord( str, "big" ) || ccStrCmpWord( str, "large" ) || ccStrCmpWord( str, "huge" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_SIZE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 2.0;
        state.modclass = BS_EVAL_MODCLASS_SIZE;
      }
      else if( ccStrCmpWord( str, "small" ) || ccStrCmpWord( str, "tiny" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_SIZE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 0.5;
        state.modclass = BS_EVAL_MODCLASS_SIZE;
      }
      else if( ccStrCmpWord( str, "many" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 2.0;
        state.modclass = BS_EVAL_MODCLASS_QUANTITY;
      }
      else if( ccStrCmpWord( str, "few" ) || ccStrCmpWord( str, "couple" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 0.5;
        state.modclass = BS_EVAL_MODCLASS_QUANTITY;
      }
      else if( ccStrCmpWord( str, "very" ) || ccStrCmpWord( str, "extra" ) || ccStrCmpWord( str, "heavy" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 2.0;
        state.modclass = BS_EVAL_MODCLASS_NONE;
      }
      else if( ccStrCmpWord( str, "too" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 1.2;
        state.modclass = BS_EVAL_MODCLASS_NONE;
      }
      else if( ccStrCmpWord( str, "slightly" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE | BS_EVAL_MODCLASS_QUANTITY );
        state.modifier *= 0.5;
        state.modclass = BS_EVAL_MODCLASS_NONE;
      }
      else if( ccStrCmpWord( str, "like" ) )
        state.modifier *= 0.9;
      else if( ccStrCmpWord( str, "quite" ) )
        state.modifier *= 0.9;
      else if( ccStrCmpWord( str, "almost" ) )
        state.modifier *= 0.8;
      else if( ccStrCmpWord( str, "play" ) )
        state.modifier *= 1.0;
      /* Inverters */
      else if( ccStrCmpWord( str, "no" ) || ccStrCmpWord( str, "not" ) )
      {
        bsEvalulateCheckModClass( &state, BS_EVAL_MODCLASS_NONE );
        state.modinv *= -1.0;
        state.modclass = BS_EVAL_MODCLASS_NONE;
      }
      else
      {
        /* Unknown keyword, reset the modifier */
        state.modifier = BS_EVAL_MODIFIER_BASE;
        state.modinv = 1.0;
        state.modclass = BS_EVAL_MODCLASS_NONE;
      }

      str = ccStrSkipWord( str );
      if( !( str ) )
        break;
      continue;
    }

    statement += statlen + 1;
  }

  free( editcomments );
  state.evalbase /= state.evalsum;
  if( state.evalbase >= BS_EVAL_THRESHOLD_LIKE_NEW )
    evalcondition = BS_EVAL_CONDITION_LIKE_NEW;
  else if( state.evalbase >= BS_EVAL_THRESHOLD_GOOD )
    evalcondition = BS_EVAL_CONDITION_GOOD;
  else
    evalcondition = BS_EVAL_CONDITION_ACCEPTABLE;
#if BS_EVAL_DEBUG
  printf( "Comments: \"%s\"\n", comments );
  switch( evalcondition )
  {
    case BS_EVAL_CONDITION_LIKE_NEW:
      printf( "Eval: Like New (%.2f)\n", state.evalbase );
      break;
    case BS_EVAL_CONDITION_GOOD:
      printf( "Eval: Good (%.2f)\n", state.evalbase );
      break;
    case BS_EVAL_CONDITION_ACCEPTABLE:
      printf( "Eval: Acceptable (%.2f)\n", state.evalbase );
      break;
  }
  printf( "\n" );
#endif

  return evalcondition;
}


////



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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"
#include "iolog.h"
#include "debugtrack.h"
#include "cpuinfo.h"
#include "rand.h"

#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <direct.h>
#else
 #error Unknown/Unsupported platform!
#endif

#include "tcp.h"
#include "tcphttp.h"
#include "oauth.h"
#include "exclperm.h"
#include "journal.h"

#include "bsx.h"
#include "bsxpg.h"
#include "json.h"
#include "bsorder.h"
#include "bricklink.h"
#include "brickowl.h"
#include "colortable.h"
#include "bstranslation.h"
#include "bsevalgrade.h"

#include "bricksync.h"


////


#define BS_COMMAND_LOCATE_ITEM_PARAM_MAX (4)

static bsxItem *bsCmdLocateItem( bsContext *context, bsxInventory *inv, char *str )
{
  int index, length, paramcount, paramflags;
  int32_t colorid;
  int64_t boid, lotid;
  bsxItem *item;
  char *strend;
  char *params[BS_COMMAND_LOCATE_ITEM_PARAM_MAX+1];
  char condition, separator;

  if( !( str ) )
    goto syntaxerror;
  strend = ccStrEndWord( str );
  if( !( strend ) )
    goto syntaxerror;
  *strend = 0;
  length = (int)( strend - str );
  params[0] = str;
  paramcount = 1;
  paramflags = 0x0;

/*
paramflags : 0x0 : BlLotID
paramflags : 0x1 : BOID-BoColor-Condition
paramflags : 0x2 : BL_ID:BlColor:Condition
paramflags : 0x4 : BoLotID
*/
  for( index = 0 ; index < length ; index++ )
  {
    if( str[index] == '-' )
      paramflags |= 0x1;
    else if( str[index] == ':' )
      paramflags |= 0x2;
    else if( str[index] == '*' )
      paramflags |= 0x4;
  }

  separator = 0;
  if( paramflags & 0x2 )
  {
    paramflags = 0x2;
    separator = ':';
  }
  else if( paramflags & 0x1 )
  {
    paramflags = 0x1;
    separator = '-';
  }

  if( separator )
  {
    for( index = 0 ; index < length ; index++ )
    {
      if( str[index] == separator )
      {
        str[index] = 0;
        params[ paramcount++ ] = &str[index+1];
      }
      if( paramcount >= (BS_COMMAND_LOCATE_ITEM_PARAM_MAX+1) )
        goto syntaxerror;
    }
  }

#if 0
  printf( "COMMAND: Item flags 0x%x\n", paramflags );
  for( index = 0 ; index < paramcount ; index++ )
    printf( "COMMAND: Item param %d : %s\n", index, params[index] );
#endif

  if( paramflags == 0x0 )
  {
    if( !( ccStrParseInt64( params[0], &lotid ) ) )
      goto syntaxerror;
    item = bsxFindLotID( inv, lotid );
    if( !( item ) )
      ioPrintf( &context->output, 0, BSMSG_ERROR "No item found with BrickLink LotID " IO_RED CC_LLD IO_WHITE ".\n", (long long)lotid );
  }
  else if( ( paramflags == 0x1 ) || ( paramflags == 0x2 ) )
  {
    colorid = 0;
    condition = 'N';
    if( paramcount >= 2 )
    {
      if( !( ccStrParseInt32( params[1], &colorid ) ) )
        goto syntaxerror;
    }
    if( paramcount == 3 )
    {
      if( params[2][1] != 0 )
        goto syntaxerror;
      if( ( params[2][0] == 'n' ) || ( params[2][0] == 'N' ) )
        condition = 'N';
      else if( ( params[2][0] == 'u' ) || ( params[2][0] == 'U' ) )
        condition = 'U';
      else
        goto syntaxerror;
    }
    if( paramflags == 0x1 )
    {
      if( !( ccStrParseInt64( params[0], &boid ) ) )
        goto syntaxerror;
      item = bsxFindItemBOID( inv, boid, bsTranslateColorBo2Bl( colorid ), condition );
      if( !( item ) )
        ioPrintf( &context->output, 0, BSMSG_ERROR "No item found with BOID " IO_RED CC_LLD IO_WHITE ", BoColor " IO_RED "%d" IO_WHITE ", Condition " IO_RED "%c" IO_WHITE ".\n", (long long)boid, (int)colorid, condition );
    }
    else
    {
      item = bsxFindItemNoType( inv, params[0], colorid, condition );
      if( !( item ) )
        ioPrintf( &context->output, 0, BSMSG_ERROR "No item found with BLID " IO_RED "%s" IO_WHITE ", BlColor " IO_RED "%d" IO_WHITE ", Condition " IO_RED "%c" IO_WHITE ".\n", params[0], (int)colorid, condition );
    }
  }
  else if( paramflags == 0x4 )
  {
    if( paramcount != 2 )
      goto syntaxerror;
    if( !( ccStrParseInt64( params[1], &lotid ) ) )
      goto syntaxerror;
    item = bsxFindOwlLotID( inv, lotid );
    if( !( item ) )
      ioPrintf( &context->output, 0, BSMSG_ERROR "No item found with BrickOwl LotID " IO_RED CC_LLD IO_WHITE ".\n", (long long)lotid );
  }
  else
    goto syntaxerror;

  return item;

  syntaxerror:
  ioPrintf( &context->output, 0, BSMSG_ERROR "Item identifier syntax error. Can be any of: BlLotID *BoLotID BL_ID:BlColor:Condition BOID-BoColor-Condition\n" );
  return 0;
}


////


#define BS_PRINT_ITEM_BOLD_BLID (1<<0)
#define BS_PRINT_ITEM_BOLD_BOID (1<<1)
#define BS_PRINT_ITEM_BOLD_NAME (1<<2)
#define BS_PRINT_ITEM_BOLD_TYPENAME (1<<3)
#define BS_PRINT_ITEM_BOLD_TYPEID (1<<4)
#define BS_PRINT_ITEM_BOLD_COLORNAME (1<<5)
#define BS_PRINT_ITEM_BOLD_BLCOLORID (1<<6)
#define BS_PRINT_ITEM_BOLD_BOCOLORID (1<<7)
#define BS_PRINT_ITEM_BOLD_CATEGORYNAME (1<<8)
#define BS_PRINT_ITEM_BOLD_CATEGORYID (1<<9)
#define BS_PRINT_ITEM_BOLD_CONDITION (1<<10)
#define BS_PRINT_ITEM_BOLD_QUANTITY (1<<11)
#define BS_PRINT_ITEM_BOLD_PRICE (1<<12)
#define BS_PRINT_ITEM_BOLD_BULK (1<<13)
#define BS_PRINT_ITEM_BOLD_COMMENTS (1<<14)
#define BS_PRINT_ITEM_BOLD_REMARKS (1<<15)
#define BS_PRINT_ITEM_BOLD_BLLOTID (1<<16)
#define BS_PRINT_ITEM_BOLD_BOLOTID (1<<17)

static void bsCmdPrintItem( bsContext *context, bsxItem *item, uint32_t boldmask )
{
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BLID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "BLID:        %s\n", ( item->id ? item->id : "???" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BOID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "BOID:        "CC_LLD"\n", (long long)item->boid );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_NAME ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Name:        \"%s\"\n", ( item->name ? item->name : "???" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_TYPENAME ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Type Name:   \"%s\"\n", ( item->typename ? item->typename : "???" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_TYPEID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Type ID:     \"%c\"\n", item->typeid );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_COLORNAME ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Color Name:  \"%s\"\n", ( item->colorname ? item->colorname : "???" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BLCOLORID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "BL Color ID: %d\n", item->colorid );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BOCOLORID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "BO Color ID: %d\n", bsTranslateColorBl2Bo( item->colorid ) );
  if( item->categoryname )
  {
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_CATEGORYNAME ? IO_CYAN : IO_DEFAULT ) );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "Category:    \"%s\"\n", ( item->categoryname ? item->categoryname : "???" ) );
  }
  if( item->categoryid )
  {
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_CATEGORYID ? IO_CYAN : IO_DEFAULT ) );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, "Category ID: %d\n", item->categoryid );
  }
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_CONDITION ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Condition:   \"%s\"\n", ( item->condition == 'N' ? "New" : "Used" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_QUANTITY ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Quantity:    %d\n", item->quantity );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_PRICE ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Price:       %.3f\n", item->price );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BULK ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Bulk:        %d\n", item->bulk );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_COMMENTS ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Comments:    \"%s\"\n", ( item->comments ? item->comments : "" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_REMARKS ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "Remarks:     \"%s\"\n", ( item->remarks ? item->remarks : "" ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BLLOTID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "BL Lot ID:   \""CC_LLD"\"\n", (long long)item->lotid );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "  %s", ( boldmask & BS_PRINT_ITEM_BOLD_BOLOTID ? IO_CYAN : IO_DEFAULT ) );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "BO Lot ID:   \""CC_LLD"\"\n", (long long)item->bolotid );
  return;
}


static int bsCmdFindNumericalArg( char *arg )
{
  for( ; *arg ; arg++ )
  {
    if( ( ( *arg < '0' ) || ( *arg > '9' ) ) && ( *arg != '.' ) )
      return 0;
  }
  return 1;
}


static void bsCmdInventoryModified( bsContext *context )
{
  journalDef journal;

  journalAlloc( &journal, 4 );
  if( !( bsSaveInventory( context, &journal ) ) )
  {
    bsFatalError( context );
    return;
  }
  context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE | BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE;
  if( !( bsSaveState( context, &journal ) ) )
  {
    bsFatalError( context );
    return;
  }
  if( !( journalExecute( BS_JOURNAL_FILE, BS_JOURNAL_TEMP_FILE, &context->output, journal.entryarray, journal.entrycount ) ) )
  {
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_ERROR "Failed to execute journal!\n" );
    bsFatalError( context );
    return;
  }
  journalFree( &journal );

  return;
}


////


#define BS_COMMAND_FIND_MAX (16)

typedef struct
{
  int findcount;
  int64_t findint[BS_COMMAND_FIND_MAX];
  float findfloat[BS_COMMAND_FIND_MAX];
  char *findstring[BS_COMMAND_FIND_MAX];
  int activeitemindex;
} bsCmdFind;

static void bsCmdFindInit( bsCmdFind *cmdfind, int findcount, char **findterms )
{
  int findindex;
  if( findcount > BS_COMMAND_FIND_MAX )
    findcount = BS_COMMAND_FIND_MAX;
  for( findindex = 0 ; findindex < findcount ; findindex++ )
  {
    if( !( findterms[findindex] ) )
      break;
    cmdfind->findstring[findindex] = findterms[findindex];
    cmdfind->findint[findindex] = -1;
    cmdfind->findfloat[findindex] = -1.0;
    if( bsCmdFindNumericalArg( findterms[findindex] ) )
    {
      if( !( ccStrParseInt64( cmdfind->findstring[findindex], &cmdfind->findint[findindex] ) ) )
        cmdfind->findint[findindex] = -1;
      if( !( ccStrParseFloat( cmdfind->findstring[findindex], &cmdfind->findfloat[findindex] ) ) )
        cmdfind->findfloat[findindex] = -1.0;
    }
  }
  cmdfind->findcount = findindex;
  cmdfind->activeitemindex = 0;
  return;
}

static void bsCmdFindReset( bsCmdFind *cmdfind )
{
  cmdfind->activeitemindex = 0;
  return;
}

static bsxItem *bsCmdFindInv( bsCmdFind *cmdfind, bsxInventory *inv, int *retmatchmask )
{
  int itemindex, findindex, matchcount, matchmask, matchpartmask;
  bsxItem *item;

  itemindex = cmdfind->activeitemindex;

  item = &inv->itemlist[ itemindex ];
  for( ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    matchcount = 0;

    matchmask = 0;
    for( findindex = 0 ; findindex < cmdfind->findcount ; findindex++ )
    {
      matchpartmask = 0;
      if( ccStrFindStr( item->id, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_BLID;
      if( ccStrFindStr( item->name, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_NAME;
      if( ccStrFindStr( item->typename, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_TYPENAME;
      if( ccStrFindStr( item->colorname, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_COLORNAME;
      if( ccStrFindStr( item->categoryname, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_CATEGORYNAME;
      if( ccStrFindStr( item->comments, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_COMMENTS;
      if( ccStrFindStr( item->remarks, cmdfind->findstring[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_REMARKS;
      if( cmdfind->findint[findindex] >= 0 )
      {
        if( item->boid == cmdfind->findint[findindex] )
          matchpartmask |= BS_PRINT_ITEM_BOLD_BOID;
        if( item->colorid == cmdfind->findint[findindex] )
          matchpartmask |= BS_PRINT_ITEM_BOLD_BLCOLORID;
        if( bsTranslateColorBl2Bo( item->colorid ) == cmdfind->findint[findindex] )
          matchpartmask |= BS_PRINT_ITEM_BOLD_BOCOLORID;
        if( item->quantity == cmdfind->findint[findindex] )
          matchpartmask |= BS_PRINT_ITEM_BOLD_QUANTITY;
        if( item->lotid == cmdfind->findint[findindex] )
          matchpartmask |= BS_PRINT_ITEM_BOLD_BLLOTID;
        if( item->bolotid == cmdfind->findint[findindex] )
          matchpartmask |= BS_PRINT_ITEM_BOLD_BOLOTID;
      }
      if( ( cmdfind->findfloat[findindex] >= 0.0 ) && bsInvPriceEqual( item->price, cmdfind->findfloat[findindex] ) )
        matchpartmask |= BS_PRINT_ITEM_BOLD_PRICE;
      if( !( matchpartmask ) )
        break;
      matchmask |= matchpartmask;
      matchcount++;
    }

    if( ( matchcount == cmdfind->findcount ) && ( matchmask ) )
    {
      cmdfind->activeitemindex = itemindex + 1;
      if( retmatchmask )
        *retmatchmask = matchmask;
      return item;
    }
  }

  return 0;
}


////


#define BS_COMMAND_ARGSTD_FLAG_FORCE (0x1)
#define BS_COMMAND_ARGSTD_FLAG_VERBOSE (0x2)
#define BS_COMMAND_ARGSTD_FLAG_SHORT (0x4)
#define BS_COMMAND_ARGSTD_FLAG_PRETEND (0x8)

static int bsCmdArgStdParse( bsContext *context, int argc, char **argv, int paramlistmin, int paramlistmax, char **retparamlist, int *retcmdflags, int optcmdflags )
{
  int argindex, cmdflags, paramlistindex;
  char *argstring;

  paramlistindex = 0;
  memset( retparamlist, 0, paramlistmax * sizeof(char *) );
  cmdflags = 0x0;
  for( argindex = 1 ; argindex < argc ; argindex++ )
  {
    argstring = argv[argindex];
    if( argstring[0] == '-' )
    {
      if( ccStrCmpEqual( &argstring[1], "f" ) )
      {
        if( !( optcmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ) )
        {
          ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid flag \"" IO_MAGENTA "-%c" IO_WHITE "\" for command.\n", (char)argstring[1] );
          return 0;
        }
        cmdflags |= BS_COMMAND_ARGSTD_FLAG_FORCE;
      }
      else if( ccStrCmpEqual( &argstring[1], "v" ) )
      {
        if( !( optcmdflags & BS_COMMAND_ARGSTD_FLAG_VERBOSE ) )
        {
          ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid flag \"" IO_MAGENTA "-%c" IO_WHITE "\" for command.\n", (char)argstring[1] );
          return 0;
        }
        cmdflags |= BS_COMMAND_ARGSTD_FLAG_VERBOSE;
      }
      else if( ccStrCmpEqual( &argstring[1], "s" ) )
      {
        if( !( optcmdflags & BS_COMMAND_ARGSTD_FLAG_SHORT ) )
        {
          ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid flag \"" IO_MAGENTA "-%c" IO_WHITE "\" for command.\n", (char)argstring[1] );
          return 0;
        }
        cmdflags |= BS_COMMAND_ARGSTD_FLAG_SHORT;
      }
      else if( ccStrCmpEqual( &argstring[1], "p" ) )
      {
        if( !( optcmdflags & BS_COMMAND_ARGSTD_FLAG_PRETEND ) )
        {
          ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid flag \"" IO_MAGENTA "-%c" IO_WHITE "\" for command.\n", (char)argstring[1] );
          return 0;
        }
        cmdflags |= BS_COMMAND_ARGSTD_FLAG_PRETEND;
      }
    }
    else
    {
      if( paramlistindex >= paramlistmax )
      {
        if( paramlistmax >= 2 )
          ioPrintf( &context->output, 0, BSMSG_ERROR "Command only allows %d parameters, you have an extra parameter \"" IO_MAGENTA "%s" IO_WHITE "\".\n", paramlistmax, argstring );
        else if( paramlistmax >= 1 )
          ioPrintf( &context->output, 0, BSMSG_ERROR "Command only allows one parameter, you specified both \"" IO_MAGENTA "%s" IO_WHITE "\" and \"" IO_MAGENTA "%s" IO_WHITE "\".\n", retparamlist[0], argstring );
        else
          ioPrintf( &context->output, 0, BSMSG_ERROR "Command does not allow a parameter \"" IO_MAGENTA "" IO_WHITE "\n", argstring );
        return 0;
      }
      retparamlist[ paramlistindex ] = argstring;
      paramlistindex++;
    }
  }
  if( paramlistindex < paramlistmin )
  {
    if( paramlistmin >= 2 )
      ioPrintf( &context->output, 0, BSMSG_ERROR "Command requires %d parameters.\n", paramlistmin );
    else
      ioPrintf( &context->output, 0, BSMSG_ERROR "Command requires a parameter.\n" );
    return 0;
  }

  if( retcmdflags )
    *retcmdflags = cmdflags;
  return 1;
}


////


static void bsPrintCheckSyncTimes( bsContext *context )
{
  time_t lasttime;
  struct tm timeinfo;
  char timebuf[64];
  ccGrowth growth;

  lasttime = context->bricklink.lastchecktime;
  if( lasttime )
  {
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)lasttime, 4 );
    timeinfo = *( localtime( &lasttime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    ioPrintf( &context->output, 0, BSMSG_INFO "Time of last BrickLink order check : " IO_CYAN "%s" IO_DEFAULT " (%s ago).\n", timebuf, growth.data );
    ccGrowthFree( &growth );
  }
  lasttime = context->brickowl.lastchecktime;
  if( lasttime )
  {
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)lasttime, 4 );
    timeinfo = *( localtime( &lasttime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    ioPrintf( &context->output, 0, BSMSG_INFO "Time of last BrickOwl order check  : " IO_CYAN "%s" IO_DEFAULT " (%s ago).\n", timebuf, growth.data );
    ccGrowthFree( &growth );
  }
  lasttime = context->bricklink.lastsynctime;
  if( lasttime )
  {
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)lasttime, 4 );
    timeinfo = *( localtime( &lasttime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    ioPrintf( &context->output, 0, BSMSG_INFO "Time of last BrickLink deep sync   : " IO_CYAN "%s" IO_DEFAULT " (%s ago).\n", timebuf, growth.data );
    ccGrowthFree( &growth );
  }
  lasttime = context->brickowl.lastsynctime;
  if( lasttime )
  {
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)lasttime, 4 );
    timeinfo = *( localtime( &lasttime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    ioPrintf( &context->output, 0, BSMSG_INFO "Time of last BrickOwl deep sync    : " IO_CYAN "%s" IO_DEFAULT " (%s ago).\n", timebuf, growth.data );
    ccGrowthFree( &growth );
  }

  return;
}


////


#define BS_LOADINV_VERIFYFLAGS_PRICES (0x1)
#define BS_LOADINV_VERIFYFLAGS_REMARKS (0x2)

static int bsLoadVerifyInventory( bsContext *context, bsxInventory *inv, int verifyflags )
{
  int warningcount, itemindex;
  bsxItem *item, *stockitem;
  bsxInventory *stockinv;

  stockinv = context->inventory;
  warningcount = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    stockitem = 0;
    if( item->bolotid >= 0 )
      stockitem = bsxFindOwlLotID( stockinv, item->bolotid );
    if( item->lotid >= 0 )
      stockitem = bsxFindLotID( stockinv, item->lotid );
    if( !( stockitem ) || !( ccStrCmpEqualTest( item->id, stockitem->id ) ) || ( item->typeid != stockitem->typeid ) || ( item->colorid != stockitem->colorid ) || ( item->condition != stockitem->condition ) )
    {
      if( ( item->bolotid >= 0 ) || ( item->lotid >= 0 ) )
        ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_FLUSH, "LOG: Mismatch for item with LotID " CC_LLD " and OwlLotID " CC_LLD ", dropping item's Lot ID references.\n", item->lotid, item->bolotid );
      item->lotid = -1;
      item->bolotid = -1;
    }
    if( ( verifyflags & BS_LOADINV_VERIFYFLAGS_PRICES ) && ( item->price <= 0.0005 ) )
    {
      ioPrintf( &context->output, 0, BSMSG_WARNING "Lot has price of zero : \"" IO_CYAN "%s" IO_WHITE "\" (" IO_GREEN "%s" IO_WHITE ") in \"" IO_CYAN "%s" IO_WHITE "\" and \"" IO_CYAN "%s" IO_WHITE "\", with quantity of " IO_GREEN "%d" IO_WHITE ".\n", ( item->name ? item->name : "???" ), ( item->id ? item->id : "???" ), ( item->colorname ? item->colorname : "???" ), ( item->condition == 'N' ? "New" : "Used" ), (int)item->quantity );
      item->price = 0.01;
      warningcount++;
    }
    if( ( verifyflags & BS_LOADINV_VERIFYFLAGS_REMARKS ) && !( item->remarks ) )
    {
      ioPrintf( &context->output, 0, BSMSG_WARNING "Lot has no remarks : \"" IO_CYAN "%s" IO_WHITE "\" (" IO_GREEN "%s" IO_WHITE ") in \"" IO_CYAN "%s" IO_WHITE "\" and \"" IO_CYAN "%s" IO_WHITE "\", with quantity of " IO_GREEN "%d" IO_WHITE ".\n", ( item->name ? item->name : "???" ), ( item->id ? item->id : "???" ), ( item->colorname ? item->colorname : "???" ), ( item->condition == 'N' ? "New" : "Used" ), (int)item->quantity );
      warningcount++;
    }
  }

  return warningcount;
}


////


static char bsFindItemTypeFromInvBLID( bsxInventory *inv, char *blid )
{
  int itemindex, typeidindex, matchpriority;
  static const char typeidlist[] = { 'P', 'M', 'S', 'G', 'I', 'O', '#', 0 };
  char matchitemtypeid;
  bsxItem *item;

  matchitemtypeid = 0;
  matchpriority = 255;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( ccStrCmpEqual( item->id, blid ) )
    {
      for( typeidindex = 0 ; typeidlist[ typeidindex ] ; typeidindex++ )
      {
        if( item->typeid == typeidlist[ typeidindex ] )
          break;
      }
      if( typeidindex < matchpriority )
      {
        matchitemtypeid = item->typeid;
        matchpriority = typeidindex;
      }
    }
  }

  return matchitemtypeid;
}


////


static void bsPrintCpuInfo( bsContext *context, cpuInfo *cpu )
{
  switch( cpu->arch )
  {
    case CPUINFO_ARCH_AMD64:
      ioPrintf( &context->output, 0, BSMSG_INFO "Arch : x86-64/AMD64\n" );
      break;
    case CPUINFO_ARCH_IA32:
      ioPrintf( &context->output, 0, BSMSG_INFO "Arch : x86/IA32\n" );
      break;
    case CPUINFO_ARCH_UNKNOWN:
    default:
      break;
  }
  if( cpu->vendorstring[0] )
    ioPrintf( &context->output, 0, BSMSG_INFO "Vendor : %s\n", cpu->vendorstring );
  if( cpu->identifier[0] )
    ioPrintf( &context->output, 0, BSMSG_INFO "Identifier : %s\n", cpu->identifier );
  if( cpu->family )
    ioPrintf( &context->output, 0, BSMSG_INFO "Family : %d\n", cpu->family );
  if( cpu->model )
    ioPrintf( &context->output, 0, BSMSG_INFO "Model  : %d\n", cpu->model );
  if( cpu->cacheline )
    ioPrintf( &context->output, 0, BSMSG_INFO "Cache Line Size  : %d bytes\n", cpu->cacheline );
  if( ( cpu->capclflush ) &&( cpu->clflushsize ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "Cache Flush Size : %d bytes\n", cpu->clflushsize );
  ioPrintf( &context->output, 0, BSMSG_INFO "Processor Layout\n" );
  if( cpu->socketlogicalcores )
    ioPrintf( &context->output, 0, BSMSG_INFO "  Socket Logical Cores  : %d\n", cpu->socketlogicalcores );
  if( cpu->socketphysicalcores )
    ioPrintf( &context->output, 0, BSMSG_INFO "  Socket Physical Cores : %d\n", cpu->socketphysicalcores );
  if( cpu->socketcount )
    ioPrintf( &context->output, 0, BSMSG_INFO "  Socket Count          : %d\n", cpu->socketcount );
  if( cpu->totalcorecount )
    ioPrintf( &context->output, 0, BSMSG_INFO "  Total Cores Count     : %d\n", cpu->totalcorecount );
  if( cpu->wordsize )
    ioPrintf( &context->output, 0, BSMSG_INFO "Word Size    : %d bits\n", cpu->wordsize );
  if( cpu->sysmemory )
    ioPrintf( &context->output, 0, BSMSG_INFO "User-Space Memory : "CC_LLD" bytes ( %.2fMb )\n", (long long)cpu->sysmemory, (double)cpu->sysmemory / 1048576.0 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Capabilities :" );
  if( cpu->capcmov )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " CMOV" );
  if( cpu->capclflush )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " CLFLUSH" );
  if( cpu->captsc )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " TSC" );
  if( cpu->capmmx )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " MMX" );
  if( cpu->capmmxext )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " MMXEXT" );
  if( cpu->cap3dnow )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " 3DNow" );
  if( cpu->cap3dnowext )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " 3DNowExt" );
  if( cpu->capsse )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSE" );
  if( cpu->capsse2 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSE2" );
  if( cpu->capsse3 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSE3" );
  if( cpu->capssse3 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSSE3" );
  if( cpu->capsse4p1 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSE4.1" );
  if( cpu->capsse4p2 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSE4.2" );
  if( cpu->capsse4a )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SSE4A" );
  if( cpu->capavx )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX" );
  if( cpu->capavx2 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX2" );
  if( cpu->capxop )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " XOP" );
  if( cpu->capfma3 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " FMA3" );
  if( cpu->capfma4 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " FMA4" );
  if( cpu->capmisalignsse )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " MisalignSSE" );
  if( cpu->capavx512f )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512F" );
  if( cpu->capavx512dq )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512DQ" );
  if( cpu->capavx512pf )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512PF" );
  if( cpu->capavx512er )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512ER" );
  if( cpu->capavx512cd )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512CD" );
  if( cpu->capavx512bw )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512BW" );
  if( cpu->capavx512vl )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AVX512VL" );
  if( cpu->capaes )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " AES" );
  if( cpu->capsha )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " SHA" );
  if( cpu->cappclmul )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " PCLMUL" );
  if( cpu->caprdrnd )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " RDRND" );
  if( cpu->caprdseed )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " RDSEED" );
  if( cpu->capcmpxchg16b )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " CMPXCHG16B" );
  if( cpu->cappopcnt )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " POPCNT" );
  if( cpu->caplzcnt )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " LZCNT" );
  if( cpu->capmovbe )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " MOVBE" );
  if( cpu->caprdtscp )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " RDTSCP" );
  if( cpu->capconstanttsc )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " ConstantTSC" );
  if( cpu->capf16c )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " F16C" );
  if( cpu->capbmi )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " BMI" );
  if( cpu->capbmi2 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " BMI2" );
  if( cpu->capbmi2 )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " TBM" );
  if( cpu->capadx )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " ADX" );
  if( cpu->caphyperthreading )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " HyperThreading" );
  if( cpu->capmwait )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " MWAIT" );
  if( cpu->caplongmode )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " LongMode" );
  if( cpu->capthermalsensor )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " ThermalSensor" );
  if( cpu->capclockmodulation )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " ClockModulation" );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" );
  if( ( cpu->cachesizeL1code > 0 ) && ( cpu->cachesizeL1data > 0 ) && ( cpu->cacheunifiedL1 ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "L1 Unified Cache Memory\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Cache Size    : %d kb\n", cpu->cachesizeL1code ); 
    ioPrintf( &context->output, 0, BSMSG_INFO "  Line Size     : %d bytes\n", cpu->cachelineL1code );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Associativity : %d way(s)\n", cpu->cacheassociativityL1code );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Shared        : %d core(s)\n", cpu->cachesharedL1code );
  }
  else
  {
    if( cpu->cachesizeL1code > 0 )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "L1 Code Cache Memory\n" );
      ioPrintf( &context->output, 0, BSMSG_INFO "  Cache Size    : %d kb\n", cpu->cachesizeL1code ); 
      ioPrintf( &context->output, 0, BSMSG_INFO "  Line Size     : %d bytes\n", cpu->cachelineL1code );
      ioPrintf( &context->output, 0, BSMSG_INFO "  Associativity : %d way(s)\n", cpu->cacheassociativityL1code );
      ioPrintf( &context->output, 0, BSMSG_INFO "  Shared        : %d core(s)\n", cpu->cachesharedL1code );
    }
    if( cpu->cachesizeL1data > 0  )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "L1 Data Cache Memory\n" );
      ioPrintf( &context->output, 0, BSMSG_INFO "  Cache Size    : %d kb\n", cpu->cachesizeL1data ); 
      ioPrintf( &context->output, 0, BSMSG_INFO "  Line Size     : %d bytes\n", cpu->cachelineL1data );
      ioPrintf( &context->output, 0, BSMSG_INFO "  Associativity : %d way(s)\n", cpu->cacheassociativityL1data );
      ioPrintf( &context->output, 0, BSMSG_INFO "  Shared        : %d core(s)\n", cpu->cachesharedL1data );
    }
  }
  if( cpu->cachesizeL2 > 0 )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "L2 Cache Memory\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Cache Size    : %d kb\n", cpu->cachesizeL2 ); 
    ioPrintf( &context->output, 0, BSMSG_INFO "  Line Size     : %d bytes\n", cpu->cachelineL2 );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Associativity : %d way(s)\n", cpu->cacheassociativityL2 );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Shared        : %d core(s)\n", cpu->cachesharedL2 );
  }
  if( cpu->cachesizeL3 > 0 )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "L3 Cache Memory\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Cache Size    : %d kb\n", cpu->cachesizeL3 ); 
    ioPrintf( &context->output, 0, BSMSG_INFO "  Line Size     : %d bytes\n", cpu->cachelineL3 );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Associativity : %d way(s)\n", cpu->cacheassociativityL3 );
    ioPrintf( &context->output, 0, BSMSG_INFO "  Shared        : %d core(s)\n", cpu->cachesharedL3 );
  }
  return;
}


static void bsPrintHelpItemCommand( bsContext *context, char *command, char *suffix )
{
  if( !( suffix ) )
    suffix = "";
  ioPrintf( &context->output, 0, BSMSG_INFO "The item identifier can be " IO_GREEN "BLLotID" IO_DEFAULT ", example: " IO_GREEN "%s 55985466 %s" IO_DEFAULT "\n", command, suffix );
  ioPrintf( &context->output, 0, BSMSG_INFO "The item identifier can be " IO_GREEN "*BOLotID" IO_DEFAULT ", example: " IO_GREEN "%s *2727357 %s" IO_DEFAULT "\n", command, suffix );
  ioPrintf( &context->output, 0, BSMSG_INFO "The item identifier can be " IO_GREEN "BLID:BLColor:Condition" IO_DEFAULT ", example: " IO_GREEN "%s 3001:11:N %s" IO_DEFAULT "\n", command, suffix );
  ioPrintf( &context->output, 0, BSMSG_INFO "The item identifier can be " IO_GREEN "BOID-BOColor-Condition" IO_DEFAULT ", example: " IO_GREEN "%s 771344-38-N %s" IO_DEFAULT "\n", command, suffix );
  ioPrintf( &context->output, 0, BSMSG_INFO "When applicable, the condition parameter (" IO_GREEN "N" IO_DEFAULT " or " IO_GREEN "U" IO_DEFAULT ") is optional, it is assumed " IO_GREEN "N" IO_DEFAULT " if ommited.\n", command, suffix );
  ioPrintf( &context->output, 0, BSMSG_INFO "If multiple items match (duplicate lots), only the first item found is displayed. See the " IO_CYAN "find" IO_DEFAULT " command to list all matches.\n", command, suffix );
  return;
}


////


void bsCommandStatus( bsContext *context, int argc, char **argv )
{
  int cmdflags;
  int64_t freediskspace;
  bsxInventory *inv, *deltainv;
  ccGrowth growth;
  char *colorstring;
  float apihistoryratio;
  // Set the current date and time for printout in the startup message
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  char cur_date_time[64];
  size_t ret = strftime(cur_date_time, sizeof(cur_date_time), "%Y-%m-%d %H:%M:%S", tm);
  // validate pointer to the time value
  assert(ret);

  if( !( bsCmdArgStdParse( context, argc, argv, 0, 0, 0, &cmdflags, BS_COMMAND_ARGSTD_FLAG_SHORT ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "status [-s]" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = context->inventory;
  bsxRecomputeTotals( inv );
  if( !( cmdflags & BS_COMMAND_ARGSTD_FLAG_SHORT ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync Status Report.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Software version : " IO_GREEN "%s" IO_DEFAULT " - " IO_GREEN "%s %s" IO_DEFAULT ".\n", BS_VERSION_STRING, __DATE__, __TIME__ );
  ioPrintf( &context->output, 0, BSMSG_INFO "Software launch time : " IO_CYAN "%s" IO_DEFAULT ".\n", cur_date_time);

  if( ( context->storename ) && ( context->username ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "Store name : " IO_GREEN "%s" IO_DEFAULT " by " IO_GREEN "%s" IO_DEFAULT ".\n", context->storename, context->username );
  else if( context->storename )
    ioPrintf( &context->output, 0, BSMSG_INFO "Store name : " IO_GREEN "%s" IO_DEFAULT ".\n", context->storename );
  ioPrintf( &context->output, 0, BSMSG_INFO "Tracked inventory : " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount - inv->itemfreecount );
  ioPrintf( &context->output, 0, BSMSG_INFO "Inventory sale price : " IO_GREEN "%.2f" IO_DEFAULT " %s.\n", inv->totalprice, context->storecurrency );
#if BS_ENABLE_REGISTRATION
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync registration status : %s.\n", ( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ? IO_GREEN "Registered" IO_DEFAULT : IO_YELLOW "Unregistered" IO_DEFAULT ) );
#endif
  ioPrintf( &context->output, 0, BSMSG_INFO "Autocheck mode is currently : %s.\n", ( context->contextflags & BS_CONTEXT_FLAGS_AUTOCHECK_MODE ? IO_GREEN "Enabled" IO_DEFAULT : IO_YELLOW "Disabled" IO_DEFAULT ) );
  if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MASTER_MODE )
    ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " is currently " IO_YELLOW "enabled" IO_DEFAULT ". All synchronization is suspended.\n" );
  ccGrowthInit( &growth, 512 );
  ccGrowthElapsedTimeString( &growth, (int64_t)context->bricklink.pollinterval, 4 );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink polling interval : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
  ccGrowthFree( &growth );
  ccGrowthInit( &growth, 512 );
  ccGrowthElapsedTimeString( &growth, (int64_t)context->bricklink.pollinterval, 4 );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl polling interval  : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
  ccGrowthFree( &growth );

  bsPrintCheckSyncTimes( context );

  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink has pending updates : %s.\n", ( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE ? IO_RED "True" IO_DEFAULT : IO_GREEN "False" IO_DEFAULT ) );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl has pending updates  : %s.\n", ( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE ? IO_RED "True" IO_DEFAULT : IO_GREEN "False" IO_DEFAULT ) );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink in sync : %s.\n", ( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ? IO_RED "False" IO_DEFAULT : IO_GREEN "True" IO_DEFAULT ) );
  if( ( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_SYNC ) && ( context->bricklink.synctime > context->curtime ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink will attempt SYNC again in " IO_GREEN "%d" IO_DEFAULT " seconds.\n", (int)( context->bricklink.synctime - context->curtime ) );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl in sync  : %s.\n", ( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_SYNC ? IO_RED "False" IO_DEFAULT : IO_GREEN "True" IO_DEFAULT ) );
  if( ( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_SYNC ) && ( context->brickowl.synctime > context->curtime ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl will attempt SYNC again in " IO_GREEN "%d" IO_DEFAULT " seconds.\n", (int)( context->brickowl.synctime - context->curtime ) );
  if( !( cmdflags & BS_COMMAND_ARGSTD_FLAG_SHORT ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink API connection status : %s.\n", ( httpGetStatus( context->bricklink.http ) ? IO_GREEN "Keep-alive, waiting" IO_DEFAULT : IO_GREEN "Closed" IO_DEFAULT ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink WEB connection status : %s.\n", ( httpGetStatus( context->bricklink.webhttp ) ? IO_GREEN "Keep-alive, waiting" IO_DEFAULT : IO_GREEN "Closed" IO_DEFAULT ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl API connection status  : %s.\n", ( httpGetStatus( context->brickowl.http ) ? IO_GREEN "Keep-alive, waiting" IO_DEFAULT : IO_GREEN "Closed" IO_DEFAULT ) );
  }

  apihistoryratio = (float)context->bricklink.apihistory.total / (float)context->bricklink.apicountlimit;
  if( apihistoryratio > 0.75 )
    colorstring = IO_RED;
  else if( apihistoryratio > 0.50 )
    colorstring = IO_YELLOW;
  else
    colorstring = IO_GREEN;
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink API usage : " "%s" "%d" IO_DEFAULT " (" "%s" "%.2f%%" IO_DEFAULT ") in the past 24 hours; " "%s" "%d" IO_DEFAULT " in the past hour.\n", colorstring, (int)context->bricklink.apihistory.total, colorstring, 100.0 * apihistoryratio, colorstring, bsApiHistoryCountPeriod( &context->bricklink.apihistory, 3600 ) );
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl API usage  : " IO_GREEN "%d" IO_DEFAULT " in the past 24 hours; " IO_GREEN "%d" IO_DEFAULT " in the past hour.\n", (int)context->brickowl.apihistory.total, bsApiHistoryCountPeriod( &context->brickowl.apihistory, 3600 ) );

  freediskspace = ccGetFreeDiskSpace( BS_BACKUP_DIR );
  if( freediskspace >= 0 )
  {
    if( freediskspace < ( 2 * BS_DISK_SPACE_CRITICAL ) )
      colorstring = IO_RED;
    else if( freediskspace < ( 2 * BS_DISK_SPACE_WARNING ) )
      colorstring = IO_YELLOW;
    else
      colorstring = IO_GREEN;
    ioPrintf( &context->output, 0, BSMSG_INFO "Available disk space : %s" CC_LLD " MB" IO_DEFAULT ".\n", colorstring, (long long)( freediskspace / 1048576 ) );
  }

  return;
}


static void bsCommandHelp( bsContext *context, int argc, char **argv )
{
  char *sysname, *pgcacheformat;
  cpuInfo cpuinfo;
  ccGrowth growth;

  if( argc <= 1 )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "BrickSync Help.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Type \"" IO_GREEN "help " IO_CYAN "[command]" IO_DEFAULT "\" for help on a specific command.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Type \"" IO_GREEN "help " IO_CYAN "[topic]" IO_DEFAULT "\" for help on a specific topic.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "General commands:\n" IO_DEFAULT );
    //ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "status help check sync verify autocheck about message runfile backup quit prunebackups resetapihistory" IO_DEFAULT "\n" );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "status help check sync verify autocheck about runfile backup quit prunebackups resetapihistory" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Inventory management commands:\n" IO_DEFAULT );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "sort blmaster add sub loadprices loadnotes loadmycost loadall merge invblxml invmycost setallremarksfromblid" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Item management commands:\n" IO_DEFAULT );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "find item listempty setquantity setprice setcomments setremarks setblid delete owlresolve consolidate regradeused" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Evaluation commands:\n" IO_DEFAULT );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "evalset evalgear evalpartout evalinv checkprices" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Order commands:\n" IO_DEFAULT );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "findorder findordertime saveorderlist" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Catalog commands:\n" IO_DEFAULT );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "owlqueryblid owlsubmitblid owlupdateblid owlforceblid owlsubmitdims owlsubmitweight" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Help topics:\n" IO_DEFAULT );
    ioPrintf( &context->output, IO_MODEBIT_NODATE, BSMSG_INFO IO_CYAN "paths sysinfo conf\n" );
    return;
  }

  if( ccStrLowCmpWord( argv[1], "status" ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "Print general information about BrickSync's current status.\n" );
  else if( ccStrLowCmpWord( argv[1], "help" ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "Print the list of commands.\n" );
  else if( ccStrLowCmpWord( argv[1], "check" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "check" IO_MAGENTA " [bricklink|brickowl]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command flags both BrickLink and BrickOwl for an order check.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Optionally, an argument \"" IO_GREEN "bricklink" IO_DEFAULT "\" or \"" IO_GREEN "brickowl" IO_DEFAULT "\" can be specified to only flag that specific service.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "If any new order or order change is found, the changes will be applied to the tracked inventory\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "and to the other service.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "sync" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "sync" IO_MAGENTA " [bricklink|brickowl]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command flags both BrickLink and BrickOwl for a deep synchronization.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Optionally, an argument \"" IO_GREEN "bricklink" IO_DEFAULT "\" or \"" IO_GREEN "brickowl" IO_DEFAULT "\" can be specified to only flag that specific service.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "During deep synchronization, the service's inventory is compared against BrickSync's local inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "All changes required to make the service's inventory match the local one are then applied.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command uses as few API calls as possible to make the inventories match.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "verify" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "verify" IO_MAGENTA " [bricklink|brickowl]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command verifies if BrickLink and BrickOwl match the local inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "It lists all the operations that would be executed by a " IO_CYAN "sync" IO_DEFAULT " command, but does not\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "apply the operations required to synchronize the remote inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Only a summary is printed in the terminal, consult the log file for detailed information.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Optionally, an argument \"" IO_GREEN "bricklink" IO_DEFAULT "\" or \"" IO_GREEN "brickowl" IO_DEFAULT "\" can be specified to only verify that specific service.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "autocheck" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "autocheck on|off" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command changes the value of the " IO_GREEN "autocheck" IO_DEFAULT " mode.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "When enabled, services are checked regularly for new orders, at intervals specified in the configuration file.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "about" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "about" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command prints some general information about BrickSync.\n" );
  }
 /*
  else if( ccStrLowCmpWord( argv[1], "message" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "message " IO_MAGENTA "[update|discard]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Print the latest BrickSync broadcast message if any.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "An argument of \"" IO_GREEN "update" IO_DEFAULT "\" will check for any new broadcast message.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "An argument of \"" IO_GREEN "discard" IO_DEFAULT "\" will discard an already received message.\n" );
  }
  */
  else if( ccStrLowCmpWord( argv[1], "runfile" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "runfile pathtofile.txt" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Run all the commands found in the specified file.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "backup" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "backup NewBackup.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command saves the current inventory as a BSX file at the path specified.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "quit" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "quit" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command quits BrickSync.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Operations will resume safely when BrickSync is launched again.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "prunebackups" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "prunebackups " IO_MAGENTA "[-p]" IO_CYAN " CountOfDays" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command deletes BrickSync's automated backups of your inventory older than the specified count of days.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Use the pretend flag (" IO_MAGENTA "-p" IO_DEFAULT ") to see the disk space taken without deleting anything.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "resetapihistory" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "resetapihistory" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command resets the history of API calls to zero.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink claims to have a limit of 5000 calls per day, although some experiments suggest otherwise. Use with caution.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "sort" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "sort SomeBsxFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command reads and updates a specified BrickStore/BrickStock BSX file.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The fields " IO_WHITE "comments" IO_DEFAULT ", " IO_WHITE "remarks" IO_DEFAULT " and " IO_WHITE "price" IO_DEFAULT " are updated for all items found in the local inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Lots with no match in the local inventory are left unchanged.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "This command is meant to assist the physical sorting of parts in your inventory, usually before an " IO_CYAN "add" IO_DEFAULT " or " IO_CYAN "merge" IO_DEFAULT " command is used to merge the inventory.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "blmaster" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "blmaster on|off" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command enters or exit the " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT ", depending on the " IO_WHITE "on" IO_DEFAULT " or " IO_WHITE "off" IO_DEFAULT " argument.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "When you enter this mode, all synchronization and order checks are suspended, and you can edit your inventory directly on BrickLink as much as you like.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "When you exit this mode, the tracked inventory and BrickOwl both are synchronized with the changes applied to the BrickLink inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "New BrickLink or BrickOwl orders received while the " IO_MAGENTA "BrickLink Master Mode" IO_DEFAULT " was active are properly integrated.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "add" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "add BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command adds a BSX file to the tracked inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Existing lots are merged, quantities are incremented.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Comments, remarks, prices and other settings are left unchanged.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "sub" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "sub BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command subtracts a BSX file from the tracked inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Quantities are decremented for all existing lots.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Comments, remarks, prices and other settings are left unchanged.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "loadprices" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "loadprices BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command updates the " IO_WHITE "prices" IO_DEFAULT " of the tracked inventory for all matching lots from the specified file.\n" );
    printload:
    ioPrintf( &context->output, 0, BSMSG_INFO "Lots are matched by " IO_WHITE "LotID" IO_DEFAULT " if available, otherwise by " IO_WHITE "BLID:Color:Condition" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Lots from the BSX file that aren't found in the tracked inventory are ignored.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "loadnotes" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "loadnotes BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command updates the " IO_WHITE "comments" IO_DEFAULT " and " IO_WHITE "remarks" IO_DEFAULT " of the tracked inventory for all matching lots from the specified file.\n" );
    goto printload;
  }
  else if( ccStrLowCmpWord( argv[1], "loadmycost" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "loadmycost BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command updates the " IO_WHITE "mycost" IO_DEFAULT " values of the tracked inventory for all matching lots from the specified file.\n" );
    goto printload;
  }
  else if( ccStrLowCmpWord( argv[1], "loadall" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "loadall BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command updates the " IO_WHITE "comments" IO_DEFAULT ", " IO_WHITE "remarks" IO_DEFAULT ", " IO_WHITE "price" IO_DEFAULT ", " IO_WHITE "mycost" IO_DEFAULT " and " IO_WHITE "bulk" IO_DEFAULT " of the tracked inventory for all matching lots from the specified file.\n" );
    goto printload;
  }
  else if( ccStrLowCmpWord( argv[1], "merge" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "merge BrickStoreFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command merges a BSX file into the tracked inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "New lots are created. Existing lots are merged, quantities are incremented, all fields are updated.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "This is the functional equivalent of the command " IO_CYAN "add" IO_DEFAULT " followed by " IO_CYAN "loadall" IO_DEFAULT ".\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "invblxml" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "invblxml SomeBsxFile.bsx" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command reads a specified BrickStore/BrickStock BSX file and outputs BrickLink XML files for it.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The XML will be saved as " IO_WHITE "%s" IO_DEFAULT " in the current working directory.\n", BS_BLXMLUPLOAD_FILE );
    ioPrintf( &context->output, 0, BSMSG_INFO "The output may be broken in multiple XML files if a single file would exceed BrickLink's maximum size for XML upload.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "invmycost" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "invmycost SomeBsxFile.bsx TotalMyCost" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command reads and updates a specified BrickStore/BrickStock BSX file.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The field " IO_WHITE "MyCost" IO_DEFAULT " of each lot is updated to sum up to " IO_WHITE "TotalMyCost" IO_DEFAULT ", proportionally to the " IO_WHITE "Cost" IO_DEFAULT " of each lot.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "find" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "find term0 " IO_MAGENTA "[term1] [term2] ..." IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command searches the tracked inventory for lots that match all specified terms. All item fields are searched for matches.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "A single search term can include spaces if specified within quotes. The search is case sensitive.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Example: " IO_GREEN "find 3001 \"uish Gray\"" IO_DEFAULT "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Example: " IO_GREEN "find 3794 \"ish Brow\"" IO_DEFAULT "\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "item" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "item ItemIdentifier" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command prints all available information for an item. There are multiple ways to specify the ItemIdentifier argument.\n" );
    bsPrintHelpItemCommand( context, "item", "" );
  }
  else if( ccStrLowCmpWord( argv[1], "listempty" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "listempty" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command lists all lots with a quantity of zero.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "You probably don't have empty lots unless the configuration variable " IO_WHITE "retainemptylots" IO_DEFAULT " was enabled.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "setquantity" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "setquantity ItemIdentifier (+/-)Number" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command adjusts the " IO_WHITE "quantity" IO_DEFAULT " for the specified item. The quantity can be absolute, or relative by specifying a '+' or '-' sign in front of the quantity.\n" );
    bsPrintHelpItemCommand( context, "setquantity", "+10" );
  }
  else if( ccStrLowCmpWord( argv[1], "setprice" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "setprice ItemIdentifier NewPrice" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command sets the " IO_WHITE "price" IO_DEFAULT " for the specified item.\n" );
    bsPrintHelpItemCommand( context, "setprice", "0.283" );
  }
  else if( ccStrLowCmpWord( argv[1], "setcomments" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "setcomments ItemIdentifier \"My New Comments\"" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command sets the " IO_WHITE "comments" IO_DEFAULT " for the specified item. Note that you'll want to use quotes to specify comments including spaces.\n" );
    bsPrintHelpItemCommand( context, "setcomments", "\"New Comments\"" );
  }
  else if( ccStrLowCmpWord( argv[1], "setremarks" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "setremarks ItemIdentifier \"My New Remarks\"" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command sets the " IO_WHITE "remarks" IO_DEFAULT " for the specified item. Note that you'll want to use quotes to specify remarks including spaces.\n" );
    bsPrintHelpItemCommand( context, "setremarks", "\"New Remarks\"" );
  }
  else if( ccStrLowCmpWord( argv[1], "setblid" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "setblid ItemIdentifier NewBLID" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command changes the " IO_WHITE "BLID" IO_DEFAULT " of the specified item.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that the command will actually delete and recreate new lots on both BrickLink and BrickOwl.\n" );
    bsPrintHelpItemCommand( context, "setblid", "NewBLID" );
  }
  else if( ccStrLowCmpWord( argv[1], "delete" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "delete ItemIdentifier" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command deletes the item entirely, even if the configuration variable " IO_WHITE "retainemptylots" IO_DEFAULT " was enabled.\n" );
    bsPrintHelpItemCommand( context, "delete", 0 );
  }
  else if( ccStrLowCmpWord( argv[1], "owlresolve" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlresolve" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command tries to resolve the " IO_WHITE "BOID" IO_DEFAULT " of all items for which we presently don't have a " IO_WHITE "BOID" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "It also prints all items currently unknown to BrickOwl, so that you can go add them to the database. :)\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "If new items were resolved, typing \"" IO_GREEN "sync brickowl" IO_DEFAULT "\" will now upload the items to BrickOwl.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "consolidate" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "consolidate " IO_MAGENTA "[-f]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command merges all lots with identical " IO_WHITE "BLID" IO_DEFAULT ", " IO_WHITE "color" IO_DEFAULT ", " IO_WHITE "condition" IO_DEFAULT ", " IO_WHITE "comments" IO_DEFAULT " and " IO_WHITE "remarks" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Lots that only differ through their " IO_WHITE "comments" IO_DEFAULT " or " IO_WHITE "remarks" IO_DEFAULT " are printed out.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_CYAN "-f" IO_DEFAULT " flag forces consolidation even if " IO_WHITE "comments" IO_DEFAULT " or " IO_WHITE "remarks" IO_DEFAULT " differ.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "regradeused" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "regradeused" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command updates the " IO_GREEN "grade" IO_DEFAULT " of all items in " IO_YELLOW "used" IO_DEFAULT " condition.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The " IO_WHITE "comments" IO_DEFAULT " of each lot are parsed to determine the appropriate quality " IO_GREEN "grades" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The parser recognizes many keywords and understands basic grammar rules from modifier keywords (such as 'not' or 'many') to form compound statements.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Keep your comments simple and the parser should do a reasonable job at assigning proper " IO_GREEN "grades" IO_DEFAULT " for all " IO_YELLOW "used" IO_DEFAULT " items.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "setallremarksfromblid" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "setallremarksfromblid" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command sets the " IO_WHITE "remarks" IO_DEFAULT " of " IO_RED "all" IO_DEFAULT " items to match their BLID.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "This command may perform several thousand API calls. If approaching the BrickLink daily limit, remaining items will be finished later.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "evalset" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "evalset SetNumber " IO_MAGENTA "[EvalSetInv.bsx]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command evaluates a set for partout, comparing how it would combine and improve the current inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The optional third argument specifies an output BSX file where to write the set's inventory with per-part statistics.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "When alternates for a part exist, only the cheapest one is added to the BSX file.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Price guide information is fetched without consuming API calls.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "evalgear" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "evalgear GearNumber " IO_MAGENTA "[EvalGearInv.bsx]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command evaluates a gear for partout, comparing how it would combine and improve the current inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The optional third argument specifies an output BSX file where to write the gear's inventory with per-part statistics.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "When alternates for a part exist, only the cheapest one is added to the BSX file.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Price guide information is fetched without consuming API calls.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "evalpartout" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "evalpartout SetNumber " IO_MAGENTA "[EvalSetInv.bsx]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command evaluates a set for partout, comparing how it would combine and improve the current inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The optional third argument specifies an output BSX file where to write the set's inventory with per-part statistics.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "All the alternates are added to the BSX file, although printed global statistics only consider the cheapest alternate.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Price guide information is fetched without consuming API calls.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "evalinv" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "evalinv InventoryToEvaluate.bsx " IO_MAGENTA "[EvalInv.bsx]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command evaluates a BSX inventory, comparing how it would combine and improve the current inventory.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The optional third argument specifies an output BSX file where to write the inventory with per-part statistics.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Price guide information is fetched without consuming API calls.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "checkprices" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "checkprices " IO_MAGENTA "[low] [high]" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command checks the prices of the current inventory compared to price guide information.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "It lists any price that falls outside of the relative range defined by the " IO_GREEN "low" IO_DEFAULT " to " IO_GREEN "high" IO_DEFAULT " bounds.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "If ommited, the " IO_GREEN "low" IO_DEFAULT " and " IO_GREEN "high" IO_DEFAULT " parameters are defined as " IO_WHITE "0.5" IO_DEFAULT " and " IO_WHITE "1.5" IO_DEFAULT ".\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "findorder" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "findorder term0 " IO_MAGENTA "[term1] [term2] ..." IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command searches past orders for an item that matches all the terms specified.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Groups of words can be searched by enclosing them in quotation marks.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Use the command \"" IO_GREEN "findordertime" IO_DEFAULT "\" to change how far back to search, in days.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "findordertime" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "findordertime CountOfDays" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command sets the count of days that subsequent invocations of \"" IO_GREEN "findordertime" IO_DEFAULT "\" will search.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The search time considers the last time the order's inventory was updated on disk.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "saveorderlist" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "saveorderlist CountOfDays" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command fetches the list of orders from both BrickLink and BrickOwl received in the time range specified, in days.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The list is saved to disk as the files " IO_WHITE "%s" IO_DEFAULT " and " IO_WHITE "%s" IO_DEFAULT ", as text and tab-delimited files respectively.\n", "orderlist.txt", "orderlist.tab.txt" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The list and summary is also printed in the terminal. The command consumes only one BrickLink API call.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "owlqueryblid" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlqueryblid BLID" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Query the BrickOwl database to resolve a " IO_WHITE "BOID" IO_DEFAULT " for the specified " IO_GREEN "BLID.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "owlsubmitblid" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlsubmitblid BOID BLID" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command edits the BrickOwl catalog, submit a new " IO_GREEN "BLID" IO_DEFAULT " for a given " IO_GREEN "BOID" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "owlupdateblid" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlupdateblid oldBLID newBLID" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command edits the BrickOwl catalog. It resolves the " IO_WHITE "BOID" IO_DEFAULT " for the specified " IO_GREEN "oldBLID" IO_DEFAULT ", then submit a " IO_GREEN "newBLID" IO_DEFAULT " to that resolved " IO_WHITE "BOID" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command will fail if " IO_GREEN "oldBLID" IO_DEFAULT " can not be resolved to a " IO_WHITE "BOID" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "owlforceblid" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlforceblid BOID BLID" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command appends a new entry in BrickSync's translation cache. It permanently maps a " IO_GREEN "BLID" IO_DEFAULT " to a " IO_GREEN "BOID" IO_DEFAULT ".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command does not replace updating the BrickOwl catalog database, manually or through \"" IO_GREEN "owlsubmitblid" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Yet, if you are waiting for the approval of BLID submissions, the command allows uploading these items to BrickOwl right away.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "owlsubmitdims" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlsubmitdims ID length width height" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command edits the BrickOwl catalog.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }
  else if( ccStrLowCmpWord( argv[1], "owlsubmitweight" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command syntax : \"" IO_CYAN "owlsubmitweight ID weight" IO_DEFAULT "\".\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "The command edits the BrickOwl catalog.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }

  else if( ccStrLowCmpWord( argv[1], "paths" ) )
  {
    int relpathflag;
    ioPrintf( &context->output, 0, BSMSG_INFO "Working Directory  : \"" IO_GREEN "%s" IO_DEFAULT "\"\n", context->cwd );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink Orders   : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING BS_BRICKLINK_ORDER_DIR IO_DEFAULT "\"\n", context->cwd );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl Orders    : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING BS_BRICKOWL_ORDER_DIR IO_DEFAULT "\"\n", context->cwd );
    ioPrintf( &context->output, 0, BSMSG_INFO "Logs Path          : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING BS_LOG_DIR IO_DEFAULT "\"\n", context->cwd );
    ioPrintf( &context->output, 0, BSMSG_INFO "Backups Path       : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING BS_BACKUP_DIR CC_DIR_SEPARATOR_STRING "%%Y-%%m-%%d" IO_DEFAULT "\"\n", context->cwd );
    ioPrintf( &context->output, 0, BSMSG_INFO "Errors Path        : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING BS_ERROR_DIR "%%Y-%%m-%%d" IO_DEFAULT "\"\n", context->cwd );
    ioPrintf( &context->output, 0, BSMSG_INFO "Translation Path   : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING BS_TRANSLATION_FILE IO_DEFAULT "\"\n", context->cwd );
    relpathflag = 1;
#if CC_UNIX
    relpathflag = ( context->priceguidepath[0] != '/' );
#elif CC_WINDOWS
    if( ( context->priceguidepath[0] ) && ( context->priceguidepath[1] == ':' ) && ( context->priceguidepath[2] == '\\' ) )
      relpathflag = 0;
#endif
    if( relpathflag )
      ioPrintf( &context->output, 0, BSMSG_INFO "Price Guide Path   : \"" IO_GREEN "%s" CC_DIR_SEPARATOR_STRING "%s" IO_DEFAULT "\"\n", context->cwd, context->priceguidepath );
    else
      ioPrintf( &context->output, 0, BSMSG_INFO "Price Guide Path   : \"" IO_GREEN "%s" IO_DEFAULT "\"\n", context->priceguidepath );
    pgcacheformat = "Unknown";
    if( context->priceguideflags & BSX_PRICEGUIDE_FLAGS_BRICKSTORE )
      pgcacheformat = "BrickStore";
    if( context->priceguideflags & BSX_PRICEGUIDE_FLAGS_BRICKSTOCK )
      pgcacheformat = "BrickStock";
    ioPrintf( &context->output, 0, BSMSG_INFO "Price Guide Format : " IO_CYAN "%s" IO_DEFAULT ".\n", pgcacheformat );
  }
  else if( ccStrLowCmpWord( argv[1], "sysinfo" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "System General Information\n" );
    cpuGetInfo( &cpuinfo );
    bsPrintCpuInfo( context, &cpuinfo );
    sysname = ccGetSystemName();
    if( sysname )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "Operating System : %s\n", sysname );
      free( sysname );
    }
  }
  else if( ccStrLowCmpWord( argv[1], "conf" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync Configuration\n" );
#if BS_ENABLE_REGISTRATION
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync registration status : %s.\n", ( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED ? IO_GREEN "Registered" IO_DEFAULT : IO_YELLOW "Unregistered" IO_DEFAULT ) );
#endif
    ioPrintf( &context->output, 0, BSMSG_INFO "Autocheck mode is currently : %s.\n", ( context->contextflags & BS_CONTEXT_FLAGS_AUTOCHECK_MODE ? IO_GREEN "Enabled" IO_DEFAULT : IO_YELLOW "Disabled" IO_DEFAULT ) );
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->bricklink.pollinterval, 4 );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink polling interval : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
    ccGrowthFree( &growth );
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->bricklink.pollinterval, 4 );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl polling interval  : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
    ccGrowthFree( &growth );
    ioPrintf( &context->output, 0, BSMSG_INFO "Retaining empty lots : " IO_GREEN "%s" IO_DEFAULT ".\n", ( context->retainemptylotsflag ? "True" : "False" ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "Reusing empty matching BrickOwl lots : " IO_GREEN "%s" IO_DEFAULT ".\n", ( context->brickowl.reuseemptyflag ? "True" : "False" ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "Checking for BrickSync broadcast messages : " IO_GREEN "%s" IO_DEFAULT ".\n", ( context->checkmessageflag ? IO_GREEN "True" IO_DEFAULT : IO_YELLOW "False" IO_DEFAULT ) );
    pgcacheformat = "Unknown";
    if( context->priceguideflags & BSX_PRICEGUIDE_FLAGS_BRICKSTORE )
      pgcacheformat = "BrickStore";
    if( context->priceguideflags & BSX_PRICEGUIDE_FLAGS_BRICKSTOCK )
      pgcacheformat = "BrickStock";
    ioPrintf( &context->output, 0, BSMSG_INFO "Format of Price Guide storage : " IO_GREEN "%s" IO_DEFAULT ".\n", pgcacheformat );
    ccGrowthInit( &growth, 512 );
    ccGrowthElapsedTimeString( &growth, (int64_t)context->priceguidecachetime, 4 );
    ioPrintf( &context->output, 0, BSMSG_INFO "Caching time for Price Guide data : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
    ccGrowthFree( &growth );
    ioPrintf( &context->output, 0, BSMSG_INFO "Size of BrickLink HTTP pipeline queue : " IO_GREEN "%d requests" IO_DEFAULT ".\n", context->bricklink.pipelinequeuesize );
    ioPrintf( &context->output, 0, BSMSG_INFO "Size of BrickOwl HTTP pipeline queue  : " IO_GREEN "%d requests" IO_DEFAULT ".\n", context->brickowl.pipelinequeuesize );
  }
  else
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Sorry, no help on this topic yet!\n", context->cwd );
  }

  return;
}


static void bsCommandCheck( bsContext *context, int argc, char **argv )
{
  int checkflags;

  checkflags = BS_STATE_FLAGS_BRICKLINK_MUST_CHECK | BS_STATE_FLAGS_BRICKOWL_MUST_CHECK;
  if( argc >= 2 )
  {
    if( ccStrLowCmpWord( argv[1], "bricklink" ) )
      checkflags = BS_STATE_FLAGS_BRICKLINK_MUST_CHECK;
    else if( ccStrLowCmpWord( argv[1], "brickowl" ) )
      checkflags = BS_STATE_FLAGS_BRICKOWL_MUST_CHECK;
  }
  context->stateflags |= checkflags;

  if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MUST_CHECK )
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink service flagged for order check.\n" );
  if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_CHECK )
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl service flagged for order check.\n" );

  return;
}


static void bsCommandSync( bsContext *context, int argc, char **argv )
{
  int syncflags;

  syncflags = BS_STATE_FLAGS_BRICKLINK_MUST_SYNC | BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
  if( argc >= 2 )
  {
    if( ccStrLowCmpWord( argv[1], "bricklink" ) )
      syncflags = BS_STATE_FLAGS_BRICKLINK_MUST_SYNC;
    else if( ccStrLowCmpWord( argv[1], "brickowl" ) )
      syncflags = BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
  }
  context->stateflags |= syncflags;

  if( syncflags & BS_STATE_FLAGS_BRICKLINK_MUST_SYNC )
  {
    if( context->curtime < context->bricklink.synctime )
      context->bricklink.synctime = context->curtime + 3;
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink service flagged for deep sync.\n" );
  }
  if( context->stateflags & BS_STATE_FLAGS_BRICKOWL_MUST_SYNC )
  {
    if( context->curtime < context->brickowl.synctime )
      context->brickowl.synctime = context->curtime + 3;
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl service flagged for deep sync.\n" );
  }

  return;
}


static void bsCommandVerify( bsContext *context, int argc, char **argv )
{
  int verifyflags;
  bsSyncStats stats;

  verifyflags = 0x1 | 0x2;
  if( argc >= 2 )
  {
    if( ccStrLowCmpWord( argv[1], "bricklink" ) )
      verifyflags = 0x1;
    else if( ccStrLowCmpWord( argv[1], "brickowl" ) )
      verifyflags = 0x2;
  }

  if( verifyflags & 0x1 )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Verifying inventory synchronization with the BrickLink service.\n" );
    if( bsSyncBrickLink( context, &stats ) )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "Summary of BrickLink operations that would be required for synchronization:\n" );
      bsSyncPrintSummary( context, &stats, 0 );
      if( !( (context->bricklink.diffinv)->itemcount ) )
        ioPrintf( &context->output, 0, BSMSG_INFO "No update required for BrickLink, we have a " IO_GREEN "perfect inventory match" IO_DEFAULT ".\n" );
      else
        ioPrintf( &context->output, 0, BSMSG_INFO "Review the log file for detailed information. You can type " IO_CYAN "sync bricklink" IO_DEFAULT " to perform the listed operations.\n" );
    }
    bsxEmptyInventory( context->bricklink.diffinv );
  }
  if( verifyflags & 0x2 )
  {
    if( verifyflags & 0x1 )
      ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Verifying inventory synchronization with the BrickOwl service.\n" );
    if( bsSyncBrickOwl( context, &stats ) )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "Summary of BrickOwl operations that would be required for synchronization:\n" );
      bsSyncPrintSummary( context, &stats, 1 );
      if( !( (context->brickowl.diffinv)->itemcount ) )
        ioPrintf( &context->output, 0, BSMSG_INFO "No update required for BrickOwl, we have a " IO_GREEN "perfect inventory match" IO_DEFAULT ".\n" );
      else
        ioPrintf( &context->output, 0, BSMSG_INFO "Review the log file for detailed information. You can type " IO_CYAN "sync brickowl" IO_DEFAULT " to perform the listed operations.\n" );
    }
    bsxEmptyInventory( context->brickowl.diffinv );
  }

  return;
}


static void bsCommandAutocheck( bsContext *context, int argc, char **argv )
{
  int autocheckflag;

  if( argc != 2 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"autocheck 1|0|on|off\"" IO_DEFAULT ".\n" );
    return;
  }

  autocheckflag = context->contextflags & BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
  if( ccStrLowCmpWord( argv[1], "0" ) )
    autocheckflag = 0;
  else if( ccStrLowCmpWord( argv[1], "off" ) )
    autocheckflag = 0;
  else if( ccStrLowCmpWord( argv[1], "no" ) )
    autocheckflag = 0;
  else if( ccStrLowCmpWord( argv[1], "1" ) )
    autocheckflag = BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
  else if( ccStrLowCmpWord( argv[1], "on" ) )
    autocheckflag = BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
  else if( ccStrLowCmpWord( argv[1], "yes" ) )
    autocheckflag = BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
  else
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid autocheck flag value. Flag can be 1 (on) or 0 (off).\n" );
    return;
  }
  context->contextflags &= ~BS_CONTEXT_FLAGS_AUTOCHECK_MODE;
  context->contextflags |= autocheckflag;
  ioPrintf( &context->output, 0, BSMSG_INFO "Autocheck mode is now : %s.\n", ( context->contextflags & BS_CONTEXT_FLAGS_AUTOCHECK_MODE ? IO_GREEN "Enabled" IO_DEFAULT : IO_YELLOW "Disabled" IO_DEFAULT ) );

  return;
}


static void bsCommandItem( bsContext *context, int argc, char **argv )
{
  bsxItem *item;

  if( argc != 2 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"item ItemIdentifier\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;

  ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "Listing known information for item." IO_DEFAULT "\n" );
  bsCmdPrintItem( context, item, 0x0 );

  return;
}


static void bsCommandFind( bsContext *context, int argc, char **argv )
{
  int findindex, resultindex, matchmask;
  bsxItem *item;
  bsxInventory *inv;
  bsCmdFind cmdfind;

  if( ( argc < 2 ) || ( argc >= 2+BS_COMMAND_FIND_MAX ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"find term0 [term1] ...\" up to %d search terms." IO_DEFAULT ".\n", BS_COMMAND_FIND_MAX );
    return;
  }

  bsCmdFindInit( &cmdfind, argc - 1, &argv[1] );
  ioPrintf( &context->output, 0, BSMSG_INFO "Listing all inventory matches for the search terms:" );
  for( findindex = 0 ; findindex < cmdfind.findcount ; findindex++ )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " \"" IO_CYAN "%s" IO_DEFAULT "\"", cmdfind.findstring[findindex] );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" );

  inv = context->inventory;
  resultindex = 0;
  for( ; ; )
  {
    item = bsCmdFindInv( &cmdfind, inv, &matchmask );
    if( !( item ) )
      break;
    resultindex++;
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "Search Result #%d:" IO_DEFAULT "\n", resultindex );
    bsCmdPrintItem( context, item, matchmask );
  }

  if( !( resultindex ) )
    ioPrintf( &context->output, 0, BSMSG_INFO IO_RED "No result for search." IO_DEFAULT "\n" );

  return;
}


static void bsCommandListEmpty( bsContext *context, int argc, char **argv )
{
  int itemindex, resultindex;
  bsxInventory *inv;
  bsxItem *item;

  inv = context->inventory;
  resultindex = 0;
  ioPrintf( &context->output, 0, BSMSG_INFO "Listing all lots with zero quantity:\n" );
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->quantity )
      continue;
    resultindex++;
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "List Result #%d:" IO_DEFAULT "\n", resultindex );
    bsCmdPrintItem( context, item, BS_PRINT_ITEM_BOLD_QUANTITY );
  }

  if( !( resultindex ) )
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "You have no lots with zero quantity." IO_DEFAULT "\n" );

  return;
}


static void bsCommandSetQuantity( bsContext *context, int argc, char **argv )
{
  int mode;
  int32_t quantity;
  char *quantitystring;
  bsxItem *item, *deltaitem;

  if( argc != 3 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"setquantity ItemIdentifier %%d\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;

  quantitystring = argv[2];
  mode = 0;
  if( quantitystring[0] == '+' )
  {
    mode = 1;
    quantitystring++;
  }
  else if( quantitystring[0] == '-' )
  {
    mode = -1;
    quantitystring++;
  }
  if( !( ccStrParseInt32( quantitystring, &quantity ) ) )
    goto syntaxerror;

  if( !( mode ) )
  {
    /* Absolute quantity */
    quantity -= item->quantity;
  }
  else if( mode < 0 )
    quantity = -quantity;
  if( !( quantity ) )
    return;
  if( ( item->quantity + quantity ) < 0 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid quantity. Can not subtract more items than present in inventory.\n" );
    return;
  }

  /* Update core inventory */
  ioPrintf( &context->output, 0, BSMSG_INFO "Adjusting quantity for item, from " IO_CYAN "%d" IO_DEFAULT " to " IO_CYAN "%d" IO_DEFAULT ".\n", item->quantity, item->quantity + quantity );
  bsxSetItemQuantity( context->inventory, item, item->quantity + quantity );

  /* Add item to deltainv, flag both services as MUST_UPDATE */

  /* BrickLink update */
  if( item->lotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
    bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, quantity );
    if( context->retainemptylotsflag )
    {
      deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
      if( !( item->quantity ) )
        deltaitem->flags |= BSX_ITEM_XFLAGS_UPDATE_STOCKROOM;
    }
    else
    {
      if( item->quantity )
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
      else
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
    }
  }

  /* BrickOwl update */
  if( item->bolotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    bsxSetItemQuantity( context->brickowl.diffinv, deltaitem, quantity );
    if( context->retainemptylotsflag )
    {
      deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
      if( !( item->quantity ) )
        deltaitem->flags |= BSX_ITEM_XFLAGS_UPDATE_STOCKROOM;
    }
    else
    {
      if( item->quantity )
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_QUANTITY;
      else
        deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
    }
  }

  if( !( context->retainemptylotsflag ) && !( item->quantity ) )
    bsxRemoveItem( context->inventory, item );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );
  return;

  syntaxerror:
  ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid quantity. Can be an absolute quantity (such as 3 or 0) or a relative quantity (such as -4 or +15).\n" );
  return;
}


static void bsCommandSetPrice( bsContext *context, int argc, char **argv )
{
  float price;
  bsxItem *item, *deltaitem;

  if( argc != 3 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"setprice ItemIdentifier %%f\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;

  if( !( ccStrParseFloat( argv[2], &price ) ) )
    goto syntaxerror;
  if( price < 0.0 )
    goto syntaxerror;

  /* Update core inventory */
  ioPrintf( &context->output, 0, BSMSG_INFO "Adjusting price for item, from " IO_CYAN "%.3f" IO_DEFAULT " to " IO_CYAN "%.3f" IO_DEFAULT ".\n", item->price, price );
  item->price = price;

  /* Add item to deltainv, flag both services as MUST_UPDATE */
  if( item->lotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
    deltaitem->price = price;
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_PRICE;
  }
  if( item->bolotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    deltaitem->price = price;
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_PRICE;
  }

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );
  return;

  syntaxerror:
  ioPrintf( &context->output, 0, BSMSG_ERROR "Invalid price value. Can be any floating point positive value.\n" );
  return;
}


static void bsCommandSetComments( bsContext *context, int argc, char **argv )
{
  int commentslength;
  bsxItem *item, *deltaitem;

  if( argc != 3 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"setcomments ItemIdentifier \"Your Public Comments Here\"\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;

  /* Update core inventory */
  ioPrintf( &context->output, 0, BSMSG_INFO "Updating comments for item, from \"" IO_CYAN "%s" IO_DEFAULT "\" to \"" IO_CYAN "%s" IO_DEFAULT "\".\n", item->comments, argv[2] );
  commentslength = strlen( argv[2] );
  bsxSetItemComments( item, argv[2], commentslength );

  /* Add item to deltainv, flag both services as MUST_UPDATE */
  if( item->lotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
    bsxSetItemComments( deltaitem, argv[2], commentslength );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS;
  }
  if( item->bolotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    bsxSetItemComments( deltaitem, argv[2], commentslength );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_COMMENTS;
  }

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandSetRemarks( bsContext *context, int argc, char **argv )
{
  int remarkslength;
  bsxItem *item, *deltaitem;

  if( argc != 3 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"setremarks ItemIdentifier \"Your Private Remarks Here\"\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;
  if( ccStrFindChar( argv[2], '~' ) >= 0 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Remarks must not contain the '" IO_MAGENTA "~" IO_WHITE "' filtering character" IO_DEFAULT ".\n" );
    return;
  }

  /* Update core inventory */
  ioPrintf( &context->output, 0, BSMSG_INFO "Updating remarks for item, from \"" IO_CYAN "%s" IO_DEFAULT "\" to \"" IO_CYAN "%s" IO_DEFAULT "\".\n", item->remarks, argv[2] );
  remarkslength = strlen( argv[2] );
  bsxSetItemRemarks( item, argv[2], remarkslength );

  /* Add item to deltainv, flag both services as MUST_UPDATE */
  if( item->lotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
    bsxSetItemRemarks( deltaitem, argv[2], remarkslength );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_REMARKS;
  }
  if( item->bolotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    bsxSetItemRemarks( deltaitem, argv[2], remarkslength );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_REMARKS;
  }

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandSetBLID( bsContext *context, int argc, char **argv )
{
  int remarkslength;
  bsxItem *item, *deltaitem;
  bsxInventory *inv;

  if( argc != 3 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"setblid ItemIdentifier NewBLID\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;

  bsStoreBackup( context, 0 );

  /* Delete items */
  if( item->lotid )
  {
    deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
    bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, -deltaitem->quantity );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
  }
  if( ( item->lotid ) || ( item->bolotid ) )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, -deltaitem->quantity );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
  }

  /* Update core inventory */
  ioPrintf( &context->output, 0, BSMSG_INFO "Changing BLID for item, from \"" IO_CYAN "%s" IO_DEFAULT "\" to \"" IO_CYAN "%s" IO_DEFAULT "\".\n", item->id, argv[2] );
  bsxSetItemId( item, argv[2], strlen( argv[2] ) );
  item->boid = translationBLIDtoBOID( &context->translationtable, item->typeid, item->id );
  item->lotid = -1;
  item->bolotid = -1;

  /* Resolve BOID for item */
  if( item->boid == -1 )
  {
    inv = bsxNewInventory();
    deltaitem = bsxAddCopyItem( inv, item );
    if( !( bsQueryBrickOwlLookupBoids( context, inv, BS_RESOLVE_FLAGS_TRYFALLBACK, 0 ) ) )
      ioPrintf( &context->output, 0, BSMSG_WARNING "Lookup of BOIDs failed.\n" );
    item->boid = deltaitem->boid;
    bsxFreeInventory( inv );
  }

  /* Must have an extid reference for bsapplydiff to set lotIDs back */
  if( item->extid == -1 )
    bsItemSetUniqueExtID( context, context->inventory, item );

  /* Create new items */
  deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
  deltaitem->flags |= BSX_ITEM_XFLAGS_TO_CREATE;
  if( item->boid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_CREATE;
  }

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandDelete( bsContext *context, int argc, char **argv )
{
  bsxItem *item, *deltaitem;

  if( argc != 2 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"delete ItemIdentifier %%d\"" IO_DEFAULT ".\n" );
    return;
  }

  item = bsCmdLocateItem( context, context->inventory, argv[1] );
  if( !( item ) )
    return;

  /* Update core inventory */
  ioPrintf( &context->output, 0, BSMSG_INFO "Deleting item, previous quantity " IO_CYAN "%d" IO_DEFAULT ".\n", item->quantity );

  /* Add item to deltainv, flag both services as MUST_UPDATE */
  if( item->lotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
    bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, -deltaitem->quantity );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
  }
  if( item->bolotid != -1 )
  {
    deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
    bsxSetItemQuantity( context->bricklink.diffinv, deltaitem, -deltaitem->quantity );
    deltaitem->flags |= BSX_ITEM_XFLAGS_TO_DELETE;
  }

  /* Delete item */
  bsxSetItemQuantity( context->inventory, item, 0 );
  bsxRemoveItem( context->inventory, item );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandSort( bsContext *context, int argc, char **argv )
{
  int itemindex;
  bsxInventory *inv;
  bsxItem *item, *stockitem;
  int found_partcount, found_lotcount;
  int notfound_partcount, notfound_lotcount;

  if( argc != 2 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"" IO_CYAN "sort foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, argv[1] ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );

  found_partcount = 0;
  found_lotcount = 0;
  notfound_partcount = 0;
  notfound_lotcount = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    stockitem = bsxFindMatchItem( context->inventory, item );
    if( !( stockitem ) )
    {
      notfound_partcount += item->quantity;
      notfound_lotcount++;
      continue;
    }
    bsxSetItemRemarks( item, stockitem->remarks, -1 );
    bsxSetItemComments( item, stockitem->comments, -1 );
    item->price = stockitem->price;
    found_partcount += item->quantity;
    found_lotcount++;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated comments/remarks/prices for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", found_partcount, found_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped non-matching entries totalling " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", notfound_partcount, notfound_lotcount );
  bsxSortInventory( inv, BSX_SORT_COLORNAME_NAME_CONDITION, 0 );
  if( bsxSaveInventory( argv[1], inv, 0, -32 ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "We have updated all comments/remarks/prices for the BSX file at path \"" IO_GREEN "%s" IO_DEFAULT "\".\n", argv[1] );
  else
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to write the BSX file back at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
  bsxFreeInventory( inv );

  return;
}


static void bsCommandAdd( bsContext *context, int argc, char **argv )
{
  int cmdflags, warningcount;
  char *inputfile;
  bsxInventory *inv;
  bsMergeInvStats stats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "add [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  warningcount = bsLoadVerifyInventory( context, inv, ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0 : BS_LOADINV_VERIFYFLAGS_PRICES | BS_LOADINV_VERIFYFLAGS_REMARKS ) );
  if( warningcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command aborted! To override the warnings, just append the " IO_GREEN "-f" IO_DEFAULT " flag to force the command.\n" );
    return;
  }

  bsStoreBackup( context, 0 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Looking up BOIDs.\n" );
  if( !( bsQueryBrickOwlLookupBoids( context, inv, BS_RESOLVE_FLAGS_TRYFALLBACK, 0 ) ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "Lookup of BOIDs failed.\n" );

  bsMergeInv( context, inv, &stats, BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "The inventory \"" IO_GREEN "%s" IO_DEFAULT "\" has been merged with the tracked inventory.\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have created " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.create_partcount, stats.create_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have deleted " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots.\n", stats.delete_partcount, stats.delete_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have added " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.added_partcount, stats.added_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have removed " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", stats.removed_partcount, stats.removed_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandSub( bsContext *context, int argc, char **argv )
{
  int cmdflags, warningcount;
  char *inputfile;
  bsxInventory *inv;
  bsMergeInvStats stats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "sub [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  warningcount = bsLoadVerifyInventory( context, inv, ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0 : BS_LOADINV_VERIFYFLAGS_PRICES | BS_LOADINV_VERIFYFLAGS_REMARKS ) );
  if( warningcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command aborted! To override the warnings, just append the " IO_GREEN "-f" IO_DEFAULT " flag to force the command.\n" );
    return;
  }

  bsxInvertQuantities( inv );
  bsStoreBackup( context, 0 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Looking up BOIDs.\n" );
  if( !( bsQueryBrickOwlLookupBoids( context, inv, BS_RESOLVE_FLAGS_TRYFALLBACK, 0 ) ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "Lookup of BOIDs failed.\n" );

  bsMergeInv( context, inv, &stats, BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "The inventory \"" IO_GREEN "%s" IO_DEFAULT "\" has been subtracted from the tracked inventory.\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have created " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.create_partcount, stats.create_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have deleted " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots.\n", stats.delete_partcount, stats.delete_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have added " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.added_partcount, stats.added_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have removed " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", stats.removed_partcount, stats.removed_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandLoadPrices( bsContext *context, int argc, char **argv )
{
  int cmdflags, warningcount;
  char *inputfile;
  bsxInventory *inv;
  bsMergeUpdateStats stats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "loadprices [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  warningcount = bsLoadVerifyInventory( context, inv, ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0 : BS_LOADINV_VERIFYFLAGS_PRICES ) );
  if( warningcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command aborted! To override the warnings, just append the " IO_GREEN "-f" IO_DEFAULT " flag to force the command.\n" );
    return;
  }

  bsStoreBackup( context, 0 );
  bsMergeLoad( context, inv, &stats, BS_MERGE_FLAGS_PRICE | BS_MERGE_FLAGS_TIERPRICES | BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "Prices have been updated to match the ones from \"" IO_GREEN "%s" IO_DEFAULT "\".\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated prices for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.updated_partcount, stats.updated_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots with unchanged prices.\n", stats.match_partcount, stats.match_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots absent from tracked inventory.\n", stats.missing_partcount, stats.missing_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandLoadNotes( bsContext *context, int argc, char **argv )
{
  int cmdflags, warningcount;
  char *inputfile;
  bsxInventory *inv;
  bsMergeUpdateStats stats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "loadnotes [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  warningcount = bsLoadVerifyInventory( context, inv, ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0 : BS_LOADINV_VERIFYFLAGS_REMARKS ) );
  if( warningcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command aborted! To override the warnings, just append the " IO_GREEN "-f" IO_DEFAULT " flag to force the command.\n" );
    return;
  }

  bsStoreBackup( context, 0 );

  bsMergeLoad( context, inv, &stats, BS_MERGE_FLAGS_COMMENTS | BS_MERGE_FLAGS_REMARKS | BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "Comments and remarks have been updated to match the ones from \"" IO_GREEN "%s" IO_DEFAULT "\".\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated notes for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.updated_partcount, stats.updated_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots with unchanged notes.\n", stats.match_partcount, stats.match_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots absent from tracked inventory.\n", stats.missing_partcount, stats.missing_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandLoadMyCost( bsContext *context, int argc, char **argv )
{
  int cmdflags;
  char *inputfile;
  bsxInventory *inv;
  bsMergeUpdateStats stats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "loadmycost [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );

  bsStoreBackup( context, 0 );

  bsMergeLoad( context, inv, &stats, BS_MERGE_FLAGS_MYCOST | BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "MyCost values have been updated to match the ones from \"" IO_GREEN "%s" IO_DEFAULT "\".\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated notes for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.updated_partcount, stats.updated_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots with unchanged notes.\n", stats.match_partcount, stats.match_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots absent from tracked inventory.\n", stats.missing_partcount, stats.missing_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandLoadAll( bsContext *context, int argc, char **argv )
{
  int cmdflags, warningcount;
  char *inputfile;
  bsxInventory *inv;
  bsMergeUpdateStats stats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "loadall [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  warningcount = bsLoadVerifyInventory( context, inv, ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0 : BS_LOADINV_VERIFYFLAGS_PRICES | BS_LOADINV_VERIFYFLAGS_REMARKS ) );
  if( warningcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command aborted! To override the warnings, just append the " IO_GREEN "-f" IO_DEFAULT " flag to force the command.\n" );
    return;
  }

  bsStoreBackup( context, 0 );

  bsMergeLoad( context, inv, &stats, BS_MERGE_FLAGS_PRICE | BS_MERGE_FLAGS_COMMENTS | BS_MERGE_FLAGS_REMARKS | BS_MERGE_FLAGS_BULK | BS_MERGE_FLAGS_MYCOST | BS_MERGE_FLAGS_TIERPRICES | BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "Prices, comments, remarks and bulk quantities have been updated to match the ones from \"" IO_GREEN "%s" IO_DEFAULT "\".\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated fields for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.updated_partcount, stats.updated_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots with unchanged fields.\n", stats.match_partcount, stats.match_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots absent from tracked inventory.\n", stats.missing_partcount, stats.missing_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandMerge( bsContext *context, int argc, char **argv )
{
  int cmdflags, warningcount;
  char *inputfile;
  bsxInventory *inv;
  bsMergeInvStats stats;
  bsMergeUpdateStats updatestats;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &inputfile, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "merge [-f] foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, inputfile ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", inputfile );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  bsInventoryFilterOutItems( context, inv );
  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  warningcount = bsLoadVerifyInventory( context, inv, ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0 : BS_LOADINV_VERIFYFLAGS_PRICES | BS_LOADINV_VERIFYFLAGS_REMARKS ) );
  if( warningcount )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Command aborted! To override the warnings, just append the " IO_GREEN "-f" IO_DEFAULT " flag to force the command.\n" );
    return;
  }

  bsStoreBackup( context, 0 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Looking up BOIDs.\n" );
  if( !( bsQueryBrickOwlLookupBoids( context, inv, BS_RESOLVE_FLAGS_TRYFALLBACK, 0 ) ) )
    ioPrintf( &context->output, 0, BSMSG_WARNING "Lookup of BOIDs failed.\n" );

  bsMergeInv( context, inv, &stats, BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "The inventory \"" IO_GREEN "%s" IO_DEFAULT "\" has been merged with the tracked inventory.\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have created " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.create_partcount, stats.create_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have deleted " IO_RED "%d" IO_DEFAULT " items in " IO_RED "%d" IO_DEFAULT " lots.\n", stats.delete_partcount, stats.delete_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have added " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", stats.added_partcount, stats.added_lotcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have removed " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", stats.removed_partcount, stats.removed_lotcount );
  bsMergeLoad( context, inv, &updatestats, BS_MERGE_FLAGS_PRICE | BS_MERGE_FLAGS_COMMENTS | BS_MERGE_FLAGS_REMARKS | BS_MERGE_FLAGS_BULK | BS_MERGE_FLAGS_MYCOST | BS_MERGE_FLAGS_UPDATE_BRICKLINK | BS_MERGE_FLAGS_UPDATE_BRICKOWL );
  ioPrintf( &context->output, 0, BSMSG_INFO "Prices, comments, remarks and bulk quantities have been updated to match the ones from \"" IO_GREEN "%s" IO_DEFAULT "\".\n", inputfile );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated fields for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", updatestats.updated_partcount, updatestats.updated_lotcount );
  bsxFreeInventory( inv );

  /* Save modified inventory, flag services for MUST_UPDATE */
  bsCmdInventoryModified( context );

  return;
}


static void bsCommandOwlResolve( bsContext *context, int argc, char **argv )
{
  int lookupcounts[4];
  if( !( bsQueryBrickOwlLookupBoids( context, context->inventory, BS_RESOLVE_FLAGS_PRINTOUT | BS_RESOLVE_FLAGS_TRYFALLBACK, lookupcounts ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Lookup of BOIDs failed.\n" );
    return;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "Resolved BOIDs for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", lookupcounts[0], lookupcounts[1] );
  ioPrintf( &context->output, 0, BSMSG_INFO "Failed to resolve BOIDs for " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", lookupcounts[2], lookupcounts[3] );
  if( lookupcounts[0] )
    ioPrintf( &context->output, 0, BSMSG_INFO "Feel free to type " IO_CYAN "sync brickowl" IO_DEFAULT " to upload all newly resolved items to BrickOwl.\n" );
  if( lookupcounts[2] )
    ioPrintf( &context->output, 0, BSMSG_INFO "Please consider taking the time to fill in the missing BLIDs in the BrickOwl catalog!\n" );
   if( !( lookupcounts[0] | lookupcounts[2] ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "Excellent, every item from your inventory is known to BrickOwl!\n" );
  return;
}


static void bsCommandOwlQueryBLID( bsContext *context, int argc, char **argv )
{
  int cmdflags, resolveflags, itemindex, updatecount[2];
  char *blid;
  int64_t boid;
  bsxInventory *inv;
  bsxItem *item;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &blid, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "owlqueryblid BLID" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  item = bsxNewItem( inv );
  bsxSetItemId( item, blid, -1 );
  item->typeid = bsFindItemTypeFromInvBLID( context->inventory, blid );
  resolveflags = 0;
  if( item->typeid )
    resolveflags |= BS_RESOLVE_FLAGS_TRYFALLBACK;
  else
    resolveflags |= BS_RESOLVE_FLAGS_TRYALL;
  if( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE )
    resolveflags |= BS_RESOLVE_FLAGS_FORCEQUERY;

  if( !( bsQueryBrickOwlLookupBoids( context, inv, resolveflags, 0 ) ) )
  {
    bsxFreeInventory( inv );
    return;
  }
  boid = item->boid;
  bsxFreeInventory( inv );

  if( boid != -1 )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "The BLID \"" IO_CYAN "%s" IO_DEFAULT "\" resolves to the BOID " IO_CYAN CC_LLD IO_WHITE ".\n", blid, (long long)boid );
    if( !( resolveflags & BS_RESOLVE_FLAGS_TRYALL ) )
    {
      /* Update BOIDs of the live inventory */
      updatecount[0] = 0;
      updatecount[1] = 0;
      inv = context->inventory;
      for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
      {
        item = &inv->itemlist[itemindex];
        if( item->flags & BSX_ITEM_FLAGS_DELETED )
          continue;
        if( ( ccStrCmpEqual( item->id, blid ) ) && ( item->boid != boid ) )
        {
          item->boid = boid;
          updatecount[0] += item->quantity;
          updatecount[1]++;
        }
      }
      if( updatecount[1] )
      {
        ioPrintf( &context->output, 0, BSMSG_INFO "The BOID of " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " has been " IO_WHITE "updated" IO_DEFAULT ".\n", updatecount[0], updatecount[1] );
        ioPrintf( &context->output, 0, BSMSG_INFO "Feel free to type " IO_CYAN "sync brickowl" IO_DEFAULT " to list items under the newly resolved BOID.\n" );
      }
    }
  }
  else
    ioPrintf( &context->output, 0, BSMSG_INFO "The BLID \"" IO_CYAN "%s" IO_DEFAULT "\" does not resolve to any BOID.\n", blid );

  return;
}


static void bsCommandOwlSubmitBLID( bsContext *context, int argc, char **argv )
{
  int cmdflags, resolveflags;
  char *params[2];
  char *blid;
  int64_t boid;
  bsxInventory *inv;
  bsxItem *item;

  if( !( bsCmdArgStdParse( context, argc, argv, 2, 2, params, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "owlsubmitblid BOID BLID" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  blid = params[1];
  if( !( ccStrParseInt64( params[0], &boid ) ) )
    goto syntaxerror;
  resolveflags = BS_RESOLVE_FLAGS_TRYALL;
  if( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE )
    resolveflags |= BS_RESOLVE_FLAGS_FORCEQUERY;

  inv = bsxNewInventory();
  item = bsxNewItem( inv );
  bsxSetItemId( item, blid, -1 );
  if( !( bsQueryBrickOwlLookupBoids( context, inv, resolveflags, 0 ) ) )
    goto end;

  if( item->boid == boid )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "The BLID \"" IO_CYAN "%s" IO_DEFAULT "\" already resolves to the BOID " IO_CYAN CC_LLD IO_DEFAULT ".\n", blid, (long long)boid );
    ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "No update was submitted to the BrickOwl catalog.\n" );
    goto end;
  }

  if( item->boid != -1 )
  {
    ioPrintf( &context->output, 0, BSMSG_WARNING "The BLID \"" IO_CYAN "%s" IO_WHITE "\" already resolves to a different BOID " IO_CYAN CC_LLD IO_WHITE ".\n", blid, (long long)boid );
    if( !( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ) )
    {
      ioPrintf( &context->output, 0, BSMSG_WARNING "We will abort the catalog submission, please verify the BLID<->BOID mapping. That other BOID might also need an update.\n" );
      ioPrintf( &context->output, 0, BSMSG_WARNING "If you are sure of your submission, try the command again with the -f flag to force.\n" );
      goto end;
    }
  }
  else
    ioPrintf( &context->output, 0, BSMSG_INFO "The BLID \"" IO_CYAN "%s" IO_DEFAULT "\" does not resolve to any BOID.\n", blid );

  item->boid = boid;
  if( bsSubmitBrickOwlEditBlid( context, item ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "We successfully submitted a BrickOwl catalog change : BLID.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }
  else
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to submit a BrickOwl catalog change.\n" );

  end:
  bsxFreeInventory( inv );
  return;
}


static void bsCommandOwlUpdateBLID( bsContext *context, int argc, char **argv )
{
  int cmdflags, resolveflags;
  char *params[2];
  char *oldblid;
  char *newblid;
  int64_t boid;

  bsxInventory *inv;
  bsxItem *item;

  if( !( bsCmdArgStdParse( context, argc, argv, 2, 2, params, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "owlupdateblid oldBLID newBLID" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  oldblid = params[0];
  newblid = params[1];
  if( ccStrCmpEqual( oldblid, newblid ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "The arguments " IO_YELLOW "oldBLID" IO_WHITE " and " IO_YELLOW "newBLID" IO_WHITE " can not be identical.\n" );
    goto syntaxerror;
  }
  resolveflags = BS_RESOLVE_FLAGS_FORCEQUERY | BS_RESOLVE_FLAGS_TRYALL;

  inv = bsxNewInventory();
  item = bsxNewItem( inv );
  bsxSetItemId( item, oldblid, -1 );
#if 0
  /* HACK: Force part update */
  item->typeid = 'P';
  resolveflags &= ~BS_RESOLVE_FLAGS_TRYALL;
#endif
  if( !( bsQueryBrickOwlLookupBoids( context, inv, resolveflags, 0 ) ) )
    goto end;

  if( item->boid == -1 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "The BLID \"" IO_CYAN "%s" IO_WHITE "\" specified does not resolve to any BOID.\n", oldblid );
    goto end;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "Resolved the BLID \"" IO_CYAN "%s" IO_DEFAULT "\" to the BOID " IO_CYAN CC_LLD IO_DEFAULT "\n", oldblid, (long long)item->boid );

  bsxSetItemId( item, newblid, -1 );
  if( bsSubmitBrickOwlEditBlid( context, item ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "We successfully submitted a BrickOwl catalog change : BLID.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }
  else
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to submit a BrickOwl catalog change.\n" );

  end:
  bsxFreeInventory( inv );
  return;
}


static void bsCommandOwlSubmitDims( bsContext *context, int argc, char **argv )
{
  int cmdflags, resolveflags, failcount;
  int32_t dims[3];
  char *params[4];
  char *blid;
  int64_t boid;
  bsxInventory *inv;
  bsxItem *item;

  if( !( bsCmdArgStdParse( context, argc, argv, 4, 4, params, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "owlsubmitdims ID length width height" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  if( !( ccStrParseInt32( params[1], &dims[0] ) ) )
    goto syntaxerror;
  if( !( ccStrParseInt32( params[2], &dims[1] ) ) )
    goto syntaxerror;
  if( !( ccStrParseInt32( params[3], &dims[2] ) ) )
    goto syntaxerror;

  inv = bsxNewInventory();
  item = bsxNewItem( inv );
  blid = params[0];
  if( blid[0] == '*' )
  {
    if( !( ccStrParseInt64( &blid[1], &boid ) ) )
    {
      bsxFreeInventory( inv );
      goto syntaxerror;
    }
    item->boid = boid;
  }
  else
  {
    bsxSetItemId( item, blid, -1 );
    item->typeid = bsFindItemTypeFromInvBLID( context->inventory, blid );
    resolveflags = 0;
    if( item->typeid )
      resolveflags |= BS_RESOLVE_FLAGS_TRYFALLBACK;
    else
      resolveflags |= BS_RESOLVE_FLAGS_TRYALL;
    if( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE )
      resolveflags |= BS_RESOLVE_FLAGS_FORCEQUERY;
    if( !( bsQueryBrickOwlLookupBoids( context, inv, resolveflags, 0 ) ) )
      goto end;
    if( item->boid != -1 )
      ioPrintf( &context->output, 0, BSMSG_INFO "Resolved the BLID \"" IO_CYAN "%s" IO_DEFAULT "\" to the BOID " IO_CYAN CC_LLD IO_DEFAULT "\n", blid, (long long)item->boid );
    else
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "The BLID \"" IO_CYAN "%s" IO_WHITE "\" specified does not resolve to any BOID.\n", blid );
      goto end;
    }
  }

  failcount = 0;
  ioPrintf( &context->output, 0, BSMSG_INFO "Submitting new dimensions for BOID " IO_CYAN CC_LLD IO_DEFAULT ": length " IO_GREEN "%d" IO_DEFAULT " mm, width " IO_GREEN "%d" IO_DEFAULT " mm, height " IO_GREEN "%d" IO_DEFAULT " mm.\n", (long long)item->boid, (int)dims[0], (int)dims[1], (int)dims[2] );
  if( !( bsSubmitBrickOwlEditLength( context, item, dims[0] ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to submit a BrickOwl catalog change.\n" );
    failcount++;
  }
  if( !( bsSubmitBrickOwlEditWidth( context, item, dims[1] ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to submit a BrickOwl catalog change.\n" );
    failcount++;
  }
  if( !( bsSubmitBrickOwlEditHeight( context, item, dims[2] ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to submit a BrickOwl catalog change.\n" );
    failcount++;
  }
  if( !( failcount ) )
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "We successfully submitted BrickOwl catalog changes : Length, Width and Height.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );

  end:
  bsxFreeInventory( inv );
  return;
}


static void bsCommandOwlSubmitWeight( bsContext *context, int argc, char **argv )
{
  int cmdflags, resolveflags;
  float weight;
  char *params[2];
  char *blid;
  int64_t boid;
  bsxInventory *inv;
  bsxItem *item;

  if( !( bsCmdArgStdParse( context, argc, argv, 2, 2, params, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "owlsubmitweight ID weight" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  if( !( ccStrParseFloat( params[1], &weight ) ) )
    goto syntaxerror;

  inv = bsxNewInventory();
  item = bsxNewItem( inv );
  blid = params[0];
  if( blid[0] == '*' )
  {
    if( !( ccStrParseInt64( &blid[1], &boid ) ) )
    {
      bsxFreeInventory( inv );
      goto syntaxerror;
    }
    item->boid = boid;
  }
  else
  {
    bsxSetItemId( item, blid, -1 );
    item->typeid = bsFindItemTypeFromInvBLID( context->inventory, blid );
    resolveflags = 0;
    if( item->typeid )
      resolveflags |= BS_RESOLVE_FLAGS_TRYFALLBACK;
    else
      resolveflags |= BS_RESOLVE_FLAGS_TRYALL;
    if( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE )
      resolveflags |= BS_RESOLVE_FLAGS_FORCEQUERY;
    if( !( bsQueryBrickOwlLookupBoids( context, inv, resolveflags, 0 ) ) )
      goto end;
    if( item->boid != -1 )
      ioPrintf( &context->output, 0, BSMSG_INFO "Resolved the BLID \"" IO_CYAN "%s" IO_DEFAULT "\" to the BOID " IO_CYAN CC_LLD IO_DEFAULT "\n", blid, (long long)item->boid );
    else
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "The BLID \"" IO_CYAN "%s" IO_WHITE "\" specified does not resolve to any BOID.\n", blid );
      goto end;
    }
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Submitting new weight for BOID " IO_CYAN CC_LLD IO_DEFAULT ": weight " IO_GREEN "%f" IO_DEFAULT " g.\n", (long long)item->boid, weight );
  if( bsSubmitBrickOwlEditWeight( context, item, weight ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "We successfully submitted a BrickOwl catalog change : Weight.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Note that it may take up to 48 hours for a catalog administrator to approve the submission.\n" );
  }
  else
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to submit a BrickOwl catalog change.\n" );

  end:
  bsxFreeInventory( inv );
  return;
}


static void bsCommandOwlForceBLID( bsContext *context, int argc, char **argv )
{
  int cmdflags, resolveflags, itemindex, forcecount[2];
  char itemtypeid;
  bsxInventory *inv;
  bsxItem *item;
  char *params[2];
  char *blid;
  int64_t boid;

  if( !( bsCmdArgStdParse( context, argc, argv, 2, 2, params, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "owlforceblid BOID BLID" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  blid = params[1];
  if( !( ccStrParseInt64( params[0], &boid ) ) )
    goto syntaxerror;

  inv = context->inventory;
  itemtypeid = bsFindItemTypeFromInvBLID( inv, blid );

  if( !( itemtypeid ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "No item found in tracked inventory with BLID " IO_RED "%s" IO_WHITE ".\n", blid );
    return;
  }  

  if( translationBLIDtoBOID( &context->translationtable, itemtypeid, blid ) == boid )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "The BLID " IO_RED "%s" IO_WHITE " already resolves to the BOID " IO_RED CC_LLD IO_WHITE " (item type: '" IO_RED "%c" IO_WHITE "').\n", blid, (long long)boid, itemtypeid );
    return;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "We have forced a new entry in BrickSync's local translation cache.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "The BLID \"" IO_CYAN "%s" IO_DEFAULT "\" was mapped to the BOID " IO_CYAN CC_LLD IO_DEFAULT " (item type: '" IO_CYAN "%c" IO_DEFAULT "').\n\n", blid, (long long)boid, itemtypeid );

  /* Add the BLID<->BOID in the translation database */
  translationTableRegisterEntry( &context->translationtable, itemtypeid, blid, boid );

  forcecount[0] = 0;
  forcecount[1] = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( ccStrCmpEqual( item->id, blid ) )
    {
      item->boid = boid;
      forcecount[0] += item->quantity;
      forcecount[1]++;
    }
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Assigned BOIDs for " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", forcecount[0], forcecount[1] );
  ioPrintf( &context->output, 0, BSMSG_INFO "Feel free to type " IO_CYAN "sync brickowl" IO_DEFAULT " to upload the newly resolved items to BrickOwl.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "If you intend to force more mappings between BOIDs to BLIDs, it's preferable to assign all of them prior to running " IO_CYAN "sync brickowl" IO_DEFAULT ".\n" );

  return;
}


static void bsCommandConsolidate( bsContext *context, int argc, char **argv )
{
  int itemindex, cmdflags, forceflags, matchflags;
  bsxInventory *inv;
  bsxItem *item, *refitem;
  int merge_partcount, merge_lotcount;
  int mismatch_partcount, mismatch_lotcount;
  const char *mismatchstring[4] =
  {
    IO_MAGENTA "comments" IO_DEFAULT " and " IO_MAGENTA "remarks" IO_DEFAULT,
    IO_MAGENTA "remarks" IO_DEFAULT,
    IO_MAGENTA "comments" IO_DEFAULT,
    ""
  };

  if( !( bsCmdArgStdParse( context, argc, argv, 0, 0, 0, &cmdflags, BS_COMMAND_ARGSTD_FLAG_FORCE ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "consolidate [-f]" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  forceflags = ( cmdflags & BS_COMMAND_ARGSTD_FLAG_FORCE ? 0x3 : 0 );

  inv = context->inventory;
  bsxSortInventory( inv, BSX_SORT_ID_COLOR_CONDITION_REMARKS_COMMENTS_QUANTITY, 0 );

  merge_partcount = 0;
  merge_lotcount = 0;
  mismatch_partcount = 0;
  mismatch_lotcount = 0;

  item = inv->itemlist;
  refitem = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( !( item->id ) )
      continue;
    if( ( refitem ) && ( ccStrCmpEqual( item->id, refitem->id ) ) && ( item->colorid == refitem->colorid ) && ( item->condition == refitem->condition ) )
    {
      matchflags = forceflags;
      matchflags |= ccStrCmpEqualTest( item->comments, refitem->comments ) << 0;
      matchflags |= ccStrCmpEqualTest( item->remarks, refitem->remarks ) << 1;
      ioPrintf( &context->output, 0, BSMSG_INFO "We have matching lots for \"" IO_CYAN "%s" IO_DEFAULT "\" (" IO_GREEN "%s" IO_DEFAULT ") in \"" IO_CYAN "%s" IO_DEFAULT "\" and \"" IO_CYAN "%s" IO_DEFAULT "\", with quantities of " IO_GREEN "%d" IO_DEFAULT " and " IO_GREEN "%d" IO_DEFAULT ".\n", ( item->name ? item->name : "???" ), ( item->id ? item->id : "???" ), ( item->colorname ? item->colorname : "???" ), ( item->condition == 'N' ? "New" : "Used" ), (int)refitem->quantity, (int)item->quantity );
      if( matchflags == 0x3 )
      {
        ioPrintf( &context->output, 0, BSMSG_INFO "The lots have been merged for a new quantity of " IO_GREEN "%d" IO_DEFAULT ".\n", refitem->quantity + item->quantity );
        merge_partcount += CC_MIN( refitem->quantity, item->quantity );
        merge_lotcount++;
        bsxSetItemQuantity( inv, refitem, refitem->quantity + item->quantity );
        bsxRemoveItem( inv, item );
      }
      else
      {
        ioPrintf( &context->output, 0, BSMSG_INFO "The %s do " IO_RED "not" IO_DEFAULT " match, therefore the lots will " IO_RED "not" IO_DEFAULT " be merged." IO_DEFAULT "\n", mismatchstring[matchflags], (int)refitem->quantity, (int)item->quantity );
        if( !( matchflags & 0x1 ) )
          ioPrintf( &context->output, 0, BSMSG_INFO "Mismatching comments: \"" IO_MAGENTA "%s" IO_DEFAULT "\" and \"" IO_MAGENTA "%s" IO_DEFAULT "\".\n", ( refitem->comments ? refitem->comments : "" ), ( item->comments ? item->comments : "" ) );
        if( !( matchflags & 0x2 ) )
          ioPrintf( &context->output, 0, BSMSG_INFO "Mismatching remarks: \"" IO_MAGENTA "%s" IO_DEFAULT "\" and \"" IO_MAGENTA "%s" IO_DEFAULT "\".\n", ( refitem->remarks ? refitem->remarks : "" ), ( item->remarks ? item->remarks : "" ) );
        mismatch_partcount += CC_MIN( refitem->quantity, item->quantity );
        mismatch_lotcount++;
        refitem = item;
      }
    }
    else
      refitem = item;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "We have consolidated " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", merge_partcount, merge_lotcount );
  if( forceflags != 0x3 )
    ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped identical lots with mismatching comments/remarks for " IO_YELLOW "%d" IO_DEFAULT " items in " IO_YELLOW "%d" IO_DEFAULT " lots.\n", mismatch_partcount, mismatch_lotcount );
  if( mismatch_partcount )
    ioPrintf( &context->output, 0, BSMSG_INFO "If you would like to consolidate lots despites mismatching comments/remarks, just append the " IO_GREEN "-f" IO_DEFAULT " flag to the consolidate command.\n" );
  if( !( merge_partcount | mismatch_partcount ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "Your inventory doesn't have identical lots to consolidate!\n" );
  if( merge_partcount )
  {
    bsxPackInventory( inv );
    bsSaveInventory( context, 0 );
    context->stateflags |= BS_STATE_FLAGS_BRICKLINK_MUST_SYNC | BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink service flagged for deep sync.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl service flagged for deep sync.\n" );
  }

  return;
}


static void bsCommandRegradeUsed( bsContext *context, int argc, char **argv )
{
  int itemindex, evalcondition, editcount, matchcount;
  char usedgrade;
  bsxInventory *inv;
  bsxItem *item;
  char *oldgradename, *newgradename;
  static const char usedgradetable[BS_EVAL_CONDITION_LIKE_COUNT] =
  {
    [BS_EVAL_CONDITION_LIKE_NEW] = 'N',
    [BS_EVAL_CONDITION_GOOD] = 'G',
    [BS_EVAL_CONDITION_ACCEPTABLE] = 'A'
  };

  ioPrintf( &context->output, 0, BSMSG_INFO "Parsing the comments of all lots in " IO_YELLOW "used" IO_DEFAULT " condition to update their quality grade.\n" );

  editcount = 0;
  matchcount = 0;
  inv = context->inventory;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->quantity == 0 )
      continue;

    if( item->typeid == 'S' )
      continue;
    if( item->condition != 'U' )
      continue;

    evalcondition = bsEvalulatePartCondition( item->comments );
    usedgrade = usedgradetable[ evalcondition ];
    if( item->usedgrade != usedgrade )
    {
      oldgradename = IO_MAGENTA "Undefined" IO_DEFAULT;
      if( item->usedgrade == 'N' )
        oldgradename = IO_GREEN "Like New" IO_DEFAULT;
      else if( item->usedgrade == 'G' )
        oldgradename = IO_CYAN "Good" IO_DEFAULT;
      else if( item->usedgrade == 'A' )
        oldgradename = IO_YELLOW "Acceptable" IO_DEFAULT;
      newgradename = IO_MAGENTA "Undefined" IO_DEFAULT;
      if( usedgrade == 'N' )
        newgradename = IO_GREEN "Like New" IO_DEFAULT;
      else if( usedgrade == 'G' )
        newgradename = IO_CYAN "Good" IO_DEFAULT;
      else if( usedgrade == 'A' )
        newgradename = IO_YELLOW "Acceptable" IO_DEFAULT;

      ioPrintf( &context->output, 0, BSMSG_INFO "Item \"" IO_CYAN "%s" IO_DEFAULT "\" (" IO_GREEN "%s" IO_DEFAULT ") in \"" IO_CYAN "%s" IO_DEFAULT "\", comments \"" IO_GREEN "%s" IO_DEFAULT "\", updating grade from %s to %s.\n", ( item->name ? item->name : "???" ), ( item->id ? item->id : "???" ), ( item->colorname ? item->colorname : "???" ), ( item->comments ? item->comments : "" ), oldgradename, newgradename );

      item->usedgrade = usedgrade;
      editcount++;
    }
    else
      matchcount++;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "We have updated the grades of " IO_CYAN "%d" IO_DEFAULT " lots in " IO_YELLOW "used" IO_DEFAULT " condition.\n", editcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "We have skipped " IO_GREEN "%d" IO_DEFAULT " lots with matching " IO_YELLOW "used" IO_DEFAULT " grades.\n", matchcount );
  if( editcount )
  {
    bsSaveInventory( context, 0 );
    context->stateflags |= BS_STATE_FLAGS_BRICKOWL_MUST_SYNC;
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickOwl service flagged for deep sync.\n" );
  }
  else
    ioPrintf( &context->output, 0, BSMSG_INFO "Inventory wasn't modified, no synchronization is necessary.\n" );

  return;
}


static void bsCommandSetAllRemarksFromBLID( bsContext *context, int argc, char **argv )
{
  int itemindex, modifiedcount;
  bsxItem *item, *deltaitem;
  bsxInventory *inv;

  if( argc != 1 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"setallremarksfromblid\"" IO_DEFAULT ".\n" );
    return;
  }

  bsStoreBackup( context, 0 );

  modifiedcount = 0;
  inv = context->inventory;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( ccStrCmpEqualTest( item->id, item->remarks ) )
      continue;
    if( item->id )
    {
      bsxSetItemRemarks( item, item->id, -1 );
      /* Add item to deltainv, flag both services as MUST_UPDATE */
      deltaitem = bsxAddCopyItem( context->bricklink.diffinv, item );
      bsxSetItemRemarks( deltaitem, item->id, -1 );
      deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_REMARKS;
      deltaitem = bsxAddCopyItem( context->brickowl.diffinv, item );
      bsxSetItemRemarks( deltaitem, item->id, -1 );
      deltaitem->flags |= BSX_ITEM_XFLAGS_TO_UPDATE | BSX_ITEM_XFLAGS_UPDATE_REMARKS;
      modifiedcount++;
    }
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Updating remarks for %d items, setting all remarks to match item BLIDs.\n", modifiedcount );

  if( modifiedcount )
    bsCmdInventoryModified( context );

  return;
}


static void bsCommandBlMaster( bsContext *context, int argc, char **argv )
{
  if( argc != 2 )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "blmaster on|off" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  if( ccStrLowCmpWord( argv[1], "on" ) )
  {
    if( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MASTER_MODE )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "BrickLink Master Mode is already enabled.\n" );
      return;
    }
    bsEnterBrickLinkMasterMode( context );
  }
  else if( ccStrLowCmpWord( argv[1], "off" ) )
  {
    if( !( context->stateflags & BS_STATE_FLAGS_BRICKLINK_MASTER_MODE ) )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "BrickLink Master Mode is already disabled.\n" );
      return;
    }
    bsExitBrickLinkMasterMode( context );
  }
  else
    goto syntaxerror;

  return;
}


static void bsCommandAbout( bsContext *context, int argc, char **argv )
{
  ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync %s - Build Date %s %s\n", BS_VERSION_STRING, __DATE__, __TIME__ );
  ioPrintf( &context->output, 0, BSMSG_INFO "Copyright (c) 2014 Alexis Naveros\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Contact: Stragus on BrickOwl and BrickLink\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Email: alexis@rayforce.net\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Paypal Donations: alexis@rayforce.net\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Coupon Donations: Stragus on BrickOwl and BrickLink\n\n" );
  if( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED )
    ioPrintf( &context->output, 0, BSMSG_INFO "Your copy of BrickSync is " IO_GREEN "registered" IO_DEFAULT ". " IO_WHITE "Thank you for supporting BrickSync!" IO_DEFAULT "\n" );
  else
    ioPrintf( &context->output, 0, BSMSG_INFO "Thank you for using BrickSync!\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Feel free to send me a note if you have bug reports or suggestions to improve the software.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "The code is entirely written in C99 with few dependencies (just OpenSSL), totalling over 25000 lines of code.\n" );
#if BS_ENABLE_REGISTRATION
  ioPrintf( &context->output, 0, BSMSG_INFO "Note that if the total of donations and registrations reach a certain amount (some small fraction of what 200 hours\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "at the day job would have paid), BrickSync will become open-source, free for everybody to use and improve forever.\n" );
#endif
  return;
}


void bsCommandMessage( bsContext *context, int argc, char **argv )
{
  if( argc >= 2 )
  {
    if( ccStrLowCmpWord( argv[1], "update" ) )
    {
      context->lastmessagetimestamp = 0;
      context->messagetime = context->curtime;
      return;
    }
    else if( ( ccStrLowCmpWord( argv[1], "discard" ) ) || ( ccStrLowCmpWord( argv[1], "silence" ) ) )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "No notification will be further received for the current BrickSync broadcast message.\n" );
      context->contextflags &= ~BS_CONTEXT_FLAGS_NEW_MESSAGE;
      return;
    }
  }
  bsReadBrickSyncMessage( context );
  ioPrintf( &context->output, 0, BSMSG_INFO "Type \"" IO_CYAN "message discard" IO_DEFAULT "\" to silence this message.\n" );
  return;
}


static int bsRunCommand( bsContext *context, char *input, int runfileflag );

void bsCommandRunFile( bsContext *context, int argc, char **argv )
{
  int offset;
  char *filedata, *input;

  if( argc != 2 )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "runfile pathtofile.txt" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  filedata = ccFileLoad( argv[1], 16*1048576, 0 );
  if( !( filedata ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a file at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    return;
  }

  for( input = filedata ; ; )
  {
    offset = ccStrFindChar( input, '\n' );
    if( offset == -1 )
      break;
    bsRunCommand( context, input, 1 );
    input += offset + 1;
  }
  bsRunCommand( context, input, 1 );
  free( filedata );

  return;
}


void bsCommandRegister( bsContext *context, int argc, char **argv )
{
#if BS_ENABLE_REGISTRATION
  ioPrintf( &context->output, 0, "\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "BrickSync is free and donation supported.\n" );
  if( context->contextflags & BS_CONTEXT_FLAGS_REGISTERED )
    ioPrintf( &context->output, 0, BSMSG_INFO "Your copy of BrickSync is " IO_GREEN "registered" IO_DEFAULT ". " IO_WHITE "Thank you for supporting BrickSync!" IO_DEFAULT "\n" );
  else
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Though, if your inventory holds more than " IO_WHITE "%d" IO_DEFAULT " parts, you are kindly invited to support BrickSync,\n", BS_LIMITS_MAX_PARTCOUNT );
    ioPrintf( &context->output, 0, BSMSG_INFO "or you'll be asked a small mathematical puzzle to " IO_CYAN "check" IO_DEFAULT " for new orders, and " IO_YELLOW "autocheck" IO_DEFAULT " will be disabled.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "Donating to BrickSync will provide you with a registration code to remove these limitations.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "And of course, smaller sellers are also very welcome to donate and support the project. :)\n\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "If you want to register, contact me at " IO_WHITE "alexis@rayforce.net" IO_DEFAULT " with your registration code:\n" );
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "Your Registration Code: " IO_MAGENTA "%s" IO_DEFAULT "\n", context->registrationcode );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "Thank you!\n" );
#else
  ioPrintf( &context->output, 0, BSMSG_INFO "No registration is required with this release! It's donation supported and free for all.\n" );
#endif
  return;
}


static void bsCommandBackup( bsContext *context, int argc, char **argv )
{
  if( argc != 2 )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"" IO_CYAN "backup inventorybackup.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }
  if( !( bsxSaveInventory( argv[1], context->inventory, 0, 0 ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to save a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    return;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "We saved a backup of the tracked inventory to \"" IO_GREEN "%s" IO_DEFAULT "\".\n", argv[1] );
  return;
}


void bsCommandPruneBackups( bsContext *context, int argc, char **argv )
{
  int cmdflags, deletecount, subdirkeepflag, pretendflag;
  char *deletetimestring;
  float deletetimerange;
  ccDir *backupdir, *subdir;
  char *filename, *filepath;
  char *subfilename, *subfilepath;
  size_t filesize, deletesize;
  time_t filetime, deletetimestamp;
  ccGrowth growth;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 1, &deletetimestring, &cmdflags, BS_COMMAND_ARGSTD_FLAG_PRETEND ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "prune [-p] CountOfDaysToPreserve" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  if( !( ccStrParseFloat( deletetimestring, &deletetimerange ) ) )
    goto syntaxerror;

  pretendflag = ( cmdflags & BS_COMMAND_ARGSTD_FLAG_PRETEND ? 1 : 0 );
  deletetimerange *= (float)(24*60*60);
  deletetimestamp = context->curtime - (int64_t)( deletetimerange );

  ccGrowthInit( &growth, 512 );
  ccGrowthElapsedTimeString( &growth, (int64_t)deletetimerange, 4 );
  ioPrintf( &context->output, 0, BSMSG_INFO "%s all BrickSync inventory backups older than : " IO_GREEN "%s" IO_DEFAULT ".\n", ( pretendflag ? "Probing" : "Deleting" ), growth.data );
  ccGrowthFree( &growth );

  deletecount = 0;
  deletesize = 0;

  backupdir = ccOpenDir( BS_BACKUP_DIR );
  if( !( backupdir ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to open the directory \"" IO_RED "%s" IO_WHITE "\" for reading.\n", BS_BACKUP_DIR );
    return;
  }
  for( ; ; )
  {
    filename = ccReadDir( backupdir );
    if( !( filename ) )
      break;
    if( filename[0] == '.' )
      continue;
    filepath = ccStrAllocPrintf( BS_BACKUP_DIR CC_DIR_SEPARATOR_STRING "%s", filename );
    subdir = ccOpenDir( filepath );
    if( subdir )
    {
      subdirkeepflag = 0;
      for( ; ; )
      {
        subfilename = ccReadDir( subdir );
        if( !( subfilename ) )
          break;
        if( subfilename[0] == '.' )
          continue;
        subfilepath = ccStrAllocPrintf( BS_BACKUP_DIR CC_DIR_SEPARATOR_STRING "%s" CC_DIR_SEPARATOR_STRING "%s", filename, subfilename );
        if( ccFileStat( subfilepath, &filesize, &filetime ) )
        {
          if( filetime < deletetimestamp )
          {
            if( !( pretendflag ) )
              remove( subfilepath );
            deletecount++;
            deletesize += filesize;
          }
          else
            subdirkeepflag = 1;
        }
        free( subfilepath );
      }
      ccCloseDir( subdir );
      if( !( pretendflag ) && !( subdirkeepflag ) )
        remove( filepath );
    }
    else if( ccFileStat( filepath, &filesize, &filetime ) )
    {
      if( filetime < deletetimestamp )
      {
        if( !( pretendflag ) )
          remove( filepath );
        deletecount++;
        deletesize += filesize;
      }
    }
    free( filepath );
  }
  ccCloseDir( backupdir );

  if( pretendflag )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "We would have deleted " IO_CYAN "%d" IO_DEFAULT " inventory backups, freeing " IO_CYAN "%d MB" IO_DEFAULT " of disk space.\n", deletecount, (int)( deletesize / 1048576 ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "Run the command without the " IO_CYAN "-p" IO_DEFAULT " flag to perform the operations.\n" );
  }
  else
    ioPrintf( &context->output, 0, BSMSG_INFO "We have deleted " IO_CYAN "%d" IO_DEFAULT " inventory backups, freeing " IO_CYAN "%d MB" IO_DEFAULT " of disk space.\n", deletecount, (int)( deletesize / 1048576 ) );

  return;
}


static void bsCommandResetApiHistory( bsContext *context, int argc, char **argv )
{
  bsApiHistoryReset( context, &context->bricklink.apihistory );
  bsApiHistoryReset( context, &context->brickowl.apihistory );
  if( !( bsSaveState( context, 0 ) ) )
  {
    bsFatalError( context );
    return;
  }
  ioPrintf( &context->output, 0, BSMSG_INFO "The API history of both BrickLink and BrickOwl has been reset to zero.\n" );
  return;
}


static int bsCommandEvalSetInternal( bsContext *context, int argc, char **argv, int fetchinvmode, char fetchitemtypeid, int keepaltmode )
{
  int cmdflags, cachetime;
  int showaltflag, removealtflag;
  char *params[2];
  char *itemid;
  bsxInventory *inv;
  bsPriceGuideState pgstate;
  double stockfloat, newfloat;
  char *stockalign, *newalign;

  if( !( bsCmdArgStdParse( context, argc, argv, 1, 2, params, &cmdflags, 0 ) ) )
    return 0;

  if( fetchinvmode )
  {
    if( ( fetchitemtypeid != 'S' ) || ( ccStrFindChar( params[0], '-' ) >= 0 ) )
      itemid = ccStrAllocPrintf( "%s", params[0] );
    else
      itemid = ccStrAllocPrintf( "%s-1", params[0] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Fetching inventory for \"" IO_CYAN "%s" IO_DEFAULT "\".\n", itemid );
    inv = bsBrickLinkFetchSetInventory( context, fetchitemtypeid, itemid );
    if( !( inv ) )
      ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to fetch inventory for \"" IO_RED "%s" IO_WHITE "\".\n", itemid );
    free( itemid );
  }
  else
  {
    inv = bsxNewInventory();
    if( !( bsxLoadInventory( inv, params[0] ) ) )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", params[0] );
      ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
      bsxFreeInventory( inv );
      inv = 0;
    }
  }
  if( !( inv ) )
    return 0;

  cachetime = context->priceguidecachetime;
  showaltflag = 0;
  removealtflag = 1;
  if( keepaltmode )
  {
    showaltflag = 1;
    removealtflag = 0;
  }

  bsPriceGuideInitState( &pgstate, inv, showaltflag, removealtflag );
  if( !( bsProcessInventoryPriceGuide( context, inv, cachetime, (void *)&pgstate, bsPriceGuideSumCallback ) ) )
  {
    bsxFreeInventory( inv );
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to fetch price guide information for the inventory.\n" );
    return 1;
  }
  bsPriceGuideFinishState( &pgstate );

  ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Eval Set " IO_CYAN "%-15s" IO_MAGENTA " Stock       " IO_GREEN "        New        " IO_CYAN "       Total\n", params[0] );

  stockfloat = 100.0 * ( (double)pgstate.stockcount / (double)pgstate.totalcount );
  stockalign = ( stockfloat < 10.0 ? "  " : ( stockfloat < 100.0 ? " " : "" ) );
  newfloat = 100.0 * ( (double)pgstate.newcount / (double)pgstate.totalcount );
  newalign = ( newfloat < 10.0 ? "  " : ( newfloat < 100.0 ? " " : "" ) );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Part Count      :   " IO_MAGENTA "%6d" IO_WHITE " (" IO_MAGENTA "%.2f%%" IO_WHITE ")%s   " IO_GREEN "%6d" IO_WHITE " (" IO_GREEN "%.2f%%" IO_WHITE ")%s   " IO_CYAN "%6d" IO_WHITE " (" IO_CYAN "%.2f%%" IO_WHITE ")\n", pgstate.stockcount, stockfloat, stockalign, pgstate.newcount, newfloat, newalign, pgstate.totalcount, 100.0 );

  stockfloat = 100.0 * ( (double)pgstate.stocklots / (double)pgstate.totallots );
  stockalign = ( stockfloat < 10.0 ? "  " : ( stockfloat < 100.0 ? " " : "" ) );
  newfloat = 100.0 * ( (double)pgstate.newlots / (double)pgstate.totallots );
  newalign = ( newfloat < 10.0 ? "  " : ( newfloat < 100.0 ? " " : "" ) );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Lot Count       :   " IO_MAGENTA "%6d" IO_WHITE " (" IO_MAGENTA "%.2f%%" IO_WHITE ")%s   " IO_GREEN "%6d" IO_WHITE " (" IO_GREEN "%.2f%%" IO_WHITE ")%s   " IO_CYAN "%6d" IO_WHITE " (" IO_CYAN "%.2f%%" IO_WHITE ")\n", pgstate.stocklots, stockfloat, stockalign, pgstate.newlots, newfloat, newalign, pgstate.totallots, 100.0 );

  stockfloat = 100.0 * ( pgstate.stockvalue / pgstate.totalvalue );
  stockalign = ( stockfloat < 10.0 ? "  " : ( stockfloat < 100.0 ? " " : "" ) );
  newfloat = 100.0 * ( pgstate.newvalue / pgstate.totalvalue );
  newalign = ( newfloat < 10.0 ? "  " : ( newfloat < 100.0 ? " " : "" ) );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Sale Price      : " IO_MAGENTA "%8.2f" IO_WHITE " (" IO_MAGENTA "%.2f%%" IO_WHITE ")%s " IO_GREEN "%8.2f" IO_WHITE " (" IO_GREEN "%.2f%%" IO_WHITE ")%s " IO_CYAN "%8.2f" IO_WHITE " (" IO_CYAN "%.2f%%" IO_WHITE ")\n", pgstate.stockvalue, stockfloat, stockalign, pgstate.newvalue, newfloat, newalign, pgstate.totalvalue, 100.0 );

  stockfloat = 100.0 * ( pgstate.stocksale / pgstate.totalsale );
  stockalign = ( stockfloat < 10.0 ? "  " : ( stockfloat < 100.0 ? " " : "" ) );
  newfloat = 100.0 * ( pgstate.newsale / pgstate.totalsale );
  newalign = ( newfloat < 10.0 ? "  " : ( newfloat < 100.0 ? " " : "" ) );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Supply Price    : " IO_MAGENTA "%8.2f" IO_WHITE " (" IO_MAGENTA "%.2f%%" IO_WHITE ")%s " IO_GREEN "%8.2f" IO_WHITE " (" IO_GREEN "%.2f%%" IO_WHITE ")%s " IO_CYAN "%8.2f" IO_WHITE " (" IO_CYAN "%.2f%%" IO_WHITE ")\n", pgstate.stocksale, stockfloat, stockalign, pgstate.newsale, newfloat, newalign, pgstate.totalsale, 100.0 );

  stockfloat = 100.0 * ( pgstate.stockprice / pgstate.totalprice );
  stockalign = ( stockfloat < 10.0 ? "  " : ( stockfloat < 100.0 ? " " : "" ) );
  newfloat = 100.0 * ( pgstate.newprice / pgstate.totalprice );
  newalign = ( newfloat < 10.0 ? "  " : ( newfloat < 100.0 ? " " : "" ) );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Stock Price     : " IO_MAGENTA "%8.2f" IO_WHITE " (" IO_MAGENTA "%.2f%%" IO_WHITE ")%s " IO_GREEN "%8.2f" IO_WHITE " (" IO_GREEN "%.2f%%" IO_WHITE ")%s " IO_CYAN "%8.2f" IO_WHITE " (" IO_CYAN "%.2f%%" IO_WHITE ")\n", pgstate.stockprice, stockfloat, stockalign, pgstate.newprice, newfloat, newalign, pgstate.totalprice, 100.0 );

  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Demand / Supply :        " IO_MAGENTA "%.3f              " IO_GREEN "%.3f              " IO_CYAN "%.3f\n", pgstate.stockpgq, pgstate.newpgq, pgstate.totalpgq );
  ioPrintf( &context->output, 0, BSMSG_INFO IO_WHITE "Sale / Price    :        " IO_MAGENTA "%.3f              " IO_GREEN "%.3f              " IO_CYAN "%.3f\n", pgstate.stockpgp, pgstate.newpgp, pgstate.totalpgp );

  ioPrintf( &context->output, 0, BSMSG_INFO "\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Sale and supply prices are based on quantity averages.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "Demand/supply and sale/price ratios are value-averaged.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "For parts with alternates, calculations are made assuming the lowest value alternate.\n" );
  ioPrintf( &context->output, 0, BSMSG_INFO "\n" );

  if( params[1] )
  {
    if( bsxSaveInventory( params[1], inv, 0, 8 ) )
    {
      ioPrintf( &context->output, 0, BSMSG_INFO "We have saved the set evaluation BSX file at \"" IO_GREEN "%s" IO_DEFAULT "\".\n", params[1] );
      ioPrintf( &context->output, 0, BSMSG_INFO "The BSX file's " IO_WHITE "Price" IO_DEFAULT " fields store the quantity averaged sale value.\n", params[1] );
      ioPrintf( &context->output, 0, BSMSG_INFO "The BSX file's " IO_WHITE "Pr. Orig" IO_DEFAULT " fields store your current inventory's value.\n", params[1] );
      ioPrintf( &context->output, 0, BSMSG_INFO "The BSX file's " IO_WHITE "Comments" IO_DEFAULT " fields store per-part Supply/Demand and Sale/Price ratios.\n", params[1] );
    }
    else
      ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to write the BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", params[1] );
  }

  bsxFreeInventory( inv );

  return 1;
}


static void bsCommandEvalSet( bsContext *context, int argc, char **argv )
{
  if( !( bsCommandEvalSetInternal( context, argc, argv, 1, 'S', 0 ) ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "evalset SetNumber [EvalSetInv.bsx]" IO_WHITE "\"" IO_DEFAULT ".\n" );
  return;
}

static void bsCommandEvalGear( bsContext *context, int argc, char **argv )
{
  if( !( bsCommandEvalSetInternal( context, argc, argv, 1, 'G', 0 ) ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "evalgear SetNumber [EvalSetInv.bsx]" IO_WHITE "\"" IO_DEFAULT ".\n" );
  return;
}

static void bsCommandEvalPartout( bsContext *context, int argc, char **argv )
{
  if( !( bsCommandEvalSetInternal( context, argc, argv, 1, 'S', 1 ) ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "evalpartout SetNumber [EvalSetInv.bsx]" IO_WHITE "\"" IO_DEFAULT ".\n" );
  return;
}

static void bsCommandEvalInv( bsContext *context, int argc, char **argv )
{
  if( !( bsCommandEvalSetInternal( context, argc, argv, 0, 0, 1 ) ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "evalinv InventoryToEvaluate.bsx [EvalInv.bsx]" IO_WHITE "\"" IO_DEFAULT ".\n" );
  return;
}


static void bsCommandCheckPrices( bsContext *context, int argc, char **argv )
{
  int cmdflags, cachetime;
  char *params[3];
  float listrange[2];
  bsxInventory *inv;

  if( !( bsCmdArgStdParse( context, argc, argv, 0, 3, params, &cmdflags, 0 ) ) )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Usage is \"" IO_CYAN "checkprices [low] [high]" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  listrange[0] = 0.5;
  listrange[1] = 1.5;
  if( ( params[0] ) && !( ccStrParseFloat( params[0], &listrange[0] ) ) )
    goto syntaxerror;
  if( ( params[1] ) && !( ccStrParseFloat( params[1], &listrange[1] ) ) )
    goto syntaxerror;

  inv = context->inventory;
  if( params[2] )
  {
    inv = bsxNewInventory();
    if( !( bsxLoadInventory( inv, params[2] ) ) )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", params[2] );
      ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
      bsxFreeInventory( inv );
      return;
    }
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Listing all inventory items with relative pricing outside of the " IO_GREEN "%.2f" IO_DEFAULT " - " IO_GREEN "%.2f" IO_DEFAULT " range.\n", listrange[0], listrange[1] );

  cachetime = context->priceguidecachetime;
  if( !( bsProcessInventoryPriceGuide( context, inv, cachetime, (void *)listrange, bsPriceGuideListRangeCallback ) ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to fetch price guide information for the inventory.\n" );

  if( inv != context->inventory )
    bsxFreeInventory( inv );

  return;
}


static void bsCommandFindOrder( bsContext *context, int argc, char **argv )
{
  int entryindex, findindex, resultindex, resultcount, matchmask;
  time_t filetime;
  struct tm timeinfo;
  bsOrderDir orderdir;
  bsOrderDirEntry *entry;
  bsxItem *item;
  bsxInventory *inv;
  bsCmdFind cmdfind;
  ccGrowth growth;
  char *servicename;
  char timebuf[64];

  if( ( argc < 2 ) || ( argc >= 2+BS_COMMAND_FIND_MAX ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"findorder term0 [term1] ...\" up to %d search terms." IO_DEFAULT ".\n", BS_COMMAND_FIND_MAX );
    return;
  }

  if( !( bsReadOrderDir( context, &orderdir, context->curtime - context->findordertime ) ) )
    return;
  bsSortOrderDir( context, &orderdir );

  bsCmdFindInit( &cmdfind, argc - 1, &argv[1] );
  ioPrintf( &context->output, 0, BSMSG_INFO "Searching orders saved to disk for the search terms:" );
  for( findindex = 0 ; findindex < cmdfind.findcount ; findindex++ )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, " \"" IO_CYAN "%s" IO_DEFAULT "\"", cmdfind.findstring[findindex] );
  ioPrintf( &context->output, IO_MODEBIT_NODATE, "\n" );

  ccGrowthInit( &growth, 512 );
  ccGrowthElapsedTimeString( &growth, (int64_t)context->findordertime, 4 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Searching orders saved to disk in the past: " IO_MAGENTA "%s" IO_DEFAULT "\n", growth.data );
  ccGrowthFree( &growth );

  inv = bsxNewInventory();

  resultcount = 0;
  for( entryindex = 0 ; entryindex < orderdir.entrycount ; entryindex++ )
  {
    entry = &orderdir.entrylist[ entryindex ];

    if( !( bsxLoadInventory( inv, entry->filepath ) ) )
      continue;

    bsCmdFindReset( &cmdfind );
    filetime = entry->filetime;
    resultindex = 0;
    for( ; ; )
    {
      item = bsCmdFindInv( &cmdfind, inv, &matchmask );
      if( !( item ) )
        break;
      if( !( resultindex ) )
      {
        ioPrintf( &context->output, 0, "\n" );
        servicename = "Unknown";
        if( entry->ordertype == BS_ORDER_DIR_ORDER_TYPE_BRICKLINK )
          servicename = "BrickLink";
        else if( entry->ordertype == BS_ORDER_DIR_ORDER_TYPE_BRICKOWL )
          servicename = "BrickOwl";
        ccGrowthInit( &growth, 512 );
        ccGrowthElapsedTimeString( &growth, (int64_t)context->curtime - (int64_t)filetime, 4 );
        timeinfo = *( localtime( &filetime ) );
        strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
        ioPrintf( &context->output, 0, BSMSG_INFO IO_CYAN "%s" IO_DEFAULT " Order #" IO_GREEN "%d" IO_DEFAULT ", saved to disk on " IO_CYAN "%s" IO_DEFAULT " (%s ago)\n", servicename, entry->orderid, timebuf, growth.data );
        ccGrowthFree( &growth );
      }
      resultindex++;
      ioPrintf( &context->output, 0, BSMSG_INFO IO_GREEN "Search Result #%d:" IO_DEFAULT "\n", resultindex );
      bsCmdPrintItem( context, item, matchmask );
    }

    resultcount += resultindex;
    bsxEmptyInventory( inv );

    if( resultcount >= 512 )
    {
      ioPrintf( &context->output, 0, BSMSG_ERROR "Way too many search results! Try to add more terms to narrow down the search.\n" );
      break;
    }
  }

  bsxFreeInventory( inv );

  if( !( resultcount ) )
    ioPrintf( &context->output, 0, BSMSG_INFO IO_RED "No result for search." IO_DEFAULT "\n" );

  bsFreeOrderDir( context, &orderdir );
  return;
}


static void bsCommandFindOrderTime( bsContext *context, int argc, char **argv )
{
  float findordertime;
  ccGrowth growth;

  if( argc != 2 )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"findordertime CountOfDays\"" IO_DEFAULT ".\n" );
    return;
  }

  if( !( ccStrParseFloat( argv[1], &findordertime ) ) )
    goto syntaxerror;

  context->findordertime = (int64_t)( findordertime * (float)(24*60*60) );

  ccGrowthInit( &growth, 512 );
  ccGrowthElapsedTimeString( &growth, (int64_t)context->findordertime, 4 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Find order time is now set to : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
  ccGrowthFree( &growth );

  return;
}


static inline int bsSortCmpOrderListDate( bsOrder *order0, bsOrder *order1 )
{
  return ( order1->date < order0->date );
}

#define HSORT_MAIN bsSortOrderListDate
#define HSORT_CMP bsSortCmpOrderListDate
#define HSORT_TYPE bsOrder
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE


static void bsCommandSaveOrderList( bsContext *context, int argc, char **argv )
{
  int orderindex, blordercount, boordercount;
  float savetimerange;
  int64_t basetimestamp;
  ccGrowth growth;
  bsOrderList blorderlist;
  bsOrderList boorderlist;
  bsOrderList mergeorderlist;
  bsOrder *order;
  void *tmp;
  time_t ordertime;
  struct tm timeinfo;
  char timebuf[64];
  FILE *textfile, *tabfile;
  float salevalue[BS_ORDER_SERVICE_COUNT], grandtotal[BS_ORDER_SERVICE_COUNT];
  char *servicecolor, *salecolor;

  if( argc != 2 )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"saveorderlist CountOfDays\"" IO_DEFAULT ".\n" );
    return;
  }

  if( !( ccStrParseFloat( argv[1], &savetimerange ) ) )
    goto syntaxerror;

  savetimerange *= (float)(24*60*60);
  basetimestamp = context->curtime - (int64_t)( savetimerange );

  ccGrowthInit( &growth, 512 );
  ccGrowthElapsedTimeString( &growth, (int64_t)savetimerange, 4 );
  ioPrintf( &context->output, 0, BSMSG_INFO "Time range for order retrieval : " IO_GREEN "%s" IO_DEFAULT ".\n", growth.data );
  ccGrowthFree( &growth );

  memset( &blorderlist, 0, sizeof(bsOrderList) );
  if( bsQueryBickLinkOrderList( context, &blorderlist, basetimestamp, basetimestamp ) )
    bsQueryUpgradeBickLinkOrderList( context, &blorderlist, BS_ORDER_INFOLEVEL_DETAILS );
  else
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Failed to fetch the BrickLink order list.\n" );
  memset( &boorderlist, 0, sizeof(bsOrderList) );
  if( bsQueryBickOwlOrderList( context, &boorderlist, basetimestamp, basetimestamp ) )
    bsQueryUpgradeBickOwlOrderList( context, &boorderlist, BS_ORDER_INFOLEVEL_DETAILS );
  else
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_WARNING "Failed to fetch the BrickOwl order list.\n" );

  ioPrintf( &context->output, 0, "\n" );

  /* Merge order lists and sort */
  mergeorderlist.ordercount = blorderlist.ordercount + boorderlist.ordercount;
  mergeorderlist.orderarray = malloc( mergeorderlist.ordercount * sizeof(bsOrder) );
  memcpy( &mergeorderlist.orderarray[ 0 ], blorderlist.orderarray, blorderlist.ordercount * sizeof(bsOrder) );
  memcpy( &mergeorderlist.orderarray[ blorderlist.ordercount ], boorderlist.orderarray, boorderlist.ordercount * sizeof(bsOrder) );
  tmp = malloc( mergeorderlist.ordercount * sizeof(bsOrder) );
  bsSortOrderListDate( mergeorderlist.orderarray, tmp, mergeorderlist.ordercount, (uint32_t)(((uintptr_t)&mergeorderlist)>>8) );
  free( tmp );

  textfile = fopen( "orderlist.txt", "w" );
  if( !( textfile ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to open \"" IO_RED "%s" IO_WHITE "\" for writing.\n", "orderlist.txt" );
  tabfile = fopen( "orderlist.tab.txt", "w" );
  if( !( tabfile ) )
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to open \"" IO_RED "%s" IO_WHITE "\" for writing.\n", "orderlist.tab.txt" );

  /* Save orders to disk */
  ioLogSetOutputFile( &context->output, textfile );
  ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, IO_WHITE "Service  " " " " Order  " "  " "Customer           " " " "Status     " " " "Date                " " " " Lots" " " " Parts" " " " SubTotal" " " " GrandTotal" " " " Payment" " " " Currency\n" IO_DEFAULT );
  if( tabfile )
    fprintf( tabfile, "Service\tOrder\tCustomer\tStatus\tDate\tLotCount\tPartCount\tSubTotal\tGrandTotal\tPayment\tCurrency\t\n" );
  salevalue[BS_ORDER_SERVICE_BRICKLINK] = 0.0f;
  salevalue[BS_ORDER_SERVICE_BRICKOWL] = 0.0f;
  grandtotal[BS_ORDER_SERVICE_BRICKLINK] = 0.0f;
  grandtotal[BS_ORDER_SERVICE_BRICKOWL] = 0.0f;
  blordercount = blorderlist.ordercount;
  boordercount = boorderlist.ordercount;
  for( orderindex = 0 ; orderindex < mergeorderlist.ordercount ; orderindex++ )
  {
    order = &mergeorderlist.orderarray[ orderindex ];
    ordertime = order->date;
    timeinfo = *( localtime( &ordertime ) );
    strftime( timebuf, 64, "%Y-%m-%d %H:%M:%S", &timeinfo );
    servicecolor = IO_MAGENTA;
    if( order->service == BS_ORDER_SERVICE_BRICKLINK )
      servicecolor = IO_GREEN;
    else if( order->service == BS_ORDER_SERVICE_BRICKOWL )
      servicecolor = IO_CYAN;
    salecolor = "";
    if( order->subtotal >= 100.0f )
      salecolor = IO_WHITE;
    ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, "%s" "%-9s" IO_DEFAULT " %8d  %-19s %-15s %-20s %s%5d %6d %9.2f %11.2f " IO_DEFAULT "%8.2f    %s \n", servicecolor, bsOrderServiceName[ order->service ], (int)order->id, ( order->customer ? order->customer : "???" ), bsGetOrderStatusColorString( order ), timebuf, salecolor, order->lotcount, order->partcount, order->subtotal, order->grandtotal, order->paymentgrandtotal, order->paymentcurrency );
    if( tabfile )
      fprintf( tabfile, "%s\t%d\t%s\t%s\t%s\t%d\t%d\t%f\t%f\t%f\t%s\t\n", bsOrderServiceName[ order->service ], (int)order->id, ( order->customer ? order->customer : "???" ), bsGetOrderStatusString( order ), timebuf, order->lotcount, order->partcount, order->subtotal, order->grandtotal, order->paymentgrandtotal, order->paymentcurrency );
    if( ( order->service == BS_ORDER_SERVICE_BRICKLINK ) && ( order->status == BL_ORDER_STATUS_CANCELLED ) )
      blordercount--;
    else if( ( order->service == BS_ORDER_SERVICE_BRICKOWL ) && ( order->status == BO_ORDER_STATUS_CANCELLED ) )
      boordercount--;
    else
    {
      salevalue[order->service] += order->subtotal;
      grandtotal[order->service] += order->grandtotal;
    }
  }

  ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, "\n" );
  ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, BSMSG_INFO "BrickLink Summary: " IO_GREEN "%3d" IO_DEFAULT " orders, sale value of " IO_GREEN "%7.2f" IO_DEFAULT ", grand total of " IO_GREEN "%7.2f" IO_DEFAULT ".\n", blordercount, salevalue[BS_ORDER_SERVICE_BRICKLINK], grandtotal[BS_ORDER_SERVICE_BRICKLINK] );
  ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, BSMSG_INFO "BrickOwl Summary:  " IO_GREEN "%3d" IO_DEFAULT " orders, sale value of " IO_GREEN "%7.2f" IO_DEFAULT ", grand total of " IO_GREEN "%7.2f" IO_DEFAULT ".\n", boordercount, salevalue[BS_ORDER_SERVICE_BRICKOWL], grandtotal[BS_ORDER_SERVICE_BRICKOWL] );
  ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, BSMSG_INFO "Global Summary:    " IO_GREEN "%3d" IO_DEFAULT " orders, sale value of " IO_GREEN "%7.2f" IO_DEFAULT ", grand total of " IO_GREEN "%7.2f" IO_DEFAULT ".\n", blordercount + boordercount, salevalue[BS_ORDER_SERVICE_BRICKLINK] + salevalue[BS_ORDER_SERVICE_BRICKOWL], grandtotal[BS_ORDER_SERVICE_BRICKLINK] + grandtotal[BS_ORDER_SERVICE_BRICKOWL] );
  ioPrintf( &context->output, IO_MODEBIT_OUTPUTFILE, "\n" );

  ioLogSetOutputFile( &context->output, 0 );

  if( textfile )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Order list was saved as a text file at \"" IO_GREEN "%s" IO_DEFAULT "\".\n", "orderlist.txt" );
    fclose( textfile );
  }
  if( tabfile )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Order list was saved as a tab-delimited file at \"" IO_GREEN "%s" IO_DEFAULT "\".\n", "orderlist.tab.txt" );
    fclose( tabfile );
  }

  /* Free merge list */
  free( mergeorderlist.orderarray );

  /* Free order lists */
  blFreeOrderList( &blorderlist );
  boFreeOrderList( &boorderlist );

  return;
}


static void bsCommandInvBlXml( bsContext *context, int argc, char **argv )
{
  bsxInventory *inv;

  if( argc != 2 )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"" IO_CYAN "invblxml foo.bsx" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, argv[1] ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );
  ioPrintf( &context->output, 0, BSMSG_INFO "Writing BrickLink upload XML file(s) for the loaded BSX file.\n" );

  bsOutputBrickLinkXML( context, inv, 0, BSX_ITEM_XFLAGS_TO_CREATE );

  bsxFreeInventory( inv );

  return;
}


static void bsCommandInvMyCost( bsContext *context, int argc, char **argv )
{
  int itemindex;
  bsxInventory *inv;
  bsxItem *item, *stockitem;
  double totalprice, totalmycost, costfactor;

  if( argc != 3 )
  {
    syntaxerror:
    ioPrintf( &context->output, 0, BSMSG_ERROR "Incorrect parameter count, usage is \"" IO_CYAN "invmycost foo.bsx totalmycost" IO_WHITE "\"" IO_DEFAULT ".\n" );
    return;
  }

  if( !( ccStrParseDouble( argv[2], &totalmycost ) ) )
    goto syntaxerror;

  inv = bsxNewInventory();
  if( !( bsxLoadInventory( inv, argv[1] ) ) )
  {
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to load a BSX file at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Current working directory is: \"" IO_GREEN "%s" IO_DEFAULT "\".\n", context->cwd );
    bsxFreeInventory( inv );
    return;
  }

  ioPrintf( &context->output, 0, BSMSG_INFO "Loaded inventory has " IO_GREEN "%d" IO_DEFAULT " items in " IO_GREEN "%d" IO_DEFAULT " lots.\n", inv->partcount, inv->itemcount );

  totalprice = 0.0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    totalprice += (double)item->price * (double)item->quantity;
  }

  costfactor = 0.0;
  if( totalprice > 0.01 )
    costfactor = totalmycost / totalprice;

  ioPrintf( &context->output, 0, BSMSG_INFO "The inventory has a total price of " IO_CYAN "%.3f" IO_DEFAULT ", target MyCost total is " IO_CYAN "%.3f" IO_DEFAULT ".\n", totalprice, totalmycost );

  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    item->mycost = costfactor * item->price;
  }

  if( bsxSaveInventory( argv[1], inv, 0, -32 ) )
    ioPrintf( &context->output, 0, BSMSG_INFO "We have updated MyCost fields for the BSX file at path \"" IO_GREEN "%s" IO_DEFAULT "\".\n", argv[1] );
  else
    ioPrintf( &context->output, 0, BSMSG_ERROR "We failed to write the BSX file back at path \"" IO_RED "%s" IO_WHITE "\".\n", argv[1] );
  bsxFreeInventory( inv );

  return;
}


////


#define BS_INPUT_MAX_ARGS (16)


static int bsRunCommand( bsContext *context, char *input, int runfileflag )
{
  int offset, argc;
  char *argv[BS_INPUT_MAX_ARGS];

  /* Remove the trailing \n or \r\n */
  offset = ccStrFindChar( input, '\n' );
  if( offset >= 0 )
  {
    if( ( offset > 0 ) && ( input[offset-1] == '\r' ) )
      offset--;
    input[offset] = 0;
  }

  /* Extract arguments */
  if( !( runfileflag ) )
    ioPrintf( &context->output, IO_MODEBIT_LOGONLY | IO_MODEBIT_FLUSH, "LOG: User command \"%s\"\n", input );
  else
  {
    ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_DEFAULT "\n" );
    ioPrintf( &context->output, IO_MODEBIT_FLUSH, BSMSG_INFO IO_WHITE "Runfile command: \"" IO_CYAN "%s" IO_WHITE "\"\n", input );
  }
  argc = ccParseParametersCut( input, argv, BS_INPUT_MAX_ARGS );
  if( argc < 1 )
    return 1;

  /* Handle user command */
  if( ccStrLowCmpWord( argv[0], "status" ) )
    bsCommandStatus( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "help" ) )
    bsCommandHelp( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "check" ) )
    bsCommandCheck( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "sync" ) )
    bsCommandSync( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "verify" ) )
    bsCommandVerify( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "autocheck" ) )
    bsCommandAutocheck( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "blmaster" ) )
    bsCommandBlMaster( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "sort" ) )
    bsCommandSort( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "add" ) )
    bsCommandAdd( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "sub" ) )
    bsCommandSub( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "loadprices" ) )
    bsCommandLoadPrices( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "loadnotes" ) )
    bsCommandLoadNotes( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "loadmycost" ) )
    bsCommandLoadMyCost( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "loadall" ) )
    bsCommandLoadAll( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "merge" ) )
    bsCommandMerge( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "item" ) )
    bsCommandItem( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "find" ) )
    bsCommandFind( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "listempty" ) )
    bsCommandListEmpty( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "setquantity" ) )
    bsCommandSetQuantity( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "setprice" ) )
    bsCommandSetPrice( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "setcomments" ) )
    bsCommandSetComments( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "setremarks" ) )
    bsCommandSetRemarks( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "setallremarksfromblid" ) )
    bsCommandSetAllRemarksFromBLID( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "setblid" ) )
    bsCommandSetBLID( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "delete" ) )
    bsCommandDelete( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlresolve" ) )
    bsCommandOwlResolve( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlqueryblid" ) )
    bsCommandOwlQueryBLID( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlsubmitblid" ) )
    bsCommandOwlSubmitBLID( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlupdateblid" ) )
    bsCommandOwlUpdateBLID( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlsubmitdims" ) )
    bsCommandOwlSubmitDims( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlsubmitweight" ) )
    bsCommandOwlSubmitWeight( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "owlforceblid" ) )
    bsCommandOwlForceBLID( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "consolidate" ) )
    bsCommandConsolidate( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "regradeused" ) )
    bsCommandRegradeUsed( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "evalset" ) )
    bsCommandEvalSet( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "evalgear" ) )
    bsCommandEvalGear( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "evalpartout" ) )
    bsCommandEvalPartout( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "evalinv" ) )
    bsCommandEvalInv( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "checkprices" ) )
    bsCommandCheckPrices( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "findorder" ) )
    bsCommandFindOrder( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "findordertime" ) )
    bsCommandFindOrderTime( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "saveorderlist" ) )
    bsCommandSaveOrderList( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "invblxml" ) )
    bsCommandInvBlXml( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "invmycost" ) )
    bsCommandInvMyCost( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "about" ) )
    bsCommandAbout( context, argc, argv );
 /* 
 else if( ccStrLowCmpWord( argv[0], "message" ) )
    bsCommandMessage( context, argc, argv );
 */ 
  else if( ccStrLowCmpWord( argv[0], "runfile" ) )
  {
    if( !( runfileflag ) )
      bsCommandRunFile( context, argc, argv );
    else
      ioPrintf( &context->output, 0, BSMSG_ERROR "The \"" IO_CYAN "runfile" IO_WHITE "\" command can not be used from within another \"" IO_CYAN "runfile" IO_WHITE "\" command.\n" );
  }
  else if( ccStrLowCmpWord( argv[0], "register" ) )
    bsCommandRegister( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "backup" ) )
    bsCommandBackup( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "prunebackups" ) )
    bsCommandPruneBackups( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "resetapihistory" ) )
    bsCommandResetApiHistory( context, argc, argv );
  else if( ccStrLowCmpWord( argv[0], "quit" ) )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Quit command issued. Exiting.\n" );
    return 0;
  }
  else
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "Unknown command \"%s\"\n", argv[0] );
    ioPrintf( &context->output, 0, BSMSG_INFO "Type \"help\" for guidance.\n" );
  }
  if( !( runfileflag ) )
    ioPrintf( &context->output, IO_MODEBIT_NODATE, IO_DEFAULT "\n" );

  return 1;
}


int bsParseInput( bsContext *context, int *retactionflag )
{
  int retval;
  char *input;

  DEBUG_SET_TRACKER();

  /* Capture stdin from thread */
  input = ioStdinReadAlloc( 0 );
  if( !( input ) )
    return 1;

  /* Remove line marker, user already entered a '\n' */
  context->output.linemarker = 0;

  /* Execute command */
  retval = bsRunCommand( context, input, 0 );

  free( input );
  ioLogFlush( &context->output );
  if( retactionflag )
    *retactionflag = 1;
  return retval;
}



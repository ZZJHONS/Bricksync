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

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"
#include "iolog.h"
#include "debugtrack.h"
#include "rand.h"
#include "xml.h"

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

#include "bricksync.h"


////


#define BS_BLXML_UPLOAD_SIZE_MAX (204800-4096)
#define BS_BLXML_UPDATE_SIZE_MAX (204800-4096)


void bsClearBrickLinkXML( bsContext *context )
{
  int uploadindex, updateindex, retvalue;
  char *path;

  DEBUG_SET_TRACKER();

  /* Remove old files */
  for( uploadindex = 0 ; uploadindex < 256 ; uploadindex++ )
  {
    path = ccStrAllocPrintf( BS_BLXMLUPLOAD_FILE, uploadindex );
    retvalue = remove( path );
    free( path );
    if( retvalue )
      break;
  }
  for( updateindex = 0 ; updateindex < 256 ; updateindex++ )
  {
    path = ccStrAllocPrintf( BS_BLXMLUPDATE_FILE, updateindex );
    retvalue = remove( path );
    free( path );
    if( retvalue )
      break;
  }

  context->bricklink.xmluploadindex = 0;
  context->bricklink.xmlupdateindex = 0;

  return;
}


int bsOutputBrickLinkXML( bsContext *context, bsxInventory *inv, int apiwarningflag, int itemforceflags )
{
  int itemindex, uploadindex, updateindex;
  int32_t itemflags;
  int uploadsize, updatesize;
  float apihistoryratio;
  char subcondition;
  bsxItem *item;
  FILE *bluploadfile;
  FILE *blupdatefile;
  char *path;
  char *encodedstring;
  char *histcolor;
  const char *emptystring = "";

  DEBUG_SET_TRACKER();

  /* Do we have entries left in inventory, requiring manual uploads? */
  if( !( inv->itemcount - inv->itemfreecount ) )
    return 1;

  if( apiwarningflag )
  {
    apihistoryratio = (float)context->bricklink.apihistory.total / (float)context->bricklink.apicountlimit;
    if( apihistoryratio > 0.75 )
      histcolor = IO_RED;
    else if( apihistoryratio > 0.50 )
      histcolor = IO_YELLOW;
    else
      histcolor = IO_GREEN;
    ioPrintf( &context->output, 0, BSMSG_WARNING "Updates were not applied for " IO_RED "%d" IO_WHITE " lots, the pool of available daily BrickLink API calls is " IO_RED "too low" IO_WHITE ".\n", (int)( inv->itemcount - inv->itemfreecount ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickLink API usage : " "%s" "%d" IO_DEFAULT " (" "%s" "%.2f%%" IO_DEFAULT ") in the past 24 hours; " "%s" "%d" IO_DEFAULT " in the past hour.\n", histcolor, (int)context->bricklink.apihistory.total, histcolor, 100.0 * apihistoryratio, histcolor, bsApiHistoryCountPeriod( &context->bricklink.apihistory, 3600 ) );
    ioPrintf( &context->output, 0, BSMSG_INFO "You can manually upload the following XML files to BrickLink. This is "  IO_WHITE "absolutely optional" IO_DEFAULT ".\n" );
  }

  bsClearBrickLinkXML( context );

  /* Output the XML */
  uploadindex = context->bricklink.xmluploadindex;
  updateindex = context->bricklink.xmlupdateindex;
  bluploadfile = 0;
  blupdatefile = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    itemflags = item->flags | itemforceflags;
    if( itemflags & BSX_ITEM_FLAGS_DELETED )
      continue;

    if( itemflags & BSX_ITEM_XFLAGS_TO_CREATE )
    {
      if( !( bluploadfile ) )
      {
        path = ccStrAllocPrintf( BS_BLXMLUPLOAD_FILE, uploadindex );
        bluploadfile = fopen( path, "w" );
        if( !( bluploadfile ) )
        {
          ioPrintf( &context->output, 0, BSMSG_ERROR "Failed to write XML file at\"" IO_RED "%s" IO_WHITE "\".\n", path );
          free( path );
          break;
        }
        ioPrintf( &context->output, 0, BSMSG_INFO "New " IO_GREEN "BrickLink XML Upload" IO_DEFAULT " file : \"" IO_CYAN "%s" IO_DEFAULT "\"\n", path );
        free( path );
        uploadindex++;
        uploadsize = 0;
        uploadsize += fprintf( bluploadfile, "<INVENTORY>\n" );
      }

      /* Write per-item XML upload */
      uploadsize += fprintf( bluploadfile, " <ITEM>\n" );
      uploadsize += fprintf( bluploadfile, "  <ITEMTYPE>%c</ITEMTYPE>\n", item->typeid );
      uploadsize += fprintf( bluploadfile, "  <ITEMID>%s</ITEMID>\n", item->id );
      if( item->categoryid )
        uploadsize += fprintf( bluploadfile, "  <CATEGORY>%d</CATEGORY>\n", item->categoryid );
      uploadsize += fprintf( bluploadfile, "  <COLOR>%d</COLOR>\n", item->colorid );
      uploadsize += fprintf( bluploadfile, "  <PRICE>%.3f</PRICE>\n", item->price );
      uploadsize += fprintf( bluploadfile, "  <QTY>%d</QTY>\n", item->quantity );
      uploadsize += fprintf( bluploadfile, "  <CONDITION>%c</CONDITION>\n", item->condition );
      if( item->typeid == 'S' )
      {
        subcondition = 'S';
        if( item->completeness == 'C' )
          subcondition = 'C';
        else if( item->completeness == 'B' )
          subcondition = 'I';
        uploadsize += fprintf( bluploadfile, "  <SUBCONDITION>%c</SUBCONDITION>\n", subcondition );
      }
      if( item->bulk >= 2 )
        uploadsize += fprintf( bluploadfile, "  <BULK>%d</BULK>\n", item->bulk );
      if( item->comments )
      {
        encodedstring = xmlEncodeEscapeString( item->comments, strlen( item->comments ), 0 );
        uploadsize += fprintf( bluploadfile, "  <DESCRIPTION>%s</DESCRIPTION>\n", encodedstring );
        free( encodedstring );
      }
      if( item->remarks )
      {
        encodedstring = xmlEncodeEscapeString( item->remarks, strlen( item->remarks ), 0 );
        uploadsize += fprintf( bluploadfile, "  <REMARKS>%s</REMARKS>\n", encodedstring );
        free( encodedstring );
      }
      if( item->mycost > 0.001 )
        uploadsize += fprintf( bluploadfile, "  <MYCOST>%.3f</MYCOST>\n", item->mycost );
      if( item->tq1 )
      {
        uploadsize += fprintf( bluploadfile, "  <TQ1>%d</TQ1>\n", item->tq1 );
        uploadsize += fprintf( bluploadfile, "  <TP1>%.3f</TP1>\n", item->tp1 );
        uploadsize += fprintf( bluploadfile, "  <TQ2>%d</TQ2>\n", item->tq2 );
        uploadsize += fprintf( bluploadfile, "  <TP2>%.3f</TP2>\n", item->tp2 );
        uploadsize += fprintf( bluploadfile, "  <TQ3>%d</TQ3>\n", item->tq3 );
        uploadsize += fprintf( bluploadfile, "  <TP3>%.3f</TP3>\n", item->tp3 );
      }

      uploadsize += fprintf( bluploadfile, " </ITEM>\n" );

      if( uploadsize >= BS_BLXML_UPLOAD_SIZE_MAX )
      {
        fprintf( bluploadfile, "</INVENTORY>\n" );
        fclose( bluploadfile );
        bluploadfile = 0;
      }
    }
    else if( itemflags & ( BSX_ITEM_XFLAGS_TO_DELETE | BSX_ITEM_XFLAGS_TO_UPDATE ) )
    {
      if( !( blupdatefile ) )
      {
        path = ccStrAllocPrintf( BS_BLXMLUPDATE_FILE, updateindex );
        blupdatefile = fopen( path, "w" );
        if( !( blupdatefile ) )
        {
          ioPrintf( &context->output, 0, BSMSG_ERROR "Failed to write XML file at\"" IO_RED "%s" IO_WHITE "\".\n", path );
          free( path );
          break;
        }
        ioPrintf( &context->output, 0, BSMSG_INFO "New " IO_GREEN "BrickLink XML Update" IO_DEFAULT " file : \"" IO_CYAN "%s" IO_DEFAULT "\"\n", path );
        free( path );
        updateindex++;
        updatesize = 0;
        updatesize += fprintf( blupdatefile, "<INVENTORY>\n" );
      }

      /* Write per-item XML update */
      updatesize += fprintf( blupdatefile, " <ITEM>\n" );
      updatesize += fprintf( blupdatefile, "  <LOTID>"CC_LLD"</LOTID>\n", (long long)item->lotid );
      /* We are doing a substraction from main inventory */
      if( itemflags & BSX_ITEM_XFLAGS_TO_DELETE )
      {
        /* Lot is gone forever */
        updatesize += fprintf( blupdatefile, "  <DELETE>Y</DELETE>\n" );
      }
      else if( itemflags & BSX_ITEM_XFLAGS_TO_UPDATE )
      {
        if( itemflags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
        {
          updatesize += fprintf( blupdatefile, "  <QTY>%c%d</QTY>\n", ( item->quantity >= 0 ? '-' : '+' ), abs( item->quantity ) );
          if( !( itemflags & BSX_ITEM_XFLAGS_UPDATE_STOCKROOM ) )
            updatesize += fprintf( blupdatefile, "  <STOCKROOM>N</STOCKROOM>\n" );
        }
        if( itemflags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
        {
          encodedstring = (char *)emptystring;
          if( item->comments )
            encodedstring = xmlEncodeEscapeString( item->comments, strlen( item->comments ), 0 );
          updatesize += fprintf( blupdatefile, "  <DESCRIPTION>%s</DESCRIPTION>\n", encodedstring );
          if( encodedstring != emptystring )
            free( encodedstring );
        }
        if( itemflags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
        {
          encodedstring = (char *)emptystring;
          if( item->remarks )
            encodedstring = xmlEncodeEscapeString( item->remarks, strlen( item->remarks ), 0 );
          updatesize += fprintf( blupdatefile, "  <REMARKS>%s</REMARKS>\n", encodedstring );
          if( encodedstring != emptystring )
            free( encodedstring );
        }
        if( itemflags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
          updatesize += fprintf( blupdatefile, "  <PRICE>%.3f</PRICE>\n", item->price );
        if( itemflags & BSX_ITEM_XFLAGS_UPDATE_BULK )
          updatesize += fprintf( blupdatefile, "  <BULK>%d</BULK>\n", ( item->bulk >= 2 ? item->bulk : 1 ) );
        if( itemflags & BSX_ITEM_XFLAGS_UPDATE_STOCKROOM )
        {
          updatesize += fprintf( blupdatefile, "  <STOCKROOM>Y</STOCKROOM>\n" );
          updatesize += fprintf( blupdatefile, "  <RETAIN>Y</RETAIN>\n" );
        }
        if( ( itemflags & BSX_ITEM_XFLAGS_UPDATE_MYCOST ) && ( item->mycost > 0.001 ) )
          updatesize += fprintf( blupdatefile, "  <MYCOST>%.3f</MYCOST>\n", item->mycost );
        if( ( itemflags & BSX_ITEM_XFLAGS_UPDATE_TIERPRICES ) && ( item->tq1 ) )
        {
          uploadsize += fprintf( bluploadfile, "  <TQ1>%d</TQ1>\n", item->tq1 );
          uploadsize += fprintf( bluploadfile, "  <TP1>%.3f</TP1>\n", item->tp1 );
          uploadsize += fprintf( bluploadfile, "  <TQ2>%d</TQ2>\n", item->tq2 );
          uploadsize += fprintf( bluploadfile, "  <TP2>%.3f</TP2>\n", item->tp2 );
          uploadsize += fprintf( bluploadfile, "  <TQ3>%d</TQ3>\n", item->tq3 );
          uploadsize += fprintf( bluploadfile, "  <TP3>%.3f</TP3>\n", item->tp3 );
        }
      }
      updatesize += fprintf( blupdatefile, " </ITEM>\n" );

      if( updatesize >= BS_BLXML_UPDATE_SIZE_MAX )
      {
        fprintf( blupdatefile, "</INVENTORY>\n" );
        fclose( blupdatefile );
        blupdatefile = 0;
      }
    }
  }

  if( bluploadfile )
  {
    fprintf( bluploadfile, "</INVENTORY>\n" );
    fclose( bluploadfile );
    bluploadfile = 0;
  }
  if( blupdatefile )
  {
    fprintf( blupdatefile, "</INVENTORY>\n" );
    fclose( blupdatefile );
    blupdatefile = 0;
  }

  if( apiwarningflag )
  {
    ioPrintf( &context->output, 0, BSMSG_INFO "When the pool of available API calls reaches healthy levels again, BrickSync will verify if updates have been manually applied or not.\n" );
    ioPrintf( &context->output, 0, BSMSG_INFO "BrickSync will resume updates if changes from the XML file%s haven't been applied.\n\n", ( ( uploadindex + updateindex ) >= 2 ? "s" : "" ) );
  }

  if( ( uploadindex > context->bricklink.xmluploadindex ) || ( updateindex > context->bricklink.xmlupdateindex ) )
  {
    if( apiwarningflag )
      context->stateflags |= BS_STATE_FLAGS_BRICKLINK_PARTIAL_SYNC;
    context->contextflags |= BS_CONTEXT_FLAGS_UPDATED_STATE;
    context->bricklink.xmluploadindex = uploadindex;
    context->bricklink.xmlupdateindex = updateindex;
  }

  return 1;
}



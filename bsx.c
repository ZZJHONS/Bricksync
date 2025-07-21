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
#include <time.h>
#include <errno.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "mmatomic.h"
#include "mmbitmap.h"

/* For mkdir() */
#if CC_UNIX
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <sys/types.h>
 #include <sys/stat.h>
#else
 #error Unknown/Unsupported platform!
#endif


#include "bsx.h"


////


#ifndef ADDRESS
 #define ADDRESS(p,o) ((void *)(((char *)p)+(o)))
#endif

static inline int intMin( int x, int y )
{
  return ( x < y ? x : y );
}

static inline int intMax( int x, int y )
{
  return ( x > y ? x : y );
}


#define ITEM_READ_DEBUG (0)


////


#define XML_CHAR_IS_DELIMITER(c) (((c)<=' ')||((c)=='<'))

int xmlStrParseInt( char *str, int64_t *retint )
{
  int negflag;
  char c;
  int64_t workint;

  if( !( str ) )
    return 0;
  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  workint = 0;
  for( ; ; str++ )
  {
    c = *str;
    if( XML_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workint = ( workint * 10 ) + ( c - '0' );
  }

  if( negflag )
    workint = -workint;
  *retint = workint;
  return 1;
}


int xmlStrParseFloat( char *str, float *retfloat )
{
  char c;
  int negflag;
  double workdouble;
  double decfactor;

  if( !( str ) )
    return 0;

  negflag = 0;
  if( *str == '-' )
    negflag = 1;
  str += negflag;

  workdouble = 0.0;
  for( ; ; str++ )
  {
    c = *str;
    if( XML_CHAR_IS_DELIMITER( c ) )
      goto done;
    if( c == '.' )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workdouble = ( workdouble * 10.0 ) + (double)( c - '0' );
  }

  str++;
  decfactor = 0.1;
  for( ; ; str++ )
  {
    c = *str;
    if( XML_CHAR_IS_DELIMITER( c ) )
      break;
    if( ( c < '0' ) || ( c > '9' ) )
      return 0;
    workdouble += (double)( c - '0' ) * decfactor;
    decfactor *= 0.1;
  }

  done:

  if( negflag )
    workdouble = -workdouble;
  *retfloat = (float)workdouble;

  return 1;
}


////



/* Build string with escape chars as required, returned string must be free()'d */
char *xmlEncodeEscapeString( char *string, int length, int *retlength )
{
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( 6*length + 1 );
  for( dst = dstbase ; length ; length--, string++ )
  {
    c = *string;
    if( c == '&' )
    {
      dst[0] = '&';
      dst[1] = 'a';
      dst[2] = 'm';
      dst[3] = 'p';
      dst[4] = ';';
      dst += 5;
    }
    else if( c == '<' )
    {
      dst[0] = '&';
      dst[1] = 'l';
      dst[2] = 't';
      dst[3] = ';';
      dst += 4;
    }
    else if( c == '>' )
    {
      dst[0] = '&';
      dst[1] = 'g';
      dst[2] = 't';
      dst[3] = ';';
      dst += 4;
    }
    else if( c == '"' )
    {
      dst[0] = '&';
      dst[1] = 'q';
      dst[2] = 'u';
      dst[3] = 'o';
      dst[4] = 't';
      dst[5] = ';';
      dst += 6;
    }
    else if( c == '\'' )
    {
      dst[0] = '&';
      dst[1] = 'a';
      dst[2] = 'p';
      dst[3] = 'o';
      dst[4] = 's';
      dst[5] = ';';
      dst += 6;
    }
    else
      *dst++ = c;
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );

  return dstbase;
}


/* Build string with decoded escape chars, returned string must be free()'d */
char *xmlDecodeEscapeString( char *string, int length, int *retlength )
{
  int skip;
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( length + 1 );
  for( dst = dstbase ; length ; length -= skip, string += skip )
  {
    c = *string;
    skip = 1;
    if( c != '&' )
      *dst++ = c;
    else
    {
      if( !( --length ) )
        goto error;
      string++;
      if( ccStrCmpSeq( string, "lt;", 3 ) )
      {
        *dst++ = '<';
        skip = 3;
      }
      else if( ccStrCmpSeq( string, "gt;", 3 ) )
      {
        *dst++ = '>';
        skip = 3;
      }
      else if( ccStrCmpSeq( string, "quot;", 4 ) )
      {
        *dst++ = '"';
        skip = 4;
      }
      else if( ccStrCmpSeq( string, "amp;", 4 ) )
      {
        *dst++ = '&';
        skip = 4;
      }
      else if( ccStrCmpSeq( string, "apos;", 4 ) )
      {
        *dst++ = '\'';
        skip = 4;
      }
      else
      {
        *dst++ = '&';
        skip = 0;
      }
    }
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );
  return dstbase;

  error:
  free( dstbase );
  return 0;
}


////


char *bsxReadString( char **readvalue, char *string, char *closestring )
{
  int len, stringlen;
  *readvalue = string;
  len = strlen( closestring );
  if( !( string = ccStrFindSeq( string, closestring, len ) ) )
  {
    printf( "BSX READ ERROR: Failed to locate matching %s\n", closestring );
    return 0;
  }
  *string = 0;
  stringlen = (int)( string - *readvalue );
  if( !( stringlen ) )
    *readvalue = 0;
  return string + len;
}

char *bsxReadStringAlloc( char **readvalue, char *string, char *closestring )
{
  int len, stringlen;
  char *value;
  value = string;
  len = strlen( closestring );
  if( !( string = ccStrFindSeq( string, closestring, len ) ) )
  {
    printf( "BSX READ ERROR: Failed to locate matching %s\n", closestring );
    return 0;
  }
  *string = 0;
  stringlen = (int)( string - value );
  if( !( stringlen ) )
    value = 0;
  *readvalue = 0;
  if( value )
  {
    *readvalue = malloc( stringlen + 1 );
    memcpy( *readvalue, value, stringlen + 1 );
  }
  return string + len;
}

char *bsxReadInt( int *readvalue, char *string, char *closestring )
{
  int64_t readint;
  if( !( xmlStrParseInt( string, &readint ) ) )
  {
    printf( "ERROR: Failed to read int : %.*s\n", 32, string );
    return 0;
  }
  *readvalue = (int)readint;
  if( !( string = ccStrFindStrSkip( string, closestring ) ) )
  {
    printf( "BSX READ ERROR: Failed to locate matching %s\n", closestring );
    return 0;
  }
  return string;
}

char *bsxReadInt64( int64_t *readvalue, char *string, char *closestring )
{
  int64_t readint;
  if( !( xmlStrParseInt( string, &readint ) ) )
  {
    printf( "ERROR: Failed to read int : %.*s\n", 32, string );
    return 0;
  }
  *readvalue = (int64_t)readint;
  if( !( string = ccStrFindStrSkip( string, closestring ) ) )
  {
    printf( "BSX READ ERROR: Failed to locate matching %s\n", closestring );
    return 0;
  }
  return string;
}

char *bsxReadFloat( float *readvalue, char *string, char *closestring )
{
  float readfloat;
  if( !( xmlStrParseFloat( string, &readfloat ) ) )
  {
    printf( "ERROR: Failed to read int : %.*s\n", 32, string );
    return 0;
  }
  *readvalue = (float)readfloat;
  if( !( string = ccStrFindStrSkip( string, closestring ) ) )
  {
    printf( "BSX READ ERROR: Failed to locate matching %s\n", closestring );
    return 0;
  }
  return string;
}

char *bsxReadChar( char *readvalue, char *string, char *closestring )
{
  *readvalue = *string;
  if( !( string = ccStrFindStrSkip( string, closestring ) ) )
  {
    printf( "BSX READ ERROR: Failed to locate matching %s\n", closestring );
    return 0;
  }
  return string;
}


////


bsxInventory *bsxNewInventory()
{
  bsxInventory *inv;
  inv = malloc( sizeof(bsxInventory) );
  memset( inv, 0, sizeof(bsxInventory) );
  return inv;
}


static char *bsxReadItem( bsxItem *item, char *input, int *successflag )
{
  int offset, itemflags;
  char *string;
  char *encodedstring, *decodedstring;

  bsxClearItem( item );
  input = ccStrFindStrSkip( input, "<Item>" );
  if( !( input ) )
  {
    *successflag = 1;
    return 0;
  }
  itemflags = 0x0;

#if ITEM_READ_DEBUG
  printf( "Start Item\n" );
#endif

  for( ; ; )
  {
    offset = ccStrFindChar( input, '<' );
    if( offset < 0 )
    {
      printf( "ERROR: Failed to locate matching </Item>\n" );
      return 0;
    }
    input = &input[offset+1];

    if( ( string = ccStrCmpWord( input, "/Item>" ) ) )
      break;
    else if( ( string = ccStrCmpWord( input, "ItemID>" ) ) )
    {
      if( !( input = bsxReadString( &item->id, string, "</ItemID>" ) ) )
        return 0;
      itemflags |= 0x1;
#if ITEM_READ_DEBUG
printf( "  Read Item ID : %s\n", item->id );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "ItemTypeID>" ) ) )
    {
      if( !( input = bsxReadChar( &item->typeid, string, "</ItemTypeID>" ) ) )
        return 0;
      itemflags |= 0x2;
#if ITEM_READ_DEBUG
printf( "  Read Type ID : %c\n", item->typeid );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "ColorID>" ) ) )
    {
      if( !( input = bsxReadInt( &item->colorid, string, "</ColorID>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Color ID : %d\n", item->colorid );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "ItemName>" ) ) )
    {
      if( !( input = bsxReadString( &encodedstring, string, "</ItemName>" ) ) )
        return 0;
      decodedstring = xmlDecodeEscapeString( encodedstring, strlen( encodedstring ), 0 );
      bsxSetItemName( item, decodedstring, -1 );
      free( decodedstring );
#if ITEM_READ_DEBUG
printf( "  Read Item Name : %s\n", item->name );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "ItemTypeName>" ) ) )
    {
      if( !( input = bsxReadString( &item->typename, string, "</ItemTypeName>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Item Type Name : %s\n", item->typename );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "ColorName>" ) ) )
    {
      if( !( input = bsxReadString( &item->colorname, string, "</ColorName>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Color Name : %s\n", item->colorname );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "CategoryID>" ) ) )
    {
      if( !( input = bsxReadInt( &item->categoryid, string, "</CategoryID>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Category ID : %d\n", item->categoryid );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "CategoryName>" ) ) )
    {
      if( !( input = bsxReadString( &item->categoryname, string, "</CategoryName>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Category Name : %s\n", item->categoryname );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Status>" ) ) )
    {
      if( !( input = bsxReadChar( &item->status, string, "</Status>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Status : %c\n", item->status );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Qty>" ) ) )
    {
      if( !( input = bsxReadInt( &item->quantity, string, "</Qty>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Quantity : %d\n", item->quantity );
#endif
      itemflags |= 0x4;
    }
    else if( ( string = ccStrCmpWord( input, "Price>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->price, string, "</Price>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Price : %f\n", item->price );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "SalePrice>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->saleprice, string, "</SalePrice>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read SalePrice : %f\n", item->saleprice );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Condition>" ) ) )
    {
      if( !( input = bsxReadChar( &item->condition, string, "</Condition>" ) ) )
        return 0;
      itemflags |= 0x8;
#if ITEM_READ_DEBUG
printf( "  Read Condition : %c\n", item->condition );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "UsedGrade>" ) ) )
    {
      if( !( input = bsxReadChar( &item->usedgrade, string, "</UsedGrade>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read UsedGrade : %c\n", item->usedgrade );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Completeness>" ) ) )
    {
      if( !( input = bsxReadChar( &item->completeness, string, "</Completeness>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Completeness : %c\n", item->completeness );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Bulk>" ) ) )
    {
      if( !( input = bsxReadInt( &item->bulk, string, "</Bulk>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Bulk : %d\n", item->bulk );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "OrigPrice>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->origprice, string, "</OrigPrice>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Original Price : %f\n", item->origprice );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Comments>" ) ) )
    {
      if( !( input = bsxReadString( &encodedstring, string, "</Comments>" ) ) )
        return 0;
      decodedstring = xmlDecodeEscapeString( encodedstring, strlen( encodedstring ), 0 );
      bsxSetItemComments( item, decodedstring, -1 );
      free( decodedstring );
#if ITEM_READ_DEBUG
printf( "  Read Comments : %s\n", item->comments );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Remarks>" ) ) )
    {
      if( !( input = bsxReadString( &encodedstring, string, "</Remarks>" ) ) )
        return 0;
      decodedstring = xmlDecodeEscapeString( encodedstring, strlen( encodedstring ), 0 );
      bsxSetItemRemarks( item, decodedstring, -1 );
      free( decodedstring );
#if ITEM_READ_DEBUG
printf( "  Read Remarks : %s\n", item->remarks );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "OrigQty>" ) ) )
    {
      if( !( input = bsxReadInt( &item->origquantity, string, "</OrigQty>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Original Quantity : %d\n", item->origquantity );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "MyCost>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->mycost, string, "</MyCost>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read My Cost : %f\n", item->mycost );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "TQ1>" ) ) )
    {
      if( !( input = bsxReadInt( &item->tq1, string, "</TQ1>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read TQ1 : %d\n", item->tq1 );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "TP1>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->tp1, string, "</TP1>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read TP1 : %f\n", item->tp1 );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "TQ2>" ) ) )
    {
      if( !( input = bsxReadInt( &item->tq2, string, "</TQ2>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read TQ2 : %d\n", item->tq2 );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "TP2>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->tp2, string, "</TP2>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read TP2 : %f\n", item->tp2 );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "TQ3>" ) ) )
    {
      if( !( input = bsxReadInt( &item->tq3, string, "</TQ3>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read TQ3 : %d\n", item->tq3 );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "TP3>" ) ) )
    {
      if( !( input = bsxReadFloat( &item->tp3, string, "</TP3>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read TP3 : %f\n", item->tp3 );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "LotID>" ) ) )
    {
      if( !( input = bsxReadInt64( &item->lotid, string, "</LotID>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read LotID : "CC_LLD"\n", (long long)item->lotid );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "OwlID>" ) ) )
    {
      if( !( input = bsxReadInt64( &item->boid, string, "</OwlID>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read OwlID : "CC_LLD"\n", (long long)item->boid );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "OwlLotID>" ) ) )
    {
      if( !( input = bsxReadInt64( &item->bolotid, string, "</OwlLotID>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read OwlLotID : "CC_LLD"\n", (long long)item->bolotid );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "Sale>" ) ) )
    {
      if( !( input = bsxReadInt( &item->sale, string, "</Sale>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read Sale : %d\n", item->sale );
#endif
    }
    else if( ( string = ccStrCmpWord( input, "AlternateID>" ) ) )
    {
      if( !( input = bsxReadInt( &item->alternateid, string, "</AlternateID>" ) ) )
        return 0;
#if ITEM_READ_DEBUG
printf( "  Read AlternateID : %d\n", item->alternateid );
#endif
    }
    else
    {
#if 0
      printf( "WARNING: Unknown tag %.*s\n", 32, input );
#endif
      /* Hack */
      offset = ccStrFindChar( input, '\n' );
      if( offset < 0 )
      {
        printf( "ERROR: Failed to locate line break\n" );
        return 0;
      }
      input += offset;
    }
  }

  if( ( itemflags & 0xf ) != 0xf )
  {
    printf( "ERROR: Incomplete item ( code 0x%x )\n", itemflags );
    return 0;
  }

  /* Item verification */
  bsxVerifyItem( item );

  /* DEBUG */
  /* DEBUG */
  if( item->condition == 'A' )
    item->condition = 'U';
  /* DEBUG */
  /* DEBUG */

#if ITEM_READ_DEBUG
  printf( "Item Done\n" );
#endif

  return input;
}


static char *bsxReadOrder( bsxInventory *inv, char *input )
{
  int offset;
  char *string;

#if ITEM_READ_DEBUG
  printf( "Start Order\n" );
#endif

  for( ; ; )
  {
    offset = ccStrFindChar( input, '<' );
    if( offset < 0 )
    {
      printf( "ERROR: Failed to locate matching </Order>\n" );
      return 0;
    }
    input = &input[offset+1];

    if( ( string = ccStrCmpWord( input, "/Order>" ) ) )
      break;
    else if( ( string = ccStrCmpWord( input, "Service>" ) ) )
    {
      if( !( input = bsxReadStringAlloc( &inv->order.service, string, "</Service>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "OrderID>" ) ) )
    {
      if( !( input = bsxReadInt( &inv->order.orderid, string, "</OrderID>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "OrderDate>" ) ) )
    {
      if( !( input = bsxReadInt64( &inv->order.orderdate, string, "</OrderDate>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "Customer>" ) ) )
    {
      if( !( input = bsxReadStringAlloc( &inv->order.customer, string, "</Customer>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "SubTotal>" ) ) )
    {
      if( !( input = bsxReadFloat( &inv->order.subtotal, string, "</SubTotal>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "GrandTotal>" ) ) )
    {
      if( !( input = bsxReadFloat( &inv->order.grandtotal, string, "</GrandTotal>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "Payment>" ) ) )
    {
      if( !( input = bsxReadFloat( &inv->order.payment, string, "</Payment>" ) ) )
        return 0;
    }
    else if( ( string = ccStrCmpWord( input, "Currency>" ) ) )
    {
      if( !( input = bsxReadStringAlloc( &inv->order.currency, string, "</Currency>" ) ) )
        return 0;
    }
    else
    {
#if 0
      printf( "WARNING: Unknown tag %.*s\n", 32, input );
#endif
      /* Hack */
      offset = ccStrFindChar( input, '\n' );
      if( offset < 0 )
      {
        printf( "ERROR: Failed to locate line break\n" );
        return 0;
      }
      input += offset;
    }
  }

#if ITEM_READ_DEBUG
  printf( "Order Done\n" );
#endif

  return input;
}


int bsxLoadInventory( bsxInventory *inv, char *path )
{
  int sectionlength, offset, successflag;
  bsxItem *item;
  char *ordersection, *inventorysection, *string;

  if( inv->xmldata )
    printf( "WARNING: inv->xmldata already defined when bsxLoadInventory() is called\n" );

  bsxEmptyInventory( inv );
  inv->xmldata = ccFileLoad( path, 16*1048576, &inv->xmlsize );
  if( !( inv->xmldata ) )
    return 0;

  /* Locate sections */
  ordersection = ccStrFindStrSkip( inv->xmldata, "<Order>" );
  inventorysection = ccStrFindStrSkip( inv->xmldata, "<Inventory" );

  /* Cut sections */
  if( !( ccStrFindStr( ordersection, "</Order>" ) ) )
    ordersection = 0;
  if( !( ccStrFindStr( inventorysection, "</Inventory>" ) ) )
    inventorysection = 0;

  /* Validate */
  if( !( inventorysection ) )
  {
    free( inv->xmldata );
    inv->xmldata = 0;
    return 0;
  }

  /* Reader <Order> section */
  if( ordersection )
  {
    if( bsxReadOrder( inv, ordersection ) )
      inv->orderblockflag = 1;
    else
      printf( "WARNING: Failed to load BSX Order block\n" );
  }

  /* Reader <Inventory> section */
  inv->itemcount = 0;
  inv->itemalloc = 0;
  inv->itemlist = 0;
  string = inventorysection;
  successflag = 0;
  for( ; ; )
  {
    if( inv->itemcount >= inv->itemalloc )
    {
      inv->itemalloc = intMax( 16384, inv->itemalloc << 1 );
      inv->itemlist = realloc( inv->itemlist, inv->itemalloc * sizeof(bsxItem) );
    }
    item = &inv->itemlist[ inv->itemcount ];
    if( !( string = bsxReadItem( item, string, &successflag ) ) )
      break;
    /* Increment inventory */
    inv->partcount += item->quantity;
    inv->totalprice += (double)item->quantity * (double)item->price;
    inv->totalorigprice += (double)item->quantity * (double)item->origprice;
    inv->itemcount++;
  }

  return successflag;
}


static void bsxFreeItem( bsxItem *item, int clearflag )
{
  if( item->flags & BSX_ITEM_FLAGS_DELETED )
    return;
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_ID )
    free( item->id );
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_NAME )
    free( item->name );
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_TYPENAME )
    free( item->typename );
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_COLORNAME )
    free( item->colorname );
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_CATEGORYNAME )
    free( item->categoryname );
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_COMMENTS )
    free( item->comments );
  if( item->flags & BSX_ITEM_FLAGS_ALLOC_REMARKS )
    free( item->remarks );
  if( clearflag )
  {
    bsxClearItem( item );
    item->flags |= BSX_ITEM_FLAGS_DELETED;
  }
  return;
}


void bsxEmptyInventory( bsxInventory *inv )
{
  int itemindex;
  bsxItem *item;

  /* Free inventory section */
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
    bsxFreeItem( item, 0 );
  if( inv->itemlist )
    free( inv->itemlist );

  /* Free order section */
  if( inv->order.service )
    free( inv->order.service );
  if( inv->order.customer )
    free( inv->order.customer );
  if( inv->order.currency )
    free( inv->order.currency );

  /* Free xmldata */
  if( inv->xmldata )
    free( inv->xmldata );
  inv->xmldata = 0;
  memset( inv, 0, sizeof(bsxInventory) );

  return;
}


void bsxFreeInventory( bsxInventory *inv )
{
  if( inv )
  {
    bsxEmptyInventory( inv );
    free( inv );
  }
  return;
}


void bsxPackInventory( bsxInventory *inv )
{
  int srcindex;
  bsxItem *srcitem, *dstitem;

  if( inv->itemfreecount < ( inv->itemcount >> 3 ) )
    return;
  srcitem = inv->itemlist;
  dstitem = inv->itemlist;
  for( srcindex = 0 ; srcindex < inv->itemcount ; srcindex++, srcitem++ )
  {
    if( srcitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( srcitem != dstitem )
      *dstitem = *srcitem;
    dstitem++;
  }
  inv->itemcount = (int)( dstitem - inv->itemlist );
  inv->itemfreecount = 0;

  return;
}


void bsxClampNegativeInventory( bsxInventory *inv )
{
  int itemindex;
  bsxItem *item;

  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->quantity < 0 )
      bsxSetItemQuantity( inv, item, 0 );
  }

  return;
}


////


static const char bsxPrefix[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE BrickStoreXML>\n<BrickStoreXML>\n";
static const char bsxSuffix[] = "\
 <GuiState Application=\"BrickStore\" Version=\"1\" >\n\
  <ItemView>\n\
   <ColumnOrder>0,1,2,3,4,5,6,7,8,13,14,9,10,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,12,11</ColumnOrder>\n\
   <ColumnWidths>48,45,75,218,40,110,40,61,61,40,40,61,61,89,89,40,61,40,61,40,61,0,0,0,0,0,0,40,40,61,61</ColumnWidths>\n\
   <ColumnWidthsHidden>0,0,0,0,0,0,0,0,0,40,40,61,0,0,0,40,61,40,61,40,61,61,61,61,61,75,40,40,40,61,61</ColumnWidthsHidden>\n\
   <SortColumn>%d</SortColumn>\n\
   <SortDirection>%c</SortDirection>\n\
  </ItemView>\n\
 </GuiState>\n\
</BrickStoreXML>\n";


int bsxSaveInventory( char *path, bsxInventory *inv, int fsyncflag, int sortcolumn )
{
  int itemindex, retval;
  char sortdirection;
  char *encodedstring;
  bsxItem *item;
  FILE *out;

  out = fopen( path, "w" );
  if( !( out ) )
  {
    printf( "ERROR: Failed to open %s for writing\n", path );
    return 0;
  }

  errno = 0;
  retval = 1;
  if( fwrite( bsxPrefix, sizeof( bsxPrefix ) - 1, 1, out ) != 1 )
    retval = 0;

  if( inv->orderblockflag )
  {
    fprintf( out, " <Order>\n" );
    if( inv->order.service )
      fprintf( out, "  <Service>%s</Service>\n", inv->order.service );
    if( inv->order.orderid )
      fprintf( out, "  <OrderID>%d</OrderID>\n", inv->order.orderid );
    if( inv->order.orderdate )
      fprintf( out, "  <OrderDate>"CC_LLD"</OrderDate>\n", (long long)inv->order.orderdate );
    if( inv->order.customer )
      fprintf( out, "  <Customer>%s</Customer>\n", inv->order.customer );
    if( inv->order.subtotal )
      fprintf( out, "  <SubTotal>%.3f</SubTotal>\n", inv->order.subtotal );
    if( inv->order.grandtotal )
      fprintf( out, "  <GrandTotal>%.3f</GrandTotal>\n", inv->order.grandtotal );
    if( inv->order.payment )
      fprintf( out, "  <Payment>%.3f</Payment>\n", inv->order.payment );
    if( inv->order.currency )
      fprintf( out, "  <Currency>%s</Currency>\n", inv->order.currency );
    fprintf( out, " </Order>\n" );
  }

  fprintf( out, " <Inventory>\n" );
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++ )
  {
    item = &inv->itemlist[itemindex];
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    fprintf( out, "  <Item>\n" );
    fprintf( out, "   <ItemID>%s</ItemID>\n", ( item->id ? item->id : "Unknown" ) );
    if( item->typeid )
      fprintf( out, "   <ItemTypeID>%c</ItemTypeID>\n", item->typeid );
    fprintf( out, "   <ColorID>%d</ColorID>\n", item->colorid );
    if( item->name )
    {
      encodedstring = xmlEncodeEscapeString( item->name, strlen( item->name ), 0 );
      fprintf( out, "   <ItemName>%s</ItemName>\n", encodedstring );
      free( encodedstring );
    }
    if( item->typename )
      fprintf( out, "   <ItemTypeName>%s</ItemTypeName>\n", item->typename );
    if( item->colorname )
      fprintf( out, "   <ColorName>%s</ColorName>\n", item->colorname );
    if( item->categoryid )
      fprintf( out, "   <CategoryID>%d</CategoryID>\n", item->categoryid );
    if( item->categoryname )
      fprintf( out, "   <CategoryName>%s</CategoryName>\n", item->categoryname );
    fprintf( out, "   <Status>%c</Status>\n", ( item->status ? item->status : 'I' ) );
    fprintf( out, "   <Qty>%d</Qty>\n", item->quantity );
    if( item->price > 0.0001 )
      fprintf( out, "   <Price>%.3f</Price>\n", item->price );
    if( item->saleprice > 0.0001 )
      fprintf( out, "   <SalePrice>%.3f</SalePrice>\n", item->saleprice );
    if( item->bulk >= 2 )
      fprintf( out, "   <Bulk>%d</Bulk>\n", item->bulk );
    if( item->sale > 0 )
      fprintf( out, "   <Sale>%d</Sale>\n", item->sale );
    if( item->alternateid > 0 )
      fprintf( out, "   <AlternateID>%d</AlternateID>\n", item->alternateid );
    fprintf( out, "   <Condition>%c</Condition>\n", ( item->condition ? item->condition : 'N' ) );
    if( ( item->condition == 'U' ) && ( item->usedgrade ) )
      fprintf( out, "   <UsedGrade>%c</UsedGrade>\n", item->usedgrade );
    if( ( item->typeid == 'S' ) && ( item->completeness ) )
      fprintf( out, "   <Completeness>%c</Completeness>\n", item->completeness );
    if( item->origprice > 0.0001 )
      fprintf( out, "   <OrigPrice>%f</OrigPrice>\n", item->origprice );
    if( item->comments )
    {
      encodedstring = xmlEncodeEscapeString( item->comments, strlen( item->comments ), 0 );
      fprintf( out, "   <Comments>%s</Comments>\n", encodedstring );
      free( encodedstring );
    }
    if( item->remarks )
    {
      encodedstring = xmlEncodeEscapeString( item->remarks, strlen( item->remarks ), 0 );
      fprintf( out, "   <Remarks>%s</Remarks>\n", encodedstring );
      free( encodedstring );
    }
    if( item->origquantity )
      fprintf( out, "   <OrigQty>%d</OrigQty>\n", item->origquantity );
    if( item->mycost > 0.0001 )
      fprintf( out, "   <MyCost>%.3f</MyCost>\n", item->mycost );
    if( item->tq1 )
    {
      fprintf( out, "   <TQ1>%d</TQ1>\n", item->tq1 );
      fprintf( out, "   <TP1>%.3f</TP1>\n", item->tp1 );
    }
    if( item->tq2 )
    {
      fprintf( out, "   <TQ2>%d</TQ2>\n", item->tq2 );
      fprintf( out, "   <TP2>%.3f</TP2>\n", item->tp2 );
    }
    if( item->tq3 )
    {
      fprintf( out, "   <TQ3>%d</TQ3>\n", item->tq3 );
      fprintf( out, "   <TP3>%.3f</TP3>\n", item->tp3 );
    }
    if( item->lotid != -1 )
      fprintf( out, "   <LotID>"CC_LLD"</LotID>\n", (long long)item->lotid );
    if( item->boid != -1 )
      fprintf( out, "   <OwlID>"CC_LLD"</OwlID>\n", (long long)item->boid );
    if( item->bolotid != -1 )
      fprintf( out, "   <OwlLotID>"CC_LLD"</OwlLotID>\n", (long long)item->bolotid );
    fprintf( out, "  </Item>\n" );
  }
  fprintf( out, " </Inventory>\n" );

  if( sortcolumn == 0 )
    sortcolumn = 8;
  sortdirection = 'D';
  if( sortcolumn < 0 )
  {
    sortcolumn = -sortcolumn;
    sortdirection = 'A';
  }
  fprintf( out, bsxSuffix, (int)sortcolumn, (char)sortdirection );
  if( fflush( out ) != 0 )
    retval = 0;
  if( fsyncflag )
  {
#if CC_LINUX
    fdatasync( fileno( out ) );
#elif CC_UNIX
    fsync( fileno( out ) );
#elif CC_WINDOWS
    FlushFileBuffers( (HANDLE)_get_osfhandle( fileno( out ) ) );
#endif
  }
  if( fclose( out ) != 0 )
    retval = 0;

  if( errno == ENOSPC )
    return 0;

  return retval;
}


////


typedef struct
{
  size_t offset;
  int reverseflag;
} bsxSortContext;

static inline int bsxSortCmpString( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int i;
  char *s0, *s1;
  s0 = *(char **)ADDRESS( item0, sortcontext->offset );
  s1 = *(char **)ADDRESS( item1, sortcontext->offset );
  if( !( s0 ) )
  {
    if( !( s1 ) )
      return sortcontext->reverseflag ^ ( item1 < item0 );
    return 1;
  }
  else if( !( s1 ) )
    return 0;
  for( i = 0 ; ; i++ )
  {
    if( s0[i] != s1[i] )
      break;
  }
  return sortcontext->reverseflag ^ ( s1[i] < s0[i] );
}

static inline int bsxSortCmpInteger( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int *i0, *i1;
  i0 = ADDRESS( item0, sortcontext->offset );
  i1 = ADDRESS( item1, sortcontext->offset );
  return sortcontext->reverseflag ^ ( *i1 < *i0 );
}

static inline int bsxSortCmpInt64( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int64_t *i0, *i1;
  i0 = ADDRESS( item0, sortcontext->offset );
  i1 = ADDRESS( item1, sortcontext->offset );
  return sortcontext->reverseflag ^ ( *i1 < *i0 );
}

static inline int bsxSortCmpFloat( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  float *f0, *f1;
  f0 = ADDRESS( item0, sortcontext->offset );
  f1 = ADDRESS( item1, sortcontext->offset );
  return sortcontext->reverseflag ^ ( *f1 < *f0 );
}

static int bsxSortCmpIdColorConditionRemarksCommentsQuantity( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int cmpvalue;

  cmpvalue = ccStrCmpStdTest( item0->id, item1->id );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  if( item0->colorid < item1->colorid )
    return 0;
  else if( item0->colorid > item1->colorid )
    return 1;

  if( item0->condition < item1->condition )
    return 0;
  if( item0->condition > item1->condition )
    return 1;

  cmpvalue = ccStrCmpStdTest( item0->remarks, item1->remarks );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  cmpvalue = ccStrCmpStdTest( item0->comments, item1->comments );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  return ( item0->quantity < item1->quantity );
}

static int bsxSortCmpColornameNameCondition( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int cmpvalue;

  cmpvalue = ccStrCmpStdTest( item0->colorname, item1->colorname );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  cmpvalue = ccStrCmpStdTest( item0->name, item1->name );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  return ( item0->condition > item1->condition );
}

static int bsxSortCmpUpdatePriority( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int value0, value1;
  int createflag, deleteflag, updateflag;

  value0 = item0->flags & BSX_ITEM_XFLAGS_TO_CREATE;
  value1 = item1->flags & BSX_ITEM_XFLAGS_TO_CREATE;
  if( value0 != value1 )
    return ( value0 < value1 ? 0 : 1 );
  createflag = value0 | value1;

  value0 = item0->flags & BSX_ITEM_XFLAGS_TO_DELETE;
  value1 = item1->flags & BSX_ITEM_XFLAGS_TO_DELETE;
  if( value0 != value1 )
    return ( value0 < value1 ? 0 : 1 );
  deleteflag = value0 | value1;
  updateflag = 1 ^ ( createflag | deleteflag );

  if( updateflag )
  {
    value0 = 0;
    value1 = 0;
    if( item0->flags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
      value0 += 0x8;
    if( item0->flags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
      value0 += 0x4;
    if( item0->flags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
      value0 += 0x2;
    if( item0->flags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
      value0 += 0x1;
    if( item1->flags & BSX_ITEM_XFLAGS_UPDATE_REMARKS )
      value1 += 0x8;
    if( item1->flags & BSX_ITEM_XFLAGS_UPDATE_QUANTITY )
      value1 += 0x4;
    if( item1->flags & BSX_ITEM_XFLAGS_UPDATE_PRICE )
      value1 += 0x2;
    if( item1->flags & BSX_ITEM_XFLAGS_UPDATE_COMMENTS )
      value1 += 0x1;
    if( value0 != value1 )
      return ( value0 < value1 ? 0 : 1 );
  }

  /* Sort by price difference magnitude */
  return ( fabsf( item0->price * (float)item0->quantity ) > fabsf( item1->price * (float)item1->quantity ) ? 0 : 1 );
}

static int bsxSortCmpCheckListOrder( bsxSortContext *sortcontext, bsxItem *item0, bsxItem *item1 )
{
  int cmpvalue, s0len, s1len;
  char *str;

  s0len = 0;
  if( item0->remarks )
    for( str = item0->remarks ; ccIsAlphaNum( *str ) ; str++, s0len++ );
  s1len = 0;
  if( item1->remarks )
    for( str = item1->remarks ; ccIsAlphaNum( *str ) ; str++, s1len++ );
  cmpvalue = ccSeqCmpSeqStdTest( item0->remarks, item1->remarks, s0len, s1len );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  cmpvalue = ccStrCmpStdTest( item0->colorname, item1->colorname );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  cmpvalue = ccStrCmpStdTest( item0->name, item1->name );
  if( cmpvalue < 0 )
    return 0;
  else if( cmpvalue > 0 )
    return 1;

  return ( item0->condition < item1->condition );
}


#define HSORT_MAIN bsxSortString
#define HSORT_CMP bsxSortCmpString
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortInteger
#define HSORT_CMP bsxSortCmpInteger
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortInt64
#define HSORT_CMP bsxSortCmpInt64
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortFloat
#define HSORT_CMP bsxSortCmpFloat
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortIdColorConditionRemarksCommentsQuantity
#define HSORT_CMP bsxSortCmpIdColorConditionRemarksCommentsQuantity
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortColornameNameCondition
#define HSORT_CMP bsxSortCmpColornameNameCondition
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortUpdatePriority
#define HSORT_CMP bsxSortCmpUpdatePriority
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT

#define HSORT_MAIN bsxSortCheckListOrder
#define HSORT_CMP bsxSortCmpCheckListOrder
#define HSORT_TYPE bsxItem
#define HSORT_CONTEXT bsxSortContext *
#include "cchybridsort.h"
#undef HSORT_MAIN
#undef HSORT_CMP
#undef HSORT_TYPE
#undef HSORT_CONTEXT


int bsxSortInventory( bsxInventory *inv, int sortfield, int reverseflag )
{
  int retvalue;
  bsxItem *tmp;
  bsxSortContext sortcontext;

  if( inv->itemcount <= 1 )
    return 1;

  retvalue = 1;
  tmp = malloc( inv->itemcount * sizeof(bsxItem) );
  sortcontext.reverseflag = reverseflag;
  switch( sortfield )
  {
    case BSX_SORT_ID:
      sortcontext.offset = offsetof(bsxItem,id);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_NAME:
      sortcontext.offset = offsetof(bsxItem,name);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_TYPENAME:
      sortcontext.offset = offsetof(bsxItem,typename);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_COLORNAME:
      sortcontext.offset = offsetof(bsxItem,colorname);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_CATEGORYNAME:
      sortcontext.offset = offsetof(bsxItem,categoryname);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_COLORID:
      sortcontext.offset = offsetof(bsxItem,colorid);
      bsxSortInteger( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_QUANTITY:
      sortcontext.offset = offsetof(bsxItem,quantity);
      bsxSortInteger( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_PRICE:
      sortcontext.offset = offsetof(bsxItem,price);
      bsxSortFloat( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_ORIGPRICE:
      sortcontext.offset = offsetof(bsxItem,origprice);
      bsxSortFloat( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_COMMENTS:
      sortcontext.offset = offsetof(bsxItem,comments);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_REMARKS:
      sortcontext.offset = offsetof(bsxItem,remarks);
      bsxSortString( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_LOTID:
      sortcontext.offset = offsetof(bsxItem,lotid);
      bsxSortInt64( inv->itemlist, tmp, inv->itemcount, &sortcontext, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_ID_COLOR_CONDITION_REMARKS_COMMENTS_QUANTITY:
      bsxSortIdColorConditionRemarksCommentsQuantity( inv->itemlist, tmp, inv->itemcount, 0, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_COLORNAME_NAME_CONDITION:
      bsxSortColornameNameCondition( inv->itemlist, tmp, inv->itemcount, 0, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_UPDATE_PRIORITY:
      bsxSortUpdatePriority( inv->itemlist, tmp, inv->itemcount, 0, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    case BSX_SORT_CHECK_LIST_ORDER:
      bsxSortCheckListOrder( inv->itemlist, tmp, inv->itemcount, 0, (uint32_t)(((uintptr_t)inv)>>8) );
      break;
    default:
      retvalue = 0;
      break;
  }

  free( tmp );

  return retvalue;
}


////


static inline int bsxStrCmpEqualInline( char *s0, char *s1 )
{
  int i;
  if( !( s0 ) || !( s1 ) )
    return 0;
  for( i = 0 ; ; i++ )
  {
    if( s0[i] != s1[i] )
      return 0;
    if( !( s0[i] ) )
      break;
  }
  return 1;
}

/* Find item that matches ID, typeID, colorID and condition */
bsxItem *bsxFindMatchItem( bsxInventory *inv, bsxItem *matchitem )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( !( bsxStrCmpEqualInline( item->id, matchitem->id ) ) )
      continue;
    if( item->typeid != matchitem->typeid )
      continue;
    if( item->colorid != matchitem->colorid )
      continue;
    if( item->condition != matchitem->condition )
      continue;
    return item;
  }

  return 0;
}

/* Find the index of the item that matches ID, typeID, colorID and condition */
int bsxFindMatchItemIndex( bsxInventory *inv, bsxItem *matchitem )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( !( ccStrCmpEqual( item->id, matchitem->id ) ) )
      continue;
    if( item->typeid != matchitem->typeid )
      continue;
    if( item->colorid != matchitem->colorid )
      continue;
    if( item->condition != matchitem->condition )
      continue;
    return itemindex;
  }

  return -1;
}

/* Find item by typeID, ID, colorID and condition */
bsxItem *bsxFindItem( bsxInventory *inv, char typeid, char *id, int colorid, char condition )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( !( bsxStrCmpEqualInline( item->id, id ) ) )
      continue;
    if( item->typeid != typeid )
      continue;
    if( item->colorid != colorid )
      continue;
    if( item->condition != condition )
      continue;
    return item;
  }

  return 0;
}

/* Find item by ID, colorID and condition */
bsxItem *bsxFindItemNoType( bsxInventory *inv, char *id, int colorid, char condition )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( !( bsxStrCmpEqualInline( item->id, id ) ) )
      continue;
    if( item->colorid != colorid )
      continue;
    if( item->condition != condition )
      continue;
    return item;
  }

  return 0;
}

/* Find item by BOID, colorID and condition */
bsxItem *bsxFindItemBOID( bsxInventory *inv, int64_t boid, int colorid, char condition )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->boid != boid )
      continue;
    if( item->colorid != colorid )
      continue;
    if( item->condition != condition )
      continue;
    return item;
  }

  return 0;
}

/* Find item that matches LotID */
bsxItem *bsxFindLotID( bsxInventory *inv, int64_t lotid )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  if( lotid == -1 )
    return 0;
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->lotid != lotid )
      continue;
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    return item;
  }

  return 0;
}

/* Find item that matches BOID && color && LotID */
bsxItem *bsxFindBoidColorConditionLotID( bsxInventory *inv, int64_t boid, int colorid, char condition, int64_t lotid )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  if( lotid == -1 )
    return 0;
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->lotid != lotid )
      continue;
    if( item->boid != boid )
      continue;
    if( item->colorid != colorid )
      continue;
    if( item->condition != condition )
      continue;
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    return item;
  }

  return 0;
}

/* Find item that matches OwlLotID */
bsxItem *bsxFindOwlLotID( bsxInventory *inv, int64_t bolotid )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  if( bolotid == -1 )
    return 0;
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->bolotid != bolotid )
      continue;
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    return item;
  }

  return 0;
}

/* Find item that matches extID */
bsxItem *bsxFindExtID( bsxInventory *inv, int64_t extid )
{
  int itemindex;
  bsxItem *item;

  /* TODO: We really need a hash based search! */
  if( extid == -1 )
    return 0;
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->extid != extid )
      continue;
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    return item;
  }

  return 0;
}


////


int bsxAddInventory( bsxInventory *dstinv, bsxInventory *srcinv )
{
  int srcindex;
  bsxItem *srcitem, *dstitem;

  srcitem = srcinv->itemlist;
  for( srcindex = 0 ; srcindex < srcinv->itemcount ; srcindex++, srcitem++ )
  {
    if( srcitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    dstitem = 0;
    if( srcitem->lotid != -1 )
      dstitem = bsxFindLotID( dstinv, srcitem->lotid );
    if( !( dstitem ) )
      dstitem = bsxFindMatchItem( dstinv, srcitem );
    if( !( dstitem ) )
      dstitem = bsxAddCopyItem( dstinv, srcitem );
    else
      bsxSetItemQuantity( dstinv, dstitem, dstitem->quantity + srcitem->quantity );
    if( !( dstitem->quantity ) )
      bsxRemoveItem( dstinv, dstitem );
  }

  return 1;
}


int bsxSubInventory( bsxInventory *dstinv, bsxInventory *srcinv )
{
  int srcindex;
  bsxItem *srcitem, *dstitem;

  srcitem = srcinv->itemlist;
  for( srcindex = 0 ; srcindex < srcinv->itemcount ; srcindex++, srcitem++ )
  {
    if( srcitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    dstitem = 0;
    if( srcitem->lotid != -1 )
      dstitem = bsxFindLotID( dstinv, srcitem->lotid );
    if( !( dstitem ) )
      dstitem = bsxFindMatchItem( dstinv, srcitem );
    if( !( dstitem ) )
    {
      dstitem = bsxAddCopyItem( dstinv, srcitem );
      bsxSetItemQuantity( dstinv, dstitem, -dstitem->quantity );
    }
    else
      bsxSetItemQuantity( dstinv, dstitem, dstitem->quantity - srcitem->quantity );
    if( !( dstitem->quantity ) )
      bsxRemoveItem( dstinv, dstitem );
  }

  return 1;
}


/* Returns a difference inventory as ( dstinv - srcinv ) */
bsxInventory *bsxDiffInventory( bsxInventory *dstinv, bsxInventory *srcinv )
{
  int srcindex, dstindex, partcount;
  size_t bitindex;
  bsxInventory *diffinv;
  bsxItem *srcitem, *dstitem, *diffitem;
  mmBitMap stockmap;

  if( !( mmBitMapInit( &stockmap, dstinv->itemcount, 0 ) ) )
    return 0;
  diffinv = bsxNewInventory();
  partcount = 0;
  srcitem = srcinv->itemlist;
  for( srcindex = 0 ; srcindex < srcinv->itemcount ; srcindex++, srcitem++ )
  {
    if( srcitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    dstitem = 0;
    if( srcitem->lotid != -1 )
      dstitem = bsxFindLotID( dstinv, srcitem->lotid );
    if( !( dstitem ) )
      dstitem = bsxFindMatchItem( dstinv, srcitem );
    if( !( dstitem ) )
    {
      diffitem = bsxAddCopyItem( diffinv, srcitem );
      diffitem->quantity = -diffitem->quantity;
      partcount += diffitem->quantity;
    }
    else
    {
      dstindex = bsxGetItemListIndex( dstinv, dstitem );
      mmBitMapDirectSet( &stockmap, dstindex );
      if( srcitem->quantity != dstitem->quantity )
      {
        diffitem = bsxAddCopyItem( diffinv, srcitem );
        diffitem->quantity = dstitem->quantity - srcitem->quantity;
        partcount += diffitem->quantity;
      }
    }
  }
  for( dstindex = 0 ; ; )
  {
    if( !( mmBitMapFindClear( &stockmap, dstindex, dstinv->itemcount - 1, &bitindex ) ) )
      break;
    dstindex = (int)bitindex;
    mmBitMapDirectSet( &stockmap, dstindex );
    dstitem = &dstinv->itemlist[ dstindex ];
    if( dstitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    diffitem = bsxAddCopyItem( diffinv, dstitem );
    partcount += diffitem->quantity;
  }
  mmBitMapFree( &stockmap );

  diffinv->partcount = partcount;
  diffinv->totalprice = 0.0;
  diffinv->totalorigprice = 0.0;

  return diffinv;
}


/* Returns a difference inventory as ( dstinv - srcinv ) */
bsxInventory *bsxDiffInventoryByLotID( bsxInventory *dstinv, bsxInventory *srcinv )
{
  int srcindex, dstindex, partcount;
  size_t bitindex;
  bsxInventory *diffinv;
  bsxItem *srcitem, *dstitem, *diffitem;
  mmBitMap stockmap;

  if( !( mmBitMapInit( &stockmap, dstinv->itemcount, 0 ) ) )
    return 0;
  diffinv = bsxNewInventory();
  partcount = 0;
  srcitem = srcinv->itemlist;
  for( srcindex = 0 ; srcindex < srcinv->itemcount ; srcindex++, srcitem++ )
  {
    if( srcitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( srcitem->lotid == -1 )
      continue;
    dstitem = bsxFindLotID( dstinv, srcitem->lotid );
    if( !( dstitem ) )
    {
      diffitem = bsxAddCopyItem( diffinv, srcitem );
      diffitem->quantity = -diffitem->quantity;
      partcount += diffitem->quantity;
    }
    else
    {
      dstindex = bsxGetItemListIndex( dstinv, dstitem );
      mmBitMapDirectSet( &stockmap, dstindex );
      if( srcitem->quantity != dstitem->quantity )
      {
        diffitem = bsxAddCopyItem( diffinv, srcitem );
        diffitem->quantity = dstitem->quantity - srcitem->quantity;
        partcount += diffitem->quantity;
      }
    }
  }
  for( dstindex = 0 ; ; )
  {
    if( !( mmBitMapFindClear( &stockmap, dstindex, dstinv->itemcount - 1, &bitindex ) ) )
      break;
    dstindex = (int)bitindex;
    mmBitMapDirectSet( &stockmap, dstindex );
    dstitem = &dstinv->itemlist[ dstindex ];
    if( dstitem->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    diffitem = bsxAddCopyItem( diffinv, dstitem );
    partcount += diffitem->quantity;
  }
  mmBitMapFree( &stockmap );

  diffinv->partcount = partcount;
  diffinv->totalprice = 0.0;
  diffinv->totalorigprice = 0.0;

  return diffinv;
}


void bsxInvertQuantities( bsxInventory *inv )
{
  int itemindex;
  bsxItem *item;
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
    item->quantity = -item->quantity;
  return;
}


/* Recompute partcount and totalprice */
void bsxRecomputeTotals( bsxInventory *inv )
{
  int itemindex, partcount;
  double totalprice;
  bsxItem *item;

  partcount = 0;
  totalprice = 0.0;
  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    partcount += item->quantity;
    totalprice += (double)item->quantity * (double)item->price;
  }
#if 0
  if( inv->partcount != partcount )
  {
    printf( "WARNING: bsxCheckQuantities() mismatch\n" );
    printf( "WARNING: Tracked partcount : %d\n", inv->partcount );
    printf( "WARNING: Real partcount    : %d\n", partcount );
  }
#endif
  inv->partcount = partcount;
  inv->totalprice = totalprice;
  return;
}


/* Remove all items with a quantity of zero */
void bsxRemoveEmptyItems( bsxInventory *inv )
{
  int itemindex;
  bsxItem *item;

  item = inv->itemlist;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->quantity == 0 )
      bsxRemoveItem( inv, item );
  }
  bsxPackInventory( inv );

  return;
}


/* Merge quantities for matching items */
void bsxConsolidateInventoryByMatch( bsxInventory *inv )
{
  int itemindex;
  bsxItem *item, *matchitem;

  bsxSortInventory( inv, BSX_SORT_ID_COLOR_CONDITION_REMARKS_COMMENTS_QUANTITY, 0 );
  item = inv->itemlist;
  matchitem = 0;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( ( matchitem ) && ( bsxStrCmpEqualInline( item->id, matchitem->id ) ) && ( item->typeid == matchitem->typeid ) && ( item->colorid == matchitem->colorid ) && ( item->condition == matchitem->condition ) )
    {
      bsxSetItemQuantity( inv, matchitem, matchitem->quantity + item->quantity );
      bsxRemoveItem( inv, item );
      matchitem->status = 'I';
    }
    else
      matchitem = item;
  }
  bsxPackInventory( inv );

  return;
}


/* Merge quantities for items sharing the same lotID */
void bsxConsolidateInventoryByLotID( bsxInventory *inv )
{
  int itemindex;
  bsxItem *item, *lotiditem;
  int64_t lotid;

  bsxSortInventory( inv, BSX_SORT_LOTID, 0 );
  item = inv->itemlist;
  lotiditem = 0;
  lotid = -1;
  for( itemindex = 0 ; itemindex < inv->itemcount ; itemindex++, item++ )
  {
    if( item->flags & BSX_ITEM_FLAGS_DELETED )
      continue;
    if( item->lotid == -1 )
      continue;
    if( item->lotid == lotid )
    {
      bsxSetItemQuantity( inv, lotiditem, lotiditem->quantity + item->quantity );
      bsxRemoveItem( inv, item );
      lotiditem->status = 'I';
    }
    else
    {
      lotid = item->lotid;
      lotiditem = item;
    }
  }
  bsxPackInventory( inv );

  return;
}



////


/* Import all LotIDs from inv to stockinv for matching items */
int bsxImportLotIDs( bsxInventory *dstinv, bsxInventory *srcinv )
{
  int itemindex, count;
  bsxItem *srcitem, *dstitem;

  count = 0;
  for( itemindex = 0 ; itemindex < srcinv->itemcount ; itemindex++ )
  {
    srcitem = &srcinv->itemlist[ itemindex ];
    if( srcitem->lotid == -1 )
      continue;
    dstitem = bsxFindExtID( dstinv, srcitem->extid );
#if 0
    if( !( dstitem ) )
      dstitem = bsxFindMatchItem( dstinv, srcitem );
#endif
    if( !( dstitem ) )
      continue;
    if( dstitem->lotid != -1 )
      continue;
    dstitem->lotid = srcitem->lotid;
    count++;
  }

  return count;
}


/* Import all OwlLotIDs from inv to stockinv for matching items */
int bsxImportOwlLotIDs( bsxInventory *dstinv, bsxInventory *srcinv )
{
  int itemindex, count;
  bsxItem *srcitem, *dstitem;

  count = 0;
  for( itemindex = 0 ; itemindex < srcinv->itemcount ; itemindex++ )
  {
    srcitem = &srcinv->itemlist[ itemindex ];
    if( srcitem->bolotid == -1 )
      continue;
    dstitem = bsxFindExtID( dstinv, srcitem->extid );
    if( !( dstitem ) )
      dstitem = bsxFindLotID( dstinv, srcitem->lotid );
#if 0
    if( !( dstitem ) )
      dstitem = bsxFindMatchItem( dstinv, srcitem );
#endif
    if( !( dstitem ) )
      continue;
    if( dstitem->bolotid != -1 )
      continue;
    dstitem->bolotid = srcitem->bolotid;
    count++;
  }

  return count;
}


////


bsxItem *bsxNewItem( bsxInventory *inv )
{
  bsxItem *item;
  if( inv->itemcount >= inv->itemalloc )
  {
    inv->itemalloc = intMax( 16384, inv->itemalloc << 1 );
    inv->itemlist = realloc( inv->itemlist, inv->itemalloc * sizeof(bsxItem) );
  }
  item = &inv->itemlist[ inv->itemcount ];
  bsxClearItem( item );

  inv->itemcount++;
  return item;
}

static void bsxAddItemCopyString( bsxItem *item, char **dst, char *src, int allocflag )
{
  int len;
  len = strlen( src ) + 1;
  *dst = malloc( len );
  memcpy( *dst, src, len );
  item->flags |= allocflag;
  return;
}

bsxItem *bsxAddItem( bsxInventory *inv, bsxItem *itemref )
{
  bsxItem *item;
  if( inv->itemcount >= inv->itemalloc )
  {
    inv->itemalloc = intMax( 16384, inv->itemalloc << 1 );
    inv->itemlist = realloc( inv->itemlist, inv->itemalloc * sizeof(bsxItem) );
  }
  item = &inv->itemlist[ inv->itemcount ];
  memcpy( item, itemref, sizeof(bsxItem) );
  item->flags = 0x0;
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_ID )
    bsxAddItemCopyString( item, &item->id, itemref->id, BSX_ITEM_FLAGS_ALLOC_ID );
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_NAME )
    bsxAddItemCopyString( item, &item->name, itemref->name, BSX_ITEM_FLAGS_ALLOC_NAME );
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_TYPENAME )
    bsxAddItemCopyString( item, &item->typename, itemref->typename, BSX_ITEM_FLAGS_ALLOC_TYPENAME );
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_COLORNAME )
    bsxAddItemCopyString( item, &item->colorname, itemref->colorname, BSX_ITEM_FLAGS_ALLOC_COLORNAME );
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_CATEGORYNAME )
    bsxAddItemCopyString( item, &item->categoryname, itemref->categoryname, BSX_ITEM_FLAGS_ALLOC_CATEGORYNAME );
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_COMMENTS )
    bsxAddItemCopyString( item, &item->comments, itemref->comments, BSX_ITEM_FLAGS_ALLOC_COMMENTS );
  if( itemref->flags & BSX_ITEM_FLAGS_ALLOC_REMARKS )
    bsxAddItemCopyString( item, &item->remarks, itemref->remarks, BSX_ITEM_FLAGS_ALLOC_REMARKS );
  inv->itemcount++;
  inv->partcount += item->quantity;
  inv->totalprice += (double)item->quantity * (double)item->price;
  inv->totalorigprice += (double)item->quantity * (double)item->origprice;
  return item;
}

bsxItem *bsxAddCopyItem( bsxInventory *inv, bsxItem *itemref )
{
  bsxItem *item;
  if( inv->itemcount >= inv->itemalloc )
  {
    inv->itemalloc = intMax( 16384, inv->itemalloc << 1 );
    inv->itemlist = realloc( inv->itemlist, inv->itemalloc * sizeof(bsxItem) );
  }
  item = &inv->itemlist[ inv->itemcount ];
  memcpy( item, itemref, sizeof(bsxItem) );
  item->flags = 0x0;
  if( itemref->id )
    bsxAddItemCopyString( item, &item->id, itemref->id, BSX_ITEM_FLAGS_ALLOC_ID );
  if( itemref->name )
    bsxAddItemCopyString( item, &item->name, itemref->name, BSX_ITEM_FLAGS_ALLOC_NAME );
  if( itemref->typename )
    bsxAddItemCopyString( item, &item->typename, itemref->typename, BSX_ITEM_FLAGS_ALLOC_TYPENAME );
  if( itemref->colorname )
    bsxAddItemCopyString( item, &item->colorname, itemref->colorname, BSX_ITEM_FLAGS_ALLOC_COLORNAME );
  if( itemref->categoryname )
    bsxAddItemCopyString( item, &item->categoryname, itemref->categoryname, BSX_ITEM_FLAGS_ALLOC_CATEGORYNAME );
  if( itemref->comments )
    bsxAddItemCopyString( item, &item->comments, itemref->comments, BSX_ITEM_FLAGS_ALLOC_COMMENTS );
  if( itemref->remarks )
    bsxAddItemCopyString( item, &item->remarks, itemref->remarks, BSX_ITEM_FLAGS_ALLOC_REMARKS );
  inv->itemcount++;
  inv->partcount += item->quantity;
  inv->totalprice += (double)item->quantity * (double)item->price;
  inv->totalorigprice += (double)item->quantity * (double)item->origprice;
  return item;
}


void bsxRemoveItem( bsxInventory *inv, bsxItem *item )
{
  inv->partcount -= item->quantity;
  inv->totalprice -= (double)item->quantity * (double)item->price;
  inv->totalorigprice -= (double)item->quantity * (double)item->origprice;
  bsxFreeItem( item, 1 );
  inv->itemfreecount++;
  return;
}


static void bsxSetItemString( bsxItem *item, size_t offset, char *string, int len, int flag )
{
  char *dst, **storage;
  storage = ADDRESS( item, offset );
  if( item->flags & flag )
  {
    free( *storage );
    item->flags &= ~flag;
  }
  *storage = 0;
  if( !( string ) || !( len ) )
    return;
  if( len < 0 )
  {
    len = strlen( string );
    if( len <= 0 )
      return;
  }
#if 1
  if( len > 255 )
    len = 255;
#endif
  dst = malloc( len + 1 );
  memcpy( dst, string, len );
  dst[ len ] = 0;
  *storage = dst;
  item->flags |= flag;
  return;
}


void bsxSetItemId( bsxItem *item, char *id, int len )
{
  bsxSetItemString( item, offsetof(bsxItem,id), id, len, BSX_ITEM_FLAGS_ALLOC_ID );
  return;
}

void bsxSetItemName( bsxItem *item, char *name, int len )
{
  int i;
  bsxSetItemString( item, offsetof(bsxItem,name), name, len, BSX_ITEM_FLAGS_ALLOC_NAME );
#if 0
  /* Correction for '&' crapping BrickStore */
  if( item->name )
  {
    for( i = 0 ; item->name[i] ; i++ )
    {
      if( item->name[i] == '&' )
        item->name[i] = ' ';
    }
  }
#endif
  return;
}

void bsxSetItemTypeName( bsxItem *item, char *typename, int len )
{
  bsxSetItemString( item, offsetof(bsxItem,typename), typename, len, BSX_ITEM_FLAGS_ALLOC_TYPENAME );
  return;
}

void bsxSetItemColorName( bsxItem *item, char *colorname, int len )
{
  bsxSetItemString( item, offsetof(bsxItem,colorname), colorname, len, BSX_ITEM_FLAGS_ALLOC_COLORNAME );
  return;
}

void bsxSetItemComments( bsxItem *item, char *comments, int len )
{
  char *trimmed_comments = ccStrTrimWhitespace(comments);
  bsxSetItemString( item, offsetof(bsxItem,comments), trimmed_comments, -1, BSX_ITEM_FLAGS_ALLOC_COMMENTS );
  free(trimmed_comments);
  return;
}

void bsxSetItemRemarks( bsxItem *item, char *remarks, int len )
{
  char *trimmed_remarks = ccStrTrimWhitespace(remarks);
  bsxSetItemString( item, offsetof(bsxItem,remarks), trimmed_remarks, -1, BSX_ITEM_FLAGS_ALLOC_REMARKS );
  free(trimmed_remarks);
  return;
}

void bsxSetItemQuantity( bsxInventory *inv, bsxItem *item, int quantity )
{
  int delta;
  delta = quantity - item->quantity;
  inv->partcount += delta;
  inv->totalprice += (double)delta * (double)item->price;
  inv->totalorigprice += (double)delta * (double)item->origprice;
  item->quantity = quantity;
  return;
}


size_t bsxGetItemListIndex( bsxInventory *inv, bsxItem *item )
{
  size_t index;
  index = item - inv->itemlist;
  return index;
}


////


static inline float bsxVerifyItemRoundPrice( double price )
{
  price = round( price * 1000.0 ) / 1000.0;
  return (float)price;
}

static inline void bsxVerifyItemTierPrices( bsxItem *item )
{
  int baseqty;
  double baseprice;

  baseqty = 0;
  baseprice = (double)item->price - 0.0005;
  for( ; ; )
  {
    if( item->tq1 <= baseqty )
    {
      item->tq1 = 0;
      item->tq2 = 0;
      item->tq3 = 0;
      item->tp1 = 0.0;
      item->tp2 = 0.0;
      item->tp3 = 0.0;
      return;
    }
    else if( (double)item->tp1 >= baseprice )
    {
      item->tq1 = item->tq2;
      item->tq2 = item->tq3;
      item->tq3 = 0;
      item->tp1 = item->tp2;
      item->tp2 = item->tp3;
      item->tp3 = 0.0;
    }
    else
      break;
  }

  baseqty = item->tq1;
  baseprice = (double)item->tp1 - 0.0005;
  for( ; ; )
  {
    if( item->tq2 <= baseqty )
    {
      item->tq2 = 0;
      item->tq3 = 0;
      item->tp2 = 0.0;
      item->tp3 = 0.0;
      return;
    }
    else if( (double)item->tp2 >= baseprice )
    {
      item->tq2 = item->tq3;
      item->tq3 = 0;
      item->tp2 = item->tp3;
      item->tp3 = 0.0;
    }
    else
      break;
  }

  baseqty = item->tq2;
  baseprice = (double)item->tp2 - 0.0005;
  for( ; ; )
  {
    if( item->tq3 <= baseqty )
    {
      item->tq3 = 0;
      item->tp3 = 0.0;
      return;
    }
    else if( (double)item->tp3 >= baseprice )
    {
      item->tq3 = 0;
      item->tp3 = 0.0;
    }
    else
      break;
  }

  return;
}


void bsxVerifyItem( bsxItem *item )
{
  item->price = bsxVerifyItemRoundPrice( item->price );
  item->tp1 = bsxVerifyItemRoundPrice( item->tp1 );
  item->tp2 = bsxVerifyItemRoundPrice( item->tp2 );
  item->tp3 = bsxVerifyItemRoundPrice( item->tp3 );
  bsxVerifyItemTierPrices( item );
  if( ( item->condition != 'N' ) && ( item->condition != 'U' ) )
    item->condition = 'N';
  if( item->price < 0.001 )
    item->price = 0.001;
  return;
}


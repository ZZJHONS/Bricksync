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

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"
#include "mm.h"
#include "iolog.h"
#include "debugtrack.h"

#include "bsx.h"
#include "bsxpg.h"
#include "tcp.h"

#include "json.h"
#include "bsorder.h"
#include "brickowl.h"


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


#define BO_PARSER_DEBUG (0)


////


static inline int boParseCheckName( char *name, int namelen, const char * const ref )
{
  return ( ( namelen == strlen( ref ) ) && ( ccMemCmpInline( name, (void *)ref, namelen ) ) );
}


////


/*
GET /v1/order/list?key=%s
[{"order_id":"9277759","order_date":"1398955037","total_quantity":"105","total_lots":"25","base_order_total":"23.87","status":"Processing","status_id":"3"},{"order_id":"3957411","order_date":"1398954091","total_quantity":"383","total_lots":"22","base_order_total":"29.80","status":"Processing","status_id":"3"},{"order_id":"3506238","order_date":"1398775922","total_quantity":"83","total_lots":"21","base_order_total":"8.47","status":"Shipped","status_id":"5"}]

GET /v1/order/view?key=%s&order_id=3506238
{"order_id":"3506238","order_time":"1398775922","processed_time":null,"iso_order_time":"2014-04-29T13:52:02+01:00","iso_processed_time":"1970-01-01T01:00:00+01:00","store_id":"907","ship_method_name":"Canada Post, Oversized Mail, Bubble Mailer","status":"Processing","status_id":"3","weight":"47.020","ship_total":"2.30","buyer_note":"","total_quantity":"83","total_lots":"21","base_currency":"USD","payment_method_type":"paypal_standard","payment_currency":"CAD","payment_total":"9.31","base_order_total":"8.47","sub_total":"6.17","coupon_discount":"0.00","payment_method_note":"","payment_transaction_id":"7GC86292WS525740F","tax_rate":"0.00","tax_amount":"0.00","tracking_number":null,"tracking_advice":"","buyer_name":"Francois Angers","combine_with":null,"refund_shipping":"0.00","refund_adjustment":"0.00","refund_subtotal":"0.00","refund_total":"0.00","refund_note":null,"affiliate_fee":0.15425,"brickowl_fee":0.15425,"seller_note":null,"ship_first_name":"Francois","ship_last_name":"Angers","ship_country_code":"CA","ship_country":"Canada","ship_post_code":"G1G1R3","ship_street_1":"4144 des Martinets","ship_street_2":"","ship_city":"Quebec","ship_region":"QC","ship_phone":""}

GET /v1/order/items?key=%s&order_id=3506238
[
{"image_small":"\/\/img.brickowl.com\/files\/image_cache\/small\/lego-black-tile-1-x-2-with-groove-3069-30-487300-38.png","condition":"New","name":"LEGO Black Tile 1 x 2 with Groove (3069)","boid":"487300-38","personal_note":"B02","bl_lot_id":"55984791","external_lot_ids":
{"other":"55984791"},
"remaining_quantity":"79","weight":"0.260","public_note":"","ordered_quantity":"1","order_item_id":"433711","base_price":"0.104","color_name":"Black","color_id":"38","lot_id":"2726913","ids":
[{"id":"3069","type":"design_id"},{"id":"306926","type":"item_no"},{"id":"306926","type":"item_no"},{"id":"3069b","type":"ldraw"},{"id":"3069b","type":"peeron_id"},{"id":"487300-38","type":"boid"}
]
}

*/


////


/*
GET /v1/order/list?key=%s
[{"order_id":"9277759","order_date":"1398955037","total_quantity":"105","total_lots":"25","base_order_total":"23.87","status":"Processing","status_id":"3"},{"order_id":"3957411","order_date":"1398954091","total_quantity":"383","total_lots":"22","base_order_total":"29.80","status":"Processing","status_id":"3"},{"order_id":"3506238","order_date":"1398775922","total_quantity":"83","total_lots":"21","base_order_total":"8.47","status":"Shipped","status_id":"5"}]
*/

static int boParseOrderEntryValue( jsonParser *parser, jsonToken *token, bsOrder *order )
{
  char *name;
  int namelen;
  int64_t readint;
  double readdouble;
  jsonToken *valuetoken;
  char *valuestring;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( boParseCheckName( name, namelen, "order_id" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    order->id = readint;
  }
  else if( boParseCheckName( name, namelen, "order_date" ) || boParseCheckName( name, namelen, "order_time" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    order->date = readint;
  }
  else if( boParseCheckName( name, namelen, "total_quantity" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      order->partcount = (int)readint;
    }
  }
  else if( boParseCheckName( name, namelen, "total_lots" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      order->lotcount = (int)readint;
    }
  }
  else if( boParseCheckName( name, namelen, "base_order_total" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->grandtotal = readdouble;
    }
  }
  else if( boParseCheckName( name, namelen, "status_id" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    order->status = (int)readint;
  }
  else if( boParseCheckName( name, namelen, "sub_total" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      order->subtotal = readdouble;
    }
  }
  else if( boParseCheckName( name, namelen, "payment_currency" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( valuetoken->length < BS_ORDER_CURRENCY_LENGTH )
    {
      memcpy( order->paymentcurrency, valuestring, valuetoken->length );
      order->paymentcurrency[ valuetoken->length ] = 0;
    }
  }
  else if( boParseCheckName( name, namelen, "payment_total" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    order->paymentgrandtotal = readdouble;
  }
  else if( boParseCheckName( name, namelen, "buyer_name" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      free( order->customer );
      order->customer = malloc( valuetoken->length + 1 );
      memcpy( order->customer, &parser->codestring[ valuetoken->offset ], valuetoken->length );
      order->customer[ valuetoken->length ] = 0;
    }
  }
  else if( boParseCheckName( name, namelen, "ship_total" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    order->shippingcost = readdouble;
  }
  else if( boParseCheckName( name, namelen, "tax_amount" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    order->taxamount = readdouble;
  }
  else
  {
    if( !( jsonParserSkipValue( parser ) ) )
      return 0;
  }

  return 1;
}

/* We accepted a '{' */
static int boParseOrderEntry( jsonParser *parser, bsOrder *order )
{
  jsonToken *nametoken;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BO_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Order ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Param : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( boParseOrderEntryValue( parser, nametoken, order ) ) )
      return 0;

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

  /* Any extra order processing */
  order->service = BS_ORDER_SERVICE_BRICKOWL;
  order->changedate = order->date;

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Order\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}

/* We accepted a '{' */
static int boParseOrderListEntry( jsonParser *parser, void *uservalue )
{
  jsonToken *nametoken;
  bsOrder *order;
  bsOrderList *orderlist;

  orderlist = uservalue;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

  orderlist->orderarray = realloc( orderlist->orderarray, ( orderlist->ordercount + 1 ) * sizeof(bsOrder) );
  order = &orderlist->orderarray[ orderlist->ordercount ];
  memset( order, 0, sizeof(bsOrder) );
  orderlist->ordercount++;

  if( !( boParseOrderEntry( parser, order ) ) )
    return 0;

  /* Any extra order processing */
  if( order->date >= orderlist->topdate )
  {
    if( order->date == orderlist->topdate )
      orderlist->topdatecount++;
    else
    {
      orderlist->topdate = order->date;
      orderlist->topdatecount = 1;
    }
  }

  if( parser->errorcount )
    return 0;
  return 1;
}


/* The orderlist returned should be free()'d */
int boReadOrderList( bsOrderList *orderlist, char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  orderlist->orderarray = 0;
  orderlist->ordercount = 0;
  orderlist->topdate = 0;
  orderlist->topdatecount = 0;

  if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACKET ) )
    jsonParserListObjects( &parser, (void *)orderlist, boParseOrderListEntry, 0 );
  else if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACE ) )
    boParseOrderListEntry( &parser, orderlist );
  else
    parser.errorcount = 1;

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    boFreeOrderList( orderlist );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


/* Read order details */
int boReadOrderView( bsOrder *order, char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACE ) )
    boParseOrderEntry( &parser, order );
  else
    parser.errorcount = 1;

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


void boFreeOrderList( bsOrderList *orderlist )
{
  DEBUG_SET_TRACKER();

  bsFreeOrderList( orderlist );
  return;
}


////


typedef struct
{
  void *uservalue;
  void (*callback)( void *uservalue, boItem *boitem );
} boParserState;


static void boParserInitBoItem( boItem *boitem )
{
  memset( boitem, 0, sizeof(boItem) );
  boitem->boid = -1;
  boitem->condition = 'N';
  boitem->bolotid = -1;
  boitem->bllotid = -1;
  return;
}


/* We accepted a '{' */
static int boParseExternalLotIds( jsonParser *parser, boItem *boitem )
{
  jsonToken *nametoken, *valuetoken;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;
#if BO_PARSER_DEBUG
  int i;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering ExtLotIds ( depth %d )\n", parser->depth );
  parser->depth++;
#endif
  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    boitem->extid = &parser->codestring[ valuetoken->offset ];
    boitem->extidlen = valuetoken->length;
#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "ExtLotID : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)valuetoken->length, &parser->codestring[ valuetoken->offset ] );
#endif
    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }
#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting ExtLotIds\n" );
#endif
  if( parser->errorcount )
    return 0;
  return 1;
}


static int boParseGenericLotValue( jsonParser *parser, jsonToken *token, boItem *boitem )
{
  char *name, *valuestring;
  int namelen, offset, valuelength, seqlength;
  int64_t readint;
  double readdouble;
  jsonToken *valuetoken;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( boParseCheckName( name, namelen, "boid" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    valuelength = valuetoken->length;
    offset = ccSeqFindChar( valuestring, valuelength, '-' );
    seqlength = offset;
    if( offset < 0 )
      seqlength = valuelength;
    if( !( ccSeqParseInt64( valuestring, seqlength, &readint ) ) )
    {
      ioPrintf( parser->log, 0, "JSON PARSER: Error, expected integer parse error, offset %d ( %s:%d )\n", (parser->token)->offset, __FILE__, __LINE__ );
      parser->errorcount++;
      return 0;
    }
    boitem->boid = readint;
    boitem->bocolorid = 0;
    if( offset >= 0 )
    {
      valuestring += offset + 1;
      valuelength -= offset + 1;
      if( !( ccSeqParseInt64( valuestring, valuelength, &readint ) ) )
      {
        ioPrintf( parser->log, 0, "JSON PARSER: Error, expected integer parse error, offset %d ( %s:%d )\n", (parser->token)->offset, __FILE__, __LINE__ );
        parser->errorcount++;
        return 0;
      }
      boitem->bocolorid = (int)readint;
    }
  }
  else if( boParseCheckName( name, namelen, "lot_id" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      boitem->bolotid = readint;
    }
  }
  else if( boParseCheckName( name, namelen, "base_price" ) )
  {
    if( !( jsonReadDouble( parser, &readdouble ) ) )
      return 0;
    boitem->price = (float)readdouble;
  }
  else if( boParseCheckName( name, namelen, "public_note" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      boitem->publicnote = &parser->codestring[ valuetoken->offset ];
      boitem->publicnotelen = valuetoken->length;
    }
  }
  else if( boParseCheckName( name, namelen, "personal_note" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      boitem->personalnote = &parser->codestring[ valuetoken->offset ];
      boitem->personalnotelen = valuetoken->length;
    }
  }
  else if( boParseCheckName( name, namelen, "sale_percent" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      boitem->sale = (int)readint;
    }
  }
  else if( boParseCheckName( name, namelen, "bulk_qty" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      boitem->bulk = (int)readint;
      if( boitem->bulk <= 1 )
        boitem->bulk = 0;
    }
  }
  else if( boParseCheckName( name, namelen, "my_cost" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonReadDouble( parser, &readdouble ) ) )
        return 0;
      boitem->mycost = (float)readdouble;
    }
  }
  else if( boParseCheckName( name, namelen, "external_lot_ids" ) )
  {
    if( jsonTokenAccept( parser, JSON_TOKEN_LBRACKET ) )
    {
      if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACKET ) ) )
        return 0;
    }
    else
    {
      if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) ) )
        return 0;
      if( !( boParseExternalLotIds( parser, boitem ) ) )
        return 0;
      jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
    }
  }
  else
    return 0;

  return 1;
}


static int boParseOrderLotValue( jsonParser *parser, jsonToken *token, boItem *boitem )
{
  char *name;
  int namelen;
  int64_t readint;
  char *valuestring;
  jsonToken *valuetoken;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( boParseCheckName( name, namelen, "name" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    boitem->name = &parser->codestring[ valuetoken->offset ];
    boitem->namelen = valuetoken->length;
  }
  else if( boParseCheckName( name, namelen, "ordered_quantity" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    boitem->quantity = (int)readint;
  }
  else if( boParseCheckName( name, namelen, "bl_lot_id" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( jsonReadInteger( parser, &readint, 1 ) )
        boitem->bllotid = readint;
    }
  }
  else if( boParseCheckName( name, namelen, "condition" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( ccStrCmpSeq( "New", valuestring, valuetoken->length ) )
      boitem->condition = 'N';
    else
      boitem->condition = 'U';
  }
  else if( boParseGenericLotValue( parser, token, boitem ) )
    return 1;
  else
  {
    if( !( jsonParserSkipValue( parser ) ) )
      return 0;
  }

  return 1;
}


/* We accepted a '{' */
static int boParseOrderLot( jsonParser *parser, void *uservalue )
{
  jsonToken *nametoken;
  int64_t readint;
  boItem boitem;
  boParserState *state;

  state = uservalue;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

  boParserInitBoItem( &boitem );

#if BO_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Lot ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( boParseOrderLotValue( parser, nametoken, &boitem ) ) )
      return 0;

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Lot\n" );
#endif

  /* If we have no lotid, try reading it from extid */
  if( boitem.extid )
  {
    if( ccSeqParseInt64( boitem.extid, boitem.extidlen, &readint ) )
      boitem.bllotid = readint;
  }

  /* Let the user process the item as he desires */
  state->callback( state->uservalue, &boitem );

  if( parser->errorcount )
    return 0;
  return 1;
}


/* Read order and call callback for every item found */
int boReadOrderInventory( void *uservalue, void (*callback)( void *uservalue, boItem *boitem ), char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;
  boParserState state;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  state.uservalue = uservalue;
  state.callback = callback;

  if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACKET ) )
  {
    jsonParserListObjects( &parser, (void *)&state, boParseOrderLot, 0 );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACKET );
  }
  else if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACE ) )
  {
    boParseOrderLot( &parser, &state );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }
  else
    parser.errorcount = 1;

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


static int boParseInvLotValue( jsonParser *parser, jsonToken *token, boItem *boitem )
{
  char *name;
  int namelen, tierindex;
  int64_t readint;
  double readdouble;
  char *valuestring;
  jsonToken *valuetoken;

  name = &parser->codestring[ token->offset ];
  namelen = token->length;

  if( boParseCheckName( name, namelen, "url" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    boitem->url = &parser->codestring[ valuetoken->offset ];
    boitem->urllen = valuetoken->length;
  }
  else if( boParseCheckName( name, namelen, "qty" ) )
  {
    if( !( jsonReadInteger( parser, &readint, 0 ) ) )
      return 0;
    boitem->quantity = (int)readint;
  }
  else if( boParseCheckName( name, namelen, "con" ) )
  {
    if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    valuestring = &parser->codestring[ valuetoken->offset ];
    if( ccStrCmpSeq( "new", valuestring, valuetoken->length ) || ccStrCmpSeq( "news", valuestring, valuetoken->length ) || ccStrCmpSeq( "newc", valuestring, valuetoken->length ) )
      boitem->condition = 'N';
    else
      boitem->condition = 'U';
  }
  else if( boParseCheckName( name, namelen, "full_con" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      valuestring = &parser->codestring[ valuetoken->offset ];
      if( ccStrCmpSeq( "usedn", valuestring, valuetoken->length ) )
        boitem->usedgrade = 'N';
      else if( ccStrCmpSeq( "usedg", valuestring, valuetoken->length ) )
        boitem->usedgrade = 'G';
      else if( ccStrCmpSeq( "useda", valuestring, valuetoken->length ) )
        boitem->usedgrade = 'A';
    }
  }
  else if( boParseCheckName( name, namelen, "tier_price" ) )
  {
    if( !( jsonTokenAccept( parser, JSON_TOKEN_NULL ) ) )
    {
      if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACKET ) ) )
        return 0;
      for( tierindex = 0 ; ; tierindex++ )
      {
        if( parser->tokentype == JSON_TOKEN_RBRACKET )
          break;
        if( !( jsonTokenAccept( parser, JSON_TOKEN_LBRACKET ) ) )
          return 0;
        if( !( jsonReadInteger( parser, &readint, 0 ) ) )
          return 0;
        if( !( jsonTokenExpect( parser, JSON_TOKEN_COMMA ) ) )
          return 0;
        if( !( jsonReadDouble( parser, &readdouble ) ) )
          return 0;
        if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACKET ) ) )
          return 0;
        if( tierindex < 3 )
        {
          boitem->tierquantity[ tierindex ] = (int)readint;
          boitem->tierprice[ tierindex ] = (float)readdouble;
/*
printf( "########## READ TIER %d : %d %f\n", tierindex, boitem->tierquantity[ tierindex ], boitem->tierprice[ tierindex ] );
*/
        }
        if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
          break;
      }
      if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACKET ) ) )
        return 0;
    }
  }
  else if( boParseGenericLotValue( parser, token, boitem ) )
    return 1;
  else
  {
    if( !( jsonParserSkipValue( parser ) ) )
      return 0;
  }

  return 1;
}


/* We accepted a '{' */
static int boParseInvLot( jsonParser *parser, void *uservalue )
{
  jsonToken *nametoken;
  int64_t readint;
  boItem boitem;
  boParserState *state;

  state = uservalue;
  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

  boParserInitBoItem( &boitem );

#if BO_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Lot ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( !( boParseInvLotValue( parser, nametoken, &boitem ) ) )
      return 0;

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Lot\n" );
#endif

  /* If we have no lotid, try reading it from extid */
  if( boitem.extid )
  {
    if( ccSeqParseInt64( boitem.extid, boitem.extidlen, &readint ) )
      boitem.bllotid = readint;
  }

  /* Let the user process the item as he desires */
  state->callback( state->uservalue, &boitem );

  if( parser->errorcount )
    return 0;
  return 1;
}


int boReadInventory( void *uservalue, void (*callback)( void *uservalue, boItem *boitem ), char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;
  boParserState state;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  state.uservalue = uservalue;
  state.callback = callback;

  if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACKET ) )
  {
    jsonParserListObjects( &parser, (void *)&state, boParseInvLot, 0 );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACKET );
  }
  else if( jsonTokenAccept( &parser, JSON_TOKEN_LBRACE ) )
  {
    boParseInvLot( &parser, &state );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


/*
[
{"con":"new","full_con":"new","qty":"12","lot_id":"2726773","price":"0.720","url":"http:\/\/angrybricks.brickowl.com\/store\/lego-panel-4-x-4-x-6-corner-round-30562-46361","owl_id":"83369","public_note":"","personal_note":"A03+","sale_percent":"0","bulk_qty":"1","for_sale":"1","boid":"306255-64","external_lot_ids":{"other":"55984821"},"type":"Part","ids":[{"id":"30562","type":"design_id"},{"id":"30562","type":"design_id"},{"id":"30562","type":"ldraw"},{"id":"30562","type":"peeron_id"},{"id":"306255-64","type":"boid"},{"id":"4224802","type":"item_no"},{"id":"4224802","type":"item_no"},{"id":"4299371","type":"item_no"},{"id":"4299371","type":"item_no"},{"id":"4631286","type":"item_no"},{"id":"4631286","type":"item_no"}]},
{"con":"new","full_con":"new","qty":"41","lot_id":"2726775","price":"0.887","url":"http:\/\/angrybricks.brickowl.com\/store\/lego-plate-6-x-12-3028","owl_id":"75594","public_note":"","personal_note":"A04*","sale_percent":"0","bulk_qty":"1","for_sale":"1","boid":"20603-50","external_lot_ids":{"other":"55984501"},"type":"Part","ids":[{"id":"20603-50","type":"boid"},{"id":"3028","type":"design_id"},{"id":"3028","type":"design_id"},{"id":"3028","type":"ldraw"},{"id":"3028","type":"peeron_id"},{"id":"4210657","type":"item_no"},{"id":"4210657","type":"item_no"},{"id":"4256149","type":"item_no"},{"id":"4256149","type":"item_no"}]}
]
*/

////


/* We accepted a '{' */
static int boParseColor( jsonParser *parser, boColorTable *colortable )
{
  int bocolor, blcolor;
  jsonToken *nametoken;
  int64_t readint;
  char *valuestring;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BO_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Color ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  bocolor = -1;
  blcolor = -1;
  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    valuestring = &parser->codestring[ nametoken->offset ];
    if( boParseCheckName( valuestring, nametoken->length, "id" ) )
    {
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      bocolor = (int)readint;
    }
    else if( boParseCheckName( valuestring, nametoken->length, "bl_ids" ) )
    {
      if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACKET ) ) )
        return 0;
      if( !( jsonReadInteger( parser, &readint, 0 ) ) )
        return 0;
      if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACKET ) ) )
        return 0;
      blcolor = (int)readint;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

  if( ( (unsigned)bocolor < BO_COLOR_TABLE_RANGE ) && ( (unsigned)blcolor < BO_COLOR_TABLE_RANGE ) )
  {
    colortable->bo2bl[ bocolor ] = blcolor;
    colortable->bl2bo[ blcolor ] = bocolor;
  }

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Color\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* We accepted a '{' */
static int boParseColorTable( jsonParser *parser, boColorTable *colortable )
{
  jsonToken *nametoken;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BO_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Table ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( jsonTokenAccept( parser, JSON_TOKEN_LBRACE ) )
    {
      if( !( boParseColor( parser, colortable ) ) )
        return 0;
      if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACE ) ) )
        return 0;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Table\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* Fill up colortable */
int boReadColorTable( boColorTable *colortable, char *string, ioLog *log )
{
  int index, retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  for( index = 0 ; index < BO_COLOR_TABLE_RANGE ; index++ )
  {
    colortable->bo2bl[index] = -1;
    colortable->bl2bo[index] = -1;
  }

  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    boParseColorTable( &parser, colortable );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


/* Read the first component of "1234" or "1234-56" */
static int boReadIntegerBOID( jsonParser *parser, int64_t *retint )
{
  int i, tokenlength;
  int64_t readint;
  char *tokenstring;
  jsonToken *token;
  token = parser->token;
  tokenstring = &parser->codestring[ token->offset ];
  tokenlength = token->length;
  for( i = 0 ; i < token->length ; i++ )
  {
    if( tokenstring[i] == '-' )
    {
      tokenlength = i;
      break;
    }
  }
  switch( parser->tokentype )
  {
    case JSON_TOKEN_INTEGER:
    case JSON_TOKEN_STRING:
      if( ccSeqParseInt64( tokenstring, tokenlength, &readint ) )
        break;
    default:
      ioPrintf( parser->log, 0, "JSON PARSER: Error, expected integer BOID parse error, offset %d\n", (parser->token)->offset );
      parser->errorcount++;
      return 0;
  }
  jsonTokenIncrement( parser );
  *retint = readint;
  return 1;
}


/* We accepted a '[' */
static int boParseLookupValue( jsonParser *parser, int64_t *retboid )
{
  if( parser->tokentype == JSON_TOKEN_RBRACKET )
    return 1;
#if BO_PARSER_DEBUG
  int i;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering List ( depth %d )\n", parser->depth );
  parser->depth++;
#endif
  for( ; ; )
  {
#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Elem\n" );
#endif
    if( *retboid == -1 )
    {
      if( !( boReadIntegerBOID( parser, retboid ) ) )
        return 0;
    }
    else
      jsonTokenExpect( parser, JSON_TOKEN_STRING );
#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Elem Success\n" );
#endif
    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting List\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* We accepted a '{' */
static int boParseLookup( jsonParser *parser, int64_t *retboid )
{
  jsonToken *nametoken;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BO_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Table ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BO_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( boParseCheckName( &parser->codestring[ nametoken->offset ], nametoken->length, "boids" ) )
    {
      if( !( jsonTokenExpect( parser, JSON_TOKEN_LBRACKET ) ) )
        return 0;
      if( !( boParseLookupValue( parser, retboid ) ) )
        return 0;
      if( !( jsonTokenExpect( parser, JSON_TOKEN_RBRACKET ) ) )
        return 0;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BO_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Table\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* Read BLID lookup */
int boReadLookup( int64_t *retboid, char *string, ioLog *log )
{
  int retval;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;
  int64_t boid;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  boid = -1;
  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    boParseLookup( &parser, &boid );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  if( parser.errorcount )
    boid = -1;

  *retboid = boid;

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


/* We accepted a '{' */
static int boParseLotID( jsonParser *parser, int64_t *retlotid )
{
  jsonToken *nametoken;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering Lot ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    if( boParseCheckName( &parser->codestring[ nametoken->offset ], nametoken->length, "lot_id" ) )
    {
      if( !( jsonReadInteger( parser, retlotid, 0 ) ) )
        return 0;
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting Lot\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* Read lotID for a single lot, as reply to a lot creation */
int boReadLotID( int64_t *retlotid, char *string, ioLog *log )
{
  int retval;
  int64_t lotid;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  lotid = -1;
  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    boParseLotID( &parser, &lotid );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  if( !( parser.errorcount ) && ( lotid != -1 ) )
    *retlotid = lotid;

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
  }

  jsonLexFree( tokenbuf );

  return retval;
}


////


/* We accepted a '{' */
static int boParseUserDetailsStore( jsonParser *parser, boUserDetails *details )
{
  char *name;
  int namelen;
  jsonToken *nametoken;
  jsonToken *valuetoken;
  char *valuestring;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering User Details ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    name = &parser->codestring[ nametoken->offset ];
    namelen = nametoken->length;

    if( boParseCheckName( name, namelen, "name" ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      valuestring = &parser->codestring[ valuetoken->offset ];
      if( valuetoken->length < BO_USER_DETAILS_NAME_LENGTH )
      {
        memcpy( details->storename, valuestring, valuetoken->length );
        details->storename[ valuetoken->length ] = 0;
      }
    }
    else if( boParseCheckName( name, namelen, "base_currency" ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      valuestring = &parser->codestring[ valuetoken->offset ];
      if( valuetoken->length < BS_ORDER_CURRENCY_LENGTH )
      {
        memcpy( details->storecurrency, valuestring, valuetoken->length );
        details->storecurrency[ valuetoken->length ] = 0;
      }
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting User Details\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* We accepted a '{' */
static int boParseUserDetails( jsonParser *parser, boUserDetails *details )
{
  char *name;
  int namelen;
  jsonToken *nametoken;
  jsonToken *valuetoken;
  char *valuestring;

  if( parser->tokentype == JSON_TOKEN_RBRACE )
    return 1;

#if BL_PARSER_DEBUG
  int i;
  jsonToken *token;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Entering User Details ( depth %d )\n", parser->depth );
  parser->depth++;
#endif

  for( ; ; )
  {
    if( !( nametoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
      return 0;
    if( !( jsonTokenExpect( parser, JSON_TOKEN_COLON ) ) )
      return 0;

#if BL_PARSER_DEBUG
    for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
      ioPrintf( parser->log, 0, "Processing Value : %.*s", (int)nametoken->length, &parser->codestring[ nametoken->offset ] );
    token = parser->token;
    ioPrintf( parser->log, 0, " : \"%.*s\"\n", (int)token->length, &parser->codestring[ token->offset ] );
#endif

    name = &parser->codestring[ nametoken->offset ];
    namelen = nametoken->length;

/*
{"username":"Stragus","uid":"9775","has_store":true,"store_details":{"store_id":"907","name":"Angry Bricks","slogan":"","base_currency":"USD","country":"CA","square_logo_24":"http:\/\/img.brickowl.com\/files\/image_cache\/logo_square_24\/angrybricks.png?1436459068","square_logo_16":"http:\/\/img.brickowl.com\/files\/image_cache\/logo_square_16\/angrybricks.png?1436459068","url":"http:\/\/angrybricks.brickowl.com","last_inventory_update":"1454391710","feedback_count":"276","minimum_order":"5.000","minimum_lot_average":"0.000","open":true}}
*/

    if( boParseCheckName( name, namelen, "username" ) )
    {
      if( !( valuetoken = jsonTokenExpect( parser, JSON_TOKEN_STRING ) ) )
        return 0;
      valuestring = &parser->codestring[ valuetoken->offset ];
      if( valuetoken->length < BO_USER_DETAILS_NAME_LENGTH )
      {
        memcpy( details->username, valuestring, valuetoken->length );
        details->username[ valuetoken->length ] = 0;
      }
    }
    else if( boParseCheckName( name, namelen, "store_details" ) )
    {
      if( jsonTokenExpect( parser, JSON_TOKEN_LBRACE ) )
      {
        if( !( boParseUserDetailsStore( parser, details ) ) )
          return 0;
        jsonTokenExpect( parser, JSON_TOKEN_RBRACE );
      }
    }
    else
    {
      if( !( jsonParserSkipValue( parser ) ) )
        return 0;
    }

    if( !( jsonTokenAccept( parser, JSON_TOKEN_COMMA ) ) )
      break;
  }

#if BL_PARSER_DEBUG
  parser->depth--;
  for( i = 0 ; i < parser->depth ; i++ ) ioPrintf( parser->log, 0, "  " );
    ioPrintf( parser->log, 0, "Exiting User Details\n" );
#endif

  if( parser->errorcount )
    return 0;
  return 1;
}


/* Read user details */
int boReadUserDetails( boUserDetails *details, char *string, ioLog *log )
{
  int retval;
  int64_t lotid;
  jsonTokenBuffer *tokenbuf;
  jsonParser parser;

  DEBUG_SET_TRACKER();

  memset( details, 0, sizeof(boUserDetails) );

  tokenbuf = jsonLexParse( string, log );
  if( !( tokenbuf ) )
    return 0;
  jsonTokenInit( &parser, string, tokenbuf, log );

  if( jsonTokenExpect( &parser, JSON_TOKEN_LBRACE ) )
  {
    boParseUserDetails( &parser, details );
    jsonTokenExpect( &parser, JSON_TOKEN_RBRACE );
  }

  retval = 1;
  if( parser.errorcount )
  {
    ioPrintf( parser.log, 0, "JSON Parse Errors Encountered\n" );
    retval = 0;
    memset( details, 0, sizeof(boUserDetails) );
  }

  jsonLexFree( tokenbuf );

  return 1;
}


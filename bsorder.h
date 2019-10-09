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

/*

BrickLink Base
{"order_id":4329553,"date_ordered":"2014-04-16T17:55:49.960Z","date_status_changed":"2014-04-29T13:02:11.000Z","seller_name":"Stragus","store_name":"Angry Bricks","buyer_name":"AmandaEdgeworth","status":"COMPLETED","total_count":22,"unique_count":1,"is_filed":false,"payment":{"method":"PayPal.com","currency_code":"CAD","date_paid":"2014-04-18T14:45:10.000Z","status":"Sent"},"cost":{"currency_code":"USD","subtotal":"11.5940","grand_total":"13.7240"},"disp_cost":{"currency_code":"CAD","subtotal":"12.7874","grand_total":"15.1367"}},

BrickOwl Base
{"order_id":"8252449","order_date":"1401330822","total_quantity":"45","total_lots":"3","base_order_total":"16.46","status":"Payment Received","status_id":"2"},

BrickOwl Full
{"order_id":"8252449","order_time":"1401330822","processed_time":"1401458811","iso_order_time":"2014-05-29T03:33:42+01:00","iso_processed_time":"2014-05-30T15:06:51+01:00","store_id":"907","ship_method_name":"Canada Post, Light Packet, Bubble Mailer","status":"Shipped","status_id":"5","weight":"99.100","ship_total":"4.29","buyer_note":"","total_quantity":"45","total_lots":"3","base_currency":"USD","payment_method_type":"paypal_standard","payment_currency":"USD","payment_total":"16.46","base_order_total":"16.46","sub_total":"12.17","coupon_discount":"0.00","payment_method_note":"","payment_transaction_id":"5DD03100NH8705441","tax_rate":0,"tax_amount":"0.00","tracking_number":null,"tracking_advice":"","buyer_name":"Benjamin Schreiber","combine_with":null,"refund_shipping":"0.00","refund_adjustment":"0.00","refund_subtotal":"0.00","refund_total":"0.00","refund_note":null,"affiliate_fee":0,"brickowl_fee":0.30425,"seller_note":null,"customer_email":"Gtrivdtkj7uRrWEHE9zUSTYvpxE@message.brickowl.com","ship_first_name":"Benjamin","ship_last_name":"Schreiber","ship_country_code":"US","ship_country":"United States","ship_post_code":"46219","ship_street_1":"330 N Bolton Ave","ship_street_2":"","ship_city":"Indianapolis","ship_region":"IN","ship_phone":"3176270905"}

*/



#define BS_ORDER_CURRENCY_LENGTH (4)

typedef struct
{
  int service;

  /* Base; simple BL, simple BO */
  int64_t id;
  int64_t date;
  int64_t changedate;
  int partcount;
  int lotcount;
  int status;
  double grandtotal;

  /* Details; simple BL, details BO */
  double subtotal;
  char paymentcurrency[BS_ORDER_CURRENCY_LENGTH];
  double paymentgrandtotal;
  char *customer;

  /* Full; details BL, details BO */
  double shippingcost;
  double taxamount;

} bsOrder;


typedef struct
{
  bsOrder *orderarray;
  int ordercount;

  /* Highest changedate and the count of orders at that date */
  int64_t topdate;
  int topdatecount;

  /* Level of data storage */
  int infolevel;
} bsOrderList;



enum
{
  BS_ORDER_INFOLEVEL_BASE,
  BS_ORDER_INFOLEVEL_DETAILS,
  BS_ORDER_INFOLEVEL_FULL,

  BS_ORDER_INFOLEVEL_COUNT
};

enum
{
  BS_ORDER_SERVICE_BRICKLINK,
  BS_ORDER_SERVICE_BRICKOWL,

  BS_ORDER_SERVICE_COUNT
};

enum
{
  BL_ORDER_STATUS_PENDING = 0,
  BL_ORDER_STATUS_UPDATED,
  BL_ORDER_STATUS_PROCESSING,
  BL_ORDER_STATUS_READY,
  BL_ORDER_STATUS_PAID,
  BL_ORDER_STATUS_PACKED,
  BL_ORDER_STATUS_SHIPPED,
  BL_ORDER_STATUS_RECEIVED,
  BL_ORDER_STATUS_COMPLETED,
  BL_ORDER_STATUS_CANCELLED,
  BL_ORDER_STATUS_PURGED,
  BL_ORDER_STATUS_NPB,
  BL_ORDER_STATUS_NPX,
  BL_ORDER_STATUS_NRS,
  BL_ORDER_STATUS_NSS,
  BL_ORDER_STATUS_OCR,

  BL_ORDER_STATUS_COUNT
};

enum
{
  BO_ORDER_STATUS_PENDING = 0,
  BO_ORDER_STATUS_PAYMENTSENT = 1,
  BO_ORDER_STATUS_PAID = 2,
  BO_ORDER_STATUS_PROCESSING = 3,
  BO_ORDER_STATUS_PROCESSED = 4,
  BO_ORDER_STATUS_SHIPPED = 5,
  BO_ORDER_STATUS_RECEIVED = 6,
  BO_ORDER_STATUS_CANCELLED = 8,

  BO_ORDER_STATUS_COUNT
};


////


extern const char *bsOrderServiceName[BS_ORDER_SERVICE_COUNT];

const char *bsGetOrderStatusString( bsOrder *order );
const char *bsGetOrderStatusColorString( bsOrder *order );

void bsFreeOrderListEntry( bsOrder *order );

void bsFreeOrderList( bsOrderList *orderlist );

/* Populate an bsx inventory with order information */
void bsOrderSetInventoryInfo( bsxInventory *inv, bsOrder *order );


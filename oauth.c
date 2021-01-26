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
#include <limits.h>
#include <float.h>


#include "oauth.h"


/* For SHA1 hash calculation */
#include <openssl/evp.h>


#define OAUTH_DEBUG (0)

#define OAUTH_DEBUG_MAIN (0)


/*
gcc oauth.c -g -o oauth -lssl
*/


////


static const unsigned char oauthTableBase64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Returned string is malloc()'ed and must be freed */
static char *oauthEncodeBase64( unsigned char *src, int srclen, int *retdstlen, int equalendflag )
{
  char *dst, *dstbase;
  dstbase = malloc( ( ( (srclen+2) / 3 ) << 2 ) + 2 );
  for( dst = dstbase ; srclen >= 3 ; srclen -= 3 )
  {
    dst[0] = oauthTableBase64[ ( src[0] >> 2 ) ];
    dst[1] = oauthTableBase64[ ( ( src[0] & 0x03 ) << 4 ) | ( ( src[1] & 0xf0 ) >> 4 ) ];
    dst[2] = oauthTableBase64[ ( ( src[1] & 0x0f ) << 2 ) | ( ( src[2] & 0xc0 ) >> 6 ) ];
    dst[3] = oauthTableBase64[ ( src[2] & 0x3f ) ];
    src += 3;
    dst += 4;
  }
  if( srclen == 1 )
  {
    dst[0] = oauthTableBase64[ ( src[0] >> 2 ) ];
    dst[1] = oauthTableBase64[ ( ( src[0] & 0x03 ) << 4 ) ];
    dst += 2;
  }
  else if( srclen == 2 )
  {
    dst[0] = oauthTableBase64[ ( src[0] >> 2 ) ];
    dst[1] = oauthTableBase64[ ( ( src[0] & 0x03 ) << 4 ) | ( ( src[1] & 0xf0 ) >> 4 ) ];
    dst[2] = oauthTableBase64[ ( ( src[1] & 0x0f ) << 2 ) ];
    dst += 3;
  }
  dst[0] = 0;
  if( equalendflag )
  {
    dst[0] = '=';
    dst[1] = 0;
    dst++;
  }
  if( retdstlen )
    *retdstlen = (int)( dst - dstbase );
  return dstbase;
}


////


static const unsigned char oauthPercentEncodeTable[256] =
{
  ['a']=1,['b']=1,['c']=1,['d']=1,['e']=1,['f']=1,['g']=1,['h']=1,['i']=1,['j']=1,['k']=1,['l']=1,['m']=1,['n']=1,['o']=1,['p']=1,['q']=1,['r']=1,['s']=1,['t']=1,['u']=1,['v']=1,['w']=1,['x']=1,['y']=1,['z']=1,
  ['A']=1,['B']=1,['C']=1,['D']=1,['E']=1,['F']=1,['G']=1,['H']=1,['I']=1,['J']=1,['K']=1,['L']=1,['M']=1,['N']=1,['O']=1,['P']=1,['Q']=1,['R']=1,['S']=1,['T']=1,['U']=1,['V']=1,['W']=1,['X']=1,['Y']=1,['Z']=1,
  ['0']=1,['1']=1,['2']=1,['3']=1,['4']=1,['5']=1,['6']=1,['7']=1,['8']=1,['9']=1,
  ['-']=1,['.']=1,['_']=1,['~']=1
};

static inline char oauthEncodeCharBase16( unsigned char c )
{
  return ( c < 10 ? '0' + c : ('A'-10) + c );
}

/* Percent-code the string, returned string must be free()'d */
char *oauthPercentEncode( char *string, int length, int *retlength )
{
  char *dst, *dstbase;
  unsigned char c;

  dstbase = malloc( 3*length + 1 );
  for( dst = dstbase ; length ; length--, string++ )
  {
    c = *string;
    if( oauthPercentEncodeTable[ c ] )
    {
      dst[0] = c;
      dst++;
    }
    else
    {
      dst[0] = '%';
      dst[1] = oauthEncodeCharBase16( c >> 4 );
      dst[2] = oauthEncodeCharBase16( c & 0xf );
      dst += 3;
    }
  }
  *dst = 0;
  if( retlength )
    *retlength = (int)( dst - dstbase );

  return dstbase;
}


////


/* EVP_MD_block_size( EVP_sha1() ) == 64 */
#define OAUTH_SHA1_BLOCK_SIZE (64)

/* EVP_MD_size( EVP_sha1() ) == 20 */
#define OAUTH_SHA1_HASH_SIZE (20)

static void oauthHmacSHA1( unsigned char *data, int datalen, unsigned char *key, int keylen, unsigned char *digest, int *retdigestlen )
{
  int i;
  unsigned char k_ipad[OAUTH_SHA1_BLOCK_SIZE];
  unsigned char k_opad[OAUTH_SHA1_BLOCK_SIZE];
  unsigned char hashvalue[EVP_MAX_MD_SIZE];
  unsigned int hashlen;

  EVP_MD_CTX *mdctx;
  mdctx = EVP_MD_CTX_new();

  if( keylen > OAUTH_SHA1_BLOCK_SIZE )
  {
    EVP_MD_CTX_init( mdctx );
    EVP_DigestInit_ex( mdctx, EVP_sha1(), NULL );
    EVP_DigestUpdate( mdctx, key, keylen );
    EVP_DigestFinal_ex( mdctx, hashvalue, &hashlen );
    key = hashvalue;
    keylen = OAUTH_SHA1_HASH_SIZE;
  }
  for( i = 0 ; i < keylen ; i++ )
  {
    k_ipad[i] = key[i] ^ 0x36;
    k_opad[i] = key[i] ^ 0x5c;
  }
  for( ; i < OAUTH_SHA1_BLOCK_SIZE ; i++ )
  {
    k_ipad[i] = 0x36;
    k_opad[i] = 0x5c;
  }
  
  EVP_MD_CTX_init( mdctx );
  EVP_DigestInit( mdctx, EVP_sha1() );
  EVP_DigestUpdate( mdctx, k_ipad, OAUTH_SHA1_BLOCK_SIZE );
  EVP_DigestUpdate( mdctx, data, datalen );
  EVP_DigestFinal_ex( mdctx, hashvalue, &hashlen );

  EVP_MD_CTX_init( mdctx );
  EVP_DigestInit( mdctx, EVP_sha1() );
  EVP_DigestUpdate( mdctx, k_opad, OAUTH_SHA1_BLOCK_SIZE );
  EVP_DigestUpdate( mdctx, hashvalue, hashlen );
  EVP_DigestFinal_ex( mdctx, digest, &hashlen );

  EVP_MD_CTX_free( mdctx );

  if( retdigestlen )
    *retdigestlen = hashlen;
  return;
}


////


static char *strAllocPrintf( char *format, ... )
{
  char *str;
  int strsize, allocsize;
  va_list ap;

  allocsize = 512;
  str = malloc( allocsize );
  for( ; ; )
  {
    va_start( ap, format );
    strsize = vsnprintf( str, allocsize, format, ap );
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    if( strsize == -1 )
      strsize = allocsize << 1;
#endif
    va_end( ap );
    if( strsize < allocsize )
      break;
    allocsize = strsize + 2;
    str = realloc( str, strsize );
  }

  return str;
}


////


uint32_t oauthRand32( oauthRandState32 *randstate )
{
  uint32_t e;
  e = randstate->a - ( ( randstate->b << 27 ) | ( randstate->b >> (32-27) ) );
  randstate->a = randstate->b ^ ( ( randstate->c << 17 ) | ( randstate->c >> (32-17) ) );
  randstate->b = randstate->c + randstate->d;
  randstate->c = randstate->d + e;
  randstate->d = e + randstate->a;
  return randstate->d;
}

void oauthRand32Seed( oauthRandState32 *randstate, uint32_t seed )
{
  uint32_t i;
  randstate->a = 0xf1ea5eed;
  randstate->b = seed;
  randstate->c = seed;
  randstate->d = seed;
  for( i = 0 ; i < 20 ; i++ )
    oauthRand32( randstate );
  return;
}


////


#define OAUTH_RAW_NONCE_LENGTH (12)

void oauthQueryInit( oauthQuery *query, time_t oauthtime, oauthRandState32 *randstate )
{
  int i;
  uint32_t rval;

  query->realm = "";
  query->consumerkey = 0;
  query->consumersecret = 0;
  query->token = 0;
  query->tokensecret = 0;
  query->signaturemethod = "HMAC-SHA1";
  query->signature = 0;
  query->signatureencoded = 0;
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
  query->timestamp = strAllocPrintf( "%I64d", (long long)oauthtime );
#else
  query->timestamp = strAllocPrintf( "%lld", (long long)oauthtime );
#endif
  query->nonce = malloc( OAUTH_RAW_NONCE_LENGTH + 1 );
  for( i = 0 ; i < OAUTH_RAW_NONCE_LENGTH ; i++ )
  {
    rval = oauthRand32( randstate ) & 0x1f;
    query->nonce[i] = rval + ( rval < 10 ? '0' : 'a'-10 );
  }
  query->nonce[i] = 0;
  query->version = "1.0";

  return;
}

void oauthQuerySet( oauthQuery *query, char *consumerkey, char *consumersecret, char *token, char *tokensecret )
{
  query->consumerkey = strdup( consumerkey );
  query->consumersecret = strdup( consumersecret );
  query->token = strdup( token );
  query->tokensecret = strdup( tokensecret );
  return;
}

static inline int oauthParamCompare( const void *p0, const void *p1 )
{
  char *s0, *s1;
  s0 = *(char * const *)p0;
  s1 = *(char * const *)p1;
  return strcmp( s0, s1 );
}

#define OEAUTH_QUERY_PARAM_MAX (256)

int oauthQueryComputeSignature( oauthQuery *query, char *httpmethod, char *url, char *parameters )
{
  int i, paramcount, digestlen;
  size_t paramsize, paramstringlength;
  char *str;
  char *paramtable[OEAUTH_QUERY_PARAM_MAX];
  char *paramstring;
  char *paramencodedstring;
  char *urlencodedstring;
  char *signaturebasestring;
  char *hashkey;
  unsigned char digest[OAUTH_SHA1_HASH_SIZE];

  /* We need to break parameters into name=value pairs, to be able to sort everything */
  paramcount = 0;
  if( ( parameters ) && ( strlen( parameters ) >= 1 ) )
  {
    /* We make a copy of 'parameters' because we are going to slice that in chunks */
    paramstring = strdup( parameters );
    paramtable[paramcount++] = paramstring;
    for( str = paramstring ; ; )
    {
      str = strchr( str, '&' );
      if( !( str ) )
        break;
      *str++ = 0;
      paramtable[paramcount++] = str;
      if( paramcount >= OEAUTH_QUERY_PARAM_MAX )
        return 0;
    }
    /* Alloc each param so paramtable[x] is all malloc'ed */
    for( i = 0 ; i < paramcount ; i++ )
      paramtable[i] = strdup( paramtable[i] );
    free( paramstring );
  }
  if( paramcount >= (OEAUTH_QUERY_PARAM_MAX-8) )
    return 0;
  paramtable[paramcount++] = strAllocPrintf( "%s=%s", "oauth_consumer_key", query->consumerkey );
  paramtable[paramcount++] = strAllocPrintf( "%s=%s", "oauth_token", query->token );
  paramtable[paramcount++] = strAllocPrintf( "%s=%s", "oauth_signature_method", query->signaturemethod );
  paramtable[paramcount++] = strAllocPrintf( "%s=%s", "oauth_timestamp", query->timestamp );
  paramtable[paramcount++] = strAllocPrintf( "%s=%s", "oauth_nonce", query->nonce );
  paramtable[paramcount++] = strAllocPrintf( "%s=%s", "oauth_version", query->version );
  /* Sort all parameters lexically as mandated by the brain-dead HMAC specifications */
  qsort( (void *)paramtable, paramcount, sizeof(char *), oauthParamCompare );

#if OAUTH_DEBUG
  for( i = 0 ; i < paramcount ; i++ )
    printf( "Parameter %d : %s\n", i, paramtable[i] );
  printf( "\n" );
#endif

  /* How large is that thing */
  paramstringlength = paramcount - 1;
  for( i = 0 ; i < paramcount ; i++ )
    paramstringlength += strlen( paramtable[i] );

  /* Pack all sorted parameters into paramstring, a '&' separated sequence */
  paramstring = malloc( paramstringlength + 1 );
  str = paramstring;
  for( i = 0 ; i < paramcount ; i++ )
  {
    if( i )
      *str++ = '&';
    paramsize = strlen( paramtable[i] );
    memcpy( str, paramtable[i], paramsize );
    str += paramsize;
  }
  *str = 0;

  /* Encode and pack into signature base string */
  paramencodedstring = oauthPercentEncode( paramstring, paramstringlength, 0 );
  urlencodedstring = oauthPercentEncode( url, strlen( url ), 0 );
  signaturebasestring = strAllocPrintf( "%s&%s&%s", httpmethod, urlencodedstring, paramencodedstring );

  /* Pack hash key, compute hash, encode hash in base 64 */
  hashkey = strAllocPrintf( "%s&%s", query->consumersecret, query->tokensecret );
  oauthHmacSHA1( (unsigned char *)signaturebasestring, strlen( signaturebasestring ), (unsigned char *)hashkey, strlen( hashkey ), digest, &digestlen );
  query->signature = oauthEncodeBase64( digest, digestlen, 0, 1 );
  query->signatureencoded = oauthPercentEncode( query->signature, strlen( query->signature ), 0 );


#if OAUTH_DEBUG
printf( "Parameter String : %s\n\n", paramstring );
printf( "Encoded Parameter String : %s\n\n", paramencodedstring );
printf( "Encoded URL String : %s\n\n", urlencodedstring );
printf( "Signature Base String : %s\n\n", signaturebasestring );
printf( "Signature : %s\n\n", query->signature );
printf( "Signature Encoded : %s\n\n", query->signatureencoded );
#endif

  /* Free everything */
  for( i = 0 ; i < paramcount ; i++ )
    free( paramtable[i] );
  free( paramstring );
  free( paramencodedstring );
  free( urlencodedstring );
  free( signaturebasestring );
  free( hashkey );

  return 1;
}

/* Return a malloc()'ed string for the Authorization header to put in HTTP header */
char *oauthQueryAuthorizationHeader( oauthQuery *query )
{
  char *str;
  str = strAllocPrintf( "Authorization: OAuth realm=\"%s\", oauth_consumer_key=\"%s\", oauth_token=\"%s\", oauth_signature_method=\"%s\", oauth_signature=\"%s\", oauth_timestamp=\"%s\", oauth_nonce=\"%s\", oauth_version=\"%s\"",
    query->realm, query->consumerkey, query->token, query->signaturemethod, query->signatureencoded, query->timestamp, query->nonce, query->version );
  return str;
}

void oauthQueryFree( oauthQuery *query )
{
  free( query->consumerkey );
  free( query->consumersecret );
  free( query->token );
  free( query->tokensecret );
  free( query->signature );
  free( query->signatureencoded );
  free( query->timestamp );
  free( query->nonce );
  memset( query, 0, sizeof(oauthQuery) );
  return;
}



////


#if OAUTH_DEBUG_MAIN


static char ConsumerKey[] = "abcd";
static char ConsumerSecret[] = "efgh";
static char Token[] = "ijkl";
static char TokenSecret[] = "mnop";


int main()
{
  oauthQuery query;
  oauthRandState32 oauthrand;
  char *oauthheader;


  /* We use a tiny home-made RNG, rand() is broken by design */
  oauthRand32Seed( &oauthrand, (uint32_t)time( 0 ) + (uint32_t)(intptr_t)&oauthrand );

  oauthQueryInit( &query, time( 0 ), &oauthrand );
  oauthQuerySet( &query, ConsumerKey, ConsumerSecret, Token, TokenSecret );
  oauthQueryComputeSignature( &query, "GET", "https://api.bricklink.com/api/store/v1/orders", "direction=in" );
  oauthheader = oauthQueryAuthorizationHeader( &query );
  oauthQueryFree( &query );

  /* Printing the stuff to insert in your HTTP header */
  printf( "... Printing OAuth Authorization String ...\n%s\n", oauthheader );

  free( oauthheader );


  return 1;
}


#endif


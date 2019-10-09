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

#include <sys/types.h>


typedef struct
{
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
} oauthRandState32;

/* Custom random number generator */
uint32_t oauthRand32( oauthRandState32 *randstate );
void oauthRand32Seed( oauthRandState32 *randstate, uint32_t seed );


typedef struct
{
  char *realm;
  char *consumerkey;
  char *consumersecret;
  char *token;
  char *tokensecret;
  char *signaturemethod;
  char *signature;
  char *signatureencoded;
  char *timestamp;
  char *nonce;
  char *version;
} oauthQuery;


void oauthQueryInit( oauthQuery *query, time_t oauthtime, oauthRandState32 *randstate );

void oauthQuerySet( oauthQuery *query, char *consumerkey, char *consumersecret, char *token, char *tokensecret );

/* Compute the HMAC-SHA1 signature for the query
 * httpmethod = "GET" ; url = "http://host.net/resource" ; parameters = "name=value&name2=value2" */
int oauthQueryComputeSignature( oauthQuery *query, char *httpmethod, char *url, char *parameters );

/* Return a malloc()'ed string for the Authorization header to put in HTTP header */
char *oauthQueryAuthorizationHeader( oauthQuery *query );

void oauthQueryFree( oauthQuery *query );


////


/* Percent-code the string, returned string must be free()'d */
char *oauthPercentEncode( char *string, int length, int *retlength );


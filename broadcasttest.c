/* -----------------------------------------------------------------------------
 *
 * broadcasttest: checks the version check and broadcast message reading (bsbroadcast.c).
 *
 * - release tags parsed and ordered (v1.7.4-271026 < v1.7.4-271026a < v1.7.4-281026);
 * - build stamps dated, release state and "only before" decided for release and hand builds;
 * - a GitHub "latest release" reply (nested objects, arrays, escapes) gives its tag_name;
 * - broadcast/message.json read: priority names, timestamp, only_before, message text;
 * - message text made safe: escapes decoded, ESC and other control characters dropped.
 * Exit code 0 when every case behaves, 1 otherwise. Built and run by build.sh test / CI.
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
#include "iolog.h"
#include "json.h"
#include "bsbroadcast.h"


static int failures = 0;

static void check( int condition, const char *what )
{
  printf( "%s %s\n", ( condition ? "ok:  " : "FAIL:" ), what );
  if( !( condition ) )
    failures++;
  return;
}

static int compareTags( const char *a, const char *b )
{
  bsReleaseTag ta, tb;
  if( !( bsReleaseTagParse( a, &ta ) ) || !( bsReleaseTagParse( b, &tb ) ) )
    return 99;
  return bsReleaseTagCompare( &ta, &tb );
}

/* The parsers work on a writable copy, as BrickSync's reply buffer is */
static int parseBroadcast( ioLog *log, const char *json, bsBroadcast *msg )
{
  int retval;
  char *copy;
  copy = strdup( json );
  retval = bsParseBroadcast( copy, log, msg );
  free( copy );
  return retval;
}

static int parseRelease( ioLog *log, const char *json, char *tag, int tagsize )
{
  int retval;
  char *copy;
  copy = strdup( json );
  retval = bsParseLatestRelease( copy, log, tag, tagsize );
  free( copy );
  return retval;
}


static void testTags( void )
{
  bsReleaseTag tag;

  check( bsReleaseTagParse( "v1.7.4-271026a", &tag ) && ( tag.major == 1 ) && ( tag.minor == 7 ) && ( tag.revision == 4 ) && ( tag.date == 20261027 ) && !( strcmp( tag.suffix, "a" ) ), "v1.7.4-271026a parsed" );
  check( bsReleaseTagParse( "v1.7.4-261026", &tag ) && ( tag.date == 20261026 ) && !( tag.suffix[0] ), "v1.7.4-261026 parsed, no suffix" );
  check( !( bsReleaseTagParse( "v1.7.4", &tag ) ), "tag without date refused" );
  check( !( bsReleaseTagParse( "v1.7.4-321026", &tag ) ), "day 32 refused" );
  check( !( bsReleaseTagParse( "v1.7.4-271326", &tag ) ), "month 13 refused" );
  check( !( bsReleaseTagParse( "v1.7.4-271026A", &tag ) ), "upper-case suffix refused" );
  check( !( bsReleaseTagParse( "v1.7.4-271026-rc1", &tag ) ), "other suffix refused" );
  check( !( bsReleaseTagParse( "v1.7.4-271026abcdefgh", &tag ) ), "suffix too long refused" );
  check( !( bsReleaseTagParse( "", &tag ) ) && !( bsReleaseTagParse( 0, &tag ) ), "empty tag refused" );

  check( compareTags( "v1.7.4-271026", "v1.7.4-271026a" ) < 0, "271026 < 271026a" );
  check( compareTags( "v1.7.4-271026a", "v1.7.4-271026b" ) < 0, "271026a < 271026b" );
  check( compareTags( "v1.7.4-271026z", "v1.7.4-271026aa" ) < 0, "271026z < 271026aa" );
  check( compareTags( "v1.7.4-311026", "v1.7.4-011126" ) < 0, "31 Oct < 1 Nov (date, not text, order)" );
  check( compareTags( "v1.7.4-311226", "v1.7.4-010127" ) < 0, "31 Dec 2026 < 1 Jan 2027" );
  check( compareTags( "v1.7.4-311226", "v1.7.5-010126" ) < 0, "revision outranks date" );
  check( compareTags( "v1.7.4-271026a", "v1.7.4-271026a" ) == 0, "same tag equal" );
  return;
}


static void testStates( void )
{
  check( bsBuildStampDate( "20261027.64" ) == 20261027, "CI stamp dated" );
  check( bsBuildStampDate( "20261027.0930" ) == 20261027, "hand-build stamp dated" );
  check( bsBuildStampDate( "20261027" ) == 20261027, "bare date stamp dated" );
  check( bsBuildStampDate( "dev" ) == 0, "dev stamp undated" );
  check( bsBuildStampDate( "1761555600" ) == 0, "old seconds stamp undated" );
  check( bsBuildStampDate( 0 ) == 0, "no stamp undated" );

  check( bsReleaseState( "v1.7.4-271026a", "20261027.64", "v1.7.4-271026a" ) == BS_RELEASE_STATE_LATEST, "release build, same as latest: LATEST" );
  check( bsReleaseState( "v1.7.4-261026b", "20261026.54", "v1.7.4-271026a" ) == BS_RELEASE_STATE_NEWER_AVAILABLE, "older release build: NEWER_AVAILABLE" );
  check( bsReleaseState( "v1.7.4-281026", "20261028.70", "v1.7.4-271026a" ) == BS_RELEASE_STATE_AHEAD, "newer release build: AHEAD" );
  check( bsReleaseState( "", "20261020.0930", "v1.7.4-271026a" ) == BS_RELEASE_STATE_NEWER_AVAILABLE, "hand build dated before latest: NEWER_AVAILABLE" );
  check( bsReleaseState( "", "20261027.1200", "v1.7.4-271026a" ) == BS_RELEASE_STATE_DEVELOPMENT, "hand build the same day: DEVELOPMENT" );
  check( bsReleaseState( "", "dev", "v1.7.4-271026a" ) == BS_RELEASE_STATE_DEVELOPMENT, "undated build: DEVELOPMENT" );
  check( bsReleaseState( "v1.7.4-271026a", "20261027.64", "garbage" ) == BS_RELEASE_STATE_UNKNOWN, "unreadable latest: UNKNOWN" );

  check( bsBuildOlderThan( "v1.7.4-261026b", "20261026.54", "v1.7.4-271026a" ), "older release is older than only_before" );
  check( !( bsBuildOlderThan( "v1.7.4-271026a", "20261027.64", "v1.7.4-271026a" ) ), "the only_before release itself is not older" );
  check( bsBuildOlderThan( "", "20261020.0930", "v1.7.4-271026a" ), "earlier hand build is older" );
  check( !( bsBuildOlderThan( "", "20261029.0930", "v1.7.4-271026a" ) ), "later hand build is not older" );
  check( bsBuildOlderThan( "", "dev", "v1.7.4-271026a" ), "undated build counts as older" );
  check( bsBuildOlderThan( "v1.7.4-271026a", "20261027.64", "nonsense" ), "unreadable only_before shows to everyone" );
  return;
}


static void testRelease( ioLog *log )
{
  char tag[BS_RELEASE_TAG_LENGTH_MAX];
  static const char *reply =
    "{\n"
    "  \"url\": \"https://api.github.com/repos/ZZJHONS/Bricksync/releases/1\",\n"
    "  \"id\": 123456789,\n"
    "  \"author\": { \"login\": \"ZZJHONS\", \"id\": 42, \"site_admin\": false, \"tag_name\": \"v9.9.9-010199\" },\n"
    "  \"tag_name\": \"v1.7.4-271026a\",\n"
    "  \"name\": \"BrickSync \\\"271026a\\\" \\u00e9\",\n"
    "  \"draft\": false,\n"
    "  \"prerelease\": false,\n"
    "  \"published_at\": \"2026-10-27T10:00:00Z\",\n"
    "  \"assets\": [ { \"name\": \"bricksync-win64.zip\", \"size\": 1234, \"uploader\": { \"login\": \"x\" } }, { \"name\": \"SHA256SUMS\" } ],\n"
    "  \"body\": \"Line one\\nLine two\",\n"
    "  \"reactions\": null\n"
    "}\n";

  check( parseRelease( log, reply, tag, sizeof(tag) ) && !( strcmp( tag, "v1.7.4-271026a" ) ), "latest release reply gives the top-level tag_name" );
  check( !( parseRelease( log, "{ \"tag_name\": \"nightly\" }", tag, sizeof(tag) ) ) && !( tag[0] ), "tag in another format refused" );
  check( !( parseRelease( log, "{ \"message\": \"Not Found\", \"documentation_url\": \"https://docs.github.com\" }", tag, sizeof(tag) ) ), "Not Found reply: no tag" );
  check( !( parseRelease( log, "{ \"tag_name\": \"v1.7.4-271026a\"", tag, sizeof(tag) ) ), "truncated reply refused" );
  check( !( parseRelease( log, "", tag, sizeof(tag) ) ), "empty reply refused" );
  return;
}


static void testBroadcast( ioLog *log )
{
  bsBroadcast msg;

  check( parseBroadcast( log, "{ \"timestamp\": 1761559200, \"priority\": \"high\", \"only_before\": \"v1.7.4-271026a\", \"message\": \"Update now.\\nIt fixes TLS.\" }", &msg ), "full message read" );
  check( msg.timestamp == 1761559200, "timestamp read" );
  check( msg.priority == BS_BROADCAST_PRIORITY_HIGH, "priority high" );
  check( !( strcmp( msg.onlybefore, "v1.7.4-271026a" ) ), "only_before read" );
  check( !( strcmp( msg.text, "Update now.\nIt fixes TLS." ) ), "message text decoded" );

  check( parseBroadcast( log, "{ \"timestamp\": 1, \"priority\": \"CRITICAL\", \"message\": \"x\", \"extra\": { \"a\": [1, 2] } }", &msg ) && ( msg.priority == BS_BROADCAST_PRIORITY_CRITICAL ), "priority case-insensitive, unknown keys skipped" );
  check( parseBroadcast( log, "{ \"timestamp\": 1, \"priority\": \"low\", \"message\": \"x\" }", &msg ) && ( msg.priority == BS_BROADCAST_PRIORITY_LOW ), "priority low" );
  check( parseBroadcast( log, "{ \"timestamp\": 1, \"priority\": \"medium\", \"message\": \"x\" }", &msg ) && ( msg.priority == BS_BROADCAST_PRIORITY_MEDIUM ), "priority medium" );
  check( parseBroadcast( log, "{ \"timestamp\": 1, \"priority\": \"urgent\", \"message\": \"x\" }", &msg ) && ( msg.priority == BS_BROADCAST_PRIORITY_NONE ), "unknown priority is none" );
  check( parseBroadcast( log, "{ \"timestamp\": 5, \"priority\": \"high\", \"message\": \"\" }", &msg ) && ( msg.priority == BS_BROADCAST_PRIORITY_NONE ), "no text: priority none" );
  check( parseBroadcast( log, "{ \"timestamp\": -5, \"priority\": \"low\", \"message\": \"x\" }", &msg ) && ( msg.timestamp == 0 ), "negative timestamp ignored" );
  check( parseBroadcast( log, "{ \"timestamp\": \"1761559200\", \"priority\": \"low\", \"message\": \"x\" }", &msg ) && ( msg.timestamp == 0 ), "timestamp as a string ignored" );
  check( parseBroadcast( log, "{}", &msg ) && ( msg.priority == BS_BROADCAST_PRIORITY_NONE ) && !( msg.text[0] ), "empty object: nothing to show" );
  check( !( parseBroadcast( log, "[ \"message\" ]", &msg ) ), "array refused" );
  check( !( parseBroadcast( log, "{ \"message\": \"x\" ", &msg ) ) && !( msg.text[0] ), "truncated file refused, nothing kept" );
  check( !( parseBroadcast( log, "<html>404</html>", &msg ) ), "HTML refused" );
  return;
}


static void testDecode( ioLog *log )
{
  char out[64];
  bsBroadcast msg;
  static const char *escaped = "a\\u001b[31mred\\u0007\\r\\u007f\\b\\f!";

  bsDecodeJsonText( out, sizeof(out), escaped, (int)strlen( escaped ) );
  check( !( strcmp( out, "a[31mred!" ) ), "ESC, BEL, CR, DEL, backspace, form feed dropped" );

  bsDecodeJsonText( out, sizeof(out), "a\x1b[2Jb", (int)strlen( "a\x1b[2Jb" ) );
  check( !( strcmp( out, "a[2Jb" ) ), "raw ESC dropped" );

  bsDecodeJsonText( out, sizeof(out), "q\\\"s\\\\t\\/", 10 );
  check( !( strcmp( out, "q\"s\\t/" ) ), "quote, backslash and slash escapes" );

  bsDecodeJsonText( out, sizeof(out), "1\\u009b2\xc2\x9b" "3\xc2\xa0" "4", (int)strlen( "1\\u009b2\xc2\x9b" "3\xc2\xa0" "4" ) );
  check( !( strcmp( out, "123\xc2\xa0" "4" ) ), "C1 controls (CSI) dropped, escaped or raw; no-break space kept" );

  bsDecodeJsonText( out, sizeof(out), "tab\\there", (int)strlen( "tab\\there" ) );
  check( !( strcmp( out, "tab\there" ) ), "tab kept" );

  bsDecodeJsonText( out, sizeof(out), "\\u00e9\\u20ac", (int)strlen( "\\u00e9\\u20ac" ) );
  check( !( strcmp( out, "\xc3\xa9\xe2\x82\xac" ) ), "\\u00e9 and \\u20ac to UTF-8" );

  bsDecodeJsonText( out, sizeof(out), "\\ud83d\\ude00", (int)strlen( "\\ud83d\\ude00" ) );
  check( !( strcmp( out, "??" ) ), "surrogate pair becomes ??" );

  bsDecodeJsonText( out, sizeof(out), "x\\u12", (int)strlen( "x\\u12" ) );
  check( !( strcmp( out, "x12" ) ), "short \\u escape left as plain text" );

  bsDecodeJsonText( out, 4, "abcdef", (int)strlen( "abcdef" ) );
  check( !( strcmp( out, "abc" ) ), "output cut at the buffer, terminated" );

  bsDecodeJsonText( out, 4, "ab\\u20ac", (int)strlen( "ab\\u20ac" ) );
  check( !( strcmp( out, "ab" ) ), "multi-byte character that does not fit left out whole" );

  check( parseBroadcast( log, "{ \"priority\": \"high\", \"message\": \"\\u001b]0;pwned\\u0007Hello\" }", &msg ) && !( strcmp( msg.text, "]0;pwnedHello" ) ), "message cannot send terminal escapes" );
  return;
}


int main( void )
{
  ioLog log;

  setvbuf( stdout, 0, _IONBF, 0 );
  if( !( ioLogInit( &log, "bt-logs", 0 ) ) )
  {
    printf( "FAIL: could not open a log\n" );
    return 1;
  }
  testTags();
  testStates();
  testRelease( &log );
  testBroadcast( &log );
  testDecode( &log );
  ioLogEnd( &log );

  printf( "%s: %d failure(s)\n", ( failures ? "broadcasttest FAILED" : "broadcasttest passed" ), failures );
  return ( failures ? 1 : 0 );
}

/* -----------------------------------------------------------------------------
 *
 * Version check and broadcast message, both read from GitHub.
 *
 * - The latest release comes from the GitHub Releases API (its tag, e.g. v1.7.4-261027a).
 * - The broadcast message is broadcast/message.json in the repository (see broadcast/README.md).
 *
 * This file holds the parts that need no network: release tag parsing and comparison,
 * reading both JSON replies, and turning JSON string text into safe terminal text.
 *
 * -----------------------------------------------------------------------------
 */

#ifndef BSBROADCAST_H
#define BSBROADCAST_H

#define BS_RELEASE_TAG_LENGTH_MAX (32)
#define BS_BROADCAST_TEXT_LENGTH_MAX (1024)


/* A release tag: v<major>.<minor>.<revision>-<YYMMDD><suffix>, suffix "" or lowercase letters */
typedef struct
{
  int major;
  int minor;
  int revision;
  /* Release date as YYYYMMDD */
  int date;
  char suffix[8];
} bsReleaseTag;

/* Parse a release tag; 1 on success */
int bsReleaseTagParse( const char *tag, bsReleaseTag *ret );

/* <0, 0, >0 as a is older than, the same as, or newer than b */
int bsReleaseTagCompare( const bsReleaseTag *a, const bsReleaseTag *b );

/* The date (YYYYMMDD) a build stamp starts with ("20261027.64", "20261027.0930"); 0 if none ("dev") */
int bsBuildStampDate( const char *stamp );


enum
{
  /* The latest release could not be read */
  BS_RELEASE_STATE_UNKNOWN,
  /* We are that release */
  BS_RELEASE_STATE_LATEST,
  /* A newer release exists */
  BS_RELEASE_STATE_NEWER_AVAILABLE,
  /* We are a release newer than the latest one (should not happen) */
  BS_RELEASE_STATE_AHEAD,
  /* We are not a release build (hand or branch build) and not older than the latest release */
  BS_RELEASE_STATE_DEVELOPMENT
};

/* Compare this build with the latest release. ourtag: the release tag this build was made from, "" for none;
 * ourstamp: its build stamp. A non-release build is compared by date only */
int bsReleaseState( const char *ourtag, const char *ourstamp, const char *latesttag );

/* Is this build older than the release named by tag? Unknown dates count as older */
int bsBuildOlderThan( const char *ourtag, const char *ourstamp, const char *tag );


enum
{
  BS_BROADCAST_PRIORITY_NONE,
  BS_BROADCAST_PRIORITY_LOW,
  BS_BROADCAST_PRIORITY_MEDIUM,
  BS_BROADCAST_PRIORITY_HIGH,
  BS_BROADCAST_PRIORITY_CRITICAL,

  BS_BROADCAST_PRIORITY_COUNT
};

typedef struct
{
  /* Unix time; a different value is a new message */
  int64_t timestamp;
  int priority;
  /* Shown only to builds older than this release tag, "" for everyone */
  char onlybefore[BS_RELEASE_TAG_LENGTH_MAX];
  /* Safe to print: no control characters other than newline and tab */
  char text[BS_BROADCAST_TEXT_LENGTH_MAX];
} bsBroadcast;

/* Read the tag_name of a GitHub "latest release" reply; 1 on success */
int bsParseLatestRelease( char *json, ioLog *log, char *rettag, int rettagsize );

/* Read broadcast/message.json; 1 on success. Unknown keys are ignored */
int bsParseBroadcast( char *json, ioLog *log, bsBroadcast *ret );

/* Turn the text of a JSON string (escapes not yet decoded) into safe terminal text: escapes decoded,
 * control characters other than newline and tab dropped (no terminal escape sequences), always terminated */
void bsDecodeJsonText( char *dst, int dstsize, const char *src, int srclen );


#endif

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

#define BS_VERSION_MAJOR (1)
#define BS_VERSION_MINOR (7)
#define BS_VERSION_REVISION (4)
#define BS_VERSION_STRING "1.7.4"
#define BS_VERSION_INTEGER_ENCODE(major,minor,revision) (((major)*10000)|((minor)*100)|((revision)*1))
#define BS_VERSION_INTEGER BS_VERSION_INTEGER_ENCODE(BS_VERSION_MAJOR,BS_VERSION_MINOR,BS_VERSION_REVISION)


////


#define BS_ENABLE_REGISTRATION (0)
#define BS_ENABLE_LIMITS (0)
#define BS_ENABLE_ANTIDEBUG (0)
#define BS_ENABLE_ANTIDEBUG_CRASH (0)
#define BS_ENABLE_MATHPUZZLE (0)


#define BS_ENABLE_DEBUG_OUTPUT (1)

#define BS_ENABLE_DEBUG_SPECIAL (0)


////


#define BS_BRICKLINK_API_SERVER "api.bricklink.com"
#define BS_BRICKLINK_WEB_SERVER "www.bricklink.com"
#define BS_BRICKOWL_API_SERVER "api.brickowl.com"
#define BS_BRICKSYNC_WEB_SERVER "www.bricksync.net"

#define BS_GLOBAL_PATH "data" CC_DIR_SEPARATOR_STRING

/* BrickSync file paths */
#define BS_INVENTORY_FILE BS_GLOBAL_PATH "bricksync.inventory.bsx"
#define BS_INVENTORY_TEMP_FILE BS_GLOBAL_PATH "temp.bricksync.inventory.bsx"
#define BS_STATE_FILE BS_GLOBAL_PATH "bricksync.state"
#define BS_STATE_TEMP_FILE BS_GLOBAL_PATH "temp.bricksync.state"
#define BS_JOURNAL_FILE BS_GLOBAL_PATH "bricksync.journal"
#define BS_JOURNAL_TEMP_FILE BS_GLOBAL_PATH "temp.bricksync.journal"
#define BS_LOCK_FILE BS_GLOBAL_PATH "bricksync.lock"
#define BS_TRANSLATION_FILE BS_GLOBAL_PATH "bricksync.trs"
#define BS_CONFIGURATION_FILE BS_GLOBAL_PATH "bricksync.conf.txt"

/* BrickSync directories and custom paths */
#define BS_LOG_DIR BS_GLOBAL_PATH "logs"
#define BS_BACKUP_DIR BS_GLOBAL_PATH "backups"
#define BS_ERROR_DIR BS_GLOBAL_PATH "errors-"
#define BS_BRICKLINK_ORDER_DIR BS_GLOBAL_PATH "orders"
#define BS_BRICKLINK_ORDER_PATH BS_GLOBAL_PATH "orders" CC_DIR_SEPARATOR_STRING "bricklink-%lld.bsx"
#define BS_BRICKLINK_ORDER_TEMP_PATH BS_GLOBAL_PATH "orders" CC_DIR_SEPARATOR_STRING ".temp.bricklink-%lld.bsx"
#define BS_BRICKOWL_ORDER_DIR BS_GLOBAL_PATH "orders"
#define BS_BRICKOWL_ORDER_PATH BS_GLOBAL_PATH "orders" CC_DIR_SEPARATOR_STRING "brickowl-%lld.bsx"
#define BS_BRICKOWL_ORDER_TEMP_PATH BS_GLOBAL_PATH "orders" CC_DIR_SEPARATOR_STRING".temp.brickowl-%lld.bsx"
#define BS_PRICEGUIDE_DIR BS_GLOBAL_PATH "pgcache"

/* BrickSync XML output */
#define BS_BLXMLUPLOAD_FILE "blupload%03d.xml.txt"
#define BS_BLXMLUPDATE_FILE "blupdate%03d.xml.txt"

#define BS_BRICKLINK_PIPELINED_FETCH (4)
#define BS_BRICKLINK_PIPELINED_FETCH_MAX (8)
#define BS_BRICKOWL_PIPELINED_FETCH (4)
#define BS_BRICKOWL_PIPELINED_FETCH_MAX (8)


/*
 * Upon initialization, fetch all BrickLink orders from up to 30 days back
 * Also fetch all pending orders up to 180 days back
 */
#define BS_INIT_ORDER_FETCH_DATE (30*24*60*60)
#define BS_INIT_ORDER_PENDING_DATE (180*24*60*60)


#define BS_POLL_SUCCESS_INTERVAL_DEFAULT (15*60)
#define BS_POLL_SUCCESS_INTERVAL_MIN (5*60)

#define BS_POLL_FAIL_INTERVAL_DEFAULT (5*60)
#define BS_POLL_FAIL_INTERVAL_MIN (2*60)

#define BS_SYNC_DELAY_BASE (30)
#define BS_SYNC_DELAY_MAX (60*30)
#define BS_SYNC_DELAY_FAIL_FACTOR (3)

#define BS_BRICKLINK_APICOUNT_LIMIT_DEFAULT (5000)
#define BS_BRICKLINK_APICOUNT_PRICELIMIT_DEFAULT (2500)
#define BS_BRICKLINK_APICOUNT_NOTESLIMIT_DEFAULT (3600)
#define BS_BRICKLINK_APICOUNT_QUANTITYLIMIT_DEFAULT (4800)
#define BS_BRICKLINK_APICOUNT_SYNCRESUME_DEFAULT (2000)

#define BS_PRICEGUIDE_CACHETIME_DEFAULT (5*24*60*60)

/* Secret offset to be decrypted by registration key */
#define BS_REGISTRATION_SECRET_OFFSET (0x9a6fc)

/* Time since lastruntime to perform an automated BL backup */
#define BS_INITBACKUP_LASTRUNTIME (12*60*60)

/* BrickSync message check interval */
#define BS_BRICKSYNC_MESSAGE_INTERVAL (3*60*60)

/* Time in seconds between printing progress of ApplyDiff() */
#define BS_APPLYDELTA_PROGRESS_PRINT_INTERVAL (5)

/* Maximum count of BrickOwl orders returned by bsQueryBickOwlOrderList() */
#define BS_FETCH_BRICKOWL_ORDERLIST_MAXIMUM (16384)


////


#if 1
 #define BS_LIMITS_WARNING_PARTCOUNT (200000)
 #define BS_LIMITS_MAX_PARTCOUNT (250000)
 #define BS_LIMITS_HARDMAX_PARTCOUNT_MASK (~0x3ffff)
#else
 #define BS_LIMITS_WARNING_PARTCOUNT (1000)
 #define BS_LIMITS_MAX_PARTCOUNT (1000)
 #define BS_LIMITS_HARDMAX_PARTCOUNT_MASK (~0x3ff)
#endif


////


#define BS_HTTP_DEBUG (0)
#define BS_HTTP_DEBUG_MAXOUT (256)

#define BS_INTERNAL_DEBUG (1)

#define BS_ENABLE_COREDUMP (1)

#define BS_INTERNAL_ERROR_EXIT() {printf( "INTERNAL ERROR: In %s at %s:%d\n", __FUNCTION__, __FILE__, __LINE__ );exit(0);}

#if BS_ENABLE_MATHPUZZLE
 #include "bsmathpuzzle.h"
#endif


////


#define BS_DEBUG_TRACKER_WATCH_MILLISECONDS (30*1000) /* check each 30 seconds */
#define BS_DEBUG_TRACKER_STALL_MILLISECONDS (15*60*1000) /* 15 minutes */


////


#define BS_DISK_SPACE_WARNING (((uint64_t)2048)*1048576)
#define BS_DISK_SPACE_CRITICAL (((uint64_t)512)*1048576)

/* Disk space check interval */
#define BS_BRICKSYNC_DISKSPACECHECK_INTERVAL (3*60*60)
#define BS_BRICKSYNC_DISKSPACECHECK_WARNING_INTERVAL (30*60)


////


#define BS_API_HISTORY_TIMETOTAL (24*60*60)
#define BS_API_HISTORY_TIMEUNIT (10*60)
#define BS_API_HISTORY_SIZE (BS_API_HISTORY_TIMETOTAL/BS_API_HISTORY_TIMEUNIT)
#define BS_API_HISTORY_SATURATE (0xffff)

/* Format must *NOT* change, struct is directly saved as such in BrickSync state file */
typedef struct
{
  int64_t basetime;
  uint16_t count[BS_API_HISTORY_SIZE];
  uint32_t total;
} bsApiHistory __attribute__ ((aligned(8)));

typedef struct
{
  /* Access credentials */
  char *consumerkey;
  char *consumersecret;
  char *token;
  char *tokensecret;
  /* IP text strings */
  char *apiaddress;
  char *webaddress;
  /* API HTTP connection */
  httpConnection *http;
  /* Web HTTP connection */
  httpConnection *webhttp;
  /* Pipelined fetch count */
  int pipelinequeuesize;
  /* Timestamp of latest order + 1 */
  int64_t orderinitdate;
  int64_t ordertopdate;
  int64_t oauthtime;
  /* Sync */
  int failinterval;
  int pollinterval;
  int syncdelay;
  time_t checktime;
  time_t synctime;
  time_t lastchecktime;
  time_t lastsynctime;
  /* Daily API call limit */
  int apicountlimit;
  int apicountpricelimit;
  int apicountnoteslimit;
  int apicountquantitylimit;
  int apicountsyncresume;
  /* XML output indices */
  int xmluploadindex;
  int xmlupdateindex;
  /* Count of pending queries */
  int querycount;
  int webquerycount;
  /* Update diff inventory when PENDING_UPDATE flag is set */
  bsxInventory *diffinv;
  /* Track counts of API usage */
  bsApiHistory apihistory;
} bsBrickLink;

typedef struct
{
  /* Access credentials */
  char *key;
  /* IP text strings */
  char *apiaddress;
  /* API HTTP connection */
  httpConnection *http;
  /* Pipelined fetch count */
  int pipelinequeuesize;
  /* Timestamp of latest order + 1 */
  int64_t orderinitdate;
  int64_t ordertopdate;
  /* Sync */
  int failinterval;
  int pollinterval;
  int syncdelay;
  time_t checktime;
  time_t synctime;
  time_t lastchecktime;
  time_t lastsynctime;
  /* Reuse BrickOwl existing lots with quantities of zero */
  int reuseemptyflag;
  /* Count of pending queries */
  int querycount;
  /* Update diff inventory when PENDING_UPDATE flag is set */
  bsxInventory *diffinv;
  /* Track counts of API usage */
  bsApiHistory apihistory;
} bsBrickOwl;

#define BS_CWD_PATH_MAX (1024)

typedef struct
{
  /* Output target */
  ioLog output;
  int32_t contextflags;

  /* bsFileState flags */
  int32_t stateflags;

#if BS_ENABLE_ANTIDEBUG
  int64_t antidebugtime0;
  int32_t antidebugcaps;
  int32_t antidebugcount1;
#endif

  /* Backup/error trackers */
  int32_t backupindex;
  int32_t errorindex;

  /* Priceguide cache storage */
  char *priceguidepath;
  int priceguideflags;
  int priceguidecachetime;

  /* User options */
  int retainemptylotsflag;
  int checkmessageflag;

#if BS_ENABLE_LIMITS
  int64_t limitinvhardmaxmask;
#endif

#if BS_ENABLE_MATHPUZZLE
  bsPuzzleSolution puzzlesolution;
#endif

  /* Service data */
  bsBrickLink bricklink;
  bsBrickOwl brickowl;

  /* Other state */
  oauthRandState32 oauthrand;

#if BS_ENABLE_ANTIDEBUG
  int64_t antidebugtime1;
  int32_t antidebugvalue;
  int32_t antidebugzero;
#endif

  /* TCP networking context */
  tcpContext tcp;

  /* Tracked local inventory */
  bsxInventory *inventory;

#if BS_ENABLE_MATHPUZZLE
  int puzzlequestiontype;
  int32_t antidebugpuzzle0;
#endif

  /* Latest time */
  time_t curtime;

  /* Version of latest state saved */
  int32_t lastrunversion;
  /* Time of latest state saved */
  int64_t lastruntime;

  /* List of completed queries */
  mmListDualHead replylist;

#if BS_ENABLE_MATHPUZZLE
  bsPuzzleBundle *puzzlebundle;
#endif

  /* BLID <-> BOID Translation Table */
  translationTable translationtable;

#if BS_ENABLE_ANTIDEBUG
  int64_t antidebugtimediff;
  int32_t antidebugarch;
  int32_t antidebugcount0;
#endif

#if BS_ENABLE_REGISTRATION
  /* Registration stuff */
  char *registrationcode;
  char *registrationkey;
  /* Decrypted offset to jump to bsCheckRegistered() */
  uint32_t registrationsecretoffset;
#endif

#if BS_ENABLE_MATHPUZZLE
  int puzzleprevquestiontype;
  int32_t antidebugpuzzle1;
#endif

#if BS_ENABLE_LIMITS
  int64_t limitwarningtime;
#endif

  /* BrickSync server HTTP connection */
  char *bricksyncwebaddress;
  httpConnection *bricksyncwebhttp;

  /* Current message, bsMessage* */
  time_t messagetime;
  void *message;
  int64_t lastmessagetimestamp;

  /* Free disk space check */
  time_t diskspacechecktime;

  /* Search time for "findorder" */
  int64_t findordertime;

#if BS_ENABLE_MATHPUZZLE
  bsPuzzleAnswer puzzleanswer;
#endif

  /* General random number generator */
  randState randstate;

  /* Current working directory */
  char cwd[BS_CWD_PATH_MAX];

  /* User details */
  char *username;
  char *storename;
  char *storecurrency;

} bsContext;

/* Is automatic mode enabled? */
#define BS_CONTEXT_FLAGS_AUTOCHECK_MODE (0x1)

/* Delayed low-priority state file update */
#define BS_CONTEXT_FLAGS_UPDATED_STATE (0x10)

/* Delayed minor inventory change, such as a BlLotID or such */
#define BS_CONTEXT_FLAGS_UPDATED_INVENTORY (0x20)

/* New BrickSync message is pending */
#define BS_CONTEXT_FLAGS_NEW_MESSAGE (0x40)

/* User has a registered copy of BrickSync, or so he pretends, eheh */
#define BS_CONTEXT_FLAGS_REGISTERED (0x100)
/* Dummy flags are always zero, they are just there to confuse REGISTERED flag check */
#define BS_CONTEXT_FLAGS_DUMMY0 (0x200)
#define BS_CONTEXT_FLAGS_DUMMY1 (0x400)
#define BS_CONTEXT_FLAGS_DUMMY2 (0x800)

/* High duplicate of flags, beginning at 0x4000 */
#define BS_CONTEXT_FLAGS_DUP_SHIFT (14)
#define BS_CONTEXT_FLAGS_DUP_REGISTERED (BS_CONTEXT_FLAGS_REGISTERED<<BS_CONTEXT_FLAGS_DUP_SHIFT)
#define BS_CONTEXT_FLAGS_DUP_DUMMY0 (BS_CONTEXT_FLAGS_DUMMY0<<BS_CONTEXT_FLAGS_DUP_SHIFT)
#define BS_CONTEXT_FLAGS_DUP_DUMMY1 (BS_CONTEXT_FLAGS_DUMMY1<<BS_CONTEXT_FLAGS_DUP_SHIFT)
#define BS_CONTEXT_FLAGS_DUP_DUMMY2 (BS_CONTEXT_FLAGS_DUMMY2<<BS_CONTEXT_FLAGS_DUP_SHIFT)


/* We still need a sync with BrickOwl */
#define BS_STATE_FLAGS_BRICKOWL_INITSYNC (0x1)
/* BrickLink master mode, all operations suspended */
#define BS_STATE_FLAGS_BRICKLINK_MASTER_MODE (0x2)

/* Must check service for new orders */
#define BS_STATE_FLAGS_BRICKLINK_MUST_CHECK (0x10)
#define BS_STATE_FLAGS_BRICKOWL_MUST_CHECK (0x20)

/* Tracked inventory was updated, flag becomes MUSTSYNC if update is interrupted or incomplete */
#define BS_STATE_FLAGS_BRICKLINK_MUST_UPDATE (0x100)
#define BS_STATE_FLAGS_BRICKOWL_MUST_UPDATE (0x200)

/* Tracked inventory was updated, service is out-of-sync */
#define BS_STATE_FLAGS_BRICKLINK_MUST_SYNC (0x1000)
#define BS_STATE_FLAGS_BRICKOWL_MUST_SYNC (0x2000)

/* Service is volountarily in partial sync, due to low API calls; perform real SYNC when apihistory allows it */
#define BS_STATE_FLAGS_BRICKLINK_PARTIAL_SYNC (0x10000)
#define BS_STATE_FLAGS_BRICKOWL_PARTIAL_SYNC (0x20000)


////


typedef struct
{
  /* Pointer to BrickStore context */
  bsContext *context;
  /* Opaque pointer to custom data for reply callback */
  void *opaquepointer;
  /* Query type */
  int type;
  /* Extra data, order ID or lot ID */
  int64_t extid;
  /* Extra pointer, bsOrder* bsOrder* */
  void *extpointer;
  /* Query result code */
  int result;
  /* Linked list node */
  mmListNode list;
} bsQueryReply;

enum
{
  BS_QUERY_TYPE_BRICKLINK,
  BS_QUERY_TYPE_WEBBRICKLINK,
  BS_QUERY_TYPE_BRICKOWL,
  BS_QUERY_TYPE_OTHER,

  BS_QUERY_TYPE_COUNT
};


typedef struct
{
  /* Success/error tracking */
  int errorcount;
  int successcount;
  int failureflag;
  int mustsyncflag;
  /* Connection */
  httpConnection *http;
} bsTracker;

typedef struct
{
  /* List iteration */
  int liststart;
  mmBitMap bitmap;
} bsWorkList;


/* Defined in bricksyncnet.c */

bsQueryReply *bsAllocReply( bsContext *context, int type, int64_t extid, void *extpointer, void *opaquepointer );
void bsFreeReply( bsContext *context, bsQueryReply *reply );

void bsBrickLinkAddQuery( bsContext *context, char *methodstring, char *pathstring, char *paramstring, char *bodystring, void *uservalue, void (*querycallback)( void *uservalue, int resultcode, httpResponse *response ) );
void bsBrickOwlAddQuery( bsContext *context, char *querystring, int httpflags, void *uservalue, void (*querycallback)( void *uservalue, int resultcode, httpResponse *response ) );

/* Flush tcp callbacks and process all http connections */
void bsFlushTcpProcessHttp( bsContext *context );

/* Wait until pending HTTP queries have been completed */
void bsWaitBrickLinkQueries( bsContext *context, int maxpending );
void bsWaitBrickLinkWebQueries( bsContext *context, int maxpending );
void bsWaitBrickOwlQueries( bsContext *context, int maxpending );
void bsWaitBrickSyncWebQueries( bsContext *context, int maxpending );

/* Connection status generic handling */
void bsTrackerInit( bsTracker *tracker, httpConnection *http );
int bsTrackerAccumResult( bsContext *context, bsTracker *tracker, int httpresult, int accumflags );
int bsTrackerProcessGenericReplies( bsContext *context, bsTracker *tracker, int allowretryflag );

#define BS_TRACKER_ACCUM_FLAGS_CANSYNC (0x1)
#define BS_TRACKER_ACCUM_FLAGS_CANRETRY (0x2)
/* Don't increment error count even if the httpresult indicates a mild failure */
#define BS_TRACKER_ACCUM_FLAGS_NO_ERROR (0x4)

int bsQueryBrickOwlUserDetails( bsContext *context, boUserDetails *userdetails );


////


/* Defined in bricsyncconf.c */

int bsConfLoad( bsContext *context, char *path );



/* Defined in bricsync.c */

void bsFatalError( bsContext *context );

/* Filter out all items with '~' in remarks and repack */
void bsInventoryFilterOutItems( bsContext *context, bsxInventory *inv );

/* Generate an unique ExtID for item */
void bsItemSetUniqueExtID( bsContext *context, bsxInventory *inv, bsxItem *item );

int bsBrickLinkCanLoadOrder( bsContext *context, bsOrder *order );
bsxInventory *bsBrickLinkLoadOrder( bsContext *context, bsOrder *order );
int bsBrickLinkSaveOrder( bsContext *context, bsOrder *order, bsxInventory *inv, journalDef *journal );
int bsBrickOwlCanLoadOrder( bsContext *context, bsOrder *order );
int bsBrickOwlSaveOrder( bsContext *context, bsOrder *order, bsxInventory *inv, journalDef *journal );

/* Store backup path, returned string must be free()'d */
char *bsInventoryBackupPath( bsContext *context, int tempflag );
/* Store backup */
int bsStoreBackup( bsContext *context, journalDef *journal );

/* Store error path, returned string must be free()'d */
char *bsErrorStoragePath( bsContext *context, int tempflag );
/* Store error */
int bsStoreError( bsContext *context, char *errortype, char *header, size_t headerlength, void *data, size_t datasize );

int bsSaveInventory( bsContext *context, journalDef *journal );
int bsSaveState( bsContext *context, journalDef *journal );



/* Defined in bricsyncinit.c */

int bsInitBrickLink( bsContext *context );

int bsInitBrickOwl( bsContext *context );




/* Defined in bsfetchorderlist.c */

/* Query BrickLink's orderlist */
int bsQueryBickLinkOrderList( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate );
/* Query BrickOwl's orderlist */
int bsQueryBickOwlOrderList( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate );

/* Upgrade BrickLink's orderlist */
int bsQueryUpgradeBickLinkOrderList( bsContext *context, bsOrderList *orderlist, int infolevel );
/* Upgrade BrickLink's orderlist */
int bsQueryUpgradeBickOwlOrderList( bsContext *context, bsOrderList *orderlist, int infolevel );

/* Strip stuff out of orderlist */
void bsOrderListFilterOutDate( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate );
int bsOrderListFilterOutDateConditional( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate, int64_t minimumchangedate, int (*condition)( bsContext *context, bsOrder *order ) );



/* Defined in bsfetchinv.c */

/* Query the inventory from BrickLink */
bsxInventory *bsQueryBrickLinkInventory( bsContext *context );
/* Query the inventory from BrickOwl */
bsxInventory *bsQueryBrickOwlInventory( bsContext *context );

/* Query BrickLink inventory and orderlist at the moment the inventory was taken, return 0 on failure */
bsxInventory *bsQueryBrickLinkFullState( bsContext *context, bsOrderList *orderlist );
/* Query BrickOwl inventory and orderlist at the moment the inventory was taken, return 0 on failure */
bsxInventory *bsQueryBrickOwlFullState( bsContext *context, bsOrderList *orderlist, int64_t minimumorderdate );




/* Defined in bsapplydiff.c */

/* Query BrickLink, apply updates for whole diff inventory, retryflag is set if failed due to NOREPLY to a !RETRY query */
int bsQueryBrickLinkApplyDiff( bsContext *context, bsxInventory *diffinv, int *retryflag );
/* Query BrickOwl, apply updates for whole diff inventory, retryflag is set if failed due to NOREPLY to a !RETRY query */
int bsQueryBrickOwlApplyDiff( bsContext *context, bsxInventory *diffinv, int *retryflag );




/* Defined in bsfetchorderinv.c */

/* Fetch orders from orderlist >= basetimestamp, call callback for each, callback must not free 'inv' */
int bsFetchBrickLinkOrders( bsContext *context, bsOrderList *orderlist, int64_t basetimestamp, int64_t pendingtimestamp, void *uservalue, int (*processorder)( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue ) );
/* Fetch orders from orderlist >= basetimestamp, call callback for each, callback must not free 'inv' */
int bsFetchBrickOwlOrders( bsContext *context, bsOrderList *orderlist, int64_t basetimestamp, int64_t pendingtimestamp, void *uservalue, int (*processorder)( bsContext *context, bsOrder *order, bsxInventory *inv, void *uservalue ) );




/* Defined in bsresolve.c */

/* Query BrickOwl for all missing BOIDs of inventory, flag to skip items with known bolotid */
int bsQueryBrickOwlLookupBoids( bsContext *context, bsxInventory *inv, int resolveflags, int *lookupcounts );
/* Lookup items with missing BOIDs in the translation database */
void bsResolveBrickOwlBoids( bsContext *context, bsxInventory *inv, int *lookupcounts );

/* Flags from 0x10000 and up are reserved for internal use */
#define BS_RESOLVE_FLAGS_SKIPBOLOTID (0x1)
#define BS_RESOLVE_FLAGS_PRINTOUT (0x2)
#define BS_RESOLVE_FLAGS_FORCEQUERY (0x4)
#define BS_RESOLVE_FLAGS_TRYFALLBACK (0x8)
#define BS_RESOLVE_FLAGS_TRYALL (0x10)




/* Defined in bscatedit.c */

/* Submit a BrickOwl catalog edit, new BLID for BOID */
int bsSubmitBrickOwlEditBlid( bsContext *context, bsxItem *item );
/* Submit a BrickOwl catalog edit, new length for BOID */
int bsSubmitBrickOwlEditLength( bsContext *context, bsxItem *item, int itemlength );
/* Submit a BrickOwl catalog edit, new width for BOID */
int bsSubmitBrickOwlEditWidth( bsContext *context, bsxItem *item, int itemwidth );
/* Submit a BrickOwl catalog edit, new height for BOID */
int bsSubmitBrickOwlEditHeight( bsContext *context, bsxItem *item, int itemheight );
/* Submit a BrickOwl catalog edit, new weight for BOID */
int bsSubmitBrickOwlEditWeight( bsContext *context, bsxItem *item, float itemweight );




/* Defined in bricksyncinput.c */

int bsParseInput( bsContext *context, int *retactionflag );

void bsCommandStatus( bsContext *context, int argc, char **argv );
void bsCommandRegister( bsContext *context, int argc, char **argv );
void bsCommandPruneBackups( bsContext *context, int argc, char **argv );




/* Defined in bscheck.c */

int bsCheckBrickLink( bsContext *context );
int bsCheckBrickOwl( bsContext *context );
void bsCheckBrickLinkState( bsContext *context );
void bsCheckBrickOwlState( bsContext *context );
void bsCheckGlobal( bsContext *context );


/* Defined in bscheckreg.c */

#if BS_ENABLE_REGISTRATION
void bsCheckRegistered( bsContext *context );
#endif



/* Defined in bssync.c */

typedef struct
{
  int skipflag_partcount;
  int skipflag_lotcount;
  int skipunknown_partcount;
  int skipunknown_lotcount;
  int deleteduplicate_partcount;
  int deleteduplicate_lotcount;
  int deleteorphan_partcount;
  int deleteorphan_lotcount;
  int match_partcount;
  int match_lotcount;
  int missing_partcount;
  int missing_lotcount;
  int extra_partcount;
  int extra_lotcount;
  int updatedata_partcount;
  int updatedata_lotcount;
} bsSyncStats;

bsxInventory *bsSyncComputeDeltaInv( bsContext *context, bsxInventory *stockinv, bsxInventory *inv, bsSyncStats *stats, int deltamode );

enum
{
  BS_SYNC_DELTA_MODE_BRICKLINK,
  BS_SYNC_DELTA_MODE_BRICKOWL,
  BS_SYNC_DELTA_MODE_COUNT
};


int bsSyncBrickLink( bsContext *context, bsSyncStats *stats );
int bsSyncBrickOwl( bsContext *context, bsSyncStats *stats );

void bsSyncPrintSummary( bsContext *context, bsSyncStats *stats, int brickowlflag );


typedef struct
{
  int create_partcount;
  int create_lotcount;
  int delete_partcount;
  int delete_lotcount;
  int added_partcount;
  int added_lotcount;
  int removed_partcount;
  int removed_lotcount;
} bsMergeInvStats;

typedef struct
{
  int match_partcount;
  int match_lotcount;
  int missing_partcount;
  int missing_lotcount;
  int updated_partcount;
  int updated_lotcount;
} bsMergeUpdateStats;

int bsMergeInv( bsContext *context, bsxInventory *bsx, bsMergeInvStats *stats, int mergeflags );
int bsMergeLoadPrices( bsContext *context, bsxInventory *inv, bsMergeUpdateStats *stats, int mergeflags );
int bsMergeLoadNotes( bsContext *context, bsxInventory *inv, bsMergeUpdateStats *stats, int mergeflags );
int bsMergeLoad( bsContext *context, bsxInventory *inv, bsMergeUpdateStats *stats, int mergeflags );

#define BS_MERGE_FLAGS_PRICE (0x1)
#define BS_MERGE_FLAGS_COMMENTS (0x2)
#define BS_MERGE_FLAGS_REMARKS (0x4)
#define BS_MERGE_FLAGS_BULK (0x8)
#define BS_MERGE_FLAGS_MYCOST (0x10)
#define BS_MERGE_FLAGS_TIERPRICES (0x20)

#define BS_MERGE_FLAGS_UPDATE_BRICKLINK (0x10000)
#define BS_MERGE_FLAGS_UPDATE_BRICKOWL (0x20000)



/* Defined in bsapihistory.c */

void bsApiHistoryReset( bsContext *context, bsApiHistory *history );
void bsApiHistoryIncrement( bsContext *context, bsApiHistory *history );
void bsApiHistoryUpdate( bsContext *context );
int bsApiHistoryCountPeriod( bsApiHistory *history, int64_t seconds );



/* Defined in bsfetchset.c */

/* Fetch the inventory for a set ID */
bsxInventory *bsBrickLinkFetchSetInventory( bsContext *context, char itemtypeid, char *setid );



/* Defined in bsfetchpriceguide.c */

/* Populate the price guide cache with all items flagged BSX_ITEM_XFLAGS_FETCH_PRICE_GUIDE */
int bsBrickLinkFetchPriceGuide( bsContext *context, bsxInventory *inv, void *callbackpointer, void (*callback)( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer ) );


/* Defined in bsoutputxml.c */

void bsClearBrickLinkXML( bsContext *context );
int bsOutputBrickLinkXML( bsContext *context, bsxInventory *inv, int apiwarningflag, int itemforceflags );



/* Defined in bspriceguice.c */

typedef struct
{
  int quantity;
  double value;
  double sale;
  double price;
  double pgq;
  double pgp;
  int stockflag;
  int itemindex;
} bsPriceGuideAlt;

typedef struct
{
  bsxInventory *inv;
  int showaltflag;
  int removealtflag;
  int altcount;
  bsPriceGuideAlt *altlist;

  int stocklots;
  int stockcount;
  double stockvalue;
  double stocksale;
  double stockprice;
  double stockpgq;
  double stockpgp;
  int newlots;
  int newcount;
  double newvalue;
  double newsale;
  double newprice;
  double newpgq;
  double newpgp;
  int totallots;
  int totalcount;
  double totalvalue;
  double totalsale;
  double totalprice;
  double totalpgq;
  double totalpgp;
} bsPriceGuideState;

int bsProcessInventoryPriceGuide( bsContext *context, bsxInventory *inv, int cachetime, void *callbackpointer, void (*callback)( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer ) );

void bsPriceGuideInitState( bsPriceGuideState *pgstate, bsxInventory *inv, int showaltflag, int removealtflag );
void bsPriceGuideFinishState( bsPriceGuideState *pgstate );
void bsPriceGuideSumCallback( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer );
void bsPriceGuideListRangeCallback( bsContext *context, bsxInventory *inv, bsxItem *item, bsxPriceGuide *pg, void *callbackpointer );



/* Defined in bsregister.c */

#if BS_ENABLE_REGISTRATION
int bsRegistrationInit( bsContext *context, char *regkeystring );
#endif



/* Defined in bsmathpuzzle.c */

#if BS_ENABLE_MATHPUZZLE
int bsPuzzleAsk( bsContext *context, bsPuzzleBundle *puzzlebundle );
#endif



/* Defined in bsmastermode.c */

void bsEnterBrickLinkMasterMode( bsContext *context );
void bsExitBrickLinkMasterMode( bsContext *context );



/* Defined in bsmessage.c */

#define BS_MESSAGE_SIGNATURE_LENGTH (8)
#define BS_MESSAGE_LENGTH_MAX (512)

typedef struct
{
  /* Signature */
  uint8_t signature[BS_MESSAGE_SIGNATURE_LENGTH];
  /* Unique identifier */
  int64_t timestamp;
  /* Latest version */
  int8_t latestversionmajor;
  int8_t latestversionminor;
  int8_t latestversionrevision;
  /* Minimum version */
  int8_t minimumversionmajor;
  int8_t minimumversionminor;
  int8_t minimumversionrevision;
  /* Message priority */
  uint8_t messagepriority;
  /* Message flags */
  uint8_t messageflags;
  /* Reserved data */
  uint8_t reserved[32];
  /* Message */
  char message[BS_MESSAGE_LENGTH_MAX];
} bsMessage;

enum
{
  BS_MESSAGE_PRIORITY_NONE,
  BS_MESSAGE_PRIORITY_LOW,
  BS_MESSAGE_PRIORITY_MEDIUM,
  BS_MESSAGE_PRIORITY_HIGH,
  BS_MESSAGE_PRIORITY_CRITICAL,

  BS_MESSAGE_PRIORITY_COUNT
};

#define BS_MESSAGE_FLAGS_REQ_REGISTERED (0x1)
#define BS_MESSAGE_FLAGS_REQ_UNREGISTERED (0x2)
#define BS_MESSAGE_FLAGS_REQ_NOT_LATESTVERSION (0x4)
#define BS_MESSAGE_FLAGS_REQ_NOT_MINIMUMVERSION (0x4)

void bsFetchBrickSyncMessage( bsContext *context );
void bsReadBrickSyncMessage( bsContext *context );



/* Defined in bsorderdir.c */

typedef struct
{
  char *filepath;
  char *filename;
  time_t filetime;
  int ordertype;
  int orderid;
} bsOrderDirEntry;

typedef struct
{
  bsOrderDirEntry *entrylist;
  int entrycount;
  int entryalloc;
} bsOrderDir;

int bsReadOrderDir( bsContext *context, bsOrderDir *orderdir, time_t ordermintime );
void bsFreeOrderDir( bsContext *context, bsOrderDir *orderdir );
void bsSortOrderDir( bsContext *context, bsOrderDir *orderdir );

enum
{
  BS_ORDER_DIR_ORDER_TYPE_BRICKLINK,
  BS_ORDER_DIR_ORDER_TYPE_BRICKOWL,
  BS_ORDER_DIR_ORDER_TYPE_UNKNOWN,

  BS_ORDER_DIR_ORDER_TYPE_COUNT
};


////



#define BSMSG_ERROR IO_RED "ERROR" IO_WHITE ": "
#define BSMSG_WARNING IO_YELLOW "WARNING" IO_WHITE ": "
#define BSMSG_INIT IO_CYAN "INIT" IO_DEFAULT ": "
#define BSMSG_INFO IO_GREEN "INFO" IO_DEFAULT ": "
#define BSMSG_QUESTION IO_MAGENTA "QUESTION" IO_WHITE ": "
#define BSMSG_COMMAND IO_BLUE "COMMAND" IO_DEFAULT ": "
#define BSMSG_ANSWER IO_CYAN "ANSWER" IO_DEFAULT ": "
#define BSMSG_DEBUG IO_BLUE "DEBUG" IO_DEFAULT ": "


/* Return 1 if the item should be filtered out */
static inline int bsInvItemFilterFlag( bsContext *context, bsxItem *item )
{
  if( ( item->remarks ) && ( ccStrFindChar( item->remarks, '~' ) >= 0 ) )
    return 1;
  return 0;
}


static inline int bsInvPriceEqual( float oldprice, float newprice )
{
  float absdiff;
  absdiff = fabsf( newprice - oldprice );
  /* Equal if difference is less than 0.001 */
  if( absdiff < 0.0015 )
    return 1;
  /* Equal if difference is less than 0.01% of higher */
  if( absdiff < ( 0.0001 * fmaxf( newprice, oldprice ) ) )
    return 1;
  return 0;
}


static inline int bsInvItemTierEqual( bsxItem *item0, bsxItem *item1 )
{
  if( item0->tq1 != item1->tq1 )
    return 0;
  if( item0->tq2 != item1->tq2 )
    return 0;
  if( item0->tq3 != item1->tq3 )
    return 0;
  if( !( bsInvPriceEqual( item0->tp1, item1->tp1 ) ) )
    return 0;
  if( !( bsInvPriceEqual( item0->tp2, item1->tp2 ) ) )
    return 0;
  if( !( bsInvPriceEqual( item0->tp3, item1->tp3 ) ) )
    return 0;
  return 1;
}


/* -----------------------------------------------------------------------------
 *
 * atomictest: checks the atomic operations of mmatomic.h.
 *
 * x86 builds use its assembly versions, other CPUs (Apple Silicon) its __atomic versions; both must
 * give the same results, so the same checks run on every build:
 * - every operation's result and return value, for 8, 16, 32 and 64 bits (wrap-around included);
 * - compare-exchange returns the value it found, and only swaps on a match;
 * - the spin-wait counters return what is left, 0 when they give up;
 * - the read/write lock helpers built on top;
 * - four threads adding at once lose no increment.
 * Exit code 0 when every case behaves, 1 otherwise. Built and run by build.sh test / CI.
 *
 * -----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
 #include <windows.h>
#else
 #include <pthread.h>
#endif

#include "mmatomic.h"


#if !defined(MM_ATOMIC_SUPPORT)
 #error "mmatomic.h gives no atomic support on this CPU"
#endif


static int failures = 0;

static void check( int ok, const char *what )
{
  if( !( ok ) )
  {
    printf( "FAIL: %s\n", what );
    failures++;
  }
  else
    printf( "ok: %s\n", what );
  return;
}


#define TEST_WIDTH(N,T,MAXV,MINV) \
static void testWidth##N( void ) \
{ \
  mmAtomic##N a; \
  mmAtomicWrite##N( &a, 5 ); \
  check( mmAtomicRead##N( &a ) == 5, #N "-bit write/read" ); \
  mmAtomicBarrierWrite##N( &a, 7 ); \
  check( mmAtomicRead##N( &a ) == 7, #N "-bit barrier write" ); \
  mmAtomicAdd##N( &a, 3 ); \
  check( mmAtomicRead##N( &a ) == 10, #N "-bit add" ); \
  mmAtomicSub##N( &a, 4 ); \
  check( mmAtomicRead##N( &a ) == 6, #N "-bit sub" ); \
  check( !mmAtomicAddTestZero##N( &a, 1 ) && mmAtomicAddTestZero##N( &a, -7 ), #N "-bit add test zero" ); \
  mmAtomicWrite##N( &a, 2 ); \
  check( !mmAtomicSubTestZero##N( &a, 1 ) && mmAtomicSubTestZero##N( &a, 1 ), #N "-bit sub test zero" ); \
  mmAtomicInc##N( &a ); \
  mmAtomicInc##N( &a ); \
  mmAtomicDec##N( &a ); \
  check( mmAtomicRead##N( &a ) == 1, #N "-bit inc/dec" ); \
  mmAtomicWrite##N( &a, -2 ); \
  check( !mmAtomicIncTestZero##N( &a ) && mmAtomicIncTestZero##N( &a ), #N "-bit inc test zero" ); \
  mmAtomicWrite##N( &a, 2 ); \
  check( !mmAtomicDecTestZero##N( &a ) && mmAtomicDecTestZero##N( &a ), #N "-bit dec test zero" ); \
  mmAtomicWrite##N( &a, 1 ); \
  check( !mmAtomicAddTestNegative##N( &a, 1 ) && mmAtomicAddTestNegative##N( &a, -3 ), #N "-bit add test negative" ); \
  mmAtomicWrite##N( &a, 1 ); \
  check( !mmAtomicSubTestNegative##N( &a, 1 ) && mmAtomicSubTestNegative##N( &a, 1 ), #N "-bit sub test negative" ); \
  mmAtomicWrite##N( &a, MAXV ); \
  check( mmAtomicAddTestNegative##N( &a, 1 ) && ( mmAtomicRead##N( &a ) == MINV ), #N "-bit add wraps to negative" ); \
  mmAtomicWrite##N( &a, 0x0f ); \
  mmAtomicAnd##N( &a, 0x3c ); \
  check( mmAtomicRead##N( &a ) == 0x0c, #N "-bit and" ); \
  check( !mmAtomicAndTestZero##N( &a, 0x04 ) && mmAtomicAndTestZero##N( &a, 0x10 ), #N "-bit and test zero" ); \
  mmAtomicOr##N( &a, 0x21 ); \
  check( mmAtomicRead##N( &a ) == 0x21, #N "-bit or" ); \
  mmAtomicWrite##N( &a, 0 ); \
  check( mmAtomicOrTestZero##N( &a, 0 ) && !mmAtomicOrTestZero##N( &a, 0x02 ), #N "-bit or test zero" ); \
  mmAtomicXor##N( &a, 0x03 ); \
  check( mmAtomicRead##N( &a ) == 0x01, #N "-bit xor" ); \
  check( !mmAtomicXorTestZero##N( &a, 0x03 ) && mmAtomicXorTestZero##N( &a, 0x02 ), #N "-bit xor test zero" ); \
  mmAtomicWrite##N( &a, 9 ); \
  check( ( mmAtomicXchg##N( &a, 4 ) == 9 ) && ( mmAtomicRead##N( &a ) == 4 ), #N "-bit exchange returns the old value" ); \
  check( ( mmAtomicCmpXchg##N( &a, 4, 6 ) == 4 ) && ( mmAtomicRead##N( &a ) == 6 ), #N "-bit compare-exchange swaps on a match" ); \
  check( ( mmAtomicCmpXchg##N( &a, 4, 8 ) == 6 ) && ( mmAtomicRead##N( &a ) == 6 ), #N "-bit compare-exchange keeps the value otherwise" ); \
  check( mmAtomicCmpReplace##N( &a, 6, 1 ) && !mmAtomicCmpReplace##N( &a, 6, 2 ) && ( mmAtomicRead##N( &a ) == 1 ), #N "-bit compare-replace" ); \
  check( ( mmAtomicAddRead##N( &a, 2 ) == 3 ) && ( mmAtomicReadAdd##N( &a, 2 ) == 3 ) && ( mmAtomicRead##N( &a ) == 5 ), #N "-bit add-read / read-add" ); \
  mmAtomicSpinWaitEq##N( &a, 5 ); \
  mmAtomicSpinWaitNeq##N( &a, 4 ); \
  check( mmAtomicSpinWaitEq##N##Count( &a, 5, 10 ) == 10, #N "-bit spin wait equal: met at once, count kept" ); \
  check( mmAtomicSpinWaitEq##N##Count( &a, 4, 10 ) == 0, #N "-bit spin wait equal: gives up with 0" ); \
  check( mmAtomicSpinWaitNeq##N##Count( &a, 4, 10 ) == 10, #N "-bit spin wait not equal: met at once" ); \
  check( mmAtomicSpinWaitNeq##N##Count( &a, 5, 10 ) == 0, #N "-bit spin wait not equal: gives up with 0" ); \
  mmAtomicSpin##N( &a, 5, 0 ); \
  check( mmAtomicRead##N( &a ) == 0, #N "-bit spin swap" ); \
  check( !mmAtomicTrySpin##N( &a, 3, 1, 10 ) && mmAtomicTrySpin##N( &a, 0, 1, 10 ), #N "-bit try spin" ); \
  return; \
}

TEST_WIDTH(8,int8_t,INT8_MAX,INT8_MIN)
TEST_WIDTH(16,int16_t,INT16_MAX,INT16_MIN)
TEST_WIDTH(32,int32_t,INT32_MAX,INT32_MIN)
#ifdef MM_ATOMIC_64_BITS_SUPPORT
TEST_WIDTH(64,int64_t,INT64_MAX,INT64_MIN)
#endif


static void testLocks( void )
{
  mmAtomicLock32 lock;
  mmAtomicLockInit32( &lock );
  check( mmAtomicLockAttemptRead32( &lock ) && mmAtomicLockAttemptRead32( &lock ), "two readers share the lock" );
  check( !mmAtomicLockAttemptWrite32( &lock ), "no writer while readers hold it" );
  mmAtomicLockDoneRead32( &lock );
  mmAtomicLockDoneRead32( &lock );
  check( mmAtomicLockAttemptWrite32( &lock ), "writer gets the free lock" );
  check( !mmAtomicLockAttemptRead32( &lock ) && !mmAtomicLockAttemptWrite32( &lock ), "nobody else while a writer holds it" );
  check( !mmAtomicLockTryRead32( &lock, 5 ), "try-read gives up while written" );
  mmAtomicLockDoneWrite32( &lock );
  check( mmAtomicLockTryRead32( &lock, 5 ), "try-read gets it once released" );
  mmAtomicLockDoneRead32( &lock );
  check( mmAtomicRead32( &lock.v ) == 0, "lock back to free" );
  return;
}


////


#define THREAD_COUNT (4)
#define THREAD_LOOPS (200000)

static mmAtomic32 sharedcount32;
#ifdef MM_ATOMIC_64_BITS_SUPPORT
static mmAtomic64 sharedcount64;
#endif
static mmAtomic32 sharedspin;
static volatile int32_t guardedcount;

static void threadWork( void )
{
  int i;
  for( i = 0 ; i < THREAD_LOOPS ; i++ )
  {
    mmAtomicInc32( &sharedcount32 );
#ifdef MM_ATOMIC_64_BITS_SUPPORT
    mmAtomicAdd64( &sharedcount64, 2 );
#endif
    /* A spin lock made of compare-exchange: the plain counter inside must not lose updates */
    mmAtomicSpin32( &sharedspin, 0, 1 );
    guardedcount = guardedcount + 1;
    mmAtomicWrite32( &sharedspin, 0 );
  }
  return;
}

#if defined(_WIN32)
static DWORD WINAPI threadMain( LPVOID arg )
{
  threadWork();
  return 0;
}
#else
static void *threadMain( void *arg )
{
  threadWork();
  return 0;
}
#endif

static void testThreads( void )
{
  int i, started;
#if defined(_WIN32)
  HANDLE threads[THREAD_COUNT];
#else
  pthread_t threads[THREAD_COUNT];
#endif

  mmAtomicWrite32( &sharedcount32, 0 );
#ifdef MM_ATOMIC_64_BITS_SUPPORT
  mmAtomicWrite64( &sharedcount64, 0 );
#endif
  mmAtomicWrite32( &sharedspin, 0 );
  guardedcount = 0;
  started = 0;
  for( i = 0 ; i < THREAD_COUNT ; i++ )
  {
#if defined(_WIN32)
    threads[i] = CreateThread( 0, 0, threadMain, 0, 0, 0 );
    if( !( threads[i] ) )
      break;
#else
    if( pthread_create( &threads[i], 0, threadMain, 0 ) )
      break;
#endif
    started++;
  }
  for( i = 0 ; i < started ; i++ )
  {
#if defined(_WIN32)
    WaitForSingleObject( threads[i], INFINITE );
    CloseHandle( threads[i] );
#else
    pthread_join( threads[i], 0 );
#endif
  }
  check( started == THREAD_COUNT, "four threads started" );
  check( mmAtomicRead32( &sharedcount32 ) == started * THREAD_LOOPS, "threads: no 32-bit increment lost" );
#ifdef MM_ATOMIC_64_BITS_SUPPORT
  check( mmAtomicRead64( &sharedcount64 ) == (int64_t)started * THREAD_LOOPS * 2, "threads: no 64-bit add lost" );
#endif
  check( guardedcount == started * THREAD_LOOPS, "threads: the spin lock let one thread in at a time" );
  return;
}


int main( void )
{
  testWidth8();
  testWidth16();
  testWidth32();
#ifdef MM_ATOMIC_64_BITS_SUPPORT
  testWidth64();
#endif
  testLocks();
  testThreads();
  if( failures )
  {
    printf( "atomictest: %d check(s) FAILED\n", failures );
    return 1;
  }
  printf( "atomictest: all checks passed\n" );
  return 0;
}

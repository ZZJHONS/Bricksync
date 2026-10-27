#!/bin/sh
# Builds BrickSync. This is the ONE place for the source list, compiler flags, libraries and
# version stamp: the GitHub Actions workflow and every hand build (compile, compile32,
# compile-win64.bat, compile-win32.bat) run it, so they cannot drift apart.
#
#   ./build.sh [TARGET] [test]
#
# TARGET: linux64, linux32, win64 or win32. Default: what this shell builds for
#         (Linux -> linux64; an MSYS2 MINGW64 shell -> win64, MINGW32 -> win32).
# test:   also build and run the tests: journaltest (crash recovery of file renames),
#         httptest (back-off on 429/5xx and Retry-After), stoptest (clean stop on
#         SIGINT/SIGTERM or Ctrl+C) and tlstest (needs internet: checks that servers are
#         authenticated).
#
# Environment:
#   BS_BUILD_ID   the stamp after the version, 1.7.4-<stamp>. Default: UTC date and time,
#                 YYYYMMDD.HHMM (e.g. 20261027.0930). CI passes YYYYMMDD.<run number>.
#   BS_PORTABLE=1 build for any CPU of the architecture instead of tuning for this machine
#                 (release builds use it).
#
# Needs gcc and OpenSSL 3 (headers + libraries). Linux: the distribution's packages
# (e.g. apt install gcc libssl-dev). Windows: MSYS2 (https://www.msys2.org), then in its
# MINGW64 shell: pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-openssl
# (MINGW32: mingw-w64-i686-toolchain mingw-w64-i686-openssl).
# On Windows OpenSSL is linked statically: bricksync.exe needs no OpenSSL DLLs.

set -e
cd "$(dirname "$0")"

target=""
runtest=0
for arg in "$@"; do
  case "$arg" in
    linux64|linux32|win64|win32) target="$arg" ;;
    test) runtest=1 ;;
    *) echo "build.sh: unknown argument '$arg' (expected linux64, linux32, win64, win32 or test)" >&2; exit 2 ;;
  esac
done
if [ -z "$target" ]; then
  case "${MSYSTEM:-}" in
    MINGW64|UCRT64|CLANG64) target=win64 ;;
    MINGW32) target=win32 ;;
    *) target=linux64 ;;
  esac
fi

stamp="${BS_BUILD_ID:-$(date -u +%Y%m%d.%H%M)}"

SOURCES="bricksync.c bricksyncconf.c bricksyncnet.c bsstop.c bricksyncinit.c bricksyncinput.c bsantidebug.c bsmessage.c bsmathpuzzle.c bsorder.c bsregister.c bsapihistory.c bstranslation.c bsevalgrade.c bsoutputxml.c bsorderdir.c bspriceguide.c bsmastermode.c bscheck.c bssync.c bsapplydiff.c bsfetchorderinv.c bsresolve.c bscatedit.c bsfetchinv.c bsfetchorderlist.c bsfetchset.c bscheckreg.c bsfetchpriceguide.c tcp.c vtlex.c cpuinfo.c antidebug.c mm.c mmhash.c mmbitmap.c cc.c ccstr.c debugtrack.c tcphttp.c oauth.c bricklink.c brickowl.c brickowlinv.c colortable.c json.c bsx.c bsxpg.c journal.c exclperm.c iolog.c crypthash.c cryptsha1.c rand.c bn512.c bn1024.c rsabn.c"
# The tests need only the code they check and what it uses
TEST_SOURCES="tlstest.c tcp.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
JOURNAL_TEST_SOURCES="journaltest.c journal.c iolog.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
HTTP_TEST_SOURCES="httptest.c tcphttp.c tcp.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
STOP_TEST_SOURCES="stoptest.c bsstop.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"

case "$target" in
  linux64|win64) bits=64 ;;
  *) bits=32 ;;
esac
CFLAGS="-std=gnu99 -m$bits -O2 -s -fvisibility=hidden"

case "$target" in
  linux*)
    exe=""
    LIBS="-lssl -lcrypto -lm -lpthread"
    ;;
  win*)
    exe=".exe"
    # Static OpenSSL; crypt32 serves the Windows certificate store
    LIBS="-static -lssl -lcrypto -lws2_32 -lwsock32 -lcrypt32 -luser32 -ladvapi32 -lgdi32 -lm"
    ;;
esac

cpuflags="-h"
[ "${BS_PORTABLE:-0}" = "1" ] && cpuflags="-h -ccenv"

echo "Building BrickSync for $target, version stamp $stamp"
gcc -std=gnu99 -m$bits cpuconf.c cpuinfo.c -O2 -s -o cpuconf$exe
./cpuconf$exe $cpuflags

res=""
case "$target" in
  win*)
    windres bricksync.rc -O coff -o bricksync.res
    res="bricksync.res"
    ;;
esac

gcc $CFLAGS $SOURCES $res -o bricksync$exe $LIBS -DBS_VERSION_BUILDTIME="$stamp"
echo "Built bricksync$exe"

if [ "$runtest" = "1" ]; then
  gcc $CFLAGS $JOURNAL_TEST_SOURCES -o journaltest$exe $LIBS
  ./journaltest$exe
  gcc $CFLAGS $HTTP_TEST_SOURCES -o httptest$exe $LIBS
  ./httptest$exe
  gcc $CFLAGS $STOP_TEST_SOURCES -o stoptest$exe $LIBS
  ./stoptest$exe
  gcc $CFLAGS $TEST_SOURCES -o tlstest$exe $LIBS
  ./tlstest$exe
fi

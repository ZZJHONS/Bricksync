#!/bin/sh
# Builds BrickSync. This is the ONE place for the source list, compiler flags, libraries and
# version stamp: the GitHub Actions workflow and every hand build (compile, compile32,
# compile-win64.bat, compile-win32.bat) run it, so they cannot drift apart.
#
#   ./build.sh [TARGET] [test]
#
# TARGET: linux64, linux32, win64, win32, macos-x64 (Intel Mac) or macos-arm64 (Apple Silicon).
#         Default: what this shell builds for (Linux -> linux64; an MSYS2 MINGW64 shell -> win64,
#         MINGW32 -> win32; a Mac -> its own chip).
# test:   also build and run the tests: atomictest (atomic operations, x86 and ARM alike), journaltest (crash recovery of file renames),
#         httptest (back-off on 429/5xx and Retry-After), stoptest (clean stop on
#         SIGINT/SIGTERM or Ctrl+C), broadcasttest (version check and broadcast message) and tlstest (needs internet: checks that servers are
#         authenticated).
#
# Environment:
#   BS_BUILD_ID   the stamp after the version, 1.7.4-<stamp>. Default: UTC date and time,
#                 YYYYMMDD.HHMM (e.g. 20261027.0930). CI passes YYYYMMDD.<run number>.
#   BS_RELEASE_TAG the release tag being built (v1.7.4-261028), for the version check; CI sets it
#                 when it builds a tag. Unset for every other build.
#   BS_PORTABLE=1 build for any CPU of the architecture instead of tuning for this machine
#                 (release builds use it).
#   OPENSSL_DIR   macOS only: an OpenSSL 3 install (include/, lib/libssl.a, lib/libcrypto.a) to link
#                 in. Default: Homebrew's openssl@3. CI builds its own (see the workflow).
#
# Needs gcc and OpenSSL 3 (headers + libraries). Linux: the distribution's packages
# (e.g. apt install gcc libssl-dev). Windows: MSYS2 (https://www.msys2.org), then in its
# MINGW64 shell: pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-openssl
# (MINGW32: mingw-w64-i686-toolchain mingw-w64-i686-openssl).
# macOS: the Xcode command line tools (xcode-select --install) and brew install openssl@3.
# On Windows and macOS OpenSSL is linked statically: the binary needs no OpenSSL libraries.

set -e
cd "$(dirname "$0")"

target=""
runtest=0
for arg in "$@"; do
  case "$arg" in
    linux64|linux32|win64|win32|macos-x64|macos-arm64) target="$arg" ;;
    test) runtest=1 ;;
    *) echo "build.sh: unknown argument '$arg' (expected linux64, linux32, win64, win32, macos-x64, macos-arm64 or test)" >&2; exit 2 ;;
  esac
done
if [ -z "$target" ]; then
  case "${MSYSTEM:-}" in
    MINGW64|UCRT64|CLANG64) target=win64 ;;
    MINGW32) target=win32 ;;
    *)
      if [ "$(uname -s)" = "Darwin" ]; then
        case "$(uname -m)" in
          arm64) target=macos-arm64 ;;
          *) target=macos-x64 ;;
        esac
      else
        target=linux64
      fi
      ;;
  esac
fi

stamp="${BS_BUILD_ID:-$(date -u +%Y%m%d.%H%M)}"
releaseflag=""
if [ -n "${BS_RELEASE_TAG:-}" ]; then
  # Only our tag format (it is compiled in as a string)
  if ! printf '%s' "$BS_RELEASE_TAG" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+-[0-9]{6}[a-z]{0,7}$'; then
    echo "build.sh: BS_RELEASE_TAG '$BS_RELEASE_TAG' is not a release tag (v1.7.4-YYMMDD[a-z], year first)" >&2
    exit 2
  fi
  releaseflag="-DBS_RELEASE_TAG=\"$BS_RELEASE_TAG\""
fi

# The registration (licence key) code is not built: it is switched off (BS_ENABLE_REGISTRATION 0 in
# bricksync.h, so nothing calls it) and its big-number maths (bn512.c, bn1024.c, rsabn.c) is x86
# assembly only, which would not build for Apple Silicon.
SOURCES="bricksync.c bricksyncconf.c bricksyncnet.c bsstop.c bricksyncinit.c bricksyncinput.c bsantidebug.c bsmessage.c bsmathpuzzle.c bsorder.c bsapihistory.c bstranslation.c bsevalgrade.c bsoutputxml.c bsorderdir.c bspriceguide.c bsmastermode.c bscheck.c bssync.c bsapplydiff.c bsfetchorderinv.c bsresolve.c bscatedit.c bsfetchinv.c bsfetchorderlist.c bsfetchset.c bscheckreg.c bsfetchpriceguide.c bsbroadcast.c tcp.c vtlex.c cpuinfo.c antidebug.c mm.c mmhash.c mmbitmap.c cc.c ccstr.c debugtrack.c tcphttp.c oauth.c bricklink.c brickowl.c brickowlinv.c colortable.c json.c bsx.c bsxpg.c journal.c exclperm.c iolog.c crypthash.c cryptsha1.c rand.c"
# The tests need only the code they check and what it uses
TEST_SOURCES="tlstest.c tcp.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
JOURNAL_TEST_SOURCES="journaltest.c journal.c iolog.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
HTTP_TEST_SOURCES="httptest.c tcphttp.c tcp.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
STOP_TEST_SOURCES="stoptest.c bsstop.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"
ATOMIC_TEST_SOURCES="atomictest.c"
BROADCAST_TEST_SOURCES="broadcasttest.c bsbroadcast.c json.c iolog.c cc.c ccstr.c mm.c debugtrack.c cpuinfo.c"

CC=gcc
exe=""
case "$target" in
  linux64|linux32|win64|win32)
    case "$target" in
      linux64|win64) bits=64 ;;
      *) bits=32 ;;
    esac
    archflags="-m$bits"
    CFLAGS="-std=gnu99 $archflags -O2 -s -fvisibility=hidden"
    ;;
  macos-*)
    # Apple's clang; the binary runs on macOS 11 (Big Sur) and later
    CC=clang
    case "$target" in
      macos-arm64) archflags="-arch arm64 -mmacosx-version-min=11.0" ;;
      *) archflags="-arch x86_64 -mmacosx-version-min=11.0" ;;
    esac
    # No -s here: Apple's linker ignores it; the binary is stripped (and signed) after linking
    CFLAGS="-std=gnu99 $archflags -O2 -fvisibility=hidden"
    ;;
esac

case "$target" in
  linux*)
    LIBS="-lssl -lcrypto -lm -lpthread"
    ;;
  win*)
    exe=".exe"
    # Static OpenSSL; crypt32 serves the Windows certificate store
    LIBS="-static -lssl -lcrypto -lws2_32 -lwsock32 -lcrypt32 -luser32 -ladvapi32 -lgdi32 -lm"
    ;;
  macos-*)
    ossl="${OPENSSL_DIR:-}"
    if [ -z "$ossl" ] && command -v brew >/dev/null 2>&1; then
      ossl="$(brew --prefix openssl@3 2>/dev/null || true)"
    fi
    if [ -z "$ossl" ] || [ ! -f "$ossl/lib/libssl.a" ] || [ ! -f "$ossl/lib/libcrypto.a" ]; then
      echo "build.sh: no static OpenSSL 3 found (set OPENSSL_DIR, or brew install openssl@3)" >&2
      exit 2
    fi
    # Static: the .a files by path, so the linker cannot pick a .dylib instead
    CFLAGS="$CFLAGS -I$ossl/include"
    LIBS="$ossl/lib/libssl.a $ossl/lib/libcrypto.a -lm -lpthread"
    ;;
esac

cpuflags="-h"
[ "${BS_PORTABLE:-0}" = "1" ] && cpuflags="-h -ccenv"

echo "Building BrickSync for $target, version stamp $stamp${BS_RELEASE_TAG:+, release $BS_RELEASE_TAG}"
$CC -std=gnu99 $archflags cpuconf.c cpuinfo.c -O2 -o cpuconf$exe
./cpuconf$exe $cpuflags

res=""
case "$target" in
  win*)
    windres bricksync.rc -O coff -o bricksync.res
    res="bricksync.res"
    ;;
esac

$CC $CFLAGS $SOURCES $res -o bricksync$exe $LIBS -DBS_VERSION_BUILDTIME="$stamp" $releaseflag
case "$target" in
  macos-*)
    # Strip, then sign ad hoc: Apple Silicon refuses to run a binary without a signature, and
    # stripping breaks the one the linker made. Ad hoc is not a developer signature (see README).
    strip -x bricksync
    codesign --force --sign - bricksync
    ;;
esac
echo "Built bricksync$exe"

if [ "$runtest" = "1" ]; then
  $CC $CFLAGS $ATOMIC_TEST_SOURCES -o atomictest$exe $LIBS
  ./atomictest$exe
  $CC $CFLAGS $JOURNAL_TEST_SOURCES -o journaltest$exe $LIBS
  ./journaltest$exe
  $CC $CFLAGS $HTTP_TEST_SOURCES -o httptest$exe $LIBS
  ./httptest$exe
  $CC $CFLAGS $STOP_TEST_SOURCES -o stoptest$exe $LIBS
  ./stoptest$exe
  $CC $CFLAGS $BROADCAST_TEST_SOURCES -o broadcasttest$exe $LIBS
  ./broadcasttest$exe
  $CC $CFLAGS $TEST_SOURCES -o tlstest$exe $LIBS
  ./tlstest$exe
fi

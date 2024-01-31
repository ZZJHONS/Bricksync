set PATH=C:\alexis\mingw64\mingw64\bin
gcc -std=gnu99 -m64 cpuconf.c cpuinfo.c -O2 -s -o cpuconf

cpuconf.exe -h

windres bricksync.rc -O coff -o bricksync.res
REM Added -Wno-implicit-function-declaration to avoid false positive compile warnings
REM Added to end to set BS_VERSION_BUILDTIME to dynamic build version in seconds from epoch -DBS_VERSION_BUILDTIME=`date '+%s'`
gcc -std=gnu99 -Wno-implicit-function-declaration -I./build-win64/ -L./build-win64/ -m64 bricksync.c bricksyncconf.c bricksyncnet.c bricksyncinit.c bricksyncinput.c bsantidebug.c bsmessage.c bsmathpuzzle.c bsorder.c bsregister.c bsapihistory.c bstranslation.c bsevalgrade.c bsoutputxml.c bsorderdir.c bspriceguide.c bsmastermode.c bscheck.c bssync.c bsapplydiff.c bsfetchorderinv.c bsresolve.c bscatedit.c bsfetchinv.c bsfetchorderlist.c bsfetchset.c bscheckreg.c bsfetchpriceguide.c tcp.c vtlex.c cpuinfo.c antidebug.c mm.c mmhash.c mmbitmap.c cc.c ccstr.c debugtrack.c tcphttp.c oauth.c bricklink.c brickowl.c brickowlinv.c colortable.c json.c bsx.c bsxpg.c journal.c exclperm.c iolog.c crypthash.c cryptsha1.c rand.c bn512.c bn1024.c rsabn.c bricksync.res -O2 -s -fvisibility=hidden -o bricksync -lm -lwsock32 -lws2_32 -lssl-1_1-x64 -lcrypto-1_1-x64  -DBS_VERSION_BUILDTIME=`date '+%s'`


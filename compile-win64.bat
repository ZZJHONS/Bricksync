@echo off
rem Hand build for 64-bit Windows: runs build.sh in an MSYS2 MINGW64 shell.
rem Everything (sources, flags, version stamp) lives in build.sh, which also says what to install.
rem MSYS2 elsewhere than C:\msys64? set MSYS2_ROOT first.
if not defined MSYS2_ROOT set MSYS2_ROOT=C:\msys64
set MSYSTEM=MINGW64
set CHERE_INVOKING=1
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "./build.sh win64 %*"
pause

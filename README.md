The BrickOwl/BrickLink synchronization software, BrickSync, is now open source!
https://github.com/ZZJHONS/Bricksync (the old www.bricksync.net site is gone)

Restrictions related to registration have been removed from the 1.7.1 builds (sorry, can't build OSX binaries, I don't have access to an OSX box...).

Releases (Windows and Linux builds, checksums): https://github.com/ZZJHONS/Bricksync/releases/latest
Source code: https://github.com/ZZJHONS/Bricksync

It's being released under a very permissible attribution license, more precisely:

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

The source code is written in C with GNUisms, therefore it will compile with GCC, mingw64, Clang or the Intel Compiler. The only dependency is OpenSSL 3 for socket encryption.

Every build runs `build.sh`, the one place for the source list, compiler flags and version stamp; `compile`, `compile32`, `compile-win64.bat`, `compile-win32.bat` and the GitHub Actions workflow all call it. `./build.sh test` also builds and runs `tlstest`, which checks that the API servers are authenticated, plus `journaltest` and `broadcasttest`.

* **Linux:** install gcc and the OpenSSL headers (e.g. `apt install gcc libssl-dev`), then run `./compile`.
* **Windows:** install [MSYS2](https://www.msys2.org), and in its MINGW64 shell run `pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-openssl` (32-bit: the MINGW32 shell and the `i686` packages). Then run `./build.sh` in that shell, or double-click `compile-win64.bat` (set `MSYS2_ROOT` first if MSYS2 is not in `C:\msys64`). OpenSSL is linked statically, so `bricksync.exe` needs no DLLs.

The version stamp after `1.7.4-` comes from `BS_BUILD_ID`: by default the UTC date and time (`YYYYMMDD.HHMM`); release builds use `YYYYMMDD.<run number>`.

BrickSync checks every server's certificate and host name and refuses servers it cannot verify. It trusts the system's certificates on Linux, and on Windows the Windows certificate store plus the `cacert.pem` shipped next to `bricksync.exe`.

Each release package comes with a `.sha256` checksum and a signed build-provenance record from GitHub Actions: `gh attestation verify <package> -R ZZJHONS/Bricksync` confirms it was built by this repository's workflow.

With `checkmessage = 1;` (the default) BrickSync asks GitHub, at start and every 3 hours, for the latest release and for the broadcast message in `broadcast/message.json`; see `broadcast/README.md`.

Happy building, and coding :)

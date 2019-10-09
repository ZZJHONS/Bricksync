The BrickOwl/BrickLink synchronization software, BrickSync, is now open source!
http://www.bricksync.net/

Restrictions related to registration have been removed from the 1.7.1 builds (sorry, can't build OSX binaries, I don't have access to an OSX box...).

Direct link to the source code:
http://www.bricksync.net/bricksync-src.tar.gz

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

The source code is written in C with GNUisms, therefore it will compile with GCC, mingw64, Clang or the Intel Compiler. The only dependency is OpenSSL for socket encryption. The source code includes a build-win64 subdirectory with headers and DLLs for OpenSSL on Windows; the code therefore will build with a plain freshly installed mingw64 (select x86-64 as target when installing mingw64, not i686).

Direct link to the mingw64 installer (mingw being the famous GCC compiler ported to Windows):
https://sourceforge.net/projects/mingw-w64/files/Toolchains targetting Win32/Personal Builds/mingw-builds/installer/mingw-w64-install.exe/download
You'll have to edit the compile-win64.bat script to set the PATH environment variable to the /bin subdirectory of wherever you installed mingw64.

Happy building, and coding :)

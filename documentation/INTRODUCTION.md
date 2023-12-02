
BrickSync
=========

**Inventory Synchronization Software between BrickLink and BrickOwl**

Contents:
---------

1.  [Key Features](#keyfeatures)
2.  [Downloads](#downloads)
3.  [Get Started](#getstarted)
4.  [Documentation](#documentation)
5.  [Quick Tips](#tips)
6.  [Source Code](#source)
7.  [Contact](#contact)

BrickSync is a piece of software meant to run on a LEGO seller's computer to synchronize inventories between the [**BrickLink**](http://www.bricklink.com/) and [**BrickOwl**](http://www.brickowl.com/) marketplaces, with many feature beyond this. All incoming orders (and backups) are stored as [**BrickStore/BrickStock**](http://brickstock.patrickbrans.com/) compatible BSX files, and the software can merge in new inventory (or update prices, and so on) from files in that same BSX format.

BrickSync is free, open source and donation supported. Since the version 1.7.1, the source code was made public and no registration is needed.

Key Features <a id="keyfeatures"><a/>
------------

*   Robust handling of all orders from both BrickLink and BrickOwl. Changes are instantly propagated to the other service.
*   If any inventory update fails with ambiguous results (i.e. we never had a reply to a query sent), the code automatically peforms a deep synchronization to detect and resolve any issue.
*   All incoming orders are saved as BrickStore/BrickStock BSX files.
*   Many commands to manipulate inventory, merge in new inventory, assist part sorting, evaluate sets for part-out, and more.
*   A _BrickLink Master_ mode, where all external inventory changes made on BrickLink are incorporated back into the tracked inventory.
*   Automated management of BrickLink's limit of 5000 API calls per day, with optional output of XML files for manual upload.
*   Can recover from any interruption, including power cuts, through file journaling bypassing hard drive internal caches.
*   BrickSync can always be closed or interrupted, execution can later resume with no consequence.

Downloads  <a id="downloads"><a/>
---------

*   See latest releases in the Github Releases section

How To Get Started  <a id="getstarted"><a/>
------------------

*   Unpack the package somewhere accessible, there's no special installation step.
*   Open the file _data/bricksync.conf.txt_ in a text editor and enter the API keys for both [BrickLink](http://api.bricklink.com/pages/api/welcome.page) and [BrickOwl](https://www.brickowl.com/user).
*   Run bricksync, it will fetch and compare your inventories on BrickLink and BrickOwl.

*   Linux and OSX users: Run ./bricksync from its own directory, it expects to find ./data/ in the current working directory.

*   BrickSync will ask your permission to apply all changes required to make the BrickOwl inventory match the BrickLink one exactly. You can review the detailed list of changes in the log file.
*   You are done! All new incoming orders will be properly propagated between the services.

Documentation  <a id="documentation"><a/>
-------------

*   [**FAQ List**](./FAQ.md) : The frequently asked questions.
*   [**Command List**](./COMMAND_LIST.md) : Documentation for most BrickSync commands. (**_Updated for 1.5.6_**)

Quick Tips  <a id="tips"><a/>
----------

*   The _BrickLink Master Mode_ lets you edit your inventory on BrickLink and incorporate back the changes into BrickSync. Note that all order checks are paused while in this mode. Use _blmaster on_ and _blmaster off_ to hop in and out.
*   You can update all your prices at once on both BrickLink and BrickOwl with the _loadprices_ command. It updates every item from the specified BSX file that is found in your inventory, matching by ID, color and condition. Example: _loadprices NewPrices.bsx_
*   For sellers who part out new sets, try out the _evalset_ command to evaluate a set's potential. Example: _evalset 10224_
*   When an order is cancelled, avoid returning the items to your inventory on BrickLink or BrickOwl. Use the _add_ command using the stored BSX file for that order. For example: _add data/orders/brickowl-6682398.bsx_ If you have already added the items back through a web interface, type _sync_ to fix everything (before or after the _add_).
*   Use the _sort_ command to prepare a BSX file for sorting into your physical inventory. It updates the file by copying all comments and remarks from your tracked inventory for matching lots, and it sorts items in an efficient way to locate items in BrickStore/BrickStock. Example: _sort MyFileAboutToBeSorted.bsx_
*   A _sync_ command will verify your entire inventory on BrickLink and BrickOwl to ensure they match the one being tracked locally. The slightest difference will be corrected.
*   Do you have items that you don't want synchronized, that you want to manage yourself on either BrickLink or BrickOwl? Just add the _~_ character anywhere in the remarks or private notes for these items, and BrickSync won't see them.
*   The _owlresolve_ command lets you see all BLIDs from your inventory that are unknown to BrickOwl. Now go edit the database and fill in the gaps. :)
*   To update BrickSync, you just need to replace the executable file. BrickSync will _always_ be able to read files generated by a previous version.

Source Code  <a id="source"><a/>
-----------

The source code is released under a very permissible license, you can do pretty much whatever you want with this source code and altered versions.

More precisely, here's the relevant legalese:

\* Copyright (c) 2014-2019 Alexis Naveros.  
\*  
\* This software is provided 'as-is', without any express or implied  
\* warranty. In no event will the authors be held liable for any damages  
\* arising from the use of this software.  
\*  
\* Permission is granted to anyone to use this software for any purpose,  
\* including commercial applications, and to alter it and redistribute it  
\* freely, subject to the following restrictions:  
\*  
\* 1. The origin of this software must not be misrepresented; you must not  
\* claim that you wrote the original software. If you use this software  
\* in a product, an acknowledgment in the product documentation would be  
\* appreciated but is not required.  
\* 2. Altered source versions must be plainly marked as such, and must not be  
\* misrepresented as being the original software.  
\* 3. This notice may not be removed or altered from any source distribution.

The source code is written in C with GNUisms, therefore it will compile with GCC, mingw64, Clang or the Intel Compiler. The only dependency is OpenSSL for socket encryption. The source code includes a build-win64 subdirectory with headers and DLLs for OpenSSL on Windows; the code therefore will build with a plain freshly installed [mingw64](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/installer/mingw-w64-install.exe/download) (select x86-64 as target when installing mingw64, not i686).

Contact  <a id="contact"><a/>
-------

*   [Stragus on BrickOwl](http://www.brickowl.com/user/9775/contact)
*   [Stragus on BrickLink](http://www.bricklink.com/contact.asp?u=Stragus)

Special Thanks
--------------

*   [BrickFeverParts](http://brickfeverparts.brickowl.com/) for being the biggest supporter of BrickSync!
*   [budgetkids](http://budgetkids.brickowl.com/) for the beta testing, numerous bug reports and support!
*   [DadsAFOL](http://constructibles.brickowl.com/) for the bug reports and being the very first supporter of BrickSync!
*   [BadLove](http://badlovesbricks.brickowl.com/) for the recurrent and well appreciated support!
*   [PSBricks/Bugsy](http://brickstopde.brickowl.com/) for the beta testing and the many useful bug reports!
*   [Brick Machine Shop](http://www.bricklink.com/store.asp?p=Eezo) for their fantastic aluminum Technic parts! (Okay, that's totally unrelated to BrickSync, but I love their products :) )
*   Many others who have supported the software through their donations, thanks all!

LEGO, the LEGO logo are trademarks of The LEGO Group of companies.
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

#include <sys/types.h>


/* fsync() directories to ensure file name consistency */
int journalDirSync( const char *dirpath );
int journalFileDirSync( const char *filepath );

/* Rename file with directory synchronization */
int journalRenameSync( char *oldpath, char *newpath, int retryflag );


////


typedef struct
{
  char *oldpath;
  char *newpath;
  int allocflags;
} journalEntry;

typedef struct
{
  journalEntry *entryarray;
  int entryalloc;
  int entrycount;
} journalDef;

int journalAlloc( journalDef *journal, int entryalloc );
void journalAddEntry( journalDef *journal, char *oldpath, char *newpath, int oldallocflag, int newallocflag );
void journalFree( journalDef *journal );

int journalExecute( char *journalpath, char *tempjournalpath, ioLog *log, journalEntry *entryarray, int entrycount );
int journalReplay( char *journalpath );


////


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


void ioStdinInit();

char *ioStdinReadLock( int waitmilliseconds );
void ioStdinUnlock();
/* Returned string must be freed */
char *ioStdinReadAlloc( int waitmilliseconds );


typedef struct
{
  char *logpath;
  int flushinterval;
  time_t curtime;
  int tm_year;
  int tm_month;
  int tm_day;
  FILE *file;
  int endlinecount;
  int linemarker;
  FILE *outputfile;
} ioLog;

int ioLogInit( ioLog *log, char *logpath, int flushinterval );
void ioLogEnd( ioLog *log );
void ioLogFlush( ioLog *log );

void ioLogSetOutputFile( ioLog *log, FILE *outputfile );

void ioPrintf( ioLog *target, int modemask, char *format, ... );


#define IO_MODEBIT_LOGONLY (0x1)
#define IO_MODEBIT_NODATE (0x2)
#define IO_MODEBIT_FLUSH (0x4)
#define IO_MODEBIT_NOLOG (0x8)
#define IO_MODEBIT_LINEMARKER (0x10)
#define IO_MODEBIT_OUTPUTFILE (0x20)


////


#define IO_COLOR_ESCAPE (0x05)
#define IO_COLOR_CODE_BASE (0x20)

#define IO_BLACK "\x05\x20"
#define IO_DKRED "\x05\x21"
#define IO_DKGREEN "\x05\x22"
#define IO_BROWN "\x05\x23"
#define IO_DKBLUE "\x05\x24"
#define IO_DKMAGENTA "\x05\x25"
#define IO_DKCYAN "\x05\x26"
#define IO_GRAY "\x05\x27"
#define IO_DKGRAY "\x05\x28"
#define IO_RED "\x05\x29"
#define IO_GREEN "\x05\x2a"
#define IO_YELLOW "\x05\x2b"
#define IO_BLUE "\x05\x2c"
#define IO_MAGENTA "\x05\x2d"
#define IO_CYAN "\x05\x2e"
#define IO_WHITE "\x05\x2f"
#define IO_DEFAULT "\x05\x30"



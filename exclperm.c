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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>

#include "cpuconfig.h"
#include "cc.h"
#include "ccstr.h"

#if CC_UNIX
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/file.h>
#elif CC_WINDOWS
 #include <windows.h>
 #include <direct.h>
#else
 #error Unknown/Unsupported platform!
#endif


////


/* Chunk of code to get exclusive access to a file, in order to prevent multiple executable instances */

typedef struct
{
#if CC_UNIX
  int fd;
#elif CC_WINDOWS
  HANDLE file;
#else
 #error Unsupported platform, either Unix or Windows!
#endif
} exclPerm;


/* Acquire exclusive lock on newly created file */
void *exclPermStart( char *exclpath )
{
  exclPerm *perm;
  perm = malloc( sizeof(exclPerm) );

#if CC_UNIX
  perm->fd = open( exclpath, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR );
  if( ( perm->fd == -1 ) || flock( perm->fd, LOCK_EX | LOCK_NB ) )
  {
    free( perm );
    return 0;
  }
#elif CC_WINDOWS
  perm->file = CreateFileA( exclpath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0 );
  if( perm->file == INVALID_HANDLE_VALUE )
  {
    free( perm );
    return 0;
  }
#endif

  return (void *)perm;
}


/* Release exclusive lock on file and delete it */
void exclPermStop( void *exclperm, char *exclpath )
{
  exclPerm *perm;

  perm = (exclPerm *)exclperm;
#if CC_UNIX
  flock( perm->fd, LOCK_UN );
  close( perm->fd );
#elif CC_WINDOWS
  CloseHandle( perm->file );
#endif
  remove( exclpath );
  free( perm );

  return;
}



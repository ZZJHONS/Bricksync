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

int main()
{
  int index;
  FILE *file;

  file = fopen( "owlupdateblid.txt", "w" );


  for( index = 1 ; index < 100 ; index++ )
    fprintf( file, "owlupdateblid -f 3626bpb%02d 3626bpb%04d\n", index, index );
  for( index = 1 ; index < 1000 ; index++ )
    fprintf( file, "owlupdateblid -f 3626bpb%03d 3626bpb%04d\n", index, index );
/*
  for( index = 1 ; index < 1000 ; index++ )
    fprintf( file, "owlupdateblid -f 3626cpb%03d 3626cpb%04d\n", index, index );

  for( index = 1 ; index < 1000 ; index++ )
    fprintf( file, "owlupdateblid -f 3068bpb%03d 3068bpb%04d\n", index, index );

  for( index = 1 ; index < 1000 ; index++ )
    fprintf( file, "owlupdateblid -f 973pb%02dc01 973pb%04dc01\n", index, index );
  for( index = 1 ; index < 1000 ; index++ )
    fprintf( file, "owlupdateblid -f 973pb%02d 973pb%04d\n", index, index );
  for( index = 1 ; index < 10 ; index++ )
    fprintf( file, "owlupdateblid -f 91884pb%02d 91884pb%03d\n", index, index );
  for( index = 1 ; index < 10 ; index++ )
    fprintf( file, "owlupdateblid -f 522pb%02d 522pb%03d\n", index, index );
*/

  fclose( file );

  return 1;
}

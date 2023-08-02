#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find( char *path, const char *name )
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ( ( fd = open( path, 0 ) ) < 0 ) {
    fprintf( 2, "find: cannot open %s\n", path );
    return;
  }

  if ( fstat( fd, &st ) < 0 ) {
    fprintf( 2, "find: cannot stat %s\n", path );
    close( fd );
    return;
  }

  if ( strlen( path ) + 1 + DIRSIZ + 1 > sizeof buf ) {
    printf( "find: path too long\n" );
  }

  strcpy( buf, path );
  p = buf + strlen( buf );
  *p++ = '/';
  while ( read( fd, &de, sizeof( de ) ) == sizeof( de ) ) {
    if ( de.inum == 0 )
      continue;
    memmove( p, de.name, DIRSIZ );
    p[DIRSIZ] = 0;
    if ( stat( buf, &st ) < 0 ) {
      printf( "find: cannot stat %s\n", buf );
      continue;
    }
    if ( st.type == T_FILE && !strcmp( de.name, name ) ) {
      printf( "%s\n", buf );
    } else if ( st.type == T_DIR && strcmp( de.name, "." ) && strcmp( de.name, ".." ) ) {
      find( buf, name );
    }
  }
  close( fd );
}

int
main( int argc, char *argv[] )
{
  find( argv[1], argv[2] );
  exit( 0 );
}
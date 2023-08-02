#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define UPPER_BOUND 36

int
main( int argc, char *argv[] )
{
  int p[2];
  pipe( p );

  for ( int i = 3; i < UPPER_BOUND; i++ ) {
    write( p[1], &i, 1 );
  }
  int bound = UPPER_BOUND;
  write( p[1], &bound, 1 );

  int prime = 2;
  printf( "prime %d\n", prime );

  while ( 1 ) {
    if ( fork() == 0 ) {
      int num = 0;
      while ( read( p[0], &num, 1 ) && num < UPPER_BOUND ) {
        if ( num % prime != 0 ) {
          write( p[1], &num, 1 );
        }
      }
      write( p[1], &bound, 1 );
      close( p[0] );
      close( p[1] );
      exit( 0 );
    } else {
      wait( 0 );
      if ( read( p[0], &prime, 1 ) && prime < UPPER_BOUND ) {
        printf( "prime %d\n", prime );
      } else {
        break;
      }
    }
  }
  close( p[0] );
  close( p[1] );
  exit( 0 );
}
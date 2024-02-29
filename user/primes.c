#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define UPPER_BOUND 36

int
main(int argc, char *argv[])
{
  int prime;
  int bound = UPPER_BOUND;

  int p[2];
  pipe(p);

  for (int i = 2; i <= UPPER_BOUND; i++)
    write(p[1], &i, 1);

  while (1) {
    if (!(read(p[0], &prime, 1) && prime < UPPER_BOUND))
      break;
    printf("prime %d\n", prime);
    if (fork() == 0) {
      int num = 0;
      while (read(p[0], &num, 1) && num < UPPER_BOUND)
        if (num % prime != 0) 
          write(p[1], &num, 1);
      write(p[1], &bound, 1);
      close(p[0]);
      close(p[1]);
      exit(0);
    } else {
      wait(0);
    }
  }
  close(p[0]);
  close(p[1]);
  exit(0);
}
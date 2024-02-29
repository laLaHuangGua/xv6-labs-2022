#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_CHAR_A_LINE_HOLD 255

int
main(int argc, char *argv[])
{
  char buf[MAX_CHAR_A_LINE_HOLD];
  char *p = buf;
  char temp = 'a';

  while (read(0, &temp, sizeof(char))) {
    if ((int)temp != '\n') {
      *p++ = temp;
      continue;
    }
    *p = 0;
    if (fork() == 0) {
      char *args[argc + 1];
      int i = 0;
      for (; i < argc - 1; i++) {
        args[i] = argv[i + 1];
      }
      args[i++] = buf;
      args[i] = 0;
      exec(argv[1], args);
      exit(0);
    } else {
      wait(0);
      p = buf;
    }
  }
  exit(0);
}
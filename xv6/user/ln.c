#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // Symbolic link: ln -s target path
  if (argc == 4 && strcmp(argv[1], "-s") == 0) {
    if (symlink(argv[2], argv[3]) < 0) {
      fprintf(2, "ln: symlink failed\n");
      exit(1);
    }
  }

  // Hard link: ln target path
  else if (argc == 3) {
    if (link(argv[1], argv[2]) < 0) {
      fprintf(2, "ln: link failed\n");
      exit(1);
    }
  }

  // Invalid arguments
  else {
    fprintf(2, "Usage: ln [-s] target path\n");
    exit(1);
  }

  exit(0);
}
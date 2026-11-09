#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int fd;
  int n;
  struct stat st;
  char target[MAXPATH + 1];

  // Check arguments
  if (argc != 2) {
    fprintf(2, "Usage: readlink path\n");
    exit(1);
  }

  // Open the symlink itself, without following it
  fd = open(argv[1], O_RDONLY | O_NOFOLLOW);

  if (fd < 0) {
    fprintf(2, "readlink: cannot open %s\n", argv[1]);
    exit(1);
  }

  // Check whether the inode is actually a symbolic link
  if (fstat(fd, &st) < 0) {
    fprintf(2, "readlink: cannot stat %s\n", argv[1]);
    close(fd);
    exit(1);
  }

  if (st.type != T_SYMLINK) {
    fprintf(2, "readlink: %s is not a symbolic link\n", argv[1]);
    close(fd);
    exit(1);
  }

  // Read the stored target pathname
  n = read(fd, target, MAXPATH);

  if (n < 0) {
    fprintf(2, "readlink: cannot read %s\n", argv[1]);
    close(fd);
    exit(1);
  }

  if (n == 0) {
    fprintf(2, "readlink: empty or invalid symbolic link\n");
    close(fd);
    exit(1);
  }

  // Ensure the string is null-terminated
  target[n] = '\0';

  // Print the target pathname
  printf("%s\n", target);

  close(fd);
  exit(0);
}
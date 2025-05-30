#include "../include/stat.hpp"

int isatty(fd)
int fd;
{
  struct stat s;

  fstat(fd, &s);
  if ((s.st_mode & FileMode::S_IFMT) ==
      static_cast<unsigned short>(FileMode::S_IFCHR))
	return(1);
  else
	return(0);
}

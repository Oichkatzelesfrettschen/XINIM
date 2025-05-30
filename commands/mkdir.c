/* mkdir - make a directory		Author: Adri Koppes */

#include "signal.hpp"

int error = 0;

/* Program entry point */
int main(int argc, char **argv)
{
	if (argc < 2) {
		std_err("Usage: mkdir directory...\n");
		exit(1);
	}
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	while (--argc)
		makedir(*++argv);
	if (error)
		exit(1);
}

/* Create a directory */
static void makedir(char *dirname)
{
	char dots[128], parent[128];
	int sl = 0, i = 0;

	while (dirname[i])
		if (dirname[i++] == '/')
			sl = i;
	strncpy(parent, dirname, sl);
	parent[sl] = '\0';
	strcat(parent, ".");
	if (access(parent, 2)) {
		stderr3("mkdir: can't access ", parent, "\n");
		exit(1);
	}
	if (mknod(dirname, 040777, 0)) {
		stderr3("mkdir: can't create ", dirname, "\n");
		error++;
		return;
	}
	chown(dirname, getuid(), getgid());
	strcpy(dots, dirname);
	strcat(dots, "/.");
	if (link(dirname, dots)) {
		stderr3("mkdir: can't link ", dots, " to ");
		stderr3(dirname, "\n", "");
		error++;
		unlink(dirname);
		return;
	}
	strcat(dots, ".");
	if (link(parent, dots)) {
		stderr3("mkdir: can't link ", dots, " to ");
		stderr3(parent, "\n", "");
		error++;
		dots[strlen(dots)] = '\0';
		unlink(dots);
		unlink(dirname);
		return;
	}
}

/* Print three error strings */
static void stderr3(const char *s1, const char *s2, const char *s3)
{
  std_err(s1);
  std_err(s2);
  std_err(s3);
}

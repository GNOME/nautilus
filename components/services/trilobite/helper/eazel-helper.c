/*
 * very stupid.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

#define RPM_EXEC	"/bin/rpm"

/* paths to search for executables (like RPM) in */
static const char *search_path[] = {
	"/bin",
	"/usr/bin",
	"/usr/local/bin",
	"/sbin",
	"/usr/sbin",
	"/usr/local/sbin",
	NULL
};


static void
chomp (char *buffer)
{
	int x = strlen (buffer);

	while ((x > 0) && ((buffer[x - 1] == '\n') || (buffer[x - 1] == '\r'))) {
		buffer[x - 1] = 0;
		x--;
	}
}

static const char *
find_path_to (const char *filename)
{
	char *path;
	int i;

	for (i = 0; search_path[i]; i++) {
		path = g_strdup_printf ("%s/%s", search_path[i], filename);
		/* i guess g_file_exists() is going away, and not part of glib anyway :( */
		if (access (path, X_OK) == 0) {
			g_free (path);
			return search_path[i];
		}
		g_free (path);
	}

	return NULL;
}

int
main (int argc, char **argv)
{
	char buffer[256];
	const char *path;
	char *filename;
	char **pargv;
	int args, i;

	printf ("* OK.\n");
	fflush (stdout);

	/* send stderr to stdout */
	dup2 (1, 2);

	/* get command */
	fgets (buffer, 256, stdin);
	if (feof (stdin)) {
		/* give up */
		exit (1);
	}
	chomp (buffer);

	/* rpm <# of parameters> */
	/* (followed by N lines of parameters) */
	if (g_strncasecmp (buffer, "rpm ", 4) == 0) {
		args = atoi (buffer + 4);
		pargv = g_new0 (char *, args + 2);

		path = find_path_to ("rpm");
		if (! path) {
			printf ("* Can't find RPM. :(\n");
			exit (1);
		}

		filename = g_strdup_printf ("%s/%s", path, "rpm");
		pargv[0] = "rpm";
		for (i = 0; i < args; i++) {
			fgets (buffer, 256, stdin);
			chomp (buffer);
			pargv[i + 1] = g_strdup (buffer);
		}
		pargv[args + 1] = NULL;

		/* we never free any of the args, but it doesn't matter, because
		 * if the exec succeeds, this all suddenly vanishes. :)
		 */
		execv (filename, pargv);

		printf ("* Can't run RPM. :(\n");
		exit (1);
	}

	printf ("* What?\n");
	exit (1);
}

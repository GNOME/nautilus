#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

/* 
 * Nautilus
 *
 * Copyright (C) 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Eric Fischer <eric@eazel.com>
 * Based on a Perl program by Kenneth Christiansen
 *
 * This program turns old uppercase nautilus XML metafiles into new
 * lowercase ones.
 *
 */

char **av;
int lastreport = 0;

int failures = 0;
int successes = 0;
int unneeded = 0;

struct list {
	char *s;
	struct list *next;
};

void recurse (char *, char *, float, float);
void check_ptr (void *);
void fix (char *, char *);
void examine (char *, char *);
void do_inside (char *, float, float);

void
check_ptr (void *p)
{
	if (!p) {
		fprintf (stderr, "%s: out of memory: %s\n", *av,
			 strerror (errno));
		exit (EXIT_FAILURE);
	}
}

void
fix (char *prefix, char *file)
{
	FILE *f;
	int c;
	int dquote, squote;
	int changed, read_only, failed_to_change;

	changed = 0;
	read_only = 0;
	failed_to_change = 0;

	/*
	 * Verified that the file modification time only changes if the
	 * file's content is actually changed; it doesn't change solely
	 * because the file was opened r+.
	 */

	f = fopen (file, "r+");
	if (!f) {
		read_only = errno;

		f = fopen (file, "r");
		if (!f) {
			fprintf (stderr, "%s: couldn't read %s/%s: %s\n", *av,
				 prefix, file, strerror (errno));
			return;
		}
	}

	/*
	 * I'm sure XML lexical analysis is actually more complicated
	 * than this, but I can't find something that really says.
	 * and it works on all the files I have to test on.  It does
	 * a more accurate job than the perl script, anyway.
	 */

	/*
	 * The "no change" comments refer to places where what's in the
	 * file is already correct and doesn't need to be altered.  If
	 * this were writing to a new file rather than making the changes
	 * in place, these would be calls to putc() to write the changes.
	 */

	while ((c = getc (f)) != EOF) {
		if (c != '<') {
			;   /* no change */ 
		} else {
			;   /* no change */

			while ((c = getc (f)) != EOF) {
				if (c == '>') {
					ungetc (c, f);
					break;
				} else if (c == '=') {
					dquote = squote = 0;
					;  /* no change */

					while ((c = getc (f)) != EOF) {
						if (c == '>' && !dquote &&
							    !squote) {
							ungetc (c, f);
							break;
						} else if (c == '\'' &&
							    !dquote) {
							squote = !squote;
							;  /* no change */
						} else if (c == '\"' &&
							    !squote) {
							dquote = !dquote;
							;  /* no change */
						} else if (isspace (c) &&
							    !dquote &&
							    !squote) {
							;  /* no change */
							break;
						} else {
							;  /* no change */
						}
					}
				} else if (isupper (c)) {
					if (read_only) {
						failed_to_change = 1;
					} else {
						fseek (f, -1L, SEEK_CUR);
						putc (tolower (c), f);

						/* Looks useless, but have to
						 * seek between reads and
						 * writes to the same file
						 */
						fseek (f, 0L, SEEK_CUR);
						changed = 1;
					}
				} else {
					;  /* no change */
				}
			}
		}
	}

	fclose (f);

	if (failed_to_change) {
		fprintf (stderr, "%s: couldn't fix %s/%s: %s\n",
			 *av, prefix, file, strerror (read_only));
		lastreport = 0;
		failures++;
	} else if (changed) {
		printf ("%s/%s: fixed\n", prefix, file);
		lastreport = 0;
		successes++;
	} else {
		unneeded++;
	}
}

void
examine (char *prefix, char *file)
{
	FILE *f;
	int shouldfix = 0;
	int c;

	f = fopen (file, "r");
	if (!f) {
		fprintf (stderr, "%s: couldn't examine %s/%s: %s\n", *av,
			 prefix, file, strerror (errno));
		return;
	}

	/*
	 * The assumption: for files we have to examine (as opposed to
	 * those that are named .nautilus-metafile.xml), if the first
	 * line begins with "<?xml" and the second with "<NAUTILUS",
	 * then it's one we need to fix; otherwise, it's somebody
	 * else's and we should leave it alone.
	 */

	if (getc (f) == '<' &&
	    getc (f) == '?' &&
	    getc (f) == 'x' &&
	    getc (f) == 'm' &&
	    getc (f) == 'l') {
		while ((c = getc (f)) != EOF) {
			if (c == '\n') {
				break;
			}
		}

		if (getc (f) == '<' &&
		    getc (f) == 'N' &&
		    getc (f) == 'A' &&
		    getc (f) == 'U' &&
		    getc (f) == 'T' &&
		    getc (f) == 'I' &&
		    getc (f) == 'L' &&
		    getc (f) == 'U' &&
		    getc (f) == 'S') {
			    shouldfix = 1;
		}
	}

	fclose (f);

	if (shouldfix) {
		fix (prefix, file);
	}
}

void
do_inside (char *prefix, float done, float frac)
{
	DIR *d;
	struct dirent *de;

	struct list *files;
	int nfiles = 0;

	struct list *lp;
	int n;

	struct stat buf;

	int report;
	float frac_over_nfiles;

	d = opendir (".");
	if (!d) {
		fprintf (stderr, "%s: couldn't read directory %s: %s\n",
			 *av, prefix, strerror (errno));
		return;
	}

	files = NULL;
	while ((de = readdir (d)) != NULL) {
		if (strcmp (de->d_name, ".") == 0)
			continue;
		if (strcmp (de->d_name, "..") == 0)
			continue;

		lp = malloc (sizeof (struct list));
		check_ptr (lp);

		lp->s = strdup (de->d_name);
		check_ptr (lp->s);

		lp->next = files;
		files = lp;

		nfiles++;
	}

	closedir (d);

	frac_over_nfiles = frac / nfiles;

	n = 0;
	while (files) {
		report = 10000 * (done + n * frac_over_nfiles);
		if (report != lastreport) {
			fprintf (stderr, "%7.2f%%\r", report / 100.0);
			lastreport = report;
		}

		if (strcmp (files->s, ".nautilus-metafile.xml") == 0) {
			fix (prefix, files->s);
		} else if (lstat (files->s, &buf) != 0) {
			fprintf (stderr,
				 "%s: couldn't check status of %s/%s: %s\n",
				 *av, prefix, files->s, strerror (errno));
		} else if (buf.st_mode & S_IFDIR) {
			recurse (prefix, files->s,
				 done + n * frac_over_nfiles, frac_over_nfiles);
		} else {
			examine (prefix, files->s);
		}

		lp = files->next;
		free (files->s);
		free (files);
		files = lp;
		n++;
	}
}

void
recurse (char *prefix, char *dir, float done, float frac)
{
	char *new_prefix;
	struct stat buf1, buf2;

	if (lstat (".", &buf1) != 0) {
		fprintf (stderr, "%s: couldn't check status of %s: %s\n",
			 *av, prefix, strerror (errno));
		return;
	}

	new_prefix = malloc (strlen (prefix) + strlen (dir) + 2);
	check_ptr (new_prefix);

	if (strcmp (dir, ".") == 0) {
		strcpy (new_prefix, prefix);
	} else {
		sprintf (new_prefix, "%s/%s", prefix, dir);
	}

	if (chdir (dir) != 0) {
		fprintf (stderr, "%s: couldn't change directory to %s: %s\n",
			 *av, new_prefix, strerror (errno));
		free (new_prefix);
		return;
	}

	do_inside (new_prefix, done, frac);

	if (strcmp (dir, ".") != 0) {
		chdir ("..");

		if (lstat (".", &buf2) != 0) {
			fprintf (stderr,
				 "%s: couldn't check status of %s: %s\n",
				 *av, ".", strerror (errno));
			exit (EXIT_FAILURE);
		}

		if (buf1.st_dev != buf2.st_dev || buf1.st_ino != buf2.st_ino) {
			fprintf (stderr, "%s: cd .. from %s did not lead "
				 "back to %s\n", *av, new_prefix, prefix);
			exit (EXIT_FAILURE);
		}
	}

	free (new_prefix);
}

int
main (int argc, char **argv)
{
	av = argv;

	if (argc == 1) {
		recurse (".", ".", 0.0, 1.0);
	} else if (argc == 2) {
		if (chdir (argv[1]) != 0) {
			fprintf (stderr,
				 "%s: couldn't change directory to %s: %s\n",
				 *av, argv[1], strerror (errno));
			exit (EXIT_FAILURE);
		}

		recurse (argv[1], ".", 0.0, 1.0);
	} else {
		fprintf (stderr, "Usage: %s [directory]\n", *av);
		exit (EXIT_FAILURE);
	}

	printf ("Files that couldn't be fixed:       %10d\n", failures);
	printf ("Files that didn't need to be fixed: %10d\n", unneeded);
	printf ("Files that were fixed:              %10d\n", successes);

	return EXIT_SUCCESS;
}

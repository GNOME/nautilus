/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 *  Verifies that the signature on an RPM is good, and returns a
 *  string identifying who signed it.
 *
 *  Copyright (C) 2000 Eazel, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Robey Pointer <robey@eazel.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <rpm/rpmlib.h>
#include <glib.h>
#include "eazel-install-rpm-signature.h"


/* there ought to be a general-purpose function for this in glib (grumble) */
#define tr_randchar ("abcdefghijklmnopqrstuvwxyz123456"[rand()%32])
static char *
trilobite_make_temp_file (int *fd)
{
	char *filename;
	long pid, pidmax;

	pid = ((long)getpid() * 100);
	pidmax = pid + 99;

	while (pid <= pidmax) {
		filename = g_strdup_printf ("%s/trilobite-%ld%c%c%c%c", g_get_tmp_dir(), pid,
					    tr_randchar, tr_randchar, tr_randchar, tr_randchar);
		*fd = open (filename, O_RDWR|O_CREAT|O_EXCL, 0600);
		if (*fd >= 0) {
			return filename;
		}

		/* try again */
		g_free (filename);
		pid++;
	}

	/* tried 100 combos, none worked.  give up. */
	return NULL;
}


/* pull the signature out of an RPM file.
 * returns 0 on success, and fills in 'fdt' with an RPM FD_t whose file pointer is aimed
 *	to the beginning of the signed contents.
 */
static int
trilobite_get_rpm_signature (const char *filename, void **signature, int *signature_len, FD_t *fdt)
{
	struct rpmlead lead;
	Header sigs;
	FD_t rpm_fdt;
	int_32 size;

	*fdt = NULL;

	/* 1. librpm documentation is incomplete, and in this case, also wrong.
	 * 2. librpm requires us to use their i/o functions like "Fopen"
	 */
	rpm_fdt = Fopen (filename, "r.ufdio");
	if (rpm_fdt == NULL) {
		goto bail;
	}
	if (readLead (rpm_fdt, &lead) != 0) {
		goto bail;
	}
	if (rpmReadSignature (rpm_fdt, &sigs, lead.signature_type) != 0) {
		goto bail;
	}

	/* at this point, the rpm_fdt file pointer is at a special point in the file.
	 * everything from here to the end of the file is the "content" that was signed.
	 * therefore we must be careful to preserve and return this FD_t for the caller.
	 */

	if (! headerGetEntry (sigs, RPMSIGTAG_GPG, NULL, signature, &size)) {
		goto bail;
	}
	*signature_len = (int)size;
	*fdt = rpm_fdt;
	return 0;

bail:
	if (rpm_fdt != NULL) {
		Fclose (rpm_fdt);
	}
	return -1;
}



/* verify the signature on an RPM file, given a keyring to use, and the filename of the RPM.
 *
 * returns 0 if the signature is ok,
 *         1 if the signature is bad,
 *	   2 if the RPM isn't signed (or doesn't exist),
 *        -1 if it was unable to check for some reason
 */
int
trilobite_check_rpm_signature (const char *filename, const char *keyring_filename, char **signer_name)
{
	int key_fd = -1;
	int stdin_fd, stdout_fd, stderr_fd;
	char *temp_filename;
	char *p;
	void *signature;
	int err;
	int i;
	int status;
	int bytes;
	char *gpg_paths[] = { "/usr/bin/gpg", "/bin/gpg", "/usr/local/bin/gpg", NULL };
	char *argv[] = { "gpg", "--batch", "--no-verbose", "--quiet", "--no-secmem-warning",
			 "--no-default-keyring", "--status-fd", "1", "--keyring", "%KEYRING%",
			 "--verify", "%FILENAME%",
			 NULL };
	char buffer[1024];
	char line[128];
	FILE *gnupg_file;
	FD_t rpm_fdt = NULL;

	*signer_name = NULL;

	/* must write the signature into a temporary file for gpg */
	if (trilobite_get_rpm_signature (filename, &signature, &bytes, &rpm_fdt) != 0) {
		goto bail_nosig;
	}
	temp_filename = trilobite_make_temp_file (&key_fd);
	if (temp_filename == NULL) {
		goto bail;
	}
	if (write (key_fd, signature, bytes) < bytes) {
		goto bail;
	}
	close (key_fd);
	key_fd = -1;

	/* fill in keyring and temp filename */
	for (i = 0; argv[i] != NULL; i++) {
		if (strcmp (argv[i], "%KEYRING%") == 0) {
			argv[i] = (char *)keyring_filename;
		}
		if (strcmp (argv[i], "%FILENAME%") == 0) {
			argv[i] = temp_filename;
		}
	}

	for (i = 0, err = -1; (gpg_paths[i] != NULL) && (err != 0); i++) {
		/* all GPG crap output goes to stderr, which we'll ignore.
		 * status output is going to stdout, so we can scan it to
		 * find out which key was used.
		 */
		err = trilobite_pexec (gpg_paths[i], argv, &stdin_fd, &stdout_fd, &stderr_fd);
	}
	if (err != 0) {
		/* can't find gpg */
		goto bail;
	}

	/* write all of the rpm file into stdin */
	while (1) {
		bytes = Fread (buffer, 1, sizeof(buffer), rpm_fdt);
		if (bytes <= 0) {
			break;
		}
		if (write (stdin_fd, buffer, bytes) != bytes) {
			break;
		}
	}
	Fclose (rpm_fdt);
	rpm_fdt = NULL;
	close (stdin_fd);
	wait (&status);

	/* gpg is finished */
	close (stderr_fd);
	unlink (temp_filename);
	temp_filename = NULL;

	if (!WIFEXITED (status)) {
		/* failed to verify the signature */
		close (stdout_fd);
		return 1;
	}

	/* read GNUPG status lines */
	gnupg_file = fdopen (stdout_fd, "r");
	if (gnupg_file == NULL) {
		close (stdout_fd);
		goto bail;
	}

	err = -1;	/* gpg didn't say anything? */

	while (! feof (gnupg_file)) {
		fgets (line, 128, gnupg_file);
		if (feof (gnupg_file)) {
			break;
		}
		line[127] = 0;
		if (line[strlen (line) - 1] == '\n') {
			line[strlen (line) - 1] = 0;
		}

		/* get the name of the signer */
		if (strncmp (line, "[GNUPG:] ", 9) == 0) {
			p = strstr (line, "GOODSIG ");
			if (p != NULL) {
				p += 8;
				while (*p && (*p != ' ')) {
					p++;
				}
				if (*p) {
					g_free (*signer_name);
					*signer_name = g_strdup (p+1);
					err = 0;
				}
			}

			p = strstr (line, "BADSIG ");
			if (p != NULL) {
				err = 1;
			}
		}
	}

	fclose (gnupg_file);
	return err;

bail_nosig:
	return 2;

bail:
	if (rpm_fdt != NULL) {
		Fclose (rpm_fdt);
	}
	if (key_fd >= 0) {
		close (key_fd);
	}
	if (temp_filename != NULL) {
		unlink (temp_filename);
	}
	return -1;
}

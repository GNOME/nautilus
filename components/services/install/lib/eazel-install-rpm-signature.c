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
#include <glib.h>
#include "eazel-install-rpm-signature.h"


/* some older versions of librpm are COMPLETELY INCOMPATIBLE with newer versions,
 * even though they have the same library version number!
 * so, parsing the RPM file headers must be done by hand. :( :( :(
 */

#define RPM_MAGIC_1	0xED
#define RPM_MAGIC_2	0xAB
#define RPM_MAGIC_3	0xEE
#define RPM_MAGIC_4	0xDB

#define RPM_GPG_BLOCK	0x3ED

typedef struct {
	gint32 magic;
	gint32 reserved;
	gint32 entries;
	gint32 data_size;
} RPMHeader;

typedef struct {
	gint32 tag;
	gint32 type;
	gint32 offset;
	gint32 count;
} RPMEntry;


/* returns 0 on success,
 *        -1 if this isn't an RPM, or is incompatible somehow
 */
static int
read_rpm_lead (int fd)
{
	unsigned char buffer[96];
	int bytes = 0;
	int n;

	while (bytes < sizeof (buffer)) {
		n = read (fd, buffer + bytes, sizeof (buffer) - bytes);
		if (n <= 0) {
			return -1;
		}
		bytes += n;
	}

	if ((buffer[0] != RPM_MAGIC_1) || (buffer[1] != RPM_MAGIC_2) ||
	    (buffer[2] != RPM_MAGIC_3) || (buffer[3] != RPM_MAGIC_4)) {
		return -1;
	}
	/* version number >= 2.0 */
	if (buffer[4] < 2) {
		return -1;
	}
	/* type = 0 */
	if ((buffer[6] != 0) || (buffer[7] != 0)) {
		return -1;
	}
	/* header signature (5) */
	if ((buffer[78] != 0) || (buffer[79] != 5)) {
		return -1;
	}

	return 0;
}

/* returns -1 if there is no GPG signature, 0 on success */
/* on success, you must g_free the signature when done */
static int
read_rpm_signature (int fd, void **signature, int *signature_len)
{
	RPMHeader header;
	RPMEntry *entry = NULL;
	int n, i, bytes;
	int offset;
	char buffer[256];

	*signature = NULL;

	for (bytes = 0; bytes < sizeof (header); ) {
		n = read (fd, &header + bytes, sizeof (header) - bytes);
		if (n <= 0) {
			return -1;
		}
		bytes += n;
	}

	header.entries = ntohl (header.entries);
	header.data_size = ntohl (header.data_size);
	entry = g_new0 (RPMEntry, header.entries);

	for (bytes = 0; bytes < (header.entries * sizeof (RPMEntry)); ) {
		n = read (fd, entry + bytes, (header.entries * sizeof (RPMEntry)) - bytes);
		if (n <= 0) {
			goto bail;
		}
		bytes += n;
	}

	offset = -1;
	for (i = 0; i < header.entries; i++) {
		entry[i].tag = ntohl (entry[i].tag);
		entry[i].offset = ntohl (entry[i].offset);
		entry[i].count = ntohl (entry[i].count);

		if (entry[i].tag == RPM_GPG_BLOCK) {
			/* found a gpg block! */
			offset = entry[i].offset;
			*signature_len = entry[i].count;
		}
	}

	if (offset < 0) {
		/* didn't find any gpg block */
		goto bail;
	}

	for (bytes = 0; bytes < offset; ) {
		n = read (fd, buffer, MIN (offset - bytes, sizeof (buffer)));
		if (n <= 0) {
			goto bail;
		}
		bytes += n;
	}

	*signature = g_malloc (*signature_len);
	for (bytes = 0; bytes < *signature_len; ) {
		n = read (fd, *signature + bytes, *signature_len - bytes);
		if (n <= 0) {
			goto bail;
		}
		bytes += n;
	}

	/* now we must move the file pointer past all this meta-data */
	/* round header data size up to a 4-byte boundary */
	if (header.data_size % 4) {
		header.data_size += 4 - (header.data_size % 4);
	}
	offset = header.data_size - (*signature_len + offset);
	for (bytes = 0; bytes < offset; ) {
		n = read (fd, buffer, MIN (offset - bytes, sizeof (buffer)));
		if (n <= 0) {
			goto bail;
		}
		bytes += n;
	}

	g_free (entry);
	return 0;

bail:
	if (*signature != NULL) {
		g_free (*signature);
		*signature = NULL;
	}
	if (entry) {
		g_free (entry);
	}
	return -1;
}


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
	int rpm_fd = -1;
	int stdin_fd, stdout_fd, stderr_fd;
	char *temp_filename;
	char *p;
	void *signature = NULL;
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

	*signer_name = NULL;

	/* read the signature out of the RPM */
	rpm_fd = open (filename, O_RDONLY);
	if (rpm_fd < 0) {
		goto bail;
	}
	if (read_rpm_lead (rpm_fd) != 0) {
		goto bail;
	}
	if (read_rpm_signature (rpm_fd, &signature, &bytes) != 0) {
		goto bail_nosig;
	}

	/* must write the signature into a temporary file for gpg */
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
		bytes = read (rpm_fd, buffer, sizeof (buffer));
		if (bytes <= 0) {
			break;
		}
		if (write (stdin_fd, buffer, bytes) != bytes) {
			break;
		}
	}
	close (rpm_fd);
	rpm_fd = -1;
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
	if (signature != NULL) {
		g_free (signature);
	}
	return 2;

bail:
	if (rpm_fd >= 0) {
		close (rpm_fd);
	}
	if (key_fd >= 0) {
		close (key_fd);
	}
	if (temp_filename != NULL) {
		unlink (temp_filename);
	}
	if (signature != NULL) {
		g_free (signature);
	}
	return -1;
}

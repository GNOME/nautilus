/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Helix Code, Inc
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Joe Shaw <joe@helixcode.com>
 *          J. Shane Culpepper <pepper@eazel.com>
 */

/* Most of this code is taken directly from Joe Shaw's Helix Code install / Updater
 * with a few very minor changes made by me. */

#include <config.h>
#include "helixcode-utils.h"

static float determine_redhat_version (void);
static float determine_mandrake_version (void);
static float determine_turbolinux_version (void);
static float determine_suse_version (void);
static float determine_debian_version (void);

char*
xml_get_value (xmlNode* node, const char* name) {
	char* ret;
	xmlNode *child;

	ret = xmlGetProp (node, name);
	if (ret) {
		return ret;
	}
	child = node->childs;
	while (child) {
		if (g_strcasecmp (child->name, name) == 0) {
			ret = xmlNodeGetContent (child);
			if (ret) {
				return ret;
			}
		}
		child = child->next;
	}
	return NULL;
} /* end xml_get_value */

xmlDocPtr
prune_xml (char* xmlbuf) {
	xmlDocPtr doc;
	char* newbuf;
	int length;
	int i;

	newbuf = strstr(xmlbuf, "<?xml");
	if (!newbuf) {
		return NULL;
	}
	length = strlen (newbuf);
	for (i = 0; i < length; i++) {
		if (newbuf[i] == '\0') {
			newbuf[i] = ' ';
		}
	}
	newbuf[length] = '\0';
	doc = xmlParseMemory (newbuf, length);

	if (!doc) {
		fprintf(stderr, "***Could not prune package file !***\n");
		return NULL;
	}

	return doc;
} /* end prune_xml */

gboolean
check_for_root_user () {
	uid_t uid;

	uid = getuid ();
	if (uid == 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
} /* end check_for_root_user */

gboolean
check_for_redhat () {
	if (g_file_exists ("/etc/redhat-release") != 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
} /* end check_for_redhat */


/* FIXME - the following functions are not closing fd correctly.  This needs
   to be fixed and the functions need to be cleaned up.  They are ugly right
   now.  Bug #908 in Bugzilla.
 */

static float
determine_redhat_version () {

	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/redhat-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	buf[1023] = '\0';
	/* These check for LinuxPPC. For whatever reason, they use
	   /etc/redhat-release */
	text = strstr (buf, "1999");
	if (text)
		return 1999;
	text = strstr (buf, "2000");
	if (text)
		return 2000;
	text = strstr (buf, "6.0");
	if (text)
		return 6.0;
	text = strstr (buf, "6.");
	if (text) {
		v = g_strndup (text, 3);
		sscanf (v, "%f", &version);
		g_free (v);
		return version;
	}
	text = strstr (buf, "5.");
	if (text) {
		v = g_strndup (text, 3);
		sscanf (v, "%f", &version);
		g_free (v);
		return version;
	}
	return 0.0;
} /* determine_redhat_version */

static float
determine_mandrake_version () {

	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/mandrake-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	buf[1023] = '\0';
	text = strstr (buf, "6.0");
	if (text)
		return 6.0;
	text = strstr (buf, "6.");
	if (text) {
		v = g_strndup (text, 3);
		sscanf (v, "%f", &version);
		g_free (v);
		return version;
	}
	text = strstr (buf, "7.");
	if (text) {
		v = g_strndup (text, 3);
		sscanf (v, "%f", &version);
		g_free (v);
		return version;
	}
	return 0.0;
} /* determine_mandrake_version */

static float
determine_turbolinux_version () {

	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/turbolinux-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	buf[1023] = '\0';
	text = strstr (buf, "7.");
	if (text)
		return 7;
	text = strstr (buf, "6.");
	if (text) {
		v = g_strndup (text, 3);
		sscanf (v, "%f", &version);
		g_free (v);
		return version;
	}
	text = strstr (buf, "4.");
	if (text) {
		v = g_strndup (text, 3);
		sscanf (v, "%f", &version);
		g_free (v);
		return version;
	}
	return 0.0;
} /* determine_turbolinux_version */

static float
determine_suse_version () {

	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/SuSE-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	buf[1023] = '\0';
	text = strstr (buf, "6.");
	if (!text)
		return 0.0;
	v = g_strndup (text, 3);
	if (!v)
		return 0.0;
	sscanf (v, "%f", &version);
	g_free (v);
	if (version >= 2.2)
		return version;
	else
		return 0.0;
} /* determine_suse_version */

static float
determine_debian_version () {

	int fd;
	char buf[1024];
        float version;
	
	fd = open ("/etc/debian_version", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	buf[1023] = '\0';
	sscanf (buf, "%f", &version);
	if (version < 2.1)
		return 0.0;
	return version;
} /* determine_debian_version */

DistributionType
determine_distribution_type (void) {

	float version;
	DistributionType rv;

	/* Check for TurboLinux */
	if (g_file_exists ("/etc/turbolinux-release")) {
		version = determine_turbolinux_version ();
		if (version >= 7) {
			rv = DISTRO_TURBOLINUX_6;
		}
		else if (version >= 6) {
			rv = DISTRO_TURBOLINUX_6;
		}
		else if (version >= 4) {
			rv = DISTRO_TURBOLINUX_4;
		}
		return rv;
	}
	/* Check for Mandrake */
	if (g_file_exists ("/etc/mandrake-release")) {
		version = determine_mandrake_version ();
		if (version >= 7) {
			rv = DISTRO_MANDRAKE_7;
		}
		else if (version > 6) {
			rv = DISTRO_MANDRAKE_6_1;
		}
		else {
			rv = DISTRO_UNKNOWN;
		}
		return rv;
	}
	/* Check for SuSE */
	else if (g_file_exists ("/etc/SuSE-release")) {
		version = determine_suse_version ();
		if (version) {
			rv = DISTRO_SUSE;
		}
		else {
			rv = DISTRO_UNKNOWN;
		}
		return rv;
	}
	/* Check for Corel */
	else if (g_file_exists ("/etc/environment.corel")) {
	        rv = DISTRO_COREL;
		return rv;
	}
	/* Check for Debian */
	else if (g_file_exists ("/etc/debian_version")) {
		version = determine_debian_version ();
		if (version == 2.1) {
			rv = DISTRO_DEBIAN_2_1;
		}
		else if (version >= 2.2) {
			rv = DISTRO_DEBIAN_2_2;
		}
		else {
			rv = DISTRO_UNKNOWN;
		}
		return rv;
	}
	/* Check for Caldera */
	else if (g_file_exists ("/etc/coas")) {
		rv = DISTRO_CALDERA;
		return rv;
	}
	/* Check for Red Hat/LinuxPPC */
	/* This has to be checked last because many of the Red Hat knockoff
	   distros keep /etc/redhat-release around. */
	if (g_file_exists ("/etc/redhat-release")) {
		version = determine_redhat_version ();
		if (version >= 1999) {
			rv = DISTRO_LINUXPPC;
		}
		else if (version == 6.0) {
			rv = DISTRO_REDHAT_6;
		}
		else if (version == 6.1) {
			rv = DISTRO_REDHAT_6_1;
		}
		else if (version >= 6.2) {
			rv = DISTRO_REDHAT_6_2;
		}
		else if (version >= 5) {
			rv = DISTRO_REDHAT_5;
		}
		else {
			rv = DISTRO_UNKNOWN;
		}
		return rv;
	}
	/* If all fail... */
	rv = DISTRO_UNKNOWN;
	return rv;
} /* determine_distribution_type */

char*
get_distribution_name (const DistributionType* dtype) {

	char* rv;
	rv = NULL;

	while (rv != NULL) {
		switch ((int) dtype) {
			case DISTRO_REDHAT_5:
				rv = g_strdup ("RedHat Linux 5.x");
				break;
			case DISTRO_REDHAT_6:
				rv = g_strdup ("RedHat Linux 6.0");
				break;
			case DISTRO_REDHAT_6_1:
				rv = g_strdup ("RedHat Linux 6.1");
				break;
			case DISTRO_REDHAT_6_2:
				rv = g_strdup ("RedHat Linux 6.2");
				break;
			case DISTRO_MANDRAKE_6_1:
				rv = g_strdup ("Mandrake Linux 6.2");
				break;
			case DISTRO_MANDRAKE_7:
				rv = g_strdup ("Mandrake Linux 7.x");
				break;
			case DISTRO_CALDERA:
				rv = g_strdup ("Caldera OpenLinux");
				break;
			case DISTRO_SUSE:
				rv = g_strdup ("SuSe Linux");
				break;
			case DISTRO_LINUXPPC:
				rv = g_strdup ("LinuxPPC");
				break;
			case DISTRO_TURBOLINUX_4:
				rv = g_strdup ("TurboLinux 4.x");
				break;
			case DISTRO_TURBOLINUX_6:
				rv = g_strdup ("TurboLinux 6.x");
				break;
			case DISTRO_COREL:
				rv = g_strdup ("Corel Linux");
				break;
			case DISTRO_DEBIAN_2_1:
				rv = g_strdup ("Debian GNU/Linux 2.1");
				break;
			case DISTRO_DEBIAN_2_2:
				rv = g_strdup ("Debian GNU/Linux 2.2");
				break;
			case DISTRO_UNKNOWN:
				rv = g_strdup ("Unknown Linux Distribtion");
				break;
			default:
				rv = g_strdup ("INVALID_RETURN_VALUE");
				break;
		}
	}
	if (rv == "INVALID_RETURN_VALUE") {
		fprintf (stderr, "Invalid DistributionType !\n");
		exit (1);
	}
	else {
		return rv;
	}
} /* end get_distribution_name */

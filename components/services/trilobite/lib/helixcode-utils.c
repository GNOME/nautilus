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

void
determine_distro (DistributionType *dtype, char **distro_string) {

	float version;

	/* Check for TurboLinux */
	if (g_file_exists ("/etc/turbolinux-release")) {
		version = determine_turbolinux_version ();
		if (version >= 7) {
			*dtype = DISTRO_TURBOLINUX_6;
			*distro_string = "TurboLinux 7.0";
		}
		else if (version >= 6) {
			*dtype = DISTRO_TURBOLINUX_6;
			*distro_string = "TurboLinux 6.0";
		}
		else if (version >= 4) {
			*dtype = DISTRO_TURBOLINUX_4;
			*distro_string = "TurboLinux 4.0";
		}
		return;
	}
	/* Check for Mandrake */
	if (g_file_exists ("/etc/mandrake-release")) {
		version = determine_mandrake_version ();
		if (version >= 7) {
			*dtype = DISTRO_MANDRAKE_7;
			*distro_string = "Linux Mandrake 7.0 or newer";
		}
		else if (version > 6) {
			*dtype = DISTRO_MANDRAKE_6_1;
			*distro_string = "Linux Mandrake 6.1";
		}
		else {
			*dtype = DISTRO_UNSUPPORTED;
			*distro_string = "Unsupported Linux Mandrake";
		}
		return;
	}
	/* Check for SuSE */
	else if (g_file_exists ("/etc/SuSE-release")) {
		version = determine_suse_version ();
		if (version) {
			*dtype = DISTRO_SUSE;
			*distro_string = "SuSE";
		}
		else {
			*dtype = DISTRO_UNSUPPORTED;
			*distro_string = "Unsupported SuSE";
		}
		return;
	}
	/* Check for Corel */
	else if (g_file_exists ("/etc/environment.corel")) {
	        *dtype = DISTRO_COREL;
		*distro_string = "Corel Linux";
		return;
	}
	/* Check for Debian */
	else if (g_file_exists ("/etc/debian_version")) {
		version = determine_debian_version ();
		if (version == 2.1) {
			*dtype = DISTRO_DEBIAN_2_1;
			*distro_string = "Debian GNU/Linux 2.1";
		}
		else if (version >= 2.2) {
			*dtype = DISTRO_DEBIAN_2_2;
			*distro_string = "Debian GNU/Linux 2.2 or newer";
		}
		else {
			*dtype = DISTRO_UNSUPPORTED;
			*distro_string = "Unsupported Debian GNU/Linux";
		}
		return;
	}
	/* Check for Caldera */
	else if (g_file_exists ("/etc/coas")) {
		*dtype = DISTRO_CALDERA;
		*distro_string = "Caldera OpenLinux";
		return;
	}
	/* Check for Red Hat/LinuxPPC */
	/* This has to be checked last because many of the Red Hat knockoff
	   distros keep /etc/redhat-release around. */
	if (g_file_exists ("/etc/redhat-release")) {
		version = determine_redhat_version ();
		if (version >= 1999) {
			*dtype = DISTRO_LINUXPPC;
			*distro_string = "LinuxPPC";
		}
		else if (version == 6.0) {
			*dtype = DISTRO_REDHAT_6;
			*distro_string = "Red Hat Linux 6.0";
		}
		else if (version > 6) {
			*dtype = DISTRO_REDHAT_6;
			*distro_string = "Red Hat Linux 6.1 or newer";
		}
		else if (version >= 5) {
			*dtype = DISTRO_REDHAT_5;
			*distro_string = "Red Hat Linux 5.x";
		}
		else {
			*dtype = DISTRO_UNSUPPORTED;
			*distro_string = "Unsupported Red Hat";
		}
		return;
	}
	/* If all fail... */
	*dtype = DISTRO_UNKNOWN;
	*distro_string = "Unknown Distribution";

} /* determine_distro */

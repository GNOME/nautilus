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
xml_get_value (xmlNode* node, const char* name)
{
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

gboolean
check_for_root_user (void)
{
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
check_for_redhat (void)
{
	if (g_file_exists ("/etc/redhat-release") != 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
} /* end check_for_redhat */

/* FIXME bugzilla.eazel.com 908: - the following functions are not
 * closing fd correctly.  This needs to be fixed and the functions
 * need to be cleaned up.  They are ugly right now.
 */
static float
determine_redhat_version (void)
{

	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/redhat-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	close (fd);
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
determine_mandrake_version (void)
{
	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/mandrake-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	close (fd);
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
determine_turbolinux_version (void)
{
	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/turbolinux-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	close (fd);
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
determine_suse_version (void)
{
	int fd;
	char buf[1024];
	char* text;
	char* v;
	float version;

	fd = open ("/etc/SuSE-release", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	close (fd);
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
determine_debian_version (void)
{

	int fd;
	char buf[1024];
        float version;
	
	fd = open ("/etc/debian_version", O_RDONLY);
	g_return_val_if_fail (fd != -1, 0);
	read (fd, buf, 1023);
	close (fd);
	buf[1023] = '\0';
	sscanf (buf, "%f", &version);
	if (version < 2.1)
		return 0.0;
	return version;
} /* determine_debian_version */

DistributionType
determine_distribution_type (void)
{
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

char *
get_distribution_name (const DistributionType* dtype)
{
	switch ((int) dtype) {
	case DISTRO_REDHAT_5:
		return g_strdup ("RedHat Linux 5.x");
	case DISTRO_REDHAT_6:
		return g_strdup ("RedHat Linux 6.0");
	case DISTRO_REDHAT_6_1:
		return g_strdup ("RedHat Linux 6.1");
	case DISTRO_REDHAT_6_2:
		return g_strdup ("RedHat Linux 6.2");
	case DISTRO_MANDRAKE_6_1:
		return g_strdup ("Mandrake Linux 6.2");
	case DISTRO_MANDRAKE_7:
		return g_strdup ("Mandrake Linux 7.x");
	case DISTRO_CALDERA:
		return g_strdup ("Caldera OpenLinux");
	case DISTRO_SUSE:
		return g_strdup ("SuSe Linux");
	case DISTRO_LINUXPPC:
		return g_strdup ("LinuxPPC");
	case DISTRO_TURBOLINUX_4:
		return g_strdup ("TurboLinux 4.x");
	case DISTRO_TURBOLINUX_6:
		return g_strdup ("TurboLinux 6.x");
	case DISTRO_COREL:
		return g_strdup ("Corel Linux");
	case DISTRO_DEBIAN_2_1:
		return g_strdup ("Debian GNU/Linux 2.1");
	case DISTRO_DEBIAN_2_2:
		return g_strdup ("Debian GNU/Linux 2.2");
	case DISTRO_UNKNOWN:
		return g_strdup ("unknown Linux distribution");
	default:
		fprintf (stderr, "Invalid DistributionType !\n");
		exit (1);
	}
} /* end get_distribution_name */

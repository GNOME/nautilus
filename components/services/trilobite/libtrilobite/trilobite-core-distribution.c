/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 */

/*
  This is based in the distribution stuff from helixcode-utils.c, from
  the HelixCode Installer.
  I had to revamp it because it used some evil float stuff (redhat v. 6.09999)
  and it's enums were evil.
 */

#include <config.h>
#include "trilobite-core-distribution.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* FIXME bugzilla.eazel.com 908
   need to implement the rest of the determine_FOO_version
*/
static void
determine_turbolinux_version (DistributionInfo *distinfo)
{
	g_assert_not_reached ();
}

static void
determine_mandrake_version (DistributionInfo *distinfo)
{
	g_assert_not_reached ();
}

static void
determine_suse_version (DistributionInfo *distinfo)
{
	g_assert_not_reached ();
}

static void
determine_debian_version (DistributionInfo *distinfo)
{
	g_assert_not_reached ();
}

static void
determine_redhat_version (DistributionInfo *distinfo)
{
	FILE *fd;
	char buf[1024];
	char* text;
	char* v;
	int version_major, version_minor;

	g_assert (distinfo != NULL);

	fd = fopen ("/etc/redhat-release", "rt");
	g_return_if_fail (fd != NULL);

	fread ((char*)buf, 1023, 1, fd);
	fclose (fd);

	buf[1023] = '\0';
	/* 
	   These check for LinuxPPC. For whatever reason, they use
	   /etc/redhat-release
	*/
	text = strstr (buf, "1999");
	if (text) {
		distinfo->version_major = 1999;
		return;
	}
	text = strstr (buf, "2000");
	if (text) {
		distinfo->version_major = 2000;
		return;
	}
	text = strstr (buf, "release");
	if (text) {
		text += 8;
		v = g_strndup (text, 3);
		sscanf (v, "%d.%d", &version_major, &version_minor);
		g_free (v);
		distinfo->version_major = version_major;
		distinfo->version_minor = version_minor;
		return;
	}
}

DistributionInfo 
trilobite_get_distribution ()
{
	DistributionInfo distinfo;

	distinfo.name = DISTRO_UNKNOWN;
	distinfo.version_major = -1;
	distinfo.version_minor = -1;
	/* Check for TurboLinux */
	if (g_file_exists ("/etc/turbolinux-release")) {
		distinfo.name = DISTRO_TURBOLINUX;
		determine_turbolinux_version (&distinfo);
	} 
	/* Check for Mandrake */
	else if (g_file_exists ("/etc/mandrake-release")) {
		distinfo.name = DISTRO_MANDRAKE;
		determine_mandrake_version (&distinfo);
	} 
	/* Check for SuSE */
	if (g_file_exists ("/etc/SuSE-release")) {
		distinfo.name = DISTRO_SUSE;
		determine_suse_version (&distinfo);
	} 
	/* Check for Corel */
	else if (g_file_exists ("/etc/environment.corel")) {
	        distinfo.name = DISTRO_COREL;
	} 
	/* Check for Debian */
	else if (g_file_exists ("/etc/debian_version")) {
		distinfo.name = DISTRO_DEBIAN;
		determine_debian_version (&distinfo);
	} 
	/* Check for Caldera */
	else if (g_file_exists ("/etc/coas")) {
		distinfo.name = DISTRO_CALDERA;
	} 
	/* Check for Red Hat/LinuxPPC */
	/* This has to be checked last because many of the Red Hat knockoff
	   distros keep /etc/redhat-release around. */
	else if (g_file_exists ("/etc/redhat-release")) {
		distinfo.name = DISTRO_REDHAT;
		determine_redhat_version (&distinfo);
	}
	return distinfo;
}

char* 
trilobite_get_distribution_name (DistributionInfo distinfo,
				 gboolean show_version)
{
	char *result;
	char *name;
	char *version;
	char *arch;

	version = g_strdup ("");
	arch    = g_strdup ("");     /* We don't set the arch type yet */

	switch (distinfo.name) {
	case DISTRO_REDHAT:
		name = g_strdup ("Red Hat Linux");
		break;
	case DISTRO_DEBIAN:
		name = g_strdup ("Debian");
		break;
	case DISTRO_CALDERA:
		name = g_strdup ("Caldera");
		break;
	case DISTRO_SUSE:
		name = g_strdup ("S.u.S.E.");
		break;
	case DISTRO_LINUXPPC:
		name = g_strdup ("LinuxPPC");
		break;
	case DISTRO_TURBOLINUX:
		name = g_strdup ("TurboLinux");
		break;
	case DISTRO_COREL:
		name = g_strdup ("Corel");
		break;
	case DISTRO_MANDRAKE:
		name = g_strdup ("Mandrake");
		break;
	default:
		name = g_strdup ("Unknown");
		break;
	}
	
	if (show_version) {		
		if (distinfo.version_major >= 0 && distinfo.version_minor >= 0) {
			g_free (version);
			version = g_strdup_printf (" %d.%d", distinfo.version_major, distinfo.version_minor);
		} else if (distinfo.version_major >= 0) {
			g_free (version);
			version = g_strdup_printf (" %d", distinfo.version_major);
		} 
	}

	/* This slightly odd g_strconcat is odd, so it
	   eg. can be expanded to have arch */
	result = g_strconcat (name, 
			      version,
			      arch,
			      NULL);

	g_free (name);
	g_free (version);
	g_free (arch);

	return result;
}

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
 
#ifndef __HELIXCODE_INSTALL_UTILS_H__
#define __HELIXCODE_INSTALL_UTILS_H__

#include <gnome.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

typedef enum _DistributionType DistributionType;

enum _DistributionType {
        DISTRO_REDHAT_5,           /* Red Hat 5.x (glibc 2.0) */
        DISTRO_REDHAT_6,           /* Red Hat 6.1 (glibc 2.1) */
        DISTRO_MANDRAKE_6_1,       /* Mandrake 6.1 (glibc 2.1) */
        DISTRO_MANDRAKE_7,         /* Mandrake 7.0 */
        DISTRO_CALDERA,            /* Caldera */
        DISTRO_SUSE,               /* SuSE */
        DISTRO_LINUXPPC,           /* LinuxPPC */
        DISTRO_TURBOLINUX_4,       /* TurboLinux 4 */
        DISTRO_TURBOLINUX_6,       /* TurboLinux 6 */
        DISTRO_COREL,              /* Corel Linux */
        DISTRO_DEBIAN_2_1,         /* Debian Linux 2.1 */
        DISTRO_DEBIAN_2_2,         /* Debian Linux 2.2 */
        DISTRO_UNSUPPORTED,        /* unsupported distribution */
        DISTRO_UNKNOWN             /* unknown distribution */
};

char* xml_get_value (xmlNode* node, const char* name);
xmlDocPtr prune_xml (char* xmlbuf);
gboolean check_for_root_user (void);
gboolean check_for_redhat (void);
void determine_distro(DistributionType *dtype, char **distro_string);


#endif /* __HELIXCODE_INSTALL_UTILS_H__ */

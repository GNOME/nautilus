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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-configuration.xml
 * file and install a services generated packages.xml.
 */

#include "eazel-install-lib.h"

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

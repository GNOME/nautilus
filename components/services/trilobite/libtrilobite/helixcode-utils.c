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


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
 
#ifndef HELIXCODE_INSTALL_UTILS_H
#define HELIXCODE_INSTALL_UTILS_H

#include <gnome.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

char* xml_get_value (xmlNode* node, const char* name);
gboolean check_for_root_user (void);
gboolean check_for_redhat (void);

#endif /* HELIXCODE_INSTALL_UTILS_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-actions.h - uri-specific versions of mime action functions

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#ifndef NAUTILUS_MIME_ACTIONS_H
#define NAUTILUS_MIME_ACTIONS_H

#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <libnautilus-private/nautilus-file.h>

NautilusFileAttributes   nautilus_mime_actions_get_minimum_file_attributes	(void);
NautilusFileAttributes   nautilus_mime_actions_get_full_file_attributes		(void);

GnomeVFSMimeApplication *nautilus_mime_get_default_application_for_file		(NautilusFile *file);
GList *                  nautilus_mime_get_open_with_applications_for_file	(NautilusFile *file);
GList *			 nautilus_mime_get_applications_for_file		(NautilusFile *file);

gboolean                 nautilus_mime_has_any_applications_for_file		(NautilusFile *file);

#endif /* NAUTILUS_MIME_ACTIONS_H */

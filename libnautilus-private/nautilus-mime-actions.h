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


GList                   *nautilus_mime_actions_get_minimum_file_attributes         (void);
GList                   *nautilus_mime_actions_get_full_file_attributes            (void);
gboolean                 nautilus_mime_actions_file_needs_full_file_attributes     (NautilusFile           *file);
GnomeVFSMimeActionType   nautilus_mime_get_default_action_type_for_file            (NautilusFile           *file);
GnomeVFSMimeAction *     nautilus_mime_get_default_action_for_file                 (NautilusFile           *file);
GnomeVFSMimeApplication *nautilus_mime_get_default_application_for_file            (NautilusFile           *file);
gboolean                 nautilus_mime_is_default_application_for_file_user_chosen (NautilusFile           *file);
OAF_ServerInfo *         nautilus_mime_get_default_component_for_file              (NautilusFile           *file);
gboolean                 nautilus_mime_is_default_component_for_file_user_chosen   (NautilusFile           *file);
GList *                  nautilus_mime_get_short_list_applications_for_file        (NautilusFile           *file);
GList *                  nautilus_mime_get_short_list_components_for_file          (NautilusFile           *file);
GList *                  nautilus_mime_get_all_applications_for_file               (NautilusFile           *file);
GList *                  nautilus_mime_get_all_components_for_file                 (NautilusFile           *file);
gboolean                 nautilus_mime_has_any_components_for_file                 (NautilusFile           *file);
gboolean                 nautilus_mime_has_any_applications_for_file               (NautilusFile           *file);
gboolean                 nautilus_mime_has_any_applications_for_file_type          (NautilusFile           *file);
GnomeVFSResult           nautilus_mime_set_default_action_type_for_file            (NautilusFile           *file,
										    GnomeVFSMimeActionType  action_type);
GnomeVFSResult           nautilus_mime_set_default_application_for_file            (NautilusFile           *file,
										    const char             *application_id);
GnomeVFSResult           nautilus_mime_set_default_component_for_file              (NautilusFile           *file,
										    const char             *component_iid);
/* Stored as delta to current user level */
GnomeVFSResult           nautilus_mime_set_short_list_applications_for_file        (NautilusFile           *file,
										    GList                  *components);
GnomeVFSResult           nautilus_mime_add_application_to_short_list_for_file      (NautilusFile           *file,
										    const char             *application_id);
GnomeVFSResult           nautilus_mime_remove_application_from_short_list_for_file (NautilusFile           *file,
										    const char             *application_id);
GnomeVFSResult           nautilus_mime_set_short_list_components_for_file          (NautilusFile           *file,
										    GList                  *components);
GnomeVFSResult           nautilus_mime_add_component_to_short_list_for_file        (NautilusFile           *file,
										    const char             *iid);
GnomeVFSResult           nautilus_mime_remove_component_from_short_list_for_file   (NautilusFile           *file,
										    const char             *iid);

/* No way to override system list; can only add. */
GnomeVFSResult           nautilus_mime_extend_all_applications_for_file            (NautilusFile           *file,
										    GList                  *applications);
/* Only "user" entries may be removed. */
GnomeVFSResult           nautilus_mime_remove_from_all_applications_for_file       (NautilusFile           *file,
										    GList                  *applications);
gboolean                 nautilus_mime_has_any_components_for_uri_scheme           (const char             *uri_scheme);


/* No way to add to all components; oafinfo database assumed trusted in this regard. */

#endif /* NAUTILUS_MIME_ACTIONS_H */

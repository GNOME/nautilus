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

#ifndef NAUTILUS_VIEW_QUERY_H
#define NAUTILUS_VIEW_QUERY_H

#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <libnautilus-private/nautilus-file.h>


Bonobo_ServerInfo *      nautilus_view_query_get_default_component_for_file              (NautilusFile           *file);
GnomeVFSResult           nautilus_view_query_set_default_component_for_file
         (NautilusFile           *file,
	  const char             *iid);
Bonobo_ServerInfo *      nautilus_view_query_get_fallback_component_for_file             (NautilusFile           *file);
GList *                  nautilus_view_query_get_components_for_file
         (NautilusFile           *file);
gboolean                 nautilus_view_query_has_any_components_for_file (NautilusFile *file);

gboolean                 nautilus_view_query_has_any_components_for_uri_scheme           (const char             *uri_scheme);


/* Bonobo components for popup menus and property pages - should probably
 * be moved into the bonobo extensions */ 
GList *                  nautilus_view_query_get_popup_components_for_file               (NautilusFile           *file);
GList *                  nautilus_view_query_get_popup_components_for_files              (GList                  *files);
GList *                  nautilus_view_query_get_property_components_for_file            (NautilusFile           *file);
GList *                  nautilus_view_query_get_property_components_for_files           (GList                  *files);

#endif /* NAUTILUS_MIME_ACTIONS_H */

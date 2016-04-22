/*
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NAUTILUS_QUERY_EDITOR_H
#define NAUTILUS_QUERY_EDITOR_H

#include <gtk/gtk.h>

#include "nautilus-query.h"

#define NAUTILUS_TYPE_QUERY_EDITOR nautilus_query_editor_get_type()

G_DECLARE_DERIVABLE_TYPE (NautilusQueryEditor, nautilus_query_editor, NAUTILUS, QUERY_EDITOR, GtkSearchBar)

struct _NautilusQueryEditorClass {
        GtkSearchBarClass parent_class;

	void (* changed)   (NautilusQueryEditor  *editor,
			    NautilusQuery        *query,
			    gboolean              reload);
	void (* cancel)    (NautilusQueryEditor *editor);
	void (* activated) (NautilusQueryEditor *editor);
};

#include "nautilus-window-slot.h"

GType      nautilus_query_editor_get_type     	   (void);
GtkWidget* nautilus_query_editor_new          	   (void);

NautilusQuery *nautilus_query_editor_get_query   (NautilusQueryEditor *editor);
void           nautilus_query_editor_set_query   (NautilusQueryEditor *editor,
						  NautilusQuery       *query);
GFile *        nautilus_query_editor_get_location (NautilusQueryEditor *editor);
void           nautilus_query_editor_set_location (NautilusQueryEditor *editor,
						   GFile               *location);
void           nautilus_query_editor_set_text     (NautilusQueryEditor *editor,
                                                   const gchar         *text);

#endif /* NAUTILUS_QUERY_EDITOR_H */

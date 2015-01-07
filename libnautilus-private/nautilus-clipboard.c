/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-clipboard.c
 *
 * Nautilus Clipboard support.  For now, routines to support component cut
 * and paste.
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-clipboard.h"
#include "nautilus-file-utilities.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

static GList *
convert_lines_to_str_list (char **lines, gboolean *cut)
{
	int i;
	GList *result;

	if (cut) {
		*cut = FALSE;
	}

	if (lines[0] == NULL) {
		return NULL;
	}

	if (strcmp (lines[0], "cut") == 0) {
		if (cut) {
			*cut = TRUE;
		}
	} else if (strcmp (lines[0], "copy") != 0) {
		return NULL;
	}

	result = NULL;
	for (i = 1; lines[i] != NULL; i++) {
		result = g_list_prepend (result, g_strdup (lines[i]));
	}
	return g_list_reverse (result);
}

GList*
nautilus_clipboard_get_uri_list_from_selection_data (GtkSelectionData *selection_data,
						     gboolean *cut,
						     GdkAtom copied_files_atom)
{
	GList *items;
	char **lines;

	if (gtk_selection_data_get_data_type (selection_data) != copied_files_atom
	    || gtk_selection_data_get_length (selection_data) <= 0) {
		items = NULL;
	} else {
		gchar *data;
		/* Not sure why it's legal to assume there's an extra byte
		 * past the end of the selection data that it's safe to write
		 * to. But gtk_editable_selection_received does this, so I
		 * think it is OK.
		 */
		data = (gchar *) gtk_selection_data_get_data (selection_data);
		data[gtk_selection_data_get_length (selection_data)] = '\0';
		lines = g_strsplit (data, "\n", 0);
		items = convert_lines_to_str_list (lines, cut);
		g_strfreev (lines);
	}
	
	return items;
}

GtkClipboard *
nautilus_clipboard_get (GtkWidget *widget)
{
	return gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (widget)),
					      GDK_SELECTION_CLIPBOARD);
}

void
nautilus_clipboard_clear_if_colliding_uris (GtkWidget *widget,
					    const GList *item_uris,
					    GdkAtom copied_files_atom)
{
	GtkSelectionData *data;
	GList *clipboard_item_uris, *l;
	gboolean collision;

	collision = FALSE;
	data = gtk_clipboard_wait_for_contents (nautilus_clipboard_get (widget),
						copied_files_atom);
	if (data == NULL) {
		return;
	}

	clipboard_item_uris = nautilus_clipboard_get_uri_list_from_selection_data (data, NULL,
										   copied_files_atom);

	for (l = (GList *) item_uris; l; l = l->next) {
		if (g_list_find_custom ((GList *) item_uris, l->data,
					(GCompareFunc) g_strcmp0)) {
			collision = TRUE;
			break;
		}
	}
	
	if (collision) {
		gtk_clipboard_clear (nautilus_clipboard_get (widget));
	}
	
	if (clipboard_item_uris) {
		g_list_free_full (clipboard_item_uris, g_free);
	}
}

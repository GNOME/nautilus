
/* nautilus-dnd.h - Common Drag & drop handling code shared by the icon container
   and the list view.

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
   see <http://www.gnu.org/licenses/>.

   Authors: Pavel Cisler <pavel@eazel.com>,
	    Ettore Perazzoli <ettore@gnu.org>
*/

#pragma once

#include <gtk/gtk.h>
#include "nautilus-file.h"

#define HOVER_TIMEOUT 500

/* Drag & Drop target names. */
#define NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE "x-special/gnome-icon-list"


/* drag&drop-related information. */
typedef struct {
    GdkContentFormats *formats;

	/* Stuff saved at "receive data" time needed later in the drag. */
	GtkSelectionData *selection_data;

	/* Start of the drag, in window coordinates. */
	int start_x, start_y;

	/* List of NautilusDragSelectionItems, representing items being dragged, or NULL
	 * if data about them has not been received from the source yet.
	 */
	GList *selection_list;

	/* cache of selected URIs, representing items being dragged */
	GList *selection_cache;

        /* File selection list information request handler, for the call for
         * information (mostly the file system info, in order to know if we want
         * co copy or move the files) about the files being dragged, that can
         * come from another nautilus process, like the desktop. */
        NautilusFileListHandle *file_list_info_handler;

	/* has the drop occurred ? */
	gboolean drop_occurred;

	/* whether or not need to clean up the previous dnd data */
	gboolean need_to_destroy;

	/* autoscrolling during dragging */
	int auto_scroll_timeout_id;
	gboolean waiting_to_autoscroll;
	gint64 start_auto_scroll_in;

        /* source context actions. Used for peek the actions using a GdkDrop
         * source at drag-begin time when they are not available yet (they become
         * available at drag-motion time) */
        GdkDragAction source_actions;

} NautilusDragInfo;

typedef void		(* NautilusDragEachSelectedItemDataGet)	(const char *url, 
								 int x, int y, int w, int h, 
								 gpointer data);
typedef void		(* NautilusDragEachSelectedItemIterator)	(NautilusDragEachSelectedItemDataGet iteratee, 
								 gpointer iterator_context, 
								 gpointer data);

void			    nautilus_drag_finalize			(NautilusDragInfo		      *drag_info);
NautilusDragSelectionItem  *nautilus_drag_selection_item_new		(void);
void			    nautilus_drag_destroy_selection_list	(GList				      *selection_list);
GList			   *nautilus_drag_build_selection_list		(GtkSelectionData		      *data);

GList *			    nautilus_drag_uri_list_from_selection_list	(const GList			      *selection_list);

GList *			    nautilus_drag_uri_list_from_array		(GStrv                                 uris);

gboolean		    nautilus_drag_items_local			(const char			      *target_uri,
									 const GList			      *selection_list);
gboolean		    nautilus_drag_uris_local			(const char			      *target_uri,
									 const GList			      *source_uri_list);
GdkDragAction               nautilus_get_drop_actions_for_icons         (const char                           *target_uri,
                                                                         const GList                          *items);
GdkDragAction               nautilus_get_drop_actions_for_uri           (const char                           *uri);
GList			   *nautilus_drag_create_selection_cache	(gpointer			       container_context,
									 NautilusDragEachSelectedItemIterator  each_selected_item_iterator);
gboolean		    nautilus_drag_drag_data_get_from_cache	(GList				      *cache,
									 GtkSelectionData		      *selection_data);
int			    nautilus_drag_modifier_based_action		(int				       default_action,
									 int				       non_default_action);

GdkDragAction		    nautilus_drag_drop_action_ask		(GtkWidget			      *widget,
									 GdkDragAction			       possible_actions);

gboolean		    nautilus_drag_autoscroll_in_scroll_region	(GtkWidget			      *widget);
void			    nautilus_drag_autoscroll_calculate_delta	(GtkWidget			      *widget,
									 float				      *x_scroll_delta,
									 float				      *y_scroll_delta);
void			    nautilus_drag_autoscroll_start		(NautilusDragInfo		      *drag_info,
									 GtkWidget			      *widget,
									 GSourceFunc			       callback,
									 gpointer			       user_data);
void			    nautilus_drag_autoscroll_stop		(NautilusDragInfo		      *drag_info);

NautilusDragInfo *          nautilus_drag_get_source_data                 (GdkDrag                            *context);

GList *                     nautilus_drag_file_list_from_selection_list   (const GList                        *selection_list);


gboolean                    nautilus_content_formats_include_text         (GdkContentFormats                  *formats);
gboolean                    nautilus_content_formats_include_uri          (GdkContentFormats                  *formats);

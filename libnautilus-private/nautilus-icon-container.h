/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-icon-container.h - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_ICON_CONTAINER_H
#define NAUTILUS_ICON_CONTAINER_H

#include <libgnomeui/gnome-canvas.h>
#include "nautilus-icon-factory.h"

typedef struct NautilusIconContainer NautilusIconContainer;
typedef struct NautilusIconContainerClass NautilusIconContainerClass;
typedef struct NautilusIconContainerDetails NautilusIconContainerDetails;

typedef struct NautilusIconData NautilusIconData;

#define NAUTILUS_ICON_CONTAINER(obj) \
	GTK_CHECK_CAST (obj, nautilus_icon_container_get_type (), NautilusIconContainer)
#define NAUTILUS_ICON_CONTAINER_CLASS(k) \
	GTK_CHECK_CLASS_CAST (k, nautilus_icon_container_get_type (), NautilusIconListView)
#define NAUTILUS_IS_ICON_CONTAINER(obj) \
	GTK_CHECK_TYPE (obj, nautilus_icon_container_get_type ())

#define NAUTILUS_ICON_CONTAINER_ICON_DATA(pointer) \
	((NautilusIconData *) (pointer))

struct NautilusIconContainer {
	GnomeCanvas canvas;
	NautilusIconContainerDetails *details;
};

typedef struct {
	int x;
	int y;
	double scale_x;
	double scale_y;
} NautilusIconPosition;

struct NautilusIconContainerClass {
	GnomeCanvasClass parent_class;

	/* Operations on the container. */
	int          (* button_press) 	          (NautilusIconContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NautilusIconContainer *container);
	void         (* middle_click) 		  (NautilusIconContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NautilusIconContainer *container,
						   NautilusIconData *data);
	void         (* context_click_selection)  (NautilusIconContainer *container);
	void	     (* move_copy_items)	  (NautilusIconContainer *container,
						   GList *item_uris,
						   const GdkPoint *relative_item_points,
						   const char *target_uri,
						   int copy_action,
						   int x,
						   int y);

	/* Queries on the container for subclass/client.
	 * These must be implemented. The default "do nothing" is not good enough.
	 */
	char *	     (* get_container_uri)	  (NautilusIconContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NautilusIconContainer *container,
						   NautilusIconData *target, 
						   const char *item_uri);
	gboolean     (* get_stored_icon_position) (NautilusIconContainer *container,
						   NautilusIconData *data,
						   NautilusIconPosition *position);
	NautilusScalableIcon *
	             (* get_icon_images)          (NautilusIconContainer *container,
						   NautilusIconData *data,
						   const char *modifier,
						   GList **emblem_icons);
	void         (* get_icon_text)            (NautilusIconContainer *container,
						   NautilusIconData *data,
						   char **editable_text,
						   char **additional_text);
	char *       (* get_icon_uri)             (NautilusIconContainer *container,
						   NautilusIconData *data);
	int          (* compare_icons)            (NautilusIconContainer *container,
						   NautilusIconData *icon_a,
						   NautilusIconData *icon_b);

	/* Notifications for the whole container. */
	void         (* selection_changed) 	  (NautilusIconContainer *container);
	void         (* layout_changed)           (NautilusIconContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NautilusIconContainer *container,
						   NautilusIconData *data,
						   const NautilusIconPosition *position);
	void         (* icon_text_changed)        (NautilusIconContainer *container,
						   NautilusIconData *data,
						   const char *text);
	int	     (* preview)		  (NautilusIconContainer *container,
						   NautilusIconData *data,
						   gboolean start_flag);
};

/* GtkObject */
guint      nautilus_icon_container_get_type                      (void);
GtkWidget *nautilus_icon_container_new                           (void);


/* adding, removing, and managing icons */
void       nautilus_icon_container_clear                         (NautilusIconContainer *view);
gboolean   nautilus_icon_container_add                           (NautilusIconContainer *view,
								  NautilusIconData      *data);
gboolean   nautilus_icon_container_remove                        (NautilusIconContainer *view,
								  NautilusIconData      *data);
void       nautilus_icon_container_request_update                (NautilusIconContainer *view,
								  NautilusIconData      *data);
void       nautilus_icon_container_request_update_all            (NautilusIconContainer *container);


/* control the layout */
gboolean   nautilus_icon_container_is_auto_layout                (NautilusIconContainer *container);
void       nautilus_icon_container_set_auto_layout               (NautilusIconContainer *container,
								  gboolean               auto_layout);
void       nautilus_icon_container_sort                          (NautilusIconContainer *container);
void       nautilus_icon_container_freeze_icon_positions         (NautilusIconContainer *container);


/* operations on all icons */
void       nautilus_icon_container_unselect_all                  (NautilusIconContainer *view);
void       nautilus_icon_container_select_all                    (NautilusIconContainer *view);


/* operations on the selection */
GList     *nautilus_icon_container_get_selection                 (NautilusIconContainer *view);
void       nautilus_icon_container_set_selection                 (NautilusIconContainer *view,
								  GList                 *selection);
gboolean   nautilus_icon_container_has_stretch_handles           (NautilusIconContainer *container);
gboolean   nautilus_icon_container_is_stretched                  (NautilusIconContainer *container);
void       nautilus_icon_container_show_stretch_handles          (NautilusIconContainer *container);
void       nautilus_icon_container_unstretch                     (NautilusIconContainer *container);
void       nautilus_icon_container_start_renaming_selected_item  (NautilusIconContainer *container);
gboolean   nautilus_icon_container_is_renaming                   (NautilusIconContainer *container);


/* label color for items to use */
guint32    nautilus_icon_container_get_label_color		 (NautilusIconContainer *container,
								  gboolean first_line);

/* options */
int        nautilus_icon_container_get_zoom_level                (NautilusIconContainer *view);
void       nautilus_icon_container_set_zoom_level                (NautilusIconContainer *view,
								  int                    new_zoom_level);
void       nautilus_icon_container_set_single_click_mode         (NautilusIconContainer *container,
								  gboolean               single_click_mode);
void       nautilus_icon_container_enable_linger_selection       (NautilusIconContainer *view,
								  gboolean               enable);
gboolean   nautilus_icon_container_get_anti_aliased_mode	 (NautilusIconContainer *view);
void       nautilus_icon_container_set_anti_aliased_mode         (NautilusIconContainer *view,
								  gboolean               anti_aliased_mode);
void       nautilus_icon_container_set_label_font_for_zoom_level (NautilusIconContainer *container,
								  int                    zoom_level,
								  GdkFont               *font);

#endif

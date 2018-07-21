
/* gnome-canvas-container.h - Canvas container widget.

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
   see <http://www.gnu.org/licenses/>.

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@bentspoon.com>
*/

#pragma once

#include <eel/eel-canvas.h>

#include "nautilus-types.h"

#define NAUTILUS_TYPE_CANVAS_CONTAINER nautilus_canvas_container_get_type()
#define NAUTILUS_CANVAS_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CANVAS_CONTAINER, NautilusCanvasContainer))
#define NAUTILUS_CANVAS_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CANVAS_CONTAINER, NautilusCanvasContainerClass))
#define NAUTILUS_IS_CANVAS_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CANVAS_CONTAINER))
#define NAUTILUS_IS_CANVAS_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CANVAS_CONTAINER))
#define NAUTILUS_CANVAS_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_CANVAS_CONTAINER, NautilusCanvasContainerClass))


typedef void (* NautilusCanvasCallback) (NautilusFile *file,
					 gpointer callback_data);

typedef struct {
	int x;
	int y;
	double scale;
} NautilusCanvasPosition;

#define	NAUTILUS_CANVAS_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct _NautilusCanvasContainer        NautilusCanvasContainer;
typedef struct  NautilusCanvasContainerDetails NautilusCanvasContainerDetails;

struct _NautilusCanvasContainer {
	EelCanvas canvas;
	NautilusCanvasContainerDetails *details;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NautilusCanvasContainer, g_object_unref)

typedef struct {
	EelCanvasClass parent_slot;

	/* Operations on the container. */
	int          (* button_press) 	          (NautilusCanvasContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NautilusCanvasContainer *container,
						   GdkEventButton *event);
	void         (* middle_click) 		  (NautilusCanvasContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NautilusCanvasContainer *container,
						   NautilusFile            *file);
	void         (* activate_alternate)       (NautilusCanvasContainer *container,
						   NautilusFile            *file);
	void         (* activate_previewer)       (NautilusCanvasContainer *container,
						   GList *files,
						   GArray *locations);
	void         (* context_click_selection)  (NautilusCanvasContainer *container,
						   GdkEventButton *event);
	void	     (* move_copy_items)	  (NautilusCanvasContainer *container,
						   const GList *item_uris,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_uri_list)    	  (NautilusCanvasContainer *container,
						   const char *uri_list,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_text)		  (NautilusCanvasContainer *container,
						   const char *text,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_raw)		  (NautilusCanvasContainer *container,
						   char *raw_data,
						   int length,
						   const char *target_uri,
						   const char *direct_save_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_hover)		  (NautilusCanvasContainer *container,
						   const char *target_uri);

	/* Queries on the container for subclass/client.
	 * These must be implemented. The default "do nothing" is not good enough.
	 */
	char *	     (* get_container_uri)	  (NautilusCanvasContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not
	 * good enough, these are _not_ signals.
	 */
	NautilusIconInfo *(* get_icon_images)     (NautilusCanvasContainer *container,
						     NautilusFile           *file,
						     int canvas_size,
						     gboolean for_drag_accept);
	void         (* get_icon_text)            (NautilusCanvasContainer *container,
						     NautilusFile           *file,
						     char **editable_text,
						     char **additional_text,
						     gboolean include_invisible);
	char *       (* get_icon_description)     (NautilusCanvasContainer *container,
						     NautilusFile           *file);
	int          (* compare_icons)            (NautilusCanvasContainer *container,
						     NautilusFile           *canvas_a,
						     NautilusFile           *canvas_b);
	int          (* compare_icons_by_name)    (NautilusCanvasContainer *container,
						     NautilusFile           *canvas_a,
						     NautilusFile           *canvas_b);
	void         (* prioritize_thumbnailing)  (NautilusCanvasContainer *container,
						   NautilusFile           *file);

	/* Queries on icons for subclass/client.
	 * These must be implemented => These are signals !
	 * The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NautilusCanvasContainer *container,
						   NautilusFile            *file,
						   const char *item_uri);
	gboolean     (* get_stored_icon_position) (NautilusCanvasContainer *container,
						     NautilusFile          *icon,
						     NautilusCanvasPosition *position);
	char *       (* get_icon_uri)             (NautilusCanvasContainer *container,
						     NautilusFile          *icon);
	char *       (* get_icon_activation_uri)  (NautilusCanvasContainer *container,
						     NautilusFile          *file);
	char *       (* get_icon_drop_target_uri) (NautilusCanvasContainer *container,
						     NautilusFile          *icon);

	/* If canvas data is NULL, the layout timestamp of the container should be retrieved.
	 * That is the time when the container displayed a fully loaded directory with
	 * all canvas positions assigned.
	 *
	 * If canvas data is not NULL, the position timestamp of the canvas should be retrieved.
	 * That is the time when the file (i.e. canvas data payload) was last displayed in a
	 * fully loaded directory with all canvas positions assigned.
	 */
	gboolean     (* get_stored_layout_timestamp) (NautilusCanvasContainer *container,
						      NautilusFile           *file,
						      time_t *time);
	/* If canvas data is NULL, the layout timestamp of the container should be stored.
	 * If canvas data is not NULL, the position timestamp of the container should be stored.
	 */
	gboolean     (* store_layout_timestamp) (NautilusCanvasContainer *container,
						 NautilusFile           *file,
						 const time_t *time);

	/* Notifications for the whole container. */
	void	     (* band_select_started)	  (NautilusCanvasContainer *container);
	void	     (* band_select_ended)	  (NautilusCanvasContainer *container);
	void         (* selection_changed) 	  (NautilusCanvasContainer *container);
	void         (* layout_changed)           (NautilusCanvasContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NautilusCanvasContainer *container,
						     NautilusFile           *file,
						     const NautilusCanvasPosition *position);
	int	     (* preview)		  (NautilusCanvasContainer *container,
						   NautilusFile            *file,
						   gboolean start_flag);
        void         (* icon_added)               (NautilusCanvasContainer *container,
						     NautilusFile          *file);
        void         (* icon_removed)             (NautilusCanvasContainer *container,
						     NautilusFile          *file);
        void         (* cleared)                  (NautilusCanvasContainer *container);
	gboolean     (* start_interactive_search) (NautilusCanvasContainer *container);
} NautilusCanvasContainerClass;

/* GtkObject */
GType             nautilus_canvas_container_get_type                      (void);
GtkWidget *       nautilus_canvas_container_new                           (void);


/* adding, removing, and managing icons */
void              nautilus_canvas_container_clear                         (NautilusCanvasContainer  *view);
gboolean          nautilus_canvas_container_add                           (NautilusCanvasContainer  *view,
									   NautilusFile             *file);
void              nautilus_canvas_container_layout_now                    (NautilusCanvasContainer *container);
gboolean          nautilus_canvas_container_remove                        (NautilusCanvasContainer  *view,
									   NautilusFile             *file);
void              nautilus_canvas_container_for_each                      (NautilusCanvasContainer  *view,
									   NautilusCanvasCallback    callback,
									   gpointer                callback_data);
void              nautilus_canvas_container_request_update                (NautilusCanvasContainer  *view,
									   NautilusFile             *file);
void              nautilus_canvas_container_request_update_all            (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_reveal                        (NautilusCanvasContainer  *container,
									   NautilusFile             *file);
gboolean          nautilus_canvas_container_is_empty                      (NautilusCanvasContainer  *container);
NautilusFile     *nautilus_canvas_container_get_first_visible_icon        (NautilusCanvasContainer  *container);
NautilusFile     *nautilus_canvas_container_get_focused_icon              (NautilusCanvasContainer  *container);
GdkRectangle      *nautilus_canvas_container_get_icon_bounding_box          (NautilusCanvasContainer  *container,
									     NautilusFile             *file);
void              nautilus_canvas_container_scroll_to_canvas                (NautilusCanvasContainer  *container,
									     NautilusFile             *datfile);

void              nautilus_canvas_container_begin_loading                 (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_end_loading                   (NautilusCanvasContainer  *container,
									   gboolean                all_icons_added);

void              nautilus_canvas_container_sort                          (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_freeze_icon_positions         (NautilusCanvasContainer  *container);

int               nautilus_canvas_container_get_max_layout_lines           (NautilusCanvasContainer  *container);
int               nautilus_canvas_container_get_max_layout_lines_for_pango (NautilusCanvasContainer  *container);

void              nautilus_canvas_container_set_highlighted_for_clipboard (NautilusCanvasContainer  *container,
									   GList                  *clipboard_canvas_data);

/* operations on all icons */
void              nautilus_canvas_container_unselect_all                  (NautilusCanvasContainer  *view);
void              nautilus_canvas_container_select_all                    (NautilusCanvasContainer  *view);


void              nautilus_canvas_container_select_first                  (NautilusCanvasContainer  *view);


/* operations on the selection */
GList     *       nautilus_canvas_container_get_selection                 (NautilusCanvasContainer  *view);
void			  nautilus_canvas_container_invert_selection				(NautilusCanvasContainer  *view);
void              nautilus_canvas_container_set_selection                 (NautilusCanvasContainer  *view,
									   GList                  *selection);
GArray    *       nautilus_canvas_container_get_selected_icon_locations   (NautilusCanvasContainer  *view);

/* options */
NautilusCanvasZoomLevel nautilus_canvas_container_get_zoom_level                (NautilusCanvasContainer  *view);
void              nautilus_canvas_container_set_zoom_level                (NautilusCanvasContainer  *view,
									   int                     new_zoom_level);
void              nautilus_canvas_container_set_single_click_mode         (NautilusCanvasContainer  *container,
									   gboolean                single_click_mode);
void              nautilus_canvas_container_enable_linger_selection       (NautilusCanvasContainer  *view,
									   gboolean                enable);
void              nautilus_canvas_container_set_font                      (NautilusCanvasContainer  *container,
									   const char             *font); 
void              nautilus_canvas_container_set_margins                   (NautilusCanvasContainer  *container,
									   int                     left_margin,
									   int                     right_margin,
									   int                     top_margin,
									   int                     bottom_margin);
char*             nautilus_canvas_container_get_icon_description          (NautilusCanvasContainer  *container,
									     NautilusFile           *file);

gboolean	  nautilus_canvas_container_is_layout_rtl			(NautilusCanvasContainer  *container);

gboolean          nautilus_canvas_container_get_store_layout_timestamps   (NautilusCanvasContainer  *container);

void              nautilus_canvas_container_widget_to_file_operation_position (NautilusCanvasContainer *container,
									       GdkPoint              *position);
guint             nautilus_canvas_container_get_icon_size_for_zoom_level (NautilusCanvasZoomLevel zoom_level);

#define CANVAS_WIDTH(container,allocation) (allocation.width		\
					    /  EEL_CANVAS (container)->pixels_per_unit)

#define CANVAS_HEIGHT(container,allocation) (allocation.height		\
					     / EEL_CANVAS (container)->pixels_per_unit)

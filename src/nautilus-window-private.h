#ifndef NAUTILUS_WINDOW_PRIVATE_H
#define NAUTILUS_WINDOW_PRIVATE_H

#include "nautilus-window.h"
#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <gtk/gtk.h>

typedef enum {
  CV_PROGRESS_INITIAL = 1,
  CV_PROGRESS_DONE,
  CV_PROGRESS_ERROR,
  VIEW_ERROR,
  RESET_TO_IDLE, /* Not a real item - a command */
  NAVINFO_RECEIVED,
  NEW_CONTENT_VIEW_ACTIVATED,
  NEW_SIDEBAR_PANEL_ACTIVATED,
  SYNC_STATE /* Not a real item - a flag */
} NautilusWindowStateItem;

/* FIXME: Need to migrate window fields into here. */
struct NautilusWindowDetails
{
	guint refresh_dynamic_bookmarks_idle_id;
	guint refresh_go_menu_idle_id;

	char *last_static_bookmark_path;
};


void                 nautilus_window_set_state_info                    (NautilusWindow             *window,
									... /* things to set, plus optional parameters */);
void                 nautilus_window_set_status                        (NautilusWindow             *window,
									const char                 *status);
void                 nautilus_window_back_or_forward                   (NautilusWindow             *window,
									gboolean                    back,
									guint                       distance);
void                 nautilus_window_load_content_view_menu            (NautilusWindow             *window);
void                 nautilus_window_synch_content_view_menu           (NautilusWindow             *window);
void                 nautilus_window_connect_view                      (NautilusWindow             *window,
									NautilusViewFrame          *view);
void		     nautilus_window_disconnect_view		       (NautilusWindow		   *window,
									NautilusViewFrame	   *view);
void                 nautilus_window_view_failed                       (NautilusWindow             *window,
									NautilusViewFrame          *view);
void                 nautilus_send_history_list_changed                (void);
void                 nautilus_add_to_history_list                      (NautilusBookmark           *bookmark);
GSList *             nautilus_get_history_list                         (void);
void                 nautilus_window_add_bookmark_for_current_location (NautilusWindow             *window);
void                 nautilus_window_initialize_menus                  (NautilusWindow             *window);
void                 nautilus_window_initialize_toolbars               (NautilusWindow             *window);
void                 nautilus_window_go_back                           (NautilusWindow             *window);
void                 nautilus_window_go_forward                        (NautilusWindow             *window);
void                 nautilus_window_go_up                             (NautilusWindow             *window);
void                 nautilus_window_toolbar_remove_theme_callback     (NautilusWindow		   *window);
NautilusUndoManager *nautilus_window_get_undo_manager                  (NautilusWindow             *window);
void                 nautilus_window_begin_location_change             (NautilusWindow             *window,
									const char                 *location,
									NautilusViewFrame          *requesting_view,
									NautilusLocationChangeType  type,
									guint                       distance);
void		     nautilus_window_remove_bookmarks_menu_callback    (NautilusWindow 		   *window);
void		     nautilus_window_remove_go_menu_callback 	       (NautilusWindow 		   *window);
void		     nautilus_window_remove_bookmarks_menu_items       (NautilusWindow 		   *window);
void		     nautilus_window_remove_go_menu_items 	       (NautilusWindow 		   *window);

#endif /* NAUTILUS_WINDOW_PRIVATE_H */

#ifndef NTL_WINDOW_PRIVATE_H
#define NTL_WINDOW_PRIVATE_H 1

#include "ntl-window.h"
#include "ntl-content-view.h"
#include <libnautilus/libnautilus.h>
#include <gtk/gtk.h>

typedef enum {
  CV_PROGRESS_INITIAL = 1,
  CV_PROGRESS_DONE,
  CV_PROGRESS_ERROR,
  VIEW_ERROR,
  RESET_TO_IDLE, /* Not a real item - a command */
  NAVINFO_RECEIVED,
  NEW_CONTENT_VIEW_ACTIVATED,
  NEW_META_VIEW_ACTIVATED,
  SYNC_STATE /* Not a real item - a flag */
} NautilusWindowStateItem;

void nautilus_window_set_state_info(NautilusWindow *window, ... /* things to set, plus optional params */);

void nautilus_window_real_request_selection_change(NautilusWindow *window,
						   Nautilus_SelectionRequestInfo *loc,
						   NautilusView *requesting_view);

void nautilus_window_real_request_status_change(NautilusWindow *window,
						Nautilus_StatusRequestInfo *loc,
						NautilusView *requesting_view);
void nautilus_window_real_request_location_change (NautilusWindow *window,
						   Nautilus_NavigationRequestInfo *loc,
						   NautilusView *requesting_view);
void nautilus_window_real_request_progress_change (NautilusWindow *window,
						   Nautilus_ProgressRequestInfo *loc,
						   NautilusView *requesting_view);
void nautilus_window_set_status(NautilusWindow *window, const char *txt);
void nautilus_window_back_or_forward (NautilusWindow *window, 
				      gboolean back, 
				      guint distance);
void nautilus_window_begin_location_change(NautilusWindow *window,
				           Nautilus_NavigationRequestInfo *loc,
				           NautilusView *requesting_view,
				      	   NautilusLocationChangeType type,
				      	   guint distance);
void nautilus_window_remove_meta_view_real(NautilusWindow *window, NautilusView *meta_view);
void nautilus_window_load_content_view_menu (NautilusWindow *window, NautilusNavigationInfo *ni);
NautilusView *nautilus_window_load_content_view(NautilusWindow *window,
                                  const char *iid,
                                  Nautilus_NavigationInfo *navinfo,
                                  NautilusView **requesting_view);
void nautilus_window_connect_content_view (NautilusWindow *window, 
				           NautilusContentView *view);
void nautilus_window_connect_view (NautilusWindow *window, 
				   NautilusView *view);
void nautilus_window_view_destroyed(NautilusView *view, NautilusWindow *window);

void nautilus_send_history_list_changed (void);
void nautilus_add_to_history_list (NautilusBookmark *bookmark);
GSList *nautilus_get_history_list (void);

void nautilus_window_add_bookmark_for_current_location (NautilusWindow *window);
void nautilus_window_edit_bookmarks (NautilusWindow *window);
void nautilus_window_initialize_menus (NautilusWindow *window);
void nautilus_window_initialize_toolbars (NautilusWindow *window);

void nautilus_window_go_back (NautilusWindow *window);
void nautilus_window_go_forward (NautilusWindow *window);
void nautilus_window_go_up (NautilusWindow *window);
void nautilus_window_go_home (NautilusWindow *window);

#endif

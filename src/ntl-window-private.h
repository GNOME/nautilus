#ifndef NTL_WINDOW_PRIVATE_H
#define NTL_WINDOW_PRIVATE_H 1

#include "ntl-window.h"
#include <libnautilus/libnautilus.h>
#include <gtk/gtk.h>

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
void nautilus_window_change_location(NautilusWindow *window,
				     Nautilus_NavigationRequestInfo *loc,
				     NautilusView *requesting_view,
				     gboolean is_back);
void nautilus_window_remove_meta_view_real(NautilusWindow *window, NautilusView *meta_view);
void nautilus_window_end_location_change(NautilusWindow *window);
void nautilus_window_connect_view (NautilusWindow *window, 
				   NautilusView *view);
void nautilus_window_view_destroyed(NautilusView *view, NautilusWindow *window);

#endif

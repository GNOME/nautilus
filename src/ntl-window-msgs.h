#ifndef NTL_WINDOW_MSGS_H
#define NTL_WINDOW_MSGS_H 1

#include "ntl-window.h"

void nautilus_window_request_location_change(NautilusWindow *window,
					     Nautilus_NavigationRequestInfo *loc,
					     NautilusView *requesting_view);
void nautilus_window_request_selection_change(NautilusWindow *window,
					      Nautilus_SelectionRequestInfo *loc,
					      NautilusView *requesting_view);
void nautilus_window_request_status_change(NautilusWindow *window,
                                           Nautilus_StatusRequestInfo *loc,
                                           NautilusView *requesting_view);
void nautilus_window_request_progress_change(NautilusWindow *window,
					     Nautilus_ProgressRequestInfo *loc,
					     NautilusView *requesting_view);

#endif

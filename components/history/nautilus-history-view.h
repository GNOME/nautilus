#ifndef _NAUTILUS_HISTORY_VIEW_H
#define _NAUTILUS_HISTORY_VIEW_H

#include <gtk/gtktreeview.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus/nautilus-view-standard-main.h>

#define VIEW_IID    "OAFIID:Nautilus_History_View"

#define NAUTILUS_TYPE_HISTORY_VIEW (nautilus_history_view_get_type ())
#define NAUTILUS_HISTORY_VIEW(obj) (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_HISTORY_VIEW, NautilusHistoryView))

typedef struct {
	NautilusView      parent;
	GtkTreeView      *tree_view;
	guint             selection_changed_id;
	gboolean         *stop_updating_history;
} NautilusHistoryView;

GType nautilus_history_view_get_type (void);

#endif

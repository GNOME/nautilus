#ifndef NTL_VIEW_CLIENT_H
#define NTL_VIEW_CLIENT_H

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_VIEW_CLIENT			(nautilus_view_client_get_type ())
#define NAUTILUS_VIEW_CLIENT(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW_CLIENT, NautilusViewClient))
#define NAUTILUS_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_CLIENT, NautilusViewClientClass))
#define NAUTILUS_IS_VIEW_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW_CLIENT))
#define NAUTILUS_IS_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW_CLIENT))

typedef struct _NautilusViewClient       NautilusViewClient;
typedef struct _NautilusViewClientClass  NautilusViewClientClass;

struct _NautilusViewClientClass
{
  GtkBinClass parent_spot;

  void (*notify_location_change)	(NautilusViewClient *view,
					 Nautilus_NavigationInfo *nav_context);
  void (*notify_selection_change)	(NautilusViewClient *view,
					 Nautilus_SelectionInfo *nav_context);
  void (*load_state) (NautilusViewClient *view, const char *config_path);
  void (*save_state) (NautilusViewClient *view, const char *config_path);
  void (*show_properties) (NautilusViewClient *view);

  GtkBinClass *parent_class;
  guint view_client_signals[5];
};

struct _NautilusViewClient
{
  GtkBin parent;

  GtkWidget *main_window;
 
  GnomeObject *control, *view_client;
  Nautilus_ViewFrame view_frame;
};

GtkType nautilus_view_client_get_type                (void);
void    nautilus_view_client_request_location_change (NautilusViewClient         *view,
						      Nautilus_NavigationRequestInfo *loc);
void    nautilus_view_client_request_selection_change (NautilusViewClient        *view,
						       Nautilus_SelectionRequestInfo *loc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

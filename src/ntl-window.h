#ifndef NTL_WINDOW_H
#define NTL_WINDOW_H 1

#include <libgnomeui/gnome-app.h>
#include "ntl-types.h"

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type())
#define NAUTILUS_WINDOW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))

typedef struct _NautilusWindow NautilusWindow;

typedef struct {
  GnomeAppClass parent_spot;

  GnomeAppClass *parent_class;

  void (* request_location_change)(NautilusWindow *window,
				   NautilusLocationReference loc,
				   GtkWidget *requesting_view);
  guint window_signals[1];
} NautilusWindowClass;

struct _NautilusWindow {
  GnomeApp parent_object;

  GtkWidget *content_view;

  GSList *nav_views;
  GtkWidget *nav_notebook, *content_hbox, *btn_back, *btn_fwd;
  NautilusLocationReference current_uri, actual_current_uri;
};

GtkType nautilus_window_get_type(void);
GtkWidget *nautilus_window_new(const char *app_id);
void nautilus_window_request_location_change(NautilusWindow *window,
					     NautilusLocationReference loc,
					     GtkWidget *requesting_view);
void nautilus_window_save_state(NautilusWindow *window, const char *config_path);
void nautilus_window_load_state(NautilusWindow *window, const char *config_path);
void nautilus_window_set_initial_state(NautilusWindow *window);

#endif

#include <config.h>

#include "nautilus.h"
#include "ntl-view-private.h"

typedef struct {
  GnomeObject *control_frame;
} BonoboControlInfo;

static void
destroy_bonobo_control_view(NautilusView *view, CORBA_Environment *ev)
{
  BonoboControlInfo *bci = view->component_data;
  g_free(bci);
}

static void
nautilus_view_activate_uri(GnomeControlFrame *frame, const char *uri, gboolean relative, NautilusView *view)
{
  Nautilus_NavigationRequestInfo nri;
  g_assert(!relative);

  memset(&nri, 0, sizeof(nri));
  nri.requested_uri = (char *)uri;
  nautilus_view_request_location_change(view, &nri);
}

static gboolean
bonobo_control_try_load_client(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev)
{
  BonoboControlInfo *bci;

  view->component_data = bci = g_new0(BonoboControlInfo, 1);

  bci->control_frame = GNOME_OBJECT(gnome_control_frame_new());
  gnome_object_add_interface(GNOME_OBJECT(bci->control_frame), view->view_frame);
  
  gnome_control_frame_set_ui_handler(GNOME_CONTROL_FRAME(bci->control_frame),
				     nautilus_window_get_uih(NAUTILUS_WINDOW(view->main_window)));
  gnome_control_frame_bind_to_control(GNOME_CONTROL_FRAME(bci->control_frame), obj);

  view->client_widget = gnome_control_frame_get_widget(GNOME_CONTROL_FRAME(bci->control_frame));
  
  gtk_signal_connect(GTK_OBJECT(bci->control_frame),
                     "activate_uri", GTK_SIGNAL_FUNC(nautilus_view_activate_uri), view);

  return TRUE;
}

static char *
bonobo_control_get_label(NautilusView *view, CORBA_Environment *ev)
{
  return g_strdup_printf(_("Control %p"), view);
}

NautilusViewComponentType bonobo_control_component_type = {
  "IDL:GNOME/Control:1.0",
  &bonobo_control_try_load_client, /* try_load */
  &destroy_bonobo_control_view, /* destroy */
  NULL, /* show_properties */
  NULL, /* save_state */
  NULL, /* load_state */
  NULL, /* notify_location_change */
  NULL, /* notify_selection_change */
  &bonobo_control_get_label /* get_label */
};

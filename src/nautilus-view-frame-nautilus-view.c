#include <config.h>

#include "nautilus.h"
#include "ntl-view-private.h"

typedef struct {
  GnomeObject *control_frame;
  CORBA_Object view_client;
} NautilusViewInfo;

static gboolean
nautilus_view_try_load_client(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev)
{
  GNOME_Control control;
  NautilusViewInfo *nvi;

  nvi = view->component_data = g_new0(NautilusViewInfo, 1);

  control = GNOME_Unknown_query_interface(obj, "IDL:GNOME/Control:1.0", ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    control = CORBA_OBJECT_NIL;

  if(CORBA_Object_is_nil(control, ev))
    goto out;

  nvi->view_client = CORBA_Object_duplicate(obj, ev);
  GNOME_Unknown_ref(nvi->view_client, ev);

  nvi->control_frame = GNOME_OBJECT(gnome_control_frame_new());
  gnome_object_add_interface(GNOME_OBJECT(nvi->control_frame), view->view_frame);

  gnome_control_frame_bind_to_control(GNOME_CONTROL_FRAME(nvi->control_frame), control);
  view->client_widget = gnome_control_frame_get_widget(GNOME_CONTROL_FRAME(nvi->control_frame));

  GNOME_Unknown_unref(control, ev);
  CORBA_Object_release(control, ev);

  return TRUE;

 out:
  g_free(nvi);

  return FALSE;
}

static void
destroy_nautilus_view(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  CORBA_Object_release(nvi->view_client, ev);

  g_free(nvi);
}

static void
nv_show_properties(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_show_properties(nvi->view_client, ev);
}

static void
nv_save_state(NautilusView *view, const char *config_path, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_save_state(nvi->view_client, config_path, ev);
}

static void
nv_load_state(NautilusView *view, const char *config_path, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_load_state(nvi->view_client, config_path, ev);
}

static void
nv_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *nav_ctx, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_notify_location_change(nvi->view_client, nav_ctx, ev);
}

static void
nv_notify_selection_change(NautilusView *view, Nautilus_SelectionInfo *nav_ctx, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;

  Nautilus_View_notify_selection_change(nvi->view_client, nav_ctx, ev);
}

static char *
nv_get_label(NautilusView *view, CORBA_Environment *ev)
{
  NautilusViewInfo *nvi = view->component_data;
  GnomePropertyBagClient *bc;
  GNOME_Property prop;
  char *retval = NULL;
  CORBA_any *anyval;
  GnomeControlFrame *control_frame;

  control_frame = GNOME_CONTROL_FRAME(nvi->control_frame);
  bc = gnome_control_frame_get_control_property_bag(control_frame);
  g_return_val_if_fail(bc, NULL);

  prop = gnome_property_bag_client_get_property(bc, "label");

  if(CORBA_Object_is_nil(prop, ev))
    return NULL;

  anyval = GNOME_Property_get_value(prop, ev);
  if(ev->_major == CORBA_NO_EXCEPTION && CORBA_TypeCode_equal(anyval->_type, TC_string, ev))
    {
      retval = g_strdup(*(CORBA_char **)anyval->_value);

      CORBA_free(anyval);
    }

  return retval;
}

NautilusViewComponentType nautilus_view_component_type = {
  "IDL:Nautilus/View:1.0",
  &nautilus_view_try_load_client, /* try_load */
  &destroy_nautilus_view, /* destroy */
  &nv_show_properties, /* show_properties */
  &nv_save_state, /* save_state */
  &nv_load_state, /* load_state */
  &nv_notify_location_change, /* notify_location_change */
  &nv_notify_selection_change, /* notify_selection_change */
  &nv_get_label /* get_label */
};

#include <config.h>

#include "nautilus.h"
#include "ntl-view-private.h"

typedef struct {
  GnomeObject *container, *client_site, *view_frame;
} BonoboSubdocInfo;

static void
destroy_bonobo_subdoc_view(NautilusView *view, CORBA_Environment *ev)
{
  BonoboSubdocInfo *bsi = view->component_data;

  g_free(bsi);
}

static void
bonobo_subdoc_notify_location_change(NautilusView *view, Nautilus_NavigationInfo *real_nav_ctx, CORBA_Environment *ev)
{
  GNOME_PersistFile persist;
  persist = gnome_object_client_query_interface(view->client_object, "IDL:GNOME/PersistFile:1.0",
                                                NULL);
  if(!CORBA_Object_is_nil(persist, ev))
    {
      GNOME_PersistFile_load(persist, real_nav_ctx->actual_uri, ev);
      GNOME_Unknown_unref(persist, ev);
      CORBA_Object_release(persist, ev);
    }
  else if((persist = gnome_object_client_query_interface(view->client_object, "IDL:GNOME/PersistStream:1.0",
                                                         NULL))
          && !CORBA_Object_is_nil(persist, ev))
    {
      GnomeStream *stream;
      
      stream = gnome_stream_fs_open(real_nav_ctx->actual_uri, GNOME_Storage_READ);
      GNOME_PersistStream_load (persist,
                                (GNOME_Stream) gnome_object_corba_objref (GNOME_OBJECT (stream)),
                                ev);
      GNOME_Unknown_unref(persist, ev);
      CORBA_Object_release(persist, ev);
    }
}      

static gboolean
bonobo_subdoc_try_load_client(NautilusView *view, CORBA_Object obj, CORBA_Environment *ev)
{
  BonoboSubdocInfo *bsi;

  view->component_data = bsi = g_new0(BonoboSubdocInfo, 1);

  bsi->container = GNOME_OBJECT(gnome_container_new());
  gnome_object_add_interface(GNOME_OBJECT(bsi->container), view->view_frame);
      
  bsi->client_site =
    GNOME_OBJECT(gnome_client_site_new(GNOME_CONTAINER(bsi->container)));
  gnome_client_site_bind_embeddable(GNOME_CLIENT_SITE(bsi->client_site), view->client_object);
  gnome_container_add(GNOME_CONTAINER(bsi->container), bsi->client_site);

  bsi->view_frame = GNOME_OBJECT(gnome_client_site_new_view(GNOME_CLIENT_SITE(bsi->client_site)));

  g_assert(bsi->view_frame);
      
  view->client_widget = gnome_view_frame_get_wrapper(GNOME_VIEW_FRAME(bsi->view_frame));
      
  return TRUE;
}

NautilusViewComponentType bonobo_subdoc_component_type = {
  "IDL:GNOME/Embeddable:1.0",
  &bonobo_subdoc_try_load_client, /* try_load */
  &destroy_bonobo_subdoc_view, /* destroy */
  NULL, /* show_properties */
  NULL, /* save_state */
  NULL, /* load_state */
  bonobo_subdoc_notify_location_change, /* notify_location_change */
  NULL, /* notify_selection_change */
  NULL, /* get_label */
};


#include <config.h>

#include <bonobo.h>
#include <gnome.h>
#include <liboaf/liboaf.h>

#include "hyperbola-nav.h"

static int object_count = 0;

static void
do_destroy(GtkObject *obj)
{
  object_count--;

  if(object_count <= 0)
    gtk_main_quit();
}

static BonoboObject *
make_obj(BonoboGenericFactory *Factory, const char *goad_id, void *closure)
{
  BonoboObject *retval = NULL;

  if(!strcmp(goad_id, "OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234"))
    retval = hyperbola_navigation_tree_new();
  else if(!strcmp(goad_id, "OAFIID:hyperbola_navigation_index:0bafadc7-09f1-4f10-8c8e-dad53124fc49"))
    retval = hyperbola_navigation_index_new();
  else if(!strcmp(goad_id, "OAFIID:hyperbola_navigation_search:89b2f3b8-4f09-49c8-9a7b-ccb14d034813"))
    retval = hyperbola_navigation_search_new();

  if(retval)
    {
      object_count++;
      gtk_signal_connect(GTK_OBJECT(retval), "destroy", do_destroy, NULL);
    }

  return retval;
}

int
main(int argc, char *argv[])
{
  BonoboGenericFactory *factory;
  CORBA_ORB orb;
  char *registration_id;


  /* Initialize gettext support */
#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
  textdomain (PACKAGE);
#endif	
	
  /* Disable session manager connection */
  gnome_client_disable_master_connection ();

  gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
  orb = oaf_init (argc, argv);

  gnome_init ("hyperbola", VERSION, 
	      argc, argv); 
  
  gdk_rgb_init ();

  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  registration_id = oaf_make_registration_id ("OAFIID:hyperbola_factory:02b54c63-101b-4b27-a285-f99ed332ecdb", g_getenv ("DISPLAY"));
  factory = bonobo_generic_factory_new_multi (registration_id, make_obj, NULL);
  g_free (registration_id);

  do {
    bonobo_main ();
  } while (object_count > 0);

  return 0;
}

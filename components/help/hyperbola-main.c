#include "config.h"

#include <libnautilus/libnautilus.h>
#include <libgnorba/gnorba.h>

/* In hyperbola-nav-tree.c */
extern BonoboObject *hyperbola_navigation_tree_new(void);
/* in hyperbola-nav-index.c */
extern BonoboObject *hyperbola_navigation_index_new(void);

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

  if(!strcmp(goad_id, "hyperbola_navigation_tree"))
    retval = hyperbola_navigation_tree_new();
  else if(!strcmp(goad_id, "hyperbola_index_view"))
    retval = hyperbola_navigation_index_new();

  if(retval)
    {
      object_count++;
      gtk_signal_connect(GTK_OBJECT(retval), "destroy", do_destroy, NULL);
    }

  return retval;
}

int main(int argc, char *argv[])
{
  BonoboGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table("hyperbola", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  factory = bonobo_generic_factory_new_multi("hyperbola_factory", make_obj, NULL);

  do {
    bonobo_main();
  } while(object_count > 0);

  return 0;
}

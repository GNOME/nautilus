#include "config.h"

#include <libnautilus/libnautilus.h>

/* In hyperbola-nav-tree.c */
extern GnomeObject *hyperbola_navigation_tree_new(void);

static GnomeObject *
make_obj(GnomeGenericFactory *Factory, const char *goad_id, void *closure)
{
  if(!strcmp(goad_id, "hyperbola_navigation_tree"))
    return hyperbola_navigation_tree_new();
}

int main(int argc, char *argv[])
{
  GnomeGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table("hyperbola", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);

  factory = gnome_generic_factory_new_multi("hyperbola_factory", make_obj, NULL);

  bonobo_main();

  return 0;
}

#include <gnome.h>

enum {
	FULL_INST,
	NAUTILUS_ONLY,
	SERVICES_ONLY,
	UPGRADE,
	UNINSTALL,
	LAST
};

void installer (GtkWidget *window,
		gint method); 


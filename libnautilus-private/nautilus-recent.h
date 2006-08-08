

#ifndef __NAUTILUS_RECENT_H__
#define __NAUTILUS_RECENT_H__

#include <gtk/gtkrecentmanager.h>
#include <libnautilus-private/nautilus-file.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

void nautilus_recent_add_file (NautilusFile *file,
			       GnomeVFSMimeApplication *application);

#endif

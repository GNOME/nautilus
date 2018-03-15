#pragma once

#include <gtk/gtk.h>
#include "nautilus-file.h"
#include <gio/gio.h>

void nautilus_recent_add_file (NautilusFile *file,
			       GAppInfo *application);
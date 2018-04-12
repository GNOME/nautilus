#pragma once

#include <gtk/gtk.h>
#include "nautilus-file.h"
#include <gio/gio.h>

void nautilus_recent_add_file          (NautilusFile *file,
                                        GAppInfo *application);
void nautilus_recent_update_file_moved (const char *old_uri,
                                        const char *new_uri,
                                        const char *old_display_name,
                                        const char *new_dispaly_name);

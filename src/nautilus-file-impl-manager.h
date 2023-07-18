#include <gio/gio.h>

#pragma once

#define NAUTILUS_TYPE_FILE_IMPL_MANAGER nautilus_file_impl_manager_get_type()

G_DECLARE_FINAL_TYPE (NautilusFileImplManager, nautilus_file_impl_manager, NAUTILUS, FILE_IMPL_MANAGER, GObject)

NautilusFileImplManager * nautilus_file_impl_manager_new (void);

gboolean nautilus_file_impl_manager_register   (NautilusFileImplManager *self,
                                                GDBusConnection         *connection,
                                                GError                 **error);
void     nautilus_file_impl_manager_unregister (NautilusFileImplManager *self);

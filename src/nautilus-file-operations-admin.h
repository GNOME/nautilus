#pragma once

#include <gio/gio.h>

#include "nautilus-file-operations-private.h"

gboolean retry_with_admin_uri (GFile     **try_file,
                               CommonJob  *job,
                               GError    **error);

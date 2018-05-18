#pragma once

#include <gio/gio.h>

#include "nautilus-types.h"

void nautilus_recent_add_file (NautilusFile *file,
                               GAppInfo     *application);

/*
 * Copyright (C) 2024 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#pragma once

#include "nautilus-directory.h"

G_BEGIN_DECLS

#define NAUTILUS_NETWORK_DIRECTORY_PROVIDER_NAME "network-directory-provider"

#define NAUTILUS_TYPE_NETWORK_DIRECTORY (nautilus_network_directory_get_type ())

G_DECLARE_FINAL_TYPE (NautilusNetworkDirectory, nautilus_network_directory, NAUTILUS, NETWORK_DIRECTORY, NautilusDirectory);

NautilusNetworkDirectory* nautilus_network_directory_new      (void);

G_END_DECLS

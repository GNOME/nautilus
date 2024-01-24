/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-list-base.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NETWORK_VIEW (nautilus_network_view_get_type())

G_DECLARE_FINAL_TYPE (NautilusNetworkView, nautilus_network_view, NAUTILUS, NETWORK_VIEW, NautilusListBase)

NautilusNetworkView *nautilus_network_view_new (void);

G_END_DECLS

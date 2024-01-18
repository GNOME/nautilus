/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NETWORK_ADDRESS_BAR (nautilus_network_address_bar_get_type())

G_DECLARE_FINAL_TYPE (NautilusNetworkAddressBar, nautilus_network_address_bar, NAUTILUS, NETWORK_ADDRESS_BAR, GtkBox)

NautilusNetworkAddressBar * nautilus_network_address_bar_new (void);

G_END_DECLS

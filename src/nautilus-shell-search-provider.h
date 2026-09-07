/*
 * nautilus-shell-search-provider.h - Implementation of a GNOME Shell search provider
 *
 * SPDX-FileCopyrightText: 2012 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#define NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER nautilus_shell_search_provider_get_type()

G_DECLARE_FINAL_TYPE (NautilusShellSearchProvider, nautilus_shell_search_provider, NAUTILUS, SHELL_SEARCH_PROVIDER, GObject)

GType nautilus_shell_search_provider_get_type (void);
NautilusShellSearchProvider * nautilus_shell_search_provider_new (void);

gboolean nautilus_shell_search_provider_register   (NautilusShellSearchProvider *self,
                                                    GDBusConnection             *connection,
                                                    GError                     **error);
void     nautilus_shell_search_provider_unregister (NautilusShellSearchProvider *self);

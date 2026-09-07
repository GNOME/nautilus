/*
 * nautilus-module.h - Interface to nautilus extensions
 *
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

void   nautilus_module_setup                   (void);
void   nautilus_module_teardown                (void);
GList *nautilus_module_get_extensions_for_type (GType  type);
void   nautilus_module_extension_list_free     (GList *list);
gchar *nautilus_module_get_installed_module_names (void);


/* Add a type to the module interface - allows nautilus to add its own modules
 * without putting them in separate shared libraries */
void   nautilus_module_add_type                (GType  type);

G_END_DECLS

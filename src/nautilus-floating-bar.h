
/* Nautilus - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <gtk/gtk.h>

#define NAUTILUS_FLOATING_BAR_ACTION_ID_STOP 1

#define NAUTILUS_TYPE_FLOATING_BAR nautilus_floating_bar_get_type()
G_DECLARE_FINAL_TYPE (NautilusFloatingBar, nautilus_floating_bar, NAUTILUS, FLOATING_BAR, GtkBox)

GtkWidget * nautilus_floating_bar_new              (const gchar *primary_label,
						    const gchar *details_label,
						    gboolean show_spinner);

void       nautilus_floating_bar_set_primary_label (NautilusFloatingBar *self,
						    const gchar *label);
void       nautilus_floating_bar_set_details_label (NautilusFloatingBar *self,
						    const gchar *label);
void        nautilus_floating_bar_set_labels        (NautilusFloatingBar *self,
						     const gchar *primary,
						     const gchar *detail);
void        nautilus_floating_bar_set_show_spinner (NautilusFloatingBar *self,
						    gboolean show_spinner);
void        nautilus_floating_bar_set_show_stop (NautilusFloatingBar *self,
						    gboolean show_spinner);

void        nautilus_floating_bar_remove_hover_timeout (NautilusFloatingBar *self);


/*
 * SPDX-FileCopyrightText: 2011 Red Hat Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
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
void        nautilus_floating_bar_set_show_stop (NautilusFloatingBar *self,
						    gboolean show_spinner);

void        nautilus_floating_bar_remove_hover_timeout (NautilusFloatingBar *self);

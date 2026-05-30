/*
 * Copyright (C) 2004 Novell, Inc.
 * Copyright (C) 2007, 2010 Red Hat, Inc.
 * Copyright (C) 2026 GNOME Files Contributors.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 *          Khalid Abu Shawarib <kas@gnome.org>
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_APP_CHOOSER_WIDGET (nautilus_app_chooser_widget_get_type())

G_DECLARE_FINAL_TYPE (NautilusAppChooserWidget, nautilus_app_chooser_widget, NAUTILUS, APP_CHOOSER_WIDGET, GtkWidget)

NautilusAppChooserWidget *nautilus_app_chooser_widget_new      (const char          *content_type);

void          nautilus_app_chooser_widget_set_search_entry     (NautilusAppChooserWidget *self,
                                                                GtkEditable         *editable);

GAppInfo *    nautilus_app_chooser_widget_get_app_info (NautilusAppChooserWidget *self);
void          nautilus_app_chooser_widget_refresh (NautilusAppChooserWidget *self);

G_END_DECLS


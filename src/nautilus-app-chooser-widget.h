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

#define NAUTILUS_TYPE_APP_CHOOSER_WIDGET            (nautilus_app_chooser_widget_get_type ())
#define NAUTILUS_APP_CHOOSER_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_APP_CHOOSER_WIDGET, NautilusAppChooserWidget))
#define NAUTILUS_IS_APP_CHOOSER_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_APP_CHOOSER_WIDGET))

typedef struct _NautilusAppChooserWidget        NautilusAppChooserWidget;

GType         nautilus_app_chooser_widget_get_type             (void) G_GNUC_CONST;

NautilusAppChooserWidget *nautilus_app_chooser_widget_new      (const char          *content_type);

void          nautilus_app_chooser_widget_set_default_text     (NautilusAppChooserWidget *self,
                                                                const char          *text);
const char *  nautilus_app_chooser_widget_get_default_text     (NautilusAppChooserWidget *self);

void          nautilus_app_chooser_widget_set_search_entry     (NautilusAppChooserWidget *self,
                                                                GtkEditable         *editable);

GAppInfo *    nautilus_app_chooser_widget_get_app_info (NautilusAppChooserWidget *self);
void          nautilus_app_chooser_widget_refresh (NautilusAppChooserWidget *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(NautilusAppChooserWidget, g_object_unref)

G_END_DECLS


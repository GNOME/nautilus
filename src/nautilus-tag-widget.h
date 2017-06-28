/* nautilus-tag-widget.h
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_TAG_WIDGET_H
#define NAUTILUS_TAG_WIDGET_H

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "nautilus-files-view.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TAG_WIDGET (nautilus_tag_widget_get_type())

G_DECLARE_FINAL_TYPE (NautilusTagWidget, nautilus_tag_widget, NAUTILUS, TAG_WIDGET, GtkEventBox);

GtkWidget*    nautilus_tag_widget_new    (const gchar   *tag_label,
                                          const gchar   *tag_id,
                                          gboolean       can_close);

const gchar*  nautilus_tag_widget_get_tag_name (NautilusTagWidget *self);

const gchar*  nautilus_tag_widget_get_tag_id (NautilusTagWidget *self);

GtkWidget* nautilus_tag_widget_queue_get_tag_with_name (GQueue      *queue,
                                                        const gchar *tag_name);

gboolean      nautilus_tag_widget_queue_contains_tag (GQueue      *queue,
                                                      const gchar *tag_name);


G_END_DECLS

#endif

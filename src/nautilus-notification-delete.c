/* nautilus-notification-delete.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
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

#include <glib/gi18n.h>

#include "nautilus-window.h"
#include "nautilus-notification-delete.h"
#include "nautilus-notification-manager.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-undo-operations.h"

struct _NautilusNotificationDeletePrivate
{
  guint timeout_id;
  NautilusWindow *window;
};

G_DEFINE_TYPE_WITH_PRIVATE (NautilusNotificationDelete, nautilus_notification_delete, GTK_TYPE_GRID)

enum {
  PROP_0,
  PROP_WINDOW,
  LAST_PROP
};

#define  NOTIFICATION_TIMEOUT 6

static void
nautilus_notification_delete_remove_timeout (NautilusNotificationDelete *self)
{
  if (self->priv->timeout_id != 0)
    {
      g_source_remove (self->priv->timeout_id);
      self->priv->timeout_id = 0;
    }
}

static void
nautilus_notification_delete_destroy (NautilusNotificationDelete *self)
{
  nautilus_notification_delete_remove_timeout (self);
  gtk_widget_destroy (GTK_WIDGET (self));
}

static gboolean
nautilus_notification_delete_on_timeout (gpointer user_data)
{
  NautilusNotificationDelete *self = NAUTILUS_NOTIFICATION_DELETE (user_data);

  self->priv->timeout_id = 0;
  gtk_widget_destroy (GTK_WIDGET (self));

  return G_SOURCE_REMOVE;
}

static void
nautilus_notification_delete_undo_clicked (NautilusNotificationDelete *self)
{
  nautilus_notification_delete_remove_timeout (self);
  /* The notification manager will destroy all notifications after the undo
   * state changes. So no need to destroy the notification it now */
  nautilus_file_undo_manager_undo (GTK_WINDOW (self->priv->window));
}

static void
nautilus_notification_delete_finalize (GObject *object)
{
  NautilusNotificationDelete *self = NAUTILUS_NOTIFICATION_DELETE (object);

  nautilus_notification_delete_remove_timeout (self);

  G_OBJECT_CLASS (nautilus_notification_delete_parent_class)->finalize (object);
}

NautilusNotificationDelete *
nautilus_notification_delete_new (NautilusWindow *window)
{
  g_assert (NAUTILUS_IS_WINDOW (window));

  return g_object_new (NAUTILUS_TYPE_NOTIFICATION_DELETE,
                       "window", window,
                       "margin-start", 12,
                       "margin-end", 4,
                       NULL);
}

static void
nautilus_notification_delete_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  NautilusNotificationDelete *self = NAUTILUS_NOTIFICATION_DELETE (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      self->priv->window = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_notification_delete_class_init (NautilusNotificationDeleteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = nautilus_notification_delete_set_property;
  object_class->finalize = nautilus_notification_delete_finalize;

  g_object_class_install_property (object_class,
                                   PROP_WINDOW,
                                   g_param_spec_object ("window",
                                                        "Window associated",
                                                        "The window that contains the notification",
                                                        NAUTILUS_TYPE_WINDOW,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

static void
nautilus_notification_delete_init (NautilusNotificationDelete *self)
{
  GtkWidget *close;
  gchar *label;
  gchar *file_label;
  gchar *markup = NULL;
  GtkWidget *label_widget;
  GtkWidget *undo_widget;
  NautilusFileUndoInfo *undo_info;
  GList *files;
  gint length;

  self->priv = nautilus_notification_delete_get_instance_private (self);

  undo_info = nautilus_file_undo_manager_get_action ();
  g_assert (NAUTILUS_IS_FILE_UNDO_INFO_TRASH (undo_info));
  files = nautilus_file_undo_info_trash_get_files (NAUTILUS_FILE_UNDO_INFO_TRASH (undo_info));

  length = g_list_length (files);
  if (length == 1)
    {
      file_label = g_file_get_basename (files->data);
      label = g_strdup_printf (_("%s deleted"), file_label);
      g_free (file_label);
    }
  else
    {
      label = g_strdup_printf (ngettext ("%d file deleted", "%d files deleted", length), length);
    }

  label_widget = gtk_label_new (label);
  gtk_label_set_max_width_chars (GTK_LABEL (label_widget), 50);
  gtk_label_set_ellipsize (GTK_LABEL (label_widget), PANGO_ELLIPSIZE_MIDDLE);
  markup = g_markup_printf_escaped ("<span font_weight=\"bold\">%s</span>", label);
  gtk_label_set_markup (GTK_LABEL (label_widget), markup);
  gtk_widget_set_halign (label_widget, GTK_ALIGN_START);
  gtk_widget_set_margin_end (label_widget, 30);
  gtk_container_add (GTK_CONTAINER (self), label_widget);

  undo_widget = gtk_button_new_with_label (_("Undo"));
  gtk_widget_set_valign (undo_widget, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (self), undo_widget);
  gtk_widget_set_margin_end (undo_widget, 6);
  g_signal_connect_swapped (undo_widget, "clicked",
                            G_CALLBACK (nautilus_notification_delete_undo_clicked),
                            self);

  close = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_valign (close, GTK_ALIGN_CENTER);
  gtk_button_set_focus_on_click (GTK_BUTTON (close), FALSE);
  gtk_button_set_relief (GTK_BUTTON (close), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER (self), close);
  g_signal_connect_swapped (close, "clicked",
                            G_CALLBACK (nautilus_notification_delete_destroy),
                            self);

  self->priv->timeout_id = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                                       NOTIFICATION_TIMEOUT,
                                                       nautilus_notification_delete_on_timeout,
                                                       g_object_ref (self),
                                                       g_object_unref);

  g_free (label);
  g_free (markup);
  g_list_free (files);
}

/* nautilus-notification-manager.c
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

#include "nautilus-notification-manager.h"

struct _NautilusNotificationManagerPrivate
{
  GtkWidget *grid;
};

G_DEFINE_TYPE_WITH_PRIVATE (NautilusNotificationManager, nautilus_notification_manager, GD_TYPE_NOTIFICATION)

NautilusNotificationManager *
nautilus_notification_manager_new (void)
{
  return g_object_new (NAUTILUS_TYPE_NOTIFICATION_MANAGER,
                       "show-close-button", FALSE,
                       "timeout", -1,
                       NULL);
}

void
nautilus_notification_manager_add_notification (NautilusNotificationManager *self,
                                                GtkWidget                   *notification)
{
  gtk_container_add (GTK_CONTAINER (self->priv->grid), notification);
  gtk_widget_show_all (GTK_WIDGET (self));
}

void
nautilus_notification_manager_remove_all (NautilusNotificationManager *self)
{
  gtk_widget_hide (GTK_WIDGET (self));
  gtk_container_foreach (GTK_CONTAINER (self->priv->grid),
                         (GtkCallback) gtk_widget_destroy,
                         NULL);
}

static void
nautilus_notification_manager_class_init (NautilusNotificationManagerClass *Klass)
{

}

static void
nautilus_notification_manager_init (NautilusNotificationManager *self)
{
  self->priv = nautilus_notification_manager_get_instance_private (self);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);

  self->priv->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->priv->grid),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self->priv->grid), 6);
  gtk_container_add (GTK_CONTAINER (self), self->priv->grid);

  g_signal_connect_swapped (self->priv->grid, "remove",
                            G_CALLBACK (nautilus_notification_manager_remove_all),
                            self);
}

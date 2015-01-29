/* nautilus-notification-manager.h
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

#ifndef NAUTILUS_NOTIFICATION_MANAGER_H
#define NAUTILUS_NOTIFICATION_MANAGER_H

#include <libgd/gd.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NOTIFICATION_MANAGER            (nautilus_notification_manager_get_type())
#define NAUTILUS_NOTIFICATION_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NOTIFICATION_MANAGER, NautilusNotificationManager))
#define NAUTILUS_NOTIFICATION_MANAGER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NOTIFICATION_MANAGER, NautilusNotificationManager const))
#define NAUTILUS_NOTIFICATION_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NAUTILUS_TYPE_NOTIFICATION_MANAGER, NautilusNotificationManagerClass))
#define NAUTILUS_IS_NOTIFICATION_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_NOTIFICATION_MANAGER))
#define NAUTILUS_IS_NOTIFICATION_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NAUTILUS_TYPE_NOTIFICATION_MANAGER))
#define NAUTILUS_NOTIFICATION_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NAUTILUS_TYPE_NOTIFICATION_MANAGER, NautilusNotificationManagerClass))

typedef struct _NautilusNotificationManager        NautilusNotificationManager;
typedef struct _NautilusNotificationManagerClass   NautilusNotificationManagerClass;
typedef struct _NautilusNotificationManagerPrivate NautilusNotificationManagerPrivate;

struct _NautilusNotificationManager
{
  GdNotification parent;

  /*< private >*/
  NautilusNotificationManagerPrivate *priv;
};

struct _NautilusNotificationManagerClass
{
  GdNotificationClass parent;
};

GType                            nautilus_notification_manager_get_type         (void);
NautilusNotificationManager     *nautilus_notification_manager_new              (void);
void                             nautilus_notification_manager_add_notification (NautilusNotificationManager *self,
                                                                                 GtkWidget                   *notification);
void                             nautilus_notification_manager_remove_all       (NautilusNotificationManager *self);

G_END_DECLS

#endif /* NAUTILUS_NOTIFICATION_MANAGER_H */

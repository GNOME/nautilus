/* nautilus-notification-delete.h
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

#ifndef NAUTILUS_NOTIFICATION_DELETE_H
#define NAUTILUS_NOTIFICATION_DELETE_H

#include <gtk/gtk.h>
#include "nautilus-window.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_NOTIFICATION_DELETE            (nautilus_notification_delete_get_type())
#define NAUTILUS_NOTIFICATION_DELETE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NOTIFICATION_DELETE, NautilusNotificationDelete))
#define NAUTILUS_NOTIFICATION_DELETE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NOTIFICATION_DELETE, NautilusNotificationDelete const))
#define NAUTILUS_NOTIFICATION_DELETE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NAUTILUS_TYPE_NOTIFICATION_DELETE, NautilusNotificationDeleteClass))
#define NAUTILUS_IS_NOTIFICATION_DELETE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_NOTIFICATION_DELETE))
#define NAUTILUS_IS_NOTIFICATION_DELETE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NAUTILUS_TYPE_NOTIFICATION_DELETE))
#define NAUTILUS_NOTIFICATION_DELETE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NAUTILUS_TYPE_NOTIFICATION_DELETE, NautilusNotificationDeleteClass))

typedef struct _NautilusNotificationDelete        NautilusNotificationDelete;
typedef struct _NautilusNotificationDeleteClass   NautilusNotificationDeleteClass;
typedef struct _NautilusNotificationDeletePrivate NautilusNotificationDeletePrivate;

struct _NautilusNotificationDelete
{
  GtkGrid parent;

  /*< private >*/
  NautilusNotificationDeletePrivate *priv;
};

struct _NautilusNotificationDeleteClass
{
  GtkGridClass parent;
};

GType                           nautilus_notification_delete_get_type (void);
NautilusNotificationDelete     *nautilus_notification_delete_new      (NautilusWindow *window);

G_END_DECLS

#endif /* NAUTILUS_NOTIFICATION_DELETE_H */

/* GNOME GUI Library
 * Copyright (C) 1995-1998 Jay Painter
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */
#ifndef __GNOME_MESSAGE_BOX_H__
#define __GNOME_MESSAGE_BOX_H__

#include "gnome-dialog.h"

BEGIN_GNOME_DECLS

#define GNOME_TYPE_MESSAGE_BOX            (gnome_message_box_get_type ())
#define GNOME_MESSAGE_BOX(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_MESSAGE_BOX, GnomeMessageBox))
#define GNOME_MESSAGE_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_MESSAGE_BOX, GnomeMessageBoxClass))
#define GNOME_IS_MESSAGE_BOX(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_MESSAGE_BOX))
#define GNOME_IS_MESSAGE_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_MESSAGE_BOX))


#define GNOME_MESSAGE_BOX_INFO      "info"
#define GNOME_MESSAGE_BOX_WARNING   "warning"
#define GNOME_MESSAGE_BOX_ERROR     "error"
#define GNOME_MESSAGE_BOX_QUESTION  "question"
#define GNOME_MESSAGE_BOX_GENERIC   "generic"


typedef struct _GnomeMessageBox        GnomeMessageBox;
typedef struct _GnomeMessageBoxClass   GnomeMessageBoxClass;
typedef struct _GnomeMessageBoxButton  GnomeMessageBoxButton;

struct _GnomeMessageBox
{
  GnomeDialog dialog;
};

struct _GnomeMessageBoxClass
{
  GnomeDialogClass parent_class;
};


guint      gnome_message_box_get_type    (void);
GtkWidget* gnome_message_box_new         (const gchar           *message,
					  const gchar           *messagebox_type,
					  ...);

GtkWidget* gnome_message_box_newv        (const gchar           *message,
					  const gchar           *messagebox_type,
					  const gchar 	      **buttons);
#ifndef GNOME_EXCLUDE_DEPRECATED
/* Deprecated in favor of gtk_window's version. Don't use. */
void       gnome_message_box_set_modal   (GnomeMessageBox *messagebox);

/* Deprecated in favor of gnome_dialog_ variant. Don't use. */
void       gnome_message_box_set_default (GnomeMessageBox *messagebox,
					  gint            button);
#endif

END_GNOME_DECLS

#endif /* __GNOME_MESSAGE_BOX_H__ */

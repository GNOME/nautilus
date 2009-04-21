/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-image-table.h - An image table.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_IMAGE_TABLE_H
#define EEL_IMAGE_TABLE_H

#include <eel/eel-wrap-table.h>

G_BEGIN_DECLS

#define EEL_TYPE_IMAGE_TABLE eel_image_table_get_type()
#define EEL_IMAGE_TABLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_IMAGE_TABLE, EelImageTable))
#define EEL_IMAGE_TABLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_IMAGE_TABLE, EelImageTableClass))
#define EEL_IS_IMAGE_TABLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_IMAGE_TABLE))
#define EEL_IS_IMAGE_TABLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_IMAGE_TABLE))
#define EEL_IMAGE_TABLE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_IMAGE_TABLE, EelImageTableClass))

typedef struct EelImageTable		EelImageTable;
typedef struct EelImageTableClass	EelImageTableClass;
typedef struct EelImageTableDetails	EelImageTableDetails;

typedef struct
{
	int x;
	int y;
	int button;
	guint state;
	GdkEvent *event;
} EelImageTableEvent;

struct EelImageTable
{
	/* Superclass */
	EelWrapTable wrap_table;

	/* Private things */
	EelImageTableDetails *details;
};

struct EelImageTableClass
{
	EelWrapTableClass parent_class;

	/* Signals */
	void (* child_enter) (EelImageTable *image_table,
			      GtkWidget *child,
			      const EelImageTableEvent *event);
	void (* child_leave) (EelImageTable *image_table,
			      GtkWidget *child,
			      const EelImageTableEvent *event);
	void (* child_pressed) (EelImageTable *image_table,
				GtkWidget *child,
				const EelImageTableEvent *event);
	void (* child_released) (EelImageTable *image_table,
				 GtkWidget *child,
				 const EelImageTableEvent *event);
	void (* child_clicked) (EelImageTable *image_table,
				GtkWidget *child,
				const EelImageTableEvent *event);
};

/* Public GtkImageTable methods */
GType      eel_image_table_get_type         (void);
GtkWidget *eel_image_table_new              (gboolean       homogeneous);
GtkWidget *eel_image_table_add_empty_image  (EelImageTable *image_table);

G_END_DECLS

#endif /* EEL_IMAGE_TABLE_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_LOCATION_DIALOG_H
#define NAUTILUS_LOCATION_DIALOG_H

#include <gtk/gtkdialog.h>
#include "nautilus-window.h"

#define NAUTILUS_TYPE_LOCATION_DIALOG         (nautilus_location_dialog_get_type ())
#define NAUTILUS_LOCATION_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_LOCATION_DIALOG, NautilusLocationDialog))
#define NAUTILUS_LOCATION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LOCATION_DIALOG, NautilusLocationDialogClass))
#define NAUTILUS_IS_LOCATION_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), NAUTILUS_TYPE_LOCATION_DIALOG)

typedef struct _NautilusLocationDialog        NautilusLocationDialog;
typedef struct _NautilusLocationDialogClass   NautilusLocationDialogClass;
typedef struct _NautilusLocationDialogDetails NautilusLocationDialogDetails;

struct _NautilusLocationDialog {
	GtkDialog parent;
	NautilusLocationDialogDetails *details;
};

struct _NautilusLocationDialogClass {
	GtkDialogClass parent_class;
};

GType      nautilus_location_dialog_get_type     (void);
GtkWidget* nautilus_location_dialog_new          (NautilusWindow         *window);
void       nautilus_location_dialog_set_location (NautilusLocationDialog *dialog,
						  const char             *location);

#endif /* NAUTILUS_LOCATION_DIALOG_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-application.h
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_APPLICATION_H
#define NAUTILUS_APPLICATION_H

#include <bonobo/bonobo-object.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define NAUTILUS_DESKTOP_ICON_VIEW_IID	"OAFIID:nautilus_file_manager_desktop_icon_view:8d8121b1-0f1e-400b-bf0d-5b0f4555f5e1"

#define NAUTILUS_TYPE_APPLICATION	     (nautilus_application_get_type ())
#define NAUTILUS_APPLICATION(obj)	     (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_APPLICATION, NautilusApplication))
#define NAUTILUS_APPLICATION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_APPLICATION, NautilusApplicationClass))
#define NAUTILUS_IS_APPLICATION(obj)	     (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_APPLICATION))
#define NAUTILUS_IS_APPLICATION_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_APPLICATION))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
typedef struct NautilusWindow NautilusWindow;
#endif

typedef struct {
	BonoboObject parent;
	GSList *windows;
	NautilusUndoManager *undo_manager;
} NautilusApplication;

typedef struct {
	BonoboObjectClass parent_class;
	gpointer servant;
	gpointer unknown_epv;
} NautilusApplicationClass;

GtkType              nautilus_application_get_type      (void);
NautilusApplication *nautilus_application_new           (void);
gboolean             nautilus_application_startup       (NautilusApplication *application,
							 gboolean             manage_desktop,
							 const char          *urls[]);
NautilusWindow *     nautilus_application_create_window (NautilusApplication *application);
void                 nautilus_application_quit          (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_APPLICATION_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* nautilus-application.h
 */

#ifndef NAUTILUS_APPLICATION_H
#define NAUTILUS_APPLICATION_H

#include <bonobo/bonobo-object.h>
#include <libnautilus-private/nautilus-undo-manager.h>

#define NAUTILUS_DESKTOP_ICON_VIEW_IID	"OAFIID:nautilus_file_manager_desktop_icon_view:8d8121b1-0f1e-400b-bf0d-5b0f4555f5e1"

#define NAUTILUS_TYPE_APPLICATION	     (nautilus_application_get_type ())
#define NAUTILUS_APPLICATION(obj)	     (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_APPLICATION, NautilusApplication))
#define NAUTILUS_APPLICATION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_APPLICATION, NautilusApplicationClass))
#define NAUTILUS_IS_APPLICATION(obj)	     (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_APPLICATION))
#define NAUTILUS_IS_APPLICATION_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_APPLICATION))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
typedef struct NautilusWindow NautilusWindow;
#endif

typedef struct {
	BonoboObject parent;
	NautilusUndoManager *undo_manager;
} NautilusApplication;

typedef struct {
	BonoboObjectClass parent_class;
} NautilusApplicationClass;

GtkType              nautilus_application_get_type          (void);
NautilusApplication *nautilus_application_new               (void);
void                 nautilus_application_startup           (NautilusApplication *application,
							     gboolean             kill_shell,
							     gboolean             restart_shell,
							     gboolean             no_default_window,
							     gboolean             no_desktop,
							     gboolean             do_first_time_druid_check,
							     const char          *default_geometry,
							     const char          *urls[]);
GList *              nautilus_application_get_window_list   (void);
NautilusWindow *     nautilus_application_create_window     (NautilusApplication *application);
void                 nautilus_application_close_all_windows (void);
void                 nautilus_application_open_desktop      (NautilusApplication *application);
void                 nautilus_application_close_desktop     (void);

#endif /* NAUTILUS_APPLICATION_H */

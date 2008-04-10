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

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <bonobo/bonobo-generic-factory.h>
#include <libnautilus-private/nautilus-undo-manager.h>

#define NAUTILUS_DESKTOP_ICON_VIEW_IID	"OAFIID:Nautilus_File_Manager_Desktop_Icon_View"

#define NAUTILUS_TYPE_APPLICATION	     (nautilus_application_get_type ())
#define NAUTILUS_APPLICATION(obj)	     (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_APPLICATION, NautilusApplication))
#define NAUTILUS_APPLICATION_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_APPLICATION, NautilusApplicationClass))
#define NAUTILUS_IS_APPLICATION(obj)	     (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_APPLICATION))
#define NAUTILUS_IS_APPLICATION_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_APPLICATION))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
typedef struct NautilusWindow NautilusWindow;
#endif

#ifndef NAUTILUS_SPATIAL_WINDOW_DEFINED
#define NAUTILUS_SPATIAL_WINDOW_DEFINED
typedef struct _NautilusSpatialWindow NautilusSpatialWindow;
#endif

typedef struct NautilusShell NautilusShell;

typedef struct {
	BonoboGenericFactory parent;
	NautilusUndoManager *undo_manager;
	NautilusShell *shell;
	gboolean shell_registered;
	GVolumeMonitor *volume_monitor;
	unsigned int automount_idle_id;
} NautilusApplication;

typedef struct {
	BonoboGenericFactoryClass parent_class;
} NautilusApplicationClass;

GType                nautilus_application_get_type          (void);
NautilusApplication *nautilus_application_new               (void);
void                 nautilus_application_startup           (NautilusApplication *application,
							     gboolean             kill_shell,
							     gboolean             restart_shell,
							     gboolean             no_default_window,
							     gboolean             no_desktop,
							     gboolean             browser_window,
							     const char          *startup_id,
							     const char          *default_geometry,
							     const char          *session_to_load,
							     const char          *urls[]);
GList *              nautilus_application_get_window_list           (void);
GList *              nautilus_application_get_spatial_window_list    (void);
unsigned int         nautilus_application_get_n_windows            (void);

NautilusWindow *     nautilus_application_present_spatial_window     (NautilusApplication *application,
								      NautilusWindow      *requesting_window,
								      const char          *startup_id,
								      GFile               *location,
								      GdkScreen           *screen);
NautilusWindow *     nautilus_application_present_spatial_window_with_selection (NautilusApplication *application,
										 NautilusWindow      *requesting_window,
										 const char          *startup_id,
										 GFile               *location,
										 GList		     *new_selection,
										 GdkScreen           *screen);

NautilusWindow *     nautilus_application_create_navigation_window     (NautilusApplication *application,
									const char          *startup_id,
									GdkScreen           *screen);

void                 nautilus_application_close_all_navigation_windows (void);
void                 nautilus_application_close_parent_windows     (NautilusSpatialWindow *window);
void                 nautilus_application_close_all_spatial_windows  (void);
void                 nautilus_application_open_desktop      (NautilusApplication *application);
void                 nautilus_application_close_desktop     (void);
void                 nautilus_application_load_session      (NautilusApplication *application,
							     const char *filename); 
char *               nautilus_application_save_session_to_file (void);
#endif /* NAUTILUS_APPLICATION_H */

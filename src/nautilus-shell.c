/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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

/* nautilus-shell.h: Server side of the Nautilus:Shell CORBA object
 * that represents the shell across processes.
 */

#include <config.h>
#include "nautilus-shell.h"

#include "nautilus-desktop-window.h"
#include "nautilus-main.h"
#include "nautilus-window-private.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <stdlib.h>

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define START_STATE_CONFIG "start-state"

struct NautilusShellDetails {
	NautilusApplication *application;
};

static void     finalize                         (GObject              *shell);
static void     corba_open_windows              (PortableServer_Servant  servant,
						 const Nautilus_URIList *list,
						 const CORBA_char       *startup_id,
						 const CORBA_char       *geometry,
						 CORBA_boolean           browser_window,
						 CORBA_Environment      *ev);
static void     corba_open_default_window       (PortableServer_Servant  servant,
						 const CORBA_char       *startup_id,
						 const CORBA_char       *geometry,
						 CORBA_boolean           browser_window,
						 CORBA_Environment      *ev);
static void     corba_load_session              (PortableServer_Servant  servant,
						 const CORBA_char       *filename,
						 CORBA_Environment      *ev);
static void     corba_start_desktop             (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static void     corba_stop_desktop              (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static void     corba_quit                      (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static void     corba_restart                   (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);

BONOBO_CLASS_BOILERPLATE_FULL (NautilusShell, nautilus_shell,
			       Nautilus_Shell,
			       BonoboObject, BONOBO_OBJECT_TYPE)

static void
nautilus_shell_class_init (NautilusShellClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = finalize;

	klass->epv.open_windows = corba_open_windows;
	klass->epv.open_default_window = corba_open_default_window;
	klass->epv.load_session = corba_load_session;
	klass->epv.start_desktop = corba_start_desktop;
	klass->epv.stop_desktop = corba_stop_desktop;
	klass->epv.quit = corba_quit;
	klass->epv.restart = corba_restart;
}

static void
nautilus_shell_instance_init (NautilusShell *shell)
{
	shell->details = g_new0 (NautilusShellDetails, 1);
}

static void
finalize (GObject *object)
{
	NautilusShell *shell;

	shell = NAUTILUS_SHELL (object);
	g_free (shell->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

NautilusShell *
nautilus_shell_new (NautilusApplication *application)
{
	NautilusShell *shell;

	shell = NAUTILUS_SHELL (g_object_new (NAUTILUS_TYPE_SHELL, NULL));
	shell->details->application = application;
	return shell;
}

static void
open_window (NautilusShell *shell, const char *uri, const char *startup_id,
	     const char *geometry, gboolean browser_window)
{
	GFile *location;
	NautilusWindow *window;

	if (browser_window ||
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
		window = nautilus_application_create_navigation_window (shell->details->application,
									startup_id,
									gdk_screen_get_default ());
		if (uri == NULL) {
			nautilus_window_go_home (window);
		} else {
			location = g_file_new_for_uri (uri);
			nautilus_window_go_to (window, location);
			g_object_unref (location);
		}
	} else {
		if (uri == NULL) {
			location = g_file_new_for_path (g_get_home_dir ());
		} else {
			location = g_file_new_for_uri (uri);
		}
		
		window = nautilus_application_present_spatial_window (shell->details->application,
								      NULL,
								      startup_id,
								      location,
								      gdk_screen_get_default ());
		g_object_unref (location);
	}
	
	if (geometry != NULL && !GTK_WIDGET_VISIBLE (window)) {
		eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
								 geometry,
								 APPLICATION_WINDOW_MIN_WIDTH,
								 APPLICATION_WINDOW_MIN_HEIGHT,
								 FALSE);
	}
}

typedef struct {
	NautilusShell *shell;
	char *startup_id;
	char *geometry;
	GList *uris;
	gboolean browser_window;
} OpenWindowData;

static gboolean
open_windows_at_idle (gpointer _data)
{
	OpenWindowData *data = _data;
	char *uri;
	GList *l;

	if (data->uris == NULL) {
		/* Open a window pointing at the default location. */
		open_window (data->shell, NULL, data->startup_id,
			     data->geometry, data->browser_window);
	} else {
		/* Open windows at each requested location. */
		for (l = data->uris; l != NULL; l = l->next) {
			uri = (char *)l->data;
			open_window (data->shell, uri, data->startup_id,
				     data->geometry, data->browser_window);
			g_free (uri);
		}
	}

	g_free (data->startup_id);
	g_free (data->geometry);
	g_list_free (data->uris);
	g_free (data);
	return FALSE;
}

static void
corba_open_windows (PortableServer_Servant servant,
		    const Nautilus_URIList *list,
		    const CORBA_char *startup_id,
		    const CORBA_char *geometry,
		    CORBA_boolean browser_window,
		    CORBA_Environment *ev)
{
	OpenWindowData *data;
	guint i;

	data = g_new0 (OpenWindowData, 1);
	
	data->shell = NAUTILUS_SHELL (bonobo_object_from_servant (servant));
	data->startup_id = g_strdup (startup_id);
	data->geometry = g_strdup (geometry);
	data->browser_window = browser_window;

	for (i = 0; i < list->_length; i++) {
		g_assert (list->_buffer[i] != NULL);
		data->uris = g_list_prepend (data->uris,
					     g_strdup (list->_buffer[i]));
	}
	data->uris = g_list_reverse (data->uris);

	g_idle_add (open_windows_at_idle, data);
	
}

static void
corba_open_default_window (PortableServer_Servant servant,
			   const CORBA_char *startup_id,
			   const CORBA_char *geometry,
			   CORBA_boolean browser_window,
			   CORBA_Environment *ev)
{
	OpenWindowData *data;

	data = g_new0 (OpenWindowData, 1);
	
	data->shell = NAUTILUS_SHELL (bonobo_object_from_servant (servant));
	data->startup_id = g_strdup (startup_id);
	data->geometry = g_strdup (geometry);
	data->browser_window = browser_window;

	g_idle_add (open_windows_at_idle, data);
}

typedef struct {
	NautilusShell *shell;
	char *filename;
} LoadSessionData;

static gboolean
load_session_at_idle (gpointer _data)
{
	NautilusApplication *application;
	LoadSessionData *data = _data;

	application = NAUTILUS_APPLICATION (data->shell->details->application);
	nautilus_application_load_session (application, data->filename);
	
	g_free (data->filename);
	g_free (data);
	return FALSE;
}

static void
corba_load_session (PortableServer_Servant servant,
		    const CORBA_char *filename,
		    CORBA_Environment *ev)
{
	LoadSessionData *data;

	data = g_new0 (LoadSessionData, 1);
	data->shell = NAUTILUS_SHELL (bonobo_object_from_servant (servant));
	data->filename = g_strdup (filename);

	g_idle_add (load_session_at_idle, data);
}

typedef struct {
	NautilusShell *shell;
} StartDesktopData;

static gboolean
start_desktop_at_idle (gpointer _data)
{
	StartDesktopData *data = _data;
	NautilusApplication *application;

	application = NAUTILUS_APPLICATION (data->shell->details->application);
	nautilus_application_open_desktop (application);

	g_free (data);
	return FALSE;
}


static void
corba_start_desktop (PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	StartDesktopData  *data;
	
	data = g_new0 (StartDesktopData, 1);
	data->shell = NAUTILUS_SHELL (bonobo_object_from_servant (servant));

	g_idle_add (start_desktop_at_idle, data);
}

static gboolean
stop_desktop_at_idle (gpointer data)
{
	nautilus_application_close_desktop ();
	return FALSE;
}

static void
corba_stop_desktop (PortableServer_Servant servant,
		    CORBA_Environment *ev)
{	
	g_idle_add (stop_desktop_at_idle, NULL);
}

static gboolean
quit_at_idle (gpointer data)
{
	nautilus_main_event_loop_quit (TRUE);
	return FALSE;
}


static void
corba_quit (PortableServer_Servant servant,
	    CORBA_Environment *ev)
{
	g_idle_add (quit_at_idle, NULL);
}

static gboolean
restart_at_idle (gpointer data)
{
	char *filename;

	filename = nautilus_application_save_session_to_file ();
	if (filename != NULL) {
		nautilus_main_event_loop_quit (TRUE);
		g_setenv ("_NAUTILUS_RESTART_SESSION_FILENAME", filename, 1);
		g_free (filename);
	} else {
		g_message ("Could not save session. Not restarting.");
	}


	return FALSE;
}

static void
corba_restart (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	g_idle_add (restart_at_idle, NULL);
}

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
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
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
						 const CORBA_char       *geometry,
						 CORBA_Environment      *ev);
static void     corba_open_default_window       (PortableServer_Servant  servant,
						 const CORBA_char       *geometry,
						 CORBA_Environment      *ev);
static void     corba_start_desktop             (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static void     corba_stop_desktop              (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static void     corba_quit                      (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static void     corba_restart                   (PortableServer_Servant  servant,
						 CORBA_Environment      *ev);
static gboolean restore_window_states           (NautilusShell          *shell);

BONOBO_CLASS_BOILERPLATE_FULL (NautilusShell, nautilus_shell,
			       Nautilus_Shell,
			       BonoboObject, BONOBO_OBJECT_TYPE)

static void
nautilus_shell_class_init (NautilusShellClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = finalize;

	klass->epv.open_windows = corba_open_windows;
	klass->epv.open_default_window = corba_open_default_window;
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
open_window (NautilusShell *shell, const char *uri, const char *geometry)
{
	NautilusWindow *window;
	NautilusWindow *existing_window;
	GList *node;
	const char *existing_location;
	gboolean prefer_existing_window;

	prefer_existing_window = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW);


        /* If the user's preference is always_open_in_new_window
	 * we raise an existing window for the location if it already exists.
	 */
	if (prefer_existing_window)
	{
        	for (node = nautilus_application_get_window_list ();
             	     node != NULL; node = node->next) {
               	     
			existing_window = NAUTILUS_WINDOW (node->data);
                     	existing_location = existing_window->details->pending_location;
                
			if (existing_location == NULL) {
                        	existing_location = existing_window->details->location;
                	}

                	if (eel_uris_match (existing_location, uri)) {
                        	gtk_window_present (GTK_WINDOW (existing_window));
                        	return;
                	}
		}
        }

        /* Otherwise, open a new window. */
	window = nautilus_application_create_window (shell->details->application,
						     gdk_screen_get_default ());

	if (geometry != NULL) {
		eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
								      geometry,
								      APPLICATION_WINDOW_MIN_WIDTH,
								      APPLICATION_WINDOW_MIN_HEIGHT,
								      FALSE);
	}

	if (uri == NULL) {
		nautilus_window_go_home (window);
	} else {
		nautilus_window_go_to (window, uri);
	}
}

static void
corba_open_windows (PortableServer_Servant servant,
		    const Nautilus_URIList *list,
		    const CORBA_char *geometry,
		    CORBA_Environment *ev)
{
	NautilusShell *shell;
	guint i;

	shell = NAUTILUS_SHELL (bonobo_object_from_servant (servant));

	/* Open windows at each requested location. */
	for (i = 0; i < list->_length; i++) {
		g_assert (list->_buffer[i] != NULL);
		open_window (shell, list->_buffer[i], geometry);
	}
}

static void
corba_open_default_window (PortableServer_Servant servant,
			   const CORBA_char *geometry,
			   CORBA_Environment *ev)
{
	NautilusShell *shell;

	shell = NAUTILUS_SHELL (bonobo_object_from_servant (servant));

	if (!restore_window_states (shell)) {
		/* Open a window pointing at the default location. */
		open_window (shell, NULL, geometry);
	}
}

static void
corba_start_desktop (PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	NautilusShell	      *shell;
	NautilusApplication   *application;

	shell	    = NAUTILUS_SHELL (bonobo_object_from_servant (servant));
	application = NAUTILUS_APPLICATION (shell->details->application);
	
	nautilus_application_open_desktop (application);
}

static void
corba_stop_desktop (PortableServer_Servant servant,
		    CORBA_Environment *ev)
{	
	nautilus_application_close_desktop ();
}

static void
corba_quit (PortableServer_Servant servant,
	    CORBA_Environment *ev)
{
	nautilus_main_event_loop_quit ();
}

/*
 * code for saving the state of nautilus windows across a restart
 *
 * for now, only the window geometry & uri is saved, into "start-state",
 * in a list of strings like:
 *
 *     "<width>,<height>,<x>,<y>,<location>"
 *
 * For example:
 *
 *     "800,600,10,10,file:///tmp"
 */

#define WINDOW_STATE_ATTRIBUTE_WIDTH	0
#define WINDOW_STATE_ATTRIBUTE_HEIGHT	1
#define WINDOW_STATE_ATTRIBUTE_X	2
#define WINDOW_STATE_ATTRIBUTE_Y	3
#define WINDOW_STATE_ATTRIBUTE_LOCATION	4
#define WINDOW_STATE_ATTRIBUTE_SCREEN	5

static void
save_window_states (void)
{
	GList *windows;
	GList *node;
	NautilusWindow *window;
	GdkWindow *gdk_window;
	char *window_attributes;
	int x, y, width, height;
	char *location;
	EelStringList *states;
	int screen_num = -1;

	states = NULL;
	windows = nautilus_application_get_window_list ();
	for (node = windows; node; node = g_list_next (node)) {
		g_assert (node->data != NULL);
		window = node->data;

		width = GTK_WIDGET (window)->allocation.width;
		height = GTK_WIDGET (window)->allocation.height;

		/* need root origin (origin of all the window dressing) */
		gdk_window = GTK_WIDGET (window)->window;
		gdk_window_get_root_origin (gdk_window, &x, &y);

		location = nautilus_window_get_location (window);

		screen_num = gdk_screen_get_number (
					gtk_window_get_screen (GTK_WINDOW (window)));

		window_attributes = g_strdup_printf ("%d,%d,%d,%d,%s,%d", 
						     width, height, 
						     x, y, 
						     location,
						     screen_num);
		g_free (location);
		
		if (states == NULL) {
			states = eel_string_list_new (TRUE);
		}
		eel_string_list_insert (states, window_attributes);
		g_free (window_attributes);
	}

	if (eel_preferences_key_is_writable (START_STATE_CONFIG)) {
		eel_preferences_set_string_list (START_STATE_CONFIG, states);
	}

	eel_string_list_free (states);
}

static void
restore_one_window_callback (const char *attributes,
			     gpointer callback_data)
{
	NautilusShell *shell;
	EelStringList *attribute_list;
	int x;
	int y;
	int width;
	int height;
	char *location;
	NautilusWindow *window;
	GdkScreen *screen = NULL;
	int screen_num;
	int list_length;

	g_return_if_fail (eel_strlen (attributes) > 0);
	g_return_if_fail (NAUTILUS_IS_SHELL (callback_data));

	shell = NAUTILUS_SHELL (callback_data);

	attribute_list = eel_string_list_new_from_tokens (attributes, ",", TRUE);

	list_length = eel_string_list_get_length (attribute_list);

	eel_string_list_nth_as_integer (attribute_list, WINDOW_STATE_ATTRIBUTE_WIDTH, &width);
	eel_string_list_nth_as_integer (attribute_list, WINDOW_STATE_ATTRIBUTE_HEIGHT, &height);
	eel_string_list_nth_as_integer (attribute_list, WINDOW_STATE_ATTRIBUTE_X, &x);
	eel_string_list_nth_as_integer (attribute_list, WINDOW_STATE_ATTRIBUTE_Y, &y);
	location = eel_string_list_nth (attribute_list, WINDOW_STATE_ATTRIBUTE_LOCATION);

	/* Support sessions with no screen number for backwards compat.
	 */
	if (list_length >= WINDOW_STATE_ATTRIBUTE_SCREEN + 1) {
		eel_string_list_nth_as_integer (
			attribute_list, WINDOW_STATE_ATTRIBUTE_SCREEN, &screen_num);

		screen = gdk_display_get_screen (gdk_display_get_default (), screen_num);
	} else {
		screen = gdk_screen_get_default ();
	}

	window = nautilus_application_create_window (shell->details->application, screen);
	
	if (eel_strlen (location) > 0) {
		nautilus_window_go_to (window, location);
	} else {
		nautilus_window_go_home (window);
	}

	gtk_window_move (GTK_WINDOW (window), x, y);
	gtk_widget_set_size_request (GTK_WIDGET (window), width, height);

	g_free (location);
	eel_string_list_free (attribute_list);
}

/* returns TRUE if there was state info which has been used to create new windows */
static gboolean
restore_window_states (NautilusShell *shell)
{
	EelStringList *states;
	gboolean result;

	states = eel_preferences_get_string_list (START_STATE_CONFIG);
	result = eel_string_list_get_length (states) > 0;
	eel_string_list_for_each (states, restore_one_window_callback, shell);
	eel_string_list_free (states);
	if (eel_preferences_key_is_writable (START_STATE_CONFIG)) {
		eel_preferences_set_string_list (START_STATE_CONFIG, NULL);
	}
	return result;
}

static void
corba_restart (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	save_window_states ();

	nautilus_main_event_loop_quit ();
	eel_setenv ("_NAUTILUS_RESTART", "yes", 1);
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nautilus-application: main Nautilus application class.
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Cosimo Cecchi <cosimoc@gnome.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include "nautilus-application.h"

#if ENABLE_EMPTY_VIEW
#include "nautilus-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */

#include "nautilus-desktop-icon-view.h"
#include "nautilus-desktop-window.h"
#include "nautilus-icon-view.h"
#include "nautilus-image-properties-page.h"
#include "nautilus-list-view.h"
#include "nautilus-navigation-window.h"
#include "nautilus-navigation-window-slot.h"
#include "nautilus-progress-ui-handler.h"
#include "nautilus-self-check-functions.h"
#include "nautilus-spatial-window.h"
#include "nautilus-window-bookmarks.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window-slot.h"

#include <libnautilus-private/nautilus-dbus-manager.h>
#include <libnautilus-private/nautilus-desktop-link-monitor.h>
#include <libnautilus-private/nautilus-directory-private.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-lib-self-check-functions.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus-extension/nautilus-menu-provider.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_APPLICATION
#include <libnautilus-private/nautilus-debug.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <libnotify/notify.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define START_STATE_CONFIG "start-state"

#define NAUTILUS_ACCEL_MAP_SAVE_DELAY 30

static NautilusApplication *singleton = NULL;

/* Keeps track of all the desktop windows. */
static GList *nautilus_application_desktop_windows;

/* Keeps track of all the object windows */
static GList *nautilus_application_spatial_window_list;

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

static void     desktop_changed_callback          (gpointer                  user_data);
static void     mount_removed_callback            (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NautilusApplication       *application);
static void     mount_added_callback              (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NautilusApplication       *application);

G_DEFINE_TYPE (NautilusApplication, nautilus_application, GTK_TYPE_APPLICATION);

struct _NautilusApplicationPriv {
	GVolumeMonitor *volume_monitor;
	GDBusProxy *ck_proxy;
	gboolean session_is_active;
	NautilusProgressUIHandler *progress_handler;

	gboolean initialized;
};

static GList *
nautilus_application_get_spatial_window_list (void)
{
	return nautilus_application_spatial_window_list;
}

static gboolean
check_required_directories (NautilusApplication *application)
{
	char *user_directory;
	char *desktop_directory;
	GSList *directories;
	gboolean ret;

	g_assert (NAUTILUS_IS_APPLICATION (application));

	ret = TRUE;

	user_directory = nautilus_get_user_directory ();
	desktop_directory = nautilus_get_desktop_directory ();

	directories = NULL;

	if (!g_file_test (user_directory, G_FILE_TEST_IS_DIR)) {
		directories = g_slist_prepend (directories, user_directory);
	}

	if (!g_file_test (desktop_directory, G_FILE_TEST_IS_DIR)) {
		directories = g_slist_prepend (directories, desktop_directory);
	}

	if (directories != NULL) {
		int failed_count;
		GString *directories_as_string;
		GSList *l;
		char *error_string;
		const char *detail_string;
		GtkDialog *dialog;

		ret = FALSE;

		failed_count = g_slist_length (directories);

		directories_as_string = g_string_new ((const char *)directories->data);
		for (l = directories->next; l != NULL; l = l->next) {
			g_string_append_printf (directories_as_string, ", %s", (const char *)l->data);
		}

		if (failed_count == 1) {
			error_string = g_strdup_printf (_("Nautilus could not create the required folder \"%s\"."),
							directories_as_string->str);
			detail_string = _("Before running Nautilus, please create the following folder, or "
					  "set permissions such that Nautilus can create it.");
		} else {
			error_string = g_strdup_printf (_("Nautilus could not create the following required folders: "
							  "%s."), directories_as_string->str);
			detail_string = _("Before running Nautilus, please create these folders, or "
					  "set permissions such that Nautilus can create them.");
		}

		dialog = eel_show_error_dialog (error_string, detail_string, NULL);
		/* We need the main event loop so the user has a chance to see the dialog. */
		gtk_application_add_window (GTK_APPLICATION (application),
					    GTK_WINDOW (dialog));

		g_string_free (directories_as_string, TRUE);
		g_free (error_string);
	}

	g_slist_free (directories);
	g_free (user_directory);
	g_free (desktop_directory);

	return ret;
}

static void
menu_provider_items_updated_handler (NautilusMenuProvider *provider, GtkWidget* parent_window, gpointer data)
{

	g_signal_emit_by_name (nautilus_signaller_get_current (),
			       "popup_menu_changed");
}

static void
menu_provider_init_callback (void)
{
        GList *providers;
        GList *l;

        providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);

        for (l = providers; l != NULL; l = l->next) {
                NautilusMenuProvider *provider = NAUTILUS_MENU_PROVIDER (l->data);

		g_signal_connect_after (G_OBJECT (provider), "items_updated",
                           (GCallback)menu_provider_items_updated_handler,
                           NULL);
        }

        nautilus_module_extension_list_free (providers);
}

static void
mark_desktop_files_trusted (void)
{
	char *do_once_file;
	GFile *f, *c;
	GFileEnumerator *e;
	GFileInfo *info;
	const char *name;
	int fd;
	
	do_once_file = g_build_filename (g_get_user_data_dir (),
					 ".converted-launchers", NULL);

	if (g_file_test (do_once_file, G_FILE_TEST_EXISTS)) {
		goto out;
	}

	f = nautilus_get_desktop_location ();
	e = g_file_enumerate_children (f,
				       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				       G_FILE_ATTRIBUTE_STANDARD_NAME ","
				       G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE
				       ,
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, NULL);
	if (e == NULL) {
		goto out2;
	}
	
	while ((info = g_file_enumerator_next_file (e, NULL, NULL)) != NULL) {
		name = g_file_info_get_name (info);
		
		if (g_str_has_suffix (name, ".desktop") &&
		    !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
			c = g_file_get_child (f, name);
			nautilus_file_mark_desktop_file_trusted (c,
								 NULL, FALSE,
								 NULL, NULL);
			g_object_unref (c);
		}
		g_object_unref (info);
	}
	
	g_object_unref (e);
 out2:
	fd = g_creat (do_once_file, 0666);
	close (fd);
	
	g_object_unref (f);
 out:	
	g_free (do_once_file);
}

#define CK_NAME       "org.freedesktop.ConsoleKit"
#define CK_PATH       "/org/freedesktop/ConsoleKit"
#define CK_INTERFACE  "org.freedesktop.ConsoleKit"

static void
ck_session_proxy_signal_cb (GDBusProxy *proxy,
			    const char *sender_name,
			    const char *signal_name,
			    GVariant   *parameters,
			    gpointer    user_data)
{
	NautilusApplication *application = user_data;

	if (g_strcmp0 (signal_name, "ActiveChanged") == 0) {
		g_variant_get (parameters, "(b)", &application->priv->session_is_active);
		DEBUG ("ConsoleKit session is active %d", application->priv->session_is_active);
	}
}

static void
ck_call_is_active_cb (GDBusProxy   *proxy,
		      GAsyncResult *result,
		      gpointer      user_data)
{
	NautilusApplication *application = user_data;
	GVariant *variant;
	GError *error = NULL;

	variant = g_dbus_proxy_call_finish (proxy, result, &error);

	if (variant == NULL) {
		g_warning ("Error when calling IsActive(): %s\n", error->message);
		application->priv->session_is_active = TRUE;

		g_error_free (error);
		return;
	}

	g_variant_get (variant, "(b)", &application->priv->session_is_active);
	DEBUG ("ConsoleKit session is active %d", application->priv->session_is_active);

	g_variant_unref (variant);
}

static void
session_proxy_appeared (GObject       *source,
                        GAsyncResult *res,
                        gpointer      user_data)
{
	NautilusApplication *application = user_data;
        GDBusProxy *proxy;
	GError *error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

	if (error != NULL) {
		g_warning ("Failed to get the current CK session: %s", error->message);
		g_error_free (error);

		application->priv->session_is_active = TRUE;
		return;
	}

	g_signal_connect (proxy, "g-signal",
			  G_CALLBACK (ck_session_proxy_signal_cb),
			  application);

	g_dbus_proxy_call (proxy,
			   "IsActive",
			   g_variant_new ("()"),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   NULL,
			   (GAsyncReadyCallback) ck_call_is_active_cb,
			   application);

        application->priv->ck_proxy = proxy;
}

static void
ck_get_current_session_cb (GDBusConnection *connection,
			   GAsyncResult    *result,
			   gpointer         user_data)
{
	NautilusApplication *application = user_data;
	GVariant *variant;
	const char *session_path = NULL;
	GError *error = NULL;

	variant = g_dbus_connection_call_finish (connection, result, &error);

	if (variant == NULL) {
		g_warning ("Failed to get the current CK session: %s", error->message);
		g_error_free (error);

		application->priv->session_is_active = TRUE;
		return;
	}

	g_variant_get (variant, "(&o)", &session_path);

	DEBUG ("Found ConsoleKit session at path %s", session_path);

	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_NONE,
				  NULL,
				  CK_NAME,
				  session_path,
				  CK_INTERFACE ".Session",
				  NULL,
				  session_proxy_appeared,
				  application);

	g_variant_unref (variant);
}

static void
do_initialize_consolekit (NautilusApplication *application)
{
	GDBusConnection *connection;

	connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

	if (connection == NULL) {
		application->priv->session_is_active = TRUE;
		return;
	}

	g_dbus_connection_call (connection,
				CK_NAME,
				CK_PATH "/Manager",
				CK_INTERFACE ".Manager",
				"GetCurrentSession",
				g_variant_new ("()"),
				G_VARIANT_TYPE ("(o)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback) ck_get_current_session_cb,
				application);

	g_object_unref (connection);
}

static void
do_upgrades_once (NautilusApplication *application,
		  gboolean no_desktop)
{
	char *metafile_dir, *updated, *nautilus_dir, *xdg_dir;
	const gchar *message;
	int fd, res;

	if (!no_desktop) {
		mark_desktop_files_trusted ();
	}

	metafile_dir = g_build_filename (g_get_home_dir (),
					 ".nautilus/metafiles", NULL);
	if (g_file_test (metafile_dir, G_FILE_TEST_IS_DIR)) {
		updated = g_build_filename (metafile_dir, "migrated-to-gvfs", NULL);
		if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
			g_spawn_command_line_async (LIBEXECDIR"/nautilus-convert-metadata --quiet", NULL);
			fd = g_creat (updated, 0600);
			if (fd != -1) {
				close (fd);
			}
		}
		g_free (updated);
	}
	g_free (metafile_dir);

	nautilus_dir = g_build_filename (g_get_home_dir (),
					 ".nautilus", NULL);
	xdg_dir = nautilus_get_user_directory ();
	if (g_file_test (nautilus_dir, G_FILE_TEST_IS_DIR)) {
		/* test if we already attempted to migrate first */
		updated = g_build_filename (nautilus_dir, "DEPRECATED-DIRECTORY", NULL);
		message = _("Nautilus 3.0 deprecated this directory and tried migrating "
			    "this configuration to ~/.config/nautilus");
		if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
			/* rename() works fine if the destination directory is
			 * empty.
			 */
			res = g_rename (nautilus_dir, xdg_dir);

			if (res == -1) {
				fd = g_creat (updated, 0600);
				if (fd != -1) {
					res = write (fd, message, strlen (message));
					close (fd);
				}
			}
		}

		g_free (updated);
	}

	g_free (nautilus_dir);
	g_free (xdg_dir);
}

static void
finish_startup (NautilusApplication *application,
		gboolean no_desktop)
{
	do_upgrades_once (application, no_desktop);
	
	/* initialize nautilus modules */
	nautilus_module_setup ();

	/* attach menu-provider module callback */
	menu_provider_init_callback ();
	
	/* Initialize the desktop link monitor singleton */
	nautilus_desktop_link_monitor_get ();

	/* Initialize the ConsoleKit listener for active session */
	do_initialize_consolekit (application);

	/* Initialize the UI handler singleton for file operations */
	notify_init (GETTEXT_PACKAGE);
	application->priv->progress_handler = nautilus_progress_ui_handler_new ();

	/* Watch for unmounts so we can close open windows */
	/* TODO-gio: This should be using the UNMOUNTED feature of GFileMonitor instead */
	application->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (application->priv->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), application, 0);
	g_signal_connect_object (application->priv->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), application, 0);
}

static void
open_window (NautilusApplication *application,
	     const char *startup_id,
	     const char *uri, GdkScreen *screen, const char *geometry,
	     gboolean browser_window)
{
	GFile *location;
	NautilusWindow *window;
	gboolean open_in_browser;

	if (uri == NULL) {
		location = g_file_new_for_path (g_get_home_dir ());
	} else {
		location = g_file_new_for_uri (uri);
	}

	open_in_browser = browser_window ||
		g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER);

	DEBUG ("Opening new window at uri %s, browser %d", uri, open_in_browser);

	if (open_in_browser) {
		window = nautilus_application_create_navigation_window (application,
									startup_id,
									screen);
	} else {
		window = nautilus_application_get_spatial_window (application,
								  NULL,
								  startup_id,
								  location,
								  screen,
								  NULL);
	}

	nautilus_window_go_to (window, location);

	g_object_unref (location);

	if (geometry != NULL && !gtk_widget_get_visible (GTK_WIDGET (window))) {
		/* never maximize windows opened from shell if a
		 * custom geometry has been requested.
		 */
		gtk_window_unmaximize (GTK_WINDOW (window));
		eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
								 geometry,
								 APPLICATION_WINDOW_MIN_WIDTH,
								 APPLICATION_WINDOW_MIN_HEIGHT,
								 FALSE);
	}
}

static void
open_windows (NautilusApplication *application,
	      const char *startup_id,
	      char **uris,
	      GdkScreen *screen,
	      const char *geometry,
	      gboolean browser_window)
{
	guint i;

	if (uris == NULL || uris[0] == NULL) {
		/* Open a window pointing at the default location. */
		open_window (application, startup_id, NULL, screen, geometry, browser_window);
	} else {
		/* Open windows at each requested location. */
		for (i = 0; uris[i] != NULL; i++) {
			open_window (application, startup_id, uris[i], screen, geometry, browser_window);
		}
	}
}

static gboolean 
nautilus_application_save_accel_map (gpointer data)
{
	if (save_of_accel_map_requested) {
		char *accel_map_filename;
	 	accel_map_filename = nautilus_get_accel_map_file ();
	 	if (accel_map_filename) {
	 		gtk_accel_map_save (accel_map_filename);
	 		g_free (accel_map_filename);
	 	}
		save_of_accel_map_requested = FALSE;
	}

	return FALSE;
}


static void 
queue_accel_map_save_callback (GtkAccelMap *object, gchar *accel_path,
		guint accel_key, GdkModifierType accel_mods,
		gpointer user_data)
{
	if (!save_of_accel_map_requested) {
		save_of_accel_map_requested = TRUE;
		g_timeout_add_seconds (NAUTILUS_ACCEL_MAP_SAVE_DELAY, 
				nautilus_application_save_accel_map, NULL);
	}
}

static void 
selection_get_cb (GtkWidget          *widget,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time)
{
	/* No extra targets atm */
}

static GtkWidget *
get_desktop_manager_selection (GdkDisplay *display, int screen)
{
	char selection_name[32];
	GdkAtom selection_atom;
	Window selection_owner;
	GtkWidget *selection_widget;

	g_snprintf (selection_name, sizeof (selection_name), "_NET_DESKTOP_MANAGER_S%d", screen);
	selection_atom = gdk_atom_intern (selection_name, FALSE);

	selection_owner = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
					      gdk_x11_atom_to_xatom_for_display (display, 
										 selection_atom));
	if (selection_owner != None) {
		return NULL;
	}
	
	selection_widget = gtk_invisible_new_for_screen (gdk_display_get_screen (display, screen));
	/* We need this for gdk_x11_get_server_time() */
	gtk_widget_add_events (selection_widget, GDK_PROPERTY_CHANGE_MASK);

	if (gtk_selection_owner_set_for_display (display,
						 selection_widget,
						 selection_atom,
						 gdk_x11_get_server_time (gtk_widget_get_window (selection_widget)))) {
		
		g_signal_connect (selection_widget, "selection_get",
				  G_CALLBACK (selection_get_cb), NULL);
		return selection_widget;
	}

	gtk_widget_destroy (selection_widget);
	
	return NULL;
}

static void
desktop_unrealize_cb (GtkWidget        *widget,
		      GtkWidget        *selection_widget)
{
	gtk_widget_destroy (selection_widget);
}

static gboolean
selection_clear_event_cb (GtkWidget	        *widget,
			  GdkEventSelection     *event,
			  NautilusDesktopWindow *window)
{
	gtk_widget_destroy (GTK_WIDGET (window));
	
	nautilus_application_desktop_windows =
		g_list_remove (nautilus_application_desktop_windows, window);

	return TRUE;
}

static void
nautilus_application_create_desktop_windows (NautilusApplication *application)
{
	GdkDisplay *display;
	NautilusDesktopWindow *window;
	GtkWidget *selection_widget;
	int screens, i;

	display = gdk_display_get_default ();
	screens = gdk_display_get_n_screens (display);

	for (i = 0; i < screens; i++) {

		DEBUG ("Creating a desktop window for screen %d", i);
		
		selection_widget = get_desktop_manager_selection (display, i);
		if (selection_widget != NULL) {
			window = nautilus_desktop_window_new (application,
							      gdk_display_get_screen (display, i));
			
			g_signal_connect (selection_widget, "selection_clear_event",
					  G_CALLBACK (selection_clear_event_cb), window);
			
			g_signal_connect (window, "unrealize",
					  G_CALLBACK (desktop_unrealize_cb), selection_widget);
			
			/* We realize it immediately so that the NAUTILUS_DESKTOP_WINDOW_ID
			   property is set so gnome-settings-daemon doesn't try to set the
			   background. And we do a gdk_flush() to be sure X gets it. */
			gtk_widget_realize (GTK_WIDGET (window));
			gdk_flush ();

			nautilus_application_desktop_windows =
				g_list_prepend (nautilus_application_desktop_windows, window);

			gtk_application_add_window (GTK_APPLICATION (application),
						    GTK_WINDOW (window));
		}
	}
}

static void
nautilus_application_open_desktop (NautilusApplication *application)
{
	if (nautilus_application_desktop_windows == NULL) {
		nautilus_application_create_desktop_windows (application);
	}
}

static void
nautilus_application_close_desktop (void)
{
	if (nautilus_application_desktop_windows != NULL) {
		g_list_foreach (nautilus_application_desktop_windows,
				(GFunc) gtk_widget_destroy, NULL);
		g_list_free (nautilus_application_desktop_windows);
		nautilus_application_desktop_windows = NULL;
	}
}

void
nautilus_application_close_all_navigation_windows (NautilusApplication *self)
{
	GList *list_copy;
	GList *l;
	
	list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (self)));
	/* First hide all window to get the feeling of quick response */
	for (l = list_copy; l != NULL; l = l->next) {
		NautilusWindow *window;
		
		window = NAUTILUS_WINDOW (l->data);

		if (NAUTILUS_IS_NAVIGATION_WINDOW (window)) {
			gtk_widget_hide (GTK_WIDGET (window));
		}
	}

	for (l = list_copy; l != NULL; l = l->next) {
		NautilusWindow *window;
		
		window = NAUTILUS_WINDOW (l->data);
		
		if (NAUTILUS_IS_NAVIGATION_WINDOW (window)) {
			nautilus_window_close (window);
		}
	}
	g_list_free (list_copy);
}

static NautilusSpatialWindow *
nautilus_application_get_existing_spatial_window (GFile *location)
{
	GList *l;
	NautilusWindowSlot *slot;
	GFile *window_location;

	for (l = nautilus_application_get_spatial_window_list ();
	     l != NULL; l = l->next) {
		slot = NAUTILUS_WINDOW (l->data)->details->active_pane->active_slot;

		window_location = slot->pending_location;
		
		if (window_location == NULL) {
			window_location = slot->location;
		}

		if (window_location != NULL) {
			if (g_file_equal (location, window_location)) {
				return NAUTILUS_SPATIAL_WINDOW (l->data);
			}
		}
	}

	return NULL;
}

static NautilusSpatialWindow *
find_parent_spatial_window (NautilusSpatialWindow *window)
{
	NautilusFile *file;
	NautilusFile *parent_file;
	NautilusWindowSlot *slot;
	GFile *location;

	slot = NAUTILUS_WINDOW (window)->details->active_pane->active_slot;

	location = slot->location;
	if (location == NULL) {
		return NULL;
	}
	file = nautilus_file_get (location);

	if (!file) {
		return NULL;
	}

	parent_file = nautilus_file_get_parent (file);
	nautilus_file_unref (file);
	while (parent_file) {
		NautilusSpatialWindow *parent_window;

		location = nautilus_file_get_location (parent_file);
		parent_window = nautilus_application_get_existing_spatial_window (location);
		g_object_unref (location);

		/* Stop at the desktop directory if it's not explicitely opened
		 * in a spatial window of its own.
		 */
		if (nautilus_file_is_desktop_directory (parent_file) && !parent_window) {
			nautilus_file_unref (parent_file);
			return NULL;
		}

		if (parent_window) {
			nautilus_file_unref (parent_file);
			return parent_window;
		}
		file = parent_file;
		parent_file = nautilus_file_get_parent (file);
		nautilus_file_unref (file);
	}

	return NULL;
}

void
nautilus_application_close_parent_windows (NautilusSpatialWindow *window)
{
	NautilusSpatialWindow *parent_window;
	NautilusSpatialWindow *new_parent_window;

	g_return_if_fail (NAUTILUS_IS_SPATIAL_WINDOW (window));

	parent_window = find_parent_spatial_window (window);
	
	while (parent_window) {
		
		new_parent_window = find_parent_spatial_window (parent_window);
		nautilus_window_close (NAUTILUS_WINDOW (parent_window));
		parent_window = new_parent_window;
	}
}

void
nautilus_application_close_all_spatial_windows (void)
{
	GList *list_copy;
	GList *l;
	
	list_copy = g_list_copy (nautilus_application_spatial_window_list);
	/* First hide all window to get the feeling of quick response */
	for (l = list_copy; l != NULL; l = l->next) {
		NautilusWindow *window;
		
		window = NAUTILUS_WINDOW (l->data);
		
		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			gtk_widget_hide (GTK_WIDGET (window));
		}
	}

	for (l = list_copy; l != NULL; l = l->next) {
		NautilusWindow *window;
		
		window = NAUTILUS_WINDOW (l->data);
		
		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			nautilus_window_close (window);
		}
	}
	g_list_free (list_copy);
}

static gboolean
nautilus_window_delete_event_callback (GtkWidget *widget,
				       GdkEvent *event,
				       gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (widget);
	nautilus_window_close (window);

	return TRUE;
}				       


static NautilusWindow *
create_window (NautilusApplication *application,
	       GType window_type,
	       const char *startup_id,
	       GdkScreen *screen)
{
	NautilusWindow *window;
	
	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);
	
	window = NAUTILUS_WINDOW (gtk_widget_new (window_type,
						  "app", application,
						  "screen", screen,
						  NULL));

	if (startup_id) {
		gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
	}
	
	g_signal_connect_data (window, "delete_event",
			       G_CALLBACK (nautilus_window_delete_event_callback), NULL, NULL,
			       G_CONNECT_AFTER);

	gtk_application_add_window (GTK_APPLICATION (application),
				    GTK_WINDOW (window));

	/* Do not yet show the window. It will be shown later on if it can
	 * successfully display its initial URI. Otherwise it will be destroyed
	 * without ever having seen the light of day.
	 */

	return window;
}

static void
spatial_window_destroyed_callback (void *user_data, GObject *window)
{
	nautilus_application_spatial_window_list =
		g_list_remove (nautilus_application_spatial_window_list, window);
}

NautilusWindow *
nautilus_application_get_spatial_window (NautilusApplication *application,
					 NautilusWindow      *requesting_window,
					 const char          *startup_id,
					 GFile               *location,
					 GdkScreen           *screen,
					 gboolean            *existing)
{
	NautilusWindow *window;

	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);

	window = NAUTILUS_WINDOW
		(nautilus_application_get_existing_spatial_window (location));

	if (window != NULL) {
		if (existing != NULL) {
			*existing = TRUE;
		}

		DEBUG ("returning existing spatial window");

		return window;
	}

	if (existing != NULL) {
		*existing = FALSE;
	}

	window = create_window (application, NAUTILUS_TYPE_SPATIAL_WINDOW, startup_id, screen);
	if (requesting_window) {
		/* Center the window over the requesting window by default */
		int orig_x, orig_y, orig_width, orig_height;
		int new_x, new_y, new_width, new_height;
		
		gtk_window_get_position (GTK_WINDOW (requesting_window), 
					 &orig_x, &orig_y);
		gtk_window_get_size (GTK_WINDOW (requesting_window), 
				     &orig_width, &orig_height);
		gtk_window_get_default_size (GTK_WINDOW (window),
					     &new_width, &new_height);
		
		new_x = orig_x + (orig_width - new_width) / 2;
		new_y = orig_y + (orig_height - new_height) / 2;
		
		if (orig_width - new_width < 10) {
			new_x += 10;
			new_y += 10;
		}

		gtk_window_move (GTK_WINDOW (window), new_x, new_y);
	}

	nautilus_application_spatial_window_list = g_list_prepend (nautilus_application_spatial_window_list, window);
	g_object_weak_ref (G_OBJECT (window), 
			   spatial_window_destroyed_callback, NULL);

	DEBUG ("Creating new spatial window");

	return window;
}

static gboolean
another_navigation_window_already_showing (NautilusApplication *application,
					   NautilusWindow *the_window)
{
	GList *list, *item;
	
	list = gtk_application_get_windows (GTK_APPLICATION (application));
	for (item = list; item != NULL; item = item->next) {
		if (item->data != the_window &&
		    NAUTILUS_IS_NAVIGATION_WINDOW (item->data)) {
			return TRUE;
		}
	}
	
	return FALSE;
}

NautilusWindow *
nautilus_application_create_navigation_window (NautilusApplication *application,
					       const char          *startup_id,
					       GdkScreen           *screen)
{
	NautilusWindow *window;
	char *geometry_string;
	gboolean maximized;

	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);

	window = create_window (application, NAUTILUS_TYPE_NAVIGATION_WINDOW, startup_id, screen);

	maximized = g_settings_get_boolean
		(nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED);
	if (maximized) {
		gtk_window_maximize (GTK_WINDOW (window));
	} else {
		gtk_window_unmaximize (GTK_WINDOW (window));
	}

	geometry_string = g_settings_get_string
		(nautilus_window_state, NAUTILUS_WINDOW_STATE_GEOMETRY);
	if (geometry_string != NULL &&
	    geometry_string[0] != 0) {
		/* Ignore saved window position if a window with the same
		 * location is already showing. That way the two windows
		 * wont appear at the exact same location on the screen.
		 */
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), 
			 geometry_string,
			 NAUTILUS_NAVIGATION_WINDOW_MIN_WIDTH,
			 NAUTILUS_NAVIGATION_WINDOW_MIN_HEIGHT,
			 another_navigation_window_already_showing (application, window));
	}
	g_free (geometry_string);

	DEBUG ("Creating a new navigation window");
	
	return window;
}

/* callback for showing or hiding the desktop based on the user's preference */
static void
desktop_changed_callback (gpointer user_data)
{
	NautilusApplication *application;

	application = NAUTILUS_APPLICATION (user_data);
	if (g_settings_get_boolean (gnome_background_preferences, NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
		nautilus_application_open_desktop (application);
	} else {
		nautilus_application_close_desktop ();
	}
}

static gboolean
window_can_be_closed (NautilusWindow *window)
{
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		return TRUE;
	}
	
	return FALSE;
}

static void
mount_added_callback (GVolumeMonitor *monitor,
		      GMount *mount,
		      NautilusApplication *application)
{
	NautilusDirectory *directory;
	GFile *root;
	gchar *uri;

	if (!application->priv->session_is_active) {
		return;
	}
		
	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	DEBUG ("Added mount at uri %s", uri);
	g_free (uri);
	
	directory = nautilus_directory_get_existing (root);
	g_object_unref (root);
	if (directory != NULL) {
		nautilus_directory_force_reload (directory);
		nautilus_directory_unref (directory);
	}
}

static NautilusWindowSlot *
get_first_navigation_slot (GList *slot_list)
{
	GList *l;

	for (l = slot_list; l != NULL; l = l->next) {
		if (NAUTILUS_IS_NAVIGATION_WINDOW_SLOT (l->data)) {
			return l->data;
		}
	}

	return NULL;
}

/* We redirect some slots and close others */
static gboolean
should_close_slot_with_mount (NautilusWindow *window,
			      NautilusWindowSlot *slot,
			      GMount *mount)
{
	if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
		return TRUE;
	}
	return nautilus_navigation_window_slot_should_close_with_mount (NAUTILUS_NAVIGATION_WINDOW_SLOT (slot),
									mount);
}

/* Called whenever a mount is unmounted. Check and see if there are
 * any windows open displaying contents on the mount. If there are,
 * close them.  It would also be cool to save open window and position
 * info.
 */
static void
mount_removed_callback (GVolumeMonitor *monitor,
			GMount *mount,
			NautilusApplication *application)
{
	GList *window_list, *node, *close_list;
	NautilusWindow *window;
	NautilusWindowSlot *slot;
	NautilusWindowSlot *force_no_close_slot;
	GFile *root, *computer;
	gboolean unclosed_slot;
	gchar *uri;

	close_list = NULL;
	force_no_close_slot = NULL;
	unclosed_slot = FALSE;

	/* Check and see if any of the open windows are displaying contents from the unmounted mount */
	window_list = gtk_application_get_windows (GTK_APPLICATION (application));

	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	DEBUG ("Removed mount at uri %s", uri);
	g_free (uri);

	/* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
	for (node = window_list; node != NULL; node = node->next) {
		window = NAUTILUS_WINDOW (node->data);
		if (window != NULL && window_can_be_closed (window)) {
			GList *l;
			GList *lp;
			GFile *location;

			for (lp = window->details->panes; lp != NULL; lp = lp->next) {
				NautilusWindowPane *pane;
				pane = (NautilusWindowPane*) lp->data;
				for (l = pane->slots; l != NULL; l = l->next) {
					slot = l->data;
					location = slot->location;
					if (location == NULL ||
					    g_file_has_prefix (location, root) ||
					    g_file_equal (location, root)) {
						close_list = g_list_prepend (close_list, slot);

						if (!should_close_slot_with_mount (window, slot, mount)) {
							/* We'll be redirecting this, not closing */
							unclosed_slot = TRUE;
						}
					} else {
						unclosed_slot = TRUE;
					}
				} /* for all slots */
			} /* for all panes */
		}
	}

	if (nautilus_application_desktop_windows == NULL &&
	    !unclosed_slot) {
		/* We are trying to close all open slots. Keep one navigation slot open. */
		force_no_close_slot = get_first_navigation_slot (close_list);
	}

	/* Handle the windows in the close list. */
	for (node = close_list; node != NULL; node = node->next) {
		slot = node->data;
		window = slot->pane->window;

		if (should_close_slot_with_mount (window, slot, mount) &&
		    slot != force_no_close_slot) {
			nautilus_window_slot_close (slot);
		} else {
			computer = g_file_new_for_uri ("computer:///");
			nautilus_window_slot_go_to (slot, computer, FALSE);
			g_object_unref(computer);
		}
	}

	g_list_free (close_list);
}

static GObject *
nautilus_application_constructor (GType type,
				  guint n_construct_params,
				  GObjectConstructParam *construct_params)
{
        GObject *retval;

        if (singleton != NULL) {
                return g_object_ref (singleton);
        }

        retval = G_OBJECT_CLASS (nautilus_application_parent_class)->constructor
                (type, n_construct_params, construct_params);

        singleton = NAUTILUS_APPLICATION (retval);
        g_object_add_weak_pointer (retval, (gpointer) &singleton);

        return retval;
}

static void
nautilus_application_init (NautilusApplication *application)
{
	application->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (application, NAUTILUS_TYPE_APPLICATION,
					     NautilusApplicationPriv);
}

static void
nautilus_application_finalize (GObject *object)
{
	NautilusApplication *application;

	application = NAUTILUS_APPLICATION (object);

	nautilus_bookmarks_exiting ();

	g_clear_object (&application->undo_manager);
	g_clear_object (&application->priv->volume_monitor);
	g_clear_object (&application->priv->ck_proxy);
	g_clear_object (&application->priv->progress_handler);

	nautilus_dbus_manager_stop ();
	notify_uninit ();

        G_OBJECT_CLASS (nautilus_application_parent_class)->finalize (object);
}

void
nautilus_application_quit (NautilusApplication *self)
{
	GApplication *app = G_APPLICATION (self);
	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
}

static gint
nautilus_application_command_line (GApplication *app,
				   GApplicationCommandLine *command_line)
{
	gboolean perform_self_check = FALSE;
	gboolean version = FALSE;
	gboolean no_default_window = FALSE;
	gboolean no_desktop = FALSE;
	gboolean browser_window = FALSE;
	gboolean kill_shell = FALSE;
	gboolean autostart_mode = FALSE;
	const gchar *autostart_id;
	gchar *geometry = NULL;
	gchar **remaining = NULL;
	const GOptionEntry options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		{ "check", 'c', 0, G_OPTION_ARG_NONE, &perform_self_check, 
		  N_("Perform a quick set of self-check tests."), NULL },
#endif
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
		  N_("Show the version of the program."), NULL },
		{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
		  N_("Create the initial window with the given geometry."), N_("GEOMETRY") },
		{ "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
		  N_("Only create windows for explicitly specified URIs."), NULL },
		{ "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop,
		  N_("Do not manage the desktop (ignore the preference set in the preferences dialog)."), NULL },
		{ "browser", '\0', 0, G_OPTION_ARG_NONE, &browser_window, 
		  N_("Open a browser window."), NULL },
		{ "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
		  N_("Quit Nautilus."), NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining, NULL,  N_("[URI...]") },

		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	NautilusApplication *self = NAUTILUS_APPLICATION (app);
	gint argc = 0;
	gchar **argv = NULL, **uris = NULL;
	gint retval = EXIT_SUCCESS;

	context = g_option_context_new (_("\n\nBrowse the file system with the file manager"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	argv = g_application_command_line_get_arguments (command_line, &argc);

	autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
	if (autostart_id != NULL && *autostart_id != '\0') {
		autostart_mode = TRUE;
        }

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("Could not parse arguments: %s\n", error->message);
		g_error_free (error);

		retval = EXIT_FAILURE;
		goto out;
	}

	if (version) {
		g_application_command_line_print (command_line, "GNOME nautilus " PACKAGE_VERSION "\n");
		goto out;
	}
	if (perform_self_check && (remaining != NULL || kill_shell)) {
		g_application_command_line_printerr (command_line, "%s\n",
						     _("--check cannot be used with other options."));
		retval = EXIT_FAILURE;
		goto out;
	}
	if (kill_shell && remaining != NULL) {
		g_application_command_line_printerr (command_line, "%s\n",
						     _("--quit cannot be used with URIs."));
		retval = EXIT_FAILURE;
		goto out;
	}
	if (geometry != NULL &&
	    remaining != NULL && remaining[0] != NULL && remaining[1] != NULL) {
		g_application_command_line_printerr (command_line, "%s\n",
						     _("--geometry cannot be used with more than one URI."));
		retval = EXIT_FAILURE;
		goto out;
	}

	/* Do either the self-check or the real work. */
	if (perform_self_check) {
#ifndef NAUTILUS_OMIT_SELF_CHECK
		/* Run the checks (each twice) for nautilus and libnautilus-private. */

		nautilus_run_self_checks ();
		nautilus_run_lib_self_checks ();
		eel_exit_if_self_checks_failed ();

		nautilus_run_self_checks ();
		nautilus_run_lib_self_checks ();
		eel_exit_if_self_checks_failed ();

		retval = EXIT_SUCCESS;
		goto out;
#endif
	}

	/* Check the user's ~/.nautilus directories and post warnings
	 * if there are problems.
	 */
	if (!kill_shell && !check_required_directories (self)) {
		retval = EXIT_FAILURE;
		goto out;
	}

	/* If in autostart mode (aka started by gnome-session), we need to ensure 
         * nautilus starts with the correct options.
         */
	if (autostart_mode) {
		no_default_window = TRUE;
		no_desktop = FALSE;
	}

	DEBUG ("Parsing command line, no_default_window %d browser %d, quit %d, "
	       "self checks %d, no_desktop %d",
	       no_default_window, browser_window, kill_shell, perform_self_check, no_desktop);

	if (kill_shell) {
		nautilus_application_quit (self);
	} else {
		if (!self->priv->initialized) {
			char *accel_map_filename;

			if (!no_desktop &&
			    !g_settings_get_boolean (gnome_background_preferences,
						     NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
				no_desktop = TRUE;
			}

			if (!no_desktop) {
				nautilus_application_open_desktop (self);
			}

			finish_startup (self, no_desktop);

			/* Monitor the preference to show or hide the desktop */
			g_signal_connect_swapped (gnome_background_preferences, "changed::" NAUTILUS_PREFERENCES_SHOW_DESKTOP,
						  G_CALLBACK (desktop_changed_callback),
						  self);

			/* load accelerator map, and register save callback */
			accel_map_filename = nautilus_get_accel_map_file ();
			if (accel_map_filename) {
				gtk_accel_map_load (accel_map_filename);
				g_free (accel_map_filename);
			}

			g_signal_connect (gtk_accel_map_get (), "changed",
					  G_CALLBACK (queue_accel_map_save_callback), NULL);

			self->priv->initialized = TRUE;
		}

		/* Convert args to URIs */
		if (remaining != NULL) {
			GFile *file;
			GPtrArray *uris_array;
			gint i;
			gchar *uri;

			uris_array = g_ptr_array_new ();

			for (i = 0; remaining[i] != NULL; i++) {
				file = g_file_new_for_commandline_arg (remaining[i]);
				if (file != NULL) {
					uri = g_file_get_uri (file);
					g_object_unref (file);
					if (uri) {
						g_ptr_array_add (uris_array, uri);
					}
				}
			}

			g_ptr_array_add (uris_array, NULL);
			uris = (char **) g_ptr_array_free (uris_array, FALSE);
			g_strfreev (remaining);
		}

		/* Create the other windows. */
		if (uris != NULL || !no_default_window) {
			open_windows (self, NULL,
				      uris,
				      gdk_screen_get_default (),
				      geometry,
				      browser_window);
		}
	}

 out:
	g_option_context_free (context);
	g_strfreev (argv);

	return retval;
}

static void
init_css (void)
{
	GtkCssProvider *provider;
	GError *error = NULL;

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_path (provider,
					 NAUTILUS_DATADIR G_DIR_SEPARATOR_S "nautilus.css", &error);

	if (error != NULL) {
		g_warning ("Can't parse Nautilus' CSS custom description: %s\n", error->message);
		g_error_free (error);
	} else {
		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
							   GTK_STYLE_PROVIDER (provider),
							   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	g_object_unref (provider);
}

static void
nautilus_application_startup (GApplication *app)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (app);

	/* chain up to the GTK+ implementation early, so gtk_init()
	 * is called for us.
	 */
	G_APPLICATION_CLASS (nautilus_application_parent_class)->startup (app);

	DEBUG ("Application startup");

	/* create an undo manager */
	self->undo_manager = nautilus_undo_manager_new ();

	/* Initialize preferences. This is needed to create the
	 * global GSettings objects.
	 */
	nautilus_global_preferences_init ();

	/* register views */
	nautilus_icon_view_register ();
	nautilus_desktop_icon_view_register ();
	nautilus_list_view_register ();
	nautilus_icon_view_compact_register ();
#if ENABLE_EMPTY_VIEW
	nautilus_empty_view_register ();
#endif /* ENABLE_EMPTY_VIEW */

	/* register property pages */
	nautilus_image_properties_page_register ();

	/* initialize CSS theming */
	init_css ();

	/* initialize search path for custom icons */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   NAUTILUS_DATADIR G_DIR_SEPARATOR_S "icons");

	nautilus_dbus_manager_start ();
}

static void
nautilus_application_quit_mainloop (GApplication *app)
{
	DEBUG ("Quitting mainloop");

	nautilus_icon_info_clear_caches ();
 	nautilus_application_save_accel_map (NULL);

	G_APPLICATION_CLASS (nautilus_application_parent_class)->quit_mainloop (app);
}

static void
nautilus_application_class_init (NautilusApplicationClass *class)
{
        GObjectClass *object_class;
	GApplicationClass *application_class;

        object_class = G_OBJECT_CLASS (class);
	object_class->constructor = nautilus_application_constructor;
        object_class->finalize = nautilus_application_finalize;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = nautilus_application_startup;
	application_class->command_line = nautilus_application_command_line;
	application_class->quit_mainloop = nautilus_application_quit_mainloop;

	g_type_class_add_private (class, sizeof (NautilusApplication));
}

NautilusApplication *
nautilus_application_dup_singleton (void)
{
	return g_object_new (NAUTILUS_TYPE_APPLICATION,
			     "application-id", "org.gnome.NautilusApplication",
			     "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
			     NULL);
}

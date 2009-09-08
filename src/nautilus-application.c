/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>,
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#include <config.h>
#include "nautilus-application.h"

#include "file-manager/fm-desktop-icon-view.h"
#include "file-manager/fm-icon-view.h"
#include "file-manager/fm-list-view.h"
#include "file-manager/fm-tree-view.h"
#if ENABLE_EMPTY_VIEW
#include "file-manager/fm-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */
#include "nautilus-information-panel.h"
#include "nautilus-history-sidebar.h"
#include "nautilus-places-sidebar.h"
#include "nautilus-notes-viewer.h"
#include "nautilus-emblem-sidebar.h"
#include "nautilus-image-properties-page.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "nautilus-desktop-window.h"
#include "nautilus-main.h"
#include "nautilus-spatial-window.h"
#include "nautilus-navigation-window.h"
#include "nautilus-window-slot.h"
#include "nautilus-navigation-window-slot.h"
#include "nautilus-window-bookmarks.h"
#include "libnautilus-private/nautilus-file-operations.h"
#include "nautilus-window-private.h"
#include "nautilus-window-manage-views.h"
#include <unistd.h>
#include <libxml/xmlsave.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <dbus/dbus-glib.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-debug-log.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus-private/nautilus-desktop-link-monitor.h>
#include <libnautilus-private/nautilus-directory-private.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-autorun.h>

enum
{
  COMMAND_0, /* unused: 0 is an invalid command */

  COMMAND_START_DESKTOP,
  COMMAND_STOP_DESKTOP,
  COMMAND_OPEN_BROWSER,
};

/* Needed for the is_kdesktop_present check */
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define START_STATE_CONFIG "start-state"

#define NAUTILUS_ACCEL_MAP_SAVE_DELAY 30

/* Keeps track of all the desktop windows. */
static GList *nautilus_application_desktop_windows;

/* Keeps track of all the nautilus windows. */
static GList *nautilus_application_window_list;

/* Keeps track of all the object windows */
static GList *nautilus_application_spatial_window_list;

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

static void     desktop_changed_callback          (gpointer                  user_data);
static void     desktop_location_changed_callback (gpointer                  user_data);
static void     mount_removed_callback            (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NautilusApplication       *application);
static void     mount_added_callback              (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NautilusApplication       *application);
static void     volume_added_callback              (GVolumeMonitor           *monitor,
						    GVolume                  *volume,
						    NautilusApplication      *application);
static void     drive_connected_callback           (GVolumeMonitor           *monitor,
						    GDrive                   *drive,
						    NautilusApplication      *application);
static void     drive_listen_for_eject_button      (GDrive *drive, 
						    NautilusApplication *application);
static gboolean is_kdesktop_present               (void);
static void     nautilus_application_load_session     (NautilusApplication *application); 
static char *   nautilus_application_get_session_data (void);
static void     ck_session_active_changed_cb (DBusGProxy *proxy,
		                              gboolean is_active,
                		              void *user_data);


G_DEFINE_TYPE (NautilusApplication, nautilus_application, G_TYPE_OBJECT);

static gboolean
_unique_message_data_set_geometry_and_uris (UniqueMessageData  *message_data,
					    const char *geometry,
					    char **uris)
{
  GString *list;
  gint i;
  gchar *result;
  gsize length;

  list = g_string_new (NULL);
  if (geometry != NULL) {
	  g_string_append (list, geometry);
  }
  g_string_append (list, "\r\n");
  
  for (i = 0; uris != NULL && uris[i]; i++) {
	  g_string_append (list, uris[i]);
	  g_string_append (list, "\r\n");
  }

  result = g_convert (list->str, list->len,
                      "ASCII", "UTF-8",
                      NULL, &length, NULL);
  g_string_free (list, TRUE);
  
  if (result) {
	  unique_message_data_set (message_data, (guchar *) result, length);
	  g_free (result);
	  return TRUE;
  }
  
  return FALSE;
}

static gchar **
_unique_message_data_get_geometry_and_uris (UniqueMessageData *message_data,
					    char **geometry)
{
  gchar **result = NULL;

  *geometry = NULL;
  
  gchar *text, *newline, *uris;
  text = unique_message_data_get_text (message_data);
  if (text) {
	  newline = strchr (text, '\n');
	  if (newline) {
		  *geometry = g_strndup (text, newline-text);
		  uris = newline+1;
	  } else {
		  uris = text;
	  }
	  
	  result = g_uri_list_extract_uris (uris);
	  g_free (text);
  }

  /* if the string is empty, make it NULL */
  if (*geometry && strlen (*geometry) == 0) {
	  g_free (*geometry);
	  *geometry = NULL;
  }

  return result;
}

GList *
nautilus_application_get_window_list (void)
{
	return nautilus_application_window_list;
}

GList *
nautilus_application_get_spatial_window_list (void)
{
	return nautilus_application_spatial_window_list;
}

unsigned int
nautilus_application_get_n_windows (void)
{
	return g_list_length (nautilus_application_window_list) +
	       g_list_length (nautilus_application_desktop_windows);
}

static void
startup_volume_mount_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	g_volume_mount_finish (G_VOLUME (source_object), res, NULL);
}

static void
automount_all_volumes (NautilusApplication *application)
{
	GList *volumes, *l;
	GMount *mount;
	GVolume *volume;
		
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT)) {
		/* automount all mountable volumes at start-up */
		volumes = g_volume_monitor_get_volumes (application->volume_monitor);
		for (l = volumes; l != NULL; l = l->next) {
			volume = l->data;
			
			if (!g_volume_should_automount (volume) ||
			    !g_volume_can_mount (volume)) {
				continue;
			}
			
			mount = g_volume_get_mount (volume);
			if (mount != NULL) {
				g_object_unref (mount);
				continue;
			}

			/* pass NULL as GMountOperation to avoid user interaction */
			g_volume_mount (volume, 0, NULL, NULL, startup_volume_mount_cb, NULL);
		}
		eel_g_object_list_free (volumes);
	}
	
}

static void
smclient_save_state_cb (EggSMClient   *client,
			GKeyFile      *state_file,
			NautilusApplication *application)
{
	char *data;

	data = nautilus_application_get_session_data ();
	if (data) {
		g_key_file_set_string (state_file,
				       "Nautilus",
				       "documents", 
				       data);
	}
	g_free (data);
}

static void
smclient_quit_cb (EggSMClient   *client,
		  NautilusApplication *application)
{
	nautilus_main_event_loop_quit (TRUE);
}

static void
nautilus_application_init (NautilusApplication *application)
{
	/* Create an undo manager */
	application->undo_manager = nautilus_undo_manager_new ();

	application->unique_app = unique_app_new_with_commands ("org.gnome.Nautilus", NULL,
								"start_desktop", COMMAND_START_DESKTOP,
								"stop_desktop", COMMAND_STOP_DESKTOP,
								"open_browser", COMMAND_OPEN_BROWSER,
								NULL);

	
        application->smclient = egg_sm_client_get ();
        g_signal_connect (application->smclient, "save_state",
                          G_CALLBACK (smclient_save_state_cb),
                          application);
	g_signal_connect (application->smclient, "quit",
			  G_CALLBACK (smclient_quit_cb),
			  application);
	/* TODO: Should connect to quit_requested and block logout on active transfer? */
	
	/* register views */
	fm_icon_view_register ();
	fm_desktop_icon_view_register ();
	fm_list_view_register ();
	fm_compact_view_register ();
#if ENABLE_EMPTY_VIEW
	fm_empty_view_register ();
#endif /* ENABLE_EMPTY_VIEW */

	/* register sidebars */
	nautilus_places_sidebar_register ();
	nautilus_information_panel_register ();
	fm_tree_view_register ();
	nautilus_history_sidebar_register ();
	nautilus_notes_viewer_register (); /* also property page */
	nautilus_emblem_sidebar_register ();

	/* register property pages */
	nautilus_image_properties_page_register ();

	/* initialize search path for custom icons */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   NAUTILUS_DATADIR G_DIR_SEPARATOR_S "icons");
}

NautilusApplication *
nautilus_application_new (void)
{
	return g_object_new (NAUTILUS_TYPE_APPLICATION, NULL);
}

static void
nautilus_application_finalize (GObject *object)
{
	NautilusApplication *application;

	application = NAUTILUS_APPLICATION (object);

	nautilus_bookmarks_exiting ();
	
	g_object_unref (application->undo_manager);

	if (application->volume_monitor) {
		g_object_unref (application->volume_monitor);
		application->volume_monitor = NULL;
	}

	g_object_unref (application->unique_app);

	if (application->automount_idle_id != 0) {
		g_source_remove (application->automount_idle_id);
		application->automount_idle_id = 0;
	}

	if (application->ck_session_proxy != NULL) {
		dbus_g_proxy_disconnect_signal (application->ck_session_proxy, "ActiveChanged",
						G_CALLBACK (ck_session_active_changed_cb), NULL);
		g_object_unref (application->ck_session_proxy);
		application->ck_session_proxy = NULL;
	}

        G_OBJECT_CLASS (nautilus_application_parent_class)->finalize (object);
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
		nautilus_main_event_loop_register (GTK_OBJECT (dialog));

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
        GList *items;
        GList *providers;
        GList *l;

        providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
        items = NULL;

        for (l = providers; l != NULL; l = l->next) {
                NautilusMenuProvider *provider = NAUTILUS_MENU_PROVIDER (l->data);

		g_signal_connect_after (G_OBJECT (provider), "items_updated",
                           (GCallback)menu_provider_items_updated_handler,
                           NULL);
        }

        nautilus_module_extension_list_free (providers);
}

static gboolean
automount_all_volumes_idle_cb (gpointer data)
{
	NautilusApplication *application = NAUTILUS_APPLICATION (data);

	automount_all_volumes (application);

	application->automount_idle_id = 0;
	return FALSE;
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

#define CK_NAME "org.freedesktop.ConsoleKit"
#define CK_PATH "/org/freedesktop/ConsoleKit"

static void
ck_session_active_changed_cb (DBusGProxy *proxy,
			      gboolean is_active,
			      void *user_data)
{
	NautilusApplication *application = user_data;

	application->session_is_active = is_active;
}

static void
ck_call_is_active_cb (DBusGProxy *proxy,
		      DBusGProxyCall *call_id,
		      void *user_data)
{
	gboolean res, is_active;
	NautilusApplication *application;

	application = user_data;

	res = dbus_g_proxy_end_call (proxy, call_id, NULL,
				     G_TYPE_BOOLEAN, &is_active,
				     G_TYPE_INVALID);
	if (!res) {
		g_object_unref (proxy);

		application->session_is_active = TRUE;
		return;
	}

	application->session_is_active = is_active;

	dbus_g_proxy_add_signal (proxy, "ActiveChanged", G_TYPE_BOOLEAN, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy, "ActiveChanged",
				     G_CALLBACK (ck_session_active_changed_cb), application,
				     NULL);
}

static void
ck_get_current_session_cb (DBusGProxy *proxy,
			   DBusGProxyCall *call_id,
			   void *user_data)
{
	gboolean res;
	char *session_id;
	NautilusApplication *application;

	application = user_data;

	res = dbus_g_proxy_end_call (proxy, call_id, NULL,
				     DBUS_TYPE_G_OBJECT_PATH, &session_id, G_TYPE_INVALID);
	if (!res) {
		g_object_unref (proxy);

		application->session_is_active = TRUE;
		return;
	}

	application->ck_session_proxy = dbus_g_proxy_new_from_proxy (proxy, CK_NAME ".Session",
								     session_id);
	dbus_g_proxy_begin_call (application->ck_session_proxy, "IsActive", ck_call_is_active_cb,
				 application, NULL, G_TYPE_INVALID);

	g_free (session_id);
	g_object_unref (proxy);
}


static void
do_initialize_consolekit (NautilusApplication *application)
{
	DBusGConnection *conn;
	DBusGProxy *proxy;
	GError *error = NULL;

	conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (error) {
		g_error_free (error);

		application->session_is_active = TRUE;

		return;
	}

	proxy = dbus_g_proxy_new_for_name (conn, CK_NAME, CK_PATH "/Manager",
 					   CK_NAME ".Manager");
	dbus_g_proxy_begin_call (proxy, "GetCurrentSession",
				 ck_get_current_session_cb, application,
				 NULL, G_TYPE_INVALID);
}

static void
do_upgrades_once (NautilusApplication *application,
		  gboolean no_desktop)
{
	char *metafile_dir, *updated;
	int fd;

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
}

static void
finish_startup (NautilusApplication *application,
		gboolean no_desktop)
{
	GList *drives;

	do_upgrades_once (application, no_desktop);
	
	/* initialize nautilus modules */
	nautilus_module_setup ();

	/* attach menu-provider module callback */
	menu_provider_init_callback ();
	
	/* Initialize the desktop link monitor singleton */
	nautilus_desktop_link_monitor_get ();

	/* Initialize the ConsoleKit listener for active session */
	do_initialize_consolekit (application);

	/* Watch for mounts so we can restore open windows This used
	 * to be for showing new window on mount, but is not used
	 * anymore */

	/* Watch for unmounts so we can close open windows */
	/* TODO-gio: This should be using the UNMOUNTED feature of GFileMonitor instead */
	application->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (application->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), application, 0);
	g_signal_connect_object (application->volume_monitor, "mount_pre_unmount",
				 G_CALLBACK (mount_removed_callback), application, 0);
	g_signal_connect_object (application->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), application, 0);
	g_signal_connect_object (application->volume_monitor, "volume_added",
				 G_CALLBACK (volume_added_callback), application, 0);
	g_signal_connect_object (application->volume_monitor, "drive_connected",
				 G_CALLBACK (drive_connected_callback), application, 0);

	/* listen for eject button presses */
	drives = g_volume_monitor_get_connected_drives (application->volume_monitor);
	g_list_foreach (drives, (GFunc) drive_listen_for_eject_button, application);
	g_list_foreach (drives, (GFunc) g_object_unref, NULL);
	g_list_free (drives);

	application->automount_idle_id = 
		g_idle_add_full (G_PRIORITY_LOW,
				 automount_all_volumes_idle_cb,
				 application, NULL);
}

static void
open_window (NautilusApplication *application,
	     const char *startup_id,
	     const char *uri, GdkScreen *screen, const char *geometry,
	     gboolean browser_window)
{
	GFile *location;
	NautilusWindow *window;

	if (browser_window ||
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
		window = nautilus_application_create_navigation_window (application,
									startup_id,
									screen);
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
		
		window = nautilus_application_present_spatial_window (application,
								      NULL,
								      startup_id,
								      location,
								      screen);
		g_object_unref (location);
	}
	
	if (geometry != NULL && !GTK_WIDGET_VISIBLE (window)) {
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

static UniqueResponse
message_received_cb (UniqueApp         *unique_app,
                     UniqueCommand      command,
                     UniqueMessageData *message,
                     guint              time_,
                     gpointer           user_data)
{
	NautilusApplication *application;
	UniqueResponse res;
	char **uris;
	char *geometry;
	GdkScreen *screen;
	
	application =  user_data;
	res = UNIQUE_RESPONSE_OK;
	
	switch (command) {
	case UNIQUE_CLOSE:
		res = UNIQUE_RESPONSE_OK;
		nautilus_main_event_loop_quit (TRUE);
		
		break;
	case UNIQUE_OPEN:
	case COMMAND_OPEN_BROWSER:
		uris = _unique_message_data_get_geometry_and_uris (message, &geometry);
		screen = unique_message_data_get_screen (message);
		open_windows (application,
			      unique_message_data_get_startup_id (message),
			      uris,
			      screen,
			      geometry,
			      command == COMMAND_OPEN_BROWSER);
		g_strfreev (uris);
		g_free (geometry);
		break;
	case COMMAND_START_DESKTOP:
		nautilus_application_open_desktop (application);
		break;
	case COMMAND_STOP_DESKTOP:
		nautilus_application_close_desktop ();
		break;
	default:
		res = UNIQUE_RESPONSE_PASSTHROUGH;
		break;
	}
	
	return res;
}

gboolean 
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

void
nautilus_application_startup (NautilusApplication *application,
			      gboolean kill_shell,
			      gboolean no_default_window,
			      gboolean no_desktop,
			      gboolean browser_window,
			      const char *geometry,
			      char **urls)
{
	UniqueMessageData *message;
	
	/* Check the user's ~/.nautilus directories and post warnings
	 * if there are problems.
	 */
	if (!kill_shell && !check_required_directories (application)) {
		return;
	}

	if (kill_shell) {
		if (unique_app_is_running (application->unique_app)) {
			unique_app_send_message (application->unique_app,
						 UNIQUE_CLOSE, NULL);
			
		}
	} else {
		char *accel_map_filename;

		/* If KDE desktop is running, then force no_desktop */
		if (is_kdesktop_present ()) {
			no_desktop = TRUE;
		}
		
		if (!no_desktop &&
		    !eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
			no_desktop = TRUE;
		}
			
		if (!no_desktop) {
			if (unique_app_is_running (application->unique_app)) {
				unique_app_send_message (application->unique_app,
							 COMMAND_START_DESKTOP, NULL);
			} else {
				nautilus_application_open_desktop (application);
			}
		}

		if (!unique_app_is_running (application->unique_app)) {
			finish_startup (application, no_desktop);
			g_signal_connect (application->unique_app, "message-received", G_CALLBACK (message_received_cb), application);			
		}
		
		/* Monitor the preference to show or hide the desktop */
		eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_SHOW_DESKTOP,
							  desktop_changed_callback,
							  application,
							  G_OBJECT (application));

		/* Monitor the preference to have the desktop */
		/* point to the Unix home folder */
		eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
							  desktop_location_changed_callback,
							  NULL,
							  G_OBJECT (application));

	  	/* Create the other windows. */
		if (urls != NULL || !no_default_window) {
			if (unique_app_is_running (application->unique_app)) {
				message = unique_message_data_new ();
				_unique_message_data_set_geometry_and_uris (message, geometry, urls);
				if (browser_window) {
					unique_app_send_message (application->unique_app,
								 COMMAND_OPEN_BROWSER, message);
				} else {
					unique_app_send_message (application->unique_app,
								 UNIQUE_OPEN, message);
				}
				unique_message_data_free (message);				
			} else {
				open_windows (application, NULL,
					      urls,
					      gdk_screen_get_default (),
					      geometry,
					      browser_window);
			}
		}

		/* Load session info if availible */
		nautilus_application_load_session (application);
		
		/* load accelerator map, and register save callback */
		accel_map_filename = nautilus_get_accel_map_file ();
		if (accel_map_filename) {
			gtk_accel_map_load (accel_map_filename);
			g_free (accel_map_filename);
		}
		g_signal_connect (gtk_accel_map_get (), "changed", G_CALLBACK (queue_accel_map_save_callback), NULL);
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
						 gdk_x11_get_server_time (selection_widget->window))) {
		
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
	static gboolean create_in_progress = FALSE;
	GdkDisplay *display;
	NautilusDesktopWindow *window;
	GtkWidget *selection_widget;
	int screens, i;

	g_return_if_fail (nautilus_application_desktop_windows == NULL);
	g_return_if_fail (NAUTILUS_IS_APPLICATION (application));

	if (create_in_progress) {
		return;
	}

	create_in_progress = TRUE;

	display = gdk_display_get_default ();
	screens = gdk_display_get_n_screens (display);

	for (i = 0; i < screens; i++) {
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
		}
	}

	create_in_progress = FALSE;
}

void
nautilus_application_open_desktop (NautilusApplication *application)
{
	if (nautilus_application_desktop_windows == NULL) {
		nautilus_application_create_desktop_windows (application);
	}
}

void
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
nautilus_application_close_all_navigation_windows (void)
{
	GList *list_copy;
	GList *l;
	
	list_copy = g_list_copy (nautilus_application_window_list);
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

	for (l = nautilus_application_get_spatial_window_list ();
	     l != NULL; l = l->next) {
		GFile *window_location;

		slot = NAUTILUS_WINDOW (l->data)->details->active_slot;
		window_location = slot->location;
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

	slot = NAUTILUS_WINDOW (window)->details->active_slot;

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
	for (l = list_copy; l != NULL; l = l->next) {
		NautilusWindow *window;
		
		window = NAUTILUS_WINDOW (l->data);
		
		if (NAUTILUS_IS_SPATIAL_WINDOW (window)) {
			nautilus_window_close (window);
		}
	}
	g_list_free (list_copy);
}

static void
nautilus_application_destroyed_window (GtkObject *object, NautilusApplication *application)
{
	nautilus_application_window_list = g_list_remove (nautilus_application_window_list, object);
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
	/* Must be called after construction finished */
	nautilus_window_constructed (window);

	if (startup_id) {
		gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
	}
	
	g_signal_connect_data (window, "delete_event",
			       G_CALLBACK (nautilus_window_delete_event_callback), NULL, NULL,
			       G_CONNECT_AFTER);

	g_signal_connect_object (window, "destroy",
				 G_CALLBACK (nautilus_application_destroyed_window), application, 0);

	nautilus_application_window_list = g_list_prepend (nautilus_application_window_list, window);

	/* Do not yet show the window. It will be shown later on if it can
	 * successfully display its initial URI. Otherwise it will be destroyed
	 * without ever having seen the light of day.
	 */

	return window;
}

static void
spatial_window_destroyed_callback (void *user_data, GObject *window)
{
	nautilus_application_spatial_window_list = g_list_remove (nautilus_application_spatial_window_list, window);
		
}

NautilusWindow *
nautilus_application_present_spatial_window (NautilusApplication *application,
					     NautilusWindow      *requesting_window,
					     const char          *startup_id,
					     GFile               *location,
					     GdkScreen           *screen)
{
	return nautilus_application_present_spatial_window_with_selection (application,
									   requesting_window,
									   startup_id,
									   location,
									   NULL,
									   screen);
}

NautilusWindow *
nautilus_application_present_spatial_window_with_selection (NautilusApplication *application,
							    NautilusWindow      *requesting_window,
							    const char          *startup_id,
							    GFile               *location,
							    GList		*new_selection,
							    GdkScreen           *screen)
{
	NautilusWindow *window;
	GList *l;
	char *uri;

	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);
	
	for (l = nautilus_application_get_spatial_window_list ();
	     l != NULL; l = l->next) {
		NautilusWindow *existing_window;
		NautilusWindowSlot *slot;
		GFile *existing_location;

		existing_window = NAUTILUS_WINDOW (l->data);
		slot = existing_window->details->active_slot;
		existing_location = slot->pending_location;
		
		if (existing_location == NULL) {
			existing_location = slot->location;
		}
		
		if (g_file_equal (existing_location, location)) {
			gtk_window_present (GTK_WINDOW (existing_window));
			if (new_selection &&
			    slot->content_view != NULL) {
				nautilus_view_set_selection (slot->content_view, new_selection);
			}

			uri = g_file_get_uri (location);
			nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
					    "present EXISTING spatial window=%p: %s",
					    existing_window, uri);
			g_free (uri);
			return existing_window;
		}
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
	
	nautilus_window_go_to_with_selection (window, location, new_selection);

	uri = g_file_get_uri (location);
	nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "present NEW spatial window=%p: %s",
			    window, uri);
	g_free (uri);
	
	return window;
}

static gboolean
another_navigation_window_already_showing (NautilusWindow *the_window)
{
	GList *list, *item;
	
	list = nautilus_application_get_window_list ();
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

	maximized = eel_preferences_get_boolean
			(NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED);
	if (maximized) {
		gtk_window_maximize (GTK_WINDOW (window));
	} else {
		gtk_window_unmaximize (GTK_WINDOW (window));
	}

	geometry_string = eel_preferences_get
			(NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY);
	if (geometry_string != NULL &&
	    geometry_string[0] != 0) {
		/* Ignore saved window position if a window with the same
		 * location is already showing. That way the two windows
		 * wont appear at the exact same location on the screen.
		 */
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), 
			 geometry_string,
			 NAUTILUS_WINDOW_MIN_WIDTH, 
			 NAUTILUS_WINDOW_MIN_HEIGHT,
			 another_navigation_window_already_showing (window));
	}
	g_free (geometry_string);

	nautilus_debug_log (FALSE, NAUTILUS_DEBUG_LOG_DOMAIN_USER,
			    "create new navigation window=%p",
			    window);

	return window;
}

/* callback for changing the directory the desktop points to */
static void
desktop_location_changed_callback (gpointer user_data)
{
	if (nautilus_application_desktop_windows != NULL) {
		g_list_foreach (nautilus_application_desktop_windows,
				(GFunc) nautilus_desktop_window_update_directory, NULL);
	}
}

/* callback for showing or hiding the desktop based on the user's preference */
static void
desktop_changed_callback (gpointer user_data)
{
	NautilusApplication *application;
	
	application = NAUTILUS_APPLICATION (user_data);
	if ( eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_DESKTOP)) {
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
volume_added_callback (GVolumeMonitor *monitor,
		       GVolume *volume,
		       NautilusApplication *application)
{
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT) &&
	    g_volume_should_automount (volume) &&
	    g_volume_can_mount (volume)) {
		nautilus_file_operations_mount_volume (NULL, volume, TRUE);
	} else {
		/* Allow nautilus_autorun() to run. When the mount is later
		 * added programmatically (i.e. for a blank CD),
		 * nautilus_autorun() will be called by mount_added_callback(). */
		nautilus_allow_autorun_for_volume (volume);
		nautilus_allow_autorun_for_volume_finish (volume);
	}
}

static void
drive_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	GError *error;
	char *primary;
	char *name;
	error = NULL;
	if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
			g_free (name);
			eel_show_error_dialog (primary,
					       error->message,
				       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
drive_eject_button_pressed (GDrive *drive,
			    NautilusApplication *application)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (NULL);
	g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb, NULL);
	g_object_unref (mount_op);
}

static void
drive_listen_for_eject_button (GDrive *drive, NautilusApplication *application)
{
	g_signal_connect (drive,
			  "eject-button",
			  G_CALLBACK (drive_eject_button_pressed),
			  application);
}

static void
drive_connected_callback (GVolumeMonitor *monitor,
			  GDrive *drive,
			  NautilusApplication *application)
{
	drive_listen_for_eject_button (drive, application);
}

static void
autorun_show_window (GMount *mount, gpointer user_data)
{
	GFile *location;
	NautilusApplication *application = user_data;
	
	location = g_mount_get_root (mount);
	
	/* Ther should probably be an easier way to do this */
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER)) {
		NautilusWindow *window;
		window = nautilus_application_create_navigation_window (application, 
									NULL, 
									gdk_screen_get_default ());
		nautilus_window_go_to (window, location);

	} else {
		nautilus_application_present_spatial_window (application, 
							     NULL, 
							     NULL, 
							     location, 
							     gdk_screen_get_default ());
	}
	g_object_unref (location);
}

static void
mount_added_callback (GVolumeMonitor *monitor,
		      GMount *mount,
		      NautilusApplication *application)
{
	NautilusDirectory *directory;
	GFile *root;

	if (!application->session_is_active) {
		return;
	}
		
	root = g_mount_get_root (mount);
	directory = nautilus_directory_get_existing (root);
	g_object_unref (root);
	if (directory != NULL) {
		nautilus_directory_force_reload (directory);
		nautilus_directory_unref (directory);
	}

	nautilus_autorun (mount, autorun_show_window, application);
}

static inline int
count_slots_of_windows (GList *window_list)
{
	NautilusWindow *window;
	GList *slots, *l;
	int count;

	count = 0;

	for (l = window_list; l != NULL; l = l->next) {
		window = NAUTILUS_WINDOW (l->data);

		slots = nautilus_window_get_slots (window);
		count += g_list_length (slots);
		g_list_free (slots);
	}

	return count;
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

/* Called whenever a mount is unmounted. Check and see if there are
 * any windows open displaying contents on the mount. If there are,
 * close them.  It would also be cool to save open window and position
 * info.
 *
 * This is also called on pre_unmount.
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
	GFile *root;

	close_list = NULL;
	force_no_close_slot = NULL;
	
	/* Check and see if any of the open windows are displaying contents from the unmounted mount */
	window_list = nautilus_application_get_window_list ();

	root = g_mount_get_root (mount);
	/* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
	for (node = window_list; node != NULL; node = node->next) {
		window = NAUTILUS_WINDOW (node->data);
  		if (window != NULL && window_can_be_closed (window)) {
			GList *l;
  			GFile *location;
  
			for (l = window->details->slots; l != NULL; l = l->next) {
				slot = l->data;
				location = slot->location;
				if (g_file_has_prefix (location, root)) {
					close_list = g_list_prepend (close_list, slot);
				} 
			}
		}
	}

	if (nautilus_application_desktop_windows == NULL &&
	    g_list_length (close_list) != 0 &&
	    g_list_length (close_list) == count_slots_of_windows (window_list)) {
		/* We are trying to close all open slots. Keep one navigation slot open. */
		force_no_close_slot = get_first_navigation_slot (close_list);
	}

	/* Handle the windows in the close list. */
	for (node = close_list; node != NULL; node = node->next) {
		slot = node->data;
		window = slot->window;

		if (NAUTILUS_IS_SPATIAL_WINDOW (window) ||
		    (nautilus_navigation_window_slot_should_close_with_mount (NAUTILUS_NAVIGATION_WINDOW_SLOT (slot), mount) &&
		     slot != force_no_close_slot)) {
			nautilus_window_slot_close (slot);
		} else {
			nautilus_window_slot_go_home (slot, FALSE);
		}
	}

	g_list_free (close_list);
}

static char *
icon_to_string (GIcon *icon)
{
	const char * const *names;
	GFile *file;
	
	if (icon == NULL) {
		return NULL;
	} else if (G_IS_THEMED_ICON (icon)) {
		names = g_themed_icon_get_names (G_THEMED_ICON (icon));
		return g_strjoinv (":", (char **)names);		
	} else if (G_IS_FILE_ICON (icon)) {
		file = g_file_icon_get_file (G_FILE_ICON (icon));
		return g_file_get_path (file);
	}
	return NULL;
}

static GIcon *
icon_from_string (const char *string)
{
	GFile *file;
	GIcon *icon;
	gchar **names;
	
	if (g_path_is_absolute (string)) {
		file = g_file_new_for_path (string);
		icon = g_file_icon_new (file);
		g_object_unref (file);
		return icon;
	} else {
		names = g_strsplit (string, ":", 0);
		icon = g_themed_icon_new_from_names (names, -1);
		g_strfreev (names);
		return icon;
	}
	return NULL;
}

static char *
nautilus_application_get_session_data (void)
{
	xmlDocPtr doc;
	xmlNodePtr root_node, history_node;
	GList *l;
	char *data;
	unsigned n_processed;
	xmlSaveCtxtPtr ctx;
	xmlBufferPtr buffer;

	doc = xmlNewDoc ("1.0");

	root_node = xmlNewNode (NULL, "session");
	xmlDocSetRootElement (doc, root_node);

	history_node = xmlNewChild (root_node, NULL, "history", NULL);

	n_processed = 0;
	for (l = nautilus_get_history_list (); l != NULL; l = l->next) {
		NautilusBookmark *bookmark;
		xmlNodePtr bookmark_node;
		GIcon *icon;
		char *tmp;

		bookmark = l->data;

		bookmark_node = xmlNewChild (history_node, NULL, "bookmark", NULL);

		tmp = nautilus_bookmark_get_name (bookmark);
		xmlNewProp (bookmark_node, "name", tmp);
		g_free (tmp);

		icon = nautilus_bookmark_get_icon (bookmark);
		tmp = icon_to_string (icon);
		g_object_unref (icon);
		if (tmp) {
			xmlNewProp (bookmark_node, "icon", tmp);
			g_free (tmp);
		}

		tmp = nautilus_bookmark_get_uri (bookmark);
		xmlNewProp (bookmark_node, "uri", tmp);
		g_free (tmp);

		if (nautilus_bookmark_get_has_custom_name (bookmark)) {
			xmlNewProp (bookmark_node, "has_custom_name", "TRUE");
		}

		if (++n_processed > 50) { /* prevent history list from growing arbitrarily large. */
			break;
		}
	}

	for (l = nautilus_application_window_list; l != NULL; l = l->next) {
		xmlNodePtr win_node, slot_node;
		NautilusWindow *window;
		NautilusWindowSlot *slot, *active_slot;
		GList *slots, *m;
		char *tmp;

		window = l->data;

		win_node = xmlNewChild (root_node, NULL, "window", NULL);

		xmlNewProp (win_node, "type", NAUTILUS_IS_NAVIGATION_WINDOW (window) ? "navigation" : "spatial");

		if (NAUTILUS_IS_NAVIGATION_WINDOW (window)) { /* spatial windows store their state as file metadata */
			tmp = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
			xmlNewProp (win_node, "geometry", tmp);
			g_free (tmp);

			if (GTK_WIDGET (window)->window &&
			    gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_MAXIMIZED) {
				xmlNewProp (win_node, "maximized", "TRUE");
			}

			if (GTK_WIDGET (window)->window &&
			    gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_STICKY) {
				xmlNewProp (win_node, "sticky", "TRUE");
			}

			if (GTK_WIDGET (window)->window &&
			    gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_ABOVE) {
				xmlNewProp (win_node, "keep-above", "TRUE");
			}
		}

		slots = nautilus_window_get_slots (window);
		active_slot = nautilus_window_get_active_slot (window);

		/* store one slot as window location. Otherwise
		 * older Nautilus versions will bail when reading the file. */
		tmp = nautilus_window_slot_get_location_uri (active_slot);
		xmlNewProp (win_node, "location", tmp);
		g_free (tmp);

		for (m = slots; m != NULL; m = m->next) {
			slot = NAUTILUS_WINDOW_SLOT (m->data);

			slot_node = xmlNewChild (win_node, NULL, "slot", NULL);

			tmp = nautilus_window_slot_get_location_uri (slot);
			xmlNewProp (slot_node, "location", tmp);
			g_free (tmp);

			if (slot == active_slot) {
				xmlNewProp (slot_node, "active", "TRUE");
			}
		}

		g_list_free (slots);
	}

	buffer = xmlBufferCreate ();
	xmlIndentTreeOutput = 1;
	ctx = xmlSaveToBuffer (buffer, "UTF-8", XML_SAVE_FORMAT);
	if (xmlSaveDoc (ctx, doc) < 0 ||
	    xmlSaveFlush (ctx) < 0) {
		g_message ("failed to save session");
	}
	
	xmlSaveClose(ctx);
	data = g_strndup (buffer->content, buffer->use);
	xmlBufferFree (buffer);

	xmlFreeDoc (doc);

	return data;
}

void
nautilus_application_load_session (NautilusApplication *application)
{
	xmlDocPtr doc;
	gboolean bail;
	xmlNodePtr root_node;
	GKeyFile *state_file;
	char *data;

	if (!egg_sm_client_is_resumed (application->smclient)) {
		return;
	}

	state_file = egg_sm_client_get_state_file (application->smclient);
	if (!state_file) {
		return;
	}

	data = g_key_file_get_string (state_file,
				      "Nautilus",
				      "documents",
				      NULL);
	if (data == NULL) {
		return;
	}
	
	bail = TRUE;

	doc = xmlReadMemory (data, strlen (data), NULL, "UTF-8", 0);
	if (doc != NULL && (root_node = xmlDocGetRootElement (doc)) != NULL) {
		xmlNodePtr node;
		
		bail = FALSE;
		
		for (node = root_node->children; node != NULL; node = node->next) {
			
			if (!strcmp (node->name, "text")) {
				continue;
			} else if (!strcmp (node->name, "history")) {
				xmlNodePtr bookmark_node;
				gboolean emit_change;
				
				emit_change = FALSE;
				
				for (bookmark_node = node->children; bookmark_node != NULL; bookmark_node = bookmark_node->next) {
					if (!strcmp (bookmark_node->name, "text")) {
						continue;
					} else if (!strcmp (bookmark_node->name, "bookmark")) {
						xmlChar *name, *icon_str, *uri;
						gboolean has_custom_name;
						GIcon *icon;
						GFile *location;
						
						uri = xmlGetProp (bookmark_node, "uri");
						name = xmlGetProp (bookmark_node, "name");
						has_custom_name = xmlHasProp (bookmark_node, "has_custom_name") ? TRUE : FALSE;
						icon_str = xmlGetProp (bookmark_node, "icon");
						icon = NULL;
						if (icon_str) {
							icon = icon_from_string (icon_str);
						}
						location = g_file_new_for_uri (uri);
						
						emit_change |= nautilus_add_to_history_list_no_notify (location, name, has_custom_name, icon);
						
						g_object_unref (location);
						
						if (icon) {
							g_object_unref (icon);
						}
						xmlFree (name);
						xmlFree (uri);
						xmlFree (icon_str);
					} else {
						g_message ("unexpected bookmark node %s while parsing session data", bookmark_node->name);
						bail = TRUE;
						continue;
					}
				}
				
				if (emit_change) {
					nautilus_send_history_list_changed ();
				}
			} else if (!strcmp (node->name, "window")) {
				NautilusWindow *window;
				xmlChar *type, *location_uri, *slot_uri;
				xmlNodePtr slot_node;
				GFile *location;
				int i;
				
				type = xmlGetProp (node, "type");
				if (type == NULL) {
					g_message ("empty type node while parsing session data");
					bail = TRUE;
					continue;
				}
				
				location_uri = xmlGetProp (node, "location");
				if (location_uri == NULL) {
					g_message ("empty location node while parsing session data");
					bail = TRUE;
					xmlFree (type);
					continue;
				}
				
				if (!strcmp (type, "navigation")) {
					xmlChar *geometry;
					
					window = nautilus_application_create_navigation_window (application, NULL, gdk_screen_get_default ());
					
					geometry = xmlGetProp (node, "geometry");
					if (geometry != NULL) {
						eel_gtk_window_set_initial_geometry_from_string 
							(GTK_WINDOW (window), 
							 geometry,
							 NAUTILUS_WINDOW_MIN_WIDTH, 
							 NAUTILUS_WINDOW_MIN_HEIGHT,
							 FALSE);
					}
					xmlFree (geometry);
					
					if (xmlHasProp (node, "maximized")) {
						gtk_window_maximize (GTK_WINDOW (window));
					} else {
						gtk_window_unmaximize (GTK_WINDOW (window));
					}
					
					if (xmlHasProp (node, "sticky")) {
						gtk_window_stick (GTK_WINDOW (window));
					} else {
						gtk_window_unstick (GTK_WINDOW (window));
					}
					
					if (xmlHasProp (node, "keep-above")) {
						gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
					} else {
						gtk_window_set_keep_above (GTK_WINDOW (window), FALSE);
					}
					
					for (i = 0, slot_node = node->children; slot_node != NULL; slot_node = slot_node->next) {
						if (!strcmp (slot_node->name, "slot")) {
							slot_uri = xmlGetProp (slot_node, "location");
							if (slot_uri != NULL) {
								NautilusWindowSlot *slot;
								
								if (i == 0) {
									slot = window->details->active_slot;
								} else {
									slot = nautilus_window_open_slot (window, NAUTILUS_WINDOW_OPEN_SLOT_APPEND);
								}
								
								location = g_file_new_for_uri (slot_uri);
								nautilus_window_slot_open_location (slot, location, FALSE);
								
								if (xmlHasProp (slot_node, "active")) {
									nautilus_window_set_active_slot (window, slot);
								}
								
								i++;
							}
							xmlFree (slot_uri);
						}
					}
					
					if (i == 0) {
						/* This may be an old session file */
						location = g_file_new_for_uri (location_uri);
						nautilus_window_slot_open_location (window->details->active_slot, location, FALSE);
						g_object_unref (location);
					}
				} else if (!strcmp (type, "spatial")) {
					location = g_file_new_for_uri (location_uri);
					window = nautilus_application_present_spatial_window (application, NULL, NULL, location, gdk_screen_get_default ());
					g_object_unref (location);
				} else {
					g_message ("unknown window type \"%s\" while parsing session data", type);
					bail = TRUE;
				}
				
				xmlFree (type);
				xmlFree (location_uri);
			} else {
				g_message ("unexpected node %s while parsing session data", node->name);
				bail = TRUE;
				continue;
			}
		}
	}
	
	if (doc != NULL) {
		xmlFreeDoc (doc);
	}

	g_free (data);

	if (bail) {
		g_message ("failed to load session");
	} 
}

#ifdef UGLY_HACK_TO_DETECT_KDE

static gboolean
get_self_typed_prop (Window      xwindow,
                     Atom        atom,
                     gulong     *val)
{  
	Atom type;
	int format;
	gulong nitems;
	gulong bytes_after;
	gulong *num;
	int err;
  
	gdk_error_trap_push ();
	type = None;
	XGetWindowProperty (gdk_display,
			    xwindow,
			    atom,
			    0, G_MAXLONG,
			    False, atom, &type, &format, &nitems,
			    &bytes_after, (guchar **)&num);  

	err = gdk_error_trap_pop ();
	if (err != Success) {
		return FALSE;
	}
  
	if (type != atom) {
		return FALSE;
	}

	if (val)
		*val = *num;
  
	XFree (num);

	return TRUE;
}

static gboolean
has_wm_state (Window xwindow)
{
	return get_self_typed_prop (xwindow,
				    XInternAtom (gdk_display, "WM_STATE", False),
				    NULL);
}

static gboolean
look_for_kdesktop_recursive (Window xwindow)
{
  
	Window ignored1, ignored2;
	Window *children;
	unsigned int n_children;
	unsigned int i;
	gboolean retval;
  
	/* If WM_STATE is set, this is a managed client, so look
	 * for the class hint and end recursion. Otherwise,
	 * this is probably just a WM frame, so keep recursing.
	 */
	if (has_wm_state (xwindow)) {      
		XClassHint ch;
      
		gdk_error_trap_push ();
		ch.res_name = NULL;
		ch.res_class = NULL;
      
		XGetClassHint (gdk_display, xwindow, &ch);
      
		gdk_error_trap_pop ();
      
		if (ch.res_name)
			XFree (ch.res_name);
      
		if (ch.res_class) {
			if (strcmp (ch.res_class, "kdesktop") == 0) {
				XFree (ch.res_class);
				return TRUE;
			}
			else
				XFree (ch.res_class);
		}

		return FALSE;
	}
  
	retval = FALSE;
  
	gdk_error_trap_push ();
  
	XQueryTree (gdk_display,
		    xwindow,
		    &ignored1, &ignored2, &children, &n_children);

	if (gdk_error_trap_pop ()) {
		return FALSE;
	}

	i = 0;
	while (i < n_children) {
		if (look_for_kdesktop_recursive (children[i])) {
			retval = TRUE;
			break;
		}
      
		++i;
	}
  
	if (children)
		XFree (children);

	return retval;
}
#endif /* UGLY_HACK_TO_DETECT_KDE */

static gboolean
is_kdesktop_present (void)
{
#ifdef UGLY_HACK_TO_DETECT_KDE
	/* FIXME this is a pretty lame hack, should be replaced
	 * eventually with e.g. a requirement that desktop managers
	 * support a manager selection, ICCCM sec 2.8
	 */
	return look_for_kdesktop_recursive (GDK_ROOT_WINDOW ());
#else
	return FALSE;
#endif
}

static void
nautilus_application_class_init (NautilusApplicationClass *class)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (class);
        object_class->finalize = nautilus_application_finalize;
}

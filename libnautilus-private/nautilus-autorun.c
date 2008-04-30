/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */
 
#include <config.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gdesktopappinfo.h>
#include <X11/XKBlib.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include "nautilus-icon-info.h"
#include "nautilus-global-preferences.h"
#include "nautilus-file-operations.h"
#include "nautilus-autorun.h"
#include "nautilus-program-choosing.h"
#include "nautilus-open-with-dialog.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-file-utilities.h"

enum
{
	AUTORUN_ASK,
	AUTORUN_IGNORE,
	AUTORUN_APP,
	AUTORUN_OPEN_FOLDER,
	AUTORUN_SEP,
};
enum
{
	COLUMN_AUTORUN_PIXBUF,
	COLUMN_AUTORUN_NAME,
	COLUMN_AUTORUN_APP_INFO,
	COLUMN_AUTORUN_X_CONTENT_TYPE,
	COLUMN_AUTORUN_ITEM_TYPE,	
};

static gboolean should_autorun_mount (GMount *mount);

void
nautilus_autorun_get_preferences (const char *x_content_type, 
				  gboolean *pref_ask, 
				  gboolean *pref_ignore, 
				  gboolean *pref_open_folder)
{
	char **x_content_ask;
	char **x_content_ignore;
	char **x_content_open_folder;

	g_return_if_fail (pref_ask != NULL);
	g_return_if_fail (pref_ignore != NULL);
	g_return_if_fail (pref_open_folder != NULL);

	*pref_ask = FALSE;
	*pref_ignore = FALSE;
	*pref_open_folder = FALSE;
	x_content_ask = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK);
	x_content_ignore = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE);
	x_content_open_folder = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);
	if (x_content_ask != NULL) {
		*pref_ask = eel_g_strv_find (x_content_ask, x_content_type) != -1;
	}
	if (x_content_ignore != NULL) {
		*pref_ignore = eel_g_strv_find (x_content_ignore, x_content_type) != -1;
	}
	if (x_content_open_folder != NULL) {
		*pref_open_folder = eel_g_strv_find (x_content_open_folder, x_content_type) != -1;
	}
	g_strfreev (x_content_ignore);
	g_strfreev (x_content_ask);

}

static void
remove_elem_from_str_array (char **v, const char *s)
{
	int n, m;
	for (n = 0; v[n] != NULL; n++) {
		if (strcmp (v[n], s) == 0) {
			for (m = n + 1; v[m] != NULL; m++) {
				v[m - 1] = v[m];
			}
			v[m - 1] = NULL;
			n--;
		}
	}
} 

static char **
add_elem_to_str_array (char **v, const char *s)
{
	guint len;
	char **r;

	len = g_strv_length (v);
	r = g_new0 (char *, len + 2);
	memcpy (r, v, len * sizeof (char *));
	r[len] = g_strdup (s);
	r[len+1] = NULL;
	g_free (v);

	return r;
}


void
nautilus_autorun_set_preferences (const char *x_content_type, gboolean pref_ask, gboolean pref_ignore, gboolean pref_open_folder)
{
	char **x_content_ask;
	char **x_content_ignore;
	char **x_content_open_folder;

	x_content_ask = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK);
	x_content_ignore = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE);
	x_content_open_folder = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);

	remove_elem_from_str_array (x_content_ask, x_content_type);
	if (pref_ask) {
		x_content_ask = add_elem_to_str_array (x_content_ask, x_content_type);
	}
	eel_preferences_set_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK, x_content_ask);

	remove_elem_from_str_array (x_content_ignore, x_content_type);
	if (pref_ignore) {
		x_content_ignore = add_elem_to_str_array (x_content_ignore, x_content_type);
	}
	eel_preferences_set_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE, x_content_ignore);

	remove_elem_from_str_array (x_content_open_folder, x_content_type);
	if (pref_open_folder) {
		x_content_open_folder = add_elem_to_str_array (x_content_open_folder, x_content_type);
	}
	eel_preferences_set_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER, x_content_open_folder);

	g_strfreev (x_content_open_folder);
	g_strfreev (x_content_ignore);
	g_strfreev (x_content_ask);

}

static gboolean
combo_box_separator_func (GtkTreeModel *model,
                          GtkTreeIter *iter,
                          gpointer data)
{
	char *str;

	gtk_tree_model_get (model, iter,
			    1, &str,
			    -1);
	if (str != NULL) {
		g_free (str);
		return FALSE;
	}
	return TRUE;
}

typedef struct
{
	guint changed_signal_id;
	GtkWidget *combo_box;

	gboolean update_settings;
	NautilusAutorunComboBoxChanged changed_cb;
	gpointer user_data;
} NautilusAutorunComboBoxData;

static void 
nautilus_autorun_combobox_data_destroy (NautilusAutorunComboBoxData *data)
{
	/* signal handler may be automatically disconnected by destroying the widget */
	if (g_signal_handler_is_connected (G_OBJECT (data->combo_box), data->changed_signal_id)) {
		g_signal_handler_disconnect (G_OBJECT (data->combo_box), data->changed_signal_id);
	}
	g_free (data);
}

static void 
combo_box_changed (GtkComboBox *combo_box,
                   NautilusAutorunComboBoxData *data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GAppInfo *app_info;
	char *x_content_type;
	int type;

	model = NULL;
	app_info = NULL;
	x_content_type = NULL;

	if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
		goto out;
	}

	model = gtk_combo_box_get_model (combo_box);
	if (model == NULL) {
		goto out;
	}

	gtk_tree_model_get (model, &iter, 
			    COLUMN_AUTORUN_APP_INFO, &app_info,
			    COLUMN_AUTORUN_X_CONTENT_TYPE, &x_content_type,
			    COLUMN_AUTORUN_ITEM_TYPE, &type,
			    -1);

	switch (type) {
	case AUTORUN_ASK:
		if (data->changed_cb != NULL) {
			data->changed_cb (TRUE, FALSE, FALSE, NULL, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, TRUE, FALSE, FALSE);
		}
		break;
	case AUTORUN_IGNORE:
		if (data->changed_cb != NULL) {
			data->changed_cb (FALSE, TRUE, FALSE, NULL, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, FALSE, TRUE, FALSE);
		}
		break;
	case AUTORUN_OPEN_FOLDER:
		if (data->changed_cb != NULL) {
			data->changed_cb (FALSE, FALSE, TRUE, NULL, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, FALSE, FALSE, TRUE);
		}
		break;
	case AUTORUN_APP:
		if (data->changed_cb != NULL) {
			data->changed_cb (FALSE, FALSE, FALSE, app_info, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, FALSE, FALSE, FALSE);
			g_app_info_set_as_default_for_type (app_info,
							    x_content_type,
							    NULL);
		}
		break;
	}
 
out:
	if (app_info != NULL) {
		g_object_unref (app_info);
	}
	if (model != NULL) {
		g_object_unref (model);
	}
	g_free (x_content_type);
}

void
nautilus_autorun_prepare_combo_box (GtkWidget *combo_box, 
				    const char *x_content_type,
				    gboolean include_ask,
				    gboolean update_settings,
				    NautilusAutorunComboBoxChanged changed_cb,
				    gpointer user_data)
{
	GList *l;
	GList *app_info_list;
	GAppInfo *default_app_info;
	GtkListStore *list_store;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	int icon_size;
	int set_active;
	int n;
	int num_apps;
	gboolean pref_ask;
	gboolean pref_ignore;
	gboolean pref_open_folder;
	NautilusAutorunComboBoxData *data;
	GtkCellRenderer *renderer;

	nautilus_autorun_get_preferences (x_content_type, &pref_ask, &pref_ignore, &pref_open_folder);

	icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

	set_active = -1;
	data = NULL;

	app_info_list = g_app_info_get_all_for_type (x_content_type);
	default_app_info = g_app_info_get_default_for_type (x_content_type, FALSE);
	num_apps = g_list_length (app_info_list);

	list_store = gtk_list_store_new (5, 
					 GDK_TYPE_PIXBUF, 
					 G_TYPE_STRING, 
					 G_TYPE_APP_INFO, 
					 G_TYPE_STRING,
					 G_TYPE_INT);

	/* no apps installed */
	if (num_apps == 0) {
		gtk_list_store_append (list_store, &iter);
		pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						   GTK_STOCK_DIALOG_ERROR,
						   icon_size,
						   0,
						   NULL);

		/* TODO: integrate with PackageKit-gnome to find applications */

		gtk_list_store_set (list_store, &iter, 
				    COLUMN_AUTORUN_PIXBUF, pixbuf, 
				    COLUMN_AUTORUN_NAME, _("No applications found"), 
				    COLUMN_AUTORUN_APP_INFO, NULL, 
				    COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
				    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_ASK,
				    -1);
		g_object_unref (pixbuf);
	} else {	
		if (include_ask) {
			gtk_list_store_append (list_store, &iter);
			pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
							   GTK_STOCK_DIALOG_QUESTION,
							   icon_size,
							   0,
							   NULL);
			gtk_list_store_set (list_store, &iter, 
					    COLUMN_AUTORUN_PIXBUF, pixbuf, 
					    COLUMN_AUTORUN_NAME, _("Ask what to do"), 
					    COLUMN_AUTORUN_APP_INFO, NULL, 
					    COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
					    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_ASK,
					    -1);
			g_object_unref (pixbuf);
		}
		
		gtk_list_store_append (list_store, &iter);
		pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						   GTK_STOCK_CLOSE,
						   icon_size,
						   0,
						   NULL);
		gtk_list_store_set (list_store, &iter, 
				    COLUMN_AUTORUN_PIXBUF, pixbuf, 
				    COLUMN_AUTORUN_NAME, _("Do Nothing"), 
				    COLUMN_AUTORUN_APP_INFO, NULL, 
				    COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
				    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_IGNORE,
				    -1);
		g_object_unref (pixbuf);		

		gtk_list_store_append (list_store, &iter);
		pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						   "nautilus",
						   icon_size,
						   0,
						   NULL);
		gtk_list_store_set (list_store, &iter, 
				    COLUMN_AUTORUN_PIXBUF, pixbuf, 
				    COLUMN_AUTORUN_NAME, _("Open Folder"), 
				    COLUMN_AUTORUN_APP_INFO, NULL, 
				    COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
				    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_OPEN_FOLDER,
				    -1);
		g_object_unref (pixbuf);		

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 
				    COLUMN_AUTORUN_PIXBUF, NULL, 
				    COLUMN_AUTORUN_NAME, NULL, 
				    COLUMN_AUTORUN_APP_INFO, NULL, 
				    COLUMN_AUTORUN_X_CONTENT_TYPE, NULL,
				    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_SEP,
				    -1);

		for (l = app_info_list, n = include_ask ? 4 : 3; l != NULL; l = l->next, n++) {
			GIcon *icon;
			NautilusIconInfo *icon_info;
			char *open_string;
			GAppInfo *app_info = l->data;
			
			/* we deliberately ignore should_show because some apps might want
			 * to install special handlers that should be hidden in the regular
			 * application launcher menus
			 */
			
			icon = g_app_info_get_icon (app_info);
			icon_info = nautilus_icon_info_lookup (icon, icon_size);
			pixbuf = nautilus_icon_info_get_pixbuf_at_size (icon_info, icon_size);
			g_object_unref (icon_info);
			
			open_string = g_strdup_printf (_("Open %s"), g_app_info_get_name (app_info));

			gtk_list_store_append (list_store, &iter);
			gtk_list_store_set (list_store, &iter, 
					    COLUMN_AUTORUN_PIXBUF, pixbuf, 
					    COLUMN_AUTORUN_NAME, open_string, 
					    COLUMN_AUTORUN_APP_INFO, app_info, 
					    COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
					    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_APP,
					    -1);
			if (pixbuf != NULL) {
				g_object_unref (pixbuf);
			}
			g_free (open_string);
			
			if (g_app_info_equal (app_info, default_app_info)) {
				set_active = n;
			}
		}
	}

	if (default_app_info != NULL) {
		g_object_unref (default_app_info);
	}
	eel_g_object_list_free (app_info_list);

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (list_store));

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", COLUMN_AUTORUN_PIXBUF,
					NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", COLUMN_AUTORUN_NAME,
					NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo_box), combo_box_separator_func, NULL, NULL);

	if (num_apps == 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
		gtk_widget_set_sensitive (combo_box, FALSE);
	} else {
		gtk_widget_set_sensitive (combo_box, TRUE);
		if (pref_ask && include_ask) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
		} else if (pref_ignore) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), include_ask ? 1 : 0);
		} else if (pref_open_folder) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), include_ask ? 2 : 1);
		} else if (set_active != -1) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), set_active);
		} else {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), include_ask ? 1 : 0);
		}

		data = g_new0 (NautilusAutorunComboBoxData, 1);
		data->update_settings = update_settings;
		data->changed_cb = changed_cb;
		data->user_data = user_data;
		data->combo_box = combo_box;
		data->changed_signal_id = g_signal_connect (G_OBJECT (combo_box),
							    "changed",
							    G_CALLBACK (combo_box_changed),
							    data);
	}

	g_object_set_data_full (G_OBJECT (combo_box),
				"nautilus_autorun_combobox_data",
				data,
				(GDestroyNotify) nautilus_autorun_combobox_data_destroy);
}

static gboolean
is_shift_pressed (void)
{
	gboolean ret;
	XkbStateRec state;
	Bool status;

	ret = FALSE;

        gdk_error_trap_push ();
	status = XkbGetState (GDK_DISPLAY (), XkbUseCoreKbd, &state);
        gdk_error_trap_pop ();

	if (status == Success) {
		ret = state.mods & ShiftMask;
	}

	return ret;
}

/*-- BEGIN MOVE TO GIO --*/

static gboolean
_check_nonempty_dir (GFile *mount_root, const char *dirname)
{
	GFile *file;
	GFileInfo *file_info;
	GFileEnumerator *file_enum;
	gboolean ret;

	ret = FALSE;

	file = g_file_get_child (mount_root, dirname);
	file_enum = g_file_enumerate_children (file,
					       G_FILE_ATTRIBUTE_STANDARD_NAME,
					       G_FILE_QUERY_INFO_NONE,
					       NULL,
					       NULL);
	if (file_enum != NULL) {
		file_info = g_file_enumerator_next_file (file_enum, NULL, NULL);
		if (file_info != NULL) {
			ret = TRUE;
			g_object_unref (file_info);
		}
		g_object_unref (file_enum);
	}
	g_object_unref (file);

	return ret;
}

static gboolean
_check_file_common (GFile *file, gboolean must_be_executable)
{
	GFileInfo *file_info;
	gboolean ret;

	ret = FALSE;

	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
				       G_FILE_QUERY_INFO_NONE,
				       NULL,
				       NULL);
	if (file_info != NULL) {
		if (must_be_executable) {
			if (g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
				ret = TRUE;
		} else {
			ret = TRUE;
		}
		g_object_unref (file_info);
	}
	g_object_unref (file);

	return ret;
}

static gboolean
_check_file (GFile *mount_root, const char *file_path,
             gboolean must_be_executable)
{
	/* Unreffed in _check_file_common() */
	GFile *file = g_file_get_child (mount_root, file_path);
	return _check_file_common (file, must_be_executable);
}

#ifdef NAUTILUS_AUTORUN_SUPPORTS_AUTORUN_EXE
static gboolean
_check_file_case_insensitive (GFile *mount_root, const char *file_path,
                              gboolean must_be_executable)
{
	/* Unreffed in _check_file_common() */
	GFile *file = nautilus_find_file_insensitive (mount_root, file_path);
	return _check_file_common (file, must_be_executable);
}
#endif

/**
 * _g_mount_guess_content_type:
 * @mount: a #GMount.
 * @force_rescan: Whether to force a rescan of the content. Otherwise a cached result will be used if available.
 * @error: return location for error or %NULL to ignore.
 *
 * Tries to guess the type of content stored on @mount. Returns one or
 * more textual identifiers of well-known content types (typically
 * prefixed with "x-content/"). TODO: link to fd.o spec about this.
 *
 * This function may do I/O and thus may take a long time to
 * complete. For the async version, see
 * _g_mount_guess_content_type_async().
 *
 * Returns: a %NULL terminated array of content types or %NULL on
 * error. Caller should free this array with g_strfreev() when done
 * with it.
 **/
char **
_g_mount_guess_content_type (GMount              *mount,
			     gboolean             force_rescan,
			     GError             **error)
{
	unsigned int n;
	char **ret;
	GPtrArray *types;
	GFile *root;
	GVolume *volume;
	char *disc_type = NULL;

	/* TODO: This cache handling isn't really threadsafe.
	 * I think this is ok for nautilus use, but not for general gio use
	 */
	ret = g_object_get_data (G_OBJECT (mount), "content-type-cache");
	if (ret != NULL) {
		return g_strdupv (ret);
	}
	
	types = g_ptr_array_new ();

	root = g_mount_get_root (mount);
	volume = g_mount_get_volume (mount);

	/* Take advantage of information from HAL's quirk lists that maps a given 
	 * make/model of (what appears to be just) a storage device to it's intended
	 * use.
	 *
	 * E.g. a mapping saying that a storage device is a music player, a digital
	 * camera, a videocam, a video play back device, a gps reader.. and so on...
	 */
	if (volume != NULL) {
		char **stor_device_caps;

		/* See gvfs/hal/ghalvolume.c:do_update_from_hal()...
		 *
		 * This hack, using g_object_set|get_data() can be
		 * removed once g_mount_guess_content_type() is in gio
		 * and the actual code for probing media is in the
		 * gvfs hal backend.
		 */
		stor_device_caps = (char **) g_object_get_data (G_OBJECT (volume), "hal-storage-device-capabilities");
		if (stor_device_caps != NULL) {
			for (n = 0; stor_device_caps[n] != NULL; n++) {
				if (strcmp (stor_device_caps[n], "portable_audio_player") == 0) {
					g_ptr_array_add (types, g_strdup ("x-content/audio-player"));
				}
				/* TODO: map other hal capabilities to x-content/ types */
			}
		}

		disc_type = (char *) g_object_get_data (G_OBJECT (volume), "hal-volume.disc.type");
	}
	
	if (g_file_has_uri_scheme (root, "cdda")) {
		g_ptr_array_add (types, g_strdup ("x-content/audio-cdda"));
		goto no_sniff;
	}

	if (g_file_has_uri_scheme (root, "burn")) {
		if (disc_type != NULL) {
			if (g_str_has_prefix (disc_type, "dvd")) {
				g_ptr_array_add (types, g_strdup ("x-content/blank-dvd"));
			} else if (g_str_has_prefix (disc_type, "hddvd")) {
				g_ptr_array_add (types, g_strdup ("x-content/blank-hddvd"));
			} else if (g_str_has_prefix (disc_type, "bd")) {
				g_ptr_array_add (types, g_strdup ("x-content/blank-bd"));
			} else {
				/* assume CD */
				g_ptr_array_add (types, g_strdup ("x-content/blank-cd"));
			}
		}
		goto no_sniff;
	}
	
	if (_check_nonempty_dir (root, "DCIM") ||
	    _check_nonempty_dir (root, "dcim")) {
		g_ptr_array_add (types, g_strdup ("x-content/image-dcf"));
	}
	
	if (_check_nonempty_dir (root, "VIDEO_TS") && 
	    disc_type != NULL) {
		g_ptr_array_add (types, g_strdup ("x-content/video-dvd"));
	}

	if (_check_nonempty_dir (root, "AUDIO_TS") && 
	    disc_type != NULL) {
		g_ptr_array_add (types, g_strdup ("x-content/audio-dvd"));
	}


	/* see http://www.ccs.neu.edu/home/bchafy/cdb/info/info.html for various docs */

	if (_check_nonempty_dir (root, "SVCD") && 
	    _check_nonempty_dir (root, "EXT") &&
	    _check_nonempty_dir (root, "MPEG-2") && 
	    disc_type != NULL) {
		/* http://everything2.com/index.pl?node_id=1009222 */
		g_ptr_array_add (types, g_strdup ("x-content/video-svcd"));
	}

	if (_check_nonempty_dir (root, "VCD") && 
	    _check_nonempty_dir (root, "MPEGAV") && 
	    disc_type != NULL) {
		/* http://www.herongyang.com/CD-DVD/VCD-Movie-File-Directory-Structure.html */
		g_ptr_array_add (types, g_strdup ("x-content/video-vcd"));
	}

	if (_check_nonempty_dir (root, "BDAV") && 
	    _check_nonempty_dir (root, "BDMV") && 
	    disc_type != NULL) {
		/* http://www.blu-raydisc.com/Section-13470/Section-13890/Index.html */
		g_ptr_array_add (types, g_strdup ("x-content/video-bluray"));
	}

	if (_check_nonempty_dir (root, "HVDVD_TS") && /* not a typo; should really spell HVDVD_TS */
	    disc_type != NULL) {
		/* http://www.cdfreaks.com/reviews/CDFreaks--CES-2006/Page-5.html */
		g_ptr_array_add (types, g_strdup ("x-content/video-hddvd"));
	}

	if (_check_nonempty_dir (root, "PICTURES") &&
	    disc_type != NULL) {
		/* http://www.re.org/kristin/picturecd.html */
		g_ptr_array_add (types, g_strdup ("x-content/image-picturecd"));
	}

	if (g_file_is_native (root) &&
	    (_check_file (root, ".autorun", TRUE) ||
	     _check_file (root, "autorun", TRUE) ||
	     _check_file (root, "autorun.sh", TRUE) ||
#ifdef NAUTILUS_AUTORUN_SUPPORTS_AUTORUN_EXE
	     /* TODO */
	     _check_file_case_insensitive (root, "autorun.exe", TRUE) ||
	     _check_file_case_insensitive (root, "autorun.inf", FALSE)))
#else
	    0))
#endif
	{
		/* http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html */
		
		/* http://bugzilla.gnome.org/show_bug.cgi?id=509823#c3 for the autorun.exe and autorun.inf stuff */
		g_ptr_array_add (types, g_strdup ("x-content/software"));
	}

no_sniff:
	
	g_ptr_array_add (types, NULL);
	ret = (char **) g_ptr_array_free (types, FALSE);

	if (volume != NULL) {
		g_object_unref (volume);
	}
	g_object_unref (root);

	g_object_set_data_full (G_OBJECT (mount),
				"content-type-cache",
				g_strdupv (ret),
				(GDestroyNotify)g_strfreev);
	
	return ret;
}

typedef struct {
	char **guessed_content_type;
	gboolean force_rescan;
} GuessContentData;

static void
guess_content_thread (GSimpleAsyncResult *res,
                      GObject            *object,
                      GCancellable       *cancellable)
{
	GuessContentData *op;
	GError *error = NULL;
	
	op = g_simple_async_result_get_op_res_gpointer (res);

	op->guessed_content_type = _g_mount_guess_content_type (G_MOUNT (object), op->force_rescan, &error);
	
	if (error != NULL) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
	}
}

/**
 * _g_mount_guess_content_type_async:
 * @mount: a #GMount.
 * @force_rescan: Whether to force a rescan of the content. Otherwise a cached result will be used if available.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: user data passed to @callback.
 *
 * This is an asynchronous version of _g_mount_guess_content_type(),
 * and is finished by calling _g_mount_guess_content_type_finish() with
 * the @mount and #GAsyncResults data returned in the @callback.
 */
void
_g_mount_guess_content_type_async (GMount              *mount,
				   gboolean             force_rescan,
				   GCancellable        *cancellable,
				   GAsyncReadyCallback  callback,
				   gpointer             user_data)
{
	GSimpleAsyncResult *res;
	GuessContentData *op;
	
	op = g_new0 (GuessContentData, 1);
	op->force_rescan = force_rescan;
	res = g_simple_async_result_new (G_OBJECT (mount), 
					 callback, 
					 user_data, 
					 _g_mount_guess_content_type_async);
	g_simple_async_result_set_op_res_gpointer (res, op, g_free);
	
	g_simple_async_result_run_in_thread (res, guess_content_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (res);
}

/**
 * _g_mount_guess_content_type_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes guessing content types of @mount. If any errors occured
 * during the operation, @error will be set to contain the errors and
 * %FALSE will be returned.
 * 
 * Returns: a %NULL terminated array of content types or %NULL on
 * error. Caller should free this array with g_strfreev() when done
 * with it.
 **/
char **
_g_mount_guess_content_type_finish (GMount              *mount,
				    GAsyncResult        *result,
				    GError             **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	GuessContentData *op;
	
	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == _g_mount_guess_content_type_async);
	
	op = g_simple_async_result_get_op_res_gpointer (simple);
	return op->guessed_content_type;
}


/*- END MOVE TO GIO ---*/

enum {
	AUTORUN_DIALOG_RESPONSE_EJECT = 0
};

typedef struct
{
	GtkWidget *dialog;

	GMount *mount;
	gboolean should_eject;

	gboolean selected_ignore;
	gboolean selected_open_folder;
	GAppInfo *selected_app;

	gboolean remember;

	char *x_content_type;

	NautilusAutorunOpenWindow open_window_func;
	gpointer user_data;
} AutorunDialogData;


void
nautilus_autorun_launch_for_mount (GMount *mount, GAppInfo *app_info)
{
	GFile *root;
	NautilusFile *file;
	GList *files;
	
	root = g_mount_get_root (mount);
	file = nautilus_file_get (root);
	g_object_unref (root);
	files = g_list_append (NULL, file);
	nautilus_launch_application (app_info,
				     files,
				     NULL); /* TODO: what to set here? */
	g_object_unref (file);
	g_list_free (files);
}

static void autorun_dialog_mount_unmounted (GMount *mount, AutorunDialogData *data);

static void
autorun_dialog_destroy (AutorunDialogData *data)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (data->mount),
					      G_CALLBACK (autorun_dialog_mount_unmounted),
					      data);

	gtk_widget_destroy (GTK_WIDGET (data->dialog));
	if (data->selected_app != NULL) {
		g_object_unref (data->selected_app);
	}
	g_object_unref (data->mount);
	g_free (data->x_content_type);
	g_free (data);
}

static void 
autorun_dialog_mount_unmounted (GMount *mount, AutorunDialogData *data)
{
	/* remove the dialog if the media is unmounted */
	autorun_dialog_destroy (data);
}

static void 
autorun_dialog_response (GtkDialog *dialog, gint response, AutorunDialogData *data)
{
	switch (response) {
	case AUTORUN_DIALOG_RESPONSE_EJECT:
		nautilus_file_operations_unmount_mount (GTK_WINDOW (dialog),
							data->mount,
							data->should_eject,
							FALSE);
		break;

	case GTK_RESPONSE_NONE:
		/* window was closed */
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	case GTK_RESPONSE_OK:
		/* do the selected action */

		if (data->remember) {
			/* make sure we don't ask again */
			nautilus_autorun_set_preferences (data->x_content_type, FALSE, data->selected_ignore, data->selected_open_folder);
			if (!data->selected_ignore && !data->selected_open_folder && data->selected_app != NULL) {
				g_app_info_set_as_default_for_type (data->selected_app,
								    data->x_content_type,
								    NULL);
			}
		} else {
			/* make sure we do ask again */
			nautilus_autorun_set_preferences (data->x_content_type, TRUE, FALSE, FALSE);
		}

		if (!data->selected_ignore && !data->selected_open_folder && data->selected_app != NULL) {
			nautilus_autorun_launch_for_mount (data->mount, data->selected_app);
		} else if (!data->selected_ignore && data->selected_open_folder) {
			if (data->open_window_func != NULL)
				data->open_window_func (data->mount, data->user_data);
		}
		break;
	}

	autorun_dialog_destroy (data);
}

static void
autorun_combo_changed (gboolean selected_ask,
		       gboolean selected_ignore,
		       gboolean selected_open_folder,
		       GAppInfo *selected_app,
		       gpointer user_data)
{
	AutorunDialogData *data = user_data;

	if (data->selected_app != NULL) {
		g_object_unref (data->selected_app);
	}
	data->selected_app = selected_app != NULL ? g_object_ref (selected_app) : NULL;
	data->selected_ignore = selected_ignore;
	data->selected_open_folder = selected_open_folder;
}


static void
autorun_always_toggled (GtkToggleButton *togglebutton, AutorunDialogData *data)
{
	data->remember = gtk_toggle_button_get_active (togglebutton);
}


/* returns TRUE if a folder window should be opened */
static gboolean
do_autorun_for_content_type (GMount *mount, const char *x_content_type, NautilusAutorunOpenWindow open_window_func, gpointer user_data)
{
	AutorunDialogData *data;
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *combo_box;
	GtkWidget *always_check_button;
	GtkWidget *eject_button;
	GtkWidget *image;
	char *markup;
	char *content_description;
	char *mount_name;
	GIcon *icon;
	GdkPixbuf *pixbuf;
	NautilusIconInfo *icon_info;
	int icon_size;
	gboolean user_forced_dialog;
	gboolean pref_ask;
	gboolean pref_ignore;
	gboolean pref_open_folder;
	char *media_greeting;
	gboolean ret;

	ret = FALSE;
	mount_name = NULL;

	user_forced_dialog = is_shift_pressed ();

	nautilus_autorun_get_preferences (x_content_type, &pref_ask, &pref_ignore, &pref_open_folder);

	if (user_forced_dialog) {
		goto show_dialog;
	}

	if (!pref_ask && !pref_ignore && !pref_open_folder) {
		GAppInfo *app_info;
		app_info = g_app_info_get_default_for_type (x_content_type, FALSE);
		if (app_info != NULL) {
			nautilus_autorun_launch_for_mount (mount, app_info);
		}
		goto out;
	}

	if (pref_open_folder) {
		ret = TRUE;
		goto out;
	}

	if (pref_ignore) {
		goto out;
	}

show_dialog:

	mount_name = g_mount_get_name (mount);

	dialog = gtk_dialog_new ();

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);

	icon = g_mount_get_icon (mount);
	icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_DIALOG);
	icon_info = nautilus_icon_info_lookup (icon, icon_size);
	pixbuf = nautilus_icon_info_get_pixbuf_at_size (icon_info, icon_size);
	g_object_unref (icon_info);
	g_object_unref (icon);
	image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), image);
	/* also use the icon on the dialog */
	gtk_window_set_title (GTK_WINDOW (dialog), mount_name);
	gtk_window_set_icon (GTK_WINDOW (dialog), pixbuf);
	g_object_unref (pixbuf);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), vbox);

	label = gtk_label_new (NULL);


	/* Customize greeting for well-known x-content types */
	if (strcmp (x_content_type, "x-content/audio-cdda") == 0) {
		media_greeting = _("You have just inserted an Audio CD.");
	} else if (strcmp (x_content_type, "x-content/audio-dvd") == 0) {
		media_greeting = _("You have just inserted an Audio DVD.");
	} else if (strcmp (x_content_type, "x-content/video-dvd") == 0) {
		media_greeting = _("You have just inserted a Video DVD.");
	} else if (strcmp (x_content_type, "x-content/video-vcd") == 0) {
		media_greeting = _("You have just inserted a Video CD.");
	} else if (strcmp (x_content_type, "x-content/video-svcd") == 0) {
		media_greeting = _("You have just inserted a Super Video CD.");
	} else if (strcmp (x_content_type, "x-content/blank-cd") == 0) {
		media_greeting = _("You have just inserted a blank CD.");
	} else if (strcmp (x_content_type, "x-content/blank-dvd") == 0) {
		media_greeting = _("You have just inserted a blank DVD.");
	} else if (strcmp (x_content_type, "x-content/blank-cd") == 0) {
		media_greeting = _("You have just inserted a blank Blu-Ray disc.");
	} else if (strcmp (x_content_type, "x-content/blank-cd") == 0) {
		media_greeting = _("You have just inserted a blank HD DVD.");
	} else if (strcmp (x_content_type, "x-content/image-photocd") == 0) {
		media_greeting = _("You have just inserted a Photo CD.");
	} else if (strcmp (x_content_type, "x-content/image-picturecd") == 0) {
		media_greeting = _("You have just inserted a Picture CD.");
	} else if (strcmp (x_content_type, "x-content/image-dcf") == 0) {
		media_greeting = _("You have just inserted media with digital photos.");
	} else if (strcmp (x_content_type, "x-content/audio-player") == 0) {
		media_greeting = _("You have just inserted a digital audio player.");
	} else if (strcmp (x_content_type, "x-content/software") == 0) {
		media_greeting = _("You have just inserted media with software intended to be automatically started.");
	} else {
		/* fallback to generic greeting */
		media_greeting = _("You have just inserted media.");
	}
	markup = g_strdup_printf ("<big><b>%s %s</b></big>", media_greeting, _("Choose what application to launch."));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), label);

	label = gtk_label_new (NULL);
	content_description = g_content_type_get_description (x_content_type);
	markup = g_strdup_printf (_("Select how to open \"%s\" and whether to perform this action in the future for other media of type \"%s\"."), mount_name, content_description);
	g_free (content_description);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), label);
	
	data = g_new0 (AutorunDialogData, 1);
	data->dialog = dialog;
	data->mount = g_object_ref (mount);
	data->remember = !pref_ask;
	data->selected_ignore = pref_ignore;
	data->x_content_type = g_strdup (x_content_type);
	data->selected_app = g_app_info_get_default_for_type (x_content_type, FALSE);
	data->open_window_func = open_window_func;
	data->user_data = user_data;

	combo_box = gtk_combo_box_new ();
	nautilus_autorun_prepare_combo_box (combo_box, x_content_type, FALSE, FALSE, autorun_combo_changed, data);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), combo_box);

	always_check_button = gtk_check_button_new_with_mnemonic (_("_Always perform this action"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (always_check_button), data->remember);
	g_signal_connect (G_OBJECT (always_check_button),
			  "toggled",
			  G_CALLBACK (autorun_always_toggled),
			  data);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), always_check_button);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), 
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	
	if (g_mount_can_eject (mount)) {
		GtkWidget *eject_image;
		eject_button = gtk_button_new_with_mnemonic (_("_Eject"));
		pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
						   "media-eject",
						   nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_BUTTON),
						   0,
						   NULL);
		eject_image = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);
		gtk_button_set_image (GTK_BUTTON (eject_button), eject_image);
		data->should_eject = TRUE;
	} else {
		eject_button = gtk_button_new_with_mnemonic (_("_Unmount"));
		data->should_eject = FALSE;
	}
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), eject_button, AUTORUN_DIALOG_RESPONSE_EJECT);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area), eject_button, TRUE);

	/* show the dialog */
	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (autorun_dialog_response),
			  data);

	g_signal_connect (G_OBJECT (data->mount),
			  "unmounted",
			  G_CALLBACK (autorun_dialog_mount_unmounted),
			  data);

out:
	g_free (mount_name);
	return ret;
}

typedef struct {
	GMount *mount;
	NautilusAutorunOpenWindow open_window_func;
	gpointer user_data;
} AutorunData;


static void
autorun_open_folder_for_mount (AutorunData *data)
{
	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT_OPEN) && 
	    data->open_window_func != NULL)
		data->open_window_func (data->mount, data->user_data);
}

static void
autorun_guessed_content_type_callback (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	GError *error;
	char **guessed_content_type;
	AutorunData *data = user_data;
	gboolean open_folder;

	open_folder = FALSE;

	error = NULL;
	guessed_content_type = _g_mount_guess_content_type_finish (G_MOUNT (source_object), res, &error);
	if (error != NULL) {
		g_warning ("Unabled to guess content type for mount: %s", error->message);
		g_error_free (error);
	} else {
		if (guessed_content_type != NULL && g_strv_length (guessed_content_type) > 0) {
			int n;
			for (n = 0; guessed_content_type[n] != NULL; n++) {
				if (do_autorun_for_content_type (data->mount, guessed_content_type[n], 
								 data->open_window_func, data->user_data)) {
					open_folder = TRUE;
				}
			}
			g_strfreev (guessed_content_type);
		} else {
			open_folder = TRUE;
		}
	}

	/* only open the folder once.. */
	if (open_folder) {
		autorun_open_folder_for_mount (data);
	}

	g_object_unref (data->mount);
	g_free (data);
}

void
nautilus_autorun (GMount *mount, NautilusAutorunOpenWindow open_window_func, gpointer user_data)
{
	AutorunData *data;

	if (!should_autorun_mount (mount) ||
	    eel_preferences_get_boolean (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_NEVER)) {
		return;
	}

	/* Sniff the newly added mount to generate x-content/ types;
	 * we do this asynchronously (in another thread) since it
	 * requires doing I/O.
	 */

	data = g_new0 (AutorunData, 1);
	data->mount = g_object_ref (mount);
	data->open_window_func = open_window_func;
	data->user_data = user_data;

	_g_mount_guess_content_type_async (mount,
					   TRUE,
					   NULL,
					   autorun_guessed_content_type_callback,
					   data);
}

typedef struct {
	NautilusAutorunGetContent callback;
	gpointer user_data;
} GetContentTypesData;

static void
get_types_cb (GObject *source_object,
	      GAsyncResult *res,
	      gpointer user_data)
{
	GetContentTypesData *data;
	char **types;

	data = user_data;
	types = _g_mount_guess_content_type_finish (G_MOUNT (source_object), res, NULL);

	if (data->callback) {
		data->callback (types, data->user_data);
	}
	g_strfreev (types);
	g_free (data);
}

void
nautilus_autorun_get_x_content_types_for_mount_async (GMount *mount,
						      NautilusAutorunGetContent callback,
						      GCancellable *cancellable,
						      gpointer user_data)
{
	char **cached;
	GetContentTypesData *data;
	
	if (mount == NULL) {
		if (callback) {
			callback (NULL, user_data);
		}
		return;
	}

	cached = g_object_get_data (G_OBJECT (mount), "content-type-cache");
	if (cached != NULL) {
		if (callback) {
			callback (cached, user_data);
		}
		return;
	}

	data = g_new (GetContentTypesData, 1);
	data->callback = callback;
	data->user_data = user_data;
	
	_g_mount_guess_content_type_async (mount,
					   FALSE,
					   cancellable,
					   get_types_cb,
					   data);
}


char **
nautilus_autorun_get_cached_x_content_types_for_mount (GMount      *mount)
{
	char **cached;
	
	if (mount == NULL) {
		return NULL;
	}

	cached = g_object_get_data (G_OBJECT (mount), "content-type-cache");
	if (cached != NULL) {
		return g_strdupv (cached);
	}

	return NULL;
}

static gboolean
remove_allow_volume (gpointer data)
{
	GVolume *volume = data;

	g_object_set_data (G_OBJECT (volume), "nautilus-allow-autorun", NULL);
	return FALSE;
}

void
nautilus_allow_autorun_for_volume (GVolume *volume)
{
	g_object_set_data (G_OBJECT (volume), "nautilus-allow-autorun", GINT_TO_POINTER (1));
}

void
nautilus_allow_autorun_for_volume_finish (GVolume *volume)
{
	if (g_object_get_data (G_OBJECT (volume), "nautilus-allow-autorun") != NULL) {
		g_timeout_add_full (0,
				    5000,
				    remove_allow_volume,
				    g_object_ref (volume),
				    g_object_unref);
	}
}

static gboolean
should_skip_native_mount_root (GFile *root)
{
	char *path;
	gboolean should_skip;

	/* skip any mounts in hidden directory hierarchies */
	path = g_file_get_path (root);
	should_skip = strstr (path, "/.") != NULL;
	g_free (path);

	return should_skip;
}

static gboolean
should_autorun_mount (GMount *mount)
{
	GFile *root;
	GVolume *enclosing_volume;
	gboolean ignore_autorun;

	ignore_autorun = TRUE;
	enclosing_volume = g_mount_get_volume (mount);
	if (enclosing_volume != NULL) {
		if (g_object_get_data (G_OBJECT (enclosing_volume), "nautilus-allow-autorun") != NULL) {
			ignore_autorun = FALSE;
			g_object_set_data (G_OBJECT (enclosing_volume), "nautilus-allow-autorun", NULL);
		}
	}

	if (ignore_autorun) {
		if (enclosing_volume != NULL) {
			g_object_unref (enclosing_volume);
		}
		return FALSE;
	}
	
	root = g_mount_get_root (mount);

	/* only do autorun on local files or files where g_volume_should_automount() returns TRUE */
	ignore_autorun = TRUE;
	if ((g_file_is_native (root) && !should_skip_native_mount_root (root)) || 
	    (enclosing_volume != NULL && g_volume_should_automount (enclosing_volume))) {
		ignore_autorun = FALSE;
	}
	if (enclosing_volume != NULL) {
		g_object_unref (enclosing_volume);
	}
	g_object_unref (root);

	return !ignore_autorun;
}

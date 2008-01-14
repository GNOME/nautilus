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

#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include "nautilus-icon-info.h"
#include "nautilus-global-preferences.h"
#include "nautilus-file-operations.h"
#include "nautilus-autorun.h"
#include "nautilus-program-choosing.h"

enum
{
	AUTORUN_ASK,
	AUTORUN_IGNORE,
	AUTORUN_APP,
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

void
nautilus_autorun_get_preferences (const char *x_content_type, gboolean *pref_ask, gboolean *pref_ignore)
{
	char **x_content_ask;
	char **x_content_ignore;

	g_return_if_fail (pref_ask != NULL);
	g_return_if_fail (pref_ignore != NULL);

	*pref_ask = FALSE;
	*pref_ignore = FALSE;
	x_content_ask = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK);
	x_content_ignore = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE);
	if (x_content_ask != NULL) {
		*pref_ask = eel_g_strv_find (x_content_ask, x_content_type) != -1;
	}
	if (x_content_ignore != NULL) {
		*pref_ignore = eel_g_strv_find (x_content_ignore, x_content_type) != -1;
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
nautilus_autorun_set_preferences (const char *x_content_type, gboolean pref_ask, gboolean pref_ignore)
{
	char **x_content_ask;
	char **x_content_ignore;

	x_content_ask = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK);
	x_content_ignore = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE);

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
	gboolean update_settings;
	NautilusAutorunComboBoxChanged changed_cb;
	gpointer user_data;
} NautilusAutorunComboBoxData;

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
			data->changed_cb (TRUE, FALSE, NULL, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, TRUE, FALSE);
		}
		break;
	case AUTORUN_IGNORE:
		if (data->changed_cb != NULL) {
			data->changed_cb (FALSE, TRUE, NULL, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, FALSE, TRUE);
		}
		break;
	case AUTORUN_APP:
		if (data->changed_cb != NULL) {
			data->changed_cb (FALSE, FALSE, app_info, data->user_data);
		}
		if (data->update_settings) {
			nautilus_autorun_set_preferences (x_content_type, FALSE, FALSE);
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
	NautilusAutorunComboBoxData *data;
	GtkCellRenderer *renderer;

	nautilus_autorun_get_preferences (x_content_type, &pref_ask, &pref_ignore);

	icon_size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

	set_active = -1;

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
						   GTK_STOCK_CANCEL,
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
		gtk_list_store_set (list_store, &iter, 
				    COLUMN_AUTORUN_PIXBUF, NULL, 
				    COLUMN_AUTORUN_NAME, NULL, 
				    COLUMN_AUTORUN_APP_INFO, NULL, 
				    COLUMN_AUTORUN_X_CONTENT_TYPE, NULL,
				    COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_SEP,
				    -1);

		for (l = app_info_list, n = include_ask ? 3 : 2; l != NULL; l = l->next, n++) {
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

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"pixbuf", 0,
					NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", 1,
					NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo_box), combo_box_separator_func, NULL, NULL);

	if (num_apps == 0) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
		gtk_widget_set_sensitive (combo_box, FALSE);
	} else {
		if (pref_ask && include_ask) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
		} else if (pref_ignore) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), include_ask ? 1 : 0);
		} else if (set_active != -1) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), set_active);
		} else {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), include_ask ? 1 : 0);
		}

		data = g_new0 (NautilusAutorunComboBoxData, 1);
		data->update_settings = update_settings;
		data->changed_cb = changed_cb;
		data->user_data = user_data;

		g_signal_connect (G_OBJECT (combo_box),
				  "changed",
				  G_CALLBACK (combo_box_changed),
				  data);

		/* TODO: unref 'data' when combo box goes bye-bye */
	}
}

#include <X11/XKBlib.h>

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
_dir_exists (GFile *mount_root, const char *dirname)
{
	GFile *file;
	GFileInfo *file_info;
	
	file = g_file_get_child (mount_root, dirname);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_NAME,
				       G_FILE_QUERY_INFO_NONE,
				       NULL,
				       NULL);
	if (file_info != NULL) {
		g_object_unref (file_info);
	}
	g_object_unref (file);

	return file_info != NULL;
}

static char **
_g_mount_guess_content_type_for_mount_root (GFile               *mount_root,
					    GError             **error)
{
	char **ret;
	GPtrArray *types;
	
	types = g_ptr_array_new ();
	
	/* TODO: analyze mount_root and add more content types as needed */

	if (g_file_has_uri_scheme (mount_root, "cdda")) {
		g_ptr_array_add (types, g_strdup ("x-content/audio-cdda"));
		goto no_sniff;
	}
	
	if (_dir_exists (mount_root, "DCIM") ||
	    _dir_exists (mount_root, "dcim")) {
		g_ptr_array_add (types, g_strdup ("x-content/image-dcf"));
	}
	
	if (_dir_exists (mount_root, "VIDEO_TS") ||
	    _dir_exists (mount_root, "video_ts")) {
		g_ptr_array_add (types, g_strdup ("x-content/video-dvd"));
	}

no_sniff:
	
	if (types->len == 0) {
		ret = NULL;
		g_ptr_array_free (types, TRUE);
	} else {
		g_ptr_array_add (types, NULL);
		ret = (char **) g_ptr_array_free (types, FALSE);
	}
	
	return ret;
}

typedef struct {
	char **guessed_content_type;
} GuessContentData;

static void
guess_content_thread (GSimpleAsyncResult *res,
                      GObject            *object,
                      GCancellable       *cancellable)
{
	GuessContentData *op;
	GError *error = NULL;
	
	op = g_simple_async_result_get_op_res_gpointer (res);
	
	op->guessed_content_type = _g_mount_guess_content_type_for_mount_root (G_FILE (object), &error);
	
	if (error != NULL) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
	}
}

/**
 * _g_mount_guess_content_type_async:
 * @mount_root: a #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: user data passed to @callback.
 *
 * Like _g_mount_guess_content_type_async() but analyzes a given sub
 * directory instead.
 */
static void
_g_mount_guess_content_type_for_mount_root_async (GFile               *mount_root,
						  GCancellable        *cancellable,
						  GAsyncReadyCallback  callback,
						  gpointer             user_data)
{
	GSimpleAsyncResult *res;
	GuessContentData *op;
	
	op = g_new0 (GuessContentData, 1);
	res = g_simple_async_result_new (G_OBJECT (mount_root), 
					 callback, 
					 user_data, 
					 _g_mount_guess_content_type_for_mount_root_async);
	g_simple_async_result_set_op_res_gpointer (res, op, g_free);
	
	g_simple_async_result_run_in_thread (res, guess_content_thread, G_PRIORITY_DEFAULT, cancellable);
	g_object_unref (res);
}

/**
 * _g_mount_guess_content_type_for_mount_root_finish:
 * @mount_root: a #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Like _g_mount_guess_content_type_finish() but analyzes a given sub
 * directory instead.
 * 
 * Returns: a %NULL terminated array of content types or %NULL on
 * error. Caller should free this array with g_strfreev() when done
 * with it.
 **/
static char **
_g_mount_guess_content_type_for_mount_root_finish (GFile               *mount_root,
						   GAsyncResult        *result,
						   GError             **error)
{
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
	GuessContentData *op;
	
	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == _g_mount_guess_content_type_for_mount_root_async);
	
	op = g_simple_async_result_get_op_res_gpointer (simple);
	return op->guessed_content_type;
}


/*- END MOVE TO GIO ---*/

typedef struct
{
	GMount *mount;
	gboolean should_eject;

	gboolean selected_ignore;
	GAppInfo *selected_app;

	gboolean remember;

	char *x_content_type;
} AutorunDialogData;

static void
autorun_launch_for_mount (GMount *mount, GAppInfo *app_info)
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

static void 
autorun_dialog_response (GtkDialog *dialog, gint response, AutorunDialogData *data)
{
	switch (response) {
	case GTK_RESPONSE_NONE:
		/* window was closed */
		break;
	case 0:
		/* eject/unmount */
		nautilus_file_operations_unmount_mount (NULL,
							data->mount,
							data->should_eject,
							FALSE);
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	case GTK_RESPONSE_OK:
		/* do the selected action */

		if (data->remember) {
			/* make sure we don't ask again */
			nautilus_autorun_set_preferences (data->x_content_type, FALSE, data->selected_ignore);
			if (!data->selected_ignore && data->selected_app != NULL) {
				g_app_info_set_as_default_for_type (data->selected_app,
								    data->x_content_type,
								    NULL);
			}
		} else {
			/* make sure we do ask again */
			nautilus_autorun_set_preferences (data->x_content_type, TRUE, FALSE);
		}

		if (!data->selected_ignore && data->selected_app != NULL) {
			autorun_launch_for_mount (data->mount, data->selected_app);
		}
		break;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	if (data->selected_app != NULL) {
		g_object_unref (data->selected_app);
	}
	g_object_unref (data->mount);
	g_free (data->x_content_type);
	g_free (data);
}

static void
autorun_combo_changed (gboolean selected_ask,
		       gboolean selected_ignore,
		       GAppInfo *selected_app,
		       gpointer user_data)
{
	AutorunDialogData *data = user_data;

	if (data->selected_app != NULL) {
		g_object_unref (data->selected_app);
	}
	data->selected_app = selected_app != NULL ? g_object_ref (selected_app) : NULL;
	data->selected_ignore = selected_ignore;
}


static void
autorun_always_toggled (GtkToggleButton *togglebutton, AutorunDialogData *data)
{
	data->remember = gtk_toggle_button_get_active (togglebutton);
}


static void
do_autorun_for_content_type (GMount *mount, const char *x_content_type)
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
	char *media_greeting;

	mount_name = NULL;

	user_forced_dialog = is_shift_pressed ();

	nautilus_autorun_get_preferences (x_content_type, &pref_ask, &pref_ignore);

	if (user_forced_dialog) {
		goto show_dialog;
	}

	if (!pref_ask && !pref_ignore) {
		GAppInfo *app_info;
		app_info = g_app_info_get_default_for_type (x_content_type, FALSE);
		if (app_info != NULL) {
			autorun_launch_for_mount (mount, app_info);
		}
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


	if (strcmp (x_content_type, "x-content/audio-cdda") == 0) {
		media_greeting = _("You have just inserted an Audio CD");
	} else if (strcmp (x_content_type, "x-content/video-dvd") == 0) {
		media_greeting = _("You have just inserted a Video DVD");
	} else if (strcmp (x_content_type, "x-content/video-vcd") == 0) {
		media_greeting = _("You have just inserted a Video CD");
	} else if (strcmp (x_content_type, "x-content/video-svcd") == 0) {
		media_greeting = _("You have just inserted a Super Video CD");
	} else if (strcmp (x_content_type, "x-content/image-dcf") == 0) {
		media_greeting = _("You have just inserted media with digital photos");
	} else if (strcmp (x_content_type, "x-content/blank-media") == 0) {
		media_greeting = _("You have just inserted blank media");
	} else {
		media_greeting = _("You have just inserted media");
	}
	markup = g_strdup_printf ("<big><b>%s. %s</b></big>", media_greeting, _("Choose what application to launch."));
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
	data->mount = g_object_ref (mount);
	data->remember = !pref_ask;
	data->selected_ignore = pref_ignore;
	data->x_content_type = g_strdup (x_content_type);
	data->selected_app = g_app_info_get_default_for_type (x_content_type, FALSE);

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
		eject_button = gtk_button_new_with_mnemonic (_("_Eject"));
		GtkWidget *eject_image;
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
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), eject_button, 0);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area), eject_button, TRUE);

	/* show the dialog */
	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog),
			  "response",
			  G_CALLBACK (autorun_dialog_response),
			  data);

out:
	g_free (mount_name);
}

static void
autorun_guessed_content_type_callback (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	GError *error;
	char **guessed_content_type;
	GMount *mount = G_MOUNT (user_data);
	
	error = NULL;
	guessed_content_type = _g_mount_guess_content_type_for_mount_root_finish (G_FILE (source_object), res, &error);
	if (error != NULL) {
		g_warning ("Unabled to guess content type for mount: %s", error->message);
		g_error_free (error);
	} else {
		if (guessed_content_type != NULL) {
			int n;
			for (n = 0; guessed_content_type[n] != NULL; n++) {
				do_autorun_for_content_type (mount, guessed_content_type[n]);
			}
			g_strfreev (guessed_content_type);
		}
	}

	g_object_unref (mount);
}

void
nautilus_autorun (GMount *mount)
{
	GFile *root;
		
	/* TODO: only do this for local mounts */

	/* Sniff the newly added mount to generate x-content/ types;
	 * we do this asynchronously (in another thread) since it
	 * requires doing I/O.
	 */
	root = g_mount_get_root (mount);
	_g_mount_guess_content_type_for_mount_root_async (root,
							  NULL,
							  autorun_guessed_content_type_callback,
							  g_object_ref (mount));
	g_object_unref (root);
}

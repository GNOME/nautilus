/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-window-menus.h - implementation of nautilus window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>

#include <locale.h> 

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-connect-server-dialog.h"
#include "nautilus-file-management-properties.h"
#include "nautilus-property-browser.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-bookmarks.h"
#include "nautilus-window-private.h"
#include "nautilus-desktop-window.h"
#include "nautilus-search-bar.h"
#include <gtk/gtkmain.h>
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkversion.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-help.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <string.h>

#define MENU_PATH_EXTENSION_ACTIONS                     "/MenuBar/File/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/background/Before Zoom Items/Extension Actions"

#define NETWORK_URI          "network:"
#define COMPUTER_URI         "computer:"
#define BURN_CD_URI          "burn:"

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NautilusBookmark *bookmark;
        NautilusWindow *window;
        guint changed_handler_id;
	NautilusBookmarkFailedCallback failed_callback;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NautilusBookmark *bookmark, 
		     NautilusWindow *window,
		     GCallback refresh_callback,
		     NautilusBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	new_bookmark_holder->failed_callback = failed_callback;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	g_object_ref (bookmark);
	new_bookmark_holder->changed_handler_id = 
		g_signal_connect_object (bookmark, "appearance_changed",
					 refresh_callback,
					 window, G_CONNECT_SWAPPED);

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	g_signal_handler_disconnect (bookmark_holder->bookmark,
				     bookmark_holder->changed_handler_id);
	g_object_unref (bookmark_holder->bookmark);
	g_free (bookmark_holder);
}

static void
bookmark_holder_free_cover (gpointer callback_data, GClosure *closure)
{
	bookmark_holder_free (callback_data);
}

static void
activate_bookmark_in_menu_item (GtkAction *action, gpointer user_data)
{
        BookmarkHolder *holder;
        GFile *location;

        holder = (BookmarkHolder *)user_data;

	if (nautilus_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		holder->failed_callback (holder->window, holder->bookmark);
	} else {
	        location = nautilus_bookmark_get_location (holder->bookmark);
	        nautilus_window_go_to (holder->window, location);
	        g_object_unref (location);
        }
}

void
nautilus_menus_append_bookmark_to_menu (NautilusWindow *window, 
					NautilusBookmark *bookmark, 
					const char *parent_path,
					const char *parent_id,
					guint index_in_parent,
					GtkActionGroup *action_group,
					guint merge_id,
					GCallback refresh_callback,
					NautilusBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *bookmark_holder;
	char action_name[128];
	char *name;
	GdkPixbuf *pixbuf;
	GtkAction *action;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	bookmark_holder = bookmark_holder_new (bookmark, window, refresh_callback, failed_callback);
	name = nautilus_bookmark_get_name (bookmark);

	/* Create menu item with pixbuf */
	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, GTK_ICON_SIZE_MENU);

	g_snprintf (action_name, sizeof (action_name), "%s%d", parent_id, index_in_parent);

	action = gtk_action_new (action_name,
				 name,
				 _("Go to the location specified by this bookmark"),
				 NULL);
	
	g_object_set_data_full (G_OBJECT (action), "menu-icon",
				g_object_ref (pixbuf),
				g_object_unref);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (activate_bookmark_in_menu_item),
			       bookmark_holder, 
			       bookmark_holder_free_cover, 0);

	gtk_action_group_add_action (action_group,
				     GTK_ACTION (action));

	g_object_unref (action);

	gtk_ui_manager_add_ui (window->details->ui_manager,
			       merge_id,
			       parent_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_object_unref (pixbuf);
	g_free (name);
}

static void
action_close_window_callback (GtkAction *action, 
			      gpointer user_data)
{
	nautilus_window_close (NAUTILUS_WINDOW (user_data));
}

static void
action_connect_to_server_callback (GtkAction *action, 
				   gpointer user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);
	GtkWidget *dialog;
	GFile *location;
	location = nautilus_window_get_location (window);
	dialog = nautilus_connect_server_dialog_new (window, location);
	if (location) {
		g_object_unref (location);
	}

	gtk_widget_show (dialog);
}

static gboolean
have_burn_uri (void)
{
	static gboolean initialized = FALSE;
	static gboolean res;
	GVfs *vfs;
	int i;
	const gchar * const * supported_uri_schemes;

	if (!initialized) {
		vfs = g_vfs_get_default ();
		supported_uri_schemes = g_vfs_get_supported_uri_schemes (vfs);

		res = FALSE;
		for (i = 0; supported_uri_schemes != NULL && supported_uri_schemes[i] != NULL; i++) {
			if (strcmp ("burn", supported_uri_schemes[i]) == 0) {
				res = TRUE;
				break;
			}
		}
		initialized = TRUE;
	}
	return res;
}

static void
action_stop_callback (GtkAction *action, 
		      gpointer user_data)
{
	nautilus_window_stop_loading (NAUTILUS_WINDOW (user_data));
}

static void
action_undo_callback (GtkAction *action, 
		      gpointer user_data) 
{
	nautilus_undo_manager_undo
		(NAUTILUS_WINDOW (user_data)->application->undo_manager);
}

static void
action_home_callback (GtkAction *action, 
		      gpointer user_data) 
{
	nautilus_window_go_home (NAUTILUS_WINDOW (user_data));
}

static void
action_go_to_computer_callback (GtkAction *action, 
				gpointer user_data) 
{
	GFile *computer;
	computer = g_file_new_for_uri (COMPUTER_URI);
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       computer);
	g_object_unref (computer);
}

static void
action_go_to_network_callback (GtkAction *action, 
				gpointer user_data) 
{
	GFile *network;
	network = g_file_new_for_uri (NETWORK_URI);
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       network);
	g_object_unref (network);
}

static void
action_go_to_templates_callback (GtkAction *action,
				 gpointer user_data) 
{
	char *path;
	GFile *location;

	path = nautilus_get_templates_directory ();
	location = g_file_new_for_path (path);
	g_free (path);
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       location);
	g_object_unref (location);
}

static void
action_go_to_trash_callback (GtkAction *action, 
			     gpointer user_data) 
{
	GFile *trash;
	trash = g_file_new_for_uri ("trash:///");
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       trash);
	g_object_unref (trash);
}

static void
action_go_to_burn_cd_callback (GtkAction *action,
			       gpointer user_data) 
{
	GFile *burn;
	burn = g_file_new_for_uri (BURN_CD_URI);
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       burn);
	g_object_unref (burn);
	
}

static void
action_reload_callback (GtkAction *action, 
			gpointer user_data) 
{
	nautilus_window_reload (NAUTILUS_WINDOW (user_data));
}

static void
action_zoom_in_callback (GtkAction *action, 
			 gpointer user_data) 
{
	nautilus_window_zoom_in (NAUTILUS_WINDOW (user_data));
}

static void
action_zoom_out_callback (GtkAction *action, 
			  gpointer user_data) 
{
	nautilus_window_zoom_out (NAUTILUS_WINDOW (user_data));
}

static void
action_zoom_normal_callback (GtkAction *action, 
			     gpointer user_data) 
{
	nautilus_window_zoom_to_default (NAUTILUS_WINDOW (user_data));
}

static void
preferences_respond_callback (GtkDialog *dialog,
			      gint response_id)
{
	if (response_id == GTK_RESPONSE_CLOSE) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static void
action_preferences_callback (GtkAction *action, 
			     gpointer user_data)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nautilus_file_management_properties_dialog_show (G_CALLBACK (preferences_respond_callback), window);
}

static void
action_backgrounds_and_emblems_callback (GtkAction *action, 
					 gpointer user_data)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nautilus_property_browser_show (gtk_window_get_screen (window));
}

static void
action_about_nautilus_callback (GtkAction *action,
				gpointer user_data)
{
	const gchar *authors[] = {
		"Alexander Larsson",
		"Ali Abdin",
		"Anders Carlsson",
		"Andy Hertzfeld",
		"Arlo Rose",
		"Darin Adler",
		"David Camp",
		"Eli Goldberg",
		"Elliot Lee",
		"Eskil Heyn Olsen",
		"Ettore Perazzoli",
		"Gene Z. Ragan",
		"George Lebl",
		"Ian McKellar",
		"J Shane Culpepper",
		"James Willcox",
		"Jan Arne Petersen",
		"John Harper",
		"John Sullivan",
		"Josh Barrow",
		"Maciej Stachowiak",
		"Mark McLoughlin",
		"Mathieu Lacage",
		"Mike Engber",
		"Mike Fleming",
		"Pavel Cisler",
		"Ramiro Estrugo",
		"Raph Levien",
		"Rebecca Schulman",
		"Robey Pointer",
		"Robin * Slomkowski",
		"Seth Nickell",
		"Susan Kare",
		NULL
	};
	const gchar *documenters[] = {
		"GNOME Documentation Team",
		"Sun Microsystem",
		NULL
	};
	const gchar *license[] = {
		N_("Nautilus is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("Nautilus is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Nautilus; if not, write to the Free Software Foundation, Inc., "
		   "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA")
	};
	gchar *license_trans;

	license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]),
					     _(license[2]), NULL);

	gtk_show_about_dialog (GTK_WINDOW (user_data),
#if GTK_CHECK_VERSION (2, 11, 0)
			       "program-name", _("Nautilus"),
#else
			       "name", _("Nautilus"),
#endif /* GTK 2.11. 0 */
			       "version", VERSION,
			       "comments", _("Nautilus is a graphical shell "
					     "for GNOME that makes it "
					     "easy to manage your files "
					     "and the rest of your system."),
			       "copyright", _("Copyright \xC2\xA9 1999-2007 "
					      "The Nautilus authors"),
			       "license", license_trans,
			       "wrap-license", TRUE,
			       "authors", authors,
			       "documenters", documenters,
				/* Translators should localize the following string
				 * which will be displayed at the bottom of the about
				 * box to give credit to the translator(s).
				 */
			      "translator-credits", _("translator-credits"),
			      "logo-icon-name", "nautilus",
			      "website", "http://www.gnome.org/projects/nautilus",
			      "website-label", _("Nautilus Web Site"),
			      NULL);

	g_free (license_trans);

}

static void
action_up_callback (GtkAction *action, 
		     gpointer user_data) 
{
	nautilus_window_go_up (NAUTILUS_WINDOW (user_data), FALSE);
}

static void
action_nautilus_manual_callback (GtkAction *action, 
				 gpointer user_data)
{
	NautilusWindow *window;
	GError *error;
	GtkWidget *dialog;

	error = NULL;
	window = NAUTILUS_WINDOW (user_data);

	if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		gdk_spawn_command_line_on_screen (
			gtk_window_get_screen (GTK_WINDOW (window)),
			"gnome-help", &error);
	} else {
		gnome_help_display_desktop_on_screen (
			NULL, "user-guide", "user-guide.xml", "gosnautilus-1",
			gtk_window_get_screen (GTK_WINDOW (window)), &error);
	}

	if (error) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was an error displaying help: \n%s"),
						 error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
		     NautilusWindow *window)
{
	GtkAction *action;
	char *message;

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
	g_return_if_fail (action != NULL);
	
	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar),
				    window->details->help_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
		       NautilusWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar),
			   window->details->help_message_cid);
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
		     GtkAction *action,
		     GtkWidget *proxy,
		     NautilusWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
connect_proxy_cb (GtkUIManager *manager,
		  GtkAction *action,
		  GtkWidget *proxy,
		  NautilusWindow *window)
{
	GdkPixbuf *icon;
	GtkWidget *widget;
	
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb), window);


		/* This is a way to easily get pixbufs into the menu items */
		icon = g_object_get_data (G_OBJECT (action), "menu-icon");
		if (icon != NULL) {
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy),
						       gtk_image_new_from_pixbuf (icon));
		}
	}
	if (GTK_IS_TOOL_BUTTON (proxy)) {
		icon = g_object_get_data (G_OBJECT (action), "toolbar-icon");
		if (icon != NULL) {
			widget = gtk_image_new_from_pixbuf (icon);
			gtk_widget_show (widget);
			gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (proxy),
							 widget);
		}
	}
	
}

static const GtkActionEntry main_entries[] = {
  /* name, stock id, label */  { "File", NULL, N_("_File") },
  /* name, stock id, label */  { "Edit", NULL, N_("_Edit") },
  /* name, stock id, label */  { "View", NULL, N_("_View") },
  /* name, stock id, label */  { "Help", NULL, N_("_Help") },
  /* name, stock id */         { "Close", GTK_STOCK_CLOSE,
  /* label, accelerator */       N_("_Close"), "<control>W",
  /* tooltip */                  N_("Close this folder"),
                                 G_CALLBACK (action_close_window_callback) },
                               { "Backgrounds and Emblems", NULL,
                                 N_("_Backgrounds and Emblems..."),               
                                 NULL, N_("Display patterns, colors, and emblems that can be used to customize appearance"),
                                 G_CALLBACK (action_backgrounds_and_emblems_callback) },
                               { "Preferences", GTK_STOCK_PREFERENCES,
                                 N_("Prefere_nces"),               
                                 NULL, N_("Edit Nautilus preferences"),
                                 G_CALLBACK (action_preferences_callback) },
  /* name, stock id, label */  { "Undo", NULL, N_("_Undo"),
                                 "<control>Z", N_("Undo the last text change"),
                                 G_CALLBACK (action_undo_callback) },
  /* name, stock id, label */  { "Up", GTK_STOCK_GO_UP, N_("Open _Parent"),
                                 "<alt>Up", N_("Open the parent folder"),
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id, label */  { "UpAccel", NULL, "UpAccel",
                                 "", NULL,
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id */         { "Stop", GTK_STOCK_STOP,
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop loading the current location"),
                                 G_CALLBACK (action_stop_callback) },
  /* name, stock id */         { "Reload", GTK_STOCK_REFRESH,
  /* label, accelerator */       N_("_Reload"), "<control>R",
  /* tooltip */                  N_("Reload the current location"),
                                 G_CALLBACK (action_reload_callback) },
  /* name, stock id */         { "Nautilus Manual", GTK_STOCK_HELP,
  /* label, accelerator */       N_("_Contents"), "F1",
  /* tooltip */                  N_("Display Nautilus help"),
                                 G_CALLBACK (action_nautilus_manual_callback) },
  /* name, stock id */         { "About Nautilus", GTK_STOCK_ABOUT,
  /* label, accelerator */       N_("_About"), NULL,
  /* tooltip */                  N_("Display credits for the creators of Nautilus"),
                                 G_CALLBACK (action_about_nautilus_callback) },
  /* name, stock id */         { "Zoom In", GTK_STOCK_ZOOM_IN,
  /* label, accelerator */       N_("Zoom _In"), "<control>plus",
  /* tooltip */                  N_("Show the contents in more detail"),
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "ZoomInAccel", NULL,
  /* label, accelerator */       "ZoomInAccel", "<control>equal",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "ZoomInAccel2", NULL,
  /* label, accelerator */       "ZoomInAccel2", "<control>KP_Add",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "Zoom Out", GTK_STOCK_ZOOM_OUT,
  /* label, accelerator */       N_("Zoom _Out"), "<control>minus",
  /* tooltip */                  N_("Show the contents in less detail"),
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "ZoomOutAccel", NULL,
  /* label, accelerator */       "ZoomOutAccel", "<control>KP_Subtract",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "Zoom Normal", GTK_STOCK_ZOOM_100,
  /* label, accelerator */       N_("Normal Si_ze"), "<control>0",
  /* tooltip */                  N_("Show the contents at the normal size"),
                                 G_CALLBACK (action_zoom_normal_callback) },
  /* name, stock id */         { "Connect to Server", NULL, 
  /* label, accelerator */       N_("Connect to _Server..."), NULL,
  /* tooltip */                  N_("Connect to a remote computer or shared disk"),
                                 G_CALLBACK (action_connect_to_server_callback) },
  /* name, stock id */         { "Home", NAUTILUS_ICON_HOME,
  /* label, accelerator */       N_("_Home Folder"), "<alt>Home",
  /* tooltip */                  N_("Open your personal folder"),
                                 G_CALLBACK (action_home_callback) },
  /* name, stock id */         { "Go to Computer", NAUTILUS_ICON_COMPUTER,
  /* label, accelerator */       N_("_Computer"), NULL,
  /* tooltip */                  N_("Browse all local and remote disks and folders accessible from this computer"),
                                 G_CALLBACK (action_go_to_computer_callback) },
  /* name, stock id */         { "Go to Network", NAUTILUS_ICON_NETWORK,
  /* label, accelerator */       N_("_Network"), NULL,
  /* tooltip */                  N_("Browse bookmarked and local network locations"),
                                 G_CALLBACK (action_go_to_network_callback) },
  /* name, stock id */         { "Go to Templates", NAUTILUS_ICON_TEMPLATE,
  /* label, accelerator */       N_("T_emplates"), NULL,
  /* tooltip */                  N_("Open your personal templates folder"),
                                 G_CALLBACK (action_go_to_templates_callback) },
  /* name, stock id */         { "Go to Trash", NAUTILUS_ICON_TRASH,
  /* label, accelerator */       N_("_Trash"), NULL,
  /* tooltip */                  N_("Open your personal trash folder"),
                                 G_CALLBACK (action_go_to_trash_callback) },
  /* name, stock id */         { "Go to Burn CD", NAUTILUS_ICON_BURN,
  /* label, accelerator */       N_("CD/_DVD Creator"), NULL,
  /* tooltip */                  N_("Open a folder into which you can drag files to burn to a CD or DVD"),
                                 G_CALLBACK (action_go_to_burn_cd_callback) },
};

/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_window_initialize_menus (NautilusWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	const char *ui;
	
	action_group = gtk_action_group_new ("ShellActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	window->details->main_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      main_entries, G_N_ELEMENTS (main_entries),
				      window);

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_UP);
	g_object_set (action, "short_label", _("_Up"), NULL);

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_HOME);
	g_object_set (action, "short_label", _("_Home"), NULL);

	window->details->ui_manager = gtk_ui_manager_new ();
	ui_manager = window->details->ui_manager;
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (ui_manager));
	
	g_signal_connect (ui_manager, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), window);
	g_signal_connect (ui_manager, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), window);
	
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	ui = nautilus_ui_string_get ("nautilus-shell-ui.xml");
	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, NULL);

	nautilus_window_initialize_bookmarks_menu (window);
}

void
nautilus_window_initialize_menus_constructed (NautilusWindow *window)
{
	GtkAction *action;

	/* Don't call have_burn_uri() for the desktop window, as this is a very
	 * expensive operation during login (around 1 second) ---
	 * have_burn_uri() has to create a "burn:///" URI, which causes
	 * gnome-vfs to link in libmapping.so from nautilus-cd-burner.
	 */
	if (nautilus_window_has_menubar_and_statusbar (window) && !have_burn_uri ()) {
		action = gtk_action_group_get_action (window->details->main_action_group, NAUTILUS_ACTION_GO_TO_BURN_CD);
 		gtk_action_set_visible (action, FALSE);
 	}
}

static GList *
get_extension_menus (NautilusWindow *window)
{
	GList *providers;
	GList *items;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;
		
		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_background_items (provider,
									  GTK_WIDGET (window),
									  window->details->viewed_file);
		items = g_list_concat (items, file_items);
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

void
nautilus_window_load_extension_menus (NautilusWindow *window)
{
	NautilusMenuItem *item;
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *items;
	GList *l;
	int i;
	guint merge_id;

	if (window->details->extensions_menu_merge_id != 0) {
		gtk_ui_manager_remove_ui (window->details->ui_manager,
					  window->details->extensions_menu_merge_id);
		window->details->extensions_menu_merge_id = 0;
	}

	if (window->details->extensions_menu_action_group != NULL) {
		gtk_ui_manager_remove_action_group (window->details->ui_manager,
						    window->details->extensions_menu_action_group);
		window->details->extensions_menu_action_group = NULL;
	}
	
	merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
	window->details->extensions_menu_merge_id = merge_id;
	action_group = gtk_action_group_new ("ExtensionsMenuGroup");
	window->details->extensions_menu_action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (window->details->ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	items = get_extension_menus (window);

	for (l = items, i = 0; l != NULL; l = l->next, i++) {
		item = NAUTILUS_MENU_ITEM (l->data);

		action = nautilus_action_from_menu_item (item);
		gtk_action_group_add_action (action_group,
					     GTK_ACTION (action));
		g_object_unref (action);
		
		gtk_ui_manager_add_ui (window->details->ui_manager,
				       merge_id,
				       MENU_PATH_EXTENSION_ACTIONS,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		gtk_ui_manager_add_ui (window->details->ui_manager,
				       merge_id,
				       POPUP_PATH_EXTENSION_ACTIONS,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		
		g_object_unref (item);
	}

	g_list_free (items);
}



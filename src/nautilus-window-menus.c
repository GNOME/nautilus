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
#include "nautilus-signaller.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-desktop-window.h"
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-help.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-undo-manager.h>

#define MENU_PATH_EXTENSION_ACTIONS                     "/MenuBar/File/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/background/Before Zoom Items/Extension Actions"

#define COMPUTER_URI          "computer:"
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
        char *uri;

        holder = (BookmarkHolder *)user_data;

	if (nautilus_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		holder->failed_callback (holder->window, holder->bookmark);
	} else {
	        uri = nautilus_bookmark_get_uri (holder->bookmark);
	        nautilus_window_go_to (holder->window, uri);
	        g_free (uri);
        }
}

void
nautilus_menus_append_bookmark_to_menu (NautilusWindow *window, 
					NautilusBookmark *bookmark, 
					const char *parent_path,
					guint index_in_parent,
					GtkActionGroup *action_group,
					guint merge_id,
					GCallback refresh_callback,
					NautilusBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *bookmark_holder;		
	char *raw_name, *display_name, *truncated_name, *action_name;
	GdkPixbuf *pixbuf;
	GtkAction *action;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	bookmark_holder = bookmark_holder_new (bookmark, window, refresh_callback, failed_callback);

	/* We double the underscores here to escape them so gtk+ will know they are
	 * not keyboard accelerator character prefixes. If we ever find we need to
	 * escape more than just the underscores, we'll add a menu helper function
	 * instead of a string utility. (Like maybe escaping control characters.)
	 */
	raw_name = nautilus_bookmark_get_name (bookmark);
	truncated_name = eel_truncate_text_for_menu_item (raw_name);
	display_name = eel_str_double_underscores (truncated_name);
	g_free (raw_name);
	g_free (truncated_name);

	/* Create menu item with pixbuf */
	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS, FALSE);

	action_name = g_strdup_printf ("bookmark_%d", index_in_parent);

	action = gtk_action_new (action_name,
				 display_name,
				 _("Go to the location specified by this bookmark"),
				 NULL);
	
	/* TODO: This should really use themed icons and
	   nautilus_bookmark_get_icon (bookmark), but that doesn't work yet*/
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
	g_free (action_name);
	g_free (display_name);
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
	GtkWidget *dialog;
	
	dialog = nautilus_connect_server_dialog_new (NAUTILUS_WINDOW (user_data));
	gtk_widget_show (dialog);
}

static gboolean
have_burn_uri (void)
{
	static gboolean initialized = FALSE;
	static gboolean res;
	GnomeVFSURI *uri;

	if (!initialized) {
		uri = gnome_vfs_uri_new ("burn:///");
		res = uri != NULL;
		if (uri != NULL) {
			gnome_vfs_uri_unref (uri);
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
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       COMPUTER_URI);
}

static void
action_go_to_templates_callback (GtkAction *action,
				 gpointer user_data) 
{
	char *uri;

	nautilus_create_templates_directory ();
	uri = nautilus_get_templates_directory_uri ();
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       uri);
	g_free (uri);
}

static void
action_go_to_trash_callback (GtkAction *action, 
			     gpointer user_data) 
{
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       EEL_TRASH_URI);
}

static void
action_go_to_burn_cd_callback (GtkAction *action,
			       gpointer user_data) 
{
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       BURN_CD_URI);
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
	static GtkWidget *about = NULL;
	const char *authors[] = {
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
	const char *copyright;
	const char *translator_credits;
	const char *locale;

	if (about == NULL) {
		/* We could probably just put a translation in en_US
		 * instead of doing this mess, but I got this working
		 * and I don't feel like fiddling with it any more.
		 */
		locale = setlocale (LC_MESSAGES, NULL);
		if (locale == NULL
		    || strcmp (locale, "C") == 0
		    || strcmp (locale, "POSIX") == 0
		    || strcmp (locale, "en_US") == 0) {
			/* The copyright character here is in UTF-8 */
			copyright = "Copyright \xC2\xA9 1999-2001 Eazel, Inc.";
		} else {
			/* Localize to deal with issues in the copyright
			 * symbol characters -- do not translate the company
			 * name, please.
			 */
			copyright = _("Copyright (C) 1999-2001 Eazel, Inc.");
		}

		/* Translators should localize the following string
		 * which will be displayed at the bottom of the about
		 * box to give credit to the translator(s).
		 */
		translator_credits = (strcmp (_("Translator Credits"), "Translator Credits") == 0) ?
			NULL : _("Translator Credits");
		
		about = gnome_about_new (_("Nautilus"),
					 VERSION,
					 copyright,
					 _("Nautilus is a graphical shell "
					   "for GNOME that makes it "
					   "easy to manage your files "
					   "and the rest of your system."),
					 authors,
					 NULL,
					 translator_credits,
					 NULL);
		gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (user_data));
		
		eel_add_weak_pointer (&about);
	}
	
	gtk_window_present (GTK_WINDOW (about));
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

static GtkActionEntry main_entries[] = {
  { "File", NULL, N_("_File") },               /* name, stock id, label */
  { "Edit", NULL, N_("_Edit") },               /* name, stock id, label */
  { "View", NULL, N_("_View") },               /* name, stock id, label */
  { "Help", NULL, N_("_Help") },               /* name, stock id, label */
  { "Close", GTK_STOCK_CLOSE,                  /* name, stock id */
    N_("_Close"), "<control>W",                /* label, accelerator */
    N_("Close this folder"),                   /* tooltip */ 
    G_CALLBACK (action_close_window_callback) },
  { "Backgrounds and Emblems", NULL,
    N_("_Backgrounds and Emblems..."),               
    NULL, N_("Display patterns, colors, and emblems that can be used to customize appearance"),
    G_CALLBACK (action_backgrounds_and_emblems_callback) },
  { "Preferences", GTK_STOCK_PREFERENCES,
    N_("Prefere_nces"),               
    NULL, N_("Edit Nautilus preferences"),
    G_CALLBACK (action_preferences_callback) },
  { "Undo", NULL, N_("_Undo"),               /* name, stock id, label */
    "<control>Z", N_("Undo the last text change"),
    G_CALLBACK (action_undo_callback) },
  { "Up", GTK_STOCK_GO_UP, N_("Open _Parent"),               /* name, stock id, label */
    "<alt>Up", N_("Open the parent folder"),
    G_CALLBACK (action_up_callback) },
  { "UpAccel", NULL, "UpAccel",               /* name, stock id, label */
    "", NULL,
    G_CALLBACK (action_up_callback) },
  { "Stop", GTK_STOCK_STOP,                        /* name, stock id */
    N_("_Stop"), NULL,           /* label, accelerator */
    NULL,                                      /* tooltip */ 
    G_CALLBACK (action_stop_callback) },
  { "Reload", GTK_STOCK_REFRESH,                        /* name, stock id */
    N_("_Reload"), "<control>R",           /* label, accelerator */
    NULL,                                      /* tooltip */ 
    G_CALLBACK (action_reload_callback) },
  { "Nautilus Manual", GTK_STOCK_HELP,                        /* name, stock id */
    N_("_Contents"), "F1",           /* label, accelerator */
    N_("Display Nautilus help"),                                      /* tooltip */ 
    G_CALLBACK (action_nautilus_manual_callback) },
  { "About Nautilus", GTK_STOCK_ABOUT,                        /* name, stock id */
    N_("_About"), NULL,           /* label, accelerator */
    N_("Display credits for the creators of Nautilus"),                                      /* tooltip */ 
    G_CALLBACK (action_about_nautilus_callback) },
  { "Zoom In", GTK_STOCK_ZOOM_IN,                        /* name, stock id */
    N_("Zoom _In"), "<control>plus",           /* label, accelerator */
    N_("Show the contents in more detail"),                                      /* tooltip */ 
    G_CALLBACK (action_zoom_in_callback) },
  { "Zoom Out", GTK_STOCK_ZOOM_OUT,                        /* name, stock id */
    N_("Zoom _Out"), "<control>minus",           /* label, accelerator */
    N_("Show the contents in less detail"),                                      /* tooltip */ 
    G_CALLBACK (action_zoom_out_callback) },
  { "Zoom Normal", GTK_STOCK_ZOOM_100,                        /* name, stock id */
    N_("Normal Si_ze"), NULL,           /* label, accelerator */
    N_("Show the contents at the normal size"),                                      /* tooltip */ 
    G_CALLBACK (action_zoom_normal_callback) },
  { "Connect to Server", NULL,                        /* name, stock id */
    N_("Connect to _Server..."), NULL,           /* label, accelerator */
    N_("Set up a connection to a network server"),                                      /* tooltip */ 
    G_CALLBACK (action_connect_to_server_callback) },
  { "Home", GTK_STOCK_HOME,                        /* name, stock id */
    N_("_Home"), "<alt>Home",           /* label, accelerator */
    N_("Go to the home folder"),                                  /* tooltip */ 
    G_CALLBACK (action_home_callback) },
  { "Go to Computer", "gnome-fs-client",                        /* name, stock id */
    N_("_Computer"), NULL,           /* label, accelerator */
    N_("Go to the computer location"),                                  /* tooltip */ 
    G_CALLBACK (action_go_to_computer_callback) },
  { "Go to Templates", NULL,                        /* name, stock id */
    N_("T_emplates"), NULL,           /* label, accelerator */
    N_("Go to the templates folder"),                                  /* tooltip */ 
    G_CALLBACK (action_go_to_templates_callback) },
  { "Go to Trash", NULL,                        /* name, stock id */
    N_("_Trash"), NULL,           /* label, accelerator */
    N_("Go to the trash folder"),                                  /* tooltip */ 
    G_CALLBACK (action_go_to_trash_callback) },
  { "Go to Burn CD", NULL,                        /* name, stock id */
    N_("CD _Creator"), NULL,           /* label, accelerator */
    N_("Go to the CD/DVD Creator"),                                  /* tooltip */ 
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

	if (!have_burn_uri ()) {
		action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_GO_TO_BURN_CD);
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

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-window-menus.h - implementation of nautilus window menu operations,
                             split into separate file just for convenience.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmark-parsing.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-property-browser.h"
#include "nautilus-signaller.h"
#include "nautilus-window-private.h"

#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus-extensions/nautilus-user-level-manager.h>
#include <libnautilus-extensions/nautilus-xml-extensions.h>

#include <libnautilus/nautilus-bonobo-ui.h>

/* gnome-XML headers */
#include <parser.h>
#include <xmlmemory.h>


#define STATIC_BOOKMARKS_FILE_NAME	"static_bookmarks.xml"

#define FEEDBACK_LOCATION		"http://www.eazel.com/feedback.html"
#define HELP_INFO_LOCATION		"http://www.eazel.com/info.html"

/*
#define WINDOW_ITEMS_TEST
*/

static GtkWindow *bookmarks_window = NULL;

static void                  activate_bookmark_in_menu_item                 (BonoboUIHandler        *uih,
									     gpointer                user_data,
									     const char             *path);
static void                  append_bookmark_to_menu                        (NautilusWindow         *window,
									     NautilusBookmark *bookmark,
									     const char             *menu_item_path,
									     gboolean		     is_in_bookmarks_menu);
static void		     append_dynamic_bookmarks 			    (NautilusWindow *window);
static void                  remove_bookmarks_after	                    (NautilusWindow         *window,
									     const char             *menu_path,
									     const char             *last_static_item_path);
static NautilusBookmarkList *get_bookmark_list                              (void);
static void                  refresh_go_menu                   		    (NautilusWindow         *window);
static void                  refresh_dynamic_bookmarks            		    (NautilusWindow         *window);
static void		     schedule_refresh_go_menu 		    	    (NautilusWindow 	    *window);
static void		     schedule_refresh_dynamic_bookmarks 		    (NautilusWindow 	    *window);
static void                  edit_bookmarks                                 (NautilusWindow         *window);




/* User level things */
static guint                 convert_menu_path_to_user_level                (const char             *path);
static const char *          convert_user_level_to_menu_path                (guint                   user_level);
static char *                get_customize_user_level_settings_menu_string  (void);
static void                  update_user_level_menu_items                   (NautilusWindow         *window);
static char *                get_customize_user_level_string                (void);
static void		     update_preferences_dialog_title		    (void);

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NautilusBookmark *bookmark;
        NautilusWindow *window;
        gboolean prompt_for_removal;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NautilusBookmark *bookmark, 
		     NautilusWindow *window,
		     gboolean prompt_for_removal)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	gtk_object_ref (GTK_OBJECT (bookmark));
	new_bookmark_holder->prompt_for_removal = prompt_for_removal;

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	gtk_object_unref (GTK_OBJECT (bookmark_holder->bookmark));
	g_free (bookmark_holder);
}

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only or because we expect to change them and
 * don't want other code relying on their existence.
 */

#define NAUTILUS_MENU_PATH_CUSTOMIZE_ITEM			"/Edit/Customize"

#define NAUTILUS_MENU_PATH_USER_LEVEL				"/UserLevel"
#define NAUTILUS_MENU_PATH_NOVICE_ITEM				"/UserLevel/Novice"
#define NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM			"/UserLevel/Intermediate"
#define NAUTILUS_MENU_PATH_EXPERT_ITEM				"/UserLevel/Expert"
#define NAUTILUS_MENU_PATH_AFTER_USER_LEVEL_SEPARATOR		"/UserLevel/After User Level Separator"
#define NAUTILUS_MENU_PATH_USER_LEVEL_CUSTOMIZE			"/UserLevel/User Level Customize"

#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_CANNED_BOOKMARKS	"/Bookmarks/Before Canned Separator"

#ifdef WINDOW_ITEMS_TEST
#define NAUTILUS_MENU_PATH_AFTER_CUSTOMIZE_SEPARATOR		"/Settings/After Customize Separator"
#define NAUTILUS_MENU_PATH_TOOLBAR_ITEM				"/Settings/Toolbar"
#define NAUTILUS_MENU_PATH_LOCATIONBAR_ITEM			"/Settings/Locationbar"
#define NAUTILUS_MENU_PATH_STATUSBAR_ITEM			"/Settings/Statusbar"
#define NAUTILUS_MENU_PATH_SIDEBAR_ITEM				"/Settings/Sidebar"
#define NAUTILUS_MENU_PATH_NORMALIZE_ITEM			"/Settings/Normalize"
#endif

static void
file_menu_new_window_callback (BonoboUIHandler *ui_handler, 
			       gpointer user_data, 
			       const char *path)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;
	
	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_window (current_window->application);
	nautilus_window_goto_uri (new_window, current_window->location);
	gtk_widget_show (GTK_WIDGET (new_window));	
}

static void
file_menu_close_window_callback (BonoboUIHandler *ui_handler, 
			         gpointer user_data, 
			         const char *path)
{
        g_assert (NAUTILUS_IS_WINDOW (user_data));
	nautilus_window_close (NAUTILUS_WINDOW (user_data));
}

static void
file_menu_close_all_windows_callback (BonoboUIHandler *ui_handler, 
			         gpointer user_data, 
			         const char *path)
{
	nautilus_application_close_all_windows ();
}

static void
edit_menu_undo_callback (BonoboUIHandler *ui_handler, 
		  	 gpointer user_data, 
		  	 const char *path) 
{
	nautilus_undo_manager_undo
		(NAUTILUS_WINDOW (user_data)->application->undo_manager);
}

static void
edit_menu_cut_callback (BonoboUIHandler *ui_handler, 
		  	gpointer user_data, 
		  	const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (window->focus_widget));
	}
}

static void
edit_menu_copy_callback (BonoboUIHandler *ui_handler, 
		   	 gpointer user_data,
		   	 const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (window->focus_widget));
	}
}


static void
edit_menu_paste_callback (BonoboUIHandler *ui_handler, 
		    	  gpointer user_data,
		    	  const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (window->focus_widget));
	}

}


static void
edit_menu_clear_callback (BonoboUIHandler *ui_handler, 
		    	  gpointer user_data,
		    	  const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		/* A negative index deletes until the end of the string */
		gtk_editable_delete_text (GTK_EDITABLE (window->focus_widget),0, -1);
	}

}

static void
go_menu_back_callback (BonoboUIHandler *ui_handler, 
		       gpointer user_data,
		       const char *path) 
{
	nautilus_window_go_back (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_forward_callback (BonoboUIHandler *ui_handler, 
		       	  gpointer user_data,
		          const char *path) 
{
	nautilus_window_go_forward (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_up_callback (BonoboUIHandler *ui_handler, 
		     gpointer user_data,
		     const char *path) 
{
	nautilus_window_go_up (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_home_callback (BonoboUIHandler *ui_handler, 
		       gpointer user_data,
		       const char *path) 
{
	nautilus_window_go_home (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_add_bookmark_callback (BonoboUIHandler *ui_handler, 
		       		      gpointer user_data,
		      		      const char *path)
{
        nautilus_window_add_bookmark_for_current_location (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_edit_bookmarks_callback (BonoboUIHandler *ui_handler, 
		       		      	gpointer user_data,
		      		      	const char *path)
{
        edit_bookmarks (NAUTILUS_WINDOW (user_data));
}

static void
user_level_customize_callback (BonoboUIHandler *ui_handler, 
					     gpointer user_data,
					     const char *path)
{
	nautilus_global_preferences_show_dialog ();
}

static void
customize_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	nautilus_property_browser_show ();
}


#ifdef WINDOW_ITEMS_TEST
static void
settings_menu_toolbar_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	NautilusWindow *window;
	window = NAUTILUS_WINDOW (user_data);

	if (bonobo_ui_handler_menu_get_toggle_state (ui_handler, path) == TRUE) {
		nautilus_window_show_toolbar (NAUTILUS_WINDOW (window));
	} else {
		nautilus_window_hide_toolbar (NAUTILUS_WINDOW (window));
	}
}

static void
settings_menu_locationbar_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	NautilusWindow *window;
	window = NAUTILUS_WINDOW (user_data);

	if (bonobo_ui_handler_menu_get_toggle_state (ui_handler, path) == TRUE) {
		nautilus_window_show_locationbar (NAUTILUS_WINDOW (window));
	} else {
		nautilus_window_hide_locationbar (NAUTILUS_WINDOW (window));
	}
}

static void
settings_menu_statusbar_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	NautilusWindow *window;
	window = NAUTILUS_WINDOW (user_data);
	
	if (bonobo_ui_handler_menu_get_toggle_state (ui_handler, path) == TRUE) {
		nautilus_window_show_statusbar (NAUTILUS_WINDOW (window));
	} else {
		nautilus_window_hide_statusbar (NAUTILUS_WINDOW (window));
	}
}

static void
settings_menu_sidebar_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	NautilusWindow *window;
	window = NAUTILUS_WINDOW (user_data);

	if (bonobo_ui_handler_menu_get_toggle_state (ui_handler, path) == TRUE) {
		nautilus_window_show_sidebar (NAUTILUS_WINDOW (window));
	} else {
		nautilus_window_hide_sidebar (NAUTILUS_WINDOW (window));
	}
}

static void
settings_menu_normalize_menu_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	NautilusWindow *window;
	GnomeApp *app;
	GnomeDockItem *dock_item;
	
	window = NAUTILUS_WINDOW (user_data);
	app = GNOME_APP (window);

	if (bonobo_ui_handler_menu_get_toggle_state (ui_handler, path) == TRUE) {
		if (app->menubar != NULL) {
			gtk_widget_set_usize(app->menubar, 0, app->menubar->allocation.height - 6);
		}
	} else {
		if (app->menubar != NULL) {
			gtk_widget_set_usize(app->menubar, 0, app->menubar->allocation.height + 6);
		}
	}

	dock_item = gnome_app_get_dock_item_by_name (app, GNOME_APP_MENUBAR_NAME);
	gtk_widget_queue_resize (GTK_WIDGET (dock_item)->parent);
}

#endif


static void
help_menu_about_nautilus_callback (BonoboUIHandler *ui_handler, 
		       		   gpointer user_data,
		      		   const char *path)
{
	static GtkWidget *aboot = NULL;

	if (aboot == NULL) {
		char *about_string;

		const char *authors[] = {
			"Darin Adler",
			"Pavel Císler",
			"Ramiro Estrugo",
			"Andy Hertzfeld",
			"Susan Kare",
			"Elliot Lee",
			"Ettore Perazzoli",
			"Gene Ragan",
			"Arlo Rose",
			"Rebecca Schulman",
			"Maciej Stachowiak",
			"John Sullivan",
			NULL
		};

		about_string = g_strdup_printf (_("The Gnome Shell\n%s"),
				       		NAUTILUS_TIMESTAMP);

		aboot = gnome_about_new(_("Nautilus"),
					VERSION,
					"Copyright (C) 1999, 2000",
					authors,
					about_string,
					"nautilus/About_Image.png");

		g_free (about_string);

		gnome_dialog_close_hides (GNOME_DIALOG (aboot), TRUE);
	}

	nautilus_gtk_window_present (GTK_WINDOW (aboot));
}

static void
help_menu_feedback_nautilus_callback (BonoboUIHandler *ui_handler, 
		       	       	      gpointer user_data,
		      		      const char *path)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;

	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_window (current_window->application);
	nautilus_window_goto_uri (new_window, FEEDBACK_LOCATION);
	gtk_widget_show (GTK_WIDGET (new_window));

}

static void
help_menu_info_nautilus_callback (BonoboUIHandler *ui_handler, 
		       	          gpointer user_data,
		      		  const char *path)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;

	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_window (current_window->application);
	nautilus_window_goto_uri (new_window, HELP_INFO_LOCATION);
	gtk_widget_show (GTK_WIDGET (new_window));

}

/* utility routine to return an image corresponding to the passed-in user level */

static GdkPixbuf*
get_user_level_image (int user_level, gboolean is_selected)
{
	const char *image_name;
	char *temp_str, *full_image_name;
	GdkPixbuf *pixbuf;
	
	switch (user_level)
	{
		case 0:
			image_name = "novice";
			break;
		case 1:
			image_name = "intermediate";
			break;
		case 2:
			image_name = "expert";
			break;
		default:
			image_name = "intermediate";
			break;
	}
	
	if (is_selected) {
		full_image_name = g_strdup_printf ("%s-selected.png", image_name);
	} else {
		full_image_name = g_strdup_printf ("%s.png", image_name);	
	}
	
	temp_str = nautilus_pixmap_file (full_image_name);
	g_free (full_image_name);
	if (temp_str) {
		pixbuf = gdk_pixbuf_new_from_file (temp_str);
		g_free (temp_str);
	} else {
		pixbuf = NULL;
	}
	
	return pixbuf;
}

/* handle user level changes */

static void
user_level_menu_item_callback (BonoboUIHandler *ui_handler, 
		       		   gpointer user_data,
		      		   const char *path)
{
	GdkPixbuf *pixbuf;
	const char *old_menu_path;
	int old_user_level, new_user_level;
		
	old_user_level = nautilus_user_level_manager_get_user_level ();
	new_user_level = convert_menu_path_to_user_level (path);

	if (old_user_level == new_user_level) {
		return;
	}

	nautilus_user_level_manager_set_user_level (new_user_level);
	
	/* change the item pixbufs to reflect the new user level */
	old_menu_path = convert_user_level_to_menu_path (old_user_level);
	pixbuf = get_user_level_image (old_user_level, FALSE);
	bonobo_ui_handler_menu_set_pixmap (ui_handler, old_menu_path, BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA, pixbuf);
	gdk_pixbuf_unref (pixbuf);
	
	pixbuf = get_user_level_image (new_user_level, TRUE);
	bonobo_ui_handler_menu_set_pixmap (ui_handler, path, BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA, pixbuf);
	gdk_pixbuf_unref (pixbuf);
	
	/* set up the menu title image to reflect the new user level */
	pixbuf = get_user_level_image (new_user_level, FALSE);
	bonobo_ui_handler_menu_set_pixmap (ui_handler, NAUTILUS_MENU_PATH_USER_LEVEL, BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA, pixbuf);
	gdk_pixbuf_unref (pixbuf);
}

static void
remove_bookmarks_for_uri (GtkWidget *button, gpointer callback_data)
{
	const char *uri;

	g_assert (GTK_IS_WIDGET (button));
	g_assert (callback_data != NULL);

	uri = (const char *)callback_data;

	nautilus_bookmark_list_delete_items_with_uri (get_bookmark_list (), uri);
}

static void
show_bogus_bookmark_window (BookmarkHolder *holder)
{
	GnomeDialog *dialog;
	char *uri;
	char *prompt;

	uri = nautilus_bookmark_get_uri (holder->bookmark);

	if (holder->prompt_for_removal) {
		prompt = g_strdup_printf (_("The location \"%s\" does not exist. Do you "
					    "want to remove any bookmarks with this "
					    "location from your list?"), uri);
		dialog = nautilus_yes_no_dialog (prompt,
						 _("Remove"),
						 GNOME_STOCK_BUTTON_CANCEL,
						 GTK_WINDOW (holder->window));

		nautilus_gtk_signal_connect_free_data
			(GTK_OBJECT (nautilus_gnome_dialog_get_button_by_index
				     (dialog, GNOME_OK)),
			 "clicked",
			 remove_bookmarks_for_uri,
			 g_strdup (uri));

		gtk_window_set_title (GTK_WINDOW (dialog), _("Bookmark for Bad Location"));			 
		gnome_dialog_set_default (dialog, GNOME_CANCEL);
	} else {
		prompt = g_strdup_printf (_("The location \"%s\" no longer exists. "
					    "It was probably moved, deleted, or renamed."), uri);
		dialog = nautilus_info_dialog (prompt, GTK_WINDOW (holder->window));
		gtk_window_set_title (GTK_WINDOW (dialog), _("Go To Bad Location"));
	}

	g_free (uri);
	g_free (prompt);
}

static void
activate_bookmark_in_menu_item (BonoboUIHandler *uih, gpointer user_data, const char *path)
{
        BookmarkHolder *holder;
        char *uri;

        holder = (BookmarkHolder *)user_data;

	if (nautilus_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		show_bogus_bookmark_window (holder);
	} else {
	        uri = nautilus_bookmark_get_uri (holder->bookmark);
	        nautilus_window_goto_uri (holder->window, uri);
	        g_free (uri);
        }
}

static void
append_separator (NautilusWindow *window, const char *separator_path)
{
        bonobo_ui_handler_menu_new_separator (window->ui_handler,
                                              separator_path,
                                              -1);
}

static void
append_bookmark_to_menu (NautilusWindow *window, 
                         NautilusBookmark *bookmark, 
                         const char *menu_item_path,
                         gboolean is_bookmarks_menu)
{
	BookmarkHolder *bookmark_holder;	
	GdkPixbuf *pixbuf;
	BonoboUIHandlerPixmapType pixmap_type;
	char *raw_name, *name;

	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS);

	/* Set up pixmap type based on result of function.  If we fail, set pixmap type to none */
	if (pixbuf != NULL) {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA;
	} else {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_NONE;
	}

	bookmark_holder = bookmark_holder_new (bookmark, window, is_bookmarks_menu);

	/* We double the underscores here to escape them so Bonobo will know they are
	 * not keyboard accelerator character prefixes. If we ever find we need to
	 * escape more than just the underscores, we'll add a menu helper function
	 * instead of a string utility. (Like maybe escaping control characters.)
	 */
	raw_name = nautilus_bookmark_get_name (bookmark);
	name = nautilus_str_double_underscores (raw_name);
	g_free (raw_name);
 	bonobo_ui_handler_menu_new_item (window->ui_handler,
					 menu_item_path,
					 name,
					 _("Go to the specified location"),
					 -1,
					 pixmap_type,
					 pixbuf,
					 0,
					 0,
					 NULL,
					 NULL);
	g_free (name);

	/* We must use "set_callback" since we have a destroy-notify function. */
	bonobo_ui_handler_menu_set_callback
		(window->ui_handler, menu_item_path,
		 activate_bookmark_in_menu_item,
		 bookmark_holder, (GDestroyNotify) bookmark_holder_free);

	/* Let's get notified whenever a bookmark changes. */
	gtk_signal_connect_object (GTK_OBJECT (bookmark), "changed",
				   is_bookmarks_menu
				   ? schedule_refresh_dynamic_bookmarks
				   : schedule_refresh_go_menu,
				   GTK_OBJECT (window));
}

static char *
get_static_bookmarks_file_path (void)
{
	char *xml_file_path;
	
	/* For now at least, the static bookmarks file is kept in the standard shared data directory. */
	/* FIXME: The service might want to overwrite this file. Can it do so in this
	 * location?
	 */

	xml_file_path = nautilus_make_path (NAUTILUS_DATADIR, STATIC_BOOKMARKS_FILE_NAME);
	if (g_file_exists (xml_file_path)) {
		return xml_file_path;
	}
	g_free (xml_file_path);

	return NULL;
}

static char *
create_menu_item_from_node (NautilusWindow *window,
			     xmlNodePtr node, 
			     const char *menu_path,
			     int index)
{
	NautilusBookmark *bookmark;
	xmlChar *xml_folder_name;
	int sub_index;
	char *index_as_string;
	char *item_path;
	char *sub_item_path;
	
	index_as_string = g_strdup_printf ("item_%d", index);
	item_path = bonobo_ui_handler_build_path (menu_path, index_as_string, NULL);
	g_free (index_as_string);

	if (strcmp (node->name, "bookmark") == 0) {
		bookmark = nautilus_bookmark_new_from_node (node);
		append_bookmark_to_menu (window, bookmark, item_path, TRUE);
		gtk_object_unref (GTK_OBJECT (bookmark));
	} else if (strcmp (node->name, "separator") == 0) {
		append_separator (window, item_path);
	} else if (strcmp (node->name, "folder") == 0) {
		xml_folder_name = xmlGetProp (node, "name");
	 	bonobo_ui_handler_menu_new_subtree (window->ui_handler,
						    item_path,
						    xml_folder_name,
						    NULL,
						    -1,
						    BONOBO_UI_HANDLER_PIXMAP_NONE,
						    NULL,
						    0,
						    0);
		for (node = nautilus_xml_get_children (node), sub_index = 0;
		     node != NULL;
		     node = node->next, ++sub_index) {
			sub_item_path = create_menu_item_from_node (window, node, item_path, sub_index);
			g_free (sub_item_path);
		}
		xmlFree (xml_folder_name);
	} else {
		g_message ("found unknown node '%s', ignoring", node->name);
	}

	return item_path;
}

static void
append_static_bookmarks (NautilusWindow *window, const char *menu_path)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *file_path;
	char *item_path;
	int index;

	/* Walk through XML tree creating bookmarks, folders, and separators. */
	file_path = get_static_bookmarks_file_path ();

	if (file_path == NULL) {
		return;
	}
	
	doc = xmlParseFile (file_path);
	g_free (file_path);

	node = nautilus_xml_get_root_children (doc);
	index = 0;
	
	if (node != NULL) {
		append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_CANNED_BOOKMARKS);
	}

	for (index = 0; node != NULL; node = node->next, ++index) {
		item_path = create_menu_item_from_node 
			(window, node, menu_path, index);
		if (node->next == NULL) {
			/* Stash away path of last canned bookmark to use when
			 * refreshing menu.
			 */
			window->details->last_static_bookmark_path = item_path;
		} else {
			g_free (item_path);
		}
	}
	
	xmlFreeDoc(doc);
}

/**
 * remove_bookmarks_after
 * 
 * Remove bookmark menu items from the end of this window's menu. Each removed
 * item should have callback data that's either NULL or is a BookmarkHolder *.
 * @window: The NautilusWindow whose menu should be cleaned.
 * @menu_path: The BonoboUIHandler-style path for the menu to be cleaned (e.g. "/Go").
 * @last_retained_item_path: The BonoboUIHandler-style path for the last menu item to
 * leave in place (e.g. "/Go/Home"). All menu items after this one will be removed.
 */
static void
remove_bookmarks_after (NautilusWindow *window, 
                        const char *menu_path, 
                        const char *last_retained_item_path)
{
	GList *children, *p;
	gboolean found_items_to_remove;
	gpointer callback_data;
	BookmarkHolder *bookmark_holder;

	g_assert (NAUTILUS_IS_WINDOW (window));

	children = bonobo_ui_handler_menu_get_child_paths (window->ui_handler, menu_path);
	found_items_to_remove = FALSE;

	for (p = children; p != NULL; p = p->next) {
                if (found_items_to_remove) {
                	bonobo_ui_handler_menu_get_callback (window->ui_handler, p->data, 
                					     NULL, &callback_data, NULL);
                	bookmark_holder = (BookmarkHolder *)callback_data;
			/* bookmark_holder is NULL for separator in Bookmarks menu */
                	if (bookmark_holder != NULL) {
	                	gtk_signal_disconnect_by_data (GTK_OBJECT (bookmark_holder->bookmark), window);
                	}
                        bonobo_ui_handler_menu_remove (window->ui_handler, p->data);
		} else if (strcmp ((const char *) p->data, last_retained_item_path) == 0) {
			found_items_to_remove = TRUE;
		}
	}

	g_assert (found_items_to_remove);
        nautilus_g_list_free_deep (children);
}

static NautilusBookmarkList *
get_bookmark_list (void)
{
        static NautilusBookmarkList *bookmarks = NULL;

        if (bookmarks == NULL) {
                bookmarks = nautilus_bookmark_list_new ();
        }

        return bookmarks;
}

static GtkWindow *
get_or_create_bookmarks_window (GtkObject *undo_manager_source)
{
	if (bookmarks_window == NULL) {
		bookmarks_window = create_bookmarks_window (get_bookmark_list(), undo_manager_source);
	}
	return bookmarks_window;
}

/**
 * nautilus_bookmarks_exiting:
 * 
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
nautilus_bookmarks_exiting (void)
{
	if (bookmarks_window != NULL) {
		nautilus_bookmarks_window_save_geometry (bookmarks_window);
	}
}

/**
 * nautilus_window_add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
void
nautilus_window_add_bookmark_for_current_location (NautilusWindow *window)
{
	NautilusBookmark *bookmark;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_assert_computed_str
		(nautilus_bookmark_get_uri (window->current_location_bookmark), 
		 window->location);

	/* Use the first bookmark in the history list rather than creating a new one. */
	bookmark = window->current_location_bookmark;

	if (!nautilus_bookmark_list_contains (get_bookmark_list (), bookmark)) {
		/* 
		 * Only append bookmark if there's not already a copy of this one in there.
		 * Maybe it would be better to remove the other one and always append?
		 * This won't be a sensible rule if we have hierarchical menus.
		 */
		nautilus_bookmark_list_append (get_bookmark_list (), bookmark);
	}
}

static void
edit_bookmarks (NautilusWindow *window)
{
        nautilus_gtk_window_present
		(get_or_create_bookmarks_window (GTK_OBJECT (window)));
}

static void
refresh_all_bookmarks (NautilusWindow *window)
{
	nautilus_window_remove_bookmarks_menu_items (window);				

	if (nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SHOW_BUILT_IN_BOOKMARKS, 
					      TRUE)) {
		append_static_bookmarks (window, NAUTILUS_MENU_PATH_BOOKMARKS_MENU);
	}

	append_dynamic_bookmarks (window);
}

/**
 * nautilus_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
static void 
nautilus_window_initialize_bookmarks_menu (NautilusWindow *window)
{
	/* Construct the initial set of bookmarks. */
	refresh_all_bookmarks (window);

	/* Recreate static & dynamic part of menu if preference about
	 * showing static bookmarks changes.
	 */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_BUILT_IN_BOOKMARKS,
					   (NautilusPreferencesCallback)refresh_all_bookmarks,
					   window);
		
	/* Recreate dynamic part of menu if bookmark list changes */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (get_bookmark_list ()),
			                       "contents_changed",
			                       schedule_refresh_dynamic_bookmarks,
			   	               GTK_OBJECT (window));

	/* Recreate static & dynamic parts of menu if icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       refresh_all_bookmarks,
					       GTK_OBJECT (window));
}

/**
 * nautilus_window_initialize_go_menu
 * 
 * Wire up signals so we'll be notified when history list changes.
 */
static void 
nautilus_window_initialize_go_menu (NautilusWindow *window)
{
	/* Recreate bookmarks part of menu if history list changes
	 * or if icon theme changes.
	 */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (nautilus_signaller_get_current ()),
			                       "history_list_changed",
			                       schedule_refresh_go_menu,
			   	               GTK_OBJECT (window));
	 
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       schedule_refresh_go_menu,
					       GTK_OBJECT (window));

}

static void
new_top_level_menu (NautilusWindow *window, 
		    const char *menu_path,
		    const char *title)
{
	/* Note that we don't bother with hints for menu titles.
	 * We can revisit this anytime if someone thinks they're useful.
	 */
	bonobo_ui_handler_menu_new_subtree (window->ui_handler,
					    menu_path,
					    title,
					    NULL,
					    -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE,
					    NULL,
					    0,
					    0);
}				    

/*( handler to receive the user_level_changed signal, so we can update the menu and dialog
    when the user level changes */
static void
user_level_changed_callback (GtkObject	*user_level_manager,
			     gpointer	user_data)
{
	g_return_if_fail (user_data != NULL);
	g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

	update_user_level_menu_items (NAUTILUS_WINDOW (user_data));

	/* Hide the customize dialog for novice user level */
	if (nautilus_user_level_manager_get_user_level () == 0) {
		nautilus_global_preferences_hide_dialog ();
	}
	/* Otherwise update its title to reflect the user level */
	else {
		update_preferences_dialog_title ();
	}
}

/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_window_initialize_menus (NautilusWindow *window)
{
        GdkPixbuf *pixbuf;
        int current_user_level;
	BonoboUIHandler *ui_handler;

        ui_handler = window->ui_handler;
        g_assert (ui_handler != NULL);
        
        bonobo_ui_handler_create_menubar (ui_handler);

	/* File menu */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_FILE_MENU, _("_File"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM,
        				 _("_New Window"),
        				 _("Create a new window"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_NEW,
        				 GNOME_KEY_NAME_NEW,
        				 GNOME_KEY_MOD_NEW,
        				 file_menu_new_window_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CLOSE_ITEM,
        				 _("_Close Window"),
        				 _("Close this window"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_CLOSE,
        				 GNOME_KEY_NAME_CLOSE,
        				 GNOME_KEY_MOD_CLOSE,
        				 file_menu_close_window_callback,
        				 window);
        				 
        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CLOSE_ALL_WINDOWS_ITEM,
        				 _("Close All Windows"),
        				 _("Close all windows"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 file_menu_close_all_windows_callback,
        				 NULL);
        				 
	/* Edit menu */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_EDIT_MENU, _("_Edit"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_UNDO_ITEM,
        				 _("_Undo"),
        				 _("Undo the last text change"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_UNDO,
        				 GNOME_KEY_NAME_UNDO,
        				 GNOME_KEY_MOD_UNDO,
        				 edit_menu_undo_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_AFTER_UNDO);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CUT_ITEM,
        				 _("_Cut Text"),
        				 _("Cuts the selected text to the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_CUT,
        				 GNOME_KEY_NAME_CUT,
        				 GNOME_KEY_MOD_CUT,
        				 edit_menu_cut_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_COPY_ITEM,
        				 _("_Copy Text"),
        				 _("Copies the selected text to the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_COPY,
        				 GNOME_KEY_NAME_COPY,
        				 GNOME_KEY_MOD_COPY,
        				 edit_menu_copy_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_PASTE_ITEM,
        				 _("_Paste Text"),
        				 _("Pastes the text stored on the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_PASTE,
        				 GNOME_KEY_NAME_PASTE,
        				 GNOME_KEY_MOD_PASTE,
        				 edit_menu_paste_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CLEAR_ITEM,
        				 _("C_lear Text"),
        				 _("Removes the selected text without putting it on the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 GNOME_KEY_NAME_CLEAR,
        				 GNOME_KEY_MOD_CLEAR,
        				 edit_menu_clear_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_AFTER_CLEAR);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_SELECT_ALL_ITEM,
        				 _("_Select All"),
        				 NULL,	/* No hint since it's insensitive here */
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'A',	/* Keyboard shortcut applies to overriders too */
        				 GDK_CONTROL_MASK,
        				 NULL,
        				 NULL);

	/* Customize */
        /*
	append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_AFTER_SELECT_ALL);
	*/
	
	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CUSTOMIZE_ITEM,
        				 _("_Customize..."),
        				 _("Displays the Property Browser, to add properties to objects and customize appearance"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 customize_callback,
        				 NULL);

	/* Go menu */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_GO_MENU, _("_Go"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_BACK_ITEM,
        				 _("_Back"),
        				 _("Go to the previous visited location"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'B',
        				 GDK_CONTROL_MASK,
        				 go_menu_back_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_FORWARD_ITEM,
        				 _("_Forward"),
        				 _("Go to the next visited location"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'F',
        				 GDK_CONTROL_MASK,
        				 go_menu_forward_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_UP_ITEM,
        				 _("_Up"),
        				 _("Go to the location that contains this one"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'U',
        				 GDK_CONTROL_MASK,
        				 go_menu_up_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_HOME_ITEM,
        				 _("_Home"),
        				 _("Go to the home location"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'H',
        				 GDK_CONTROL_MASK,
        				 go_menu_home_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY);

        
	/* Bookmarks */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_BOOKMARKS_MENU, _("_Bookmarks"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_ADD_BOOKMARK_ITEM,
        				 _("_Add Bookmark"),
        				 _("Add a bookmark for the current location to this menu"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 bookmarks_menu_add_bookmark_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM,
        				 _("_Edit Bookmarks..."),
        				 _("Display a window that allows editing the bookmarks in this menu"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 bookmarks_menu_edit_bookmarks_callback,
        				 window);
#ifdef WINDOW_ITEMS_TEST
	/* Test window modification items */
	bonobo_ui_handler_menu_new_separator (ui_handler,
					      NAUTILUS_MENU_PATH_AFTER_CURSTOMIZE_SEPARATOR,
					      -1);


	bonobo_ui_handler_menu_new_toggleitem (ui_handler,
        				 NAUTILUS_MENU_PATH_TOOLBAR_ITEM,
        				 _("Toolbar"),
        				 _("Show/Hide Stuff"),
        				 -1,
        				 0,
        				 0,
        				 settings_menu_toolbar_callback,
        				 window);
	bonobo_ui_handler_menu_set_toggle_state (ui_handler, NAUTILUS_MENU_PATH_TOOLBAR_ITEM, TRUE);
        				 

	bonobo_ui_handler_menu_new_toggleitem (ui_handler,
        				 NAUTILUS_MENU_PATH_LOCATIONBAR_ITEM,
        				 _("Locationbar"),
        				 _("Show/Hide Stuff"),
        				 -1,
        				 0,
        				 0,
        				 settings_menu_locationbar_callback,
        				 window);        				 
	bonobo_ui_handler_menu_set_toggle_state (ui_handler, NAUTILUS_MENU_PATH_LOCATIONBAR_ITEM, TRUE);

	bonobo_ui_handler_menu_new_toggleitem (ui_handler,
        				 NAUTILUS_MENU_PATH_STATUSBAR_ITEM,
        				 _("Statusbar"),
        				 _("Show/Hide Stuff"),
        				 -1,
        				 0,
        				 0,
        				 settings_menu_statusbar_callback,
        				 window);        				 
	bonobo_ui_handler_menu_set_toggle_state (ui_handler, NAUTILUS_MENU_PATH_STATUSBAR_ITEM, TRUE);

	bonobo_ui_handler_menu_new_toggleitem (ui_handler,
        				 NAUTILUS_MENU_PATH_SIDEBAR_ITEM,
        				 _("Sidebar"),
        				 _("Show/Hide Stuff"),
        				 -1,
        				 0,
        				 0,
        				 settings_menu_sidebar_callback,
        				 window);    
	bonobo_ui_handler_menu_set_toggle_state (ui_handler, NAUTILUS_MENU_PATH_SIDEBAR_ITEM, TRUE);
	
	bonobo_ui_handler_menu_new_toggleitem (ui_handler,
        				 NAUTILUS_MENU_PATH_NORMALIZE_ITEM,
        				 _("Normalize Menu"),
        				 _("Show/Hide Stuff"),
        				 -1,
        				 0,
        				 0,
        				 settings_menu_normalize_menu_callback,
        				 window);            				     				 
	bonobo_ui_handler_menu_set_toggle_state (ui_handler, NAUTILUS_MENU_PATH_NORMALIZE_ITEM, FALSE);        				 
#endif

	/* Help */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_HELP_MENU, _("_Help"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_ABOUT_ITEM,
        				 _("_About Nautilus"),
        				 _("Displays information about the Nautilus program"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_ABOUT,
        				 0,
        				 0,
        				 help_menu_about_nautilus_callback,
        				 NULL);

	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_FEEDBACK_ITEM,
        				 _("_Preview Release Feedback"),
        				 _("Displays a feedback form for the Nautilus Preview"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 help_menu_feedback_nautilus_callback,
        				 window);

	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_HELP_INFO_ITEM,
        				 _("_Preview Release Info"),
        				 _("Displays an information blurb for the Nautilus Preview"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 help_menu_info_nautilus_callback,
        				 window);

        /* Desensitize the items that aren't implemented at this level.
         * Some (hopefully all) will be overridden by implementations by the
         * different content views.
         */
	bonobo_ui_handler_menu_set_sensitivity (ui_handler, 
        				        NAUTILUS_MENU_PATH_SELECT_ALL_ITEM, 
						FALSE);

	/* connect to the user level changed signal, so we can update the menu when the
	   user level changes */
	gtk_signal_connect (GTK_OBJECT (nautilus_user_level_manager_get ()),
			    "user_level_changed",
			    user_level_changed_callback,
			    window);

	/* create the user level menu.  First, the menu item itself */

	current_user_level = nautilus_user_level_manager_get_user_level ();
	pixbuf = get_user_level_image (current_user_level, FALSE);	
	bonobo_ui_handler_menu_new_subtree (ui_handler,
					    NAUTILUS_MENU_PATH_USER_LEVEL,
					    "",
					    NULL,
					    -1,
					    BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA,
					    pixbuf,
					    0,
					    0);
	gdk_pixbuf_unref (pixbuf);
	
	/* now add items for each of the three user levels */
	pixbuf = get_user_level_image (0, current_user_level == 0);
	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_NOVICE_ITEM,
        				 _(" Novice"),
 					 _("Set Novice User Level"),
       				 	 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA,
        				 pixbuf,
        				 0,
        				 0,
        				 user_level_menu_item_callback,
        				 window);
	gdk_pixbuf_unref (pixbuf);

	pixbuf = get_user_level_image (1, current_user_level == 1);
	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM,
        				 _(" Intermediate"),
 					 _("Set Intermediate User Level"),
         				 -1,
         				 BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA,
        				 pixbuf,
       				 	 0,
        				 0,
        				 user_level_menu_item_callback,
        				 window);

	gdk_pixbuf_unref (pixbuf);

	pixbuf = get_user_level_image (2, current_user_level == 2);
	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_EXPERT_ITEM,
        				 _(" Expert"),
 					 _("Set Expert User Level"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA,
        				 pixbuf,
        				 0,
        				 0,
        				 user_level_menu_item_callback,
        				 window);
	gdk_pixbuf_unref (pixbuf);

	bonobo_ui_handler_menu_new_separator (ui_handler,
					      NAUTILUS_MENU_PATH_AFTER_USER_LEVEL_SEPARATOR,
					      -1);

	bonobo_ui_handler_menu_new_item (ui_handler,
					 NAUTILUS_MENU_PATH_USER_LEVEL_CUSTOMIZE,
					 _("Edit Settings..."),
					 _("Edit Settings for the Current User Level"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 user_level_customize_callback,
        				 window);

        update_user_level_menu_items (window);
	
	/* Connect to undo manager so it will handle the menu item. */
	nautilus_undo_manager_set_up_bonobo_ui_handler_undo_item
		(window->application->undo_manager,
		 ui_handler, NAUTILUS_MENU_PATH_UNDO_ITEM,
		 _("_Undo"), _("Undo the last text change"));
	
        nautilus_window_initialize_bookmarks_menu (window);
        nautilus_window_initialize_go_menu (window);
}

void
nautilus_window_remove_bookmarks_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_dynamic_bookmarks_idle_id != 0) {
                gtk_idle_remove (window->details->refresh_dynamic_bookmarks_idle_id);
		window->details->refresh_dynamic_bookmarks_idle_id = 0;
        }
}

void
nautilus_window_remove_go_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_go_menu_idle_id != 0) {
                gtk_idle_remove (window->details->refresh_go_menu_idle_id);
		window->details->refresh_go_menu_idle_id = 0;
        }
}

void
nautilus_window_remove_bookmarks_menu_items (NautilusWindow *window)
{
	remove_bookmarks_after (window, 
				NAUTILUS_MENU_PATH_BOOKMARKS_MENU, 
				NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM);

	window->details->last_static_bookmark_path = NULL;
}

void
nautilus_window_remove_go_menu_items (NautilusWindow *window)
{
	remove_bookmarks_after (window, 
				NAUTILUS_MENU_PATH_GO_MENU, 
				NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY);
}

static void
append_dynamic_bookmarks (NautilusWindow *window)
{
        NautilusBookmarkList *bookmarks;
	guint 	bookmark_count;
	guint	index;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	bookmarks = get_bookmark_list ();

	bookmark_count = nautilus_bookmark_list_length (bookmarks);

        /* Add separator before bookmarks, unless there are no bookmarks. */
        if (bookmark_count > 0) {
	        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_BOOKMARKS);
        }

	/* append new set of bookmarks */
	for (index = 0; index < bookmark_count; ++index)
	{
		char *path;

		path = g_strdup_printf ("/Bookmarks/Bookmark%d", index);
		append_bookmark_to_menu (window, 
		                         nautilus_bookmark_list_item_at (bookmarks, index), 
		                         path,
		                         TRUE); 
                g_free (path);
	}	
}

/**
 * refresh_dynamic_bookmarks:
 * 
 * Refresh user's list of bookmarks at end of Bookmarks menu to match centralized list.
 * @window: The NautilusWindow whose Bookmarks menu will be refreshed.
 **/
static void
refresh_dynamic_bookmarks (NautilusWindow *window)
{
	char *last_static_item;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_window_remove_bookmarks_menu_callback (window);

	/* Remove old set of bookmarks. */
	last_static_item = window->details->last_static_bookmark_path;
	if (last_static_item == NULL) {
		last_static_item = NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM;
	}
	remove_bookmarks_after (window, 
				NAUTILUS_MENU_PATH_BOOKMARKS_MENU, 
				last_static_item);


	/* append new set of bookmarks */
	append_dynamic_bookmarks (window);
}

static gboolean
refresh_dynamic_bookmarks_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_WINDOW (data));

	refresh_dynamic_bookmarks (NAUTILUS_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_dynamic_bookmarks (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->refresh_dynamic_bookmarks_idle_id == 0) {
                window->details->refresh_dynamic_bookmarks_idle_id
                        = gtk_idle_add (refresh_dynamic_bookmarks_idle_callback,
                                        window);
	}	
}

/**
 * refresh_go_menu:
 * 
 * Refresh list of bookmarks at end of Go menu to match centralized history list.
 * @window: The NautilusWindow whose Go menu will be refreshed.
 **/
static void
refresh_go_menu (NautilusWindow *window)
{
	GSList *p;
	int index;

	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_window_remove_go_menu_callback (window);

	/* Remove old set of history items. */
	remove_bookmarks_after (window, 
				NAUTILUS_MENU_PATH_GO_MENU, 
				NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY);

	/* Add in a new set of history items. */
	index = 0;
	for (p = nautilus_get_history_list (); p != NULL; p = p->next) {
		char *path;

		path = g_strdup_printf ("/Go/History%d", index); 
                append_bookmark_to_menu (window,
                                         NAUTILUS_BOOKMARK (p->data),
                                         path,
                                         FALSE);
                g_free (path);

                ++index;
	}	
}

static gboolean
refresh_go_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_WINDOW (data));

	refresh_go_menu (NAUTILUS_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_go_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->refresh_go_menu_idle_id == 0) {
                window->details->refresh_go_menu_idle_id
                        = gtk_idle_add (refresh_go_menu_idle_callback,
                                        window);
	}	
}


static void
update_user_level_menu_items (NautilusWindow *window)
{
	char *customize_string;
	int user_level;
	
        g_assert (window != NULL);
        g_assert (NAUTILUS_IS_WINDOW (window));

	user_level = nautilus_user_level_manager_get_user_level ();

	customize_string = get_customize_user_level_settings_menu_string ();
	g_assert (customize_string != NULL);

 	/* Update the user radio group to reflect reality */
	bonobo_ui_handler_menu_set_radio_state (window->ui_handler, 
						convert_user_level_to_menu_path (user_level),
						TRUE);

	/* FIXME bugzilla.eazel.com 1247: 
	 * We want to hide the customize button for the novice user level.
	 * It cant find a bonobo ui handler call to hide a menu item, so make it 
	 * insensitive for now.
	 */
	bonobo_ui_handler_menu_set_sensitivity (window->ui_handler,
						NAUTILUS_MENU_PATH_USER_LEVEL_CUSTOMIZE,
						(user_level > 0));


 	/* Update the "Edit Settings..." item to reflect the user level to customize */
	bonobo_ui_handler_menu_set_label (window->ui_handler,
					  NAUTILUS_MENU_PATH_USER_LEVEL_CUSTOMIZE,
					  customize_string);

	g_free (customize_string);
}

static guint
convert_menu_path_to_user_level (const char *path)
{
        g_assert (path != NULL);
	
	if (strcmp (path, NAUTILUS_MENU_PATH_NOVICE_ITEM) == 0) {
		return 0;
	}
	else if (strcmp (path, NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM) == 0) {
		return 1;
	}
	else if (strcmp (path, NAUTILUS_MENU_PATH_EXPERT_ITEM) == 0) {
		return 2;
	}

	g_assert_not_reached ();

	return 0;
}

static const char *
convert_user_level_to_menu_path (guint user_level)
{
	switch (user_level) {
	case 0:
		return NAUTILUS_MENU_PATH_NOVICE_ITEM;
		break;

	case 1:
		return NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM;
		break;

	case 2:
		return NAUTILUS_MENU_PATH_EXPERT_ITEM;
		break;
	}

	g_assert_not_reached ();

	return NAUTILUS_MENU_PATH_NOVICE_ITEM;
}

static char *
get_customize_user_level_string (void)
{
	char *user_level_string;
	char *capitalized_user_level_string;
	char *title;
	
	user_level_string = nautilus_user_level_manager_get_user_level_as_string ();
	g_assert (user_level_string != NULL);

	capitalized_user_level_string = nautilus_str_capitalize (user_level_string);
	g_assert (capitalized_user_level_string != NULL);
	
	g_free (user_level_string);

	title = g_strdup_printf ("Edit %s Settings", capitalized_user_level_string);

	g_free (capitalized_user_level_string);

	return title;
}

static char *
get_customize_user_level_settings_menu_string (void)
{
	char *title;
	char *ellipse_suffixed_title;

	title = get_customize_user_level_string ();
	g_assert (title != NULL);

	ellipse_suffixed_title = g_strdup_printf ("%s...", title);

	g_free (title);

	return ellipse_suffixed_title;
}

static void
update_preferences_dialog_title (void)
{
	char *dialog_title;
	
	dialog_title = get_customize_user_level_string ();
	g_assert (dialog_title != NULL);
	
	nautilus_global_preferences_set_dialog_title (dialog_title);

	g_free (dialog_title);
}


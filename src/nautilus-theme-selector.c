/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 */

/* This is the implementation of the theme selector window, which
 * displays the available user interface themes and allows the user
 * to pick one.
 */

#include <config.h>
#include "nautilus-theme-selector.h"

#include <ctype.h>
#include <eel/eel-background.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-label.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-theme.h>
#include <math.h>

struct NautilusThemeSelectorDetails {
	GtkWidget *container;

	GtkWidget *title_label;
	GtkWidget *help_label;
	
	GtkWidget *theme_list;
	
	GtkWidget *add_button;
	GtkWidget *add_button_label;	
	GtkWidget *remove_button;

	GdkColor main_row_color;
	GdkColor alt_row_color;
	
	GtkWidget *dialog;
	int selected_row;
	
	gboolean remove_mode;
	gboolean has_local_themes; 
	gboolean populating_theme_list;
	gboolean handling_theme_change;
};

static void  nautilus_theme_selector_initialize_class (GtkObjectClass          *object_klass);
static void  nautilus_theme_selector_initialize       (GtkObject               *object);
static void  nautilus_theme_selector_destroy          (GtkObject               *object);

static void  add_new_theme_button_callback                  (GtkWidget               *widget,
						       NautilusThemeSelector *theme_selector);
static void  remove_button_callback                   (GtkWidget               *widget,
						       NautilusThemeSelector *theme_selector);
static gboolean nautilus_theme_selector_delete_event_callback (GtkWidget *widget,
							GdkEvent  *event,
							gpointer   user_data);

static void  nautilus_theme_selector_theme_changed	(gpointer user_data);
static void  populate_list_with_themes 			(NautilusThemeSelector *theme_selector);

static void  theme_select_row_callback			(GtkCList              *clist,
							 int                    row,
							 int                    column,
							 GdkEventButton        *event,
							 NautilusThemeSelector *theme_selector); 
static void  theme_style_set_callback			(GtkWidget             *widget, 
							 GtkStyle              *previous_style,
							 NautilusThemeSelector *theme_selector); 

static void  exit_remove_mode 				(NautilusThemeSelector *theme_selector);
static void  set_help_label				(NautilusThemeSelector *theme_selector,
							 gboolean remove_mode);
							 
#define THEME_SELECTOR_WIDTH  460
#define THEME_SELECTOR_HEIGHT 264

static NautilusThemeSelector *main_theme_selector = NULL;

EEL_DEFINE_CLASS_BOILERPLATE (NautilusThemeSelector, nautilus_theme_selector, GTK_TYPE_WINDOW)

/* initializing the class object by installing the operations we override */
static void
nautilus_theme_selector_initialize_class (GtkObjectClass *object_klass)
{
	NautilusThemeSelectorClass *klass;
	/* GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (object_klass); */

	klass = NAUTILUS_THEME_SELECTOR_CLASS (object_klass);
	object_klass->destroy = nautilus_theme_selector_destroy;
}

/* handle the "done" button by destroying the window */
static void
done_button_callback (GtkWidget *widget, GtkWidget *theme_selector)
{
	gtk_widget_destroy (theme_selector);
}

/* initialize the instance's fields, create the necessary subviews, etc. */
static void
nautilus_theme_selector_initialize (GtkObject *object)
{
 	NautilusThemeSelector *theme_selector;
 	GtkWidget* widget, *temp_hbox;
	GtkWidget *scrollwindow;
	GtkWidget *bottom_box;
	GtkWidget *temp_button;
	
	theme_selector = NAUTILUS_THEME_SELECTOR (object);
	widget = GTK_WIDGET (object);

	theme_selector->details = g_new0 (NautilusThemeSelectorDetails, 1);
	
	/* set the initial size of the window */
	gtk_widget_set_usize (widget, THEME_SELECTOR_WIDTH, THEME_SELECTOR_HEIGHT);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 0);				

	/* set the title and standard close accelerator */
	gtk_window_set_title (GTK_WINDOW (widget), _("Nautilus Theme Selector"));
	gtk_window_set_wmclass(GTK_WINDOW(widget), "theme_selector", "Nautilus");
	eel_gtk_window_set_up_close_accelerator (GTK_WINDOW (widget));
	gtk_window_set_policy (GTK_WINDOW (widget), FALSE, TRUE, FALSE);
	
	/* create the container box */  
  	theme_selector->details->container =  gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (theme_selector->details->container), 0);				
	gtk_widget_show (GTK_WIDGET (theme_selector->details->container));
	gtk_container_add (GTK_CONTAINER (theme_selector),
			   GTK_WIDGET (theme_selector->details->container));	
	
  	/* create the title box */

  	
  	temp_hbox = gtk_hbox_new(FALSE, 0);
  	gtk_widget_show(temp_hbox);
	gtk_box_pack_start (GTK_BOX(theme_selector->details->container), temp_hbox, FALSE, FALSE, 0);
 	
	/* add the title label */
	theme_selector->details->title_label = eel_label_new (_("Nautilus Theme:"));
	eel_label_make_larger (EEL_LABEL (theme_selector->details->title_label), 2);
	eel_label_make_bold (EEL_LABEL (theme_selector->details->title_label));

  	gtk_widget_show(theme_selector->details->title_label);
	gtk_box_pack_start (GTK_BOX(temp_hbox), theme_selector->details->title_label, FALSE, FALSE, 8);
 
 	/* add the help label */
	theme_selector->details->help_label = eel_label_new ("");
	set_help_label (theme_selector, FALSE);
	eel_label_make_smaller (EEL_LABEL (theme_selector->details->help_label), 2);
	eel_label_set_justify (EEL_LABEL (theme_selector->details->help_label), GTK_JUSTIFY_RIGHT);
  	
	gtk_widget_show(theme_selector->details->help_label);
	gtk_box_pack_end (GTK_BOX(temp_hbox), theme_selector->details->help_label, FALSE, FALSE, 8);
 
 	/* add the main part of the content, which is a list view, embedded in a scrollwindow */
		theme_selector->details->theme_list = gtk_clist_new (2);
	gtk_clist_set_row_height   (GTK_CLIST (theme_selector->details->theme_list), 48);
	
	gtk_clist_set_column_width (GTK_CLIST(theme_selector->details->theme_list), 0, 72);
	gtk_clist_set_column_width (GTK_CLIST(theme_selector->details->theme_list), 1, 130);
	
	gtk_clist_set_shadow_type  (GTK_CLIST (theme_selector->details->theme_list), GTK_SHADOW_IN);
			
	scrollwindow = gtk_scrolled_window_new (NULL, gtk_clist_get_vadjustment (GTK_CLIST (theme_selector->details->theme_list)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrollwindow), theme_selector->details->theme_list);	
	gtk_container_set_border_width (GTK_CONTAINER (scrollwindow), 6);
	gtk_clist_set_selection_mode (GTK_CLIST (theme_selector->details->theme_list), GTK_SELECTION_BROWSE);

	gtk_box_pack_start (GTK_BOX (theme_selector->details->container), scrollwindow, TRUE, TRUE, 0);	
	gtk_widget_show (theme_selector->details->theme_list);
	gtk_widget_show (scrollwindow);

	/* connect a signal to let us know when a row is selected */
	gtk_signal_connect (GTK_OBJECT (theme_selector->details->theme_list),
			    "select_row",
			    GTK_SIGNAL_FUNC (theme_select_row_callback),
			    theme_selector);

	/* we will have to reset some style stuff when a new style is set,
	 * so that we follow gtk+ themes correctly */
	gtk_signal_connect (GTK_OBJECT (theme_selector->details->theme_list),
			    "style_set",
			    GTK_SIGNAL_FUNC (theme_style_set_callback),
			    theme_selector);

  	bottom_box = gtk_hbox_new (FALSE, 0);
  	gtk_widget_show (bottom_box);
	gtk_container_set_border_width (GTK_CONTAINER (bottom_box), GNOME_PAD_SMALL);
	gtk_box_pack_end (GTK_BOX(theme_selector->details->container), bottom_box, FALSE, FALSE, 0);

 	/* create the done button */
 	temp_button = gtk_button_new_with_label (_("Done"));
  	eel_gtk_button_set_padding (GTK_BUTTON (temp_button), GNOME_PAD_SMALL);
	gtk_widget_show(temp_button);
	gtk_box_pack_end (GTK_BOX(bottom_box), temp_button, FALSE, FALSE, GNOME_PAD_SMALL);  
 	gtk_signal_connect(GTK_OBJECT (temp_button), "clicked", GTK_SIGNAL_FUNC (done_button_callback), theme_selector);
 	
  	/* create the "add new" button */
  	theme_selector->details->add_button = gtk_button_new ();
	gtk_widget_show(theme_selector->details->add_button);
	
	theme_selector->details->add_button_label = gtk_label_new (_("Add New Theme..."));
	gtk_widget_show(theme_selector->details->add_button_label);
	gtk_container_add (GTK_CONTAINER(theme_selector->details->add_button), theme_selector->details->add_button_label);
  	eel_gtk_button_set_padding (GTK_BUTTON (theme_selector->details->add_button), GNOME_PAD_SMALL);
	gtk_box_pack_end (GTK_BOX (bottom_box), theme_selector->details->add_button, FALSE, FALSE, GNOME_PAD_SMALL);
 	  
 	gtk_signal_connect(GTK_OBJECT (theme_selector->details->add_button), "clicked", GTK_SIGNAL_FUNC (add_new_theme_button_callback), theme_selector);
	
	/* now create the "remove" button */
  	theme_selector->details->remove_button = gtk_button_new_with_label (_("Remove Theme..."));
  	eel_gtk_button_set_padding (GTK_BUTTON (theme_selector->details->remove_button), GNOME_PAD_SMALL);
	gtk_box_pack_end (GTK_BOX (bottom_box),
			  theme_selector->details->remove_button,
			  FALSE,
			  FALSE,
			  GNOME_PAD_SMALL);
	
 	gtk_signal_connect (GTK_OBJECT (theme_selector->details->remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (remove_button_callback),
			    theme_selector);

	/* generate the actual content */
	populate_list_with_themes (theme_selector);

	/* add a callback for when the theme changes */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, 
					   nautilus_theme_selector_theme_changed,
					   theme_selector);	
	
	gtk_signal_connect (GTK_OBJECT (theme_selector), "delete_event",
                    	    GTK_SIGNAL_FUNC (nautilus_theme_selector_delete_event_callback),
                    	    NULL);
}

static void
nautilus_theme_selector_destroy (GtkObject *object)
{
	NautilusThemeSelector *theme_selector;

	theme_selector = NAUTILUS_THEME_SELECTOR (object);
		
	eel_nullify_cancel (&theme_selector->details->dialog);

	g_free (theme_selector->details);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_theme_selector_theme_changed,
					      theme_selector);

	if (object == GTK_OBJECT (main_theme_selector))
		main_theme_selector = NULL;
		
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));

}

/* create a new instance */
NautilusThemeSelector *
nautilus_theme_selector_new (void)
{
	NautilusThemeSelector *browser = NAUTILUS_THEME_SELECTOR
		(gtk_widget_new (nautilus_theme_selector_get_type (), NULL));
	
	gtk_container_set_border_width (GTK_CONTAINER (browser), 0);
  	gtk_window_set_policy (GTK_WINDOW(browser), TRUE, TRUE, FALSE);
  	gtk_widget_show (GTK_WIDGET(browser));
	
	return browser;
}

/* show the main property browser */

void
nautilus_theme_selector_show (void)
{
	if (main_theme_selector == NULL) {
		main_theme_selector = nautilus_theme_selector_new ();
	} else {
		eel_gtk_window_present (GTK_WINDOW (main_theme_selector));
	}	
	gtk_clist_moveto (GTK_CLIST(main_theme_selector->details->theme_list), main_theme_selector ->details->selected_row, 0, 0.0, 0.0);		
}

static gboolean
nautilus_theme_selector_delete_event_callback (GtkWidget *widget,
					   GdkEvent  *event,
					   gpointer   user_data)
{
	/* Hide but don't destroy */
	gtk_widget_hide(widget);
	return TRUE;
}


/* callback to add a newly selected theme to the user's theme collection */
static void
add_theme_to_icons (GtkWidget *widget, gpointer *data)
{
	char *theme_path, *theme_name, *temp_path;
	char *theme_destination_path, *xml_path;
	char *user_directory, *directory_path;
	NautilusThemeSelector *theme_selector;
	GnomeVFSResult result;
		
	theme_selector = NAUTILUS_THEME_SELECTOR (data);
	theme_path = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (theme_selector->details->dialog)));

	/* get rid of the file selection dialog */
	gtk_widget_destroy (theme_selector->details->dialog);
	theme_selector->details->dialog = NULL;
		
	/* make sure it's a valid theme directory  - check for xml file */
	theme_name = eel_uri_get_basename (theme_path);
	
	temp_path = nautilus_make_path (theme_path, theme_name);
	xml_path = g_strconcat (temp_path, ".xml", NULL);
	g_free (temp_path);
	
	if (!g_file_exists (xml_path)) {
		char *message = g_strdup_printf (_("Sorry, but \"%s\" is not a valid theme folder."), theme_path);
		eel_show_error_dialog (message, _("Couldn't add theme"), GTK_WINDOW (theme_selector));
		g_free (message);
	} else {
		/* copy the theme directory into ~/.nautilus/themes.  First, create the themes directory if it doesn't exist */
		user_directory = nautilus_get_user_directory ();

		directory_path = nautilus_make_path (user_directory, "themes");
		g_free (user_directory);
	
		result = GNOME_VFS_OK;
		if (!g_file_exists (directory_path)) {
			result = gnome_vfs_make_directory (directory_path,
					  	GNOME_VFS_PERM_USER_ALL
					  	| GNOME_VFS_PERM_GROUP_ALL
					  	| GNOME_VFS_PERM_OTHER_READ);
		}

		theme_destination_path = nautilus_make_path (directory_path, theme_name);		
		g_free(directory_path);
			
		/* copy the new theme into the themes directory */
		if (result == GNOME_VFS_OK) {
			result = eel_copy_uri_simple (theme_path, theme_destination_path);
		}
		
		g_free (theme_destination_path);
		
		if (result != GNOME_VFS_OK) {
			char *message = g_strdup_printf (_("Sorry, but the \"%s\" theme couldn't be installed."), theme_path);
			eel_show_error_dialog (message, _("Couldn't install theme"), GTK_WINDOW (theme_selector));
			g_free (message);
		
		} else {	
			/* update the theme selector display */
			populate_list_with_themes (theme_selector);		
		}
	}
			
	g_free (xml_path);
	g_free (theme_name);
	g_free (theme_path);
}

/* handle the add_new button */
static void
add_new_theme_button_callback (GtkWidget *widget, NautilusThemeSelector *theme_selector)
{
	if (theme_selector->details->remove_mode) {
		exit_remove_mode (theme_selector);
		return;
	}
	
	if (theme_selector->details->dialog != NULL) {
		gtk_widget_show (theme_selector->details->dialog);
		if (theme_selector->details->dialog->window) {
			gdk_window_raise (theme_selector->details->dialog->window);
		}

	} else {
		GtkFileSelection *file_dialog;

		theme_selector->details->dialog = gtk_file_selection_new
			(_("Select a theme folder to add as a new theme:"));
		file_dialog = GTK_FILE_SELECTION (theme_selector->details->dialog);
		
		eel_nullify_when_destroyed (&theme_selector->details->dialog);
		
		gtk_signal_connect (GTK_OBJECT (file_dialog->ok_button),
				    "clicked",
				    (GtkSignalFunc) add_theme_to_icons,
				    theme_selector);
		
		gtk_signal_connect_object (GTK_OBJECT (file_dialog->cancel_button),
					   "clicked",
					   (GtkSignalFunc) gtk_widget_destroy,
					   GTK_OBJECT(file_dialog));

		gtk_window_set_position (GTK_WINDOW (file_dialog), GTK_WIN_POS_MOUSE);
		gtk_window_set_transient_for (GTK_WINDOW (file_dialog), GTK_WINDOW (theme_selector));
 		gtk_window_set_wmclass (GTK_WINDOW (file_dialog), "file_selector", "Nautilus");
		gtk_widget_show (GTK_WIDGET(file_dialog));
	}
}

static void
remove_button_callback (GtkWidget *widget, NautilusThemeSelector *theme_selector)
{
	if (theme_selector->details->remove_mode) {
		return;
	}
	
	theme_selector->details->remove_mode = TRUE;

	eel_label_set_text (EEL_LABEL (theme_selector->details->help_label),
				 _("Click on a theme to remove it."));
	gtk_label_set_text (GTK_LABEL (theme_selector->details->add_button_label),
				 _("Cancel Remove"));
	
	populate_list_with_themes (theme_selector);
}

/* utility routine to highlight the row that contains the passed in name */

static void
nautilus_theme_selector_highlight_by_name (NautilusThemeSelector *theme_selector, const char *theme_name)
{
	int index;
	char *row_theme;
	GtkCList *list;
	
	list = GTK_CLIST (theme_selector->details->theme_list);
	for (index = 0; index < list->rows; index++) {
		row_theme = gtk_clist_get_row_data (list, index);	
		if (!eel_strcmp (row_theme, theme_name)) {			
			gtk_clist_select_row (list, index, 0);
			theme_selector->details->selected_row = index ;
			return;
		}
	}	
}

/* handle theme changes by updating the browser contents */

static void
nautilus_theme_selector_theme_changed (gpointer user_data)
{
	char *current_theme;
	NautilusThemeSelector *theme_selector;
	
	theme_selector = NAUTILUS_THEME_SELECTOR (user_data);
	current_theme = nautilus_theme_get_theme();
	nautilus_theme_selector_highlight_by_name (theme_selector, current_theme);
	
	g_free (current_theme);
}

static void
set_help_label (NautilusThemeSelector *theme_selector, gboolean remove_mode)
{
	if (remove_mode) {
		eel_label_set_text (EEL_LABEL (theme_selector->details->help_label),
					_("Click on a theme to remove it."));	
	} else {
		eel_label_set_text (EEL_LABEL (theme_selector->details->help_label),
					_("Click on a theme to change the\n"
					  "appearance of Nautilus."));	

	}
}

static void
exit_remove_mode (NautilusThemeSelector *theme_selector)
{
	theme_selector->details->remove_mode = FALSE;
	set_help_label (theme_selector, FALSE);
	
	/* change the add button label back to it's normal state */
	gtk_label_set_text (GTK_LABEL (theme_selector->details->add_button_label), _("Add New Theme..."));
	populate_list_with_themes (theme_selector);	
}

/* handle clicks on the theme selector by setting the theme */

static void 
theme_select_row_callback (GtkCList * clist, int row, int column, GdkEventButton *event, NautilusThemeSelector *theme_selector)
{
	GnomeVFSResult result;
	char *theme_name, *current_theme;
	char *user_directory, *themes_directory, *this_theme_directory;
	GList *uri_list;
	
	if (theme_selector->details->handling_theme_change || theme_selector->details->populating_theme_list)
		return;

	theme_name = gtk_clist_get_row_data (clist, row);	
	theme_selector->details->handling_theme_change = TRUE;	

	if (theme_selector->details->remove_mode) {
		/* don't allow the current theme to be deleted */
		current_theme = nautilus_theme_get_theme();	
		
		if (eel_strcmp (theme_name, current_theme) == 0) {
			g_free (current_theme);
			exit_remove_mode (theme_selector);
			eel_show_error_dialog (_("Sorry, but you can't remove the current theme. "
						      "Please change to another theme before removing this one."),
				                    _("Can't delete current theme"),
				                    GTK_WINDOW (theme_selector));
			theme_selector->details->handling_theme_change = FALSE;	
			
			return;
		}

		g_free (current_theme);	
		
		/* delete the selected theme.  First, build the uri */
		user_directory = nautilus_get_user_directory ();
		themes_directory = nautilus_make_path (user_directory, "themes");
		this_theme_directory = nautilus_make_path (themes_directory, theme_name);
		
		uri_list = g_list_append(NULL, gnome_vfs_uri_new (this_theme_directory));			
		result = gnome_vfs_xfer_delete_list (uri_list, GNOME_VFS_XFER_RECURSIVE,
	      		      			     GNOME_VFS_XFER_ERROR_MODE_ABORT,
	      		      			     NULL, NULL);
		gnome_vfs_uri_list_free (uri_list);
				   
		if (result != GNOME_VFS_OK) {
			eel_show_error_dialog (_("Sorry, but that theme could not be removed!"), 
					            _("Couldn't remove theme"), GTK_WINDOW (theme_selector));
		
		}
				
		g_free (user_directory);
		g_free (themes_directory);
		g_free (this_theme_directory);
		
		/* exit remove mode */
		exit_remove_mode (theme_selector);
	} else {		
		if (theme_name) {
			nautilus_theme_set_theme (theme_name);
		}
	}
	theme_selector->details->handling_theme_change = FALSE;

}

/* handle changing of gtk themes by regenerating the theme list */
static void
theme_style_set_callback (GtkWidget             *widget, 
			  GtkStyle              *previous_style,
			  NautilusThemeSelector *theme_selector)
{
	populate_list_with_themes (theme_selector);
}

static gboolean
vfs_file_exists (const char *file_uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;
	
	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (file_uri, file_info, 0);
	gnome_vfs_file_info_unref (file_info);

	return result == GNOME_VFS_OK;
}

/* utility routine to test for the presence of an icon file */
static gboolean
has_image_file (const char *path_uri, const char *dir_name, const char *image_file)
{
	char* image_uri;
	gboolean exists;
	
	image_uri = g_strdup_printf ("%s/%s/%s.png", path_uri, dir_name, image_file);
	exists = vfs_file_exists (image_uri);
	g_free (image_uri);
	if (exists)
		return TRUE;

	image_uri = g_strdup_printf ("%s/%s/%s.svg", path_uri, dir_name, image_file);
	exists = vfs_file_exists (image_uri);
	g_free (image_uri);
	return exists;
}

/* derive the theme description from the theme name by reading its xml file */
static char*
get_theme_description_and_display_name (const char *theme_name, const char *theme_path_uri, char** theme_display_name)
{
	char *theme_file_name, *theme_path, *theme_local_path;
	char *description_result, *temp_str;
	xmlDocPtr theme_document;
	
	description_result = NULL;
	if (theme_display_name) {
		*theme_display_name = NULL;
	}
	
	theme_file_name = g_strdup_printf ("%s.xml", theme_name);	
	theme_local_path = gnome_vfs_get_local_path_from_uri (theme_path_uri);
	if (theme_local_path == NULL) {
		theme_path = NULL;
	} else {
		theme_path = nautilus_make_path (theme_local_path, theme_file_name);
		g_free (theme_local_path);
	}
		
	if (theme_path != NULL) {
		/* read the xml document */
		theme_document = xmlParseFile(theme_path);
		
		if (theme_document != NULL) {
			/* fetch the description, if any */		
			temp_str = eel_xml_get_property_translated (xmlDocGetRootElement (theme_document), "description");
			description_result = g_strdup (temp_str);
			xmlFree (temp_str);
			
			if (theme_display_name) {
				temp_str = eel_xml_get_property_translated (xmlDocGetRootElement (theme_document), "name");
				*theme_display_name = g_strdup (temp_str);
				xmlFree (temp_str);
			}
			
			xmlFreeDoc (theme_document);
		}
		
		g_free (theme_path);
	}
	
	g_free (theme_file_name);
	if (description_result)
		return description_result;
	return g_strdup_printf (_("No description available for the \"%s\" theme"),
				*theme_display_name == NULL ? theme_name : *theme_display_name);
}

/* utility to add the theme name and description to the selector.
 * we want to display the theme name and description in the same cell, with the theme
 * name on top in a bigger font, but the clist isn't flexible enough to allow this.  To get around
 * this, we draw what we want into a pixbuf and add that to the clist.
 */

#define DESCRIPTION_WIDTH 320

static GdkPixbuf *
render_theme_name_and_description (GtkWidget *widget, const char *theme_name, const char *theme_description)
{
	GdkVisual *visual;
	GdkPixbuf *pixbuf, *pixbuf_with_alpha;
	GdkColormap *colormap;
	GdkGC *gc;
	GdkFont *big_font, *small_font;
	GtkStyle *style;
	GdkPixmap *pixmap;
	guchar *pixels;
	int description_height;
	
	int big_height, small_height;
	
	visual = gdk_visual_get_system ();

	/* allocate the fonts and measure the text so we know how big a pixmap to allocate */
	gtk_widget_realize (widget);
	style = widget->style;
	big_font = eel_gdk_font_get_larger (style->font, 6);
	small_font = eel_gdk_font_get_smaller (style->font, 3);
	big_height = gdk_text_height (big_font, theme_name, strlen (theme_name));	
	small_height = gdk_text_height (small_font, theme_description, strlen (theme_description));	
	description_height = big_height + small_height + 20;
		
	/* allocate a pixmap to draw the text into, and clear it to white */
	pixmap = gdk_pixmap_new (NULL, DESCRIPTION_WIDTH, description_height, visual->depth);	
	
	gc = gdk_gc_new (pixmap);
	gdk_rgb_gc_set_foreground (gc, EEL_RGB_COLOR_WHITE);
	
	gdk_draw_rectangle (pixmap, gc, TRUE,
			    0, 0,
			    DESCRIPTION_WIDTH, description_height);
	gdk_gc_unref (gc);
	
	/* draw the text with gdk */
	
	gdk_draw_text (pixmap, big_font, style->fg_gc[GTK_STATE_NORMAL], 1, big_height + 4, theme_name, strlen (theme_name));
	gdk_draw_text (pixmap, small_font, style->fg_gc[GTK_STATE_NORMAL], 1, big_height + small_height + 6, theme_description, strlen (theme_description));

	gdk_font_unref (big_font);
	gdk_font_unref (small_font);
	
	/* turn it into a pixbuf, with transparency where there was white */
	colormap = gdk_colormap_new (visual, FALSE);
	pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap,
		 				0, 0,
		 				0, 0, 
		 				DESCRIPTION_WIDTH,  description_height);
	gdk_pixmap_unref (pixmap);
	
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	pixbuf_with_alpha = gdk_pixbuf_add_alpha (pixbuf, TRUE, pixels[0], pixels[1], pixels[2]);
	gdk_pixbuf_unref (pixbuf);
	gdk_colormap_unref (colormap);

	return pixbuf_with_alpha;
}


/* add a pixbuf to a clist */
static void
add_pixbuf_to_theme_list (GtkCList *list, int row, int column, GdkPixbuf *pixbuf)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, EEL_STANDARD_ALPHA_THRESHHOLD);
	gtk_clist_set_pixmap (list, row, column, pixmap ,mask);
		
	gdk_pixmap_unref (pixmap);
	if (mask != NULL) {
		gdk_bitmap_unref (mask);
	}
}

/* utility to add a theme folder to the list */
static void
add_theme (NautilusThemeSelector *theme_selector,
	   const char *theme_path_uri,
	   const char *theme_name,
	   const char *current_theme,
	   int theme_index)
{
	char *theme_description, *theme_display_name;
	GdkPixbuf *theme_pixbuf;
	GdkPixbuf *scaled_pixbuf;
	char      *clist_entry[2];

	/* generate an empty entry to be inserted in the list; we'll set the cell contents later */		
	clist_entry[0] = NULL;
	clist_entry[1] = NULL;
	gtk_clist_append (GTK_CLIST(theme_selector->details->theme_list), clist_entry);
	
	/* add a pixbuf to represent the theme graphically */
	theme_pixbuf = nautilus_theme_make_selector (theme_name);
	scaled_pixbuf = eel_gdk_pixbuf_scale_down_to_fit (theme_pixbuf, 70, 48);
	gdk_pixbuf_unref (theme_pixbuf);
	add_pixbuf_to_theme_list (GTK_CLIST (theme_selector->details->theme_list), theme_index, 0, scaled_pixbuf);
	gdk_pixbuf_unref (scaled_pixbuf);			

	/* get the name and description of the theme */
	theme_description = get_theme_description_and_display_name (theme_name, theme_path_uri, &theme_display_name);
	if (theme_display_name == NULL) {
		theme_display_name = g_strdup (theme_name);
	}
	
	/* add the theme name and description */
	theme_pixbuf = render_theme_name_and_description (theme_selector->details->theme_list, theme_display_name, theme_description);
	add_pixbuf_to_theme_list (GTK_CLIST (theme_selector->details->theme_list), theme_index, 1, theme_pixbuf);
	gdk_pixbuf_unref (theme_pixbuf);			
	
	g_free (theme_display_name);
	g_free (theme_description);
	
	/* set up the row data to specify the theme to set when clicked on */
	gtk_clist_set_row_data_full (GTK_CLIST (theme_selector->details->theme_list),
				     theme_index,
				     g_strdup (theme_name),
				     g_free /*destroy notification*/);
}

/* utility routine to populate by iterating through the passed-in directory */
static int
populate_list_with_themes_from_directory (NautilusThemeSelector *theme_selector, const char *directory_uri, int *index)
{
	int selected_index;
	char *theme_uri, *current_theme;
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GList *list, *element;
	
	selected_index = -1;
				
	result = gnome_vfs_directory_list_load (&list, directory_uri,
					       GNOME_VFS_FILE_INFO_DEFAULT, NULL);
	if (result != GNOME_VFS_OK) {
		return -1;
	}

	current_theme = nautilus_theme_get_theme();

	/* interate through the directory for each file */
	for (element = list; element != NULL; element = element->next) {
		current_file_info = element->data;
		if ((current_file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) && (current_file_info->name[0] != '.')) {
			if (has_image_file (directory_uri, current_file_info->name, "i-directory" )) {
				if (!eel_strcmp (current_theme, current_file_info->name)) {
					selected_index = *index;
				}
				theme_uri = nautilus_make_path (directory_uri, current_file_info->name);
				add_theme (theme_selector, theme_uri, current_file_info->name, current_theme, *index);
				g_free (theme_uri);
				*index += 1;
			}
		}
	}

	gnome_vfs_file_info_list_free (list);	
	g_free (current_theme);
	return selected_index;
}

 
/* populate the list view with the available themes, glean by iterating  */
static void
populate_list_with_themes (NautilusThemeSelector *theme_selector)
{
	int index, save_index, selected_index, alt_selected_index;
	char *pixmap_directory, *directory_uri, *user_directory;
	char *current_theme;
	
	/* first, clear out the list */
	gtk_clist_clear (GTK_CLIST (theme_selector->details->theme_list));
	
	theme_selector->details->has_local_themes = FALSE;
	theme_selector->details->populating_theme_list = TRUE;
	
	/* allocate the colors for the rows */
	
	gdk_color_parse ("rgb:FF/FF/FF", &theme_selector->details->main_row_color);
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (theme_selector->details->theme_list)),
				  &theme_selector->details->main_row_color, FALSE, TRUE);

	gdk_color_parse ("rgb:DD/DD/DD", &theme_selector->details->alt_row_color);	
	gdk_colormap_alloc_color (gtk_widget_get_colormap (GTK_WIDGET (theme_selector->details->theme_list)),
				  &theme_selector->details->alt_row_color, FALSE, TRUE);
	
	/* iterate the pixmap directory to find other installed themes */	
	index = 0;
	selected_index = -1;
	if (!theme_selector->details->remove_mode) {

		pixmap_directory = nautilus_get_pixmap_directory ();
		directory_uri = gnome_vfs_get_uri_from_local_path (pixmap_directory);
		g_free (pixmap_directory);
		
		/* add a theme element for the default theme */
		current_theme = nautilus_theme_get_theme();	
		add_theme (theme_selector, directory_uri, "default", current_theme, index++);
		g_free (current_theme);
		
		/* process the built-in themes */
		selected_index = populate_list_with_themes_from_directory (theme_selector, directory_uri, &index);
		g_free (directory_uri);
	}
		
	/* now process the user-added themes */
	
	user_directory = nautilus_get_user_directory ();
	directory_uri = nautilus_make_path (user_directory, "themes");
	g_free (user_directory);
	
	save_index = index;
	alt_selected_index = populate_list_with_themes_from_directory (theme_selector, directory_uri, &index);
	g_free (directory_uri);
	
	if (selected_index == -1) {
		selected_index = alt_selected_index;
	}
	
	theme_selector->details->has_local_themes = index > save_index;
	if (theme_selector->details->has_local_themes && !theme_selector->details->remove_mode)
		gtk_widget_show(theme_selector->details->remove_button);
	else
		gtk_widget_hide(theme_selector ->details->remove_button);

	/* select the appropriate row, and make sure it's visible on the screen */	
	if (selected_index >= 0) {
		theme_selector->details->selected_row = selected_index;
		gtk_clist_select_row (GTK_CLIST(theme_selector->details->theme_list), selected_index, 0);
		/* gtk_clist_moveto (GTK_CLIST(theme_selector->details->theme_list), selected_index, 0, 0.0, 0.0); */		
	}
	theme_selector->details->populating_theme_list = FALSE;
}

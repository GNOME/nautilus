/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 * Authors: Andy Hertzfeld <andy@eazel.com>
 *          Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-theme-selector.c - Nautilus theme selector widget. */

#include <config.h>
#include "nautilus-theme-selector.h"

#include <eel/eel-art-gtk-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image-chooser.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-viewport.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-theme.h>

#define NUM_VISIBLE_THEMES 3
#define DEFAULT_THEME_NAME "default"
#define THEME_SELECTOR_DATA_KEY "nautilus-theme-selector"

struct NautilusThemeSelectorDetails
{
	GtkWidget *help_label;
	GtkWidget *install_theme_button;
	GtkWidget *remove_theme_button;
	GtkWidget *cancel_remove_button;
	EelImageChooser *theme_selector;
	EelImageChooser *remove_theme_selector;
	GtkWidget *scrolled_window;
	GtkWidget *remove_scrolled_window;
	guint theme_selector_changed_connection;
	GtkWindow *parent_window;
};

/* Signals */
typedef enum
{
	THEME_CHANGED,
	LAST_SIGNAL
} ThemeSelectorSignals;

static guint theme_selector_signals[LAST_SIGNAL];

/* GtkObjectClass methods */
static void     nautilus_theme_selector_initialize_class              (NautilusThemeSelectorClass  *theme_selector_class);
static void     nautilus_theme_selector_initialize                    (NautilusThemeSelector       *theme_selector);
static void     theme_selector_destroy                                (GtkObject                   *object);

/* Private stuff */
static void     theme_selector_populate_list                          (EelImageChooser             *image_chooser,
								       GtkWidget                   *scrolled_window,
								       gboolean                     include_builtin);
static void     theme_selector_changed_callback                       (EelImageChooser             *image_chooser,
								       gpointer                     callback_data);
static void     remove_theme_selector_changed_callback                (EelImageChooser             *image_chooser,
								       gpointer                     callback_data);
static void     install_theme_button_clicked_callback                 (GtkWidget                   *button,
								       gpointer                     callback_data);
static void     remove_theme_button_clicked_callback                  (GtkWidget                   *button,
								       gpointer                     callback_data);
static void     cancel_remove_button_clicked_callback                 (GtkWidget                   *button,
								       gpointer                     callback_data);
static gboolean theme_selector_has_user_themes                        (const NautilusThemeSelector *theme_selector);
static void     theme_selector_update_remove_theme_button             (NautilusThemeSelector       *theme_selector);
static void     theme_selector_update_selected_theme_from_preferences (NautilusThemeSelector       *theme_selector);
static void     theme_selector_update_help_label                      (NautilusThemeSelector       *theme_selector,
								       gboolean                     removing);
	
EEL_DEFINE_CLASS_BOILERPLATE (NautilusThemeSelector, nautilus_theme_selector, GTK_TYPE_VBOX)

/* NautilusThemeSelectorClass methods */
static void
nautilus_theme_selector_initialize_class (NautilusThemeSelectorClass *theme_selector_class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (theme_selector_class);

	/* GtkObjectClass */
	object_class->destroy = theme_selector_destroy;

	/* Signals */
	theme_selector_signals[THEME_CHANGED] = gtk_signal_new ("theme_changed",
							       GTK_RUN_LAST,
							       object_class->type,
							       0,
							       gtk_marshal_NONE__STRING,
							       GTK_TYPE_NONE, 
							       0);
	
	gtk_object_class_add_signals (object_class, theme_selector_signals, LAST_SIGNAL);
}

static void
make_widgets_same_size (GtkWidget *one, GtkWidget *two)
{
	EelDimensions one_dimensions;
	EelDimensions two_dimensions;

	g_return_if_fail (GTK_IS_WIDGET (one));
	g_return_if_fail (GTK_IS_WIDGET (two));

	one_dimensions = eel_gtk_widget_get_preferred_dimensions (one);
	two_dimensions = eel_gtk_widget_get_preferred_dimensions (two);

	gtk_widget_set_usize (one,
			      MAX (one_dimensions.width, two_dimensions.width),
			      MAX (one_dimensions.height, two_dimensions.height));

	gtk_widget_set_usize (two,
			      MAX (one_dimensions.width, two_dimensions.width),
			      MAX (one_dimensions.height, two_dimensions.height));
}

static void
nautilus_theme_selector_initialize (NautilusThemeSelector *theme_selector)
{
	GtkWidget *button_box;
	GtkWidget *alignment_box;

	theme_selector->details = g_new0 (NautilusThemeSelectorDetails, 1);
	
	theme_selector->details->help_label = gtk_label_new ("");

	gtk_label_set_justify (GTK_LABEL (theme_selector->details->help_label),
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (theme_selector->details->help_label), 0.0, 1.0);

	theme_selector_update_help_label (theme_selector, FALSE);

 	theme_selector->details->scrolled_window = 
		eel_scrolled_image_chooser_new ((GtkWidget**) &theme_selector->details->theme_selector);

 	theme_selector->details->remove_scrolled_window = 
		eel_scrolled_image_chooser_new ((GtkWidget**) &theme_selector->details->remove_theme_selector);

 	alignment_box = gtk_hbox_new (FALSE, 4);
	button_box = gtk_hbox_new (TRUE, 2);
	
	gtk_box_pack_start (GTK_BOX (theme_selector), theme_selector->details->help_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (theme_selector), theme_selector->details->scrolled_window, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (theme_selector), theme_selector->details->remove_scrolled_window, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (theme_selector), alignment_box, FALSE, TRUE, 2);

	gtk_box_pack_end (GTK_BOX (alignment_box), button_box, FALSE, FALSE, 2);

	theme_selector->details->install_theme_button = gtk_button_new_with_label (_("Add New Theme..."));
	gtk_signal_connect (GTK_OBJECT (theme_selector->details->install_theme_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (install_theme_button_clicked_callback),
			    theme_selector);

	theme_selector->details->remove_theme_button = gtk_button_new_with_label (_("Remove Theme..."));
	gtk_signal_connect (GTK_OBJECT (theme_selector->details->remove_theme_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (remove_theme_button_clicked_callback),
			    theme_selector);

	theme_selector->details->cancel_remove_button = gtk_button_new_with_label (_("Cancel Remove"));
	gtk_signal_connect (GTK_OBJECT (theme_selector->details->cancel_remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (cancel_remove_button_clicked_callback),
			    theme_selector);

	gtk_box_pack_start (GTK_BOX (button_box), theme_selector->details->cancel_remove_button, TRUE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_box), theme_selector->details->install_theme_button, TRUE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (button_box), theme_selector->details->remove_theme_button, TRUE, FALSE, 4);

	make_widgets_same_size (theme_selector->details->install_theme_button,
				theme_selector->details->remove_theme_button);
	
	theme_selector_populate_list (theme_selector->details->theme_selector,
				      theme_selector->details->scrolled_window,
				      TRUE);
	theme_selector_update_selected_theme_from_preferences (theme_selector);

	theme_selector_populate_list (theme_selector->details->remove_theme_selector,
				      theme_selector->details->remove_scrolled_window,
				      FALSE);

	gtk_widget_show (theme_selector->details->help_label);
	gtk_widget_show_all (theme_selector->details->scrolled_window);
	gtk_widget_show_all (theme_selector->details->remove_scrolled_window);
 	gtk_widget_show_all (alignment_box);

	gtk_widget_hide (theme_selector->details->remove_scrolled_window);
	gtk_widget_hide (theme_selector->details->cancel_remove_button);

	theme_selector_update_remove_theme_button (theme_selector);

	gtk_signal_connect (GTK_OBJECT (theme_selector->details->theme_selector),
			    "selection_changed",
			    GTK_SIGNAL_FUNC (theme_selector_changed_callback),
			    theme_selector);

	theme_selector->details->theme_selector_changed_connection =
		gtk_signal_connect (GTK_OBJECT (theme_selector->details->remove_theme_selector),
				    "selection_changed",
				    GTK_SIGNAL_FUNC (remove_theme_selector_changed_callback),
				    theme_selector);
}

static void
theme_selector_update_help_label (NautilusThemeSelector *theme_selector,
				  gboolean removing)
{
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));

	if (removing) {
		gtk_label_set_text (GTK_LABEL (theme_selector->details->help_label),
				    _("Click on a theme to remove it."));
	} else {
		gtk_label_set_text (GTK_LABEL (theme_selector->details->help_label),
				    _("Click on a theme to change the appearance of Nautilus."));
	}
}

static void
file_selection_ok_clicked_callback (GtkWidget *button,
				    gpointer callback_data)
{
	NautilusThemeSelector *theme_selector;
	const char *selected_path;
	NautilusThemeInstallResult result;
	char *message;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_FILE_SELECTION (callback_data));

	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (gtk_object_get_data (GTK_OBJECT (callback_data),
									   THEME_SELECTOR_DATA_KEY)));

	theme_selector = NAUTILUS_THEME_SELECTOR (gtk_object_get_data (GTK_OBJECT (callback_data),
								       THEME_SELECTOR_DATA_KEY));
	
	selected_path = gtk_file_selection_get_filename (GTK_FILE_SELECTION (callback_data));

	result = nautilus_theme_install_user_theme (selected_path);

	switch (result) {
	case NAUTILUS_THEME_INSTALL_NOT_A_THEME_DIRECTORY:
		message = g_strdup_printf (_("Sorry, but \"%s\" is not a valid theme folder."),
					   selected_path);
		eel_show_error_dialog (message, _("Couldn't add theme"),
				       theme_selector->details->parent_window);
		g_free (message);
		break;

	case NAUTILUS_THEME_INSTALL_FAILED_USER_THEMES_DIRECTORY_CREATION:
	case NAUTILUS_THEME_INSTALL_FAILED:
		message = g_strdup_printf (_("Sorry, but the \"%s\" theme couldn't be installed."),
					   selected_path);
		eel_show_error_dialog (message, _("Couldn't install theme"),
				       theme_selector->details->parent_window);
		g_free (message);
		break;

	case NAUTILUS_THEME_INSTALL_OK:
		/* Re populate the theme lists to match the stored state */
		theme_selector_populate_list (theme_selector->details->theme_selector,
					      theme_selector->details->scrolled_window,
					      TRUE);
		theme_selector_update_selected_theme_from_preferences (theme_selector);
		
		theme_selector_populate_list (theme_selector->details->remove_theme_selector,
					      theme_selector->details->remove_scrolled_window,
					      FALSE);
		
		/* Update the showing state of the remove button */
		theme_selector_update_remove_theme_button (theme_selector);
		break;
	}

	gtk_widget_destroy (GTK_WIDGET (callback_data));
}

static void
file_selection_cancel_clicked_callback (GtkWidget *button,
					gpointer callback_data)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GTK_IS_FILE_SELECTION (callback_data));

	gtk_widget_destroy (GTK_WIDGET (callback_data));
}

static void
install_theme_button_clicked_callback (GtkWidget *button,
				       gpointer callback_data)
{
	NautilusThemeSelector *theme_selector;
	GtkWidget *file_selection_dialog;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (callback_data));

	theme_selector = NAUTILUS_THEME_SELECTOR (callback_data);
	file_selection_dialog = gtk_file_selection_new (_("Select a theme folder to add as a new theme:"));

	eel_gtk_window_set_up_close_accelerator (GTK_WINDOW (file_selection_dialog));

	gtk_object_set_data (GTK_OBJECT (file_selection_dialog),
			     THEME_SELECTOR_DATA_KEY,
			     callback_data);

	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_selection_dialog)->ok_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (file_selection_ok_clicked_callback),
			    file_selection_dialog);

	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (file_selection_dialog)->cancel_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (file_selection_cancel_clicked_callback),
			    file_selection_dialog);

	gtk_window_set_position (GTK_WINDOW (file_selection_dialog), GTK_WIN_POS_MOUSE);
	if (theme_selector->details->parent_window != NULL) {
		gtk_window_set_transient_for (GTK_WINDOW (file_selection_dialog),
					      theme_selector->details->parent_window);
	}
	gtk_window_set_wmclass (GTK_WINDOW (file_selection_dialog), "file_selector", "Nautilus");
	gtk_widget_show (GTK_WIDGET (file_selection_dialog));
}

static void
remove_theme_button_clicked_callback (GtkWidget *button,
				      gpointer callback_data)
{
	NautilusThemeSelector *theme_selector;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (callback_data));

	theme_selector = NAUTILUS_THEME_SELECTOR (callback_data);

	theme_selector_update_help_label (theme_selector, TRUE);
	
 	gtk_widget_hide (theme_selector->details->scrolled_window);
 	gtk_widget_show (theme_selector->details->remove_scrolled_window);
 	gtk_widget_show (theme_selector->details->cancel_remove_button);
 	gtk_widget_hide (theme_selector->details->install_theme_button);
 	gtk_widget_hide (theme_selector->details->remove_theme_button);
}

static void
theme_selector_finish_remove (NautilusThemeSelector *theme_selector)
{
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));

	theme_selector_update_help_label (theme_selector, FALSE);

 	gtk_widget_show (theme_selector->details->scrolled_window);
 	gtk_widget_hide (theme_selector->details->remove_scrolled_window);
 	gtk_widget_hide (theme_selector->details->cancel_remove_button);
 	gtk_widget_show (theme_selector->details->install_theme_button);
	theme_selector_update_remove_theme_button (theme_selector);

	eel_image_chooser_set_selected_row (theme_selector->details->remove_theme_selector, -1);
}

static gboolean
theme_selector_has_user_themes (const NautilusThemeSelector *theme_selector)
{
	g_return_val_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector), FALSE);

	return eel_image_chooser_get_num_rows (theme_selector->details->remove_theme_selector) > 0;
}

static void
theme_selector_update_remove_theme_button (NautilusThemeSelector *theme_selector)
{
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));

	eel_gtk_widget_set_shown (theme_selector->details->remove_theme_button,
				  theme_selector_has_user_themes (theme_selector));
}	

static void
cancel_remove_button_clicked_callback (GtkWidget *button,
				       gpointer callback_data)
{
	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (callback_data));

	theme_selector_finish_remove (NAUTILUS_THEME_SELECTOR (callback_data));
}

static char *
theme_selector_get_selected_theme (const NautilusThemeSelector *theme_selector)
{
	int selected_row_position;
	gpointer row_data;

	g_return_val_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector), NULL);
	
	selected_row_position = eel_image_chooser_get_selected_row (theme_selector->details->theme_selector);
	g_return_val_if_fail (selected_row_position != -1, NULL);
	
	row_data = eel_image_chooser_get_row_data (theme_selector->details->theme_selector,
						   selected_row_position);
	g_return_val_if_fail (row_data != NULL, NULL);
	
	return g_strdup (row_data);
}

static void
remove_theme_selector_changed_callback (EelImageChooser *image_chooser,
					gpointer callback_data)
{
	NautilusThemeSelector *theme_selector;
	int theme_to_remove_position;
	char *theme_to_remove;
	char *selected_theme;
	GnomeVFSResult remove_result;

	g_return_if_fail (EEL_IS_IMAGE_CHOOSER (image_chooser));
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (callback_data));
	
	theme_selector = NAUTILUS_THEME_SELECTOR (callback_data);
	
	theme_to_remove_position = eel_image_chooser_get_selected_row (image_chooser);
	g_return_if_fail (theme_to_remove_position != -1);

	theme_to_remove = g_strdup ((const char *)eel_image_chooser_get_row_data (image_chooser, theme_to_remove_position));

	gtk_signal_handler_block (GTK_OBJECT (theme_selector->details->remove_theme_selector),
				  theme_selector->details->theme_selector_changed_connection);
	theme_selector_finish_remove (theme_selector);
	gtk_signal_handler_unblock (GTK_OBJECT (theme_selector->details->remove_theme_selector),
				    theme_selector->details->theme_selector_changed_connection);

	selected_theme = theme_selector_get_selected_theme (theme_selector);
	g_return_if_fail (selected_theme != NULL);

	/* Don't allow the current theme to be deleted */
	if (eel_str_is_equal (selected_theme, theme_to_remove)) {
		eel_image_chooser_set_selected_row (theme_selector->details->remove_theme_selector, -1);
		g_free (selected_theme);
		g_free (theme_to_remove);
		eel_show_error_dialog (_("Sorry, but you can't remove the current theme. "
					 "Please change to another theme before removing this one."),
				       _("Can't delete current theme"),
				       theme_selector->details->parent_window);
		return;
	}

	remove_result = nautilus_theme_remove_user_theme (theme_to_remove);

	if (remove_result != GNOME_VFS_OK) {
		eel_show_error_dialog (_("Sorry, but that theme could not be removed!"), 
				       _("Couldn't remove theme"), NULL);
	}

	/* Re populate the theme lists to match the stored state */
	theme_selector_populate_list (theme_selector->details->theme_selector,
				      theme_selector->details->scrolled_window,
				      TRUE);
	theme_selector_update_selected_theme_from_preferences (theme_selector);

	theme_selector_populate_list (theme_selector->details->remove_theme_selector,
				      theme_selector->details->remove_scrolled_window,
				      FALSE);

	/* Update the showing state of the remove button */
	theme_selector_update_remove_theme_button (theme_selector);

	g_free (selected_theme);
	g_free (theme_to_remove);
}

static void
theme_selector_changed_callback (EelImageChooser *image_chooser,
				 gpointer callback_data)
{
	int selected_row_position;

	g_return_if_fail (EEL_IS_IMAGE_CHOOSER (image_chooser));
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (callback_data));
	
	image_chooser = EEL_IMAGE_CHOOSER (image_chooser);
	
	selected_row_position = eel_image_chooser_get_selected_row (image_chooser);
	
	gtk_signal_emit (GTK_OBJECT (callback_data),
			 theme_selector_signals[THEME_CHANGED]);
}

/* GtkObjectClass methods */
static void
theme_selector_destroy (GtkObject *object)
{
	NautilusThemeSelector *theme_selector;
	
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (object));
	
	theme_selector = NAUTILUS_THEME_SELECTOR (object);
	theme_selector->details->theme_selector_changed_connection = 0;

	g_free (theme_selector->details);

	/* Chain destroy */
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* Private stuff */
typedef struct
{
	EelImageChooser *image_chooser;
	gboolean include_builtin;
} ForEachThemeData;

static void
for_each_theme_callback (const char *name,
			 const char *path,
			 const char *display_name,
			 const char *description,
			 GdkPixbuf *preview_pixbuf,
			 gboolean builtin,
			 gpointer callback_data)
{
	ForEachThemeData *data;

	g_return_if_fail (name != NULL);
	g_return_if_fail (path != NULL);
	g_return_if_fail (display_name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (preview_pixbuf != NULL);
	g_return_if_fail (callback_data != NULL);

	data = callback_data;
	g_return_if_fail (EEL_IS_IMAGE_CHOOSER (data->image_chooser));

	if (!data->include_builtin && builtin) {
		return;
	}

	eel_image_chooser_insert_row (data->image_chooser,
				      preview_pixbuf,
				      display_name,
				      description,
				      g_strdup (name),
				      g_free);
}

static void
theme_selector_populate_list (EelImageChooser *image_chooser,
			      GtkWidget *scrolled_window,
			      gboolean include_builtin)
{
	ForEachThemeData data;

	g_return_if_fail (EEL_IS_IMAGE_CHOOSER (image_chooser));
	g_return_if_fail (GTK_IS_SCROLLED_WINDOW (scrolled_window));

	eel_image_chooser_clear (image_chooser);

	data.image_chooser = image_chooser;
	data.include_builtin = include_builtin;
	nautilus_theme_for_each_theme (for_each_theme_callback, &data);
	
	eel_scrolled_image_chooser_set_num_visible_rows (image_chooser,
							 scrolled_window,
							 NUM_VISIBLE_THEMES);
	
	eel_scrolled_image_chooser_show_selected_row (image_chooser, scrolled_window);
}

static void
theme_selector_update_selected_theme_from_preferences (NautilusThemeSelector *theme_selector)
{
	char *theme_name;
	
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));

	theme_name = eel_preferences_get (NAUTILUS_PREFERENCES_THEME);
	nautilus_theme_selector_set_selected_theme (theme_selector, theme_name);
	g_free (theme_name);

	eel_scrolled_image_chooser_show_selected_row (theme_selector->details->theme_selector,
						      theme_selector->details->scrolled_window);
}

/* NautilusThemeSelector public methods */
GtkWidget *
nautilus_theme_selector_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_THEME_SELECTOR, NULL);
}

char *
nautilus_theme_selector_get_selected_theme (const NautilusThemeSelector *theme_selector)
{
	const char *theme_name;
	int selected_row_position;

	g_return_val_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector), NULL);
	
	selected_row_position = eel_image_chooser_get_selected_row (theme_selector->details->theme_selector);
	g_return_val_if_fail (selected_row_position != -1, g_strdup (DEFAULT_THEME_NAME));
	theme_name = eel_image_chooser_get_row_data (theme_selector->details->theme_selector,
						     selected_row_position);
	
	return g_strdup (theme_name);
}

void
nautilus_theme_selector_set_selected_theme (NautilusThemeSelector *theme_selector,
					    char *new_theme_name)
{
	guint i;
	const char *theme_name;

	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));

	for (i = 0; i < eel_image_chooser_get_num_rows (theme_selector->details->theme_selector); i++) {
		theme_name = eel_image_chooser_get_row_data (theme_selector->details->theme_selector, i);
		
		if (eel_str_is_equal (theme_name, new_theme_name)) {
			eel_image_chooser_set_selected_row (theme_selector->details->theme_selector, i);
			eel_scrolled_image_chooser_show_selected_row (theme_selector->details->theme_selector,
								      theme_selector->details->scrolled_window);
			return;
		}
	}
}

void
nautilus_theme_selector_set_parent_window (NautilusThemeSelector *theme_selector,
					   GtkWindow *parent_window)
{
	g_return_if_fail (NAUTILUS_IS_THEME_SELECTOR (theme_selector));
	g_return_if_fail (GTK_IS_WINDOW (parent_window));

	theme_selector->details->parent_window = parent_window;
}

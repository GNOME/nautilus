/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-preferences-item.c - Implementation for an individual prefs item.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-preferences-item.h"
#include "nautilus-preferences.h"
#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include <libgnomevfs/gnome-vfs.h>

#include <gtk/gtkcheckbutton.h>

#include "nautilus-radio-button-group.h"
#include "nautilus-string-picker.h"
#include "nautilus-text-caption.h"

/* Arguments */
enum
{
	ARG_0,
	ARG_SHOW_DESCRIPTION,
	ARG_DESCRIPTION_STRING,
	ARG_TITLE_STRING
};

enum
{
	ACTIVATE,
	LAST_SIGNAL
};

static const guint PREFERENCES_ITEM_TITLE_SPACING = 4;
static const guint PREFERENCES_ITEM_FRAME_BORDER_WIDTH = 6;
static const gint PREFERENCES_ITEM_UNDEFINED_ITEM = -1;

struct _NautilusPreferencesItemDetails
{
	gchar				*preference_name;
	NautilusPreferencesItemType	item_type;
	GtkWidget			*child;
};

/* NautilusPreferencesItemClass methods */
static void nautilus_preferences_item_initialize_class (NautilusPreferencesItemClass *klass);
static void nautilus_preferences_item_initialize       (NautilusPreferencesItem      *preferences_item);


/* GtkObjectClass methods */
static void preferences_item_destroy                   (GtkObject                    *object);
static void preferences_item_set_arg                   (GtkObject                    *object,
							GtkArg                       *arg,
							guint                         arg_id);
static void preferences_item_get_arg                   (GtkObject                    *object,
							GtkArg                       *arg,
							guint                         arg_id);

/* Private stuff */
static void preferences_item_construct                 (NautilusPreferencesItem      *item,
							const gchar                  *preference_name,
							NautilusPreferencesItemType   item_type);
static void preferences_item_create_enum               (NautilusPreferencesItem      *item,
							const NautilusPreference     *preference);
static void preferences_item_create_short_enum         (NautilusPreferencesItem      *item,
							const NautilusPreference     *preference);
static void preferences_item_create_boolean            (NautilusPreferencesItem      *item,
							const NautilusPreference     *preference);
static void preferences_item_create_editable_string    (NautilusPreferencesItem      *item,
							const NautilusPreference     *preference);
static void preferences_item_create_font_family               (NautilusPreferencesItem      *item,
							const NautilusPreference     *preference);
static void preferences_item_create_theme	       (NautilusPreferencesItem      *item,
							const NautilusPreference     *preference);
static void enum_radio_group_changed_callback          (GtkWidget                    *button_group,
							GtkWidget                    *button,
							gpointer                      user_data);
static void boolean_button_toggled_callback            (GtkWidget                    *button_group,
							gpointer                      user_data);
static void text_item_changed_callback                 (GtkWidget                    *string_picker,
							gpointer                      user_data);
static void editable_string_changed_callback           (GtkWidget                    *caption,
							gpointer                      user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusPreferencesItem, nautilus_preferences_item, GTK_TYPE_VBOX)

/* NautilusPreferencesItemClass methods */
static void
nautilus_preferences_item_initialize_class (NautilusPreferencesItemClass *preferences_item_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (preferences_item_class);
	widget_class = GTK_WIDGET_CLASS (preferences_item_class);

 	parent_class = gtk_type_class (gtk_vbox_get_type ());

	/* Arguments */
	gtk_object_add_arg_type ("NautilusPreferencesItem::show_description",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SHOW_DESCRIPTION);

	gtk_object_add_arg_type ("NautilusPreferencesItem::description_string",
				 GTK_TYPE_STRING,
				 GTK_ARG_WRITABLE,
				 ARG_DESCRIPTION_STRING);

	gtk_object_add_arg_type ("NautilusPreferencesItem::title_string",
				 GTK_TYPE_STRING,
				 GTK_ARG_WRITABLE,
				 ARG_TITLE_STRING);

	/* GtkObjectClass */
	object_class->destroy = preferences_item_destroy;
	object_class->set_arg = preferences_item_set_arg;
	object_class->get_arg = preferences_item_get_arg;
}

static void
nautilus_preferences_item_initialize (NautilusPreferencesItem *item)
{
	item->details = g_new (NautilusPreferencesItemDetails, 1);

	item->details->preference_name = NULL;
	item->details->item_type = PREFERENCES_ITEM_UNDEFINED_ITEM;
	item->details->child = NULL;
}

/* GtkObjectClass methods */
static void
preferences_item_destroy (GtkObject *object)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

	if (item->details->preference_name)
	{
		g_free (item->details->preference_name);
	}

	g_free (item->details);
	
	/* Chain */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
preferences_item_set_arg (GtkObject	*object,
			  GtkArg	*arg,
			  guint		arg_id)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

#if 0	
	switch (arg_id)
	{
	case ARG_SHOW_DESCRIPTION:
		item->details->show_description = GTK_VALUE_BOOL (*arg);

		if (item->details->show_description)
		{
			gtk_widget_show (item->details->description_label);
		}
		else
		{
			gtk_widget_hide (item->details->description_label);
		}

		break;

	case ARG_DESCRIPTION_STRING:
		
		gtk_label_set_text (GTK_LABEL (item->details->description_label),
				    GTK_VALUE_STRING (*arg));
		break;

	case ARG_TITLE_STRING:

		gtk_frame_set_label (GTK_FRAME (object), GTK_VALUE_STRING (*arg));

		break;
	}
#endif

}

static void
preferences_item_get_arg (GtkObject	*object,
			  GtkArg	*arg,
			  guint		arg_id)
{
	NautilusPreferencesItem * item;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_ITEM (object));
	
	item = NAUTILUS_PREFERENCES_ITEM (object);

#if 0	
	switch (arg_id)
	{
	case ARG_SHOW_DESCRIPTION:
 		GTK_VALUE_BOOL (*arg) = 
			GTK_WIDGET_VISIBLE (item->details->description_label);
		break;
	}
#endif
}

/*
 * Private stuff
 */
static void
preferences_item_construct (NautilusPreferencesItem	*item,
			    const gchar			*preference_name,
			    NautilusPreferencesItemType	item_type)
{
	const NautilusPreference	*preference;

	g_assert (item != NULL);

	g_assert (preference_name != NULL);
	g_assert (item_type != PREFERENCES_ITEM_UNDEFINED_ITEM);

	g_assert (item->details->child == NULL);

	g_assert (item->details->preference_name == NULL);
	
	item->details->preference_name = g_strdup (preference_name);

	preference = nautilus_preference_find_by_name (item->details->preference_name);
	
	g_assert (preference != NULL);

	/* Create the child widget according to the item type */
	switch (item_type)
	{
	case NAUTILUS_PREFERENCE_ITEM_BOOLEAN:
		preferences_item_create_boolean (item, preference);
		break;
		
	case NAUTILUS_PREFERENCE_ITEM_ENUM:
		preferences_item_create_enum (item, preference);
		break;

	case NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM:
		preferences_item_create_short_enum (item, preference);
		break;

	case NAUTILUS_PREFERENCE_ITEM_FONT_FAMILY:
		preferences_item_create_font_family (item, preference);
		break;
	
	case NAUTILUS_PREFERENCE_ITEM_THEME:
		preferences_item_create_theme (item, preference);
		break;
	case NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING:
		preferences_item_create_editable_string (item, preference);
		break;	
	}

	gtk_object_unref (GTK_OBJECT (preference));

	g_assert (item->details->child != NULL);

	gtk_box_pack_start (GTK_BOX (item),
			    item->details->child,
			    FALSE,
			    FALSE,
			    0);
	
	gtk_widget_show (item->details->child);
}

static void
preferences_item_create_enum (NautilusPreferencesItem	*item,
			      const NautilusPreference	*preference)
{
	guint	i;
	gint	value;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);

	item->details->child = nautilus_radio_button_group_new (FALSE);
		
 	value = nautilus_preferences_get_enum (item->details->preference_name, 0);
	
	for (i = 0; i < nautilus_preference_enum_get_num_entries (preference); i++) {
		char *description;
		
		description = nautilus_preference_enum_get_nth_entry_description (preference, i);

		g_assert (description != NULL);

		nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child),
						    description);

		g_free (description);
		
		if (value == nautilus_preference_enum_get_nth_entry_value (preference, i)) {
			
			nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child), i);
		}
	}
	
	gtk_signal_connect (GTK_OBJECT (item->details->child),
			    "changed",
			    GTK_SIGNAL_FUNC (enum_radio_group_changed_callback),
			    (gpointer) item);
}

/* This is just like preferences_item_create_enum except the choices
 * are laid out horizontally instead of vertically (hence it works decently
 * only with short text for the choices).
 */
static void
preferences_item_create_short_enum (NautilusPreferencesItem	*item,
			      	    const NautilusPreference	*preference)
{
	guint	i;
	gint	value;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);

	item->details->child = nautilus_radio_button_group_new (TRUE);
		
 	value = nautilus_preferences_get_enum (item->details->preference_name, 0);
	
	for (i = 0; i < nautilus_preference_enum_get_num_entries (preference); i++) {
		char *description;
		
		description = nautilus_preference_enum_get_nth_entry_description (preference, i);

		g_assert (description != NULL);

		nautilus_radio_button_group_insert (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child),
						    description);

		g_free (description);
		
		if (value == nautilus_preference_enum_get_nth_entry_value (preference, i)) {
			
			nautilus_radio_button_group_set_active_index (NAUTILUS_RADIO_BUTTON_GROUP (item->details->child), i);
		}
	}
	
	gtk_signal_connect (GTK_OBJECT (item->details->child),
			    "changed",
			    GTK_SIGNAL_FUNC (enum_radio_group_changed_callback),
			    (gpointer) item);
}

static void
preferences_item_create_boolean (NautilusPreferencesItem	*item,
				 const NautilusPreference	*preference)
{
	gboolean	value;
	char		*description;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);
	description = nautilus_preference_get_description (preference);

	g_assert (description != NULL);

	item->details->child = gtk_check_button_new_with_label (description);

	g_free (description);

	value = nautilus_preferences_get_boolean (item->details->preference_name, FALSE);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->details->child), value);
				      
	gtk_signal_connect (GTK_OBJECT (item->details->child),
			    "toggled",
			    GTK_SIGNAL_FUNC (boolean_button_toggled_callback),
			    (gpointer) item);
}

static void
preferences_item_create_editable_string (NautilusPreferencesItem	*item,
					 const NautilusPreference	*preference)
{
	char	*current_value;
	char	*description;
	
	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);
	description = nautilus_preference_get_description (preference);
	
	g_assert (description != NULL);
	
	item->details->child = nautilus_text_caption_new ();

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (item->details->child), description);

	g_free (description);

	current_value = nautilus_preferences_get (item->details->preference_name, "file://home/pavel");

	g_assert (current_value != NULL);
	nautilus_text_caption_set_text (NAUTILUS_TEXT_CAPTION (item->details->child), current_value);
	g_free (current_value);
	
 	gtk_signal_connect (GTK_OBJECT (item->details->child),
 			    "changed",
 			    GTK_SIGNAL_FUNC (editable_string_changed_callback),
 			    (gpointer) item);
}

static void
preferences_item_create_font_family (NautilusPreferencesItem	*item,
				     const NautilusPreference	*preference)
{
	char			*description;
	char			*current_value;
	NautilusStringList	*font_list;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);
	description = nautilus_preference_get_description (preference);

	g_assert (description != NULL);

	item->details->child = nautilus_string_picker_new ();

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (item->details->child), description);
	
	g_free (description);

	/* FIXME bugzilla.eazel.com 1274: Need to query system for available fonts */
	font_list = nautilus_string_list_new ();

	nautilus_string_list_insert (font_list, "helvetica");
	nautilus_string_list_insert (font_list, "times");
	nautilus_string_list_insert (font_list, "courier");
	nautilus_string_list_insert (font_list, "lucida");
	nautilus_string_list_insert (font_list, "fixed");

	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (item->details->child), font_list);

	current_value = nautilus_preferences_get (item->details->preference_name, "helvetica");

	g_assert (current_value != NULL);
	g_assert (nautilus_string_list_contains (font_list, current_value));

	nautilus_string_picker_set_text (NAUTILUS_STRING_PICKER (item->details->child), current_value);

	g_free (current_value);

	nautilus_string_list_free (font_list);
	
 	gtk_signal_connect (GTK_OBJECT (item->details->child),
 			    "changed",
 			    GTK_SIGNAL_FUNC (text_item_changed_callback),
 			    (gpointer) item);
}

/* utility to determine if an image file exists in the candidate directory */

static const char *icon_file_name_suffixes[] =
{
	".svg",
	".SVG",
	".png",
	".PNG"
};

static gboolean
has_image_file(const char *directory_uri, const char *dir_name, const char *required_file)
{
	char *temp_str, *base_uri;
	int index;
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;
	
	file_info = gnome_vfs_file_info_new ();
	
	temp_str = nautilus_make_path(directory_uri, dir_name);
	base_uri = nautilus_make_path(temp_str, required_file);
	g_free(temp_str);
	
	for (index = 0; index < NAUTILUS_N_ELEMENTS (icon_file_name_suffixes); index++) {
		temp_str = g_strconcat (base_uri, icon_file_name_suffixes[index], NULL);
		gnome_vfs_file_info_init (file_info);
		result = gnome_vfs_get_file_info (temp_str, file_info, 0);
		g_free(temp_str);
		if (result == GNOME_VFS_OK) {
			g_free(base_uri);
			gnome_vfs_file_info_unref (file_info);
			return TRUE;	
		}
	}
	
	gnome_vfs_file_info_unref (file_info);
	g_free(base_uri);
	return FALSE;
}

/* add available icon themes to the theme list by iterating through the
   nautilus icons directory, looking for sub-directories */
static void
add_icon_themes(NautilusStringList *theme_list, char *required_file)
{
	char *directory_uri;
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;
	char *pixmap_directory;
		
	pixmap_directory = nautilus_get_pixmap_directory ();

	/* get the uri for the images directory */
	directory_uri = nautilus_get_uri_from_local_path (pixmap_directory);
	g_free (pixmap_directory);
			
	result = gnome_vfs_directory_list_load (&list, directory_uri,
					       GNOME_VFS_FILE_INFO_DEFAULT, NULL);
	if (result != GNOME_VFS_OK) {
		g_free(directory_uri);
		return;
	}

	/* interate through the directory for each file */
	current_file_info = gnome_vfs_directory_list_first(list);
	while (current_file_info != NULL) {
		if ((current_file_info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) &&
			(current_file_info->name[0] != '.'))
			if (has_image_file(directory_uri, current_file_info->name, required_file))
				nautilus_string_list_insert (theme_list, current_file_info->name);	
		current_file_info = gnome_vfs_directory_list_next(list);
	}
	
	g_free(directory_uri);
	gnome_vfs_directory_list_destroy(list);
}

static void
preferences_item_create_theme (NautilusPreferencesItem	*item,
				     const NautilusPreference	*preference)
{
	char			*description;
	char			*current_value;
	NautilusStringList	*theme_list;

	g_assert (item != NULL);
	g_assert (preference != NULL);

	g_assert (item->details->preference_name != NULL);
	description = nautilus_preference_get_description (preference);

	g_assert (description != NULL);

	item->details->child = nautilus_string_picker_new ();

	nautilus_caption_set_title_label (NAUTILUS_CAPTION (item->details->child), description);
	
	g_free (description);

	theme_list = nautilus_string_list_new ();
	nautilus_string_list_insert (theme_list, "default");
	add_icon_themes(theme_list, "i-directory");
	
	nautilus_string_picker_set_string_list (NAUTILUS_STRING_PICKER (item->details->child), theme_list);

	current_value = nautilus_preferences_get (item->details->preference_name, "default");

	g_assert (current_value != NULL);
	g_assert (nautilus_string_list_contains (theme_list, current_value));
	
	nautilus_string_picker_set_text (NAUTILUS_STRING_PICKER (item->details->child), current_value);

	g_free (current_value);

	nautilus_string_list_free (theme_list);
	
 	gtk_signal_connect (GTK_OBJECT (item->details->child),
 			    "changed",
 			    GTK_SIGNAL_FUNC (text_item_changed_callback),
 			    (gpointer) item);
}

/* NautilusPreferencesItem public methods */
GtkWidget *
nautilus_preferences_item_new (const gchar			*preference_name,
			       NautilusPreferencesItemType	item_type)
{
	NautilusPreferencesItem * item;

	g_return_val_if_fail (preference_name != NULL, NULL);

	item = gtk_type_new (nautilus_preferences_item_get_type ());

	/* Cast away the constness so that the preferences object can be
	 * refed in this object. */
	preferences_item_construct (item, preference_name, item_type);

	return GTK_WIDGET (item);
}

static void
enum_radio_group_changed_callback (GtkWidget *buttons, GtkWidget * button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	const NautilusPreference	*preference;
	gint				i;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));
	
	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->preference_name != NULL);

	preference = nautilus_preference_find_by_name (item->details->preference_name);

	i = nautilus_radio_button_group_get_active_index (NAUTILUS_RADIO_BUTTON_GROUP (buttons));

	nautilus_preferences_set_enum (item->details->preference_name,
				       nautilus_preference_enum_get_nth_entry_value (preference, i));
}

static void
boolean_button_toggled_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	gboolean		active_state;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));
	
	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	active_state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

	nautilus_preferences_set_boolean (item->details->preference_name, active_state);
}

static void
text_item_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	char			*text;

	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (NAUTILUS_IS_STRING_PICKER (item->details->child));

	text = nautilus_string_picker_get_text (NAUTILUS_STRING_PICKER (item->details->child));

	if (text != NULL)
	{
		nautilus_preferences_set (item->details->preference_name, text);
		
		g_free (text);
	}
}
static void
editable_string_changed_callback (GtkWidget *button, gpointer user_data)
{
	NautilusPreferencesItem	*item;
	char			*text;
	
	g_assert (user_data != NULL);
	g_assert (NAUTILUS_IS_PREFERENCES_ITEM (user_data));

	item = NAUTILUS_PREFERENCES_ITEM (user_data);

	g_assert (item->details->child != NULL);
	g_assert (NAUTILUS_IS_TEXT_CAPTION (item->details->child));

	text = nautilus_text_caption_get_text (NAUTILUS_TEXT_CAPTION (item->details->child));
	
	if (text != NULL)
	{
		nautilus_preferences_set (item->details->preference_name, text);
		
		g_free (text);
	}
}

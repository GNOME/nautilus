/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Author: Michael Meeks <michael@ximian.com>
*/
#include <config.h>

#include <libgnome/gnome-macros.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-global-preferences.h>

#include "fm-icon-container.h"

#define ICON_TEXT_ATTRIBUTES_NUM_ITEMS		3
#define ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS	"size,date_modified,type"

GNOME_CLASS_BOILERPLATE (FMIconContainer, fm_icon_container,
			 NautilusIconContainer,
			 nautilus_icon_container_get_type ())

static FMIconView *
get_icon_view (NautilusIconContainer *container)
{
	g_assert (FM_IS_ICON_CONTAINER (container));

	return FM_ICON_CONTAINER (container)->view;
}

static NautilusScalableIcon *
fm_icon_container_get_icon_images (NautilusIconContainer *container,
				   NautilusIconData      *data,
				   const char            *modifier,
				   GList                **emblem_icons)
{
	FMIconView *icon_view;
	EelStringList *emblems_to_ignore;
	NautilusFile *file;

	file = (NautilusFile *) data;

	g_assert (NAUTILUS_IS_FILE (file));
	icon_view = get_icon_view (container);
	g_return_val_if_fail (icon_view != NULL, NULL);

	if (emblem_icons != NULL) {
		emblems_to_ignore = fm_directory_view_get_emblem_names_to_exclude 
			(FM_DIRECTORY_VIEW (icon_view));
		*emblem_icons = nautilus_icon_factory_get_emblem_icons_for_file
			(file, emblems_to_ignore);
		eel_string_list_free (emblems_to_ignore);
	}

	return nautilus_icon_factory_get_icon_for_file (file, modifier);
}

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static EelStringList *
fm_icon_container_get_icon_text_attributes_from_preferences (void)
{
	static const EelStringList *attributes;

	if (attributes == NULL) {
		eel_preferences_add_auto_string_list (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
						      &attributes);
	}

	/* A simple check that the attributes list matches the expected length */
	g_return_val_if_fail (eel_string_list_get_length (attributes) == ICON_TEXT_ATTRIBUTES_NUM_ITEMS,
			      eel_string_list_new_from_tokens (ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS, ",", TRUE));


	/* We don't need to sanity check the attributes list even though it came
	 * from preferences.
	 *
	 * There are 2 ways that the values in the list could be bad.
	 *
	 * 1) The user picks "bad" values.  "bad" values are those that result in 
	 *    there being duplicate attributes in the list.
	 *
	 * 2) Value stored in GConf are tampered with.  Its possible physically do
	 *    this by pulling the rug underneath GConf and manually editing its
	 *    config files.  Its also possible to use a third party GConf key 
	 *    editor and store garbage for the keys in question.
	 *
	 * Thankfully, the Nautilus preferences machinery deals with both of 
	 * these cases.
	 *
	 * In the first case, the preferences dialog widgetry prevents
	 * duplicate attributes by making "bad" choices insensitive.
	 *
	 * In the second case, the preferences getter (and also the auto storage) for
	 * string_list values are always valid members of the enumeration associated
	 * with the preference.
	 *
	 * So, no more error checking on attributes is needed here and we can return
	 * a copy of the auto stored value.
	 */
	return eel_string_list_copy (attributes);
}

/**
 * fm_icon_view_get_icon_text_attribute_names:
 *
 * Get a string representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Use g_free to free the result.
 * @view: FMIconView to query.
 * 
 * Return value: A |-delimited string comprising attribute names, e.g. "name|size".
 * 
 **/
static char *
fm_icon_container_get_icon_text_attribute_names (NautilusIconContainer *container)
{
	EelStringList *attributes;
	char *result;
	int piece_count;

	const int pieces_by_level[] = {
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALLEST */
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALLER */
		0,	/* NAUTILUS_ZOOM_LEVEL_SMALL */
		1,	/* NAUTILUS_ZOOM_LEVEL_STANDARD */
		2,	/* NAUTILUS_ZOOM_LEVEL_LARGE */
		2,	/* NAUTILUS_ZOOM_LEVEL_LARGER */
		3	/* NAUTILUS_ZOOM_LEVEL_LARGEST */
	};

	piece_count = pieces_by_level[nautilus_icon_container_get_zoom_level (container)];
	
	attributes = fm_icon_container_get_icon_text_attributes_from_preferences ();
	g_return_val_if_fail ((guint)piece_count <= eel_string_list_get_length (attributes), NULL);
	
	result = eel_string_list_as_string (attributes, "|", piece_count);
	eel_string_list_free (attributes);

	return result;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
fm_icon_container_get_icon_text (NautilusIconContainer *container,
				 NautilusIconData      *data,
				 char                 **editable_text,
				 char                 **additional_text)
{
	char *actual_uri, *path;
	gchar *description;
	char *attribute_names;
	char **text_array;
	int i , slot_index;
	char *attribute_string;
	FMIconView *icon_view;
	NautilusFile *file;

	file = NAUTILUS_FILE (data);

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (editable_text != NULL);
	g_assert (additional_text != NULL);
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);

	/* In the smallest zoom mode, no text is drawn. */
	if (nautilus_icon_container_get_zoom_level (container) == NAUTILUS_ZOOM_LEVEL_SMALLEST) {
		*editable_text = NULL;
	} else {
		/* Strip the suffix for nautilus object xml files. */
		*editable_text = nautilus_file_get_display_name (file);
	}

	/* Handle link files specially. */
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.gnome.org 42531: Does sync. I/O and works only locally. */
		actual_uri = nautilus_file_get_uri (file);
		path = gnome_vfs_get_local_path_from_uri (actual_uri);
		g_free (actual_uri);
 		*additional_text = NULL;
		if (path != NULL) {
			description = nautilus_link_local_get_additional_text (path);
			g_free (path);
			if (description)
				*additional_text = g_strdup_printf (" \n%s\n ", description);
			g_free (description);
		}
		/* Don't show the normal extra information for desktop files, it doesn't
		 * make sense. */
		return;
	}
	
	/* Find out what attributes go below each icon. */
	attribute_names = fm_icon_container_get_icon_text_attribute_names (container);
	text_array = g_strsplit (attribute_names, "|", 0);
	g_free (attribute_names);

	/* Get the attributes. */
	for (i = 0; text_array[i] != NULL; i++)	{
		/* if the attribute is "none", delete the array slot */
		while (eel_strcmp (text_array[i], "none") == 0) {
			g_free (text_array[i]);
			text_array[i] = NULL;
			slot_index = i + 1;			
			while (text_array[slot_index] != NULL) {
				text_array[slot_index - 1] = text_array[slot_index];
				text_array[slot_index++] = NULL;
			}
			if (text_array[i] == NULL)
				break;
		} 
		
		if (text_array[i] == NULL)
			break;
			
		attribute_string = nautilus_file_get_string_attribute_with_default
			(file, text_array[i]);
				
		/* Replace each attribute name in the array with its string value */
		g_free (text_array[i]);
		text_array[i] = attribute_string;
	}

	/* Return them. */
	*additional_text = g_strjoinv ("\n", text_array);

	g_strfreev (text_array);
}

static char *
get_local_path (NautilusFile *file)
{
	char *uri, *local_path;

	uri = nautilus_file_get_uri (file);
	local_path = gnome_vfs_get_local_path_from_uri (uri);
	g_free (uri);
	return local_path;
}

/* Sort as follows:
 *   1) home link
 *   2) mount links
 *   3) other
 *   4) trash link
 */
typedef enum {
	SORT_HOME_LINK,
	SORT_MOUNT_LINK,
	SORT_OTHER,
	SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (NautilusFile *file)
{
	char *path;
	SortCategory category;

	if (!nautilus_file_is_nautilus_link (file)) {
		category = SORT_OTHER;
	} else {
		path = get_local_path (file);
		g_return_val_if_fail (path != NULL, SORT_OTHER);
		
		switch (nautilus_link_local_get_link_type (path)) {
		case NAUTILUS_LINK_HOME:
			category = SORT_HOME_LINK;
			break;
		case NAUTILUS_LINK_MOUNT:
			category = SORT_MOUNT_LINK;
			break;
		case NAUTILUS_LINK_TRASH:
			category = SORT_TRASH_LINK;
			break;
		default:
			category = SORT_OTHER;
			break;
		}
		
		g_free (path);
	}
	
	return category;
}

static int
fm_desktop_icon_container_icons_compare (NautilusIconContainer *container,
					 NautilusIconData      *data_a,
					 NautilusIconData      *data_b)
{
	NautilusFile *file_a;
	NautilusFile *file_b;
	FMDirectoryView *directory_view;
	SortCategory category_a, category_b;

	file_a = (NautilusFile *) data_a;
	file_b = (NautilusFile *) data_b;

	directory_view = FM_DIRECTORY_VIEW (FM_ICON_CONTAINER (container)->view);
	g_return_val_if_fail (directory_view != NULL, 0);
	
	category_a = get_sort_category (file_a);
	category_b = get_sort_category (file_b);

	if (category_a == category_b) {
		return nautilus_file_compare_for_sort 
			(file_a, file_b, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, 
			 fm_directory_view_should_sort_directories_first (directory_view),
			 FALSE);
	}

	if (category_a < category_b) {
		return -1;
	} else {
		return +1;
	}
}

static int
fm_icon_container_compare_icons (NautilusIconContainer *container,
				 NautilusIconData      *icon_a,
				 NautilusIconData      *icon_b)
{
	FMIconView *icon_view;

	icon_view = get_icon_view (container);
	g_return_val_if_fail (icon_view != NULL, 0);

	if (FM_ICON_CONTAINER (container)->sort_for_desktop) {
		return fm_desktop_icon_container_icons_compare
			(container, icon_a, icon_b);
	}

	return fm_icon_view_compare_files (icon_view,
					   NAUTILUS_FILE (icon_a),
					   NAUTILUS_FILE (icon_b));
}

static int
fm_icon_container_compare_icons_by_name (NautilusIconContainer *container,
					 NautilusIconData      *icon_a,
					 NautilusIconData      *icon_b)
{
	return nautilus_file_compare_for_sort
		(NAUTILUS_FILE (icon_a),
		 NAUTILUS_FILE (icon_b),
		 NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
		 FALSE, FALSE);
}

static void
fm_icon_container_dispose (GObject *object)
{
	FMIconContainer *icon_container;

	icon_container = FM_ICON_CONTAINER (object);

	icon_container->view = NULL;

	GNOME_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
fm_icon_container_class_init (FMIconContainerClass *klass)
{
	NautilusIconContainerClass *ic_class;

	ic_class = &klass->parent_class;

	ic_class->get_icon_text = fm_icon_container_get_icon_text;
	ic_class->get_icon_images = fm_icon_container_get_icon_images;

	ic_class->compare_icons = fm_icon_container_compare_icons;
	ic_class->compare_icons_by_name = fm_icon_container_compare_icons_by_name;

	G_OBJECT_CLASS (klass)->dispose = fm_icon_container_dispose;
}

static void
fm_icon_container_instance_init (FMIconContainer *icon_container)
{
}

NautilusIconContainer *
fm_icon_container_construct (FMIconContainer *icon_container, FMIconView *view)
{
	g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

	icon_container->view = view;

	return NAUTILUS_ICON_CONTAINER (icon_container);
}

NautilusIconContainer *
fm_icon_container_new (FMIconView *view)
{
	return fm_icon_container_construct
		(g_object_new (FM_TYPE_ICON_CONTAINER, NULL),
		 view);
}

void
fm_icon_container_set_sort_desktop (FMIconContainer *container,
				    gboolean         desktop)
{
	container->sort_for_desktop = desktop;
}

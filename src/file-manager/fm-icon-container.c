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

#include <string.h>
#include <libgnome/gnome-macros.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-thumbnails.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>

#include "fm-icon-container.h"

#define ICON_TEXT_ATTRIBUTES_NUM_ITEMS		3
#define ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS	"size,date_modified,type"

GNOME_CLASS_BOILERPLATE (FMIconContainer, fm_icon_container,
			 NautilusIconContainer,
			 nautilus_icon_container_get_type ())

static GQuark attribute_none_q;

static FMIconView *
get_icon_view (NautilusIconContainer *container)
{
	/* Type unsafe comparison for performance */
	return ((FMIconContainer *)container)->view;
}

static NautilusIconInfo *
fm_icon_container_get_icon_images (NautilusIconContainer *container,
				   NautilusIconData      *data,
				   int                    size,
				   GList                **emblem_pixbufs,
				   char                 **embedded_text,
				   gboolean               for_drag_accept,
				   gboolean               need_large_embeddded_text,
				   gboolean              *embedded_text_needs_loading,
				   gboolean              *has_window_open)
{
	FMIconView *icon_view;
	char **emblems_to_ignore;
	NautilusFile *file;
	gboolean use_embedding;
	NautilusFileIconFlags flags;
	guint emblem_size;

	file = (NautilusFile *) data;

	g_assert (NAUTILUS_IS_FILE (file));
	icon_view = get_icon_view (container);
	g_return_val_if_fail (icon_view != NULL, NULL);

	use_embedding = FALSE;
	if (embedded_text) {
		*embedded_text = nautilus_file_peek_top_left_text (file, need_large_embeddded_text, embedded_text_needs_loading);
		use_embedding = *embedded_text != NULL;
	}
	
	if (emblem_pixbufs != NULL) {
		emblem_size = nautilus_icon_get_emblem_size_for_icon_size (size);
		/* don't return images larger than the actual icon size */
		emblem_size = MIN (emblem_size, size);

		if (emblem_size > 0) {
			emblems_to_ignore = fm_directory_view_get_emblem_names_to_exclude 
				(FM_DIRECTORY_VIEW (icon_view));
			*emblem_pixbufs = nautilus_file_get_emblem_pixbufs (file,
									    emblem_size,
									    FALSE,
									    emblems_to_ignore);
			g_strfreev (emblems_to_ignore);
		}
	}

	*has_window_open = nautilus_file_has_open_window (file);

	flags = 0;
	if (!fm_icon_view_is_compact (icon_view) ||
	    nautilus_icon_container_get_zoom_level (container) > NAUTILUS_ZOOM_LEVEL_STANDARD) {
		flags |= NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS;
	}

	if (use_embedding) {
		flags |= NAUTILUS_FILE_ICON_FLAGS_EMBEDDING_TEXT;
	}
	if (for_drag_accept) {
		flags |= NAUTILUS_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
	}
	
	return nautilus_file_get_icon (file, size, flags);
}

static char *
fm_icon_container_get_icon_description (NautilusIconContainer *container,
				        NautilusIconData      *data)
{
	NautilusFile *file;
	char *mime_type;
	const char *description;

	file = NAUTILUS_FILE (data);
	g_assert (NAUTILUS_IS_FILE (file));

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		return NULL;
	}

	mime_type = nautilus_file_get_mime_type (file);
	description = g_content_type_get_description (mime_type);
	g_free (mime_type);
	return g_strdup (description);
}

static void
fm_icon_container_start_monitor_top_left (NautilusIconContainer *container,
					  NautilusIconData      *data,
					  gconstpointer          client,
					  gboolean               large_text)
{
	NautilusFile *file;
	NautilusFileAttributes attributes;
		
	file = (NautilusFile *) data;

	g_assert (NAUTILUS_IS_FILE (file));

	attributes = NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT;
	if (large_text) {
		attributes |= NAUTILUS_FILE_ATTRIBUTE_LARGE_TOP_LEFT_TEXT;
	}
	nautilus_file_monitor_add (file, client, attributes);
}

static void
fm_icon_container_stop_monitor_top_left (NautilusIconContainer *container,
					 NautilusIconData      *data,
					 gconstpointer          client)
{
	NautilusFile *file;

	file = (NautilusFile *) data;

	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_file_monitor_remove (file, client);
}

static void
fm_icon_container_prioritize_thumbnailing (NautilusIconContainer *container,
					   NautilusIconData      *data)
{
	NautilusFile *file;
	char *uri;

	file = (NautilusFile *) data;

	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_thumbnailing (file)) {
		uri = nautilus_file_get_uri (file);
		nautilus_thumbnail_prioritize (uri);
		g_free (uri);
	}
}

static void
update_captions (GQuark **attributes_p)
{
	char **attribute_names;
	int i;

	attribute_names = eel_preferences_get_string_array (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

	g_free (*attributes_p);
	*attributes_p = g_new (GQuark, g_strv_length (attribute_names) + 1);

	for (i = 0; attribute_names[i] != NULL; ++i) {
		(*attributes_p)[i] = g_quark_from_string (attribute_names[i]);
	}
	(*attributes_p)[i] = 0;
	
	g_strfreev (attribute_names);
}

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static GQuark *
fm_icon_container_get_icon_text_attributes_from_preferences (void)
{
	static GQuark *attributes = NULL;

	if (attributes == NULL) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
					      (EelPreferencesCallback)update_captions,
					      &attributes);
		update_captions (&attributes);
	}
	
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
	 * string_array values are always valid members of the enumeration associated
	 * with the preference.
	 *
	 * So, no more error checking on attributes is needed here and we can return
	 * a the auto stored value.
	 */
	return attributes;
}

static int
quarkv_length (GQuark *attributes)
{
	int i;
	i = 0;
	while (attributes[i] != 0) {
		i++;
	}
	return i;
}

/**
 * fm_icon_view_get_icon_text_attribute_names:
 *
 * Get a list representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Don't free the result.
 * @view: FMIconView to query.
 * 
 **/
static GQuark *
fm_icon_container_get_icon_text_attribute_names (NautilusIconContainer *container,
						 int *len)
{
	GQuark *attributes;
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

	*len = MIN (piece_count, quarkv_length (attributes));

	return attributes;
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
	char *actual_uri;
	gchar *description;
	GQuark *attributes;
	char *text_array[4];
	int i, j, num_attributes;
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

	if (fm_icon_view_is_compact (icon_view)) {
		*additional_text = NULL;
		return;
	}

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		/* Don't show the normal extra information for desktop icons, it doesn't
		 * make sense. */
 		*additional_text = NULL;
		return;
	}
	
	/* Handle link files specially. */
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.gnome.org 42531: Does sync. I/O and works only locally. */
 		*additional_text = NULL;
		if (nautilus_file_is_local (file)) {
			actual_uri = nautilus_file_get_uri (file);
			description = nautilus_link_local_get_additional_text (actual_uri);
			if (description)
				*additional_text = g_strdup_printf (" \n%s\n ", description);
			g_free (description);
			g_free (actual_uri);
		}
		/* Don't show the normal extra information for desktop files, it doesn't
		 * make sense. */
		return;
	}

	/* Find out what attributes go below each icon. */
	attributes = fm_icon_container_get_icon_text_attribute_names (container,
									   &num_attributes);

	/* Get the attributes. */
	j = 0;
	for (i = 0; i < num_attributes; ++i) {
		if (attributes[i] == attribute_none_q) {
			continue;
		}

		text_array[j++] =
			nautilus_file_get_string_attribute_with_default_q (file, attributes[i]);
	}
	text_array[j] = NULL;

	/* Return them. */
	if (j == 0) {
		*additional_text = NULL;
	} else if (j == 1) {
		/* Only one item, avoid the strdup + free */
		*additional_text = text_array[0];
	} else {
		*additional_text = g_strjoinv ("\n", text_array);
		
		for (i = 0; i < j; i++) {
			g_free (text_array[i]);
		}
	}
}

/* Sort as follows:
 *   0) computer link
 *   1) home link
 *   2) network link
 *   3) mount links
 *   4) other
 *   5) trash link
 */
typedef enum {
	SORT_COMPUTER_LINK,
	SORT_HOME_LINK,
	SORT_NETWORK_LINK,
	SORT_MOUNT_LINK,
	SORT_OTHER,
	SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (NautilusFile *file)
{
	NautilusDesktopLink *link;
	SortCategory category;

	category = SORT_OTHER;
	
	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));
		if (link != NULL) {
			switch (nautilus_desktop_link_get_link_type (link)) {
			case NAUTILUS_DESKTOP_LINK_COMPUTER:
				category = SORT_COMPUTER_LINK;
				break;
			case NAUTILUS_DESKTOP_LINK_HOME:
				category = SORT_HOME_LINK;
				break;
			case NAUTILUS_DESKTOP_LINK_MOUNT:
				category = SORT_MOUNT_LINK;
				break;
			case NAUTILUS_DESKTOP_LINK_TRASH:
				category = SORT_TRASH_LINK;
				break;
			case NAUTILUS_DESKTOP_LINK_NETWORK:
				category = SORT_NETWORK_LINK;
				break;
			default:
				category = SORT_OTHER;
				break;
			}
			g_object_unref (link);
		}
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

	/* Type unsafe comparisons for performance */
	return fm_icon_view_compare_files (icon_view,
					   (NautilusFile *)icon_a,
					   (NautilusFile *)icon_b);
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
fm_icon_container_freeze_updates (NautilusIconContainer *container)
{
	FMIconView *icon_view;
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);
	fm_directory_view_freeze_updates (FM_DIRECTORY_VIEW (icon_view));
}

static void
fm_icon_container_unfreeze_updates (NautilusIconContainer *container)
{
	FMIconView *icon_view;
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);
	fm_directory_view_unfreeze_updates (FM_DIRECTORY_VIEW (icon_view));
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

	attribute_none_q = g_quark_from_static_string ("none");
	
	ic_class->get_icon_text = fm_icon_container_get_icon_text;
	ic_class->get_icon_images = fm_icon_container_get_icon_images;
	ic_class->get_icon_description = fm_icon_container_get_icon_description;
	ic_class->start_monitor_top_left = fm_icon_container_start_monitor_top_left;
	ic_class->stop_monitor_top_left = fm_icon_container_stop_monitor_top_left;
	ic_class->prioritize_thumbnailing = fm_icon_container_prioritize_thumbnailing;

	ic_class->compare_icons = fm_icon_container_compare_icons;
	ic_class->compare_icons_by_name = fm_icon_container_compare_icons_by_name;
	ic_class->freeze_updates = fm_icon_container_freeze_updates;
	ic_class->unfreeze_updates = fm_icon_container_unfreeze_updates;

	G_OBJECT_CLASS (klass)->dispose = fm_icon_container_dispose;
}

static void
fm_icon_container_instance_init (FMIconContainer *icon_container)
{
}

NautilusIconContainer *
fm_icon_container_construct (FMIconContainer *icon_container, FMIconView *view)
{
	AtkObject *atk_obj;

	g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

	icon_container->view = view;
	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (icon_container));
	atk_object_set_name (atk_obj, _("Icon View"));

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

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
 * This program is distributed in the hope that it will be useful,
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

/* This is the index title widget, which is the title part of the index panel
 */

#include <config.h>
#include "nautilus-sidebar-title.h"
#include "nautilus-sidebar.h"

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-font-extensions.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-search-uri.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-theme.h>
#include <libnautilus-extensions/nautilus-label-with-background.h>
#include <libnautilus-extensions/nautilus-image-with-background.h>

static void                nautilus_sidebar_title_initialize_class (NautilusSidebarTitleClass *klass);
static void                nautilus_sidebar_title_destroy          (GtkObject                 *object);
static void                nautilus_sidebar_title_initialize       (NautilusSidebarTitle      *pixmap);
static void                nautilus_sidebar_title_size_allocate    (GtkWidget                 *widget,
								    GtkAllocation             *allocation);
static void                nautilus_sidebar_title_theme_changed    (gpointer                   user_data);
static void                update_icon                             (NautilusSidebarTitle      *sidebar_title);
static GtkWidget *         sidebar_title_create_title_label        (void);
static GtkWidget *         sidebar_title_create_more_info_label    (void);
static void                smooth_graphics_mode_changed_callback   (gpointer                   callback_data);
static NautilusBackground *nautilus_sidebar_title_background       (NautilusSidebarTitle      *sidebar_title);

struct NautilusSidebarTitleDetails {
	NautilusFile		*file;
	guint			file_changed_connection;
	char			*title_text;
	GtkWidget		*icon;
	GtkWidget		*title_label;
	GtkWidget		*more_info_label;
	GtkWidget		*emblem_box;
	GtkWidget		*notes;

	int			shadow_offset;
};

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSidebarTitle, nautilus_sidebar_title, gtk_vbox_get_type ())

static void
nautilus_sidebar_title_initialize_class (NautilusSidebarTitleClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	
	object_class->destroy = nautilus_sidebar_title_destroy;
	widget_class->size_allocate = nautilus_sidebar_title_size_allocate;
}

static void
appearance_changed_callback (NautilusBackground *background, NautilusSidebarTitle *sidebar_title)
{
	nautilus_sidebar_title_select_text_color (sidebar_title);
}		

static void
realize_callback (NautilusSidebarTitle *sidebar_title)
{
	NautilusBackground *background;
	
	background = nautilus_sidebar_title_background (sidebar_title);

	g_return_if_fail (background != NULL);

	gtk_signal_connect_while_alive (GTK_OBJECT (background),
					"appearance_changed",
					appearance_changed_callback,
					sidebar_title,
					GTK_OBJECT (sidebar_title));
}

static void
nautilus_sidebar_title_initialize (NautilusSidebarTitle *sidebar_title)
{ 
	sidebar_title->details = g_new0 (NautilusSidebarTitleDetails, 1);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_icon,
					       GTK_OBJECT (sidebar_title));
	gtk_signal_connect (GTK_OBJECT (sidebar_title), "realize", realize_callback, NULL);

	/* Create the icon */
	sidebar_title->details->icon = nautilus_image_new_with_background (NULL);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->icon, 0, 0, 0);
	gtk_widget_show (sidebar_title->details->icon);

	/* Create the title label */
	sidebar_title->details->title_label = sidebar_title_create_title_label ();
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->title_label, 0, 0, 0);
	gtk_widget_show (sidebar_title->details->title_label);

	/* Create the more info label */
	sidebar_title->details->more_info_label = sidebar_title_create_more_info_label ();
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->more_info_label, 0, 0, 0);
	gtk_widget_show (sidebar_title->details->more_info_label);

	sidebar_title->details->emblem_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (sidebar_title->details->emblem_box);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->emblem_box, 0, 0, 0);

	sidebar_title->details->notes = GTK_WIDGET (gtk_label_new (NULL));
	gtk_label_set_line_wrap (GTK_LABEL (sidebar_title->details->notes), TRUE);
	gtk_widget_show (sidebar_title->details->notes);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->notes, 0, 0, 0);

	/* Keep track of changes in graphics trade offs */
	smooth_graphics_mode_changed_callback (sidebar_title);
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
					   smooth_graphics_mode_changed_callback, 
					   sidebar_title);

	/* set up the label colors according to the theme, and get notified of changes */
	nautilus_sidebar_title_theme_changed (sidebar_title);
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, nautilus_sidebar_title_theme_changed, sidebar_title);

}

/* destroy by throwing away private storage */

static void
release_file (NautilusSidebarTitle *sidebar_title)
{
	if (sidebar_title->details->file_changed_connection != 0) {
		gtk_signal_disconnect (GTK_OBJECT (sidebar_title->details->file),
				       sidebar_title->details->file_changed_connection);
		sidebar_title->details->file_changed_connection = 0;
	}

	if (sidebar_title->details->file != NULL) {
		nautilus_file_monitor_remove (sidebar_title->details->file, sidebar_title);
		nautilus_file_unref (sidebar_title->details->file);
		sidebar_title->details->file = NULL;
	}
}

static void
nautilus_sidebar_title_destroy (GtkObject *object)
{
	NautilusSidebarTitle *sidebar_title;

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (object);

	release_file (sidebar_title);
	
	g_free (sidebar_title->details->title_text);
	g_free (sidebar_title->details);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME, nautilus_sidebar_title_theme_changed, sidebar_title);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, smooth_graphics_mode_changed_callback, sidebar_title);
  	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* return a new index title object */

GtkWidget *
nautilus_sidebar_title_new (void)
{
	return gtk_widget_new (nautilus_sidebar_title_get_type (), NULL);
}

static NautilusBackground *
nautilus_sidebar_title_background (NautilusSidebarTitle *sidebar_title)
{
	GtkWidget *widget;
	GtkWidget *sidebar;
	NautilusBackground *background;

	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (sidebar_title), NULL);
	
	widget = GTK_WIDGET (sidebar_title)->parent;

	 
	if (widget != NULL) {
		sidebar = widget->parent;
		g_return_val_if_fail (NAUTILUS_IS_SIDEBAR (sidebar), NULL);
		background = nautilus_get_widget_background (sidebar);
		g_return_val_if_fail (NAUTILUS_IS_BACKGROUND (background), NULL);
		return background;
	} else {
		/* FIXME bugzilla.eazel.com 5042
		 * It would be preferable to assert widget != NULL and not have
		 * this else case. Doing this would require us to be carful when
		 * nautilus_sidebar_title_select_text_color is called - which would
		 * probably involve doing more of the sidebar_title initialization
		 * at realize time.
		 */
		return NULL;
	}
}

/* utility to determine if the sidebar is using the default theme */
static gboolean
nautilus_sidebar_title_background_is_default (NautilusSidebarTitle *sidebar_title)
{
	char *background_color, *background_image;
	gboolean is_default;
	
	background_color = nautilus_file_get_metadata (sidebar_title->details->file,
						       NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
						       NULL);
	background_image = nautilus_file_get_metadata (sidebar_title->details->file,
						       NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
						       NULL);
	
	is_default = background_color == NULL && background_image == NULL;
	g_free (background_color);
	g_free (background_image);
	
	return is_default;
}

/* utility that returns true if the title is over a dark background.  We do this by finding the
   sidebar and asking its background */
void
nautilus_sidebar_title_select_text_color (NautilusSidebarTitle *sidebar_title)
{
	NautilusBackground *background;
	char *sidebar_title_color;
	char *sidebar_info_title_color;
	char *sidebar_title_shadow_color;
	
	/* if the background is set to the default, the theme can explicitly define the title colors.  Check if
	 * the background has been customized and if the theme specified any colors
	 */
	sidebar_title_color = NULL;
	sidebar_info_title_color = NULL;
	sidebar_title_shadow_color = NULL;
	
	background = nautilus_sidebar_title_background (sidebar_title);
	if (background != NULL) {
		if (nautilus_sidebar_title_background_is_default (sidebar_title)) {
			sidebar_title_color = nautilus_theme_get_theme_data ("sidebar", "TITLE_COLOR");
			sidebar_info_title_color = nautilus_theme_get_theme_data ("sidebar", "TITLE_INFO_COLOR");
			sidebar_title_shadow_color = nautilus_theme_get_theme_data ("sidebar", "TITLE_SHADOW_COLOR"); 
		}
		
		if (sidebar_title_color == NULL) {
			/* FIXME bugzilla.eazel.com 2496: for now, both the title and info colors are the same */
			if (nautilus_background_is_dark (background)) {
				sidebar_title_color = g_strdup("rgb:FFFF/FFFF/FFFF");
				sidebar_info_title_color = g_strdup("rgb:FFFF/FFFF/FFFF");
				sidebar_title_shadow_color = g_strdup("rgb:0000/0000/0000");
				
			} else {
				sidebar_title_color = g_strdup("rgb:0000/0000/0000");
				sidebar_info_title_color = g_strdup("rgb:0000/0000/0000");
				sidebar_title_shadow_color = g_strdup("rgb:FFFF/FFFF/FFFF");
			}
		} else {
			if (sidebar_info_title_color == NULL) {
				sidebar_info_title_color = g_strdup (sidebar_title_color);
			}
			if (sidebar_title_shadow_color == NULL) {
				sidebar_title_shadow_color = g_strdup("rgb:FFFF/FFFF/FFFF");
			}
		}

		nautilus_label_set_text_color (NAUTILUS_LABEL (sidebar_title->details->title_label),
						       nautilus_parse_rgb_with_white_default (sidebar_title_color));
		
		nautilus_label_set_smooth_drop_shadow_color (NAUTILUS_LABEL (sidebar_title->details->title_label),
							      nautilus_parse_rgb_with_white_default (sidebar_title_shadow_color));
		
		nautilus_label_set_smooth_drop_shadow_offset (NAUTILUS_LABEL (sidebar_title->details->title_label),
							       sidebar_title->details->shadow_offset);
		
		nautilus_label_set_text_color (NAUTILUS_LABEL (sidebar_title->details->more_info_label),
						       nautilus_parse_rgb_with_white_default (sidebar_info_title_color));
		
		nautilus_label_set_smooth_drop_shadow_color (NAUTILUS_LABEL (sidebar_title->details->more_info_label),
							      nautilus_parse_rgb_with_white_default (sidebar_title_shadow_color));
		
		nautilus_label_set_smooth_drop_shadow_offset (NAUTILUS_LABEL (sidebar_title->details->more_info_label),
							       sidebar_title->details->shadow_offset);
		
		g_free (sidebar_title_color);	
		g_free (sidebar_info_title_color);	
		g_free (sidebar_title_shadow_color);
	}
}

/* handle theme changes by setting up the color of the labels */
static void
nautilus_sidebar_title_theme_changed (gpointer user_data)
{
	char *shadow_offset_str;
	NautilusSidebarTitle *sidebar_title;	

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (user_data);		
	
	shadow_offset_str = nautilus_theme_get_theme_data ("sidebar", "SHADOW_OFFSET");
	if (shadow_offset_str) {
		sidebar_title->details->shadow_offset = atoi (shadow_offset_str);	
		g_free (shadow_offset_str);
	} else {
		sidebar_title->details->shadow_offset = 1;	
	}
	
	nautilus_sidebar_title_select_text_color (sidebar_title);
}

/* set up the icon image */
static void
update_icon (NautilusSidebarTitle *sidebar_title)
{
	GdkPixbuf *pixbuf;
	char *uri, *icon_path;
	
	/* FIXME bugzilla.eazel.com 5043: currently, components can't specify their own sidebar icon.  This
	  needs to be added to the framework, but for now we special-case some
	  important ones here */
	
	uri = NULL;
	icon_path = NULL;
	if (sidebar_title->details->file) {
		uri = nautilus_file_get_uri (sidebar_title->details->file);
	}	
	
	if (nautilus_istr_has_prefix (uri, "eazel:") || nautilus_istr_has_prefix (uri, "eazel-services:")) {
		icon_path = nautilus_theme_get_image_path ("big_services_icon.png");
	} else if (nautilus_istr_has_prefix (uri, "http:")) {
		icon_path = nautilus_theme_get_image_path ("i-web-72.png");
	} else if (nautilus_istr_has_prefix (uri, "man:")) {
		icon_path = nautilus_theme_get_image_path ("manual.png");
	} else if (nautilus_istr_has_prefix (uri, "hardware:")) {
		icon_path = nautilus_theme_get_image_path ("computer.svg");
	}
	
	if (icon_path != NULL) {
		pixbuf = nautilus_icon_factory_get_pixbuf_from_name (icon_path, NULL, NAUTILUS_ICON_SIZE_LARGE, TRUE);
	} else if (nautilus_icon_factory_is_icon_ready_for_file (sidebar_title->details->file)) {
		pixbuf = nautilus_icon_factory_get_pixbuf_for_file (sidebar_title->details->file,
								    "accept",
								    NAUTILUS_ICON_SIZE_LARGE,
								    TRUE);
	} else {
		pixbuf = NULL;
	}

	g_free (uri);	
	g_free (icon_path);
	
	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (sidebar_title->details->icon), pixbuf);
}

static void
update_font (NautilusSidebarTitle *sidebar_title)
{
	guint largest_size;
	const guint font_sizes[4] = { 20, 18, 14, 12 };
	GdkFont *label_font;
	NautilusScalableFont *smooth_font;

	smooth_font = nautilus_label_get_smooth_font (NAUTILUS_LABEL (sidebar_title->details->title_label));
	
	largest_size = nautilus_scalable_font_largest_fitting_font_size (smooth_font,
									 sidebar_title->details->title_text,
									 GTK_WIDGET (sidebar_title)->allocation.width - 4,
									 font_sizes,
									 NAUTILUS_N_ELEMENTS (font_sizes));
	gtk_object_unref (GTK_OBJECT (smooth_font));
	
	nautilus_label_set_smooth_font_size (NAUTILUS_LABEL (sidebar_title->details->title_label), largest_size);
	
	/* FIXME bugzilla.eazel.com 1103: Make this use the font
	 * factory and be failsafe if the given font is not found.
	 */
		
	/* leave 4 pixels of slop so the text doesn't go right up to the edge */
	label_font = nautilus_get_largest_fitting_font
		(sidebar_title->details->title_text,
		 GTK_WIDGET (sidebar_title)->allocation.width - 4,
		 "-adobe-helvetica-bold-r-normal-*-%d-*-*-*-*-*-*-*");
	
	if (label_font == NULL) {
		label_font = nautilus_font_factory_get_fallback_font ();
	}
	
	nautilus_gtk_widget_set_font (sidebar_title->details->title_label, label_font);
	gdk_font_unref (label_font);
}

/* set up the filename label */
static void
update_title (NautilusSidebarTitle *sidebar_title)
{
	/* FIXME bugzilla.eazel.com 2500: We could defer showing the title until the icon is ready. */
	nautilus_label_set_text (NAUTILUS_LABEL (sidebar_title->details->title_label),
				  sidebar_title->details->title_text);

	update_font (sidebar_title);
}

static void
append_and_eat (GString *string, const char *separator, char *new_string)
{
	if (new_string == NULL) {
		return;
	}
	if (separator != NULL) {
		g_string_append (string, separator);
	}
	g_string_append (string, new_string);
	g_free (new_string);
}

static gboolean
file_is_search_location (NautilusFile *file)
{
	char *uri;
	gboolean is_search_uri;

	uri = nautilus_file_get_uri (file);
	is_search_uri = nautilus_is_search_uri (uri);
	g_free (uri);

	return is_search_uri;
}

static void
update_more_info (NautilusSidebarTitle *sidebar_title)
{
	NautilusFile *file;
	GString *info_string;
	char *type_string, *search_string, *search_uri;
	
	file = sidebar_title->details->file;
	
	/* FIXME bugzilla.eazel.com 2500: We could defer showing info until the icon is ready. */
	/* Adding this special case for search results to 
	   correspond to the fix for bug 2341.  */
	if (file != NULL && file_is_search_location (file)) {
		search_uri = nautilus_file_get_uri (file);
		search_string = nautilus_search_uri_to_human (search_uri);
		g_free (search_uri);
		info_string = g_string_new (search_string);
		g_free (search_string);
		append_and_eat (info_string, "\n ",
				nautilus_file_get_string_attribute (file, "size"));
	}
	else {
		info_string = g_string_new (NULL);
		type_string = nautilus_file_get_string_attribute (file, "type");
		if (type_string != NULL) {
			append_and_eat (info_string, NULL, type_string);
			append_and_eat (info_string, ", ",
					nautilus_file_get_string_attribute (file, "size"));
		} else {
			append_and_eat (info_string, NULL,
					nautilus_file_get_string_attribute (file, "size"));
		}
		append_and_eat (info_string, "\n",
				nautilus_file_get_string_attribute (file, "date_modified"));
		g_string_append_c (info_string, '\0');
	}

	nautilus_label_set_text (NAUTILUS_LABEL (sidebar_title->details->more_info_label),
				  info_string->str);

	g_string_free (info_string, TRUE);
}

/* add a pixbuf to the emblem box */
static void
add_emblem (NautilusSidebarTitle *sidebar_title, GdkPixbuf *pixbuf)
{
	GtkWidget *image_widget;

	image_widget = nautilus_image_new_with_background (NULL);
	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image_widget), pixbuf);	
  	gtk_widget_show (image_widget);
	gtk_container_add (GTK_CONTAINER (sidebar_title->details->emblem_box), image_widget);	
}

static void
update_emblems (NautilusSidebarTitle *sidebar_title)
{
	GList *icons, *p;
	GdkPixbuf *pixbuf;

	/* FIXME bugzilla.eazel.com 2500: We could defer showing emblems until the icon is ready. */
	/* exit if we don't have the file yet */
	if (sidebar_title->details->file == NULL) {
		return;
	}
	
	/* First, deallocate any existing ones */
	gtk_container_foreach (GTK_CONTAINER (sidebar_title->details->emblem_box),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);

	/* make sure we have the file */
	if (sidebar_title->details->file) {
	}
	
	/* fetch the emblem icons from metadata */
	icons = nautilus_icon_factory_get_emblem_icons_for_file (sidebar_title->details->file, FALSE, NULL);

	/* loop through the list of emblems, installing them in the box */
	for (p = icons; p != NULL; p = p->next) {
		pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data,
			 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD,
			 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD,
			 NULL, FALSE);
		if (pixbuf != NULL) {
			add_emblem (sidebar_title, pixbuf);
			gdk_pixbuf_unref (pixbuf);
		}
	}
	
	nautilus_scalable_icon_list_free (icons);
}

static void
update_notes (NautilusSidebarTitle *sidebar_title)
{
	char *text;
	
	/* FIXME bugzilla.eazel.com 2500: We could defer showing notes until the icon is ready. */

	text = nautilus_file_get_metadata (sidebar_title->details->file,
					   NAUTILUS_METADATA_KEY_NOTES,
					   NULL);
	gtk_label_set_text (GTK_LABEL (sidebar_title->details->notes), text);
	g_free (text);
}

/* return the filename text */
char *
nautilus_sidebar_title_get_text (NautilusSidebarTitle *sidebar_title)
{
	return g_strdup (sidebar_title->details->title_text);
}

/* set up the filename text */
void
nautilus_sidebar_title_set_text (NautilusSidebarTitle *sidebar_title,
				 const char* new_text)
{
	g_free (sidebar_title->details->title_text);
	sidebar_title->details->title_text = g_strdup (new_text);

	/* Recompute the displayed text. */
	update_title (sidebar_title);
}

static void
update_all (NautilusSidebarTitle *sidebar_title)
{
	update_icon (sidebar_title);
	
	update_title (sidebar_title);
	update_more_info (sidebar_title);
	nautilus_sidebar_title_select_text_color (sidebar_title);
	
	update_emblems (sidebar_title);
	update_notes (sidebar_title);
}

void
nautilus_sidebar_title_set_file (NautilusSidebarTitle *sidebar_title,
				 NautilusFile *file,
				 const char *initial_text)
{
	GList *attributes;

	if (file != sidebar_title->details->file) {
		release_file (sidebar_title);
		sidebar_title->details->file = file;
		nautilus_file_ref (sidebar_title->details->file);
	
		/* attach file */
		if (file != NULL) {
			sidebar_title->details->file_changed_connection =
				gtk_signal_connect_object (GTK_OBJECT (sidebar_title->details->file),
							   "changed",
							   update_all,
							   GTK_OBJECT (sidebar_title));
			
			/* Monitor the things needed to get the right
			 * icon. Also monitor a directory's item count because
			 * the "size" attribute is based on that.
			 */
			attributes = nautilus_icon_factory_get_required_file_attributes ();		
			attributes = g_list_prepend (attributes,
						     NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
			attributes = g_list_prepend (attributes,
						     NAUTILUS_FILE_ATTRIBUTE_METADATA);

			nautilus_file_monitor_add (sidebar_title->details->file, sidebar_title,
						   attributes);
			g_list_free (attributes);
		}
	}

	g_free (sidebar_title->details->title_text);
	sidebar_title->details->title_text = g_strdup (initial_text);

	update_all (sidebar_title);
}

static void
nautilus_sidebar_title_size_allocate (GtkWidget *widget,
				      GtkAllocation *allocation)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
	/* Need to update the font if the width changes. */
	update_font (NAUTILUS_SIDEBAR_TITLE (widget));
}

gboolean
nautilus_sidebar_title_hit_test_icon (NautilusSidebarTitle *sidebar_title, int x, int y)
{
	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (sidebar_title), FALSE);

	return nautilus_point_in_widget (sidebar_title->details->icon, x, y);
}

static GtkWidget *
sidebar_title_create_title_label (void)
{ 
	GtkWidget *title_label;

	title_label = nautilus_label_new_with_background ("");
	nautilus_label_make_bold (NAUTILUS_LABEL (title_label));
	nautilus_label_set_wrap (NAUTILUS_LABEL (title_label), TRUE);
	nautilus_label_set_justify (NAUTILUS_LABEL (title_label), GTK_JUSTIFY_CENTER);

	return title_label;
}

static GtkWidget *
sidebar_title_create_more_info_label (void)
{
	GtkWidget *more_info_label;

	more_info_label = nautilus_label_new_with_background ("");
	nautilus_label_make_smaller (NAUTILUS_LABEL (more_info_label), 2);
	nautilus_label_set_justify (NAUTILUS_LABEL (more_info_label), GTK_JUSTIFY_CENTER);
	
	return more_info_label;
}

static void
smooth_graphics_mode_changed_callback (gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (callback_data));
	
	update_all (NAUTILUS_SIDEBAR_TITLE (callback_data));
}

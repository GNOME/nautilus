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

/* This is the sidebar title widget, which is the title part of the sidebar. */

#include <config.h>
#include <math.h>
#include "nautilus-sidebar-title.h"

#include "nautilus-window.h"

#include <eel/eel-background.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-pango-extensions.h>
#include <eel/eel-string.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-sidebar.h>
#include <string.h>
#include <stdlib.h>

/* maximum allowable size to be displayed as the title */
#define MAX_TITLE_SIZE 		256
#define MINIMUM_INFO_WIDTH	32
#define SIDEBAR_INFO_MARGIN	4
#define SHADOW_OFFSET		1

#define MORE_INFO_FONT_SIZE 	 12
#define MIN_TITLE_FONT_SIZE 	 12
#define TITLE_PADDING		  4

static void                nautilus_sidebar_title_class_init (NautilusSidebarTitleClass *klass);
static void                nautilus_sidebar_title_destroy          (GtkObject                 *object);
static void                nautilus_sidebar_title_init       (NautilusSidebarTitle      *pixmap);
static void                nautilus_sidebar_title_size_allocate    (GtkWidget                 *widget,
								    GtkAllocation             *allocation);
static void                update_icon                             (NautilusSidebarTitle      *sidebar_title);
static GtkWidget *         sidebar_title_create_title_label        (void);
static GtkWidget *         sidebar_title_create_more_info_label    (void);
static void		   update_all 				   (NautilusSidebarTitle      *sidebar_title);
static void		   update_more_info			   (NautilusSidebarTitle      *sidebar_title);
static void		   update_title_font			   (NautilusSidebarTitle      *sidebar_title);
static void                style_set                               (GtkWidget                 *widget,
								    GtkStyle                  *previous_style);
static guint		   get_best_icon_size 			   (NautilusSidebarTitle      *sidebar_title);

struct NautilusSidebarTitleDetails {
	NautilusFile		*file;
	guint			 file_changed_connection;
	gboolean                 monitoring_count;

	char			*title_text;
	GtkWidget		*icon;
	GtkWidget		*title_label;
	GtkWidget		*more_info_label;
	GtkWidget		*emblem_box;

	guint                    best_icon_size;

	gboolean		 determined_icon;
};

EEL_CLASS_BOILERPLATE (NautilusSidebarTitle, nautilus_sidebar_title, gtk_vbox_get_type ())

static void
nautilus_sidebar_title_class_init (NautilusSidebarTitleClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	
	object_class->destroy = nautilus_sidebar_title_destroy;
	widget_class->size_allocate = nautilus_sidebar_title_size_allocate;
	widget_class->style_set = style_set;

}

static void
style_set (GtkWidget *widget,
	   GtkStyle  *previous_style)
{
	NautilusSidebarTitle *sidebar_title;
	PangoFontDescription *font_desc;
	GtkStyle *style;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (widget));

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (widget);

	/* Update the dynamically-sized title font */
	update_title_font (sidebar_title);

	/* Update the fixed-size "more info" font */
	style = gtk_widget_get_style (widget);
	font_desc = pango_font_description_copy (style->font_desc);
	if (pango_font_description_get_size (font_desc) < MORE_INFO_FONT_SIZE * PANGO_SCALE) {
		pango_font_description_set_size (font_desc, MORE_INFO_FONT_SIZE * PANGO_SCALE);
	}
	
	gtk_widget_modify_font (sidebar_title->details->more_info_label,
				font_desc);
	pango_font_description_free (font_desc);
}

static void
nautilus_sidebar_title_init (NautilusSidebarTitle *sidebar_title)
{
	sidebar_title->details = g_new0 (NautilusSidebarTitleDetails, 1);
	
	/* Create the icon */
	sidebar_title->details->icon = gtk_image_new ();
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

	sidebar_title->details->best_icon_size = get_best_icon_size (sidebar_title);
	/* Keep track of changes in graphics trade offs */
	update_all (sidebar_title);

	/* initialize the label colors & fonts */
	style_set (GTK_WIDGET (sidebar_title), NULL);

	eel_preferences_add_callback_while_alive (
		NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
		(EelPreferencesCallback) update_more_info,
		sidebar_title, G_OBJECT (sidebar_title));
}

/* destroy by throwing away private storage */
static void
release_file (NautilusSidebarTitle *sidebar_title)
{
	if (sidebar_title->details->file_changed_connection != 0) {
		g_signal_handler_disconnect (sidebar_title->details->file,
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

	if (sidebar_title->details) {
		release_file (sidebar_title);

		g_free (sidebar_title->details->title_text);
		g_free (sidebar_title->details);
		sidebar_title->details = NULL;
	}

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* return a new index title object */
GtkWidget *
nautilus_sidebar_title_new (void)
{
	return gtk_widget_new (nautilus_sidebar_title_get_type (), NULL);
}

void
nautilus_sidebar_title_select_text_color (NautilusSidebarTitle *sidebar_title,
					  EelBackground        *background,
					  gboolean              is_default)
{
	char *sidebar_title_color;
	char *sidebar_info_title_color;
	char *sidebar_title_shadow_color;

	g_return_if_fail (background != NULL);
	
	/* if the background is set to the default, the theme can explicitly
	 * define the title colors.  Check if the background has been customized
	 * and if the theme specified any colors
	 */
	sidebar_title_color = NULL;
	sidebar_info_title_color = NULL;
	sidebar_title_shadow_color = NULL;
	
	/* FIXME bugzilla.gnome.org 42496: for now, both the title and info
	 * colors are the same - and hard coded */
	if (eel_background_is_dark (background)) {
		sidebar_title_color = g_strdup ("#FFFFFF");
		sidebar_info_title_color = g_strdup ("#FFFFFF");
		sidebar_title_shadow_color = g_strdup ("#000000");
	} else {
		sidebar_title_color = g_strdup ("#000000");
		sidebar_info_title_color = g_strdup ("#000000");
		sidebar_title_shadow_color = g_strdup ("#FFFFFF");
	}

	eel_gtk_widget_set_foreground_color (sidebar_title->details->title_label,
					     sidebar_title_color);
	eel_gtk_widget_set_foreground_color (sidebar_title->details->more_info_label,
					     sidebar_info_title_color);

	eel_gtk_label_set_drop_shadow_color (GTK_LABEL (sidebar_title->details->title_label),
					     eel_parse_rgb_with_white_default (sidebar_title_shadow_color));
	eel_gtk_label_set_drop_shadow_color (GTK_LABEL (sidebar_title->details->more_info_label),
					     eel_parse_rgb_with_white_default (sidebar_title_shadow_color));

	eel_gtk_label_set_drop_shadow_offset (GTK_LABEL (sidebar_title->details->title_label), 
					      SHADOW_OFFSET);
	eel_gtk_label_set_drop_shadow_offset (GTK_LABEL (sidebar_title->details->more_info_label),
					      SHADOW_OFFSET);
		
	g_free (sidebar_title_color);	
	g_free (sidebar_info_title_color);	
	g_free (sidebar_title_shadow_color);
}

static char*
get_property_from_component (NautilusSidebarTitle *sidebar_title, const char *property)
{
	/* There used to be a way to get icon and summary_text from main view,
	 *  but its not used right now, so this sas stubbed out for now
	 */
	return NULL;
}

static guint
get_best_icon_size (NautilusSidebarTitle *sidebar_title)
{
	gint width;

	width = GTK_WIDGET (sidebar_title)->allocation.width - TITLE_PADDING;

	if (width < 0) {
		/* use smallest available icon size */
		return nautilus_icon_get_smaller_icon_size (0);
	} else {
		return nautilus_icon_get_smaller_icon_size ((guint) width);
	}
}

/* set up the icon image */
static void
update_icon (NautilusSidebarTitle *sidebar_title)
{
	GdkPixbuf *pixbuf;
	NautilusIconInfo *info;
	char *icon_name;
	gboolean leave_pixbuf_unchanged;
	
	leave_pixbuf_unchanged = FALSE;

	/* see if the current content view is specifying an icon */
	icon_name = get_property_from_component (sidebar_title, "icon_name");

	pixbuf = NULL;
	if (icon_name != NULL && icon_name[0] != '\0') {
		info = nautilus_icon_info_lookup_from_name (icon_name, NAUTILUS_ICON_SIZE_LARGE);
		pixbuf = nautilus_icon_info_get_pixbuf_at_size (info,  NAUTILUS_ICON_SIZE_LARGE);
		g_object_unref (info);
	} else if (sidebar_title->details->file != NULL &&
		   nautilus_file_check_if_ready (sidebar_title->details->file,
						 NAUTILUS_FILE_ATTRIBUTES_FOR_ICON)) {
		pixbuf = nautilus_file_get_icon_pixbuf (sidebar_title->details->file,
							sidebar_title->details->best_icon_size,
							FALSE,
							NAUTILUS_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT);
	} else if (sidebar_title->details->determined_icon) {
		/* We used to know the icon for this file, but now the file says it isn't
		 * ready. This means that some file info has been invalidated, which
		 * doesn't necessarily mean that the previously-determined icon is
		 * wrong (in fact, in practice it usually doesn't mean that). Keep showing
		 * the one we last determined for now.
		 */
		 leave_pixbuf_unchanged = TRUE;
	}
	
	g_free (icon_name);
	
	if (pixbuf != NULL) {
		sidebar_title->details->determined_icon = TRUE;
	}

	if (!leave_pixbuf_unchanged) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (sidebar_title->details->icon), pixbuf);
	}
}

static void
update_title_font (NautilusSidebarTitle *sidebar_title)
{
	int available_width;
	PangoFontDescription *title_font;
	int largest_fitting_font_size;
	int max_style_font_size;
	GtkStyle *style;

	/* Make sure theres work to do */
	if (eel_strlen (sidebar_title->details->title_text) < 1) {
		return;
	}

	available_width = GTK_WIDGET (sidebar_title)->allocation.width - TITLE_PADDING;

	/* No work to do */
	if (available_width <= 0) {
		return;
	}

	style = gtk_widget_get_style (GTK_WIDGET (sidebar_title));
	title_font = pango_font_description_copy (style->font_desc);

	max_style_font_size = pango_font_description_get_size (title_font) * 1.8 / PANGO_SCALE;
	if (max_style_font_size < MIN_TITLE_FONT_SIZE + 1) {
		max_style_font_size = MIN_TITLE_FONT_SIZE + 1;
	}

	largest_fitting_font_size = eel_pango_font_description_get_largest_fitting_font_size (
		title_font,
		gtk_widget_get_pango_context (sidebar_title->details->title_label),
		sidebar_title->details->title_text,
		available_width,
		MIN_TITLE_FONT_SIZE,
		max_style_font_size);
	pango_font_description_set_size (title_font, largest_fitting_font_size * PANGO_SCALE); 

	pango_font_description_set_weight (title_font, PANGO_WEIGHT_BOLD);
	
	gtk_widget_modify_font (sidebar_title->details->title_label,
				title_font);
	pango_font_description_free (title_font);
}

static void
update_title (NautilusSidebarTitle *sidebar_title)
{
	GtkLabel *label;
	const char *text;

	label = GTK_LABEL (sidebar_title->details->title_label);
	text = sidebar_title->details->title_text;

	if (eel_strcmp (text, gtk_label_get_text (label)) == 0) {
		return;
	}
	gtk_label_set_text (label, text);
	update_title_font (sidebar_title);
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

static int
measure_width_callback (const char *string, gpointer callback_data)
{
	PangoLayout *layout;
	int width;

	layout = PANGO_LAYOUT (callback_data);
	pango_layout_set_text (layout, string, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	return width;
}

static void
update_more_info (NautilusSidebarTitle *sidebar_title)
{
	NautilusFile *file;
	GString *info_string;
	char *type_string, *component_info;
	char *date_modified_str;
	int sidebar_width;
	PangoLayout *layout;
	
	file = sidebar_title->details->file;

	/* allow components to specify the info if they wish to */
	component_info = get_property_from_component (sidebar_title, "summary_info");
	if (component_info != NULL && strlen (component_info) > 0) {
		info_string = g_string_new (component_info);
		g_free (component_info);
	} else {
		info_string = g_string_new (NULL);

		type_string = NULL;
		if (file != NULL && nautilus_file_should_show_type (file)) {
			type_string = nautilus_file_get_string_attribute (file, "type");
		}

		if (type_string != NULL) {
			append_and_eat (info_string, NULL, type_string);
			append_and_eat (info_string, ", ",
					nautilus_file_get_string_attribute (file, "size"));
		} else {
			append_and_eat (info_string, NULL,
					nautilus_file_get_string_attribute (file, "size"));
		}
		
		sidebar_width = GTK_WIDGET (sidebar_title)->allocation.width - 2 * SIDEBAR_INFO_MARGIN;
		if (sidebar_width > MINIMUM_INFO_WIDTH) {
			layout = pango_layout_copy (gtk_label_get_layout (GTK_LABEL (sidebar_title->details->more_info_label)));
			pango_layout_set_width (layout, -1);
			date_modified_str = nautilus_file_fit_modified_date_as_string
				(file, sidebar_width, measure_width_callback, NULL, layout);
			g_object_unref (layout);
				append_and_eat (info_string, "\n", date_modified_str);
		}
	}
	gtk_label_set_text (GTK_LABEL (sidebar_title->details->more_info_label),
			    info_string->str);

	g_string_free (info_string, TRUE);
}

/* add a pixbuf to the emblem box */
static void
add_emblem (NautilusSidebarTitle *sidebar_title, GdkPixbuf *pixbuf)
{
	GtkWidget *image_widget;

	image_widget = gtk_image_new_from_pixbuf (pixbuf);
  	gtk_widget_show (image_widget);
	gtk_container_add (GTK_CONTAINER (sidebar_title->details->emblem_box), image_widget);	
}

static void
update_emblems (NautilusSidebarTitle *sidebar_title)
{
	GList *pixbufs, *p;
	GdkPixbuf *pixbuf;

	/* exit if we don't have the file yet */
	if (sidebar_title->details->file == NULL) {
		return;
	}
	
	/* First, deallocate any existing ones */
	gtk_container_foreach (GTK_CONTAINER (sidebar_title->details->emblem_box),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);

	/* fetch the emblem icons from metadata */
	pixbufs = nautilus_file_get_emblem_pixbufs (sidebar_title->details->file,
						    nautilus_icon_get_emblem_size_for_icon_size (NAUTILUS_ICON_SIZE_STANDARD),
						    FALSE,
						    NULL);

	/* loop through the list of emblems, installing them in the box */
	for (p = pixbufs; p != NULL; p = p->next) {
		pixbuf = p->data;
		add_emblem (sidebar_title, pixbuf);
		g_object_unref (pixbuf);
	}
	g_list_free (pixbufs);
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
	
	/* truncate the title to a reasonable size */
	if (new_text && strlen (new_text) > MAX_TITLE_SIZE) {
		sidebar_title->details->title_text = g_strndup (new_text, MAX_TITLE_SIZE);
	} else {
		sidebar_title->details->title_text = g_strdup (new_text);
	}
	/* Recompute the displayed text. */
	update_title (sidebar_title);
}

static gboolean
item_count_ready (NautilusSidebarTitle *sidebar_title)
{
	return sidebar_title->details->file != NULL
		&& nautilus_file_get_directory_item_count
		(sidebar_title->details->file, NULL, NULL) != 0;
}

static void
monitor_add (NautilusSidebarTitle *sidebar_title)
{
	NautilusFileAttributes attributes;

	/* Monitor the things needed to get the right icon. Don't
	 * monitor a directory's item count at first even though the
	 * "size" attribute is based on that, because the main view
	 * will get it for us in most cases, and in other cases it's
	 * OK to not show the size -- if we did monitor it, we'd be in
	 * a race with the main view and could cause it to have to
	 * load twice. Once we have a size, though, we want to monitor
	 * the size to guarantee it stays up to date.
	 */

	sidebar_title->details->monitoring_count = item_count_ready (sidebar_title);

	attributes = NAUTILUS_FILE_ATTRIBUTES_FOR_ICON | NAUTILUS_FILE_ATTRIBUTE_METADATA;
	if (sidebar_title->details->monitoring_count) {
		attributes |= NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;
	}

	nautilus_file_monitor_add (sidebar_title->details->file, sidebar_title, attributes);
}

static void
update_all (NautilusSidebarTitle *sidebar_title)
{
	update_icon (sidebar_title);
	
	update_title (sidebar_title);
	update_more_info (sidebar_title);
	
	update_emblems (sidebar_title);

	/* Redo monitor once the count is ready. */
	if (!sidebar_title->details->monitoring_count && item_count_ready (sidebar_title)) {
		nautilus_file_monitor_remove (sidebar_title->details->file, sidebar_title);
		monitor_add (sidebar_title);
	}
}

void
nautilus_sidebar_title_set_file (NautilusSidebarTitle *sidebar_title,
				 NautilusFile         *file,
				 const char           *initial_text)
{
	if (file != sidebar_title->details->file) {
		release_file (sidebar_title);
		sidebar_title->details->file = file;
		sidebar_title->details->determined_icon = FALSE;
		nautilus_file_ref (sidebar_title->details->file);
	
		/* attach file */
		if (file != NULL) {
			sidebar_title->details->file_changed_connection =
				g_signal_connect_object
					(sidebar_title->details->file, "changed",
					 G_CALLBACK (update_all), sidebar_title, G_CONNECT_SWAPPED);
			monitor_add (sidebar_title);
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
	NautilusSidebarTitle *sidebar_title;
	guint16 old_width;
	guint best_icon_size;

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (widget);

	old_width = widget->allocation.width;

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	if (old_width != widget->allocation.width) {
		best_icon_size = get_best_icon_size (sidebar_title);
		if (best_icon_size != sidebar_title->details->best_icon_size) {
			sidebar_title->details->best_icon_size = best_icon_size;
			update_icon (sidebar_title);
		}

		/* update the title font and info format as the size changes. */
		update_title_font (sidebar_title);
		update_more_info (sidebar_title);	
	}
}

gboolean
nautilus_sidebar_title_hit_test_icon (NautilusSidebarTitle *sidebar_title, int x, int y)
{
	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (sidebar_title), FALSE);

	return eel_point_in_widget (sidebar_title->details->icon, x, y);
}

static GtkWidget *
sidebar_title_create_title_label (void)
{ 
	GtkWidget *title_label;

	title_label = gtk_label_new ("");
	eel_gtk_label_make_bold (GTK_LABEL (title_label));
	gtk_label_set_line_wrap (GTK_LABEL (title_label), TRUE);
	gtk_label_set_justify (GTK_LABEL (title_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable (GTK_LABEL (title_label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (title_label), PANGO_ELLIPSIZE_END);

	return title_label;
}

static GtkWidget *
sidebar_title_create_more_info_label (void)
{
	GtkWidget *more_info_label;

	more_info_label = gtk_label_new ("");
	eel_gtk_label_set_scale (GTK_LABEL (more_info_label), PANGO_SCALE_SMALL);
	gtk_label_set_justify (GTK_LABEL (more_info_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable (GTK_LABEL (more_info_label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (more_info_label), PANGO_ELLIPSIZE_END);
	
	return more_info_label;
}

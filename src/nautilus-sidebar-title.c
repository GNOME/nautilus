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
#include "nautilus-sidebar-title.h"

#include "nautilus-sidebar.h"
#include "nautilus-window.h"

#include <ctype.h>
#include <bonobo/bonobo-property-bag-client.h>
#include <bonobo/bonobo-exception.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwidget.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <eel/eel-background.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-font-factory.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <eel/eel-image-with-background.h>
#include <eel/eel-label-with-background.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-search-uri.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-theme.h>
#include <math.h>
#include <string.h>

/* maximum allowable size to be displayed as the title */
#define MAX_TITLE_SIZE 		256
#define MINIMUM_INFO_WIDTH	32
#define SIDEBAR_INFO_MARGIN	4

#define MORE_INFO_FONT_SIZE 	 12
#define MIN_TITLE_FONT_SIZE 	 12
#define MAX_TITLE_FONT_SIZE 	 20
#define TITLE_PADDING		  4

static void                nautilus_sidebar_title_initialize_class (NautilusSidebarTitleClass *klass);
static void                nautilus_sidebar_title_destroy          (GtkObject                 *object);
static void                nautilus_sidebar_title_initialize       (NautilusSidebarTitle      *pixmap);
static void                nautilus_sidebar_title_size_allocate    (GtkWidget                 *widget,
								    GtkAllocation             *allocation);
static void                nautilus_sidebar_title_theme_changed    (gpointer                   user_data);
static void                update_icon                             (NautilusSidebarTitle      *sidebar_title);
static GtkWidget *         sidebar_title_create_title_label        (void);
static GtkWidget *         sidebar_title_create_more_info_label    (void);
static void		   update_all 				   (NautilusSidebarTitle      *sidebar_title);
static void		   update_title_font			   (NautilusSidebarTitle      *sidebar_title);
static EelBackground 	  *nautilus_sidebar_title_background       (NautilusSidebarTitle      *sidebar_title);

static const char *non_smooth_font_name;

struct NautilusSidebarTitleDetails {
	NautilusFile		*file;
	guint			 file_changed_connection;
	gboolean                 monitoring_count;

	char			*title_text;
	GtkWidget		*icon;
	GtkWidget		*title_label;
	GtkWidget		*more_info_label;
	GtkWidget		*emblem_box;
	GtkWidget		*notes;

	int			 shadow_offset;
	gboolean		 determined_icon;
};

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSidebarTitle, nautilus_sidebar_title, gtk_vbox_get_type ())

static void
nautilus_sidebar_title_initialize_class (NautilusSidebarTitleClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	
	object_class->destroy = nautilus_sidebar_title_destroy;
	widget_class->size_allocate = nautilus_sidebar_title_size_allocate;

	eel_preferences_add_auto_string (NAUTILUS_PREFERENCES_DEFAULT_FONT,
					 &non_smooth_font_name);
}

static void
appearance_changed_callback (EelBackground *background, NautilusSidebarTitle *sidebar_title)
{
	nautilus_sidebar_title_select_text_color (sidebar_title);
}		

static void
realize_callback (NautilusSidebarTitle *sidebar_title)
{
	EelBackground *background;
	
	background = nautilus_sidebar_title_background (sidebar_title);

	g_return_if_fail (background != NULL);

	gtk_signal_connect_while_alive (GTK_OBJECT (background),
					"appearance_changed",
					appearance_changed_callback,
					sidebar_title,
					GTK_OBJECT (sidebar_title));
}

static void
smooth_font_changed_callback (gpointer callback_data)
{
	EelScalableFont *new_font;
	EelScalableFont *new_bold_font;
	NautilusSidebarTitle *sidebar_title;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (callback_data));

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (callback_data);
	
	new_font = nautilus_global_preferences_get_default_smooth_font ();
	new_bold_font = nautilus_global_preferences_get_default_smooth_bold_font ();

	eel_label_set_smooth_font (EEL_LABEL (sidebar_title->details->title_label), new_bold_font);
	eel_label_set_smooth_font (EEL_LABEL (sidebar_title->details->more_info_label), new_font);

	gtk_object_unref (GTK_OBJECT (new_font));
	gtk_object_unref (GTK_OBJECT (new_bold_font));
}

static GdkFont *
get_non_smooth_font (int font_size)
{
	GdkFont *result;

	result = nautilus_font_factory_get_font_by_family (non_smooth_font_name, font_size);
	g_assert (result != NULL);

	return result;
}

static void
non_smooth_font_changed_callback (gpointer callback_data)
{
	NautilusSidebarTitle *sidebar_title;
	GdkFont *new_font;

	g_return_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (callback_data));

	sidebar_title = NAUTILUS_SIDEBAR_TITLE (callback_data);

	/* Update the dynamically-sized title font */
	update_title_font (sidebar_title);

	/* Update the fixed-size "more info" font */
	new_font = get_non_smooth_font (MORE_INFO_FONT_SIZE);
	eel_gtk_widget_set_font (sidebar_title->details->more_info_label,
				 new_font);	
	gdk_font_unref (new_font);
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
	sidebar_title->details->icon = eel_image_new_with_background (NULL);
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

	/* FIXME: This should use EelLabel like the other displayed text.
	 * But I don't think this feature is ever used? Someone should consult
	 * with Andy about this. (This is not the same as the info in the Notes
	 * sidebar panel.)
	 */
	sidebar_title->details->notes = GTK_WIDGET (gtk_label_new (NULL));
	gtk_label_set_line_wrap (GTK_LABEL (sidebar_title->details->notes), TRUE);
	gtk_widget_show (sidebar_title->details->notes);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->notes, 0, 0, 0);

	/* Keep track of changes in graphics trade offs */
	update_all (sidebar_title);
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DEFAULT_FONT,
						       non_smooth_font_changed_callback,
						       sidebar_title,
						       GTK_OBJECT (sidebar_title));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
						       smooth_font_changed_callback,
						       sidebar_title,
						       GTK_OBJECT (sidebar_title));
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_THEME,
						       nautilus_sidebar_title_theme_changed,
						       sidebar_title,
						       GTK_OBJECT (sidebar_title));

	/* initialize the label colors & fonts */
	nautilus_sidebar_title_theme_changed (sidebar_title);
	smooth_font_changed_callback (sidebar_title);
	non_smooth_font_changed_callback (sidebar_title);
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

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* return a new index title object */
GtkWidget *
nautilus_sidebar_title_new (void)
{
	return gtk_widget_new (nautilus_sidebar_title_get_type (), NULL);
}

static EelBackground *
nautilus_sidebar_title_background (NautilusSidebarTitle *sidebar_title)
{
	GtkWidget *widget;
	GtkWidget *sidebar;
	EelBackground *background;

	g_return_val_if_fail (NAUTILUS_IS_SIDEBAR_TITLE (sidebar_title), NULL);
	
	widget = GTK_WIDGET (sidebar_title)->parent;

	 
	if (widget != NULL) {
		sidebar = widget->parent;
		g_return_val_if_fail (NAUTILUS_IS_SIDEBAR (sidebar), NULL);
		background = eel_get_widget_background (sidebar);
		g_return_val_if_fail (EEL_IS_BACKGROUND (background), NULL);
		return background;
	} else {
		/* FIXME bugzilla.gnome.org 45042
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
	EelBackground *background;
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
			sidebar_title_color = nautilus_theme_get_theme_data ("sidebar", "title_color");
			sidebar_info_title_color = nautilus_theme_get_theme_data ("sidebar", "title_info_color");
			sidebar_title_shadow_color = nautilus_theme_get_theme_data ("sidebar", "title_shadow_color"); 
		}
		
		if (sidebar_title_color == NULL) {
			/* FIXME bugzilla.gnome.org 42496: for now, both the title and info colors are the same */
			if (eel_background_is_dark (background)) {
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

		eel_label_set_text_color (EEL_LABEL (sidebar_title->details->title_label),
						       eel_parse_rgb_with_white_default (sidebar_title_color));
		
		eel_label_set_smooth_drop_shadow_color (EEL_LABEL (sidebar_title->details->title_label),
							      eel_parse_rgb_with_white_default (sidebar_title_shadow_color));
		
		eel_label_set_smooth_drop_shadow_offset (EEL_LABEL (sidebar_title->details->title_label),
							       sidebar_title->details->shadow_offset);
		
		eel_label_set_text_color (EEL_LABEL (sidebar_title->details->more_info_label),
						       eel_parse_rgb_with_white_default (sidebar_info_title_color));
		
		eel_label_set_smooth_drop_shadow_color (EEL_LABEL (sidebar_title->details->more_info_label),
							      eel_parse_rgb_with_white_default (sidebar_title_shadow_color));
		
		eel_label_set_smooth_drop_shadow_offset (EEL_LABEL (sidebar_title->details->more_info_label),
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
	
	shadow_offset_str = nautilus_theme_get_theme_data ("sidebar", "shadow_offset");
	if (shadow_offset_str) {
		sidebar_title->details->shadow_offset = atoi (shadow_offset_str);	
		g_free (shadow_offset_str);
	} else {
		sidebar_title->details->shadow_offset = 1;	
	}
	
	nautilus_sidebar_title_select_text_color (sidebar_title);
}

/* get a property from the current content view's property bag if we can */
static char*
get_property_from_component (NautilusSidebarTitle *sidebar_title, const char *property)
{
	GtkWidget *window;
	Bonobo_Control control;
	CORBA_Environment ev;
	Bonobo_PropertyBag property_bag;
	char* icon_name;
		
	window = gtk_widget_get_ancestor (GTK_WIDGET (sidebar_title), NAUTILUS_TYPE_WINDOW);

	if (window == NULL || NAUTILUS_WINDOW (window)->content_view == NULL) {
		return NULL;
	}

	
	control = nautilus_view_frame_get_control (NAUTILUS_VIEW_FRAME (NAUTILUS_WINDOW (window)->content_view));	
	if (control == NULL) {
		return NULL;	
	}

	CORBA_exception_init (&ev);
	property_bag = Bonobo_Control_getProperties (control, &ev);
	if (BONOBO_EX (&ev)) {
		property_bag = CORBA_OBJECT_NIL;
	}
	CORBA_exception_free (&ev);

	if (property_bag == CORBA_OBJECT_NIL) {
		return NULL;
	}	
	
	icon_name = bonobo_property_bag_client_get_value_string
		(property_bag, property, NULL);
	bonobo_object_release_unref (property_bag, NULL);

	return icon_name;
}

/* set up the icon image */
static void
update_icon (NautilusSidebarTitle *sidebar_title)
{
	GdkPixbuf *pixbuf;
	char *uri;
	char *icon_name;
	gboolean leave_pixbuf_unchanged;
	
	leave_pixbuf_unchanged = FALSE;
	uri = NULL;
	icon_name = NULL;
	if (sidebar_title->details->file) {
		uri = nautilus_file_get_uri (sidebar_title->details->file);
	}	

	/* see if the current content view is specifying an icon */
	icon_name = get_property_from_component (sidebar_title, "icon_name");

	pixbuf = NULL;
	if (icon_name != NULL && strlen (icon_name) > 0) {
		pixbuf = nautilus_icon_factory_get_pixbuf_from_name (icon_name, NULL, NAUTILUS_ICON_SIZE_LARGE, TRUE);
	} else if (nautilus_icon_factory_is_icon_ready_for_file (sidebar_title->details->file)) {
		pixbuf = nautilus_icon_factory_get_pixbuf_for_file (sidebar_title->details->file,
								    "accept",
								    NAUTILUS_ICON_SIZE_LARGE,
								    TRUE);
	} else if (sidebar_title->details->determined_icon) {
		/* We used to know the icon for this file, but now the file says it isn't
		 * ready. This means that some file info has been invalidated, which
		 * doesn't necessarily mean that the previously-determined icon is
		 * wrong (in fact, in practice it usually doesn't mean that). Keep showing
		 * the one we last determined for now.
		 */
		 leave_pixbuf_unchanged = TRUE;
	}
	
	g_free (uri);	
	g_free (icon_name);
	
	if (pixbuf != NULL) {
		sidebar_title->details->determined_icon = TRUE;
	}

	if (!leave_pixbuf_unchanged) {
		eel_image_set_pixbuf (EEL_IMAGE (sidebar_title->details->icon), pixbuf);
	}
}

static void
update_title_font (NautilusSidebarTitle *sidebar_title)
{
	int available_width;
	GdkFont *template_font;
	GdkFont *bold_template_font;
	GdkFont *largest_fitting_font;
	int largest_fitting_smooth_font_size;
	EelScalableFont *smooth_font;

	/* Make sure theres work to do */
	if (eel_strlen (sidebar_title->details->title_text) < 1) {
		return;
	}

	available_width = GTK_WIDGET (sidebar_title)->allocation.width - TITLE_PADDING;

	/* No work to do */
	if (available_width <= 0) {
		return;
	}
	
	/* Update the smooth font */
	smooth_font = eel_label_get_smooth_font (EEL_LABEL (sidebar_title->details->title_label));
	largest_fitting_smooth_font_size = eel_scalable_font_largest_fitting_font_size
		(smooth_font,
		 sidebar_title->details->title_text,
		 available_width,
		 MIN_TITLE_FONT_SIZE,
		 MAX_TITLE_FONT_SIZE);
	
	eel_label_set_smooth_font_size (EEL_LABEL (sidebar_title->details->title_label), 
					largest_fitting_smooth_font_size);
	
	gtk_object_unref (GTK_OBJECT (smooth_font));

	/* Update the regular font */
	template_font = get_non_smooth_font (MAX_TITLE_FONT_SIZE);
	bold_template_font = eel_gdk_font_get_bold (template_font);
	
	largest_fitting_font = eel_gdk_font_get_largest_fitting
		(bold_template_font,
		 sidebar_title->details->title_text,
		 available_width,
		 MIN_TITLE_FONT_SIZE,
		 MAX_TITLE_FONT_SIZE);
	
	if (largest_fitting_font == NULL) {
		largest_fitting_font = eel_gdk_font_get_fixed ();
	}
	
	eel_gtk_widget_set_font (sidebar_title->details->title_label,
				      largest_fitting_font);
	
	gdk_font_unref (largest_fitting_font);
	gdk_font_unref (bold_template_font);
	gdk_font_unref (template_font);
}

static void
update_title (NautilusSidebarTitle *sidebar_title)
{
	/* FIXME bugzilla.gnome.org 42500: We could defer showing the title until the icon is ready. */
	if (eel_label_set_text (EEL_LABEL (sidebar_title->details->title_label),
				     sidebar_title->details->title_text)) {
		update_title_font (sidebar_title);
	}
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

static int
measure_width_callback (const char *string, void *context)
{
	EelLabel *label;
	EelScalableFont *smooth_font;
	int smooth_font_size;
	
	label = (EelLabel*) context;
	smooth_font = eel_label_get_smooth_font (label); 
	smooth_font_size = eel_label_get_smooth_font_size (label);
	return eel_scalable_font_text_width (smooth_font, smooth_font_size, string, strlen (string));
}

static void
update_more_info (NautilusSidebarTitle *sidebar_title)
{
	NautilusFile *file;
	GString *info_string;
	char *type_string, *component_info;
	char *search_string, *search_uri;
	char *date_modified_str;
	int sidebar_width;
	
	file = sidebar_title->details->file;

	/* allow components to specify the info if they wish to */
	component_info = get_property_from_component (sidebar_title, "summary_info");
	if (component_info != NULL && strlen (component_info) > 0) {
		info_string = g_string_new (component_info);
		g_free (component_info);
	} else {
		/* FIXME bugzilla.gnome.org 42500: We could defer showing info until the icon is ready. */
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
		} else {
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
			
			sidebar_width = GTK_WIDGET (sidebar_title)->allocation.width - 2 * SIDEBAR_INFO_MARGIN;
			if (sidebar_width > MINIMUM_INFO_WIDTH) {
				date_modified_str = nautilus_file_fit_modified_date_as_string (file, sidebar_width,
											measure_width_callback, NULL,
											sidebar_title->details->more_info_label);
				append_and_eat (info_string, "\n", date_modified_str);
				g_string_append_c (info_string, '\0');
			}
		}
	}
	eel_label_set_text (EEL_LABEL (sidebar_title->details->more_info_label),
			    info_string->str);

	g_string_free (info_string, TRUE);
}

/* add a pixbuf to the emblem box */
static void
add_emblem (NautilusSidebarTitle *sidebar_title, GdkPixbuf *pixbuf)
{
	GtkWidget *image_widget;

	image_widget = eel_image_new_with_background (NULL);
	eel_image_set_pixbuf (EEL_IMAGE (image_widget), pixbuf);	
  	gtk_widget_show (image_widget);
	gtk_container_add (GTK_CONTAINER (sidebar_title->details->emblem_box), image_widget);	
}

static void
update_emblems (NautilusSidebarTitle *sidebar_title)
{
	GList *icons, *p;
	GdkPixbuf *pixbuf;

	/* FIXME bugzilla.gnome.org 42500: We could defer showing emblems until the icon is ready. */
	/* exit if we don't have the file yet */
	if (sidebar_title->details->file == NULL) {
		return;
	}
	
	/* First, deallocate any existing ones */
	gtk_container_foreach (GTK_CONTAINER (sidebar_title->details->emblem_box),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);

	/* fetch the emblem icons from metadata */
	icons = nautilus_icon_factory_get_emblem_icons_for_file (sidebar_title->details->file, NULL);

	/* loop through the list of emblems, installing them in the box */
	for (p = icons; p != NULL; p = p->next) {
		pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data,
			 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD,
			 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD,
			 FALSE, NULL, FALSE);
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
	
	/* FIXME bugzilla.gnome.org 42500: We could defer showing notes until the icon is ready. */

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
	GList *attributes;

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

	attributes = nautilus_icon_factory_get_required_file_attributes ();		
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_METADATA);
	if (sidebar_title->details->monitoring_count) {
		attributes = g_list_prepend (attributes,
					     NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
	}

	nautilus_file_monitor_add (sidebar_title->details->file, sidebar_title, attributes);

	g_list_free (attributes);
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

	/* Redo monitor once the count is ready. */
	if (!sidebar_title->details->monitoring_count && item_count_ready (sidebar_title)) {
		nautilus_file_monitor_remove (sidebar_title->details->file, sidebar_title);
		monitor_add (sidebar_title);
	}
}

void
nautilus_sidebar_title_set_file (NautilusSidebarTitle *sidebar_title,
				 NautilusFile *file,
				 const char *initial_text)
{
	if (file != sidebar_title->details->file) {
		release_file (sidebar_title);
		sidebar_title->details->file = file;
		sidebar_title->details->determined_icon = FALSE;
		nautilus_file_ref (sidebar_title->details->file);
	
		/* attach file */
		if (file != NULL) {
			sidebar_title->details->file_changed_connection =
				gtk_signal_connect_object (GTK_OBJECT (file),
							   "changed",
							   update_all,
							   GTK_OBJECT (sidebar_title));
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
	guint16 old_width;

	old_width = widget->allocation.width;

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	if (old_width != widget->allocation.width) {
		/* update the title font and info format as the size changes. */
		update_title_font (NAUTILUS_SIDEBAR_TITLE (widget));
		update_more_info (NAUTILUS_SIDEBAR_TITLE (widget));	
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

	title_label = eel_label_new_with_background ("");
	eel_label_make_bold (EEL_LABEL (title_label));
	eel_label_set_wrap (EEL_LABEL (title_label), TRUE);
	eel_label_set_justify (EEL_LABEL (title_label), GTK_JUSTIFY_CENTER);

	return title_label;
}

static GtkWidget *
sidebar_title_create_more_info_label (void)
{
	GtkWidget *more_info_label;

	more_info_label = eel_label_new_with_background ("");
	eel_label_make_smaller (EEL_LABEL (more_info_label), 2);
	eel_label_set_justify (EEL_LABEL (more_info_label), GTK_JUSTIFY_CENTER);
	eel_label_set_justify (EEL_LABEL (more_info_label), GTK_JUSTIFY_CENTER);
	
	return more_info_label;
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
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
 *
 * This is the index title widget, which is the title part of the index panel
 *
 */

#include <config.h>
#include "nautilus-sidebar-title.h"

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-font-factory.h>

static void     nautilus_sidebar_title_initialize_class   (NautilusSidebarTitleClass *klass);
static void     nautilus_sidebar_title_destroy            (GtkObject                 *object);
static void     nautilus_sidebar_title_initialize         (NautilusSidebarTitle      *pixmap);
static gboolean nautilus_sidebar_title_button_press_event (GtkWidget                 *widget,
							   GdkEventButton            *event);
static void     update_icon                               (NautilusSidebarTitle      *sidebar_title);

struct NautilusSidebarTitleDetails {
	NautilusFile *file;
	guint file_changed_connection;
	char *title_text;
	GtkWidget *icon;
	GtkWidget *title;
	GtkWidget *more_info;
	GtkWidget *emblem_box;
	GtkWidget *notes;
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
	widget_class->button_press_event = nautilus_sidebar_title_button_press_event;
}

static void
nautilus_sidebar_title_initialize (NautilusSidebarTitle *sidebar_title)
{ 
	GdkFont *font;

	sidebar_title->details = g_new0 (NautilusSidebarTitleDetails, 1);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_icon,
					       GTK_OBJECT (sidebar_title));

	sidebar_title->details->icon = GTK_WIDGET (nautilus_gtk_pixmap_new_empty ());
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->icon, 0, 0, 0);

	sidebar_title->details->title = GTK_WIDGET (gtk_label_new (NULL));
	gtk_label_set_line_wrap (GTK_LABEL (sidebar_title->details->title), TRUE);
	gtk_widget_show (sidebar_title->details->title);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->title, 0, 0, 0);

	sidebar_title->details->more_info = GTK_WIDGET (gtk_label_new (NULL));
        font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (sidebar_title->details->more_info, font);
        gdk_font_unref (font);
	gtk_widget_show (sidebar_title->details->more_info);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->more_info, 0, 0, 0);

	sidebar_title->details->emblem_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (sidebar_title->details->emblem_box);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->emblem_box, 0, 0, 0);

	sidebar_title->details->notes = GTK_WIDGET (gtk_label_new (NULL));
	gtk_label_set_line_wrap (GTK_LABEL (sidebar_title->details->notes), TRUE);
	font = nautilus_font_factory_get_font_from_preferences (12);
	nautilus_gtk_widget_set_font (sidebar_title->details->notes, font);
	gdk_font_unref (font);
	gtk_widget_show (sidebar_title->details->notes);
	gtk_box_pack_start (GTK_BOX (sidebar_title), sidebar_title->details->notes, 0, 0, 0);
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
  	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* return a new index title object */

GtkWidget *
nautilus_sidebar_title_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_sidebar_title_get_type ()));
}

/* set up the icon image */
static void
update_icon (NautilusSidebarTitle *sidebar_title)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	if (nautilus_icon_factory_is_icon_ready_for_file (sidebar_title->details->file)) {
		nautilus_icon_factory_get_pixmap_and_mask_for_file
			(sidebar_title->details->file,
			 NAUTILUS_ICON_SIZE_STANDARD,
			 &pixmap, &mask);
	} else {
		pixmap = NULL;
		mask = NULL;
	}

	gtk_pixmap_set (GTK_PIXMAP (sidebar_title->details->icon),
			pixmap, mask);
	if (pixmap == NULL) {
		gtk_widget_hide (sidebar_title->details->icon);
	} else {
		gtk_widget_show (sidebar_title->details->icon);
	}
}

/* set up the filename label */
static void
update_title (NautilusSidebarTitle *sidebar_title)
{
	GdkFont *label_font;
	const char *title_text;

	/* FIXME: We could defer showing the title until the icon is ready. */
	title_text = sidebar_title->details->title_text;

	gtk_label_set_text (GTK_LABEL (sidebar_title->details->title),
			    title_text);

	/* FIXME bugzilla.eazel.com 1103: Make this use the font
	 * factory.
	 */
	/* FIXME: Where does the "4" come from? */
	label_font = nautilus_get_largest_fitting_font
		(title_text,
		 GTK_WIDGET (sidebar_title)->allocation.width - 4,
		 "-*-helvetica-medium-r-normal-*-%d-*-*-*-*-*-*-*");
	nautilus_gtk_widget_set_font (sidebar_title->details->title, label_font);
	/* FIXME: Is there a font leak here? */
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

static void
update_more_info (NautilusSidebarTitle *sidebar_title)
{
	NautilusFile *file;
	GString *info_string;
	char *type_string;

	file = sidebar_title->details->file;
	
	/* FIXME: We could defer showing info until the icon is ready. */

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

	gtk_label_set_text (GTK_LABEL (sidebar_title->details->more_info), info_string->str);

	g_string_free (info_string, TRUE);
}

/* add a pixbuf to the emblem box */
static void
add_emblem (NautilusSidebarTitle *sidebar_title, GdkPixbuf *pixbuf)
{
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *pixmap_widget;
	
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
	pixmap_widget = GTK_WIDGET (gtk_pixmap_new (pixmap, mask));
	gtk_widget_show (pixmap_widget);
	gtk_container_add (GTK_CONTAINER (sidebar_title->details->emblem_box), pixmap_widget);	
}

static void
update_emblems (NautilusSidebarTitle *sidebar_title)
{
	GList *icons, *p;
	GdkPixbuf *pixbuf;

	/* FIXME: We could defer showing emblems until the icon is ready. */

	/* First, deallocate any existing ones */
	gtk_container_foreach (GTK_CONTAINER (sidebar_title->details->emblem_box),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);

	/* fetch the emblem icons from metadata */
	icons = nautilus_icon_factory_get_emblem_icons_for_file (sidebar_title->details->file);

	/* loop through the list of emblems, installing them in the box */
	for (p = icons; p != NULL; p = p->next) {
		pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
			(p->data,
			 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD,
			 NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_ICON_SIZE_STANDARD);
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
	
	/* FIXME: We could defer showing notes until the icon is ready. */

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
	update_emblems (sidebar_title);
	update_notes (sidebar_title);
}

void
nautilus_sidebar_title_set_uri (NautilusSidebarTitle *sidebar_title,
				const char *new_uri,
				const char *initial_text)
{
	GList *attributes;

	release_file (sidebar_title);

	if (new_uri == NULL) {
		sidebar_title->details->file = NULL;
	} else {
		sidebar_title->details->file = nautilus_file_get (new_uri);
	}

	/* attach file */
	if (sidebar_title->details->file != NULL) {
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
		nautilus_file_monitor_add (sidebar_title->details->file, sidebar_title,
					   attributes, TRUE);
		g_list_free (attributes);
	}

	g_free (sidebar_title->details->title_text);
	sidebar_title->details->title_text = g_strdup (initial_text);

	update_all (sidebar_title);
}

static gboolean
nautilus_sidebar_title_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	/* FIXME: Do we want to do something when clicked? If not,
	 * remove this.
	 */
	g_message ("button press");
	return TRUE;
}

gboolean
nautilus_sidebar_title_hit_test_icon (NautilusSidebarTitle *title, int x, int y)
{
	return nautilus_point_in_widget (title->details->icon, x, y);
}

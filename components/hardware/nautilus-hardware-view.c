/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 */


/* hardware view - presents various views of the hardware configuration */

#include <config.h>
#include "nautilus-hardware-view.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>

struct _NautilusHardwareViewDetails {
        char *uri;
        NautilusView *nautilus_view;
        
        GtkWidget *form;
};


enum {
	TARGET_URI_LIST,
	TARGET_COLOR,
        TARGET_GNOME_URI_LIST
};

static GtkTargetEntry hardware_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

static void nautilus_hardware_view_drag_data_received (GtkWidget                 *widget,
                                                       GdkDragContext            *context,
                                                       int                        x,
                                                       int                        y,
                                                       GtkSelectionData          *selection_data,
                                                       guint                      info,
                                                       guint                      time);
static void nautilus_hardware_view_initialize_class   (NautilusHardwareViewClass *klass);
static void nautilus_hardware_view_initialize         (NautilusHardwareView      *view);
static void nautilus_hardware_view_destroy            (GtkObject                 *object);
static void hardware_view_load_location_callback      (NautilusView              *view,
                                                       const char                *location,
                                                       NautilusHardwareView      *hardware_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusHardwareView, nautilus_hardware_view, GTK_TYPE_EVENT_BOX)

#define HARDWARE_DEFAULT_BACKGROUND_COLOR  "rgb:DDDD/DDDD/BBBB"

static void
nautilus_hardware_view_initialize_class (NautilusHardwareViewClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = nautilus_hardware_view_destroy;
	widget_class->drag_data_received  = nautilus_hardware_view_drag_data_received;
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_hardware_view_initialize (NautilusHardwareView *hardware_view)
{
  	NautilusBackground *background;
	hardware_view->details = g_new0 (NautilusHardwareViewDetails, 1);

	hardware_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (hardware_view));

	gtk_signal_connect (GTK_OBJECT (hardware_view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (hardware_view_load_location_callback), 
			    hardware_view);

	hardware_view->details->form = NULL;

  	background = nautilus_get_widget_background (GTK_WIDGET (hardware_view));
  	nautilus_background_set_color (background, HARDWARE_DEFAULT_BACKGROUND_COLOR);

	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (hardware_view),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   hardware_dnd_target_table, NAUTILUS_N_ELEMENTS (hardware_dnd_target_table), GDK_ACTION_COPY);
  		
	gtk_widget_show_all (GTK_WIDGET (hardware_view));
}

static void
nautilus_hardware_view_destroy (GtkObject *object)
{
	NautilusHardwareView *hardware_view = NAUTILUS_HARDWARE_VIEW (object);

	g_free (hardware_view->details->uri);
	g_free (hardware_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Component embedding support */
NautilusView *
nautilus_hardware_view_get_nautilus_view (NautilusHardwareView *hardware_view)
{
	return hardware_view->details->nautilus_view;
}

static char* 
read_proc_info(const gchar* proc_filename)
{
	FILE *thisFile;
	char *result;
	char buffer[256];
	char* path_name = g_strdup_printf("/proc/%s", proc_filename);	
	GString* string_data = g_string_new("");
	
	thisFile = fopen(  path_name, "r");
	
	while (fgets(buffer, 255, thisFile) != NULL) {
		g_string_append(string_data, buffer);		
	}
	fclose(thisFile);
	
	result = strdup(string_data->str);
	g_string_free(string_data, TRUE);
	g_free(path_name);

	return result;
}

/* utility routine to extract information from a string and add it to an XML node */

static char*
extract_info(gchar* data, const gchar *field_name, gint nth)
{
	int index;
	char **info_array;
	char *field_data = NULL;
	
	/* parse the data into a string array */
	info_array = g_strsplit(data, "\n", 32);
	/* iterate through the data isolating the field */
	for (index = 0; index < 32; index++) {
		if (info_array[index] == NULL)
			break;
		if (nautilus_str_has_prefix(info_array[index], field_name)) {
			if(nth>0) nth--;
			else {
				field_data = info_array[index] + strlen(field_name);
				field_data = strchr(field_data, ':') + 1;
				field_data =  g_strchug(field_data);
				break;
			}
		}
	}
	
	/* add the requested node if the field was found */
	if (field_data == NULL)
		return NULL;
	field_data = strdup(field_data);
	g_strfreev(info_array);
	return field_data;
}

/* get descriptive text about the CPU */

static char*
get_CPU_description(gint nth)
{
	char *temp_str, *result;
	GString* string_data = g_string_new("");
	char* proc_data = read_proc_info("cpuinfo");

	if(extract_info(proc_data, "processor", nth) == NULL) {
		/* can't find nth processor */
		g_free(proc_data);
		g_string_free(string_data, TRUE);
		return NULL;
	}
	
	temp_str = extract_info(proc_data, "model name", nth);
	g_string_append(string_data, temp_str);
	g_string_append(string_data, " CPU\n");
	g_free(temp_str);
	
	temp_str = extract_info(proc_data, "cpu MHz", nth);
	g_string_append(string_data, temp_str);
	g_string_append(string_data, " MHz\n");
	g_free(temp_str);
	
	temp_str = extract_info(proc_data, "cache size", nth);
	g_string_append(string_data, temp_str);
	g_string_append(string_data, " cache size\n");
	g_free(temp_str);
		
	g_free(proc_data);
	result = strdup(string_data->str);
	g_string_free(string_data, TRUE);

	return result;	
}

/* get descriptive information about main memory */
static char *
get_RAM_description (void)
{
	char *temp_str, *num_str, *result;
	GString* string_data = g_string_new("");
	char* proc_data = read_proc_info("meminfo");
	
	temp_str = extract_info(proc_data, "MemTotal", 0);
	/* strip kbyte suffix */
	temp_str[strlen(temp_str) - 3] = '\0';

        /* FIXME: Would 1024 give a better result? */
        num_str = gnome_vfs_format_file_size_for_display (1000 * atoi (temp_str));
	
	g_string_append(string_data, num_str);
	g_string_append(string_data, " RAM");
	g_free(num_str);
	g_free(temp_str);

	g_free(proc_data);
	result = strdup(string_data->str);
	g_string_free(string_data, TRUE);

	return result;
}

/* shared utility to allocate a title for a form */

static void
setup_form_title (NautilusHardwareView *view, const char* image_name, const char* title_text)
{
	GtkWidget *temp_widget;
	char *file_name;	
	GtkWidget *temp_container = gtk_hbox_new(FALSE, 0);
	GdkFont *font;

	gtk_box_pack_start (GTK_BOX(view->details->form), temp_container, 0, 0, 4);	
	gtk_widget_show(temp_container);
	
	if (image_name != NULL) {
 		file_name = gnome_pixmap_file (image_name);
		if (file_name != NULL) {
			temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
			gtk_box_pack_start(GTK_BOX(temp_container), temp_widget, 0, 0, 8);		
			gtk_widget_show(temp_widget);
			g_free (file_name);
		}
	}
	
 	temp_widget = gtk_label_new (title_text);

        font = nautilus_font_factory_get_font_from_preferences (18);

	nautilus_gtk_widget_set_font (temp_widget, font);
        gdk_font_unref (font);

	gtk_box_pack_start (GTK_BOX (temp_container), temp_widget, 0, 0, 8);			 	
	gtk_widget_show (temp_widget);
}

/* set up the widgetry for the overview page */

static void setup_overview_form(NautilusHardwareView *view)
{
	char  *file_name, *temp_text;
	GtkWidget *temp_widget, *temp_box;
	GtkWidget *container_box;
	int cpunum = 0;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* set up the title */	
	
	setup_form_title(view, NULL, "Hardware Overview");

	/* allocate a horizontal box */

	container_box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view->details->form), container_box, 0, 0, 2);	
	gtk_widget_show (GTK_WIDGET(container_box));

	while( (temp_text = get_CPU_description(cpunum)) != NULL ) {
		temp_box = gtk_vbox_new(FALSE, 4);
		gtk_box_pack_start (GTK_BOX (container_box), temp_box, 0, 0, 24);	
		gtk_widget_show (temp_box);

		file_name = nautilus_pixmap_file ("cpu.png");
                temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
		gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0);		
			gtk_widget_show(temp_widget);
			g_free (file_name);
		
		//temp_text = get_CPU_description(cpunum);
		temp_widget = gtk_label_new (temp_text);
		g_free(temp_text);
		gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0 );			
		gtk_widget_show (temp_widget);

		cpunum++;
	}

	/* set up the memory info */
	
	temp_box = gtk_vbox_new(FALSE, 4);
  	gtk_box_pack_start (GTK_BOX (container_box), temp_box, 0, 0, 24);	
	gtk_widget_show (temp_box);

 	file_name = nautilus_pixmap_file ("memory_chip.gif");
  	temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0);		
  	gtk_widget_show(temp_widget);
  	g_free (file_name);
	
	temp_text = get_RAM_description();
	temp_widget = gtk_label_new (temp_text);
	g_free(temp_text);
	gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0 );			
 	gtk_widget_show (temp_widget);
	
}

/* set up the widgetry for the CPU page */

static void setup_CPU_form(NautilusHardwareView *view)
{
	char *message;
	GtkWidget *temp_widget;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* set up the title */	
	setup_form_title(view, NULL, "CPU");
	
	message = "This is a placeholder for the CPU page.";
	temp_widget = gtk_label_new (message);
 	gtk_label_set_line_wrap(GTK_LABEL(temp_widget), TRUE);
	
	gtk_box_pack_start(GTK_BOX(view->details->form), temp_widget, 0, 0, 12);			
 	gtk_widget_show (temp_widget);
}

/* set up the widgetry for the RAM page */

static void setup_RAM_form(NautilusHardwareView *view)
{
	char *message;
	GtkWidget *temp_widget;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* set up the title */	
	setup_form_title(view, NULL, "RAM");
	
	message = "This is a placeholder for the RAM page.";
	temp_widget = gtk_label_new (message);
 	gtk_label_set_line_wrap(GTK_LABEL(temp_widget), TRUE);
	
	gtk_box_pack_start(GTK_BOX(view->details->form), temp_widget, 0, 0, 12);			
 	gtk_widget_show (temp_widget);
}

/* utility for checking uri */
static gboolean is_location(char *document_str, const char *place_str)
{
	return document_str && !strncmp(document_str + 1, place_str, strlen(place_str));
}

/* load the uri by casing out on the path */

void
nautilus_hardware_view_load_uri (NautilusHardwareView *view, const char *uri)
{
	char *document_name;
	
	/* dispose of the old uri and copy the new one */
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy(view->details->form);
		view->details->form = NULL;	
	}
		
	/* extract the document part of the uri */
	document_name = strchr(uri, ':');
	
	/* load the appropriate form, based on the uri and the registration state */
	if (is_location(document_name, "overview"))
		setup_overview_form(view);
	else if (is_location(document_name, "CPU"))
		setup_CPU_form(view);
	else if (is_location(document_name, "RAM"))
		setup_RAM_form(view);
	else
		setup_overview_form(view); /* if we don't understand it, go to the overview */
}

static void
hardware_view_load_location_callback (NautilusView *view, 
                                      const char *location, 
                                      NautilusHardwareView *hardware_view)
{
	nautilus_view_report_load_underway (hardware_view->details->nautilus_view);
	nautilus_hardware_view_load_uri (hardware_view, location);
	nautilus_view_report_load_complete (hardware_view->details->nautilus_view);
}

/* handle drag and drop */

static void  
nautilus_hardware_view_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data, guint info, guint time)
{
	g_return_if_fail (NAUTILUS_IS_HARDWARE_VIEW (widget));

	switch (info) {
        case TARGET_GNOME_URI_LIST:
        case TARGET_URI_LIST: 	
                g_message ("dropped data on hardware_view: %s", selection_data->data); 			
                break;
  		
                
        case TARGET_COLOR:
                /* Let the background change based on the dropped color. */
                nautilus_background_receive_dropped_color (nautilus_get_widget_background (widget),
                                                           widget, x, y, selection_data);
                break;
                
        default:
                g_warning ("unknown drop type");
                break;
        }
}

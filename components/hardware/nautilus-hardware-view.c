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
 *          Shane Butler <shane_b@bigfoot.com>
 *
 */


/* hardware view - presents various views of the hardware configuration */

#include <config.h>
#include "nautilus-hardware-view.h"

#include <dirent.h>
#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gnome.h>
#include <gtk/gtksignal.h>
#include <libgnorba/gnorba.h>
#include <eel/eel-background.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image.h>
#include <eel/eel-label.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <eel/eel-string.h>
#include <libnautilus/libnautilus.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

struct _NautilusHardwareViewDetails {
        NautilusView *nautilus_view;
        GtkWidget *form;
};

enum {
	TARGET_COLOR,
};

static GtkTargetEntry hardware_dnd_target_table[] = {
	{ "application/x-color", 0, TARGET_COLOR },
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

EEL_DEFINE_CLASS_BOILERPLATE (NautilusHardwareView, nautilus_hardware_view, GTK_TYPE_EVENT_BOX)

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
  	EelBackground *background;
	hardware_view->details = g_new0 (NautilusHardwareViewDetails, 1);

	hardware_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (hardware_view));

	gtk_signal_connect (GTK_OBJECT (hardware_view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (hardware_view_load_location_callback), 
			    hardware_view);

	hardware_view->details->form = NULL;

  	background = eel_get_widget_background (GTK_WIDGET (hardware_view));
  	eel_background_set_color (background, HARDWARE_DEFAULT_BACKGROUND_COLOR);

	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (hardware_view),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   hardware_dnd_target_table, EEL_N_ELEMENTS (hardware_dnd_target_table), GDK_ACTION_COPY);
  		
	gtk_widget_show_all (GTK_WIDGET (hardware_view));
}

static void
nautilus_hardware_view_destroy (GtkObject *object)
{
	NautilusHardwareView *hardware_view;

        hardware_view = NAUTILUS_HARDWARE_VIEW (object);

	g_free (hardware_view->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* Component embedding support */
NautilusView *
nautilus_hardware_view_get_nautilus_view (NautilusHardwareView *hardware_view)
{
	return hardware_view->details->nautilus_view;
}

static char * 
read_proc_info (const char* proc_filename)
{
	FILE *thisFile;
	char *result;
	char buffer[256];
	char* path_name;
	GString* string_data = g_string_new("");
	
        path_name = g_strdup_printf ("/proc/%s", proc_filename);	
	thisFile = fopen (path_name, "r");
	g_free (path_name);
	while (fgets (buffer, 255, thisFile) != NULL) {
		g_string_append (string_data, buffer);		
	}
	fclose (thisFile);
	
	result = string_data->str;
	g_string_free(string_data, FALSE);

	return result;
}

/* utility routine to extract information from a string and add it to an XML node */

static char*
extract_info (char* data, const char *field_name, int nth)
{
	int index;
	char **info_array;
	char *field_data = NULL;
	
	/* parse the data into a string array */
	info_array = g_strsplit (data, "\n", 32);
	/* iterate through the data isolating the field */
	for (index = 0; index < 32; index++) {
		if (info_array[index] == NULL) {
			break;
                }
		if (eel_str_has_prefix(info_array[index], field_name)) {
			if (nth > 0) {
                                nth--;
			} else {
				field_data = info_array[index] + strlen(field_name);
				field_data = strchr(field_data, ':') + 1;
				field_data =  g_strchug(field_data);
				break;
			}
		}
	}
	
	/* add the requested node if the field was found */
	field_data = g_strdup (field_data);
	g_strfreev (info_array);
	return field_data;
}

/* get descriptive text about the CPU */

static char *
get_CPU_description (int nth)
{
	char *proc_data;
        char *model, *speed, *cache_size;
	char *result;

        proc_data = read_proc_info ("cpuinfo");
	model = extract_info (proc_data, "model name", nth);
	speed = extract_info (proc_data, "cpu MHz", nth);
	cache_size = extract_info (proc_data, "cache size", nth);
	g_free (proc_data);

        /* FIXME: The MHz value always comes in with a "." as the
         * radix character. We need to change that to the local one
         * ("," for many European countries).
         */

        /* FIXME bugzilla.eazel.com 5298: The KB string that comes
         * from the proc data is wrong -- "kB" is correct, and we use
         * "K" for file sizes as of this writing (although we use "MB"
         * and "GB").
         */
        
        if (model == NULL || speed == NULL || cache_size == NULL) {
                result = NULL;
        } else {
                result = g_strdup_printf (_("%s CPU\n"
                                            "%s MHz\n"
                                            "%s cache size"),
                                          model, speed, cache_size);
        }

        g_free (model);
        g_free (speed);
        g_free (cache_size);

	return result;	
}

/* get descriptive information about main memory */
static char *
get_RAM_description (void)
{
	char *temp_str, *result;
	char *proc_data;
        gulong ram_size;

        proc_data = read_proc_info ("meminfo");

	temp_str = extract_info (proc_data, "MemTotal", 0);
        if (temp_str == NULL || strlen (temp_str) < 3) {
                g_free (temp_str);
                return NULL;
        }

        /* strip kbyte suffix */
	temp_str[strlen(temp_str) - 3] = '\0';

	ram_size = (strtoul (temp_str, NULL, 10) + 500) / 1000;
	if (ram_size >= 1000) {
		result = g_strdup_printf (_("%lu GB RAM"), ram_size / 1000);
	} else {
		result = g_strdup_printf (_("%lu MB RAM"), ram_size);
	}

 	g_free (temp_str);

	g_free (proc_data);

	return result;
}

static char *
get_IDE_description (const char *device)
{
        char *temp_str, *num_str, *result;
        GString *string_data = g_string_new("");
        char *proc_file;
        gulong capacity;

        /* Read model information string */
        proc_file = g_strdup_printf ("%s/model", device);
        temp_str = read_proc_info (proc_file);
        temp_str[strlen (temp_str) - 1] = '\0';
        g_string_append (string_data, temp_str);
        g_free (temp_str);
        g_free (proc_file);
        
        /* Read media type */
        proc_file = g_strdup_printf ("%s/media", device);
        temp_str = read_proc_info (proc_file);
        g_free (proc_file);
        
        /* If a hard disk, get the size */
        if (strcmp (temp_str, "disk\n") == 0) {
                g_free (temp_str);

                proc_file = g_strdup_printf ("%s/capacity", device);
                temp_str = read_proc_info (proc_file);
                temp_str[strlen (temp_str) - 1] = '\0';

                 /* NOTE: this should be 
		  *  capacity = strtoul (...)
		  *  num_str = gnome_vfs_format_file_size_for_display (512 * numsectors);   
		  *               
		  *  (512 bytes per sector)
		  *
		  *  but with large disks we overflow an unsigned long, which is the
		  *  the type that gnome_vfs uses (Darin: Not true, gnome-vfs uses 64-bit integers).  
		  *
		  *  ALSO, in keeping with disk manufacturer convention, disk sizes
		  *  are quoted in powers of 10 (i.e., MB is 10^6, GB is 10^9).
		  *  (see http://www.maxtor.com/technology/Digi_vs_Deci.html
		  *  So as to not confuse the user too much, we will follow the
		  *  same convention.)
		  *
		  */
   
		capacity = (512 * (strtoul (temp_str, NULL, 10) / 1000)) / 1000;
		if (capacity >= 1000) {
			num_str = g_strdup_printf (_("%lu GB"), capacity / 1000);
		} else {
			num_str = g_strdup_printf (_("%lu MB"), capacity);
		}
             
		g_string_append (string_data, "\n");
                g_string_append (string_data, num_str);
                g_free (temp_str);
                g_free (proc_file);
        } else {
                g_free (temp_str);
        }
        
        result = string_data->str;
        g_string_free (string_data, FALSE);
        
        return result;
}


/* shared utility to allocate a title for a form */

static void
setup_form_title (NautilusHardwareView *view,
                  const char *image_name,
                  const char *title_text)
{
	GtkWidget *temp_widget;
	char *file_name;	
	GtkWidget *temp_container = gtk_hbox_new(FALSE, 0);

	gtk_box_pack_start (GTK_BOX(view->details->form), temp_container, 0, 0, 4);	
	gtk_widget_show(temp_container);
	
	if (image_name != NULL) {
 		file_name = gnome_pixmap_file (image_name);
		if (file_name != NULL) {
			temp_widget = eel_image_new (file_name);
			gtk_box_pack_start (GTK_BOX(temp_container), temp_widget, 0, 0, 8);		
			gtk_widget_show (temp_widget);
			g_free (file_name);
		}
	}
	
 	temp_widget = eel_label_new (title_text);
	eel_label_make_larger (EEL_LABEL (temp_widget), 10);

	gtk_box_pack_start (GTK_BOX (temp_container), temp_widget, 0, 0, 8);			 	
	gtk_widget_show (temp_widget);
}

/* utility to add an element to the hardware view */
static void
add_element_to_table (GtkWidget *table, GtkWidget *element, int element_index)
{
	int column_pos, row_pos;
	
	column_pos = element_index % 3;
	row_pos = element_index / 3;
  	
	gtk_table_attach (GTK_TABLE (table),element,
			  column_pos, column_pos + 1, row_pos ,row_pos + 1, 
			  GTK_FILL, GTK_FILL, 12, 12);
}

/* set up the widgetry for the overview page */
static void
setup_overview_form (NautilusHardwareView *view)
{
	char  *file_name, *temp_text;
	GtkWidget *temp_widget, *pixmap_widget, *temp_box;
	GtkWidget *table;
	int cpunum, element_index;
	DIR *directory;
        struct dirent* entry;
        char *device, *proc_file, *ide_media;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* set up the title */	
	setup_form_title (view, NULL, _("Hardware Overview"));

	/* allocate a table to hold the elements */
	table =  gtk_table_new (3, 3, FALSE);	
	gtk_box_pack_start (GTK_BOX (view->details->form), table, 0, 0, 2);	
	gtk_widget_show (GTK_WIDGET(table));
   	element_index = 0;
	
   	cpunum = 0;
	while( (temp_text = get_CPU_description(cpunum)) != NULL ) {
		temp_box = gtk_vbox_new(FALSE, 4);
		add_element_to_table (table, temp_box, element_index++);
		gtk_widget_show (temp_box);

		file_name = nautilus_pixmap_file ("cpu.png");
                temp_widget = eel_image_new (file_name);
		gtk_box_pack_start (GTK_BOX(temp_box), temp_widget, 0, 0, 0);		
		gtk_widget_show (temp_widget);
		g_free (file_name);
		
		temp_widget = eel_label_new (temp_text);
		eel_label_make_larger (EEL_LABEL (temp_widget), 2);
		g_free(temp_text);
		gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0 );			
		gtk_widget_show (temp_widget);

		cpunum++;
	}

	/* set up the memory info */
       	temp_box = gtk_vbox_new(FALSE, 4);
	add_element_to_table (table, temp_box, element_index++);
	gtk_widget_show (temp_box);

 	file_name = nautilus_pixmap_file ("memory_chip.gif");
  	temp_widget = eel_image_new (file_name);
	gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0);		
  	gtk_widget_show(temp_widget);
  	g_free (file_name);
	
	temp_text = get_RAM_description ();
	temp_widget = eel_label_new (temp_text);
	eel_label_make_larger (EEL_LABEL (temp_widget), 2);
	g_free (temp_text);
	gtk_box_pack_start (GTK_BOX(temp_box), temp_widget, 0, 0, 0 );			
 	gtk_widget_show (temp_widget);
	
        /* Set up ide devices : by Shane Butler <shane_b@bigfoot.com> */
        /* Open the ide devices directory */
        if((directory = opendir("/proc/ide/")) != NULL) {
                while((entry = readdir(directory)) != NULL) {
                        /* Scan though each entry for actual device dirs */
                        if(!strncmp(entry->d_name, "hd", 2)) {
                                temp_box = gtk_vbox_new(FALSE, 4);
				add_element_to_table (table, temp_box, element_index++);
                                gtk_widget_show(temp_box);
                                
                                device = g_strdup_printf("ide/%s", entry->d_name);
                                
                                proc_file = g_strdup_printf("%s/media", device);
                                ide_media = read_proc_info(proc_file);
                                g_free(proc_file);
                                
                                /* Set the icon depending on the type of device */
                                if(!strcmp(ide_media, "disk\n")) {
                                        file_name = nautilus_pixmap_file("i-harddisk.png");
                                } else if(!strcmp(ide_media, "cdrom\n")) {
                                        file_name = nautilus_pixmap_file("CD_drive.png");
                                } else {
                                        /* some other device ... still set an icon */
                                        file_name = nautilus_pixmap_file("i-harddisk.png");
                                }
                                
				pixmap_widget = eel_image_new (file_name);
				gtk_box_pack_start (GTK_BOX(temp_box), pixmap_widget, 0, 0, 0);
				gtk_widget_show(pixmap_widget);
				g_free(file_name);
				g_free(ide_media);
                                
				temp_text = get_IDE_description (device);
				temp_widget = eel_label_new (temp_text);
				eel_label_make_larger (EEL_LABEL (temp_widget), 2);
				eel_label_set_justify (EEL_LABEL (temp_widget), GTK_JUSTIFY_CENTER);

				g_free(temp_text);
                                gtk_box_pack_start(GTK_BOX(temp_box), temp_widget, 0, 0, 0);
                                gtk_widget_show(temp_widget);
   
                                g_free(device);
                        }
                }
                closedir(directory);
        }
}

#ifdef ENABLE_SUBVIEWS

/* set up the widgetry for the CPU page */

static void
setup_CPU_form (NautilusHardwareView *view)
{
	char *message;
	GtkWidget *temp_widget;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* set up the title */	
	setup_form_title (view, NULL, "CPU");
	
	message = _("This is a placeholder for the CPU page.");
	temp_widget = eel_label_new (message);
	eel_label_make_larger (EEL_LABEL (temp_widget), 2);
 	eel_label_set_wrap(EEL_LABEL(temp_widget), TRUE);
	
	gtk_box_pack_start(GTK_BOX(view->details->form), temp_widget, 0, 0, 12);			
 	gtk_widget_show (temp_widget);
}

/* set up the widgetry for the RAM page */

static void
setup_RAM_form (NautilusHardwareView *view)
{
	char *message;
	GtkWidget *temp_widget;
	
	/* allocate a vbox as the container */	
	view->details->form = gtk_vbox_new(FALSE,0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);	
	gtk_widget_show(view->details->form);

	/* set up the title */	
	setup_form_title (view, NULL, "RAM");
	
	message = _("This is a placeholder for the RAM page.");
	temp_widget = eel_label_new (message);
	eel_label_make_larger (EEL_LABEL (temp_widget), 2);
 	eel_label_set_wrap(EEL_LABEL(temp_widget), TRUE);
	
	gtk_box_pack_start(GTK_BOX(view->details->form), temp_widget, 0, 0, 12);			
 	gtk_widget_show (temp_widget);
}

/* set up the widgetry for the IDE page */

static void
setup_IDE_form (NautilusHardwareView *view)
{
        char *message;
        GtkWidget *temp_widget;
        
        /* allocate a vbox as the container */  
        view->details->form = gtk_vbox_new(FALSE,0);
        gtk_container_add (GTK_CONTAINER (view), view->details->form);  
        gtk_widget_show(view->details->form);
        
        /* set up the title */  
        setup_form_title (view, NULL, "IDE");
        
        message = _("This is a placeholder for the IDE page.");
        temp_widget = eel_label_new (message);
	eel_label_make_larger (EEL_LABEL (temp_widget), 2);
        eel_label_set_wrap(EEL_LABEL(temp_widget), TRUE);
        
        gtk_box_pack_start(GTK_BOX(view->details->form), temp_widget, 0, 0, 12);            
        gtk_widget_show (temp_widget);
}


/* utility for checking uri */
static gboolean
is_location (const char *document_str, const char *place_str)
{
	return document_str && strncmp(document_str + 1, place_str, strlen (place_str)) == 0;
}

#endif /* ENABLE_SUBVIEWS */

/* load the uri by casing out on the path */

void
nautilus_hardware_view_load_uri (NautilusHardwareView *view, const char *uri)
{
#ifdef ENABLE_SUBVIEWS
	const char *document_name;
#endif
	
	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;	
	}
		
#ifndef ENABLE_SUBVIEWS
        setup_overview_form (view);
#else
	/* extract the document part of the uri */
	document_name = strchr (uri, ':');
	
	/* load the appropriate form, based on the uri and the registration state */
	if (is_location (document_name, "overview")) {
		setup_overview_form (view);
	} else if (is_location (document_name, "CPU")) {
		setup_CPU_form (view);
	} else if (is_location (document_name, "RAM")) {
		setup_RAM_form (view);
        } else if (is_location (document_name, "IDE")) {
                setup_IDE_form (view);
	} else {
		setup_overview_form (view); /* if we don't understand it, go to the overview */
        }
#endif
}

static void
hardware_view_load_location_callback (NautilusView *view, 
                                      const char *location, 
                                      NautilusHardwareView *hardware_view)
{
	nautilus_view_report_load_underway (hardware_view->details->nautilus_view);
	nautilus_view_set_title (hardware_view->details->nautilus_view, _("Hardware"));	
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
        case TARGET_COLOR:
                /* Let the background change based on the dropped color. */
                eel_background_receive_dropped_color (eel_get_widget_background (widget),
                                                           widget, x, y, selection_data);
                break;
                
        default:
                g_warning ("unknown drop type");
                break;
        }
}

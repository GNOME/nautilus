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
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image.h>
#include <eel/eel-label.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <eel/eel-string.h>
#include <libnautilus/libnautilus.h>
#include <limits.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/types.h>

struct _NautilusHardwareViewDetails {
        NautilusView *nautilus_view;
	BonoboPropertyBag *property_bag;
	        
        GtkWidget *form;
	
	EelLabel  *uptime_label;
	int timer_task;
	
	int cpu_count;
	int mem_size;
};

/* drag and drop properties */
enum {
	TARGET_COLOR
};

/* property bag properties */
enum {
	ICON_NAME,
	COMPONENT_INFO
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
static char* make_summary_string		      (NautilusHardwareView	 *hardware_view);
static int  update_uptime_text			      (gpointer			 callback_data);

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

/* property bag property access routines */
static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
	char *hardware_summary;
	
	switch (arg_id) {
        	case ICON_NAME:
                	BONOBO_ARG_SET_STRING (arg, "computer");					
                	break;
        	
        	case COMPONENT_INFO:
                	hardware_summary = make_summary_string ((NautilusHardwareView*) callback_data);
                	BONOBO_ARG_SET_STRING (arg, hardware_summary);					
                	g_free (hardware_summary);
                	break;
	
        	default:
                	g_warning ("Unhandled arg %d", arg_id);
                	break;
	}
}

/* there are no settable properties, so complain if someone tries to set one */
static void
set_bonobo_properties (BonoboPropertyBag *bag,
			const BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
                g_warning ("Bad Property set on hardware view: property ID %d", arg_id);
}

/* initialize ourselves by connecting to the load_location signal and allocating our subviews */
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
 
 	/* allocate a property bag to specify the name of the icon for this component */
	hardware_view->details->property_bag = bonobo_property_bag_new (get_bonobo_properties,  set_bonobo_properties, hardware_view);
	bonobo_control_set_properties (nautilus_view_get_bonobo_control (hardware_view->details->nautilus_view), hardware_view->details->property_bag);
	bonobo_property_bag_add (hardware_view->details->property_bag, "icon_name", ICON_NAME, BONOBO_ARG_STRING, NULL,
				 _("name of icon for the hardware view"), 0);
	bonobo_property_bag_add (hardware_view->details->property_bag, "summary_info", COMPONENT_INFO, BONOBO_ARG_STRING, NULL,
				 _("summary of hardware info"), 0);

	/* add the timer task to update the uptime */
	hardware_view->details->timer_task = gtk_timeout_add (60000, update_uptime_text, hardware_view); 

	gtk_widget_show_all (GTK_WIDGET (hardware_view));
}

static void
nautilus_hardware_view_destroy (GtkObject *object)
{
	NautilusHardwareView *hardware_view;

        hardware_view = NAUTILUS_HARDWARE_VIEW (object);

	/* free the property bag */
	if (hardware_view->details->property_bag != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (hardware_view->details->property_bag));
	}
	
	/* remove the timer task */
	if (hardware_view->details->timer_task != 0) {
		gtk_timeout_remove (hardware_view->details->timer_task);
	}

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
        char *localized_speed;
	char *result;
	char *p;
 	struct lconv *l;

        proc_data = read_proc_info ("cpuinfo");
	model = extract_info (proc_data, "model name", nth);
	speed = extract_info (proc_data, "cpu MHz", nth);
	cache_size = extract_info (proc_data, "cache size", nth);
	g_free (proc_data);
        
  	/* Hack to localize the decimal_point */
        p = speed == NULL ? NULL : strrchr (speed, '.');
        if (p == NULL) {
                localized_speed = g_strdup (speed);
        } else {
                *p = '\0';
                l = localeconv ();
                localized_speed = g_strconcat (speed, l->decimal_point,
                                               p + 1, NULL);
        }
        
        /* Remove the " KB" part so that it can be localized */
        if (cache_size != NULL) {
                p = strchr (cache_size, ' ');
                if (p != NULL) {
                        *p = '\0';
                }   
        }  	
        
        if (model == NULL || speed == NULL || cache_size == NULL) {
                result = NULL;
        } else {
                result = g_strdup_printf (_("%s CPU\n"
                                            "%s MHz\n"
                                            "%s K cache size"),
                                          model, localized_speed, cache_size);
        }
        
        g_free (model);
        g_free (speed);
        g_free (localized_speed);
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

/* uptime the uptime label with the current uptime */
static int
update_uptime_text (gpointer callback_data)
{
	char *uptime_data, *uptime_text;
	double uptime_seconds;
	int uptime_days, uptime_hours, uptime_minutes;
	
	uptime_data = read_proc_info ("uptime");
	uptime_seconds = atof (uptime_data);
	
	uptime_days = uptime_seconds / 86400;
	uptime_hours = (uptime_seconds - (uptime_days * 86400)) / 3600;
	uptime_minutes = (uptime_seconds - (uptime_days * 86400) - (uptime_hours * 3600)) / 60;
	
	uptime_text = g_strdup_printf (_("Uptime is %d days, %d hours, %d minutes"), uptime_days, uptime_hours, uptime_minutes);
	eel_label_set_text (NAUTILUS_HARDWARE_VIEW (callback_data)->details->uptime_label, uptime_text);
	g_free (uptime_text);
	
	g_free (uptime_data);
	return TRUE;
}

/* set up the widgetry for the overview page */
static void
setup_overview_form (NautilusHardwareView *view)
{
	char  *file_name, *temp_text;
	GtkWidget *temp_widget, *pixmap_widget, *temp_box;
	GtkWidget *table;
	int element_index;
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
	
   	view->details->cpu_count = 0;
	while( (temp_text = get_CPU_description (view->details->cpu_count)) != NULL ) {
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

		view->details->cpu_count++;
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

	/* allocate the uptime label */
	view->details->uptime_label = EEL_LABEL (eel_label_new (""));
	eel_label_make_larger (view->details->uptime_label, 2);
	eel_label_set_justify (view->details->uptime_label, GTK_JUSTIFY_LEFT);

	gtk_box_pack_end (GTK_BOX (view->details->form), GTK_WIDGET (view->details->uptime_label), 0, 0, GNOME_PAD);
	update_uptime_text (view);
	gtk_widget_show(GTK_WIDGET (view->details->uptime_label));
	
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

/* Create a string summarizing the most important hardware attributes */
static char *
make_summary_string (NautilusHardwareView *hardware_view)
{
	if (hardware_view->details->cpu_count == 1) {
		return g_strdup ("1 CPU");
	} else {
		return g_strdup_printf ("%d CPUs", hardware_view->details->cpu_count);
	}
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

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
 
/* The RPM view component is used to provide an easy-to-use overview of a rpm package */

#include <config.h>
#include "nautilus-rpm-view.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <rpm/rpmlib.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus/nautilus-background.h>
#include <libnautilus/nautilus-file.h>
#include <libnautilus/nautilus-file-utilities.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-metadata.h>
#include <libnautilus/nautilus-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>

struct _NautilusRPMViewDetails {
        char *current_uri;
        NautilusContentViewFrame *view_frame;
        
        GtkWidget *package_image;
        GtkWidget *package_title;
        GtkWidget *package_release;
        GtkWidget *package_summary;
        GtkWidget *package_size;
        GtkWidget *package_idate;
        GtkWidget *package_license;
        GtkWidget *package_bdate;
        GtkWidget *package_distribution;
        GtkWidget *package_vendor;
        
        GtkWidget *package_description;    
        GtkVBox   *package_container;
	
	GtkWidget *package_file_list;
        int background_connection;
};


#define RPM_VIEW_DEFAULT_BACKGROUND_COLOR  "rgb:BBBB/BBBB/FFFF"

enum {
	TARGET_URI_LIST,
	TARGET_COLOR,
        TARGET_GNOME_URI_LIST
};

static GtkTargetEntry rpm_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "special/x-gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

/*
static void nautilus_rpm_view_background_changed     (NautilusRPMView        *rpm_view);
*/
  
static void nautilus_rpm_view_drag_data_received     (GtkWidget                *widget,
                                                        GdkDragContext           *context,
                                                        int                       x,
                                                        int                       y,
                                                        GtkSelectionData         *selection_data,
                                                        guint                     info,
                                                        guint                     time);
static void nautilus_rpm_view_initialize_class       (NautilusRPMViewClass   *klass);
static void nautilus_rpm_view_initialize             (NautilusRPMView        *view);
static void nautilus_rpm_view_destroy                (GtkObject                *object);
static void nautilus_rpm_view_realize                (GtkWidget                *widget);
static void setup_title_font                           (NautilusRPMView        *rpm_view);
static void rpm_view_notify_location_change_callback (NautilusContentViewFrame *view,
                                                        Nautilus_NavigationInfo  *navinfo,
                                                        NautilusRPMView        *rpm_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusRPMView, nautilus_rpm_view, GTK_TYPE_EVENT_BOX)

static void
nautilus_rpm_view_initialize_class (NautilusRPMViewClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = nautilus_rpm_view_destroy;
	widget_class->realize = nautilus_rpm_view_realize;	
	widget_class->drag_data_received  = nautilus_rpm_view_drag_data_received;
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_rpm_view_initialize (NautilusRPMView *rpm_view)
{
  	NautilusBackground *background;
	GtkWidget *temp_box, *temp_title_box, *temp_widget;
	GtkTable *table;
  	static gchar *list_headers[] = { "Package Contents" };
	
	rpm_view->details = g_new0 (NautilusRPMViewDetails, 1);

	rpm_view->details->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (rpm_view));

	gtk_signal_connect (GTK_OBJECT (rpm_view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (rpm_view_notify_location_change_callback), 
			    rpm_view);

	rpm_view->details->current_uri = NULL;
	rpm_view->details->background_connection = 0;

	/* set up the default background color */
  	background = nautilus_get_widget_background (GTK_WIDGET (rpm_view));
  	nautilus_background_set_color (background, RPM_VIEW_DEFAULT_BACKGROUND_COLOR);
	 
	/* allocate a vbox to contain all of the views */
	
	rpm_view->details->package_container = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (rpm_view), GTK_WIDGET (rpm_view->details->package_container));
	gtk_widget_show (GTK_WIDGET (rpm_view->details->package_container));
	
	/* allocate a box to hold the title */
	
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (rpm_view->details->package_container), temp_box);
	gtk_widget_show (temp_box);
	
	/* allocate the package icon widget */
	
	/* allocate a vbox to hold the title info */
	
	temp_title_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (temp_box), temp_title_box);
	gtk_widget_show (temp_title_box);
	
	/* allocate the name field */
	
	rpm_view->details->package_title = gtk_label_new ("Package Title");
	gtk_box_pack_start (GTK_BOX (temp_title_box), rpm_view->details->package_title, 0, 0, 0);	
	gtk_widget_show (rpm_view->details->package_title);
	
	/* allocate the release-version field */
	
	rpm_view->details->package_release = gtk_label_new ("1.0-1");
	gtk_box_pack_start (GTK_BOX (temp_title_box), rpm_view->details->package_release, 0, 0, 0);	
	gtk_widget_show (rpm_view->details->package_release);
	
	/* allocate the summary field */
	
	rpm_view->details->package_summary = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (temp_title_box), rpm_view->details->package_summary, 0, 0, 0);	
	gtk_widget_show (rpm_view->details->package_summary);
	
	/* allocate an hbox to hold the optional package logo and the song list */
	
	/*
	rpm_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), rpm_box, 0, 0, 2);	
	gtk_widget_show (rpm_box);
	*/
	/* allocate a placeholder widget for the package logo, but don't show it yet */
  	/*
  	file_name = gnome_pixmap_file ("nautilus/i-directory.png");
  	rpm_view->details->package_image = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start(GTK_BOX(rpm_box), rpm_view->details->package_image, 0, 0, 0);		
  	g_free (file_name);
	*/
	
	/* allocate a table to hold the fields of information */
	
	table = GTK_TABLE(gtk_table_new(4, 4, FALSE));

  	temp_widget = gtk_label_new(_("Size: "));
  	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
  	gtk_table_attach(table, temp_widget, 0,1, 1,2, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	rpm_view->details->package_size = gtk_label_new("<size>");
  	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_size), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_size, 1, 2, 1, 2, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_size);

  	temp_widget = gtk_label_new(_("Install Date: "));
  	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
  	gtk_table_attach(table, temp_widget, 2,3, 1,2, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	rpm_view->details->package_idate = gtk_label_new(_("<unknown>"));
  	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_idate), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_idate, 3,4, 1,2, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_idate);

	temp_widget = gtk_label_new(_("License: "));
	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
	gtk_table_attach(table, temp_widget, 0,1, 2,3, GTK_FILL, GTK_FILL, 0,0);
	gtk_widget_show(temp_widget);
	rpm_view->details->package_license = gtk_label_new("<unknown>");
	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_license), 0.0, 0.5);
	gtk_table_attach(table, rpm_view->details->package_license, 1,2, 2,3, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
	gtk_widget_show(rpm_view->details->package_license);

  	temp_widget = gtk_label_new(_("Build Date: "));
  	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
  	gtk_table_attach(table, temp_widget, 2,3, 2,3, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	rpm_view->details->package_bdate = gtk_label_new("<unknown>");
  	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_bdate), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_bdate, 3,4, 2,3, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_bdate);

  	temp_widget = gtk_label_new(_("Distribution: "));
  	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
  	gtk_table_attach(table, temp_widget, 0,1, 3,4, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	rpm_view->details->package_distribution = gtk_label_new("<unknown>");
  	gtk_label_set_line_wrap(GTK_LABEL(rpm_view->details->package_distribution), TRUE);
  	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_distribution), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_distribution, 1,2, 3,4, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_distribution);

  	temp_widget = gtk_label_new(_("Vendor: "));
  	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
  	gtk_table_attach(table, temp_widget, 2,3, 3,4, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	rpm_view->details->package_vendor = gtk_label_new("<unknown>");
  	gtk_label_set_line_wrap(GTK_LABEL(rpm_view->details->package_vendor), TRUE);
  	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_vendor), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_vendor, 3,4, 3,4, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_vendor);

	/* insert the data table */
	
	temp_widget = gtk_hseparator_new();	
	gtk_box_pack_start (GTK_BOX (temp_title_box),temp_widget, 0, 0, 2);	
	gtk_widget_show (temp_widget);
	
	gtk_box_pack_start (GTK_BOX (temp_title_box), GTK_WIDGET(table), 0, 0, 2);	
	gtk_widget_show (GTK_WIDGET(table));
	
	/* add the list of files contained in the package */

  	temp_widget = gtk_scrolled_window_new(NULL, NULL);
  	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(temp_widget),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), temp_widget, 0, 0, 8);	
  	gtk_widget_show(temp_widget);
 
  	rpm_view->details->package_file_list = gtk_clist_new_with_titles(1, list_headers);
  	gtk_widget_set_usize(rpm_view->details->package_file_list, -1, 104);
  	gtk_clist_column_titles_passive(GTK_CLIST(rpm_view->details->package_file_list));
  	gtk_container_add (GTK_CONTAINER (temp_widget), rpm_view->details->package_file_list);	
  	gtk_widget_show(rpm_view->details->package_file_list);
	
	/* add the description */
	rpm_view->details->package_description = gtk_label_new ("Description");
	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), rpm_view->details->package_description, 0, 0, 8);	
	gtk_widget_show (rpm_view->details->package_description);
	
	/* prepare ourselves to receive dropped objects */
	
	gtk_drag_dest_set (GTK_WIDGET (rpm_view),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   rpm_dnd_target_table, NAUTILUS_N_ELEMENTS (rpm_dnd_target_table), GDK_ACTION_COPY);

	/* finally, show the view itself */	
	gtk_widget_show (GTK_WIDGET (rpm_view));
}

static void
nautilus_rpm_view_destroy (GtkObject *object)
{
	NautilusRPMView *rpm_view = NAUTILUS_RPM_VIEW (object);

        bonobo_object_unref (BONOBO_OBJECT (rpm_view->details->view_frame));

	g_free (rpm_view->details->current_uri);
	g_free (rpm_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Component embedding support */
NautilusContentViewFrame *
nautilus_rpm_view_get_view_frame (NautilusRPMView *rpm_view)
{
	return rpm_view->details->view_frame;
}

/* utility routine to set up the font for the package title and summary, called after we're realized */
static void
setup_title_font(NautilusRPMView *rpm_view)
{
	GtkStyle *temp_style;

        temp_style = gtk_style_new();

	gtk_widget_realize (rpm_view->details->package_title);	
	temp_style->font = gdk_font_load ("-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*"); ;
	gtk_widget_set_style (rpm_view->details->package_title,
                              gtk_style_attach (temp_style, rpm_view->details->package_title->window));

}

/* set up fonts, colors, etc after we're realized */
void
nautilus_rpm_view_realize(GtkWidget *widget)
{
	NautilusRPMView *rpm_view;
	NautilusBackground *background;
 
 	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));
	
  	rpm_view = NAUTILUS_RPM_VIEW (widget);

	setup_title_font (rpm_view);
  
	background = nautilus_get_widget_background (widget);
	nautilus_background_set_color (background, RPM_VIEW_DEFAULT_BACKGROUND_COLOR);
}

/* utility to format time using std library routines */

#if 0

static char* format_time(time_t time_value)
{
	char *time_string = g_strdup(ctime(&time_value));
	time_string[strlen(time_string) - 1] = '\0';
	return time_string;
}

#endif

/* here's where we do most of the real work of populating the view with info from the package */
/* open the package and copy the information, and then set up the appropriate views with it */
/* FIXME: use gnome-vfs to open the package */

static void 
nautilus_rpm_view_update_from_uri (NautilusRPMView *rpm_view, const char *uri)
{
	/* open the package */
	HeaderIterator iterator;
	Header header_info, signature;
	char buffer[512];
	gint iterator_tag, type, data_size, result, index, file_count;
	gchar *data_ptr, *temp_str;
	gint file_descriptor;
	gint *integer_ptr;
  	
	gchar **path = NULL;
	gchar **links = NULL;	
	gchar *temp_version = NULL;
	gchar *temp_release = NULL;
	const char *path_name = uri + 7;
	
	file_descriptor = open(path_name, O_RDONLY, 0644);
	 
	if (file_descriptor >= 0) {
                
		/* read out the appropriate fields, and set them up in the view */
		result = rpmReadPackageInfo((FD_t)&file_descriptor, &signature, &header_info);
		if (result) {
			g_message("couldnt read package!");
			return;
		}
		
		iterator = headerInitIterator(header_info);
		while (headerNextIterator(iterator, &iterator_tag, &type, (void**)&data_ptr, &data_size)) {
			integer_ptr = (int*) data_ptr;
			switch (iterator_tag) {
                        case RPMTAG_NAME:
                                temp_str = g_strdup_printf("Package \"%s\" ", data_ptr);
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_title), temp_str);				 
                                g_free(temp_str);
                                break;
                        case RPMTAG_VERSION:
                                temp_version = g_strdup(data_ptr);
                                break;
                        case RPMTAG_RELEASE:
                                temp_release = g_strdup(data_ptr);
                                break;
                        case RPMTAG_SUMMARY:
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_summary), data_ptr+4);				 
                                break;
                        case RPMTAG_DESCRIPTION:
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_description), data_ptr+4);				 
                                break;
                        case RPMTAG_SIZE:
                                temp_str = gnome_vfs_file_size_to_string (*integer_ptr);
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_size), temp_str);				 
                                g_free(temp_str);					
                                break;
                        case RPMTAG_DISTRIBUTION:
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_distribution), data_ptr+4);
                                break;
                        case RPMTAG_GROUP:
                                break;
                        case RPMTAG_ICON:
                                break;
                        case RPMTAG_LICENSE:
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_license), data_ptr);
                                break;
                        case RPMTAG_BUILDTIME:
                                strftime(buffer, 511, "%a %b %d %I:%M:%S %Z %Y", gmtime((time_t *) data_ptr));
                                gtk_label_set(GTK_LABEL(rpm_view->details->package_bdate), buffer);
                                break;
                        case RPMTAG_INSTALLTIME:
                                strftime(buffer, 511, "%a %b %d %I:%M:%S %Z %Y", gmtime((time_t *) data_ptr));
                                gtk_label_set(GTK_LABEL(rpm_view->details->package_idate), buffer);
                                break;
                        case RPMTAG_VENDOR:
                                gtk_label_set (GTK_LABEL (rpm_view->details->package_vendor), data_ptr);
                                break;
                        case RPMTAG_GIF:
                                break;
                        case RPMTAG_XPM:
                                break;
			}
			
		}
		
		if (temp_version) {
			temp_str = g_strdup_printf("version %s-%s", temp_version, temp_release);
			gtk_label_set (GTK_LABEL (rpm_view->details->package_release), temp_str);				 
			g_free(temp_str);
			if (temp_version)
				g_free(temp_version);
			if (temp_release)
				g_free(temp_release);
		}
		
		headerFreeIterator(iterator);			
		/* close the package */
		close(file_descriptor);	
	}
	
	/* add the files in the package to the list */

  	gtk_clist_freeze(GTK_CLIST(rpm_view->details->package_file_list));
  	gtk_clist_clear(GTK_CLIST(rpm_view->details->package_file_list));
  	
	headerGetEntry(header_info, RPMTAG_FILENAMES, NULL, (void **)&path, &file_count);
  	headerGetEntry(header_info, RPMTAG_FILELINKTOS, NULL, (void **)&links, NULL);
	
  	for (index = 0; index < file_count; index++) {
  
    		if (*(links[index]) == '\0')
      			temp_str = path[index];
    		else {
      			g_snprintf(buffer, 511, "%s -> %s", path[index], links[index]);
      			temp_str = buffer;
    		}
    		gtk_clist_append(GTK_CLIST(rpm_view->details->package_file_list), &temp_str);
 	}
  	

	temp_str = g_strdup_printf("Package Contents: %d files", file_count);
	gtk_clist_set_column_title (GTK_CLIST(rpm_view->details->package_file_list), 0, temp_str);
	g_free(temp_str);
	
	g_free(path);
  	g_free(links);
  	gtk_clist_thaw(GTK_CLIST(rpm_view->details->package_file_list));
	
	/* determine if the package is installed or not */
	
	/* set up the appropriate buttons */

}

void
nautilus_rpm_view_load_uri (NautilusRPMView *rpm_view, const char *uri)
{
	g_free(rpm_view->details->current_uri);
	rpm_view->details->current_uri = g_strdup (uri);	
	nautilus_rpm_view_update_from_uri(rpm_view, uri);
}

static void
rpm_view_notify_location_change_callback (NautilusContentViewFrame *view, 
                                          Nautilus_NavigationInfo *navinfo, 
                                          NautilusRPMView *rpm_view)
{
	Nautilus_ProgressRequestInfo progress;
 
 	memset(&progress, 0, sizeof(progress));

	/* send required PROGRESS_UNDERWAY signal */
  
	progress.type = Nautilus_PROGRESS_UNDERWAY;
	progress.amount = 0.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (rpm_view->details->view_frame), &progress);

	/* do the actual work here */
	nautilus_rpm_view_load_uri (rpm_view, navinfo->actual_uri);

	/* send the required PROGRESS_DONE signal */
	progress.type = Nautilus_PROGRESS_DONE_OK;
	progress.amount = 100.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (rpm_view->details->view_frame), &progress);
}

/* handle drag and drop */


/*
static void
nautilus_rpm_view_background_changed (NautilusRPMView *rpm_view)
{
	NautilusBackground *background;
	NautilusDirectory *directory;
	char *color_spec;
		
	background = nautilus_get_widget_background (GTK_WIDGET (rpm_view));
	color_spec = nautilus_background_get_color (background);
	directory = nautilus_directory_get (rpm_view->details->current_uri);
	nautilus_directory_set_metadata (directory,
					 ICON_VIEW_BACKGROUND_COLOR_METADATA_KEY,
					 RPM_VIEW_DEFAULT_BACKGROUND_COLOR,
					 color_spec);
	g_free (color_spec);		
	nautilus_directory_unref (directory);
}
*/

static void  
nautilus_rpm_view_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data, guint info, guint time)
{
	g_return_if_fail (NAUTILUS_IS_RPM_VIEW (widget));

	switch (info) {
        case TARGET_GNOME_URI_LIST:
        case TARGET_URI_LIST: 	
                g_message ("dropped data on rpm_view: %s", selection_data->data); 			
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

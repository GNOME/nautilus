/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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
#include "nautilus-rpm-verify-window.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-ctree.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-theme.h>
#include <libnautilus-extensions/nautilus-viewport.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtklabel.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>

/* must be after any services includes */
#include "nautilus-rpm-view-private.h"


#define RPM_VIEW_DEFAULT_BACKGROUND_COLOR  "rgb:DDDD/DDDD/BBBB"

enum {
	TARGET_COLOR,
};

static GtkTargetEntry rpm_dnd_target_table[] = {
	{ "application/x-color", 0, TARGET_COLOR },
};

static void nautilus_rpm_view_drag_data_received (GtkWidget            *widget,
                                                  GdkDragContext       *context,
                                                  int                   x,
                                                  int                   y,
                                                  GtkSelectionData     *selection_data,
                                                  guint                 info,
                                                  guint                 time);
static void nautilus_rpm_view_initialize_class   (NautilusRPMViewClass *klass);
static void nautilus_rpm_view_initialize         (NautilusRPMView      *view);
static void nautilus_rpm_view_destroy            (GtkObject            *object);
static void rpm_view_load_location_callback      (NautilusView         *view,
                                                  const char           *location,
                                                  NautilusRPMView      *rpm_view);
static void nautilus_rpm_view_verify_package_callback (GtkWidget *widget,
				   		  NautilusRPMView *rpm_view);

static void file_activation_callback (NautilusCTree *ctree, NautilusCTreeNode *node, int column,
                                      NautilusRPMView *rpm_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusRPMView, nautilus_rpm_view, GTK_TYPE_EVENT_BOX)

static void
nautilus_rpm_view_initialize_class (NautilusRPMViewClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = nautilus_rpm_view_destroy;
	widget_class->drag_data_received  = nautilus_rpm_view_drag_data_received;
}

/* initialize the rpm view */

static void
nautilus_rpm_view_initialize (NautilusRPMView *rpm_view)
{
  	NautilusBackground *background;
	GtkWidget *temp_box, *temp_widget;
	GtkWidget *icon_title_box, *title_box;
	GtkTable *table;
  	char *default_icon_path;
        GtkWidget *viewport;
  	
  	static gchar *list_headers[] = { N_("Package Contents") };
	
	rpm_view->details = g_new0 (NautilusRPMViewDetails, 1);

	rpm_view->details->package_system = eazel_package_system_new (NULL);
        eazel_package_system_set_debug (rpm_view->details->package_system, EAZEL_PACKAGE_SYSTEM_DEBUG_FAIL);
        rpm_view->details->package = NULL;

	rpm_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (rpm_view));

	gtk_signal_connect (GTK_OBJECT (rpm_view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (rpm_view_load_location_callback), 
			    rpm_view);

	rpm_view->details->current_uri = NULL;
	rpm_view->details->background_connection = 0;
	rpm_view->details->selected_file = NULL;
	
	/* set up the default background color */
  	background = nautilus_get_widget_background (GTK_WIDGET (rpm_view));
  	nautilus_background_set_color (background, RPM_VIEW_DEFAULT_BACKGROUND_COLOR);
	 
	/* allocate a vbox to contain all of the views */
	
	rpm_view->details->package_container = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (rpm_view), GTK_WIDGET (rpm_view->details->package_container));
	gtk_widget_show (GTK_WIDGET (rpm_view->details->package_container));

	/* allocate an hbox to hold the package icon and the title info */
	icon_title_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (icon_title_box);
	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), icon_title_box, FALSE, FALSE, 0);	
	
	/* allocate a nautilus_image to hold the icon */
	default_icon_path = nautilus_theme_get_image_path ("gnome-pack-rpm.png");
	rpm_view->details->package_image = nautilus_image_new (default_icon_path);
	g_free (default_icon_path);
	gtk_widget_show (rpm_view->details->package_image);
	gtk_box_pack_start (GTK_BOX (icon_title_box), rpm_view->details->package_image, FALSE, FALSE, 8);	
	
	/* allocate another vbox to hold the titles */
	title_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (title_box);
	gtk_box_pack_start (GTK_BOX (icon_title_box), title_box, TRUE, TRUE, 8);	
	
	/* allocate the name field */
	rpm_view->details->package_title = gtk_label_new (_("Package Title"));
        nautilus_gtk_label_make_larger (GTK_LABEL (rpm_view->details->package_title), 4);
	gtk_box_pack_start (GTK_BOX (title_box), rpm_view->details->package_title, FALSE, FALSE, 1);	
	gtk_widget_show (rpm_view->details->package_title);
		
	/* allocate the release-version field */	
	rpm_view->details->package_release = gtk_label_new ("1.0-1");
	gtk_box_pack_start (GTK_BOX (title_box), rpm_view->details->package_release, FALSE, FALSE, 1);	

	gtk_widget_show (rpm_view->details->package_release);
	
	/* allocate the summary field */	
	rpm_view->details->package_summary = gtk_label_new ("");	
	gtk_box_pack_start (GTK_BOX (title_box), rpm_view->details->package_summary, FALSE, FALSE, 2);		

	gtk_widget_show (rpm_view->details->package_summary);
		
	/* allocate a table to hold the fields of information */
	table = GTK_TABLE(gtk_table_new(4, 4, FALSE));
	gtk_widget_set_usize (GTK_WIDGET (table), 420, -1);
	
  	temp_widget = gtk_label_new(_("Installed Size: "));
	gtk_label_set_justify (GTK_LABEL (temp_widget), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
	gtk_table_attach(table, temp_widget, 0,1, 1,2, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	
	rpm_view->details->package_size = gtk_label_new(_("<size>"));
	gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_size), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_size), 0.0, 0.5);
	gtk_table_attach(table, rpm_view->details->package_size, 1, 2, 1, 2, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_size);

  	temp_widget = gtk_label_new(_("Install Date: "));
 	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (temp_widget), GTK_JUSTIFY_RIGHT);
 	gtk_table_attach(table, temp_widget, 2,3, 1,2, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	
	rpm_view->details->package_idate = gtk_label_new(_("<unknown>"));
 	gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_idate), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_idate), 0.0, 0.5);
	gtk_table_attach(table, rpm_view->details->package_idate, 3,4, 1,2, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_idate);

	temp_widget = gtk_label_new(_("License: "));
	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (temp_widget), GTK_JUSTIFY_RIGHT);
	gtk_table_attach(table, temp_widget, 0,1, 2,3, GTK_FILL, GTK_FILL, 0,0);
	gtk_widget_show(temp_widget);
	
	rpm_view->details->package_license = gtk_label_new(_("<unknown>"));
	gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_license), GTK_JUSTIFY_LEFT);	
	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_license), 0.0, 0.5);
	gtk_table_attach(table, rpm_view->details->package_license, 1,2, 2,3, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
	gtk_widget_show(rpm_view->details->package_license);

  	temp_widget = gtk_label_new(_("Build Date: "));
	gtk_misc_set_alignment(GTK_MISC (temp_widget), 1.0, 0.5);
 	gtk_label_set_justify (GTK_LABEL (temp_widget), GTK_JUSTIFY_RIGHT);
 	gtk_table_attach(table, temp_widget, 2,3, 2,3, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	
	rpm_view->details->package_bdate = gtk_label_new(_("<unknown>"));
	gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_bdate), GTK_JUSTIFY_LEFT);	
	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_bdate), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_bdate, 3,4, 2,3, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_bdate);

  	temp_widget = gtk_label_new(_("Distribution: "));
 	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
 	gtk_label_set_justify (GTK_LABEL (temp_widget), GTK_JUSTIFY_RIGHT);
 	gtk_table_attach(table, temp_widget, 0,1, 3,4, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	
	rpm_view->details->package_distribution = gtk_label_new(_("<unknown>"));
 	gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_distribution), GTK_JUSTIFY_LEFT);	
	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_distribution), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_distribution, 1,2, 3,4, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_distribution);

  	temp_widget = gtk_label_new(_("Vendor: "));
  	gtk_misc_set_alignment(GTK_MISC(temp_widget), 1.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (temp_widget), GTK_JUSTIFY_RIGHT);
  	gtk_table_attach(table, temp_widget, 2,3, 3,4, GTK_FILL, GTK_FILL, 0,0);
  	gtk_widget_show(temp_widget);
  	
	rpm_view->details->package_vendor = gtk_label_new(_("<unknown>"));
 	gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_vendor), GTK_JUSTIFY_LEFT);	
   	gtk_misc_set_alignment(GTK_MISC(rpm_view->details->package_vendor), 0.0, 0.5);
  	gtk_table_attach(table, rpm_view->details->package_vendor, 3,4, 3,4, GTK_FILL|GTK_EXPAND, GTK_FILL, 0,0);
  	gtk_widget_show(rpm_view->details->package_vendor);

	/* insert the data table */
	temp_widget = gtk_hseparator_new();	
	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), temp_widget, FALSE, FALSE, 2);	
	gtk_widget_show (temp_widget);
	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), GTK_WIDGET (table), FALSE, FALSE, 2);	
	gtk_widget_show (GTK_WIDGET(table));
	
	/* make the install message and button area  */
	
	temp_box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (rpm_view->details->package_container), temp_box, FALSE, FALSE, 8);	
	gtk_widget_show(temp_box);
	
	rpm_view->details->package_installed_message = gtk_label_new("");
 	
	gtk_box_pack_start (GTK_BOX (temp_box), rpm_view->details->package_installed_message,
				 FALSE, FALSE, 2);		
	gtk_widget_show (rpm_view->details->package_installed_message);
	
	/* install button */
	rpm_view->details->package_install_button = gtk_button_new();		    
	temp_widget = gtk_label_new (_("Install"));
	gtk_widget_show (temp_widget);
	gtk_container_add (GTK_CONTAINER (rpm_view->details->package_install_button), temp_widget); 	
	gtk_box_pack_start(GTK_BOX (temp_box), rpm_view->details->package_install_button,
				 FALSE, FALSE, 2);		
	gtk_widget_show(rpm_view->details->package_install_button);

#ifdef EAZEL_SERVICES
        gtk_signal_connect (GTK_OBJECT (rpm_view->details->package_install_button), 
                            "clicked", 
                            GTK_SIGNAL_FUNC (nautilus_rpm_view_install_package_callback), 
                            rpm_view);
#endif /* EAZEL_SERVICES */        
	
	/* update button */
	rpm_view->details->package_update_button = gtk_button_new();		    
	temp_widget = gtk_label_new (_("Update"));
	gtk_widget_show (temp_widget);
	gtk_container_add (GTK_CONTAINER (rpm_view->details->package_update_button), temp_widget); 	
	gtk_box_pack_start(GTK_BOX (temp_box), rpm_view->details->package_update_button,
				 FALSE, FALSE, 2);		
	gtk_widget_show(rpm_view->details->package_update_button);
	
	/* uninstall button */
	rpm_view->details->package_uninstall_button = gtk_button_new();		    
	temp_widget = gtk_label_new (_("Uninstall"));
	gtk_widget_show (temp_widget);
	gtk_container_add (GTK_CONTAINER (rpm_view->details->package_uninstall_button), temp_widget); 	
	gtk_box_pack_start(GTK_BOX (temp_box), rpm_view->details->package_uninstall_button,
				 FALSE, FALSE, 4);		
	gtk_widget_show(rpm_view->details->package_uninstall_button);

#ifdef EAZEL_SERVICES
        gtk_signal_connect (GTK_OBJECT (rpm_view->details->package_uninstall_button), 
                            "clicked", 
                            GTK_SIGNAL_FUNC (nautilus_rpm_view_uninstall_package_callback), 
                            rpm_view);
#endif /* EAZEL_SERVICES */        

	/* verify button */
	rpm_view->details->package_verify_button = gtk_button_new();		    
	temp_widget = gtk_label_new (_("Verify"));
	gtk_widget_show (temp_widget);
	gtk_container_add (GTK_CONTAINER (rpm_view->details->package_verify_button), temp_widget); 	
	gtk_box_pack_start(GTK_BOX (temp_box), rpm_view->details->package_verify_button,
				 FALSE, FALSE, 4);		
        /* FIXME - 6974, 5123 This feature is buggy and is therefore
         * being disabled for 1.0. It should either be turned on and
         * fixed after 1.0 or should be left to the planned 1.2
         * rewrite.
         gtk_widget_show(rpm_view->details->package_verify_button);
        */

        gtk_signal_connect (GTK_OBJECT (rpm_view->details->package_verify_button), 
                            "clicked", 
                            GTK_SIGNAL_FUNC (nautilus_rpm_view_verify_package_callback), 
                            rpm_view);

	/* add the list of files contained in the package */

  	temp_widget = gtk_scrolled_window_new(NULL, NULL);
  	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(temp_widget),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), temp_widget, TRUE, TRUE, 0);	
  	gtk_widget_show (temp_widget);

        /* use a tree list now */
        list_headers[0] = _(list_headers[0]);
        rpm_view->details->package_file_tree = nautilus_ctree_new_with_titles (1, 0, list_headers);
        gtk_container_add (GTK_CONTAINER (temp_widget), rpm_view->details->package_file_tree);
        gtk_signal_connect (GTK_OBJECT (rpm_view->details->package_file_tree),
                            "tree-activate-row", GTK_SIGNAL_FUNC (file_activation_callback), rpm_view);
        nautilus_ctree_set_line_style (NAUTILUS_CTREE (rpm_view->details->package_file_tree), NAUTILUS_CTREE_LINES_NONE);
        gtk_widget_show (rpm_view->details->package_file_tree);

	/* add an hbox for buttons that operate on the package list */
	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), temp_box, FALSE, FALSE, 4);
	gtk_widget_show (temp_box);
		
	/* add the description */
        temp_widget = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (temp_widget),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        viewport = nautilus_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
        gtk_container_add (GTK_CONTAINER (temp_widget), viewport);
	background = nautilus_get_widget_background (viewport);
	nautilus_background_set_color (background, RPM_VIEW_DEFAULT_BACKGROUND_COLOR);
        gtk_widget_show (viewport);
        gtk_box_pack_start (GTK_BOX (rpm_view->details->package_container), temp_widget, TRUE, TRUE, 8);
        gtk_widget_show (temp_widget);

	rpm_view->details->package_description = gtk_label_new (_("Description"));
        gtk_label_set_justify (GTK_LABEL (rpm_view->details->package_description), GTK_JUSTIFY_LEFT);
        gtk_container_add (GTK_CONTAINER (viewport), rpm_view->details->package_description);
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
#ifdef EAZEL_SERVICES
        PackageData *pack;
#endif /* EAZEL_SERVICES */        

#ifdef EAZEL_SERVICES
        pack = (PackageData *) gtk_object_get_data (GTK_OBJECT (rpm_view), "packagedata");
        if (pack) {
                gtk_object_unref (GTK_OBJECT (pack));
        }

	if (rpm_view->details->root_client) {
		trilobite_root_client_unref (GTK_OBJECT (rpm_view->details->root_client));
	}
	if (rpm_view->details->installer) {
		eazel_install_callback_unref (GTK_OBJECT (rpm_view->details->installer));
	}
	if (rpm_view->details->package_system) {
		gtk_object_unref (GTK_OBJECT (rpm_view->details->package_system));
	}

	g_free (rpm_view->details->remembered_password);
#endif /* EAZEL_SERVICES */

	if (rpm_view->details->verify_window) {
		gtk_object_destroy (GTK_OBJECT (rpm_view->details->verify_window));
	}
	
	g_free (rpm_view->details->current_uri);
	g_free (rpm_view->details->package_name);
	g_free (rpm_view->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* Component embedding support */
NautilusView *
nautilus_rpm_view_get_nautilus_view (NautilusRPMView *rpm_view)
{
	return rpm_view->details->nautilus_view;
}

static void
file_activation_callback (NautilusCTree *ctree, NautilusCTreeNode *node, int column, NautilusRPMView *rpm_view)
{
	char *path_name;
        char *path_item;
        char *new_name;

        path_name = g_strdup ("");
        while (1) {
                nautilus_ctree_node_get_pixtext (ctree, node,
                                                 0, &path_item, NULL, NULL, NULL);
                if (strcmp (path_item, "/") != 0) {
                        new_name = g_strdup_printf ("/%s%s", path_item, path_name);
                        g_free (path_name);
                        path_name = new_name;
                }

                if (NAUTILUS_CTREE_ROW (node)->parent == NULL) {
                        break;
                }
                node = NAUTILUS_CTREE_ROW (node)->parent;
        }

	nautilus_view_open_location_in_this_window (rpm_view->details->nautilus_view, path_name);
}

static void
add_to_filename_tree (char *name, NautilusRPMView *view)
{
        char **path;
        GNode *node, *next_node;
        gboolean found;
        int i;

        path = g_strsplit (name, "/", 0);
        node = view->details->filename_tree;
        for (i = 0; path[i] != NULL; i++) {
                if (path[i][0] == '\0') {
                        continue;
                }
                found = FALSE;
                for (next_node = g_node_first_child (node); next_node != NULL;
                     next_node = g_node_next_sibling (next_node)) {
                        if (strcmp (path[i], (char *) next_node->data) == 0) {
                                /* hit! */
                                found = TRUE;
                                node = next_node;
                                break;
                        }
                }
                if (! found) {
                        next_node = g_node_new (g_strdup (path[i]));
                        g_node_insert (node, -1, next_node);
                        node = next_node;
                }
        }
}

static void
fill_filename_tree_int (GtkWidget *ctree, NautilusCTreeNode *parent, GNode *node)
{
        GNode *child;
        NautilusCTreeNode *me;
        char *text[1];

        for (child = g_node_first_child (node); child != NULL; child = g_node_next_sibling (child)) {
                text[0] = (char *) child->data;
                me = nautilus_ctree_insert_node (NAUTILUS_CTREE (ctree), parent, NULL, text,
                                                 0, NULL, NULL, NULL, NULL,
                                                 (child->children == NULL) ? TRUE : FALSE,
                                                 TRUE);
                fill_filename_tree_int (ctree, me, child);
        }
}

static void
fill_filename_tree (NautilusRPMView *rpm_view)
{
        NautilusCTreeNode *top;
        char *text[] = { "/" };

        top = nautilus_ctree_insert_node (NAUTILUS_CTREE (rpm_view->details->package_file_tree),
                                          NULL, NULL, text,
                                          0, NULL, NULL, NULL, NULL,
                                          FALSE, TRUE);
        fill_filename_tree_int (rpm_view->details->package_file_tree, top, rpm_view->details->filename_tree);
}

static gboolean
filename_node_free (GNode *node)
{
        g_free (node->data);
        return FALSE;
}

/* here's where we do most of the real work of populating the view with info from the package */
/* open the package and copy the information, and then set up the appropriate views with it */
/* FIXME bugzilla.eazel.com 725: use gnome-vfs to open the package */

static gboolean
nautilus_rpm_view_update_from_uri (NautilusRPMView *rpm_view, const char *uri)
{
	char *temp_str;
	gboolean is_installed;
	char *path_name, *default_icon_path;
	
	path_name = gnome_vfs_get_local_path_from_uri (uri);
		
	/* load the standard icon as the default */
	default_icon_path = nautilus_theme_get_image_path ("gnome-pack-rpm.png");
    	nautilus_image_set_pixbuf_from_file_name (NAUTILUS_IMAGE (rpm_view->details->package_image), default_icon_path);
        g_free (default_icon_path);

#ifdef EAZEL_SERVICES	       
        rpm_view->details->package = eazel_package_system_load_package (rpm_view->details->package_system,
                                                                        rpm_view->details->package,
                                                                        path_name,
                                                                        PACKAGE_FILL_EVERYTHING);
	g_free (path_name);

	if (rpm_view->details->package == NULL) {
		return FALSE;
	}

        /* set the file tree */
        gtk_clist_freeze (GTK_CLIST (rpm_view->details->package_file_tree));
        gtk_clist_clear (GTK_CLIST (rpm_view->details->package_file_tree));
        if (rpm_view->details->filename_tree != NULL) {
                g_node_traverse (rpm_view->details->filename_tree, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
                                 (GNodeTraverseFunc)filename_node_free, NULL);
                g_node_destroy (rpm_view->details->filename_tree);
        }
        rpm_view->details->filename_tree = g_node_new (g_strdup ("/"));
        g_list_foreach (rpm_view->details->package->provides, (GFunc)add_to_filename_tree, rpm_view);
        fill_filename_tree (rpm_view);
        temp_str = g_strdup_printf(_("Package Contents: %d files"), 
                                   g_list_length (rpm_view->details->package->provides));
        gtk_clist_set_column_title (GTK_CLIST(rpm_view->details->package_file_tree), 0, temp_str);
        g_free(temp_str);                
        gtk_clist_thaw(GTK_CLIST(rpm_view->details->package_file_tree));
                
        temp_str = g_strdup_printf(_("Package \"%s\" "), rpm_view->details->package->name);
        gtk_label_set_text (GTK_LABEL (rpm_view->details->package_title), temp_str);
        g_free(temp_str);

        temp_str = gnome_vfs_format_file_size_for_display (rpm_view->details->package->bytesize);
        gtk_label_set_text (GTK_LABEL (rpm_view->details->package_size), temp_str);
        g_free (temp_str);					

        temp_str = trilobite_get_distribution_name (rpm_view->details->package->distribution, TRUE, FALSE);
        gtk_label_set_text (GTK_LABEL (rpm_view->details->package_distribution), 
                            temp_str);
        g_free (temp_str);
        
        /* I no longer set
           buildtime
           installtime
           vendor
           gif
           xpm
        */
        
        gtk_label_set_text (GTK_LABEL (rpm_view->details->package_description), 
                            rpm_view->details->package->description);
        gtk_label_set_text (GTK_LABEL (rpm_view->details->package_summary), 
                            rpm_view->details->package->summary );

        
        temp_str = packagedata_get_readable_name (rpm_view->details->package);
        gtk_label_set_text (GTK_LABEL (rpm_view->details->package_release), temp_str);
        g_free (temp_str);
	
	/* determine if the package is installed */
	is_installed = eazel_package_system_is_installed (rpm_view->details->package_system,
                                                          NULL,
                                                          rpm_view->details->package->name,
                                                          rpm_view->details->package->version,
                                                          rpm_view->details->package->minor,
                                                          EAZEL_SOFTCAT_SENSE_EQ);
	rpm_view->details->package_installed = is_installed != 0;
			
	/* set up the install message and buttons */
	if (is_installed) {
		gtk_label_set_text (GTK_LABEL(rpm_view->details->package_installed_message),
                                    _("This package is currently installed"));
	} else {
		gtk_label_set_text (GTK_LABEL(rpm_view->details->package_installed_message),
                                    _("This package is currently not installed"));
        }
	
	if (is_installed == 0) {
		gtk_widget_show(rpm_view->details->package_install_button);
	} else {
		gtk_widget_hide(rpm_view->details->package_install_button);
	}
	if (is_installed == 255) {
		gtk_widget_show(rpm_view->details->package_update_button);
	} else {
		gtk_widget_hide(rpm_view->details->package_update_button);
	}
	if (is_installed != 0) {
		gtk_widget_show (rpm_view->details->package_uninstall_button);
                /* FIXME - 6974, 5123 This feature is buggy and is therefore
                 * being disabled for 1.0. It should either be turned on and
                 * fixed after 1.0 or should be left to the planned 1.2
                 * rewrite.
                 gtk_widget_show (rpm_view->details->package_verify_button);
                */
        } else {
                gtk_widget_hide (rpm_view->details->package_uninstall_button);
                /* FIXME - 6974, 5123 This feature is buggy and is therefore
                 * being disabled for 1.0. It should either be turned on and
                 * fixed after 1.0 or should be left to the planned 1.2
                 * rewrite.
                 gtk_widget_hide (rpm_view->details->package_verify_button);
                */
        }	
#endif /* EAZEL_SERVICES */              
	return TRUE;
}

char*
nautilus_rpm_view_get_uri (NautilusRPMView *view)
{
        return view->details->current_uri;
}

gboolean 
nautilus_rpm_view_get_installed (NautilusRPMView *view)
{
        return view->details->package_installed;
}

NautilusView* 
nautilus_rpm_view_get_view (NautilusRPMView *view)
{
        return view->details->nautilus_view;
}

gboolean
nautilus_rpm_view_load_uri (NautilusRPMView *rpm_view, const char *uri)
{
        g_free(rpm_view->details->current_uri);
        rpm_view->details->current_uri = g_strdup (uri);	
        return nautilus_rpm_view_update_from_uri(rpm_view, uri);
}

static void
rpm_view_load_location_callback (NautilusView *view, 
                                 const char *location,
                                 NautilusRPMView *rpm_view)
{
        nautilus_view_report_load_underway (rpm_view->details->nautilus_view);
        if (nautilus_rpm_view_load_uri (rpm_view, location)) {
        	nautilus_view_report_load_complete (rpm_view->details->nautilus_view);
	} else {
		nautilus_view_report_load_failed (rpm_view->details->nautilus_view);
	}
}

static gboolean
verify_failed_signal (EazelPackageSystem *system,
                      EazelPackageSystemOperation op, 
                      const PackageData *package,
                      NautilusRPMView *rpm_view)
{
        nautilus_rpm_verify_window_set_error_mode (NAUTILUS_RPM_VERIFY_WINDOW (rpm_view->details->verify_window), TRUE);
        rpm_view->details->verify_success = FALSE;

        return FALSE;
}

static gboolean  
verify_progress_signal (EazelPackageSystem *system,
                        EazelPackageSystemOperation op,
                        const PackageData *pack,
                        unsigned long *info,
                        GtkWidget *verify_window)
{
        g_assert (verify_window);
        g_assert (NAUTILUS_RPM_VERIFY_WINDOW (verify_window));
        nautilus_rpm_verify_window_set_progress (NAUTILUS_RPM_VERIFY_WINDOW (verify_window),
                                                 (g_list_nth (pack->provides, info[0]-1))->data,
                                                 info[0], info[1]);
        return TRUE;
}

/* routine to handle the verify command */
static void 
nautilus_rpm_view_verify_files (GtkWidget *widget,
                                NautilusRPMView *rpm_view)
{
        GList *packages = g_list_prepend (NULL, rpm_view->details->package);
        guint failsignal, progresssignal;
        
	/* put up a window to give file handling feedback */
	if (rpm_view->details->verify_window == NULL) {
		rpm_view->details->verify_window = nautilus_rpm_verify_window_new (rpm_view->details->package->name);	
                /*
		gtk_signal_connect (GTK_OBJECT (rpm_view->details->verify_window), 
			    "continue",
			    GTK_SIGNAL_FUNC (rpm_view_continue_verify), 
			    rpm_view);
                */
	
	}
	
	nautilus_gtk_window_present (GTK_WINDOW (rpm_view->details->verify_window));

        failsignal = gtk_signal_connect (GTK_OBJECT (rpm_view->details->package_system),
                                         "failed",
                                         (GtkSignalFunc)verify_failed_signal,
                                         rpm_view);
        progresssignal = gtk_signal_connect (GTK_OBJECT (rpm_view->details->package_system),
                                             "progress",
                                             (GtkSignalFunc)verify_progress_signal,
                                             rpm_view->details->verify_window);

        rpm_view->details->verify_success = TRUE;
        eazel_package_system_verify (rpm_view->details->package_system,
                                     NULL, 
                                     packages);

        gtk_signal_disconnect (GTK_OBJECT (rpm_view->details->package_system), failsignal);
        gtk_signal_disconnect (GTK_OBJECT (rpm_view->details->package_system), progresssignal);                               
        g_list_free (packages);
		
        if (rpm_view->details->verify_success) {
                nautilus_rpm_verify_window_set_error_mode (NAUTILUS_RPM_VERIFY_WINDOW (rpm_view->details->verify_window), FALSE);
                nautilus_rpm_verify_window_set_message (NAUTILUS_RPM_VERIFY_WINDOW (rpm_view->details->verify_window),
                                                        _("Verification completed, package ok."));
        } else {
        }
}

/* callback to handle the verify command */
static void 
nautilus_rpm_view_verify_package_callback (GtkWidget *widget,
                                           NautilusRPMView *rpm_view)
{
	rpm_view->details->last_file_index = 0;
	nautilus_rpm_view_verify_files (widget, rpm_view);
}

/* handle drag and drop */
static void  
nautilus_rpm_view_drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                      int x, int y,
                                      GtkSelectionData *selection_data, guint info, guint time)
{
        g_return_if_fail (NAUTILUS_IS_RPM_VIEW (widget));

        switch (info) {
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

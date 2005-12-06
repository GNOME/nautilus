/* nautilus-pathbar.c
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-preferences.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include "nautilus-pathbar.h"

enum {
        PATH_CLICKED,
        LAST_SIGNAL
};

typedef enum {
        NORMAL_BUTTON,
        ROOT_BUTTON,
        HOME_BUTTON,
        DESKTOP_BUTTON,
	VOLUME_BUTTON,
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *)(x))

#define SCROLL_TIMEOUT           150
#define INITIAL_SCROLL_TIMEOUT   300

static guint path_bar_signals [LAST_SIGNAL] = { 0 };

static gboolean desktop_is_home;

#define NAUTILUS_PATH_BAR_ICON_SIZE 16

#define DEFAULT_ICON 		"gnome-fs-directory"
#define DEFAULT_DESKTOP_ICON 	"gnome-fs-desktop"
#define DEFAULT_HOME_ICON 	"gnome-fs-home"
#define DEFAULT_FILESYSTEM_ICON	"gnome-dev-harddisk"

typedef struct _ButtonData ButtonData;

struct _ButtonData
{
        GtkWidget *button;
        ButtonType type;
        char *dir_name;
        char *path;

	/* custom icon */ 
	char *custom_icon_name;

	/* flag to indicate its the base folder in the URI */
	gboolean is_base_dir;

        GtkWidget *image;
        GtkWidget *label;
        guint ignore_changes : 1;
        guint file_is_hidden : 1;
};

/* This macro is used to check if a button can be used as a fake root.
 * All buttons in front of a fake root are automatically hidden when in a
 * directory below a fake root and replaced with the "<" arrow button.
 */
#define BUTTON_IS_FAKE_ROOT(button) ((button)->type == HOME_BUTTON || (button)->type == VOLUME_BUTTON)

G_DEFINE_TYPE (NautilusPathBar,
	       nautilus_path_bar,
	       GTK_TYPE_CONTAINER);

static void     nautilus_path_bar_finalize                 (GObject         *object);
static void     nautilus_path_bar_dispose                  (GObject         *object);
static void     nautilus_path_bar_size_request             (GtkWidget       *widget,
							    GtkRequisition  *requisition);
static void     nautilus_path_bar_unmap                    (GtkWidget       *widget);
static void     nautilus_path_bar_size_allocate            (GtkWidget       *widget,
							    GtkAllocation   *allocation);
static void     nautilus_path_bar_add                      (GtkContainer    *container,
							    GtkWidget       *widget);
static void     nautilus_path_bar_remove                   (GtkContainer    *container,
							    GtkWidget       *widget);
static void     nautilus_path_bar_forall                   (GtkContainer    *container,
							    gboolean         include_internals,
							    GtkCallback      callback,
							    gpointer         callback_data);
static void     nautilus_path_bar_scroll_up                (GtkWidget       *button,
							    NautilusPathBar *path_bar);
static void     nautilus_path_bar_scroll_down              (GtkWidget       *button,
							    NautilusPathBar *path_bar);
static void     nautilus_path_bar_stop_scrolling           (NautilusPathBar *path_bar);
static gboolean nautilus_path_bar_slider_button_press      (GtkWidget       *widget,
							    GdkEventButton  *event,
							    NautilusPathBar *path_bar);
static gboolean nautilus_path_bar_slider_button_release    (GtkWidget       *widget,
							    GdkEventButton  *event,
							    NautilusPathBar *path_bar);
static void     nautilus_path_bar_grab_notify              (GtkWidget       *widget,
							    gboolean         was_grabbed);
static void     nautilus_path_bar_state_changed            (GtkWidget       *widget,
							    GtkStateType     previous_state);
static void     nautilus_path_bar_style_set                (GtkWidget       *widget,
							    GtkStyle        *previous_style);
static void     nautilus_path_bar_screen_changed           (GtkWidget       *widget,
							    GdkScreen       *previous_screen);
static void     nautilus_path_bar_check_icon_theme         (NautilusPathBar *path_bar);
static void     nautilus_path_bar_update_button_appearance (NautilusPathBar *path_bar,
							    ButtonData      *button_data,
							    gboolean         current_dir);
static gboolean nautilus_path_bar_update_path              (NautilusPathBar *path_bar,
							    const char      *file_path);

static GtkWidget *
get_slider_button (NautilusPathBar  *path_bar,
		   GtkArrowType arrow_type)
{
        GtkWidget *button;

        gtk_widget_push_composite_child ();

        button = gtk_button_new ();
        gtk_container_add (GTK_CONTAINER (button), gtk_arrow_new (arrow_type, GTK_SHADOW_OUT));
        gtk_container_add (GTK_CONTAINER (path_bar), button);
        gtk_widget_show_all (button);

        gtk_widget_pop_composite_child ();

        return button;
}

static void
update_button_types (NautilusPathBar *path_bar)
{
	GList *list;
	char *path = NULL;

	for (list = path_bar->button_list; list; list = list->next) {
		ButtonData *button_data;
		button_data = BUTTON_DATA (list->data);
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button))) {
			path = button_data->path;
			break;
		}
        }
	if (path != NULL) {
		nautilus_path_bar_update_path (path_bar, path);
	}
}


static void
desktop_location_changed_callback (gpointer user_data)
{
	NautilusPathBar *path_bar;
	
	path_bar = NAUTILUS_PATH_BAR (user_data);
	
	g_free (path_bar->desktop_path);
	g_free (path_bar->home_path);
	path_bar->desktop_path = nautilus_get_desktop_directory_uri ();
	path_bar->home_path = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	desktop_is_home = (strcmp (path_bar->home_path, path_bar->desktop_path) == 0);

        if (path_bar->home_icon) {
                g_object_unref (path_bar->home_icon);
                path_bar->home_icon = NULL;
        }
	
	update_button_types (path_bar);
}

static void
nautilus_path_bar_init (NautilusPathBar *path_bar)
{
        GTK_WIDGET_SET_FLAGS (path_bar, GTK_NO_WINDOW);
        gtk_widget_set_redraw_on_allocate (GTK_WIDGET (path_bar), FALSE);

        path_bar->spacing = 3;
        path_bar->up_slider_button = get_slider_button (path_bar, GTK_ARROW_LEFT);
        path_bar->down_slider_button = get_slider_button (path_bar, GTK_ARROW_RIGHT);
        path_bar->icon_size = NAUTILUS_PATH_BAR_ICON_SIZE;

        path_bar->desktop_path = nautilus_get_desktop_directory_uri ();
	path_bar->home_path = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	path_bar->root_path = g_strdup ("file:///");
	desktop_is_home = (strcmp (path_bar->home_path, path_bar->desktop_path) == 0);

	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
						  desktop_location_changed_callback,
						  path_bar,
						  G_OBJECT (path_bar));

        g_signal_connect (path_bar->up_slider_button, "clicked", G_CALLBACK (nautilus_path_bar_scroll_up), path_bar);
        g_signal_connect (path_bar->down_slider_button, "clicked", G_CALLBACK (nautilus_path_bar_scroll_down), path_bar);

        g_signal_connect (path_bar->up_slider_button, "button_press_event", G_CALLBACK (nautilus_path_bar_slider_button_press), path_bar);
        g_signal_connect (path_bar->up_slider_button, "button_release_event", G_CALLBACK (nautilus_path_bar_slider_button_release), path_bar);
        g_signal_connect (path_bar->down_slider_button, "button_press_event", G_CALLBACK (nautilus_path_bar_slider_button_press), path_bar);
        g_signal_connect (path_bar->down_slider_button, "button_release_event", G_CALLBACK (nautilus_path_bar_slider_button_release), path_bar);
}

static void
nautilus_path_bar_class_init (NautilusPathBarClass *path_bar_class)
{
        GObjectClass *gobject_class;
        GtkObjectClass *object_class;
        GtkWidgetClass *widget_class;
        GtkContainerClass *container_class;

        gobject_class = (GObjectClass *) path_bar_class;
        object_class = (GtkObjectClass *) path_bar_class;
        widget_class = (GtkWidgetClass *) path_bar_class;
        container_class = (GtkContainerClass *) path_bar_class;

        gobject_class->finalize = nautilus_path_bar_finalize;
        gobject_class->dispose = nautilus_path_bar_dispose;

        widget_class->size_request = nautilus_path_bar_size_request;
	widget_class->unmap = nautilus_path_bar_unmap;
        widget_class->size_allocate = nautilus_path_bar_size_allocate;
        widget_class->style_set = nautilus_path_bar_style_set;
        widget_class->screen_changed = nautilus_path_bar_screen_changed;
        widget_class->grab_notify = nautilus_path_bar_grab_notify;
        widget_class->state_changed = nautilus_path_bar_state_changed;

        container_class->add = nautilus_path_bar_add;
        container_class->forall = nautilus_path_bar_forall;
        container_class->remove = nautilus_path_bar_remove;

        path_bar_signals [PATH_CLICKED] =
                g_signal_new ("path-clicked",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (NautilusPathBarClass, path_clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);
}


static void
nautilus_path_bar_finalize (GObject *object)
{
        NautilusPathBar *path_bar;

        path_bar = NAUTILUS_PATH_BAR (object);

	nautilus_path_bar_stop_scrolling (path_bar);

        g_list_free (path_bar->button_list);
	if (path_bar->root_path) {
		g_free (path_bar->root_path);
		path_bar->root_path = NULL;
	}
	if (path_bar->home_path) {
		g_free (path_bar->home_path);
		path_bar->home_path = NULL;
	}
	if (path_bar->desktop_path) {
		g_free (path_bar->desktop_path);
		path_bar->desktop_path = NULL;
	}

	if (path_bar->root_icon) {
		g_object_unref (path_bar->root_icon);
		path_bar->root_icon = NULL;
	}
	if (path_bar->home_icon) {
                g_object_unref (path_bar->home_icon);
		path_bar->home_icon = NULL;
	}
	if (path_bar->desktop_icon) {
		g_object_unref (path_bar->desktop_icon);
		path_bar->desktop_icon = NULL;
	}

        G_OBJECT_CLASS (nautilus_path_bar_parent_class)->finalize (object);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (NautilusPathBar *path_bar,
			GdkScreen  *screen)
{
	if (path_bar->settings_signal_id) {
 	 	GtkSettings *settings;
	
 	     	settings = gtk_settings_get_for_screen (screen);
 	     	g_signal_handler_disconnect (settings,
	   				     path_bar->settings_signal_id);
	      	path_bar->settings_signal_id = 0;
        }
}

static void
nautilus_path_bar_dispose (GObject *object)
{
        remove_settings_signal (NAUTILUS_PATH_BAR (object), gtk_widget_get_screen (GTK_WIDGET (object)));

        G_OBJECT_CLASS (nautilus_path_bar_parent_class)->dispose (object);
}

/* Size requisition:
 * 
 * Ideally, our size is determined by another widget, and we are just filling
 * available space.
 */
static void
nautilus_path_bar_size_request (GtkWidget      *widget,
			        GtkRequisition *requisition)
{
        ButtonData *button_data;
        NautilusPathBar *path_bar;
        GtkRequisition child_requisition;
        GList *list;

        path_bar = NAUTILUS_PATH_BAR (widget);

        requisition->width = 0;
        requisition->height = 0;

	for (list = path_bar->button_list; list; list = list->next) {
		button_data = BUTTON_DATA (list->data);
                gtk_widget_size_request (button_data->button, &child_requisition);
                requisition->width = MAX (child_requisition.width, requisition->width);
                requisition->height = MAX (child_requisition.height, requisition->height);
        }

        /* Add space for slider, if we have more than one path */
        /* Theoretically, the slider could be bigger than the other button.  But we're */
        /* not going to worry about that now.*/

        path_bar->slider_width = MIN(requisition->height * 2 / 3 + 5, requisition->height);
	if (path_bar->button_list && path_bar->button_list->next != NULL) {
		requisition->width += (path_bar->spacing + path_bar->slider_width) * 2;
	}

        gtk_widget_size_request (path_bar->up_slider_button, &child_requisition);
        gtk_widget_size_request (path_bar->down_slider_button, &child_requisition);

        requisition->width += GTK_CONTAINER (widget)->border_width * 2;
        requisition->height += GTK_CONTAINER (widget)->border_width * 2;

        widget->requisition = *requisition;
}

static void
nautilus_path_bar_update_slider_buttons (NautilusPathBar *path_bar)
{
	if (path_bar->button_list) {
                	
      		GtkWidget *button;

	        button = BUTTON_DATA (path_bar->button_list->data)->button;
   		if (gtk_widget_get_child_visible (button)) {
			gtk_widget_set_sensitive (path_bar->down_slider_button, FALSE);
		} else {
			gtk_widget_set_sensitive (path_bar->down_slider_button, TRUE);
		}
       		button = BUTTON_DATA (g_list_last (path_bar->button_list)->data)->button;
                if (gtk_widget_get_child_visible (button)) {
			gtk_widget_set_sensitive (path_bar->up_slider_button, FALSE);
                } else {
			gtk_widget_set_sensitive (path_bar->up_slider_button, TRUE);
		}
	}
}

static void
nautilus_path_bar_unmap (GtkWidget *widget)
{
	nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));

	GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->unmap (widget);
}

/* This is a tad complicated */
static void
nautilus_path_bar_size_allocate (GtkWidget     *widget,
			    	 GtkAllocation *allocation)
{
        GtkWidget *child;
        NautilusPathBar *path_bar;
        GtkTextDirection direction;
        GtkAllocation child_allocation;
        GList *list, *first_button;
        gint width;
        gint allocation_width;
        gint border_width;
        gboolean need_sliders;
        gint up_slider_offset;
        gint down_slider_offset;

	need_sliders = FALSE;
	up_slider_offset = 0;
	down_slider_offset = 0;
	path_bar = NAUTILUS_PATH_BAR (widget);

        widget->allocation = *allocation;


        /* No path is set so we don't have to allocate anything. */
        if (path_bar->button_list == NULL) {
                return;
	}
        direction = gtk_widget_get_direction (widget);
        border_width = (gint) GTK_CONTAINER (path_bar)->border_width;
        allocation_width = allocation->width - 2 * border_width;

  	/* First, we check to see if we need the scrollbars. */
  	if (path_bar->fake_root) {
		width = path_bar->spacing + path_bar->slider_width;
	} else {
		width = 0;
	}

	width += BUTTON_DATA (path_bar->button_list->data)->button->requisition.width;

        for (list = path_bar->button_list->next; list; list = list->next) {
        	child = BUTTON_DATA (list->data)->button;
                width += child->requisition.width + path_bar->spacing;

		if (list == path_bar->fake_root) {
			break;
		}
        }

        if (width <= allocation_width) {
                if (path_bar->fake_root) {
			first_button = path_bar->fake_root;
      		} else {
			first_button = g_list_last (path_bar->button_list);
		}
        } else {
                gboolean reached_end;
                gint slider_space;
		reached_end = FALSE;
		slider_space = 2 * (path_bar->spacing + path_bar->slider_width);

                if (path_bar->first_scrolled_button) {
			first_button = path_bar->first_scrolled_button;
		} else {
			first_button = path_bar->button_list;
                }        

		need_sliders = TRUE;
      		/* To see how much space we have, and how many buttons we can display.
       		* We start at the first button, count forward until hit the new
       		* button, then count backwards.
       		*/
      		/* Count down the path chain towards the end. */
                width = BUTTON_DATA (first_button->data)->button->requisition.width;
                list = first_button->prev;
                while (list && !reached_end) {
	  		child = BUTTON_DATA (list->data)->button;

	  		if (width + child->requisition.width + path_bar->spacing + slider_space > allocation_width) {
	    			reached_end = TRUE;
	  		} else {
				if (list == path_bar->fake_root) {
					break;
				} else {
	    				width += child->requisition.width + path_bar->spacing;
				}
			}

	  		list = list->prev;
		}

                /* Finally, we walk up, seeing how many of the previous buttons we can add*/

                while (first_button->next && ! reached_end) {
	  		child = BUTTON_DATA (first_button->next->data)->button;
	  		if (width + child->requisition.width + path_bar->spacing + slider_space > allocation_width) {
	      			reached_end = TRUE;
	    		} else {
	      			width += child->requisition.width + path_bar->spacing;
				if (first_button == path_bar->fake_root) {
					break;
				}
	      			first_button = first_button->next;
	    		}
		}
        }

        /* Now, we allocate space to the buttons */
        child_allocation.y = allocation->y + border_width;
        child_allocation.height = MAX (1, (gint) allocation->height - border_width * 2);

        if (direction == GTK_TEXT_DIR_RTL) {
                child_allocation.x = allocation->x + allocation->width - border_width;
                if (need_sliders || path_bar->fake_root) {
	  		child_allocation.x -= (path_bar->spacing + path_bar->slider_width);
	  		up_slider_offset = allocation->width - border_width - path_bar->slider_width;
		}
        } else {
                child_allocation.x = allocation->x + border_width;
                if (need_sliders || path_bar->fake_root) {
	  		up_slider_offset = border_width;
	  		child_allocation.x += (path_bar->spacing + path_bar->slider_width);
		}
        }

        for (list = first_button; list; list = list->prev) {
                child = BUTTON_DATA (list->data)->button;

                child_allocation.width = child->requisition.width;
                if (direction == GTK_TEXT_DIR_RTL) {
			child_allocation.x -= child_allocation.width;
		}
                /* Check to see if we've don't have any more space to allocate buttons */
                if (need_sliders && direction == GTK_TEXT_DIR_RTL) {
	  		if (child_allocation.x - path_bar->spacing - path_bar->slider_width < widget->allocation.x + border_width) {
			    break;
			}
		} else {
			if (need_sliders && direction == GTK_TEXT_DIR_LTR) {
	  			if (child_allocation.x + child_allocation.width + path_bar->spacing + path_bar->slider_width > widget->allocation.x + border_width + allocation_width) {
	    				break;	
				}	
			}
		}

                gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, TRUE);
                gtk_widget_size_allocate (child, &child_allocation);

                if (direction == GTK_TEXT_DIR_RTL) {
			child_allocation.x -= path_bar->spacing;
	  		down_slider_offset = child_allocation.x - widget->allocation.x - path_bar->slider_width;
	  		down_slider_offset = border_width;
		} else {
			down_slider_offset = child_allocation.x - widget->allocation.x;
	  		down_slider_offset = allocation->width - border_width - path_bar->slider_width;
	  		child_allocation.x += child_allocation.width + path_bar->spacing;
		}
        }
        /* Now we go hide all the widgets that don't fit */
        while (list) {
        	gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, FALSE);
                list = list->prev;
        }
        for (list = first_button->next; list; list = list->next) {
 	        gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, FALSE);
        }

        if (need_sliders || path_bar->fake_root) {
                child_allocation.width = path_bar->slider_width;
                child_allocation.x = up_slider_offset + allocation->x;
                gtk_widget_size_allocate (path_bar->up_slider_button, &child_allocation);

                gtk_widget_set_child_visible (path_bar->up_slider_button, TRUE);
                gtk_widget_show_all (path_bar->up_slider_button);

        } else {
        	gtk_widget_set_child_visible (path_bar->up_slider_button, FALSE);
        }
	
	if (need_sliders) {
    	        child_allocation.width = path_bar->slider_width;
        	child_allocation.x = down_slider_offset + allocation->x;
        	gtk_widget_size_allocate (path_bar->down_slider_button, &child_allocation);

      		gtk_widget_set_child_visible (path_bar->down_slider_button, TRUE);
      		gtk_widget_show_all (path_bar->down_slider_button);
      		nautilus_path_bar_update_slider_buttons (path_bar);
    	} else {
    		gtk_widget_set_child_visible (path_bar->down_slider_button, FALSE);
	}
}

static void
nautilus_path_bar_style_set (GtkWidget *widget,	GtkStyle  *previous_style)
{
        if (GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->style_set) {
        	GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->style_set (widget, previous_style);
	}

        nautilus_path_bar_check_icon_theme (NAUTILUS_PATH_BAR (widget));
}

static void
nautilus_path_bar_screen_changed (GtkWidget *widget,
			          GdkScreen *previous_screen)
{
        if (GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->screen_changed) {
                GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->screen_changed (widget, previous_screen);
	}
        /* We might nave a new settings, so we remove the old one */
        if (previous_screen) {
                remove_settings_signal (NAUTILUS_PATH_BAR (widget), previous_screen);
	}
        nautilus_path_bar_check_icon_theme (NAUTILUS_PATH_BAR (widget));
}

static void
nautilus_path_bar_add (GtkContainer *container,
		       GtkWidget    *widget)
{
        gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
nautilus_path_bar_remove_1 (GtkContainer *container,
		       	    GtkWidget    *widget)
{
        gboolean was_visible = GTK_WIDGET_VISIBLE (widget);
        gtk_widget_unparent (widget);
        if (was_visible) {
                gtk_widget_queue_resize (GTK_WIDGET (container));
	}
}

static void
nautilus_path_bar_remove (GtkContainer *container,
		          GtkWidget    *widget)
{
        NautilusPathBar *path_bar;
        GList *children;

        path_bar = NAUTILUS_PATH_BAR (container);

        if (widget == path_bar->up_slider_button) {
                nautilus_path_bar_remove_1 (container, widget);
                path_bar->up_slider_button = NULL;
                return;
        }

        if (widget == path_bar->down_slider_button) {
                nautilus_path_bar_remove_1 (container, widget);
                path_bar->down_slider_button = NULL;
                return;
        }

        children = path_bar->button_list;
        while (children) {              
                if (widget == BUTTON_DATA (children->data)->button) {
			nautilus_path_bar_remove_1 (container, widget);
	  		path_bar->button_list = g_list_remove_link (path_bar->button_list, children);
	  		g_list_free (children);
	  		return;
		}
                children = children->next;
        }
}

static void
nautilus_path_bar_forall (GtkContainer *container,
		     	  gboolean      include_internals,
		     	  GtkCallback   callback,
		     	  gpointer      callback_data)
{
        NautilusPathBar *path_bar;
        GList *children;

        g_return_if_fail (callback != NULL);
        path_bar = NAUTILUS_PATH_BAR (container);

        children = path_bar->button_list;
        while (children) {
               GtkWidget *child;
               child = BUTTON_DATA (children->data)->button;
                children = children->next;
                (* callback) (child, callback_data);
        }

        if (path_bar->up_slider_button) {
                (* callback) (path_bar->up_slider_button, callback_data);
	}

        if (path_bar->down_slider_button) {
                (* callback) (path_bar->down_slider_button, callback_data);
	}
}

static void
nautilus_path_bar_scroll_down (GtkWidget *button, NautilusPathBar *path_bar)
{
        GList *list;
        GList *down_button;
        GList *up_button;
        gint space_available;
        gint space_needed;
        gint border_width;
        GtkTextDirection direction;

	down_button = NULL;
	up_button = NULL;

        if (path_bar->ignore_click) {
                path_bar->ignore_click = FALSE;
                return;   
        }

        gtk_widget_queue_resize (GTK_WIDGET (path_bar));

        border_width = GTK_CONTAINER (path_bar)->border_width;
        direction = gtk_widget_get_direction (GTK_WIDGET (path_bar));
  
        /* We find the button at the 'down' end that we have to make */
        /* visible */
        for (list = path_bar->button_list; list; list = list->next) {
        	if (list->next && gtk_widget_get_child_visible (BUTTON_DATA (list->next->data)->button)) {
			down_button = list;
	  		break;
		}
        }
  
        /* Find the last visible button on the 'up' end */
        for (list = g_list_last (path_bar->button_list); list; list = list->prev) {
                if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button)) {
	  		up_button = list;
	  		break;
		}
        }

        space_needed = BUTTON_DATA (down_button->data)->button->allocation.width + path_bar->spacing;
        if (direction == GTK_TEXT_DIR_RTL) {
                space_available = path_bar->down_slider_button->allocation.x - GTK_WIDGET (path_bar)->allocation.x;
	} else {
                space_available = (GTK_WIDGET (path_bar)->allocation.x + GTK_WIDGET (path_bar)->allocation.width - border_width) -
                        (path_bar->down_slider_button->allocation.x + path_bar->down_slider_button->allocation.width);
	}

  	/* We have space_available extra space that's not being used.  We
   	* need space_needed space to make the button fit.  So we walk down
   	* from the end, removing buttons until we get all the space we
   	* need. */
        while (space_available < space_needed) {
                space_available += BUTTON_DATA (up_button->data)->button->allocation.width + path_bar->spacing;
                up_button = up_button->prev;
                path_bar->first_scrolled_button = up_button;
        }
}

static void
nautilus_path_bar_scroll_up (GtkWidget *button, NautilusPathBar *path_bar)
{
        GList *list;

        if (path_bar->ignore_click) {
                path_bar->ignore_click = FALSE;
                return;   
        }

        gtk_widget_queue_resize (GTK_WIDGET (path_bar));

        for (list = g_list_last (path_bar->button_list); list; list = list->prev) {
                if (list->prev && gtk_widget_get_child_visible (BUTTON_DATA (list->prev->data)->button)) {
			if (list->prev == path_bar->fake_root) {
	    			path_bar->fake_root = NULL;
			}
			path_bar->first_scrolled_button = list;
	  		return;
		}
        }
}

static gboolean
nautilus_path_bar_scroll_timeout (NautilusPathBar *path_bar)
{
        gboolean retval = FALSE;

        GDK_THREADS_ENTER ();

        if (path_bar->timer) {
                if (GTK_WIDGET_HAS_FOCUS (path_bar->up_slider_button)) {
			nautilus_path_bar_scroll_up (path_bar->up_slider_button, path_bar);
		} else {
			if (GTK_WIDGET_HAS_FOCUS (path_bar->down_slider_button)) {
				nautilus_path_bar_scroll_down (path_bar->down_slider_button, path_bar);
			}
         	}
         	if (path_bar->need_timer) {
			path_bar->need_timer = FALSE;

	  		path_bar->timer = g_timeout_add (SCROLL_TIMEOUT,
				   			 (GSourceFunc)nautilus_path_bar_scroll_timeout,
				   			 path_bar);
	  
		} else {
			retval = TRUE;
		}
        }            
                

        GDK_THREADS_LEAVE ();

        return retval;
}

static void 
nautilus_path_bar_stop_scrolling (NautilusPathBar *path_bar)
{
        if (path_bar->timer) {
                g_source_remove (path_bar->timer);
                path_bar->timer = 0;
                path_bar->need_timer = FALSE;
        }
}

static gboolean
nautilus_path_bar_slider_button_press (GtkWidget       *widget, 
	   			       GdkEventButton  *event,
				       NautilusPathBar *path_bar)
{
        if (!GTK_WIDGET_HAS_FOCUS (widget)) {
                gtk_widget_grab_focus (widget);
	}

        if (event->type != GDK_BUTTON_PRESS || event->button != 1) {
                return FALSE;
	}

        path_bar->ignore_click = FALSE;

        if (widget == path_bar->up_slider_button) {
                nautilus_path_bar_scroll_up (path_bar->up_slider_button, path_bar);
	} else {
		if (widget == path_bar->down_slider_button) {
                       nautilus_path_bar_scroll_down (path_bar->down_slider_button, path_bar);
		}
	}

        if (!path_bar->timer) {
                path_bar->need_timer = TRUE;
                path_bar->timer = g_timeout_add (INITIAL_SCROLL_TIMEOUT,
					         (GSourceFunc)nautilus_path_bar_scroll_timeout,
				                 path_bar);
        }

        return FALSE;
}

static gboolean
nautilus_path_bar_slider_button_release (GtkWidget      *widget, 
  				         GdkEventButton *event,
				         NautilusPathBar     *path_bar)
{
        if (event->type != GDK_BUTTON_RELEASE) {
                return FALSE;
	}

        path_bar->ignore_click = TRUE;
        nautilus_path_bar_stop_scrolling (path_bar);

        return FALSE;
}

static void
nautilus_path_bar_grab_notify (GtkWidget *widget,
			       gboolean   was_grabbed)
{
        if (!was_grabbed) {
                nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
	}
}

static void
nautilus_path_bar_state_changed (GtkWidget    *widget,
			         GtkStateType  previous_state)
{
        if (!GTK_WIDGET_IS_SENSITIVE (widget)) {
                nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
	}
}



/* Changes the icons wherever it is needed */
static void
reload_icons (NautilusPathBar *path_bar)
{
        GList *list;

        if (path_bar->root_icon) {
                g_object_unref (path_bar->root_icon);
                path_bar->root_icon = NULL;
        }
        if (path_bar->home_icon) {
                g_object_unref (path_bar->home_icon);
                path_bar->home_icon = NULL;
        }
        if (path_bar->desktop_icon) {
                g_object_unref (path_bar->desktop_icon);
                path_bar->desktop_icon = NULL;
        }


        for (list = path_bar->button_list; list; list = list->next) {
                ButtonData *button_data;
                gboolean current_dir;

                button_data = BUTTON_DATA (list->data);
		if (button_data->type != NORMAL_BUTTON) {
                	current_dir = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button));
                	nautilus_path_bar_update_button_appearance (path_bar, button_data, current_dir);
		}

        }
}

static void
change_icon_theme (NautilusPathBar *path_bar)
{
	path_bar->icon_size = NAUTILUS_PATH_BAR_ICON_SIZE;
        reload_icons (path_bar);
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject    *object,
		    GParamSpec *pspec,
		    NautilusPathBar *path_bar)
{
        const char *name;

        name = g_param_spec_get_name (pspec);

      	if (! strcmp (name, "gtk-icon-theme-name") || ! strcmp (name, "gtk-icon-sizes")) {
	      change_icon_theme (path_bar);	
	}
}

static void
nautilus_path_bar_check_icon_theme (NautilusPathBar *path_bar)
{
        GtkSettings *settings;

        if (path_bar->settings_signal_id) {
                return;
	}

        settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_bar)));
        path_bar->settings_signal_id = g_signal_connect (settings, "notify", G_CALLBACK (settings_notify_cb), path_bar);

        change_icon_theme (path_bar);
}

/* Public functions and their helpers */
static void
nautilus_path_bar_clear_buttons (NautilusPathBar *path_bar)
{
        while (path_bar->button_list != NULL) {
                gtk_container_remove (GTK_CONTAINER (path_bar), BUTTON_DATA (path_bar->button_list->data)->button);
        }
        path_bar->first_scrolled_button = NULL;
	path_bar->fake_root = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
		   gpointer   data)
{
        ButtonData *button_data;
        NautilusPathBar *path_bar;
        GList *button_list;
        gboolean child_is_hidden;

        button_data = BUTTON_DATA (data);
        if (button_data->ignore_changes) {
                return;
	}

        path_bar = NAUTILUS_PATH_BAR (button->parent);

        button_list = g_list_find (path_bar->button_list, button_data);
        g_assert (button_list != NULL);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

        if (button_list->prev) {
                ButtonData *child_data;

                child_data = BUTTON_DATA (button_list->prev->data);
                child_is_hidden = child_data->file_is_hidden;
        } else {
                child_is_hidden = FALSE;
	}
        g_signal_emit (path_bar, path_bar_signals [PATH_CLICKED], 0, button_data->path);
}

static char *
get_icon_name_for_file_path (const char *file_path)
{
	NautilusFile *file;
	char 	     *icon_name;

	file = nautilus_file_get (file_path);
	if (!file) {
		return g_strdup (DEFAULT_ICON);
	}
	icon_name = NULL;
	if (nautilus_icon_factory_is_icon_ready_for_file (file)) {
		icon_name = nautilus_icon_factory_get_icon_for_file (file, FALSE);
	}
	if (!icon_name) {
		icon_name = g_strdup (DEFAULT_ICON);
	}
	nautilus_file_unref (file);
	return icon_name;
}

static GdkPixbuf *
get_button_image (NautilusPathBar *path_bar,
		  ButtonType  button_type)
{
	char *icon_name;

	switch (button_type)
        {
		case ROOT_BUTTON:
			if (path_bar->root_icon != NULL) {
				return path_bar->root_icon;
                       	}

			icon_name = get_icon_name_for_file_path (path_bar->root_path);
			if (strcmp (icon_name, DEFAULT_ICON) == 0) {
			        path_bar->root_icon = nautilus_icon_factory_get_pixbuf_from_name (DEFAULT_FILESYSTEM_ICON,
											  NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											  TRUE, NULL);
			} else {
				path_bar->root_icon = nautilus_icon_factory_get_pixbuf_from_name (icon_name,
											  NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											  TRUE, NULL);
			}

			g_free (icon_name);
			return path_bar->root_icon;

		case HOME_BUTTON:
		      	if (path_bar->home_icon != NULL) {
		      		return path_bar->home_icon;
			}

			if (!desktop_is_home) {
				icon_name = get_icon_name_for_file_path (path_bar->home_path);
				if (strcmp (icon_name, DEFAULT_ICON) == 0) {
			        	path_bar->home_icon = nautilus_icon_factory_get_pixbuf_from_name (DEFAULT_HOME_ICON,
											  NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											  TRUE, NULL);
				} else {
					path_bar->home_icon = nautilus_icon_factory_get_pixbuf_from_name (icon_name,
											  NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											  TRUE, NULL);
				}

				g_free (icon_name);
			} else {
				path_bar->home_icon = nautilus_icon_factory_get_pixbuf_from_name (DEFAULT_DESKTOP_ICON,
											     NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											     TRUE, NULL);
			}
                       	return path_bar->home_icon;

                case DESKTOP_BUTTON:
                      	if (path_bar->desktop_icon != NULL) {
				return path_bar->desktop_icon;
			}
			icon_name = get_icon_name_for_file_path (path_bar->desktop_path);
			if (strcmp (icon_name, DEFAULT_ICON) == 0) {
		      		path_bar->desktop_icon = nautilus_icon_factory_get_pixbuf_from_name (DEFAULT_DESKTOP_ICON,
											     NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											     TRUE, NULL);
			} else {
				path_bar->desktop_icon = nautilus_icon_factory_get_pixbuf_from_name (icon_name,
											  NULL, NAUTILUS_PATH_BAR_ICON_SIZE,
											  TRUE, NULL);
			}

			g_free (icon_name);
      			return path_bar->desktop_icon;

	    	default:
                       return NULL;
        }
  
       	return NULL;
}

static void
button_data_free (ButtonData *button_data)
{
        g_free (button_data->path);
        g_free (button_data->dir_name);
	if (button_data->custom_icon_name) {
		g_free (button_data->custom_icon_name);
	}
        g_free (button_data);
}

static const char *
get_dir_name (ButtonData *button_data)
{
        if (button_data->type == DESKTOP_BUTTON || (button_data->type == HOME_BUTTON && desktop_is_home)) {
		return _("Desktop");
	} else {
		return button_data->dir_name;
	}
}

/* We always want to request the same size for the label, whether
 * or not the contents are bold
 */
static void
label_size_request_cb (GtkWidget       *widget,
		       GtkRequisition  *requisition,
		       ButtonData      *button_data)
{
        const gchar *dir_name = get_dir_name (button_data);
        PangoLayout *layout;
        gint bold_width, bold_height;
        gchar *markup;
	
	layout = gtk_widget_create_pango_layout (button_data->label, dir_name);
        pango_layout_get_pixel_size (layout, &requisition->width, &requisition->height);
  
        markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);
        pango_layout_set_markup (layout, markup, -1);
        g_free (markup);

        pango_layout_get_pixel_size (layout, &bold_width, &bold_height);
        requisition->width = MAX (requisition->width, bold_width);
        requisition->height = MAX (requisition->height, bold_height);
  
        g_object_unref (layout);
}

static void
nautilus_path_bar_update_button_appearance (NautilusPathBar *path_bar,
				            ButtonData *button_data,
				            gboolean    current_dir)
{
        const gchar *dir_name = get_dir_name (button_data);
	

        if (button_data->label != NULL) {
                if (current_dir) {
			char *markup;

	  		markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);
	  		gtk_label_set_markup (GTK_LABEL (button_data->label), markup);
	  		g_free (markup);
		} else {
			gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);
		}
        }

        if (button_data->image != NULL) {
		if (button_data->type == VOLUME_BUTTON || (button_data->type == NORMAL_BUTTON && button_data->is_base_dir) ) {
	
			/* set custom icon for roots */
			if (button_data->custom_icon_name) {
				gtk_image_set_from_icon_name (GTK_IMAGE (button_data->image), button_data->custom_icon_name, GTK_ICON_SIZE_MENU);  
			}
		} else {
	                GdkPixbuf *pixbuf;
	                pixbuf = get_button_image (path_bar, button_data->type);
       	        	gtk_image_set_from_pixbuf (GTK_IMAGE (button_data->image), pixbuf);
		}
        }

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)) != current_dir) {
                button_data->ignore_changes = TRUE;
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_data->button), current_dir);
                button_data->ignore_changes = FALSE;
        }
}

static gboolean
is_file_path_equal (const char *file_path_1, const char *file_path_2)
{
	GnomeVFSURI *vfs_uri_1, *vfs_uri_2;
	gboolean result;

	vfs_uri_1 = gnome_vfs_uri_new (file_path_1);
	if (vfs_uri_1 == NULL) {
		return FALSE;
	}

	vfs_uri_2 = gnome_vfs_uri_new (file_path_2);
	if (vfs_uri_2 == NULL) {
		gnome_vfs_uri_unref (vfs_uri_1);
		return FALSE;
	}

	result = gnome_vfs_uri_equal (vfs_uri_1, vfs_uri_2);

	gnome_vfs_uri_unref (vfs_uri_1);
	gnome_vfs_uri_unref (vfs_uri_2);
	return result;
}

static gboolean
is_file_path_mounted_volume (const char *file_path, ButtonData *button_data)
{
	GnomeVFSVolumeMonitor *volume_monitor;
	GList 		      *volumes, *l;
	GnomeVFSVolume 	      *volume;
	gboolean	       result;
	char		      *mount_uri;

	result = FALSE;
	volume_monitor = gnome_vfs_get_volume_monitor ();
	volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		if (result || !gnome_vfs_volume_is_user_visible (volume)) {
			gnome_vfs_volume_unref (volume);
			continue;
		}
		mount_uri = gnome_vfs_volume_get_activation_uri (volume);
		if (is_file_path_equal (file_path, mount_uri)) {
			result = TRUE;
			/* set volume specific details in button_data */
			if (button_data) {
				button_data->custom_icon_name = gnome_vfs_volume_get_icon (volume);
				if (!button_data->custom_icon_name) {
					button_data->custom_icon_name = g_strdup (DEFAULT_ICON);
				}
				button_data->path = g_strdup (mount_uri);
				button_data->dir_name = gnome_vfs_volume_get_display_name (volume);
			}
			gnome_vfs_volume_unref (volume);
			g_free (mount_uri);
			continue;
		}
		gnome_vfs_volume_unref (volume);
		g_free (mount_uri);
	}
	g_list_free (volumes);
	return result;
}

static ButtonType
find_button_type (NautilusPathBar  *path_bar,
		  const char       *path,
		  ButtonData       *button_data)
{


        if (path_bar->root_path != NULL && is_file_path_equal (path, path_bar->root_path)) {
                return ROOT_BUTTON;
	}
        if (path_bar->home_path != NULL && is_file_path_equal (path, path_bar->home_path)) {
	       	return HOME_BUTTON;
	}
        if (path_bar->desktop_path != NULL && is_file_path_equal (path, path_bar->desktop_path)) {
		if (!desktop_is_home) {
                	return DESKTOP_BUTTON;
		} else {
			return NORMAL_BUTTON;
		}
	}
	if (is_file_path_mounted_volume (path, button_data)) {
		return VOLUME_BUTTON;
	}

 	return NORMAL_BUTTON;
}

static void
button_drag_data_get_cb (GtkWidget          *widget,
			 GdkDragContext     *context,
			 GtkSelectionData   *selection_data,
			 guint               info,
			 guint               time_,
			 gpointer            data)
{
        ButtonData *button_data;
        char *uri_list;

        button_data = data;
        uri_list = g_strconcat (button_data->path, "\r\n", NULL);
        gtk_selection_data_set (selection_data,
			  	selection_data->target,
			  	8,
			  	uri_list,
			  	strlen (uri_list));
        g_free (uri_list);
}

static ButtonData *
make_directory_button (NautilusPathBar  *path_bar,
		       const char       *dir_name,
		       const char       *path,
		       gboolean          current_dir,
		       gboolean          base_dir,	
		       gboolean          file_is_hidden)
{
        const GtkTargetEntry targets[] = {
                { "text/uri-list", 0, 0 }
        };

        GtkWidget *child;
        GtkWidget *label_alignment;
        ButtonData *button_data;

	child = NULL;
	label_alignment = NULL;

        file_is_hidden = !! file_is_hidden;
        /* Is it a special button? */
        button_data = g_new0 (ButtonData, 1);

        button_data->type = find_button_type (path_bar, path, button_data);
        button_data->button = gtk_toggle_button_new ();
	
        switch (button_data->type) {
                case ROOT_BUTTON:
                        button_data->image = gtk_image_new ();
                        child = button_data->image;
                        button_data->label = NULL;
                        break;
                case HOME_BUTTON:
                case DESKTOP_BUTTON:
		case VOLUME_BUTTON:
                        button_data->image = gtk_image_new ();
                        button_data->label = gtk_label_new (NULL);
                        label_alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
                        gtk_container_add (GTK_CONTAINER (label_alignment), button_data->label);
                        child = gtk_hbox_new (FALSE, 2);
                        gtk_box_pack_start (GTK_BOX (child), button_data->image, FALSE, FALSE, 0);
                        gtk_box_pack_start (GTK_BOX (child), label_alignment, FALSE, FALSE, 0);
                        break;
		case NORMAL_BUTTON:
    		default:
			if (base_dir) {
	                        button_data->image = gtk_image_new ();
        	                button_data->label = gtk_label_new (NULL);
        	                label_alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
        	                gtk_container_add (GTK_CONTAINER (label_alignment), button_data->label);
        	                child = gtk_hbox_new (FALSE, 2);
        	                gtk_box_pack_start (GTK_BOX (child), button_data->image, FALSE, FALSE, 0);
        	                gtk_box_pack_start (GTK_BOX (child), label_alignment, FALSE, FALSE, 0);
				button_data->is_base_dir = TRUE;
				button_data->custom_icon_name = get_icon_name_for_file_path (path);
			} else {
				button_data->is_base_dir = FALSE;
	      			button_data->label = gtk_label_new (NULL);
      				label_alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      				gtk_container_add (GTK_CONTAINER (label_alignment), button_data->label);
      				child = label_alignment;
      				button_data->image = NULL;
			}
        }

  	/* label_alignment is created because we can't override size-request
   	* on label itself and still have the contents of the label centered
   	* properly in the label's requisition
   	*/

        if (label_alignment) {
                g_signal_connect (label_alignment, "size-request",
			      	  G_CALLBACK (label_size_request_cb), button_data);
	}
	
	/* do not set these for volumes */
	if (button_data->type != VOLUME_BUTTON) {
		button_data->dir_name = g_strdup (dir_name);
        	button_data->path = g_strdup (path);
	}

        button_data->file_is_hidden = file_is_hidden;
			  
        gtk_container_add (GTK_CONTAINER (button_data->button), child);
        gtk_widget_show_all (button_data->button);

        nautilus_path_bar_update_button_appearance (path_bar, button_data, current_dir);

        g_signal_connect (button_data->button, "clicked", G_CALLBACK (button_clicked_cb), button_data);
        g_object_weak_ref (G_OBJECT (button_data->button), (GWeakNotify) button_data_free, button_data);

        gtk_drag_source_set (button_data->button,
		       	     GDK_BUTTON1_MASK,
		       	     targets,
		             G_N_ELEMENTS (targets),
		             GDK_ACTION_COPY);
        g_signal_connect (button_data->button, "drag-data-get",G_CALLBACK (button_drag_data_get_cb), button_data);

        return button_data;
}

static gboolean
nautilus_path_bar_check_parent_path (NautilusPathBar *path_bar,
				     const char  *file_path)
{
        GList *list;
        GList *current_path;
	gboolean need_new_fake_root;

	current_path = NULL;
	need_new_fake_root = FALSE;

        for (list = path_bar->button_list; list; list = list->next) {
                ButtonData *button_data;

                button_data = list->data;
                if (is_file_path_equal (file_path, button_data->path)) {
			current_path = list;
		  	break;
		}
		if (list == path_bar->fake_root) {
			need_new_fake_root = TRUE;
		}
        }

        if (current_path) {

		if (need_new_fake_root) {
			path_bar->fake_root = NULL;
	  		for (list = current_path; list; list = list->next) {
	      			ButtonData *button_data;

	      			button_data = list->data;
	      			if (BUTTON_IS_FAKE_ROOT (button_data)) {
		  			path_bar->fake_root = list;
		  			break;
				}
	    		}
		}

                for (list = path_bar->button_list; list; list = list->next) {

	  		nautilus_path_bar_update_button_appearance (path_bar,
					  			    BUTTON_DATA (list->data),
						                    (list == current_path) ? TRUE : FALSE);
		}

                if (!gtk_widget_get_child_visible (BUTTON_DATA (current_path->data)->button)) {
			path_bar->first_scrolled_button = current_path;
	  		gtk_widget_queue_resize (GTK_WIDGET (path_bar));
		}
                return TRUE;
        }
        return FALSE;
}

static char *
get_parent_directory (const char *file_path) 
{
	GnomeVFSURI *vfs_uri, *parent_vfs_uri;
	char *parent_directory;


	vfs_uri = gnome_vfs_uri_new (file_path);
	if (vfs_uri == NULL) {
		return NULL;
	}

	parent_vfs_uri = gnome_vfs_uri_get_parent (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	if (parent_vfs_uri == NULL) {
		return NULL;
	}

	parent_directory = gnome_vfs_uri_to_string (parent_vfs_uri,
		  				    GNOME_VFS_URI_HIDE_NONE);
	gnome_vfs_uri_unref (parent_vfs_uri);
	return parent_directory;

}


static char *
get_display_name_for_folder (const char *file_path)
{
	GnomeVFSURI *vfs_uri;
	NautilusFile *file;
	char *name;

	vfs_uri = gnome_vfs_uri_new (file_path);
 	if (vfs_uri == NULL) {
		file = nautilus_file_get (file_path);
		name = nautilus_file_get_display_name (file);
		nautilus_file_unref (file);
		return name;
	}
	name = nautilus_get_uri_shortname_for_display (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);	
	return name;
}


static gboolean
nautilus_path_bar_update_path (NautilusPathBar *path_bar, const char *file_path)
{
        char *path, *parent_path, *name;
        gboolean first_directory, last_directory;
        gboolean result;
        GList *new_buttons, *l, *fake_root;
        ButtonData *button_data;

        g_return_val_if_fail (NAUTILUS_IS_PATH_BAR (path_bar), FALSE);
        g_return_val_if_fail (file_path != NULL, FALSE);

        name = NULL;
	fake_root = NULL;
        result = TRUE;
	parent_path = NULL;
	first_directory = TRUE;
	last_directory = FALSE;
	new_buttons = NULL;

        path = g_strdup (file_path);

        gtk_widget_push_composite_child ();

        while (path != NULL) {

                parent_path = get_parent_directory (path);
                name = get_display_name_for_folder (path);	
		last_directory = !parent_path;
                button_data = make_directory_button (path_bar, name, path, first_directory, last_directory, FALSE);
                g_free (path);
		g_free (name);

                new_buttons = g_list_prepend (new_buttons, button_data);

		if (BUTTON_IS_FAKE_ROOT (button_data)) {
			fake_root = new_buttons;
		}
		
                path = parent_path;
                first_directory = FALSE;
        }

        nautilus_path_bar_clear_buttons (path_bar);
       	path_bar->button_list = g_list_reverse (new_buttons);
	path_bar->fake_root = fake_root;

       	for (l = path_bar->button_list; l; l = l->next) {
		GtkWidget *button;
		button = BUTTON_DATA (l->data)->button;
		gtk_container_add (GTK_CONTAINER (path_bar), button);
	}	

        gtk_widget_pop_composite_child ();

        return result;
}

gboolean
nautilus_path_bar_set_path (NautilusPathBar *path_bar, const char *file_path)
{
        g_return_val_if_fail (NAUTILUS_IS_PATH_BAR (path_bar), FALSE);
        g_return_val_if_fail (file_path != NULL, FALSE);
	
        /* Check whether the new path is already present in the pathbar as buttons.
         * This could be a parent directory or a previous selected subdirectory. */
        if (nautilus_path_bar_check_parent_path (path_bar, file_path)) {
                return TRUE;
	}

	return nautilus_path_bar_update_path (path_bar, file_path);
}



/**
 * _nautilus_path_bar_up:
 * @path_bar: a #NautilusPathBar
 * 
 * If the selected button in the pathbar is not the furthest button "up" (in the
 * root direction), act as if the user clicked on the next button up.
 **/
void
nautilus_path_bar_up (NautilusPathBar *path_bar)
{
        GList *l;

        for (l = path_bar->button_list; l; l = l->next) {
                GtkWidget *button = BUTTON_DATA (l->data)->button;
                if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
			if (l->next) {
			        GtkWidget *next_button = BUTTON_DATA (l->next->data)->button;
	      			button_clicked_cb (next_button, l->next->data);
			}
	  		break;
		}
        }
}

/**
 * _nautilus_path_bar_down:
 * @path_bar: a #NautilusPathBar
 * 
 * If the selected button in the pathbar is not the furthest button "down" (in the
 * leaf direction), act as if the user clicked on the next button down.
 **/
void
nautilus_path_bar_down (NautilusPathBar *path_bar)
{
        GList *l;

        for (l = path_bar->button_list; l; l = l->next) {
                GtkWidget *button = BUTTON_DATA (l->data)->button;
                if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
	  		if (l->prev) {
		      		GtkWidget *prev_button = BUTTON_DATA (l->prev->data)->button;
	      			button_clicked_cb (prev_button, l->prev->data);
	    		}
	  		break;
		}
        }
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus News Viewer
 *
 *  Copyright (C) 2001 Eazel, Inc.
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

/* This is the News sidebar panel, which displays current news headlines from
 * a variety of web sites, by fetching and displaying RSS files
 */

#include <config.h>
#include <time.h>

#include "nautilus-cell-renderer-news.h"

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtknotebook.h>

#include <bonobo/bonobo-property-bag.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#include <libxml/parser.h>

#include <libgnomevfs/gnome-vfs-utils.h>

#include <eel/eel-background.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>

#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>

#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus/nautilus-view-standard-main.h>

/* property bag getting and setting routines */
enum {
	TAB_IMAGE,
	CLOSE_NOTIFY,
};

typedef enum {
        PAGE_MAIN,
        PAGE_CONFIGURE,
        PAGE_ADD_SITE
} NewsPageNum;

enum {
        MAIN_PAGE_DISPLAY,
        MAIN_PAGE_EMPTY
};

enum {
        REMOVE_COL_NAME,
        LAST_REMOVE_COL
};

enum {
        NEWS_COL_DATA,
        NEWS_COL_WIDTH,
        LAST_NEWS_COL
};

typedef struct _RSSNodeData    RSSNodeData;
typedef struct _RSSChannelData RSSChannelData;
typedef struct _RSSItemData    RSSItemData;

/* data structure for the news view */
typedef struct {
	NautilusView *view;
	BonoboPropertyBag *property_bag;
	
	GList *channel_list;

	GdkPixbuf *bullet;
        GdkPixbuf *prelit_bullet;
	GdkPixbuf *changed_bullet;
	GdkPixbuf *prelight_changed_bullet;

        GtkWidget *main_container;
	GtkWidget *main_box;
        GtkWidget *news_notebook;
	GtkWidget *news_display;
	GtkWidget *news_display_scrolled_window;
	GtkWidget *empty_message;

        GtkTreeModel *news_model;
	
	GtkWidget *configure_box;
	GtkWidget *checkbox_list;
	
	GtkWidget *edit_site_box;
	GtkWidget *item_name_field;
	GtkWidget *item_location_field;
	
	GtkWidget *remove_site_list;
	GtkWidget *remove_button;
	
	int max_item_count;
	uint update_interval;
	int update_timeout;
	
	gboolean news_changed;
	gboolean opened;

        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        NewsPageNum current_page;

	guint timer_task;

        GHashTable *item_uris;

        RSSItemData *current_item;

        int wrap_idle_handle;

        int last_width;
} News;

struct _RSSNodeData {
        char *title;
        char *markup;
        char *uri;

        GtkTreePath *path;
	
	GdkPixbuf *pixbuf;
	GdkPixbuf *prelight_pixbuf;
};

/* per channel structure for rss channel */
struct _RSSChannelData {
        RSSNodeData node_data;

	char *name;
	char *uri;
	News *owner;
	
        gboolean is_showing;
        gboolean is_open;
        gboolean initial_load_flag;
        gboolean channel_changed;
        gboolean update_in_progress;

	GList *items;
	
	EelReadFileHandle *load_file_handle;
	EelPixbufLoadHandle *load_image_handle;
	
	GtkWidget *checkbox;
        
	time_t last_update;	
};

/* per item structure for rss items */
struct _RSSItemData {
	RSSNodeData node_data;

        RSSChannelData *owner;
        gboolean new_item;
};

#define EMPTY_MESSAGE_MARGIN 12
#define EXPANDER_EXTRA_PADDING 4

static char *news_get_indicator_image   (News *news_data);
static void nautilus_news_free_channel_list (News *news_data);
static gboolean nautilus_news_save_channel_state (News* news_data);

static char* get_xml_path (const char *file_name, gboolean force_local);
static int check_for_updates (gpointer callback_data);
static RSSChannelData* get_channel_from_name (News *news_data, const char *channel_name);
static void nautilus_news_clear_changed_flags (News* news_data);
static void clear_channel_changed_flags (RSSChannelData *channel_data);
static void set_views_for_mode (News *news);
static void max_items_changed (gpointer user_data);
static void update_interval_changed (gpointer user_data);

static void add_channel_entry (News *news_data, const char *channel_name,
			       int index, gboolean is_showing);
static void update_channels (News *news_data);
static void update_items (RSSChannelData *channel_data, GList *old_items);

static RSSChannelData*
nautilus_news_make_new_channel (News *news_data,
				const char *name,
				const char* channel_uri,
				gboolean is_open,
				gboolean is_showing);


static void
update_node (News *news, RSSNodeData *node)
{
        GtkTreeIter iter;
        
        if (node->path) {
                gtk_tree_model_get_iter (news->news_model, &iter, node->path);
                gtk_tree_model_row_changed (news->news_model,
                                            node->path, &iter);
        }
}

static char *
get_channel_markup (RSSChannelData *channel_data)
{
        const char *title;
        char *markup;
        char *escaped;

        title = channel_data->node_data.title ? channel_data->node_data.title : "";
        escaped = g_markup_escape_text (title, -1);
        markup = g_strdup_printf ("<span weight=\"bold\" size=\"large\">%s</span>", escaped);
        g_free (escaped);
        return markup;
}

static char *
get_item_markup (RSSItemData *item_data)
{
        char *title;
        char *markup;
        char *escaped;
        title = item_data->node_data.title ? item_data->node_data.title : "";
        escaped = g_markup_escape_text (title, strlen (title));
        markup = g_strdup_printf ("<u>%s</u>", escaped);
        g_free (escaped);
        return markup;
}

/* property bag property access routines */
static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
        char *indicator_image;
        News *news;

	news = (News *) callback_data;
	
	switch (arg_id) {
        case TAB_IMAGE:	{
                /* if there is a note, return the name of the indicator image,
                   otherwise, return NULL */
                indicator_image = news_get_indicator_image (news);
                BONOBO_ARG_SET_STRING (arg, indicator_image);					
                g_free (indicator_image);
                break;
        }
        case CLOSE_NOTIFY: {
		/* this shouldn't be read, but return it anyway */
		BONOBO_ARG_SET_BOOLEAN (arg, news->opened);
		break;
	}
	
        default:
                g_warning ("Unhandled arg %d", arg_id);
                break;
	}
}

static void
set_bonobo_properties (BonoboPropertyBag *bag,
			const BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
       News *news;

	news = (News *) callback_data;
	
	switch (arg_id) {
        case TAB_IMAGE:	{
		g_warning ("cant set tab image in news view");
                break;
        }
 
 	/* when closed, clear the changed flags; also, exit configure mode */
        case CLOSE_NOTIFY: {
		if (BONOBO_ARG_GET_BOOLEAN (arg)) {
			news->opened = FALSE;
			nautilus_news_clear_changed_flags (news);
                        news->current_page = PAGE_MAIN;
			set_views_for_mode (news);
		} else {
			news->opened = TRUE;
		}
		break;
	}
	
        default:
                g_warning ("Unhandled arg %d", arg_id);
                break;
	}
}

/* do_destroy is invoked when the nautilus view is destroyed to deallocate the resources used
 * by the news panel
 */
static void
do_destroy (GtkObject *obj, News *news)
{
        nautilus_news_save_channel_state (news);
        
	if (news->timer_task != 0) {
		gtk_timeout_remove (news->timer_task);
		news->timer_task = 0;
	}

	if (news->update_timeout > 0) {
		gtk_timeout_remove (news->update_timeout);
		news->update_timeout = -1;
	}	
	
	if (news->bullet != NULL) {
		g_object_unref (news->bullet);
	}	

	if (news->prelit_bullet != NULL) {
		g_object_unref (news->prelit_bullet);
	}	
	
	if (news->changed_bullet != NULL) {
		g_object_unref (news->changed_bullet);
	}	

	if (news->prelight_changed_bullet != NULL) {
		g_object_unref (news->prelight_changed_bullet);
	}

        g_hash_table_destroy (news->item_uris);
	
	/* free all the channel data */
	nautilus_news_free_channel_list (news);

	/* free the property bag */
        bonobo_object_unref (news->property_bag);
	
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS,
                                         max_items_changed,
                                         news);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL,
                                         update_interval_changed,
                                         news);
	
        g_free (news);
}

/* utility routine to tell Nautilus to navigate to the passed-in uri */
static void
go_to_uri (News* news_data, const char* uri)
{
	if (uri != NULL) {
		nautilus_view_open_location_in_this_window (news_data->view, uri);
	}
}

static void
set_row_widths (News *news)
{
        GtkTreeIter iter;
        GList *channel_item;
        GList *news_item;
        RSSNodeData *node_data;
        int cell_width;
        int expander_size;
        int horizontal_separator;
        
        /* Set the width attribute on all the rows by getting the width
         * of the widget and subtracting the depth * expander size */
        
        gtk_widget_style_get (news->news_display,
                              "expander_size", &expander_size, 
                              "horizontal_separator", &horizontal_separator,
                              NULL);
        expander_size += EXPANDER_EXTRA_PADDING;
        
        channel_item = news->channel_list;
	while (channel_item != NULL) {	
		node_data = (RSSNodeData*) channel_item->data;
                channel_item = channel_item->next;

                if (node_data->path == NULL) {
                        continue;
                }
                cell_width = news->news_display->allocation.width;
                cell_width -= ((gtk_tree_path_get_depth (node_data->path) * expander_size) + horizontal_separator);
                if (cell_width > 0) {
                        gtk_tree_model_get_iter (news->news_model,
                                                 &iter, node_data->path);
                        gtk_tree_store_set (GTK_TREE_STORE (news->news_model),
                                            &iter, 
                                            NEWS_COL_WIDTH, cell_width, 
                                            -1);
                }
                
         	news_item = ((RSSChannelData*)node_data)->items;
                while (news_item != NULL) {
                        node_data = (RSSNodeData*)news_item->data;
                        news_item = news_item->next;
                        if (node_data->path == NULL) {
                                continue;
                        }

                        cell_width = news->news_display->allocation.width;
                        cell_width -= (gtk_tree_path_get_depth (node_data->path) * expander_size);
                        if (cell_width > 0) {
                                gtk_tree_model_get_iter (news->news_model, 
                                                         &iter, node_data->path);
                                
                                gtk_tree_store_set (GTK_TREE_STORE (news->news_model),
                                                    &iter, 
                                                    NEWS_COL_WIDTH, cell_width, 
                                                    -1);
                        }
                }
        }
}

static gboolean
wrap_idle (gpointer data)
{
        News *news = (News*)data;
        
        set_row_widths (news);
        
        news->wrap_idle_handle = -1;
        return FALSE;
}

static void
nautilus_news_size_allocate (GtkWidget *widget, GtkAllocation *alloc,
                             News *news_data)
{
        if (news_data->last_width != alloc->width) {
                if (news_data->wrap_idle_handle == -1) {
                        news_data->wrap_idle_handle = gtk_idle_add (wrap_idle,
                                                                    news_data);
                }
                news_data->last_width = alloc->width;
        }
}

static RSSNodeData *
node_data_for_path (News *news, GtkTreePath *path)
{
        GtkTreeIter iter;
        RSSNodeData *node_data = NULL;

        if (gtk_tree_model_get_iter (news->news_model, &iter, path)) {
                gtk_tree_model_get (news->news_model, &iter, 
                                    NEWS_COL_DATA,
                                    &node_data,
                                    -1);
        }
        
        return node_data;
}

static void
nautilus_news_activate_path (News *news, 
                             GtkTreePath *path)
{
        RSSNodeData *node_data;

        node_data = node_data_for_path (news, path);     
        if (node_data) {
                go_to_uri (news, node_data->uri);
        }
}

static void
nautilus_news_row_activated (GtkTreeView *tree_view, 
                             GtkTreePath *path, 
                             GtkTreeViewColumn *column,
                             gpointer data)
{
        News *news = (News*)data;
        
        nautilus_news_activate_path (news, path);        
}

/* handle the news display hit-testing */
static gint
nautilus_news_button_release_event (GtkWidget *widget, GdkEventButton *event, News *news_data )
{
        GtkTreePath *path;

	/* we only respond to the first button */
	if (event->button != 1) {
		return FALSE;
	}
        
        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (news_data->news_display),
                                           event->x, event->y, 
                                           &path, NULL, NULL, NULL)) {
                nautilus_news_activate_path (news_data, path);
                gtk_tree_path_free (path);
        }
        
        return FALSE;
}
 
static void
nautilus_news_set_title (RSSChannelData *channel_data, const char *title)
{
        if (channel_data->node_data.title) {
                g_free (channel_data->node_data.title);
                channel_data->node_data.title = NULL;
        }

        if (channel_data->node_data.markup) {
                g_free (channel_data->node_data.markup);
                channel_data->node_data.markup = NULL;
        }
        
        channel_data->node_data.title = g_strdup (title ? title : "");
        
        if (channel_data->node_data.pixbuf) {
                channel_data->node_data.markup = eel_strdup_strftime (_("%I:%M %p"), localtime (&channel_data->last_update));
        } else {
                channel_data->node_data.markup = get_channel_markup (channel_data);
        }
        
        update_node (channel_data->owner, (RSSNodeData*)channel_data);
}

static void
free_rss_data_item (RSSItemData *item)
{
	g_free (item->node_data.title);
	g_free (item->node_data.uri);
        g_free (item->node_data.markup);

	g_free (item);
}

static void
free_rss_channel_items (RSSChannelData *channel_data)
{
	eel_g_list_free_deep_custom (channel_data->items, (GFunc) free_rss_data_item, NULL);
	channel_data->items = NULL;
}

/* this frees a single channel object */
static void
free_channel (RSSChannelData *channel_data)
{
	g_free (channel_data->name);
	g_free (channel_data->uri);

        if (channel_data->node_data.uri) {
                g_free (channel_data->node_data.uri);
        }

        if (channel_data->node_data.title != NULL) {
                g_free (channel_data->node_data.title);
        }

        if (channel_data->node_data.markup != NULL) {
                g_free (channel_data->node_data.markup);
        }
	
	if (channel_data->node_data.pixbuf != NULL) {
		g_object_unref (channel_data->node_data.pixbuf);
	}
	if (channel_data->node_data.prelight_pixbuf != NULL) {
		g_object_unref (channel_data->node_data.prelight_pixbuf);
	}

	if (channel_data->load_file_handle != NULL) {
		eel_read_file_cancel (channel_data->load_file_handle);
	}
	
	if (channel_data->load_image_handle != NULL) {
		eel_cancel_gdk_pixbuf_load (channel_data->load_image_handle);
	}

	free_rss_channel_items (channel_data);

	g_free (channel_data);
}

/* free the entire channel list */
static void
nautilus_news_free_channel_list (News *news_data)
{
	GList *current_item;
	
	current_item = news_data->channel_list;
	while (current_item != NULL) {
		free_channel ((RSSChannelData*) current_item->data);
		current_item = current_item->next;
	}
	
	g_list_free (news_data->channel_list);
	news_data->channel_list = NULL;
}

/* utilities to deal with the changed flags */
static void
nautilus_news_set_news_changed (News *news_data, gboolean changed_flag)
{
	char *tab_image;
	BonoboArg *tab_image_arg;
	
	if (news_data->news_changed != changed_flag) {
		news_data->news_changed = changed_flag;

		tab_image = news_get_indicator_image (news_data);	
		
		tab_image_arg = bonobo_arg_new (BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (tab_image_arg, tab_image);			
                bonobo_event_source_notify_listeners_full (news_data->property_bag->es, 
                                                           "Bonobo/Property",
                                                           "change",
                                                           "tab_image",
                                                           tab_image_arg,
                                                           NULL);

		bonobo_arg_release (tab_image_arg);
		g_free (tab_image);
	}
}

static void
clear_channel_changed_flags (RSSChannelData *channel_data)
{
	GList *current_item;
	RSSItemData *item_data;
        
	current_item = channel_data->items;
	while (current_item != NULL) {
		item_data = (RSSItemData*) current_item->data;
		item_data->new_item = FALSE;

                if (item_data->node_data.path) {
                        update_node (channel_data->owner,
                                     (RSSNodeData*)item_data);
                }
                
		current_item = current_item->next;
	}
	channel_data->channel_changed = FALSE;
        update_node (channel_data->owner, (RSSNodeData*)channel_data);
}

static void
nautilus_news_clear_changed_flags (News* news_data)
{
	GList *current_channel;
	RSSChannelData *channel_data;
	
	current_channel = news_data->channel_list;
	while (current_channel != NULL) {
		channel_data = (RSSChannelData*) current_channel->data;
		clear_channel_changed_flags (channel_data);
		current_channel = current_channel->next;
	}
	nautilus_news_set_news_changed (news_data, FALSE);
}

/* utility to express boolean as a string */
static char *
bool_to_text (gboolean value)
{
	return value ? "true" : "false";
}

/* build a channels xml file from the current channels state */
static xmlDocPtr
nautilus_news_make_channel_document (News* news_data)
{
	xmlDoc  *channel_doc;
	xmlNode *root_node;
	xmlNode *channel_node;
	RSSChannelData *channel_data;
	GList *next_channel;
	gboolean is_open;

	channel_doc = xmlNewDoc ("1.0");
	
	/* add the root node to the channel document */
	root_node = xmlNewDocNode (channel_doc, NULL, "rss_news_channels", NULL);
	xmlDocSetRootElement (channel_doc, root_node);

	/* loop through the channels, adding a node for each channel */
	next_channel = news_data->channel_list;
	while (next_channel != NULL) {
		channel_node = xmlNewChild (root_node, NULL, "rss_channel", NULL);
		channel_data = (RSSChannelData*) next_channel->data;
		
		xmlSetProp (channel_node, "name", channel_data->name);
		xmlSetProp (channel_node, "uri", channel_data->uri);
		xmlSetProp (channel_node, "show", bool_to_text (channel_data->is_showing));
                
                is_open = channel_data->is_open;
		xmlSetProp (channel_node, "open", bool_to_text (channel_data->is_open));

		next_channel = next_channel->next;
	}
	return channel_doc;
}

/* save the current channel state to disk */
static gboolean
nautilus_news_save_channel_state (News* news_data)
{
	int  result;
	char *path;
	xmlDoc *channel_doc;
	
	path = get_xml_path ("news_channels.xml", TRUE);
	channel_doc = nautilus_news_make_channel_document (news_data);
	
	result = xmlSaveFile (path, channel_doc);
	
	g_free (path);
	xmlFreeDoc (channel_doc);
	
	return result > 0;
}


static void
rss_logo_callback (GnomeVFSResult  error, GdkPixbuf *pixbuf, gpointer callback_data)
{
	RSSChannelData *channel_data;
	
	channel_data = (RSSChannelData*) callback_data;
	channel_data->load_image_handle = NULL;
	
	if (channel_data->node_data.pixbuf) {
		g_object_unref (channel_data->node_data.pixbuf);
		channel_data->node_data.pixbuf = NULL;
	}
	
	if (pixbuf != NULL) {
		g_object_ref (pixbuf);
		pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, 192, 40);		

		channel_data->node_data.pixbuf = pixbuf;
                channel_data->node_data.prelight_pixbuf = eel_create_spotlight_pixbuf (pixbuf);

                if (channel_data->node_data.markup) {
                        g_free (channel_data->node_data.markup);
			channel_data->node_data.markup = eel_strdup_strftime (_("%I:%M %p"), localtime (&channel_data->last_update));
                }

                update_node (channel_data->owner, (RSSNodeData*)channel_data);
	}
}


/* utility routine to extract items from a node, returning the count of items found */
static int
extract_items (RSSChannelData *channel_data, xmlNodePtr container_node)
{
	RSSItemData *item_parameters;
	xmlNodePtr current_node, title_node, temp_node;
	int item_count;
	char *title, *temp_str;
	gboolean scripting_news_format;
	
	current_node = container_node->children;
	item_count = 0;
	while (current_node != NULL) {
		if (eel_strcmp (current_node->name, "item") == 0) {
			title_node = eel_xml_get_child_by_name (current_node, "title");
			/* look for "text", too, to support Scripting News format */
			scripting_news_format = FALSE;
			if (title_node == NULL) {
				title_node = eel_xml_get_child_by_name (current_node, "text");
				scripting_news_format = title_node != NULL;
			}
			if (title_node != NULL) {
				item_parameters = (RSSItemData*) g_new0 (RSSItemData, 1);
                                item_parameters->owner = channel_data;
				title = xmlNodeGetContent (title_node);
				item_parameters->node_data.title = g_strdup (title);
				xmlFree (title);
				
				temp_node = eel_xml_get_child_by_name (current_node, "link");
				if (temp_node) {
					if (scripting_news_format) {
						temp_node = eel_xml_get_child_by_name (temp_node, "url");		
					}		
					temp_str = xmlNodeGetContent (temp_node);
					item_parameters->node_data.uri = g_strdup (temp_str);
					xmlFree (temp_str);	
				}
				
				if (item_parameters->node_data.title != NULL && item_parameters->node_data.uri != NULL) {
					channel_data->items = g_list_append (channel_data->items, item_parameters);
					item_count += 1;
				} else {
					free_rss_data_item (item_parameters);
				}
			}
		}
		current_node = current_node->next;
	}
	return item_count;
}

/* utility routine to search for the passed-in uri in an item list */
static gboolean
has_matching_uri (GList *items, const char *target_uri, gboolean *old_changed_flag)
{
	GList *current_item;
	RSSItemData *item_data;
	char *mapped_target_uri, *mapped_item_uri;
	gboolean found_match;
	
	*old_changed_flag = FALSE;
	
	if (target_uri == NULL) {
		return FALSE;
	}

	mapped_target_uri = gnome_vfs_make_uri_canonical (target_uri);
	
	current_item = items;
	found_match = FALSE;
	while (current_item != NULL && !found_match) {
		item_data = (RSSItemData*) current_item->data;
		mapped_item_uri = gnome_vfs_make_uri_canonical (item_data->node_data.uri);
		if (eel_strcasecmp (mapped_item_uri, target_uri) == 0) {
			found_match = TRUE;
			*old_changed_flag = item_data->new_item;
		}	
		g_free (mapped_item_uri);
		current_item = current_item->next;
	}
	g_free (mapped_target_uri);
	return found_match;
}

/* take a look at the newly generated items in the passed-in channel,
 * comparing them with the old items and marking them as new if necessary.
 */
static int
mark_new_items (RSSChannelData *channel_data, GList *old_items)
{
	GList *current_item;
	RSSItemData *item_data;
	int changed_count;
	gboolean old_changed_flag;
	
	current_item = channel_data->items;
	changed_count = 0;
	while (current_item != NULL) {	
		item_data = (RSSItemData*) current_item->data;
		if (!has_matching_uri (old_items, item_data->node_data.uri, &old_changed_flag) && !channel_data->initial_load_flag) {
			item_data->new_item = TRUE;	
			channel_data->channel_changed = TRUE;
			nautilus_news_set_news_changed (channel_data->owner, TRUE);
			changed_count += 1;
		} else {
			item_data->new_item = old_changed_flag;
		}
		
		current_item = current_item->next;
	}	
	return changed_count;
}

/* error handling utility */
static void
rss_read_error (RSSChannelData *channel_data)
{
	char *error_message;

	channel_data->update_in_progress = FALSE;
	error_message = g_strdup_printf (_("Couldn't load %s"), channel_data->name);
	nautilus_news_set_title (channel_data, error_message);
	g_free (error_message);
}

/* utility routine to extract the title from a standard rss document.  Return TRUE
 * if we find a valid title.
 */
static gboolean
extract_rss_title (RSSChannelData *channel_data, xmlDoc *rss_document)
{
	gboolean got_title;
	xmlNode *channel_node, *temp_node;
	char *title, *temp_str;
	
	got_title = FALSE;
	channel_node = eel_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "channel");
	if (channel_node != NULL) {		
			temp_node = eel_xml_get_child_by_name (channel_node, "title");
			if (temp_node != NULL) {
				title = xmlNodeGetContent (temp_node);				
				if (title != NULL) {
					nautilus_news_set_title (channel_data, title);
					got_title = TRUE;
					xmlFree (title);	
				}
			}
			
			temp_node = eel_xml_get_child_by_name (channel_node, "link");
			if (temp_node != NULL) {
				temp_str = xmlNodeGetContent (temp_node);				
				if (temp_str != NULL) {
					g_free (channel_data->node_data.uri);
					channel_data->node_data.uri = g_strdup (temp_str);
					xmlFree (temp_str);	
				}
			}
		
	}
	return got_title;
}

/* extract the title for the scripting news variant format */
static gboolean
extract_scripting_news_title (RSSChannelData *channel_data, xmlDoc *rss_document)
{
	gboolean got_title;
	xmlNode *channel_node, *temp_node;
	char *title, *temp_str;

	got_title = FALSE;
	channel_node = eel_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "header");
	if (channel_node != NULL) {
		temp_node = eel_xml_get_child_by_name (channel_node, "channelTitle");
		if (temp_node != NULL) {
			title = xmlNodeGetContent (temp_node);				
			if (title != NULL) {
				nautilus_news_set_title (channel_data, title);
				got_title = TRUE;
				xmlFree (title);	
			}
		}	
		temp_node = eel_xml_get_child_by_name (channel_node, "channelLink");
		if (temp_node != NULL) {
			temp_str = xmlNodeGetContent (temp_node);				
			if (temp_str != NULL) {
				g_free (channel_data->node_data.uri);
				channel_data->node_data.uri = g_strdup (temp_str);
				xmlFree (temp_str);	
			}
		}

	}
	return got_title;
}

/* utility routine to extract the logo image from a standard rss file and start loading it;
 * return true if we get one
 */
static gboolean
extract_rss_image (RSSChannelData *channel_data, xmlDoc *rss_document)
{
	gboolean got_image;
	xmlNode *image_node, *uri_node;
	xmlNode *channel_node;
	char *image_uri;
	
	got_image = FALSE;
	image_node = eel_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "image");
	
	/* if we can't find it at the top level, look inside the channel */
	if (image_node == NULL) {
		channel_node = eel_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "channel");
		if (channel_node != NULL) {
			image_node = eel_xml_get_child_by_name (channel_node, "image");
		}
	} 
	
	if (image_node != NULL) {		
		uri_node = eel_xml_get_child_by_name (image_node, "url");
		if (uri_node != NULL) {
			image_uri = xmlNodeGetContent (uri_node);
			if (image_uri != NULL) {
				channel_data->load_image_handle = eel_gdk_pixbuf_load_async (image_uri, 
					     GNOME_VFS_PRIORITY_DEFAULT, rss_logo_callback, channel_data);
				got_image = TRUE;
				xmlFree (image_uri);
			}
		}
	}
	return got_image;
}

/* utility routine to extract the logo image from a scripting news format rss file and start loading it;
 * return true if we get one
 */
static gboolean
extract_scripting_news_image (RSSChannelData *channel_data, xmlDoc *rss_document)
{
	gboolean got_image;
	xmlNode *image_node, *header_node;
	char *image_uri;

	got_image = FALSE;
	header_node = eel_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "header");
	if (header_node != NULL) {
		image_node = eel_xml_get_child_by_name (header_node, "imageUrl");
		if (image_node != NULL) {
			image_uri = xmlNodeGetContent (image_node);
			if (image_uri != NULL) {
				channel_data->load_image_handle = eel_gdk_pixbuf_load_async (image_uri, 
						GNOME_VFS_PRIORITY_DEFAULT, rss_logo_callback, channel_data);
				got_image = TRUE;
				xmlFree (image_uri);
			}

		}
	}	
	return got_image;
}


/* completion routine invoked when we've loaded the rss file uri.  Parse the xml document, and
 * then extract the various elements that we require.
 */
static void
rss_read_done_callback (GnomeVFSResult result,
			 GnomeVFSFileSize file_size,
			 char *file_contents,
			 gpointer callback_data)
{
	xmlDocPtr rss_document;
	xmlNodePtr channel_node, current_node;
	GList *old_items;
	int item_count, changed_count;
	RSSChannelData *channel_data;
        xmlSAXHandler   silent_handler;
	
	char *buffer;

	channel_data = (RSSChannelData*) callback_data;
	channel_data->load_file_handle = NULL;

	/* make sure the read was successful */
	if (result != GNOME_VFS_OK) {
		g_assert (file_contents == NULL);
		rss_read_error (channel_data);
		return;
	}

	/* flag the update time */
	time (&channel_data->last_update);

	/* Parse the rss file with libxml. The libxml parser requires a zero-terminated array. */
	buffer = g_realloc (file_contents, file_size + 1);
	buffer[file_size] = '\0';

        initxmlDefaultSAXHandler (&silent_handler, FALSE);
        silent_handler.error = NULL;
        silent_handler.fatalError = NULL;
	rss_document = xmlSAXParseMemory (&silent_handler, buffer, file_size, 0);
	g_free (buffer);

	/* make sure there wasn't in error parsing the document */
	if (rss_document == NULL) {
		rss_read_error (channel_data);
		return;
	}
	
	/* set the title to the channel name, in case we don't get anything better from the file */
	nautilus_news_set_title (channel_data, channel_data->name);
	channel_node = eel_xml_get_child_by_name (xmlDocGetRootElement (rss_document), "channel");
	
	if (!extract_rss_title (channel_data, rss_document)) {
		extract_scripting_news_title (channel_data, rss_document);
	}

	/* extract the image uri and, if found, load it asynchronously; don't refetch if we already have one */
	if (channel_data->node_data.pixbuf == NULL && channel_data->load_image_handle == NULL) {
		if (!extract_rss_image (channel_data, rss_document)) {
			extract_scripting_news_image (channel_data, rss_document);
		}	
	}
				
	/* extract the items */
	old_items = channel_data->items;
	channel_data->items = NULL;
		
	current_node = xmlDocGetRootElement (rss_document);

	item_count = extract_items (channel_data, current_node);
	
	/* if we couldn't find any items at the main level, look inside the channel node */
	if (item_count == 0 && channel_node != NULL) {
		item_count = extract_items (channel_data, channel_node);
	}

	changed_count = mark_new_items (channel_data, old_items);

        update_items (channel_data, old_items);
        set_row_widths (channel_data->owner);

	/* we're done, so free everything up */
	eel_g_list_free_deep_custom (old_items, (GFunc) free_rss_data_item, NULL);
	xmlFreeDoc (rss_document);
	channel_data->update_in_progress = FALSE;
	channel_data->initial_load_flag = FALSE;
}

static void
nautilus_news_insert_channel (News *news_data, 
                              RSSChannelData *channel_data,
                              int pos)
{
        GtkTreeIter iter;
        GtkTreePath *path;
        
        g_return_if_fail (channel_data->is_showing);
        g_return_if_fail (channel_data->node_data.path == NULL);

        gtk_tree_store_insert (GTK_TREE_STORE (news_data->news_model), 
                               &iter, NULL, pos);
        gtk_tree_store_set (GTK_TREE_STORE (news_data->news_model),
                            &iter,
                            NEWS_COL_DATA, channel_data,
                            NEWS_COL_WIDTH, -1,
                            -1);
        
        path = gtk_tree_model_get_path (news_data->news_model,
                                        &iter);
        channel_data->node_data.path = path;
}

/* initiate the loading of a channel, by fetching the rss file through gnome-vfs */
static void
nautilus_news_load_channel (News *news_data, RSSChannelData *channel_data)
{
	char *title;

	/* don't load if it's not showing, or it's already loading */
	if (!channel_data->is_showing || channel_data->update_in_progress ||
	    channel_data->load_file_handle != NULL) {
		return;
	}	

	/* load the uri asynchronously, calling a completion routine when completed */
	channel_data->update_in_progress = TRUE;
	channel_data->load_file_handle = eel_read_entire_file_async (channel_data->uri, 
                                                                     GNOME_VFS_PRIORITY_DEFAULT, rss_read_done_callback, channel_data);
	
	/* put up a title that's displayed while we wait */
	title = g_strdup_printf (_("Loading %s"), channel_data->name);
	nautilus_news_set_title (channel_data, title);
	g_free (title);
}

/* detach items from the tree by clearing their path and taking them out
   of the uri hash table */
static void
detach_items (News *news, GList *items)
{
        GList *item;
        RSSItemData *item_data;

        item = items;
        while (item) {
                item_data = (RSSItemData *)item->data;
                
                if (item_data->node_data.path) {
                        gtk_tree_path_free (item_data->node_data.path);
                        item_data->node_data.path = NULL;
                }
                
                if (item_data->node_data.uri) {
                        g_hash_table_remove (news->item_uris,
                                             item_data->node_data.uri);
                }
                
                item = item->next;
        }        
}


static void
remove_items (RSSChannelData *channel_data, GList *items)
{
        int num_children;
        GtkTreeIter parent_iter;
        GtkTreeIter iter;

        detach_items (channel_data->owner, items);

        gtk_tree_model_get_iter (channel_data->owner->news_model, 
                                 &parent_iter, channel_data->node_data.path);
        
        num_children = gtk_tree_model_iter_n_children (channel_data->owner->news_model,
                                                       &parent_iter);
        gtk_tree_model_iter_children (channel_data->owner->news_model,
                                      &iter, &parent_iter);
        
        while (num_children != 0) {
                gtk_tree_store_remove (GTK_TREE_STORE (channel_data->owner->news_model),
                                       &iter);
                num_children--;
        }


}

static void
update_channels (News *news_data)
{
        GList *channel_item;
        RSSChannelData *channel_data;
        gboolean was_showing;

        gtk_tree_store_clear (GTK_TREE_STORE (news_data->news_model));

	channel_item = news_data->channel_list;
	while (channel_item != NULL) {	
		channel_data = (RSSChannelData*) channel_item->data;

                if (channel_data->node_data.path) {
                        gtk_tree_path_free (channel_data->node_data.path);
                        channel_data->node_data.path = NULL;

                        detach_items (news_data, channel_data->items);

                        was_showing = TRUE;
                } else {
                        was_showing = FALSE;
                }

                if (channel_data->is_showing) {
                        nautilus_news_insert_channel (news_data, 
                                                      channel_data, -1);
                        if (was_showing) {
                                update_items (channel_data, NULL);
                        } else {
                                nautilus_news_load_channel (news_data,
                                                            channel_data);
                        }
                }
                
                channel_item = channel_item->next;
        }
        set_row_widths (news_data);
}

static void 
update_items (RSSChannelData *channel_data, GList *old_items)
{
        GtkTreeIter parent_iter;
        GtkTreeIter iter;
        GtkTreePath *path;
        GList *item;
        RSSItemData *item_data;
        int pos;

        g_return_if_fail (channel_data->node_data.path != NULL);

        remove_items (channel_data, old_items);

        gtk_tree_model_get_iter (channel_data->owner->news_model, 
                                 &parent_iter, channel_data->node_data.path);
        
        pos = 0;
        item = channel_data->items;
        while (item && pos < channel_data->owner->max_item_count) {
                item_data = (RSSItemData *)item->data;
                item_data->node_data.markup = get_item_markup (item_data);

                if (item_data->new_item && (channel_data->owner->changed_bullet != NULL)) {
                        item_data->node_data.pixbuf = channel_data->owner->changed_bullet;
                        item_data->node_data.prelight_pixbuf = channel_data->owner->prelight_changed_bullet;
                } else {
                        item_data->node_data.pixbuf = channel_data->owner->bullet;
                        item_data->node_data.prelight_pixbuf = channel_data->owner->prelit_bullet;
                }
                
                gtk_tree_store_insert (GTK_TREE_STORE (channel_data->owner->news_model),
                                       &iter, &parent_iter, pos++);
                gtk_tree_store_set (GTK_TREE_STORE (channel_data->owner->news_model),
                                    &iter, 
                                    NEWS_COL_DATA,
                                    item_data,
                                    NEWS_COL_WIDTH,
                                    -1,
                                    -1);
                path = gtk_tree_model_get_path (channel_data->owner->news_model,
                                                &iter);
                item_data->node_data.path = path;

                g_hash_table_insert (channel_data->owner->item_uris, 
                                     item_data->node_data.uri,
                                     item_data);
                
                item = item->next;                       
        }

        if (channel_data->is_open) {
                gtk_tree_view_expand_row (GTK_TREE_VIEW (channel_data->owner->news_display),
                                          channel_data->node_data.path,
                                          TRUE);
        }
}

/* create a new channel object and initialize it, and start loading the content */
static RSSChannelData*
nautilus_news_make_new_channel (News *news_data,
				const char *name,
				const char* channel_uri,
				gboolean is_open,
                                gboolean is_showing)
{
	RSSChannelData *channel_data;

	channel_data = g_new0 (RSSChannelData, 1);
 	channel_data->name = g_strdup (name);
	channel_data->uri = g_strdup (channel_uri);
 	channel_data->owner = news_data;
	channel_data->is_open = is_open;
	channel_data->is_showing = is_showing;
	channel_data->initial_load_flag = TRUE;

	return channel_data;	
}

/* comparison routine to put channels in alphabetical order */
static gint
compare_channel_names (RSSChannelData *channel_1, RSSChannelData *channel_2)
{
	return strcmp (channel_1->name, channel_2->name);
}

/* add the channels defined in the passed in xml document to the channel list,
 * and start fetching the actual channel data
 */
static void
nautilus_news_add_channels (News *news_data, xmlDocPtr channels)
{
	xmlNodePtr current_channel;
	RSSChannelData *channel_data;
	char *uri, *name;
	char *open_str, *show_str;
	gboolean is_open, is_showing;
	
	/* walk through the children of the root object, generating new channel
	 * objects and adding them to the channel list 
	 */
	current_channel = xmlDocGetRootElement (channels)->children;
	while (current_channel != NULL) {
		if (eel_strcmp (current_channel->name, "rss_channel") == 0) { 				
			name = xmlGetProp (current_channel, "name");
			uri = xmlGetProp (current_channel, "uri");
			open_str = xmlGetProp (current_channel, "open");
			show_str = xmlGetProp (current_channel, "show");
		
			if (uri != NULL) {
				is_open = eel_strcasecmp (open_str, "true") == 0;
				is_showing = eel_strcasecmp (show_str, "true") == 0;
				
				channel_data = nautilus_news_make_new_channel (news_data, name, uri, is_open, is_showing);
				xmlFree (uri);
				if (channel_data != NULL) {
					news_data->channel_list = g_list_insert_sorted (news_data->channel_list,
											channel_data,
											(GCompareFunc) compare_channel_names);
				}
			}
			xmlFree (open_str);
			xmlFree (show_str);
			xmlFree (name);
		}
		current_channel = current_channel->next;
	}
}

static char*
get_xml_path (const char *file_name, gboolean force_local)
{
	char *xml_path;
	char *user_directory;

	user_directory = nautilus_get_user_directory ();

	/* first try the user's home directory */
	xml_path = g_build_filename (user_directory,
                                     file_name,
                                     NULL);
	g_free (user_directory);
	if (force_local || g_file_test (xml_path, G_FILE_TEST_EXISTS)) {
		return xml_path;
	}
	g_free (xml_path);
	
	/* next try the shared directory */
	xml_path = g_build_filename (NAUTILUS_DATADIR,
                                     file_name,
                                     NULL);
	if (g_file_test (xml_path, G_FILE_TEST_EXISTS)) {
		return xml_path;
	}
	g_free (xml_path);

	return NULL;
}

/* read the channel definition xml file and load the channels */
static void
read_channel_list (News *news_data)
{
	char *path;
	xmlDocPtr channel_doc;

	/* free the old channel data, if any  */
	nautilus_news_free_channel_list (news_data);
	
	/* get the path to the local copy of the channels file */
	path = get_xml_path ("news_channels.xml", FALSE);
	if (path != NULL) {	
		channel_doc = xmlParseFile (path);

		if (channel_doc) {
			nautilus_news_add_channels (news_data, channel_doc);
			xmlFreeDoc (channel_doc);
		}
                g_free (path);
	}
}

/* handle periodically updating the channels if necessary */
static int
check_for_updates (gpointer callback_data)
{
	News *news_data;
	guint current_time, next_update_time;
	GList *current_item;
	RSSChannelData *channel_data;
	
	news_data = (News*) callback_data;	
	current_time = time (NULL);
	
	/* loop through the channel list, checking to see if any need updating */
	current_item = news_data->channel_list;
	while (current_item != NULL) {
		channel_data = (RSSChannelData*) current_item->data;	
		next_update_time = channel_data->last_update + channel_data->owner->update_interval;
		
		if (current_time > next_update_time && !channel_data->update_in_progress && channel_data->is_showing) {
			nautilus_news_load_channel (news_data, channel_data);
		}
		current_item = current_item->next;
	}

	return TRUE;
}

/* return an image if there is a new article since last viewing, otherwise return NULL */
static char *
news_get_indicator_image (News *news_data)
{
	if (news_data->news_changed) {
		return g_strdup ("changed_bullet.png");
	}
	return NULL;
}

/* utility routine to load images needed by the news view */
static void
nautilus_news_load_images (News *news_data)
{
	char *news_bullet_path;
	
	if (news_data->bullet != NULL) {
		g_object_unref (news_data->bullet);
	}
	if (news_data->prelit_bullet != NULL) {
		g_object_unref (news_data->prelit_bullet);
	}
	
	news_bullet_path = nautilus_theme_get_image_path ("news_bullet.png");	
	if (news_bullet_path != NULL) {
		news_data->bullet = gdk_pixbuf_new_from_file (news_bullet_path, NULL);
		news_data->prelit_bullet = eel_create_spotlight_pixbuf (news_data->bullet);
		g_free (news_bullet_path);
	}

	if (news_data->changed_bullet != NULL) {
		g_object_unref (news_data->changed_bullet);
	}
	if (news_data->prelight_changed_bullet != NULL) {
		g_object_unref (news_data->prelight_changed_bullet);
	}
	
	news_bullet_path = nautilus_theme_get_image_path ("changed_bullet.png");	
	if (news_bullet_path != NULL) {
		news_data->changed_bullet = gdk_pixbuf_new_from_file (news_bullet_path, NULL);
		news_data->prelight_changed_bullet = eel_create_spotlight_pixbuf (news_data->changed_bullet);
		g_free (news_bullet_path);
	}
}

static void
nautilus_news_cell_data_func (GtkTreeViewColumn *column, 
                              GtkCellRenderer *cell,
                              GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer data)
{
        RSSNodeData *node_data;
        int cell_width;

        gtk_tree_model_get (model, iter, 
                            NEWS_COL_DATA, &node_data, 
                            NEWS_COL_WIDTH, &cell_width, 
                            -1);

        if (node_data) {
                g_object_set (GTK_CELL_RENDERER (cell), "bullet", 
                              node_data->pixbuf, NULL);
                g_object_set (GTK_CELL_RENDERER (cell), "prelit_bullet", 
                              node_data->prelight_pixbuf, NULL);
                g_object_set (GTK_CELL_RENDERER (cell), "markup", node_data->markup);
                g_object_set (GTK_CELL_RENDERER (cell), "wrap_at", 
                              cell_width, NULL);
        }
}

static void
nautilus_news_row_expanded (GtkTreeView *tree_view,
                            GtkTreeIter *iter,
                            GtkTreePath *path,
                            gpointer data)
{
        News *news = (News*)data;
        RSSChannelData *channel_data;
        g_return_if_fail (gtk_tree_path_get_depth (path) == 1);
        
        gtk_tree_model_get (news->news_model, iter, 
                            NEWS_COL_DATA, &channel_data,
                            -1);

        channel_data->is_open = TRUE;
}

static void
nautilus_news_row_collapsed (GtkTreeView *tree_view,
                             GtkTreeIter *iter,
                             GtkTreePath *path,
                             gpointer data)
{
        News *news = (News*)data;
        RSSChannelData *channel_data;
        g_return_if_fail (gtk_tree_path_get_depth (path) == 1);
        
        gtk_tree_model_get (news->news_model, iter, 
                            NEWS_COL_DATA, &channel_data,
                            -1);

        channel_data->is_open = FALSE;
}

/* handle preference changes */
static void
max_items_changed (gpointer user_data)
{
	News *news;
	
	news = (News*) user_data;
	
	news->max_item_count = eel_preferences_get_integer (NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS);
	if (news->max_item_count <= 0) {
		news->max_item_count = 2;		
	}
}

static void
update_interval_changed (gpointer user_data)
{
	News *news;
	
	news = (News*) user_data;
	
	news->update_interval = 60 * eel_preferences_get_integer (NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL);
	if (news->update_interval < 60) {
		news->update_interval = 60;		
	}
}

/* utility to count the visible channels */
static int
count_visible_channels (News *news)
{
	GList *current_item;
	RSSChannelData *current_channel;
	int visible_count;

	visible_count = 0;
	current_item = news->channel_list;
	while (current_item != NULL) {
		current_channel = (RSSChannelData *) current_item->data;
		if (current_channel->is_showing) {
			visible_count += 1;
		}
		current_item = current_item->next;
	}
	return visible_count;
}

/* utility to show and hide the views based on the mode */
static void
set_views_for_mode (News *news)
{
        gtk_notebook_set_current_page (GTK_NOTEBOOK (news->main_container), 
                                       news->current_page);

        if (news->current_page == PAGE_MAIN) {
		if (count_visible_channels (news) == 0) {
                        gtk_notebook_set_current_page (GTK_NOTEBOOK (news->news_notebook), MAIN_PAGE_EMPTY);
		} else {
                        gtk_notebook_set_current_page (GTK_NOTEBOOK (news->news_notebook), MAIN_PAGE_DISPLAY);
		}
        }
}

static void
switch_page (News *news, NewsPageNum page)
{
        if (news->current_page == PAGE_CONFIGURE) {
                nautilus_news_save_channel_state (news);
        }
 
        news->current_page = page;        
        set_views_for_mode (news);

        if (page == PAGE_MAIN) {
                update_channels (news);
                check_for_updates (news);
        }
}

/* here's the button callback routine that toggles between display modes  */
static void
configure_button_clicked (GtkWidget *widget, News *news)
{
        if (news->current_page == PAGE_CONFIGURE) {
                switch_page (news, PAGE_MAIN);
        } else {
                switch_page (news, PAGE_CONFIGURE);
        }
}

/* here's the button callback routine that handles the add new site button
 * by showing the relevant widgets.
 */
static void
add_site_button_clicked (GtkWidget *widget, News *news)
{
        switch_page (news, PAGE_ADD_SITE);
}


/* utility to add an entry to the remove channel clist */
static void
add_channel_to_remove_list (News *news_data, const char *channel_name)
{
        GtkTreeIter iter;
        GtkListStore *store;
        store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (news_data->remove_site_list)));
        
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, REMOVE_COL_NAME, channel_name, -1);
}

static void
update_remove_button (News *news)
{
        GtkTreeSelection *selection;
        GtkTreeIter iter;
        gboolean sensitive;

        sensitive = FALSE;
        if (news->channel_list != NULL) {
                selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (news->remove_site_list));
                if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                        sensitive = TRUE;
                }        
        }
        
	gtk_widget_set_sensitive (news->remove_button, sensitive);
}

/* handle adding a new site from the data in the "add site" fields */
static void
add_site_from_fields (GtkWidget *widget, News *news)
{
	char *site_name, *site_location;
	char *site_uri, *buffer;
	RSSChannelData *channel_data;
	GnomeVFSResult result;
	int channel_count, byte_count;
	gboolean got_xml_file;
	
	site_name = (char *) gtk_entry_get_text (GTK_ENTRY (news->item_name_field));
	site_location = (char *)gtk_entry_get_text (GTK_ENTRY (news->item_location_field));

	/* make sure there's something in the fields */
	if (site_name == NULL || strlen (site_name) == 0) {
		eel_show_error_dialog (_("Sorry, but you have not specified a name for the site!"), _("Missing Site Name Error"), NULL);
		return;
	}
	if (site_location == NULL || strlen (site_location) == 0) {
		eel_show_error_dialog (_("Sorry, but you have not specified a URL for the site!"), _("Missing URL Error"), NULL);
		return;
	}
	
	/* if there isn't a protocol specified for the location, use http */
	if (strchr (site_location, ':') == NULL) {
		site_uri = g_strconcat ("http://", site_location, NULL);
	} else {
		site_uri = g_strdup (site_location);
	}

	/* verify that we can read the specified location and that it's an xml file */
	result = eel_read_entire_file (site_uri, &byte_count, &buffer);
	got_xml_file = (result == GNOME_VFS_OK) && eel_istr_has_prefix (buffer, "<?xml");
	g_free (buffer);
	if (!got_xml_file) {
		g_free (site_uri);
		eel_show_error_dialog (_("Sorry, but the specified url doesn't seem to be a valid RSS file!"), _("Invalid RSS URL"), NULL);
		return;
	}
	
	/* make the new channel */		
	channel_data = nautilus_news_make_new_channel (news, site_name, site_uri, TRUE, TRUE);
	g_free (site_uri);
	
	if (channel_data != NULL) {
		news->channel_list = g_list_insert_sorted (news->channel_list,
							   channel_data,
							   (GCompareFunc) compare_channel_names);
		channel_count = g_list_length (news->channel_list);
		add_channel_entry (news, site_name, channel_count, TRUE);
		add_channel_to_remove_list (news, site_name);
	}
	/* clear fields for next time */
	gtk_editable_delete_text (GTK_EDITABLE (news->item_name_field), 0, -1);
	gtk_editable_delete_text (GTK_EDITABLE (news->item_location_field), 0, -1);

	update_remove_button (news);
			
	/* back to configure mode */
        switch_page (news, PAGE_CONFIGURE);
}

/* handle the remove command  */
static void
remove_selected_site (GtkWidget *widget, News *news)
{
        GtkTreeSelection *selection;
        GtkTreeIter iter;
	RSSChannelData *channel_data;
	GList *channel_item;
	const char *channel_name;
        GValue channel_name_value = { 0, };
        
        GtkTreeModel *model;
        
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (news->remove_site_list));
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (news->remove_site_list));
        
        if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                gtk_tree_model_get_value (model, 
                                          &iter, 
                                          REMOVE_COL_NAME,
                                          &channel_name_value);

                channel_name = g_value_get_string (&channel_name_value);

                /* remove the channel from the channel linked list */
                channel_data = get_channel_from_name (news, channel_name);
                
                channel_item = g_list_find (news->channel_list, channel_data);
                if (channel_item != NULL) {
                        news->channel_list = g_list_remove_link (news->channel_list, channel_item);
                }
                
                /* remove the channel from the add list and release it */
                if (channel_data != NULL) {
                        gtk_widget_destroy (channel_data->checkbox);
                        free_channel (channel_data);	
                }
                
                gtk_list_store_remove (GTK_LIST_STORE (model), 
                                       &iter);
                update_remove_button (news);
                switch_page (news, PAGE_CONFIGURE);
        }
}

/* utility routine to create the button box and constituent buttons */
static GtkWidget *
add_command_buttons (News *news_data, const char* label, gboolean from_configure)
{
	GtkWidget *frame;
	GtkWidget *button_box;
	GtkWidget *button;
	
	frame = gtk_frame_new (NULL);
  	gtk_frame_set_shadow_type( GTK_FRAME (frame), GTK_SHADOW_OUT);

    	button_box = gtk_hbutton_box_new ();

	gtk_container_set_border_width (GTK_CONTAINER (button_box), 2);
        gtk_widget_show (button_box);
  	gtk_container_add (GTK_CONTAINER (frame), button_box);

  	/* Set the appearance of the Button Box */
  	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 4);
	
	if (from_configure) {
		button = gtk_button_new_with_mnemonic (_("Edi_t"));
                gtk_widget_show (button);
		gtk_container_add (GTK_CONTAINER (button_box), button);

		g_signal_connect (button, "clicked",
                                  G_CALLBACK (add_site_button_clicked), news_data);
	}
	
	button = gtk_button_new_from_stock (label);
        gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (button_box), button);

	g_signal_connect (button, "clicked",
                          G_CALLBACK (configure_button_clicked), news_data);
		      	
	return frame;
}

/* utility routine to look up a channel from it's name */
static RSSChannelData*
get_channel_from_name (News *news_data, const char *channel_name)
{
	GList *channel_item;
	RSSChannelData *channel_data;
	
	channel_item = news_data->channel_list;
	while (channel_item != NULL) {	
		channel_data = (RSSChannelData*) channel_item->data;
		if (eel_strcasecmp (channel_data->name, channel_name) == 0) {
			return channel_data;
		}
		channel_item = channel_item->next;
	}
	return NULL;
}

/* here's the handler for handling clicks in channel check boxes */
static void
check_button_toggled_callback (GtkToggleButton *toggle_button, gpointer user_data)
{
	News *news_data;
	char *channel_name;
	RSSChannelData *channel_data;
	
	news_data = (News*) user_data;
	channel_name = g_object_get_data (G_OBJECT (toggle_button), "channel_name");
	
	channel_data = get_channel_from_name (news_data, channel_name);
	if (channel_data != NULL) { 
		channel_data->is_showing = !channel_data->is_showing;
		if (channel_data->is_showing) {
			channel_data->is_open = TRUE;
		}
	}	
}

static gboolean
check_button_focus_in_callback (GtkWidget *widget, 
                                GdkEventFocus *event, 
                                gpointer data)
{
	g_return_val_if_fail (widget->parent && widget->parent->parent, FALSE);
	g_return_val_if_fail (GTK_IS_VIEWPORT (widget->parent->parent), FALSE);
        
	eel_gtk_viewport_scroll_to_rect (GTK_VIEWPORT (widget->parent->parent), 
					 &widget->allocation);
	
	return FALSE;
}


/* callback to maintain the current location */
static void
nautilus_news_load_location (NautilusView *view, const char *location, News *news)
{
        RSSItemData *item;
        char *markup;
        char *bold;

        if (news->current_item) {
                if (news->current_item->node_data.markup) {
                        g_free (news->current_item->node_data.markup);
                }
                news->current_item->node_data.markup = 
                        get_item_markup (news->current_item);
                update_node (news, (RSSNodeData*)news->current_item);
                
                news->current_item = NULL;
        }

        item = g_hash_table_lookup (news->item_uris, location);
        if (item) {
                markup = get_item_markup (item);
                if (markup) {
                        bold = g_strdup_printf ("<b>%s</b>", markup);
                        g_free (markup);
                
                } else {
                        bold = NULL;
                }
                
                if (item->node_data.markup) {
                        g_free (item->node_data.markup);
                }
                item->node_data.markup = bold;

                update_node (news, (RSSNodeData *)item);

                news->current_item = item;
        }
}

/* utility routine to determine the sort position of a checkbox */
static int
determine_sort_position (GtkWidget *container, const char *name)
{
	GList *checkboxes, *current_item;
	char *current_name;
	int index;
	
	checkboxes = gtk_container_get_children (GTK_CONTAINER (container));
	index = 0;
	current_item = checkboxes;
	while (current_item != NULL) {
		current_name = g_object_get_data (G_OBJECT (current_item->data), "channel_name");
		
		if (eel_strcasecmp (current_name, name) > 0) {
			g_list_free (checkboxes);
			return index;
		}
		
		index += 1;
		current_item = current_item->next;
	}	
	g_list_free (checkboxes);
	return index;
}

/* utility routine to add a check-box entry to the channel list */
static void
add_channel_entry (News *news_data, const char *channel_name, int index, gboolean is_showing)
{
	GtkWidget *check_button;
	RSSChannelData *channel_data;
	int sort_position;
	
	check_button = gtk_check_button_new_with_label (channel_name);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), is_showing);
        gtk_widget_show (check_button);
	gtk_box_pack_start (GTK_BOX (news_data->checkbox_list), check_button, FALSE, FALSE, 0);

	g_signal_connect (check_button, "toggled",
                          G_CALLBACK (check_button_toggled_callback),
                          news_data);
        g_signal_connect (check_button, "focus_in_event",
                          G_CALLBACK (check_button_focus_in_callback),
                          news_data);

	/* reorder newly added button so it's sorted by it's name */
	sort_position = determine_sort_position (news_data->checkbox_list, channel_name);
	gtk_box_reorder_child (GTK_BOX (news_data->checkbox_list), check_button, sort_position);	
	
	/* set up pointer in channel object to checkbox, so we can delete it */
	channel_data = get_channel_from_name (news_data, channel_name);
	if (channel_data != NULL) {
		channel_data->checkbox = check_button;
	}
	
	/* set up user data to use in toggle handler */
        g_object_set_data (G_OBJECT (check_button), "user_data", news_data);
	g_object_set_data_full (G_OBJECT(check_button),
				  "channel_name",
				  g_strdup (channel_name),
				  g_free);
}

/* here's the routine that loads and parses the xml file, then iterates through it
 * to add channels to the enable/disable lists
 */
static void
add_channels_to_lists (News* news_data)
{
	char *path;
	char *channel_name, *show_str;
	xmlDocPtr channel_doc;
	xmlNodePtr current_channel;
	int channel_index;
	gboolean is_shown;
	
	/* read the xml file and parse it */
	path = get_xml_path ("news_channels.xml", FALSE);
	if (path == NULL) {	
		return;
	}	
	
	channel_doc = xmlParseFile (path);
	g_free (path);
	if (channel_doc == NULL) {
		return;
	}
	
	/* loop through the channel entries, adding an entry to the configure
	 * list for each entry in the file
	 */
	current_channel = xmlDocGetRootElement (channel_doc)->children;
	channel_index = 0;
	while (current_channel != NULL) {
		if (eel_strcmp (current_channel->name, "rss_channel") == 0) { 				
			channel_name = xmlGetProp (current_channel, "name");
			show_str = xmlGetProp (current_channel, "show");
			is_shown = eel_strcasecmp (show_str, "true") == 0;
			
			/* add an entry to the channel list */
			if (channel_name != NULL) {
				add_channel_entry (news_data, channel_name, channel_index, is_shown);
				add_channel_to_remove_list (news_data, channel_name);

				channel_index += 1;
			}
			
			xmlFree (show_str);
			xmlFree (channel_name);
		}
		current_channel = current_channel->next;
	}

	xmlFreeDoc (channel_doc);
}

/* code-saving utility to allocate a left-justified anti-aliased label */
static GtkWidget *
news_label_new (const char *label_text, gboolean title_mode)
{
	GtkWidget *label;
	
	label = gtk_label_new_with_mnemonic (label_text);
	if (title_mode) {
		eel_gtk_label_make_bold (GTK_LABEL (label));
	}
	
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	return label;	
}

static void
remove_list_selection_changed (GObject *obj, News *news)
{
        update_remove_button (news);
}

/* generate the remove widgets */
static void
make_remove_widgets (News *news, GtkWidget *container)
{
	GtkWidget *button_box;
	GtkScrolledWindow *scrolled_window;
	GtkListStore *store;
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
        GtkTreeSelection *selection;

        store = gtk_list_store_new (LAST_REMOVE_COL, G_TYPE_STRING);

	news->remove_site_list = 
                gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (news->remove_site_list), FALSE);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (news->remove_site_list));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        g_signal_connect (selection, "changed", 
                          G_CALLBACK (remove_list_selection_changed), news);

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Site Name"),
                                                           renderer, 
                                                           "text", 
                                                           REMOVE_COL_NAME,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (news->remove_site_list), 
                                     column);
        g_object_unref (store);

        gtk_widget_show (news->remove_site_list);
        
        scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);   
        gtk_scrolled_window_set_shadow_type (scrolled_window, 
                                             GTK_SHADOW_IN);
        gtk_widget_show (GTK_WIDGET (scrolled_window));
	gtk_container_add (GTK_CONTAINER (scrolled_window), news->remove_site_list);
	gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);

	/* install the remove button */
    	button_box = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (container), button_box, FALSE, FALSE, 4);
 	
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 4);
	
	news->remove_button = gtk_button_new_with_mnemonic (_("_Remove Site"));
        gtk_widget_show (news->remove_button);
        
	gtk_container_add (GTK_CONTAINER (button_box), news->remove_button);

        gtk_widget_show (button_box);

	g_signal_connect (news->remove_button, "clicked",
                            (GtkSignalFunc) remove_selected_site, news);
}

/* generate the add new site widgets */
static void
make_add_widgets (News *news, GtkWidget *container)
{
	GtkWidget *label;
	GtkWidget *temp_vbox;
	GtkWidget *button_box;
	GtkWidget *button;
	
	temp_vbox = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (temp_vbox);
        
	gtk_box_pack_start (GTK_BOX (container), temp_vbox, FALSE, FALSE, 0);

	/* allocate the name field */
	label = news_label_new (_("Site _Name:"), FALSE);
        gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (temp_vbox), label, FALSE, FALSE, 0);

	news->item_name_field = nautilus_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), 
                                       news->item_name_field);
        
	gtk_widget_show (news->item_name_field);
	gtk_box_pack_start (GTK_BOX (temp_vbox), news->item_name_field, FALSE, FALSE, 0);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (news->item_name_field), TRUE);
        
	/* allocate the location field */
	temp_vbox = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (temp_vbox);
        
	gtk_box_pack_start (GTK_BOX (container), temp_vbox, FALSE, FALSE, 0);

	label = news_label_new (_("Site _RSS URL:"), FALSE);
        gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (temp_vbox), label, FALSE, FALSE, 0);

	news->item_location_field = nautilus_entry_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), 
                                       news->item_location_field);
        gtk_widget_show (news->item_location_field);
	gtk_box_pack_start (GTK_BOX (temp_vbox), news->item_location_field, FALSE, FALSE, 0);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (news->item_location_field), TRUE);
        
	/* install the add buttons */
    	button_box = gtk_hbutton_box_new ();
        gtk_widget_show (button_box);

	gtk_box_pack_start (GTK_BOX (container), button_box, FALSE, FALSE, 4);
 	
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 4);
	
	button = gtk_button_new_with_mnemonic (_("_Add New Site"));
        gtk_widget_show (button);

	gtk_container_add (GTK_CONTAINER (button_box), button);
	g_signal_connect (button, "clicked",
                          G_CALLBACK (add_site_from_fields), news);
}

/* allocate the add/remove location widgets */
static void
set_up_edit_widgets (News *news, GtkWidget *container)
{
	GtkWidget  *label;
	GtkWidget  *expand_box;
	GtkWidget *button_box;
	GtkWidget *temp_vbox;
	
	news->edit_site_box = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (news->edit_site_box);
        
	gtk_notebook_insert_page (GTK_NOTEBOOK (container), 
                                  news->edit_site_box, NULL, PAGE_ADD_SITE);

	expand_box = gtk_vbox_new (FALSE, 0);	
        gtk_widget_show (expand_box);
        
	gtk_box_pack_start (GTK_BOX (news->edit_site_box), expand_box, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (expand_box), 4);

	/* make the add new site label */
	label = news_label_new (_("Add a New Site:"), TRUE);
        gtk_widget_show (label);
        
	gtk_box_pack_start (GTK_BOX (expand_box), label, FALSE, FALSE, 0);
	
	/* allocate the add new site widgets */
	make_add_widgets (news, expand_box);
	
	/* allocate the remove label */
	temp_vbox = gtk_vbox_new (FALSE, 0);

	label = news_label_new (_("Remove a _Site:"), TRUE);
        gtk_widget_show (label);
        
	gtk_box_pack_start (GTK_BOX (temp_vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (temp_vbox);
        
	/* allocate the remove widgets */
	make_remove_widgets (news, temp_vbox);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), 
                                       news->remove_site_list);
        
	gtk_box_pack_start (GTK_BOX (expand_box), temp_vbox, TRUE, TRUE, 0);
	
	/* add the button box at the bottom with a cancel button */
	button_box = add_command_buttons (news, GTK_STOCK_CANCEL, FALSE);
        gtk_widget_show (button_box);
	gtk_box_pack_start (GTK_BOX (news->edit_site_box), button_box, FALSE, FALSE, 0);	
}

static gboolean
site_list_mnemonic_activate (GtkWidget *widget, gboolean group_cycling,
                             gpointer data)
{
        News *news;
        RSSChannelData *channel_data;
        
        news = (News*)data;
        channel_data = (RSSChannelData*)news->channel_list->data;
        
        gtk_widget_grab_focus (channel_data->checkbox);

        return TRUE;
}

/* allocate the widgets for the configure mode */
static void
set_up_configure_widgets (News *news, GtkWidget *container)
{
	GtkWidget *button_box;
	GtkWidget *viewport;
	GtkScrolledWindow *scrolled_window;
	GtkWidget  *label;
	
	news->configure_box = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (news->configure_box);
        
	gtk_notebook_insert_page (GTK_NOTEBOOK (container), 
                                  news->configure_box, NULL, PAGE_CONFIGURE);

	/* add a descriptive label */
	label = news_label_new (_("_Select Sites:"), TRUE);

        gtk_widget_show (label);
        
	gtk_box_pack_start (GTK_BOX (news->configure_box), label, FALSE, FALSE, 0);
	
	/* allocate a table to hold the check boxes */
	news->checkbox_list = gtk_vbox_new (FALSE, 0);
        g_signal_connect (GTK_WIDGET (news->checkbox_list), 
                          "mnemonic_activate", 
                          G_CALLBACK (site_list_mnemonic_activate), news);

        gtk_label_set_mnemonic_widget (GTK_LABEL (label), news->checkbox_list);

        gtk_widget_show (news->checkbox_list);
        
	scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);   
        gtk_widget_show (GTK_WIDGET (scrolled_window));

	viewport = gtk_viewport_new (gtk_scrolled_window_get_hadjustment (scrolled_window),
			  		gtk_scrolled_window_get_vadjustment (scrolled_window));
        gtk_widget_show (viewport);

	gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);
	gtk_container_add (GTK_CONTAINER (viewport), news->checkbox_list);

 	gtk_box_pack_start (GTK_BOX (news->configure_box), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);
		
	/* allocate the button box for the done button */
        button_box = add_command_buttons (news, _("_Done"), TRUE);
        gtk_widget_show (button_box);
	gtk_box_pack_start (GTK_BOX (news->configure_box), button_box, FALSE, FALSE, 0); 
}

/* allocate the widgets for the main display mode */
static void
set_up_main_widgets (News *news, GtkWidget *container)
{
        GtkWidget *button_box;
	GtkWidget *scrolled_window;
	
	/* allocate a vbox to hold all of the main UI elements elements */
        news->main_box = gtk_vbox_new (FALSE, 0);
        gtk_widget_show (news->main_box);

        gtk_notebook_insert_page (GTK_NOTEBOOK (container), 
                                  news->main_box, NULL, PAGE_MAIN);

        news->news_notebook = gtk_notebook_new ();
        gtk_widget_show (news->news_notebook);
        
        gtk_notebook_set_show_border (GTK_NOTEBOOK (news->news_notebook),
                                      FALSE);
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (news->news_notebook), 
                                    FALSE);
        
        gtk_widget_show (news->news_notebook);
        gtk_box_pack_start (GTK_BOX (news->main_box), news->news_notebook,
                            TRUE, TRUE, 0);

        /* create and install the display area */
        news->news_model = GTK_TREE_MODEL (gtk_tree_store_new (LAST_NEWS_COL,
                                                               G_TYPE_POINTER,
                                                               G_TYPE_INT));
        
        news->news_display = gtk_tree_view_new_with_model (GTK_TREE_MODEL (news->news_model));
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (news->news_display),
                                           FALSE);
        gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (news->news_display)),
                                     GTK_SELECTION_NONE);

        news->column = gtk_tree_view_column_new ();

        news->renderer = nautilus_cell_renderer_news_new ();
        gtk_tree_view_column_pack_start (news->column, news->renderer, FALSE);
        gtk_tree_view_column_set_cell_data_func (news->column,
                                                 news->renderer,
                                                 nautilus_news_cell_data_func,
                                                 news, NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (news->news_display), 
                                     news->column);

        gtk_widget_show (news->news_display);
	/* put the display in a scrolled window so it can scroll */
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                             GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), 
                           news->news_display);
        gtk_widget_show (scrolled_window);
        
	gtk_notebook_append_page (GTK_NOTEBOOK (news->news_notebook), 
                                  scrolled_window, NULL);
 	news->news_display_scrolled_window = scrolled_window;
	
	/* add the empty message */
	news->empty_message = gtk_label_new (_("The News panel displays current headlines from your favorite websites.  Click the \'Select Sites\' button to select the sites to display."));
	eel_gtk_label_set_scale (GTK_LABEL (news->empty_message), PANGO_SCALE_LARGE);
	gtk_label_set_line_wrap (GTK_LABEL (news->empty_message), TRUE);	
        gtk_widget_show (news->empty_message);
        
	gtk_notebook_append_page (GTK_NOTEBOOK (news->news_notebook), 
                                  news->empty_message, NULL);
	
  	g_signal_connect (news->news_display, "size_allocate",
                          G_CALLBACK (nautilus_news_size_allocate), news);
  	g_signal_connect (news->news_display, "row_activated",
                          G_CALLBACK (nautilus_news_row_activated), news);
        g_signal_connect (news->news_display, "row_expanded",
                          G_CALLBACK (nautilus_news_row_expanded), news);
        g_signal_connect (news->news_display, "row_collapsed",
                          G_CALLBACK (nautilus_news_row_collapsed), news);
        
  	g_signal_connect_after (news->news_display, "button_release_event",
                          G_CALLBACK (nautilus_news_button_release_event), news);

        /* create a button box to hold the command buttons */
        button_box = add_command_buttons (news, _("_Select Sites"), FALSE);
        gtk_widget_show (button_box);
        gtk_box_pack_start (GTK_BOX (news->main_box), button_box, FALSE, FALSE, 0); 
}


static BonoboObject *
make_news_view (const char *iid, void *callback_data)
{
	News *news;
	
	/* create the private data for the news view */         
        news = g_new0 (News, 1);


	/* allocate the main container */
	news->main_container = gtk_notebook_new ();
        gtk_notebook_set_show_border (GTK_NOTEBOOK (news->main_container),
                                      FALSE);
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (news->main_container), 
                                    FALSE);

	/* set up the widgets for the main,configure and add modes */
	set_up_main_widgets (news, news->main_container);
	set_up_configure_widgets (news, news->main_container);
	set_up_edit_widgets (news, news->main_container);

        gtk_widget_show (news->main_container);
		
	/* get preferences and sanity check them */
	news->max_item_count = eel_preferences_get_integer (NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS);
	news->update_interval = 60 * eel_preferences_get_integer (NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL);	
	news->update_timeout = -1;
		
	if (news->max_item_count <= 0) {
		news->max_item_count = 2;		
	}

	if (news->update_interval < 60) {
		news->update_interval = 60;		
	}
        
        news->wrap_idle_handle = -1;
        
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS, max_items_changed, news);	
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL, update_interval_changed, news);
	
	/* load some images */
	nautilus_news_load_images (news);

	/* set up the update timeout */
	news->timer_task = gtk_timeout_add (10000, check_for_updates, news);

        gtk_widget_show_all (news->main_container);

	/* Create the nautilus view CORBA object. */
        news->view = nautilus_view_new (news->main_container);
        g_signal_connect (news->view, "destroy", G_CALLBACK (do_destroy), news);

	g_signal_connect (news->view, "load_location",
                            G_CALLBACK (nautilus_news_load_location), news);

	/* allocate a property bag to reflect the TAB_IMAGE property */
	news->property_bag = bonobo_property_bag_new (get_bonobo_properties,  set_bonobo_properties, news);

	bonobo_control_set_properties (nautilus_view_get_bonobo_control (news->view), BONOBO_OBJREF (news->property_bag), NULL);
	bonobo_property_bag_add (news->property_bag, "tab_image", TAB_IMAGE, BONOBO_ARG_STRING, NULL,
				 _("image indicating that the news has changed"), 0);
	bonobo_property_bag_add (news->property_bag, "close", CLOSE_NOTIFY,
				 BONOBO_ARG_BOOLEAN, NULL, "close notification", 0);
 	
	nautilus_news_clear_changed_flags (news);
 	
        news->item_uris = g_hash_table_new (g_str_hash, g_str_equal);

        /* read the channel definition file and start loading the channels */
	read_channel_list (news);
        update_channels (news);

 	/* populate the configuration list */
	add_channels_to_lists (news);
	update_remove_button (news);

        /* default to the main mode */
        news->current_page = PAGE_MAIN;
	set_views_for_mode (news);

  	/* return the nautilus view */    
        return BONOBO_OBJECT (news->view);
}

int
main(int argc, char *argv[])
{
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
        return nautilus_view_standard_main ("nautilus-news",
                                            VERSION,
                                            GETTEXT_PACKAGE,
                                            GNOMELOCALEDIR,
                                            argc,
                                            argv,
                                            "OAFIID:Nautilus_News_View_Factory",
                                            "OAFIID:Nautilus_News_View",
                                            make_news_view,
                                            nautilus_global_preferences_init,
                                            NULL);
}

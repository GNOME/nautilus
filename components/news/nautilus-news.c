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

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkvbox.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libgnomevfs/gnome-vfs-utils.h>

#include <eel/eel-background.h>
#include <eel/eel-debug.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-label.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-smooth-text-layout.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>

#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-font-factory.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-theme.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>

#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus/nautilus-view-standard-main.h>

#include "nautilus-news-pixmaps.h"

/* property bag getting and setting routines */
enum {
	TAB_IMAGE,
	CLOSE_NOTIFY,
};

/* data structure for the news view */
typedef struct {
	NautilusView *view;
	BonoboPropertyBag *property_bag;
	
	char *uri; /* keep track of where content view is at */
	
	GList *channel_list;
	
	GdkPixbuf *pixbuf; /* offscreen buffer for news display */
	
	GdkPixbuf *closed_triangle;
	GdkPixbuf *closed_triangle_changed;
	GdkPixbuf *open_triangle;
	GdkPixbuf *open_triangle_changed;
	GdkPixbuf *bullet;
	GdkPixbuf *changed_bullet;
	
	GtkWidget *main_box;
	GtkWidget *news_display;
	GtkWidget *news_display_scrolled_window;
	GtkWidget *empty_message;
	
	GtkWidget *configure_box;
	GtkWidget *checkbox_list;
	
	GtkWidget *edit_site_box;
	GtkWidget *item_name_field;
	GtkWidget *item_location_field;
	
	GtkWidget *remove_site_list;
	GtkWidget *remove_button;
	int remove_selection_index;
	
	int line_width;
	int display_height;
	int max_item_count;
	uint update_interval;
	int update_timeout;
	
	EelScalableFont *font;	
	EelScalableFont *bold_font;	
	
	gboolean news_changed;
	gboolean always_display_title;
	gboolean configure_mode;
	gboolean opened;
		
	guint timer_task;
} News;

/* per item structure for rss items */
typedef struct {
	char *item_title;
	char *item_url;	
	
	int item_start_y;
	int item_end_y;

	gboolean new_item;
} RSSItemData;

/* per channel structure for rss channel */
typedef struct {
	char *name;
	char *uri;
	char *link_uri;
	News *owner;
	
	char *title;
	GdkPixbuf *logo_image;	
	
	GList *items;
	
	EelReadFileHandle *load_file_handle;
	EelPixbufLoadHandle *load_image_handle;
	
	GtkWidget *checkbox;
	
	int prelight_index;
	
	int logo_start_y;
	int logo_end_y;
	int items_start_y;
	int items_end_y;
	
	time_t last_update;
	
	gboolean is_open;	
	gboolean is_showing;	
	
	gboolean initial_load_flag;
	gboolean channel_changed;
	gboolean update_in_progress;
} RSSChannelData;

/* pixel and drawing constant defines */

#define RSS_ITEM_HEIGHT 12
#define CHANNEL_GAP_SIZE 6
#define LOGO_GAP_SIZE 2
#define DISCLOSURE_RIGHT_POSITION 16
#define LOGO_LEFT_OFFSET 15
#define INITIAL_Y_OFFSET 2
#define ITEM_POSITION 31
#define RIGHT_ITEM_MARGIN 8
#define EMPTY_MESSAGE_MARGIN 12
#define MAX_CHARS_IN_ITEM 140
#define ITEM_FONT_SIZE 11
#define TIME_FONT_SIZE 9
#define TIME_MARGIN_OFFSET 2
#define TITLE_FONT_SIZE 18
#define MINIMUM_DRAW_SIZE 16
#define NEWS_BACKGROUND_RGBA 0xFFFFFFFF
#define ELLIPSIS "..."

/* special prelight values for logo and triangle */
#define PRELIGHT_LOGO 65536
#define PRELIGHT_TRIANGLE 65537

static char *news_get_indicator_image   (News *news_data);
static void nautilus_news_free_channel_list (News *news_data);
static gboolean nautilus_news_save_channel_state (News* news_data);
static void update_size_and_redraw (News* news_data);
static void queue_update_size_and_redraw (News* news_data);

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
static RSSChannelData*
nautilus_news_make_new_channel (News *news_data,
				const char *name,
				const char* channel_uri,
				gboolean is_open,
				gboolean is_showing);

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
			news->configure_mode = FALSE;
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
	g_free (news->uri);
	
	if (news->font) {
		gtk_object_unref (GTK_OBJECT (news->font));
	}
	
	if (news->bold_font) {
		gtk_object_unref (GTK_OBJECT (news->bold_font));
	}

	if (news->timer_task != 0) {
		gtk_timeout_remove (news->timer_task);
		news->timer_task = 0;
	}

	if (news->update_timeout > 0) {
		gtk_timeout_remove (news->update_timeout);
		news->update_timeout = -1;
	}
	
        if (news->closed_triangle != NULL) {
		gdk_pixbuf_unref (news->closed_triangle);
	}
        
	if (news->closed_triangle_changed != NULL) {
		gdk_pixbuf_unref (news->closed_triangle_changed);
	}
        
	if (news->open_triangle != NULL) {
		gdk_pixbuf_unref (news->open_triangle);
	}
	
	if (news->open_triangle_changed != NULL) {
		gdk_pixbuf_unref (news->open_triangle_changed);
	}
	
	if (news->bullet != NULL) {
		gdk_pixbuf_unref (news->bullet);
	}	
	
	if (news->changed_bullet != NULL) {
		gdk_pixbuf_unref (news->changed_bullet);
	}	
	
	/* free all the channel data */
	nautilus_news_free_channel_list (news);

	/* free the property bag */
	if (news->property_bag != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (news->property_bag));
	}
	
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS,
                                         max_items_changed,
                                         news);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL,
                                         update_interval_changed,
                                         news);
	
        g_free (news);
}


/* drawing routines start here... */

/* convenience routine to composite an image with the proper clipping */
static void
pixbuf_composite (GdkPixbuf *source, GdkPixbuf *destination, int x_offset, int y_offset, int alpha)
{
	int source_width, source_height, dest_width, dest_height;
	double float_x_offset, float_y_offset;
	
	source_width  = gdk_pixbuf_get_width (source);
	source_height = gdk_pixbuf_get_height (source);
	dest_width  = gdk_pixbuf_get_width (destination);
	dest_height = gdk_pixbuf_get_height (destination);
	
	float_x_offset = x_offset;
	float_y_offset = y_offset;
	
	/* clip to the destination size */
	if ((x_offset + source_width) > dest_width) {
		source_width = dest_width - x_offset;
	}
	if ((y_offset + source_height) > dest_height) {
		source_height = dest_height - y_offset;
	}
	
	gdk_pixbuf_composite (source, destination, x_offset, y_offset, source_width, source_height,
					float_x_offset, float_y_offset, 1.0, 1.0, GDK_PIXBUF_ALPHA_BILEVEL, alpha);
}

/* utility to draw the disclosure triangle */
static void
draw_triangle (GdkPixbuf *pixbuf, RSSChannelData *channel_data, int v_offset)
{
	GdkPixbuf *triangle;
	GdkPixbuf *mapped_image;
	int v_delta, triangle_position;
	int logo_height;
	if (channel_data->is_open) {
		if (channel_data->channel_changed) {
			triangle = channel_data->owner->open_triangle_changed;
		} else {
			triangle = channel_data->owner->open_triangle;
		}
	} else {
		if (channel_data->channel_changed) {
			triangle = channel_data->owner->closed_triangle_changed;
		} else {
			triangle = channel_data->owner->closed_triangle;
		}
	}	
	
	if (channel_data->logo_image == NULL) {
		logo_height = TITLE_FONT_SIZE;
	} else {
		logo_height = gdk_pixbuf_get_height (channel_data->logo_image);
	}

	mapped_image = triangle;
	if (channel_data->prelight_index == PRELIGHT_TRIANGLE) {
		mapped_image = eel_create_spotlight_pixbuf (triangle);
	}
			
	v_delta = logo_height - gdk_pixbuf_get_height (triangle);
	triangle_position = v_offset + (v_delta / 2);
	pixbuf_composite (mapped_image, pixbuf, 2, triangle_position, 255);

	if (mapped_image != triangle) {
		gdk_pixbuf_unref (mapped_image);
	}
}

/* draw the logo image */
static int
draw_rss_logo_image (RSSChannelData *channel_data,
		     GdkPixbuf *pixbuf,
		     int offset,
		     gboolean measure_only)
{
	char *time_str;
	int logo_width, logo_height;
	int v_offset, pixbuf_width;
	int time_x_pos, time_y_pos;
	GdkPixbuf *mapped_image;
	EelDimensions time_dimensions;
	
	v_offset = offset;
	
	if (channel_data->logo_image != NULL) {
		logo_width = gdk_pixbuf_get_width (channel_data->logo_image);
		logo_height = gdk_pixbuf_get_height (channel_data->logo_image);

		if (!measure_only) {
			draw_triangle (pixbuf, channel_data, v_offset);			
			
			mapped_image = channel_data->logo_image;
			if (channel_data->prelight_index == PRELIGHT_LOGO) {
				mapped_image = eel_create_spotlight_pixbuf (channel_data->logo_image);
			}
			pixbuf_composite (mapped_image, pixbuf, LOGO_LEFT_OFFSET, v_offset, 255);
			if (mapped_image != channel_data->logo_image) {
				gdk_pixbuf_unref (mapped_image);
			}
		}
		v_offset += logo_height + 2;

		/* also, draw the update time in the upper right corner if it fits*/
		if (channel_data->last_update != 0 && !measure_only) {
			time_str = eel_strdup_strftime (_("%I:%M %p"), localtime (&channel_data->last_update));

			time_dimensions = eel_scalable_font_measure_text (channel_data->owner->font, 9, time_str, strlen (time_str));
	
			pixbuf_width = gdk_pixbuf_get_width (pixbuf);
			time_x_pos = pixbuf_width - time_dimensions.width - TIME_MARGIN_OFFSET;
			if (time_x_pos >= LOGO_LEFT_OFFSET + logo_width) {
				time_y_pos = offset + ((gdk_pixbuf_get_height (channel_data->logo_image) - time_dimensions.height) / 2);
				eel_scalable_font_draw_text (channel_data->owner->font, pixbuf, 
							     time_x_pos, time_y_pos,
							     eel_gdk_pixbuf_whole_pixbuf,
							     TIME_FONT_SIZE, time_str, strlen (time_str),
							     EEL_RGB_COLOR_BLACK, EEL_OPACITY_FULLY_OPAQUE);
			}
			g_free (time_str);
		}
	}
	return v_offset;
}

/* draw the title */
static int
draw_rss_title (RSSChannelData *channel_data,
		GdkPixbuf *pixbuf,
		int v_offset,
		gboolean measure_only)
{
	EelDimensions title_dimensions;
	int label_offset;
	gboolean is_prelit;
	
	if (channel_data->title == NULL || channel_data->owner->font == NULL) {
		return v_offset;
	}
	
	/* first, measure the text */
	title_dimensions = eel_scalable_font_measure_text (channel_data->owner->font, 
					     TITLE_FONT_SIZE,
					     channel_data->title, strlen (channel_data->title));
	
	/* if there is no image, draw the disclosure triangle */
	if (channel_data->logo_image == NULL && !measure_only) {
		draw_triangle (pixbuf, channel_data, v_offset);			
		label_offset = LOGO_LEFT_OFFSET;
	} else {
		label_offset = 4;
	}

	is_prelit = channel_data->prelight_index == PRELIGHT_LOGO &&
			channel_data->link_uri != NULL;
	
	/* draw the name into the pixbuf using anti-aliased text */	
	if (!measure_only) {
		eel_scalable_font_draw_text (channel_data->owner->font, pixbuf, 
					  label_offset, v_offset,
					  eel_gdk_pixbuf_whole_pixbuf,
					  18,
					  channel_data->title, strlen (channel_data->title),
					  is_prelit ? EEL_RGBA_COLOR_PACK (0, 0, 128, 255) : EEL_RGB_COLOR_BLACK,
					  EEL_OPACITY_FULLY_OPAQUE);
	}
	
	return v_offset + title_dimensions.height;
}

/* utility to determine if a uri matches the current one */
static gboolean
is_current_uri (News *news_data, const char *candidate_uri)
{
	return eel_strcasecmp (news_data->uri, candidate_uri) == 0;
}

/* draw the items */
static int
draw_rss_items (RSSChannelData *channel_data,
		GdkPixbuf *pixbuf,
		int v_offset,
		gboolean measure_only)
{
	GList *current_item;
	RSSItemData *item_data;
	int bullet_width, bullet_height, font_size;
	int item_index, bullet_alpha;
	int bullet_x_pos, bullet_y_pos;
	guint32 text_color;
	ArtIRect dest_bounds;
	EelSmoothTextLayout *smooth_text_layout;
	EelDimensions text_dimensions;
	EelScalableFont *font;
	GdkPixbuf *bullet;
	
	if (channel_data->owner->bullet) {
		bullet_width = gdk_pixbuf_get_width (channel_data->owner->bullet);
		bullet_height = gdk_pixbuf_get_height (channel_data->owner->bullet);
	} else {
		bullet_width = 0;
		bullet_height = 0;
	}
	
	current_item = channel_data->items;
	item_index = 0;
	
	while (current_item != NULL) {					
		/* draw the text */
		item_data = (RSSItemData*) current_item->data;		
		bullet_alpha = 255;

		if (item_index == channel_data->prelight_index) {
			text_color = EEL_RGBA_COLOR_PACK (0, 0, 128, 255);
			bullet_alpha = 192;
		} else {
			text_color = EEL_RGB_COLOR_BLACK;
		}
		
		if (item_data->new_item && (channel_data->owner->changed_bullet != NULL)) {
			bullet = channel_data->owner->changed_bullet;
		} else {
			bullet = channel_data->owner->bullet;
		}
		
		font_size = ITEM_FONT_SIZE;	
		item_data->item_start_y = v_offset;
		if (is_current_uri (channel_data->owner, item_data->item_url)) {
			font = channel_data->owner->bold_font;
		} else {
			font = channel_data->owner->font;
		}
		
		smooth_text_layout = eel_smooth_text_layout_new
                        (item_data->item_title,
                         eel_strlen (item_data->item_title),
                         font, font_size, TRUE);

		if (channel_data->owner->line_width > RIGHT_ITEM_MARGIN) {
			eel_smooth_text_layout_set_line_wrap_width (smooth_text_layout, channel_data->owner->line_width - RIGHT_ITEM_MARGIN);
		}
		
		text_dimensions = eel_smooth_text_layout_get_dimensions (smooth_text_layout);
		
		if (!measure_only) {			
			dest_bounds.x0 = ITEM_POSITION;
			dest_bounds.y0 = v_offset;
			dest_bounds.x1 = gdk_pixbuf_get_width (pixbuf);
			dest_bounds.y1 = gdk_pixbuf_get_height (pixbuf);

			if (!art_irect_empty (&dest_bounds)) {
				eel_smooth_text_layout_draw_to_pixbuf
					(smooth_text_layout, pixbuf,
		 			 0, 0, dest_bounds, GTK_JUSTIFY_LEFT,
                                         TRUE, text_color,
                                         EEL_OPACITY_FULLY_OPAQUE);
									
				/* draw the bullet */	
				if (bullet != NULL) {
					bullet_x_pos = ITEM_POSITION - bullet_width - 2;
					bullet_y_pos = v_offset + 2;
					pixbuf_composite (bullet, pixbuf,
						  bullet_x_pos, bullet_y_pos, bullet_alpha);
				}	
			}
		}

		gtk_object_unref (GTK_OBJECT (smooth_text_layout));
		
		item_data->item_end_y = item_data->item_start_y + text_dimensions.height;
		v_offset += text_dimensions.height + 4;

		item_index += 1;
		current_item = current_item->next;

		/* only allow a fixed number of items, max */
		if (item_index >= channel_data->owner->max_item_count) {
			break;
		}
	}
	return v_offset; 
}

/* draw a single channel */
static int
nautilus_news_draw_channel (News *news_data,
			    RSSChannelData *channel,
			    int v_offset,
			    gboolean measure_only)
{
	channel->logo_start_y = v_offset;
	v_offset = draw_rss_logo_image (channel, news_data->pixbuf, v_offset, measure_only);
	
	if (news_data->always_display_title || channel->logo_image == NULL) {
		v_offset = draw_rss_title (channel, news_data->pixbuf, v_offset, measure_only);
	}
	
	channel->logo_end_y = v_offset;
	v_offset += LOGO_GAP_SIZE;
	
	channel->items_start_y = v_offset;
	if (channel->is_open) {
		v_offset = draw_rss_items (channel, news_data->pixbuf, v_offset, measure_only);
	}
	channel->items_end_y = v_offset;
	return v_offset;
}

/* main routine to render the channel list into the display pixbuf */
static int
nautilus_news_update_display (News *news_data, gboolean measure_only)
{
	int width, height, v_offset;
	GList *channel_item;
	RSSChannelData *channel_data;
	
	v_offset = INITIAL_Y_OFFSET;
	
	if (news_data->pixbuf == NULL && !measure_only) {
		return v_offset;
	}
	
	/* don't draw if too small */
	if (!measure_only) {
		width = gdk_pixbuf_get_width (news_data->pixbuf);
		height = gdk_pixbuf_get_height (news_data->pixbuf);
	
		/* don't draw when too small, like during size negotiation */
		if ((width < MINIMUM_DRAW_SIZE || height < MINIMUM_DRAW_SIZE) && !measure_only) {
			return v_offset;
		}
		
		eel_gdk_pixbuf_fill_rectangle_with_color (news_data->pixbuf,
							  eel_gdk_pixbuf_whole_pixbuf,
							  NEWS_BACKGROUND_RGBA);	
	}
	
	/* loop through the channel list, drawing one channel at a time */
	channel_item = news_data->channel_list;
	while (channel_item != NULL) {	
		channel_data = (RSSChannelData*) channel_item->data;
		if (channel_data->is_showing) {
		
			v_offset = nautilus_news_draw_channel (news_data, 
						       channel_data,
						       v_offset, measure_only);
			if (channel_data->is_open) {
				v_offset += CHANNEL_GAP_SIZE;
			}
		}
		channel_item = channel_item->next;
	}	
	return v_offset;
}

/* allocate the pixbuf to draw into */
static gint
nautilus_news_configure_event (GtkWidget *widget, GdkEventConfigure *event, News *news_data )
{
	if (news_data->pixbuf != NULL) {
		gdk_pixbuf_unref (news_data->pixbuf);
	}
		
	news_data->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
					    widget->allocation.width, widget->allocation.height);
	
	eel_gdk_pixbuf_fill_rectangle_with_color (news_data->pixbuf,
						  eel_gdk_pixbuf_whole_pixbuf,
						  NEWS_BACKGROUND_RGBA);	
	return TRUE;
}

/* handle the news display drawing */
static gint
nautilus_news_expose_event( GtkWidget *widget, GdkEventExpose *event, News *news_data )
{
	int pixbuf_width, pixbuf_height;
	
	nautilus_news_update_display (news_data, FALSE);
	
	pixbuf_width = gdk_pixbuf_get_width (news_data->pixbuf);
	pixbuf_height = gdk_pixbuf_get_height (news_data->pixbuf);
		
	gdk_pixbuf_render_to_drawable_alpha (news_data->pixbuf,
					widget->window,
					0, 0,
					widget->allocation.x, widget->allocation.y,
					pixbuf_width, pixbuf_height,
					GDK_PIXBUF_ALPHA_BILEVEL, 128,
					GDK_RGB_DITHER_MAX,
					0, 0);
	return FALSE;
}

/* utility to set the prelight index of a channel and redraw if necessary */
static void
nautilus_news_set_prelight_index (RSSChannelData *channel_data, int new_prelight_index)
{
	if (channel_data->prelight_index != new_prelight_index) {
		channel_data->prelight_index = new_prelight_index;
		gtk_widget_queue_draw (GTK_WIDGET (channel_data->owner->news_display));				
	}
}


/* utility routine to tell Nautilus to navigate to the passed-in uri */
static void
go_to_uri (News* news_data, const char* uri)
{
	if (uri != NULL) {
		nautilus_view_open_location_in_this_window (news_data->view, uri);
	}
}

/* utility routine to toggle the open state of the passed in channel */
static void
toggle_open_state (RSSChannelData *channel_data)
{
	channel_data->is_open = !channel_data->is_open;
	if (!channel_data->is_open) {
		clear_channel_changed_flags (channel_data);
	}
	
	update_size_and_redraw (channel_data->owner);	
	nautilus_news_save_channel_state (channel_data->owner);
}

/* handle item hit testing */
static int
item_hit_test (RSSChannelData *channel_data, int y_pos)
{
	RSSItemData *item_data;
	GList *next_item;
	int item_index;
	
	item_index = 0;
	next_item = channel_data->items;
	while (next_item != NULL) {
		item_data = (RSSItemData*) next_item->data;
		if (y_pos >= item_data->item_start_y && y_pos <= item_data->item_end_y) {
			return item_index;
		}
		item_index += 1;
		next_item = next_item->next;
	}
	return -1;
}

/* handle the news display hit-testing */
static gint
nautilus_news_button_release_event (GtkWidget *widget, GdkEventButton *event, News *news_data )
{
	GList *current_channel;
	GList *selected_item;
	RSSChannelData *channel_data;
	RSSItemData *item_data;
	int which_item;

	/* we only respond to the first button */
	if (event->button != 1) {
		return FALSE;
	}
	
	/* loop through all of the channels */
	current_channel = news_data->channel_list;
	while (current_channel != NULL) {
		channel_data = (RSSChannelData*) current_channel->data;	
		
		/* if the channel isn't showing, skip all this */
		if (!channel_data->is_showing) {
			current_channel = current_channel->next;
			continue;
		}
		
		/* see if the mouse went down in this channel */
		if (event->y >= channel_data->logo_start_y && event->y <= channel_data->items_end_y) {
			
			/* see if the user clicked on the logo or title area */
			if (event->y <= channel_data->logo_end_y) {
				/* distinguish between the disclosure triangle area and the logo proper */
				if (event->x < DISCLOSURE_RIGHT_POSITION) {
					toggle_open_state (channel_data);
				} else {
					go_to_uri (news_data, channel_data->link_uri);
				}
				return TRUE;
			}
			
			
			/* if it's open, determine which item was clicked */
			if (channel_data->is_open && event->y >= channel_data->items_start_y) {
				which_item = item_hit_test (channel_data, event->y);		
				if (which_item < (int) g_list_length (channel_data->items)) {
					selected_item = g_list_nth (channel_data->items, which_item);
					item_data = (RSSItemData*) selected_item->data;
					go_to_uri (news_data, item_data->item_url);
					item_data->new_item = FALSE;
					return TRUE;
				}
			}				
		}
		current_channel = current_channel->next;
	}
	return TRUE;
}

/* handle motion notify events by prelighting as appropriate */
static gint
nautilus_news_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, News *news_data )
{
	GList *current_channel;
	RSSChannelData *channel_data;
	int which_item;
	int prelight_value;
	
	/* loop through all of the channels to find the one the mouse is over */
	current_channel = news_data->channel_list;
	while (current_channel != NULL) {
		channel_data = (RSSChannelData*) current_channel->data;	

		/* if the channel isn't showing, skip hit-test */
		if (!channel_data->is_showing) {
			current_channel = current_channel->next;
			continue;
		}

		/* see if it's in the items for this channel */
		if (event->y >= channel_data->items_start_y && event->y <= channel_data->items_end_y) {
			which_item = item_hit_test (channel_data, event->y);
			if (which_item < (int) g_list_length (channel_data->items)) {
				nautilus_news_set_prelight_index (channel_data, which_item);
				return TRUE;
			}
		} else {
			if (event->y >= channel_data->logo_start_y && event->y <= channel_data->logo_end_y) {
				if (event->x < DISCLOSURE_RIGHT_POSITION) {
					prelight_value = PRELIGHT_TRIANGLE;
				} else {
					prelight_value = PRELIGHT_LOGO;
				}
			} else {			
				prelight_value = -1;
			}
			nautilus_news_set_prelight_index (channel_data, prelight_value);
		}
						
		current_channel = current_channel->next;
	}
	return TRUE;
}

/* handle leave notify events by turning off any prelighting */
static gint
nautilus_news_leave_notify_event (GtkWidget *widget, GdkEventMotion *event, News *news_data )
{
	GList *current_channel;
	RSSChannelData *channel_data;
	
	/* loop through all of the channels to turn off prelighting */
	current_channel = news_data->channel_list;
	while (current_channel != NULL) {
		channel_data = (RSSChannelData*) current_channel->data;	
		nautilus_news_set_prelight_index (channel_data, -1);
		current_channel = current_channel->next;
	}
	return TRUE;
}

static void
nautilus_news_set_title (RSSChannelData *channel_data, const char *title)
{
	if (eel_strcmp (channel_data->title, title) == 0) {
		return;
	}
	
	if (channel_data->title) {
		g_free (channel_data->title);
	}
	if (title != NULL) {
		channel_data->title = g_strdup (title);
	} else {
		channel_data->title = NULL;	
	}
}

static void
free_rss_data_item (RSSItemData *item)
{
	g_free (item->item_title);
	g_free (item->item_url);
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
	g_free (channel_data->link_uri);
	g_free (channel_data->title);
	
	if (channel_data->logo_image != NULL) {
		gdk_pixbuf_unref (channel_data->logo_image);
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
                
		bonobo_property_bag_notify_listeners (news_data->property_bag,
                                                      "tab_image", tab_image_arg, NULL);
                
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
		current_item = current_item->next;
	}
	channel_data->channel_changed = FALSE;
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
	
	if (channel_data->logo_image) {
		gdk_pixbuf_unref (channel_data->logo_image);
		channel_data->logo_image = NULL;
	}
	
	if (pixbuf != NULL) {
		gdk_pixbuf_ref (pixbuf);
		pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, 192, 40);		
		channel_data->logo_image = pixbuf;
		queue_update_size_and_redraw (channel_data->owner);
	}
}

/* routine to ellipsize the passed in string at a word boundary based on the string length. */
static  char*
ellipsize_string (const char *raw_text)
{
	char *result, *last_char_ptr;
	int truncated_length;
	
	if (raw_text == NULL) {
		return NULL;
	}
	
	if (strlen (raw_text) > MAX_CHARS_IN_ITEM) {
		truncated_length = MAX_CHARS_IN_ITEM;
		last_char_ptr = (char*) raw_text + MAX_CHARS_IN_ITEM;
		while (*last_char_ptr != '\0' && *last_char_ptr != ' ' && *last_char_ptr != '.') {
			last_char_ptr += 1;
			truncated_length += 1;
		}
		result = g_malloc (truncated_length + strlen (ELLIPSIS) + 1);
		memcpy (result, raw_text, truncated_length);
		strcpy (result + truncated_length, ELLIPSIS);

		return result;
	} else {
		return g_strdup (raw_text);
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
	
	current_node = container_node->childs;
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

				title = xmlNodeGetContent (title_node);
				item_parameters->item_title = ellipsize_string (title);
				xmlFree (title);
				
				temp_node = eel_xml_get_child_by_name (current_node, "link");
				if (temp_node) {
					if (scripting_news_format) {
						temp_node = eel_xml_get_child_by_name (temp_node, "url");		
					}		
					temp_str = xmlNodeGetContent (temp_node);
					item_parameters->item_url = g_strdup (temp_str);
					xmlFree (temp_str);	
				}
				
				if (item_parameters->item_title != NULL && item_parameters->item_url != NULL) {
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

/* utility routine to resize the news display and redraw it */
static void
update_size_and_redraw (News* news_data)
{
	int display_size;
	
	display_size = nautilus_news_update_display (news_data, TRUE);
	if (display_size != news_data->news_display->allocation.height) {
		gtk_widget_set_usize (news_data->news_display, -1, display_size);
		gtk_widget_queue_resize (news_data->news_display);				
	}
	gtk_widget_queue_draw (news_data->news_display);
}

/* handle the timeout firing by updating and redrawing */
static int
update_timeout_callback (gpointer callback_data)
{
	News *news_data;
	news_data = (News*) callback_data;

	news_data->update_timeout = -1;	
	update_size_and_redraw (news_data);
	return 0;
}

/* utility routine to queue an update for the future, so many quick ones get coalesced */
static void
queue_update_size_and_redraw (News* news_data)
{
	/* if there already is one pending, simply return */
	if (news_data->update_timeout > 0) {
		return;
	}
	
	news_data->update_timeout = gtk_timeout_add (1500, update_timeout_callback, news_data);
}

/* utility routine to search for the passed-in url in an item list */
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
		mapped_item_uri = gnome_vfs_make_uri_canonical (item_data->item_url);
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
		if (!has_matching_uri (old_items, item_data->item_url, &old_changed_flag) && !channel_data->initial_load_flag) {
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
					g_free (channel_data->link_uri);
					channel_data->link_uri = g_strdup (temp_str);
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
				g_free (channel_data->link_uri);
				channel_data->link_uri = g_strdup (temp_str);
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
				channel_data->load_image_handle = eel_gdk_pixbuf_load_async (image_uri, rss_logo_callback, channel_data);
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
				channel_data->load_image_handle = eel_gdk_pixbuf_load_async (image_uri, rss_logo_callback, channel_data);
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

	/* Parse the rss file with gnome-xml. The gnome-xml parser requires a zero-terminated array. */
	buffer = g_realloc (file_contents, file_size + 1);
	buffer[file_size] = '\0';
	rss_document = xmlParseMemory (buffer, file_size);
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
	if (channel_data->logo_image == NULL && channel_data->load_image_handle == NULL) {
		if (!extract_rss_image (channel_data, rss_document)) {
			extract_scripting_news_image (channel_data, rss_document);
		}	
	}
				
	/* extract the items */
	old_items = channel_data->items;
	channel_data->items = NULL;
		
	current_node = rss_document->root;
	item_count = extract_items (channel_data, current_node);
	
	/* if we couldn't find any items at the main level, look inside the channel node */
	if (item_count == 0 && channel_node != NULL) {
		item_count = extract_items (channel_data, channel_node);
	}
	
	changed_count = mark_new_items (channel_data, old_items);
		
	/* we're done, so free everything up */
	eel_g_list_free_deep_custom (old_items, (GFunc) free_rss_data_item, NULL);
	xmlFreeDoc (rss_document);
	channel_data->update_in_progress = FALSE;
	channel_data->initial_load_flag = FALSE;
	
	/* update the size of the display area to reflect the new content and
	 * schedule a redraw.
	 */
	if (changed_count > 0) {
		queue_update_size_and_redraw (channel_data->owner); 
	}
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
	channel_data->load_file_handle = eel_read_entire_file_async (channel_data->uri, rss_read_done_callback, channel_data);
	
	/* put up a title that's displayed while we wait */
	title = g_strdup_printf (_("Loading %s"), channel_data->name);
	nautilus_news_set_title (channel_data, title);
	g_free (title);
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
 	channel_data->prelight_index = -1;
	channel_data->is_open = is_open;
	channel_data->is_showing = is_showing;
	channel_data->initial_load_flag = TRUE;
	
	if (channel_data->is_showing) {
 		nautilus_news_load_channel (news_data, channel_data);
 	}
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
	current_channel = xmlDocGetRootElement (channels)->childs;
	while (current_channel != NULL) {
		if (eel_strcmp (current_channel->name, "rss_channel") == 0) { 				
			name = xmlGetProp (current_channel, "name");
			uri = xmlGetProp (current_channel, "uri");
			open_str = xmlGetProp (current_channel, "open");
			show_str = xmlGetProp (current_channel, "show");
		
			if (uri != NULL) {
				is_open = eel_strcasecmp (open_str, "true") == 0;
				is_showing = eel_strcasecmp (show_str, "true") == 0;
				
				channel_data = nautilus_news_make_new_channel (news_data, name, uri,
									       is_open, is_showing);
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
	xml_path = nautilus_make_path (user_directory,
				       file_name);
	g_free (user_directory);
	if (force_local || g_file_exists (xml_path)) {
		return xml_path;
	}
	g_free (xml_path);
	
	/* next try the shared directory */
	xml_path = nautilus_make_path (NAUTILUS_DATADIR,
				       file_name);
	if (g_file_exists (xml_path)) {
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

/* utility to load an xpm image */
static void
load_xpm_image (GdkPixbuf** image_result, const char** image_name)
{
	if (*image_result != NULL) {
		gdk_pixbuf_unref (*image_result);
	}
	*image_result = gdk_pixbuf_new_from_xpm_data (image_name);
}

/* utility routine to load images needed by the news view */
static void
nautilus_news_load_images (News *news_data)
{
	char *news_bullet_path;
	
	load_xpm_image (&news_data->closed_triangle, (const char**) triangle_xpm);
	load_xpm_image (&news_data->closed_triangle_changed, (const char**) triangle_changed_xpm);
	load_xpm_image (&news_data->open_triangle, (const char**) open_triangle_xpm);
	load_xpm_image (&news_data->open_triangle_changed, (const char**) open_triangle_changed_xpm);
	
	if (news_data->bullet != NULL) {
		gdk_pixbuf_unref (news_data->bullet);
	}
	
	news_bullet_path = nautilus_theme_get_image_path ("news_bullet.png");	
	if (news_bullet_path != NULL) {
		news_data->bullet = gdk_pixbuf_new_from_file (news_bullet_path);
		g_free (news_bullet_path);
	}

	if (news_data->changed_bullet != NULL) {
		gdk_pixbuf_unref (news_data->changed_bullet);
	}
	
	news_bullet_path = nautilus_theme_get_image_path ("changed_bullet.png");	
	if (news_bullet_path != NULL) {
		news_data->changed_bullet = gdk_pixbuf_new_from_file (news_bullet_path);
		g_free (news_bullet_path);
	}

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
	update_size_and_redraw (news);
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
	if (news->configure_mode) {
		gtk_widget_show_all (news->configure_box);
		gtk_widget_hide_all (news->main_box);
		gtk_widget_hide_all (news->edit_site_box);
	} else {
		gtk_widget_show_all (news->main_box);
		gtk_widget_hide_all (news->configure_box);
		gtk_widget_hide_all (news->edit_site_box);
	
		if (count_visible_channels (news) == 0) {
			gtk_widget_hide (news->news_display_scrolled_window);
		} else {
			gtk_widget_hide (news->empty_message);
		}
	}
}

/* here's the button callback routine that toggles between display modes  */
static void
configure_button_clicked (GtkWidget *widget, News *news)
{
	news->configure_mode = !news->configure_mode;
	set_views_for_mode (news);
	if (!news->configure_mode) {
		/* when exiting configure mode, update everything */		
		nautilus_news_save_channel_state (news);		
		update_size_and_redraw (news);
		check_for_updates (news);
	}
}

/* here's the button callback routine that handles the add new site button
 * by showing the relevant widgets.
 */
static void
add_site_button_clicked (GtkWidget *widget, News *news)
{
	news->configure_mode = FALSE;
	gtk_widget_hide_all (news->configure_box);
	gtk_widget_show_all (news->edit_site_box);
}

/* utility to add an entry to the remove channel clist */
static void
add_channel_to_remove_list (News *news_data, const char *channel_name)
{
	char* entry[1];
	
	entry[0] = g_strdup (channel_name);
	gtk_clist_append ( GTK_CLIST (news_data->remove_site_list), entry);
}

static void
update_remove_button (News *news)
{
	gtk_widget_set_sensitive (news->remove_button, news->channel_list != NULL);
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
	
	site_name = gtk_entry_get_text (GTK_ENTRY (news->item_name_field));
	site_location = gtk_entry_get_text (GTK_ENTRY (news->item_location_field));

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
	configure_button_clicked (widget, news);
}

/* handle the remove command  */
static void
remove_selected_site (GtkWidget *widget, News *news)
{
	RSSChannelData *channel_data;
	GList *channel_item;
	char *channel_name;

	gtk_clist_get_text (GTK_CLIST (news->remove_site_list),
			    news->remove_selection_index, 0,
			    &channel_name);
	
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
	
	/* remove the channel from the remove list */
	gtk_clist_remove (GTK_CLIST (news->remove_site_list), news->remove_selection_index);
	update_remove_button (news);
	
	/* back to configure mode */
	configure_button_clicked (widget, news);
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
  	gtk_container_add (GTK_CONTAINER (frame), button_box);

  	/* Set the appearance of the Button Box */
  	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (button_box), 4);
  	gtk_button_box_set_child_size (GTK_BUTTON_BOX (button_box), 24, 14);
	
	if (from_configure) {
		button = gtk_button_new_with_label (_("Edit"));
		gtk_container_add (GTK_CONTAINER (button_box), button);

		gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) add_site_button_clicked, news_data);
	}
	
	button = gtk_button_new_with_label (label);
	gtk_container_add (GTK_CONTAINER (button_box), button);

	gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) configure_button_clicked, news_data);
		      	
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
	channel_name = gtk_object_get_data (GTK_OBJECT (toggle_button), "channel_name");
	
	channel_data = get_channel_from_name (news_data, channel_name);
	if (channel_data != NULL) { 
		channel_data->is_showing = !channel_data->is_showing;
		if (channel_data->is_showing) {
			channel_data->is_open = TRUE;
		}
	}	
}

/* callback to maintain the current location */
static void
nautilus_news_load_location (NautilusView *view, const char *location, News *news)
{
	g_free (news->uri);
	news->uri = g_strdup (location);
	
	/* only do work if we're open */
	if (news->opened) {
		update_size_and_redraw (news);
	}
}

/* utility routine to determine the sort position of a checkbox */
static int
determine_sort_position (GtkWidget *container, const char *name)
{
	GList *checkboxes, *current_item;
	char *current_name;
	int index;
	
	checkboxes = gtk_container_children (GTK_CONTAINER (container));
	index = 0;
	current_item = checkboxes;
	while (current_item != NULL) {
		current_name = gtk_object_get_data (GTK_OBJECT (current_item->data), "channel_name");
		
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
	gtk_box_pack_start (GTK_BOX (news_data->checkbox_list), check_button, FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (check_button), "toggled",
      	              	    			GTK_SIGNAL_FUNC (check_button_toggled_callback),
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
        gtk_object_set_user_data (GTK_OBJECT (check_button), news_data);
	gtk_object_set_data_full (GTK_OBJECT (check_button),
				  "channel_name",
				  g_strdup (channel_name),
				  (GtkDestroyNotify) g_free);
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
	current_channel = xmlDocGetRootElement (channel_doc)->childs;
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

/* when the empty message is resized, adjust its wrap width */
static void
empty_message_size_allocate (GtkWidget *widget, GtkAllocation *allocation, News *news_data)
{
	int wrap_width;
	
	wrap_width = allocation->width - 2*EMPTY_MESSAGE_MARGIN;
	if (wrap_width > 0) {
		eel_label_set_smooth_line_wrap_width (EEL_LABEL (widget), allocation->width - 2*EMPTY_MESSAGE_MARGIN);
	}
}

/* handle resizing the news display by recalculating our size if necessary */
static void
news_display_size_allocate (GtkWidget *widget, GtkAllocation *allocation, News *news_data)
{
	int old_line_width, old_height;
	old_line_width = news_data->line_width;
	old_height = news_data->display_height;
	
	news_data->line_width = allocation->width - ITEM_POSITION; 
	news_data->display_height = allocation->height; 
	
	if (old_line_width != news_data->line_width || old_height != news_data->display_height) {
		update_size_and_redraw (news_data);
	}
}


/* code-saving utility to allocate a left-justified anti-aliased label */
static GtkWidget *
news_label_new (const char *label_text, gboolean title_mode)
{
	GtkWidget *label;
	
	label = gtk_label_new (label_text);
	if (title_mode) {
		eel_gtk_label_make_bold (GTK_LABEL (label));
	}
	
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	return label;	
}

static void
select_row_in_remove_list (GtkCList *clist, gint row, gint column,
			   GdkEventButton *event, News* news)
{
	news->remove_selection_index = row;
}

/* generate the remove widgets */
static void
make_remove_widgets (News *news, GtkWidget *container)
{
	GtkWidget *button_box;
	GtkScrolledWindow *scrolled_window;
	
	news->remove_site_list = gtk_clist_new (1);

	gtk_clist_column_titles_hide (GTK_CLIST (news->remove_site_list));
	gtk_clist_set_column_width (GTK_CLIST (news->remove_site_list), 0, 108);
	gtk_clist_set_selection_mode (GTK_CLIST (news->remove_site_list), GTK_SELECTION_BROWSE);		
	gtk_clist_set_auto_sort (GTK_CLIST (news->remove_site_list), TRUE);
	
	gtk_signal_connect (GTK_OBJECT (news->remove_site_list), "select_row",
			    GTK_SIGNAL_FUNC (select_row_in_remove_list), news);
			    
	scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);   
	gtk_container_add (GTK_CONTAINER (scrolled_window), news->remove_site_list);
	gtk_box_pack_start (GTK_BOX (container), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);

	/* install the remove button */
    	button_box = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (container), button_box, FALSE, FALSE, 4);
 	
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (button_box), 4);
  	gtk_button_box_set_child_size (GTK_BUTTON_BOX (button_box), 24, 14);
	
	news->remove_button = gtk_button_new_with_label (_("Remove Site"));
	gtk_container_add (GTK_CONTAINER (button_box), news->remove_button);
	gtk_signal_connect (GTK_OBJECT (news->remove_button), "clicked",
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
	gtk_box_pack_start (GTK_BOX (container), temp_vbox, FALSE, FALSE, 0);

	/* allocate the name field */
	label = news_label_new (_("Site Name:"), FALSE);
	gtk_box_pack_start (GTK_BOX (temp_vbox), label, FALSE, FALSE, 0);

	news->item_name_field = nautilus_entry_new ();
	gtk_box_pack_start (GTK_BOX (temp_vbox), news->item_name_field, FALSE, FALSE, 0);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (news->item_name_field), TRUE);
	
	/* allocate the location field */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (container), temp_vbox, FALSE, FALSE, 0);

	label = news_label_new (_("Site RSS URL:"), FALSE);
	gtk_box_pack_start (GTK_BOX (temp_vbox), label, FALSE, FALSE, 0);

	news->item_location_field = nautilus_entry_new ();
	gtk_box_pack_start (GTK_BOX (temp_vbox), news->item_location_field, FALSE, FALSE, 0);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (news->item_location_field), TRUE);
	
	/* install the add buttons */
    	button_box = gtk_hbutton_box_new ();
	gtk_box_pack_start (GTK_BOX (container), button_box, FALSE, FALSE, 4);
 	
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing (GTK_BUTTON_BOX (button_box), 4);
  	gtk_button_box_set_child_size (GTK_BUTTON_BOX (button_box), 24, 14);
	
	button = gtk_button_new_with_label (_("Add New Site"));
	gtk_container_add (GTK_CONTAINER (button_box), button);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
		(GtkSignalFunc) add_site_from_fields, news);
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
	gtk_box_pack_start (GTK_BOX (container), news->edit_site_box, TRUE, TRUE, 0);

	expand_box = gtk_vbox_new (FALSE, 0);	
	gtk_box_pack_start (GTK_BOX (news->edit_site_box), expand_box, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (expand_box), 4);

	/* make the add new site label */
	label = news_label_new (_("Add a New Site:"), TRUE);
	gtk_box_pack_start (GTK_BOX (expand_box), label, FALSE, FALSE, 0);
	
	/* allocate the add new site widgets */
	make_add_widgets (news, expand_box);
	
	/* allocate the remove label */
	temp_vbox = gtk_vbox_new (FALSE, 0);
	
	label = news_label_new (_("Remove a Site:"), TRUE);
	gtk_box_pack_start (GTK_BOX (temp_vbox), label, FALSE, FALSE, 0);
	
	/* allocate the remove widgets */
	make_remove_widgets (news, temp_vbox);
	gtk_box_pack_start (GTK_BOX (expand_box), temp_vbox, TRUE, TRUE, 0);
	
	/* add the button box at the bottom with a cancel button */
	button_box = add_command_buttons (news, _("Cancel"), FALSE);
	gtk_box_pack_start (GTK_BOX (news->edit_site_box), button_box, FALSE, FALSE, 0);	
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
	gtk_box_pack_start (GTK_BOX (container), news->configure_box, TRUE, TRUE, 0);

	/* add a descriptive label */
	label = news_label_new (_("Select Sites:"), TRUE);
	gtk_box_pack_start (GTK_BOX (news->configure_box), label, FALSE, FALSE, 0);
	
	/* allocate a table to hold the check boxes */
	news->checkbox_list = gtk_vbox_new (FALSE, 0);
	
	scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);   
	viewport = gtk_viewport_new (gtk_scrolled_window_get_hadjustment (scrolled_window),
			  		gtk_scrolled_window_get_vadjustment (scrolled_window));
	gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);
	gtk_container_add (GTK_CONTAINER (viewport), news->checkbox_list);
 	gtk_box_pack_start (GTK_BOX (news->configure_box), GTK_WIDGET (scrolled_window), TRUE, TRUE, 0);
		
	/* allocate the button box for the done button */
        button_box = add_command_buttons (news, _("Done"), TRUE);
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
        gtk_box_pack_start (GTK_BOX (container), news->main_box, TRUE, TRUE, 0);
	
        /* create and install the display area */               
        news->news_display = gtk_drawing_area_new ();
 	
	/* put the display in a scrolled window so it can scroll */
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), news->news_display);
	gtk_box_pack_start (GTK_BOX (news->main_box), scrolled_window, TRUE, TRUE, 0);
 	news->news_display_scrolled_window = scrolled_window;
	
	/* add the empty message */
	news->empty_message = eel_label_new (_("The News panel displays current headlines from your favorite websites.  Click the \'Select Sites\' button to select the sites to display."));
	eel_label_set_smooth_font_size (EEL_LABEL (news->empty_message), 14);
	eel_label_set_justify (EEL_LABEL (news->empty_message), GTK_JUSTIFY_LEFT);
	eel_label_set_wrap (EEL_LABEL (news->empty_message), TRUE);	

	gtk_box_pack_start (GTK_BOX (news->main_box), news->empty_message, TRUE,
		TRUE, 0);
	
 	/* connect the appropriate signals for drawing and event handling */
 	gtk_signal_connect (GTK_OBJECT (news->news_display), "expose_event",
		      (GtkSignalFunc) nautilus_news_expose_event, news);
  	gtk_signal_connect (GTK_OBJECT(news->news_display),"configure_event",
		      (GtkSignalFunc) nautilus_news_configure_event, news);

  	gtk_signal_connect (GTK_OBJECT (news->news_display), "motion_notify_event",
		      (GtkSignalFunc) nautilus_news_motion_notify_event, news);
  	gtk_signal_connect (GTK_OBJECT (news->news_display), "leave_notify_event",
		      (GtkSignalFunc) nautilus_news_leave_notify_event, news);
  	gtk_signal_connect (GTK_OBJECT (news->news_display), "button_release_event",
		      (GtkSignalFunc) nautilus_news_button_release_event, news);

  	gtk_widget_set_events (news->news_display, GDK_EXPOSURE_MASK
			 | GDK_LEAVE_NOTIFY_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_BUTTON_RELEASE_MASK);
 	gtk_widget_add_events (news->news_display, GDK_POINTER_MOTION_MASK);

        /* create a button box to hold the command buttons */
        button_box = add_command_buttons (news, _("Select Sites"), FALSE);
        gtk_box_pack_start (GTK_BOX (news->main_box), button_box, FALSE, FALSE, 0); 
}

static NautilusView *
make_news_view (const char *iid, gpointer callback_data)
{
	News *news;
	GtkWidget *main_container;
	
	/* create the private data for the news view */         
        news = g_new0 (News, 1);

	/* allocate the main container */
	main_container = gtk_vbox_new (FALSE, 0);

	/* set up the widgets for the main,configure and add modes */
	set_up_main_widgets (news, main_container);
	set_up_configure_widgets (news, main_container);
	set_up_edit_widgets (news, main_container);
		
	/* set up the fonts */
 	news->font = eel_scalable_font_get_default_font ();
 	news->bold_font = eel_scalable_font_get_default_bold_font ();
       	
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
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS, max_items_changed, news);	
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL, update_interval_changed, news);
	
	/* load some images */
	nautilus_news_load_images (news);

	/* set up the update timeout */
	news->timer_task = gtk_timeout_add (10000, check_for_updates, news);

	/* arrange for notification when we're resized */
	gtk_signal_connect (GTK_OBJECT (news->news_display), "size_allocate", news_display_size_allocate, news);
	gtk_signal_connect (GTK_OBJECT (news->empty_message), "size_allocate", empty_message_size_allocate, news);
		
	/* Create the nautilus view CORBA object. */
        news->view = nautilus_view_new (main_container);
        gtk_signal_connect (GTK_OBJECT (news->view), "destroy", do_destroy, news);

	gtk_signal_connect (GTK_OBJECT (news->view), "load_location",
                            nautilus_news_load_location, news);

	/* allocate a property bag to reflect the TAB_IMAGE property */
	news->property_bag = bonobo_property_bag_new (get_bonobo_properties,  set_bonobo_properties, news);
	bonobo_control_set_properties (nautilus_view_get_bonobo_control (news->view), news->property_bag);
	bonobo_property_bag_add (news->property_bag, "tab_image", TAB_IMAGE, BONOBO_ARG_STRING, NULL,
				 _("image indicating that the news has changed"), 0);
	bonobo_property_bag_add (news->property_bag, "close", CLOSE_NOTIFY,
				 BONOBO_ARG_BOOLEAN, NULL, "close notification", 0);
 	
	nautilus_news_clear_changed_flags (news);
 	
        /* read the channel definition file and start loading the channels */
	read_channel_list (news);
 
 	/* populate the configuration list */
	add_channels_to_lists (news);
	update_remove_button (news);

        /* default to the main mode */
	gtk_widget_show (main_container);
	set_views_for_mode (news);

  	/* return the nautilus view */    
        return news->view;
}

int
main(int argc, char *argv[])
{
	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger
			(G_LOG_DOMAIN, g_log_domain_glib, "Gdk", "Gtk", "GnomeVFS", "GnomeUI", "Bonobo", NULL);
	}
	
        return nautilus_view_standard_main ("nautilus-news",
                                            VERSION,
                                            PACKAGE,
                                            GNOMELOCALEDIR,
                                            argc,
                                            argv,
                                            "OAFIID:nautilus_news_view_factory:041601",
                                            "OAFIID:nautilus_news_view:041601",
                                            make_news_view,
                                            nautilus_global_preferences_initialize,
                                            NULL);
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
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
 * This is the implementation for the nautilus about dialog
 *
 */

#include <config.h>
#include "nautilus-about.h"

#include <libgnome/gnome-defs.h>

#include <math.h>
#include <gnome.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-pixmap.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-theme.h>

struct NautilusAboutDetails {
	GtkWidget *drawing_area;
	GdkPixbuf *background_pixbuf;	
	int	  last_update_time;
	int	  timer_task;
	char	  **authors;
	int	  *order_array;
};

static gboolean nautilus_about_close            (NautilusAbout       *about,
						 gpointer            *unused);
static void     nautilus_about_initialize_class (NautilusAboutClass  *klass);
static void     nautilus_about_initialize       (NautilusAbout       *about);
static void     nautilus_about_destroy          (GtkObject           *object);
static void     nautilus_about_repaint          (GtkWidget           *drawing_area,
						 GdkEventExpose      *event,
						 NautilusAbout       *about);
static void     nautilus_about_draw_info        (NautilusAbout       *about,
						 const char          *title,
						 const char          *version,
						 const char          *copyright,
						 const char         **authors,
						 const char          *comments,
						 const char	     *translators,
						 const char          *time_stamp);
static int      update_authors_if_necessary     (gpointer             callback_data);

/* author box layout definitions */
#define	AUTHOR_TOP_POS 88
#define	AUTHOR_LEFT_POS 200
#define	AUTHOR_COLUMN_WIDTH 140
#define AUTHOR_LINE_HEIGHT 15
#define ITEMS_PER_COLUMN 11

/* delay between randomizing, in seconds  */
#define UPDATE_TIME_INTERVAL 8

/* gtk class definition boilerplate */
EEL_DEFINE_CLASS_BOILERPLATE (NautilusAbout, nautilus_about, GNOME_TYPE_DIALOG)

static void
nautilus_about_initialize_class (NautilusAboutClass *about_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (about_class);
		
	object_class->destroy = nautilus_about_destroy;
}

static void 
nautilus_about_destroy (GtkObject *object)
{
	NautilusAbout *about;
	
	about = NAUTILUS_ABOUT (object);
	
	if (about->details->background_pixbuf != NULL) {
		gdk_pixbuf_unref (about->details->background_pixbuf);
	}
	
	g_strfreev (about->details->authors);
	g_free (about->details->order_array);

	if (about->details->timer_task != 0) {
		gtk_timeout_remove (about->details->timer_task);
	}
	
	g_free (about->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


/* initialize the about */
static void
nautilus_about_initialize (NautilusAbout *about)
{
	char *background_path;
	GtkWidget *frame;
	int area_width, area_height;
	
	about->details = g_new0 (NautilusAboutDetails, 1);

	background_path = nautilus_theme_get_image_path ("about_background.png");
	about->details->background_pixbuf = gdk_pixbuf_new_from_file (background_path);
	g_free (background_path);	

	/* set the window title and standard close key accelerator */
	gtk_window_set_title (GTK_WINDOW (about), _("About Nautilus"));
	gtk_window_set_wmclass (GTK_WINDOW (about), "about", "Nautilus");
	eel_gtk_window_set_up_close_accelerator (GTK_WINDOW (about));
	
	/* allocate a frame to hold the drawing area */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);

	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG(about)->vbox),
			   GTK_WIDGET(frame));
	gtk_widget_show (frame);
	
	/* allocate the drawing area */
	about->details->drawing_area = gtk_drawing_area_new ();
	
	area_width  = gdk_pixbuf_get_width  (about->details->background_pixbuf);
	area_height = gdk_pixbuf_get_height (about->details->background_pixbuf);
	
	gtk_widget_set_usize (GTK_WIDGET (about->details->drawing_area),
			      area_width, area_height);
	gtk_widget_set_events (about->details->drawing_area, GDK_EXPOSURE_MASK);

	gtk_signal_connect (GTK_OBJECT (about->details->drawing_area), "expose_event",
			    nautilus_about_repaint, about);

	gtk_widget_show (about->details->drawing_area);
 	gtk_container_add (GTK_CONTAINER (frame), about->details->drawing_area);

	/* set up the timer task */
	about->details->timer_task = gtk_timeout_add (2000, update_authors_if_necessary, about); 
	       
	/* configure the dialog */                                  
	gnome_dialog_append_button (GNOME_DIALOG(about),
				    GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_set_close (GNOME_DIALOG(about), TRUE);			
	gnome_dialog_close_hides (GNOME_DIALOG(about), TRUE);

	gtk_signal_connect
	        (GTK_OBJECT (about),
	         "close",
	         GTK_SIGNAL_FUNC (nautilus_about_close), 
	         NULL);
}

/* allocate a new about dialog */
GtkWidget *
nautilus_about_new (const char *title,
		    const char *version,
		    const char *copyright,
		    const char **authors,
		    const char *comments,
		    const char *translators,
		    const char *time_stamp)
{
	NautilusAbout *about;
	
	about = NAUTILUS_ABOUT (gtk_widget_new (nautilus_about_get_type (), NULL));
	
	/* draw the info onto the pixbuf, once and for all */
	nautilus_about_draw_info (about, title, version, copyright,
				  authors, comments, translators, time_stamp);
	
	return GTK_WIDGET (about);
}


/* utility to simplify drawing */
static void
draw_pixbuf (GdkPixbuf *pixbuf, GdkDrawable *drawable, int x, int y)
{
	gdk_pixbuf_render_to_drawable_alpha (pixbuf, drawable, 0, 0, x, y,
					     gdk_pixbuf_get_width (pixbuf),
					     gdk_pixbuf_get_height (pixbuf),
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);
}

/* here's the routine that does all the real work, repainting the drawing area */
static void
nautilus_about_repaint (GtkWidget *widget,
			GdkEventExpose *event,
			NautilusAbout *about)
{
	/* draw the background image */
	if (about->details->background_pixbuf != NULL) {
		draw_pixbuf (about->details->background_pixbuf,
			     widget->window, 0, 0);
	}
}

/* utility routine to draw a string at a position */
static void
draw_aa_string (EelScalableFont *font,
		GdkPixbuf *pixbuf,
		int font_size,
		int x_pos,
		int y_pos,
		guint color,
		guint shadow_color,
		const char *text,
		int shadow_offset)
{
	if (shadow_offset != 0) {
		eel_scalable_font_draw_text (font, pixbuf,
					     x_pos + shadow_offset, y_pos + shadow_offset,
					     eel_gdk_pixbuf_whole_pixbuf,
					     font_size,
					     text, strlen (text),
					     shadow_color, EEL_OPACITY_FULLY_OPAQUE);	
	}
	
	eel_scalable_font_draw_text (font, pixbuf, x_pos, y_pos,
				     eel_gdk_pixbuf_whole_pixbuf,
				     font_size,
				     text, strlen (text), color, EEL_OPACITY_FULLY_OPAQUE);
}

/* randomize_authors randomizes the order array so different names get displayed in different positions each time */

static void
randomize_authors (NautilusAbout *about)
{
	int author_count;
	int index, temp;
	int swap_element;

	/* count the authors */	
	author_count = 0;
	while (about->details->authors[author_count] != NULL) {
		author_count += 1;
	}

	/* initialize the order array */
	g_free (about->details->order_array);
	about->details->order_array = g_new (int, author_count);
	for (index = 0; index < author_count; index++) {
		about->details->order_array[index] = index;
	}
	
	/* shuffle the order array */
	for (index = 0; index < author_count - 1; index++) {
		swap_element = rand() % (author_count - index); 
		temp = about->details->order_array[index];
		about->details->order_array[index] = about->details->order_array[swap_element];
		about->details->order_array[swap_element] = temp;
	}
}

/* draw the author list */
static void
draw_author_list (NautilusAbout *about,
		  GdkPixbuf *pixbuf,
		  EelScalableFont *plain_font)
{
	int index, column_count;
	int xpos, ypos;
	
	/* draw the authors */
	index = 0;
	column_count = 0;
	
	xpos = AUTHOR_LEFT_POS; ypos = AUTHOR_TOP_POS;
	while (about->details->authors[about->details->order_array[index]] != NULL) {
		draw_aa_string (plain_font, pixbuf, 12, xpos, ypos,
				EEL_RGB_COLOR_BLACK, EEL_RGB_COLOR_BLACK,
				about->details->authors[about->details->order_array[index]],
				0);
		ypos += AUTHOR_LINE_HEIGHT;
		index++;
		column_count++;
		if (column_count >= ITEMS_PER_COLUMN) {
			/* two columns only and then stop drawing */
			if (xpos != AUTHOR_LEFT_POS)
				break;
			
			column_count = 0;
			ypos = AUTHOR_TOP_POS;
			xpos += AUTHOR_COLUMN_WIDTH;
		}
	}	
}

/* draw the information onto the pixbuf */
static void
nautilus_about_draw_info (NautilusAbout	*about,
			  const char *title,
			  const char *version,
			  const char *copyright,
			  const char **authors,
			  const char *comments,
			  const char *translators,
			  const char *time_stamp)
{
	char *display_str, *temp_str;
	char **comment_array;
	EelScalableFont *plain_font, *bold_font;
	GdkPixbuf *pixbuf;
	uint	black, white, grey;
	int xpos, ypos, total_height;
	int index;

	plain_font = eel_scalable_font_get_default_font ();
	bold_font  = eel_scalable_font_make_bold (plain_font);

	pixbuf = about->details->background_pixbuf;
	total_height = gdk_pixbuf_get_height (pixbuf);
	
	black = EEL_RGBA_COLOR_PACK (0, 0, 0, 255);
	white = EEL_RGBA_COLOR_PACK (255, 255, 255, 255);
	grey = EEL_RGBA_COLOR_PACK (153, 153, 153, 255);
	
	/* draw the name and version */
	display_str = g_strdup_printf ("%s %s", title, version);
	draw_aa_string (bold_font, pixbuf, 24, 12, 5, white, black, display_str, 1);
	g_free (display_str);
	
	/* draw the copyright notice */
	draw_aa_string (bold_font, pixbuf, 11, 12, 40, black, black, copyright, 0);

	/* draw the authors title */
	draw_aa_string (bold_font, pixbuf, 20, 184, 64, black, black, _("Authors"), 0);
	
	/* draw the time stamp */
	draw_aa_string (plain_font, pixbuf, 11, 284, total_height - 14, grey, black, time_stamp, 0);

	/* draw the translator's credit, if necessary */
	if (eel_strcmp (translators, "Translator Credits") != 0) {
		comment_array = g_strsplit (translators, "\n", 10);
		index = 0;
		while (comment_array[index] != NULL) {
			index += 1;
		}
		
		xpos = 6;
		ypos = total_height - (14 * index);
		
		index = 0;
		while (comment_array[index] != NULL) {
			draw_aa_string (plain_font, pixbuf, 11, xpos, ypos, black, black, comment_array[index], 0);
			ypos += 14;
			index++;	
		}
		g_strfreev (comment_array);
	}
	
	/* remember the authors */
	g_strfreev (about->details->authors);

	temp_str = g_strjoinv ("\n", (char**) authors);
	about->details->authors = g_strsplit (temp_str, "\n", -1);
	g_free (temp_str);
	
	randomize_authors (about);
	draw_author_list (about, pixbuf, plain_font);
	about->details->last_update_time = time (NULL);
	
	/* draw the comment, breaking it up into multiple lines */
	comment_array = g_strsplit (comments, "\n", 10);
	index = 0;
	xpos = 6; ypos = 118;
	while (comment_array[index] != NULL) {
		draw_aa_string (plain_font, pixbuf, 14, xpos, ypos, black, black, comment_array[index], 0);
		ypos += 18;
		index++;	
	}
	g_strfreev (comment_array);
		
	/* release the fonts */	
	gtk_object_unref (GTK_OBJECT(plain_font));
	gtk_object_unref (GTK_OBJECT(bold_font));
}

/* update authors is called to randomize the author array and redraw it */ 

void
nautilus_about_update_authors (NautilusAbout *about)
{
	ArtIRect author_area;
	EelScalableFont *plain_font;
	
	/* clear the author area */
	author_area = eel_art_irect_assign (AUTHOR_LEFT_POS - 24,
					    AUTHOR_TOP_POS,
					    2 * AUTHOR_COLUMN_WIDTH,
					    AUTHOR_LINE_HEIGHT * ITEMS_PER_COLUMN);
	
	eel_gdk_pixbuf_fill_rectangle_with_color
		(about->details->background_pixbuf,
		 author_area,
		 EEL_RGBA_COLOR_PACK (255, 255, 255, 255));

	/* randomize the author array */
	randomize_authors (about);
	
	/* redraw the authors */
	plain_font = eel_scalable_font_get_default_font ();
	draw_author_list (about, about->details->background_pixbuf, plain_font);
	gtk_object_unref (GTK_OBJECT(plain_font));
	
	about->details->last_update_time = time (NULL);

	/* set up the timer task if necessary */
	if (about->details->timer_task == 0) {
		about->details->timer_task = gtk_timeout_add
			(2000, update_authors_if_necessary, about); 
  	}  
	
	/* schedule a redraw for the about box */
	gtk_widget_queue_draw (GTK_WIDGET (about));
}

/* handle the dialog closing by killing the timer task */

static gboolean
nautilus_about_close (NautilusAbout *about, gpointer  *unused)
{
	if (about->details->timer_task != 0) {
		gtk_timeout_remove (about->details->timer_task);
		about->details->timer_task = 0;
	}  
	return FALSE;
}

static int
update_authors_if_necessary (gpointer callback_data)
{
	NautilusAbout *about;
	guint current_time, next_time;

	about = NAUTILUS_ABOUT (callback_data);
	
	current_time = time (NULL);
	next_time = about->details->last_update_time + UPDATE_TIME_INTERVAL;

	if (current_time > next_time) {
		nautilus_about_update_authors (about);
	}
	return TRUE;
}

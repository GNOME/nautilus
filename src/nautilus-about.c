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
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-theme.h>

struct NautilusAboutDetails {
	GtkWidget *drawing_area;
	GdkPixbuf *background_pixbuf;	
};

static void     nautilus_about_initialize_class	(NautilusAboutClass *klass);
static void     nautilus_about_initialize		(NautilusAbout *about);
static void	nautilus_about_destroy		(GtkObject *object);
static void 	nautilus_about_repaint    	(GtkWidget *drawing_area, 
				    		 GdkEventExpose *event,
				    		 NautilusAbout *about);
static void	nautilus_about_draw_info 	(NautilusAbout  *about,
						 const char	*title,
						 const char	*version,
						 const char	*copyright,
						 const char	**authors,
						 const char	*comments,
						 const char	*timestamp);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusAbout, nautilus_about, GNOME_TYPE_DIALOG)

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
	
	if (about->details->background_pixbuf) {
		gdk_pixbuf_unref (about->details->background_pixbuf);
	}
	
	g_free (NAUTILUS_ABOUT (object)->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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

	/* set the window title */
	gtk_window_set_title (GTK_WINDOW (about), _("About Nautilus"));
	
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
	
	gtk_widget_set_usize ( GTK_WIDGET (about->details->drawing_area), area_width, area_height);
	gtk_widget_set_events (about->details->drawing_area, GDK_EXPOSURE_MASK);

	gtk_signal_connect (GTK_OBJECT (about->details->drawing_area), "expose_event",
			    (GtkSignalFunc) nautilus_about_repaint, (gpointer) about);

	gtk_widget_show (about->details->drawing_area);
 	gtk_container_add (GTK_CONTAINER (frame), about->details->drawing_area);
        
	/* configure the dialog */                                  
	gnome_dialog_append_button ( GNOME_DIALOG(about),
				     GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_set_close( GNOME_DIALOG(about), TRUE);			
	gnome_dialog_close_hides ( GNOME_DIALOG(about), TRUE);
}

/* allocate a new about dialog */
GtkWidget*
nautilus_about_new (const char	*title,
		    const char	*version,
		    const char	*copyright,
		    const char	**authors,
		    const char	*comments,
		    const char *timestamp)
{
	NautilusAbout *about;
	
	about = gtk_type_new (nautilus_about_get_type ());
	
	/* draw the info onto the pixbuf, once and for all */
	nautilus_about_draw_info (about, title, version, copyright, authors, comments, timestamp);
	
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
nautilus_about_repaint (GtkWidget *widget, GdkEventExpose *event, NautilusAbout *about)
{
	/* draw the background image */
	if (about->details->background_pixbuf) {
		draw_pixbuf (about->details->background_pixbuf,
						     widget->window, 0, 0);
	}
}

/* utility routine to draw a string at a position */
static void
draw_aa_string (NautilusScalableFont *font, GdkPixbuf *pixbuf, int font_size, int x_pos, int y_pos, uint color, uint shadow_color, const char* text, int shadow_offset)
{
	if (shadow_offset) {
		nautilus_scalable_font_draw_text (font, pixbuf,
						  x_pos + shadow_offset, y_pos + shadow_offset,
						  NULL,
						  font_size, font_size,
						  text, strlen (text),
						  shadow_color, 255, FALSE);	
	}
	
	nautilus_scalable_font_draw_text (font, pixbuf, x_pos, y_pos, NULL, font_size, font_size, text, strlen (text), color, 255, FALSE);	
}

/* draw the information onto the pixbuf */
static void
nautilus_about_draw_info (
	NautilusAbout	*about,
	const char	*title,
	const char	*version,
	const char	*copyright,
	const char	**authors,
	const char	*comments,
	const char	*timestamp)
{
	char *display_str;
	char **comment_array;
	NautilusScalableFont *plain_font, *bold_font;
	GdkPixbuf *pixbuf;
	uint	black, white, grey;
	int xpos, ypos, total_height, index;
	
	plain_font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("helvetica", "medium", NULL, NULL));
	bold_font  = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new ("helvetica", "bold", NULL, NULL));

	pixbuf = about->details->background_pixbuf;
	total_height = gdk_pixbuf_get_height (pixbuf);
	
	black = NAUTILUS_RGBA_COLOR_PACK (0, 0, 0, 255);
	white = NAUTILUS_RGBA_COLOR_PACK (255, 255, 255, 255);
	grey = NAUTILUS_RGBA_COLOR_PACK (153, 153, 153, 255);
	
	/* draw the name and version */
	display_str = g_strdup_printf ("%s %s", title, version);
	draw_aa_string (bold_font, pixbuf, 24, 12, 10, white, black, display_str, 1);
	g_free (display_str);
	
	/* draw the copyright notice */
	draw_aa_string (bold_font, pixbuf, 11, 12, 40, black, black, copyright, 0);

	/* draw the authors title */
	draw_aa_string (bold_font, pixbuf, 20, 184, 64, black, black, "Authors", 0);
	
	/* draw the timestamp */
	draw_aa_string (plain_font, pixbuf, 11, 284, total_height - 14, grey, black, timestamp, 0);
	
	/* draw the authors */
	index = 0;
	xpos = 200; ypos = 88;
	while (authors[index] != NULL) {
		draw_aa_string (plain_font, pixbuf, 12, xpos, ypos, black, black, authors[index], 0);
		ypos += 15;
		index++;	
		if (index == 11) {
			ypos = 88;
			xpos = 340;
		}
	}	

	/* draw the comment, breaking it up into multiple lines */
	comment_array = g_strsplit (comments, "\n", 10);
	index = 0;
	xpos = 6; ypos = 118;
	while (comment_array[index] != NULL) {
		draw_aa_string (plain_font, pixbuf, 14, xpos, ypos, black, white, comment_array[index], 1);
		ypos += 18;
		index++;	
	}
	g_strfreev (comment_array);
	
	/* release the fonts */	
	gtk_object_unref (GTK_OBJECT(plain_font));
	gtk_object_unref (GTK_OBJECT(bold_font));
}

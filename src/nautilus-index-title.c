/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
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
 * This is the index title widget, which is the title part of the index panel
 *
 */

#include <config.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "nautilus-index-title.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-directory.h>
#include <libnautilus/nautilus-icon-factory.h>
#include <libnautilus/nautilus-metadata.h>

static void     nautilus_index_title_initialize_class   (NautilusIndexTitleClass *klass);
static void     nautilus_index_title_finalize           (GtkObject               *object);
static void     nautilus_index_title_initialize         (NautilusIndexTitle      *pixmap);
static gboolean nautilus_index_title_button_press_event (GtkWidget               *widget,
							 GdkEventButton          *event);
static GdkFont *select_font                             (const char              *text_to_format,
							 gint                     width,
							 const char              *font_template);
static void     set_up_font                             (GtkWidget               *widget,
							 GdkFont                 *font);
static void     nautilus_index_title_set_up_icon        (NautilusIndexTitle      *index_title);
static void     nautilus_index_title_set_up_label       (NautilusIndexTitle      *index_title);
static void     nautilus_index_title_set_up_info        (NautilusIndexTitle      *index_title);

struct _NautilusIndexTitleDetails {
	char *uri;
	GtkWidget *icon;
	GtkWidget *title;
	GtkWidget *more_info;
	GtkWidget *notes;
};

/* constants */

#define MAX_ICON_WIDTH 100.0
#define MAX_ICON_HEIGHT 120.0

/* button assignments */

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIndexTitle, nautilus_index_title, gtk_vbox_get_type ())

static void
nautilus_index_title_initialize_class (NautilusIndexTitleClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	
	object_class->finalize = nautilus_index_title_finalize;
	widget_class->button_press_event = nautilus_index_title_button_press_event;
}

static void
nautilus_index_title_initialize (NautilusIndexTitle *index_title)
{ 
	index_title->details = g_new0 (NautilusIndexTitleDetails, 1);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       nautilus_index_title_set_up_icon,
					       GTK_OBJECT (index_title));
}

/* finalize by throwing away private storage */

static void
nautilus_index_title_finalize (GtkObject *object)
{
	NautilusIndexTitle *index_title = NAUTILUS_INDEX_TITLE (object);

	g_free (index_title->details->uri);
	g_free (index_title->details);
  	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, finalize, (object));
}

/* return a new index title object */

GtkWidget *
nautilus_index_title_new (void)
{
	return GTK_WIDGET (gtk_type_new (nautilus_index_title_get_type ()));
}

/* set up the icon image */
static void
nautilus_index_title_set_up_icon (NautilusIndexTitle *index_title)
{
	NautilusFile *file_object;
	gint width, height;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkPixbuf *pixbuf;
	double scale_factor;
	double h_scale = 1.0;
	double v_scale = 1.0;

	file_object = nautilus_file_get (index_title->details->uri);
	
	if (file_object == NULL)
		return;
	
	pixbuf = nautilus_icon_factory_get_pixbuf_for_file(file_object, NAUTILUS_ICON_SIZE_STANDARD);
	nautilus_file_unref (file_object);

	/* even though we asked for standard size, it might still be really huge, if it's not
	   part of the standard set, so scale it down if necessary */
	
	width  = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	if (width > MAX_ICON_WIDTH)
		h_scale = MAX_ICON_WIDTH / (double) width;
	if (height > MAX_ICON_HEIGHT)
		v_scale = MAX_ICON_HEIGHT  / (double) height;
	if (h_scale < v_scale)
		scale_factor = h_scale;
	else
		scale_factor = v_scale;
	
	if (scale_factor < 1.0) {
		GdkPixbuf *scaled_pixbuf;
		gint scaled_width  =  floor(width * scale_factor + .5);
		gint scaled_height =  floor(height * scale_factor + .5);
				
		scaled_pixbuf = gdk_pixbuf_scale_simple(pixbuf, scaled_width, scaled_height, ART_FILTER_BILINEAR);	
		gdk_pixbuf_unref(pixbuf);
		pixbuf = scaled_pixbuf;
	}
	
	/* make a pixmap and mask to pass to the widget */
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
	gdk_pixbuf_unref(pixbuf);
	
	/* if there's no pixmap so far, so allocate one */
	if (index_title->details->icon)
		gtk_pixmap_set (GTK_PIXMAP (index_title->details->icon),
				pixmap, mask);
	else {
		index_title->details->icon = GTK_WIDGET (gtk_pixmap_new (pixmap, mask));
		gtk_widget_show (index_title->details->icon);
		gtk_box_pack_start (GTK_BOX (index_title), index_title->details->icon, 0, 0, 0);
		gtk_box_reorder_child (GTK_BOX (index_title), index_title->details->icon, 0);
	}   
}

/* utility routine (FIXME: should be located elsewhere) to find the largest font that fits */

GdkFont *
select_font(const char *text_to_format, gint width, const char* font_template)
{
	gint font_index, this_width;
	char *font_name;
	gint font_sizes[5] = { 28, 24, 18, 14, 12 };
	GdkFont *candidate_font = NULL;
	char *alt_text_to_format = NULL;
	char *temp_str = strdup(text_to_format);
	char *cr_pos = strchr(temp_str, '\n');

	if (cr_pos)
	  {
	  *cr_pos = '\0';
	  alt_text_to_format = cr_pos + 1;
	  }
	  
	for (font_index = 0; font_index < NAUTILUS_N_ELEMENTS (font_sizes); font_index++) {
		if (candidate_font != NULL)
			gdk_font_unref (candidate_font);
		
		font_name = g_strdup_printf (font_template, font_sizes[font_index]);
		candidate_font = gdk_font_load (font_name);
		g_free (font_name);
		
		this_width = gdk_string_width (candidate_font, temp_str);
		if (alt_text_to_format)
		   {
		     gint alt_width = gdk_string_width (candidate_font, alt_text_to_format);
		     if ((this_width <= width) && (alt_width <= width))
		     	{
		     	  g_free(temp_str);
		     	  return candidate_font;
		   	}
		   }
		else
		  if (this_width <= width)
			{
			  g_free(temp_str);
			  return candidate_font;
			}
	}
	
	g_free(temp_str);
	return candidate_font;
}

/* utility to apply font */
void
set_up_font(GtkWidget *widget, GdkFont *font)
{
	GtkStyle *temp_style;
	gtk_widget_realize (widget);	
	temp_style = gtk_style_new ();	  	
	temp_style->font = font;
	gtk_widget_set_style (widget, gtk_style_attach (temp_style, widget->window));
}


/* set up the filename label */

void
nautilus_index_title_set_up_label (NautilusIndexTitle *index_title)
{
	GnomeVFSURI *vfs_uri;
	GdkFont *label_font;
	char *file_name;
		
	vfs_uri = gnome_vfs_uri_new (index_title->details->uri);
	if (vfs_uri == NULL)
		return;
	
	file_name = gnome_vfs_uri_extract_short_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);
	
	if (file_name == NULL)
		return;
	
	/* split the filename into two lines if necessary */
	
	if (strlen(file_name) >= 16) {
		/* find an appropriate split point if we can */
		gint index;
		gint mid_point = strlen(file_name) >> 1;
		gint quarter_point = mid_point >> 1;
		for (index = 0; index < quarter_point; index++) {
			gint split_offset = 0;
			
			if (!isalnum(file_name[mid_point + index]))
				split_offset = mid_point + index;
			else if (!isalnum(file_name[mid_point - index]))
				split_offset = mid_point - index;
			
			if (split_offset != 0) {
				char *buffer = (char *) g_malloc(strlen(file_name) + 2);
				
				/* build the new string, with a CR inserted, also remembering them separately for measuring */
				
				memcpy(buffer, file_name, split_offset);
				buffer[split_offset] = '\n';
				strcpy(&buffer[split_offset + 1], &file_name[split_offset]);
				
				/* free up the old string and replace it with the new one with the return inserted */
				
				g_free(file_name);
				file_name = buffer;		  
			} 
		}
	}
	
	if (index_title->details->title != NULL)
		gtk_label_set_text (GTK_LABEL (index_title->details->title), file_name);
	else {  
		index_title->details->title = GTK_WIDGET (gtk_label_new (file_name));
		gtk_label_set_line_wrap (GTK_LABEL (index_title->details->title), TRUE);   
		gtk_label_set_justify(GTK_LABEL(index_title->details->title), GTK_JUSTIFY_CENTER);
		gtk_widget_show (index_title->details->title);
		gtk_box_pack_start (GTK_BOX (index_title), index_title->details->title, 0, 0, 0);
		gtk_box_reorder_child (GTK_BOX (index_title), index_title->details->title, 1);
	}   
	
	/* FIXME: don't use hardwired font like this - get it from preferences */    	
	label_font = select_font(file_name, GTK_WIDGET (index_title)->allocation.width - 4,
				 "-bitstream-courier-medium-r-normal-*-%d-*-*-*-*-*-*-*");	
	
	set_up_font(index_title->details->title, label_font);	
	g_free (file_name);
}

/* set up more info about the file */

void
nautilus_index_title_set_up_info (NautilusIndexTitle *index_title)
{
	NautilusFile *file_object;
	GdkFont *label_font;
	char *notes_text;
	char *temp_string = NULL;
	char *info_string;
	
	file_object = nautilus_file_get (index_title->details->uri);
	
	if (file_object == NULL)
		return;
	
	info_string = nautilus_file_get_string_attribute(file_object, "type");
	
	if (info_string != NULL) {
		/* combine the type and the size */
	  
		temp_string = nautilus_file_get_string_attribute(file_object, "size");
		if (temp_string != NULL) {
			char *new_info_string = g_strconcat(info_string, ", ", temp_string, NULL);
			g_free (info_string);
			g_free (temp_string);
			info_string = new_info_string; 
		}
		
		/* append the date modified */
		temp_string = nautilus_file_get_string_attribute(file_object, "date_modified");
		if (temp_string != NULL) {
			char *new_info_string = g_strconcat(info_string, "\n", temp_string, NULL);
			g_free (info_string);
			g_free (temp_string);
			info_string = new_info_string; 
		}
		
		if (index_title->details->more_info)
			gtk_label_set_text(GTK_LABEL(index_title->details->more_info), info_string);
		else {  
			index_title->details->more_info = GTK_WIDGET(gtk_label_new(info_string));
			gtk_widget_show (index_title->details->more_info);
			gtk_box_pack_start (GTK_BOX (index_title), index_title->details->more_info, 0, 0, 0);
			gtk_box_reorder_child (GTK_BOX (index_title), index_title->details->more_info, 2);
		}   
		
		/* FIXME: shouldn't use hardwired font */     	
		label_font = gdk_font_load("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");	
		set_up_font(index_title->details->more_info, label_font);	
		
		g_free(info_string);
		
		/* see if there are any notes for this file. If so, display them */
		notes_text = nautilus_file_get_metadata(file_object, NAUTILUS_NOTES_METADATA_KEY, NULL);
		if (notes_text)	{
			if (index_title->details->notes != NULL)
				gtk_label_set_text(GTK_LABEL(index_title->details->notes), notes_text);
			else  {  
				index_title->details->notes = GTK_WIDGET(gtk_label_new(notes_text));
				gtk_label_set_line_wrap(GTK_LABEL(index_title->details->notes), TRUE);
				gtk_widget_show (index_title->details->notes);
				gtk_box_pack_start (GTK_BOX (index_title), index_title->details->notes, 0, 0, 0);
				gtk_box_reorder_child (GTK_BOX (index_title), index_title->details->notes, 3);
			}
			
			/* FIXME: don't use hardwired font like this */    	
			label_font = gdk_font_load("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");	 
			set_up_font(index_title->details->notes, label_font);	
			
			g_free (notes_text);     
		}
		else if (index_title->details->notes)
			gtk_label_set_text(GTK_LABEL(index_title->details->notes), "");
	}

	nautilus_file_unref (file_object);
}

/* here's the place where we set everything up passed on the passed in uri */

void
nautilus_index_title_set_uri(NautilusIndexTitle *index_title, const char* new_uri)
{
	NautilusFile *file_object;

	if (index_title->details->uri != NULL) {
		g_free (index_title->details->uri);
	}
	index_title->details->uri = g_strdup (new_uri);

	file_object = nautilus_file_get(new_uri);

	/* add the icon */
	nautilus_index_title_set_up_icon (index_title);

	/* add the name, in a variable-sized label */
	nautilus_index_title_set_up_label (index_title);

	/* add various info */
	nautilus_index_title_set_up_info(index_title);

	/* FIXME: file_object can be NULL if this is a bad url, or one that
	 * NautilusFile can't handle (e.g. http). The UI here needs to
	 * be changed to account for that too.
	 */
	if (file_object != NULL)
		nautilus_file_unref (file_object);
}

/* handle a button press */

static gboolean
nautilus_index_title_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	/*
	  NautilusIndexTitle *index_title = NAUTILUS_INDEX_TITLE (widget);  
	*/
	g_message ("button press");
	
	return TRUE;
}

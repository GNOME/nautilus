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

#include "ntl-meta-view.h"
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libnautilus/nautilus-background.h>
#include <libnautilus/nautilus-directory.h>
#include <libnautilus/nautilus-glib-extensions.h>
#include <libnautilus/nautilus-icon-factory.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-metadata.h>
#include <libnautilus/nautilus-string.h>
#include <gnome.h>
#include <math.h>

#include "nautilus-index-title.h"

static void nautilus_index_title_class_initialize 	(NautilusIndexTitleClass *klass);
static void nautilus_index_title_finalize (GtkObject *object);

static void nautilus_index_title_initialize       	(NautilusIndexTitle *pixmap);
static gboolean nautilus_index_title_button_press_event(GtkWidget *widget, GdkEventButton *event);

GdkFont *select_font(const gchar *text_to_format, gint width, const gchar* font_template);
void set_up_font(GtkWidget *widget, GdkFont *font);

void nautilus_index_title_set_uri      (NautilusIndexTitle *index_title, const gchar* new_uri);
void nautilus_index_title_set_up_icon  (NautilusIndexTitle *index_title, NautilusFile *file_object);
void nautilus_index_title_set_up_label (NautilusIndexTitle *index_title, const gchar *uri);
void nautilus_index_title_set_up_info  (NautilusIndexTitle *index_title, NautilusFile *file_object);

static GtkHBoxClass *parent_class;

struct _NautilusIndexTitleDetails
{
  GtkWidget *icon;
  GtkWidget *title;
  GtkWidget *more_info;
  GtkWidget *notes;
};

/* button assignments */

GtkType
nautilus_index_title_get_type (void)
{
  static GtkType index_title_type = 0;

  if (!index_title_type)
    {
      static const GtkTypeInfo index_title_info =
      {
	"NautilusIndexTitle",
	sizeof (NautilusIndexTitle),
	sizeof (NautilusIndexTitleClass),
	(GtkClassInitFunc) nautilus_index_title_class_initialize,
	(GtkObjectInitFunc) nautilus_index_title_initialize,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      index_title_type = gtk_type_unique (gtk_vbox_get_type(), &index_title_info);
    }

  return index_title_type;
}

static void
nautilus_index_title_class_initialize (NautilusIndexTitleClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (gtk_vbox_get_type ());
  
  object_class->finalize = nautilus_index_title_finalize;
  widget_class->button_press_event = nautilus_index_title_button_press_event;
}

static void
nautilus_index_title_initialize (NautilusIndexTitle *index_title)
{ 
  index_title  ->details = g_new0 (NautilusIndexTitleDetails, 1);

  index_title->details->icon = NULL;
  index_title->details->title = NULL;  
  index_title->details->more_info = NULL;  
  index_title->details->notes = NULL;  
}

/* finalize by throwing away private storage */

static void
nautilus_index_title_finalize (GtkObject *object)
{
  NautilusIndexTitle *index_title = NAUTILUS_INDEX_TITLE(object);
   	   
  g_free(index_title->details);
  	
 (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* return a new index title object */

GtkWidget*
nautilus_index_title_new ()
{
  NautilusIndexTitle *index_title = gtk_type_new (nautilus_index_title_get_type ());
  return GTK_WIDGET (index_title);
}

/* set up the icon image */

void
nautilus_index_title_set_up_icon (NautilusIndexTitle *index_title, NautilusFile *file_object)
{  
  GdkPixmap *pixmap_for_dragged_file;
  GdkBitmap *mask_for_dragged_file;
  NautilusScalableIcon *icon;
  GdkPixbuf *pixbuf; 

  icon = nautilus_icon_factory_get_icon_for_file(file_object);
  if (icon == NULL)
  	return;
  
  pixbuf = nautilus_icon_factory_get_pixbuf_for_icon(icon, NAUTILUS_ICON_SIZE_STANDARD); 
   
  /* set up the pixmap and mask of the new gtk_pixmap */

   gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap_for_dragged_file,
				      &mask_for_dragged_file, 128);

  /* if there's no pixmap so far, so allocate one */
  if (index_title->details->icon)
     gtk_pixmap_set(GTK_PIXMAP(index_title->details->icon), pixmap_for_dragged_file, mask_for_dragged_file);
  else 
    {  
      index_title->details->icon = GTK_WIDGET (gtk_pixmap_new(pixmap_for_dragged_file, mask_for_dragged_file));
      gtk_widget_show (index_title->details->icon);
      gtk_box_pack_start (GTK_BOX (index_title), index_title->details->icon, 0, 0, 0);
    }   
  
  /* we're done with the pixmap and icon */
  gdk_pixbuf_unref(pixbuf);
  nautilus_scalable_icon_unref (icon);
}

/* utility routine (FIXME: should be located elsewhere) to find the largest font that fits */

GdkFont *
select_font(const gchar *text_to_format, gint width, const gchar* font_template)
{
	GdkFont *candidate_font = NULL;
	gchar *font_name;
	gint this_width;
	gint font_sizes[5] = { 28, 24, 18, 14, 12 };
	gint font_index;
	
	for (font_index = 0; font_index < NAUTILUS_N_ELEMENTS (font_sizes); font_index++) {
		if (candidate_font != NULL)
			gdk_font_unref (candidate_font);
		
		font_name = g_strdup_printf (font_template, font_sizes[font_index]);
		candidate_font = gdk_font_load (font_name);
		g_free (font_name);
		
		this_width = gdk_string_width (candidate_font, text_to_format);
		if (this_width < width)
			return candidate_font;
	}
	
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


/* set up the label */

void
nautilus_index_title_set_up_label (NautilusIndexTitle *index_title, const gchar *uri)
{
  GnomeVFSURI *vfs_uri;
  gchar *file_name;
  GdkFont *label_font;
	
  vfs_uri = gnome_vfs_uri_new (uri);
  if (vfs_uri == NULL)
	 return;
	
  file_name = gnome_vfs_uri_extract_short_name (vfs_uri);
  gnome_vfs_uri_unref (vfs_uri);
	
  if (file_name == NULL)
    return;

  if (index_title->details->title)
     gtk_label_set_text(GTK_LABEL(index_title->details->title), file_name);
  else 
    {  
      index_title->details->title = GTK_WIDGET(gtk_label_new(file_name));
      gtk_label_set_line_wrap(GTK_LABEL(index_title->details->title), TRUE);   
      gtk_widget_show (index_title->details->title);
      gtk_box_pack_start (GTK_BOX (index_title), index_title->details->title, 0, 0, 0);
    }   
  
  /* FIXME: don't use hardwired font like this */    	
  label_font = select_font(file_name, GTK_WIDGET (index_title)->allocation.width - 4,
			       "-bitstream-courier-medium-r-normal-*-%d-*-*-*-*-*-*-*");	

  set_up_font(index_title->details->title, label_font);	
  g_free (file_name);
}

/* set up more info about the file */

void
nautilus_index_title_set_up_info (NautilusIndexTitle *index_title, NautilusFile *file_object)
{
  GdkFont *label_font;
  gchar *notes_text;
  gchar *temp_string = NULL;
  gchar *info_string;

  if (file_object == NULL)
  	return;

  info_string = nautilus_file_get_string_attribute(file_object, "type");
  
   if (info_string == NULL)
  	return;

  /* combine the type and the size */
  
  temp_string = nautilus_file_get_string_attribute(file_object, "size");
  if (temp_string != NULL)
    {
      gchar *new_info_string = g_strconcat(info_string, ", ", temp_string, NULL);
      g_free(info_string);
      g_free(temp_string);
      info_string = new_info_string; 
    }
  
  /* append the date modified */
  temp_string = nautilus_file_get_string_attribute(file_object, "date_modified");
  if (temp_string != NULL)
    {
      gchar *new_info_string = g_strconcat(info_string, "\n", temp_string, NULL);
      g_free(info_string);
      g_free(temp_string);
      info_string = new_info_string; 
    }
  
  if (index_title->details->more_info)
     gtk_label_set_text(GTK_LABEL(index_title->details->more_info), info_string);
  else 
    {  
      index_title->details->more_info = GTK_WIDGET(gtk_label_new(info_string));
      gtk_widget_show (index_title->details->more_info);
      gtk_box_pack_start (GTK_BOX (index_title), index_title->details->more_info, 0, 0, 0);
    }   
   
  /* FIXME: shouldn't use hardwired font */     	
  label_font = gdk_font_load("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");	
  set_up_font(index_title->details->more_info, label_font);	
  
  g_free(info_string);

  /* see if there are any notes for this file. If so, display them */
  notes_text = nautilus_file_get_metadata(file_object, NAUTILUS_NOTES_METADATA_KEY, NULL);
  if (notes_text)
    {
	  if (index_title->details->notes)
	     gtk_label_set_text(GTK_LABEL(index_title->details->notes), notes_text);
	  else 
	    {  
	      index_title->details->notes = GTK_WIDGET(gtk_label_new(notes_text));
	      gtk_label_set_line_wrap(GTK_LABEL(index_title->details->notes), TRUE);   
	      gtk_widget_show (index_title->details->notes);
	      gtk_box_pack_start (GTK_BOX (index_title), index_title->details->notes, 0, 0, 0);
	    }   
	  
	  /* FIXME: don't use hardwired font like this */    	
      label_font = gdk_font_load("-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*");	 
	  set_up_font(index_title->details->notes, label_font);	
	  
	  g_free (notes_text);     
    }
  else
    if (index_title->details->notes)
	    gtk_label_set_text(GTK_LABEL(index_title->details->notes), "");
}

/* here's the place where we set everything up passed on the passed in uri */

void
nautilus_index_title_set_uri(NautilusIndexTitle *index_title, const gchar* new_uri)
{
  NautilusFile *file_object;

  file_object = nautilus_file_get(new_uri);

  nautilus_index_title_set_up_icon (index_title, file_object);
	
/* add the name, in a variable-sized label */
  nautilus_index_title_set_up_label (index_title, new_uri);

/* add various info */
  nautilus_index_title_set_up_info(index_title, file_object);

  /* FIXME: file_object can be NULL if this is a bad url, or one that
   * NautilusFile can't handle (e.g. http). The UI here needs to
   * be changed to account for that too.
   */
  if (file_object != NULL)
  	nautilus_file_unref(file_object);
}

/* handle a button press */

static gboolean
nautilus_index_title_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  /*
  NautilusIndexTitle *index_title = NAUTILUS_INDEX_TITLE (widget);  
  */
  printf("button press\n");
  
  return TRUE;
}

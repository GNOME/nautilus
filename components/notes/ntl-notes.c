/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
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

/* annotation metaview - allows you to annotate a directory or file */
#include "config.h"

#include <libnautilus/libnautilus.h>
#include <libnautilus/nautilus-metadata.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>
#include <ctype.h>
#include <libnautilus/nautilus-background.h>

#define NOTES_DEFAULT_BACKGROUND_COLOR  "rgb:FFFF/FFFF/BBBB"


typedef struct {
  NautilusViewFrame *view;

  GtkWidget *note_text_field;
  gchar* current_uri;
  
 } NotesView;

static int notes_object_count = 0;

static void notes_load_metainfo (NotesView *hview)
{
  NautilusFile *file_object;

  gtk_editable_delete_text(GTK_EDITABLE(hview->note_text_field), 0, -1);   
  
  file_object = nautilus_file_get(hview->current_uri);
  if (file_object != NULL)
    {
      gint position;
      gchar *notes_text;
     
      notes_text = nautilus_file_get_metadata(file_object, NAUTILUS_ANNOTATION_METADATA_KEY, "");
       if (notes_text)
        {
	  if (strlen(notes_text))
	    gtk_editable_insert_text(GTK_EDITABLE(hview->note_text_field), notes_text, strlen(notes_text), &position);
	  g_free(notes_text);
	}

      /* set up the label */

      nautilus_file_unref (file_object);
    }  
}

/* save the metainfo corresponding to the current uri, if any, into the text field */

static void notes_save_metainfo(NotesView *hview)
{
  NautilusFile *file_object;
  if (strlen(hview->current_uri) == 0)
    return;
  
  file_object = nautilus_file_get(hview->current_uri);	
  if (file_object != NULL)
    {
      gchar *notes_text = gtk_editable_get_chars(GTK_EDITABLE(hview->note_text_field), 0 , -1);
      if (notes_text == NULL)
        notes_text = strdup("");
	
      nautilus_file_set_metadata(file_object, NAUTILUS_ANNOTATION_METADATA_KEY, NULL, notes_text);
      
      g_free(notes_text);
      nautilus_file_unref (file_object);
    }
}

static void
notes_notify_location_change (NautilusViewFrame *view,
                                Nautilus_NavigationInfo *loci,
                                NotesView *hview)
{ 
  if (strcmp(hview->current_uri, loci->requested_uri))
    {
      notes_save_metainfo(hview);
      g_free(hview->current_uri);
      hview->current_uri = strdup(loci->requested_uri);
      notes_load_metainfo(hview);
    }
}

static void
do_destroy(GtkObject *obj, NotesView *hview)
{  
  notes_object_count--;
  if(notes_object_count <= 0)
    gtk_main_quit();
}

static BonoboObject *
make_obj(BonoboGenericFactory *Factory, const char *goad_id, gpointer closure)
{
  GtkWidget *vbox;
  NotesView *hview;
  NautilusBackground *background;

  g_return_val_if_fail (!strcmp (goad_id, "ntl_notes_view"), NULL);

  hview = g_new0 (NotesView, 1);

  /* initialize the current uri */
  hview->current_uri = strdup("");
  
  /* allocate a vbox to hold all of the UI elements */
  
  vbox = gtk_vbox_new(FALSE, 0);

  /* create the text container */
  
  hview->note_text_field = gtk_text_new(NULL, NULL);
  gtk_text_set_editable(GTK_TEXT(hview->note_text_field), TRUE);
  gtk_box_pack_start (GTK_BOX(vbox), hview->note_text_field, TRUE, TRUE, 0);
  background = nautilus_get_widget_background (GTK_WIDGET (hview->note_text_field));
  nautilus_background_set_color (background, NOTES_DEFAULT_BACKGROUND_COLOR);


  gtk_widget_show_all (vbox);
  
  /* Create CORBA object. */
  hview->view = NAUTILUS_VIEW_FRAME (nautilus_meta_view_frame_new (vbox));
  gtk_signal_connect (GTK_OBJECT (hview->view), "destroy", do_destroy, hview);
  notes_object_count++;


  /* handle events */
  gtk_signal_connect (GTK_OBJECT (hview->view), "notify_location_change", notes_notify_location_change, hview);

  /* set description */
  nautilus_meta_view_frame_set_label(NAUTILUS_META_VIEW_FRAME(hview->view),
                                     _("Notes"));

  return BONOBO_OBJECT (hview->view);
}

int main(int argc, char *argv[])
{
  BonoboGenericFactory *factory;
  CORBA_ORB orb;
  CORBA_Environment ev;

  /* initialize CORBA and Bonobo */
  CORBA_exception_init(&ev);
  orb = gnome_CORBA_init_with_popt_table("ntl-notes", VERSION, &argc, argv, NULL, 0, NULL,
					 GNORBA_INIT_SERVER_FUNC, &ev);
  bonobo_init(orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL);
  
  /* initialize gnome-vfs, etc */
  g_thread_init (NULL);
  gnome_vfs_init ();

  factory = bonobo_generic_factory_new_multi("ntl_notes_view_factory", make_obj, NULL);
  
  do {
    bonobo_main();
  } while (notes_object_count > 0);

  return 0;
}

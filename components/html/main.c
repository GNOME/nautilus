/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 *
 * Copyright (C) 1999  Free Software Foundaton
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <bonobo/gnome-bonobo.h>

#include <gtkhtml/gtkhtml.h>

#include "gnome-progressive-loader.h"


/* Per-GnomeEmbeddable data.  */
struct _EmbeddableData {
	/* The embeddable object.  */
	GnomeEmbeddable *embeddable;

	/* The HTML widget.  */
	GtkHTML *html;

	/* The scrolled window containing the HTML widget */
	GtkWidget *scrolled_window;

	/* The progressive data sink object used to feed our baby with juicy
	   HTML.  */
	GnomeProgressiveDataSink *pdsink;

	/* The GtkHTML stream associated with the HTML widget.  Data is passed
           to it through the ProgressiveDataSink interface.  */
	GtkHTMLStreamHandle html_stream;

	/* Total number of open views.  */
	guint view_count;
};
typedef struct _EmbeddableData EmbeddableData;

/* Per-GnomeView data.  */
struct _ViewData {
	/* The GnomeView itself.  */
	GnomeView *view;

	/* Data for the corresponding GnomeEmbeddable.  */
	EmbeddableData *embeddable_data;
};
typedef struct _ViewData ViewData;


/* Number of running objects.  */
static guint num_running_objects = 0;

/* Our embeddable factory object.  */
static GnomeEmbeddableFactory *embeddable_factory_object = NULL;


/* ProgressiveDataSink callbacks for the main HTML widget.  */

static gint
pdsink_start (GnomeProgressiveDataSink *pdsink,
	      gpointer data)
{
	EmbeddableData *embeddable_data;

	printf ("*** PSINK START\n");

	embeddable_data = (EmbeddableData *) data;

	if (embeddable_data->html_stream != NULL)
		gtk_html_end (embeddable_data->html,
			      embeddable_data->html_stream,
			      GTK_HTML_STREAM_OK);

	/* FIXME URL */
	embeddable_data->html_stream = gtk_html_begin (embeddable_data->html,
						       "Unknown name");
	gtk_html_parse (embeddable_data->html);

	return 0;
}

static gint
pdsink_end (GnomeProgressiveDataSink *pdsink,

	    gpointer data)
{
	EmbeddableData *embeddable_data;

	printf ("*** PSINK END\n");

	embeddable_data = (EmbeddableData *) data;
	gtk_html_end (embeddable_data->html,
		      embeddable_data->html_stream,
		      GTK_HTML_STREAM_OK);

	embeddable_data->html_stream = NULL;

	return 0;
}

static gint
pdsink_add_data (GnomeProgressiveDataSink *pdsink,
		 const GNOME_ProgressiveDataSink_iobuf *iobuf,
		 gpointer data)
{
	EmbeddableData *embeddable_data;
	gchar *p;

	embeddable_data = (EmbeddableData *) data;
	if (embeddable_data->html_stream == NULL)
		return 0;

	p = g_strndup (iobuf->_buffer, iobuf->_length);
	printf ("*** DATA: %s\n", p);
	g_free (p);

	gtk_html_write (embeddable_data->html,
			embeddable_data->html_stream,
			iobuf->_buffer,
			iobuf->_length);

	printf ("*** DATA END\n");

	return 0;
}

static gint
pdsink_set_size (GnomeProgressiveDataSink *pdsink,
		 const CORBA_long count,
		 gpointer data)
{
	EmbeddableData *embeddable_data;

	embeddable_data = (EmbeddableData *) data;

	/* Not useful.  */

	return 0;
}


/* ProgressiveDataSink callbacks for the streams generated on the
   "url_requested" signal.  */

struct _ReqPDSinkData {
	GtkHTML *html;
	GtkHTMLStreamHandle stream;
	GnomeProgressiveDataSink *pdsink;
};
typedef struct _ReqPDSinkData ReqPDSinkData;

static gint
req_pdsink_start (GnomeProgressiveDataSink *pdsink,
		  gpointer data)
{
	ReqPDSinkData *pdsink_data;

	printf ("*** REQ PDSink START\n");

	pdsink_data = (ReqPDSinkData *) data;

	if (pdsink_data->stream != NULL)
		gtk_html_end (pdsink_data->html,
			      pdsink_data->stream,
			      GTK_HTML_STREAM_OK);

	return 0;
}

static gint
req_pdsink_end (GnomeProgressiveDataSink *pdsink,
		gpointer data)
{
	ReqPDSinkData *pdsink_data;

	printf ("*** REQ PDSink END\n");

	pdsink_data = (ReqPDSinkData *) data;
	gtk_html_end (pdsink_data->html,
		      pdsink_data->stream,
		      GTK_HTML_STREAM_OK);

	gnome_object_unref (GNOME_OBJECT (pdsink_data->pdsink));
	g_free (pdsink_data);

	return 0;
}

static gint
req_pdsink_add_data (GnomeProgressiveDataSink *pdsink,
		     const GNOME_ProgressiveDataSink_iobuf *iobuf,
		     gpointer data)
{
	ReqPDSinkData *pdsink_data;

	pdsink_data = (ReqPDSinkData *) data;
	if (pdsink_data->stream == NULL)
		return 0;

	printf ("*** REQ PDSink DATA: length %u\n", iobuf->_length);

	gtk_html_write (pdsink_data->html,
			pdsink_data->stream,
			iobuf->_buffer,
			iobuf->_length);

	printf ("*** REQ PDSink DATA END\n");

	return 0;
}

static gint
req_pdsink_set_size (GnomeProgressiveDataSink *pdsink,
		     const CORBA_long count,
		     gpointer data)
{
	/* Not useful.  */
	return 0;
}


/* GtkHTML signals.  */

static void
title_changed_cb (GtkHTML *html,
		  gpointer data)
{
}

static void
url_requested_cb (GtkHTML *html,
		  const char *url,
		  GtkHTMLStreamHandle stream,
		  gpointer data)
{
	EmbeddableData *embeddable_data;
	GNOME_ClientSite client_site;
	GNOME_ProgressiveLoader loader;
	GnomeProgressiveDataSink *pdsink;
	ReqPDSinkData *req_pdsink_data;
	GNOME_ProgressiveDataSink corba_pdsink;
	CORBA_Environment ev;

	/* We are requested an extra URL.  So we request a
           GnomeProgressiveDataSink interface to our container and feed data
           from it.  */

	embeddable_data = (EmbeddableData *) data;
	client_site = embeddable_data->embeddable->client_site;

	CORBA_exception_init (&ev);

	/* FIXME cache the result.  */

	g_warning ("query_interface on the ClientSite.");
	loader = GNOME_Unknown_query_interface
		(client_site, "IDL:GNOME/ProgressiveLoader:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot query GNOME::ProgressiveLoader on the GNOME::ClientSite.");
		CORBA_exception_free (&ev);
		return;
	}
	if (loader == CORBA_OBJECT_NIL) {
		g_warning ("Our client site does not support GNOME::ProgressiveLoader!");
		CORBA_exception_free (&ev);
		return;
	}

	req_pdsink_data = g_new (ReqPDSinkData, 1);

	pdsink = gnome_progressive_data_sink_new (req_pdsink_start,
						  req_pdsink_end,
						  req_pdsink_add_data,
						  req_pdsink_set_size,
						  req_pdsink_data);
	if (pdsink == NULL) {
		g_warning ("Cannot create GNOME::ProgressiveDataSink interface for extra requested URL.");
		g_free (req_pdsink_data);
		CORBA_exception_free (&ev);
		return;
	}

	corba_pdsink = GNOME_OBJECT (pdsink)->corba_objref;

	/* Please send mail to sopwith@redhat.com for this evil cast.  :-) */
	GNOME_ProgressiveLoader_load (loader, (CORBA_char *) url, corba_pdsink, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot start client site progressive loading on the GNOME::ProgressiveDatasink interface.");
		g_free (req_pdsink_data);
		CORBA_exception_free (&ev);
		return;
	}

	req_pdsink_data->html = embeddable_data->html;
	req_pdsink_data->stream = stream;
	req_pdsink_data->pdsink = pdsink;

	CORBA_exception_free (&ev);
}

static void
load_done_cb (GtkHTML *html,
	      gpointer data)
{
}

static void
link_followed_cb (GtkHTML *html,
		  const char *url,
		  gpointer data)
{
}

static void
connect_gtk_html_signals (GtkHTML *html,
			  EmbeddableData *embeddable_data)
{
	gtk_signal_connect (GTK_OBJECT (html), "title_changed",
			    GTK_SIGNAL_FUNC (title_changed_cb),
			    embeddable_data);
	gtk_signal_connect (GTK_OBJECT (html), "url_requested",
			    GTK_SIGNAL_FUNC (url_requested_cb),
			    embeddable_data);
	gtk_signal_connect (GTK_OBJECT (html), "load_done",
			    GTK_SIGNAL_FUNC (load_done_cb),
			    embeddable_data);
	gtk_signal_connect (GTK_OBJECT (html), "link_followed",
			    GTK_SIGNAL_FUNC (link_followed_cb),
			    embeddable_data);
}


/* GnomeView callbacks.  */

static void
view_size_query_cb (GnomeView *view,
		    int *desired_width,
		    int *desired_height,
		    ViewData *view_data)
{
	GtkHTML *html;

	html = view_data->embeddable_data->html;

	/* FIXME this is *bogus*!  */
	*desired_width  = html->engine->width;
	*desired_height = html->engine->height;
}

static void
view_activate_cb (GnomeView *view,
		  gboolean activate,
		  ViewData *data)
{
	gnome_view_activate_notify (view, activate);
}

static void
view_system_exception_cb (GnomeView *view,
			  CORBA_Object corba_object,
			  CORBA_Environment *ev,
			  gpointer data)
{
	gnome_object_destroy (GNOME_OBJECT (view));
}

static void
view_destroy_cb (GnomeView *view,
		 ViewData *view_data)
{
	view_data->embeddable_data->view_count--;
	g_free (view_data);
}


/* GnomeView factory.  */

static GnomeView *
view_factory (GnomeEmbeddable *embeddable,
	      const GNOME_ViewFrame view_frame,
	      EmbeddableData *embeddable_data)
{
	ViewData *view_data;
	GnomeView *view;

	view_data = g_new0 (ViewData, 1);

	/* Create the GnomeView object.  */
	view = gnome_view_new (embeddable_data->scrolled_window);
	gtk_object_set_data (GTK_OBJECT (view), "view_data", view_data);

	gnome_view_set_view_frame (view, view_frame);

	gtk_signal_connect (GTK_OBJECT (view), "size_query",
			    GTK_SIGNAL_FUNC (view_size_query_cb), view_data);

	gtk_signal_connect (GTK_OBJECT (view), "view_activate",
			    GTK_SIGNAL_FUNC (view_activate_cb), view_data);

	gtk_signal_connect (GTK_OBJECT (view), "system_exception",
			    GTK_SIGNAL_FUNC (view_system_exception_cb),
			    view_data);
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (view_destroy_cb), view_data);

	view_data->embeddable_data = embeddable_data;
	view_data->view = view;

	embeddable_data->view_count++;

	return view;
}


/* GnomeEmbeddable callbacks.  */

static void
embeddable_destroy_cb (GnomeEmbeddable *embeddable,
		       EmbeddableData *embeddable_data)
{
	num_running_objects--;
	if (num_running_objects > 0)
		return;

	if (embeddable_data->pdsink != NULL)
		gnome_object_unref (GNOME_OBJECT (embeddable_data->pdsink));

	if (embeddable_data->html != NULL)
		gtk_widget_destroy (GTK_WIDGET (embeddable_data->html));

	gnome_object_unref (GNOME_OBJECT (embeddable_factory_object));

	g_free (embeddable_data);

	gtk_main_quit ();
}

static void
embeddable_system_exception_cb (GnomeEmbeddable *embeddable,
				CORBA_Object corba_object,
				CORBA_Environment *ev,
				gpointer data)
{
	gnome_object_destroy (GNOME_OBJECT (embeddable));
}


/* GnomeEmbeddable factory.  */

static GnomeObject *
embeddable_factory (GnomeEmbeddableFactory *this,
		    void *data)
{
	GnomeEmbeddable *embeddable;
	GnomeProgressiveDataSink *pdsink;
	EmbeddableData *embeddable_data;
	GtkWidget *html;
	GtkWidget *scrolled_window;

	embeddable_data = g_new (EmbeddableData, 1);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);

	html = gtk_html_new
		(gtk_scrolled_window_get_hadjustment
		 	(GTK_SCROLLED_WINDOW (scrolled_window)),
		 gtk_scrolled_window_get_vadjustment
		 	(GTK_SCROLLED_WINDOW (scrolled_window)));
	connect_gtk_html_signals (GTK_HTML (html), embeddable_data);

	gtk_container_add (GTK_CONTAINER (scrolled_window), html);
	gtk_widget_show (html);
	gtk_widget_show (scrolled_window);

	embeddable = gnome_embeddable_new (GNOME_VIEW_FACTORY (view_factory),
					   embeddable_data);

	pdsink = gnome_progressive_data_sink_new (pdsink_start,
						  pdsink_end,
						  pdsink_add_data,
						  pdsink_set_size,
						  embeddable_data);

	gnome_object_add_interface (GNOME_OBJECT (embeddable),
				    GNOME_OBJECT (pdsink));

	num_running_objects++;

	gtk_signal_connect (GTK_OBJECT (embeddable), "system_exception",
			    GTK_SIGNAL_FUNC (embeddable_system_exception_cb),
			    embeddable_data);

	gtk_signal_connect (GTK_OBJECT (embeddable), "destroy",
			    GTK_SIGNAL_FUNC (embeddable_destroy_cb),
			    embeddable_data);

	embeddable_data->embeddable = embeddable;
	embeddable_data->pdsink = pdsink;
	embeddable_data->scrolled_window = scrolled_window;
	embeddable_data->html = GTK_HTML (html);
	embeddable_data->html_stream = NULL;
	embeddable_data->view_count = 0;

	return GNOME_OBJECT (embeddable);
}


/* Main.  */

static GnomeEmbeddableFactory *
init_html_factory (void)
{
	return gnome_embeddable_factory_new
		("embeddable-factory:explorer-html-component",
		 embeddable_factory, NULL);
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_ORB orb;

	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table
		("explorer-html-component", VERSION,
		 &argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	CORBA_exception_free (&ev);

	orb = gnome_CORBA_ORB ();
	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error (_("Could not initialize Bonobo!"));
}
 
int
main (int argc, char **argv)
{
	/* Setup the factory.  */
	init_server_factory (argc, argv);
	embeddable_factory_object = init_html_factory ();

	gdk_rgb_init ();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	/* Start processing.  */
	bonobo_main ();

	return 0;
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
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
#include <bonobo.h>

#include <gtkhtml/gtkhtml.h>
#include <libnautilus/gnome-progressive-loader.h>


/* Per-BonoboEmbeddable data.  */
struct _EmbeddableData {
	/* The embeddable object.  */
	BonoboEmbeddable *embeddable;

	/* The HTML widget.  */
	GtkHTML *html;

	/* The scrolled window containing the HTML widget */
	GtkWidget *scrolled_window;

	/* The progressive data sink object used to feed our baby with juicy
	   HTML.  */
	BonoboProgressiveDataSink *pdsink;

	/* The GtkHTML stream associated with the HTML widget.  Data is passed
           to it through the ProgressiveDataSink interface.  */
	GtkHTMLStreamHandle html_stream;

	/* Total number of open views.  */
	guint view_count;
};
typedef struct _EmbeddableData EmbeddableData;

/* Per-BonoboView data.  */
struct _ViewData {
	/* The BonoboView itself.  */
	BonoboView *view;

	/* Data for the corresponding BonoboEmbeddable.  */
	EmbeddableData *embeddable_data;
};
typedef struct _ViewData ViewData;


/* Number of running objects.  */
static guint num_running_objects = 0;

/* Our embeddable factory object.  */
static BonoboEmbeddableFactory *embeddable_factory_object = NULL;


/* ProgressiveDataSink callbacks for the main HTML widget.  */

static gint
pdsink_start (BonoboProgressiveDataSink *pdsink,
	      gpointer data)
{
	EmbeddableData *embeddable_data;

	printf ("*** PSINK START\n");

	embeddable_data = (EmbeddableData *) data;

	if (embeddable_data->html_stream != NULL)
		gtk_html_end (embeddable_data->html,
			      embeddable_data->html_stream,
			      GTK_HTML_STREAM_OK);

	/* FIXME bugzilla.eazel.com 718: Need the URL here. */
	embeddable_data->html_stream = gtk_html_begin (embeddable_data->html,
						       "Unknown name");
	gtk_html_parse (embeddable_data->html);

	return 0;
}

static gint
pdsink_end (BonoboProgressiveDataSink *pdsink,

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
pdsink_add_data (BonoboProgressiveDataSink *pdsink,
		 const Bonobo_ProgressiveDataSink_iobuf *iobuf,
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
pdsink_set_size (BonoboProgressiveDataSink *pdsink,
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
	BonoboProgressiveDataSink *pdsink;
};
typedef struct _ReqPDSinkData ReqPDSinkData;

static gint
req_pdsink_start (BonoboProgressiveDataSink *pdsink,
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
req_pdsink_end (BonoboProgressiveDataSink *pdsink,
		gpointer data)
{
	ReqPDSinkData *pdsink_data;

	printf ("*** REQ PDSink END\n");

	pdsink_data = (ReqPDSinkData *) data;
	gtk_html_end (pdsink_data->html,
		      pdsink_data->stream,
		      GTK_HTML_STREAM_OK);

	bonobo_object_unref (BONOBO_OBJECT (pdsink_data->pdsink));
	g_free (pdsink_data);

	return 0;
}

static gint
req_pdsink_add_data (BonoboProgressiveDataSink *pdsink,
		     const Bonobo_ProgressiveDataSink_iobuf *iobuf,
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
req_pdsink_set_size (BonoboProgressiveDataSink *pdsink,
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
	Bonobo_ClientSite client_site;
	Bonobo_ProgressiveLoader loader;
	BonoboProgressiveDataSink *pdsink;
	ReqPDSinkData *req_pdsink_data;
	Bonobo_ProgressiveDataSink corba_pdsink;
	CORBA_Environment ev;

	/* We are requested an extra URL.  So we request a
           BonoboProgressiveDataSink interface to our container and feed data
           from it.  */

	embeddable_data = (EmbeddableData *) data;
	client_site = embeddable_data->embeddable->client_site;

	CORBA_exception_init (&ev);

	/* FIXME bugzilla.eazel.com 716: cache the result.  */

	g_warning ("query_interface on the ClientSite.");
	loader = Bonobo_Unknown_query_interface
		(client_site, "IDL:Bonobo/ProgressiveLoader:1.0", &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot query Bonobo::ProgressiveLoader on the Bonobo::ClientSite.");
		CORBA_exception_free (&ev);
		return;
	}
	if (loader == CORBA_OBJECT_NIL) {
		g_warning ("Our client site does not support Bonobo::ProgressiveLoader!");
		CORBA_exception_free (&ev);
		return;
	}

	req_pdsink_data = g_new (ReqPDSinkData, 1);

	pdsink = bonobo_progressive_data_sink_new (req_pdsink_start,
						  req_pdsink_end,
						  req_pdsink_add_data,
						  req_pdsink_set_size,
						  req_pdsink_data);
	if (pdsink == NULL) {
		g_warning ("Cannot create Bonobo::ProgressiveDataSink interface for extra requested URL.");
		g_free (req_pdsink_data);
		CORBA_exception_free (&ev);
		return;
	}

	corba_pdsink = BONOBO_OBJECT (pdsink)->corba_objref;

	/* Please send mail to sopwith@redhat.com for this evil cast.  :-) */
	Bonobo_ProgressiveLoader_load (loader, (CORBA_char *) url, corba_pdsink, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot start client site progressive loading on the Bonobo::ProgressiveDatasink interface.");
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


/* BonoboView callbacks.  */

static void
view_size_query_cb (BonoboView *view,
		    int *desired_width,
		    int *desired_height,
		    ViewData *view_data)
{
	GtkHTML *html;

	html = view_data->embeddable_data->html;

	/* FIXME bugzilla.eazel.com 717: this is *bogus*!  */
	*desired_width  = html->engine->width;
	*desired_height = html->engine->height;
}

static void
view_activate_cb (BonoboView *view,
		  gboolean activate,
		  ViewData *data)
{
	bonobo_view_activate_notify (view, activate);
}

static void
view_system_exception_cb (BonoboView *view,
			  CORBA_Object corba_object,
			  CORBA_Environment *ev,
			  gpointer data)
{
	bonobo_object_destroy (BONOBO_OBJECT (view));
}

static void
view_destroy_cb (BonoboView *view,
		 ViewData *view_data)
{
	view_data->embeddable_data->view_count--;
	g_free (view_data);
}


/* BonoboView factory.  */

static BonoboView *
view_factory (BonoboEmbeddable *embeddable,
	      const Bonobo_ViewFrame view_frame,
	      EmbeddableData *embeddable_data)
{
	ViewData *view_data;
	BonoboView *view;

	view_data = g_new0 (ViewData, 1);

	/* Create the BonoboView object.  */
	view = bonobo_view_new (embeddable_data->scrolled_window);
	gtk_object_set_data (GTK_OBJECT (view), "view_data", view_data);

	bonobo_view_set_view_frame (view, view_frame);

	gtk_signal_connect (GTK_OBJECT (view), "size_query",
			    GTK_SIGNAL_FUNC (view_size_query_cb), view_data);

	gtk_signal_connect (GTK_OBJECT (view), "activate",
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


/* BonoboEmbeddable callbacks.  */

static void
embeddable_destroy_cb (BonoboEmbeddable *embeddable,
		       EmbeddableData *embeddable_data)
{
	num_running_objects--;
	if (num_running_objects > 0)
		return;

	if (embeddable_data->pdsink != NULL)
		bonobo_object_unref (BONOBO_OBJECT (embeddable_data->pdsink));

	if (embeddable_data->html != NULL)
		gtk_widget_destroy (GTK_WIDGET (embeddable_data->html));

	bonobo_object_unref (BONOBO_OBJECT (embeddable_factory_object));

	g_free (embeddable_data);

	gtk_main_quit ();
}

static void
embeddable_system_exception_cb (BonoboEmbeddable *embeddable,
				CORBA_Object corba_object,
				CORBA_Environment *ev,
				gpointer data)
{
	bonobo_object_destroy (BONOBO_OBJECT (embeddable));
}


/* BonoboEmbeddable factory.  */

static BonoboObject *
embeddable_factory (BonoboEmbeddableFactory *this,
		    void *data)
{
	BonoboEmbeddable *embeddable;
	BonoboProgressiveDataSink *pdsink;
	EmbeddableData *embeddable_data;
	GtkWidget *html;
	GtkWidget *scrolled_window;

	embeddable_data = g_new (EmbeddableData, 1);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);

	html = gtk_html_new ();
	connect_gtk_html_signals (GTK_HTML (html), embeddable_data);

	gtk_container_add (GTK_CONTAINER (scrolled_window), html);
	gtk_widget_show (html);
	gtk_widget_show (scrolled_window);

	embeddable = bonobo_embeddable_new (BONOBO_VIEW_FACTORY (view_factory),
					   embeddable_data);

	pdsink = bonobo_progressive_data_sink_new (pdsink_start,
						  pdsink_end,
						  pdsink_add_data,
						  pdsink_set_size,
						  embeddable_data);

	bonobo_object_add_interface (BONOBO_OBJECT (embeddable),
				    BONOBO_OBJECT (pdsink));

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

	return BONOBO_OBJECT (embeddable);
}


/* Main.  */

static BonoboEmbeddableFactory *
init_html_factory (void)
{
	return bonobo_embeddable_factory_new
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

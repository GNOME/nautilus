/*
 * Generic image loading embeddable using gdk-pixbuf.
 *
 * Author:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Gene Z. Ragan (gzr@eazel.com)
 *
 * TODO:
 *    Progressive loading.
 *    Do not display more than required
 *    Queue request-resize on image size change/load
 *    Save image
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, Eazel, Inc.
 */
 
#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <gnome.h>
#include <liboaf/liboaf.h>

#include <bonobo.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_alphagamma.h>
#include <libnautilus/nautilus-zoomable.h>

#include "io-png.h"

#undef EOG_DEBUG

/*
 * Number of running objects
 */ 
static int running_objects = 0;
static BonoboGenericFactory    *image_factory = NULL;

/*
 * BonoboObject data
 */
typedef struct {
	BonoboEmbeddable *bonobo_object;
	GdkPixbuf        *pixbuf;
} bonobo_object_data_t;

/*
 * View data
 */
typedef struct {
	bonobo_object_data_t *bod;
	GtkWidget            *drawing_area;
        GtkWidget            *scrolled_window;
	GdkPixbuf            *scaled;
	NautilusZoomable     *zoomable;
        gboolean              size_allocated;
} view_data_t;

static void
release_pixbuf_cb (BonoboView *view, void *data)
{
	view_data_t *view_data = gtk_object_get_data (GTK_OBJECT (view),
						      "view_data");
	if (view_data == NULL || view_data->scaled == NULL) {
		return;
	}
	
	gdk_pixbuf_unref (view_data->scaled);
	view_data->scaled = NULL;
}
/*
 * Releases an image
 */
static void
release_pixbuf (bonobo_object_data_t *bod)
{
	g_return_if_fail (bod != NULL);

	if (bod->pixbuf != NULL) {
		gdk_pixbuf_unref (bod->pixbuf);
	}
	bod->pixbuf = NULL;
	
	bonobo_embeddable_foreach_view (bod->bonobo_object,
					release_pixbuf_cb,
					NULL);
}

static void
bod_destroy_cb (BonoboEmbeddable *embeddable, bonobo_object_data_t *bod)
{
        if (bod == NULL) {
		return;
	}

	release_pixbuf (bod);

	g_free (bod);

	running_objects--;
	if (running_objects > 0) {
		return;
	}
	/*
	 * When last object has gone unref the factory & quit.
	 */
	bonobo_object_unref (BONOBO_OBJECT (image_factory));
	gtk_main_quit ();
}

static GdkPixbuf *
get_pixbuf (view_data_t *view_data)
{
	g_return_val_if_fail (view_data != NULL, NULL);

	if (view_data->scaled != NULL) {
		return view_data->scaled;
	} else {
		bonobo_object_data_t *bod = view_data->bod;
		g_return_val_if_fail (bod != NULL, NULL);
		return bod->pixbuf;
	}
}

static void
render_pixbuf (GdkPixbuf *buf, GtkWidget *dest_widget,
	       GdkRectangle *rect)
{
	g_return_if_fail (buf != NULL);

	if (!GTK_IS_DRAWING_AREA (dest_widget)) {
		g_warning ("Non drawing area widget");
		return;
	}

	/* No drawing area yet ! */
	if (dest_widget == NULL || dest_widget->window == NULL) {
		return;
	}

	/*
	 * Do not draw outside the region that we know how to display
	 */
	if (rect->x > gdk_pixbuf_get_width (buf) ||
	    rect->y > gdk_pixbuf_get_height (buf)) {
/*		g_warning ("Render outside range %d %d %d %d (%d, %d)", rect->x, rect->y,
			   gdk_pixbuf_get_width (buf), gdk_pixbuf_get_height (buf),
			   rect->width, rect->height);*/
		return;
	}

	/*
	 * Clip the draw region
	 */
	if (rect->x + rect->width > gdk_pixbuf_get_width (buf)) {
		rect->width = gdk_pixbuf_get_width (buf) - rect->x;
	}

	if (rect->y + rect->height > gdk_pixbuf_get_height (buf)) {
		rect->height = gdk_pixbuf_get_height (buf) - rect->y;
	}

	/* Draw into the exposed region. */
	if (gdk_pixbuf_get_has_alpha (buf)) {
		gdk_draw_rgb_32_image (dest_widget->window,
				       dest_widget->style->white_gc,
				       rect->x, rect->y,
				       rect->width,
				       rect->height,
				       GDK_RGB_DITHER_NORMAL,
				       gdk_pixbuf_get_pixels (buf)
				       + (gdk_pixbuf_get_rowstride (buf) * rect->y + rect->x * 4),
				       gdk_pixbuf_get_rowstride (buf));
	} else {
		gdk_draw_rgb_image (dest_widget->window,
				    dest_widget->style->white_gc,
				    rect->x, rect->y,
				    rect->width,
				    rect->height,
				    GDK_RGB_DITHER_NORMAL,
				    gdk_pixbuf_get_pixels (buf)
				    + (gdk_pixbuf_get_rowstride (buf) * rect->y + rect->x * 3),
				    gdk_pixbuf_get_rowstride (buf));
	}
}

static void
redraw_view (view_data_t *view_data, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (view_data);

	if (buf == NULL) {
		return;
	}

	/*
	 * Don't actually render unless our size has been allocated,
	 * so we don't screw up the size allocation process by drawing
	 * an unscaled image too early.
	 */
	if (view_data->size_allocated) {
	        render_pixbuf (buf, view_data->drawing_area, rect);
	}
}

static void
configure_size (view_data_t *view_data, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (view_data);

	if (buf == NULL) {
		return;
	}

	/*
	 * Don't configure the size if it hasn't gotten allocated, to
	 * avoid messing with size_allocate process.
	 */
	if (!view_data->size_allocated) {
		gtk_widget_set_usize (view_data->drawing_area,
				      gdk_pixbuf_get_width (buf),
				      gdk_pixbuf_get_height (buf));
	  
		rect->x = 0;
		rect->y = 0;
		rect->width  = gdk_pixbuf_get_width (buf);
		rect->height = gdk_pixbuf_get_height (buf);

		view_data->size_allocated = TRUE;
	} else {
		GtkAllocation *a = &view_data->drawing_area->allocation;
		rect->x = a->x;
		rect->y = a->y;
		rect->width  = a->width;
		rect->height = a->height;
	}
}

static void
resize_all_cb (BonoboView *view, void *data)
{
	GtkWidget *widget;
	view_data_t *view_data;
	GdkRectangle rect;

	g_return_if_fail (view != NULL);

	widget = bonobo_control_get_widget (BONOBO_CONTROL (view));

	/* Clear out old bitmap data in drawing area */
	view_data = gtk_object_get_data (GTK_OBJECT (view), "view_data");
	if (view_data != NULL) {		
		if (view_data->drawing_area != NULL && view_data->drawing_area->window != NULL) {
			gdk_window_clear (view_data->drawing_area->window);
		}
		
		/* Update scrollbar size and postion */
		view_data->size_allocated = FALSE;
		configure_size (view_data, &rect);
	}
	
	gtk_widget_queue_resize (widget);
}

static void
view_update (view_data_t *view_data)
{
	GdkRectangle rect;

	g_return_if_fail (view_data != NULL);

	configure_size (view_data, &rect);
		
	redraw_view (view_data, &rect);
}

/*
 * Loads a png to a Bonobo_Stream
 */
static void
save_image_to_stream (BonoboPersistStream *ps, Bonobo_Stream stream,
		      Bonobo_Persist_ContentType type, void *data,
		      CORBA_Environment *ev)
{
	bonobo_object_data_t *bod = data;

	if (bod->pixbuf == NULL) {
		return;
	}

	image_save (stream, bod->pixbuf, ev);
}

/*
 * Loads an Image from a Bonobo_Stream
 */
static void
load_image_from_stream (BonoboPersistStream *ps, Bonobo_Stream stream,
			Bonobo_Persist_ContentType type, void *data,
			CORBA_Environment *ev)
{
	bonobo_object_data_t *bod = data;
	GdkPixbufLoader      *loader = gdk_pixbuf_loader_new ();
	Bonobo_Stream_iobuf  *buffer;
	CORBA_long            len;

	/* Free old data */
	release_pixbuf (bod);

	/* Load new data from stream */
	do {
		Bonobo_Stream_read (stream, 4096, &buffer, ev);
		if (ev->_major != CORBA_NO_EXCEPTION) {
			gdk_pixbuf_loader_close (loader);
			gtk_object_unref (GTK_OBJECT (loader));
			return;
		}

		if (buffer->_buffer &&
		     !gdk_pixbuf_loader_write (loader,
					       buffer->_buffer,
					       buffer->_length)) {
			CORBA_free (buffer);
			if (ev->_major == CORBA_NO_EXCEPTION) {
				gdk_pixbuf_loader_close (loader);
				gtk_object_unref (GTK_OBJECT (loader));
				return;
			} else {
				CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				ex_Bonobo_Persist_WrongDataType, NULL);
				gdk_pixbuf_loader_close (loader);
				gtk_object_unref (GTK_OBJECT (loader));
				return;
			}
		}
		len = buffer->_length;

		CORBA_free (buffer);
	} while (len > 0);

	bod->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (bod->pixbuf == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_Bonobo_Persist_WrongDataType, NULL);
		gdk_pixbuf_loader_close (loader);
		gtk_object_unref (GTK_OBJECT (loader));
	} else {
		gdk_pixbuf_ref (bod->pixbuf);
		bonobo_embeddable_foreach_view (bod->bonobo_object,
						resize_all_cb, bod);
	}
}

static void
destroy_view (BonoboView *view, view_data_t *view_data)
{
	g_return_if_fail (view_data != NULL);
	
	if (view_data->scaled != NULL) {
		gdk_pixbuf_unref (view_data->scaled);
	}
	view_data->scaled = NULL;

	gtk_widget_destroy (view_data->drawing_area);
	view_data->drawing_area = NULL;

	if (view_data->scrolled_window != NULL) {
		gtk_widget_destroy (view_data->scrolled_window);
	}
	view_data->scrolled_window = NULL;

	g_free (view_data);
}

#if 0
static void
zoomable_zoom_in_callback (BonoboView *view, view_data_t *view_data)
{
}

static void
zoomable_zoom_out_callback (BonoboView *view, view_data_t *view_data)
{
}

static void
zoomable_set_zoom_level_callback (BonoboView *view, view_data_t *view_data)
{
}

static void
zoomable_zoom_to_fit_callback (BonoboView *view, view_data_t *view_data)
{
}
#endif
			    
static int
drawing_area_exposed (GtkWidget *widget, GdkEventExpose *event, view_data_t *view_data)
{
	if (view_data->bod->pixbuf == NULL) {
		return TRUE;
	}
	
	redraw_view (view_data, &event->area);

	return TRUE;
}

/*
 * This callback will be invoked when the container assigns us a size.
 */
static void
view_size_allocate_cb (GtkWidget *drawing_area, GtkAllocation *allocation,
		       view_data_t *view_data)
{
	const GdkPixbuf *buf;
	GdkPixbuf       *view_buf;
	GdkInterpType    type;

	g_return_if_fail (view_data != NULL);
	g_return_if_fail (allocation != NULL);
	g_return_if_fail (view_data->bod != NULL);

	view_data->size_allocated = TRUE;

	if (view_data->bod->pixbuf == NULL) {
		return;
	}

	buf = view_data->bod->pixbuf;


#ifdef EOG_DEBUG
	g_warning ("Size allocate");
#endif

	if (allocation->width  == gdk_pixbuf_get_width (buf) &&
	    allocation->height == gdk_pixbuf_get_height (buf)) {
		if (view_data->scaled != NULL) {
			gdk_pixbuf_unref (view_data->scaled);
			view_data->scaled = NULL;
		}
		return;
	}

	view_buf = view_data->scaled;
	if (view_buf != NULL) {
		if (allocation->width  == gdk_pixbuf_get_width (view_buf) &&
		    allocation->height == gdk_pixbuf_get_height (view_buf)) {
#ifdef EOG_DEBUG
			g_warning ("Correct size %d, %d", allocation->width, allocation->height);
#endif
			return;
		} else {
			view_data->scaled = NULL;
			gdk_pixbuf_unref (view_buf);
			view_buf = NULL;
		}
	}

#ifdef EOG_DEBUG
	g_warning ("Re-scale to %d, %d", allocation->width, allocation->height);
#endif
	/* Too slow below this */
	if (allocation->width < gdk_pixbuf_get_width (buf) / 4 ||
	    allocation->width < gdk_pixbuf_get_width (buf) / 4)
		type = ART_FILTER_NEAREST;
	else
		type = ART_FILTER_TILES;

	view_data->scaled = gdk_pixbuf_scale_simple (buf, allocation->width,
						     allocation->height, type);
#ifdef EOG_DEBUG
	g_warning ("Scaling done");
#endif
	view_update (view_data);
}


/*
 * This callback will be invoked when the container assigns us a size.
 */
static void
scrolled_view_size_allocate_cb (GtkWidget *drawing_area, GtkAllocation *allocation,
		       view_data_t *view_data)
{	
	view_update (view_data);
}


static double zoom_levels[] = {
	(double) 100.0
};

static BonoboView *
view_factory_common (BonoboEmbeddable *bonobo_object,
		     GtkWidget        *scrolled_window,
		     const Bonobo_ViewFrame view_frame,
		     void *data)
{
        BonoboView *view;
	bonobo_object_data_t *bod = data;
	view_data_t *view_data = g_new0 (view_data_t, 1);
	GtkWidget   *root;

	view_data->bod = bod;
	view_data->scaled = NULL;
	view_data->drawing_area = gtk_drawing_area_new ();
	view_data->size_allocated = FALSE;
	view_data->scrolled_window = scrolled_window;

	gtk_signal_connect (
		GTK_OBJECT (view_data->drawing_area),
		"expose_event",
		GTK_SIGNAL_FUNC (drawing_area_exposed), view_data);

	if (scrolled_window) {
		root = scrolled_window;
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (root), 
						       view_data->drawing_area);
	} else
		root = view_data->drawing_area;

	gtk_widget_show_all (root);
	view = bonobo_view_new (root);

	view_data->zoomable = nautilus_zoomable_new_from_bonobo_control (BONOBO_CONTROL (view),
		 		.25, 4.0, FALSE, zoom_levels, 1);		

	gtk_object_set_data (GTK_OBJECT (view), "view_data",
			     view_data);

	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (destroy_view), view_data);

#if 0
	gtk_signal_connect (GTK_OBJECT (view), 
			    "zoom_in",
			    zoomable_zoom_in_callback,
			    view_data);
	gtk_signal_connect (GTK_OBJECT (view), 
			    "zoom_out", 
			    zoomable_zoom_out_callback,
			    view_data);
	gtk_signal_connect (GTK_OBJECT (view), 
			    "set_zoom_level", 
			    zoomable_set_zoom_level_callback,
			    view_data);
	gtk_signal_connect (GTK_OBJECT (view), 
			    "zoom_to_fit", 
			    zoomable_zoom_to_fit_callback,
			    view_data);
#endif
			    
	running_objects++;

        return view;
}

static BonoboView *
scaled_view_factory (BonoboEmbeddable *bonobo_object,
		     const Bonobo_ViewFrame view_frame,
		     void *data)
{
        BonoboView  *view;
	view_data_t *view_data;

	view = view_factory_common (bonobo_object, NULL, view_frame, data);

	view_data = gtk_object_get_data (GTK_OBJECT (view), "view_data");

	gtk_signal_connect (GTK_OBJECT (view_data->drawing_area), "size_allocate",
			    GTK_SIGNAL_FUNC (view_size_allocate_cb), view_data);

        return view;
}

static BonoboView *
scrollable_view_factory (BonoboEmbeddable *bonobo_object,
			 const Bonobo_ViewFrame view_frame,
			 void *data)
{
        BonoboView *view;
	view_data_t *view_data;
	GtkWidget   *scroll;

	scroll = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	view = view_factory_common (bonobo_object, scroll, view_frame, data);

	view_data = gtk_object_get_data (GTK_OBJECT (view), "view_data");

	gtk_signal_connect (GTK_OBJECT (view_data->drawing_area), "size_allocate",
			    GTK_SIGNAL_FUNC (scrolled_view_size_allocate_cb), view_data);

        return view;
}

static BonoboObject *
bonobo_object_factory (BonoboGenericFactory *this, const char *oaf_iid, void *data)
{
	BonoboEmbeddable     *bonobo_object;
	BonoboPersistStream  *stream;
	bonobo_object_data_t *bod;

	g_return_val_if_fail (this != NULL, NULL);
	g_return_val_if_fail (this->goad_id != NULL, NULL);

	bod = g_new0 (bonobo_object_data_t, 1);
	if (bod == NULL) {
		return NULL;
	}
	bod->pixbuf = NULL;
	
	/*
	 * Creates the BonoboObject server
	 */

	if (strcmp (oaf_iid, "OAFIID:nautilus-image-generic:6ed7ef0d-9274-4132-9a27-9f048142782f") == 0) {
		bonobo_object = bonobo_embeddable_new (scaled_view_factory, bod);
	} else if (strcmp (oaf_iid, "OAFIID:nautilus-image-viewer:30686633-23d5-422b-83c6-4f1b06f8abcd") == 0) {
		bonobo_object = bonobo_embeddable_new (scrollable_view_factory, bod);
	} else {
		g_free (bod);
		return NULL;
	}

	if (bonobo_object == NULL) {
		g_free (bod);
		return NULL;
	}

	/*
	 * Interface Bonobo::PersistStream 
	 */
	stream = bonobo_persist_stream_new (load_image_from_stream, 
					    save_image_to_stream, 
					    NULL, NULL, bod);
	if (stream == NULL) {
		gtk_object_unref (GTK_OBJECT (bonobo_object));
		g_free (bod);
		return NULL;
	}

	bod->bonobo_object = bonobo_object;

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (bod_destroy_cb), bod);
	/*
	 * Bind the interfaces
	 */
	bonobo_object_add_interface (BONOBO_OBJECT (bonobo_object),
				     BONOBO_OBJECT (stream));

	return BONOBO_OBJECT (bonobo_object);
}

static void
init_bonobo_image_generic_factory (void)
{
        char *registration_id;

	registration_id = oaf_make_registration_id ("OAFIID:nautilus_image_view_factory:61ea9ab1-e4b4-4da8-8f54-61cf6f33c4f6", g_getenv ("DISPLAY"));

	image_factory = bonobo_generic_factory_new_multi 
		(registration_id,
		 bonobo_object_factory, NULL);

	g_free (registration_id);
}

static void
init_server_factory (int argc, char **argv)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

        gnome_init_with_popt_table("bonobo-image-generic", VERSION,
				   argc, argv,
				   oaf_popt_options, 0, NULL); 
	oaf_init (argc, argv);

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL))
		g_error (_("I could not initialize Bonobo"));

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
	init_server_factory (argc, argv);

	init_bonobo_image_generic_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}

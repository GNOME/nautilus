/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Generic image loading embeddable using gdk-pixbuf.
 *
 * Author:
 *   Michael Meeks (mmeeks@gnu.org)
 *   Gene Z. Ragan (gzr@eazel.com)
 *   Martin Baulig (baulig@suse.de)
 *
 * TODO:
 *    Progressive loading.
 *    Do not display more than required
 *    Queue request-resize on image size change/load
 *    Save image
 *
 * Copyright 2000, Helixcode Inc.
 * Copyright 2000, SuSE GmbH.
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

#include <eel/eel-debug.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_alphagamma.h>

#include "io-png.h"

#define LOAD_BUFFER_SIZE 65536

/*
 * Number of running objects
 */ 
static int running_objects = 0;
static BonoboGenericFactory *image_factory = NULL;

/*
 * BonoboObject data
 */
typedef struct {
	BonoboControl    *control;
	GdkPixbuf        *pixbuf;

	GtkWidget        *root;
	GtkWidget        *drawing_area;
        GtkWidget        *scrolled_window;
	GdkPixbuf        *scaled;
        gboolean          size_allocated;
	gboolean	  got_initial_size;
	gboolean	  got_penultimate_allocation;
	
	GdkPixbuf        *zoomed;
	float             zoom_level;
	BonoboZoomable   *zoomable;
} bonobo_object_data_t;

static void control_update (bonobo_object_data_t *bod);

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

	if (bod->zoomed != NULL) {
		gdk_pixbuf_unref (bod->zoomed);
	}
	bod->zoomed = NULL;

	if (bod->scaled != NULL) {
		gdk_pixbuf_unref (bod->scaled);
	}
	bod->scaled = NULL;
}

static void
control_destroy_callback (BonoboControl *control, bonobo_object_data_t *bod)
{
        if (bod == NULL) {
		return;
	}

	release_pixbuf (bod);

	if (bod->drawing_area != NULL) {
		gtk_widget_unref (bod->drawing_area);
		bod->drawing_area = NULL;
	}

	bod->root = NULL;
	bod->scrolled_window = NULL;

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
get_pixbuf (bonobo_object_data_t *bod)
{
	g_return_val_if_fail (bod != NULL, NULL);

	if (bod->zoomed != NULL) {
		return bod->zoomed;
	} else if (bod->scaled != NULL) {
		return bod->scaled;
	} else {
		return bod->pixbuf;
	}
}

static void
render_pixbuf (GdkPixbuf *buf, GtkWidget *dest_widget, GdkRectangle *rect)
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
redraw_control (bonobo_object_data_t *bod, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (bod);

	if (buf == NULL) {
		return;
	}

	/*
	 * Don't actually render unless our size has been allocated,
	 * so we don't screw up the size allocation process by drawing
	 * an unscaled image too early.
	 */
	if (bod->got_initial_size) {
		render_pixbuf (buf, bod->drawing_area, rect);
	}
}

static void
configure_size (bonobo_object_data_t *bod, GdkRectangle *rect)
{
	GdkPixbuf *buf = get_pixbuf (bod);

	if (buf == NULL) {
		return;
	}

	/*
	 * Don't configure the size if it hasn't gotten allocated, to
	 * avoid messing with size_allocate process.
	 */
	if (!bod->size_allocated) {
		gtk_widget_set_usize (bod->drawing_area,
				      gdk_pixbuf_get_width (buf),
				      gdk_pixbuf_get_height (buf));
	  
		rect->x = 0;
		rect->y = 0;
		rect->width  = gdk_pixbuf_get_width (buf);
		rect->height = gdk_pixbuf_get_height (buf);

		bod->size_allocated = TRUE;
	} else {
		GtkAllocation *a = &bod->drawing_area->allocation;
		rect->x = a->x;
		rect->y = a->y;
		rect->width  = a->width;
		rect->height = a->height;
	}
}

static float preferred_zoom_levels[] = {
	1.0 / 10.0,  1.0 / 8.0, 1.0 / 6.0, 1.0 / 5.0, 
	1.0 / 4.0, 1.0 / 3.0, 1.0 / 2.0, 2.0 / 3.0, 1.0, 1.5, 2.0,
	3.0, 4.0, 5.0, 6.0, 8.0, 10.0
};
static const gchar *preferred_zoom_level_names[] = {
	"1:10", "1:8", "1:6", "1:5", "1:4", "1:3",
	"1:2", "2:3",  "1:1", "3:2", "2:1", "3:1", "4:1", "5:1", "6:1",
	"8:1", "10:1"
};

static const gint max_preferred_zoom_levels = (sizeof (preferred_zoom_levels) /
					       sizeof (float)) - 1;

static int
zoom_index_from_float (float zoom_level)
{
	int i;

	for (i = 0; i < max_preferred_zoom_levels; i++) {
		float this, epsilon;

		/* if we're close to a zoom level */
		this = preferred_zoom_levels [i];
		epsilon = this * 0.01;

		if (zoom_level < this+epsilon)
			return i;
	}

	return max_preferred_zoom_levels;
}

static float
zoom_level_from_index (int index)
{
	if (index > max_preferred_zoom_levels)
		index = max_preferred_zoom_levels;

	return preferred_zoom_levels [index];
}

static void
zoomable_zoom_in_callback (BonoboZoomable *zoomable, bonobo_object_data_t *bod)
{
	float this_zoom_level, new_zoom_level;
	int index;

	g_return_if_fail (bod != NULL);

	index = zoom_index_from_float (bod->zoom_level);
	if (index == max_preferred_zoom_levels)
		return;

	/* if we were zoomed to fit, we're not on one of the pre-defined level.
	 * We want to zoom into the next real level instead of skipping it
	 */
	this_zoom_level = zoom_level_from_index (index);
	
	if (this_zoom_level > bod->zoom_level) {
		new_zoom_level = this_zoom_level;
	} else {  
		index++;
		new_zoom_level = zoom_level_from_index (index);
	}
	
	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
				 new_zoom_level);
}

static void
zoomable_zoom_out_callback (BonoboZoomable *zoomable, bonobo_object_data_t *bod)
{
	float new_zoom_level;
	int index;

	g_return_if_fail (bod != NULL);

	index = zoom_index_from_float (bod->zoom_level);
	if (index == 0)
		return;

	index--;
	new_zoom_level = zoom_level_from_index (index);

	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
				 new_zoom_level);
}

static void
zoomable_zoom_to_fit_callback (BonoboZoomable *zoomable, bonobo_object_data_t *bod)
{
	GtkAdjustment *hadj, *vadj;
	float width, height;
	float x_level, y_level;
	float new_zoom_level;

	width = gdk_pixbuf_get_width (bod->pixbuf);
	height = gdk_pixbuf_get_height (bod->pixbuf);

	hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (bod->scrolled_window));
	vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bod->scrolled_window));

	x_level = hadj->page_size / width;
	y_level = vadj->page_size / height;

	new_zoom_level = (x_level < y_level) ? x_level : y_level;
	if (new_zoom_level > 0) {
		gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
				 new_zoom_level);
	}
}

static void
zoomable_zoom_to_default_callback (BonoboZoomable *zoomable, bonobo_object_data_t *bod)
{
	gtk_signal_emit_by_name (GTK_OBJECT (zoomable), "set_zoom_level",
				 1.0);
}

static void
resize_control (bonobo_object_data_t *bod)
{
	GdkRectangle rect;

	g_return_if_fail (bod != NULL);

	/* Clear out old bitmap data in drawing area */
	if ((bod->drawing_area != NULL) &&
	    (bod->drawing_area->window != NULL)) {
		gdk_window_clear (bod->drawing_area->window);
	}
		
	/* Update scrollbar size and postion */
	bod->size_allocated = FALSE;
	configure_size (bod, &rect);
	
	gtk_widget_queue_resize (bod->root);
}

static void
rezoom_control (bonobo_object_data_t *bod, float new_zoom_level)
{
	const GdkPixbuf *pixbuf;

	float old_width, old_height;
	float new_width, new_height;

	pixbuf = bod->pixbuf;
	old_width = gdk_pixbuf_get_width (pixbuf);
	old_height = gdk_pixbuf_get_height (pixbuf);

	new_width = old_width * new_zoom_level;
	new_height = old_height * new_zoom_level;

	if (bod->zoomed)
		gdk_pixbuf_unref (bod->zoomed);

	if (new_width >= 1 && new_height >= 1) {
		bod->zoomed = gdk_pixbuf_scale_simple (pixbuf, new_width, 
						       new_height, GDK_INTERP_BILINEAR);
	}

	resize_control (bod);
}

static void
zoomable_set_zoom_level_callback (BonoboZoomable *zoomable, float new_zoom_level,
				  bonobo_object_data_t *bod)
{
	g_return_if_fail (bod != NULL);

	rezoom_control (bod, new_zoom_level);
	bod->zoom_level = new_zoom_level;

	control_update (bod);

	bonobo_zoomable_report_zoom_level_changed (bod->zoomable,
						   new_zoom_level);
}

static void
control_update (bonobo_object_data_t *bod)
{
	GdkRectangle rect;

	g_return_if_fail (bod != NULL);

	configure_size (bod, &rect);
		
	redraw_control (bod, &rect);
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
		Bonobo_Stream_read (stream, LOAD_BUFFER_SIZE, &buffer, ev);
		if (ev->_major != CORBA_NO_EXCEPTION) {
			gdk_pixbuf_loader_close (loader);
			gtk_object_unref (GTK_OBJECT (loader));
			return;
		}
		
		if (buffer->_buffer != NULL && 
		    !gdk_pixbuf_loader_write (loader, buffer->_buffer, buffer->_length)) {
			CORBA_free (buffer);
				if (ev->_major != CORBA_NO_EXCEPTION) {
					CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
										 ex_Bonobo_Persist_WrongDataType, NULL);
				}				
				gdk_pixbuf_loader_close (loader);
				gtk_object_unref (GTK_OBJECT (loader));
				return;
		}
		
		len = buffer->_length;
		CORBA_free (buffer);
	} while (len > 0);

	gdk_pixbuf_loader_close (loader);
	bod->pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	if (bod->pixbuf == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_Bonobo_Persist_WrongDataType, NULL);
		gtk_object_unref (GTK_OBJECT (loader));
	} else {
		gdk_pixbuf_ref (bod->pixbuf);
		
		/* Restore current zoomed pixbuf cache. */
		if (bod->zoom_level != 1.0) {
			rezoom_control (bod, bod->zoom_level);
		}
		
		resize_control (bod);
	}
}

static int
drawing_area_exposed (GtkWidget *widget, GdkEventExpose *event,
		      bonobo_object_data_t *bod)
{
	if (bod->pixbuf == NULL) {
		return TRUE;
	}
	
	redraw_control (bod, &event->area);

	return TRUE;
}

/*
 * This callback will be invoked when the container assigns us a size.
 */
static void
control_size_allocate_callback (GtkWidget *drawing_area, GtkAllocation *allocation,
				bonobo_object_data_t *bod)
{
	const GdkPixbuf *buf;
	GdkPixbuf       *control_buf;
	
	g_return_if_fail (bod != NULL);
	g_return_if_fail (allocation != NULL);

	bod->size_allocated = TRUE;

	if (bod->pixbuf == NULL) {
		return;
	}

	buf = bod->pixbuf;

	if (allocation->width  == gdk_pixbuf_get_width (buf) &&
	    allocation->height == gdk_pixbuf_get_height (buf)) {
		if (bod->scaled != NULL) {
			gdk_pixbuf_unref (bod->scaled);
			bod->scaled = NULL;
		}
		return;
	}

	control_buf = bod->scaled;
	if (control_buf != NULL) {
		if (allocation->width  == gdk_pixbuf_get_width (control_buf) &&
		    allocation->height == gdk_pixbuf_get_height (control_buf)) {
			return;
		} else {
			bod->scaled = NULL;
			gdk_pixbuf_unref (control_buf);
			control_buf = NULL;
		}
	}
	
	if (allocation->width >= 1 && allocation->height >= 1) {
		bod->scaled = gdk_pixbuf_scale_simple (buf, allocation->width,
					       	       allocation->height, GDK_INTERP_BILINEAR);
	}
	
	control_update (bod);
}

/*
 * This callback will be invoked when the container assigns us a size.
 */
static void
scrolled_control_size_allocate_callback (GtkWidget *drawing_area,
					 GtkAllocation *allocation,
					 bonobo_object_data_t *bod)
{	
	control_update (bod);
}

/*
 * determine if the image is larger than the display area *
 */
static gboolean
image_fits_in_container (bonobo_object_data_t *bod)
{
	GtkAdjustment *hadj, *vadj;
	float width, height;

	width = gdk_pixbuf_get_width (bod->pixbuf);
	height = gdk_pixbuf_get_height (bod->pixbuf);

	hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (bod->scrolled_window));
	vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (bod->scrolled_window));

	return width <= hadj->page_size && height <= vadj->page_size;
}
 
/*
 * This callback will be invoked when the container assigns us a size.
 */
static void
scrolled_window_size_allocate_callback (GtkWidget *drawing_area,
					GtkAllocation *allocation,
					bonobo_object_data_t *bod)
{	
	/* Implement initial shrink-to-fit if necessary. It's hard to
	 * tell when resizing is complete, inspiring this hackish
	 * solution determining when; it should be replaced with a
	 * cleaner approach when the framework is improved.
	 */

	if (bod->got_penultimate_allocation
	    && !bod->got_initial_size
	    && allocation->width > 1
	    && allocation->height > 1) {
		bod->got_initial_size = TRUE;
	 	if (!image_fits_in_container (bod)) {
			zoomable_zoom_to_fit_callback (bod->zoomable, bod);
	 	}
		gtk_widget_queue_draw (bod->drawing_area);
	} else if (!bod->got_penultimate_allocation
		   && allocation->width == 1
		   && allocation->height == 1) {
		bod->got_penultimate_allocation = TRUE;
	}
}

static void
control_activate_callback (BonoboControl *control, gboolean activate, gpointer data)
{
	/*
	 * Notify the ControlFrame that we accept to be activated or
	 * deactivated (we are an acquiescent BonoboControl, yes we are).
	 */
	bonobo_control_activate_notify (control, activate);
}

static bonobo_object_data_t *
control_factory_common (GtkWidget *scrolled_window)
{
	BonoboPersistStream *stream;
	bonobo_object_data_t *bod;

	bod = g_new0 (bonobo_object_data_t, 1);
	bod->zoom_level = 1.0;
	bod->drawing_area = gtk_drawing_area_new ();
	bod->scrolled_window = scrolled_window;

	gtk_signal_connect (GTK_OBJECT (bod->drawing_area),
			    "expose_event",
			    GTK_SIGNAL_FUNC (drawing_area_exposed), bod);

	if (scrolled_window) {
		bod->root = scrolled_window;
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (bod->root), 
						       bod->drawing_area);
	} else {
		bod->root = bod->drawing_area;
		bod->got_initial_size = TRUE;
	}

	gtk_widget_show_all (bod->root);
	bod->control = bonobo_control_new (bod->root);

	/* We ref the drawing area to keep its memory from being freed when it
	 * is destroyed. The control may live on after the widget is destroyed
	 * and if it has been freed, tests like:
	 *  (bod->drawing_area != NULL) && (bod->drawing_area->window != NULL)
	 * won't work.
	 */
	gtk_widget_ref (bod->drawing_area);

	gtk_signal_connect (GTK_OBJECT (bod->control), "destroy",
			    GTK_SIGNAL_FUNC (control_destroy_callback), bod);

	bod->zoomable = bonobo_zoomable_new ();

	gtk_signal_connect (GTK_OBJECT (bod->zoomable), "set_zoom_level",
			    GTK_SIGNAL_FUNC (zoomable_set_zoom_level_callback), bod);
	gtk_signal_connect (GTK_OBJECT (bod->zoomable), "zoom_in",
			    GTK_SIGNAL_FUNC (zoomable_zoom_in_callback), bod);
	gtk_signal_connect (GTK_OBJECT (bod->zoomable), "zoom_out",
			    GTK_SIGNAL_FUNC (zoomable_zoom_out_callback), bod);
	gtk_signal_connect (GTK_OBJECT (bod->zoomable), "zoom_to_fit",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_fit_callback), bod);
	gtk_signal_connect (GTK_OBJECT (bod->zoomable), "zoom_to_default",
			    GTK_SIGNAL_FUNC (zoomable_zoom_to_default_callback), bod);

	bod->zoom_level = 1.0;
	bonobo_zoomable_set_parameters_full (bod->zoomable,
					     bod->zoom_level,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [max_preferred_zoom_levels],
					     FALSE, FALSE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     max_preferred_zoom_levels + 1);


	bonobo_object_add_interface (BONOBO_OBJECT (bod->control),
				     BONOBO_OBJECT (bod->zoomable));

	gtk_signal_connect (GTK_OBJECT (bod->control), "activate",
			    GTK_SIGNAL_FUNC (control_activate_callback), bod);

	/*
	 * Interface Bonobo::PersistStream 
	 */
	stream = bonobo_persist_stream_new (load_image_from_stream, 
					    save_image_to_stream, 
					    NULL, NULL, bod);
	bonobo_object_add_interface (BONOBO_OBJECT (bod->control),
				     BONOBO_OBJECT (stream));

	running_objects++;

        return bod;
}

static bonobo_object_data_t *
scaled_control_factory (void)
{
        bonobo_object_data_t *bod;

	bod = control_factory_common (NULL);

	gtk_signal_connect (GTK_OBJECT (bod->drawing_area), "size_allocate",
			    GTK_SIGNAL_FUNC (control_size_allocate_callback), bod);

        return bod;
}

static bonobo_object_data_t *
scrollable_control_factory (void)
{
	bonobo_object_data_t *bod;
	GtkWidget *scroll;

	scroll = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	bod = control_factory_common (scroll);

	gtk_signal_connect (GTK_OBJECT (bod->drawing_area), "size_allocate",
			    GTK_SIGNAL_FUNC (scrolled_control_size_allocate_callback),
			    bod);
	
	gtk_signal_connect (GTK_OBJECT (bod->scrolled_window), "size_allocate",
			    GTK_SIGNAL_FUNC (scrolled_window_size_allocate_callback),
			    bod);

        return bod;
}

static BonoboObject *
bonobo_object_factory (BonoboGenericFactory *this, const char *oaf_iid,
		       void *data)
{
	bonobo_object_data_t *bod;

	g_return_val_if_fail (this != NULL, NULL);

	/*
	 * Creates the BonoboObject server
	 */

	if (strcmp (oaf_iid, "OAFIID:nautilus-image-generic:6ed7ef0d-9274-4132-9a27-9f048142782f") == 0) {
		bod = scaled_control_factory ();
	} else if (strcmp (oaf_iid, "OAFIID:nautilus-image-viewer:30686633-23d5-422b-83c6-4f1b06f8abcd") == 0) {
		bod = scrollable_control_factory ();
	} else {
		return NULL;
	}

	if (bod == NULL) {
		return NULL;
	} else {
		return BONOBO_OBJECT (bod->control);
	}
}

static void
init_bonobo_image_generic_factory (void)
{
        char *registration_id;

	registration_id = oaf_make_registration_id ("OAFIID:nautilus_image_view_factory:61ea9ab1-e4b4-4da8-8f54-61cf6f33c4f6",
						    g_getenv ("DISPLAY"));

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
	
	/* Disable session manager connection */
	gnome_client_disable_master_connection ();

	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	oaf_init (argc, argv);

        gnome_init ("bonobo-image-generic", VERSION,
		    argc, argv); 
	gdk_rgb_init ();

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL))
		g_error (_("I could not initialize Bonobo"));

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
	/* Initialize gettext support */
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

	/* Make criticals and warnings stop in the debugger if NAUTILUS_DEBUG is set.
	 * Unfortunately, this has to be done explicitly for each domain.
	 */
	if (g_getenv ("NAUTILUS_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger (G_LOG_DOMAIN, NULL);
	}

	init_server_factory (argc, argv);

	init_bonobo_image_generic_factory ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual   (gdk_rgb_get_visual ());

	bonobo_main ();
	
	return 0;
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Author: Maciej Stachowiak
 */

/* nautilus-sample-content-view.c - sample content view
   component. This component just displays a simple label of the URI
   and does nothing else. It should be a good basis for writing
   out-of-proc content views.*/

#include <config.h>

#include "nautilus-sample-content-view.h"

#include <libnautilus/nautilus-gtk-macros.h>
#include <gtk/gtksignal.h>

struct _NautilusSampleContentViewDetails {
	gchar                     *uri;
	NautilusContentViewFrame  *view_frame;
};


static void nautilus_sample_content_view_initialize_class (NautilusSampleContentViewClass *klass);
static void nautilus_sample_content_view_initialize       (NautilusSampleContentView *view);
static void nautilus_sample_content_view_destroy          (GtkObject *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSampleContentView, nautilus_sample_content_view, GTK_TYPE_LABEL)
     
static void sample_notify_location_change_cb              (NautilusContentViewFrame  *view, 
							   Nautilus_NavigationInfo   *navinfo, 
							   NautilusSampleContentView *sample);
     
     
static void
nautilus_sample_content_view_initialize_class (NautilusSampleContentViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_sample_content_view_destroy;
}

static void
nautilus_sample_content_view_initialize (NautilusSampleContentView *view)
{
	view->details = g_new0 (NautilusSampleContentViewDetails, 1);
	
	gtk_label_set_text (GTK_LABEL (view), g_strdup ("(none)"));
	
	view->details->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (view));
	
	gtk_signal_connect (GTK_OBJECT (view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (sample_notify_location_change_cb), 
			    view);
	
	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_sample_content_view_destroy (GtkObject *object)
{
	NautilusSampleContentView *view;
	
	view = NAUTILUS_SAMPLE_CONTENT_VIEW (object);
	
	g_free (view->details->uri);
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* Component embedding support */
NautilusContentViewFrame *
nautilus_sample_content_view_get_view_frame (NautilusSampleContentView *view)
{
	return view->details->view_frame;
}

/* URI handling */
void
nautilus_sample_content_view_load_uri (NautilusSampleContentView *view,
				       const gchar               *uri)
{
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);
	gtk_label_set_text (GTK_LABEL (view), uri);
}

static void
sample_notify_location_change_cb (NautilusContentViewFrame  *view, 
				  Nautilus_NavigationInfo   *navinfo, 
				  NautilusSampleContentView *sample)
{
	Nautilus_ProgressRequestInfo pri;
	
	memset(&pri, 0, sizeof(pri));
	
	/* It's mandatory to send a PROGRESS_UNDERWAY message once the
	   component starts loading, otherwise nautilus will assume it
	   failed. In a real component, this will probably happen in some
	   sort of callback from whatever loading mechanism it is using to
	   load the data; this component loads no data, so it gives the
	   progrss upodate here. */
	
	pri.type = Nautilus_PROGRESS_UNDERWAY;
	pri.amount = 0.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (sample->details->view_frame), &pri);
	
	nautilus_sample_content_view_load_uri (sample, navinfo->actual_uri);
	
	/* It's mandatory to send a PROGRESS_DONE_OK message once the
	   component is done loading successfully, or PROGRESS_DONE_ERROR if
	   it completes unsuccessfully. In a real component, this will
	   probably happen in some sort of callback from whatever loading
	   mechanism it is using to load the data; this component loads no
	   data, so it gives the progrss upodate here. */

	pri.type = Nautilus_PROGRESS_DONE_OK;
	pri.amount = 100.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (sample->details->view_frame), &pri);
}


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
 * Authors:  Michael Fleming <mfleming@eazel.com>
 *           Robey Pointer <robey@eazel.com>
 *
 * Based on nautilus-sample-content-view by Maciej Stachowiak <mjs@eazel.com>
 */

/* trilobite-eazel-time-view.c: a simple Nautilus view for a simple service
 */

#include <config.h>
#include "trilobite-eazel-time-service.h"
#include "trilobite-eazel-time-view.h"

#include <gnome.h>
#include <bonobo.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-password-dialog.h>

#include <libtrilobite/libtrilobite.h>

/*
 * Defaults Macros
 *
 */

/* FIXME bugzilla.eazel.com 2588: no way to save the url once you change it */
#define DEFAULT_SERVER  "nist1.sjc.certifiedtime.com"
#define DEFAULT_TIME_DIFF	"180"

#define OAFIID_TIME_SERVICE	"OAFIID:trilobite_eazel_time_service:13a2dbd9-84f9-4400-bd9e-bb4575b86894"

#define STATUS_ERROR_NO_SERVER	"Could not load time service backend"

/* A NautilusContentView's private information. */
struct TrilobiteEazelTimeViewDetails {
	char *		server;
	char *		max_time_diff;
	NautilusView *	nautilus_view;
	GtkEntry *	p_localtime;
	GtkEntry *	p_servertime;
	GtkEntry *	p_server;
	GtkEntry *	p_maxdiff;
	GtkLabel *	p_status;
	Trilobite_Eazel_Time service;
	guint		timeout_slot;
	TrilobiteRootClient *root_client;
	char *		remembered_password;
	guint		password_attempts;
};

static void trilobite_eazel_time_view_initialize_class (TrilobiteEazelTimeViewClass *klass);
static void trilobite_eazel_time_view_initialize       (TrilobiteEazelTimeView      *view);
static void trilobite_eazel_time_view_destroy          (GtkObject                   *object);
static void load_location_callback                     (NautilusView                *nautilus_view,
							const char                  *location,
							TrilobiteEazelTimeView      *view);


EEL_DEFINE_CLASS_BOILERPLATE (TrilobiteEazelTimeView, trilobite_eazel_time_view, GTK_TYPE_EVENT_BOX)
   
static void
set_status_text (TrilobiteEazelTimeView *view, const char *status_str)
{
	g_return_if_fail (view->details->p_status);
	gtk_label_set_text (view->details->p_status, status_str);
}


static void
sync_button_pressed( GtkButton *p_button, TrilobiteEazelTimeView *view )
{
	CORBA_Environment ev;
	
	g_assert (TRILOBITE_IS_EAZEL_TIME_VIEW (view));

	if (! view->details->service) {
		set_status_text (view, STATUS_ERROR_NO_SERVER);
		return;
	}

	view->details->password_attempts = 0;

	CORBA_exception_init (&ev);

	while (1) {
		Trilobite_Eazel_Time_update_time (view->details->service, &ev);

		if (ev._major != CORBA_USER_EXCEPTION) {
			set_status_text (view, "Set time!");
			break;
		}

		if ( 0 == strcmp( ex_Trilobite_Eazel_Time_NotPermitted, CORBA_exception_id (&ev) ) ) {
			/* bad password -- let em try again? */
			if (view->details->password_attempts == 0) {
				/* cancelled */
				set_status_text (view, "Cancelled");
				break;
			}

			/* a wrong password shouldn't be remembered :) */
			g_free (view->details->remembered_password);
			view->details->remembered_password = NULL;
			CORBA_exception_free (&ev);

			if (view->details->password_attempts >= 3) {
				/* give up. */
				set_status_text (view, "Operation not permitted");
				break;
			}
		} else {
			set_status_text (view, "Exception: could not set time");
			break;
		}
	}

	CORBA_exception_free (&ev);
}


static void
server_button_pressed( GtkButton *p_button, TrilobiteEazelTimeView *view )
{
	CORBA_Environment ev;

	g_assert (TRILOBITE_IS_EAZEL_TIME_VIEW (view));

	CORBA_exception_init (&ev);

	if (view->details->service) {

		g_free (view->details->server);
		view->details->server = gtk_editable_get_chars (GTK_EDITABLE (view->details->p_server), 0, -1);
			
		Trilobite_Eazel_Time_set_time_server (view->details->service, view->details->server, &ev);

		set_status_text (view, "Time server changed");

	} else {
		set_status_text (view, STATUS_ERROR_NO_SERVER);
	}	

	CORBA_exception_free (&ev);
}

static void
timediff_button_pressed( GtkButton *p_button, TrilobiteEazelTimeView *view )
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	g_assert (TRILOBITE_IS_EAZEL_TIME_VIEW (view));

	if (view->details->service) {

		g_free (view->details->server);
		view->details->server = gtk_editable_get_chars (GTK_EDITABLE (view->details->p_maxdiff), 0, -1);
			
		Trilobite_Eazel_Time_set_max_difference (view->details->service, atoi (view->details->max_time_diff), &ev);

		set_status_text (view, "Max Difference changed");

	} else {
		set_status_text (view, STATUS_ERROR_NO_SERVER);
	}

	CORBA_exception_free (&ev);

}

static gint
update_time_display( TrilobiteEazelTimeView *view )
{
	time_t local_time, remote_time;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	g_return_val_if_fail (TRILOBITE_IS_EAZEL_TIME_VIEW (view), 1 );
	
	local_time = time( NULL );

	if (view->details->service) {
		remote_time = Trilobite_Eazel_Time_check_time(view->details->service, &ev );

		if (CORBA_USER_EXCEPTION == ev._major) {
			if ( 0 == strcmp( ex_Trilobite_Eazel_Time_CannotGetTime, CORBA_exception_id (&ev) ) ) {
				set_status_text (view, "Error: Cannot get remote time");
			} else {
				set_status_text (view, "Exception: could not get time");
			}
		} else {
			/* time service returns 0 if you're within the max time diff delta,
			 * or the number of seconds you'd need to add to be correct.
			 */
			remote_time += local_time;
			gtk_entry_set_text (GTK_ENTRY(view->details->p_servertime),  ctime (&remote_time));
		}
	}

	gtk_entry_set_text (GTK_ENTRY(view->details->p_localtime), ctime( &local_time));

	CORBA_exception_free (&ev);

	return TRUE;
}

static char *
trilobite_eazel_time_view_get_password (GtkObject *object, const char *prompt, void *my_object)
{
	TrilobiteEazelTimeView *view;
	GtkWidget *dialog;
	gboolean okay;
	char *tmp;
	char *message = NULL;

	view = TRILOBITE_EAZEL_TIME_VIEW (my_object);

	if (view->details->remembered_password) {
		return g_strdup (view->details->remembered_password);
	}

	if (view->details->password_attempts > 0) {
		message = "Incorrect password.";
	}

	dialog = eel_password_dialog_new ("Authenticate Me", message, prompt, "", TRUE);
	okay = eel_password_dialog_run_and_block (EEL_PASSWORD_DIALOG (dialog));

	if (! okay) {
		view->details->password_attempts = 0;
		tmp = g_strdup ("");
	} else {
		tmp =  eel_password_dialog_get_password (EEL_PASSWORD_DIALOG (dialog));
		if (eel_password_dialog_get_remember (EEL_PASSWORD_DIALOG (dialog))) {
			view->details->remembered_password = g_strdup (tmp);
		}
	}
	gtk_widget_destroy (dialog);
	gtk_main_iteration ();

	if (okay) {
		view->details->password_attempts++;
	}

	return tmp;
}

static void
trilobite_eazel_time_view_initialize_class (TrilobiteEazelTimeViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = trilobite_eazel_time_view_destroy;
}

static void
trilobite_eazel_time_view_initialize (TrilobiteEazelTimeView *view)
{
	GtkWidget *p_vbox;
	GtkWidget *p_hbox0;
	GtkWidget *p_hbox1;
	GtkWidget *p_hbox2;
	GtkWidget *p_hbox3;
	GtkWidget *p_sync;
	GtkWidget *p_server_button;
	GtkWidget *p_timediff_button;
	BonoboObjectClient *p_service;

	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	view->details = g_new0 (TrilobiteEazelTimeViewDetails, 1);

	/* Instance initialization */

	view->details->nautilus_view 	= nautilus_view_new (GTK_WIDGET (view));

	view->details->server 	        = g_strdup(DEFAULT_SERVER);
	view->details->max_time_diff 	= g_strdup(DEFAULT_TIME_DIFF);

	view->details->p_localtime 	= GTK_ENTRY (gtk_entry_new());
	view->details->p_servertime 	= GTK_ENTRY (gtk_entry_new());
	view->details->p_server 		= GTK_ENTRY (gtk_entry_new());
	view->details->p_maxdiff 	= GTK_ENTRY (gtk_entry_new());
	view->details->p_status 	= GTK_LABEL (gtk_label_new("(no status)"));

	gtk_entry_set_editable (GTK_ENTRY(view->details->p_localtime), FALSE);
	gtk_entry_set_editable (GTK_ENTRY(view->details->p_servertime), FALSE);

	gtk_entry_set_text (GTK_ENTRY(view->details->p_server), view->details->server);
	gtk_entry_set_text (GTK_ENTRY(view->details->p_maxdiff), view->details->max_time_diff);

	p_service	= bonobo_object_activate (OAFIID_TIME_SERVICE, 0);

	/* Ensure service object exists and is OK */
	if (NULL == p_service 
		|| NULL == (view->details->service = bonobo_object_query_interface (BONOBO_OBJECT (p_service),  "IDL:Trilobite/Eazel/Time:1.0"))
	) {
		set_status_text (view, STATUS_ERROR_NO_SERVER);
		g_warning (STATUS_ERROR_NO_SERVER);
	}

	/* Initialize service params */

	if (view->details->service) {
		Trilobite_Eazel_Time_set_time_server (view->details->service, view->details->server, &ev);
		Trilobite_Eazel_Time_set_max_difference (view->details->service, atoi (view->details->max_time_diff), &ev);
	}

	/* init the widgets */

	p_vbox = gtk_vbox_new (FALSE, 0);

	/* label */
	gtk_box_pack_start (GTK_BOX(p_vbox), gtk_label_new ("Eazel Time Service"), FALSE, FALSE, 0);

	/* Local Time */
	p_hbox0 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_hbox0), gtk_label_new ("Local Time"), FALSE, FALSE, 0);	
	gtk_box_pack_start (GTK_BOX(p_hbox0), GTK_WIDGET (view->details->p_localtime), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_vbox), GTK_WIDGET (p_hbox0), FALSE, FALSE, 0);

	/* Server Time */
	p_hbox1 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_hbox1), gtk_label_new ("Server Time"), FALSE, FALSE, 0);	
	gtk_box_pack_start (GTK_BOX(p_hbox1), GTK_WIDGET (view->details->p_servertime), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_vbox), GTK_WIDGET (p_hbox1), FALSE, FALSE, 0);

	/* sync button */
	p_sync = gtk_button_new_with_label ("Synchronize local time\nwith server");

	gtk_signal_connect (GTK_OBJECT (p_sync), "pressed", GTK_SIGNAL_FUNC (sync_button_pressed), view);

	gtk_box_pack_start (GTK_BOX (p_vbox), GTK_WIDGET (p_sync), FALSE, FALSE, 0);

	/* Time SERVER */

	p_hbox2 = gtk_hbox_new (FALSE,0);
	gtk_box_pack_start (GTK_BOX(p_hbox2), gtk_label_new ("Time server"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_hbox2), GTK_WIDGET (view->details->p_server), FALSE, FALSE, 0);
	p_server_button = gtk_button_new_with_label ("Set");
	gtk_signal_connect (GTK_OBJECT (p_server_button), "pressed", GTK_SIGNAL_FUNC (server_button_pressed), view);
	gtk_box_pack_start (GTK_BOX(p_hbox2), GTK_WIDGET (p_server_button), FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (p_vbox), GTK_WIDGET (p_hbox2), FALSE, FALSE, 0);

	/* Time diff */

	p_hbox3 = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_hbox3), gtk_label_new ("Max Time Diff"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_hbox3), GTK_WIDGET (view->details->p_maxdiff), FALSE, FALSE, 0);
	p_timediff_button = gtk_button_new_with_label ("Set");
	gtk_signal_connect (GTK_OBJECT (p_timediff_button), "pressed", GTK_SIGNAL_FUNC (timediff_button_pressed), view);
	gtk_box_pack_start (GTK_BOX(p_hbox3), GTK_WIDGET (p_timediff_button), FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (p_vbox), GTK_WIDGET (p_hbox3), FALSE, FALSE, 0);

	/* Status */

	gtk_box_pack_start (GTK_BOX (p_vbox), GTK_WIDGET (view->details->p_status), FALSE, FALSE, 0 );
	
	gtk_container_add (GTK_CONTAINER(view), GTK_WIDGET(p_vbox));
	
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (load_location_callback), 
			    view);

	/* set up callbacks to ask for the root password */
	view->details->root_client = trilobite_root_client_new ();
	trilobite_root_client_attach (view->details->root_client, p_service);
	gtk_signal_connect (GTK_OBJECT (view->details->root_client), "need_password",
			    GTK_SIGNAL_FUNC (trilobite_eazel_time_view_get_password), view);

	if (p_service) {
		bonobo_object_unref (BONOBO_OBJECT(p_service));
		p_service = NULL;
	}

	update_time_display (view);

	view->details->timeout_slot = gtk_timeout_add( 1000, (GtkFunction)update_time_display, view );
	
	gtk_widget_show_all (GTK_WIDGET (view));

	CORBA_exception_free (&ev);

}

static void
trilobite_eazel_time_view_destroy (GtkObject *object)
{
	TrilobiteEazelTimeView *view;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	view = TRILOBITE_EAZEL_TIME_VIEW (object);
	
	g_free (view->details->server);
	g_free (view->details->max_time_diff);
	g_free (view->details->remembered_password);

	Trilobite_Eazel_Time_unref (view->details->service, NULL);
	trilobite_root_client_unref (GTK_OBJECT (view->details->root_client));

	gtk_timeout_remove (view->details->timeout_slot);

	g_free (view->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));

	CORBA_exception_free (&ev);

}


/**
 * trilobite_eazel_time_view_get_nautilus_view:
 *
 * Return the NautilusView object associated with this view; this
 * is needed to export the view via CORBA/Bonobo.
 * @view: TrilobiteEazelTimeView to get the nautilus_view from..
 * 
 **/
NautilusView *
trilobite_eazel_time_view_get_nautilus_view (TrilobiteEazelTimeView *view)
{
	return view->details->nautilus_view;
}

/**
 * trilobite_eazel_time_view_load_uri:
 *
 * Load the resource pointed to by the specified URI.
 * 
 **/
void
trilobite_eazel_time_view_load_uri (TrilobiteEazelTimeView *view,
				       const char               *uri)
{
}

static void
load_location_callback (NautilusView *nautilus_view, 
			const char *location,
			TrilobiteEazelTimeView *view)
{
	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	trilobite_eazel_time_view_load_uri (view, location);
	nautilus_view_report_load_complete (nautilus_view);
}

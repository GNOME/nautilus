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
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-password-dialog.h>
#include <glade/glade.h>

#include <libtrilobite/libtrilobite.h>

/*
 * Defaults Macros
 *
 */

/* FIXME: no way to save the url once you change it */
#define DEFAULT_SERVER_URL	"http://testmachine.eazel.com:8888/examples/time/current"
#define DEFAULT_TIME_DIFF	"180"

#define OAFIID_TIME_SERVICE	"OAFIID:trilobite_eazel_time_service:13a2dbd9-84f9-4400-bd9e-bb4575b86894"

#define STATUS_ERROR_NO_SERVER	"Could not load time service backend"

/* A NautilusContentView's private information. */
struct TrilobiteEazelTimeViewDetails {
	char *		server_url;
	char *		max_time_diff;
	NautilusView *	nautilus_view;
	GtkEntry *	p_localtime;
	GtkEntry *	p_servertime;
	GtkEntry *	p_url;
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


NAUTILUS_DEFINE_CLASS_BOILERPLATE (TrilobiteEazelTimeView, trilobite_eazel_time_view, GTK_TYPE_EVENT_BOX)
   
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
url_button_pressed( GtkButton *p_button, TrilobiteEazelTimeView *view )
{
	CORBA_Environment ev;

	g_assert (TRILOBITE_IS_EAZEL_TIME_VIEW (view));

	CORBA_exception_init (&ev);

	if (view->details->service) {

		g_free (view->details->server_url);
		view->details->server_url = gtk_editable_get_chars (GTK_EDITABLE (view->details->p_url), 0, -1);
			
		Trilobite_Eazel_Time_set_time_url (view->details->service, view->details->server_url, &ev);

		set_status_text (view, "Server URL changed");

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

		g_free (view->details->server_url);
		view->details->server_url = gtk_editable_get_chars (GTK_EDITABLE (view->details->p_maxdiff), 0, -1);
			
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

	dialog = nautilus_password_dialog_new ("Authenticate Me", message, prompt, "", TRUE);
	okay = nautilus_password_dialog_run_and_block (NAUTILUS_PASSWORD_DIALOG (dialog));

	if (! okay) {
		view->details->password_attempts = 0;
		tmp = g_strdup ("");
	} else {
		tmp =  nautilus_password_dialog_get_password (NAUTILUS_PASSWORD_DIALOG (dialog));
		if (nautilus_password_dialog_get_remember (NAUTILUS_PASSWORD_DIALOG (dialog))) {
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
	GtkWidget *p_url_button;
	GtkWidget *p_timediff_button;
	BonoboObjectClient *p_service;

	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	view->details = g_new0 (TrilobiteEazelTimeViewDetails, 1);

	/* Instance initialization */

	view->details->nautilus_view 	= nautilus_view_new (GTK_WIDGET (view));

	view->details->server_url 	= g_strdup(DEFAULT_SERVER_URL);
	view->details->max_time_diff 	= g_strdup(DEFAULT_TIME_DIFF);

	view->details->p_localtime 	= GTK_ENTRY (gtk_entry_new());
	view->details->p_servertime 	= GTK_ENTRY (gtk_entry_new());
	view->details->p_url 		= GTK_ENTRY (gtk_entry_new());
	view->details->p_maxdiff 	= GTK_ENTRY (gtk_entry_new());
	view->details->p_status 	= GTK_LABEL (gtk_label_new("(no status)"));

	gtk_entry_set_editable (GTK_ENTRY(view->details->p_localtime), FALSE);
	gtk_entry_set_editable (GTK_ENTRY(view->details->p_servertime), FALSE);

	gtk_entry_set_text (GTK_ENTRY(view->details->p_url), view->details->server_url);
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
		Trilobite_Eazel_Time_set_time_url (view->details->service, view->details->server_url, &ev);
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

	/* Time URL */

	p_hbox2 = gtk_hbox_new (FALSE,0);
	gtk_box_pack_start (GTK_BOX(p_hbox2), gtk_label_new ("Server URL"), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(p_hbox2), GTK_WIDGET (view->details->p_url), FALSE, FALSE, 0);
	p_url_button = gtk_button_new_with_label ("Set");
	gtk_signal_connect (GTK_OBJECT (p_url_button), "pressed", GTK_SIGNAL_FUNC (url_button_pressed), view);
	gtk_box_pack_start (GTK_BOX(p_hbox2), GTK_WIDGET (p_url_button), FALSE, FALSE, 0);

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

#if 0
	/* 
	 * Get notified when our bonobo control is activated so we
	 * can merge menu & toolbar items into Nautilus's UI.
	 */
        gtk_signal_connect (GTK_OBJECT (nautilus_view_get_bonobo_control
					(view->details->nautilus_view)),
                            "activate",
                            sample_merge_bonobo_items_callback,
                            view);
	
#endif /* 0 */

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
	
	g_free (view->details->server_url);
	g_free (view->details->max_time_diff);
	g_free (view->details->remembered_password);

	Trilobite_Eazel_Time_unref (view->details->service, NULL);
	trilobite_root_client_unref (GTK_OBJECT (view->details->root_client));

	gtk_timeout_remove (view->details->timeout_slot);

	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

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
#if 0
	char *label_text;
	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	label_text = g_strdup_printf (_("%s\n\nThis is a sample Nautilus content view component."), uri);
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
#endif /* 0 */
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

#if 0
static void
bonobo_sample_callback (BonoboUIHandler *ui_handler, gpointer user_data, const char *path)
{
 	TrilobiteEazelTimeView *view;
#if 0
	char *label_text;
#endif /* 0 */

        g_assert (TRILOBITE_IS_EAZEL_TIME_VIEW (user_data));

	view = TRILOBITE_EAZEL_TIME_VIEW (user_data);

#if 0
	if (strcmp (path, "/File/Sample") == 0) {
		label_text = g_strdup_printf ("%s\n\nYou selected the Sample menu item.", view->details->uri);
	} else {
		g_assert (strcmp (path, "/Main/Sample") == 0);
		label_text = g_strdup_printf (_("%s\n\nYou clicked the Sample toolbar button."), view->details->uri);
	}
	
	gtk_label_set_text (GTK_LABEL (view), label_text);
	g_free (label_text);
#endif /* 0 */
}
#endif /* 0 */

#if 0

static void
sample_merge_bonobo_items_callback (BonoboObject *control, gboolean state, gpointer user_data)
{
 	TrilobiteEazelTimeView *view;
	BonoboUIHandler *local_ui_handler;
	GdkPixbuf		*pixbuf;
	BonoboUIHandlerPixmapType pixmap_type;
	char *path;
	
	g_assert (TRILOBITE_IS_EAZEL_TIME_VIEW (user_data));

	view = TRILOBITE_EAZEL_TIME_VIEW (user_data);
	local_ui_handler = bonobo_control_get_ui_handler (BONOBO_CONTROL (control));

	if (state) {
		/* Tell the Nautilus window to merge our bonobo_ui_handler items with its ones */
		bonobo_ui_handler_set_container (local_ui_handler, 
                                                 bonobo_control_get_remote_ui_handler (BONOBO_CONTROL (control)));

		/* Load test pixbuf */
		pixbuf = gdk_pixbuf_new_from_file ("/gnome/share/pixmaps/nautilus/i-directory-24.png");		
		if (pixbuf != NULL)
			pixmap_type = BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA;
		else
			pixmap_type = BONOBO_UI_HANDLER_PIXMAP_NONE;

	       /* Create our sample menu item. */ 
		path = bonobo_ui_handler_build_path (NULL,
						     "File", 
						     "Sample",
						     NULL);
		bonobo_ui_handler_menu_new_item 
			(local_ui_handler,					/* BonoboUIHandler */
	        	 path,							/* menu item path, must start with /some-existing-menu-path and be otherwise unique */
	                 _("_Sample"),						/* menu item user-displayed label */
	                 _("This is a sample merged menu item"),		/* hint that appears in status bar */
	                 bonobo_ui_handler_menu_get_pos 
	                 	(local_ui_handler, 
	                         NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM) + 1, 	/* position within menu; -1 means last */
	                 pixmap_type,						/* pixmap type */
	                 pixbuf,						/* pixmap data */
	                 'M',							/* accelerator key, couldn't bear the thought of using Control-S for anything except Save */
	                 GDK_CONTROL_MASK,					/* accelerator key modifiers */
	                 bonobo_sample_callback,				/* callback function */
	                 view);                					/* callback function data */
		g_free (path);

                /* Create our sample toolbar button. */ 
                path = bonobo_ui_handler_build_path (NAUTILUS_TOOLBAR_PATH_MAIN_TOOLBAR, 
                				     "Sample",
                				     NULL);
	        bonobo_ui_handler_toolbar_new_item 
	        	(local_ui_handler,				/* BonoboUIHandler */
	        	 path,						/* button path, must start with /some-existing-toolbar-path and be otherwise unique */
	        	 _("Sample"),					/* button user-displayed label */
	        	 _("This is a sample merged toolbar button"),	/* hint that appears in status bar */
	        	 -1,						/* position, -1 means last */
	        	 pixmap_type,					/* pixmap type */
	        	 pixbuf,					/* pixmap data */
	        	 0,						/* accelerator key */
	        	 0,						/* accelerator key modifiers */
	        	 bonobo_sample_callback,			/* callback function */
	        	 view);						/* callback function's data */
	        g_free (path);
	} else {
		/* Do nothing. */
	}

        /* 
         * Note that we do nothing if state is FALSE. Nautilus content views are activated
         * when installed, but never explicitly deactivated. When the view changes to another,
         * the content view object is destroyed, which ends up calling bonobo_ui_handler_unset_container,
         * which removes its merged menu & toolbar items.
         */
}

#endif /* 0 */

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
 * Author: J Shane Culpepper <pepper@eazel.com>
 *         Andy Hertzfeld <andy@eazel.com>
 *         Ramiro Estrugo <ramiro@eazel.com>
 */

#include <config.h>

#include "nautilus-login-view.h"
#include "shared-service-widgets.h"
#include "shared-service-utilities.h"

#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <stdio.h>
#include <unistd.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>

#if 0
#define TEST_TARGET "http://tortoise.eazel.com:8080/protected/"
#define TEST_LOGIN  "login.cgi"
#endif


typedef enum {
	PENDING_NONE,
	PENDING_LOGIN,
} PendingOperation;

/* A NautilusContentView's private information. */
struct _NautilusLoginViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*account_name;
	GtkWidget	*account_old_password;
	GtkWidget	*account_new_password;
	GtkWidget	*account_repeat_password;
	GtkWidget	*login_button;
	GtkWidget	*maintenance_button;
	GtkWidget	*feedback_text;
	PendingOperation pending;

	/* for talking to the proxy (login/logout/pass-change) */
	EazelProxy_UserControl user_control;
	EazelProxy_AuthnCallback authn_callback;
};

#define SERVICE_SUMMARY_LOCATION                "eazel-summary:"
#define SERVICE_HELP_LOCATION                   "http://www.eazel.com"

static void       nautilus_login_view_initialize_class (NautilusLoginViewClass     *klass);
static void       nautilus_login_view_initialize       (NautilusLoginView          *view);
static void       nautilus_login_view_destroy          (GtkObject                  *object);
static void       login_load_location_callback         (NautilusView               *nautilus_view,
							const char                 *location,
							NautilusLoginView          *view);
static void       generate_login_form                  (NautilusLoginView          *view);
static void       entry_changed_cb                     (GtkWidget                  *entry,
							NautilusLoginView          *view);
static void       login_button_cb                      (GtkWidget                  *button,
							NautilusLoginView          *view);
static void       maintenance_button_cb                (GtkWidget                  *button,
							NautilusLoginView          *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLoginView, nautilus_login_view, GTK_TYPE_EVENT_BOX)

static void
generate_login_form (NautilusLoginView	*view) 
{
	GtkTable	*table;
	GtkWidget	*temp_widget;
	GtkWidget	*temp_box;
	GtkWidget	*login_label;
	GtkWidget	*maintenance_button;
	GtkWidget	*maintenance_label;
	GdkFont		*font;
	GtkWidget	*title;

	/* allocate a box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* Setup the title */
	title = create_services_title_widget ("Please Sign in!");

        gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
        gtk_widget_show (title);

	/* initialize the parent form */
	temp_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
	gtk_widget_show (temp_box);

	/* allocate a table to hold the login form */
	table = GTK_TABLE (gtk_table_new (4, 3, TRUE));

	/* username */
	temp_widget = gtk_label_new ("User Name: ");

	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (temp_widget, font);
	gdk_font_unref (font);

	gtk_table_attach (table, temp_widget, 0, 2, 0, 1, GTK_FILL, GTK_FILL, 2, 2);
	gtk_widget_show (temp_widget);

	view->details->account_name = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_name, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 4);
	gtk_widget_show (view->details->account_name);

	/* old password */
	temp_widget = gtk_label_new ("Current password: ");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (temp_widget, font);
	gdk_font_unref (font);
	gtk_table_attach (table, temp_widget, 0, 2, 2, 3, GTK_FILL, GTK_FILL, 2, 2);
	gtk_widget_show (temp_widget);

	view->details->account_old_password = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_old_password, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 4, 4);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_old_password), FALSE);
	gtk_widget_show (view->details->account_old_password);

	/* new password */
	temp_widget = gtk_label_new ("New password: ");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (temp_widget, font);
	gdk_font_unref (font);
	gtk_table_attach (table, temp_widget, 0, 2, 4, 5, GTK_FILL, GTK_FILL, 2, 2);
	gtk_widget_show (temp_widget);

	view->details->account_new_password = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_new_password, 1, 2, 5, 6, GTK_FILL, GTK_FILL, 4, 4);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_new_password), FALSE);
	gtk_widget_show (view->details->account_new_password);

	/* repeat password */
	temp_widget = gtk_label_new ("New password (again): ");
	font = nautilus_font_factory_get_font_from_preferences (16);
	nautilus_gtk_widget_set_font (temp_widget, font);
	gdk_font_unref (font);
	gtk_table_attach (table, temp_widget, 0, 2, 6, 7, GTK_FILL, GTK_FILL, 2, 2);
	gtk_widget_show (temp_widget);

	view->details->account_repeat_password = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_repeat_password, 1, 2, 7, 8, GTK_FILL, GTK_FILL, 4, 4);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_repeat_password), FALSE);
	gtk_widget_show (view->details->account_repeat_password);

	/* insert the table */
	gtk_box_pack_start (GTK_BOX (view->details->form), GTK_WIDGET(table), 0, 0, 4);
	gtk_widget_show (GTK_WIDGET(table));

	/* attach a changed signal to the 2 entry fields, so we can enable the button when something is typed into both fields */
	gtk_signal_connect (GTK_OBJECT (view->details->account_name),		"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_old_password),	"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_new_password),	"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_repeat_password),"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);

	/* allocate the command buttons - first the login button */

	view->details->login_button = gtk_button_new ();
	login_label = gtk_label_new (" I'm ready to login! ");
	gtk_widget_show (login_label);
	gtk_container_add (GTK_CONTAINER (view->details->login_button), login_label);

	temp_box = gtk_hbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->login_button, FALSE, FALSE, 21);

	gtk_signal_connect (GTK_OBJECT (view->details->login_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (login_button_cb), view);
	gtk_widget_set_sensitive (view->details->login_button, FALSE);
	gtk_widget_show (view->details->login_button);
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 4);

        /* now allocate the account maintenance button */

        maintenance_button = gtk_button_new ();
        maintenance_label = gtk_label_new ("  I need some help!  ");
        gtk_widget_show (maintenance_label);
        gtk_container_add (GTK_CONTAINER (maintenance_button), maintenance_label);
	temp_box = gtk_hbox_new (TRUE, 0);
        gtk_box_pack_start (GTK_BOX (temp_box), maintenance_button, FALSE, FALSE, 21);
        gtk_signal_connect (GTK_OBJECT (maintenance_button), "clicked",
			    GTK_SIGNAL_FUNC (maintenance_button_cb), view);
        gtk_widget_show (maintenance_button);
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 4);

        /* add a label for error messages, but don't show it until there's an error */
        view->details->feedback_text = gtk_label_new ("");
        gtk_box_pack_end (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 8);
}

/* callback to enable/disable the login button when something is typed in the field */
static void
entry_changed_cb (GtkWidget	*entry, NautilusLoginView	*view)
{

	char		*user_name;
	char		*old_password;
	char		*new_password;
	char		*repeat_password;
	gboolean	button_enabled;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	old_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_old_password));
	new_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_new_password));
	repeat_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_repeat_password));

	button_enabled = user_name && strlen (user_name) && old_password && strlen (old_password) &&
		new_password && strlen (new_password) && repeat_password && strlen (repeat_password) &&
		(view->details->pending == PENDING_NONE);
	gtk_widget_set_sensitive (view->details->login_button, button_enabled);
}


static void
authn_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	NautilusLoginView *view;

	view = NAUTILUS_LOGIN_VIEW (state);
	g_assert (view->details->pending == PENDING_LOGIN);

	view->details->pending = PENDING_NONE;
	g_message ("Login succeeeded!");

	/* more ... */

	ammonite_auth_callback_wrapper_free (bonobo_poa (), view->details->authn_callback);
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));

#if 0
	if (registered_ok) {
		go_to_uri (view->details->nautilus_view, SERVICE_SUMMARY_LOCATION);
	}
#endif
}

static void
authn_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state,
	      CORBA_Environment *ev)
{
	NautilusLoginView *view;

	view = NAUTILUS_LOGIN_VIEW (state);
	g_assert (view->details->pending == PENDING_LOGIN);

	view->details->pending = PENDING_NONE;
	gtk_widget_set_sensitive (view->details->login_button, TRUE);

	show_feedback (view->details->feedback_text, "Incorrect current password.  Please try again.");
	gtk_entry_set_text (GTK_ENTRY (view->details->account_old_password), "");
	/* more? */

	ammonite_auth_callback_wrapper_free (bonobo_poa (), view->details->authn_callback);
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
}


static void
start_login (NautilusLoginView *view, const char *username, const char *password, const char *new_password)
{
	EazelProxy_AuthnInfo *authinfo;
	CORBA_Environment ev;
	CORBA_char *corba_new_password;

	AmmoniteAuthCallbackWrapperFuncs callback = {
		authn_succeeded,
		authn_failed
	};

	CORBA_exception_init (&ev);
	g_assert (view->details->pending == PENDING_NONE);

	if (view->details->user_control == CORBA_OBJECT_NIL) {
		/* can't do anything. */
		return;
	}

	view->details->authn_callback = ammonite_auth_callback_wrapper_new (bonobo_poa (), &callback, view);
	authinfo = EazelProxy_AuthnInfo__alloc ();
	authinfo->username = CORBA_string_dup (username);
	authinfo->password = CORBA_string_dup (password);
#ifdef TEST_TARGET
	authinfo->services_redirect_uri = CORBA_string_dup (TEST_TARGET);
	authinfo->services_login_path = CORBA_string_dup (TEST_LOGIN);
#else
	authinfo->services_redirect_uri = CORBA_string_dup ("");
	authinfo->services_login_path = CORBA_string_dup ("");
#endif

	/* ref myself until the callback returns */
	bonobo_object_ref (BONOBO_OBJECT (view->details->nautilus_view));

	view->details->pending = PENDING_LOGIN;
	corba_new_password = CORBA_string_dup (new_password);
	EazelProxy_UserControl_set_new_user_password (view->details->user_control, authinfo, corba_new_password,
						      view->details->authn_callback, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception during EazelProxy login: %s", CORBA_exception_id (&ev));
		view->details->pending = PENDING_NONE;
		bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));
	}

	CORBA_exception_free (&ev);
}


/* callback to handle the login button.  Right now only dumps a simple feedback message. */
static void
login_button_cb (GtkWidget	*button, NautilusLoginView	*view)
{

	char		*user_name;
	char		*old_password;
	char		*new_password;
	char		*repeat_password;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	old_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_old_password));
	new_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_new_password));
	repeat_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_repeat_password));

	if (strcmp (new_password, repeat_password) != 0) {
		show_feedback (view->details->feedback_text, "The new password fields don't match.  Please try again.");
		gtk_entry_set_text (GTK_ENTRY (view->details->account_new_password), "");
		gtk_entry_set_text (GTK_ENTRY (view->details->account_repeat_password), "");
		gtk_widget_set_sensitive (view->details->login_button, FALSE);
		return;
	}

	gtk_widget_set_sensitive (view->details->login_button, FALSE);
	gtk_widget_hide (view->details->feedback_text);

	start_login (view, user_name, old_password, new_password);
}

/* callback to point account maintenance button to webpage */
static void
maintenance_button_cb (GtkWidget	*button, NautilusLoginView	*view)
{

	go_to_uri (view->details->nautilus_view, SERVICE_HELP_LOCATION);

}

/* utility routine to go to another uri */

static void
nautilus_login_view_initialize_class (NautilusLoginViewClass *klass)
{

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_login_view_destroy;
}

static void
nautilus_login_view_initialize (NautilusLoginView *view)
{
	NautilusBackground *background;
	CORBA_Environment ev;

	view->details = g_new0 (NautilusLoginViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	view->details->pending = PENDING_NONE;
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (login_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	CORBA_exception_init (&ev);
	view->details->user_control = (EazelProxy_UserControl) oaf_activate_from_id (IID_EAZELPROXY, 0, NULL, &ev);
	if ( CORBA_NO_EXCEPTION != ev._major ) {
		/* FIXME user should be warned that Ammonite may not be installed */
		g_warning ("Couldn't instantiate eazel-proxy");
		view->details->user_control = CORBA_OBJECT_NIL;
	}
	CORBA_exception_free (&ev);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_login_view_destroy (GtkObject *object)
{
	NautilusLoginView *view;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	view = NAUTILUS_LOGIN_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	CORBA_Object_release (view->details->user_control, &ev);
	g_free (view->details);

	CORBA_exception_free (&ev);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusView *
nautilus_login_view_get_nautilus_view (NautilusLoginView *view)
{

	return view->details->nautilus_view;

}

void
nautilus_login_view_load_uri (NautilusLoginView	*view,
			      const char	*uri)
{

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_login_form (view);
}

static void
login_load_location_callback (NautilusView	*nautilus_view, 
			      const char	*location,
			      NautilusLoginView	*view)
{

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_login_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}


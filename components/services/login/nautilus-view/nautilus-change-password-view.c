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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Andy Hertzfeld <andy@eazel.com>
 *          Ramiro Estrugo <ramiro@eazel.com>
 *	    Robey Pointer <robey@eazel.com>
 */

#include <config.h>

#include "nautilus-change-password-view.h"
#include "password-box.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#include <gnome.h>
#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-viewport.h>
#include <stdio.h>
#include <unistd.h>
#include <libtrilobite/eazelproxy.h>
#include <libtrilobite/libammonite.h>
#include <libtrilobite/libtrilobite.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>


typedef enum {
	PENDING_NONE,
	PENDING_LOGIN,
} PendingOperation;

/* A NautilusContentView's private information. */
struct _NautilusChangePasswordViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*viewport;
	GtkWidget	*form;
	GtkWidget	*form_title;
	PasswordBox	*account_name;
	PasswordBox	*account_old_password;
	PasswordBox	*account_new_password;
	PasswordBox	*account_repeat_password;
	GtkWidget	*change_password_button;
	GtkWidget	*maintenance_button;
	PendingOperation pending;

	/* for talking to the proxy (change_password/logout/pass-change) */
	EazelProxy_UserControl user_control;
	EazelProxy_AuthnCallback authn_callback;
};

#define SERVICE_SUMMARY_LOCATION                "eazel:"
#define SERVICE_HELP_LOCATION                   "eazel-services://anonymous/account/login/lost_pwd_form"

static void       nautilus_change_password_view_initialize_class (NautilusChangePasswordViewClass     *klass);
static void       nautilus_change_password_view_initialize       (NautilusChangePasswordView          *view);
static void       nautilus_change_password_view_destroy          (GtkObject                  *object);
static void       change_password_load_location_callback         (NautilusView               *nautilus_view,
							const char                 *location,
							NautilusChangePasswordView          *view);
static void       generate_change_password_form                  (NautilusChangePasswordView          *view);
static void       entry_changed_cb                     (GtkWidget                  *entry,
							NautilusChangePasswordView          *view);
static void       change_password_button_cb                      (GtkWidget                  *button,
							NautilusChangePasswordView          *view);
static void       maintenance_button_cb                (GtkWidget                  *button,
							NautilusChangePasswordView          *view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusChangePasswordView, nautilus_change_password_view, GTK_TYPE_EVENT_BOX)


static char *
user_logged_in (NautilusChangePasswordView *view)
{
	CORBA_Environment ev;
	EazelProxy_User *user;
	char *username;

	if (view->details->user_control == CORBA_OBJECT_NIL) {
		return NULL;
	}

	CORBA_exception_init (&ev);
	user = EazelProxy_UserControl_get_default_user (view->details->user_control, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		username = NULL;
	} else {
		username = g_strdup (user->user_name);
		CORBA_free (user);
	}
	CORBA_exception_free (&ev);

	return username;
}

/* meant to be called from a timeout */
static gboolean
run_away_timer (NautilusChangePasswordView *view)
{
	nautilus_view_open_location_in_this_window (view->details->nautilus_view, SERVICE_SUMMARY_LOCATION);
	return FALSE;	/* don't run this timer again */
}

static void
generate_change_password_form (NautilusChangePasswordView	*view) 
{
	GtkWidget	*hbox;
	GtkWidget	*vbox_buttons, *hbox_buttons;
	GtkWidget	*maintenance_button;
	GtkWidget	*title;
	GtkWidget	*filler;
	GtkWidget	*pane;
	GtkWidget	*subform;
	char		*username;
	NautilusBackground *background;

	/* allocate a box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* Setup the title */
	title = eazel_services_header_title_new (_("Please Change Your Eazel Password"));
        gtk_box_pack_start (GTK_BOX (view->details->form), title, FALSE, FALSE, 0);
        gtk_widget_show (title);

	/* make an opportunistic scrollbar panel for it all */
        pane = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pane), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	view->details->viewport = nautilus_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (view->details->viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (pane), view->details->viewport);
	gtk_widget_show (view->details->viewport);
	gtk_container_add (GTK_CONTAINER (view->details->form), pane);
        gtk_widget_show (pane);
	background = nautilus_get_widget_background (GTK_WIDGET (view->details->viewport));
	nautilus_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	subform = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view->details->viewport), subform);
	gtk_widget_show (subform);

	/* add password boxes */
	view->details->account_name = password_box_new (_("User Name:"));
	gtk_widget_show (view->details->account_name->table);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), view->details->account_name->table, TRUE, TRUE, 75);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (subform), hbox, FALSE, FALSE, 6);

	view->details->account_old_password = password_box_new (_("Current Password:"));
	gtk_widget_show (view->details->account_old_password->table);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), view->details->account_old_password->table, TRUE, TRUE, 75);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (subform), hbox, FALSE, FALSE, 6);

	view->details->account_new_password = password_box_new (_("New Password:"));
	gtk_widget_show (view->details->account_new_password->table);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), view->details->account_new_password->table, TRUE, TRUE, 75);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (subform), hbox, FALSE, FALSE, 6);

	view->details->account_repeat_password = password_box_new (_("Confirm New Password:"));
	gtk_widget_show (view->details->account_repeat_password->table);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), view->details->account_repeat_password->table, TRUE, TRUE, 75);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (subform), hbox, FALSE, FALSE, 6);

	/* set up text entries */
	username = user_logged_in (view);
	if (username != NULL) {
		gtk_entry_set_text (GTK_ENTRY (view->details->account_name->entry), username);
		gtk_entry_set_editable (GTK_ENTRY (view->details->account_name->entry), FALSE);
		gtk_widget_set_sensitive (view->details->account_name->entry, FALSE);
		g_free (username);
	}

	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_old_password->entry), FALSE);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_new_password->entry), FALSE);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_repeat_password->entry), FALSE);

	/* attach a changed signal to the 2 entry fields,
	 * so we can enable the button when something is typed into both fields */
	gtk_signal_connect (GTK_OBJECT (view->details->account_name->entry),
			    "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_old_password->entry),
			    "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_new_password->entry),
			    "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_repeat_password->entry),
			    "changed", GTK_SIGNAL_FUNC (entry_changed_cb), view);

	/* allocate the command buttons - first the change_password button */
	view->details->change_password_button = gtk_button_new_with_label (_("Change my password"));
	gtk_signal_connect (GTK_OBJECT (view->details->change_password_button), "clicked",
			    GTK_SIGNAL_FUNC (change_password_button_cb), view);
	gtk_widget_set_sensitive (view->details->change_password_button, FALSE);
	gtk_widget_show (view->details->change_password_button);

        /* now allocate the account maintenance button */
        maintenance_button = gtk_button_new_with_label (_("I need assistance"));
        gtk_signal_connect (GTK_OBJECT (maintenance_button), "clicked",
			    GTK_SIGNAL_FUNC (maintenance_button_cb), view);
        gtk_widget_show (maintenance_button);

	vbox_buttons = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox_buttons), view->details->change_password_button, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox_buttons), maintenance_button, FALSE, FALSE, 4);
	gtk_widget_show (vbox_buttons);

	hbox_buttons = gtk_hbox_new (FALSE, 0);
	filler = gtk_label_new ("");
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (hbox_buttons), filler, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox_buttons), vbox_buttons, FALSE, FALSE, 75);
	gtk_widget_show (hbox_buttons);

	gtk_box_pack_start (GTK_BOX (subform), hbox_buttons, FALSE, FALSE, 0);
}

/* callback to enable/disable the change_password button when something is typed in the field */
static void
entry_changed_cb (GtkWidget	*entry, NautilusChangePasswordView	*view)
{
	char		*user_name;
	char		*old_password;
	char		*new_password;
	char		*repeat_password;
	gboolean	button_enabled;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name->entry));
	old_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_old_password->entry));
	new_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_new_password->entry));
	repeat_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_repeat_password->entry));

	button_enabled = user_name && strlen (user_name) && old_password && strlen (old_password) &&
		new_password && strlen (new_password) && repeat_password && strlen (repeat_password) &&
		(view->details->pending == PENDING_NONE);
	gtk_widget_set_sensitive (view->details->change_password_button, button_enabled);
}


static void
authn_succeeded (const EazelProxy_User *user, gpointer state, CORBA_Environment *ev)
{
	NautilusChangePasswordView *view;
	GtkWidget *dialog;
	GtkWidget *toplevel;
	char *text;

	view = NAUTILUS_CHANGE_PASSWORD_VIEW (state);
	g_assert (view->details->pending == PENDING_LOGIN);

	view->details->pending = PENDING_NONE;
	trilobite_debug ("ChangePassword succeeeded!");

	password_box_show_error (view->details->account_old_password, FALSE);
	password_box_show_error (view->details->account_new_password, FALSE);
	password_box_show_error (view->details->account_repeat_password, FALSE);

	ammonite_auth_callback_wrapper_free (bonobo_poa (), view->details->authn_callback);
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));

	text = _("Your password has been changed!");
	toplevel = gtk_widget_get_toplevel (view->details->form);
	if (GTK_IS_WINDOW (toplevel)) {
		dialog = gnome_ok_dialog_parented (text, GTK_WINDOW (toplevel));
	} else {
		dialog = gnome_ok_dialog (text);
	}
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	gtk_timeout_add (0, (GtkFunction)run_away_timer, view);
}

static void
authn_failed (const EazelProxy_User *user, const EazelProxy_AuthnFailInfo *info, gpointer state,
	      CORBA_Environment *ev)
{
	NautilusChangePasswordView *view;
	gboolean current_bad = FALSE;
	gboolean new_bad = FALSE;
	char *text = NULL;

	view = NAUTILUS_CHANGE_PASSWORD_VIEW (state);
	g_assert (view->details->pending == PENDING_LOGIN);

	view->details->pending = PENDING_NONE;
	gtk_widget_set_sensitive (view->details->change_password_button, TRUE);

	switch (info->code) {
	case EAZELPROXY_PASSWORD_CHANGE_BAD_ORIGINAL:
		current_bad = TRUE;
		text = _("I'm sorry, but that password\nis incorrect.  Please try again.");
		break;
	case EAZELPROXY_PASSWORD_CHANGE_TOO_SHORT:
		new_bad = TRUE;
		text = _("I'm sorry, but your new password\nmust be at least six (6) characters long.\nPlease try another one.");
		break;
	case EAZELPROXY_PASSWORD_CHANGE_TOO_LONG:
		/* shouldn't happen */
		new_bad = TRUE;
		text = _("I'm sorry, but your new password\ncan't be ridiculously long.\nPlease try another one.");
		break;
	case EAZELPROXY_PASSWORD_CHANGE_BAD_MATCH:
		/* shouldn't happen */
		new_bad = TRUE;
		text = "foo";
		break;
	case EAZELPROXY_PASSWORD_CHANGE_TOO_BLAND:
		new_bad = TRUE;
		text = _("I'm sorry, but your new password must\ncontain letters along with at least one\nnumber or symbol. Please try another one.");
		break;
	default:
		current_bad = TRUE;
		text = _("I'm sorry, but I hit an unexpected\nerror. Please try again, with\ndifferent passwords.");
		break;
	}

	if (current_bad) {
		password_box_set_error_text (view->details->account_old_password, text);
		password_box_show_error (view->details->account_old_password, TRUE);
		password_box_show_error (view->details->account_new_password, FALSE);
		password_box_show_error (view->details->account_repeat_password, FALSE);
	} else if (new_bad) {
		password_box_set_error_text (view->details->account_new_password, text);
		password_box_show_error (view->details->account_old_password, FALSE);
		password_box_show_error (view->details->account_new_password, TRUE);
		password_box_show_error (view->details->account_repeat_password, FALSE);
	} else {
		password_box_show_error (view->details->account_old_password, FALSE);
		password_box_show_error (view->details->account_new_password, FALSE);
		password_box_show_error (view->details->account_repeat_password, FALSE);
	}

	if (current_bad) {
		gtk_entry_set_text (GTK_ENTRY (view->details->account_old_password->entry), "");
		gtk_widget_grab_focus (view->details->account_old_password->entry);
	} else {
		gtk_entry_set_text (GTK_ENTRY (view->details->account_old_password->entry),
				    gtk_entry_get_text (GTK_ENTRY (view->details->account_old_password->entry)));
		gtk_widget_grab_focus (view->details->account_new_password->entry);
	}
	gtk_entry_set_text (GTK_ENTRY (view->details->account_new_password->entry), "");
	gtk_entry_set_text (GTK_ENTRY (view->details->account_repeat_password->entry), "");

	trilobite_debug ("ChangePassword failed: code = %ld, result = '%s'", (long)info->code, info->http_result);

	ammonite_auth_callback_wrapper_free (bonobo_poa (), view->details->authn_callback);
	bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));

	if (info->code == EAZELPROXY_AUTHN_FAIL_SERVER) {
		/* not sure what to do here.  apparently there's no way to start over. */
		nautilus_view_open_location_in_this_window
			(view->details->nautilus_view, SERVICE_SUMMARY_LOCATION);
	}
}


static void
start_change_password (NautilusChangePasswordView *view, const char *username, const char *password, const char *new_password)
{
	EazelProxy_AuthnInfo *authinfo;
	CORBA_Environment ev;
	CORBA_char *corba_new_password;
	char *text;

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
	authinfo->services_redirect_uri = CORBA_string_dup ("");
	authinfo->services_login_path = CORBA_string_dup ("");

	/* ref myself until the callback returns */
	bonobo_object_ref (BONOBO_OBJECT (view->details->nautilus_view));

	view->details->pending = PENDING_LOGIN;
	corba_new_password = CORBA_string_dup (new_password);
	EazelProxy_UserControl_set_new_user_password (view->details->user_control, authinfo, corba_new_password,
						      view->details->authn_callback, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception during EazelProxy change_password: %s", CORBA_exception_id (&ev));
		view->details->pending = PENDING_NONE;
		bonobo_object_unref (BONOBO_OBJECT (view->details->nautilus_view));

		text = _("I'm sorry, but I got an unexpected\nerror.  Please try again.");
		password_box_set_error_text (view->details->account_old_password, text);
		password_box_show_error (view->details->account_old_password, TRUE);
		password_box_show_error (view->details->account_new_password, FALSE);
		password_box_show_error (view->details->account_repeat_password, FALSE);
		gtk_widget_grab_focus (view->details->account_old_password->entry);

		gtk_entry_set_text (GTK_ENTRY (view->details->account_old_password->entry), "");
		gtk_entry_set_text (GTK_ENTRY (view->details->account_new_password->entry), "");
		gtk_entry_set_text (GTK_ENTRY (view->details->account_repeat_password->entry), "");
		gtk_widget_set_sensitive (view->details->change_password_button, FALSE);
	}

	CORBA_exception_free (&ev);
}


/* callback to handle the change_password button.  Right now only dumps a simple feedback message. */
static void
change_password_button_cb (GtkWidget	*button, NautilusChangePasswordView	*view)
{
	char		*user_name;
	char		*old_password;
	char		*new_password;
	char		*repeat_password;
	char		*text;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name->entry));
	old_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_old_password->entry));
	new_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_new_password->entry));
	repeat_password = gtk_entry_get_text (GTK_ENTRY (view->details->account_repeat_password->entry));

	if (strcmp (new_password, repeat_password) != 0) {
		text = _("I'm sorry, but your new password\nwasn't typed the same way twice.\nPlease try again.");
		password_box_set_error_text (view->details->account_new_password, text);
		password_box_show_error (view->details->account_old_password, FALSE);
		password_box_show_error (view->details->account_new_password, TRUE);
		password_box_show_error (view->details->account_repeat_password, FALSE);
		gtk_widget_grab_focus (view->details->account_new_password->entry);

		gtk_entry_set_text (GTK_ENTRY (view->details->account_new_password->entry), "");
		gtk_entry_set_text (GTK_ENTRY (view->details->account_repeat_password->entry), "");
		gtk_widget_set_sensitive (view->details->change_password_button, FALSE);
		return;
	}

	gtk_widget_set_sensitive (view->details->change_password_button, FALSE);
	start_change_password (view, user_name, old_password, new_password);
}

/* callback to point account maintenance button to webpage */
static void
maintenance_button_cb (GtkWidget *button, NautilusChangePasswordView *view)
{

	nautilus_view_open_location_in_this_window
		(view->details->nautilus_view, SERVICE_HELP_LOCATION);

}

/* utility routine to go to another uri */

static void
nautilus_change_password_view_initialize_class (NautilusChangePasswordViewClass *klass)
{

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_change_password_view_destroy;
}

static void
nautilus_change_password_view_initialize (NautilusChangePasswordView *view)
{
	NautilusBackground *background;
	CORBA_Environment ev;

	view->details = g_new0 (NautilusChangePasswordViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	view->details->pending = PENDING_NONE;
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (change_password_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	CORBA_exception_init (&ev);
	view->details->user_control = (EazelProxy_UserControl) oaf_activate_from_id (IID_EAZELPROXY, 0, NULL, &ev);
	if ( CORBA_NO_EXCEPTION != ev._major ) {
		/* FIXME bugzilla.eazel.com 2740: user should be
		 * warned that Ammonite may not be installed 
		 */

		g_warning ("Couldn't instantiate eazel-proxy");
		view->details->user_control = CORBA_OBJECT_NIL;
	}
	CORBA_exception_free (&ev);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_change_password_view_destroy (GtkObject *object)
{
	NautilusChangePasswordView *view;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	view = NAUTILUS_CHANGE_PASSWORD_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	CORBA_Object_release (view->details->user_control, &ev);
	g_free (view->details);

	CORBA_exception_free (&ev);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

NautilusView *
nautilus_change_password_view_get_nautilus_view (NautilusChangePasswordView *view)
{

	return view->details->nautilus_view;

}

void
nautilus_change_password_view_load_uri (NautilusChangePasswordView	*view,
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

	generate_change_password_form (view);
}

static void
change_password_load_location_callback (NautilusView	*nautilus_view, 
			      const char	*location,
			      NautilusChangePasswordView	*view)
{

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_change_password_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}


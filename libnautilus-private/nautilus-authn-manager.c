/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-authn-manager.c - machinary for handling authentication to URI's

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Michael Fleming <mikef@praxis.etla.net>
*/

/*
 * Questions:
 *  -- Can we center the authn dialog over the window where the operation
 *     is occuring?  (which doesn't even make sense in a drag between two windows)
 *  -- Can we provide a CORBA interface for components to access this info?
 *
 * dispatch stuff needs to go in utils
 *
 */

#include <config.h>

#include "nautilus-authn-manager.h"

#include <gnome.h>
#include <libgnome/gnome-i18n.h>
#include <eel/eel-password-dialog.h>
#include <libgnomevfs/gnome-vfs-app-context.h>
#include <libgnomevfs/gnome-vfs-standard-callbacks.h>
#include <libgnomevfs/gnome-vfs-utils.h>


#if 1
#define DEBUG_MSG(x) printf x
#else 
#define DEBUG_MSG(x)
#endif

static EelPasswordDialog *
construct_password_dialog (gboolean is_proxy_authn, const GnomeVFSCallbackSimpleAuthIn *in_args)
{
	char *message;
	EelPasswordDialog *dialog;

	message = g_strdup_printf (
		is_proxy_authn
			? _("Your HTTP Proxy requires you to log in.\n")
			: _("The following location requires you to log in:\n%s\n\n%s"), 
		in_args->uri, 
		in_args->auth_type == AuthTypeBasic
			? _("Your password will be transmitted unencrypted") 
			: _("Your password will be transmitted encrypted"));

	dialog = EEL_PASSWORD_DIALOG (eel_password_dialog_new (
			_("Authentication Required"),
			message,
			_(""),
			_(""),
			FALSE));

	g_free (message);

	return dialog;
}

static void
present_authn_dialog_blocking (gboolean is_proxy_authn,
			    const GnomeVFSCallbackSimpleAuthIn * in_args,
			    GnomeVFSCallbackSimpleAuthOut *out_args)
{
	EelPasswordDialog *dialog;
	gboolean dialog_result;

	dialog = construct_password_dialog (is_proxy_authn, in_args);

	dialog_result = eel_password_dialog_run_and_block (dialog);

	if (dialog_result) {
		out_args->username = eel_password_dialog_get_username (dialog);
		out_args->password = eel_password_dialog_get_password (dialog);
	} else {
		out_args->username = NULL;
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

typedef struct {
	const GnomeVFSCallbackSimpleAuthIn	*in_args;
	GnomeVFSCallbackSimpleAuthOut		*out_args;
	gboolean				 is_proxy_authn;

	volatile gboolean	complete;
} CallbackInfo;

static GMutex * 	callback_mutex;
static GCond *	 	callback_cond;

static void
mark_callback_completed (CallbackInfo *info)
{
	info->complete = TRUE;
	g_cond_broadcast (callback_cond);
}

static void
authn_dialog_button_clicked (GnomeDialog *dialog, gint button_number, CallbackInfo *info)
{
	DEBUG_MSG (("+%s button: %d\n", __FUNCTION__, button_number));

	if (button_number == GNOME_OK) {
		info->out_args->username 
			= eel_password_dialog_get_username (EEL_PASSWORD_DIALOG (dialog));
		info->out_args->password
			= eel_password_dialog_get_password (EEL_PASSWORD_DIALOG (dialog));
	}

	/* a NULL in the username field indicates "no credentials" to the caller */

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
authn_dialog_closed (GnomeDialog *dialog, CallbackInfo *info)
{
	DEBUG_MSG (("+%s\n", __FUNCTION__));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
authn_dialog_destroyed (GnomeDialog *dialog, CallbackInfo *info)
{
	DEBUG_MSG (("+%s\n", __FUNCTION__));

	mark_callback_completed (info);	
}

static gint /* GtkFunction */
present_authn_dialog_nonblocking (gpointer data)
{
	CallbackInfo *info;
	EelPasswordDialog *dialog;

	g_return_val_if_fail (data != NULL, 0);

	info = data;

	dialog = construct_password_dialog (info->is_proxy_authn, info->in_args);

	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);

	gtk_signal_connect (GTK_OBJECT (dialog), 
			     "clicked", 
			     GTK_SIGNAL_FUNC (authn_dialog_button_clicked),
			     data);

	gtk_signal_connect (GTK_OBJECT (dialog), 
			     "close", 
			     GTK_SIGNAL_FUNC (authn_dialog_closed),
			     data);

	gtk_signal_connect (GTK_OBJECT (dialog), 
			     "destroy", 
			     GTK_SIGNAL_FUNC (authn_dialog_destroyed),
			     data);

	gtk_widget_show_all (GTK_WIDGET (dialog));

	return 0;
}

static void 
run_authn_dialog_on_main_thread (gboolean is_proxy_authn,
			    	 const GnomeVFSCallbackSimpleAuthIn *in_args,
			    	 GnomeVFSCallbackSimpleAuthOut *out_args)
{
	CallbackInfo info;

	if (gnome_vfs_is_primary_thread ()) {
		present_authn_dialog_blocking (is_proxy_authn, in_args, out_args);
	} else {
		info.is_proxy_authn = is_proxy_authn;
		info.in_args = in_args;
		info.out_args = out_args;
		info.complete = FALSE;

		g_idle_add (present_authn_dialog_nonblocking, &info);

		/* The callback_mutex actually isn't used for anything */
		g_mutex_lock (callback_mutex);
		while (!info.complete) {
			g_cond_wait (callback_cond, callback_mutex);
		}
		g_mutex_unlock (callback_mutex);
	}
}


/* This function may be dispatched on a gnome-vfs job thread */
static void /* GnomeVFSCallback */
vfs_authn_callback (gpointer user_data, gconstpointer in, size_t in_size, gpointer out, size_t out_size)
{
	GnomeVFSCallbackSimpleAuthIn *in_real;
	GnomeVFSCallbackSimpleAuthOut *out_real;
	gboolean is_proxy_authn;
	
	g_return_if_fail (sizeof (GnomeVFSCallbackSimpleAuthIn) == in_size
		&& sizeof (GnomeVFSCallbackSimpleAuthOut) == out_size);

	g_return_if_fail (in != NULL);
	g_return_if_fail (out != NULL);

	in_real = (GnomeVFSCallbackSimpleAuthIn *)in;
	out_real = (GnomeVFSCallbackSimpleAuthOut *)out;

	is_proxy_authn = (user_data == GINT_TO_POINTER (1));

	DEBUG_MSG (("+%s uri:'%s' is_proxy_auth: %u\n", __FUNCTION__, in_real->uri, (unsigned) is_proxy_authn));

	run_authn_dialog_on_main_thread (is_proxy_authn, in_real, out_real);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

void
nautilus_authn_manager_initialize (void)
{
        GnomeVFSAppContext *app_context;

	callback_cond = g_cond_new ();
	callback_mutex = g_mutex_new ();


	app_context = gnome_vfs_app_context_new ();

	gnome_vfs_app_context_set_callback_full (app_context, 
		GNOME_VFS_HOOKNAME_BASIC_AUTH, 
		vfs_authn_callback, 
		GINT_TO_POINTER (0),
		TRUE,
		NULL);

	gnome_vfs_app_context_set_callback_full (app_context, 
		GNOME_VFS_HOOKNAME_HTTP_PROXY_AUTH, 
		vfs_authn_callback, 
		GINT_TO_POINTER (1),
		TRUE,
		NULL);

	gnome_vfs_app_context_push_takesref (app_context);	
}

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
 *  -- Can we center the authentication dialog over the window where the operation
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
#include <libgnomevfs/gnome-vfs-module-callback.h>
#include <libgnomevfs/gnome-vfs-standard-callbacks.h>
#include <libgnomevfs/gnome-vfs-utils.h>


#if 1
#define DEBUG_MSG(x) printf x
#else 
#define DEBUG_MSG(x)
#endif

static EelPasswordDialog *
construct_password_dialog (gboolean is_proxy_authentication, const GnomeVFSModuleCallbackAuthenticationIn *in_args)
{
	char *message;
	EelPasswordDialog *dialog;

	message = g_strdup_printf (
		is_proxy_authentication
			? _("Your HTTP Proxy requires you to log in.\n")
			: _("You must log in to access \"%s\".\n\n%s"), 
		in_args->uri, 
		in_args->auth_type == AuthTypeBasic
			? _("Your password will be transmitted unencrypted.") 
			: _("Your password will be transmitted encrypted."));

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
present_authentication_dialog_blocking (gboolean is_proxy_authentication,
			    const GnomeVFSModuleCallbackAuthenticationIn * in_args,
			    GnomeVFSModuleCallbackAuthenticationOut *out_args)
{
	EelPasswordDialog *dialog;
	gboolean dialog_result;

	dialog = construct_password_dialog (is_proxy_authentication, in_args);

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
	const GnomeVFSModuleCallbackAuthenticationIn	*in_args;
	GnomeVFSModuleCallbackAuthenticationOut		*out_args;
	gboolean				 is_proxy_authentication;

	GnomeVFSModuleCallbackResponse response;
	gpointer response_data;
} CallbackInfo;

static GMutex * 	callback_mutex;
static GCond *	 	callback_cond;

static void
mark_callback_completed (CallbackInfo *info)
{
	info->response (info->response_data);
	g_free (info);
}

static void
authentication_dialog_button_clicked (GnomeDialog *dialog, 
				      gint button_number, 
				      CallbackInfo *info)
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
authentication_dialog_closed (GnomeDialog *dialog, CallbackInfo *info)
{
	DEBUG_MSG (("+%s\n", __FUNCTION__));

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
authentication_dialog_destroyed (GnomeDialog *dialog, CallbackInfo *info)
{
	DEBUG_MSG (("+%s\n", __FUNCTION__));

	mark_callback_completed (info);	
}

static gint /* GtkFunction */
present_authentication_dialog_nonblocking (CallbackInfo *info)
{
	EelPasswordDialog *dialog;

	g_return_val_if_fail (info != NULL, 0);

	dialog = construct_password_dialog (info->is_proxy_authentication, info->in_args);

	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);

	gtk_signal_connect (GTK_OBJECT (dialog), 
			     "clicked", 
			     GTK_SIGNAL_FUNC (authentication_dialog_button_clicked),
			     info);

	gtk_signal_connect (GTK_OBJECT (dialog), 
			     "close", 
			     GTK_SIGNAL_FUNC (authentication_dialog_closed),
			     info);

	gtk_signal_connect (GTK_OBJECT (dialog), 
			     "destroy", 
			     GTK_SIGNAL_FUNC (authentication_dialog_destroyed),
			     info);

	gtk_widget_show_all (GTK_WIDGET (dialog));

	return 0;
}

static void /* GnomeVFSAsyncModuleCallback */
vfs_async_authentication_callback (gconstpointer in, size_t in_size, 
				   gpointer out, size_t out_size, 
				   gpointer user_data,
				   GnomeVFSModuleCallbackResponse response,
				   gpointer response_data)
{
	GnomeVFSModuleCallbackAuthenticationIn *in_real;
	GnomeVFSModuleCallbackAuthenticationOut *out_real;
	gboolean is_proxy_authentication;
	CallbackInfo *info;
	
	puts ("XXX - async callback invoked.");

	g_return_if_fail (sizeof (GnomeVFSModuleCallbackAuthenticationIn) == in_size
		&& sizeof (GnomeVFSModuleCallbackAuthenticationOut) == out_size);

	g_return_if_fail (in != NULL);
	g_return_if_fail (out != NULL);

	in_real = (GnomeVFSModuleCallbackAuthenticationIn *)in;
	out_real = (GnomeVFSModuleCallbackAuthenticationOut *)out;

	is_proxy_authentication = (user_data == GINT_TO_POINTER (1));

	DEBUG_MSG (("+%s uri:'%s' is_proxy_auth: %u\n", __FUNCTION__, in_real->uri, (unsigned) is_proxy_authentication));

	info = g_new (CallbackInfo, 1);

	info->is_proxy_authentication = is_proxy_authentication;
	info->in_args = in_real;
	info->out_args = out_real;
	info->response = response;
	info->response_data = response_data;

	present_authentication_dialog_nonblocking (info);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

static void /* GnomeVFSModuleCallback */
vfs_authentication_callback (gconstpointer in, size_t in_size, 
			     gpointer out, size_t out_size, 
			     gpointer user_data)
{
	GnomeVFSModuleCallbackAuthenticationIn *in_real;
	GnomeVFSModuleCallbackAuthenticationOut *out_real;
	gboolean is_proxy_authentication;
	
	g_return_if_fail (sizeof (GnomeVFSModuleCallbackAuthenticationIn) == in_size
		&& sizeof (GnomeVFSModuleCallbackAuthenticationOut) == out_size);

	g_return_if_fail (in != NULL);
	g_return_if_fail (out != NULL);

	in_real = (GnomeVFSModuleCallbackAuthenticationIn *)in;
	out_real = (GnomeVFSModuleCallbackAuthenticationOut *)out;

	is_proxy_authentication = (user_data == GINT_TO_POINTER (1));

	DEBUG_MSG (("+%s uri:'%s' is_proxy_auth: %u\n", __FUNCTION__, in_real->uri, (unsigned) is_proxy_authentication));

	present_authentication_dialog_blocking (is_proxy_authentication, in_real, out_real);

	DEBUG_MSG (("-%s\n", __FUNCTION__));
}

void
nautilus_authentication_manager_initialize (void)
{
	callback_cond = g_cond_new ();
	callback_mutex = g_mutex_new ();

	gnome_vfs_async_module_callback_set_default (GNOME_VFS_MODULE_CALLBACK_AUTHENTICATION,
						     vfs_async_authentication_callback, 
						     GINT_TO_POINTER (0),
						     NULL);
	gnome_vfs_async_module_callback_set_default (GNOME_VFS_MODULE_CALLBACK_HTTP_PROXY_AUTHENTICATION, 
						     vfs_async_authentication_callback, 
						     GINT_TO_POINTER (1),
						     NULL);

	/* These are in case someone makes a synchronous http call for
	 * some reason. 
	 */

	gnome_vfs_module_callback_set_default (GNOME_VFS_MODULE_CALLBACK_AUTHENTICATION,
					       vfs_authentication_callback, 
					       GINT_TO_POINTER (0),
					       NULL);
	gnome_vfs_module_callback_set_default (GNOME_VFS_MODULE_CALLBACK_HTTP_PROXY_AUTHENTICATION, 
					       vfs_authentication_callback, 
					       GINT_TO_POINTER (1),
					       NULL);
}

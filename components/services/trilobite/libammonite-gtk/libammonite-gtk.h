/* $Id$
 * 
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
 * Author:  Michael Fleming <mfleming@eazel.com>
 *
 */

#ifndef _LIBAMMONITE_GTK_H_
#define _LIBAMMONITE_GTK_H_

#include <libtrilobite/libammonite.h>
#include "ammonite-login-dialog.h"

typedef void (*AmmonitePromptLoginCb) (
	gpointer user_data, 
	const EazelProxy_User *user, 
	const EazelProxy_AuthnFailInfo *fail_info
);

gboolean
ammonite_do_prompt_login_async (
	const char *username, 
	const char *services_redirect_uri, 
	const char *services_login_path,
	gpointer user_data,
	AmmonitePromptLoginCb callback
);

EazelProxy_User *
ammonite_do_prompt_login (
	const char *username, 
	const char *services_redirect_uri, 
	const char *services_login_path,
	/*OUT*/ CORBA_long *p_fail_code
);

gboolean
ammonite_do_prompt_dialog (
	const char *user, 
	const char *pw, 
	gboolean retry, 
	char **p_user, 
	char **p_pw
);

void ammonite_do_authn_error_dialog (void);
void ammonite_do_network_error_dialog (void);



#endif /* _LIBAMMONITE_GTK_H_ */
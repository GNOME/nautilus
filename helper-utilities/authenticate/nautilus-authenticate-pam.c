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
 * Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

/* nautilus-authenticate-pam.c - Use PAM to authenticate a user.
 */

#include <config.h>
#include "nautilus-authenticate.h"

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <string.h>

typedef struct _PamConvData
{
	char *username;
	char *password;
} PamConvData;

static int pam_conversion_func (int				num_msg, 
				const struct pam_message	**msg, 
				struct pam_response		**response, 
				void				*appdata_ptr)
{
	PamConvData * pdata = (PamConvData *) appdata_ptr;
	
	struct pam_response * reply = 
		(struct pam_response *) malloc (sizeof (struct pam_response) * num_msg);
	
	g_assert (pdata);
	g_assert (reply);

	if (reply) {
		int replies;

		for (replies = 0; replies < num_msg; replies++) {
			switch (msg[replies]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = strdup (pdata->username);
				/* PAM frees resp */
				break;
				
			case PAM_PROMPT_ECHO_OFF:
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = strdup (pdata->password);
				/* PAM frees resp */
				break;
				
			case PAM_TEXT_INFO:
				/* nothing */
				
			case PAM_ERROR_MSG:
				/* Ignore */
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = NULL;
				break;
				
			default:
				/* Huh? */
				free (reply);
				
				reply=NULL;
				
				return PAM_CONV_ERR;
			}
		}
		
		if (reply)
			*response = reply;
		
		return PAM_SUCCESS;
	}
	
	return PAM_CONV_ERR;
}

gboolean
nautilus_authenticate_authenticate(const char *username,
				   const char *password)
{
	char * username_copy = g_strdup(username);
	char * password_copy = g_strdup(password);

	gboolean rv = FALSE;
	pam_handle_t * pam_handle = NULL;
	
	struct pam_conv pam_conv_data;
	
	static PamConvData client_data;
	
	client_data.username = username_copy;
	client_data.password = password_copy;
	
	/* Setup the pam conversion structure */
	pam_conv_data.conv = pam_conversion_func;
	pam_conv_data.appdata_ptr = (void *) &client_data;
	
	/* Start pam */
	if (pam_start("su", username_copy, &pam_conv_data, &pam_handle) == PAM_SUCCESS) {
		/* Attempt auth */
		if (pam_authenticate(pam_handle, PAM_SILENT) == PAM_SUCCESS) {
			/* Authentication worked */
			pam_end (pam_handle, PAM_SUCCESS);
			
			rv = TRUE;
		}
	}
	
	if (!rv)
		pam_end (pam_handle, 0);
	
	g_free (username_copy);
	g_free (password_copy);

	return rv;
}

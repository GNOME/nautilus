/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#ifndef EAZEL_SOFTCAT_PRIVATE_H
#define EAZEL_SOFTCAT_PRIVATE_H

#include "eazel-softcat.h"

#define SOFTCAT_DEFAULT_SERVER		"services.eazel.com"
#define SOFTCAT_DEFAULT_PORT		80
#define SOFTCAT_DEFAULT_CGI_PATH	"/catalog/find"

struct _EazelSoftCatPrivate {
	char *server;
	unsigned int port;
	char *server_str;
	char *cgi_path;
	char *username;		/* username on the service (can be NULL for default user) */
	gboolean use_authn;	/* use SSL proxy?  won't work for "slim" */

	/* number of times to try connecting to softcat, and delay between attempts (in usec) */
	unsigned int retries;
        unsigned int delay;

	/* This is used to track the server update status */
	gboolean server_update_set;
	guint server_update_val;
};

#endif /* EAZEL_SOFTCAT_PRIVATE_H */

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
 *
 */

#include <config.h>

#include "shared-service-utilities.h"

#include <stdio.h>

/* utility routine to go to another uri */

void
go_to_uri (NautilusView	*nautilus_view, char	*uri) {

	nautilus_view_open_location (nautilus_view, uri);

}

/* utility for checking a uri */

gboolean
is_location (char	*document_string,	const char	*place_string) {

	return document_string && !strncmp (document_string + 1, place_string, strlen (place_string));

}

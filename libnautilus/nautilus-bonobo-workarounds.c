/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Darin Adler <darin@eazel.com>
 *
 */

#include <config.h>
#include "nautilus-bonobo-workarounds.h"

#include <bonobo/bonobo-stream.h>

POA_Bonobo_Unknown__epv *
nautilus_bonobo_object_get_epv (void)
{
	static POA_Bonobo_Unknown__epv bonobo_object_epv;
	static gboolean set_up;
	POA_Bonobo_Unknown__epv *epv;

	/* Make our own copy. */
 	if (!set_up) {
		epv = bonobo_object_get_epv ();
		bonobo_object_epv = *epv;
		g_free (epv);
		set_up = TRUE;
	}

	return &bonobo_object_epv;
}

POA_Bonobo_Stream__epv *
nautilus_bonobo_stream_get_epv (void)
{
	static POA_Bonobo_Stream__epv bonobo_stream_epv;
	static gboolean set_up;
	POA_Bonobo_Stream__epv *epv;

	/* Make our own copy. */
 	if (!set_up) {
		epv = bonobo_stream_get_epv ();
		bonobo_stream_epv = *epv;
		g_free (epv);
		set_up = TRUE;
	}

	return &bonobo_stream_epv;
}

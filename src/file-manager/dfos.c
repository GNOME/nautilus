/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* dfos.h - Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#include <config.h>
#include "dfos.h"

#include <gnome.h>
#include <orb/orbit.h>


struct _DFOS {
	GNOME_Desktop_FileOperationService corba_objref;
};


DFOS *
dfos_new (void)
{
	GNOME_Desktop_FileOperationService corba_objref;
	DFOS *dfos;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	dfos = g_new (DFOS, 1);
	corba_objref = dfos_corba_init (dfos);
	if (CORBA_Object_is_nil (corba_objref, &ev)) {
		g_free (dfos);
		dfos = NULL;
	} else {
		dfos->corba_objref = corba_objref;
	}

	CORBA_exception_free (&ev);

	return dfos;
}

void
dfos_destroy (DFOS *dfos)
{
	CORBA_Environment ev;

	g_return_if_fail (dfos != NULL);

	CORBA_exception_init (&ev);
	CORBA_Object_release (dfos->corba_objref, &ev);
	CORBA_exception_free (&ev);

	g_free (dfos);
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 *  trilobite-redirect: functions to fetch a redirection table from
 *  a remote xml file, store it in gconf, and then lookup entries
 *  later.  this may only be useful for eazel services.
 *
 *  Copyright (C) 2000, 2001 Eazel, Inc
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
 *  Authors: Robey Pointer <robey@eazel.com>
 */

#ifndef TRILOBITE_REDIRECT_H
#define TRILOBITE_REDIRECT_H

#include <libgnomevfs/gnome-vfs.h>
#include <glib.h>

typedef void     (* TrilobiteRedirectFetchCallback) (GnomeVFSResult result,
						     gboolean parsed_xml,
						     gpointer callback_data);

typedef struct TrilobiteRedirectFetchHandle TrilobiteRedirectFetchHandle;


TrilobiteRedirectFetchHandle *trilobite_redirect_fetch_table_async (const char *uri,
								    TrilobiteRedirectFetchCallback callback,
								    gpointer callback_data);

void                          trilobite_redirect_fetch_table_cancel (TrilobiteRedirectFetchHandle *handle);


char *trilobite_redirect_lookup (const char *key);

const char *trilobite_get_services_address (void);

#endif	/* TRILOBITE_REDIRECT_H */

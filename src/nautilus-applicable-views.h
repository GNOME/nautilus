/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Coypright (C) 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *           Maciej Stachowiak <mjs@eazel.com>
 *
 */

/* nautilus-applicable-views.h: Interface for mapping a location
   change request to a set of views and actual URL to be loaded. */

#ifndef NAUTILUS_APPLICABLE_VIEWS_H
#define NAUTILUS_APPLICABLE_VIEWS_H

#include <libnautilus-private/nautilus-view-identifier.h>

typedef struct NautilusDetermineViewHandle NautilusDetermineViewHandle;

/* These are the different ways that Nautilus can fail to locate an
 * initial view for a given location NAUTILUS_DETERMINE_VIEW_OK means
 * the uri was displayed successfully. These are similar to
 * GnomeVFSResults but there are Nautilus-specific codes and many of
 * the GnomeVFSResults are treated the same here.
 */
typedef enum {
	NAUTILUS_DETERMINE_VIEW_OK,
	NAUTILUS_DETERMINE_VIEW_UNSPECIFIC_ERROR,
	NAUTILUS_DETERMINE_VIEW_NO_HANDLER_FOR_TYPE,
	NAUTILUS_DETERMINE_VIEW_NOT_FOUND,
	NAUTILUS_DETERMINE_VIEW_UNSUPPORTED_SCHEME,
	NAUTILUS_DETERMINE_VIEW_INVALID_URI,
	NAUTILUS_DETERMINE_VIEW_LOGIN_FAILED,
	NAUTILUS_DETERMINE_VIEW_SERVICE_NOT_AVAILABLE,
	NAUTILUS_DETERMINE_VIEW_ACCESS_DENIED,
	NAUTILUS_DETERMINE_VIEW_HOST_NOT_FOUND,
	NAUTILUS_DETERMINE_VIEW_HOST_HAS_NO_ADDRESS,
} NautilusDetermineViewResult;

typedef void (* NautilusDetermineViewCallback) (NautilusDetermineViewHandle  *handle,
                                                NautilusDetermineViewResult   result,
                                                const NautilusViewIdentifier *initial_view,
                                                gpointer                      callback_data);

NautilusDetermineViewHandle *nautilus_determine_initial_view        (const char                    *location,
                                                                     NautilusDetermineViewCallback  callback,
                                                                     gpointer                       callback_data);
void                         nautilus_determine_initial_view_cancel (NautilusDetermineViewHandle   *handle);

#endif /* NAUTILUS_APPLICABLE_VIEWS_H */

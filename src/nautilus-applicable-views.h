/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */

/* nautilus-applicable-views.h: Interface for mapping a location change request to a set of views and actual URL to be loaded. */

#ifndef NAUTILUS_URI_MAP_H
#define NAUTILUS_URI_MAP_H

#include <glib.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libnautilus/nautilus-view-component.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>

typedef struct NautilusNavigationInfo NautilusNavigationInfo;

/* These are the different ways that Nautilus can fail to
 * display the contents of a given uri. NAUTILUS_NAVIGATION_RESULT_OK
 * means the uri was displayed successfully. These are similar to
 * GnomeVFSResults but there are nautilus-specific codes and many of
 * the GnomeVFSResults are treated the same here.
 */
typedef enum {
	NAUTILUS_NAVIGATION_RESULT_UNDEFINED = -1,
	NAUTILUS_NAVIGATION_RESULT_OK,
	NAUTILUS_NAVIGATION_RESULT_UNSPECIFIC_ERROR,
	NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE,
	NAUTILUS_NAVIGATION_RESULT_NOT_FOUND,
	NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME,
	NAUTILUS_NAVIGATION_RESULT_INVALID_URI,
	NAUTILUS_NAVIGATION_RESULT_LOGIN_FAILED,
	NAUTILUS_NAVIGATION_RESULT_SERVICE_NOT_AVAILABLE
} NautilusNavigationResult;

typedef void (*NautilusNavigationCallback) (NautilusNavigationResult result,
                                            NautilusNavigationInfo  *info,
                                            gpointer                 callback_data);

struct NautilusNavigationInfo {
        char *location;

	char *referring_iid;		     		/* iid of content view that we're coming from */
	NautilusViewIdentifier *initial_content_id;	/* NautilusViewIdentifier for content view that we're going to display */
        GList *files;                        		/* NautilusFile's for files in the dir, if it is one. */
        GList *explicit_iids;                		/* IIDs explicitly mentioned in the metafile. */

	/* internal usage */
	NautilusNavigationCallback callback;
	gpointer callback_data;
	GnomeVFSAsyncHandle *ah;
        NautilusDirectory *directory;
};

NautilusNavigationInfo *nautilus_navigation_info_new    (const char                 *location,
                                                         NautilusNavigationCallback  ready_callback,
                                                         gpointer                    callback_data,
                                                         const char                 *referring_iid);
void                    nautilus_navigation_info_cancel (NautilusNavigationInfo     *info);
void                    nautilus_navigation_info_free   (NautilusNavigationInfo     *info);

#endif

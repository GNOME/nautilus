/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
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
/* ntl-uri-map.c: Implementation of routines for mapping a location change request to a set of views and actual URL to be loaded. */

#include "ntl-uri-map.h"
#include <libgnorba/gnorba.h>

NautilusNavigationInfo *
nautilus_navinfo_new(NautilusNavigationInfo *navinfo,
		     Nautilus_NavigationRequestInfo *nri,
		     NautilusLocationReference referring_uri,
		     NautilusLocationReference actual_referring_uri,
		     const char *referring_content_type,
		     GtkWidget *requesting_view)
{
  memset(navinfo, 0, sizeof(*navinfo));

  navinfo->navinfo.requested_uri = nri->requested_uri;
  navinfo->navinfo.referring_uri = referring_uri;
  navinfo->navinfo.actual_referring_uri = actual_referring_uri;
  navinfo->navinfo.referring_content_type = (char *)referring_content_type;

  navinfo->requesting_view = requesting_view;

  navinfo->navinfo.content_type = gnome_mime_type_of_file(navinfo->navinfo.requested_uri);

  /* Given a content type and a URI, what do we do? Basically the "expert system" below
     tries to answer that question

     Check if the URI is in an abnormal scheme (e.g. one not supported by gnome-vfs)
       If so
          Lookup a content view by scheme name, go.
          Lookup meta views by scheme name, go.

       If not
          Figure out content type.
          Lookup a content view by content type, go.
          Lookup meta views by content type, go.

     The lookup-and-go process works like:
         Generate a list of all possibilities ordered by quality.
         Put possibilities on menu.

         Find if the user has specified any default(s) globally, modify selection.
         Find if the user has specified any default(s) per-page, modify selection.
  */

  return NULL;
}

void
nautilus_navinfo_free(NautilusNavigationInfo *navinfo)
{
  g_free(navinfo->navinfo.content_type);
}

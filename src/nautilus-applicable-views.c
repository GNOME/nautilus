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
/* ntl-uri-map.c: Implementation of routines for mapping a location
   change request to a set of views and actual URL to be loaded. */

#include "ntl-uri-map.h"
#include <libgnorba/gnorba.h>

NautilusNavigationInfo *
nautilus_navinfo_new(NautilusNavigationInfo *navinfo,
		     Nautilus_NavigationRequestInfo *nri,
                     Nautilus_NavigationInfo *old_navinfo,
		     NautilusView *requesting_view)
{
  const char *meta_keys[] = {"icon-filename", NULL};
  memset(navinfo, 0, sizeof(*navinfo));

  navinfo->navinfo.requested_uri = nri->requested_uri;
  if(old_navinfo)
    {
      navinfo->navinfo.referring_uri = old_navinfo->requested_uri;
      navinfo->navinfo.actual_referring_uri = old_navinfo->actual_uri;
      navinfo->navinfo.referring_content_type = old_navinfo->content_type;
    }

  navinfo->requesting_view = requesting_view;

  navinfo->vfs_fileinfo = gnome_vfs_file_info_new();
  gnome_vfs_get_file_info(navinfo->navinfo.requested_uri,
                          navinfo->vfs_fileinfo,
                          GNOME_VFS_FILE_INFO_GETMIMETYPE
                          |GNOME_VFS_FILE_INFO_FOLLOWLINKS,
                          meta_keys);
  navinfo->navinfo.content_type = (char *)gnome_vfs_file_info_get_mime_type(navinfo->vfs_fileinfo);

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

  g_message("Content type of %s is %s",
            navinfo->navinfo.requested_uri,
            navinfo->navinfo.content_type);
  if(!strcmp(navinfo->navinfo.content_type, "text/html"))
    {
      navinfo->content_iid = "embeddable:explorer-html-component";
    }
  else if(!strcmp(navinfo->navinfo.content_type, "text/plain"))
    {
      navinfo->content_iid = "embeddable:text-plain";
    }
  else if(!strcmp(navinfo->navinfo.content_type, "special/directory"))
    {
      navinfo->content_iid = "ntl_file_manager";
    }


  return navinfo;
}

void
nautilus_navinfo_free(NautilusNavigationInfo *navinfo)
{
  gnome_vfs_file_info_destroy(navinfo->vfs_fileinfo);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

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
/* ntl-uri-map.c: Implementation of routines for mapping a location
   change request to a set of views and actual URL to be loaded. */

#include "ntl-uri-map.h"
#include "ntl-prefs.h"

#include <libgnorba/gnorba.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

static NautilusViewIdentifier *
nautilus_view_identifier_new (const char *iid, const char *name)
{
  NautilusViewIdentifier *new_identifier;

  g_return_val_if_fail (iid != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  new_identifier = g_new0 (NautilusViewIdentifier, 1);
  new_identifier->iid = g_strdup (iid);
  new_identifier->name = g_strdup (name);

  return new_identifier;
}

static void
nautilus_view_identifier_free (NautilusViewIdentifier *identifier)
{
  if (identifier != NULL)
  {
    g_free (identifier->iid);
    g_free (identifier->name);
    g_free (identifier);
  }
}

static void
nautilus_navinfo_append_globals(gpointer value, gpointer data)
{
  GSList **target = data;

  *target = g_slist_prepend(*target, g_strdup(value));
}

static NautilusNavigationResult
get_nautilus_navigation_result_from_gnome_vfs_result (GnomeVFSResult gnome_vfs_result)
{
  switch (gnome_vfs_result)
    {
      case GNOME_VFS_OK:
        return NAUTILUS_NAVIGATION_RESULT_OK;
      case GNOME_VFS_ERROR_NOTFOUND:
      case GNOME_VFS_ERROR_HOSTNOTFOUND:
        return NAUTILUS_NAVIGATION_RESULT_NOT_FOUND;
      case GNOME_VFS_ERROR_INVALIDURI:
        return NAUTILUS_NAVIGATION_RESULT_INVALID_URI;
      case GNOME_VFS_ERROR_NOTSUPPORTED:
        return NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME;
      default:
        /* Whenever this message fires, we should consider adding a specific case
         * to make the error as comprehensible as possible to the user. Please
         * bug me (sullivan@eazel.com) if you see this fire and don't have the
         * inclination to immediately make a good message yourself (tell me
         * what GnomeVFSResult code the message reported, and what caused it to
         * fire).
         */
        g_message ("in ntl-uri-map.c, got unhandled GnomeVFSResult %d.", gnome_vfs_result);
        return NAUTILUS_NAVIGATION_RESULT_UNSPECIFIC_ERROR;
      }
}

static void
my_notify_when_ready(GnomeVFSAsyncHandle *ah, GnomeVFSResult result,
                     GnomeVFSFileInfo *vfs_fileinfo,
                     gpointer data)
{
  NautilusNavigationInfo *navinfo = data;
  NautilusNavigationInfoFunc notify_ready = navinfo->notify_ready;
  gpointer notify_ready_data = navinfo->data;

  if (navinfo->ah) {
    gnome_vfs_async_cancel (navinfo->ah);
    navinfo->ah = NULL;
  }

  if(result != GNOME_VFS_OK)
    {
      /* Map GnomeVFSResult to one of the types that Nautilus knows how to handle.
       * Leave navinfo intact so notify_ready function can access the uri.
       * (notify_ready function is responsible for freeing navinfo).
       */
      navinfo->result_code = get_nautilus_navigation_result_from_gnome_vfs_result (result);
      goto out;
    }

  navinfo->navinfo.content_type = g_strdup(gnome_vfs_file_info_get_mime_type(vfs_fileinfo));

  /* Given a content type and a URI, what do we do? Basically the "expert system" below
     tries to answer that question

     Check if the URI is in an abnormal scheme (e.g. one not supported by gnome-vfs)
       If so
          Lookup content views by scheme name, go.
          Lookup meta views by scheme name, go.

       If not
          Figure out content type.
          Lookup content views by content type, go.
          Lookup meta views by content type, go.

     The lookup-and-go process works like:
         Generate a list of all possibilities ordered by quality.
         Put possibilities on menu.

         Find if the user has specified any default(s) globally, modify selection.
         Find if the user has specified any default(s) per-page, modify selection.
  */

  /* This is just a hardcoded hack until OAF works with Bonobo.
     In the future we will use OAF queries to determine this information. */
  if(!navinfo->navinfo.content_type)
    navinfo->navinfo.content_type = g_strdup("text/plain");

  if(!strcmp(navinfo->navinfo.content_type, "text/html"))
    {
      navinfo->default_content_iid = "ntl_web_browser";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_web_browser", "Web Page"));
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("embeddable:text-plain", "Text"));
    }
  else if(!strcmp(navinfo->navinfo.content_type, "text/plain"))
    {
      navinfo->default_content_iid = "embeddable:text-plain";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new (navinfo->default_content_iid, "Text"));
    }
  else if(!strncmp(navinfo->navinfo.content_type, "image/", 6))
    {
      navinfo->default_content_iid = "embeddable:image-generic";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new (navinfo->default_content_iid, "Image"));
    }
  else if(!strcmp(navinfo->navinfo.content_type, "special/directory")
          || !strcmp(navinfo->navinfo.content_type, "application/x-nautilus-vdir"))
    {
      navinfo->default_content_iid = "ntl_file_manager_icon_view";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_file_manager_icon_view", "Icons"));
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_file_manager_list_view", "List"));
    }
  else
    {
      /* Can't display file; nothing registered to handle this file type. */
      navinfo->result_code = NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE;
      goto out;
    }

  g_slist_foreach(nautilus_prefs.global_meta_views, nautilus_navinfo_append_globals, &navinfo->meta_iids);

 out:
  notify_ready(navinfo, notify_ready_data);
}

void
nautilus_navinfo_init(void)
{
}

gpointer
nautilus_navinfo_new(Nautilus_NavigationRequestInfo *nri,
                     Nautilus_NavigationInfo *old_navinfo,
                     NautilusNavigationInfoFunc notify_when_ready,
                     gpointer notify_data)
{
  GnomeVFSResult res;
  const char *meta_keys[] = {"icon-filename", NULL};
  NautilusNavigationInfo *navinfo;
                          
  navinfo = g_new0(NautilusNavigationInfo, 1);

  navinfo->notify_ready = notify_when_ready;
  navinfo->data = notify_data;

  if(old_navinfo)
    {
      navinfo->navinfo.referring_uri = old_navinfo->requested_uri;
      navinfo->navinfo.actual_referring_uri = old_navinfo->actual_uri;
      navinfo->navinfo.referring_content_type = old_navinfo->content_type;
    }

  navinfo->navinfo.requested_uri = g_strdup(nri->requested_uri);

  res = gnome_vfs_async_get_file_info(&navinfo->ah, navinfo->navinfo.requested_uri,
                                      GNOME_VFS_FILE_INFO_GETMIMETYPE
                                      |GNOME_VFS_FILE_INFO_FOLLOWLINKS,
                                      meta_keys, my_notify_when_ready, navinfo);

  if(res != GNOME_VFS_OK)
    {
      /* Note: Not sure if or when this case ever occurs. res == GNOME_VFS_OK
       * for normally-handled uris, for non-existent uris, and for uris for which
       * Nautilus has no content viewer.
       */    
      navinfo->result_code = get_nautilus_navigation_result_from_gnome_vfs_result (res);
      notify_when_ready(navinfo, notify_data);
    }

  return navinfo?navinfo->ah:NULL;
}

void
nautilus_navinfo_free(NautilusNavigationInfo *navinfo)
{
  g_return_if_fail(navinfo != NULL);

  if (navinfo->ah)
    gnome_vfs_async_cancel (navinfo->ah);

  g_slist_foreach(navinfo->content_identifiers, (GFunc)nautilus_view_identifier_free, NULL);
  g_slist_free(navinfo->content_identifiers);
  g_slist_foreach(navinfo->meta_iids, (GFunc)g_free, NULL);
  g_slist_free(navinfo->meta_iids);
  g_free(navinfo->navinfo.requested_uri);
  g_free(navinfo->navinfo.actual_uri);
  g_free(navinfo->navinfo.content_type);
  g_free(navinfo);
}

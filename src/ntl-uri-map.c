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

#include <libnautilus/nautilus-directory.h>
#include <libnautilus/nautilus-metadata.h>
#include <libnautilus/nautilus-global-preferences.h>

#include <libgnorba/gnorba.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

/* forward declarations */

static void add_components_from_metadata(NautilusNavigationInfo *navinfo);
static void add_meta_view_iids_from_preferences(NautilusNavigationInfo *navinfo);

/* Nautilus View Identifiers associate a component name with a user displayable name */

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

/* GCompareFunc-style function for checking whether a given string matches
 * the iid of a NautilusViewIdentifier. Returns 0 if there is a match.
 */
static int
check_iid (gconstpointer a, gconstpointer b)
{
	NautilusViewIdentifier *identifier;
	char *string;

	identifier = (NautilusViewIdentifier *)a;
	string = (char *)b;

	return strcmp (identifier->iid, string) != 0;
}

static void
set_initial_content_iid (NautilusNavigationInfo *navinfo,
			 const char *fallback_value)
{
	NautilusDirectory *directory;
	char *remembered_value;
	const char *value;

	g_assert (fallback_value != NULL);
	g_assert (g_slist_length (navinfo->content_identifiers) > 0);
	
	value = fallback_value;

	directory = nautilus_directory_get (navinfo->navinfo.requested_uri);
	if (directory != NULL) {
		remembered_value = nautilus_directory_get_metadata (directory,
								    NAUTILUS_INITIAL_VIEW_METADATA_KEY,
							 	    NULL);

		/* Use the remembered value if it's non-NULL and in the list of choices. */
		if (remembered_value != NULL) {
			if (g_slist_find_custom (navinfo->content_identifiers, remembered_value, check_iid)) {
				value = remembered_value;
			} else {
				g_message ("Unknown iid \"%s\" stored for %s", remembered_value, navinfo->navinfo.requested_uri);
			}
		}
	}

	navinfo->initial_content_iid = g_strdup (value);

	g_free (remembered_value);
}

static void
my_notify_when_ready(GnomeVFSAsyncHandle *ah, GnomeVFSResult result,
                     GnomeVFSFileInfo *vfs_fileinfo,
                     gpointer data)
{
  NautilusNavigationInfo *navinfo = data;
  NautilusNavigationInfoFunc notify_ready = navinfo->notify_ready;
  gpointer notify_ready_data = navinfo->data;
  const char *fallback_iid;

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
      if(navinfo->result_code == NAUTILUS_NAVIGATION_RESULT_UNSUPPORTED_SCHEME
         || navinfo->result_code == NAUTILUS_NAVIGATION_RESULT_INVALID_URI)
        {
          /* Special scheme mapping stuff */
          if(!strncmp(navinfo->navinfo.requested_uri, "irc://", 6))
            {
              navinfo->navinfo.content_type = g_strdup("special/x-irc-session");
              navinfo->result_code = NAUTILUS_NAVIGATION_RESULT_OK;
            }
          else
            goto out;
        }
      else
        goto out;
    }
  else
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
      fallback_iid = "ntl_web_browser";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_web_browser", "Web Page"));
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("embeddable:text-plain", "Text"));
    }
  else if(!strcmp(navinfo->navinfo.content_type, "text/plain"))
    {
      fallback_iid = "embeddable:text-plain";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("embeddable:text-plain", "Text"));
    }
  else if(!strncmp(navinfo->navinfo.content_type, "image/", 6))
    {
      fallback_iid = "eog-image-viewer";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("eog-image-viewer", "Image"));
    }
  else if(!strcmp(navinfo->navinfo.content_type, "special/x-irc-session"))
    {
      fallback_iid = "xchat";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("xchat", "Chat room"));
    }
  else if(!strcmp(navinfo->navinfo.content_type, "special/directory")
          || !strcmp(navinfo->navinfo.content_type, "application/x-nautilus-vdir"))
    {
      fallback_iid = "ntl_file_manager_icon_view";
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_file_manager_icon_view", "Icons"));
      navinfo->content_identifiers = g_slist_append (
                                                     navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_file_manager_list_view", "List"));
       
      /* besides the information in OAF/GConf, we also want to offer components that are specifically refered to in the metadata,
      so we ask the metadata for content views here and add them accordingly.  */      
	   
	/* FIXME:  for now, we just do this for directories but it should apply to all places with available metadata */
	add_components_from_metadata(navinfo);
    }
  else if(!strcmp(navinfo->navinfo.content_type, "special/webdav-directory"))
    {
      fallback_iid = "ntl_web_browser";
      navinfo->content_identifiers = g_slist_append (navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_web_browser", "Web Page"));
      navinfo->content_identifiers = g_slist_append (navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_file_manager_icon_view", "Icons"));
      navinfo->content_identifiers = g_slist_append (navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("ntl_file_manager_list_view", "List"));
      navinfo->content_identifiers = g_slist_append (navinfo->content_identifiers, 
                                                     nautilus_view_identifier_new ("embeddable:text-plain", "Text"));

       
      /* besides the information in OAF/GConf, we also want to offer components that are specifically refered to in the metadata,
      so we ask the metadata for content views here and add them accordingly.  */      
	   
	/* FIXME:  for now, we just do this for directories but it should apply to all places with available metadata */
	add_components_from_metadata(navinfo);
    }
  else
    {
      /* Can't display file; nothing registered to handle this file type. */
      navinfo->result_code = NAUTILUS_NAVIGATION_RESULT_NO_HANDLER_FOR_TYPE;
      goto out;
    }
  
  /* FIXME: Should do this only when in some special testing mode or something. */
  navinfo->content_identifiers = g_slist_append (navinfo->content_identifiers, 
                                                 nautilus_view_identifier_new ("nautilus_sample_content_view", "Sample"));

  /* Now that all the content_identifiers are in place, we're ready to choose
   * the initial one.
   */
  g_assert (fallback_iid != NULL);
  set_initial_content_iid (navinfo, fallback_iid);

  add_meta_view_iids_from_preferences (navinfo);

 out:
  notify_ready(navinfo, notify_ready_data);
}

/* The following routine uses metadata associated with the current url to add content view components specified in the metadata */
/* the content views are specified in the string as "componentname1:label1\ncomponentname2:label2\n..." */

static void
add_components_from_metadata(NautilusNavigationInfo *navinfo)
{
	NautilusDirectory *directory = nautilus_directory_get(navinfo->navinfo.requested_uri);
	gchar *content_views = nautilus_directory_get_metadata(directory, NAUTILUS_CONTENT_VIEWS_METADATA_KEY, NULL);
	
	if (content_views) {
		char **pieces;
		const char *component_str;
		gchar *colon_pos;
		gint index;
	 	pieces = g_strsplit (content_views, "\n", 0);
	 	for (index = 0; (component_str = pieces[index]) != NULL; index++) {
			/* break the component string into the name and label */
			colon_pos = strchr(component_str, ':');
			if (colon_pos) {
				*colon_pos++ = '\0';
				
				/* add it to the list */
				navinfo->content_identifiers = g_slist_append (navinfo->content_identifiers, 
								nautilus_view_identifier_new (component_str, colon_pos));				
			}			
	 	}
	 	g_strfreev (pieces);	 	 		 	
	 	g_free(content_views); 	
	}
	   
	gtk_object_unref(GTK_OBJECT(directory));
}

static void
add_meta_view_iids_from_preferences (NautilusNavigationInfo *navinfo)
{
	const NautilusStringList *meta_view_iids;
	guint		     i;

	g_assert (navinfo != NULL);
	
	meta_view_iids= nautilus_global_preferences_get_meta_view_iids ();
	
	g_assert (meta_view_iids != NULL);
	
	for (i = 0; i < nautilus_string_list_get_length (meta_view_iids); i++)
	{
		gchar		*iid;
		gboolean	enabled;
		GString		*pref_name;

		iid = nautilus_string_list_nth (meta_view_iids, i);
		
		g_assert (iid != NULL);

		pref_name = g_string_new ("/nautilus/metaviews/");

		g_string_append (pref_name, iid);

		enabled = nautilus_preferences_get_boolean (nautilus_preferences_get_global_preferences (),
							    pref_name->str);


		g_string_free (pref_name, TRUE);
		
		if (enabled)
		{
			navinfo->meta_iids = g_slist_prepend (navinfo->meta_iids, g_strdup (iid));
		}
		
		g_free (iid);
	}
}


/* navinfo stuff */

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
      navinfo->ah = NULL;
      my_notify_when_ready(NULL, res, NULL, navinfo);
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
  g_free(navinfo->initial_content_iid);
  g_free(navinfo->navinfo.requested_uri);
  g_free(navinfo->navinfo.actual_uri);
  g_free(navinfo->navinfo.content_type);
  g_free(navinfo);
}

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

typedef struct {
  enum { MAP_COMPONENT, MAP_TRANSFORM } type;
  union {
    char *component_iid;
    char *transform_info;
  } u;
  char *output_mimetype;
} SchemeMapping;

static GHashTable *scheme_mappings = NULL;

static void
nautilus_navinfo_add_mapping(const char *scheme, int type, const char *data, const char *mimetype)
{
  SchemeMapping *ent;

  ent = g_new0(SchemeMapping, 1);

  ent->type = type;
  ent->u.component_iid = g_strdup(data);
  ent->output_mimetype = g_strdup(mimetype);

  g_hash_table_insert(scheme_mappings, g_strdup(scheme), ent);
}

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
navinfo_read_map_file(const char *fn)
{
  FILE *fh;
  char aline[LINE_MAX];

  fh = fopen(fn, "r");
  if(!fh)
    return;

  while(fgets(aline, sizeof(aline), fh))
    {
      char *sname, *type, *mimetype, *data;
      int typeval;

      if(isspace(aline[0]) || aline[0] == '#')
        continue;

      sname = strtok(aline, " \t");
      type = strtok(NULL, " \t");
      mimetype = strtok(NULL, " \t");
      data = strtok(NULL, "\n");

      if(!sname || !type || !mimetype || !data)
        continue;

      if(!strcasecmp(type, "component"))
        typeval = MAP_COMPONENT;
      else if(!strcasecmp(type, "transform"))
        typeval = MAP_TRANSFORM;
      else
        {
          g_warning("Unrecognized scheme mapping type %s", type);
          continue;
        }

      if(!strcasecmp(mimetype, "NULL"))
        mimetype = NULL;

      nautilus_navinfo_add_mapping(sname, typeval, data, mimetype);
    }
  

  fclose(fh);
}

static void
navinfo_load_mappings(char *dir)
{
  DIR *dirh;
  struct dirent *dent;

  dirh = opendir(dir);
  if(!dirh)
    goto out;

  readdir(dirh);
  readdir(dirh);

  while((dent = readdir(dirh))) {
    char full_name[PATH_MAX];

    g_snprintf(full_name, sizeof(full_name), "%s/%s", dir, dent->d_name);
    navinfo_read_map_file(full_name);
  }
  closedir(dirh);
  
 out:
  g_free(dir);
}

static gboolean
my_notify_when_ready(gpointer data)
{
  NautilusNavigationInfo *navi = data;

  navi->notify_tag = 0;
  navi->notify_ready(navi, navi->data);

  return FALSE;
}

void
nautilus_navinfo_init(void)
{
  GSList *dirs = NULL;

  if(!scheme_mappings)
    scheme_mappings = g_hash_table_new(g_str_hash, g_str_equal);

#if 0
  gnome_file_locate(gnome_file_domain_config, "nautilus/scheme-mappings", TRUE, &dirs);
#else
  {
    char *ctmp;
    ctmp = gnome_config_file("nautilus/scheme-mappings");
    if(ctmp)
      dirs = g_slist_append(NULL, ctmp);
  }
#endif

  g_slist_foreach(dirs, (GFunc)navinfo_load_mappings, NULL);
  g_slist_free(dirs); /* The contents are already freed by navinfo_load_mappings */
}

static void
nautilus_navinfo_map(NautilusNavigationInfo *navinfo)
{
  SchemeMapping *mapping;
  char scheme[128], *ctmp;

  g_assert(scheme_mappings);

  ctmp = strchr(navinfo->navinfo.requested_uri, ':');

  if(!ctmp)
    goto out;
  g_snprintf(scheme, sizeof(scheme), "%.*s", (int)(ctmp - navinfo->navinfo.requested_uri), navinfo->navinfo.requested_uri);

  mapping = g_hash_table_lookup(scheme_mappings, scheme);
  if(!mapping)
    goto out;

  switch(mapping->type)
    {
    case MAP_COMPONENT:
      navinfo->navinfo.actual_uri = g_strdup_printf("component:%s", mapping->u.component_iid);
      break;
    case MAP_TRANSFORM:
      navinfo->navinfo.actual_uri = g_strdup_printf(mapping->u.transform_info,
                                                    ctmp+1, ctmp+1, ctmp+1, /* hello, lame hack! :) */
                                                    ctmp+1, ctmp+1, ctmp+1);
      break;
    }

  navinfo->navinfo.content_type = g_strdup(mapping->output_mimetype);

  return;

 out:
  navinfo->navinfo.actual_uri = g_strdup(navinfo->navinfo.requested_uri);
}

static void
nautilus_navinfo_append_globals(gpointer value, gpointer data)
{
  GSList **target = data;

  *target = g_slist_prepend(*target, g_strdup(value));
}

guint
nautilus_navinfo_new(Nautilus_NavigationRequestInfo *nri,
                     Nautilus_NavigationInfo *old_navinfo,
                     NautilusNavigationInfoFunc notify_when_ready,
                     gpointer notify_data)
{
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

  nautilus_navinfo_map(navinfo);

  if(!navinfo->navinfo.content_type) /* May have already been filed in by nautilus_navinfo_map() */
    {
      GnomeVFSFileInfo *vfs_fileinfo;
      GnomeVFSResult res;

      vfs_fileinfo = gnome_vfs_file_info_new();
      res = gnome_vfs_get_file_info(navinfo->navinfo.actual_uri,
                                    vfs_fileinfo,
                                    GNOME_VFS_FILE_INFO_GETMIMETYPE
                                    |GNOME_VFS_FILE_INFO_FOLLOWLINKS,
                                    meta_keys);
      if(res != GNOME_VFS_OK)
        {
          gnome_vfs_file_info_unref(vfs_fileinfo);
          nautilus_navinfo_free(navinfo); navinfo = NULL;
          goto out;
        }

      navinfo->navinfo.content_type = g_strdup(gnome_vfs_file_info_get_mime_type(vfs_fileinfo));
      gnome_vfs_file_info_unref(vfs_fileinfo);
    }


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
  if(navinfo->navinfo.content_type)
    {
      if(!strcmp(navinfo->navinfo.content_type, "text/html"))
        {
          navinfo->default_content_iid = "ntl_web_browser";
          navinfo->content_identifiers = g_slist_append (
          	navinfo->content_identifiers, 
          	nautilus_view_identifier_new (navinfo->default_content_iid, "Web Page"));
        }
      else if(!strcmp(navinfo->navinfo.content_type, "text/plain"))
        {
          navinfo->default_content_iid = "embeddable:text-plain";
          navinfo->content_identifiers = g_slist_append (
          	navinfo->content_identifiers, 
          	nautilus_view_identifier_new (navinfo->default_content_iid, "Text"));
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
          /* Error - couldn't handle */
          nautilus_navinfo_free(navinfo); navinfo = NULL;
          goto out;
        }
    }

  g_slist_foreach(nautilus_prefs.global_meta_views, nautilus_navinfo_append_globals, &navinfo->meta_iids);

  if(notify_when_ready)
    navinfo->notify_tag = g_idle_add(my_notify_when_ready, navinfo);

 out:
  return navinfo?navinfo->notify_tag:0;
}

void
nautilus_navinfo_free(NautilusNavigationInfo *navinfo)
{
  if(navinfo->notify_tag)
    g_source_remove(navinfo->notify_tag);

  g_slist_foreach(navinfo->content_identifiers, (GFunc)nautilus_view_identifier_free, NULL);
  g_slist_free(navinfo->content_identifiers);
  g_slist_foreach(navinfo->meta_iids, (GFunc)g_free, NULL);
  g_slist_free(navinfo->meta_iids);
  g_free(navinfo->navinfo.requested_uri);
  g_free(navinfo->navinfo.actual_uri);
  g_free(navinfo->navinfo.content_type);
  g_free(navinfo);
}

/*
 * nautilus-shell-search-provider.c - Implementation of a GNOME Shell
 *   search provider
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include <gio/gio.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>

#include "nautilus-file.h"
#include "nautilus-file-utilities.h"
#include "nautilus-scheme.h"
#include "nautilus-search-engine.h"
#include "nautilus-search-provider.h"
#include "nautilus-ui-utilities.h"

#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-shell-search-provider-generated.h"
#include "nautilus-shell-search-provider.h"

typedef struct
{
    NautilusShellSearchProvider *self;

    NautilusSearchEngine *engine;
    NautilusQuery *query;

    GHashTable *hits;
    GDBusMethodInvocation *invocation;

    gint64 start_time;
} PendingSearch;

struct _NautilusShellSearchProvider
{
    GObject parent;

    NautilusShellSearchProvider2 *skeleton;

    PendingSearch *current_search;

    GList *metas_requests;
    GHashTable *metas_cache;
};

G_DEFINE_TYPE (NautilusShellSearchProvider, nautilus_shell_search_provider, G_TYPE_OBJECT)

static gchar *
get_display_name (NautilusShellSearchProvider *self,
                  NautilusFile                *file)
{
    GFile *location;
    NautilusBookmark *bookmark;
    NautilusBookmarkList *bookmarks;

    bookmarks = nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (g_application_get_default ()));

    location = nautilus_file_get_location (file);
    bookmark = nautilus_bookmark_list_item_with_location (bookmarks, location, NULL);
    g_object_unref (location);

    if (bookmark)
    {
        return g_strdup (nautilus_bookmark_get_name (bookmark));
    }
    else
    {
        return nautilus_file_get_display_name (file);
    }
}

static GIcon *
get_gicon (NautilusShellSearchProvider *self,
           NautilusFile                *file)
{
    GFile *location;
    NautilusBookmark *bookmark;
    NautilusBookmarkList *bookmarks;

    bookmarks = nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (g_application_get_default ()));

    location = nautilus_file_get_location (file);
    bookmark = nautilus_bookmark_list_item_with_location (bookmarks, location, NULL);
    g_object_unref (location);

    if (bookmark)
    {
        return nautilus_bookmark_get_icon (bookmark);
    }
    else
    {
        return nautilus_file_get_gicon (file, 0);
    }
}

static void
pending_search_free (PendingSearch *search)
{
    g_hash_table_destroy (search->hits);
    g_clear_object (&search->query);
    g_signal_handlers_disconnect_by_data (G_OBJECT (search->engine), search);
    g_clear_object (&search->engine);
    g_clear_object (&search->invocation);

    g_slice_free (PendingSearch, search);
}

static void
pending_search_finish (PendingSearch         *search,
                       GDBusMethodInvocation *invocation,
                       GVariant              *result)
{
    NautilusShellSearchProvider *self = search->self;

    g_dbus_method_invocation_return_value (invocation, result);

    if (search == self->current_search)
    {
        self->current_search = NULL;
    }

    g_application_release (g_application_get_default ());
    pending_search_free (search);
}

static void
cancel_current_search (NautilusShellSearchProvider *self)
{
    if (self->current_search != NULL)
    {
        NautilusSearchProvider *engine;

        g_debug ("*** Cancel current search");

        engine = NAUTILUS_SEARCH_PROVIDER (self->current_search->engine);
        /* The finish signal may be emitted during the call to nautilus_search_provider_stop
         * which causes shell_search_provider to free the engine. Increase
         * the ref count to prevent use after free issues.
         */
        g_object_ref (engine);
        nautilus_search_provider_stop (engine);
        g_object_unref (engine);
    }
}

static void
cancel_current_search_ignoring_partial_results (NautilusShellSearchProvider *self)
{
    cancel_current_search (self);

    if (self->current_search != NULL)
    {
        pending_search_finish (self->current_search, self->current_search->invocation,
                               g_variant_new ("(as)", NULL));
    }
}

static void
search_hits_added_cb (NautilusSearchEngine *engine,
                      GList                *hits,
                      gpointer              user_data)
{
    PendingSearch *search = user_data;
    GList *l;
    NautilusSearchHit *hit;
    const gchar *hit_uri;

    g_debug ("*** Search engine hits added");

    for (l = hits; l != NULL; l = l->next)
    {
        hit = l->data;
        nautilus_search_hit_compute_scores (hit, search->query);
        hit_uri = nautilus_search_hit_get_uri (hit);
        g_debug ("    %s", hit_uri);

        g_hash_table_replace (search->hits, g_strdup (hit_uri), g_object_ref (hit));
    }
}

static gint
search_hit_compare_relevance (gconstpointer a,
                              gconstpointer b)
{
    NautilusSearchHit *hit_a, *hit_b;
    gdouble relevance_a, relevance_b;

    hit_a = NAUTILUS_SEARCH_HIT ((gpointer) a);
    hit_b = NAUTILUS_SEARCH_HIT ((gpointer) b);

    relevance_a = nautilus_search_hit_get_relevance (hit_a);
    relevance_b = nautilus_search_hit_get_relevance (hit_b);

    if (relevance_a > relevance_b)
    {
        return -1;
    }
    else if (relevance_a == relevance_b)
    {
        return 0;
    }

    return 1;
}

static void
search_finished_cb (NautilusSearchEngine         *engine,
                    NautilusSearchProviderStatus  status,
                    gpointer                      user_data)
{
    PendingSearch *search = user_data;
    GList *hits, *l;
    NautilusSearchHit *hit;
    GVariantBuilder builder;
    gint64 current_time;

    current_time = g_get_monotonic_time ();
    g_debug ("*** Search engine search finished - time elapsed %dms",
             (gint) ((current_time - search->start_time) / 1000));

    hits = g_hash_table_get_values (search->hits);
    hits = g_list_sort (hits, search_hit_compare_relevance);

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

    for (l = hits; l != NULL; l = l->next)
    {
        hit = l->data;
        g_variant_builder_add (&builder, "s", nautilus_search_hit_get_uri (hit));
    }

    g_list_free (hits);
    pending_search_finish (search, search->invocation,
                           g_variant_new ("(as)", &builder));
}

static void
search_error_cb (NautilusSearchEngine *engine,
                 const gchar          *error_message,
                 gpointer              user_data)
{
    NautilusShellSearchProvider *self = user_data;
    PendingSearch *search = self->current_search;

    g_debug ("*** Search engine search error");
    pending_search_finish (search, search->invocation,
                           g_variant_new ("(as)", NULL));
}

typedef struct
{
    gchar *uri;
    gchar *string_for_compare;
} SearchHitCandidate;

static void
search_hit_candidate_free (SearchHitCandidate *candidate)
{
    g_free (candidate->uri);
    g_free (candidate->string_for_compare);

    g_slice_free (SearchHitCandidate, candidate);
}

static SearchHitCandidate *
search_hit_candidate_new (const gchar *uri,
                          const gchar *name)
{
    SearchHitCandidate *candidate = g_slice_new0 (SearchHitCandidate);

    candidate->uri = g_strdup (uri);
    candidate->string_for_compare = g_strdup (name);

    return candidate;
}

static void
search_add_volumes_and_bookmarks (PendingSearch *search)
{
    NautilusSearchHit *hit;
    NautilusBookmark *bookmark;
    const gchar *name;
    gchar *string, *uri;
    gdouble match;
    GList *l, *m, *drives, *volumes, *mounts, *mounts_to_check, *candidates;
    GDrive *drive;
    GVolume *volume;
    GMount *mount;
    GFile *location;
    SearchHitCandidate *candidate;
    NautilusBookmarkList *bookmarks;
    GList *all_bookmarks;
    GVolumeMonitor *volume_monitor;

    bookmarks = nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (g_application_get_default ()));
    all_bookmarks = nautilus_bookmark_list_get_all (bookmarks);
    volume_monitor = g_volume_monitor_get ();
    candidates = NULL;

    /* first add bookmarks */
    for (l = all_bookmarks; l != NULL; l = l->next)
    {
        bookmark = NAUTILUS_BOOKMARK (l->data);
        name = nautilus_bookmark_get_name (bookmark);
        if (name == NULL)
        {
            continue;
        }

        uri = nautilus_bookmark_get_uri (bookmark);
        candidate = search_hit_candidate_new (uri, name);
        candidates = g_list_prepend (candidates, candidate);

        g_free (uri);
    }

    /* home dir */
    uri = nautilus_get_home_directory_uri ();
    candidate = search_hit_candidate_new (uri, _("Home"));
    candidates = g_list_prepend (candidates, candidate);
    g_free (uri);

    /* trash */
    candidate = search_hit_candidate_new (SCHEME_TRASH ":///", _("Trash"));
    candidates = g_list_prepend (candidates, candidate);

    /* now add mounts */
    mounts_to_check = NULL;

    /* first check all connected drives */
    drives = g_volume_monitor_get_connected_drives (volume_monitor);
    for (l = drives; l != NULL; l = l->next)
    {
        drive = l->data;
        volumes = g_drive_get_volumes (drive);

        for (m = volumes; m != NULL; m = m->next)
        {
            volume = m->data;
            mount = g_volume_get_mount (volume);
            if (mount != NULL)
            {
                mounts_to_check = g_list_prepend (mounts_to_check, mount);
            }
        }

        g_list_free_full (volumes, g_object_unref);
    }
    g_list_free_full (drives, g_object_unref);

    /* then volumes that don't have a drive */
    volumes = g_volume_monitor_get_volumes (volume_monitor);
    for (l = volumes; l != NULL; l = l->next)
    {
        volume = l->data;
        drive = g_volume_get_drive (volume);

        if (drive == NULL)
        {
            mount = g_volume_get_mount (volume);
            if (mount != NULL)
            {
                mounts_to_check = g_list_prepend (mounts_to_check, mount);
            }
        }
        g_clear_object (&drive);
    }
    g_list_free_full (volumes, g_object_unref);

    /* then mounts that have no volume */
    mounts = g_volume_monitor_get_mounts (volume_monitor);
    for (l = mounts; l != NULL; l = l->next)
    {
        mount = l->data;

        if (g_mount_is_shadowed (mount))
        {
            continue;
        }

        volume = g_mount_get_volume (mount);
        if (volume == NULL)
        {
            mounts_to_check = g_list_prepend (mounts_to_check, g_object_ref (mount));
        }
        g_clear_object (&volume);
    }
    g_list_free_full (mounts, g_object_unref);

    /* actually add mounts to candidates */
    for (l = mounts_to_check; l != NULL; l = l->next)
    {
        mount = l->data;

        string = g_mount_get_name (mount);
        if (string == NULL)
        {
            continue;
        }

        location = g_mount_get_default_location (mount);
        uri = g_file_get_uri (location);
        candidate = search_hit_candidate_new (uri, string);
        candidates = g_list_prepend (candidates, candidate);

        g_free (uri);
        g_free (string);
        g_object_unref (location);
    }
    g_list_free_full (mounts_to_check, g_object_unref);

    /* now do the actual string matching */
    candidates = g_list_reverse (candidates);

    for (l = candidates; l != NULL; l = l->next)
    {
        candidate = l->data;
        match = nautilus_query_matches_string (search->query,
                                               candidate->string_for_compare);

        if (match > -1)
        {
            hit = nautilus_search_hit_new (candidate->uri);
            nautilus_search_hit_set_fts_rank (hit, match);
            nautilus_search_hit_compute_scores (hit, search->query);
            g_hash_table_replace (search->hits, g_strdup (candidate->uri), hit);
        }
    }
    g_list_free_full (candidates, (GDestroyNotify) search_hit_candidate_free);
    g_object_unref (volume_monitor);
}

static NautilusQuery *
shell_query_new (gchar **terms)
{
    NautilusQuery *query;
    g_autofree gchar *terms_joined = NULL;

    terms_joined = g_strjoinv (" ", terms);

    query = nautilus_query_new ();
    nautilus_query_set_text (query, terms_joined);
    /* Global search is not limited by location. */
    nautilus_query_set_location (query, NULL);
    nautilus_query_set_recursive (query, NAUTILUS_QUERY_RECURSIVE_INDEXED_ONLY);

    return query;
}

static void
execute_search (NautilusShellSearchProvider  *self,
                GDBusMethodInvocation        *invocation,
                gchar                       **terms)
{
    NautilusQuery *query;
    PendingSearch *pending_search;

    cancel_current_search (self);

    /* don't attempt searches for a single character */
    if (g_strv_length (terms) == 1 &&
        g_utf8_strlen (terms[0], -1) == 1)
    {
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
        return;
    }

    query = shell_query_new (terms);
    nautilus_query_set_show_hidden_files (query, FALSE);

    pending_search = g_slice_new0 (PendingSearch);
    pending_search->invocation = g_object_ref (invocation);
    pending_search->hits = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
    pending_search->query = query;
    pending_search->engine = nautilus_search_engine_new ();
    pending_search->start_time = g_get_monotonic_time ();
    pending_search->self = self;

    g_signal_connect (pending_search->engine, "hits-added",
                      G_CALLBACK (search_hits_added_cb), pending_search);
    g_signal_connect (pending_search->engine, "finished",
                      G_CALLBACK (search_finished_cb), pending_search);
    g_signal_connect (pending_search->engine, "error",
                      G_CALLBACK (search_error_cb), pending_search);

    self->current_search = pending_search;
    g_application_hold (g_application_get_default ());

    search_add_volumes_and_bookmarks (pending_search);

    /* start searching */
    g_debug ("*** Search engine search started");
    nautilus_search_engine_enable_recent (pending_search->engine);
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (pending_search->engine),
                                        query);
    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (pending_search->engine));
}

static gboolean
handle_get_initial_result_set (NautilusShellSearchProvider2  *skeleton,
                               GDBusMethodInvocation         *invocation,
                               gchar                        **terms,
                               gpointer                       user_data)
{
    NautilusShellSearchProvider *self = user_data;

    g_debug ("****** GetInitialResultSet");
    execute_search (self, invocation, terms);
    return TRUE;
}

static gboolean
handle_get_subsearch_result_set (NautilusShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation         *invocation,
                                 gchar                        **previous_results,
                                 gchar                        **terms,
                                 gpointer                       user_data)
{
    NautilusShellSearchProvider *self = user_data;

    g_debug ("****** GetSubSearchResultSet");
    execute_search (self, invocation, terms);
    return TRUE;
}

typedef struct
{
    NautilusShellSearchProvider *self;

    gint64 start_time;
    NautilusFileListHandle *handle;
    GDBusMethodInvocation *invocation;

    gchar **uris;
} ResultMetasData;

static void
result_metas_data_free (ResultMetasData *data)
{
    g_clear_pointer (&data->handle, nautilus_file_list_cancel_call_when_ready);
    g_clear_object (&data->self);
    g_clear_object (&data->invocation);
    g_strfreev (data->uris);

    g_slice_free (ResultMetasData, data);
}

static void
result_metas_return_from_cache (ResultMetasData *data)
{
    GVariantBuilder builder;
    GVariant *meta;
    gint64 current_time;
    gint idx;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    if (data->uris)
    {
        for (idx = 0; data->uris[idx] != NULL; idx++)
        {
            meta = g_hash_table_lookup (data->self->metas_cache,
                                        data->uris[idx]);
            g_variant_builder_add_value (&builder, meta);
        }
    }

    current_time = g_get_monotonic_time ();
    g_debug ("*** GetResultMetas completed - time elapsed %dms",
             (gint) ((current_time - data->start_time) / 1000));

    g_dbus_method_invocation_return_value (data->invocation,
                                           g_variant_new ("(aa{sv})", &builder));
}

static void
result_metas_return_empty (ResultMetasData *data)
{
    g_clear_pointer (&data->uris, g_strfreev);
    result_metas_return_from_cache (data);
    result_metas_data_free (data);
}

static void
cancel_result_meta_requests (NautilusShellSearchProvider *self)
{
    g_debug ("*** Cancel Results Meta requests");

    g_list_free_full (self->metas_requests,
                      (GDestroyNotify) result_metas_return_empty);
    self->metas_requests = NULL;
}

static void
result_list_attributes_ready_cb (GList    *file_list,
                                 gpointer  user_data)
{
    ResultMetasData *data = user_data;
    GVariantBuilder meta;
    NautilusFile *file;
    GList *l;
    gchar *uri, *display_name;
    gchar *path, *description;
    gchar *thumbnail_path;
    GIcon *gicon;
    GFile *location;
    GVariant *meta_variant;
    gint icon_scale;

    icon_scale = gdk_monitor_get_scale_factor (g_list_model_get_item (gdk_display_get_monitors (gdk_display_get_default ()), 0));

    for (l = file_list; l != NULL; l = l->next)
    {
        g_autoptr (GFile) file_location = NULL;
        g_autoptr (GVariant) icon_variant = NULL;

        file = l->data;
        g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

        uri = nautilus_file_get_uri (file);
        display_name = get_display_name (data->self, file);
        file_location = nautilus_file_get_location (file);
        path = g_file_get_path (file_location);
        description = path ? g_path_get_dirname (path) : NULL;

        g_variant_builder_add (&meta, "{sv}",
                               "id", g_variant_new_string (uri));
        g_variant_builder_add (&meta, "{sv}",
                               "name", g_variant_new_string (display_name));
        /* Some backends like trash:/// don't have a path, so we show the uri itself. */
        g_variant_builder_add (&meta, "{sv}",
                               "description", g_variant_new_string (description ? description : uri));

        gicon = NULL;
        thumbnail_path = nautilus_file_get_thumbnail_path (file);

        if (thumbnail_path != NULL)
        {
            location = g_file_new_for_path (thumbnail_path);
            gicon = g_file_icon_new (location);

            g_free (thumbnail_path);
            g_object_unref (location);
        }
        else
        {
            gicon = get_gicon (data->self, file);
        }

        if (gicon == NULL)
        {
            gicon = G_ICON (nautilus_file_get_icon_texture (file, 128,
                                                            icon_scale,
                                                            NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS));
        }

        icon_variant = g_icon_serialize (gicon);
        g_variant_builder_add (&meta, "{sv}",
                               "icon", icon_variant);
        g_object_unref (gicon);

        meta_variant = g_variant_builder_end (&meta);
        g_hash_table_insert (data->self->metas_cache,
                             g_strdup (uri), g_variant_ref_sink (meta_variant));

        g_free (display_name);
        g_free (path);
        g_free (description);
        g_free (uri);
    }

    data->handle = NULL;
    data->self->metas_requests = g_list_remove (data->self->metas_requests, data);

    result_metas_return_from_cache (data);
    result_metas_data_free (data);
}

static gboolean
handle_get_result_metas (NautilusShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation         *invocation,
                         gchar                        **results,
                         gpointer                       user_data)
{
    NautilusShellSearchProvider *self = user_data;
    GList *missing_files = NULL;
    const gchar *uri;
    ResultMetasData *data;
    gint idx;

    g_debug ("****** GetResultMetas");

    for (idx = 0; results[idx] != NULL; idx++)
    {
        uri = results[idx];

        if (!g_hash_table_lookup (self->metas_cache, uri))
        {
            missing_files = g_list_prepend (missing_files, nautilus_file_get_by_uri (uri));
        }
    }

    data = g_slice_new0 (ResultMetasData);
    data->self = g_object_ref (self);
    data->invocation = g_object_ref (invocation);
    data->start_time = g_get_monotonic_time ();
    data->uris = g_strdupv (results);

    if (missing_files == NULL)
    {
        result_metas_return_from_cache (data);
        result_metas_data_free (data);
        return TRUE;
    }

    nautilus_file_list_call_when_ready (missing_files,
                                        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                        &data->handle,
                                        result_list_attributes_ready_cb,
                                        data);
    self->metas_requests = g_list_prepend (self->metas_requests, data);
    nautilus_file_list_free (missing_files);
    return TRUE;
}

typedef struct
{
    GFile *file;
    NautilusShellSearchProvider2 *skeleton;
    GDBusMethodInvocation *invocation;
} ShowURIData;

static void
show_uri_callback (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
    ShowURIData *data = user_data;

    if (!gtk_show_uri_full_finish (NULL, result, NULL))
    {
        g_application_open (g_application_get_default (), &data->file, 1, "");
    }

    nautilus_shell_search_provider2_complete_activate_result (data->skeleton, data->invocation);

    g_object_unref (data->file);
    g_free (data);
}

static gboolean
handle_activate_result (NautilusShellSearchProvider2  *skeleton,
                        GDBusMethodInvocation         *invocation,
                        gchar                         *result,
                        gchar                        **terms,
                        guint32                        timestamp,
                        gpointer                       user_data)
{
    ShowURIData *data;

    data = g_new (ShowURIData, 1);
    data->file = g_file_new_for_uri (result);
    data->skeleton = skeleton;
    data->invocation = invocation;

    gtk_show_uri_full (NULL, result, timestamp, NULL, show_uri_callback, data);

    return TRUE;
}

static gboolean
handle_launch_search (NautilusShellSearchProvider2  *skeleton,
                      GDBusMethodInvocation         *invocation,
                      gchar                        **terms,
                      guint32                        timestamp,
                      gpointer                       user_data)
{
    GApplication *app = g_application_get_default ();
    g_autoptr (NautilusQuery) query = shell_query_new (terms);

    nautilus_application_search (NAUTILUS_APPLICATION (app), query);

    nautilus_shell_search_provider2_complete_launch_search (skeleton, invocation);
    return TRUE;
}

static void
search_provider_dispose (GObject *obj)
{
    NautilusShellSearchProvider *self = NAUTILUS_SHELL_SEARCH_PROVIDER (obj);

    g_clear_object (&self->skeleton);
    g_hash_table_destroy (self->metas_cache);
    cancel_current_search_ignoring_partial_results (self);
    cancel_result_meta_requests (self);

    G_OBJECT_CLASS (nautilus_shell_search_provider_parent_class)->dispose (obj);
}

static void
nautilus_shell_search_provider_init (NautilusShellSearchProvider *self)
{
    self->metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, (GDestroyNotify) g_variant_unref);

    self->skeleton = nautilus_shell_search_provider2_skeleton_new ();

    g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                      G_CALLBACK (handle_get_initial_result_set), self);
    g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                      G_CALLBACK (handle_get_subsearch_result_set), self);
    g_signal_connect (self->skeleton, "handle-get-result-metas",
                      G_CALLBACK (handle_get_result_metas), self);
    g_signal_connect (self->skeleton, "handle-activate-result",
                      G_CALLBACK (handle_activate_result), self);
    g_signal_connect (self->skeleton, "handle-launch-search",
                      G_CALLBACK (handle_launch_search), self);
}

static void
nautilus_shell_search_provider_class_init (NautilusShellSearchProviderClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->dispose = search_provider_dispose;
}

NautilusShellSearchProvider *
nautilus_shell_search_provider_new (void)
{
    return g_object_new (nautilus_shell_search_provider_get_type (),
                         NULL);
}

gboolean
nautilus_shell_search_provider_register (NautilusShellSearchProvider  *self,
                                         GDBusConnection              *connection,
                                         GError                      **error)
{
    return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                             connection,
                                             "/org/gnome/Nautilus" PROFILE "/SearchProvider", error);
}

void
nautilus_shell_search_provider_unregister (NautilusShellSearchProvider *self)
{
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));
}

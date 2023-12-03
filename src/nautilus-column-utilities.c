/* nautilus-column-utilities.h - Utilities related to column specifications
 *
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the column COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Dave Camp <dave@ximian.com>
 */

#include <config.h>
#include "nautilus-column-utilities.h"

#include <string.h>
#include <glib/gi18n.h>
#include <nautilus-extension.h>
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-module.h"

static const char *default_column_order[] =
{
    "name",
    "size",
    "type",
    "owner",
    "group",
    "permissions",
    "detailed_type",
    "where",
    "date_modified",
    "date_accessed",
    "date_created",
    "recency",
    "starred",
    NULL
};

static const char *default_columns_for_recent[] =
{
    "name", "size", "recency", NULL
};

static const char *default_columns_for_trash[] =
{
    "name", "size", "trashed_on", NULL
};

static GList *
get_builtin_columns (void)
{
    GList *columns;

    columns = g_list_append (NULL,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "name",
                                           "attribute", "name",
                                           "label", _("Name"),
                                           "description", _("The name and icon of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "size",
                                           "attribute", "size",
                                           "label", _("Size"),
                                           "description", _("The size of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "type",
                                           "attribute", "type",
                                           "label", _("Type"),
                                           "description", _("The type of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "date_modified",
                                           "attribute", "date_modified",
                                           "label", _("Modified"),
                                           "description", _("The date the file was modified."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "detailed_type",
                                           "attribute", "detailed_type",
                                           "label", _("Detailed Type"),
                                           "description", _("The detailed type of the file."),
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "date_accessed",
                                           "attribute", "date_accessed",
                                           "label", _("Accessed"),
                                           "description", _("The date the file was accessed."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));
    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "date_created",
                                           "attribute", "date_created",
                                           "label", _("Created"),
                                           "description", _("The date the file was created."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "owner",
                                           "attribute", "owner",
                                           "label", _("Owner"),
                                           "description", _("The owner of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "group",
                                           "attribute", "group",
                                           "label", _("Group"),
                                           "description", _("The group of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "permissions",
                                           "attribute", "permissions",
                                           "label", _("Permissions"),
                                           "description", _("The permissions of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "where",
                                           "attribute", "where",
                                           "label", _("Location"),
                                           "description", _("The location of the file."),
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "recency",
                                           "attribute", "recency",
                                           "label", _("Recency"),
                                           "description", _("The date the file was accessed by the user."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 1.0,
                                           NULL));

    columns = g_list_append (columns,
                             g_object_new (NAUTILUS_TYPE_COLUMN,
                                           "name", "starred",
                                           "attribute", "starred",
                                           "label", _("Star"),
                                           "description", _("Shows if file is starred."),
                                           "default-sort-order", GTK_SORT_DESCENDING,
                                           "xalign", 0.5,
                                           NULL));

    return columns;
}

static GList *
get_extension_columns (void)
{
    GList *columns;
    GList *providers;
    GList *l;

    providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_COLUMN_PROVIDER);

    columns = NULL;

    for (l = providers; l != NULL; l = l->next)
    {
        NautilusColumnProvider *provider;
        GList *provider_columns;

        provider = NAUTILUS_COLUMN_PROVIDER (l->data);
        provider_columns = nautilus_column_provider_get_columns (provider);
        columns = g_list_concat (columns, provider_columns);
    }

    nautilus_module_extension_list_free (providers);

    return columns;
}

static GList *
get_trash_columns (void)
{
    static GList *columns = NULL;

    if (columns == NULL)
    {
        columns = g_list_append (columns,
                                 g_object_new (NAUTILUS_TYPE_COLUMN,
                                               "name", "trashed_on",
                                               "attribute", "trashed_on",
                                               "label", _("Trashed On"),
                                               "description", _("Date when file was moved to the Trash"),
                                               "xalign", 1.0,
                                               NULL));
        columns = g_list_append (columns,
                                 g_object_new (NAUTILUS_TYPE_COLUMN,
                                               "name", "trash_orig_path",
                                               "attribute", "trash_orig_path",
                                               "label", _("Original Location"),
                                               "description", _("Original location of file before moved to the Trash"),
                                               NULL));
    }

    return nautilus_column_list_copy (columns);
}

static GList *
get_search_columns (void)
{
    static GList *columns = NULL;

    if (columns == NULL)
    {
        columns = g_list_append (columns,
                                 g_object_new (NAUTILUS_TYPE_COLUMN,
                                               "name", "search_relevance",
                                               "attribute", "search_relevance",
                                               "label", _("Relevance"),
                                               "description", _("Relevance rank for search"),
                                               NULL));
    }

    return nautilus_column_list_copy (columns);
}

GList *
nautilus_get_common_columns (void)
{
    static GList *columns = NULL;

    if (!columns)
    {
        columns = g_list_concat (get_builtin_columns (),
                                 get_extension_columns ());
    }

    return nautilus_column_list_copy (columns);
}

GList *
nautilus_get_all_columns (void)
{
    GList *columns = NULL;

    columns = g_list_concat (nautilus_get_common_columns (),
                             get_trash_columns ());
    columns = g_list_concat (columns,
                             get_search_columns ());

    return columns;
}

GList *
nautilus_get_columns_for_file (NautilusFile *file)
{
    GList *columns;

    columns = nautilus_get_common_columns ();

    if (file != NULL && nautilus_file_is_in_trash (file))
    {
        columns = g_list_concat (columns,
                                 get_trash_columns ());
    }

    return columns;
}

GList *
nautilus_column_list_copy (GList *columns)
{
    GList *ret;
    GList *l;

    ret = g_list_copy (columns);

    for (l = ret; l != NULL; l = l->next)
    {
        g_object_ref (l->data);
    }

    return ret;
}

void
nautilus_column_list_free (GList *columns)
{
    GList *l;

    for (l = columns; l != NULL; l = l->next)
    {
        g_object_unref (l->data);
    }

    g_list_free (columns);
}

static int
strv_index (char       **strv,
            const char  *str)
{
    int i;

    for (i = 0; strv[i] != NULL; ++i)
    {
        if (strcmp (strv[i], str) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int
column_compare (NautilusColumn  *a,
                NautilusColumn  *b,
                char           **column_order)
{
    int index_a;
    int index_b;
    char *name_a;
    char *name_b;
    int ret;

    g_object_get (G_OBJECT (a), "name", &name_a, NULL);
    index_a = strv_index (column_order, name_a);

    g_object_get (G_OBJECT (b), "name", &name_b, NULL);
    index_b = strv_index (column_order, name_b);

    if (index_a == index_b)
    {
        int pos_a;
        int pos_b;

        pos_a = strv_index ((char **) default_column_order, name_a);
        pos_b = strv_index ((char **) default_column_order, name_b);

        if (pos_a == pos_b)
        {
            char *label_a;
            char *label_b;

            g_object_get (G_OBJECT (a), "label", &label_a, NULL);
            g_object_get (G_OBJECT (b), "label", &label_b, NULL);
            ret = strcmp (label_a, label_b);
            g_free (label_a);
            g_free (label_b);
        }
        else if (pos_a == -1)
        {
            ret = 1;
        }
        else if (pos_b == -1)
        {
            ret = -1;
        }
        else
        {
            ret = index_a - index_b;
        }
    }
    else if (index_a == -1)
    {
        ret = 1;
    }
    else if (index_b == -1)
    {
        ret = -1;
    }
    else
    {
        ret = index_a - index_b;
    }

    g_free (name_a);
    g_free (name_b);

    return ret;
}

GList *
nautilus_sort_columns (GList  *columns,
                       char  **column_order)
{
    if (column_order == NULL)
    {
        return columns;
    }

    return g_list_sort_with_data (columns,
                                  (GCompareDataFunc) column_compare,
                                  column_order);
}

void
nautilus_column_save_metadata (NautilusFile *file,
                               GStrv         column_order,
                               GStrv         visible_columns)
{
    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                     visible_columns);
    nautilus_file_set_metadata_list (file,
                                     NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
                                     column_order);
}

GStrv
nautilus_column_get_default_visible_columns (NautilusFile *file)
{
    if (nautilus_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_columns_for_trash);
    }

    if (nautilus_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_columns_for_recent);
    }

    return g_settings_get_strv (nautilus_list_view_preferences,
                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
}

GStrv
nautilus_column_get_visible_columns (NautilusFile *file)
{
    g_autofree gchar **visible_columns = NULL;

    visible_columns = nautilus_file_get_metadata_list (file,
                                                       NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);
    if (visible_columns == NULL || visible_columns[0] == NULL)
    {
        return nautilus_column_get_default_visible_columns (file);
    }

    return g_steal_pointer (&visible_columns);
}

GStrv
nautilus_column_get_default_column_order (NautilusFile *file)
{
    if (nautilus_file_is_in_trash (file))
    {
        return g_strdupv ((gchar **) default_columns_for_trash);
    }

    if (nautilus_file_is_in_recent (file))
    {
        return g_strdupv ((gchar **) default_columns_for_recent);
    }

    return g_settings_get_strv (nautilus_list_view_preferences,
                                NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
}

GStrv
nautilus_column_get_column_order (NautilusFile *file)
{
    g_autofree gchar **column_order = NULL;

    column_order = nautilus_file_get_metadata_list (file,
                                                    NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);

    if (column_order != NULL && column_order[0] != NULL)
    {
        return g_steal_pointer (&column_order);
    }

    return nautilus_column_get_default_column_order (file);
}

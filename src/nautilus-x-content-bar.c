/*
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Paolo Borelli <pborelli@katamail.com>
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nautilus-x-content-bar.h"
#include "nautilus-icon-info.h"
#include "nautilus-file-utilities.h"
#include "nautilus-program-choosing.h"
#include "nautilus-gtk4-helpers.h"

struct _NautilusXContentBar
{
    GtkBin parent_instance;
    GtkWidget *label;

    char **x_content_types;
    GMount *mount;
};

enum
{
    PROP_0,
    PROP_MOUNT,
    PROP_X_CONTENT_TYPES,
};

enum
{
    CONTENT_BAR_RESPONSE_APP = 1
};

G_DEFINE_TYPE (NautilusXContentBar, nautilus_x_content_bar, GTK_TYPE_INFO_BAR)

static void
content_bar_response_cb (GtkInfoBar *infobar,
                         gint        response_id,
                         gpointer    user_data)
{
    GAppInfo *default_app;
    NautilusXContentBar *bar = user_data;

    if (response_id < 0)
    {
        return;
    }

    if (bar->x_content_types == NULL ||
        bar->mount == NULL)
    {
        return;
    }

    /* FIXME */
    default_app = g_app_info_get_default_for_type (bar->x_content_types[response_id], FALSE);
    if (default_app != NULL)
    {
        nautilus_launch_application_for_mount (default_app, bar->mount,
                                               GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bar))));
        g_object_unref (default_app);
    }
}

static void
nautilus_x_content_bar_set_x_content_types (NautilusXContentBar *bar,
                                            const char * const  *x_content_types)
{
    char *message = NULL;
    guint num_types;
    guint n;
    GPtrArray *types;
    GPtrArray *apps;
    GAppInfo *default_app;

    g_strfreev (bar->x_content_types);

    if (!should_handle_content_types (x_content_types))
    {
        g_warning ("Content types in content types bar cannot be handled. Check before creating the content bar if they can be handled.");
        return;
    }

    types = g_ptr_array_new ();
    apps = g_ptr_array_new ();
    g_ptr_array_set_free_func (apps, g_object_unref);
    for (n = 0; x_content_types[n] != NULL; n++)
    {
        if (!should_handle_content_type (x_content_types[n]))
        {
            continue;
        }

        default_app = g_app_info_get_default_for_type (x_content_types[n], FALSE);
        g_ptr_array_add (types, g_strdup (x_content_types[n]));
        g_ptr_array_add (apps, default_app);
    }

    num_types = types->len;
    g_ptr_array_add (types, NULL);

    bar->x_content_types = (char **) g_ptr_array_free (types, FALSE);

    switch (num_types)
    {
        case 1:
        {
            message = get_message_for_content_type (bar->x_content_types[0]);
        }
        break;

        case 2:
        {
            message = get_message_for_two_content_types ((const char * const *) bar->x_content_types);
        }
        break;

        default:
        {
            message = g_strdup (_("Open with:"));
        }
        break;
    }

    gtk_label_set_text (GTK_LABEL (bar->label), message);
    g_free (message);

    gtk_widget_show (bar->label);

    for (n = 0; bar->x_content_types[n] != NULL; n++)
    {
        const char *name;
        GIcon *icon;
        GtkWidget *image;
        GtkWidget *info_bar;
        GtkWidget *button;
        GAppInfo *app;
        gboolean has_app;
        guint i;
        GtkWidget *box;

        default_app = g_ptr_array_index (apps, n);
        has_app = FALSE;

        for (i = 0; i < n; i++)
        {
            app = g_ptr_array_index (apps, i);
            if (g_app_info_equal (app, default_app))
            {
                has_app = TRUE;
                break;
            }
        }

        if (has_app)
        {
            continue;
        }

        icon = g_app_info_get_icon (default_app);
        if (icon != NULL)
        {
            image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
        }
        else
        {
            image = NULL;
        }

        name = g_app_info_get_name (default_app);
        info_bar = gtk_bin_get_child (GTK_BIN (bar));
        button = gtk_info_bar_add_button (GTK_INFO_BAR (info_bar), name, n);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

        if (image != NULL)
        {
            gtk_box_append (GTK_BOX (box), image);
        }
        gtk_box_append (GTK_BOX (box), gtk_label_new (name));

        gtk_button_set_child (GTK_BUTTON (button), box);

        gtk_widget_show (button);
    }

    g_ptr_array_free (apps, TRUE);
}

static void
nautilus_x_content_bar_set_mount (NautilusXContentBar *bar,
                                  GMount              *mount)
{
    if (bar->mount != NULL)
    {
        g_object_unref (bar->mount);
    }
    bar->mount = mount != NULL ? g_object_ref (mount) : NULL;
}


static void
nautilus_x_content_bar_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    NautilusXContentBar *bar = NAUTILUS_X_CONTENT_BAR (object);

    switch (prop_id)
    {
        case PROP_MOUNT:
        {
            nautilus_x_content_bar_set_mount (bar, G_MOUNT (g_value_get_object (value)));
        }
        break;

        case PROP_X_CONTENT_TYPES:
        {
            nautilus_x_content_bar_set_x_content_types (bar, g_value_get_boxed (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
nautilus_x_content_bar_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusXContentBar *bar = NAUTILUS_X_CONTENT_BAR (object);

    switch (prop_id)
    {
        case PROP_MOUNT:
        {
            g_value_set_object (value, bar->mount);
        }
        break;

        case PROP_X_CONTENT_TYPES:
        {
            g_value_set_boxed (value, &bar->x_content_types);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
nautilus_x_content_bar_finalize (GObject *object)
{
    NautilusXContentBar *bar = NAUTILUS_X_CONTENT_BAR (object);

    g_strfreev (bar->x_content_types);
    if (bar->mount != NULL)
    {
        g_object_unref (bar->mount);
    }

    G_OBJECT_CLASS (nautilus_x_content_bar_parent_class)->finalize (object);
}

static void
nautilus_x_content_bar_class_init (NautilusXContentBarClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = nautilus_x_content_bar_get_property;
    object_class->set_property = nautilus_x_content_bar_set_property;
    object_class->finalize = nautilus_x_content_bar_finalize;

    g_object_class_install_property (object_class,
                                     PROP_MOUNT,
                                     g_param_spec_object (
                                         "mount",
                                         "The GMount to run programs for",
                                         "The GMount to run programs for",
                                         G_TYPE_MOUNT,
                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property (object_class,
                                     PROP_X_CONTENT_TYPES,
                                     g_param_spec_boxed ("x-content-types",
                                                         "The x-content types for the cluebar",
                                                         "The x-content types for the cluebar",
                                                         G_TYPE_STRV,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
nautilus_x_content_bar_init (NautilusXContentBar *bar)
{
    GtkWidget *info_bar;
    PangoAttrList *attrs;

    info_bar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_QUESTION);
    gtk_widget_show (info_bar);
    adw_bin_set_child (ADW_BIN (bar), info_bar);

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    bar->label = gtk_label_new (NULL);
    gtk_label_set_attributes (GTK_LABEL (bar->label), attrs);
    pango_attr_list_unref (attrs);

    gtk_label_set_ellipsize (GTK_LABEL (bar->label), PANGO_ELLIPSIZE_END);
    gtk_info_bar_add_child (GTK_INFO_BAR (info_bar), bar->label);

    g_signal_connect (info_bar, "response",
                      G_CALLBACK (content_bar_response_cb),
                      bar);
}

GtkWidget *
nautilus_x_content_bar_new (GMount             *mount,
                            const char * const *x_content_types)
{
    return g_object_new (NAUTILUS_TYPE_X_CONTENT_BAR,
                         "mount", mount,
                         "x-content-types", x_content_types,
                         NULL);
}

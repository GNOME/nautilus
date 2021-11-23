/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nautilus-special-location-bar.h"
#include "nautilus-enum-types.h"
#include "nautilus-gtk4-helpers.h"

struct _NautilusSpecialLocationBar
{
    GtkInfoBar parent_instance;

    GtkWidget *label;
    GtkWidget *learn_more_label;
    NautilusSpecialLocation special_location;
};

enum
{
    PROP_0,
    PROP_SPECIAL_LOCATION,
};

G_DEFINE_TYPE (NautilusSpecialLocationBar, nautilus_special_location_bar, GTK_TYPE_INFO_BAR)

static void
set_special_location (NautilusSpecialLocationBar *bar,
                      NautilusSpecialLocation     location)
{
    char *message;
    char *learn_more_markup = NULL;

    switch (location)
    {
        case NAUTILUS_SPECIAL_LOCATION_TEMPLATES:
        {
            message = g_strdup (_("Put files in this folder to use them as templates for new documents."));
            learn_more_markup = g_strdup (_("<a href=\"help:gnome-help/files-templates\" title=\"GNOME help for templates\">Learn more…</a>"));
        }
        break;

        case NAUTILUS_SPECIAL_LOCATION_SCRIPTS:
        {
            message = g_strdup (_("Executable files in this folder will appear in the Scripts menu."));
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    gtk_label_set_text (GTK_LABEL (bar->label), message);
    g_free (message);

    gtk_widget_show (bar->label);

    if (learn_more_markup)
    {
        gtk_label_set_markup (GTK_LABEL (bar->learn_more_label),
                              learn_more_markup);
        gtk_widget_show (bar->learn_more_label);
        g_free (learn_more_markup);
    }
    else
    {
        gtk_widget_hide (bar->learn_more_label);
    }
}

static void
nautilus_special_location_bar_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
    NautilusSpecialLocationBar *bar;

    bar = NAUTILUS_SPECIAL_LOCATION_BAR (object);

    switch (prop_id)
    {
        case PROP_SPECIAL_LOCATION:
        {
            set_special_location (bar, g_value_get_enum (value));
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
nautilus_special_location_bar_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
    NautilusSpecialLocationBar *bar;

    bar = NAUTILUS_SPECIAL_LOCATION_BAR (object);

    switch (prop_id)
    {
        case PROP_SPECIAL_LOCATION:
        {
            g_value_set_enum (value, bar->special_location);
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
nautilus_special_location_bar_class_init (NautilusSpecialLocationBarClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->get_property = nautilus_special_location_bar_get_property;
    object_class->set_property = nautilus_special_location_bar_set_property;

    g_object_class_install_property (object_class,
                                     PROP_SPECIAL_LOCATION,
                                     g_param_spec_enum ("special-location",
                                                        "special-location",
                                                        "special-location",
                                                        NAUTILUS_TYPE_SPECIAL_LOCATION,
                                                        NAUTILUS_SPECIAL_LOCATION_TEMPLATES,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
nautilus_special_location_bar_init (NautilusSpecialLocationBar *bar)
{
    GtkWidget *action_area;
    PangoAttrList *attrs;

    action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));

    gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area), GTK_ORIENTATION_HORIZONTAL);

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    bar->label = gtk_label_new (NULL);
    gtk_label_set_attributes (GTK_LABEL (bar->label), attrs);
    pango_attr_list_unref (attrs);

    gtk_label_set_ellipsize (GTK_LABEL (bar->label), PANGO_ELLIPSIZE_END);
    gtk_info_bar_add_child (GTK_INFO_BAR (bar), bar->label);

    bar->learn_more_label = gtk_label_new (NULL);
    gtk_widget_set_hexpand (bar->learn_more_label, TRUE);
    gtk_widget_set_halign (bar->learn_more_label, GTK_ALIGN_END);
    gtk_info_bar_add_child (GTK_INFO_BAR (bar), bar->learn_more_label);
}

GtkWidget *
nautilus_special_location_bar_new (NautilusSpecialLocation location)
{
    return g_object_new (NAUTILUS_TYPE_SPECIAL_LOCATION_BAR,
                         "message-type", GTK_MESSAGE_QUESTION,
                         "special-location", location,
                         NULL);
}

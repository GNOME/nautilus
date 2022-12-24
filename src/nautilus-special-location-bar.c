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

#include "nautilus-dbus-launcher.h"
#include "nautilus-global-preferences.h"
#include "nautilus-special-location-bar.h"
#include "nautilus-enum-types.h"

struct _NautilusSpecialLocationBar
{
    AdwBin parent_instance;

    GtkWidget *label;
    GtkWidget *learn_more_label;
    GtkWidget *button;
    int button_response;
    NautilusSpecialLocation special_location;
};

enum
{
    PROP_0,
    PROP_SPECIAL_LOCATION,
};

enum
{
    SPECIAL_LOCATION_SHARING_RESPONSE = 1,
    SPECIAL_LOCATION_TRASH_RESPONSE = 2,
};

G_DEFINE_TYPE (NautilusSpecialLocationBar, nautilus_special_location_bar, ADW_TYPE_BIN)

static void
on_info_bar_response (GtkInfoBar *infobar,
                      gint        response_id,
                      gpointer    user_data)
{
    NautilusSpecialLocationBar *bar = user_data;
    GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (bar));

    switch (bar->button_response)
    {
        case SPECIAL_LOCATION_SHARING_RESPONSE:
        {
            GVariant *parameters;

            parameters = g_variant_new_parsed ("('launch-panel', [<('sharing', @av [])>], "
                                               "@a{sv} {})");
            nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                         NAUTILUS_DBUS_LAUNCHER_SETTINGS,
                                         "Activate",
                                         parameters, GTK_WINDOW (window));
        }
        break;

        case SPECIAL_LOCATION_TRASH_RESPONSE:
        {
            GVariant *parameters;

            parameters = g_variant_new_parsed ("('launch-panel', [<('usage', @av [])>], "
                                               "@a{sv} {})");
            nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                         NAUTILUS_DBUS_LAUNCHER_SETTINGS,
                                         "Activate",
                                         parameters, GTK_WINDOW (window));
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static gchar *
parse_old_files_age_preferences_value (void)
{
    guint old_files_age = g_settings_get_uint (gnome_privacy_preferences, "old-files-age");

    switch (old_files_age)
    {
        case 0:
        {
            return g_strdup (_("Items in Trash older than 1 hour are automatically deleted"));
        }

        default:
        {
            return g_strdup_printf (ngettext ("Items in Trash older than %d day are automatically deleted",
                                              "Items in Trash older than %d days are automatically deleted",
                                              old_files_age),
                                    old_files_age);
        }
    }
}

static void
old_files_age_preferences_changed (GSettings *settings,
                                   gchar     *key,
                                   gpointer   user_data)
{
    NautilusSpecialLocationBar *bar;
    g_autofree gchar *message = NULL;

    g_assert (NAUTILUS_IS_SPECIAL_LOCATION_BAR (user_data));

    bar = NAUTILUS_SPECIAL_LOCATION_BAR (user_data);

    message = parse_old_files_age_preferences_value ();

    gtk_label_set_text (GTK_LABEL (bar->label), message);
}

static void
set_special_location (NautilusSpecialLocationBar *bar,
                      NautilusSpecialLocation     location)
{
    char *message;
    char *learn_more_markup = NULL;
    char *button_label = NULL;

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

        case NAUTILUS_SPECIAL_LOCATION_SHARING:
        {
            message = g_strdup (_("Turn on File Sharing to share the contents of this folder over the network."));
            button_label = _("Sharing Settings");
            bar->button_response = SPECIAL_LOCATION_SHARING_RESPONSE;
        }
        break;

        case NAUTILUS_SPECIAL_LOCATION_TRASH:
        {
            message = parse_old_files_age_preferences_value ();
            button_label = _("_Settings");
            bar->button_response = SPECIAL_LOCATION_TRASH_RESPONSE;

            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::old-files-age",
                                     G_CALLBACK (old_files_age_preferences_changed),
                                     bar, 0);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    gtk_label_set_text (GTK_LABEL (bar->label), message);
    g_free (message);

    if (learn_more_markup)
    {
        gtk_label_set_markup (GTK_LABEL (bar->learn_more_label),
                              learn_more_markup);
        gtk_widget_set_visible (bar->learn_more_label, TRUE);
        g_free (learn_more_markup);
    }
    else
    {
        gtk_widget_set_visible (bar->learn_more_label, FALSE);
    }

    if (button_label)
    {
        gtk_button_set_label (GTK_BUTTON (bar->button), button_label);
        gtk_widget_set_visible (bar->button, TRUE);
    }
    else
    {
        gtk_widget_set_visible (bar->button, FALSE);
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
    GtkWidget *info_bar;
    PangoAttrList *attrs;
    GtkWidget *button;

    info_bar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_QUESTION);
    adw_bin_set_child (ADW_BIN (bar), info_bar);

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    bar->label = gtk_label_new (NULL);
    gtk_label_set_attributes (GTK_LABEL (bar->label), attrs);
    pango_attr_list_unref (attrs);

    gtk_label_set_ellipsize (GTK_LABEL (bar->label), PANGO_ELLIPSIZE_END);
    gtk_info_bar_add_child (GTK_INFO_BAR (info_bar), bar->label);

    button = gtk_info_bar_add_button (GTK_INFO_BAR (info_bar), "", GTK_RESPONSE_OK);
    bar->button = button;

    bar->learn_more_label = gtk_label_new (NULL);
    gtk_widget_set_hexpand (bar->learn_more_label, TRUE);
    gtk_widget_set_halign (bar->learn_more_label, GTK_ALIGN_END);
    gtk_info_bar_add_child (GTK_INFO_BAR (info_bar), bar->learn_more_label);

    g_signal_connect (info_bar, "response", G_CALLBACK (on_info_bar_response), bar);
}

GtkWidget *
nautilus_special_location_bar_new (NautilusSpecialLocation location)
{
    return g_object_new (NAUTILUS_TYPE_SPECIAL_LOCATION_BAR,
                         "special-location", location,
                         NULL);
}

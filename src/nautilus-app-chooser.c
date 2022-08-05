/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-app-chooser.h"

#include <libadwaita-1/adwaita.h>
#include <glib/gi18n.h>

#include "nautilus-file.h"
#include "nautilus-signaller.h"

struct _NautilusAppChooser
{
    GtkDialog parent_instance;

    gchar *content_type;
    gboolean single_content_type;

    GtkWidget *app_chooser_widget_box;
    GtkWidget *label_content_type_description;
    GtkWidget *set_as_default_switch;
    GtkWidget *set_default_box;

    GtkWidget *app_chooser_widget;
};

G_DEFINE_TYPE (NautilusAppChooser, nautilus_app_chooser, GTK_TYPE_DIALOG)

enum
{
    PROP_0,
    PROP_CONTENT_TYPE,
    PROP_SINGLE_CONTENT_TYPE,
    LAST_PROP
};

static void
open_button_clicked_cb (GtkButton          *button,
                        NautilusAppChooser *self)
{
    gboolean state;
    g_autoptr (GAppInfo) info = NULL;
    g_autoptr (GError) error = NULL;

    state = gtk_switch_get_active (GTK_SWITCH (self->set_as_default_switch));

    if (!self->single_content_type)
    {
        /* Don't attempt to set an association with multiple content types */
        return;
    }

    if (state)
    {
        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));
        g_app_info_set_as_default_for_type (info, self->content_type,
                                            &error);
    }
    else
    {
        g_app_info_reset_type_associations (self->content_type);
        gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
    }

    if (error != NULL)
    {
        g_autofree gchar *message = NULL;
        GtkWidget *message_dialog;

        message = g_strdup_printf (_("Error while setting “%s” as default application: %s"),
                                   g_app_info_get_display_name (info), error->message);
        message_dialog = adw_message_dialog_new (GTK_WINDOW (self),
                                                 _("Could not set as default"),
                                                 message);
        adw_message_dialog_add_response (ADW_MESSAGE_DIALOG (message_dialog), "close", _("OK"));
        gtk_window_present (GTK_WINDOW (message_dialog));
    }

    gtk_app_chooser_refresh (GTK_APP_CHOOSER (self->app_chooser_widget));
    g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
}

static void
on_application_selected (GtkAppChooserWidget *widget,
                         GAppInfo            *info,
                         gpointer             user_data)
{
    NautilusAppChooser *self = NAUTILUS_APP_CHOOSER (user_data);
    g_autoptr (GAppInfo) default_app = NULL;
    gboolean is_default;

    gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, info != NULL);

    default_app = g_app_info_get_default_for_type (self->content_type, FALSE);
    is_default = default_app != NULL && g_app_info_equal (info, default_app);

    gtk_switch_set_state (GTK_SWITCH (self->set_as_default_switch), is_default);
}

static void
nautilus_app_chooser_set_property (GObject      *object,
                                   guint         param_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
    NautilusAppChooser *self = NAUTILUS_APP_CHOOSER (object);

    switch (param_id)
    {
        case PROP_CONTENT_TYPE:
        {
            self->content_type = g_value_dup_string (value);
        }
        break;

        case PROP_SINGLE_CONTENT_TYPE:
        {
            self->single_content_type = g_value_get_boolean (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_app_chooser_init (NautilusAppChooser *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_widget_set_name (GTK_WIDGET (self), "NautilusAppChooser");
}

static void
nautilus_app_chooser_constructed (GObject *object)
{
    NautilusAppChooser *self = NAUTILUS_APP_CHOOSER (object);
    g_autoptr (GAppInfo) info = NULL;
    g_autofree gchar *content_type_description = NULL;
    gchar *title;

    G_OBJECT_CLASS (nautilus_app_chooser_parent_class)->constructed (object);

    self->app_chooser_widget = gtk_app_chooser_widget_new (self->content_type);
    gtk_widget_set_vexpand (self->app_chooser_widget, TRUE);
    gtk_widget_add_css_class (self->app_chooser_widget, "lowres-icon");
    gtk_box_append (GTK_BOX (self->app_chooser_widget_box), self->app_chooser_widget);

    gtk_app_chooser_widget_set_show_default (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_fallback (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_other (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);

    /* initialize sensitivity */
    info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));
    if (info != NULL)
    {
        on_application_selected (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget),
                                 info, self);
    }

    g_signal_connect (self->app_chooser_widget,
                      "application-selected",
                      G_CALLBACK (on_application_selected),
                      self);

    if (!self->single_content_type)
    {
        title = _("Open Items");
    }
    else if (g_strcmp0 (self->content_type, "inode/directory") == 0)
    {
        title = _("Open Folder");
    }
    else
    {
        title = _("Open File");
    }

    gtk_header_bar_set_title_widget (GTK_HEADER_BAR (gtk_dialog_get_header_bar (GTK_DIALOG (self))),
                                     adw_window_title_new (title, NULL));

    if (self->single_content_type)
    {
        content_type_description = g_content_type_get_description (self->content_type);
        gtk_label_set_label (GTK_LABEL (self->label_content_type_description), content_type_description);
    }
    else
    {
        gtk_widget_set_visible (self->set_default_box, FALSE);
    }
}

static void
nautilus_app_chooser_finalize (GObject *object)
{
    NautilusAppChooser *self = (NautilusAppChooser *) object;

    g_clear_pointer (&self->content_type, g_free);

    G_OBJECT_CLASS (nautilus_app_chooser_parent_class)->finalize (object);
}

static void
nautilus_app_chooser_class_init (NautilusAppChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_app_chooser_finalize;
    object_class->constructed = nautilus_app_chooser_constructed;
    object_class->set_property = nautilus_app_chooser_set_property;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-app-chooser.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, app_chooser_widget_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, set_as_default_switch);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, label_content_type_description);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, set_default_box);

    gtk_widget_class_bind_template_callback (widget_class, open_button_clicked_cb);

    g_object_class_install_property (object_class,
                                     PROP_CONTENT_TYPE,
                                     g_param_spec_string ("content-type", "", "",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
                                     PROP_SINGLE_CONTENT_TYPE,
                                     g_param_spec_boolean ("single-content-type", "", "",
                                                           TRUE,
                                                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

NautilusAppChooser *
nautilus_app_chooser_new (GList     *files,
                          GtkWindow *parent_window)
{
    gboolean single_content_type = TRUE;
    g_autofree gchar *content_type = NULL;

    content_type = nautilus_file_get_mime_type (files->data);

    for (GList *l = files; l != NULL; l = l->next)
    {
        g_autofree gchar *temp_mime_type = NULL;
        temp_mime_type = nautilus_file_get_mime_type (l->data);
        if (g_strcmp0 (content_type, temp_mime_type) != 0)
        {
            single_content_type = FALSE;
            break;
        }
    }

    return NAUTILUS_APP_CHOOSER (g_object_new (NAUTILUS_TYPE_APP_CHOOSER,
                                               "transient-for", parent_window,
                                               "content-type", content_type,
                                               "use-header-bar", TRUE,
                                               "single-content-type", single_content_type,
                                               NULL));
}

GAppInfo *
nautilus_app_chooser_get_app_info (NautilusAppChooser *self)
{
    return gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));
}

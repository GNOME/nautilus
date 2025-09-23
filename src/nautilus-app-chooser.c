/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-app-chooser.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include "nautilus-file.h"
#include "nautilus-signaller.h"

struct _NautilusAppChooser
{
    AdwDialog parent_instance;

    gchar *content_type;
    gchar *file_name;
    gboolean single_content_type;

    GtkWidget *ok_button;
    GtkWidget *content_box;
    GtkWidget *label_description;
    GtkWidget *set_default_list_box;
    GtkWidget *set_default_row;

    GtkWidget *app_chooser_widget;
};

G_DEFINE_TYPE (NautilusAppChooser, nautilus_app_chooser, ADW_TYPE_DIALOG)

enum
{
    PROP_0,
    PROP_CONTENT_TYPE,
    PROP_SINGLE_CONTENT_TYPE,
    PROP_FILE_NAME,
    LAST_PROP
};

enum
{
    SIGNAL_APP_SELECTED,
    SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static void
open_cb (NautilusAppChooser *self)
{
    gboolean set_new_default = FALSE;
    g_autoptr (GAppInfo) info = NULL;
    g_autoptr (GError) error = NULL;

    info = nautilus_app_chooser_get_app_info (self);

    if (!self->single_content_type)
    {
        /* Don't attempt to set an association with multiple content types */
        return;
    }

    /* The switch is insensitive if the selected app is already default */
    if (gtk_widget_get_sensitive (self->set_default_row))
    {
        set_new_default = adw_switch_row_get_active (ADW_SWITCH_ROW (self->set_default_row));
    }

    if (set_new_default)
    {
        g_app_info_set_as_default_for_type (info, self->content_type,
                                            &error);
        g_signal_emit_by_name (nautilus_signaller_get_current (), "mime-data-changed");
    }

    if (error != NULL)
    {
        AdwAlertDialog *dialog;

        dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (_("Could not set as default"), NULL));
        adw_alert_dialog_format_body (dialog,
                                      _("Error while setting “%s” as default app: %s"),
                                      g_app_info_get_display_name (info), error->message);
        adw_alert_dialog_add_response (dialog, "close", _("_OK"));

        adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
    }

    g_signal_emit (self, signals[SIGNAL_APP_SELECTED], 0, info);
}

static void
on_application_activated (NautilusAppChooser *self)
{
    open_cb (self);
}

static void
on_application_selected (GtkAppChooserWidget *widget,
                         GAppInfo            *info,
                         gpointer             user_data)
{
    NautilusAppChooser *self = NAUTILUS_APP_CHOOSER (user_data);
    g_autoptr (GAppInfo) default_app = NULL;
    gboolean is_default;

    gtk_widget_set_sensitive (self->ok_button, info != NULL);

    default_app = g_app_info_get_default_for_type (self->content_type, FALSE);
    is_default = default_app != NULL && g_app_info_equal (info, default_app);

    adw_switch_row_set_active (ADW_SWITCH_ROW (self->set_default_row), is_default);
    gtk_widget_set_sensitive (GTK_WIDGET (self->set_default_row), !is_default);
}

static void
focus_app_chooser_widget (NautilusAppChooser *self)
{
    /* This is a very hacky way to make focusing on the app chooser widget work.
     * The widget is deprecated anyway and intended to be replaced by a new
     * implementation, so we'll live with this rather than patching GTK.
     */

    GtkWidget *child = gtk_widget_get_first_child (self->app_chooser_widget);
    g_return_if_fail (GTK_IS_OVERLAY (child));

    child = gtk_widget_get_first_child (child);
    g_return_if_fail (GTK_IS_SCROLLED_WINDOW (child));

    child = gtk_widget_get_first_child (child);
    g_return_if_fail (GTK_IS_LIST_VIEW (child));

    gtk_widget_grab_focus (child);

    /* Matching ref of timeout creation */
    g_object_unref (self);
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

        case PROP_FILE_NAME:
        {
            self->file_name = g_value_dup_string (value);
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

static gboolean
content_type_is_folder (NautilusAppChooser *self)
{
    return g_strcmp0 (self->content_type, "inode/directory") == 0;
}

static void
nautilus_app_chooser_constructed (GObject *object)
{
    NautilusAppChooser *self = NAUTILUS_APP_CHOOSER (object);
    g_autoptr (GAppInfo) info = NULL;
    g_autofree gchar *description = NULL;
    gchar *title;

    G_OBJECT_CLASS (nautilus_app_chooser_parent_class)->constructed (object);

    self->app_chooser_widget = gtk_app_chooser_widget_new (self->content_type);
    gtk_widget_set_vexpand (self->app_chooser_widget, TRUE);
    gtk_widget_add_css_class (self->app_chooser_widget, "lowres-icon");
    gtk_box_append (GTK_BOX (self->content_box), self->app_chooser_widget);

    gtk_app_chooser_widget_set_show_default (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_fallback (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);
    gtk_app_chooser_widget_set_show_other (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget), TRUE);

    /* See comment in focus_app_chooser_widget(). Hold self reference to prevent segfaults. */
    guint upper_dialog_creation_estimate = 100;
    g_timeout_add_once (upper_dialog_creation_estimate,
                        (GSourceOnceFunc) focus_app_chooser_widget,
                        g_object_ref (self));

    /* initialize sensitivity */
    info = nautilus_app_chooser_get_app_info (self);
    if (info != NULL)
    {
        on_application_selected (GTK_APP_CHOOSER_WIDGET (self->app_chooser_widget),
                                 info, self);
    }

    g_signal_connect_object (self->app_chooser_widget, "application-selected",
                             G_CALLBACK (on_application_selected), self, 0);
    g_signal_connect_object (self->app_chooser_widget, "application-activated",
                             G_CALLBACK (on_application_activated), self, G_CONNECT_SWAPPED);

    if (self->file_name != NULL)
    {
        /* Translators: %s is the filename.  i.e. "Choose an app to open test.jpg" */
        description = g_strdup_printf (_("Choose an app to open <b>%s</b>"), self->file_name);
        gtk_label_set_markup (GTK_LABEL (self->label_description), description);
    }

    if (!self->single_content_type)
    {
        title = _("Open Items");
    }
    else if (content_type_is_folder (self))
    {
        title = _("Open Folder");
    }
    else
    {
        title = _("Open File");
    }

    adw_dialog_set_title (ADW_DIALOG (self), title);

    if (self->single_content_type && !content_type_is_folder (self))
    {
        g_autofree gchar *type_description = g_content_type_get_description (self->content_type);
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->set_default_row), type_description);
    }
    else
    {
        gtk_widget_set_visible (self->set_default_list_box, FALSE);
    }
}

static void
nautilus_app_chooser_dispose (GObject *object)
{
    NautilusAppChooser *self = (NautilusAppChooser *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_APP_CHOOSER);

    G_OBJECT_CLASS (nautilus_app_chooser_parent_class)->dispose (object);
}

static void
nautilus_app_chooser_finalize (GObject *object)
{
    NautilusAppChooser *self = (NautilusAppChooser *) object;

    g_clear_pointer (&self->content_type, g_free);
    g_clear_pointer (&self->file_name, g_free);

    G_OBJECT_CLASS (nautilus_app_chooser_parent_class)->finalize (object);
}

static void
nautilus_app_chooser_class_init (NautilusAppChooserClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_app_chooser_dispose;
    object_class->finalize = nautilus_app_chooser_finalize;
    object_class->constructed = nautilus_app_chooser_constructed;
    object_class->set_property = nautilus_app_chooser_set_property;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-app-chooser.ui");

    signals[SIGNAL_APP_SELECTED] = g_signal_new ("app-selected",
                                                 NAUTILUS_TYPE_APP_CHOOSER,
                                                 G_SIGNAL_RUN_LAST,
                                                 0, NULL, NULL, NULL,
                                                 G_TYPE_NONE,
                                                 0,
                                                 NULL);

    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, ok_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, content_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, label_description);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, set_default_list_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooser, set_default_row);

    gtk_widget_class_bind_template_callback (widget_class, open_cb);

    g_object_class_install_property (object_class,
                                     PROP_CONTENT_TYPE,
                                     g_param_spec_string ("content-type", "", "",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
                                     PROP_FILE_NAME,
                                     g_param_spec_string ("file-name", "", "",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

    g_object_class_install_property (object_class,
                                     PROP_SINGLE_CONTENT_TYPE,
                                     g_param_spec_boolean ("single-content-type", "", "",
                                                           TRUE,
                                                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

NautilusAppChooser *
nautilus_app_chooser_new (GList *files)
{
    gboolean single_content_type = TRUE;
    const char *content_type = nautilus_file_get_mime_type (files->data);
    const char *file_name = files->next ? NULL : nautilus_file_get_display_name (files->data);

    for (GList *l = files; l != NULL; l = l->next)
    {
        if (g_strcmp0 (content_type, nautilus_file_get_mime_type (l->data)) != 0)
        {
            single_content_type = FALSE;
            break;
        }
    }

    return NAUTILUS_APP_CHOOSER (g_object_new (NAUTILUS_TYPE_APP_CHOOSER,
                                               "content-type", content_type,
                                               "file-name", file_name,
                                               "single-content-type", single_content_type,
                                               NULL));
}

GAppInfo *
nautilus_app_chooser_get_app_info (NautilusAppChooser *self)
{
    return gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser_widget));
}

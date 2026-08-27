/*
 * SPDX-FileCopyrightText: 2004 Novell, Inc.
 * SPDX-FileCopyrightText: 2007, 2010 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2026 GNOME Files Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 *          Khalid Abu Shawarib <kas@gnome.org>
 */

#include "nautilus-app-chooser-widget.h"

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

/**
 * NautilusAppChooserWidget:
 *
 * `NautilusAppChooserWidget` is a widget for selecting applications.
 *
 * It is the main building block for [class@Nautilus.AppChooser].
 * Most applications only need to use the latter; but you can use
 * this widget as part of a larger widget if you have special needs.
 *
 * When an application is chosen, [signal@Nautilus.AppChooserWidget::app-chosen]
 * is emitted.
 *
 * ## CSS nodes
 *
 * `NautilusAppChooserWidget` has a single CSS node with name appchooser.
 */

#define NAUTILUS_TYPE_APP_ITEM (nautilus_app_item_get_type ())
G_DECLARE_FINAL_TYPE (NautilusAppItem, nautilus_app_item, NAUTILUS, APP_ITEM, GObject)

struct _NautilusAppItem
{
    GObject parent_instance;
    GAppInfo *app_info;
    gboolean is_default;
    gboolean is_recommended;
    gboolean is_fallback;
};

enum
{
    ITEM_PROP_NAME = 1,
    ITEM_PROP_ICON,
    ITEM_PROP_SELF,
    NUM_ITEM_PROPS,
};

static GParamSpec *item_properties[NUM_ITEM_PROPS];

G_DEFINE_FINAL_TYPE (NautilusAppItem, nautilus_app_item, G_TYPE_OBJECT)

static void
nautilus_app_item_init (NautilusAppItem *item)
{
}

static void
nautilus_app_item_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    NautilusAppItem *item = NAUTILUS_APP_ITEM (object);

    switch (prop_id)
    {
        case ITEM_PROP_NAME:
        {
            g_value_set_string (value, g_app_info_get_display_name (item->app_info));
            break;
        }

        case ITEM_PROP_ICON:
        {
            g_value_set_object (value, g_app_info_get_icon (item->app_info));
            break;
        }

        case ITEM_PROP_SELF:
        {
            g_value_set_object (value, item);
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
        }
    }
}

static void
nautilus_app_item_finalize (GObject *object)
{
    NautilusAppItem *item = NAUTILUS_APP_ITEM (object);

    g_object_unref (item->app_info);

    G_OBJECT_CLASS (nautilus_app_item_parent_class)->finalize (object);
}

static void
nautilus_app_item_class_init (NautilusAppItemClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->get_property = nautilus_app_item_get_property;
    object_class->finalize = nautilus_app_item_finalize;

    item_properties[ITEM_PROP_NAME] = g_param_spec_string ("name", NULL, NULL,
                                                           NULL,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS);

    item_properties[ITEM_PROP_ICON] = g_param_spec_object ("icon", NULL, NULL,
                                                           G_TYPE_ICON,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS);

    item_properties[ITEM_PROP_SELF] = g_param_spec_object ("self", NULL, NULL,
                                                           NAUTILUS_TYPE_APP_ITEM,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, NUM_ITEM_PROPS, item_properties);
}

static NautilusAppItem *
nautilus_app_item_new (GAppInfo *app_info,
                       gboolean  is_default,
                       gboolean  is_recommended,
                       gboolean  is_fallback)
{
    NautilusAppItem *item = g_object_new (NAUTILUS_TYPE_APP_ITEM, NULL);

    item->app_info = g_object_ref (app_info);
    item->is_default = is_default;
    item->is_recommended = is_recommended;
    item->is_fallback = is_fallback;

    return item;
}

struct _NautilusAppChooserWidget
{
    AdwBin parent_instance;

    GAppInfo *selected_app_info;

    GtkStack *list_stack;

    char *content_type;
    gboolean show_all;

    GtkGridView *program_list;
    GListStore *app_info_store;
    GListModel *selection_model;
    GtkStringFilter *name_filter;
    GtkCustomSorter *section_sorter;
    GtkGridView *grid;
    AdwStatusPage *no_apps_page;

    GAppInfoMonitor *monitor;

    GtkWidget *popup_menu;
};

enum
{
    PROP_CONTENT_TYPE = 1,
    PROP_SHOW_ALL,
    N_PROPERTIES
};

static GParamSpec *widget_properties[N_PROPERTIES];

enum
{
    SIGNAL_APP_CHOSEN,
    N_SIGNALS
};

static guint signals[N_SIGNALS];


G_DEFINE_FINAL_TYPE (NautilusAppChooserWidget, nautilus_app_chooser_widget, ADW_TYPE_BIN);

static void
on_app_chosen (GtkGridView              *grid,
               guint                     position,
               NautilusAppChooserWidget *self);
static void
changed_cb (GtkEditable              *editable,
            NautilusAppChooserWidget *self);

static guint
app_info_hash (gconstpointer key)
{
    GAppInfo *app = (gpointer) key;
    guint hash = g_str_hash (g_app_info_get_name (app));
    const GIcon *icon = g_app_info_get_icon (app);
    const char *executable = g_app_info_get_executable (app);
    const char *commandline = g_app_info_get_commandline (app);

    if (icon != NULL)
    {
        hash ^= g_icon_hash (icon);
    }

    if (executable != NULL)
    {
        hash ^= g_str_hash (executable);
    }

    if (commandline != NULL)
    {
        hash ^= g_str_hash (commandline);
    }

    return hash;
}

static gboolean
app_info_equal (gconstpointer a,
                gconstpointer b)
{
    GAppInfo *app_a = (gpointer) a;
    GAppInfo *app_b = (gpointer) b;
    const char *name_a = g_app_info_get_name (app_a);
    const char *name_b = g_app_info_get_name (app_b);
    GIcon *icon_a = g_app_info_get_icon (app_a);
    GIcon *icon_b = g_app_info_get_icon (app_b);
    const char *exec_a = g_app_info_get_executable (app_a);
    const char *exec_b = g_app_info_get_executable (app_b);
    const char *cmd_a = g_app_info_get_commandline (app_a);
    const char *cmd_b = g_app_info_get_commandline (app_b);

    return g_strcmp0 (name_a, name_b) == 0 &&
           g_icon_equal (icon_a, icon_b) &&
           g_strcmp0 (exec_a, exec_b) == 0 &&
           g_strcmp0 (cmd_a, cmd_b) == 0;
}

static void
nautilus_app_chooser_widget_add_section (NautilusAppChooserWidget *self,
                                         gboolean                  recommended,
                                         gboolean                  fallback,
                                         GList                    *applications,
                                         GHashTable               *seen_apps)
{
    for (GList *l = applications; l != NULL; l = l->next)
    {
        GAppInfo *app = l->data;

        if (self->content_type != NULL &&
            !g_app_info_supports_uris (app) &&
            !g_app_info_supports_files (app))
        {
            continue;
        }

        if (g_hash_table_contains (seen_apps, app))
        {
            continue;
        }

        g_hash_table_add (seen_apps, app);

        g_autoptr (NautilusAppItem) item = nautilus_app_item_new (app, FALSE, recommended, fallback);

        g_list_store_append (self->app_info_store, item);
    }
}

static gboolean
display_app_check (NautilusAppChooserWidget *self,
                   NautilusAppItem          *item)
{
    return self->show_all || item->is_default || item->is_recommended || item->is_fallback;
}

static void
nautilus_app_chooser_widget_real_add_items (NautilusAppChooserWidget *self)
{
    g_autolist (GAppInfo) recommended_apps = NULL;
    g_autolist (GAppInfo) fallback_apps = NULL;
    g_autoptr (GHashTable) seen_apps = g_hash_table_new (app_info_hash, app_info_equal);

    if (self->content_type != NULL)
    {
        recommended_apps = g_app_info_get_recommended_for_type (self->content_type);

        nautilus_app_chooser_widget_add_section (self,
                                                 TRUE, /* mark as recommended */
                                                 FALSE, /* mark as fallback */
                                                 recommended_apps, seen_apps);

        fallback_apps = g_app_info_get_fallback_for_type (self->content_type);

        nautilus_app_chooser_widget_add_section (self,
                                                 FALSE, /* mark as recommended */
                                                 TRUE, /* mark as fallback */
                                                 fallback_apps, seen_apps);
    }

    if (self->show_all)
    {
        g_autolist (GAppInfo) all_applications = g_app_info_get_all ();

        nautilus_app_chooser_widget_add_section (self,
                                                 FALSE,
                                                 FALSE,
                                                 all_applications, seen_apps);
    }

    gboolean apps_added = g_hash_table_size (seen_apps) > 0;

    if (!apps_added && self->content_type != NULL)
    {
        g_autofree char *desc = g_content_type_get_description (self->content_type);
        g_autofree char *text = g_strdup_printf (_("No Apps Found For “%s”"), desc);

        adw_status_page_set_title (self->no_apps_page, text);
    }
}

static void
nautilus_app_chooser_widget_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
    NautilusAppChooserWidget *self = NAUTILUS_APP_CHOOSER_WIDGET (object);

    switch (property_id)
    {
        case PROP_CONTENT_TYPE:
        {
            self->content_type = g_value_dup_string (value);
            break;
        }

        case PROP_SHOW_ALL:
        {
            self->show_all = g_value_get_boolean (value);
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
        }
    }
}

static void
nautilus_app_chooser_widget_get_property (GObject    *object,
                                          guint       property_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
    NautilusAppChooserWidget *self = NAUTILUS_APP_CHOOSER_WIDGET (object);

    switch (property_id)
    {
        case PROP_CONTENT_TYPE:
        {
            g_value_set_string (value, self->content_type);
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
        }
    }
}

static void
nautilus_app_chooser_widget_constructed (GObject *object)
{
    NautilusAppChooserWidget *self = NAUTILUS_APP_CHOOSER_WIDGET (object);

    G_OBJECT_CLASS (nautilus_app_chooser_widget_parent_class)->constructed (object);

    nautilus_app_chooser_widget_refresh (self);

    /* Assure first item is scrolled into view */
    gtk_grid_view_scroll_to (self->program_list, 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void
nautilus_app_chooser_widget_finalize (GObject *object)
{
    NautilusAppChooserWidget *self = NAUTILUS_APP_CHOOSER_WIDGET (object);

    g_free (self->content_type);
    g_object_unref (self->monitor);

    G_OBJECT_CLASS (nautilus_app_chooser_widget_parent_class)->finalize (object);
}

static void
nautilus_app_chooser_widget_dispose (GObject *object)
{
    NautilusAppChooserWidget *self = NAUTILUS_APP_CHOOSER_WIDGET (object);

    g_clear_object (&self->selected_app_info);

    G_OBJECT_CLASS (nautilus_app_chooser_widget_parent_class)->dispose (object);
}

static void
nautilus_app_chooser_widget_class_init (NautilusAppChooserWidgetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->dispose = nautilus_app_chooser_widget_dispose;
    gobject_class->finalize = nautilus_app_chooser_widget_finalize;
    gobject_class->set_property = nautilus_app_chooser_widget_set_property;
    gobject_class->get_property = nautilus_app_chooser_widget_get_property;
    gobject_class->constructed = nautilus_app_chooser_widget_constructed;

    g_type_ensure (NAUTILUS_TYPE_APP_ITEM);

    /**
     * NautilusAppChooserWidget:content-type:
     *
     * The content type of the `NautilusAppChooserWidget` object.
     *
     * See `GContentType` for more information about content types.
     */
    widget_properties[PROP_CONTENT_TYPE] =
        g_param_spec_string ("content-type", NULL, NULL,
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    widget_properties[PROP_SHOW_ALL] =
        g_param_spec_boolean ("show-all", NULL, NULL,
                              FALSE,
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class,
                                       G_N_ELEMENTS (widget_properties),
                                       widget_properties);

    /**
     * NautilusAppChooserWidget::app-chosen:
     * @self: the object which received the signal
     * @application: the chosen `GAppInfo`
     *
     * Emitted when an application item is chosen.
     */
    signals[SIGNAL_APP_CHOSEN] =
        g_signal_new ("app-chosen",
                      NAUTILUS_TYPE_APP_CHOOSER_WIDGET,
                      G_SIGNAL_RUN_FIRST,
                      0,
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE,
                      1, G_TYPE_APP_INFO);

    /* Bind class to template
     */
    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-app-chooser-widget.ui");
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, program_list);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, selection_model);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, no_apps_page);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, list_stack);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, app_info_store);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, name_filter);
    gtk_widget_class_bind_template_child (widget_class, NautilusAppChooserWidget, section_sorter);

    gtk_widget_class_bind_template_callback (widget_class, display_app_check);
    gtk_widget_class_bind_template_callback (widget_class, on_app_chosen);

    gtk_widget_class_set_css_name (widget_class, "appchooser");
}

static void
on_app_chosen (GtkGridView              *grid,
               guint                     position,
               NautilusAppChooserWidget *self)
{
    g_autoptr (NautilusAppItem) app_item =
        g_list_model_get_item (G_LIST_MODEL (gtk_grid_view_get_model (grid)), position);

    g_set_object (&self->selected_app_info, app_item->app_info);

    g_signal_emit (self, signals[SIGNAL_APP_CHOSEN], 0, self->selected_app_info);
}

static int
compare_section (gconstpointer a,
                 gconstpointer b,
                 gpointer      data)
{
    const NautilusAppItem *item1 = a;
    const NautilusAppItem *item2 = b;

    if (item1->is_default != item2->is_default)
    {
        return item1->is_default ? -1 : 1;
    }
    else if (item1->is_recommended != item2->is_recommended)
    {
        return item1->is_recommended ? -1 : 1;
    }
    else if (item1->is_fallback != item2->is_fallback)
    {
        return item1->is_fallback ? -1 : 1;
    }

    return 0;
}

static void
nautilus_app_chooser_widget_init (NautilusAppChooserWidget *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    gtk_custom_sorter_set_sort_func (self->section_sorter, compare_section, NULL, NULL);

    self->monitor = g_app_info_monitor_get ();
    g_signal_connect_object (self->monitor, "changed",
                             G_CALLBACK (nautilus_app_chooser_widget_refresh), self, G_CONNECT_SWAPPED);
}

GAppInfo *
nautilus_app_chooser_widget_get_app_info (NautilusAppChooserWidget *self)
{
    if (self->selected_app_info == NULL)
    {
        return NULL;
    }

    return g_object_ref (self->selected_app_info);
}

void
nautilus_app_chooser_widget_refresh (NautilusAppChooserWidget *self)
{
    g_list_store_remove_all (self->app_info_store);
    nautilus_app_chooser_widget_real_add_items (self);
}

/**
 * nautilus_app_chooser_widget_new:
 * @content_type: the content type to show applications for
 * @search_entry: (nullable): the search entry to use for filtering applications
 *
 * Creates a new `NautilusAppChooserWidget` for applications
 * that can handle content of the given type.
 *
 * Returns: a newly created `NautilusAppChooserWidget`
 */
NautilusAppChooserWidget *
nautilus_app_chooser_widget_new (const char  *content_type,
                                 GtkEditable *search_entry)
{
    NautilusAppChooserWidget *self = g_object_new (NAUTILUS_TYPE_APP_CHOOSER_WIDGET,
                                                   "content-type", content_type,
                                                   "show-all", search_entry != NULL,
                                                   NULL);

    if (search_entry != NULL)
    {
        g_signal_connect (search_entry, "changed", G_CALLBACK (changed_cb), self);
    }

    return self;
}

static void
changed_cb (GtkEditable              *editable,
            NautilusAppChooserWidget *self)
{
    gtk_string_filter_set_search (self->name_filter, gtk_editable_get_text (editable));

    if (g_list_model_get_n_items (self->selection_model) > 0)
    {
        gtk_stack_set_visible_child_name (self->list_stack, "list");
    }
    else
    {
        gtk_stack_set_visible_child_name (self->list_stack, "no-apps");
    }
}

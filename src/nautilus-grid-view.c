/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-list-base-private.h"
#include "nautilus-grid-view.h"

#include "nautilus-grid-cell.h"
#include "nautilus-global-preferences.h"

struct _NautilusGridView
{
    NautilusListBase parent_instance;

    GtkGridView *view_ui;

    GActionGroup *action_group;
    gint zoom_level;

    gboolean directories_first;

    GQuark caption_attributes[NAUTILUS_GRID_CELL_N_CAPTIONS];

    GQuark sort_attribute;
    gboolean reversed;
};

G_DEFINE_TYPE (NautilusGridView, nautilus_grid_view, NAUTILUS_TYPE_LIST_BASE)

static guint get_icon_size_for_zoom_level (NautilusGridZoomLevel zoom_level);

#define get_view_item(li) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

static gint
nautilus_grid_view_sort (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data)
{
    NautilusGridView *self = user_data;
    NautilusFile *file_a;
    NautilusFile *file_b;

    file_a = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) a));
    file_b = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM ((gpointer) b));

    return nautilus_file_compare_for_sort_by_attribute_q (file_a, file_b,
                                                          self->sort_attribute,
                                                          self->directories_first,
                                                          self->reversed);
}

static void
update_sort_directories_first (NautilusGridView *self)
{
    NautilusFile *directory_as_file = nautilus_list_base_get_directory_as_file (NAUTILUS_LIST_BASE (self));

    if (nautilus_file_is_in_search (directory_as_file))
    {
        self->directories_first = FALSE;
    }
    else
    {
        self->directories_first = g_settings_get_boolean (gtk_filechooser_preferences,
                                                          NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
    }

    nautilus_view_model_sort (nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self)));
}

static void
nautilus_grid_view_setup_directory (NautilusListBase  *list_base,
                                    NautilusDirectory *new_directory)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);

    NAUTILUS_LIST_BASE_CLASS (nautilus_grid_view_parent_class)->setup_directory (list_base, new_directory);

    update_sort_directories_first (self);
}

static void
real_bump_zoom_level (NautilusFilesView *files_view,
                      int                zoom_increment)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (files_view);
    NautilusGridZoomLevel new_level;

    new_level = self->zoom_level + zoom_increment;

    if (new_level >= NAUTILUS_GRID_ZOOM_LEVEL_SMALL &&
        new_level <= NAUTILUS_GRID_ZOOM_LEVEL_EXTRA_LARGE)
    {
        g_action_group_change_action_state (self->action_group,
                                            "zoom-to-level",
                                            g_variant_new_int32 (new_level));
    }
}

static guint
get_icon_size_for_zoom_level (NautilusGridZoomLevel zoom_level)
{
    switch (zoom_level)
    {
        case NAUTILUS_GRID_ZOOM_LEVEL_SMALL:
        {
            return NAUTILUS_GRID_ICON_SIZE_SMALL;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_SMALL_PLUS:
        {
            return NAUTILUS_GRID_ICON_SIZE_SMALL_PLUS;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_MEDIUM:
        {
            return NAUTILUS_GRID_ICON_SIZE_MEDIUM;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_LARGE:
        {
            return NAUTILUS_GRID_ICON_SIZE_LARGE;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_EXTRA_LARGE:
        {
            return NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE;
        }
        break;
    }
    g_return_val_if_reached (NAUTILUS_GRID_ICON_SIZE_MEDIUM);
}

static gint
get_default_zoom_level (void)
{
    NautilusGridZoomLevel default_zoom_level;

    default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
                                              NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

    /* Sanitize preference value */
    return CLAMP (default_zoom_level,
                  NAUTILUS_GRID_ZOOM_LEVEL_SMALL,
                  NAUTILUS_GRID_ZOOM_LEVEL_EXTRA_LARGE);
}

static void
set_captions_from_preferences (NautilusGridView *self)
{
    g_auto (GStrv) value = NULL;
    gint n_captions_for_zoom_level;

    value = g_settings_get_strv (nautilus_icon_view_preferences,
                                 NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

    /* Set a celling on the number of captions depending on the zoom level. */
    n_captions_for_zoom_level = MIN ((uint) self->zoom_level + 1,
                                     G_N_ELEMENTS (self->caption_attributes));

    /* Reset array to zeros beforehand, as we may not refill all elements. */
    memset (&self->caption_attributes, 0, sizeof (self->caption_attributes));
    for (gint i = 0, quark_i = 0;
         value[i] != NULL && quark_i < n_captions_for_zoom_level;
         i++)
    {
        if (g_strcmp0 (value[i], "none") == 0)
        {
            continue;
        }

        /* Convert to quarks in advance, otherwise each NautilusFile attribute
         * getter would call g_quark_from_string() once for each file. */
        self->caption_attributes[quark_i] = g_quark_from_string (value[i]);
        quark_i++;
    }
}

static void
set_zoom_level (NautilusGridView *self,
                guint             new_level)
{
    self->zoom_level = new_level;

    /* The zoom level may change how many captions are allowed. Update it before
     * notifying the icon size change, under the assumption that NautilusGridCell
     * updates captions whenever the icon size is set*/
    set_captions_from_preferences (self);

    g_object_notify (G_OBJECT (self), "icon-size");

    nautilus_files_view_update_toolbar_menus (NAUTILUS_FILES_VIEW (self));
}

static void
real_restore_standard_zoom_level (NautilusFilesView *files_view)
{
    NautilusGridView *self;

    self = NAUTILUS_GRID_VIEW (files_view);
    g_action_group_change_action_state (self->action_group,
                                        "zoom-to-level",
                                        g_variant_new_int32 (NAUTILUS_GRID_ZOOM_LEVEL_MEDIUM));
}

static gboolean
real_is_zoom_level_default (NautilusFilesView *files_view)
{
    NautilusGridView *self;
    guint icon_size;

    self = NAUTILUS_GRID_VIEW (files_view);
    icon_size = get_icon_size_for_zoom_level (self->zoom_level);

    return icon_size == NAUTILUS_GRID_ICON_SIZE_MEDIUM;
}

static gboolean
real_can_zoom_in (NautilusFilesView *files_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (files_view);

    return self->zoom_level < NAUTILUS_GRID_ZOOM_LEVEL_EXTRA_LARGE;
}

static gboolean
real_can_zoom_out (NautilusFilesView *files_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (files_view);

    return self->zoom_level > NAUTILUS_GRID_ZOOM_LEVEL_SMALL;
}

/* The generic implementation in src/nautilus-list-base.c doesn't allow the
 * 2-dimensional movements expected from a grid. Let's hack GTK here. */
static void
real_preview_selection_event (NautilusListBase *list_base,
                              GtkDirectionType  direction)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base);
    guint direction_keyval;
    g_autoptr (GtkShortcutTrigger) direction_trigger = NULL;
    g_autoptr (GListModel) controllers = NULL;
    gboolean success = FALSE;

    /* We want the same behavior as when the user presses the arrow keys while
     * the focus is in the view. So, let's get the matching arrow key. */
    switch (direction)
    {
        case GTK_DIR_UP:
        {
            direction_keyval = GDK_KEY_Up;
        }
        break;

        case GTK_DIR_DOWN:
        {
            direction_keyval = GDK_KEY_Down;
        }
        break;

        case GTK_DIR_LEFT:
        {
            direction_keyval = GDK_KEY_Left;
        }
        break;

        case GTK_DIR_RIGHT:
        {
            direction_keyval = GDK_KEY_Right;
        }
        break;

        default:
        {
            g_return_if_reached ();
        }
    }

    /* We cannot simulate a click, but we can find the shortcut it triggers and
     * activate its action programatically.
     *
     * First, we create out would-be trigger.*/
    direction_trigger = gtk_keyval_trigger_new (direction_keyval, 0);

    /* Then we iterate over the shortcut installed in GtkGridView until we find
     * a matching trigger. There may be multiple shortcut controllers, and each
     * shortcut controller may hold multiple shortcuts each. Let's loop. */
    controllers = gtk_widget_observe_controllers (GTK_WIDGET (self->view_ui));
    for (guint i = 0; i < g_list_model_get_n_items (controllers); i++)
    {
        g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);

        if (!GTK_IS_SHORTCUT_CONTROLLER (controller))
        {
            continue;
        }

        for (guint j = 0; j < g_list_model_get_n_items (G_LIST_MODEL (controller)); j++)
        {
            g_autoptr (GtkShortcut) shortcut = g_list_model_get_item (G_LIST_MODEL (controller), j);
            GtkShortcutTrigger *trigger = gtk_shortcut_get_trigger (shortcut);

            if (gtk_shortcut_trigger_equal (trigger, direction_trigger))
            {
                /* Match found. Activate the action to move cursor. */
                success = gtk_shortcut_action_activate (gtk_shortcut_get_action (shortcut),
                                                        0,
                                                        GTK_WIDGET (self->view_ui),
                                                        gtk_shortcut_get_arguments (shortcut));
                break;
            }
        }
    }

    /* If the hack fails (GTK may change it's internal behavior), fallback. */
    if (!success)
    {
        NAUTILUS_LIST_BASE_CLASS (nautilus_grid_view_parent_class)->preview_selection_event (list_base, direction);
    }
}

/* We only care about the keyboard activation part that GtkGridView provides,
 * but we don't need any special filtering here. Indeed, we ask GtkGridView
 * to not activate on single click, and we get to handle double clicks before
 * GtkGridView does (as one of widget subclassing's goal is to modify the parent
 * class's behavior), while claiming the click gestures, so it means GtkGridView
 * will never react to a click event to emit this signal. So we should be pretty
 * safe here with regards to our custom item click handling.
 */
static void
on_grid_view_item_activated (GtkGridView *grid_view,
                             guint        position,
                             gpointer     user_data)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (user_data);

    nautilus_files_view_activate_selection (NAUTILUS_FILES_VIEW (self));
}

static guint
real_get_icon_size (NautilusListBase *list_base_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    return get_icon_size_for_zoom_level (self->zoom_level);
}

static GtkWidget *
real_get_view_ui (NautilusListBase *list_base_view)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    return GTK_WIDGET (self->view_ui);
}

static void
real_scroll_to (NautilusListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (list_base_view);

    gtk_grid_view_scroll_to (self->view_ui, position, flags, scroll);
}

static void
action_sort_order_changed (GSimpleAction *action,
                           GVariant      *value,
                           gpointer       user_data)
{
    g_autoptr (GVariant) old_value = g_action_get_state (G_ACTION (action));
    const gchar *target_name;
    NautilusGridView *self = NAUTILUS_GRID_VIEW (user_data);
    NautilusViewModel *model;
    g_autoptr (GtkCustomSorter) sorter = NULL;

    /* Don't resort if the action is in the same state as before */
    if (g_variant_equal (value, old_value))
    {
        return;
    }

    g_variant_get (value, "(&sb)", &target_name, &self->reversed);
    self->sort_attribute = g_quark_from_string (target_name);

    sorter = gtk_custom_sorter_new (nautilus_grid_view_sort, self, NULL);
    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));
    nautilus_view_model_set_sorter (model, GTK_SORTER (sorter));

    g_simple_action_set_state (action, value);
}

static guint
real_get_view_id (NautilusFilesView *files_view)
{
    return NAUTILUS_VIEW_GRID_ID;
}

static void
action_zoom_to_level (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (user_data);
    int zoom_level;

    zoom_level = g_variant_get_int32 (state);
    set_zoom_level (self, zoom_level);
    g_simple_action_set_state (G_SIMPLE_ACTION (action), state);

    if (g_settings_get_enum (nautilus_icon_view_preferences,
                             NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL) != zoom_level)
    {
        g_settings_set_enum (nautilus_icon_view_preferences,
                             NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                             zoom_level);
    }
}

static void
on_captions_preferences_changed (NautilusGridView *self)
{
    set_captions_from_preferences (self);

    /* Hack: this relies on the assumption that NautilusGridCell updates
     * captions whenever the icon size is set (even if it's the same value). */
    g_object_notify (G_OBJECT (self), "icon-size");
}

static void
dispose (GObject *object)
{
    G_OBJECT_CLASS (nautilus_grid_view_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_grid_view_parent_class)->finalize (object);
}

static void
bind_cell (GtkSignalListItemFactory *factory,
           GtkListItem              *listitem,
           gpointer                  user_data)
{
    GtkWidget *cell;
    g_autoptr (NautilusViewItem) item = NULL;

    cell = gtk_list_item_get_child (listitem);
    item = get_view_item (listitem);
    g_return_if_fail (item != NULL);

    nautilus_view_item_set_item_ui (item, cell);

    if (nautilus_view_cell_once (NAUTILUS_VIEW_CELL (cell)))
    {
        GtkWidget *parent;

        /* At the time of ::setup emission, the item ui has got no parent yet,
         * that's why we need to complete the widget setup process here, on the
         * first time ::bind is emitted. */
        parent = gtk_widget_get_parent (cell);
        gtk_widget_set_halign (parent, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (parent, GTK_ALIGN_START);

        gtk_accessible_update_relation (GTK_ACCESSIBLE (parent),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, cell, NULL,
                                        -1);
    }
}

static void
unbind_cell (GtkSignalListItemFactory *factory,
             GtkListItem              *listitem,
             gpointer                  user_data)
{
    g_autoptr (NautilusViewItem) item = NULL;

    item = get_view_item (listitem);

    /* item may be NULL when row has just been destroyed. */
    if (item != NULL)
    {
        nautilus_view_item_set_item_ui (item, NULL);
    }
}

static void
setup_cell (GtkSignalListItemFactory *factory,
            GtkListItem              *listitem,
            gpointer                  user_data)
{
    NautilusGridView *self = NAUTILUS_GRID_VIEW (user_data);
    NautilusGridCell *cell;

    cell = nautilus_grid_cell_new (NAUTILUS_LIST_BASE (self));
    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), NAUTILUS_VIEW_CELL (cell));
    setup_cell_hover (NAUTILUS_VIEW_CELL (cell));

    g_object_bind_property (self, "icon-size",
                            cell, "icon-size",
                            G_BINDING_SYNC_CREATE);

    nautilus_grid_cell_set_caption_attributes (cell, self->caption_attributes);
}

static GtkGridView *
create_view_ui (NautilusGridView *self)
{
    NautilusViewModel *model;
    GtkListItemFactory *factory;
    GtkWidget *widget;

    model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_cell), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_cell), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cell), self);

    widget = gtk_grid_view_new (GTK_SELECTION_MODEL (model), factory);

    /* We don't use the built-in child activation feature for clicks because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkGridView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_grid_view_set_single_click_activate (GTK_GRID_VIEW (widget), FALSE);
    gtk_grid_view_set_max_columns (GTK_GRID_VIEW (widget), 20);
    gtk_grid_view_set_enable_rubberband (GTK_GRID_VIEW (widget), TRUE);
    gtk_grid_view_set_tab_behavior (GTK_GRID_VIEW (widget), GTK_LIST_TAB_ITEM);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter).
     */
    g_signal_connect (widget, "activate", G_CALLBACK (on_grid_view_item_activated), self);

    return GTK_GRID_VIEW (widget);
}

const GActionEntry view_icon_actions[] =
{
    { .name = "sort", .parameter_type = "(sb)", .state = "('invalid',false)", .change_state = action_sort_order_changed },
    { .name = "zoom-to-level", .state = "100", .change_state = action_zoom_to_level }
};

static void
nautilus_grid_view_class_init (NautilusGridViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFilesViewClass *files_view_class = NAUTILUS_FILES_VIEW_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->dispose = dispose;
    object_class->finalize = finalize;

    files_view_class->bump_zoom_level = real_bump_zoom_level;
    files_view_class->can_zoom_in = real_can_zoom_in;
    files_view_class->can_zoom_out = real_can_zoom_out;
    files_view_class->get_view_id = real_get_view_id;
    files_view_class->restore_standard_zoom_level = real_restore_standard_zoom_level;
    files_view_class->is_zoom_level_default = real_is_zoom_level_default;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_view_ui = real_get_view_ui;
    list_base_view_class->preview_selection_event = real_preview_selection_event;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->setup_directory = nautilus_grid_view_setup_directory;
}

static void
nautilus_grid_view_init (NautilusGridView *self)
{
    GtkWidget *content_widget;

    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-grid-view");

    set_captions_from_preferences (self);
    g_signal_connect_object (nautilus_icon_view_preferences,
                             "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
                             G_CALLBACK (on_captions_preferences_changed),
                             self,
                             G_CONNECT_SWAPPED);

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));

    self->view_ui = create_view_ui (self);
    nautilus_list_base_setup_gestures (NAUTILUS_LIST_BASE (self));

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_widget),
                                   GTK_WIDGET (self->view_ui));

    self->action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     view_icon_actions,
                                     G_N_ELEMENTS (view_icon_actions),
                                     self);

    g_signal_connect_object (gtk_filechooser_preferences,
                             "changed::" NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
                             G_CALLBACK (update_sort_directories_first),
                             self,
                             G_CONNECT_SWAPPED);

    self->zoom_level = get_default_zoom_level ();
    /* Keep the action synced with the actual value, so the toolbar can poll it */
    g_action_group_change_action_state (nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self)),
                                        "zoom-to-level", g_variant_new_int32 (self->zoom_level));
}

NautilusGridView *
nautilus_grid_view_new (NautilusWindowSlot *slot)
{
    return g_object_new (NAUTILUS_TYPE_GRID_VIEW,
                         "window-slot", slot,
                         NULL);
}

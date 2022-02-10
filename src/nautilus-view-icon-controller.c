#include "nautilus-view-icon-controller.h"
#include "nautilus-view-item-model.h"
#include "nautilus-view-icon-item-ui.h"
#include "nautilus-view-model.h"
#include "nautilus-files-view.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include "nautilus-window-slot.h"
#include "nautilus-directory.h"
#include "nautilus-clipboard.h"
#include "nautilus-global-preferences.h"
#include "nautilus-thumbnails.h"

struct _NautilusViewIconController
{
    NautilusFilesView parent_instance;

    GtkGridView *view_ui;
    NautilusViewModel *model;

    GList *cut_files;

    GIcon *view_icon;
    GActionGroup *action_group;
    gint zoom_level;
    GQuark caption_attributes[NAUTILUS_VIEW_ICON_N_CAPTIONS];

    gboolean single_click_mode;
    gboolean activate_on_release;

    guint scroll_to_file_handle_id;
    guint prioritize_thumbnailing_handle_id;
    GtkAdjustment *vadjustment;
};

G_DEFINE_TYPE (NautilusViewIconController, nautilus_view_icon_controller, NAUTILUS_TYPE_FILES_VIEW)

typedef struct
{
    const NautilusFileSortType sort_type;
    const gchar *metadata_name;
    const gchar *action_target_name;
    gboolean reversed;
} SortConstants;

static const SortConstants sorts_constants[] =
{
    {
        NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
        "name",
        "name",
        FALSE,
    },
    {
        NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
        "name",
        "name-desc",
        TRUE,
    },
    {
        NAUTILUS_FILE_SORT_BY_SIZE,
        "size",
        "size",
        TRUE,
    },
    {
        NAUTILUS_FILE_SORT_BY_TYPE,
        "type",
        "type",
        FALSE,
    },
    {
        NAUTILUS_FILE_SORT_BY_MTIME,
        "modification date",
        "modification-date",
        FALSE,
    },
    {
        NAUTILUS_FILE_SORT_BY_MTIME,
        "modification date",
        "modification-date-desc",
        TRUE,
    },
    {
        NAUTILUS_FILE_SORT_BY_ATIME,
        "access date",
        "access-date",
        FALSE,
    },
    {
        NAUTILUS_FILE_SORT_BY_ATIME,
        "access date",
        "access-date-desc",
        TRUE,
    },
    {
        NAUTILUS_FILE_SORT_BY_BTIME,
        "creation date",
        "creation-date",
        FALSE,
    },
    {
        NAUTILUS_FILE_SORT_BY_BTIME,
        "creation date",
        "creation-date-desc",
        TRUE,
    },
    {
        NAUTILUS_FILE_SORT_BY_TRASHED_TIME,
        "trashed",
        "trash-time",
        TRUE,
    },
    {
        NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE,
        "search_relevance",
        "search-relevance",
        TRUE,
    }
};

static guint get_icon_size_for_zoom_level (NautilusGridZoomLevel zoom_level);

static const SortConstants *
get_sorts_constants_from_action_target_name (const gchar *action_target_name)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (g_strcmp0 (sorts_constants[i].action_target_name, action_target_name) == 0)
        {
            return &sorts_constants[i];
        }
    }

    return &sorts_constants[0];
}

static const SortConstants *
get_sorts_constants_from_sort_type (NautilusFileSortType sort_type,
                                    gboolean             reversed)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (sort_type == sorts_constants[i].sort_type
            && reversed == sorts_constants[i].reversed)
        {
            return &sorts_constants[i];
        }
    }

    return &sorts_constants[0];
}

static const SortConstants *
get_sorts_constants_from_metadata_text (const char *metadata_name,
                                        gboolean    reversed)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (sorts_constants); i++)
    {
        if (g_strcmp0 (sorts_constants[i].metadata_name, metadata_name) == 0
            && reversed == sorts_constants[i].reversed)
        {
            return &sorts_constants[i];
        }
    }

    return &sorts_constants[0];
}

static const SortConstants *
get_default_sort_order (NautilusFile *file)
{
    NautilusFileSortType sort_type;
    gboolean reversed;

    sort_type = nautilus_file_get_default_sort_type (file, &reversed);

    return get_sorts_constants_from_sort_type (sort_type, reversed);
}

static const SortConstants *
get_directory_sort_by (NautilusFile *file)
{
    const SortConstants *default_sort;
    g_autofree char *sort_by = NULL;
    gboolean reversed;

    default_sort = get_default_sort_order (file);
    g_return_val_if_fail (default_sort != NULL, NULL);

    sort_by = nautilus_file_get_metadata (file,
                                          NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
                                          default_sort->metadata_name);

    reversed = nautilus_file_get_boolean_metadata (file,
                                                   NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                                   default_sort->reversed);

    return get_sorts_constants_from_metadata_text (sort_by, reversed);
}

static void
set_directory_sort_metadata (NautilusFile        *file,
                             const SortConstants *sort)
{
    const SortConstants *default_sort;

    default_sort = get_default_sort_order (file);

    nautilus_file_set_metadata (file,
                                NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
                                default_sort->metadata_name,
                                sort->metadata_name);
    nautilus_file_set_boolean_metadata (file,
                                        NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                        default_sort->reversed,
                                        sort->reversed);
}

static void
update_sort_order_from_metadata_and_preferences (NautilusViewIconController *self)
{
    const SortConstants *default_directory_sort;
    GActionGroup *view_action_group;

    default_directory_sort = get_directory_sort_by (nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self)));
    view_action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_group_change_action_state (view_action_group,
                                        "sort",
                                        g_variant_new_string (get_sorts_constants_from_sort_type (default_directory_sort->sort_type, default_directory_sort->reversed)->action_target_name));
}

static void
real_begin_loading (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    /* TODO: This calls sort once, and update_context_menus calls update_actions which calls
     * the action again
     */
    update_sort_order_from_metadata_and_preferences (self);

    /*TODO move this to the files view class begin_loading and hook up? */

    /* We could have changed to the trash directory or to searching, and then
     * we need to update the menus */
    nautilus_files_view_update_context_menus (files_view);
    nautilus_files_view_update_toolbar_menus (files_view);
}

static void
real_clear (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    nautilus_view_model_remove_all_items (self->model);
}

static void
real_file_changed (NautilusFilesView *files_view,
                   NautilusFile      *file,
                   NautilusDirectory *directory)
{
    NautilusViewIconController *self;
    NautilusViewItemModel *item_model;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    item_model = nautilus_view_model_get_item_from_file (self->model, file);
    nautilus_view_item_model_file_changed (item_model);
}

static GList *
real_get_selection (NautilusFilesView *files_view)
{
    NautilusViewIconController *self;
    g_autoptr (GtkSelectionFilterModel) selection = NULL;
    guint n_selected;
    GList *selected_files = NULL;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    selection = gtk_selection_filter_model_new (GTK_SELECTION_MODEL (self->model));
    n_selected = g_list_model_get_n_items (G_LIST_MODEL (selection));
    for (guint i = 0; i < n_selected; i++)
    {
        NautilusViewItemModel *item_model;

        item_model = g_list_model_get_item (G_LIST_MODEL (selection), i);
        selected_files = g_list_prepend (selected_files,
                                         g_object_ref (nautilus_view_item_model_get_file (item_model)));
    }

    return selected_files;
}

static gboolean
real_is_empty (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return g_list_model_get_n_items (G_LIST_MODEL (self->model)) == 0;
}

static void
real_end_file_changes (NautilusFilesView *files_view)
{
}

static void
real_remove_file (NautilusFilesView *files_view,
                  NautilusFile      *file,
                  NautilusDirectory *directory)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    NautilusViewItemModel *item_model;

    item_model = nautilus_view_model_get_item_from_file (self->model, file);
    if (item_model != NULL)
    {
        nautilus_view_model_remove_item (self->model, item_model);
    }
}

static GQueue *
convert_glist_to_queue (GList *list)
{
    GList *l;
    GQueue *queue;

    queue = g_queue_new ();
    for (l = list; l != NULL; l = l->next)
    {
        g_queue_push_tail (queue, l->data);
    }

    return queue;
}

static GQueue *
convert_files_to_item_models (NautilusViewIconController *self,
                              GQueue                     *files)
{
    GList *l;
    GQueue *models;

    models = g_queue_new ();
    for (l = g_queue_peek_head_link (files); l != NULL; l = l->next)
    {
        NautilusViewItemModel *item_model;

        item_model = nautilus_view_item_model_new (NAUTILUS_FILE (l->data),
                                                   get_icon_size_for_zoom_level (self->zoom_level));
        g_queue_push_tail (models, item_model);
    }

    return models;
}

static void
real_set_selection (NautilusFilesView *files_view,
                    GList             *selection)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    g_autoptr (GQueue) selection_files = NULL;
    g_autoptr (GQueue) selection_item_models = NULL;
    g_autoptr (GtkBitset) update_set = NULL;
    g_autoptr (GtkBitset) selection_set = NULL;

    update_set = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (self->model));
    selection_set = gtk_bitset_new_empty ();

    /* Convert file list into set of model indices */
    selection_files = convert_glist_to_queue (selection);
    selection_item_models = nautilus_view_model_get_items_from_files (self->model, selection_files);
    for (GList *l = g_queue_peek_head_link (selection_item_models); l != NULL ; l = l->next)
    {
        gtk_bitset_add (selection_set,
                        nautilus_view_model_get_index (self->model, l->data));
    }

    gtk_bitset_union (update_set, selection_set);
    gtk_selection_model_set_selection (GTK_SELECTION_MODEL (self->model),
                                       selection_set,
                                       update_set);
}

static void
real_select_all (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    gtk_selection_model_select_all (GTK_SELECTION_MODEL (self->model));
}

static void
real_invert_selection (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (self->model);
    g_autoptr (GtkBitset) selected = NULL;
    g_autoptr (GtkBitset) all = NULL;
    g_autoptr (GtkBitset) new_selected = NULL;

    selected = gtk_selection_model_get_selection (selection_model);

    /* We are going to flip the selection state of every item in the model. */
    all = gtk_bitset_new_range (0, g_list_model_get_n_items (G_LIST_MODEL (self->model)));

    /* The new selection is all items minus the ones currently selected. */
    new_selected = gtk_bitset_copy (all);
    gtk_bitset_subtract (new_selected, selected);

    gtk_selection_model_set_selection (selection_model, new_selected, all);
}

static guint
get_first_selected_item (NautilusViewIconController *self)
{
    g_autolist (NautilusFile) selection = NULL;
    NautilusFile *file;
    NautilusViewItemModel *item_model;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    if (selection == NULL)
    {
        return G_MAXUINT;
    }

    file = NAUTILUS_FILE (selection->data);
    item_model = nautilus_view_model_get_item_from_file (self->model, file);

    return nautilus_view_model_get_index (self->model, item_model);
}

static void
real_reveal_selection (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    gtk_widget_activate_action (GTK_WIDGET (self->view_ui),
                                "list.scroll-to-item",
                                "u",
                                get_first_selected_item (self));
}

static gboolean
showing_recent_directory (NautilusFilesView *view)
{
    NautilusFile *file;

    file = nautilus_files_view_get_directory_as_file (view);
    if (file != NULL)
    {
        return nautilus_file_is_in_recent (file);
    }
    return FALSE;
}

static gboolean
showing_search_directory (NautilusFilesView *view)
{
    NautilusFile *file;

    file = nautilus_files_view_get_directory_as_file (view);
    if (file != NULL)
    {
        return nautilus_file_is_in_search (file);
    }
    return FALSE;
}

static void
real_update_actions_state (NautilusFilesView *files_view)
{
    GAction *action;
    GActionGroup *view_action_group;

    NAUTILUS_FILES_VIEW_CLASS (nautilus_view_icon_controller_parent_class)->update_actions_state (files_view);

    view_action_group = nautilus_files_view_get_action_group (files_view);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "sort");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                 !showing_recent_directory (files_view) &&
                                 !showing_search_directory (files_view));
}

static void
real_bump_zoom_level (NautilusFilesView *files_view,
                      int                zoom_increment)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    NautilusGridZoomLevel new_level;

    new_level = self->zoom_level + zoom_increment;

    if (new_level >= NAUTILUS_GRID_ZOOM_LEVEL_SMALL &&
        new_level <= NAUTILUS_GRID_ZOOM_LEVEL_LARGEST)
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

        case NAUTILUS_GRID_ZOOM_LEVEL_STANDARD:
        {
            return NAUTILUS_GRID_ICON_SIZE_STANDARD;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_LARGE:
        {
            return NAUTILUS_GRID_ICON_SIZE_LARGE;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_LARGER:
        {
            return NAUTILUS_GRID_ICON_SIZE_LARGER;
        }
        break;

        case NAUTILUS_GRID_ZOOM_LEVEL_LARGEST:
        {
            return NAUTILUS_GRID_ICON_SIZE_LARGEST;
        }
        break;
    }
    g_return_val_if_reached (NAUTILUS_GRID_ICON_SIZE_STANDARD);
}

static gint
get_default_zoom_level (void)
{
    NautilusGridZoomLevel default_zoom_level;

    default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
                                              NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

    return default_zoom_level;
}

static void
set_captions_from_preferences (NautilusViewIconController *self)
{
    g_auto (GStrv) value = NULL;
    gint n_captions_for_zoom_level;

    value = g_settings_get_strv (nautilus_icon_view_preferences,
                                 NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

    /* Set a celling on the number of captions depending on the zoom level. */
    n_captions_for_zoom_level = MIN (self->zoom_level,
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
set_icon_size (NautilusViewIconController *self,
               gint                        icon_size)
{
    NautilusViewItemModel *current_item_model;
    guint i = 0;

    while ((current_item_model = NAUTILUS_VIEW_ITEM_MODEL (g_list_model_get_item (G_LIST_MODEL (self->model), i))))
    {
        nautilus_view_item_model_set_icon_size (current_item_model,
                                                get_icon_size_for_zoom_level (self->zoom_level));
        i++;
    }
}

static void
set_zoom_level (NautilusViewIconController *self,
                guint                       new_level)
{
    self->zoom_level = new_level;

    /* The zoom level may change how many captions are allowed. Update it before
     * setting the icon size, under the assumption that NautilusViewIconItemUi
     * updates captions whenever the icon size is set*/
    set_captions_from_preferences (self);

    set_icon_size (self, get_icon_size_for_zoom_level (new_level));

    nautilus_files_view_update_toolbar_menus (NAUTILUS_FILES_VIEW (self));
}

static void
real_restore_standard_zoom_level (NautilusFilesView *files_view)
{
    NautilusViewIconController *self;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    g_action_group_change_action_state (self->action_group,
                                        "zoom-to-level",
                                        g_variant_new_int32 (NAUTILUS_GRID_ZOOM_LEVEL_LARGE));
}

static gfloat
real_get_zoom_level_percentage (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return (gfloat) get_icon_size_for_zoom_level (self->zoom_level) /
           NAUTILUS_GRID_ICON_SIZE_LARGE;
}

static gboolean
real_is_zoom_level_default (NautilusFilesView *files_view)
{
    NautilusViewIconController *self;
    guint icon_size;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    icon_size = get_icon_size_for_zoom_level (self->zoom_level);

    return icon_size == NAUTILUS_GRID_ICON_SIZE_LARGE;
}

static gboolean
real_can_zoom_in (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return self->zoom_level < NAUTILUS_GRID_ZOOM_LEVEL_LARGEST;
}

static gboolean
real_can_zoom_out (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return self->zoom_level > NAUTILUS_GRID_ZOOM_LEVEL_SMALL;
}

static GdkRectangle *
get_rectangle_for_item_ui (NautilusViewIconController *self,
                           GtkWidget                  *item_ui)
{
    GdkRectangle *rectangle;
    GtkWidget *content_widget;
    gdouble view_x;
    gdouble view_y;

    rectangle = g_new0 (GdkRectangle, 1);
    gtk_widget_get_allocation (item_ui, rectangle);

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    gtk_widget_translate_coordinates (item_ui, content_widget,
                                      rectangle->x, rectangle->y,
                                      &view_x, &view_y);
    rectangle->x = view_x;
    rectangle->y = view_y;

    return rectangle;
}

static GdkRectangle *
real_compute_rename_popover_pointing_to (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    NautilusViewItemModel *item;
    GtkWidget *item_ui;

    /* We only allow one item to be renamed with a popover */
    item = g_list_model_get_item (G_LIST_MODEL (self->model),
                                  get_first_selected_item (self));
    item_ui = nautilus_view_item_model_get_item_ui (item);
    g_return_val_if_fail (item_ui != NULL, NULL);

    return get_rectangle_for_item_ui (self, item_ui);
}

static GdkRectangle *
real_reveal_for_selection_context_menu (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    g_autoptr (GtkSelectionFilterModel) selection = NULL;
    guint n_selected;
    GtkWidget *focus_child;
    guint i;
    NautilusViewItemModel *item;
    GtkWidget *item_ui;

    selection = gtk_selection_filter_model_new (GTK_SELECTION_MODEL (self->model));
    n_selected = g_list_model_get_n_items (G_LIST_MODEL (selection));
    g_return_val_if_fail (n_selected > 0, NULL);

    /* Get the focused item_ui, if selected.
     * Otherwise, get the selected item_ui which is sorted the lowest.*/
    focus_child = gtk_widget_get_focus_child (GTK_WIDGET (self->view_ui));
    for (i = 0; i < n_selected; i++)
    {
        item = g_list_model_get_item (G_LIST_MODEL (selection), i);
        item_ui = nautilus_view_item_model_get_item_ui (item);
        if (item_ui != NULL && gtk_widget_get_parent (item_ui) == focus_child)
        {
            break;
        }
    }

    gtk_widget_activate_action (GTK_WIDGET (self->view_ui),
                                "list.scroll-to-item",
                                "u",
                                i);

    return get_rectangle_for_item_ui (self, item_ui);
}

static void
set_click_mode_from_settings (NautilusViewIconController *self)
{
    int click_policy;

    click_policy = g_settings_get_enum (nautilus_preferences,
                                        NAUTILUS_PREFERENCES_CLICK_POLICY);

    self->single_click_mode = (click_policy == NAUTILUS_CLICK_POLICY_SINGLE);
}

static void
real_click_policy_changed (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    set_click_mode_from_settings (self);
}

static void
activate_selection_on_click (NautilusViewIconController *self,
                             gboolean                    open_in_new_tab)
{
    g_autolist (NautilusFile) selection = NULL;
    NautilusOpenFlags flags = 0;
    NautilusFilesView *files_view = NAUTILUS_FILES_VIEW (self);

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    if (open_in_new_tab)
    {
        flags |= NAUTILUS_OPEN_FLAG_NEW_TAB;
        flags |= NAUTILUS_OPEN_FLAG_DONT_MAKE_ACTIVE;
    }
    nautilus_files_view_activate_files (files_view, selection, flags, TRUE);
}

static void
on_click_pressed (GtkGestureClick *gesture,
                  gint             n_press,
                  gdouble          x,
                  gdouble          y,
                  gpointer         user_data)
{
    NautilusViewIconController *self;
    GtkWidget *event_widget;
    guint button;
    GdkModifierType modifiers;
    gboolean selection_mode;
    gdouble view_x;
    gdouble view_y;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    event_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    modifiers = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

    selection_mode = (modifiers & (GDK_CONTROL_MASK | GDK_SHIFT_MASK));

    gtk_widget_translate_coordinates (event_widget, GTK_WIDGET (self),
                                      x, y,
                                      &view_x, &view_y);
    if (NAUTILUS_IS_VIEW_ICON_ITEM_UI (event_widget))
    {
        self->activate_on_release = (self->single_click_mode &&
                                     button == GDK_BUTTON_PRIMARY &&
                                     n_press == 1 &&
                                     !selection_mode);

        /* GtkGridView changes selection only with the primary button, but we
         * need that to happen with all buttons, otherwise e.g. opening context
         * menus would require two clicks: a primary click to select the item,
         * followed by a secondary click to open the menu.
         * When holding Ctrl and Shift, GtkGridView does a good job, let's not
         * interfere in that case. */
        if (!selection_mode)
        {
            GtkSelectionModel *selection_model = GTK_SELECTION_MODEL (self->model);
            NautilusViewItemModel *item_model;
            guint position;

            item_model = nautilus_view_icon_item_ui_get_model (NAUTILUS_VIEW_ICON_ITEM_UI (event_widget));
            position = nautilus_view_model_get_index (self->model, item_model);
            if (!gtk_selection_model_is_selected (selection_model, position))
            {
                gtk_selection_model_select_item (selection_model, position, TRUE);
            }
        }

        if (button == GDK_BUTTON_PRIMARY && n_press == 2)
        {
            activate_selection_on_click (self, modifiers & GDK_SHIFT_MASK);
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
            self->activate_on_release = FALSE;
        }
        else if (button == GDK_BUTTON_MIDDLE && n_press == 1 && !selection_mode)
        {
            activate_selection_on_click (self, TRUE);
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        }
        else if (button == GDK_BUTTON_SECONDARY)
        {
            nautilus_files_view_pop_up_selection_context_menu (NAUTILUS_FILES_VIEW (self),
                                                               view_x, view_y);
            gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        }
    }
    else
    {
        /* Don't interfere with GtkGridView default selection handling when
         * holding Ctrl and Shift. */
        if (!selection_mode && !self->activate_on_release)
        {
            nautilus_view_set_selection (NAUTILUS_VIEW (self), NULL);
        }

        if (button == GDK_BUTTON_SECONDARY)
        {
            nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (self),
                                                                view_x, view_y);
        }
    }
}

static void
on_click_released (GtkGestureClick *gesture,
                   gint             n_press,
                   gdouble          x,
                   gdouble          y,
                   gpointer         user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);

    if (self->activate_on_release)
    {
        activate_selection_on_click (self, FALSE);
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
    self->activate_on_release = FALSE;
}

static void
on_click_stopped (GtkGestureClick *gesture,
                  gpointer         user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);

    self->activate_on_release = FALSE;
}

static void
on_longpress_gesture_pressed_callback (GtkGestureLongPress *gesture,
                                       gdouble              x,
                                       gdouble              y,
                                       gpointer             user_data)
{
    NautilusViewIconController *self;
    GtkWidget *event_widget;
    gdouble view_x;
    gdouble view_y;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    event_widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

    gtk_widget_translate_coordinates (event_widget,
                                      GTK_WIDGET (self),
                                      x, y, &view_x, &view_y);
    if (NAUTILUS_IS_VIEW_ICON_ITEM_UI (event_widget))
    {
        nautilus_files_view_pop_up_selection_context_menu (NAUTILUS_FILES_VIEW (self),
                                                           view_x, view_y);
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
    else
    {
        nautilus_view_set_selection (NAUTILUS_VIEW (self), NULL);
        nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (self),
                                                            view_x, view_y);
    }
}

static int
real_compare_files (NautilusFilesView *files_view,
                    NautilusFile      *file1,
                    NautilusFile      *file2)
{
    GActionGroup *view_action_group;
    GAction *action;
    const gchar *target_name;
    const SortConstants *sort_constants;
    gboolean directories_first;

    view_action_group = nautilus_files_view_get_action_group (files_view);
    action = g_action_map_lookup_action (G_ACTION_MAP (view_action_group), "sort");
    target_name = g_variant_get_string (g_action_get_state (action), NULL);
    sort_constants = get_sorts_constants_from_action_target_name (target_name);
    directories_first = nautilus_files_view_should_sort_directories_first (files_view);

    return nautilus_file_compare_for_sort (file1, file2,
                                           sort_constants->sort_type,
                                           directories_first,
                                           sort_constants->reversed);
}

static void
on_clipboard_contents_received (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
    NautilusFilesView *files_view = NAUTILUS_FILES_VIEW (source_object);
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    NautilusClipboard *clip;
    NautilusViewItemModel *item;

    for (GList *l = self->cut_files; l != NULL; l = l->next)
    {
        item = nautilus_view_model_get_item_from_file (self->model, l->data);
        if (item != NULL)
        {
            nautilus_view_item_model_set_cut (item, FALSE);
        }
    }
    g_clear_list (&self->cut_files, g_object_unref);

    clip = nautilus_files_view_get_clipboard_finish (files_view, res, NULL);
    if (clip != NULL && nautilus_clipboard_is_cut (clip))
    {
        self->cut_files = g_list_copy_deep (nautilus_clipboard_peek_files (clip),
                                            (GCopyFunc) g_object_ref,
                                            NULL);
    }

    for (GList *l = self->cut_files; l != NULL; l = l->next)
    {
        item = nautilus_view_model_get_item_from_file (self->model, l->data);
        if (item != NULL)
        {
            nautilus_view_item_model_set_cut (item, TRUE);
        }
    }
}

static void
update_clipboard_status (NautilusFilesView *files_view)
{
    nautilus_files_view_get_clipboard_async (files_view,
                                             on_clipboard_contents_received,
                                             NULL);
}

static void
on_clipboard_owner_changed (GdkClipboard *clipboard,
                            gpointer      user_data)
{
    update_clipboard_status (NAUTILUS_FILES_VIEW (user_data));
}


static void
real_end_loading (NautilusFilesView *files_view,
                  gboolean           all_files_seen)
{
    update_clipboard_status (files_view);
}

static guint
get_first_visible_item (NautilusViewIconController *self)
{
    guint n_items;
    gdouble scrolled_y;

    n_items = g_list_model_get_n_items (G_LIST_MODEL (self->model));
    scrolled_y = gtk_adjustment_get_value (self->vadjustment);
    for (guint i = 0; i < n_items; i++)
    {
        NautilusViewItemModel *item;
        GtkWidget *item_ui;

        item = g_list_model_get_item (G_LIST_MODEL (self->model), i);
        item_ui = nautilus_view_item_model_get_item_ui (item);
        if (item_ui != NULL)
        {
            gdouble y;

            gtk_widget_translate_coordinates (item_ui, GTK_WIDGET (self->view_ui),
                                              0, 0, NULL, &y);
            if (gtk_widget_is_visible (item_ui) && y >= scrolled_y)
            {
                return i;
            }
        }
    }

    return G_MAXUINT;
}

static char *
real_get_first_visible_file (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    guint i;
    NautilusViewItemModel *item;
    gchar *uri = NULL;

    i = get_first_visible_item (self);
    if (i < G_MAXUINT)
    {
        item = g_list_model_get_item (G_LIST_MODEL (self->model), i);
        uri = nautilus_file_get_uri (nautilus_view_item_model_get_file (item));
    }
    return uri;
}

typedef struct
{
    NautilusViewIconController *view;
    char *uri;
} ScrollToFileData;

static void
scroll_to_file_data_free (ScrollToFileData *data)
{
    g_free (data->uri);
    g_free (data);
}

static gboolean
scroll_to_file_on_idle (ScrollToFileData *data)
{
    NautilusViewIconController *self = data->view;
    g_autoptr (NautilusFile) file = NULL;
    NautilusViewItemModel *item;
    guint i;

    file = nautilus_file_get_existing_by_uri (data->uri);
    item = nautilus_view_model_get_item_from_file (self->model, file);
    i = nautilus_view_model_get_index (self->model, item);

    gtk_widget_activate_action (GTK_WIDGET (self->view_ui),
                                "list.scroll-to-item",
                                "u",
                                i);

    self->scroll_to_file_handle_id = 0;
    return G_SOURCE_REMOVE;
}

static void
real_scroll_to_file (NautilusFilesView *files_view,
                     const char        *uri)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    ScrollToFileData *data;
    guint handle_id;

    /* Not exactly sure why, but the child widgets are not yet realized when
     * this is usually called (which is when view finishes loading. Maybe
     * because GtkFlowBox only generates children at the next GMainContext
     * iteration? Anyway, doing it on idle as low priority works. */

    data = g_new (ScrollToFileData, 1);
    data->view = self;
    data->uri = g_strdup (uri);
    handle_id = g_idle_add_full (G_PRIORITY_LOW,
                                 (GSourceFunc) scroll_to_file_on_idle,
                                 data,
                                 (GDestroyNotify) scroll_to_file_data_free);
    self->scroll_to_file_handle_id = handle_id;
}

static void
real_sort_directories_first_changed (NautilusFilesView *files_view)
{
    NautilusViewModelSortData sort_data;
    NautilusViewModelSortData *current_sort_data;
    NautilusViewIconController *self;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    current_sort_data = nautilus_view_model_get_sort_type (self->model);
    sort_data.sort_type = current_sort_data->sort_type;
    sort_data.reversed = current_sort_data->reversed;
    sort_data.directories_first = nautilus_files_view_should_sort_directories_first (NAUTILUS_FILES_VIEW (self));

    nautilus_view_model_set_sort_type (self->model, &sort_data);
}

static void
action_sort_order_changed (GSimpleAction *action,
                           GVariant      *value,
                           gpointer       user_data)
{
    const gchar *target_name;
    const SortConstants *sort_constants;
    NautilusViewModelSortData sort_data;
    NautilusViewIconController *self;

    /* Don't resort if the action is in the same state as before */
    if (g_strcmp0 (g_variant_get_string (value, NULL), g_variant_get_string (g_action_get_state (G_ACTION (action)), NULL)) == 0)
    {
        return;
    }

    self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    target_name = g_variant_get_string (value, NULL);
    sort_constants = get_sorts_constants_from_action_target_name (target_name);
    sort_data.sort_type = sort_constants->sort_type;
    sort_data.reversed = sort_constants->reversed;
    sort_data.directories_first = nautilus_files_view_should_sort_directories_first (NAUTILUS_FILES_VIEW (self));

    nautilus_view_model_set_sort_type (self->model, &sort_data);
    set_directory_sort_metadata (nautilus_files_view_get_directory_as_file (NAUTILUS_FILES_VIEW (self)),
                                 sort_constants);

    g_simple_action_set_state (action, value);
}

static void
real_add_files (NautilusFilesView *files_view,
                GList             *files)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    g_autoptr (GQueue) files_queue = NULL;
    g_autoptr (GQueue) item_models = NULL;
    gdouble adjustment_value;

    files_queue = convert_glist_to_queue (files);
    item_models = convert_files_to_item_models (self, files_queue);
    nautilus_view_model_add_items (self->model, item_models);

    /* GtkListBase anchoring doesn't cope well with our lazy loading.
     * Assuming that GtkListBase|list.scroll-to-item resets the anchor to 0, use
     * that as a workaround to prevent scrolling while we are at the top. */
    adjustment_value = gtk_adjustment_get_value (self->vadjustment);
    if (G_APPROX_VALUE (adjustment_value, 0.0, DBL_EPSILON))
    {
        gtk_widget_activate_action (GTK_WIDGET (self->view_ui), "list.scroll-to-item", "u", 0);
    }
}


static guint
real_get_view_id (NautilusFilesView *files_view)
{
    return NAUTILUS_VIEW_GRID_ID;
}

static GIcon *
real_get_icon (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return self->view_icon;
}

static void
real_select_first (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    NautilusViewItemModel *item;
    NautilusFile *file;
    g_autoptr (GList) selection = NULL;

    item = NAUTILUS_VIEW_ITEM_MODEL (g_list_model_get_item (G_LIST_MODEL (self->model), 0));
    if (item == NULL)
    {
        return;
    }
    file = nautilus_view_item_model_get_file (item);
    selection = g_list_prepend (selection, file);
    nautilus_view_set_selection (NAUTILUS_VIEW (files_view), selection);
}

static void
real_preview_selection_event (NautilusFilesView *files_view,
                              GtkDirectionType   direction)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    GtkMovementStep step;
    gint count;
    gboolean handled;

    step = (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN) ?
           GTK_MOVEMENT_DISPLAY_LINES : GTK_MOVEMENT_VISUAL_POSITIONS;
    count = (direction == GTK_DIR_RIGHT || direction == GTK_DIR_DOWN) ?
            1 : -1;

    g_signal_emit_by_name (self->view_ui, "move-cursor", step, count, &handled);
}

static void
action_zoom_to_level (GSimpleAction *action,
                      GVariant      *state,
                      gpointer       user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
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
on_captions_preferences_changed (NautilusViewIconController *self)
{
    set_captions_from_preferences (self);

    /* Hack: this relies on the assumption that NautilusViewIconItemUi updates
     * captions whenever the icon size is set (even if it's the same value). */
    set_icon_size (self, get_icon_size_for_zoom_level (self->zoom_level));
}

static void
on_default_sort_order_changed (NautilusViewIconController *self)
{
    update_sort_order_from_metadata_and_preferences (self);
}

static void
dispose (GObject *object)
{
    NautilusViewIconController *self;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (object);

    g_clear_handle_id (&self->scroll_to_file_handle_id, g_source_remove);
    g_clear_handle_id (&self->prioritize_thumbnailing_handle_id, g_source_remove);

    g_signal_handlers_disconnect_by_data (nautilus_preferences, self);

    G_OBJECT_CLASS (nautilus_view_icon_controller_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (object);

    g_clear_list (&self->cut_files, g_object_unref);

    G_OBJECT_CLASS (nautilus_view_icon_controller_parent_class)->finalize (object);
}

static void
prioritize_thumbnailing_on_idle (NautilusViewIconController *self)
{
    gdouble page_size;
    GtkWidget *first_visible_child;
    GtkWidget *next_child;
    guint first_index;
    guint next_index;
    gdouble y;
    guint last_index;
    NautilusViewItemModel *item;
    NautilusFile *file;

    self->prioritize_thumbnailing_handle_id = 0;

    page_size = gtk_adjustment_get_page_size (self->vadjustment);
    first_index = get_first_visible_item (self);
    if (first_index == G_MAXUINT)
    {
        return;
    }

    item = g_list_model_get_item (G_LIST_MODEL (self->model), first_index);

    first_visible_child = nautilus_view_item_model_get_item_ui (item);

    for (next_index = first_index + 1; next_index < g_list_model_get_n_items (G_LIST_MODEL (self->model)); next_index++)
    {
        item = g_list_model_get_item (G_LIST_MODEL (self->model), next_index);

        next_child = nautilus_view_item_model_get_item_ui (item);
        if (next_child == NULL)
        {
            break;
        }
        if (gtk_widget_translate_coordinates (next_child, first_visible_child,
                                              0, 0, NULL, &y))
        {
            if (y > page_size)
            {
                break;
            }
        }
    }
    last_index = next_index - 1;

    /* Do the iteration in reverse to give higher priority to the top */
    for (gint i = 0; i <= last_index - first_index; i++)
    {
        item = g_list_model_get_item (G_LIST_MODEL (self->model), last_index - i);
        g_return_if_fail (item != NULL);

        file = nautilus_view_item_model_get_file (NAUTILUS_VIEW_ITEM_MODEL (item));
        if (file != NULL && nautilus_file_is_thumbnailing (file))
        {
            g_autofree gchar *uri = nautilus_file_get_uri (file);
            nautilus_thumbnail_prioritize (uri);
        }
    }
}

static void
on_vadjustment_changed (GtkAdjustment *adjustment,
                        gpointer       user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    guint handle_id;

    /* Schedule on idle to rate limit and to avoid delaying scrolling. */
    if (self->prioritize_thumbnailing_handle_id == 0)
    {
        handle_id = g_idle_add ((GSourceFunc) prioritize_thumbnailing_on_idle, self);
        self->prioritize_thumbnailing_handle_id = handle_id;
    }
}

static void
bind_item_ui (GtkSignalListItemFactory *factory,
              GtkListItem              *listitem,
              gpointer                  user_data)
{
    GtkWidget *item_ui;
    NautilusViewItemModel *item_model;

    item_ui = gtk_list_item_get_child (listitem);
    item_model = NAUTILUS_VIEW_ITEM_MODEL (gtk_list_item_get_item (listitem));

    nautilus_view_icon_item_ui_set_model (NAUTILUS_VIEW_ICON_ITEM_UI (item_ui),
                                          item_model);
    nautilus_view_item_model_set_item_ui (item_model, item_ui);

    if (nautilus_view_icon_item_ui_once (NAUTILUS_VIEW_ICON_ITEM_UI (item_ui)))
    {
        GtkWidget *parent;

        /* At the time of ::setup emission, the item ui has got no parent yet,
         * that's why we need to complete the widget setup process here, on the
         * first time ::bind is emitted. */
        parent = gtk_widget_get_parent (item_ui);
        gtk_widget_set_halign (parent, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (parent, GTK_ALIGN_START);
        gtk_widget_set_margin_top (parent, 3);
        gtk_widget_set_margin_bottom (parent, 3);
        gtk_widget_set_margin_start (parent, 3);
        gtk_widget_set_margin_end (parent, 3);
    }
}

static void
unbind_item_ui (GtkSignalListItemFactory *factory,
                GtkListItem              *listitem,
                gpointer                  user_data)
{
    NautilusViewIconItemUi *item_ui;
    NautilusViewItemModel *item_model;

    item_ui = NAUTILUS_VIEW_ICON_ITEM_UI (gtk_list_item_get_child (listitem));
    item_model = NAUTILUS_VIEW_ITEM_MODEL (gtk_list_item_get_item (listitem));

    nautilus_view_icon_item_ui_set_model (item_ui, NULL);
    nautilus_view_item_model_set_item_ui (item_model, NULL);
}

static void
setup_item_ui (GtkSignalListItemFactory *factory,
               GtkListItem              *listitem,
               gpointer                  user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    NautilusViewIconItemUi *item_ui;
    GtkEventController *controller;

    item_ui = nautilus_view_icon_item_ui_new ();
    nautilus_view_item_ui_set_caption_attributes (item_ui, self->caption_attributes);
    gtk_list_item_set_child (listitem, GTK_WIDGET (item_ui));

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (item_ui), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed", G_CALLBACK (on_click_pressed), self);
    g_signal_connect (controller, "stopped", G_CALLBACK (on_click_stopped), self);
    g_signal_connect (controller, "released", G_CALLBACK (on_click_released), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (GTK_WIDGET (item_ui), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed", G_CALLBACK (on_longpress_gesture_pressed_callback), self);
}

static GtkGridView *
create_view_ui (NautilusViewIconController *self)
{
    GtkListItemFactory *factory;
    GtkWidget *widget;

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (setup_item_ui), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_item_ui), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_item_ui), self);

    widget = gtk_grid_view_new (GTK_SELECTION_MODEL (self->model), factory);
    gtk_widget_set_focusable (widget, TRUE);
    gtk_widget_set_valign (widget, GTK_ALIGN_START);

    /* We don't use the built-in child activation feature because it doesn't
     * fill all our needs nor does it match our expected behavior. Instead, we
     * roll our own event handling and double/single click mode.
     * However, GtkGridView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_grid_view_set_single_click_activate (GTK_GRID_VIEW (widget), FALSE);
    gtk_grid_view_set_max_columns (GTK_GRID_VIEW (widget), 20);
    gtk_grid_view_set_enable_rubberband (GTK_GRID_VIEW (widget), TRUE);

    return GTK_GRID_VIEW (widget);
}

const GActionEntry view_icon_actions[] =
{
    { "sort", NULL, "s", "'invalid'", action_sort_order_changed },
    { "zoom-to-level", NULL, NULL, "100", action_zoom_to_level }
};

static void
constructed (GObject *object)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (object);
    GtkWidget *content_widget;
    GtkAdjustment *vadjustment;
    GActionGroup *view_action_group;
    GtkEventController *controller;

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (content_widget));

    self->vadjustment = vadjustment;
    g_signal_connect (vadjustment, "changed", (GCallback) on_vadjustment_changed, self);
    g_signal_connect (vadjustment, "value-changed", (GCallback) on_vadjustment_changed, self);

    self->model = nautilus_view_model_new ();

    self->view_ui = create_view_ui (self);
    gtk_widget_show (GTK_WIDGET (self->view_ui));

    g_signal_connect_swapped (GTK_SELECTION_MODEL (self->model),
                              "selection-changed",
                              G_CALLBACK (nautilus_files_view_notify_selection_changed),
                              NAUTILUS_FILES_VIEW (self));

    self->view_icon = g_themed_icon_new ("view-grid-symbolic");

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
    gtk_widget_add_controller (GTK_WIDGET (content_widget), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (controller), 0);
    g_signal_connect (controller, "pressed",
                      G_CALLBACK (on_click_pressed), self);
    g_signal_connect (controller, "stopped",
                      G_CALLBACK (on_click_stopped), self);
    g_signal_connect (controller, "released",
                      G_CALLBACK (on_click_released), self);

    controller = GTK_EVENT_CONTROLLER (gtk_gesture_long_press_new ());
    gtk_widget_add_controller (GTK_WIDGET (self->view_ui), controller);
    gtk_event_controller_set_propagation_phase (controller, GTK_PHASE_BUBBLE);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (controller), TRUE);
    g_signal_connect (controller, "pressed",
                      (GCallback) on_longpress_gesture_pressed_callback, self);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_widget),
                                   GTK_WIDGET (self->view_ui));

    self->action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     view_icon_actions,
                                     G_N_ELEMENTS (view_icon_actions),
                                     self);

    gtk_widget_show (GTK_WIDGET (self));

    view_action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_map_add_action_entries (G_ACTION_MAP (view_action_group),
                                     view_icon_actions,
                                     G_N_ELEMENTS (view_icon_actions),
                                     self);
    self->zoom_level = get_default_zoom_level ();
    /* Keep the action synced with the actual value, so the toolbar can poll it */
    g_action_group_change_action_state (nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self)),
                                        "zoom-to-level", g_variant_new_int32 (self->zoom_level));
}

static void
nautilus_view_icon_controller_class_init (NautilusViewIconControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFilesViewClass *files_view_class = NAUTILUS_FILES_VIEW_CLASS (klass);

    object_class->dispose = dispose;
    object_class->finalize = finalize;
    object_class->constructed = constructed;

    files_view_class->add_files = real_add_files;
    files_view_class->begin_loading = real_begin_loading;
    files_view_class->bump_zoom_level = real_bump_zoom_level;
    files_view_class->can_zoom_in = real_can_zoom_in;
    files_view_class->can_zoom_out = real_can_zoom_out;
    files_view_class->click_policy_changed = real_click_policy_changed;
    files_view_class->clear = real_clear;
    files_view_class->file_changed = real_file_changed;
    files_view_class->get_selection = real_get_selection;
    /* TODO: remove this get_selection_for_file_transfer, this doesn't even
     * take into account we could us the view for recursive search :/
     * CanvasView has the same issue. */
    files_view_class->get_selection_for_file_transfer = real_get_selection;
    files_view_class->is_empty = real_is_empty;
    files_view_class->remove_file = real_remove_file;
    files_view_class->update_actions_state = real_update_actions_state;
    files_view_class->reveal_selection = real_reveal_selection;
    files_view_class->select_all = real_select_all;
    files_view_class->invert_selection = real_invert_selection;
    files_view_class->set_selection = real_set_selection;
    files_view_class->compare_files = real_compare_files;
    files_view_class->sort_directories_first_changed = real_sort_directories_first_changed;
    files_view_class->end_file_changes = real_end_file_changes;
    files_view_class->end_loading = real_end_loading;
    files_view_class->get_view_id = real_get_view_id;
    files_view_class->get_first_visible_file = real_get_first_visible_file;
    files_view_class->scroll_to_file = real_scroll_to_file;
    files_view_class->get_icon = real_get_icon;
    files_view_class->select_first = real_select_first;
    files_view_class->restore_standard_zoom_level = real_restore_standard_zoom_level;
    files_view_class->get_zoom_level_percentage = real_get_zoom_level_percentage;
    files_view_class->is_zoom_level_default = real_is_zoom_level_default;
    files_view_class->compute_rename_popover_pointing_to = real_compute_rename_popover_pointing_to;
    files_view_class->reveal_for_selection_context_menu = real_reveal_for_selection_context_menu;
    files_view_class->preview_selection_event = real_preview_selection_event;
}

static void
nautilus_view_icon_controller_init (NautilusViewIconController *self)
{
    GdkClipboard *clipboard;

    gtk_widget_add_css_class (GTK_WIDGET (self), "view");
    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-grid-view");
    set_click_mode_from_settings (self);

    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
                              G_CALLBACK (on_default_sort_order_changed),
                              self);
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                              G_CALLBACK (on_default_sort_order_changed),
                              self);

    set_captions_from_preferences (self);
    g_signal_connect_swapped (nautilus_icon_view_preferences,
                              "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
                              G_CALLBACK (on_captions_preferences_changed),
                              self);

    clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
    g_signal_connect_object (clipboard, "changed",
                             G_CALLBACK (on_clipboard_owner_changed), self, 0);
}

NautilusViewIconController *
nautilus_view_icon_controller_new (NautilusWindowSlot *slot)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ICON_CONTROLLER,
                         "window-slot", slot,
                         NULL);
}

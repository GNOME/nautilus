#include "nautilus-view-icon-controller.h"
#include "nautilus-view-item-model.h"
#include "nautilus-view-icon-item-ui.h"
#include "nautilus-view-model.h"
#include "nautilus-files-view.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include "nautilus-window-slot.h"
#include "nautilus-directory.h"
#include "nautilus-global-preferences.h"
#include "nautilus-thumbnails.h"
#include "nautilus-gtk4-helpers.h"

struct _NautilusViewIconController
{
    NautilusFilesView parent_instance;

    GtkFlowBox *view_ui;
    NautilusViewModel *model;

    GIcon *view_icon;
    GActionGroup *action_group;
    gint zoom_level;
    GQuark caption_attributes[NAUTILUS_VIEW_ICON_N_CAPTIONS];

    gboolean single_click_mode;
    gboolean activate_on_release;
    GtkGesture *multi_press_gesture;

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
        NULL,
        "search-relevance",
        TRUE,
    }
};

static guint get_icon_size_for_zoom_level (NautilusCanvasZoomLevel zoom_level);

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
    GList *selected_files = NULL;
    GList *l;
    g_autoptr (GList) selected_items = NULL;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    selected_items = gtk_flow_box_get_selected_children (self->view_ui);
    for (l = selected_items; l != NULL; l = l->next)
    {
        NautilusViewItemModel *item_model;

        item_model = g_list_model_get_item (G_LIST_MODEL (self->model),
                                            gtk_flow_box_child_get_index (l->data));
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

    selection_files = convert_glist_to_queue (selection);
    selection_item_models = nautilus_view_model_get_items_from_files (self->model, selection_files);

    gtk_flow_box_unselect_all (self->view_ui);
    for (GList *l = g_queue_peek_head_link (selection_item_models); l != NULL ; l = l->next)
    {
        GtkWidget *item_ui;

        item_ui = nautilus_view_item_model_get_item_ui (NAUTILUS_VIEW_ITEM_MODEL (l->data));
        gtk_flow_box_select_child (self->view_ui, GTK_FLOW_BOX_CHILD (item_ui));
    }

    nautilus_files_view_notify_selection_changed (files_view);
}

static void
real_select_all (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    gtk_flow_box_select_all (self->view_ui);
}

static void
real_invert_selection (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    g_autoptr (GList) selected_children = NULL;

    selected_children = gtk_flow_box_get_selected_children (self->view_ui);

    /* First select all, then unselect the previously selected children. */
    gtk_flow_box_select_all (self->view_ui);
    for (GList *l = selected_children; l != NULL; l = l->next)
    {
        gtk_flow_box_unselect_child (self->view_ui, GTK_FLOW_BOX_CHILD (l->data));
    }
}

static GtkWidget *
get_first_selected_item_ui (NautilusViewIconController *self)
{
    g_autolist (NautilusFile) selection = NULL;
    NautilusFile *file;
    NautilusViewItemModel *item_model;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
    if (selection == NULL)
    {
        return NULL;
    }

    file = NAUTILUS_FILE (selection->data);
    item_model = nautilus_view_model_get_item_from_file (self->model, file);

    return nautilus_view_item_model_get_item_ui (item_model);
}

static void
reveal_item_ui (NautilusViewIconController *self,
                GtkWidget                  *item_ui)
{
    int item_y;
    int item_height;

    gtk_widget_translate_coordinates (item_ui, GTK_WIDGET (self->view_ui),
                                      0, 0,
                                      NULL, &item_y);
    item_height = gtk_widget_get_allocated_height (item_ui);

    gtk_adjustment_clamp_page (self->vadjustment, item_y, item_y + item_height);
}

static void
real_reveal_selection (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    GtkWidget *item_ui;

    item_ui = get_first_selected_item_ui (self);

    if (item_ui != NULL)
    {
        reveal_item_ui (self, item_ui);
    }
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
    NautilusCanvasZoomLevel new_level;

    new_level = self->zoom_level + zoom_increment;

    if (new_level >= NAUTILUS_CANVAS_ZOOM_LEVEL_SMALL &&
        new_level <= NAUTILUS_CANVAS_ZOOM_LEVEL_LARGEST)
    {
        g_action_group_change_action_state (self->action_group,
                                            "zoom-to-level",
                                            g_variant_new_int32 (new_level));
    }
}

static guint
get_icon_size_for_zoom_level (NautilusCanvasZoomLevel zoom_level)
{
    switch (zoom_level)
    {
        case NAUTILUS_CANVAS_ZOOM_LEVEL_SMALL:
        {
            return NAUTILUS_CANVAS_ICON_SIZE_SMALL;
        }
        break;

        case NAUTILUS_CANVAS_ZOOM_LEVEL_STANDARD:
        {
            return NAUTILUS_CANVAS_ICON_SIZE_STANDARD;
        }
        break;

        case NAUTILUS_CANVAS_ZOOM_LEVEL_LARGE:
        {
            return NAUTILUS_CANVAS_ICON_SIZE_LARGE;
        }
        break;

        case NAUTILUS_CANVAS_ZOOM_LEVEL_LARGER:
        {
            return NAUTILUS_CANVAS_ICON_SIZE_LARGER;
        }
        break;

        case NAUTILUS_CANVAS_ZOOM_LEVEL_LARGEST:
        {
            return NAUTILUS_CANVAS_ICON_SIZE_LARGEST;
        }
        break;
    }
    g_return_val_if_reached (NAUTILUS_CANVAS_ICON_SIZE_STANDARD);
}

static gint
get_default_zoom_level (void)
{
    NautilusCanvasZoomLevel default_zoom_level;

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
                                        g_variant_new_int32 (NAUTILUS_CANVAS_ZOOM_LEVEL_LARGE));
}

static gfloat
real_get_zoom_level_percentage (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return (gfloat) get_icon_size_for_zoom_level (self->zoom_level) /
           NAUTILUS_CANVAS_ICON_SIZE_LARGE;
}

static gboolean
real_is_zoom_level_default (NautilusFilesView *files_view)
{
    NautilusViewIconController *self;
    guint icon_size;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    icon_size = get_icon_size_for_zoom_level (self->zoom_level);

    return icon_size == NAUTILUS_CANVAS_ICON_SIZE_LARGE;
}

static gboolean
real_can_zoom_in (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return self->zoom_level < NAUTILUS_CANVAS_ZOOM_LEVEL_LARGEST;
}

static gboolean
real_can_zoom_out (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);

    return self->zoom_level > NAUTILUS_CANVAS_ZOOM_LEVEL_SMALL;
}

static GdkRectangle *
get_rectangle_for_item_ui (NautilusViewIconController *self,
                           GtkWidget                  *item_ui)
{
    GdkRectangle *rectangle;
    GtkWidget *content_widget;
    GtkAdjustment *vadjustment;
    GtkAdjustment *hadjustment;

    rectangle = g_new0 (GdkRectangle, 1);
    gtk_widget_get_allocation (item_ui, rectangle);

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (content_widget));
    hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (content_widget));

    rectangle->x -= gtk_adjustment_get_value (hadjustment);
    rectangle->y -= gtk_adjustment_get_value (vadjustment);

    return rectangle;
}

static GdkRectangle *
real_compute_rename_popover_pointing_to (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    GtkWidget *item_ui;

    /* We only allow one item to be renamed with a popover */
    item_ui = get_first_selected_item_ui (self);
    g_return_val_if_fail (item_ui != NULL, NULL);

    return get_rectangle_for_item_ui (self, item_ui);
}

static GdkRectangle *
real_reveal_for_selection_context_menu (NautilusFilesView *files_view)
{
    g_autolist (NautilusFile) selection = NULL;
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    GtkWidget *item_ui;

    selection = nautilus_view_get_selection (NAUTILUS_VIEW (files_view));
    g_return_val_if_fail (selection != NULL, NULL);

    /* Get the focused item_ui, if selected.
     * Otherwise, get the selected item_ui which is sorted the lowest.*/
    item_ui = gtk_widget_get_focus_child (GTK_WIDGET (self->view_ui));
    if (item_ui == NULL || !gtk_flow_box_child_is_selected (GTK_FLOW_BOX_CHILD (item_ui)))
    {
        g_autoptr (GList) list = gtk_flow_box_get_selected_children (self->view_ui);

        list = g_list_last (list);
        item_ui = GTK_WIDGET (list->data);
    }

    reveal_item_ui (self, item_ui);

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
on_button_press_event (GtkGestureMultiPress *gesture,
                       gint                  n_press,
                       gdouble               x,
                       gdouble               y,
                       gpointer              user_data)
{
    NautilusViewIconController *self;
    guint button;
    GdkEventSequence *sequence;
    const GdkEvent *event;
    GdkModifierType modifiers = 0;
    gint view_x;
    gint view_y;
    GtkFlowBoxChild *child_at_pos;

    self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
    event = gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
#if GTK_MAJOR_VERSION < 4
    gtk_get_current_event_state (&modifiers);
#else
    modifiers = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));
#endif

    gtk_widget_translate_coordinates (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)),
                                      GTK_WIDGET (self->view_ui),
                                      x, y, &view_x, &view_y);
    child_at_pos = gtk_flow_box_get_child_at_pos (self->view_ui, view_x, view_y);
    if (child_at_pos != NULL)
    {
        gboolean selection_mode;

        selection_mode = (modifiers & (GDK_CONTROL_MASK | GDK_SHIFT_MASK));
        self->activate_on_release = (self->single_click_mode &&
                                     button == GDK_BUTTON_PRIMARY &&
                                     n_press == 1 &&
                                     !selection_mode);

        /* GtkFlowBox changes selection only with the primary button, but we
         * need that to happen with all buttons, otherwise e.g. opening context
         * menus would require two clicks: a primary click to select the item,
         * followed by a secondary click to open the menu.
         * When holding Ctrl and Shift, GtkFlowBox does a good job, let's not
         * interfere in that case. */
        if (!selection_mode)
        {
            NautilusFile *selected_file;
            NautilusViewItemModel *item_model;
            g_autolist (NautilusFile) selection = NULL;

            item_model = g_list_model_get_item (G_LIST_MODEL (self->model),
                                                gtk_flow_box_child_get_index (child_at_pos));
            selected_file = nautilus_view_item_model_get_file (item_model);
            selection = nautilus_view_get_selection (NAUTILUS_VIEW (self));
            if (g_list_find (selection, selected_file) == NULL)
            {
                nautilus_view_set_selection (NAUTILUS_VIEW (self),
                                             &(GList){ .data = selected_file });
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
                                                               event);
        }
    }
    else
    {
        nautilus_view_set_selection (NAUTILUS_VIEW (self), NULL);
        if (button == GDK_BUTTON_SECONDARY)
        {
            nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (self),
                                                                event);
        }
    }
}

static void
on_click_released (GtkGestureMultiPress *gesture,
                   gint                  n_press,
                   gdouble               x,
                   gdouble               y,
                   gpointer              user_data)
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
on_click_stopped (GtkGestureMultiPress *gesture,
                  gpointer              user_data)
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
    GtkFlowBoxChild *child_at_pos;
    GdkEventSequence *event_sequence;
    GdkEvent *event;
    gint view_x;
    gint view_y;

    event_sequence = gtk_gesture_get_last_updated_sequence (GTK_GESTURE (gesture));
    event = (GdkEvent *) gtk_gesture_get_last_event (GTK_GESTURE (gesture), event_sequence);

    self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);

    gtk_widget_translate_coordinates (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)),
                                      GTK_WIDGET (self->view_ui),
                                      x, y, &view_x, &view_y);
    child_at_pos = gtk_flow_box_get_child_at_pos (self->view_ui, view_x, view_y);
    if (child_at_pos != NULL)
    {
        nautilus_files_view_pop_up_selection_context_menu (NAUTILUS_FILES_VIEW (self),
                                                           event);
    }
    else
    {
        nautilus_view_set_selection (NAUTILUS_VIEW (self), NULL);
        nautilus_files_view_pop_up_background_context_menu (NAUTILUS_FILES_VIEW (self),
                                                            event);
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
real_end_loading (NautilusFilesView *files_view,
                  gboolean           all_files_seen)
{
}

static GtkFlowBoxChild *
get_first_visible_item_ui (NautilusViewIconController *self)
{
    GtkFlowBoxChild *child_at_0;
    gint x0;
    gint y0;
    gint scrolled_y;

    child_at_0 = gtk_flow_box_get_child_at_index (self->view_ui, 0);
    if (child_at_0 == NULL)
    {
        return NULL;
    }
    gtk_widget_translate_coordinates (GTK_WIDGET (child_at_0),
                                      GTK_WIDGET (self->view_ui),
                                      0, 0, &x0, &y0);
    scrolled_y = gtk_adjustment_get_value (self->vadjustment);

    return gtk_flow_box_get_child_at_pos (self->view_ui,
                                          x0,
                                          MAX (y0, scrolled_y));
}

static char *
real_get_first_visible_file (NautilusFilesView *files_view)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (files_view);
    GtkFlowBoxChild *child;
    NautilusViewItemModel *item;
    gchar *uri = NULL;

    child = get_first_visible_item_ui (self);
    if (child != NULL)
    {
        item = g_list_model_get_item (G_LIST_MODEL (self->model),
                                      gtk_flow_box_child_get_index (child));
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
    GtkWidget *item_ui;
    int item_y;

    file = nautilus_file_get_existing_by_uri (data->uri);
    item = nautilus_view_model_get_item_from_file (self->model, file);
    item_ui = nautilus_view_item_model_get_item_ui (item);
    gtk_widget_translate_coordinates (item_ui, GTK_WIDGET (self->view_ui),
                                      0, 0,
                                      NULL, &item_y);
    gtk_adjustment_set_value (self->vadjustment, item_y);

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

    files_queue = convert_glist_to_queue (files);
    item_models = convert_files_to_item_models (self, files_queue);
    nautilus_view_model_add_items (self->model, item_models);
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

    g_clear_object (&self->multi_press_gesture);
    g_clear_handle_id (&self->scroll_to_file_handle_id, g_source_remove);
    g_clear_handle_id (&self->prioritize_thumbnailing_handle_id, g_source_remove);

    g_signal_handlers_disconnect_by_data (nautilus_preferences, self);

    G_OBJECT_CLASS (nautilus_view_icon_controller_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_view_icon_controller_parent_class)->finalize (object);
}

static void
prioritize_thumbnailing_on_idle (NautilusViewIconController *self)
{
    gdouble page_size;
    GtkFlowBoxChild *first_visible_child;
    GtkFlowBoxChild *next_child;
    gint first_index;
    gint next_index;
    gint y;
    gint last_index;
    gpointer item;
    NautilusFile *file;

    self->prioritize_thumbnailing_handle_id = 0;

    page_size = gtk_adjustment_get_page_size (self->vadjustment);
    first_visible_child = get_first_visible_item_ui (self);
    if (first_visible_child == NULL)
    {
        return;
    }

    first_index = gtk_flow_box_child_get_index (first_visible_child);
    for (next_index = first_index + 1; next_index < G_MAXINT; next_index++)
    {
        next_child = gtk_flow_box_get_child_at_index (self->view_ui, next_index);
        if (next_child == NULL)
        {
            break;
        }
        if (gtk_widget_translate_coordinates (GTK_WIDGET (next_child),
                                              GTK_WIDGET (first_visible_child),
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
    for (gint i = last_index; i >= first_index; i--)
    {
        item = g_list_model_get_item (G_LIST_MODEL (self->model), i);
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
on_ui_selected_children_changed (GtkFlowBox *box,
                                 gpointer    user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);

    nautilus_files_view_notify_selection_changed (NAUTILUS_FILES_VIEW (self));
}

static void
bind_item_ui (GtkWidget             **child,
              NautilusViewItemModel  *item_model,
              gpointer                user_data)
{
    nautilus_view_icon_item_ui_set_model (NAUTILUS_VIEW_ICON_ITEM_UI (*child),
                                          item_model);
    nautilus_view_item_model_set_item_ui (item_model, *child);
}

static void
setup_item_ui (GtkWidget **child,
               gpointer    user_data)
{
    NautilusViewIconController *self = NAUTILUS_VIEW_ICON_CONTROLLER (user_data);
    NautilusViewIconItemUi *item_ui;

    item_ui = nautilus_view_icon_item_ui_new ();
    nautilus_view_item_ui_set_caption_attributes (item_ui, self->caption_attributes);

    *child = GTK_WIDGET (item_ui);
    gtk_widget_show_all (*child);
}

static GtkWidget *
create_widget_func (gpointer item,
                    gpointer user_data)
{
    NautilusViewItemModel *item_model = NAUTILUS_VIEW_ITEM_MODEL (item);
    GtkWidget *child = NULL;

    setup_item_ui (&child, user_data);
    bind_item_ui (&child, item_model, user_data);

    return child;
}

static GtkFlowBox *
create_view_ui (NautilusViewIconController *self)
{
    GtkWidget *widget;
    GtkFlowBox *flowbox;

    widget = gtk_flow_box_new ();
    gtk_widget_set_can_focus (widget, TRUE);
    gtk_widget_set_valign (widget, GTK_ALIGN_START);

    flowbox = GTK_FLOW_BOX (widget);
    /* We don't use GtkFlowBox::child-activated because it doesn't fill all our
     * needs nor does it match our expected behavior. Instead, we roll our own
     * event handling and double/single click mode.
     * However, GtkFlowBox::activate-on-single-click has other effects besides
     * activation, as it affects the selection behavior as well. Setting it to
     * FALSE gives us the expected range-selection with Shift+click for free. */
    gtk_flow_box_set_activate_on_single_click (flowbox, FALSE);
    gtk_flow_box_set_max_children_per_line (flowbox, 20);
    gtk_flow_box_set_selection_mode (flowbox, GTK_SELECTION_MULTIPLE);
    gtk_flow_box_set_homogeneous (flowbox, FALSE);
    gtk_flow_box_set_column_spacing (flowbox, 6);
    gtk_flow_box_set_row_spacing (flowbox, 6);

    return flowbox;
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
    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;
    GActionGroup *view_action_group;
    GtkGesture *longpress_gesture;

    content_widget = nautilus_files_view_get_content_widget (NAUTILUS_FILES_VIEW (self));
    hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (content_widget));
    vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (content_widget));

    self->vadjustment = vadjustment;
    g_signal_connect (vadjustment, "changed", (GCallback) on_vadjustment_changed, self);
    g_signal_connect (vadjustment, "value-changed", (GCallback) on_vadjustment_changed, self);

    self->model = nautilus_view_model_new ();

    self->view_ui = create_view_ui (self);
    gtk_flow_box_set_hadjustment (self->view_ui, hadjustment);
    gtk_flow_box_set_vadjustment (self->view_ui, vadjustment);
    gtk_widget_show (GTK_WIDGET (self->view_ui));

    g_signal_connect (self->view_ui, "selected-children-changed", (GCallback) on_ui_selected_children_changed, self);

    gtk_flow_box_bind_model (self->view_ui,
                             G_LIST_MODEL (self->model),
                             create_widget_func, self, NULL);

    self->view_icon = g_themed_icon_new ("view-grid-symbolic");

    /* Compensating for the lack of event boxen to allow clicks outside the flow box. */
    self->multi_press_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (content_widget));
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->multi_press_gesture),
                                                GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multi_press_gesture),
                                   0);
    g_signal_connect (self->multi_press_gesture, "pressed",
                      G_CALLBACK (on_button_press_event), self);
    g_signal_connect (self->multi_press_gesture, "stopped",
                      G_CALLBACK (on_click_stopped), self);
    g_signal_connect (self->multi_press_gesture, "released",
                      G_CALLBACK (on_click_released), self);

    longpress_gesture = gtk_gesture_long_press_new (GTK_WIDGET (self->view_ui));
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (longpress_gesture),
                                                GTK_PHASE_CAPTURE);
    gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (longpress_gesture),
                                       TRUE);
    g_signal_connect (longpress_gesture, "pressed",
                      (GCallback) on_longpress_gesture_pressed_callback,
                      self);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (content_widget),
                                   GTK_WIDGET (self->view_ui));

    self->action_group = nautilus_files_view_get_action_group (NAUTILUS_FILES_VIEW (self));
    g_action_map_add_action_entries (G_ACTION_MAP (self->action_group),
                                     view_icon_actions,
                                     G_N_ELEMENTS (view_icon_actions),
                                     self);

    gtk_widget_show_all (GTK_WIDGET (self));

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
    gtk_widget_add_css_class (GTK_WIDGET (self), "view");
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
}

NautilusViewIconController *
nautilus_view_icon_controller_new (NautilusWindowSlot *slot)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ICON_CONTROLLER,
                         "window-slot", slot,
                         NULL);
}

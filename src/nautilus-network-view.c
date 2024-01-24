/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-list-base-private.h"
#include "nautilus-network-view.h"

#include <glib/gi18n.h>

#include "nautilus-global-preferences.h"
#include "nautilus-network-cell.h"
#include "nautilus-scheme.h"

struct _NautilusNetworkView
{
    NautilusListBase parent_instance;

    GtkListView *view_ui;
};

G_DEFINE_TYPE (NautilusNetworkView, nautilus_network_view, NAUTILUS_TYPE_LIST_BASE)

#define get_view_item(li) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

static const NautilusViewInfo network_view_info =
{
    .view_id = NAUTILUS_VIEW_NETWORK_ID,
    .zoom_level_min = NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_max = NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_standard = NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
};

static NautilusViewInfo
real_get_view_info (NautilusListBase *list_base)
{
    return network_view_info;
}

enum
{
    SECTION_CONNECTED,
    SECTION_PREVIOUS,
    SECTION_AVAILABLE,
};

static inline gint
get_section (NautilusViewItem *item)
{
    NautilusFile *file = nautilus_view_item_get_file (item);

    if (nautilus_file_can_unmount (file))
    {
        return SECTION_CONNECTED;
    }

    g_autoptr (GFile) location = nautilus_file_get_location (file);

    if (g_file_has_uri_scheme (location, SCHEME_NETWORK))
    {
        return SECTION_AVAILABLE;
    }
    else
    {
        return SECTION_PREVIOUS;
    }
}

static gint
sort_network_sections (gconstpointer a,
                       gconstpointer b,
                       gpointer      user_data)
{
    GtkTreeListRow *row_a = GTK_TREE_LIST_ROW ((gpointer) a);
    GtkTreeListRow *row_b = GTK_TREE_LIST_ROW ((gpointer) b);
    g_autoptr (NautilusViewItem) item_a = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row_a));
    g_autoptr (NautilusViewItem) item_b = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row_b));

    return get_section (item_a) - get_section (item_b);
}

static gint
sort_network_items (gconstpointer a,
                    gconstpointer b,
                    gpointer      user_data)
{
    NautilusViewItem *item_a = NAUTILUS_VIEW_ITEM ((gpointer) a);
    NautilusViewItem *item_b = NAUTILUS_VIEW_ITEM ((gpointer) b);
    NautilusFile *file_a = nautilus_view_item_get_file (item_a);
    NautilusFile *file_b = nautilus_view_item_get_file (item_b);

    if (get_section (item_a) == SECTION_PREVIOUS &&
        get_section (item_a) == SECTION_PREVIOUS)
    {
        return nautilus_file_compare_for_sort (file_a, file_b,
                                               NAUTILUS_FILE_SORT_BY_ATIME,
                                               FALSE, TRUE /* reversed */);
    }

    return nautilus_file_compare_for_sort (file_a, file_b,
                                           NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                           FALSE, FALSE);
}

static void
real_set_zoom_level (NautilusListBase *list_base,
                     int               new_level)
{
    g_warn_if_fail (new_level == network_view_info.zoom_level_standard);
}

/* We only care about the keyboard activation part that GtkListView provides,
 * but we don't need any special filtering here. Indeed, we ask GtkListView
 * to not activate on single click, and we get to handle double clicks before
 * GtkListView does (as one of widget subclassing's goal is to modify the parent
 * class's behavior), while claiming the click gestures, so it means GtkListView
 * will never react to a click event to emit this signal. So we should be pretty
 * safe here with regards to our custom item click handling.
 */
static void
on_list_view_item_activated (GtkListView *list_view,
                             guint        position,
                             gpointer     user_data)
{
    NautilusNetworkView *self = NAUTILUS_NETWORK_VIEW (user_data);

    nautilus_list_base_activate_selection (NAUTILUS_LIST_BASE (self), FALSE);
}

static guint
real_get_icon_size (NautilusListBase *list_base_view)
{
    return NAUTILUS_LIST_ICON_SIZE_SMALL;
}

static GtkWidget *
real_get_view_ui (NautilusListBase *list_base_view)
{
    NautilusNetworkView *self = NAUTILUS_NETWORK_VIEW (list_base_view);

    return GTK_WIDGET (self->view_ui);
}

static int
real_get_zoom_level (NautilusListBase *list_base_view)
{
    return network_view_info.zoom_level_standard;
}

static void
real_scroll_to (NautilusListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    NautilusNetworkView *self = NAUTILUS_NETWORK_VIEW (list_base_view);

    gtk_list_view_scroll_to (self->view_ui, position, flags, scroll);
}

static GVariant *
real_get_sort_state (NautilusListBase *list_base)
{
    return g_variant_take_ref (g_variant_new ("(sb)", "invalid", FALSE));
}

static void
real_set_sort_state (NautilusListBase *list_base,
                     GVariant         *value)
{
    /* No op */
}

static void
nautilus_network_view_dispose (GObject *object)
{
    G_OBJECT_CLASS (nautilus_network_view_parent_class)->dispose (object);
}

static void
nautilus_network_view_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_network_view_parent_class)->finalize (object);
}

static void
bind_cell (GtkSignalListItemFactory *factory,
           GtkListItem              *listitem,
           gpointer                  user_data)
{
    GtkWidget *cell = gtk_list_item_get_child (listitem);
    g_autoptr (NautilusViewItem) item = get_view_item (listitem);

    nautilus_view_item_set_item_ui (item, cell);
}

static void
unbind_cell (GtkSignalListItemFactory *factory,
             GtkListItem              *listitem,
             gpointer                  user_data)
{
    g_autoptr (NautilusViewItem) item = get_view_item (listitem);

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
    NautilusNetworkView *self = NAUTILUS_NETWORK_VIEW (user_data);
    NautilusViewCell *cell;
    GtkExpression *expression;

    cell = nautilus_network_cell_new (NAUTILUS_LIST_BASE (self));
    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), cell);
    setup_cell_hover (cell);

    g_object_bind_property (self, "icon-size",
                            cell, "icon-size",
                            G_BINDING_SYNC_CREATE);

    /* Use file display name as accessible label. Explaining in pseudo-code:
     * listitem:accessible-name :- listitem:item:item:file:display-name */
    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_VIEW_ITEM, expression, "file");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_FILE, expression, "display-name");
    gtk_expression_bind (expression, listitem, "accessible-label", listitem);
}

static void
bind_header (GtkSignalListItemFactory *factory,
             GtkListHeader            *listheader,
             gpointer                  user_data)
{
    GtkWidget *label = gtk_list_header_get_child (listheader);
    GtkTreeListRow *row = GTK_TREE_LIST_ROW (gtk_list_header_get_item (listheader));
    g_autoptr (NautilusViewItem) item = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));

    switch (get_section (item))
    {
        case SECTION_CONNECTED:
        {
            /* Translators: This refers to network places which are currently mounted */
            gtk_label_set_label (GTK_LABEL (label), _("Connected"));
        }
        break;

        case SECTION_PREVIOUS:
        {
            /* Translators: This refers to network servers the user has previously connected to */
            gtk_label_set_label (GTK_LABEL (label), _("Previous"));
        }
        break;

        case SECTION_AVAILABLE:
        {
            gtk_label_set_label (GTK_LABEL (label), _("Available on Current Network"));
            /* TODO: Use network name from NMClient:primary-connection:id to
             * match design mockup: "Available on Network1234"
             */
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static void
setup_header (GtkSignalListItemFactory *factory,
              GtkListHeader            *listheader,
              gpointer                  user_data)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_widget_add_css_class (label, "heading");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);

    gtk_list_header_set_child (listheader, label);
}

static void
on_model_changed (NautilusNetworkView *self)
{
    NautilusViewModel *model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    if (model != NULL)
    {
        g_autoptr (GtkCustomSorter) sorter = gtk_custom_sorter_new (sort_network_items, self, NULL);
        g_autoptr (GtkCustomSorter) sections_sorter = gtk_custom_sorter_new (sort_network_sections, self, NULL);

        nautilus_view_model_set_sorter (model, GTK_SORTER (sorter));
        nautilus_view_model_set_section_sorter (model, GTK_SORTER (sections_sorter));
    }

    gtk_list_view_set_model (self->view_ui, GTK_SELECTION_MODEL (model));
}

static GtkListView *
create_view_ui (NautilusNetworkView *self)
{
    g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
    g_autoptr (GtkListItemFactory) header_factory = gtk_signal_list_item_factory_new ();
    GtkListView *list_view;

    g_signal_connect (factory, "setup", G_CALLBACK (setup_cell), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_cell), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cell), self);

    list_view = GTK_LIST_VIEW (gtk_list_view_new (NULL, g_steal_pointer (&factory)));

    g_signal_connect (header_factory, "setup", G_CALLBACK (setup_header), self);
    g_signal_connect (header_factory, "bind", G_CALLBACK (bind_header), self);
    gtk_list_view_set_header_factory (list_view, header_factory);

    /* We don't use the built-in child activation feature for clicks because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkListView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_list_view_set_single_click_activate (list_view, FALSE);
    gtk_list_view_set_enable_rubberband (list_view, FALSE);
    gtk_list_view_set_tab_behavior (list_view, GTK_LIST_TAB_ITEM);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter).
     */
    g_signal_connect (list_view, "activate", G_CALLBACK (on_list_view_item_activated), self);

    return list_view;
}

static void
nautilus_network_view_class_init (NautilusNetworkViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->dispose = nautilus_network_view_dispose;
    object_class->finalize = nautilus_network_view_finalize;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_sort_state = real_get_sort_state;
    list_base_view_class->get_view_info = real_get_view_info;
    list_base_view_class->get_view_ui = real_get_view_ui;
    list_base_view_class->get_zoom_level = real_get_zoom_level;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->set_sort_state = real_set_sort_state;
    list_base_view_class->set_zoom_level = real_set_zoom_level;
}

static void
nautilus_network_view_init (NautilusNetworkView *self)
{
    GtkWidget *scrolled_window = nautilus_list_base_get_scrolled_window (NAUTILUS_LIST_BASE (self));

    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-network-view");

    self->view_ui = create_view_ui (self);
    nautilus_list_base_setup_gestures (NAUTILUS_LIST_BASE (self));

    g_signal_connect_swapped (self, "notify::model", G_CALLBACK (on_model_changed), self);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   GTK_WIDGET (self->view_ui));
    nautilus_list_base_set_zoom_level (NAUTILUS_LIST_BASE (self), network_view_info.zoom_level_standard);
}

NautilusNetworkView *
nautilus_network_view_new (void)
{
    return g_object_new (NAUTILUS_TYPE_NETWORK_VIEW, NULL);
}

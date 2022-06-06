#include "nautilus-view-icon-item-ui.h"

struct _NautilusViewIconItemUi
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark *caption_attributes;

    GtkWidget *fixed_height_box;
    GtkWidget *icon;
    GtkWidget *label;
    GtkWidget *first_caption;
    GtkWidget *second_caption;
    GtkWidget *third_caption;
};

G_DEFINE_TYPE (NautilusViewIconItemUi, nautilus_view_icon_item_ui, NAUTILUS_TYPE_VIEW_CELL)

#define EXTRA_WIDTH_FOR_TEXT 36

static void
update_icon (NautilusViewIconItemUi *self)
{
    NautilusViewItemModel *item;
    NautilusFileIconFlags flags;
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    GtkStyleContext *style_context;
    NautilusFile *file;
    guint icon_size;
    g_autofree gchar *thumbnail_path = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_model_get_file (item);
    icon_size = nautilus_view_item_model_get_icon_size (item);
    flags = NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
            NAUTILUS_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE |
            NAUTILUS_FILE_ICON_FLAGS_USE_EMBLEMS |
            NAUTILUS_FILE_ICON_FLAGS_USE_ONE_EMBLEM;

    icon_paintable = nautilus_file_get_icon_paintable (file, icon_size, 1, flags);
    gtk_picture_set_paintable (GTK_PICTURE (self->icon), icon_paintable);

    /* Set the same height and width for all icons regardless of aspect ratio.
     */
    gtk_widget_set_size_request (self->fixed_height_box, icon_size, icon_size);
    if (icon_size < NAUTILUS_GRID_ICON_SIZE_LARGEST)
    {
        int extra_margins = 0.5 * EXTRA_WIDTH_FOR_TEXT;
        gtk_widget_set_margin_start (self->fixed_height_box, extra_margins);
        gtk_widget_set_margin_end (self->fixed_height_box, extra_margins);
    }
    style_context = gtk_widget_get_style_context (self->icon);
    thumbnail_path = nautilus_file_get_thumbnail_path (file);
    if (thumbnail_path != NULL &&
        nautilus_file_should_show_thumbnail (file))
    {
        gtk_style_context_add_class (style_context, "thumbnail");
    }
    else
    {
        gtk_style_context_remove_class (style_context, "thumbnail");
    }
}

static void
update_captions (NautilusViewIconItemUi *self)
{
    NautilusViewItemModel *item;
    NautilusFile *file;
    GtkWidget * const caption_labels[] =
    {
        self->first_caption,
        self->second_caption,
        self->third_caption
    };
    G_STATIC_ASSERT (G_N_ELEMENTS (caption_labels) == NAUTILUS_VIEW_ICON_N_CAPTIONS);

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_model_get_file (item);
    for (guint i = 0; i < NAUTILUS_VIEW_ICON_N_CAPTIONS; i++)
    {
        GQuark attribute_q = self->caption_attributes[i];
        gboolean show_caption;

        show_caption = (attribute_q != 0);
        gtk_widget_set_visible (caption_labels[i], show_caption);
        if (show_caption)
        {
            g_autofree gchar *string = NULL;
            string = nautilus_file_get_string_attribute_q (file, attribute_q);
            gtk_label_set_text (GTK_LABEL (caption_labels[i]), string);
        }
    }
}

static void
on_file_changed (NautilusViewIconItemUi *self)
{
    NautilusViewItemModel *item;
    NautilusFile *file;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_model_get_file (item);

    update_icon (self);

    gtk_label_set_text (GTK_LABEL (self->label),
                        nautilus_file_get_display_name (file));
    update_captions (self);
}

static void
on_item_size_changed (NautilusViewIconItemUi *self)
{
    update_icon (self);
    update_captions (self);
}

static void
on_item_is_cut_changed (NautilusViewIconItemUi *self)
{
    gboolean is_cut;

    g_object_get (self, "is-cut", &is_cut, NULL);
    if (is_cut)
    {
        gtk_widget_add_css_class (self->icon, "cut");
    }
    else
    {
        gtk_widget_remove_css_class (self->icon, "cut");
    }
}

static void
finalize (GObject *object)
{
    NautilusViewIconItemUi *self = (NautilusViewIconItemUi *) object;

    g_object_unref (self->item_signal_group);
    G_OBJECT_CLASS (nautilus_view_icon_item_ui_parent_class)->finalize (object);
}

static void
nautilus_view_icon_item_ui_class_init (NautilusViewIconItemUiClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-view-icon-item-ui.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusViewIconItemUi, fixed_height_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusViewIconItemUi, icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusViewIconItemUi, label);
    gtk_widget_class_bind_template_child (widget_class, NautilusViewIconItemUi, first_caption);
    gtk_widget_class_bind_template_child (widget_class, NautilusViewIconItemUi, second_caption);
    gtk_widget_class_bind_template_child (widget_class, NautilusViewIconItemUi, third_caption);
}

static void
nautilus_view_icon_item_ui_init (NautilusViewIconItemUi *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM_MODEL);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::icon-size",
                                    (GCallback) on_item_size_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) on_item_is_cut_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) on_file_changed, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) on_file_changed, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);

#if PANGO_VERSION_CHECK (1, 44, 4)
    {
        PangoAttrList *attr_list;

        /* GTK4 TODO: This attribute is set in the UI file but GTK 3 ignores it.
         * Remove this block after the switch to GTK 4. */
        attr_list = pango_attr_list_new ();
        pango_attr_list_insert (attr_list, pango_attr_insert_hyphens_new (FALSE));
        gtk_label_set_attributes (GTK_LABEL (self->label), attr_list);
        pango_attr_list_unref (attr_list);
    }
#endif
}

NautilusViewIconItemUi *
nautilus_view_icon_item_ui_new (NautilusListBase *view)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ICON_ITEM_UI,
                         "view", view,
                         NULL);
}

void
nautilus_view_item_ui_set_caption_attributes (NautilusViewIconItemUi *self,
                                              GQuark                 *attrs)
{
    self->caption_attributes = attrs;
}

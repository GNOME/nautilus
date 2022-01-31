#include "nautilus-view-icon-item-ui.h"
#include "nautilus-view-item-model.h"
#include "nautilus-file.h"
#include "nautilus-thumbnails.h"

struct _NautilusViewIconItemUi
{
    GtkBox parent_instance;

    NautilusViewItemModel *model;
    GQuark *caption_attributes;

    GtkWidget *fixed_height_box;
    GtkWidget *icon;
    GtkWidget *label;
    GtkWidget *first_caption;
    GtkWidget *second_caption;
    GtkWidget *third_caption;
};

G_DEFINE_TYPE (NautilusViewIconItemUi, nautilus_view_icon_item_ui, GTK_TYPE_BOX)

enum
{
    PROP_0,
    PROP_MODEL,
    N_PROPS
};

#define EXTRA_WIDTH_FOR_TEXT 36

static void
update_icon (NautilusViewIconItemUi *self)
{
    NautilusFileIconFlags flags;
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    GtkStyleContext *style_context;
    NautilusFile *file;
    guint icon_size;
    g_autofree gchar *thumbnail_path = NULL;

    file = nautilus_view_item_model_get_file (self->model);
    icon_size = nautilus_view_item_model_get_icon_size (self->model);
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
    NautilusFile *file;
    GtkWidget * const caption_labels[] =
    {
        self->first_caption,
        self->second_caption,
        self->third_caption
    };
    G_STATIC_ASSERT (G_N_ELEMENTS (caption_labels) == NAUTILUS_VIEW_ICON_N_CAPTIONS);

    file = nautilus_view_item_model_get_file (self->model);
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
    NautilusFile *file;

    file = nautilus_view_item_model_get_file (self->model);

    update_icon (self);

    gtk_label_set_text (GTK_LABEL (self->label),
                        nautilus_file_get_display_name (file));
    update_captions (self);
}

static void
on_view_item_size_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (user_data);

    update_icon (self);
    update_captions (self);
}

static void
set_model (NautilusViewIconItemUi *self,
           NautilusViewItemModel  *model);

static void
finalize (GObject *object)
{
    NautilusViewIconItemUi *self = (NautilusViewIconItemUi *) object;

    set_model (self, NULL);
    G_OBJECT_CLASS (nautilus_view_icon_item_ui_parent_class)->finalize (object);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (object);

    switch (prop_id)
    {
        case PROP_MODEL:
        {
            g_value_set_object (value, self->model);
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_model (NautilusViewIconItemUi *self,
           NautilusViewItemModel  *model)
{
    NautilusFile *file;

    if (self->model == model)
    {
        return;
    }

    if (self->model != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->model, self);
        g_clear_object (&self->model);
    }

    if (model == NULL)
    {
        return;
    }

    self->model = g_object_ref (model);

    file = nautilus_view_item_model_get_file (self->model);

    update_icon (self);
    gtk_label_set_text (GTK_LABEL (self->label),
                        nautilus_file_get_display_name (file));
    update_captions (self);

    g_signal_connect (self->model, "notify::icon-size",
                      (GCallback) on_view_item_size_changed, self);
    g_signal_connect_swapped (self->model, "file-changed",
                              (GCallback) on_file_changed, self);
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (object);

    switch (prop_id)
    {
        case PROP_MODEL:
        {
            set_model (self, g_value_get_object (value));
        }
        break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
nautilus_view_icon_item_ui_class_init (NautilusViewIconItemUiClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;

    g_object_class_install_property (object_class,
                                     PROP_MODEL,
                                     g_param_spec_object ("model",
                                                          "Item model",
                                                          "The item model that this UI reprensents",
                                                          NAUTILUS_TYPE_VIEW_ITEM_MODEL,
                                                          G_PARAM_READWRITE));

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
nautilus_view_icon_item_ui_new (void)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ICON_ITEM_UI, NULL);
}

void
nautilus_view_icon_item_ui_set_model (NautilusViewIconItemUi *self,
                                      NautilusViewItemModel  *model)
{
    g_object_set (self, "model", model, NULL);
}

NautilusViewItemModel *
nautilus_view_icon_item_ui_get_model (NautilusViewIconItemUi *self)
{
    NautilusViewItemModel *model = NULL;

    g_object_get (self, "model", &model, NULL);

    return model;
}

void
nautilus_view_item_ui_set_caption_attributes (NautilusViewIconItemUi *self,
                                              GQuark                 *attrs)
{
    self->caption_attributes = attrs;
}

#include "nautilus-view-icon-item-ui.h"
#include "nautilus-view-item-model.h"
#include "nautilus-file.h"
#include "nautilus-thumbnails.h"

struct _NautilusViewIconItemUi
{
    GtkFlowBoxChild parent_instance;

    NautilusViewItemModel *model;

    GtkWidget *fixed_height_box;
    GtkWidget *icon;
    GtkWidget *label;
};

G_DEFINE_TYPE (NautilusViewIconItemUi, nautilus_view_icon_item_ui, GTK_TYPE_FLOW_BOX_CHILD)

enum
{
    PROP_0,
    PROP_MODEL,
    N_PROPS
};

static void
update_icon (NautilusViewIconItemUi *self)
{
    NautilusFileIconFlags flags;
    g_autoptr (GdkPixbuf) icon_pixbuf = NULL;
    GtkStyleContext *style_context;
    NautilusFile *file;
    guint icon_size;

    file = nautilus_view_item_model_get_file (self->model);
    icon_size = nautilus_view_item_model_get_icon_size (self->model);
    flags = NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS |
            NAUTILUS_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE |
            NAUTILUS_FILE_ICON_FLAGS_USE_EMBLEMS |
            NAUTILUS_FILE_ICON_FLAGS_USE_ONE_EMBLEM;

    icon_pixbuf = nautilus_file_get_icon_pixbuf (file, icon_size,
                                                 TRUE, 1, flags);
    gtk_image_set_from_pixbuf (GTK_IMAGE (self->icon), icon_pixbuf);

    gtk_widget_set_size_request (self->fixed_height_box, icon_size, icon_size);
    style_context = gtk_widget_get_style_context (self->fixed_height_box);
    if (nautilus_can_thumbnail (file) &&
        nautilus_file_should_show_thumbnail (file))
    {
        gtk_style_context_add_class (style_context, "icon-background");
    }
    else
    {
        gtk_style_context_remove_class (style_context, "icon-background");
    }
}

static void
on_view_item_file_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (user_data);
    NautilusFile *file;

    file = nautilus_view_item_model_get_file (self->model);

    update_icon (self);

    gtk_label_set_text (GTK_LABEL (self->label),
                        nautilus_file_get_display_name (file));
}

static void
on_view_item_size_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (user_data);

    update_icon (self);
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

    g_signal_connect (self->model, "notify::icon-size",
                      (GCallback) on_view_item_size_changed, self);
    g_signal_connect (self->model, "notify::file",
                      (GCallback) on_view_item_file_changed, self);
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

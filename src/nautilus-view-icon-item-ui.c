#include "nautilus-view-icon-item-ui.h"
#include "nautilus-view-item-model.h"
#include "nautilus-container-max-width.h"
#include "nautilus-file.h"
#include "nautilus-thumbnails.h"

struct _NautilusViewIconItemUi
{
    GtkFlowBoxChild parent_instance;

    NautilusViewItemModel *model;

    NautilusContainerMaxWidth *item_container;
    GtkWidget *icon;
    GtkLabel *label;
};

G_DEFINE_TYPE (NautilusViewIconItemUi, nautilus_view_icon_item_ui, GTK_TYPE_FLOW_BOX_CHILD)

enum
{
    PROP_0,
    PROP_MODEL,
    N_PROPS
};

static GtkWidget *
create_icon (NautilusViewIconItemUi *self)
{
    NautilusFileIconFlags flags;
    g_autoptr (GdkPixbuf) icon_pixbuf;
    GtkImage *icon;
    GtkBox *fixed_height_box;
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
    icon = GTK_IMAGE (gtk_image_new_from_pixbuf (icon_pixbuf));
    gtk_widget_set_hexpand (GTK_WIDGET (icon), TRUE);
    gtk_widget_set_vexpand (GTK_WIDGET (icon), TRUE);
    gtk_widget_set_valign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);
    gtk_widget_set_halign (GTK_WIDGET (icon), GTK_ALIGN_CENTER);

    fixed_height_box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_valign (GTK_WIDGET (fixed_height_box), GTK_ALIGN_CENTER);
    gtk_widget_set_halign (GTK_WIDGET (fixed_height_box), GTK_ALIGN_CENTER);
    gtk_widget_set_size_request (GTK_WIDGET (fixed_height_box), icon_size, icon_size);

    if (nautilus_can_thumbnail (file) &&
        nautilus_file_should_show_thumbnail (file))
    {
        style_context = gtk_widget_get_style_context (GTK_WIDGET (fixed_height_box));
        gtk_style_context_add_class (style_context, "icon-background");
    }

    gtk_box_pack_start (fixed_height_box, GTK_WIDGET (icon));

    return GTK_WIDGET (fixed_height_box);
}

static void
update_icon (NautilusViewIconItemUi *self)
{
    GtkBox *box;
    guint icon_size;

    icon_size = nautilus_view_item_model_get_icon_size (self->model);
    nautilus_container_max_width_set_max_width (NAUTILUS_CONTAINER_MAX_WIDTH (self->item_container),
                                                icon_size);
    box = GTK_BOX (gtk_bin_get_child (GTK_BIN (self->item_container)));
    if (self->icon)
    {
        gtk_container_remove (GTK_CONTAINER (box), GTK_WIDGET (self->icon));
    }
    self->icon = create_icon (self);
    gtk_widget_show_all (GTK_WIDGET (self->icon));
    gtk_box_pack_start (box, GTK_WIDGET (self->icon));
}

static void
on_view_item_file_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (user_data);
    NautilusFile *file;

    file = nautilus_view_item_model_get_file (self->model);

    if (self->icon)
    {
        update_icon (self);
    }

    if (self->label)
    {
        gtk_label_set_text (self->label,
                            nautilus_file_get_display_name (file));
    }
}

static void
on_view_item_size_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (user_data);

    if (self->icon)
    {
        update_icon (self);
    }
}

static void
constructed (GObject *object)
{
    NautilusViewIconItemUi *self = NAUTILUS_VIEW_ICON_ITEM_UI (object);
    GtkBox *container;
    GtkBox *item_selection_background;
    GtkLabel *label;
    GtkStyleContext *style_context;
    NautilusFile *file;
    guint icon_size;

    G_OBJECT_CLASS (nautilus_view_icon_item_ui_parent_class)->constructed (object);

    file = nautilus_view_item_model_get_file (self->model);
    icon_size = nautilus_view_item_model_get_icon_size (self->model);
    container = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
    /* This container is for having a constant selection background, instead of
     * the dinamically sized one of the GtkFlowBox
     */
    item_selection_background = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_halign (GTK_WIDGET (item_selection_background), GTK_ALIGN_CENTER);
    gtk_widget_set_valign (GTK_WIDGET (item_selection_background), GTK_ALIGN_START);
    self->item_container = nautilus_container_max_width_new ();
    self->icon = create_icon (self);
    gtk_box_pack_start (container, GTK_WIDGET (self->icon));

    label = GTK_LABEL (gtk_label_new (nautilus_file_get_display_name (file)));
    gtk_widget_set_vexpand (GTK_WIDGET (label), TRUE);
    gtk_widget_show (GTK_WIDGET (label));
    gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_line_wrap (label, TRUE);
    gtk_label_set_line_wrap_mode (label, PANGO_WRAP_WORD_CHAR);
    gtk_label_set_lines (label, 3);
    gtk_label_set_justify (label, GTK_JUSTIFY_CENTER);
    gtk_widget_set_valign (GTK_WIDGET (label), GTK_ALIGN_START);
    gtk_box_pack_end (container, GTK_WIDGET (label));

    style_context = gtk_widget_get_style_context (GTK_WIDGET (item_selection_background));
    gtk_style_context_add_class (style_context, "icon-item-background");

    gtk_widget_set_valign (GTK_WIDGET (container), GTK_ALIGN_START);
    gtk_widget_set_halign (GTK_WIDGET (container), GTK_ALIGN_CENTER);

    gtk_container_add (GTK_CONTAINER (self->item_container),
                       GTK_WIDGET (container));
    nautilus_container_max_width_set_max_width (NAUTILUS_CONTAINER_MAX_WIDTH (self->item_container),
                                                icon_size);

    gtk_container_add (GTK_CONTAINER (item_selection_background),
                       GTK_WIDGET (self->item_container));
    gtk_container_add (GTK_CONTAINER (self),
                       GTK_WIDGET (item_selection_background));
    gtk_widget_show_all (GTK_WIDGET (self));

    g_signal_connect (self->model, "notify::icon-size",
                      (GCallback) on_view_item_size_changed, self);
    g_signal_connect (self->model, "notify::file",
                      (GCallback) on_view_item_file_changed, self);
}

static void
finalize (GObject *object)
{
    NautilusViewIconItemUi *self = (NautilusViewIconItemUi *) object;

    g_signal_handlers_disconnect_by_data (self->model, self);
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
    self->model = g_object_ref (model);
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

    object_class->finalize = finalize;
    object_class->get_property = get_property;
    object_class->set_property = set_property;
    object_class->constructed = constructed;

    g_object_class_install_property (object_class,
                                     PROP_MODEL,
                                     g_param_spec_object ("model",
                                                          "Item model",
                                                          "The item model that this UI reprensents",
                                                          NAUTILUS_TYPE_VIEW_ITEM_MODEL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
nautilus_view_icon_item_ui_init (NautilusViewIconItemUi *self)
{
}

NautilusViewIconItemUi *
nautilus_view_icon_item_ui_new (NautilusViewItemModel *model)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_ICON_ITEM_UI,
                         "model", model,
                         NULL);
}

NautilusViewItemModel *
nautilus_view_icon_item_ui_get_model (NautilusViewIconItemUi *self)
{
    return self->model;
}

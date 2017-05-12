#include "nautilus-view-list-item-ui.h"
#include "nautilus-view-item-model.h"
#include "nautilus-container-max-width.h"
#include "nautilus-file.h"
#include "nautilus-thumbnails.h"

struct _NautilusViewListItemUi
{
    GtkListBoxRow parent_instance;

    NautilusViewItemModel *model;

    GtkBox *container;
    GtkWidget *icon;
    GtkLabel *label;
};

G_DEFINE_TYPE (NautilusViewListItemUi, nautilus_view_list_item_ui, GTK_TYPE_LIST_BOX_ROW)

enum
{
    PROP_0,
    PROP_MODEL,
    N_PROPS
};

static GtkWidget *
create_icon (NautilusViewListItemUi *self)
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

    gtk_box_pack_start (fixed_height_box, GTK_WIDGET (icon), FALSE, FALSE, 0);

    return GTK_WIDGET (fixed_height_box);
}

static void
update_icon (NautilusViewListItemUi *self)
{
    if (self->icon)
    {
        gtk_container_remove (GTK_CONTAINER (self->container), GTK_WIDGET (self->icon));
    }
    self->icon = create_icon (self);
    gtk_widget_show_all (GTK_WIDGET (self->icon));
    gtk_box_pack_start (self->container, GTK_WIDGET (self->icon), FALSE, FALSE, 0);
}

static void
on_view_item_file_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
    NautilusViewListItemUi *self = NAUTILUS_VIEW_LIST_ITEM_UI (user_data);
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
    NautilusViewListItemUi *self = NAUTILUS_VIEW_LIST_ITEM_UI (user_data);

    if (self->icon)
    {
        update_icon (self);
    }
}

static void
constructed (GObject *object)
{
    NautilusViewListItemUi *self = NAUTILUS_VIEW_LIST_ITEM_UI (object);
    GtkLabel *label;
    GtkStyleContext *style_context;
    NautilusFile *file;

    G_OBJECT_CLASS (nautilus_view_list_item_ui_parent_class)->constructed (object);

    file = nautilus_view_item_model_get_file (self->model);
    self->container = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10));
    gtk_widget_set_halign (GTK_WIDGET (self->container), GTK_ALIGN_START);

    self->icon = create_icon (self);
    gtk_box_pack_start (self->container, GTK_WIDGET (self->icon), FALSE, FALSE, 0);

    label = GTK_LABEL (gtk_label_new (nautilus_file_get_display_name (file)));
    gtk_widget_show (GTK_WIDGET (label));
    gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_line_wrap (label, FALSE);
    gtk_label_set_line_wrap_mode (label, PANGO_WRAP_WORD_CHAR);
    gtk_label_set_justify (label, GTK_JUSTIFY_CENTER);
    gtk_box_pack_end (self->container, GTK_WIDGET (label), TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->container));
    gtk_widget_show_all (GTK_WIDGET (self->container));

    style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_add_class (style_context, "view-list-row");

    g_signal_connect (self->model, "notify::icon-size",
                      (GCallback) on_view_item_size_changed, self);
    g_signal_connect (self->model, "notify::file",
                      (GCallback) on_view_item_file_changed, self);
}

static void
finalize (GObject *object)
{
    NautilusViewListItemUi *self = (NautilusViewListItemUi *) object;

    g_signal_handlers_disconnect_by_data (self->model, self);
    G_OBJECT_CLASS (nautilus_view_list_item_ui_parent_class)->finalize (object);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    NautilusViewListItemUi *self = NAUTILUS_VIEW_LIST_ITEM_UI (object);

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
set_model (NautilusViewListItemUi *self,
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
    NautilusViewListItemUi *self = NAUTILUS_VIEW_LIST_ITEM_UI (object);

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
nautilus_view_list_item_ui_class_init (NautilusViewListItemUiClass *klass)
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
nautilus_view_list_item_ui_init (NautilusViewListItemUi *self)
{
}

NautilusViewListItemUi *
nautilus_view_list_item_ui_new (NautilusViewItemModel *model)
{
    return g_object_new (NAUTILUS_TYPE_VIEW_LIST_ITEM_UI,
                         "model", model,
                         NULL);
}

NautilusViewItemModel *
nautilus_view_list_item_ui_get_model (NautilusViewListItemUi *self)
{
    return self->model;
}

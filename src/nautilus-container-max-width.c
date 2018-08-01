#include "nautilus-container-max-width.h"

#define DEFAULT_MAX_SIZE 120

struct _NautilusContainerMaxWidth
{
    GtkBin parent_instance;
    guint max_width;

    gboolean width_maximized;
    guint change_width_maximized_idle_id;
};

G_DEFINE_TYPE (NautilusContainerMaxWidth, nautilus_container_max_width, GTK_TYPE_BIN)

enum
{
    PROP_0,
    PROP_MAX_WIDTH,
    PROP_WIDTH_MAXIMIZED,
    N_PROPS
};

void
nautilus_container_max_width_set_max_width (NautilusContainerMaxWidth *self,
                                            guint                      max_width)
{
    self->max_width = max_width;
    gtk_widget_queue_allocate (GTK_WIDGET (self));
}

guint
nautilus_container_max_width_get_max_width (NautilusContainerMaxWidth *self)
{
    return self->max_width;
}

gboolean
nautilus_container_max_width_get_width_maximized (NautilusContainerMaxWidth *self)
{
    return self->width_maximized;
}

NautilusContainerMaxWidth *
nautilus_container_max_width_new (void)
{
    return g_object_new (NAUTILUS_TYPE_CONTAINER_MAX_WIDTH, NULL);
}

static void
nautilus_container_max_width_finalize (GObject *object)
{
    NautilusContainerMaxWidth *self = NAUTILUS_CONTAINER_MAX_WIDTH (object);

    if (self->change_width_maximized_idle_id != 0)
    {
        g_source_remove (self->change_width_maximized_idle_id);
    }

    G_OBJECT_CLASS (nautilus_container_max_width_parent_class)->finalize (object);
}

static void
nautilus_container_max_width_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
    NautilusContainerMaxWidth *self = NAUTILUS_CONTAINER_MAX_WIDTH (object);

    switch (prop_id)
    {
        case PROP_MAX_WIDTH:
        {
            g_value_set_int (value, self->max_width);
        }
        break;

        case PROP_WIDTH_MAXIMIZED:
        {
            g_value_set_boolean (value, self->width_maximized);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
nautilus_container_max_width_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
    NautilusContainerMaxWidth *self = NAUTILUS_CONTAINER_MAX_WIDTH (object);

    switch (prop_id)
    {
        case PROP_MAX_WIDTH:
        {
            nautilus_container_max_width_set_max_width (self, g_value_get_int (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
get_preferred_width (GtkWidget *widget,
                     gint      *minimum_size,
                     gint      *natural_size)
{
    GtkWidget *child;
    NautilusContainerMaxWidth *self;
    GtkStyleContext *style_context;
    GtkBorder padding;

    self = NAUTILUS_CONTAINER_MAX_WIDTH (widget);
    child = gtk_bin_get_child (GTK_BIN (self));

    *natural_size = 0;
    *minimum_size = 0;
    gtk_widget_get_preferred_width (child, minimum_size, natural_size);

    if (self->max_width != -1 && *minimum_size > self->max_width)
    {
        g_critical ("NautilusContainerMaxWidth's child requested %d while set maximum width is %d",
                    *minimum_size, self->max_width);
    }
    *natural_size = self->max_width == -1 ? *natural_size :
                    MAX (*minimum_size, MIN (self->max_width, *natural_size));

    style_context = gtk_widget_get_style_context (widget);
    gtk_style_context_get_padding (style_context,
                                   gtk_widget_get_state_flags (widget),
                                   &padding);
    *minimum_size += padding.left + padding.right;
    *natural_size += padding.left + padding.right;
}

static void
get_preferred_height (GtkWidget *widget,
                      gint      *minimum_size,
                      gint      *natural_size)
{
    GtkWidget *child;
    NautilusContainerMaxWidth *self;
    gint minimum_width = 0;
    gint natural_width = 0;
    GtkStyleContext *style_context;
    GtkBorder padding;

    self = NAUTILUS_CONTAINER_MAX_WIDTH (widget);
    child = gtk_bin_get_child (GTK_BIN (self));

    get_preferred_width (widget, &minimum_width, &natural_width);
    natural_width = self->max_width == -1 ? natural_width : MIN (self->max_width, natural_width);

    gtk_widget_get_preferred_height_for_width (child, natural_width, minimum_size, natural_size);

    style_context = gtk_widget_get_style_context (widget);
    gtk_style_context_get_padding (style_context,
                                   gtk_widget_get_state_flags (widget),
                                   &padding);
    *minimum_size += padding.top + padding.bottom;
    *natural_size += padding.top + padding.bottom;
}

static void
get_preferred_height_for_width (GtkWidget *widget,
                                gint       width,
                                gint      *minimum_size,
                                gint      *natural_size)
{
    get_preferred_height (widget, minimum_size, natural_size);
}

static void
size_allocate (GtkWidget     *widget,
               GtkAllocation *allocation)
{
    GTK_WIDGET_CLASS (nautilus_container_max_width_parent_class)->size_allocate (widget, allocation);
}

static void
get_preferred_width_for_height (GtkWidget *widget,
                                gint       height,
                                gint      *minimum_size,
                                gint      *natural_size)
{
    get_preferred_width (widget, minimum_size, natural_size);
}

static gboolean
change_width_maximized_idle_callback (gpointer userdata)
{
    NautilusContainerMaxWidth *self = userdata;

    self->change_width_maximized_idle_id = 0;

    self->width_maximized = !self->width_maximized;
    g_object_notify (G_OBJECT (self), "width-maximized");

    return G_SOURCE_REMOVE;
}

static void
on_size_allocate (GtkWidget    *widget,
                  GdkRectangle *allocation,
                  gpointer      userdata)
{
    NautilusContainerMaxWidth *self = NAUTILUS_CONTAINER_MAX_WIDTH (widget);
    gboolean is_width_maximized;

    is_width_maximized = self->max_width == -1 ? FALSE : allocation->width >= self->max_width;

    if (self->width_maximized != is_width_maximized)
    {
        /* The handlers of the "notify::width-maximized" signal may trigger
         * a reallocation, which shouldn't happen at this point because we are
         * still in size_allocate phase. So, change the property on idle*/
        if (self->change_width_maximized_idle_id == 0)
        {
            self->change_width_maximized_idle_id = g_idle_add (change_width_maximized_idle_callback, self);
        }
    }
    else if (self->change_width_maximized_idle_id != 0)
    {
        /* This was going to change self->width_maximized, let's cancel it. */
        g_source_remove (self->change_width_maximized_idle_id);
        self->change_width_maximized_idle_id = 0;
    }
}

static void
constructed (GObject *obj)
{
    NautilusContainerMaxWidth *self = NAUTILUS_CONTAINER_MAX_WIDTH (obj);

    G_OBJECT_CLASS (nautilus_container_max_width_parent_class)->constructed (obj);

    /* We want our parent to gives our preferred width */
    gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
    self->max_width = -1;

    /* We want to know when the container has grown to its max width */
    self->width_maximized = FALSE;
    g_signal_connect (GTK_WIDGET (self),
                      "size-allocate",
                      G_CALLBACK (on_size_allocate),
                      NULL);
}

static void
nautilus_container_max_width_class_init (NautilusContainerMaxWidthClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_container_max_width_finalize;
    object_class->get_property = nautilus_container_max_width_get_property;
    object_class->set_property = nautilus_container_max_width_set_property;
    object_class->constructed = constructed;

    widget_class->get_preferred_width = get_preferred_width;
    widget_class->get_preferred_width_for_height = get_preferred_width_for_height;
    widget_class->get_preferred_height = get_preferred_height;
    widget_class->get_preferred_height_for_width = get_preferred_height_for_width;
    widget_class->size_allocate = size_allocate;

    g_object_class_install_property (object_class,
                                     PROP_MAX_WIDTH,
                                     g_param_spec_int ("max-width",
                                                       "Max width",
                                                       "The max width of the container",
                                                       G_MININT,
                                                       G_MAXINT,
                                                       0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                                     PROP_WIDTH_MAXIMIZED,
                                     g_param_spec_boolean ("width-maximized",
                                                           "Width maximized",
                                                           "Whether the container is at the max width",
                                                           FALSE,
                                                           G_PARAM_READABLE));
}
static void
nautilus_container_max_width_init (NautilusContainerMaxWidth *self)
{
}

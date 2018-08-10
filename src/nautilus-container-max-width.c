#include "nautilus-container-max-width.h"

struct _NautilusContainerMaxWidth
{
    GtkBin parent_instance;
    int max_width;

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
                                            int                        max_width)
{
    g_return_if_fail (NAUTILUS_IS_CONTAINER_MAX_WIDTH (self));

    g_object_set (self, "max-width", max_width, NULL);
}

int
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
            self->max_width = g_value_get_int (value);
            gtk_widget_queue_allocate (GTK_WIDGET (self));
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
measure (GtkWidget      *widget,
         GtkOrientation  orientation,
         int             for_size,
         int            *minimum,
         int            *natural,
         int            *minimum_baseline,
         int            *natural_baseline)
{
    NautilusContainerMaxWidth *self;
    GtkWidget *child;
    int child_minimum;
    int child_natural;
    int child_minimum_width;
    int child_natural_width;
    GtkBorder padding;

    self = NAUTILUS_CONTAINER_MAX_WIDTH (widget);
    child = gtk_bin_get_child (GTK_BIN (widget));
    child_minimum = 0;
    child_natural = 0;

    if (child == NULL || !gtk_widget_is_visible (child))
    {
        goto finish;
    }

    gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1,
                        &child_minimum_width, &child_natural_width,
                        NULL, NULL);

    gtk_style_context_get_padding (gtk_widget_get_style_context (widget), &padding);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        if (self->max_width != -1)
        {
            if (child_minimum_width > self->max_width)
            {
                g_critical ("%s: child measures %d in width, while the maximum is %d",
                            g_type_name (NAUTILUS_TYPE_CONTAINER_MAX_WIDTH),
                            child_minimum_width, self->max_width);
            }

            child_minimum = child_minimum_width;
            child_natural = MAX (child_minimum,
                                 MIN (self->max_width, child_natural_width));
        }

        child_minimum += padding.left + padding.right;
        child_natural += padding.left + padding.right;
    }
    else
    {
        if (self->max_width != -1)
        {
            child_natural_width = MIN (self->max_width, child_natural_width);
        }

        gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, child_natural_width,
                            &child_minimum, &child_natural,
                            NULL, NULL);

        child_minimum += padding.top + padding.bottom;
        child_natural += padding.top + padding.bottom;
    }

finish:
    if (minimum != NULL)
    {
        *minimum = child_minimum;
    }
    if (natural != NULL)
    {
        *natural = child_natural;
    }
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
                  int           baseline,
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

    widget_class->measure = measure;

    g_object_class_install_property (object_class,
                                     PROP_MAX_WIDTH,
                                     g_param_spec_int ("max-width",
                                                       "Max width",
                                                       "The max width of the container",
                                                       -1,
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

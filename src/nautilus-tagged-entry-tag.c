/* GTK+ 4 implementation of GdTaggedEntry for Nautilus
 * © 2018  Ernestas Kulik <ernestask@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include "nautilus-tagged-entry-tag.h"

struct _NautilusTaggedEntryTag
{
    GtkWidget parent_instance;

    GtkWidget *label;
    GtkWidget *image;

    bool active;
};

G_DEFINE_TYPE (NautilusTaggedEntryTag, nautilus_tagged_entry_tag, GTK_TYPE_WIDGET)

#define SPACING 6

enum
{
    PROP_0,
    PROP_LABEL,
    PROP_SHOW_CLOSE_BUTTON,
    N_PROPERTIES
};

enum
{
    CLICKED,
    BUTTON_CLICKED,
    LAST_SIGNAL
};

static GParamSpec *properties[N_PROPERTIES];
static unsigned int signals[LAST_SIGNAL];

static void
on_multi_press_gesture_pressed (GtkGestureMultiPress *gesture,
                                int                   n_press,
                                double                x,
                                double                y,
                                gpointer              user_data)
{
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_multi_press_gesture_released (GtkGestureMultiPress *gesture,
                                 int                   n_press,
                                 double                x,
                                 double                y,
                                 gpointer              user_data)
{
    g_signal_emit (user_data, signals[CLICKED], 0);
}

static void
set_image_pressed (NautilusTaggedEntryTag *self,
                   bool                    pressed)
{
    self->active = pressed;

    if (pressed)
    {
        gtk_widget_set_state_flags (self->image, GTK_STATE_FLAG_ACTIVE, false);
    }
    else
    {
        gtk_widget_unset_state_flags (self->image, GTK_STATE_FLAG_ACTIVE);
    }
}

static void
on_image_multi_press_gesture_pressed (GtkGestureMultiPress *gesture,
                                      int                   n_press,
                                      double                x,
                                      double                y,
                                      gpointer              user_data)
{
    set_image_pressed (user_data, true);

    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_image_multi_press_gesture_released (GtkGestureMultiPress *gesture,
                                       int                   n_press,
                                       double                x,
                                       double                y,
                                       gpointer              user_data)
{
    set_image_pressed (user_data, false);

    g_signal_emit (user_data, signals[BUTTON_CLICKED], 0);
}

static void
on_image_multi_press_gesture_cancel (GtkGesture       *gesture,
                                     GdkEventSequence *sequence,
                                     gpointer          user_data)
{
    set_image_pressed (user_data, false);
}

static void
on_image_multi_press_gesture_update (GtkGesture       *gesture,
                                     GdkEventSequence *sequence,
                                     gpointer          user_data)
{
    GtkWidget *widget;
    double x;
    double y;
    bool contains;

    widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

    gtk_gesture_get_point (gesture, sequence, &x, &y);

    contains = gtk_widget_contains (widget, x, y);
    if (contains)
    {
        gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, false);
    }
    else
    {
        gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);
    }

    set_image_pressed (user_data, contains);
}

static void
nautilus_tagged_entry_tag_init (NautilusTaggedEntryTag *self)
{
    GtkGesture *gesture;
    GtkStyleContext *context;

    self->label = gtk_label_new (NULL);
    self->image = gtk_image_new_from_icon_name ("window-close-symbolic");
    self->active = false;

    gtk_widget_set_has_surface (GTK_WIDGET (self), false);

    gtk_widget_hide (self->label);
    gtk_widget_insert_after (self->label, GTK_WIDGET (self), NULL);

    gtk_widget_insert_after (self->image, GTK_WIDGET (self), self->label);

    gesture = gtk_gesture_multi_press_new ();
    gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

    g_signal_connect (gesture,
                      "pressed", G_CALLBACK (on_multi_press_gesture_pressed),
                      self);
    g_signal_connect (gesture,
                      "released", G_CALLBACK (on_multi_press_gesture_released),
                      self);

    gesture = gtk_gesture_multi_press_new ();
    gtk_widget_add_controller (self->image, GTK_EVENT_CONTROLLER (gesture));

    g_signal_connect (gesture,
                      "pressed", G_CALLBACK (on_image_multi_press_gesture_pressed),
                      self);
    g_signal_connect (gesture,
                      "released", G_CALLBACK (on_image_multi_press_gesture_released),
                      self);
    g_signal_connect (gesture,
                      "cancel", G_CALLBACK (on_image_multi_press_gesture_cancel),
                      self);
    g_signal_connect (gesture,
                      "update", G_CALLBACK (on_image_multi_press_gesture_update),
                      self);

    context = gtk_widget_get_style_context (GTK_WIDGET (self));
    gtk_style_context_add_class (context, "entry-tag");

    context = gtk_widget_get_style_context (self->image);
    gtk_style_context_add_class (context, "entry-tag");
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusTaggedEntryTag *self;

    self = NAUTILUS_TAGGED_ENTRY_TAG (object);

    switch (property_id)
    {
        case PROP_LABEL:
        {
            const char *string;
            bool empty;

            string = g_value_get_string (value);
            empty = (g_strcmp0 (string, "") == 0);

            gtk_label_set_label (GTK_LABEL (self->label), string);
            gtk_widget_set_visible (self->label, !empty);
        }
        break;

        case PROP_SHOW_CLOSE_BUTTON:
        {
            gtk_widget_set_visible (self->image, g_value_get_boolean (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
get_property (GObject    *object,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
    NautilusTaggedEntryTag *self;

    self = NAUTILUS_TAGGED_ENTRY_TAG (object);

    switch (property_id)
    {
        case PROP_LABEL:
        {
            g_value_set_string (value, gtk_label_get_label (GTK_LABEL (self->label)));
        };
        break;

        case PROP_SHOW_CLOSE_BUTTON:
        {
            g_value_set_boolean (value, gtk_widget_is_visible (self->image));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
finalize (GObject *object)
{
    NautilusTaggedEntryTag *self;

    self = NAUTILUS_TAGGED_ENTRY_TAG (object);

    gtk_widget_unparent (self->label);
    gtk_widget_unparent (self->image);

    G_OBJECT_CLASS (nautilus_tagged_entry_tag_parent_class)->finalize (object);
}

static void
size_allocate (GtkWidget *widget,
               int        width,
               int        height,
               int        baseline)
{
    NautilusTaggedEntryTag *self;
    GtkAllocation label_allocation = { 0 };
    GtkAllocation image_allocation = { 0 };

    self = NAUTILUS_TAGGED_ENTRY_TAG (widget);

    gtk_widget_measure (self->label,
                        GTK_ORIENTATION_HORIZONTAL,
                        -1,
                        &label_allocation.width, NULL,
                        NULL, NULL);
    gtk_widget_measure (self->label,
                        GTK_ORIENTATION_VERTICAL,
                        label_allocation.width,
                        &label_allocation.height, NULL,
                        NULL, NULL);

    label_allocation.y = (height - label_allocation.height) / 2;

    gtk_widget_size_allocate (self->label, &label_allocation, -1);

    image_allocation.x = label_allocation.width + SPACING;

    gtk_widget_measure (self->image,
                        GTK_ORIENTATION_HORIZONTAL,
                        -1,
                        &image_allocation.width, NULL,
                        NULL, NULL);
    gtk_widget_measure (self->image,
                        GTK_ORIENTATION_VERTICAL,
                        image_allocation.width,
                        &image_allocation.height, NULL,
                        NULL, NULL);

    image_allocation.y = (height - image_allocation.height) / 2;

    gtk_widget_size_allocate (self->image, &image_allocation, -1);
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
    NautilusTaggedEntryTag *self;
    int label_minimum;
    int label_natural;
    int image_minimum;
    int image_natural;

    self = NAUTILUS_TAGGED_ENTRY_TAG (widget);

    gtk_widget_measure (self->label, orientation, for_size,
                        &label_minimum, &label_natural,
                        NULL, NULL);
    gtk_widget_measure (self->image, orientation, for_size,
                        &image_minimum, &image_natural,
                        NULL, NULL);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        if (minimum != NULL)
        {
            *minimum = label_minimum + image_minimum;

            if (gtk_widget_is_visible (self->image))
            {
                *minimum += SPACING;
            }
        }
        if (natural != NULL)
        {
            *natural = label_natural + image_natural;

            if (gtk_widget_is_visible (self->image))
            {
                *natural += SPACING;
            }
        }
    }
    else
    {
        if (minimum != NULL)
        {
            *minimum = MAX (label_minimum, image_minimum);
        }
        if (natural != NULL)
        {
            *natural = MAX (label_natural, image_natural);
        }
    }
}

static void
snapshot (GtkWidget   *widget,
          GtkSnapshot *snapshot)
{
    NautilusTaggedEntryTag *self;
    GtkWidgetClass *parent_widget_class;

    self = NAUTILUS_TAGGED_ENTRY_TAG (widget);
    parent_widget_class = GTK_WIDGET_CLASS (nautilus_tagged_entry_tag_parent_class);

    parent_widget_class->snapshot (widget, snapshot);

    gtk_widget_snapshot_child (widget, self->label, snapshot);
    gtk_widget_snapshot_child (widget, self->image, snapshot);
}

static void
nautilus_tagged_entry_tag_class_init (NautilusTaggedEntryTagClass *klass)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;
    GtkCssProvider *provider;
    GdkDisplay *display;

    object_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);
    provider = gtk_css_provider_new ();
    display = gdk_display_get_default ();

    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    widget_class->size_allocate = size_allocate;
    widget_class->measure = measure;
    widget_class->snapshot = snapshot;

    gtk_css_provider_load_from_data (provider,
                                     ".entry-tag {"
                                     "    border-radius: 4px;"
                                     "    padding: 0 6px 0 6px;"
                                     "    margin-left: 0;"
                                     "    margin-right: 0;"
                                     "}"
                                     ".entry-tag label {"
                                     "    color: inherit;"
                                     "}"
                                     /* Reserving space for the border
                                      * to prevent jumping around.
                                      */
                                     ".entry-tag.button:not(:hover),"
                                     ".entry-tag.button:backdrop {"
                                     "    border: 1px solid transparent;"
                                     "}"
                                     ".entry-tag.button {"
                                     "    margin: 0;"
                                     "    padding: 0;"
                                     "}"
                                     ,
                                     -1);

    gtk_style_context_add_provider_for_display (display,
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_USER);

    properties[PROP_LABEL] =
        g_param_spec_string ("label", "Label", "Label",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_SHOW_CLOSE_BUTTON] =
        g_param_spec_boolean ("show-close-button", "Show Close Button", "Toggles close button visibility",
                              true,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[CLICKED] = g_signal_new ("clicked",
                                     G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_FIRST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE,
                                     0);
    signals[BUTTON_CLICKED] = g_signal_new ("button-clicked",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_FIRST,
                                            0,
                                            NULL, NULL,
                                            g_cclosure_marshal_VOID__VOID,
                                            G_TYPE_NONE,
                                            0);
}

void
nautilus_tagged_entry_tag_set_label (NautilusTaggedEntryTag *self,
                                     const char             *label)
{
    g_return_if_fail (NAUTILUS_IS_TAGGED_ENTRY_TAG (self));

    g_object_set (self, "label", label, NULL);
}

void
nautilus_tagged_entry_tag_set_show_close_button (NautilusTaggedEntryTag *self,
                                                 bool                    show_close_button)
{
    g_return_if_fail (NAUTILUS_IS_TAGGED_ENTRY_TAG (self));

    gtk_widget_set_visible (self->image, show_close_button);
}

GtkWidget *
nautilus_tagged_entry_tag_new (const char *label)
{
    return g_object_new (NAUTILUS_TYPE_TAGGED_ENTRY_TAG, "label", label, NULL);
}

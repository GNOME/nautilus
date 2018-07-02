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

#include "nautilus-tagged-entry.h"

#include "nautilus-tagged-entry-tag.h"

struct _NautilusTaggedEntry
{
    GtkSearchEntry parent_instance;

    GtkWidget *box;
};

G_DEFINE_TYPE (NautilusTaggedEntry, nautilus_tagged_entry, GTK_TYPE_SEARCH_ENTRY)

#define SPACING 3

enum
{
    TAG_CLICKED,
    TAG_BUTTON_CLICKED,
    LAST_SIGNAL
};

static unsigned int signals[LAST_SIGNAL];

static void
nautilus_tagged_entry_init (NautilusTaggedEntry *self)
{
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (GTK_WIDGET (self));

    gtk_style_context_add_class (context, "tagged-entry");

    self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, SPACING);

    gtk_widget_set_cursor_from_name (self->box, "default");
    gtk_widget_set_parent (self->box, GTK_WIDGET (self));
    gtk_widget_set_vexpand (self->box, true);
}

static void
finalize (GObject *object)
{
    NautilusTaggedEntry *self;

    self = NAUTILUS_TAGGED_ENTRY (object);

    gtk_widget_unparent (self->box);

    G_OBJECT_CLASS (nautilus_tagged_entry_parent_class)->finalize (object);
}

static void
size_allocate (GtkWidget           *widget,
               const GtkAllocation *allocation,
               int                  baseline)
{
    NautilusTaggedEntry *self;
    GtkWidgetClass *parent_widget_class;
    GtkAllocation box_allocation = { 0 };
    GtkAllocation entry_allocation = { 0 };
    g_autoptr (GList) children = NULL;

    self = NAUTILUS_TAGGED_ENTRY (widget);
    parent_widget_class = GTK_WIDGET_CLASS (nautilus_tagged_entry_parent_class);
    box_allocation = *allocation;
    entry_allocation = *allocation;
    children = gtk_container_get_children (GTK_CONTAINER (self->box));

    gtk_widget_measure (self->box,
                        GTK_ORIENTATION_HORIZONTAL,
                        -1,
                        &box_allocation.width, NULL,
                        NULL, NULL);

    box_allocation.x = allocation->width - box_allocation.width;

    if (children != NULL)
    {
        GtkStyleContext *context;
        GtkBorder padding = { 0 };

        context = gtk_widget_get_style_context (widget);

        gtk_style_context_get_padding (context, &padding);

        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        {
            box_allocation.width += padding.right;
        }
        else
        {
            box_allocation.width += padding.left;
        }
    }

    entry_allocation.width -= box_allocation.width;

    parent_widget_class->size_allocate (widget, &entry_allocation, -1);

    gtk_widget_size_allocate (self->box, &box_allocation, -1);
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
    NautilusTaggedEntry *self;
    GtkWidgetClass *parent_widget_class;
    int entry_minimum;
    int entry_natural;
    int box_minimum;
    int box_natural;

    self = NAUTILUS_TAGGED_ENTRY (widget);
    parent_widget_class = GTK_WIDGET_CLASS (nautilus_tagged_entry_parent_class);

    parent_widget_class->measure (widget, orientation, for_size,
                                  &entry_minimum, &entry_natural,
                                  NULL, NULL);

    gtk_widget_measure (self->box, orientation, for_size,
                        &box_minimum, &box_natural,
                        NULL, NULL);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        if (minimum != NULL)
        {
            *minimum = entry_minimum + box_minimum;
        }
        if (natural != NULL)
        {
            *natural = entry_natural + box_minimum;
        }
    }
    else
    {
        if (minimum != NULL)
        {
            *minimum = entry_minimum;
        }
        if (natural != NULL)
        {
            *natural = entry_natural;
        }
    }
}

static void
snapshot (GtkWidget   *widget,
          GtkSnapshot *snapshot)
{
    NautilusTaggedEntry *self;

    self = NAUTILUS_TAGGED_ENTRY (widget);

    GTK_WIDGET_CLASS (nautilus_tagged_entry_parent_class)->snapshot (widget, snapshot);

    gtk_widget_snapshot_child (widget, self->box, snapshot);
}

static void
nautilus_tagged_entry_class_init (NautilusTaggedEntryClass *klass)
{
    GObjectClass *object_class;
    GtkWidgetClass *widget_class;
    GtkCssProvider *provider;
    GdkDisplay *display;

    object_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);
    provider = gtk_css_provider_new ();
    display = gdk_display_get_default ();

    object_class->finalize = finalize;

    widget_class->size_allocate = size_allocate;
    widget_class->measure = measure;
    widget_class->snapshot = snapshot;

    gtk_widget_class_set_css_name (widget_class, "entry");

    gtk_css_provider_load_from_data (provider,
                                     ".tagged-entry entry {"
                                     "    background: transparent;"
                                     "    border: none;"
                                     "    box-shadow: none;"
                                     "}"
                                     ,
                                     -1);

    gtk_style_context_add_provider_for_display (display,
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_USER);

    signals[TAG_CLICKED] = g_signal_new ("tag-clicked",
                                         G_TYPE_FROM_CLASS (klass),
                                         G_SIGNAL_RUN_FIRST,
                                         0,
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE,
                                         1,
                                         NAUTILUS_TYPE_TAGGED_ENTRY);
    signals[TAG_BUTTON_CLICKED] = g_signal_new ("tag-button-clicked",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_FIRST,
                                                0,
                                                NULL, NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1,
                                                NAUTILUS_TYPE_TAGGED_ENTRY);
}

static void
on_tag_clicked (NautilusTaggedEntryTag *tag,
                gpointer                user_data)
{
    g_signal_emit (user_data, signals[TAG_CLICKED], 0, tag);
}

static void
on_tag_button_clicked (NautilusTaggedEntryTag *tag,
                       gpointer                user_data)
{
    g_signal_emit (user_data, signals[TAG_BUTTON_CLICKED], 0, tag);
}

void
nautilus_tagged_entry_add_tag (NautilusTaggedEntry    *self,
                               NautilusTaggedEntryTag *tag)
{
    unsigned long handler_id;

    g_return_if_fail (NAUTILUS_IS_TAGGED_ENTRY (self));
    g_return_if_fail (NAUTILUS_IS_TAGGED_ENTRY_TAG (tag));

    gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (tag));

    handler_id = g_signal_connect (tag,
                                   "clicked", G_CALLBACK (on_tag_clicked),
                                   self);
    g_object_set_data (G_OBJECT (tag),
                       "clicked-handler-id", GUINT_TO_POINTER (handler_id));

    handler_id = g_signal_connect (tag,
                                   "button-clicked", G_CALLBACK (on_tag_button_clicked),
                                   self);
    g_object_set_data (G_OBJECT (tag),
                       "button-clicked-handler-id", GUINT_TO_POINTER (handler_id));
}

void
nautilus_tagged_entry_remove_tag (NautilusTaggedEntry    *self,
                                  NautilusTaggedEntryTag *tag)
{
    GtkWidget *parent;
    gpointer data;
    unsigned long handler_id;

    g_return_if_fail (NAUTILUS_IS_TAGGED_ENTRY (self));
    g_return_if_fail (NAUTILUS_IS_TAGGED_ENTRY_TAG (tag));

    parent = gtk_widget_get_parent (GTK_WIDGET (tag));

    g_return_if_fail (parent != GTK_WIDGET (self));

    data = g_object_get_data (G_OBJECT (tag), "clicked-handler-id");
    handler_id = GPOINTER_TO_UINT (data);
    g_signal_handler_disconnect (tag, handler_id);

    data = g_object_get_data (G_OBJECT (tag), "button-clicked-handler-id");
    handler_id = GPOINTER_TO_UINT (data);
    g_signal_handler_disconnect (tag, handler_id);

    gtk_container_remove (GTK_CONTAINER (self->box), GTK_WIDGET (tag));
}

GtkWidget *
nautilus_tagged_entry_new (void)
{
    return g_object_new (NAUTILUS_TYPE_TAGGED_ENTRY, NULL);
}

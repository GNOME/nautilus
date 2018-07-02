/* Copyright (C) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "eel-event.h"

struct _EelEvent
{
    GObject parent_instance;

    GdkEventType type;

    GdkSurface *surface;

    gdouble x;
    gdouble y;

    guint button;

    GdkModifierType state;

    guint32 time;
};

G_DEFINE_TYPE (EelEvent, eel_event, G_TYPE_OBJECT)

static void
eel_event_init (EelEvent *self)
{
    self->type = GDK_NOTHING;
    self->surface = NULL;
    self->x = 0.0;
    self->y = 0.0;
    self->button = 0;
    self->state = 0;
    self->time = 0;
}

static void
eel_event_class_init (EelEventClass *klass)
{
}

GdkEventType
eel_event_get_event_type (EelEvent *self)
{
    g_return_val_if_fail (EEL_IS_EVENT (self), GDK_NOTHING);

    return self->type;
}

void
eel_event_set_event_type (EelEvent     *self,
                          GdkEventType  type)
{
    g_return_if_fail (EEL_IS_EVENT (self));
    g_return_if_fail (type >= GDK_NOTHING && type < GDK_EVENT_LAST);

    self->type = type;
}

GdkSurface *
eel_event_get_surface (EelEvent *self)
{
    g_return_val_if_fail (EEL_IS_EVENT (self), NULL);

    return self->surface;
}

void
eel_event_set_surface (EelEvent   *self,
                       GdkSurface *surface)
{
    g_return_if_fail (EEL_IS_EVENT (self));
    g_return_if_fail (GDK_IS_SURFACE (surface));

    self->surface = surface;
}

void
eel_event_get_coords (EelEvent *self,
                      gdouble  *x,
                      gdouble  *y)
{
    g_return_if_fail (EEL_IS_EVENT (self));

    if (x != NULL)
    {
        *x = self->x;
    }
    if (y != NULL)
    {
        *y = self->y;
    }
}

void
eel_event_set_coords (EelEvent *self,
                      gdouble   x,
                      gdouble   y)
{
    g_return_if_fail (EEL_IS_EVENT (self));

    self->x = x;
    self->y = y;
}

guint
eel_event_get_button (EelEvent *self)
{
    g_return_val_if_fail (EEL_IS_EVENT (self), 0);

    return self->button;
}

GdkModifierType
eel_event_get_state (EelEvent *self)
{
    g_return_val_if_fail (EEL_IS_EVENT (self), 0);

    return self->state;
}

void
eel_event_set_state (EelEvent        *self,
                     GdkModifierType  state)
{
    g_return_if_fail (EEL_IS_EVENT (self));

    self->state = state;
}

guint32
eel_event_get_time (EelEvent *self)
{
    g_return_val_if_fail (EEL_IS_EVENT (self), 0);

    return self->time;
}

EelEvent *
eel_event_copy (EelEvent *self)
{
    EelEvent *event;

    g_return_val_if_fail (EEL_IS_EVENT (self), NULL);

    event = eel_event_new ();

    event->type = self->type;
    event->surface = self->surface;
    event->x = self->x;
    event->y = self->y;
    event->button = self->button;
    event->state = self->state;
    event->time = self->time;

    return event;
}

EelEvent *
eel_event_new (void)
{
    return g_object_new (EEL_TYPE_EVENT, NULL);
}

EelEvent *
eel_event_new_from_gdk_event (const GdkEvent *gdk_event)
{
    EelEvent *event;

    g_return_val_if_fail (gdk_event != NULL, NULL);

    event = eel_event_new ();

    event->type = gdk_event_get_event_type (gdk_event);
    event->surface = gdk_event_get_surface (gdk_event);
    event->time = gdk_event_get_time (gdk_event);

    gdk_event_get_coords (gdk_event, &event->x, &event->y);
    gdk_event_get_button (gdk_event, &event->button);
    gdk_event_get_state (gdk_event, &event->state);

    return event;
}

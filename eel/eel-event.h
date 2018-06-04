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

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define EEL_TYPE_EVENT (eel_event_get_type ())

G_DECLARE_FINAL_TYPE (EelEvent, eel_event, EEL, EVENT, GObject)

GdkEventType     eel_event_get_event_type     (EelEvent        *event);
void             eel_event_set_event_type     (EelEvent        *event,
                                               GdkEventType     type);

GdkWindow       *eel_event_get_window         (EelEvent        *event);
void             eel_event_set_window         (EelEvent        *event,
                                               GdkWindow       *window);

void             eel_event_get_coords         (EelEvent        *event,
                                               gdouble         *x,
                                               gdouble         *y);
void             eel_event_set_coords         (EelEvent        *event,
                                               gdouble          x,
                                               gdouble          y);

guint            eel_event_get_button         (EelEvent        *event);
void             eel_event_set_button         (EelEvent        *event,
                                               guint            button);

GdkModifierType  eel_event_get_state          (EelEvent        *event);
void             eel_event_set_state          (EelEvent        *event,
                                               GdkModifierType  state);

guint32          eel_event_get_time           (EelEvent        *event);

EelEvent        *eel_event_copy               (EelEvent        *event);

EelEvent        *eel_event_new                (void);
EelEvent        *eel_event_new_from_gdk_event (const GdkEvent  *gdk_event);

G_END_DECLS

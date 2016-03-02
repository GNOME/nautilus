/* nautilus-action-bar.h
 *
 * Copyright (C) 2016 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_ACTION_BAR_H
#define NAUTILUS_ACTION_BAR_H

#include <gtk/gtk.h>

#include "nautilus-view.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_ACTION_BAR (nautilus_action_bar_get_type())

G_DECLARE_FINAL_TYPE (NautilusActionBar, nautilus_action_bar, NAUTILUS, ACTION_BAR, GtkFrame)

GtkWidget*           nautilus_action_bar_new                     (NautilusView      *view);

G_END_DECLS

#endif /* NAUTILUS_ACTION_BAR_H */

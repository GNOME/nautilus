/* nautilus-bottom-toolbar.c
 *
 * Copyright 2020 Christopher Davis <christopherdavis@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-bottom-toolbar.h"

#include <math.h>

#include "nautilus-navigation-direction.h"
#include "nautilus-window.h"

struct _NautilusBottomToolbar
{
    GtkRevealer parent_instance;

    NautilusWindow *window;
};

G_DEFINE_TYPE (NautilusBottomToolbar, nautilus_bottom_toolbar, GTK_TYPE_REVEALER);

static void
nautilus_bottom_toolbar_class_init (NautilusBottomToolbarClass *klass)
{
    // GObjectClass *oclass;
    GtkWidgetClass *widget_class;

    widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-bottom-toolbar.ui");
}

static void
nautilus_bottom_toolbar_init (NautilusBottomToolbar *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

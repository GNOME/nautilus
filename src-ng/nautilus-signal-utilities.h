/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#ifndef NAUTILUS_SIGNAL_UTILITIES_H_INCLUDED
#define NAUTILUS_SIGNAL_UTILITIES_H_INCLUDED

#include <glib.h>

void nautilus_emit_signal_in_main_context_va_list (gpointer      instance,
                                                   GMainContext *main_context,
                                                   guint         signal_id,
                                                   GQuark        detail,
                                                   va_list       ap);
void nautilus_emit_signal_in_main_context_by_name (gpointer      instance,
                                                   GMainContext *main_context,
                                                   const gchar  *detailed_signal,
                                                   ...);
void nautilus_emit_signal_in_main_context         (gpointer      instance,
                                                   GMainContext *main_context,
                                                   guint         signal_id,
                                                   GQuark        detail,
                                                   ...);

#endif

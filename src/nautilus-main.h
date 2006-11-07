/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* nautilus-main.c
 */

#ifndef NAUTILUS_MAIN_H
#define NAUTILUS_MAIN_H

#include <gtk/gtktypeutils.h>

void     nautilus_main_event_loop_register    (GtkObject *object);
gboolean nautilus_main_is_event_loop_mainstay (GtkObject *object);
void     nautilus_main_event_loop_quit        (gboolean explicit);

#endif /* NAUTILUS_MAIN_H */


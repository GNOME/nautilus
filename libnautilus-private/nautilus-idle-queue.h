/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libnautilus: A library for nautilus view implementations.
 *
 *  Copyright (C) 2001 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef NAUTILUS_IDLE_QUEUE_H
#define NAUTILUS_IDLE_QUEUE_H

#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef struct NautilusIdleQueue NautilusIdleQueue;

NautilusIdleQueue *nautilus_idle_queue_new     (void);
void               nautilus_idle_queue_add     (NautilusIdleQueue *queue,
						GFunc              callback,
						gpointer           data,
						gpointer           callback_data,
						GFreeFunc          free_callback_data);
void               nautilus_idle_queue_destroy (NautilusIdleQueue *queue);

G_END_DECLS

#endif /* NAUTILUS_IDLE_QUEUE_H */

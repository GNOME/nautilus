/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Handy Queue classes
 * 
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Rebecca Schulman <rebecka@eazel.com>
 *  
 */

/* nautilus-queue.h -- A simple FIFO structure */

#ifndef NAUTILUS_QUEUE_H
#define NAUTILUS_QUEUE_H

#include <glib.h>

typedef struct _NautilusQueue NautilusQueue;

NautilusQueue *  nautilus_queue_new           (void);
/* Enqueue */
void             nautilus_queue_add           (NautilusQueue *queue,
                                               gpointer element);
/* Dequeue */
gpointer         nautilus_queue_remove        (NautilusQueue *queue);

void             nautilus_queue_free_deep     (NautilusQueue *queue);

gboolean         nautilus_queue_is_empty      (NautilusQueue *queue);

void             nautilus_queue_free          (NautilusQueue *queue);


					     
#endif /* NAUTILUS_QUEUE_H */

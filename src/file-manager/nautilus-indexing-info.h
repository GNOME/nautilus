/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  nautilus-indexing-info.h: Indexing Info for the search service
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
 *  Author: George Lebl <jirka@5z.com>
 *
 */

#ifndef NAUTILUS_INDEXING_INFO_H
#define NAUTILUS_INDEXING_INFO_H

/* Show the indexing info dialog,  If one is already running
 * just raise that one.
 */
void  nautilus_indexing_info_show_dialog         (void);
char *nautilus_indexing_info_get_last_index_time (void);

#endif /* NAUTILUS_INDEXING_INFO_H */

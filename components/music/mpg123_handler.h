/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * (C) 1998, 1999 John Ellis
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: John Ellis
 * integrated with Nautilus by Andy Hertzfeld
 *
 */

/* header file for the mpg123 handler for playing mp3 files */

#define	STATUS_STOP 0
#define	STATUS_PAUSE 1
#define	STATUS_PLAY 2
#define	STATUS_NEXT 3

void	start_playing_file (gchar* filename, gboolean start_from_beginning);
void	stop_playing_file (void);
void	pause_playing_file (void);
int	get_play_status(void);
int	get_current_frame(void);
void	set_current_frame(int new_frame);
	
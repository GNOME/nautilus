/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sound.h: manage the sound playing process and other sound utilities
 
   Copyright (C) 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Authors: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_SOUND_H
#define NAUTILUS_SOUND_H

#include <glib.h>
#include <sys/wait.h>

void		nautilus_sound_initialize (void);
gboolean	nautilus_sound_can_play_sound (void);
void		nautilus_sound_kill_sound (void);
void		nautilus_sound_register_sound (pid_t sound_process);

#endif /* NAUTILUS_SOUND_H */

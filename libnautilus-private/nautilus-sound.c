/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sound.c: manage the sound playing process and other sound utilities
  
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <esd.h>

#include "nautilus-global-preferences.h"
#include "nautilus-preferences.h"
#include "nautilus-sound.h"

static void
kill_sound_if_necessary (void)
{
	pid_t child;
	int status_result;
	pid_t sound_process;
	
	/* fetch the sound state */
	sound_process = nautilus_preferences_get_integer (NAUTILUS_PREFERENCES_CURRENT_SOUND_STATE);
	
	/* if there was a sound playing, kill it */
	if (sound_process > 0) {
		kill (-sound_process, SIGTERM);
 		child = waitpid (sound_process, &status_result, 0);
	}
}

/* initialize_sound is called at application start up time.  It puts the sound system
   into a quiescent state */
void
nautilus_sound_initialize (void)
{ 	
 	nautilus_preferences_set_integer (NAUTILUS_PREFERENCES_CURRENT_SOUND_STATE, 0); 	
}

/* if there is a sound registered, kill it, and register the empty sound */
void
nautilus_sound_kill_sound (void)
{
	/* if there is a sound in progress, kill it */
	kill_sound_if_necessary ();
		
	/* set the process state to quiescent */
 	nautilus_preferences_set_integer (NAUTILUS_PREFERENCES_CURRENT_SOUND_STATE, 0);
}

/* register a new sound process, including kill any old one if necessary */
void
nautilus_sound_register_sound (pid_t sound_process)
{
	/* if there is a sound in progress, kill it */
	kill_sound_if_necessary ();
	
	/* record the new sound process ID */
	nautilus_preferences_set_integer (NAUTILUS_PREFERENCES_CURRENT_SOUND_STATE, sound_process);
}

/* This function does two things. First it checks to see a sound is currently playing.  If it is,
 * it returns the process id of the external application playing the sound. If no sound is playing,
 * it return the value set in nautilus_sound_initialize() when system audio output capabilites
 * were queried.
 */
gboolean
nautilus_sound_can_play_sound (void)
{
	int sound_process, open_result;
	
	/* first see if there's already one in progress; if so, return true */
	sound_process = nautilus_preferences_get_integer (NAUTILUS_PREFERENCES_CURRENT_SOUND_STATE);
	if (sound_process > 0) {
		return TRUE;
	}
		
	/* Now check and see if system has audio out capabilites */
	open_result = esd_open_sound (NULL);
	if (open_result == -1) {
 		return FALSE;
	} else {
		esd_close (open_result);
		return TRUE;
	}
}


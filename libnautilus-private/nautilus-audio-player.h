/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-audio-player.h - Simple threaded audio file playback.

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Gene Z. Ragan <gzr@eazel.com>
*/

#ifndef NAUTILUS_AUDIO_PLAYER__
#define NAUTILUS_AUDIO_PLAYER__

#include <config.h>
#include <sys/types.h>

#include <audiofile.h>
#include <pthread.h>
#include <glib.h>

typedef struct {
	gboolean use_remote;
	gchar *server;
	gint port;
	gint buffer_size;
	gint prebuffer;
} ESDConfig;

typedef struct {
	AFfilehandle handle;
	pthread_t player_id;
	gboolean running;
	ESDConfig esd_config;
} NautilusAudioPlayerData;


NautilusAudioPlayerData	*nautilus_audio_player_play	(const char			*filename);
void			nautilus_audio_player_stop 	(NautilusAudioPlayerData 	*data);


#endif

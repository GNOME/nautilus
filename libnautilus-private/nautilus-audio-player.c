/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-audio-player.c - Simple threaded audio file playback.

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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <esd.h>

#include "nautilus-audio-player.h"

/* BUFFER_FRAMES represents the size of the buffer in frames. */
#define BUFFER_FRAMES 		4096

#define PLAYER_STREAM_NAME 	"nautilus-audio-player"

typedef enum {
	FORMAT_U8, 
	FORMAT_S8, 
	FORMAT_U16_LE, 
	FORMAT_U16_BE, 
	FORMAT_U16_NE,
	FORMAT_S16_LE, 
	FORMAT_S16_BE, 
	FORMAT_S16_NE
}
AudioFormat;

typedef struct {
	pthread_t buffer_thread;
	gint fd;
	gpointer buffer;
	gboolean going, prebuffer, paused;
	gint buffer_size, prebuffer_size, block_size;
	gint rd_index, wr_index;
	gint output_time_offset;
	guint64 written, output_bytes;
	gint bps, ebps;
	gint flush;
	gint channels, frequency, latency;
	AudioFormat format;
	esd_format_t esd_format;
	gint input_bps, input_format, input_frequency, input_channels;
	char *hostname;
	ESDConfig esd_config;
	void *(*esd_translate)(void *, gint);
} ESDInfo;


static gboolean esdout_open 		(ESDInfo 			*info,
				 	 AudioFormat 			format,
				 	 gint 				rate,
				 	 gint 				nch);
static void 	esdout_close 		(ESDInfo 			*info);
static void 	esdout_set_audio_params	(ESDInfo 			*info);
static void 	esdout_write 		(ESDInfo 			*info,
					 gpointer 			data,
					 int 				length);
static int	esdout_used 		(ESDInfo 			*info);
static gboolean esdout_playing 		(ESDInfo 			*info);

static void *
player_thread (void *arg)
{
	NautilusAudioPlayerData *data;
	AFframecount frames_read;
	int sample_format, frame_size, channel_count, sample_width;
	double rate;
	void *buffer;	
	ESDInfo info;

	data = arg;
	
	if (data == NULL) {
		pthread_exit (NULL);
		return (void *) 0;
	}
	
	/* Read information from file */
	afGetSampleFormat (data->handle, AF_DEFAULT_TRACK, &sample_format, &sample_width);
	frame_size = afGetFrameSize (data->handle, AF_DEFAULT_TRACK, 1);
	channel_count = afGetChannels (data->handle, AF_DEFAULT_TRACK);
	rate = afGetRate (data->handle, AF_DEFAULT_TRACK);
	
	/* Attempt to open ESD */
	if (!esdout_open (&info, sample_width == 16 ? FORMAT_S16_NE : FORMAT_U8, (int)rate, channel_count)) {
		pthread_exit (NULL);		
		return (void *) 0;
	}

	/* Read audio data from file and send it to the esd output thread */
	buffer = malloc (BUFFER_FRAMES * frame_size);
	frames_read = afReadFrames (data->handle, AF_DEFAULT_TRACK, buffer, BUFFER_FRAMES);
	while (frames_read > 0 && data->running) {		
		esdout_write (&info, buffer, frames_read * frame_size);
		frames_read = afReadFrames (data->handle, AF_DEFAULT_TRACK, buffer, BUFFER_FRAMES);
	}
	afCloseFile (data->handle);
	
	/* Now wait for the esd output thread to complete it task */
	while (esdout_playing (&info) && data->running) {
		usleep (20000);
	}
		
	/* Shutdown esd output thread */
	esdout_close (&info);

	g_free (buffer);
	
	pthread_exit (NULL);

	return (void *) 0;
}

NautilusAudioPlayerData *
nautilus_audio_player_play (const char *filename)
{
	AFfilehandle handle;
	NautilusAudioPlayerData *data;
	
	handle = afOpenFile (filename, "r", NULL);
	if (handle == AF_NULL_FILEHANDLE) {
		return NULL;
	}
	
	data = g_new0 (NautilusAudioPlayerData, 1);
	data->handle = handle;
	data->running = TRUE;
	
	pthread_create (&data->player_id, NULL, player_thread, data);
	
	return data;
}

void
nautilus_audio_player_stop (NautilusAudioPlayerData *data)
{	
	if (data == NULL) {
		return;
	}
	
	data->running = FALSE;
	pthread_join (data->player_id, NULL);
}


static void 
esdout_init (ESDInfo *info)
{
	memset (&info->esd_config, 0, sizeof (ESDConfig));
	
	info->fd = 0;
	info->going = FALSE;
	info->paused = FALSE;
	info->buffer = NULL;
	info->block_size = BUFFER_FRAMES;
	info->rd_index = 0;
	info->wr_index = 0;
	info->output_time_offset = 0;
	info->written = 0;
	info->output_bytes = 0;
	info->hostname = NULL;
	info->esd_config.port = ESD_DEFAULT_PORT;
	info->esd_config.buffer_size = 3000;
	info->esd_config.prebuffer = 25;
}

static void
esdout_write (ESDInfo *info, gpointer data, int length)
{
	int count, offset;
		
	offset = 0;
	
	info->written += length;
	
	while (length > 0) {
		count = MIN (length, info->buffer_size - info->wr_index);
		memcpy ((char *)info->buffer + info->wr_index, (char *)data + offset, count);
		info->wr_index = (info->wr_index + count) % info->buffer_size;
		length -= count;
		offset += count;
	}
}


static gint 
get_latency (ESDInfo *config)
{
	int fd, amount = 0;

#ifndef HAVE_ESD_GET_LATENCY
	esd_server_info_t *info;
#endif

	fd = esd_open_sound (config->hostname);
	if (fd == -1) {
		return 0;
	}

#ifdef HAVE_ESD_GET_LATENCY
	amount = get_latency (fd);
#else
	info = esd_get_server_info (fd);
	if (info != NULL) {
		if (info->format & ESD_STEREO) {
			if (info->format & ESD_BITS16)
				amount = (44100 * (ESD_BUF_SIZE + 64)) / info->rate;
			else
				amount = (44100 * (ESD_BUF_SIZE + 128)) / info->rate;
		} else {
			if (info->format & ESD_BITS16)
				amount = (2 * 44100 * (ESD_BUF_SIZE + 128)) / info->rate;
			else
				amount = (2 * 44100 * (ESD_BUF_SIZE + 256)) / info->rate;
		}
		free (info);
	}
	amount += ESD_BUF_SIZE * 2;
#endif
	esd_close (fd);
	return amount;
}

static void *
esd_stou8 (void *data, gint length)
{
	int len = length;
	unsigned char *dat = (unsigned char *)data;
	while (len-- > 0)
		*dat++ ^= 0x80;
	return data;
}

static void *
esd_utos16sw (void *data, gint length)
{
	int len = length;
	short *dat = data;
	while ( len > 0 ) {
		*dat = GUINT16_SWAP_LE_BE ( *dat ) ^ 0x8000;
		dat++;
		len-=2;
	}
	return data;
}

static void *
esd_utos16 (void *data, gint length)
{
	int len = length;
	short *dat = data;
	while ( len > 0 ) {
		*dat ^= 0x8000;
		dat++;
		len-=2;
	}
	return data;
}

static void *
esd_16sw (void *data, gint length)
{
	int len = length;
	short *dat = data;
	while ( len > 0 ) {
		*dat = GUINT16_SWAP_LE_BE( *dat );
		dat++;
		len-=2;
	}
	return data;
}

static void 
esdout_setup_format (ESDInfo *info, AudioFormat format, gint rate, gint nch)
{
	gboolean swap_sign = FALSE;
	gboolean swap_16 = FALSE;

	info->format = format;
	info->frequency = rate;
	info->channels = nch;
	
	switch (format) {
		case FORMAT_S8:
			swap_sign = TRUE;
		case FORMAT_U8:
			info->esd_format = ESD_BITS8;
			break;
		case FORMAT_U16_LE:
		case FORMAT_U16_BE:
		case FORMAT_U16_NE:
			swap_sign = TRUE;
		case FORMAT_S16_LE:
		case FORMAT_S16_BE:
		case FORMAT_S16_NE:
			info->esd_format = ESD_BITS16;
			break;
	}

#ifdef WORDS_BIGENDIAN
	if (format == FORMAT_U16_LE || format == FORMAT_S16_LE) {
#else
	if (format == FORMAT_U16_BE || format == FORMAT_S16_BE) {
#endif
		swap_16 = TRUE;
	}

	info->esd_translate = (void*(*)())NULL;
	if (info->esd_format == ESD_BITS8) {
		if (swap_sign == TRUE) {
			info->esd_translate = esd_stou8;
		}
	} else {
		if (swap_sign == TRUE) {
			if (swap_16 == TRUE) {
				info->esd_translate = esd_utos16sw;
			} else {
				info->esd_translate = esd_utos16;
			}
		} else {
			if (swap_16 == TRUE) {
				info->esd_translate = esd_16sw;
			}
		}
	}

	info->bps = rate * nch;
	if (info->esd_format == ESD_BITS16) {
		info->bps *= 2;
	}
	
	if (nch == 1) {
		info->esd_format |= ESD_MONO;
	} else {
		info->esd_format |= ESD_STEREO;
	}
	
	info->esd_format |= ESD_STREAM | ESD_PLAY;
	
	info->latency = ((get_latency (info) * info->frequency) / 44100) * info->channels;
	if (info->format != FORMAT_U8 && info->format != FORMAT_S8) {
		info->latency *= 2;
	}
}
	

static gint 
esdout_used (ESDInfo *info)
{
	if (info->wr_index >= info->rd_index) {
		return info->wr_index - info->rd_index;
	}
	
	return info->buffer_size - (info->rd_index - info->wr_index);
}

static void 
esdout_write_audio (ESDInfo *info, gint length)
{
	AudioFormat new_format;
	gint new_frequency, new_channels;
	char *data;
		
	data = (char *)info->buffer + info->rd_index;
	
	new_format = info->input_format;
	new_frequency = info->input_frequency;
	new_channels = info->input_channels;
		
	if (new_format != info->format || new_frequency != info->frequency || new_channels != info->channels) {
		info->output_time_offset += (gint) ((info->output_bytes * 1000) / info->ebps);
		info->output_bytes = 0;
		esdout_setup_format (info, new_format, new_frequency, new_channels);
		info->frequency = new_frequency;
		info->channels = new_channels;
		close (info->fd);
		esdout_set_audio_params (info);
	}
	
        if (info->esd_translate) {
                info->output_bytes += write (info->fd, info->esd_translate (data, length), length);
	} else {
		info->output_bytes += write (info->fd, data, length);
	}
}


static void 
esdout_close (ESDInfo *info)
{	
	info->going = FALSE;
	info->wr_index = 0;
	info->rd_index = 0;
	info->going = 0;
	g_free (info->hostname);
	info->hostname = NULL;
	pthread_join (info->buffer_thread, NULL);
}

static void *
esdout_loop (void *arg)
{
	int length, count, used;
	ESDInfo *info;
	
	info = arg;

	while (info->going) {
		used = esdout_used (info);
		
		if (used > info->prebuffer_size) {
			info->prebuffer = FALSE;
		}
		
		if (used > 0 && !info->paused && !info->prebuffer) {
			length = MIN (info->block_size, used);
			while (length > 0) {
				count = MIN (length, info->buffer_size - info->rd_index);
				esdout_write_audio (info, count);
				info->rd_index = (info->rd_index + count) % info->buffer_size;
				length -= count;				
			}
		} else {
			usleep (10000);
		}

		if (info->flush != -1) {
			info->output_time_offset = info->flush;
			info->written = (guint64)(info->flush / 10) * (guint64)(info->input_bps / 100);
			info->rd_index = info->wr_index = info->output_bytes = 0;
			info->flush = -1;
			info->prebuffer = TRUE;
		}
	}

	close (info->fd);
	g_free (info->buffer);

	while (info->going) {
		usleep (10000);	
	}
	
	pthread_exit (NULL);
	
	return (void *) 0;
}

static void 
esdout_set_audio_params (ESDInfo *info)
{
	info->fd = esd_play_stream (info->esd_format, info->frequency, info->hostname, PLAYER_STREAM_NAME);
	info->ebps = info->frequency * info->channels;
	if (info->format == FORMAT_U16_BE || info->format == FORMAT_U16_LE || info->format == FORMAT_S16_BE
	    || info->format == FORMAT_S16_LE || info->format == FORMAT_S16_NE || info->format == FORMAT_U16_NE) {
		info->ebps *= 2;
	}
}

static gboolean
esdout_open (ESDInfo *info, AudioFormat format, gint rate, gint nch)
{	
	esdout_init (info);
		
	esdout_setup_format (info, format, rate, nch);
	
	info->input_format = info->format;
	info->input_channels = info->channels;
	info->input_frequency = info->frequency;
	info->input_bps = info->bps;

	info->buffer_size = (info->esd_config.buffer_size * info->input_bps) / 1000;	
	if (info->buffer_size < 8192) {
		info->buffer_size = 8192;
	}
	
	info->prebuffer_size = (info->buffer_size * info->esd_config.prebuffer) / 100;
	if (info->buffer_size - info->prebuffer_size < 4096) {
		info->prebuffer_size = info->buffer_size - 4096;
	}

	info->buffer = g_malloc0 (info->buffer_size);
	
	info->flush = -1;
	info->prebuffer = TRUE;
	info->wr_index = info->rd_index = info->output_time_offset = info->written = info->output_bytes = 0;
	info->paused = FALSE;

	if (info->hostname != NULL) {
		g_free (info->hostname);
	}
	
	if (info->esd_config.use_remote) {
		info->hostname = g_strdup_printf ("%s:%d", info->esd_config.server, info->esd_config.port);
	} else {
		info->hostname = NULL;
	}
	
	esdout_set_audio_params (info);
	if (info->fd == -1) {
		g_free (info->buffer);
		info->buffer = NULL;
		return FALSE;
	}
	
	info->going = TRUE;

	pthread_create (&info->buffer_thread, NULL, esdout_loop, info);
	
	return TRUE;
}

static gboolean 
esdout_playing (ESDInfo *info)
{
	if (!info->going) {
		return FALSE;
	}
		
	if (esdout_used (info) <= 0) {
		return FALSE;
	}
	
	return TRUE;
}



#include <config.h>
#include <pthread.h>

#include "esd-audio.h"
#include "config.h"

static gint fd = 0;
static gpointer buffer;
static gboolean going = FALSE, prebuffer, paused = FALSE, remove_prebuffer = FALSE;
static gint buffer_size, prebuffer_size, blk_size = 4096;
static gint rd_index = 0, wr_index = 0;
static gint output_time_offset = 0;
static guint64 written = 0, output_bytes = 0;
static gint bps, ebps;
static gint flush;
static gint channels, frequency, latency;
static AFormat format;
static esd_format_t esd_format;
static gint input_bps, input_format, input_frequency, input_channels;
static gchar *hostname;
static pthread_t buffer_thread;
static void *(*esd_translate)(void *, gint);


ESDConfig esd_cfg;
esd_info_t *all_info;
esd_player_info_t *player_info;

static void 
esdout_init (void)
{
	memset (&esd_cfg, 0, sizeof (ESDConfig));
	esd_cfg.port = ESD_DEFAULT_PORT;
	esd_cfg.buffer_size = 3000;
	esd_cfg.prebuffer = 25;
}

static gint 
get_latency(void)
{
	int fd, amount = 0;

#ifndef HAVE_ESD_GET_LATENCY
	esd_server_info_t *info;
#endif

	fd = esd_open_sound (hostname);
	if (fd == -1) {
		return 0;
	}

#ifdef HAVE_ESD_GET_LATENCY
	amount = esd_get_latency(fd);
#else
	info = esd_get_server_info(fd);
	if (info) {
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
		free(info);
	}
	amount += ESD_BUF_SIZE * 2;
#endif
	esd_close (fd);
	return amount;
}

static void *
esd_stou8(void *data, gint length)
{
	int len = length;
	unsigned char *dat = (unsigned char *)data;
	while (len-- > 0)
		*dat++ ^= 0x80;
	return data;
}

static void *esd_utos16sw(void *data, gint length)
{
	int len = length;
	short *dat = data;
	while ( len > 0 ) {
		*dat = GUINT16_SWAP_LE_BE( *dat ) ^ 0x8000;
		dat++;
		len-=2;
	}
	return data;
}

static void *esd_utos16(void *data, gint length)
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

static void *esd_16sw(void *data, gint length)
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
esdout_setup_format (AFormat fmt, gint rate, gint nch)
{
	gboolean swap_sign = FALSE;
	gboolean swap_16 = FALSE;

	format = fmt;
	frequency = rate;
	channels = nch;
	switch (fmt)
	{
		case FMT_S8:
			swap_sign = TRUE;
		case FMT_U8:
			esd_format = ESD_BITS8;
			break;
		case FMT_U16_LE:
		case FMT_U16_BE:
		case FMT_U16_NE:
			swap_sign = TRUE;
		case FMT_S16_LE:
		case FMT_S16_BE:
		case FMT_S16_NE:
			esd_format = ESD_BITS16;
			break;
	}

#ifdef WORDS_BIGENDIAN
	if (fmt == FMT_U16_LE || fmt == FMT_S16_LE)
#else
	if (fmt == FMT_U16_BE || fmt == FMT_S16_BE)
#endif
		swap_16 = TRUE;

	esd_translate = (void*(*)())NULL;
	if (esd_format == ESD_BITS8) {
		if (swap_sign == TRUE)
			esd_translate = esd_stou8;
	} else {
		if (swap_sign == TRUE) {
			if (swap_16 == TRUE)
				esd_translate = esd_utos16sw;
			else
				esd_translate = esd_utos16;
		} else {
			if (swap_16 == TRUE)
				esd_translate = esd_16sw;
		}
	}

	bps = rate * nch;
	if (esd_format == ESD_BITS16)
		bps *= 2;
	if(nch == 1)
		esd_format |= ESD_MONO;
	else
		esd_format |= ESD_STEREO;
	esd_format |= ESD_STREAM | ESD_PLAY;
	
	latency = ((get_latency() * frequency) / 44100) * channels;
	if (format != FMT_U8 && format != FMT_S8)
		latency *= 2;
}
	

gint 
esdout_get_written_time (void)
{
	if (!going)
		return 0;
	return (gint) ((written * 1000) / input_bps);
}

gint 
esdout_get_output_time (void)
{
	guint64 bytes;

	if (!fd || !going) {
		return 0;
	}

	bytes = output_bytes;
	if (!paused) {
		bytes -= (bytes < (guint64)latency ? bytes : (guint64)latency);
	}
	
	return output_time_offset + (gint) ((bytes * 1000) / ebps);
}

gint 
esdout_used (void)
{
	if (wr_index >= rd_index) {
		return wr_index - rd_index;
	}
	return buffer_size - (rd_index - wr_index);
}

gint 
esdout_playing (void)
{
	int fd;
	
	fd = esd_open_sound (hostname);
	if (fd == -1) {
		return TRUE;
	}
	esd_close (fd);
	
	if (!going) {
		return FALSE;
	}
	
	if (!esdout_used ()) {
		return FALSE;
	}
			
	return TRUE;
}

gint 
esdout_free (void)
{
	if (remove_prebuffer && prebuffer) {
		prebuffer = FALSE;
		remove_prebuffer = FALSE;
	}
	
	if (prebuffer) {
		remove_prebuffer = TRUE;
	}

	if (rd_index > wr_index) {
		return (rd_index - wr_index) - 1;
	}
	
	return (buffer_size - (wr_index - rd_index)) - 1;
}

static void 
esdout_write_audio (gpointer data, gint length)
{
	AFormat new_format;
	gint new_frequency,new_channels;
	
	new_format = input_format;
	new_frequency = input_frequency;
	new_channels = input_channels;
		
	if (new_format != format || new_frequency != frequency || new_channels != channels) {
		output_time_offset += (gint) ((output_bytes * 1000) / ebps);
		output_bytes = 0;
		esdout_setup_format(new_format, new_frequency, new_channels);
		frequency = new_frequency;
		channels = new_channels;
		close(fd);
		esdout_set_audio_params();
	}
	
        if (esd_translate) {
                output_bytes += write(fd,esd_translate(data,length),length);
	} else {
		output_bytes += write(fd,data,length);
	}
}


void 
esdout_write (gpointer ptr, gint length)
{
	gint cnt, off = 0;

	remove_prebuffer = FALSE;

	written += length;
	while (length > 0) {
		cnt = MIN(length, buffer_size - wr_index);
		memcpy((gchar *)buffer + wr_index, (gchar *)ptr + off, cnt);
		wr_index = (wr_index + cnt) % buffer_size;
		length -= cnt;
		off += cnt;

	}
}

void 
esdout_close (void)
{
	if (!going) {
		return;
	}
	
	wr_index = 0;
	rd_index = 0;
	going = 0;
	g_free (hostname);
	hostname = NULL;
	pthread_join (buffer_thread, NULL);
}

void 
esdout_flush (gint time)
{
	flush = time;
	while (flush != -1) {
		usleep(10000);
	}
}

void 
esdout_pause (gboolean p)
{
	paused = p;
}

static void *
esdout_loop (void *arg)
{
	gint length, cnt;
	
	while (going) {
		if (esdout_used () > prebuffer_size) {
			prebuffer = FALSE;
		}
		
		if (esdout_used () > 0 && !paused && !prebuffer) {
			length = MIN (blk_size, esdout_used ());
			while (length > 0) {
				cnt = MIN(length,buffer_size-rd_index);
				esdout_write_audio ((gchar *)buffer + rd_index, cnt);
				rd_index=(rd_index+cnt)%buffer_size;
				length-=cnt;				
			}
		} else {
			usleep (10000);
		}

		if (flush != -1) {
			output_time_offset = flush;
			written = (guint64)(flush / 10) * (guint64)(input_bps / 100);
			rd_index = wr_index = output_bytes = 0;
			flush = -1;
			prebuffer = TRUE;
		}
	}

	close (fd);
	g_free (buffer);
	pthread_exit (NULL);
	return NULL;  /* make gcc happy */
}

void 
esdout_set_audio_params(void)
{
	/* frequency = GUINT16_SWAP_LE_BE( frequency ); */
	fd = esd_play_stream (esd_format, frequency, hostname, "nautilus-music-view");
	ebps = frequency * channels;
	if (format == FMT_U16_BE || format == FMT_U16_LE || format == FMT_S16_BE || format == FMT_S16_LE || format == FMT_S16_NE || format == FMT_U16_NE)
		ebps *= 2;
}

gint
esdout_open (AFormat fmt, gint rate, gint nch)
{	
	esdout_init ();
	
	esdout_setup_format (fmt,rate,nch);
	
	input_format = format;
	input_channels = channels;
	input_frequency = frequency;
	input_bps = bps;

	buffer_size = (esd_cfg.buffer_size * input_bps) / 1000;
	if (buffer_size < 8192)
		buffer_size = 8192;
	prebuffer_size = (buffer_size * esd_cfg.prebuffer) / 100;
	if (buffer_size - prebuffer_size < 4096)
		prebuffer_size = buffer_size - 4096;

	buffer = g_malloc0(buffer_size);
	
	flush = -1;
	prebuffer = 1;
	wr_index = rd_index = output_time_offset = written = output_bytes = 0;
	paused = FALSE;
	remove_prebuffer = FALSE;

	if (hostname)
		g_free (hostname);
	if (esd_cfg.use_remote)
		hostname = g_strdup_printf("%s:%d", esd_cfg.server, esd_cfg.port);
	else
		hostname = NULL;
	
	esdout_set_audio_params ();
	if (fd == -1) {
		g_free(buffer);
		return 0;
	}
	going = 1;

	pthread_create (&buffer_thread, NULL, esdout_loop, NULL);
	
	return 1;
}

#ifndef ESD_AUDIO_H
#define ESD_AUDIO_H

#include <gnome.h>
#include <esd.h>
#include <config.h>

typedef struct {
	gboolean use_remote;
	gchar *server;
	gint port;
	gint buffer_size;
	gint prebuffer;
}
ESDConfig;

extern ESDConfig esd_cfg;

typedef enum {
	FMT_U8, 
	FMT_S8, 
	FMT_U16_LE, 
	FMT_U16_BE, 
	FMT_U16_NE, 
	FMT_S16_LE, 
	FMT_S16_BE, 
	FMT_S16_NE
}
AFormat;

gint 	esdout_open (AFormat fmt, gint rate, gint nch);
void 	esdout_set_audio_params (void);
gint 	esdout_get_written_time (void);
gint 	esdout_used (void);
gint 	esdout_playing (void);
gint 	esdout_free (void);
void 	esdout_write (gpointer ptr, gint length);
void 	esdout_close (void);
void 	esdout_flush (gint time);
void 	esdout_pause (gboolean p);
gint 	esdout_get_output_time (void);

/* esd-mixer.c */
void esdout_get_volume(int *l, int *r);
void esdout_set_volume(int l, int r);

#endif




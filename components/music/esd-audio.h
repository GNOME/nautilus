#ifndef ESD_AUDIO_H
#define ESD_AUDIO_H

#include <esd.h>
#include <glib/gtypes.h>

typedef struct {
	gboolean use_remote;
	char *server;
	int port;
	int buffer_size;
	int prebuffer;
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

int 	esdout_open (AFormat fmt, int rate, int nch);
void 	esdout_set_audio_params (void);
int 	esdout_get_written_time (void);
int 	esdout_used (void);
int 	esdout_playing (void);
int 	esdout_free (void);
void 	esdout_write (gpointer ptr, int length);
void 	esdout_close (void);
void 	esdout_flush (int time);
void 	esdout_pause (gboolean p);
int 	esdout_get_output_time (void);

/* esd-mixer.c */
void esdout_get_volume(int *l, int *r);
void esdout_set_volume(int l, int r);

#endif




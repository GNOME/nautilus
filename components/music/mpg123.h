/*
 * mpg123 defines 
 * used source: musicout.h from mpegaudio package
 */

#ifndef __MPG123_H__
#define __MPG123_H__

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#include <math.h>

#include <gtk/gtk.h>

#include "dxhead.h"
#include "id3.h"

#define real float

/*  #define         MAX_NAME_SIZE           81 */
#define         SBLIMIT                 32
#define         SCALE_BLOCK             12
#define         SSLIMIT                 18

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

/* Pre Shift fo 16 to 8 bit converter table */
#define AUSHIFT (3)

struct id3v1tag_t {
	char tag[3]; /* always "TAG": defines ID3v1 tag 128 bytes before EOF */
	char title[30];
	char artist[30];
	char album[30];
	char year[4];
	char comment[28];
	char fill;
	char track;
	unsigned char genre;
};

struct id3tag_t {
	char title[64];
	char artist[64];
	char album[64];
	char year[5];
	char comment[256];
	char genre[256];
	char track[4];
};

typedef struct
{
	gboolean going, paused;	
	gint num_frames, eof, jump_to_time, eq_active;
	gint songtime;
	gdouble tpf;
	gfloat eq_mul[576];
	gboolean output_audio;
	gboolean first_frame;
	guint32 filesize;	/* Filesize without junk */
}
PlayerInfo;

void mpg123_set_eq(int on, float preamp, float *band);
void mpg123_file_info_box(char *);

extern PlayerInfo *mpg123_info;

struct al_table
{
	short bits;
	short d;
};

struct frame
{
	struct al_table *alloc;
	int (*synth) (real *, int, unsigned char *, int *);
	int (*synth_mono) (real *, unsigned char *, int *);
#ifdef USE_3DNOW
	void (*dct36)(real *,real *,real *,real *,real *);
#endif
	int stereo;
	int jsbound;
	int single;
	int II_sblimit;
	int down_sample_sblimit;
	int lsf;
	int mpeg25;
	int down_sample;
	int header_change;
	int lay;
	int (*do_layer) (struct frame * fr);
	int error_protection;
	int bitrate_index;
	int sampling_frequency;
	int padding;
	int extension;
	int mode;
	int mode_ext;
	int copyright;
	int original;
	int emphasis;
	int framesize;		/* computed framesize */
};

void mpg123_configure(void);

typedef struct
{
	gint resolution;
	gint channels;
	gint downsample;
	gint downsample_custom;
	gint http_buffer_size;
	gint http_prebuffer;
	gboolean use_proxy;
	gchar *proxy_host;
	gint proxy_port;
	gboolean proxy_use_auth;
	gchar *proxy_user, *proxy_pass;
	gboolean save_http_stream;
	gchar *save_http_path;
	gboolean cast_title_streaming;
	gboolean use_udp_channel;
	gchar *id3_format;
	gboolean use_id3, disable_id3v2;
	gboolean detect_by_content;
}
MPG123Config;

extern MPG123Config mpg123_cfg;

struct bitstream_info
{
	int bitindex;
	unsigned char *wordpointer;
};

extern struct bitstream_info bsi;

/* ------ Declarations from "http.c" ------ */

extern int mpg123_http_open(char *url);
int mpg123_http_read(gpointer data, gint length);
void mpg123_http_close(void);
gchar *mpg123_http_get_title(gchar * url);
gint mpg123_http_get_icy_br(void);

/* ------ Declarations from "common.c" ------ */
extern unsigned int mpg123_get1bit(void);
extern unsigned int mpg123_getbits(int);
extern unsigned int mpg123_getbits_fast(int);

extern void mpg123_open_stream(char *bs_filenam, int fd);
extern int mpg123_head_check(unsigned long);
extern void mpg123_stream_close(void);

extern void mpg123_set_pointer(long);

extern unsigned char *mpg123_pcm_sample;
extern int mpg123_pcm_point;

struct gr_info_s
{
	int scfsi;
	unsigned part2_3_length;
	unsigned big_values;
	unsigned scalefac_compress;
	unsigned block_type;
	unsigned mixed_block_flag;
	unsigned table_select[3];
	unsigned subblock_gain[3];
	unsigned maxband[3];
	unsigned maxbandl;
	unsigned maxb;
	unsigned region1start;
	unsigned region2start;
	unsigned preflag;
	unsigned scalefac_scale;
	unsigned count1table_select;
	real *full_gain[3];
	real *pow2gain;
};

struct III_sideinfo
{
	unsigned main_data_begin;
	unsigned private_bits;
	struct
	{
		struct gr_info_s gr[2];
	}
	ch[2];
};

extern void open_stream(char *, int fd);
extern long mpg123_tell_stream(void);
extern void mpg123_read_frame_init(void);
extern int mpg123_read_frame(struct frame *fr);
extern int mpg123_back_frame(struct frame *fr, int num);
void mpg123_stream_jump_to_frame(struct frame *fr, int frame);
void mpg123_stream_jump_to_byte(struct frame *fr, int byte);
int mpg123_stream_check_for_xing_header(struct frame *fr, XHEADDATA * xhead);
int mpg123_calc_numframes(struct frame *fr);

extern int mpg123_do_layer3(struct frame *fr);
extern int mpg123_do_layer2(struct frame *fr);
extern int mpg123_do_layer1(struct frame *fr);

#ifdef I386_ASSEM
extern int mpg123_synth_1to1_pent(real *, int, unsigned char *);

#endif
extern int mpg123_synth_1to1(real *, int, unsigned char *, int *);
extern int mpg123_synth_1to1_8bit(real *, int, unsigned char *, int *);
extern int mpg123_synth_1to1_mono(real *, unsigned char *, int *);
extern int mpg123_synth_1to1_mono2stereo(real *, unsigned char *, int *);
extern int mpg123_synth_1to1_8bit_mono(real *, unsigned char *, int *);
extern int mpg123_synth_1to1_8bit_mono2stereo(real *, unsigned char *, int *);

extern int mpg123_synth_2to1(real *, int, unsigned char *, int *);
extern int mpg123_synth_2to1_8bit(real *, int, unsigned char *, int *);
extern int mpg123_synth_2to1_mono(real *, unsigned char *, int *);
extern int mpg123_synth_2to1_mono2stereo(real *, unsigned char *, int *);
extern int mpg123_synth_2to1_8bit_mono(real *, unsigned char *, int *);
extern int mpg123_synth_2to1_8bit_mono2stereo(real *, unsigned char *, int *);

extern int mpg123_synth_4to1(real *, int, unsigned char *, int *);
extern int mpg123_synth_4to1_8bit(real *, int, unsigned char *, int *);
extern int mpg123_synth_4to1_mono(real *, unsigned char *, int *);
extern int mpg123_synth_4to1_mono2stereo(real *, unsigned char *, int *);
extern int mpg123_synth_4to1_8bit_mono(real *, unsigned char *, int *);
extern int mpg123_synth_4to1_8bit_mono2stereo(real *, unsigned char *, int *);

extern int mpg123_synth_ntom(real *, int, unsigned char *, int *);
extern int mpg123_synth_ntom_8bit(real *, int, unsigned char *, int *);
extern int mpg123_synth_ntom_mono(real *, unsigned char *, int *);
extern int mpg123_synth_ntom_mono2stereo(real *, unsigned char *, int *);
extern int mpg123_synth_ntom_8bit_mono(real *, unsigned char *, int *);
extern int mpg123_synth_ntom_8bit_mono2stereo(real *, unsigned char *, int *);

/* 3DNow! optimizations */
#ifdef USE_3DNOW
extern int mpg123_getcpuflags(void);
extern int support_3dnow(void);
extern void dct36(real *,real *,real *,real *,real *);
extern void dct36_3dnow(real *,real *,real *,real *,real *);
extern int mpg123_synth_1to1_3dnow(real *,int,unsigned char *,int *);
#endif

extern void mpg123_rewindNbits(int bits);
extern int mpg123_hsstell(void);
extern void mpg123_set_pointer(long);
extern void mpg123_huffman_decoder(int, int *);
extern void mpg123_huffman_count1(int, int *);
extern int mpg123_get_songlen(struct frame *fr, int no);

extern void mpg123_init_layer3(int);
extern void mpg123_init_layer2(void);
extern void mpg123_make_decode_tables(long scale);
extern void mpg123_make_conv16to8_table(void);
extern void mpg123_dct64(real *, real *, real *);

extern void mpg123_synth_ntom_set_step(long, long);

int mpg123_decode_header(struct frame *fr, unsigned long newhead);
double mpg123_compute_bpf(struct frame *fr);
double mpg123_compute_tpf(struct frame *fr);
guint mpg123_strip_spaces(char *src, size_t n);
void mpg123_get_id3v2(id3_t * id3d, struct id3tag_t *tag);
gchar *mpg123_format_song_title(struct id3tag_t *tag, gchar *filename);
void mpg123_id3v1_to_id3v2 (struct id3v1tag_t *v1, struct id3tag_t *v2);



extern unsigned char *mpg123_conv16to8;
extern long mpg123_freqs[9];
extern real mpg123_muls[27][64];
extern real mpg123_decwin[512 + 32];
extern real *mpg123_pnts[5];

#define GENRE_MAX 0x94
extern const gchar *mpg123_id3_genres[GENRE_MAX];
extern int tabsel_123[2][3][16];


void get_song_info (char *filename, char **title_real, int *len_real);
void mpg123_play_file (const char *filename);
void mpg123_stop (void);
void mpg123_seek (int time);
void mpg123_pause (gboolean pause);
int get_time (void);

#endif

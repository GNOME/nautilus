
#include "mpg123.h"
//#include "audio.h"
#include "esd-audio.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

static long outscale = 32768;

static struct frame fr, temp_fr;

PlayerInfo *mpg123_info = NULL;
static pthread_t decode_thread;
MPG123Config mpg123_cfg;

static gboolean vbr_set = FALSE, have_xing_header = FALSE;
static XHEADDATA xing_header;
static unsigned char xing_toc[100];
static gboolean audio_error = FALSE;
gint mpg123_bitrate, mpg123_frequency, mpg123_length, mpg123_layer, mpg123_lsf;
gchar *mpg123_title = NULL, *mpg123_filename = NULL;
static gint disp_bitrate, skip_frames = 0;
gboolean mpg123_stereo, mpg123_mpeg25;
gint mpg123_mode;

const gchar *mpg123_id3_genres[GENRE_MAX] =
{
	N_("Blues"), N_("Classic Rock"), N_("Country"), N_("Dance"),
	N_("Disco"), N_("Funk"), N_("Grunge"), N_("Hip-Hop"),
	N_("Jazz"), N_("Metal"), N_("New Age"), N_("Oldies"),
	N_("Other"), N_("Pop"), N_("R&B"), N_("Rap"), N_("Reggae"),
	N_("Rock"), N_("Techno"), N_("Industrial"), N_("Alternative"),
	N_("Ska"), N_("Death Metal"), N_("Pranks"), N_("Soundtrack"),
	N_("Euro-Techno"), N_("Ambient"), N_("Trip-Hop"), N_("Vocal"),
	N_("Jazz+Funk"), N_("Fusion"), N_("Trance"), N_("Classical"),
	N_("Instrumental"), N_("Acid"), N_("House"), N_("Game"),
	N_("Sound Clip"), N_("Gospel"), N_("Noise"), N_("Alt"),
	N_("Bass"), N_("Soul"), N_("Punk"), N_("Space"),
	N_("Meditative"), N_("Instrumental Pop"),
	N_("Instrumental Rock"), N_("Ethnic"), N_("Gothic"),
	N_("Darkwave"), N_("Techno-Industrial"), N_("Electronic"),
	N_("Pop-Folk"), N_("Eurodance"), N_("Dream"),
	N_("Southern Rock"), N_("Comedy"), N_("Cult"),
	N_("Gangsta Rap"), N_("Top 40"), N_("Christian Rap"),
	N_("Pop/Funk"), N_("Jungle"), N_("Native American"),
	N_("Cabaret"), N_("New Wave"), N_("Psychedelic"), N_("Rave"),
	N_("Showtunes"), N_("Trailer"), N_("Lo-Fi"), N_("Tribal"),
	N_("Acid Punk"), N_("Acid Jazz"), N_("Polka"), N_("Retro"),
	N_("Musical"), N_("Rock & Roll"), N_("Hard Rock"), N_("Folk"),
	N_("Folk/Rock"), N_("National Folk"), N_("Swing"),
	N_("Fast-Fusion"), N_("Bebob"), N_("Latin"), N_("Revival"),
	N_("Celtic"), N_("Bluegrass"), N_("Avantgarde"),
	N_("Gothic Rock"), N_("Progressive Rock"),
	N_("Psychedelic Rock"), N_("Symphonic Rock"), N_("Slow Rock"),
	N_("Big Band"), N_("Chorus"), N_("Easy Listening"),
	N_("Acoustic"), N_("Humour"), N_("Speech"), N_("Chanson"),
	N_("Opera"), N_("Chamber Music"), N_("Sonata"), N_("Symphony"),
	N_("Booty Bass"), N_("Primus"), N_("Porn Groove"),
	N_("Satire"), N_("Slow Jam"), N_("Club"), N_("Tango"),
	N_("Samba"), N_("Folklore"), N_("Ballad"), N_("Power Ballad"),
	N_("Rhythmic Soul"), N_("Freestyle"), N_("Duet"),
	N_("Punk Rock"), N_("Drum Solo"), N_("A Cappella"),
	N_("Euro-House"), N_("Dance Hall"), N_("Goa"),
	N_("Drum & Bass"), N_("Club-House"), N_("Hardcore"),
	N_("Terror"), N_("Indie"), N_("BritPop"), N_("Negerpunk"),
	N_("Polsk Punk"), N_("Beat"), N_("Christian Gangsta Rap"),
	N_("Heavy Metal"), N_("Black Metal"), N_("Crossover"),
	N_("Contemporary Christian"), N_("Christian Rock"),
	N_("Merengue"), N_("Salsa"), N_("Thrash Metal"),
	N_("Anime"), N_("JPop"), N_("Synthpop")
};

double mpg123_compute_tpf(struct frame *fr)
{
	static int bs[4] =
	{0, 384, 1152, 1152};
	double tpf;

	tpf = (double) bs[fr->lay];
	tpf /= mpg123_freqs[fr->sampling_frequency] << (fr->lsf);
	return tpf;
}

static void 
set_mpg123_synth_functions(struct frame *fr)
{
	typedef int (*func) (real *, int, unsigned char *, int *);
	typedef int (*func_mono) (real *, unsigned char *, int *);
	int ds = fr->down_sample;
	int p8 = 0;

	static func funcs[2][4] =
	{
		{mpg123_synth_1to1,
		 mpg123_synth_2to1,
		 mpg123_synth_4to1,
		 mpg123_synth_ntom},
		{mpg123_synth_1to1_8bit,
		 mpg123_synth_2to1_8bit,
		 mpg123_synth_4to1_8bit,
		 mpg123_synth_ntom_8bit}
	};

	static func_mono funcs_mono[2][2][4] =
	{
		{
			{mpg123_synth_1to1_mono2stereo,
			 mpg123_synth_2to1_mono2stereo,
			 mpg123_synth_4to1_mono2stereo,
			 mpg123_synth_ntom_mono2stereo},
			{mpg123_synth_1to1_8bit_mono2stereo,
			 mpg123_synth_2to1_8bit_mono2stereo,
			 mpg123_synth_4to1_8bit_mono2stereo,
			 mpg123_synth_ntom_8bit_mono2stereo}},
		{
			{mpg123_synth_1to1_mono,
			 mpg123_synth_2to1_mono,
			 mpg123_synth_4to1_mono,
			 mpg123_synth_ntom_mono},
			{mpg123_synth_1to1_8bit_mono,
			 mpg123_synth_2to1_8bit_mono,
			 mpg123_synth_4to1_8bit_mono,
			 mpg123_synth_ntom_8bit_mono}}
	};

	if (mpg123_cfg.resolution == 8)
		p8 = 1;
	fr->synth = funcs[p8][ds];
	fr->synth_mono = funcs_mono[1][p8][ds];

	if (p8) {
		mpg123_make_conv16to8_table();
	}
}

static void 
mpg123_init (void)
{
	mpg123_make_decode_tables(outscale);

	mpg123_cfg.resolution = 16;
	mpg123_cfg.channels = 2;
	mpg123_cfg.downsample = 0;
	mpg123_cfg.downsample_custom = 44100;
	mpg123_cfg.proxy_port = 8080;
	mpg123_cfg.proxy_use_auth = FALSE;
	mpg123_cfg.proxy_user = NULL;
	mpg123_cfg.proxy_pass = NULL;
	mpg123_cfg.cast_title_streaming = FALSE;
	mpg123_cfg.use_udp_channel = TRUE;
	mpg123_cfg.use_id3 = TRUE;
	mpg123_cfg.disable_id3v2 = FALSE;
	mpg123_cfg.detect_by_content = FALSE;
	mpg123_cfg.id3_format = g_strdup("%1 - %2");
	mpg123_cfg.proxy_host = g_strdup("localhost");
}

static guint32 
convert_to_header (guint8 * buf)
{

	return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
}

static void 
play_frame (struct frame *fr)
{
	if (fr->error_protection) {
		bsi.wordpointer += 2;
		/*  mpg123_getbits(16); */	/* skip crc */
	}
	
	if(!fr->do_layer(fr)) {
		skip_frames = 2;
		mpg123_info->output_audio = FALSE;
	} else {
		if(!skip_frames)
			mpg123_info->output_audio = TRUE;
		else
			skip_frames--;
	}
}

/* as defined in WinAmp's Nullsoft Nitrane pllugin preferences
   default is "%1 - %2" (Artist - Title), %% = '%' */
enum id3_format_codes
{
	ID3_ARTIST = '1', ID3_TITLE, ID3_ALBUM, ID3_YEAR,
	ID3_COMMENT, ID3_GENRE, FILE_NAME, FILE_PATH,
	FILE_EXT
};

static const gchar *get_id3_genre(unsigned char genre_code)
{
	return (genre_code < GENRE_MAX) ? mpg123_id3_genres[genre_code] : "";
};

guint mpg123_strip_spaces(char *src, size_t n)
/* strips trailing spaces from string of length n
   returns length of adjusted string */
{
	gchar *space = NULL,	/* last space in src */
	     *start = src;

	while (n--)
		switch (*src++)
		{
			case '\0':
				n = 0;	/* breaks out of while loop */

				src--;
				break;
			case ' ':
				if (space == NULL)
					space = src - 1;
				break;
			default:
				space = NULL;	/* don't terminate intermediate spaces */

				break;
		}
	if (space != NULL)
	{
		src = space;
		*src = '\0';
	}
	return src - start;
}

/*
 * Function extname (filename)
 *
 *    Return pointer within filename to its extenstion, or NULL if
 *    filename has no extension.
 *
 */
static gchar *extname(const char *filename)
{
	gchar *ext = strrchr(filename, '.');

	if (ext != NULL)
		++ext;

	return ext;
}

static gchar *eval_id3_format(const char *id3_format, struct id3tag_t *id3,
			      const char *filename)
/* returns ID3 and filename data as specified in format
   as in Nullsoft's Nitrane plugin v1.31b (their MPEG decoder) */
{
	gchar *ans, c, *base, *path, *ext;
	guint length = 0, allocated, baselen, pathlen, extlen, tmp;
	const size_t alloc_size = 256;	/* size of memory block allocations */
	gboolean got_field = FALSE;

	ans = g_malloc(allocated = alloc_size);
	pathlen = strlen(path = g_dirname(filename));
	base = g_strdup(g_basename(filename));
	if ((ext = extname(base)) == NULL)
	{
		ext = "";
		extlen = 0;
	}
	else
	{
		*(ext - 1) = '\0';
		extlen = strlen(ext);
	}
	baselen = strlen(base);
	while ((c = *id3_format++) != '\0')
	{
		tmp = 1;
		if (c == '%')
		{
			switch (*id3_format++)
			{
				case 0:
					id3_format--;	/* otherwise we'll lose terminator */

				case '%':
					ans[length] = '%';
					break;
				case ID3_ARTIST:
					tmp = strlen(id3->artist);
					if(tmp != 0)
						got_field = TRUE;
					strncpy(&ans[length], id3->artist, tmp);
					break;
				case ID3_TITLE:
					tmp = strlen(id3->title);
					if(tmp != 0)
						got_field = TRUE;
					strncpy(&ans[length], id3->title, tmp);
					break;
				case ID3_ALBUM:
					tmp = strlen(id3->album);
					if(tmp != 0)
						got_field = TRUE;
					strncpy(&ans[length], id3->album, tmp);
					break;
				case ID3_YEAR:
					tmp = strlen(id3->year);
					if(tmp != 0)
						got_field = TRUE;
					strncpy(&ans[length], id3->year, tmp);
					break;
				case ID3_COMMENT:
					tmp = strlen(id3->comment);					
					if(tmp != 0)
						got_field = TRUE;
					strncpy(&ans[length], id3->comment, tmp);
					break;
				case ID3_GENRE:					
					tmp = strlen(id3->genre);
					if(tmp != 0)
						got_field = TRUE;
					strncpy(&ans[length], id3->genre, tmp);
					break;
				case FILE_NAME:
					strncpy(&ans[length], base, tmp = baselen);
					got_field = TRUE;
					break;
				case FILE_PATH:
					strncpy(&ans[length], path, tmp = pathlen);
					got_field = TRUE;
					break;
				case FILE_EXT:
					strncpy(&ans[length], ext, tmp = extlen);
					got_field = TRUE;
					break;
				default:
					ans[length] = c;
					break;
			}
		}
		else
			ans[length] = c;
		ans[length += tmp] = '\0';
		if (allocated - length <= 30)
			ans = g_realloc(ans, allocated += alloc_size);
	}
	ans = g_realloc(ans, length + 1);
	if(!got_field)
	{
		g_free(ans);
		ans = g_strdup(base);
	}
	g_free(base);
	g_free(path);
	return ans;
}

/*
 * Function id3v1_to_id3v2 (v1, v2)
 *
 *    Convert ID3v1 tag `v1' to ID3v2 tag `v2'.
 *
 */

void 
mpg123_id3v1_to_id3v2 (struct id3v1tag_t *v1, struct id3tag_t *v2)
{
	memset(v2,0,sizeof(struct id3tag_t));
	strncpy(v2->title, v1->title, 30);
	strncpy(v2->artist, v1->artist, 30);
	strncpy(v2->album, v1->album, 30);
	strncpy(v2->year, v1->year, 4);
	strncpy(v2->comment, v1->comment, 28);
	g_snprintf(v2->track, sizeof v2->track, "%d", v1->track);
	strncpy(v2->genre, get_id3_genre(v1->genre), sizeof (v2->genre));
	mpg123_strip_spaces(v2->title, 30);
	mpg123_strip_spaces(v2->artist, 30);
	mpg123_strip_spaces(v2->album, 30);
	mpg123_strip_spaces(v2->year, 4);
	mpg123_strip_spaces(v2->comment, 30);
	mpg123_strip_spaces(v2->genre, sizeof (v2->genre));
}

/*
 * Function mpg123_format_song_title (tag, filename)
 *
 *    Create song title according to `tag' and/or `filename' and
 *    return it.  The title must be subsequently freed using g_free().
 *
 */
gchar *mpg123_format_song_title(struct id3tag_t *tag, gchar * filename)
{
	gchar *ret = NULL;

	if (mpg123_cfg.use_id3 && tag)
	{
		/*
		 * Format according to tag.
		 */
		ret = eval_id3_format(mpg123_cfg.id3_format,
				      tag, filename);
	}

	if (!ret)
	{
		/*
		 * Format according to filename.
		 */
		ret = g_strdup(g_basename(filename));
		if (extname(ret) != NULL)
			*(extname(ret) - 1) = '\0';	/* removes period */
	}

	return ret;
}

/*
 * Function mpg123_get_id3v2 (id3d, tag)
 *
 *    Get desired contents from the indicated id3tag and store it in
 *    `tag'. 
 *
 */
void mpg123_get_id3v2(id3_t * id3d, struct id3tag_t *tag)
{
	id3_frame_t *id3frm;
	char *txt;
	int tlen;

#define ID3_SET(_tid,_fld) 						\
{									\
	id3frm = id3_get_frame( id3d, _tid, 1 );			\
	if (id3frm) {							\
		txt = _tid == ID3_TCON ? id3_get_content(id3frm)	\
		    : id3_get_text(id3frm);				\
		if(txt)							\
		{							\
			tlen = strlen(txt);				\
			if ( tlen >= (int)sizeof(tag->_fld) ) 		\
				tlen = sizeof(tag->_fld)-1;		\
			strncpy( tag->_fld, txt, tlen );		\
			tag->_fld[tlen] = 0;				\
		}							\
		else							\
			tag->_fld[0] = 0;				\
	} else {							\
		tag->_fld[0] = 0;					\
	}								\
}
	ID3_SET(ID3_TIT2, title);
	ID3_SET(ID3_TPE1, artist);
	ID3_SET(ID3_TALB, album);
	ID3_SET(ID3_TYER, year);
	ID3_SET(ID3_TXXX, comment);
	ID3_SET(ID3_TCON, genre);
}

/*
 * Function get_song_title (fd, filename)
 *
 *    Get song title of file.  File position of `fd' will be
 *    clobbered.  `fd' may be NULL, in which case `filename' is opened
 *    separately.  The returned song title must be subsequently freed
 *    using g_free().
 *
 */
static gchar *get_song_title(FILE * fd, char *filename)
{
	FILE *file = fd;
	char *ret = NULL;
	struct id3v1tag_t id3v1tag;
	struct id3tag_t id3tag;

	if (file || (file = fopen(filename, "rb")) != 0)
	{
		id3_t *id3 = NULL;

		/*
		 * Try reading ID3v2 tag.
		 */
		if (!mpg123_cfg.disable_id3v2)
		{
			fseek(file, 0, SEEK_SET);
			id3 = id3_open_fp(file, 0);
			if (id3)
			{
				mpg123_get_id3v2(id3, &id3tag);
				ret = mpg123_format_song_title(&id3tag, filename);
				id3_close(id3);
			}
		}

		/*
		 * Try reading ID3v1 tag.
		 */
		if (!id3 && (fseek(file, -1 * sizeof (id3v1tag), SEEK_END) == 0) &&
		    (fread(&id3v1tag, 1, sizeof (id3v1tag), file) == sizeof (id3v1tag)) &&
		    (strncmp(id3v1tag.tag, "TAG", 3) == 0))
		{
			mpg123_id3v1_to_id3v2(&id3v1tag, &id3tag);
			ret = mpg123_format_song_title(&id3tag, filename);
		}

		if (!fd)
			/*
			 * File was opened in this function.
			 */
			fclose(file);
	}

	if (ret == NULL)
		/*
		 * Unable to get ID3 tag.
		 */
		ret = mpg123_format_song_title(NULL, filename);

	return ret;
}

static guint
get_song_time (FILE * file)
{
	guint32 head;
	guchar tmp[4], *buf;
	struct frame frm;
	XHEADDATA xing_header;
	double tpf, bpf;
	guint32 len;

	if (!file)
		return -1;

	fseek(file, 0, SEEK_SET);
	if (fread(tmp, 1, 4, file) != 4)
		return 0;
	head = convert_to_header(tmp);
	while (!mpg123_head_check(head))
	{
		head <<= 8;
		if (fread(tmp, 1, 1, file) != 1)
			return 0;
		head |= tmp[0];
	}
	if (mpg123_decode_header(&frm, head))
	{
		buf = g_malloc(frm.framesize + 4);
		fseek(file, -4, SEEK_CUR);
		fread(buf, 1, frm.framesize + 4, file);
		xing_header.toc = NULL;
		tpf = mpg123_compute_tpf(&frm);
		if (mpg123_get_xing_header(&xing_header, buf))
		{
			g_free(buf);
			return ((guint) (tpf * xing_header.frames * 1000));
		}
		g_free(buf);
		bpf = mpg123_compute_bpf(&frm);
		fseek(file, 0, SEEK_END);
		len = ftell(file);
		fseek(file, -128, SEEK_END);
		fread(tmp, 1, 3, file);
		if (!strncmp(tmp, "TAG", 3))
			len -= 128;
		return ((guint) ((guint)(len / bpf) * tpf * 1000));
	}
	return 0;
}

void 
get_song_info (char *filename, char **title_real, int *len_real)
{
	FILE *file;

	(*len_real) = -1;
	(*title_real) = NULL;

	if ((file = fopen(filename, "rb")) != NULL) {
		(*len_real) = get_song_time (file);
		(*title_real) = get_song_title (file, filename);
		fclose(file);
	}
}

static void 
*decode_loop (void *arg)
{
	gboolean output_opened = FALSE;
	gint disp_count = 0, temp_time;
	gchar *filename = arg;

	mpg123_bitrate = 0;

	mpg123_pcm_sample = (unsigned char *) g_malloc0(32768);
	mpg123_pcm_point = 0;
	mpg123_filename = filename;

	mpg123_read_frame_init();

	mpg123_open_stream (filename, -1);
	if (mpg123_info->eof || !mpg123_read_frame(&fr)) {
		mpg123_info->eof = TRUE;
	}
	
	if(!mpg123_info->eof) {
		if (mpg123_cfg.channels == 2) {
			fr.single = -1;
		} else {
			fr.single = 3;
		}
		
		fr.down_sample = mpg123_cfg.downsample;
		fr.down_sample_sblimit = SBLIMIT >> mpg123_cfg.downsample;
		set_mpg123_synth_functions(&fr);
		mpg123_init_layer3 (fr.down_sample_sblimit);

		mpg123_info->tpf = mpg123_compute_tpf (&fr);
		xing_header.toc = xing_toc;
		if (mpg123_stream_check_for_xing_header (&fr, &xing_header)) {
			mpg123_info->num_frames = xing_header.frames;
			have_xing_header = TRUE;
			vbr_set = TRUE;
			mpg123_read_frame (&fr);
		}

		for(;;) {
			memcpy (&temp_fr,&fr,sizeof(struct frame));
			if (!mpg123_read_frame (&temp_fr)) {
				mpg123_info->eof = TRUE;
				break;
			}
			
			if (fr.lay != temp_fr.lay || fr.sampling_frequency != temp_fr.sampling_frequency || 
	        	    fr.stereo != temp_fr.stereo || fr.lsf != temp_fr.lsf) {
				memcpy(&fr,&temp_fr,sizeof(struct frame));
			} else {
				break;
			}
		}
		
		if(!have_xing_header) { 
			mpg123_info->num_frames = mpg123_calc_numframes (&fr);
		}

		memcpy (&fr,&temp_fr,sizeof(struct frame));
		mpg123_bitrate = disp_bitrate = tabsel_123[fr.lsf][fr.lay - 1][fr.bitrate_index];
		mpg123_frequency = mpg123_freqs[fr.sampling_frequency];
		mpg123_stereo = fr.stereo;
		mpg123_layer = fr.lay;
		mpg123_lsf = fr.lsf;
		mpg123_mpeg25 = fr.mpeg25;
		mpg123_mode = fr.mode;

		mpg123_length = mpg123_info->num_frames * mpg123_info->tpf * 1000;
		if (!mpg123_title) {
			mpg123_title = get_song_title(NULL,filename);
		}
			
		output_opened = TRUE;
		if (!esdout_open (mpg123_cfg.resolution == 16 ? FMT_S16_NE : FMT_U8,
			mpg123_freqs[fr.sampling_frequency] >> mpg123_cfg.downsample,
			mpg123_cfg.channels == 2 ? fr.stereo : 1)) {
			audio_error = TRUE;
			mpg123_info->eof = TRUE;
		}
	}

	mpg123_info->first_frame = FALSE;
	while (mpg123_info->going) {
		if (mpg123_info->jump_to_time != -1) {
			if (!have_xing_header) {
				mpg123_stream_jump_to_frame(&fr, (int) (mpg123_info->jump_to_time / mpg123_info->tpf));
			} else {
				mpg123_stream_jump_to_byte(&fr, mpg123_seek_point(xing_toc, xing_header.bytes, ((double) mpg123_info->jump_to_time * 100.0) / ((double) mpg123_info->num_frames * mpg123_info->tpf)));
			}
			esdout_flush (mpg123_info->jump_to_time * 1000);
			mpg123_info->jump_to_time = -1;
			mpg123_info->eof = FALSE;
		}
		
		if (!mpg123_info->eof) {
			if (mpg123_read_frame(&fr) != 0) {
				if(fr.lay != mpg123_layer || fr.lsf != mpg123_lsf) {
					memcpy(&temp_fr,&fr,sizeof(struct frame));
					if(mpg123_read_frame(&temp_fr) != 0) {
						if(fr.lay == temp_fr.lay && fr.lsf == temp_fr.lsf) {
							mpg123_layer = fr.lay;
							mpg123_lsf = fr.lsf;
							memcpy(&fr,&temp_fr,sizeof(struct frame));
							set_mpg123_synth_functions(&fr);
						} else {
							memcpy(&fr,&temp_fr,sizeof(struct frame));
							skip_frames = 2;
							mpg123_info->output_audio = FALSE;
							continue;
						}						
					}
				}
				
				if(mpg123_freqs[fr.sampling_frequency] != mpg123_frequency || mpg123_stereo != fr.stereo) {
					memcpy(&temp_fr,&fr,sizeof(struct frame));
					if(mpg123_read_frame(&temp_fr) != 0) {
						if(fr.sampling_frequency == temp_fr.sampling_frequency && temp_fr.stereo == fr.stereo) {
							esdout_free ();
							esdout_free ();
							while(mpg123_info->going && mpg123_info->jump_to_time == -1) {
								usleep (20000);
							}
							
							if(!mpg123_info->going) {
								break;
							}
							temp_time = esdout_get_output_time ();
							esdout_close ();
							mpg123_frequency = mpg123_freqs[fr.sampling_frequency];
							mpg123_stereo = fr.stereo;

							if (!esdout_open (mpg123_cfg.resolution == 16 ? FMT_S16_NE : FMT_U8,
								mpg123_freqs[fr.sampling_frequency] >> mpg123_cfg.downsample,
				  				mpg123_cfg.channels == 2 ? fr.stereo : 1)) {
								audio_error = TRUE;
								mpg123_info->eof = TRUE;
							}
							esdout_flush (temp_time);
							//mpg123_ip.set_info(mpg123_title, mpg123_length, mpg123_bitrate * 1000, mpg123_frequency, mpg123_stereo);
							memcpy(&fr,&temp_fr,sizeof(struct frame));
							set_mpg123_synth_functions(&fr);
						}
						else
						{
							memcpy(&fr,&temp_fr,sizeof(struct frame));
							skip_frames = 2;
							mpg123_info->output_audio = FALSE;
							continue;
						}
					}					
				}
				
				if (tabsel_123[fr.lsf][fr.lay - 1][fr.bitrate_index] != mpg123_bitrate) {
					mpg123_bitrate = tabsel_123[fr.lsf][fr.lay - 1][fr.bitrate_index];
				}
				
				if (!disp_count) {
					disp_count = 20;
					if (mpg123_bitrate != disp_bitrate) {
						disp_bitrate = mpg123_bitrate;
						if(!have_xing_header) {
							mpg123_info->num_frames = mpg123_calc_numframes(&fr);
							mpg123_info->tpf = mpg123_compute_tpf(&fr);
							mpg123_length = mpg123_info->num_frames * mpg123_info->tpf * 1000;
						}
						//mpg123_ip.set_info(mpg123_title, mpg123_length, mpg123_bitrate * 1000, mpg123_frequency, mpg123_stereo);
					}
				} else {
					disp_count--;
				}
				play_frame (&fr);				
			} else {
				esdout_free ();
				esdout_free ();
				mpg123_info->eof = TRUE;
				usleep(10000);
			}
		} else {
			usleep(10000);
		}
	}
	
	g_free(mpg123_title);
	mpg123_title = NULL;
	mpg123_stream_close();
	if (output_opened && !audio_error) {
		esdout_close ();
	}

	g_free (mpg123_pcm_sample);

	pthread_exit (NULL);
	return NULL;  /* make GCC happy */
}

void 
mpg123_play_file (const char *filename)
{
	/* Just to be safe */
	mpg123_stop ();

	/* Start playback */
	mpg123_init ();

	memset(&fr, 0, sizeof (struct frame));
	memset(&temp_fr, 0, sizeof (struct frame));

	mpg123_info = g_malloc0(sizeof (PlayerInfo));
	mpg123_info->going = TRUE;
	mpg123_info->paused = FALSE;
	mpg123_info->first_frame = TRUE;
	mpg123_info->output_audio = TRUE;
	mpg123_info->jump_to_time = -1;
	skip_frames = 0;
	audio_error = FALSE;
	have_xing_header = FALSE;

	pthread_create (&decode_thread, NULL, decode_loop, g_strdup (filename));
}

void 
mpg123_stop (void)
{	
	if (mpg123_info && mpg123_info->going) {
		g_free (mpg123_filename);
		mpg123_filename = NULL;
		mpg123_info->going = FALSE;
		pthread_join (decode_thread, NULL);
		g_free (mpg123_info);
		mpg123_info = NULL;
	}
}

void 
mpg123_seek (int time)
{
	mpg123_info->jump_to_time = time;

	while (mpg123_info->jump_to_time != -1) {
		usleep(10000);
	}
}

void 
mpg123_pause (gboolean pause)
{
	esdout_pause (pause);
}

int 
get_time (void)
{
	if (audio_error)
		return -2;
		
	if (!mpg123_info)
		return -1;
		
	if (!mpg123_info->going || mpg123_info->eof) {
		return -1;
	}

	return esdout_get_output_time ();
}

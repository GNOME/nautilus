/* -*- linux-c -*- */

/*
 * Routines for ginterfacing/controlling mpg123
 *
 * (C)1998, 1999 John Ellis
 *
 * Author: John Ellis
 *
 * integrated with Nautilus by Andy Hertzfeld
 *
 * This software is released under the GNU General Public License.
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at you own risk!
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk_imlib.h>

#include "mpg123_handler.h"


#define		MPG123_DOWNSAMPLE_AUTO 0
#define		MPG123_DOWNSAMPLE_22 1
#define		MPG123_DOWNSAMPLE_11 2
#define		MPG123_DOWNSAMPLE_CUSTOM 3

#define		PLAYLIST_MODE_EMPTY 0
#define		PLAYLIST_MODE_SESSION 1
#define		PLAYLIST_MODE_FILE 2

#define		TYPE_MPEG_FILE 0
#define		TYPE_MPEG_SHOUTCAST 1

/* ----------------------------------------------------------
   input / output ginterface to mpg123
   ----------------------------------------------------------*/

#define MPG123_VER_0_59O 0	/* versions 0.59o and 0.59p */
#define MPG123_VER_0_59Q 1	/* version 0.59q */
#define MPG123_VER_UNKNOWN 9	/* unknown, treat as latest in case newer ver */

#define MPG123_VER_STRING "0.59q"	/* text representation of latest version */

gchar	song_name_data[512];

gchar	*song_path = NULL;
gint	frames = 0;
gint	frames_remaining = 0;
gint	seconds = 0;
gint	seconds_remaining = 0;
gchar	*song_title = NULL;
gint	mpeg_version = 0;
gint	mpeg_layer = 0;
gint	mpeg_mode = 0;
gint	mpeg_bitrate = 0;
gint	mpeg_channels = 0;
gint	mpeg_hz = 0;
gint	output_channels = 0;
gint	output_hz = 0;
gint	output_bits = 0;
gint	output_conversion = 0;

	/* mpg123 options */
gint	mpg123_buffer_enable = FALSE;
gint	mpg123_buffer = 512;
gint	mpg123_downsample = MPG123_DOWNSAMPLE_AUTO;
gint	mpg123_custom_sample_size = 44100;
gint	mpg123_mono = FALSE;
gint	mpg123_8bit = FALSE;
gint	mpg123_device_enable = FALSE;
gchar	*mpg123_device = NULL;

gint debug_mode = 0;
gint new_song = 0;

static gint mpg123_status = STATUS_STOP;
static gint mp3_pid;
static void sigchld_handler(int);

static FILE *mpgpipe = NULL;
static gint mypipe[2];
static gchar pipebuf1[255];
static gchar *pipebuf_ptr;
static gint signal_timeout_id = 0;
static gint mpg123_version = MPG123_VER_0_59O;

/* headers */

static void parse_header_info();
static void parse_frame_info();
static void stop_data();
static void song_finished();
static gint parse_pipe_buffer();
static gint check_pipe_for_data(gint fd);
static gint read_data();

/* cover routines for accessing globals */

int get_play_status ()
{
	return mpg123_status;

}

int get_current_frame ()
{
	return frames;

}
void set_current_frame (int new_frame)
{
	frames = new_frame;

}

/* process signal handler, for notification if mpg123 terminates */
static void sigchld_handler(int sig)
{
	pid_t child;
	gint status;

	signal(SIGCHLD, sigchld_handler);

	child = waitpid(0, &status, 0);
}


static void parse_header_info ()
{
	gchar s[128];
	gchar *ptr;
	gchar *ptr2;

if (debug_mode) printf("h");

	ptr = pipebuf1;

	/* Version line */
	if (!strncmp(pipebuf1,"Version",7))
		{
if (debug_mode) putchar('V');
		ptr += 8;
		ptr2 = s;
		while (ptr[0] != ' ' && ptr[0] != '\0')
			{
			ptr2[0] = ptr[0];
			ptr2++;
			ptr++;
			}
		ptr2[0] = '\0';
		if (strcmp(s, "0.59o") == 0)
			mpg123_version = MPG123_VER_0_59O;
		else if (strcmp(s, "0.59p") == 0)
			mpg123_version = MPG123_VER_0_59O;
		else if ((strcmp(s, "0.59q") == 0) || (strcmp(s, "0.59r") == 0))
			mpg123_version = MPG123_VER_0_59Q;
		else
			{
			mpg123_version = MPG123_VER_UNKNOWN;
			printf("unknown version of mpg123, assuming" MPG123_VER_STRING "compatible\n");
			printf("(Nautilus requires mpg123 0.59o or later)\n");
			}
		if (debug_mode) printf("mpg123 version detected: %d\n", mpg123_version);
		}

	/* MPEG, layer, freq, mode line */
	if (!strncmp(pipebuf1,"MPEG",4))
		{
if (debug_mode) putchar('M');
		ptr += 4;
		ptr2 = s;
		while (ptr[0] != 'L' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ' && ptr[0] != ',')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		mpeg_version = strtol(s,NULL,10);

		ptr += 6;
		ptr2 = s;
		while (ptr[0] != 'F' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ' && ptr[0] != ',')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		mpeg_layer = strlen(s);

		ptr += 5;
		ptr2 = s;
		while (ptr[0] != 'm' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ' && ptr[0] != ',')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		mpeg_hz = strtol(s,NULL,10);

		ptr += 5;
		ptr2 = s;
		while (ptr[0] != ',' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		if (!strcmp(s,"Stereo")) mpeg_mode = 1;
		if (!strcmp(s,"Jogint-Stereo")) mpeg_mode = 2;
		if (!strcmp(s,"Dual-Channel")) mpeg_mode = 3;
		if (!strcmp(s,"Single-Channel")) mpeg_mode = 4;
		}

	/* Channels  line */
	if (!strncmp(pipebuf1,"Channels:",9))
		{
if (debug_mode) putchar('C');
		ptr += 9;
		ptr2 = s;
		while (ptr[0] != ',' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		mpeg_channels = strtol(s,NULL,10);
		}

	/* Bitrate line */
	if (!strncmp(pipebuf1,"Bitrate:",8))
		{
if (debug_mode) putchar('B');
		ptr += 8;
		ptr2 = s;
		while (ptr[0] != 'K' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		mpeg_bitrate = strtol(s,NULL,10);
		}

	/* Audio output line */
	if (!strncmp(pipebuf1,"Audio:",6))
		{
if (debug_mode) putchar('A');
		ptr += 6;
		ptr2 = s;
		while (ptr[0] != ',' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		output_conversion = strtol(s,NULL,10);

		ptr += 7;
		ptr2 = s;
		while (ptr[0] != 'e' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ' && ptr[0] != ',')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		output_hz = strtol(s,NULL,10);

		ptr += 9;
		ptr2 = s;
		while (ptr[0] != ',' && ptr[0] != '\0')
			{
			if (ptr[0] >= '0' && ptr[0] <= '9')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		output_bits = strtol(s,NULL,10);

		ptr += 11;
		ptr2 = s;
		while (ptr[0] != 10 && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		output_channels = strtol(s,NULL,10);

		new_song = TRUE;
		}
}

	/* we parse the output to get the current song position
		including seconds and frames (played and remaining) */
static void parse_frame_info()
{
	gchar frame_cnt[12] = "";
	gchar frame_cnt_rem [12] = "";
	gchar sec_cnt [12] = "";
	gchar sec_cnt_rem [12] = "";
	gchar min_cnt [12] = "";
	gchar min_cnt_rem [12] = "";
	gchar *ptr;
	gchar *ptr2;
	
	if (debug_mode) printf("f");

	ptr = pipebuf1;
	ptr += 6;
	ptr2 = frame_cnt;
	while (ptr[0] != '[' && ptr[0] != '\0')
		{
		if (ptr[0] != ' ')
			{
			ptr2[0] = ptr[0];
			ptr2++;
			}
		ptr++;
		}
	ptr2[0] = '\0';
	ptr++;
	ptr2 = frame_cnt_rem;
	while (ptr[0] != ']' && ptr[0] != '\0')
		{
		if (ptr[0] != ' ')
			{
			ptr2[0] = ptr[0];
			ptr2++;
			}
		ptr++;
		}
	ptr2[0] = '\0';
	while (ptr[0] != ':' && ptr[0] != '\0')
		{
		ptr++;
		}
	ptr++;

	if (mpg123_version == MPG123_VER_0_59O) /* 0.00 [000.00] */
		{
		ptr2 = sec_cnt;
		while (ptr[0] != '[' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		ptr++;
		ptr2 = sec_cnt_rem;
		while (ptr[0] != ']' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			else
				{
				ptr2[0] = '0';
				ptr2++;
				}
				
			ptr++;
			}
		ptr2[0] = '\0';
		}
	else /* 00:00.00 [00:00.00] */
		{
		ptr2 = min_cnt;
		while (ptr[0] != ':' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		ptr++;
		ptr2 = sec_cnt;
		while (ptr[0] != '[' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		ptr++;
		ptr2 = min_cnt_rem;
		while (ptr[0] != ':' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		ptr++;
		ptr2 = sec_cnt_rem;
		while (ptr[0] != ']' && ptr[0] != '\0')
			{
			if (ptr[0] != ' ')
				{
				ptr2[0] = ptr[0];
				ptr2++;
				}
			ptr++;
			}
		ptr2[0] = '\0';
		}

	frames = strtol(frame_cnt,NULL,10);
	frames_remaining = strtol(frame_cnt_rem,NULL,10);
	seconds = (strtol(min_cnt,NULL,0) * 60) + strtol(sec_cnt,NULL,10);
	seconds_remaining = (strtol(min_cnt_rem,NULL,10) * 60) + strtol(sec_cnt_rem,NULL,10);
}

static void stop_data()
{
	if (signal_timeout_id)
		{
		gtk_timeout_remove(signal_timeout_id);
		signal_timeout_id = 0;
		}
	if (mpgpipe)
		{
		fclose (mpgpipe);
		mpgpipe = NULL;
		}
if (debug_mode) printf("closed\n");
}

static void song_finished()
{
	stop_data();
	
	mpg123_status = STATUS_NEXT;
	frames = 0;
}

/* this routine parses the output from mpg123 to check it's current status
   when the end is detected, 1 or TRUE is returned; any error return -1;
   a detected frame (second & frame info) return 1 or TRUE; 2 is returned
   on anything else (including header info) */
static gint parse_pipe_buffer()
{
	/* check for Frame data */
	if (!strncmp(pipebuf1,"Frame",5))
		{
		parse_frame_info();
		return 1;
		}

	/* check for end of song (mpg123 reports a line starting
		with a '[') */
	if (!strncmp(pipebuf1,"[",1))
		{
    	signal(SIGCHLD, SIG_IGN);	
		waitpid (mp3_pid, NULL, 0);
 		signal(SIGCHLD, sigchld_handler);
		
		song_finished();
		return 0;
		}

	/* we start looking for mpg123 errors here! */

	/* mpg123 couldn't open device */
	if (!strncmp(pipebuf1,"Can't open",10))
		{

		printf("Error opening output device (mpg123 reported:\"%s\")\n",pipebuf1);

    	signal(SIGCHLD, SIG_IGN);	
		waitpid (mp3_pid, NULL, 0);
 		signal(SIGCHLD, sigchld_handler);
		stop_data();
		mpg123_status = STATUS_PAUSE;
		return -1;
		}

	parse_header_info();
	return 2;
}

	/* copied from the Unix programming FAQ */
static gint check_pipe_for_data(gint fd)
     {
	gint rc;
	fd_set fds;
	struct timeval tv;
	
	FD_ZERO(&fds);
	FD_SET(fd,&fds);
	tv.tv_sec = tv.tv_usec = 0;

	rc = select(fd+1, &fds, NULL, NULL, &tv);
	if (rc < 0)
		return -1;

	return FD_ISSET(fd,&fds) ? 1 : 0;
}


static gint read_data()
{
	gint c, d;
	static gint dcheck;

	if (debug_mode) printf("I");

	if (mpg123_status != STATUS_PLAY) return FALSE;
	
	if (!mpgpipe)
		{
		mpgpipe = fdopen (mypipe[0], "r");

		if (debug_mode) printf("pipe opened\n");

		setvbuf(mpgpipe,NULL,_IONBF,BUFSIZ);
		pipebuf_ptr = pipebuf1;
		}

	/* if mpg123 starts sending too much data, we exit this loop
		after 8k bytes so the program will respond to user
		input. Otherwise, the program will appear hung.*/
	d = 8000;

	while (d > 0 && check_pipe_for_data(mypipe[0]))
		{
		c = getc(mpgpipe);
		if (c == 13 || c == 10)
			{
			pipebuf_ptr[0] = '\0';
			if (parse_pipe_buffer() == -1) return FALSE;
			if (mpg123_status == STATUS_NEXT) return FALSE;
			pipebuf_ptr = pipebuf1;
			}
		else
			{
			pipebuf_ptr[0] = c;
			pipebuf_ptr++;
			}
		d--;
		}

	/* here we check to see if mpg123 sent data within the last
		10 seconds (68 times through [at 150ms per] with no
		data). no data means mpg123 encountered an error we
		did not catch or it crashed */
	if ( d == 8000)
		dcheck++;
	else
		dcheck = 0;
	
	/*
	if (dcheck > 68)
		{
		printf("Error: mpg123 stopped sending data!\n");
		waitpid (mp3_pid, NULL, 0);
		song_finished();
		return FALSE;
		}
	*/
	
	if (debug_mode) printf("O");

	return TRUE;
}

void start_playing_file(gchar* filename, gboolean start_from_beginning)
{
	pid_t frk_pid;
	gchar cmd_arguments[16][512];
	gchar *cmd_ptr[16];
	gint cmd_cnt = 0;

	if (mpg123_status == STATUS_PLAY) return;
	
	if (start_from_beginning)
		frames = 0;
		
	/* create all command line arguments */

	strcpy(cmd_arguments[cmd_cnt],"mpg123");
	cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
	cmd_cnt++;

	strcpy(cmd_arguments[cmd_cnt],"-v");
	cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
	cmd_cnt++;

	strcpy(cmd_arguments[cmd_cnt],"-k");
	cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
	cmd_cnt++;
	sprintf(cmd_arguments[cmd_cnt],"%d",frames);
	cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
	cmd_cnt++;

	if (mpg123_buffer_enable)
		{
		strcpy(cmd_arguments[cmd_cnt],"-b");
		cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
		cmd_cnt++;
		sprintf(cmd_arguments[cmd_cnt],"%d",mpg123_buffer);
		cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
		cmd_cnt++;
		}

	if (mpg123_downsample != MPG123_DOWNSAMPLE_AUTO)
		{
		if (mpg123_downsample == MPG123_DOWNSAMPLE_22)
			{
			strcpy(cmd_arguments[cmd_cnt],"-2");
			cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
			cmd_cnt++;
			}
		if (mpg123_downsample == MPG123_DOWNSAMPLE_11)
			{
			strcpy(cmd_arguments[cmd_cnt],"-4");
			cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
			cmd_cnt++;
			}
		if (mpg123_downsample == MPG123_DOWNSAMPLE_CUSTOM)
			{
			sprintf(cmd_arguments[cmd_cnt],"-r %d", mpg123_custom_sample_size);
			cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
			cmd_cnt++;
			}
		}

	if (mpg123_mono)
		{
		strcpy(cmd_arguments[cmd_cnt],"-m");
		cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
		cmd_cnt++;
		}

	if (mpg123_8bit)
		{
		strcpy(cmd_arguments[cmd_cnt],"--8bit");
		cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
		cmd_cnt++;
		}

	if (mpg123_device_enable && mpg123_device)
		{
		strcpy(cmd_arguments[cmd_cnt],"-a");
		cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
		cmd_cnt++;
		strcpy(cmd_arguments[cmd_cnt],mpg123_device);
		cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
		cmd_cnt++;
		}
		
	strcpy(song_name_data, filename);
	song_path = &song_name_data[0];
	
	strcpy(cmd_arguments[cmd_cnt],song_path);
	cmd_ptr[cmd_cnt] = cmd_arguments[cmd_cnt];
	cmd_cnt++;

	strcpy(cmd_arguments[cmd_cnt], "");
	cmd_ptr[cmd_cnt] = NULL;
	cmd_cnt++;


	  /* Create the pipe. */
	if (pipe (mypipe))
		{
		fprintf (stderr, "Pipe failed.\n");
		return;
		}

	if (debug_mode) printf("opening: %s\n",song_path);

	/* Create the child process. */
	frk_pid = fork ();
	if (frk_pid == (pid_t) 0)
		{
		/* This is the child process. */
		dup2(mypipe[1],2);
		close(mypipe[0]);

		/* set the group (session) id to this process for future killing */
		setsid();

		execvp("mpg123",cmd_ptr);
		printf("unable to run mpg123 (in the path?)\n");
		exit(1);
		}
	else if (frk_pid < (pid_t) 0)
		{
		/* The fork failed. */
		fprintf (stderr, "Fork failed.\n");
		mp3_pid = 0;
		return;
		}
	else
		{
		/* This is the parent process. */
		mp3_pid = (gint) frk_pid;
		close(mypipe[1]);
		}

	if (debug_mode) printf("mpg123 pid = %d\n", mp3_pid);

	mpg123_status = STATUS_PLAY;
	signal_timeout_id = gtk_timeout_add(150,(GtkFunction) read_data,NULL);
}

void stop_playing_file ()
{
	pid_t child;
	gint  status_result;
	if (mpg123_status == STATUS_STOP) 
		return;

	if (debug_mode) printf("sending SIGTERM to pid = -%d\n", mp3_pid);

	/* kill the entire mpg123 group to work around mpg123 buffer bug */

	kill(-mp3_pid, SIGINT);

    signal(SIGCHLD, SIG_IGN);
 	child = waitpid(mp3_pid, &status_result, 0);       
 	signal(SIGCHLD, sigchld_handler);

	stop_data();
    mpg123_status = STATUS_STOP;
}

void pause_playing_file ()
{
	if (mpg123_status == STATUS_STOP) return;
	
	if (mpg123_status == STATUS_PLAY)
		{
		stop_playing_file();
		mpg123_status = STATUS_PAUSE;
		}
	else
		{
		start_playing_file(song_path, FALSE);
		}		

}
	

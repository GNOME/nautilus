/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Andy Hertzfeld <andy@eazel.com>
 *
 */

/* music view - presents the contents of the directory as an album of music */

#include <config.h>
#include "nautilus-music-view.h"
#include "mpg123_handler.h"
#include "mp3head.h"

#include "pixmaps.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <libnautilus-extensions/nautilus-string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>

struct _NautilusMusicViewDetails {
        char *uri;
	NautilusView *nautilus_view;
        
	int sort_mode;
	int selected_index;
	int status_timeout;
	
	int current_file_size;
	int current_bitrate;
	int last_play_status;
	int current_samprate;
	
	gboolean slider_dragging;
	
	GtkVBox   *album_container;
	GtkWidget *album_title;
	GtkWidget *song_list;
	GtkWidget *album_image;
        
	GtkWidget *control_box;
	GtkWidget *play_control_box;

	GtkWidget *song_label;
	GtkWidget *total_track_time;
	
	GtkWidget *playtime;
	GtkWidget *playtime_bar;
	GtkObject *playtime_adjustment;

	GtkWidget *inactive_play_pixwidget;
	GtkWidget *active_play_pixwidget;
	GtkWidget *inactive_pause_pixwidget;
	GtkWidget *active_pause_pixwidget;
};


/* structure for holding song info */

typedef struct {
	int track_number;
	int bitrate;
	int track_time;
	int stereo;
	int samprate;
	
	char *title;
	char *artist;
	char *album;
	char *year;
	char *comment;
	char *path_uri;
} SongInfo;

enum {
	TARGET_URI_LIST,
	TARGET_COLOR,  
	TARGET_BGIMAGE,
        TARGET_GNOME_URI_LIST
};

/* button commands */

enum {
	PREVIOUS_BUTTON,
	PLAY_BUTTON,
	PAUSE_BUTTON,
	STOP_BUTTON,
	NEXT_BUTTON
};

/* sort modes */

enum {
	SORT_BY_NUMBER,
	SORT_BY_TITLE,
	SORT_BY_ARTIST,
	SORT_BY_BITRATE,
	SORT_BY_TIME
};

static GtkTargetEntry music_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "property/bgimage", 0, TARGET_BGIMAGE },
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

static void nautilus_music_view_drag_data_received (GtkWidget              *widget,
                                                    GdkDragContext         *context,
                                                    int                     x,
                                                    int                     y,
                                                    GtkSelectionData       *selection_data,
                                                    guint                   info,
                                                    guint                   time);
static void nautilus_music_view_initialize_class   (NautilusMusicViewClass *klass);
static void nautilus_music_view_initialize         (NautilusMusicView      *view);
static void nautilus_music_view_destroy            (GtkObject              *object);
static void nautilus_music_view_update_from_uri    (NautilusMusicView      *music_view,
                                                    const char             *uri);
static void music_view_load_location_callback      (NautilusView           *view,
                                                    const char             *location,
                                                    NautilusMusicView      *music_view);
static void selection_callback                     (GtkCList               *clist,
                                                    int                     row,
                                                    int                     column,
                                                    GdkEventButton         *event,
                                                    NautilusMusicView      *music_view);
static void music_view_set_selected_song_title     (NautilusMusicView      *music_view,
                                                    int                     row);
static void add_play_controls                      (NautilusMusicView      *music_view);
static void click_column_callback                  (GtkCList               *clist,
                                                    gint                    column,
                                                    NautilusMusicView      *music_view);
static void go_to_next_track                       (NautilusMusicView      *music_view);
static void play_current_file                      (NautilusMusicView      *music_view,
                                                    gboolean                from_start);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusMusicView, nautilus_music_view, GTK_TYPE_EVENT_BOX)

static void
nautilus_music_view_initialize_class (NautilusMusicViewClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = nautilus_music_view_destroy;
	widget_class->drag_data_received  = nautilus_music_view_drag_data_received;
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_music_view_initialize (NautilusMusicView *music_view)
{
	GtkWidget *scrollwindow;
	char *titles[] = {_("Track "), _("Title"), _("Artist"), _("Year"), _("Bitrate "), _("Time "), _("Album"),  _("Comment"), _("Channels"),  _("Sample Rate"),};
	GdkFont *font;
	
	music_view->details = g_new0 (NautilusMusicViewDetails, 1);

	music_view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (music_view));
    	
	gtk_signal_connect (GTK_OBJECT (music_view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (music_view_load_location_callback), 
			    music_view);

	music_view->details->status_timeout = -1;
	music_view->details->slider_dragging = FALSE;
	
	/* allocate a vbox to contain all of the views */
	
	music_view->details->album_container = GTK_VBOX (gtk_vbox_new (FALSE, 8));
	gtk_container_set_border_width (GTK_CONTAINER (music_view->details->album_container), 4);
	gtk_container_add (GTK_CONTAINER (music_view), GTK_WIDGET (music_view->details->album_container));
	
	gtk_widget_show (GTK_WIDGET (music_view->details->album_container));
	
	/* allocate a widget for the album title */
	
	music_view->details->album_title = gtk_label_new (_("Album Title"));

        font = nautilus_font_factory_get_font_from_preferences (18);
	nautilus_gtk_widget_set_font (music_view->details->album_title, font);

	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->album_title, FALSE, FALSE, 0);	
	gtk_widget_show (music_view->details->album_title);
	
	/* allocate a list widget to hold the song list */

	music_view->details->song_list = gtk_clist_new_with_titles (10, titles);
		
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 0, 36);		/* track number */
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 1, 204);	/* song name */
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 2, 96);		/* artist */
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 3, 42);		/* year */	
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 4, 42);		/* bitrate */	
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 5, 42);		/* time */
 
 	/* we have 2 invisible columns at the end to hold data displayed as the song title */
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 6, 0);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 7, 0);

	 /* two more so we can make correct calculations for all files */
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 8, 0);		/* Stereo/Mono */
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 9, 0);		/* sample rate */
	 
	 /* default the year, album, comment, stereo and samprate to hidden */
	gtk_clist_set_column_visibility (GTK_CLIST (music_view->details->song_list), 3, FALSE);	 
	gtk_clist_set_column_visibility (GTK_CLIST (music_view->details->song_list), 6, FALSE);
	gtk_clist_set_column_visibility (GTK_CLIST (music_view->details->song_list), 7, FALSE);
	gtk_clist_set_column_visibility (GTK_CLIST (music_view->details->song_list), 8, FALSE);
	gtk_clist_set_column_visibility (GTK_CLIST (music_view->details->song_list), 9, FALSE);

  
 	/* make some of the columns right justified */
 		
 	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 0, GTK_JUSTIFY_RIGHT);
 	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 3, GTK_JUSTIFY_RIGHT);
 	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 4, GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 5, GTK_JUSTIFY_RIGHT);
 	
 	gtk_signal_connect (GTK_OBJECT (music_view->details->song_list),
                            "select_row", GTK_SIGNAL_FUNC (selection_callback), music_view);
 
	scrollwindow = gtk_scrolled_window_new (NULL, gtk_clist_get_vadjustment (GTK_CLIST (music_view->details->song_list)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrollwindow), music_view->details->song_list);	
	gtk_clist_set_selection_mode (GTK_CLIST (music_view->details->song_list), GTK_SELECTION_BROWSE);

	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), scrollwindow, TRUE, TRUE, 0);	
	gtk_widget_show (music_view->details->song_list);
	gtk_widget_show (scrollwindow);

	/* connect a signal to let us know when the column titles are clicked */
	gtk_signal_connect(GTK_OBJECT(music_view->details->song_list), "click_column",
				GTK_SIGNAL_FUNC(click_column_callback), music_view);

	/* make an hbox to hold the optional cover and other controls */
	
	music_view->details->control_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->control_box, FALSE, FALSE, 2);	
	gtk_widget_show (music_view->details->control_box);
	
	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (music_view),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   music_dnd_target_table, NAUTILUS_N_ELEMENTS (music_dnd_target_table), GDK_ACTION_COPY);

	/* finally, show the view itself */	
	gtk_widget_show (GTK_WIDGET (music_view));
}

static void
nautilus_music_view_destroy (GtkObject *object)
{
	NautilusMusicView *music_view = NAUTILUS_MUSIC_VIEW (object);

	/* we'd rather allow the song to keep playing, but it's hard to main state */
	/* so we stop things on exit for now, and improve it post 1.0 */
	stop_playing_file();

	g_free (music_view->details->uri);
	g_free (music_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* utility to return the text describing a song */
static char *
get_song_text (NautilusMusicView *music_view, int row)
{
	char *song_text, *song_name, *album_name, *year;
	
	album_name = NULL;
	year = NULL;
	
	gtk_clist_get_text (GTK_CLIST(music_view->details->song_list), row, 1, &song_name);
	gtk_clist_get_text (GTK_CLIST(music_view->details->song_list), row, 3, &year);
	gtk_clist_get_text (GTK_CLIST(music_view->details->song_list), row, 6, &album_name);
	
	if (album_name) {
		if (year)
			song_text = g_strdup_printf("%s\n%s (%s)", song_name, album_name, year);
		else
			song_text = g_strdup_printf("%s\n%s", song_name, album_name);
		
	} else if (year) {
		song_text = g_strdup_printf("%s (%s)", song_name, year);
	} else {
		song_text = g_strdup(song_name);
	}
	
	return song_text;
}

/* set the song title to the selected one */

static void 
music_view_set_selected_song_title (NautilusMusicView *music_view, int row)
{
	char *label_text;
	char *temp_str;

	music_view->details->selected_index = row;
	
	label_text = get_song_text(music_view, row);
	gtk_label_set(GTK_LABEL(music_view->details->song_label), label_text);
	g_free(label_text);
        
        gtk_clist_get_text (GTK_CLIST(music_view->details->song_list), row, 5, &temp_str);
	gtk_label_set(GTK_LABEL(music_view->details->total_track_time), temp_str);
}


/* handle a row being selected in the list view by playing the corresponding song */
static void 
selection_callback(GtkCList * clist, int row, int column, GdkEventButton * event, NautilusMusicView* music_view)
{
	gboolean is_playing;
	char *song_name;
	int play_mode;
	
	
	play_mode = get_play_status();
	is_playing = (play_mode == STATUS_PLAY) || (play_mode == STATUS_PAUSE);
	
	if (is_playing && (music_view->details->selected_index == row))
		return;
		
	if (is_playing) 
		stop_playing_file();


        song_name = gtk_clist_get_row_data (clist, row);
	if (song_name == NULL) {
		return;
        }

        music_view_set_selected_song_title(music_view, row);
	if (is_playing)
		play_current_file(music_view, FALSE);
} 

/* handle clicks in the songlist columns */

static void
click_column_callback (GtkCList * clist, gint column, NautilusMusicView *music_view)
{
	if (music_view->details->sort_mode == column)
		return;
	music_view->details->sort_mode = column;
	nautilus_music_view_update_from_uri (music_view, music_view->details->uri);
}

/* Component embedding support */
NautilusView *
nautilus_music_view_get_nautilus_view (NautilusMusicView *music_view)
{
	return music_view->details->nautilus_view;
}

/* here are some utility routines for reading ID3 tags from mp3 files */

/* initialize a songinfo structure */

static void
initialize_song_info (SongInfo *info)
{
        /* Only called after g_new0. */
	info->track_number = -1;
}

/* deallocate a songinfo structure */

static void
release_song_info (SongInfo *info)
{
        g_free (info->title);
        g_free (info->artist);
        g_free (info->album);
        g_free (info->year);
        g_free (info->comment);
        g_free (info->path_uri);
	g_free (info);
}

/* determine if a file is an mp3 file by looking at the mime type */
static gboolean
is_mp3_file(GnomeVFSFileInfo *file_info)
{
	return nautilus_istr_has_prefix (file_info->mime_type, "audio/")
		&& nautilus_istr_has_suffix (file_info->mime_type, "mp3");
}

/* utility routine to strip the trailing blank padding from the end of a string */
static void
strip_trailing_blanks (char *str)
{
	int index;
	for (index = strlen(str); index > 0 && str[index] <= 0x20; str[index--] = '\0');
}

/* read the id3 tag of the file if present */

static gboolean
read_id_tag (const char *song_uri, SongInfo *song_info)
{
	GnomeVFSHandle *mp3_file;
	GnomeVFSResult result;
	GnomeVFSFileSize bytes_read;
	char tag_buffer[129];
	char temp_str[31];

	result = gnome_vfs_open(&mp3_file, song_uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return FALSE;
        }
 
 	gnome_vfs_seek (mp3_file, GNOME_VFS_SEEK_END, -128);
  
	if (gnome_vfs_read (mp3_file, tag_buffer, 128, &bytes_read) != GNOME_VFS_OK) {
  		gnome_vfs_close (mp3_file);
 		return FALSE;
	}
	gnome_vfs_close (mp3_file);
	
	if (tag_buffer[0] != 'T' || tag_buffer[1] != 'A' || tag_buffer[2] != 'G')
		return FALSE;
	
	temp_str[30] = '\0';
	strncpy (temp_str, &tag_buffer[3], 30);
	strip_trailing_blanks(temp_str);
	song_info->title = g_strdup(temp_str);
  
	strncpy (temp_str, &tag_buffer[33], 30);
	strip_trailing_blanks(temp_str);
	song_info->artist = g_strdup(temp_str);

	strncpy (temp_str, &tag_buffer[63], 30);
	strip_trailing_blanks(temp_str);
	song_info->album = g_strdup(temp_str); 

	temp_str[4] = '\0';
	strncpy (temp_str, &tag_buffer[93], 4);
	song_info->year = g_strdup(temp_str);

	strncpy (temp_str, &tag_buffer[97], 30);
	strip_trailing_blanks(temp_str);
	song_info->comment = g_strdup(temp_str);

    	if (tag_buffer[97 + 28] == 0) {
        	song_info->track_number = tag_buffer[97 + 29];
        } else {
                song_info->track_number = -1;
        }

	return TRUE;
}

/* fetch_play_time takes the pathname to a file and returns the play time in seconds */
static int
fetch_play_time (GnomeVFSFileInfo *file_info, int bitrate)
{
        if ((file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) == 0) {
                return 0;
        }

        /* Avoid divide by zero. */
	return bitrate == 0 ? 0 : file_info->size / (125 * bitrate);
}

/* format_play_time takes the pathname to a file and returns the play time formated as mm:ss */
static char *
format_play_time (int track_time)
{
	int seconds, minutes, remain_seconds;

        seconds = track_time;
	minutes = seconds / 60;
	remain_seconds = seconds - (60 * minutes);
	return g_strdup_printf ("%d:%02d ", minutes, remain_seconds);
}

/* extract a track number from the file name
   return -1 if there wasn't any */
static int
extract_number(const char *name_str)
{
	char *temp_str;
	gboolean found_digit;
	int accumulator;
	
	found_digit = FALSE;
	accumulator = 0;
	if (isdigit(*name_str)) temp_str = (char*) name_str;
	else if (strchr(name_str,'(')!=NULL) temp_str = (char *)strchr(name_str,'(')+1;
	else return -1;
	
	while (*temp_str) {
		if (isdigit(*temp_str)) {
			found_digit = TRUE;
			accumulator = (10 * accumulator) + *temp_str - 48;
		} else
			break;
		temp_str += 1;
	}		
	
	if (found_digit)
		return accumulator;
	return -1;
}

/* allocate a return a song info record, from an mp3 tag if present, or from intrinsic info */

static SongInfo *
fetch_song_info (const char *song_uri, GnomeVFSFileInfo *file_info, int file_order) 
{
	gboolean has_info = FALSE;
	SongInfo *info; 
	guchar buffer[1024];  
	GnomeVFSHandle *mp3_file;
	GnomeVFSResult result;
	GnomeVFSFileSize length_read;


	if (!is_mp3_file (file_info)) {
		return NULL;
        }

	info = g_new0 (SongInfo, 1); 
	initialize_song_info (info);
	
	has_info = read_id_tag (song_uri, info);

	/* if we couldn't get a track number, see if we can pull one from
	   the file name */
	if (info->track_number <= 0) {
		info->track_number = extract_number(file_info->name);
	}
		  	
	/* there was no id3 tag, so set up the info heuristically from the file name and file order */
	if (!has_info) {
		info->title = g_strdup (file_info->name);
	}	

	result = gnome_vfs_open(&mp3_file, song_uri, GNOME_VFS_OPEN_READ);
	if (result == GNOME_VFS_OK) {
  		result = gnome_vfs_read(mp3_file, buffer, sizeof(buffer), &length_read);
		if ((result == GNOME_VFS_OK) && (length_read > 512)) {
			info->bitrate = get_bitrate (buffer,length_read);
			info->samprate = get_samprate (buffer,length_read);
			info->stereo = get_stereo (buffer,length_read);
			info->track_time = fetch_play_time (file_info, info->bitrate);
		}
		gnome_vfs_close(mp3_file);
	}

	return	info;
}

/* sort comparison routines */

static int
sort_by_track_number (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return (int) a->track_number - b->track_number;
}

static int
sort_by_title (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return nautilus_strcmp (a->title, b->title);
}

static int
sort_by_artist (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return nautilus_strcmp (a->artist, b->artist);
}

static int
sort_by_time (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return a->track_time - b->track_time;
}

static int
sort_by_bitrate (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return a->bitrate - b->bitrate;
}


/* utility routine to determine most common attribute in song list.  The passed in boolean selects
   album or artist. Return NULL if no names or too heterogenous.   This first cut just captures 
   the first one it can - soon, we'll use a hash table and count them up */

static char *
determine_attribute (GList *song_list, gboolean is_artist)
{
	SongInfo *info;
	GList *p;

        for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *) p->data;
		if (is_artist && info->artist != NULL) {
			return g_strdup (info->artist);
                } else if (!is_artist && info->album) {
			return g_strdup (info->album);
                }
	}
                return NULL;
}

/* utility routine to sort the song list */
static GList *
sort_song_list(NautilusMusicView *music_view, GList* song_list)
{
	/* sort by the specified criteria */	
	switch (music_view->details->sort_mode) {
		case SORT_BY_NUMBER:
			song_list = g_list_sort (song_list, sort_by_track_number);
			break;
		case SORT_BY_TITLE:
			song_list = g_list_sort (song_list, sort_by_title);
			break;
		case SORT_BY_ARTIST:
			song_list = g_list_sort (song_list, sort_by_artist);
			break;
		case SORT_BY_BITRATE:
			song_list = g_list_sort (song_list, sort_by_bitrate);
			break;
		case SORT_BY_TIME:
			song_list = g_list_sort (song_list, sort_by_time);
			break;
		default:
			g_warning("unknown sort mode");
			break;
	}
	
	return song_list;
}

/* update the status feedback of the play controls */

static void
update_play_controls_status (NautilusMusicView *music_view, int new_status)
{
	if (new_status == STATUS_PLAY) {
		gtk_widget_show(music_view->details->active_play_pixwidget);
		gtk_widget_hide(music_view->details->inactive_play_pixwidget);

	} else {
		gtk_widget_hide(music_view->details->active_play_pixwidget);
		gtk_widget_show(music_view->details->inactive_play_pixwidget);
	}

	if (new_status == STATUS_PAUSE) {
		gtk_widget_show(music_view->details->active_pause_pixwidget);
		gtk_widget_hide(music_view->details->inactive_pause_pixwidget);

	} else {
		gtk_widget_hide(music_view->details->active_pause_pixwidget);
		gtk_widget_show(music_view->details->inactive_pause_pixwidget);
	}

}

/* utility to reset the playtime to the inactive state */
static void
reset_playtime (NautilusMusicView *music_view)
{
	gtk_adjustment_set_value(GTK_ADJUSTMENT(music_view->details->playtime_adjustment), 0.0);
 	gtk_range_set_adjustment(GTK_RANGE(music_view->details->playtime_bar), GTK_ADJUSTMENT(music_view->details->playtime_adjustment));	
	gtk_widget_set_sensitive(music_view->details->playtime_bar, FALSE);	
	gtk_label_set(GTK_LABEL(music_view->details->playtime), "--:--");	
}

/* status display timer task */

static int 
play_status_display (NautilusMusicView *music_view)
{
	int minutes, seconds;
	float percentage;
	char play_time_str[256];
	int frameNo, status;
	gboolean is_playing;
	int samps_per_frame;
	gfloat avgframesize;
	
	status = get_play_status();
	is_playing = (status == STATUS_PLAY) || (status == STATUS_PAUSE);
	
	if (status == STATUS_NEXT) {
		stop_playing_file();
		go_to_next_track(music_view);
		music_view->details->status_timeout = -1;		
		return FALSE;
	}
	
	if (music_view->details->last_play_status != status) {
		music_view->details->last_play_status = status;
		update_play_controls_status(music_view, status);
	}
	
	if (is_playing) {			
		if (!music_view->details->slider_dragging) {
			frameNo = get_current_frame();	
			samps_per_frame = (music_view->details->current_samprate >= 32000) ? 1152 : 576;
                        /* FIXME: Divide by zero possible here? */
			seconds = frameNo * samps_per_frame / music_view->details->current_samprate;
		
			minutes = seconds / 60;
			seconds = seconds % 60;
			sprintf(play_time_str, "%02d:%02d", minutes, seconds);
			
                        /* FIXME: Divide by zero possible here? */
			avgframesize = (gfloat)samps_per_frame * music_view->details->current_bitrate * 125 / music_view->details->current_samprate;
                        /* FIXME: Divide by zero possible here? */
			percentage = (gfloat) frameNo * avgframesize / music_view->details->current_file_size * 100;
			gtk_adjustment_set_value(GTK_ADJUSTMENT(music_view->details->playtime_adjustment), percentage);
 			gtk_range_set_adjustment(GTK_RANGE(music_view->details->playtime_bar), GTK_ADJUSTMENT(music_view->details->playtime_adjustment));	

			if (!music_view->details->slider_dragging)
		 		gtk_label_set(GTK_LABEL(music_view->details->playtime), play_time_str);	

		}
		
	} else 
		reset_playtime(music_view);

	if (!is_playing)
		music_view->details->status_timeout = -1;		
	
	return is_playing;
}

/* track incrementing routines */

static void
play_current_file (NautilusMusicView *music_view, gboolean from_start)
{
	int play_mode;
	char *song_filename, *temp_str, *song_uri;
        GnomeVFSResult result;
        GnomeVFSFileInfo file_info;
        
	play_mode = get_play_status();
	gtk_clist_select_row (GTK_CLIST(music_view->details->song_list), music_view->details->selected_index, 0);
	
	song_uri = gtk_clist_get_row_data (GTK_CLIST (music_view->details->song_list),
                                                music_view->details->selected_index);
	if (song_uri == NULL) {
		return;
	}
	song_filename = nautilus_get_local_path_from_uri(song_uri);
	
	/* set up the current bitrate and file size so we can give progress feedback */
        
        gtk_clist_get_text (GTK_CLIST(music_view->details->song_list),
                            music_view->details->selected_index, 4, &temp_str);
	music_view->details->current_bitrate = atoi (temp_str);
        gtk_clist_get_text (GTK_CLIST(music_view->details->song_list),
                            music_view->details->selected_index, 9, &temp_str);
	music_view->details->current_samprate = atoi (temp_str);
        result = gnome_vfs_get_file_info (song_uri, &file_info,
                                          GNOME_VFS_FILE_INFO_DEFAULT);
 	music_view->details->current_file_size =
                (result == GNOME_VFS_OK
                 && (file_info.valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) != 0)
                ? file_info.size
                : 0;

	gtk_widget_set_sensitive (music_view->details->playtime_bar, TRUE);
	
	if (music_view->details->status_timeout != -1)
		gtk_timeout_remove(music_view->details->status_timeout);
		
	music_view->details->status_timeout = gtk_timeout_add(900, (GtkFunction) play_status_display, music_view);
 	start_playing_file(song_filename, from_start || (play_mode != STATUS_PAUSE));
	g_free(song_filename);
}

static void
stop_if_playing(NautilusMusicView *music_view)
{
	int play_mode;
	
	play_mode = get_play_status();
	
	if (play_mode == STATUS_PLAY || play_mode == STATUS_PAUSE)
		stop_playing_file();
}

static void
go_to_next_track (NautilusMusicView *music_view)
{
	stop_if_playing(music_view);		
	if (music_view->details->selected_index < (GTK_CLIST(music_view->details->song_list)->rows - 1)) {
		music_view->details->selected_index += 1;
		play_current_file(music_view, TRUE);
	}
	else {  
		update_play_controls_status(music_view, STATUS_STOP);
		reset_playtime(music_view);
		stop_playing_file();
	}
}

static void
go_to_previous_track (NautilusMusicView *music_view) {
	int frame;
	
	stop_if_playing(music_view);		
	frame = get_current_frame();
	
	/* if we're in the first 3 seconds of the song, go to the previous one, otherwise go to the beginning of this track */	
	if ((frame < 3*72) && (music_view->details->selected_index > 0))
		music_view->details->selected_index -= 1;
	play_current_file(music_view, TRUE);
}


/* callback for the play control semantics */

/* callback for buttons */

static void
play_button_callback (GtkWidget * widget, NautilusMusicView *music_view)
{
	play_current_file(music_view, FALSE);
}

static void
stop_button_callback (GtkWidget * widget, NautilusMusicView *music_view)
{
	stop_playing_file();
}

static void
pause_button_callback (GtkWidget * widget, NautilusMusicView *music_view)
{
	pause_playing_file();
}

static void
prev_button_callback (GtkWidget * widget, NautilusMusicView *music_view)
{
	go_to_previous_track(music_view);
}

static void
next_button_callback (GtkWidget * widget, NautilusMusicView *music_view)
{
	go_to_next_track(music_view);
}

/* here are the  callbacks that handle seeking within a song by dragging the progress bar.
   "Mouse down" sets the slider_dragging boolean, "motion_notify" updates the label on the left, while
   "mouse up" actually moves the frame index.  */

/* handle slider button press */

static void
slider_press_callback(GtkWidget *bar, GdkEvent *event, NautilusMusicView *music_view)
{
	music_view->details->slider_dragging = TRUE;
}

/* handle mouse motion by updating the time, but not actually seeking until the user lets go */

static void
slider_moved_callback(GtkWidget *bar, GdkEvent *event, NautilusMusicView *music_view)
{
	char temp_str[256];
	int nframe, seconds, minutes;
	GtkAdjustment *adjustment;
	int samps_per_frame;
	gfloat avgframesize;
		
	if (music_view->details->slider_dragging) {
		adjustment = gtk_range_get_adjustment(GTK_RANGE(bar));
		samps_per_frame = (music_view->details->current_samprate >= 32000) ? 1152 : 576;
                /* FIXME: Divide by zero possible here? */
		avgframesize = (gfloat)samps_per_frame * music_view->details->current_bitrate * 125 / music_view->details->current_samprate;
                /* FIXME: Divide by zero possible here? */
		nframe = adjustment->value / (avgframesize / music_view->details->current_file_size * 100.0);	
                /* FIXME: Divide by zero possible here? */
		seconds = nframe * samps_per_frame / music_view->details->current_samprate; 
		minutes = seconds / 60;
		seconds = seconds % 60;
		sprintf(temp_str, "%02d:%02d", minutes, seconds);
		gtk_label_set(GTK_LABEL(music_view->details->playtime), temp_str);
	}
}
	
/* callback for slider button release - seek to desired location */
static void
slider_release_callback (GtkWidget *bar, GdkEvent *event, NautilusMusicView *music_view)
{
	int play_status, nframe;
	GtkAdjustment *adjustment;
	int samps_per_frame;
	gfloat avgframesize;
	
	play_status = get_play_status();
	if (music_view->details->slider_dragging) {
		adjustment = gtk_range_get_adjustment(GTK_RANGE(bar));
		samps_per_frame = (music_view->details->current_samprate >= 32000) ? 1152 : 576;
                /* FIXME: Divide by zero possible here? */
		avgframesize = (gfloat)samps_per_frame * music_view->details->current_bitrate * 125 / music_view->details->current_samprate;
                /* FIXME: Divide by zero possible here? */
		nframe = adjustment->value / (avgframesize / music_view->details->current_file_size * 100.0);	
		if ((play_status == STATUS_PLAY) || (play_status == STATUS_PAUSE)) {
			pause_playing_file ();
			set_current_frame (nframe);
			play_current_file (music_view, FALSE);
		}
		
	}	    
	music_view->details->slider_dragging = FALSE;
}

/* create a button with an xpm label */

static GtkWidget *
xpm_label_box (NautilusMusicView *music_view, gchar * xpm_data[])
{
	GtkWidget *box;
	GtkStyle *style;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkWidget *pix_widget;
	
	box = gtk_hbox_new(FALSE, 0);
	gtk_container_border_width(GTK_CONTAINER(box), 2);

	style = gtk_widget_get_style(GTK_WIDGET(music_view));

	pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(music_view)->window, &mask, &style->bg[GTK_STATE_NORMAL], xpm_data);
	pix_widget = gtk_pixmap_new(pixmap, mask);

	gtk_box_pack_start(GTK_BOX(box), pix_widget, TRUE, FALSE, 3);
	gtk_widget_show(pix_widget);

	return box;
}

/* creates a button with 2 internal pixwidgets, with only one visible at a time */

static GtkWidget *
xpm_dual_label_box (NautilusMusicView *music_view, char * xpm_data[],
                    gchar *alt_xpm_data[],
                    GtkWidget **main_pixwidget, GtkWidget **alt_pixwidget )
{
	GtkWidget *box;
	GtkStyle *style;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	
	box = gtk_hbox_new(FALSE, 0);
	gtk_container_border_width(GTK_CONTAINER(box), 2);

	style = gtk_widget_get_style(GTK_WIDGET(music_view));

	/* create the main pixwidget */
	pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(music_view)->window, &mask, &style->bg[GTK_STATE_NORMAL], xpm_data);
	*main_pixwidget = gtk_pixmap_new(pixmap, mask);

	gtk_box_pack_start(GTK_BOX(box), *main_pixwidget, TRUE, FALSE, 3);
	gtk_widget_show(*main_pixwidget);

	/* create the alternative pixwidget */
	pixmap = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(music_view)->window, &mask, &style->bg[GTK_STATE_NORMAL], alt_xpm_data);
	*alt_pixwidget = gtk_pixmap_new(pixmap, mask);

	gtk_box_pack_start(GTK_BOX(box), *alt_pixwidget, TRUE, FALSE, 3);
	gtk_widget_hide(*alt_pixwidget);

	return box;
}


/* add the play controls */

static void
add_play_controls (NautilusMusicView *music_view)
{
	GtkWidget *table;
	GtkWidget *box; 
	GtkWidget *vbox, *hbox2;
	GtkWidget *button;
	GtkTooltips *tooltips;
	
	tooltips = gtk_tooltips_new();

	table = gtk_table_new(3, 7, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 1);
	
	music_view->details->song_label = gtk_label_new(_("Song Title"));
	gtk_widget_show(music_view->details->song_label);
		
	vbox = gtk_vbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(music_view->details->control_box), vbox, FALSE, FALSE, 6);
	
	gtk_box_pack_start(GTK_BOX(vbox), music_view->details->song_label, FALSE, FALSE, 2);	
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 4);
	gtk_widget_show(vbox);
	
	music_view->details->play_control_box = vbox;
	
	/* playtime label */

	hbox2 = gtk_hbox_new(0, 0);
	gtk_table_attach(GTK_TABLE(table), hbox2, 0, 6, 0, 1, 0, 0, 0, 0);
	gtk_widget_show(hbox2);
	
	music_view->details->playtime = gtk_label_new("--:--");
	gtk_widget_show(music_view->details->playtime);
	gtk_box_pack_start(GTK_BOX(hbox2), music_view->details->playtime, FALSE, FALSE, 0);

	/* progress bar */

	music_view->details->playtime_adjustment = gtk_adjustment_new(0, 0, 101, 1, 5, 1);
	music_view->details->playtime_bar = gtk_hscale_new(GTK_ADJUSTMENT(music_view->details->playtime_adjustment));
	
	gtk_signal_connect(GTK_OBJECT(music_view->details->playtime_bar), "button_press_event", GTK_SIGNAL_FUNC(slider_press_callback), music_view);
	gtk_signal_connect(GTK_OBJECT(music_view->details->playtime_bar), "button_release_event", GTK_SIGNAL_FUNC(slider_release_callback), music_view);
 	gtk_signal_connect(GTK_OBJECT(music_view->details->playtime_bar), "motion_notify_event", GTK_SIGNAL_FUNC(slider_moved_callback), music_view);
   
   	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), music_view->details->playtime_bar, "Drag to seek within track", NULL);
	gtk_scale_set_draw_value(GTK_SCALE(music_view->details->playtime_bar), 0);
	gtk_widget_show(music_view->details->playtime_bar);
	gtk_widget_set_sensitive(music_view->details->playtime_bar, FALSE);
	gtk_box_pack_start(GTK_BOX(hbox2), music_view->details->playtime_bar, FALSE, FALSE, 4);	
	
	/* total label */

	music_view->details->total_track_time = gtk_label_new("--:--");
	gtk_widget_show(music_view->details->total_track_time);
	gtk_box_pack_start(GTK_BOX(hbox2), music_view->details->total_track_time, FALSE, FALSE, 0);

	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 5);
	gtk_widget_show(music_view->details->playtime_bar);

	/* buttons */

	/* previous track button */	
	box = xpm_label_box (music_view, prev_xpm);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, _("Previous"), NULL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect (GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(prev_button_callback), music_view);
	gtk_table_attach (GTK_TABLE(table), button, 0, 1, 1, 2, 0, 0, 0, 0);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_widget_show (button);

	/* play button */
	box = xpm_dual_label_box (music_view, play_xpm, play_green_xpm, &music_view->details->inactive_play_pixwidget,  &music_view->details->active_play_pixwidget);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, _("Play"), NULL);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect (GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(play_button_callback), music_view);
	gtk_table_attach (GTK_TABLE(table), button, 1, 2, 1, 2, 0, 0, 0, 0);
	gtk_widget_show (button);

	/* pause button */
	box = xpm_dual_label_box (music_view, pause_xpm, pause_green_xpm, &music_view->details->inactive_pause_pixwidget,  &music_view->details->active_pause_pixwidget);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, _("Pause"), NULL);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect (GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(pause_button_callback), music_view);
	gtk_table_attach (GTK_TABLE(table), button, 2, 3, 1, 2, 0, 0, 0, 0);
	gtk_widget_show (button);

	/* stop button */
	box = xpm_label_box (music_view, stop_xpm);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, _("Stop"), NULL);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(stop_button_callback), music_view);
	gtk_table_attach (GTK_TABLE(table), button, 3, 4, 1, 2, 0, 0, 0, 0);
	gtk_widget_show(button);

	/* next button */
	box = xpm_label_box (music_view, next_xpm);
	gtk_widget_show(box);
	button = gtk_button_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), button, _("Next"), NULL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add(GTK_CONTAINER(button), box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(next_button_callback), music_view);
	gtk_table_attach (GTK_TABLE(table), button, 4, 5, 1, 2, 0, 0, 0, 0);
	gtk_widget_show(button);

	gtk_widget_show(table);
}

/* here's where we do most of the real work of populating the view with info from the new uri */

static void
nautilus_music_view_update_from_uri (NautilusMusicView *music_view, const char *uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;

	char* clist_entry[10];
	GList *p;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;	
	GList *song_list;
	SongInfo *info;
	char *path_uri, *escaped_name;
	char *image_path, *image_path_uri;
	int file_index;
	int track_index;
	
	song_list = NULL;
	image_path_uri = NULL;
	file_index = 1;
	track_index = 0;

	/* connect the music view background to directory metadata */	
	nautilus_connect_background_to_directory_metadata_by_uri (GTK_WIDGET (music_view), uri);
			
	/* iterate through the directory, collecting mp3 files and extracting id3 data if present */

	result = gnome_vfs_directory_list_load (&list, uri,
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE, 
						NULL);
	if (result != GNOME_VFS_OK) {
		/* FIXME bugzilla.eazel.com 1280: need to show an alert here */
		g_warning("cant open %s in music_view_update", uri);		
		return;
	}
	
	current_file_info = gnome_vfs_directory_list_first (list);
	while (current_file_info != NULL) {
		/* skip invisible files, for now */
		if (current_file_info->name[0] == '.') {
                        current_file_info = gnome_vfs_directory_list_next(list);
                        continue;
                }
		
 		escaped_name = gnome_vfs_escape_string (current_file_info->name);
		path_uri = nautilus_make_path (uri, escaped_name);
		g_free(escaped_name);
			
		/* fetch info and queue it if it's an mp3 file */
		info = fetch_song_info (path_uri, current_file_info, file_index);
		if (info) {
			info->path_uri = path_uri;
			file_index += 1;
                        song_list = g_list_append (song_list, info);
		} else {
		        /* it's not an mp3 file, so see if it's an image */
        		const char *mime_type = gnome_vfs_file_info_get_mime_type
                                (current_file_info);
		        	
		        if (nautilus_istr_has_prefix (mime_type, "image/")) {
		        	/* for now, just keep the first image */
		        	if (image_path_uri == NULL) {
		        		image_path_uri = g_strdup (path_uri);
                                }
		        }
                        
		        g_free (path_uri);
		}
		
		current_file_info = gnome_vfs_directory_list_next(list);
	}
	gnome_vfs_directory_list_destroy(list);
	
	song_list = sort_song_list(music_view, song_list);
		
	/* populate the clist */
	
	gtk_clist_clear (GTK_CLIST (music_view->details->song_list));
	
	for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *) p->data;
		
		clist_entry[0] = malloc(4);
		if (info->track_number > 0)
			sprintf(clist_entry[0], "%d ", info->track_number);
		else	
			clist_entry[0] = '\0';
			
		clist_entry[1] = NULL;
		clist_entry[2] = NULL;
		clist_entry[3] = NULL;
		clist_entry[4] = NULL;
		clist_entry[5] = NULL;
		clist_entry[6] = NULL;
		clist_entry[7] = NULL;
		clist_entry[8] = NULL;
		clist_entry[9] = NULL;
		
		if (info->title)
			clist_entry[1] = g_strdup(info->title);
		if (info->artist)
			clist_entry[2] = g_strdup(info->artist);
		if (info->year)
			clist_entry[3] = g_strdup(info->year);
		if (info->bitrate > 0)
			clist_entry[4] = g_strdup_printf("%d ", info->bitrate);
		if (info->track_time > 0)
			clist_entry[5] = format_play_time (info->track_time);
		if (info->album)
			clist_entry[6] = g_strdup(info->album);
		if (info->comment)
			clist_entry[7] = g_strdup(info->comment);
			clist_entry[8] = g_strdup(info->stereo ? "Stereo" : "Mono");
		if (info->samprate > 0)
			clist_entry[9] = g_strdup_printf("%d", info->samprate);
			
		gtk_clist_append(GTK_CLIST(music_view->details->song_list), clist_entry);
		gtk_clist_set_row_data(GTK_CLIST(music_view->details->song_list),
					track_index, g_strdup(info->path_uri));
		
		track_index += 1;
	}
	
	/* install the album cover */
		
	if (image_path_uri != NULL) {
  		image_path = nautilus_get_local_path_from_uri(image_path_uri);
  		pixbuf = gdk_pixbuf_new_from_file(image_path);
		pixbuf = nautilus_gdk_pixbuf_scale_to_fit(pixbuf, 128, 128);

       		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 128);
		gdk_pixbuf_unref (pixbuf);
		
		if (music_view->details->album_image == NULL) {
			music_view->details->album_image = gtk_pixmap_new(pixmap, mask);
			gtk_box_pack_start(GTK_BOX(music_view->details->control_box), 
					   music_view->details->album_image, FALSE, FALSE, 2);	
		}
		else { 
			gtk_pixmap_set (GTK_PIXMAP (music_view->details->album_image), pixmap, mask);
		}
		
		gtk_widget_show (music_view->details->album_image);
 		g_free(image_path_uri);
 		g_free(image_path);
	} else if (music_view->details->album_image != NULL) {
		gtk_widget_hide (music_view->details->album_image);
        }
		
	/* determine the album title/artist line */
	
	if (music_view->details->album_title) {
		char *artist_name, *temp_str;
		char* album_name;

                album_name = determine_attribute (song_list, FALSE);
		if (album_name == NULL) {
			album_name = g_strdup (gnome_vfs_unescape_string_for_display(g_basename (uri)));
                }
		
		artist_name = determine_attribute (song_list, TRUE);
		if (artist_name != NULL) {
			temp_str = g_strdup_printf ("%s by %s", album_name, artist_name);
			g_free (artist_name);
		} else {
			temp_str = g_strdup (album_name);
                }
		gtk_label_set (GTK_LABEL (music_view->details->album_title), temp_str);
		
		g_free (temp_str);
		g_free (album_name);
	}


	/* allocate the play controls if necessary */
	
	if (music_view->details->play_control_box == NULL)
		add_play_controls(music_view);
	
	music_view_set_selected_song_title(music_view, 0);
	
	/* release the song list */
	for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *) p->data;
		release_song_info(info);
	}
	g_list_free (song_list);	
}


void
nautilus_music_view_load_uri (NautilusMusicView *music_view, const char *uri)
{
	g_free (music_view->details->uri);
  	music_view->details->uri = g_strdup (uri);	
	nautilus_music_view_update_from_uri (music_view, uri);
}

static void
music_view_load_location_callback (NautilusView *view, 
                                   const char *location,
                                   NautilusMusicView *music_view)
{
        nautilus_view_report_load_underway (music_view->details->nautilus_view);
	nautilus_music_view_load_uri (music_view, location);
        nautilus_view_report_load_complete (music_view->details->nautilus_view);
}

/* handle receiving dropped objects */
static void  
nautilus_music_view_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data, guint info, guint time)
{
	char **uris;

	g_return_if_fail (NAUTILUS_IS_MUSIC_VIEW (widget));

	uris = g_strsplit (selection_data->data, "\r\n", 0);

	switch (info) {
        case TARGET_GNOME_URI_LIST:
        case TARGET_URI_LIST: 	
                /* FIXME: the music view should accept mp3 files */
                g_message ("dropped data on music_view: %s", selection_data->data); 			
                break;
  		
                
        case TARGET_COLOR:
                /* Let the background change based on the dropped color. */
                nautilus_background_receive_dropped_color
                        (nautilus_get_widget_background (widget),
                         widget, x, y, selection_data);
                break;
  
  	case TARGET_BGIMAGE:
		nautilus_background_receive_dropped_background_image
			(nautilus_get_widget_background (widget),
                         uris[0]);
  		break;              

        default:
                g_warning ("unknown drop type");
                break;
        }
	
	g_strfreev (uris);
}

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
#include "pixmaps.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnorba/gnorba.h>
#include <limits.h>

struct _NautilusMusicViewDetails {
        char *uri;
        NautilusContentViewFrame *view_frame;
        
        int background_connection;
        int sort_mode;
	
        GtkVBox   *album_container;
        GtkWidget *album_title;
        GtkWidget *song_list;
        GtkWidget *album_image;
        GtkWidget *control_box;
	GtkWidget *play_control_box;
};


/* structure for holding song info */

typedef struct {
	int track_number;
	int bitrate;
	int track_time;
	
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
	SORT_BY_TIME
};

static int bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1};

static GtkTargetEntry music_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "property/bgimage", 0, TARGET_BGIMAGE },
	{ "special/x-gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

#define DEFAULT_BACKGROUND_COLOR  "rgb:DDDD/DDDD/BBBB"

static void nautilus_music_view_drag_data_received     (GtkWidget                *widget,
                                                        GdkDragContext           *context,
                                                        int                       x,
                                                        int                       y,
                                                        GtkSelectionData         *selection_data,
                                                        guint                     info,
                                                        guint                     time);
static void nautilus_music_view_initialize_class       (NautilusMusicViewClass   *klass);
static void nautilus_music_view_initialize             (NautilusMusicView        *view);
static void nautilus_music_view_destroy                (GtkObject                *object);
static void nautilus_music_view_update_from_uri        (NautilusMusicView *music_view, 
							const char *uri);
static void music_view_notify_location_change_callback (NautilusContentViewFrame *view,
                                                        Nautilus_NavigationInfo  *navinfo,
                                                        NautilusMusicView        *music_view);
static void selection_callback                         (GtkCList                 *clist,
                                                        int                       row,
                                                        int                       column,
                                                        GdkEventButton           *event);
static void add_play_controls 				(NautilusMusicView *music_view);

static void click_column_callback(GtkCList * clist, gint column, NautilusMusicView *music_view);

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
	char *titles[] = {_("Track "), _("Title"), _("Artist"), _("Bitrate "), _("Time ")};
	
	music_view->details = g_new0 (NautilusMusicViewDetails, 1);

	music_view->details->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (music_view));
	music_view->details->sort_mode = SORT_BY_NUMBER;
	
	gtk_signal_connect (GTK_OBJECT (music_view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (music_view_notify_location_change_callback), 
			    music_view);

	/* allocate a vbox to contain all of the views */
	
	music_view->details->album_container = GTK_VBOX (gtk_vbox_new (FALSE, 8));
	gtk_container_set_border_width (GTK_CONTAINER (music_view->details->album_container), 4);
	gtk_container_add (GTK_CONTAINER (music_view), GTK_WIDGET (music_view->details->album_container));
	
	gtk_widget_show (GTK_WIDGET (music_view->details->album_container));
	
	/* allocate a widget for the album title */
	
	music_view->details->album_title = gtk_label_new (_("Album Title"));
        /* FIXME bugzilla.eazel.com 667: don't use hardwired font like this */
	nautilus_gtk_widget_set_font_by_name (music_view->details->album_title,
                                              "-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*"); ;
	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->album_title, FALSE, FALSE, 0);	
	gtk_widget_show (music_view->details->album_title);
	
	/* allocate a list widget to hold the song list */

	music_view->details->song_list = gtk_clist_new_with_titles (5, titles);
		
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 0, 36);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 1, 208);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 2, 116);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 3, 42);	
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 4, 42);
 	
 	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 0, GTK_JUSTIFY_RIGHT);
 	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 3, GTK_JUSTIFY_RIGHT);
 	gtk_clist_set_column_justification(GTK_CLIST(music_view->details->song_list), 4, GTK_JUSTIFY_RIGHT);
 	
 	gtk_signal_connect (GTK_OBJECT (music_view->details->song_list),
                            "select_row", GTK_SIGNAL_FUNC (selection_callback), NULL);
 
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

        bonobo_object_unref (BONOBO_OBJECT (music_view->details->view_frame));

	g_free (music_view->details->uri);
	g_free (music_view->details);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* handle a row being selected in the list view by playing the corresponding song */
/* FIXME bugzilla.eazel.com 721: xmms shouldn't be hardwired */
static void 
selection_callback(GtkCList * clist, int row, int column, GdkEventButton * event)
{
        pid_t play_pid;
	char *song_name;

        song_name = gtk_clist_get_row_data (clist, row);
	if (song_name == NULL) {
		return;
        }
		
	/* fork off a task to play the sound file */
	if (!(play_pid = fork())) {
		execlp ("xmms", "xmms", song_name + 7, NULL);
   	   	exit (0); 
   	}		
} 

/* handle clicks in the songlist columns */

static void click_column_callback (GtkCList * clist, gint column, NautilusMusicView *music_view)
{
	if (music_view->details->sort_mode == column)
		return;
	music_view->details->sort_mode = column;
	nautilus_music_view_update_from_uri (music_view, music_view->details->uri);
}

/* Component embedding support */
NautilusContentViewFrame *
nautilus_music_view_get_view_frame (NautilusMusicView *music_view)
{
	return music_view->details->view_frame;
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

/* determine if the passed in filename is an mp3 file by looking at the extension */
/* FIXME: use mime-type for this? */
static gboolean
is_mp3_file(const char *song_uri)
{
	return nautilus_str_has_suffix(song_uri, ".mp3")
                || nautilus_str_has_suffix(song_uri, ".MP3");
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
	song_info->title = g_strdup(temp_str);
  
	strncpy (temp_str, &tag_buffer[33], 30);
	song_info->artist = g_strdup(temp_str);

	strncpy (temp_str, &tag_buffer[63], 30);
	song_info->album = g_strdup(temp_str); 

	temp_str[4] = '\0';
	strncpy (temp_str, &tag_buffer[93], 4);
	song_info->year = g_strdup(temp_str);

	strncpy (temp_str, &tag_buffer[97], 30);
	song_info->comment = g_strdup(temp_str);

    	if (tag_buffer[97 + 28] == 0) {
        	song_info->track_number = tag_buffer[97 + 29];
        } else {
                song_info->track_number = -1;
        }

	return TRUE;
}

/* this utility routine is the inner loop of fetch_bit_rate that scans the passed in buffer
   for a sync field.  If it finds a valid header, it returns the bit-rate index; otherwise, return -1 */
static int
scan_for_header(guchar  *buffer, int buffer_length)
{
	int index;
			
	for (index = 0; index < (buffer_length - 2); index++) {
		if ((buffer[index] == 255) && 
			((buffer[index + 1] & 224) != 0)) {
				return (buffer[index + 2] >> 4) & 15;
		}
	}
	
	return -1;
}

/* fetch_bit_rate returns the bit rate of the file by scanning for a frame and extracting the
   information from the frame header */
static int
fetch_bit_rate (const char *song_uri)
{
	guchar buffer[1024];
	GnomeVFSHandle *mp3_file;
	GnomeVFSResult result;
	GnomeVFSFileSize length_read;
	int bit_rate_index;

	/* open the file */
	
	result = gnome_vfs_open(&mp3_file, song_uri, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		return -1;
        }
	
	/* read a byte at a time until we get a sync field, which consists of a byte of 255 followed
	   by a byte with the next 3 bits on */
    	
    	bit_rate_index = -1;
    	while (TRUE) {
		result = gnome_vfs_read(mp3_file, buffer, sizeof(buffer), &length_read);
		if ((result != GNOME_VFS_OK) || (length_read < 3)) {
			break;
		}
		bit_rate_index = scan_for_header(buffer, length_read);
    		if (bit_rate_index >= 0)
    			break;
	};
		
    	gnome_vfs_close(mp3_file);
    	
    	/* fetch the bitrate field, and look up the actual bitrate in the table */
    	if (bit_rate_index < 0)
    		return -1;
    			
	return bitrates[bit_rate_index];
}

/* fetch_play_time takes the pathname to a file and returns the play time in seconds */
static int
fetch_play_time (const char *song_uri, int bitrate)
{
	NautilusFile *file = nautilus_file_get (song_uri + 7);
 	GnomeVFSFileSize file_size = nautilus_file_get_size (file);       			
	nautilus_file_unref(file);
	
	return file_size / (125 * bitrate);
}

/* format_play_time takes the pathname to a file and returns the play time formated as mm:ss */
static char *
format_play_time (const char *song_uri, int bitrate)
{
	int seconds = fetch_play_time(song_uri, bitrate);
	int minutes = seconds / 60;
	int remain_seconds = seconds - (60 * minutes);
	char *result = g_strdup_printf ("%d:%02d ", minutes, remain_seconds);
	return result;
}

/* utility routine to pull an initial number from the beginning of the passed in name.
   return -1 if there wasn't any */
static int
extract_initial_number(const char *name_str)
{
	char *temp_str;
	gboolean found_digit;
	int accumulator;
	
	found_digit = FALSE;
	accumulator = 0;
	temp_str = (char*) name_str;
	
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
fetch_song_info (const char *song_uri, int file_order) 
{
	gboolean has_info = FALSE;
	SongInfo *info; 

	if (!is_mp3_file (song_uri)) {
		return NULL;
        }

	info = g_new0 (SongInfo, 1); 
	initialize_song_info (info);
	
	has_info = read_id_tag (song_uri, info);

	/* if we couldn't get a track number, see if we can pull one from
	   the beginning of the file name */
	if (info->track_number <= 0) {
		info->track_number = extract_initial_number(g_basename(song_uri));
	}
  	
	/* there was no id3 tag, so set up the info heuristically from the file name and file order */
	if (!has_info) {
		info->title = g_strdup (g_basename (song_uri));
	}	
	
	info->bitrate = fetch_bit_rate(song_uri);
	info->track_time = fetch_play_time(song_uri, info->bitrate);
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

	return strcmp(a->title, b->title);
}

static int
sort_by_artist (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return strcmp(a->artist, b->artist);
}

static int
sort_by_time (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return a->track_time - b->track_time;
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

/* handle the "background changed" signal */

static void
nautilus_music_view_background_changed (NautilusMusicView *music_view)
{
	NautilusDirectory *directory;
	NautilusBackground *background;
	char *color_spec, *image;
	
	directory = nautilus_directory_get (music_view->details->uri);
	background = nautilus_get_widget_background (GTK_WIDGET (music_view));
	
	color_spec = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (directory,
					 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
					 DEFAULT_BACKGROUND_COLOR,
					 color_spec);	
	g_free (color_spec);

	
	image = nautilus_background_get_tile_image_uri (background);
	nautilus_directory_set_metadata (directory,
					 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
					 NULL,
					 image);	
	g_free (image);
	nautilus_directory_unref(directory);
}

/* set up the background of the music view from the metadata associated with the uri */

static void
nautilus_music_view_set_up_background (NautilusMusicView *music_view, const char *uri)
{
	NautilusDirectory *directory;
	NautilusBackground *background;
	char *background_color;
	char *background_image;
	
	directory = nautilus_directory_get (music_view->details->uri);
	
	/* Connect the background changed signal to code that writes the color. */
	background = nautilus_get_widget_background (GTK_WIDGET (music_view));
        if (music_view->details->background_connection == 0) {
		music_view->details->background_connection =
			gtk_signal_connect_object (GTK_OBJECT (background),
						   "changed",
						   nautilus_music_view_background_changed,
						   GTK_OBJECT (music_view));
	}

	/* Set up the background color and image from the metadata. */
	background_color = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
							    DEFAULT_BACKGROUND_COLOR);
	background_image = nautilus_directory_get_metadata (directory,
							    NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
							    NULL);
	nautilus_directory_unref(directory);
	
	nautilus_background_set_color (background, background_color);	
	g_free (background_color);
	
	nautilus_background_set_tile_image_uri (background, background_image);
	g_free (background_image);
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
		case SORT_BY_TIME:
			song_list = g_list_sort (song_list, sort_by_time);
			break;
		default:
			g_warning("unknown sort mode");
			break;
	}
	
	return song_list;
}

/* callback for the play control semantics */

/* callback for buttons */

static void button_callback (GtkWidget * widget, gint which_button)
{
	g_message("button %d clicked", which_button);
}

/* here are the  callbacks that handle seeking within a song by dragging the progress bar.
   "Mouse down" sets the slider_dragging boolean, "motion_notify" updates the label on the left, while
   "mouse up" actually moves the frame index.  */

/* handle slider button press */

static void slider_press_callback(GtkWidget *bar, GdkEvent *event, void *dummy)
{
	g_message("slider pressed");

}

/* handle mouse motion */

static void slider_moved_callback(GtkWidget *bar, void* header)
{
	g_message("slider moved");
}
	
/* callback for slider button release */
static void slider_release_callback(GtkWidget *bar, GdkEvent *event, void *dummy)
{
	g_message("slider released");
}
	
/* callback for volume */
static void volume_callback(GtkAdjustment *gadj, void *header)
{
 	gint int_vol = gadj->value;
 	g_message("volume is %d", int_vol);
 	/* set_volume(int_vol); */
}


/* create a button with an xpm label */

static GtkWidget *xpm_label_box (NautilusMusicView *music_view, gchar * xpm_data[])
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

/* add the play controls */

static void add_play_controls (NautilusMusicView *music_view)
	{
	GtkWidget *table;
	GtkWidget *box; 
	GtkWidget *hbox, *hbox2;
	GtkWidget *button;
	GtkObject *adj, *v_adj;
	GtkWidget *playtime, *bar, *volume_b, *total_track_time;
	GtkTooltips *tooltips;
	
	tooltips = gtk_tooltips_new();

	table = gtk_table_new(3, 7, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 1);
	
	hbox = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(music_view->details->control_box), hbox, 0, 0, 5);
	gtk_box_pack_start(GTK_BOX(hbox), table, 0, 0, 6);
	gtk_widget_show(hbox);
	
	music_view->details->play_control_box = hbox;
	
	/* playtime label */

	hbox2 = gtk_hbox_new(0, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox2, 0, 6, 0, 1);
	gtk_widget_show(hbox2);
	
	playtime = gtk_label_new("--:--");
	gtk_widget_show(playtime);
	gtk_box_pack_start(GTK_BOX(hbox2), playtime, 0, 0, 0);

	/* progress bar */

	adj = gtk_adjustment_new(0, 0, 101, 1, 5, 1);
	bar = gtk_hscale_new(GTK_ADJUSTMENT(adj));
	gtk_signal_connect(GTK_OBJECT(bar), "button_press_event", GTK_SIGNAL_FUNC(slider_press_callback), NULL);
	gtk_signal_connect(GTK_OBJECT(bar), "button_release_event", GTK_SIGNAL_FUNC(slider_release_callback), NULL);
 	gtk_signal_connect(GTK_OBJECT(bar), "motion_notify_event", GTK_SIGNAL_FUNC(slider_moved_callback), NULL);
   
   	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), bar, "Drag to seek within track", NULL);
	gtk_scale_set_draw_value(GTK_SCALE(bar), 0);
	gtk_widget_show(bar);
	gtk_box_pack_start(GTK_BOX(hbox2), bar, 1, 1, 5);

	/* volume bar */

	/*
	gtk_adjustment_set_value(GTK_ADJUSTMENT(v_adj), get_volume() / 1.0);
	*/
	
	gtk_signal_connect(GTK_OBJECT(v_adj), "value_changed", GTK_SIGNAL_FUNC(volume_callback), NULL);

	volume_b = gtk_vscale_new(GTK_ADJUSTMENT(v_adj));
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), volume_b, "PCM mixer volume", NULL);
	gtk_scale_set_draw_value(GTK_SCALE(volume_b), 0);
	gtk_widget_show(volume_b);
	gtk_table_attach(GTK_TABLE(table), volume_b, 6, 7, 0, 3, 0, GTK_FILL | GTK_EXPAND, 5, 0);

	/* total label */

	total_track_time = gtk_label_new("--:--");
	gtk_widget_show(total_track_time);
	gtk_box_pack_start(GTK_BOX(hbox2), total_track_time, 0, 0, 0);

	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 5);
	gtk_widget_show(bar);

	/* buttons */

	/* previous track button */	
	box = xpm_label_box (music_view, prev_xpm);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, "Previous", NULL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect (GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_callback), (gpointer)  PREVIOUS_BUTTON);
	gtk_table_attach_defaults (GTK_TABLE(table), button, 0, 1, 1, 2);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_widget_show (button);

	/* play button */
	box = xpm_label_box (music_view, play_xpm);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, "Play", NULL);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect (GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_callback), (gpointer)  PLAY_BUTTON);
	gtk_table_attach_defaults (GTK_TABLE(table), button, 1, 2, 1, 2);
	gtk_widget_show (button);

	/* pause button */
	box = xpm_label_box (music_view, pause_xpm);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, "Pause", NULL);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect (GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_callback), (gpointer)  PAUSE_BUTTON);
	gtk_table_attach_defaults (GTK_TABLE(table), button, 2, 3, 1, 2);
	gtk_widget_show (button);

	/* stop button */
	box = xpm_label_box (music_view, stop_xpm);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS(tooltips), button, "Stop", NULL);
	gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER(button), box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_callback), (gpointer)  STOP_BUTTON);
	gtk_table_attach_defaults(GTK_TABLE(table), button, 3, 4, 1, 2);
	gtk_widget_show(button);

	/* next button */
	box = xpm_label_box (music_view, next_xpm);
	gtk_widget_show(box);
	button = gtk_button_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltips), button, "Next", NULL);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NORMAL);
	gtk_container_add(GTK_CONTAINER(button), box);

	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_callback), (gpointer) NEXT_BUTTON);
	gtk_table_attach_defaults(GTK_TABLE(table), button, 4, 5, 1, 2);
	gtk_widget_show(button);
}

/* here's where we do most of the real work of populating the view with info from the new uri */

static void
nautilus_music_view_update_from_uri (NautilusMusicView *music_view, const char *uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GnomeVFSDirectoryList *list;

	char* clist_entry[5];
	GList *p;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;	
	GList *song_list;
	SongInfo *info;
	char *path_uri;
	char *image_path_uri;
	int file_index;
	int track_index;
	
	song_list = NULL;
	image_path_uri = NULL;
	file_index = 1;
	track_index = 0;

	/* set up the background from the metadata */	
	nautilus_music_view_set_up_background(music_view, uri);
	
	/* allocate the play controls if necessary */
	
	/* if (music_view->details->play_control_box == NULL) */
	if (FALSE) /* disable temporarily for checkin */
		add_play_controls(music_view);
		
	/* iterate through the directory, collecting mp3 files and extracting id3 data if present */

	result = gnome_vfs_directory_list_load (&list, uri,
						GNOME_VFS_FILE_INFO_GETMIMETYPE, 
						NULL, NULL);
	if (result != GNOME_VFS_OK) {
		/* FIXME: need to show an alert here */
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
		
		path_uri = nautilus_make_path(uri, current_file_info->name);
			
		/* fetch info and queue it if it's an mp3 file */
		info = fetch_song_info (path_uri, file_index);
		if (info) {
			
			info->path_uri = path_uri;
			file_index += 1;
                        song_list = g_list_append (song_list, info);
		} else {
		        /* it's not an mp3 file, so see if it's an image */
		        NautilusFile *file = nautilus_file_get (path_uri + 7);
        		char *mime_type = nautilus_file_get_mime_type (file);
		        	
		        if (nautilus_str_has_prefix (mime_type, "image/")) {
		        	/* for now, just keep the first image */
		        	if (image_path_uri == NULL) {
		        		image_path_uri = g_strdup (path_uri);
                                }
		        }
		        	
		        nautilus_file_unref (file);
		        g_free (path_uri);
                        g_free (mime_type);
		}
		
		current_file_info = gnome_vfs_directory_list_next(list);
	}
	gnome_vfs_directory_list_destroy(list);
	
	song_list = sort_song_list(music_view, song_list);
		
	/* populate the clist */
	
	gtk_clist_clear (GTK_CLIST (music_view->details->song_list));
	
	for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *)p->data;
		
		clist_entry[0] = malloc(4);
		if (info->track_number > 0)
			sprintf(clist_entry[0], "%d ", info->track_number);
		else	
			clist_entry[0] = '\0';
			
		clist_entry[1] = NULL;
		clist_entry[2] = NULL;
		clist_entry[3] = NULL;
		clist_entry[4] = NULL;
		
		if (info->title)
			clist_entry[1] = g_strdup(info->title);
		if (info->artist)
			clist_entry[2] = g_strdup(info->artist);
		if (info->bitrate > 0)
			clist_entry[3] = g_strdup_printf("%d ", info->bitrate);
		if (info->path_uri)
			clist_entry[4] = format_play_time(info->path_uri, info->bitrate);
		if (info->bitrate > 0)
			
		gtk_clist_append(GTK_CLIST(music_view->details->song_list), clist_entry);
		gtk_clist_set_row_data(GTK_CLIST(music_view->details->song_list),
					track_index, g_strdup(info->path_uri));
		
		track_index += 1;
	}
	
	/* install the album cover */
		
	if (image_path_uri != NULL) {
		pixbuf = gdk_pixbuf_new_from_file(image_path_uri + 7);
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
 	} else if (music_view->details->album_image != NULL) {
		gtk_widget_hide (music_view->details->album_image);
        }
		
	/* determine the album title/artist line */
	
	if (music_view->details->album_title) {
		char *artist_name, *temp_str;
		char* album_name;

                album_name = determine_attribute (song_list, FALSE);
		if (album_name == NULL) {
			album_name = g_strdup (g_basename (uri));
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
music_view_notify_location_change_callback (NautilusContentViewFrame *view, 
                                            Nautilus_NavigationInfo *navinfo, 
                                            NautilusMusicView *music_view)
{
	Nautilus_ProgressRequestInfo progress;

 	memset(&progress, 0, sizeof(progress));

	/* send required PROGRESS_UNDERWAY signal */
  
	progress.type = Nautilus_PROGRESS_UNDERWAY;
	progress.amount = 0.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (music_view->details->view_frame), &progress);

	/* do the actual work here */
	nautilus_music_view_load_uri (music_view, navinfo->actual_uri);

	/* send the required PROGRESS_DONE signal */
	progress.type = Nautilus_PROGRESS_DONE_OK;
	progress.amount = 100.0;
	nautilus_view_frame_request_progress_change (NAUTILUS_VIEW_FRAME (music_view->details->view_frame), &progress);
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
                g_message ("dropped data on music_view: %s", selection_data->data); 			
                break;
  		
                
        case TARGET_COLOR:
                /* Let the background change based on the dropped color. */
                nautilus_background_receive_dropped_color (nautilus_get_widget_background (widget),
                                                           widget, x, y, selection_data);
                break;
  
  	case TARGET_BGIMAGE:
		nautilus_background_set_tile_image_uri
			(nautilus_get_widget_background (widget),
		 	uris[0]);

  		break;              
        default:
                g_warning ("unknown drop type");
                break;
        }
	
	g_strfreev (uris);
}

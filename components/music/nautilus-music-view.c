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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <libnautilus/libnautilus.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-directory-background.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
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
        
        GtkVBox *album_container;
        GtkWidget *album_title;
        GtkWidget *song_list;
        GtkWidget *album_image;
};

/* structure for holding song info */

typedef struct {
	int track_number;
	char *title;
	char *artist;
	char *album;
	char *year;
        char *comment;
        char *path_name;
} SongInfo;

enum {
	TARGET_URI_LIST,
	TARGET_COLOR,
        TARGET_GNOME_URI_LIST
};

static GtkTargetEntry music_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "special/x-gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

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
static void music_view_notify_location_change_callback (NautilusContentViewFrame *view,
                                                        Nautilus_NavigationInfo  *navinfo,
                                                        NautilusMusicView        *music_view);
static void selection_callback                         (GtkCList                 *clist,
                                                        int                       row,
                                                        int                       column,
                                                        GdkEventButton           *event);

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
	char *file_name;
	GtkWidget *song_box, *scrollwindow;
	char *titles[] = {_("Track"), _("Title"), _("Artist"), _("Time")};
	
	music_view->details = g_new0 (NautilusMusicViewDetails, 1);

	music_view->details->view_frame = nautilus_content_view_frame_new (GTK_WIDGET (music_view));

	gtk_signal_connect (GTK_OBJECT (music_view->details->view_frame), 
			    "notify_location_change",
			    GTK_SIGNAL_FUNC (music_view_notify_location_change_callback), 
			    music_view);

	/* allocate a vbox to contain all of the views */
	
	music_view->details->album_container = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	gtk_container_add (GTK_CONTAINER (music_view), GTK_WIDGET (music_view->details->album_container));
	gtk_widget_show (GTK_WIDGET (music_view->details->album_container));
	
	/* allocate a widget for the album title */
	
	music_view->details->album_title = gtk_label_new (_("Album Title"));
        /* FIXME bugzilla.eazel.com 667: don't use hardwired font like this */
	nautilus_gtk_widget_set_font_by_name (music_view->details->album_title,
                                              "-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-*-*"); ;
	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->album_title, 0, 0, 0);	
	gtk_widget_show (music_view->details->album_title);
	
	/* allocate an hbox to hold the optional album cover and the song list */
	
	song_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), song_box, 0, 0, 2);	
	gtk_widget_show (song_box);
	
	/* allocate a placeholder widget for the album cover, but don't show it yet */
  	file_name = gnome_pixmap_file ("nautilus/i-directory.png");
  	music_view->details->album_image = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
	gtk_box_pack_start(GTK_BOX(song_box), music_view->details->album_image, 0, 0, 0);		
  	g_free (file_name);
	
	/* allocate a widget to hold the song list */

	music_view->details->song_list = gtk_clist_new_with_titles (4, titles);
		
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 0, 32);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 1, 172);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 2, 96);
	gtk_clist_set_column_width (GTK_CLIST (music_view->details->song_list), 3, 42);
 	
 	gtk_signal_connect (GTK_OBJECT (music_view->details->song_list),
                            "select_row", GTK_SIGNAL_FUNC (selection_callback), NULL);
 
	scrollwindow = gtk_scrolled_window_new (NULL, gtk_clist_get_vadjustment (GTK_CLIST (music_view->details->song_list)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);   
	gtk_widget_set_usize (scrollwindow, 384, 232);
	gtk_container_add (GTK_CONTAINER (scrollwindow), music_view->details->song_list);	
	gtk_clist_set_selection_mode (GTK_CLIST (music_view->details->song_list), GTK_SELECTION_BROWSE);

	gtk_box_pack_start (GTK_BOX (song_box), scrollwindow, 0, 0, 4);	
	gtk_widget_show (music_view->details->song_list);
	gtk_widget_show (scrollwindow);

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
		execlp ("xmms", "xmms", song_name, NULL);
   	   	exit (0); 
   	}		
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
        g_free (info->path_name);
	g_free (info);
}

/* determine if the passed in filename is an mp3 file by looking at the extension */
static gboolean
is_mp3_file(const char *song_path)
{
	return nautilus_str_has_suffix(song_path, ".mp3")
                || nautilus_str_has_suffix(song_path, ".MP3");
}

/* read the id3 tag of the file if present */
/* FIXME bugzilla.eazel.com 722: need to use gnome vfs for this */

static gboolean
read_id_tag (const char *song_path, SongInfo *song_info)
{
	int mp3_file;

	char tag_buffer[129];
	char temp_str[31];

	mp3_file = open(song_path, O_RDONLY);
	if (mp3_file == 0) {
		return FALSE;
        }
  
	lseek (mp3_file, -128, SEEK_END);
  
	if (read (mp3_file, tag_buffer, 129) <= 0) {
  		close (mp3_file);
  		return FALSE;
	}
	close (mp3_file);

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

/* allocate a return a song info record, from an mp3 tag if present, or from intrinsic info */

static SongInfo *
fetch_song_info (const char *song_path, int file_order) 
{
	gboolean has_info = FALSE;
	SongInfo *info; 

	if (!is_mp3_file (song_path)) {
		return NULL;
        }

	info = g_new0 (SongInfo, 1); 
	initialize_song_info (info);
	
	if (is_mp3_file (song_path)) {
		has_info = read_id_tag (song_path, info);
        }
	
	/* there was no id3 tag, so set up the info heuristically from the file name and file order */
	if (!has_info) {
		info->title = g_strdup (g_basename (song_path));
		info->track_number = file_order;
	}	
	
	return	info;
}

/* format_play_time takes the pathname to a file and returns the play time formated as mm:ss */
/* FIXME bugzilla.eazel.com 723: assumes 128k bits/second.  Must read header and factor in bitrate */

static char *
format_play_time (const char *song_path_name)
{
	NautilusFile *file = nautilus_file_get (song_path_name);
 	GnomeVFSFileSize file_size = nautilus_file_get_size (file);       			
	int seconds = (file_size - 512) / 16384;
	int minutes = seconds / 60;
	int remain_seconds = seconds - (60 * minutes);
	char *result = g_strdup_printf ("%d:%02d", minutes, remain_seconds);
	nautilus_file_unref (file);
	return result;
}

/* sort comparison routine - for now, just sort by track number */

static int
sort_by_track_number (gconstpointer ap, gconstpointer bp)
{
	SongInfo *a, *b;

	a = (SongInfo *) ap;
	b = (SongInfo *) bp;

	return (int) a->track_number - b->track_number;
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

/* here's where we do most of the real work of populating the view with info from the new uri */
/* FIXME bugzilla.eazel.com 722: need to use gnome-vfs for iterating the directory */

static void
nautilus_music_view_update_from_uri (NautilusMusicView *music_view, const char *uri)
{
	DIR *dir;
	struct dirent *entry;
	char* clist_entry[4];
	GList *p;
	GList *song_list = NULL ;
	SongInfo *info;
	char *path_name;
	char *image_path_name = NULL;
	int file_index = 1;
	int track_index = 0;

	/* iterate through the directory, collecting mp3 files and extracting id3 data if present */
	/* soon we'll use gnomevfs, but at first just the standard unix stuff */
	
	if ((dir = opendir (uri + 7)) == NULL)
		g_warning("cant open %s in music_view_update", uri);
	else {
  		while ((entry = readdir(dir)) != NULL) {
			/* skip invisible files, for now */
			
			if (entry->d_name[0] == '.') {
				continue;
                        }
			
			path_name = nautilus_make_path(uri + 7, entry->d_name);
			
			/* fetch info and queue it if it's an mp3 file */
			info = fetch_song_info (path_name, file_index);
			if (info) {
				info->path_name = path_name;
				file_index += 1;
                                song_list = g_list_append (song_list, info);
			} else {
		        	/* it's not an mp3 file, so see if it's an image */
		        	NautilusFile *file = nautilus_file_get (path_name);
        			const char *mime_type = nautilus_file_get_mime_type (file);
		        	
		        	if (nautilus_str_has_prefix (mime_type, "image/")) {
		        		/* for now, just keep the first image */
		        		if (image_path_name == NULL) {
		        			image_path_name = g_strdup (path_name);
                                        }
		        	}
		        	
		        	nautilus_file_unref (file);
		        	g_free (path_name);
			}
		}
		
		closedir(dir);
	 }
	
	/* sort by track number */	
	song_list = g_list_sort (song_list, sort_by_track_number);
	
	/* populate the clist */
	
	gtk_clist_clear (GTK_CLIST (music_view->details->song_list));
	
	for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *)p->data;
		
		clist_entry[0] = malloc(4);
		if (info->track_number > 0)
			sprintf(clist_entry[0], "%d", info->track_number);
		else	
			clist_entry[0] = '\0';
			
		clist_entry[1] = NULL;
		clist_entry[2] = NULL;
		clist_entry[3] = NULL;
		
		if (info->title)
			clist_entry[1] = g_strdup(info->title);
		if (info->artist)
			clist_entry[2] = g_strdup(info->artist);
		if (info->path_name)
		clist_entry[3] = format_play_time(info->path_name);
			
		gtk_clist_append(GTK_CLIST(music_view->details->song_list), clist_entry);
		gtk_clist_set_row_data(GTK_CLIST(music_view->details->song_list), track_index, g_strdup(info->path_name));
		
		track_index += 1;
	}
	
	/* install the album cover */
	
	if (image_path_name != NULL) {
		gnome_pixmap_load_file (GNOME_PIXMAP (music_view->details->album_image),
                                        image_path_name);			
		gtk_widget_show (music_view->details->album_image);
 		g_free (image_path_name);
	} else {
		gtk_widget_hide (music_view->details->album_image);
        }
	
	/* set up background color */
	nautilus_connect_background_to_directory_metadata_by_uri
                (GTK_WIDGET (music_view), music_view->details->uri);
	
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

/* handle drag and drop */

static void  
nautilus_music_view_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					 int x, int y,
					 GtkSelectionData *selection_data, guint info, guint time)
{
	g_return_if_fail (NAUTILUS_IS_MUSIC_VIEW (widget));

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
                
        default:
                g_warning ("unknown drop type");
                break;
        }
}

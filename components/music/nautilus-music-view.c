/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000, 2001 Eazel, Inc.
 *  Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  by the Free Software Foundation; either version 2 of the
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
 *  Authors: Andy Hertzfeld <andy@eazel.com>
 *           Anders Carlsson <andersca@gnu.org>
 *
 */

/* music view - presents the contents of the directory as an album of music */

#include <config.h>
#include "nautilus-music-view.h"

#include "esd-audio.h"
#include "mp3head.h"
#include "mpg123.h"
#include "pixmaps.h"
#include <eel/eel-background.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image.h>
#include <eel/eel-preferences.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-string.h>
#include <esd.h>
#include <fcntl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreeview.h>
#include <libgnomevfs/gnome-vfs-directory.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-directory-notify.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-font-factory.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-sound.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SCALED_IMAGE_WIDTH	108
#define SCALED_IMAGE_HEIGHT 	108

typedef enum {
	PLAYER_STOPPED,
	PLAYER_PAUSED,
	PLAYER_PLAYING,
	PLAYER_NEXT
} PlayerState;

struct NautilusMusicViewDetails {
        NautilusFile *file;
	GtkWidget *event_box;
        
	int selected_index;
	int status_timeout;
	
	int current_samprate;
	int current_duration;
		
	gboolean slider_dragging;
	
        GtkListStore *list_store;
        GtkWidget *tree_view;
        
	GtkVBox   *album_container;
	GtkWidget *scroll_window;
	GtkWidget *album_title;
	GtkWidget *album_image;
	GtkWidget *image_box;
	GtkWidget *dialog;
        
	GtkWidget *control_box;
	GtkWidget *play_control_box;

	GtkWidget *song_label;
	
	GtkWidget *playtime;
	GtkWidget *playtime_bar;
	GtkObject *playtime_adjustment;

	GtkWidget *previous_track_button;
	GtkWidget *pause_button;
	GtkWidget *stop_button;
	GtkWidget *next_track_button;
	GtkWidget *inactive_play_pixwidget;
	GtkWidget *active_play_pixwidget;
	GtkWidget *inactive_pause_pixwidget;
	GtkWidget *active_pause_pixwidget;

	PlayerState player_state;
	PlayerState last_player_state;
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

/* list columns */
enum {
        TRACK_NUMBER_COLUMN,
        TITLE_COLUMN,
        ARTIST_COLUMN,
        BITRATE_COLUMN,
        TIME_COLUMN,
        SAMPLE_RATE_COLUMN,
        YEAR_COLUMN,
        COMMENT_COLUMN,
        PATH_URI_COLUMN,
        ALBUM_COLUMN,
        NUM_COLUMNS
};

/* button commands */
enum {
	PREVIOUS_BUTTON,
	PLAY_BUTTON,
	PAUSE_BUTTON,
	STOP_BUTTON,
	NEXT_BUTTON
};

static GtkTargetEntry music_dnd_target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "application/x-color", 0, TARGET_COLOR },
	{ "property/bgimage", 0, TARGET_BGIMAGE },
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST }
};

static void nautilus_music_view_drag_data_received            (GtkWidget              *widget,
                                                               GdkDragContext         *context,
                                                               int                     x,
                                                               int                     y,
                                                               GtkSelectionData       *selection_data,
                                                               guint                   info,
                                                               guint                   time,
                                                               gpointer                user_data);
static void nautilus_music_view_class_init              (NautilusMusicViewClass *klass);
static void nautilus_music_view_init                    (NautilusMusicView      *view);
static void nautilus_music_view_destroy                       (BonoboObject           *object);
static void nautilus_music_view_finalize                      (GObject                *object);

static void nautilus_music_view_update                        (NautilusMusicView      *music_view);
static void music_view_background_appearance_changed_callback (EelBackground     *background,
                                                               NautilusMusicView      *music_view);
static void music_view_load_location_callback                 (NautilusView           *view,
                                                               const char             *location,
                                                               NautilusMusicView      *music_view);

static void selection_changed                                 (GtkTreeSelection       *selection,
                                                               NautilusMusicView      *music_view);
static void row_activated_callback                            (GtkTreeView            *tree_view,
                                                               GtkTreePath            *path,
                                                               GtkTreeViewColumn      *column,
                                                               NautilusMusicView      *music_view);
static void nautilus_music_view_set_album_image               (NautilusMusicView      *music_view,
                                                               const char             *image_path_uri);
static void image_button_callback                             (GtkWidget              *widget,
                                                               NautilusMusicView      *music_view);
static void go_to_next_track                                  (NautilusMusicView      *music_view);
static void play_current_file                                 (NautilusMusicView      *music_view,
                                                               gboolean                from_start);
static void detach_file                                       (NautilusMusicView      *music_view);
static void start_playing_file 				      (NautilusMusicView      *music_view, 
							       const char 	      *file_name);
static void stop_playing_file 				      (NautilusMusicView      *music_view);
static PlayerState get_player_state 			      (NautilusMusicView      *music_view);
static void set_player_state 				      (NautilusMusicView      *music_view, 
							       PlayerState 	       state);

static void list_reveal_row                                   (GtkTreeView            *view, 
                                                               int                     row_index);


static void nautilus_music_view_load_uri (NautilusMusicView *view,
                                          const char        *uri);



EEL_CLASS_BOILERPLATE (NautilusMusicView,
                                   nautilus_music_view,
                                   NAUTILUS_TYPE_VIEW)

static void
nautilus_music_view_class_init (NautilusMusicViewClass *klass)
{
	GObjectClass *gobject_class;
	BonoboObjectClass *object_class;

	gobject_class = G_OBJECT_CLASS (klass);
	object_class = BONOBO_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_music_view_destroy;
        gobject_class->finalize = nautilus_music_view_finalize;
}


static void
track_cell_data_func (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
        int track_number;
        char *str;
        
        gtk_tree_model_get (tree_model,
                            iter,
                            TRACK_NUMBER_COLUMN, &track_number,
                            -1);

        /* Don't show the track number if it's lower than 1 */
        if (track_number < 1) {
                str = NULL;
        }
        else {
                str = g_strdup_printf ("%d", track_number);
        }

        g_object_set (cell,
                      "text", str,
                      NULL);

                      g_free (str);
}

static void
bitrate_cell_data_func (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
        int bitrate;
        char *str;
        
        gtk_tree_model_get (tree_model,
                            iter,
                            BITRATE_COLUMN, &bitrate,
                            -1);

        if (bitrate <= 0) {
                str = g_strdup (_("Unknown"));
        }
        else {
                str = g_strdup_printf ("%d kbps", bitrate);
        }

        g_object_set (cell,
                      "text", str,
                      NULL);

                      g_free (str);
}

static void
time_cell_data_func (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
        int time;
	int seconds, minutes, remain_seconds;
        char *str;

        gtk_tree_model_get (tree_model,
                            iter,
                            TIME_COLUMN, &time,
                            -1);

        seconds = time;
	minutes = seconds / 60;
	remain_seconds = seconds - (60 * minutes);
        
	str = g_strdup_printf ("%d:%02d ", minutes, remain_seconds);

        g_object_set (cell,
                      "text", str,
                      NULL);

        g_free (str);
}

static void
set_up_tree_view (NautilusMusicView *music_view)
{
        GtkCellRenderer *cell;
        GtkTreeViewColumn *column;
        GtkTreeView *tree_view;

        tree_view = GTK_TREE_VIEW (music_view->details->tree_view);

        /* The track number column */
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell,
                      "xalign", 1.0,
                      NULL);
        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("Track"));
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, cell,
                                                 track_cell_data_func,
                                                 NULL, NULL);
        gtk_tree_view_column_set_sort_column_id (column, TRACK_NUMBER_COLUMN);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (tree_view, column);

        /* The name column */
        cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Title"),
                                                           cell,
                                                           "text", TITLE_COLUMN,
                                                           NULL);
        gtk_tree_view_column_set_sort_column_id (column, TITLE_COLUMN);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (tree_view, column);

        /* The artist column */
        cell = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Artist"),
                                                           cell,
                                                           "text", ARTIST_COLUMN,
                                                           NULL);
        gtk_tree_view_column_set_sort_column_id (column, ARTIST_COLUMN);
        gtk_tree_view_column_set_resizable (column, TRUE);        
        gtk_tree_view_append_column (tree_view, column);

        /* The bitrate column */
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell,
                      "xalign", 1.0,
                      NULL);
        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("Bit Rate"));
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, cell,
                                                 bitrate_cell_data_func,
                                                 NULL, NULL);
        gtk_tree_view_column_set_sort_column_id (column, BITRATE_COLUMN);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (tree_view, column);

        /* The time column */
        cell = gtk_cell_renderer_text_new ();
        g_object_set (cell,
                      "xalign", 1.0,
                      NULL);
        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_title (column, _("Time"));
        gtk_tree_view_column_pack_start (column, cell, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, cell,
                                                 time_cell_data_func,
                                                 NULL, NULL);
        gtk_tree_view_column_set_sort_column_id (column, TIME_COLUMN);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (tree_view, column);
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_music_view_init (NautilusMusicView *music_view)
{
	GtkWidget *label;
	GtkWidget *button;
	char *font_name;
	int standard_font_size;
	
	music_view->details = g_new0 (NautilusMusicViewDetails, 1);

        music_view->details->event_box = gtk_event_box_new ();
        gtk_widget_show (music_view->details->event_box);
        
        g_signal_connect (music_view->details->event_box,
                             "drag_data_received",
                             G_CALLBACK (nautilus_music_view_drag_data_received),
                             music_view);

	nautilus_view_construct (NAUTILUS_VIEW (music_view), 
				 music_view->details->event_box);
	
    	
	g_signal_connect (music_view, 
			    "load_location",
			    G_CALLBACK (music_view_load_location_callback), 
			    music_view);
			    
	g_signal_connect (eel_get_widget_background (GTK_WIDGET (music_view->details->event_box)), 
			    "appearance_changed",
			    G_CALLBACK (music_view_background_appearance_changed_callback), 
			    music_view);

	/* NOTE: we don't show the widgets until the directory has been loaded,
	   to avoid showing degenerate widgets during the loading process */
	   
	/* allocate a vbox to contain all of the views */	
	music_view->details->album_container = GTK_VBOX (gtk_vbox_new (FALSE, 8));
        gtk_widget_show (GTK_WIDGET (music_view->details->album_container));
	gtk_container_set_border_width (GTK_CONTAINER (music_view->details->album_container), 4);
	gtk_container_add (GTK_CONTAINER (music_view->details->event_box), GTK_WIDGET (music_view->details->album_container));
		
	/* allocate a widget for the album title */	
	music_view->details->album_title = gtk_label_new ("");
        gtk_widget_show (music_view->details->album_title);
	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->album_title, FALSE, FALSE, 0);	

        /* Create list model*/
        music_view->details->list_store = gtk_list_store_new (NUM_COLUMNS,
                                                              G_TYPE_INT, /* TRACK_NUMBER_COLUMN */
                                                              G_TYPE_STRING, /* TITLE_COLUMN */
                                                              G_TYPE_STRING, /* ARTIST_COLUMN */
                                                              G_TYPE_INT, /* BITRATE_COLUMN */
                                                              G_TYPE_INT, /* TIME_COLUMN */
                                                              G_TYPE_INT, /* BITRATE_COLUMN */
                                                              G_TYPE_STRING, /* YEAR_COLUMN */
                                                              G_TYPE_STRING, /* COMMENT_COLUMN */
                                                              G_TYPE_STRING, /* PATH_URI_COLUMN */
                                                              G_TYPE_STRING /* ALBUM_COLUMN */);
        music_view->details->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (music_view->details->list_store));
        g_signal_connect (music_view->details->tree_view,
                          "row_activated",
                          G_CALLBACK (row_activated_callback),
                          music_view);
        
        g_object_unref (music_view->details->list_store);
        set_up_tree_view (music_view);

        g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (music_view->details->tree_view)),
                          "changed",
                          G_CALLBACK (selection_changed),
                          music_view);

        /* We sort ascending by track number by default */
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (music_view->details->list_store),
                                              TRACK_NUMBER_COLUMN, GTK_SORT_ASCENDING);

        gtk_widget_show (music_view->details->tree_view);

	font_name = eel_preferences_get (NAUTILUS_PREFERENCES_LIST_VIEW_FONT);
	standard_font_size = eel_preferences_get_integer (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE);
        
#ifdef GNOME2_CONVERSION_COMPLETE
	font = nautilus_font_factory_get_font_by_family (font_name, standard_font_size);
	eel_gtk_widget_set_font (GTK_WIDGET (music_view->details->song_list), font);

#endif 	
	music_view->details->scroll_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (music_view->details->scroll_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (music_view->details->scroll_window),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (music_view->details->scroll_window),
                                             GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (music_view->details->scroll_window), music_view->details->tree_view);	

	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->scroll_window, TRUE, TRUE, 0);	

	/* make an hbox to hold the optional cover and other controls */
	music_view->details->control_box = gtk_hbox_new (FALSE, 2);
        gtk_widget_show (music_view->details->control_box);
	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->control_box, FALSE, FALSE, 2);	
	
	/* make the "set album button"  and show it */
  	music_view->details->image_box = gtk_vbox_new (0, FALSE);
        gtk_widget_show (music_view->details->image_box);
        button = gtk_button_new ();
        gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (music_view->details->image_box), button, FALSE, FALSE, 2);

	label = gtk_label_new (_("Set Cover Image"));
        gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER(button), label);
	gtk_box_pack_end (GTK_BOX(music_view->details->control_box), music_view->details->image_box, FALSE, FALSE, 4);  
 	g_signal_connect (button, "clicked", G_CALLBACK (image_button_callback), music_view);
 	
	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (music_view->details->event_box),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   music_dnd_target_table, G_N_ELEMENTS (music_dnd_target_table), GDK_ACTION_COPY);


	music_view->details->player_state = PLAYER_STOPPED;
	music_view->details->last_player_state = PLAYER_STOPPED;
}

static void
nautilus_music_view_destroy (BonoboObject *object)
{
	NautilusMusicView *music_view;

        music_view = NAUTILUS_MUSIC_VIEW (object);

	/* we'd rather allow the song to keep playing, but it's hard to maintain state */
	/* so we stop things on exit for now, and improve it post 1.0 */
	stop_playing_file (music_view);
	
	/* Free the status timer callback */
	if (music_view->details->status_timeout != 0) {
		gtk_timeout_remove (music_view->details->status_timeout);
		music_view->details->status_timeout = 0;
	}

        detach_file (music_view);

	EEL_CALL_PARENT (BONOBO_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_music_view_finalize (GObject *object)
{
	NautilusMusicView *music_view;

        music_view = NAUTILUS_MUSIC_VIEW (object);

	g_free (music_view->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static gboolean
string_non_empty (char *str)
{
        return (str != NULL && str[0] != '\0');
}

static void
get_tree_iter_for_row (NautilusMusicView *music_view, int row, GtkTreeIter *iter)
{
        GtkTreePath *path;
        
        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, row);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (music_view->details->list_store), iter, path);
        gtk_tree_path_free (path);
}

/* utility to return the text describing a song */
static char *
get_song_text (NautilusMusicView *music_view, int row)
{
        char *artist_album_string; 
	char *song_text;
        char *song_title;
        GtkTreeIter iter;
        char *info_title, *info_album, *info_year, *info_artist;
        
	song_text = NULL;
	artist_album_string = NULL;

        get_tree_iter_for_row (music_view, row, &iter);
        
        gtk_tree_model_get (GTK_TREE_MODEL (music_view->details->list_store),
                            &iter,
                            ARTIST_COLUMN, &info_artist,
                            TITLE_COLUMN, &info_title,
                            ALBUM_COLUMN, &info_album,
                            YEAR_COLUMN, &info_year,
                            -1);
                            
        if (!string_non_empty (info_title)) {
                song_title = "-";
        } else {
                song_title = info_title;
        }

	if (string_non_empty (info_album)) {
                if (string_non_empty (info_artist)) {
                        artist_album_string = g_strdup_printf ("%s / %s", info_artist, info_album);
                } else {
                        artist_album_string = g_strdup (info_album);
                }
        } else {
                if (string_non_empty (info_artist)) {
                        artist_album_string = g_strdup (info_artist);
                }
        }
                
	if (string_non_empty (artist_album_string)) {
		if (string_non_empty (info_year)) {
			song_text = g_strdup_printf ("%s\n%s (%s)", song_title, artist_album_string, info_year);
		} else {
			song_text = g_strdup_printf ("%s\n%s", song_title, artist_album_string);
                }
	} else {
		if (string_non_empty (info_year)) {
                        song_text = g_strdup_printf ("%s (%s)\n-", song_title, info_year);
                } else {
                        song_text = g_strdup_printf ("%s\n-", song_title);
                }
        }

        g_free (artist_album_string);
	
	return song_text;
}

/* set the song title to the selected one */

static void 
music_view_set_selected_song_title (NautilusMusicView *music_view, int row)
{
	char *label_text;
	char *temp_str;
	
	music_view->details->selected_index = row;
	
	label_text = get_song_text (music_view, row);

        temp_str = g_strdup_printf ("<span size=\"x-large\">%s</span>", label_text);
        
	gtk_label_set_markup (GTK_LABEL(music_view->details->song_label), temp_str);
	g_free (label_text);
        g_free (temp_str);
}

static void
row_activated_callback (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, NautilusMusicView *music_view)
{
        PlayerState state;
	gboolean is_playing_or_paused;

        state = get_player_state (music_view);
	is_playing_or_paused = (state == PLAYER_PLAYING || state == PLAYER_PAUSED);

        play_current_file (music_view, FALSE);
}

/* handle a row being selected in the list view by playing the corresponding song */
static void 
selection_changed (GtkTreeSelection *selection, NautilusMusicView *music_view)
{
	gboolean is_playing_or_paused;
	PlayerState state;
	GtkTreePath *path;
        GtkTreeIter iter;
        int row;

	state = get_player_state (music_view);
	is_playing_or_paused = (state == PLAYER_PLAYING || state == PLAYER_PAUSED);

        gtk_tree_selection_get_selected (selection, NULL, &iter);
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (music_view->details->list_store), &iter);
        row = gtk_tree_path_get_indices (path)[0];
        gtk_tree_path_free (path);
        
	/* Exit if we are playing and clicked on the row that represents the playing song */
	if (is_playing_or_paused && (music_view->details->selected_index == row)) {
		return;
        }

	if (is_playing_or_paused) {
		stop_playing_file (music_view);
        }
        
        music_view_set_selected_song_title (music_view, row);
} 


/* utility routine to check if the passed-in uri is an image file */
static gboolean
ensure_uri_is_image(const char *uri)
{	
	gboolean is_image;
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;

	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info
		(uri, file_info,
		 GNOME_VFS_FILE_INFO_GET_MIME_TYPE
		 | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
        /* FIXME: can the code really handle any type of image other
         * than an SVG? Probably not.  
         */
         is_image = eel_istr_has_prefix (file_info->mime_type, "image/") && (eel_strcmp (file_info->mime_type, "image/svg") != 0);
	gnome_vfs_file_info_unref (file_info);
	return is_image;
}

/* callback to handle setting the album cover image */
static void
set_album_cover (GtkWidget *widget, gpointer *data)
{
	char *path_name, *path_uri;
	NautilusMusicView *music_view;
	
	music_view = NAUTILUS_MUSIC_VIEW (data);
	
	/* get the file path from the file selection widget */
	path_name = g_strdup (gtk_file_selection_get_filename (GTK_FILE_SELECTION (music_view->details->dialog)));
	path_uri = gnome_vfs_get_uri_from_local_path (path_name);

	/* make sure that it's an image */
	if (!ensure_uri_is_image (path_uri)) {
		char *message = g_strdup_printf
			(_("Sorry, but '%s' is not a usable image file."),
			 path_name);
		eel_show_error_dialog (message, _("Not an Image"), NULL);
		g_free (message);
		
		g_free (path_uri);
		g_free (path_name);
		return;
	}
	
	/* set the meta-data */
	nautilus_file_set_metadata (music_view->details->file,
                                    NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, path_uri);
	
	/* set the album image */
	nautilus_music_view_set_album_image (music_view, path_uri);
	g_free (path_uri);
	
	/* tell the world the file changed */
	nautilus_file_changed (music_view->details->file);
	
	/* destroy the file dialog */
	gtk_widget_destroy (music_view->details->dialog);
	music_view->details->dialog = NULL;

	g_free (path_name);
}

/* Callback used when the color selection dialog is destroyed */
static gboolean
dialog_destroy (GtkWidget *widget, gpointer data)
{
	NautilusMusicView *music_view = NAUTILUS_MUSIC_VIEW (data);
	music_view->details->dialog = NULL;
	return FALSE;
}

/* handle the set image button by displaying a file selection dialog */
static void
image_button_callback (GtkWidget * widget, NautilusMusicView *music_view)
{
	if (music_view->details->dialog) {
		gtk_widget_show(music_view->details->dialog);
		if (music_view->details->dialog->window)
			gdk_window_raise(music_view->details->dialog->window);

	} else {
		GtkFileSelection *file_dialog;

		music_view->details->dialog = gtk_file_selection_new
			(_("Select an image file for the album cover:"));
		file_dialog = GTK_FILE_SELECTION (music_view->details->dialog);
		
		g_signal_connect (music_view->details->dialog,
				    "destroy",
				    (GtkSignalFunc) dialog_destroy,
				    music_view);
		g_signal_connect (file_dialog->ok_button,
				    "clicked",
				    (GtkSignalFunc) set_album_cover,
				    music_view);
		gtk_signal_connect_object (GTK_OBJECT (file_dialog->cancel_button),
					   "clicked",
					   (GtkSignalFunc) gtk_widget_destroy,
					   GTK_OBJECT(file_dialog));

		gtk_window_set_position (GTK_WINDOW (file_dialog), GTK_WIN_POS_MOUSE);
		gtk_window_set_wmclass (GTK_WINDOW (file_dialog), "file_selector", "Nautilus");
		gtk_widget_show (GTK_WIDGET(file_dialog));
	}
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
song_info_free (SongInfo *info)
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
is_mp3_file (GnomeVFSFileInfo *file_info)
{
	return eel_istr_has_prefix (file_info->mime_type, "audio/")
		&& eel_istr_has_suffix (file_info->mime_type, "mp3");
}


static char *
filter_out_unset_year (const char *year)
{
        /* All-zero year should be interpreted as unset year. */
        if (strspn (year, "0 ") == strlen (year)) {
                return g_strdup ("");
        } else {
                return g_strdup (year);
        } 
}

/* read the id3 tag of the file if present */
static gboolean
read_id_tag (const char *song_uri, SongInfo *song_info)
{
	char *path;
	id3_t *id3;
	struct id3v1tag_t id3v1tag;
	struct id3tag_t tag;
	FILE *file;
	
	path = gnome_vfs_get_local_path_from_uri (song_uri);
        if (path == NULL) {
                return FALSE;
        }

	file = fopen (path, "rb");
        g_free (path);

	if (file == NULL) {
		return FALSE;	
	}
	
	/* Try ID3v2 tag first */
	fseek (file, 0, SEEK_SET);
	id3 = id3_open_fp (file, O_RDONLY);
	if (id3 != NULL) {
		mpg123_get_id3v2 (id3, &tag);
		id3_close (id3);
	} else if ((fseek (file, -1 * sizeof (id3v1tag), SEEK_END) == 0) &&
            (fread (&id3v1tag, 1, sizeof (id3v1tag), file) == sizeof (id3v1tag)) &&
	    (strncmp (id3v1tag.tag, "TAG", 3) == 0)) {
		/* Try reading ID3v1 tag. */
		mpg123_id3v1_to_id3v2 (&id3v1tag, &tag);
	} else {
		/* Failed to read any sort of tag */
		fclose (file);
		return FALSE;		
	}
	
	/* Copy data from tag into our info struct */
	song_info->title = g_strdup (tag.title);
	song_info->artist = g_strdup (tag.artist);
	song_info->album = g_strdup (tag.album); 
        song_info->year = filter_out_unset_year (tag.year);
	song_info->comment = g_strdup (tag.comment);
	song_info->track_number = atoi (tag.track);

	/* Clean up */
	fclose (file);
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

/* extract a track number from the file name
   return -1 if there wasn't any */
static int
extract_number(const char *name_str)
{
	const char *temp_str;
	gboolean found_digit;
	int accumulator;
	
	found_digit = FALSE;
	accumulator = 0;
	if (g_ascii_isdigit (*name_str)) {
                temp_str = name_str;
	} else if (strchr(name_str, '(') != NULL) {
                temp_str = strchr(name_str, '(') + 1;
        } else {
                return -1;
        }
	
	while (g_ascii_isdigit (*temp_str)) {
                found_digit = TRUE;
                accumulator = (10 * accumulator) + *temp_str - 48;
		temp_str += 1;
	}		
	
	if (found_digit) {
		return accumulator;
        }

	return -1;
}

/* allocate a return a song info record, from an mp3 tag if present, or from intrinsic info */
static SongInfo *
fetch_song_info (const char *song_uri, GnomeVFSFileInfo *file_info, int file_order) 
{
	gboolean has_info = FALSE;
	SongInfo *info; 
	guchar buffer[8192];
	GnomeVFSHandle *mp3_file;
	GnomeVFSResult result;
	GnomeVFSFileSize length_read;
	ID3V2Header v2header;
	long header_size;

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

	result = gnome_vfs_open (&mp3_file, song_uri, GNOME_VFS_OPEN_READ);
	if (result == GNOME_VFS_OK) {
  		result = gnome_vfs_read (mp3_file, buffer, sizeof (buffer), &length_read);
		if ((result == GNOME_VFS_OK) && (length_read > 512)) {
			/* Make sure ID3v2 tag is not at start of file */
			if ( buffer[0] == 'I' && buffer[1] == 'D' && buffer[2] == '3' ) {
				/* Read in header and determine size */
				gnome_vfs_seek (mp3_file, GNOME_VFS_SEEK_START, 0);
				result = gnome_vfs_read (mp3_file, &v2header, sizeof (ID3V2Header), &length_read);
				if (result != GNOME_VFS_OK) {
					return info;
				}

				header_size = ((long) v2header.size[3] | 
					      ((long) v2header.size[2] << (8 - 1)) |
	    				      ((long) v2header.size[1] << (16 - 2)) | 
	    				      ((long) v2header.size[0] << (24 - 3))) 
	    				      + sizeof (ID3V2Header);

				/* Seek past the tag to the mp3 data */
				gnome_vfs_seek (mp3_file, GNOME_VFS_SEEK_START, header_size);
				result = gnome_vfs_read (mp3_file, buffer, sizeof (buffer), &length_read);
				if (result != GNOME_VFS_OK) {
					return info;
				}
			}

			info->bitrate = get_bitrate (buffer, length_read);
			info->samprate = get_samprate (buffer, length_read);
			info->stereo = get_stereo (buffer, length_read);
			info->track_time = fetch_play_time (file_info, info->bitrate);
		}
		gnome_vfs_close (mp3_file);
	}
	
	return	info;
}

/* utility routine to determine most common attribute in song list.  The passed in boolean selects
   album or artist. Return NULL if they are heterogenous */
static char *
determine_attribute (GList *song_list, gboolean is_artist)
{
	SongInfo *info;
	GList *p;
	char *current_attribute, *this_attribute;
	
	current_attribute = NULL;
	
        for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *) p->data;
		this_attribute = is_artist ? info->artist : info->album;
		
		if (this_attribute && eel_strcmp (this_attribute, current_attribute)) {
			if (current_attribute == NULL) {
				current_attribute = g_strdup (this_attribute);
			} else {
				g_free (current_attribute);
				return NULL;
			}
			
		}
	}
	return current_attribute;
}

/* update the status feedback of the play controls */
static void
update_play_controls_status (NautilusMusicView *music_view, PlayerState state)
{
	if (state == PLAYER_PLAYING) {
		gtk_widget_show (music_view->details->active_play_pixwidget);
		gtk_widget_hide (music_view->details->inactive_play_pixwidget);
		//gtk_widget_set_sensitive (music_view->details->pause_button, TRUE);
	} else {
		gtk_widget_hide (music_view->details->active_play_pixwidget);
		gtk_widget_show (music_view->details->inactive_play_pixwidget);		
	}

	if (state == PLAYER_PAUSED) {
		gtk_widget_show (music_view->details->active_pause_pixwidget);
		gtk_widget_hide (music_view->details->inactive_pause_pixwidget);
		//gtk_widget_set_sensitive (music_view->details->pause_button, FALSE);
	} else {
		gtk_widget_hide (music_view->details->active_pause_pixwidget);
		gtk_widget_show (music_view->details->inactive_pause_pixwidget);
	}
}

/* utility to reset the playtime to the inactive state */
static void
reset_playtime (NautilusMusicView *music_view)
{
	gtk_adjustment_set_value (GTK_ADJUSTMENT (music_view->details->playtime_adjustment), 0.0);
 	gtk_range_set_adjustment (GTK_RANGE (music_view->details->playtime_bar),
                                  GTK_ADJUSTMENT (music_view->details->playtime_adjustment));	
	gtk_widget_set_sensitive (music_view->details->playtime_bar, FALSE);	
	gtk_label_set_markup (GTK_LABEL (music_view->details->playtime), "<span size=\"x-large\">--:--</span>");
}

/* status display timer task */
static int 
play_status_display (NautilusMusicView *music_view)
{
	int minutes, seconds;
	float percentage;
	char *play_time_str;
	int current_time;
	gboolean is_playing_or_paused;
	int samps_per_frame;
	PlayerState status;
			
	status = get_player_state (music_view);
	is_playing_or_paused = (status == PLAYER_PLAYING) || (status == PLAYER_PAUSED);
	
	if (status == PLAYER_NEXT) {
		stop_playing_file (music_view);
		go_to_next_track (music_view);
		return FALSE;
	}
		
	if (music_view->details->last_player_state != status) {
		music_view->details->last_player_state = status;
		update_play_controls_status (music_view, status);
	}
	
	if (is_playing_or_paused) {			
		if (!music_view->details->slider_dragging) {									
			samps_per_frame = (music_view->details->current_samprate >= 32000) ? 1152 : 576;
			
  			if (music_view->details->current_duration != 0) {
				current_time = esdout_get_output_time ();
                     		seconds = current_time / 1000;
				minutes = seconds / 60;
				seconds = seconds % 60;

                                play_time_str = g_strdup_printf ("<span size=\"x-large\">%02d:%02d</span>", minutes, seconds);
												
				percentage = (float) ((float)current_time / (float)music_view->details->current_duration) * 100.0;
				
				gtk_adjustment_set_value (GTK_ADJUSTMENT (music_view->details->playtime_adjustment), percentage);
				gtk_range_set_adjustment (GTK_RANGE (music_view->details->playtime_bar),
                                			  GTK_ADJUSTMENT(music_view->details->playtime_adjustment));	

				if (!music_view->details->slider_dragging) {
		 			gtk_label_set_markup (GTK_LABEL(music_view->details->playtime),
                                                            play_time_str);
                        	}
                                g_free (play_time_str);
			}
		}		
	} else  {
		reset_playtime (music_view);
	}
	
	return is_playing_or_paused;
}

static void
list_reveal_row (GtkTreeView *view, int row_index)
{
        GtkTreePath *path;
        GtkTreeModel *model;

        model = gtk_tree_view_get_model (view);
        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, row_index);

        gtk_tree_view_scroll_to_cell (view, path, NULL,
                                      TRUE, 0.5, 0.5);
}

/* track incrementing routines */
static void
play_current_file (NautilusMusicView *music_view, gboolean from_start)
{
	char *song_filename, *title;
        GnomeVFSResult result;
        GnomeVFSFileInfo file_info;
	int length;
        char *path_uri;
        GtkTreeIter iter;
        
	/* Check gnome config sound preference */
	if (!gnome_config_get_bool ("/sound/system/settings/start_esd=true")) {
		eel_show_error_dialog (_("Sorry, but the music view is unable to play back sound right now. "
					      "This is because the Enable sound server startup setting "
					      "in the Sound section of the Control Center is turned off."),
				            _("Unable to Play File"),
					    //GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (&music_view->details->event_box->parent))));
					    NULL);		
		return;	

	}

	if (esdout_playing ()) {
		eel_show_error_dialog (_("Sorry, but the music view is unable to play back sound right now. "
					      "Either another program is using or blocking the sound card, "
					      "or your sound card is not configured properly. Try quitting any "
					      "applications that may be blocking use of the sound card."),
				            _("Unable to Play File"),
					    //GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (&music_view->details->event_box->parent))));
					    NULL);		
		return;	
	}
       	
	/* Scroll the list to display the current new selection */
	list_reveal_row (GTK_TREE_VIEW (music_view->details->tree_view), music_view->details->selected_index);
        get_tree_iter_for_row (music_view, music_view->details->selected_index, &iter);

        gtk_tree_model_get (GTK_TREE_MODEL (music_view->details->list_store),
                            &iter,
                            PATH_URI_COLUMN, &path_uri,
                            -1);

        /* Make the song selected */
        gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (music_view->details->tree_view)),
                                        &iter);
        
	song_filename = gnome_vfs_get_local_path_from_uri (path_uri);

	/* for now, we can only play local files, so apologize to the user and give up */	
	if (song_filename == NULL) {
                eel_show_error_dialog
                        ( _("Sorry, but the music view can't play non-local files yet."),
                          _("Can't Play Remote Files"),
                          NULL);
                return;
	}
	
	/* set up the current duration so we can give progress feedback */        
	get_song_info (song_filename, &title, &length);
	music_view->details->current_duration = length;
	g_free (title);
	
        result = gnome_vfs_get_file_info (path_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result != GNOME_VFS_OK) {
		/* File must be unavailable for some reason. Let's yank it from the list */
                gtk_list_store_remove (music_view->details->list_store, &iter);
		g_free (song_filename);
		music_view->details->selected_index -= 1;
		go_to_next_track (music_view);
		return;
	}

	gtk_widget_set_sensitive (music_view->details->playtime_bar, TRUE);
	
	if (music_view->details->status_timeout != 0) {
		gtk_timeout_remove (music_view->details->status_timeout);
		music_view->details->status_timeout = 0;
        }
        
	music_view->details->status_timeout = gtk_timeout_add (900, (GtkFunction) play_status_display, music_view);

	start_playing_file (music_view, song_filename);

	g_free (song_filename);
}


static void
go_to_next_track (NautilusMusicView *music_view)
{
        int num_rows;
        
	mpg123_stop ();

        num_rows = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (music_view->details->list_store), NULL);
        
	if (music_view->details->selected_index < num_rows) {
		music_view->details->selected_index += 1;		
		play_current_file (music_view, TRUE);
	} else {  
		update_play_controls_status (music_view, get_player_state (music_view));
		reset_playtime (music_view);
	}
}

static void
go_to_previous_track (NautilusMusicView *music_view)
{	
	/* if we're in the first 3 seconds of the song, go to the previous one, otherwise go to the beginning of this track */	
	if ((esdout_get_output_time () < 300) && (music_view->details->selected_index > 0)) {
		music_view->details->selected_index -= 1;
	}
	
	mpg123_stop ();	
	play_current_file (music_view, TRUE);
}


/* callback for the play control semantics */

/* callback for buttons */
static void
play_button_callback (GtkWidget *widget, NautilusMusicView *music_view)
{
	if (get_player_state (music_view) == PLAYER_PLAYING) {
		return;
	}

	if (get_player_state (music_view) == PLAYER_PAUSED) {				
		set_player_state (music_view, PLAYER_PLAYING);
		mpg123_pause (FALSE);
	} else {
		play_current_file (music_view, FALSE);
	}
}

static void
stop_button_callback (GtkWidget *widget, NautilusMusicView *music_view)
{
	stop_playing_file (music_view);
}

static void
pause_button_callback (GtkWidget *widget, NautilusMusicView *music_view)
{
	PlayerState state;
	state = get_player_state (music_view);
	
	if (state == PLAYER_PLAYING) {
		set_player_state (music_view, PLAYER_PAUSED);
		mpg123_pause (TRUE);
	} else if (state == PLAYER_PAUSED) {
		set_player_state (music_view, PLAYER_PLAYING);
		mpg123_pause (FALSE);
	}
}

static void
prev_button_callback (GtkWidget *widget, NautilusMusicView *music_view)
{
	go_to_previous_track (music_view);
}

static void
next_button_callback (GtkWidget *widget, NautilusMusicView *music_view)
{
	go_to_next_track (music_view);
}

/* here are the  callbacks that handle seeking within a song by dragging the progress bar.
   "Mouse down" sets the slider_dragging boolean, "motion_notify" updates the label on the left, while
   "mouse up" actually moves the frame index.  */

/* handle slider button press */
static int
slider_press_callback (GtkWidget *bar, GdkEvent *event, NautilusMusicView *music_view)
{
	music_view->details->slider_dragging = TRUE;
        return FALSE;
}

/* handle mouse motion by updating the time, but not actually seeking until the user lets go */
static int
slider_moved_callback (GtkWidget *bar, GdkEvent *event, NautilusMusicView *music_view)
{	
	GtkAdjustment *adjustment;
	char *temp_str;
	int time, seconds, minutes;
	float multiplier;
		
	if (music_view->details->slider_dragging) {
		adjustment = gtk_range_get_adjustment (GTK_RANGE (bar));
		
		/* don't attempt this if any of the values are zero */	
		if (music_view->details->current_duration == 0) {
			return FALSE;
		}

		multiplier = adjustment->value / 100.0;
		time = (int) (multiplier * (float)music_view->details->current_duration);
		
		seconds = time / 1000; 
		minutes = seconds / 60;
		seconds = seconds % 60;

                temp_str = g_strdup_printf ("<span size=\"x-large\">%02d:%02d</span>", minutes, seconds);

		gtk_label_set_markup (GTK_LABEL(music_view->details->playtime), temp_str);
	}
        return FALSE;
}
	
/* callback for slider button release - seek to desired location */
static int
slider_release_callback (GtkWidget *bar, GdkEvent *event, NautilusMusicView *music_view)
{
	GtkAdjustment *adjustment;
	float multiplier;
	int time;
	
	if (music_view->details->slider_dragging) {
		adjustment = gtk_range_get_adjustment (GTK_RANGE (bar));
		
		if (music_view->details->current_duration == 0) {
			music_view->details->slider_dragging = FALSE;
			return FALSE;		
		}

		/* Seek to time */
		multiplier = adjustment->value / 100.0;
		time = (int) (multiplier * (float)music_view->details->current_duration);

		mpg123_seek (time / 1000);
	}
	music_view->details->slider_dragging = FALSE;
	
        return FALSE;
}

/* create a button with an xpm label */
static GtkWidget *
xpm_label_box (NautilusMusicView *music_view, char * xpm_data[])
{
        GdkPixbuf *pixbuf;
        GdkPixmap *pixmap;
        GdkBitmap *mask;
        GtkWidget *pix_widget;
        GtkWidget *box;
        GtkStyle *style;

        box = gtk_hbox_new (FALSE, 0);
        gtk_container_border_width (GTK_CONTAINER (box), 2);
        style = gtk_widget_get_style (GTK_WIDGET (music_view->details->event_box));

        pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)xpm_data);
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, GDK_PIXBUF_ALPHA_FULL);

        pix_widget = gtk_pixmap_new (pixmap, mask);
        gtk_box_pack_start (GTK_BOX (box), pix_widget, TRUE, FALSE, 3);
        gtk_widget_show (pix_widget);

        return box;
}

/* creates a button with 2 internal pixwidgets, with only one visible at a time */

static GtkWidget *
xpm_dual_label_box (NautilusMusicView *music_view, char * xpm_data[],
                    char *alt_xpm_data[],
                    GtkWidget **main_pixwidget, GtkWidget **alt_pixwidget )
{
        GtkWidget *box;
        GtkStyle *style;
        GdkPixmap *pixmap;
        GdkBitmap *mask;
        GdkPixbuf *pixbuf;


        box = gtk_hbox_new (FALSE, 0);
        gtk_container_border_width (GTK_CONTAINER (box), 2);

        style = gtk_widget_get_style (GTK_WIDGET (music_view->details->event_box));

        /* create the main pixwidget */
        pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)xpm_data);
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, GDK_PIXBUF_ALPHA_FULL);

        *main_pixwidget = gtk_pixmap_new (pixmap, mask);

        gtk_box_pack_start (GTK_BOX (box), *main_pixwidget, TRUE, FALSE, 3);
        gtk_widget_show (*main_pixwidget);

        /* create the alternative pixwidget */
        pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)alt_xpm_data);
        gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, GDK_PIXBUF_ALPHA_FULL);

        *alt_pixwidget = gtk_pixmap_new (pixmap, mask);

        gtk_box_pack_start (GTK_BOX (box), *alt_pixwidget, TRUE, FALSE, 3);
        gtk_widget_hide (*alt_pixwidget);

        return box;
}

/* add the play controls */

static void
add_play_controls (NautilusMusicView *music_view)
{
	GtkWidget *box; 
	GtkWidget *vbox, *hbox;
	GtkWidget *button;
	GtkTooltips *tooltips;
	
	tooltips = gtk_tooltips_new ();

	/* Create main vbox */
	vbox = gtk_vbox_new (0, 0);
	gtk_box_pack_start (GTK_BOX (music_view->details->control_box), vbox, FALSE, FALSE, 6);
	gtk_widget_show (vbox);
	music_view->details->play_control_box = vbox;

	
	/* Pack the items into the box in reverse order so that the controls are always
	 * at the bottom regardless of the size of the album image.
	 */
	 
	 /* hbox to hold slider and song progress time */
	hbox = gtk_hbox_new (0, 0);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 7);
	gtk_widget_show (hbox);

	/* progress bar */
	music_view->details->playtime_adjustment = gtk_adjustment_new (0, 0, 101, 1, 5, 1);
	music_view->details->playtime_bar = gtk_hscale_new (GTK_ADJUSTMENT (music_view->details->playtime_adjustment));
	
	g_signal_connect (music_view->details->playtime_bar, "button_press_event",
                            G_CALLBACK (slider_press_callback), music_view);
	g_signal_connect (music_view->details->playtime_bar, "button_release_event",
                            G_CALLBACK (slider_release_callback), music_view);
 	g_signal_connect (music_view->details->playtime_bar, "motion_notify_event",
                            G_CALLBACK (slider_moved_callback), music_view);
   
   	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), music_view->details->playtime_bar,
                              _("Drag to seek within track"), NULL);
	gtk_scale_set_draw_value (GTK_SCALE (music_view->details->playtime_bar), 0);
	gtk_widget_show (music_view->details->playtime_bar);
	gtk_widget_set_sensitive (music_view->details->playtime_bar, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->playtime_bar, FALSE, FALSE, 4);
	gtk_widget_set_size_request (music_view->details->playtime_bar, 150, -1);
	gtk_widget_show (music_view->details->playtime_bar);

	/* playtime label */
	music_view->details->playtime = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (music_view->details->playtime), "<span size=\"x-large\">--:--</span>");

	gtk_label_set_justify (GTK_LABEL (music_view->details->playtime), GTK_JUSTIFY_LEFT);

	gtk_misc_set_alignment (GTK_MISC (music_view->details->playtime), 0.0, 0.0);
	gtk_widget_show (music_view->details->playtime);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->playtime, FALSE, FALSE, 0);
	 
	/* Buttons */
        hbox = gtk_hbox_new (0, 0);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	/* previous track button */	
	box = xpm_label_box (music_view, prev_xpm);
	gtk_widget_show (box);
	music_view->details->previous_track_button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), music_view->details->previous_track_button, _("Previous"), NULL);
	gtk_container_add (GTK_CONTAINER (music_view->details->previous_track_button), box);
	g_signal_connect (music_view->details->previous_track_button, "clicked", G_CALLBACK (prev_button_callback), music_view);
	gtk_widget_set_sensitive (music_view->details->previous_track_button, TRUE);
	gtk_button_set_relief (GTK_BUTTON (music_view->details->previous_track_button), GTK_RELIEF_NORMAL);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->previous_track_button, FALSE, FALSE, 0);
	gtk_widget_show (music_view->details->previous_track_button);

	/* play button */
	box = xpm_dual_label_box (music_view, play_xpm, play_green_xpm,
                                  &music_view->details->inactive_play_pixwidget,
                                  &music_view->details->active_play_pixwidget);
	gtk_widget_show (box);
	button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), button, _("Play"), NULL);
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER (button), box);
	gtk_widget_set_sensitive (button, TRUE);
	g_signal_connect (button, "clicked", G_CALLBACK (play_button_callback), music_view);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	/* pause button */
	box = xpm_dual_label_box (music_view, pause_xpm, pause_green_xpm,
                                  &music_view->details->inactive_pause_pixwidget,
                                  &music_view->details->active_pause_pixwidget);
	gtk_widget_show (box);
	music_view->details->pause_button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), music_view->details->pause_button, _("Pause"), NULL);
	gtk_button_set_relief (GTK_BUTTON (music_view->details->pause_button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER (music_view->details->pause_button), box);
	gtk_widget_set_sensitive (music_view->details->pause_button, TRUE);

	g_signal_connect (music_view->details->pause_button, "clicked",
                            G_CALLBACK(pause_button_callback), music_view);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->pause_button, FALSE, FALSE, 0);
	gtk_widget_show (music_view->details->pause_button);

	/* stop button */
	box = xpm_label_box (music_view, stop_xpm);
	gtk_widget_show (box);
	music_view->details->stop_button = gtk_button_new ();
	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), music_view->details->stop_button, _("Stop"), NULL);
	gtk_button_set_relief (GTK_BUTTON (music_view->details->stop_button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER (music_view->details->stop_button), box);
	gtk_widget_set_sensitive (music_view->details->stop_button, TRUE);

	g_signal_connect (music_view->details->stop_button, "clicked",
                           G_CALLBACK (stop_button_callback), music_view);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->stop_button, FALSE, FALSE, 0);
	gtk_widget_show (music_view->details->stop_button);

	/* next button */
	box = xpm_label_box (music_view, next_xpm);
	gtk_widget_show (box);
	music_view->details->next_track_button = gtk_button_new();
	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), music_view->details->next_track_button, _("Next"), NULL);
	gtk_button_set_relief (GTK_BUTTON (music_view->details->next_track_button), GTK_RELIEF_NORMAL);
	gtk_container_add (GTK_CONTAINER (music_view->details->next_track_button), box);
	gtk_widget_set_sensitive (music_view->details->next_track_button, TRUE);

	g_signal_connect (music_view->details->next_track_button, "clicked",
                            G_CALLBACK (next_button_callback), music_view);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->next_track_button, FALSE, FALSE, 0);
	gtk_widget_show (music_view->details->next_track_button);

	/* Song title label */
	music_view->details->song_label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (music_view->details->song_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_end (GTK_BOX (vbox), music_view->details->song_label, FALSE, FALSE, 2);	
	gtk_widget_show (music_view->details->song_label);	
}

/* set the album image, or hide it if none */
static void
nautilus_music_view_set_album_image (NautilusMusicView *music_view, const char *image_path_uri)
{
	char* image_path;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;	

	if (image_path_uri != NULL) {
  		image_path = gnome_vfs_get_local_path_from_uri (image_path_uri);  		
  		pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
		
		if (pixbuf != NULL) {
			scaled_pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, SCALED_IMAGE_WIDTH, SCALED_IMAGE_HEIGHT);
			g_object_unref (pixbuf);

       			gdk_pixbuf_render_pixmap_and_mask (scaled_pixbuf, &pixmap, &mask, EEL_STANDARD_ALPHA_THRESHHOLD);
			g_object_unref (scaled_pixbuf);
			
			if (music_view->details->album_image == NULL) {
				music_view->details->album_image = gtk_pixmap_new (pixmap, mask);
				gtk_box_pack_start (GTK_BOX (music_view->details->image_box), 
                                                    music_view->details->album_image, FALSE, FALSE, 2);	
			} else {
				gtk_pixmap_set (GTK_PIXMAP (music_view->details->album_image), pixmap, mask);
			}
		
			gtk_widget_show (music_view->details->album_image);

 			g_free (image_path);
		}
	} else if (music_view->details->album_image != NULL) {
		gtk_widget_hide (music_view->details->album_image);
	}
}

/* handle callback that's invoked when file metadata is available */
static void
metadata_callback (NautilusFile *file, gpointer callback_data)
{
	char *album_image_path;
	NautilusMusicView *music_view;
	
	music_view = NAUTILUS_MUSIC_VIEW (callback_data);
	
	album_image_path = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);	
	if (album_image_path != NULL) {
		nautilus_music_view_set_album_image (music_view, album_image_path);
		g_free (album_image_path);
	}
}


/* here's where we do most of the real work of populating the view with info from the new uri */
static void
nautilus_music_view_update (NautilusMusicView *music_view)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *current_file_info;
	GList *list, *node;
	
        char *uri;
	GList *p;
	GList *song_list, *attributes;
	SongInfo *info;
	char *path_uri, *escaped_name;
	char *image_path_uri;
        char *path, *message;
	GtkTreeIter iter;
        
	int file_index;
	int image_count;

        uri = nautilus_file_get_uri (music_view->details->file);
	
	song_list = NULL;
	image_path_uri = NULL;
	file_index = 1;
	image_count = 0;
	
	/* connect the music view background to directory metadata */
	nautilus_connect_background_to_file_metadata (GTK_WIDGET (music_view->details->event_box), 
                                                      music_view->details->file);
#ifdef GNOME2_CONVERSION_COMPLETE
	nautilus_connect_background_to_file_metadata (GTK_WIDGET (music_view->details->song_list),
                                                      music_view->details->file);
#endif
	/* iterate through the directory, collecting mp3 files and extracting id3 data if present */
	result = gnome_vfs_directory_list_load (&list, uri,
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE
                                                | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result != GNOME_VFS_OK) {
		path = gnome_vfs_get_local_path_from_uri (uri);
		message = g_strdup_printf (_("Sorry, but there was an error reading %s."), path);
		eel_show_error_dialog (message, _("Can't Read Folder"), 
                                       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (music_view->details->event_box))));
		g_free (path);
		g_free (message);
                g_free (uri);
		return;
	}
	
	for (node = list; node != NULL; node = node->next) {
		current_file_info = node->data;

		/* skip invisible files, for now */
		if (current_file_info->name[0] == '.') {
                        continue;
                }
		
 		escaped_name = gnome_vfs_escape_string (current_file_info->name);
		path_uri = nautilus_make_path (uri, escaped_name);
		g_free (escaped_name);
                
		/* fetch info and queue it if it's an mp3 file */
		info = fetch_song_info (path_uri, current_file_info, file_index);
		if (info) {
			info->path_uri = path_uri;
			file_index += 1;
                        song_list = g_list_prepend (song_list, info);
		} else {
		        /* it's not an mp3 file, so see if it's an image */
        		const char *mime_type = gnome_vfs_file_info_get_mime_type (current_file_info);		        	
		        if (eel_istr_has_prefix (mime_type, "image/")) {
		        	/* for now, just keep the first image */
		        	if (image_path_uri == NULL) {
		        		image_path_uri = g_strdup (path_uri);
				}
				image_count += 1;
		        }
                        
		        g_free (path_uri);
		}
		
	}
	gnome_vfs_file_info_list_free (list);	

        song_list = g_list_reverse (song_list);
	
	/* populate the list */
        gtk_list_store_clear (music_view->details->list_store);
	
	for (p = song_list; p != NULL; p = p->next) {
		info = (SongInfo *) p->data;

                gtk_list_store_append (music_view->details->list_store,
                                       &iter);
                gtk_list_store_set (music_view->details->list_store,
                                    &iter,
                                    TRACK_NUMBER_COLUMN, info->track_number,
                                    TITLE_COLUMN, g_strdup (info->title),
                                    ARTIST_COLUMN, g_strdup (info->artist),
                                    BITRATE_COLUMN, info->bitrate,
                                    TIME_COLUMN, info->track_time,
                                    SAMPLE_RATE_COLUMN, info->samprate,
                                    YEAR_COLUMN, g_strdup (info->year),
                                    COMMENT_COLUMN, g_strdup (info->comment),
                                    PATH_URI_COLUMN, g_strdup (info->path_uri),
                                    -1);

	}
	
	/* if there was more than one image in the directory, don't use any */	
	if (image_count > 1) {
		g_free (image_path_uri);
		image_path_uri = NULL;
	}

	/* set up the image (including hiding the widget if there isn't one */
	nautilus_music_view_set_album_image (music_view, image_path_uri);
	g_free (image_path_uri);

	/* Check if one is specified in metadata; if so, it will be set by the callback */	
	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON);
	nautilus_file_call_when_ready (music_view->details->file, attributes, metadata_callback, music_view);
	g_list_free (attributes);
        
	/* determine the album title/artist line */	
	if (music_view->details->album_title) {
		char *album_name, *artist_name, *temp_str;

                album_name = determine_attribute (song_list, FALSE);
		if (album_name == NULL) {
			album_name = g_strdup (gnome_vfs_unescape_string_for_display (g_basename (uri)));
                }
		
		artist_name = determine_attribute (song_list, TRUE);
		if (artist_name != NULL) {
			temp_str = g_strdup_printf (_("<span size=\"xx-large\">%s - %s</span>"), album_name, artist_name);
			g_free (artist_name);
		} else {
			temp_str = g_strdup_printf ("<span size=\"xx-large\">%s</span>", album_name);
                }
		gtk_label_set_markup (GTK_LABEL (music_view->details->album_title), temp_str);
		
		g_free (temp_str);
		g_free (album_name);
	}

	/* allocate the play controls if necessary */	
	if (music_view->details->play_control_box == NULL) {
		add_play_controls (music_view);
	}
	
	music_view_set_selected_song_title (music_view, 0);
	
	/* release the song list */
	eel_g_list_free_deep_custom (song_list, (GFunc) song_info_free, NULL);

        g_free (uri);
}

static void
detach_file (NautilusMusicView *music_view)
{
        if (music_view->details->file != NULL) {
                nautilus_file_cancel_call_when_ready (music_view->details->file,
                                                      metadata_callback, music_view);
                nautilus_file_unref (music_view->details->file);
                music_view->details->file = NULL;
        }
}

void
nautilus_music_view_load_uri (NautilusMusicView *music_view, const char *uri)
{
	stop_playing_file (music_view);
        detach_file (music_view);
        music_view->details->file = nautilus_file_get (uri);
	nautilus_music_view_update (music_view);
	
	update_play_controls_status (music_view, get_player_state (music_view));
}

static void
music_view_background_appearance_changed_callback (EelBackground *background, NautilusMusicView *music_view)
{
        GdkColor color;
        
        color = eel_gdk_rgb_to_color (eel_background_is_dark (background) ? EEL_RGB_COLOR_WHITE : EEL_RGB_COLOR_BLACK);
        
	if (music_view->details->album_title != NULL) {
		gtk_widget_modify_fg (music_view->details->album_title,
                                      GTK_STATE_NORMAL, &color);
	}
	if (music_view->details->song_label != NULL) {
		gtk_widget_modify_fg (music_view->details->song_label,
                                      GTK_STATE_NORMAL, &color);
	}
	if (music_view->details->playtime != NULL) {
		gtk_widget_modify_fg (music_view->details->playtime,
                                      GTK_STATE_NORMAL, &color);
	}
}

static void
music_view_load_location_callback (NautilusView *view, 
                                   const char *location,
                                   NautilusMusicView *music_view)
{
        nautilus_view_report_load_underway (NAUTILUS_VIEW (music_view));
	nautilus_music_view_load_uri (music_view, location);
        nautilus_view_report_load_complete (NAUTILUS_VIEW (music_view));
}

/* handle receiving dropped objects */
static void  
nautilus_music_view_drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                        int x, int y,
                                        GtkSelectionData *selection_data, 
                                        guint info, guint time,
                                        gpointer user_data)
{
	char **uris;

	g_return_if_fail (NAUTILUS_IS_MUSIC_VIEW (user_data));

	uris = g_strsplit (selection_data->data, "\r\n", 0);

	switch (info) {
        case TARGET_GNOME_URI_LIST:
        case TARGET_URI_LIST: 	
                /* FIXME bugzilla.gnome.org 42406: 
                 * the music view should accept mp3 files.
                 */
                break;
  		
        case TARGET_COLOR:
                /* Let the background change based on the dropped color. */
                eel_background_receive_dropped_color
                        (eel_get_widget_background (widget),
                         widget, x, y, selection_data);
                break;
  
  	case TARGET_BGIMAGE:
		eel_background_receive_dropped_background_image
			(eel_get_widget_background (widget),
                         uris[0]);
  		break;              

        default:
                g_warning ("unknown drop type");
                break;
        }
	
	g_strfreev (uris);
}

static void
start_playing_file (NautilusMusicView *music_view, const char *file_name)
{
	set_player_state (music_view, PLAYER_PLAYING);
	mpg123_play_file (file_name);
}

static void
stop_playing_file (NautilusMusicView *music_view)
{
	PlayerState state;

	state = get_player_state (music_view);
	
	if (state == PLAYER_PLAYING || state == PLAYER_PAUSED) {
		set_player_state (music_view, PLAYER_STOPPED);
		mpg123_stop ();
	}
}

static PlayerState
get_player_state (NautilusMusicView *music_view)
{
	if (music_view->details->player_state == PLAYER_PLAYING && !esdout_playing ()) {
		music_view->details->player_state = PLAYER_NEXT;
	}

	return music_view->details->player_state;
}

static void
set_player_state (NautilusMusicView *music_view, PlayerState state)
{
	music_view->details->player_state = state;
}

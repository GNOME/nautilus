/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000, 2001 Eazel, Inc.
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
 *  Author: Andy Hertzfeld <andy@eazel.com>
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
#include <libnautilus-private/nautilus-directory-background.h>
#include <libnautilus-private/nautilus-directory-notify.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-font-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-image.h>
#include <eel/eel-label.h>
#include <eel/eel-list.h>
#include <eel/eel-preferences.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-font-factory.h>

#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-sound.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-string.h>
#include <libnautilus/libnautilus.h>

#include <gnome.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>
#include <esd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>

#define SCALED_IMAGE_WIDTH	108
#define SCALED_IMAGE_HEIGHT 	108

typedef enum {
	PLAYER_STOPPED,
	PLAYER_PAUSED,
	PLAYER_PLAYING,
	PLAYER_NEXT
} PlayerState;


typedef enum {
        TRACK_NUMBER = 0,
        TITLE,
        ARTIST,
        BITRATE,
        TIME
} Column;


struct NautilusMusicViewDetails {
        NautilusFile *file;
	GtkWidget *event_box;
        
	int sort_column;
	int sort_reversed;
	int selected_index;
	int status_timeout;
	
	int current_samprate;
	int current_duration;
		
	gboolean slider_dragging;
	
	GtkVBox   *album_container;
	GtkWidget *scroll_window;
	GtkWidget *album_title;
	GtkWidget *song_list;
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
static void nautilus_music_view_initialize_class              (NautilusMusicViewClass *klass);
static void nautilus_music_view_initialize                    (NautilusMusicView      *view);
static void nautilus_music_view_destroy                       (GtkObject              *object);
static void nautilus_music_view_update                        (NautilusMusicView      *music_view);
static void music_view_background_appearance_changed_callback (EelBackground     *background,
                                                               NautilusMusicView      *music_view);
static void music_view_load_location_callback                 (NautilusView           *view,
                                                               const char             *location,
                                                               NautilusMusicView      *music_view);
static void selection_callback                                (EelCList               *clist,
                                                               int                     row,
                                                               int                     column,
                                                               GdkEventButton         *event,
                                                               NautilusMusicView      *music_view);
static void value_changed_callback                            (GtkAdjustment          *adjustment,
							       EelCList      	      *clist);
static void nautilus_music_view_set_album_image               (NautilusMusicView      *music_view,
                                                               const char             *image_path_uri);
static void click_column_callback                             (EelCList               *clist,
                                                               int                     column,
                                                               NautilusMusicView      *music_view);
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
static void sort_list 					      (NautilusMusicView      *music_view);
static void list_reveal_row                                   (EelCList               *clist, 
                                                               int                     row_index);

static void nautilus_music_view_load_uri (NautilusMusicView *view,
                                          const char        *uri);



EEL_DEFINE_CLASS_BOILERPLATE (NautilusMusicView,
                                   nautilus_music_view,
                                   NAUTILUS_TYPE_VIEW)

static void
nautilus_music_view_initialize_class (NautilusMusicViewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_music_view_destroy;
}


static char *
get_cell_text (GtkWidget *widget, int column_index, int cell_width,
               EelCListRow *row, GdkFont *font, gpointer data)
{
	const char *cell_text;
	EelCList *clist;
	
	clist = EEL_CLIST (widget);

	switch ((EelCellType)row->cell[column_index].type) {
	case EEL_CELL_PIXTEXT:
		cell_text = EEL_CELL_PIXTEXT (row->cell[column_index])->text;
		break;
	case EEL_CELL_TEXT:
	case EEL_CELL_LINK_TEXT:
		cell_text = EEL_CELL_TEXT (row->cell[column_index])->text;
		break;
	default:
		g_assert_not_reached ();
		cell_text = NULL;
		break;
	}
		
	return eel_string_ellipsize (cell_text, font, cell_width, EEL_ELLIPSIZE_END);
}

/* initialize ourselves by connecting to the location change signal and allocating our subviews */

static void
nautilus_music_view_initialize (NautilusMusicView *music_view)
{
	GtkWidget *label;
	GtkWidget *button;
	char *font_name;
	int standard_font_size;
	GdkFont *font;
	guint i;
        gpointer foo;
	char *titles[] = { N_("Track"), N_("Title"), N_("Artist"), N_("Bit Rate"), N_("Time")};

        foo = &selection_callback;
        foo = &value_changed_callback;
        foo = &click_column_callback;
        foo = &get_cell_text;

	
	music_view->details = g_new0 (NautilusMusicViewDetails, 1);

        music_view->details->event_box = gtk_event_box_new ();
        gtk_widget_show (music_view->details->event_box);
        
        gtk_signal_connect (GTK_OBJECT (music_view->details->event_box),
                             "drag_data_received",
                             nautilus_music_view_drag_data_received,
                             music_view);

	nautilus_view_construct (NAUTILUS_VIEW (music_view), 
				 music_view->details->event_box);
	
    	
	gtk_signal_connect (GTK_OBJECT (music_view), 
			    "load_location",
			    music_view_load_location_callback, 
			    music_view);
			    
	gtk_signal_connect (GTK_OBJECT (eel_get_widget_background (GTK_WIDGET (music_view->details->event_box))), 
			    "appearance_changed",
			    music_view_background_appearance_changed_callback, 
			    music_view);

	/* NOTE: we don't show the widgets until the directory has been loaded,
	   to avoid showing degenerate widgets during the loading process */
	   
	/* allocate a vbox to contain all of the views */	
	music_view->details->album_container = GTK_VBOX (gtk_vbox_new (FALSE, 8));
        gtk_widget_show (GTK_WIDGET (music_view->details->album_container));
	gtk_container_set_border_width (GTK_CONTAINER (music_view->details->album_container), 4);
	gtk_container_add (GTK_CONTAINER (music_view->details->event_box), GTK_WIDGET (music_view->details->album_container));
		
	/* allocate a widget for the album title */	
	music_view->details->album_title = eel_label_new ("");
        gtk_widget_show (music_view->details->album_title);
	eel_label_make_larger (EEL_LABEL (music_view->details->album_title), 8);

	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->album_title, FALSE, FALSE, 0);	
	
        /* Localize the titles */
        for (i = 0; i < EEL_N_ELEMENTS (titles); i++) {
		titles[i] = _(titles[i]);
	}

	/* allocate a list widget to hold the song list */
	music_view->details->song_list = eel_list_new_with_titles (EEL_N_ELEMENTS (titles), (const char * const *) titles);
        
	EEL_CLIST_SET_FLAG (EEL_CLIST (music_view->details->song_list), CLIST_SHOW_TITLES);

        gtk_widget_show (music_view->details->song_list);

	gtk_signal_connect (GTK_OBJECT (music_view->details->song_list),
			    "get_cell_text",
			    GTK_SIGNAL_FUNC (get_cell_text),
			    NULL);	

	font_name = eel_preferences_get (NAUTILUS_PREFERENCES_LIST_VIEW_FONT);
	standard_font_size = eel_preferences_get_integer (NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE);
	font = nautilus_font_factory_get_font_by_family (font_name, standard_font_size);
	eel_gtk_widget_set_font (GTK_WIDGET (music_view->details->song_list), font);
        eel_list_set_anti_aliased_mode (EEL_LIST (music_view->details->song_list), FALSE);
	gdk_font_unref (font);

	eel_clist_set_column_width (EEL_CLIST (music_view->details->song_list), TRACK_NUMBER, 36);		/* track number */
	eel_clist_set_column_width (EEL_CLIST (music_view->details->song_list), TITLE, 204);	/* song name */
	eel_clist_set_column_width (EEL_CLIST (music_view->details->song_list), ARTIST, 96);		/* artist */
eel_clist_set_column_width (EEL_CLIST (music_view->details->song_list), BITRATE, 42);		/* bitrate */	
	eel_clist_set_column_width (EEL_CLIST (music_view->details->song_list), TIME, 42);		/* time */
 
 	eel_clist_set_column_justification(EEL_CLIST(music_view->details->song_list), TRACK_NUMBER, GTK_JUSTIFY_RIGHT);
 	eel_clist_set_column_justification(EEL_CLIST(music_view->details->song_list), BITRATE, GTK_JUSTIFY_RIGHT);
	eel_clist_set_column_justification(EEL_CLIST(music_view->details->song_list), TIME, GTK_JUSTIFY_RIGHT);
 	
 	gtk_signal_connect (GTK_OBJECT (music_view->details->song_list),
                            "select-row", selection_callback, music_view);

	music_view->details->scroll_window = gtk_scrolled_window_new (NULL, eel_clist_get_vadjustment (EEL_CLIST (music_view->details->song_list)));
        gtk_widget_show (music_view->details->scroll_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (music_view->details->scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (music_view->details->scroll_window), music_view->details->song_list);	
	eel_clist_set_selection_mode (EEL_CLIST (music_view->details->song_list), GTK_SELECTION_BROWSE);

	gtk_box_pack_start (GTK_BOX (music_view->details->album_container), music_view->details->scroll_window, TRUE, TRUE, 0);	

	/* We have to know when we the adjustment is changed to cause a redraw due to a lame CList bug */
	gtk_signal_connect (GTK_OBJECT (eel_clist_get_vadjustment (EEL_CLIST (music_view->details->song_list))),
			    "value-changed", value_changed_callback, music_view->details->song_list);
	
	/* connect a signal to let us know when the column titles are clicked */
	gtk_signal_connect (GTK_OBJECT (music_view->details->song_list), "click_column",
                            click_column_callback, music_view);

        gtk_widget_show (music_view->details->song_list);

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
 	gtk_signal_connect (GTK_OBJECT (button), "clicked", image_button_callback, music_view);
 	
	/* prepare ourselves to receive dropped objects */
	gtk_drag_dest_set (GTK_WIDGET (music_view->details->event_box),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   music_dnd_target_table, EEL_N_ELEMENTS (music_dnd_target_table), GDK_ACTION_COPY);


	music_view->details->player_state = PLAYER_STOPPED;
	music_view->details->last_player_state = PLAYER_STOPPED;
	
	music_view->details->sort_column = TRACK_NUMBER;
}

static void
nautilus_music_view_destroy (GtkObject *object)
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
	g_free (music_view->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static gboolean
string_non_empty (char *str)
{
        return (str != NULL && str[0] != '\0');
}

/* utility to return the text describing a song */
static char *
get_song_text (NautilusMusicView *music_view, int row)
{
        SongInfo *info;
        char *artist_album_string; 
	char *song_text;
        char *song_title;
		
	song_text = NULL;
	artist_album_string = NULL;
        
        info = eel_clist_get_row_data (EEL_CLIST (music_view->details->song_list),
                                       row);

        if (!string_non_empty (info->title)) {
                song_title = "-";
        } else {
                song_title = info->title;
        }

	if (string_non_empty (info->album)) {
                if (string_non_empty (info->artist)) {
                        artist_album_string = g_strdup_printf ("%s / %s", info->artist, info->album);
                } else {
                        artist_album_string = g_strdup (info->album);
                }
        } else {
                if (string_non_empty (info->artist)) {
                        artist_album_string = g_strdup (info->artist);
                }
        }
                
	if (string_non_empty (artist_album_string)) {
		if (string_non_empty (info->year)) {
			song_text = g_strdup_printf ("%s\n%s (%s)", song_title, artist_album_string, info->year);
		} else {
			song_text = g_strdup_printf ("%s\n%s", song_title, artist_album_string);
                }
	} else {
		if (string_non_empty (info->year)) {
                        song_text = g_strdup_printf ("%s (%s)\n-", song_title, info->year);
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
	
	label_text = NULL;
	temp_str = NULL;

	music_view->details->selected_index = row;
	
	label_text = get_song_text (music_view, row);
	eel_label_set_text (EEL_LABEL(music_view->details->song_label), label_text);
	g_free (label_text);
        
	eel_clist_get_text (EEL_CLIST(music_view->details->song_list), row, 5, &temp_str);
}


/* handle a row being selected in the list view by playing the corresponding song */
static void 
selection_callback (EelCList *clist, int row, int column, GdkEventButton *event, NautilusMusicView *music_view)
{
	gboolean is_playing_or_paused;
	SongInfo *song_info;
	PlayerState state;
	
	state = get_player_state (music_view);
	is_playing_or_paused = (state == PLAYER_PLAYING || state == PLAYER_PAUSED);
	 
	/* Exit if we are playing and clicked on the row that represents the playing song */
	if (is_playing_or_paused && (music_view->details->selected_index == row)) {
		return;
        }

	if (is_playing_or_paused) {
		stop_playing_file (music_view);
        }
        
        song_info = eel_clist_get_row_data (clist, row);
	if (song_info == NULL) {
		return;
        }

        music_view_set_selected_song_title (music_view, row);

        /* Play if playback was already happening or there was a double click */
	if ((is_playing_or_paused) || (event != NULL && event->type == GDK_2BUTTON_PRESS)) {
		play_current_file (music_view, FALSE);
        }
        
        /* Redraw to fix lame bug EelCList has with setting the wrong GC */
        //gtk_widget_queue_draw (GTK_WIDGET (clist));
} 


static void
value_changed_callback (GtkAdjustment *adjustment, EelCList *clist)
{
        /* Redraw to fix lame bug EelCList has with setting the wrong GC */
 	//gtk_widget_queue_draw (GTK_WIDGET (clist));
}


static gint
compare_song_numbers (EelCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	EelCListRow *row1, *row2;
	SongInfo *info1, *info2;
        int result;

	row1 = (EelCListRow *) ptr1;
	row2 = (EelCListRow *) ptr2;
	
	info1 = row1->data;
	info2 = row2->data;
	
	if (info1 == NULL || info2 == NULL) {
		return 0;
	}
		
	result = info1->track_number - info2->track_number;

        return result;
}

static int
compare_song_titles (EelCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	SongInfo *info1, *info2;
	EelCListRow *row1, *row2;
	int result;

	row1 = (EelCListRow *) ptr1;
	row2 = (EelCListRow *) ptr2;
	
	info1 = row1->data;
	info2 = row2->data;

	if (info1 == NULL || info2 == NULL) {
		return 0;
	}

	result = eel_strcoll (info1->title, info2->title);

        return result;
}

static int
compare_song_artists (EelCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	SongInfo *info1, *info2;
	EelCListRow *row1, *row2;
        int result;

	row1 = (EelCListRow *) ptr1;
	row2 = (EelCListRow *) ptr2;
	
	info1 = row1->data;
	info2 = row2->data;

	if (info1 == NULL || info2 == NULL) {
		return 0;
	}

	result = eel_strcoll (info1->artist, info2->artist);

        return result;
}

static int
compare_song_times (EelCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	SongInfo *info1, *info2;
	EelCListRow *row1, *row2;
        int result;
	
	row1 = (EelCListRow *) ptr1;
	row2 = (EelCListRow *) ptr2;
	
	info1 = row1->data;
	info2 = row2->data;

	if (info1 == NULL || info2 == NULL) {
		return 0;
	}

	result = info1->track_time - info2->track_time;

        return result;
}

static int
compare_song_bitrates (EelCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	SongInfo *info1, *info2;
	EelCListRow *row1, *row2;
	int result;

	row1 = (EelCListRow *) ptr1;
	row2 = (EelCListRow *) ptr2;
	
	info1 = row1->data;
	info2 = row2->data;

	if (info1 == NULL || info2 == NULL) {
		return 0;
	}

	result = info1->bitrate - info2->bitrate;

        return result;
}

static void
sort_list (NautilusMusicView *music_view)
{
	GList *row;
	EelCList *clist;
	
	clist = EEL_CLIST (music_view->details->song_list);

        eel_list_set_sort_type (EEL_LIST (clist), music_view->details->sort_reversed
                                ? GTK_SORT_DESCENDING
                                : GTK_SORT_ASCENDING);
	eel_list_set_sort_column (EEL_LIST (clist), music_view->details->sort_column);

	/* sort by the specified criteria */	
	switch (music_view->details->sort_column) {
        case TRACK_NUMBER:
		eel_clist_set_compare_func (clist, compare_song_numbers);
                break;
        case TITLE:
		eel_clist_set_compare_func (clist, compare_song_titles);
                break;
        case ARTIST:
		eel_clist_set_compare_func (clist, compare_song_artists);
                break;
        case BITRATE:
        	eel_clist_set_compare_func (clist, compare_song_bitrates);
                break;
        case TIME:
        	eel_clist_set_compare_func (clist, compare_song_times);
                break;
        default:
                g_warning ("unknown sort mode");
                break;
	}
		
	eel_clist_sort (clist);
	
	/* Determine current selection index */
        row = clist->selection;
        if (row != NULL) {
		music_view->details->selected_index = GPOINTER_TO_INT (row->data);
	} 

}

/* handle clicks in the songlist columns */
static void
click_column_callback (EelCList *clist, int column, NautilusMusicView *music_view)
{					

	if (music_view->details->sort_column == column) {
		music_view->details->sort_reversed = !music_view->details->sort_reversed;
        } else {
                music_view->details->sort_reversed = FALSE;
        }
        
        music_view->details->sort_column = column;
        
        sort_list (music_view);

	list_reveal_row (EEL_CLIST(music_view->details->song_list), music_view->details->selected_index);
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
		
		gtk_signal_connect (GTK_OBJECT (music_view->details->dialog),
				    "destroy",
				    (GtkSignalFunc) dialog_destroy,
				    music_view);
		gtk_signal_connect (GTK_OBJECT (file_dialog->ok_button),
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
	const char *temp_str;
	gboolean found_digit;
	int accumulator;
	
	found_digit = FALSE;
	accumulator = 0;
	if (isdigit (*name_str)) {
                temp_str = name_str;
	} else if (strchr(name_str, '(') != NULL) {
                temp_str = strchr(name_str, '(') + 1;
        } else {
                return -1;
        }
	
	while (isdigit (*temp_str)) {
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
	eel_label_set_text  (EEL_LABEL (music_view->details->playtime), "--:--");
}

/* status display timer task */
static int 
play_status_display (NautilusMusicView *music_view)
{
	int minutes, seconds;
	float percentage;
	char play_time_str[256];
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
				sprintf(play_time_str, "%02d:%02d", minutes, seconds);
												
				percentage = (float) ((float)current_time / (float)music_view->details->current_duration) * 100.0;
				
				gtk_adjustment_set_value (GTK_ADJUSTMENT (music_view->details->playtime_adjustment), percentage);
				gtk_range_set_adjustment (GTK_RANGE (music_view->details->playtime_bar),
                                			  GTK_ADJUSTMENT(music_view->details->playtime_adjustment));	

				if (!music_view->details->slider_dragging) {
		 			eel_label_set_text (EEL_LABEL(music_view->details->playtime),
                                                                 play_time_str);	
                        	}                             
			}
		}		
	} else  {
		reset_playtime (music_view);
	}
	
	return is_playing_or_paused;
}


/* The following are copied from gtkclist.c and eel-clist.c */ 
#define CELL_SPACING 1

/* gives the top pixel of the given row in context of
 * the clist's voffset */
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
				    
static void
list_move_vertical (EelCList *clist, gint row, gfloat align)
{
	gfloat value;

	g_return_if_fail (clist != NULL);

	if (!clist->vadjustment) {
		return;
	}

	value = (ROW_TOP_YPIXEL (clist, row) - clist->voffset -
		 align * (clist->clist_window_height - clist->row_height) +
		 (2 * align - 1) * CELL_SPACING);

	if (value + clist->vadjustment->page_size > clist->vadjustment->upper) {
		value = clist->vadjustment->upper - clist->vadjustment->page_size;
	}

	gtk_adjustment_set_value (clist->vadjustment, value);
}


static void
list_moveto (EelCList *clist, gint row, gint column, gfloat row_align, gfloat col_align)
{
	g_return_if_fail (clist != NULL);

	if (row < -1 || row >= clist->rows) {
		return;
	}
	
	if (column < -1 || column >= clist->columns) {
		return;
	}

	row_align = CLAMP (row_align, 0, 1);
	col_align = CLAMP (col_align, 0, 1);

	/* adjust vertical scrollbar */
	if (clist->vadjustment && row >= 0) {
		list_move_vertical (clist, row, row_align);
	}
}


static void
list_reveal_row (EelCList *clist, int row_index)
{
	g_return_if_fail (row_index >= 0 && row_index < clist->rows);
		
	if (ROW_TOP_YPIXEL (clist, row_index) + clist->row_height > clist->clist_window_height) {
		list_moveto (clist, row_index, -1, 1, 0);
     	} else if (ROW_TOP_YPIXEL (clist, row_index) < 0) {
		list_moveto (clist, row_index, -1, 0, 0);
     	}
}


/* track incrementing routines */
static void
play_current_file (NautilusMusicView *music_view, gboolean from_start)
{
	char *song_filename, *title;
	SongInfo *song_info;
        GnomeVFSResult result;
        GnomeVFSFileInfo file_info;
	int length;

	
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
       	
	eel_clist_select_row (EEL_CLIST(music_view->details->song_list), music_view->details->selected_index, 0);

	/* Scroll the list to display the current new selection */
	list_reveal_row (EEL_CLIST(music_view->details->song_list), music_view->details->selected_index);
	
	song_info = eel_clist_get_row_data (EEL_CLIST (music_view->details->song_list),
                                                music_view->details->selected_index);
	if (song_info == NULL) {
		return;
	}
	song_filename = gnome_vfs_get_local_path_from_uri (song_info->path_uri);

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
	
        result = gnome_vfs_get_file_info (song_info->path_uri, &file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result != GNOME_VFS_OK) {
		/* File must be unavailable for some reason. Let's yank it from the list */
		eel_clist_remove (EEL_CLIST (music_view->details->song_list), music_view->details->selected_index);
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
	mpg123_stop ();		
	if (music_view->details->selected_index < (EEL_CLIST (music_view->details->song_list)->rows - 1)) {
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
	char temp_str[256];
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
		sprintf(temp_str, "%02d:%02d", minutes, seconds);

		eel_label_set_text (EEL_LABEL(music_view->details->playtime), temp_str);
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
	
	gtk_signal_connect (GTK_OBJECT (music_view->details->playtime_bar), "button_press_event",
                            GTK_SIGNAL_FUNC (slider_press_callback), music_view);
	gtk_signal_connect (GTK_OBJECT (music_view->details->playtime_bar), "button_release_event",
                            GTK_SIGNAL_FUNC (slider_release_callback), music_view);
 	gtk_signal_connect (GTK_OBJECT (music_view->details->playtime_bar), "motion_notify_event",
                            GTK_SIGNAL_FUNC (slider_moved_callback), music_view);
   
   	gtk_tooltips_set_tip (GTK_TOOLTIPS (tooltips), music_view->details->playtime_bar,
                              _("Drag to seek within track"), NULL);
	gtk_scale_set_draw_value (GTK_SCALE (music_view->details->playtime_bar), 0);
	gtk_widget_show (music_view->details->playtime_bar);
	gtk_widget_set_sensitive (music_view->details->playtime_bar, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->playtime_bar, FALSE, FALSE, 4);
	gtk_widget_set_usize (music_view->details->playtime_bar, 150, -1);
	gtk_widget_show (music_view->details->playtime_bar);

	/* playtime label */
	music_view->details->playtime = eel_label_new ("--:--");
	eel_label_make_larger (EEL_LABEL (music_view->details->playtime), 2);
	eel_label_set_justify (EEL_LABEL (music_view->details->playtime), GTK_JUSTIFY_LEFT);	
	gtk_misc_set_alignment (GTK_MISC (music_view->details->playtime), 0.0, 0.0);
	gtk_widget_show (music_view->details->playtime);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->playtime, FALSE, FALSE, 0);
	gtk_widget_set_usize (music_view->details->playtime, 40, -1);
	 
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
	gtk_signal_connect (GTK_OBJECT (music_view->details->previous_track_button), "clicked", GTK_SIGNAL_FUNC (prev_button_callback), music_view);
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
	gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (play_button_callback), music_view);
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

	gtk_signal_connect (GTK_OBJECT (music_view->details->pause_button), "clicked",
                            GTK_SIGNAL_FUNC(pause_button_callback), music_view);
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

	gtk_signal_connect (GTK_OBJECT (music_view->details->stop_button), "clicked",
                           GTK_SIGNAL_FUNC (stop_button_callback), music_view);
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

	gtk_signal_connect (GTK_OBJECT (music_view->details->next_track_button), "clicked",
                            GTK_SIGNAL_FUNC (next_button_callback), music_view);
	gtk_box_pack_start (GTK_BOX (hbox), music_view->details->next_track_button, FALSE, FALSE, 0);
	gtk_widget_show (music_view->details->next_track_button);

	/* Song title label */
	music_view->details->song_label = eel_label_new ("");
	eel_label_make_larger (EEL_LABEL (music_view->details->song_label), 2);
	eel_label_set_justify (EEL_LABEL (music_view->details->song_label), GTK_JUSTIFY_LEFT);
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
  		pixbuf = gdk_pixbuf_new_from_file (image_path);
		
		if (pixbuf != NULL) {
			scaled_pixbuf = eel_gdk_pixbuf_scale_down_to_fit (pixbuf, SCALED_IMAGE_WIDTH, SCALED_IMAGE_HEIGHT);
			gdk_pixbuf_unref (pixbuf);

       			gdk_pixbuf_render_pixmap_and_mask (scaled_pixbuf, &pixmap, &mask, EEL_STANDARD_ALPHA_THRESHHOLD);
			gdk_pixbuf_unref (scaled_pixbuf);
			
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
	char *clist_entry[10];
	GList *p;
	GList *song_list, *attributes;
	SongInfo *info;
	char *path_uri, *escaped_name;
	char *image_path_uri;
        char *path, *message;
	
	int file_index;
	int track_index;
	int image_count;

        uri = nautilus_file_get_uri (music_view->details->file);
	
	song_list = NULL;
	image_path_uri = NULL;
	file_index = 1;
	track_index = 0;
	image_count = 0;
	
	/* connect the music view background to directory metadata */	
	nautilus_connect_background_to_file_metadata (GTK_WIDGET (music_view->details->event_box), 
                                                      music_view->details->file);
	
	nautilus_connect_background_to_file_metadata (GTK_WIDGET (music_view->details->song_list),
                                                      music_view->details->file);

	/* iterate through the directory, collecting mp3 files and extracting id3 data if present */
	result = gnome_vfs_directory_list_load (&list, uri,
						GNOME_VFS_FILE_INFO_GET_MIME_TYPE
                                                | GNOME_VFS_FILE_INFO_FOLLOW_LINKS,
						NULL);
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
	
	/* populate the clist */	
	eel_clist_clear (EEL_CLIST (music_view->details->song_list));
	
	for (p = song_list; p != NULL; p = p->next) {
		int i;
		info = (SongInfo *) p->data;

		for (i = 0; i < 10; i ++) {
			clist_entry[i] = NULL;
		}
		
		if (info->track_number > 0)
			clist_entry[TRACK_NUMBER] = g_strdup_printf("%d ", info->track_number);
		if (info->title)
			clist_entry[TITLE] = g_strdup(info->title);
		if (info->artist)
			clist_entry[ARTIST] = g_strdup(info->artist);
		if (info->bitrate > 0)
			clist_entry[BITRATE] = g_strdup_printf("%d ", info->bitrate);
		if (info->track_time > 0)
			clist_entry[TIME] = format_play_time (info->track_time);

		eel_clist_append(EEL_CLIST(music_view->details->song_list), clist_entry);		
		eel_clist_set_row_data_full (EEL_CLIST(music_view->details->song_list),
					track_index, info, (GtkDestroyNotify)release_song_info);

		for (i = 0; i < 10; i ++) {
			g_free (clist_entry[i]);
			clist_entry[i] = NULL;
		}
		
		track_index += 1;
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
			temp_str = g_strdup_printf (_("%s - %s"), album_name, artist_name);
			g_free (artist_name);
		} else {
			temp_str = g_strdup (album_name);
                }
		eel_label_set_text (EEL_LABEL (music_view->details->album_title), temp_str);
		
		g_free (temp_str);
		g_free (album_name);
	}

	/* allocate the play controls if necessary */	
	if (music_view->details->play_control_box == NULL) {
		add_play_controls (music_view);
	}
	
	music_view_set_selected_song_title (music_view, 0);
	
	/* Do initial sort */
	sort_list (music_view);
	
	/* release the song list */
	g_list_free (song_list);

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
	guint32 text_color;

	text_color = eel_background_is_dark (background) ? EEL_RGBA_COLOR_OPAQUE_WHITE : EEL_RGBA_COLOR_OPAQUE_BLACK;

	if (music_view->details->album_title != NULL) {
		eel_label_set_text_color (EEL_LABEL (music_view->details->album_title),
                                               text_color);
	}
	if (music_view->details->song_label != NULL) {
		eel_label_set_text_color (EEL_LABEL (music_view->details->song_label),
                                               text_color);
	}
	if (music_view->details->playtime != NULL) {
		eel_label_set_text_color (EEL_LABEL (music_view->details->playtime),
                                               text_color);
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

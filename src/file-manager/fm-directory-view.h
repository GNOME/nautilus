/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#ifndef FM_DIRECTORY_VIEW_H
#define FM_DIRECTORY_VIEW_H

#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwindow.h>
#include <eel/eel-background.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-icon-container.h>
#include <libnautilus-private/nautilus-link.h>
#include <eel/eel-string-list.h>
#include <libnautilus/nautilus-view.h>

typedef struct FMDirectoryView FMDirectoryView;
typedef struct FMDirectoryViewClass FMDirectoryViewClass;


/* Bonobo command paths that are used by subclasses. Others are defined in fm-directory-view.c */
#define FM_DIRECTORY_VIEW_COMMAND_RESET_BACKGROUND		"/commands/Reset Background"

#define FM_TYPE_DIRECTORY_VIEW			(fm_directory_view_get_type ())
#define FM_DIRECTORY_VIEW(obj)			(GTK_CHECK_CAST ((obj), FM_TYPE_DIRECTORY_VIEW, FMDirectoryView))
#define FM_DIRECTORY_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW, FMDirectoryViewClass))
#define FM_IS_DIRECTORY_VIEW(obj)		(GTK_CHECK_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW))
#define FM_IS_DIRECTORY_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), FM_TYPE_DIRECTORY_VIEW))

typedef struct FMDirectoryViewDetails FMDirectoryViewDetails;

struct FMDirectoryView {
	GtkScrolledWindow parent;
	FMDirectoryViewDetails *details;
};

struct FMDirectoryViewClass {
	GtkScrolledWindowClass parent_class;

	/* The 'clear' signal is emitted to empty the view of its contents.
	 * It must be replaced by each subclass.
	 */
	void 	(* clear) 		 (FMDirectoryView *view);
	
	/* The 'begin_file_changes' signal is emitted before a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary preparation for a set of new files. The default
	 * implementation does nothing.
	 */
	void 	(* begin_file_changes) (FMDirectoryView *view);
	
	/* The 'add_file' signal is emitted to add one file to the view.
	 * It must be replaced by each subclass.
	 */
	void    (* add_file) 		 (FMDirectoryView *view, 
					  NautilusFile *file);
	void    (* remove_file)		 (FMDirectoryView *view, 
					  NautilusFile *file);

	/* The 'file_changed' signal is emitted to signal a change in a file,
	 * including the file being removed.
	 * It must be replaced by each subclass.
	 */
	void 	(* file_changed)         (FMDirectoryView *view, 
					  NautilusFile *file);

	/* The 'end_file_changes' signal is emitted after a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary cleanup (typically, cleanup for code in begin_file_changes).
	 * The default implementation does nothing.
	 */
	void 	(* end_file_changes)    (FMDirectoryView *view);
	
	/* The 'begin_loading' signal is emitted before any of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary preparation to start dealing with a
	 * new directory. The default implementation does nothing.
	 */
	void 	(* begin_loading) 	 (FMDirectoryView *view);

	/* The 'end_loading' signal is emitted after all of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary clean-up. The default implementation 
	 * does nothing.
	 */
	void 	(* end_loading) 	 (FMDirectoryView *view);

	/* The 'load_error' signal is emitted when the directory model
	 * reports an error in the process of monitoring the directory's
	 * contents.  The load error indicates that the process of 
	 * loading the contents has ended, but the directory is still
	 * being monitored. The default implementation handles common
	 * load failures like ACCESS_DENIED.
	 */
	void    (* load_error)           (FMDirectoryView *view,
					  GnomeVFSResult result);

	/* Function pointers that don't have corresponding signals */

        /* reset_to_defaults is a function pointer that subclasses must 
         * override to set sort order, zoom level, etc to match default
         * values. 
         */
        void     (* reset_to_defaults)	         (FMDirectoryView *view);

	/* get_selection is not a signal; it is just a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NautilusFile pointers.
	 */
	GList *	(* get_selection) 	 	(FMDirectoryView *view);
	
        /* select_all is a function pointer that subclasses must override to
         * select all of the items in the view */
        void     (* select_all)	         	(FMDirectoryView *view);

        /* set_selection is a function pointer that subclasses must
         * override to select the specified items (and unselect all
         * others). The argument is a list of NautilusFiles. */

        void     (* set_selection)	 	(FMDirectoryView *view, 
        					 GList *selection);

	/* Return an array of locations of selected icons in their view. */
	GArray * (* get_selected_icon_locations) (FMDirectoryView *view);

        /* bump_zoom_level is a function pointer that subclasses must override
         * to change the zoom level of an object. */
        void    (* bump_zoom_level)      	(FMDirectoryView *view,
					  	 int zoom_increment);

        /* zoom_to_level is a function pointer that subclasses must override
         * to set the zoom level of an object to the specified level. */
        void    (* zoom_to_level) 		(FMDirectoryView *view, 
        				         gint 		 level);

        /* restore_default_zoom_level is a function pointer that subclasses must override
         * to restore the zoom level of an object to a default setting. */
        void    (* restore_default_zoom_level) (FMDirectoryView *view);

        /* can_zoom_in is a function pointer that subclasses must override to
         * return whether the view is at maximum size (furthest-in zoom level) */
        gboolean (* can_zoom_in)	 	(FMDirectoryView *view);

        /* can_zoom_out is a function pointer that subclasses must override to
         * return whether the view is at minimum size (furthest-out zoom level) */
        gboolean (* can_zoom_out)	 	(FMDirectoryView *view);
        
        /* reveal_selection is a function pointer that subclasses may
         * override to make sure the selected items are sufficiently
         * apparent to the user (e.g., scrolled into view). By default,
         * this does nothing.
         */
        void     (* reveal_selection)	 	(FMDirectoryView *view);

        /* get_background is a function pointer that subclasses must
         * override to return the EelBackground for this view.
         */
        GtkWidget * (* get_background_widget)	(FMDirectoryView *view);

        /* merge_menus is a function pointer that subclasses can override to
         * add their own menu items to the window's menu bar.
         * If overridden, subclasses must call parent class's function.
         */
        void    (* merge_menus)         	(FMDirectoryView *view);

        /* update_menus is a function pointer that subclasses can override to
         * update the sensitivity or wording of menu items in the menu bar.
         * It is called (at least) whenever the selection changes. If overridden, 
         * subclasses must call parent class's function.
         */
        void    (* update_menus)         	(FMDirectoryView *view);

	/* sort_files is a function pointer that subclasses can override
	 * to provide a sorting order to determine which files should be
	 * presented when only a partial list is provided.
	 */
	void     (* sort_files)                 (FMDirectoryView *view,
						 GList          **files);

	/* get_emblem_names_to_exclude is a function pointer that subclasses
	 * may override to specify a set of emblem names that should not
	 * be displayed with each file. By default, all emblems returned by
	 * NautilusFile are displayed.
	 */
	EelStringList * (* get_emblem_names_to_exclude)	(FMDirectoryView *view);

	/* file_limit_reached is a function pointer that subclasses may
	 * override to control what happens when a directory is loaded
	 * that has more files than Nautilus can handle. The default
	 * implmentation puts up a dialog box that is appropriate to
	 * display when the user explicitly tried to visit a location that
	 * they would think of as a normal directory.
	 */
	void (* file_limit_reached)		(FMDirectoryView *view);

	/* supports_properties is a function pointer that subclasses may
	 * override to control whether the "Show Properties" menu item
	 * should be enabled for selected items. The default implementation 
	 * returns TRUE.
	 */
	gboolean (* supports_properties)	(FMDirectoryView *view);

	/* supports_zooming is a function pointer that subclasses may
	 * override to control whether or not the zooming control and
	 * menu items should be enabled. The default implementation
	 * returns TRUE.
	 */
	gboolean (* supports_zooming)		(FMDirectoryView *view);

	/* is_read_only is a function pointer that subclasses may
	 * override to control whether or not the user is allowed to
	 * change the contents of the currently viewed directory. The
	 * default implementation checks the permissions of the
	 * directory.
	 */
	gboolean (* is_read_only)	        (FMDirectoryView *view);

	/* is_empty is a function pointer that subclasses must
	 * override to report whether the view contains any items.
	 */
	gboolean (* is_empty)                   (FMDirectoryView *view);

	/* supports_creating_files is a function pointer that subclasses may
	 * override to control whether or not new items can be created.
	 * be accepted. The default implementation checks whether the
	 * user has write permissions for the viewed directory, and whether
	 * the viewed directory is in the trash.
	 */
	gboolean (* supports_creating_files)	(FMDirectoryView *view);

	/* accepts_dragged_files is a function pointer that subclasses may
	 * override to control whether or not files can be dropped in this
	 * location. The default implementation returns TRUE.
	 */
	gboolean (* accepts_dragged_files)	(FMDirectoryView *view);

	void	 (* start_renaming_item)        (FMDirectoryView *view,
					  	 const char *uri);

	gboolean (* file_still_belongs)		(FMDirectoryView *view,
						 NautilusFile	 *file);

	/* Preference change callbacks, overriden by icon and list views. 
	 * Icon and list views respond by synchronizing to the new preference
	 * values and forcing an update if appropriate.
	 */
	void	(* text_attribute_names_changed)   (FMDirectoryView *view);
	void	(* embedded_text_policy_changed)   (FMDirectoryView *view);
	void	(* image_display_policy_changed)   (FMDirectoryView *view);
	void	(* click_policy_changed)	   (FMDirectoryView *view);
	void	(* smooth_graphics_mode_changed)   (FMDirectoryView *view);
	void	(* sort_directories_first_changed) (FMDirectoryView *view);

	void	(* emblems_changed)                (FMDirectoryView *view);
};

/* GtkObject support */
GtkType             fm_directory_view_get_type                         (void);

/* Component embedding support */
NautilusView *      fm_directory_view_get_nautilus_view                (FMDirectoryView  *view);

/* Functions callable from the user interface and elsewhere. */
char *              fm_directory_view_get_uri                          (FMDirectoryView  *view);
gboolean            fm_directory_view_can_accept_item                  (NautilusFile     *target_item,
									const char       *item_uri,
									FMDirectoryView  *view);
void                fm_directory_view_display_selection_info           (FMDirectoryView  *view);
GList *             fm_directory_view_get_selection                    (FMDirectoryView  *view);
void                fm_directory_view_stop                             (FMDirectoryView  *view);
gboolean            fm_directory_view_can_zoom_in                      (FMDirectoryView  *view);
gboolean            fm_directory_view_can_zoom_out                     (FMDirectoryView  *view);
GtkWidget *         fm_directory_view_get_background_widget            (FMDirectoryView  *view);
void                fm_directory_view_bump_zoom_level                  (FMDirectoryView  *view,
									int               zoom_increment);
void                fm_directory_view_zoom_to_level                    (FMDirectoryView  *view,
									int               zoom_level);
void                fm_directory_view_set_zoom_level                   (FMDirectoryView  *view,
									int               zoom_level);
void                fm_directory_view_restore_default_zoom_level       (FMDirectoryView  *view);
void                fm_directory_view_reset_to_defaults                (FMDirectoryView  *view);
void                fm_directory_view_select_all                       (FMDirectoryView  *view);
void                fm_directory_view_set_selection                    (FMDirectoryView  *view,
									GList            *selection);
GArray *            fm_directory_get_selected_icon_locations           (FMDirectoryView  *view);
void                fm_directory_view_reveal_selection                 (FMDirectoryView  *view);
gboolean            fm_directory_view_is_empty                         (FMDirectoryView  *view);
gboolean            fm_directory_view_is_read_only                     (FMDirectoryView  *view);
gboolean            fm_directory_view_supports_creating_files          (FMDirectoryView  *view);
gboolean            fm_directory_view_accepts_dragged_files            (FMDirectoryView  *view);
gboolean            fm_directory_view_supports_properties              (FMDirectoryView  *view);
gboolean            fm_directory_view_supports_zooming                 (FMDirectoryView  *view);
void                fm_directory_view_move_copy_items                  (const GList      *item_uris,
									GArray           *relative_item_points,
									const char       *target_uri,
									int               copy_action,
									int               x,
									int               y,
									FMDirectoryView  *view);
gboolean            fm_directory_link_type_in_selection                (FMDirectoryView  *view,
									NautilusLinkType  link_type);

/* Wrappers for signal emitters. These are normally called 
 * only by FMDirectoryView itself. They have corresponding signals
 * that observers might want to connect with.
 */
void                fm_directory_view_clear                            (FMDirectoryView  *view);
void                fm_directory_view_begin_loading                    (FMDirectoryView  *view);
void                fm_directory_view_end_loading                      (FMDirectoryView  *view);

/* Hooks for subclasses to call. These are normally called only by 
 * FMDirectoryView and its subclasses 
 */
void                fm_directory_view_activate_files                   (FMDirectoryView  *view,
									GList            *files);
void                fm_directory_view_start_batching_selection_changes (FMDirectoryView  *view);
void                fm_directory_view_stop_batching_selection_changes  (FMDirectoryView  *view);
gboolean            fm_directory_view_confirm_multiple_windows         (FMDirectoryView  *view,
									int               window_count);
void                fm_directory_view_queue_file_change                (FMDirectoryView  *view,
									NautilusFile     *file);
void                fm_directory_view_notify_selection_changed         (FMDirectoryView  *view);
Bonobo_UIContainer  fm_directory_view_get_bonobo_ui_container          (FMDirectoryView  *view);
BonoboControl *     fm_directory_view_get_bonobo_control               (FMDirectoryView  *view);
EelStringList *     fm_directory_view_get_emblem_names_to_exclude      (FMDirectoryView  *view);
NautilusDirectory  *fm_directory_view_get_model                        (FMDirectoryView  *view);
GtkWindow	   *fm_directory_view_get_containing_window	       (FMDirectoryView  *view);
NautilusFile       *fm_directory_view_get_directory_as_file            (FMDirectoryView  *view);
EelBackground *     fm_directory_view_get_background                   (FMDirectoryView  *view);
void                fm_directory_view_pop_up_background_context_menu   (FMDirectoryView  *view,
									GdkEventButton   *event);
void                fm_directory_view_pop_up_selection_context_menu    (FMDirectoryView  *view,
									GdkEventButton   *event); 
void                fm_directory_view_send_selection_change            (FMDirectoryView *view);
gboolean            fm_directory_view_should_show_file                 (FMDirectoryView  *view,
									NautilusFile     *file);
gboolean	    fm_directory_view_should_sort_directories_first    (FMDirectoryView  *view);
void                fm_directory_view_update_menus                     (FMDirectoryView  *view);
void                fm_directory_view_new_folder                       (FMDirectoryView  *view);
void                fm_directory_view_ignore_hidden_file_preferences   (FMDirectoryView  *view);

#endif /* FM_DIRECTORY_VIEW_H */

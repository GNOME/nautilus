/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifndef FM_DIRECTORY_VIEW_H
#define FM_DIRECTORY_VIEW_H

#include <bonobo/bonobo-ui-handler.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkscrolledwindow.h>
#include <libnautilus/nautilus-view.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-file.h>
#include <libnautilus-extensions/nautilus-icon-container.h>

typedef struct FMDirectoryView FMDirectoryView;
typedef struct FMDirectoryViewClass FMDirectoryViewClass;

/* Paths to use when creating & referring to bonobo menu items.
 * These are the new ones defined by FMDirectoryView. The
 * Nautilus-wide ones are in <libnautilus/nautilus-bonobo-ui.h>
 */
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN                      		"/File/Open"
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN_IN_NEW_WINDOW        		"/File/OpenNew"
#define FM_DIRECTORY_VIEW_MENU_PATH_OPEN_WITH				"/File/Open With"
#define FM_DIRECTORY_VIEW_MENU_PATH_NEW_FOLDER				"/File/New Folder"
#define FM_DIRECTORY_VIEW_MENU_PATH_SEPARATOR_AFTER_CLOSE_ALL_WINDOWS	"/File/SeparatorAfterClose"
#define FM_DIRECTORY_VIEW_MENU_PATH_DELETE                    		"/File/Delete"
#define FM_DIRECTORY_VIEW_MENU_PATH_TRASH                    		"/File/Trash"
#define FM_DIRECTORY_VIEW_MENU_PATH_DUPLICATE                	 	"/File/Duplicate"
#define FM_DIRECTORY_VIEW_MENU_PATH_EMPTY_TRASH             	   	"/File/Empty Trash"
#define FM_DIRECTORY_VIEW_MENU_PATH_SHOW_PROPERTIES         	   	"/File/Show Properties"
#define FM_DIRECTORY_VIEW_MENU_PATH_SEPARATOR_BEFORE_RESET		"/Edit/Before Reset"
#define FM_DIRECTORY_VIEW_MENU_PATH_RESET_BACKGROUND			"/Edit/Reset Background"
#define FM_DIRECTORY_VIEW_MENU_PATH_REMOVE_CUSTOM_ICONS			"/Edit/Remove Custom Icons"
#define FM_DIRECTORY_VIEW_MENU_PATH_OTHER_APPLICATION    		"/File/Open With/OtherApplication"
#define FM_DIRECTORY_VIEW_MENU_PATH_SEPARATOR_BEFORE_VIEWERS		"/File/Open With/SeparatorBeforeViewers"
#define FM_DIRECTORY_VIEW_MENU_PATH_OTHER_VIEWER	   		"/File/Open With/OtherViewer"

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
	
	/* The 'begin_adding_files' signal is emitted before a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary preparation for a set of new files. The default
	 * implementation does nothing.
	 */
	void 	(* begin_adding_files) (FMDirectoryView *view);
	
	/* The 'add_file' signal is emitted to add one file to the view.
	 * It must be replaced by each subclass.
	 */
	void 	(* add_file) 		 (FMDirectoryView *view, 
					  NautilusFile *file);

	/* The 'file_changed' signal is emitted to signal a change in a file,
	 * including the file being removed.
	 * It must be replaced by each subclass.
	 */
	void 	(* file_changed)         (FMDirectoryView *view, 
					  NautilusFile *file);

	/* The 'done_adding_files' signal is emitted after a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary cleanup (typically, cleanup for code in begin_adding_files).
	 * The default implementation does nothing.
	 */
	void 	(* done_adding_files)    (FMDirectoryView *view);
	
	/* The 'begin_loading' signal is emitted before any of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary preparation to start dealing with a
	 * new directory. The default implementation does nothing.
	 */
	void 	(* begin_loading) 	 (FMDirectoryView *view);

	/* The 'create_selection_context_menu_items' signal is emitted 
	 * when creating a context menu for the selected items. @files is
	 * the list of selected files; it isn't destroyed until the menu
	 * is destroyed so it can be used in callbacks.
	 * Subclasses might want to override this function to
	 * modify the menu contents.
	 */
	void 	(* create_selection_context_menu_items) 	 
					 (FMDirectoryView *view,
					  GtkMenu *menu,
					  GList *files);

	/* The 'create_background_context_menu_items' signal is emitted 
	 * when creating a context menu, either an item-specific one or
	 * a background one. Subclasses might want to override this to
	 * modify the menu contents.
	 */
	void 	(* create_background_context_menu_items) 	 
					 (FMDirectoryView *view,
					  GtkMenu *menu);
	 
	/* Function pointers that don't have corresponding signals */

	/* get_selection is not a signal; it is just a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NautilusFile pointers.
	 */
	GList *	(* get_selection) 	 	(FMDirectoryView *view);
	
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
        
        /* select_all is a function pointer that subclasses must override to
         * select all of the items in the view */
        void     (* select_all)	         	(FMDirectoryView *view);

        /* set_selection is a function pointer that subclasses must
         * override to select the specified items (and unselect all
         * others). The argument is a list of NautilusFiles. */

        void     (* set_selection)	 	(FMDirectoryView *view, 
        					 GList *selection);

        /* get_background is a function pointer that subclasses must
         * override to return the NautilusBackground for this view.
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

	/* get_required_metadata_keys is a function pointer that subclasses
	 * may override to request additional metadata to be read before showing
	 * the directory view. If overridden, subclasses must all parent class's
	 * function.
	 */
	void    (* get_required_metadata_keys)	(FMDirectoryView *view,
					  	 GList           **directory_metadata_keys,
					  	 GList           **file_metadata_keys);

	void	(* start_renaming_item)	 	(FMDirectoryView *view,
					  	 const char *uri);

	/* Preference change callbacks, overriden by icon and list views. 
	 * Icon and list views respond by synchronizing to the new preference
	 * values and forcing an update if appropriate.
	 */
	void	(* text_attribute_names_changed)(FMDirectoryView *view);
	void	(* embedded_text_policy_changed)(FMDirectoryView *view);
	void	(* image_display_policy_changed)(FMDirectoryView *view);
	void	(* font_family_changed)		(FMDirectoryView *view);
	void	(* click_policy_changed)	(FMDirectoryView *view);
	void	(* anti_aliased_mode_changed)	(FMDirectoryView *view);
};

/* GtkObject support */
GtkType            fm_directory_view_get_type                       (void);

/* Component embedding support */
NautilusView *     fm_directory_view_get_nautilus_view              (FMDirectoryView       *view);

/* URI handling */
void               fm_directory_view_load_uri                       (FMDirectoryView       *view,
								     const char            *uri);

/* Functions callable from the user interface and elsewhere. */
char *             fm_directory_view_get_uri		            (FMDirectoryView       *view);
gboolean           fm_directory_view_can_accept_item                (NautilusFile          *target_item,
								     const char            *item_uri,
								     FMDirectoryView       *view);
void		   fm_directory_view_display_selection_info	    (FMDirectoryView	   *view);
GList *            fm_directory_view_get_selection                  (FMDirectoryView       *view);
void               fm_directory_view_stop                           (FMDirectoryView       *view);
gboolean           fm_directory_view_can_zoom_in                    (FMDirectoryView       *view);
gboolean           fm_directory_view_can_zoom_out                   (FMDirectoryView       *view);
GtkWidget *	   fm_directory_view_get_background_widget 	    (FMDirectoryView	   *view);
void               fm_directory_view_bump_zoom_level                (FMDirectoryView       *view,
								     int                    zoom_increment);
void               fm_directory_view_zoom_to_level                  (FMDirectoryView        *view,
								     int                    zoom_level);
void               fm_directory_view_set_zoom_level		    (FMDirectoryView        *view,
								     int                    zoom_level);
void               fm_directory_view_restore_default_zoom_level     (FMDirectoryView       *view);
void               fm_directory_view_select_all                     (FMDirectoryView       *view);
void               fm_directory_view_set_selection                  (FMDirectoryView       *view,
								     GList                 *selection);
void               fm_directory_view_move_copy_items                (const GList           *item_uris,
								     const GdkPoint        *relative_item_points,
								     const char            *target_uri,
								     int                    copy_action,
								     int                    x,
								     int                    y,
								     FMDirectoryView       *view);
gint               fm_directory_view_get_context_menu_index         (const char            *menu_name);


/* Wrappers for signal emitters. These are normally called 
 * only by FMDirectoryView itself. They have corresponding signals
 * that observers might want to connect with.
 */
void               fm_directory_view_clear                          (FMDirectoryView       *view);
void               fm_directory_view_begin_loading                  (FMDirectoryView       *view);

/* Hooks for subclasses to call. These are normally called only by 
 * FMDirectoryView and its subclasses 
 */
void               fm_directory_view_activate_files                 (FMDirectoryView       *view,
								     GList          	   *files);
void               fm_directory_view_notify_selection_changed       (FMDirectoryView       *view);
BonoboUIHandler *  fm_directory_view_get_bonobo_ui_handler          (FMDirectoryView       *view);
NautilusDirectory *fm_directory_view_get_model                      (FMDirectoryView       *view);
void               fm_directory_view_pop_up_background_context_menu (FMDirectoryView       *view);
void               fm_directory_view_pop_up_selection_context_menu  (FMDirectoryView       *view); 
void               fm_directory_view_update_menus                   (FMDirectoryView       *view);
void		   fm_directory_view_add_menu_item		    (FMDirectoryView 	   *view, 
								     GtkMenu 		   *menu, 
								     const char 	   *label,
								     void 		  (* activate_handler) (GtkMenuItem *, FMDirectoryView *),
								     gboolean 		   sensitive);

#endif /* FM_DIRECTORY_VIEW_H */

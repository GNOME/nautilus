/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-info.h: Interface for nautilus windows
 
   Copyright (C) 2004 Red Hat Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_WINDOW_INFO_H
#define NAUTILUS_WINDOW_INFO_H

#include <glib-object.h>
#include <libnautilus-private/nautilus-view.h>
#include <gtk/gtkuimanager.h>
#include "../src/nautilus-bookmark-list.h"

G_BEGIN_DECLS

typedef enum
{
	NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT,
	NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_ENABLE,
	NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DISABLE
} NautilusWindowShowHiddenFilesMode;


typedef enum {
	NAUTILUS_WINDOW_OPEN_ACCORDING_TO_MODE,
	NAUTILUS_WINDOW_OPEN_IN_SPATIAL,
	NAUTILUS_WINDOW_OPEN_IN_NAVIGATION
} NautilusWindowOpenMode;

typedef enum {
	/* used in spatial mode */
	NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND = 1<<0,
	/* used in navigation mode */
	NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW = 1<<1
} NautilusWindowOpenFlags;

typedef	enum {
	NAUTILUS_WINDOW_SPATIAL,
	NAUTILUS_WINDOW_NAVIGATION,
	NAUTILUS_WINDOW_DESKTOP
} NautilusWindowType;

#define NAUTILUS_TYPE_WINDOW_INFO           (nautilus_window_info_get_type ())
#define NAUTILUS_WINDOW_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW_INFO, NautilusWindowInfo))
#define NAUTILUS_IS_WINDOW_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW_INFO))
#define NAUTILUS_WINDOW_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_WINDOW_INFO, NautilusWindowInfoIface))

#ifndef NAUTILUS_WINDOW_DEFINED
#define NAUTILUS_WINDOW_DEFINED
/* Using NautilusWindow for the vtable to make implementing this in 
 * NautilusWindow easier */
typedef struct NautilusWindow          NautilusWindow;
#endif

typedef NautilusWindow                  NautilusWindowInfo;

typedef struct _NautilusWindowInfoIface NautilusWindowInfoIface;

struct _NautilusWindowInfoIface 
{
	GTypeInterface g_iface;

	/* signals: */

        void           (* loading_uri)              (NautilusWindowInfo *window,
						     const char        *uri);
	/* Emitted when the view in the window changes the selection */
        void           (* selection_changed)        (NautilusWindowInfo *window);
        void           (* title_changed)            (NautilusWindowInfo *window,
						     const char         *title);
        void           (* hidden_files_mode_changed)(NautilusWindowInfo *window);
  
	/* VTable: */
	/* A view calls this once after a load_location, once it starts loading the
	 * directory. Might be called directly, or later on the mainloop.
	 * This can also be called at any other time if the view needs to
	 * re-load the location. But the view needs to call load_complete first if
	 * its currently loading. */
	void (* report_load_underway) (NautilusWindowInfo *window,
				       NautilusView *view);
	/* A view calls this once after reporting load_underway, when the location
	   has been fully loaded, or when the load was stopped
	   (by an error or by the user). */
	void (* report_load_complete) (NautilusWindowInfo *window,
				       NautilusView *view);
	/* This can be called at any time when there has been a catastrophic failure of
	   the view. It will result in the view being removed. */
	void (* report_view_failed)   (NautilusWindowInfo *window,
				       NautilusView *view);
	void (* report_selection_changed) (NautilusWindowInfo *window);
	
	/* Returns the number of selected items in the view */	
	int  (* get_selection_count)  (NautilusWindowInfo    *window);
	
	/* Returns a list of uris for th selected items in the view, caller frees it */	
	GList *(* get_selection)      (NautilusWindowInfo    *window);

	char * (* get_current_location)  (NautilusWindowInfo *window);
	void   (* set_status)            (NautilusWindowInfo *window,
					  const char *status);
	char * (* get_title)             (NautilusWindowInfo *window);
	GList *(* get_history)           (NautilusWindowInfo *window);
	NautilusBookmarkList *  
	       (* get_bookmark_list)     (NautilusWindowInfo *window);
	NautilusWindowType
	       (* get_window_type)       (NautilusWindowInfo *window);
	NautilusWindowShowHiddenFilesMode
	       (* get_hidden_files_mode) (NautilusWindowInfo *window);
	void   (* set_hidden_files_mode) (NautilusWindowInfo *window,
				       NautilusWindowShowHiddenFilesMode mode);

	void   (* open_location)      (NautilusWindowInfo *window,
				       GFile *location,
				       NautilusWindowOpenMode mode,
				       NautilusWindowOpenFlags flags,
				       GList *selection);
	void   (* show_window)        (NautilusWindowInfo *window);
	void   (* close_window)       (NautilusWindowInfo *window);
	GtkUIManager *     (* get_ui_manager)   (NautilusWindowInfo *window);
};

GType                             nautilus_window_info_get_type                 (void);
void                              nautilus_window_info_report_load_underway     (NautilusWindowInfo                *window,
										 NautilusView                      *view);
void                              nautilus_window_info_report_load_complete     (NautilusWindowInfo                *window,
										 NautilusView                      *view);
void                              nautilus_window_info_report_view_failed       (NautilusWindowInfo                *window,
										 NautilusView                      *view);
void                              nautilus_window_info_report_selection_changed (NautilusWindowInfo                *window);
void                              nautilus_window_info_open_location            (NautilusWindowInfo                *window,
										 GFile                             *location,
										 NautilusWindowOpenMode             mode,
										 NautilusWindowOpenFlags            flags,
										 GList                             *selection);
void                              nautilus_window_info_show_window              (NautilusWindowInfo                *window);
void                              nautilus_window_info_close                    (NautilusWindowInfo                *window);
void                              nautilus_window_info_set_status               (NautilusWindowInfo                *window,
										 const char                        *status);
NautilusWindowType                nautilus_window_info_get_window_type          (NautilusWindowInfo                *window);
char *                            nautilus_window_info_get_title                (NautilusWindowInfo                *window);
GList *                           nautilus_window_info_get_history              (NautilusWindowInfo                *window);
NautilusBookmarkList *		  nautilus_window_info_get_bookmark_list        (NautilusWindowInfo                *window);
char *                            nautilus_window_info_get_current_location     (NautilusWindowInfo                *window);
int                               nautilus_window_info_get_selection_count      (NautilusWindowInfo                *window);
GList *                           nautilus_window_info_get_selection            (NautilusWindowInfo                *window);
NautilusWindowShowHiddenFilesMode nautilus_window_info_get_hidden_files_mode    (NautilusWindowInfo                *window);
void                              nautilus_window_info_set_hidden_files_mode    (NautilusWindowInfo                *window,
										 NautilusWindowShowHiddenFilesMode  mode);
GtkUIManager *                    nautilus_window_info_get_ui_manager           (NautilusWindowInfo                *window);


G_END_DECLS

#endif /* NAUTILUS_WINDOW_INFO_H */

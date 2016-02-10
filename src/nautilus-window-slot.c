/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-slot.c: Nautilus window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#include "config.h"

#include "nautilus-window-slot.h"

#include "nautilus-application.h"
#include "nautilus-canvas-view.h"
#include "nautilus-desktop-window.h"
#include "nautilus-desktop-canvas-view.h"
#include "nautilus-list-view.h"
#include "nautilus-mime-actions.h"
#include "nautilus-places-view.h"
#include "nautilus-special-location-bar.h"
#include "nautilus-trash-bar.h"
#include "nautilus-view.h"
#include "nautilus-window.h"
#include "nautilus-x-content-bar.h"

#include <glib/gi18n.h>
#include <eel/eel-stock-dialogs.h>

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-monitor.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-extension/nautilus-location-widget-provider.h>

G_DEFINE_TYPE (NautilusWindowSlot, nautilus_window_slot, GTK_TYPE_BOX);

enum {
	ACTIVE,
	INACTIVE,
	LAST_SIGNAL
};

enum {
        PROP_ACTIVE = 1,
	PROP_WINDOW,
        PROP_ICON,
        PROP_VIEW_WIDGET,
	PROP_LOADING,
        PROP_LOCATION,
	NUM_PROPERTIES
};

struct NautilusWindowSlotDetails {
	NautilusWindow *window;

        gboolean active : 1;
        guint loading : 1;

	/* slot contains
	 *  1) an vbox containing extra_location_widgets
	 *  2) the view
	 */
	GtkWidget *extra_location_widgets;

        /* Slot actions */
        GActionGroup *slot_action_group;

	/* Current location. */
	GFile *location;
	gchar *title;

	/* Viewed file */
	NautilusView *content_view;
	NautilusView *new_content_view;
	NautilusFile *viewed_file;
	gboolean viewed_file_seen;
	gboolean viewed_file_in_trash;

	/* Information about bookmarks and history list */
	NautilusBookmark *current_location_bookmark;
	NautilusBookmark *last_location_bookmark;
	GList *back_list;
	GList *forward_list;

	/* Query editor */
	NautilusQueryEditor *query_editor;
	gulong qe_changed_id;
	gulong qe_cancel_id;
	gulong qe_activated_id;

        /* Load state */
	GCancellable *find_mount_cancellable;
        /* It could be either the view is loading the files or the search didn't
         * finish. Used for showing a spinner to provide feedback to the user. */
	gboolean allow_stop;
	gboolean needs_reload;
	gboolean load_with_search;

	/* New location. */
	GFile *pending_location;
	NautilusLocationChangeType location_change_type;
	guint location_change_distance;
	char *pending_scroll_to;
	GList *pending_selection;
	NautilusFile *determine_view_file;
	GCancellable *mount_cancellable;
	GError *mount_error;
	gboolean tried_mount;
        gint view_mode_before_search;
        gint view_mode_before_places;
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void nautilus_window_slot_force_reload (NautilusWindowSlot *slot);
static void change_view (NautilusWindowSlot *slot);
static void hide_query_editor (NautilusWindowSlot *slot);
static void nautilus_window_slot_sync_actions (NautilusWindowSlot *slot);
static void nautilus_window_slot_connect_new_content_view (NautilusWindowSlot *slot);
static void nautilus_window_slot_disconnect_content_view (NautilusWindowSlot *slot);
static gboolean nautilus_window_slot_content_view_matches (NautilusWindowSlot *slot, guint id);
static NautilusView* nautilus_window_slot_get_view_for_location (NautilusWindowSlot *slot, GFile *location);
static void nautilus_window_slot_set_content_view (NautilusWindowSlot *slot, guint id);
static void nautilus_window_slot_set_loading (NautilusWindowSlot *slot, gboolean loading);
char * nautilus_window_slot_get_location_uri (NautilusWindowSlot *slot);
static void nautilus_window_slot_set_search_visible (NautilusWindowSlot *slot,
					             gboolean            visible);
static gboolean nautilus_window_slot_get_search_visible (NautilusWindowSlot *slot);
static void nautilus_window_slot_set_location (NautilusWindowSlot *slot,
                                               GFile              *location);

static NautilusView*
nautilus_window_slot_get_view_for_location (NautilusWindowSlot *slot,
                                            GFile              *location)
{
        NautilusWindow *window;
        NautilusView *view;
        NautilusFile *file;

        window = nautilus_window_slot_get_window (slot);
        file = nautilus_file_get (location);
        view = NULL;

        /* FIXME bugzilla.gnome.org 41243:
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
        if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
                view = NAUTILUS_VIEW (nautilus_files_view_new (NAUTILUS_VIEW_DESKTOP_ID, slot));
        } else if (nautilus_file_is_other_locations (file)) {
                view = NAUTILUS_VIEW (nautilus_places_view_new ());

                /* Save the current view, so we can go back after places view */
                if (slot->details->content_view && NAUTILUS_IS_FILES_VIEW (slot->details->content_view)) {
                        slot->details->view_mode_before_places = nautilus_files_view_get_view_id (NAUTILUS_FILES_VIEW (slot->details->content_view));
                }
        } else {
                guint view_id;

                view_id = NAUTILUS_VIEW_INVALID_ID;

                /* If we are in search, try to use by default list view. */
                if (nautilus_file_is_in_search (file)) {
                        /* If it's already set, is because we already made the change to search mode,
                         * so the view mode of the current view will be the one search is using,
                         * which is not the one we are interested in */
                        if (slot->details->view_mode_before_search == NAUTILUS_VIEW_INVALID_ID) {
                                slot->details->view_mode_before_search = nautilus_files_view_get_view_id (NAUTILUS_FILES_VIEW (slot->details->content_view));
                        }
                        view_id = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_SEARCH_VIEW);
                } else if (slot->details->content_view != NULL) {
                        /* If there is already a view, just use the view mode that it's currently using, or
                         * if we were on search before, use what we were using before entering
                         * search mode */
                        if (slot->details->view_mode_before_search != NAUTILUS_VIEW_INVALID_ID) {
                                view_id = slot->details->view_mode_before_search;
                                slot->details->view_mode_before_search = NAUTILUS_VIEW_INVALID_ID;
                        } else if (NAUTILUS_IS_PLACES_VIEW (slot->details->content_view)) {
                                view_id = slot->details->view_mode_before_places;
                                slot->details->view_mode_before_places = NAUTILUS_VIEW_INVALID_ID;
                        } else {
		                view_id = nautilus_files_view_get_view_id (NAUTILUS_FILES_VIEW (slot->details->content_view));
                        }
	        }

                /* If there is not previous view in this slot, use the default view mode
                 * from preferences */
	        if (view_id == NAUTILUS_VIEW_INVALID_ID) {
		        view_id = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER);
	        }

                /* Try to reuse the current view */
                if (nautilus_window_slot_content_view_matches (slot, view_id)) {
                        view = slot->details->content_view;
                } else {
                        view = NAUTILUS_VIEW (nautilus_files_view_new (view_id, slot));
                }
        }

        nautilus_file_unref (file);

        return view;
}

static gboolean
nautilus_window_slot_content_view_matches (NautilusWindowSlot *slot,
                                           guint                id)
{
	if (slot->details->content_view == NULL) {
		return FALSE;
	}

        if (id == NAUTILUS_VIEW_INVALID_ID && NAUTILUS_IS_PLACES_VIEW (slot->details->content_view)) {
                return TRUE;
        } else if (id != NAUTILUS_VIEW_INVALID_ID && NAUTILUS_IS_FILES_VIEW (slot->details->content_view)){
                return nautilus_files_view_get_view_id (NAUTILUS_FILES_VIEW (slot->details->content_view)) == id;
        } else {
                return FALSE;
        }
}

static void
update_search_visible (NautilusWindowSlot *slot)
{
        NautilusQuery *query;
        NautilusView *view;
        GAction *action;

        action =  g_action_map_lookup_action (G_ACTION_MAP (slot->details->slot_action_group),
                                              "search-visible");
        /* Don't allow search on desktop */
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
                                      !NAUTILUS_IS_DESKTOP_CANVAS_VIEW (nautilus_window_slot_get_current_view (slot)));

        view = nautilus_window_slot_get_current_view (slot);
        /* If we changed location just to another search location, for example,
         * when changing the query, just keep the search visible.
         * Make sure the search is visible though, since we could be returning
         * from a previous search location when using the history */
        if (nautilus_view_is_searching (view)) {
                nautilus_window_slot_set_search_visible (slot, TRUE);
                return;
         }

        query = nautilus_query_editor_get_query (slot->details->query_editor);
        if (query) {
                /* If the view is not searching, but search is visible, and the
                 * query is empty, we don't hide it. Some users enable the search
                 * and then change locations, then they search. */
                 if (!nautilus_query_is_empty (query))
                        nautilus_window_slot_set_search_visible (slot, FALSE);
                g_object_unref (query);
        }
}

static void
nautilus_window_slot_sync_actions (NautilusWindowSlot *slot)
{
        GAction *action;
        GVariant *variant;

        if (!nautilus_window_slot_get_active (slot)) {
		return;
	}

	if (slot->details->content_view == NULL || slot->details->new_content_view != NULL) {
		return;
	}

        /* Check if we need to close the search or show search after changing the location.
         * Needs to be done after the change has been done, if not, a loop happens,
         * because setting the search enabled or not actually opens a location */
        update_search_visible (slot);

        /* Files view mode */
        action =  g_action_map_lookup_action (G_ACTION_MAP (slot->details->slot_action_group), "files-view-mode");
        if (NAUTILUS_IS_FILES_VIEW (nautilus_window_slot_get_current_view (slot)) &&
            !NAUTILUS_IS_DESKTOP_CANVAS_VIEW (nautilus_window_slot_get_current_view (slot))) {
                variant = g_variant_new_uint32 (nautilus_files_view_get_view_id (NAUTILUS_FILES_VIEW (nautilus_window_slot_get_current_view (slot))));
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
                g_action_change_state (action, variant);
        } else {
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
        }
}

static void
query_editor_cancel_callback (NautilusQueryEditor *editor,
			      NautilusWindowSlot *slot)
{
	nautilus_window_slot_set_search_visible (slot, FALSE);
}

static void
query_editor_activated_callback (NautilusQueryEditor *editor,
				 NautilusWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
                if (NAUTILUS_IS_FILES_VIEW (slot->details->content_view)) {
                        nautilus_files_view_activate_selection (NAUTILUS_FILES_VIEW (slot->details->content_view));
                }

                nautilus_window_slot_set_search_visible (slot, FALSE);
	}
}

static void
query_editor_changed_callback (NautilusQueryEditor *editor,
			       NautilusQuery *query,
			       gboolean reload,
			       NautilusWindowSlot *slot)
{
        NautilusView *view;

        view = nautilus_window_slot_get_current_view (slot);

        nautilus_view_set_search_query (view, query);
        nautilus_window_slot_open_location_full (slot, nautilus_view_get_location (view), 0, NULL);
}

static void
hide_query_editor (NautilusWindowSlot *slot)
{
        NautilusView *view;

        view = nautilus_window_slot_get_current_view (slot);

	gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (slot->details->query_editor), FALSE);

	if (slot->details->qe_changed_id > 0) {
		g_signal_handler_disconnect (slot->details->query_editor, slot->details->qe_changed_id);
		slot->details->qe_changed_id = 0;
	}
	if (slot->details->qe_cancel_id > 0) {
		g_signal_handler_disconnect (slot->details->query_editor, slot->details->qe_cancel_id);
		slot->details->qe_cancel_id = 0;
	}
	if (slot->details->qe_activated_id > 0) {
		g_signal_handler_disconnect (slot->details->query_editor, slot->details->qe_activated_id);
		slot->details->qe_activated_id = 0;
	}

	nautilus_query_editor_set_query (slot->details->query_editor, NULL);

        if (nautilus_view_is_searching (view)) {
                GList *selection;

                selection = nautilus_view_get_selection (view);

                nautilus_view_set_search_query (view, NULL);
                nautilus_window_slot_open_location_full (slot,
                                                         nautilus_view_get_location (view),
                                                         0,
                                                         selection);

                nautilus_file_list_free (selection);
        }

        if (nautilus_window_slot_get_active (slot)) {
                gtk_widget_grab_focus (GTK_WIDGET (slot->details->window));
        }
}

static GFile *
nautilus_window_slot_get_current_location (NautilusWindowSlot *slot)
{
	if (slot->details->pending_location != NULL) {
		return slot->details->pending_location;
	}

	if (slot->details->location != NULL) {
		return slot->details->location;
	}

	return NULL;
}

static void
show_query_editor (NautilusWindowSlot *slot)
{
        NautilusView *view;

        view = nautilus_window_slot_get_current_view (slot);

        if (nautilus_view_is_searching (view)) {
                NautilusQuery *query;

                query = nautilus_view_get_search_query (view);

                if (query != NULL) {
                        nautilus_query_editor_set_query (slot->details->query_editor, query);
                }
        }

        gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (slot->details->query_editor), TRUE);
	gtk_widget_grab_focus (GTK_WIDGET (slot->details->query_editor));

	if (slot->details->qe_changed_id == 0) {
		slot->details->qe_changed_id =
			g_signal_connect (slot->details->query_editor, "changed",
					  G_CALLBACK (query_editor_changed_callback), slot);
	}
	if (slot->details->qe_cancel_id == 0) {
		slot->details->qe_cancel_id =
			g_signal_connect (slot->details->query_editor, "cancel",
					  G_CALLBACK (query_editor_cancel_callback), slot);
	}
	if (slot->details->qe_activated_id == 0) {
		slot->details->qe_activated_id =
			g_signal_connect (slot->details->query_editor, "activated",
					  G_CALLBACK (query_editor_activated_callback), slot);
	}
}

static void
nautilus_window_slot_set_search_visible (NautilusWindowSlot *slot,
                                        gboolean            visible)
{
        GAction *action;

        action = g_action_map_lookup_action (G_ACTION_MAP (slot->details->slot_action_group),
                                             "search-visible");
        g_action_change_state (action, g_variant_new_boolean (visible));
}

static gboolean
nautilus_window_slot_get_search_visible (NautilusWindowSlot *slot)
{
        GAction *action;
        GVariant *state;
        gboolean searching;

        action = g_action_map_lookup_action (G_ACTION_MAP (slot->details->slot_action_group),
                                             "search-visible");
        state = g_action_get_state (action);
        searching = g_variant_get_boolean (state);

        g_variant_unref (state);

        return searching;
}

gboolean
nautilus_window_slot_handle_event (NautilusWindowSlot *slot,
				   GdkEventKey        *event)
{
	NautilusWindow *window;
	gboolean retval;

	retval = FALSE;
	window = nautilus_window_slot_get_window (slot);
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
                retval = gtk_search_bar_handle_event (GTK_SEARCH_BAR (slot->details->query_editor),
                                                      (GdkEvent*) event);
	}

	if (retval) {
		nautilus_window_slot_set_search_visible (slot, TRUE);
	}

	return retval;
}

static void
real_active (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	int page_num;

	window = slot->details->window;
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (nautilus_window_get_notebook (window)),
					  GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (nautilus_window_get_notebook (window)), page_num);

	/* sync window to new slot */
	nautilus_window_sync_allow_stop (window, slot);
	nautilus_window_sync_title (window, slot);
	nautilus_window_sync_location_widgets (window);
	nautilus_window_slot_sync_actions (slot);

        gtk_widget_insert_action_group (GTK_WIDGET (window), "slot", slot->details->slot_action_group);
}

static void
real_inactive (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = nautilus_window_slot_get_window (slot);
	g_assert (slot == nautilus_window_get_active_slot (window));

        gtk_widget_insert_action_group (GTK_WIDGET (window), "slot", NULL);
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NautilusWindowSlot *slot = data;
	NautilusDirectory *directory;

	directory = nautilus_directory_get (slot->details->location);
	if (widget != GTK_WIDGET (slot->details->query_editor)) {
		gtk_container_remove (GTK_CONTAINER (slot->details->extra_location_widgets), widget);
	}

	nautilus_directory_unref (directory);
}

static void
nautilus_window_slot_remove_extra_location_widgets (NautilusWindowSlot *slot)
{
	gtk_container_foreach (GTK_CONTAINER (slot->details->extra_location_widgets),
			       remove_all_extra_location_widgets,
			       slot);
}

static void
nautilus_window_slot_add_extra_location_widget (NautilusWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->details->extra_location_widgets),
			    widget, FALSE, TRUE, 0);
	gtk_widget_show (slot->details->extra_location_widgets);
}

static void
nautilus_window_slot_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);

	switch (property_id) {
        case PROP_ACTIVE:
                nautilus_window_slot_set_active (slot, g_value_get_boolean (value));
                break;
	case PROP_WINDOW:
		nautilus_window_slot_set_window (slot, g_value_get_object (value));
		break;
	case PROP_LOCATION:
		nautilus_window_slot_set_location (slot, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_window_slot_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);

	switch (property_id) {
        case PROP_ACTIVE:
                g_value_set_boolean (value, nautilus_window_slot_get_active (slot));
                break;
	case PROP_WINDOW:
		g_value_set_object (value, slot->details->window);
		break;
        case PROP_ICON:
                g_value_set_object (value, nautilus_window_slot_get_icon (slot));
                break;
        case PROP_VIEW_WIDGET:
                g_value_set_object (value, nautilus_window_slot_get_view_widget (slot));
                break;
        case PROP_LOADING:
                g_value_set_boolean (value, nautilus_window_slot_get_loading (slot));
                break;
        case PROP_LOCATION:
                g_value_set_object (value, nautilus_window_slot_get_current_location (slot));
                break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_window_slot_constructed (GObject *object)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);
	GtkWidget *extras_vbox;

	G_OBJECT_CLASS (nautilus_window_slot_parent_class)->constructed (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	slot->details->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->details->query_editor = NAUTILUS_QUERY_EDITOR (nautilus_query_editor_new ());
        gtk_widget_show (GTK_WIDGET (slot->details->query_editor));
        nautilus_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->details->query_editor));

        g_object_bind_property (slot, "location",
                                slot->details->query_editor, "location",
                                G_BINDING_DEFAULT);

	slot->details->title = g_strdup (_("Loading…"));
}

static void
action_search_visible (GSimpleAction *action,
                       GVariant      *state,
                       gpointer       user_data)
{
        NautilusWindowSlot *slot;
        GVariant *current_state;

        slot = NAUTILUS_WINDOW_SLOT (user_data);
        current_state = g_action_get_state (G_ACTION (action));
        if (g_variant_get_boolean (current_state) != g_variant_get_boolean (state)) {
                g_simple_action_set_state (action, state);

                if (g_variant_get_boolean (state)) {
                        show_query_editor (slot);
                } else {
                        hide_query_editor (slot);
                }
        }

        g_variant_unref (current_state);
}

static void
action_files_view_mode (GSimpleAction *action,
		        GVariant      *value,
		        gpointer       user_data)
{
        NautilusWindowSlot *slot;
        const gchar *preferences_key;
        guint view_id;

        view_id =  g_variant_get_uint32 (value);
        slot = NAUTILUS_WINDOW_SLOT (user_data);

        if (!NAUTILUS_IS_FILES_VIEW (nautilus_window_slot_get_current_view (slot)))
                return;

        nautilus_window_slot_set_content_view (slot, view_id);
        preferences_key = nautilus_view_is_searching (nautilus_window_slot_get_current_view (slot)) ?
                          NAUTILUS_PREFERENCES_SEARCH_VIEW :
                          NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER;

        g_settings_set_enum (nautilus_preferences, preferences_key, view_id);

        g_simple_action_set_state (action, value);
}

const GActionEntry slot_entries[] = {
        /* 4 is NAUTILUS_VIEW_INVALID_ID */
        { "files-view-mode", NULL, "u", "uint32 4", action_files_view_mode },
        { "search-visible", NULL, NULL, "false", action_search_visible },
};

static void
nautilus_window_slot_init (NautilusWindowSlot *slot)
{
	GApplication *app;

        app = g_application_get_default ();

	slot->details = G_TYPE_INSTANCE_GET_PRIVATE
		(slot, NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlotDetails);

        slot->details->slot_action_group = G_ACTION_GROUP (g_simple_action_group_new ());
        g_action_map_add_action_entries (G_ACTION_MAP (slot->details->slot_action_group),
                                         slot_entries,
                                         G_N_ELEMENTS (slot_entries),
                                         slot);
        gtk_widget_insert_action_group (GTK_WIDGET (slot),
                                        "slot",
                                        G_ACTION_GROUP (slot->details->slot_action_group));
        nautilus_application_add_accelerator (app, "slot.files-view-mode(1)", "<control>1");
        nautilus_application_add_accelerator (app, "slot.files-view-mode(0)", "<control>2");
        nautilus_application_add_accelerator (app, "slot.search-visible", "<control>f");
}

#define DEBUG_FLAG NAUTILUS_DEBUG_WINDOW
#include <libnautilus-private/nautilus-debug.h>

/* FIXME bugzilla.gnome.org 41243:
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

static void begin_location_change                     (NautilusWindowSlot         *slot,
                                                       GFile                      *location,
                                                       GFile                      *previous_location,
                                                       GList                      *new_selection,
                                                       NautilusLocationChangeType  type,
                                                       guint                       distance,
                                                       const char                 *scroll_pos);
static void free_location_change                      (NautilusWindowSlot         *slot);
static void end_location_change                       (NautilusWindowSlot         *slot);
static void got_file_info_for_view_selection_callback (NautilusFile               *file,
						       gpointer                    callback_data);
static gboolean setup_view                            (NautilusWindowSlot *slot,
                                                       NautilusView       *view);
static void load_new_location                         (NautilusWindowSlot         *slot,
						       GFile                      *location,
						       GList                      *selection,
						       gboolean                    tell_current_content_view,
						       gboolean                    tell_new_content_view);

void
nautilus_window_slot_open_location_full (NautilusWindowSlot      *slot,
                                         GFile                   *location,
                                         NautilusWindowOpenFlags  flags,
                                         GList                   *new_selection)
{
	GFile *old_location;
	GList *old_selection;
        NautilusWindow *window;
        gboolean is_desktop;

	old_selection = NULL;
        old_location = nautilus_window_slot_get_location (slot);
        window = nautilus_window_slot_get_window (slot);
        is_desktop = NAUTILUS_IS_DESKTOP_CANVAS_VIEW (window);

        if (slot->details->content_view) {
                old_selection = nautilus_view_get_selection (slot->details->content_view);
	}
        if (!is_desktop &&
            old_location && g_file_equal (old_location, location) &&
            nautilus_file_selection_equal (old_selection, new_selection))
          goto done;

	begin_location_change (slot, location, old_location, new_selection,
			       NAUTILUS_LOCATION_CHANGE_STANDARD, 0, NULL);

 done:
	nautilus_file_list_free (old_selection);
	nautilus_profile_end (NULL);
}

static GList*
check_select_old_location_containing_folder (GList              *new_selection,
                                             GFile              *location,
                                             GFile              *previous_location)
{
	GFile *from_folder, *parent;

	/* If there is no new selection and the new location is
	 * a (grand)parent of the old location then we automatically
	 * select the folder the previous location was in */
	if (new_selection == NULL && previous_location != NULL &&
	    g_file_has_prefix (previous_location, location)) {
		from_folder = g_object_ref (previous_location);
		parent = g_file_get_parent (from_folder);
		while (parent != NULL && !g_file_equal (parent, location)) {
			g_object_unref (from_folder);
			from_folder = parent;
			parent = g_file_get_parent (from_folder);
		}

		if (parent != NULL) {
			new_selection = g_list_prepend (NULL, nautilus_file_get (from_folder));
			g_object_unref (parent);
		}

		g_object_unref (from_folder);
	}

        return new_selection;
}

static void
check_force_reload (GFile                      *location,
                    NautilusLocationChangeType  type)
{
        NautilusDirectory *directory;
        NautilusFile *file;
	gboolean force_reload;

        /* The code to force a reload is here because if we do it
	 * after determining an initial view (in the components), then
	 * we end up fetching things twice.
	 */
        directory = nautilus_directory_get (location);
        file = nautilus_file_get (location);

	if (type == NAUTILUS_LOCATION_CHANGE_RELOAD) {
		force_reload = TRUE;
	} else {
		force_reload = !nautilus_directory_is_local (directory);
	}

        /* We need to invalidate file attributes as well due to how mounting works
         * in the window slot and to avoid other caching issues.
         * Read handle_mount_if_needed for one example */
	if (force_reload) {
                nautilus_file_invalidate_all_attributes (file);
		nautilus_directory_force_reload (directory);
	}

        nautilus_directory_unref (directory);
        nautilus_file_unref (file);
}

static void
save_scroll_position_for_history (NautilusWindowSlot *slot)
{
        char *current_pos;
        /* Set current_bookmark scroll pos */
        if (slot->details->current_location_bookmark != NULL &&
            slot->details->content_view != NULL &&
            NAUTILUS_IS_FILES_VIEW (slot->details->content_view)) {
                current_pos = nautilus_files_view_get_first_visible_file (NAUTILUS_FILES_VIEW (slot->details->content_view));
                nautilus_bookmark_set_scroll_pos (slot->details->current_location_bookmark, current_pos);
                g_free (current_pos);
        }
}

/*
 * begin_location_change
 *
 * Change a window slot's location.
 * @window: The NautilusWindow whose location should be changed.
 * @location: A url specifying the location to load
 * @previous_location: The url that was previously shown in the window that initialized the change, if any
 * @new_selection: The initial selection to present after loading the location
 * @type: Which type of location change is this? Standard, back, forward, or reload?
 * @distance: If type is back or forward, the index into the back or forward chain. If
 * type is standard or reload, this is ignored, and must be 0.
 * @scroll_pos: The file to scroll to when the location is loaded.
 *
 * This is the core function for changing the location of a window. Every change to the
 * location begins here.
 */
static void
begin_location_change (NautilusWindowSlot         *slot,
                       GFile                      *location,
                       GFile                      *previous_location,
                       GList                      *new_selection,
                       NautilusLocationChangeType  type,
                       guint                       distance,
                       const char                 *scroll_pos)
{
	g_assert (slot != NULL);
        g_assert (location != NULL);
        g_assert (type == NAUTILUS_LOCATION_CHANGE_BACK
                  || type == NAUTILUS_LOCATION_CHANGE_FORWARD
                  || distance == 0);

	nautilus_profile_start (NULL);

        /* Avoid to update status from the current view in our async calls */
        nautilus_window_slot_disconnect_content_view (slot);
        /* We are going to change the location, so make sure we stop any loading
         * or searching of the previous view, so we avoid to be slow */
        nautilus_window_slot_stop_loading (slot);

	nautilus_window_slot_set_allow_stop (slot, TRUE);

        new_selection = check_select_old_location_containing_folder (new_selection, location, previous_location);

	g_assert (slot->details->pending_location == NULL);

	slot->details->pending_location = g_object_ref (location);
	slot->details->location_change_type = type;
	slot->details->location_change_distance = distance;
	slot->details->tried_mount = FALSE;
	slot->details->pending_selection = nautilus_file_list_copy (new_selection);

	slot->details->pending_scroll_to = g_strdup (scroll_pos);

        check_force_reload (location, type);

        save_scroll_position_for_history (slot);

	/* Get the info needed to make decisions about how to open the new location */
	slot->details->determine_view_file = nautilus_file_get (location);
	g_assert (slot->details->determine_view_file != NULL);

	nautilus_file_call_when_ready (slot->details->determine_view_file,
				       NAUTILUS_FILE_ATTRIBUTE_INFO |
				       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                                       got_file_info_for_view_selection_callback,
				       slot);

	nautilus_profile_end (NULL);
}

static void
nautilus_window_slot_set_location (NautilusWindowSlot *slot,
				   GFile *location)
{
	GFile *old_location;

	if (slot->details->location &&
	    g_file_equal (location, slot->details->location)) {
		/* The location name could be updated even if the location
		 * wasn't changed. This is the case for a search.
		 */
		nautilus_window_slot_update_title (slot);
		return;
	}

	old_location = slot->details->location;
	slot->details->location = g_object_ref (location);

        if (nautilus_window_slot_get_active (slot)) {
		nautilus_window_sync_location_widgets (slot->details->window);
	}

	nautilus_window_slot_update_title (slot);

	if (old_location) {
		g_object_unref (old_location);
	}

        g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_LOCATION]);
}

static void
viewed_file_changed_callback (NautilusFile *file,
                              NautilusWindowSlot *slot)
{
        GFile *new_location;
	gboolean is_in_trash, was_in_trash;

        g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (file == slot->details->viewed_file);

        if (!nautilus_file_is_not_yet_confirmed (file)) {
                slot->details->viewed_file_seen = TRUE;
        }

	was_in_trash = slot->details->viewed_file_in_trash;

	slot->details->viewed_file_in_trash = is_in_trash = nautilus_file_is_in_trash (file);

	if (nautilus_file_is_gone (file) || (is_in_trash && !was_in_trash)) {
                if (slot->details->viewed_file_seen) {
			GFile *go_to_file;
			GFile *parent;
			GFile *location;
			GMount *mount;

			parent = NULL;
			location = nautilus_file_get_location (file);

			if (g_file_is_native (location)) {
				mount = nautilus_get_mounted_mount_for_root (location);

				if (mount == NULL) {
					parent = g_file_get_parent (location);
				}

				g_clear_object (&mount);
			}

			if (parent != NULL) {
				/* auto-show existing parent */
				go_to_file = nautilus_find_existing_uri_in_hierarchy (parent);
			} else {
				go_to_file = g_file_new_for_path (g_get_home_dir ());
			}

			nautilus_window_slot_open_location_full (slot, go_to_file, 0, NULL);

			g_clear_object (&parent);
			g_object_unref (go_to_file);
			g_object_unref (location);
                }
	} else {
                new_location = nautilus_file_get_location (file);
		nautilus_window_slot_set_location (slot, new_location);
		g_object_unref (new_location);
        }
}

static void
nautilus_window_slot_go_home (NautilusWindowSlot *slot,
			      NautilusWindowOpenFlags flags)
{
	GFile *home;

	g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

	home = g_file_new_for_path (g_get_home_dir ());
	nautilus_window_slot_open_location_full (slot, home, flags, NULL);
	g_object_unref (home);
}

static void
nautilus_window_slot_set_viewed_file (NautilusWindowSlot *slot,
				      NautilusFile *file)
{
	NautilusFileAttributes attributes;

	if (slot->details->viewed_file == file) {
		return;
	}

	nautilus_file_ref (file);

	if (slot->details->viewed_file != NULL) {
		g_signal_handlers_disconnect_by_func (slot->details->viewed_file,
						      G_CALLBACK (viewed_file_changed_callback),
						      slot);
		nautilus_file_monitor_remove (slot->details->viewed_file,
					      slot);
	}

	if (file != NULL) {
		attributes =
			NAUTILUS_FILE_ATTRIBUTE_INFO |
			NAUTILUS_FILE_ATTRIBUTE_LINK_INFO;
		nautilus_file_monitor_add (file, slot, attributes);

		g_signal_connect_object (file, "changed",
					 G_CALLBACK (viewed_file_changed_callback), slot, 0);
	}

	nautilus_file_unref (slot->details->viewed_file);
	slot->details->viewed_file = file;
}

typedef struct {
	GCancellable *cancellable;
	NautilusWindowSlot *slot;
} MountNotMountedData;

static void
mount_not_mounted_callback (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	MountNotMountedData *data;
	NautilusWindowSlot *slot;
	GError *error;
	GCancellable *cancellable;

	data = user_data;
	slot = data->slot;
	cancellable = data->cancellable;
	g_free (data);

	if (g_cancellable_is_cancelled (cancellable)) {
		/* Cancelled, don't call back */
		g_object_unref (cancellable);
		return;
	}

	slot->details->mount_cancellable = NULL;

	slot->details->determine_view_file = nautilus_file_get (slot->details->pending_location);

	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error)) {
		slot->details->mount_error = error;
		got_file_info_for_view_selection_callback (slot->details->determine_view_file, slot);
		slot->details->mount_error = NULL;
		g_error_free (error);
	} else {
		nautilus_file_invalidate_all_attributes (slot->details->determine_view_file);
		nautilus_file_call_when_ready (slot->details->determine_view_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);
	}

	g_object_unref (cancellable);
}

static void
nautilus_window_slot_display_view_selection_failure (NautilusWindow *window,
                                                     NautilusFile   *file,
                                                     GFile          *location,
                                                     GError         *error)
{
	char *error_message;
	char *detail_message;
	char *scheme_string;

	/* Some sort of failure occurred. How 'bout we tell the user? */

	error_message = g_strdup (_("Oops! Something went wrong."));
	detail_message = NULL;
	if (error == NULL) {
		if (nautilus_file_is_directory (file)) {
			detail_message = g_strdup (_("Unable to display the contents of this folder."));
		} else {
			detail_message = g_strdup (_("This location doesn't appear to be a folder."));
		}
	} else if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_NOT_FOUND:
			detail_message = g_strdup (_("Unable to find the requested file. Please check the spelling and try again."));
			break;
		case G_IO_ERROR_NOT_SUPPORTED:
			scheme_string = g_file_get_uri_scheme (location);
			if (scheme_string != NULL) {
				detail_message = g_strdup_printf (_("“%s” locations are not supported."),
								  scheme_string);
			} else {
				detail_message = g_strdup (_("Unable to handle this kind of location."));
			}
			g_free (scheme_string);
			break;
		case G_IO_ERROR_NOT_MOUNTED:
			detail_message = g_strdup (_("Unable to access the requested location."));
			break;
		case G_IO_ERROR_PERMISSION_DENIED:
			detail_message = g_strdup (_("Don't have permission to access the requested location."));
			break;
		case G_IO_ERROR_HOST_NOT_FOUND:
			/* This case can be hit for user-typed strings like "foo" due to
			 * the code that guesses web addresses when there's no initial "/".
			 * But this case is also hit for legitimate web addresses when
			 * the proxy is set up wrong.
			 */
			detail_message = g_strdup (_("Unable to find the requested location. Please check the spelling or the network settings."));
			break;
		case G_IO_ERROR_CANCELLED:
		case G_IO_ERROR_FAILED_HANDLED:
			goto done;
		default:
			break;
		}
	}

	if (detail_message == NULL) {
		detail_message = g_strdup_printf (_("Unhandled error message: %s"), error->message);
	}

	eel_show_error_dialog (error_message, detail_message, GTK_WINDOW (window));
 done:
	g_free (error_message);
	g_free (detail_message);
}

/* FIXME: This works in the folowwing way. begin_location_change tries to get the
 * information of the file directly.
 * If the nautilus file finds that there is an error trying to get its
 * information and the error match that the file is not mounted, it sets an
 * internal attribute with the error then we try to mount it here.
 *
 * However, files are cached, and if the file doesn't get finalized in a location
 * change, because needs to be in the navigation history or is a bookmark, and the
 * file is not the root of the mount point, which is tracked by a volume monitor,
 * and it gets unmounted aftwerwards, the file doesn't realize it's unmounted, and
 * therefore this trick to open an unmounted file will fail the next time the user
 * tries to open.
 * For that, we need to always invalidate the file attributes when a location is
 * changed, which is done in check_force_reload.
 * A better way would be to make sure any children of the mounted root gets
 * akwnoledge by it either by adding a reference to its parent volume monitor
 * or with another solution. */
static gboolean
handle_mount_if_needed (NautilusWindowSlot *slot,
                        NautilusFile       *file)
{
	NautilusWindow *window;
  	GMountOperation *mount_op;
	MountNotMountedData *data;
	GFile *location;
        GError *error = NULL;
        gboolean needs_mount_handling = FALSE;

	window = nautilus_window_slot_get_window (slot);
        if (slot->details->mount_error) {
                error = g_error_copy (slot->details->mount_error);
        } else if (nautilus_file_get_file_info_error (file) != NULL) {
                error = g_error_copy (nautilus_file_get_file_info_error (file));
        }

        if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED &&
            !slot->details->tried_mount) {
                slot->details->tried_mount = TRUE;

                mount_op = gtk_mount_operation_new (GTK_WINDOW (window));
                g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
                location = nautilus_file_get_location (file);
                data = g_new0 (MountNotMountedData, 1);
                data->cancellable = g_cancellable_new ();
                data->slot = slot;
                slot->details->mount_cancellable = data->cancellable;
                g_file_mount_enclosing_volume (location, 0, mount_op, slot->details->mount_cancellable,
                                               mount_not_mounted_callback, data);
                g_object_unref (location);
                g_object_unref (mount_op);

                needs_mount_handling = TRUE;
        }

        g_clear_error (&error);

        return needs_mount_handling;
}

static gboolean
handle_regular_file_if_needed (NautilusWindowSlot *slot,
                               NautilusFile       *file)
{
        NautilusFile *parent_file;
        gboolean needs_regular_file_handling = FALSE;

        parent_file = nautilus_file_get_parent (file);
        if ((parent_file != NULL) &&
            nautilus_file_get_file_type (file) == G_FILE_TYPE_REGULAR) {
            if (slot->details->pending_selection != NULL) {
                nautilus_file_list_free (slot->details->pending_selection);
            }

            g_clear_object (&slot->details->pending_location);
            g_free (slot->details->pending_scroll_to);

            slot->details->pending_location = nautilus_file_get_parent_location (file);
            slot->details->pending_selection = g_list_prepend (NULL, nautilus_file_ref (file));
            slot->details->determine_view_file = nautilus_file_ref (parent_file);
            slot->details->pending_scroll_to = nautilus_file_get_uri (file);

            nautilus_file_invalidate_all_attributes (slot->details->determine_view_file);
            nautilus_file_call_when_ready (slot->details->determine_view_file,
                               NAUTILUS_FILE_ATTRIBUTE_INFO |
                               NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                               got_file_info_for_view_selection_callback,
                               slot);

           needs_regular_file_handling = TRUE;
        }

        nautilus_file_unref (parent_file);

        return needs_regular_file_handling;
}

static void
got_file_info_for_view_selection_callback (NautilusFile *file,
					   gpointer callback_data)
{
        GError *error = NULL;
	NautilusWindow *window;
	NautilusWindowSlot *slot;
	NautilusFile *viewed_file;
        NautilusView *view;
	GFile *location;

	NautilusApplication *app;

	slot = callback_data;
	window = nautilus_window_slot_get_window (slot);

	g_assert (slot->details->determine_view_file == file);
	slot->details->determine_view_file = NULL;

	nautilus_profile_start (NULL);

        if (handle_mount_if_needed (slot, file))
                goto done;

        if (handle_regular_file_if_needed (slot, file))
                goto done;

        if (slot->details->mount_error) {
                error = g_error_copy (slot->details->mount_error);
        } else if (nautilus_file_get_file_info_error (file) != NULL) {
                error = g_error_copy (nautilus_file_get_file_info_error (file));
        }

	location = slot->details->pending_location;

        /* desktop and other-locations GFile operations report G_IO_ERROR_NOT_SUPPORTED,
         * but it's not an actual error for Nautilus */
        if (!error || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
                view = nautilus_window_slot_get_view_for_location (slot, location);
                setup_view (slot, view);
	} else {
		if (error == NULL) {
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
					     _("Unable to load location"));
		}
		nautilus_window_slot_display_view_selection_failure (window,
                                                                     file,
                                                                     location,
                                                                     error);

		if (!gtk_widget_get_visible (GTK_WIDGET (window))) {
			/* Destroy never-had-a-chance-to-be-seen window. This case
			 * happens when a new window cannot display its initial URI.
			 */
			/* if this is the only window, we don't want to quit, so we redirect it to home */

			app = NAUTILUS_APPLICATION (g_application_get_default ());

			if (g_list_length (nautilus_application_get_windows (app)) == 1) {
				/* the user could have typed in a home directory that doesn't exist,
				   in which case going home would cause an infinite loop, so we
				   better test for that */

				if (!nautilus_is_root_directory (location)) {
					if (!nautilus_is_home_directory (location)) {
						nautilus_window_slot_go_home (slot, FALSE);
					} else {
						GFile *root;

						root = g_file_new_for_path ("/");
						/* the last fallback is to go to a known place that can't be deleted! */
						nautilus_window_slot_open_location_full (slot, location, 0, NULL);
						g_object_unref (root);
					}
				} else {
					gtk_widget_destroy (GTK_WIDGET (window));
				}
			} else {
				/* Since this is a window, destroying it will also unref it. */
				gtk_widget_destroy (GTK_WIDGET (window));
			}
		} else {
			GFile *slot_location;

			/* Clean up state of already-showing window */
			end_location_change (slot);
			slot_location = nautilus_window_slot_get_location (slot);

			/* We're missing a previous location (if opened location
			 * in a new tab) so close it and return */
			if (slot_location == NULL) {
				nautilus_window_slot_close (window, slot);
			} else {
				/* We disconnected this, so we need to re-connect it */
				viewed_file = nautilus_file_get (slot_location);
				nautilus_window_slot_set_viewed_file (slot, viewed_file);
				nautilus_file_unref (viewed_file);

				/* Leave the location bar showing the bad location that the user
				 * typed (or maybe achieved by dragging or something). Many times
				 * the mistake will just be an easily-correctable typo. The user
				 * can choose "Refresh" to get the original URI back in the location bar.
				 */
			}
		}
	}

 done:
	g_clear_error (&error);

	nautilus_file_unref (file);
	nautilus_profile_end (NULL);
}

/* Load a view into the window, either reusing the old one or creating
 * a new one. This happens when you want to load a new location, or just
 * switch to a different view.
 * If pending_location is set we're loading a new location and
 * pending_location/selection will be used. If not, we're just switching
 * view, and the current location will be used.
 */
static gboolean
setup_view (NautilusWindowSlot *slot,
            NautilusView       *view)
{
	gboolean ret = TRUE;
	GFile *old_location;

	nautilus_profile_start (NULL);

        nautilus_window_slot_disconnect_content_view (slot);

        slot->details->new_content_view = view;

        nautilus_window_slot_connect_new_content_view (slot);

	/* Forward search selection and state before loading the new model */
        old_location = slot->details->content_view ? nautilus_view_get_location (slot->details->content_view) : NULL;

	/* Actually load the pending location and selection: */
        if (slot->details->pending_location != NULL) {
		load_new_location (slot,
				   slot->details->pending_location,
				   slot->details->pending_selection,
				   FALSE,
				   TRUE);

		nautilus_file_list_free (slot->details->pending_selection);
		slot->details->pending_selection = NULL;
	} else if (old_location != NULL) {
                GList *selection;

                selection = nautilus_view_get_selection (slot->details->content_view);

		load_new_location (slot,
				   old_location,
				   selection,
				   FALSE,
				   TRUE);
		nautilus_file_list_free (selection);
	} else {
                ret = FALSE;
                goto out;
        }

        change_view (slot);
        gtk_widget_show (GTK_WIDGET (slot->details->window));

out:
	nautilus_profile_end (NULL);

	return ret;
}

static void
load_new_location (NautilusWindowSlot *slot,
		   GFile *location,
		   GList *selection,
		   gboolean tell_current_content_view,
		   gboolean tell_new_content_view)
{
	NautilusView *view;

	g_assert (slot != NULL);
	g_assert (location != NULL);

	view = NULL;

	nautilus_profile_start (NULL);
	/* Note, these may recurse into report_load_underway */
        if (slot->details->content_view != NULL && tell_current_content_view) {
		view = slot->details->content_view;
		nautilus_view_set_location (slot->details->content_view, location);
        }

        if (slot->details->new_content_view != NULL && tell_new_content_view &&
	    (!tell_current_content_view ||
	     slot->details->new_content_view != slot->details->content_view) ) {
		view = slot->details->new_content_view;
		nautilus_view_set_location (slot->details->new_content_view, location);
        }
        if (view) {
		/* new_content_view might have changed here if
		   report_load_underway was called from load_location */
		nautilus_view_set_selection (view, selection);
	}

	nautilus_profile_end (NULL);
}

static void
end_location_change (NautilusWindowSlot *slot)
{
	char *uri;

	uri = nautilus_window_slot_get_location_uri (slot);
	if (uri) {
		DEBUG ("Finished loading window for uri %s", uri);
		g_free (uri);
	}

	nautilus_window_slot_set_allow_stop (slot, FALSE);

	/* Now we can free details->pending_scroll_to, since the load_complete
	 * callback already has been emitted.
	 */
	g_free (slot->details->pending_scroll_to);
	slot->details->pending_scroll_to = NULL;

	free_location_change (slot);
}

static void
free_location_change (NautilusWindowSlot *slot)
{
	g_clear_object (&slot->details->pending_location);
	nautilus_file_list_free (slot->details->pending_selection);
	slot->details->pending_selection = NULL;

        /* Don't free details->pending_scroll_to, since thats needed until
         * the load_complete callback.
         */

	if (slot->details->mount_cancellable != NULL) {
		g_cancellable_cancel (slot->details->mount_cancellable);
		slot->details->mount_cancellable = NULL;
	}

        if (slot->details->determine_view_file != NULL) {
		nautilus_file_cancel_call_when_ready
			(slot->details->determine_view_file,
			 got_file_info_for_view_selection_callback, slot);
                slot->details->determine_view_file = NULL;
        }
}

static void
nautilus_window_slot_set_content_view (NautilusWindowSlot *slot,
                                       guint                id)
{
        NautilusFilesView *view;
        GList *selection;
	char *uri;

	g_assert (slot != NULL);

	uri = nautilus_window_slot_get_location_uri (slot);
	DEBUG ("Change view of window %s to %d", uri, id);
	g_free (uri);

	if (nautilus_window_slot_content_view_matches (slot, id)) {
		return;
        }

        selection = nautilus_view_get_selection (slot->details->content_view);
        view = nautilus_files_view_new (id, slot);

        nautilus_window_slot_stop_loading (slot);

        nautilus_window_slot_set_allow_stop (slot, TRUE);

        if (g_list_length (selection) == 0 && NAUTILUS_IS_FILES_VIEW (slot->details->content_view)) {
                /* If there is no selection, queue a scroll to the same icon that
                 * is currently visible */
                slot->details->pending_scroll_to = nautilus_files_view_get_first_visible_file (NAUTILUS_FILES_VIEW (slot->details->content_view));
        }

	slot->details->location_change_type = NAUTILUS_LOCATION_CHANGE_RELOAD;

        if (!setup_view (slot, NAUTILUS_VIEW (view))) {
		/* Just load the homedir. */
		nautilus_window_slot_go_home (slot, FALSE);
	}
}

void
nautilus_window_back_or_forward (NautilusWindow *window,
				 gboolean back,
				 guint distance,
				 NautilusWindowOpenFlags flags)
{
	NautilusWindowSlot *slot;
	GList *list;
	GFile *location;
	guint len;
	NautilusBookmark *bookmark;
	GFile *old_location;

	slot = nautilus_window_get_active_slot (window);
	list = back ? slot->details->back_list : slot->details->forward_list;

        len = (guint) g_list_length (list);

        /* If we can't move in the direction at all, just return. */
        if (len == 0)
                return;

        /* If the distance to move is off the end of the list, go to the end
           of the list. */
        if (distance >= len)
                distance = len - 1;

        bookmark = g_list_nth_data (list, distance);
	location = nautilus_bookmark_get_location (bookmark);

	if (flags != 0) {
		nautilus_window_slot_open_location_full (slot, location, flags, NULL);
	} else {
		char *scroll_pos;

		old_location = nautilus_window_slot_get_location (slot);
		scroll_pos = nautilus_bookmark_get_scroll_pos (bookmark);
		begin_location_change
			(slot,
			 location, old_location, NULL,
			 back ? NAUTILUS_LOCATION_CHANGE_BACK : NAUTILUS_LOCATION_CHANGE_FORWARD,
			 distance,
			 scroll_pos);

		g_free (scroll_pos);
	}

	g_object_unref (location);
}

/* reload the contents of the window */
static void
nautilus_window_slot_force_reload (NautilusWindowSlot *slot)
{
	GFile *location;
        char *current_pos;
	GList *selection;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	location = nautilus_window_slot_get_location (slot);
	if (location == NULL) {
		return;
	}

	/* peek_slot_field (window, location) can be free'd during the processing
	 * of begin_location_change, so make a copy
	 */
	g_object_ref (location);
	current_pos = NULL;
	selection = NULL;
        if (slot->details->new_content_view) {
		selection = nautilus_view_get_selection (slot->details->content_view);

                if (NAUTILUS_IS_FILES_VIEW (slot->details->new_content_view)) {
                        current_pos = nautilus_files_view_get_first_visible_file (NAUTILUS_FILES_VIEW (slot->details->content_view));
                }
	}
	begin_location_change
		(slot, location, location, selection,
		 NAUTILUS_LOCATION_CHANGE_RELOAD, 0, current_pos);
        g_free (current_pos);
	g_object_unref (location);
	nautilus_file_list_free (selection);
}

void
nautilus_window_slot_queue_reload (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (nautilus_window_slot_get_location (slot) == NULL) {
		return;
	}

	if (slot->details->pending_location != NULL
	    || slot->details->content_view == NULL
	    || nautilus_view_is_loading (slot->details->content_view)) {
		/* there is a reload in flight */
		slot->details->needs_reload = TRUE;
		return;
	}

	nautilus_window_slot_force_reload (slot);
}

static void
nautilus_window_slot_clear_forward_list (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->details->forward_list, g_object_unref);
	slot->details->forward_list = NULL;
}

static void
nautilus_window_slot_clear_back_list (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->details->back_list, g_object_unref);
	slot->details->back_list = NULL;
}

static void
nautilus_window_slot_update_bookmark (NautilusWindowSlot *slot, NautilusFile *file)
{
        gboolean recreate;
	GFile *new_location;

	new_location = nautilus_file_get_location (file);

	if (slot->details->current_location_bookmark == NULL) {
		recreate = TRUE;
	} else {
		GFile *bookmark_location;
		bookmark_location = nautilus_bookmark_get_location (slot->details->current_location_bookmark);
		recreate = !g_file_equal (bookmark_location, new_location);
		g_object_unref (bookmark_location);
        }

	if (recreate) {
		char *display_name = NULL;

		/* We've changed locations, must recreate bookmark for current location. */
		g_clear_object (&slot->details->last_location_bookmark);
		slot->details->last_location_bookmark = slot->details->current_location_bookmark;

		display_name = nautilus_file_get_display_name (file);
		slot->details->current_location_bookmark = nautilus_bookmark_new (new_location, display_name);
		g_free (display_name);
        }

	g_object_unref (new_location);
}

static void
check_bookmark_location_matches (NautilusBookmark *bookmark, GFile *location)
{
        GFile *bookmark_location;
        char *bookmark_uri, *uri;

	bookmark_location = nautilus_bookmark_get_location (bookmark);
	if (!g_file_equal (location, bookmark_location)) {
		bookmark_uri = g_file_get_uri (bookmark_location);
		uri = g_file_get_uri (location);
		g_warning ("bookmark uri is %s, but expected %s", bookmark_uri, uri);
		g_free (uri);
		g_free (bookmark_uri);
	}
	g_object_unref (bookmark_location);
}

/* Debugging function used to verify that the last_location_bookmark
 * is in the state we expect when we're about to use it to update the
 * Back or Forward list.
 */
static void
check_last_bookmark_location_matches_slot (NautilusWindowSlot *slot)
{
	check_bookmark_location_matches (slot->details->last_location_bookmark,
					 nautilus_window_slot_get_location (slot));
}

static void
handle_go_direction (NautilusWindowSlot *slot,
		     GFile              *location,
		     gboolean            forward)
{
	GList **list_ptr, **other_list_ptr;
	GList *list, *other_list, *link;
	NautilusBookmark *bookmark;
	gint i;

	list_ptr = (forward) ? (&slot->details->forward_list) : (&slot->details->back_list);
	other_list_ptr = (forward) ? (&slot->details->back_list) : (&slot->details->forward_list);
	list = *list_ptr;
	other_list = *other_list_ptr;

	/* Move items from the list to the other list. */
	g_assert (g_list_length (list) > slot->details->location_change_distance);
	check_bookmark_location_matches (g_list_nth_data (list, slot->details->location_change_distance),
					 location);
	g_assert (nautilus_window_slot_get_location (slot) != NULL);

	/* Move current location to list */
	check_last_bookmark_location_matches_slot (slot);

	/* Use the first bookmark in the history list rather than creating a new one. */
	other_list = g_list_prepend (other_list, slot->details->last_location_bookmark);
	g_object_ref (other_list->data);

	/* Move extra links from the list to the other list */
	for (i = 0; i < slot->details->location_change_distance; ++i) {
		bookmark = NAUTILUS_BOOKMARK (list->data);
		list = g_list_remove (list, bookmark);
		other_list = g_list_prepend (other_list, bookmark);
	}

	/* One bookmark falls out of back/forward lists and becomes viewed location */
	link = list;
	list = g_list_remove_link (list, link);
	g_object_unref (link->data);
	g_list_free_1 (link);

	*list_ptr = list;
	*other_list_ptr = other_list;
}

static void
handle_go_elsewhere (NautilusWindowSlot *slot,
		     GFile *location)
{
	GFile *slot_location;

	/* Clobber the entire forward list, and move displayed location to back list */
	nautilus_window_slot_clear_forward_list (slot);
	slot_location = nautilus_window_slot_get_location (slot);

	if (slot_location != NULL) {
		/* If we're returning to the same uri somehow, don't put this uri on back list.
		 * This also avoids a problem where set_displayed_location
		 * didn't update last_location_bookmark since the uri didn't change.
		 */
		if (!g_file_equal (slot_location, location)) {
			/* Store bookmark for current location in back list, unless there is no current location */
			check_last_bookmark_location_matches_slot (slot);
			/* Use the first bookmark in the history list rather than creating a new one. */
			slot->details->back_list = g_list_prepend (slot->details->back_list,
							  slot->details->last_location_bookmark);
			g_object_ref (slot->details->back_list->data);
		}
	}
}

static void
update_history (NautilusWindowSlot *slot,
                NautilusLocationChangeType type,
                GFile *new_location)
{
        switch (type) {
        case NAUTILUS_LOCATION_CHANGE_STANDARD:
		handle_go_elsewhere (slot, new_location);
                return;
        case NAUTILUS_LOCATION_CHANGE_RELOAD:
                /* for reload there is no work to do */
                return;
        case NAUTILUS_LOCATION_CHANGE_BACK:
                handle_go_direction (slot, new_location, FALSE);
                return;
        case NAUTILUS_LOCATION_CHANGE_FORWARD:
                handle_go_direction (slot, new_location, TRUE);
                return;
        }
	g_return_if_fail (FALSE);
}

typedef struct {
	NautilusWindowSlot *slot;
	GCancellable *cancellable;
	GMount *mount;
} FindMountData;

static void
nautilus_window_slot_show_x_content_bar (NautilusWindowSlot *slot, GMount *mount, const char **x_content_types)
{
	GtkWidget *bar;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	bar = nautilus_x_content_bar_new (mount, x_content_types);
	gtk_widget_show (bar);
	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

static void
found_content_type_cb (const char **x_content_types,
		       gpointer user_data)
{
	NautilusWindowSlot *slot;
	FindMountData *data = user_data;

	if (g_cancellable_is_cancelled (data->cancellable)) {
		goto out;
	}

	slot = data->slot;

	if (x_content_types != NULL && x_content_types[0] != NULL) {
		nautilus_window_slot_show_x_content_bar (slot, data->mount, x_content_types);
	}

	slot->details->find_mount_cancellable = NULL;

 out:
	g_object_unref (data->mount);
	g_object_unref (data->cancellable);
	g_free (data);
}

static void
found_mount_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	FindMountData *data = user_data;
	GMount *mount;

	if (g_cancellable_is_cancelled (data->cancellable)) {
		goto out;
	}

	mount = g_file_find_enclosing_mount_finish (G_FILE (source_object),
						    res,
						    NULL);
	if (mount != NULL) {
		data->mount = mount;
		nautilus_get_x_content_types_for_mount_async (mount,
							      found_content_type_cb,
							      data->cancellable,
							      data);
		return;
	}

	data->slot->details->find_mount_cancellable = NULL;

 out:
	g_object_unref (data->cancellable);
	g_free (data);
}

static void
nautilus_window_slot_show_trash_bar (NautilusWindowSlot *slot)
{
	GtkWidget *bar;
        NautilusView *view;

	view = nautilus_window_slot_get_current_view (slot);
	bar = nautilus_trash_bar_new (NAUTILUS_FILES_VIEW (view));
	gtk_widget_show (bar);

	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

static void
nautilus_window_slot_show_special_location_bar (NautilusWindowSlot     *slot,
						NautilusSpecialLocation special_location)
{
	GtkWidget *bar;

	bar = nautilus_special_location_bar_new (special_location);
	gtk_widget_show (bar);

	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

static void
slot_add_extension_extra_widgets (NautilusWindowSlot *slot)
{
	GList *providers, *l;
	GtkWidget *widget;
	char *uri;
	NautilusWindow *window;

	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_LOCATION_WIDGET_PROVIDER);
	window = nautilus_window_slot_get_window (slot);

	uri = nautilus_window_slot_get_location_uri (slot);
	for (l = providers; l != NULL; l = l->next) {
		NautilusLocationWidgetProvider *provider;

		provider = NAUTILUS_LOCATION_WIDGET_PROVIDER (l->data);
		widget = nautilus_location_widget_provider_get_widget (provider, uri, GTK_WIDGET (window));
		if (widget != NULL) {
			nautilus_window_slot_add_extra_location_widget (slot, widget);
		}
	}
	g_free (uri);

	nautilus_module_extension_list_free (providers);
}

static void
nautilus_window_slot_update_for_new_location (NautilusWindowSlot *slot)
{
        GFile *new_location;
        NautilusFile *file;

	new_location = slot->details->pending_location;
	slot->details->pending_location = NULL;

	file = nautilus_file_get (new_location);
	nautilus_window_slot_update_bookmark (slot, file);

	update_history (slot, slot->details->location_change_type, new_location);

        /* Create a NautilusFile for this location, so we can catch it
         * if it goes away.
         */
	nautilus_window_slot_set_viewed_file (slot, file);
	slot->details->viewed_file_seen = !nautilus_file_is_not_yet_confirmed (file);
	slot->details->viewed_file_in_trash = nautilus_file_is_in_trash (file);
        nautilus_file_unref (file);

	nautilus_window_slot_set_location (slot, new_location);

	/* Sync the actions for this new location. */
        nautilus_window_slot_sync_actions (slot);
}

static void
view_started_loading (NautilusWindowSlot *slot,
                      NautilusView       *view)
{
        if (view == slot->details->content_view) {
                nautilus_window_slot_set_allow_stop (slot, TRUE);
        }

        gtk_widget_grab_focus (GTK_WIDGET (slot->details->window));

        gtk_widget_show (GTK_WIDGET (slot->details->window));

        nautilus_window_slot_set_loading (slot, TRUE);
}

static void
view_ended_loading (NautilusWindowSlot *slot,
                    NautilusView       *view)
{
        if (view == slot->details->content_view) {
                if (NAUTILUS_IS_FILES_VIEW (view) && slot->details->pending_scroll_to != NULL) {
                        nautilus_files_view_scroll_to_file (NAUTILUS_FILES_VIEW (slot->details->content_view), slot->details->pending_scroll_to);
                }

                end_location_change (slot);
        }

        if (slot->details->needs_reload) {
                nautilus_window_slot_queue_reload (slot);
                slot->details->needs_reload = FALSE;
        }

        nautilus_window_slot_set_allow_stop (slot, FALSE);

        nautilus_window_slot_set_loading (slot, FALSE);
}

static void
view_is_loading_changed_cb (GObject            *object,
                            GParamSpec         *pspec,
                            NautilusWindowSlot *slot)
{
        NautilusView *view;

        view = NAUTILUS_VIEW (object);

	nautilus_profile_start (NULL);

        if (nautilus_view_is_loading (view)) {
                view_started_loading (slot, view);
        } else {
                view_ended_loading (slot, view);
        }

        nautilus_profile_end (NULL);
}

static void
nautilus_window_slot_setup_extra_location_widgets (NautilusWindowSlot *slot)
{
	GFile *location;
	FindMountData *data;
	NautilusDirectory *directory;

	location = nautilus_window_slot_get_current_location (slot);

	if (location == NULL) {
		return;
	}

	directory = nautilus_directory_get (location);

	if (nautilus_directory_is_in_trash (directory)) {
		nautilus_window_slot_show_trash_bar (slot);
	} else {
		NautilusFile *file;
		GFile *scripts_file;
		char *scripts_path = nautilus_get_scripts_directory_path ();

		scripts_file = g_file_new_for_path (scripts_path);
		g_free (scripts_path);

		file = nautilus_file_get (location);

		if (nautilus_should_use_templates_directory () &&
		    nautilus_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES)) {
			nautilus_window_slot_show_special_location_bar (slot, NAUTILUS_SPECIAL_LOCATION_TEMPLATES);
		} else if (g_file_equal (location, scripts_file)) {
			nautilus_window_slot_show_special_location_bar (slot, NAUTILUS_SPECIAL_LOCATION_SCRIPTS);
		}

		g_object_unref (scripts_file);
		nautilus_file_unref (file);
	}

	/* need the mount to determine if we should put up the x-content cluebar */
	if (slot->details->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->details->find_mount_cancellable);
		slot->details->find_mount_cancellable = NULL;
	}

	data = g_new (FindMountData, 1);
	data->slot = slot;
	data->cancellable = g_cancellable_new ();
	data->mount = NULL;

	slot->details->find_mount_cancellable = data->cancellable;
	g_file_find_enclosing_mount_async (location,
					   G_PRIORITY_DEFAULT,
					   data->cancellable,
					   found_mount_cb,
					   data);

	nautilus_directory_unref (directory);

	slot_add_extension_extra_widgets (slot);
}

static void
nautilus_window_slot_connect_new_content_view (NautilusWindowSlot *slot)
{
        if (slot->details->new_content_view) {
                g_signal_connect (slot->details->new_content_view,
                                  "notify::is-loading",
                                  G_CALLBACK (view_is_loading_changed_cb),
                                  slot);
        }
}

static void
nautilus_window_slot_disconnect_content_view (NautilusWindowSlot *slot)
{
        if (slot->details->new_content_view && NAUTILUS_IS_FILES_VIEW (slot->details->new_content_view)) {
		/* disconnect old view */
                g_signal_handlers_disconnect_by_func (slot->details->content_view,
                                                      G_CALLBACK (view_is_loading_changed_cb),
                                                      slot);
	}
}

static void
nautilus_window_slot_switch_new_content_view (NautilusWindowSlot *slot)
{
	GtkWidget *widget;
        gboolean reusing_view;

        reusing_view = slot->details->new_content_view &&
                       gtk_widget_get_parent (GTK_WIDGET (slot->details->new_content_view)) != NULL;
        /* We are either reusing the view, so new_content_view and content_view
         * are the same, or the new_content_view is invalid */
        if (slot->details->new_content_view == NULL || reusing_view)
                goto done;

	if (slot->details->content_view != NULL) {
		widget = GTK_WIDGET (slot->details->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->details->content_view);
		slot->details->content_view = NULL;
	}

	if (slot->details->new_content_view != NULL) {
		slot->details->content_view = slot->details->new_content_view;
		slot->details->new_content_view = NULL;

		widget = GTK_WIDGET (slot->details->content_view);
                gtk_container_add (GTK_CONTAINER (slot), widget);
                gtk_widget_set_vexpand (widget, TRUE);
		gtk_widget_show (widget);

                g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_ICON]);
                g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_VIEW_WIDGET]);
	}

done:
        /* Clean up, so we don't confuse having a new_content_view available or
         * just that we didn't care about it here */
        slot->details->new_content_view = NULL;

}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
change_view (NautilusWindowSlot *slot)
{
	/* Switch to the new content view.
	 * Destroy the extra location widgets first, since they might hold
	 * a pointer to the old view, which will possibly be destroyed inside
	 * nautilus_window_slot_switch_new_content_view().
	 */
	nautilus_window_slot_remove_extra_location_widgets (slot);
	nautilus_window_slot_switch_new_content_view (slot);

	if (slot->details->pending_location != NULL) {
		/* Tell the window we are finished. */
		nautilus_window_slot_update_for_new_location (slot);
	}

	/* Now that we finished switching to the new location,
	 * add back the extra location widgets.
	 */
	nautilus_window_slot_setup_extra_location_widgets (slot);
}

static void
nautilus_window_slot_dispose (GObject *object)
{
	NautilusWindowSlot *slot;
	GtkWidget *widget;

	slot = NAUTILUS_WINDOW_SLOT (object);

	nautilus_window_slot_clear_forward_list (slot);
	nautilus_window_slot_clear_back_list (slot);

	nautilus_window_slot_remove_extra_location_widgets (slot);

	if (slot->details->content_view) {
		widget = GTK_WIDGET (slot->details->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->details->content_view);
		slot->details->content_view = NULL;
	}

	if (slot->details->new_content_view) {
		widget = GTK_WIDGET (slot->details->new_content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->details->new_content_view);
		slot->details->new_content_view = NULL;
	}

	nautilus_window_slot_set_viewed_file (slot, NULL);

        g_clear_object (&slot->details->location);

	nautilus_file_list_free (slot->details->pending_selection);
	slot->details->pending_selection = NULL;

	g_clear_object (&slot->details->current_location_bookmark);
	g_clear_object (&slot->details->last_location_bookmark);

	if (slot->details->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->details->find_mount_cancellable);
		slot->details->find_mount_cancellable = NULL;
	}

	slot->details->window = NULL;

	g_free (slot->details->title);
	slot->details->title = NULL;

	free_location_change (slot);

	G_OBJECT_CLASS (nautilus_window_slot_parent_class)->dispose (object);
}

static void
nautilus_window_slot_grab_focus (GtkWidget *widget)
{
        NautilusWindowSlot *slot;

        slot = NAUTILUS_WINDOW_SLOT (widget);

        GTK_WIDGET_CLASS (nautilus_window_slot_parent_class)->grab_focus (widget);

        if (nautilus_window_slot_get_search_visible (slot)) {
                gtk_widget_grab_focus (GTK_WIDGET (slot->details->query_editor));
        } else if (slot->details->content_view) {
                gtk_widget_grab_focus (GTK_WIDGET (slot->details->content_view));
        } else if (slot->details->new_content_view) {
                gtk_widget_grab_focus (GTK_WIDGET (slot->details->new_content_view));
        }
}

static void
nautilus_window_slot_class_init (NautilusWindowSlotClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	klass->active = real_active;
	klass->inactive = real_inactive;

	oclass->dispose = nautilus_window_slot_dispose;
	oclass->constructed = nautilus_window_slot_constructed;
	oclass->set_property = nautilus_window_slot_set_property;
	oclass->get_property = nautilus_window_slot_get_property;

        widget_class->grab_focus = nautilus_window_slot_grab_focus;

	signals[ACTIVE] =
		g_signal_new ("active",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusWindowSlotClass, active),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[INACTIVE] =
		g_signal_new ("inactive",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusWindowSlotClass, inactive),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

        properties[PROP_ACTIVE] =
                g_param_spec_boolean ("active",
                                      "Whether the slot is active",
                                      "Whether the slot is the active slot of the window",
                                      FALSE,
                                      G_PARAM_READWRITE);
        properties[PROP_LOADING] =
                g_param_spec_boolean ("loading",
                                      "Whether the slot loading",
                                      "Whether the slot is loading a new location",
                                      FALSE,
                                      G_PARAM_READABLE);

	properties[PROP_WINDOW] =
		g_param_spec_object ("window",
				     "The NautilusWindow",
				     "The NautilusWindow this slot is part of",
				     NAUTILUS_TYPE_WINDOW,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

        properties[PROP_ICON] =
		g_param_spec_object ("icon",
				     "Icon that represents the slot",
				     "The icon that represents the slot",
				     G_TYPE_ICON,
				     G_PARAM_READABLE);

        properties[PROP_VIEW_WIDGET] =
		g_param_spec_object ("view-widget",
				     "Widget for the view menu",
				     "The widget for the view's menu",
				     GTK_TYPE_WIDGET,
				     G_PARAM_READABLE);

        properties[PROP_LOCATION] =
		g_param_spec_object ("location",
				     "Current location visible on the slot",
				     "Either the location that is used currently, or the pending location. Clients will see the same value they set, and therefore it will be cosistent from clients point of view.",
				     G_TYPE_FILE,
				     G_PARAM_READWRITE);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (klass, sizeof (NautilusWindowSlotDetails));
}

GFile *
nautilus_window_slot_get_location (NautilusWindowSlot *slot)
{
	g_assert (slot != NULL);

	return slot->details->location;
}

const gchar *
nautilus_window_slot_get_title (NautilusWindowSlot *slot)
{
	return slot->details->title;
}

char *
nautilus_window_slot_get_location_uri (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->details->location) {
		return g_file_get_uri (slot->details->location);
	}
	return NULL;
}

NautilusWindow *
nautilus_window_slot_get_window (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	return slot->details->window;
}

void
nautilus_window_slot_set_window (NautilusWindowSlot *slot,
				 NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (slot->details->window != window) {
		slot->details->window = window;
		g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_WINDOW]);
	}
}

/* nautilus_window_slot_update_title:
 *
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NautilusWindowSlot in question.
 *
 */
void
nautilus_window_slot_update_title (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	char *title;
	gboolean do_sync = FALSE;

	title = nautilus_compute_title_for_location (slot->details->location);
	window = nautilus_window_slot_get_window (slot);

	if (g_strcmp0 (title, slot->details->title) != 0) {
		do_sync = TRUE;

		g_free (slot->details->title);
		slot->details->title = title;
		title = NULL;
	}

	if (strlen (slot->details->title) > 0) {
		do_sync = TRUE;
	}

	if (do_sync) {
		nautilus_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

gboolean
nautilus_window_slot_get_allow_stop (NautilusWindowSlot *slot)
{
	return slot->details->allow_stop;
}

void
nautilus_window_slot_set_allow_stop (NautilusWindowSlot *slot,
				     gboolean allow)
{
	NautilusWindow *window;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	slot->details->allow_stop = allow;

	window = nautilus_window_slot_get_window (slot);
	nautilus_window_sync_allow_stop (window, slot);
}

void
nautilus_window_slot_stop_loading (NautilusWindowSlot *slot)
{
        GList *selection;
        GFile *location;
        NautilusDirectory *directory;

        location = nautilus_window_slot_get_location (slot);

        directory = nautilus_directory_get (slot->details->location);

        if (NAUTILUS_IS_FILES_VIEW (slot->details->content_view)) {
                nautilus_files_view_stop_loading (NAUTILUS_FILES_VIEW (slot->details->content_view));
        }

        nautilus_directory_unref (directory);

        if (slot->details->pending_location != NULL &&
            location != NULL &&
            slot->details->content_view != NULL &&
            NAUTILUS_IS_FILES_VIEW (slot->details->content_view)) {
                /* No need to tell the new view - either it is the
                 * same as the old view, in which case it will already
                 * be told, or it is the very pending change we wish
                 * to cancel.
                 */
                selection = nautilus_view_get_selection (slot->details->content_view);
                load_new_location (slot,
                                   location,
                                   selection,
                                   TRUE,
                                   FALSE);
                nautilus_file_list_free (selection);
        }

        end_location_change (slot);

        if (slot->details->new_content_view) {
                g_object_unref (slot->details->new_content_view);
                slot->details->new_content_view = NULL;
        }
}

NautilusView*
nautilus_window_slot_get_current_view (NautilusWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
		return slot->details->content_view;
	} else if (slot->details->new_content_view) {
		return slot->details->new_content_view;
	}

	return NULL;
}

NautilusBookmark *
nautilus_window_slot_get_bookmark (NautilusWindowSlot *slot)
{
	return slot->details->current_location_bookmark;
}

GList *
nautilus_window_slot_get_back_history (NautilusWindowSlot *slot)
{
	return slot->details->back_list;
}

GList *
nautilus_window_slot_get_forward_history (NautilusWindowSlot *slot)
{
	return slot->details->forward_list;
}

NautilusWindowSlot *
nautilus_window_slot_new (NautilusWindow *window)
{
	return g_object_new (NAUTILUS_TYPE_WINDOW_SLOT,
			     "window", window,
			     NULL);
}

GIcon*
nautilus_window_slot_get_icon (NautilusWindowSlot *slot)
{
        NautilusView *view;

        g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot), NULL);

        view = nautilus_window_slot_get_current_view (slot);

        return view ? nautilus_view_get_icon (view) : NULL;
}

GtkWidget*
nautilus_window_slot_get_view_widget (NautilusWindowSlot *slot)
{
        NautilusView *view;

        g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot), NULL);

        view = nautilus_window_slot_get_current_view (slot);

        return view ? nautilus_view_get_view_widget (view) : NULL;
}

gboolean
nautilus_window_slot_get_active (NautilusWindowSlot *slot)
{
        g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot), FALSE);

        return slot->details->active;
}

void
nautilus_window_slot_set_active (NautilusWindowSlot *slot,
                                 gboolean            active)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

        if (slot->details->active != active) {
                slot->details->active = active;

                if (active) {
                        g_signal_emit (slot, signals[ACTIVE], 0);
                } else {
                        g_signal_emit (slot, signals[INACTIVE], 0);
                }

                g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_ACTIVE]);
        }
}

static void
nautilus_window_slot_set_loading (NautilusWindowSlot *slot,
                                  gboolean            loading)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

        slot->details->loading = loading;

        g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_LOADING]);
}

gboolean
nautilus_window_slot_get_loading (NautilusWindowSlot *slot)
{
        g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot), FALSE);

        return slot->details->loading;
}

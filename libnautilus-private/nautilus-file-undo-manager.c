/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-undo-manager.c - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-file-undo-manager.h"

#include "nautilus-file-operations.h"
#include "nautilus-file.h"
#include "nautilus-file-undo-operations.h"
#include "nautilus-file-undo-types.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <gdk/gdk.h>
#include <eel/eel-glib-extensions.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_UNDO
#include "nautilus-debug.h"

/* Default depth of the undo/redo stack. */
#define DEFAULT_UNDO_DEPTH 32

#define NAUTILUS_FILE_UNDO_MANAGER_GET_PRIVATE(o)	\
	 (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_FILE_UNDO_MANAGER, NautilusFileUndoManagerPrivate))

enum {
	SIGNAL_UNDO_CHANGED,
	NUM_SIGNALS,
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (NautilusFileUndoManager, nautilus_file_undo_manager, G_TYPE_OBJECT)

static NautilusFileUndoManager *singleton = NULL;

/* must be called with the mutex locked */
static NautilusFileUndoData *
get_next_redo_action (NautilusFileUndoManagerPrivate *priv)
{
	NautilusFileUndoData *retval = NULL;

	if (g_queue_is_empty (priv->stack)) {
		DEBUG ("Queue is empty, no redo actions to return");
		goto out;
	}

	if (priv->index == 0) {
		DEBUG ("Queue has only undoable actions, no redo actions to return");
		goto out;
	}

	DEBUG ("Getting redo action for index %d", priv->index);
	retval = g_queue_peek_nth (priv->stack, priv->index - 1);

	if (retval && retval->locked) {
		DEBUG ("Action is locked, no redo actions to return");
		retval = NULL;
	}

 out:
	DEBUG ("Returning %p as next redo action", retval);
	return retval;
}

/* must be called with the mutex locked */
static NautilusFileUndoData *
get_next_undo_action (NautilusFileUndoManagerPrivate *priv)
{
	NautilusFileUndoData *retval = NULL;
	guint stack_size;

	if (g_queue_is_empty (priv->stack)) {
		DEBUG ("Queue is empty, no undo actions to return");
		goto out;
	}

	stack_size = g_queue_get_length (priv->stack);

	if (priv->index == stack_size) {
		DEBUG ("Queue has only of undone actions, no undo actions to return");
		goto out;
	}

	DEBUG ("Getting undo action for index %d", priv->index);
	retval = g_queue_peek_nth (priv->stack, priv->index);

	if (retval->locked) {
		DEBUG ("Action is locked, no undo actions to return");
		retval = NULL;
	}

 out:
	DEBUG ("Returning %p as next undo action", retval);
	return retval;
}

/* must be called with the mutex locked */
static void
clear_redo_actions (NautilusFileUndoManagerPrivate *priv)
{
	DEBUG ("Clearing redo actions, index is %d", priv->index);

	while (priv->index > 0) {
		NautilusFileUndoData *head;

		head = g_queue_pop_head (priv->stack);
		nautilus_file_undo_data_free (head);

		DEBUG ("Removed action, index was %d", priv->index);

		priv->index--;
	}
}

/* must be called with the mutex locked */
static gboolean
can_undo (NautilusFileUndoManagerPrivate *priv)
{
	return (get_next_undo_action (priv) != NULL);
}

/* must be called with the mutex locked */
static gboolean
can_redo (NautilusFileUndoManagerPrivate *priv)
{
	return (get_next_redo_action (priv) != NULL);
}

/* must be called with the mutex locked */
static NautilusFileUndoData *
stack_scroll_right (NautilusFileUndoManagerPrivate *priv)
{
	NautilusFileUndoData *data;

	if (!can_undo (priv)) {
		DEBUG ("Scrolling stack right, but no undo actions");
		return NULL;
	}

	data = g_queue_peek_nth (priv->stack, priv->index);

	if (priv->index < g_queue_get_length (priv->stack)) {
		priv->index++;
		DEBUG ("Scrolling stack right, increasing index to %d", priv->index);
	}

	return data;
}

/* must be called with the mutex locked */
static NautilusFileUndoData *
stack_scroll_left (NautilusFileUndoManagerPrivate *priv)
{
	NautilusFileUndoData *data;

	if (!can_redo (priv)) {
		DEBUG ("Scrolling stack left, but no redo actions");
		return NULL;
	}

	priv->index--;
	data = g_queue_peek_nth (priv->stack, priv->index);

	DEBUG ("Scrolling stack left, decreasing index to %d", priv->index);

	return data;
}

/* must be called with the mutex locked */
static void
stack_clear_n_oldest (GQueue *stack,
		      guint n)
{
	NautilusFileUndoData *action;
	guint i;

	DEBUG ("Clearing %u oldest actions from the undo stack", n);

	for (i = 0; i < n; i++) {
		action = (NautilusFileUndoData *) g_queue_pop_tail (stack);
		if (action->locked) {
			action->freed = TRUE;
		} else {
			nautilus_file_undo_data_free (action);
		}
	}
}

/* must be called with the mutex locked */
static void
stack_ensure_size (NautilusFileUndoManager *self)
{
	NautilusFileUndoManagerPrivate *priv = self->priv;
	guint length;

	length = g_queue_get_length (priv->stack);

	if (length > priv->undo_levels) {
		if (priv->index > (priv->undo_levels + 1)) {
			/* If the index will fall off the stack
			 * move it back to the maximum position */
			priv->index = priv->undo_levels + 1;
		}
		stack_clear_n_oldest (priv->stack, length - (priv->undo_levels));
	}
}

/* must be called with the mutex locked */
static void
stack_push_action (NautilusFileUndoManager *self,
		   NautilusFileUndoData *action)
{
	NautilusFileUndoManagerPrivate *priv = self->priv;

	/* clear all the redo items, as we're pushing a new undoable action */
	clear_redo_actions (priv);

	g_queue_push_head (priv->stack, action);

	/* cleanup items in excess, if any */
	stack_ensure_size (self);
}

static void
nautilus_file_undo_manager_init (NautilusFileUndoManager * self)
{
	NautilusFileUndoManagerPrivate *priv;

	priv = NAUTILUS_FILE_UNDO_MANAGER_GET_PRIVATE (self);

	self->priv = priv;

	/* Initialize private fields */
	priv->stack = g_queue_new ();
	g_mutex_init(&priv->mutex);
	priv->index = 0;
	priv->undo_redo_flag = FALSE;

	/* initialize the undo stack depth */
	priv->undo_levels = DEFAULT_UNDO_DEPTH;

	g_mutex_lock (&priv->mutex);
	stack_ensure_size (self);
	g_mutex_unlock (&priv->mutex);
}

static void
nautilus_file_undo_manager_finalize (GObject * object)
{
	NautilusFileUndoManager *self = NAUTILUS_FILE_UNDO_MANAGER (object);
	NautilusFileUndoManagerPrivate *priv = self->priv;

	g_mutex_lock (&priv->mutex);
	
	g_queue_foreach (priv->stack, (GFunc) nautilus_file_undo_data_free, NULL);
	g_queue_free (priv->stack);

	g_mutex_unlock (&priv->mutex);
	g_mutex_clear (&priv->mutex);

	G_OBJECT_CLASS (nautilus_file_undo_manager_parent_class)->finalize (object);
}

static GObject *
nautilus_file_undo_manager_constructor (GType type,
					 guint n_construct_params,
					 GObjectConstructParam *construct_params)
{
	GObject *retval;

	if (singleton != NULL) {
		return G_OBJECT (singleton);
	}

	retval = G_OBJECT_CLASS (nautilus_file_undo_manager_parent_class)->constructor
		(type, n_construct_params, construct_params);

	singleton = NAUTILUS_FILE_UNDO_MANAGER (retval);
	g_object_add_weak_pointer (retval, (gpointer) &singleton);

	return retval;
}

static void
nautilus_file_undo_manager_class_init (NautilusFileUndoManagerClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);

	oclass->constructor = nautilus_file_undo_manager_constructor;
	oclass->finalize = nautilus_file_undo_manager_finalize;

	signals[SIGNAL_UNDO_CHANGED] =
		g_signal_new ("undo-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (NautilusFileUndoManagerPrivate));
}

void
nautilus_file_undo_manager_trash_has_emptied (NautilusFileUndoManager *manager)
{
	NautilusFileUndoManagerPrivate *priv;
	NautilusFileUndoData *action;
	guint newest_move_to_trash_position;
	guint i, length;

	priv = manager->priv;

	g_mutex_lock (&priv->mutex);

	/* Clear actions from the oldest to the newest move to trash */
	clear_redo_actions (priv);

	/* Search newest move to trash */
	length = g_queue_get_length (priv->stack);
	newest_move_to_trash_position = -1;
	action = NULL;

	for (i = 0; i < length; i++) {
		action = (NautilusFileUndoData *)g_queue_peek_nth (priv->stack, i);
		if (action->type == NAUTILUS_FILE_UNDO_MOVE_TO_TRASH) {
			newest_move_to_trash_position = i;
			break;
		}
	}

	if (newest_move_to_trash_position >= 0) {
		guint to_clear;
		to_clear = length - newest_move_to_trash_position;
		stack_clear_n_oldest (priv->stack, to_clear);
	}

	g_mutex_unlock (&priv->mutex);
}

guint64
nautilus_file_undo_manager_get_file_modification_time (GFile * file)
{
	GFileInfo *info;
	guint64 mtime;

	/* TODO: Synch-I/O, Error checking. */
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, FALSE, NULL);
	if (info == NULL) {
		return -1;
	}

	mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

	g_object_unref (info);

	return mtime;
}

static void
refresh_strings (NautilusFileUndoData *action)
{
	gchar **descriptions;
	gchar **labels;

	descriptions = (gchar **) g_malloc0 (3 * sizeof (gchar *));
	descriptions[2] = NULL;

	labels = (gchar **) g_malloc0 (3 * sizeof (gchar *));
	labels[2] = NULL;

	action->strings_func (action, action->count,
			      labels, descriptions);

	action->undo_label = labels[0];
	action->redo_label = labels[1];

	action->undo_description = descriptions[0];
	action->redo_description = descriptions[1];

	g_free (descriptions);
	g_free (labels);
}

static const gchar *
get_redo_label (NautilusFileUndoData *action)
{
	if (action->redo_label == NULL) {
		refresh_strings (action);
	}

	return action->redo_label;
}

static const gchar *
get_undo_label (NautilusFileUndoData *action)
{
	if (action->undo_label == NULL) {
		refresh_strings (action);
	}

	return action->undo_label;
}

static const char *
get_undo_description (NautilusFileUndoData *action)
{
	if (action->undo_description == NULL) {
		refresh_strings (action);
	}

	return action->undo_description;
}

static const char *
get_redo_description (NautilusFileUndoData *action)
{
	if (action->redo_description == NULL) {
		refresh_strings (action);
	}

	return action->redo_description;
}

static void
do_undo_redo (NautilusFileUndoManager *self,
	      GtkWindow *parent_window,
	      gboolean undo,
	      NautilusFileUndoFinishCallback callback,
	      gpointer user_data)
{
	NautilusFileUndoManagerPrivate *priv = self->priv;
	NautilusFileUndoData *action;

	/* Update the menus invalidating undo/redo while an operation is already underway */
	g_mutex_lock (&priv->mutex);

	if (undo) {
		action = stack_scroll_right (priv);
	} else {
		action = stack_scroll_left (priv);
	}

	/* Action will be NULL if redo is not possible */
	if (action != NULL) {
		action->locked = TRUE;  /* Remember to unlock when finished */
	}

	g_mutex_unlock (&priv->mutex);

	g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);

	if (action != NULL) {
		priv->undo_redo_flag = TRUE;

		if (undo) {
			action->undo_func (action, parent_window);
		} else {
			action->redo_func (action, parent_window);
		}
	}

	if (callback != NULL) {
		callback (user_data);
	}
}

void
nautilus_file_undo_manager_redo (NautilusFileUndoManager        *manager,
				 GtkWindow                      *parent_window,
                                  NautilusFileUndoFinishCallback  callback,
                                  gpointer                         user_data)
{
	do_undo_redo (manager, parent_window, FALSE, callback, user_data);
}

void
nautilus_file_undo_manager_undo (NautilusFileUndoManager        *manager,
				 GtkWindow                      *parent_window,
				  NautilusFileUndoFinishCallback  callback,
				  gpointer                         user_data)
{
	do_undo_redo (manager, parent_window, TRUE, callback, user_data);
}

void
nautilus_file_undo_manager_add_action (NautilusFileUndoManager    *self,
                                        NautilusFileUndoData *action)
{
	NautilusFileUndoManagerPrivate *priv;

	priv = self->priv;

	DEBUG ("Adding action %p, type %d", action, action->type);

	if (!(action && action->is_valid)) {
		DEBUG ("Action %p is not valid, ignoring", action);
		nautilus_file_undo_data_free (action);
		return;
	}

	action->manager = self;

	g_mutex_lock (&priv->mutex);
	stack_push_action (self, action);
	g_mutex_unlock (&priv->mutex);

	g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

gboolean
nautilus_file_undo_manager_is_undo_redo (NautilusFileUndoManager *manager)
{
	NautilusFileUndoManagerPrivate *priv;

	priv = manager->priv;

	if (priv->undo_redo_flag) {
		priv->undo_redo_flag = FALSE;
		return TRUE;
	}

	return FALSE;
}

NautilusFileUndoManager *
nautilus_file_undo_manager_get (void)
{
	NautilusFileUndoManager *manager;

	manager = g_object_new (NAUTILUS_TYPE_FILE_UNDO_MANAGER,
				NULL);

	return manager;
}

NautilusFileUndoMenuData *
nautilus_file_undo_manager_get_menu_data (NautilusFileUndoManager *self)
{
	NautilusFileUndoData *action;
	NautilusFileUndoManagerPrivate *priv;
	NautilusFileUndoMenuData *data;

	priv = self->priv;
	data = g_slice_new0 (NautilusFileUndoMenuData);

	DEBUG ("Getting menu data");

	g_mutex_lock (&priv->mutex);

	action = get_next_undo_action (priv);

	if (action != NULL) {
		data->undo_label = get_undo_label (action);
		data->undo_description = get_undo_description (action);
	}

	action = get_next_redo_action (priv);

	if (action != NULL) {
		data->redo_label = get_redo_label (action);
		data->redo_description = get_redo_description (action);
	}

	g_mutex_unlock (&priv->mutex);

	return data;
}

void
nautilus_file_undo_menu_data_free (NautilusFileUndoMenuData *data)
{
	g_slice_free (NautilusFileUndoMenuData, data);
}

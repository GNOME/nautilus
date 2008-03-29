/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-progress-info.h: file operation progress info.
 
   Copyright (C) 2007 Red Hat, Inc.
  
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

#include <config.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <eel/eel-string.h>
#include <eel/eel-glib-extensions.h>
#include "nautilus-progress-info.h"

enum {
  CHANGED,
  PROGRESS_CHANGED,
  STARTED,
  FINISHED,
  LAST_SIGNAL
};

/* TODO:
 * Want an icon for the operation.
 * Add and implement cancel button
 */

#define SIGNAL_DELAY_MSEC 100

static guint signals[LAST_SIGNAL] = { 0 };

struct _NautilusProgressInfo
{
	GObject parent_instance;
	
	GCancellable *cancellable;
	
	char *status;
	char *details;
	double progress;
	gboolean activity_mode;
	gboolean started;
	gboolean finished;
	gboolean paused;
	
	GSource *idle_source;
	gboolean source_is_now;
	
	gboolean start_at_idle;
	gboolean finish_at_idle;
	gboolean changed_at_idle;
	gboolean progress_at_idle;
};

struct _NautilusProgressInfoClass
{
	GObjectClass parent_class;
};

static GList *active_progress_infos = NULL;

static GtkStatusIcon *status_icon = NULL;
static int n_progress_ops = 0;


G_LOCK_DEFINE_STATIC(progress_info);

G_DEFINE_TYPE (NautilusProgressInfo, nautilus_progress_info, G_TYPE_OBJECT)

GList *
nautilus_get_all_progress_info (void)
{
	GList *l;
	
	G_LOCK (progress_info);

	l = eel_g_object_list_copy (active_progress_infos);
	
	G_UNLOCK (progress_info);

	return l;
}

static void
nautilus_progress_info_finalize (GObject *object)
{
	NautilusProgressInfo *info;
	
	info = NAUTILUS_PROGRESS_INFO (object);

	g_free (info->status);
	g_free (info->details);
	g_object_unref (info->cancellable);
	
	if (G_OBJECT_CLASS (nautilus_progress_info_parent_class)->finalize) {
		(*G_OBJECT_CLASS (nautilus_progress_info_parent_class)->finalize) (object);
	}
}

static void
nautilus_progress_info_dispose (GObject *object)
{
	NautilusProgressInfo *info;
	
	info = NAUTILUS_PROGRESS_INFO (object);

	G_LOCK (progress_info);

	/* Remove from active list in dispose, since a get_all_progress_info()
	   call later could revive the object */
	active_progress_infos = g_list_remove (active_progress_infos, object);
	
	/* Destroy source in dispose, because the callback
	   could come here before the destroy, which should
	   ressurect the object for a while */
	if (info->idle_source) {
		g_source_destroy (info->idle_source);
		g_source_unref (info->idle_source);
		info->idle_source = NULL;
	}
	G_UNLOCK (progress_info);
}

static void
nautilus_progress_info_class_init (NautilusProgressInfoClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = nautilus_progress_info_finalize;
	gobject_class->dispose = nautilus_progress_info_dispose;
	
	signals[CHANGED] =
		g_signal_new ("changed",
			      NAUTILUS_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[PROGRESS_CHANGED] =
		g_signal_new ("progress-changed",
			      NAUTILUS_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[STARTED] =
		g_signal_new ("started",
			      NAUTILUS_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[FINISHED] =
		g_signal_new ("finished",
			      NAUTILUS_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
}

static gboolean
delete_event (GtkWidget *widget,
	      GdkEventAny *event)
{
	gtk_widget_hide (widget);
	return TRUE;
}

static void
status_icon_activate_cb (GtkStatusIcon *icon,
			 GtkWidget *progress_window)
{
	if (GTK_WIDGET_VISIBLE (progress_window)) {
		gtk_widget_hide (progress_window);
	} else {
		gtk_window_present (GTK_WINDOW (progress_window));
	}
}

static GtkWidget *
get_progress_window (void)
{
	static GtkWidget *progress_window = NULL;
	GtkWidget *vbox;
	
	if (progress_window != NULL) {
		return progress_window;
	}
	
	progress_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable (GTK_WINDOW (progress_window),
				  FALSE);

	gtk_window_set_title (GTK_WINDOW (progress_window),
			      _("File Operations"));
	gtk_window_set_wmclass (GTK_WINDOW (progress_window),
				"file_progress", "Nautilus");
	gtk_window_set_position (GTK_WINDOW (progress_window),
				 GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name (GTK_WINDOW (progress_window),
				"system-file-manager");
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);
	
	gtk_container_add (GTK_CONTAINER (progress_window),
			   vbox);

	g_signal_connect (progress_window, "delete_event", (GCallback)delete_event, NULL);

	status_icon = gtk_status_icon_new_from_icon_name ("system-file-manager");
	g_signal_connect (status_icon, "activate",
			  (GCallback)status_icon_activate_cb,
			  progress_window);

	gtk_status_icon_set_visible (status_icon, FALSE);
	
	return progress_window;
}


typedef struct {
	GtkWidget *widget;
	NautilusProgressInfo *info;
	GtkLabel *status;
	GtkLabel *details;
	GtkProgressBar *progress_bar;
} ProgressWidgetData;

static void
progress_widget_data_free (ProgressWidgetData *data)
{
	g_object_unref (data->info);
	g_free (data);
}

static void
update_data (ProgressWidgetData *data)
{
	char *status, *details;
	char *markup;

	status = nautilus_progress_info_get_status (data->info);
	gtk_label_set_text (data->status, status);
	g_free (status);

	details = nautilus_progress_info_get_details (data->info);
	markup = g_markup_printf_escaped ("<span size='small'>%s</span>", details);
	gtk_label_set_markup (data->details, markup);
	g_free (details);
	g_free (markup);
}

static void
update_progress (ProgressWidgetData *data)
{
	double progress;

	progress = nautilus_progress_info_get_progress (data->info);
	if (progress < 0) {
		gtk_progress_bar_pulse (data->progress_bar);
	} else {
		gtk_progress_bar_set_fraction (data->progress_bar, progress);
	}
}

static void
update_status_icon_and_window (void)
{
	char *tooltip;

	tooltip = g_strdup_printf (ngettext ("%'d file operation active",
					     "%'d file operations active",
					     n_progress_ops),
				   n_progress_ops);
	gtk_status_icon_set_tooltip (status_icon, tooltip);
	g_free (tooltip);
	
	if (n_progress_ops == 0) {
		gtk_status_icon_set_visible (status_icon, FALSE);
		gtk_widget_hide (get_progress_window ());
	} else {
		gtk_status_icon_set_visible (status_icon, TRUE);
	}
}

static void
op_finished (ProgressWidgetData *data)
{
	gtk_widget_destroy (data->widget);
	
	n_progress_ops--;
	update_status_icon_and_window ();
}

static void
cancel_clicked (GtkWidget *button,
		ProgressWidgetData *data)
{
	nautilus_progress_info_cancel (data->info);
	gtk_widget_set_sensitive (button, FALSE);
}


static GtkWidget *
progress_widget_new (NautilusProgressInfo *info)
{
	ProgressWidgetData *data;
	GtkWidget *label, *bar, *hbox, *vbox, *button, *image;

	data = g_new0 (ProgressWidgetData, 1);
	data->info = g_object_ref (info);
	
	hbox = gtk_hbox_new (FALSE, 0);
	data->widget = hbox;
	g_object_set_data_full (G_OBJECT (data->widget),
				"data", data, (GDestroyNotify)progress_widget_data_free);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox,
			    FALSE, FALSE,
			    2);
	gtk_widget_show (vbox);
	
	label = gtk_label_new ("details");
	gtk_widget_set_size_request (label,
				     400, -1);
	data->status = GTK_LABEL (label);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox),
			    label,
			    FALSE, FALSE,
			    2);
	
	bar = gtk_progress_bar_new ();
	data->progress_bar = GTK_PROGRESS_BAR (bar);
	gtk_progress_bar_set_pulse_step (data->progress_bar, 0.05);
	
	gtk_widget_show (bar);
	gtk_box_pack_start (GTK_BOX (vbox),
			    bar,
			    FALSE, FALSE,
			    2);
	
	label = gtk_label_new ("status");
	gtk_widget_set_size_request (label,
				     400, -1);
	data->details = GTK_LABEL (label);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox),
			    label,
			    FALSE, FALSE,
			    0);


	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox,
			    FALSE, FALSE,
			    0);
	gtk_widget_show (vbox);

	image = gtk_image_new_from_stock (GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), image);
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (vbox),
			    button,
			    TRUE, FALSE,
			    0);

	g_signal_connect (button, "clicked", (GCallback)cancel_clicked, data);

	
	update_data (data);
	update_progress (data);

	g_signal_connect_swapped (data->info, "changed", (GCallback)update_data, data);
	g_signal_connect_swapped (data->info, "progress_changed", (GCallback)update_progress, data);
	g_signal_connect_swapped (data->info, "finished", (GCallback)op_finished, data);
	
	gtk_widget_show (data->widget);

	return data->widget;
}

static void
handle_new_progress_info (NautilusProgressInfo *info)
{
	GtkWidget *window, *progress;

	window = get_progress_window ();
	
	progress = progress_widget_new (info);
	gtk_box_pack_start (GTK_BOX (GTK_BIN (window)->child),
			    progress,
			    FALSE, FALSE, 6);

	gtk_window_present (GTK_WINDOW (window));

	n_progress_ops++;
	update_status_icon_and_window ();	
}

static gboolean
new_op_started_timeout (NautilusProgressInfo *info)
{
	if (nautilus_progress_info_get_is_paused (info)) {
		return TRUE;
	}
	if (!nautilus_progress_info_get_is_finished (info)) {
		handle_new_progress_info (info);
	}
	g_object_unref (info);
	return FALSE;
}

static void
new_op_started (NautilusProgressInfo *info)
{
	g_signal_handlers_disconnect_by_func (info, (GCallback)new_op_started, NULL);
	g_timeout_add_seconds (2,
			       (GSourceFunc)new_op_started_timeout,
			       g_object_ref (info));
}

static void
nautilus_progress_info_init (NautilusProgressInfo *info)
{
	info->cancellable = g_cancellable_new ();

	G_LOCK (progress_info);
	active_progress_infos = g_list_append (active_progress_infos, info);
	G_UNLOCK (progress_info);

	g_signal_connect (info, "started", (GCallback)new_op_started, NULL);
}

NautilusProgressInfo *
nautilus_progress_info_new (void)
{
	NautilusProgressInfo *info;
	
	info = g_object_new (NAUTILUS_TYPE_PROGRESS_INFO, NULL);
	
	return info;
}

char *
nautilus_progress_info_get_status (NautilusProgressInfo *info)
{
	char *res;
	
	G_LOCK (progress_info);
	
	if (info->status) {
		res = g_strdup (info->status);
	} else {
		res = g_strdup (_("Preparing"));
	}
	
	G_UNLOCK (progress_info);
	
	return res;
}

char *
nautilus_progress_info_get_details (NautilusProgressInfo *info)
{
	char *res;
	
	G_LOCK (progress_info);
	
	if (info->details) {
		res = g_strdup (info->details);
	} else {
		res = g_strdup (_("Preparing"));
	}
	
	G_UNLOCK (progress_info);

	return res;
}

double
nautilus_progress_info_get_progress (NautilusProgressInfo *info)
{
	double res;
	
	G_LOCK (progress_info);

	if (info->activity_mode) {
		res = -1.0;
	} else {
		res = info->progress;
	}
	
	G_UNLOCK (progress_info);
	
	return res;
}

void
nautilus_progress_info_cancel (NautilusProgressInfo *info)
{
	G_LOCK (progress_info);
	
	g_cancellable_cancel (info->cancellable);
	
	G_UNLOCK (progress_info);
}

GCancellable *
nautilus_progress_info_get_cancellable (NautilusProgressInfo *info)
{
	GCancellable *c;
	
	G_LOCK (progress_info);
	
	c = g_object_ref (info->cancellable);
	
	G_UNLOCK (progress_info);
	
	return c;
}

gboolean
nautilus_progress_info_get_is_started (NautilusProgressInfo *info)
{
	gboolean res;
	
	G_LOCK (progress_info);
	
	res = info->started;
	
	G_UNLOCK (progress_info);
	
	return res;
}

gboolean
nautilus_progress_info_get_is_finished (NautilusProgressInfo *info)
{
	gboolean res;
	
	G_LOCK (progress_info);
	
	res = info->finished;
	
	G_UNLOCK (progress_info);
	
	return res;
}

gboolean
nautilus_progress_info_get_is_paused (NautilusProgressInfo *info)
{
	gboolean res;
	
	G_LOCK (progress_info);
	
	res = info->paused;
	
	G_UNLOCK (progress_info);
	
	return res;
}

static gboolean
idle_callback (gpointer data)
{
	NautilusProgressInfo *info = data;
	gboolean start_at_idle;
	gboolean finish_at_idle;
	gboolean changed_at_idle;
	gboolean progress_at_idle;
	GSource *source;

	source = g_main_current_source ();
	
	G_LOCK (progress_info);

	/* Protect agains races where the source has
	   been destroyed on another thread while it
	   was being dispatched.
	   Similar to what gdk_threads_add_idle does.
	*/
	if (g_source_is_destroyed (source)) {
		G_UNLOCK (progress_info);
		return FALSE;
	}

	/* We hadn't destroyed the source, so take a ref.
	 * This might ressurect the object from dispose, but
	 * that should be ok.
	 */
	g_object_ref (info);

	g_assert (source == info->idle_source);
	
	g_source_unref (source);
	info->idle_source = NULL;
	
	start_at_idle = info->start_at_idle;
	finish_at_idle = info->finish_at_idle;
	changed_at_idle = info->changed_at_idle;
	progress_at_idle = info->progress_at_idle;
	
	info->start_at_idle = FALSE;
	info->finish_at_idle = FALSE;
	info->changed_at_idle = FALSE;
	info->progress_at_idle = FALSE;
	
	G_UNLOCK (progress_info);
	
	if (start_at_idle) {
		g_signal_emit (info,
			       signals[STARTED],
			       0);
	}
	
	if (changed_at_idle) {
		g_signal_emit (info,
			       signals[CHANGED],
			       0);
	}
	
	if (progress_at_idle) {
		g_signal_emit (info,
			       signals[PROGRESS_CHANGED],
			       0);
	}
	
	if (finish_at_idle) {
		g_signal_emit (info,
			       signals[FINISHED],
			       0);
	}
	
	g_object_unref (info);
	
	return FALSE;
}

/* Called with lock held */
static void
queue_idle (NautilusProgressInfo *info, gboolean now)
{
	if (info->idle_source == NULL ||
	    (now && !info->source_is_now)) {
		if (info->idle_source) {
			g_source_destroy (info->idle_source);
			g_source_unref (info->idle_source);
			info->idle_source = NULL;
		}
		
		info->source_is_now = now;
		if (now) {
			info->idle_source = g_idle_source_new ();
		} else {
			info->idle_source = g_timeout_source_new (SIGNAL_DELAY_MSEC);
		}
		g_source_set_callback (info->idle_source, idle_callback, info, NULL);
		g_source_attach (info->idle_source, NULL);
	}
}

void
nautilus_progress_info_pause (NautilusProgressInfo *info)
{
	G_LOCK (progress_info);

	if (!info->paused) {
		info->paused = TRUE;
	}

	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_resume (NautilusProgressInfo *info)
{
	G_LOCK (progress_info);

	if (info->paused) {
		info->paused = FALSE;
	}

	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_start (NautilusProgressInfo *info)
{
	G_LOCK (progress_info);
	
	if (!info->started) {
		info->started = TRUE;
		
		info->start_at_idle = TRUE;
		queue_idle (info, TRUE);
	}
	
	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_finish (NautilusProgressInfo *info)
{
	G_LOCK (progress_info);
	
	if (!info->finished) {
		info->finished = TRUE;
		
		info->finish_at_idle = TRUE;
		queue_idle (info, TRUE);
	}
	
	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_take_status (NautilusProgressInfo *info,
				    char *status)
{
	G_LOCK (progress_info);
	
	if (eel_strcmp (info->status, status) != 0) {
		g_free (info->status);
		info->status = status;
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	} else {
		g_free (status);
	}
	
	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_status (NautilusProgressInfo *info,
				   const char *status)
{
	G_LOCK (progress_info);
	
	if (eel_strcmp (info->status, status) != 0) {
		g_free (info->status);
		info->status = g_strdup (status);
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
	
	G_UNLOCK (progress_info);
}


void
nautilus_progress_info_take_details (NautilusProgressInfo *info,
				     char           *details)
{
	G_LOCK (progress_info);
	
	if (eel_strcmp (info->details, details) != 0) {
		g_free (info->details);
		info->details = details;
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	} else {
		g_free (details);
	}
  
	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_details (NautilusProgressInfo *info,
				    const char           *details)
{
	G_LOCK (progress_info);
	
	if (eel_strcmp (info->details, details) != 0) {
		g_free (info->details);
		info->details = g_strdup (details);
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
  
	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_pulse_progress (NautilusProgressInfo *info)
{
	G_LOCK (progress_info);

	info->activity_mode = TRUE;
	info->progress = 0.0;
	info->progress_at_idle = TRUE;
	queue_idle (info, FALSE);
	
	G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_progress (NautilusProgressInfo *info,
				     double                current,
				     double                total)
{
	double current_percent;
	
	if (total <= 0) {
		current_percent = 1.0;
	} else {
		current_percent = current / total;

		if (current_percent < 0) {
			current_percent	= 0;
		}
		
		if (current_percent > 1.0) {
			current_percent	= 1.0;
		}
	}
	
	G_LOCK (progress_info);
	
	if (info->activity_mode || /* emit on switch from activity mode */
	    fabs (current_percent - info->progress) > 0.005 /* Emit on change of 0.5 percent */
	    ) {
		info->activity_mode = FALSE;
		info->progress = current_percent;
		info->progress_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
	
	G_UNLOCK (progress_info);
}

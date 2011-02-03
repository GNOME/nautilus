/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nautilus-progress-ui-handler.c: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
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
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-progress-ui-handler.h"

#include "nautilus-application.h"
#include "nautilus-progress-info-widget.h"

#include <libnautilus-private/nautilus-progress-info.h>
#include <libnautilus-private/nautilus-progress-info-manager.h>

static GtkStatusIcon *status_icon = NULL;
static int n_progress_ops = 0;

struct _NautilusProgressUIHandlerPriv {
	NautilusProgressInfoManager *manager;

	GtkWidget *progress_window;
};

G_DEFINE_TYPE (NautilusProgressUIHandler, nautilus_progress_ui_handler, G_TYPE_OBJECT);

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
	if (gtk_widget_get_visible (progress_window)) {
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
	GIcon *icon;
	
	if (progress_window != NULL) {
		return progress_window;
	}
	
	progress_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable (GTK_WINDOW (progress_window),
				  FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (progress_window), 10);
 
	gtk_window_set_title (GTK_WINDOW (progress_window),
			      _("File Operations"));
	gtk_window_set_wmclass (GTK_WINDOW (progress_window),
				"file_progress", "Nautilus");
	gtk_window_set_position (GTK_WINDOW (progress_window),
				 GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name (GTK_WINDOW (progress_window),
				"system-file-manager");

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_set_spacing (GTK_BOX (vbox), 5);
		
	gtk_container_add (GTK_CONTAINER (progress_window),
			   vbox);

	gtk_widget_show_all (progress_window);

	g_signal_connect (progress_window,
			  "delete_event",
			  (GCallback)delete_event, NULL);

	icon = g_themed_icon_new_with_default_fallbacks ("system-file-manager-symbolic");
	status_icon = gtk_status_icon_new_from_gicon (icon);
	g_signal_connect (status_icon, "activate",
			  (GCallback)status_icon_activate_cb,
			  progress_window);

	gtk_status_icon_set_visible (status_icon, FALSE);
	g_object_unref (icon);

	return progress_window;
}

static void
update_status_icon_and_window (void)
{
	char *tooltip;

	tooltip = g_strdup_printf (ngettext ("%'d file operation active",
					     "%'d file operations active",
					     n_progress_ops),
				   n_progress_ops);
	gtk_status_icon_set_tooltip_text (status_icon, tooltip);
	g_free (tooltip);
	
	if (n_progress_ops == 0) {
		gtk_status_icon_set_visible (status_icon, FALSE);
		gtk_widget_hide (get_progress_window ());
	} else {
		gtk_status_icon_set_visible (status_icon, TRUE);
	}
}

static void
handle_new_progress_info (NautilusProgressInfo *info)
{
	GtkWidget *window, *progress;

	window = get_progress_window ();
	
	progress = nautilus_progress_info_widget_new (info);
	gtk_box_pack_start (GTK_BOX (gtk_bin_get_child (GTK_BIN (window))),
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
progress_info_finished_cb (NautilusProgressInfo *info,
			   NautilusProgressUIHandler *self)
{
	NautilusApplication *app;

	app = nautilus_application_dup_singleton ();
	g_application_release (G_APPLICATION (app));

	g_object_unref (app);

	n_progress_ops--;
	update_status_icon_and_window ();
}

static void
progress_info_started_cb (NautilusProgressInfo *info,
			  NautilusProgressUIHandler *self)
{
	NautilusApplication *app;

	app = nautilus_application_dup_singleton ();
	g_application_hold (G_APPLICATION (app));

	g_signal_connect (info, "finished",
			  G_CALLBACK (progress_info_finished_cb), self);

	g_object_unref (app);

	/* timeout for the progress window to appear */
	g_timeout_add_seconds (2,
			       (GSourceFunc) new_op_started_timeout,
			       g_object_ref (info));
}

static void
new_progress_info_cb (NautilusProgressInfoManager *manager,
		      NautilusProgressInfo *info,
		      NautilusProgressUIHandler *self)
{
	g_signal_connect (info, "started",
			  G_CALLBACK (progress_info_started_cb), self);
}

static void
nautilus_progress_ui_handler_dispose (GObject *obj)
{
	NautilusProgressUIHandler *self = NAUTILUS_PROGRESS_UI_HANDLER (obj);

	g_clear_object (&self->priv->manager);

	G_OBJECT_CLASS (nautilus_progress_ui_handler_parent_class)->dispose (obj);
}

static void
nautilus_progress_ui_handler_init (NautilusProgressUIHandler *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_PROGRESS_UI_HANDLER,
						  NautilusProgressUIHandlerPriv);

	self->priv->manager = nautilus_progress_info_manager_new ();

	g_signal_connect (self->priv->manager, "new-progress-info",
			  G_CALLBACK (new_progress_info_cb), self);
}

static void
nautilus_progress_ui_handler_class_init (NautilusProgressUIHandlerClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->dispose = nautilus_progress_ui_handler_dispose;
	
	g_type_class_add_private (klass, sizeof (NautilusProgressUIHandlerPriv));
}

NautilusProgressUIHandler *
nautilus_progress_ui_handler_new (void)
{
	return g_object_new (NAUTILUS_TYPE_PROGRESS_UI_HANDLER, NULL);
}

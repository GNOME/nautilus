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

#include <libnautilus-private/nautilus-progress-info.h>
#include <libnautilus-private/nautilus-progress-info-manager.h>

struct _NautilusProgressUIHandlerPriv {
	NautilusProgressInfoManager *manager;
};

G_DEFINE_TYPE (NautilusProgressUIHandler, nautilus_progress_ui_handler, G_TYPE_OBJECT);

static void
progress_info_finished_cb (NautilusProgressInfo *info,
			   NautilusProgressUIHandler *self)
{
	NautilusApplication *app;

	app = nautilus_application_dup_singleton ();
	g_application_release (G_APPLICATION (app));

	g_object_unref (app);
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

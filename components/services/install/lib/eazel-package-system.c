/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include "eazel-package-system-private.h"

enum {
	START,
	END,
	PROGRESS,
	FAILED,
	LAST_SIGNAL
};

/* The signal array, used for building the signal bindings */
static guint signals[LAST_SIGNAL] = { 0 };
/* This is the parent class pointer */
static GtkObjectClass *eazel_package_system_parent_class;

/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_finalize (GtkObject *object)
{
	EazelPackageSystem *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM (object));

	system = EAZEL_PACKAGE_SYSTEM (object);

	if (GTK_OBJECT_CLASS (eazel_package_system_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_parent_class)->finalize (object);
	}
}

void eazel_package_system_unref (GtkObject *object) 
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM (object));
	gtk_object_unref (object);
}

static void
eazel_package_system_class_initialize (EazelPackageSystemClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_finalize;
	object_class->set_arg = eazel_package_system_set_arg;
	
	eazel_package_system_parent_class = gtk_type_class (gtk_object_get_type ());

	signals[START] = 
		gtk_signal_new ("start",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, download_progress),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);	
	signals[END] = 
		gtk_signal_new ("end",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, preflight_check),
				gtk_marshal_BOOL__POINTER_INT_INT,
				GTK_TYPE_BOOL, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[PROGRESS] = 
		gtk_signal_new ("progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, install_progress),
				eazel_package_system_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[FAILED] = 
		gtk_signal_new ("failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelPackageSystemClass, download_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	klass->start = NULL;
	klass->progress = NULL;
	klass->failed = NULL;
	klass->end = NULL;
}

static void
eazel_package_system_initialize (EazelPackageSystem *system) {
	g_assert (system != NULL);
	g_assert (IS_EAZEL_PACKAGE_SYSTEM (system));
}

GtkType
eazel_package_system_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystem",
			sizeof (EazelPackageSystem),
			sizeof (EazelPackageSystemClass),
			(GtkClassInitFunc) eazel_package_system_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (gtk_object_get_type (), &system_info);
	}

	return system_type;
}

EazelPackageSystem *
eazel_package_system_new (void)
{
	EazelPackageSystem *system;

	system = EAZEL_PACKAGE_SYSTEM (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM, NULL));
	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));
	
	return system;
}

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
#include "eazel-softcat.h"

#include "eazel-softcat-private.h"

/* This is the parent class pointer */
static GtkObjectClass *eazel_softcat_parent_class;

/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_softcat_finalize (GtkObject *object)
{
	EazelSoftCat *softcat;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_SOFTCAT (object));

	softcat = EAZEL_SOFTCAT (object);

	if (GTK_OBJECT_CLASS (eazel_softcat_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_softcat_parent_class)->finalize (object);
	}
}

static void
eazel_softcat_class_initialize (EazelSoftCatClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_softcat_finalize;
#if 0
	object_class->set_arg = eazel_softcat_set_arg;
#endif
	
	eazel_softcat_parent_class = gtk_type_class (gtk_object_get_type ());

#if 0
	signals[START] = 
		gtk_signal_new ("start",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, start),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);	
	signals[END] = 
		gtk_signal_new ("end",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, end),
				gtk_marshal_BOOL__POINTER_INT_INT,
				GTK_TYPE_BOOL, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[PROGRESS] = 
		gtk_signal_new ("progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, progress),
				eazel_softcat_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[FAILED] = 
		gtk_signal_new ("failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelSoftCatClass, failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
#endif
}

static void
eazel_softcat_initialize (EazelSoftCat *softcat) {
	g_assert (softcat != NULL);
	g_assert (IS_EAZEL_SOFTCAT (softcat));
}

GtkType
eazel_softcat_get_type() {
	static GtkType softcat_type = 0;

	/* First time it's called ? */
	if (!softcat_type)
	{
		static const GtkTypeInfo softcat_info =
		{
			"EazelSoftCat",
			sizeof (EazelSoftCat),
			sizeof (EazelSoftCatClass),
			(GtkClassInitFunc) eazel_softcat_class_initialize,
			(GtkObjectInitFunc) eazel_softcat_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		softcat_type = gtk_type_unique (gtk_object_get_type (), &softcat_info);
	}

	return softcat_type;
}

EazelSoftCat *
eazel_softcat_new (void)
{
	EazelSoftCat *softcat;

	softcat = EAZEL_SOFTCAT (gtk_object_new (TYPE_EAZEL_SOFTCAT, NULL));
	gtk_object_ref (GTK_OBJECT (softcat));
	gtk_object_sink (GTK_OBJECT (softcat));
	
	return softcat;
}

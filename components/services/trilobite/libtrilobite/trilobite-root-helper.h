/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * GtkObject definition for TrilobiteRootHelper.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Robey Pointer <robey@eazel.com>
 *
 */

#ifndef _TRILOBITE_ROOT_HELPER_H_
#define _TRILOBITE_ROOT_HELPER_H_

/* #include <libgnome/gnome-defs.h> */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TRILOBITE_TYPE_ROOT_HELPER		(trilobite_root_helper_get_type ())
#define TRILOBITE_ROOT_HELPER(obj)		(GTK_CHECK_CAST ((obj), TRILOBITE_TYPE_ROOT_HELPER, \
							TrilobiteRootHelper))
#define TRILOBITE_ROOT_HELPER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), TRILOBITE_TYPE_ROOT_HELPER, \
							TrilobiteRootHelperClass))
#define TRILOBITE_IS_ROOT_HELPER(obj)		(GTK_CHECK_TYPE ((obj), TRILOBITE_TYPE_ROOT_HELPER))
#define TRILOBITE_IS_ROOT_HELPER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), TRILOBITE_TYPE_ROOT_HELPER))

typedef struct _TrilobiteRootHelper TrilobiteRootHelper;
typedef struct _TrilobiteRootHelperClass TrilobiteRootHelperClass;

/* used only internally */
typedef enum {
	TRILOBITE_ROOT_HELPER_STATE_NEW = 0,
	TRILOBITE_ROOT_HELPER_STATE_CONNECTED,
	TRILOBITE_ROOT_HELPER_STATE_PIPE	/* gave stdout pipe to caller */
} TrilobiteRootHelperState;

typedef enum {
	TRILOBITE_ROOT_HELPER_SUCCESS = 0,
	TRILOBITE_ROOT_HELPER_NO_USERHELPER,	/* couldn't fork/exec the "userhelper" utility */
	TRILOBITE_ROOT_HELPER_NEED_PASSWORD,	/* no signal handler supplied a root password */
	TRILOBITE_ROOT_HELPER_BAD_PASSWORD,	/* password was supplied, but it was wrong */
	TRILOBITE_ROOT_HELPER_LOST_PIPE,	/* "userhelper" pipe closed prematurely */
	TRILOBITE_ROOT_HELPER_BAD_ARGS,		/* arguments to trilobite_root_helper_run() were bad */
	TRILOBITE_ROOT_HELPER_BAD_COMMAND,	/* command unknown to TrilobiteRootHelper */
	TRILOBITE_ROOT_HELPER_INTERNAL_ERROR	/* something weird went wrong */
} TrilobiteRootHelperStatus;

/* commands that can be sent to the root helper, once it's running */
typedef enum {
	TRILOBITE_ROOT_HELPER_RUN_RPM = 1,	/* argv: args to rpm -- fd: pipe from rpm */
	TRILOBITE_ROOT_HELPER_RUN_SET_TIME,	/* argv: list of 1 string containing a time_t -- fd: unused */
	TRILOBITE_ROOT_HELPER_RUN_LS = 23	/* argv: args to ls -- fd: pipe from ls  [DEMO] */
} TrilobiteRootHelperCommand;

struct _TrilobiteRootHelperClass
{
	GtkObjectClass parent_class;

	gchar *(*need_password) (TrilobiteRootHelper *helper);
};

struct _TrilobiteRootHelper
{
	GtkObject parent;
	TrilobiteRootHelperState state;
	int pipe_stdin;		/* pipe to/from the eazel-helper utility */
	int pipe_stdout;
};


#define TRILOBITE_ROOT_HELPER_IS_CONNECTED(obj)	(TRILOBITE_ROOT_HELPER (obj)->state == \
						TRILOBITE_ROOT_HELPER_STATE_CONNECTED)


GtkType trilobite_root_helper_get_type (void);
TrilobiteRootHelper *trilobite_root_helper_new (void);
void trilobite_root_helper_destroy (GtkObject *object);

TrilobiteRootHelperStatus trilobite_root_helper_start (TrilobiteRootHelper *root_helper);
TrilobiteRootHelperStatus trilobite_root_helper_run (TrilobiteRootHelper *root_helper,
						     TrilobiteRootHelperCommand command, GList *argv, int *fd);
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif	/* _TRILOBITE_ROOT_HELPER_H_ */

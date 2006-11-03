/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* nautilus-shell.h: Server side of Nautilus:Shell CORBA object that
 * represents the shell across processes.
 */

#ifndef NAUTILUS_SHELL_H
#define NAUTILUS_SHELL_H

#include "nautilus-application.h"
#include "nautilus-shell-interface.h"

#define NAUTILUS_TYPE_SHELL	       (nautilus_shell_get_type ())
#define NAUTILUS_SHELL(obj)	       (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SHELL, NautilusShell))
#define NAUTILUS_SHELL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SHELL, NautilusShellClass))
#define NAUTILUS_IS_SHELL(obj)	       (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SHELL))
#define NAUTILUS_IS_SHELL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SHELL))

typedef struct NautilusShellDetails NautilusShellDetails;

struct NautilusShell {
	BonoboObject parent_slot;
	NautilusShellDetails *details;
};

typedef struct {
	BonoboObjectClass parent_slot;
	POA_Nautilus_Shell__epv epv;
} NautilusShellClass;

GType          nautilus_shell_get_type (void);
NautilusShell *nautilus_shell_new      (NautilusApplication *application);

#endif /* NAUTILUS_SHELL_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the rpm verify window dialog
 *
 */

#ifndef NAUTILUS_RPM_VERIFY_WINDOW_H
#define NAUTILUS_RPM_VERIFY_WINDOW_H

#include <gdk/gdk.h>
#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_RPM_VERIFY_WINDOW		(nautilus_rpm_verify_window_get_type ())
#define NAUTILUS_RPM_VERIFY_WINDOW(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_RPM_VERIFY_WINDOW, NautilusRPMVerifyWindow))
#define NAUTILUS_RPM_VERIFY_WINDOW_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_RPM_VERIFY_WINDOW, NautilusRPMVerifyWindowClass))
#define NAUTILUS_IS_RPM_VERIFY_WINDOW(obj)	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_RPM_VERIFY_WINDOW))
#define NAUTILUS_IS_RPM_VERIFY_WINDOW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_RPM_VERIFY_WINDOW))

typedef struct NautilusRPMVerifyWindow NautilusRPMVerifyWindow;
typedef struct NautilusRPMVerifyWindowClass NautilusRPMVerifyWindowClass;
typedef struct NautilusRPMVerifyWindowDetails NautilusRPMVerifyWindowDetails;

struct NautilusRPMVerifyWindow {
	GnomeDialog parent;
	NautilusRPMVerifyWindowDetails *details;
};

struct NautilusRPMVerifyWindowClass {
	GnomeDialogClass parent_class;
};

GtkType		nautilus_rpm_verify_window_get_type	(void);
GtkWidget*	nautilus_rpm_verify_window_new	(const char *package_name);
void		nautilus_rpm_verify_window_set_message (NautilusRPMVerifyWindow *window, const char *message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NAUTILUS_RPM_VERIFY_WINDOW_H */

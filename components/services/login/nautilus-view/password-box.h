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
 * Authors: Robey Pointer <robey@eazel.com>
 */

#ifndef _PASSWORD_BOX_H_
#define _PASSWORD_BOX_H_

#include <gnome.h>

typedef struct {
	GtkWidget *hbox;
	GtkWidget *vbox_left;
	GtkWidget *hbox_left;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *hbox_right;
	GtkWidget *alignment;
	GtkWidget *vbox_bong;
	GtkWidget *bong;
	GtkWidget *line;
	GtkWidget *label_right;
	GtkWidget *viewport;
	GtkWidget *table;
} PasswordBox;

/* colors to use when happy */
#define BUBBLE_BACKGROUND_RGB		0xDCDCDC
#define BUBBLE_FOREGROUND_RGB		0x000000

/* colors to use when sad */
#define BUBBLE_BACKGROUND_ALERT_RGB	0xCC6666
#define BUBBLE_FOREGROUND_ALERT_RGB	0xFFFFFF

/* icon filenames */
#define TINY_ALERT_ICON_FILENAME	"tiny-alert.png"
#define BUBBLE_UL_FILENAME		"bubble-UL.png"
#define BUBBLE_UR_FILENAME		"bubble-UR.png"
#define BUBBLE_LL_FILENAME		"bubble-LL.png"
#define BUBBLE_LR_FILENAME		"bubble-LR.png"


PasswordBox *password_box_new (const char *title);
void password_box_set_colors (PasswordBox *box, int foreground, int background);
GtkWidget *password_box_get_entry (PasswordBox *box);
void password_box_set_error_text (PasswordBox *box, const char *message);
void password_box_show_error (PasswordBox *box, gboolean show_it);


#endif	/* _PASSWORD_BOX_H_ */

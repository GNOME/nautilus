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
 * Authors: J Shane Culpepper
 */

#ifndef NAUTILUS_SUMMARY_DIALOGS_H
#define NAUTILUS_SUMMARY_DIALOGS_H

void	nautilus_summary_show_login_failure_dialog (NautilusSummaryView *view,
						    const char 		*message);

void	nautilus_summary_show_error_dialog         (NautilusSummaryView	*view,
						    const char		*message);
void	nautilus_summary_show_login_dialog         (NautilusSummaryView	*view);
void	widget_set_eel_background_color	   (GtkWidget		*widget,
						    const char		*color);

#endif /* NAUTILUS_SUMMARY_DIALOGS_H */


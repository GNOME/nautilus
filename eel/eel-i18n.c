/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-i18n.c:  I18n stuff for Eel.

   Copyright (C) 2002 MandrakeSoft.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Frederic Crozat <fcrozat@mandrakesoft.com>
*/

#include <config.h>
#include <glib.h>
#include "eel-i18n.h"

#ifdef ENABLE_NLS

#include <libintl.h>



G_CONST_RETURN char *
_eel_gettext (const char *str)
{
	static gboolean _eel_gettext_initialized = FALSE;

	if (!_eel_gettext_initialized) {
		bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
#    ifdef HAVE_BIND_TEXTDOMAIN_CODESET
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#    endif
		_eel_gettext_initialized = TRUE;
	}

	return dgettext (GETTEXT_PACKAGE, str);
}

#endif /* ENABLE_NLS */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/*
 * nautilus-mozilla-encoding-tables.c - Tables for obtaining translated
 * labels for Mozilla charset decoders and charset groups.
 */

#include <config.h>

#include "nautilus-mozilla-encoding-tables.h"
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

static const char *encoding_groups_table[] =
{
	N_("Arabic"),
	N_("Baltic"),
	N_("Central European"),
	N_("Chinese"),
	N_("Cyrillic"),
	N_("Greek"),
	N_("Hebrew"),
	N_("Japanese"),
	N_("Turkish"),
	N_("Unicode"),
	N_("UTF"),
	N_("Vietnamese"),
	N_("Western")
};

static const char *encoding_table[] =
{
	N_("Arabic (IBM-864)"),
	N_("Arabic (ISO-8859-6)"),
	N_("Arabic (ISO-8859-6-E)"),
	N_("Arabic (ISO-8859-6-I)"),
	N_("Arabic (Windows-1256)"),
	N_("Armenian (ARMSCII-8)"),
	N_("Baltic (ISO-8859-13)"),
	N_("Baltic (ISO-8859-4)"),
	N_("Baltic (Windows-1257)"),
	N_("Celtic (ISO-8859-14)"),
	N_("Central European (IBM-852)"),
	N_("Central European (ISO-8859-2)"),
	N_("Central European (MacCE)"),
	N_("Central European (Windows-1250)"),
	N_("Chinese Simplified (GB2312)"),
	N_("Chinese Simplified (GBK)"),
	N_("Chinese Simplified (HZ)"),
	N_("Chinese Traditional (Big5)"),
	N_("Chinese Traditional (EUC-TW)"),
	N_("Croatian (MacCroatian)"),
	N_("Cyrillic (IBM-855)"),
	N_("Cyrillic (ISO-8859-5)"),
	N_("Cyrillic (ISO-IR-111)"),
	N_("Cyrillic (MacCyrillic)"),
	N_("Cyrillic (Windows-1251)"),
	N_("Cyrillic/Russian (IBM-866)"),
	N_("Cyrillic/Russian (KOI8-R)"),
	N_("Cyrillic/Ukrainian (KOI8-U)"),
	N_("Cyrillic/Ukrainian (MacUkrainian)"),
	N_("English (US-ASCII)"),
	N_("Greek (ISO-8859-7)"),
	N_("Greek (MacGreek)"),
	N_("Greek (Windows-1253)"),
	N_("Hebrew (IBM-862)"),
	N_("Hebrew (ISO-8859-8-E)"),
	N_("Hebrew (ISO-8859-8-I)"),
	N_("Hebrew (Windows-1255)"),
	N_("Icelandic (MacIcelandic)"),
	N_("Japanese (EUC-JP)"),
	N_("Japanese (ISO-2022-JP)"),
	N_("Japanese (Shift_JIS)"),
	N_("Korean (EUC-KR)"),
	N_("Nordic (ISO-8859-10)"),
	N_("Romanian (MacRomanian)"),
	N_("South European (ISO-8859-3)"),
	N_("T.61-8bit"),
	N_("Thai (TIS-620)"),
	N_("Turkish (IBM-857)"),
	N_("Turkish (ISO-8859-9)"),
	N_("Turkish (MacTurkish)"),
	N_("Turkish (Windows-1254)"),
	N_("Unicode (UTF-7)"),
	N_("Unicode (UTF-8)"),
	N_("User Defined"),
	N_("UTF-16BE"),
	N_("UTF-16LE"),
	N_("UTF-32BE"),
	N_("UTF-32LE"),
	N_("Vietnamese (TCVN)"),
	N_("Vietnamese (VISCII)"),
	N_("Vietnamese (VPS)"),
	N_("Vietnamese (Windows-1258)"),
	N_("Visual Hebrew (ISO-8859-8)"),
	N_("Western (IBM-850)"),
	N_("Western (ISO-8859-1)"),
	N_("Western (ISO-8859-15)"),
	N_("Western (MacRoman)"),
	N_("Western (Windows-1252)"),
	N_("windows-936"),
	N_("x-imap4-modified-utf7"),
	N_("x-u-escaped")
};

guint
mozilla_encoding_groups_table_get_count (void)
{
	return sizeof (encoding_groups_table) / sizeof ((encoding_groups_table)[0]);
}

const char *
mozilla_encoding_groups_table_peek_nth (guint n)
{
	g_return_val_if_fail (n < mozilla_encoding_groups_table_get_count (), NULL);

	return encoding_groups_table[n];
}

const char *
mozilla_encoding_groups_table_peek_nth_translated (guint n)
{
	g_return_val_if_fail (n < mozilla_encoding_groups_table_get_count (), NULL);

	return gettext (encoding_groups_table[n]);
}

const char *
mozilla_encoding_groups_table_find_translated (const char *encoding_group)
{
	guint i;

	g_return_val_if_fail (encoding_group != NULL, NULL);
	g_return_val_if_fail (strlen (encoding_group) > 0, NULL);
	
	for (i = 0; i < mozilla_encoding_groups_table_get_count (); i++) {
		if (g_strcasecmp (encoding_group, mozilla_encoding_groups_table_peek_nth (i)) == 0) {
			return mozilla_encoding_groups_table_peek_nth_translated (i);
		}
	}
	
	return NULL;
}

guint
mozilla_encoding_table_get_count (void)
{
	return sizeof (encoding_table) / sizeof ((encoding_table)[0]);
}

const char *
mozilla_encoding_table_peek_nth (guint n)
{
	g_return_val_if_fail (n < mozilla_encoding_table_get_count (), NULL);

	return encoding_table[n];
}

const char *
mozilla_encoding_table_peek_nth_translated (guint n)
{
	g_return_val_if_fail (n < mozilla_encoding_table_get_count (), NULL);

	return gettext (encoding_table[n]);
}

const char *
mozilla_encoding_table_find_translated (const char *encoding)
{
	guint i;

	g_return_val_if_fail (encoding != NULL, NULL);
	g_return_val_if_fail (strlen (encoding) > 0, NULL);
	
	for (i = 0; i < mozilla_encoding_table_get_count (); i++) {
		if (g_strcasecmp (encoding, mozilla_encoding_table_peek_nth (i)) == 0) {
			return mozilla_encoding_table_peek_nth_translated (i);
		}
	}

	return NULL;
}

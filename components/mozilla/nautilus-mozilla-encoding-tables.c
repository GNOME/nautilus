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

typedef struct
{
	const char *string;
	const char *translated_string;
} TranslatedString;

static TranslatedString encoding_groups_table[] =
{
	{ "Arabic",		N_("Arabic") },
	{ "Baltic",		N_("Baltic") },
	{ "Central European",	N_("Central European") },
	{ "Chinese",		N_("Chinese") },
	{ "Cyrillic",		N_("Cyrillic") },
	{ "Greek",		N_("Greek") },
	{ "Hebrew",		N_("Hebrew") },
	{ "Japanese",		N_("Japanese"), },
	{ "Turkish",		N_("Turkish") },
	{ "Unicode",		N_("Unicode") },
	{ "UTF",		N_("UTF") },
	{ "Vietnamese",		N_("Vietnamese") },
	{ "Western",		N_("Western") }
};

static TranslatedString encoding_table[] =
{
	{ "Arabic (IBM-864)",			N_("Arabic (IBM-864)")},
	{ "Arabic (ISO-8859-6)",		N_("Arabic (ISO-8859-6)")},
	{ "Arabic (ISO-8859-6-E)",		N_("Arabic (ISO-8859-6-E)")},
	{ "Arabic (ISO-8859-6-I)",		N_("Arabic (ISO-8859-6-I)")},
	{ "Arabic (Windows-1256)",		N_("Arabic (Windows-1256)")},
	{ "Armenian (ARMSCII-8)",		N_("Armenian (ARMSCII-8)")},
	{ "Baltic (ISO-8859-13)",		N_("Baltic (ISO-8859-13)")},
	{ "Baltic (ISO-8859-4)",		N_("Baltic (ISO-8859-4)")},
	{ "Baltic (Windows-1257)",		N_("Baltic (Windows-1257)")},
	{ "Celtic (ISO-8859-14)",		N_("Celtic (ISO-8859-14)")},
	{ "Central European (IBM-852)",		N_("Central European (IBM-852)")},
	{ "Central European (ISO-8859-2)",	N_("Central European (ISO-8859-2)")},
	{ "Central European (MacCE)",		N_("Central European (MacCE)")},
	{ "Central European (Windows-1250)",	N_("Central European (Windows-1250)")},
	{ "Chinese Simplified (GB2312)",	N_("Chinese Simplified (GB2312)")},
	{ "Chinese Simplified (GBK)",		N_("Chinese Simplified (GBK)")},
	{ "Chinese Simplified (HZ)",		N_("Chinese Simplified (HZ)")},
	{ "Chinese Traditional (Big5)",		N_("Chinese Traditional (Big5)")},
	{ "Chinese Traditional (EUC-TW)",	N_("Chinese Traditional (EUC-TW)")},
	{ "Croatian (MacCroatian)",		N_("Croatian (MacCroatian)")},
	{ "Cyrillic (IBM-855)",			N_("Cyrillic (IBM-855)")},
	{ "Cyrillic (ISO-8859-5)",		N_("Cyrillic (ISO-8859-5)")},
	{ "Cyrillic (ISO-IR-111)",		N_("Cyrillic (ISO-IR-111)")},
	{ "Cyrillic (KOI8-R)",			N_("Cyrillic (KOI8-R)")},
	{ "Cyrillic (MacCyrillic)",		N_("Cyrillic (MacCyrillic)")},
	{ "Cyrillic (Windows-1251)",		N_("Cyrillic (Windows-1251)")},
	{ "Cyrillic/Russian (IBM-866)",		N_("Cyrillic/Russian (IBM-866)")},
	{ "Cyrillic/Ukrainian (KOI8-U)",	N_("Cyrillic/Ukrainian (KOI8-U)")},
	{ "Cyrillic/Ukrainian (MacUkrainian)",	N_("Cyrillic/Ukrainian (MacUkrainian)")},
	{ "English (US-ASCII)",			N_("English (US-ASCII)")},
	{ "Greek (ISO-8859-7)",			N_("Greek (ISO-8859-7)")},
	{ "Greek (MacGreek)",			N_("Greek (MacGreek)")},
	{ "Greek (Windows-1253)",		N_("Greek (Windows-1253)")},
	{ "Hebrew (IBM-862)",			N_("Hebrew (IBM-862)")},
	{ "Hebrew (ISO-8859-8-E)",		N_("Hebrew (ISO-8859-8-E)")},
	{ "Hebrew (ISO-8859-8-I)",		N_("Hebrew (ISO-8859-8-I)")},
	{ "Hebrew (Windows-1255)",		N_("Hebrew (Windows-1255)")},
	{ "Icelandic (MacIcelandic)",		N_("Icelandic (MacIcelandic)")},
	{ "Japanese (EUC-JP)",			N_("Japanese (EUC-JP)")},
	{ "Japanese (ISO-2022-JP)",		N_("Japanese (ISO-2022-JP)")},
	{ "Japanese (Shift_JIS)",		N_("Japanese (Shift_JIS)")},
	{ "Korean (EUC-KR)",			N_("Korean (EUC-KR)")},
	{ "Nordic (ISO-8859-10)",		N_("Nordic (ISO-8859-10)")},
	{ "Romanian (MacRomanian)",		N_("Romanian (MacRomanian)")},
	{ "South European (ISO-8859-3)",	N_("South European (ISO-8859-3)")},
	{ "T.61-8bit",				N_("T.61-8bit")},
	{ "Thai (TIS-620)",			N_("Thai (TIS-620)")},
	{ "Turkish (IBM-857)",			N_("Turkish (IBM-857)")},
	{ "Turkish (ISO-8859-9)",		N_("Turkish (ISO-8859-9)")},
	{ "Turkish (MacTurkish)",		N_("Turkish (MacTurkish)")},
	{ "Turkish (Windows-1254)",		N_("Turkish (Windows-1254)")},
	{ "Unicode (UTF-7)",			N_("Unicode (UTF-7)")},
	{ "Unicode (UTF-8)",			N_("Unicode (UTF-8)")},
	{ "User Defined",			N_("User Defined")},
	{ "UTF-16BE",				N_("UTF-16BE")},
	{ "UTF-16LE",				N_("UTF-16LE")},
	{ "UTF-32BE",				N_("UTF-32BE")},
	{ "UTF-32LE",				N_("UTF-32LE")},
	{ "Vietnamese (TCVN)",			N_("Vietnamese (TCVN)")},
	{ "Vietnamese (VISCII)",		N_("Vietnamese (VISCII)")},
	{ "Vietnamese (VPS)",			N_("Vietnamese (VPS)")},
	{ "Vietnamese (Windows-1258)",		N_("Vietnamese (Windows-1258)")},
	{ "Visual Hebrew (ISO-8859-8)",		N_("Visual Hebrew (ISO-8859-8)")},
	{ "Western (IBM-850)",			N_("Western (IBM-850)")},
	{ "Western (ISO-8859-1)",		N_("Western (ISO-8859-1)")},
	{ "Western (ISO-8859-15)",		N_("Western (ISO-8859-15)")},
	{ "Western (MacRoman)",			N_("Western (MacRoman)")},
	{ "Western (Windows-1252)",		N_("Western (Windows-1252)")},
	{ "windows-936",			N_("windows-936")},
	{ "x-imap4-modified-utf7",		N_("x-imap4-modified-utf7")},
	{ "x-u-escaped",			N_("x-u-escaped") }
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

	return encoding_groups_table[n].string;
}

const char *
mozilla_encoding_groups_table_peek_nth_translated (guint n)
{
	g_return_val_if_fail (n < mozilla_encoding_groups_table_get_count (), NULL);

	return gettext (encoding_groups_table[n].string);
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

	return encoding_table[n].string;
}

const char *
mozilla_encoding_table_peek_nth_translated (guint n)
{
	g_return_val_if_fail (n < mozilla_encoding_table_get_count (), NULL);

	return gettext (encoding_table[n].string);
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

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
 * nautilus-mozilla-embed-extensions.cpp - Extensions to GtkMozEmbed.
 */

#include <config.h>

#include "nautilus-mozilla-embed-extensions.h"
#include "gtkmozembed_internal.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include "nsIServiceManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"

typedef struct
{
	char *encoding;
	char *encoding_title;
} Entry;

typedef struct
{
	char *encoding_title;
	char *translated_encoding_title;
} TranslatedEncoding;

static GList *encoding_entry_list = NULL;

static nsIDocShell *mozilla_embed_get_primary_docshell                     (const GtkMozEmbed *mozilla_embed);
static guint        translated_encoding_get_count                          (void);
static const char * translated_encoding_peek_nth_encoding_title            (guint              n);
static const char * translated_encoding_peek_nth_translated_encoding_title (guint              n);
static const char * translated_encoding_find_translated_title              (const char        *title);

static guint        encoding_group_get_count                               (void);

static char *       convert_ns_string_to_c_string                          (const              nsString &ns_string);

static void
encoding_entry_list_free_one_entry (gpointer data,
				    gpointer user_data)
{
	Entry *entry;
	g_return_if_fail (data != NULL);
	entry = static_cast<Entry *>(data);
	g_free (entry->encoding);
	g_free (entry->encoding_title);
	g_free (entry);
}

static void
encoding_entry_list_free (void)
{
	if (encoding_entry_list == NULL) {
		return;
	}

	g_list_foreach (encoding_entry_list, encoding_entry_list_free_one_entry, NULL);
	g_list_free (encoding_entry_list);
	encoding_entry_list = NULL;
}

static const Entry *
encoding_entry_list_find_entry (const char *encoding_title)
{
	GList *node;
	const char *find;

	node = encoding_entry_list;

	while (node) {
		const Entry *entry;
		entry = static_cast<Entry *>(node->data);
		g_assert (entry != NULL);

		if (strcmp (entry->encoding_title, encoding_title) == 0) {
			return entry;
		}

		node = node->next;
	}

	return NULL;
}

static const Entry *
encoding_entry_list_peek_nth_entry (guint n)
{
	const char *s;
	
	if (n >= g_list_length (encoding_entry_list)) {
		return NULL;
	}

	return static_cast<Entry *>(g_list_nth_data (encoding_entry_list, n));
}

static guint
encoding_entry_list_length (void)
{
	return g_list_length (encoding_entry_list);
}

static void
encoding_entry_list_insert (const char *encoding,
			    const char *encoding_title)
{
	Entry *entry;

	g_return_if_fail (encoding != NULL);
	g_return_if_fail (encoding[0] != '\0');
	g_return_if_fail (encoding_title != NULL);
	g_return_if_fail (encoding_title[0] != '\0');
	g_return_if_fail (encoding_entry_list_find_entry (encoding_title) == NULL);

	entry = g_new0 (Entry, 1);
	entry->encoding = g_strdup (encoding);
	entry->encoding_title = g_strdup (encoding_title);
	encoding_entry_list = g_list_append (encoding_entry_list, entry);
}

static int
compare_entry (const Entry *a,
	       const Entry *b)
{
	g_return_val_if_fail (a != NULL, 1);
	g_return_val_if_fail (b != NULL, 1);

	return g_strcasecmp (a->encoding_title, b->encoding_title);
}

static void
encoding_entry_list_populate_once (void)
{
	static gboolean populated = FALSE;

	if (populated) {
		return;
	}

	populated = TRUE;

	nsresult rv;
	PRUint32 cscount;
	
	nsCOMPtr<nsIAtom> docCharsetAtom;

	nsCOMPtr<nsICharsetConverterManager2> ccm2 = do_GetService (NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
	
	g_return_if_fail (NS_SUCCEEDED (rv));
	
	nsCOMPtr <nsISupportsArray> cs_list;

	rv = ccm2->GetDecoderList(getter_AddRefs(cs_list));

	g_return_if_fail (NS_SUCCEEDED (rv));
	
	rv = cs_list->Count (&cscount);
	g_assert (NS_SUCCEEDED (rv));

	for (PRUint32 i = 0; i < cscount; i++) {
		nsCOMPtr<nsISupports> cssupports = (dont_AddRef)(cs_list->ElementAt(i));
		nsCOMPtr<nsIAtom> csatom ( do_QueryInterface(cssupports) );

		nsString ns_charset = NULL;
		nsString ns_charset_title = NULL;
		
		rv = csatom->ToString (ns_charset);
		g_assert (NS_SUCCEEDED (rv));
		char *charset = convert_ns_string_to_c_string (ns_charset);
		g_assert (charset != NULL);

		rv = ccm2->GetCharsetTitle2 (csatom, &ns_charset_title);
		
		char *charset_title = NULL;
		
		if (NS_SUCCEEDED (rv)) {
			charset_title = convert_ns_string_to_c_string (ns_charset_title);
		}
		
		if (charset_title == NULL || strlen (charset_title) == 0) {
			g_free (charset_title);
			charset_title = g_strdup (charset);
		}

		const char *tmp = translated_encoding_find_translated_title (charset_title);
		char *translated_charset_title;

		if (tmp && strlen (tmp) > 0) {
			translated_charset_title = g_strdup (tmp);
		} else {
			translated_charset_title = g_strdup (charset_title);
		}
		
 		encoding_entry_list_insert (charset, translated_charset_title);

		g_free (charset);
		g_free (charset_title);
		g_free (translated_charset_title);
	}

	encoding_entry_list = g_list_sort (encoding_entry_list, (GCompareFunc) compare_entry);
}

extern "C" guint
mozilla_charset_get_num_encodings (const GtkMozEmbed *mozilla_embed)
{
	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), 0);

	encoding_entry_list_populate_once ();

	return encoding_entry_list_length ();
}

extern "C" char *
mozilla_charset_get_nth_encoding (const GtkMozEmbed *mozilla_embed,
				  guint n)
{
	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), NULL);
	g_return_val_if_fail (n < mozilla_charset_get_num_encodings (mozilla_embed), NULL);

	const Entry *entry = encoding_entry_list_peek_nth_entry (n);
	g_return_val_if_fail (entry != NULL, NULL);
	return g_strdup (entry->encoding);
}

extern "C" char *
mozilla_charset_get_nth_encoding_title (const GtkMozEmbed *mozilla_embed,
					guint n)
{
	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), NULL);
	g_return_val_if_fail (n < mozilla_charset_get_num_encodings (mozilla_embed), NULL);

	const Entry *entry = encoding_entry_list_peek_nth_entry (n);
	g_return_val_if_fail (entry != NULL, NULL);
	return g_strdup (entry->encoding_title);
}

extern "C" gboolean
mozilla_charset_set_encoding (GtkMozEmbed *mozilla_embed,
			      const char *charset_encoding)
{
	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), FALSE);
	g_return_val_if_fail (charset_encoding != NULL, FALSE);

	nsCOMPtr<nsIDocShell> docShell;
	nsresult rv;

	docShell = mozilla_embed_get_primary_docshell (mozilla_embed);
	
	if (!docShell) {
		return FALSE;
	}

	nsCOMPtr<nsIContentViewer> contentViewer;
	rv = docShell->GetContentViewer (getter_AddRefs (contentViewer));
	if (!NS_SUCCEEDED (rv) || !contentViewer) {
		return FALSE;
	}

	nsCOMPtr<nsIMarkupDocumentViewer> markupDocumentViewer = do_QueryInterface (contentViewer, &rv);
	if (!NS_SUCCEEDED (rv) || !markupDocumentViewer) {
		return FALSE;
	}
	
	nsAutoString charsetString;
	charsetString.AssignWithConversion (charset_encoding);
	rv = markupDocumentViewer->SetForceCharacterSet (charsetString.ToNewUnicode());

	return NS_SUCCEEDED (rv) ? TRUE : FALSE;
}


/* This nonsense is needed to get the allocators right */
static char *
convert_ns_string_to_c_string (const nsString & ns_string)
{
	char *c_string;
	char *ns_c_string = ns_string.ToNewCString ();
	
	if (ns_c_string == NULL) {
		return NULL;
	}

	c_string = g_strdup (ns_c_string);

	nsMemory::Free (ns_c_string);

	return c_string;
}

static char *encoding_groups[] =
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

static guint
encoding_group_get_count (void)
{
	return sizeof (encoding_groups) / sizeof ((encoding_groups)[0]);
}

extern "C" char *
mozilla_charset_find_encoding_group (const GtkMozEmbed *mozilla_embed,
				     const char *encoding)
{
	guint i;

	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), NULL);
	g_return_val_if_fail (encoding != NULL, NULL);

	for (i = 0; i < encoding_group_get_count (); i++) {
		const char *group = encoding_groups[i];
		char *find;

		find = strstr (encoding, group);
		
		if (find != NULL) {
			return g_strdup (group);
		}
	}

	return NULL;
}

int
mozilla_charset_get_encoding_group_index (const GtkMozEmbed *mozilla_embed,
					  const char *encoding_group)
{
	guint i;

	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), -1);

	if (encoding_group == NULL) {
		return -1;
	}
	for (i = 0; i < encoding_group_get_count (); i++) {
		if (strcmp (encoding_groups[i], encoding_group) == 0) {
			return i;
		}
	}

	return -1;
}

static TranslatedEncoding translated_encodings[] =
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

static guint
translated_encoding_get_count (void)
{
	return sizeof (translated_encodings) / sizeof ((translated_encodings)[0]);
}

static const char *
translated_encoding_peek_nth_encoding_title (guint n)
{
	g_return_val_if_fail (n >= translated_encoding_get_count (), NULL);

	return translated_encodings[n].encoding_title;
}

static const char *
translated_encoding_find_translated_title (const char *title)
{
	g_return_val_if_fail (title != NULL, NULL);
	g_return_val_if_fail (title[0] != '\0', NULL);

	for (guint i = 0; i < translated_encoding_get_count (); i++) {
		const char *encoding_title;
		
		if (g_strcasecmp (translated_encodings[i].encoding_title, title) == 0) {
			return translated_encodings[i].translated_encoding_title;
		}
	}
	
	return NULL;
}

/* FIXME: This is cut-n-pasted from mozilla-events.cpp */
static nsIDocShell *
mozilla_embed_get_primary_docshell (const GtkMozEmbed *mozilla_embed)
{
	g_return_val_if_fail (GTK_IS_MOZ_EMBED (mozilla_embed), NULL);

	nsIWebBrowser *web_browser;
	gtk_moz_embed_get_nsIWebBrowser (const_cast<GtkMozEmbed *> (mozilla_embed), &web_browser);

	nsCOMPtr<nsIDocShell> doc_shell;
        nsCOMPtr<nsIDocShellTreeItem> browserAsItem = do_QueryInterface (web_browser);
	if (!browserAsItem) return NULL;

	// get the tree owner for that item
	nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
	nsresult rv = browserAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
	if (!NS_SUCCEEDED (rv) || ! treeOwner) return NULL;

	// get the primary content shell as an item
	nsCOMPtr<nsIDocShellTreeItem> contentItem;
	rv = treeOwner->GetPrimaryContentShell(getter_AddRefs(contentItem));
	if (!NS_SUCCEEDED (rv) || ! contentItem) return NULL;

	// QI that back to a docshell
	doc_shell = do_QueryInterface (contentItem);

	return doc_shell;
}


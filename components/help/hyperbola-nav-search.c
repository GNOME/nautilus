#include <config.h>
#include <libnautilus/libnautilus.h>
#include <gnome.h>
#include "hyperbola-filefmt.h"
#include <gtk/gtk.h>
#include <string.h>
#include <limits.h>
#include <libxml/parser.h>
#include <dirent.h>
#include <ctype.h>

#include "hyperbola-nav.h"

typedef struct {
	NautilusView *nautilus_view;

	GtkWidget *clist, *ent;

	gint8 notify_count;
} HyperbolaNavigationSearch;

static void
hyperbola_navigation_search_ent_changed (GtkWidget * ent,
					 HyperbolaNavigationSearch * hns)
{
}

static void
hyperbola_navigation_search_ent_activate (GtkWidget * ent,
					  HyperbolaNavigationSearch * hns)
{
	GString *query;
	FILE *fh;
	char *dir;
	char *ctmp, *line;

	query = g_string_new (NULL);

	gtk_clist_freeze (GTK_CLIST (hns->clist));
	gtk_clist_clear (GTK_CLIST (hns->clist));

	dir = gnome_datadir_file ("gnome/help");
	g_string_sprintf (query, "sgrep -l '");

	line = gtk_entry_get_text (GTK_ENTRY (hns->ent));
	while ((ctmp = strtok (line, " \t"))) {
		g_string_sprintfa (query, "%sword(\"%s\")",
				   line ? "" : " and ", ctmp);
		line = NULL;
	}
	g_string_sprintfa (query, "' %s/*/*/*.sgml", dir);
	g_free (dir);

	fh = popen (query->str, "r");
	g_string_free (query, TRUE);

	if (fh) {
		char aline[LINE_MAX];
		char uri[512], *uriptr;
		GHashTable *uri_hash;

		uri_hash = g_hash_table_new (g_str_hash, g_str_equal);

		while (fgets (aline, sizeof (aline), fh)) {
			char *filename, *ctmp;
			int len, rownum;

			if (strncmp (aline, "---", 3) != 0)	/* Skip non match-info lines */
				continue;

			g_strstrip (aline);
			filename = strtok (aline, " \t");
			if (!filename)
				continue;
			filename = strtok (NULL, " \t");
			if (!filename)
				continue;
			filename = strtok (NULL, " \t");
			if (!filename)
				continue;

			len = strlen (filename);
			if (filename[len - 1] == ':')
				filename[len - 1] = '\0';

			/* XXX lame! Should use gnome_file_locate eventually to figure out all possible prefixes. */
			ctmp = strstr (filename, "/help/");
			if (ctmp) {
				char **pieces;

				ctmp += strlen ("/help/");

				pieces = g_strsplit (ctmp, "/", -1);

				if (!pieces || !pieces[0] || !pieces[1] ||
				    !pieces[2] || pieces[3])
					ctmp = NULL;
				else {
					ctmp = strrchr (pieces[2], '.');
					if (ctmp && !strcmp (ctmp, ".sgml"))
						*ctmp = '\0';

					g_snprintf (uri, sizeof (uri),
						    "help:%s/%s", pieces[0],
						    pieces[2]);
				}

				g_strfreev (pieces);
			}

			if (!ctmp)
				g_snprintf (uri, sizeof (uri), "file://%s",
					    filename);

			uriptr = g_hash_table_lookup (uri_hash, uri);
			if (uriptr)
				continue;

			uriptr = uri;
			rownum =
				gtk_clist_append (GTK_CLIST (hns->clist),
						  &uriptr);
			if (gtk_clist_get_text
			    (GTK_CLIST (hns->clist), rownum, 0, &uriptr))
				g_hash_table_insert (uri_hash, uriptr,
						     uriptr);
		}

		pclose (fh);
		g_hash_table_destroy (uri_hash);
	}

	gtk_clist_thaw (GTK_CLIST (hns->clist));
}

static void
hyperbola_navigation_search_select_row (GtkWidget * clist, gint row,
					gint column, GdkEvent * event,
					HyperbolaNavigationSearch * hns)
{
	char *uri;

	if (!event || event->type != GDK_2BUTTON_PRESS)	/* we only care if the user has double-clicked on an item...? */
		return;

	if (gtk_clist_get_text (GTK_CLIST (clist), row, 0, &uri))
		return;

	nautilus_view_open_location_in_this_window (hns->nautilus_view, uri);
}

BonoboObject *
hyperbola_navigation_search_new (void)
{
	HyperbolaNavigationSearch *hns;
	GtkWidget *wtmp, *vbox;

	hns = g_new0 (HyperbolaNavigationSearch, 1);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD);

	hns->ent = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (hns->ent), "changed",
			    hyperbola_navigation_search_ent_changed, hns);
	gtk_signal_connect (GTK_OBJECT (hns->ent), "activate",
			    hyperbola_navigation_search_ent_activate, hns);
	gtk_container_add (GTK_CONTAINER (vbox), hns->ent);

	hns->clist = gtk_clist_new (1);
	gtk_clist_freeze (GTK_CLIST (hns->clist));
	gtk_clist_set_selection_mode (GTK_CLIST (hns->clist),
				      GTK_SELECTION_BROWSE);

	gtk_signal_connect (GTK_OBJECT (hns->clist), "select_row",
			    hyperbola_navigation_search_select_row, hns);

	wtmp =
		gtk_scrolled_window_new (gtk_clist_get_hadjustment
					 (GTK_CLIST (hns->clist)),
					 gtk_clist_get_vadjustment (GTK_CLIST
								    (hns->
								     clist)));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wtmp),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (wtmp), hns->clist);
	gtk_container_add (GTK_CONTAINER (vbox), wtmp);

	gtk_clist_columns_autosize (GTK_CLIST (hns->clist));
	gtk_clist_thaw (GTK_CLIST (hns->clist));
	gtk_widget_show_all (vbox);

	hns->nautilus_view = nautilus_view_new (vbox);

	return BONOBO_OBJECT (hns->nautilus_view);
}

#include "test.h"

#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-progress-info.h>

static void
copy_done (GHashTable *debuting_uris, gpointer data)
{
	g_print ("Copy done\n");
}

static void
changed_cb (NautilusProgressInfo *info,
	    gpointer data)
{
	g_print ("Changed: %s -- %s\n",
		 nautilus_progress_info_get_status (info),
		 nautilus_progress_info_get_details (info));
}

static void
progress_changed_cb (NautilusProgressInfo *info,
		     gpointer data)
{
	g_print ("Progress changed: %f\n",
		 nautilus_progress_info_get_progress (info));
}

static void
finished_cb (NautilusProgressInfo *info,
	     gpointer data)
{
	g_print ("Finished\n");
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GList *sources;
	GFile *dest;
	GFile *source;
	int i;
	GList *infos;
	NautilusProgressInfo *progress_info;
	
	g_thread_init (NULL);
	
	test_init (&argc, &argv);

	if (argc < 3) {
		g_print ("Usage test-copy <sources...> <dest dir>\n");
		return 1;
	}

	sources = NULL;
	for (i = 1; i < argc - 1; i++) {
		source = g_file_new_for_commandline_arg (argv[i]);
		sources = g_list_prepend (sources, source);
	}
	sources = g_list_reverse (sources);
	
	dest = g_file_new_for_commandline_arg (argv[i]);
	
	window = test_window_new ("copy test", 5);
	
	gtk_widget_show (window);

	nautilus_file_operations_copy (sources,
				       NULL /* GArray *relative_item_points */,
				       dest,
				       GTK_WINDOW (window),
				       copy_done, NULL);

	infos = nautilus_get_all_progress_info ();

	if (infos == NULL) {
		return 0;
	}

	progress_info = NAUTILUS_PROGRESS_INFO (infos->data);

	g_signal_connect (progress_info, "changed", (GCallback)changed_cb, NULL);
	g_signal_connect (progress_info, "progress-changed", (GCallback)progress_changed_cb, NULL);
	g_signal_connect (progress_info, "finished", (GCallback)finished_cb, NULL);
	
	gtk_main ();
	
	return 0;
}



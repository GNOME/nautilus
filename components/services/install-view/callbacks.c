/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001  Eazel, Inc
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Robey Pointer <robey@eazel.com>
 */

#include <config.h>

#include "nautilus-service-install-view.h"
#include "forms.h"
#include "callbacks.h"
#include "eazel-install-metadata.h"		/* eazel_install_configure_check_jump_after_install */
#include "libtrilobite/libammonite-gtk.h"
#include <stdio.h>
#include <errno.h>
#include <libeazelinstall.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>


/* do what gnome ought to do automatically */
static void
reply_callback (int reply, gboolean *answer)
{
	*answer = (reply == 0);
}

/* keep this info for secret later use */
void
nautilus_service_install_dependency_check (EazelInstallCallback *cb, const PackageData *package,
					   const PackageData *needs, NautilusServiceInstallView *view)
{
	char *key, *value;

	/* add to deps hash for later */
	if (g_hash_table_lookup_extended (view->details->deps, needs->name, (void **)&key, (void **)&value)) {
		g_hash_table_remove (view->details->deps, key);
		g_free (key);
		g_free (value);
	}
	g_hash_table_insert (view->details->deps, g_strdup (needs->name), g_strdup (package->name));

	/* this stuff flies by so fast, it's probably enough to just say "info" */
	value = g_strdup (_("Getting package information ..."));
	show_overall_feedback (view, value);
	g_free (value);
}

/* keep the user up-to-date on the install service's long-ass contemplations */
void
nautilus_service_install_conflict_check (EazelInstallCallback *cb, const PackageData *pack,
					 NautilusServiceInstallView *view)
{
	char *out;

	if (view->details->installer == NULL) {
		g_warning ("Got conflict check after unref!");
		return;
	}

	g_assert (pack->name != NULL);
	out = g_strdup_printf (_("Checking \"%s\" for conflicts..."), pack->name);
	show_overall_feedback (view, out);
	g_free (out);
}

gboolean
nautilus_service_install_preflight_check (EazelInstallCallback *cb, 
					  EazelInstallCallbackOperation op,
					  const GList *packages,
					  int total_bytes, 
					  int total_packages,
					  NautilusServiceInstallView *view)
{
	gboolean answer;
	PackageData *package;
	GList *package_list;
	GList *iter;
	char *out;
	char *extra;
	unsigned long total_k;

	if (view->details->cancelled) {
		/* user has already hit the cancel button */
		view->details->cancelled_before_downloads = TRUE;
		return FALSE;
	}

	/* assemble initial list of packages to browse */
        package_list = flatten_packagedata_dependency_tree ((GList *)packages);
	package_list = g_list_reverse (package_list);

	view->details->download_bytes_total = view->details->download_bytes_sofar = 0;
	for (iter = g_list_first (package_list); iter != NULL; iter = g_list_next (iter)) {
		package = PACKAGEDATA (iter->data);
		view->details->download_bytes_total += (package->filesize > 0 ? package->filesize : package->bytesize);

		if (package->toplevel) {
			nautilus_service_install_check_for_desktop_files (view,
									  cb,
									  package);
		}
	}

        /* draw the query box, and spin in gtk_main waiting for an answer */
        make_query_box (view, op, package_list);
        gtk_widget_hide (view->details->overall_feedback_text);

	nautilus_view_report_load_complete (view->details->nautilus_view);
        gtk_widget_ref (GTK_WIDGET (view));
        while (view->details->preflight_status == PREFLIGHT_UNKNOWN) {
                while (gtk_events_pending ()) {
                        gtk_main_iteration ();
                }
        }
        if (view->details->preflight_status == PREFLIGHT_PANIC_BUTTON) {
                /* user destroyed the view!
                 * hold on to our ref (it's probably the last one) and bail out as quickly as possible.
                 */
                g_list_free (package_list);
                return FALSE;
        }
        gtk_widget_unref (GTK_WIDGET (view));

        gtk_widget_hide (view->details->query_box);
	nautilus_view_report_load_underway (view->details->nautilus_view);
        answer = (view->details->preflight_status == PREFLIGHT_OK) ? TRUE : FALSE;

	if (!answer) {
		g_list_free (package_list);
		view->details->cancelled = TRUE;
		view->details->cancelled_before_downloads = TRUE;
		/* EVIL EVIL hack that causes the next dialog to show up instead of being hidden */
		sleep (1);
		while (gtk_events_pending ())
			gtk_main_iteration ();
		return answer;
	}

	total_k = (view->details->download_bytes_total+512)/1024;
	/* arbitrary dividing line */
	if (total_k > 4096) {
		extra = g_strdup_printf ("%ld MB", (total_k+512)/1024);
	} else {
		extra = g_strdup_printf ("%ld KB", total_k);
	}

	if (g_list_length (package_list) == 1) {
		out = g_strdup_printf (_("Downloading 1 package (%s)"), extra);
	} else {
		out = g_strdup_printf (_("Downloading %d packages (%s)"), g_list_length (package_list), extra);
	}
	show_overall_feedback (view, out);
	g_free (out);
	g_free (extra);

	g_list_free (package_list);
	view->details->current_package = 0;
	return answer;
}

void
nautilus_service_install_download_progress (EazelInstallCallback *cb, const PackageData *pack, int amount, int total,
                                            NautilusServiceInstallView *view)
{
	char *out;
	const char *needed_by;
	InstallMessage *im = view->details->current_im;
	float fake_amount;

	if (amount > total) {
		/* work around temporary EI bug where amount is sometimes total+1k */
		return;
	}

	if (view->details->installer == NULL) {
		g_warning ("Got download notice after unref!");
		return;
	}

	/* install lib better damn well know the name of the package by the time we download it! */
	g_assert (pack->name != NULL);
	view->details->downloaded_anything = TRUE;

	if (amount == 0) {
		/* could be a redundant zero-trigger for the same rpm... */
		if (view->details->current_rpm && (strcmp (view->details->current_rpm, pack->name) == 0)) {
			return;
		}

		if (view->details->cylon_timer) {
			gtk_timeout_remove (view->details->cylon_timer);
			view->details->cylon_timer = 0;
		}

		g_free (view->details->current_rpm);
		view->details->current_rpm = g_strdup (pack->name);

		if (pack->toplevel) {
			update_package_info_display (view, pack, _("Downloading \"%s\""));
		}

		/* new progress message and bar */
		im = view->details->current_im = install_message_new (view, pack->name);
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 0.0);
		out = g_strdup_printf (_("0K of %dK"), total/1024);
		nautilus_label_set_text (NAUTILUS_LABEL (im->progress_label), out);
		g_free (out);
		view->details->last_k = 0;

		needed_by = g_hash_table_lookup (view->details->deps, pack->name);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" requires \"%s\" to run.\nDownloading \"%s\" now."),
					       needed_by, pack->name, pack->name);
		} else {
			out = g_strdup_printf (_("Attempting to download package \"%s\"."), pack->name);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);
	} else if (amount == total) {
		/* done! */
		current_progress_bar_complete (view, _("Complete"));
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 1.0);
		needed_by = g_hash_table_lookup (view->details->deps, pack->name);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" requires \"%s\" to run.\n\"%s\" has been successfully downloaded."),
					       needed_by, pack->name, pack->name);
		} else {
			out = g_strdup_printf (_("The package \"%s\" has been successfully downloaded."), pack->name);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);
		g_free (view->details->current_rpm);
		view->details->current_rpm = NULL;
		view->details->current_im = NULL;
		/* update downloaded bytes */
		view->details->download_bytes_sofar += (pack->filesize > 0 ? pack->filesize : pack->bytesize);
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar),
					     (float) view->details->download_bytes_sofar /
					     (float) view->details->download_bytes_total);
	} else {
		/* could be a leftover event, after user hit STOP (in which case, current_im = NULL) */
		if ((im != NULL) && (im->progress_bar != NULL)) {
			gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar),
						     (float) amount / (float) total);
			if ((amount/1024) >= view->details->last_k + 10) {
				out = g_strdup_printf (_("%dK of %dK"), amount/1024, total/1024);
				nautilus_label_set_text (NAUTILUS_LABEL (im->progress_label), out);
				g_free (out);
				view->details->last_k = (amount/1024);
			}
		}

		/* so, for PR3, we are given a "size" field in the softcat XML which is actually 
		 * the size of the decompressed files.  so this little hocus-pocus scales the
		 * actual size (which we know once we start downloading the file) to match the   
		 * previously-assumed size
		 */
		fake_amount = (float)amount * (float)(pack->filesize > 0 ? pack->filesize : pack->bytesize) / (float)total;
		gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar),
					     ((float) view->details->download_bytes_sofar + fake_amount) /
					     (float) view->details->download_bytes_total);
	}
}

void
nautilus_service_install_download_failed (EazelInstallCallback *cb, const PackageData *pack,
					  NautilusServiceInstallView *view)
{
	char *out, *tmp;

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	tmp = packagedata_get_readable_name (pack);
	out = g_strdup_printf (_("Download of package \"%s\" failed!"), tmp);
	g_free (tmp);
	if (view->details->current_im != NULL) {
		nautilus_label_set_text (NAUTILUS_LABEL (view->details->current_im->label), out);
	}
	g_free (out);
}

static void
previous_install_finished (NautilusServiceInstallView *view)
{
	InstallMessage *im;
	char *needed_by;
	char *out;

	im = view->details->current_im;
	if (im != NULL) {
		current_progress_bar_complete (view, _("Complete"));
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 1.0);

		needed_by = g_hash_table_lookup (view->details->deps, view->details->current_rpm);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" requires \"%s\" to run.\n\"%s\" has been successfully downloaded and installed."),
					       needed_by, view->details->current_rpm, view->details->current_rpm);
		} else {
			out = g_strdup_printf (_("\"%s\" has been successfully downloaded and installed."), view->details->current_rpm);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);
	}
	g_free (view->details->current_rpm);
	view->details->current_rpm = NULL;
	view->details->current_im = NULL;
}

void
nautilus_service_install_progress (EazelInstallCallback *cb, const PackageData *pack,
                                   int current_package, int total_packages,
                                   int package_progress, int package_total,
                                   int total_progress, int total_total,
                                   NautilusServiceInstallView *view)
{
	InstallMessage *im;
	gfloat overall_complete, complete;
	char *out;
	char *needed_by;

	im = view->details->current_im;
	if (current_package != view->details->current_package) {
		/* no longer "loading" anything */
		nautilus_view_report_load_complete (view->details->nautilus_view);

		/* starting a new package -- create new progress indicator */
		out = g_strdup_printf (_("Installing package %d of %d"), current_package, total_packages);
		show_overall_feedback (view, out);
		g_free (out);

		/* new behavior: sometimes the previous package wasn't quite closed out -- do it now */
		if (im != NULL) {
			previous_install_finished (view);
		}

		/* if you're looking for the place where we notice that one of nautilus's core
		 * packages is being upgraded, this is it.  this is an evil, evil way to do it,
		 * but nobody's come up with anything better yet.
		 */
		if (pack->name) {		
			if ((g_strncasecmp (pack->name, "nautilus", 8) == 0) ||
			    (g_strncasecmp (pack->name, "gnome-vfs", 9) == 0) ||
			    (g_strncasecmp (pack->name, "oaf", 3) == 0)) {
				view->details->core_package = TRUE;
			} 
		}

		g_free (view->details->current_rpm);
		view->details->current_rpm = g_strdup (pack->name);
		view->details->current_im = im = install_message_new (view, pack->name);
		gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), 0.0);
		needed_by = g_hash_table_lookup (view->details->deps, pack->name);
		if (needed_by != NULL) {
			out = g_strdup_printf (_("The package \"%s\" requires \"%s\" to run.\n\"%s\" is now being installed."),
					       needed_by, pack->name, pack->name);
		} else {
			out = g_strdup_printf (_("Now installing package \"%s\"."), pack->name);
		}
		nautilus_label_set_text (NAUTILUS_LABEL (im->label), out);
		g_free (out);

		view->details->current_package = current_package;

		if (pack->toplevel) {
			update_package_info_display (view, pack, _("Installing \"%s\""));
		}
	}

	complete = (gfloat) package_progress / package_total;
	overall_complete = (gfloat) total_progress / total_total;
	gtk_progress_set_percentage (GTK_PROGRESS (im->progress_bar), complete);
	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), overall_complete);
	out = g_strdup_printf (_("%d%%"), (int)(complete*100.0));
	nautilus_label_set_text (NAUTILUS_LABEL (im->progress_label), out);
	g_free (out);

	if ((package_progress == package_total) && (package_total > 0)) {
		/* done with another package! */
		previous_install_finished (view);
	}
}

void
nautilus_service_install_failed (EazelInstallCallback *cb, const PackageData *package, NautilusServiceInstallView *view)
{
	char *tmp, *message;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

	if (package->status == PACKAGE_ALREADY_INSTALLED) {
		view->details->already_installed = TRUE;
		return;
	}

	/* override the "success" result for install_done signal */
	view->details->failures++;

	tmp = packagedata_get_readable_name (package);
	message = g_strdup_printf (_("Installation failed on %s"), tmp);
	show_overall_feedback (view, message);
	g_free (tmp);
	g_free (message);

	/* Get the new set of problem cases */
	eazel_install_problem_tree_to_case (view->details->problem,
					    package,
					    FALSE,
					    &(view->details->problem_cases));
}

/* most likely OBSOLETE */
static gboolean
nautilus_service_install_solve_cases (NautilusServiceInstallView *view)
{
	gboolean answer = FALSE;
	GtkWidget *toplevel;
	GString *messages;
	GList *strings;
	GtkWidget *dialog;

	messages = g_string_new ("");

	if (view->details->problem_cases) {
		GList *iterator;
		/* Create string versions to show the user */
		g_string_sprintfa (messages, "%s\n%s\n\n", 
				   _("I ran into problems while installing."), 
				   _("I'd like to try the following :"));
		strings = eazel_install_problem_cases_to_string (view->details->problem,
								 view->details->problem_cases);
		for (iterator = strings; iterator; iterator = g_list_next (iterator)) {
			g_string_sprintfa (messages, " \xB7 %s\n", (char*)(iterator->data));
		}
		g_list_foreach (strings, (GFunc)g_free, NULL);
		g_list_free (strings);
		g_string_sprintfa (messages, "\n%s",  
				   _("Is this ok ?"));
		
		toplevel = gtk_widget_get_toplevel (view->details->message_box);
		if (GTK_IS_WINDOW (toplevel)) {
			dialog = gnome_question_dialog_parented (messages->str, (GnomeReplyCallback)reply_callback,
								 &answer, GTK_WINDOW (toplevel));
		} else {
			dialog = gnome_question_dialog (messages->str, (GnomeReplyCallback)reply_callback, &answer);
		}
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		g_string_free (messages, TRUE);
	}

	return answer;
}

void
nautilus_service_install_done (EazelInstallCallback *cb, gboolean success, NautilusServiceInstallView *view)
{
	CORBA_Environment ev;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	char *message;
	char *name;
	GString *real_message;
	gboolean answer = FALSE;
	gboolean question_dialog;
	GList *packlist, *iter;
	PackageData *pack;

	g_assert (NAUTILUS_IS_SERVICE_INSTALL_VIEW (view));

        if (view->details->preflight_status == PREFLIGHT_PANIC_BUTTON) {
                /* user actually destroyed us -- panic! */
g_warning ("done: panic!");
                return;
        }

	/* 'success' will be FALSE if even *one* package failed.  need to check for that. */

	packlist = ((CategoryData *) view->details->categories->data)->packages;

#if 1
	for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
		pack = PACKAGEDATA (iter->data);
		trilobite_debug ("package %s status %d", pack->name, pack->status);
	}
#endif

	/* no longer "loading" anything */
	nautilus_view_report_load_complete (view->details->nautilus_view);

	gtk_progress_set_percentage (GTK_PROGRESS (view->details->total_progress_bar), success ? 1.0 : 0.0);

	g_free (view->details->current_rpm);
	view->details->current_rpm = NULL;

	if (view->details->cancelled) {
		message = _("Installation aborted.");
	} else if (view->details->already_installed) {
		message = _("This package has already been installed.");
	} else if (success) {
		message = _("Installation complete.");
	} else {
		/* FIXME 5906: this isn't really working right yet, so fix it later */
		if (1 || ((guint) view->details->failures == g_list_length (packlist))) {
			message = _("Installation failed.");
			answer = nautilus_service_install_solve_cases (view);
		} else {
			/* some succeeded; some failed */
			real_message = g_string_new (_("Some packages installed successfully:"));
			for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
				pack = PACKAGEDATA (iter->data);
				if (pack->status == PACKAGE_RESOLVED) {
					name = packagedata_get_readable_name (pack);
					g_string_sprintfa (real_message, "\n  \xB7 %s", name);
					g_free (name);
				}
			}
			g_string_sprintfa (real_message, _("\nSome packages failed:"));
			for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
				pack = PACKAGEDATA (iter->data);
				if (pack->status != PACKAGE_RESOLVED) {
					name = packagedata_get_readable_name (pack);
					g_string_sprintfa (real_message, "\n  \xB7 %s", name);
					g_free (name);
				}
			}

			message = real_message->str;
			g_string_free (real_message, FALSE);
			answer = nautilus_service_install_solve_cases (view);
		}
	}

	show_overall_feedback (view, message);

	if (answer) {
		eazel_install_problem_handle_cases (view->details->problem, 
						    view->details->installer, 
						    &(view->details->problem_cases), 
						    &(view->details->categories),
						    NULL,
						    NULL);
	} else {
		real_message = g_string_new (message);
		question_dialog = TRUE;
		answer = FALSE;

		if (success && view->details->desktop_files &&
		    !view->details->cancelled &&
		    !view->details->already_installed) {
			g_string_sprintfa (real_message, "\n%s", nautilus_install_service_locate_menu_entries (view));
		}
		if (view->details->cancelled_before_downloads ||
		    view->details->already_installed ||
		    (nautilus_preferences_get_user_level () < NAUTILUS_USER_LEVEL_ADVANCED)) {
			/* don't ask about erasing rpms */
			question_dialog = FALSE;
			answer = TRUE;
		} else if (view->details->downloaded_anything) {
			if (view->details->cancelled || view->details->failures) {
				g_string_sprintfa (real_message, "\n%s", _("Erase the RPM files?"));
			} else {
				g_string_sprintfa (real_message, "\n%s", _("Erase the leftover RPM files?"));
			}
		} else {
			question_dialog = FALSE;
		}

		toplevel = gtk_widget_get_toplevel (view->details->message_box);
		if (GTK_IS_WINDOW (toplevel)) {
			if (question_dialog) {
				dialog = gnome_question_dialog_parented (real_message->str,
									 (GnomeReplyCallback)reply_callback,
									 &answer, GTK_WINDOW (toplevel));
			} else {
				dialog = gnome_ok_dialog_parented (real_message->str, GTK_WINDOW (toplevel));
			}
		} else {
			if (question_dialog) {
				dialog = gnome_question_dialog (real_message->str,
								(GnomeReplyCallback)reply_callback, &answer);
			} else {
				dialog = gnome_ok_dialog (real_message->str);
			}
		}
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		g_string_free (real_message, TRUE);

		if (answer) {
			CORBA_exception_init (&ev);
			eazel_install_callback_delete_files (cb, &ev);
			CORBA_exception_free (&ev);
		}
		
		if (success && view->details->core_package) {
			message = _("A core package of Nautilus has been\n"
				    "updated.  You should restart Nautilus.\n\n"
				    "Do you wish to do that now?");
			if (GTK_IS_WINDOW (toplevel)) {
				dialog = gnome_question_dialog_parented (message,
									 (GnomeReplyCallback)reply_callback,
									 &answer, GTK_WINDOW (toplevel));
			} else {
				dialog = gnome_question_dialog (message, (GnomeReplyCallback)reply_callback,
								&answer);
			}
			gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
			gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
			
			if (answer) {
				if (execlp ("nautilus", "nautilus", "--restart", NULL) < 0) {
					g_message ("Exec error %s", strerror (errno));
				}
			}
		}

		/* send them to the predetermined "next" url
		 * -- but only if they haven't set jump-after-install off
		 */
		if ((view->details->username != NULL) &&
		    (strcasecmp (view->details->username, EAZELPROXY_USERNAME_ANONYMOUS) == 0)) {
			/* send anonymous users elsewhere, so they won't have to login */
			message = g_strdup (NEXT_URL_ANONYMOUS);
		} else {
			message = g_strdup (NEXT_URL);
		}
		message = NULL;
		if (eazel_install_configure_check_jump_after_install (&message)) {
			if (message != NULL) {
				nautilus_view_open_location_in_this_window (view->details->nautilus_view, message);
			} else {
				g_warning ("attemping to go back");
				nautilus_view_go_back (view->details->nautilus_view);
			}
		}
		g_free (message);
	}
}

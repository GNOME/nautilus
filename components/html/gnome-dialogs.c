/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <WWWCore.h>
#include <WWWStream.h>
#include <WWWTrans.h>
#include <WWWHTTP.h>
#include <HTDialog.h>

#undef PACKAGE
#undef VERSION
#undef _

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "glibwww.h"

#include <gnome.h>

static const char * HTDialogs[HT_MSG_ELEMENTS] = {
	HT_MSG_ENGLISH_INITIALIZER
};
#if 0
static HTErrorMessage HTErrors[HTERR_ELEMENTS] = {
	HTERR_ENGLISH_INITIALIZER
};
#endif

typedef struct _ProgressInfo ProgressInfo;
struct _ProgressInfo {
	HTRequest *req;
	GtkBox *box;
	GtkLabel *url;
	GtkProgress *progress;
};

static GtkWidget *prog_win = NULL;
static GtkWidget *prog_box = NULL;
static GList *prog_info = NULL;

static gint
hide_win(GtkWidget *win)
{
	gtk_widget_hide(win);
	return TRUE;
}

/* a simple routine to remove the progress meter from the screen when the
 * request finishes */
static int
hide_progress(HTRequest *request, HTResponse *response,
	      void *param, int status)
{
	GList *tmp;

	for (tmp = prog_info; tmp; tmp = tmp->next) {
		ProgressInfo *info = tmp->data;

		if (info->req == request) {
			/* this will destroy the widgets */
			gtk_container_remove(GTK_CONTAINER(prog_box), GTK_WIDGET(info->box));
			prog_info = g_list_remove(prog_info, info);
			g_free(info);
			if (prog_info == NULL)
				gtk_widget_hide(prog_win);
			return HT_OK;
		}
	}
	return HT_OK;
}

static ProgressInfo *
get_progress(HTRequest *req)
{
	GList *tmp;
	ProgressInfo *info;
	GtkWidget *box, *label, *progress;
	char *uri;

	for (tmp = prog_info; tmp; tmp = tmp->next) {
		info = tmp->data;
		if (info->req == req)
			return info;
	}

	if (prog_win == NULL) {
		prog_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(prog_win), _("Transfer Progress"));
		gtk_window_set_policy(GTK_WINDOW(prog_win), FALSE, FALSE, TRUE);
		gtk_signal_connect(GTK_OBJECT(prog_win), "delete_event",
				   GTK_SIGNAL_FUNC(hide_win), NULL);
		prog_box = gtk_vbox_new(FALSE, GNOME_PAD);
		gtk_container_set_border_width(GTK_CONTAINER(prog_box), GNOME_PAD_SMALL);
		gtk_container_add(GTK_CONTAINER(prog_win), prog_box);
		gtk_widget_show(prog_box);
	}    

	info = g_new(ProgressInfo, 1);
	prog_info = g_list_append(prog_info, info);
	info->req = req;

	HTRequest_addAfter(req, hide_progress, NULL, NULL,
			   HT_ALL, HT_FILTER_LAST, FALSE);

	box = gtk_vbox_new(FALSE, GNOME_PAD_SMALL);
	info->box = GTK_BOX(box);

	uri = HTAnchor_address((HTAnchor *)HTRequest_anchor(req));
	label = gtk_label_new(uri);
	info->url = GTK_LABEL(label);
	HT_FREE(uri);
	gtk_box_pack_start(info->box, label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	progress = gtk_progress_bar_new();
	info->progress = GTK_PROGRESS(progress);
	gtk_progress_set_show_text(info->progress, TRUE);
	gtk_box_pack_start(info->box, progress, TRUE, TRUE, 0);
	gtk_widget_show(progress);

	gtk_box_pack_start(GTK_BOX(prog_box), box, TRUE, TRUE, 0);
	gtk_widget_show(box);
	gtk_widget_show(prog_win);

	return info;
}

static BOOL
glibwww_progress(HTRequest *request, HTAlertOpcode op,
		 int msgnum, const char *dfault, void *input,
		 HTAlertPar *reply)
{
	ProgressInfo *info;
	gchar *text;
	long cl;

	if (!request)
		return NO;
	info = get_progress(request);
	switch (op) {
	case HT_PROG_DNS:
		text = g_strdup_printf(_("Looking up %s"), input?(char*)input:"");
		gtk_progress_set_format_string(info->progress, text);
		g_free(text);
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_CONNECT:
		text = g_strdup_printf(_("Contacting %s"), input?(char*)input:"");
		gtk_progress_set_format_string(info->progress, text);
		g_free(text);
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_ACCEPT:
		gtk_progress_set_format_string(info->progress,
					       _("Waiting for a connection..."));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_LOGIN:
		gtk_progress_set_format_string(info->progress, _("Logging in..."));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_READ:
		cl = HTAnchor_length(HTRequest_anchor(request));
		if (cl > 0) {
			long b_read = HTRequest_bodyRead(request);
			gfloat pcnt = (double)b_read/cl;

			text = g_strdup_printf(_("Read %d%%%% of %ld"), (int)pcnt, cl);
			gtk_progress_set_format_string(info->progress, text);
			g_free(text);
			gtk_progress_set_activity_mode(info->progress, FALSE);
			gtk_progress_set_percentage(info->progress, pcnt);
		} else {
			long b_read = HTRequest_bytesRead(request);
			int *raw_read = input ? (int *)input : NULL;

			if (b_read > 0)
				text = g_strdup_printf(_("Read %ld bytes"), b_read);
			else if (raw_read && *raw_read > 0)
				text = g_strdup_printf(_("Read %d bytes"), *raw_read);
			else
				text = g_strdup(_("Reading..."));
			gtk_progress_set_format_string(info->progress, text);
			g_free(text);
			gtk_progress_set_activity_mode(info->progress, TRUE);
		}
		break;
	case HT_PROG_WRITE:
		if (HTMethod_hasEntity(HTRequest_method(request))) {
			HTParentAnchor *anchor = HTRequest_anchor(HTRequest_source(request));

			cl = HTAnchor_length(anchor);
			if (cl > 0) {
				long b_write = HTRequest_bodyWritten(request);
				gfloat pcnt = (double)b_write/cl;

				text = g_strdup_printf(_("Writing %d%%%% of %ld"), (int)pcnt, cl);
				gtk_progress_set_format_string(info->progress, text);
				g_free(text);
				gtk_progress_set_activity_mode(info->progress, FALSE);
				gtk_progress_set_percentage(info->progress, pcnt);
			} else {
				long b_write = HTRequest_bytesWritten(request);
				int *raw_write = input ? (int *)input : NULL;

				if (b_write > 0)
					text = g_strdup_printf(_("Writing %ld bytes"), b_write);
				else if (raw_write && *raw_write > 0)
					text = g_strdup_printf(_("Writing %d bytes"), *raw_write);
				else
					text = g_strdup(_("Writing..."));
				gtk_progress_set_format_string(info->progress, text);
				g_free(text);
				gtk_progress_set_activity_mode(info->progress, TRUE);
			}
		}
		break;
	case HT_PROG_DONE:
		gtk_progress_set_format_string(info->progress, _("Done!"));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_INTERRUPT:
		gtk_progress_set_format_string(info->progress, _("Interrupted!"));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_OTHER:
		gtk_progress_set_format_string(info->progress, _("Please wait..."));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	case HT_PROG_TIMEOUT:
		gtk_progress_set_format_string(info->progress, _("Request timeout!"));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	default:
		gtk_progress_set_format_string(info->progress, _("Unknown"));
		gtk_progress_set_activity_mode(info->progress, TRUE);
		break;
	}
	return YES;
}

static BOOL
glibwww_confirm(HTRequest *request, HTAlertOpcode op,
		int msgnum, const char *dfault, void *input,
		HTAlertPar *reply)
{
	GtkWidget *dlg = gnome_message_box_new(HTDialogs[msgnum],
					       GNOME_MESSAGE_BOX_QUESTION,
					       GNOME_STOCK_BUTTON_YES,
					       GNOME_STOCK_BUTTON_NO,
					       NULL);
	gint button;

	gnome_dialog_set_default(GNOME_DIALOG(dlg), 0);
	gnome_dialog_close_hides(GNOME_DIALOG(dlg), FALSE);
	gnome_dialog_set_close(GNOME_DIALOG(dlg), TRUE);

	button = gnome_dialog_run_and_close(GNOME_DIALOG(dlg));
	return button == 0;
}

static BOOL
glibwww_prompt(HTRequest *request, HTAlertOpcode op,
	       int msgnum, const char *dfault, void *input,
	       HTAlertPar *reply)
{
	GtkWidget *dlg;
	GtkWidget *label;
	GtkWidget *entry;
	gint button;

	if (msgnum == HT_MSG_FILENAME) {
		if (dfault)
			HTAlert_setReplyMessage(reply, dfault);
		return YES;
	}

	dlg = gnome_dialog_new(_("Enter Text"), GNOME_STOCK_BUTTON_OK, NULL);
	if (input) {
		gchar *tmp = g_strconcat(HTDialogs[msgnum], " (",
					 (char *)input, ")", NULL);
		label = gtk_label_new(tmp);
		g_free(tmp);
	} else
		label = gtk_label_new(HTDialogs[msgnum]);
	entry = gtk_entry_new();
	if (dfault)
		gtk_entry_set_text(GTK_ENTRY(entry), dfault);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), entry, TRUE, TRUE, 0);
	gtk_widget_show(label);
	gtk_widget_show(entry);

	gnome_dialog_set_default(GNOME_DIALOG(dlg), 0);
	gnome_dialog_close_hides(GNOME_DIALOG(dlg), TRUE);
	gnome_dialog_set_close(GNOME_DIALOG(dlg), TRUE);
	gnome_dialog_editable_enters(GNOME_DIALOG(dlg), GTK_EDITABLE(entry));

	button = gnome_dialog_run_and_close(GNOME_DIALOG(dlg));
	if (button == 0) {
		HTAlert_setReplyMessage(reply, gtk_entry_get_text(GTK_ENTRY(entry)));
		gtk_widget_unref(dlg);
		return YES;
	}
	gtk_widget_unref(dlg);
	return NO;
}

static BOOL
glibwww_prompt_password(HTRequest *request, HTAlertOpcode op,
			int msgnum, const char *dfault,
			void *input, HTAlertPar *reply)
{
	GtkWidget *dlg;
	GtkWidget *label;
	GtkWidget *entry;
	gint button;

	dlg = gnome_dialog_new(_("Enter Password"), GNOME_STOCK_BUTTON_OK, NULL);
	if (input) {
		gchar *tmp = g_strconcat(HTDialogs[msgnum], " (",
					 (char *)input, ")", NULL);
		label = gtk_label_new(tmp);
		g_free(tmp);
	} else
		label = gtk_label_new(HTDialogs[msgnum]);
	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), entry, TRUE, TRUE, 0);
	gtk_widget_show(label);
	gtk_widget_show(entry);

	gnome_dialog_set_default(GNOME_DIALOG(dlg), 0);
	gnome_dialog_close_hides(GNOME_DIALOG(dlg), TRUE);
	gnome_dialog_set_close(GNOME_DIALOG(dlg), TRUE);
	gnome_dialog_editable_enters(GNOME_DIALOG(dlg), GTK_EDITABLE(entry));

	button = gnome_dialog_run_and_close(GNOME_DIALOG(dlg));
	if (button == 0) {
		HTAlert_setReplySecret(reply, gtk_entry_get_text(GTK_ENTRY(entry)));
		gtk_widget_unref(dlg);
		return YES;
	}
	gtk_widget_unref(dlg);
	return NO;
}

static BOOL
glibwww_prompt_username_and_password(HTRequest *request, HTAlertOpcode op,
				     int msgnum, const char *dfault,
				     void *input, HTAlertPar *reply)
{
	GtkWidget *dlg;
	GtkWidget *label;
	GtkWidget *entry1;
	GtkWidget *entry2;
	gint button;

	dlg = gnome_dialog_new(_("Enter Password"), GNOME_STOCK_BUTTON_OK,
			       GNOME_STOCK_BUTTON_CANCEL, NULL);
	if (input) {
		gchar *tmp = g_strconcat(HTDialogs[msgnum], " (",
					 (char *)input, ")", NULL);
		label = gtk_label_new(tmp);
		g_free(tmp);
	} else
		label = gtk_label_new(HTDialogs[msgnum]);
	entry1 = gtk_entry_new();
	if (dfault)
		gtk_entry_set_text(GTK_ENTRY(entry1), dfault);
	entry2 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry2), FALSE);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), entry1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dlg)->vbox), entry2, TRUE, TRUE, 0);
	gtk_widget_show(label);
	gtk_widget_show(entry1);
	gtk_widget_show(entry2);

	gnome_dialog_set_default(GNOME_DIALOG(dlg), 0);
	gnome_dialog_close_hides(GNOME_DIALOG(dlg), TRUE);
	gnome_dialog_set_close(GNOME_DIALOG(dlg), TRUE);
	/* enter on first entry moves focus to second one */
	gtk_signal_connect_object(GTK_OBJECT(entry1), "activate",
				  GTK_SIGNAL_FUNC(gtk_widget_grab_focus),
				  GTK_OBJECT(entry2));
	gnome_dialog_editable_enters(GNOME_DIALOG(dlg), GTK_EDITABLE(entry2));

	button = gnome_dialog_run_and_close(GNOME_DIALOG(dlg));
	if (button == 0) {
		HTAlert_setReplyMessage(reply, gtk_entry_get_text(GTK_ENTRY(entry1)));
		HTAlert_setReplySecret(reply, gtk_entry_get_text(GTK_ENTRY(entry2)));
		gtk_widget_unref(dlg);
		return YES;
	}
	gtk_widget_unref(dlg);
	return NO;
}

void
glibwww_register_gnome_dialogs(GLibWWWDialogType type)
{
	if(type & DLG_PROGRESS)
		HTAlert_add(glibwww_progress, HT_A_PROGRESS);
	/* HTAlert_add(glibwww_message, HT_A_MESSAGE);
	 */
	if(type & DLG_CONFIRM)
		HTAlert_add(glibwww_confirm, HT_A_CONFIRM);
	if(type & DLG_PROMPT)
		HTAlert_add(glibwww_prompt, HT_A_PROMPT);
	if(type & DLG_AUTH) {
		HTAlert_add(glibwww_prompt_password, HT_A_SECRET);
		HTAlert_add(glibwww_prompt_username_and_password, HT_A_USER_PW);
	}

	HTAlert_setInteractive(YES);
}

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

#ifndef NAUTILUS_SUMMARY_VIEW_PRIVATE_H
#define NAUTILUS_SUMMARY_VIEW_PRIVATE_H

#include <libtrilobite/libammonite.h>

#include <libtrilobite/trilobite-redirect.h>

#include <gnome.h>

#define DEFAULT_SUMMARY_BACKGROUND_COLOR_SPEC	"rgb:FFFF/FFFF/FFFF"
#define DEFAULT_SUMMARY_BACKGROUND_COLOR_RGB	NAUTILUS_RGB_COLOR_WHITE
#define DEFAULT_SUMMARY_TEXT_COLOR_RGB		NAUTILUS_RGB_COLOR_BLACK

#define URL_REDIRECT_TABLE_HOME			"eazel-services://anonymous/services/urls"
#define URL_REDIRECT_TABLE_HOME_2		"eazel-services:/services/urls"
#define SUMMARY_CONFIG_XML			"eazel-services://anonymous/services"
#define SUMMARY_CONFIG_XML_2			"eazel-services:/services"

#define SUMMARY_TERMS_OF_USE_URI		"eazel-services://anonymous/aboutus/terms_of_use"
#define SUMMARY_PRIVACY_STATEMENT_URI		"eazel-services://anonymous/aboutus/privacy"
#define SUMMARY_CHANGE_PWD_FORM			"eazel-services://anonymous/account/login/lost_pwd_form"

#define SUMMARY_XML_KEY				"eazel_summary_xml"
#define URL_REDIRECT_TABLE			"eazel_url_table_xml"
#define REGISTER_KEY				"eazel_service_register"
#define PREFERENCES_KEY				"eazel_service_account_maintenance"

#define MAX_IMAGE_WIDTH				50
#define MAX_IMAGE_HEIGHT			50

#define FOOTER_REGISTER_OR_PREFERENCES		0
#define FOOTER_LOGIN_OR_LOGOUT			1
#define FOOTER_TERMS_OF_USER			2
#define FOOTER_PRIVACY_STATEMENT		3


enum {
	LOGIN_DIALOG_NAME_ROW,
	LOGIN_DIALOG_PASSWORD_ROW,
	LOGIN_DIALOG_ROW_COUNT
};

enum {
	LOGIN_DIALOG_REGISTER_BUTTON_INDEX,
	LOGIN_DIALOG_OK_BUTTON_INDEX,
	LOGIN_DIALOG_CANCEL_BUTTON
};

typedef enum {
	Pending_None,
	Pending_Login,
} SummaryPendingOperationType;

typedef enum {
	initial,
	retry,
	fail,
} SummaryLoginAttemptType;


/* A NautilusContentView's private information. */
struct _NautilusSummaryViewDetails {
	char				*uri;
	NautilusView			*nautilus_view;
	SummaryData			*xml_data;

	/* Parent form and title */
	GtkWidget			*form;
	GtkWidget			*header;
	GtkWidget			*news_pane;
	GtkWidget                       *news_item_vbox;
	GtkWidget			*services_list_pane;
	GtkWidget                       *services_list_vbox;
	GtkWidget			*featured_downloads_pane;
	GtkWidget                       *featured_downloads_vbox;
	GtkWidget			*footer;

	/* Login State */
	char				*user_name;
	volatile gboolean		logged_in;
	GtkWidget			*caption_table;
	SummaryLoginAttemptType		current_attempt;
	int				attempt_number;

	/* EazelProxy -- for logging in/logging out */
	EazelProxy_UserControl		user_control;
	SummaryPendingOperationType	pending_operation;
	EazelProxy_AuthnCallback	authn_callback;

	/* Login Dialog */
	GnomeDialog			*login_dialog;

	/* Async XML fetch handles */
	TrilobiteRedirectFetchHandle *redirect_fetch_handle;
	EazelSummaryFetchHandle *summary_fetch_handle;
};


#endif /* NAUTILUS_SUMMARY_VIEW_PRIVATE_H */




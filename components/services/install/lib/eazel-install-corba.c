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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>

#ifndef EAZEL_INSTALL_NO_CORBA
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libtrilobite/libtrilobite.h>

#include "trilobite-eazel-install.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"
#include "eazel-install-corba-types.h"
#include "eazel-install-query.h"

#define RELEASE_CB if (servant->object->callback != CORBA_OBJECT_NIL) { \
   CORBA_Object_release (servant->object->callback, ev); \
}
#define SET_CB(cb) servant->object->callback = CORBA_Object_duplicate(cb, ev)

/*****************************************
  Corba stuff
*****************************************/

typedef struct {
	POA_GNOME_Trilobite_Eazel_Install poa;
	EazelInstall *object;
} impl_POA_GNOME_Trilobite_Eazel_Install;

static void 
impl_Eazel_Install_install(impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			   const CORBA_char *package_list,
			   const GNOME_Trilobite_Eazel_InstallCallback cb,
			   CORBA_Environment * ev) 
{
	RELEASE_CB;
	SET_CB (cb);
}

static void 
impl_Eazel_Install_uninstall(impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			     const CORBA_char *package_list,
			     const GNOME_Trilobite_Eazel_InstallCallback cb,
			     CORBA_Environment * ev) 
{
	RELEASE_CB;
	SET_CB (cb);

	return;
}

static void 
impl_Eazel_Install_install_packages(impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				    const GNOME_Trilobite_Eazel_CategoryStructList *corbacategories,
				    const CORBA_char *root,
				    const GNOME_Trilobite_Eazel_InstallCallback cb,
				    CORBA_Environment * ev) 
{
	GList *categories;

	RELEASE_CB;
	SET_CB (cb);

	categories = NULL;
	categories = categorydata_list_from_corba_categorystructlist (corbacategories);
	eazel_install_install_packages (servant->object, 
					categories, 
					!root || strcmp (root, "")==0 ? NULL : root);

	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	g_list_free (categories);

	return;
}

static void 
impl_Eazel_Install_uninstall_packages(impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				      const GNOME_Trilobite_Eazel_CategoryStructList *corbacategories,
				      const CORBA_char *root,
				      const GNOME_Trilobite_Eazel_InstallCallback cb,
				      CORBA_Environment * ev) 
{
	GList *categories;

	RELEASE_CB;
	SET_CB (cb);

	categories = NULL;
	categories = categorydata_list_from_corba_categorystructlist (corbacategories);
	eazel_install_uninstall_packages (servant->object, 
					  categories,
					  !root || strcmp (root, "")==0 ? NULL : root);

	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	g_list_free (categories);

	return;
}

static void
impl_Eazel_Install_stop (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			 CORBA_Environment * ev)
{
	trilobite_debug (">>> STOP <<<");
	servant->object->private->cancel_download = TRUE;
}

static void
impl_Eazel_Install_delete_files (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 CORBA_Environment * ev)
{
	eazel_install_delete_downloads (servant->object);
}

static void
impl_Eazel_Install_revert_transaction (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				       const CORBA_char *xml, 
				       const CORBA_char *root,
				       const GNOME_Trilobite_Eazel_InstallCallback cb,
				       CORBA_Environment * ev) 
{
	RELEASE_CB;
	SET_CB (cb);       

	eazel_install_revert_transaction_from_xmlstring (servant->object, 
							 xml, 
							 strlen (xml),
							 !root || strcmp (root, "")==0 ? NULL : root);

	return;
}

static void
impl_Eazel_Install__set_verbose (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 const CORBA_boolean value,
				 CORBA_Environment *ev)
{
	eazel_install_set_verbose (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_verbose (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 CORBA_Environment *ev)
{
	return eazel_install_get_verbose (servant->object);
}

static void
impl_Eazel_Install__set_debug (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 const CORBA_boolean value,
				 CORBA_Environment *ev)
{
	eazel_install_set_debug (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_debug (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 CORBA_Environment *ev)
{
	return eazel_install_get_debug (servant->object);
}

static void
impl_Eazel_Install__set_silent (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				const CORBA_boolean value,
				CORBA_Environment *ev)
{
	eazel_install_set_silent (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_silent (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				CORBA_Environment *ev)
{
	return eazel_install_get_silent (servant->object);
}

static void
impl_Eazel_Install__set_test_mode (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				   const CORBA_boolean value,
				   CORBA_Environment *ev)
{
	eazel_install_set_test (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_test_mode (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				   CORBA_Environment *ev)
{
	return eazel_install_get_test (servant->object);
}


static void
impl_Eazel_Install__set_force (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			       const CORBA_boolean value,
			       CORBA_Environment *ev)
{
	eazel_install_set_force (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_force (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			       CORBA_Environment *ev)
{
	return eazel_install_get_force (servant->object);
}

static void
impl_Eazel_Install__set_ei2 (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			       const CORBA_boolean value,
			       CORBA_Environment *ev)
{
	eazel_install_set_ei2 (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_ei2 (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			       CORBA_Environment *ev)
{
	return eazel_install_get_ei2 (servant->object);
}

static void
impl_Eazel_Install__set_auth (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			       const CORBA_boolean value,
			       CORBA_Environment *ev)
{
	eazel_install_set_eazel_auth (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_auth (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
			       CORBA_Environment *ev)
{
	return eazel_install_get_eazel_auth (servant->object);
}

static void
impl_Eazel_Install__set_update (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				const CORBA_boolean value,
				CORBA_Environment *ev)
{
	eazel_install_set_update (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_update (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				CORBA_Environment *ev)
{
	return eazel_install_get_update (servant->object);
}

static void
impl_Eazel_Install__set_downgrade (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				   const CORBA_boolean value,
				   CORBA_Environment *ev)
{
	eazel_install_set_downgrade (servant->object, value);
}

static CORBA_boolean
impl_Eazel_Install__get_downgrade (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				   CORBA_Environment *ev)
{
	return eazel_install_get_downgrade (servant->object);
}

static void
impl_Eazel_Install__set_tmp_dir (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 const CORBA_char *value,
				 CORBA_Environment *ev)
{
	eazel_install_set_tmp_dir (servant->object, value);
}


static CORBA_char*
impl_Eazel_Install__get_tmp_dir (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 CORBA_Environment *ev)
{
	return eazel_install_get_tmp_dir (servant->object);
}

static void
impl_Eazel_Install__set_ssl_rename (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				    const CORBA_boolean value,
				    CORBA_Environment *ev)
{
	eazel_install_set_ssl_rename (servant->object, value);
}


static CORBA_boolean
impl_Eazel_Install__get_ssl_rename (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				    CORBA_Environment *ev)
{
	return eazel_install_get_ssl_rename (servant->object);
}

static void
impl_Eazel_Install__set_ignore_file_conflicts (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
					       const CORBA_boolean value,
					       CORBA_Environment *ev)
{
	eazel_install_set_ignore_file_conflicts (servant->object, value);
}


static CORBA_boolean
impl_Eazel_Install__get_ignore_file_conflicts (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
					       CORBA_Environment *ev)
{
	return eazel_install_get_ignore_file_conflicts (servant->object);
}

static void
impl_Eazel_Install__set_server (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				const CORBA_char *value,
				CORBA_Environment *ev)
{
	eazel_install_set_server (servant->object, value);
}


static CORBA_char*
impl_Eazel_Install__get_server (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				CORBA_Environment *ev)
{
	return eazel_install_get_server (servant->object);
}

static void
impl_Eazel_Install__set_cgi (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				const CORBA_char *value,
				CORBA_Environment *ev)
{
	eazel_install_set_cgi_path (servant->object, value);
}


static CORBA_char*
impl_Eazel_Install__get_cgi (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				CORBA_Environment *ev)
{
	return eazel_install_get_cgi_path (servant->object);
}

static void
impl_Eazel_Install__set_username (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				const CORBA_char *value,
				CORBA_Environment *ev)
{
	eazel_install_set_username (servant->object, value);
}


static CORBA_char*
impl_Eazel_Install__get_username (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				CORBA_Environment *ev)
{
	return eazel_install_get_username (servant->object);
}

static void
impl_Eazel_Install__set_server_port (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				     const CORBA_long value,
				     CORBA_Environment *ev)
{
	eazel_install_set_server_port (servant->object, value);
}


static CORBA_long
impl_Eazel_Install__get_server_port (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				     CORBA_Environment *ev)
{
	return eazel_install_get_server_port (servant->object);
}

static void
impl_Eazel_Install__set_log_file (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				  const CORBA_char *value,
				  CORBA_Environment *ev)
{
	eazel_install_open_log (servant->object, value);
}


static CORBA_char*
impl_Eazel_Install__get_log_file (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				  CORBA_Environment *ev)
{
	return servant->object->private->logfilename;
}

static void
impl_Eazel_Install__set_package_list (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				      const CORBA_char *value,
				      CORBA_Environment *ev)
{
	eazel_install_set_package_list (servant->object, value);
}


static CORBA_char*
impl_Eazel_Install__get_package_list (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				      CORBA_Environment *ev)
{
	return eazel_install_get_package_list (servant->object); 
}

static void
impl_Eazel_Install__set_protocol (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				  const GNOME_Trilobite_Eazel_ProtocolEnum value,
				  CORBA_Environment *ev)
{
	switch (value) {
	case GNOME_Trilobite_Eazel_PROTOCOL_HTTP:
		eazel_install_set_protocol (servant->object, PROTOCOL_HTTP);
		break;
	case GNOME_Trilobite_Eazel_PROTOCOL_FTP:
		eazel_install_set_protocol (servant->object, PROTOCOL_FTP);
		break;
	case GNOME_Trilobite_Eazel_PROTOCOL_LOCAL:
		eazel_install_set_protocol (servant->object, PROTOCOL_LOCAL);
		break;
	}
}


static GNOME_Trilobite_Eazel_ProtocolEnum
impl_Eazel_Install__get_protocol (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				  CORBA_Environment *ev)
{
	switch (eazel_install_get_protocol (servant->object)) {
	default:
	case PROTOCOL_HTTP:
		return GNOME_Trilobite_Eazel_PROTOCOL_HTTP;
		break;
	case PROTOCOL_FTP:
		return GNOME_Trilobite_Eazel_PROTOCOL_FTP;
		break;
	case PROTOCOL_LOCAL:
		return GNOME_Trilobite_Eazel_PROTOCOL_LOCAL;
		break;
	}
}

static GNOME_Trilobite_Eazel_PackageDataStructList*
impl_Eazel_Install_simple_query (impl_POA_GNOME_Trilobite_Eazel_Install *servant,
				 const CORBA_char *query,
				 const CORBA_char *root,
				 CORBA_Environment *ev)
{
	GNOME_Trilobite_Eazel_PackageDataStructList *result;
	GList *tmp_result;

	g_free (servant->object->private->cur_root);
	if (!root || strcmp (root, "")==0) {
		servant->object->private->cur_root = NULL;
	} else {
		servant->object->private->cur_root = g_strdup (root);
	}

	tmp_result = eazel_install_query_package_system (servant->object, 
							 query, 
							 EI_SIMPLE_QUERY_MATCHES, 
							 servant->object->private->cur_root);
	result = corba_packagedatastructlist_from_packagedata_list (tmp_result);

	g_list_foreach (tmp_result, (GFunc)gtk_object_unref, NULL);
	
	return result;
}

POA_GNOME_Trilobite_Eazel_Install__epv* 
eazel_install_get_epv () 
{
	POA_GNOME_Trilobite_Eazel_Install__epv *epv;

	epv = g_new0 (POA_GNOME_Trilobite_Eazel_Install__epv, 1);
	epv->install            = (gpointer)&impl_Eazel_Install_install;
	epv->uninstall          = (gpointer)&impl_Eazel_Install_uninstall;
	epv->stop		= (gpointer)&impl_Eazel_Install_stop;
	epv->delete_files       = (gpointer)&impl_Eazel_Install_delete_files;
	epv->install_packages   = (gpointer)&impl_Eazel_Install_install_packages;
	epv->uninstall_packages = (gpointer)&impl_Eazel_Install_uninstall_packages;
	epv->revert_transaction = (gpointer)&impl_Eazel_Install_revert_transaction;

	epv->_set_verbose = (gpointer)&impl_Eazel_Install__set_verbose;
	epv->_get_verbose = (gpointer)&impl_Eazel_Install__get_verbose;

	epv->_set_debug = (gpointer)&impl_Eazel_Install__set_debug;
	epv->_get_debug = (gpointer)&impl_Eazel_Install__get_debug;

	epv->_set_silent = (gpointer)&impl_Eazel_Install__set_silent;
	epv->_get_silent = (gpointer)&impl_Eazel_Install__get_silent;

	epv->_set_test_mode = (gpointer)&impl_Eazel_Install__set_test_mode;
	epv->_get_test_mode = (gpointer)&impl_Eazel_Install__get_test_mode;

	epv->_set_force = (gpointer)&impl_Eazel_Install__set_force;
	epv->_get_force = (gpointer)&impl_Eazel_Install__get_force;

	epv->_set_ei2 = (gpointer)&impl_Eazel_Install__set_ei2;
	epv->_get_ei2 = (gpointer)&impl_Eazel_Install__get_ei2;

	epv->_set_auth = (gpointer)&impl_Eazel_Install__set_auth;
	epv->_get_auth = (gpointer)&impl_Eazel_Install__get_auth;

	epv->_set_update = (gpointer)&impl_Eazel_Install__set_update;
	epv->_get_update = (gpointer)&impl_Eazel_Install__get_update;

	epv->_set_downgrade = (gpointer)&impl_Eazel_Install__set_downgrade;
	epv->_get_downgrade = (gpointer)&impl_Eazel_Install__get_downgrade;

	epv->_set_protocol = (gpointer)&impl_Eazel_Install__set_protocol;
	epv->_get_protocol = (gpointer)&impl_Eazel_Install__get_protocol;

	epv->_set_server = (gpointer)&impl_Eazel_Install__set_server;
	epv->_get_server = (gpointer)&impl_Eazel_Install__get_server;

	epv->_set_cgi = (gpointer)&impl_Eazel_Install__set_cgi;
	epv->_get_cgi = (gpointer)&impl_Eazel_Install__get_cgi;

	epv->_set_username = (gpointer)&impl_Eazel_Install__set_username;
	epv->_get_username = (gpointer)&impl_Eazel_Install__get_username;

	epv->_set_server_port = (gpointer)&impl_Eazel_Install__set_server_port;
	epv->_get_server_port = (gpointer)&impl_Eazel_Install__get_server_port;

	epv->_set_log_file = (gpointer)&impl_Eazel_Install__set_log_file;
	epv->_get_log_file = (gpointer)&impl_Eazel_Install__get_log_file;

	epv->_set_package_list = (gpointer)&impl_Eazel_Install__set_package_list;
	epv->_get_package_list = (gpointer)&impl_Eazel_Install__get_package_list;

	epv->_set_tmp_dir = (gpointer)&impl_Eazel_Install__set_tmp_dir;
	epv->_get_tmp_dir = (gpointer)&impl_Eazel_Install__get_tmp_dir;

	epv->_set_ignore_file_conflicts = (gpointer)&impl_Eazel_Install__set_ignore_file_conflicts;
	epv->_get_ignore_file_conflicts = (gpointer)&impl_Eazel_Install__get_ignore_file_conflicts;

	epv->_set_ssl_rename = (gpointer)&impl_Eazel_Install__set_ssl_rename;
	epv->_get_ssl_rename = (gpointer)&impl_Eazel_Install__get_ssl_rename;

	epv->simple_query = (gpointer)&impl_Eazel_Install_simple_query;

	return epv;
};

GNOME_Trilobite_Eazel_Install
eazel_install_create_corba_object (BonoboObject *service) {
	impl_POA_GNOME_Trilobite_Eazel_Install *servant;
	CORBA_Environment ev;

	g_assert (service != NULL);
	g_assert (EAZEL_IS_INSTALL (service));
	
	CORBA_exception_init (&ev);
	
	servant = g_new0 (impl_POA_GNOME_Trilobite_Eazel_Install,1);
	servant->object = EAZEL_INSTALL (service);

	((POA_GNOME_Trilobite_Eazel_Install*) servant)->vepv = EAZEL_INSTALL_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;
	POA_GNOME_Trilobite_Eazel_Install__init (servant, &ev);
	ORBIT_OBJECT_KEY (((POA_GNOME_Trilobite_Eazel_Install*)servant)->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Eazel_Install corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	/* Return the bonobo activation of the servant */
	return (GNOME_Trilobite_Eazel_Install) bonobo_object_activate_servant (service, servant);
}
#endif /* EAZEL_INSTALL_NO_CORBA */

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
 * mozilla-components.cpp - A small C wrapper for using mozilla components
 */

#define nopeDEBUG_ramiro 1

#include <config.h>

#include "mozilla-components.h"

#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsILocalFile.h"

#define LOCAL_FILE_ID NS_LOCAL_FILE_CONTRACTID

extern "C" gboolean
mozilla_components_register_library (const char *class_uuid,
				     const char *library_file_name,
				     const char *class_name,
				     const char *prog_id)
{
	g_return_val_if_fail (class_uuid != NULL, FALSE);
	g_return_val_if_fail (library_file_name != NULL, FALSE);
	g_return_val_if_fail (class_name != NULL, FALSE);
	g_return_val_if_fail (prog_id != NULL, FALSE);
	
	nsCOMPtr<nsILocalFile> spec;

	nsresult rv = nsComponentManager::CreateInstance (LOCAL_FILE_ID, 
							  nsnull, 
							  NS_GET_IID (nsILocalFile), 
							  getter_AddRefs (spec));
	
	if (NS_FAILED (rv) || (!spec)) {
#ifdef DEBUG_ramiro
		g_warning ("create nsILocalFile failed");
#endif
		return FALSE;
	}

	rv = spec->InitWithPath (library_file_name);
	
	if (NS_FAILED (rv)) {
#ifdef DEBUG_ramiro
		g_warning ("init with path failed");
#endif
		return FALSE;
	}

	nsID cid;

	PRBool parse_rv = cid.Parse (class_uuid);

	if (!parse_rv) {
#ifdef DEBUG_ramiro
		g_warning ("Parsing class_uuid '%s' failed", class_uuid);
#endif
		return FALSE;
	}


	rv = nsComponentManager::RegisterComponentSpec (cid,
							class_name,
							prog_id,
							spec,
							PR_TRUE,
							PR_FALSE);

	if (NS_FAILED (rv)) {
#ifdef DEBUG_ramiro
		g_warning ("Failed to register component (%s,%s,%s,%s)",
			   class_uuid,
			   class_name,
			   prog_id,
			   library_file_name);
#endif
		return FALSE;
	}

#ifdef DEBUG_ramiro
	g_print ("Successful registration of component (%s,%s,%s,%s)",
		 class_uuid,
		 class_name,
		 prog_id,
		 library_file_name);
#endif
	
	return TRUE;
}

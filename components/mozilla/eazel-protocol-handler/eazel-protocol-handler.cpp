/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * eazel-protocol-handler.{cpp,h} is based on the finger protocol
 * handler written by bryner@netscape.com.
 *
 * The Initial Developer of the Original Code is Eazel, Inc.
 * Portions created by Eazel are Copyright (C) 2000 Eazel, Inc.
 * All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@eazel.com>
 *   Ramiro Estrugo <ramiro@eazel.com>
 *
 */

/* This is a really dumb cut-n-paste job of bryner's finger protocol.  Its
 * sad really, that its needed simply to let mozilla know about the eazel: 
 * protocol.
 *
 * When Mozilla M18 is released, there will be a better way to do this component
 * will be obsolete and trahsed.
 */

#include "nspr.h"
#include "eazel-protocol-channel.h"
#include "eazel-protocol-handler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);


eazelProtocolHandler::eazelProtocolHandler() {
	NS_INIT_REFCNT();
}

eazelProtocolHandler::~eazelProtocolHandler() {
}

NS_IMPL_ISUPPORTS(eazelProtocolHandler, NS_GET_IID(nsIProtocolHandler));

NS_METHOD
eazelProtocolHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {

	eazelProtocolHandler* ph = new eazelProtocolHandler();
	if (ph == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(ph);
	nsresult rv = ph->QueryInterface(aIID, aResult);
	NS_RELEASE(ph);
	return rv;
}
    
// nsIProtocolHandler methods:
NS_IMETHODIMP
eazelProtocolHandler::GetScheme(char* *result) {
	*result = nsCRT::strdup("eazel");
	if (!*result) return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

NS_IMETHODIMP
eazelProtocolHandler::GetDefaultPort(PRInt32 *result) {
	// We dont got no stinkin port
	*result = -1;
	return NS_OK;
}

NS_IMETHODIMP
eazelProtocolHandler::NewURI (const char *aSpec,
			      nsIURI *aBaseURI,
			      nsIURI **result) {
	nsresult rv;

	// no concept of a relative eazel url
	NS_ASSERTION(!aBaseURI, "base url passed into eazel protocol handler");
	
	nsIURI* url;
	rv = nsComponentManager::CreateInstance (kSimpleURICID, nsnull,
						 NS_GET_IID(nsIURI),
						 (void**)&url);
	if (NS_FAILED (rv)) {
		return rv;
	}
	
	rv = url->SetSpec((char*)aSpec);

	if (NS_FAILED (rv)) {
		NS_RELEASE(url);
		return rv;
	}
	
	*result = url;

	return rv;
}

NS_IMETHODIMP
eazelProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
	nsresult rv;
    
	eazelChannel* channel;
	rv = eazelChannel::Create(nsnull, NS_GET_IID(nsIChannel), (void**)&channel);
	if (NS_FAILED (rv)) return rv;

	rv = channel->Init(url);
	if (NS_FAILED (rv)) {
		NS_RELEASE(channel);
		return rv;
	}

	*result = channel;
	return NS_OK;
}

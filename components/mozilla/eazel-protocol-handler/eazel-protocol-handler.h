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

// The eazel protocol handler creates "eazel" URIs of the form
// "eazel:something".  Its a very dumb protocol handler which 
// is really just needed so that these special up uris can
// be intercepted in the mozilla component.  When this happens,
// loading of these uri is aborted, and the uri is grokked by
// Nautilus itself.

#ifndef EAZEL_PROTOCOL_HANDLER_H
#define EAZEL_PROTOCOL_HANDLER_H

#include "nsIProtocolHandler.h"

#define EAZEL_PORT 79

// {fb21b992-1dd1-11b2-aab4-906384831dd4}
#define NS_EAZELHANDLER_CID     \
{ 0xfb21b992, 0x1dd1, 0x11b2, \
   {0xaa, 0xb4, 0x90, 0x63, 0x84, 0x83, 0x1d, 0xd4} }

class eazelProtocolHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER

    // eazelProtocolHandler methods:
    eazelProtocolHandler();
    virtual ~eazelProtocolHandler();

    // Define a Create method to be used with a factory:
    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
};

#endif // EAZEL_PROTOCOL_HANDLER_H


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
 * eazel-protocol-module.cpp is based on the finger protocol
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

#include "nsIGenericFactory.h"
#include "eazel-protocol-handler.h"

static nsModuleComponentInfo gResComponents[] = {
    { "The Eazel Protocol Handler", 
      NS_EAZELHANDLER_CID,
      NS_NETWORK_PROTOCOL_PROGID_PREFIX "eazel",
      eazelProtocolHandler::Create
    },
    { "The Eazel Protocol Handler", 
      NS_EAZELHANDLER_CID,
      NS_NETWORK_PROTOCOL_PROGID_PREFIX "eazel-summary",
      eazelProtocolHandler::Create
    },
};

NS_IMPL_NSGETMODULE("eazel", gResComponents)

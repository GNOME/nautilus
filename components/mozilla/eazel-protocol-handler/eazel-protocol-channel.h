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
 * eazel-protocol-channel.{cpp,h} is based on the finger protocol
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

#ifndef EAZEL_PROTOCOL_CHANNEL_H
#define EAZEL_PROTOCOL_CHANNEL_H

#include "nsString.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "eazel-protocol-handler.h"
#include "nsIStreamListener.h"


class eazelChannel : public nsIChannel, public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMOBSERVER

    // eazelChannel methods:
    eazelChannel();
    virtual ~eazelChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIURI* uri);

protected:
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mUrl;
    nsCOMPtr<nsIStreamListener>         mListener;
    PRUint32                            mLoadAttributes;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCString                           mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsISupports>               mOwner; 
    PRUint32                            mBufferSegmentSize;
    PRUint32                            mBufferMaxSize;
    PRBool                              mActAsObserver;

    PRInt32                             mPort;
    nsXPIDLCString                      mHost;
    nsXPIDLCString                      mUser;

    nsXPIDLCString                      mRequest;

    nsCOMPtr<nsISupports>               mResponseContext;
    nsCOMPtr<nsIChannel>                mTransport;
    nsresult                            mStatus;

protected:
    nsresult SendRequest(nsIChannel* aChannel);
};

#endif // EAZEL_PROTOCOL_CHANNEL_H

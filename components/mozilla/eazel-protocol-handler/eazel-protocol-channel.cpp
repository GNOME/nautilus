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

/* This is a really dumb cut-n-paste job of bryner's finger protocol.  Its
 * sad really, that its needed simply to let mozilla know about the eazel: 
 * protocol.
 *
 * When Mozilla M18 is released, there will be a better way to do this component
 * will be obsolete and trahsed.
 */

#include "eazel-protocol-channel.h"
#include "nsIServiceManager.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsXPIDLString.h"
#include "nsISocketTransportService.h"
#include "nsIStringStream.h"
#include "nsMimeTypes.h"
#include "nsIStreamConverterService.h"
#include "nsITXTToHTMLConv.h"

#define DEBUG_bryner

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

#define BUFFER_SEG_SIZE (4*1024)
#define BUFFER_MAX_SIZE (64*1024)

// eazelChannel methods
eazelChannel::eazelChannel()
    : mContentLength(-1),
      mActAsObserver(PR_TRUE),
      mPort(-1),
      mStatus(NS_OK)
{
    NS_INIT_REFCNT();
}

eazelChannel::~eazelChannel() {
}

NS_IMPL_THREADSAFE_ISUPPORTS4(eazelChannel, nsIChannel, nsIRequest,
                              nsIStreamListener, nsIStreamObserver)

    nsresult
eazelChannel::Init(nsIURI* uri)
{
    nsresult rv;
    nsXPIDLCString autoBuffer;

    NS_ASSERTION(uri, "no uri");

    mUrl = uri;

//  For security reasons, we do not allow the user to specify a
//  non-default port for eazel: URL's.

    mPort = EAZEL_PORT;

    rv = mUrl->GetPath(getter_Copies(autoBuffer)); // autoBuffer = user@host
    if (NS_FAILED (rv)) return rv;

    nsCString cString(autoBuffer);
    nsCString tempBuf;

    PRUint32 i;

    // Now parse out the user and host
    for (i=0; cString[i] != '\0'; i++) {
        if (cString[i] == '@') {
            cString.Left(tempBuf, i);
            mUser = tempBuf;
            cString.Right(tempBuf, cString.Length() - i - 1);
            mHost = tempBuf;
            break;
        }
    }

    // Catch the case of just the host being given

    if (cString[i] == '\0') {
        mHost = cString;
    }

#ifdef DEBUG_bryner
    printf("Status:mUser = %s, mHost = %s\n", (const char*)mUser,
           (const char*)mHost);
#endif
    if (!*(const char *)mHost) return NS_ERROR_NOT_INITIALIZED;

    return NS_OK;
}

NS_METHOD
eazelChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    eazelChannel* fc = new eazelChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

// nsIRequest methods:

NS_IMETHODIMP
eazelChannel::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("eazelChannel::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::IsPending(PRBool *result)
{
    NS_NOTREACHED("eazelChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED (status), "shouldn't cancel with a success code");
    nsresult rv = NS_ERROR_FAILURE;

    mStatus = status;
    if (mTransport) {
        rv = mTransport->Cancel(status);
    }
    return rv;
}

NS_IMETHODIMP
eazelChannel::Suspend(void)
{
    NS_NOTREACHED("eazelChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::Resume(void)
{
    NS_NOTREACHED("eazelChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIChannel methods:

NS_IMETHODIMP
eazelChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mUrl;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetURI(nsIURI* aURI)
{
    mUrl = aURI;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::OpenInputStream(nsIInputStream **_retval)
{
    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsISocketTransportService, socketService, kSocketTransportServiceCID, &rv);
    if (NS_FAILED (rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = socketService->CreateTransport(mHost, mPort, nsnull, -1, BUFFER_SEG_SIZE,
                                        BUFFER_MAX_SIZE, getter_AddRefs(channel));
    if (NS_FAILED (rv)) return rv;

    rv = channel->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED (rv)) return rv;

    return channel->OpenInputStream(_retval);
}

NS_IMETHODIMP
eazelChannel::OpenOutputStream(nsIOutputStream **_retval)
{
    NS_NOTREACHED("eazelChannel::OpenOutputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::AsyncRead(nsIStreamListener *aListener, nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsISocketTransportService, socketService, kSocketTransportServiceCID, &rv);
    if (NS_FAILED (rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = socketService->CreateTransport(mHost, mPort, nsnull, -1, BUFFER_SEG_SIZE,
                                        BUFFER_MAX_SIZE, getter_AddRefs(channel));
    if (NS_FAILED (rv)) return rv;

    rv = channel->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED (rv)) return rv;

    mListener = aListener;
    mResponseContext = ctxt;
    mTransport = channel;

    return SendRequest(channel);
}

NS_IMETHODIMP
eazelChannel::AsyncWrite(nsIInputStream *fromStream,
                         nsIStreamObserver *observer,
                         nsISupports *ctxt)
{
    NS_NOTREACHED("eazelChannel::AsyncWrite");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::GetContentType(char* *aContentType) {
    if (!aContentType) return NS_ERROR_NULL_POINTER;

    *aContentType = nsCRT::strdup (TEXT_HTML);
    if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetContentType(const char *aContentType)
{
    //It doesn't make sense to set the content-type on this type
    // of channel...
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
eazelChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("eazelChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    NS_NOTREACHED("eazelChannel::GetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    NS_NOTREACHED("eazelChannel::SetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    NS_NOTREACHED("eazelChannel::GetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::SetTransferCount(PRInt32 aTransferCount)
{
    NS_NOTREACHED("eazelChannel::SetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    NS_NOTREACHED("eazelChannel::GetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    NS_NOTREACHED("eazelChannel::SetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    NS_NOTREACHED("eazelChannel::GetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    NS_NOTREACHED("eazelChannel::SetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetLocalFile(nsIFile* *file)
{
    *file = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
eazelChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
eazelChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
eazelChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    return NS_OK;
}

NS_IMETHODIMP 
eazelChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

// nsIStreamObserver methods
NS_IMETHODIMP
eazelChannel::OnStartRequest(nsIChannel *aChannel, nsISupports *aContext) {
    if (!mActAsObserver) {
        // acting as a listener
        return mListener->OnStartRequest(this, aContext);
    } else {
        // we don't want to pass our AsyncWrite's OnStart through
        // we just ignore this
        return NS_OK;
    }
}


NS_IMETHODIMP
eazelChannel::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext,
                            nsresult aStatus, const PRUnichar* aStatusArg)
{
#ifdef DEBUG_bryner
    printf("eazelChannel::OnStopRequest, mActAsObserver=%d\n",
           mActAsObserver);
    printf("  aChannel = %p\n", aChannel);
#endif
    nsresult rv = NS_OK;

    if (NS_FAILED (aStatus) || !mActAsObserver) {
        if (mLoadGroup) {
            rv = mLoadGroup->RemoveChannel(this, nsnull, aStatus, aStatusArg);
            if (NS_FAILED (rv)) return rv;
        }
        rv = mListener->OnStopRequest(this, aContext, aStatus, aStatusArg);
        mTransport = 0;
        return rv;
    } else {
        // at this point we know the request has been sent.
        // we're no longer acting as an observer.
 
        mActAsObserver = PR_FALSE;
        nsCOMPtr<nsIStreamListener> converterListener;

        NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService,
                        kStreamConverterServiceCID, &rv);
        if (NS_FAILED (rv)) return rv;

        nsAutoString fromStr; fromStr.AssignWithConversion("text/plain");
        nsAutoString toStr; toStr.AssignWithConversion("text/html");

        rv = StreamConvService->AsyncConvertData(fromStr.GetUnicode(),
                                                 toStr.GetUnicode(), this, mResponseContext,
                                                 getter_AddRefs(converterListener));
        if (NS_FAILED (rv)) return rv;

        nsCOMPtr<nsITXTToHTMLConv> converter(do_QueryInterface(converterListener));
        if (converter) {
            nsAutoString title; title.AssignWithConversion("Eazel information for ");
            nsXPIDLCString userHost;
            rv = mUrl->GetPath(getter_Copies(userHost));
            title.AppendWithConversion(userHost);
            converter->SetTitle(title.GetUnicode());
            converter->PreFormatHTML(PR_TRUE);
        }

        return aChannel->AsyncRead(converterListener, mResponseContext);
    }

}


// nsIStreamListener method
NS_IMETHODIMP
eazelChannel::OnDataAvailable(nsIChannel* aChannel, nsISupports* aContext,
                              nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                              PRUint32 aLength) {
    mContentLength = aLength;
    return mListener->OnDataAvailable(this, aContext, aInputStream, aSourceOffset, aLength);
}

nsresult
eazelChannel::SendRequest(nsIChannel* aChannel) {
    // The text to send should already be in mUser

    nsresult rv = NS_OK;
    nsCOMPtr<nsISupports> result;
    nsCOMPtr<nsIInputStream> charstream;
    nsCString requestBuffer(mUser);

    if (mLoadGroup) {
        mLoadGroup->AddChannel(this, nsnull);
    }

    requestBuffer.Append(CRLF);

    mRequest = requestBuffer.ToNewCString();

    rv = NS_NewCharInputStream(getter_AddRefs(result), mRequest);
    if (NS_FAILED (rv)) return rv;

    charstream = do_QueryInterface(result, &rv);
    if (NS_FAILED (rv)) return rv;

#ifdef DEBUG_bryner
    printf("Sending: %s\n", requestBuffer.GetBuffer());
#endif

    rv = aChannel->SetTransferCount(requestBuffer.Length());
    if (NS_FAILED (rv)) return rv;
    rv = aChannel->AsyncWrite(charstream, this, 0);
    return rv;
}



/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <WWWCore.h>
#include <WWWStream.h>
#include <WWWTrans.h>
#include <WWWHTTP.h>
#include <WWWMIME.h>
#include <WWWFTP.h>
#include <WWWFile.h>
#include <WWWGophe.h>
#include <WWWZip.h>

/* clean up the cpp namespace -- libwww is particularly dirty */
#undef PACKAGE
#undef VERSION
#undef _

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include "glibwww.h"

/* For some reason, the folks at Sun are missing this declaration.
 * Rather than figure out why, I just decided to add it here.
 */
extern HTCoder HTZLib_inflate;

#ifndef FTP_PORT
#define FTP_PORT        21
#endif
#ifndef HTTP_PORT
#define HTTP_PORT       80
#endif
/*#include "WWWMIME.h"*/

static void HTMIMEInit (void);

static void glibwww_parse_proxy_env (void);

static int HTProxyFilter       (HTRequest *request, void *param, int status);
static int HTRedirectFilter    (HTRequest *request, HTResponse *response,
				void *param, int status);
static int HTCredentialsFilter (HTRequest *request, void *param, int status);
static int HTAuthFilter        (HTRequest *request, HTResponse *response,
				void *param, int status);
static int HTAuthInfoFilter    (HTRequest *request, HTResponse *response,
				void *param, int status);

static gboolean exitfunc = FALSE;

void
glibwww_init(const gchar *appName, const gchar *appVersion)
{
	if (!HTLib_isInitialized())
		HTLibInit(appName, appVersion);
	/*HTAlertInit();
	  HTAlert_setInteractive(NO);*/

	HTTransport_add("tcp",         HT_TP_SINGLE,HTReader_new,HTWriter_new);
	HTTransport_add("buffered_tcp",HT_TP_SINGLE,HTReader_new,HTBufferWriter_new);
	HTTransport_add("local",       HT_TP_SINGLE,HTReader_new,HTWriter_new);

	HTProtocol_add("ftp",  "tcp",          FTP_PORT,  NO, HTLoadFTP,  NULL);
	HTProtocol_add("http", "buffered_tcp", HTTP_PORT, NO, HTLoadHTTP, NULL);
	HTProtocol_add("file", "local",        0,         NO, HTLoadFile, NULL);

	HTNet_setMaxSocket(6);

	HTNet_addBefore(HTCredentialsFilter,        "http://*",     NULL, HT_FILTER_LATE);
	HTNet_addBefore(HTProxyFilter,              NULL,           NULL, HT_FILTER_LATE);
	HTNet_addAfter(HTAuthFilter,        "http://*",     NULL, HT_NO_ACCESS,     
		       HT_FILTER_MIDDLE);
	HTNet_addAfter(HTAuthFilter,        "http://*",     NULL, HT_REAUTH,        
		       HT_FILTER_MIDDLE);
	HTNet_addAfter(HTRedirectFilter,    "http://*",     NULL, HT_PERM_REDIRECT, 
		       HT_FILTER_MIDDLE);
	HTNet_addAfter(HTRedirectFilter,    "http://*",     NULL, HT_FOUND,         
		       HT_FILTER_MIDDLE);
	HTNet_addAfter(HTRedirectFilter,    "http://*",     NULL, HT_SEE_OTHER,     
		       HT_FILTER_MIDDLE);
	HTNet_addAfter(HTRedirectFilter,    "http://*",     NULL, HT_TEMP_REDIRECT, 
		       HT_FILTER_MIDDLE);
	HTNet_addAfter(HTAuthInfoFilter,    "http://*",     NULL, HT_ALL,           
		       HT_FILTER_MIDDLE);
  
	HTAA_newModule ("basic", HTBasic_generate, HTBasic_parse, NULL,
			HTBasic_delete);

	glibwww_parse_proxy_env();
	/* set proxy from our config files ... */
	/* glibwww_add_proxy("http", ...); */
	/* glibwww_add_proxy("ftp", ...); */

	HTMIME_setSaveStream (HTSaveLocally);

	HTFormat_addConversion("message/rfc822", "*/*", HTMIMEConvert,
			       1.0, 0.0, 0.0);
	HTFormat_addConversion("message/x-rfc822-foot", "*/*", HTMIMEFooter,
			       1.0, 0.0, 0.0);
	HTFormat_addConversion("message/x-rfc822-head", "*/*", HTMIMEHeader,
			       1.0, 0.0, 0.0);
	HTFormat_addConversion("message/x-rfc822-cont", "*/*", HTMIMEContinue,
			       1.0, 0.0, 0.0);
	HTFormat_addConversion("message/x-rfc822-upgrade", "*/*", HTMIMEUpgrade,
			       1.0, 0.0, 0.0);
	HTFormat_addConversion("message/x-rfc822-partial", "*/*", HTMIMEPartial,
			       1.0, 0.0, 0.0);

	HTFormat_addConversion("text/x-http", "*/*", HTTPStatus_new,
			       1.0, 0.0, 0.0);

	HTFormat_addCoding("*", HTIdentityCoding, HTIdentityCoding, 0.3);

	HTFormat_addTransferCoding("deflate", NULL, HTZLib_inflate, 1.0);
	HTFormat_addTransferCoding("chunked", HTChunkedEncoder,HTChunkedDecoder,1.0);

	HTMIMEInit();

	HTFileInit();
  
	HTHost_setEventTimeout(30000);

	HTFTP_setTransferMode(FTP_BINARY_TRANSFER_MODE);

	glibwww_register_callbacks();

	if (!exitfunc)
		g_atexit(glibwww_cleanup);
	exitfunc = TRUE;

	/*WWWTRACE = SHOW_MEM_TRACE;*/
}

void glibwww_cleanup(void) {
	if (HTLib_isInitialized()) {
		HTFormat_deleteAll();
		HTLibTerminate();
	}
}

struct ProxyEntry {
	gchar *protocol;
	gchar *proxy;
};

static GList *proxies = NULL;
static GList *noproxy = NULL;

void
glibwww_add_proxy(const gchar *protocol, const gchar *proxy)
{
	GList *tmp;
	struct ProxyEntry *ent;

	for (tmp = proxies; tmp; tmp = tmp->next) {
		ent = tmp->data;
		if (!g_strcasecmp(protocol, ent->protocol)) {
			g_free(ent->proxy);
			ent->proxy = g_strdup(proxy);
			return;
		}
	}
	ent = g_new(struct ProxyEntry, 1);
	ent->protocol = g_strdup(protocol);
	ent->proxy = g_strdup(proxy);
	proxies = g_list_prepend(proxies, ent);
}

void
glibwww_add_noproxy(const gchar *host)
{
	noproxy = g_list_prepend(noproxy, g_strdup(host));
}

static const gchar *
glibwww_get_proxy(const gchar *url)
{
	gchar *protocol;
	GList *tmp;

	if (!url || !proxies)
		return NULL;
	if (noproxy) {
		char *host = HTParse(url, "", PARSE_HOST);
		char *ptr = strchr(host, ':');

		if (ptr != NULL) *ptr = ':';
		for (tmp = noproxy; tmp; tmp = tmp->next) {
			char *nophost = tmp->data;
			char *np = nophost + strlen(nophost);
			char *hp = host + strlen(host);

			while (np>=nophost && hp>=host && (*np--==*hp--))
				;
			if (np==nophost-1 && (hp==host-1 || *hp=='.'))
				return NULL;
		}
	}
	protocol = HTParse(url, "", PARSE_ACCESS);
	for (tmp = proxies; tmp; tmp = tmp->next) {
		struct ProxyEntry *ent = tmp->data;

		if (!g_strcasecmp(ent->protocol, protocol)) {
			HT_FREE(protocol);
			return ent->proxy;
		}
	}
	HT_FREE(protocol);
	return NULL;
}

static void
glibwww_parse_proxy_env(void)
{
	static const char *protocollist[] = {
		"http",
		"ftp",
		"news",
		"wais",
		"gopher",
		NULL
	};
	const char **prot = protocollist;
	char *nop;

	for (prot = protocollist; *prot != NULL; prot++) {
		gchar *var = g_strconcat(*prot, "_proxy", NULL);
		gchar *proxy = g_getenv(var);

		if (proxy && proxy[0])
			glibwww_add_proxy(*prot, proxy);
		else {
			gchar *up = var;
			while ((*up = TOUPPER(*up))) up++;
			if ((proxy = g_getenv(var)) != NULL && proxy[0])
				glibwww_add_proxy(*prot, proxy);
		}
		g_free(var);
	}
	nop = g_getenv("no_proxy");
	if (nop && nop[0]) {
		char *str = g_strdup(nop);
		char *ptr = str;
		char *name;

		while ((name = HTNextField(&ptr)) != NULL)
			glibwww_add_noproxy(name);
		g_free(str);
	}
}

/* mime initialisation */
static void
HTMIMEInit(void)
{
	struct {
		char *string;
		HTParserCallback *pHandler;
	} fixedHandlers[] = {
		{"accept", &HTMIME_accept}, 
		{"accept-charset", &HTMIME_acceptCharset}, 
		{"accept-encoding", &HTMIME_acceptEncoding}, 
		{"accept-language", &HTMIME_acceptLanguage}, 
		{"accept-ranges", &HTMIME_acceptRanges}, 
		{"authorization", NULL},
		{"cache-control", &HTMIME_cacheControl},
		{"connection", &HTMIME_connection}, 
		{"content-encoding", &HTMIME_contentEncoding}, 
		{"content-length", &HTMIME_contentLength}, 
		{"content-range", &HTMIME_contentRange},
		{"content-transfer-encoding", &HTMIME_contentTransferEncoding}, 
		{"content-type", &HTMIME_contentType},
		{"digest-MessageDigest", &HTMIME_messageDigest}, 
		{"keep-alive", &HTMIME_keepAlive}, 
		{"link", &HTMIME_link},
		{"location", &HTMIME_location},
		{"max-forwards", &HTMIME_maxForwards}, 
		{"mime-version", NULL}, 
		{"pragma", &HTMIME_pragma},
		{"protocol", &HTMIME_protocol},
		{"protocol-info", &HTMIME_protocolInfo},
		{"protocol-request", &HTMIME_protocolRequest},
		{"proxy-authenticate", &HTMIME_authenticate},
		{"proxy-authorization", &HTMIME_proxyAuthorization},
		{"public", &HTMIME_public},
		{"range", &HTMIME_range},
		{"referer", &HTMIME_referer},
		{"retry-after", &HTMIME_retryAfter}, 
		{"server", &HTMIME_server}, 
		{"trailer", &HTMIME_trailer},
		{"transfer-encoding", &HTMIME_transferEncoding}, 
		{"upgrade", &HTMIME_upgrade},
		{"user-agent", &HTMIME_userAgent},
		{"vary", &HTMIME_vary},
		{"via", &HTMIME_via},
		{"warning", &HTMIME_warning},
		{"www-authenticate", &HTMIME_authenticate}, 
		{"authentication-info", &HTMIME_authenticationInfo},
		{"proxy-authentication-info", &HTMIME_proxyAuthenticationInfo}
	};
	int i;

	for (i = 0; i < sizeof(fixedHandlers)/sizeof(fixedHandlers[0]); i++)
		HTHeader_addParser(fixedHandlers[i].string, NO, fixedHandlers[i].pHandler);
}

/* the following filters are from HTFilter.c: */
static int
HTProxyFilter(HTRequest *request, void *param, int status)
{
	HTParentAnchor *anchor = HTRequest_anchor(request);
	char *addr = HTAnchor_physical(anchor);
	const char *physical = NULL;

	if ((physical = glibwww_get_proxy(addr))) {
		HTRequest_setFullURI(request, YES);
		HTRequest_setProxy(request, physical);
	} else {
		HTRequest_setFullURI(request, NO);
		HTRequest_deleteProxy(request);
	}
	return HT_OK;
}

static int
HTRedirectFilter(HTRequest *request, HTResponse *response,
		 void *param, int status)
{
	HTMethod method = HTRequest_method(request);
	HTAnchor *new_anchor = HTResponse_redirection(response);

	if (!new_anchor)
		return HT_OK;

	if (!HTMethod_isSafe(method))
		return HT_OK;

	HTRequest_deleteCredentialsAll(request);
	if (HTRequest_doRetry(request)) {
		HTRequest_setAnchor(request, new_anchor);
		HTLoad(request, NO);
	} else {
		HTRequest_addError(request, ERR_FATAL, NO, HTERR_MAX_REDIRECT,
				   NULL, 0, "HTRedirectFilter");
		return HT_OK;
	}
	return HT_ERROR;
}

static int
HTCredentialsFilter(HTRequest *request, void *param, int status)
{
	if (HTAA_beforeFilter(request, param, status) == HT_OK)
		return HT_OK;
	else {
		HTRequest_addError(request, ERR_FATAL, NO, HTERR_UNAUTHORIZED,
				   NULL, 0, "HTCredentialsFilter");
		return HT_ERROR;
	}
}

static int
HTAuthFilter(HTRequest *request, HTResponse *response,
	     void *param, int status)
{
	if (HTAA_afterFilter(request, response, param, status) == HT_OK) {
		HTLoad(request, NO);
		return HT_ERROR;
	}
	return HT_OK;
}
static int
HTAuthInfoFilter(HTRequest *request, HTResponse *response,
		 void *param, int status)
{
	if (!HTResponse_challenge(response))
		return HT_OK;
	else if (HTAA_updateFilter(request, response, param, status) == HT_OK)
		return HT_OK;
	else
		return HT_ERROR;
}


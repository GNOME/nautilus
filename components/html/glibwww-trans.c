/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <WWWCore.h>
#include <WWWStream.h>
#include <WWWTrans.h>

#undef PACKAGE
#undef VERSION
#undef _

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "glibwww.h"
#include <stdio.h>

static gboolean
delete_request(HTRequest *request)
{
	/*HTRequest_delete(request);*/
	return FALSE;
}

struct LoadToFileData {
	gchar *url;
	gchar *file;
	GWWWLoadToFileFunc callback;
	gpointer user_data;
};

static int
after_load_to_file(HTRequest *request, HTResponse *response,
		   void *param, int status)
{
	struct LoadToFileData *data = (struct LoadToFileData *)param;

	/* Ignore these after events.  We will get another call to the
	 * after filter when the data actually loads. */
	switch (status) {
	case HT_NO_ACCESS:
	case HT_REAUTH:
	case HT_PERM_REDIRECT:
	case HT_FOUND:
	case HT_SEE_OTHER:
	case HT_TEMP_REDIRECT:
		return HT_OK;
	default:
		break;
	}

	if (data) {
		if (data->callback)
			(* data->callback)(data->url, data->file, status,
					   data->user_data);
		g_free(data->url);
		g_free(data->file);
		g_free(data);
	}

	/* schedule for the request to be deleted */
	g_idle_add((GSourceFunc)delete_request, request);

	return HT_OK;
}

GWWWRequest *
glibwww_load_to_file(const gchar *url, const gchar *file,
		     GWWWLoadToFileFunc callback, gpointer user_data)
{
	FILE *fp;
	HTRequest *request;
	HTStream *writer;
	struct LoadToFileData *data;

	g_return_val_if_fail(url != NULL, NULL);
	g_return_val_if_fail(file != NULL, NULL);

	if ((fp = fopen(file, "wb")) == NULL)
		return NULL;

	request = HTRequest_new();
	writer = HTFWriter_new(request, fp, NO);
	HTRequest_setOutputFormat(request, WWW_SOURCE);
	HTRequest_setOutputStream(request, writer);
	HTRequest_setDebugStream(request, writer);
	HTRequest_setAnchor(request, HTAnchor_findAddress(url));

	data = g_new(struct LoadToFileData, 1);
	data->url = g_strdup(url);
	data->file = g_strdup(file);
	data->callback = callback;
	data->user_data = user_data;
	HTRequest_addAfter(request, after_load_to_file, NULL, data,
			   HT_ALL, HT_FILTER_LAST, FALSE);

	if (HTLoad(request, NO) == NO) {
		fclose(fp);
		HTRequest_delete(request);
		return NULL;
	}
	return request;
}

struct LoadToMemData {
	gchar *url;
	HTChunk *chunk;
	GWWWLoadToMemFunc callback;
	gpointer user_data;
};

static int
after_load_to_mem(HTRequest *request, HTResponse *response,
		  void *param, int status)
{
	struct LoadToMemData *data = (struct LoadToMemData *)param;

	/* Ignore these after events.  We will get another call to the
	 * after filter when the data actually loads. */
	switch (status) {
	case HT_NO_ACCESS:
	case HT_REAUTH:
	case HT_PERM_REDIRECT:
	case HT_FOUND:
	case HT_SEE_OTHER:
	case HT_TEMP_REDIRECT:
		return HT_OK;
	default:
		break;
	}

	if (data->callback)
		(* data->callback)(data->url, HTChunk_data(data->chunk),
				   HTChunk_size(data->chunk), status, data->user_data);
	g_free(data->url);
	HTChunk_delete(data->chunk);
	g_free(data);

	/* schedule for the request to be deleted */
	g_idle_add((GSourceFunc)delete_request, request);

	return HT_OK;
}

GWWWRequest *
glibwww_load_to_mem(const gchar *url, GWWWLoadToMemFunc callback,
		    gpointer user_data)
{
	HTRequest *request;
	HTStream *writer;
	HTChunk *chunk = NULL;
	struct LoadToMemData *data;

	g_return_val_if_fail(url != NULL, NULL);

	request = HTRequest_new();
	writer = HTStreamToChunk(request, &chunk, 0);
	HTRequest_setOutputFormat(request, WWW_SOURCE);
	HTRequest_setOutputStream(request, writer);
	HTRequest_setDebugStream(request, writer);
	HTRequest_setAnchor(request, HTAnchor_findAddress(url));

	data = g_new(struct LoadToMemData, 1);
	data->url = g_strdup(url);
	data->chunk = chunk;
	data->callback = callback;
	data->user_data = user_data;
	HTRequest_addAfter(request, after_load_to_mem, NULL, data,
			   HT_ALL, HT_FILTER_LAST, FALSE);

	if (HTLoad(request, NO) == NO) {
		HTChunk_delete(chunk);
		HTRequest_delete(request);
		return NULL;
	}
	return request;
}

gboolean
glibwww_abort_request(GWWWRequest *request)
{
	g_return_val_if_fail(request != NULL, FALSE);

	return HTRequest_kill(request) == YES;
}

void
glibwww_request_progress(GWWWRequest *request, glong *nread, glong *total)
{
	glong tot = HTAnchor_length(HTRequest_anchor(request));
	glong nr = -1;

	if (tot > 0)
		nr = HTRequest_bodyRead(request);
	else
		nr = HTRequest_bytesRead(request);

	if (nread)
		*nread = nr;
	if (total)
		*total = tot;
}

/* morla - Copyright (C) 2005-2007 bakunin - Andrea Marchesini 
 *                                      <bakunin@morlardf.net>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
# error Use configure; make; make install
#endif

#include "editor.h"

#define DOWNLOAD_TIMEOUT	10

gchar *download_proxy = NULL;
gint download_proxy_port = 0;
gchar *download_proxy_user = NULL;
gchar *download_proxy_password = NULL;

#ifdef USE_GCONF
gboolean download_proxy_gconf = TRUE;
#endif

struct download_mem_t
{
  gchar *mm;
  gint size;
};

static size_t download_memory (void *ptr, size_t size, size_t nmemb,
			       void *data);
static gchar *download_codes (gint ret);
static gchar *download_mimetype (gchar * uri);

gboolean
download_uri (gchar * uri, gchar ** buffer, struct download_t *download,
	      gchar ** error)
{
  CURL *curl;
  CURLcode code;
  struct download_mem_t *chunk;
  struct curl_slist *slist = NULL;
  long retcode;
  gchar *mime_type;

  curl_global_init (CURL_GLOBAL_DEFAULT);

  if (!(curl = curl_easy_init ()))
    {
      curl_easy_cleanup (curl);
      if (error)
	*error = g_strdup ("Error memory.");
      return FALSE;
    }

  chunk = g_malloc0 (sizeof (struct download_mem_t));

  curl_easy_setopt (curl, CURLOPT_URL, uri);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, download_memory);
  curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt (curl, CURLOPT_FILE, (void *) chunk);

  if ((mime_type = download_mimetype (uri)))
    {
      slist = curl_slist_append (slist, mime_type);
      curl_easy_setopt (curl, CURLOPT_HTTPHEADER, slist);
    }

#ifdef USE_GCONF
  if (download_proxy_gconf)
    {
      gchar *proxy;
      gint port;
      gchar *user;
      gchar *password;
      GSList *list, *l;
      gboolean ignore = FALSE;

      if (morla_gconf_proxy (&proxy, &port, &user, &password, &list) == TRUE)
	{
	  for (l = list; l; l = l->next)
	    {
	      if (g_str_has_prefix (uri, l->data) == TRUE)
		{
		  ignore = TRUE;
		  break;
		}
	    }

	  if (ignore == FALSE)
	    {
	      curl_easy_setopt (curl, CURLOPT_PROXY, proxy);
	      curl_easy_setopt (curl, CURLOPT_PROXYPORT, port);

	      if (user && password)
		{
		  gchar *auth = g_strdup_printf ("%s:%s", user, password);
		  curl_easy_setopt (curl, CURLOPT_PROXYUSERPWD, auth);
		  g_free (auth);
		}
	    }
	}
    }
  else
#endif

    /* Valori di default: */
  if (download_proxy)
    {
      curl_easy_setopt (curl, CURLOPT_PROXY, download_proxy);

      if (download_proxy_port)
	curl_easy_setopt (curl, CURLOPT_PROXYPORT, download_proxy_port);

      if (download_proxy_user && download_proxy_password)
	{
	  gchar *auth = g_strdup_printf ("%s:%s", download_proxy_user,
					 download_proxy_password);
	  curl_easy_setopt (curl, CURLOPT_PROXYUSERPWD, auth);
	  g_free (auth);
	}
    }

  /* Configurazione: */
  if (download)
    {
      if (download->timeout > 0)
	curl_easy_setopt (curl, CURLOPT_TIMEOUT, download->timeout);

      if (download->certfile)
	{
	  curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER,
			    download->verifypeer);
	  curl_easy_setopt (curl, CURLOPT_SSLCERT, download->certfile);

	  if (download->certpassword)
	    curl_easy_setopt (curl, CURLOPT_SSLCERTPASSWD,
			      download->certpassword);

	  if (download->cacertfile)
	    curl_easy_setopt (curl, CURLOPT_CAINFO, download->cacertfile);
	}

      if (download->auth_user && download->auth_password)
	{
	  gchar *auth = g_strdup_printf ("%s:%s", download->auth_user,
					 download->auth_password);
	  curl_easy_setopt (curl, CURLOPT_USERPWD, auth);
	  g_free (auth);
	}
    }

  curl_easy_setopt (curl, CURLOPT_USERAGENT, user_agent ());

  if ((code = curl_easy_perform (curl)) != CURLE_OK)
    {
      if (chunk->mm)
	g_free (chunk->mm);

      g_free (chunk);
      curl_easy_cleanup (curl);

      if (error)
	*error =
	  g_strdup_printf (_("Download error: %s"),
			   curl_easy_strerror (code));

      if (mime_type)
	curl_slist_free_all (slist);

      return FALSE;
    }

  if (mime_type)
    curl_slist_free_all (slist);

  if (curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &retcode) != CURLE_OK
      || ((retcode < 200 || retcode >= 300) && retcode != 0))
    {
      if (chunk->mm)
	g_free (chunk->mm);

      g_free (chunk);

      curl_easy_cleanup (curl);

      if (error)
	*error =
	  g_strdup_printf (_("Download error - code %d%s"), (int) retcode,
			   download_codes ((gint) retcode));
      return FALSE;
    }

  curl_easy_cleanup (curl);

  *buffer = chunk->mm;
  g_free (chunk);

  return TRUE;
}

static size_t
download_memory (void *ptr, size_t size, size_t nmemb, void *data)
{
  gint realsize = size * nmemb;
  struct download_mem_t *mem = (struct download_mem_t *) data;

  if (!mem->mm)
    mem->mm = g_malloc (realsize + 1);
  else
    mem->mm = g_realloc (mem->mm, mem->size + realsize + 1);

  memcpy (mem->mm + mem->size, ptr, realsize);
  mem->size += realsize;
  mem->mm[mem->size] = 0;

  return realsize;
}

void
download_free (struct download_t *download)
{
  if (!download)
    return;

  if (download->auth_user)
    g_free (download->auth_user);

  if (download->auth_password)
    g_free (download->auth_password);

  if (download->certfile)
    g_free (download->certfile);

  if (download->certpassword)
    g_free (download->certpassword);

  if (download->cacertfile)
    g_free (download->cacertfile);

  g_free (download);
}

struct download_t *
download_copy (struct download_t *download)
{
  struct download_t *new;

  if (!download)
    return NULL;

  new = g_malloc0 (sizeof (struct download_t));

  new->verifypeer = download->verifypeer;
  new->timeout = download->timeout;

  if (download->auth_user)
    new->auth_user = g_strdup (download->auth_user);

  if (download->auth_password)
    new->auth_password = g_strdup (download->auth_password);

  if (download->certfile)
    new->certfile = g_strdup (download->certfile);

  if (download->certpassword)
    new->certpassword = g_strdup (download->certpassword);

  if (download->cacertfile)
    new->cacertfile = g_strdup (download->cacertfile);

  return new;
}

static gchar *
download_codes (gint ret)
{
  switch (ret)
    {
    case 400:
      return _(" Bad Request");
    case 401:
      return _(" Unauthorized");
    case 402:
      return _(" Payment Required");
    case 403:
      return _(" Forbidden");
    case 404:
      return _(" Not Found");
    case 405:
      return _(" Method Not Allowed");
    case 406:
      return _(" Not Acceptable");
    case 407:
      return _(" Proxy Authentication Required");
    case 408:
      return _(" Request Timeout");
    case 409:
      return _(" Conflict");
    case 500:
      return _(" Internal Server Error");
    case 501:
      return _(" Not Implemented");
    case 502:
      return _(" Bad Gateway");
    case 503:
      return _(" Service Unavailable");

    default:
      return "";
    }
}

static gchar *
download_mimetype (gchar * uri)
{
  return "Accept: application/rdf+xml,application/x-turtle,*/*;q=0.1";
}

/* EOF */

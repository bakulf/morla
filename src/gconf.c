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

#ifdef USE_GCONF

#define GCONF_PROXY_NS			"/system/http_proxy"
#define GCONF_PROXY_KEY			GCONF_PROXY_NS "/use_http_proxy"
#define GCONF_PROXY_SERVER_KEY		GCONF_PROXY_NS "/host"
#define GCONF_PROXY_PORT_KEY		GCONF_PROXY_NS "/port"
#define GCONF_PROXY_USER_KEY		GCONF_PROXY_NS "/authentication_user"
#define GCONF_PROXY_PASSWORD_KEY 	GCONF_PROXY_NS "/authentication_password"
#define GCONF_PROXY_IGNORE_KEY		GCONF_PROXY_NS "/ignore_hosts"

static GConfClient *morla_gcc = NULL;

static gboolean morla_proxy_enable = FALSE;
static gchar *morla_proxy = NULL;
static gint morla_proxy_port = 0;
static gchar *morla_proxy_user = NULL;
static gchar *morla_proxy_password = NULL;
static GSList *morla_proxy_ignore_list = NULL;

static void morla_gconf_refresh (void);
static void morla_gconf_refresh_single (GConfClient * client, guint id,
					GConfEntry * entry, gpointer dummy);
static void morla_gconf_free (void);

void
morla_gconf_init (void)
{
  g_type_init ();

  morla_gcc = gconf_client_get_default ();

  morla_gconf_refresh ();
  gconf_client_notify_add (morla_gcc, GCONF_PROXY_NS,
			   morla_gconf_refresh_single, NULL, NULL, NULL);
}

static void
morla_gconf_free (void)
{
  if (morla_proxy)
    g_free (morla_proxy);

  if (morla_proxy_user)
    g_free (morla_proxy_user);

  if (morla_proxy_password)
    g_free (morla_proxy_password);

  if (morla_proxy_ignore_list)
    {
      g_slist_foreach (morla_proxy_ignore_list, (GFunc) g_free, NULL);
      g_slist_free (morla_proxy_ignore_list);
    }
}

static void
morla_gconf_refresh (void)
{
  morla_gconf_free ();

  morla_proxy_enable =
    gconf_client_get_bool (morla_gcc, GCONF_PROXY_KEY, NULL);

  morla_proxy =
    gconf_client_get_string (morla_gcc, GCONF_PROXY_SERVER_KEY, NULL);

  morla_proxy_port =
    gconf_client_get_int (morla_gcc, GCONF_PROXY_PORT_KEY, NULL);

  morla_proxy_user =
    gconf_client_get_string (morla_gcc, GCONF_PROXY_USER_KEY, NULL);

  morla_proxy_password =
    gconf_client_get_string (morla_gcc, GCONF_PROXY_PASSWORD_KEY, NULL);

  morla_proxy_ignore_list =
    gconf_client_get_list (morla_gcc, GCONF_PROXY_IGNORE_KEY,
			   GCONF_VALUE_STRING, NULL);
}

static void
morla_gconf_refresh_single (GConfClient * client, guint id,
			    GConfEntry * entry, gpointer dummy)
{
  GConfValue *value;

  if (!entry || !entry->key)
    return;

  value = gconf_entry_get_value (entry);

  if (!strcmp (entry->key, GCONF_PROXY_KEY))
    morla_proxy_enable = gconf_value_get_bool (value);

  else if (!strcmp (entry->key, GCONF_PROXY_SERVER_KEY))
    {
      if (morla_proxy)
	g_free (morla_proxy);

      if ((morla_proxy = (gchar *) gconf_value_get_string (value)))
	morla_proxy = g_strdup (morla_proxy);
    }

  else if (!strcmp (entry->key, GCONF_PROXY_PORT_KEY))
    morla_proxy_port = gconf_value_get_int (value);

  else if (!strcmp (entry->key, GCONF_PROXY_USER_KEY))
    {
      if (morla_proxy_user)
	g_free (morla_proxy_user);

      if ((morla_proxy_user = (gchar *) gconf_value_get_string (value)))
	morla_proxy_user = g_strdup (morla_proxy_user);
    }

  else if (!strcmp (entry->key, GCONF_PROXY_PASSWORD_KEY))
    {
      if (morla_proxy_password)
	g_free (morla_proxy_password);

      if ((morla_proxy_password = (gchar *) gconf_value_get_string (value)))
	morla_proxy_password = g_strdup (morla_proxy_password);
    }

  else if (!strcmp (entry->key, GCONF_PROXY_IGNORE_KEY))
    {
      if (morla_proxy_ignore_list)
	{
	  g_slist_foreach (morla_proxy_ignore_list, (GFunc) g_free, NULL);
	  g_slist_free (morla_proxy_ignore_list);
	}

      if ((morla_proxy_ignore_list = gconf_value_get_list (value)))
	{
	  GSList *l;

	  morla_proxy_ignore_list = g_slist_copy (morla_proxy_ignore_list);

	  for (l = morla_proxy_ignore_list; l; l = l->next)
	    l->data = g_strdup (l->data);
	}
    }
}

void
morla_gconf_shutdown (void)
{
  g_object_unref (morla_gcc);

  morla_gconf_free ();
}

gboolean
morla_gconf_proxy (gchar ** proxy, gint * port, gchar ** user,
		   gchar ** password, GSList ** list)
{
  if (morla_proxy_enable == FALSE)
    return FALSE;

  *proxy = morla_proxy;
  *port = morla_proxy_port;
  *user = morla_proxy_user;
  *password = morla_proxy_password;
  *list = morla_proxy_ignore_list;
  return TRUE;
}

#endif

/* EOF */

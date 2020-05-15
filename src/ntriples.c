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

/* Prototypes: */
static gchar *ntriples_markup_blank (gchar * ptr);
static gchar *ntriples_markup (gchar * ptr);

gchar *
ntriples (librdf_node * node)
{
  librdf_uri *uri;
  GString *str;

  gchar *tmp;

  if (!node)
    return NULL;

  str = g_string_new (NULL);

  switch (librdf_node_get_type (node))
    {
    case LIBRDF_NODE_TYPE_RESOURCE:
      uri = librdf_node_get_uri (node);
      tmp = ntriples_markup ((gchar *) librdf_uri_as_string (uri));
      g_string_append_printf (str, " <%s>", tmp);
      g_free (tmp);
      break;

    case LIBRDF_NODE_TYPE_BLANK:
      tmp =
	ntriples_markup_blank ((gchar *)
			       librdf_node_get_blank_identifier (node));
      g_string_append_printf (str, " _:%s", tmp);
      g_free (tmp);
      break;

    case LIBRDF_NODE_TYPE_LITERAL:
      tmp = ntriples_markup ((gchar *) librdf_node_get_literal_value (node));
      g_string_append_printf (str, " \"%s\"", tmp);
      g_free (tmp);

      if ((tmp = (gchar *) librdf_node_get_literal_value_language (node)))
	{
	  tmp = ntriples_markup (tmp);
	  g_string_append_printf (str, "@%s", tmp);
	  g_free (tmp);
	}

      if ((uri = librdf_node_get_literal_value_datatype_uri (node)))
	{
	  tmp = ntriples_markup ((gchar *) librdf_uri_as_string (uri));
	  g_string_append_printf (str, "^^%s", tmp);
	  g_free (tmp);
	}

      break;

    default:
      break;
    }

  return g_string_free (str, FALSE);
}

static gchar *
ntriples_markup (gchar * ptr)
{
  gunichar unichr;
  GString *ret;


  ret = g_string_new (NULL);

  while ((unichr = g_utf8_get_char (ptr)))
    {
      ptr = g_utf8_next_char (ptr);

      if (unichr >= 0x20 && unichr <= 0x7E)
	{
	  switch (unichr)
	    {
	    case '\\':
	      g_string_append_printf (ret, "\\\\");
	      break;

	    case '\"':
	      g_string_append_printf (ret, "\\\"");
	      break;

	    case '\n':
	      g_string_append_printf (ret, "\\\n");
	      break;

	    case '\r':
	      g_string_append_printf (ret, "\\\r");
	      break;

	    case '\t':
	      g_string_append_printf (ret, "\\\t");
	      break;

	    default:
	      g_string_append_printf (ret, "%c", unichr);
	      break;
	    }
	}

      else if (unichr < 0x10000)
	g_string_append_printf (ret, "\\u%.4X", unichr);

      else
	g_string_append_printf (ret, "\\U%.8X", unichr);
    }

  return g_string_free (ret, FALSE);
}

static gchar *
ntriples_markup_blank (gchar * ptr)
{
  gunichar unichr;
  gboolean first = TRUE;
  gchar *ret;
  gint j = 0;

  ret = g_malloc (strlen (ptr) + 1);

  while ((unichr = g_utf8_get_char (ptr)))
    {
      ptr = g_utf8_next_char (ptr);

      if ((unichr >= 'A' && unichr <= 'Z')
	  || (unichr >= 'a' && unichr <= 'z')
	  || (first == FALSE ? (unichr >= '0' && unichr <= '9') : 0))
	ret[j++] = unichr;

      first = FALSE;

    }

  ret[j] = 0;
  return ret;
}

/* EOF */

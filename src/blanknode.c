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

gchar *blanknode_name = NULL;

gboolean
blanknode_get (gchar * what, gint * id)
{
  gchar **sp;
  gchar buf[1024];
  gint len;

  if (!blanknode_name)
    return FALSE;

  if (!(sp = g_strsplit (blanknode_name, "%d", 2)))
    return FALSE;

  if (!*sp[0])
    {
      g_strfreev (sp);
      return FALSE;
    }

  len = strlen (sp[0]);
  if (strncmp (sp[0], what, len))
    {
      g_strfreev (sp);
      return FALSE;
    }

  what += len;
  *id = atoi (what);
  snprintf (buf, sizeof (buf), "%d", *id);

  len = strlen (buf);
  what += len;

  if (sp[1])
    {
      len = strlen (sp[1]);
      if (strncmp (sp[1], what, len))
	{
	  g_strfreev (sp);
	  return FALSE;
	}
    }

  g_strfreev (sp);
  return TRUE;
}

gchar *
blanknode_create (gint id)
{
  gchar **sp;
  GString *str;

  if (!blanknode_name)
    return g_strdup_printf ("%s%d", BLANK_NODE, id);

  if (!(sp = g_strsplit (blanknode_name, "%d", 2)))
    return g_strdup_printf ("%s%d", blanknode_name, id);

  if (!sp[0])
    {
      g_strfreev (sp);
      return g_strdup_printf ("%s%d", blanknode_name, id);
    }

  str = g_string_new (sp[0]);
  g_string_append_printf (str, "%d", id);

  if (sp[1])
    g_string_append_printf (str, "%s", sp[1]);

  g_strfreev (sp);
  return g_string_free (str, FALSE);
}

/* EOF */

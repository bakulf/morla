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

/* Questa funzione esegue un determinato uri dentro ad un browser */
void
browser (gchar * what)
{
  GList *list, *old;
  gchar **a;
  gint id, ok;

  /* Funzione interna per lo split dei comandi in liste */
  list = old = split (browser_default);

  /* Creo l'array + 2 (comando e NULL) */
  a = g_malloc (sizeof (gchar *) * (g_list_length (list) + 2));

  /* Riempo l'array */
  ok = id = 0;

  while (list)
    {
      if (!ok && !strcmp ((gchar *) list->data, "%s"))
	{
	  a[id++] = what;
	  ok++;
	}
      else
	a[id++] = list->data;

      list = list->next;
    }

  if (!ok)
    a[id++] = what;

  a[id] = NULL;

  /* Funzione che esegue il comando. Non passo argomenti particolari
   * quindi non verra' lanciata nessuna callback al termine del comando */
  run (a, NULL, NULL);

  /* Libero l'array e la lista dei comandi */
  g_free (a);
  g_list_foreach (old, (GFunc) g_free, NULL);
  g_list_free (old);
}

/* Questa funzione lancia il browser col sito internet del progetto morla: */
void
browser_website (void)
{
  browser (MORLA_WEBSITE);
}

/* EOF */

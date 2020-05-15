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

gint last_document_value;
GList *last_list;
GtkWidget *last_menu;

static void
last_read (void)
{
  static int done = 0;
  gchar *path;
  int ch, i;
  gchar buffer[1024];
  FILE *fl;

  if (done)
    return;
  done++;

  last_list = NULL;

  path =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "last_documents", NULL);

  if (!(fl = g_fopen (path, "rb")))
    {
      g_free (path);
      return;
    }

  g_free (path);

  i = 0;
  while ((ch = fgetc (fl)) != EOF)
    {
      if (ch == '\n')
	{
	  if (i)
	    {
	      buffer[i] = 0;
	      last_list = g_list_append (last_list, g_strdup (buffer));

	      i = 0;
	    }
	}

      else if (i < sizeof (buffer) - 1)
	buffer[i++] = ch;
    }

  fclose (fl);
}

static void
last_open (GtkWidget * w, gchar * uri)
{
  editor_open_uri (uri, NULL);
}

void
last_update (void)
{
  GtkWidget *menuitem;

  last_read ();

  gtk_container_foreach (GTK_CONTAINER (last_menu),
			 (GtkCallback) gtk_widget_destroy, NULL);

  if (last_list)
    {
      GList *list;

      list = last_list;
      while (list)
	{
	  menuitem = gtk_menu_item_new_with_label (list->data);
	  gtk_container_add (GTK_CONTAINER (last_menu), menuitem);
	  g_signal_connect ((gpointer) menuitem, "activate",
			    G_CALLBACK (last_open), list->data);

	  list = list->next;
	}
    }
  else
    {
      menuitem = gtk_menu_item_new_with_label (_("<empty>"));
      gtk_container_add (GTK_CONTAINER (last_menu), menuitem);
      gtk_widget_set_sensitive (menuitem, FALSE);
    }

  gtk_widget_show_all (last_menu);
}

static void
last_save (void)
{
  GList *list;
  gchar *path;
  FILE *fl;

  path =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "last_documents", NULL);

  if (!(fl = g_fopen (path, "wb")))
    {
      g_free (path);
      return;
    }

  list = last_list;
  while (list)
    {
      fprintf (fl, "%s\n", (gchar *) list->data);
      list = list->next;
    }

  g_free (path);

  fclose (fl);
}

void
last_add (gchar * uri)
{
  GList *list = last_list;

  while (list)
    {
      if (!strcmp ((gchar *) list->data, uri))
	{
	  last_list = g_list_remove (last_list, list->data);
	  break;
	}

      list = list->next;
    }

  last_list = g_list_prepend (last_list, g_strdup (uri));

  while (g_list_length (last_list) > last_document_value)
    last_list = g_list_remove (last_list, g_list_last (last_list)->data);

  last_update ();
  last_save ();
}

/* Quando si cambia il livello di history dalle preferences devo
 * rimuovere gli eccessi: */
void
last_change_value (void)
{
  gint len = g_list_length (last_list);

  if (len > last_document_value)
    {
      len -= last_document_value;

      while (len)
	{
	  GList *tmp = g_list_last (last_list);

	  g_free (tmp->data);
	  last_list = g_list_remove (last_list, tmp->data);

	  len--;
	}
    }

  last_update ();
  last_save ();
}

/* EOF */

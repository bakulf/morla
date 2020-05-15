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

struct data_t
{
  gchar *uri;
  gchar *error;

  gint flag;
  GList *meta;
  GList *other;
};

static void search_activate (GtkWidget * widget, GtkTreePath * path,
			     GtkTreeViewColumn * column,
			     void (*func) (gchar *, gpointer));

/* Questa funzione cerca se l'elemento s dentro la lista list e poi dentro
 * other. Se non lo trova, inserisce dentro list */
static void
check_lists (GList ** list, GList ** other, gchar * s)
{
  GList *l;

  l = *list;
  while (l)
    {
      if (!strcmp (s, (gchar *) l->data))
	return;

      l = l->next;
    }

  if (other)
    {
      l = *other;
      while (l)
	{
	  if (!strcmp (s, (gchar *) l->data))
	    return;

	  l = l->next;
	}
    }

  (*list) = g_list_append (*list, g_strdup (s));
}

/* Questa funzione fa il controllo di un tag preciso degli head: */
static void
check_meta_node (nxml_data_t * node, gchar * uri, GList ** meta)
{
  nxml_attr_t *attr = node->attributes;
  gboolean rel = FALSE;
  gchar *href = NULL;

  while (attr)
    {

      if (!strcmp (attr->name, "rel") && !strcmp (attr->value, "meta"))
	rel = TRUE;

      else if (!strcmp (attr->name, "href"))
	href = attr->value;

      attr = attr->next;
    }

  if (rel == TRUE && href)
    {
      gchar s[1024];

      if (!g_str_has_prefix (href, "http://") &&
	  !g_str_has_prefix (href, "https://") &&
	  !g_str_has_prefix (href, "ftp://") &&
	  !g_str_has_prefix (href, "ftps://"))
	{
	  g_snprintf (s, sizeof (s), "%s%s%s", uri,
		      g_str_has_suffix (uri, "/") ? "" : "/", href);
	  check_lists (meta, NULL, s);
	}
      else
	check_lists (meta, NULL, href);
    }
}

/* Cerca la sezione head di un documento html e poi per ogni elemento
 * usa la funzione di sopra */
static void
check_meta (nxml_data_t * node, gchar * uri, GList ** meta)
{
  node = node->children;

  while (node)
    {
      if (node->type == NXML_TYPE_ELEMENT && !strcmp (node->value, "head"))
	{
	  node = node->children;
	  while (node)
	    {
	      if (node->type == NXML_TYPE_ELEMENT
		  && !strcmp (node->value, "link"))
		{
		  check_meta_node (node, uri, meta);
		}

	      node = node->next;
	    }

	  return;
	}

      node = node->next;
    }
}

/* Per tutto il resto del documento faccio un check sulle estensioni: */
static void
check_string (gchar * str, gchar * uri, GList ** list, GList ** meta)
{
  if (!str)
    return;

  if (g_str_has_suffix (str, ".rdf") == TRUE)
    {
      gchar s[1024];

      if (!g_str_has_prefix (str, "http://") &&
	  !g_str_has_prefix (str, "https://") &&
	  !g_str_has_prefix (str, "ftp://") &&
	  !g_str_has_prefix (str, "ftps://"))
	{
	  g_snprintf (s, sizeof (s), "%s%s%s", uri,
		      g_str_has_suffix (uri, "/") ? "" : "/", str);
	  check_lists (list, meta, s);
	}
      else
	check_lists (list, meta, str);
    }
}

/* Controlla un nodo e per tutti i suoi attributi lancia la funzione di
 * sopra: */
static void
check_node (nxml_data_t * node, gchar * uri, GList ** list, GList ** meta)
{
  nxml_attr_t *attr = node->attributes;

  while (attr)
    {
      check_string (attr->value, uri, list, meta);
      attr = attr->next;
    }

  node = node->children;

  while (node)
    {
      if (node->type == NXML_TYPE_ELEMENT)
	check_node (node, uri, list, meta);
      node = node->next;
    }
}

/* Thread per il lavoro sporco. Usa libnxml per parsare una pagina html: */
static void *
thread (void *what)
{
  nxml_t *nxml;
  struct data_t *data = (struct data_t *) what;
  nxml_data_t *e;

  if (nxml_new (&nxml) != NXML_OK)
    {
      data->flag = 1;
      data->meta = NULL;
      data->other = NULL;
      data->error = g_strdup (_("Memory error ?!?"));
      g_thread_exit (NULL);
      return NULL;
    }

  if (nxml_set_user_agent (nxml, user_agent ()) != NXML_OK)
    {
      data->flag = 1;
      data->meta = NULL;
      data->other = NULL;
      data->error = g_strdup (_("Memory error ?!?"));
      nxml_free (nxml);
      g_thread_exit (NULL);
      return NULL;
    }

  /* non e' un documento XML! */
  if (nxml_set_timeout (nxml, 10) != NXML_OK
      || nxml_parse_url (nxml, data->uri) != NXML_OK
      || nxml_root_element (nxml, &e) != NXML_OK || !e)
    {
      data->flag = 1;
      data->meta = NULL;
      data->other = NULL;
      data->error = g_strdup (_("This document has not a XML syntax!"));
      nxml_free (nxml);
      g_thread_exit (NULL);
      return NULL;
    }

  /* Se e' html faccio il check dei meta: */
  if (!strcmp (e->value, "html"))
    check_meta (e, data->uri, &data->meta);

  /* Per tutto il resto: */
  while (e)
    {
      if (e->type == NXML_TYPE_ELEMENT)
	check_node (e, data->uri, &data->other, &data->meta);
      e = e->next;
    }

  /* Fine: */
  data->flag = 1;
  nxml_free (nxml);
  g_thread_exit (NULL);
  return NULL;
}

/* Questa funzione viene lanciata quando il thread ha finito di compiere
 * le sue operazioni ma si e' premuto cancel. Libera la memoria: */
static void
search_thread_free (struct data_t *data)
{
  if (data->uri)
    g_free (data->uri);

  if (data->error)
    g_free (data->error);

  if (data->meta)
    {
      g_list_foreach (data->meta, (GFunc) g_free, NULL);
      g_list_free (data->meta);
    }

  if (data->other)
    {
      g_list_foreach (data->other, (GFunc) g_free, NULL);
      g_list_free (data->other);
    }

  g_free (data);
}

/* Se si preme cancel si aggiunge un elemento alla lista dei thread da
 * gestire: */
static void
search_thread_abort (GtkWidget * widget, struct data_t *data)
{
  struct thread_t *new;
  gboolean *ret = g_object_get_data (G_OBJECT (widget), "ret");

  new = (struct thread_t *) g_malloc0 (sizeof (struct thread_t));

  new->func = (GFunc) search_thread_free;
  new->data = data;
  new->flag = &data->flag;

  thread_child = g_list_append (thread_child, new);
  *ret = 1;
}

/* Questa funzione fai la finestra di attesa attendo il processo thread: */
gboolean
search (gchar * uri, GList ** meta, GList ** other, gchar ** error)
{
  GtkWidget *window;
  GtkWidget *pb;
  GThread *th;
  struct data_t *data;
  gboolean ret = 0;

  data = (struct data_t *) g_malloc (sizeof (struct data_t));

  data->uri = g_strdup (uri);
  data->flag = 0;
  data->meta = NULL;
  data->other = NULL;
  data->error = NULL;

  window = dialog_wait (G_CALLBACK (search_thread_abort), data, &pb);
  g_object_set_data (G_OBJECT (window), "ret", &ret);

  th = g_thread_create ((GThreadFunc) thread, data, FALSE, NULL);

  while (!data->flag && !ret)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pb));

      while (gtk_events_pending () && !data->flag && !ret)
	gtk_main_iteration ();

      g_usleep (50000);
    }

  gtk_widget_destroy (window);

  if (ret)
    {
      *error = NULL;
      return FALSE;
    }

  *error = data->error;
  *meta = data->meta;
  *other = data->other;

  g_free (data->uri);
  g_free (data);

  return (*other || *meta) ? TRUE : FALSE;
}

/* Questa funzione attiva il sistema di ricerca e mostra i risultati: */
void
search_open (gchar * text, void (*func) (gchar *, gpointer), gpointer data)
{
  GList *l, *meta, *other;
  gchar *error;
  GtkWidget *window, *scrolledwindow, *treeview, *button, *frame;
  GtkListStore *model;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  gchar s[1024];

  /* Qui la funzione di ricerca: */
  if (search (text, &meta, &other, &error) == FALSE)
    {
      if (error)
	{
	  g_snprintf (s, sizeof (s), _("No RDF Document found! %s"), error);
	  g_free (error);
	}
      else
	g_snprintf (s, sizeof (s), _("No RDF Document found!"));

      dialog_msg (s);
      return;
    }

  /* Interfaccia: */
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("RDF founder"));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 400, 300);

  frame = gtk_frame_new (_("Meta RDFs"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), frame, TRUE, TRUE,
		      0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model = gtk_list_store_new (1, G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  g_object_set_data (G_OBJECT (treeview), "data", data);
  g_signal_connect ((gpointer) treeview, "row_activated",
		    G_CALLBACK (search_activate), func);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("RDF"), renderer, "text", 0,
					      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  /* Riempo i dati trovati in meta: */
  l = meta;
  while (l)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, l->data, -1);
      l = l->next;
    }

  g_object_unref (model);

  frame = gtk_frame_new (_("Other RDFs"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), frame, TRUE, TRUE,
		      0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model = gtk_list_store_new (1, G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  g_object_set_data (G_OBJECT (treeview), "data", data);
  g_signal_connect ((gpointer) treeview, "row_activated",
		    G_CALLBACK (search_activate), func);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("RDF"), renderer, "text", 0,
					      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_NO);
  gtk_widget_set_can_default(button, TRUE);

  /* Gli altri: */
  l = other;
  while (l)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, l->data, -1);
      l = l->next;
    }

  g_object_unref (model);

  gtk_widget_show_all (window);
  gtk_dialog_run (GTK_DIALOG (window));

  /* Libero memoria: */
  if (meta)
    {
      g_list_foreach (meta, (GFunc) g_free, NULL);
      g_list_free (meta);
    }

  if (other)
    {
      g_list_foreach (other, (GFunc) g_free, NULL);
      g_list_free (other);
    }

  gtk_widget_destroy (window);
}

/* Questa funzione viene lanciata quando si attiva una riga della funzione
 * di sopra */
static void
search_activate (GtkWidget * widget, GtkTreePath * path,
		 GtkTreeViewColumn * column, void (*func) (gchar *, gpointer))
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  gpointer other_data = g_object_get_data (G_OBJECT (widget), "data");
  GtkTreeIter iter;
  gchar *text;

  /* Se non c'e' selezionato niente, comunico ed esco */
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      dialog_msg (_("Select some URIs!"));
      return;
    }

  /* Prendo il testo selezionato */
  gtk_tree_model_get (model, &iter, 0, &text, -1);
  func (text, other_data);
}

/* EOF */

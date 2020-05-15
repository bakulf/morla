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

gchar *update_url = NULL;
gint update_show;

static struct update_t *update_data = NULL;
static GMutex *update_mutex = NULL;
static gint status = 0;

/* Libero un template dalla memoria: */
static void
update_template_free (struct update_template_t *t)
{
  if (!t)
    return;

  if (t->uri)
    g_free (t->uri);

  if (t->name)
    g_free (t->name);

  if (t->description)
    g_free (t->description);

  if (t->author)
    g_free (t->author);

  if (t->version)
    g_free (t->version);

  g_free (t);
}

/* Libero un intero update: */
static void
update_free (struct update_t *up)
{
  if (!up)
    return;

  if (up->last_version)
    g_free (up->last_version);

  if (up->templates)
    {
      g_list_foreach (up->templates, (GFunc) update_template_free, NULL);
      g_list_free (up->templates);
    }

  g_free (up);
}

/* Funzione per parsare un template dentro al file XML scaricato dal sito */
static void
thread_template (nxml_data_t * data, struct update_t *ret)
{
  struct update_template_t *t;

  t =
    (struct update_template_t *)
    g_malloc0 (sizeof (struct update_template_t));

  while (data)
    {
      if (data->type == NXML_TYPE_ELEMENT)
	{
	  if (!strcmp (data->value, "uri"))
	    nxml_get_string (data, &t->uri);
	  else if (!strcmp (data->value, "name"))
	    nxml_get_string (data, &t->name);
	  else if (!strcmp (data->value, "description"))
	    nxml_get_string (data, &t->description);
	  else if (!strcmp (data->value, "author"))
	    nxml_get_string (data, &t->author);
	  else if (!strcmp (data->value, "version"))
	    nxml_get_string (data, &t->version);
	}

      data = data->next;
    }

  if (!t->uri || !t->name)
    update_template_free (t);

  else
    ret->templates = g_list_append (ret->templates, t);
}

/* Per ogni sezione template del file di configurazione lancia la funzione
 * di sopra: */
static void
thread_templates (nxml_data_t * data, struct update_t *ret)
{
  while (data)
    {
      if (data->type == NXML_TYPE_ELEMENT
	  && !strcmp (data->value, "template"))
	thread_template (data->children, ret);

      data = data->next;
    }
}

/* Questa funzione lanciata in un thread separato fa il check di eventuali
 * aggiornamenti riempiendo una struttura dati in memoria: */
static void *
thread (void *dummy)
{
  nxml_t *nxml;
  nxml_data_t *data;
  struct update_t *ret;

  if (nxml_new (&nxml) != NXML_OK)
    {
      status = 0;
      g_thread_exit (NULL);
      return NULL;
    }

  if (nxml_set_user_agent (nxml, user_agent ()) != NXML_OK)
    {
      status = 0;
      g_thread_exit (NULL);
      return NULL;
    }

  if (nxml_parse_url (nxml, update_url) != NXML_OK)
    {
      nxml_free (nxml);
      status = 0;
      g_thread_exit (NULL);
      return NULL;
    }

  if (nxml_root_element (nxml, &data) != NXML_OK || !data
      || strcmp (data->value, "morla"))
    {
      nxml_free (nxml);
      status = 0;
      g_thread_exit (NULL);
      return NULL;
    }

  ret = (struct update_t *) g_malloc0 (sizeof (struct update_t));

  data = data->children;
  while (data)
    {

      if (data->type == NXML_TYPE_ELEMENT)
	{
	  if (!strcmp (data->value, "last_version"))
	    nxml_get_string (data, &ret->last_version);

	  else if (!strcmp (data->value, "templates"))
	    thread_templates (data->children, ret);
	}

      data = data->next;
    }

  nxml_free (nxml);

  /* Quando devo aggiornare, locko la struttura dati e scrivo: */
  g_mutex_lock (update_mutex);
  if (update_data)
    update_free (update_data);

  update_data = ret;
  g_mutex_unlock (update_mutex);

  status = 0;
  g_thread_exit (NULL);
  return NULL;
}

/* Questa funzione lanciata ogni 20 minuti, lancia il thread se quello di
 * prima ha finito: */
gint
update_timer (void)
{
  GThread *th;

  if (status)
    return 1;

  status = 1;
  update_mutex = g_mutex_new ();
  th = g_thread_create ((GThreadFunc) thread, NULL, FALSE, NULL);

  return 1;
}

/* Ci sono aggiornamenti ? */
int
update_check (void)
{
  gchar *version = NULL;

  /* Se il thread non sta salvando dentro la zona di memoria: */
  if (g_mutex_trylock (update_mutex) == FALSE)
    return 0;

  if (status || !update_data)
    {
      g_mutex_unlock (update_mutex);
      return 0;
    }

  /* Controllo la versione: */
  if (strcmp (update_data->last_version, VERSION))
    version = g_strdup (update_data->last_version);

  g_mutex_unlock (update_mutex);

  /* Se la versione non coincide faccio il popup: */
  if (version)
    {
      gchar *s;
      s =
	g_strdup_printf (_("A new release of %s is out!\nTry %s %s!"),
			 PACKAGE, PACKAGE, version);

      g_free (version);
      dialog_msg (s);
      g_free (s);
    }

  return version ? 1 : 0;
}

static void
update_search_refresh (GtkListStore * model)
{
  GtkTreeIter iter;
  GList *list, *l;
  struct update_template_t *t;
  gint ret;

  /* Rimuovo tutto: */
  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (model), &iter, NULL, 0))
    gtk_list_store_remove (model, &iter);

  /* Per ogni template presente in elenco inserisco una riga: */
  list = update_data->templates;
  while (list)
    {
      t = list->data;

      ret = 1;
      l = template_list;
      while (l)
	{
	  /* inserisco solo se e' una versione diversa da quella
	   * gia' importata: */
	  if (!strcmp (((struct template_t *) l->data)->uri, t->uri) &&
	      !strcmp (((struct template_t *) l->data)->version, t->version))
	    {
	      ret = 0;
	      break;
	    }
	  l = l->next;
	}

      if (ret)
	{
	  gtk_list_store_append (model, &iter);
	  gtk_list_store_set (model, &iter, 0, t->name, 1, t->description,
			      2, t->author, 3, t->version, 4, t->uri, -1);
	}

      list = list->next;
    }
}

/* Funzione per la ricerca di aggiornamenti. Questa funzione e' bloccante
 * grazie al mutex. */
void
update_search (void)
{
  GtkWidget *window, *hbox, *scrolledwindow, *treeview, *button, *frame;
  GtkListStore *model;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  gchar s[1024];
  gint ret;

  g_mutex_lock (update_mutex);

  if (!update_data)
    {
      g_mutex_unlock (update_mutex);
      dialog_msg (_("No updates found!"));
      return;
    }

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Remote templates"));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 400, 200);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, TRUE, TRUE,
		      0);

  frame = gtk_frame_new (_("Remote templates"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model =
    gtk_list_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "text", 0,
					      NULL);
  gtk_tree_view_column_set_sort_column_id (column, 0);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Description"), renderer,
					      "text", 1, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 1);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Author"), renderer, "text",
					      2, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 2);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Version"), renderer, "text",
					      3, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 3);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  button = gtk_button_new_from_stock ("gtk-add");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_NO);
  gtk_widget_set_can_default(button, TRUE);

  update_search_refresh (model);
  gtk_widget_show_all (window);

  while (1)
    {
      ret = gtk_dialog_run (GTK_DIALOG (window));

      /* se devo importare: */
      if (ret == GTK_RESPONSE_OK)
	{
	  gchar *uri;

	  /* Prelevo quallo selezionato: */
	  if (gtk_tree_selection_get_selected (selection, NULL, &iter) ==
	      FALSE)
	    {
	      dialog_msg (_("Select some templates!"));
	      continue;
	    }

	  gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 4, &uri, -1);

	  /* importo: */
	  if (template_import (uri, NULL, TRUE, TRUE) == TRUE)
	    {
	      template_update ();
	      template_save ();
	      update_search_refresh (model);
	    }

	  g_free (uri);
	  continue;
	}

      else
	break;
    }

  g_object_unref (model);
  gtk_widget_destroy (window);

  g_mutex_unlock (update_mutex);
}

/* EOF */

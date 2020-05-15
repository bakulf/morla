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

static void merge_file (GtkWidget * widget, GtkWidget * treeview);
static void merge_uri (GtkWidget * widget, GtkWidget * treeview);
static void merge_remove (GtkWidget * widget, GtkWidget * treeview);

/* Questa funzione disegna un widget per aggiungere ad un elenco degli URI
 * e poi avviare la procedura per mergiarli in un grafo */
void
merge_graph (void)
{
  GtkWidget *window, *hbox, *scrolledwindow, *treeview;
  GtkWidget *box, *button, *frame, *vbox, *image, *label;
  GtkListStore *model;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkTreeIter iter;
  gchar s[1024];
  GString *error_msg;

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Merge more one RDF in a graph..."));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_widget_set_size_request (window, 400, 200);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, TRUE, TRUE,
		      0);

  frame = gtk_frame_new (_("List of RDFs"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();

  column =
    gtk_tree_view_column_new_with_attributes (_("RDF"), renderer, "text", 0,
					      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (merge_file),
		    treeview);

  box = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (button), box);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("_Open a RDF file"));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (merge_uri),
		    treeview);

  box = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (button), box);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("Open a RDF _URI"));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-remove");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (merge_remove),
		    treeview);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button,
				GTK_RESPONSE_CANCEL);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(button, TRUE);

  gtk_widget_show_all (window);

  while ((gtk_dialog_run (GTK_DIALOG (window))) == GTK_RESPONSE_OK)
    {
      GList *rdf, *list;
      gboolean error = FALSE;

      /* Se devo generare il grafo devono esserci dati */
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter) ==
	  FALSE)
	{
	  dialog_msg (_("No RDF Document present!"));
	  continue;
	}

      rdf = NULL;
      gtk_tree_selection_select_all (selection);

      /* Per ogni dato: */
      do
	{
	  gchar s[1024];
	  gchar *file;
	  gchar *uri;
	  struct download_t *download;

	  gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0, &file, 1,
			      &download, -1);

	  /* Ricreo un uri nel caso di un file: */
	  if (g_file_test (file, G_FILE_TEST_EXISTS) == FALSE)
	    uri = g_strdup (file);
	  else
	    uri = g_filename_to_uri (file, NULL, NULL);

	  /* Parso: */
	  error_msg = NULL;
	  if (!uri || !(list = merge (rdf, uri, download, &error_msg)))
	    {
	      gchar *str;

	      if (error_msg)
		str = g_string_free (error_msg, FALSE);
	      else
		str = NULL;

	      g_snprintf (s, sizeof (s), _("Error parsing URI '%s'!"),
			  uri ? uri : file);
	      dialog_msg_with_error (s, str);

	      if (str)
		g_free (str);

	      error = TRUE;

	      /* Deleseziono tutto tranne lui: */
	      gtk_tree_selection_unselect_all (selection);
	      gtk_tree_selection_select_iter (selection, &iter);

	      if (uri)
		g_free (uri);

	      g_free (file);
	      break;
	    }

	  rdf = list;

	  g_free (uri);
	  g_free (file);

	  /* Deseleziono quello fatto: */
	  gtk_tree_selection_unselect_iter (selection, &iter);
	}
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter));

      /* Se non ci sono errori: */
      if (error == FALSE && rdf)
	{
	  struct editor_data_t *data;

	  /* Grafi vuole una struttura editor_data_t e quindi simulo
	   * di averla: */
	  data =
	    (struct editor_data_t *)
	    g_malloc0 (sizeof (struct editor_data_t));

	  /* Parso i namespace */
	  data->rdf = rdf;
	  namespace_parse (rdf, &data->namespace);

	  /* Graph... */
	  graph (data);

	  /* Libero memoria: */
	  g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
	  g_list_free (data->namespace);
	  g_free (data);
	}

      /* Libero memoria: */
      if (rdf)
	{
	  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
	  g_list_free (rdf);
	}
    }

  /* Distruggo l'oggetto: */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter) == TRUE)
    {

      do
	{
	  struct download_t *download;

	  gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &download,
			      -1);

	  if (download)
	    download_free (download);
	}
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter));
    }

  g_object_unref (model);
  gtk_widget_destroy (window);
}

/* Se mi si chiede di rimuovere un URI: */
static void
merge_remove (GtkWidget * widget, GtkWidget * treeview)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  GList *list;

  /* Se non si e' selezionato qualcosa: */
  if (!(list = gtk_tree_selection_get_selected_rows (selection, &model)))
    {
      dialog_msg (_("No RDF Document selected!"));
      return;
    }

  /* Chiedo conferma: */
  if (dialog_ask (_("Sure to remove these RDFs?")) == GTK_RESPONSE_OK)
    {
      gint i = 0, j = 0;
      GList *l = list;

      /* Rimuovo: */
      while (l)
	{
	  struct download_t *download;

	  for (j = 0; j < i; j++)
	    gtk_tree_path_prev (l->data);

	  gtk_tree_model_get_iter (model, &iter, l->data);
	  gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &download,
			      -1);

	  if (download)
	    download_free (download);

	  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	  i++;
	  l = l->next;
	}
    }

/* Libero memoria: */
  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (list);
}

/* Se devo aggiungere un file: */
static void
merge_file (GtkWidget * widget, GtkWidget * treeview)
{
  GSList *list;
  GtkListStore *store;
  GtkTreeIter iter;

  store = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  /* filechooser: */
  if ((list =
       file_chooser (_("Open a new RDF file..."), GTK_SELECTION_MULTIPLE,
		     GTK_FILE_CHOOSER_ACTION_OPEN, NULL)))
    {
      GSList *l = list;

      /* Appendo alla lista: */
      while (l)
	{
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter, 0, l->data, 1, NULL, -1);
	  g_free (l->data);

	  l = l->next;
	}

      g_slist_free (list);
    }
}

/* Uguale per gli URI */
static void
merge_uri (GtkWidget * widget, GtkWidget * treeview)
{
  gchar *uri;
  GtkListStore *store;
  GtkTreeIter iter;
  struct download_t *download;

  if ((uri = uri_chooser (_("Open a RDF URI"), &download)))
    {
      store =
	(GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, uri, 1, download, -1);
      g_free (uri);
    }
}

GList *
merge (GList * list, gchar * what, struct download_t *download,
       GString ** error)
{
  GList *rdf, *ns;
  struct rdf_t *r;
  gint last = 0, i;

  if (rdf_parse_thread (what, download, &rdf, &ns, FALSE, NULL, error) ==
      FALSE)
    return NULL;

  g_list_foreach (ns, (GFunc) namespace_free, NULL);
  g_list_free (ns);

  ns = list;
  while (ns)
    {
      r = ns->data;

      if (blanknode_get (r->subject, &i) == TRUE && i > last)
	last = i;

      if (blanknode_get (r->object, &i) == TRUE && i > last)
	last = i;

      ns = ns->next;
    }

  last++;
  ns = rdf;
  while (ns)
    {
      r = ns->data;

      if (blanknode_get (r->subject, &i) == TRUE)
	{
	  g_free (r->subject);
	  r->subject = blanknode_create (i + last);
	}

      if (blanknode_get (r->object, &i) == TRUE)
	{
	  g_free (r->object);
	  r->object = blanknode_create (i + last);
	}

      list = g_list_append (list, r);
      ns = ns->next;
    }

  g_list_free (rdf);

  return list;
}

/* EOF ?!? */

/*
Guy Debord:In girum imus nocte ecce et consumimur igni
*/

/* EOF */

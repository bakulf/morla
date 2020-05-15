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

static void rdfs_refresh (GtkWidget * treeview);
static void cell_edited (GtkCellRendererText * cell,
			 const gchar * path_string, const gchar * new_text,
			 gpointer data);
static void rdfs_open (GtkWidget * treeview);
static gint rdfs_remove (GtkWidget * treeview);
static void rdfs_free (struct rdfs_t *rdfs);

/* Questa funzione crea l'interfaccia grafica che mostra tutti gli rdfs
 * importanti e permette operazioni su questo elenco: */
void
rdfs (GtkWidget * w, gpointer dummy)
{
  GtkWidget *window, *hbox, *scrolledwindow, *treeview, *button, *frame;
  GtkListStore *model;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  gchar s[1024];
  gint ret;

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("RDFSs import"));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 400, 200);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, TRUE, TRUE,
		      0);

  frame = gtk_frame_new (_("RDFSs import"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "editable", TRUE, NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), model);
  g_object_set_data (G_OBJECT (renderer), "column", (gpointer) 0);

  column =
    gtk_tree_view_column_new_with_attributes (_("RDFS"), renderer, "text", 0,
					      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "editable", TRUE, NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), model);
  g_object_set_data (G_OBJECT (renderer), "column", (gpointer) 1);

  column =
    gtk_tree_view_column_new_with_attributes (_("Default prefix"), renderer,
					      "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  button = gtk_button_new_from_stock ("gtk-open");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button,
				GTK_RESPONSE_YES);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-add");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-remove");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button,
				GTK_RESPONSE_CANCEL);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_NO);
  gtk_widget_set_can_default(button, TRUE);

  rdfs_refresh (treeview);
  g_object_unref (model);

  gtk_widget_show_all (window);

  while (1)
    {
      ret = gtk_dialog_run (GTK_DIALOG (window));

      /* Aprire: */
      if (ret == GTK_RESPONSE_YES)
	rdfs_open (treeview);

      /* Aggiunta: */
      else if (ret == GTK_RESPONSE_OK)
	{
	  if (rdfs_add (NULL, NULL))
	    rdfs_refresh (treeview);
	}

      /* rimozione: */
      else if (ret == GTK_RESPONSE_CANCEL)
	{
	  if (dialog_ask (_("Sure to remove this RDFS ?")) == GTK_RESPONSE_OK
	      && rdfs_remove (treeview))
	    {
	      init_save ();
	      rdfs_refresh (treeview);
	    }
	}

      else
	break;
    }

  gtk_widget_destroy (window);
}

/* La procedura di refresh aggiorna l'elenco dell'interfaccia: */
static void
rdfs_refresh (GtkWidget * treeview)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *list;
  struct rdfs_t *rdfs;

  /* Rimuovo prima tutto: */
  store = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (store), &iter, NULL, 0))
    gtk_list_store_remove (store, &iter);

  /* Poi inserisco: */
  list = rdfs_list;
  while (list)
    {
      rdfs = (struct rdfs_t *) list->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, rdfs->resource, 1, rdfs->prefix,
			  -1);

      list = list->next;
    }
}

/* Questa funzione serve per editare un elemento della lista: */
static void
cell_edited (GtkCellRendererText * cell, const gchar * path_string,
	     const gchar * new_text, gpointer data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  GList *list = rdfs_list;
  gint i;

  gint column = (gint) (g_object_get_data (G_OBJECT (cell), "column"));

  gtk_tree_model_get_iter (model, &iter, path);
  i = gtk_tree_path_get_indices (path)[0];

  switch (column)
    {
      /* RDFS: */
    case 0:
      /* Trovo l'elemento della lista: */
      if ((list = g_list_nth (list, i)))
	{
	  struct rdfs_t *rdfs = (struct rdfs_t *) list->data;

	  /* Libero memoria e aggiorno: */
	  if (rdfs->resource)
	    g_free (rdfs->resource);
	  rdfs->resource = g_strdup (new_text);

	  gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
			      new_text, -1);
	  init_save ();
	}
      break;

      /* PREFIX: */
    case 1:

      if ((list = g_list_nth (list, i)))
	{
	  struct rdfs_t *rdfs = (struct rdfs_t *) list->data;

	  if (rdfs->prefix)
	    g_free (rdfs->prefix);

	  rdfs->prefix = g_strdup (new_text);

	  gtk_list_store_set (GTK_LIST_STORE (model), &iter, column,
			      new_text, -1);
	  init_save ();
	}
      break;
    }

  gtk_tree_path_free (path);
}

/* Questa funzione serve per aprire un documento RDFS dentro l'editor: */
static void
rdfs_open (GtkWidget * treeview)
{
  struct rdfs_t *rdfs;
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  /* Becco cio' che e' selezionato: */
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;
      GList *list = rdfs_list;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];

      /* Cerco l'elemento: */
      if ((list = g_list_nth (list, i)))
	{
	  rdfs = list->data;

	  /* Apro quello salvato... non l'originale */
	  if (editor_open_file (rdfs->path, rdfs->resource) == TRUE)
	    {
	      struct editor_data_t *t;

	      /* Poi vado a rimuovere il nome in modo che quando si
	       * chieda di salvarlo, esso venga salvato in un nuovo file */
	      t = editor_data;
	      while (t)
		{
		  if (t->file && !strcmp (rdfs->path, t->file))
		    {
		      g_free (t->file);
		      t->file = NULL;
		      break;
		    }

		  t = t->next;
		}
	    }
	}

      gtk_tree_path_free (path);
    }

  return;
}

/* Rimozione di un RDFS: */
static gint
rdfs_remove (GtkWidget * treeview)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  /* Quelli selezionati: */
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;
      GList *list = rdfs_list;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      /* Rimuovo e libero memoria: */
      if ((list = g_list_nth (list, i)))
	{
	  struct rdfs_t *rdfs = (struct rdfs_t *) list->data;

	  g_remove (rdfs->path);
	  rdfs_list = g_list_remove (rdfs_list, rdfs);
	  rdfs_free (rdfs);
	}

      gtk_tree_path_free (path);
      return 1;
    }

  return 0;
}

/* Questa funzione serve per aggiungere un nuovo elemento RDFS con prefix: */
gint
rdfs_add (gchar * resource, gchar * p)
{
  GtkWidget *dialog;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *ns;
  GtkWidget *prefix;
  GtkWidget *button;
  gint ret = 0;
  gchar s[1024];

  dialog = gtk_dialog_new ();

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("New RDFS..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE,
		      0);

  label = gtk_label_new (_("RDFS: "));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

  ns = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), ns, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND,
		    0, 0, 0);

  if (resource)
    gtk_entry_set_text (GTK_ENTRY (ns), resource);

  label = gtk_label_new (_("Prefix: "));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

  prefix = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), prefix, 1, 2, 1, 2,
		    GTK_FILL | GTK_EXPAND, 0, 0, 0);

  if (p)
    gtk_entry_set_text (GTK_ENTRY (prefix), p);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CANCEL);

  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  gtk_widget_show_all (dialog);

  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      gchar *ns_text, *prefix_text;
      GList *list, *nslist;
      struct rdfs_t *rdfs;
      gchar *file;
      gint must_continue;
      GString *error = NULL;

      /* Faccio alcuni controlli: */
      ns_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (ns));
      prefix_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (prefix));

      if (!ns_text || !*ns_text)
	{
	  dialog_msg (_("Incompatible RDFS!"));
	  continue;
	}

      if (!prefix_text || !*prefix_text)
	{
	  dialog_msg (_("Incompatible Prefix!"));
	  continue;
	}

      must_continue = 0;

      /* Cerco se c'e' gia' nell'elenco l'RDFS o il prefix: */
      list = rdfs_list;
      while (list)
	{
	  rdfs = (struct rdfs_t *) list->data;

	  if (!strcmp (rdfs->resource, ns_text))
	    {
	      dialog_msg (_("RDFS already present in the list!"));
	      must_continue = 1;
	      break;
	    }

	  if (!strcmp (rdfs->prefix, prefix_text))
	    {
	      gchar s[1024];
	      g_snprintf (s, sizeof (s),
			  _("Prefix already used for namespace '%s'"),
			  rdfs->resource);
	      dialog_msg (s);
	      must_continue = 1;
	      break;
	    }

	  list = list->next;
	}

      if (must_continue)
	continue;

      gtk_widget_hide (dialog);

      /* Parso elemento richiesto: */
      if (rdf_parse_thread
	  (ns_text, NULL, &list, &nslist, FALSE, NULL, &error) == FALSE)
	{
	  gchar *str;

	  if (error)
	    str = g_string_free (error, FALSE);
	  else
	    str = NULL;

	  dialog_msg_with_error (_("Error opening the RDFS!"), str);
	  gtk_widget_show (dialog);

	  if (str)
	    g_free (str);

	  continue;
	}

      /* Salvo in un file locale: */
      file = rdfs_file ();
      if (rdf_save_file (file, list, &nslist, RDF_FORMAT_XML) == FALSE)
	{
	  g_list_foreach (list, (GFunc) rdf_free, NULL);
	  g_list_free (list);

	  g_list_foreach (nslist, (GFunc) namespace_free, NULL);
	  g_list_free (nslist);

	  g_free (file);

	  dialog_msg (_("Error importing the RDFS document!"));
	  gtk_widget_show (dialog);
	  continue;
	}

      /* Alloco ed inserisco nella lista: */
      rdfs = (struct rdfs_t *) g_malloc0 (sizeof (struct rdfs_t));

      rdfs->resource = g_strdup (ns_text);
      rdfs->prefix = g_strdup (prefix_text);

      rdfs->data = list;
      rdfs->nsdata = nslist;
      rdfs->path = file;

      rdfs_list = g_list_append (rdfs_list, rdfs);

      /* Salvo la configurazione: */
      init_save ();

      ret = 1;
      break;
    }

  gtk_widget_destroy (dialog);

  return ret;
}

/* Libero memoria: */
static void
rdfs_free (struct rdfs_t *rdfs)
{
  if (!rdfs)
    return;

  if (rdfs->resource)
    g_free (rdfs->resource);

  if (rdfs->prefix)
    g_free (rdfs->prefix);

  if (rdfs->path)
    g_free (rdfs->path);

  if (rdfs->data)
    {
      g_list_foreach (rdfs->data, (GFunc) rdf_free, NULL);
      g_list_free (rdfs->data);
    }

  if (rdfs->nsdata)
    {
      g_list_foreach (rdfs->nsdata, (GFunc) namespace_free, NULL);
      g_list_free (rdfs->nsdata);
    }

  g_free (rdfs);
}

/* Questa funzione ritorna il nome del prossimo RDFS in base al last_Id
 * della configurazione: */
gchar *
rdfs_file (void)
{
  gchar s[1024];
  g_snprintf (s, sizeof (s), "%d", ++last_id);

  return g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		       "rdfs", s, NULL);
}

/* EOF */

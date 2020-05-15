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

/* Lista di struct rdf_t per il copia ed incolla */
static GList *edit_dump = NULL;

static void edit_remove (struct editor_data_t *data, GtkWidget * widget);
static gboolean check_item (struct rdf_t *rdf, gchar * subject, gint * id);
static void check_item_set_auto (GtkWidget * widget, GtkWidget * spin);
static gboolean check_list (gchar * str, GtkWindow * parent);
static gboolean event_box_popup_show (GtkWidget * w, GdkEventButton * event,
				      GtkWidget * label);

/* CUT, COPY, PASTE *********************************************************/

/* Questa funzione cancella le triplette selezionate */
void
editor_delete (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  GList *rdf;
  gint len;
  gboolean ok = FALSE;
  struct unredo_t *unredo;

  /* Ci devono essere i dati */
  if (!data || (!data->node_select && !data->node_nsselect))
    return;

  /* Mi salvo la lunghezza del namespace selezionato, altrimenti 0 */
  if (data->node_nsselect)
    len = strlen (data->node_nsselect);
  else
    len = 0;

  /* Creo una istanza di undo: tipologia DEL */
  unredo = unredo_new (data, UNREDO_TYPE_DEL);

  /* Per ogni tripletta di questo documento */
  rdf = data->rdf;
  while (rdf)
    {
      /* Se devo cancellare dato una tripletta selezionata */
      if ((data->node_select &&
	   /* e questa tripletta ha un soggetto che corrisponde */
	   !strcmp (((struct rdf_t *) rdf->data)->subject,
		    data->node_select)) ||
	  /* Oppure se devo cancellare dato ad un namespace selezionato */
	  (data->node_nsselect &&
	   /* e questo soggetto coincide */
	   !strncmp (((struct rdf_t *) rdf->data)->subject,
		     data->node_nsselect, len)))
	{
	  /* Cancello ma non libero la memoria perche' devo inserirlo... */
	  edit_remove (data, ((struct rdf_t *) rdf->data)->subject_widget);

	  /* ...in undo: */
	  unredo_add (unredo, rdf->data, NULL);

	  /* Ho fatto una modifica: */
	  ok = TRUE;

	  /* Passo al prossimo */
	  if (rdf->next)
	    {
	      rdf = rdf->next;
	      data->rdf = g_list_remove (data->rdf, rdf->prev->data);
	      continue;
	    }
	  else
	    {
	      data->rdf = g_list_remove (data->rdf, rdf->data);
	      break;
	    }
	}

      /* Lo stesso lo faccio per il predicato... */
      else
	if ((data->node_select
	     && !strcmp (((struct rdf_t *) rdf->data)->predicate,
			 data->node_select)) || (data->node_nsselect
						 &&
						 !strncmp (((struct rdf_t *)
							    rdf->data)->
							   predicate,
							   data->
							   node_nsselect,
							   len)))
	{
	  edit_remove (data, ((struct rdf_t *) rdf->data)->predicate_widget);
	  unredo_add (unredo, rdf->data, NULL);
	  ok = TRUE;

	  if (rdf->next)
	    {
	      rdf = rdf->next;
	      data->rdf = g_list_remove (data->rdf, rdf->prev->data);
	      continue;
	    }
	  else
	    {
	      data->rdf = g_list_remove (data->rdf, rdf->data);
	      break;
	    }
	}

      /* ... e per il coggetto */
      else
	if ((data->node_select
	     && !strcmp (((struct rdf_t *) rdf->data)->object,
			 data->node_select)) || (data->node_nsselect
						 &&
						 !strncmp (((struct rdf_t *)
							    rdf->data)->
							   object,
							   data->
							   node_nsselect,
							   len)))
	{
	  edit_remove (data, ((struct rdf_t *) rdf->data)->object_widget);
	  unredo_add (unredo, rdf->data, NULL);
	  ok = TRUE;

	  if (rdf->next)
	    {
	      rdf = rdf->next;
	      data->rdf = g_list_remove (data->rdf, rdf->prev->data);
	      continue;
	    }
	  else
	    {
	      data->rdf = g_list_remove (data->rdf, rdf->data);
	      break;
	    }
	}

      /* Cosi' fino alla fine... */
      rdf = rdf->next;
    }

  /* Se devo cancellare un namespace lo devo rimuovere dalla lista */
  if (data->node_nsselect)
    {
      GList *tmp = data->namespace;

      while (tmp)
	{
	  /* Se lo trovo, lo cancello e libero la memoria */
	  if (!strcmp
	      (((struct namespace_t *) tmp->data)->namespace,
	       data->node_nsselect))
	    {
	      ok = TRUE;
	      namespace_free (tmp->data);
	      data->namespace = g_list_remove (data->namespace, tmp->data);

	      /* Refresho l'interfaccia */
	      editor_refresh_nslist (data);
	      break;
	    }

	  tmp = tmp->next;
	}
    }

  /* Se ho fatto una modifica lo devo segnalare */
  data->changed = ok;
}

/* Questa funzione cencella solo la tripletta selezionata */
void
editor_delete_triple (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  struct unredo_t *unredo;

  /* Se non ci sono i dati base per compiere questa operazione */
  if (!data || !data->rdf_select)
    return;

  /* Istanza UNDO: */
  unredo = unredo_new (data, UNREDO_TYPE_DEL);
  unredo_add (unredo, data->rdf_select, NULL);

  /* Rimuovo: */
  data->rdf = g_list_remove (data->rdf, data->rdf_select);
  edit_remove (data, data->rdf_select->subject_widget);
  data->rdf_select = NULL;

  /* Modifica comunicata */
  data->changed = TRUE;
}

/* Il cut e' copy + delete */
void
editor_cut (GtkWidget * w, gpointer dummy)
{
  editor_copy (w, dummy);
  editor_delete (w, dummy);
}

/* Come sopra */
void
editor_cut_triple (GtkWidget * w, gpointer dummy)
{
  editor_copy_triple (w, dummy);
  editor_delete_triple (w, dummy);
}

/* Il copy: */
void
editor_copy (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  GList *rdf, *new = NULL;
  gint len;

  /* Devono esserci gli elementi base */
  if (!data || (!data->node_select && !data->node_nsselect))
    return;

  /* Mi salvo la lunghezza: */
  if (data->node_nsselect)
    len = strlen (data->node_nsselect);
  else
    len = 0;

  /* Per ogni tripletta di questo documento... */
  rdf = data->rdf;
  while (rdf)
    {
      /* Se devo copiare dato una tripletta... */
      if ((data->node_select &&
	   /* ... e questo soggetto coincide, */
	   !strcmp (((struct rdf_t *) rdf->data)->subject, data->node_select))
	  /* oppure se devo copiare un base ad un namespace... */
	  || (data->node_nsselect &&
	      /* ... e questo soggetto coincide... */
	      !strncmp (((struct rdf_t *)
			 rdf->data)->subject, data->node_nsselect, len)))
	{
	  /* Copio nella lista NEW un dump della tripletta: */
	  new = g_list_append (new, rdf_copy (rdf->data));
	}

      /* Idem per il predicato */
      else
	if ((data->node_select
	     && !strcmp (((struct rdf_t *) rdf->data)->predicate,
			 data->node_select)) || (data->node_nsselect
						 &&
						 !strncmp (((struct rdf_t *)
							    rdf->data)->
							   predicate,
							   data->
							   node_nsselect,
							   len)))
	new = g_list_append (new, rdf_copy (rdf->data));

      /* Per il c.oggetto */
      else
	if ((data->node_select
	     && !strcmp (((struct rdf_t *) rdf->data)->object,
			 data->node_select)) || (data->node_nsselect
						 &&
						 !strncmp (((struct rdf_t *)
							    rdf->data)->
							   object,
							   data->
							   node_nsselect,
							   len)))
	new = g_list_append (new, rdf_copy (rdf->data));

      rdf = rdf->next;
    }

  /* Se ho una copia precedente, la libero: */
  if (edit_dump)
    {
      g_list_foreach (edit_dump, (GFunc) rdf_free, NULL);
      g_list_free (edit_dump);
    }

  /* Metto NEW dentro edit_dump: */
  edit_dump = new;
}

/* Questo copia una sola tripletta: */
void
editor_copy_triple (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();

  if (!data || !data->rdf_select)
    return;

  /* Libero memoria: */
  if (edit_dump)
    {
      g_list_foreach (edit_dump, (GFunc) rdf_free, NULL);
      g_list_free (edit_dump);
    }

  /* Dump: */
  edit_dump = g_list_append (NULL, rdf_copy (data->rdf_select));
}

/* Questa funzione incolla: */
void
editor_paste (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  struct unredo_t *unredo;
  struct rdf_t *rdf;
  GList *list;

  /* Devo avere un qualcosa copiato: */
  if (!data || !edit_dump)
    return;

  /* Istanza di undo: */
  unredo = unredo_new (data, UNREDO_TYPE_ADD);

  /* Per ognuno della lista di dump... */
  list = edit_dump;
  while (list)
    {
      /* Dumplico la tripletta */
      rdf = rdf_copy (list->data);
      /* Inserisco in undo come inserimento */
      unredo_add (unredo, rdf, NULL);
      /* Appendo alla lista delle triplette per questo documento */
      data->rdf = g_list_prepend (data->rdf, rdf);

      list = list->next;
    }

  /* Libero i namespace precedenti: */
  g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
  g_list_free (data->namespace);

  /* Parso i namespace nuovamente: */
  namespace_parse (data->rdf, &data->namespace);

  /* Comunico il cambiamento: */
  data->changed = TRUE;

  /* Refresh dell'interfaccia: */
  editor_refresh (data);
}

/* LISTS ********************************************************************/

/* Questa funzione data una lista, la restituisce senza doppioni: */
GList *
create_no_dup (GList * ret)
{
  gchar *prev;
  GList *list;

  /* Ordino per nome: */
  ret = list = g_list_sort (ret, (GCompareFunc) strcmp);
  prev = NULL;

  /* Per ogni elemento della lista */
  while (list)
    {
      /* Se non esiste un precedente oppure se quello attuale e' diverso
       * dal precedente, aggiorno il precedente: */
      if (!prev || strcmp (prev, list->data))
	prev = (gchar *) list->data;

      /* Altrimenti rimuovo: */
      else
	{
	  if (list->next)
	    {
	      list = list->next;
	      ret = g_list_remove (ret, list->prev->data);
	      continue;
	    }
	  else
	    {
	      ret = g_list_remove (ret, list->data);
	      break;
	    }
	}

      list = list->next;
    }

  /* Riconsegno: */
  return ret;
}

/* Questa funzione crea una lista con tutte le risorse esistenti in questo
 * documento: */
GList *
create_resources (struct editor_data_t * data)
{
  GList *list = data->rdf;
  GList *ret = NULL;
  struct rdf_t *rdf;

  /* Per ogni tripletta... */
  while (list)
    {
      rdf = list->data;

      /* Il soggetto e' sempre una risorsa: */
      ret = g_list_append (ret, rdf->subject);

      /* ... e a volte anche il c.oggetto: */
      if (rdf->object_type != RDF_OBJECT_LITERAL)
	ret = g_list_append (ret, rdf->object);

      list = list->next;
    }

  /* Tolgo i dublicati e consegno: */
  return create_no_dup (ret);
}

/* Questa funzione crea una lista con tutte le risorse degli RDFS: */
GList *
create_resources_by_rdfs (GList * ret)
{
  GList *list, *l;
  struct rdfs_t *rdfs;
  struct rdf_t *rdf;

  /* Per ogni rdfs: */
  list = rdfs_list;
  while (list)
    {
      rdfs = list->data;

      /* per ogni tripletta di questo rdfs: */
      l = rdfs->data;
      while (l)
	{
	  rdf = l->data;

	  /* il soggetto se non e' un blank node: */
	  if (!rdf_blank (rdfs->data, rdf->subject))
	    ret = g_list_append (ret, rdf->subject);

	  /* il complemento oggetto se e' una risorsa: */
	  if (rdf->object_type == RDF_OBJECT_RESOURCE)
	    ret = g_list_append (ret, rdf->object);

	  l = l->next;
	}

      list = list->next;
    }

  /* No duplicati: */
  return create_no_dup (ret);
}

/* Questa funzione crea una lista con tutti i blank node: */
GList *
create_blank (struct editor_data_t * data)
{
  GList *list = data->rdf;
  GList *ret = NULL;
  struct rdf_t *rdf;

  /* Per ogni tripletta di questo documento: */
  while (list)
    {
      rdf = list->data;

      /* Se e' un blank node: */
      if (rdf->object_type == RDF_OBJECT_BLANK)
	ret = g_list_append (ret, rdf->object);

      list = list->next;
    }

  /* no duplicati: */
  return create_no_dup (ret);
}

/* SEARCH ********************************************************************/

static gboolean
search_resources_cmp_fragment (const gchar * a, const gchar * b)
{
  gchar *a1, *b1, *c1;

  a1 = g_utf8_casefold (a, -1);
  b1 = g_utf8_casefold (b, -1);

  c1 = g_strrstr (a1, b1);

  g_free (a1);
  g_free (b1);

  return c1 ? TRUE : FALSE;
}

static gboolean
search_resources_cmp (const gchar * a, const gchar * b)
{
  gchar **part, **old;
  gboolean ret = 1;

  old = part = g_strsplit (b, " ", 0);

  if (!part)
    return 0;

  while (*part)
    {

      if (!(ret = search_resources_cmp_fragment (a, *part)))
	break;

      part++;
    }

  g_strfreev (old);

  return ret;
}

static void
search_resources_changed (GtkWidget * entry, GtkListStore * store)
{
  GtkTreeIter iter;
  const gchar *text;
  GList *list;

  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (store), &iter, NULL, 0))
    gtk_list_store_remove (store, &iter);

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  list = g_object_get_data (G_OBJECT (entry), "list");

  while (list)
    {
      if ((text && text[0] && search_resources_cmp (list->data, text))
	  || (!text || !text[0]))
	{
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter, 0, list->data, -1);
	}

      list = list->next;
    }
}

static void
search_resources_clicked (GtkWidget * button, GtkListStore * store)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (button), "entry");

  search_resources_changed (entry, store);
}

static void
search_resources_select (GtkWidget * widget, GtkTreePath * p,
			 GtkTreeViewColumn * c, GtkWidget * dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

void
search_resources (GtkWidget * widget, GtkWidget * entry)
{
  GList *list;
  gchar *text;

  list = g_object_get_data (G_OBJECT (widget), "list");

  if ((text =
       search_list (list, GTK_WINDOW (gtk_widget_get_toplevel (widget)))))
    {
      gtk_entry_set_text (GTK_ENTRY (entry), text);
      g_free (text);
    }
}

gchar *
search_list (GList * list, GtkWindow * parent)
{
  GtkWidget *dialog;
  GtkWidget *e;
  GtkWidget *box;
  GtkWidget *sw;
  GtkWidget *tv;
  GtkWidget *button;
  GtkWidget *image;
  GtkListStore *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;
  gchar s[1024];
  gchar *text = NULL;

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Search datas..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  gtk_widget_set_size_request (dialog, 500, 300);

  box = gtk_hbox_new (0, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), box, FALSE, FALSE,
		      0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), sw, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  model = gtk_list_store_new (1, G_TYPE_STRING);

  tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tv), FALSE);
  gtk_container_add (GTK_CONTAINER (sw), tv);

  g_signal_connect ((gpointer) tv, "row_activated",
		    G_CALLBACK (search_resources_select), dialog);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tv));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tv), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tv), -1,
					       _("Results"), renderer,
					       "text", 0, NULL);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_NO);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  e = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (box), e, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (e), "list", list);
  g_signal_connect (G_OBJECT (e), "changed",
		    G_CALLBACK (search_resources_changed), model);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "entry", e);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (search_resources_clicked), model);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  while (list)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, list->data, -1);
      list = list->next;
    }

  gtk_widget_show_all (dialog);

  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      if (!gtk_tree_selection_count_selected_rows (selection))
	{
	  dialog_msg (_("Select a item of the list!"));
	  continue;
	}

      if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0, &text, -1);

      break;
    }

  g_object_unref (model);
  gtk_widget_destroy (dialog);

  return text;
}

/* ADD CALLBACK **************************************************************/

/* Aggiunge al GtkListStore delle proprieta' quelle che si rieferiscono
 * all'rdfs dato l'ID della lista rdfs_list */
static void
add_items (GtkListStore * store, gint id)
{
  GList *list;
  GList *end;
  GtkTreeIter iter;
  struct rdf_t *rdf;

  /* Cerco l'rdfs nella lista rdfs_list */
  if (!(list = g_list_nth (rdfs_list, id)))
    return;

  /* Rimuovo le proprieta' prevedenti: */
  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (store), &iter, NULL, 0))
    gtk_list_store_remove (store, &iter);

  /* Per ogni tripletta dell'rdfs... */
  list = ((struct rdfs_t *) list->data)->data;
  end = NULL;

  while (list)
    {
      rdf = list->data;

      /* Se e': <what> <rdf:type> <rdf:property> oppure,
         <what> <rdf:type> <owl:datatypeproperty> oppure,
         <what> <rdf:type> <owl:objectproperty> appendo alla lista: */
      if (rdf->object_type == RDF_OBJECT_RESOURCE
	  && !strcmp (rdf->predicate, RDF_TYPE)
	  && (!strcmp (rdf->object, RDF_PROPERTY)
	      || !strcmp (rdf->object, OWL_O_PROPERTY)
	      || !strcmp (rdf->object, OWL_DT_PROPERTY)))
	{
	  end = g_list_append (end, rdf->subject);
	}

      /* Cosi' fino alla fine: */
      list = list->next;
    }

  list = end = create_no_dup (end);

  while (list)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, list->data, -1);

      list = list->next;
    }

  g_list_free (end);
}

/* Quesa funzione aggiorna i campi GtkLabel con i commenti, label, domain,
 * range della propieta' selezionata: */
static void
info_changed (GtkWidget * w, GtkWidget * widget)
{
  GList *comments = NULL, *labels = NULL;

  /* ID della proprieta' dato il combobox */
  gint id_p = gtk_combo_box_get_active (GTK_COMBO_BOX (w));

  /* ID del rdfs dato il combo box: */
  gint id_n = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  /* Tutti gli oggetti GtkLabel che devo aggiornare: */
  GtkWidget *label = g_object_get_data (G_OBJECT (w), "label");
  GtkWidget *comment = g_object_get_data (G_OBJECT (w), "comment");
  GtkWidget *domain = g_object_get_data (G_OBJECT (w), "domain");
  GtkWidget *range = g_object_get_data (G_OBJECT (w), "range");

  GtkTreeModel *model;
  GtkTreeIter iter;

  GList *rdfs, *list;
  gchar *what = NULL;

  struct info_box_t *info;

  /* Annullo il contenuto: */
  gtk_label_set_text (GTK_LABEL (label), "");
  gtk_label_set_text (GTK_LABEL (comment), "");
  gtk_label_set_text (GTK_LABEL (domain), "");
  gtk_label_set_text (GTK_LABEL (range), "");

  /* Cerco il base all'ID l'rdfs: */
  if (!(rdfs = g_list_nth (rdfs_list, id_n)))
    return;

  /* Cerco la proprieta' in base al GtkTreeModel: */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (w));

  if (id_p < 0 || !gtk_tree_model_iter_nth_child (model, &iter, NULL, id_p))
    return;

  gtk_tree_model_get (model, &iter, 0, &what, -1);

  if (!what)
    return;

  /* Cerco in base all'RDFS... */
  list = ((struct rdfs_t *) rdfs->data)->data;
  while (list)
    {
      struct rdf_t *rdf = list->data;

      /* ... in corrispondenza del soggetto: */
      if (!strcmp (rdf->subject, what))
	{

	  /* ... il commento: */
	  if (!strcmp (rdf->predicate, RDFS_COMMENT)
	      && rdf->object_type == RDF_OBJECT_LITERAL)
	    {
	      /* I commenti possono essere multilungua,
	       * e quindi ne creo una lista */
	      info = g_malloc0 (sizeof (struct info_box_t));

	      info->lang = rdf->lang ? g_strdup (rdf->lang) : NULL;
	      info->value = g_strdup (rdf->object);

	      comments = g_list_append (comments, info);
	    }

	  /* ... la label */
	  else if (!strcmp (rdf->predicate, RDFS_LABEL)
		   && rdf->object_type == RDF_OBJECT_LITERAL)
	    {
	      /* tratto i label come i commenti */
	      info = g_malloc0 (sizeof (struct info_box_t));

	      info->lang = rdf->lang ? g_strdup (rdf->lang) : NULL;
	      info->value = g_strdup (rdf->object);

	      labels = g_list_append (labels, info);
	    }

	  /* ... il dominio */
	  else if (!strcmp (rdf->predicate, RDFS_DOMAIN))
	    gtk_label_set_text (GTK_LABEL (domain), rdf->object);

	  /* ... il range */
	  else if (!strcmp (rdf->predicate, RDFS_RANGE))
	    gtk_label_set_text (GTK_LABEL (range), rdf->object);
	}

      list = list->next;
    }

  /* Se c'e' un commento: */
  if (comments)
    {
      /* Cerco quelli senza lingua: */
      list = comments;
      while (list)
	{
	  info = list->data;

	  if (!info->lang)
	    {
	      gtk_label_set_text (GTK_LABEL (comment), info->value);
	      break;
	    }

	  list = list->next;
	}

      if (!list)
	{
	  info = comments->data;
	  gtk_label_set_text (GTK_LABEL (comment), info->value);
	}

      if ((list = g_object_get_data (G_OBJECT (comment), "data")))
	{
	  g_list_foreach (list, (GFunc) info_box_free, NULL);
	  g_list_free (list);
	}

      g_object_set_data (G_OBJECT (comment), "data", comments);
    }

  /* Idem per i labels: */
  if (labels)
    {
      list = labels;
      while (list)
	{
	  info = list->data;

	  if (!info->lang)
	    {
	      gtk_label_set_text (GTK_LABEL (label), info->value);
	      break;
	    }

	  list = list->next;
	}

      if (!list)
	{
	  info = labels->data;
	  gtk_label_set_text (GTK_LABEL (label), info->value);
	}

      if ((list = g_object_get_data (G_OBJECT (label), "data")))
	{
	  g_list_foreach (list, (GFunc) info_box_free, NULL);
	  g_list_free (list);
	}

      g_object_set_data (G_OBJECT (label), "data", labels);
    }
}

/* Questa funzione viene azionata quando viene cambiato il combobox degli
 * rdfs: */
static void
property_changed (GtkWidget * widget, GtkWidget * w)
{
  GtkListStore *store =
    (GtkListStore *) gtk_combo_box_get_model (GTK_COMBO_BOX (w));
  gint id = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  /* Lancio l'aggiornamento delle proprieta': */
  add_items (store, id);

  /* Setto attivo l'item 0: */
  gtk_combo_box_set_active (GTK_COMBO_BOX (w), 0);

  /* Aggiorno le info: */
  info_changed (w, widget);
}

/* Questa funzione e' lanciata quando si distruggono gli oggetti di
 * suggerimento che contengono delle liste */
static void
destroy_list (GtkWidget * w, gpointer dummy)
{
  GList *list = g_object_get_data (G_OBJECT (w), "data");

  if (list)
    {
      g_list_foreach (list, (GFunc) info_box_free, NULL);
      g_list_free (list);
    }
}

/* Questa funzione viene avviata quando si seleziona il predicato o
 * il predicato ad item */
static void
toggled_predicate (GtkWidget * w, gpointer dummy)
{
  GtkWidget *f_predicate;
  GtkWidget *f_item;
  GtkWidget *b_predicate;
  GtkWidget *b_item;

  /* I valori che devo gestire */
  f_predicate = g_object_get_data (G_OBJECT (w), "frame_predicate");
  f_item = g_object_get_data (G_OBJECT (w), "frame_item");
  b_predicate = g_object_get_data (G_OBJECT (w), "button_predicate");
  b_item = g_object_get_data (G_OBJECT (w), "button_item");

  /* Se e' stato selezionato il predicato, rendo sensibili le sue sezioni
   * e non le altre */
  if (b_predicate == w)
    {
      gtk_widget_set_sensitive (f_predicate, TRUE);
      gtk_widget_set_sensitive (f_item, FALSE);
    }

  /* il contraio: */
  else
    {
      gtk_widget_set_sensitive (f_predicate, FALSE);
      gtk_widget_set_sensitive (f_item, TRUE);
    }
}

/* Questa funzione viene azionata dai radiobutton della tipologia coggetto */
static void
toggled_object (GtkWidget * w, gpointer dummy)
{
  GtkWidget *t_literal;
  GtkWidget *t_resource;
  GtkWidget *t_blanknode;
  GtkWidget *object;
  GtkWidget *lang;
  GtkWidget *datatype;
  GList *resources;
  GList *blanks;

  /* Una serie di valori che mi servono: */
  t_literal = (GtkWidget *) g_object_get_data (G_OBJECT (w), "literal");
  t_resource = (GtkWidget *) g_object_get_data (G_OBJECT (w), "resource");
  t_blanknode = (GtkWidget *) g_object_get_data (G_OBJECT (w), "blanknode");
  blanks = (GList *) g_object_get_data (G_OBJECT (w), "blank_list");
  resources = (GList *) g_object_get_data (G_OBJECT (w), "resources_list");
  object = (GtkWidget *) g_object_get_data (G_OBJECT (w), "object");
  lang = (GtkWidget *) g_object_get_data (G_OBJECT (w), "lang");
  datatype = (GtkWidget *) g_object_get_data (G_OBJECT (w), "datatype");

  /* Se e' stato selezionato un letterale non ci devono essere suggerimenti
   * e la lungua e' sensibile:*/
  if (w == t_literal)
    {
      g_object_steal_data (G_OBJECT (object), "list");
      gtk_widget_set_sensitive (lang, TRUE);
      gtk_widget_set_sensitive (datatype, TRUE);
    }

  /* Negli altri casi cambia la tipologia di suggerimento: */
  else if (w == t_resource)
    {
      g_object_set_data (G_OBJECT (object), "list", resources);
      gtk_widget_set_sensitive (lang, FALSE);
      gtk_widget_set_sensitive (datatype, FALSE);
    }

  else
    {
      g_object_set_data (G_OBJECT (object), "list", blanks);
      gtk_widget_set_sensitive (lang, FALSE);
      gtk_widget_set_sensitive (datatype, FALSE);
    }
}

/* ADD FUNCTIONS *************************************************************/

/* Questa funzione aggiunge una tripletta all documento RDF: */
void
editor_add (GtkWidget * w, gpointer dummy)
{
  struct rdf_t *rdf;
  struct editor_data_t *data = editor_get_data ();
  struct unredo_t *unredo;

  if (!data)
    return;

  /* Uso editor_triple per la creazione della tripletta */
  if (!(rdf = editor_triple (NULL, _("Add RDF triple..."))))
    return;

  /* Attivo un undo in modalita' ADD: */
  unredo = unredo_new (data, UNREDO_TYPE_ADD);
  unredo_add (unredo, rdf, NULL);

  /* Aggiungo la tripletta: */
  data->rdf = g_list_append (data->rdf, rdf);

  /* Refreshio i namespace e li rigenero */
  g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
  g_list_free (data->namespace);

  namespace_parse (data->rdf, &data->namespace);

  /* Comunico il cambiamento */
  data->changed = TRUE;

  /* Refreshio: */
  editor_refresh (data);
}

/* Questa funzione crea realmente una tripletta. Se gli si da' un prev_rdf
 * setta dei valori di default: */
struct rdf_t *
editor_triple (struct rdf_t *prev_rdf, gchar * title)
{
  GtkWidget *dialog;
  GtkWidget *box, *vbox, *hbox;
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *subject;
  GtkWidget *namespace;
  GtkWidget *property;
  GtkWidget *object;
  GtkWidget *lang;
  GtkWidget *datatype;
  GtkWidget *image;
  GtkWidget *type_literal;
  GtkWidget *type_resource;
  GtkWidget *type_blanknode;
  GtkWidget *button_predicate, *button_item;
  GtkWidget *button_subject, *button_subject_this;
  GtkWidget *frame_predicate, *frame_item;
  GtkTreeIter iter;
  GtkListStore *store;
  GtkCellRenderer *renderer;
  GtkWidget *item;
  GtkWidget *event;
  GtkObject *adj;
  GList *list;
  GList *resources;
  GList *blanks;
  struct editor_data_t *data = editor_get_data ();
  GSList *l;
  struct rdfs_t *rdfs;
  struct rdf_t *rdf = NULL;
  gint id = 0;
  gchar s[1024];
  struct datatype_t *datatypes = datatype_list ();

  /* Se non c'e' il documento: */
  if (!data)
    return NULL;

  /* Creo le liste: */
  resources = create_resources (data);
  resources = create_resources_by_rdfs (resources);
  blanks = create_blank (data);

  /* interfaccia: */
  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION, title);
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  /* Soggetto risorsa: */
  label = gtk_label_new (_("<b>Subject</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE,
		      5);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  l = NULL;
  button = gtk_radio_button_new_with_label (NULL, _("Resource: "));
  button_subject = button;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 3);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 5);

  /* Creazione soggetto con freccia: */
  subject = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), subject, TRUE, TRUE, 0);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "list", resources);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), subject);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  button = gtk_radio_button_new_with_label (NULL, _("This Document"));
  button_subject_this = button;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 3);

  if (prev_rdf && !strcmp (prev_rdf->subject, THIS_DOCUMENT))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_subject_this),
				  TRUE);
  else
    {
      if (prev_rdf)
	gtk_entry_set_text (GTK_ENTRY (subject), prev_rdf->subject);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_subject), TRUE);
    }

  l = NULL;
  label = gtk_label_new (_("<b>Predicate</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  /* Radio button per il predicato */
  button = gtk_radio_button_new (NULL);
  button_predicate = button;
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), button);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE,
		      5);

  vbox = gtk_vbox_new (FALSE, 5);
  frame_predicate = vbox;
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new (_("RDFS: "));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  /* Aggiungo gli RDFs ad un store: */
  store = gtk_list_store_new (1, G_TYPE_STRING);

  list = rdfs_list;

  while (list)
    {
      rdfs = list->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, rdfs->resource, -1);

      list = list->next;
    }

  namespace = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (namespace), 0);
  gtk_box_pack_start (GTK_BOX (box), namespace, TRUE, TRUE, 5);

  /* Se esiste un precedente e non e' di una tipologia LISTA... */
  if (prev_rdf && strncmp (prev_rdf->predicate, RDF_ITEM, strlen (RDF_ITEM)))
    {
      gint len;
      struct namespace_t *ns;
      gchar *prev_ns = NULL;

      /* lo ricerco: */
      list = data->namespace;
      while (list)
	{
	  ns = (struct namespace_t *) list->data;

	  /* attenzione agli '/' perche' potrebbero esserci namespace
	   * con un uri pressoche' identico ma + breve: */
	  len = strlen (ns->namespace);
	  if (!strncmp (ns->namespace, prev_rdf->predicate, len)
	      && !strstr (prev_rdf->predicate + len, "/"))
	    {
	      prev_ns = ns->namespace;
	      break;
	    }

	  list = list->next;
	}

      /* Se ho trovato il precedente, lo vado a cercare: */
      if (prev_ns
	  && gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store),
					    &iter) == TRUE)
	{
	  gchar *tmp;
	  id = 0;

	  do
	    {
	      gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &tmp, -1);

	      /* Se coicide, lo attivo: */
	      if (!strcmp (prev_ns, tmp))
		{
		  gtk_combo_box_set_active (GTK_COMBO_BOX (namespace), id);
		  g_free (tmp);
		  break;
		}

	      g_free (tmp);
	      id++;
	    }
	  /* Cosi' per ogni elemento dello store */
	  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
	}
    }

  /* Se non l'ho trovato o se non l'ho cercato: id e' 0: */
  else
    id = 0;

  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (namespace), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (namespace), renderer,
				  "text", 0, NULL);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new (_("Property: "));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  /* Creo lo store e lo riempo con i valori: */
  store = gtk_list_store_new (1, G_TYPE_STRING);
  add_items (store, prev_rdf ? id : 0);

  property = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (property), 0);
  gtk_box_pack_start (GTK_BOX (box), property, TRUE, TRUE, 5);

  /* Se c'e' un precedente e' questo non e' una LISTA */
  if (prev_rdf && strncmp (prev_rdf->predicate, RDF_ITEM, strlen (RDF_ITEM))
      && gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store),
					&iter) == TRUE)
    {
      gchar *tmp;
      id = 0;

      do
	{
	  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &tmp, -1);

	  /* Vado alla ricerca e se lo trovo lo seleziono: */
	  if (!strcmp (prev_rdf->predicate, tmp))
	    {
	      gtk_combo_box_set_active (GTK_COMBO_BOX (property), id);
	      g_free (tmp);
	      break;
	    }

	  g_free (tmp);
	  id++;
	}
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
    }

  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (property), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (property), renderer,
				  "text", 0, NULL);

  /* I campi di informazione: */
  frame = gtk_frame_new (_("Information"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 5);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 2);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Label: "));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  event = gtk_event_box_new ();
  gtk_widget_set_events (event, GDK_BUTTON_PRESS_MASK);
  gtk_box_pack_start (GTK_BOX (box), event, TRUE, TRUE, 5);

  g_signal_connect (G_OBJECT (event), "enter-notify-event",
		    G_CALLBACK (info_box_enter), NULL);
  g_signal_connect (G_OBJECT (event), "leave-notify-event",
		    G_CALLBACK (info_box_leave), NULL);

  label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (event), label);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  g_object_set_data (G_OBJECT (property), "label", label);
  g_signal_connect (G_OBJECT (label), "destroy", G_CALLBACK (destroy_list),
		    NULL);

  g_signal_connect (G_OBJECT (event), "button_press_event",
		    G_CALLBACK (event_box_popup_show), label);
  g_signal_connect (G_OBJECT (event), "button_release_event",
		    G_CALLBACK (info_box_popup_hide), NULL);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 2);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Comment: "));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  event = gtk_event_box_new ();
  gtk_widget_set_events (event, GDK_BUTTON_PRESS_MASK);
  gtk_box_pack_start (GTK_BOX (box), event, TRUE, TRUE, 5);

  g_signal_connect (G_OBJECT (event), "enter-notify-event",
		    G_CALLBACK (info_box_enter), NULL);
  g_signal_connect (G_OBJECT (event), "leave-notify-event",
		    G_CALLBACK (info_box_leave), NULL);

  label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (event), label);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  g_object_set_data (G_OBJECT (property), "comment", label);
  g_signal_connect (G_OBJECT (label), "destroy", G_CALLBACK (destroy_list),
		    NULL);

  g_signal_connect (G_OBJECT (event), "button_press_event",
		    G_CALLBACK (event_box_popup_show), label);
  g_signal_connect (G_OBJECT (event), "button_release_event",
		    G_CALLBACK (info_box_popup_hide), NULL);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 2);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Domain: "));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  label = gtk_label_new ("");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 5);
  g_object_set_data (G_OBJECT (property), "domain", label);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 2);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Range: "));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  label = gtk_label_new ("");
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 5);
  g_object_set_data (G_OBJECT (property), "range", label);

  info_changed (property, namespace);

  g_signal_connect ((gpointer) namespace, "changed",
		    G_CALLBACK (property_changed), property);

  g_signal_connect ((gpointer) property, "changed",
		    G_CALLBACK (info_changed), namespace);

  label = gtk_label_new (_("<b>Predicate Item</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  /* Predicate Item radio button */
  button = gtk_radio_button_new (NULL);
  button_item = button;
  gtk_container_add (GTK_CONTAINER (button), label);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), button);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE,
		      5);

  box = gtk_hbox_new (FALSE, 5);
  frame_item = box;
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Number: "));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  adj = gtk_adjustment_new (0, 0, 9999, 1, 10, 100);
  item = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (box), item, TRUE, TRUE, 5);

  /* Qui c'e' uno spin button settato al valore del precedente: */
  if (prev_rdf && !strncmp (prev_rdf->predicate, RDF_ITEM, strlen (RDF_ITEM)))
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (item),
			       atoi (prev_rdf->predicate +
				     strlen (RDF_ITEM)));

  button = gtk_button_new_with_label (_("Auto"));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 5);
  g_object_set_data (G_OBJECT (button), "subject", subject);
  g_object_set_data (G_OBJECT (button), "subject_this", button_subject_this);
  g_signal_connect_after ((gpointer) button, "clicked",
			  G_CALLBACK (check_item_set_auto), item);

  /* Setto tutte le variabili che mi serviranno */
  g_object_set_data (G_OBJECT (button_item), "frame_predicate",
		     frame_predicate);
  g_object_set_data (G_OBJECT (button_item), "frame_item", frame_item);
  g_object_set_data (G_OBJECT (button_item), "button_predicate",
		     button_predicate);
  g_object_set_data (G_OBJECT (button_item), "button_item", button_item);

  g_object_set_data (G_OBJECT (button_predicate), "frame_predicate",
		     frame_predicate);
  g_object_set_data (G_OBJECT (button_predicate), "frame_item", frame_item);
  g_object_set_data (G_OBJECT (button_predicate), "button_predicate",
		     button_predicate);
  g_object_set_data (G_OBJECT (button_predicate), "button_item", button_item);

  g_signal_connect_after ((gpointer) button_predicate, "toggled",
			  G_CALLBACK (toggled_predicate), NULL);
  g_signal_connect_after ((gpointer) button_item, "toggled",
			  G_CALLBACK (toggled_predicate), NULL);

  /* Do una preconfigurazione: */
  if (!prev_rdf)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_predicate),
				    TRUE);
      toggled_predicate (button_predicate, NULL);
    }
  else
    {
      if (!strncmp (prev_rdf->predicate, RDF_ITEM, strlen (RDF_ITEM)))
	{
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_item),
					TRUE);
	  toggled_predicate (button_item, NULL);
	}
      else
	{
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_predicate),
					TRUE);
	  toggled_predicate (button_predicate, NULL);
	}
    }

  label = gtk_label_new (_("<b>Object</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE,
		      TRUE, 5);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new ("Type: ");
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  /* Possibilita' delle tipologie per il c.oggettto: */
  l = NULL;
  type_literal = gtk_radio_button_new_with_label (NULL, _("Literal"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (type_literal), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (type_literal));
  gtk_box_pack_start (GTK_BOX (box), type_literal, TRUE, TRUE, 3);

  type_resource = gtk_radio_button_new_with_label (NULL, _("Resource"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (type_resource), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (type_resource));
  gtk_box_pack_start (GTK_BOX (box), type_resource, TRUE, TRUE, 3);

  type_blanknode = gtk_radio_button_new_with_label (NULL, _("Blank node"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (type_blanknode), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (type_blanknode));
  gtk_box_pack_start (GTK_BOX (box), type_blanknode, TRUE, TRUE, 3);

  /* Preconfigurazione: */
  if (prev_rdf)
    {
      switch (prev_rdf->object_type)
	{
	case RDF_OBJECT_LITERAL:
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type_literal),
					TRUE);
	  break;

	case RDF_OBJECT_RESOURCE:
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type_resource),
					TRUE);
	  break;

	case RDF_OBJECT_BLANK:
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
					(type_blanknode), TRUE);
	  break;
	}
    }
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type_literal), TRUE);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new (_("Language: "));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  lang = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (box), lang, TRUE, TRUE, 5);

  if (prev_rdf && prev_rdf->lang)
    gtk_entry_set_text (GTK_ENTRY (lang), prev_rdf->lang);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 5);

  box = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), box);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 5);

  label = gtk_label_new (_("Datatype: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  datatype = gtk_combo_box_entry_new_text ();
  gtk_box_pack_start (GTK_BOX (hbox), datatype, TRUE, TRUE, 5);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

  gtk_combo_box_append_text (GTK_COMBO_BOX (datatype), datatypes[0].uri);
  gtk_combo_box_set_active (GTK_COMBO_BOX (datatype), 0);

  for (id = 1; datatypes[id].uri; id++)
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (datatype), datatypes[id].uri);

      if (prev_rdf && prev_rdf->datatype
	  && !strcmp (prev_rdf->datatype, datatypes[id].uri))
	gtk_combo_box_set_active (GTK_COMBO_BOX (datatype), id);
    }

  datatype_change (datatype, label);
  g_signal_connect_after ((gpointer) datatype, "changed",
			  G_CALLBACK (datatype_change), label);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new (_("Value: "));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 5);

  object = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), object, TRUE, TRUE, 0);

  if (prev_rdf)
    gtk_entry_set_text (GTK_ENTRY (object), prev_rdf->object);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), object);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  /* In base al precedente setto dei flag nella lista di suggerimento: */
  if (prev_rdf)
    {
      switch (prev_rdf->object_type)
	{
	case RDF_OBJECT_BLANK:
	  g_object_set_data (G_OBJECT (button), "list", blanks);
	  break;

	case RDF_OBJECT_RESOURCE:
	  g_object_set_data (G_OBJECT (button), "list", resources);
	  break;

	default:
	  break;
	}
    }

  /* Variabili per le funzioni di callback: */
  g_object_set_data (G_OBJECT (type_literal), "literal", type_literal);
  g_object_set_data (G_OBJECT (type_literal), "resource", type_resource);
  g_object_set_data (G_OBJECT (type_literal), "blanknode", type_blanknode);
  g_object_set_data (G_OBJECT (type_literal), "blank_list", blanks);
  g_object_set_data (G_OBJECT (type_literal), "resources_list", resources);
  g_object_set_data (G_OBJECT (type_literal), "object", button);
  g_object_set_data (G_OBJECT (type_literal), "lang", lang);
  g_object_set_data (G_OBJECT (type_literal), "datatype", datatype);

  g_object_set_data (G_OBJECT (type_resource), "literal", type_literal);
  g_object_set_data (G_OBJECT (type_resource), "resource", type_resource);
  g_object_set_data (G_OBJECT (type_resource), "blanknode", type_blanknode);
  g_object_set_data (G_OBJECT (type_resource), "blank_list", blanks);
  g_object_set_data (G_OBJECT (type_resource), "resources_list", resources);
  g_object_set_data (G_OBJECT (type_resource), "object", button);
  g_object_set_data (G_OBJECT (type_resource), "lang", lang);
  g_object_set_data (G_OBJECT (type_resource), "datatype", datatype);

  g_object_set_data (G_OBJECT (type_blanknode), "literal", type_literal);
  g_object_set_data (G_OBJECT (type_blanknode), "resource", type_resource);
  g_object_set_data (G_OBJECT (type_blanknode), "blanknode", type_blanknode);
  g_object_set_data (G_OBJECT (type_blanknode), "blank_list", blanks);
  g_object_set_data (G_OBJECT (type_blanknode), "resources_list", resources);
  g_object_set_data (G_OBJECT (type_blanknode), "object", button);
  g_object_set_data (G_OBJECT (type_blanknode), "lang", lang);
  g_object_set_data (G_OBJECT (type_blanknode), "datatype", datatype);

  g_signal_connect_after ((gpointer) type_literal, "toggled",
			  G_CALLBACK (toggled_object), NULL);
  g_signal_connect_after ((gpointer) type_blanknode, "toggled",
			  G_CALLBACK (toggled_object), NULL);
  g_signal_connect_after ((gpointer) type_resource, "toggled",
			  G_CALLBACK (toggled_object), NULL);

  if (prev_rdf)
    {
      switch (prev_rdf->object_type)
	{
	case RDF_OBJECT_LITERAL:
	  toggled_object (type_literal, NULL);
	  break;

	case RDF_OBJECT_BLANK:
	  toggled_object (type_blanknode, NULL);
	  break;

	case RDF_OBJECT_RESOURCE:
	default:
	  toggled_object (type_resource, NULL);
	  break;

	}
    }

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CANCEL);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);

  /* Mostro la finestra: */
  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      gchar *s_text, *p_text, *o_text, *l_text, *d_text;
      gboolean is_item;

      /* Prelevo il soggetto: */
      if (gtk_toggle_button_get_active
	  (GTK_TOGGLE_BUTTON (button_subject_this)) == TRUE)
	s_text = THIS_DOCUMENT;
      else
	s_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (subject));

      if (!s_text || !*s_text)
	{
	  dialog_msg (_("Invalid subject!"));
	  continue;
	}

      s_text = g_strdup (s_text);

      /* E' un predicato standard controllo: */
      if ((is_item =
	   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_item))) ==
	  FALSE)
	{
	  p_text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (property));
	  if (!p_text || !*p_text)
	    {
	      if (p_text)
		g_free (p_text);

	      g_free (s_text);

	      dialog_msg (_("Invalid predicate!"));
	      continue;
	    }
	}

      /* Se e' un item: */
      else
	{
	  gint id = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (item));

	  /* Controlliamo se i valori sono compatibili con i precedenti: */
	  if (check_item (prev_rdf, s_text, &id))
	    {
	      g_free (s_text);
	      continue;
	    }

	  /* Creo il vero predicato: */
	  p_text = g_strdup_printf ("%s%d", RDF_ITEM, id);
	}

      /* C.Oggetto: */
      o_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (object));
      if (!o_text || !*o_text)
	{
	  g_free (p_text);
	  g_free (s_text);

	  dialog_msg (_("Invalid object!"));
	  continue;
	}

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_resource)))
	{
	  if (rdf_blank (data->rdf, o_text) && dialog_ask (_
							   ("In the document this object is a blank node.\n"
							    "This triple can create some incongruences\n"
							    "You are sure?"))
	      != GTK_RESPONSE_OK)
	    {
	      g_free (p_text);
	      g_free (s_text);

	      continue;
	    }
	}

      else
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_blanknode)))
	{
	  if (rdf_resource (data->rdf, o_text) && dialog_ask (_
							      ("In the document this object is a resource.\n"
							       "This triple can create some incongruences\n"
							       "You are sure?"))
	      != GTK_RESPONSE_OK)
	    {
	      g_free (p_text);
	      g_free (s_text);

	      continue;
	    }
	}

      o_text = g_strdup (o_text);

      /* Controlliamo se e' una lista e se lo e' se i valori sono OK: */
      if (is_item == TRUE && check_list (s_text, GTK_WINDOW (dialog)))
	{
	  g_free (s_text);
	  g_free (p_text);
	  g_free (o_text);
	  continue;
	}

      /* Controlliamo i datatype: */
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_literal)))
	{
	  gboolean value;

	  if ((d_text =
	       gtk_combo_box_get_active_text (GTK_COMBO_BOX (datatype))))
	    {
	      value =
		datatype_check (gtk_combo_box_get_active
				(GTK_COMBO_BOX (datatype)), o_text, NULL);
	      g_free (d_text);

	      if (!value)
		{
		  g_free (s_text);
		  g_free (p_text);
		  g_free (o_text);

		  continue;
		}
	    }
	}

      /* A questo punto alloco ed inserisco: */
      rdf = g_malloc0 (sizeof (struct rdf_t));

      /* Le variabili gia' allocate: */
      rdf->subject = s_text;
      rdf->predicate = p_text;
      rdf->object = o_text;

      /* E' un letterale? */
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_literal)))
	{
	  rdf->object_type = RDF_OBJECT_LITERAL;

	  l_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (lang));
	  if (l_text && *l_text)
	    rdf->lang = g_strdup (l_text);

	  d_text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (datatype));
	  if (d_text && *d_text)
	    rdf->datatype = d_text;

	  else if (d_text)
	    g_free (d_text);
	}

      /* E' una risorsa? */
      else
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_resource)))
	rdf->object_type = RDF_OBJECT_RESOURCE;

      /* E' un blank node? */
      else
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_blanknode)))
	rdf->object_type = RDF_OBJECT_BLANK;

      /* Chiudiamo la finestra: */
      break;
    }

  gtk_widget_destroy (dialog);

  /* Libero le liste. Solo cosi' perche' i valori sono solo puntatori: */
  if (resources)
    g_list_free (resources);

  if (blanks)
    g_list_free (blanks);

  /* Ritorno: */
  return rdf;
}

static gboolean
event_box_popup_show (GtkWidget * w, GdkEventButton * event,
		      GtkWidget * label)
{
  GList *list = g_object_get_data (G_OBJECT (label), "data");

  return info_box_popup_show (w, event, list);
}

/* REMOVE ******************************************************************/

/* Questa funzione rimuove un elemento dall'interfaccia grafica: */
static void
edit_remove (struct editor_data_t *data, GtkWidget * widget)
{
  GList *list, *old;

  /* Cerco il parente: */
  widget = gtk_widget_get_parent (widget);

  /* Devo rimuovere la riga dopo ogni lista e quindi per ogni elemento
   * contenuto dal padre: */
  if ((list = old =
       gtk_container_get_children (GTK_CONTAINER
				   (gtk_widget_get_parent (widget)))))
    {
      while (list)
	{
	  /* Se e' questo, ed esiste un successivo, lo rimovo: */
	  if (list->data == widget && list->next)
	    {
	      gtk_widget_destroy (list->next->data);
	      break;
	    }

	  list = list->next;
	}

      g_list_free (old);
    }

  /* Rimuovo, infine l'elemento: */
  gtk_widget_destroy (widget);
}

/* CHECK ITEM ****************************************************************/

/* Questa funzione serve per ordinare in base all'ID una serie di elementi
 * di una lista: */
static gint
check_item_compare (struct rdf_t *a, struct rdf_t *b)
{
  gint len, id_a, id_b;

  len = strlen (RDF_ITEM);
  id_a = atoi (a->predicate + len);
  id_b = atoi (b->predicate + len);

  return (a > b);
}

/* Questa funzione attiva l'autonumerazione di un soggetto. */
static void
check_item_auto (gchar * subject, gint * id)
{
  struct editor_data_t *data = editor_get_data ();
  gint len = strlen (RDF_ITEM);
  GList *list, *new;
  struct rdf_t *rdf;
  gint tocheck;
  gint expect = 1;

  /* Il documento esiste? */
  if (!data)
    return;

  /* per ogni tripletta... */
  list = data->rdf;
  new = NULL;
  while (list)
    {
      rdf = (struct rdf_t *) list->data;

      /* Se coicide col soggetto ed e' una tipologia lista, aggiungilo 
       * nella lista temporea */
      if (!strcmp (rdf->subject, subject)
	  && !strncmp (rdf->predicate, RDF_ITEM, len))
	new = g_list_append (new, rdf);

      list = list->next;
    }

  /* Ordino la lista: */
  new = g_list_sort (new, (GCompareFunc) check_item_compare);

  /* Per ogni elemento della lista temporanea: */
  list = new;
  while (list)
    {
      rdf = list->data;

      /* Prelevo il valore: */
      tocheck = atoi (rdf->predicate + len);

      /* Se non coincide con expect che e' un int che autoincremento,
       * si e' saltato un valore. Uso questo: */
      if (tocheck != expect)
	{
	  *id = expect;
	  break;
	}

      expect++;
      list = list->next;
    }

  /* Se non ho trovato niente, uso l'ultimo valore accettabile: */
  if (!list)
    *id = expect;

  /* Libero la lista */
  g_list_free (new);
}

/* Questa funzione setta in automatico il valore dentro allo spin button */
static void
check_item_set_auto (GtkWidget * widget, GtkWidget * spin)
{
  GtkWidget *subject, *subject_this;
  gchar *text;
  gint id;

  /* Prelevo il soggetto e se non c'e' esco: */
  subject_this = g_object_get_data (G_OBJECT (widget), "subject_this");
  subject = g_object_get_data (G_OBJECT (widget), "subject");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (subject_this)) == TRUE)
    text = THIS_DOCUMENT;
  else
    text = (gchar *) gtk_entry_get_text (GTK_ENTRY (subject));

  if (!text || !*text)
    {
      dialog_msg (_("Set a subject!"));
      return;
    }

  /* Prelevo un ID con la funzione check_item_auto e lo inserisco nello
   * spin button */
  check_item_auto (text, &id);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), id);
}

/* Questa funzione controlla se l'item e' corretto in base ai precedenti: */
static gboolean
check_item (struct rdf_t *prev, gchar * subject, gint * id)
{
  gint todo = 0;

  /* Se l'ID non esiste... gia' c'e' un errore: */
  if (*id < 1)
    todo = -1;

  else
    {
      struct editor_data_t *data = editor_get_data ();
      gint len = strlen (RDF_ITEM);
      GList *list, *new;
      struct rdf_t *rdf;
      gint tocheck;

      /* Per ogni tripletta: */
      list = data->rdf;
      new = NULL;
      while (list)
	{
	  rdf = (struct rdf_t *) list->data;

	  /* Se e' di tipologia lista e coincide con questo soggetto,
	   * lo inserisco in una lista temporanea */
	  if (!strcmp (rdf->subject, subject)
	      && !strncmp (rdf->predicate, RDF_ITEM, len))
	    new = g_list_append (new, rdf);
	  list = list->next;
	}

      /* Ordino la lista */
      new = g_list_sort (new, (GCompareFunc) check_item_compare);

      /* Per ogni elemento di questa lista temporanea */
      list = new;
      while (list)
	{
	  rdf = list->data;
	  tocheck = atoi (rdf->predicate + len);

	  /* Se questo id e' gia' usato e non e' usato da se stesso
	   * (vedi prevedente): */
	  if (tocheck == *id && (!prev || prev != rdf))
	    {
	      todo = 1;
	      break;
	    }

	  list = list->next;
	}

      g_list_free (new);
    }

  /* Se devo comunicare l'errore del ID: */
  if (todo)
    {
      gchar s[1024];

      if (todo > 0)
	g_snprintf (s, sizeof (s),
		    _("This index is already used for this subject!\n"
		      "Active the auto numeration?"));
      else
	g_snprintf (s, sizeof (s),
		    _("The index can be 0!\n" "Active the auto numeration?"));

      /* Mostro la domanda e in base alla risposta: */
      switch (dialog_ask_with_cancel (s))
	{
	case GTK_RESPONSE_NO:
	  return 0;

	case GTK_RESPONSE_OK:
	  /* Qui uso l'auto numerazione */
	  check_item_auto (subject, id);
	  return 0;

	default:
	  return 1;
	}
    }

  /* Di default, tutto bene: */
  return 0;
}

/* Questa funzione fa il controllo se un soggetto e' un ALT, SEQ o BAG: */
static gboolean
check_list (gchar * str, GtkWindow * parent)
{
  struct editor_data_t *data = editor_get_data ();
  GList *list;
  struct rdf_t *rdf;
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  gchar s[1024];
  gboolean ret;

  /* Per ogni tripletta di questo documento: */
  list = data->rdf;
  while (list)
    {
      rdf = (struct rdf_t *) list->data;

      if (!strcmp (rdf->subject, str) && !strcmp (rdf->predicate, RDF_TYPE)
	  && rdf->object_type == RDF_OBJECT_RESOURCE)
	{
	  /* Se lo trovo, ok: */
	  if (!strcmp (rdf->object, RDF_ALT) || !strcmp (rdf->object, RDF_BAG)
	      || !strcmp (rdf->object, RDF_SEQ))
	    return 0;
	}
      list = list->next;
    }

  /* Pongo la domanda di controllo: */
  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Question..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label =
    gtk_label_new
    (_
     ("The subject of this triple is not a bag, a sequence or a alternative!\n"
      "Do you want create it?"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  button = gtk_button_new_with_label (_("Alternative"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 1);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_with_label (_("Sequence"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 2);

  button = gtk_button_new_with_label (_("Bag"));
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 3);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 4);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 5);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);

  rdf = NULL;
  /* In base alla risposta del dialog creo o no una nuova tripletta: */
  switch (gtk_dialog_run (GTK_DIALOG (dialog)))
    {
    case 1:			/* ALT */
      rdf = g_malloc0 (sizeof (struct rdf_t));

      rdf->subject = g_strdup (str);
      rdf->predicate = g_strdup (RDF_TYPE);
      rdf->object = g_strdup (RDF_ALT);
      rdf->object_type = RDF_OBJECT_RESOURCE;
      ret = 0;
      break;

    case 2:			/* SEQ */
      rdf = g_malloc0 (sizeof (struct rdf_t));

      rdf->subject = g_strdup (str);
      rdf->predicate = g_strdup (RDF_TYPE);
      rdf->object = g_strdup (RDF_SEQ);
      rdf->object_type = RDF_OBJECT_RESOURCE;
      ret = 0;
      break;

    case 3:			/* BAG */
      rdf = g_malloc0 (sizeof (struct rdf_t));

      rdf->subject = g_strdup (str);
      rdf->predicate = g_strdup (RDF_TYPE);
      rdf->object = g_strdup (RDF_BAG);
      rdf->object_type = RDF_OBJECT_RESOURCE;
      ret = 0;
      break;

    case 4:			/* Niente ma bene */
      ret = 0;
      break;

    default:			/* Cancel o altro */
      ret = 1;
      break;
    }

  gtk_widget_destroy (dialog);

  /* Se ho un nuovo elemento, lo appendo alla lista */
  if (rdf)
    data->rdf = g_list_append (data->rdf, rdf);

  return ret;
}

/* Questa funzione merga al documento attuale un file RDF: */
void
editor_merge_file (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  gchar *file;
  GList *list;
  gchar s[1024];
  gchar *uri;
  GString *error = NULL;

  if (!data)
    return;

  /* Setto lo statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Import a RDF file..."));

  if (!
      (file =
       file_chooser (_("Import a RDF file..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_OPEN, NULL)))
    {
      statusbar_set (_("Import a RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Creo da file ad uri e attivo le librdf: */
  uri = g_filename_to_uri (file, NULL, NULL);

  /* Mergio: */
  if (!uri || !(list = merge (data->rdf, uri, NULL, &error)))
    {
      gchar *str;

      if (error)
	str = g_string_free (error, FALSE);
      else
	str = NULL;

      g_snprintf (s, sizeof (s), _("Error opening URI %s!"),
		  uri ? uri : file);

      dialog_msg_with_error (s, str);

      if (str)
	g_free (str);

      if (uri)
	g_free (uri);

      g_free (file);
      return;
    }

  g_free (uri);

  data->rdf = list;

  /* Libero i namespace e l'altra memoria: */
  g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
  g_list_free (data->namespace);

  namespace_parse (data->rdf, &data->namespace);

  g_free (file);

  statusbar_set (_("RDF file merged!"));
  editor_statusbar_lock = WAIT;
  editor_refresh (data);
}

/* Questa funzione merga al documento attuale un uri RDF: */
void
editor_merge_uri (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  gchar *uri;
  gchar s[1024];
  GList *list;
  GString *error = NULL;
  struct download_t *download;

  if (!data)
    return;

  /* Setto lo statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Import a RDF URI..."));

  if (!(uri = uri_chooser (_("Import a RDF URI..."), &download)))
    {
      statusbar_set (_("Import a RDF URI: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Mergio: */
  if (!(list = merge (data->rdf, uri, download, &error)))
    {
      gchar *str;

      if (error)
	str = g_string_free (error, FALSE);
      else
	str = NULL;

      g_snprintf (s, sizeof (s), _("Error opening the RDF URI %s!"), uri);
      dialog_msg_with_error (s, str);

      if (str)
	g_free (str);

      g_free (uri);

      if (download)
	download_free (download);
      return;
    }

  if (download)
    download_free (download);

  data->rdf = list;

  /* Libero i namespace e l'altra memoria: */
  g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
  g_list_free (data->namespace);

  namespace_parse (data->rdf, &data->namespace);

  g_free (uri);

  statusbar_set (_("RDF URI merged!"));
  editor_statusbar_lock = WAIT;
  editor_refresh (data);
}

/* EOF */

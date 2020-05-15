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

/* INFO BOX *****************************************************************/

/* Questa funzione viene eseguita quando si clicca su label o comment: */
gboolean
info_box_popup_show (GtkWidget * w, GdkEventButton * event, GList * list)
{
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *l;
  gint x, y;
  gchar *s;
  gboolean bold;

  /* Se c'era una finestra prima... la rimuovo: */
  if ((window = g_object_get_data (G_OBJECT (w), "window")))
    gtk_widget_destroy (window);

  bold = g_object_get_data (G_OBJECT (w), "bold") ? TRUE : FALSE;

  /* Creo la finestra con i suggerimenti: */
  window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_title (GTK_WINDOW (window), PACKAGE);
  gtk_window_set_type_hint (GTK_WINDOW (window),
			    GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (window), frame);

  vbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  while (list)
    {
      struct info_box_t *d = list->data;

      if (bold == TRUE)
	{
	  gchar *value = markup (d->value);

	  if (d->lang)
	    {
	      gchar *lang = markup (d->lang);
	      s = g_strdup_printf ("<b>(%s) %s</b>", lang, value);
	      g_free (lang);
	    }
	  else
	    s = g_strdup_printf ("<b>%s</b>", value);
	  g_free (value);
	}
      else
	{
	  if (d->lang)
	    s = g_strdup_printf ("(%s) %s", d->lang, d->value);
	  else
	    s = g_strdup_printf ("%s", d->value);
	}

      l = gtk_label_new (s);

      if (bold == TRUE)
	gtk_label_set_use_markup (GTK_LABEL (l), TRUE);

      gtk_label_set_line_wrap (GTK_LABEL (l), TRUE);
      g_free (s);

      gtk_box_pack_start (GTK_BOX (vbox), l, FALSE, FALSE, 5);

      list = list->next;
    }

  /* La realizzo ocn le giuste dimensioni: */
  gtk_widget_show_all (frame);
  gtk_window_set_default_size (GTK_WINDOW (window), w->allocation.width, -1);
  gtk_widget_realize (window);

  /* Calcolo il centro del padre: */
  gdk_window_get_origin (w->window, &x, &y);
  x += (w->allocation.width / 2);
  y += (w->allocation.height / 2);

  /* Muovo la finestra nell'identico centro: */
  gtk_window_move (GTK_WINDOW (window), x - (window->allocation.width / 2),
		   y - (window->allocation.height / 2));

  gtk_widget_show_all (window);

  /* Salvo la finestra dentro al label: */
  g_object_set_data (G_OBJECT (w), "window", window);

  return TRUE;
}

/* Quando si molla il pulsante distruggo la finestra: */
gboolean
info_box_popup_hide (GtkWidget * w, GdkEventButton * event, gpointer dummy)
{
  GtkWidget *window = g_object_get_data (G_OBJECT (w), "window");

  if (window)
    gtk_widget_destroy (window);

  g_object_steal_data (G_OBJECT (w), "window");

  return TRUE;
}

/* Quando si entra nel box di suggerimento: */
gboolean
info_box_enter (GtkWidget * box, GdkEventCrossing * event, gpointer dummy)
{
  static GdkCursor *cursor = NULL;

  if (!cursor)
    cursor = gdk_cursor_new (GDK_HAND2);

  gdk_window_set_cursor (box->window, cursor);
  return TRUE;
}

/* Quando si esce dal box di suggerimento: */
gboolean
info_box_leave (GtkWidget * box, GdkEventCrossing * event, gpointer dummy)
{
  gdk_window_set_cursor (box->window, NULL);
  return TRUE;
}

void
info_box_free (struct info_box_t *i)
{
  if (!i)
    return;

  if (i->lang)
    g_free (i->lang);

  if (i->value)
    g_free (i->value);

  g_free (i);
}

/* INFO **********************************************************************/
static void info_run (gchar * uri, gboolean);
static void info_run_rec (gchar * uri, gboolean, GList **, GList **);

/* Questa funzione lancia info_run su un item selezionato: */
void
info_selected (void)
{
  struct editor_data_t *data = editor_get_data ();

  if (!data || !data->node_select)
    {
      dialog_msg (_("No selected item!"));
      return;
    }

  /* Se e' come l'URI di questo documento: */
  if (data->uri && !strcmp (data->node_select, data->uri))
    info_run (THIS_DOCUMENT, FALSE);
  else
    info_run (data->node_select, rdf_blank (data->rdf, data->node_select));
}

/* Questa funzione lancia info_run su un item selezionato: */
void
info_this (void)
{
  struct editor_data_t *data = editor_get_data ();

  if (!data || !data->rdf)
    {
      dialog_msg (_("This RDF document is empty!"));
      return;
    }

  info_run (THIS_DOCUMENT, FALSE);
}

/* Questa funzione lancia info_run con un URI inserito dall'utente: */
void
info_uri (void)
{
  gchar *uri;

  if ((uri = uri_chooser (_("Info about an URI"), NULL)))
    {
      info_run (uri, FALSE);
      g_free (uri);
    }
}

static gboolean
info_dialog_rdfs (GList * list)
{
  GtkWidget *window, *scrolledwindow, *treeview;
  GtkWidget *button, *frame, *label;
  GtkListStore *model;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  gchar s[1024];
  gboolean ret;

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Import other RDFS..."));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 400, 200);

  label =
    gtk_label_new (_
		   ("Some property/class is unknown. Do you want import them?"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), label, FALSE,
		      FALSE, 0);

  frame = gtk_frame_new (_("Ignored properties"));
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
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Property"), renderer, "text",
					      0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button,
				GTK_RESPONSE_CANCEL);
  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default (button, TRUE);

  while (list)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, list->data, -1);

      list = list->next;
    }

  gtk_widget_show_all (window);
  ret = gtk_dialog_run (GTK_DIALOG (window));
  g_object_unref (model);
  gtk_widget_destroy (window);
  return ret;
}

/* Mostra informazioni su un elemento passato come argomento: */
static void
info_run (gchar * uri, gboolean blank)
{
  GList *list = NULL, *ignore = NULL;
  struct editor_data_t *data;

  info_run_rec (uri, blank, &list, &ignore);

  /* Ci sono item ignorati: */
  if (ignore)
    {

      /* Chiedo di importarli: */
      if (info_dialog_rdfs (ignore) == GTK_RESPONSE_OK)
	{
	  GList *l, *ll, *ns = NULL;

	  for (l = ignore; l; l = l->next)
	    {
	      gchar *tmp = g_strdup (l->data);
	      gint len = strlen (tmp);

	      while (len--)
		if (tmp[len] == '/' || tmp[len] == '#')
		  break;

	      tmp[len + 1] = 0;

	      for (ll = ns; ll; ll = ll->next)
		if (!strcmp (ll->data, tmp))
		  break;

	      if (ll)
		g_free (tmp);
	      else
		ns = g_list_append (ns, tmp);
	    }

	  /* Chiedo per l'import: */
	  for (l = ns; l; l = l->next)
	    {
	      rdfs_add (l->data, NULL);

	      g_free (l->data);
	    }

	  /* Libero memoria: */
	  g_list_free (ns);

	  /* Analizzo nuovamente gli uri ignorati: */
	  for (l = ignore; l; l = l->next)
	    info_run_rec (l->data, FALSE, &list, NULL);
	}

      /* Libero memoria: */
      g_list_free (ignore);
    }

  /* Creo una struttura nuova e la aggiungo ai documenti aperti: */
  if (list)
    {
      data = g_malloc0 (sizeof (struct editor_data_t));

      data->rdf = list;
      namespace_parse (list, &data->namespace);
      editor_new_maker (data);
    }

  else
    dialog_msg (_("No info for this URI"));
}

/* Define capace di effettuare un check su un URI e una PROPRIETA': */
#define INFO_CHECK( x , y ) \
      /* Se e' il soggetto giusto: */ \
      if (!strcmp (uri, rdf->subject)) \
	{ \
	  if (!strcmp (rdf->predicate, x) \
	      && (rdf->object_type == RDF_OBJECT_RESOURCE || \
	      (rdf->object_type == RDF_OBJECT_BLANK))) \
	    { \
	      GList *l; \
              \
              /* Se non c'e' gia' nella lista. */ \
	      for (l = *global; l; l = l->next) \
		{ \
		  struct rdf_t *info = l->data; \
		  if (!strcmp (info->subject, uri) \
		      && !strcmp (info->predicate, x) \
		      && !strcmp (info->object, rdf->object)) \
		    break; \
		} \
              \
	      if (!l) \
		{ \
                  /* Inserisco: */ \
		  *global = g_list_append (*global, rdf_copy(rdf)); \
		  ok= TRUE; \
                  \
                  /* Se non c'e' gia' l'oggetto nella lista: */ \
		  for (l = *global; l; l = l->next) \
		    if (!strcmp \
			(((struct rdf_t *) l->data)->subject, rdf->object)) \
		      break; \
                  \
		  if (!l && ignore) \
		    info_run_rec (rdf->object, rdf_blank(y, rdf->object), \
		                  global, ignore); \
		} \
	    } \
	}

/* Funzione ricorsiva per la raccolta di informazioni: */
static void
info_run_rec (gchar * uri, gboolean blank, GList ** global, GList ** ignore)
{
  struct editor_data_t *data = editor_get_data ();
  GList *list, *ns;
  gboolean ok = FALSE;

  /* Ricerco informazioni nel documento attuale: */
  for (list = data->rdf; list; list = list->next)
    {
      struct rdf_t *rdf = list->data;

      /* Controllo diverse proprieta': */
      INFO_CHECK (RDF_TYPE, data->rdf);
      INFO_CHECK (RDF_SUBCLASSOF, data->rdf);
      INFO_CHECK (RDF_SUBPROPERTYOF, data->rdf);
    }

  /* Se non devo ricercare il THIS_DOCUMENT: */
  if (blank == FALSE && strcmp (uri, THIS_DOCUMENT))
    {
      /* Ricerco info in ogni documento namespace conosciuto: */
      for (ns = rdfs_list; ns; ns = ns->next)
	{
	  struct rdfs_t *rdfs = ns->data;

	  /* Per ogni tripletta di questo namespace: */
	  for (list = rdfs->data; list; list = list->next)
	    {
	      struct rdf_t *rdf = list->data;

	      /* Controllo diverse proprieta': */
	      INFO_CHECK (RDF_TYPE, rdfs->data);
	      INFO_CHECK (RDF_SUBCLASSOF, rdfs->data);
	      INFO_CHECK (RDF_SUBPROPERTYOF, rdfs->data);
	    }
	}
    }

  /* Se non ho trovato informazioni: */
  if (ok == FALSE && ignore && strcmp (THIS_DOCUMENT, uri))
    {
      GList *l;

      for (l = *ignore; l; l = l->next)
	if (!strcmp ((gchar *) l->data, uri))
	  break;

      if (!l)
	*ignore = g_list_append (*ignore, uri);
    }
}

/* EOF */

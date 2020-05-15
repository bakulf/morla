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

#define HISTORY_MAX 10
#define BACK_FORWARD_MAX 10

struct navigator_history_t
{
  GList *rdf;
  gchar *uri;
  gdouble adj;
};

struct navigator_t
{
  GList *back;
  GList *forward;

  gchar *uri;
  GList *rdf;

  GtkWidget *sw;
  GtkWidget *label;
  GtkWidget *widget;
  GtkWidget *notebook;
};

static void navigator (GList * rdf);
static void navigator_back (GtkWidget * button, GtkWidget * notebook);
static void navigator_forward (GtkWidget * button, GtkWidget * notebook);
static void navigator_quit (GtkWidget * button, GtkWidget * notebook);
static void navigator_close (GtkWidget * button, GtkWidget * notebook);
static void navigator_switch_page (GtkWidget * widget, gpointer * page,
				   guint page_num);
static void navigator_destroy (GtkWidget * notebook);
static void navigator_free (struct navigator_t *navigator);
static void navigator_refresh (struct navigator_t *, gdouble adj);
static void notebook_position (GtkMenu * menu, gint * xp, gint * yp,
			       gboolean * p, GtkWidget *);
static gint navigator_open (GtkWidget *, GdkEventButton *,
			    struct navigator_t *);
static GtkWidget *notebook_menu (GtkWidget * notebook);
static void navigator_browser (GtkWidget * button, GtkWidget * notebook);
static void navigator_show (GtkWidget * button, GtkWidget * notebook);
static void navigator_show_tab (GtkWidget * button, GtkWidget * notebook);
static void navigator_import (GtkWidget * button, GtkWidget * notebook);
static void navigator_import_new (GtkWidget * button, GtkWidget * notebook);
static void navigator_clicked (GtkWidget * button,
			       struct navigator_t *navigator);
static void navigator_start (struct navigator_t *navigator, gchar * value,
			     GList * rdf);
static void navigator_history_free (struct navigator_history_t *n);
static gboolean navigator_uri_match (GtkEntryCompletion * completion,
				     const gchar * key, GtkTreeIter * iter,
				     GtkWidget * notebook);
static void navigator_pages (GtkWidget * notebook, GList * rdf);
static void navigator_go (GtkWidget * button, GtkWidget * notebook);
static void navigator_try (gchar * uri, gpointer notebook);
static void navigator_info (GtkWidget * widget, gchar * predicate);

/* Questa funzione esegue il navigatore sul documento in primo piano */
void
navigator_run (void)
{
  struct editor_data_t *data = editor_get_data ();
  GList *ret, *tmp;
  struct rdf_t *rdf;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This RDF document is empty!"));
      return;
    }

  tmp = data->rdf;
  ret = NULL;

  while (tmp)
    {
      rdf = rdf_copy (tmp->data);
      if (!strcmp (rdf->subject, THIS_DOCUMENT))
	{
	  g_free (rdf->subject);

	  rdf->subject =
	    g_strdup (data->uri ? data->uri : _("This Document"));
	}

      ret = g_list_append (ret, rdf);
      tmp = tmp->next;
    }

  /* Lancia navigator */
  navigator (ret);

  g_list_foreach (ret, (GFunc) rdf_free, NULL);
  g_list_free (ret);
}

/* Questa funzione e' il navigatore: */
static void
navigator (GList * rdf)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *sep;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *notebook;
  gchar s[1024];
  GtkEntryCompletion *completion;
  gint x, y;

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION, _("Navigator"));

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), s);

  y = gdk_screen_height () * 2 / 3;
  if (y < 480)
    y = 480;

  x = gdk_screen_width () * 2 / 3;
  if (x < 640)
    x = 640;

  gtk_window_resize (GTK_WINDOW (window), x, y);

  notebook = gtk_notebook_new ();
  g_object_set_data (G_OBJECT (notebook), "menu", notebook_menu (notebook));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  hbox = gtk_hbox_new (0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-go-back");
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_object_set_data (G_OBJECT (notebook), "back_widget", button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (navigator_back), notebook);

  button = gtk_button_new_from_stock ("gtk-go-forward");
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_object_set_data (G_OBJECT (notebook), "forward_widget", button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (navigator_forward), notebook);

  sep = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), sep, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  g_signal_connect (entry, "activate", G_CALLBACK (navigator_go), notebook);
  g_object_set_data (G_OBJECT (notebook), "entry", entry);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock ("gtk-execute");
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (navigator_go), notebook);

  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_match_func (completion,
				       (GtkEntryCompletionMatchFunc)
				       navigator_uri_match, notebook, NULL);

  gtk_entry_set_completion (GTK_ENTRY (entry), completion);

  gtk_entry_completion_set_text_column (completion, 0);
  g_object_unref (completion);

  sep = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), sep, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (navigator_close), notebook);

  sep = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (hbox), sep, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-quit");
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (navigator_quit), notebook);

  gtk_widget_show (notebook);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  g_signal_connect ((gpointer) notebook, "switch_page",
		    G_CALLBACK (navigator_switch_page), NULL);
  g_signal_connect ((gpointer) notebook, "destroy",
		    G_CALLBACK (navigator_destroy), NULL);

  /* Questa funzione crea le pagine data la lista di RDF */
  navigator_pages (notebook, rdf);

  gtk_widget_show_all (window);
}

/* Questa funzione viene attivata quando avviene lo switch tra una page
 * e l'altra del notebook: */
static void
navigator_switch_page (GtkWidget * widget, gpointer * page, guint page_num)
{
  GtkWidget *back_widget =
    g_object_get_data (G_OBJECT (widget), "back_widget");
  GtkWidget *forward_widget =
    g_object_get_data (G_OBJECT (widget), "forward_widget");
  GtkWidget *entry = g_object_get_data (G_OBJECT (widget), "entry");
  GList *data = g_object_get_data (G_OBJECT (widget), "data");
  struct navigator_t *navigator;

  if (!data)
    return;

  /* Cerco il navigatore in primo piano: */
  data = g_list_nth (data, page_num);
  navigator = data->data;

  /* Setto il testo nell'entry di navigazione: */
  gtk_entry_set_text (GTK_ENTRY (entry), navigator->uri);

  /* Attivo i pulsanti back e forward */
  gtk_widget_set_sensitive (back_widget, navigator->back ? TRUE : FALSE);
  gtk_widget_set_sensitive (forward_widget,
			    navigator->forward ? TRUE : FALSE);
}

/* Funzione che chiude il navigatore: */
static void
navigator_quit (GtkWidget * button, GtkWidget * notebook)
{
  gtk_widget_destroy (gtk_widget_get_toplevel (button));
}

/* Questa funzione viene lanciata quando avviene la distruzione del
 * navigatore: */
static void
navigator_destroy (GtkWidget * notebook)
{
  GList *data;

  /* Se trovo la lista dei navigatori aperti, la libero: */
  if ((data = g_object_get_data (G_OBJECT (notebook), "data")))
    {
      g_list_foreach (data, (GFunc) navigator_free, NULL);
      g_list_free (data);
    }

  /* Rimuovo la history del navigatore: */
  if ((data = g_object_get_data (G_OBJECT (notebook), "entry_history")))
    {
      g_list_foreach (data, (GFunc) g_free, NULL);
      g_list_free (data);
    }
}

/* Libero una struttura di navigazione: */
static void
navigator_free (struct navigator_t *navigator)
{
  if (!navigator)
    return;

  /* Tutti i back: */
  if (navigator->back)
    {
      g_list_foreach (navigator->back, (GFunc) navigator_history_free, NULL);
      g_list_free (navigator->back);
    }

  /* Tutti i forward: */
  if (navigator->forward)
    {
      g_list_foreach (navigator->forward, (GFunc) navigator_history_free,
		      NULL);
      g_list_free (navigator->forward);
    }

  /* Tutti gli rdf: */
  if (navigator->rdf)
    {
      g_list_foreach (navigator->rdf, (GFunc) rdf_free, NULL);
      g_list_free (navigator->rdf);
    }

  /* L'uri: */
  if (navigator->uri)
    g_free (navigator->uri);

  free (navigator);
}

/* Rimozione di una history: */
static void
navigator_history_free (struct navigator_history_t *n)
{
  if (!n)
    return;

  /* Se c'e' rdf: */
  if (n->rdf)
    {
      g_list_foreach (n->rdf, (GFunc) rdf_free, NULL);
      g_list_free (n->rdf);
    }

  if (n->uri)
    g_free (n->uri);

  g_free (n);
}

/* Quando si clicca back: */
static void
navigator_back (GtkWidget * button, GtkWidget * notebook)
{
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  GtkWidget *entry = g_object_get_data (G_OBJECT (notebook), "entry");
  GList *list;
  struct navigator_t *navigator;
  struct navigator_history_t *history, *new;

  /* Cerco la lista dei navigatori e il navigatore in primo piano: */
  list = g_object_get_data (G_OBJECT (notebook), "data");
  list = g_list_nth (list, page);

  if (!list)
    return;

  navigator = list->data;

  /* Se non c'e' history back */
  if (!navigator->back)
    return;

  /* Rimuovo l'elemento history back: */
  history = navigator->back->data;
  navigator->back = g_list_remove (navigator->back, history);

  /* Creo l'elemento per il forward: */
  new =
    (struct navigator_history_t *)
    g_malloc0 (sizeof (struct navigator_history_t));

  /* Se c'e' un elenco diverso di rdf nell'history, allora lo sostituisco
   * con quelli presenti nel documento attuale: */
  if (history->rdf)
    {
      new->rdf = navigator->rdf;
      navigator->rdf = history->rdf;
    }

  /* Cambio uri: */
  new->uri = navigator->uri;
  navigator->uri = history->uri;

  /* Setto nella entry del navigatori l'uri: */
  gtk_entry_set_text (GTK_ENTRY (entry), navigator->uri);
  navigator->forward = g_list_prepend (navigator->forward, new);

  /* Se la history e' troppo alta devo rimuovere un elemento nel forward: */
  if (g_list_length (navigator->forward) > BACK_FORWARD_MAX)
    {
      list = g_list_last (navigator->forward);
      navigator_history_free ((struct navigator_history_t *) list->data);
      navigator->forward = g_list_remove (navigator->forward, list->data);
    }

  /* Aggiorno il navigatore: */
  navigator_refresh (navigator, history->adj);
  g_free (history);
}

/* Uguale ma invertito, la funzione di forward: */
static void
navigator_forward (GtkWidget * button, GtkWidget * notebook)
{
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  GtkWidget *entry = g_object_get_data (G_OBJECT (notebook), "entry");
  GList *list;
  struct navigator_t *navigator;
  struct navigator_history_t *history, *new;

  list = g_object_get_data (G_OBJECT (notebook), "data");
  list = g_list_nth (list, page);

  if (!list)
    return;

  navigator = list->data;

  if (!navigator->forward)
    return;

  history = navigator->forward->data;
  navigator->forward = g_list_remove (navigator->forward, history);

  new =
    (struct navigator_history_t *)
    g_malloc0 (sizeof (struct navigator_history_t));

  if (history->rdf)
    {
      new->rdf = navigator->rdf;
      navigator->rdf = history->rdf;
    }

  new->uri = navigator->uri;
  navigator->uri = history->uri;

  gtk_entry_set_text (GTK_ENTRY (entry), navigator->uri);
  navigator->back = g_list_prepend (navigator->back, new);

  if (g_list_length (navigator->back) > BACK_FORWARD_MAX)
    {
      list = g_list_last (navigator->back);
      navigator_history_free ((struct navigator_history_t *) list->data);
      navigator->back = g_list_remove (navigator->back, list->data);
    }

  navigator_refresh (navigator, history->adj);
  g_free (history);
}

/* Quando si chiede di chiudere un navigatore: */
static void
navigator_close (GtkWidget * button, GtkWidget * notebook)
{
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  GList *list, *item;

  /* Rimozione della pagina: */
  gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), page);

  /* Libero la memoria: */
  list = g_object_get_data (G_OBJECT (notebook), "data");
  item = g_list_nth (list, page);
  navigator_free (item->data);
  list = g_list_remove (list, item->data);

  g_object_set_data (G_OBJECT (notebook), "data", list);

  /* Se non c'e' piu' nessun navigatore, distruggo la finestra: */
  if (!list)
    gtk_widget_destroy (gtk_widget_get_toplevel (button));
}

/* Funzione per ordinare in base alla proprieta' una lista di triplette: */
static gint
navigator_order (struct rdf_t *a, struct rdf_t *b)
{
  if (a && b)
    return strcmp (a->predicate, b->predicate);
  return 0;
}

/* Refresh del navigatore: */
static void
navigator_refresh (struct navigator_t *navigator, gdouble adj_value)
{
  GtkWidget *back_widget, *forward_widget, *entry;
  GtkWidget *label, *button, *image;
  GtkAdjustment *adj;
  GList *list, *ret;
  struct rdf_t *rdf;

  /* Rimuovo tutto cio' che c'e' visualizzato ora: */
  gtk_container_foreach (GTK_CONTAINER (navigator->widget),
			 (GtkCallback) gtk_widget_destroy, NULL);

  /* Setto nuovamente la label: */
  gtk_label_set_text (GTK_LABEL (navigator->label), navigator->uri);

  /* Per ogni tripletta: */
  list = navigator->rdf;
  ret = NULL;
  while (list)
    {
      rdf = list->data;

      /* Se e' del giusto soggetto, la inserisco in una lista temporanea */
      if (!strcmp (rdf->subject, navigator->uri))
	ret = g_list_append (ret, rdf);

      list = list->next;
    }

  /* Se questa esiste: */
  if (ret)
    {
      gint row = 0;

      ret = g_list_sort (ret, (GCompareFunc) navigator_order);

      /* Per ognuna di questi elementi, creo una voce in piu' nella finestra: */
      list = ret;
      while (list)
	{
	  rdf = list->data;

	  button = gtk_button_new ();
	  gtk_table_attach (GTK_TABLE (navigator->widget), button, 0, 1, row,
			    row + 1, GTK_FILL, 0, 3, 3);

	  g_signal_connect ((gpointer) button, "clicked",
			    G_CALLBACK (navigator_info), rdf->predicate);

	  image = gtk_image_new_from_stock (
#ifdef GTK_STOCK_INFO
					     "gtk-info",
#else
					     "gtk-dialog-info",
#endif
					     GTK_ICON_SIZE_BUTTON);

	  gtk_container_add (GTK_CONTAINER (button), image);

	  label = gtk_label_new (rdf->predicate);
	  gtk_label_set_selectable (GTK_LABEL (label), TRUE);

	  gtk_table_attach (GTK_TABLE (navigator->widget), label, 1, 2, row,
			    row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 3, 3);

	  /* Se l'oggetto e' letterale: */
	  if (rdf->object_type == RDF_OBJECT_LITERAL)
	    {
	      gchar s[1024];

	      if (rdf->lang && rdf->datatype)
		g_snprintf (s, sizeof (s), "[%s] <%s> %s", rdf->lang,
			    rdf->datatype, rdf->object);

	      else if (rdf->datatype)
		g_snprintf (s, sizeof (s), "<%s> %s", rdf->datatype,
			    rdf->object);

	      else if (rdf->lang)
		g_snprintf (s, sizeof (s), "[%s] %s", rdf->lang, rdf->object);

	      else
		g_snprintf (s, sizeof (s), "%s", rdf->object);

	      label = gtk_label_new (s);
	      gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

	      gtk_table_attach (GTK_TABLE (navigator->widget), label, 2, 3,
				row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL,
				3, 3);
	    }

	  /* Se invece e' una risorsa devo fare il sistema a menu: */
	  else
	    {
	      GtkWidget *box;
	      GtkWidget *button;
	      GtkWidget *label;
	      GtkWidget *arrow;

	      box = gtk_hbox_new (0, 0);

	      button = gtk_button_new ();

	      label = gtk_label_new (rdf->object);
	      gtk_container_add (GTK_CONTAINER (button), label);
	      gtk_widget_set_size_request (label, 200, -1);
	      gtk_label_set_ellipsize (GTK_LABEL (label),
				       PANGO_ELLIPSIZE_END);

	      g_object_set_data (G_OBJECT (button), "value", rdf->object);
	      gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);

	      g_signal_connect ((gpointer) button, "clicked",
				G_CALLBACK (navigator_clicked), navigator);

	      button = gtk_button_new ();
	      g_object_set_data (G_OBJECT (button), "value", rdf->object);
	      gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	      g_signal_connect ((gpointer) button, "button_press_event",
				G_CALLBACK (navigator_open), navigator);

	      arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	      gtk_container_add (GTK_CONTAINER (button), arrow);

	      gtk_table_attach (GTK_TABLE (navigator->widget), box, 2, 3,
				row, row + 1, GTK_EXPAND | GTK_FILL,
				GTK_FILL, 3, 3);
	    }

	  row++;

	  /* Se c'e' un successivo creo una linea vuota: */
	  if (list->next)
	    {
	      gtk_table_attach (GTK_TABLE (navigator->widget),
				gtk_hseparator_new (), 0, 3, row,
				row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 3,
				3);
	      row++;
	    }

	  list = list->next;
	}

      g_list_free (ret);
    }

  /* Se non c'e' elemento legato al soggettom scrivo no data: */
  else
    {
      gtk_table_attach (GTK_TABLE (navigator->widget),
			gtk_label_new (_("No data found!")), 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 3, 3);
    }

  /* Setto lo scroll come l'avevamo lasciato (vale solo per il back e forward */
  adj =
    gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (navigator->sw));
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj), adj_value);

  /* Aggiorno back, forward e la barra di navigazione */
  back_widget =
    g_object_get_data (G_OBJECT (navigator->notebook), "back_widget");
  forward_widget =
    g_object_get_data (G_OBJECT (navigator->notebook), "forward_widget");
  entry = g_object_get_data (G_OBJECT (navigator->notebook), "entry");

  gtk_entry_set_text (GTK_ENTRY (entry), navigator->uri);
  gtk_widget_set_sensitive (back_widget, navigator->back ? TRUE : FALSE);
  gtk_widget_set_sensitive (forward_widget,
			    navigator->forward ? TRUE : FALSE);

  /* Cerco nell'history questo uri: */
  list = ret =
    g_object_get_data (G_OBJECT (navigator->notebook), "entry_history");
  while (list)
    {
      if (!strcmp (list->data, navigator->uri))
	break;
      list = list->next;
    }

  /* Se non c'e' lo inserisco: */
  if (!list)
    {
      GtkListStore *store;
      GtkTreeIter iter;
      GtkEntryCompletion *completion;

      store = gtk_list_store_new (1, G_TYPE_STRING);
      ret = g_list_prepend (ret, g_strdup (navigator->uri));

      list = ret;
      while (list)
	{
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter, 0, list->data, -1);
	  list = list->next;
	}

      /* Se supero' il numero max di elementi rimuovo l'ultimo: */
      if (g_list_length (ret) > HISTORY_MAX)
	{
	  list = g_list_last (ret);
	  free (list->data);
	  ret = g_list_remove (ret, list->data);
	}

      g_object_set_data (G_OBJECT (navigator->notebook), "entry_history",
			 ret);

      /* Attivo il sistema di complemento */
      completion = gtk_entry_get_completion (GTK_ENTRY (entry));
      gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
      g_object_unref (store);
    }

  /* Mostro tutto: */
  gtk_widget_show_all (navigator->widget);
}

/* Mostro il menu data una risorsa: */
static gint
navigator_open (GtkWidget * button, GdkEventButton * event,
		struct navigator_t *navigator)
{
  if (event->type == GDK_BUTTON_PRESS)
    {
      GtkWidget *menu =
	g_object_get_data (G_OBJECT (navigator->notebook), "menu");
      gchar *value = g_object_get_data (G_OBJECT (button), "value");

      g_object_set_data (G_OBJECT (menu), "value", value);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
		      (GtkMenuPositionFunc) notebook_position, button,
		      event->button, event->time);

      return TRUE;
    }

  return FALSE;
}

/* Calcolo la posizione del menu (vedi quella in maker.c): */
static void
notebook_position (GtkMenu * menu, gint * xp, gint * yp, gboolean * p,
		   GtkWidget * widget)
{
  GtkRequisition req;
  GdkScreen *screen;
  gint x, y, px, py, monitor_n;
  GdkRectangle monitor;
  static gint menu_size = 0;

  if (!menu_size)
    {
      gtk_widget_realize (GTK_WIDGET (menu));
      menu_size = GTK_WIDGET (menu)->allocation.width;
    }

  widget = gtk_widget_get_parent (widget);

  if (menu_size < widget->allocation.width)
    gtk_widget_set_size_request (GTK_WIDGET (menu), widget->allocation.width,
				 -1);
  else
    gtk_widget_set_size_request (GTK_WIDGET (menu), menu_size, -1);

  gdk_window_get_origin (widget->window, &px, &py);
  gtk_widget_size_request (gtk_widget_get_toplevel (widget), &req);

  y = py + widget->allocation.y + widget->allocation.height + 1;
  x = px + widget->allocation.x;

  screen = gtk_widget_get_screen (gtk_widget_get_toplevel (widget));
  monitor_n = gdk_screen_get_monitor_at_point (screen, px, py);
  gdk_screen_get_monitor_geometry (screen, monitor_n, &monitor);

  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  if ((x + req.width) > monitor.x + monitor.width)
    x -= (x + req.width) - (monitor.x + monitor.width);
  else if (x < monitor.x)
    x = monitor.x;

  if ((y + req.height) > monitor.y + monitor.height)
    y -= widget->allocation.height + req.height + 1;
  else if (y < monitor.y)
    y = monitor.y;

  *xp = x;
  *yp = y;
}

/* Questa funzione crea il menu per una risorsa: */
static GtkWidget *
notebook_menu (GtkWidget * notebook)
{
  GtkWidget *menu, *item, *image;

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_with_mnemonic (_("Open"));
  gtk_container_add (GTK_CONTAINER (menu), item);
  g_signal_connect ((gpointer) item, "activate",
		    G_CALLBACK (navigator_show), notebook);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  item = gtk_image_menu_item_new_with_mnemonic (_("Open in a new tab"));
  gtk_container_add (GTK_CONTAINER (menu), item);
  g_signal_connect ((gpointer) item, "activate",
		    G_CALLBACK (navigator_show_tab), notebook);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_set_sensitive (item, FALSE);

  item = gtk_image_menu_item_new_with_mnemonic (_("Import as RDF"));
  gtk_container_add (GTK_CONTAINER (menu), item);
  g_signal_connect ((gpointer) item, "activate",
		    G_CALLBACK (navigator_import), notebook);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  item =
    gtk_image_menu_item_new_with_mnemonic (_
					   ("Import as RDF in a new window"));
  gtk_container_add (GTK_CONTAINER (menu), item);
  g_signal_connect ((gpointer) item, "activate",
		    G_CALLBACK (navigator_import_new), notebook);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  item = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), item);
  gtk_widget_set_sensitive (item, FALSE);

  item = gtk_image_menu_item_new_with_mnemonic (_("Open in a browser"));
  gtk_container_add (GTK_CONTAINER (menu), item);
  g_signal_connect ((gpointer) item, "activate",
		    G_CALLBACK (navigator_browser), notebook);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  gtk_widget_show_all (menu);
  return menu;
}

/* Questa funzione apre il navigatore ad una uri leggendo questo uri
 * dall'oggetto grafico */
static void
navigator_clicked (GtkWidget * button, struct navigator_t *navigator)
{
  gchar *value;

  value = g_object_get_data (G_OBJECT (button), "value");
  navigator_start (navigator, value, NULL);
}

/* Apertura di un URI: */
static void
navigator_start (struct navigator_t *navigator, gchar * value, GList * rdf)
{
  /* Se c'era gia' un URI aperto aggiorno il back: */
  if (navigator->uri)
    {
      struct navigator_history_t *history;
      GtkAdjustment *adj;

      history =
	(struct navigator_history_t *)
	g_malloc0 (sizeof (struct navigator_t));

      history->uri = navigator->uri;

      /* Salvo il valore della scrollbar: */
      adj =
	gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW
					     (navigator->sw));
      history->adj = gtk_adjustment_get_value (adj);

      /* Se mi passano una lista rdf vuol dire che il documento nuovo non
       * ha lo stessa base dati. Quindi il back deve avere salvata quella
       * vecchia: */
      if (rdf)
	history->rdf = navigator->rdf;

      navigator->back = g_list_prepend (navigator->back, history);

      if (g_list_length (navigator->back) > BACK_FORWARD_MAX)
	{
	  GList *list;

	  list = g_list_last (navigator->back);
	  navigator_history_free ((struct navigator_history_t *) list->data);
	  navigator->back = g_list_remove (navigator->back, list->data);
	}
    }

  /* Aggiorno l'uri e lancio la refresh: */
  navigator->uri = g_strdup (value);
  if (rdf)
    navigator->rdf = rdf;

  navigator_refresh (navigator, 0);
}

/* Questa funzione lancia un browser: */
static void
navigator_browser (GtkWidget * button, GtkWidget * notebook)
{
  GtkWidget *menu = gtk_widget_get_parent (button);
  gchar *value;

  value = g_object_get_data (G_OBJECT (menu), "value");
  browser (value);
}

/* Questa funzione lancia la navigator start da un click sul menu popup: */
static void
navigator_show (GtkWidget * button, GtkWidget * notebook)
{
  GtkWidget *menu = gtk_widget_get_parent (button);
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  gchar *value;
  GList *list;
  struct navigator_t *navigator;

  value = g_object_get_data (G_OBJECT (menu), "value");
  list = g_object_get_data (G_OBJECT (notebook), "data");
  list = g_list_nth (list, page);

  if (!list)
    return;

  navigator = list->data;
  navigator_start (navigator, value, NULL);
}

/* Uguale a quella sopra ma in una nuova tab: */
static void
navigator_show_tab (GtkWidget * button, GtkWidget * notebook)
{
  GtkWidget *menu = gtk_widget_get_parent (button);
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  struct navigator_t *navigator;
  GList *data, *tmp, *list;
  gchar *value;
  GtkWidget *sw;

  value = g_object_get_data (G_OBJECT (menu), "value");
  data = g_object_get_data (G_OBJECT (notebook), "data");
  list = g_list_nth (data, page);

  if (!list)
    return;

  /* Creo una nuova struttura: */
  navigator = (struct navigator_t *) g_malloc0 (sizeof (struct navigator_t));

  navigator->uri = g_strdup (value);
  data = g_list_append (data, navigator);
  g_object_set_data (G_OBJECT (notebook), "data", data);

  /* Duplico tutte le triplette: */
  tmp = ((struct navigator_t *) list->data)->rdf;
  while (tmp)
    {
      navigator->rdf = g_list_append (navigator->rdf, rdf_copy (tmp->data));
      tmp = tmp->next;
    }

  /* Creo la grafica: */
  navigator->label = gtk_label_new (value);
  gtk_widget_set_size_request (navigator->label, 200, -1);
  gtk_label_set_ellipsize (GTK_LABEL (navigator->label), PANGO_ELLIPSIZE_END);

  navigator->widget = gtk_table_new (0, 0, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  navigator->sw = sw;
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
					 navigator->widget);

  /* Lancio la refresh: */
  navigator->notebook = notebook;
  navigator_refresh (navigator, 0);

  /* Setto in primo piano: */
  page = gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				   sw, navigator->label);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
  gtk_widget_show_all (notebook);
}

/* Questa funzione importa un documento rdf: */
static void
navigator_import (GtkWidget * button, GtkWidget * notebook)
{
  GtkWidget *menu = gtk_widget_get_parent (button);
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  gchar *value;
  GList *list;
  GList *rdf, *ns;

  value = g_object_get_data (G_OBJECT (menu), "value");
  list = g_object_get_data (G_OBJECT (notebook), "data");
  list = g_list_nth (list, page);

  if (!list)
    return;

  /* Se non riesco a parsarlo, chiedo se voglio provare a ricercare
   * documenti RDF nell'URI */
  if (rdf_parse_thread (value, NULL, &rdf, &ns, FALSE, NULL, NULL) == FALSE)
    {
      gchar s[1024];

      g_snprintf (s, sizeof (s),
		  _("Error opening resource '%s'\n"
		    "Try to search some RDFs in this URI?"), value);

      /* Se si' lancio la funzione navigator try */
      if (dialog_ask (s) == GTK_RESPONSE_OK)
	search_open (value, navigator_try, notebook);

      return;
    }

  /* Libero memoria: */
  g_list_foreach (ns, (GFunc) namespace_free, NULL);
  g_list_free (ns);

  /* Richieamo navigator_pages per creare tutte le tab necessarie: */
  navigator_pages (notebook, rdf);

  /* Libero memoria: */
  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
  g_list_free (rdf);
}

/* Questa funzione importa in una nuova finestra. Cambia il finale, ma e'
 * uguale a quella sopra:*/
static void
navigator_import_new (GtkWidget * button, GtkWidget * notebook)
{
  GtkWidget *menu = gtk_widget_get_parent (button);
  gint page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  gchar *value;
  GList *list;
  GList *rdf, *ns;

  value = g_object_get_data (G_OBJECT (menu), "value");
  list = g_object_get_data (G_OBJECT (notebook), "data");
  list = g_list_nth (list, page);

  if (!list)
    return;

  if (rdf_parse_thread (value, NULL, &rdf, &ns, FALSE, NULL, NULL) == FALSE)
    {
      gchar s[1024];

      g_snprintf (s, sizeof (s),
		  _("Error opening resource '%s'\n"
		    "Try to search some RDFs in this URI?"), value);

      if (dialog_ask (s) == GTK_RESPONSE_OK)
	search_open (value, navigator_try, NULL);

      return;
    }

  g_list_foreach (ns, (GFunc) namespace_free, NULL);
  g_list_free (ns);

  /* Lancio un navigatore: */
  navigator (rdf);

  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
  g_list_free (rdf);
}

/* Questa funzione serve per l'autocompletmaento nella barra di navigazione: */
static gboolean
navigator_uri_match (GtkEntryCompletion * completion,
		     const gchar * key, GtkTreeIter * iter,
		     GtkWidget * notebook)
{
  GtkTreeModel *model;
  char *uri = NULL;
  gboolean ret = FALSE;
  GValue val = { 0, };

  model = gtk_entry_completion_get_model (completion);

  gtk_tree_model_get_value (model, iter, 0, &val);
  uri = (gchar *) g_value_get_string (&val);

  if (g_str_has_prefix (uri, key) ||
      (g_str_has_prefix (uri, "ftp://") &&
       g_str_has_prefix (uri + 6, key)) ||
      (g_str_has_prefix (uri, "http://") &&
       g_str_has_prefix (uri + 7, key)) ||
      (g_str_has_prefix (uri, "https://") && g_str_has_prefix (uri + 8, key)))
    ret = TRUE;

  g_free (uri);

  return ret;
}

/* Questa funzione crea tante pagine quanti i soggetti di una serie di
 * triplette: */
static void
navigator_pages (GtkWidget * notebook, GList * rdf)
{
  struct navigator_t *navigator;
  struct rdf_t *data;
  GList *t;
  gint page;
  GtkWidget *sw;
  GList *list = g_object_get_data (G_OBJECT (notebook), "data");

  /* Per ogni tripletta: */
  t = rdf;
  while (t)
    {
      data = t->data;

      /* Se non e' un nodo blank... */
      if (rdf_blank (rdf, data->subject) == FALSE)
	{
	  GList *tmp;

	  /* ... e non e' gia' in elenco: */
	  tmp = list;
	  while (tmp)
	    {
	      navigator = tmp->data;
	      if (!strcmp (data->subject, navigator->uri))
		break;
	      tmp = tmp->next;
	    }

	  /* Alloco: */
	  if (!tmp)
	    {
	      navigator =
		(struct navigator_t *)
		g_malloc0 (sizeof (struct navigator_t));

	      navigator->uri = g_strdup (data->subject);
	      list = g_list_append (list, navigator);

	      tmp = rdf;
	      while (tmp)
		{
		  navigator->rdf =
		    g_list_append (navigator->rdf, rdf_copy (tmp->data));
		  tmp = tmp->next;
		}

	      navigator->label = gtk_label_new (data->subject);
	      gtk_widget_set_size_request (navigator->label, 200, -1);
	      gtk_label_set_ellipsize (GTK_LABEL (navigator->label),
				       PANGO_ELLIPSIZE_END);

	      navigator->widget = gtk_table_new (0, 0, 0);

	      sw = gtk_scrolled_window_new (NULL, NULL);
	      navigator->sw = sw;
	      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
						   GTK_SHADOW_ETCHED_IN);
	      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					      GTK_POLICY_AUTOMATIC,
					      GTK_POLICY_AUTOMATIC);

	      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
						     navigator->widget);

	      navigator->notebook = notebook;
	      /* Refreshio: */
	      navigator_refresh (navigator, 0);

	      /* Metto in primo piano: */
	      page = gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
					       sw, navigator->label);

	      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);
	    }
	}

      t = t->next;
    }

  /* Aggiorno nel widget la lista */
  g_object_set_data (G_OBJECT (notebook), "data", list);
  gtk_widget_show_all (notebook);
}

/* Quando si attiva la barra di navigazione: */
static void
navigator_go (GtkWidget * button, GtkWidget * notebook)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (notebook), "entry");
  gchar *text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));
  GList *rdf, *ns;

  /* Parso l'uri o faccio ricerca: */
  if (rdf_parse_thread (text, NULL, &rdf, &ns, FALSE, NULL, NULL) == FALSE)
    {
      gchar s[1024];

      g_snprintf (s, sizeof (s),
		  _("Error opening resource '%s'\n"
		    "Try to search some RDFs in this URI?"), text);

      if (dialog_ask (s) == GTK_RESPONSE_OK)
	search_open (text, navigator_try, notebook);

      return;
    }

  /* Libero memoria, apro le pagine, ed esco */
  g_list_foreach (ns, (GFunc) namespace_free, NULL);
  g_list_free (ns);

  navigator_pages (notebook, rdf);

  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
  g_list_free (rdf);
}

/* Questa funzione chiede un URI da aprire e poi lancia il navigatore: */
void
navigator_open_uri (void)
{
  gchar *uri;
  GList *rdf, *ns;
  struct download_t *download;

  if (!
      (uri = uri_chooser (_("Open RDF URI for the navigator..."), &download)))
    return;

  if (rdf_parse_thread (uri, download, &rdf, &ns, FALSE, NULL, NULL) == FALSE)
    {
      gchar s[1024];

      g_snprintf (s, sizeof (s),
		  _("Error opening resource '%s'\n"
		    "Try to search some RDFs in this URI?"), uri);

      if (dialog_ask (s) == GTK_RESPONSE_OK)
	search_open (uri, navigator_try, NULL);

      g_free (uri);

      if (download)
	download_free (download);
      return;
    }

  if (download)
    download_free (download);

  g_list_foreach (ns, (GFunc) namespace_free, NULL);
  g_list_free (ns);

  navigator (rdf);

  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
  g_list_free (rdf);

  g_free (uri);
}

/* Questa funzione e' di callback rispetto a search open */
static void
navigator_try (gchar * text, gpointer notebook)
{
  GList *rdf, *ns;

  if (rdf_parse_thread (text, NULL, &rdf, &ns, FALSE, NULL, NULL) == FALSE)
    {
      gchar s[1024];

      g_snprintf (s, sizeof (s), _("Error opening resource '%s'!"), text);

      dialog_msg (s);
      return;
    }

  if (notebook)
    navigator_pages (notebook, rdf);
  else
    navigator (rdf);

  /* Libero memoria, apro pagine ed esco */
  g_list_foreach (ns, (GFunc) namespace_free, NULL);
  g_list_free (ns);

  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
  g_list_free (rdf);
}

static void
navigator_info (GtkWidget * widget, gchar * predicate)
{
  GtkWidget *dialog;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *event;
  GtkWidget *button;
  gchar s[1024];

  GList *labels = NULL;
  GList *comments = NULL;
  gchar *range = NULL;
  gchar *domain = NULL;

  GList *list;

  list = rdfs_list;
  while (list)
    {
      GList *l;
      struct rdfs_t *rdfs;

      rdfs = (struct rdfs_t *) list->data;
      l = rdfs->data;

      while (l)
	{
	  struct rdf_t *rdf = l->data;

	  if (!strcmp (rdf->subject, predicate))
	    {
	      if (!strcmp (rdf->predicate, RDFS_COMMENT)
		  && rdf->object_type == RDF_OBJECT_LITERAL)
		{
		  struct info_box_t *info =
		    g_malloc0 (sizeof (struct info_box_t));
		  info->lang = rdf->lang;
		  info->value = rdf->object;

		  comments = g_list_append (comments, info);
		}

	      else if (!strcmp (rdf->predicate, RDFS_LABEL)
		       && rdf->object_type == RDF_OBJECT_LITERAL)
		{
		  struct info_box_t *info =
		    g_malloc0 (sizeof (struct info_box_t));
		  info->lang = rdf->lang;
		  info->value = rdf->object;

		  labels = g_list_append (labels, info);
		}

	      else if (!strcmp (rdf->predicate, RDFS_DOMAIN))
		domain = rdf->object;

	      else if (!strcmp (rdf->predicate, RDFS_RANGE))
		range = rdf->object;
	    }


	  l = l->next;
	}

      list = list->next;
    }

  if (!comments && !labels && !range && !domain)
    {
      dialog_msg (_("No info for this URI"));
      return;
    }

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s -%s", PACKAGE, VERSION,
	      _("Information..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  frame = gtk_frame_new (predicate);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE,
		      0);

  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

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

  if (labels)
    {
      list = labels;
      while (list)
	{
	  struct info_box_t *info = list->data;

	  if (!info->lang)
	    {
	      gtk_label_set_text (GTK_LABEL (label), info->value);
	      break;
	    }

	  list = list->next;
	}

      if (!list)
	gtk_label_set_text (GTK_LABEL (label),
			    ((struct info_box_t *) labels->data)->value);
    }

  g_signal_connect (G_OBJECT (event), "button_press_event",
		    G_CALLBACK (info_box_popup_show), labels);
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

  g_signal_connect (G_OBJECT (event), "button_press_event",
		    G_CALLBACK (info_box_popup_show), comments);
  g_signal_connect (G_OBJECT (event), "button_release_event",
		    G_CALLBACK (info_box_popup_hide), NULL);

  if (comments)
    {
      list = comments;
      while (list)
	{
	  struct info_box_t *info = list->data;

	  if (!info->lang)
	    {
	      gtk_label_set_text (GTK_LABEL (label), info->value);
	      break;
	    }

	  list = list->next;
	}

      if (!list)
	gtk_label_set_text (GTK_LABEL (label),
			    ((struct info_box_t *) comments->data)->value);
    }

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 2);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Domain: "));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  label = gtk_label_new (domain);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 5);

  frame = gtk_frame_new (NULL);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 2);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Range: "));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  label = gtk_label_new (range);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 5);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (labels)
    {
      g_list_foreach (labels, (GFunc) g_free, NULL);
      g_list_free (labels);
    }

  if (comments)
    {
      g_list_foreach (comments, (GFunc) g_free, NULL);
      g_list_free (comments);
    }
}

/* EOF ?!? */

/*
Le grand palindrome - George Perec

  Trace l'inégal palindrome. Neige. Bagatelle, dira Hercule. Le brut repentir,
cet écrit né Perec. L'arc lu pèse trop, lis à vice-versa.
  Perte. Cerise d'une vérité banale, le Malstrom, Alep, mort édulcoré, crêpe
porté de ce désir brisé d'un iota. Livre si aboli, tes sacres ont éreinté,
cor cruel, nos albatros. Etre las, autel bâti, miette vice-versa du jeu que
fit, nacré, médical, le sélénite relaps, ellipsoïdal.
  Ivre il bat, la turbine bat, l'isolé me ravale: le verre si obéi du Pernod --
eh, port su! -- obsédante sonate teintée d'ivresse.
  Ce rêve se mit -- peste! -- à blaguer. Beh! L'art sec n'a si peu qu'algèbre
s'élabore de l'or évalué. Idiome étiré, hésite, bâtard replié, l'os nu. Si, à
la gêne sècrete-- verbe nul à l'instar de cinq occis--, rets amincis, drailles
inégales, il, avatar espacé, caresse ce noir Belzebuth, il offensé, tire!
  L'écho fit (à désert): Salut, sang, robe et été.
  Fièvres.
  Adam, rauque; il écrit: Abrupt ogre, eh, cercueil, l'avenir tu, effilé, génial
à la rue (murmure sud eu ne tire vaseline séparée; l'épeire gelée rode: Hep,
mortel ?) lia ta balafre native.
  Litige. Regagner (et ne m'...).
  Ressac. Il frémit, se sape, na! Eh, cavale! Timide, il nia ce sursaut.
  Hasard repu, tel, le magicien à morte me lit. Un ignare le rapsode, lacs ému,
mixa, mêla:
  Hep, Oceano Nox, ô, béchamel azur! éjaculer! Topaze!
  Le cèdre, malabar faible, Arsinoë le macule, mante ivre, glauque, pis, l'air
atone (sic). Art sournois: si, médicinale, l'autre glace (Melba ?) l'un ?
  N'alertai ni pollen (retêter: gercé, repu, denté...) ni tobacco.
  Tu, désir, brio rimé, eh, prolixe nécrophore, tu ferres l'avenir velu, ocre,
cromant-né?
  Rage, l'ara. Veuglaire. Sedan, tes elzévirs t'obsèdent. Romain? Exact. Et
  Nemrod selle ses Samson!
  Et nier téocalli ?
  Cave canem (car ce nu trop minois -- rembuscade d'éruptives à babil --
admonesta, fil accru, Têtebleu! qu'Ariane évitât net.
  Attention, ébénier factice, ressorti du réel. Ci-git. Alpaga, gnôme, le héros
se lamente, trompé, chocolat: ce laid totem, ord, nil aplati, rituel biscornu;
ce sacré bédeau (quel bât ce Jésus!). Palace piégé, Torpédo drue si à fellah
tôt ne peut ni le Big à ruer bezef.
  L'eugéniste en rut consuma d'art son épi d'éolienne ici rot (eh... rut ?).
Toi, d'idem gin, élèvera, élu, bifocal, l'ithos et notre pathos à la hauteur de
sec salamalec?
  élucider. Ion éclaté: Elle? Tenu. Etna but (item mal famé), degré vide,
julep: macédoine d'axiomes, sac semé d'école, véniel, ah, le verbe enivré (ne
sucer ni arreter, eh ça jamais!) lu n'abolira le hasard?
  Nu, ottoman à écho, l'art su, oh, tara zéro, belle Deborah, ô, sacre! Pute,
vertubleu, qualité si vertu à la part tarifé (décalitres ?) et nul n'a lu trop
s'il séria de ce basilic Iseut.
  Il a prié bonzes, Samaritain, Tora, vilains monstres (idolâtre DNA en sus)
rêvés, évaporés:
  Arbalète (bètes) en noce du Tell ivre-mort, émeri tu: O, trapu à elfe, il lie
l'os, il lia jérémiade lucide. Petard! Rate ta reinette, bigleur cruel, non à
ce lot! Si, farcis-toi dito le coeur!
  Lied à monstre velu, ange ni bête, sec à pseudo délire: Tsarine (sellée, là),
  Cid, Arétin, abruti de Ninive, Déjanire. . .
  Le Phenix, eve de sables, écarté, ne peut égarer racines radiales en mana:
l'Oubli, fétiche en argile.
  Foudre.
  Prix: Ile de la Gorgone en roc, et, ô, Licorne écartelée,
  Sirène, rumb à bannir à ma (Red n'osa) niére de mimosa:
  Paysage d'Ourcq ocre sous ive d'écale;
  Volcan. Roc: tarot célé du Père.
  Livres.
  Silène bavard, replié sur sa nullité (nu à je) belge: ipséité banale. L' (eh,
ça!) hydromel à ri, psaltérion. Errée Lorelei...
  Fi! Marmelade déviré d'Aladine. D'or, Noël: crèche (l'an ici taverne gelée
dès bol...) à santon givré, fi!, culé de l'âne vairon.
  Lapalisse élu, gnoses sans orgueil (écru, sale, sec). Saluts: angiome. T'es
si crâneur!
  . . .
  Rue. Narcisse! Témoignas-tu ! l'ascèse, là, sur ce lieu gros, nasses
ongulées...
  S'il a pal, noria vénale de Lucifer, vignot nasal (obsédée, le genre
vaticinal), eh, Cercle, on rode, nid à la dérive, Dèdale (M. . . !) ramifié?
  Le rôle erre, noir, et la spirale mord, y hache l'élan abêti: Espiègle
(béjaune) Till: un as rusé. 
  Il perdra. Va bene.
  Lis, servile repu d'électorat, cornac, Lovelace. De visu, oser ?
  Coq cru, ô, Degas, y'a pas, ô mime, de rein à sonder: à marin nabab, murène
risée.
  Le trace en roc, ilote cornéen.
  O, grog, ale d'elixir perdu, ô, feligrane! Eh, cité, fil bu!
ô ! l'anamnèse, lai d'arsenic, arrérage tué, pénétra ce sel-base de Vexin. Eh,
pèlerin à (Je: devin inédit) urbanité radicale (elle s'en ira...), stérile,
dodu.
  Espaces (été biné ? gnaule ?) verts.
  Nomade, il rue, ocelot. Idiot-sic rafistolé: canon! Leur cruel gibet te
niera, têtard raté, pédicule d'aimé rejailli.
  Soleil lie, fléau, partout ire (Métro, Mer, Ville...) tu déconnes. été:
bètel à brasero. Pavese versus Neandertal! O, diserts noms ni à Livarot ni à
Tir!
  Amassez.
  N'obéir.
  Pali, tu es ici: lis abécédaires, lis portulan: l'un te sert-il? à ce défi
rattrapa l'autre? Vise-t-il auquel but rêvé tu perças?
  Oh, arobe d'ellébore, Zarathoustra! L'ohcéan à mot (Toundra? Sahel?) à ri: Lob
à nul si à ma jachère, terrain récusé, nervi, née brève l'haleine véloce de
mes casse-moix à (Déni, ô!) décampé.
  Lu, je diverge de ma flamme titubante: une telle (étal, ce noir édicule cela
mal) ascèse drue tua, ha, l'As.
  Oh, taper ! Tontes ! Oh, tillac, ô, fibule à reve l'énigme (d'idiot tu)
rhétoricienne.
  Il, ‘dipe, Nostradamus nocturne et, si né Guelfe, zébreur à Gibelin tué
(pentothal?), le faiseur d'ode protège.
  Ipéca...: lapsus.
  Eject à bleu qu'aède berça sec. Un roc si bleu! Tir. ital.: palindrome tôt
dialectal. Oc ? Oh, cep mort et né, mal essoré, hélé. Mon gag aplati gicle.
érudit rossérecit, ça freine, benoit, net.
  Ta tentative en air auquel bète, turc, califat se (nom d'Ali-Baba!) sévit,
pure de -- d'ac? -- submersion importune, crac, menace, vacilla, co-étreinte...
  Nos masses, elles dorment ? Etc... Axé ni à mort-né des bots. Rivez! Les Etna
de Serial-Guevara l'égarent. N'amorcer coulevrine.
  Valser. Refuter.
  Oh, porc en exil (Orphée), miroir brisé du toc cabotin et né du Perec: Regret
éternel. L'opiniâtre. L'annu- lable.
  Mec, Alger tua l'élan ici démission. Ru ostracisé, notarial, si peu qu'Alger,
  Viet-Nam (élu caméléon!), Israël, Biafra, bal à merde: celez, apôtre Luc à
  Jéruzalem, ah ce boxon! On à écopé, ha, le maximum
  Escale d'os, pare le rang inutile. Métromane ici gamelle, tu perdras. Ah, tu
as rusé! Cain! Lied imité la vache (à ne pas estimer) (flic assermenté,
rengagé) régit.
  Il évita, nerf à la bataille trompé.
  Hé, dorée, l'égérie pelée rape, sénile, sa vérité nue du sérum: rumeur à la
laine, gel, if, feutrine, val, lieu-créche, ergot, pur, Bâtir ce lieu qu'Armada
serve: if étété, éborgnas-tu l'astre sédatif?
  Oh, célérités ! Nef! Folie! Oh, tubez ! Le brio ne cessera, ce cap sera ta
valise; l'âge: ni sel-liard (sic) ni master-(sic)-coq, ni cédrats, ni la lune
brève. Tercé, sénégalais, un soleil perdra ta bétise héritée (Moi-Dieu, la
vérole!)
  Déroba le serbe glauque, pis, ancestral, hébreu (Galba et Septime-Sévère).
  Cesser, vidé et nié. Tetanos. Etna dès boustrophédon répudié. Boiser. Révèle
l'avare mélo, s'il t'a béni, brutal tablier vil. Adios. Pilles, pale rétine, 
e sel, l'acide mercanti. Feu que Judas rêve, civette imitable, tu as alerté,
sort à blason, leur croc. Et nier et n'oser. Casse-t-il, ô, baiser vil ? à toi,
nu désir brisé, décédé, trope percé, roc lu. Détrompe la. Morts: l'Ame, l'élan
abêti, revenu. Désire ce trépas rêvé: Ci va! S'il porte, sépulcral, ce
repentir, cet écrit ne perturbe le lucre: Haridelle, ta gabegie ne mord ni la
plage ni l'écart. 
*/

/* EOF */

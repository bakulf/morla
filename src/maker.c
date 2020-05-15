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

static GtkWidget *create_list (struct editor_data_t *data);
static GtkWidget *create_nslist (struct editor_data_t *data);
static GtkWidget *my_label (struct rdf_t *rdf, const gchar * text,
			    GCallback callback, gpointer user_data,
			    GCallback callback_press);
static void callback_select (GtkWidget * w, gpointer dummy);
static void callback_nsselect (GtkWidget * w, gpointer dummy);
static void on_order_by_namespace (GtkWidget * w, struct editor_data_t *data);
static void on_order_by_prefix (GtkWidget * w, struct editor_data_t *data);
static void on_order_by_visible (GtkWidget * w, struct editor_data_t *data);
static void on_order_by_subject (GtkWidget *, struct editor_data_t *data);
static void on_order_by_predicate (GtkWidget *, struct editor_data_t *data);
static void on_order_by_object (GtkWidget *, struct editor_data_t *data);
static void create_arrow (GtkWidget ** w, gchar * text, gboolean show,
			  GtkArrowType type);
static void toggled_ns (GtkWidget * w, struct editor_data_t *data);
static gint button_press_callback (GtkWidget * w, GdkEventButton * e,
				   gpointer dummy);
static gint button_press_nscallback (GtkWidget * w, GdkEventButton * e,
				     gpointer dummy);
static void set_color (struct editor_data_t *data);
static void change_prefix (GtkWidget * w, struct editor_data_t *data);
static void change_triple (GtkWidget * w, struct editor_data_t *data);
static void change_resources (GtkWidget * w, struct editor_data_t *data);
static void change_nsresource (GtkWidget * w, struct editor_data_t *data);
static void open_resource (GtkWidget * w, struct editor_data_t *data);
static void import_resource (GtkWidget * w, struct editor_data_t *data);
static void open_browser (GtkWidget * w, struct editor_data_t *data);
static GtkStyle *style_normal (void);
static GtkStyle *style_selected (void);
static void set_style (GtkWidget * w, GtkStyle * s);

/* Ritorna l'elemento attualmente visualizzato dall'utente fra tutti i 
 * documenti aperti: */
struct editor_data_t *
editor_get_data (void)
{
  struct editor_data_t *data;
  gint page;

  /* E' il notebook che mi dice la pagina */
  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
  data = editor_data;

  while (data)
    {
      if (!page)
	return data;

      page--;
      data = data->next;
    }

  return NULL;
}

/* Timer generico */
gint
editor_timeout (gpointer dummy)
{
  struct editor_data_t *data;
  struct thread_t *t;
  GList *l;

  /* Se c'e' stato un aggiornamento e devo comunicarlo, lo faccio e poi
   * basta: */
  if (update_show == 1 && update_check ())
    update_show = 0;

  /* Controllo tutti i thread abortiti se controllo se qualcuno e' stato
   * chiuso: */
  l = thread_child;
  while (l)
    {
      t = l->data;

      /* Se e' stato chiuso me lo dice un flag > 0: */
      if (*t->flag > 0)
	{
	  /* Se devo lanciare la callback al termine del processo, questo
	   * e' il momento: */
	  if (t->func)
	    t->func (t->data, NULL);

	  /* Rimuovo dalla lista e libero la memoria: */
	  thread_child = g_list_remove (thread_child, (gpointer) t);
	  g_free (t);
	  break;
	}

      l = l->next;
    }

  /* Guardo lo status della statusbar: */
  switch (editor_statusbar_lock)
    {
      /* E' locckata non devo fare niente: */
    case LOCK:
      return TRUE;

      /* Attesa... sblocco e al prossimo giro la risetto: */
    case WAIT:
      editor_statusbar_lock = UNLOCK;
      return TRUE;
    }

  /* Becco il documento attuale e aggiorno lo statusbar: */
  data = editor_get_data ();

  if (!data || rdf_is_no_saved (data) == FALSE)
    statusbar_set (_("Editing..."));

  return TRUE;
}

/* Questa funzione crea un nuovo marker cioe' un nuovo spazio di lavoro: */
void
editor_new_maker (struct editor_data_t *data)
{
  gint page;
  struct editor_data_t *tmp;
  GtkWidget *vbox, *sw, *paned, *button, *image;

  /* Inserisco nella lista degli spazi di lavoro: */
  if (editor_data)
    {
      tmp = editor_data;
      while (tmp->next)
	tmp = tmp->next;

      tmp->next = data;
      tmp = tmp->next;
    }
  else
    editor_data = data;

  /* Creo interfaccia: */
  vbox = gtk_vbox_new (FALSE, 5);
  data->maker = vbox;

  paned = gtk_vpaned_new ();
  gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_paned_pack1 (GTK_PANED (paned), sw, TRUE, FALSE);

  data->list = create_list (data);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
					 data->list);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_paned_pack2 (GTK_PANED (paned), sw, FALSE, TRUE);

  data->nslist = create_nslist (data);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
					 data->nslist);

  gtk_widget_show_all (data->maker);

  /* Setto il titolo: */
  if (data->uri)
    data->label = gtk_label_new (data->uri);
  else
    data->label = gtk_label_new (_("New RDF Document"));

  vbox = gtk_hbox_new (FALSE, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), data->label, FALSE, FALSE, 0);

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_container_set_border_width (GTK_CONTAINER (button), 0);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (editor_close_selected), data);

  gtk_widget_show_all (vbox);

  /* Attacco al notebook e metto in primo piano: */
  page =
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), data->maker, vbox);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page);

  /* Setto la statusbar: */
  editor_statusbar_lock = WAIT;
  statusbar_set (_("New RDF Document"));
}

/* Refresh della finestra e un bel casino: dividi ed impera */
void
editor_refresh (struct editor_data_t *data)
{
  GtkWidget *parent;

  /* Distruggo tutto: */
  parent = gtk_widget_get_parent (data->list);
  gtk_container_remove (GTK_CONTAINER (parent), data->list);

  /* Ordino le triplette: */
  data->rdf =
    g_list_sort_with_data (data->rdf, (GCompareDataFunc) editor_compare,
			   data);

  /* Creo la lista: */
  data->list = create_list (data);

  gtk_container_add (GTK_CONTAINER (parent), data->list);
  gtk_widget_show_all (data->list);

  /* Refreshio lo spazio di namespace: */
  editor_refresh_nslist (data);
}

/* Spazio namespace refresh: */
void
editor_refresh_nslist (struct editor_data_t *data)
{
  GtkWidget *parent;

  /* Distruggo tutto: */
  parent = gtk_widget_get_parent (data->nslist);
  gtk_container_remove (GTK_CONTAINER (parent), data->nslist);

  /* Creo e mostro: */
  data->nslist = create_nslist (data);
  gtk_container_add (GTK_CONTAINER (parent), data->nslist);
  gtk_widget_show_all (data->nslist);
}

/* ORDERS *******************************************************************/

/* Questa funzione serve per ordinare le triplette. Viene usata da g_list_sort
 * e si basa sulla colonna di ordine scelta dall'utente: */
gint
editor_compare (struct rdf_t *a, struct rdf_t *b, struct editor_data_t *data)
{
  switch (data->compare)
    {
    case ORDER_SUBJECT:
      return strcmp (a->subject, b->subject);

    case ORDER_SUBJECT_DEC:
      return strcmp (b->subject, a->subject);

    case ORDER_PREDICATE:
      return strcmp (a->predicate, b->predicate);

    case ORDER_PREDICATE_DEC:
      return strcmp (b->predicate, a->predicate);

    case ORDER_OBJECT:
      return strcmp (a->object, b->object);

    case ORDER_OBJECT_DEC:
      return strcmp (b->object, a->object);

    default:
      return 0;
    }
}

/* Questa ordina i namespace e funziona come sopra */
gint
editor_nscompare (struct namespace_t * a, struct namespace_t * b,
		  struct editor_data_t * data)
{
  switch (data->nscompare)
    {
    case NSORDER_NAMESPACE:
      return strcmp (a->namespace, b->namespace);

    case NSORDER_NAMESPACE_DEC:
      return strcmp (b->namespace, a->namespace);

    case NSORDER_PREFIX:
      return strcmp (a->prefix, b->prefix);

    case NSORDER_PREFIX_DEC:
      return strcmp (b->prefix, a->prefix);

    case NSORDER_VISIBLE:
      return a->visible == b->visible;

    case NSORDER_VISIBLE_DEC:
      return a->visible != b->visible;

    default:
      return 0;
    }
}

/* Quando si clicca per ordinare in base al namespace: */
static void
on_order_by_namespace (GtkWidget * w, struct editor_data_t *data)
{
  if (data->nscompare == NSORDER_NAMESPACE)
    data->nscompare = NSORDER_NAMESPACE_DEC;
  else
    data->nscompare = NSORDER_NAMESPACE;

  if (data->namespace)
    data->namespace =
      g_list_sort_with_data (data->namespace,
			     (GCompareDataFunc) editor_nscompare, data);

  /* Refresh: */
  editor_refresh_nslist (data);
}

/* Lo stesso vare per i prefix: */
static void
on_order_by_prefix (GtkWidget * w, struct editor_data_t *data)
{
  if (data->nscompare == NSORDER_PREFIX)
    data->nscompare = NSORDER_PREFIX_DEC;
  else
    data->nscompare = NSORDER_PREFIX;

  if (data->namespace)
    data->namespace =
      g_list_sort_with_data (data->namespace,
			     (GCompareDataFunc) editor_nscompare, data);

  editor_refresh_nslist (data);
}

/* Ordina in base all'essere visibile: */
static void
on_order_by_visible (GtkWidget * w, struct editor_data_t *data)
{
  if (data->nscompare == NSORDER_VISIBLE)
    data->nscompare = NSORDER_VISIBLE_DEC;
  else
    data->nscompare = NSORDER_VISIBLE;

  if (data->namespace)
    data->namespace =
      g_list_sort_with_data (data->namespace,
			     (GCompareDataFunc) editor_nscompare, data);

  editor_refresh_nslist (data);
}

/* Le triplette si ordinano in base al soggetto: */
static void
on_order_by_subject (GtkWidget * w, struct editor_data_t *data)
{
  if (data->compare == ORDER_SUBJECT)
    data->compare = ORDER_SUBJECT_DEC;
  else
    data->compare = ORDER_SUBJECT;

  if (data->rdf)
    data->rdf =
      g_list_sort_with_data (data->rdf, (GCompareDataFunc) editor_compare,
			     data);

  editor_refresh (data);
}

/* Le triplette si ordinano in base al predicato: */
static void
on_order_by_predicate (GtkWidget * w, struct editor_data_t *data)
{
  if (data->compare == ORDER_PREDICATE)
    data->compare = ORDER_PREDICATE_DEC;
  else
    data->compare = ORDER_PREDICATE;

  if (data->rdf)
    data->rdf =
      g_list_sort_with_data (data->rdf, (GCompareDataFunc) editor_compare,
			     data);

  editor_refresh (data);
}

/* Le triplette si ordinano in base all'oggetto: */
static void
on_order_by_object (GtkWidget * w, struct editor_data_t *data)
{
  if (data->compare == ORDER_OBJECT)
    data->compare = ORDER_OBJECT_DEC;
  else
    data->compare = ORDER_OBJECT;

  if (data->rdf)
    data->rdf =
      g_list_sort_with_data (data->rdf, (GCompareDataFunc) editor_compare,
			     data);

  editor_refresh (data);
}

/* Questa funzione viene lanciata quando si chicca su un namespace per
 * renderlo visible: */
static void
toggled_ns (GtkWidget * w, struct editor_data_t *data)
{
  struct namespace_t *ns = g_object_get_data (G_OBJECT (w), "namespace");

  if (!ns || !GTK_IS_WIDGET (data->list))
    return;

  /* Invero la sua visibilita': */
  ns->visible = !ns->visible;

  /* Refreshio: */
  editor_refresh (data);
}

/* MODE LIST *****************************************************************/

/* Questa funzione crea le frecce e il titolo di ogni colonna: */
static void
create_arrow (GtkWidget ** w, gchar * text, gboolean show, GtkArrowType type)
{
  GtkWidget *parent;
  GtkWidget *new, *box, *arrow, *label;

  /* Creo un pulsante nuovo: */
  new = gtk_button_new ();

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (new), box);

  gtk_box_pack_start (GTK_BOX (box), gtk_hbox_new (0, 0), TRUE, TRUE, 0);

  /* Devo proprio metterle? */
  if (show)
    {
      arrow = gtk_arrow_new (type, GTK_SHADOW_NONE);
      gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 0);
    }

  label = gtk_label_new (text);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  /* Come sopra: */
  if (show)
    {
      arrow = gtk_arrow_new (type, GTK_SHADOW_NONE);
      gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 0);
    }

  /* Sostituisco al bottone precedente: */
  if (GTK_IS_WIDGET (*w) && (parent = gtk_widget_get_parent (*w)))
    {
      gtk_container_remove (GTK_CONTAINER (parent), *w);
      gtk_container_add (GTK_CONTAINER (parent), new);
    }

  gtk_box_pack_start (GTK_BOX (box), gtk_hbox_new (0, 0), TRUE, TRUE, 0);

  gtk_widget_show_all (new);
  *w = new;
}

/* La creazione di un elemento grafico avviene in questa funzione: */
static GtkWidget *
create_item (struct rdf_t *rdf, gchar * text, GCallback callback,
	     gpointer dummy, GtkStyle * style, GCallback button_callback)
{
  GtkWidget *event, *t, *label;

  event = gtk_event_box_new ();
  gtk_widget_set_style (event, style);

  t = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (event), t);

  /* Uso la funzione my_label. Essenziale e' la callback da lanciare quando
   * si clicca sopra:*/
  label =
    my_label (rdf, text, G_CALLBACK (callback), dummy,
	      G_CALLBACK (button_callback));
  gtk_widget_set_style (label, style);

  gtk_box_pack_start (GTK_BOX (t), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (t), gtk_hbox_new (0, 0), FALSE, FALSE, 0);

  return event;
}

/* Questa funzione crea l'oggetto grafico lista per una serie di triplette: */
static GtkWidget *
create_list (struct editor_data_t *data)
{
  GtkWidget *box, *list;
  GtkStyle *style;
  GtkWidget *event, *e;
  GtkWidget *label, *t;
  GList *l;

  /* Genero gli style: */
  style = style_normal ();

  e = gtk_event_box_new ();
  gtk_widget_set_style (e, style);

  box = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (e), box);

  list = gtk_hbox_new (TRUE, 0);

  /* Creo i titoli colonna */
  data->subject_order = NULL;
  create_arrow (&data->subject_order, _("Subject"),
		(data->compare == ORDER_SUBJECT
		 || data->compare == ORDER_SUBJECT_DEC),
		(data->compare == ORDER_SUBJECT_DEC));

  gtk_box_pack_start (GTK_BOX (list), data->subject_order, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (data->subject_order), "clicked",
		    G_CALLBACK (on_order_by_subject), data);

  data->predicate_order = NULL;
  create_arrow (&data->predicate_order, _("Predicate"),
		(data->compare == ORDER_PREDICATE
		 || data->compare == ORDER_PREDICATE_DEC),
		(data->compare == ORDER_PREDICATE_DEC));

  gtk_box_pack_start (GTK_BOX (list), data->predicate_order, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (data->predicate_order), "clicked",
		    G_CALLBACK (on_order_by_predicate), data);

  data->object_order = NULL;
  create_arrow (&data->object_order, _("Object"),
		(data->compare == ORDER_OBJECT
		 || data->compare == ORDER_OBJECT_DEC),
		(data->compare == ORDER_OBJECT_DEC));

  gtk_box_pack_start (GTK_BOX (list), data->object_order, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (data->object_order), "clicked",
		    G_CALLBACK (on_order_by_object), data);

  gtk_box_pack_start (GTK_BOX (box), list, FALSE, FALSE, 0);

  /* Per ogni tripletta: */
  l = data->rdf;
  while (l)
    {
      struct rdf_t *rdf = l->data;

      /* Se questo elemento e' invisibile: */
      if (!namespace_visible (data->namespace, rdf))
	{
	  rdf->subject_widget = NULL;
	  rdf->predicate_widget = NULL;
	  rdf->object_widget = NULL;

	  l = l->next;
	  continue;
	}

      /* Creo un item per soggetto, predicato e coggetto: */
      list = gtk_hbox_new (TRUE, 0);

      if (!strcmp (rdf->subject, THIS_DOCUMENT))
	event =
	  create_item (rdf, data->uri ? data->uri : _("This Document"),
		       G_CALLBACK (callback_select), data, style,
		       G_CALLBACK (button_press_callback));
      else
	event =
	  create_item (rdf, rdf->subject, G_CALLBACK (callback_select), data,
		       style, G_CALLBACK (button_press_callback));

      gtk_box_pack_start (GTK_BOX (list), event, TRUE, TRUE, 0);
      rdf->subject_widget = event;

      event =
	create_item (rdf, rdf->predicate, G_CALLBACK (callback_select), data,
		     style, G_CALLBACK (button_press_callback));

      gtk_box_pack_start (GTK_BOX (list), event, TRUE, TRUE, 0);
      rdf->predicate_widget = event;

      event = gtk_event_box_new ();
      gtk_widget_set_style (event, style);

      t = gtk_hbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (event), t);

      /* Per l'oggetto devo comunicare se e' risorsa, letterale o
       * blank node: */
      switch (rdf->object_type)
	{
	case RDF_OBJECT_RESOURCE:
	  label = gtk_label_new (_("<b>(R)</b> "));
	  break;

	case RDF_OBJECT_LITERAL:
	  label = gtk_label_new (_("<b>(L)</b> "));
	  break;

	case RDF_OBJECT_BLANK:
	  label = gtk_label_new (_("<b>(B)</b> "));
	  break;

	default:
	  label = NULL;
	}

      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_widget_set_style (label, style);
      gtk_box_pack_start (GTK_BOX (t), label, FALSE, FALSE, 0);

      /* setto la lingua: */
      if (rdf->lang)
	{
	  gchar *s;

	  s = g_strdup_printf ("<b>[%s]</b> ", rdf->lang);
	  label = gtk_label_new (s);
	  g_free (s);

	  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	  gtk_widget_set_style (label, style);
	  gtk_box_pack_start (GTK_BOX (t), label, FALSE, FALSE, 0);
	}

      /* setto il datatype: */
      if (rdf->datatype)
	{
	  gchar *s;

	  s = g_strdup_printf ("<b>&lt;%s&gt;</b> ", rdf->datatype);
	  label = gtk_label_new (s);
	  g_free (s);

	  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	  gtk_widget_set_style (label, style);
	  gtk_box_pack_start (GTK_BOX (t), label, FALSE, FALSE, 0);
	}

      /* Infine l'oggetto: */
      label =
	my_label (rdf, rdf->object, G_CALLBACK (callback_select), data,
		  G_CALLBACK (button_press_callback));
      gtk_widget_set_style (label, style);

      gtk_box_pack_start (GTK_BOX (t), label, TRUE, TRUE, 0);
      gtk_box_pack_start (GTK_BOX (t), gtk_hbox_new (0, 0), FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (list), event, TRUE, TRUE, 0);
      rdf->object_widget = event;

      gtk_box_pack_start (GTK_BOX (box), list, FALSE, FALSE, 0);

      l = l->next;

      /* Tra un elemento e l'altro metto una linea di separazione: */
      if (l)
	gtk_box_pack_start (GTK_BOX (box), gtk_hseparator_new (), FALSE,
			    FALSE, 0);
    }

  g_object_unref (G_OBJECT (style));
  set_color (data);
  return e;
}

/* Creazione dell'oggetto lista per i namespace: */
static GtkWidget *
create_nslist (struct editor_data_t *data)
{
  GtkWidget *box, *list;
  GtkStyle *style;
  GtkWidget *event, *e, *label, *tbox;
  gchar *s;
  GList *l;

  /* Style: */
  style = style_normal ();

  e = gtk_event_box_new ();
  gtk_widget_set_style (e, style);

  box = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (e), box);

  list = gtk_hbox_new (TRUE, 0);

  /* titoli: */
  data->namespace_order = NULL;
  create_arrow (&data->namespace_order, _("Namespace"),
		(data->nscompare == NSORDER_NAMESPACE
		 || data->nscompare == NSORDER_NAMESPACE_DEC),
		(data->nscompare == NSORDER_NAMESPACE_DEC));

  gtk_box_pack_start (GTK_BOX (list), data->namespace_order, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (data->namespace_order), "clicked",
		    G_CALLBACK (on_order_by_namespace), data);

  data->prefix_order = NULL;
  create_arrow (&data->prefix_order, _("Prefix"),
		(data->nscompare == NSORDER_PREFIX
		 || data->nscompare == NSORDER_PREFIX_DEC),
		(data->nscompare == NSORDER_PREFIX_DEC));

  gtk_box_pack_start (GTK_BOX (list), data->prefix_order, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (data->prefix_order), "clicked",
		    G_CALLBACK (on_order_by_prefix), data);

  data->visible_order = NULL;
  create_arrow (&data->visible_order, _("Visible"),
		(data->nscompare == NSORDER_VISIBLE
		 || data->nscompare == NSORDER_VISIBLE_DEC),
		(data->nscompare == NSORDER_VISIBLE_DEC));

  gtk_box_pack_start (GTK_BOX (list), data->visible_order, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (data->visible_order), "clicked",
		    G_CALLBACK (on_order_by_visible), data);

  gtk_box_pack_start (GTK_BOX (box), list, FALSE, FALSE, 0);

  /* Per ogni namespace: */
  l = data->namespace;
  while (l)
    {
      list = gtk_hbox_new (TRUE, 0);

      /* il namespace */
      event =
	create_item (NULL, ((struct namespace_t *) l->data)->namespace,
		     G_CALLBACK (callback_nsselect), data, style,
		     G_CALLBACK (button_press_nscallback));

      gtk_box_pack_start (GTK_BOX (list), event, TRUE, TRUE, 0);
      ((struct namespace_t *) l->data)->namespace_widget = event;

      /* preference */
      event = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (list), event, TRUE, TRUE, 0);
      gtk_widget_set_style (event, style);

      tbox = gtk_hbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (event), tbox);

      s =
	g_strdup_printf ("<b>%s</b>",
			 ((struct namespace_t *) l->data)->prefix);
      label = gtk_label_new (s);
      g_free (s);

      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_widget_set_style (label, style);

      ((struct namespace_t *) l->data)->prefix_widget = label;
      gtk_box_pack_start (GTK_BOX (tbox), label, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (tbox), gtk_hbox_new (0, 0), TRUE, TRUE, 0);

      /* visibile */
      event = gtk_event_box_new ();
      gtk_box_pack_start (GTK_BOX (list), event, TRUE, TRUE, 0);
      gtk_widget_set_style (event, style);

      tbox = gtk_hbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (event), tbox);

      label = gtk_check_button_new ();
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label),
				    ((struct namespace_t *) l->data)->
				    visible);
      g_object_set_data (G_OBJECT (label), "namespace", l->data);
      g_signal_connect (G_OBJECT (label), "toggled", G_CALLBACK (toggled_ns),
			data);
      gtk_widget_set_style (label, style);

      ((struct namespace_t *) l->data)->visible_widget = label;
      gtk_box_pack_start (GTK_BOX (tbox), label, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (tbox), gtk_hbox_new (0, 0), TRUE, TRUE, 0);

      gtk_box_pack_start (GTK_BOX (box), list, FALSE, FALSE, 0);

      l = l->next;

      /* Anche que una linea di separazione: */
      if (l)
	gtk_box_pack_start (GTK_BOX (box), gtk_hseparator_new (), FALSE,
			    FALSE, 0);
    }

  g_object_unref (G_OBJECT (style));
  set_color (data);
  return e;
}

/* Questa funzione viene lanciata dalle GTK quando un elemento my_label
 * viene distrutto. Serve per liberare memoria: */
static void
on_clear_label (GtkWidget * widget)
{
  gchar *label = g_object_get_data (G_OBJECT (widget), "label");
  g_free (label);
}

/* Se il mouse entra nel campo di analisi di una label: */
static gboolean
on_ebox_enter (GtkWidget * box, GdkEventCrossing * event, GtkLabel * label)
{
  static GdkCursor *cursor = NULL;

  /* Cambio cursore e stato: */
  gtk_widget_set_state (GTK_WIDGET (label), GTK_STATE_PRELIGHT);

  if (!cursor)
    cursor = gdk_cursor_new (GDK_HAND2);

  gdk_window_set_cursor (box->window, cursor);
  return TRUE;
}


/* Quando si esce da un label: */
static gboolean
on_ebox_leave (GtkWidget * box, GdkEventCrossing * event, GtkLabel * label)
{
  /* Setto lo stato normale e metto il cursore di prima */
  gtk_widget_set_state (GTK_WIDGET (label), GTK_STATE_NORMAL);
  gdk_window_set_cursor (box->window, NULL);
  return TRUE;
}

/* Questa funzione crea un my_label object  che e' una label gtk + evoluta: */
static GtkWidget *
my_label (struct rdf_t *rdf, const gchar * text, GCallback callback,
	  gpointer user_data, GCallback callback_press)
{
  GtkWidget *ebox;
  GtkWidget *label;
  gchar *temp;
  GtkStyle *style;
  gchar *s;

  /* Style */
  style = style_normal ();

  /* Un event box che wrappa certi segnali: */
  ebox = gtk_event_box_new ();
  gtk_widget_set_tooltip_text (ebox, text);
  gtk_widget_set_events (ebox,
			 GDK_BUTTON_PRESS_MASK |
			 GDK_KEY_PRESS_MASK |
			 GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  gtk_widget_set_can_focus(ebox, TRUE);

  gtk_widget_show (ebox);

  /* la label: */
  temp = markup ((gchar *) text);
  s = g_strdup_printf ("<b>%s</b>", temp);
  g_free (temp);

  label = gtk_label_new (s);
  g_free (s);

  gtk_widget_set_style (label, style);
  g_object_unref (G_OBJECT (style));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  temp = g_strdup (text);

  g_object_set_data (G_OBJECT (label), "label", temp);
  g_object_set_data (G_OBJECT (label), "rdf", rdf);
  g_signal_connect (G_OBJECT (label), "destroy",
		    G_CALLBACK (on_clear_label), NULL);

  g_signal_connect (G_OBJECT (ebox), "enter-notify-event",
		    G_CALLBACK (on_ebox_enter), label);
  g_signal_connect (G_OBJECT (ebox), "leave-notify-event",
		    G_CALLBACK (on_ebox_leave), label);

  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (ebox), label);

  g_object_set_data (G_OBJECT (ebox), "callback", callback);

  g_signal_connect (G_OBJECT (ebox), "button-press-event",
		    G_CALLBACK (callback_press), user_data);

  return ebox;
}

/* MENUS ********************************************************************/

/* Creazione del menu per le triplette: */
static GtkMenu *
create_menu (struct editor_data_t *data)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *image;

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_with_label (_("Open with this editor"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (open_resource),
		    data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Open in a browser"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (open_browser),
		    data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Modify this RDF triple"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-edit", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (change_triple), data);

  item =
    gtk_image_menu_item_new_with_label (_
					("Modify all nodes/archs like this"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-edit", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (change_resources), data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item =
    gtk_image_menu_item_new_with_label (_("Cut all nodes/archs like this"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-cut", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_cut), NULL);

  item =
    gtk_image_menu_item_new_with_label (_("Copy all nodes/archs like this"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-copy", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_copy), NULL);

  item =
    gtk_image_menu_item_new_with_label (_
					("Delete all nodes/archs like this"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_delete), NULL);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Cut this RDF triple"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-cut", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_cut_triple), NULL);

  item = gtk_image_menu_item_new_with_label (_("Copy this RDF triple"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-copy", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_copy_triple), NULL);

  item = gtk_image_menu_item_new_with_label (_("Delete this RDF triple"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_delete_triple), NULL);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item =
    gtk_image_menu_item_new_with_label (_("Show info about this element"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock (
#ifdef GTK_STOCK_INFO
				     "gtk-info",
#else
				     "gtk-dialog-info",
#endif
				     GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (info_selected),
		    NULL);


  return GTK_MENU (menu);
}

/* menu per i namespace: */
static GtkMenu *
create_nsmenu (struct editor_data_t *data)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *image;

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_with_label (_("Import this RDFS"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (import_resource),
		    data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Open with this editor"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (open_resource),
		    data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Open in a browser"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (open_browser),
		    data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Modify the RDF resource"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-edit", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (change_nsresource), data);

  item = gtk_image_menu_item_new_with_label (_("Modify the prefix"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-edit", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (change_prefix), data);

  item = gtk_menu_item_new ();
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  item = gtk_image_menu_item_new_with_label (_("Cut this RDFS"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-cut", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_cut), NULL);

  item = gtk_image_menu_item_new_with_label (_("Copy this RDFS"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-copy", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_copy), NULL);


  item = gtk_image_menu_item_new_with_label (_("Delete this RDFS"));
  gtk_widget_show (item);
  gtk_container_add (GTK_CONTAINER (menu), item);

  image = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

  g_signal_connect (G_OBJECT (item), "activate",
		    G_CALLBACK (editor_delete), NULL);

  return GTK_MENU (menu);
}

/* Questa funzione setta il menu in un punto preciso dello schermo. Esattamente
 * sotto la label o subito sopra: */
static void
popup_position (GtkMenu * menu, gint * xp, gint * yp, gboolean * p,
		gpointer data)
{
  GtkRequisition req;
  GdkScreen *screen;
  gint x, y, px, py, monitor_n;
  GdkRectangle monitor;
  static gint menu_size = 0;

  /* Il padre: */
  data = (gpointer) gtk_widget_get_parent (GTK_WIDGET (data));

  /* Se non so la grandezza, relizzo il menu e salvo: */
  if (!menu_size)
    {
      gtk_widget_realize (GTK_WIDGET (menu));
      menu_size = GTK_WIDGET (menu)->allocation.width;
    }

  /* Se le dimensioni sono diverse dal padre, metto a posto: */
  if (menu_size != GTK_WIDGET (data)->allocation.width)
    gtk_widget_set_size_request (GTK_WIDGET (menu),
				 GTK_WIDGET (data)->allocation.width, -1);
  else
    gtk_widget_set_size_request (GTK_WIDGET (menu), menu_size, -1);

  /* Prendo x e y del padre: */
  gdk_window_get_origin (GTK_WIDGET (data)->window, &px, &py);
  gtk_widget_size_request (gtk_widget_get_toplevel (GTK_WIDGET (data)), &req);

  /* Calcolo le nuove x e y */
  y =
    py + GTK_WIDGET (data)->allocation.y +
    GTK_WIDGET (data)->allocation.height + 1;
  x = px + GTK_WIDGET (data)->allocation.x;

  /* Prendo lo screen per sapere la geometria: */
  screen =
    gtk_widget_get_screen (gtk_widget_get_toplevel (GTK_WIDGET (data)));
  monitor_n = gdk_screen_get_monitor_at_point (screen, px, py);
  gdk_screen_get_monitor_geometry (screen, monitor_n, &monitor);

  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  /* Se il menu starebbe fuori lo rimetto dentro: */
  if ((x + req.width) > monitor.x + monitor.width)
    x -= (x + req.width) - (monitor.x + monitor.width);
  else if (x < monitor.x)
    x = monitor.x;

  if ((y + req.height) > monitor.y + monitor.height)
    y -= GTK_WIDGET (data)->allocation.height + req.height + 1;
  else if (y < monitor.y)
    y = monitor.y;

  /* Questi sono i valori di x e y: */
  *xp = x;
  *yp = y;
}

/* Funzione attivata dal click su una my_label delle triplette: */
static gint
button_press_callback (GtkWidget * w, GdkEventButton * e, gpointer dummy)
{
  void *(*callback) (GtkWidget *, gpointer) =
    g_object_get_data (G_OBJECT (w), "callback");

  if (!callback)
    return FALSE;

  /* Se e' un doppio click oppure e' un tasto destro: */
  if (e->type == GDK_2BUTTON_PRESS || e->button == 3)
    {
      if (e->button == 3)
	callback (w, dummy);

      gtk_menu_popup (create_menu (dummy), NULL, NULL, popup_position, w,
		      e->button, e->time);
      return TRUE;
    }

  /* Altrimenti lancio la funzione salvata dentro l'oggetto */
  else if (e->type == GDK_BUTTON_PRESS)
    {
      callback (w, dummy);
      return TRUE;
    }

  return FALSE;
}

/* Idem per i namespace: */
static gint
button_press_nscallback (GtkWidget * w, GdkEventButton * e, gpointer dummy)
{
  void *(*callback) (GtkWidget *, gpointer) =
    g_object_get_data (G_OBJECT (w), "callback");

  if (!callback)
    return FALSE;

  if (e->type == GDK_2BUTTON_PRESS || e->button == 3)
    {
      if (e->button == 3)
	callback (w, dummy);

      gtk_menu_popup (create_nsmenu (dummy), NULL, NULL, popup_position, w,
		      e->button, e->time);
      return TRUE;
    }

  else if (e->type == GDK_BUTTON_PRESS)
    {
      callback (w, dummy);
      return TRUE;
    }

  return FALSE;
}

/* MENU CALLBACKS ***********************************************************/

/* Questa funzione permette di cambiare prefix: */
static void
change_prefix (GtkWidget * w, struct editor_data_t *data)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *button;
  struct namespace_t *ns;
  GList *list;
  gchar *s;

  if (!data || !data->node_nsselect)
    return;

  list = data->namespace;
  while (list)
    {
      ns = list->data;
      if (!strcmp (ns->namespace, data->node_nsselect))
	break;

      list = list->next;
    }

  if (!list)
    return;

  dialog = gtk_dialog_new ();

  s =
    g_strdup_printf ("%s %s - %s", PACKAGE, VERSION,
		     _("Change Prefix for..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  g_free (s);

  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  s = g_strdup_printf (_("RDFS: <b>%s</b>"), data->node_nsselect);
  label = gtk_label_new (s);
  g_free (s);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, TRUE, TRUE,
		      0);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  label = gtk_label_new (_("Prefix: "));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), ns->prefix);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

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
      gchar *text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!text || !*text)
	{
	  dialog_msg (_("No a valid prefix!"));
	  continue;
	}

      /* Libero memoria e aggiorno: */
      if (ns->prefix)
	g_free (ns->prefix);

      ns->prefix = g_strdup (text);
      editor_refresh_nslist (data);
      data->changed = TRUE;

      break;
    }

  gtk_widget_destroy (dialog);
}

/* Questa funzione rinomina tutte le risorse di un certo tipo: */
static void
change_resources (GtkWidget * w, struct editor_data_t *data)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *button;
  GList *list;
  struct rdf_t *rdf;
  gchar s[1024];
  struct unredo_t *unredo;
  int ok;

  if (!data || !data->node_select)
    return;

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Modify all URIs like this..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  label = gtk_label_new (_("New URI: "));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), data->node_select);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

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
      gchar *text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!text || !*text)
	{
	  dialog_msg (_("No a valid URI!"));
	  continue;
	}

      /* Qui attivo un undo: */
      unredo = unredo_new (data, UNREDO_TYPE_CHANGE);

      /* Per ogni tripletta: */
      list = data->rdf;
      while (list)
	{
	  rdf = (struct rdf_t *) list->data;
	  ok = 0;

	  /* Se coincide aggiorno l'undo copiando la tripletta e poi 
	   * sostituisco, sia per il soggetto: */
	  if (!strcmp (rdf->subject, data->node_select))
	    {
	      unredo_add (unredo, rdf, rdf_copy (rdf));
	      ok++;

	      g_free (rdf->subject);
	      rdf->subject = g_strdup (text);
	    }

	  /* Sia per il predicato */
	  if (!strcmp (rdf->predicate, data->node_select))
	    {
	      if (!ok)
		{
		  unredo_add (unredo, rdf, rdf_copy (rdf));
		  ok++;
		}

	      g_free (rdf->predicate);
	      rdf->predicate = g_strdup (text);
	    }

	  /* Sia per il complemento oggetto: */
	  if (!strcmp (rdf->object, data->node_select))
	    {
	      if (!ok)
		{
		  unredo_add (unredo, rdf, rdf_copy (rdf));
		  ok++;
		}

	      g_free (rdf->object);
	      rdf->object = g_strdup (text);
	    }

	  list = list->next;
	}

      /* Libero memoria e dico che ora ad essere selezionato e' il
       * valore nuovo: */
      free (data->node_select);
      data->node_select = g_strdup (text);

      /* Aggiorno i namespace: */
      g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
      g_list_free (data->namespace);
      namespace_parse (data->rdf, &data->namespace);

      /* Refreshio: */
      editor_refresh (data);
      data->changed = TRUE;
      break;
    }

  gtk_widget_destroy (dialog);
}

/* Cambiare una tripletta e' molto piu' semplice: */
static void
change_triple (GtkWidget * w, struct editor_data_t *data)
{
  struct rdf_t *new;
  struct unredo_t *unredo;

  if (!data || !data->rdf_select)
    return;

  /* Uso editor_triple: */
  if (!
      (new =
       editor_triple (data->rdf_select, _("Modify this RDF triple..."))))
    return;

  /* Attivo un sistema di undo: */
  unredo = unredo_new (data, UNREDO_TYPE_CHANGE);
  unredo_add (unredo, data->rdf_select, rdf_copy (data->rdf_select));

  /* Rimetto i valori nuovi: */
  if (data->rdf_select->subject)
    g_free (data->rdf_select->subject);
  data->rdf_select->subject = new->subject;

  if (data->rdf_select->predicate)
    g_free (data->rdf_select->predicate);
  data->rdf_select->predicate = new->predicate;

  if (data->rdf_select->object)
    g_free (data->rdf_select->object);
  data->rdf_select->object = new->object;

  if (data->rdf_select->lang)
    g_free (data->rdf_select->lang);
  data->rdf_select->lang = new->lang;

  if (data->rdf_select->datatype)
    g_free (data->rdf_select->datatype);
  data->rdf_select->datatype = new->datatype;

  data->rdf_select->object_type = new->object_type;

  g_free (new);

  /* Ricreo i namespace: */
  g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
  g_list_free (data->namespace);

  namespace_parse (data->rdf, &data->namespace);

  /* Refresh: */
  data->changed = TRUE;
  editor_refresh (data);
}

/* Quando voglio cambiare una risorsa rdfs uso questa funzione: */
static void
change_nsresource (GtkWidget * w, struct editor_data_t *data)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *label;
  GtkWidget *button;
  gchar s[1024];

  if (!data || !data->node_nsselect)
    return;

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Modify this RDF URI..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  label = gtk_label_new (_("New URI: "));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), data->node_nsselect);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

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
      GList *list;
      gint len;
      gchar *tmp, *text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));
      struct unredo_t *unredo;
      int ok;

      if (!text || !*text)
	{
	  dialog_msg (_("No a valid URI!"));
	  continue;
	}

      /* Per ogni namespace sostituisco: */
      list = data->namespace;
      while (list)
	{
	  if (!strcmp
	      (((struct namespace_t *) list->data)->namespace,
	       data->node_nsselect))
	    {
	      g_free (((struct namespace_t *) list->data)->namespace);
	      ((struct namespace_t *) list->data)->namespace =
		g_strdup (text);
	    }

	  list = list->next;
	}

      /* Attivo un undo: */
      unredo = unredo_new (data, UNREDO_TYPE_CHANGE);

      /* Cerco ora tra tutte le triplette: */
      len = strlen (data->node_nsselect);

      list = data->rdf;
      while (list)
	{
	  ok = 0;

	  /* Se coincide, aggiungo all'undo, libero la memoria e
	   * sostituisco, sia per soggetto: */
	  if (!strncmp
	      (((struct rdf_t *) list->data)->subject, data->node_nsselect,
	       len))
	    {
	      unredo_add (unredo, (struct rdf_t *) list->data,
			  rdf_copy ((struct rdf_t *) list->data));
	      ok++;

	      tmp =
		g_strdup_printf ("%s%s", text,
				 ((struct rdf_t *) list->data)->subject +
				 len);
	      g_free (((struct rdf_t *) list->data)->subject);
	      ((struct rdf_t *) list->data)->subject = tmp;
	    }

	  /* sia per predicato: */
	  if (!strncmp
	      (((struct rdf_t *) list->data)->predicate, data->node_nsselect,
	       len))
	    {
	      if (!ok)
		{
		  unredo_add (unredo, (struct rdf_t *) list->data,
			      rdf_copy ((struct rdf_t *) list->data));
		  ok++;
		}

	      tmp =
		g_strdup_printf ("%s%s", text,
				 ((struct rdf_t *) list->data)->predicate +
				 len);
	      g_free (((struct rdf_t *) list->data)->predicate);
	      ((struct rdf_t *) list->data)->predicate = tmp;
	    }

	  /* sia per c.oggetto: */
	  if (((struct rdf_t *) list->data)->object_type ==
	      RDF_OBJECT_RESOURCE
	      && !strncmp (((struct rdf_t *) list->data)->object,
			   data->node_nsselect, len))
	    {
	      if (!ok)
		{
		  unredo_add (unredo, (struct rdf_t *) list->data,
			      rdf_copy ((struct rdf_t *) list->data));
		  ok++;
		}

	      tmp =
		g_strdup_printf ("%s%s", text,
				 ((struct rdf_t *) list->data)->object + len);
	      g_free (((struct rdf_t *) list->data)->object);
	      ((struct rdf_t *) list->data)->object = tmp;
	    }

	  list = list->next;
	}

      /* Cambio la selezione: */
      g_free (data->node_nsselect);
      data->node_nsselect = g_strdup (text);

      /* Modifica e Refresh: */
      data->changed = TRUE;
      editor_refresh (data);

      break;
    }

  gtk_widget_destroy (dialog);
}

/* Quando si chiede di aprire una risorsa dal submenu uso questa funzione: */
static void
open_resource (GtkWidget * w, struct editor_data_t *data)
{
  if (!data)
    return;

  /* Uso editor_open_uri: */
  if (editor_open_uri
      (data->node_select ? data->node_select : data->node_nsselect,
       NULL) == FALSE)
    {
      gchar s[1024];

      g_snprintf (s, sizeof (s), _("Error opening '%s' resource!"),
		  data->node_select ? data->node_select : data->
		  node_nsselect);

      dialog_msg (s);
    }
}

/* Se devo importare un rdfs dalla lista dei namespace faccio: */
static void
import_resource (GtkWidget * w, struct editor_data_t *data)
{
  GList *list;
  struct namespace_t *ns;

  if (!data || !data->node_nsselect)
    return;

  /* Cerco l'elemento: */
  list = data->namespace;
  while (list)
    {
      ns = list->data;
      if (!strcmp (ns->namespace, data->node_nsselect))
	break;

      list = list->next;
    }

  if (!list)
    return;

  /* se l'ho trovato uso rdfs_add */
  rdfs_add (ns->namespace, ns->prefix);
}

/* Apertura in un browser: */
static void
open_browser (GtkWidget * w, struct editor_data_t *data)
{
  if (data->node_select)
    browser (data->node_select);
  else
    browser (data->node_nsselect);
}

/* SELECTIONS ***************************************************************/

/* E' stato selezionato un nodo: */
static void
editor_select_node (struct editor_data_t *data, GtkWidget * w)
{
  GtkWidget *label;
  gchar *text;
  GList *list;
  struct rdf_t *rdf;

  if (!(list = gtk_container_get_children (GTK_CONTAINER (w))))
    return;

  label = list->data;
  g_list_free (list);

  /* Prendo il testo dalla my_label e l'rdf associato: */
  text = g_object_get_data (G_OBJECT (label), "label");
  rdf = (struct rdf_t *) g_object_get_data (G_OBJECT (label), "rdf");

  /* Libero memoria e setto il nuovo elemento: */
  if (data->node_nsselect)
    {
      g_free (data->node_nsselect);
      data->node_nsselect = NULL;
    }

  if (data->node_select)
    g_free (data->node_select);

  data->node_select = g_strdup (text);
  data->rdf_select = rdf;

  /* Setto i colori: */
  set_color (data);
}

/* Item per la selezione dei namespace */
static void
editor_select_nsnode (struct editor_data_t *data, GtkWidget * w)
{
  GtkWidget *label;
  gchar *text;
  GList *list;

  if (!(list = gtk_container_get_children (GTK_CONTAINER (w))))
    return;
  label = list->data;
  g_list_free (list);

  text = g_object_get_data (G_OBJECT (label), "label");

  if (data->node_select)
    {
      g_free (data->node_select);
      data->node_select = NULL;
    }

  if (data->node_nsselect)
    g_free (data->node_nsselect);

  data->node_nsselect = g_strdup (text);
  data->rdf_select = NULL;

  set_color (data);
}

/* Callback della selezione nodo */
static void
callback_select (GtkWidget * w, gpointer dummy)
{
  editor_select_node ((struct editor_data_t *) dummy, w);
}

/* Callback della selezione namesapce */
static void
callback_nsselect (GtkWidget * w, gpointer dummy)
{
  editor_select_nsnode ((struct editor_data_t *) dummy, w);
}

/* COLORS *******************************************************************/

/* Settare i colori in base a cio' che e' stato selezionato: */
static void
set_color (struct editor_data_t *data)
{
  GtkStyle *style1, *style2;
  GList *l;
  gint len;

  /* Mi calcolo la lunghezza del namespace nel caso e' questo selezionato: */
  if (data->node_nsselect)
    {
      if (!(len = strlen (data->node_nsselect)))
	len = -1;
    }

  else if (data->node_select)
    len = 0;

  else
    len = -1;

  style1 = style_normal ();
  style2 = style_selected ();

  /* Per ogni tripletta: */
  l = data->rdf;
  while (l)
    {
      struct rdf_t *rdf = l->data;

      if (rdf->subject_widget)
	{
	  /* Se e' selezionato una risorsa e questa coincide */
	  if ((!len && !strcmp (rdf->subject, data->node_select)) ||
	      /* Oppure se e' selezionato un rdfs e questo coincide: */
	      (len > 0
	       && !strncmp (rdf->subject, data->node_nsselect, len)) ||
	      /* Oppure se e' selezionato una risorsa e quest coincide con 
	       * l'uri del documento, e  questo soggetto e' settato a
	       * this:document */
	      (!len && !strcmp (rdf->subject, THIS_DOCUMENT)
	       && !strcmp (data->node_select,
			   data->uri ? data->uri : _("This Document"))))
	    /* ... coloro: */
	    set_style (rdf->subject_widget, style2);
	  else
	    /* ... altrimenti no: */
	    set_style (rdf->subject_widget, style1);
	}

      /* Item per i predicati: */
      if (rdf->predicate_widget)
	{
	  if ((!len && !strcmp (rdf->predicate, data->node_select))
	      || (len > 0
		  && !strncmp (rdf->predicate, data->node_nsselect, len)))
	    set_style (rdf->predicate_widget, style2);
	  else
	    set_style (rdf->predicate_widget, style1);
	}

      /* E per i c.oggetti */
      if (rdf->object_widget)
	{
	  if (((!len && !strcmp (rdf->object, data->node_select))
	       || (len > 0
		   && !strncmp (rdf->object, data->node_nsselect, len))))
	    set_style (rdf->object_widget, style2);
	  else
	    set_style (rdf->object_widget, style1);
	}

      l = l->next;
    }

  /* Per ogni namespace: */
  l = data->namespace;
  while (l)
    {
      /* Seleziono anche qui: */
      if (((struct namespace_t *) l->data)->namespace_widget)
	{
	  if (len > 0
	      && !strncmp (((struct namespace_t *) l->data)->namespace,
			   data->node_nsselect, len))
	    set_style (((struct namespace_t *) l->data)->namespace_widget,
		       style2);
	  else
	    set_style (((struct namespace_t *) l->data)->namespace_widget,
		       style1);
	}

      l = l->next;
    }

  /* Libero memoria: */
  g_object_unref (G_OBJECT (style1));
  g_object_unref (G_OBJECT (style2));
}

/* Questa funzione genera uno stile normale in base ai colori della
 * configurazione: */
static GtkStyle *
style_normal (void)
{
  GtkStyle *style;
  GdkColor color;

  style = gtk_style_copy (gtk_widget_get_default_style ());
  gdk_color_parse (normal_bg_color, &color);
  style->bg[GTK_STATE_NORMAL] = color;
  gdk_color_parse (normal_fg_color, &color);
  style->fg[GTK_STATE_NORMAL] = color;
  gdk_color_parse (prelight_fg_color, &color);
  style->fg[GTK_STATE_PRELIGHT] = color;

  return style;
}

/* Questa funzione genera uno stile selezionato in base ai colori della
 * configurazione: */
static GtkStyle *
style_selected (void)
{
  GtkStyle *style;
  GdkColor color;

  style = gtk_style_copy (gtk_widget_get_default_style ());
  gdk_color_parse (prelight_bg_color, &color);
  style->bg[GTK_STATE_NORMAL] = color;
  gdk_color_parse (normal_fg_color, &color);
  style->fg[GTK_STATE_NORMAL] = color;
  gdk_color_parse (prelight_fg_color, &color);
  style->fg[GTK_STATE_PRELIGHT] = color;

  return style;
}

/* Questa funzione setta lo style ad ogni oggetto contenuto da un widget
 * ricorsivamente: */
static void
set_style (GtkWidget * w, GtkStyle * s)
{
  GList *list, *old;

  gtk_widget_set_style (GTK_WIDGET (w), s);
  if (!GTK_IS_CONTAINER (w))
    return;

  if (!(list = old = gtk_container_get_children (GTK_CONTAINER (w))))
    return;

  while (list)
    {
      set_style (GTK_WIDGET (list->data), s);
      list = list->next;
    }

  g_list_free (old);
}

/* EOF */

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

gint undo_max_value;

static void unredo_refresh (struct editor_data_t *data);
static void change_free (struct unredo_change_t *change);
static struct unredo_t *unredo_run (struct editor_data_t *, struct unredo_t *,
				    void *f);

/* Questa funzione e' come unredo_new ma e' studiata per gli redo. Gli
 * redo sono gestiti SOLO in questo file. Gli altri gestiscono gli undo: */
static struct unredo_t *
unredo_new_redo (struct editor_data_t *data, enum unredo_type_t type)
{
  struct unredo_t *ret;

  ret = (struct unredo_t *) g_malloc0 (sizeof (struct unredo_t));

  ret->type = type;

  data->redo = g_list_prepend (data->redo, ret);

  /* Attivo la sensibilita' degli redo: */
  gtk_widget_set_sensitive (redo_widget[0], TRUE);
  gtk_widget_set_sensitive (redo_widget[1], TRUE);

  return ret;
}

/* Dato un documento e una tipologia di undo, questo viene allocato,
 * inserito in una lista e restituito: */
struct unredo_t *
unredo_new (struct editor_data_t *data, enum unredo_type_t type)
{
  struct unredo_t *ret;

  ret = (struct unredo_t *) g_malloc0 (sizeof (struct unredo_t));

  ret->type = type;

  data->undo = g_list_prepend (data->undo, ret);

  if (g_list_length (data->undo) > undo_max_value)
    {
      GList *tmp = g_list_last (data->undo);
      unredo_destroy (data, tmp->data);
    }

  /* Attivo la sensibilita' degli undo: */
  gtk_widget_set_sensitive (undo_widget[0], TRUE);
  gtk_widget_set_sensitive (undo_widget[1], TRUE);

  return ret;
}

/* Aggiunge un elemento ad un redo o un undo: */
void
unredo_add (struct unredo_t *unredo, struct rdf_t *rdf, struct rdf_t *other)
{
  struct unredo_change_t *change;

  /* A seconda della tipologia inserisco in una lista direttamente il
   * puntatore oppure creo una struttura dati change apposta: */
  switch (unredo->type)
    {
    case UNREDO_TYPE_ADD:
      unredo->list = g_list_append (unredo->list, rdf);
      break;

    case UNREDO_TYPE_DEL:
      unredo->list = g_list_append (unredo->list, rdf);
      break;

    case UNREDO_TYPE_CHANGE:
      change =
	(struct unredo_change_t *)
	g_malloc0 (sizeof (struct unredo_change_t));

      change->now = rdf;
      change->prev = other;

      unredo->list = g_list_append (unredo->list, change);
      break;
    }
}

/* Distrugge un undo o un redo: */
void
unredo_destroy (struct editor_data_t *data, struct unredo_t *del)
{
  data->undo = g_list_remove (data->undo, del);

  /* Rimuovo l'ultimo elemento oppure rimuovo anche della memoria: */
  switch (del->type)
    {
    case UNREDO_TYPE_ADD:
      g_list_free (del->list);
      break;

    case UNREDO_TYPE_DEL:
      g_list_foreach (del->list, (GFunc) rdf_free, NULL);
      g_list_free (del->list);
      break;

    case UNREDO_TYPE_CHANGE:
      g_list_foreach (del->list, (GFunc) change_free, NULL);
      g_list_free (del->list);
      break;
    }

  g_free (del);
}

/* Questa funzione aziona un undo: */
void
unredo_undo (void)
{
  struct editor_data_t *data = editor_get_data ();
  struct unredo_t *unredo, *new;

  if (!data || !data->undo)
    return;

  /* Prendo il primo unredo, eseguo, rimuovo e refreshio: */
  unredo = (struct unredo_t *) data->undo->data;

  /* Qui dico che l'operazione inversa sara' un unredo_new_redo */
  new = unredo_run (data, unredo, unredo_new_redo);

  data->undo = g_list_remove (data->undo, unredo);
  g_free (unredo);

  unredo_refresh (data);

  data->changed = TRUE;
  editor_refresh (data);
}

/* Come sopra ma per gli redo: */
void
unredo_redo (void)
{
  struct editor_data_t *data = editor_get_data ();
  struct unredo_t *unredo, *new;

  if (!data || !data->redo)
    return;

  unredo = (struct unredo_t *) data->redo->data;

  /* Qui dico che l'operazione inversa sara' un unredo_new */
  new = unredo_run (data, unredo, unredo_new);

  data->redo = g_list_remove (data->redo, unredo);
  g_free (unredo);

  unredo_refresh (data);

  data->changed = TRUE;
  editor_refresh (data);
}

/* Aziona un undo o redo: */
static struct unredo_t *
unredo_run (struct editor_data_t *data, struct unredo_t *unredo, void *f)
{
  struct unredo_t *new = NULL;
  struct unredo_change_t *change;
  struct rdf_t *rdf;
  GList *l;
  struct unredo_t *(*func) (struct editor_data_t *, enum unredo_type_t) = f;

  /* A seconda della tipologia, creo una operazione inversa passatami come
   * argomento: */
  switch (unredo->type)
    {
    case UNREDO_TYPE_ADD:
      new = func (data, UNREDO_TYPE_DEL);

      /* Ricerco l'elemento e se lo trovo, rimuovo e metto dentro al redo */
      l = unredo->list;
      while (l)
	{
	  if (g_list_find (data->rdf, l->data))
	    {
	      unredo_add (new, l->data, NULL);
	      data->rdf = g_list_remove (data->rdf, l->data);
	    }

	  l = l->next;
	}

      g_list_free (unredo->list);
      break;

    case UNREDO_TYPE_DEL:
      new = func (data, UNREDO_TYPE_ADD);

      /* Inserisco semplicemente: */
      l = unredo->list;
      while (l)
	{
	  unredo_add (new, l->data, NULL);
	  data->rdf = g_list_append (data->rdf, l->data);
	  l = l->next;
	}

      g_list_free (unredo->list);
      break;

    case UNREDO_TYPE_CHANGE:
      new = func (data, UNREDO_TYPE_CHANGE);

      /* Per i cambiamenti: */
      l = unredo->list;
      while (l)
	{
	  change = l->data;

	  /* Se e' presente nella lista... */
	  if (g_list_find (data->rdf, change->now))
	    {
	      rdf = change->now;

	      /* Se esiste una versione precedente della stessa tripletta
	       * sostituisco: */
	      if (change->prev)
		{

		  unredo_add (new, change->now, rdf_copy (rdf));

		  g_free (rdf->subject);
		  rdf->subject = g_strdup (change->prev->subject);

		  g_free (rdf->predicate);
		  rdf->predicate = g_strdup (change->prev->predicate);

		  g_free (rdf->object);
		  rdf->object = g_strdup (change->prev->object);

		  if (rdf->lang)
		    g_free (rdf->lang);

		  rdf->lang =
		    change->prev->lang ? g_strdup (change->prev->lang) : NULL;

		  if (rdf->datatype)
		    g_free (rdf->datatype);

		  rdf->datatype =
		    change->prev->datatype ? g_strdup (change->prev->
						       datatype) : NULL;

		  rdf->object_type = change->prev->object_type;
		}

	      /* Se non esiste vuol dire che e' un cambiamento misto
	       * ad inserimento e quindi procedo con la rimozione: */
	      else
		{
		  unredo_add (new, rdf_copy (rdf), NULL);
		  data->rdf = g_list_remove (data->rdf, rdf);
		}
	    }

	  /* Se non e' presente nell'elenco vuol dire che e' un cambiamento
	   * misto ad un inserimento e devo rimettere la tripletta: */
	  else
	    {
	      unredo_add (new, change->now, NULL);
	      data->rdf = g_list_append (data->rdf, change->now);
	    }

	  change_free (change);
	  l = l->next;
	}

      g_list_free (unredo->list);
      break;
    }

  return new;
}

/* Questa funzione fa il refresh della sensibilita' dei widget */
static void
unredo_refresh (struct editor_data_t *data)
{
  gtk_widget_set_sensitive (undo_widget[0], data->undo ? TRUE : FALSE);
  gtk_widget_set_sensitive (undo_widget[1], data->undo ? TRUE : FALSE);
  gtk_widget_set_sensitive (redo_widget[0], data->redo ? TRUE : FALSE);
  gtk_widget_set_sensitive (redo_widget[1], data->redo ? TRUE : FALSE);
}

/* Questa funzione libera la memoria di un change */
static void
change_free (struct unredo_change_t *change)
{
  if (!change)
    return;

  /* Se esiste un precedente, lo rimuovo: */
  if (change->prev)
    rdf_free (change->prev);

  /* Altrimenti e' un cambiamento misto e devo rimuovere se non e' presente
   * nel file: */
  else
    {
      struct editor_data_t *data = editor_get_data ();

      if (!g_list_find (data->rdf, change->now))
	rdf_free (change->now);
    }

  g_free (change);
}

/* Quando si cambia il livello di undo/redo dalle preferences devo
 * rimuovere gli eccessi: */
void
unredo_change_value (void)
{
  struct editor_data_t *data = editor_data;

  while (data)
    {
      gint len = g_list_length (data->undo);

      if (len > undo_max_value)
	{
	  len -= undo_max_value;

	  while (len)
	    {
	      GList *tmp = g_list_last (data->undo);
	      unredo_destroy (data, tmp->data);
	      len--;
	    }
	}

      data = data->next;
    }
}

/* EOF */

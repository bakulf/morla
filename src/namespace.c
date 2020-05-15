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

static gchar *namespace_check_prefix (gchar * ns);

/* Funzione per ordinare namespace */
static gint
namespace_compare (struct namespace_t *a, struct namespace_t *b)
{
  return strcmp (a->namespace, b->namespace);
}

/* Controlla l'esistenza di un namespace in elenco e lo comunica */
static gboolean
namespace_check (GList * list, gchar * ns)
{
  while (list)
    {
      if (!strcmp (ns, ((struct namespace_t *) list->data)->namespace))
	return TRUE;

      list = list->next;
    }

  return FALSE;
}

/* Libera la memoria di un namespace: */
void
namespace_free (struct namespace_t *namespace)
{
  if (!namespace)
    return;

  if (namespace->namespace)
    g_free (namespace->namespace);

  if (namespace->prefix)
    g_free (namespace->prefix);

  g_free (namespace);
}

/* Parsa una lista rdf generando una lista di namespace: */
gboolean
namespace_parse (GList * rdf, GList ** ret_list)
{
  GList *ret = NULL;
  struct rdf_t *data;
  gint i = 0;

  /* Per ogni tripletta: */
  while (rdf)
    {
      data = (struct rdf_t *) rdf->data;

      /* Guardo solo il predicato: */
      if (data && data->predicate)
	{
	  gint len = strlen (data->predicate);
	  struct namespace_t *ns;
	  gchar *t;

	  /* Cerco caratteri particolari: */
	  while (len--)
	    if (data->predicate[len] == '/' || data->predicate[len] == '#' ||
		data->predicate[len] == ':')
	      break;

	  /* Se ho trovato qualcosa... */
	  if (len)
	    {
	      t = g_strdup (data->predicate);
	      t[len + 1] = 0;

	      /* ... controllo se ce l'ho gia' ed inserisco: */
	      if (namespace_check (ret, t) == FALSE)
		{
		  ns =
		    (struct namespace_t *)
		    g_malloc0 (sizeof (struct namespace_t));

		  ns->namespace = g_strdup (t);
		  if (!(ns->prefix = namespace_check_prefix (t)))
		    ns->prefix = g_strdup_printf ("_%d", ++i);
		  ns->visible = TRUE;

		  ret = g_list_append (ret, ns);
		}

	      g_free (t);
	    }
	}

      rdf = rdf->next;
    }

  /* Ordino: */
  *ret_list = g_list_sort (ret, (GCompareFunc) namespace_compare);
  return TRUE;
}

/* Chiede se e' visibile un determinato rdf_t in base ad una lista
 * di namespace: */
gboolean
namespace_visible (GList * list, struct rdf_t * rdf)
{
  struct namespace_t *ns;
  gint len;

  while (list)
    {
      ns = (struct namespace_t *) list->data;

      /* Se non e' visibile... */
      if (!ns->visible)
	{
	  len = strlen (ns->namespace);

	  /* Ed e' nel soggetto, predicato o coggetto */
	  if (!strncmp (ns->namespace, rdf->subject, len))
	    return FALSE;

	  if (!strncmp (ns->namespace, rdf->predicate, len))
	    return FALSE;

	  if (rdf->object_type == RDF_OBJECT_RESOURCE
	      && !strncmp (ns->namespace, rdf->object, len))
	    return FALSE;
	}

      list = list->next;
    }

  return TRUE;
}

/* Ritorna il prefisso associato ad una tripletta cercandolo nella lista dei
 * namespace. Se non lo trova, inserisce: */
gchar *
namespace_prefix (GList ** namespace, gchar * t)
{
  struct namespace_t *ns;
  gint max, i;
  GList *list = *namespace;

  max = -1;

  while (list)
    {
      ns = list->data;
      if (!strcmp (ns->namespace, t))
	return ns->prefix;

      /* Anche se non l'ho trovato mi segno il max valore trovato
       * nel sistema di autonumerazione: */
      if (ns->prefix[0] == '_' && (i = atoi (ns->prefix + 1)) && max < i)
	max = i;

      list = list->next;
    }

  /* Non l'ho trovato, alloco: */
  ns = (struct namespace_t *) g_malloc0 (sizeof (struct namespace_t));

  ns->namespace = g_strdup (t);

  if (!(ns->prefix = namespace_check_prefix (t)))
    ns->prefix = g_strdup_printf ("_%d", ++max);

  ns->visible = TRUE;

  /* appendo alla lista e comunico: */
  (*namespace) = g_list_append (*namespace, ns);

  return ns->prefix;
}

/* Dato una stringa cerca un possibile namespace e ritorna una stringa
 * da scrivere dentro ad un file rdf: */
gchar *
namespace_create (GList * l, gchar * t)
{
  struct namespace_t *r;
  gint len;

  while (l)
    {
      r = (struct namespace_t *) l->data;

      /* Se l'hai trovato: */
      len = strlen (r->namespace);
      if (!strncmp (r->namespace, t, len) && !strstr (t + len, "/")
	  && !strstr (t + len, "#") && !strstr (t + len, ":"))
	return g_strdup_printf ("%s:%s", r->prefix, t + len);

      l = l->next;
    }

  /* errore: */
  return g_strdup_printf ("_:%s", t);
}

/* Controlla se esiste un prefisso di un determinato namepsace nella lista
 * degli RDFS importati */
static gchar *
namespace_check_prefix (gchar * ns)
{
  GList *list;
  struct rdfs_t *r;

  list = rdfs_list;
  while (list)
    {
      r = (struct rdfs_t *) list->data;
      if (r->prefix && !strcmp (ns, r->resource))
	return g_strdup (r->prefix);

      list = list->next;
    }

  return NULL;
}

/* EOF */

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

static gchar *rdf_trim (gchar * tmp);
static void rdf_parse_this (struct rdf_t *data, gchar * uri);
static gboolean rdf_save_buffer_xml (GList * rdf, GList ** namespace,
				     gchar ** buffer);
static gboolean rdf_save_buffer_ntriples (GList * rdf, GList ** namespace,
					  gchar ** buffer);
static gboolean rdf_save_buffer_turtle (GList * rdf, GList ** namespace,
					gchar ** buffer);

static gchar *markup_ntriples (gchar * str);
static gchar *markup_ntriples_blank (gchar * ptr);

/* Questa funzione serve per ordinare liste di rdf con g_list_sort */
static gint
rdf_compare (struct rdf_t *a, struct rdf_t *b)
{
  return strcmp (a->subject, b->subject);
}

/* Questa funzione server per memorizzare dati di un nodo */
static void
rdf_node (gchar ** where, gint * type, gchar ** lang, gchar ** datatype,
	  librdf_node * node, GList ** id)
{
  librdf_uri *uri;

  /* Che tipologia e'? */
  switch (librdf_node_get_type (node))
    {
    case LIBRDF_NODE_TYPE_RESOURCE:
      uri = librdf_node_get_uri (node);

      if ((*where = (gchar *) librdf_uri_as_string (uri)))
	*where = g_strdup (*where);

      *type = RDF_OBJECT_RESOURCE;
      break;

    case LIBRDF_NODE_TYPE_LITERAL:
      if ((*where = (gchar *) librdf_node_get_literal_value (node)))
	*where = g_strdup (*where);

      if ((*lang = (gchar *) librdf_node_get_literal_value_language (node)))
	*lang = g_strdup (*lang);

      if ((uri = librdf_node_get_literal_value_datatype_uri (node)))
	{
	  if ((*datatype = (gchar *) librdf_uri_as_string (uri)))
	    *datatype = g_strdup (*datatype);
	}

      *type = RDF_OBJECT_LITERAL;
      break;

    case LIBRDF_NODE_TYPE_BLANK:
      {
	gint ok = 0;
	GList *t;

	if ((*where = (gchar *) librdf_node_get_blank_identifier (node)))
	  {
	    *where = g_strdup (*where);

	    t = *id;
	    ok = 0;

	    while (t)
	      {
		if (!strcmp ((gchar *) t->data, *where))
		  {
		    ok = 1;
		    break;
		  }
		t = t->next;
	      }

	    if (!ok)
	      *id = g_list_append (*id, g_strdup (*where));
	  }

	*type = RDF_OBJECT_BLANK;
      }

      break;

    default:
      break;
    }
}

/* Parsa un documento RDF e ritorna una lista di triplette: */
gboolean
rdf_parse (gchar * rdf, struct download_t *download, gboolean this_document,
	   GList ** ret, enum rdf_format_t *format, GString ** error)
{
  GString *str;
  gchar *buffer;
  gchar *err;
  gboolean should_empty = FALSE;

  str = NULL;

  if (download_uri (rdf, &buffer, download, error ? &err : NULL) == FALSE)
    {
      if (error)
	{
	  if (!*error)
	    *error = g_string_new (NULL);

	  if (err)
	    {
	      g_string_append_printf (*error, "%c%s\n", PARSER_CHAR_ERROR,
				      err);
	      g_free (err);
	    }
	}

      return FALSE;
    }

  /* Parso in formato XML passando una stringa interna per la gestione
   * degli errori: */
  if (rdf_parse_format
      (rdf, buffer, this_document, ret, RDF_FORMAT_XML,
       error ? &str : NULL) == TRUE)
    {
      if (format)
	*format = RDF_FORMAT_XML;

      /* Successo, quindi rimuovo eventuali messaggi di errore: */
      if (*ret)
	{
	  if (str)
	    {
	      g_string_free (str, TRUE);
	      *error = NULL;
	    }

	  g_free (buffer);
	  return TRUE;
	}
      else
	should_empty = TRUE;
    }

  /* Se mi sono ritornati dei messaggi di errore, metto una informazione
   * iniziale e metto tutto in error: */
  if (str)
    {
      gchar *t;

      if (!*error)
	*error = g_string_new (NULL);

      g_string_append_printf (*error, "%c%s\n", PARSER_CHAR_INIT,
			      _("RDF/XML parser:"));

      t = g_string_free (str, FALSE);
      g_string_append (*error, t);
      g_free (t);
    }

  str = NULL;

  /* Procedo con lo studio in formato ntrples: */
  if (rdf_parse_format
      (rdf, buffer, this_document, ret, RDF_FORMAT_NTRIPLES,
       error ? &str : NULL) == TRUE)
    {
      if (format)
	*format = RDF_FORMAT_NTRIPLES;

      /* Successo, quindi rimuovo eventuale stringa error: */
      if (*ret)
	{
	  if (error && *error)
	    {
	      g_string_free (*error, TRUE);
	      *error = NULL;
	    }

	  /* Rimuovo una eventuale stringa di errore: */
	  if (str)
	    g_string_free (str, TRUE);

	  g_free (buffer);
	  return TRUE;
	}
      else
	should_empty = TRUE;
    }

  /* Se mi sono ritornati dei messaggi di errore, metto una informazione
   * iniziale: */
  if (str)
    {
      gchar *t;

      if (!*error)
	*error = g_string_new (NULL);

      g_string_append_printf (*error, "%c%s\n", PARSER_CHAR_INIT,
			      _("RDF/N-TRIPLES parser:"));

      t = g_string_free (str, FALSE);
      g_string_append (*error, t);
      g_free (t);
    }

  str = NULL;

  /* Procedo con lo studio in formato Turtle: */
  if (rdf_parse_format
      (rdf, buffer, this_document, ret, RDF_FORMAT_TURTLE,
       error ? &str : NULL) == TRUE)
    {
      if (format)
	*format = RDF_FORMAT_TURTLE;

      /* Successo, quindi rimuovo eventuale stringa error: */
      if (*ret)
	{
	  if (error && *error)
	    {
	      g_string_free (*error, TRUE);
	      *error = NULL;
	    }

	  /* Rimuovo una eventuale stringa di errore: */
	  if (str)
	    g_string_free (str, TRUE);

	  g_free (buffer);
	  return TRUE;
	}
      else
	should_empty = TRUE;
    }

  /* Se mi sono ritornati dei messaggi di errore, metto una informazione
   * iniziale: */
  if (str)
    {
      gchar *t;

      if (!*error)
	*error = g_string_new (NULL);

      g_string_append_printf (*error, "%c%s\n", PARSER_CHAR_INIT,
			      _("RDF/TURTLE parser:"));

      t = g_string_free (str, FALSE);
      g_string_append (*error, t);
      g_free (t);
    }

  g_free (buffer);

  if (should_empty == TRUE)
    {
      if (error && *error)
	{
	  g_string_free (*error, TRUE);
	  *error = NULL;
	}
    }

  return should_empty;
}

gint
rdf_log (GString ** error, gchar * str, enum parser_char_t c)
{
  gint i, len, j;

  if (!error)
    return 1;

  if (!*error)
    *error = g_string_new (NULL);

  /*Splitto tutte le righe in modo da mettere il carattere di log
   * ad ogni inizio riga: */
  for (i = j = 0, len = strlen (str); i < len; i++)
    {
      if (str[i] == '\n')
	{
	  str[i] = 0;
	  g_string_append_printf (*error, "%c%s\n", c, str + j);
	  j = i + 1;
	}
    }

  g_string_append_printf (*error, "%c%s\n", c, str + j);

  return 1;
}

gint
rdf_error (void *dummy, const char *msg, va_list va)
{
  int ret;
  gchar *buf = g_strdup_vprintf (msg, va);
  ret = rdf_log ((GString **) dummy, buf, PARSER_CHAR_ERROR);
  g_free (buf);

  return ret;
}

gint
rdf_warning (void *dummy, const char *msg, va_list va)
{
  int ret;
  gchar *buf = g_strdup_vprintf (msg, va);
  ret = rdf_log ((GString **) dummy, buf, PARSER_CHAR_WARNING);
  g_free (buf);

  return ret;
}

/* Questa funzione ritorna il world: */
librdf_world *
rdf_world (void)
{
  static librdf_world *world = NULL;
  static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

  g_static_mutex_lock (&mutex);

  if (!world)
    {
      if (!(world = librdf_new_world ()))
	{
	  g_static_mutex_unlock (&mutex);
	  return NULL;
	}

      librdf_world_open (world);
    }

  librdf_world_set_error (world, NULL, NULL);
  librdf_world_set_warning (world, NULL, NULL);

  g_static_mutex_unlock (&mutex);
  return world;
}

/* Questa funzione parsa un documento in standard RDF/XML: */
gboolean
rdf_parse_format (gchar * rdf, gchar * buffer, gboolean this_document,
		  GList ** ret, enum rdf_format_t format, GString ** error)
{
  static librdf_world *world = NULL;
  librdf_storage *storage;
  librdf_parser *parser;
  librdf_model *model;
  librdf_stream *stream;
  librdf_uri *uri;
  GList *list;

  world = rdf_world ();

  librdf_world_set_error (world, error, rdf_error);
  librdf_world_set_warning (world, error, rdf_warning);

  if (!(uri = librdf_new_uri (world, (const unsigned char *) rdf)))
    return FALSE;

  if (!(storage = librdf_new_storage (world, "memory", NULL, NULL)))
    {
      librdf_free_uri (uri);
      return FALSE;
    }

  if (!(model = librdf_new_model (world, storage, NULL)))
    {
      librdf_free_storage (storage);
      librdf_free_uri (uri);
      return FALSE;
    }

  switch (format)
    {
    case RDF_FORMAT_NTRIPLES:
      parser = librdf_new_parser (world, "ntriples", NULL, NULL);
      break;

    case RDF_FORMAT_TURTLE:
      parser = librdf_new_parser (world, "turtle", NULL, NULL);
      break;

    default:
      parser = librdf_new_parser (world, "raptor", NULL, NULL);
      break;
    }

  if (!parser)
    {
      librdf_free_model (model);
      librdf_free_storage (storage);
      librdf_free_uri (uri);
      return FALSE;
    }

  if (librdf_parser_parse_string_into_model
      (parser, (unsigned char *) buffer, uri, model))
    {
      librdf_free_parser (parser);
      librdf_free_model (model);
      librdf_free_storage (storage);
      librdf_free_uri (uri);
      return FALSE;
    }

  if (!(stream = librdf_model_as_stream (model)))
    {
      librdf_free_parser (parser);
      librdf_free_model (model);
      librdf_free_storage (storage);
      librdf_free_uri (uri);
      return FALSE;
    }

  list = rdf_parse_stream (stream);

  if (this_document == TRUE)
    g_list_foreach (list, (GFunc) rdf_parse_this, rdf);

  /* Libero memoria: */
  librdf_free_stream (stream);
  librdf_free_parser (parser);
  librdf_free_model (model);
  librdf_free_storage (storage);
  librdf_free_uri (uri);

  /* Ordino e ritorno: */
  *ret = g_list_sort (list, (GCompareFunc) rdf_compare);
  return TRUE;
}

GList *
rdf_parse_stream (librdf_stream * stream)
{
  GList *list = NULL;
  GList *blank = NULL;

  while (!librdf_stream_end (stream))
    {
      librdf_node *node;
      gint fake;
      gchar *tmp;
      librdf_statement *statement;
      struct rdf_t *new;

      if (!(statement = librdf_stream_get_object (stream)))
	break;

      new = (struct rdf_t *) g_malloc0 (sizeof (struct rdf_t));

      node = librdf_statement_get_subject (statement);
      rdf_node (&new->subject, &fake, &new->lang, &new->datatype, node,
		&blank);
      if (!new->subject)
	{
	  rdf_free (new);
	  continue;
	}

      node = librdf_statement_get_predicate (statement);
      rdf_node (&new->predicate, &fake, &new->lang, &new->datatype, node,
		&blank);
      if (!new->predicate)
	{
	  rdf_free (new);
	  continue;
	}

      node = librdf_statement_get_object (statement);
      rdf_node (&new->object, &fake, &new->lang, &new->datatype, node,
		&blank);
      if (!new->object)
	{
	  rdf_free (new);
	  continue;
	}

      new->object_type = fake;

      tmp = new->subject;
      new->subject = rdf_trim (tmp);
      g_free (tmp);

      tmp = new->predicate;
      new->predicate = rdf_trim (tmp);
      g_free (tmp);

      tmp = new->object;
      new->object = rdf_trim (tmp);
      g_free (tmp);

      if (new->lang)
	{
	  tmp = new->lang;
	  new->lang = rdf_trim (tmp);
	  g_free (tmp);
	}

      if (new->datatype)
	{
	  tmp = new->datatype;
	  new->datatype = rdf_trim (tmp);
	  g_free (tmp);
	}

      list = g_list_append (list, new);

      librdf_stream_next (stream);
    }

  /* Qui sostituisco i blank con qualcosa di piu' bellino a vedersi: */
  if (blank)
    {
      struct rdf_t *rdf_t;
      GList *tmp, *t;
      gint id = 0;

      tmp = blank;
      while (tmp)
	{
	  t = list;
	  while (t)
	    {
	      rdf_t = (struct rdf_t *) t->data;
	      if (!strcmp (rdf_t->subject, (gchar *) tmp->data))
		{
		  g_free (rdf_t->subject);
		  rdf_t->subject = blanknode_create (id);
		}

	      if (rdf_t->object_type == RDF_OBJECT_BLANK
		  && !strcmp (rdf_t->object, (gchar *) tmp->data))
		{
		  g_free (rdf_t->object);
		  rdf_t->object = blanknode_create (id);
		}

	      t = t->next;
	    }

	  id++;
	  g_free (tmp->data);
	  tmp = tmp->next;
	}

      g_list_free (blank);
    }

  return list;
}

/* Questa funzione sostituisce tutti i soggetti uguali all'URI del documento
 * con this:document: */
static void
rdf_parse_this (struct rdf_t *data, gchar * uri)
{
  if (!strcmp (data->subject, uri))
    {
      g_free (data->subject);
      data->subject = g_strdup (THIS_DOCUMENT);
    }
}

/* Questo documento RDF e' salvato? */
gboolean
rdf_is_no_saved (struct editor_data_t *data)
{
  return data->changed;
}

/* Funzione di trim: */
static gchar *
rdf_trim (gchar * tmp)
{
  gchar *ret;
  gint i, j, q;
  gint size;

  i = 0;
  while (tmp[i] == ' ' || tmp[i] == '\t' || tmp[i] == '\r' || tmp[i] == '\n')
    tmp++;

  i = strlen (tmp);
  i--;

  while (tmp[i] == ' ' || tmp[i] == '\t' || tmp[i] == '\r' || tmp[i] == '\n')
    i--;

  tmp[i + 1] = 0;

  size = strlen (tmp);
  ret = g_malloc (sizeof (gchar) * (size + 1));

  for (q = i = j = 0; i < size; i++)
    {
      if (*(tmp + i) == 0xD)
	continue;

      if (*(tmp + i) == 0xA || *(tmp + i) == 0x9 || *(tmp + i) == 0x20)
	{
	  if (!q)
	    {
	      q = 1;
	      ret[j++] = *(tmp + i);
	    }
	}

      else
	{
	  q = 0;
	  ret[j++] = *(tmp + i);
	}
    }

  ret[j] = 0;
  tmp = g_strdup (ret);
  g_free (ret);

  return tmp;
}

/* Libero memoria: */
void
rdf_free (struct rdf_t *rdf)
{
  if (!rdf)
    return;

  if (rdf->subject)
    g_free (rdf->subject);

  if (rdf->predicate)
    g_free (rdf->predicate);

  if (rdf->object)
    g_free (rdf->object);

  if (rdf->lang)
    g_free (rdf->lang);

  if (rdf->datatype)
    g_free (rdf->datatype);

  g_free (rdf);
}

/* Questa funzione crea un dump di una tripletta: */
struct rdf_t *
rdf_copy (struct rdf_t *rdf)
{
  struct rdf_t *new;

  if (!rdf)
    return NULL;

  new = (struct rdf_t *) g_malloc0 (sizeof (struct rdf_t));

  if (rdf->subject)
    new->subject = g_strdup (rdf->subject);

  if (rdf->predicate)
    new->predicate = g_strdup (rdf->predicate);

  if (rdf->object)
    new->object = g_strdup (rdf->object);

  if (rdf->lang)
    new->lang = g_strdup (rdf->lang);

  if (rdf->datatype)
    new->datatype = g_strdup (rdf->datatype);

  new->object_type = rdf->object_type;
  return new;
}

/* Questa funzione salva in un file una serie di triplette: */
gboolean
rdf_save_file (gchar * file, GList * rdf, GList ** namespace,
	       enum rdf_format_t format)
{
  FILE *fl;
  gchar *buffer;

  if (rdf_save_buffer (rdf, namespace, format, &buffer) == FALSE)
    return FALSE;

  if (!(fl = g_fopen (file, "wb")))
    {
      g_free (buffer);
      return FALSE;
    }

  fprintf (fl, "%s", buffer);
  fclose (fl);

  g_free (buffer);
  return TRUE;
}

gboolean
rdf_save_buffer (GList * rdf, GList ** namespace, enum rdf_format_t format,
		 gchar ** buffer)
{
  switch (format)
    {
    case RDF_FORMAT_NTRIPLES:
      return rdf_save_buffer_ntriples (rdf, namespace, buffer);
    case RDF_FORMAT_TURTLE:
      return rdf_save_buffer_turtle (rdf, namespace, buffer);
    default:
      return rdf_save_buffer_xml (rdf, namespace, buffer);
    }
}

/* Questa funzione salva un RDF in formato XML: */
static gboolean
rdf_save_buffer_xml (GList * rdf, GList ** namespace, gchar ** buffer)
{
  gchar *str;
  gchar *prefix, *prev = NULL, *tmp;
  GString *ret;
  GList *l;

  ret = g_string_new ("<?xml version=\"1.0\"?>\n");

  prefix =
    markup (namespace_prefix
	    (namespace, "http://www.w3.org/1999/02/22-rdf-syntax-ns#"));

  g_string_append_printf (ret, "<%s:RDF\n", prefix);

  l = *namespace;
  while (l)
    {

      str = markup (((struct namespace_t *) l->data)->prefix);
      g_string_append_printf (ret, "   xmlns:%s=\"", str);
      g_free (str);

      str = markup (((struct namespace_t *) l->data)->namespace);
      g_string_append_printf (ret, "%s\"\n", str);
      g_free (str);

      l = l->next;
    }

  g_string_append_printf (ret, ">\n\n");

  if (rdf)
    {

      rdf = g_list_copy (rdf);
      rdf = g_list_sort (rdf, (GCompareFunc) rdf_compare);

      l = rdf;
      while (l)
	{
	  struct rdf_t *r = l->data;

	  if (!prev || strcmp (prev, r->subject))
	    {

	      if (prev)
		g_string_append_printf (ret, "  </%s:Description>\n\n",
					prefix);

	      prev = r->subject;
	      str = markup (prev);

	      if (!rdf_blank (rdf, prev))
		g_string_append_printf (ret, "  <%s:Description %s:about=\"",
					prefix, prefix);
	      else
		g_string_append_printf (ret, "  <%s:Description %s:nodeID=\"",
					prefix, prefix);

	      if (strcmp (str, THIS_DOCUMENT))
		g_string_append_printf (ret, "%s", str);

	      g_string_append_printf (ret, "\">\n");
	      g_free (str);
	    }

	  tmp = namespace_create (*namespace, r->predicate);

	  switch (r->object_type)
	    {
	    case RDF_OBJECT_LITERAL:
	      str = markup (tmp);
	      g_string_append_printf (ret, "    <%s", str);
	      g_free (str);

	      if (r->lang)
		{
		  str = markup (r->lang);
		  g_string_append_printf (ret, " xml:lang=\"%s\"", str);
		  g_free (str);
		}

	      if (r->datatype)
		{
		  str = markup (r->datatype);
		  g_string_append_printf (ret, " %s:datatype=\"%s\"", prefix,
					  str);
		  g_free (str);
		}

	      g_string_append_printf (ret, ">");

	      str = markup (r->object);
	      g_string_append_printf (ret, "%s", str);
	      g_free (str);

	      str = markup (tmp);
	      g_string_append_printf (ret, "</%s>\n", str);
	      g_free (str);

	      break;
	    case RDF_OBJECT_RESOURCE:
	      str = markup (tmp);
	      g_string_append_printf (ret, "    <%s ", str);
	      g_free (str);

	      str = markup (r->object);
	      g_string_append_printf (ret, "%s:resource=\"%s\" />\n", prefix,
				      str);
	      g_free (str);
	      break;

	    case RDF_OBJECT_BLANK:
	      str = markup (tmp);
	      g_string_append_printf (ret, "    <%s ", str);
	      g_free (str);

	      str = markup (r->object);
	      g_string_append_printf (ret, "%s:nodeID=\"%s\" />\n", prefix,
				      str);
	      g_free (str);
	      break;
	    }

	  g_free (tmp);

	  l = l->next;
	}

      g_list_free (rdf);
    }

  if (prev)
    g_string_append_printf (ret, "  </%s:Description>\n\n", prefix);

  g_string_append_printf (ret, "</%s:RDF>\n\n", prefix);

  g_free (prefix);
  *buffer = g_string_free (ret, FALSE);
  return TRUE;
}

/* Questa funzione salva un RDF in formato NTRIPLES: */
static gboolean
rdf_save_buffer_ntriples (GList * rdf, GList ** namespace, gchar ** buffer)
{
  gchar *str;
  GList *l;
  GString *ret;

  ret = g_string_new (NULL);

  l = rdf;

  /* Per ogni tripletta: */
  while (l)
    {
      struct rdf_t *r = l->data;

      /* Se e' questo documento: */
      if (!strcmp (r->subject, THIS_DOCUMENT))
	g_string_append_printf (ret, "<>");

      /* Se non e' un blank node: */
      else if (!rdf_blank (rdf, r->subject))
	{
	  str = markup_ntriples (r->subject);
	  g_string_append_printf (ret, "<%s>", str);
	  g_free (str);
	}

      /* Se e' un blank node: */
      else
	{
	  str = markup_ntriples_blank (r->subject);
	  g_string_append_printf (ret, "_:%s", str);
	  g_free (str);
	}

      /* Proprieta': */
      str = markup_ntriples (r->predicate);
      g_string_append_printf (ret, " <%s>", str);
      g_free (str);

      switch (r->object_type)
	{
	case RDF_OBJECT_LITERAL:
	  str = markup_ntriples (r->object);
	  g_string_append_printf (ret, " \"%s\"", str);
	  g_free (str);

	  if (r->lang)
	    {
	      str = markup_ntriples (r->lang);
	      g_string_append_printf (ret, "@%s", str);
	      g_free (str);
	    }

	  if (r->datatype)
	    {
	      str = markup_ntriples (r->datatype);
	      g_string_append_printf (ret, "^^<%s>", str);
	      g_free (str);
	    }

	  break;

	case RDF_OBJECT_RESOURCE:
	  str = markup_ntriples (r->object);
	  g_string_append_printf (ret, " <%s>", str);
	  g_free (str);
	  break;

	case RDF_OBJECT_BLANK:
	  str = markup_ntriples_blank (r->object);
	  g_string_append_printf (ret, " _:%s", str);
	  g_free (str);
	  break;
	}

      g_string_append_printf (ret, " .\n");

      l = l->next;
    }

  *buffer = g_string_free (ret, FALSE);
  return TRUE;
}

/* Questa funzione salva un RDF in formato TURTLE: */
static gboolean
rdf_save_buffer_turtle (GList * rdf, GList ** namespace, gchar ** buffer)
{
  gchar *str, *tmp;
  GList *l;
  GString *ret;

  namespace_prefix (namespace, "http://www.w3.org/1999/02/22-rdf-syntax-ns#");

  ret = g_string_new (NULL);

  l = *namespace;
  while (l)
    {

      str = markup_ntriples (((struct namespace_t *) l->data)->prefix);
      g_string_append_printf (ret, "@prefix %c%s: ", *str == '_' ? 'p' : *str,
			      str + 1);
      g_free (str);

      str = markup_ntriples (((struct namespace_t *) l->data)->namespace);
      g_string_append_printf (ret, "<%s> .\n", str);
      g_free (str);

      l = l->next;
    }

  g_string_append_printf (ret, "\n");

  if (rdf)
    {
      gchar *subject = NULL;

      rdf = g_list_copy (rdf);
      rdf = g_list_sort (rdf, (GCompareFunc) rdf_compare);

      l = rdf;

      /* Per ogni tripletta: */
      while (l)
	{
	  struct rdf_t *r = l->data;

	  if (!subject || strcmp (subject, r->subject))
	    {
	      if (subject)
		g_string_append_printf (ret, " .\n\n");

	      subject = r->subject;

	      /* Se e' questo documento: */
	      if (!strcmp (r->subject, THIS_DOCUMENT))
		g_string_append_printf (ret, "<>");

	      /* Se non e' un blank node: */
	      else if (!rdf_blank (rdf, r->subject))
		{
		  str = markup_ntriples (r->subject);
		  g_string_append_printf (ret, "<%s>", str);
		  g_free (str);
		}

	      /* Se e' un blank node: */
	      else
		{
		  str = markup_ntriples_blank (r->subject);
		  g_string_append_printf (ret, "_:%s", str);
		  g_free (str);
		}
	    }
	  else
	    g_string_append_printf (ret, " ;");

	  /* Proprieta': */
	  tmp = namespace_create (*namespace, r->predicate);
	  str = markup_ntriples (tmp);
	  g_string_append_printf (ret, "\n\t%c%s", *str == '_' ? 'p' : *str,
				  str + 1);
	  g_free (str);
	  g_free (tmp);

	  switch (r->object_type)
	    {
	    case RDF_OBJECT_LITERAL:
	      str = markup_ntriples (r->object);
	      g_string_append_printf (ret, " \"%s\"", str);
	      g_free (str);

	      if (r->lang)
		{
		  str = markup_ntriples (r->lang);
		  g_string_append_printf (ret, "@%s", str);
		  g_free (str);
		}

	      if (r->datatype)
		{
		  str = markup_ntriples (r->datatype);
		  g_string_append_printf (ret, "^^<%s>", str);
		  g_free (str);
		}

	      break;

	    case RDF_OBJECT_RESOURCE:
	      str = markup_ntriples (r->object);
	      g_string_append_printf (ret, " <%s>", str);
	      g_free (str);
	      break;

	    case RDF_OBJECT_BLANK:
	      str = markup_ntriples_blank (r->object);
	      g_string_append_printf (ret, " _:%s", str);
	      g_free (str);
	      break;
	    }

	  l = l->next;
	}

      if (subject)
	g_string_append_printf (ret, " .\n\n");

      g_list_free (rdf);
    }

  *buffer = g_string_free (ret, FALSE);
  return TRUE;
}

/* Questa funzione comunica se un nodo e' blank o no */
gboolean
rdf_blank (GList * rdf, gchar * t)
{
  struct rdf_t *r;

  while (rdf)
    {
      r = (struct rdf_t *) rdf->data;

      if (r->object_type == RDF_OBJECT_BLANK && !strcmp (r->object, t))
	return TRUE;
      rdf = rdf->next;
    }

  return FALSE;
}

/* Questa funzione comunica se un nodo e' una risorsa o no */
gboolean
rdf_resource (GList * rdf, gchar * t)
{
  struct rdf_t *r;

  while (rdf)
    {
      r = (struct rdf_t *) rdf->data;

      if (r->object_type == RDF_OBJECT_RESOURCE && !strcmp (r->object, t))
	return TRUE;
      rdf = rdf->next;
    }

  return FALSE;
}

struct data_t
{
  gchar *resource;
  struct download_t *download;

  GList *list;
  GList *nslist;

  enum rdf_format_t format;
  GString *error;

  gboolean this_document;
  gboolean ret;
  gint flag;
};

/* Questa funzione fa il lavoro sporco di parsing */
static void *
thread (void *what)
{
  GList *l, *ns;
  struct data_t *data = (struct data_t *) what;

  /* usa rdf_parse: */
  if (rdf_parse
      (data->resource, data->download, data->this_document, &l, &data->format,
       &data->error) == FALSE)
    {
      data->flag = 1;
      data->ret = FALSE;
      g_thread_exit (NULL);
      return NULL;
    }

  if (namespace_parse (l, &ns) == FALSE)
    {
      g_list_foreach (l, (GFunc) rdf_free, NULL);
      g_list_free (l);

      data->flag = 1;
      data->ret = FALSE;
      g_thread_exit (NULL);
      return NULL;
    }

  data->list = l;
  data->nslist = ns;
  data->flag = 1;
  data->ret = TRUE;

  g_thread_exit (NULL);
  return NULL;
}

/* Questa funzione viene lanciata quando il thread ha finito di compiere
 * le sue operazioni ma si e' premuto cancel. Libera la memoria: */
static void
rdf_thread_free (struct data_t *data)
{
  if (data->list)
    {
      g_list_foreach (data->list, (GFunc) rdf_free, NULL);
      g_list_free (data->list);
    }

  if (data->nslist)
    {
      g_list_foreach (data->nslist, (GFunc) namespace_free, NULL);
      g_list_free (data->nslist);
    }

  if (data->resource)
    g_free (data->resource);

  if (data->error)
    g_string_free (data->error, TRUE);

  if (data->download)
    download_free (data->download);

  g_free (data);
}

/* Se si preme cancel si aggiunge un elemento alla lista dei thread da
 * gestire: */
static void
rdf_parse_thread_abort (GtkWidget * widget, struct data_t *data)
{
  struct thread_t *new;
  gboolean *ret;

  widget = gtk_widget_get_toplevel (widget);
  ret = g_object_get_data (G_OBJECT (widget), "ret");

  new = (struct thread_t *) g_malloc0 (sizeof (struct thread_t));

  new->func = (GFunc) rdf_thread_free;
  new->data = data;
  new->flag = &data->flag;

  thread_child = g_list_append (thread_child, new);
  *ret = FALSE;
}

/* Quando si chiede di parse qualcosa si usa questa perche' e' multi threading
 * e non fa bloccar e l'interfaccia: */
gboolean
rdf_parse_thread (gchar * resource, struct download_t *download,
		  GList ** list, GList ** nslist, gboolean this_document,
		  enum rdf_format_t *format, GString ** error)
{
  GtkWidget *window;
  GtkWidget *pb;
  GThread *th;
  struct data_t *data;
  gboolean ret = TRUE;

  data = (struct data_t *) g_malloc0 (sizeof (struct data_t));

  data->resource = g_strdup (resource);
  data->download = download ? download_copy (download) : NULL;
  data->this_document = this_document;
  data->list = NULL;
  data->nslist = NULL;
  data->flag = 0;
  data->format = RDF_FORMAT_XML;

  /* Se e' richiesto una stringa di errore, ne setto una interna perche'
   * cosi' e' possibile abortire il thread impedendo al resto del programma
   * di liberare la stringa sotto al culo del thread di parsing: */
  data->error = error ? g_string_new (NULL) : NULL;

  window = dialog_wait (G_CALLBACK (rdf_parse_thread_abort), data, &pb);
  g_object_set_data (G_OBJECT (window), "ret", &ret);

  /* Attivo un processo separato ed apsetto: */
  th = g_thread_create ((GThreadFunc) thread, data, FALSE, NULL);

  while (!data->flag && ret)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pb));

      while (gtk_events_pending () && !data->flag && ret)
	gtk_main_iteration ();

      g_usleep (50000);
    }

  gtk_widget_destroy (window);

  /* Questo succede se si e' cliccato sul pulsante abort: */
  if (ret == FALSE)
    return FALSE;

  ret = data->ret;
  (*list) = data->list;
  (*nslist) = data->nslist;

  if (format)
    *format = data->format;

  if (error)
    {
      if (data->error->len > 0)
	*error = data->error;
      else
	g_string_free (data->error, TRUE);
    }

  if (data->download)
    download_free (data->download);

  g_free (data->resource);
  g_free (data);

  return ret;
}

/* Conversione dei caratteri particolari in una stringa compatibile ntriples: */
static gchar *
markup_ntriples (gchar * ptr)
{
  gunichar unichr;
  GString *ret;


  ret = g_string_new (NULL);

  while ((unichr = g_utf8_get_char (ptr)))
    {
      ptr = g_utf8_next_char (ptr);

      if (unichr >= 0x20 && unichr <= 0x7E)
	{
	  switch (unichr)
	    {
	    case '\\':
	      g_string_append_printf (ret, "\\\\");
	      break;

	    case '\"':
	      g_string_append_printf (ret, "\\\"");
	      break;

	    case '\n':
	      g_string_append_printf (ret, "\\\n");
	      break;

	    case '\r':
	      g_string_append_printf (ret, "\\\r");
	      break;

	    case '\t':
	      g_string_append_printf (ret, "\\\t");
	      break;

	    default:
	      g_string_append_printf (ret, "%c", unichr);
	      break;
	    }
	}

      else if (unichr < 0x10000)
	g_string_append_printf (ret, "\\u%.4X", unichr);

      else
	g_string_append_printf (ret, "\\U%.8X", unichr);
    }

  return g_string_free (ret, FALSE);
}

/* Conversione di una stringa in un qualcosa di compatibile NodeID in ntriples: */
static gchar *
markup_ntriples_blank (gchar * ptr)
{
  gunichar unichr;
  gboolean first = TRUE;
  gchar *ret;
  gint j = 0;

  ret = g_malloc (strlen (ptr) + 1);

  /* Conversione dei caratteri particolari: */
  while ((unichr = g_utf8_get_char (ptr)))
    {
      ptr = g_utf8_next_char (ptr);

      if ((unichr >= 'A' && unichr <= 'Z') || (unichr >= 'a' && unichr <= 'z')
	  || (first == FALSE ? (unichr >= '0' && unichr <= '9') : 0))
	ret[j++] = unichr;

      first = FALSE;

    }

  ret[j] = 0;
  return ret;
}

/* EOF */

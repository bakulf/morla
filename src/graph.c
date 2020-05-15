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

#ifndef G_OS_WIN32
#  include <signal.h>
#endif

static gchar *graph_file (struct editor_data_t *data, gchar * file);
static void graph_create_cb (GPid pid, gint status, gint * id);

/* Questa funzione escapa i nomi ritornandone altri */
static gchar *
graph_escape (gchar * str)
{
  gchar *ret;
  gint len = strlen (str);
  gint i, j;

  i = sizeof (gchar) * (len * 2) + 4;
  ret = (gchar *) g_malloc0 (i);

  for (i = j = 0; i < len; i++)
    {
      if (str[i] == '\"')
	{
	  ret[j++] = '\\';
	  ret[j++] = '\"';
	}

      else if (str[i] == '\n')
	{
	  ret[j++] = '.';
	  ret[j++] = '.';
	  ret[j++] = '.';
	  break;
	}

      else
	ret[j++] = str[i];
    }

  ret[j] = 0;
  str = g_strdup (ret);
  g_free (ret);

  return str;
}

/* Questa funzione e' la callback per la chiusura del programma di immagini.
 * Quando il programma viene chiuso, morla lancia questa funzione per la
 * rimozione del file immagine temporaneo: */
static void
graph_remove (GPid pid, gint status, gchar * file)
{
  if (!file)
    return;

#ifndef ENABLE_MACOSX
  g_remove (file);
#endif

  g_free (file);
}

/* Se si preme cancel creo un elemento per attendere la fine del processo: */
static void
graph_create_abort (GtkWidget * widget, gint * ret)
{
  *ret = 1;
}

/* Questa funzione mostra la finestra "Attendere prego" e lancia il processo
 * dentro uno spawn: */
static gchar *
graph_create (struct editor_data_t *data, gchar * file)
{
  GtkWidget *window;
  GtkWidget *pb;
  gboolean ret;
  gchar *tmp;
  GPid pid;
  gchar *array[6];
  gchar s[1024];
  gint flag;

  /* No grafici vuoti pls: */
  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return NULL;
    }

  if (!(tmp = graph_file (data, NULL)))
    {
      dialog_msg (_("Creating graph error!"));
      return NULL;
    }

  /* Se non mi hanno dato un nome, lo invento io: */
  if (!file)
    {
      file = editor_tmp_file ();
      g_snprintf (s, sizeof (s), "%s.%s", file, graph_format);
      g_free (file);
      file = g_strdup (s);
    }
  else
    file = g_strdup (file);

  /* Creo il comando per dot: */
  g_snprintf (s, sizeof (s), "-T%s", graph_format);

  array[0] = dot_cmd ();
  array[1] = s;
  array[2] = "-o";
  array[3] = file;
  array[4] = tmp;
  array[5] = NULL;

  if (g_spawn_async
      (NULL, array, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH |
       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
       &pid, NULL) == FALSE)
    {
      g_free (file);
      dialog_msg (_
		  ("Execution of external software error.\n"
		   "Check the softwares in your preferences!"));
      return NULL;
    }

  g_child_watch_add (pid, (GChildWatchFunc) graph_create_cb, &flag);

  window = dialog_wait (G_CALLBACK (graph_create_abort), &ret, &pb);

  /* Mi limito ad aspettare il termine del processo preoccupandomi 
   * solo di aggiornare l'interfaccia: */

  flag = ret = 0;

  while (!flag && !ret)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pb));

      while (gtk_events_pending () && !flag && !ret)
	gtk_main_iteration ();

      g_usleep (50000);
    }

  /* Distruggo la finestra di attesa: */
  gtk_widget_destroy (window);

  /* Questo succede se si e' cliccato sul pulsante abort: */
  if (ret)
    {
#ifndef G_OS_WIN32
      kill (pid, SIGTERM);
#endif
      g_spawn_close_pid (pid);
      g_free (file);
      return NULL;
    }

  /* Se non c'e' l'immagine comunico errore: */
  if (flag < 0)
    {
      dialog_msg (_("Creating graph error!"));
      g_free (file);
      return NULL;
    }

  /* Ritorno il nome dell'immagine: */
  return file;
}

/* Questa funzione viene lanciata al termine del comando dot */
static void
graph_create_cb (GPid pid, gint status, gint * id)
{
  if (status == 0)
    *id = 1;
  else
    *id = -1;
}

/* Questa funzione lancia graph: */
void
graph_run (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();

  if (data)
    graph (data);
}

/* Graph genera il grafico e lo mostra con un programma esterno: */
void
graph (struct editor_data_t *data)
{
  GList *list, *old;
  gchar **a;
  gchar *file;
  gint id, ok;

  /* Uso graph_create: */
  if (!(file = graph_create (data, NULL)))
    return;

  /* Splitto il comando per il viewer, creo l'array, lo riempo: */
  list = old = split (viewer_default);
  a = g_malloc (sizeof (gchar *) * (g_list_length (list) + 2));

  ok = id = 0;
  while (list)
    {
      if (!ok && !strcmp ((gchar *) list->data, "%s"))
	{
	  a[id++] = file;
	  ok++;
	}
      else
	a[id++] = list->data;

      list = list->next;
    }

  if (!ok)
    a[id++] = file;

  a[id] = NULL;

  /* Eseguo il comando passandogli la callback: */
  run (a, (GChildWatchFunc) graph_remove, file);

  /* Libero la memoria ma non il nome del file. Questo sara' liberato
   * dalla funzione di callback */
  g_list_foreach (old, (GFunc) g_free, NULL);
  g_list_free (old);
  g_free (a);
}

/* Salvataggio di un grafo: */
void
graph_save (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();
  gchar *file;
  gchar *t;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  /* Aggiorno la statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Save the graph in a file..."));

  /* Chiedo il nome: */
  if (!
      (file =
       file_chooser (_("Save your graph"), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_SAVE, NULL)))
    {
      statusbar_set (_("Save the graph: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Se esiste chiedo conferma per la sovrascrittura: */
  if (g_file_test (file, G_FILE_TEST_EXISTS) == TRUE
      && dialog_ask (_("The file exists. Overwrite?")) != GTK_RESPONSE_OK)
    {
      statusbar_set (_("Save the graph: aborted"));
      editor_statusbar_lock = WAIT;

      g_free (file);
      return;
    }

  /* Genero il grafo sul file: */
  if (!(t = graph_create (data, file)))
    {
      statusbar_set (_("Save the graph: aborted"));
      editor_statusbar_lock = WAIT;

      g_free (file);
      return;
    }

  /* Libero memoria, sblocco la statusbar e ritorno: */
  statusbar_set (_("Graph saved!"));
  editor_statusbar_lock = WAIT;
  g_free (file);
  g_free (t);
}

static gchar *
graph_file (struct editor_data_t *data, gchar * file)
{
  gchar *tmp;
  gchar *s, *p;
  GList *resources;
  FILE *fl;
  struct rdf_t *rdf;
  gchar *str;
  gint literal = 0;

  /* Creo l'elenco delle risorse: */
  resources = create_resources (data);

  /* Mi prendo un file temporaneo se non mi viene passato: */
  if (!file)
    tmp = editor_tmp_file ();
  else
    tmp = g_strdup (file);

  if (!(fl = g_fopen (tmp, "wb")))
    {
      g_free (tmp);
      return NULL;
    }

  /* Scrivo il sorgente per dot: */
  fprintf (fl, "digraph dotfile {\nrankdir=LR;\n\n");

  /* Per ogni risorsa: */
  while (resources)
    {
      str = graph_escape (resources->data);

      /* Se e' un blank node... */
      if (rdf_blank (data->rdf, resources->data))
	fprintf (fl,
		 "\"%s\" [color=\"%s\",fontcolor=\"%s\",style=\"%s\",fillcolor=\"%s\",fontname=\"%s\",fontsize=\"%d\",shape=\"%s\"];\n",
		 (gchar *) str, graph_blank_node.color,
		 graph_blank_node.fontcolor, graph_blank_node.style,
		 graph_blank_node.fillcolor, graph_blank_node.fontname,
		 graph_blank_node.fontsize, graph_blank_node.shape);

      /* Altrimenti se e' questo documento: */
      else if (!strcmp (str, THIS_DOCUMENT))
	fprintf (fl,
		 "\"%s\" [color=\"%s\",fontcolor=\"%s\",style=\"%s\",fillcolor=\"%s\",fontname=\"%s\",fontsize=\"%d\",shape=\"%s\"];\n",
		 data->uri ? data->uri : _("This Document"),
		 graph_resource_node.color, graph_resource_node.fontcolor,
		 graph_resource_node.style, graph_resource_node.fillcolor,
		 graph_resource_node.fontname, graph_resource_node.fontsize,
		 graph_resource_node.shape);

      /* Altrimenti: */
      else
	fprintf (fl,
		 "\"%s\" [color=\"%s\",fontcolor=\"%s\",style=\"%s\",fillcolor=\"%s\",fontname=\"%s\",fontsize=\"%d\",shape=\"%s\"];\n",
		 str, graph_resource_node.color,
		 graph_resource_node.fontcolor, graph_resource_node.style,
		 graph_resource_node.fillcolor, graph_resource_node.fontname,
		 graph_resource_node.fontsize, graph_resource_node.shape);

      g_free (str);

      resources = resources->next;
    }

  /* Parso le triplette: */
  resources = data->rdf;
  while (resources)
    {
      rdf = resources->data;

      if (!strcmp (rdf->subject, THIS_DOCUMENT))
	s = graph_escape (data->uri ? data->uri : _("This Document"));
      else
	s = graph_escape (rdf->subject);

      p = graph_escape (rdf->predicate);

      /* Se l'oggetto e' un letterale: */
      if (rdf->object_type == RDF_OBJECT_LITERAL)
	{
	  if (strlen (rdf->object) > 40)
	    {
	      gchar s[50];
	      strncpy (s, rdf->object, 40);
	      s[40] = 0;
	      strcat (s, "...");
	      str = graph_escape (s);
	    }
	  else
	    str = graph_escape (rdf->object);

	  fprintf (fl,
		   "\"literal_%d\" [label=\"%s\",color=\"%s\",fontcolor=\"%s\",style=\"%s\",fillcolor=\"%s\",fontname=\"%s\",fontsize=\"%d\",shape=\"%s\"];\n",
		   literal, (gchar *) str, graph_literal_node.color,
		   graph_literal_node.fontcolor, graph_literal_node.style,
		   graph_literal_node.fillcolor, graph_literal_node.fontname,
		   graph_literal_node.fontsize, graph_literal_node.shape);

	  fprintf (fl,
		   "\"%s\" -> \"literal_%d\"  [label=\"%s\",fontsize=\"%d\",fontname=\"%s\",color=\"%s\",fontcolor=\"%s\",style=\"%s\"];\n",
		   s, literal, p, graph_edge.fontsize, graph_edge.fontname,
		   graph_edge.color, graph_edge.fontcolor, graph_edge.style);

	  g_free (str);

	  literal++;
	}

      /* Altrimenti: */
      else
	{
	  str = graph_escape (rdf->object);

	  fprintf (fl,
		   "\"%s\" -> \"%s\"  [label=\"%s\",fontsize=\"%d\",fontname=\"%s\",color=\"%s\",fontcolor=\"%s\",style=\"%s\"];\n",
		   s, str, p, graph_edge.fontsize, graph_edge.fontname,
		   graph_edge.color, graph_edge.fontcolor, graph_edge.style);

	  g_free (str);
	}

      g_free (s);
      g_free (p);

      resources = resources->next;
    }

  /* Chiudo il sorgente e il file: */
  fprintf (fl, "\n}\n\n");
  fclose (fl);

  return tmp;
}

void
graph_graphviz (void)
{
  struct editor_data_t *data = editor_get_data ();
  gchar *file;
  gchar *t;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  /* Aggiorno la statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Save the graphviz source in a file..."));

  /* Chiedo il nome: */
  if (!
      (file =
       file_chooser (_("Save the graphviz source in a file..."),
		     GTK_SELECTION_SINGLE, GTK_FILE_CHOOSER_ACTION_SAVE,
		     NULL)))
    {
      statusbar_set (_("Save the graphviz source: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Se esiste chiedo conferma per la sovrascrittura: */
  if (g_file_test (file, G_FILE_TEST_EXISTS) == TRUE
      && dialog_ask (_("The file exists. Overwrite?")) != GTK_RESPONSE_OK)
    {
      statusbar_set (_("Save the graphviz source: aborted"));
      editor_statusbar_lock = WAIT;

      g_free (file);
      return;
    }

  /* Genero il sorgente del grafo sul file: */
  if (!(t = graph_file (data, file)))
    {
      statusbar_set (_("Save the graphviz source: aborted"));
      editor_statusbar_lock = WAIT;

      g_free (file);
      return;
    }

  /* Libero memoria, sblocco la statusbar e ritorno: */
  statusbar_set (_("The graphviz source saved!"));
  editor_statusbar_lock = WAIT;
  g_free (file);
  g_free (t);
}

/* EOF */

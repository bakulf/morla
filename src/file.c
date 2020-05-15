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

static void editor_close_real (struct editor_data_t *data);

gboolean automatic_extensions;

/* Questa funzione apre un file e comunica il risultato dell'operazione */
gboolean
editor_open_file (gchar * file, gchar * prev_uri)
{
  GList *rdf;
  GList *ns;
  struct editor_data_t *tmp;
  struct editor_data_t *data;
  enum rdf_format_t format;
  gchar s[1024];
  gchar *uri;
  GString *error = NULL;

  if (!file)
    return FALSE;

  /* Se e' una path assoluta, va bene: */
  if (g_path_is_absolute (file))
    file = g_strdup (file);

  /* ... altrimenti la devo creare: */
  else
    {
      gchar *cwd;
      cwd = g_get_current_dir ();
      file = g_strjoin (G_DIR_SEPARATOR_S, cwd, file, NULL);
      g_free (cwd);
    }

  /* Controllo se e' gia' aperto il file: */
  tmp = editor_data;
  while (tmp)
    {
      if (tmp->file && !strcmp (tmp->file, file))
	{
	  dialog_msg (_("This RDF Document is already opened!"));
	  g_free (file);
	  return FALSE;
	}
      tmp = tmp->next;
    }

  /* Creo da file ad uri e attivo le librdf: */
  uri = g_filename_to_uri (file, NULL, NULL);

  if (!uri
      || rdf_parse_thread (uri, NULL, &rdf, &ns, TRUE, &format,
			   &error) == FALSE)
    {
      gchar *str;

      if (error)
	str = g_string_free (error, FALSE);
      else
	str = NULL;

      g_snprintf (s, sizeof (s), _("Error opening the RDF file %s!"),
		  uri ? uri : file);
      dialog_msg_with_error (s, str);

      if (str)
	g_free (str);

      if (uri)
	g_free (uri);

      g_free (file);
      return FALSE;
    }

  /* Alloco memoria per questo nuovo elemento: */
  data = g_malloc0 (sizeof (struct editor_data_t));

  data->file = file;
  data->format = format;

  /* Setto l'uri a seconda esiste come argomento: */
  if (prev_uri)
    {
      data->uri = g_strdup (prev_uri);
      g_free (uri);
    }
  else
    data->uri = uri;

  data->rdf = rdf;
  data->namespace = ns;

  /* Creo l'interfaccia: */
  editor_new_maker (data);

  /* Aggiungo ai documenti aperti: */
  last_add (data->uri);

  return TRUE;
}

/* Questa funzione fa la stessa cosa di quella precedente, ma con gli uri: */
gboolean
editor_open_uri (gchar * resource, struct download_t * download)
{
  GList *list, *nslist;
  struct editor_data_t *data;
  enum rdf_format_t format;
  gchar s[1024];
  GString *error = NULL;

  /* Parso con librdf: */
  if (rdf_parse_thread
      (resource, download, &list, &nslist, TRUE, &format, &error) == FALSE)
    {
      gchar *str;

      if (error)
	str = g_string_free (error, FALSE);
      else
	str = NULL;

      g_snprintf (s, sizeof (s), _("Error opening the RDF resource '%s'"),
		  resource);
      dialog_msg_with_error (s, str);

      if (str)
	g_free (str);

      return FALSE;
    }

  /* Alloco memoria per questo nuovo elemento: */
  data = g_malloc0 (sizeof (struct editor_data_t));

  /* Non setto il file! Cosi' al primo salvataggio verra' richiesto
   * il file sui cui salvare */
  data->uri = g_strdup (resource);
  data->format = format;

  data->rdf = list;
  data->namespace = nslist;

  /* Creo l'interfaccia: */
  editor_new_maker (data);

  /* Aggiungo ai documenti aperti: */
  last_add (data->uri);

  return TRUE;
}

/* Funzione per l'apertura di un file lato interfaccia: */
void
editor_open (GtkWidget * w, gpointer dummy)
{
  gchar *file;
  gchar s[1024];

  /* Setto lo statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Open a new RDF file..."));

  /* Uso un filechooser: */
  if (!
      (file =
       file_chooser (_("Open a new RDF file..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_OPEN, NULL)))
    {
      statusbar_set (_("Open a new RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Apro il file: */
  if (!editor_open_file (file, NULL))
    {
      gchar *tmp;

      tmp = utf8 (file);
      g_snprintf (s, sizeof (s), _("The RDF file '%s' opened"), tmp);
      g_free (tmp);

      statusbar_set (s);
    }

  /* Sblocco lo statusbar e libero la memoria: */
  editor_statusbar_lock = WAIT;
  g_free (file);
}

/* Funzione per il salvataggio con nome: */
void
editor_save_as (GtkWidget * w, gpointer dummy)
{
  gchar *file;
  gchar s[1024];
  gchar *uri;
  struct editor_data_t *data = editor_get_data ();

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  /* Blocco lo statusbar e setto il titolo: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Save in a new RDF file..."));

  /* Uso filechooser in modalita' salvataggio: */
  if (!
      (file =
       file_chooser (_("Save the RDF file"), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_SAVE, &data->format)))
    {
      statusbar_set (_("Save the RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Estensione automatica: */
  if (automatic_extensions)
    {
      gint len;

      for (len = strlen (file) - 1; len >= 0; len--)
	if (file[len] == '.')
	  break;

      if (len <= 0)
	{
	  gchar *tmp;

	  switch (data->format)
	    {
	    case RDF_FORMAT_NTRIPLES:
	      tmp = g_strdup_printf ("%s.nt", file);
	      break;

	    case RDF_FORMAT_TURTLE:
	      tmp = g_strdup_printf ("%s.turtle", file);
	      break;

	    default:
	      tmp = g_strdup_printf ("%s.rdf", file);
	      break;

	    }

	  g_free (file);
	  file = tmp;
	}
    }
  /* Se esiste chiedo se devo continuare: */
  if (g_file_test (file, G_FILE_TEST_EXISTS) == TRUE
      && dialog_ask (_("The file exists. Overwrite?")) != GTK_RESPONSE_OK)
    {
      statusbar_set (_("Save the RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      g_free (file);

      return;
    }

  /* Lancio la funzione di salvataggio: */
  if (rdf_save_file (file, data->rdf, &data->namespace, data->format) ==
      FALSE)
    {
      dialog_msg (_("Error writing on file."));

      statusbar_set (_("Save the RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      g_free (file);

      return;
    }

  if (!(uri = g_filename_to_uri (file, NULL, NULL)))
    {
      g_snprintf (s, sizeof (s), _("Error writing on RDF file '%s'!"), file);
      dialog_msg (s);

      statusbar_set (_("Save the RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      g_free (file);

      return;
    }

  /* Annullo il cambiamento: */
  data->changed = FALSE;

  /* Libero la memoria del file e dell'URI per risettarli corretti: */
  if (data->file)
    g_free (data->file);

  data->file = g_strdup (file);

  if (data->uri)
    g_free (data->uri);

  data->uri = uri;

  /* Aggiorno la label: */
  gtk_label_set_text (GTK_LABEL (data->label), uri);

  /* Refreshio l'editor per eventuali this:document */
  editor_refresh (data);

  /* Aggiungo ai documenti aperti: */
  last_add (data->uri);

  /* Sbocco la statusbar e libero la memoria: */
  statusbar_set (_("RDF file saved!"));
  editor_statusbar_lock = WAIT;
  g_free (file);
}

/* Salvataggio standard: */
void
editor_save (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data = editor_get_data ();

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  /* Se esiste il salvataggio via modulo, lo eseguo: */
  if (data->module_save)
    {
      data->module_save->data->save_function (data->module_save->data,
					      data->rdf);
      return;
    }

  /* Se non esiste il nome del file, uso l'altra funzione: */
  if (!data->file)
    {
      editor_save_as (NULL, NULL);
      return;
    }

  /* Setto la statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("RDF file saving..."));

  /* Salvo: */
  if (rdf_save_file (data->file, data->rdf, &data->namespace, data->format) ==
      FALSE)
    {
      dialog_msg (_("Error writing on the RDF file."));

      statusbar_set (_("Save the RDF file: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Annullo il cambiamento: */
  data->changed = FALSE;

  /* Sblocco la statusbar e ritorno */
  statusbar_set (_("RDF file saved!"));
  editor_statusbar_lock = WAIT;
}

/* Chiusura di un editor: */
void
editor_close (GtkWidget * w, gpointer dummy)
{
  editor_close_real (editor_get_data ());
}

/* Chiusura di un editor: */
void
editor_close_selected (GtkWidget * w, struct editor_data_t *data)
{
  editor_close_real (data);
}

static void
editor_close_real (struct editor_data_t *data)
{
  struct editor_data_t *tmp;

  if (!data)
    return;

  /* Questo documento e' salvato? Chiedo, altrimenti fai: */
  if ((rdf_is_no_saved (data) == TRUE
       && dialog_ask (_("Exit without save?")) == GTK_RESPONSE_OK)
      || rdf_is_no_saved (data) == FALSE)
    {

      /* Statusbar: */
      editor_statusbar_lock = LOCK;
      statusbar_set (_("Closing..."));

      /* Tolgo dalla lista: */
      if (data == editor_data)
	editor_data = data->next;

      else
	{
	  tmp = editor_data;
	  while (tmp->next)
	    {
	      if (tmp->next == data)
		{
		  tmp->next = data->next;
		  break;
		}
	      tmp = tmp->next;
	    }
	}

      /* Tolgo dal notebook: */
      gtk_notebook_remove_page (GTK_NOTEBOOK (notebook),
				gtk_notebook_page_num (GTK_NOTEBOOK
						       (notebook),
						       data->maker));

      /* Libero la memoria di questo documento: */
      g_list_foreach (data->rdf, (GFunc) rdf_free, NULL);
      g_list_free (data->rdf);

      g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
      g_list_free (data->namespace);

      if (data->file)
	g_free (data->file);

      if (data->uri)
	g_free (data->uri);

      g_free (data);

      /* Sblocco la statusbar */
      statusbar_set (_("Closed"));
      editor_statusbar_lock = WAIT;
    }

  if (!editor_data)
    {
      gchar s[1024];
      g_snprintf (s, sizeof (s), _("Do you want exit from %s?"), PACKAGE);

      if (dialog_ask (s) == GTK_RESPONSE_OK)
	editor_quit (NULL, NULL, NULL);
      else
	editor_new (NULL, NULL);
    }
}

/* Funzione per un nuovo editor: */
void
editor_new (GtkWidget * w, gpointer dummy)
{
  struct editor_data_t *data;

  /* Alloco una struttura vuota e lancio new_maker */
  data = g_malloc0 (sizeof (struct editor_data_t));

  editor_new_maker (data);

  /* Setto la statusbar: */
  statusbar_set (_("new RDF editor created"));
  editor_statusbar_lock = WAIT;
}

/* Questa funzione ritorna un nome di un file temporaneo: */
gchar *
editor_tmp_file (void)
{
  gint id = 0;
  gchar s[1024];
  gchar i[1024];

  /* Provo l'esistenza di 9999 file */
  while (++id < 9999)
    {
      g_snprintf (s, sizeof (s), "%s%c%d", g_get_tmp_dir (), G_DIR_SEPARATOR,
		  id);
      g_snprintf (i, sizeof (i), "%s.jpg", s);

      if (g_file_test (s, G_FILE_TEST_EXISTS) == FALSE)
	{

	  g_snprintf (i, sizeof (i), "%s.png", s);
	  if (g_file_test (i, G_FILE_TEST_EXISTS) == TRUE)
	    continue;

	  g_snprintf (i, sizeof (i), "%s.jpg", s);
	  if (g_file_test (i, G_FILE_TEST_EXISTS) == TRUE)
	    continue;

	  g_snprintf (i, sizeof (i), "%s.gif", s);
	  if (g_file_test (i, G_FILE_TEST_EXISTS) == TRUE)
	    continue;

	  g_snprintf (i, sizeof (i), "%s.ps", s);
	  if (g_file_test (i, G_FILE_TEST_EXISTS) == TRUE)
	    continue;

	  g_snprintf (i, sizeof (i), "%s.svg", s);
	  if (g_file_test (i, G_FILE_TEST_EXISTS) == TRUE)
	    continue;

	  return g_strdup (s);
	}
    }

  return g_strdup_printf ("%s%cIMPOSSIBLE", g_get_tmp_dir (),
			  G_DIR_SEPARATOR);
}

/* EOF */

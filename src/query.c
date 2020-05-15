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

#define SPARQL_EXAMPLE \
  "SELECT ?x ?y ?z\n" \
  "WHERE { ?x ?y ?z . }\n"

#define RDQL_EXAMPLE \
  "SELECT ?x, ?y, ?z\n" \
  "WHERE (?x ?y ?z)\n"

gboolean highlighting_active = TRUE;
GList *highlighting_tags = NULL;

static gchar *query_sparql_last = NULL;
static gchar *query_rdql_last = NULL;

static GtkWidget *query_frame = NULL;
static GString *rasqal_errors = NULL;

/* Struttura per il thread di parsing: */
struct data_t
{
  gboolean flag;
  gboolean ret;
  gchar *buf;

  librdf_world *world;
  librdf_query_results *results;
  librdf_query *query;
  librdf_model *model;
  librdf_storage *storage;

  struct editor_data_t *data;

  gchar *name;
};

static void query (gchar * format);
static void query_open (GtkWidget * w, GtkWidget * textivew);
static void query_save (GtkWidget * w, GtkTextBuffer * buffer);
static void query_save_as (GtkWidget * w, GtkTextBuffer * buffer);
static void query_results_open (GtkWidget * w, GtkWidget * scrolledwindow);
static void query_results_save_as (GtkWidget * w, GtkWidget * scrolledwindow);
static void query_destroy (GtkTextBuffer * buffer);
static gboolean query_get_change (GtkTextBuffer * buffer);
static void query_set_change (GtkTextBuffer * buffer, gboolean);
static void query_changed (GtkWidget * textview, gpointer dummy);
static void query_quit (GtkWidget * w, GtkWidget * dialog);
static void query_cut (GtkWidget * w, GtkTextBuffer * buffer);
static void query_copy (GtkWidget * w, GtkTextBuffer * buffer);
static void query_paste (GtkWidget * w, GtkTextBuffer * buffer);
static void query_exec (GtkTextBuffer * buffer, GtkWidget * scrolledwindow,
			GtkWidget * menuitem, gchar * name,
			struct editor_data_t *);
static gboolean query_exec_real (GtkTextBuffer * buffer,
				 GtkWidget * scrolledwindow,
				 GtkWidget * menuitem, librdf_world * world,
				 gchar * name, struct editor_data_t *);
static gpointer query_exec_thread (struct data_t *str);
static void query_exec_thread_abort (GtkWidget * widget, struct data_t *data);
static void query_exec_thread_free (struct data_t *data);
static GtkWidget *query_exec_texttree (GtkWidget * scrolledwindow,
				       gboolean tree, gboolean visible);
static void query_exec_show (GtkWidget * scrolledwindow, GtkWidget * menuitem,
			     librdf_query_results * results);
static void query_exec_show_bindings (GtkWidget * scrolledwindow,
				      librdf_query_results * results);
static void query_exec_show_boolean (GtkWidget * scrolledwindow,
				     librdf_query_results * results);
static void query_exec_show_graph (GtkWidget * scrolledwindow,
				   librdf_query_results * results);
static gboolean query_results_save (gchar * file, GtkWidget * scrolledwindow);
static void query_insert (GtkWidget * w, GtkWidget * textview);
static void query_rdfs (GtkWidget * w, GtkWidget * textview);
static void query_save_session (gchar * buf, gchar * name);
static void query_textview_map (GtkWidget * text);
static gboolean query_prepare (GtkTextBuffer * buffer);
static void query_show_errors (GtkWidget * widget, gpointer dummy);

/* Apertura della quary sparql: */
void
query_sparql (void)
{
  query ("SPARQL");
}

/* Rsql */
void
query_rsql (void)
{
  query ("RDQL");
}

/* Apertura finestra di query: */
static void
query (gchar * format)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *scrolledwindow;
  GtkWidget *scrolledresult;
  GtkWidget *textview;
  GtkWidget *frame;
  GtkWidget *box;
  GtkWidget *paned;
  GtkWidget *toolbar;
  GtkWidget *sep;
  GtkWidget *combo;
  GtkWidget *cbox;
  GtkWidget *hbox;

  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkWidget *image;
  GtkWidget *menubar;
  GtkWidget *menuitem;
  GtkWidget *menumenu;
  GtkWidget *menuresult;
  GtkAccelGroup *accel;

  gchar s[1024];
  GList *list;

  guint tid;

  struct editor_data_t *data = editor_get_data ();
  GList *resources;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  /* Check della versione di rasqal e messaggio informativo: */
  check_rasqal ();

  /* Setto lo statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Query a RDF file..."));

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Query a RDF document"));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 640, 480);

  textview = textview_new (G_CALLBACK (query_changed), NULL);
  query_textview_map (textview);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

  g_object_set_data (G_OBJECT (buffer), "format", format);
  tid = g_timeout_add (500, (GSourceFunc) query_prepare, buffer);

  resources = create_resources (data);
  resources = create_resources_by_rdfs (resources);
  g_object_set_data (G_OBJECT (buffer), "resources", resources);

  scrolledresult = gtk_scrolled_window_new (NULL, NULL);

  accel = gtk_accel_group_new ();
  gtk_window_add_accel_group (GTK_WINDOW (window), accel);

  box = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), box, TRUE, TRUE,
		      0);

  menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (box), menubar, FALSE, FALSE, 0);

  menuitem = gtk_menu_item_new_with_mnemonic (_("_File"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-quit", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_quit), window);

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Query"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-open", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_open), textview);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-save", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_save), buffer);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-save-as", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_save_as), buffer);

  menuresult = menuitem = gtk_menu_item_new_with_mnemonic (_("_Results"));
  gtk_widget_set_sensitive (menuitem, FALSE);
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Open with Morla"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_results_open), scrolledresult);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-save-as", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_results_save_as), scrolledresult);

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Find resource"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_insert), textview);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-cut", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_cut), buffer);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-copy", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_copy), buffer);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-paste", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_paste), buffer);

  /* Toolbar: */
  toolbar = gtk_toolbar_new ();
  gtk_box_pack_start (GTK_BOX (box), toolbar, FALSE, FALSE, 0);
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-open");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_open), textview);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-save");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_save), buffer);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-save-as");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_save_as), buffer);

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-cut");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_cut), buffer);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-copy");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_copy), buffer);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-paste");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_paste), buffer);

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), button);

  cbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (button), cbox);

  gtk_box_pack_start (GTK_BOX (cbox), gtk_event_box_new (), TRUE, TRUE, 0);

  hbox = gtk_hbox_new (0, 0);
  gtk_box_pack_start (GTK_BOX (cbox), hbox, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (cbox), gtk_event_box_new (), TRUE, TRUE, 0);

  combo = gtk_combo_box_new_text ();

  for (list = rdfs_list; list; list = list->next)
    {
      struct rdfs_t *rdfs = (struct rdfs_t *) list->data;
      gtk_combo_box_append_text (GTK_COMBO_BOX (combo), rdfs->prefix);
    }

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
  gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);

  button = gtk_button_new_with_label (_("Insert RDFS"));
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (button), "combo", combo);
  g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (query_rdfs),
		    textview);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_LARGE_TOOLBAR);

  button = (GtkWidget *) gtk_tool_button_new (image, _("Find Resources"));
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_insert), textview);

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  /* Body */
  paned = gtk_vpaned_new ();
  gtk_box_pack_start (GTK_BOX (box), paned, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  gtk_paned_pack1 (GTK_PANED (paned), frame, TRUE, TRUE);

  g_snprintf (s, sizeof (s), "%s %s", format, _("Text"));
  button = gtk_button_new_with_label (s);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_frame_set_label_widget (GTK_FRAME (frame), button);

  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (query_show_errors), NULL);

  if ((list = gtk_container_get_children (GTK_CONTAINER (button))))
    {
      query_frame = list->data;
      g_list_free (list);
    }

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  gtk_widget_show (textview);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), textview);
  gtk_widget_set_size_request (textview, -1, 200);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), TRUE);

  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

  if (!strcasecmp (format, "sparql"))
    textview_insert (textview, &iter,
		     query_sparql_last ? query_sparql_last : SPARQL_EXAMPLE,
		     -1);
  else if (!strcasecmp (format, "rdql"))
    textview_insert (textview, &iter,
		     query_rdql_last ? query_rdql_last : RDQL_EXAMPLE, -1);

  frame = gtk_frame_new (_("Results"));
  gtk_paned_pack2 (GTK_PANED (paned), frame, TRUE, TRUE);

  gtk_widget_show (scrolledresult);
  gtk_container_add (GTK_CONTAINER (frame), scrolledresult);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledresult),
				       GTK_SHADOW_IN);
  gtk_widget_set_sensitive (scrolledresult, FALSE);
  gtk_widget_set_size_request (scrolledresult, -1, 200);

  query_exec_texttree (scrolledresult, FALSE, FALSE);

  button = gtk_button_new_from_stock ("gtk-execute");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_NO);
  gtk_widget_set_can_default(button, TRUE);

  query_set_change (buffer, FALSE);
  gtk_widget_show_all (window);

  while (1)
    {
      /* Esecuzione: */
      if (gtk_dialog_run (GTK_DIALOG (window)) == GTK_RESPONSE_OK)
	query_exec (buffer, scrolledresult, menuresult, format, data);

      else if (g_object_get_data (G_OBJECT (buffer), "file")
	       && query_get_change (buffer) == FALSE)
	continue;

      else
	break;
    }

  g_source_remove (tid);

  gtk_widget_destroy (window);

  query_destroy (buffer);

  statusbar_set (_("Querying done!"));
  editor_statusbar_lock = WAIT;
}

/* Apertura di un file: */
static void
query_open (GtkWidget * w, GtkWidget * textview)
{
  gchar *file;
  gchar *contents;
  GError *error = NULL;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

  /* Salvare i cambiamenti? */
  if (query_get_change (buffer) == FALSE)
    return;

  if (!
      (file =
       file_chooser (_("Open a QUERY file..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_OPEN, NULL)))
    return;

  /* Butto nel text view: */
  if (g_file_get_contents (file, &contents, NULL, &error) == TRUE)
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (buffer, &start);
      gtk_text_buffer_get_end_iter (buffer, &end);
      gtk_text_buffer_delete (buffer, &start, &end);

      gtk_text_buffer_get_start_iter (buffer, &start);
      textview_insert (textview, &start, contents, -1);

      g_free (contents);

      query_set_change (buffer, FALSE);
    }

  else if (error)
    {
      dialog_msg (error->message);
      g_error_free (error);
    }

  g_free (file);
}

/* Salvataggio della query: */
static void
query_save (GtkWidget * w, GtkTextBuffer * buffer)
{
  gchar *file;
  GtkTextIter start, end;
  gchar *buf;
  FILE *fl;

  if (!(file = g_object_get_data (G_OBJECT (buffer), "file")))
    {
      query_save_as (w, buffer);
      return;
    }

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);

  buf = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);

  if (!(fl = g_fopen (file, "wb")))
    {
      gchar s[1024];
      snprintf (s, sizeof (s), _("Error creating the file '%s'!"), file);
      dialog_msg (s);

      if (buf)
	g_free (buf);
      return;
    }

  fprintf (fl, "%s", buf ? buf : "");
  fclose (fl);

  if (buf)
    g_free (buf);

  query_set_change (buffer, FALSE);
}

/* Salva come: usa la funzione sopra: */
static void
query_save_as (GtkWidget * w, GtkTextBuffer * buffer)
{
  gchar *file;
  gchar *old_file;

  if (!
      (file =
       file_chooser (_("Save a QUERY in a file..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_SAVE, NULL)))
    return;

  if (g_file_test (file, G_FILE_TEST_EXISTS)
      && dialog_ask (_("The file exists. Overwrite?")) != GTK_RESPONSE_OK)
    {
      g_free (file);
      return;
    }

  if ((old_file = g_object_get_data (G_OBJECT (buffer), "file")))
    g_free (old_file);

  g_object_set_data (G_OBJECT (buffer), "file", file);
  query_save (w, buffer);
}

/* La distruzione della finestra richiede la rimozione del nome del
 * file e del buffer: */
static void
query_destroy (GtkTextBuffer * buffer)
{
  gchar *file;
  GList *list;

  if ((file = g_object_get_data (G_OBJECT (buffer), "file")))
    g_free (file);

  if ((list = g_object_get_data (G_OBJECT (buffer), "resources")))
    g_list_free (list);

  g_object_unref (buffer);
}

/* Funzione per sattare il cambiamento dentro al buffer: */
static void
query_set_change (GtkTextBuffer * buffer, gboolean value)
{
  g_object_set_data (G_OBJECT (buffer), "changed", (gpointer) value);
}

/* Se c'e' cambiamento, chiede il saltaggio, altrimenti ritorna lo 
 * status: */
static gboolean
query_get_change (GtkTextBuffer * buffer)
{
  if (g_object_get_data (G_OBJECT (buffer), "changed") == FALSE)
    return TRUE;

  if (dialog_ask (_("Close without save?")) == GTK_RESPONSE_OK)
    return TRUE;

  return FALSE;
}

/* Funzione di callback per il cambio di stato: */
static void
query_changed (GtkWidget * textview, gpointer dummy)
{
  GtkTextBuffer *buffer;
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  query_set_change (buffer, TRUE);
}

/* Chiusura: */
static void
query_quit (GtkWidget * w, GtkWidget * dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
}

/* Funzioni da menu edit: */
static void
query_cut (GtkWidget * w, GtkTextBuffer * buffer)
{
  gtk_text_buffer_cut_clipboard (buffer, gtk_clipboard_get (GDK_NONE), TRUE);
}

static void
query_copy (GtkWidget * w, GtkTextBuffer * buffer)
{
  gtk_text_buffer_copy_clipboard (buffer, gtk_clipboard_get (GDK_NONE));
}

static void
query_paste (GtkWidget * w, GtkTextBuffer * buffer)
{
  gtk_text_buffer_paste_clipboard (buffer, gtk_clipboard_get (GDK_NONE), NULL,
				   TRUE);
}

/* Esecuzione della query: */
static void
query_exec (GtkTextBuffer * buffer, GtkWidget * scrolledwindow,
	    GtkWidget * menuitem, gchar * name, struct editor_data_t *data)
{
  librdf_world *world;
  GString *error = NULL;

  /* Prelevo il mondo e setto le funzioni di errore: */
  if (!(world = rdf_world ()))
    {
      dialog_msg (_("Error creating the enviroment for your query"));
      return;
    }

  librdf_world_set_error (world, &error, rdf_error);
  librdf_world_set_warning (world, &error, rdf_warning);

  /* Eseguo e comunico l'errore: */
  if (query_exec_real (buffer, scrolledwindow, menuitem, world, name, data) ==
      FALSE)
    {
      gchar *str;

      if (error)
	{
	  str = g_string_free (error, FALSE);
	  error = NULL;
	}
      else
	str = NULL;

      dialog_msg_with_error (_("Error executing the query!"), str);

      if (str)
	g_free (str);
    }

  if (error)
    g_string_free (error, TRUE);
}

/* Questa funzione controlla il thread separato per la gestione della
 * query, infine mostra i risultati: */
static gboolean
query_exec_real (GtkTextBuffer * buffer, GtkWidget * scrolledwindow,
		 GtkWidget * menuitem, librdf_world * world, gchar * name,
		 struct editor_data_t *editor_data)
{
  GtkWidget *window;

  GtkWidget *pb;
  GThread *th;
  struct data_t *data;
  gboolean ret = 0;

  GtkTextIter start, end;

  /* Creo la struttura per il thread: */
  data = (struct data_t *) g_malloc0 (sizeof (struct data_t));

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);

  data->buf = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
  data->world = world;
  data->name = name;
  data->data = editor_data;

  query_save_session (data->buf, name);

  /* Finestra di waiting con gestione dell'abort: */
  window = dialog_wait (G_CALLBACK (query_exec_thread_abort), data, &pb);
  g_object_set_data (G_OBJECT (window), "ret", &ret);

  /* Nascondo il dialog perche' altrimenti la finestra di wait sta sotto: */
  gtk_widget_hide (gtk_widget_get_parent (scrolledwindow));

  th = g_thread_create ((GThreadFunc) query_exec_thread, data, FALSE, NULL);

  while (!data->flag && !ret)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pb));

      while (gtk_events_pending () && !data->flag && !ret)
	gtk_main_iteration ();

      g_usleep (50000);
    }

  gtk_widget_destroy (window);
  gtk_widget_show (gtk_widget_get_parent (scrolledwindow));

  if (ret)
    return FALSE;

  if (data->buf)
    g_free (data->buf);

  /* Mostro i risultati se questi esistono: */
  if (data->ret == TRUE && data->results)
    {
      query_exec_show (scrolledwindow, menuitem, data->results);

      /* Rimuovo i dati: */
      librdf_free_query_results (data->results);
      librdf_free_query (data->query);
      librdf_free_model (data->model);
      librdf_free_storage (data->storage);
    }
  else
    query_exec_texttree (scrolledwindow, FALSE, FALSE);

  ret = data->ret;
  g_free (data);

  return ret;
}

/* Esecuzione del thread separato: */
static gpointer
query_exec_thread (struct data_t *str)
{
  GList *list;

  /* Creo un storage e un model. Infine metto tutto dentro al model: */
  if (!(str->storage = librdf_new_storage (str->world, "memory", NULL, NULL)))
    {
      str->ret = FALSE;
      str->flag = 1;
      g_thread_exit (NULL);
      return NULL;
    }

  if (!(str->model = librdf_new_model (str->world, str->storage, NULL)))
    {
      librdf_free_storage (str->storage);

      str->ret = FALSE;
      str->flag = 1;
      g_thread_exit (NULL);
      return NULL;
    }

  for (list = str->data->rdf; list; list = list->next)
    {
      struct rdf_t *rdf = list->data;
      librdf_node *node;
      librdf_statement *statement;

      statement = librdf_new_statement (str->world);

      if (rdf_blank (str->data->rdf, rdf->subject) == FALSE)
	node =
	  librdf_new_node_from_uri_string (str->world,
					   (const unsigned char *) rdf->
					   subject);
      else
	node =
	  librdf_new_node_from_blank_identifier (str->world,
						 (const unsigned char *) rdf->
						 subject);

      librdf_statement_set_subject (statement, node);

      node =
	librdf_new_node_from_uri_string (str->world,
					 (const unsigned char *) rdf->
					 predicate);
      librdf_statement_set_predicate (statement, node);

      switch (rdf->object_type)
	{
	case RDF_OBJECT_LITERAL:
	  if (!rdf->datatype)
	    node =
	      librdf_new_node_from_literal (str->world,
					    (const unsigned char *) rdf->
					    object, rdf->lang, 0);

	  else
	    {
	      librdf_uri *uri = librdf_new_uri (str->world,
						(const unsigned char *) rdf->
						datatype);
	      node =
		librdf_new_node_from_typed_literal (str->world,
						    (const unsigned char *)
						    rdf->object, rdf->lang,
						    uri);
	    }

	  break;

	case RDF_OBJECT_BLANK:
	  node =
	    librdf_new_node_from_blank_identifier (str->world,
						   (const unsigned char *)
						   rdf->object);
	  break;

	case RDF_OBJECT_RESOURCE:
	default:
	  node =
	    librdf_new_node_from_uri_string (str->world,
					     (const unsigned char *) rdf->
					     object);
	  break;
	}

      librdf_statement_set_object (statement, node);
      librdf_model_add_statement (str->model, statement);
    }

  /* Parso la query e eseguo: */
  if (!
      (str->query =
       librdf_new_query (str->world,
			 !strcmp (str->name, "SPARQL") ? "sparql" : "rdql",
			 NULL, (unsigned char *) str->buf, NULL)))
    {
      librdf_free_model (str->model);
      librdf_free_storage (str->storage);

      str->ret = FALSE;
      str->flag = 1;
      g_thread_exit (NULL);
      return NULL;
    }

  if (!(str->results = librdf_model_query_execute (str->model, str->query)))
    {
      librdf_free_query (str->query);
      librdf_free_model (str->model);
      librdf_free_storage (str->storage);

      str->ret = FALSE;
      str->flag = 1;
      g_thread_exit (NULL);
      return NULL;
    }

  str->ret = TRUE;
  str->flag = 1;
  g_thread_exit (NULL);
  return NULL;
}

/* Questa funzione imposta la funzione di abort all'interno del gestore del
 * thread: */
static void
query_exec_thread_abort (GtkWidget * widget, struct data_t *data)
{
  struct thread_t *new;
  gboolean *ret;

  widget = gtk_widget_get_toplevel (widget);
  ret = g_object_get_data (G_OBJECT (widget), "ret");

  new = (struct thread_t *) g_malloc0 (sizeof (struct thread_t));

  new->func = (GFunc) query_exec_thread_free;
  new->data = data;
  new->flag = &data->flag;

  thread_child = g_list_append (thread_child, new);
  *ret = 1;
}

/* Questa funzione libera la memoria alla chiusura del thread: */
static void
query_exec_thread_free (struct data_t *data)
{
  if (!data)
    return;

  if (data->buf)
    g_free (data->buf);

  if (data->results)
    {
      librdf_free_query_results (data->results);
      librdf_free_query (data->query);
      librdf_free_model (data->model);
      librdf_free_storage (data->storage);
    }

  g_free (data);
}

/* Creazione dell'oggetto giusto per mostrare i dati (tree o text view): */
static GtkWidget *
query_exec_texttree (GtkWidget * scrolledwindow, gboolean tree,
		     gboolean visible)
{
  GtkWidget *widget;

  gtk_widget_set_sensitive (scrolledwindow, visible);
  gtk_container_foreach (GTK_CONTAINER (scrolledwindow),
			 (GtkCallback) gtk_widget_destroy, NULL);

  if (tree == FALSE)
    {
      widget = gtk_text_view_new ();
      gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (widget), FALSE);
    }
  else
    widget = gtk_tree_view_new ();

  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), widget);

  return widget;
}

/* Mostrare i risultati lo si fa con funzioni diverse a seconda della
 * tipologia di risultato: */
static void
query_exec_show (GtkWidget * scrolledwindow, GtkWidget * menuitem,
		 librdf_query_results * results)
{
  if (librdf_query_results_is_bindings (results))
    {
      query_exec_show_bindings (scrolledwindow, results);
      gtk_widget_set_sensitive (menuitem, FALSE);
    }

  else if (librdf_query_results_is_boolean (results))
    {
      query_exec_show_boolean (scrolledwindow, results);
      gtk_widget_set_sensitive (menuitem, FALSE);
    }

  else if (librdf_query_results_is_graph (results))
    {
      query_exec_show_graph (scrolledwindow, results);
      gtk_widget_set_sensitive (menuitem, TRUE);
    }

  else
    {
      query_exec_texttree (scrolledwindow, FALSE, TRUE);
      gtk_widget_set_sensitive (menuitem, FALSE);
    }
}

/* Tipologia bindings: */
static void
query_exec_show_bindings (GtkWidget * scrolledwindow,
			  librdf_query_results * results)
{
  GtkWidget *w;
  GtkListStore *store = NULL;
  GtkTreeIter iter;
  gboolean init = FALSE;

  w = query_exec_texttree (scrolledwindow, TRUE, TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (w), TRUE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (w), TRUE);

  while (!librdf_query_results_finished (results))
    {
      gint len = librdf_query_results_get_bindings_count (results);
      gint i;

      /* Inizializzo lo store per il tree view: */
      if (init == FALSE)
	{
	  GType types[len];
	  init = TRUE;

	  for (i = 0; i < len; i++)
	    types[i] = G_TYPE_STRING;

	  store = gtk_list_store_newv (len, types);
	  gtk_tree_view_set_model (GTK_TREE_VIEW (w), GTK_TREE_MODEL (store));

	  /* Creazione delle colonne: */
	  for (i = 0; i < len; i++)
	    {
	      GtkCellRenderer *renderer;
	      GtkTreeViewColumn *column;

	      renderer = gtk_cell_renderer_text_new ();
	      column =
		gtk_tree_view_column_new_with_attributes ((gchar *)
							  librdf_query_results_get_binding_name
							  (results, i),
							  renderer, "text", i,
							  NULL);
	      gtk_tree_view_append_column (GTK_TREE_VIEW (w), column);
	      gtk_tree_view_column_set_resizable (column, TRUE);
	    }
	}

      gtk_list_store_append (store, &iter);

      /* Inserimento dei risultati: */
      for (i = 0; i < len; i++)
	{
	  librdf_node *value;
	  gchar *str;

	  if ((value = librdf_query_results_get_binding_value (results, i)))
	    str = ntriples (value);
	  else
	    str = NULL;

	  gtk_list_store_set (store, &iter, i, str, -1);

	  free (str);

	  if (value)
	    librdf_free_node (value);
	}

      librdf_query_results_next (results);
    }

  if (store)
    g_object_unref (store);
}

/* Tipologia boolean: */
static void
query_exec_show_boolean (GtkWidget * scrolledwindow,
			 librdf_query_results * results)
{
  GtkTextIter start;
  GtkTextBuffer *buffer;
  GtkWidget *w = query_exec_texttree (scrolledwindow, FALSE, TRUE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w));

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_insert (buffer, &start,
			  librdf_query_results_get_boolean (results) ?
			  _("true") : _("false"), -1);
}

/* Graph usa il parsing normale dei risultati: */
static void
query_exec_show_graph (GtkWidget * scrolledwindow,
		       librdf_query_results * results)
{
  GList *list;
  GList *namespace = NULL;
  gchar *tmp;
  gchar *contents;
  GtkWidget *w;
  librdf_stream *stream;

  if (!(stream = librdf_query_results_as_stream (results)))
    {
      query_exec_texttree (scrolledwindow, FALSE, TRUE);
      return;
    }

  tmp = editor_tmp_file ();
  list = rdf_parse_stream (stream);
  namespace_parse (list, &namespace);

  /* File temporaneo per il salvataggio: */
  if (rdf_save_file (tmp, list, &namespace, RDF_FORMAT_XML) == FALSE)
    {
      dialog_msg (_("Error saving the temporary file."));
      librdf_free_stream (stream);
      g_free (tmp);
      return;
    }

  w = query_exec_texttree (scrolledwindow, FALSE, TRUE);

  if (g_file_get_contents (tmp, &contents, NULL, NULL) == TRUE)
    {
      GtkTextIter start, end;
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w));

      gtk_text_buffer_get_start_iter (buffer, &start);
      gtk_text_buffer_get_end_iter (buffer, &end);
      gtk_text_buffer_delete (buffer, &start, &end);

      gtk_text_buffer_get_start_iter (buffer, &start);
      gtk_text_buffer_insert (buffer, &start, contents, -1);

      g_free (contents);
    }

  librdf_free_stream (stream);
  g_remove (tmp);
  g_free (tmp);
}

static void
query_results_open (GtkWidget * w, GtkWidget * scrolledwindow)
{
  gchar *file;
  gchar *uri;
  GList *list = NULL, *nslist = NULL;
  GString *error = NULL;
  struct editor_data_t *data;

  file = editor_tmp_file ();
  if (query_results_save (file, scrolledwindow) == FALSE)
    {
      g_free (file);
      return;
    }

  /* Creo da file ad uri e attivo le librdf: */
  uri = g_filename_to_uri (file, NULL, NULL);
  g_free (file);

  /* Parso con librdf: */
  if (rdf_parse_thread
      (uri, NULL, &list, &nslist, TRUE, RDF_FORMAT_XML, &error) == FALSE)
    {
      gchar *str;

      if (error)
	str = g_string_free (error, FALSE);
      else
	str = NULL;

      dialog_msg_with_error (_("Error opening the RDF results"), str);

      if (str)
	g_free (str);

      g_free (uri);
      return;
    }

  g_free (uri);

  /* Alloco memoria per questo nuovo elemento: */
  data = g_malloc0 (sizeof (struct editor_data_t));

  data->format = RDF_FORMAT_XML;
  data->rdf = list;
  data->namespace = nslist;

  /* Creo l'interfaccia: */
  editor_new_maker (data);
}

static void
query_results_save_as (GtkWidget * w, GtkWidget * scrolledwindow)
{
  gchar *file;

  if (!
      (file =
       file_chooser (_("Save a QUERY in a file..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_SAVE, NULL)))
    return;

  if (g_file_test (file, G_FILE_TEST_EXISTS)
      && dialog_ask (_("The file exists. Overwrite?")) != GTK_RESPONSE_OK)
    {
      g_free (file);
      return;
    }

  query_results_save (file, scrolledwindow);
  g_free (file);
}

static gboolean
query_results_save (gchar * file, GtkWidget * scrolledwindow)
{
  GtkWidget *w;
  GtkTextBuffer *buffer;
  GList *list;
  GtkTextIter start, end;
  gchar *buf;
  FILE *fl;

  list = gtk_container_get_children (GTK_CONTAINER (scrolledwindow));
  w = list->data;
  g_list_free (list);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w));
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);

  buf = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);

  if (!(fl = g_fopen (file, "wb")))
    {
      gchar s[1024];
      snprintf (s, sizeof (s), _("Error creating the file '%s'!"), file);
      dialog_msg (s);

      if (buf)
	g_free (buf);

      return FALSE;
    }

  fprintf (fl, "%s", buf ? buf : "");
  fclose (fl);

  if (buf)
    g_free (buf);

  return TRUE;
}

static void
query_insert (GtkWidget * w, GtkWidget * textview)
{
  gchar *text;
  GList *resources;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

  resources = g_object_get_data (G_OBJECT (buffer), "resources");

  if ((text =
       search_list (resources, GTK_WINDOW (gtk_widget_get_toplevel (w)))))
    {
      textview_insert_at_cursor (textview, text, -1);
      g_free (text);
    }
}

static void
query_save_session (gchar * buf, gchar * name)
{
  if (!strcmp (name, "SPARQL"))
    {
      if (query_sparql_last)
	g_free (query_sparql_last);

      query_sparql_last = g_strdup (buf);
    }
  else
    {
      if (query_rdql_last)
	g_free (query_rdql_last);

      query_rdql_last = g_strdup (buf);
    }
}

static void
query_rdfs (GtkWidget * w, GtkWidget * textview)
{
  GtkWidget *combo;
  gint id;
  struct rdfs_t *rdfs;
  gchar *text;

  combo = g_object_get_data (G_OBJECT (w), "combo");

  if ((id = gtk_combo_box_get_active (GTK_COMBO_BOX (combo))) < 0)
    return;

  if (!(rdfs = g_list_nth_data (rdfs_list, id)))
    return;

  text = g_strdup_printf ("PREFIX %s: <%s>\n", rdfs->prefix, rdfs->resource);
  textview_insert_at_cursor (textview, text, -1);
  g_free (text);
}

static void
query_textview_map (GtkWidget * text)
{
  GList *list;
  struct tag_t *tag;

  if (highlighting_active == FALSE)
    return;

  for (list = highlighting_tags; list; list = list->next)
    {
      tag = list->data;

      textview_add_tag (text, tag->tag, tag->style, tag->variant, tag->weight,
			tag->stretch, tag->scale, tag->foreground,
			tag->background, tag->underline);
    }
}

static void
query_rasqal_error (void *data, raptor_log_message* message)
{
  gchar *buf = g_strdup (message->text);
  if (message->level <= RAPTOR_LOG_LEVEL_WARN) {
    rdf_log (&rasqal_errors, buf, PARSER_CHAR_WARNING);
  } else {
    rdf_log (&rasqal_errors, buf, PARSER_CHAR_ERROR);
  }
  g_free (buf);
}

static gboolean
query_prepare (GtkTextBuffer * buffer)
{
  static raptor_uri *base_uri = NULL;
  static gchar *previous = NULL;

#ifndef USE_RASQAL_OLD
  static rasqal_world *rasqal = NULL;
  if (rasqal == NULL) {
    rasqal = rasqal_new_world ();
  }
#endif

  static raptor_world* raptor = NULL;
  if (raptor == NULL) {
    raptor = raptor_new_world();
  }

  GtkTextIter start, end;
  gchar *buf;

  if (query_frame)
    {
      GtkStyle *style = gtk_style_copy (gtk_widget_get_default_style ());
      rasqal_query *query;

      gtk_text_buffer_get_start_iter (buffer, &start);
      gtk_text_buffer_get_end_iter (buffer, &end);

      buf = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);

      if (previous && !strcmp (buf, previous))
	{
	  g_free (buf);
	  return TRUE;
	}

      if (previous)
	g_free (previous);

      previous = buf;

      if (rasqal_errors)
	{
	  g_string_free (rasqal_errors, TRUE);
	  rasqal_errors = NULL;
	}

      if (!base_uri)
	{
	  unsigned char *uri_string;

#ifdef USE_RASQAL_OLD
	  rasqal_init ();
#endif

	  uri_string = raptor_uri_filename_to_uri_string ("");
	  base_uri = raptor_new_uri (raptor, uri_string);
	  raptor_free_memory (uri_string);
	}

      if (!strcmp (g_object_get_data (G_OBJECT (buffer), "format"), "SPARQL"))
#ifdef USE_RASQAL_OLD
	query = rasqal_new_query ("sparql", NULL);
#else
	query = rasqal_new_query (rasqal, "sparql", NULL);
#endif
      else
#ifdef USE_RASQAL_OLD
	query = rasqal_new_query ("rdql", NULL);
#else
	query = rasqal_new_query (rasqal, "rdql", NULL);
#endif

      if (query)
	{
          rasqal_world_set_log_handler(rasqal, NULL, query_rasqal_error);

	  if (rasqal_query_prepare
	      (query, (const unsigned char *) buf, base_uri))
	    gdk_color_parse ("red", &style->fg[GTK_STATE_NORMAL]);

	  gtk_widget_set_style (query_frame, style);

	  g_object_unref (style);

	  rasqal_free_query (query);
	}
    }

  return TRUE;
}

static void
query_show_errors (GtkWidget * widget, gpointer dummy)
{
  if (rasqal_errors)
    {
      gchar *buf = g_strdup (rasqal_errors->str);
      dialog_msg_with_error (_("Error in your query!"), buf);
      g_free (buf);
      return;
    }

  dialog_msg (_("No errors in your query."));
}

/* EOF */

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

/* Preso da somax: */
static void
file_chooser_response (GtkWidget * w, gpointer dummy)
{
  gtk_dialog_response (GTK_DIALOG (w), GTK_RESPONSE_ACCEPT);
}

void *
file_chooser (gchar * title, GtkSelectionMode w, GtkFileChooserAction t,
	      enum rdf_format_t *format)
{
  GtkWidget *dialog;
  GtkWidget *combo = NULL;
  static gchar *path = NULL;
  void *l = NULL;

  dialog = gtk_file_chooser_dialog_new (title, NULL, t,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					t ==
					GTK_FILE_CHOOSER_ACTION_SAVE ?
					GTK_STOCK_SAVE : GTK_STOCK_OPEN,
					GTK_RESPONSE_ACCEPT, NULL);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());

  g_signal_connect ((gpointer) dialog, "file_activated",
		    G_CALLBACK (file_chooser_response), NULL);

  if (format)
    {
      GtkListStore *store;
      GtkTreeIter iter;
      GtkCellRenderer *renderer;

      store = gtk_list_store_new (1, G_TYPE_STRING);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, _("RDF/XML format"), -1);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, _("RDF/N-TRIPLES format"), -1);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, _("RDF/TURTLE format"), -1);

      combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), *format);
      gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), combo);

      renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
				      "text", 0, NULL);

      g_object_unref (store);
    }

  if (w == GTK_SELECTION_MULTIPLE)
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);
  else
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);

  if (!path)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
					 g_get_home_dir ());
  else
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), path);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      if (w == GTK_SELECTION_MULTIPLE)
	{
	  l = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (dialog));

	  if (l)
	    {

	      if (path)
		g_free (path);

	      path = g_path_get_dirname ((gchar *) ((GSList *) l)->data);
	    }
	}
      else
	{
	  l = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

	  if (l)
	    {
	      if (path)
		g_free (path);

	      path = g_path_get_dirname ((gchar *) l);
	    }
	}
    }

  if (format)
    *format = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  gtk_widget_destroy (dialog);
  return l;
}

/* Chiede un uri: */
gchar *
uri_chooser (gchar * title, struct download_t ** retoptions)
{
  GtkWidget *dialog;
  GtkWidget *frame;
  GtkWidget *box;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *expander;
  gchar s[1024];
  gchar *uri = NULL;
  static GList *uris = NULL;
  GList *l;
  GtkObject *adj;

  GtkWidget *auth_user;
  GtkWidget *auth_password;
  GtkWidget *cert_file;
  GtkWidget *cert_password;
  GtkWidget *cacert_file;
  GtkWidget *cert_verifypeer;
  GtkWidget *timeout;

  struct download_t *options = NULL;

  if (*retoptions)
    *retoptions = NULL;

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION, title);
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  label = gtk_label_new (_("URI: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_combo_box_entry_new_text ();
  gtk_widget_set_size_request (entry, 300, -1);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

  if (retoptions)
    {
      expander = gtk_expander_new_with_mnemonic (_("_Preferences..."));
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), expander,
			  FALSE, FALSE, 0);

      box = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (expander), box);

      frame = gtk_frame_new (_("Authentication"));
      gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("User:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      auth_user = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), auth_user, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("Password:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      auth_password = gtk_entry_new ();
      gtk_entry_set_visibility (GTK_ENTRY (auth_password), FALSE);
      gtk_box_pack_start (GTK_BOX (hbox), auth_password, TRUE, TRUE, 0);

      frame = gtk_frame_new (_("SSL Client Authentication"));
      gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("Certificate File:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      cert_file =
	gtk_file_chooser_button_new (_("Certificate SSL File"),
				     GTK_FILE_CHOOSER_ACTION_OPEN);
      gtk_box_pack_start (GTK_BOX (hbox), cert_file, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("Password:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      cert_password = gtk_entry_new ();
      gtk_entry_set_visibility (GTK_ENTRY (cert_password), FALSE);
      gtk_box_pack_start (GTK_BOX (hbox), cert_password, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("CA Certificate File:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      cacert_file =
	gtk_file_chooser_button_new (_("CA Certificate SSL File"),
				     GTK_FILE_CHOOSER_ACTION_OPEN);
      gtk_box_pack_start (GTK_BOX (hbox), cacert_file, TRUE, TRUE, 0);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("Verify Peer:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      cert_verifypeer = gtk_check_button_new ();
      gtk_box_pack_start (GTK_BOX (hbox), cert_verifypeer, TRUE, TRUE, 0);

      frame = gtk_frame_new (_("Timeout"));
      gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (frame), vbox);

      hbox = gtk_hbox_new (TRUE, 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      label = gtk_label_new (_("Seconds:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      adj = gtk_adjustment_new (undo_max_value, 1, 60, 1, 10, 10);
      timeout = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
      gtk_box_pack_start (GTK_BOX (hbox), timeout, TRUE, TRUE, 0);
    }

  l = uris;
  while (l)
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), (gchar *) l->data);
      l = l->next;
    }

  if (uris)
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry), 0);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CANCEL);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);

  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      gchar *tmp;
      gint tmpi;

      uri = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

      if (!uri || !*uri)
	{
	  if (uri)
	    g_free (uri);

	  dialog_msg (_("No compatible URI!"));
	  continue;
	}

      l = uris;
      while (l)
	{
	  if (!strcmp ((gchar *) l->data, uri))
	    break;
	  l = l->next;
	}

      if (!l)
	{
	  uris = g_list_prepend (uris, g_strdup (uri));

	  if (g_list_length (uris) > 10)
	    {
	      l = g_list_last (uris);

	      g_free (l->data);
	      uris = g_list_remove (uris, l->data);
	    }
	}

      if (retoptions)
	{
	  if ((tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (auth_user)))
	      && *tmp)
	    {
	      if (!options)
		options = g_malloc0 (sizeof (struct download_t));

	      options->auth_user = g_strdup (tmp);
	    }

	  if ((tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (auth_password)))
	      && *tmp)
	    {
	      if (!options)
		options = g_malloc0 (sizeof (struct download_t));

	      options->auth_password = g_strdup (tmp);
	    }

	  if ((tmp =
	       gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (cert_file))))
	    {
	      if (!options)
		options = g_malloc0 (sizeof (struct download_t));

	      options->certfile = tmp;
	    }

	  if ((tmp =
	       gtk_file_chooser_get_filename (GTK_FILE_CHOOSER
					      (cacert_file))))
	    {
	      if (!options)
		options = g_malloc0 (sizeof (struct download_t));

	      options->cacertfile = tmp;
	    }

	  if ((tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (cert_password)))
	      && *tmp)
	    {
	      if (!options)
		options = g_malloc0 (sizeof (struct download_t));

	      options->certpassword = g_strdup (tmp);
	    }

	  if (options
	      &&
	      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
					    (cert_verifypeer)) == TRUE)
	    options->verifypeer = TRUE;

	  if ((tmpi =
	       gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (timeout))))
	    {
	      if (!options)
		options = g_malloc0 (sizeof (struct download_t));

	      options->timeout = tmpi;
	    }
	}

      break;
    }

  gtk_widget_destroy (dialog);

  if (options)
    {
      if (retoptions)
	*retoptions = options;
      else
	download_free (options);
    }

  return uri;
}

/* EOF */

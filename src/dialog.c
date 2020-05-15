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

/* From somax */
gint
dialog_ask (gchar * text)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  gint ret;
  gchar s[1024];

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Question..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (text);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_NO);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);
  ret = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return ret;
}

gint
dialog_ask_with_cancel (gchar * text)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  gint ret;
  gchar s[1024];

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Question..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (text);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CANCEL);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_NO);

  gtk_widget_set_can_default (button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);
  ret = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return ret;
}

void
dialog_msg (gchar * text)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  gchar s[1024];

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Information..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (text);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

void
dialog_msg_with_error (gchar * text, gchar * error)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  gchar s[1024];

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Information..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (text);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  /* Se c'e' l'errore mostro l'espander: */
  if (error)
    {
      GtkWidget *expander;
      GtkWidget *sw;
      GtkWidget *textview;
      GtkTextBuffer *buffer;
      GtkTextIter iter;
      gint i, j, len;
      GtkTextTag *tag_init, *tag_error, *tag_warning;

      expander = gtk_expander_new (NULL);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), expander, TRUE,
			  TRUE, 0);


      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (expander), sw);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				      GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

      textview = gtk_text_view_new ();
      gtk_container_add (GTK_CONTAINER (sw), textview);
      gtk_widget_set_size_request (textview, -1, 200);
      gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

      tag_init =
	gtk_text_buffer_create_tag (buffer, NULL, "weight", PANGO_WEIGHT_BOLD,
				    NULL);
      tag_error =
	gtk_text_buffer_create_tag (buffer, NULL, "foreground", "red", NULL);
      tag_warning =
	gtk_text_buffer_create_tag (buffer, NULL, "foreground", "blue", NULL);

      for (i = j = 0, len = strlen (error); i < len; i++)
	{
	  if (error[i] == '\n')
	    {
	      switch (*(error + j))
		{
		case PARSER_CHAR_INIT:
		  gtk_text_buffer_insert_with_tags (buffer, &iter,
						    error + j + 1, i - j,
						    tag_init, NULL);
		  break;

		case PARSER_CHAR_WARNING:
		  gtk_text_buffer_insert_with_tags (buffer, &iter,
						    error + j + 1, i - j,
						    tag_warning, NULL);
		  break;

		case PARSER_CHAR_ERROR:
		  gtk_text_buffer_insert_with_tags (buffer, &iter,
						    error + j + 1, i - j,
						    tag_error, NULL);
		  break;
		}

	      j = i + 1;
	    }
	}

      label = gtk_label_new (_("Show error messages"));
      gtk_expander_set_label_widget (GTK_EXPANDER (expander), label);
    }

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
dialog_wait_destroy (GtkWidget * window, GdkCursor * cursor)
{
  gdk_cursor_unref (cursor);
}

GtkWidget *
dialog_wait (GCallback callback, gpointer data, GtkWidget ** pb_ret)
{
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *box;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *pb;
  GtkWidget *sep;
  GtkWidget *button;
  GtkWidget *label;
  GdkCursor *cursor;
  gchar s[1024];

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Please Wait..."));
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_type_hint (GTK_WINDOW (window),
			    GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_widget_set_size_request (window, 200, -1);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (window), frame);

  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (_("Please Wait..."));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  pb = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (box), pb, TRUE, TRUE, 0);

  if (callback)
    {
      sep = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box), sep, FALSE, FALSE, 3);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (hbox), gtk_hbox_new (0, 0), TRUE, TRUE, 0);

      button = gtk_button_new_from_stock ("gtk-cancel");
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (button), "clicked", callback, data);
    }

  gtk_widget_show_all (window);

  cursor = gdk_cursor_new (GDK_WATCH);
  gdk_window_set_cursor (window->window, cursor);

  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (dialog_wait_destroy), cursor);

  *pb_ret = pb;
  return window;
}

/* EOF */

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

#define MORLA_POPUP_HEIGHT	150
#define MORLA_POPUP_WIDTH	300
#define MORLA_POPUP_SPAN	5

struct textview_tag_t
{
  gchar *tag;
  gchar *cmp;

  PangoStyle style;
  PangoVariant variant;
  PangoWeight weight;
  PangoStretch stretch;
  gdouble scale;
  gchar *foreground;
  gchar *background;
  PangoUnderline underline;
};

static void textview_popup_hide (GtkTextBuffer * buffer);
static void textview_destroy (GtkWidget * widget, GtkTextBuffer * buffer);

static gchar *
textview_word (GtkTextBuffer * buffer, GtkTextIter ** ret_start,
	       GtkTextIter ** ret_end)
{
  GtkTextIter end;
  GtkTextMark *cursor;
  GString *word;
  gunichar c;
  gchar *utf8;
  gchar *str;

  /* Current iter: */
  cursor = gtk_text_buffer_get_mark (buffer, "insert");
  gtk_text_buffer_get_iter_at_mark (buffer, &end, cursor);

  *ret_end = gtk_text_iter_copy (&end);

  /* Until it is not inside a word, go back: */
  while (gtk_text_iter_inside_word (*ret_end) == TRUE
	 && gtk_text_iter_forward_char (*ret_end) == TRUE);

  /* If it is the last one, it is always empty: */
  if (gtk_text_iter_ends_word (&end) == TRUE
      && gtk_text_iter_backward_char (&end) == FALSE)
    {
      gtk_text_iter_free (*ret_end);
      return NULL;
    }

  /* If it is not inside a word: */
  if (gtk_text_iter_inside_word (&end) == FALSE)
    {
      gtk_text_iter_free (*ret_end);
      return NULL;
    }

  /* Until it is not inside a word, go back: */
  while (gtk_text_iter_inside_word (&end) == TRUE
	 && gtk_text_iter_backward_char (&end) == TRUE);

  word = g_string_new (NULL);

  /* If it is not the first char of the word, go to next one: */
  if (gtk_text_iter_inside_word (&end) == FALSE)
    gtk_text_iter_forward_char (&end);

  *ret_start = gtk_text_iter_copy (&end);

  /* Get the word: */
  do
    {
      c = gtk_text_iter_get_char (&end);
      gtk_text_iter_forward_char (&end);
      word = g_string_append_unichar (word, c);
    }
  while (gtk_text_iter_inside_word (&end) == TRUE);

  str = g_string_free (word, FALSE);
  utf8 = g_utf8_casefold (str, -1);
  g_free (str);

  return utf8;
}

static gboolean
textview_key_press (GtkWidget * widget, GdkEventKey * event,
		    GtkTextBuffer * buffer)
{
  GtkWidget *tree;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  tree = g_object_get_data (G_OBJECT (buffer), "treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));

  switch (event->keyval)
    {
      /* The next element in the list: */
    case GDK_Down:
      if (gtk_tree_selection_get_selected (selection, NULL, &iter) == TRUE
	  && gtk_tree_model_iter_next (model, &iter) == TRUE)
	gtk_tree_selection_select_iter (selection, &iter);
      else
	textview_popup_hide (buffer);

      return TRUE;

      /* The previous element in the list: */
    case GDK_Up:
      if (gtk_tree_selection_get_selected (selection, NULL, &iter) == TRUE)
	{
	  GtkTreePath *path;

	  path = gtk_tree_model_get_path (model, &iter);

	  if (gtk_tree_path_prev (path) == TRUE)
	    gtk_tree_selection_select_path (selection, path);
	  else
	    textview_popup_hide (buffer);

	  gtk_tree_path_free (path);
	}

      return TRUE;

      /* Insert this one: */
    case GDK_Return:
      if (gtk_tree_selection_get_selected (selection, NULL, &iter) == TRUE)
	{
	  gchar *str, *word;
	  GtkTextIter *start, *end;

	  gtk_tree_model_get (model, &iter, 0, &str, -1);

	  if ((word = textview_word (buffer, &start, &end)))
	    {
	      gtk_text_buffer_delete (buffer, start, end);
	      gtk_text_buffer_insert (buffer, start, str, -1);

	      gtk_text_iter_free (start);
	      gtk_text_iter_free (end);
	      g_free (word);

	      textview_popup_hide (buffer);
	    }

	  g_free (str);
	}

      return TRUE;

      /* Close the popup: */
    case GDK_Escape:
      textview_popup_hide (buffer);
      return TRUE;

    default:
      return FALSE;
    }
}

static void
textview_popup_show (GtkTextBuffer * buffer, GList * list, GtkTextIter * iter)
{
  GtkWidget *text;
  GtkWidget *window;
  GtkWidget *treeview;
  GdkRectangle location;
  GtkTreeSelection *selection;
  GtkListStore *store;
  GtkTreeIter first;
  gint x, y, gx, gy;
  gulong id, *idp;

  /* If no window was created, I create it: */
  if (!(window = g_object_get_data (G_OBJECT (buffer), "textview_popup")))
    {
      GtkWidget *scrolledwindow;
      GtkCellRenderer *renderer;
      GtkTreeViewColumn *column;

      window = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_type_hint (GTK_WINDOW (window),
				GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      gtk_window_set_decorated (GTK_WINDOW (window), FALSE);

      gtk_widget_set_size_request (window, MORLA_POPUP_WIDTH,
				   MORLA_POPUP_HEIGHT);
      g_object_set_data (G_OBJECT (buffer), "textview_popup", window);

      scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (window), scrolledwindow);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
					   (scrolledwindow), GTK_SHADOW_IN);

      store = gtk_list_store_new (1, G_TYPE_STRING);

      treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
      gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
      gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

      gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

      renderer = gtk_cell_renderer_text_new ();

      column =
	gtk_tree_view_column_new_with_attributes ("Completion", renderer,
						  "text", 0, NULL);
      gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

      g_object_set_data (G_OBJECT (buffer), "treeview", treeview);
    }

  /* Else I get the treeview widget: */
  else
    treeview = g_object_get_data (G_OBJECT (buffer), "treeview");

  /* Get the current position and set the window there: */
  text = g_object_get_data (G_OBJECT (buffer), "widget");
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (text), iter, &location);

  gdk_window_get_origin (text->window, &gx, &gy);
  x = gx + location.x + MORLA_POPUP_SPAN;
  y = gy + location.y + location.height + MORLA_POPUP_SPAN;

  if (gdk_screen_width () <= x + MORLA_POPUP_WIDTH)
    x -= x + MORLA_POPUP_WIDTH - gdk_screen_width ();

  if (gdk_screen_height () <= y + MORLA_POPUP_HEIGHT)
    y = gy + location.y - MORLA_POPUP_SPAN - MORLA_POPUP_HEIGHT;

  gtk_window_move (GTK_WINDOW (window), x, y);

  /* Remove the previous element and insert the next ones: */
  store = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (store), &first, NULL, 0))
    gtk_list_store_remove (store, &first);

  for (; list; list = list->next)
    {
      gtk_list_store_append (store, &first);
      gtk_list_store_set (store, &first, 0, list->data, -1);
    }

  if (!g_object_get_data (G_OBJECT (buffer), "handler"))
    {
      id =
	g_signal_connect (text, "key-press-event",
			  G_CALLBACK (textview_key_press), buffer);
      idp = g_malloc0 (sizeof (gulong));
      *idp = id;

      g_object_set_data (G_OBJECT (buffer), "handler", idp);
    }

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &first) == TRUE)
    gtk_tree_selection_select_iter (selection, &first);

  /* Show it: */
  gtk_widget_show_all (window);
}

static void
textview_popup_hide (GtkTextBuffer * buffer)
{
  GtkWidget *window;
  GtkWidget *text;
  gulong *idp;

  if ((window = g_object_get_data (G_OBJECT (buffer), "textview_popup")))
    {
      gtk_widget_destroy (window);
      g_object_steal_data (G_OBJECT (buffer), "textview_popup");
    }

  if ((idp = g_object_get_data (G_OBJECT (buffer), "handler"))
      && (text = g_object_get_data (G_OBJECT (buffer), "widget")))
    {
      g_object_steal_data (G_OBJECT (buffer), "handler");
      g_signal_handler_disconnect (text, *idp);
      g_free (idp);
    }
}

static void
textview_color (GtkTextBuffer * buffer, gchar * word, GtkTextIter * start,
		GtkTextIter * end)
{
  struct textview_tag_t *tag;
  GList *list;
  GSList *tags;

  if (!(list = g_object_get_data (G_OBJECT (buffer), "tags")))
    return;

  for (; list; list = list->next)
    {
      tag = list->data;

      if (!g_utf8_collate (tag->cmp, word))
	break;
    }

  /* Founded: */
  if (list)
    {
      if (!(tags = gtk_text_iter_get_tags (start)))
	{
	  GtkTextTag *t;

	  if (tag->style != PANGO_STYLE_NORMAL
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "style",
					      tag->style, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->variant != PANGO_VARIANT_NORMAL
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "variant",
					      tag->variant, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->weight != PANGO_WEIGHT_NORMAL
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "weight",
					      tag->weight, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->stretch != PANGO_STRETCH_NORMAL
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "stretch",
					      tag->stretch, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->scale != PANGO_SCALE_MEDIUM
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "scale",
					      tag->scale, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->foreground
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "foreground",
					      tag->foreground, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->background
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "background",
					      tag->background, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);

	  if (tag->underline
	      && (t =
		  gtk_text_buffer_create_tag (buffer, NULL, "underline",
					      tag->underline, NULL)))
	    gtk_text_buffer_apply_tag (buffer, t, start, end);
	}
    }

  /* Not founded: */
  else
    {
      if ((tags = gtk_text_iter_get_tags (start)))
	for (; tags; tags = tags->next)
	  gtk_text_buffer_remove_tag (buffer, tags->data, start, end);
    }
}

static void
textview_propage (GtkTextBuffer * buffer)
{
  void (*changed) (GtkWidget *, gpointer) =
    g_object_get_data (G_OBJECT (buffer), "changed_func");

  if (changed)
    {
      gpointer data = g_object_get_data (G_OBJECT (buffer), "changed_data");
      GtkWidget *widget = g_object_get_data (G_OBJECT (buffer), "widget");

      changed (widget, data);
    }
}

static void
textview_textview_changed (GtkTextBuffer * buffer, GCompletion * completion)
{
  GList *list;
  gchar *word;
  GtkTextIter *start, *end;

  if (!(word = textview_word (buffer, &start, &end)))
    {
      textview_popup_hide (buffer);
      textview_propage (buffer);
      return;
    }

  /* Color the word: */
  textview_color (buffer, word, start, end);

  /* Search it: */
  if (!g_object_get_data (G_OBJECT (buffer), "popup_off")
      && (list = g_completion_complete (completion, word, NULL)))
    textview_popup_show (buffer, list, end);
  else
    textview_popup_hide (buffer);

  g_free (word);
  gtk_text_iter_free (start);
  gtk_text_iter_free (end);

  textview_propage (buffer);
}

void
textview_add_tag (GtkWidget * text, gchar * tag, PangoStyle style,
		  PangoVariant variant, PangoWeight weight,
		  PangoStretch stretch, gdouble scale, gchar * foreground,
		  gchar * background, PangoUnderline underline)
{
  GList *list;
  struct textview_tag_t *data;
  GCompletion *completion;
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));
  list = g_object_get_data (G_OBJECT (buffer), "tags");

  data = g_malloc0 (sizeof (struct textview_tag_t));
  data->tag = g_strdup (tag);
  data->cmp = g_utf8_casefold (tag, -1);
  data->style = style;
  data->variant = variant;
  data->weight = weight;
  data->stretch = stretch;
  data->scale = scale;
  data->foreground = g_strdup (foreground);
  data->background = g_strdup (background);
  data->underline = underline;

  list = g_list_append (list, data);

  g_object_set_data (G_OBJECT (buffer), "tags", list);

  list = g_list_append (NULL, tag);
  completion = g_object_get_data (G_OBJECT (buffer), "completion");
  g_completion_add_items (completion, list);
}

static gint
textview_compare (gchar * s1, gchar * s2, gsize n)
{
  gint ret;

  s1 = g_utf8_casefold (s1, g_utf8_strlen (s1, n));
  s2 = g_utf8_casefold (s2, g_utf8_strlen (s2, n));

  ret = g_utf8_collate (s1, s2);

  g_free (s1);
  g_free (s2);

  return ret;
}

static gboolean
textview_focus_out (GtkTextView * text, GdkEventFocus * event,
		    GtkTextBuffer * buffer)
{
  textview_popup_hide (buffer);
  return FALSE;
}

GtkWidget *
textview_new (GCallback change, gpointer data)
{
  GtkWidget *text;
  GtkTextBuffer *buffer;
  GCompletion *completion;

  completion = g_completion_new (NULL);
  g_completion_set_compare (completion,
			    (GCompletionStrncmpFunc) textview_compare);

  buffer = gtk_text_buffer_new (NULL);
  g_signal_connect ((gpointer) buffer, "changed",
		    G_CALLBACK (textview_textview_changed), completion);

  text = gtk_text_view_new_with_buffer (buffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text), TRUE);
  g_object_set_data (G_OBJECT (buffer), "widget", text);
  g_object_set_data (G_OBJECT (buffer), "completion", completion);

  if (change)
    {
      g_object_set_data (G_OBJECT (buffer), "changed_func", change);

      if (data)
	g_object_set_data (G_OBJECT (buffer), "changed_data", data);
    }

  g_signal_connect ((gpointer) text, "focus-out-event",
		    G_CALLBACK (textview_focus_out), buffer);

  g_signal_connect ((gpointer) text, "destroy", G_CALLBACK (textview_destroy),
		    buffer);

  return text;
}

static void
textview_color_all (GtkTextBuffer * buffer)
{
  GtkTextIter start, end, wend;
  gchar *word, *utf8;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);

  while (gtk_text_iter_compare (&start, &end) < 0)
    {
      wend = start;

      gtk_text_iter_forward_word_end (&wend);

      word = gtk_text_buffer_get_text (buffer, &start, &wend, FALSE);
      utf8 = g_utf8_casefold (word, -1);
      g_free (word);

      textview_color (buffer, utf8, &start, &wend);
      g_free (utf8);

      /* now move wend to the beginning of the next word, */
      gtk_text_iter_forward_word_end (&wend);
      gtk_text_iter_backward_word_start (&wend);

      /* make sure we've actually advanced
       * (we don't advance in some corner cases), */
      if (gtk_text_iter_equal (&start, &wend))
	break;

      start = wend;
    }
}

void
textview_insert (GtkWidget * textview, GtkTextIter * iter, gchar * text,
		 gint len)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  g_object_set_data (G_OBJECT (buffer), "popup_off", buffer);
  gtk_text_buffer_insert (buffer, iter, text, len);
  textview_color_all (buffer);
  g_object_steal_data (G_OBJECT (buffer), "popup_off");
}

void
textview_insert_at_cursor (GtkWidget * textview, gchar * text, gint len)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  g_object_set_data (G_OBJECT (buffer), "popup_off", buffer);
  gtk_text_buffer_insert_at_cursor (buffer, text, len);
  textview_color_all (buffer);
  g_object_steal_data (G_OBJECT (buffer), "popup_off");
}

static void
textview_tags_free (struct textview_tag_t *tag)
{
  if (!tag)
    return;

  if (tag->tag)
    g_free (tag->tag);

  if (tag->cmp)
    g_free (tag->cmp);

  if (tag->foreground)
    g_free (tag->foreground);

  if (tag->background)
    g_free (tag->background);

  g_free (tag);
}

void
textview_clean_tags (GtkWidget * widget)
{
  GtkTextBuffer *buffer;
  GList *list;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

  if ((list = g_object_get_data (G_OBJECT (buffer), "tags")))
    {
      g_list_foreach (list, (GFunc) textview_tags_free, NULL);
      g_list_free (list);
      g_object_steal_data (G_OBJECT (buffer), "tags");
    }
}

static void
textview_destroy (GtkWidget * widget, GtkTextBuffer * buffer)
{
  gulong *idp;

  textview_popup_hide (buffer);

  if ((idp = g_object_get_data (G_OBJECT (buffer), "handler")))
    {
      g_object_steal_data (G_OBJECT (buffer), "handler");
      g_signal_handler_disconnect (widget, *idp);
      g_free (idp);
    }

  textview_clean_tags (widget);
}

/* EOF */

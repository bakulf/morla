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

struct preferences_node_t
{
  GtkWidget *image;
  GtkWidget *color;
  GtkWidget *fontcolor;
  GtkWidget *style;
  GtkWidget *fillcolor;
  GtkWidget *shape;
  GtkWidget *font;
  void (*renderer) (struct preferences_node_t *, GtkWidget * pb);
};

static void create_node (struct preferences_node_t *, GtkWidget * pb);
static void create_edge (struct preferences_node_t *, GtkWidget * pb);
static void create_object (struct preferences_node_t *);

static void conf_refresh (GtkWidget * w, struct preferences_node_t *);
static void color_refresh (GtkWidget * w, gpointer dummy);

static void preferences_renderer (struct preferences_node_t *,
				  GtkWidget * pb);

static gboolean on_ebox_enter (GtkWidget * box, GdkEventCrossing * event,
			       GtkLabel * label);
static gboolean on_ebox_leave (GtkWidget * box, GdkEventCrossing * event,
			       GtkLabel * label);
static void on_ebox_press (GtkWidget * box);
static void set_style (GtkWidget * w, GtkStyle * s);

static gboolean preferences_wait (gchar ** array, GtkWidget * pb);
static void preferences_switch (GtkWidget * w, GtkWidget * box);

static void preferences_highlighting_free (void);
static void preferences_highlighting_free_single (struct tag_t *);
static void preferences_highlighting_add (GtkWidget * button,
					  GtkWidget * treeview);
static void preferences_highlighting_rename (GtkWidget * button,
					     GtkWidget * treeview);
static void preferences_highlighting_remove (GtkWidget * button,
					     GtkWidget * treeview);
static void preferences_highlighting_selection (GtkTreeSelection * selection,
						GtkWidget * frame);
static void preferences_highlighting_refresh (GtkWidget * treeview);
static void preferences_highlighting_save (GtkWidget * button,
					   GtkTreeView * treeview);
static void preferences_highlighting_demo (GtkWidget *, struct tag_t *tag);
static void preferences_highlighting_reset (GtkWidget * w,
					    GtkWidget * treeview);
#ifdef USE_GCONF
static void preferences_proxy (GtkWidget * w, GtkWidget * hbox);
static void preferences_proxy_gconf (GtkWidget * w, GtkWidget * proxy);
#endif

/* Questa funzione configura le preferenze */
void
preferences (void)
{
  GtkWidget *dialog;
  GtkWidget *notebook;
  GtkWidget *frame;
  GtkWidget *box;
  GtkWidget *gbox;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *browser;
  GtkWidget *viewer;
  GtkWidget *update;
  GtkWidget *blanknode;
  GtkWidget *help;
  GtkWidget *updated;
  GtkWidget *extensions;
  GtkWidget *undo;
  GtkWidget *last_document;
  GtkWidget *button;
  GtkWidget *format;
  GtkWidget *nbg_color;
  GtkWidget *nfg_color;
  GtkWidget *pbg_color;
  GtkWidget *pfg_color;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkObject *adj;
  GdkColor color;
  gchar s[1024];

  GtkWidget *proxy;
#ifdef USE_GCONF
  GtkWidget *proxy_gconf;
#endif
  GtkWidget *proxy_server;
  GtkWidget *proxy_port;
  GtkWidget *proxy_user;
  GtkWidget *proxy_password;

  GtkWidget *scrolledwindow;
  GtkWidget *treeview;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GList *list;

  GtkWidget *highlighting;
  GtkWidget *highlighting_demo;

  GtkWidget *wait;
  GtkWidget *pb;

  struct preferences_node_t edge;
  struct preferences_node_t node_r;
  struct preferences_node_t node_l;
  struct preferences_node_t node_b;

  memset (&edge, 0, sizeof (struct preferences_node_t));
  memset (&node_r, 0, sizeof (struct preferences_node_t));
  memset (&node_l, 0, sizeof (struct preferences_node_t));
  memset (&node_b, 0, sizeof (struct preferences_node_t));

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Preferences..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), notebook, TRUE,
		      TRUE, 0);

  /* GENERAL */
  gbox = gtk_vbox_new (FALSE, 8);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gbox,
			    gtk_label_new (_("General")));

  frame = gtk_frame_new (_("Softwares"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  label = gtk_label_new (_("Select your web browser:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  browser = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), browser, TRUE, TRUE, 5);
  gtk_entry_set_text (GTK_ENTRY (browser), browser_default);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  label = gtk_label_new (_("Select your image viewer:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  viewer = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), viewer, TRUE, TRUE, 5);
  gtk_entry_set_text (GTK_ENTRY (viewer), viewer_default);

  label = gtk_label_new (_("'%s' = url or image"));
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 5);

  frame = gtk_frame_new (_("Undo and Redo"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new (_("Select your undo level:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  adj = gtk_adjustment_new (undo_max_value, 1, 60, 1, 10, 10);
  undo = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), undo, TRUE, TRUE, 5);

  frame = gtk_frame_new (_("Last Documents"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new (_("Select your last documents history:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  adj = gtk_adjustment_new (last_document_value, 1, 60, 1, 10, 10);
  last_document = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), last_document, TRUE, TRUE, 5);

  frame = gtk_frame_new (_("Show update"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new (_("Show with popup:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  updated = gtk_check_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), updated, TRUE, TRUE, 5);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (updated),
				update_show >= 0 ? TRUE : FALSE);

  frame = gtk_frame_new (_("Update Server"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new (_("Set the update server:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  update = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), update, TRUE, TRUE, 5);
  gtk_entry_set_text (GTK_ENTRY (update), update_url);

  frame = gtk_frame_new (_("Documentation Server"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  label = gtk_label_new (_("Set the documentation server:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  help = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), help, TRUE, TRUE, 5);
  gtk_entry_set_text (GTK_ENTRY (help), help_url);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  label =
    gtk_label_new (_("(This input can be a directory of a remote server)"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  frame = gtk_frame_new (_("Blank node name"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  label = gtk_label_new (_("Set the blank node name:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  blanknode = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), blanknode, TRUE, TRUE, 5);
  gtk_entry_set_text (GTK_ENTRY (blanknode), blanknode_name);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);

  label =
    gtk_label_new (_
		   ("(You can use '%d' if you want specify where the number should be: the_%d_blank_node)"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  frame = gtk_frame_new (_("Automatic Extensions"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (TRUE, 8);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new (_("Set automatic extensions:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  extensions = gtk_check_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), extensions, TRUE, TRUE, 5);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (extensions),
				automatic_extensions);


  /* INTERFACE */
  gbox = gtk_vbox_new (FALSE, 8);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gbox,
			    gtk_label_new (_("Interface")));

  frame = gtk_frame_new (_("Colors"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  vbox = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Normal Background:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  nbg_color = gtk_color_button_new ();
  gdk_color_parse (normal_bg_color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (nbg_color), &color);
  gtk_box_pack_start (GTK_BOX (hbox), nbg_color, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Normal Foreground:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  nfg_color = gtk_color_button_new ();
  gdk_color_parse (normal_fg_color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (nfg_color), &color);
  gtk_box_pack_start (GTK_BOX (hbox), nfg_color, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Prelight Background:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  pbg_color = gtk_color_button_new ();
  gdk_color_parse (prelight_bg_color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (pbg_color), &color);
  gtk_box_pack_start (GTK_BOX (hbox), pbg_color, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Prelight Foreground:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  pfg_color = gtk_color_button_new ();
  gdk_color_parse (prelight_fg_color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (pfg_color), &color);
  gtk_box_pack_start (GTK_BOX (hbox), pfg_color, TRUE, TRUE, 0);

  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (nbg_color), "nbg_color", nbg_color);
  g_object_set_data (G_OBJECT (nbg_color), "nfg_color", nfg_color);
  g_object_set_data (G_OBJECT (nbg_color), "pbg_color", pbg_color);
  g_object_set_data (G_OBJECT (nbg_color), "pfg_color", pfg_color);
  g_object_set_data (G_OBJECT (nbg_color), "frame", frame);

  g_object_set_data (G_OBJECT (nfg_color), "nbg_color", nbg_color);
  g_object_set_data (G_OBJECT (nfg_color), "nfg_color", nfg_color);
  g_object_set_data (G_OBJECT (nfg_color), "pbg_color", pbg_color);
  g_object_set_data (G_OBJECT (nfg_color), "pfg_color", pfg_color);
  g_object_set_data (G_OBJECT (nfg_color), "frame", frame);

  g_object_set_data (G_OBJECT (pbg_color), "nbg_color", nbg_color);
  g_object_set_data (G_OBJECT (pbg_color), "nfg_color", nfg_color);
  g_object_set_data (G_OBJECT (pbg_color), "pbg_color", pbg_color);
  g_object_set_data (G_OBJECT (pbg_color), "pfg_color", pfg_color);
  g_object_set_data (G_OBJECT (pbg_color), "frame", frame);

  g_object_set_data (G_OBJECT (pfg_color), "nbg_color", nbg_color);
  g_object_set_data (G_OBJECT (pfg_color), "nfg_color", nfg_color);
  g_object_set_data (G_OBJECT (pfg_color), "pbg_color", pbg_color);
  g_object_set_data (G_OBJECT (pfg_color), "pfg_color", pfg_color);
  g_object_set_data (G_OBJECT (pfg_color), "frame", frame);

  g_signal_connect ((gpointer) nbg_color, "color_set",
		    G_CALLBACK (color_refresh), NULL);
  g_signal_connect ((gpointer) nfg_color, "color_set",
		    G_CALLBACK (color_refresh), NULL);
  g_signal_connect ((gpointer) pbg_color, "color_set",
		    G_CALLBACK (color_refresh), NULL);
  g_signal_connect ((gpointer) pfg_color, "color_set",
		    G_CALLBACK (color_refresh), NULL);

  color_refresh (pfg_color, NULL);

  /* GRAPH */
  gbox = gtk_vbox_new (FALSE, 8);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gbox,
			    gtk_label_new (_("Graphs")));

  frame = gtk_frame_new (_("Resource nodes"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_r.color = gtk_color_button_new ();
  gdk_color_parse (graph_resource_node.color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_r.color), &color);
  gtk_box_pack_start (GTK_BOX (box), node_r.color, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_r.fontcolor = gtk_color_button_new ();
  gdk_color_parse (graph_resource_node.fontcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_r.fontcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), node_r.fontcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Style:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("filled"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("solid"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dashed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dotted"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("bold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("invis"), -1);

  node_r.style = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

  if (!strcmp (graph_resource_node.style, "filled"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.style), 0);

  else if (!strcmp (graph_resource_node.style, "solid"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.style), 1);

  else if (!strcmp (graph_resource_node.style, "dashed"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.style), 2);

  else if (!strcmp (graph_resource_node.style, "dotted"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.style), 3);

  else if (!strcmp (graph_resource_node.style, "bold"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.style), 4);

  else if (!strcmp (graph_resource_node.style, "invis"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.style), 5);

  gtk_box_pack_start (GTK_BOX (box), node_r.style, TRUE, TRUE, 0);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (node_r.style), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (node_r.style), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Fill Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_r.fillcolor = gtk_color_button_new ();
  gdk_color_parse (graph_resource_node.fillcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_r.fillcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), node_r.fillcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Shape:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("plaintext"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("ellipse"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("circle"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("egg"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("triangle"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("box"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("diamond"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("trapezium"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("parallelogram"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("house"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("hexagon"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("octagon"), -1);

  node_r.shape = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 0);
  gtk_box_pack_start (GTK_BOX (box), node_r.shape, TRUE, TRUE, 0);

  if (!strcmp (graph_resource_node.shape, "plaintext"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 0);

  else if (!strcmp (graph_resource_node.shape, "ellipse"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 1);

  else if (!strcmp (graph_resource_node.shape, "circle"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 2);

  else if (!strcmp (graph_resource_node.shape, "egg"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 3);

  else if (!strcmp (graph_resource_node.shape, "triangle"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 4);

  else if (!strcmp (graph_resource_node.shape, "box"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 5);

  else if (!strcmp (graph_resource_node.shape, "diamond"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 6);

  else if (!strcmp (graph_resource_node.shape, "trapezium"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 7);

  else if (!strcmp (graph_resource_node.shape, "parallelogram"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 8);

  else if (!strcmp (graph_resource_node.shape, "house"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 9);

  else if (!strcmp (graph_resource_node.shape, "hexagon"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 10);

  else if (!strcmp (graph_resource_node.shape, "octagon"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_r.shape), 11);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (node_r.shape), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (node_r.shape), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_r.font = gtk_font_button_new ();
  g_snprintf (s, sizeof (s), "%s %d", graph_resource_node.fontname,
	      graph_resource_node.fontsize);
  gtk_font_button_set_font_name (GTK_FONT_BUTTON (node_r.font), s);
  gtk_box_pack_start (GTK_BOX (box), node_r.font, TRUE, TRUE, 0);

  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  node_r.image = gtk_image_new ();
  g_object_set_data (G_OBJECT (node_r.image), "label", _("Resource"));
  gtk_container_add (GTK_CONTAINER (frame), node_r.image);

  /* BLANK NODE */
  frame = gtk_frame_new (_("Blank nodes"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_b.color = gtk_color_button_new ();
  gdk_color_parse (graph_blank_node.color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_b.color), &color);
  gtk_box_pack_start (GTK_BOX (box), node_b.color, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_b.fontcolor = gtk_color_button_new ();
  gdk_color_parse (graph_blank_node.fontcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_b.fontcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), node_b.fontcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Style:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("filled"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("solid"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dashed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dotted"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("bold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("invis"), -1);

  node_b.style = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

  if (!strcmp (graph_blank_node.style, "filled"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.style), 0);

  else if (!strcmp (graph_blank_node.style, "solid"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.style), 1);

  else if (!strcmp (graph_blank_node.style, "dashed"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.style), 2);

  else if (!strcmp (graph_blank_node.style, "dotted"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.style), 3);

  else if (!strcmp (graph_blank_node.style, "bold"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.style), 4);

  else if (!strcmp (graph_blank_node.style, "invis"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.style), 5);

  gtk_box_pack_start (GTK_BOX (box), node_b.style, TRUE, TRUE, 0);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (node_b.style), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (node_b.style), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Fill Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_b.fillcolor = gtk_color_button_new ();
  gdk_color_parse (graph_blank_node.fillcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_b.fillcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), node_b.fillcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Shape:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("plaintext"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("ellipse"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("circle"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("egg"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("triangle"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("box"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("diamond"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("trapezium"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("parallelogram"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("house"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("hexagon"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("octagon"), -1);

  node_b.shape = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 0);
  gtk_box_pack_start (GTK_BOX (box), node_b.shape, TRUE, TRUE, 0);

  if (!strcmp (graph_blank_node.shape, "plaintext"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 0);

  else if (!strcmp (graph_blank_node.shape, "ellipse"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 1);

  else if (!strcmp (graph_blank_node.shape, "circle"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 2);

  else if (!strcmp (graph_blank_node.shape, "egg"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 3);

  else if (!strcmp (graph_blank_node.shape, "triangle"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 4);

  else if (!strcmp (graph_blank_node.shape, "box"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 5);

  else if (!strcmp (graph_blank_node.shape, "diamond"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 6);

  else if (!strcmp (graph_blank_node.shape, "trapezium"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 7);

  else if (!strcmp (graph_blank_node.shape, "parallelogram"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 8);

  else if (!strcmp (graph_blank_node.shape, "house"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 9);

  else if (!strcmp (graph_blank_node.shape, "hexagon"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 10);

  else if (!strcmp (graph_blank_node.shape, "octagon"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_b.shape), 11);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (node_b.shape), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (node_b.shape), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_b.font = gtk_font_button_new ();
  g_snprintf (s, sizeof (s), "%s %d", graph_blank_node.fontname,
	      graph_blank_node.fontsize);
  gtk_font_button_set_font_name (GTK_FONT_BUTTON (node_b.font), s);
  gtk_box_pack_start (GTK_BOX (box), node_b.font, TRUE, TRUE, 0);

  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  node_b.image = gtk_image_new ();
  g_object_set_data (G_OBJECT (node_b.image), "label", _("Blank node"));
  gtk_container_add (GTK_CONTAINER (frame), node_b.image);

  /* LITERAL */
  frame = gtk_frame_new (_("Literal nodes"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_l.color = gtk_color_button_new ();
  gdk_color_parse (graph_literal_node.color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_l.color), &color);
  gtk_box_pack_start (GTK_BOX (box), node_l.color, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_l.fontcolor = gtk_color_button_new ();
  gdk_color_parse (graph_literal_node.fontcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_l.fontcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), node_l.fontcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Style:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("filled"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("solid"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dashed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dotted"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("bold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("invis"), -1);

  node_l.style = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

  if (!strcmp (graph_literal_node.style, "filled"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.style), 0);

  else if (!strcmp (graph_literal_node.style, "solid"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.style), 1);

  else if (!strcmp (graph_literal_node.style, "dashed"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.style), 2);

  else if (!strcmp (graph_literal_node.style, "dotted"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.style), 3);

  else if (!strcmp (graph_literal_node.style, "bold"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.style), 4);

  else if (!strcmp (graph_literal_node.style, "invis"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.style), 5);

  gtk_box_pack_start (GTK_BOX (box), node_l.style, TRUE, TRUE, 0);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (node_l.style), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (node_l.style), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Fill Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_l.fillcolor = gtk_color_button_new ();
  gdk_color_parse (graph_literal_node.fillcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (node_l.fillcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), node_l.fillcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Shape:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("plaintext"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("ellipse"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("circle"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("egg"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("triangle"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("box"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("diamond"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("trapezium"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("parallelogram"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("house"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("hexagon"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("octagon"), -1);

  node_l.shape = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 0);
  gtk_box_pack_start (GTK_BOX (box), node_l.shape, TRUE, TRUE, 0);

  if (!strcmp (graph_literal_node.shape, "plaintext"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 0);

  else if (!strcmp (graph_literal_node.shape, "ellipse"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 1);

  else if (!strcmp (graph_literal_node.shape, "circle"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 2);

  else if (!strcmp (graph_literal_node.shape, "egg"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 3);

  else if (!strcmp (graph_literal_node.shape, "triangle"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 4);

  else if (!strcmp (graph_literal_node.shape, "box"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 5);

  else if (!strcmp (graph_literal_node.shape, "diamond"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 6);

  else if (!strcmp (graph_literal_node.shape, "trapezium"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 7);

  else if (!strcmp (graph_literal_node.shape, "parallelogram"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 8);

  else if (!strcmp (graph_literal_node.shape, "house"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 9);

  else if (!strcmp (graph_literal_node.shape, "hexagon"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 10);

  else if (!strcmp (graph_literal_node.shape, "octagon"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (node_l.shape), 11);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (node_l.shape), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (node_l.shape), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  node_l.font = gtk_font_button_new ();
  g_snprintf (s, sizeof (s), "%s %d", graph_literal_node.fontname,
	      graph_literal_node.fontsize);
  gtk_font_button_set_font_name (GTK_FONT_BUTTON (node_l.font), s);
  gtk_box_pack_start (GTK_BOX (box), node_l.font, TRUE, TRUE, 0);

  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  node_l.image = gtk_image_new ();
  g_object_set_data (G_OBJECT (node_l.image), "label", _("Literal"));
  gtk_container_add (GTK_CONTAINER (frame), node_l.image);

  /* EDGE */
  frame = gtk_frame_new (_("Edge"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  edge.color = gtk_color_button_new ();
  gdk_color_parse (graph_edge.color, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (edge.color), &color);
  gtk_box_pack_start (GTK_BOX (box), edge.color, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font Color:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  edge.fontcolor = gtk_color_button_new ();
  gdk_color_parse (graph_edge.fontcolor, &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (edge.fontcolor), &color);
  gtk_box_pack_start (GTK_BOX (box), edge.fontcolor, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Font:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  edge.font = gtk_font_button_new ();
  g_snprintf (s, sizeof (s), "%s %d", graph_edge.fontname,
	      graph_edge.fontsize);
  gtk_font_button_set_font_name (GTK_FONT_BUTTON (edge.font), s);
  gtk_box_pack_start (GTK_BOX (box), edge.font, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Style:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("solid"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dashed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("dotted"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("bold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("invis"), -1);

  edge.style = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

  if (!strcmp (graph_edge.style, "solid"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (edge.style), 0);

  else if (!strcmp (graph_edge.style, "dashed"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (edge.style), 1);

  else if (!strcmp (graph_edge.style, "dotted"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (edge.style), 2);

  else if (!strcmp (graph_edge.style, "bold"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (edge.style), 3);

  else if (!strcmp (graph_edge.style, "invis"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (edge.style), 4);

  gtk_box_pack_start (GTK_BOX (box), edge.style, TRUE, TRUE, 0);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (edge.style), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (edge.style), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  edge.image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (frame), edge.image);

  node_r.renderer = create_node;
  node_b.renderer = create_node;
  node_l.renderer = create_node;
  edge.renderer = create_edge;

  wait = dialog_wait (NULL, NULL, &pb);

  preferences_renderer (&node_r, pb);
  preferences_renderer (&node_b, pb);
  preferences_renderer (&node_l, pb);
  preferences_renderer (&edge, pb);

  gtk_widget_destroy (wait);

  create_object (&node_r);
  create_object (&node_b);
  create_object (&node_l);
  create_object (&edge);

  /* FORMAT */
  frame = gtk_frame_new (_("Output format"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new (_("Format:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("png"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("jpg"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("gif"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("ps"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("svg"), -1);

  format = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

  if (!strcmp (graph_format, "png"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (format), 0);

  else if (!strcmp (graph_format, "jpg"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (format), 1);

  else if (!strcmp (graph_format, "gif"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (format), 2);

  else if (!strcmp (graph_format, "ps"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (format), 3);

  else if (!strcmp (graph_format, "svg"))
    gtk_combo_box_set_active (GTK_COMBO_BOX (format), 4);

  gtk_box_pack_start (GTK_BOX (hbox), format, TRUE, TRUE, 0);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (format), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (format), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  /* NETWORK */
  gbox = gtk_vbox_new (FALSE, 8);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gbox,
			    gtk_label_new (_("Network")));

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  proxy = gtk_check_button_new_with_label (_("Proxy"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), proxy);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

#ifdef USE_GCONF
  proxy_gconf = gtk_check_button_new ();
  g_object_set_data (G_OBJECT (proxy), "gconf", proxy_gconf);

  g_signal_connect_after ((gpointer) proxy, "toggled",
			  G_CALLBACK (preferences_proxy), hbox);
#else
  g_signal_connect_after ((gpointer) proxy, "toggled",
			  G_CALLBACK (preferences_switch), hbox);
#endif

  if (!download_proxy)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy), FALSE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy), TRUE);

#ifdef USE_GCONF
  preferences_proxy (proxy, hbox);
#else
  preferences_switch (proxy, hbox);
#endif

  vbox = gtk_vbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Proxy Server:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  proxy_server = gtk_entry_new ();
  if (download_proxy)
    gtk_entry_set_text (GTK_ENTRY (proxy_server), download_proxy);
  gtk_box_pack_start (GTK_BOX (box), proxy_server, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Proxy Port:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  adj =
    gtk_adjustment_new (download_proxy_port ? download_proxy_port : 8080, 1,
			65536, 1, 10, 100);
  proxy_port = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (box), proxy_port, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Proxy User:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  proxy_user = gtk_entry_new ();
  if (download_proxy_user)
    gtk_entry_set_text (GTK_ENTRY (proxy_user), download_proxy_user);
  gtk_box_pack_start (GTK_BOX (box), proxy_user, TRUE, TRUE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

  label = gtk_label_new (_("Proxy Password:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  proxy_password = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (proxy_password), FALSE);
  if (download_proxy_password)
    gtk_entry_set_text (GTK_ENTRY (proxy_password), download_proxy_password);
  gtk_box_pack_start (GTK_BOX (box), proxy_password, TRUE, TRUE, 0);

#ifdef USE_GCONF
  frame = gtk_frame_new (_("Gnome"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  box = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), box);

  label = gtk_label_new (_("Use Gnome Setting:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (box), proxy_gconf, TRUE, TRUE, 0);
  g_signal_connect_after ((gpointer) proxy_gconf, "toggled",
			  G_CALLBACK (preferences_proxy_gconf), proxy);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy_gconf),
				download_proxy_gconf);
#endif

  /* HIGHLIGHTING */

  gbox = gtk_vbox_new (FALSE, 8);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gbox,
			    gtk_label_new (_("Highlighting")));

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);

  highlighting = gtk_check_button_new_with_label (_("Highlighting"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), highlighting);

  hbox = gtk_hbox_new (TRUE, 5);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  g_signal_connect_after ((gpointer) highlighting, "toggled",
			  G_CALLBACK (preferences_switch), hbox);

  if (!highlighting_active)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (highlighting), FALSE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (highlighting), TRUE);

  preferences_switch (highlighting, hbox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  store =
    gtk_list_store_new (9, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
			G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_INT);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();

  column =
    gtk_tree_view_column_new_with_attributes (_("Tags"), renderer, "text", 0,
					      NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, FALSE);

  for (list = highlighting_tags; list; list = list->next)
    {
      GtkTreeIter iter;
      struct tag_t *tag = list->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, tag->tag, 1, tag->style, 2,
			  tag->variant, 3, tag->weight, 4, tag->stretch, 5,
			  tag->scale, 6, tag->foreground, 7, tag->background,
			  8, tag->underline, -1);
    }

  g_object_unref (store);

  button = gtk_button_new_from_stock ("gtk-add");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (preferences_highlighting_add), treeview);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (preferences_highlighting_rename), treeview);

  box = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (button), box);

  gtk_box_pack_start (GTK_BOX (box), gtk_event_box_new (), TRUE, TRUE, 0);

  label = gtk_image_new_from_stock ("gtk-refresh", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  label = gtk_label_new (_("Rename"));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (box), gtk_event_box_new (), TRUE, TRUE, 0);

  button = gtk_button_new_from_stock ("gtk-remove");
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (preferences_highlighting_remove), treeview);

  gbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), gbox, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (gbox), frame, TRUE, TRUE, 0);
  g_signal_connect ((gpointer) selection, "changed",
		    G_CALLBACK (preferences_highlighting_selection), gbox);

  if (highlighting_tags)
    gtk_widget_set_sensitive (frame, TRUE);
  else
    gtk_widget_set_sensitive (frame, FALSE);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Style:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Normal"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Oblique"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Italic"), -1);

  button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "style", button);
  g_signal_connect ((gpointer) button, "changed",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Variant:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Normal"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Small Caps"), -1);

  button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "variant", button);
  g_signal_connect ((gpointer) button, "changed",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Weight:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Ultralight"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Light"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Normal"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Semibold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Bold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Ultrabold"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Heavy"), -1);

  button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "weight", button);
  g_signal_connect ((gpointer) button, "changed",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Stretch:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Ultra Condensed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Extra Condensed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Condensed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Semi Condensed"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Normal"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Semi Expanded"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Expanded"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Extra Expanded"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Ultra Expanded"), -1);

  button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "stretch", button);
  g_signal_connect ((gpointer) button, "changed",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Scale:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("XX Small"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("X Small"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Small"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Normal"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("X Large"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("XX Large"), -1);

  button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "scale", button);
  g_signal_connect ((gpointer) button, "changed",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Foreground:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  button = gtk_color_button_new ();
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "foreground", button);
  g_signal_connect ((gpointer) button, "color-set",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Background:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  button = gtk_color_button_new ();
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "background", button);
  g_signal_connect ((gpointer) button, "color-set",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  box = gtk_hbox_new (TRUE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  label = gtk_label_new (_("Underline:"));
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("None"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Single"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, _("Double"), -1);

  button = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_object_set_data (G_OBJECT (treeview), "underline", button);
  g_signal_connect ((gpointer) button, "changed",
		    G_CALLBACK (preferences_highlighting_save), treeview);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (button), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (button), renderer,
				  "text", 0, NULL);

  g_object_unref (store);

  button = gtk_button_new_with_label (_("Reset the default tags"));
  gtk_box_pack_start (GTK_BOX (gbox), button, FALSE, FALSE, 0);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (preferences_highlighting_reset), treeview);

  frame = gtk_frame_new (_("Demo"));
  gtk_box_pack_start (GTK_BOX (gbox), frame, FALSE, FALSE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  highlighting_demo = textview_new (NULL, NULL);
  g_object_set_data (G_OBJECT (treeview), "demo", highlighting_demo);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), highlighting_demo);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (highlighting_demo), FALSE);
  gtk_widget_set_size_request (highlighting_demo, -1, 150);

  store = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter) == TRUE)
    {
      gtk_tree_selection_select_iter (selection, &iter);
      preferences_highlighting_selection (selection, gbox);
    }
  else
    {
      gtk_tree_selection_unselect_all (selection);
      preferences_highlighting_selection (selection, gbox);
    }

  /* OTHER */
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
      gboolean need_refresh = 0;
      gchar *b_text, *v_text, *u_text, *h_text, *bn_text;
      gchar *tmp;
      GdkColor c;
      gchar *fontname;
      gchar *c_text;
      gint i;

      /* SOFTWARES */
      b_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (browser));
      v_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (viewer));
      u_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (update));
      bn_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (blanknode));
      h_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (help));

      if (!b_text || !*b_text)
	{
	  dialog_msg (_("No a compatible web browser!"));
	  continue;
	}

      if (!v_text || !*v_text)
	{
	  dialog_msg (_("No a compatible image viewer!"));
	  continue;
	}

      if (!u_text || !*u_text)
	{
	  dialog_msg (_("No a compatible update server!"));
	  continue;
	}

      if (!h_text || !*h_text)
	{
	  dialog_msg (_("No a compatible documentation server!"));
	  continue;
	}

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (proxy)) == TRUE
	  && (!(tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (proxy_server)))
	      || !*tmp))
	{
	  dialog_msg (_("No a valid proxy value!"));
	  continue;
	}

      g_free (browser_default);
      browser_default = g_strdup (b_text);

      g_free (viewer_default);
      viewer_default = g_strdup (v_text);

      undo_max_value =
	gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (undo));
      unredo_change_value ();

      last_document_value =
	gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (last_document));
      last_change_value ();

      g_free (update_url);
      update_url = g_strdup (u_text);

      g_free (help_url);
      help_url = g_strdup (h_text);

      g_free (blanknode_name);
      if (!bn_text || !*bn_text)
	blanknode_name = g_strdup (BLANK_NODE);
      else
	blanknode_name = g_strdup (bn_text);

      i = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (updated));

      if (i == FALSE)
	update_show = -1;
      else if (update_show != 0)
	update_show = 1;

      automatic_extensions =
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (extensions));

      /* COLOR */
      gtk_color_button_get_color (GTK_COLOR_BUTTON (nbg_color), &c);
      c_text =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      if (strcmp (normal_bg_color, c_text))
	{
	  g_free (normal_bg_color);
	  normal_bg_color = c_text;
	  need_refresh = 1;
	}
      else
	g_free (c_text);

      gtk_color_button_get_color (GTK_COLOR_BUTTON (nfg_color), &c);
      c_text =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      if (strcmp (normal_fg_color, c_text))
	{
	  g_free (normal_fg_color);
	  normal_fg_color = c_text;
	  need_refresh = 1;
	}
      else
	g_free (c_text);

      gtk_color_button_get_color (GTK_COLOR_BUTTON (pbg_color), &c);
      c_text =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      if (strcmp (prelight_bg_color, c_text))
	{
	  g_free (prelight_bg_color);
	  prelight_bg_color = c_text;
	  need_refresh = 1;
	}
      else
	g_free (c_text);

      gtk_color_button_get_color (GTK_COLOR_BUTTON (pfg_color), &c);
      c_text =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      if (strcmp (prelight_fg_color, c_text))
	{
	  g_free (prelight_fg_color);
	  prelight_fg_color = c_text;
	  need_refresh = 1;
	}
      else
	g_free (c_text);

      /* RESOURCE */
      g_free (graph_resource_node.color);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_r.color), &c);
      graph_resource_node.color =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_resource_node.fontcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_r.fontcolor), &c);
      graph_resource_node.fontcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_resource_node.style);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node_r.style)))
	{
	case 0:
	  graph_resource_node.style = g_strdup ("filled");
	  break;
	case 1:
	  graph_resource_node.style = g_strdup ("solid");
	  break;
	case 2:
	  graph_resource_node.style = g_strdup ("dashed");
	  break;
	case 3:
	  graph_resource_node.style = g_strdup ("dotted");
	  break;
	case 4:
	  graph_resource_node.style = g_strdup ("bold");
	  break;
	default:
	  graph_resource_node.style = g_strdup ("invis");
	  break;
	}

      g_free (graph_resource_node.fillcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_r.fillcolor), &c);
      graph_resource_node.fillcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_resource_node.shape);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node_r.shape)))
	{
	case 0:
	  graph_resource_node.shape = g_strdup ("plaintext");
	  break;
	case 1:
	  graph_resource_node.shape = g_strdup ("ellipse");
	  break;
	case 2:
	  graph_resource_node.shape = g_strdup ("circle");
	  break;
	case 3:
	  graph_resource_node.shape = g_strdup ("egg");
	  break;
	case 4:
	  graph_resource_node.shape = g_strdup ("triangle");
	  break;
	case 5:
	  graph_resource_node.shape = g_strdup ("box");
	  break;
	case 6:
	  graph_resource_node.shape = g_strdup ("diamond");
	  break;
	case 7:
	  graph_resource_node.shape = g_strdup ("trapezium");
	  break;
	case 8:
	  graph_resource_node.shape = g_strdup ("parallelogram");
	  break;
	case 9:
	  graph_resource_node.shape = g_strdup ("house");
	  break;
	case 10:
	  graph_resource_node.shape = g_strdup ("hexagon");
	  break;
	default:
	  graph_resource_node.shape = g_strdup ("octagon");
	  break;
	}

      fontname =
	g_strdup (gtk_font_button_get_font_name
		  (GTK_FONT_BUTTON (node_r.font)));

      for (i = strlen (fontname); i != 0; i--)
	if (fontname[i] == ' ')
	  break;

      if (i)
	{
	  graph_resource_node.fontsize = atoi (fontname + i + 1);
	  fontname[i] = 0;
	}

      g_free (graph_resource_node.fontname);
      graph_resource_node.fontname = g_strdup (fontname);
      g_free (fontname);

      /* BLANK */
      g_free (graph_blank_node.color);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_b.color), &c);
      graph_blank_node.color =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_blank_node.fontcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_b.fontcolor), &c);
      graph_blank_node.fontcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_blank_node.style);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node_b.style)))
	{
	case 0:
	  graph_blank_node.style = g_strdup ("filled");
	  break;
	case 1:
	  graph_blank_node.style = g_strdup ("solid");
	  break;
	case 2:
	  graph_blank_node.style = g_strdup ("dashed");
	  break;
	case 3:
	  graph_blank_node.style = g_strdup ("dotted");
	  break;
	case 4:
	  graph_blank_node.style = g_strdup ("bold");
	  break;
	default:
	  graph_blank_node.style = g_strdup ("invis");
	  break;
	}

      g_free (graph_blank_node.fillcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_b.fillcolor), &c);
      graph_blank_node.fillcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_blank_node.shape);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node_b.shape)))
	{
	case 0:
	  graph_blank_node.shape = g_strdup ("plaintext");
	  break;
	case 1:
	  graph_blank_node.shape = g_strdup ("ellipse");
	  break;
	case 2:
	  graph_blank_node.shape = g_strdup ("circle");
	  break;
	case 3:
	  graph_blank_node.shape = g_strdup ("egg");
	  break;
	case 4:
	  graph_blank_node.shape = g_strdup ("triangle");
	  break;
	case 5:
	  graph_blank_node.shape = g_strdup ("box");
	  break;
	case 6:
	  graph_blank_node.shape = g_strdup ("diamond");
	  break;
	case 7:
	  graph_blank_node.shape = g_strdup ("trapezium");
	  break;
	case 8:
	  graph_blank_node.shape = g_strdup ("parallelogram");
	  break;
	case 9:
	  graph_blank_node.shape = g_strdup ("house");
	  break;
	case 10:
	  graph_blank_node.shape = g_strdup ("hexagon");
	  break;
	default:
	  graph_blank_node.shape = g_strdup ("octagon");
	  break;
	}

      fontname =
	g_strdup (gtk_font_button_get_font_name
		  (GTK_FONT_BUTTON (node_b.font)));

      for (i = strlen (fontname); i != 0; i--)
	if (fontname[i] == ' ')
	  break;

      if (i)
	{
	  graph_blank_node.fontsize = atoi (fontname + i + 1);
	  fontname[i] = 0;
	}

      g_free (graph_blank_node.fontname);
      graph_blank_node.fontname = g_strdup (fontname);
      g_free (fontname);

      /* LITERAL */
      g_free (graph_literal_node.color);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_l.color), &c);
      graph_literal_node.color =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_literal_node.fontcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_l.fontcolor), &c);
      graph_literal_node.fontcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_literal_node.style);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node_l.style)))
	{
	case 0:
	  graph_literal_node.style = g_strdup ("filled");
	  break;
	case 1:
	  graph_literal_node.style = g_strdup ("solid");
	  break;
	case 2:
	  graph_literal_node.style = g_strdup ("dashed");
	  break;
	case 3:
	  graph_literal_node.style = g_strdup ("dotted");
	  break;
	case 4:
	  graph_literal_node.style = g_strdup ("bold");
	  break;
	default:
	  graph_literal_node.style = g_strdup ("invis");
	  break;
	}

      g_free (graph_literal_node.fillcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (node_l.fillcolor), &c);
      graph_literal_node.fillcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_literal_node.shape);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node_l.shape)))
	{
	case 0:
	  graph_literal_node.shape = g_strdup ("plaintext");
	  break;
	case 1:
	  graph_literal_node.shape = g_strdup ("ellipse");
	  break;
	case 2:
	  graph_literal_node.shape = g_strdup ("circle");
	  break;
	case 3:
	  graph_literal_node.shape = g_strdup ("egg");
	  break;
	case 4:
	  graph_literal_node.shape = g_strdup ("triangle");
	  break;
	case 5:
	  graph_literal_node.shape = g_strdup ("box");
	  break;
	case 6:
	  graph_literal_node.shape = g_strdup ("diamond");
	  break;
	case 7:
	  graph_literal_node.shape = g_strdup ("trapezium");
	  break;
	case 8:
	  graph_literal_node.shape = g_strdup ("parallelogram");
	  break;
	case 9:
	  graph_literal_node.shape = g_strdup ("house");
	  break;
	case 10:
	  graph_literal_node.shape = g_strdup ("hexagon");
	  break;
	default:
	  graph_literal_node.shape = g_strdup ("octagon");
	  break;
	}

      fontname =
	g_strdup (gtk_font_button_get_font_name
		  (GTK_FONT_BUTTON (node_l.font)));

      for (i = strlen (fontname); i != 0; i--)
	if (fontname[i] == ' ')
	  break;

      if (i)
	{
	  graph_literal_node.fontsize = atoi (fontname + i + 1);
	  fontname[i] = 0;
	}

      g_free (graph_literal_node.fontname);
      graph_literal_node.fontname = g_strdup (fontname);
      g_free (fontname);

      /* EDGE */
      g_free (graph_edge.color);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (edge.color), &c);
      graph_edge.color =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_edge.fontcolor);
      gtk_color_button_get_color (GTK_COLOR_BUTTON (edge.fontcolor), &c);
      graph_edge.fontcolor =
	g_strdup_printf ("#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
			 (c.blue >> 8));

      g_free (graph_edge.style);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (edge.style)))
	{
	case 0:
	  graph_edge.style = g_strdup ("solid");
	  break;
	case 1:
	  graph_edge.style = g_strdup ("dashed");
	  break;
	case 2:
	  graph_edge.style = g_strdup ("dotted");
	  break;
	case 3:
	  graph_edge.style = g_strdup ("bold");
	  break;
	default:
	  graph_edge.style = g_strdup ("invis");
	  break;
	}

      fontname =
	g_strdup (gtk_font_button_get_font_name
		  (GTK_FONT_BUTTON (edge.font)));

      for (i = strlen (fontname); i != 0; i--)
	if (fontname[i] == ' ')
	  break;

      if (i)
	{
	  graph_edge.fontsize = atoi (fontname + i + 1);
	  fontname[i] = 0;
	}

      g_free (graph_edge.fontname);
      graph_edge.fontname = g_strdup (fontname);
      g_free (fontname);

      /* FORMAT */
      g_free (graph_format);

      switch (gtk_combo_box_get_active (GTK_COMBO_BOX (format)))
	{
	case 0:
	  graph_format = g_strdup ("png");
	  break;
	case 1:
	  graph_format = g_strdup ("jpg");
	  break;
	case 2:
	  graph_format = g_strdup ("gif");
	  break;
	case 3:
	  graph_format = g_strdup ("ps");
	  break;
	default:
	  graph_format = g_strdup ("svg");
	  break;
	}

      if (need_refresh)
	{
	  struct editor_data_t *data;

	  data = editor_data;

	  while (data)
	    {
	      editor_refresh (data);
	      data = data->next;
	    }
	}

      /* NETWORK */
#ifdef USE_GCONF
      download_proxy_gconf =
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (proxy_gconf));
#endif

      if (!(tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (proxy_server)))
	  || !*tmp)
	{
	  if (download_proxy)
	    {
	      g_free (download_proxy);
	      download_proxy = NULL;
	    }
	}
      else
	{
	  if (download_proxy)
	    g_free (download_proxy);

	  download_proxy = g_strdup (tmp);
	}

      download_proxy_port =
	gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (proxy_port));

      if (!(tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (proxy_user)))
	  || !*tmp)
	{
	  if (download_proxy_user)
	    {
	      g_free (download_proxy_user);
	      download_proxy_user = NULL;
	    }
	}
      else
	{
	  if (download_proxy_user)
	    g_free (download_proxy_user);

	  download_proxy_user = g_strdup (tmp);
	}

      if (!(tmp = (gchar *) gtk_entry_get_text (GTK_ENTRY (proxy_password)))
	  || !*tmp)
	{
	  if (download_proxy_password)
	    {
	      g_free (download_proxy_password);
	      download_proxy_password = NULL;
	    }
	}
      else
	{
	  if (download_proxy_password)
	    g_free (download_proxy_password);

	  download_proxy_password = g_strdup (tmp);
	}

      /* HIGHLIGHTING */
      highlighting_active =
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (highlighting));

      if ((store =
	   (GtkListStore *)
	   gtk_tree_view_get_model (GTK_TREE_VIEW (treeview))))
	{
	  GtkTreeIter iter;

	  preferences_highlighting_free ();

	  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter) ==
	      TRUE)
	    {
	      struct tag_t *tag;

	      do
		{
		  tag = g_malloc0 (sizeof (struct tag_t));
		  highlighting_tags = g_list_append (highlighting_tags, tag);

		  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0,
				      &tag->tag, 1, &tag->style, 2,
				      &tag->variant, 3, &tag->weight, 4,
				      &tag->stretch, 5, &tag->scale, 6,
				      &tag->foreground, 7, &tag->background,
				      8, &tag->underline, -1);
		}
	      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter)
		     == TRUE);
	    }
	}

      init_save ();
      break;
    }

  gtk_widget_destroy (dialog);
}

/* Crea un nodo di preview date tutti una serie di parametri: */
static void
create_node (struct preferences_node_t *node, GtkWidget * pb)
{
  gchar *tmp, *file;
  GdkColor c;
  gchar *label = g_object_get_data (G_OBJECT (node->image), "label");
  gchar *fontname;
  gint i;
  FILE *fl;
  gchar buf[1024];
  gchar *array[6];

  /* Scrivo il sorgente per dot: */
  tmp = editor_tmp_file ();

  if (!(fl = g_fopen (tmp, "wb")))
    {
      g_free (tmp);
      return;
    }

  fprintf (fl, "digraph dotfile {\nrankdir=LR;\n\n");

  fprintf (fl, "\"%s\" [", label);

  gtk_color_button_get_color (GTK_COLOR_BUTTON (node->color), &c);
  fprintf (fl, "color=\"#%.2X%.2X%.2X\",", (c.red >> 8), (c.green >> 8),
	   (c.blue >> 8));

  gtk_color_button_get_color (GTK_COLOR_BUTTON (node->fontcolor), &c);
  fprintf (fl, "fontcolor=\"#%.2X%.2X%.2X\",", (c.red >> 8), (c.green >> 8),
	   (c.blue >> 8));

  gtk_color_button_get_color (GTK_COLOR_BUTTON (node->fillcolor), &c);
  fprintf (fl, "fillcolor=\"#%.2X%.2X%.2X\",", (c.red >> 8), (c.green >> 8),
	   (c.blue >> 8));

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node->style)))
    {
    case 0:
      fprintf (fl, "style=\"filled\",");
      break;
    case 1:
      fprintf (fl, "style=\"solid\",");
      break;
    case 2:
      fprintf (fl, "style=\"dashed\",");
      break;
    case 3:
      fprintf (fl, "style=\"dotted\",");
      break;
    case 4:
      fprintf (fl, "style=\"bold\",");
      break;
    case 5:
      fprintf (fl, "style=\"invis\",");
      break;
    }

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node->shape)))
    {
    case 0:
      fprintf (fl, "shape=\"plaintext\",");
      break;
    case 1:
      fprintf (fl, "shape=\"ellipse\",");
      break;
    case 2:
      fprintf (fl, "shape=\"circle\",");
      break;
    case 3:
      fprintf (fl, "shape=\"egg\",");
      break;
    case 4:
      fprintf (fl, "shape=\"triangle\",");
      break;
    case 5:
      fprintf (fl, "shape=\"box\",");
      break;
    case 6:
      fprintf (fl, "shape=\"diamond\",");
      break;
    case 7:
      fprintf (fl, "shape=\"trapezium\",");
      break;
    case 8:
      fprintf (fl, "shape=\"parallelogram\",");
      break;
    case 9:
      fprintf (fl, "shape=\"house\",");
      break;
    case 10:
      fprintf (fl, "shape=\"hexagon\",");
      break;
    case 11:
      fprintf (fl, "shape=\"octagon\",");
      break;
    }

  fontname =
    g_strdup (gtk_font_button_get_font_name (GTK_FONT_BUTTON (node->font)));

  for (i = strlen (fontname); i != 0; i--)
    if (fontname[i] == ' ')
      break;

  if (i)
    {
      fprintf (fl, "fontsize=\"%d\",", atoi (fontname + i + 1));
      fontname[i] = 0;
    }

  fprintf (fl, "fontname=\"%s\"];\n", fontname);
  g_free (fontname);

  fprintf (fl, "\n}\n\n");

  fclose (fl);

  file = editor_tmp_file ();
  g_snprintf (buf, sizeof (buf), "%s.png", file);
  g_free (file);
  file = g_strdup (buf);

  /* Eseguo il comando: */
  array[0] = dot_cmd ();
  array[1] = "-Tpng";
  array[2] = "-o";
  array[3] = file;
  array[4] = tmp;
  array[5] = NULL;

  if (preferences_wait (array, pb))
    {
      g_free (file);
      file = NULL;
    }

  /* Rimuovo il file: */
  g_remove (tmp);
  g_free (tmp);

  /* Mostro l'immagine e libero la memoria: */
  if (file)
    {
      gtk_image_set_from_file (GTK_IMAGE (node->image), file);
      g_remove (file);
      g_free (file);
    }
}

/* Setta tutte le variabili all'interno dei vari oggetti: */
static void
create_object (struct preferences_node_t *node)
{
  g_signal_connect ((gpointer) node->color, "color_set",
		    G_CALLBACK (conf_refresh), node);

  g_signal_connect ((gpointer) node->fontcolor, "color_set",
		    G_CALLBACK (conf_refresh), node);

  g_signal_connect ((gpointer) node->style, "changed",
		    G_CALLBACK (conf_refresh), node);

  if (node->fillcolor)
    g_signal_connect ((gpointer) node->fillcolor, "color_set",
		      G_CALLBACK (conf_refresh), node);

  if (node->shape)
    g_signal_connect ((gpointer) node->shape, "changed",
		      G_CALLBACK (conf_refresh), node);

  g_signal_connect ((gpointer) node->font, "font_set",
		    G_CALLBACK (conf_refresh), node);
}

/* Questa funzione usa dot per creare un esempio di arco: */
static void
create_edge (struct preferences_node_t *node, GtkWidget * pb)
{
  gchar *tmp, *file;
  GdkColor c;
  gchar *fontname;
  gint i;
  FILE *fl;
  gchar buf[1024];
  gchar *array[6];

  tmp = editor_tmp_file ();

  if (!(fl = g_fopen (tmp, "wb")))
    {
      g_free (tmp);
      return;
    }

  fprintf (fl, "digraph dotfile {\nrankdir=LR;\n\n");

  fprintf (fl, "\"A\" -> \"B\" [label=\"edge\"");

  gtk_color_button_get_color (GTK_COLOR_BUTTON (node->color), &c);
  fprintf (fl, "color=\"#%.2X%.2X%.2X\",", (c.red >> 8), (c.green >> 8),
	   (c.blue >> 8));

  gtk_color_button_get_color (GTK_COLOR_BUTTON (node->fontcolor), &c);
  fprintf (fl, "fontcolor=\"#%.2X%.2X%.2X\",", (c.red >> 8), (c.green >> 8),
	   (c.blue >> 8));

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (node->style)))
    {
    case 0:
      fprintf (fl, "style=\"solid\",");
      break;
    case 1:
      fprintf (fl, "style=\"dashed\",");
      break;
    case 2:
      fprintf (fl, "style=\"dotted\",");
      break;
    case 3:
      fprintf (fl, "style=\"bold\",");
      break;
    case 4:
      fprintf (fl, "style=\"invis\",");
      break;
    }

  fontname =
    g_strdup (gtk_font_button_get_font_name (GTK_FONT_BUTTON (node->font)));

  for (i = strlen (fontname); i != 0; i--)
    if (fontname[i] == ' ')
      break;

  if (i)
    {
      fprintf (fl, "fontsize=\"%d\",", atoi (fontname + i + 1));
      fontname[i] = 0;
    }

  fprintf (fl, "fontname=\"%s\"];\n", fontname);
  g_free (fontname);

  fprintf (fl, "\n}\n\n");

  fclose (fl);

  file = editor_tmp_file ();
  g_snprintf (buf, sizeof (buf), "%s.png", file);
  g_free (file);
  file = g_strdup (buf);

  array[0] = dot_cmd ();
  array[1] = "-Tpng";
  array[2] = "-o";
  array[3] = file;
  array[4] = tmp;
  array[5] = NULL;

  if (preferences_wait (array, pb))
    {
      g_free (file);
      file = NULL;
    }

  g_remove (tmp);
  g_free (tmp);

  if (file)
    {
      gtk_image_set_from_file (GTK_IMAGE (node->image), file);
      g_remove (file);
      g_free (file);
    }
}

/* Questa funzione preleva i dati che servono per poi lanciare la funzione
 * che genera la preview di colori */
static void
conf_refresh (GtkWidget * w, struct preferences_node_t *node)
{
  preferences_renderer (node, NULL);
}

/* Questa funzione genera la preview dei colori */
static void
color_refresh (GtkWidget * w, gpointer dummy)
{
  GtkWidget *ebox;
  GtkWidget *label;
  GtkWidget *tmp;

  /* Prima creo: */
  ebox = gtk_event_box_new ();
  g_object_set_data (G_OBJECT (ebox), "nbg_color",
		     g_object_get_data (G_OBJECT (w), "nbg_color"));
  g_object_set_data (G_OBJECT (ebox), "nfg_color",
		     g_object_get_data (G_OBJECT (w), "nfg_color"));
  g_object_set_data (G_OBJECT (ebox), "pbg_color",
		     g_object_get_data (G_OBJECT (w), "pbg_color"));
  g_object_set_data (G_OBJECT (ebox), "pfg_color",
		     g_object_get_data (G_OBJECT (w), "pfg_color"));

  gtk_widget_set_events (ebox,
			 GDK_BUTTON_PRESS_MASK |
			 GDK_KEY_PRESS_MASK |
			 GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  gtk_widget_set_can_focus(ebox, TRUE);

  gtk_widget_show (ebox);

  label = gtk_label_new (_("<b>Preview for these colors</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  g_signal_connect (G_OBJECT (ebox), "enter-notify-event",
		    G_CALLBACK (on_ebox_enter), label);
  g_signal_connect (G_OBJECT (ebox), "leave-notify-event",
		    G_CALLBACK (on_ebox_leave), label);

  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (ebox), label);

  g_signal_connect (G_OBJECT (ebox), "button-press-event",
		    G_CALLBACK (on_ebox_press), NULL);

  tmp = g_object_get_data (G_OBJECT (w), "frame");

  /* Rimovo il precedente: */
  gtk_container_foreach (GTK_CONTAINER (tmp),
			 (GtkCallback) gtk_widget_destroy, NULL);

  g_object_set_data (G_OBJECT (ebox), "selected", ebox);
  on_ebox_press (ebox);

  /* Inserisco: */
  gtk_container_add (GTK_CONTAINER (tmp), ebox);
}

/* Come in maker.c */
static gboolean
on_ebox_enter (GtkWidget * box, GdkEventCrossing * event, GtkLabel * label)
{
  static GdkCursor *Cursor = NULL;

  (void) event;

  gtk_widget_set_state (GTK_WIDGET (label), GTK_STATE_PRELIGHT);

  if (!Cursor)
    Cursor = gdk_cursor_new (GDK_HAND2);

  gdk_window_set_cursor (box->window, Cursor);
  return TRUE;
}

/* Come in maker.c */
static gboolean
on_ebox_leave (GtkWidget * box, GdkEventCrossing * event, GtkLabel * label)
{
  (void) event;

  gtk_widget_set_state (GTK_WIDGET (label), GTK_STATE_NORMAL);
  gdk_window_set_cursor (box->window, NULL);
  return TRUE;
}

/* Questa fa switchare i colori: */
static void
on_ebox_press (GtkWidget * box)
{
  GtkStyle *style;
  GdkColor color;
  GtkWidget *tmp;

  style = gtk_style_copy (gtk_widget_get_default_style ());

  if (g_object_get_data (G_OBJECT (box), "selected"))
    {
      tmp = g_object_get_data (G_OBJECT (box), "nbg_color");
      gtk_color_button_get_color (GTK_COLOR_BUTTON (tmp), &color);
      style->bg[GTK_STATE_NORMAL] = color;

      tmp = g_object_get_data (G_OBJECT (box), "nfg_color");
      gtk_color_button_get_color (GTK_COLOR_BUTTON (tmp), &color);
      style->fg[GTK_STATE_NORMAL] = color;

      tmp = g_object_get_data (G_OBJECT (box), "pfg_color");
      gtk_color_button_get_color (GTK_COLOR_BUTTON (tmp), &color);
      style->fg[GTK_STATE_PRELIGHT] = color;

      g_object_steal_data (G_OBJECT (box), "selected");
    }

  else
    {
      tmp = g_object_get_data (G_OBJECT (box), "pbg_color");
      gtk_color_button_get_color (GTK_COLOR_BUTTON (tmp), &color);
      style->bg[GTK_STATE_NORMAL] = color;

      tmp = g_object_get_data (G_OBJECT (box), "nfg_color");
      gtk_color_button_get_color (GTK_COLOR_BUTTON (tmp), &color);
      style->fg[GTK_STATE_NORMAL] = color;

      tmp = g_object_get_data (G_OBJECT (box), "pfg_color");
      gtk_color_button_get_color (GTK_COLOR_BUTTON (tmp), &color);
      style->fg[GTK_STATE_PRELIGHT] = color;

      g_object_set_data (G_OBJECT (box), "selected", box);
    }

  set_style (box, style);
  g_object_unref (G_OBJECT (style));
}

/* Come in maker.c */
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

/* Questa funzione viene lanciata al termine del comando dot */
static void
preferences_wait_cb (GPid pid, gint status, gint * id)
{
  if (status == 0)
    *id = 1;
  else
    *id = -1;
}

static gboolean
preferences_wait (gchar ** array, GtkWidget * pb)
{
  GtkWidget *window = NULL;
  GPid pid;
  gint end;
  gboolean done;

  done = pb ? FALSE : TRUE;

  if (done)
    window = dialog_wait (NULL, NULL, &pb);

  if (g_spawn_async
      (NULL, array, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH |
       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
       &pid, NULL) == FALSE)
    {
      return 1;
    }

  end = 0;
  g_child_watch_add (pid, (GChildWatchFunc) preferences_wait_cb, &end);

  /* Mi limito ad aspettare il termine dell'altro processo preoccupandomi 
   * solo di aggiornare l'interfaccia: */
  while (!end)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pb));

      while (gtk_events_pending ())
	gtk_main_iteration ();

      g_usleep (50000);
    }

  /* Distruggo la finestra di attesa: */
  if (done)
    gtk_widget_destroy (window);

  return end > 0 ? 0 : 1;
}

static void
preferences_renderer (struct preferences_node_t *node, GtkWidget * pb)
{
  node->renderer (node, pb);
}

static void
preferences_switch (GtkWidget * w, GtkWidget * box)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)) == TRUE)
    gtk_widget_set_sensitive (box, TRUE);
  else
    gtk_widget_set_sensitive (box, FALSE);
}

static void
preferences_highlighting_free (void)
{
  g_list_foreach (highlighting_tags,
		  (GFunc) preferences_highlighting_free_single, NULL);
  g_list_free (highlighting_tags);

  highlighting_tags = NULL;
}

static void
preferences_highlighting_free_single (struct tag_t *tag)
{
  if (!tag)
    return;

  if (tag->tag)
    g_free (tag->tag);

  if (tag->foreground)
    g_free (tag->foreground);

  if (tag->background)
    g_free (tag->background);

  g_free (tag);
}

static gchar *
preferences_highlighting_add_tag (GtkWindow * parent)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;
  gchar s[1024];

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION, _("New Tag..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE,
		      0);

  label = gtk_label_new (_("New Tag"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_NO);

  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  gtk_widget_show_all (dialog);

  while (1)
    {
      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
	{
	  gchar *what = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

	  if (!what || !*what)
	    {
	      dialog_msg (_("The tag cann't be empty!"));
	      continue;
	    }

	  what = g_strdup (what);
	  gtk_widget_destroy (dialog);
	  return what;
	}

      else
	{
	  gtk_widget_destroy (dialog);
	  return NULL;
	}
    }
}

static void
preferences_highlighting_add (GtkWidget * button, GtkWidget * treeview)
{
  GtkTreeIter iter;
  gchar *tag;
  GtkListStore *store;

  if (!
      (tag =
       preferences_highlighting_add_tag (GTK_WINDOW
					 (gtk_widget_get_toplevel (button)))))
    return;

  store = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, tag, 1, PANGO_STYLE_NORMAL, 2,
		      PANGO_VARIANT_NORMAL, 3, PANGO_WEIGHT_NORMAL, 4,
		      PANGO_STRETCH_NORMAL, 5, PANGO_SCALE_MEDIUM, 6, NULL, 7,
		      NULL, 8, PANGO_UNDERLINE_NONE, -1);

  g_free (tag);
}

static void
preferences_highlighting_rename (GtkWidget * button, GtkWidget * treeview)
{
  gchar *tag;
  GtkTreeIter iter;
  GtkListStore *store =
    (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      dialog_msg (_("Select a tag!"));
      return;
    }

  if (!
      (tag =
       preferences_highlighting_add_tag (GTK_WINDOW
					 (gtk_widget_get_toplevel (button)))))
    return;

  gtk_list_store_set (store, &iter, 0, tag, 1, PANGO_STYLE_NORMAL, 2,
		      PANGO_VARIANT_NORMAL, 3, PANGO_WEIGHT_NORMAL, 4,
		      PANGO_STRETCH_NORMAL, 5, PANGO_SCALE_MEDIUM, 6, NULL, 7,
		      NULL, 8, PANGO_UNDERLINE_NONE, -1);

  g_free (tag);
}

static void
preferences_highlighting_remove (GtkWidget * button, GtkWidget * treeview)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      dialog_msg (_("Select a tag!"));
      return;
    }

  if (dialog_ask (_("Sure to remove this tag?")) == GTK_RESPONSE_OK)
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
preferences_highlighting_selection (GtkTreeSelection * selection,
				    GtkWidget * box)
{
  GtkTreeIter iter;
  GList *l, *list = gtk_container_get_children (GTK_CONTAINER (box));

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      for (l = list; l; l = l->next)
	if (!GTK_IS_BUTTON (l->data))
	  gtk_widget_set_sensitive (l->data, FALSE);
    }

  else
    {
      for (l = list; l; l = l->next)
	if (!GTK_IS_BUTTON (l->data))
	  gtk_widget_set_sensitive (l->data, TRUE);

      preferences_highlighting_refresh ((GtkWidget *)
					gtk_tree_selection_get_tree_view
					(selection));
    }

  g_list_free (list);
}

static void
preferences_highlighting_refresh (GtkWidget * treeview)
{
  GtkWidget *widget;
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  struct tag_t tag;
  GdkColor color;

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get (model, &iter, 0, &tag.tag, 1, &tag.style, 2,
		      &tag.variant, 3, &tag.weight, 4, &tag.stretch, 5,
		      &tag.scale, 6, &tag.foreground, 7, &tag.background, 8,
		      &tag.underline, -1);

  widget = g_object_get_data (G_OBJECT (treeview), "style");

  switch (tag.style)
    {
    case PANGO_STYLE_OBLIQUE:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
      break;

    case PANGO_STYLE_ITALIC:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
      break;

    default:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "variant");

  switch (tag.variant)
    {
    case PANGO_VARIANT_SMALL_CAPS:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
      break;

    default:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "weight");

  switch (tag.weight)
    {
    case PANGO_WEIGHT_ULTRALIGHT:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
      break;

    case PANGO_WEIGHT_LIGHT:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
      break;

    case PANGO_WEIGHT_SEMIBOLD:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 3);
      break;

    case PANGO_WEIGHT_BOLD:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 4);
      break;

    case PANGO_WEIGHT_ULTRABOLD:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 5);
      break;

    case PANGO_WEIGHT_HEAVY:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 6);
      break;

    default:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "stretch");

  switch (tag.stretch)
    {
    case PANGO_STRETCH_ULTRA_CONDENSED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
      break;

    case PANGO_STRETCH_EXTRA_CONDENSED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
      break;

    case PANGO_STRETCH_CONDENSED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
      break;

    case PANGO_STRETCH_SEMI_CONDENSED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 3);
      break;

    case PANGO_STRETCH_SEMI_EXPANDED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 5);
      break;

    case PANGO_STRETCH_EXPANDED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 6);
      break;

    case PANGO_STRETCH_EXTRA_EXPANDED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 7);
      break;

    case PANGO_STRETCH_ULTRA_EXPANDED:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 8);
      break;

    default:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 4);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "scale");

  if (tag.scale == PANGO_SCALE_XX_SMALL)
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

  else if (tag.scale == PANGO_SCALE_X_SMALL)
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);

  else if (tag.scale == PANGO_SCALE_SMALL)
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);

  else if (tag.scale == PANGO_SCALE_LARGE)
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 4);

  else if (tag.scale == PANGO_SCALE_X_LARGE)
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 5);

  else if (tag.scale == PANGO_SCALE_XX_LARGE)
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 6);

  else
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 3);

  widget = g_object_get_data (G_OBJECT (treeview), "foreground");
  gdk_color_parse (tag.foreground ? tag.foreground : "#000000", &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color);

  widget = g_object_get_data (G_OBJECT (treeview), "background");
  gdk_color_parse (tag.background ? tag.background : "#FFFFFF", &color);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color);

  widget = g_object_get_data (G_OBJECT (treeview), "underline");

  switch (tag.underline)
    {
    case PANGO_UNDERLINE_SINGLE:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 1);
      break;

    case PANGO_UNDERLINE_DOUBLE:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 2);
      break;

    default:
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
      break;
    }

  preferences_highlighting_demo (treeview, &tag);

  g_free (tag.tag);
  g_free (tag.background);
  g_free (tag.foreground);
}

static void
preferences_highlighting_save (GtkWidget * button, GtkTreeView * treeview)
{
  GtkWidget *widget;
  GtkTreeIter iter;
  GtkListStore *store =
    (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  GdkColor c;
  gchar str[1024];

  struct tag_t tag;

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  widget = g_object_get_data (G_OBJECT (treeview), "style");

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
    {
    case 1:
      gtk_list_store_set (store, &iter, 1, PANGO_STYLE_OBLIQUE, -1);
      break;

    case 2:
      gtk_list_store_set (store, &iter, 1, PANGO_STYLE_ITALIC, -1);
      break;

    default:
      gtk_list_store_set (store, &iter, 1, PANGO_STYLE_NORMAL, -1);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "variant");

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
    {
    case 1:
      gtk_list_store_set (store, &iter, 2, PANGO_VARIANT_SMALL_CAPS, -1);
      break;

    default:
      gtk_list_store_set (store, &iter, 2, PANGO_VARIANT_NORMAL, -1);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "weight");

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
    {
    case 0:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_ULTRALIGHT, -1);
      break;

    case 1:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_LIGHT, -1);
      break;

    case 3:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_SEMIBOLD, -1);
      break;

    case 4:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_BOLD, -1);
      break;

    case 5:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_ULTRABOLD, -1);
      break;

    case 6:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_HEAVY, -1);
      break;

    default:
      gtk_list_store_set (store, &iter, 3, PANGO_WEIGHT_NORMAL, -1);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "stretch");

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
    {
    case 0:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_ULTRA_CONDENSED, -1);
      break;

    case 1:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_EXTRA_CONDENSED, -1);
      break;

    case 2:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_CONDENSED, -1);
      break;

    case 3:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_SEMI_CONDENSED, -1);
      break;

    case 5:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_SEMI_EXPANDED, -1);
      break;

    case 6:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_EXPANDED, -1);
      break;

    case 7:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_EXTRA_EXPANDED, -1);
      break;

    case 8:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_ULTRA_EXPANDED, -1);
      break;

    default:
      gtk_list_store_set (store, &iter, 4, PANGO_STRETCH_NORMAL, -1);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "scale");

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
    {
    case 0:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_XX_SMALL, -1);
      break;

    case 1:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_X_SMALL, -1);
      break;

    case 2:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_SMALL, -1);
      break;

    case 4:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_LARGE, -1);
      break;

    case 5:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_X_LARGE, -1);
      break;

    case 6:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_XX_LARGE, -1);
      break;

    default:
      gtk_list_store_set (store, &iter, 5, PANGO_SCALE_MEDIUM, -1);
      break;
    }

  widget = g_object_get_data (G_OBJECT (treeview), "foreground");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &c);
  snprintf (str, sizeof (str), "#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
	    (c.blue >> 8));
  gtk_list_store_set (store, &iter, 6, str, -1);

  widget = g_object_get_data (G_OBJECT (treeview), "background");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &c);
  snprintf (str, sizeof (str), "#%.2X%.2X%.2X", (c.red >> 8), (c.green >> 8),
	    (c.blue >> 8));
  gtk_list_store_set (store, &iter, 7, str, -1);

  widget = g_object_get_data (G_OBJECT (treeview), "underline");

  switch (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)))
    {
    case 1:
      gtk_list_store_set (store, &iter, 8, PANGO_UNDERLINE_SINGLE, -1);
      break;

    case 2:
      gtk_list_store_set (store, &iter, 8, PANGO_UNDERLINE_DOUBLE, -1);
      break;

    default:
      gtk_list_store_set (store, &iter, 8, PANGO_UNDERLINE_NONE, -1);
      break;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &tag.tag, 1,
		      &tag.style, 2, &tag.variant, 3, &tag.weight, 4,
		      &tag.stretch, 5, &tag.scale, 6, &tag.foreground, 7,
		      &tag.background, 8, &tag.underline, -1);

  preferences_highlighting_demo (GTK_WIDGET (treeview), &tag);

  g_free (tag.tag);
  g_free (tag.background);
  g_free (tag.foreground);
}

static void
preferences_highlighting_demo (GtkWidget * treeview, struct tag_t *tag)
{
  GtkWidget *demo = g_object_get_data (G_OBJECT (treeview), "demo");
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (demo));
  GtkTextIter start, end;

  textview_clean_tags (demo);

  textview_add_tag (demo, tag->tag, tag->style, tag->variant, tag->weight,
		    tag->stretch, tag->scale, tag->foreground,
		    tag->background, tag->underline);

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_delete (buffer, &start, &end);

  gtk_text_buffer_get_start_iter (buffer, &start);
  textview_insert (demo, &start, tag->tag, -1);
}

static void
preferences_highlighting_reset (GtkWidget * w, GtkWidget * treeview)
{
  GtkTreeIter iter;
  GtkListStore *store =
    (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  GList *def = init_highlighting ();
  GList *list;

  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (store), &iter, NULL, 0))
    gtk_list_store_remove (store, &iter);

  for (list = def; list; list = list->next)
    {
      GtkTreeIter iter;
      struct tag_t *tag = list->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, tag->tag, 1, tag->style, 2,
			  tag->variant, 3, tag->weight, 4, tag->stretch, 5,
			  tag->scale, 6, tag->foreground, 7, tag->background,
			  8, tag->underline, -1);
    }

  g_list_foreach (def, (GFunc) preferences_highlighting_free_single, NULL);
  g_list_free (def);
}

#ifdef USE_GCONF
static void
preferences_proxy (GtkWidget * w, GtkWidget * box)
{
  GtkWidget *gconf = g_object_get_data (G_OBJECT (w), "gconf");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)) == TRUE)
    {
      gtk_widget_set_sensitive (box, TRUE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gconf), FALSE);
    }
  else
    gtk_widget_set_sensitive (box, FALSE);
}

static void
preferences_proxy_gconf (GtkWidget * w, GtkWidget * proxy)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy), FALSE);
}
#endif

/* EOF */

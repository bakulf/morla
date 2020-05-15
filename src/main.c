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

static GtkWidget *window;

gint statusbar_id;
GtkWidget *statusbar;
gchar *browser_default = NULL;
gchar *viewer_default = NULL;
GList *fork_child = NULL;
GList *thread_child = NULL;
GList *rdfs_list = NULL;
gint last_id = 0;

gint default_width = 0;
gint default_height = 0;

gint editor_statusbar_lock;

gint global_argc;
gchar **global_argv;

struct editor_data_t *editor_data = NULL;
GtkWidget *notebook;

struct graph_data_t graph_blank_node;
struct graph_data_t graph_resource_node;
struct graph_data_t graph_literal_node;
struct graph_data_t graph_edge;
gchar *graph_format;

gchar *normal_bg_color;
gchar *prelight_bg_color;
gchar *normal_fg_color;
gchar *prelight_fg_color;

GtkWidget *undo_widget[2];
GtkWidget *redo_widget[2];

static gint editor_id = 0;
static gint update_id = 0;

/* Prototipi: */
static void window_resize (GtkWidget *, GtkRequisition *, gpointer);
static gchar *user_agent_search (void);
static gboolean template_starter_open (struct template_t *t);
static gboolean template_starter_edit (struct template_t *t);
static const gchar *main_path_icon (void);

/* Questa funzione viene avviata ogni volta che si passa da un documento
 * all'altro usando le label del notebook. Aggiorna undo e redo: */
static void
switch_page (GtkWidget * widget, gint arg1, gpointer udata)
{
  struct editor_data_t *data = editor_get_data ();

  if (!data)
    {
      gtk_widget_set_sensitive (undo_widget[0], FALSE);
      gtk_widget_set_sensitive (undo_widget[1], FALSE);
      gtk_widget_set_sensitive (redo_widget[0], FALSE);
      gtk_widget_set_sensitive (redo_widget[1], FALSE);
      return;
    }

  gtk_widget_set_sensitive (undo_widget[0], data->undo ? TRUE : FALSE);
  gtk_widget_set_sensitive (undo_widget[1], data->undo ? TRUE : FALSE);
  gtk_widget_set_sensitive (redo_widget[0], data->redo ? TRUE : FALSE);
  gtk_widget_set_sensitive (redo_widget[1], data->redo ? TRUE : FALSE);
}

/* Urla qualcosa ed esce: */
void
fatal (gchar * text, ...)
{
  va_list va;
  gchar s[1024];
  gint i;
  gchar **a;

  a = (gchar **) g_malloc (sizeof (gchar *) * (global_argc + 1));

  va_start (va, text);
  g_vsnprintf (s, sizeof (s), text, va);
  va_end (va);

  splash_hide ();
  dialog_msg (s);

  for (i = 0; i < global_argc; i++)
    a[i] = global_argv[i];

  a[i] = NULL;

  g_spawn_async (NULL, a, NULL,
		 G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
		 G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);

  g_free (a);
  exit (1);
}

/* Questa funzione viene richiamata quando si chiede di uscire: */
gint
editor_quit (GtkWidget * w, GdkEvent * event, gpointer dummy)
{
  struct editor_data_t *tmp;
  GList *list = NULL;
  GList *l;
  gchar s[1024];

  tmp = editor_data;

  /* Guardo quali file non sono ancora salvati e creo una lista: */
  while (tmp)
    {
      if (rdf_is_no_saved (tmp) == TRUE)
	{
	  if (tmp->file)
	    list = g_list_append (list, g_strdup (tmp->file));
	  else
	    list = g_list_append (list, g_strdup (_("New RDF Document")));
	}

      tmp = tmp->next;
    }

  /* Se non c'e' la lista, non oppongo resistenza: */
  if (!list)
    {
      g_source_remove (editor_id);
      g_source_remove (update_id);
      gtk_main_quit ();
      return FALSE;
    }

  /* Creo la scritta per il popup: */
  g_snprintf (s, sizeof (s), _("Exit without save?\nCheck: "));

  l = list;
  while (l)
    {
      strncat (s, (gchar *) l->data, strlen (s));

      if (l->next)
	strncat (s, ", ", strlen (s));

      g_free (l->data);
      l = l->next;
    }

  /* Rimuovo la lista: */
  g_list_free (list);

  /* E chiedo conferma: */
  if (dialog_ask (s) == GTK_RESPONSE_OK)
    {
      g_source_remove (editor_id);
      g_source_remove (update_id);
      gtk_main_quit ();
      return FALSE;
    }

  return TRUE;
}

/* Se dal menu qualcuno chiede di aggiungere un rdfs: */
static void
menu_rdfs_add (GtkWidget * w, gpointer dummy)
{
  rdfs_add (NULL, NULL);
}

/* Se dal menu qualcuno chiede di aprire un RDF remoto */
void
menu_open_remote (GtkWidget * w, gpointer dummy)
{
  gchar *uri;
  struct download_t *download;

  if ((uri = uri_chooser (_("Open a RDF URI"), &download)))
    {
      editor_open_uri (uri, download);
      g_free (uri);
    }

  if (download)
    download_free (download);
}

/* Funzione di callback rispetto alla search open */
void
menu_search_cb (gchar * uri, gpointer dummy)
{
  editor_open_uri (uri, dummy);
  g_free (uri);
}

/* Se dal menu qualcuno chiede di cercare RDFs in un URI remoto */
void
menu_search_remote (GtkWidget * w, gpointer dummy)
{
  gchar *uri;
  struct download_t *download;

  if ((uri = uri_chooser (_("Open a RDF URI"), &download)))
    {
      search_open (uri, menu_search_cb, download);
      g_free (uri);
    }

  if (download)
    download_free (download);
}

/* Si parte: */
int
main (int argc, gchar ** argv, gchar ** e)
{
  GtkWidget *vbox;
  GtkWidget *toolbar;
  GtkWidget *sep;
  GtkWidget *button;
  GtkWidget *image;
  GtkAccelGroup *accel;
  GtkWidget *menubar;
  GtkWidget *menuitem;
  GtkWidget *menumenu;
  GtkWidget *t_menu;
  GtkWidget *o_menu;
  GtkWidget *import_menu;

  gchar s[1024];
  gint y, x;

  guint t_timer = 0;

  global_argv = argv;
  global_argc = argc;

  gchar **items = NULL;
  gchar *t_selected = NULL;
  gchar **t_import = NULL;
  gchar *m_config = NULL;

  GOptionEntry entries[] = {
    {"template", 't', 0, G_OPTION_ARG_STRING, &t_selected,
     "Open a template by the name or the uri", "NAMEorURI"},
    {"import", 'i', 0, G_OPTION_ARG_STRING_ARRAY, &t_import,
     "Import a template from the uri", "URI"},
    {"moduleconfig", 'm', 0, G_OPTION_ARG_STRING, &m_config,
     "Use a particular config file for modules", "FILE"},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &items, NULL, NULL},
    {NULL}
  };

  GOptionContext *context;
  GError *error = NULL;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  g_thread_init (NULL);
  gtk_init (NULL, NULL);

  g_set_prgname (PACKAGE);

  context = g_option_context_new ("URIs...files");
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  if (g_option_context_parse (context, &argc, &argv, &error) == FALSE)
    {
      fprintf (stderr, PACKAGE " " VERSION ": %s\n", error->message);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  gtk_window_set_default_icon_from_file (main_path_icon (), NULL);

#ifdef USE_GCONF
  morla_gconf_init ();
#endif

  init (m_config);

  if (m_config)
    g_free (m_config);

  accel = gtk_accel_group_new ();

  g_snprintf (s, sizeof (s), "%s %s", PACKAGE, VERSION);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), s);

  /* Funzione per il cambio di dimensione: */
  g_signal_connect_after ((gpointer) window, "size-request",
			  G_CALLBACK (window_resize), NULL);

  /* Setto le dimensioni: */
  if (!default_width || !default_height)
    {
      y = gdk_screen_height () * 2 / 3;
      if (y < 480)
	y = 480;

      x = gdk_screen_width () * 2 / 3;
      if (x < 640)
	x = 640;
    }
  else
    {
      x = default_width;
      y = default_height;
    }

  gtk_window_resize (GTK_WINDOW (window), x, y);

  g_signal_connect ((gpointer) window, "delete_event",
		    G_CALLBACK (editor_quit), NULL);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

  /* MENU FILE */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_File"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-new", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_new), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  if (module_default_open ())
    {
      menuitem = gtk_image_menu_item_new_from_stock ("gtk-open", accel);
      gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate",
			G_CALLBACK (module_run_open), module_default_open ());

      menuitem =
	gtk_image_menu_item_new_with_mnemonic (_("_Open a RDF file"));
      gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate",
			G_CALLBACK (editor_open), NULL);

    }
  else
    {
      menuitem = gtk_image_menu_item_new_from_stock ("gtk-open", accel);
      gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate",
			G_CALLBACK (editor_open), NULL);
    }

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Open a RDF URI"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (menu_open_remote), NULL);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("_Search RDF in an URI"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (menu_search_remote), NULL);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("_Special Opening procedure"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  o_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), o_menu);
  module_menu_open (GTK_MENU (o_menu), menuitem);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("Last Documents"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  last_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), last_menu);

  last_update ();

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  if (module_default_save ())
    {
      menuitem = gtk_image_menu_item_new_from_stock ("gtk-save", accel);
      gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate",
			G_CALLBACK (module_run_save), module_default_save ());

      menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Save file"));
      gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate",
			G_CALLBACK (editor_save), NULL);
    }
  else
    {
      menuitem = gtk_image_menu_item_new_from_stock ("gtk-save", accel);
      gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate",
			G_CALLBACK (editor_save), NULL);
    }

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-save-as", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_save_as), NULL);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("_Special Saving procedure"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  o_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), o_menu);
  module_menu_save (GTK_MENU (o_menu), menuitem);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-preferences", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (preferences), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-close", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_close), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-quit", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_quit), NULL);

  /* MENU EDIT */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-undo", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (unredo_undo), NULL);
  gtk_widget_set_sensitive (menuitem, FALSE);
  undo_widget[0] = menuitem;

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-redo", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (unredo_redo), NULL);
  gtk_widget_set_sensitive (menuitem, FALSE);
  redo_widget[0] = menuitem;

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-add", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_add), NULL);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Add from templates"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  t_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), t_menu);
  g_object_set_data (G_OBJECT (t_menu), "func", template_open);
  template_menus = g_list_append (template_menus, t_menu);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("_Modify with templates"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-edit", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  t_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), t_menu);
  g_object_set_data (G_OBJECT (t_menu), "func", template_edit);
  template_menus = g_list_append (template_menus, t_menu);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("_Import a RDF Document"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  import_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), import_menu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("... from a file"));
  gtk_container_add (GTK_CONTAINER (import_menu), menuitem);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_merge_file), NULL);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("... from an uri"));
  gtk_container_add (GTK_CONTAINER (import_menu), menuitem);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_merge_uri), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-cut", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_cut), NULL);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-copy", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_copy), NULL);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-paste", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_paste), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-delete", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (editor_delete), NULL);

  /* INFO */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Information"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("Info about this RDF Document"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock (
#ifdef GTK_STOCK_INFO
				     "gtk-info",
#else
				     "gtk-dialog-info",
#endif
				     GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  g_signal_connect ((gpointer) menuitem, "activate", G_CALLBACK (info_this),
		    NULL);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_
					   ("Info about the selected element"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock (
#ifdef GTK_STOCK_INFO
				     "gtk-info",
#else
				     "gtk-dialog-info",
#endif
				     GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (info_selected), NULL);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("Info about an URI..."));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock (
#ifdef GTK_STOCK_INFO
				     "gtk-info",
#else
				     "gtk-dialog-info",
#endif
				     GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
  g_signal_connect ((gpointer) menuitem, "activate", G_CALLBACK (info_uri),
		    NULL);

  /* TEMPLATE */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Template"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("Import from a _file..."));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (template_import_file), NULL);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("Import from an _URI..."));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (template_import_uri), NULL);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Export a template"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-save-as", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  t_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), t_menu);
  g_object_set_data (G_OBJECT (t_menu), "func", template_save_single);
  template_menus = g_list_append (template_menus, t_menu);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Show templates"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (template), NULL);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  /* MENU RDFS */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_RDFS"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Add RDFS"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (menu_rdfs_add), NULL);

  image = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Show RDFS"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate", G_CALLBACK (rdfs), NULL);

  image = gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  /* MENU VIEW */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_View"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  module_menu_view (GTK_MENU (menumenu), menuitem);

  /* MENU GRAPH */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Graph"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("Create _Graph"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (graph_run), NULL);

  image = gtk_image_new_from_stock ("gtk-execute", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Save Graph As"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (graph_save), NULL);

  image = gtk_image_new_from_stock ("gtk-save-as", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("Save Graph_viz Source As"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (graph_graphviz), NULL);

  image = gtk_image_new_from_stock ("gtk-save-as", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_
					   ("_Merge more RDF Document in a Graph"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (merge_graph), NULL);

  image = gtk_image_new_from_stock ("gtk-connect", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  /* MENU NAVIGATOR */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Navigator"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("Open _navigator"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (navigator_run), NULL);

  image = gtk_image_new_from_stock ("gtk-media-play", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_
					   ("Open a RDF URI with the navigator"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (navigator_open_uri), NULL);

  image = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  /* MENU QUERY */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Query"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("SPARQL"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_sparql), NULL);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_
					   ("RDQL (RDF Data Query Language)"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (query_rsql), NULL);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  /* HELP */
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Help"));
  gtk_menu_item_set_right_justified (GTK_MENU_ITEM (menuitem), TRUE);
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menumenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menumenu);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-help", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate", G_CALLBACK (help), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Find updates"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-cdrom", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (update_search), NULL);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic
    (_("_Open the Web Page of Morla Project"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  image = gtk_image_new_from_stock ("gtk-network", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (browser_website), NULL);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem =
    gtk_image_menu_item_new_with_mnemonic (_("_Show used Libraries"));
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);

  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (about_libraries), NULL);

  image = gtk_image_new_from_stock (
#ifdef GTK_STOCK_INFO
				     "gtk-info",
#else
				     "gtk-dialog-info",
#endif
				     GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  gtk_widget_set_sensitive (menuitem, FALSE);

  menuitem = gtk_image_menu_item_new_from_stock ("gtk-about", accel);
  gtk_container_add (GTK_CONTAINER (menumenu), menuitem);
  g_signal_connect ((gpointer) menuitem, "activate",
		    G_CALLBACK (about), NULL);

  /* TOOLBAR */
  toolbar = gtk_toolbar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-new");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_new), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("New RDF Document"));

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-open");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_open), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Open a RDF Document"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-save");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_save), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Save this RDF Document"));

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-save-as");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_save_as), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Save this RDF Document as a new file"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-undo");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (unredo_undo), NULL);
  gtk_widget_set_sensitive (button, FALSE);
  undo_widget[1] = button;

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Undo editing"));

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-redo");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (unredo_redo), NULL);
  gtk_widget_set_sensitive (button, FALSE);
  redo_widget[1] = button;

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Redo editing"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-close");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_close), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Close this RDF Document"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_menu_tool_button_new_from_stock ("gtk-add");
  gtk_container_add (GTK_CONTAINER (toolbar), button);

  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_add), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Add a new RDF triple"));

  menumenu = gtk_menu_new ();
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (button), menumenu);
  g_object_set_data (G_OBJECT (menumenu), "func", template_open);
  template_menus = g_list_append (template_menus, menumenu);

  template_update ();

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-cut");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_cut), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Cut"));

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-copy");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_copy), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Copy"));

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-paste");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_paste), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Paste"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  button = (GtkWidget *) gtk_tool_button_new_from_stock ("gtk-delete");
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (editor_delete), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Delete"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  image =
    gtk_image_new_from_stock ("gtk-convert", GTK_ICON_SIZE_LARGE_TOOLBAR);
  button = (GtkWidget *) gtk_tool_button_new (image, _("rdfs"));
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked", G_CALLBACK (rdfs), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Show the RDFS"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  image =
    gtk_image_new_from_stock ("gtk-execute", GTK_ICON_SIZE_LARGE_TOOLBAR);
  button = (GtkWidget *) gtk_tool_button_new (image, _("graph"));
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (graph_run), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Show the graph of this RDF Document"));

  sep = (GtkWidget *) gtk_separator_tool_item_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), sep);

  image =
    gtk_image_new_from_stock ("gtk-media-play", GTK_ICON_SIZE_LARGE_TOOLBAR);

  button = (GtkWidget *) gtk_tool_button_new (image, _("navigator"));
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (navigator_run), NULL);

  gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button),
				  _("Open navigator with this RDF Document"));

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  g_signal_connect ((gpointer) notebook, "change-current-page",
		    G_CALLBACK (switch_page), NULL);

  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), statusbar, FALSE, FALSE, 0);

  if (items)
    {
      gchar **list;

      for (list = items; list && *list; list++)
	{
	  if (g_file_test (*list, G_FILE_TEST_EXISTS) == FALSE)
	    editor_open_uri (*list, NULL);
	  else
	    editor_open_file (*list, NULL);
	}

      g_strfreev (items);
    }

  if (!editor_data)
    editor_new (NULL, NULL);

  if (t_import)
    {
      struct template_t *t;
      gchar **ti;
      GList *list;
      gboolean save = FALSE, found = FALSE;

      for (ti = t_import; ti && *ti; ti++)
	{
	  found = FALSE;
	  list = template_list;
	  while (list)
	    {
	      t = list->data;

	      if (!strcmp (t->uri, *ti))
		{
		  found = TRUE;
		  break;
		}

	      list = list->next;
	    }

	  if (found == FALSE
	      && template_import (*ti, NULL, TRUE, TRUE) == TRUE)
	    save = TRUE;
	}

      if (save == TRUE);
      template_save ();

      g_strfreev (t_import);
    }

  if (t_selected)
    {
      struct template_t *t;
      GList *list;

      list = template_list;
      while (list)
	{
	  t = list->data;

	  if (!strcmp (t->uri, t_selected) || !strcmp (t->name, t_selected))
	    {
	      if (!items)
		t_timer =
		  g_timeout_add (1, (GSourceFunc) template_starter_open, t);
	      else
		t_timer =
		  g_timeout_add (1, (GSourceFunc) template_starter_edit, t);

	      break;
	    }

	  list = list->next;
	}

      g_free (t_selected);
    }

  statusbar_set (_("Welcome to RDF editor!"));
  editor_statusbar_lock = WAIT;

  /* Setto un timer generico: */
  editor_id = g_timeout_add (2000, editor_timeout, NULL);

  gtk_window_add_accel_group (GTK_WINDOW (window), accel);

  if (!t_timer)
    gtk_widget_show_all (window);

  /* Check the redland version: */
  check_redland ();

  /* Ed uno per gli aggiormanti ogni 20 minuti: */
  update_timer ();
  update_id =
    g_timeout_add (20 * 60 * 1000, (GSourceFunc) update_timer, NULL);

  gtk_main ();

#ifdef USE_GCONF
  morla_gconf_shutdown ();
#endif

  module_shutdown ();
  return 0;
}

/* Funzione di ridimensione: */
static void
window_resize (GtkWidget * widget, GtkRequisition * req, gpointer dummy)
{
  if (widget->allocation.width != default_width
      || widget->allocation.height != default_height)
    {
      default_width = widget->allocation.width;
      default_height = widget->allocation.height;

      init_save ();
    }
}

/* 
 * From GLIB 
 * Authors: Havoc Pennington <hp@redhat.com>, Owen Taylor <otaylor@redhat.com>
 */
static gchar *
make_valid_utf8 (const gchar * name)
{
  GString *string;
  const gchar *remainder, *invalid;
  gint remaining_bytes, valid_bytes;

  string = NULL;
  remainder = name;
  remaining_bytes = strlen (name);

  while (remaining_bytes != 0)
    {
      if (g_utf8_validate (remainder, remaining_bytes, &invalid))
	break;
      valid_bytes = invalid - remainder;

      if (string == NULL)
	string = g_string_sized_new (remaining_bytes);

      g_string_append_len (string, remainder, valid_bytes);
      g_string_append_c (string, '?');

      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
    }

  if (string == NULL)
    return g_strdup (name);

  g_string_append (string, remainder);

  return g_string_free (string, FALSE);
}

/* UTF8: */
gchar *
utf8 (gchar * str)
{
  gchar *tmp;

  if (!str)
    return g_strdup ("");

  if (!(tmp = g_locale_to_utf8 (str, -1, NULL, NULL, NULL)))
    tmp = make_valid_utf8 (str);

  return tmp;
}

/* Funzione che si occupa di sostituire caratteri strani di una stringa: */
gchar *
markup (gchar * text)
{
  GString *ret;
  gunichar unichr;

  if (!text)
    return g_strdup ("");

  ret = g_string_new (NULL);

  while ((unichr = g_utf8_get_char (text)))
    {
      text = g_utf8_next_char (text);

      switch (unichr)
	{
	case '<':
	  g_string_append (ret, "&lt;");
	  break;

	case '>':
	  g_string_append (ret, "&gt;");
	  break;

	case '&':
	  g_string_append (ret, "&amp;");
	  break;

	case '\'':
	  g_string_append (ret, "&apos;");
	  break;

	case '\"':
	  g_string_append (ret, "&quot;");
	  break;

	default:
	  g_string_append_unichar (ret, unichr);
	  break;
	}
    }

  return g_string_free (ret, FALSE);
}

/* Aggiornamento della statusbar */
void
statusbar_set (gchar * what, ...)
{
  va_list va;
  gchar s[1024];
  gint ret;

  va_start (va, what);

  ret = g_vsnprintf (s, sizeof (s), what, va);

  while (s[ret] == '\n' || s[ret] == '\r')
    ret--;

  s[ret] = 0;

  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), statusbar_id);
  statusbar_id =
    gtk_statusbar_push (GTK_STATUSBAR (statusbar), statusbar_id, s);

  va_end (va);
}

/* Esecuzione di un comando e gestione della callback: */
void
run (gchar ** a, GChildWatchFunc func, gpointer point)
{
  GPid pid;

  if (g_spawn_async
      (NULL, a, NULL,
       (func ? G_SPAWN_DO_NOT_REAP_CHILD : 0) | G_SPAWN_SEARCH_PATH |
       G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL,
       &pid, NULL) == FALSE)
    {
      dialog_msg
	(_
	 ("Execution of external software error.\n"
	  "Check the softwares in your preferences!"));
      return;
    }

  if (func)
    g_child_watch_add (pid, func, point);
}

/* Funzione atta a splittare una stringa in tanti blocchi */
GList *
split (gchar * ar)
{
  gint q1 = 0;
  gint q2 = 0;
  GList *ret = NULL;
  gunichar unichr;
  GString *opt;

  opt = g_string_new (NULL);

  while ((unichr = g_utf8_get_char (ar)))
    {
      ar = g_utf8_next_char (ar);

      if (unichr == '\"' && !q2)
	q1 = !q1;

      else if (unichr == '\'' && !q1)
	q2 = !q2;

      else if ((unichr == ' ' || unichr == '\t') && !q1 && !q2)
	{
	  if (opt->len)
	    {
	      ret = g_list_append (ret, g_string_free (opt, FALSE));
	      opt = g_string_new (NULL);
	    }
	}

      else
	g_string_append_unichar (opt, unichr);
    }

  if (opt->len)
    ret = g_list_append (ret, g_string_free (opt, FALSE));
  else
    g_string_free (opt, TRUE);

  return ret;
}

/* User Agent for HTTP (this code is only for my stats :) */
gchar *
user_agent (void)
{
  static gchar *s = NULL;

  if (!s)
    s = user_agent_search ();

  return s;
}

static gchar *
user_agent_search (void)
{
#ifdef G_OS_WIN32

  OSVERSIONINFOEX osvi;
  SYSTEM_INFO si;
  BOOL bOsVersionInfoEx;

  memset (&si, 0, sizeof (SYSTEM_INFO));
  memset (&osvi, 0, sizeof (OSVERSIONINFOEX));

  osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);

  if (!(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) & osvi)))
    {
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (!GetVersionEx ((OSVERSIONINFO *) & osvi))
	return PACKAGE " " VERSION " (Win32)";
    }

  GetSystemInfo (&si);

  switch (osvi.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
      if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0)
	{
	  if (osvi.wProductType == VER_NT_WORKSTATION)
	    return PACKAGE " " VERSION " (Microsoft Windows Vista)";
	  else
	    return PACKAGE " " VERSION " (Windows Server \"Longhorn\")";
	}

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2)
	{
	  if (osvi.wProductType == VER_NT_WORKSTATION &&
	      si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
	    return PACKAGE " " VERSION
	      " (Microsoft Windows XP Professional x64 Edition)";
	  else
	    return PACKAGE " " VERSION " (Microsoft Windows Server 2003)";
	}

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
	return PACKAGE " " VERSION " (Microsoft Windows XP)";

      if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
	return PACKAGE " " VERSION " (Microsoft Windows 2000)";

      if (osvi.dwMajorVersion <= 4)
	return PACKAGE " " VERSION " (Microsoft Windows NT)";

      return PACKAGE " " VERSION " (Win32)";

    case VER_PLATFORM_WIN32_WINDOWS:

      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
	return PACKAGE " " VERSION " (Microsoft Windows 95)";

      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
	{
	  if (osvi.szCSDVersion[1] == 'A' || osvi.szCSDVersion[1] == 'B')
	    return PACKAGE " " VERSION " (Microsoft Windows 98 SE)";
	  else
	    return PACKAGE " " VERSION " (Microsoft Windows 98)";
	}

      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
	return PACKAGE " " VERSION " (Microsoft Windows Millennium Edition)";

      return PACKAGE " " VERSION " (Win32)";
      break;

    case VER_PLATFORM_WIN32s:
      return PACKAGE " " VERSION " (Win32s)";

    default:
      return PACKAGE " " VERSION " (Win32s)";
    }
#endif

#ifdef G_OS_BEOS
  return PACKAGE " " VERSION " (BeOS)";
#endif

#ifdef G_OS_UNIX
  struct utsname u;
  static gchar s[1024];

  if (uname (&u))
    return PACKAGE " " VERSION " (Unix)";

  snprintf (s, sizeof (s), PACKAGE " " VERSION " (%s)", u.sysname);
  return s;
#endif

  return PACKAGE " " VERSION;
}

static gboolean
template_starter_open (struct template_t *t)
{
  template_open (NULL, t);
  gtk_widget_show_all (window);
  return FALSE;
}

static gboolean
template_starter_edit (struct template_t *t)
{
  template_edit (NULL, t);
  gtk_widget_show_all (window);
  return FALSE;
}

gchar *
dot_cmd (void)
{
  gchar *cmd = (gchar *) g_getenv ("MORLA_DOT_CMD");

  if (cmd)
    return cmd;

  return DOT_CMD;
}

GtkWindow *
main_window (void)
{
  return GTK_WINDOW (window);
}

static const gchar *
main_path_icon (void)
{
#ifndef ENABLE_MACOSX
  return PATH_ICON;
#endif    

  static const gchar *path;
  static gboolean done = FALSE;

  if (done == FALSE)
    {
      path = g_getenv ("MORLA_PATH_ICON");
      done = TRUE;
    }

  if (!path)
    return PATH_ICON;

  return path;
}

/* EOF */

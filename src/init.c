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
#include "morla.h"

static void init_read_config (gchar * file, gboolean cp);
static void init_create (void);
static void init_graph (struct graph_data_t *graph, nxml_data_t * data,
			gchar * file);
static void init_graph_reset (void);
static void init_color_reset (void);
static gchar *init_browser (void);
static gchar *init_viewer (void);
static void init_morla_save (void);
static void init_template (void);

static gchar *init_highlighting_style2str (PangoStyle);
static gchar *init_highlighting_variant2str (PangoVariant);
static gchar *init_highlighting_weight2str (PangoWeight);
static gchar *init_highlighting_stretch2str (PangoStretch);
static gchar *init_highlighting_scale2str (gdouble);
static gchar *init_highlighting_underline2str (PangoUnderline);

static PangoStyle init_highlighting_str2style (gchar *);
static PangoVariant init_highlighting_str2variant (gchar *);
static PangoWeight init_highlighting_str2weight (gchar *);
static PangoStretch init_highlighting_str2stretch (gchar *);
static gdouble init_highlighting_str2scale (gchar *);
static PangoUnderline init_highlighting_str2underline (gchar *);

static void website_update (gchar ** str);

#ifndef G_OS_WIN32
static gboolean init_stat (gchar * sw);
#else
# include <shlwapi.h>
#endif

static gboolean init_retrocompatibility (void);

/* Funzione di inizializzazione di morla: */
void
init (gchar * m_config)
{
  gchar *file;
  gboolean change_path;

  /* Mostro lo splash: */
  splash_show ();
  splash_set_text_with_effect (PACKAGE " " VERSION);
  splash_sleep (1);

  /* Carico moduli: */
  module_init (m_config);

  /* Setto i valori di default: */
  init_graph_reset ();
  init_color_reset ();

  /* Retro compatibility: */
  change_path = init_retrocompatibility ();

  /* Cerco il file di configurazione: */
  file =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "config.xml", NULL);

  /* Se il file non esiste, allora e' la prima volta che morla viene
   * avviato e lancio la procedura di creazione config: */
  if (g_file_test (file, G_FILE_TEST_EXISTS) == FALSE)
    init_create ();

  /* Altrimenti leggo la config: */
  else
    {
      /* Inizializzo i template: */
      init_template ();

      /* Leggo la configurazione: */
      init_read_config (file, change_path);
    }

  /* Libero memoria e chiudo lo splash: */
  g_free (file);
  splash_hide ();
}

/* Questa funzione legge il file di configurazione passato come argomento: */
static void
init_read_config (gchar * file, gboolean cp)
{
  nxml_t *nxml;
  nxml_data_t *data;
  nxml_attr_t *attr;
  struct rdfs_t *rdfs;
  gchar *last;
  gchar *uri;

  gboolean highlighting_found = FALSE;

  /* Setto il titolo nello splash */
  splash_set_text (_("Reading the config file"));

  /* Uso le libnxml che ho fatto io e funzionano tanto bene: */
  if (nxml_new (&nxml) != NXML_OK)
    fatal (_("Error memory!"));

  if (nxml_parse_file (nxml, file) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (nxml_root_element (nxml, &data) != NXML_OK || !data)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (strcmp (data->value, PACKAGE))
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  /* Per ogni elemento del file di configurazione: */
  data = data->children;
  while (data)
    {
      /* Se e' last_id lo aggiorno: */
      if (!strcmp (data->value, "last_id"))
	{
	  nxml_get_string (data, &last);

	  if (!last)
	    last_id = 0;
	  else
	    {
	      last_id = atoi (last);
	      g_free (last);
	    }
	}

      /* Browser: */
      else if (!strcmp (data->value, "browser") && !browser_default)
	nxml_get_string (data, &browser_default);

      /* Viewer: */
      else if (!strcmp (data->value, "viewer") && !viewer_default)
	nxml_get_string (data, &viewer_default);

      /* Normal_bg: */
      else if (!strcmp (data->value, "normal_bg") && !normal_bg_color)
	nxml_get_string (data, &normal_bg_color);

      /* Prelight_bg: */
      else if (!strcmp (data->value, "prelight_bg") && !prelight_bg_color)
	nxml_get_string (data, &prelight_bg_color);

      /* Normal_fg: */
      else if (!strcmp (data->value, "normal_fg") && !normal_fg_color)
	nxml_get_string (data, &normal_fg_color);

      /* Prelight_fg: */
      else if (!strcmp (data->value, "prelight_fg") && !prelight_fg_color)
	nxml_get_string (data, &prelight_fg_color);

      /* I vari RDFS importati: */
      else if (!strcmp (data->value, "rdfs"))
	{
	  GList *list = NULL, *nslist = NULL;
	  gchar *rp;

	  /* Alloco un nuovo elemento: */
	  rdfs = (struct rdfs_t *) g_malloc0 (sizeof (struct rdfs_t));

	  /* Cerco l'attributo "resource": */
	  if (nxml_find_attribute (data, "resource", &attr) != NXML_OK
	      || !attr)
	    {
	      g_remove (file);
	      fatal (_("Syntax error in file '%s'"), file);
	    }

	  rdfs->resource = g_strdup (attr->value);

	  /* Cerco l'attributo "prefix": */
	  if (nxml_find_attribute (data, "prefix", &attr) != NXML_OK || !attr)
	    {
	      g_remove (file);
	      fatal (_("Syntax error in file '%s'"), file);
	    }

	  rdfs->prefix = g_strdup (attr->value);

	  /* Prendo la path: */
	  if (nxml_get_string (data, &rdfs->path) != NXML_OK || !rdfs->path)
	    {
	      g_remove (file);
	      fatal (_("Syntax error in file '%s'"), file);
	    }

	  /* Retro compatibility: */
	  if (cp == TRUE)
	    {
	      gchar *path =
		g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (),
			      "." PACKAGE, "rdfs", NULL);
	      gint len = strlen (path);

	      if (!strncmp (rdfs->path, path, len))
		{
		  gchar *new = rdfs->path + len;

		  while (new[0] && new[0] == G_DIR_SEPARATOR)
		    new++;

		  new = g_strdup (new);
		  g_free (rdfs->path);
		  rdfs->path = new;
		}

	      g_free (path);
	    }

	  /* Setto la path corretta: */
	  rp =
	    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (),
			  PACKAGE, "rdfs", rdfs->path, NULL);
	  g_free (rdfs->path);
	  rdfs->path = rp;

	  /* Aggiorno lo splash e parso questo rdfs: */
	  splash_set_text (_("Parsing local dump of '%s'"), rdfs->resource);

	  uri = g_filename_to_uri (rdfs->path, NULL, NULL);

	  if (!uri
	      || rdf_parse (uri, NULL, FALSE, &list, NULL, NULL) == FALSE)
	    {
	      g_remove (rdfs->path);
	      g_remove (file);

	      if (uri)
		g_free (uri);

	      fatal (_("Syntax error in file '%s'"), rdfs->path);
	    }

	  g_free (uri);

	  /* Parso il namespace: */
	  if (namespace_parse (list, &nslist) == FALSE)
	    {
	      g_remove (rdfs->path);
	      g_remove (file);
	      fatal (_("Syntax error in file '%s'"), rdfs->path);
	    }

	  /* Aggiungo i dati e inserisco in lista rdfs_list */
	  rdfs->data = list;
	  rdfs->nsdata = nslist;

	  rdfs_list = g_list_append (rdfs_list, rdfs);
	}

      /* Parso i colori dei blank_node nei grafo: */
      else if (!strcmp (data->value, "graph_blank_node"))
	init_graph (&graph_blank_node, data, file);

      /* Parso i colori delle risorse nei grafo: */
      else if (!strcmp (data->value, "graph_resource_node"))
	init_graph (&graph_resource_node, data, file);

      /* Parso i colori dei letterali nei grafo: */
      else if (!strcmp (data->value, "graph_literal_node"))
	init_graph (&graph_literal_node, data, file);

      /* Parso i colori degli archi nei grafo: */
      else if (!strcmp (data->value, "graph_edge"))
	init_graph (&graph_edge, data, file);

      /* Il formato dei grafi: */
      else if (!strcmp (data->value, "graph_format"))
	{
	  nxml_get_string (data, &graph_format);

	  if (!graph_format)
	    {
	      g_remove (file);
	      fatal (_("Syntax error in file '%s'"), file);
	    }
	}

      /* Il livello di undo */
      else if (!strcmp (data->value, "undo_max_value"))
	{
	  gchar *t;

	  nxml_get_string (data, &t);
	  undo_max_value = atoi (t);
	  g_free (t);
	}

      /* Il livello di history */
      else if (!strcmp (data->value, "last_document_value"))
	{
	  gchar *t;

	  nxml_get_string (data, &t);
	  last_document_value = atoi (t);
	  g_free (t);
	}

      /* il nome del blanknode: */
      else if (!strcmp (data->value, "blanknode_name"))
	nxml_get_string (data, &blanknode_name);

      /* l'URI per gli aggiornamenti: */
      else if (!strcmp (data->value, "update_url"))
	nxml_get_string (data, &update_url);

      /* l'URI per la documentazione: */
      else if (!strcmp (data->value, "help_url"))
	nxml_get_string (data, &help_url);

      /* Devo mostrare il popup per gli aggiornamenti: */
      else if (!strcmp (data->value, "update_show"))
	{
	  gchar *t;

	  nxml_get_string (data, &t);
	  update_show = atoi (t);
	  g_free (t);
	}

      /* Devo inserire l'estensione di defualt: */
      else if (!strcmp (data->value, "automatic_extensions"))
	{
	  gchar *t;

	  nxml_get_string (data, &t);
	  automatic_extensions = atoi (t) == 1 ? TRUE : FALSE;
	  g_free (t);
	}

      /* Dimensione della finestra: */
      else if (!strcmp (data->value, "size"))
	{
	  nxml_attr_t *attr;

	  if (nxml_find_attribute (data, "height", &attr) == NXML_OK && attr)
	    default_height = atoi (attr->value);

	  if (nxml_find_attribute (data, "width", &attr) == NXML_OK && attr)
	    default_width = atoi (attr->value);
	}

      /* Proxy: */
      else if (!strcmp (data->value, "proxy"))
	{
	  nxml_attr_t *attr;

#ifdef USE_GCONF
	  if (nxml_find_attribute (data, "gconf", &attr) == NXML_OK && attr
	      && !strcmp (attr->value, "true"))
	    download_proxy_gconf = TRUE;
	  else
	    download_proxy_gconf = FALSE;
#endif

	  if (nxml_find_attribute (data, "server", &attr) == NXML_OK && attr)
	    download_proxy = g_strdup (attr->value);

	  if (nxml_find_attribute (data, "port", &attr) == NXML_OK && attr)
	    download_proxy_port = atoi (attr->value);

	  if (nxml_find_attribute (data, "user", &attr) == NXML_OK && attr)
	    download_proxy_user = g_strdup (attr->value);

	  if (nxml_find_attribute (data, "password", &attr) == NXML_OK
	      && attr)
	    download_proxy_password = g_strdup (attr->value);
	}

      /* Highlighting: */
      else if (!strcmp (data->value, "highlighting"))
	{
	  nxml_attr_t *attr;
	  nxml_data_t *child;

	  highlighting_found = TRUE;

	  if (nxml_find_attribute (data, "active", &attr) == NXML_OK && attr
	      && !strcmp (attr->value, "true"))
	    highlighting_active = TRUE;
	  else
	    highlighting_active = FALSE;

	  for (highlighting_tags = NULL, child = data->children; child;
	       child = child->next)
	    {
	      if (!strcmp (child->value, "tag"))
		{
		  struct tag_t *tag;

		  tag = g_malloc0 (sizeof (struct tag_t));
		  highlighting_tags = g_list_append (highlighting_tags, tag);
		  nxml_get_string (child, &tag->tag);

		  for (attr = child->attributes; attr; attr = attr->next)
		    {
		      if (!strcmp (attr->name, "style"))
			tag->style =
			  init_highlighting_str2style (attr->value);
		      else if (!strcmp (attr->name, "variant"))
			tag->variant =
			  init_highlighting_str2variant (attr->value);
		      else if (!strcmp (attr->name, "weight"))
			tag->weight =
			  init_highlighting_str2weight (attr->value);
		      else if (!strcmp (attr->name, "stretch"))
			tag->stretch =
			  init_highlighting_str2stretch (attr->value);
		      else if (!strcmp (attr->name, "scale"))
			tag->scale =
			  init_highlighting_str2scale (attr->value);
		      else if (!strcmp (attr->name, "foreground"))
			tag->foreground = g_strdup (attr->value);
		      else if (!strcmp (attr->name, "background"))
			tag->background = g_strdup (attr->value);
		      else if (!strcmp (attr->name, "underline"))
			tag->underline =
			  init_highlighting_str2underline (attr->value);
		    }
		}
	    }
	}

      /* Cosi' per ogni nodo dell'XML: */
      data = data->next;
    }

  /* Gli ultimi controlli: */
  if (!browser_default)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (!viewer_default)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (!update_url)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (!help_url)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  /* Non presenti nel file di config, quindi versione di morla antecedente
   * il supporto dell'highlighting */
  if (highlighting_found == FALSE)
    {
      highlighting_active = TRUE;
      highlighting_tags = init_highlighting ();
    }

  /* Certi li setto io di default: */
  if (!undo_max_value)
    undo_max_value = UNDO_MAX_VALUE;

  if (!last_document_value)
    last_document_value = LAST_DOCUMENT_VALUE;

  if (!update_show)
    update_show = 1;

  if (!blanknode_name)
    blanknode_name = g_strdup (BLANK_NODE);

  /* Set the morlardf.net uri: */
  website_update (&update_url);
  website_update (&help_url);

  /* Libero libnxml */
  nxml_free (nxml);

  /* Salvataggio per retrocompatibility: */
  if (cp == TRUE)
    init_save ();
}

/* Questa funzione crea i file di configurazione: */
static void
init_create (void)
{
  gchar *file =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE, NULL);

  /* Splash: */
  splash_set_text (_("Creating the config file"));

  /* Creo la directory: */
  if (g_mkdir_with_parents (file, 0755)
      && g_file_test (file, G_FILE_TEST_IS_DIR) == FALSE)
    fatal (_("Error creating the directory '%s'!"), file);

  g_free (file);

  /* Creo la directory per gli rdfs importati: */
  file =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "rdfs", NULL);

  if (g_mkdir_with_parents (file, 0755)
      && g_file_test (file, G_FILE_TEST_IS_DIR) == FALSE)
    fatal (_("Error creating the directory '%s'!"), file);

  g_free (file);

  /* Cerco il programmi presenti sul sistema */
  browser_default = init_browser ();
  viewer_default = init_viewer ();

  /* Setto alcuni valori noti: */
  undo_max_value = UNDO_MAX_VALUE;
  last_document_value = LAST_DOCUMENT_VALUE;
  update_url = g_strdup (UPDATE_URL);
  blanknode_name = g_strdup (BLANK_NODE);
  help_url = g_strdup (HELP_URL);

  automatic_extensions = TRUE;

  highlighting_active = TRUE;
  highlighting_tags = init_highlighting ();

  /* Salvo rdfs di morla di default come importato: */
  init_morla_save ();

  /* Salvo la configurazione appena creata: */
  splash_set_text (_("Saving the config file"));
  init_save ();
}

/* Questa funzione salva la configurazione: */
void
init_save (void)
{
  gchar *str;
  gchar *file;
  FILE *fl;
  GList *list;

  /* Apro il file: */
  file =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "config.xml", NULL);

  if (!(fl = g_fopen (file, "wb")))
    fatal (_("Error creating the file '%s'!"), file);

  g_free (file);

  /* Scrivo dentro l'XML: */
  fprintf (fl, "<?xml version=\"1.0\"?>\n");
  fprintf (fl, "<%s>\n\n", PACKAGE);

  /* Uso markup che e' una funzione per escepare caratteri strani: */
  str = markup (browser_default);
  fprintf (fl, "  <browser>%s</browser>\n", str);
  g_free (str);

  str = markup (viewer_default);
  fprintf (fl, "  <viewer>%s</viewer>\n\n", str);
  g_free (str);

  fprintf (fl, "  <undo_max_value>%d</undo_max_value>\n\n", undo_max_value);
  fprintf (fl, "  <last_document_value>%d</last_document_value>\n\n",
	   last_document_value);

  fprintf (fl, "  <update_show>%d</update_show>\n",
	   update_show >= 0 ? 1 : -1);

  fprintf (fl, "  <automatic_extensions>%d</automatic_extensions>\n",
	   automatic_extensions == TRUE ? 1 : 0);

  str = markup (blanknode_name);
  fprintf (fl, "  <blanknode_name>%s</blanknode_name>\n\n", str);
  g_free (str);

  str = markup (update_url);
  fprintf (fl, "  <update_url>%s</update_url>\n\n", str);
  g_free (str);

  str = markup (help_url);
  fprintf (fl, "  <help_url>%s</help_url>\n\n", str);
  g_free (str);

  fprintf (fl, "  <last_id>%d</last_id>\n\n", last_id);

  fprintf (fl, "  <normal_bg>%s</normal_bg>\n", normal_bg_color);
  fprintf (fl, "  <prelight_bg>%s</prelight_bg>\n", prelight_bg_color);
  fprintf (fl, "  <normal_fg>%s</normal_fg>\n", normal_fg_color);
  fprintf (fl, "  <prelight_fg>%s</prelight_fg>\n\n", prelight_fg_color);

  fprintf (fl, "  <graph_blank_node\n"
	   "    color=\"%s\"\n"
	   "    fontcolor=\"%s\"\n"
	   "    style=\"%s\"\n"
	   "    fillcolor=\"%s\"\n"
	   "    fontname=\"%s\"\n"
	   "    fontsize=\"%d\"\n"
	   "    shape=\"%s\" />\n", graph_blank_node.color,
	   graph_blank_node.fontcolor, graph_blank_node.style,
	   graph_blank_node.fillcolor, graph_blank_node.fontname,
	   graph_blank_node.fontsize, graph_blank_node.shape);

  fprintf (fl, "  <graph_resource_node\n"
	   "    color=\"%s\"\n"
	   "    fontcolor=\"%s\"\n"
	   "    style=\"%s\"\n"
	   "    fillcolor=\"%s\"\n"
	   "    fontname=\"%s\"\n"
	   "    fontsize=\"%d\"\n"
	   "    shape=\"%s\" />\n", graph_resource_node.color,
	   graph_resource_node.fontcolor, graph_resource_node.style,
	   graph_resource_node.fillcolor, graph_resource_node.fontname,
	   graph_resource_node.fontsize, graph_resource_node.shape);

  fprintf (fl, "  <graph_literal_node\n"
	   "    color=\"%s\"\n"
	   "    fontcolor=\"%s\"\n"
	   "    style=\"%s\"\n"
	   "    fillcolor=\"%s\"\n"
	   "    fontname=\"%s\"\n"
	   "    fontsize=\"%d\"\n"
	   "    shape=\"%s\" />\n", graph_literal_node.color,
	   graph_literal_node.fontcolor, graph_literal_node.style,
	   graph_literal_node.fillcolor, graph_literal_node.fontname,
	   graph_literal_node.fontsize, graph_literal_node.shape);

  fprintf (fl, "  <graph_edge\n"
	   "    color=\"%s\"\n"
	   "    fontcolor=\"%s\"\n"
	   "    style=\"%s\"\n"
	   "    fillcolor=\"%s\"\n"
	   "    fontname=\"%s\"\n"
	   "    fontsize=\"%d\"\n"
	   "    shape=\"%s\" />\n", graph_edge.color, graph_edge.fontcolor,
	   graph_edge.style, graph_edge.fillcolor, graph_edge.fontname,
	   graph_edge.fontsize, graph_edge.shape);

  fprintf (fl, "  <graph_format>%s</graph_format>\n\n", graph_format);

  fprintf (fl, "  <size height=\"%d\" width=\"%d\" />\n\n", default_height,
	   default_width);

  fprintf (fl, "  <proxy");

#ifdef USE_GCONF
  fprintf (fl, " gconf=\"%s\"", download_proxy_gconf ? "true" : "false");
#endif

  if (download_proxy)
    fprintf (fl, " server=\"%s\"", download_proxy);

  if (download_proxy_port)
    fprintf (fl, " port=\"%d\"", download_proxy_port);

  if (download_proxy_user)
    fprintf (fl, " user=\"%s\"", download_proxy_user);

  if (download_proxy_password)
    fprintf (fl, " password=\"%s\"", download_proxy_password);

  fprintf (fl, " />\n\n");

  fprintf (fl, "  <highlighting active=\"%s\">\n",
	   highlighting_active == TRUE ? "true" : "false");

  for (list = highlighting_tags; list; list = list->next)
    {
      struct tag_t *tag = list->data;

      fprintf (fl, "    <tag style=\"%s\"",
	       init_highlighting_style2str (tag->style));
      fprintf (fl, " variant=\"%s\"",
	       init_highlighting_variant2str (tag->variant));
      fprintf (fl, " weight=\"%s\"",
	       init_highlighting_weight2str (tag->weight));
      fprintf (fl, " stretch=\"%s\"",
	       init_highlighting_stretch2str (tag->stretch));
      fprintf (fl, " scale=\"%s\"", init_highlighting_scale2str (tag->scale));

      if (tag->foreground)
	fprintf (fl, " foreground=\"%s\"", tag->foreground);

      if (tag->background)
	fprintf (fl, " background=\"%s\"", tag->background);

      fprintf (fl, " underline=\"%s\"",
	       init_highlighting_underline2str (tag->underline));

      fprintf (fl, ">%s</tag>\n", tag->tag);
    }

  fprintf (fl, "  </highlighting>\n\n");

  /* Scrivo tutti gli Rdfs importati: */
  for (list = rdfs_list; list; list = list->next)
    {
      struct rdfs_t *rdfs = list->data;

      fprintf (fl, "  <rdfs");

      if (rdfs->resource)
	{
	  str = markup (rdfs->resource);
	  fprintf (fl, " resource=\"%s\"", str);
	  g_free (str);
	}

      if (rdfs->prefix)
	{
	  str = markup (rdfs->prefix);
	  fprintf (fl, " prefix=\"%s\"", str);
	  g_free (str);
	}

      if (rdfs->path)
	{
	  gchar *path, *real;

	  path =
	    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (),
			  PACKAGE, "rdfs", NULL);
	  real = rdfs->path + strlen (path);
	  g_free (path);

	  while (real[0] && real[0] == G_DIR_SEPARATOR)
	    real++;

	  str = markup (real);
	  fprintf (fl, ">%s</rdfs>\n", str);
	  g_free (str);
	}
      else
	fprintf (fl, " />\n");
    }

  fprintf (fl, "</%s>\n", PACKAGE);

  /* Chiudo e salvo: */
  fclose (fl);
}

/* Questa funzione parsa gli argomenti di un punto colori del config file
 * cercando tutti gli argomenti essenziali: */
static void
init_graph (struct graph_data_t *graph, nxml_data_t * data, gchar * file)
{
  nxml_attr_t *attr;

  if (nxml_find_attribute (data, "color", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    {
      if (graph->color)
	g_free (graph->color);
      graph->color = g_strdup (attr->value);
    }

  if (nxml_find_attribute (data, "fontcolor", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    {
      if (graph->fontcolor)
	g_free (graph->fontcolor);
      graph->fontcolor = g_strdup (attr->value);
    }

  if (nxml_find_attribute (data, "style", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    {
      if (graph->style)
	g_free (graph->style);
      graph->style = g_strdup (attr->value);
    }

  if (nxml_find_attribute (data, "fillcolor", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    {
      if (graph->fillcolor)
	g_free (graph->fillcolor);
      graph->fillcolor = g_strdup (attr->value);
    }

  if (nxml_find_attribute (data, "shape", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    {
      if (graph->shape)
	g_free (graph->shape);
      graph->shape = g_strdup (attr->value);
    }

  if (nxml_find_attribute (data, "fontname", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    {
      if (graph->fontname)
	g_free (graph->fontname);
      graph->fontname = g_strdup (attr->value);
    }

  if (nxml_find_attribute (data, "fontsize", &attr) != NXML_OK)
    {
      g_remove (file);
      fatal (_("Syntax error in file '%s'"), file);
    }

  if (attr)
    graph->fontsize = atoi (attr->value);
}

/* Questa funzione setta i valori di default per i colori del grafo: */
static void
init_graph_reset (void)
{
  graph_blank_node.color = g_strdup ("red");
  graph_blank_node.fontcolor = g_strdup ("black");
  graph_blank_node.style = g_strdup ("filled");
  graph_blank_node.fillcolor = g_strdup ("#2266ff");
#ifndef G_OS_WIN32
  graph_blank_node.fontname = g_strdup ("Sans");
#else
  graph_blank_node.fontname = g_strdup ("Arial");
#endif
  graph_blank_node.shape = g_strdup ("ellipse");
  graph_blank_node.fontsize = 10;

  graph_resource_node.color = g_strdup ("red");
  graph_resource_node.fontcolor = g_strdup ("black");
  graph_resource_node.style = g_strdup ("filled");
  graph_resource_node.fillcolor = g_strdup ("#aabbcc");
#ifndef G_OS_WIN32
  graph_resource_node.fontname = g_strdup ("Sans");
#else
  graph_resource_node.fontname = g_strdup ("Arial");
#endif
  graph_resource_node.shape = g_strdup ("ellipse");
  graph_resource_node.fontsize = 10;

  graph_literal_node.color = g_strdup ("#ff22aa");
  graph_literal_node.fontcolor = g_strdup ("black");
  graph_literal_node.style = g_strdup ("filled");
  graph_literal_node.fillcolor = g_strdup ("#ccbbaa");
#ifndef G_OS_WIN32
  graph_literal_node.fontname = g_strdup ("Sans");
#else
  graph_literal_node.fontname = g_strdup ("Arial");
#endif
  graph_literal_node.fontsize = 10;
  graph_literal_node.shape = g_strdup ("box");

  graph_edge.color = g_strdup ("black");
  graph_edge.fontcolor = g_strdup ("black");
  graph_edge.style = g_strdup ("solid");
  graph_edge.fillcolor = g_strdup ("#ccbbaa");
#ifndef G_OS_WIN32
  graph_edge.fontname = g_strdup ("Sans");
#else
  graph_edge.fontname = g_strdup ("Arial");
#endif
  graph_edge.fontsize = 10;
  graph_edge.shape = g_strdup ("box");

  graph_format = g_strdup ("png");
}

/* Questa funzione setta i colori dei default per morla: */
static void
init_color_reset (void)
{
  normal_bg_color = g_strdup ("#ffffff");
  prelight_bg_color = g_strdup ("#00ffff");
  normal_fg_color = g_strdup ("#10297c");
  prelight_fg_color = g_strdup ("#8c0498");
}

#ifndef G_OS_WIN32
/* Questa funzione cerca un programma 'sw' usando la variabile di ambiente
 * PATH. Se lo trova, comunica ok: */
static gboolean
init_stat (gchar * sw)
{
  gchar *path;

  if ((path = g_find_program_in_path (sw)))
    {
      g_free (path);
      return 0;
    }

  return 1;
}
#endif

/* Questa funzione cerca i programmi di default per morla win32 version */
static gchar *
init_browser (void)
{
#ifdef G_OS_WIN32
  GList *list, *tmp;
  GString *ret;
  char szDefault[MAX_PATH];
  DWORD ccDefault = MAX_PATH;
  HRESULT rc = AssocQueryString (0, ASSOCSTR_EXECUTABLE,
				 "http", "open", szDefault, &ccDefault);

  if (!SUCCEEDED (rc) || !(tmp = list = split (szDefault)))
    return g_strdup ("firefox %s");

  ret = g_string_new (NULL);
  g_string_append_printf (ret, "\"%s\"", (gchar *) tmp->data);

  tmp = tmp->next;

  while (tmp)
    {
      if (!strcmp ((gchar *) tmp->data, "%1"))
	g_string_append_printf (ret, " %%s");
      else
	g_string_append_printf (ret, " \"%s\"", (gchar *) tmp->data);

      tmp = tmp->next;
    }

  g_list_foreach (list, (GFunc) g_free, NULL);
  g_list_free (list);

  return g_string_free (ret, FALSE);

#else

#ifdef ENABLE_MACOSX
  return g_strdup ("open %s");
#endif

  if (!init_stat ("x-www-browser"))
    return g_strdup ("x-www-browser %s");

  if (!init_stat ("firefox"))
    return g_strdup ("firefox %s");

  if (!init_stat ("mozilla"))
    return g_strdup ("mozilla %s");

  if (!init_stat ("dillo"))
    return g_strdup ("dillo %s");

  if (!init_stat ("epiphany"))
    return g_strdup ("epiphany %s");

  if (!init_stat ("konqueror"))
    return g_strdup ("konqueror %s");

  if (!init_stat ("galeon"))
    return g_strdup ("galeon %s");

  return g_strdup ("firefox %s");
#endif
}

/* Questa funzione cerca il viewer: */
static gchar *
init_viewer (void)
{
#ifdef G_OS_WIN32
  GList *tmp, *list;
  GString *ret;
  char szDefault[MAX_PATH];
  DWORD ccDefault = MAX_PATH;
  HRESULT rc = AssocQueryString (0, ASSOCSTR_EXECUTABLE,
				 ".png", "open", szDefault, &ccDefault);

  if (!SUCCEEDED (rc) || !(tmp = list = split (szDefault)))
    return g_strdup ("firefox %s");

  ret = g_string_new (NULL);
  g_string_append_printf (ret, "\"%s\"", (gchar *) tmp->data);

  tmp = tmp->next;

  while (tmp)
    {
      if (!strcmp ((gchar *) tmp->data, "%1"))
	g_string_append_printf (ret, " %%s");
      else
	g_string_append_printf (ret, " \"%s\"", (gchar *) tmp->data);

      tmp = tmp->next;
    }

  g_list_foreach (list, (GFunc) g_free, NULL);
  g_list_free (list);

  return g_string_free (ret, FALSE);
#else

#ifdef ENABLE_MACOSX
  return g_strdup ("open %s");
#endif

  if (!init_stat ("gqview"))
    return g_strdup ("gqview -f %s");

  if (!init_stat ("gimp"))
    return g_strdup ("gimp -d -s -f %s");

  if (!init_stat ("kview"))
    return g_strdup ("kview %s");

  return g_strdup ("gqview -f %s");
#endif
}

/* Questa funzione salva l'RDFS di morla: */
static void
init_morla_save (void)
{
  gchar *file;
  int fd;
  int len, done, ret;
  gchar *uri;
  struct rdfs_t *rdfs;
  GList *list;

  splash_set_text (_("Creating the RDFS morla file"));

  /* Cancello il precedente: */
  file = rdfs_file ();
  g_remove (file);

  /* Apro, scrivo, chiudo: */
  if ((fd = open (file, O_WRONLY | O_CREAT, 0644)) < 0)
    return;

  len = strlen (morla_rdfs);
  done = 0;

  while (done < len)
    {
      if ((ret = write (fd, morla_rdfs + done, len - done)) < 1)
	fatal (_("Error writing file '%s'!"), file);

      done += ret;
    }

  close (fd);

  /* Parso il nuovo file: */
  uri = g_filename_to_uri (file, NULL, NULL);

  if (!uri || rdf_parse (uri, NULL, FALSE, &list, NULL, NULL) == FALSE)
    {
      if (uri)
	g_free (uri);

      g_remove (file);
      return;
    }

  /* Creo la struttura e inserisco: */
  rdfs = (struct rdfs_t *) g_malloc0 (sizeof (struct rdfs_t));

  rdfs->resource = g_strdup (MORLA_NS);
  rdfs->prefix = g_strdup ("morla");	/* Si questo di default... */

  rdfs->data = list;
  namespace_parse (list, &rdfs->nsdata);
  rdfs->path = file;

  rdfs_list = g_list_append (rdfs_list, rdfs);

  /* Fatto questo importo i template presenti nell'RDFS di morla: */
  template_import (uri, NULL, FALSE, FALSE);
  template_save ();

  g_free (uri);
}

/* Parsa il file di template e lo gestisce: */
static void
init_template (void)
{
  gchar *path;
  gchar *uri;

  splash_set_text (_("Reading the template file"));

  /* Se non esiste esco: */
  path =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "template.rdf", NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS) == FALSE)
    {
      g_free (path);
      return;
    }

  /* Importo questo uri: */
  uri = g_filename_to_uri (path, NULL, NULL);

  if (!uri || template_import (uri, NULL, FALSE, FALSE) == FALSE)
    {
      splash_hide ();
      dialog_msg (_("Import template error!"));
      g_remove (path);
    }

  if (uri)
    g_free (uri);

  g_free (path);
}

/* Tags per l'highlighting: */
GList *
init_highlighting (void)
{
  GList *list = NULL;
  struct tag_t *tag;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("SELECT");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#CC33CC");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("CONSTRUCT");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#CC33CC");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("DESCRIBE");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#CC33CC");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("ASK");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#CC33CC");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("PREFIX");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#337733");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("WHERE");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#559911");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("FILTER");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#1199FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("OPTIONAL");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#1199FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("FROM");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#FF2222");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("NAMED");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#FF2222");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("GRAPH");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#22FF22");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("ORDER");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#2222FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("BY");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#2222FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("DESC");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#9999FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("DISTINCT");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#99FF99");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("REDUCED");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#FF9999");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("LIMIT");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#1199FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  tag = g_malloc0 (sizeof (struct tag_t));
  list = g_list_append (list, tag);

  tag->tag = g_strdup ("OFFSET");
  tag->style = PANGO_STYLE_NORMAL;
  tag->variant = PANGO_VARIANT_NORMAL;
  tag->weight = PANGO_WEIGHT_BOLD;
  tag->stretch = PANGO_STRETCH_NORMAL;
  tag->scale = PANGO_SCALE_MEDIUM;
  tag->foreground = g_strdup ("#1199FF");
  tag->background = NULL;
  tag->underline = PANGO_UNDERLINE_NONE;

  return list;
}

static gchar *
init_highlighting_style2str (PangoStyle style)
{
  switch (style)
    {
    case PANGO_STYLE_OBLIQUE:
      return "STYLE_OBLIQUE";
    case PANGO_STYLE_ITALIC:
      return "STYLE_ITALIC";
    default:
      return "STYLE_NORMAL";
    }
}

static gchar *
init_highlighting_variant2str (PangoVariant variant)
{
  switch (variant)
    {
    case PANGO_VARIANT_SMALL_CAPS:
      return "VARIANT_SMALL_CAPS";
    default:
      return "VARIANT_NORMAL";
    }
}

static gchar *
init_highlighting_weight2str (PangoWeight weight)
{
  switch (weight)
    {
    case PANGO_WEIGHT_ULTRALIGHT:
      return "WEIGHT_ULTRALIGHT";
    case PANGO_WEIGHT_LIGHT:
      return "WEIGHT_LIGHT";
    case PANGO_WEIGHT_SEMIBOLD:
      return "WEIGHT_SEMIBOLD";
    case PANGO_WEIGHT_BOLD:
      return "WEIGHT_BOLD";
    case PANGO_WEIGHT_ULTRABOLD:
      return "WEIGHT_ULTRABOLD";
    case PANGO_WEIGHT_HEAVY:
      return "WEIGHT_HEAVY";
    default:
      return "WEIGHT_NORMAL";
    }
}

static gchar *
init_highlighting_stretch2str (PangoStretch stretch)
{
  switch (stretch)
    {
    case PANGO_STRETCH_ULTRA_CONDENSED:
      return "STRETCH_ULTRA_CONDENSED";
    case PANGO_STRETCH_EXTRA_CONDENSED:
      return "STRETCH_EXTRA_CONDENSED";
    case PANGO_STRETCH_CONDENSED:
      return "STRETCH_CONDENSED";
    case PANGO_STRETCH_SEMI_CONDENSED:
      return "STRETCH_SEMI_CONDENSED";
    case PANGO_STRETCH_SEMI_EXPANDED:
      return "STRETCH_SEMI_EXPANDED";
    case PANGO_STRETCH_EXPANDED:
      return "STRETCH_EXPANDED";
    case PANGO_STRETCH_EXTRA_EXPANDED:
      return "STRETCH_EXTRA_EXPANDED";
    case PANGO_STRETCH_ULTRA_EXPANDED:
      return "STRETCH_ULTRA_EXPANDED";
    default:
      return "STRETCH_NORMAL";
    }
}

static gchar *
init_highlighting_scale2str (gdouble scale)
{
  if (scale == PANGO_SCALE_XX_SMALL)
    return "SCALE_XX_SMALL";
  if (scale == PANGO_SCALE_X_SMALL)
    return "SCALE_X_SMALL";
  if (scale == PANGO_SCALE_SMALL)
    return "SCALE_SMALL";
  if (scale == PANGO_SCALE_LARGE)
    return "SCALE_LARGE";
  if (scale == PANGO_SCALE_X_LARGE)
    return "SCALE_X_LARGE";
  if (scale == PANGO_SCALE_XX_LARGE)
    return "SCALE_XX_LARGE";
  return "SCALE_MEDIUM";
}

static gchar *
init_highlighting_underline2str (PangoUnderline underline)
{
  switch (underline)
    {
    case PANGO_UNDERLINE_SINGLE:
      return "UNDERLINE_SINGLE";
    case PANGO_UNDERLINE_DOUBLE:
      return "UNDERLINE_DOUBLE";
    default:
      return "UNDERLINE_NONE";
    }
}

static PangoStyle
init_highlighting_str2style (gchar * str)
{
  if (!strcmp (str, "STYLE_OBLIQUE"))
    return PANGO_STYLE_OBLIQUE;
  if (!strcmp (str, "STYLE_ITALIC"))
    return PANGO_STYLE_ITALIC;
  return PANGO_STYLE_NORMAL;
}

static PangoVariant
init_highlighting_str2variant (gchar * str)
{
  if (!strcmp (str, "VARIANT_SMALL_CAPS"))
    return PANGO_VARIANT_SMALL_CAPS;
  return PANGO_VARIANT_NORMAL;
}

static PangoWeight
init_highlighting_str2weight (gchar * str)
{
  if (!strcmp (str, "WEIGHT_ULTRALIGHT"))
    return PANGO_WEIGHT_ULTRALIGHT;
  if (!strcmp (str, "WEIGHT_LIGHT"))
    return PANGO_WEIGHT_LIGHT;
  if (!strcmp (str, "WEIGHT_SEMIBOLD"))
    return PANGO_WEIGHT_SEMIBOLD;
  if (!strcmp (str, "WEIGHT_BOLD"))
    return PANGO_WEIGHT_BOLD;
  if (!strcmp (str, "WEIGHT_ULTRABOLD"))
    return PANGO_WEIGHT_ULTRABOLD;
  if (!strcmp (str, "WEIGHT_HEAVY"))
    return PANGO_WEIGHT_HEAVY;
  return PANGO_WEIGHT_NORMAL;
}

static PangoStretch
init_highlighting_str2stretch (gchar * str)
{
  if (!strcmp (str, "STRETCH_ULTRA_CONDENSED"))
    return PANGO_STRETCH_ULTRA_CONDENSED;
  if (!strcmp (str, "STRETCH_EXTRA_CONDENSED"))
    return PANGO_STRETCH_EXTRA_CONDENSED;
  if (!strcmp (str, "STRETCH_CONDENSED"))
    return PANGO_STRETCH_CONDENSED;
  if (!strcmp (str, "STRETCH_SEMI_CONDENSED"))
    return PANGO_STRETCH_SEMI_CONDENSED;
  if (!strcmp (str, "STRETCH_SEMI_EXPANDED"))
    return PANGO_STRETCH_SEMI_EXPANDED;
  if (!strcmp (str, "STRETCH_EXPANDED"))
    return PANGO_STRETCH_EXPANDED;
  if (!strcmp (str, "STRETCH_EXTRA_EXPANDED"))
    return PANGO_STRETCH_EXTRA_EXPANDED;
  if (!strcmp (str, "STRETCH_ULTRA_EXPANDED"))
    return PANGO_STRETCH_ULTRA_EXPANDED;
  return PANGO_STRETCH_NORMAL;
}

static gdouble
init_highlighting_str2scale (gchar * str)
{
  if (!strcmp (str, "SCALE_XX_SMALL"))
    return PANGO_SCALE_XX_SMALL;
  if (!strcmp (str, "SCALE_X_SMALL"))
    return PANGO_SCALE_X_SMALL;
  if (!strcmp (str, "SCALE_SMALL"))
    return PANGO_SCALE_SMALL;
  if (!strcmp (str, "SCALE_LARGE"))
    return PANGO_SCALE_LARGE;
  if (!strcmp (str, "SCALE_X_LARGE"))
    return PANGO_SCALE_X_LARGE;
  if (!strcmp (str, "SCALE_XX_LARGE"))
    return PANGO_SCALE_XX_LARGE;
  return PANGO_SCALE_MEDIUM;
}

static PangoUnderline
init_highlighting_str2underline (gchar * str)
{
  if (!strcmp (str, "UNDERLINE_SINGLE"))
    return PANGO_UNDERLINE_SINGLE;
  if (!strcmp (str, "UNDERLINE_DOUBLE"))
    return PANGO_UNDERLINE_DOUBLE;
  return PANGO_UNDERLINE_NONE;
}

/* This function change to the new website the URL: */
static void
website_update (gchar ** str)
{
  if (!str || !*str)
    return;

  if (!strncmp (*str, MORLA_OLD_WEBSITE_1, strlen (MORLA_OLD_WEBSITE_1)))
    {
      gchar *new = g_strdup_printf ("%s%s", MORLA_WEBSITE,
				    (*str) + strlen (MORLA_OLD_WEBSITE_1));
      g_free (*str);
      *str = new;
      return;
    }

  if (!strncmp (*str, MORLA_OLD_WEBSITE_2, strlen (MORLA_OLD_WEBSITE_2)))
    {
      gchar *new = g_strdup_printf ("%s%s", MORLA_WEBSITE,
				    (*str) + strlen (MORLA_OLD_WEBSITE_2));
      g_free (*str);
      *str = new;
      return;
    }

  if (!strncmp (*str, MORLA_OLD_WEBSITE_3, strlen (MORLA_OLD_WEBSITE_3)))
    {
      gchar *new = g_strdup_printf ("%s%s", MORLA_WEBSITE,
				    (*str) + strlen (MORLA_OLD_WEBSITE_3));
      g_free (*str);
      *str = new;
      return;
    }
}

static gboolean
init_retrocompatibility (void)
{
  gchar *path, *new;
  GDir *dir;

  gboolean ret = FALSE;

  if (!strcmp (g_get_user_config_dir (), g_get_home_dir ()))
    return FALSE;

  path =
    g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), "." PACKAGE, NULL);

  if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) == FALSE)
    {
      g_free (path);
      return FALSE;
    }

  new =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE, NULL);

  if (g_file_test (new, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) == FALSE)
    {
      if (g_mkdir_with_parents (new, 0755) < 0)
	{
	  g_free (new);
	  g_free (path);
	  return FALSE;
	}
    }

  if ((dir = g_dir_open (path, 0, NULL)))
    {
      const gchar *file;
      gchar *ofile, *nfile;

      while ((file = g_dir_read_name (dir)))
	{
	  ofile =
	    g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), "." PACKAGE,
			  file, NULL);
	  nfile =
	    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (),
			  PACKAGE, file, NULL);

	  g_rename (ofile, nfile);

	  g_free (nfile);
	  g_free (ofile);
	}

      g_dir_close (dir);
      ret = TRUE;

      g_rmdir (path);
    }

  g_free (new);
  g_free (path);

  return ret;
}

/* EOF */

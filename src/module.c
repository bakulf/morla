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

static GList *modules_list = NULL;
static struct module_t *module_default_open_p = NULL;
static struct module_t *module_default_save_p = NULL;

static struct module_t *module_load (gchar * module);
static void module_run_view (GtkWidget * widget, struct module_t *module);
static GList *module_list_module_create (GList *);
static GList *module_list_morla_create (GList *);
static void module_list_free (GList * list);
static const gchar *module_path (void);

void
module_init (gchar * config)
{
  gchar *index;
  gchar *buffer;
  gsize len;

  nxml_t *nxml;
  nxml_data_t *cur;
  nxml_attr_t *attr;

  struct module_t *module;

  gchar *str;

  if (!g_module_supported () == FALSE)
    return;

  if (config)
    {
      if (g_file_test (module_path (), G_FILE_TEST_EXISTS) == FALSE)
	{
	  g_message (_("The config file '%s' doesn't exist!"), config);
	  return;
	}
      return;
    }

  else if (g_file_test (module_path (), G_FILE_TEST_IS_DIR) == FALSE)
    return;

  if (config)
    index = g_strdup (config);
  else
    if (!
	(index =
	 g_build_path (G_DIR_SEPARATOR_S, module_path (), "index.xml", NULL)))
    return;

  if (g_file_get_contents (index, &buffer, &len, NULL) == FALSE)
    {
      g_free (index);
      return;
    }

  g_free (index);

  if (nxml_new (&nxml) != NXML_OK)
    {
      g_free (buffer);
      return;
    }

  if (nxml_parse_buffer (nxml, buffer, len) != NXML_OK
      || nxml_root_element (nxml, &cur) != NXML_OK || !cur
      || strcmp (cur->value, "morla_modules"))
    {
      nxml_free (nxml);
      g_free (buffer);
      return;
    }

  for (cur = cur->children; cur; cur = cur->next)
    {
      if (cur->type == NXML_TYPE_ELEMENT && !strcmp (cur->value, "module")
	  && nxml_get_string (cur, &str) == NXML_OK && str)
	{
	  splash_set_text (_("Loading module '%s'"), str);
	  module = module_load (str);
	  free (str);

	  if (module
	      && nxml_find_attribute (cur, "default_open", &attr) == NXML_OK
	      && attr && !strcmp (attr->value, "yes"))
	    {
	      if (module_default_open_p)
		g_message ("A default open module already exists!");
	      else
		module_default_open_p = module;
	    }

	  if (module
	      && nxml_find_attribute (cur, "default_save", &attr) == NXML_OK
	      && attr && !strcmp (attr->value, "yes"))
	    {
	      if (module_default_save_p)
		g_message ("A default save module already exists!");
	      else
		module_default_save_p = module;
	    }
	}
    }

  nxml_free (nxml);
  g_free (buffer);
}

void
module_shutdown (void)
{
  GList *list;
  gpointer pointer;
  MorlaModuleShutdown morla_module_shutdown;
  struct module_t *data;

  for (list = modules_list; list; list = list->next)
    {
      data = list->data;

      if (g_module_symbol
	  (data->module, "morla_module_shutdown",
	   (gpointer *) & pointer) == TRUE)
	{
	  morla_module_shutdown = pointer;
	  morla_module_shutdown (data->data);
	}

      g_module_close (data->module);
      g_free (data);
    }

  g_list_free (modules_list);
}

static struct module_t *
module_load (gchar * name)
{
  MorlaModuleInit morla_module_init;
  struct module_t *data = NULL;
  gpointer pointer;
  GModule *module;
  gchar *file;

  if (!(file = g_module_build_path (module_path (), name)))
    return NULL;

  if ((module = g_module_open (file, G_MODULE_BIND_MASK)))
    {
      if (g_module_symbol
	  (module, "morla_module_init", (gpointer *) & pointer) == FALSE)
	{
	  g_message
	    ("No 'morla_module_init' for the module '%s'. This module is disabled.",
	     name);
	  g_module_close (module);
	}

      else
	{
	  morla_module_init = pointer;

	  if (!(pointer = morla_module_init ()))
	    {
	      g_message
		("morla_module_init function returns NULL for the module '%s'. This module is disabled.",
		 name);
	      g_module_close (module);
	    }
	  else
	    {
	      data = g_malloc0 (sizeof (struct module_t));
	      data->module = module;
	      data->data = pointer;
	      modules_list = g_list_append (modules_list, data);
	    }
	}
    }
  else
    g_message ("Error loading the module '%s': %s", name, g_module_error ());

  g_free (file);
  return data;
}

void
module_menu_open (GtkMenu * menu, GtkWidget * parent)
{
  GtkWidget *menuitem;
  struct module_t *module;
  GList *list;
  gboolean empty = TRUE;

  for (list = modules_list; list; list = list->next)
    {
      module = list->data;

      if (module->data->open_function)
	{
	  if (module->data->open_label)
	    menuitem =
	      gtk_menu_item_new_with_label (module->data->open_label);
	  else
	    {
	      gchar *s = g_strdup_printf ("Open with the module '%s'",
					  module->data->module_name ? module->
					  data->
					  module_name :
					  g_module_name (module->module));

	      menuitem = gtk_menu_item_new_with_label (s);
	      g_free (s);
	    }

	  gtk_container_add (GTK_CONTAINER (menu), menuitem);

	  g_signal_connect ((gpointer) menuitem, "activate",
			    G_CALLBACK (module_run_open), module);
	  empty = FALSE;
	}
    }

  if (empty == TRUE)
    gtk_widget_set_sensitive (parent, FALSE);
}

void
module_menu_save (GtkMenu * menu, GtkWidget * parent)
{
  GtkWidget *menuitem;
  struct module_t *module;
  GList *list;
  gboolean empty = TRUE;

  for (list = modules_list; list; list = list->next)
    {
      module = list->data;

      if (module->data->save_function)
	{
	  if (module->data->save_label)
	    menuitem =
	      gtk_menu_item_new_with_label (module->data->save_label);
	  else
	    {
	      gchar *s = g_strdup_printf ("Save with the module '%s'",
					  module->data->module_name ? module->
					  data->
					  module_name :
					  g_module_name (module->module));

	      menuitem = gtk_menu_item_new_with_label (s);
	      g_free (s);
	    }

	  gtk_container_add (GTK_CONTAINER (menu), menuitem);

	  g_signal_connect ((gpointer) menuitem, "activate",
			    G_CALLBACK (module_run_save), module);

	  empty = FALSE;
	}
    }

  if (empty == TRUE)
    gtk_widget_set_sensitive (parent, FALSE);
}

void
module_menu_view (GtkMenu * menu, GtkWidget * parent)
{
  GtkWidget *menuitem;
  struct module_t *module;
  GList *list;
  gboolean empty = TRUE;

  for (list = modules_list; list; list = list->next)
    {
      module = list->data;

      if (module->data->view_function)
	{
	  if (module->data->view_label)
	    menuitem =
	      gtk_menu_item_new_with_label (module->data->view_label);
	  else
	    {
	      gchar *s = g_strdup_printf ("View with the module '%s'",
					  module->data->module_name ? module->
					  data->
					  module_name :
					  g_module_name (module->module));

	      menuitem = gtk_menu_item_new_with_label (s);
	      g_free (s);
	    }

	  gtk_container_add (GTK_CONTAINER (menu), menuitem);

	  g_signal_connect ((gpointer) menuitem, "activate",
			    G_CALLBACK (module_run_view), module);

	  empty = FALSE;
	}
    }

  if (empty == TRUE)
    gtk_widget_set_sensitive (parent, FALSE);
}

void
module_run_open (GtkWidget * widget, struct module_t *module)
{
  GList *list = NULL;
  gchar *uri = NULL;

  struct editor_data_t *data;

  if (module->data->open_function (module->data, &list, &uri) == TRUE)
    {
      if (uri)
	{
	  GList *real = module_list_morla_create (list);
	  module_list_free (list);

	  data = g_malloc0 (sizeof (struct editor_data_t));
	  data->format = RDF_FORMAT_XML;
	  data->uri = uri;

	  if (namespace_parse (real, &data->namespace) == FALSE)
	    data->namespace = NULL;

	  data->rdf = real;
	  editor_new_maker (data);
	}
      else
	g_message ("The module '%s' returns a NULL uri!",
		   g_module_name (module->module));
    }
}

void
module_run_save (GtkWidget * widget, struct module_t *module)
{
  struct editor_data_t *data = editor_get_data ();
  GList *list;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  list = module_list_module_create (data->rdf);

  if (module->data->save_function (module->data, list) == TRUE)
    data->module_save = module;

  module_list_free (list);
}

static void
module_run_view (GtkWidget * widget, struct module_t *module)
{
  struct editor_data_t *data = editor_get_data ();
  GList *list;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  list = module_list_module_create (data->rdf);

  module->data->view_function (module->data, list);

  module_list_free (list);
}

struct module_t *
module_default_open (void)
{
  return module_default_open_p;
}

struct module_t *
module_default_save (void)
{
  return module_default_save_p;
}


static GList *
module_list_module_create (GList * l)
{
  GList *list;

  for (list = NULL; l; l = l->next)
    {
      struct rdf_t *rdf = l->data;

      MorlaRdf *new = g_malloc0 (sizeof (MorlaRdf));
      new->subject = rdf->subject ? g_strdup (rdf->subject) : NULL;
      new->predicate = rdf->predicate ? g_strdup (rdf->predicate) : NULL;
      new->object = rdf->object ? g_strdup (rdf->object) : NULL;;
      new->object_type = rdf->object_type;
      new->lang = rdf->lang ? g_strdup (rdf->lang) : NULL;;
      new->datatype = rdf->datatype ? g_strdup (rdf->datatype) : NULL;;

      list = g_list_append (list, new);
    }

  return list;
}

static GList *
module_list_morla_create (GList * l)
{
  GList *real;

  for (real = NULL; l; l = l->next)
    {
      MorlaRdf *rdf = l->data;

      struct rdf_t *new = g_malloc0 (sizeof (struct rdf_t));

      if (rdf->subject)
	new->subject = g_strdup (rdf->subject);

      if (rdf->predicate)
	new->predicate = g_strdup (rdf->predicate);

      if (rdf->object)
	new->object = g_strdup (rdf->object);

      new->object_type = rdf->object_type;

      if (rdf->lang)
	new->lang = g_strdup (rdf->lang);

      if (rdf->datatype)
	new->datatype = g_strdup (rdf->datatype);

      real = g_list_append (real, new);
    }

  return real;
}

static void
module_list_free (GList * list)
{
  GList *l;

  for (l = list; l; l = l->next)
    {
      MorlaRdf *rdf = l->data;

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

  g_list_free (list);
}

/* Public Functions */
void
morla_dialog_msg (gchar * message)
{
  g_return_if_fail (message != NULL);
  dialog_msg (message);
}

gboolean
morla_dialog_ask (gchar * message)
{
  g_return_val_if_fail (message != NULL, FALSE);

  if (dialog_ask (message) == GTK_RESPONSE_OK)
    return TRUE;

  return FALSE;
}

gboolean
morla_rdf_parse (gchar * buffer, gchar * uri, GList ** ret)
{
  GList *list;

  g_return_val_if_fail (buffer != NULL, FALSE);
  g_return_val_if_fail (ret != NULL, FALSE);

  list = *ret = NULL;

  if (rdf_parse_format (uri, buffer, FALSE, &list, RDF_FORMAT_XML, NULL) ==
      FALSE
      && rdf_parse_format (uri, buffer, FALSE, &list, RDF_FORMAT_NTRIPLES,
			   NULL) == FALSE
      && rdf_parse_format (uri, buffer, FALSE, &list, RDF_FORMAT_TURTLE,
			   NULL) == FALSE)
    return FALSE;

  *ret = module_list_module_create (list);

  g_list_foreach (list, (GFunc) rdf_free, NULL);
  g_list_free (list);
  return TRUE;
}

void
morla_rdf_free (GList * list)
{
  g_return_if_fail (list != NULL);
  module_list_free (list);
}

static gboolean
morla_rdf_write (GList * list, gchar ** buffer_ret, enum rdf_format_t format)
{
  GList *rdf;
  gboolean ret;
  GList *namespace = NULL;

  if (!(rdf = module_list_morla_create (list)))
    return FALSE;

  if (namespace_parse (rdf, &namespace) == FALSE)
    namespace = NULL;

  ret = rdf_save_buffer (rdf, &namespace, format, buffer_ret);

  g_list_foreach (rdf, (GFunc) rdf_free, NULL);
  g_list_free (rdf);

  g_list_foreach (namespace, (GFunc) namespace_free, NULL);
  g_list_free (namespace);

  return ret;
}

gboolean
morla_rdf_write_xml (GList * list, gchar ** buffer_ret)
{
  return morla_rdf_write (list, buffer_ret, RDF_FORMAT_XML);
}

gboolean
morla_rdf_write_ntriples (GList * list, gchar ** buffer_ret)
{
  return morla_rdf_write (list, buffer_ret, RDF_FORMAT_NTRIPLES);
}

gboolean
morla_rdf_write_turtle (GList * list, gchar ** buffer_ret)
{
  return morla_rdf_write (list, buffer_ret, RDF_FORMAT_TURTLE);
}

static const gchar *
module_path (void)
{
  static const gchar *path;
  static gboolean done = FALSE;

#ifndef ENABLE_MACOSX
  return PATH_MODULE;
#endif

  if (done == FALSE)
    {
      path = g_getenv ("MORLA_PATH_MODULE");
      done = TRUE;
    }

  if (!path)
    return PATH_MODULE;

  return path;
}

/* EOF */

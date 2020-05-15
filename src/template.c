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

struct invisible_t
{
  struct template_input_t *input;
  struct rdf_t *rdf;
};

enum template_widget_t
{
  TEMPLATE_WIDGET_LITERAL,
  TEMPLATE_WIDGET_RESOURCE,
  TEMPLATE_WIDGET_ALT_LITERAL,
  TEMPLATE_WIDGET_ALT_RESOURCE,
  TEMPLATE_WIDGET_CARDINALITY,
  TEMPLATE_WIDGET_OTHER,
  TEMPLATE_WIDGET_DEFAULT
};

struct template_func_t
{
  gchar *(*name) (GtkWidget *, gchar *);
    gboolean (*low_check) (GtkWidget *, gchar *, gint min_cardinality);
    gboolean (*high_check) (GtkWidget *, gchar *, gint min_cardinality);
  void (*destroy) (GtkWidget *);
  void (*save) (GtkWidget *, struct editor_data_t *, gchar * subject,
		gchar * predicate, struct unredo_t * unredo,
		struct rdf_t * rdf_prev);
    gboolean (*is_your) (GtkWidget *, struct rdf_t *, GList **);
  void (*get) (GtkWidget *, struct template_value_t * value);
  void (*set) (GtkWidget *, struct template_value_t * value);

  enum template_widget_t type;
};

GList *template_menus = NULL;
GList *template_list = NULL;

static void template_import_new (gchar * template, GList ** list,
				 GList * rdf);
static void template_parse_input (gchar * input, GList ** list, GList * rdf);
static gchar *template_parse_input_typevalue (gchar * node, GList * rdf,
					      GList ** alt);
static void template_input_free (struct template_input_t *i);
static void template_free (struct template_t *t);
static void template_refresh (GtkWidget * treeview);
static gint template_remove (GtkWidget * treeview);
static void template_open_dialog (GtkWidget * treeview);
static void template_activate (GtkWidget * w, GtkTreePath * p,
			       GtkTreeViewColumn * c, GtkWidget * dialog);
static GtkWidget *template_widget_create (GList * resources, GList * blanks,
					  struct template_input_t *,
					  gboolean cardinality
#ifdef USE_JS
					  , struct js_t *js
#endif
  );
static gboolean template_widget_check (GtkWidget * widget);
static void template_widget_save_on (GtkWidget * widget);
static gboolean template_widget_is_your (GtkWidget *, struct rdf_t *,
					 GList **);
static void template_widget_save_off (GtkWidget * widget);
static void template_widget_save_reset (GtkWidget * widget);
static void template_widget_save (GtkWidget * widget, struct editor_data_t *,
				  gchar *, struct unredo_t *unredo);
static void template_widget_set (GtkWidget * widget,
				 struct template_value_t *);
static void template_widget_destroy (GtkWidget * wiget);
static gchar *template_widget_name (GtkWidget * wiget);
static gchar *template_widget_blank (struct editor_data_t *data);
static void template_invisible_save (gchar * subject,
				     struct template_input_t *i,
				     struct editor_data_t *data,
				     struct unredo_t *unredo, struct rdf_t *);
static void template_update_real (GtkWidget * menu);
static gboolean template_run (GtkWidget * widget, struct template_t *);
static GtkWidget *template_run_create (GtkWidget * widget_inputs,
				       struct template_t *t,
				       GList * resources, GList * blanks,
				       GtkWidget ** type_resource,
				       GtkWidget ** type_blanknode,
				       GtkWidget ** object,
				       GtkWidget ** toggle);
static void template_edit_set (GList ** wi, GList ** rdf, gchar * subject);
static GList *template_edit_set_sort (GList * widgets);
static void template_edit_invisible (GList ** invisible, GList ** rdf,
				     gchar * subject, struct template_t *t);
static void template_set_size (GtkWidget * dialog, GtkWidget * sw,
			       GtkWidget *);

#ifdef USE_JS
static gboolean template_destroy (GtkWidget * widget, GdkEvent * event,
				  gpointer user_data);
static gboolean template_js_evaluate (struct js_t *js, GtkWidget * widget,
				      gchar * func);
#endif

/* UPDATE DEI MENU **********************************************************/

/* Questa funzione aggiorna i menu con i template importati: */
void
template_update (void)
{
  g_list_foreach (template_menus, (GFunc) template_update_real, NULL);
}

/* Check fra le categorie di due template (utile per l'ordinamento) : */
static gint
template_strcmp (struct template_t *a, struct template_t *b)
{
  if (!a->category && !b->category)
    return 0;

  if (!a->category && b->category)
    return 1;

  if (a->category && !b->category)
    return -1;

  return strcmp (a->category, b->category);
}

/* Estrapolare un nome sensato da una categoria sotto forma di URI: */
static gchar *
template_parse_category (gchar * str)
{
  gint len = strlen (str);

  while (--len)
    {
      if (*(str + len) == '#' || *(str + len) == '/')
	return str + len + 1;
    }

  return str;
}

/* Questa funzione aggiorna un menu con la lista dei template: */
static void
template_update_real (GtkWidget * menu)
{
  GList *list;
  GtkWidget *menuitem;
  GtkWidget *label;
  struct template_t *t;
  gchar *category;
  gchar *str;
  gpointer func;

  /* Prelevo la funzione da eseguire al click: */
  func = g_object_get_data (G_OBJECT (menu), "func");

  /* Cancello tutto cio' che c'e': */
  gtk_container_foreach (GTK_CONTAINER (menu),
			 (GtkCallback) gtk_widget_destroy, NULL);

  if (!template_list)
    return;

  /* Ordino la lista: */
  template_list = g_list_sort (template_list, (GCompareFunc) template_strcmp);

  /* Creo il menu: */
  menuitem = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menu), menuitem);

  /* Setto il primo nome della categoria: */
  category = ((struct template_t *) template_list->data)->category;

  if (category)
    {
      gchar *s = markup (template_parse_category (category));
      str = g_strdup_printf ("<b>%s</b>", s);
      g_free (s);
    }
  else
    str = g_strdup_printf (_("<b>Generic</b>"));

  label = gtk_label_new (str);
  g_free (str);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_container_add (GTK_CONTAINER (menuitem), label);

  /* Per ogni item della lista aggiungo una voce: */
  list = template_list;
  while (list)
    {
      t = list->data;

      /* Se cambia categoria: */
      if (category
	  && ((!t->category && category) || strcmp (t->category, category)))
	{
	  category = t->category;

	  menuitem = gtk_menu_item_new ();
	  gtk_widget_set_sensitive (menuitem, FALSE);
	  gtk_container_add (GTK_CONTAINER (menu), menuitem);

	  menuitem = gtk_menu_item_new ();
	  gtk_container_add (GTK_CONTAINER (menu), menuitem);

	  if (category)
	    {
	      gchar *s = markup (template_parse_category (category));
	      str = g_strdup_printf ("<b>%s</b>", s);
	      g_free (s);
	    }
	  else
	    str = g_strdup_printf (_("<b>Generic</b>"));

	  label = gtk_label_new (str);
	  g_free (str);

	  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	  gtk_container_add (GTK_CONTAINER (menuitem), label);
	}

      /* Inserisco il template e lo rendo cliccabile: */
      menuitem = gtk_menu_item_new_with_label (t->name);
      gtk_container_add (GTK_CONTAINER (menu), menuitem);
      g_signal_connect ((gpointer) menuitem, "activate", G_CALLBACK (func),
			t);

      list = list->next;
    }

  gtk_widget_show_all (menu);
}

/* IMPORT DEI TEMPLATE ******************************************************/

/* Questa funzione importa templates da file: */
void
template_import_file (GtkWidget * w, gpointer dummy)
{
  gchar *file;
  gchar *uri;
  gchar s[1024];

  /* Statusbar: */
  editor_statusbar_lock = LOCK;
  statusbar_set (_("Import a new template..."));

  /* Richiedo un file: */
  if (!
      (file =
       file_chooser (_("Open a template file..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_OPEN, NULL)))
    {
      statusbar_set (_("Open a template file: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Creo un uri compatibile: */
  uri = g_filename_to_uri (file, NULL, NULL);

  /* Provo ad importarlo: */
  if (uri && template_import (uri, NULL, TRUE, TRUE) == TRUE)
    {
      template_update ();
      template_save ();

      g_snprintf (s, sizeof (s), _("Template %s imported"), file);
      statusbar_set (s);

      g_free (uri);
    }
  else
    {
      statusbar_set (_("Open a template file: error!"));

      if (uri)
	g_free (uri);
    }

  /* Libero la statusbar e la memoria: */
  editor_statusbar_lock = WAIT;
  g_free (file);
}

/* Come sopra ma per gli uri: */
void
template_import_uri (GtkWidget * w, gpointer dummy)
{
  gchar *uri;
  gchar s[1024];
  struct download_t *download;

  editor_statusbar_lock = LOCK;
  statusbar_set (_("Import a new template..."));

  if (!(uri = uri_chooser (_("Import a new template..."), &download)))
    {
      statusbar_set (_("Import a new template: about"));
      editor_statusbar_lock = WAIT;
      return;
    }

  /* Importo: */
  if (template_import (uri, download, TRUE, TRUE) == TRUE)
    {
      template_update ();
      template_save ();

      g_snprintf (s, sizeof (s), _("Template %s imported"), uri);
      statusbar_set (s);
    }

  if (download)
    download_free (download);

  /* Statusbar e memory */
  editor_statusbar_lock = WAIT;
  g_free (uri);
}

/* Funzione utile per ordinare: */
static gint
template_list_sort (struct template_t *t1, struct template_t *t2)
{
  return strcmp (t1->name, t2->name);
}

/* Questa funzione importa un uri: */
gboolean
template_import (gchar * uri, struct download_t * download, gboolean msg,
		 gboolean thread)
{
  GList *list, *nslist;
  GList *l;
  GList *t = NULL, *templates = NULL;
  struct rdf_t *rdf;
  gchar s[1024];
  gboolean ok = TRUE;

  /* Se devo usare i thread parso l'RDF con rdf_parse_thread: */
  if (thread == TRUE)
    {
      if (rdf_parse_thread (uri, download, &list, &nslist, FALSE, NULL, NULL)
	  == FALSE)
	ok = FALSE;
    }

  /* Altrimenti faccio senza thread: */
  else
    {
      if (rdf_parse (uri, download, FALSE, &list, NULL, NULL) == FALSE)
	ok = FALSE;

      else if (namespace_parse (list, &nslist) == FALSE)
	ok = FALSE;
    }

  /* Se ci sono problemi comunico solo se devo: */
  if (ok == FALSE)
    {
      if (msg)
	{
	  g_snprintf (s, sizeof (s), _("Error opening resource '%s'"), uri);
	  dialog_msg (s);
	}

      return FALSE;
    }

  /* Libero memoria: */
  g_list_foreach (nslist, (GFunc) namespace_free, NULL);
  g_list_free (nslist);

  /* Cerco delle valide triplette di possibili template e li salvo dentro
   * ad una lista: */
  l = list;
  while (l)
    {
      rdf = l->data;

      if (!strcmp (rdf->predicate, RDF_TYPE)
	  && rdf->object_type == RDF_OBJECT_RESOURCE
	  && !strcmp (rdf->object, MORLA_NS MORLA_TEMPLATE))
	t = g_list_append (t, rdf->subject);

      l = l->next;
    }

  /* Se ho una lista per ognuni elemento provo ad importare il template: */
  if (t)
    {
      l = t;
      while (l)
	{
	  template_import_new (l->data, &templates, list);
	  l = l->next;
	}

      g_list_free (t);
    }

  /* Libero altra memoria: */
  g_list_foreach (list, (GFunc) rdf_free, NULL);
  g_list_free (list);

  /* Se non ho template importati, comuinico: */
  if (!templates)
    {
      if (msg)
	dialog_msg (_("No template in this RDF Document!"));

      return FALSE;
    }

  /* Per ogni template controllo se ce l'ho gia: */
  l = templates;
  while (l)
    {
      ok = TRUE;

      /* Cerco duplicati: */
      list = template_list;
      while (list)
	{
	  /* Se coincidono: */
	  if (!strcmp
	      (((struct template_t *) l->data)->uri,
	       ((struct template_t *) list->data)->uri))
	    {

	      /* e le versioni sono uguali... */
	      if (!strcmp (((struct template_t *) l->data)->version,
			   ((struct template_t *) list->data)->version))
		ok = FALSE;

	      /* altrimenti rimuovo quello vecchio: */
	      else
		{
		  template_free (list->data);
		  template_list = g_list_remove (template_list, list->data);
		}

	      break;
	    }

	  list = list->next;
	}

      /* Se non ci sono problemi aggiungo: */
      if (ok == TRUE)
	{
	  template_list = g_list_append (template_list, l->data);
	  template_list =
	    g_list_sort (template_list, (GCompareFunc) template_list_sort);
	}

      /* altrimenti comunico e libero la memoria: */
      else
	{
	  if (msg)
	    {
	      g_snprintf (s, sizeof (s), _("Duplicated template '%s'!"),
			  ((struct template_t *) l->data)->name);
	      dialog_msg (s);
	    }

	  template_free (l->data);
	}

      l = l->next;
    }

  /* Libero memoria finale: */
  g_list_free (templates);

  return TRUE;
}

/* Questa funzione cerca di importare un template preciso dato una lista
 * di triplette e un nome: */
static void
template_import_new (gchar * template, GList ** list, GList * rdf)
{
  GList *l, *input = NULL;
  struct rdf_t *r;
  gchar *uri = NULL;
  gchar *name = NULL;
  gchar *version = NULL;
  gchar *category = NULL;
  struct template_t *ret;
  GList *input_ret = NULL;

  l = rdf;

  while (l)
    {
      r = l->data;
      if (!strcmp (r->subject, template))
	{
	  /* Cerco le triplette che hanno alcune caratteriche proprieta': */
	  if (r->object_type == RDF_OBJECT_LITERAL
	      && !strcmp (r->predicate, RDFS_LABEL))
	    {
	      name = r->object;
	      uri = r->subject;
	    }

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_HASVERSION))
	    version = r->object;

	  else if (r->object_type == RDF_OBJECT_RESOURCE
		   && !strcmp (r->predicate, MORLA_NS MORLA_CATEGORY))
	    category = r->object;

	  else if (r->object_type != RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_HASINPUT))
	    input = g_list_append (input, r->object);
	}

      l = l->next;
    }

  /* Ultimi controlli: */
  if (!input)
    {
      g_message (_("No input for template '%s'"), template);
      return;
    }

  if (!uri || !name || !version)
    {
      g_message (_("No URI, name or version for template '%s'"), template);
      g_list_free (input);
      return;
    }

  /* Per ogni input provo ad importarlo: */
  l = input;
  while (l)
    {
      template_parse_input (l->data, &input_ret, rdf);
      l = l->next;
    }

  /* Libero memoria: */
  g_list_free (input);

  /* se non ho importato nessun input: */
  if (!input_ret)
    {
      g_message (_("No input for template '%s'"), template);
      return;
    }

  /* Alloco memoria ed inserisco nella lista: */
  ret = (struct template_t *) g_malloc0 (sizeof (struct template_t));

  ret->name = g_strdup (name);
  ret->uri = g_strdup (uri);
  ret->version = g_strdup (version);
  ret->category = category ? g_strdup (category) : NULL;
  ret->input = input_ret;

  (*list) = g_list_append (*list, ret);
}

/* Funzione per l'import di input di template: */
static void
template_parse_input (gchar * input, GList ** list, GList * rdf)
{
  GList *l;
  struct rdf_t *r;
  GList *labels = NULL;
  GList *comments = NULL;
  gint min_cardinality = 0;
  gint max_cardinality = 1;
  gchar *typevalue = NULL;
  GList *alt_typevalue = NULL;
  gchar *default_value = NULL;
  gchar *default_language = NULL;
  gchar *default_datatype = NULL;
  gchar *function_init = NULL;
  gchar *function_check = NULL;
  gchar *function_destroy = NULL;
  gchar *function_save = NULL;
  gboolean visible = 1;
  gboolean lang = 1;
  gboolean datatype = 1;
  gboolean longtext = 0;
  gchar *rdfs = NULL;
  struct template_input_t *ret;
  struct info_box_t *lang_input;
  gboolean ok = 0;

  l = rdf;

  /* Per ogni tripletta cerco alcune property particolari: */
  while (l)
    {
      r = l->data;

      if (!strcmp (r->subject, input))
	{
	  if (r->object_type == RDF_OBJECT_LITERAL
	      && !strcmp (r->predicate, RDFS_LABEL))
	    {
	      lang_input = g_malloc (sizeof (struct info_box_t));
	      lang_input->value = g_strdup (r->object);
	      lang_input->lang = r->lang ? g_strdup (r->lang) : NULL;

	      labels = g_list_append (labels, lang_input);
	    }

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, RDFS_COMMENT))
	    {
	      lang_input = g_malloc (sizeof (struct info_box_t));
	      lang_input->value = g_strdup (r->object);
	      lang_input->lang = r->lang ? g_strdup (r->lang) : NULL;

	      comments = g_list_append (comments, lang_input);
	    }

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_MINCARD))
	    min_cardinality = atoi (r->object);

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_MAXCARD))
	    max_cardinality = atoi (r->object);

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_VISIBLE))
	    visible = !g_ascii_strcasecmp (r->object, "yes");

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_LANGUAGE))
	    lang = !g_ascii_strcasecmp (r->object, "yes");

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_DATATYPE))
	    datatype = !g_ascii_strcasecmp (r->object, "yes");

	  else if (!strcmp (r->predicate, MORLA_NS MORLA_TYPEVALUE))
	    {
	      switch (r->object_type)
		{
		case RDF_OBJECT_RESOURCE:
		  typevalue = r->object;
		  break;

		case RDF_OBJECT_BLANK:
		  typevalue =
		    template_parse_input_typevalue (r->object, rdf,
						    &alt_typevalue);
		  break;

		default:
		  break;
		}
	    }

	  else if (!strcmp (r->predicate, MORLA_NS MORLA_DEFAULTVALUE))
	    default_value = r->object;

	  else if (r->object_type == RDF_OBJECT_RESOURCE
		   && !strcmp (r->predicate, MORLA_NS MORLA_DEFAULTDATATYPE))
	    default_datatype = r->object;

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_DEFAULTLANGUAGE))
	    default_language = r->object;

	  else if (r->object_type == RDF_OBJECT_RESOURCE
		   && !strcmp (r->predicate, MORLA_NS MORLA_HASRDFS))
	    rdfs = r->object;

	  else if (r->object_type == RDF_OBJECT_RESOURCE
		   && !strcmp (r->predicate, RDF_TYPE)
		   && !strcmp (r->object, MORLA_NS MORLA_INPUT))
	    ok = 1;

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_FUNCTIONINIT))
	    function_init = r->object;

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_FUNCTIONCHECK))
	    function_check = r->object;

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_FUNCTIONDESTROY))
	    function_destroy = r->object;

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_FUNCTIONSAVE))
	    function_save = r->object;

	  else if (r->object_type == RDF_OBJECT_LITERAL
		   && !strcmp (r->predicate, MORLA_NS MORLA_LONGTEXT))
	    longtext = !g_ascii_strcasecmp (r->object, "yes");

	}

      l = l->next;
    }

  /* Ultimi check: */
  if (!ok || !rdfs || !labels)
    {
      if (labels)
	{
	  g_list_foreach (labels, (GFunc) info_box_free, NULL);
	  g_list_free (labels);
	}

      if (comments)
	{
	  g_list_foreach (comments, (GFunc) info_box_free, NULL);
	  g_list_free (comments);
	}

      g_message (_("Input '%s' has no rdfs, label or istance of input!"),
		 input);
      return;
    }

  /* Controllo se ci sono problemi nella visibilita': */
  if (!visible
      && (!default_value
	  || (strcmp (typevalue, RDFS_LITERAL)
	      && strcmp (typevalue, RDFS_RESOURCE))))
    {
      g_list_foreach (labels, (GFunc) info_box_free, NULL);
      g_list_free (labels);

      if (comments)
	{
	  g_list_foreach (comments, (GFunc) info_box_free, NULL);
	  g_list_free (comments);
	}

      g_message (_
		 ("This input '%s' is novisible but has not a defualt value!"),
		 input);
      return;
    }

  /* Controllo se ci sono problemi nella cardinalita' degli invisibili visibilita': */
  if (!visible && (!max_cardinality > 0 || min_cardinality != 1))
    {
      g_list_foreach (labels, (GFunc) info_box_free, NULL);
      g_list_free (labels);

      if (comments)
	{
	  g_list_foreach (comments, (GFunc) info_box_free, NULL);
	  g_list_free (comments);
	}

      g_message (_
		 ("This input '%s' is novisible but has max_cardinality or min_cardinality != 1!"),
		 input);
      return;
    }

  /* Alloco e inserisco nella lista: */
  ret =
    (struct template_input_t *) g_malloc0 (sizeof (struct template_input_t));

  ret->labels = labels;
  ret->comments = comments;

  /* Controllo il typevalue: */
  if (typevalue)
    {
      if (!strcmp (typevalue, RDFS_LITERAL))
	{
	  if (alt_typevalue)
	    {
	      g_message
		(_
		 ("This input '%s' has a alternative list of value but it is not a alternative typevalue!"),
		 input);
	      template_input_free (ret);
	      return;
	    }

	  ret->typevalue = TEMPLATE_INPUT_TYPEVALUE_LITERAL;
	  ret->datatype = datatype;
	  ret->lang = lang;
	}

      else if (!strcmp (typevalue, RDFS_RESOURCE))
	{
	  if (alt_typevalue)
	    {
	      g_message
		(_
		 ("This input '%s' has a alternative list of value but it is not a alternative typevalue!"),
		 input);
	      template_input_free (ret);
	      return;
	    }

	  ret->typevalue = TEMPLATE_INPUT_TYPEVALUE_RESOURCE;
	}

      else if (!strcmp (typevalue, MORLA_NS MORLA_ALT_LITERAL))
	{
	  if (!alt_typevalue)
	    {
	      g_message
		(_("This input '%s' has no a alternative list of value!"),
		 input);
	      template_input_free (ret);
	      return;
	    }

	  ret->typevalue = TEMPLATE_INPUT_TYPEVALUE_ALT_LITERAL;
	  ret->alt_typevalue = alt_typevalue;
	}

      else if (!strcmp (typevalue, MORLA_NS MORLA_ALT_RESOURCE))
	{
	  if (!alt_typevalue)
	    {
	      g_message
		(_("This input '%s' has no a alternative list of value!"),
		 input);
	      template_input_free (ret);
	      return;
	    }

	  ret->typevalue = TEMPLATE_INPUT_TYPEVALUE_ALT_RESOURCE;
	  ret->alt_typevalue = alt_typevalue;
	}

      else
	{
	  if (alt_typevalue)
	    {
	      g_message
		(_
		 ("This input '%s' has a alternative list of value but it is not a alternative typevalue!"),
		 input);
	      template_input_free (ret);
	      return;
	    }

	  ret->other_typevalue = g_strdup (typevalue);
	  ret->typevalue = TEMPLATE_INPUT_TYPEVALUE_OTHER;
	}
    }
  else
    ret->typevalue = TEMPLATE_INPUT_TYPEVALUE_NONE;

  /* Resto dei valori: */
  ret->rdfs = g_strdup (rdfs);

  if (default_language)
    ret->default_language = g_strdup (default_language);

  if (default_datatype)
    {
      if (datatype_exists (default_datatype) == TRUE)
	ret->default_datatype = g_strdup (default_datatype);

      else
	{
	  g_message (_
		     ("The input '%s' tries to use a impossibile datatype '%s'."),
		     input, default_datatype);
	  template_input_free (ret);
	  return;
	}
    }

  if (default_value)
    ret->default_value = g_strdup (default_value);

  if (min_cardinality > 0)
    ret->min_cardinality = min_cardinality;

  if (max_cardinality > 0 && max_cardinality < min_cardinality)
    max_cardinality = min_cardinality;

  if (function_init)
    ret->function_init = g_strdup (function_init);

  if (function_check)
    ret->function_check = g_strdup (function_check);

  if (function_destroy)
    ret->function_destroy = g_strdup (function_destroy);

  if (function_save)
    ret->function_save = g_strdup (function_save);

  ret->longtext = longtext;

  ret->max_cardinality = max_cardinality;

  ret->visible = visible;

  (*list) = g_list_append (*list, ret);
}

/* Questa funzione crea la lista di alternative */
static gchar *
template_parse_input_typevalue (gchar * node, GList * rdf, GList ** alt)
{
  struct rdf_t *data;
  gchar *ret = NULL;
  GList *list = NULL;

  while (rdf)
    {
      data = rdf->data;

      if (!strcmp (data->subject, node))
	{
	  if (!strcmp (data->predicate, RDF_TYPE)
	      && data->object_type == RDF_OBJECT_RESOURCE)
	    ret = data->object;

	  else if (!strncmp (data->predicate, RDF_ITEM, strlen (RDF_ITEM)))
	    {
	      list = g_list_append (list, g_strdup (data->object));
	    }

	}

      rdf = rdf->next;
    }

  *alt = list;
  return ret;
}

/* SALVATAGGIO **************************************************************/

static void
template_save_init (FILE * fl)
{
  fprintf (fl, "<?xml version=\"1.0\"?>\n");
  fprintf (fl, "<rdf:RDF\n");
  fprintf (fl,
	   "   xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n");
  fprintf (fl, "   xmlns:rdfs=\"http://www.w3.org/2000/01/rdf-schema#\"\n");
  fprintf (fl, "   xmlns:morla=\"" MORLA_NS "\"\n");
  fprintf (fl, ">\n\n");
}

static void
template_save_one (FILE * fl, struct template_t *template, gint t_id)
{
  struct template_input_t *input;
  struct info_box_t *lang_input;
  gchar *str;
  GList *ll, *list;
  gint i, id;

  str = markup (template->uri);
  fprintf (fl, "  <rdf:Description rdf:about=\"%s\">\n", str);
  g_free (str);

  fprintf (fl, "    <rdf:type rdf:resource=\"%s\" />\n",
	   MORLA_NS MORLA_TEMPLATE);

  str = markup (template->name);
  fprintf (fl, "    <rdfs:label>%s</rdfs:label>\n", str);
  g_free (str);

  str = markup (template->version);
  fprintf (fl,
	   "    <morla:" MORLA_HASVERSION ">%s</morla:" MORLA_HASVERSION
	   ">\n", str);
  g_free (str);

  if (template->category)
    {
      str = markup (template->category);
      fprintf (fl, "    <morla:" MORLA_CATEGORY " rdf:resource=\"%s\" />\n",
	       str);
      g_free (str);
    }

  for (i = 0, id = g_list_length (template->input); i < id; i++)
    fprintf (fl,
	     "    <morla:" MORLA_HASINPUT " rdf:nodeID=\"input_%d_%d\" />\n",
	     t_id, i);
  fprintf (fl, "  </rdf:Description>\n\n");

  i = 0;

  /* Per ogni input scrivo: */
  ll = template->input;
  while (ll)
    {
      input = ll->data;

      if (input->typevalue == TEMPLATE_INPUT_TYPEVALUE_ALT_RESOURCE)
	{
	  GList *list;
	  gint id = 0;

	  fprintf (fl,
		   "  <rdf:Description rdf:nodeID=\"input_%d_%d_typevalue\">\n",
		   t_id, i);
	  fprintf (fl, "    <rdf:type rdf:resource=\"%s\" />\n",
		   MORLA_NS MORLA_ALT_RESOURCE);

	  list = input->alt_typevalue;
	  while (list)
	    {
	      fprintf (fl, "    <rdf:_%d rdf:resource=\"%s\" />\n", ++id,
		       (gchar *) list->data);
	      list = list->next;
	    }

	  fprintf (fl, "  </rdf:Description>\n\n");
	}

      if (input->typevalue == TEMPLATE_INPUT_TYPEVALUE_ALT_LITERAL)
	{
	  GList *list;
	  gint id = 1;

	  fprintf (fl,
		   "  <rdf:Description rdf:nodeID=\"input_%d_%d_typevalue\">\n",
		   t_id, i);
	  fprintf (fl, "    <rdf:type rdf:resource=\"%s\" />\n",
		   MORLA_NS MORLA_ALT_LITERAL);

	  list = input->alt_typevalue;
	  while (list)
	    {
	      fprintf (fl, "    <rdf:_%d>%s</rdf:_%d>\n", id,
		       (gchar *) list->data, id);
	      id++;
	      list = list->next;
	    }

	  fprintf (fl, "  </rdf:Description>\n\n");
	}

      fprintf (fl, "  <rdf:Description rdf:nodeID=\"input_%d_%d\">\n", t_id,
	       i);
      fprintf (fl, "    <rdf:type rdf:resource=\"%s\" />\n",
	       MORLA_NS MORLA_INPUT);

      list = input->labels;
      while (list)
	{
	  lang_input = list->data;

	  fprintf (fl, "    <rdfs:label");

	  if (lang_input->lang)
	    {
	      str = markup (lang_input->lang);
	      fprintf (fl, " xml:lang=\"%s\"", str);
	      g_free (str);
	    }

	  str = markup (lang_input->value);
	  fprintf (fl, ">%s</rdfs:label>\n", str);
	  g_free (str);

	  list = list->next;
	}

      str = markup (input->rdfs);
      fprintf (fl, "    <morla:" MORLA_HASRDFS " rdf:resource=\"%s\" />\n",
	       str);
      g_free (str);

      fprintf (fl,
	       "    <morla:" MORLA_MINCARD ">%d</morla:" MORLA_MINCARD ">\n",
	       input->min_cardinality);
      fprintf (fl,
	       "    <morla:" MORLA_MAXCARD ">%d</morla:" MORLA_MAXCARD ">\n",
	       input->max_cardinality);

      fprintf (fl,
	       "    <morla:" MORLA_VISIBLE ">%s</morla:" MORLA_VISIBLE ">\n",
	       input->visible ? "yes" : "no");

      if (input->function_init)
	fprintf (fl,
		 "    <morla:" MORLA_FUNCTIONINIT ">%s</morla:"
		 MORLA_FUNCTIONINIT ">\n", input->function_init);

      if (input->function_check)
	fprintf (fl,
		 "    <morla:" MORLA_FUNCTIONCHECK ">%s</morla:"
		 MORLA_FUNCTIONCHECK ">\n", input->function_check);

      if (input->function_destroy)
	fprintf (fl,
		 "    <morla:" MORLA_FUNCTIONDESTROY ">%s</morla:"
		 MORLA_FUNCTIONDESTROY ">\n", input->function_destroy);

      if (input->function_save)
	fprintf (fl,
		 "    <morla:" MORLA_FUNCTIONSAVE ">%s</morla:"
		 MORLA_FUNCTIONSAVE ">\n", input->function_save);

      fprintf (fl,
	       "    <morla:" MORLA_LONGTEXT ">%s</morla:" MORLA_LONGTEXT
	       ">\n", input->longtext ? "yes" : "no");

      switch (input->typevalue)
	{
	case TEMPLATE_INPUT_TYPEVALUE_NONE:
	  break;

	case TEMPLATE_INPUT_TYPEVALUE_RESOURCE:
	  fprintf (fl,
		   "    <morla:" MORLA_TYPEVALUE " rdf:resource=\"%s\" />\n",
		   RDFS_RESOURCE);
	  break;

	case TEMPLATE_INPUT_TYPEVALUE_LITERAL:
	  fprintf (fl,
		   "    <morla:" MORLA_TYPEVALUE " rdf:resource=\"%s\" />\n",
		   RDFS_LITERAL);
	  fprintf (fl,
		   "    <morla:" MORLA_LANGUAGE ">%s</morla:" MORLA_LANGUAGE
		   ">\n", input->lang ? "yes" : "no");
	  fprintf (fl,
		   "    <morla:" MORLA_DATATYPE ">%s</morla:" MORLA_DATATYPE
		   ">\n", input->datatype ? "yes" : "no");
	  break;

	case TEMPLATE_INPUT_TYPEVALUE_ALT_RESOURCE:
	case TEMPLATE_INPUT_TYPEVALUE_ALT_LITERAL:
	  fprintf (fl,
		   "    <morla:" MORLA_TYPEVALUE
		   " rdf:nodeID=\"input_%d_%d_typevalue\" />\n", t_id, i);
	  break;

	case TEMPLATE_INPUT_TYPEVALUE_OTHER:
	  fprintf (fl,
		   "    <morla:" MORLA_TYPEVALUE " rdf:resource=\"%s\" />\n",
		   input->other_typevalue);
	  break;
	}

      if (input->default_value)
	{
	  str = markup (input->default_value);
	  fprintf (fl,
		   "    <morla:" MORLA_DEFAULTVALUE ">%s</morla:"
		   MORLA_DEFAULTVALUE ">\n", str);
	  g_free (str);
	}

      if (input->default_language)
	{
	  str = markup (input->default_language);
	  fprintf (fl,
		   "    <morla:" MORLA_DEFAULTLANGUAGE ">%s</morla:"
		   MORLA_DEFAULTLANGUAGE ">\n", str);
	  g_free (str);
	}

      if (input->default_datatype)
	{
	  str = markup (input->default_datatype);
	  fprintf (fl,
		   "    <morla:" MORLA_DEFAULTDATATYPE
		   " rdf:resource=\"%s\" />\n", str);
	  g_free (str);
	}

      list = input->comments;
      while (list)
	{
	  lang_input = list->data;

	  fprintf (fl, "    <rdfs:comment");

	  if (lang_input->lang)
	    {
	      str = markup (lang_input->lang);
	      fprintf (fl, " xml:lang=\"%s\"", str);
	      g_free (str);
	    }

	  str = markup (lang_input->value);
	  fprintf (fl, ">%s</rdfs:comment>\n", str);
	  g_free (str);

	  list = list->next;
	}

      fprintf (fl, "  </rdf:Description>\n\n");

      i++;
      ll = ll->next;
    }
}

static void
template_save_close (FILE * fl)
{
  fprintf (fl, "</rdf:RDF>\n\n");
}

/* Questa funzione salva i template dentro ad un file: */
void
template_save (void)
{
  gchar *path;
  FILE *fl;
  GList *list;
  gint t_id;

  path =
    g_build_path (G_DIR_SEPARATOR_S, g_get_user_config_dir (), PACKAGE,
		  "template.rdf", NULL);

  if (!template_list)
    {
      if (g_file_test (path, G_FILE_TEST_EXISTS) == TRUE && g_remove (path))
	dialog_msg (_("Error saving the template file!"));
      g_free (path);
      return;
    }

  if (!(fl = g_fopen (path, "wb")))
    {
      dialog_msg (_("Error saving the template file!"));
      g_free (path);
      return;
    }

  g_free (path);

  t_id = 0;
  template_save_init (fl);

  /* Per ogni template scrivo: */
  for (list = template_list; list; list = list->next)
    {
      template_save_one (fl, list->data, t_id);
      t_id++;
    }

  template_save_close (fl);
  fclose (fl);
}

/* Questa funzione salva un template dentro ad un file: */
void
template_save_single (GtkWidget * w, struct template_t *template)
{
  gchar *path;
  FILE *fl;

  editor_statusbar_lock = LOCK;
  statusbar_set (_("Export a template..."));

  /* Richiedo un file: */
  if (!
      (path =
       file_chooser (_("Export a template..."), GTK_SELECTION_SINGLE,
		     GTK_FILE_CHOOSER_ACTION_SAVE, NULL)))
    {
      statusbar_set (_("Export a template: aborted"));
      editor_statusbar_lock = WAIT;
      return;
    }

  if (!(fl = g_fopen (path, "wb")))
    {
      dialog_msg (_("Error saving the template into a file!"));
      statusbar_set (_("Export a template: error."));
      editor_statusbar_lock = WAIT;
      g_free (path);
      return;
    }

  g_free (path);

  template_save_init (fl);
  template_save_one (fl, template, 0);
  template_save_close (fl);
  fclose (fl);

  statusbar_set (_("Export a template: done."));
  editor_statusbar_lock = WAIT;
}

/* DISTRUZIONE ***************************************************************/

/* Questa funzione libera la memoria di un input */
static void
template_input_free (struct template_input_t *i)
{
  if (!i)
    return;

  if (i->labels)
    {
      g_list_foreach (i->labels, (GFunc) info_box_free, NULL);
      g_list_free (i->labels);
    }

  if (i->rdfs)
    g_free (i->rdfs);

  if (i->alt_typevalue)
    {
      g_list_foreach (i->alt_typevalue, (GFunc) g_free, NULL);
      g_list_free (i->alt_typevalue);
    }

  if (i->other_typevalue)
    g_free (i->other_typevalue);

  if (i->comments)
    {
      g_list_foreach (i->comments, (GFunc) info_box_free, NULL);
      g_list_free (i->comments);
    }

  if (i->default_value)
    g_free (i->default_value);

  if (i->default_language)
    g_free (i->default_language);

  if (i->default_datatype)
    g_free (i->default_datatype);

  if (i->function_init)
    g_free (i->function_init);

  if (i->function_check)
    g_free (i->function_check);

  if (i->function_destroy)
    g_free (i->function_destroy);

  if (i->function_save)
    g_free (i->function_save);

  g_free (i);
}

/* Questa funzione libera la memoria di un template: */
static void
template_free (struct template_t *t)
{
  if (!t)
    return;

  if (t->uri)
    g_free (t->uri);

  if (t->name)
    g_free (t->name);

  if (t->version)
    g_free (t->version);

  if (t->category)
    g_free (t->category);

  if (t->input)
    {
      g_list_foreach (t->input, (GFunc) template_input_free, NULL);
      g_list_free (t->input);
    }

  g_free (t);
}

/* FINESTRA DEI TEMPLATE *****************************************************/

/* Questa funzione mostra i template importati in una interfaccia a lista: */
void
template (GtkWidget * w, gpointer dummy)
{
  GtkWidget *window, *hbox, *scrolledwindow, *treeview, *button, *frame;
  GtkListStore *model;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  gchar s[1024];
  gint ret;

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Templates import"));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 400, 200);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, TRUE, TRUE,
		      0);

  frame = gtk_frame_new (_("Templates import"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model =
    gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);
  g_signal_connect ((gpointer) treeview, "row_activated",
		    G_CALLBACK (template_activate), window);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Template"), renderer, "text",
					      0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Version"), renderer, "text",
					      1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Category"), renderer, "text",
					      2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("URI"), renderer, "text",
					      3, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  button = gtk_button_new_from_stock ("gtk-add");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-remove");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button,
				GTK_RESPONSE_CANCEL);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-close");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_NO);
  gtk_widget_set_can_default(button, TRUE);

  /* Refresh della finestra: */
  template_refresh (treeview);
  gtk_widget_show_all (window);

  g_object_unref (model);

  while (1)
    {
      ret = gtk_dialog_run (GTK_DIALOG (window));

      if (ret == GTK_RESPONSE_OK)
	{
	  if (!gtk_tree_selection_count_selected_rows (selection))
	    {
	      dialog_msg (_("Select a template!"));
	      continue;
	    }

	  template_open_dialog (treeview);
	  gtk_widget_hide (window);
	  break;
	}

      else if (ret == GTK_RESPONSE_CANCEL)
	{
	  if (!gtk_tree_selection_count_selected_rows (selection))
	    {
	      dialog_msg (_("Select a template!"));
	      continue;
	    }

	  if (dialog_ask (_("Sure to remove this template?")) ==
	      GTK_RESPONSE_OK && template_remove (treeview))
	    {
	      template_update ();
	      template_save ();
	      template_refresh (treeview);
	    }
	}

      else
	break;
    }

  gtk_widget_destroy (window);
}

/* Questa funzione refreshia l'intefaccia di sopra: */
static void
template_refresh (GtkWidget * treeview)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *list;
  struct template_t *template;

  /* Rimuovo tutto: */
  store = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  while (gtk_tree_model_iter_nth_child
	 (GTK_TREE_MODEL (store), &iter, NULL, 0))
    gtk_list_store_remove (store, &iter);

  /* Appendo: */
  list = template_list;
  while (list)
    {
      template = (struct template_t *) list->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, template->name, 1,
			  template->version, 2, template->category, 3,
			  template->uri, -1);

      list = list->next;
    }
}

/* Questa funzione rimuove gli input selezionati: */
static gint
template_remove (GtkWidget * treeview)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;
      GList *list = template_list;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];

      while (list)
	{
	  if (!i)
	    {
	      template_free (list->data);
	      template_list = g_list_remove (template_list, list->data);
	      break;
	    }
	  i--;
	  list = list->next;
	}

      gtk_tree_path_free (path);
      return 1;
    }

  return 0;
}

/* Questa funzione attiva l'uso di un template selezionato nell'interfaccia: */
static void
template_open_dialog (GtkWidget * treeview)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkTreePath *path;
      GList *list = template_list;

      path = gtk_tree_model_get_path (model, &iter);
      i = gtk_tree_path_get_indices (path)[0];

      /* Uso la funzione template_open: */
      if ((list = g_list_nth (template_list, i)))
	template_open (NULL, list->data);

      gtk_tree_path_free (path);
    }
}

/* Questa funzione simula il click del pulsante alla selezione di una riga
 * dell'interfaccia: */
static void
template_activate (GtkWidget * w, GtkTreePath * p, GtkTreeViewColumn * c,
		   GtkWidget * dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

/* REMOTE TEMPLATE ***********************************************************/

static GList *template_remote_list = NULL;

/* Questa funzione distrugge la lista dei template remoti gia' richiesti: */
static void
template_remote_init (void)
{
  if (template_remote_list)
    {
      g_list_free (template_remote_list);
      template_remote_list = NULL;
    }
}

/* Questa funzione controlla se ci sono elementi gia' richiamati: */
static void
template_remote_parse (GList ** remote)
{
  GList *l = template_remote_list;

  while (l)
    {
      GList *list = *remote;

      while (list)
	{
	  if (!strcmp (list->data, l->data))
	    {
	      *remote = g_list_remove (*remote, l->data);
	      break;
	    }

	  list = list->next;
	}

      l = l->next;
    }
}

/* Questa funzione aggiunge tutti quei templete che si e' richiesto di 
 * aggiungere: */
static void
template_remote_add (GList * add)
{
  while (add)
    {
      template_remote_list = g_list_append (template_remote_list, add->data);
      add = add->next;
    }
}

/* Questa funzione mostra la finestra per richeidere l'import di template remotati */
static gint
template_dialog_remote (GList * remote)
{
  GString *str;
  GList *list;
  gchar *buf;
  gint ret;

  template_remote_add (remote);

  str =
    g_string_new (_
		  ("This template requires other templates. Do you want import them?\n"));

  for (list = remote; list; list = list->next)
    g_string_append_printf (str, "%s\n", (gchar *) list->data);

  buf = g_string_free (str, FALSE);
  ret = dialog_ask_with_cancel (buf);
  g_free (buf);

  return ret;
}

/* CREATE TEMPLATE ***********************************************************/

/* Questa funzione crea l'interfaccia per l'uso di un template: */
static GtkWidget *
template_open_real (GList * resources, GList * blanks, struct template_t *t)
{
  GList *list;
  GList *remote;
  GList *real;
  GtkWidget *box;
  GtkWidget *widget;
  gint i;

#ifdef USE_JS
  struct js_t *js;
#endif

  /* Controllo se qualche input richiede template remotati: */
  list = t->input;
  remote = NULL;
  while (list)
    {
      struct template_input_t *input = list->data;

      if (input->other_typevalue)
	remote = g_list_append (remote, input->other_typevalue);

      list = list->next;
    }

  if (remote)
    {
      GList *l;

      /* Rimuovo eventuali duplicati: */
      remote = create_no_dup (remote);

      /* Rimuovo quelli che ho gia': */
      list = template_list;
      while (list)
	{
	  struct template_t *t = list->data;

	  l = remote;
	  while (l)
	    {
	      if (!strcmp (t->uri, l->data))
		{
		  remote = g_list_remove (remote, l->data);
		  break;
		}

	      l = l->next;
	    }

	  list = list->next;
	}
    }

  /* Controllo se li ho gia' chiesti: */
  if (remote)
    template_remote_parse (&remote);

  if (remote)
    {
      /* Rimuovo eventuali duplicati: */
      remote = create_no_dup (remote);

      switch (template_dialog_remote (remote))
	{
	  /* Importare i template remotati: */
	case GTK_RESPONSE_OK:
	  for (list = remote; list; list = list->next)
	    template_import ((gchar *) list->data, NULL, TRUE, TRUE);

	  template_update ();
	  template_save ();
	  break;

	  /* Continuo: */
	case GTK_RESPONSE_NO:
	  break;

	  /* Esco: */
	default:
	  g_list_free (remote);
	  return NULL;
	}

      g_list_free (remote);
    }

#ifdef USE_JS
  /* JS Context: */
  js = js_new ();
#endif

  box = gtk_table_new (0, 0, FALSE);

  /* Tolgo tra tutti gli input quelli non visibili: */
  list = t->input;
  real = NULL;
  while (list)
    {
      if (((struct template_input_t *) list->data)->visible)
	real = g_list_append (real, list->data);

      list = list->next;
    }

  /* Per ogni input visibile creo l'oggetto grafico: */
  list = real;
  i = 0;

  while (list)
    {
      widget =
	template_widget_create (resources, blanks,
				(struct template_input_t *) list->data, TRUE
#ifdef USE_JS
				, js
#endif
	);
      gtk_table_attach (GTK_TABLE (box), widget, 0, 1, i, i + 1,
			GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

      /* 2 per riga: */
      if (list->next)
	{
	  list = list->next;

	  widget =
	    template_widget_create (resources, blanks,
				    (struct template_input_t *) list->data,
				    TRUE
#ifdef USE_JS
				    , js
#endif
	    );
	  gtk_table_attach (GTK_TABLE (box), widget, 1, 2, i, i + 1,
			    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);
	}

      i++;
      list = list->next;
    }

#ifdef USE_JS
  g_object_set_data (G_OBJECT (box), "js", js);
  g_signal_connect ((gpointer) box, "destroy", G_CALLBACK (template_destroy),
		    NULL);
#endif

  /* Libero memoria: */
  g_list_free (real);
  return box;
}

/* Questa funzione crea l'interfaccia per l'uso di un template: */
void
template_open (GtkWidget * w, struct template_t *t)
{
  GtkWidget *dialog;
  GtkWidget *vbox, *hbox, *gbox;
  GtkWidget *image;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *subject;
  GtkWidget *sw;
  GtkWidget *button_subject;
  GtkWidget *button_subject_this;
  GtkWidget *widget_inputs;
  GList *list;
  GList *resources;
  GList *blanks;
  GSList *l;
  struct editor_data_t *data = editor_get_data ();
  gchar s[1024];

  if (!data)
    return;

  /* Annullo i template remotati richiesti precedentemente: */
  template_remote_init ();

  resources = create_resources (data);
  resources = create_resources_by_rdfs (resources);
  blanks = create_blank (data);

  if (!(widget_inputs = template_open_real (resources, blanks, t)))
    {
      g_list_free (resources);
      g_list_free (blanks);
      return;
    }

  dialog = gtk_dialog_new ();

  g_snprintf (s, sizeof (s), "%s %s - %s: %s %s", PACKAGE, VERSION,
	      _("Template"), t->name, t->version);
  gtk_window_set_title (GTK_WINDOW (dialog), s);

  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

  label = gtk_label_new (_("<b>Subject</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, FALSE,
		      FALSE, 5);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  gbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gbox, FALSE, FALSE, 0);

  l = NULL;
  button = gtk_radio_button_new_with_label (NULL, _("Resource: "));
  button_subject = button;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_box_pack_start (GTK_BOX (gbox), button, FALSE, FALSE, 3);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (gbox), hbox, TRUE, TRUE, 5);

  /* Creazione soggetto con freccia: */
  subject = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), subject, TRUE, TRUE, 0);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "list", resources);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), subject);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  gbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gbox, FALSE, FALSE, 0);

  button = gtk_radio_button_new_with_label (NULL, _("This document"));
  button_subject_this = button;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_box_pack_start (GTK_BOX (gbox), button, TRUE, TRUE, 3);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), sw, TRUE, TRUE, 5);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
					 widget_inputs);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CLOSE);

  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  template_set_size (dialog, sw, widget_inputs);
  gtk_widget_show_all (dialog);

  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      GList *old;
      gchar *text;
      struct unredo_t *unredo;

      /* Check del soggetto: */
      if (gtk_toggle_button_get_active
	  (GTK_TOGGLE_BUTTON (button_subject_this)) == TRUE)
	text = THIS_DOCUMENT;
      else
	text = (gchar *) gtk_entry_get_text (GTK_ENTRY (subject));

      if (!text || !*text)
	{
	  dialog_msg (_("Invalid subject!"));
	  continue;
	}

      text = g_strdup (text);

      /* Controllo TUTTI i widget di input: */
      list = old = gtk_container_get_children (GTK_CONTAINER (widget_inputs));

      while (list)
	{
	  if (template_widget_check (list->data))
	    break;

	  list = list->next;
	}

      /* Si lamenta la funzione widget_check, quindi io devo solo
       * continuare: */
      if (list)
	{
	  g_free (text);
	  g_list_free (old);
	  continue;
	}

      /* A questo punto inserisco, quindi undo: */
      unredo = unredo_new (data, UNREDO_TYPE_ADD);

      list = old;
      while (list)
	{
	  /* Funzione che fa l'inserimento: */
	  template_widget_save (list->data, data, text, unredo);
	  list = list->next;
	}

      g_list_foreach (old, (GFunc) template_widget_destroy, NULL);
      g_list_free (old);

      /* Poi tutti gli oggetti invisibili: */
      list = t->input;
      while (list)
	{
	  if (!((struct template_input_t *) list->data)->visible)
	    template_invisible_save (text, list->data, data, unredo, NULL);

	  list = list->next;
	}

      g_free (text);
      data->changed = TRUE;

      /* Refreshio i namespace e li rigenero */
      g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
      g_list_free (data->namespace);

      namespace_parse (data->rdf, &data->namespace);

      editor_refresh (data);
      break;
    }

  /* Libero memoria: */
  g_list_free (resources);
  g_list_free (blanks);

  gtk_widget_destroy (dialog);
}

/* Setto la dimensione della finestra di template */
static void
template_set_size (GtkWidget * dialog, GtkWidget * sw, GtkWidget * w)
{
  gint h;

  /* Chiedo alle gtk di realizzare l'oggetto: */
  gtk_widget_show_all (dialog);

  if (w->allocation.height > 1
      && w->allocation.height <= gdk_screen_height () * 2 / 3)
    h = w->allocation.height + 200;
  else
    h = gdk_screen_height () * 2 / 3;

  gdk_window_move_resize (dialog->window,
			  gdk_screen_width () / 2 -
			  dialog->allocation.width / 2,
			  gdk_screen_height () / 2 - h / 2,
			  dialog->allocation.width, h);
}

/* Questa funzione rimuove un elemento dalla liste multiple di input: */
static void
template_widget_list_remove (GtkWidget * widget, GtkTreeView * treeview)
{
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
  GtkWidget *add = g_object_get_data (G_OBJECT (widget), "add");

  /* Se c'e' qualcosa selezionato: */
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gint i;
      GtkWidget *w;
      struct template_input_t *input;

      gtk_tree_model_get (model, &iter, 1, &w, -1);

      if (w)
	{
	  w = gtk_widget_get_toplevel (w);
	  gtk_widget_destroy (w);
	}

      /* Rimuovo l'elemento: */
      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      /* Guardo quali pulsanti devo disattivare o attivare: */
      i = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL);

      input = g_object_get_data (G_OBJECT (widget), "input");
      if (input->max_cardinality > 0 && i < input->max_cardinality)
	gtk_widget_set_sensitive (add, TRUE);

      if (!i)
	gtk_widget_set_sensitive (widget, FALSE);
    }
}

/* Questa funzione mostra un elemento gia' presente nella lista a cardinalita': */
static void
template_widget_list_select (GtkWidget * widget, GtkTreePath * path,
			     GtkTreeViewColumn * c, gpointer dummy)
{
  GtkTreeSelection *selection =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  GtkTreeIter iter;
  GtkWidget *dialog;
  GtkWidget *entry;
  GtkWidget *w;

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      dialog_msg (_("Select a input!"));
      return;
    }

  gtk_tree_model_get (model, &iter, 1, &w, -1);

  entry = g_object_get_data (G_OBJECT (w), "entry");

  /* Se e' un pulsante, simulo il click: */
  if (GTK_IS_BUTTON (entry))
    {
      gchar *name;
      struct template_t *template =
	g_object_get_data (G_OBJECT (entry), "template");

      template_run (entry, template);

      name = template_widget_name (w);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, -1);
      g_free (name);
      return;
    }

  template_widget_save_on (w);

  dialog = gtk_widget_get_toplevel (w);

  gtk_widget_show_all (dialog);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    template_widget_save_off (w);
  else
    template_widget_save_reset (w);

  gtk_widget_hide (dialog);
}

/* Funzione per gli oggetti in solitaria: */
static GtkWidget *
template_widget_standalone (GtkWidget * w)
{
  GtkWidget *dialog, *button;
  gchar s[1024];

  dialog = gtk_dialog_new ();

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Single input..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);

  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
				GTK_WINDOW (gtk_widget_get_toplevel (w)));
  gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), w, TRUE, TRUE, 5);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CLOSE);

  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  return dialog;
}

/* Questa funzione aggiunge un elemento alla lista degli input multiple: */
static void
template_widget_list_add (GtkWidget * widget, GtkTreeView * treeview)
{
  struct template_input_t *input;
  GtkListStore *model;
  GtkTreeIter iter;
  GtkWidget *del;
  GtkWidget *dialog;
  GtkWidget *entry;
  GtkWidget *w;
  GList *resources;
  GList *blanks;
  gchar *name;
  gint ret;

  input = g_object_get_data (G_OBJECT (widget), "input");
  resources = g_object_get_data (G_OBJECT (widget), "resources");
  blanks = g_object_get_data (G_OBJECT (widget), "blanks");

  w = template_widget_create (resources, blanks, input, FALSE
#ifdef USE_JS
			      , g_object_get_data (G_OBJECT (widget), "js")
#endif
    );

  /* Se e' un pulsante attivo direttamente la funzione come se
   * fosse stato cliccato: */
  entry = g_object_get_data (G_OBJECT (w), "entry");
  if (GTK_IS_BUTTON (entry))
    {
      struct template_t *template =
	g_object_get_data (G_OBJECT (entry), "template");

      /* Se questa funzione ritorna TRUE, inserisco la linea: */
      if (template_run (entry, template) == TRUE)
	{
	  model =
	    (GtkListStore *)
	    gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	  gtk_list_store_append (model, &iter);

	  name = template_widget_name (w);
	  gtk_list_store_set (model, &iter, 0, name, 1, w, -1);
	  g_free (name);

	  /* Disattivo o attivo i pulsanti: */
	  if (input->max_cardinality > 0
	      && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model),
						 NULL) >=
	      input->max_cardinality)
	    gtk_widget_set_sensitive (widget, FALSE);

	  del = g_object_get_data (G_OBJECT (widget), "del");
	  gtk_widget_set_sensitive (del, TRUE);
	}
      return;
    }

  dialog = template_widget_standalone (w);
  gtk_widget_show_all (dialog);

  while ((ret = gtk_dialog_run (GTK_DIALOG (dialog))) == GTK_RESPONSE_OK)
    {
      /* Controllo il widget: */
      if (template_widget_check (w))
	continue;

      gtk_widget_hide (dialog);

      model =
	(GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
      gtk_list_store_append (model, &iter);

      name = template_widget_name (w);
      gtk_list_store_set (model, &iter, 0, name, 1, w, -1);
      g_free (name);

      /* Disattivo o attivo i pulsanti: */
      if (input->max_cardinality > 0
	  && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model),
					     NULL) >= input->max_cardinality)
	gtk_widget_set_sensitive (widget, FALSE);

      del = g_object_get_data (G_OBJECT (widget), "del");
      gtk_widget_set_sensitive (del, TRUE);
      break;
    }

  if (ret != GTK_RESPONSE_OK)
    gtk_widget_destroy (dialog);
}

/* Questa funzione ritorna il primo blank node libero nella numerazione
 * blanknode_name %d: */
static gchar *
template_widget_blank (struct editor_data_t *data)
{
  GList *list;
  gint id = -1;
  gint max = -1;

  list = data->rdf;
  while (list)
    {
      struct rdf_t *rdf = list->data;

      if (blanknode_get (rdf->subject, &id) == TRUE && id > max)
	max = id;

      if (blanknode_get (rdf->object, &id) == TRUE && id > max)
	max = id;

      list = list->next;
    }

  return blanknode_create (max + 1);
}

/* TEMPLATE DESTROY ********************************************************/

#ifdef USE_JS
static gboolean
template_destroy (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
  struct js_t *js = g_object_get_data (G_OBJECT (widget), "js");

  if (js)
    {
      g_object_steal_data (G_OBJECT (widget), "js");
      js_destroy (js);
    }

  return FALSE;
}
#endif

/* TEMPLATE NAME WIDGETS ****************************************************/
static gchar *
template_widget_name_literal (GtkWidget * widget, gchar * name)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    return g_strdup_printf ("%s: %s", name,
			    gtk_entry_get_text (GTK_ENTRY (entry)));
  else
    {
      GtkTextIter start, end;
      gchar *tmp, *ret;
      gint i;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);

      tmp =
	gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (entry), &start, &end,
				   FALSE);

      for (i = 0; tmp[i] && i < 15; i++)
	if (tmp[i] == '\n')
	  break;

      if (!tmp[i])
	ret = g_strdup_printf ("%s: %s", name, tmp);
      else
	{
	  tmp[i] = 0;
	  ret = g_strdup_printf ("%s: %s...", name, tmp);
	}

      g_free (tmp);

      return ret;
    }
}

static gchar *
template_widget_name_resource (GtkWidget * widget, gchar * name)
{
  return template_widget_name_literal (widget, name);
}

static gchar *
template_widget_name_alt_literal (GtkWidget * widget, gchar * name)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (widget), "entry");
  gchar *text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  gchar *ret = g_strdup_printf ("%s: %s", name, text);
  g_free (text);

  return ret;
}

static gchar *
template_widget_name_alt_resource (GtkWidget * widget, gchar * name)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (widget), "entry");
  gchar *text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  gchar *ret = g_strdup_printf ("%s: %s", name, text);
  g_free (text);

  return ret;
}

static gchar *
template_widget_name_other (GtkWidget * widget, gchar * name)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    return g_strdup_printf ("%s: %s", name,
			    gtk_entry_get_text (GTK_ENTRY (entry)));
  else
    {
      GString *str;

      GtkWidget *type =
	g_object_get_data (G_OBJECT (entry), "type_resource_input");
      GtkWidget *object =
	g_object_get_data (G_OBJECT (entry), "subject_input");
      GtkWidget *toggle =
	g_object_get_data (G_OBJECT (entry), "toggle_input");

      str = g_string_new (name);

      g_string_append_printf (str, ": %s",
			      gtk_entry_get_text (GTK_ENTRY (object)));

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)))
	g_string_append_printf (str, " (%s", _("Resource"));
      else
	g_string_append_printf (str, " (%s", _("Blanknode"));

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)))
	g_string_append_printf (str, _(" with template)"));
      else
	g_string_append_printf (str, _(" without template)"));

      return g_string_free (str, FALSE);
    }
}

static gchar *
template_widget_name_default (GtkWidget * widget, gchar * name)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (widget), "entry");
  GtkWidget *type;

  type = g_object_get_data (G_OBJECT (widget), "type_literal");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    return g_strdup_printf (_("%s: (L) %s"), name,
			    gtk_entry_get_text (GTK_ENTRY (entry)));

  type = g_object_get_data (G_OBJECT (widget), "type_resource");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    return g_strdup_printf (_("%s: (R) %s"), name,
			    gtk_entry_get_text (GTK_ENTRY (entry)));

  type = g_object_get_data (G_OBJECT (widget), "type_blanknode");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    return g_strdup_printf (_("%s: (B) %s"), name,
			    gtk_entry_get_text (GTK_ENTRY (entry)));

  return g_strdup_printf ("%s: %s", name,
			  gtk_entry_get_text (GTK_ENTRY (entry)));
}

static gchar *
template_widget_name (GtkWidget * widget)
{
  struct template_func_t *func;
  gchar *label;

  label = (gchar *) g_object_get_data (G_OBJECT (widget), "label");

  func = g_object_get_data (G_OBJECT (widget), "func");

  if (func && func->name)
    return func->name (widget, label);

  return g_strdup ("empty");
}

/* TEMPLATE LOW CHECK WIDGETS ***********************************************/
static gboolean
template_widget_low_check_literal (GtkWidget * widget, gchar * label,
				   gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;
  gboolean allocated = FALSE;

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!text || !*text)
	return 0;
    }
  else
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);

      text =
	gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (entry), &start, &end,
				   FALSE);
      allocated = TRUE;
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      gchar s[1024];
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);

      if (datatype_check
	  (gtk_combo_box_get_active (GTK_COMBO_BOX (entry)), text,
	   s) == FALSE)
	{
	  if (allocated == TRUE)
	    g_free (text);
	  return 1;
	}
    }

  if (allocated == TRUE)
    g_free (text);
  return 0;
}

static gboolean
template_widget_low_check_other (GtkWidget * widget, gchar * label,
				 gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!text || !*text)
	return 0;
    }

  else
    {
      GtkWidget *widget_inputs;
      GtkWidget *button;
      GList *list, *old;

      if (!
	  (widget_inputs =
	   g_object_get_data (G_OBJECT (entry), "widget_inputs")))
	return 0;

      button = g_object_get_data (G_OBJECT (entry), "toggle_input");

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  list = old =
	    gtk_container_get_children (GTK_CONTAINER (widget_inputs));

	  while (list)
	    {
	      if (template_widget_check (list->data))
		{
		  g_list_free (old);
		  return 1;
		}

	      list = list->next;
	    }

	  g_list_free (old);
	}
    }

  return 0;
}

static gboolean
template_widget_low_check_default (GtkWidget * widget, gchar * label,
				   gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");
  text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  if (!text || !*text)
    return 0;

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      gchar s[1024];
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);

      if (datatype_check
	  (gtk_combo_box_get_active (GTK_COMBO_BOX (entry)), text,
	   s) == FALSE)
	return 1;
    }

  return 0;
}

/* TEMPLATE HIGH CHECK WIDGETS **********************************************/
static gboolean
template_widget_high_check_literal (GtkWidget * widget, gchar * label,
				    gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;
  gchar s[1024];
  gboolean allocated = FALSE;

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  else
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);

      text =
	gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (entry), &start, &end,
				   FALSE);
      allocated = TRUE;
    }

  if (!text || !*text)
    {
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);
      dialog_msg (s);

      if (allocated == TRUE)
	g_free (text);
      return 1;
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      gchar s[1024];
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);

      if (datatype_check
	  (gtk_combo_box_get_active (GTK_COMBO_BOX (entry)), text,
	   s) == FALSE)
	{
	  if (allocated == TRUE)
	    g_free (text);
	  return 1;
	}
    }

  if (allocated == TRUE)
    g_free (text);
  return 0;
}

static gboolean
template_widget_high_check_resource (GtkWidget * widget, gchar * label,
				     gint min_cardinality)
{
  return template_widget_high_check_literal (widget, label, min_cardinality);
}

static gboolean
template_widget_high_check_alt_resource (GtkWidget * widget, gchar * label,
					 gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;
  gchar s[1024];

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");
  text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  if (!text || !*text)
    {
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);
      dialog_msg (s);

      if (text)
	g_free (text);

      return 1;
    }

  return 0;
}

static gboolean
template_widget_high_check_alt_literal (GtkWidget * widget, gchar * label,
					gint min_cardinality)
{
  return template_widget_high_check_alt_resource (widget, label,
						  min_cardinality);
}

static gboolean
template_widget_high_check_other (GtkWidget * widget, gchar * label,
				  gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;
  gchar s[1024];

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!text || !*text)
	{
	  g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		      label);
	  dialog_msg (s);
	  return 1;
	}
    }
  else
    {
      GtkWidget *widget_inputs;
      GtkWidget *button;
      GList *list, *old;

      if (!
	  (widget_inputs =
	   g_object_get_data (G_OBJECT (entry), "widget_inputs")))
	{
	  g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		      label);
	  dialog_msg (s);
	  return 1;
	}

      button = g_object_get_data (G_OBJECT (entry), "toggle_input");

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  list = old =
	    gtk_container_get_children (GTK_CONTAINER (widget_inputs));

	  while (list)
	    {
	      if (template_widget_check (list->data))
		{
		  g_list_free (old);
		  return 1;
		}

	      list = list->next;
	    }

	  g_list_free (old);
	}
    }

  return 0;
}

static gboolean
template_widget_high_check_default (GtkWidget * widget, gchar * label,
				    gint min_cardinality)
{
  GtkWidget *entry;
  gchar *text;
  gchar s[1024];

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");
  text = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  if (!text || !*text)
    {
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);
      dialog_msg (s);
      return 1;
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      gchar s[1024];
      g_snprintf (s, sizeof (s), _("No compatible value for input %s"),
		  label);

      if (datatype_check
	  (gtk_combo_box_get_active (GTK_COMBO_BOX (entry)), text,
	   s) == FALSE)
	return 1;
    }

  return 0;
}

static gboolean
template_widget_high_check_cardinality (GtkWidget * widget, gchar * label,
					gint min_cardinality)
{
  GtkWidget *entry;
  gchar s[1024];
  GtkTreeModel *model;

  entry = (GtkWidget *) g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (entry));

  if (min_cardinality > gtk_tree_model_iter_n_children (model, NULL))
    {
      g_snprintf (s, sizeof (s), _("Not enough elements for input %s"),
		  label);
      dialog_msg (s);
      return 1;
    }

  return 0;
}

/* Questa funzione fa il check se l'oggetto e' stato riempito bene: */
static gboolean
template_widget_check (GtkWidget * widget)
{
  struct template_func_t *func;
  gint min_cardinality =
    (gboolean) g_object_get_data (G_OBJECT (widget), "min_cardinality");
  gchar *label = (gchar *) g_object_get_data (G_OBJECT (widget), "label");
  gchar *function_check =
    (gchar *) g_object_get_data (G_OBJECT (widget), "function_check");

  if (function_check)
    {
#ifdef USE_JS
      struct js_t *js = g_object_get_data (G_OBJECT (widget), "js");

      if (template_js_evaluate (js, widget, function_check) == 1)
	return 1;
#else
      fprintf (stderr,
	       "WARNING: Javascript not supported on this computer\n");
#endif
    }

  /* se non e' richiesto uso la low check: */
  if (!min_cardinality)
    {
      func = g_object_get_data (G_OBJECT (widget), "func");
      if (func && func->low_check)
	return func->low_check (widget, label, min_cardinality);

      return 0;
    }

  func = g_object_get_data (G_OBJECT (widget), "func");
  if (func && func->high_check)
    return func->high_check (widget, label, min_cardinality);

  return 0;
}

/* TEMPLATE DESTROY WIDGETS **************************************************/

static void
template_widget_destroy_other (GtkWidget * widget)
{
  GtkWidget *entry;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (!GTK_IS_ENTRY (entry))
    {
      GtkWidget *widget_inputs;
      GList *list, *old;

      if ((widget_inputs =
	   g_object_get_data (G_OBJECT (entry), "widget_inputs")))
	{
	  list = old =
	    gtk_container_get_children (GTK_CONTAINER (widget_inputs));

	  while (list)
	    {
	      template_widget_destroy (list->data);
	      list = list->next;
	    }

	  g_list_free (old);

	  gtk_widget_destroy (gtk_widget_get_toplevel (widget_inputs));
	  g_object_steal_data (G_OBJECT (entry), "widget_inputs");
	}

      if ((list = g_object_get_data (G_OBJECT (entry), "invisible_inputs")))
	{
	  g_list_foreach (list, (GFunc) g_free, NULL);
	  g_list_free (list);
	}
    }
}

static void
template_widget_destroy_cardinality (GtkWidget * widget)
{
  GtkWidget *entry;
  GtkTreeModel *model;
  GtkTreeIter iter;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (entry));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
	{
	  GtkWidget *w;
	  gtk_tree_model_get (model, &iter, 1, &w, -1);
	  template_widget_destroy (w);
	  gtk_widget_destroy (gtk_widget_get_toplevel (w));
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

/* Questa funzione salva il valore di un widget input: */
static void
template_widget_destroy (GtkWidget * widget)
{
  struct template_func_t *func;
  gchar *function_destroy =
    (gchar *) g_object_get_data (G_OBJECT (widget), "function_destroy");

  if (function_destroy)
    {
#ifdef USE_JS
      struct js_t *js = g_object_get_data (G_OBJECT (widget), "js");
      template_js_evaluate (js, widget, function_destroy);
#else
      fprintf (stderr,
	       "WARNING: Javascript not supported on this computer\n");
#endif
    }

  func = g_object_get_data (G_OBJECT (widget), "func");
  if (func && func->destroy)
    func->destroy (widget);

  if (func)
    {
      g_object_steal_data (G_OBJECT (widget), "func");
      g_free (func);
    }
}

/* TEMPLATE SAVE WIDGETS ****************************************************/
static void
template_widget_save_real (GtkWidget * widget, struct editor_data_t *data,
			   gchar * subject, gchar * predicate, gchar * object,
			   gchar * lang, gchar * datatype,
			   enum rdf_object_t object_type,
			   struct unredo_t *unredo, struct rdf_t *rdf_prev)
{
#ifdef USE_JS
  struct js_t *js = g_object_get_data (G_OBJECT (widget), "js");
  gchar *function_save =
    (gchar *) g_object_get_data (G_OBJECT (widget), "function_save");

  if (function_save)
    template_js_evaluate (js, widget, function_save);
#endif

  /* Se devo salvare su una tripletta esistente: */
  if (rdf_prev)
    {
      unredo_add (unredo, rdf_prev, rdf_copy (rdf_prev));

      g_free (rdf_prev->subject);
      rdf_prev->subject = g_strdup (subject);

      g_free (rdf_prev->predicate);
      rdf_prev->predicate = g_strdup (predicate);

      g_free (rdf_prev->object);
      rdf_prev->object = g_strdup (object);

      if (rdf_prev->lang)
	g_free (rdf_prev->lang);

      rdf_prev->lang = lang ? g_strdup (lang) : NULL;

      if (rdf_prev->datatype)
	g_free (rdf_prev->datatype);

      rdf_prev->datatype = datatype ? g_strdup (datatype) : NULL;

      rdf_prev->object_type = object_type;
    }

  /* Altrimenti creo una tripletta nuova: */
  else
    {
      struct rdf_t *rdf;

      /* Alloco e inserisco in lista: */
      rdf = (struct rdf_t *) g_malloc0 (sizeof (struct rdf_t));

      rdf->subject = g_strdup (subject);
      rdf->predicate = g_strdup (predicate);
      rdf->object = g_strdup (object);
      rdf->lang = lang ? g_strdup (lang) : NULL;
      rdf->datatype = datatype ? g_strdup (datatype) : NULL;
      rdf->object_type = object_type;

      data->rdf = g_list_append (data->rdf, rdf);
      unredo_add (unredo, rdf, NULL);
    }
}

static void
template_widget_save_literal (GtkWidget * widget, struct editor_data_t *data,
			      gchar * subject, gchar * predicate,
			      struct unredo_t *unredo, struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  gchar *object;
  gchar *lang = NULL;
  gchar *datatype = NULL;
  gboolean allocated = FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      object = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!object || !*object)
	return;
    }
  else
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);

      object =
	gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (entry), &start, &end,
				   FALSE);
      allocated = TRUE;
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "lang")))
    {
      lang = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!lang || !*lang)
	lang = NULL;
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      datatype = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

      if (!*datatype)
	{
	  g_free (datatype);
	  datatype = NULL;
	}
    }

  template_widget_save_real (widget, data, subject, predicate, object, lang,
			     datatype, RDF_OBJECT_LITERAL, unredo, rdf_prev);

  if (allocated == TRUE)
    g_free (object);

  if (datatype)
    g_free (datatype);
}

static void
template_widget_save_resource (GtkWidget * widget, struct editor_data_t *data,
			       gchar * subject, gchar * predicate,
			       struct unredo_t *unredo,
			       struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  object = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  if (!object || !*object)
    return;

  template_widget_save_real (widget, data, subject, predicate, object, NULL,
			     NULL, RDF_OBJECT_RESOURCE, unredo, rdf_prev);
}

static void
template_widget_save_alt_literal (GtkWidget * widget,
				  struct editor_data_t *data, gchar * subject,
				  gchar * predicate, struct unredo_t *unredo,
				  struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  object = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  if (!object)
    return;

  if (!*object)
    {
      g_free (object);
      return;
    }

  template_widget_save_real (widget, data, subject, predicate, object, NULL,
			     NULL, RDF_OBJECT_LITERAL, unredo, rdf_prev);

  g_free (object);
}

static void
template_widget_save_alt_resource (GtkWidget * widget,
				   struct editor_data_t *data,
				   gchar * subject, gchar * predicate,
				   struct unredo_t *unredo,
				   struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  object = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  if (!object)
    return;

  if (!*object)
    {
      g_free (object);
      return;
    }

  template_widget_save_real (widget, data, subject, predicate, object, NULL,
			     NULL, RDF_OBJECT_RESOURCE, unredo, rdf_prev);

  g_free (object);
}

static void
template_widget_save_other (GtkWidget * widget, struct editor_data_t *data,
			    gchar * subject, gchar * predicate,
			    struct unredo_t *unredo, struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      object = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!object || !*object)
	return;

      template_widget_save_real (widget, data, subject, predicate, object,
				 NULL, NULL, RDF_OBJECT_RESOURCE, unredo,
				 rdf_prev);
    }
  else
    {
      GtkWidget *widget_inputs;
      GtkWidget *type_resource;
      GtkWidget *button;
      GtkWidget *value;

      if (!
	  (widget_inputs =
	   g_object_get_data (G_OBJECT (entry), "widget_inputs")))
	return;

      type_resource =
	g_object_get_data (G_OBJECT (entry), "type_resource_input");
      value = g_object_get_data (G_OBJECT (entry), "subject_input");

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_resource)))
	{
	  object = (gchar *) gtk_entry_get_text (GTK_ENTRY (value));
	  if (!object || !*object)
	    return;

	  object = g_strdup (object);

	  template_widget_save_real (widget, data, subject, predicate, object,
				     NULL, NULL, RDF_OBJECT_RESOURCE, unredo,
				     rdf_prev);
	}
      else
	{
	  object = (gchar *) gtk_entry_get_text (GTK_ENTRY (value));
	  if (!object || !*object)
	    object = template_widget_blank (data);
	  else
	    object = g_strdup (object);

	  template_widget_save_real (widget, data, subject, predicate, object,
				     NULL, NULL, RDF_OBJECT_BLANK, unredo,
				     rdf_prev);
	}

      button = g_object_get_data (G_OBJECT (entry), "toggle_input");
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
	{
	  GList *list, *old;

	  list = old =
	    gtk_container_get_children (GTK_CONTAINER (widget_inputs));

	  while (list)
	    {
	      template_widget_save (list->data, data, object, unredo);
	      list = list->next;
	    }

	  g_list_free (old);

	  if ((list =
	       g_object_get_data (G_OBJECT (entry), "invisible_inputs")))
	    {
	      struct invisible_t *inv;

	      while (list)
		{
		  inv = list->data;

		  template_invisible_save (object, inv->input, data, unredo,
					   inv->rdf);
		  list = list->next;
		}
	    }
	}

      g_free (object);
    }
}

static void
template_widget_save_default (GtkWidget * widget, struct editor_data_t *data,
			      gchar * subject, gchar * predicate,
			      struct unredo_t *unredo, struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  GtkWidget *type;
  gchar *object;
  gchar *lang = NULL;
  gchar *datatype = NULL;
  enum rdf_object_t object_type = RDF_OBJECT_LITERAL;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  type = g_object_get_data (G_OBJECT (widget), "type_literal");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    object_type = RDF_OBJECT_LITERAL;

  type = g_object_get_data (G_OBJECT (widget), "type_resource");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    object_type = RDF_OBJECT_RESOURCE;

  type = g_object_get_data (G_OBJECT (widget), "type_blanknode");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    object_type = RDF_OBJECT_BLANK;

  object = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  if (!object || !*object)
    return;

  if ((entry = g_object_get_data (G_OBJECT (widget), "lang")))
    {
      lang = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (!lang || !*lang)
	lang = NULL;
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      datatype = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

      if (!*datatype)
	{
	  g_free (datatype);
	  datatype = NULL;
	}
    }

  template_widget_save_real (widget, data, subject, predicate, object, lang,
			     datatype, object_type, unredo, rdf_prev);

  if (datatype)
    free (datatype);
}

static void
template_widget_save_cardinality (GtkWidget * widget,
				  struct editor_data_t *data, gchar * subject,
				  gchar * predicate, struct unredo_t *unredo,
				  struct rdf_t *rdf_prev)
{
  GtkWidget *entry;
  GtkTreeModel *model;
  GtkTreeIter iter;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (entry));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
	{
	  GtkWidget *w;
	  gtk_tree_model_get (model, &iter, 1, &w, -1);
	  template_widget_save (w, data, subject, unredo);
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

/* Questa funzione salva il valore di un widget input: */
static void
template_widget_save (GtkWidget * widget, struct editor_data_t *data,
		      gchar * subject, struct unredo_t *unredo)
{
  gchar *property;
  struct template_func_t *func;
  struct rdf_t *rdf_prev;

  property = g_object_get_data (G_OBJECT (widget), "rdfs");
  func = g_object_get_data (G_OBJECT (widget), "func");
  rdf_prev = g_object_get_data (G_OBJECT (widget), "rdf_prev");

  if (func && func->save)
    func->save (widget, data, subject, property, unredo, rdf_prev);
}

/* Gli oggetti invisibili sono semplici semplici: */
static void
template_invisible_save (gchar * subject, struct template_input_t *i,
			 struct editor_data_t *data, struct unredo_t *unredo,
			 struct rdf_t *rdf_prev)
{
  enum template_input_typevalue_t typevalue;

  switch (i->typevalue)
    {
    case TEMPLATE_INPUT_TYPEVALUE_LITERAL:
      typevalue = RDF_OBJECT_LITERAL;
      break;

    case TEMPLATE_INPUT_TYPEVALUE_RESOURCE:
    default:
      typevalue = RDF_OBJECT_RESOURCE;
      break;
    }

  template_widget_save_real (NULL, data, subject, i->rdfs, i->default_value,
			     i->default_language, i->default_datatype,
			     typevalue, unredo, rdf_prev);
}

/* TEMPLATE SAVE ON WIDGETS *************************************************/

static void
template_widget_get_literal (GtkWidget * widget,
			     struct template_value_t *value)
{
  GtkWidget *entry;
  gchar *str;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      str = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (str && *str)
	value->value = g_strdup (str);

    }
  else
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);

      value->value =
	gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (entry), &start, &end,
				   FALSE);
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "lang")))
    {
      str = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (str && *str)
	value->lang = g_strdup (str);
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      str = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

      if (str && *str)
	value->datatype = g_strdup (str);

      if (str)
	g_free (str);
    }

  value->type = RDF_OBJECT_LITERAL;
}

static void
template_widget_get_resource (GtkWidget * widget,
			      struct template_value_t *value)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  object = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  if (object && *object)
    value->value = g_strdup (object);

  value->type = RDF_OBJECT_RESOURCE;
}

static void
template_widget_get_alt_literal (GtkWidget * widget,
				 struct template_value_t *value)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  object = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  if (!object)
    return;

  if (!*object)
    {
      g_free (object);
      return;
    }

  value->value = object;
  value->type = RDF_OBJECT_LITERAL;
}

static void
template_widget_get_alt_resource (GtkWidget * widget,
				  struct template_value_t *value)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  object = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

  if (!object)
    return;

  if (!*object)
    {
      g_free (object);
      return;
    }

  value->value = object;
  value->type = RDF_OBJECT_RESOURCE;
}

static void
template_widget_get_other (GtkWidget * widget, struct template_value_t *value)
{
  GtkWidget *entry;
  gchar *object;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      object = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (object && *object)
	value->value = g_strdup (object);

      value->type = RDF_OBJECT_RESOURCE;
    }

  else
    {
      GtkWidget *type_resource;

      type_resource =
	g_object_get_data (G_OBJECT (entry), "type_resource_input");

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_resource)))
	value->type = RDF_OBJECT_RESOURCE;

      else
	value->type = RDF_OBJECT_BLANK;
    }
}

static void
template_widget_get_default (GtkWidget * widget,
			     struct template_value_t *value)
{
  GtkWidget *entry;
  GtkWidget *type;
  gchar *str;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  str = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

  if (str && *str)
    value->value = g_strdup (str);

  type = g_object_get_data (G_OBJECT (widget), "type_literal");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    value->type = RDF_OBJECT_LITERAL;

  type = g_object_get_data (G_OBJECT (widget), "type_resource");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    value->type = RDF_OBJECT_RESOURCE;

  type = g_object_get_data (G_OBJECT (widget), "type_blanknode");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type)) == TRUE)
    value->type = RDF_OBJECT_BLANK;

  if ((entry = g_object_get_data (G_OBJECT (widget), "lang")))
    {
      str = (gchar *) gtk_entry_get_text (GTK_ENTRY (entry));

      if (str && *str)
	value->lang = g_strdup (str);
    }

  if ((entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    {
      str = gtk_combo_box_get_active_text (GTK_COMBO_BOX (entry));

      if (str && *str)
	value->datatype = g_strdup (str);

      if (str)
	g_free (str);
    }
}

static void
template_widget_save_on (GtkWidget * widget)
{
  struct template_func_t *func;
  struct template_value_t *value;

  if ((value = g_object_get_data (G_OBJECT (widget), "saved")))
    {
      g_object_steal_data (G_OBJECT (widget), "saved");
      template_value_free (value);
    }

  value = g_malloc0 (sizeof (struct template_value_t));

  func = g_object_get_data (G_OBJECT (widget), "func");
  if (func && func->get)
    func->get (widget, value);

  g_object_set_data (G_OBJECT (widget), "saved", value);
}

/* TEMPLATE SAVE OFF WIDGETS ***********************************************/

static void
template_widget_save_off (GtkWidget * widget)
{
  struct template_value_t *value;

  if ((value = g_object_get_data (G_OBJECT (widget), "saved")))
    {
      g_object_steal_data (G_OBJECT (widget), "saved");
      template_value_free (value);
    }
}

/* TEMPLATE SET WIDGETS ***********************************************/

static void
template_widget_set_literal (GtkWidget * widget,
			     struct template_value_t *value)
{
  GtkWidget *entry;

  if (value->value
      && (entry = g_object_get_data (G_OBJECT (widget), "entry")))
    {
      if (GTK_IS_ENTRY (entry))
	gtk_entry_set_text (GTK_ENTRY (entry), value->value);
      else
	{
	  GtkTextIter start, end;

	  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
	  gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);
	  gtk_text_buffer_delete (GTK_TEXT_BUFFER (entry), &start, &end);

	  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
	  gtk_text_buffer_insert (GTK_TEXT_BUFFER (entry), &start,
				  value->value, -1);
	}
    }

  if (value->lang
      && ((entry = g_object_get_data (G_OBJECT (widget), "lang"))))
    gtk_entry_set_text (GTK_ENTRY (entry), value->lang);

  if (value->datatype
      && (entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry),
			      datatype_id (value->datatype));
}

static void
template_widget_set_resource (GtkWidget * widget,
			      struct template_value_t *value)
{
  GtkWidget *entry;

  if (value->value
      && (entry = g_object_get_data (G_OBJECT (widget), "entry")))
    gtk_entry_set_text (GTK_ENTRY (entry), value->value);
}

static void
template_widget_set_alt_literal (GtkWidget * widget,
				 struct template_value_t *value)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint ok = 0;
  gint id = 0;
  GtkWidget *entry;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {

      do
	{
	  gchar *what;

	  gtk_tree_model_get (model, &iter, 0, &what, -1);

	  if (!strcmp (what, value->value))
	    {
	      ok = 1;
	      g_free (what);
	      break;
	    }


	  g_free (what);
	  id++;
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }

  if (ok)
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id);
  else
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), value->value);
      gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id + 1);
    }
}

static void
template_widget_set_alt_resource (GtkWidget * widget,
				  struct template_value_t *value)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint ok = 0;
  gint id = 0;
  GtkWidget *entry;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {

      do
	{
	  gchar *what;

	  gtk_tree_model_get (model, &iter, 0, &what, -1);

	  if (!strcmp (what, value->value))
	    {
	      ok = 1;
	      g_free (what);
	      break;
	    }


	  g_free (what);
	  id++;
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }

  if (ok)
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id);
  else
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), value->value);
      gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id + 1);
    }
}

static void
template_widget_set_other (GtkWidget * widget, struct template_value_t *value)
{
  GtkWidget *entry;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry) && value->value)
    gtk_entry_set_text (GTK_ENTRY (entry), value->value);
}

static void
template_widget_set_default (GtkWidget * widget,
			     struct template_value_t *value)
{
  GtkWidget *entry;
  GtkWidget *type;

  switch (value->type)
    {
    case RDF_OBJECT_LITERAL:
      type = g_object_get_data (G_OBJECT (widget), "type_literal");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type), TRUE);
      break;

    case RDF_OBJECT_RESOURCE:
      type = g_object_get_data (G_OBJECT (widget), "type_resource");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type), TRUE);
      break;

    case RDF_OBJECT_BLANK:
      type = g_object_get_data (G_OBJECT (widget), "type_blanknode");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type), TRUE);
      break;
    }

  if (value->value)
    {
      entry = g_object_get_data (G_OBJECT (widget), "entry");
      gtk_entry_set_text (GTK_ENTRY (entry), value->value);
    }

  if (value->lang)
    {
      entry = g_object_get_data (G_OBJECT (widget), "lang");
      gtk_entry_set_text (GTK_ENTRY (entry), value->lang);
    }

  if (value->datatype)
    {
      entry = g_object_get_data (G_OBJECT (widget), "datatype");
      gtk_combo_box_set_active (GTK_COMBO_BOX (entry),
				datatype_id (value->datatype));
    }
}

static void
template_widget_set (GtkWidget * widget, struct template_value_t *value)
{
  struct template_func_t *func;

  func = g_object_get_data (G_OBJECT (widget), "func");

  if (func && func->set)
    func->set (widget, value);
}

static void
template_widget_save_reset (GtkWidget * widget)
{
  struct template_value_t *value;

  if (!(value = g_object_get_data (G_OBJECT (widget), "saved")))
    return;

  template_widget_set (widget, value);

  template_value_free (value);
  g_object_steal_data (G_OBJECT (widget), "saved");
}

/* TEMPLATE IS YOUR WIDGETS ***********************************************/

static gboolean
template_widget_is_your_literal (GtkWidget * widget, struct rdf_t *rdf,
				 GList ** dummy)
{
  gchar *predicate;
  GtkWidget *entry;

  if (rdf->object_type != RDF_OBJECT_LITERAL)
    return FALSE;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    gtk_entry_set_text (GTK_ENTRY (entry), rdf->object);

  else
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (entry), &end);
      gtk_text_buffer_delete (GTK_TEXT_BUFFER (entry), &start, &end);

      gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
      gtk_text_buffer_insert (GTK_TEXT_BUFFER (entry), &start, rdf->object,
			      -1);
    }

  if (rdf->lang && (entry = g_object_get_data (G_OBJECT (widget), "lang")))
    gtk_entry_set_text (GTK_ENTRY (entry), rdf->lang);

  if (rdf->datatype
      && (entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry),
			      datatype_id (rdf->datatype));

  return TRUE;
}

static gboolean
template_widget_is_your_resource (GtkWidget * widget, struct rdf_t *rdf,
				  GList ** dummy)
{
  gchar *predicate;
  GtkWidget *entry;

  if (rdf->object_type != RDF_OBJECT_RESOURCE)
    return FALSE;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  gtk_entry_set_text (GTK_ENTRY (entry), rdf->object);

  return TRUE;
}

static gboolean
template_widget_is_your_alt_literal (GtkWidget * widget, struct rdf_t *rdf,
				     GList ** dummy)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint ok = 0;
  gint id = 0;
  GtkWidget *entry;
  gchar *predicate;

  if (rdf->object_type != RDF_OBJECT_LITERAL)
    return FALSE;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {

      do
	{
	  gchar *what;

	  gtk_tree_model_get (model, &iter, 0, &what, -1);

	  if (!strcmp (what, rdf->object))
	    {
	      ok = 1;
	      g_free (what);
	      break;
	    }


	  g_free (what);
	  id++;
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }

  if (ok)
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id);
  else
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), rdf->object);
      gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id + 1);
    }

  return TRUE;
}

static gboolean
template_widget_is_your_alt_resource (GtkWidget * widget, struct rdf_t *rdf,
				      GList ** dummy)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint ok = 0;
  gint id = 0;
  GtkWidget *entry;
  gchar *predicate;

  if (rdf->object_type != RDF_OBJECT_RESOURCE)
    return FALSE;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {

      do
	{
	  gchar *what;

	  gtk_tree_model_get (model, &iter, 0, &what, -1);

	  if (!strcmp (what, rdf->object))
	    {
	      ok = 1;
	      g_free (what);
	      break;
	    }


	  g_free (what);
	  id++;
	}
      while (gtk_tree_model_iter_next (model, &iter));
    }

  if (ok)
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id);
  else
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), rdf->object);
      gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id + 1);
    }

  return TRUE;
}

static gboolean
template_widget_is_your_default (GtkWidget * widget, struct rdf_t *rdf,
				 GList ** dummy)
{
  GtkWidget *entry;
  GtkWidget *type;
  gchar *predicate;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  switch (rdf->object_type)
    {
    case RDF_OBJECT_LITERAL:
      type = g_object_get_data (G_OBJECT (widget), "type_literal");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type), TRUE);
      break;

    case RDF_OBJECT_RESOURCE:
      type = g_object_get_data (G_OBJECT (widget), "type_resource");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type), TRUE);
      break;

    case RDF_OBJECT_BLANK:
      type = g_object_get_data (G_OBJECT (widget), "type_blanknode");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type), TRUE);
      break;
    }

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  gtk_entry_set_text (GTK_ENTRY (entry), rdf->object);

  if (rdf->lang && (entry = g_object_get_data (G_OBJECT (widget), "lang")))
    gtk_entry_set_text (GTK_ENTRY (entry), rdf->lang);

  if (rdf->datatype
      && (entry = g_object_get_data (G_OBJECT (widget), "datatype")))
    gtk_combo_box_set_active (GTK_COMBO_BOX (entry),
			      datatype_id (rdf->datatype));

  return TRUE;
}

static gboolean
template_widget_is_your_other (GtkWidget * widget, struct rdf_t *rdf,
			       GList ** other)
{
  GtkWidget *entry;
  GtkWidget *widget_inputs;
  GtkWidget *dialog;
  GtkWidget *type_resource;
  GtkWidget *type_blanknode;
  GtkWidget *object;
  GtkWidget *toggle;
  gchar *predicate;
  GList *resources;
  GList *blanks;
  GList *invisible;
  GList *list;
  struct template_t *template;
  gint len;

  if (rdf->object_type != RDF_OBJECT_RESOURCE
      && rdf->object_type != RDF_OBJECT_BLANK)
    return FALSE;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");

  if (GTK_IS_ENTRY (entry))
    {
      gtk_entry_set_text (GTK_ENTRY (entry), rdf->object);
      return TRUE;
    }

  resources = g_object_get_data (G_OBJECT (entry), "resources");
  blanks = g_object_get_data (G_OBJECT (entry), "blanks");
  template = g_object_get_data (G_OBJECT (entry), "template");

  if (!(widget_inputs = template_open_real (resources, blanks, template)))
    return FALSE;

  dialog =
    template_run_create (widget_inputs, template, resources, blanks,
			 &type_resource, &type_blanknode, &object, &toggle);
  gtk_widget_hide (dialog);

  gtk_entry_set_text (GTK_ENTRY (object), rdf->object);

  if (rdf->object_type == RDF_OBJECT_RESOURCE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type_resource), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type_blanknode), TRUE);

  gtk_entry_set_text (GTK_ENTRY (object), rdf->object);

  len = g_list_length (*other);

  if ((list = gtk_container_get_children (GTK_CONTAINER (widget_inputs))))
    {
      GList *sorted = template_edit_set_sort (list);
      g_list_free (list);

      template_edit_set (&sorted, other, rdf->object);

      if (sorted)
	g_list_free (sorted);
    }

  if (len == g_list_length (*other))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), FALSE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);

  g_object_set_data (G_OBJECT (entry), "widget_inputs", widget_inputs);
  g_object_set_data (G_OBJECT (entry), "subject_input", object);
  g_object_set_data (G_OBJECT (entry), "type_resource_input", type_resource);
  g_object_set_data (G_OBJECT (entry), "type_blanknode_input",
		     type_blanknode);
  g_object_set_data (G_OBJECT (entry), "type_blanknode_a_input", 0);
  g_object_set_data (G_OBJECT (entry), "toggle_input", toggle);

  invisible = NULL;
  template_edit_invisible (&invisible, other, rdf->object, template);
  g_object_set_data (G_OBJECT (entry), "invisible_inputs", invisible);

  return TRUE;
}

static gboolean
template_widget_is_your_cardinality (GtkWidget * widget, struct rdf_t *rdf,
				     GList ** other)
{
  GtkWidget *entry;
  GtkListStore *model;
  GtkTreeIter iter;
  GtkWidget *w;
  GtkWidget *del;
  GList *resources;
  GList *blanks;
  struct template_input_t *input;
  gchar *predicate;
  gchar *name;

  predicate = g_object_get_data (G_OBJECT (widget), "rdfs");
  if (!predicate || strcmp (predicate, rdf->predicate))
    return FALSE;

  entry = g_object_get_data (G_OBJECT (widget), "entry");
  input = g_object_get_data (G_OBJECT (entry), "input");

  switch (input->typevalue)
    {
    case TEMPLATE_INPUT_TYPEVALUE_RESOURCE:
    case TEMPLATE_INPUT_TYPEVALUE_ALT_RESOURCE:
      if (rdf->object_type != RDF_OBJECT_RESOURCE)
	return FALSE;
      break;

    case TEMPLATE_INPUT_TYPEVALUE_LITERAL:
    case TEMPLATE_INPUT_TYPEVALUE_ALT_LITERAL:
      if (rdf->object_type != RDF_OBJECT_LITERAL)
	return FALSE;
      break;

    default:
      break;
    }

  resources = g_object_get_data (G_OBJECT (entry), "resources");
  blanks = g_object_get_data (G_OBJECT (entry), "blanks");

  w = template_widget_create (resources, blanks, input, FALSE
#ifdef USE_JS
			      , g_object_get_data (G_OBJECT (widget), "js")
#endif
    );

  template_widget_is_your (w, rdf, other);

  model = (GtkListStore *) gtk_tree_view_get_model (GTK_TREE_VIEW (entry));
  gtk_list_store_append (model, &iter);

  name = template_widget_name (w);
  gtk_list_store_set (model, &iter, 0, name, 1, w, -1);
  g_free (name);

  if (!GTK_IS_BUTTON (w))
    template_widget_standalone (w);

  /* Disattivo o attivo i pulsanti: */
  input = g_object_get_data (G_OBJECT (entry), "input");
  if (input->max_cardinality > 0
      && gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model),
					 NULL) >= input->max_cardinality)
    gtk_widget_set_sensitive (widget, FALSE);

  del = g_object_get_data (G_OBJECT (entry), "del");
  gtk_widget_set_sensitive (del, TRUE);

  return TRUE;
}

static gboolean
template_widget_is_your (GtkWidget * widget, struct rdf_t *rdf,
			 GList ** other)
{
  struct template_func_t *func;

  func = g_object_get_data (G_OBJECT (widget), "func");

  if (func && func->is_your && func->is_your (widget, rdf, other) == TRUE)
    {
      g_object_set_data (G_OBJECT (widget), "rdf_prev", rdf);
      return TRUE;
    }

  return FALSE;
}

/* TEMPLATE CREATE WIDGETS ***************************************************/

static void
template_widget_create_literal (struct template_input_t *input,
				GList * resources, GList * blanks,
				GtkWidget * table, gint * line,
				GtkWidget * top)
{
  struct template_func_t *func;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *lang;
  GtkWidget *datatype;

  label = gtk_label_new (_("String:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  if (input->longtext == FALSE)
    {
      entry = gtk_entry_new ();
      gtk_table_attach (GTK_TABLE (table), entry, 1, 2, *line, *line + 1,
			(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 5, 0);

      if (input->default_value)
	gtk_entry_set_text (GTK_ENTRY (entry), input->default_value);
    }

  else
    {
      GtkWidget *text;
      GtkWidget *scrolledwindow;

      scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_table_attach (GTK_TABLE (table), scrolledwindow, 1, 2, *line,
			*line + 1, (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 5, 0);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);


      entry = (GtkWidget *) gtk_text_buffer_new (NULL);

      text = gtk_text_view_new_with_buffer (GTK_TEXT_BUFFER (entry));
      gtk_text_view_set_editable (GTK_TEXT_VIEW (text), TRUE);
      gtk_container_add (GTK_CONTAINER (scrolledwindow), text);

      if (input->default_value)
	{
	  GtkTextIter start;

	  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (entry), &start);
	  gtk_text_buffer_insert (GTK_TEXT_BUFFER (entry), &start,
				  input->default_value, -1);
	}
    }

  g_object_set_data (G_OBJECT (top), "entry", entry);

  if (input->lang)
    {
      (*line)++;

      label = gtk_label_new (_("Language:"));
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 5, 0);

      lang = gtk_entry_new ();
      gtk_table_attach (GTK_TABLE (table), lang, 1, 2, *line, *line + 1,
			(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 5, 0);

      if (input->default_language)
	gtk_entry_set_text (GTK_ENTRY (lang), input->default_language);

      g_object_set_data (G_OBJECT (top), "lang", lang);
    }

  if (input->datatype)
    {
      gint id;
      GtkWidget *frame, *box, *hbox;
      struct datatype_t *datatypes = datatype_list ();

      (*line)++;

      frame = gtk_frame_new (NULL);
      gtk_table_attach (GTK_TABLE (table), frame, 0, 2, *line, *line + 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 5, 0);

      box = gtk_vbox_new (FALSE, 5);
      gtk_container_add (GTK_CONTAINER (frame), box);

      hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 5);

      label = gtk_label_new (_("Datatype:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

      datatype = gtk_combo_box_entry_new_text ();
      gtk_box_pack_start (GTK_BOX (hbox), datatype, TRUE, TRUE, 5);

      (*line)++;

      label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

      gtk_combo_box_append_text (GTK_COMBO_BOX (datatype), datatypes[0].uri);
      gtk_combo_box_set_active (GTK_COMBO_BOX (datatype), 0);

      for (id = 1; datatypes[id].uri; id++)
	{
	  gtk_combo_box_append_text (GTK_COMBO_BOX (datatype),
				     datatypes[id].uri);

	  if (input->default_datatype
	      && !strcmp (input->default_datatype, datatypes[id].uri))
	    gtk_combo_box_set_active (GTK_COMBO_BOX (datatype), id);
	}

      datatype_change (datatype, label);
      g_signal_connect_after ((gpointer) datatype, "changed",
			      G_CALLBACK (datatype_change), label);

      g_object_set_data (G_OBJECT (top), "datatype", datatype);
    }

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_LITERAL;
  func->low_check = template_widget_low_check_literal;
  func->high_check = template_widget_high_check_literal;
  func->save = template_widget_save_literal;
  func->get = template_widget_get_literal;
  func->set = template_widget_set_literal;
  func->is_your = template_widget_is_your_literal;
  func->name = template_widget_name_literal;

  g_object_set_data (G_OBJECT (top), "func", func);
}

static void
template_widget_create_resource (struct template_input_t *input,
				 GList * resources, GList * blanks,
				 GtkWidget * table, gint * line,
				 GtkWidget * top)
{
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *image;
  struct template_func_t *func;

  label = gtk_label_new (_("Resource:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  hbox = gtk_hbox_new (FALSE, 0);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

  if (input->default_value)
    gtk_entry_set_text (GTK_ENTRY (entry), input->default_value);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "list", resources);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), entry);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
		    (GtkAttachOptions) (0), 5, 0);

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_RESOURCE;
  func->high_check = template_widget_high_check_resource;
  func->save = template_widget_save_resource;
  func->get = template_widget_get_resource;
  func->set = template_widget_set_resource;
  func->is_your = template_widget_is_your_resource;
  func->name = template_widget_name_resource;

  g_object_set_data (G_OBJECT (top), "func", func);
  g_object_set_data (G_OBJECT (top), "entry", entry);
}

static void
template_widget_create_alt_literal (struct template_input_t *input,
				    GList * resources, GList * blanks,
				    GtkWidget * table, gint * line,
				    GtkWidget * top)
{
  GtkWidget *label;
  GtkWidget *entry;
  GList *list;
  gint id = 0;
  struct template_func_t *func;

  label = gtk_label_new (_("Literal:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  entry = gtk_combo_box_entry_new_text ();

  list = input->alt_typevalue;
  id = 0;

  while (list)
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), list->data);

      if (input->default_value
	  && !strcmp ((gchar *) list->data, input->default_value))
	gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id);

      id++;
      list = list->next;
    }

  gtk_combo_box_set_active (GTK_COMBO_BOX (entry), 0);

  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
		    (GtkAttachOptions) (0), 5, 0);

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_ALT_LITERAL;
  func->high_check = template_widget_high_check_alt_literal;
  func->save = template_widget_save_alt_literal;
  func->get = template_widget_get_alt_literal;
  func->set = template_widget_set_alt_literal;
  func->is_your = template_widget_is_your_alt_literal;
  func->name = template_widget_name_alt_literal;

  g_object_set_data (G_OBJECT (top), "entry", entry);
  g_object_set_data (G_OBJECT (top), "func", func);
}

static void
template_widget_create_alt_resource (struct template_input_t *input,
				     GList * resources, GList * blanks,
				     GtkWidget * table, gint * line,
				     GtkWidget * top)
{
  GtkWidget *label;
  GtkWidget *entry;
  GList *list;
  gint id = 0;
  struct template_func_t *func;

  label = gtk_label_new (_("Resource:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  entry = gtk_combo_box_entry_new_text ();

  list = input->alt_typevalue;
  id = 0;

  while (list)
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (entry), list->data);

      if (input->default_value
	  && !strcmp ((gchar *) list->data, input->default_value))
	gtk_combo_box_set_active (GTK_COMBO_BOX (entry), id);

      id++;
      list = list->next;
    }

  gtk_combo_box_set_active (GTK_COMBO_BOX (entry), 0);

  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
		    (GtkAttachOptions) (0), 5, 0);

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_ALT_RESOURCE;
  func->high_check = template_widget_high_check_alt_resource;
  func->save = template_widget_save_alt_resource;
  func->get = template_widget_get_alt_resource;
  func->set = template_widget_set_alt_resource;
  func->is_your = template_widget_is_your_alt_resource;
  func->name = template_widget_name_alt_resource;

  g_object_set_data (G_OBJECT (top), "entry", entry);
  g_object_set_data (G_OBJECT (top), "func", func);
  g_object_set_data (G_OBJECT (top), "entry", entry);
}

static void
template_widget_create_other (struct template_input_t *input,
			      GList * resources, GList * blanks,
			      GtkWidget * table, gint * line, GtkWidget * top)
{
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *image;
  GList *list;
  struct template_t *template;
  struct template_func_t *func;

  list = template_list;
  while (list)
    {
      template = list->data;

      if (!strcmp (template->uri, input->other_typevalue))
	break;

      list = list->next;
    }

  if (!list)
    {
      label = gtk_label_new (_("Template:"));
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line,
			*line + 1, (GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 5, 0);

      hbox = gtk_hbox_new (FALSE, 0);

      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

      button = gtk_button_new ();
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_object_set_data (G_OBJECT (button), "list", resources);
      g_signal_connect ((gpointer) button, "clicked",
			G_CALLBACK (search_resources), entry);

      image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
      gtk_container_add (GTK_CONTAINER (button), image);

      gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, *line,
			*line + 1,
			(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 5, 0);
    }
  else
    {
      entry = gtk_button_new_with_label (template->name);
      g_object_set_data (G_OBJECT (entry), "resources", resources);
      g_object_set_data (G_OBJECT (entry), "blanks", blanks);
      g_object_set_data (G_OBJECT (entry), "template", template);
      g_signal_connect ((gpointer) entry, "clicked",
			G_CALLBACK (template_run), template);

      gtk_table_attach (GTK_TABLE (table), entry, 0, 2, *line,
			*line + 1,
			(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 5, 0);
    }

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_OTHER;
  func->low_check = template_widget_low_check_other;
  func->high_check = template_widget_high_check_other;
  func->save = template_widget_save_other;
  func->get = template_widget_get_other;
  func->set = template_widget_set_other;
  func->is_your = template_widget_is_your_other;
  func->name = template_widget_name_other;
  func->destroy = template_widget_destroy_other;

  g_object_set_data (G_OBJECT (top), "entry", entry);
  g_object_set_data (G_OBJECT (top), "func", func);
}

/* Questa funzione viene azionata dai radiobutton della tipologia coggetto */
static void
template_widget_create_default_toggled (GtkWidget * w, gpointer dummy)
{
  GtkWidget *t_literal;
  GtkWidget *t_resource;
  GtkWidget *t_blanknode;
  GtkWidget *object;
  GtkWidget *lang;
  GtkWidget *datatype;
  GList *resources;
  GList *blanks;

  /* Una serie di valori che mi servono: */
  t_literal = (GtkWidget *) g_object_get_data (G_OBJECT (w), "literal");
  t_resource = (GtkWidget *) g_object_get_data (G_OBJECT (w), "resource");
  t_blanknode = (GtkWidget *) g_object_get_data (G_OBJECT (w), "blanknode");
  blanks = (GList *) g_object_get_data (G_OBJECT (w), "blank_list");
  resources = (GList *) g_object_get_data (G_OBJECT (w), "resources_list");
  object = (GtkWidget *) g_object_get_data (G_OBJECT (w), "object");
  lang = (GtkWidget *) g_object_get_data (G_OBJECT (w), "lang");
  datatype = (GtkWidget *) g_object_get_data (G_OBJECT (w), "datatype");

  /* Se e' stato selezionato un letterale non ci devono essere suggerimenti
   * e la lungua e' sensibile:*/
  if (w == t_literal)
    {
      g_object_steal_data (G_OBJECT (object), "list");
      gtk_widget_set_sensitive (lang, TRUE);
      gtk_widget_set_sensitive (datatype, TRUE);
    }

  /* Negli altri casi cambia la tipologia di suggerimento: */
  else if (w == t_resource)
    {
      g_object_set_data (G_OBJECT (object), "list", resources);
      gtk_widget_set_sensitive (lang, FALSE);
      gtk_widget_set_sensitive (datatype, FALSE);
    }

  else
    {
      g_object_set_data (G_OBJECT (object), "list", blanks);
      gtk_widget_set_sensitive (lang, FALSE);
      gtk_widget_set_sensitive (datatype, FALSE);
    }
}

static void
template_widget_create_default (struct template_input_t *input,
				GList * resources, GList * blanks,
				GtkWidget * table, gint * line,
				GtkWidget * top)
{
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *lang;
  GtkWidget *datatype;
  GtkWidget *type_literal;
  GtkWidget *type_resource;
  GtkWidget *type_blanknode;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *frame;
  GtkWidget *box;
  gint id;
  GSList *l = NULL;
  struct template_func_t *func;
  struct datatype_t *datatypes = datatype_list ();

  label = gtk_label_new (_("Type:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 3,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  type_literal = gtk_radio_button_new_with_label (NULL, _("Literal"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (type_literal), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (type_literal));

  gtk_table_attach (GTK_TABLE (table), type_literal, 1, 2, *line,
		    *line + 1, (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  type_resource = gtk_radio_button_new_with_label (NULL, _("Resource"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (type_resource), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (type_resource));

  (*line)++;
  gtk_table_attach (GTK_TABLE (table), type_resource, 1, 2, *line,
		    *line + 1, (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  type_blanknode = gtk_radio_button_new_with_label (NULL, _("Blank node"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (type_blanknode), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (type_blanknode));

  (*line)++;
  gtk_table_attach (GTK_TABLE (table), type_blanknode, 1, 2, *line,
		    *line + 1, (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  (*line)++;
  label = gtk_label_new (_("Language:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  lang = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (table), lang, 1, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
		    (GtkAttachOptions) (0), 5, 0);
  g_object_set_data (G_OBJECT (top), "lang", lang);

  if (input->default_language)
    gtk_entry_set_text (GTK_ENTRY (lang), input->default_language);

  (*line)++;

  frame = gtk_frame_new (NULL);
  gtk_table_attach (GTK_TABLE (table), frame, 0, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  box = gtk_vbox_new (FALSE, 5);
  gtk_container_add (GTK_CONTAINER (frame), box);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 5);

  label = gtk_label_new (_("Datatype:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);

  datatype = gtk_combo_box_entry_new_text ();
  gtk_box_pack_start (GTK_BOX (hbox), datatype, TRUE, TRUE, 5);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

  gtk_combo_box_append_text (GTK_COMBO_BOX (datatype), datatypes[0].uri);
  gtk_combo_box_set_active (GTK_COMBO_BOX (datatype), 0);

  for (id = 1; datatypes[id].uri; id++)
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (datatype), datatypes[id].uri);

      if (input->default_datatype
	  && !strcmp (input->default_datatype, datatypes[id].uri))
	gtk_combo_box_set_active (GTK_COMBO_BOX (datatype), id);
    }

  datatype_change (datatype, label);
  g_signal_connect_after ((gpointer) datatype, "changed",
			  G_CALLBACK (datatype_change), label);

  g_object_set_data (G_OBJECT (top), "datatype", datatype);

  (*line)++;
  label = gtk_label_new (_("Value:"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  hbox = gtk_hbox_new (FALSE, 0);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

  if (input->default_value)
    gtk_entry_set_text (GTK_ENTRY (entry), input->default_value);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "list", resources);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), entry);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
		    (GtkAttachOptions) (0), 5, 0);

  g_object_set_data (G_OBJECT (top), "entry", entry);
  g_object_set_data (G_OBJECT (top), "type_literal", type_literal);
  g_object_set_data (G_OBJECT (top), "type_resource", type_resource);
  g_object_set_data (G_OBJECT (top), "type_blanknode", type_blanknode);

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_DEFAULT;
  func->low_check = template_widget_low_check_default;
  func->high_check = template_widget_high_check_default;
  func->save = template_widget_save_default;
  func->get = template_widget_get_default;
  func->set = template_widget_set_default;
  func->is_your = template_widget_is_your_default;
  func->name = template_widget_name_default;

  g_object_set_data (G_OBJECT (top), "entry", entry);
  g_object_set_data (G_OBJECT (top), "func", func);

  /* Variabili per le funzioni di callback: */
  g_object_set_data (G_OBJECT (type_literal), "literal", type_literal);
  g_object_set_data (G_OBJECT (type_literal), "resource", type_resource);
  g_object_set_data (G_OBJECT (type_literal), "blanknode", type_blanknode);
  g_object_set_data (G_OBJECT (type_literal), "blank_list", blanks);
  g_object_set_data (G_OBJECT (type_literal), "resources_list", resources);
  g_object_set_data (G_OBJECT (type_literal), "object", button);
  g_object_set_data (G_OBJECT (type_literal), "lang", lang);
  g_object_set_data (G_OBJECT (type_literal), "datatype", datatype);

  g_object_set_data (G_OBJECT (type_resource), "literal", type_literal);
  g_object_set_data (G_OBJECT (type_resource), "resource", type_resource);
  g_object_set_data (G_OBJECT (type_resource), "blanknode", type_blanknode);
  g_object_set_data (G_OBJECT (type_resource), "blank_list", blanks);
  g_object_set_data (G_OBJECT (type_resource), "resources_list", resources);
  g_object_set_data (G_OBJECT (type_resource), "object", button);
  g_object_set_data (G_OBJECT (type_resource), "lang", lang);
  g_object_set_data (G_OBJECT (type_resource), "datatype", datatype);

  g_object_set_data (G_OBJECT (type_blanknode), "literal", type_literal);
  g_object_set_data (G_OBJECT (type_blanknode), "resource", type_resource);
  g_object_set_data (G_OBJECT (type_blanknode), "blanknode", type_blanknode);
  g_object_set_data (G_OBJECT (type_blanknode), "blank_list", blanks);
  g_object_set_data (G_OBJECT (type_blanknode), "resources_list", resources);
  g_object_set_data (G_OBJECT (type_blanknode), "object", button);
  g_object_set_data (G_OBJECT (type_blanknode), "lang", lang);
  g_object_set_data (G_OBJECT (type_blanknode), "datatype", datatype);

  g_signal_connect_after ((gpointer) type_literal, "toggled",
			  G_CALLBACK (template_widget_create_default_toggled),
			  NULL);
  g_signal_connect_after ((gpointer) type_blanknode, "toggled",
			  G_CALLBACK (template_widget_create_default_toggled),
			  NULL);
  g_signal_connect_after ((gpointer) type_resource, "toggled",
			  G_CALLBACK (template_widget_create_default_toggled),
			  NULL);
}

static void
template_widget_create_cardinality (struct template_input_t *input,
				    GList * resources, GList * blanks,
				    GtkWidget * table, gint * line,
				    GtkWidget * top)
{
  GtkWidget *label;
  GtkWidget *entry;
  gchar buf[1024];
  GtkWidget *scrolledwindow;
  GtkListStore *model;
  GtkWidget *b_add, *b_del;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  struct template_func_t *func;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_table_attach (GTK_TABLE (table), scrolledwindow, 0, 2, *line,
		    *line + 1, (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
				       (scrolledwindow), GTK_SHADOW_IN);

  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

  entry = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (entry), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), entry);
  g_signal_connect ((gpointer) entry, "row_activated",
		    G_CALLBACK (template_widget_list_select), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (entry));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (entry), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("List"), renderer,
					      "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (entry), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  (*line)++;

  /* Pulsanti per l'aggiunta e la rimozione di elementi: */
  b_del = gtk_button_new_from_stock ("gtk-remove");
  gtk_widget_set_sensitive (b_del, FALSE);
  g_object_set_data (G_OBJECT (b_del), "input", input);

  gtk_table_attach (GTK_TABLE (table), b_del, 0, 1, *line, *line + 1,
		    (GTK_EXPAND | GTK_FILL), 0, 5, 0);
  g_signal_connect (G_OBJECT (b_del), "clicked",
		    G_CALLBACK (template_widget_list_remove), entry);

  b_add = gtk_button_new_from_stock ("gtk-add");
  g_object_set_data (G_OBJECT (b_add), "del", b_del);
  g_object_set_data (G_OBJECT (b_add), "input", input);
  g_object_set_data (G_OBJECT (b_add), "resources", resources);
  g_object_set_data (G_OBJECT (b_add), "blanks", blanks);

#ifdef USE_JS
  g_object_set_data (G_OBJECT (b_add), "js",
		     g_object_get_data (G_OBJECT (top), "js"));
#endif

  g_object_set_data (G_OBJECT (b_del), "add", b_add);

  gtk_table_attach (GTK_TABLE (table), b_add, 1, 2, *line, *line + 1,
		    (GTK_EXPAND | GTK_FILL), 0, 5, 0);
  g_signal_connect (G_OBJECT (b_add), "clicked",
		    G_CALLBACK (template_widget_list_add), entry);

  /* Informazioni sulla cardinalita': */
  (*line)++;
  g_snprintf (buf, sizeof (buf), _("Min Card.: %d"), input->min_cardinality);
  label = gtk_label_new (buf);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL),
		    (GtkAttachOptions) (0), 5, 0);

  if (input->max_cardinality <= 0)
    g_snprintf (buf, sizeof (buf), _("Max Card.: infinite"));
  else
    g_snprintf (buf, sizeof (buf), _("Max Card.: %d"),
		input->max_cardinality);

  label = gtk_label_new (buf);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, *line, *line + 1,
		    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
		    (GtkAttachOptions) (0), 5, 0);

  g_object_set_data (G_OBJECT (top), "entry", entry);
  g_object_set_data (G_OBJECT (entry), "input", input);
  g_object_set_data (G_OBJECT (entry), "resources", resources);
  g_object_set_data (G_OBJECT (entry), "blanks", blanks);
  g_object_set_data (G_OBJECT (entry), "del", b_del);

  func = g_malloc0 (sizeof (struct template_func_t));

  func->type = TEMPLATE_WIDGET_CARDINALITY;
  func->high_check = template_widget_high_check_cardinality;
  func->save = template_widget_save_cardinality;
  func->is_your = template_widget_is_your_cardinality;
  func->destroy = template_widget_destroy_cardinality;

  g_object_set_data (G_OBJECT (top), "func", func);
}

/* Questa funzione crea l'oggetto di un input per template: */
static GtkWidget *
template_widget_create (GList * resources, GList * blanks,
			struct template_input_t *input, gboolean cardinality
#ifdef USE_JS
			, struct js_t *js
#endif
  )
{
  GtkWidget *frame;
  GtkWidget *table;
  GtkWidget *event;
  GtkWidget *label;
  gint line = 0;
  struct info_box_t *label_value = NULL;
  struct info_box_t *comment_value = NULL;
  GList *list;

  frame = gtk_frame_new (NULL);
#ifdef USE_JS
  g_object_set_data (G_OBJECT (frame), "js", js);
#endif

  /* Cerco il label piu' appropriato: */
  list = input->labels;
  while (list)
    {
      label_value = list->data;

      if (!label_value->lang)
	break;

      list = list->next;
    }

  if (!list)
    label_value = input->labels->data;

  event = gtk_event_box_new ();
  gtk_widget_set_events (event, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (G_OBJECT (event), "enter-notify-event",
		    G_CALLBACK (info_box_enter), NULL);
  g_signal_connect (G_OBJECT (event), "leave-notify-event",
		    G_CALLBACK (info_box_leave), NULL);
  g_signal_connect (G_OBJECT (event), "button_press_event",
		    G_CALLBACK (info_box_popup_show), input->labels);
  g_signal_connect (G_OBJECT (event), "button_release_event",
		    G_CALLBACK (info_box_popup_hide), NULL);

  gtk_frame_set_label_widget (GTK_FRAME (frame), event);

  /* Grassetto se la cardinalita' minima e' > di 0: */
  if (cardinality == TRUE && input->min_cardinality > 0)
    {
      gchar *s;
      gchar *str;

      str = markup (label_value->value);
      s = g_strdup_printf ("<b>%s</b>", str);
      g_free (str);

      label = gtk_label_new (s);
      g_free (s);

      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      g_object_set_data (G_OBJECT (event), "bold", (gpointer) TRUE);
    }
  else
    {
      gchar *str;

      str = markup (label_value->value);
      label = gtk_label_new (str);
      g_free (str);
    }

  gtk_container_add (GTK_CONTAINER (event), label);

  table = gtk_table_new (0, 0, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);

  /* Se la cardinalita' mi impone 1 elemento come massimo allora: */
  if (cardinality == FALSE
      || (input->min_cardinality <= 1 && input->max_cardinality == 1))
    {
      switch (input->typevalue)
	{
	  /* Se e' un letterale, valore e lingua: */
	case TEMPLATE_INPUT_TYPEVALUE_LITERAL:
	  template_widget_create_literal (input, resources, blanks, table,
					  &line, frame);
	  break;

	  /* Se e' una risorsa un combobox con suggerimenti: */
	case TEMPLATE_INPUT_TYPEVALUE_RESOURCE:
	  template_widget_create_resource (input, resources, blanks, table,
					   &line, frame);
	  break;

	  /* Se e' un'alternativa genero una combobox: */
	case TEMPLATE_INPUT_TYPEVALUE_ALT_RESOURCE:
	  template_widget_create_alt_resource (input, resources, blanks,
					       table, &line, frame);
	  break;

	case TEMPLATE_INPUT_TYPEVALUE_ALT_LITERAL:
	  template_widget_create_alt_literal (input, resources, blanks, table,
					      &line, frame);
	  break;

	  /* Se devo richiamare un altro template: */
	case TEMPLATE_INPUT_TYPEVALUE_OTHER:
	  template_widget_create_other (input, resources, blanks, table,
					&line, frame);
	  break;

	  /* Altrimenti scelta multipla: */
	default:
	  template_widget_create_default (input, resources, blanks, table,
					  &line, frame);
	  break;
	}
    }

  /* Cardinalita' maggiore di 1: */
  else
    template_widget_create_cardinality (input, resources, blanks, table,
					&line, frame);

  /* Se ho un commento da mostrare lo mostro: */
  if (input->comments)
    {
      gchar *str;

      line++;

      /* Cerco il commento piu' appropriato: */
      list = input->comments;
      while (list)
	{
	  comment_value = list->data;

	  if (!comment_value->lang)
	    break;

	  list = list->next;
	}

      if (!list)
	comment_value = input->comments->data;

      label = gtk_label_new (_("Comment:"));
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, line, line + 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 5, 0);

      event = gtk_event_box_new ();
      gtk_widget_set_events (event, GDK_BUTTON_PRESS_MASK);
      g_signal_connect (G_OBJECT (event), "enter-notify-event",
			G_CALLBACK (info_box_enter), NULL);
      g_signal_connect (G_OBJECT (event), "leave-notify-event",
			G_CALLBACK (info_box_leave), NULL);
      g_signal_connect (G_OBJECT (event), "button_press_event",
			G_CALLBACK (info_box_popup_show), input->comments);
      g_signal_connect (G_OBJECT (event), "button_release_event",
			G_CALLBACK (info_box_popup_hide), NULL);

      gtk_table_attach (GTK_TABLE (table), event, 1, 2, line, line + 1,
			(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
			(GtkAttachOptions) (0), 5, 0);

      str = markup (comment_value->value);
      label = gtk_label_new (str);
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      g_free (str);

      gtk_container_add (GTK_CONTAINER (event), label);
    }

  g_object_set_data (G_OBJECT (frame), "label", label_value->value);
  g_object_set_data (G_OBJECT (frame), "min_cardinality",
		     (gpointer) input->min_cardinality);
  g_object_set_data (G_OBJECT (frame), "rdfs", input->rdfs);
  g_object_set_data (G_OBJECT (frame), "typevalue",
		     (gpointer) input->typevalue);

  if (input->function_check)
    g_object_set_data (G_OBJECT (frame), "function_check",
		       (gpointer) input->function_check);

  if (input->function_destroy)
    g_object_set_data (G_OBJECT (frame), "function_destroy",
		       (gpointer) input->function_destroy);

  if (input->function_save)
    g_object_set_data (G_OBJECT (frame), "function_save",
		       (gpointer) input->function_save);

  if (input->function_init)
    {
#ifdef USE_JS
      template_js_evaluate (js, frame, input->function_init);
#else
      fprintf (stderr,
	       "WARNING: Javascript not supported on this computer\n");
#endif
    }

  return frame;
}

/* RUN OTHER TEMPLATE *******************************************************/
static void
template_run_toggled (GtkWidget * w, GtkWidget * sw)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)) == TRUE)
    gtk_widget_set_sensitive (sw, TRUE);
  else
    gtk_widget_set_sensitive (sw, FALSE);
}

static GtkWidget *
template_run_create (GtkWidget * widget_inputs, struct template_t *t,
		     GList * resources, GList * blanks,
		     GtkWidget ** type_resource, GtkWidget ** type_blanknode,
		     GtkWidget ** object, GtkWidget ** toggle)
{
  GtkWidget *button;
  GtkWidget *sw;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *image;
  GtkWidget *vbox, *box, *hbox;
  GtkWidget *dialog;
  gchar s[1024];
  GSList *l;

  dialog = gtk_dialog_new ();

  g_snprintf (s, sizeof (s), "%s %s - %s: %s %s", PACKAGE, VERSION,
	      _("Template"), t->name, t->version);
  gtk_window_set_title (GTK_WINDOW (dialog), s);

  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

  label = gtk_label_new (_("<b>Type</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, FALSE,
		      FALSE, 5);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new ("Type: ");
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  /* Possibilita' delle tipologie per l'input: */
  l = NULL;
  *type_resource = gtk_radio_button_new_with_label (NULL, _("Resource"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (*type_resource), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (*type_resource));
  gtk_box_pack_start (GTK_BOX (box), *type_resource, TRUE, TRUE, 3);

  *type_blanknode = gtk_radio_button_new_with_label (NULL, _("Blank node"));
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (*type_blanknode), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (*type_blanknode));
  gtk_box_pack_start (GTK_BOX (box), *type_blanknode, TRUE, TRUE, 3);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (*type_resource), TRUE);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 5);

  label = gtk_label_new (_("Value: "));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 5);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 5);

  *object = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), *object, TRUE, TRUE, 0);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "list", resources);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), object);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  label = gtk_label_new (_("<b>Template</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  *toggle = gtk_check_button_new ();
  gtk_container_add (GTK_CONTAINER (*toggle), label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (*toggle), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), *toggle);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE,
		      TRUE, 5);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  g_signal_connect_after ((gpointer) * toggle, "toggled",
			  G_CALLBACK (template_run_toggled), sw);

  gtk_container_add (GTK_CONTAINER (frame), sw);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
					 widget_inputs);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CLOSE);

  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  template_set_size (dialog, sw, widget_inputs);

  return dialog;
}

static gboolean
template_run (GtkWidget * widget, struct template_t *t)
{
  GtkWidget *dialog;
  GtkWidget *object;
  GtkWidget *widget_inputs;
  GtkWidget *type_resource;
  GtkWidget *type_blanknode;
  GtkWidget *toggle;
  GList *resources;
  GList *blanks;
  gint ret;
  gint automatic;

  if ((widget_inputs =
       g_object_get_data (G_OBJECT (widget), "widget_inputs")))
    {
      gchar *object_saved;
      gboolean toggle_saved;
      gboolean type_resource_saved;

      object = g_object_get_data (G_OBJECT (widget), "subject_input");
      type_resource =
	g_object_get_data (G_OBJECT (widget), "type_resource_input");
      type_blanknode =
	g_object_get_data (G_OBJECT (widget), "type_blanknode_input");
      automatic =
	(gint) g_object_get_data (G_OBJECT (widget),
				  "type_blanknode_a_input");
      toggle = g_object_get_data (G_OBJECT (widget), "toggle_input");

      /* Salvo i dati precedenti: */
      object_saved = (gchar *) gtk_entry_get_text (GTK_ENTRY (object));
      if (object_saved)
	object_saved = g_strdup (object_saved);

      toggle_saved =
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));

      type_resource_saved =
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_resource));

      gtk_container_foreach (GTK_CONTAINER (widget_inputs),
			     (GtkCallback) template_widget_save_on, NULL);

      dialog = gtk_widget_get_toplevel (widget_inputs);
      gtk_widget_show_all (dialog);

      while ((ret = gtk_dialog_run (GTK_DIALOG (dialog))) == GTK_RESPONSE_OK)
	{

	  /* Se e' attiva la tipologia risorsa: */
	  if (gtk_toggle_button_get_active
	      (GTK_TOGGLE_BUTTON (type_resource)))
	    {
	      gchar *text;

	      text = (gchar *) gtk_entry_get_text (GTK_ENTRY (object));
	      if (!text || !*text)
		{
		  dialog_msg (_("No a compatible resource!"));
		  continue;
		}
	    }

	  /* Se e' invece e' attivo il blank node: */
	  else if (!automatic
		   &&
		   gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
						 (type_blanknode)))
	    {
	      gchar *text;

	      text = (gchar *) gtk_entry_get_text (GTK_ENTRY (object));
	      if (!text || !*text)
		{
		  if (dialog_ask
		      (_
		       ("No a compatible blank node. Do you want create a automatic name?"))
		      != GTK_RESPONSE_OK)
		    continue;
		  else
		    automatic = 1;
		}
	    }

	  /* Controllo TUTTI i widget di input: */
	  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)))
	    {
	      GList *old, *list;

	      list = old =
		gtk_container_get_children (GTK_CONTAINER (widget_inputs));

	      while (list)
		{
		  if (template_widget_check (list->data))
		    break;

		  list = list->next;
		}

	      /* Si lamenta la funzione widget_check, quindi io devo solo
	       * continuare: */
	      if (list)
		{
		  g_list_free (old);
		  continue;
		}

	      g_list_free (old);
	    }

	  break;
	}

      /* Analizzo la risposta: */
      switch (ret)
	{

	  /* Se e' OK: */
	case GTK_RESPONSE_OK:
	  gtk_container_foreach (GTK_CONTAINER (widget_inputs),
				 (GtkCallback) template_widget_save_off,
				 NULL);
	  if (object_saved)
	    g_free (object_saved);

	  gtk_widget_hide (dialog);
	  break;

	default:
	  /* Rimetto i dati com'erano prima se non si vuole salvarli: */
	  gtk_container_foreach (GTK_CONTAINER (widget_inputs),
				 (GtkCallback) template_widget_save_reset,
				 NULL);

	  gtk_entry_set_text (GTK_ENTRY (object),
			      object_saved ? object_saved : "");
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle),
					toggle_saved);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (type_resource),
					type_resource_saved);

	  if (object_saved)
	    g_free (object_saved);

	  gtk_widget_hide (dialog);
	  break;
	}

      return TRUE;
    }

  resources = g_object_get_data (G_OBJECT (widget), "resources");
  blanks = g_object_get_data (G_OBJECT (widget), "blanks");

  if (!(widget_inputs = template_open_real (resources, blanks, t)))
    return FALSE;

  dialog =
    template_run_create (widget_inputs, t, resources, blanks, &type_resource,
			 &type_blanknode, &object, &toggle);
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
				GTK_WINDOW (gtk_widget_get_toplevel
					    (widget)));
  gtk_widget_show_all (dialog);

  automatic = 0;
  while ((ret = gtk_dialog_run (GTK_DIALOG (dialog))) == GTK_RESPONSE_OK)
    {
      GList *list, *old;
      GList *invisible;

      /* Se e' attiva la tipologia risorsa: */
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (type_resource)))
	{
	  gchar *text;

	  text = (gchar *) gtk_entry_get_text (GTK_ENTRY (object));
	  if (!text || !*text)
	    {
	      dialog_msg (_("No a compatible resource!"));
	      continue;
	    }
	}

      /* Se e' invece e' attivo il blank node: */
      else if (!automatic
	       &&
	       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
					     (type_blanknode)))
	{
	  gchar *text;

	  text = (gchar *) gtk_entry_get_text (GTK_ENTRY (object));
	  if (!text || !*text)
	    {
	      if (dialog_ask
		  (_
		   ("No a compatible blank node. Do you want create a automatic name?"))
		  != GTK_RESPONSE_OK)
		continue;
	      else
		automatic = 1;
	    }
	}

      /* Controllo TUTTI i widget di input: */
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)))
	{
	  list = old =
	    gtk_container_get_children (GTK_CONTAINER (widget_inputs));

	  while (list)
	    {
	      if (template_widget_check (list->data))
		break;

	      list = list->next;
	    }

	  /* Si lamenta la funzione widget_check, quindi io devo solo
	   * continuare: */
	  if (list)
	    {
	      g_list_free (old);
	      continue;
	    }

	  g_list_free (old);
	}

      g_object_set_data (G_OBJECT (widget), "widget_inputs", widget_inputs);
      gtk_widget_hide (dialog);

      invisible = NULL;
      list = t->input;
      while (list)
	{
	  struct template_input_t *input = list->data;

	  if (!input->visible)
	    {
	      struct invisible_t *inv =
		g_malloc0 (sizeof (struct invisible_t));

	      inv->input = input;
	      invisible = g_list_append (invisible, inv);
	    }

	  list = list->next;
	}

      g_object_set_data (G_OBJECT (widget), "invisible_inputs", invisible);
      g_object_set_data (G_OBJECT (widget), "subject_input", object);
      g_object_set_data (G_OBJECT (widget), "type_resource_input",
			 type_resource);
      g_object_set_data (G_OBJECT (widget), "type_blanknode_input",
			 type_blanknode);
      g_object_set_data (G_OBJECT (widget), "type_blanknode_a_input",
			 (gpointer) automatic);
      g_object_set_data (G_OBJECT (widget), "toggle_input", toggle);
      break;
    }

  if (ret != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (dialog);
      return FALSE;
    }

  return TRUE;
}

/* TEMPLATE EDIT ************************************************************/
static void
template_edit_set (GList ** widget_inputs, GList ** rdf, gchar * subject)
{
  GList *list;
  GList *widget;

  /* TODO: per ogni widget, fare tripletta... */
  widget = *widget_inputs;
  while (widget)
    {

      /* Per ogni singola tripletta: */
      list = *rdf;
      while (list)
	{
	  struct rdf_t *r = list->data;

	  /* Se ha lo stesso soggetto richiesto: */
	  if (!strcmp (r->subject, subject))
	    {
	      /* Rimuovo per eventuali problemi di ricorsione: */
	      list = list->next;
	      *rdf = g_list_remove (*rdf, r);

	      /* Se coincide e viene assegnato: */
	      if (template_widget_is_your (widget->data, r, rdf) == TRUE)
		{
		  struct template_func_t *func;

		  if ((func =
		       g_object_get_data (G_OBJECT (widget->data), "func")))
		    {

		      /* Se e' una tipologia diversa da cardinality, rimuovo: */
		      if (func->type != TEMPLATE_WIDGET_CARDINALITY)
			*widget_inputs =
			  g_list_remove (*widget_inputs, widget->data);
		    }

		  template_edit_set (widget_inputs, rdf, subject);
		  return;
		}

	      *rdf = g_list_prepend (*rdf, r);
	      continue;
	    }

	  list = list->next;
	}

      widget = widget->next;
    }
}

static gint
template_edit_set_sort_func (GtkWidget * a, GtkWidget * b)
{
  gint mca = (gboolean) g_object_get_data (G_OBJECT (a), "min_cardinality");
  gint mcb = (gboolean) g_object_get_data (G_OBJECT (b), "min_cardinality");

  if (mca >= mcb)
    return 0;
  return 1;
}

static GList *
template_edit_set_sort (GList * widgets)
{
  GList *copy = g_list_copy (widgets);
  return g_list_sort (copy, (GCompareFunc) template_edit_set_sort_func);
}

static void
template_edit_subject_activated (GtkWidget * w, GtkTreePath * p,
				 GtkTreeViewColumn * c, GtkWidget * dialog)
{
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static gchar *
template_edit_subject (GList * list)
{
  GtkWidget *window, *hbox, *scrolledwindow, *treeview, *button, *frame;
  GtkListStore *model;
  GtkTreeIter iter;
  GtkCellRenderer *renderer;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  gchar s[1024];
  gchar *ret = NULL;
  GList *subjects = NULL, *tmp;

  while (list)
    {
      struct rdf_t *rdf = list->data;

      for (tmp = subjects; tmp; tmp = tmp->next)
	if (!strcmp (rdf->subject, tmp->data))
	  break;

      if (!tmp)
	subjects = g_list_append (subjects, rdf->subject);

      list = list->next;
    }

  if (g_list_length (subjects) == 1)
    {
      ret = g_strdup (subjects->data);
      g_list_free (subjects);
      return ret;
    }

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Template edit..."));

  window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), s);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), main_window ());
  gtk_widget_set_size_request (window, 400, 200);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), hbox, TRUE, TRUE,
		      0);

  frame = gtk_frame_new (_("Select the Subject"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);
  g_signal_connect ((gpointer) treeview, "row_activated",
		    G_CALLBACK (template_edit_subject_activated), window);


  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Subject"), renderer, "text",
					      0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  button = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button,
				GTK_RESPONSE_CANCEL);
  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (window), button, GTK_RESPONSE_OK);
  gtk_widget_set_can_default(button, TRUE);

  tmp = subjects;
  while (tmp)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0,
			  strcmp (tmp->data,
				  THIS_DOCUMENT) ? tmp->
			  data : _("This Document"), 1, tmp->data, -1);
      tmp = tmp->next;
    }

  g_list_free (subjects);

  gtk_widget_show_all (window);

  while ((gtk_dialog_run (GTK_DIALOG (window))) == GTK_RESPONSE_OK)
    {
      if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
	  dialog_msg (_("Select a Subject!"));
	  continue;
	}

      gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &ret, -1);
      break;
    }

  g_object_unref (model);
  gtk_widget_destroy (window);
  return ret;
}

/* Questa funzione controlla l'esistenza delle triplette invisibli: */
static void
template_edit_invisible (GList ** invisible, GList ** rdf, gchar * subject,
			 struct template_t *t)
{
  GList *list = t->input;
  struct invisible_t *inv;

  /* Per ogni input: */
  while (list)
    {
      struct template_input_t *input = list->data;

      /* Se e' un invisibile: */
      if (!input->visible)
	{

	  /* Controllo se esiste all'interno dell'elenco: */
	  GList *tmp = *rdf;
	  while (tmp)
	    {
	      struct rdf_t *r = tmp->data;

	      /* Se lo trovo creo un input invisibile con la tripletta
	       * e la rimuovo dall'elenco: */
	      if (!strcmp (r->subject, subject)
		  && !strcmp (r->predicate, input->rdfs)
		  && !strcmp (r->object, input->default_value))
		{
		  inv = g_malloc0 (sizeof (struct invisible_t));
		  inv->input = input;
		  inv->rdf = r;

		  *invisible = g_list_append (*invisible, inv);
		  *rdf = g_list_remove (*rdf, r);
		  break;
		}

	      tmp = tmp->next;
	    }

	  /* Se non lo trovo, lo metto come nuovo da mettere: */
	  if (!tmp)
	    {
	      inv = g_malloc0 (sizeof (struct invisible_t));
	      inv->input = input;
	      inv->rdf = NULL;
	      *invisible = g_list_append (*invisible, inv);
	    }
	}

      list = list->next;
    }
}

static void
template_edit_dialog (GList * rdf, gint len)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *scrolledwindow;
  GtkWidget *treeview;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *frame;
  GtkListStore *model;
  GtkTreeIter iter;
  gchar s[1024];
  gint i;

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("Information..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_widget_set_size_request (dialog, 400, 200);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE,
		      0);

  image = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  i = g_list_length (rdf);
  g_snprintf (s, sizeof (s), _("Set %d of %d RDF triples"), len - i, len);

  label = gtk_label_new (s);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  frame = gtk_frame_new (_("RDF triples ignored"));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, TRUE, TRUE,
		      0);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow),
				       GTK_SHADOW_IN);

  model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (treeview), TRUE);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), treeview);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Subject"), renderer, "text",
					      0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Predicate"), renderer,
					      "text", 1, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Object"), renderer, "text",
					      2, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_column_set_resizable (column, TRUE);

  while (rdf)
    {
      struct rdf_t *r = rdf->data;

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, r->subject, 1, r->predicate, 2,
			  r->object, -1);

      rdf = rdf->next;
    }

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

void
template_edit (GtkWidget * w, struct template_t *t)
{
  struct editor_data_t *data = editor_get_data ();
  GtkWidget *dialog;
  GtkWidget *vbox, *hbox, *gbox;
  GtkWidget *image;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *subject;
  GtkWidget *sw;
  GtkWidget *button_subject;
  GtkWidget *button_subject_this;
  GtkWidget *widget_inputs;
  GList *list, *tmp, *invisible;
  GSList *l;
  GList *resources;
  GList *blanks;
  gchar s[1024];
  gchar *s_str;
  gint len;

  if (!data || !data->rdf)
    {
      dialog_msg (_("This document is empty!"));
      return;
    }

  /* Rimuovo i template remoti chiesti precedentemente: */
  template_remote_init ();

  if (!(s_str = template_edit_subject (data->rdf)))
    return;

  resources = create_resources (data);
  resources = create_resources_by_rdfs (resources);
  blanks = create_blank (data);

  if (!(widget_inputs = template_open_real (resources, blanks, t)))
    {
      g_list_free (resources);
      g_list_free (blanks);
      g_free (s_str);
      return;
    }

  /* Creo una copia della lista perche' queste vado pezzo per pezzo a
   * rimuoverle: */
  for (tmp = NULL, list = data->rdf; list; list = list->next)
    tmp = g_list_append (tmp, list->data);

  len = g_list_length (tmp);

  /* Questa funzione imposta i valori delle varie triplette all'interno
   * dei widget */
  if ((list = gtk_container_get_children (GTK_CONTAINER (widget_inputs))))
    {
      GList *sorted = template_edit_set_sort (list);
      g_list_free (list);

      template_edit_set (&sorted, &tmp, s_str);

      if (sorted)
	g_list_free (sorted);
    }

  /* Rimuovo gli elementi invisibili: */
  invisible = NULL;
  template_edit_invisible (&invisible, &tmp, s_str, t);

  template_edit_dialog (tmp, len);
  g_list_free (tmp);

  dialog = gtk_dialog_new ();

  g_snprintf (s, sizeof (s), "%s %s - %s: %s %s", PACKAGE, VERSION,
	      _("Template"), t->name, t->version);
  gtk_window_set_title (GTK_WINDOW (dialog), s);

  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

  label = gtk_label_new (_("<b>Subject</b>"));
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), frame, FALSE,
		      FALSE, 5);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  gbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gbox, FALSE, FALSE, 0);

  l = NULL;
  button = gtk_radio_button_new_with_label (NULL, _("Resource: "));
  button_subject = button;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_box_pack_start (GTK_BOX (gbox), button, FALSE, FALSE, 3);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (gbox), hbox, TRUE, TRUE, 5);

  /* Creazione soggetto con freccia: */
  subject = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), subject, TRUE, TRUE, 0);

  button = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_object_set_data (G_OBJECT (button), "list", resources);
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (search_resources), subject);

  image = gtk_image_new_from_stock ("gtk-find", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  gbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gbox, FALSE, FALSE, 0);

  button = gtk_radio_button_new_with_label (NULL, _("This document"));
  button_subject_this = button;
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (button), l);
  l = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
  gtk_box_pack_start (GTK_BOX (gbox), button, TRUE, TRUE, 3);

  if (!strcmp (s_str, THIS_DOCUMENT))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  else
    gtk_entry_set_text (GTK_ENTRY (subject), s_str);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), sw, TRUE, TRUE, 5);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
					 widget_inputs);

  button = gtk_button_new_from_stock ("gtk-no");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
				GTK_RESPONSE_CLOSE);

  gtk_widget_set_can_default(button, TRUE);

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default(button, TRUE);

  template_set_size (dialog, sw, widget_inputs);
  gtk_widget_show_all (dialog);

  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      GList *old;
      gchar *text;
      struct unredo_t *unredo;

      /* Check del soggetto: */
      if (gtk_toggle_button_get_active
	  (GTK_TOGGLE_BUTTON (button_subject_this)) == TRUE)
	text = THIS_DOCUMENT;
      else
	text = (gchar *) gtk_entry_get_text (GTK_ENTRY (subject));

      if (!text || !*text)
	{
	  dialog_msg (_("Invalid subject!"));
	  continue;
	}

      text = g_strdup (text);

      /* Controllo TUTTI i widget di input: */
      list = old = gtk_container_get_children (GTK_CONTAINER (widget_inputs));

      while (list)
	{
	  if (template_widget_check (list->data))
	    break;

	  list = list->next;
	}

      /* Si lamenta la funzione widget_check, quindi io devo solo
       * continuare: */
      if (list)
	{
	  g_free (text);
	  g_list_free (old);
	  continue;
	}

      /* A questo punto inserisco, quindi undo: */
      unredo = unredo_new (data, UNREDO_TYPE_CHANGE);

      list = old;
      while (list)
	{
	  /* Funzione che fa l'inserimento: */
	  template_widget_save (list->data, data, text, unredo);
	  list = list->next;
	}

      /* Tutto quello che riguarda le triplette invisibili: */
      if (invisible)
	{
	  struct invisible_t *inv;

	  /* Per ogni input invisible: */
	  list = invisible;
	  while (list)
	    {
	      inv = list->data;
	      template_invisible_save (text, inv->input, data, unredo,
				       inv->rdf);

	      /* Libero memoria: */
	      g_free (inv);
	      list = list->next;
	    }

	  /* Distruggo la lista degli invisibli: */
	  g_list_free (invisible);
	}

      g_list_foreach (old, (GFunc) template_widget_destroy, NULL);
      g_list_free (old);

      g_free (text);
      data->changed = TRUE;

      /* Refreshio i namespace e li rigenero */
      g_list_foreach (data->namespace, (GFunc) namespace_free, NULL);
      g_list_free (data->namespace);

      namespace_parse (data->rdf, &data->namespace);

      editor_refresh (data);
      break;
    }

  /* Libero memoria: */
  g_list_free (resources);
  g_list_free (blanks);
  g_free (s_str);

  gtk_widget_destroy (dialog);
}

/* JAVASCRIPT ****************************************************************/
#ifdef USE_JS
static gboolean
template_js_evaluate (struct js_t *js, GtkWidget * widget, gchar * func)
{
  js_current_widget (js, widget);
  return js_evaluate (js, func);
}
#endif

/* VALUE FUNCTIONS ***********************************************************/

struct template_value_t *
template_value_new (GtkWidget * widget)
{
  struct template_func_t *func;
  struct template_value_t *value;

  value = g_malloc0 (sizeof (struct template_value_t));

  func = g_object_get_data (G_OBJECT (widget), "func");
  if (func && func->get)
    func->get (widget, value);

  return value;
}

void
template_value_set (GtkWidget * w, struct template_value_t *value)
{
  template_widget_save_off (w);
  template_widget_set (w, value);
}

void
template_value_free (struct template_value_t *saved)
{
  if (!saved)
    return;

  if (saved->value)
    g_free (saved->value);

  if (saved->lang)
    g_free (saved->lang);

  if (saved->datatype)
    g_free (saved->datatype);

  g_free (saved);
}

/* EOF ?!? */

/*
 * Il lonfo - Fosco Maraini
 *
 * Il lonfo non vaterca né gluisce
 * e molto raramente barigatta,
 * ma quando soffia il bego a bisce bisce
 * sdilenca un poco, e gnagio s'archipatta.
 * 
 * E' frusco il lonfo! E' pieno di lupigna
 * arrafferia malversa e sofolenta!
 * 
 * Se cionfi ti sbiduglia e t'arrupigna
 * se lugri ti botalla e ti criventa.
 * 
 * Eppure il vecchio lonfo ammargelluto
 * che bete e zugghia e fonca nei trombazzi
 * fa lègica busìa, fa gisbuto;
 * e quasi quasi in segno di sberdazzi
 * gli affarferesti un gniffo. Ma lui zuto
 * t'alloppa, ti sbernecchia; e tu l'accazzi.
 * 
 */

/* EOF */

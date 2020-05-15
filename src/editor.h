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

#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <nxml.h>

#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <redland.h>

#ifdef USE_GTKHTML
#  include <libgtkhtml/gtkhtml.h>
#endif

#ifdef ENABLE_NLS
#  include <glib/gi18n-lib.h>
#else
#  define _(String) (String)
#endif

#ifdef G_OS_UNIX
#  include <sys/utsname.h>
#endif

#ifdef USE_JS
#  ifdef USE_NGS_JS
#    include <js.h>
#  elif USE_MOZILLA_JS
#    include <jsapi.h>
#  else
#    error "No javascript engine selected!"
#  endif
#endif

#ifdef USE_GCONF
#  include <gconf/gconf-client.h>
#endif

#include "morla-module.h"

struct module_t
{
  GModule *module;
  MorlaModule *data;
};

enum rdf_object_t
{
  RDF_OBJECT_BLANK,
  RDF_OBJECT_LITERAL,
  RDF_OBJECT_RESOURCE
};

enum order_t
{
  ORDER_SUBJECT,
  ORDER_SUBJECT_DEC,
  ORDER_PREDICATE,
  ORDER_PREDICATE_DEC,
  ORDER_OBJECT,
  ORDER_OBJECT_DEC
};

enum nsorder_t
{
  NSORDER_NAMESPACE,
  NSORDER_NAMESPACE_DEC,
  NSORDER_PREFIX,
  NSORDER_PREFIX_DEC,
  NSORDER_VISIBLE,
  NSORDER_VISIBLE_DEC
};

struct namespace_t
{
  gchar *namespace;
  GtkWidget *namespace_widget;

  gchar *prefix;
  GtkWidget *prefix_widget;

  gboolean visible;
  GtkWidget *visible_widget;
};

enum rdf_format_t
{
  RDF_FORMAT_XML = 0,
  RDF_FORMAT_NTRIPLES,
  RDF_FORMAT_TURTLE
};

struct rdf_t
{
  gchar *subject;
  GtkWidget *subject_widget;

  gchar *predicate;
  GtkWidget *predicate_widget;

  gchar *object;
  GtkWidget *object_widget;

  enum rdf_object_t object_type;
  GtkWidget *object_type_widget;

  gchar *lang;
  GtkWidget *lang_widget;

  gchar *datatype;
  GtkWidget *datatype_widget;
};

struct rdfs_t
{
  gchar *resource;
  gchar *prefix;
  gchar *path;

  GList *data;
  GList *nsdata;
};

struct thread_t
{
  GFunc func;
  gpointer data;
  gint *flag;
};

struct graph_data_t
{
  gchar *color;
  gchar *fontcolor;
  gchar *style;
  gchar *fillcolor;
  gchar *shape;
  gchar *fontname;
  gint fontsize;
};

enum unredo_type_t
{
  UNREDO_TYPE_ADD,
  UNREDO_TYPE_DEL,
  UNREDO_TYPE_CHANGE
};

struct unredo_change_t
{
  struct rdf_t *now;
  struct rdf_t *prev;
};

struct unredo_t
{
  enum unredo_type_t type;
  GList *list;
};

struct info_box_t
{
  gchar *value;
  gchar *lang;
};

struct editor_data_t
{
  struct editor_data_t *next;

  gchar *file;
  gchar *uri;
  struct module_t *module_save;

  enum rdf_format_t format;

  GList *rdf;
  GList *namespace;

  GList *undo;
  GList *redo;

  gboolean changed;

  GtkWidget *label;
  GtkWidget *maker;

  GtkWidget *list;
  GtkWidget *nslist;

  GtkWidget *subject_order;
  GtkWidget *predicate_order;
  GtkWidget *object_order;
  enum order_t compare;

  GtkWidget *namespace_order;
  GtkWidget *prefix_order;
  GtkWidget *visible_order;
  enum nsorder_t nscompare;

  gchar *node_select;
  gchar *node_nsselect;

  struct rdf_t *rdf_select;
};

struct template_t
{
  gchar *uri;
  gchar *name;
  gchar *version;
  gchar *category;
  GList *input;
};

enum template_input_typevalue_t
{
  TEMPLATE_INPUT_TYPEVALUE_NONE,
  TEMPLATE_INPUT_TYPEVALUE_RESOURCE,
  TEMPLATE_INPUT_TYPEVALUE_LITERAL,
  TEMPLATE_INPUT_TYPEVALUE_ALT_RESOURCE,
  TEMPLATE_INPUT_TYPEVALUE_ALT_LITERAL,
  TEMPLATE_INPUT_TYPEVALUE_OTHER
};

struct template_input_t
{
  GList *labels;
  GList *comments;

  gchar *rdfs;

  gboolean lang;
  gboolean datatype;

  gint min_cardinality;
  gint max_cardinality;

  enum template_input_typevalue_t typevalue;
  GList *alt_typevalue;
  gchar *other_typevalue;

  gboolean visible;
  gchar *default_value;
  gchar *default_language;
  gchar *default_datatype;

  gchar *function_init;
  gchar *function_check;
  gchar *function_destroy;
  gchar *function_save;

  gboolean longtext;
};

struct template_value_t
{
  gchar *value;
  gchar *lang;
  gchar *datatype;
  enum rdf_object_t type;
};

struct update_template_t
{
  gchar *uri;
  gchar *name;
  gchar *description;
  gchar *author;
  gchar *version;
};

struct update_t
{
  gchar *last_version;
  GList *templates;
};

enum parser_char_t
{
  PARSER_CHAR_INIT = '*',
  PARSER_CHAR_ERROR = '!',
  PARSER_CHAR_WARNING = '_'
};

struct datatype_t {
  gchar *uri;
  gchar *name;
  gchar *description;
  gboolean (*check)(gchar *, gchar *);
};

struct download_t {
  gint timeout;

  gboolean verifypeer;
  gchar *certfile;
  gchar *certpassword;
  gchar *cacertfile;

  gchar *auth_user;
  gchar *auth_password;
};

struct tag_t
{
  gchar *tag;

  PangoStyle style;
  PangoVariant variant;
  PangoWeight weight;
  PangoStretch stretch;
  gdouble scale;
  gchar *foreground;
  gchar *background;
  PangoUnderline underline;
};

#ifdef USE_JS

#  define JS_MORLA_OK		0
#  define JS_MORLA_FAILURE	1

struct js_t
{
#  ifdef USE_NGS_JS
  JSInterpPtr interp;
#  elif USE_MOZILLA_JS
  JSRuntime *rt;
  JSContext *cx; 
  JSObject *global;
#  endif
  GtkWidget *widget;
};

#endif

#define LOCK	1
#define WAIT	2
#define UNLOCK	3

#define MORLA_COPYRIGHT	"Copyright 2005-2007 - GPL - Andrea Marchesini"

#define THIS_DOCUMENT	"this:document"
#define BLANK_NODE	"blank_node:"

#define RDF_XSD		"http://www.w3.org/2001/XMLSchema#"

#define RDF_TYPE	"http://www.w3.org/1999/02/22-rdf-syntax-ns#type"
#define RDF_ITEM	"http://www.w3.org/1999/02/22-rdf-syntax-ns#_"

#define RDF_PROPERTY	"http://www.w3.org/1999/02/22-rdf-syntax-ns#Property"
#define RDFS_LITERAL	"http://www.w3.org/2000/01/rdf-schema#Literal"
#define RDFS_RESOURCE	"http://www.w3.org/2000/01/rdf-schema#Resource"

#define RDFS_COMMENT	"http://www.w3.org/2000/01/rdf-schema#comment"
#define RDFS_LABEL	"http://www.w3.org/2000/01/rdf-schema#label"

#define RDFS_DOMAIN	"http://www.w3.org/2000/01/rdf-schema#domain"
#define RDFS_RANGE	"http://www.w3.org/2000/01/rdf-schema#range"

#define RDF_ALT		"http://www.w3.org/1999/02/22-rdf-syntax-ns#Alt"
#define RDF_BAG		"http://www.w3.org/1999/02/22-rdf-syntax-ns#Bag"
#define RDF_SEQ		"http://www.w3.org/1999/02/22-rdf-syntax-ns#Seq"

#define OWL_O_PROPERTY	"http://www.w3.org/2002/07/owl#ObjectProperty"
#define OWL_DT_PROPERTY	"http://www.w3.org/2002/07/owl#DatatypeProperty"

#define RDF_SUBCLASSOF	"http://www.w3.org/2000/01/rdf-schema#subClassOf"
#define RDF_SUBPROPERTYOF "http://www.w3.org/2000/01/rdf-schema#subPropertyOf"

#define MORLA_NS	"http://www.morlardf.net/morla#"

#define MORLA_TEMPLATE	"Template"
#define MORLA_INPUT	"Input"

#define MORLA_HASINPUT	      "hasInput"
#define MORLA_HASVERSION      "hasVersion"
#define MORLA_CATEGORY	      "category"
#define MORLA_HASRDFS         "hasRDFS"
#define MORLA_TYPEVALUE	      "typeValue"
#define MORLA_MINCARD	      "minCardinality"
#define MORLA_MAXCARD	      "maxCardinality"
#define MORLA_VISIBLE	      "visible"
#define MORLA_LANGUAGE        "language"
#define MORLA_DEFAULTLANGUAGE "defaultLanguage"
#define MORLA_DATATYPE	      "datatype"
#define MORLA_DEFAULTDATATYPE "defaultDatatype"
#define MORLA_DEFAULTVALUE    "defaultValue"
#define MORLA_LONGTEXT        "longText"

#define MORLA_FUNCTIONINIT    "functionInit"
#define MORLA_FUNCTIONCHECK   "functionCheck"
#define MORLA_FUNCTIONDESTROY "functionDestroy"
#define MORLA_FUNCTIONSAVE    "functionSave"

#define MORLA_ALT_RESOURCE "AltResource"
#define MORLA_ALT_LITERAL  "AltLiteral"

#define MORLA_WEBSITE		"http://www.morlardf.net/"
#define MORLA_OLD_WEBSITE_1	"http://www.autistici.org/bakunin/morla/"
#define MORLA_OLD_WEBSITE_2	"http://autistici.org/bakunin/morla/"
#define MORLA_OLD_WEBSITE_3	"http://www2.autistici.org/bakunin/morla/"

#define UPDATE_URL	"http://www.morlardf.net/update"
#define DOAP_URL	"http://www.morlardf.net/doap.rdf"

#ifdef USE_GTKHTML
#  define HELP_URL	"http://www.morlardf.net/doc/"
#else
#  define HELP_URL	"http://morlardf.net/doc.php"
#endif

#define UNDO_MAX_VALUE		10
#define LAST_DOCUMENT_VALUE	5

extern int editor_statusbar_lock;

extern struct editor_data_t *editor_data;
extern GtkWidget *notebook;

extern gchar *browser_default;
extern gchar *viewer_default;

extern gint default_width;
extern gint default_height;

extern GList *thread_child;
extern GList *rdfs_list;
extern gint last_id;

extern struct graph_data_t graph_blank_node;
extern struct graph_data_t graph_resource_node;
extern struct graph_data_t graph_literal_node;
extern struct graph_data_t graph_edge;
extern gchar *graph_format;

extern gchar *normal_bg_color;
extern gchar *prelight_bg_color;
extern gchar *normal_fg_color;
extern gchar *prelight_fg_color;

extern GList *template_list;

extern GList *template_menus;
extern GtkWidget *template_toolbar;

extern GList *last_list;
extern GtkWidget *last_menu;
extern gint last_document_value;

extern GtkWidget *undo_widget[2];
extern GtkWidget *redo_widget[2];
extern gint undo_max_value;

extern gint update_show;
extern gchar *update_url;

extern gchar *blanknode_name;

extern gchar *help_url;

extern gchar *download_proxy;

#ifdef USE_GCONF
extern gboolean download_proxy_gconf;
#endif

extern gint download_proxy_port;
extern gchar *download_proxy_user;
extern gchar *download_proxy_password;

extern gboolean automatic_extensions;

extern GList *highlighting_tags;
extern gboolean highlighting_active;

/* edit.c */
void		editor_add		(GtkWidget *	w,
					 gpointer	data);

void		editor_cut		(GtkWidget *	w,
					 gpointer	data);

void		editor_copy		(GtkWidget *	w,
					 gpointer	data);

void		editor_paste		(GtkWidget *	w,
					 gpointer	data);

void		editor_delete		(GtkWidget *	w,
					 gpointer	data);

void		editor_cut_triple	(GtkWidget *	w,
					 gpointer	data);

void		editor_copy_triple	(GtkWidget *	w,
					 gpointer	data);

void		editor_delete_triple	(GtkWidget *	w,
					 gpointer	data);

struct rdf_t *	editor_triple		(struct rdf_t *	prev,
					 gchar *	title);

void		editor_merge_file	(GtkWidget *	w,
					 gpointer	data);

void		editor_merge_uri	(GtkWidget *	w,
					 gpointer	data);

/* file.c */
gboolean	editor_open_file	(gchar *		f,
					 gchar *		ff);

gboolean	editor_open_uri		(gchar *		f,
					 struct download_t *	d);

void		editor_open		(GtkWidget *		w,
					 gpointer		data);

void		editor_save_as		(GtkWidget *		w,
					 gpointer		data);

void		editor_save		(GtkWidget *		w,
					 gpointer		data);

void		editor_close		(GtkWidget *		w,
					 gpointer		data);

void		editor_close_selected	(GtkWidget *		w,
					 struct editor_data_t *	data);

void		editor_new		(GtkWidget *		w,
	       				 gpointer		data);

gchar *		editor_tmp_file		(void);

/* main.c */
int		editor_quit	(GtkWidget *		w,
				 GdkEvent *		event,
				 gpointer		data);

gchar *		utf8		(gchar *		str);

char *		markup 		(gchar *		str);

void		statusbar_set	(gchar *		what,
				 ...);

void		fatal		(gchar *		text,
				 ...);

void		run		(gchar **		a,
				 GChildWatchFunc	func,
				 gpointer		data);

GList *		split		(gchar *		ar);

gchar *		user_agent	(void);

gchar *		dot_cmd		(void);

GtkWindow *	main_window	(void);

/* maker.c */
struct editor_data_t *	editor_get_data		(void);

gint			editor_timeout		(gpointer data);

void			editor_new_maker	(struct editor_data_t *	data);

void			editor_refresh		(struct editor_data_t *	data);

void			editor_refresh_nslist	(struct editor_data_t *	data);

gint			editor_compare		(struct rdf_t *		rdf,
						 struct rdf_t *		what,
						 struct editor_data_t *	data);

gint			editor_nscompare	(struct namespace_t *	ns,
						 struct namespace_t *	ns2,
						 struct editor_data_t *	data);

/* edit.c */
void	editor_view			(GtkWidget *		w,
					 gpointer		data);

void	editor_your_view		(GtkWidget *		w,
					 gpointer		data);

void	editor_preferences		(GtkWidget *		w,
					 gpointer		data);

GList *	create_no_dup			(GList *		ret);

GList *	create_resources		(struct editor_data_t *	data);

GList *	create_resources_by_rdfs	(GList *		ret);

GList *	create_blank			(struct editor_data_t *	data);

void	search_resources		(GtkWidget *		widget,
					 GtkWidget *		entry);

gchar *	search_list			(GList *		list,
					 GtkWindow *		parent);

/* dialog.c */
void		dialog_msg		(gchar *	msg);

void		dialog_msg_with_error	(gchar *	msg,
					 gchar *	other);

int		dialog_ask		(gchar *	msg);

int		dialog_ask_with_cancel	(gchar *	msg);

GtkWidget *	dialog_wait		(GCallback	callback,
					 gpointer	data,
					 GtkWidget **	pb);

/* filechooser.c */
void *	file_chooser	(gchar *		str,
	       		 GtkSelectionMode	mode,
			 GtkFileChooserAction	action,
			 enum rdf_format_t *	rdf);

gchar *	uri_chooser	(gchar *		str,
			 struct download_t **	options);

/* rdf.c */
librdf_world *	rdf_world		(void);
gboolean	rdf_parse		(gchar *		what,
					 struct download_t *	download,
					 gboolean		this_document,
					 GList **		list,
					 enum rdf_format_t *	format,
					 GString **		string);

gboolean	rdf_parse_thread	(gchar *		resource,
					 struct download_t *	download,
					 GList **		list,
					 GList **		nslist,
					 gboolean		this_document,
					 enum rdf_format_t *	format,
					 GString **		string);

GList *		rdf_parse_stream	(librdf_stream *	stream);

gboolean	rdf_parse_format	(gchar *		resource,
					 gchar *		buffer,
					 gboolean		this_document,
					 GList **		ret,
					 enum rdf_format_t	format,
					 GString **		string);

gboolean	rdf_save_file		(gchar *		file,
					 GList *		rdf,
					 GList **		ns,
					 enum rdf_format_t	format);

gboolean	rdf_save_buffer		(GList *		rdf,
					 GList **		ns,
					 enum rdf_format_t	format,
					 gchar **		buffer_ret);

gboolean	rdf_is_no_saved		(struct editor_data_t *	data);

void		rdf_free		(struct rdf_t *		rdf);

struct rdf_t *	rdf_copy		(struct rdf_t *		rdf);

gboolean	rdf_blank		(GList *		rdf,
					 gchar *		t);

gboolean	rdf_resource		(GList *		rdf,
					 gchar *		t);

int		rdf_error		(void *			dummy,
					 const char *		msg,
					 va_list va);

int		rdf_warning		(void *			dummy,
					 const char *		msg,
					 va_list		va);

gint		rdf_log			(GString **		error,
					 gchar *		str,
					 enum parser_char_t	c);

/* namespace.c */
gboolean	namespace_parse		(GList *		rdf,
					 GList **		list);
void		namespace_free		(struct namespace_t *	ns);
gboolean	namespace_visible	(GList *		ns,
					 struct rdf_t *		rdf);
gchar *		namespace_prefix	(GList **		ns,
					 gchar *		t);
gchar *		namespace_create	(GList *		ns,
					 gchar *		t);

/* browser.c */
void	browser		(gchar *	what);

void	browser_website	(void);

/* init.c */
void	init			(gchar *	m_config);

GList *	init_highlighting	(void);

void	init_save		(void);

/* rdfs.c */
void	rdfs		(GtkWidget *	w,
			 gpointer	dummy);

gint	rdfs_add	(gchar *	resource,
			 gchar *	prefix);

gchar *	rdfs_file	(void);

/* graph.c */
void	graph_run	(GtkWidget *		w,
			 gpointer		dummy);

void	graph		(struct editor_data_t *	data);

void	graph_save	(GtkWidget *		w,
			 gpointer		dummy);

void	graph_graphviz	(void);

/* about.c */
void	about		(GtkWidget *	w,
		 	gpointer	dummy);

void	about_libraries	(void);

/* template.c */
void		template_update		(void);

void		template_import_uri	(GtkWidget *		w,
					 gpointer		dummy);

void		template_import_file	(GtkWidget *		w,
					 gpointer		dummy);

void		template		(GtkWidget *		w,
					 gpointer		dummy);

gboolean	template_import		(gchar *		uri,
					 struct download_t *	download,
					 gboolean		b1,
					 gboolean		b2);

void		template_save		(void);

void		template_save_single	(GtkWidget *		w,
					 struct template_t *	t);

void		template_open		(GtkWidget *		w,
					 struct template_t *	t);

void		template_edit		(GtkWidget *		w,
					 struct template_t *	t);

struct template_value_t *
		template_value_new	(GtkWidget *		w);

void		template_value_set	(GtkWidget *		w,
					 struct template_value_t *
								value);

void		template_value_free	(struct template_value_t *
								value);

/* preferences.c */
void	preferences	(void);

/* splash.c */
void		splash_show			(void);

void		splash_hide			(void);

gboolean	splash_showed			(void);

void		splash_set_text			(char *		str,
						 ...);

void		splash_set_text_with_effect	(char *		str,
						 ...);

void		splash_sleep			(int		sleep);

void		splash_close_on_click		(gboolean	value);

/* unredo.c */
struct unredo_t *	unredo_new		(struct editor_data_t *	data,
						 enum unredo_type_t	type);

void			unredo_add		(struct unredo_t *	unredo,
	       					 struct rdf_t *		rdf,
						 struct rdf_t *		rdf2);

void			unredo_destroy		(struct editor_data_t *	data,
						 struct unredo_t *	unredo);

void			unredo_undo		(void);

void			unredo_redo		(void);

void			unredo_change_value	(void);

/* update.c */
gint	update_timer	(void);

void	update_search	(void);

gint	update_check	(void);

/* merge.c */
GList *	merge		(GList *		rdf,
			 gchar *		what,
			 struct download_t *	download,
			 GString **		string);

void	merge_graph	(void);

/* navigator.c */
void	navigator_run	(void);

void	navigator_open_uri	(void);

/* search.c */
gboolean	search		(gchar *	uri,
				 GList **	meta,
				 GList **	other,
				 gchar **	error);

void		search_open	(gchar *	text,
				 void		(*func) (gchar *, gpointer),
				 gpointer	data);

/* last.c */
void	last_update		(void);

void	last_add		(gchar *	uri);

void	last_change_value	(void);

/* help.c */
void	help	(void);

/* info.c */
void		info_selected		(void);

void		info_uri		(void);

void		info_this		(void);

gboolean	info_box_popup_show	(GtkWidget *		w,
					 GdkEventButton *	event,
					 GList *		list);

gboolean	info_box_popup_hide	(GtkWidget *		w,
					 GdkEventButton *	event,
					 gpointer		data);

gboolean	info_box_enter		(GtkWidget *		w,
					 GdkEventCrossing *	event,
					 gpointer		data);

gboolean	info_box_leave		(GtkWidget *		w,
					 GdkEventCrossing *	event,
					 gpointer		data);

void		info_box_free		(struct info_box_t *	info);

/* query.c */
void	query_sparql	(void);
void	query_rsql	(void);

/* checks.c */
void	check_rasqal	(void);
void	check_redland	(void);

/* datatype.c */
struct datatype_t *	datatype_list	(void);

gboolean		datatype_check	(gint		id,
					 gchar *	what,
					 gchar *	check);

void		datatype_change		(GtkWidget *	w,
					 GtkWidget *	label);

gint		datatype_id		(gchar *	what);

gboolean	datatype_exists		(gchar *	what);

/* download.c */
gboolean		download_uri	(gchar *		uri,
					 gchar **		buffer,
					 struct download_t *	download,
					 gchar **		error);

void			download_free	(struct download_t *	donwload);

struct download_t *	download_copy	(struct download_t *	download);

/* ntriples.c */
gchar *	ntriples	(librdf_node *	node);

/* blanknode.c */
gboolean	blanknode_get		(gchar *	what,
					 gint *		id);

gchar *		blanknode_create	(gint		id);

/* textview.c */
GtkWidget *	textview_new			(GCallback	change,
						 gpointer	data);

void		textview_add_tag		(GtkWidget *	text,
						 gchar *	tag,
						 PangoStyle	style,
						 PangoVariant	variant,
						 PangoWeight	weight,
						 PangoStretch	stretch,
						 gdouble	scale,
						 gchar *	foreground,
						 gchar *	background,
						 PangoUnderline	underline);

void		textview_insert			(GtkWidget *	widget,
						 GtkTextIter *	iter,
						 gchar *	text,
						 gint		len);

void		textview_insert_at_cursor	(GtkWidget *	widget,
						 gchar *	text,
						 gint		len);

void		textview_clean_tags		(GtkWidget *	widget);

/* js.c */
struct js_t *	js_new			(void);

void		js_destroy		(struct js_t *	js);

void		js_current_widget	(struct js_t *	js,
					 GtkWidget *	widget);

gboolean	js_evaluate		(struct js_t *	js,
					 gchar *	buffer);

/* module.c */
void		module_init		(gchar *	config);

void		module_shutdown		(void);

void		module_menu_open	(GtkMenu *	menu,
					 GtkWidget *	parent);

void		module_menu_save	(GtkMenu *	menu,
					 GtkWidget *	parent);

void		module_menu_view	(GtkMenu *	menu,
					 GtkWidget *	parent);

void		module_run_open		(GtkWidget *	widget,
					 struct module_t *
					 		module);

void		module_run_save		(GtkWidget *	widget,
					 struct module_t *
					 		module);

struct module_t *
		module_default_open	(void);

struct module_t *
		module_default_save	(void);

/* gconf.c */
void		morla_gconf_init	(void);

gboolean	morla_gconf_proxy	(gchar **	proxy,
					 gint *		port,
					 gchar **	user,
					 gchar **	password,
					 GSList **	list);

void		morla_gconf_shutdown	(void);

/* EOF */

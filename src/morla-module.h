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


typedef struct morla_module_t	MorlaModule;

/* Every GList must contain MorlaRdf data structs */
typedef struct morla_rdf_t	MorlaRdf;
typedef enum morla_rdf_object_t	MorlaRdfObject;

typedef MorlaModule *	(*MorlaModuleInit)	(void);

typedef void		(*MorlaModuleShutdown)	(MorlaModule *	data);

typedef gboolean	(*MorlaOpen)		(MorlaModule *	data,
						 GList **	rdf,
						 gchar **	uri);

typedef gboolean	(*MorlaSave)		(MorlaModule *	data,
						 GList *	rdf);

typedef gboolean	(*MorlaView)		(MorlaModule *	data,
						 GList *	rdf);

struct morla_module_t
{
  gchar *	module_name;

  MorlaOpen	open_function;
  gchar *	open_label;

  MorlaSave	save_function;
  gchar *	save_label;

  MorlaView	view_function;
  gchar *	view_label;

  /* This pointer is usable as you want: */
  gpointer data;
};

enum morla_rdf_object_t
{
  MORLA_RDF_OBJECT_BLANK,
  MORLA_RDF_OBJECT_LITERAL,
  MORLA_RDF_OBJECT_RESOURCE
};

struct morla_rdf_t
{
  gchar *		subject;

  gchar *		predicate;
  
  gchar *		object;
  MorlaRdfObject	object_type;

  gchar *		lang;

  gchar *		datatype;
};

/**
 * This function will be executed when the module is loaded: */
MorlaModule *	morla_module_init	(void);

/**
 * This function will be executed when the module is unloaded: */
void		morla_module_shutdown	(MorlaModule *	data);

/* Public Functions */
void		morla_dialog_msg	(gchar *	message);

gboolean	morla_dialog_ask	(gchar *	message);

gboolean	morla_rdf_parse		(gchar *	buffer,
					 gchar *	uri,
					 GList **	ret);

void		morla_rdf_free		(GList *	list);

gboolean	morla_rdf_write_xml	(GList *	list,
					 gchar **	buffer_ret);

gboolean	morla_rdf_write_ntriples
					(GList *	list,
					 gchar **	buffer_ret);

gboolean	morla_rdf_write_turtle	(GList *	list,
					 gchar **	buffer_ret);

/* EOF */

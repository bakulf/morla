/* morla - Copyright (C) 2005-2007 bakunin - Andrea Marchesini 
 *                                     <bakunin@somasuite.org>
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
#include <gmodule.h>

#include "morla-module.h"

static gboolean open_function (MorlaModule * data, GList ** rdf,
			       gchar ** uri);
static gboolean save_function (MorlaModule * data, GList * rdf);
static gboolean view_function (MorlaModule * data, GList * rdf);

static MorlaModule module = {
  "Morla Test",

  open_function,
  "Morla Test Open",

  save_function,
  "Morla Test Save",

  view_function,
  "Morla Test View",

  NULL
};

G_MODULE_EXPORT MorlaModule *
morla_module_init (void)
{
  return &module;
}

static gboolean
open_function (MorlaModule * data, GList ** rdf, gchar ** uri)
{
  g_message ("Open!");
  *uri = g_strdup ("http://test.net/example.rdf");
  return TRUE;
}

static gboolean
save_function (MorlaModule * data, GList * rdf)
{
  g_message ("Save!");
  return TRUE;
}

static gboolean
view_function (MorlaModule * data, GList * rdf)
{
  g_message ("View!");
  return TRUE;
}

/* EOF */

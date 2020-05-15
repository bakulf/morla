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

/* Check della versione di rasqal. Alcune release crashiano: */
void
check_rasqal (void)
{
  gchar s[1024];

  if (rasqal_version_major >= 1)
    return;

  snprintf (s, sizeof (s),
	    _("Your system has not a stable release of lib Rasqal (%s).\n"
	      "Some query may not return true results. "
	      "Other ones crash and if it is, morla crashes too.\n"
	      "Try to update this library (maybe also Redland).\n\n"
	      "Lib Rasqal copyright:\n%s"),
	    rasqal_version_string, rasqal_copyright_string);
  dialog_msg (s);
}

void
check_redland (void)
{
  gchar s[1024];
  gint version;

  version =
    librdf_version_major * 10000 + librdf_version_minor * 100 +
    librdf_version_release;

  if (version >= 10002)
    return;

  snprintf (s, sizeof (s),
	    _("Your system has not a compatible or stable release of "
	      "libRDF - Redland (%s).\n"
	      "Some RDF document may not be opened. "
	      "Other ones crash and if it is, morla crashes too.\n"
	      "Try to update this library.\n\n"
	      "Lib RDF - Redland copyright:\n%s"),
	    librdf_version_string, librdf_copyright_string);
  dialog_msg (s);
}

/* EOF */

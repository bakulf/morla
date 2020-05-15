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

/* Funzione about: */
void
about (GtkWidget * w, gpointer dummy)
{
  gint i;

  gchar *authors[] = {
    "Andrea Marchesini <andrea.marchesini@morlardf.net>",
    "Ippolita.Net <info@ippolita.net>",
    "Some Pataphisic Ministers",
    NULL
  };

  gchar *artists[] = {
    "Maox <mala@mala.it> http://www.malasystem.com",
    NULL
  };

  if (splash_showed ())
    {
      splash_hide ();
      return;
    }

  splash_show ();
  splash_close_on_click (TRUE);

  splash_set_text_with_effect (PACKAGE " " VERSION);
  splash_sleep (2);

  splash_set_text_with_effect (_(MORLA_COPYRIGHT));
  splash_sleep (2);

  splash_set_text_with_effect (MORLA_WEBSITE);
  splash_sleep (2);

  splash_set_text_with_effect (_("Authors:"));
  splash_sleep (2);

  for (i = 0; authors[i]; i++)
    {
      splash_set_text_with_effect (authors[i]);
      splash_sleep (2);
    }

  splash_set_text_with_effect (_("Artists:"));
  splash_sleep (2);

  for (i = 0; artists[i]; i++)
    {
      splash_set_text_with_effect (artists[i]);
      splash_sleep (2);
    }

  splash_set_text_with_effect ("");
  splash_sleep (2);

  splash_set_text_with_effect ("In girum imus nocte ecce et consumimur igni");
  splash_sleep (2);

  splash_hide ();
}

static void
about_libraries_open (GtkWidget * widget, gchar * url)
{
  browser (url);
}

#define LIB_GTK_WEBSITE		"http://www.gtk.org"
#define LIB_GLIB_WEBSITE	"http://www.gtk.org"
#define LIB_GTKHTML_WEBSITE	"http://ftp.gnome.org/pub/GNOME/sources/gtkhtml/"
#define LIB_LIBXSLT_WEBSITE	"http://xmlsoft.org/XSLT/"
#define LIB_NXML_WEBSITE	"http://www.autistici.org/bakunin/codes.php"
#define LIB_CURL_WEBSITE	"http://curl.haxx.se/"
#define LIB_REDLAND_WEBSITE	"http://librdf.org/"
#define LIB_RASQAL_WEBSITE	"http://librdf.org/rasqal/"

void
about_libraries (void)
{
  GtkWidget *dialog;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;
  gchar s[1024];
  gint i;

  dialog = gtk_dialog_new ();
  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION,
	      _("About Libraries..."));
  gtk_window_set_title (GTK_WINDOW (dialog), s);
  gtk_window_set_type_hint (GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), main_window ());
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  gtk_widget_set_size_request (dialog, 300, -1);

  box = gtk_table_new (0, 0, FALSE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), box, TRUE, TRUE,
		      0);

  i = 0;

  /* GTK: */
  button = gtk_button_new_with_label ("GTK+");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_GTK_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  g_snprintf (s, sizeof (s), "%d.%d.%d", gtk_major_version, gtk_minor_version,
	      gtk_micro_version);

  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* Glib: */
  button = gtk_button_new_with_label ("Glib");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_GLIB_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  g_snprintf (s, sizeof (s), "%d.%d.%d", glib_major_version,
	      glib_minor_version, glib_micro_version);

  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* GtkHtml: */
  button = gtk_button_new_with_label ("GtkHtml");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_GTKHTML_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);


  g_snprintf (s, sizeof (s), "2.x");
  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* libXSLT: */
  button = gtk_button_new_with_label ("LibXSLT");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_LIBXSLT_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

#if defined(LIBXSLT_DOTTED_VERSION)
  g_snprintf (s, sizeof (s), "%s", LIBXSLT_DOTTED_VERSION);
#else
  g_snprintf (s, sizeof (s), "1.x");
#endif

  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* nXML: */
  button = gtk_button_new_with_label ("nXML");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_NXML_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

#if defined(LIBNXML_VERSION_STRING)
  g_snprintf (s, sizeof (s), "%s", LIBNXML_VERSION_STRING);
#else
  g_snprintf (s, sizeof (s), "0.x");
#endif

  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* Curl: */
  button = gtk_button_new_with_label ("LibCurl");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_CURL_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

#if defined(LIBCURL_VERSION)
  g_snprintf (s, sizeof (s), "%s", LIBCURL_VERSION);
#else
  g_snprintf (s, sizeof (s), "1.x");
#endif

  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* Redland: */
  button = gtk_button_new_with_label ("Redland");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_REDLAND_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  g_snprintf (s, sizeof (s), librdf_version_string);
  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  /* Rasqal: */
  button = gtk_button_new_with_label ("Rasqal");
  g_signal_connect ((gpointer) button, "clicked",
		    G_CALLBACK (about_libraries_open), LIB_RASQAL_WEBSITE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  gtk_table_attach (GTK_TABLE (box), button, 0, 1, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  g_snprintf (s, sizeof (s), rasqal_version_string);
  label = gtk_label_new (s);
  gtk_table_attach (GTK_TABLE (box), label, 1, 2, i, i + 1,
		    GTK_EXPAND | GTK_FILL, GTK_EXPAND, 3, 3);

  i++;

  button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);

  gtk_widget_set_can_default (button, TRUE);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/* EOF */

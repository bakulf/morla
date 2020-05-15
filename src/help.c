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

gchar *help_url = NULL;
#define HELP_MAX_LIST	10

#ifdef USE_GTKHTML

struct help_t
{
  gchar *input;
  gchar *output;

  gint flag;
};

static GList *help_back_list;
static GList *help_forward_list;

static GtkWidget *help_back_widget;
static GtkWidget *help_forward_widget;
static GtkWidget *help_index_widget;

static GtkWidget *help_pb;
static GtkWidget *help_window;
static HtmlDocument *help_doc;

static gchar *help_uri;

static void help_page (gchar * page, gboolean rec);
static gchar *help_download_file (gchar * file);

/* HELP FUNCTIONS ***********************************************************/
static void
help_back_forward (void)
{
  gtk_widget_set_sensitive (help_back_widget, help_back_list ? TRUE : FALSE);
  gtk_widget_set_sensitive (help_forward_widget,
			    help_forward_list ? TRUE : FALSE);
}

static void
help_back (GtkWidget * w, gpointer dummy)
{
  gchar *uri;

  if (!help_back_list)
    return;

  if (help_uri)
    {
      help_forward_list = g_list_prepend (help_forward_list, help_uri);

      while (g_list_length (help_forward_list) > HELP_MAX_LIST)
	{
	  GList *list = g_list_last (help_forward_list);

	  g_free (list->data);
	  help_forward_list = g_list_remove (help_forward_list, list->data);
	}

      help_uri = NULL;
    }

  uri = help_back_list->data;
  help_back_list = g_list_remove (help_back_list, help_back_list->data);
  help_page (uri, FALSE);
  g_free (uri);

  help_back_forward ();
}

static void
help_forward (GtkWidget * w, gpointer dummy)
{
  gchar *uri;

  if (!help_forward_list)
    return;

  if (help_uri)
    {
      help_back_list = g_list_prepend (help_back_list, help_uri);

      while (g_list_length (help_back_list) > HELP_MAX_LIST)
	{
	  GList *list = g_list_last (help_back_list);

	  g_free (list->data);
	  help_back_list = g_list_remove (help_back_list, list->data);
	}

      help_uri = NULL;
    }

  uri = help_forward_list->data;
  help_forward_list =
    g_list_remove (help_forward_list, help_forward_list->data);
  help_page (uri, FALSE);
  g_free (uri);

  help_back_forward ();
}

static void
help_link (HtmlDocument * doc, const gchar * uri, gpointer dummy)
{
  if (strncmp (uri, "morla://", 8))
    browser ((gchar *) uri);

  else
    {
      gchar *str = NULL;

      if (help_uri)
	{
	  help_back_list = g_list_prepend (help_back_list, help_uri);
	  help_uri = NULL;
	}

      if (strlen (uri) > 8)
	str = g_strdup_printf ("%s/%s", help_url, uri + 8);

      help_page (str, FALSE);
      g_free (str);
    }
}

static void
help_null (void *fl, const char *str, ...)
{
  /* void */
}

static gchar *
help_xslt (gchar * memory)
{
  xsltStylesheetPtr cur = NULL;
  xmlDocPtr doc, res, ss_doc;
  xmlChar *ret;
  int len;
  gchar *output;
  gchar *ss;

  xmlSubstituteEntitiesDefault (1);
  xmlSetGenericErrorFunc (xmlGenericErrorContext, help_null);

  if (!strncmp (help_url, "http://", 7))
    {
      gchar *s = g_strdup_printf ("%s/xslt.xslt", help_url);
      if (download_uri (s, &ss, NULL, NULL) == FALSE)
	ss = NULL;
      g_free (s);
    }

  else
    {
      gchar *s = g_strdup_printf ("%s/xslt.xslt", help_url);
      ss = help_download_file (s);
      g_free (s);
    }

  if (!ss)
    return NULL;

  if (!(doc = xmlParseMemory (memory, strlen (memory))))
    {
      g_free (ss);
      return NULL;
    }

  if (!(ss_doc = xmlParseMemory (ss, strlen (ss))))
    {
      xmlFreeDoc (doc);
      g_free (ss);
      return NULL;
    }

  g_free (ss);

  if (!(cur = xsltParseStylesheetDoc (ss_doc)))
    {
      xmlFreeDoc (ss_doc);
      xmlFreeDoc (doc);
      return NULL;
    }

  if (!(res = xsltApplyStylesheet (cur, doc, NULL)))
    {
      xsltFreeStylesheet (cur);
      xmlFreeDoc (ss_doc);
      xmlFreeDoc (doc);
      return NULL;
    }

  if (xsltSaveResultToString (&ret, &len, res, cur))
    {
      xsltFreeStylesheet (cur);
      xmlFreeDoc (ss_doc);
      xmlFreeDoc (doc);
      xmlFreeDoc (res);
      return NULL;
    }

  xsltFreeStylesheet (cur);
  xmlFreeDoc (ss_doc);
  xmlFreeDoc (doc);
  xmlFreeDoc (res);

  xsltCleanupGlobals ();
  xmlCleanupParser ();

  output = g_malloc (sizeof (gchar) * (len + 1));
  strncpy (output, (gchar *) ret, len);
  output[len] = 0;

  xmlFree (ret);

  return output;
}

static void *
thread (void *dummy)
{
  struct help_t *data = (struct help_t *) dummy;
  gchar *mm;

  if (!strncmp (data->input, "http://", 7))
    {
      if (download_uri (data->input, &mm, NULL, NULL) == FALSE)
	mm = NULL;
    }
  else
    mm = help_download_file (data->input);

  if (!mm)
    {
      data->flag = 1;
      g_thread_exit (NULL);
      return NULL;
    }

  data->output = help_xslt (mm);
  g_free (mm);

  data->flag = 1;
  g_thread_exit (NULL);
  return NULL;
}

static void
help_page (gchar * page, gboolean rec)
{
  GThread *th;
  struct help_t data;

  gtk_widget_set_sensitive (help_index_widget, FALSE);
  gtk_widget_set_sensitive (help_back_widget, FALSE);
  gtk_widget_set_sensitive (help_forward_widget, FALSE);

  if (!page)
    page = g_strdup_printf ("%s/index.xml", help_url);
  else
    page = g_strdup (page);

  data.input = page;
  data.output = NULL;
  data.flag = 0;

  th = g_thread_create ((GThreadFunc) thread, &data, FALSE, NULL);

  while (!data.flag)
    {
      gtk_progress_bar_pulse (GTK_PROGRESS_BAR (help_pb));

      while (gtk_events_pending () && !data.flag)
	gtk_main_iteration ();

      g_usleep (50000);
    }

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (help_pb), 0);

  html_document_clear (help_doc);
  html_document_open_stream (help_doc, "text/html");

  if (!data.output)
    {
      if (rec == FALSE)
	{
	  gchar *s = g_strdup_printf ("%s/error.xml", help_url);
	  help_page (s, TRUE);
	  g_free (s);
	}

      else
	{
	  gchar *tmp;

	  tmp =
	    g_strdup_printf
	    ("<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
	     "    <head>\n" "      <title>%s</title>\n"
	     "      <link rel=\"meta\" href=\"" DOAP_URL
	     "\" title=\"rdf\"/>\n"
	     "      <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/>\n"
	     "      <style type=\"text/css\">\n" "        #title {\n"
	     "	  background: #CCC none;\n" "	  border: 1px solid #000;\n"
	     "	  padding: 2px;\n" "	}\n" "\n" "        #subtitle {\n"
	     "	  padding: 2px 2px 10px 2px;\n" "	}\n" "\n"
	     "        #content {\n"
	     "	  border-top: 1px dashed #000;\n"
	     "	  border-bottom: 1px dashed #000;\n"
	     "	  padding: 0px;\n" "	}\n" "\n"
	     "        #end {\n"
	     "	  padding: 10px;\n"
	     "	  text-align: right;\n"
	     "	}\n" "\n"
	     "        #sectitle {\n"
	     "	  padding: 5px 2px 2px 2px;\n"
	     "	  font-size: 14px;\n"
	     "	}\n" "\n"
	     "        a {\n"
	     "          text-decoration: none;\n"
	     "        }\n"
	     "\n"
	     "        a:hover {\n"
	     "          text-decoration: underline;\n"
	     "	}\n"
	     "      </style>\n"
	     "    </head>\n"
	     "    \n"
	     "    <body>\n"
	     "      <div id=\"title\"><h1>%s</h1></div>\n"
	     "      <div id=\"content\">%s</div>\n"
	     "    </body>\n"
	     "  </html>\n",
	     _
	     ("Error!"),
	     _
	     ("Error!"),
	     _
	     ("The error file does not exist, or your computer has some problem with the network!"));

	  html_document_write_stream (help_doc, tmp, strlen (tmp));

	  g_free (tmp);
	}
    }

  else
    {
      html_document_write_stream (help_doc, data.output,
				  strlen (data.output));
      g_free (data.output);

      if (help_uri)
	g_free (help_uri);

      help_uri = g_strdup (page);
    }

  gtk_widget_set_sensitive (help_index_widget, TRUE);
  help_back_forward ();

  g_free (data.input);
}

static void
help_index (GtkWidget * w, gpointer dummy)
{
  help_page (NULL, FALSE);
}

static void
help_destroy (GtkWidget * w, gpointer dummy)
{
  if (help_back_list)
    {
      g_list_foreach (help_back_list, (GFunc) g_free, NULL);
      g_list_free (help_back_list);
      help_back_list = NULL;
    }

  if (help_forward_list)
    {
      g_list_foreach (help_forward_list, (GFunc) g_free, NULL);
      g_list_free (help_forward_list);
      help_forward_list = NULL;
    }

  if (help_uri)
    {
      g_free (help_uri);
      help_uri = NULL;
    }

  help_doc = NULL;
}

void
help (void)
{
  GtkWidget *window;
  GtkWidget *frame;
  GtkWidget *sw;
  GtkWidget *vbox, *box, *bbox;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *html;

  gchar s[1024];
  gint x, y;

  if (help_doc)
    {
      gtk_window_present (GTK_WINDOW (help_window));
      return;
    }

  g_snprintf (s, sizeof (s), "%s %s - %s", PACKAGE, VERSION, _("Help"));

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), s);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (help_destroy),
		    NULL);
  help_window = window;

  x = gdk_screen_width () * 1 / 4;

  if (x < 250)
    x = 250;

  y = gdk_screen_width () * 1 / 2;

  if (y < 400)
    y = 400;

  gtk_window_resize (GTK_WINDOW (window), x, y);

  vbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  box = gtk_hbox_new (0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  help_back_widget = button;
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (help_back),
		    NULL);
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);

  bbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (button), bbox);

  image = gtk_image_new_from_stock ("gtk-go-back", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (bbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (_("Back"));
  gtk_box_pack_start (GTK_BOX (bbox), label, TRUE, TRUE, 0);

  button = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

  button = gtk_button_new ();
  help_index_widget = button;
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (help_index),
		    NULL);
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);

  bbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (button), bbox);

  image = gtk_image_new_from_stock ("gtk-index", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (bbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (_("Index"));
  gtk_box_pack_start (GTK_BOX (bbox), label, TRUE, TRUE, 0);

  button = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  help_forward_widget = button;
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (help_forward),
		    NULL);
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);

  bbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (button), bbox);

  image = gtk_image_new_from_stock ("gtk-go-forward", GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (bbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (_("Forward"));
  gtk_box_pack_start (GTK_BOX (bbox), label, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), sw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  help_doc = html_document_new ();
  g_signal_connect (G_OBJECT (help_doc), "link_clicked",
		    G_CALLBACK (help_link), NULL);

  html_document_open_stream (help_doc, "text/html");

  html = html_view_new ();
  gtk_container_add (GTK_CONTAINER (sw), html);
  html_view_set_document (HTML_VIEW (html), help_doc);

  help_pb = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), help_pb, FALSE, FALSE, 0);

  gtk_widget_show_all (window);

  help_page (NULL, FALSE);
}

static gchar *
help_download_file (gchar * file)
{
  gchar *buffer;

  if (g_file_get_contents (file, &buffer, NULL, NULL) == TRUE)
    return buffer;

  return NULL;
}

#else

void
help (void)
{
  gchar *page = g_strdup_printf ("%s/index.xml", help_url);
  browser (page);
  free (page);
}

#endif

/*
 * Giuseppe Varaldo
 * 11 luglio 1982
 * 
 * Ai lati, a esordir, dama e re, Pertini trepida, tira lieti moccoli, 
 * dialoga - vocina, pipa… -, ricorre alle battute. È durata!… ne patì Trap: 
 * allena - mèritasi lodi testé - Juvitalia, mai amata. Il boato n’eruppe su 
 * filato, mero atto d’ira: assorga da gai palati, ingoi l’arena! Si rise, 
 * noi: gara azzurra - felicità, reti - e ricca! Né tacerò pose, ire, rapidi 
 * miti; citerò paure… però meritan oro. Ci sono rari tiri? Sia! ma i latini
 * eroi goderono di rigore - c’è fallo -; “Fatale far tale rete”: lassa 
 * prosopopea nei peani dona aìre facile. Ma “fatale” malessere globi dilata, 
 * rene, vene ci necrotizza: ratto, vago, da finir al còre (l’oblierà? 
 * Dall’idea - l’Erinni! - trepiderà: tic e tac…)… Lapsus saliente (idra! 
 * sillabo!): non amai Cabrini; flusso acre - pus era? sudore? -, bile 
 * d’ittero ci assalì: risa brutali, amaro icore… Fiore italo, cari miei,
 * secca, alidirà vizzito là, se sol - a foci nuove diretti, fisi - a metà 
 * recedete: l’itala idea di vis (i redivivi, noti, ilari miti!) trapasserà,
 * inerte e vana, in italianità lisa, banal. Attutite relativa ira, correte: 
 * eterni onori n’avrete! Sibili - tre “fi” - di arbitro: finita lì metà
 * partita; reca loro l’animo di lotta, fidata ripresa! mira, birra rida’! 
 * attuta ire, bile! La si disse “eterea”, la Catalogna: alla pari terrò
 * cotali favolose ore… Notte molle, da re! Poeti m’illusero (”Va’!”, “Fa’!”,
 * “Osa!”) colla fusione - esile, serica, viva -, rime lepide, tra anelito 
 * d’età d’oro e rudezze d’orpello; così cederò all’eros, ai sensi rei; amai 
 * -l’amavo… - una grata città, la gag, la vita; nutro famosa cara sete,
 * relativa a Lalo, Varese, De Falla, Petrassi, e Ravel, e Adam, e Nono… 
 * Sor… bene, totale opaca arte; né pago fui per attori, dive, divi (lo
 * sarò?)… Là ogni avuto, mai sopito piacere s’evaporò, leggera falena era: se
 * con amor, lì, alla cara - cotale! - virile sera - coi gaudi sereni, grevi 
 * da dare angine, beati - lo paragono, decàde a ludo, mollica, vile cineseria,
 * onere. Sì! Taccola barocca allora rimane, meno mi tange: solo apatia
 * apporterà, goffa noia… Paride, Ettore e soci trovarono sì dure sorti -
 * riverberare di pira desueta! - coi gelosi re dei Dori (trono era d’ira,
 * Era, Muse); a Ilio nati e no, di elato tono, di rango, là tacitati - re…
 * mogi -, videro Elleni libare, simil a Titani, su al Pergamo: idem i Renani
 * e noi… “… caparbi”, vaticinò - tono trepido -, ed ora tange là tale causale
 * trofeo (coppa di rito è la meta della partita), trainer fisso; mìralo come
 * l’anemone: fisso, raro, da elogi… D’animo nobile, divo mai, mai tetro,
 * fatale varò la tattica. Cito Gay, ognor abile devo dir: da Maracanà sono
 * tacco, battuta… Ai lati issò vela l’ala latina Bruno: cerca la rete, si
 * batte assai, opera lì, fora, rimargina… Bergomi, nauta ragazzo, riserra
 * giù sì care fila: è l’età… Coi gradi vedo - troppa la soavità… - capitano
 * Dino, razza ladina. Rete vigila! dilàtati…!: la turba, l’arena, ti venera.
 * Ad ogni rado, torpido e no, tirabile tiro, trapelò rapidità sua: parò (la
 * tivù, lì, diè nitidi casi). Di tutto - fiero, mai di fatica, vivace -
 * raccatta: e, se tarpate, le ali loro - è la verità - paion logore. Zoff
 * (ùtinam !) è dei.. Parà: para… Piede, mani, tuffo: zero gol, noi a patire.
 * Vale oro: lì, là… è l’età… “Pratese, attacca! reca vivacità!”, “Fidiamo!”,
 * “Rei fottuti disaciditi!”… Nei diluvi, talora pausati, di parole partorite
 * lì, baritone o di proto, da ring o da arene (”Vita nera là, brutalità tali
 * da ligi veterani, da… lazzaroni!”, “Dònati! pàcati! va’! osa!: l’apporto
 * devi dar!”, “Giocate leali, feraci!”, “Su i garresi!”, “Rozza gara!”, “Tu,
 * animo!”, “Grèbani! Grami!”, “Raro filare!”; poi: “Assaetta!”, “Bis!” e
 * “Ter!”), alacre, con urbanità, l’alalà levossi: “Italia!”, a tutta bocca,
 * tonò. Sana cara Madrid, ove delibaron Goya… gotica città talora velata:
 * forte ti amiamo! Vi delibo nomina di goleador a Rossi - fenomenale! -: 
 * mo’, colà, rimossi freni artati (tra palle date male o tiri dappoco è forte
 * la sua celata legnata), rode, o d’ipertono, tonicità, vibra. Pacione inane,
 * rimediò magre, plausi - nati tali - miserabili nelle ore di Vigo
 * (meritàti!); Catalogna ridonò totale idoneità - noi lì a esumare, a ridare
 * onor -, tiro diede, riso; le giocate use - da ripide, rare, brevi, ritrose,
 * rudi - son ora vorticose e rotte, e d’ira paion affogare (troppa?). Aìta,
 * Paolo!: segna, timone mena, mira, rolla, accora, balòccati sereno, aìre -
 * se Nice lì vacillò - modula e da’ (cedono…): gara polita e benigna - e rada,
 * di vergine residua… - gioca. Re s’è lì rivelato (Caracalla? Il romano
 * Cesare!): anela, fa, regge loro, pavese reca…: ipotiposi amo. Tu va’ in
 * goal, ora! Sol, ivi, devi dirottare più foga: penetra a capo elato -
 * tenebroso non è… -, ma da elevare, issar te, palla, fede, sera (vola, là) a
 * vitale rete! Sarà caso… Ma Fortuna ti valga galattica targa, nuova malìa:
 * mai Eris ne sia sorella! Or è deciso; colle prodezze, dure o rodate doti -
 * lena, arte di Pelé, mira -, vivaci rese lì sé e noi: su fallo (caso a favore
 * sul limite, opera dell’ometto nero) è solo, va filato, corre, tira, palla
 * angolata cala… è rete! Essi di sale, l’Iberia tutta a dir “Arriba!”, rimaser.
 * Pirata? Di fatto li domina… Loro lacerati tra patemi; Latini forti, braidi,
 * fertili: bis e ter van, ìrono in rete… E terrò cari a vita: le reti; tutta
 * l’anabasi latina; i Latini, a nave e treni, a ressa partiti (mìrali!); i
 * toni vivi, derisivi, d’aedi alati; le tede cerate (”Mai sì fitte” ridevo:
 * unico falò s’esalò, tizzi vari di là accesi); e i miracolati eroi, feroci…
 * Oramai la turba si rilassa: i coretti deliberò d’usare. Supercaos sul finir!
 * Baciamano? No: balli sardi, etnei lassù (spalcate!); citaredi per tinnire,
 * là, ed il “la” dare; il Bolero, clarini, fado, gavotta, razzi, torce (Nice
 * n’è venerata) lì. Di bolge, resse, la melata famelica “feria” anodina è
 * piena, e po’ po’ sorpassa l’etere la trafelata folla. Fecero giri d’onore:
 * dogi o re, in Italia, mai si ritirarono sì coronati. Remore, Perù, aporetici
 * timidi pareri… e sopore, catenacci reiterati, Cile, far ruzza: a ragione si
 * risanerà lì ogni itala piaga; da grossa a ridotta, o remota, lì fu, seppure
 * nota, obliata. Mai amai la tivù: jet-set, idoli, satire…; ma nella partita
 * - penata, rude e tutta bella: erro? - ci rapì: panico vago, lai di locco,
 * mite ilarità di Pertini… tre pere a Madrid, rosea Italia!
 */

/* EOF */

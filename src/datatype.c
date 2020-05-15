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

#define DATATYPE_XSD( a , b , c ) \
  if (i >= len || w[i] < a || w[i] > b) \
    { \
      gchar s[1024]; \
      snprintf (s, sizeof (s), c, msg, w); \
      dialog_msg (s); \
      \
      return FALSE; \
    } \
  else \
    i++;

static gboolean datatype_check_string (gchar *, gchar *);
static gboolean datatype_check_boolean (gchar *, gchar *);
static gboolean datatype_check_decimal (gchar *, gchar *);
static gboolean datatype_check_float (gchar *, gchar *);
static gboolean datatype_check_double (gchar *, gchar *);
static gboolean datatype_check_duration (gchar *, gchar *);
static gboolean datatype_check_dateTime (gchar *, gchar *);
static gboolean datatype_check_time (gchar *, gchar *);
static gboolean datatype_check_date (gchar *, gchar *);
static gboolean datatype_check_gYearMonth (gchar *, gchar *);
static gboolean datatype_check_gYear (gchar *, gchar *);
static gboolean datatype_check_gMonth (gchar *, gchar *);
static gboolean datatype_check_gMonthDay (gchar *, gchar *);
static gboolean datatype_check_gDay (gchar *, gchar *);
static gboolean datatype_check_hexBinary (gchar *, gchar *);
static gboolean datatype_check_base64Binary (gchar *, gchar *);
static gboolean datatype_check_anyURI (gchar *, gchar *);
static gboolean datatype_check_qname (gchar *, gchar *);
static gboolean datatype_check_notation (gchar *, gchar *);

struct datatype_t *
datatype_list (void)
{
  static struct datatype_t *datatypes = NULL;

  if (!datatypes)
    {
      datatypes = malloc (sizeof (struct datatype_t) * 21);

      datatypes[0].uri = "";
      datatypes[0].name = "";
      datatypes[0].description = _("no datatype");
      datatypes[0].check = NULL;

      datatypes[1].uri = RDF_XSD "string";
      datatypes[1].name = _("String");
      datatypes[1].description =
	_("The string datatype represents character strings in XML.");
      datatypes[1].check = datatype_check_string;

      datatypes[2].uri = RDF_XSD "boolean";
      datatypes[2].name = _("Boolean value");
      datatypes[2].description =
	_
	("Boolean has the value space required to support the mathematical concept of binary-valued logic: {true, false}.");
      datatypes[2].check = datatype_check_boolean;

      datatypes[3].uri = RDF_XSD "decimal";
      datatypes[3].name = _("Decimal number");
      datatypes[3].description =
	_
	("Decimal represents a subset of the real numbers; which can be represented by decimal numerals.");
      datatypes[3].check = datatype_check_decimal;

      datatypes[4].uri = RDF_XSD "float";
      datatypes[4].name = _("Float number");
      datatypes[4].description =
	_
	("Float is patterned after the IEEE single-precision 32-bit floating point type.");
      datatypes[4].check = datatype_check_float;

      datatypes[5].uri = RDF_XSD "double";
      datatypes[5].name = _("Double number");
      datatypes[5].description =
	_
	("The double datatype is patterned after the IEEE double-precision 64-bit floating point type.");
      datatypes[5].check = datatype_check_double;

      datatypes[6].uri = RDF_XSD "duration";
      datatypes[6].name = _("Duration");
      datatypes[6].description = _("Duration represents a duration of time.");
      datatypes[6].check = datatype_check_duration;

      datatypes[7].uri = RDF_XSD "dateTime";
      datatypes[7].name = _("Data and Time");
      datatypes[7].description =
	_
	("DateTime values may be viewed as objects with integer-valued year, month, day, hour and minute properties, a decimal-valued second property, and a boolean timezoned property.");
      datatypes[7].check = datatype_check_dateTime;

      datatypes[8].uri = RDF_XSD "time";
      datatypes[8].name = _("Time");
      datatypes[8].description =
	_("Time represents an instant of time that recurs every day.");
      datatypes[8].check = datatype_check_time;

      datatypes[9].uri = RDF_XSD "date";
      datatypes[9].name = _("Date");
      datatypes[9].description =
	_
	("The value space of date consists of top-open intervals of exactly one day in length on the timelines of dateTime, beginning on the beginning moment of each day (in each timezone), i.e. '00:00:00', up to but not including '24:00:00' (which is identical with '00:00:00' of the next day).");
      datatypes[9].check = datatype_check_date;

      datatypes[10].uri = RDF_XSD "gYearMonth";
      datatypes[10].name = _("Year and Month");
      datatypes[10].description =
	_
	("gYearMonth represents a specific gregorian month in a specific gregorian year.");
      datatypes[10].check = datatype_check_gYearMonth;

      datatypes[11].uri = RDF_XSD "gYear";
      datatypes[11].name = _("Year");
      datatypes[11].description =
	_("gYear represents a gregorian calendar year.");
      datatypes[11].check = datatype_check_gYear;

      datatypes[12].uri = RDF_XSD "gMonthDay";
      datatypes[12].name = _("Month and Day");
      datatypes[12].description =
	_
	("gMonthDay is a gregorian date that recurs, specifically a day of the year such as the third of May.");
      datatypes[12].check = datatype_check_gMonthDay;

      datatypes[13].uri = RDF_XSD "gDay";
      datatypes[13].name = _("Day");
      datatypes[13].description =
	_
	("gDay is a gregorian day that recurs, specifically a day of the month such as the 5th of the month.");
      datatypes[13].check = datatype_check_gDay;

      datatypes[14].uri = RDF_XSD "gMonth";
      datatypes[14].name = _("Month");
      datatypes[14].description =
	_("gMonth is a gregorian month that recurs every year.");
      datatypes[14].check = datatype_check_gMonth;

      datatypes[15].uri = RDF_XSD "hexBinary";
      datatypes[15].name = _("hex-encoded binary data");
      datatypes[15].description =
	_("hexBinary represents arbitrary hex-encoded binary data.");
      datatypes[15].check = datatype_check_hexBinary;

      datatypes[16].uri = RDF_XSD "base64Binary";
      datatypes[16].name = _("Base64 encoded binary data");
      datatypes[16].description =
	_("base64Binary represents Base64-encoded arbitrary binary data.");
      datatypes[16].check = datatype_check_base64Binary;

      datatypes[17].uri = RDF_XSD "anyURI";
      datatypes[17].name = _("Uri");
      datatypes[17].description =
	_("anyURI represents a Uniform Resource Identifier Reference (URI).");
      datatypes[17].check = datatype_check_anyURI;

      datatypes[18].uri = RDF_XSD "QName";
      datatypes[18].name = _("Qualified Name");
      datatypes[18].description = _("QName represents XML qualified names.");
      datatypes[18].check = datatype_check_qname;

      datatypes[19].uri = RDF_XSD "NOTATION";
      datatypes[19].name = _("Notation");
      datatypes[19].description =
	_
	("NOTATION represents the NOTATION attribute type from XML 1.0 (Second Edition).");
      datatypes[19].check = datatype_check_notation;

      datatypes[20].uri = NULL;
      datatypes[20].name = NULL;
      datatypes[20].description = NULL;
      datatypes[20].check = NULL;
    }

  return datatypes;
}

gboolean
datatype_check (gint id, gchar * what, gchar * msg)
{
  struct datatype_t *datatype = datatype_list ();
  gchar s[1024];

  if (id < 0)
    return TRUE;

  if (!datatype[id].check)
    return TRUE;

  snprintf (s, sizeof (s), "%s%s", msg ? msg : "", msg ? ": " : "");
  return datatype[id].check (what, s);
}

/* Questa funzione viene avviata quando si cambia il datatype */
void
datatype_change (GtkWidget * w, GtkWidget * label)
{
  gint id = gtk_combo_box_get_active (GTK_COMBO_BOX (w));
  struct datatype_t *datatype = datatype_list ();
  gchar s[1024];

  if (id < 0)
    {
      gtk_label_set_text (GTK_LABEL (label), _("Datatype unknown"));
      return;
    }

  if (*datatype[id].name)
    snprintf (s, sizeof (s), "%s\n%s", datatype[id].name,
	      datatype[id].description);
  else
    snprintf (s, sizeof (s), "%s", datatype[id].description);

  gtk_label_set_text (GTK_LABEL (label), s);
}

gint
datatype_id (gchar * what)
{
  gint i;
  struct datatype_t *datatype = datatype_list ();

  for (i = 0; datatype[i].uri; i++)
    if (!strcmp (datatype[i].uri, what))
      return i;

  return 0;
}

static gboolean
datatype_check_string (gchar * w, gchar * msg)
{
  return TRUE;
}

static gboolean
datatype_check_boolean (gchar * w, gchar * msg)
{
  if (strcmp (w, "true") && strcmp (w, "false"))
    {
      gchar s[1024];

      snprintf (s, sizeof (s),
		_("%sThe object '%s' is not a boolean value (true or false)"),
		msg, w);
      dialog_msg (s);
      return FALSE;
    }

  return TRUE;
}

static gboolean
datatype_check_decimal (gchar * w, gchar * msg)
{
  gint len, i;

  len = strlen (w);
  i = 0;

  if (w[i] == '-' || w[i] == '+')
    i++;

  for (; i < len; i++)
    DATATYPE_XSD ('0', '9',
		  _("%sThe object '%s' is not a decimal value (-12, 20)"));

  return TRUE;
}

static gboolean
datatype_check_float (gchar * w, gchar * msg)
{
  if (!strtod (w, NULL) && strcmp (w, "0"))
    {
      gchar s[1024];

      snprintf (s, sizeof (s),
		_
		("%sThe object '%s' is not a float value (-1E4, 1267.43233E12, 12.78e-2, 12 , -0, 0)"),
		msg, w);
      dialog_msg (s);
      return FALSE;
    }

  return TRUE;
}

static gboolean
datatype_check_double (gchar * w, gchar * msg)
{
  if (!atof (w) && strcmp (w, "0"))
    {
      gchar s[1024];

      snprintf (s, sizeof (s),
		_
		("%sThe object '%s' is not a double value (-1E4, 1267.43233E12, 12.78e-2, 12 , -0, 0 and INF)"),
		msg, w);
      dialog_msg (s);
      return FALSE;
    }

  return TRUE;
}

static gboolean
datatype_check_duration (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  for (; i < len; i++)
    {
      if ((w[i] < '0' || w[i] > '9') && w[i] != 'H' && w[i] != 'M'
	  && w[i] != 'S' && w[i] != 'Y' && w[i] != 'D' && w[i] != 'P')
	{
	  gchar s[1024];

	  snprintf (s, sizeof (s),
		    _
		    ("%sThe object '%s' is not a duration value (P1347Y, P1347M and P1Y2MT2H)"),
		    msg, w);
	  dialog_msg (s);
	  return FALSE;
	}
    }

  return TRUE;
}

static gboolean
datatype_check_dateTime (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  if (w[0] == '-')
    i++;

  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('-', '-',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('-', '-',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('T', 'T',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD (':', ':',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD (':', ':',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_
		("%sThe object '%s' is not a dateTime value (0001-01-01T00:00:00)"));

  return TRUE;
}

static gboolean
datatype_check_time (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD (':', ':',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD (':', ':',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a time value (00:00:00)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a time value (00:00:00)"));

  return TRUE;
}

static gboolean
datatype_check_date (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('-', '-',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('-', '-',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a date value (0001-01-01)"));

  return TRUE;
}

static gboolean
datatype_check_gYearMonth (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));
  DATATYPE_XSD ('-', '-',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gYearMonth value (2005-04)"));

  return TRUE;
}

static gboolean
datatype_check_gYear (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gYear value (2005)"));
  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gYear value (2005)"));
  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gYear value (2005)"));
  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gYear value (2005)"));

  return TRUE;
}

static gboolean
datatype_check_gMonth (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gMonth value (05)"));
  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gMonth value (05)"));

  return TRUE;
}

static gboolean
datatype_check_gMonthDay (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gMonthDay value (MM:DD)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gMonthDay value (MM:DD)"));
  DATATYPE_XSD (':', ':',
		_("%sThe object '%s' is not a gMonthDay value (MM:DD)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gMonthDay value (MM:DD)"));
  DATATYPE_XSD ('0', '9',
		_("%sThe object '%s' is not a gMonthDay value (MM:DD)"));

  return TRUE;
}

static gboolean
datatype_check_gDay (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gDay value (DD)"));
  DATATYPE_XSD ('0', '9', _("%sThe object '%s' is not a gDay value (DD)"));

  return TRUE;
}

static gboolean
datatype_check_hexBinary (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  for (; i < len; i++)
    {
      if ((w[i] < '0' || w[i] > '9') && (w[i] < 'A' || w[i] > 'F')
	  && (w[i] < 'a' || w[i] > 'f'))
	{
	  gchar s[1024];

	  snprintf (s, sizeof (s),
		    _("%sThe object '%s' is not a hexBinary value (0AF37CD)"),
		    msg, w);
	  dialog_msg (s);
	  return FALSE;

	  return FALSE;
	}
    }

  return TRUE;
}

static gboolean
datatype_check_base64Binary (gchar * w, gchar * msg)
{
  gint i, len;

  len = strlen (w);
  i = 0;

  for (; i < len; i++)
    {
      if ((w[i] < '0' || w[i] > '9') && (w[i] < 'A' || w[i] > 'F')
	  && (w[i] < 'a' || w[i] > 'f'))
	{
	  gchar s[1024];

	  snprintf (s, sizeof (s),
		    _
		    ("%sThe object '%s' is not a base64Binary value (0AF37CD)"),
		    msg, w);
	  dialog_msg (s);
	  return FALSE;

	  return FALSE;
	}
    }

  return TRUE;
}

static gboolean
datatype_check_anyURI (gchar * w, gchar * msg)
{
  /* Uncheckable */
  return TRUE;
}

static gboolean
datatype_check_qname (gchar * w, gchar * msg)
{
  /* Uncheckable */
  return TRUE;
}

static gboolean
datatype_check_notation (gchar * w, gchar * msg)
{
  /* Uncheckable */
  return TRUE;
}

gboolean
datatype_exists (gchar * what)
{
  struct datatype_t *datatypes = datatype_list ();
  gint i;

  for (i = 1; datatypes[i].uri; i++)
    if (!strcmp (datatypes[i].uri, what))
      return TRUE;

  return FALSE;
}

/* EOF */

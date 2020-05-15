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

#ifdef USE_JS

#ifdef USE_NGS_JS
static int js_stderr (void *context, unsigned char *buffer,
		      unsigned int amount);

static JSMethodResult js_alert (void *context, JSInterpPtr interp, int argc,
				JSType * argv, JSType * result_return,
				char *error_return);
static JSMethodResult js_confirm (void *context, JSInterpPtr interp, int argc,
				  JSType * argv, JSType * result_return,
				  char *error_return);
static JSMethodResult js_get_value (void *context, JSInterpPtr interp,
				    int argc, JSType * argv,
				    JSType * result_return,
				    char *error_return);
static JSMethodResult js_set_value (void *context, JSInterpPtr interp,
				    int argc, JSType * argv,
				    JSType * result_return,
				    char *error_return);
static gchar *js_set_value_real (JSType * var);
static JSMethodResult js_morla_version (void *context, JSInterpPtr interp,
					int argc, JSType * argv,
					JSType * result_return,
					char *error_return);

#elif USE_MOZILLA_JS
static void js_stderr (JSContext * context, const char *message,
		       JSErrorReport * report);

static JSBool js_alert (JSContext * cx, JSObject * obj, uintN argc,
			jsval * argv, jsval * rval);
static JSBool js_confirm (JSContext * cx, JSObject * obj, uintN argc,
			  jsval * argv, jsval * rval);
static JSBool js_get_value (JSContext * cx, JSObject * obj, uintN argc,
			    jsval * argv, jsval * rval);
static JSBool js_set_value (JSContext * cx, JSObject * obj, uintN argc,
			    jsval * argv, jsval * rval);
static JSBool js_version (JSContext * cx, JSObject * obj, uintN argc,
			  jsval * argv, jsval * rval);

static JSClass globalClass = {
  "Global", 0, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_PropertyStub, JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
  JS_FinalizeStub
};

static JSFunctionSpec functions[] = {
  {"morla_alert", js_alert, 0},
  {"morla_confirm", js_confirm, 0},
  {"morla_get_value", js_get_value, 0},
  {"morla_set_value", js_set_value, 0},
  {"morla_version", js_version, 0},
  {0}
};
#endif

/* Javascript engine init: */
struct js_t *
js_new (void)
{
  struct js_t *js;

#ifdef USE_NGS_JS
  JSInterpPtr interp;
  JSInterpOptions options;

  JSType var;

  js_init_default_options (&options);

  options.s_stderr = js_stderr;
  options.s_context = NULL;

  interp = js_create_interp (&options);

  js = g_malloc0 (sizeof (struct js_t));
  js->interp = interp;

  /* Variables: */
  var.type = JS_TYPE_BOOLEAN;
  var.u.i = JS_MORLA_OK;
  js_set_var (interp, "MORLA_OK", &var);

  var.type = JS_TYPE_BOOLEAN;
  var.u.i = JS_MORLA_FAILURE;
  js_set_var (interp, "MORLA_FAILURE", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = 0;
  js_set_var (interp, "MORLA_VALUE", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = 1;
  js_set_var (interp, "MORLA_LANGUAGE", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = 2;
  js_set_var (interp, "MORLA_DATATYPE", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = 3;
  js_set_var (interp, "MORLA_TYPE", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = RDF_OBJECT_BLANK;
  js_set_var (interp, "MORLA_RDF_BLANKNODE", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = RDF_OBJECT_LITERAL;
  js_set_var (interp, "MORLA_RDF_LITERAL", &var);

  var.type = JS_TYPE_INTEGER;
  var.u.i = RDF_OBJECT_RESOURCE;
  js_set_var (interp, "MORLA_RDF_RESOURCE", &var);

  /* Functions: */
  js_create_global_method (interp, "morla_alert", js_alert, js, NULL);
  js_create_global_method (interp, "morla_confirm", js_confirm, js, NULL);
  js_create_global_method (interp, "morla_get_value", js_get_value, js, NULL);
  js_create_global_method (interp, "morla_set_value", js_set_value, js, NULL);
  js_create_global_method (interp, "morla_version", js_morla_version, js,
			   NULL);

#elif USE_MOZILLA_JS
  JSRuntime *rt;
  JSContext *cx;
  JSObject *global;

  GString *str;
  gchar *buffer;
  jsval rval;

  rt = JS_NewRuntime (0x400000L);
  cx = JS_NewContext (rt, 0x1000);

  global = JS_NewObject (cx, &globalClass, NULL, NULL);

  JS_InitStandardClasses (cx, global);
  JS_DefineFunctions (cx, global, functions);

  str = g_string_new (NULL);
  g_string_append_printf (str, "var MORLA_OK=%d;\n", JS_MORLA_OK);
  g_string_append_printf (str, "var MORLA_FAILURE=%d;\n", JS_MORLA_FAILURE);
  g_string_append_printf (str, "var MORLA_VALUE=0;\n");
  g_string_append_printf (str, "var MORLA_LANGUAGE=1;\n");
  g_string_append_printf (str, "var MORLA_DATATYPE=2;\n");
  g_string_append_printf (str, "var MORLA_TYPE=3;\n");
  g_string_append_printf (str, "var MORLA_RDF_BLANKNODE=%d;\n",
			  RDF_OBJECT_BLANK);
  g_string_append_printf (str, "var MORLA_RDF_LITERAL=%d;\n",
			  RDF_OBJECT_LITERAL);
  g_string_append_printf (str, "var MORLA_RDF_RESOURCE=%d;\n",
			  RDF_OBJECT_RESOURCE);
  g_string_append_printf (str, "var morla_status=0;\n");

  buffer = g_string_free (str, FALSE);
  JS_EvaluateScript (cx, global, buffer, strlen (buffer), "script", 0, &rval);
  g_free (buffer);

  JS_SetErrorReporter (cx, js_stderr);

  js = g_malloc0 (sizeof (struct js_t));
  js->rt = rt;
  js->cx = cx;
  js->global = global;

  JS_SetContextPrivate (cx, js);
#endif

  return js;
}

/* Destroy everything: */
void
js_destroy (struct js_t *js)
{
  if (!js)
    return;

#ifdef USE_NGS_JS
  js_destroy_interp (js->interp);

#elif USE_MOZILLA_JS
  JS_DestroyContext (js->cx);
  JS_DestroyRuntime (js->rt);
#endif

  g_free (js);
}

/* Change the current widget: */
void
js_current_widget (struct js_t *js, GtkWidget * widget)
{
  if (js && widget)
    js->widget = widget;
}

/* Evaluate a js script. The script is ok if the morla_status variabile will be
 * setted to: MORLA_OK: */
gboolean
js_evaluate (struct js_t *js, gchar * buffer)
{
#ifdef USE_NGS_JS
  JSType type;

  type.type = JS_TYPE_BOOLEAN;
  type.u.i = 0;

  js_set_var (js->interp, "morla_status", &type);

  js_eval (js->interp, buffer);

  js_get_var (js->interp, "morla_status", &type);

  if (type.type == JS_TYPE_BOOLEAN || type.type == JS_TYPE_INTEGER)
    return type.u.i == JS_MORLA_OK ? JS_MORLA_OK : JS_MORLA_FAILURE;

  return JS_MORLA_FAILURE;
#elif USE_MOZILLA_JS
  jsval rval;
  gchar *str;

  str = "morla_status=0;\n";
  if (JS_EvaluateScript
      (js->cx, js->global, str, strlen (str), "script", 0, &rval) == JS_FALSE)
    return JS_MORLA_FAILURE;

  if (JS_EvaluateScript
      (js->cx, js->global, buffer, strlen (buffer), NULL, 0,
       &rval) == JS_FALSE)
    return JS_MORLA_FAILURE;

  str = "morla_status;\n";
  if (JS_EvaluateScript
      (js->cx, js->global, str, strlen (str), "script", 0, &rval) == JS_FALSE)
    return JS_MORLA_FAILURE;

  return !JSVAL_TO_DOUBLE (rval) ? JS_MORLA_OK : JS_MORLA_FAILURE;
#endif
}

#ifdef USE_NGS_JS
static int
js_stderr (void *context, unsigned char *buffer, unsigned int amount)
{
  return fwrite (buffer, 1, amount, stderr);
}
#elif USE_MOZILLA_JS
static void
js_stderr (JSContext * context, const char *message, JSErrorReport * report)
{
  if (!report)
    {
      g_fprintf (stderr, "%s", message);
      return;
    }

  g_fprintf (stderr, "Line %d: %s\n", report->lineno, message);
}
#endif

/* FUNCTIONS ********************************************************************/

#ifdef USE_NGS_JS
/* Alert: */
static gchar *
js_alert_real (int argc, JSType * argv)
{
  GString *str = g_string_new (NULL);
  gint i;

  for (i = 0; i < argc; i++)
    {
      JSType *arg = &argv[i];

      switch (arg->type)
	{
	case JS_TYPE_STRING:
	  if (i > 0)
	    str = g_string_append (str, " ");

	  if (arg->u.s && arg->u.s->data)
	    str =
	      g_string_append_len (str, (const gchar *) arg->u.s->data,
				   arg->u.s->len);
	  break;

	case JS_TYPE_INTEGER:
	case JS_TYPE_BOOLEAN:
	  if (i > 0)
	    str = g_string_append (str, " ");

	  g_string_append_printf (str, "%ld", arg->u.i);
	  break;

	case JS_TYPE_DOUBLE:
	  if (i > 0)
	    str = g_string_append (str, " ");

	  g_string_append_printf (str, "%f", arg->u.d);
	  break;

	case JS_TYPE_ARRAY:
	  if (i > 0)
	    str = g_string_append (str, " ");

	  g_string_append_printf (str, "Array");
	  break;

	default:
	  return NULL;
	  break;
	}
    }

  return g_string_free (str, FALSE);
}

static JSMethodResult
js_alert (void *context, JSInterpPtr interp, int argc, JSType * argv,
	  JSType * result_return, char *error_return)
{
  gchar *buffer;

  if (!(buffer = js_alert_real (argc, argv)))
    {
      sprintf (error_return, "morla_alert: illegal argument");
      return JS_ERROR;
    }


  dialog_msg (buffer);
  g_free (buffer);

  result_return->type = JS_TYPE_NULL;
  return JS_OK;
}

/* Confirm: */
static JSMethodResult
js_confirm (void *context, JSInterpPtr interp, int argc, JSType * argv,
	    JSType * result_return, char *error_return)
{
  gchar *buffer;

  if (!(buffer = js_alert_real (argc, argv)))
    {
      sprintf (error_return, "morla_confirm: illegal argument");
      return JS_ERROR;
    }

  if (dialog_ask (buffer) == GTK_RESPONSE_OK)
    result_return->u.i = 1;
  else
    result_return->u.i = 0;

  g_free (buffer);

  result_return->type = JS_TYPE_BOOLEAN;
  return JS_OK;
}

static JSMethodResult
js_get_value (void *context, JSInterpPtr interp, int argc, JSType * argv,
	      JSType * result_return, char *error_return)
{
  struct js_t *js = context;
  struct template_value_t *value;

  value = template_value_new (js->widget);

  js_type_make_array (js->interp, result_return, 4);

  if (value->value)
    js_type_make_string (js->interp, &result_return->u.array->data[0],
			 (unsigned char *) value->value,
			 strlen (value->value));
  else
    js_type_make_string (js->interp, &result_return->u.array->data[0],
			 (unsigned char *) "", 0);

  if (value->lang)
    js_type_make_string (js->interp, &result_return->u.array->data[1],
			 (unsigned char *) value->lang, strlen (value->lang));
  else
    js_type_make_string (js->interp, &result_return->u.array->data[1],
			 (unsigned char *) "", 0);

  if (value->datatype)
    js_type_make_string (js->interp, &result_return->u.array->data[2],
			 (unsigned char *) value->datatype,
			 strlen (value->datatype));
  else
    js_type_make_string (js->interp, &result_return->u.array->data[2],
			 (unsigned char *) "", 0);

  result_return->u.array->data[3].type = JS_TYPE_INTEGER;
  result_return->u.array->data[3].u.i = value->type;

  template_value_free (value);

  return JS_OK;
}

static JSMethodResult
js_set_value (void *context, JSInterpPtr interp, int argc, JSType * argv,
	      JSType * result_return, char *error_return)
{
  struct template_value_t *value;
  struct js_t *js = context;

  if (argc != 1)
    {
      sprintf (error_return, "morla_set_value: too many argument");
      return JS_ERROR;
    }

  if (argv[0].type != JS_TYPE_ARRAY || !argv[0].u.array)
    {
      sprintf (error_return, "morla_set_value: the argument is not an array");
      return JS_ERROR;
    }

  value = g_malloc0 (sizeof (struct template_value_t));

#define JS_SET_VALUE( x , y ) \
  if (argv[0].u.array->length >= y + 1) \
    value->x = js_set_value_real (argv[0].u.array->data + y); \
  else \
    value->x = NULL; \

  JS_SET_VALUE (value, 0);
  JS_SET_VALUE (lang, 1);
  JS_SET_VALUE (datatype, 2);

  if (argv[0].u.array->length >= 3 && argv[0].u.array->data + 3)
    {
      JSType *var = argv[0].u.array->data + 3;

      if (var->type == JS_TYPE_INTEGER && var->u.i >= 0 && var->u.i <= 3)
	value->type = var->u.i;
      else
	value->type = RDF_OBJECT_RESOURCE;

    }

  template_value_set (js->widget, value);
  template_value_free (value);

  result_return->type = JS_TYPE_NULL;
  return JS_OK;
}

static gchar *
js_set_value_real (JSType * var)
{
  if (var->type == JS_TYPE_INTEGER || var->type == JS_TYPE_BOOLEAN)
    return g_strdup_printf ("%ld", var->u.i);

  else if (var->type == JS_TYPE_STRING && var->u.s->len)
    return g_strndup ((const gchar *) var->u.s->data, var->u.s->len);

  else if (var->type == JS_TYPE_DOUBLE)
    return g_strdup_printf ("%f", var->u.d);

  return NULL;
}

static JSMethodResult
js_morla_version (void *context, JSInterpPtr interp, int argc, JSType * argv,
		  JSType * result_return, char *error_return)
{
  struct js_t *js = context;
  js_type_make_string (js->interp, result_return, (unsigned char *) VERSION,
		       strlen (VERSION));
  return JS_OK;
}

#elif USE_MOZILLA_JS
static JSBool
js_alert (JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
	  jsval * rval)
{
  JSString *str;

  if ((!JSVAL_IS_STRING (argv[0]) && !JSVAL_IS_NUMBER (argv[0]))
      || !(str = JS_ValueToString (cx, argv[0])))
    return JS_FALSE;

  dialog_msg (JS_GetStringBytes (str));
  return JS_TRUE;
}

static JSBool
js_confirm (JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
	    jsval * rval)
{
  JSString *str;

  if ((!JSVAL_IS_STRING (argv[0]) && !JSVAL_IS_NUMBER (argv[0]))
      || !(str = JS_ValueToString (cx, argv[0])))
    return JS_FALSE;

  if (dialog_ask (JS_GetStringBytes (str)) == GTK_RESPONSE_OK)
    *rval = JSVAL_TRUE;
  else
    *rval = JSVAL_FALSE;

  return JS_TRUE;
}

static JSBool
js_version (JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
	    jsval * rval)
{
  JSString *version = JS_NewStringCopyN (cx, VERSION, strlen (VERSION));
  *rval = STRING_TO_JSVAL (version);
  return JS_TRUE;
}

static JSBool
js_get_value (JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
	      jsval * rval)
{
  struct template_value_t *value;
  struct js_t *js = JS_GetContextPrivate (cx);
  JSObject *array = JS_NewArrayObject (cx, 0, NULL);

  JSString *str;
  jsval val;

  value = template_value_new (js->widget);

  if (value->value)
    {
      str = JS_NewStringCopyN (cx, value->value, strlen (value->value));
      val = STRING_TO_JSVAL (str);
      JS_SetElement (cx, array, 0, &val);
    }
  else
    {
      str = JS_NewStringCopyN (cx, "", 0);
      val = STRING_TO_JSVAL (str);
      JS_SetElement (cx, array, 0, &val);
    }

  if (value->lang)
    {
      str = JS_NewStringCopyN (cx, value->lang, strlen (value->lang));
      val = STRING_TO_JSVAL (str);
      JS_SetElement (cx, array, 1, &val);
    }
  else
    {
      str = JS_NewStringCopyN (cx, "", 0);
      val = STRING_TO_JSVAL (str);
      JS_SetElement (cx, array, 1, &val);
    }

  if (value->datatype)
    {
      str = JS_NewStringCopyN (cx, value->datatype, strlen (value->datatype));
      val = STRING_TO_JSVAL (str);
      JS_SetElement (cx, array, 2, &val);
    }
  else
    {
      str = JS_NewStringCopyN (cx, "", 0);
      val = STRING_TO_JSVAL (str);
      JS_SetElement (cx, array, 2, &val);
    }

  val = DOUBLE_TO_JSVAL (value->type);
  JS_SetElement (cx, array, 3, &val);
  template_value_free (value);

  *rval = OBJECT_TO_JSVAL (array);
  return JS_TRUE;
}

static JSBool
js_set_value (JSContext * cx, JSObject * obj, uintN argc, jsval * argv,
	      jsval * rval)
{
  struct template_value_t *value;
  struct js_t *js = JS_GetContextPrivate (cx);
  JSObject *array;
  JSString *str;
  JSBool found;
  jsval val;

  if (!JS_IsArrayObject (cx, JSVAL_TO_OBJECT (argv[0])))
    {
      JS_ReportError (cx, "morla_set_value: the argument is not an array");
      return JS_FALSE;
    }

  array = JSVAL_TO_OBJECT (argv[0]);
  value = g_malloc0 (sizeof (struct template_value_t));

#define JS_SET_VALUE( x , y ) \
  if (JS_HasElement(cx, array, y, &found) == JS_TRUE && \
      found == JS_TRUE && \
      JS_GetElement (cx, array, y, &val) && \
      (JSVAL_IS_STRING (val) || JSVAL_IS_NUMBER (val)) && \
      (str = JS_ValueToString (cx, val))) \
      value->x = g_strdup (JS_GetStringBytes (str)); \
  else \
    value->x = NULL;

  JS_SET_VALUE (value, 0);
  JS_SET_VALUE (lang, 1);
  JS_SET_VALUE (datatype, 2);

  if (JS_HasElement (cx, array, 3, &found) == JS_TRUE && found == JS_TRUE
      && JS_GetElement (cx, array, 3, &val) && JSVAL_IS_NUMBER (val))
    value->type = (gint) JSVAL_TO_DOUBLE (val);

  template_value_set (js->widget, value);
  template_value_free (value);

  return JS_TRUE;
}

#endif
#endif

/* EOF */

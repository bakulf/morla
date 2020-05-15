/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
# error Use configure; make; make install
#endif

#include "editor.h"

typedef struct
{
  GtkWidget *splash;
  gboolean close_on_click;

  GtkWidget *logo_area;
  GdkPixmap *logo_pixmap;
  GdkRectangle pixmaparea;

  GdkBitmap *shape_bitmap;
  GdkGC *trans_gc;
  GdkGC *opaque_gc;

  GdkRectangle textarea;

  PangoLayout *layout;

  gint timer;

  gint index;
  gint animstep;
  gboolean visible;

} SplashScreen;

static PangoColor color_start = { 5000, 5000, 5000 };
static PangoColor color_end = { 51914, 51914, 51914 };

static SplashScreen splash_data = { 0, FALSE, };

static gboolean splash_load_logo (GtkWidget * window);
static void splash_destroy (GtkObject * object, gpointer data);
static void splash_unmap (GtkWidget * widget, gpointer data);
static void splash_center (GtkWindow * window);
static gboolean splash_logo_expose (GtkWidget * widget,
				    GdkEventExpose * event, gpointer data);
static gboolean splash_button_press (GtkWidget * widget,
				     GdkEventButton * event, gpointer data);
static gboolean splash_timer (gpointer data);
static const gchar *splash_path_image (void);

void
splash_show (void)
{
  GtkWidget *widget;
  GdkGCValues shape_gcv;
  gint w, h;

  if (splash_showed ())
    {
      g_message ("Showed?!?");
      return;
    }

  splash_data.visible = FALSE;
  splash_data.animstep = -1;
  splash_data.timer = -1;

  widget = g_object_new (GTK_TYPE_WINDOW,
			 "type", GTK_WINDOW_POPUP,
			 "title", _("Morla BootSplash"),
			 "window_position", GTK_WIN_POS_CENTER,
			 "resizable", FALSE, NULL);

  splash_data.splash = widget;

  gtk_window_set_role (GTK_WINDOW (widget), "splash-dialog");

  g_signal_connect (widget, "destroy", G_CALLBACK (splash_destroy), NULL);
  g_signal_connect (widget, "unmap", G_CALLBACK (splash_unmap), NULL);

  if (!splash_load_logo (widget))
    {
      gtk_widget_destroy (widget);
      return;
    }

  splash_center (GTK_WINDOW (widget));

  widget = gtk_drawing_area_new ();
  splash_data.logo_area = widget;

  gtk_widget_set_size_request (widget,
			       splash_data.pixmaparea.width,
			       splash_data.pixmaparea.height);
  gtk_widget_set_events (widget, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_container_add (GTK_CONTAINER (splash_data.splash), widget);
  gtk_widget_show (widget);

  g_signal_connect (widget, "expose_event",
		    G_CALLBACK (splash_logo_expose), NULL);

  g_signal_connect (widget, "button_press_event",
		    G_CALLBACK (splash_button_press), NULL);

  gtk_widget_realize (widget);
  gdk_window_set_background (widget->window, &(widget->style)->black);

  splash_data.shape_bitmap = gdk_pixmap_new (widget->window,
					     splash_data.pixmaparea.width,
					     splash_data.pixmaparea.height,
					     1);
  splash_data.trans_gc = gdk_gc_new (splash_data.shape_bitmap);

  splash_data.opaque_gc = gdk_gc_new (splash_data.shape_bitmap);
  gdk_gc_get_values (splash_data.opaque_gc, &shape_gcv);
  gdk_gc_set_foreground (splash_data.opaque_gc, &shape_gcv.background);

  gdk_draw_rectangle (splash_data.shape_bitmap,
		      splash_data.trans_gc,
		      TRUE,
		      0, 0,
		      splash_data.pixmaparea.width,
		      splash_data.pixmaparea.height);

  gdk_draw_line (splash_data.shape_bitmap,
		 splash_data.opaque_gc, 0, 0, splash_data.pixmaparea.width,
		 0);

  gtk_widget_shape_combine_mask (splash_data.splash,
				 splash_data.shape_bitmap, 0, 0);

  splash_data.layout =
    gtk_widget_create_pango_layout (splash_data.logo_area, NULL);
  g_object_weak_ref (G_OBJECT (splash_data.logo_area),
		     (GWeakNotify) g_object_unref, splash_data.layout);

  pango_layout_set_justify (splash_data.layout, PANGO_ALIGN_CENTER);
  pango_layout_get_pixel_size (splash_data.layout, &w, &h);

  splash_data.textarea.width = splash_data.pixmaparea.width;
  splash_data.textarea.height = h + 10;
  splash_data.textarea.x = 0;
  splash_data.textarea.y = (splash_data.pixmaparea.height -
			    splash_data.textarea.height);

  if (!gtk_widget_get_visible (splash_data.splash))
    {
      splash_data.index = 0;
      pango_layout_set_text (splash_data.layout, "", -1);
    }

  gtk_window_present (GTK_WINDOW (splash_data.splash));

  gtk_widget_show (splash_data.splash);

  while (splash_data.timer)
    {
      while (gtk_events_pending ())
	gtk_main_iteration ();

      g_usleep (500);
    }
}

void
splash_hide (void)
{
  if (splash_showed ())
    gtk_widget_destroy (splash_data.splash);
}

gboolean
splash_showed (void)
{
  return splash_data.splash ? TRUE : FALSE;
}

static void
splash_destroy (GtkObject * object, gpointer data)
{
  splash_data.splash = NULL;
  splash_unmap (NULL, NULL);
}

static void
splash_unmap (GtkWidget * widget, gpointer data)
{
  if (splash_data.timer > 0)
    {
      g_source_remove (splash_data.timer);
      splash_data.timer = 0;
    }

  if (splash_data.splash)
    {
      gdk_draw_rectangle (splash_data.shape_bitmap,
			  splash_data.trans_gc,
			  TRUE,
			  0, 0,
			  splash_data.pixmaparea.width,
			  splash_data.pixmaparea.height);

      gdk_draw_line (splash_data.shape_bitmap,
		     splash_data.opaque_gc,
		     0, 0, splash_data.pixmaparea.width, 0);

      gtk_widget_shape_combine_mask (splash_data.splash,
				     splash_data.shape_bitmap, 0, 0);
    }
}

static void
splash_center (GtkWindow * window)
{
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (window));
  GdkScreen *screen;
  GdkRectangle rect;
  gint monitor;
  gint x, y;

  gdk_display_get_pointer (display, &screen, &x, &y, NULL);

  monitor = gdk_screen_get_monitor_at_point (screen, x, y);
  gdk_screen_get_monitor_geometry (screen, monitor, &rect);

  gtk_window_set_screen (window, screen);
  gtk_window_move (window,
		   rect.x + (rect.width - splash_data.pixmaparea.width) / 2,
		   rect.y + (rect.height -
			     splash_data.pixmaparea.height) / 2);
}

static gboolean
splash_button_press (GtkWidget * widget, GdkEventButton * event,
		     gpointer data)
{
  if (splash_data.close_on_click)
    gtk_widget_destroy (splash_data.splash);
  return FALSE;
}

static gboolean
splash_logo_expose (GtkWidget * widget, GdkEventExpose * event, gpointer data)
{
  gint width, height;

  if (splash_data.timer < 0)
    {
      splash_data.index = 1;
      splash_data.animstep = -1;
      splash_data.visible = FALSE;
      gdk_draw_rectangle (splash_data.shape_bitmap,
			  splash_data.trans_gc,
			  TRUE,
			  0, 0,
			  splash_data.pixmaparea.width,
			  splash_data.pixmaparea.height);

      gdk_draw_line (splash_data.shape_bitmap,
		     splash_data.opaque_gc,
		     0, 0, splash_data.pixmaparea.width, 0);

      gtk_widget_shape_combine_mask (splash_data.splash,
				     splash_data.shape_bitmap, 0, 0);
      splash_data.timer = g_timeout_add (30, splash_timer, NULL);

    }

  /* only operate on the region covered by the pixmap */
  if (!gdk_rectangle_intersect (&(splash_data.pixmaparea),
				&(event->area), &(event->area)))
    return FALSE;

  gdk_gc_set_clip_rectangle (widget->style->black_gc, &(event->area));

  gdk_draw_drawable (widget->window,
		     widget->style->white_gc,
		     splash_data.logo_pixmap,
		     event->area.x, event->area.y,
		     event->area.x, event->area.y,
		     event->area.width, event->area.height);

  if (splash_data.visible == TRUE)
    {
      gint layout_x, layout_y;

      pango_layout_get_pixel_size (splash_data.layout, &width, &height);

      layout_x =
	splash_data.textarea.x + (splash_data.textarea.width - width) / 2;
      layout_y = splash_data.textarea.y + 5;

      gdk_draw_layout (widget->window,
		       widget->style->text_gc[GTK_STATE_NORMAL],
		       layout_x, layout_y, splash_data.layout);

    }

  gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);

  return FALSE;
}

static gchar *
insert_spacers (const gchar * string)
{
  gchar *normalized, *ptr;
  gunichar unichr;
  GString *str;

  str = g_string_new (NULL);

  normalized = g_utf8_normalize (string, -1, G_NORMALIZE_DEFAULT_COMPOSE);
  ptr = normalized;

  while ((unichr = g_utf8_get_char (ptr)))
    {
      g_string_append_unichar (str, unichr);
      g_string_append_unichar (str, 0x200b);	/* ZERO WIDTH SPACE */
      ptr = g_utf8_next_char (ptr);
    }

  g_free (normalized);

  return g_string_free (str, FALSE);
}

static void
mix_colors (PangoColor * start, PangoColor * end,
	    PangoColor * target, gdouble pos)
{
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (target != NULL);
  g_return_if_fail (pos >= 0.0 && pos <= 1.0);

  target->red = start->red + (end->red - start->red) * pos;
  target->green = start->green + (end->green - start->green) * pos;
  target->blue = start->blue + (end->blue - start->blue) * pos;
}

static void
decorate_text (PangoLayout * layout, gdouble time)
{
  gint text_length = 0;
  gint text_bytelen = 0;
  gint cluster_start, cluster_end;
  const gchar *text;
  const gchar *ptr;
  gunichar unichr;
  PangoAttrList *attrlist = NULL;
  PangoAttribute *attr;
  PangoRectangle irect = { 0, 0, 0, 0 };
  PangoRectangle lrect = { 0, 0, 0, 0 };
  PangoColor mix = { 0, 0, 0 };

  mix_colors (&color_start, &color_end, &mix, time);

  text = pango_layout_get_text (layout);
  text_length = g_utf8_strlen (text, -1);
  text_bytelen = strlen (text);

  attrlist = pango_attr_list_new ();

  attr = pango_attr_foreground_new (mix.red, mix.green, mix.blue);
  attr->start_index = 0;
  attr->end_index = text_bytelen;
  pango_attr_list_insert (attrlist, attr);

  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attr->start_index = 0;
  attr->end_index = text_bytelen;
  pango_attr_list_insert (attrlist, attr);

  ptr = text;

  cluster_start = 0;
  while ((unichr = g_utf8_get_char (ptr)))
    {
      ptr = g_utf8_next_char (ptr);
      cluster_end = (ptr - text);

      if (unichr == 0x200b)
	{
	  lrect.width = (1.0 - time) * 15.0 * PANGO_SCALE + 0.5;
	  attr = pango_attr_shape_new (&irect, &lrect);
	  attr->start_index = cluster_start;
	  attr->end_index = cluster_end;
	  pango_attr_list_change (attrlist, attr);
	}
      cluster_start = cluster_end;
    }

  pango_layout_set_attributes (layout, attrlist);
  pango_attr_list_unref (attrlist);
}

static gboolean
splash_timer (gpointer data)
{
  gint height;

  splash_data.visible = TRUE;

  if (splash_data.index > (splash_data.pixmaparea.height / 16) + 16)
    {
      splash_data.index = 0;
      splash_data.timer = 0;
      return FALSE;
    }

  height = splash_data.index * 16;

  if (height < splash_data.pixmaparea.height)
    gdk_draw_line (splash_data.shape_bitmap,
		   splash_data.opaque_gc,
		   0, height, splash_data.pixmaparea.width, height);

  height -= 15;
  while (height > 0)
    {
      if (height < splash_data.pixmaparea.height)
	gdk_draw_line (splash_data.shape_bitmap,
		       splash_data.opaque_gc,
		       0, height, splash_data.pixmaparea.width, height);
      height -= 15;
    }

  gtk_widget_shape_combine_mask (splash_data.splash,
				 splash_data.shape_bitmap, 0, 0);
  splash_data.index++;
  splash_data.visible = FALSE;
  gtk_widget_queue_draw_area (splash_data.logo_area,
			      0, 0,
			      splash_data.pixmaparea.width,
			      splash_data.pixmaparea.height);

  return TRUE;
}

static gboolean
splash_timer_effect (gpointer data)
{
  gboolean return_val = TRUE;

  splash_data.animstep++;

  if (splash_data.animstep < 20)
    decorate_text (splash_data.layout, ((float) splash_data.animstep) / 20.0);

  else
    {
      return_val = FALSE;
      splash_data.timer = 0;
    }

  gtk_widget_queue_draw_area (splash_data.logo_area,
			      splash_data.textarea.x,
			      splash_data.textarea.y,
			      splash_data.textarea.width,
			      splash_data.textarea.height);

  return return_val;
}

/* some handy shortcuts */
static gboolean
splash_load_logo (GtkWidget * window)
{
  GdkPixbuf *pixbuf;
  GdkGC *gc;

  if (splash_data.logo_pixmap)
    return TRUE;

  if (!(pixbuf = gdk_pixbuf_new_from_file (splash_path_image (), NULL)))
    return FALSE;

  splash_data.pixmaparea.x = 0;
  splash_data.pixmaparea.y = 0;
  splash_data.pixmaparea.width = gdk_pixbuf_get_width (pixbuf);
  splash_data.pixmaparea.height = gdk_pixbuf_get_height (pixbuf);

  gtk_widget_realize (window);

  splash_data.logo_pixmap = gdk_pixmap_new (window->window,
					    splash_data.pixmaparea.width,
					    splash_data.pixmaparea.height * 2,
					    gtk_widget_get_visual (window)->
					    depth);

  gc = gdk_gc_new (splash_data.logo_pixmap);

  /* draw a defined content to the Pixmap */
  gdk_draw_rectangle (GDK_DRAWABLE (splash_data.logo_pixmap),
		      gc, TRUE,
		      0, 0,
		      splash_data.pixmaparea.width,
		      splash_data.pixmaparea.height * 2);

  gdk_draw_pixbuf (GDK_DRAWABLE (splash_data.logo_pixmap),
		   gc, pixbuf,
		   0, 0, 0, 0,
		   splash_data.pixmaparea.width,
		   splash_data.pixmaparea.height, GDK_RGB_DITHER_NORMAL, 0,
		   0);

  g_object_unref (pixbuf);
  g_object_unref (gc);

  return TRUE;
}

void
splash_set_text (char *str, ...)
{
  char buf[1024];
  va_list va;
  PangoAttrList *attrlist = NULL;
  PangoAttribute *attr;

  if (!splash_showed ())
    return;

  va_start (va, str);
  g_vsnprintf (buf, sizeof (buf), str, va);
  va_end (va);

  pango_layout_set_text (splash_data.layout, buf, -1);

  attrlist = pango_attr_list_new ();

  attr =
    pango_attr_foreground_new (color_end.red, color_end.green,
			       color_end.blue);
  attr->start_index = 0;
  attr->end_index = strlen (buf);
  pango_attr_list_insert (attrlist, attr);

  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attr->start_index = 0;
  attr->end_index = strlen (buf);
  pango_attr_list_insert (attrlist, attr);

  pango_layout_set_attributes (splash_data.layout, attrlist);
  pango_attr_list_unref (attrlist);

  gtk_widget_queue_draw_area (splash_data.logo_area,
			      splash_data.textarea.x,
			      splash_data.textarea.y,
			      splash_data.textarea.width,
			      splash_data.textarea.height);

  while (gtk_events_pending ())
    gtk_main_iteration ();
}

void
splash_set_text_with_effect (char *str, ...)
{
  char buf[1024];
  va_list va;
  gchar *text;

  if (!splash_showed ())
    return;

  va_start (va, str);
  g_vsnprintf (buf, sizeof (buf), str, va);
  va_end (va);

  text = insert_spacers (buf);
  pango_layout_set_text (splash_data.layout, text, -1);
  g_free (text);

  pango_layout_set_attributes (splash_data.layout, NULL);
  splash_data.animstep = 0;

  splash_data.timer = g_timeout_add (30, splash_timer_effect, NULL);

  while (splash_data.timer)
    {
      while (gtk_events_pending ())
	gtk_main_iteration ();

      g_usleep (500);
    }
}

void
splash_sleep (gint timer)
{
  time_t t;

  if (!splash_showed ())
    return;

  t = time (NULL);

  while ((time (NULL) - t) <= timer)
    {
      while (gtk_events_pending ())
	gtk_main_iteration ();

      g_usleep (500);
    }
}

void
splash_close_on_click (gboolean value)
{
  splash_data.close_on_click = value;
}

static const gchar *
splash_path_image (void)
{
  static const gchar *path;
  static gboolean done = FALSE;

#ifndef ENABLE_MACOSX
  return PATH_IMAGE;
#endif

  if (done == FALSE)
    {
      path = g_getenv ("MORLA_PATH_IMAGE");
      done = TRUE;
    }

  if (!path)
    return PATH_IMAGE;

  return path;
}

/* EOF */

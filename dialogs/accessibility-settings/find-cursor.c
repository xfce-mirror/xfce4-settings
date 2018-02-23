/*
 *  Copyright (c) 2018 Simon Steinbeiß <simon@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <gdk/gdkx.h>
#include <math.h>

#include <X11/extensions/shape.h>
#include <gdk/gdkkeysyms.h>

#include <xfconf/xfconf.h>

/* global var to keep track of the circle size */
double px = 1;


gboolean timeout (gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (data);
    gtk_widget_queue_draw (widget);
    return TRUE;
}


static GdkPixbuf
*get_rectangle_screenshot (gint x, gint y, GtkWidget *widget)
{
    GdkPixbuf *screenshot = NULL;
    GdkWindow *root_window = gdk_get_default_root_window ();
    GdkColormap *colormap = gdk_colormap_get_system();

    screenshot =
        gdk_pixbuf_get_from_drawable (NULL, root_window, colormap,
                                      x,
                                      y,
                                      0, 0,
                                      500,
                                      500);
    return screenshot;
}



static gboolean
find_cursor_motion_notify_event (GtkWidget      *widget,
                                 GdkEventMotion *event,
                                 gpointer userdata)
{
    GdkWindow *window = gtk_widget_get_window (widget);
    gint x, y, root_x, root_y;

    gdk_window_get_pointer (window, &x, &y, NULL);
    gtk_window_get_position (GTK_WINDOW (widget), &root_x, &root_y);
    gtk_window_move (GTK_WINDOW (widget), root_x + x - 250, root_y + y - 250);
    return FALSE;
}


static gboolean
find_cursor_window_composited (GtkWidget *widget) {
    gboolean     composited;
    GdkScreen   *screen = gtk_widget_get_screen (widget);
    GdkColormap *colormap = gdk_screen_get_rgba_colormap (screen);

    if (gdk_screen_is_composited (screen))
       composited = TRUE;
    else
    {
       colormap = gdk_screen_get_rgb_colormap (screen);
       composited = FALSE;
    }

    gtk_widget_set_colormap (widget, colormap);
    return composited;
}

GdkPixbuf *pixbuf = NULL;

static gboolean
find_cursor_window_expose (GtkWidget *widget,
                           GdkEvent  *event,
                           gpointer   user_data) {
    cairo_t *cr;
    GdkWindow *window = gtk_widget_get_window (widget);
    int width, height;
    gint x, y, root_x, root_y;
    int i = 0;
    int arcs = 1;
    gboolean composited = GPOINTER_TO_INT (user_data);

    gtk_widget_get_size_request (widget, &width, &height);
    gdk_window_get_pointer (window, &x, &y, NULL);
    gtk_window_get_position (GTK_WINDOW (widget), &root_x, &root_y);

    cr = gdk_cairo_create (window);
    if (composited) {
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    }
    else {
        /* only take a screenshot once in the first iteration */
        if (px == 1) {
            pixbuf = get_rectangle_screenshot (root_x + x - 250 + 1, root_y + y - 250 , widget);
            if (!pixbuf)
                g_warning("Getting screenshot failed");
        }

        if (pixbuf) {
    	    /* FIXME: use 0,0 as coordinates */
            gdk_cairo_set_source_pixbuf (cr, pixbuf, 1, 0);
        }
    }

    cairo_paint (cr);

    cairo_set_line_width (cr, 3.0);
    cairo_translate (cr, width / 2, height / 2);

    if (px > 90.0)
        arcs = 4;
    else if (px > 60.0)
        arcs = 3;
    else if (px > 30.0)
        arcs = 2;

    /* draw fill */
    cairo_arc (cr, 0, 0, px, 0, 2 * M_PI);
    cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.5);
    cairo_fill (cr);

    /* draw several arcs, depending on the radius */
    cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
    for (i = 0; i < arcs; i++) {
        cairo_arc (cr, 0, 0, px - (i * 30.0), 0, 2 * M_PI);
        cairo_stroke (cr);
    }

    /* stop before the circles get bigger than the window */
    if (px > 200) {
        if (pixbuf)
            g_object_unref (pixbuf);
        gtk_main_quit();
    }

    px += 3;

    return FALSE;
}


gint
main (gint argc, gchar **argv)
{
    XfconfChannel *accessibility_channel = NULL;
    GError        *error = NULL;
    GtkWidget     *window;
    GdkWindow     *root_window;
    gint           x,y;
    gboolean       composited;

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the channels */
    accessibility_channel = xfconf_channel_new ("accessibility");

    if (xfconf_channel_get_bool (accessibility_channel, "/FindCursor", TRUE))
        g_warning ("continue");
    else
        return 0;

    gtk_init (&argc, &argv);

    /* just get the position of the mouse cursor */
    root_window = gdk_get_default_root_window ();
    gdk_window_get_pointer (root_window, &x, &y, NULL);

    /* popup tells the wm to ignore if parts of the window are offscreen */
    window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);
    gtk_widget_set_size_request (window, 500, 500);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_widget_set_app_paintable (window, TRUE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);

    /* center the window around the mouse cursor */
    gtk_window_move (GTK_WINDOW (window), x - 250, y - 250);

    /* check if we're in a composited environment */
    composited = find_cursor_window_composited (window);

    /* make the circles follow the mouse cursor */
    if (composited) {
        gtk_widget_set_events (window, GDK_POINTER_MOTION_MASK);
        g_signal_connect (G_OBJECT (window), "motion-notify-event",
                          G_CALLBACK (find_cursor_motion_notify_event), NULL);
    }
    g_signal_connect (G_OBJECT (window), "expose-event",
                      G_CALLBACK (find_cursor_window_expose), GINT_TO_POINTER (composited));
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (gtk_main_quit), NULL);


    gtk_widget_show_all (window);

    g_timeout_add (10, timeout, window);

    gtk_main ();

    return 0;
}

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

#include <xfconf/xfconf.h>

/* global var to keep track of the circle size */
double px = 10;


gboolean timeout (gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (data);
    gtk_widget_queue_draw (widget);
    return TRUE;
}


static void
find_cursor_window_screen_changed (GtkWidget *widget,
                                   GdkScreen *old_screen,
                                   gpointer userdata) {
    gboolean     supports_alpha;
    GdkScreen   *screen = gtk_widget_get_screen (widget);
    GdkColormap *colormap = gdk_screen_get_rgba_colormap (screen);

    if (gdk_screen_is_composited (screen))
    {
       supports_alpha = TRUE;
       g_warning ("Your screen supports alpha!");
    }
    else
    {
       colormap = gdk_screen_get_rgb_colormap (screen);
       supports_alpha = FALSE;
    }

    gtk_widget_set_colormap (widget, colormap);
}


static gboolean
find_cursor_window_expose (GtkWidget *widget,
                           GdkEvent  *event,
                           gpointer   user_data) {
    cairo_t *cr;
    GdkWindow *window = gtk_widget_get_window (widget);
    int width, height;
    int i = 0;
    int arcs = 1;

    gtk_widget_get_size_request (widget, &width, &height);

    cr = gdk_cairo_create (window);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
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
    if (px > 200)
        gtk_main_quit();

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

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);
    gtk_widget_set_size_request (window, 500, 500);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_widget_set_app_paintable (window, TRUE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
    /* tell the wm to ignore if parts of the window are offscreen */
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DOCK);
    /* center the window around the mouse cursor */
    gtk_window_move (GTK_WINDOW (window), x - 250, y - 250);

    g_signal_connect (G_OBJECT (window), "expose-event",
                      G_CALLBACK (find_cursor_window_expose), NULL);
    g_signal_connect (G_OBJECT(window), "screen-changed",
                      G_CALLBACK (find_cursor_window_screen_changed), NULL);
    g_signal_connect (G_OBJECT(window), "destroy",
                      G_CALLBACK(gtk_main_quit), NULL);
    find_cursor_window_screen_changed (window, NULL, NULL);
    gtk_widget_show_all (window);

    g_timeout_add (10, timeout, window);

    gtk_main ();

    return 0;
}

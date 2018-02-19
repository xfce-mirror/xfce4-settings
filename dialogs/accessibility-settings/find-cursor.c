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
#include <gtk/gtkx.h>

#include <gdk/gdkx.h>
#include <math.h>


gboolean timeout (gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (data);
    gtk_widget_queue_draw (widget);
    return TRUE;
}


static void
screen_changed (GtkWidget *widget, GdkScreen *old_screen, gpointer userdata) {
    gboolean supports_alpha;
    GdkScreen *screen = gtk_widget_get_screen(widget);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

    /* this is a purely informatonal check at the moment. could later be user_data
     * to call some fallback in non-composited envs */
    if (!visual) {
        g_warning ("Your screen does not support alpha channels!");
        visual = gdk_screen_get_system_visual(screen);
        supports_alpha = FALSE;
    } else {
        g_warning ("Your screen supports alpha channels!");
        supports_alpha = TRUE;
    }

    gtk_widget_set_visual (widget, visual);
}

double px = 10;
double vx = 3;

static gboolean
find_cursor_window_draw  (GtkWidget      *window,
                          cairo_t *cr,
                          gpointer     user_data) {
    int width, height;
    int i = 1;
    int arcs = 1;

    gtk_window_get_size (GTK_WINDOW (window), &width, &height);

    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    cairo_paint (cr);

    cairo_set_line_width (cr, 3.0);
    cairo_translate (cr, width / 2, height / 2);
    if (px > 30.0)
        arcs = 2;
    if (px > 60.0)
        arcs = 3;
    if (px > 90.0)
        arcs = 4;

    for (i = 1; i <= arcs; i++) {
      cairo_arc (cr, 0, 0, px - ((i - 1) * 30.0), 0, 2 * M_PI);
    }
    cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.5);
    cairo_fill_preserve (cr);
    cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
    cairo_stroke (cr);

    /* stop before the circles get bigger than the window */
    if (px <= 3 || px >= 200-3)
        gtk_main_quit();
    px += vx;

    return FALSE;
}


gint
main (gint argc, gchar **argv)
{
    gtk_init (&argc, &argv);

    GtkWidget     *window;
    GdkDisplay    *display = gdk_display_get_default ();
    GdkSeat       *seat = gdk_display_get_default_seat (display);
    GdkDevice     *device = gdk_seat_get_pointer (seat);
    GdkScreen     *screen = gdk_screen_get_default ();
    gint           x,y;

    // just get the position of the mouse cursor
    gdk_device_get_position (device, &screen, &x, &y);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);
    gtk_widget_set_size_request (window, 500, 500);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_widget_set_app_paintable (window, TRUE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);

    /* this results in the same as getting the mouse cursor and setting the positions
       of the window with gtk_window_move */
    //gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_MOUSE);
    gtk_window_set_gravity (GTK_WINDOW (window), GDK_GRAVITY_NORTH_WEST);
    gtk_window_move (GTK_WINDOW (window), x - 250, y - 250);

    g_signal_connect (G_OBJECT (window), "draw",
                      G_CALLBACK (find_cursor_window_draw), NULL);
    g_signal_connect (G_OBJECT(window), "screen-changed", G_CALLBACK (screen_changed), NULL);
    g_signal_connect (G_OBJECT(window), "destroy",
                      G_CALLBACK(gtk_main_quit), NULL);
    screen_changed (window, NULL, NULL);
    gtk_widget_show_all (window);

    g_timeout_add (10, timeout, window);

    gtk_main ();

    return 0;
}

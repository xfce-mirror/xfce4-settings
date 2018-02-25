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

/* size of the final circles */
gint circle_size = 500;

GdkPixbuf *pixbuf = NULL;
gint screenshot_offset_x, screenshot_offset_y;

/* gdk_cairo_set_source_pixbuf() crashes with 0,0 */
gint workaround_offset = 1;

gboolean timeout (gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (data);
    gtk_widget_queue_draw (widget);
    return TRUE;
}


static GdkPixbuf
*get_rectangle_screenshot (gint x, gint y)
{
    GdkPixbuf *screenshot = NULL;
    GdkWindow *root_window = gdk_get_default_root_window ();
    GdkColormap *colormap = gdk_colormap_get_system();
    gint width = circle_size + workaround_offset;
    gint height = circle_size + workaround_offset;

    /* cut down screenshot if it's out of bounds */
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }

    screenshot =
        gdk_pixbuf_get_from_drawable (NULL, root_window, colormap,
                                      x,
                                      y,
                                      0, 0,
                                      width,
                                      height);
    return screenshot;
}



static gboolean
find_cursor_motion_notify_event (GtkWidget      *widget,
                                 GdkEventMotion *event,
                                 gpointer userdata)
{
    gtk_window_move (GTK_WINDOW (widget), event->x_root - (circle_size/2), event->y_root - (circle_size/2));
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



static gboolean
find_cursor_window_expose (GtkWidget       *widget,
                           GdkEventExpose  *event,
                           gpointer         user_data) {
    cairo_t *cr;
    int i = 0;
    int arcs = 1;
    gboolean composited = GPOINTER_TO_INT (user_data);

    cr = gdk_cairo_create (event->window);
    if (composited) {
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    }
    else {
        if (pixbuf) {
            if (screenshot_offset_x > 0) screenshot_offset_x = 0;
            if (screenshot_offset_y > 0) screenshot_offset_y = 0;

            gdk_cairo_set_source_pixbuf (cr, pixbuf, 0 - screenshot_offset_x - workaround_offset, 0 - screenshot_offset_y - workaround_offset);
        }
        else
            g_warning ("Something with the screenshot went wrong.");
    }

    cairo_paint (cr);

    cairo_set_line_width (cr, 3.0);
    cairo_translate (cr, circle_size / 2, circle_size / 2);

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
    if (px >= (circle_size/2)) {
        if (pixbuf)
            g_object_unref (pixbuf);
        gtk_main_quit ();
    }

    px += 3;

    cairo_destroy (cr);

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
    gtk_window_set_default_size (GTK_WINDOW (window), circle_size, circle_size);
    gtk_widget_set_size_request (GTK_WIDGET (window), circle_size, circle_size);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);

    /* center the window around the mouse cursor */
    gtk_window_move (GTK_WINDOW (window), x - (circle_size/2), y - (circle_size/2));

    /* check if we're in a composited environment */
    composited = find_cursor_window_composited (GTK_WIDGET (window));

    /* make the circles follow the mouse cursor */
    if (composited) {
        gtk_widget_set_events (GTK_WIDGET (window), GDK_POINTER_MOTION_MASK);
        g_signal_connect (G_OBJECT (window), "motion-notify-event",
                          G_CALLBACK (find_cursor_motion_notify_event), NULL);
    }
    else {
        /* this offset has to match the screenshot */
        screenshot_offset_x = x - (circle_size/2) - workaround_offset;
        screenshot_offset_y = y - (circle_size/2) - workaround_offset;

        pixbuf = get_rectangle_screenshot (screenshot_offset_x, screenshot_offset_y);
        if (!pixbuf)
            g_warning("Getting screenshot failed");
    }
    g_signal_connect (G_OBJECT (window), "expose-event",
                      G_CALLBACK (find_cursor_window_expose), GINT_TO_POINTER (composited));
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (gtk_main_quit), NULL);


    gtk_widget_show_all (GTK_WIDGET (window));

    g_timeout_add (10, timeout, window);

    gtk_main ();

    xfconf_shutdown();

    return 0;
}

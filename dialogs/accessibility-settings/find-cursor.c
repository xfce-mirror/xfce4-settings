/*
 *  Copyright (c) 2019 Simon Steinbeiß <simon@xfce.org>
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <xfconf/xfconf.h>

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#ifdef ENABLE_X11
#include <gdk/gdkx.h>

/* size of the window and circles */
#define CIRCLE_SIZE 500
#define CIRCLE_RADIUS 250
#endif

#ifdef ENABLE_WAYLAND
#include "protocols/xfce-accessibility-v1-client.h"

#include <gdk/gdkwayland.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#define XFCE_ACCESSIBILITY_VERSION 1
#endif

#ifdef ENABLE_X11

/* global var to keep track of the circle size */
double px = 1;

GdkPixbuf *pixbuf = NULL;
gint screenshot_offset_x, screenshot_offset_y;

/* gdk_cairo_set_source_pixbuf() crashes with 0,0 */
gint workaround_offset = 1;



static gboolean
timeout (gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (data);
    gtk_widget_queue_draw (widget);
    return TRUE;
}



static GdkPixbuf *
get_rectangle_screenshot (gint x,
                          gint y)
{
    GdkPixbuf *screenshot = NULL;
    GdkWindow *root_window = gdk_get_default_root_window ();
    gint width = CIRCLE_SIZE + workaround_offset;
    gint height = width;

    /* cut down screenshot if it's out of bounds */
    if (x < 0)
    {
        width += x;
        x = 0;
    }
    if (y < 0)
    {
        height += y;
        y = 0;
    }

    screenshot = gdk_pixbuf_get_from_window (root_window,
                                             x, y,
                                             width, height);
    return screenshot;
}



static gboolean
find_cursor_motion_notify_event (GtkWidget *widget,
                                 GdkEventMotion *event,
                                 gpointer userdata)
{
    gtk_window_move (GTK_WINDOW (widget), event->x_root - CIRCLE_RADIUS, event->y_root - CIRCLE_RADIUS);
    return FALSE;
}



static gboolean
find_cursor_window_composited (GtkWidget *widget)
{
    GdkScreen *screen = gtk_widget_get_screen (widget);
    GdkVisual *visual = gdk_screen_get_rgba_visual (screen);
    gboolean composited;

    if (gdk_screen_is_composited (screen))
        composited = TRUE;
    else
    {
        visual = gdk_screen_get_system_visual (screen);
        composited = FALSE;
    }

    gtk_widget_set_visual (widget, visual);
    return composited;
}



static void
find_cursor_window_destroy (GtkWidget *widget,
                            gpointer user_data)
{
    if (pixbuf)
        g_object_unref (pixbuf);
    gtk_main_quit ();
}



static gboolean
find_cursor_window_draw (GtkWidget *widget,
                         cairo_t *cr,
                         gpointer user_data)
{
    int i = 0;
    int arcs = 1;
    gboolean composited = GPOINTER_TO_INT (user_data);

    if (composited)
    {
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    }
    else
    {
        if (pixbuf)
        {
            if (screenshot_offset_x > 0)
                screenshot_offset_x = 0;
            if (screenshot_offset_y > 0)
                screenshot_offset_y = 0;

            gdk_cairo_set_source_pixbuf (cr, pixbuf,
                                         0 - screenshot_offset_x - workaround_offset,
                                         0 - screenshot_offset_y - workaround_offset);
        }
        else
            g_warning ("Something with the screenshot went wrong.");
    }
    cairo_paint (cr);

    cairo_set_line_width (cr, 3.0);
    cairo_translate (cr, CIRCLE_RADIUS, CIRCLE_RADIUS);

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
    for (i = 0; i < arcs; i++)
    {
        cairo_arc (cr, 0, 0, px - (i * 30.0), 0, 2 * M_PI);
        cairo_stroke (cr);
    }

    /* stop before the circles get bigger than the window */
    if (px >= CIRCLE_RADIUS)
    {
        find_cursor_window_destroy (widget, NULL);
    }

    px += 3;

    return FALSE;
}


static int
show_x11 (GdkSeat *seat)
{
    GtkWidget *window;
    GdkScreen *screen;
    GdkDevice *device;
    gint x, y;
    gboolean composited;

    /* just get the position of the mouse cursor */
    device = gdk_seat_get_pointer (seat);
    screen = gdk_screen_get_default ();
    gdk_device_get_position (device, &screen, &x, &y);

    /* popup tells the wm to ignore if parts of the window are offscreen */
    window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
    gtk_window_set_default_size (GTK_WINDOW (window), CIRCLE_SIZE, CIRCLE_SIZE);
    gtk_widget_set_size_request (GTK_WIDGET (window), CIRCLE_SIZE, CIRCLE_SIZE);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);

    /* center the window around the mouse cursor */
    gtk_window_move (GTK_WINDOW (window), x - CIRCLE_RADIUS, y - CIRCLE_RADIUS);

    /* check if we're in a composited environment */
    composited = find_cursor_window_composited (GTK_WIDGET (window));

    /* with compositor: make the circles follow the mouse cursor */
    if (composited)
    {
        gtk_widget_set_events (GTK_WIDGET (window), GDK_POINTER_MOTION_MASK);
        g_signal_connect (G_OBJECT (window), "motion-notify-event",
                          G_CALLBACK (find_cursor_motion_notify_event), NULL);
    }
    /* fake transparency by creating a screenshot and using that as window bg */
    else
    {
        /* this offset has to match the screenshot */
        screenshot_offset_x = x - CIRCLE_RADIUS - workaround_offset;
        screenshot_offset_y = y - CIRCLE_RADIUS - workaround_offset;

        pixbuf = get_rectangle_screenshot (screenshot_offset_x, screenshot_offset_y);
        if (!pixbuf)
            g_warning ("Getting screenshot failed");
    }

    g_signal_connect (G_OBJECT (window), "draw",
                      G_CALLBACK (find_cursor_window_draw), GINT_TO_POINTER (composited));
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (find_cursor_window_destroy), NULL);


    gtk_widget_show_all (GTK_WIDGET (window));

    g_timeout_add (10, timeout, window);

    gtk_main ();

    return EXIT_SUCCESS;
}

#endif /* ENABLE_X11 */

#ifdef ENABLE_WAYLAND

static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t name,
                 const char *interface,
                 uint32_t version)
{

    if (g_strcmp0 (interface, xfce_accessibility_manager_private_v1_interface.name) == 0)
    {
        struct xfce_accessibility_manager_private_v1 **accessibility_manager = data;
        *accessibility_manager = wl_registry_bind (registry,
                                                   name,
                                                   &xfce_accessibility_manager_private_v1_interface,
                                                   XFCE_ACCESSIBILITY_VERSION);
    }
}



static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t name)
{
}



static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};



static int
show_wayland (GdkDisplay *display,
              GdkSeat *seat)
{
    struct wl_display *wl_display = gdk_wayland_display_get_wl_display (display);
    struct wl_registry *registry = wl_display_get_registry (wl_display);

    struct xfce_accessibility_manager_private_v1 *accessibility_manager = NULL;
    wl_registry_add_listener (registry, &registry_listener, &accessibility_manager);

    wl_display_roundtrip (wl_display);

    if (accessibility_manager == NULL)
    {
        g_message ("Your compositor does not support the xfce-accessibility-manager-private-v1 protocol");
    }
    else
    {
        struct wl_seat *wl_seat = gdk_wayland_seat_get_wl_seat (seat);

        xfce_accessibility_manager_private_v1_find_pointer (accessibility_manager, wl_seat);
        xfce_accessibility_manager_private_v1_destroy (accessibility_manager);
    }

    wl_registry_destroy (registry);
    gdk_display_flush (display);

    return accessibility_manager != NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif /* ENABLE_WAYLAND */



gint
main (gint argc,
      gchar **argv)
{
    XfconfChannel *accessibility_channel = NULL;
    GError *error = NULL;
    GdkDisplay *display;
    GdkSeat *seat;
    int exitcode = EXIT_FAILURE;

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_critical ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the channels */
    accessibility_channel = xfconf_channel_new ("accessibility");

    /* don't do anything if the /FindCursor setting is not enabled */
    if (!xfconf_channel_get_bool (accessibility_channel, "/FindCursor", TRUE))
        return EXIT_SUCCESS;

    gtk_init (&argc, &argv);

    display = gdk_display_get_default ();
    seat = gdk_display_get_default_seat (display);

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (display))
    {
        exitcode = show_x11 (seat);
    }
#endif

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (display))
    {
        exitcode = show_wayland (display, seat);
    }
#endif

    xfconf_shutdown ();

    return exitcode;
}

/*
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

#include "displays-iio.h"
#include "glib.h"

#include "common/debug.h"
#include "gio/gio.h"



static void
xfce_displays_iio_dispose (GObject *object);

static void
xfce_displays_iio_clear (XfceDisplaysIIO *iio);



struct _XfceDisplaysIIOClass
{
    GObjectClass __parent__;

    void (*orientation_changed) (XfceDisplaysIIO *iio, gint rotation);
};

struct _XfceDisplaysIIO
{
    GObject __parent__;

    guint iio_watch_id;
    GDBusProxy *iio_proxy;
    gulong handler;

    gint target_rotation;
};

enum
{
    ORIENTATION_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };



G_DEFINE_TYPE (XfceDisplaysIIO, xfce_displays_iio, G_TYPE_OBJECT);



static void
xfce_displays_iio_class_init (XfceDisplaysIIOClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->dispose = xfce_displays_iio_dispose;

    signals[ORIENTATION_CHANGED] =
        g_signal_new ("orientation-changed",
                      XFCE_TYPE_DISPLAYS_IIO,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XfceDisplaysIIOClass, orientation_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE, 1, G_TYPE_INT);
}



static void
xfce_displays_iio_update_orientation (XfceDisplaysIIO *iio)
{
    g_autofree gchar *orientation = NULL;
    GVariant *v = g_dbus_proxy_get_cached_property (iio->iio_proxy, "AccelerometerOrientation");
    gint rotation = 0;

    if (!v)
        return;

    orientation = g_variant_dup_string (v, NULL);
    g_variant_unref (v);

    if (g_strcmp0 (orientation, "normal") == 0)
        rotation = 0;
    else if (g_strcmp0 (orientation, "bottom-up") == 0)
        rotation = 180;
    else if (g_strcmp0 (orientation, "left-up") == 0)
        rotation = 90;
    else if (g_strcmp0 (orientation, "right-up") == 0)
        rotation = 270;
    else
        return;

    iio->target_rotation = rotation;
    g_signal_emit (G_OBJECT (iio), signals[ORIENTATION_CHANGED], 0, iio->target_rotation);
}



static void
xfce_displays_iio_sensor_properties_changed (GDBusProxy *proxy,
                                             GVariant *changed_properties,
                                             GStrv invalidated_properties,
                                             gpointer data)
{
    XfceDisplaysIIO *iio = XFCE_DISPLAYS_IIO (data);

    if (g_variant_lookup (changed_properties, "AccelerometerOrientation", "s", NULL))
        xfce_displays_iio_update_orientation (iio);
}



static void
xfce_displays_iio_sensor_appeared_cb (GDBusConnection *connection,
                                      const gchar *name,
                                      const gchar *name_owner,
                                      gpointer user_data)
{
    XfceDisplaysIIO *iio = XFCE_DISPLAYS_IIO (user_data);

    g_autoptr (GError) error = NULL;

    xfce_displays_iio_clear (iio);

    iio->iio_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    "net.hadess.SensorProxy",
                                                    "/net/hadess/SensorProxy",
                                                    "net.hadess.SensorProxy",
                                                    NULL, &error);

    if (error != NULL)
    {
        g_warning ("Failed to claim accelerometer: %s", error->message);
        return;
    }

    iio->handler = g_signal_connect (G_OBJECT (iio->iio_proxy), "g-properties-changed",
                                     G_CALLBACK (xfce_displays_iio_sensor_properties_changed), iio);

    g_dbus_proxy_call_sync (iio->iio_proxy,
                            "ClaimAccelerometer", NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1, NULL, &error);

    if (error != NULL)
    {
        g_warning ("Failed to claim accelerometer: %s", error->message);
        return;
    }

    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "iio-sensor-proxy detected on D-Bus.");

    xfce_displays_iio_update_orientation (iio);
}



static void
xfce_displays_iio_clear (XfceDisplaysIIO *iio)
{
    if (iio->handler > 0)
    {
        g_signal_handler_disconnect (G_OBJECT (iio->iio_proxy),
                                     iio->handler);
        iio->handler = 0;
    }

    if (iio->iio_proxy)
    {
        g_dbus_proxy_call (iio->iio_proxy,
                           "ReleaseAccelerometer", NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);

        g_clear_object (&iio->iio_proxy);
    }
}



static void
xfce_displays_iio_sensor_vanished_cb (GDBusConnection *connection,
                                      const gchar *name,
                                      gpointer data)
{
    XfceDisplaysIIO *iio = XFCE_DISPLAYS_IIO (data);
    xfce_displays_iio_clear (iio);
}



static void
xfce_displays_iio_init (XfceDisplaysIIO *iio)
{
    iio->iio_watch_id = g_bus_watch_name (
        G_BUS_TYPE_SYSTEM,
        "net.hadess.SensorProxy",
        G_BUS_NAME_WATCHER_FLAGS_NONE,
        xfce_displays_iio_sensor_appeared_cb,
        xfce_displays_iio_sensor_vanished_cb,
        iio, NULL);
}



static void
xfce_displays_iio_dispose (GObject *object)
{
    XfceDisplaysIIO *iio = XFCE_DISPLAYS_IIO (object);

    if (iio->iio_watch_id > 0)
    {
        g_bus_unwatch_name (iio->iio_watch_id);
        iio->iio_watch_id = 0;
    }

    xfce_displays_iio_clear (iio);

    G_OBJECT_CLASS (xfce_displays_iio_parent_class)->dispose (object);
}

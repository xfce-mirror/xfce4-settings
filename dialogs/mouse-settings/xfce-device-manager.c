/*
 *  Copyright (c) 2026 Brian Tarricone <brian@tarricone.org>
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

#include <gio/gio.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

#ifdef ENABLE_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "xfce-device-manager.h"

#ifdef ENABLE_X11
#include "xfce-device-manager-x11.h"
#endif

#ifdef ENABLE_WAYLAND
#include "xfce-device-manager-wayland.h"
#endif

#define GET_PRIV(manager) ((XfceDeviceManagerPrivate *) xfce_device_manager_get_instance_private (XFCE_DEVICE_MANAGER (manager)))

typedef struct _XfceDeviceManagerPrivate
{
    GdkDisplay *display;
    XfconfChannel *channel;
    GList *devices;
} XfceDeviceManagerPrivate;

enum
{
    PROP_0,
    PROP_DISPLAY,
    PROP_CHANNEL,
};

enum
{
    DEVICE_ADDED,
    DEVICE_REMOVED,
    N_SIGNALS,
};

static guint signals[N_SIGNALS];

static void
xfce_device_manager_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec);
static void
xfce_device_manager_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec);
static void
xfce_device_manager_finalize (GObject *object);


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (XfceDeviceManager, xfce_device_manager, G_TYPE_OBJECT)


static void
xfce_device_manager_class_init (XfceDeviceManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = xfce_device_manager_set_property;
    gobject_class->get_property = xfce_device_manager_get_property;
    gobject_class->finalize = xfce_device_manager_finalize;

    g_object_class_install_property (gobject_class,
                                     PROP_DISPLAY,
                                     g_param_spec_object ("display",
                                                          "display",
                                                          "display",
                                                          GDK_TYPE_DISPLAY,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_CHANNEL,
                                     g_param_spec_object ("channel",
                                                          "channel",
                                                          "channel",
                                                          XFCONF_TYPE_CHANNEL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    signals[DEVICE_ADDED] = g_signal_new ("device-added",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE, 1, XFCE_TYPE_DEVICE);

    signals[DEVICE_REMOVED] = g_signal_new ("device-removed",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_LAST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE, 1, XFCE_TYPE_DEVICE);
}

static void
xfce_device_manager_init (XfceDeviceManager *manager)
{
}

static void
xfce_device_manager_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    XfceDeviceManagerPrivate *manager = GET_PRIV (object);

    switch (property_id)
    {
        case PROP_DISPLAY:
            g_set_object (&manager->display, g_value_get_object (value));
            break;

        case PROP_CHANNEL:
            g_set_object (&manager->channel, g_value_get_object (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_manager_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    XfceDeviceManagerPrivate *manager = GET_PRIV (object);

    switch (property_id)
    {
        case PROP_DISPLAY:
            g_value_set_object (value, manager->display);
            break;

        case PROP_CHANNEL:
            g_value_set_object (value, manager->channel);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_manager_finalize (GObject *object)
{
    XfceDeviceManagerPrivate *manager = GET_PRIV (object);

    g_list_free_full (manager->devices, g_object_unref);
    g_clear_object (&manager->channel);
    g_clear_object (&manager->display);

    G_OBJECT_CLASS (xfce_device_manager_parent_class)->finalize (object);
}

GList *
xfce_device_manager_list_devices (XfceDeviceManager *manager)
{
    return GET_PRIV (manager)->devices;
}

GdkDisplay *
xfce_device_manager_get_display (XfceDeviceManager *manager)
{
    return GET_PRIV (manager)->display;
}

XfconfChannel *
xfce_device_manager_get_channel (XfceDeviceManager *manager)
{
    return GET_PRIV (manager)->channel;
}

void
_xfce_device_manager_add_device (XfceDeviceManager *manager,
                                 XfceDevice *device)
{
    XfceDeviceManagerPrivate *priv = GET_PRIV (manager);
    priv->devices = g_list_append (priv->devices, device);
    g_signal_emit (manager, signals[DEVICE_ADDED], 0, device);
}

void
_xfce_device_manager_remove_device (XfceDeviceManager *manager,
                                    XfceDevice *device)
{
    XfceDeviceManagerPrivate *priv = GET_PRIV (manager);
    priv->devices = g_list_remove (priv->devices, device);
    g_signal_emit (manager, signals[DEVICE_REMOVED], 0, device);
    g_object_unref (device);
}

XfceDeviceManager *
xfce_device_manager_new (GdkDisplay *display,
                         XfconfChannel *channel,
                         GError **error)
{
#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (display))
    {
        return xfce_device_manager_x11_new (display, channel, error);
    }
#endif

#ifdef ENABLE_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (display))
    {
        return xfce_device_manager_wayland_new (display, channel, error);
    }
#endif

    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         "No supported windowing system available");
    return NULL;
}

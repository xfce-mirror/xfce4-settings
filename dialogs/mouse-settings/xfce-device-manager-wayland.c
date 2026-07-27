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

#include "xfce-device-manager-wayland.h"
#include "xfce-device-wayland.h"

#include "protocols/xfce-input-device-list-v1-client.h"

#include <gdk/gdkwayland.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

struct _XfceDeviceManagerWayland
{
    XfceDeviceManager parent_instance;

    struct wl_registry *registry;
    struct xfce_input_device_list_private_v1 *device_list;

    GList *pending_devices;
};

static void
xfce_device_manager_wayland_initable_init (GInitableIface *iface);
static gboolean
xfce_device_manager_wayland_real_init (GInitable *initable,
                                       GCancellable *cancellable,
                                       GError **error);
static void
xfce_device_manager_wayland_finalize (GObject *object);

static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t name,
                 const char *interface,
                 uint32_t version);
static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t name);

static void
device_list_device (void *data,
                    struct xfce_input_device_list_private_v1 *device_list,
                    struct xfce_input_device_v1 *handle,
                    const char *name,
                    uint32_t capabilities);
static void
device_list_done (void *data,
                  struct xfce_input_device_list_private_v1 *device_list);


G_DEFINE_FINAL_TYPE_WITH_CODE (XfceDeviceManagerWayland,
                               xfce_device_manager_wayland,
                               XFCE_TYPE_DEVICE_MANAGER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, xfce_device_manager_wayland_initable_init))


static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static const struct xfce_input_device_list_private_v1_listener device_list_listener = {
    .device = device_list_device,
    .done = device_list_done,
};

static void
xfce_device_manager_wayland_class_init (XfceDeviceManagerWaylandClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_device_manager_wayland_finalize;
}

static void
xfce_device_manager_wayland_init (XfceDeviceManagerWayland *manager)
{
}

static void
xfce_device_manager_wayland_initable_init (GInitableIface *iface)
{
    iface->init = xfce_device_manager_wayland_real_init;
}

static gboolean
xfce_device_manager_wayland_real_init (GInitable *initable,
                                       GCancellable *cancellable,
                                       GError **error)
{
    XfceDeviceManagerWayland *self = XFCE_DEVICE_MANAGER_WAYLAND (initable);
    GdkDisplay *gdk_display = xfce_device_manager_get_display (XFCE_DEVICE_MANAGER (self));
    struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display);

    self->registry = wl_display_get_registry (wl_display);
    wl_registry_add_listener (self->registry, &registry_listener, self);
    wl_display_roundtrip (wl_display);

    if (self->device_list == NULL)
    {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             _("The compositor does not support the xfce_input_device_list_private_v1 protocol"));
        return FALSE;
    }
    else
    {
        wl_display_roundtrip (wl_display);
        return TRUE;
    }
}

static void
xfce_device_manager_wayland_finalize (GObject *object)
{
    XfceDeviceManagerWayland *self = XFCE_DEVICE_MANAGER_WAYLAND (object);

    g_list_free_full (self->pending_devices, g_object_unref);
    g_clear_pointer (&self->device_list, xfce_input_device_list_private_v1_destroy);
    g_clear_pointer (&self->registry, wl_registry_destroy);

    G_OBJECT_CLASS (xfce_device_manager_wayland_parent_class)->finalize (object);
}

static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t name,
                 const char *interface,
                 uint32_t version)
{
    if (g_strcmp0 (interface, xfce_input_device_list_private_v1_interface.name) == 0)
    {
        XfceDeviceManagerWayland *self = XFCE_DEVICE_MANAGER_WAYLAND (data);
        self->device_list = wl_registry_bind (registry, name, &xfce_input_device_list_private_v1_interface, 1);
        xfce_input_device_list_private_v1_add_listener (self->device_list, &device_list_listener, self);
    }
}

static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t name)
{
}

static void
device_list_device (void *data,
                    struct xfce_input_device_list_private_v1 *device_list,
                    struct xfce_input_device_v1 *handle,
                    const char *name,
                    uint32_t capabilities)
{
    XfceDeviceManagerWayland *self = XFCE_DEVICE_MANAGER_WAYLAND (data);

    if ((capabilities
         & (XFCE_DEVICE_CAPABILITIES_POINTER
            | XFCE_DEVICE_CAPABILITIES_TOUCH
            | XFCE_DEVICE_CAPABILITIES_TABLET_TOOL
            | XFCE_DEVICE_CAPABILITIES_TABLET_PAD
            | XFCE_DEVICE_CAPABILITIES_GESTURE))
        == 0)
    {
        xfce_input_device_v1_release (handle);
    }
    else
    {
        XfceDevice *device = xfce_device_wayland_new (XFCE_DEVICE_MANAGER (self), handle, name, capabilities);
        self->pending_devices = g_list_append (self->pending_devices, device);
    }
}

static void
device_list_done (void *data,
                  struct xfce_input_device_list_private_v1 *device_list)
{
    XfceDeviceManagerWayland *self = XFCE_DEVICE_MANAGER_WAYLAND (data);

    for (GList *lp = self->pending_devices; lp != NULL; lp = lp->next)
    {
        _xfce_device_manager_add_device (XFCE_DEVICE_MANAGER (self), lp->data);
    }
    g_clear_pointer (&self->pending_devices, g_list_free);
}

XfceDeviceManager *
xfce_device_manager_wayland_new (GdkDisplay *display,
                                 XfconfChannel *channel,
                                 GError **error)
{
    return g_initable_new (XFCE_TYPE_DEVICE_MANAGER_WAYLAND,
                           NULL,
                           error,
                           "display", display,
                           "channel", channel,
                           NULL);
}

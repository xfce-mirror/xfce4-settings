/*
 *  Copyright (c) 2008-2011 Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2008      Jannis Pohlmann <jannis@xfce.org>
 *  Copyright (c) 2026      Brian Tarricone <brian@tarricone.org>
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

#include "xfce-device-manager-x11.h"
#include "xfce-device-x11.h"

#include "xfsettingsd/pointers-defines.h"

#include <gdk/gdkx.h>
#include <gio/gio.h>

struct _XfceDeviceManagerX11
{
    XfceDeviceManager parent_instance;

    gint device_presence_event_type;
};

static void
xfce_device_manager_x11_initable_init (GInitableIface *iface);
static gboolean
xfce_device_manager_x11_real_init (GInitable *initable,
                                   GCancellable *cancellable,
                                   GError **error);
static void
xfce_device_manager_x11_finalize (GObject *object);
static void
xfce_device_manager_x11_reconcile (XfceDeviceManagerX11 *self);
static GdkFilterReturn
xfce_device_manager_x11_event_filter (GdkXEvent *xevent,
                                      GdkEvent *event,
                                      gpointer data);


G_DEFINE_FINAL_TYPE_WITH_CODE (XfceDeviceManagerX11,
                               xfce_device_manager_x11,
                               XFCE_TYPE_DEVICE_MANAGER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, xfce_device_manager_x11_initable_init))


static void
xfce_device_manager_x11_class_init (XfceDeviceManagerX11Class *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_device_manager_x11_finalize;
}

static void
xfce_device_manager_x11_init (XfceDeviceManagerX11 *manager)
{
}

static void
xfce_device_manager_x11_initable_init (GInitableIface *iface)
{
    iface->init = xfce_device_manager_x11_real_init;
}

static gboolean
xfce_device_manager_x11_real_init (GInitable *initable,
                                   GCancellable *cancellable,
                                   GError **error)
{
    XfceDeviceManagerX11 *self = XFCE_DEVICE_MANAGER_X11 (initable);
    GdkDisplay *gdk_display = xfce_device_manager_get_display (XFCE_DEVICE_MANAGER (self));
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display);

    XExtensionVersion *version = XGetExtensionVersion (xdisplay, INAME);
    if (version == NULL || ((long) version) == NoSuchExtension || !version->present)
    {
        if (version != NULL && ((long) version) != NoSuchExtension)
        {
            XFree (version);
        }
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             "The XInput extension is not present");
        return FALSE;
    }
    else if (version->major_version < MIN_XI_VERS_MAJOR
             || (version->major_version == MIN_XI_VERS_MAJOR
                 && version->minor_version < MIN_XI_VERS_MINOR))
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                     "XInput %d.%d is too old; %d.%d is required",
                     version->major_version, version->minor_version,
                     MIN_XI_VERS_MAJOR, MIN_XI_VERS_MINOR);
        XFree (version);
        return FALSE;
    }
    XFree (version);

    xfce_device_manager_x11_reconcile (self);

    XEventClass event_class;
    gdk_x11_display_error_trap_push (gdk_display);
    DevicePresence (xdisplay, self->device_presence_event_type, event_class);
    XSelectExtensionEvent (xdisplay, RootWindow (xdisplay, DefaultScreen (xdisplay)), &event_class, 1);
    if (gdk_x11_display_error_trap_pop (gdk_display) != 0)
    {
        g_warning ("Failed to set up the device hotplug event filter");
    }
    else
    {
        gdk_window_add_filter (NULL, xfce_device_manager_x11_event_filter, self);
    }

    return TRUE;
}

static void
xfce_device_manager_x11_finalize (GObject *object)
{
    gdk_window_remove_filter (NULL, xfce_device_manager_x11_event_filter, XFCE_DEVICE_MANAGER_X11 (object));

    G_OBJECT_CLASS (xfce_device_manager_x11_parent_class)->finalize (object);
}

static gboolean
xfce_device_should_list (XDeviceInfo *info)
{
    return info->use == IsXExtensionPointer
           && info->name != NULL
           && !g_str_has_prefix (info->name, "Virtual core XTEST");
}

static XfceDeviceX11 *
xfce_device_manager_x11_find (XfceDeviceManager *manager,
                              XID xid)
{
    for (GList *l = xfce_device_manager_list_devices (manager); l != NULL; l = l->next)
    {
        XfceDeviceX11 *device = XFCE_DEVICE_X11 (l->data);
        if (xfce_device_x11_get_xid (device) == xid)
        {
            return device;
        }
    }

    return NULL;
}

static void
xfce_device_manager_x11_reconcile (XfceDeviceManagerX11 *self)
{
    XfceDeviceManager *manager = XFCE_DEVICE_MANAGER (self);
    GdkDisplay *gdk_display = xfce_device_manager_get_display (manager);
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display);
    XfconfChannel *channel = xfce_device_manager_get_channel (manager);
    gint ndevices = 0;

    gdk_x11_display_error_trap_push (gdk_display);
    XDeviceInfo *device_list = XListInputDevices (xdisplay, &ndevices);
    if (gdk_x11_display_error_trap_pop (gdk_display) != 0 || device_list == NULL)
    {
        g_message ("No devices found");
        return;
    }

    /* remove devices that are no longer present */
    GHashTable *present = g_hash_table_new (NULL, NULL);
    for (gint i = 0; i < ndevices; i++)
    {
        if (xfce_device_should_list (&device_list[i]))
        {
            g_hash_table_add (present, GSIZE_TO_POINTER (device_list[i].id));
        }
    }

    GList *stale = NULL;
    for (GList *l = xfce_device_manager_list_devices (manager); l != NULL; l = l->next)
    {
        XfceDeviceX11 *device = XFCE_DEVICE_X11 (l->data);
        if (!g_hash_table_contains (present, GSIZE_TO_POINTER (xfce_device_x11_get_xid (device))))
        {
            stale = g_list_prepend (stale, device);
        }
    }
    for (GList *l = stale; l != NULL; l = l->next)
    {
        _xfce_device_manager_remove_device (manager, l->data);
    }
    g_list_free (stale);
    g_hash_table_destroy (present);

    for (gint i = 0; i < ndevices; i++)
    {
        XDeviceInfo *info = &device_list[i];
        if (xfce_device_should_list (info)
            && xfce_device_manager_x11_find (manager, info->id) == NULL)
        {
            XfceDevice *device = xfce_device_x11_new (channel, info->id, info->name);
            _xfce_device_manager_add_device (manager, device);
        }
    }

    XFreeDeviceList (device_list);
}

static GdkFilterReturn
xfce_device_manager_x11_event_filter (GdkXEvent *xevent,
                                      GdkEvent *event,
                                      gpointer data)
{
    XEvent *xev = xevent;
    XDevicePresenceNotifyEvent *dpn = xevent;
    XfceDeviceManagerX11 *self = XFCE_DEVICE_MANAGER_X11 (data);

    if (xev->type == self->device_presence_event_type
        && (dpn->devchange == DeviceAdded || dpn->devchange == DeviceRemoved))
    {
        xfce_device_manager_x11_reconcile (self);
    }

    return GDK_FILTER_CONTINUE;
}

XfceDeviceManager *
xfce_device_manager_x11_new (GdkDisplay *display,
                             XfconfChannel *channel,
                             GError **error)
{
    return g_initable_new (XFCE_TYPE_DEVICE_MANAGER_X11,
                           NULL,
                           error,
                           "display", display,
                           "channel", channel,
                           NULL);
}

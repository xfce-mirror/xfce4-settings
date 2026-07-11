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

#ifndef _XFCE_DEVICE_MANAGER_H_
#define _XFCE_DEVICE_MANAGER_H_

#include "xfce-device.h"

#include <gdk/gdk.h>
#include <glib-object.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

#define XFCE_TYPE_DEVICE_MANAGER (xfce_device_manager_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfceDeviceManager, xfce_device_manager, XFCE, DEVICE_MANAGER, GObject)

struct _XfceDeviceManagerClass
{
    GObjectClass parent_class;
};

XfceDeviceManager *
xfce_device_manager_new (GdkDisplay *display,
                         XfconfChannel *channel,
                         GError **error);
GList *
xfce_device_manager_list_devices (XfceDeviceManager *manager);
GdkDisplay *
xfce_device_manager_get_display (XfceDeviceManager *manager);
XfconfChannel *
xfce_device_manager_get_channel (XfceDeviceManager *manager);

/* Called by the backends to publish devices as they appear and disappear. Not
 * for use by the dialog. Ownership of the device is transferred to the
 * manager. */
void
_xfce_device_manager_add_device (XfceDeviceManager *manager,
                                 XfceDevice *device);
void
_xfce_device_manager_remove_device (XfceDeviceManager *manager,
                                    XfceDevice *device);

G_END_DECLS

#endif

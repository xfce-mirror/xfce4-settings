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

#ifndef _XFCE_DEVICE_MANAGER_WAYLAND_H_
#define _XFCE_DEVICE_MANAGER_WAYLAND_H_

#include "xfce-device-manager.h"

#include <gdk/gdk.h>
#include <glib-object.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

#define XFCE_TYPE_DEVICE_MANAGER_WAYLAND (xfce_device_manager_wayland_get_type ())
G_DECLARE_FINAL_TYPE (XfceDeviceManagerWayland, xfce_device_manager_wayland, XFCE, DEVICE_MANAGER_WAYLAND, XfceDeviceManager)

XfceDeviceManager *
xfce_device_manager_wayland_new (GdkDisplay *display,
                                 XfconfChannel *channel,
                                 GError **error);

G_END_DECLS

#endif

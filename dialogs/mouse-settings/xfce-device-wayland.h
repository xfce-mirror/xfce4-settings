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

#ifndef _XFCE_DEVICE_WAYLAND_H_
#define _XFCE_DEVICE_WAYLAND_H_

#include "xfce-device-manager.h"
#include "xfce-device.h"

#include <glib-object.h>

G_BEGIN_DECLS

struct xfce_input_device_v1;

#define XFCE_TYPE_DEVICE_WAYLAND (xfce_device_wayland_get_type ())
G_DECLARE_FINAL_TYPE (XfceDeviceWayland, xfce_device_wayland, XFCE, DEVICE_WAYLAND, XfceDevice)

/* The manager is borrowed, not referenced: it owns the devices it creates, and
 * is told through it when the compositor removes one. */
XfceDevice *
xfce_device_wayland_new (XfceDeviceManager *manager,
                         struct xfce_input_device_v1 *handle,
                         const gchar *name,
                         XfceDeviceCapabilities capabilities);

G_END_DECLS

#endif

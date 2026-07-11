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

#ifndef _XFCE_DEVICE_X11_H_
#define _XFCE_DEVICE_X11_H_

#include "xfce-device.h"

#include <X11/Xlib.h>
#include <glib-object.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

#define XFCE_TYPE_DEVICE_X11 (xfce_device_x11_get_type ())
G_DECLARE_FINAL_TYPE (XfceDeviceX11, xfce_device_x11, XFCE, DEVICE_X11, XfceDevice)

XfceDevice *
xfce_device_x11_new (XfconfChannel *channel,
                     XID xid,
                     const gchar *name);
XID
xfce_device_x11_get_xid (XfceDeviceX11 *device);

/* Whether one of the legacy X input drivers is driving the device rather than
 * the libinput one, which decides which of the X11-only widgets below apply; the
 * rest of the dialog's device-type decisions come from the shared capabilities
 * and *_available getters. */
gboolean
xfce_device_x11_is_libinput (XfceDeviceX11 *device);

/*
 * Settings that only exist on X11 (core-X feedback, the hi-res wheel scroll
 * libinput property, the legacy wacom driver, and the synaptics-only
 * horizontal-scroll toggle). The dialog reaches these behind a
 * `XFCE_IS_DEVICE_X11 ()` check.
 *
 * The tap and scroll-method settings themselves — circular scrolling included —
 * are the shared `xfce_device_get/set_*`; `XfceDeviceX11` overrides the setters
 * to also write the driver-specific synaptics keys.
 */

gboolean
xfce_device_x11_get_threshold_available (XfceDeviceX11 *device);
gint
xfce_device_x11_get_threshold (XfceDeviceX11 *device);
gboolean
xfce_device_x11_get_hires_scrolling_available (XfceDeviceX11 *device);
gboolean
xfce_device_x11_get_hires_scrolling (XfceDeviceX11 *device);
gint
xfce_device_x11_get_wacom_mode (XfceDeviceX11 *device);
gboolean
xfce_device_x11_get_synaptics_scroll_horizontal_available (XfceDeviceX11 *device);
gboolean
xfce_device_x11_get_synaptics_scroll_horizontal (XfceDeviceX11 *device);

void
xfce_device_x11_set_threshold (XfceDeviceX11 *device,
                               gint threshold);
void
xfce_device_x11_reset_feedback (XfceDeviceX11 *device);
void
xfce_device_x11_set_hires_scrolling (XfceDeviceX11 *device,
                                     gboolean enabled);
void
xfce_device_x11_set_wacom_mode (XfceDeviceX11 *device,
                                const gchar *mode);
void
xfce_device_x11_set_synaptics_scroll_horizontal (XfceDeviceX11 *device,
                                                 gboolean horizontal);

G_END_DECLS

#endif

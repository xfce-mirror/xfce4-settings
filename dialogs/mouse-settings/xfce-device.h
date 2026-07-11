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

#ifndef _XFCE_DEVICE_H_
#define _XFCE_DEVICE_H_

#include <glib-object.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

#define XFCE_TYPE_DEVICE (xfce_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfceDevice, xfce_device, XFCE, DEVICE, GObject)

#define XFCE_TYPE_DEVICE_CAPABILITIES (xfce_device_capabilities_get_type ())

/* The value enums below mirror the ones in the
 * xfce_input_device_list_private_v1 Wayland protocol and deliberately reuse
 * the same numeric values, so the Wayland backend can forward protocol values
 * without translation. The X11 backend translates its X-property encodings
 * into these before pushing them into the device. */

typedef enum
{
    XFCE_DEVICE_CAPABILITIES_KEYBOARD = (1 << 0),
    XFCE_DEVICE_CAPABILITIES_POINTER = (1 << 1),
    XFCE_DEVICE_CAPABILITIES_TOUCH = (1 << 2),
    XFCE_DEVICE_CAPABILITIES_TABLET_TOOL = (1 << 3),
    XFCE_DEVICE_CAPABILITIES_TABLET_PAD = (1 << 4),
    XFCE_DEVICE_CAPABILITIES_GESTURE = (1 << 5),
    XFCE_DEVICE_CAPABILITIES_SWITCH = (1 << 6),
} XfceDeviceCapabilities;

typedef enum
{
    XFCE_DEVICE_SEND_EVENTS_ENABLED = 0,
    XFCE_DEVICE_SEND_EVENTS_DISABLED = (1 << 0),
    XFCE_DEVICE_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE = (1 << 1),
} XfceDeviceSendEvents;

typedef enum
{
    XFCE_DEVICE_ACCEL_PROFILE_NONE = 0,
    XFCE_DEVICE_ACCEL_PROFILE_FLAT = (1 << 0),
    XFCE_DEVICE_ACCEL_PROFILE_ADAPTIVE = (1 << 1),
    XFCE_DEVICE_ACCEL_PROFILE_CUSTOM = (1 << 2),
} XfceDeviceAccelProfile;

typedef enum
{
    XFCE_DEVICE_SCROLL_METHOD_NO_SCROLL = 0,
    XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER = (1 << 0),
    XFCE_DEVICE_SCROLL_METHOD_EDGE = (1 << 1),
    XFCE_DEVICE_SCROLL_METHOD_ON_BUTTON_DOWN = (1 << 2),
    /* No protocol/libinput equivalent: the legacy synaptics driver's circular
     * scrolling. Placed at a high bit to leave room for future libinput scroll
     * methods, and appended so the shared scroll-method infrastructure can carry
     * it. */
    XFCE_DEVICE_SCROLL_METHOD_CIRCULAR = (1 << 16),
} XfceDeviceScrollMethod;

typedef enum
{
    XFCE_DEVICE_CLICK_METHOD_NONE = 0,
    XFCE_DEVICE_CLICK_METHOD_BUTTON_AREAS = (1 << 0),
    XFCE_DEVICE_CLICK_METHOD_CLICKFINGER = (1 << 1),
} XfceDeviceClickMethod;

typedef enum
{
    XFCE_DEVICE_ROTATION_NONE = 0,
    XFCE_DEVICE_ROTATION_90,
    XFCE_DEVICE_ROTATION_180,
    XFCE_DEVICE_ROTATION_270,
} XfceDeviceRotation;

typedef enum
{
    XFCE_DEVICE_REFLECTION_NONE = 0,
    XFCE_DEVICE_REFLECTION_X,
    XFCE_DEVICE_REFLECTION_Y,
    XFCE_DEVICE_REFLECTION_XY,
} XfceDeviceReflection;

struct _XfceDeviceClass
{
    GObjectClass parent_class;

    /* Re-read the device's state from the windowing system and republish it via
     * the _xfce_device_update_* setters. Optional: a backend whose devices push
     * their own updates does not need to implement this. */
    void (*refresh) (XfceDevice *device);

    /* The tap and scroll-method settings each have both a libinput and a legacy
     * synaptics representation; the base writes the libinput one and the X11
     * device overrides these to also write the synaptics one. */
    void (*set_tap) (XfceDevice *device,
                     gboolean tap);
    void (*set_scroll_method) (XfceDevice *device,
                               XfceDeviceScrollMethod method);
};

GType
xfce_device_capabilities_get_type (void);

void
xfce_device_refresh (XfceDevice *device);

const gchar *
xfce_device_get_name (XfceDevice *device);
XfceDeviceCapabilities
xfce_device_get_capabilities (XfceDevice *device);

gboolean
xfce_device_get_send_events_available (XfceDevice *device);
gboolean
xfce_device_get_enabled (XfceDevice *device);

gboolean
xfce_device_get_acceleration_available (XfceDevice *device);
gdouble
xfce_device_get_acceleration (XfceDevice *device);

gboolean
xfce_device_get_accel_profile_available (XfceDevice *device);
XfceDeviceAccelProfile
xfce_device_get_accel_profile_supported (XfceDevice *device);
XfceDeviceAccelProfile
xfce_device_get_accel_profile (XfceDevice *device);

gboolean
xfce_device_get_natural_scroll_available (XfceDevice *device);
gboolean
xfce_device_get_natural_scroll (XfceDevice *device);

gboolean
xfce_device_get_left_handed_available (XfceDevice *device);
gboolean
xfce_device_get_left_handed (XfceDevice *device);

gboolean
xfce_device_get_tap_available (XfceDevice *device);
gboolean
xfce_device_get_tap (XfceDevice *device);

gboolean
xfce_device_get_scroll_method_available (XfceDevice *device);
XfceDeviceScrollMethod
xfce_device_get_scroll_method_supported (XfceDevice *device);
XfceDeviceScrollMethod
xfce_device_get_scroll_method (XfceDevice *device);

gboolean
xfce_device_get_click_method_available (XfceDevice *device);
XfceDeviceClickMethod
xfce_device_get_click_method_supported (XfceDevice *device);
XfceDeviceClickMethod
xfce_device_get_click_method (XfceDevice *device);

gboolean
xfce_device_get_tablet_rotation_available (XfceDevice *device);
XfceDeviceRotation
xfce_device_get_tablet_rotation (XfceDevice *device);

XfceDeviceRotation
xfce_device_get_touchscreen_rotation (XfceDevice *device);
XfceDeviceReflection
xfce_device_get_touchscreen_reflection (XfceDevice *device);
gchar *
xfce_device_get_assigned_monitor (XfceDevice *device);

gboolean
xfce_device_get_dwt_available (XfceDevice *device);
gboolean
xfce_device_get_dwt (XfceDevice *device);

/* The setters persist to xfconf and record the new value as the current one, so
 * a getter always reflects the last value asked for. That is optimistic: the
 * settings daemon applies the write asynchronously, and only
 * xfce_device_refresh() reports what the device really ended up with.
 * Subclasses that cache state of their own must follow the same rule. */
void
xfce_device_set_enabled (XfceDevice *device,
                         gboolean enabled);
void
xfce_device_set_acceleration (XfceDevice *device,
                              gdouble acceleration);
void
xfce_device_set_accel_profile (XfceDevice *device,
                               gboolean adaptive);
void
xfce_device_set_natural_scroll (XfceDevice *device,
                                gboolean natural_scroll);
void
xfce_device_set_left_handed (XfceDevice *device,
                             gboolean left_handed);
void
xfce_device_set_tap (XfceDevice *device,
                     gboolean tap);
void
xfce_device_set_scroll_method (XfceDevice *device,
                               XfceDeviceScrollMethod method);
void
xfce_device_set_click_method (XfceDevice *device,
                              XfceDeviceClickMethod method);
void
xfce_device_set_tablet_rotation (XfceDevice *device,
                                 XfceDeviceRotation rotation);
void
xfce_device_set_touchscreen_rotation (XfceDevice *device,
                                      XfceDeviceRotation rotation);
void
xfce_device_set_touchscreen_reflection (XfceDevice *device,
                                        XfceDeviceReflection reflection);
void
xfce_device_set_assigned_monitor (XfceDevice *device,
                                  const gchar *edid);
void
xfce_device_set_dwt (XfceDevice *device,
                     gboolean dwt);

/* Semi-private accessors for subclasses that persist device-specific settings
 * the base does not model. Not for use by the dialog. The two key builders
 * spell the xfconf property names the settings daemon expects, so every write
 * goes through them rather than repeating the convention. */
XfconfChannel *
_xfce_device_get_channel (XfceDevice *device);
gchar *
_xfce_device_prop (XfceDevice *device,
                   const gchar *suffix);
gchar *
_xfce_device_libinput_prop (XfceDevice *device,
                            const gchar *libinput_prop);

void
_xfce_device_update_capabilities (XfceDevice *device,
                                  XfceDeviceCapabilities capabilities);
void
_xfce_device_update_send_events_state (XfceDevice *device,
                                       XfceDeviceSendEvents supported,
                                       XfceDeviceSendEvents default_value,
                                       XfceDeviceSendEvents current_value);
void
_xfce_device_update_accel_speed (XfceDevice *device,
                                 gdouble default_value,
                                 gdouble current_value);
void
_xfce_device_update_accel_profile (XfceDevice *device,
                                   XfceDeviceAccelProfile supported,
                                   XfceDeviceAccelProfile default_value,
                                   XfceDeviceAccelProfile current_value,
                                   gboolean custom_slot);
void
_xfce_device_update_natural_scroll (XfceDevice *device,
                                    gboolean default_value,
                                    gboolean current_value);
void
_xfce_device_update_left_handed (XfceDevice *device,
                                 gboolean default_value,
                                 gboolean current_value);
void
_xfce_device_update_tap (XfceDevice *device,
                         gboolean default_value,
                         gboolean current_value);
void
_xfce_device_update_scroll_method (XfceDevice *device,
                                   XfceDeviceScrollMethod supported,
                                   XfceDeviceScrollMethod default_value,
                                   XfceDeviceScrollMethod current_value);
void
_xfce_device_update_click_method (XfceDevice *device,
                                  XfceDeviceClickMethod supported,
                                  XfceDeviceClickMethod default_value,
                                  XfceDeviceClickMethod current_value);
XfceDeviceRotation
_xfce_device_tablet_rotation_from_wacom (gint wacom);
void
_xfce_device_update_tablet_rotation (XfceDevice *device,
                                     XfceDeviceRotation default_value,
                                     XfceDeviceRotation current_value);
void
_xfce_device_update_dwt (XfceDevice *device,
                         gboolean default_value,
                         gboolean current_value);

G_END_DECLS

#endif

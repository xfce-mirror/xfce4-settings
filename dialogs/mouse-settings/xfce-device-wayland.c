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

#include "xfce-device-wayland.h"

#include "protocols/xfce-input-device-list-v1-client.h"

#include <gdk/gdkwayland.h>

struct _XfceDeviceWayland
{
    XfceDevice parent_instance;

    struct xfce_input_device_v1 *handle;
    XfceDeviceManager *manager;
};

enum
{
    PROP_0,
    PROP_HANDLE,
    PROP_MANAGER,
};

static void
xfce_device_wayland_constructed (GObject *object);
static void
xfce_device_wayland_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec);
static void
xfce_device_wayland_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec);
static void
xfce_device_wayland_finalize (GObject *object);
static void
device_rotation_property_changed (XfconfChannel *channel,
                                  const gchar *property,
                                  const GValue *value,
                                  XfceDeviceWayland *device);

static void
device_removed (void *data,
                struct xfce_input_device_v1 *handle);
static void
device_send_events (void *data,
                    struct xfce_input_device_v1 *handle,
                    uint32_t supported,
                    uint32_t default_value,
                    uint32_t current_value);
static void
device_accel_speed (void *data,
                    struct xfce_input_device_v1 *handle,
                    wl_fixed_t default_value,
                    wl_fixed_t current_value);
static void
device_accel_profile (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t supported,
                      uint32_t default_value,
                      uint32_t current_value);
static void
device_natural_scroll (void *data,
                       struct xfce_input_device_v1 *handle,
                       uint32_t default_value,
                       uint32_t current_value);
static void
device_scroll_method (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t supported,
                      uint32_t default_value,
                      uint32_t current_value);
static void
device_scroll_button (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t default_value,
                      uint32_t current_value);
static void
device_scroll_button_lock (void *data,
                           struct xfce_input_device_v1 *handle,
                           uint32_t default_value,
                           uint32_t current_value);
static void
device_click_method (void *data,
                     struct xfce_input_device_v1 *handle,
                     uint32_t supported,
                     uint32_t default_value,
                     uint32_t current_value);
static void
device_clickfinger_button_map (void *data,
                               struct xfce_input_device_v1 *handle,
                               uint32_t default_value,
                               uint32_t current_value);
static void
device_left_handed (void *data,
                    struct xfce_input_device_v1 *handle,
                    uint32_t default_value,
                    uint32_t current_value);
static void
device_middle_emulation (void *data,
                         struct xfce_input_device_v1 *handle,
                         uint32_t default_value,
                         uint32_t current_value);
static void
device_tap (void *data,
            struct xfce_input_device_v1 *handle,
            uint32_t default_value,
            uint32_t current_value);
static void
device_tap_finger_count (void *data,
                         struct xfce_input_device_v1 *handle,
                         uint32_t count);
static void
device_tap_button_map (void *data,
                       struct xfce_input_device_v1 *handle,
                       uint32_t default_value,
                       uint32_t current_value);
static void
device_tap_drag (void *data,
                 struct xfce_input_device_v1 *handle,
                 uint32_t default_value,
                 uint32_t current_value);
static void
device_tap_drag_lock (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t default_value,
                      uint32_t current_value);
static void
device_three_finger_drag (void *data,
                          struct xfce_input_device_v1 *handle,
                          uint32_t max_fingers,
                          uint32_t default_value,
                          uint32_t current_value);
static void
device_dwt (void *data,
            struct xfce_input_device_v1 *handle,
            uint32_t default_value,
            uint32_t current_value);
static void
device_dwt_timeout (void *data,
                    struct xfce_input_device_v1 *handle,
                    uint32_t default_value,
                    uint32_t current_value);
static void
device_dwtp (void *data,
             struct xfce_input_device_v1 *handle,
             uint32_t default_value,
             uint32_t current_value);
static void
device_dwtp_timeout (void *data,
                     struct xfce_input_device_v1 *handle,
                     uint32_t default_value,
                     uint32_t current_value);
static void
device_rotation (void *data,
                 struct xfce_input_device_v1 *handle,
                 uint32_t default_value,
                 uint32_t current_value);
static void
device_calibration (void *data,
                    struct xfce_input_device_v1 *handle,
                    wl_fixed_t default_a,
                    wl_fixed_t default_b,
                    wl_fixed_t default_c,
                    wl_fixed_t default_d,
                    wl_fixed_t default_e,
                    wl_fixed_t default_f,
                    wl_fixed_t current_a,
                    wl_fixed_t current_b,
                    wl_fixed_t current_c,
                    wl_fixed_t current_d,
                    wl_fixed_t current_e,
                    wl_fixed_t current_f);
static void
device_area (void *data,
             struct xfce_input_device_v1 *handle,
             wl_fixed_t default_x1,
             wl_fixed_t default_y1,
             wl_fixed_t default_x2,
             wl_fixed_t default_y2,
             wl_fixed_t current_x1,
             wl_fixed_t current_y1,
             wl_fixed_t current_x2,
             wl_fixed_t current_y2);


G_DEFINE_FINAL_TYPE (XfceDeviceWayland, xfce_device_wayland, XFCE_TYPE_DEVICE)


static const struct xfce_input_device_v1_listener device_listener = {
    .removed = device_removed,
    .send_events = device_send_events,
    .accel_speed = device_accel_speed,
    .accel_profile = device_accel_profile,
    .natural_scroll = device_natural_scroll,
    .scroll_method = device_scroll_method,
    .scroll_button = device_scroll_button,
    .scroll_button_lock = device_scroll_button_lock,
    .click_method = device_click_method,
    .clickfinger_button_map = device_clickfinger_button_map,
    .left_handed = device_left_handed,
    .middle_emulation = device_middle_emulation,
    .tap = device_tap,
    .tap_finger_count = device_tap_finger_count,
    .tap_button_map = device_tap_button_map,
    .tap_drag = device_tap_drag,
    .tap_drag_lock = device_tap_drag_lock,
    .three_finger_drag = device_three_finger_drag,
    .dwt = device_dwt,
    .dwt_timeout = device_dwt_timeout,
    .dwtp = device_dwtp,
    .dwtp_timeout = device_dwtp_timeout,
    .rotation = device_rotation,
    .calibration = device_calibration,
    .area = device_area,
};

static void
xfce_device_wayland_class_init (XfceDeviceWaylandClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->constructed = xfce_device_wayland_constructed;
    gobject_class->set_property = xfce_device_wayland_set_property;
    gobject_class->get_property = xfce_device_wayland_get_property;
    gobject_class->finalize = xfce_device_wayland_finalize;

    g_object_class_install_property (gobject_class,
                                     PROP_HANDLE,
                                     g_param_spec_pointer ("handle",
                                                           "handle",
                                                           "handle",
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_MANAGER,
                                     g_param_spec_pointer ("manager",
                                                           "manager",
                                                           "manager",
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
xfce_device_wayland_init (XfceDeviceWayland *device)
{
}

static void
xfce_device_wayland_constructed (GObject *object)
{
    G_OBJECT_CLASS (xfce_device_wayland_parent_class)->constructed (object);

    XfceDeviceWayland *self = XFCE_DEVICE_WAYLAND (object);
    xfce_input_device_v1_add_listener (self->handle, &device_listener, self);

    // Tablet rotation is the one setting not read from the protocol. The
    // compositor applies it as a transformation matrix, so it comes back as a
    // calibration event, which a rotation cannot be recovered from once a
    // reflection or a calibration of its own is folded in. xfconf is where the
    // value was written and holds it exactly, so it is read straight back, and
    // watched so that a change made elsewhere is picked up the way a protocol
    // event would have been.
    if ((xfce_device_get_capabilities (XFCE_DEVICE (self)) & XFCE_DEVICE_CAPABILITIES_TABLET_TOOL) != 0)
    {
        XfconfChannel *channel = _xfce_device_get_channel (XFCE_DEVICE (self));
        gchar *prop = _xfce_device_prop (XFCE_DEVICE (self), "/Properties/Wacom_Rotation");

        XfceDeviceRotation rotation =
            _xfce_device_tablet_rotation_from_wacom (xfconf_channel_get_int (channel, prop, 0));
        _xfce_device_update_tablet_rotation (XFCE_DEVICE (self), XFCE_DEVICE_ROTATION_NONE, rotation);

        gchar *signal_name = g_strconcat ("property-changed::", prop, NULL);
        g_signal_connect_object (channel, signal_name,
                                 G_CALLBACK (device_rotation_property_changed), self, G_CONNECT_DEFAULT);
        g_free (signal_name);
        g_free (prop);
    }
}

static void
xfce_device_wayland_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    XfceDeviceWayland *self = XFCE_DEVICE_WAYLAND (object);

    switch (property_id)
    {
        case PROP_HANDLE:
            self->handle = g_value_get_pointer (value);
            break;

        case PROP_MANAGER:
            self->manager = g_value_get_pointer (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_wayland_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    XfceDeviceWayland *self = XFCE_DEVICE_WAYLAND (object);

    switch (property_id)
    {
        case PROP_HANDLE:
            g_value_set_pointer (value, self->handle);
            break;

        case PROP_MANAGER:
            g_value_set_pointer (value, self->manager);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_wayland_finalize (GObject *object)
{
    XfceDeviceWayland *self = XFCE_DEVICE_WAYLAND (object);

    g_clear_pointer (&self->handle, xfce_input_device_v1_release);

    G_OBJECT_CLASS (xfce_device_wayland_parent_class)->finalize (object);
}

static void
device_removed (void *data,
                struct xfce_input_device_v1 *handle)
{
    XfceDeviceWayland *self = XFCE_DEVICE_WAYLAND (data);
    // Manager holds the only reference; no need to unref here.
    _xfce_device_manager_remove_device (self->manager, XFCE_DEVICE (self));
}

// The protocol reports a setting only when the device has it, so each of the
// events below means "available, and here is its state". The shared enums carry
// the protocol's own values, so they need no translation; only accel_speed is
// scaled, from the protocol's [-1, 1] to the range the dialog's slider uses.

static void
device_send_events (void *data,
                    struct xfce_input_device_v1 *handle,
                    uint32_t supported,
                    uint32_t default_value,
                    uint32_t current_value)
{
    _xfce_device_update_send_events_state (XFCE_DEVICE (data), supported, default_value, current_value);
}

static void
device_accel_speed (void *data,
                    struct xfce_input_device_v1 *handle,
                    wl_fixed_t default_value,
                    wl_fixed_t current_value)
{
    _xfce_device_update_accel_speed (XFCE_DEVICE (data),
                                     (wl_fixed_to_double (default_value) + 1.0) * 5.0,
                                     (wl_fixed_to_double (current_value) + 1.0) * 5.0);
}

static void
device_accel_profile (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t supported,
                      uint32_t default_value,
                      uint32_t current_value)
{
    // The written array keeps a slot for the custom profile whenever the device
    // has one: unlike X11 there is no driver-wide slot count to consult.
    _xfce_device_update_accel_profile (XFCE_DEVICE (data), supported, default_value, current_value,
                                       (supported & XFCE_DEVICE_ACCEL_PROFILE_CUSTOM) != 0);
}

static void
device_natural_scroll (void *data,
                       struct xfce_input_device_v1 *handle,
                       uint32_t default_value,
                       uint32_t current_value)
{
    _xfce_device_update_natural_scroll (XFCE_DEVICE (data), default_value != 0, current_value != 0);
}

static void
device_scroll_method (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t supported,
                      uint32_t default_value,
                      uint32_t current_value)
{
    _xfce_device_update_scroll_method (XFCE_DEVICE (data), supported, default_value, current_value);
}

static void
device_click_method (void *data,
                     struct xfce_input_device_v1 *handle,
                     uint32_t supported,
                     uint32_t default_value,
                     uint32_t current_value)
{
    _xfce_device_update_click_method (XFCE_DEVICE (data), supported, default_value, current_value);
}

static void
device_left_handed (void *data,
                    struct xfce_input_device_v1 *handle,
                    uint32_t default_value,
                    uint32_t current_value)
{
    _xfce_device_update_left_handed (XFCE_DEVICE (data), default_value != 0, current_value != 0);
}

static void
device_tap (void *data,
            struct xfce_input_device_v1 *handle,
            uint32_t default_value,
            uint32_t current_value)
{
    _xfce_device_update_tap (XFCE_DEVICE (data), default_value != 0, current_value != 0);
}

static void
device_dwt (void *data,
            struct xfce_input_device_v1 *handle,
            uint32_t default_value,
            uint32_t current_value)
{
    _xfce_device_update_dwt (XFCE_DEVICE (data), default_value != 0, current_value != 0);
}

static void
device_scroll_button (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t default_value,
                      uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_scroll_button_lock (void *data,
                           struct xfce_input_device_v1 *handle,
                           uint32_t default_value,
                           uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_clickfinger_button_map (void *data,
                               struct xfce_input_device_v1 *handle,
                               uint32_t default_value,
                               uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_middle_emulation (void *data,
                         struct xfce_input_device_v1 *handle,
                         uint32_t default_value,
                         uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_tap_finger_count (void *data,
                         struct xfce_input_device_v1 *handle,
                         uint32_t count)
{
    // Currently unused by the dialog.
}

static void
device_tap_button_map (void *data,
                       struct xfce_input_device_v1 *handle,
                       uint32_t default_value,
                       uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_tap_drag (void *data,
                 struct xfce_input_device_v1 *handle,
                 uint32_t default_value,
                 uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_tap_drag_lock (void *data,
                      struct xfce_input_device_v1 *handle,
                      uint32_t default_value,
                      uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_three_finger_drag (void *data,
                          struct xfce_input_device_v1 *handle,
                          uint32_t max_fingers,
                          uint32_t default_value,
                          uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_dwt_timeout (void *data,
                    struct xfce_input_device_v1 *handle,
                    uint32_t default_value,
                    uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_dwtp (void *data,
             struct xfce_input_device_v1 *handle,
             uint32_t default_value,
             uint32_t current_value)
{
    // Currently unused by the dialog.
}

static void
device_dwtp_timeout (void *data,
                     struct xfce_input_device_v1 *handle,
                     uint32_t default_value,
                     uint32_t current_value)
{
    // Currently unused by the dialog.
}

// A reset property arrives with an unset value, which reads as no rotation.
static void
device_rotation_property_changed (XfconfChannel *channel,
                                  const gchar *property,
                                  const GValue *value,
                                  XfceDeviceWayland *device)
{
    XfceDeviceRotation rotation =
        _xfce_device_tablet_rotation_from_wacom (G_VALUE_HOLDS_INT (value) ? g_value_get_int (value) : 0);
    _xfce_device_update_tablet_rotation (XFCE_DEVICE (device), XFCE_DEVICE_ROTATION_NONE, rotation);
}

// This reports libinput's rotation angle, which adjusts a relative device's
// motion — a trackball mounted askew — and is not the tablet rotation the dialog
// offers. That one reaches the compositor as a transformation matrix, so it
// comes back as a calibration event instead.
static void
device_rotation (void *data,
                 struct xfce_input_device_v1 *handle,
                 uint32_t default_value,
                 uint32_t current_value)
{
}

static void
device_calibration (void *data,
                    struct xfce_input_device_v1 *handle,
                    wl_fixed_t default_a,
                    wl_fixed_t default_b,
                    wl_fixed_t default_c,
                    wl_fixed_t default_d,
                    wl_fixed_t default_e,
                    wl_fixed_t default_f,
                    wl_fixed_t current_a,
                    wl_fixed_t current_b,
                    wl_fixed_t current_c,
                    wl_fixed_t current_d,
                    wl_fixed_t current_e,
                    wl_fixed_t current_f)
{
    // The dialog shows the calibration matrix stored in xfconf, not what's
    // currently set.
}

static void
device_area (void *data,
             struct xfce_input_device_v1 *handle,
             wl_fixed_t default_x1,
             wl_fixed_t default_y1,
             wl_fixed_t default_x2,
             wl_fixed_t default_y2,
             wl_fixed_t current_x1,
             wl_fixed_t current_y1,
             wl_fixed_t current_x2,
             wl_fixed_t current_y2)
{
    // Currently unused by the dialog.
}

XfceDevice *
xfce_device_wayland_new (XfceDeviceManager *manager,
                         struct xfce_input_device_v1 *handle,
                         const gchar *name,
                         XfceDeviceCapabilities capabilities)
{
    return g_object_new (XFCE_TYPE_DEVICE_WAYLAND,
                         "channel", xfce_device_manager_get_channel (manager),
                         "name", name,
                         "capabilities", capabilities,
                         "handle", handle,
                         "manager", manager,
                         NULL);
}

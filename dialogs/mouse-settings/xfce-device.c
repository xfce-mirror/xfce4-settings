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

#include "libinput-properties.h"
#include "xfce-device.h"

#include <string.h>

#define GET_PRIV(device) ((XfceDevicePrivate *) xfce_device_get_instance_private (XFCE_DEVICE (device)))

typedef struct
{
    gboolean available;
    gboolean default_value;
    gboolean current;
} BoolSetting;

// Everything a refresh republishes is grouped here so that discarding the
// previous read stays a single memset as settings are added.
typedef struct
{
    struct
    {
        gboolean available;
        XfceDeviceSendEvents supported;
        XfceDeviceSendEvents default_value;
        XfceDeviceSendEvents current;
    } send_events;

    struct
    {
        gboolean available;
        gdouble default_value;
        gdouble current;
    } accel_speed;

    struct
    {
        gboolean available;
        XfceDeviceAccelProfile supported;
        XfceDeviceAccelProfile default_value;
        XfceDeviceAccelProfile current;
        // Whether the enabled-profile array carries a custom slot, i.e. how many
        // elements a write must emit. Distinct from whether the custom profile is
        // in `supported`: the slot exists whenever the backend exposes it, even
        // for a device that does not offer that profile.
        gboolean has_custom_slot;
    } accel_profile;

    struct
    {
        gboolean available;
        XfceDeviceScrollMethod supported;
        XfceDeviceScrollMethod default_value;
        XfceDeviceScrollMethod current;
    } scroll_method;

    struct
    {
        gboolean available;
        XfceDeviceClickMethod supported;
        XfceDeviceClickMethod default_value;
        XfceDeviceClickMethod current;
    } click_method;

    struct
    {
        gboolean available;
        XfceDeviceRotation default_value;
        XfceDeviceRotation current;
    } tablet_rotation;

    BoolSetting natural_scroll;
    BoolSetting left_handed;
    BoolSetting tap;
    BoolSetting dwt;
} XfceDeviceSettings;

typedef struct _XfceDevicePrivate
{
    gchar *name;
    gchar *xfconf_name;
    XfceDeviceCapabilities capabilities;
    XfconfChannel *channel;

    XfceDeviceSettings settings;

    // Set while a refresh runs, so the per-setting updates collapse into the
    // single "changed" emitted once the refresh is done.
    gboolean in_refresh;
} XfceDevicePrivate;

enum
{
    PROP_0,
    PROP_NAME,
    PROP_CAPABILITIES,
    PROP_CHANNEL,
};

enum
{
    CHANGED,
    N_SIGNALS,
};

G_DEFINE_FLAGS_TYPE (XfceDeviceCapabilities,
                     xfce_device_capabilities,
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_KEYBOARD, "keyboard"),
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_POINTER, "pointer"),
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_TOUCH, "touch"),
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_TABLET_TOOL, "tablet-tool"),
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_TABLET_PAD, "tablet-pad"),
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_GESTURE, "gesture"),
                     G_DEFINE_ENUM_VALUE (XFCE_DEVICE_CAPABILITIES_SWITCH, "switch"));

static void
xfce_device_constructed (GObject *object);
static void
xfce_device_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec);
static void
xfce_device_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec);
static void
xfce_device_finalize (GObject *object);

static gchar *
xfce_device_make_xfconf_name (const gchar *name);

static void
xfce_device_real_set_tap (XfceDevice *device,
                          gboolean tap);
static void
xfce_device_real_set_scroll_method (XfceDevice *device,
                                    XfceDeviceScrollMethod method);

G_DEFINE_TYPE_WITH_PRIVATE (XfceDevice, xfce_device, G_TYPE_OBJECT)


static guint signals[N_SIGNALS];

static void
xfce_device_class_init (XfceDeviceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->constructed = xfce_device_constructed;
    gobject_class->set_property = xfce_device_set_property;
    gobject_class->get_property = xfce_device_get_property;
    gobject_class->finalize = xfce_device_finalize;

    klass->set_tap = xfce_device_real_set_tap;
    klass->set_scroll_method = xfce_device_real_set_scroll_method;

    g_object_class_install_property (gobject_class,
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "name",
                                                          "name",
                                                          NULL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_CAPABILITIES,
                                     g_param_spec_flags ("capabilities",
                                                         "capabilities",
                                                         "capabilities",
                                                         XFCE_TYPE_DEVICE_CAPABILITIES,
                                                         0,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class,
                                     PROP_CHANNEL,
                                     g_param_spec_object ("channel",
                                                          "channel",
                                                          "channel",
                                                          XFCONF_TYPE_CHANNEL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    signals[CHANGED] = g_signal_new ("changed",
                                     G_TYPE_FROM_CLASS (klass),
                                     G_SIGNAL_RUN_LAST,
                                     0, NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);
}

static void
xfce_device_init (XfceDevice *device)
{
}

static void
xfce_device_constructed (GObject *object)
{
    XfceDevicePrivate *device = GET_PRIV (object);

    if (device->name != NULL)
    {
        device->xfconf_name = xfce_device_make_xfconf_name (device->name);
    }

    G_OBJECT_CLASS (xfce_device_parent_class)->constructed (object);
}

static void
xfce_device_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
    XfceDevicePrivate *device = GET_PRIV (object);

    switch (property_id)
    {
        case PROP_NAME:
            device->name = g_value_dup_string (value);
            break;

        case PROP_CAPABILITIES:
            device->capabilities = g_value_get_flags (value);
            break;

        case PROP_CHANNEL:
            g_set_object (&device->channel, g_value_get_object (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
    XfceDevicePrivate *device = GET_PRIV (object);

    switch (property_id)
    {
        case PROP_NAME:
            g_value_set_string (value, device->name);
            break;

        case PROP_CAPABILITIES:
            g_value_set_flags (value, device->capabilities);
            break;

        case PROP_CHANNEL:
            g_value_set_object (value, device->channel);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_finalize (GObject *object)
{
    XfceDevicePrivate *device = GET_PRIV (object);

    g_free (device->name);
    g_free (device->xfconf_name);
    g_clear_object (&device->channel);

    G_OBJECT_CLASS (xfce_device_parent_class)->finalize (object);
}

// Build an xfconf property name with only valid characters. NOTE: this must
// stay identical to the equivalent function in the xfsettingsd pointers
// helper.
static gchar *
xfce_device_make_xfconf_name (const gchar *name)
{
    GString *string = g_string_sized_new (strlen (name));

    for (const gchar *p = name; *p != '\0'; p++)
    {
        if ((*p >= 'A' && *p <= 'Z')
            || (*p >= 'a' && *p <= 'z')
            || (*p >= '0' && *p <= '9')
            || *p == '_' || *p == '-')
        {
            string = g_string_append_c (string, *p);
        }
        else if (*p == ' ')
        {
            string = g_string_append_c (string, '_');
        }
    }

    return g_string_free (string, FALSE);
}


void
xfce_device_refresh (XfceDevice *device)
{
    XfceDeviceClass *klass = XFCE_DEVICE_GET_CLASS (device);

    if (klass->refresh != NULL)
    {
        // Nothing about the previous read survives: a setting the backend no
        // longer reports must not keep its old value, nor stay available behind
        // it. The refresh republishes whatever the device still has.
        XfceDevicePrivate *priv = GET_PRIV (device);
        memset (&priv->settings, 0, sizeof (priv->settings));

        // The whole re-read is one change as far as anyone watching is
        // concerned, so hold the per-setting emissions back and send one.
        priv->in_refresh = TRUE;
        klass->refresh (device);
        priv->in_refresh = FALSE;

        g_signal_emit (device, signals[CHANGED], 0);
    }
}

static void
xfce_device_emit_changed (XfceDevice *device)
{
    if (!GET_PRIV (device)->in_refresh)
    {
        g_signal_emit (device, signals[CHANGED], 0);
    }
}

static void
xfce_device_update_bool_setting (XfceDevice *device,
                                 BoolSetting *setting,
                                 gboolean default_value,
                                 gboolean current_value)
{
    setting->available = TRUE;
    setting->default_value = default_value;
    setting->current = current_value;
    xfce_device_emit_changed (device);
}

const gchar *
xfce_device_get_name (XfceDevice *device)
{
    return GET_PRIV (device)->name;
}

XfceDeviceCapabilities
xfce_device_get_capabilities (XfceDevice *device)
{
    return GET_PRIV (device)->capabilities;
}

gboolean
xfce_device_get_send_events_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.send_events.available;
}

gboolean
xfce_device_get_enabled (XfceDevice *device)
{
    // "Disabled on external mouse" reads as enabled for the binary switch; a
    // device with no send-events setting reads as disabled (matching a missing
    // "Device Enabled" property).
    XfceDevicePrivate *priv = GET_PRIV (device);
    return priv->settings.send_events.available && priv->settings.send_events.current != XFCE_DEVICE_SEND_EVENTS_DISABLED;
}

gboolean
xfce_device_get_acceleration_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.accel_speed.available;
}

gdouble
xfce_device_get_acceleration (XfceDevice *device)
{
    return GET_PRIV (device)->settings.accel_speed.current;
}

gboolean
xfce_device_get_accel_profile_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.accel_profile.available;
}

XfceDeviceAccelProfile
xfce_device_get_accel_profile_supported (XfceDevice *device)
{
    return GET_PRIV (device)->settings.accel_profile.supported;
}

XfceDeviceAccelProfile
xfce_device_get_accel_profile (XfceDevice *device)
{
    return GET_PRIV (device)->settings.accel_profile.current;
}

gboolean
xfce_device_get_natural_scroll_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.natural_scroll.available;
}

gboolean
xfce_device_get_natural_scroll (XfceDevice *device)
{
    return GET_PRIV (device)->settings.natural_scroll.current;
}

gboolean
xfce_device_get_left_handed_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.left_handed.available;
}

gboolean
xfce_device_get_left_handed (XfceDevice *device)
{
    return GET_PRIV (device)->settings.left_handed.current;
}

gboolean
xfce_device_get_tap_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.tap.available;
}

gboolean
xfce_device_get_tap (XfceDevice *device)
{
    return GET_PRIV (device)->settings.tap.current;
}

gboolean
xfce_device_get_scroll_method_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.scroll_method.available;
}

XfceDeviceScrollMethod
xfce_device_get_scroll_method_supported (XfceDevice *device)
{
    return GET_PRIV (device)->settings.scroll_method.supported;
}

XfceDeviceScrollMethod
xfce_device_get_scroll_method (XfceDevice *device)
{
    return GET_PRIV (device)->settings.scroll_method.current;
}

gboolean
xfce_device_get_click_method_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.click_method.available;
}

XfceDeviceClickMethod
xfce_device_get_click_method_supported (XfceDevice *device)
{
    return GET_PRIV (device)->settings.click_method.supported;
}

XfceDeviceClickMethod
xfce_device_get_click_method (XfceDevice *device)
{
    return GET_PRIV (device)->settings.click_method.current;
}

gboolean
xfce_device_get_tablet_rotation_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.tablet_rotation.available;
}

XfceDeviceRotation
xfce_device_get_tablet_rotation (XfceDevice *device)
{
    return GET_PRIV (device)->settings.tablet_rotation.current;
}

XfceDeviceRotation
xfce_device_get_touchscreen_rotation (XfceDevice *device)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    gchar *prop = _xfce_device_prop (device, "/Rotation");
    gint degrees = xfconf_channel_get_int (priv->channel, prop, 0);
    g_free (prop);

    switch (degrees)
    {
        case 90:
            return XFCE_DEVICE_ROTATION_90;
        case 180:
            return XFCE_DEVICE_ROTATION_180;
        case 270:
            return XFCE_DEVICE_ROTATION_270;
        default:
            return XFCE_DEVICE_ROTATION_NONE;
    }
}

XfceDeviceReflection
xfce_device_get_touchscreen_reflection (XfceDevice *device)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    gchar *prop = _xfce_device_prop (device, "/Reflection");
    gchar *axes = xfconf_channel_get_string (priv->channel, prop, NULL);
    g_free (prop);

    XfceDeviceReflection reflection;
    if (g_strcmp0 (axes, "X") == 0)
    {
        reflection = XFCE_DEVICE_REFLECTION_X;
    }
    else if (g_strcmp0 (axes, "Y") == 0)
    {
        reflection = XFCE_DEVICE_REFLECTION_Y;
    }
    else if (g_strcmp0 (axes, "XY") == 0)
    {
        reflection = XFCE_DEVICE_REFLECTION_XY;
    }
    else
    {
        reflection = XFCE_DEVICE_REFLECTION_NONE;
    }
    g_free (axes);

    return reflection;
}

gchar *
xfce_device_get_assigned_monitor (XfceDevice *device)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    gchar *prop = _xfce_device_prop (device, "/AssignedMonitor");
    gchar *edid = xfconf_channel_get_string (priv->channel, prop, NULL);
    g_free (prop);

    return edid;
}

gboolean
xfce_device_get_dwt_available (XfceDevice *device)
{
    return GET_PRIV (device)->settings.dwt.available;
}

gboolean
xfce_device_get_dwt (XfceDevice *device)
{
    return GET_PRIV (device)->settings.dwt.current;
}

gchar *
_xfce_device_prop (XfceDevice *device,
                   const gchar *suffix)
{
    return g_strconcat ("/", GET_PRIV (device)->xfconf_name, suffix, NULL);
}

gchar *
_xfce_device_libinput_prop (XfceDevice *device,
                            const gchar *libinput_prop)
{
    gchar *prop = g_strconcat ("/", GET_PRIV (device)->xfconf_name, "/Properties/", libinput_prop, NULL);
    g_strdelimit (prop, " ", '_');
    return prop;
}

void
xfce_device_set_enabled (XfceDevice *device,
                         gboolean enabled)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.send_events.current = enabled ? XFCE_DEVICE_SEND_EVENTS_ENABLED : XFCE_DEVICE_SEND_EVENTS_DISABLED;

    gchar *prop = _xfce_device_prop (device, "/Properties/Device_Enabled");
    xfconf_channel_set_int (priv->channel, prop, enabled);
    g_free (prop);
}

void
xfce_device_set_acceleration (XfceDevice *device,
                              gdouble acceleration)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.accel_speed.current = acceleration;

    gchar *prop = _xfce_device_prop (device, "/Acceleration");
    if (xfconf_channel_get_double (priv->channel, prop, -1.0) != acceleration)
    {
        xfconf_channel_set_double (priv->channel, prop, acceleration);
    }
    g_free (prop);
}

void
xfce_device_set_accel_profile (XfceDevice *device,
                               gboolean adaptive)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.accel_profile.current = adaptive ? XFCE_DEVICE_ACCEL_PROFILE_ADAPTIVE : XFCE_DEVICE_ACCEL_PROFILE_FLAT;

    gint adaptive_value = adaptive ? 1 : 0;
    gint flat = adaptive ? 0 : 1;
    gint custom = 0;

    gchar *prop = _xfce_device_libinput_prop (device, LIBINPUT_PROP_ACCEL_PROFILE_ENABLED);
    if (priv->settings.accel_profile.has_custom_slot)
    {
        xfconf_channel_set_array (priv->channel, prop,
                                  G_TYPE_INT, &adaptive_value, G_TYPE_INT, &flat, G_TYPE_INT, &custom, G_TYPE_INVALID);
    }
    else
    {
        xfconf_channel_set_array (priv->channel, prop,
                                  G_TYPE_INT, &adaptive_value, G_TYPE_INT, &flat, G_TYPE_INVALID);
    }
    g_free (prop);
}

void
xfce_device_set_natural_scroll (XfceDevice *device,
                                gboolean natural_scroll)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.natural_scroll.current = natural_scroll;

    gchar *prop = _xfce_device_prop (device, "/ReverseScrolling");
    if (xfconf_channel_get_bool (priv->channel, prop, FALSE) != natural_scroll)
    {
        xfconf_channel_set_bool (priv->channel, prop, natural_scroll);
    }
    g_free (prop);
}

void
xfce_device_set_left_handed (XfceDevice *device,
                             gboolean left_handed)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.left_handed.current = left_handed;

    gboolean right_handed = !left_handed;
    gchar *prop = _xfce_device_prop (device, "/RightHanded");
    if (!xfconf_channel_has_property (priv->channel, prop)
        || xfconf_channel_get_bool (priv->channel, prop, TRUE) != right_handed)
    {
        xfconf_channel_set_bool (priv->channel, prop, right_handed);
    }
    g_free (prop);
}

void
xfce_device_set_tap (XfceDevice *device,
                     gboolean tap)
{
    GET_PRIV (device)->settings.tap.current = tap;
    XFCE_DEVICE_GET_CLASS (device)->set_tap (device, tap);
}

static void
xfce_device_real_set_tap (XfceDevice *device,
                          gboolean tap)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    gchar *prop = _xfce_device_libinput_prop (device, LIBINPUT_PROP_TAP);
    xfconf_channel_set_int (priv->channel, prop, tap);
    g_free (prop);
}

void
xfce_device_set_scroll_method (XfceDevice *device,
                               XfceDeviceScrollMethod method)
{
    // Recorded here rather than in the default implementation: a subclass may
    // pass a different method up to it when the backend cannot express this one.
    GET_PRIV (device)->settings.scroll_method.current = method;
    XFCE_DEVICE_GET_CLASS (device)->set_scroll_method (device, method);
}

static void
xfce_device_real_set_scroll_method (XfceDevice *device,
                                    XfceDeviceScrollMethod method)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    gint two_finger = (method == XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER);
    gint edge = (method == XFCE_DEVICE_SCROLL_METHOD_EDGE);
    gint button = (method == XFCE_DEVICE_SCROLL_METHOD_ON_BUTTON_DOWN);

    gchar *prop = _xfce_device_libinput_prop (device, LIBINPUT_PROP_SCROLL_METHOD_ENABLED);
    xfconf_channel_set_array (priv->channel, prop,
                              G_TYPE_INT, &two_finger, G_TYPE_INT, &edge, G_TYPE_INT, &button, G_TYPE_INVALID);
    g_free (prop);
}

void
xfce_device_set_click_method (XfceDevice *device,
                              XfceDeviceClickMethod method)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.click_method.current = method;

    gint button_areas = (method == XFCE_DEVICE_CLICK_METHOD_BUTTON_AREAS);
    gint clickfinger = (method == XFCE_DEVICE_CLICK_METHOD_CLICKFINGER);

    gchar *prop = _xfce_device_libinput_prop (device, LIBINPUT_PROP_CLICK_METHOD_ENABLED);
    xfconf_channel_set_array (priv->channel, prop,
                              G_TYPE_INT, &button_areas, G_TYPE_INT, &clickfinger, G_TYPE_INVALID);
    g_free (prop);
}

void
xfce_device_set_tablet_rotation (XfceDevice *device,
                                 XfceDeviceRotation rotation)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.tablet_rotation.current = rotation;

    /* the wacom driver orders these none, clockwise, counter-clockwise, half */
    gint wacom;
    switch (rotation)
    {
        case XFCE_DEVICE_ROTATION_90:
            wacom = 1;
            break;
        case XFCE_DEVICE_ROTATION_180:
            wacom = 3;
            break;
        case XFCE_DEVICE_ROTATION_270:
            wacom = 2;
            break;
        default:
            wacom = 0;
            break;
    }

    gchar *prop = _xfce_device_prop (device, "/Properties/Wacom_Rotation");
    xfconf_channel_set_int (priv->channel, prop, wacom);
    g_free (prop);
}

void
xfce_device_set_touchscreen_rotation (XfceDevice *device,
                                      XfceDeviceRotation rotation)
{
    gint degrees;
    switch (rotation)
    {
        case XFCE_DEVICE_ROTATION_90:
            degrees = 90;
            break;
        case XFCE_DEVICE_ROTATION_180:
            degrees = 180;
            break;
        case XFCE_DEVICE_ROTATION_270:
            degrees = 270;
            break;
        default:
            degrees = 0;
            break;
    }

    gchar *prop = _xfce_device_prop (device, "/Rotation");
    xfconf_channel_set_int (GET_PRIV (device)->channel, prop, degrees);
    g_free (prop);
}

void
xfce_device_set_touchscreen_reflection (XfceDevice *device,
                                        XfceDeviceReflection reflection)
{
    const gchar *axes;
    switch (reflection)
    {
        case XFCE_DEVICE_REFLECTION_X:
            axes = "X";
            break;
        case XFCE_DEVICE_REFLECTION_Y:
            axes = "Y";
            break;
        case XFCE_DEVICE_REFLECTION_XY:
            axes = "XY";
            break;
        default:
            axes = "0";
            break;
    }

    gchar *prop = _xfce_device_prop (device, "/Reflection");
    xfconf_channel_set_string (GET_PRIV (device)->channel, prop, axes);
    g_free (prop);
}

void
xfce_device_set_assigned_monitor (XfceDevice *device,
                                  const gchar *edid)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    gchar *prop = _xfce_device_prop (device, "/AssignedMonitor");

    if (edid != NULL)
    {
        xfconf_channel_set_string (priv->channel, prop, edid);
    }
    else
    {
        xfconf_channel_reset_property (priv->channel, prop, FALSE);
    }
    g_free (prop);
}

void
xfce_device_set_dwt (XfceDevice *device,
                     gboolean dwt)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.dwt.current = dwt;

    gchar *prop = _xfce_device_libinput_prop (device, LIBINPUT_PROP_DISABLE_WHILE_TYPING);
    xfconf_channel_set_int (priv->channel, prop, dwt);
    g_free (prop);
}

XfconfChannel *
_xfce_device_get_channel (XfceDevice *device)
{
    return GET_PRIV (device)->channel;
}

void
_xfce_device_update_capabilities (XfceDevice *device,
                                  XfceDeviceCapabilities capabilities)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->capabilities = capabilities;
    xfce_device_emit_changed (device);
}

void
_xfce_device_update_send_events_state (XfceDevice *device,
                                       XfceDeviceSendEvents supported,
                                       XfceDeviceSendEvents default_value,
                                       XfceDeviceSendEvents current_value)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.send_events.available = TRUE;
    priv->settings.send_events.supported = supported;
    priv->settings.send_events.default_value = default_value;
    priv->settings.send_events.current = current_value;
    xfce_device_emit_changed (device);
}

void
_xfce_device_update_accel_speed (XfceDevice *device,
                                 gdouble default_value,
                                 gdouble current_value)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.accel_speed.available = TRUE;
    priv->settings.accel_speed.default_value = default_value;
    priv->settings.accel_speed.current = current_value;
    xfce_device_emit_changed (device);
}

void
_xfce_device_update_accel_profile (XfceDevice *device,
                                   XfceDeviceAccelProfile supported,
                                   XfceDeviceAccelProfile default_value,
                                   XfceDeviceAccelProfile current_value,
                                   gboolean custom_slot)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.accel_profile.available = TRUE;
    priv->settings.accel_profile.supported = supported;
    priv->settings.accel_profile.default_value = default_value;
    priv->settings.accel_profile.current = current_value;
    priv->settings.accel_profile.has_custom_slot = custom_slot;
    xfce_device_emit_changed (device);
}

void
_xfce_device_update_natural_scroll (XfceDevice *device,
                                    gboolean default_value,
                                    gboolean current_value)
{
    xfce_device_update_bool_setting (device, &GET_PRIV (device)->settings.natural_scroll,
                                     default_value, current_value);
}

void
_xfce_device_update_left_handed (XfceDevice *device,
                                 gboolean default_value,
                                 gboolean current_value)
{
    xfce_device_update_bool_setting (device, &GET_PRIV (device)->settings.left_handed,
                                     default_value, current_value);
}

void
_xfce_device_update_tap (XfceDevice *device,
                         gboolean default_value,
                         gboolean current_value)
{
    xfce_device_update_bool_setting (device, &GET_PRIV (device)->settings.tap,
                                     default_value, current_value);
}

void
_xfce_device_update_scroll_method (XfceDevice *device,
                                   XfceDeviceScrollMethod supported,
                                   XfceDeviceScrollMethod default_value,
                                   XfceDeviceScrollMethod current_value)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.scroll_method.available = TRUE;
    priv->settings.scroll_method.supported = supported;
    priv->settings.scroll_method.default_value = default_value;
    priv->settings.scroll_method.current = current_value;
    xfce_device_emit_changed (device);
}

void
_xfce_device_update_click_method (XfceDevice *device,
                                  XfceDeviceClickMethod supported,
                                  XfceDeviceClickMethod default_value,
                                  XfceDeviceClickMethod current_value)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.click_method.available = TRUE;
    priv->settings.click_method.supported = supported;
    priv->settings.click_method.default_value = default_value;
    priv->settings.click_method.current = current_value;
    xfce_device_emit_changed (device);
}

XfceDeviceRotation
_xfce_device_tablet_rotation_from_wacom (gint wacom)
{
    switch (wacom)
    {
        case 1:
            return XFCE_DEVICE_ROTATION_90;
        case 2:
            return XFCE_DEVICE_ROTATION_270;
        case 3:
            return XFCE_DEVICE_ROTATION_180;
        default:
            return XFCE_DEVICE_ROTATION_NONE;
    }
}

void
_xfce_device_update_tablet_rotation (XfceDevice *device,
                                     XfceDeviceRotation default_value,
                                     XfceDeviceRotation current_value)
{
    XfceDevicePrivate *priv = GET_PRIV (device);
    priv->settings.tablet_rotation.available = TRUE;
    priv->settings.tablet_rotation.default_value = default_value;
    priv->settings.tablet_rotation.current = current_value;
    xfce_device_emit_changed (device);
}

void
_xfce_device_update_dwt (XfceDevice *device,
                         gboolean default_value,
                         gboolean current_value)
{
    xfce_device_update_bool_setting (device, &GET_PRIV (device)->settings.dwt,
                                     default_value, current_value);
}

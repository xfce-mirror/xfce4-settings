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

#include "xfce-device-x11.h"

#include "common/libinput-properties.h"

#include <X11/Xatom.h>
#include <X11/extensions/XInput.h>
#include <gdk/gdkx.h>

struct _XfceDeviceX11
{
    XfceDevice parent_instance;

    XID xid;

    // Device type, used for tab visibility
    gboolean is_libinput;
    gboolean is_synaptics;
    gboolean is_wacom;

    // X11-only state kept for the legacy tabs
    gint threshold;
    gboolean has_hires_scrolling;
    gboolean hires_scrolling;
    gint wacom_mode;

    // The synaptics scroll arrays bundle the direction with the method, so the
    // horizontal state is kept here to rewrite them whenever either changes.
    // The method itself is the shared one; do not cache a second copy.
    gboolean synaptics_horizontal;
};

enum
{
    PROP_0,
    PROP_XID,
};

typedef union
{
    gchar c;
    guchar uc;
    gint16 i16;
    guint16 u16;
    gint32 i32;
    guint32 u32;
    float f;
    Atom a;
} propdata_t;

static void
xfce_device_x11_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec);
static void
xfce_device_x11_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec);
static void
xfce_device_x11_refresh (XfceDevice *device);
static void
xfce_device_x11_set_tap (XfceDevice *device,
                         gboolean tap);
static void
xfce_device_x11_set_scroll_method (XfceDevice *device,
                                   XfceDeviceScrollMethod method);


G_DEFINE_FINAL_TYPE (XfceDeviceX11, xfce_device_x11, XFCE_TYPE_DEVICE)


// Requires libinput 1.23.0
static gboolean libinput_supports_custom_accel_profile = FALSE;

static void
xfce_device_x11_class_init (XfceDeviceX11Class *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->set_property = xfce_device_x11_set_property;
    gobject_class->get_property = xfce_device_x11_get_property;

    XfceDeviceClass *device_class = XFCE_DEVICE_CLASS (klass);
    device_class->refresh = xfce_device_x11_refresh;
    device_class->set_tap = xfce_device_x11_set_tap;
    device_class->set_scroll_method = xfce_device_x11_set_scroll_method;

    g_object_class_install_property (gobject_class,
                                     PROP_XID,
                                     g_param_spec_ulong ("xid",
                                                         "xid",
                                                         "xid",
                                                         0, G_MAXULONG, 0,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
xfce_device_x11_init (XfceDeviceX11 *device)
{
    device->threshold = -1;
    device->wacom_mode = -1;
}

static void
xfce_device_x11_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    XfceDeviceX11 *device = XFCE_DEVICE_X11 (object);

    switch (property_id)
    {
        case PROP_XID:
            device->xid = g_value_get_ulong (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
xfce_device_x11_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    XfceDeviceX11 *device = XFCE_DEVICE_X11 (object);

    switch (property_id)
    {
        case PROP_XID:
            g_value_set_ulong (value, device->xid);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

XfceDevice *
xfce_device_x11_new (XfconfChannel *channel,
                     XID xid,
                     const gchar *name)
{
    return g_object_new (XFCE_TYPE_DEVICE_X11,
                         "channel", channel,
                         "name", name,
                         "capabilities", XFCE_DEVICE_CAPABILITIES_POINTER,
                         "xid", (gulong) xid,
                         NULL);
}

XID
xfce_device_x11_get_xid (XfceDeviceX11 *device)
{
    return device->xid;
}

static gboolean
xfce_device_x11_get_device_prop (Display *xdisplay,
                                 XDevice *device,
                                 const gchar *prop_name,
                                 Atom type,
                                 guint n_items,
                                 propdata_t *retval)
{
    Atom prop = XInternAtom (xdisplay, prop_name, False);
    Atom float_type = XInternAtom (xdisplay, "FLOAT", False);
    Atom type_ret;
    gint format;
    gulong n_items_ret, bytes_after;
    guchar *data;
    gint rc;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    rc = XGetDeviceProperty (xdisplay, device, prop, 0, 1, False,
                             type, &type_ret, &format, &n_items_ret,
                             &bytes_after, &data);
    gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
    if (rc == Success && type_ret == type && n_items_ret >= n_items)
    {
        gboolean success = TRUE;
        gint size;
        guchar *ptr;

        switch (format)
        {
            case 8:
                size = sizeof (gchar);
                break;
            case 16:
                size = sizeof (gint16);
                break;
            case 32:
            default:
                size = sizeof (gint32);
                break;
        }
        ptr = data;

        for (guint i = 0; i < n_items; i++)
        {
            switch (type_ret)
            {
                case XA_INTEGER:
                    switch (format)
                    {
                        case 8:
                            retval[i].c = *((gchar *) ptr);
                            break;
                        case 16:
                            retval[i].i16 = *((gint16 *) (gpointer) ptr);
                            break;
                        case 32:
                            retval[i].i32 = *((gint32 *) (gpointer) ptr);
                            break;
                    }
                    break;
                case XA_CARDINAL:
                    switch (format)
                    {
                        case 8:
                            retval[i].uc = *((guchar *) ptr);
                            break;
                        case 16:
                            retval[i].u16 = *((guint16 *) (gpointer) ptr);
                            break;
                        case 32:
                            retval[i].u32 = *((guint32 *) (gpointer) ptr);
                            break;
                    }
                    break;
                case XA_ATOM:
                    retval[i].a = *((Atom *) (gpointer) ptr);
                    break;
                default:
                    if (type_ret == float_type)
                    {
                        retval[i].f = *((float *) (gpointer) ptr);
                    }
                    else
                    {
                        success = FALSE;
                        g_warning ("Unhandled type, please implement it");
                    }
                    break;
            }
            ptr += size;
        }
        XFree (data);

        return success;
    }

    return FALSE;
}

static gboolean
xfce_device_x11_get_libinput_accel (Display *xdisplay,
                                    XDevice *device,
                                    gdouble *val)
{
    propdata_t pdata[1] = { 0 };
    Atom float_type = XInternAtom (xdisplay, "FLOAT", False);

    if (xfce_device_x11_get_device_prop (xdisplay, device, LIBINPUT_PROP_ACCEL, float_type, 1, &pdata[0]))
    {
        *val = (gdouble) (pdata[0].f + 1.0) * 5.0;
        return TRUE;
    }

    return FALSE;
}

static gboolean
xfce_device_x11_get_libinput_boolean (Display *xdisplay,
                                      XDevice *device,
                                      const gchar *prop_name,
                                      gboolean *val)
{
    propdata_t pdata[1] = { 0 };

    if (xfce_device_x11_get_device_prop (xdisplay, device, prop_name, XA_INTEGER, 1, &pdata[0]))
    {
        *val = (gboolean) (pdata[0].c);
        return TRUE;
    }

    return FALSE;
}

static gboolean
xfce_device_x11_get_libinput_click_method (Display *xdisplay,
                                           XDevice *device,
                                           const gchar *prop_name,
                                           XfceDeviceClickMethod *click_method)
{
    propdata_t pdata[2] = { 0 };

    if (xfce_device_x11_get_device_prop (xdisplay, device, prop_name, XA_INTEGER, 2, &pdata[0]))
    {
        // The driver's array is ordered [button areas, clickfinger].
        *click_method = XFCE_DEVICE_CLICK_METHOD_NONE;
        if (pdata[0].c)
        {
            *click_method |= XFCE_DEVICE_CLICK_METHOD_BUTTON_AREAS;
        }
        if (pdata[1].c)
        {
            *click_method |= XFCE_DEVICE_CLICK_METHOD_CLICKFINGER;
        }
        return TRUE;
    }

    return FALSE;
}

static gboolean
xfce_device_x11_get_libinput_accel_profile (Display *xdisplay,
                                            XDevice *device,
                                            const gchar *prop_name,
                                            XfceDeviceAccelProfile *accel_profile)
{
    propdata_t pdata[3] = { 0 };
    gboolean ok;

    ok = xfce_device_x11_get_device_prop (xdisplay, device, prop_name, XA_INTEGER, 3, &pdata[0]);
    if (ok)
    {
        libinput_supports_custom_accel_profile = TRUE;
    }
    else if (!libinput_supports_custom_accel_profile)
    {
        ok = xfce_device_x11_get_device_prop (xdisplay, device, prop_name, XA_INTEGER, 2, &pdata[0]);
    }

    if (ok)
    {
        // The driver's array is ordered [adaptive, flat, custom], which is not
        // the order the shared enum numbers them in.
        *accel_profile = XFCE_DEVICE_ACCEL_PROFILE_NONE;
        if (pdata[0].c)
        {
            *accel_profile |= XFCE_DEVICE_ACCEL_PROFILE_ADAPTIVE;
        }
        if (pdata[1].c)
        {
            *accel_profile |= XFCE_DEVICE_ACCEL_PROFILE_FLAT;
        }
        if (pdata[2].c)
        {
            *accel_profile |= XFCE_DEVICE_ACCEL_PROFILE_CUSTOM;
        }
        return TRUE;
    }

    return FALSE;
}

static gint
xfce_device_x11_get_int_property (XDevice *device,
                                  Atom prop,
                                  guint offset,
                                  gint *horiz)
{
    Atom type;
    gint format;
    gulong n_items, bytes_after;
    guchar *data;
    gint val = -1;
    gint res;

    gdk_x11_display_error_trap_push (gdk_display_get_default ());
    res = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                              device, prop, 0, 1000, False,
                              AnyPropertyType, &type, &format,
                              &n_items, &bytes_after, &data);
    if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) == 0 && res == Success)
    {
        if (type == XA_INTEGER)
        {
            if (n_items > offset)
            {
                val = data[offset];
            }
            if (n_items > 1 + offset && horiz != NULL)
            {
                *horiz = data[offset + 1];
            }
        }
        XFree (data);
    }

    return val;
}

static void
xfce_device_x11_refresh (XfceDevice *base)
{
    GdkDisplay *gdk_display = gdk_display_get_default ();
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display);
    XfceDeviceX11 *self = XFCE_DEVICE_X11 (base);
    XDevice *device;

    // The base has already discarded its half of the previous read, so discard
    // this half too rather than leave it behind if the device cannot be opened.
    self->is_libinput = FALSE;
    self->is_synaptics = FALSE;
    self->is_wacom = FALSE;
    self->threshold = -1;
    self->has_hires_scrolling = FALSE;
    self->hires_scrolling = FALSE;
    self->wacom_mode = -1;
    self->synaptics_horizontal = FALSE;

    gdk_x11_display_error_trap_push (gdk_display);
    device = XOpenDevice (xdisplay, self->xid);
    if (gdk_x11_display_error_trap_pop (gdk_display) != 0 || device == NULL)
    {
        g_warning ("Unable to open device %lu", self->xid);
        return;
    }

    // number of buttons and (wacom) valuator mode
    gint nbuttons = 0;
    gint wacom_mode = -1;
    gint ndevices = 0;
    gdk_x11_display_error_trap_push (gdk_display);
    XDeviceInfo *device_info = XListInputDevices (xdisplay, &ndevices);
    if (gdk_x11_display_error_trap_pop (gdk_display) == 0 && device_info != NULL)
    {
        for (gint i = 0; i < ndevices; i++)
        {
            if (device_info[i].id != self->xid)
            {
                continue;
            }

            XAnyClassPtr any = device_info[i].inputclassinfo;
            for (gint n = 0; n < device_info[i].num_classes; n++)
            {
                if (any->class == ButtonClass)
                {
                    nbuttons = ((XButtonInfoPtr) any)->num_buttons;
                }
                else if (any->class == ValuatorClass)
                {
                    wacom_mode = ((XValuatorInfoPtr) any)->mode == Absolute ? 0 : 1;
                }
                any = (XAnyClassPtr) (gpointer) ((gchar *) any + any->length);
            }
            break;
        }
        XFreeDeviceList (device_info);
    }

    gboolean left_handed = FALSE;
    gboolean reverse_scrolling = FALSE;
    gboolean is_libinput = xfce_device_x11_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_LEFT_HANDED, &left_handed);
    xfce_device_x11_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_NATURAL_SCROLL, &reverse_scrolling);

    gboolean hires_scrolling = FALSE;
    gboolean has_hires_scrolling = xfce_device_x11_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_HIRES_WHEEL_SCROLL_ENABLED, &hires_scrolling);

    XfceDeviceAccelProfile accel_profile_available = XFCE_DEVICE_ACCEL_PROFILE_NONE;
    XfceDeviceAccelProfile accel_profile_current = XFCE_DEVICE_ACCEL_PROFILE_NONE;
    gboolean has_accel_profile = FALSE;
    if (xfce_device_x11_get_libinput_accel_profile (xdisplay, device, LIBINPUT_PROP_ACCEL_PROFILES_AVAILABLE, &accel_profile_available))
    {
        has_accel_profile = xfce_device_x11_get_libinput_accel_profile (xdisplay, device, LIBINPUT_PROP_ACCEL_PROFILE_ENABLED, &accel_profile_current);
    }

    if (!is_libinput)
    {
        if (nbuttons > 0)
        {
            guchar *buttonmap = g_new0 (guchar, nbuttons);
            gint id_1 = 0, id_3 = 0, id_4 = 0, id_5 = 0, id_6 = 0, id_7 = 0;

            gdk_x11_display_error_trap_push (gdk_display);
            XGetDeviceButtonMapping (xdisplay, device, buttonmap, nbuttons);
            if (gdk_x11_display_error_trap_pop (gdk_display) != 0)
            {
                g_critical ("Failed to get button map");
            }

            for (gint i = 0; i < nbuttons; i++)
            {
                if (buttonmap[i] == 1)
                {
                    id_1 = i;
                }
                else if (buttonmap[i] == (nbuttons < 3 ? 2 : 3))
                {
                    id_3 = i;
                }
                else if (buttonmap[i] == 4)
                {
                    id_4 = i;
                }
                else if (buttonmap[i] == 5)
                {
                    id_5 = i;
                }
                else if (buttonmap[i] == 6)
                {
                    id_6 = i;
                }
                else if (buttonmap[i] == 7)
                {
                    id_7 = i;
                }
            }
            g_free (buttonmap);
            left_handed = (id_1 > id_3);
            reverse_scrolling = (id_5 < id_4) && (id_7 < id_6);
        }
        else
        {
            g_critical ("Device has no buttons");
        }
    }

    gdouble acceleration = -1.0;
    gint threshold = -1;
    if (!xfce_device_x11_get_libinput_accel (xdisplay, device, &acceleration))
    {
        gint nstates = 0;
        gdk_x11_display_error_trap_push (gdk_display);
        XFeedbackState *states = XGetFeedbackControl (xdisplay, device, &nstates);
        if (gdk_x11_display_error_trap_pop (gdk_display) != 0 || states == NULL)
        {
            g_critical ("Failed to get feedback states");
        }
        else
        {
            XFeedbackState *pt = states;
            for (gint i = 0; i < nstates; i++)
            {
                if (pt->class == PtrFeedbackClass)
                {
                    XPtrFeedbackState *state = (XPtrFeedbackState *) pt;
                    acceleration = (gdouble) state->accelNum / (gdouble) state->accelDenom;
                    threshold = state->threshold;
                }
                pt = (XFeedbackState *) (gpointer) ((gchar *) pt + pt->length);
            }
            XFreeFeedbackList (states);
        }
    }

    gint is_enabled = -1;
    gboolean is_synaptics = FALSE, is_wacom = FALSE, is_touchscreen = FALSE;
    gint synaptics_tap_to_click = -1;
    gint synaptics_edge_scroll = -1, synaptics_edge_hscroll = -1;
    gint synaptics_two_scroll = -1, synaptics_two_hscroll = -1;
    gint synaptics_circ_scroll = -1;
    gint wacom_rotation = -1;
    gint libinput_dwt = -1;
    gboolean has_click_method = FALSE;
    XfceDeviceClickMethod click_methods_available = XFCE_DEVICE_CLICK_METHOD_NONE;
    XfceDeviceClickMethod click_method_current = XFCE_DEVICE_CLICK_METHOD_NONE;

    Atom device_enabled_prop = XInternAtom (xdisplay, "Device Enabled", True);
    Atom synaptics_prop = XInternAtom (xdisplay, "Synaptics Off", True);
    Atom wacom_prop = XInternAtom (xdisplay, "Wacom Tool Type", True);
    // libinput maps touch coordinates onto the display through its calibration
    // matrix, while the legacy input stack exposes touch axis data instead.
    // Both are looked for: is_libinput cannot decide between them, because it
    // comes from a property libinput only creates for devices that can be
    // switched to left-handed, which a touchscreen cannot.
    Atom touchscreen_prop = XInternAtom (xdisplay, LIBINPUT_PROP_CALIBRATION, True);
    Atom touchscreen_legacy_prop = XInternAtom (xdisplay, "Abs MT Position X", True);
    Atom synaptics_tap_prop = XInternAtom (xdisplay, "Synaptics Tap Action", True);
    Atom synaptics_edge_scroll_prop = XInternAtom (xdisplay, "Synaptics Edge Scrolling", True);
    Atom synaptics_two_scroll_prop = XInternAtom (xdisplay, "Synaptics Two-Finger Scrolling", True);
    Atom synaptics_circ_scroll_prop = XInternAtom (xdisplay, "Synaptics Circular Scrolling", True);
    Atom wacom_rotation_prop = XInternAtom (xdisplay, "Wacom Rotation", True);
    Atom libinput_dwt_prop = XInternAtom (xdisplay, LIBINPUT_PROP_DISABLE_WHILE_TYPING, True);
    Atom libinput_tap_prop = XInternAtom (xdisplay, LIBINPUT_PROP_TAP, True);
    Atom libinput_scroll_methods_prop = XInternAtom (xdisplay, LIBINPUT_PROP_SCROLL_METHOD_ENABLED, True);
    Atom libinput_click_method_prop = XInternAtom (xdisplay, LIBINPUT_PROP_CLICK_METHOD_ENABLED, True);

    gint nprops = 0;
    gdk_x11_display_error_trap_push (gdk_display);
    Atom *props = XListDeviceProperties (xdisplay, device, &nprops);
    if (gdk_x11_display_error_trap_pop (gdk_display) == 0 && props != NULL)
    {
        for (gint i = 0; i < nprops; i++)
        {
            if (props[i] == device_enabled_prop)
            {
                is_enabled = xfce_device_x11_get_int_property (device, props[i], 0, NULL);
            }
            else if (props[i] == synaptics_prop)
            {
                is_synaptics = TRUE;
            }
            else if (props[i] == wacom_prop)
            {
                is_wacom = TRUE;
            }
            else if (props[i] == touchscreen_prop || props[i] == touchscreen_legacy_prop)
            {
                is_touchscreen = TRUE;
            }
            else if (props[i] == synaptics_tap_prop)
            {
                synaptics_tap_to_click = xfce_device_x11_get_int_property (device, props[i], 4, NULL);
            }
            else if (props[i] == synaptics_edge_scroll_prop)
            {
                synaptics_edge_scroll = xfce_device_x11_get_int_property (device, props[i], 0, &synaptics_edge_hscroll);
            }
            else if (props[i] == synaptics_two_scroll_prop)
            {
                synaptics_two_scroll = xfce_device_x11_get_int_property (device, props[i], 0, &synaptics_two_hscroll);
            }
            else if (props[i] == synaptics_circ_scroll_prop)
            {
                synaptics_circ_scroll = xfce_device_x11_get_int_property (device, props[i], 0, NULL);
            }
            else if (props[i] == wacom_rotation_prop)
            {
                wacom_rotation = xfce_device_x11_get_int_property (device, props[i], 0, NULL);
            }
            else if (props[i] == libinput_dwt_prop)
            {
                is_synaptics = TRUE;
                xfce_device_x11_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_DISABLE_WHILE_TYPING, &libinput_dwt);
            }
            else if (props[i] == libinput_tap_prop)
            {
                is_synaptics = TRUE;
                xfce_device_x11_get_libinput_boolean (xdisplay, device, LIBINPUT_PROP_TAP, &synaptics_tap_to_click);
            }
            else if (props[i] == libinput_scroll_methods_prop)
            {
                propdata_t pdata[3] = { 0 };

                if (xfce_device_x11_get_device_prop (xdisplay, device, LIBINPUT_PROP_SCROLL_METHOD_ENABLED, XA_INTEGER, 3, &pdata[0]))
                {
                    synaptics_two_scroll = (gint) pdata[0].c;
                    synaptics_edge_scroll = (gint) pdata[1].c;
                    synaptics_circ_scroll = -1;
                }

                if (xfce_device_x11_get_device_prop (xdisplay, device, LIBINPUT_PROP_SCROLL_METHODS_AVAILABLE, XA_INTEGER, 3, &pdata[0]))
                {
                    if (!pdata[0].c)
                    {
                        synaptics_two_scroll = -1;
                    }
                    if (!pdata[1].c)
                    {
                        synaptics_edge_scroll = -1;
                    }
                }
            }
            else if (props[i] == libinput_click_method_prop)
            {
                if (xfce_device_x11_get_libinput_click_method (xdisplay, device, LIBINPUT_PROP_CLICK_METHODS_AVAILABLE, &click_methods_available))
                {
                    has_click_method = xfce_device_x11_get_libinput_click_method (xdisplay, device, LIBINPUT_PROP_CLICK_METHOD_ENABLED, &click_method_current);
                }
            }
        }
        XFree (props);
    }

    gdk_x11_display_error_trap_push (gdk_display);
    XCloseDevice (xdisplay, device);
    gdk_x11_display_error_trap_pop_ignored (gdk_display);

    self->is_libinput = is_libinput;
    self->is_synaptics = is_synaptics;
    self->is_wacom = is_wacom;
    self->threshold = threshold;
    self->has_hires_scrolling = has_hires_scrolling;
    self->hires_scrolling = hires_scrolling;
    self->wacom_mode = wacom_mode;
    self->synaptics_horizontal = (synaptics_edge_hscroll == 1 || synaptics_two_hscroll == 1);

    if (wacom_rotation != -1)
    {
        XfceDeviceRotation rotation = _xfce_device_tablet_rotation_from_wacom (wacom_rotation);
        _xfce_device_update_tablet_rotation (base, rotation, rotation);
    }

    XfceDeviceCapabilities capabilities = XFCE_DEVICE_CAPABILITIES_POINTER;
    if (is_touchscreen)
    {
        capabilities |= XFCE_DEVICE_CAPABILITIES_TOUCH;
    }
    if (is_wacom)
    {
        capabilities |= XFCE_DEVICE_CAPABILITIES_TABLET_TOOL;
    }
    _xfce_device_update_capabilities (base, capabilities);

    if (nbuttons > 0)
    {
        _xfce_device_update_left_handed (base, FALSE, left_handed);
    }

    if (nbuttons >= 5)
    {
        _xfce_device_update_natural_scroll (base, FALSE, reverse_scrolling);
    }

    if (acceleration != -1.0)
    {
        _xfce_device_update_accel_speed (base, acceleration, acceleration);
    }

    if (has_accel_profile)
    {
        // The written array's length must match the property's slot count, which
        // the libinput version fixes system-wide and is independent of whether
        // this device actually offers the custom profile.
        _xfce_device_update_accel_profile (base, accel_profile_available,
                                           accel_profile_current, accel_profile_current,
                                           libinput_supports_custom_accel_profile);
    }

    if (is_enabled != -1)
    {
        XfceDeviceSendEvents current = is_enabled > 0 ? XFCE_DEVICE_SEND_EVENTS_ENABLED : XFCE_DEVICE_SEND_EVENTS_DISABLED;
        _xfce_device_update_send_events_state (base,
                                               XFCE_DEVICE_SEND_EVENTS_ENABLED | XFCE_DEVICE_SEND_EVENTS_DISABLED,
                                               XFCE_DEVICE_SEND_EVENTS_ENABLED,
                                               current);
    }

    if (synaptics_tap_to_click != -1)
    {
        _xfce_device_update_tap (base, FALSE, synaptics_tap_to_click > 0);
    }

    {
        XfceDeviceScrollMethod supported = XFCE_DEVICE_SCROLL_METHOD_NO_SCROLL;
        if (synaptics_two_scroll != -1)
        {
            supported |= XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER;
        }
        if (synaptics_edge_scroll != -1)
        {
            supported |= XFCE_DEVICE_SCROLL_METHOD_EDGE;
        }
        if (synaptics_circ_scroll != -1)
        {
            supported |= XFCE_DEVICE_SCROLL_METHOD_CIRCULAR;
        }

        // A later match wins, so circular takes precedence over two-finger
        // over edge.
        XfceDeviceScrollMethod current = XFCE_DEVICE_SCROLL_METHOD_NO_SCROLL;
        if (synaptics_edge_scroll > 0)
        {
            current = XFCE_DEVICE_SCROLL_METHOD_EDGE;
        }
        if (synaptics_two_scroll > 0)
        {
            current = XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER;
        }
        if (synaptics_circ_scroll > 0)
        {
            current = XFCE_DEVICE_SCROLL_METHOD_CIRCULAR;
        }

        if (supported != XFCE_DEVICE_SCROLL_METHOD_NO_SCROLL)
        {
            _xfce_device_update_scroll_method (base, supported, current, current);
        }
    }

    if (has_click_method)
    {
        _xfce_device_update_click_method (base, click_methods_available,
                                          click_method_current, click_method_current);
    }

    if (libinput_dwt != -1)
    {
        _xfce_device_update_dwt (base, FALSE, libinput_dwt > 0);
    }
}

gboolean
xfce_device_x11_is_libinput (XfceDeviceX11 *device)
{
    return device->is_libinput;
}

gboolean
xfce_device_x11_get_threshold_available (XfceDeviceX11 *device)
{
    return device->threshold != -1;
}

gint
xfce_device_x11_get_threshold (XfceDeviceX11 *device)
{
    return device->threshold;
}

gboolean
xfce_device_x11_get_hires_scrolling_available (XfceDeviceX11 *device)
{
    // Hi-res scrolling is for mouse wheels, not touchpads
    return device->has_hires_scrolling && device->is_libinput && !device->is_synaptics;
}

gboolean
xfce_device_x11_get_hires_scrolling (XfceDeviceX11 *device)
{
    return device->hires_scrolling;
}

gint
xfce_device_x11_get_wacom_mode (XfceDeviceX11 *device)
{
    return device->wacom_mode;
}

gboolean
xfce_device_x11_get_synaptics_scroll_horizontal_available (XfceDeviceX11 *device)
{
    // The synaptics driver has a horizontal-scroll toggle; libinput scrolls
    // horizontally on its own.
    return !device->is_libinput && xfce_device_get_scroll_method_available (XFCE_DEVICE (device));
}

gboolean
xfce_device_x11_get_synaptics_scroll_horizontal (XfceDeviceX11 *device)
{
    return device->synaptics_horizontal;
}

// Rewrite the synaptics scroll arrays from the current method + horizontal
// state; both the shared scroll-method setter and the X11-only horizontal
// setter feed this.
static void
xfce_device_x11_write_synaptics_scroll (XfceDeviceX11 *device)
{
    XfconfChannel *channel = _xfce_device_get_channel (XFCE_DEVICE (device));
    gboolean horizontal = device->synaptics_horizontal;
    XfceDeviceScrollMethod method = xfce_device_get_scroll_method (XFCE_DEVICE (device));
    gint edge_scroll[3] = { 0, 0, 0 };
    gint two_scroll[2] = { 0, 0 };
    gint circ_scroll = 0;
    gint circ_trigger = 0;

    switch (method)
    {
        case XFCE_DEVICE_SCROLL_METHOD_EDGE:
            edge_scroll[0] = TRUE;
            edge_scroll[1] = horizontal;
            break;

        case XFCE_DEVICE_SCROLL_METHOD_TWO_FINGER:
            two_scroll[0] = TRUE;
            two_scroll[1] = horizontal;
            break;

        case XFCE_DEVICE_SCROLL_METHOD_CIRCULAR:
            circ_scroll = TRUE;
            if (horizontal)
            {
                circ_trigger = 3;
                edge_scroll[1] = TRUE;
            }
            break;

        default:
            break;
    }

    gchar *prop = _xfce_device_prop (XFCE_DEVICE (device), "/Properties/Synaptics_Edge_Scrolling");
    xfconf_channel_set_array (channel, prop,
                              G_TYPE_INT, &edge_scroll[0],
                              G_TYPE_INT, &edge_scroll[1],
                              G_TYPE_INT, &edge_scroll[2],
                              G_TYPE_INVALID);
    g_free (prop);

    prop = _xfce_device_prop (XFCE_DEVICE (device), "/Properties/Synaptics_Two-Finger_Scrolling");
    xfconf_channel_set_array (channel, prop,
                              G_TYPE_INT, &two_scroll[0],
                              G_TYPE_INT, &two_scroll[1],
                              G_TYPE_INVALID);
    g_free (prop);

    prop = _xfce_device_prop (XFCE_DEVICE (device), "/Properties/Synaptics_Circular_Scrolling");
    xfconf_channel_set_int (channel, prop, circ_scroll);
    g_free (prop);

    prop = _xfce_device_prop (XFCE_DEVICE (device), "/Properties/Synaptics_Circular_Scrolling_Trigger");
    xfconf_channel_set_int (channel, prop, circ_trigger);
    g_free (prop);
}

static void
xfce_device_x11_set_tap (XfceDevice *device,
                         gboolean tap)
{
    // The shared (libinput) key first.
    XFCE_DEVICE_CLASS (xfce_device_x11_parent_class)->set_tap (device, tap);

    // Then the synaptics driver's tap-action array, preserving the button
    // actions the dialog does not expose.
    GdkDisplay *gdk_display = gdk_display_get_default ();
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display);
    XDevice *xdevice;

    gdk_x11_display_error_trap_push (gdk_display);
    xdevice = XOpenDevice (xdisplay, XFCE_DEVICE_X11 (device)->xid);
    if (gdk_x11_display_error_trap_pop (gdk_display) != 0 || xdevice == NULL)
    {
        return;
    }

    Atom tap_action_prop = XInternAtom (xdisplay, "Synaptics Tap Action", True);
    Atom type;
    gint format;
    gulong n_items, bytes_after;
    guchar *data;
    gint res;

    gdk_x11_display_error_trap_push (gdk_display);
    res = XGetDeviceProperty (xdisplay, xdevice, tap_action_prop, 0, 1000, False,
                              AnyPropertyType, &type, &format,
                              &n_items, &bytes_after, &data);
    if (gdk_x11_display_error_trap_pop (gdk_display) == 0 && res == Success)
    {
        if (type == XA_INTEGER && format == 8 && n_items >= 7)
        {
            // Format: RT, RB, LT, LB, F1, F2, F3.
            data[4] = tap ? 1 : 0;
            data[5] = tap ? 3 : 0;
            data[6] = tap ? 2 : 0;

            GPtrArray *array = g_ptr_array_sized_new (n_items);
            for (gulong n = 0; n < n_items; n++)
            {
                GValue *val = g_new0 (GValue, 1);
                g_value_init (val, G_TYPE_INT);
                g_value_set_int (val, data[n]);
                g_ptr_array_add (array, val);
            }

            XfconfChannel *channel = _xfce_device_get_channel (device);
            gchar *prop = _xfce_device_prop (device, "/Properties/Synaptics_Tap_Action");
            xfconf_channel_set_arrayv (channel, prop, array);
            g_free (prop);

            xfconf_array_free (array);
        }

        XFree (data);
    }

    gdk_x11_display_error_trap_push (gdk_display);
    XCloseDevice (xdisplay, xdevice);
    gdk_x11_display_error_trap_pop_ignored (gdk_display);
}

static void
xfce_device_x11_set_scroll_method (XfceDevice *device,
                                   XfceDeviceScrollMethod method)
{
    // libinput has no circular scrolling; map it to "no scroll" for the shared
    // key and let the synaptics arrays carry it.
    XfceDeviceScrollMethod libinput_method = method;
    if (method == XFCE_DEVICE_SCROLL_METHOD_CIRCULAR)
    {
        libinput_method = XFCE_DEVICE_SCROLL_METHOD_NO_SCROLL;
    }
    XFCE_DEVICE_CLASS (xfce_device_x11_parent_class)->set_scroll_method (device, libinput_method);

    xfce_device_x11_write_synaptics_scroll (XFCE_DEVICE_X11 (device));
}

void
xfce_device_x11_set_threshold (XfceDeviceX11 *device,
                               gint threshold)
{
    device->threshold = threshold;

    XfconfChannel *channel = _xfce_device_get_channel (XFCE_DEVICE (device));
    gchar *prop = _xfce_device_prop (XFCE_DEVICE (device), "/Threshold");
    if (xfconf_channel_get_int (channel, prop, -1) != threshold)
    {
        xfconf_channel_set_int (channel, prop, threshold);
    }
    g_free (prop);
}

// Unlike the setters, this asks for the driver defaults back rather than for a
// known value, so there is nothing to record: the caller has to refresh to find
// out what the device settled on.
void
xfce_device_x11_reset_feedback (XfceDeviceX11 *device)
{
    XfconfChannel *channel = _xfce_device_get_channel (XFCE_DEVICE (device));

    gchar *prop = _xfce_device_prop (XFCE_DEVICE (device), "/Threshold");
    xfconf_channel_set_int (channel, prop, -1);
    g_free (prop);

    prop = _xfce_device_prop (XFCE_DEVICE (device), "/Acceleration");
    xfconf_channel_set_double (channel, prop, -1.0);
    g_free (prop);
}

void
xfce_device_x11_set_hires_scrolling (XfceDeviceX11 *device,
                                     gboolean enabled)
{
    device->hires_scrolling = enabled;

    XfconfChannel *channel = _xfce_device_get_channel (XFCE_DEVICE (device));
    gchar *prop = _xfce_device_libinput_prop (XFCE_DEVICE (device), LIBINPUT_PROP_HIRES_WHEEL_SCROLL_ENABLED);
    xfconf_channel_set_int (channel, prop, enabled);
    g_free (prop);
}

void
xfce_device_x11_set_wacom_mode (XfceDeviceX11 *device,
                                const gchar *mode)
{
    device->wacom_mode = g_strcmp0 (mode, "ABSOLUTE") == 0 ? 0 : 1;

    XfconfChannel *channel = _xfce_device_get_channel (XFCE_DEVICE (device));
    gchar *prop = _xfce_device_prop (XFCE_DEVICE (device), "/Mode");
    xfconf_channel_set_string (channel, prop, mode);
    g_free (prop);
}


void
xfce_device_x11_set_synaptics_scroll_horizontal (XfceDeviceX11 *device,
                                                 gboolean horizontal)
{
    device->synaptics_horizontal = horizontal;
    xfce_device_x11_write_synaptics_scroll (device);
}

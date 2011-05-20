/*
 *  Copyright (c) 2008-2011 Nick Schermer <nick@xfce.org>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#include "debug.h"
#include "pointers.h"



#define MAX_DENOMINATOR (100.00)

/* Xi 1.4 is required */
#define MIN_XI_VERS_MAJOR 1
#define MIN_XI_VERS_MINOR 4

/* test if the required version of inputproto (1.4.2) is available */
#if XI_Add_DevicePresenceNotify_Major >= 1 && defined (DeviceRemoved)
#define HAS_DEVICE_HOTPLUGGING
#else
#undef HAS_DEVICE_HOTPLUGGING
#endif
#ifndef IsXExtensionPointer
#define IsXExtensionPointer 4
#endif



static void             xfce_pointers_helper_restore_devices                (XfcePointersHelper      *helper,
                                                                             XID                     *xid);
static void             xfce_pointers_helper_channel_property_changed       (XfconfChannel           *channel,
                                                                             const gchar             *property_name,
                                                                             const GValue            *value);
#ifdef HAS_DEVICE_HOTPLUGGING
static GdkFilterReturn  xfce_pointers_helper_event_filter                   (GdkXEvent               *xevent,
                                                                             GdkEvent                *gdk_event,
                                                                             gpointer                 user_data);
#endif



struct _XfcePointersHelperClass
{
    GObjectClass __parent__;
};

struct _XfcePointersHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel *channel;

#ifdef HAS_DEVICE_HOTPLUGGING
    /* device presence event type */
    gint           device_presence_event_type;
#endif
};



G_DEFINE_TYPE (XfcePointersHelper, xfce_pointers_helper, G_TYPE_OBJECT);



static void
xfce_pointers_helper_class_init (XfcePointersHelperClass *klass)
{

}



static void
xfce_pointers_helper_init (XfcePointersHelper *helper)
{
    XExtensionVersion *version = NULL;
    Display           *xdisplay;
#ifdef HAS_DEVICE_HOTPLUGGING
    XEventClass        event_class;
#endif

    /* get the default display */
    xdisplay = gdk_x11_display_get_xdisplay (gdk_display_get_default ());

    /* query the extension version */
    version = XGetExtensionVersion (xdisplay, INAME);

    /* check for Xi */
    if (version == NULL || !version->present)
    {
        g_critical ("XI is not present.");
    }
    else if (version->major_version < MIN_XI_VERS_MAJOR
             || (version->major_version == MIN_XI_VERS_MAJOR
                 && version->minor_version < MIN_XI_VERS_MINOR))
    {
        g_critical ("Your XI is too old (%d.%d) version %d.%d is required.",
                    version->major_version, version->minor_version,
                    MIN_XI_VERS_MAJOR, MIN_XI_VERS_MINOR);
    }
    else
    {
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "initialized xi %d.%d",
                        version->major_version, version->minor_version);

        /* open the channel */
        helper->channel = xfconf_channel_get ("pointers");

        /* restore the pointer devices */
        xfce_pointers_helper_restore_devices (helper, NULL);

        /* monitor the channel */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed",
             G_CALLBACK (xfce_pointers_helper_channel_property_changed), NULL);

#ifdef HAS_DEVICE_HOTPLUGGING
        if (G_LIKELY (xdisplay != NULL))
        {
            /* monitor device changes */
            gdk_error_trap_push ();
            DevicePresence (xdisplay, helper->device_presence_event_type, event_class);
            XSelectExtensionEvent (xdisplay, RootWindow (xdisplay, DefaultScreen (xdisplay)), &event_class, 1);

            /* add an event filter */
            if (gdk_error_trap_pop () == 0)
                gdk_window_add_filter (NULL, xfce_pointers_helper_event_filter, helper);
            else
                g_warning ("Failed to create device filter");
        }
#endif
    }
}



static gboolean
xfce_pointers_helper_change_button_mapping_swap (guchar   *buttonmap,
                                                 gshort    num_buttons,
                                                 gint      id_1,
                                                 gint      id_2,
                                                 gboolean  reverse)
{
    gshort n;
    gint   id_a = -1;
    gint   id_b = -1;

    /* figure out the position of the id_1 and id_2 buttons in the map */
    for (n = 0; n < num_buttons; n++)
    {
        if (buttonmap[n] == id_1)
            id_a = n;
        else if (buttonmap[n] == id_2)
            id_b = n;
    }

    /* only change the map when id_a and id_b where found */
    if (G_LIKELY (id_a != -1 && id_b != -1))
    {
        /* check if we need to change the buttonmap */
        if ((!reverse && (id_a < id_b))
            || (reverse && (id_a > id_b)))
        {
            /* swap the buttons in the button map */
            buttonmap[id_a] = id_2;
            buttonmap[id_b] = id_1;

            return TRUE;
        }
    }

    return FALSE;
}



static void
xfce_pointers_helper_change_button_mapping (XDeviceInfo *device_info,
                                            XDevice     *device,
                                            Display     *xdisplay,
                                            gint         right_handed,
                                            gint         reverse_scrolling)
{
    XAnyClassPtr  ptr;
    gshort        num_buttons = 0;
    guchar       *buttonmap;
    gboolean      map_changed = FALSE;
    gint          n;
    gint          right_button;
    GString      *readable_map;

    /* search the number of buttons */
    for (n = 0, ptr = device_info->inputclassinfo; n < device_info->num_classes; n++)
    {
        if (ptr->class == ButtonClass)
        {
            num_buttons = ((XButtonInfoPtr) ptr)->num_buttons;
            break;
        }

        /* advance the offset */
        ptr = (XAnyClassPtr) ((gchar *) ptr + ptr->length);
    }

    if (num_buttons == 0)
    {
        g_critical ("Device %s has no buttons", device_info->name);
        return;
    }

    /* allocate the button map */
    buttonmap = g_new0 (guchar, num_buttons);

    gdk_error_trap_push ();
    XGetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);
    if (gdk_error_trap_pop () != 0)
    {
        g_warning ("Failed to get button mapping");
        goto leave;
    }

    /* -1 means we don't change this in the mapping */
    if (right_handed != -1)
    {
        /* get the right button number */
        right_button = MIN (num_buttons, 3);

        /* check the buttons and swap them if needed */
        if (xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons,
                                                             1 /* left button */,
                                                             right_button,
                                                             right_handed))
            map_changed = TRUE;
    }

    /* -1 means we don't change this in the mapping */
    if (reverse_scrolling != -1 && num_buttons >= 5)
    {
        /* check the buttons and swap them if needed */
        if (xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons,
                                                             4 /* scroll up */,
                                                             5 /* scroll down */,
                                                             !reverse_scrolling))
            map_changed = TRUE;
    }

    /* only set on changes */
    if (map_changed)
    {
        gdk_error_trap_push ();
        XSetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);
        if (gdk_error_trap_pop () != 0)
            g_warning ("Failed to set button mapping");

        /* don't put a hard time on ourselves and make debugging a lot better */
        readable_map = g_string_sized_new (num_buttons);
        for (n = 0; n < num_buttons; n++)
            g_string_append_printf (readable_map, "%d ", buttonmap[n]);
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "[%s] new buttonmap is [%s]",
                        device_info->name, readable_map->str);
        g_string_free (readable_map, TRUE);
    }
    else
    {
        xfsettings_dbg (XFSD_DEBUG_POINTERS, "[%s] new buttonmap not changed",
                        device_info->name);
    }

    leave:

    g_free (buttonmap);
}



static gint
xfce_pointers_helper_gcd (gint num,
                          gint denom)
{
    /* calc the greatest common divisor using euclidean's algorithm */
    return (denom != 0 ? xfce_pointers_helper_gcd (denom, num % denom) : num);
}



static void
xfce_pointers_helper_change_feedback (XDeviceInfo *device_info,
                                      XDevice     *device,
                                      Display     *xdisplay,
                                      gint         threshold,
                                      gdouble      acceleration)
{
    XFeedbackState      *states, *pt;
    gint                 num_feedbacks;
    XPtrFeedbackControl  feedback;
    gint                 n;
    gulong               mask = 0;
    gint                 num, denom, gcd;
    gboolean             found = FALSE;

    /* get the feedback states for this device */
    gdk_error_trap_push ();
    states = XGetFeedbackControl (xdisplay, device, &num_feedbacks);
    if (gdk_error_trap_pop() != 0 || states == NULL)
    {
        g_critical ("Failed to get the feedback states of device %s",
                    device_info->name);
        return;
    }

    for (pt = states, n = 0; n < num_feedbacks; n++)
    {
        /* find the pointer feedback class */
        if (pt->class != PtrFeedbackClass)
        {
            /* advance the offset */
            pt = (XFeedbackState *) ((gchar *) pt + pt->length);
            continue;
        }

        /* initialize the feedback, -1 for reset if the
         * mask matches in XChangeFeedbackControl */
        feedback.class = PtrFeedbackClass;
        feedback.length = sizeof (XPtrFeedbackControl);
        feedback.id = pt->id;
        feedback.threshold = -1;
        feedback.accelNum = -1;
        feedback.accelDenom = -1;

        found = TRUE;

        /* above 0 is a valid value, -1 is reset, -2.00
         * is passed if no change is required */
        if (acceleration > 0 || acceleration == -1)
        {
            if (acceleration > 0)
            {
                /* calculate the faction of the acceleration */
                num = acceleration * MAX_DENOMINATOR;
                denom = MAX_DENOMINATOR;
                gcd = xfce_pointers_helper_gcd (num, denom);
                num /= gcd;
                denom /= gcd;

                feedback.accelNum = num;
                feedback.accelDenom = denom;
            }

            /* include acceleration in the mask */
            mask |= DvAccelNum | DvAccelDenom;
        }

        /* above 0 is a valid value, -1 is reset, -2 is
         * passed if no change is required */
        if (threshold > 0 || threshold == -1)
        {
            if (threshold > 0)
                feedback.threshold = threshold;

            mask |= DvThreshold;
        }

        /* update the feedback of the device */
        gdk_error_trap_push ();
        XChangeFeedbackControl (xdisplay, device, mask,
                                (XFeedbackControl *) &feedback);
        if (gdk_error_trap_pop() != 0)
        {
            g_warning ("Failed to set feedback states for device %s",
                       device_info->name);
        }

        xfsettings_dbg (XFSD_DEBUG_POINTERS,
                        "[%s] change feedback (threshold=%d, "
                        "accelNum=%d, accelDenom=%d)",
                        device_info->name, feedback.threshold,
                        feedback.accelNum, feedback.accelDenom);

        break;
    }

    if (!found)
    {
        g_critical ("Unable to find PtrFeedbackClass for %s",
                    device_info->name);
    }

    XFreeFeedbackList (states);
}



static gchar *
xfce_pointers_helper_device_xfconf_name (const gchar *name)
{
    GString     *string;
    const gchar *p;

    /* NOTE: this function exists in both the dialog and
     *       helper code and they have to identical! */

    /* allocate a string */
    string = g_string_sized_new (strlen (name));

    /* create a name with only valid chars */
    for (p = name; *p != '\0'; p++)
    {
        if ((*p >= 'A' && *p <= 'Z')
            || (*p >= 'a' && *p <= 'z')
            || (*p >= '0' && *p <= '9')
            || *p == '_' || *p == '-')
        {
            g_string_append_c (string, *p);
        }
        else if (*p == ' ')
        {
            string = g_string_append_c (string, '_');
        }
    }

    /* return the new string */
    return g_string_free (string, FALSE);
}



static void
xfce_pointers_helper_restore_devices (XfcePointersHelper *helper,
                                      XID                *xid)
{
    Display     *xdisplay = GDK_DISPLAY ();
    XDeviceInfo *device_list, *device_info;
    gint         n, ndevices;
    XDevice     *device;
    gchar       *device_name;
    gchar        prop[256];
    gboolean     right_handed;
    gboolean     reverse_scrolling;
    gint         threshold;
    gdouble      acceleration;

    gdk_error_trap_push ();
    device_list = XListInputDevices (xdisplay, &ndevices);
    if (gdk_error_trap_pop () != 0 || device_list == NULL)
    {
        g_message ("No input devices found");
        return;
    }

    for (n = 0; n < ndevices; n++)
    {
        /* filter the pointer devices */
        device_info = &device_list[n];
        if (device_info->use != IsXExtensionPointer
            || device_info->name == NULL)
            continue;

        /* filter out the device if one is set */
        if (xid != NULL && device_info->id != *xid)
            continue;

        /* open the device */
        gdk_error_trap_push ();
        device = XOpenDevice (xdisplay, device_info->id);
        if (gdk_error_trap_pop () != 0 || device == NULL)
        {
            g_critical ("Unable to open device %s", device_info->name);
            continue;
        }

        /* create a valid xfconf property name for the device */
        device_name = xfce_pointers_helper_device_xfconf_name (device_info->name);

        /* read buttonmap properties */
        g_snprintf (prop, sizeof (prop), "/%s/RightHanded", device_name);
        right_handed = xfconf_channel_get_bool (helper->channel, prop, -1);

        g_snprintf (prop, sizeof (prop), "/%s/ReverseScrolling", device_name);
        reverse_scrolling = xfconf_channel_get_bool (helper->channel, prop, -1);

        if (right_handed != -1 || reverse_scrolling != -1)
        {
            xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                        right_handed, reverse_scrolling);
        }

        /* read feedback settings */
        g_snprintf (prop, sizeof (prop), "/%s/Threshold", device_name);
        threshold = xfconf_channel_get_int (helper->channel, prop, -1);

        g_snprintf (prop, sizeof (prop), "/%s/Acceleration", device_name);
        acceleration = xfconf_channel_get_double (helper->channel, prop, -1.00);

        if (threshold != -1 || acceleration != -1.00)
        {
            xfce_pointers_helper_change_feedback (device_info, device, xdisplay,
                                                  threshold, acceleration);
        }

        g_free (device_name);
        XCloseDevice (xdisplay, device);
    }

    XFreeDeviceList (device_list);
}



static void
xfce_pointers_helper_channel_property_changed (XfconfChannel *channel,
                                               const gchar   *property_name,
                                               const GValue  *value)
{
    Display      *xdisplay = GDK_DISPLAY ();
    XDeviceInfo  *device_list, *device_info;
    XDevice      *device;
    gint          n, ndevices;
    gchar       **names;
    gchar        *device_name;

    if (G_UNLIKELY (property_name == NULL))
         return;

    /* split the property name (+1 so skip the first slash in the name) */
    names = g_strsplit (property_name + 1, "/", -1);

    if (names != NULL && g_strv_length (names) >= 2)
    {
        gdk_error_trap_push ();
        device_list = XListInputDevices (xdisplay, &ndevices);
        if (gdk_error_trap_pop () != 0 || device_list == NULL)
        {
            g_message ("No input devices found");
            return;
        }

        for (n = 0; n < ndevices; n++)
        {
            /* filter the pointer devices */
            device_info = &device_list[n];
            if (device_info->use != IsXExtensionPointer
                || device_info->name == NULL)
                continue;

            /* search the device name */
            device_name = xfce_pointers_helper_device_xfconf_name (device_info->name);
            if (strcmp (names[0], device_name) == 0)
            {
                /* open the device */
                gdk_error_trap_push ();
                device = XOpenDevice (xdisplay, device_info->id);
                if (gdk_error_trap_pop () != 0 || device == NULL)
                {
                    g_critical ("Unable to open device %s", device_info->name);
                    continue;
                }

                /* check the property that requires updating */
                if (strcmp (names[1], "RightHanded") == 0)
                {
                    xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                                g_value_get_boolean (value), -1);
                }
                else if (strcmp (names[1], "ReverseScrolling") == 0)
                {
                    xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                                -1, g_value_get_boolean (value));
                }
                else if (strcmp (names[1], "Threshold") == 0)
                {
                    xfce_pointers_helper_change_feedback (device_info, device, xdisplay,
                                                          g_value_get_int (value), -2.00);
                }
                else if (strcmp (names[1], "Acceleration") == 0)
                {
                    xfce_pointers_helper_change_feedback (device_info, device, xdisplay,
                                                          -2, g_value_get_double (value));
                }
                /*else if (strcmp (names[1], "Properties") == 0)
                {
                    xfce_pointers_helper_change_properties (device_info, device, xdisplay);
                }*/
                else
                {
                    g_warning ("Unknown property %s set for device %s",
                               property_name, device_info->name);
                }

                XCloseDevice (xdisplay, device);

                /* stop searching */
                n = ndevices;
            }

            g_free (device_name);
        }

        XFreeDeviceList (device_list);
    }

    g_strfreev (names);
}



#ifdef HAS_DEVICE_HOTPLUGGING
static GdkFilterReturn
xfce_pointers_helper_event_filter (GdkXEvent *xevent,
                                   GdkEvent  *gdk_event,
                                   gpointer   user_data)
{
    XEvent                     *event = xevent;
    XDevicePresenceNotifyEvent *dpn_event = xevent;
    XfcePointersHelper         *helper = XFCE_POINTERS_HELPER (user_data);

    /* update on device changes */
    if (event->type == helper->device_presence_event_type
        && dpn_event->devchange == DeviceAdded)
    {
        xfce_pointers_helper_restore_devices (helper, &dpn_event->deviceid);
    }

    return GDK_FILTER_CONTINUE;
}
#endif

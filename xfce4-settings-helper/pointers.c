/* $Id$ */
/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
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



static void             xfce_pointers_helper_change_button_mapping_swap     (guchar                  *buttonmap,
                                                                             gshort                   num_buttons,
                                                                             gint                     id_1,
                                                                             gint                     id_2,
                                                                             gboolean                 reverse);
static void             xfce_pointers_helper_change_button_mapping          (XDeviceInfo             *device_info,
                                                                             XDevice                 *device,
                                                                             Display                 *xdisplay,
                                                                             gint                     right_handed,
                                                                             gint                     reverse_scrolling);
static gint             xfce_pointers_helper_gcd                            (gint                     num,
                                                                             gint                     denom);
static void             xfce_pointers_helper_change_feedback                (XDevice                 *device,
                                                                             Display                 *xdisplay,
                                                                             gint                     threshold,
                                                                             gdouble                  acceleration);
static gchar           *xfce_pointers_helper_device_xfconf_name             (const gchar             *name);
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
        /* open the channel */
        helper->channel = xfconf_channel_get ("pointers");

        /* restore the pointer devices */
        xfce_pointers_helper_restore_devices (helper, NULL);

        /* monitor the channel */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_pointers_helper_channel_property_changed), NULL);

#ifdef HAS_DEVICE_HOTPLUGGING
        /* flush x and trap errors */
        gdk_flush ();
        gdk_error_trap_push ();


        if (G_LIKELY (xdisplay))
        {
            /* monitor device changes */
            DevicePresence (xdisplay, helper->device_presence_event_type, event_class);
            XSelectExtensionEvent (xdisplay, RootWindow (xdisplay, DefaultScreen (xdisplay)), &event_class, 1);

            /* add an event filter */
            gdk_window_add_filter (NULL, xfce_pointers_helper_event_filter, helper);
        }

        /* flush and remove the x error trap */
        gdk_flush ();
        gdk_error_trap_pop ();
#endif
    }
}



static void
xfce_pointers_helper_change_button_mapping_swap (guchar   *buttonmap,
                                                 gshort    num_buttons,
                                                 gint      id_1,
                                                 gint      id_2,
                                                 gboolean  reverse)
{
    gint n;
    gint id_a;
    gint id_b;

    /* figure out the position of the id_1 and id_2 buttons in the map */
    for (n = 0, id_a = id_b = -1; n < num_buttons; n++)
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
        if ((!reverse && (id_a < id_b)) || (reverse && (id_a > id_b)))
        {
            /* swap the buttons in the button map */
            buttonmap[id_a] = id_2;
            buttonmap[id_b] = id_1;
        }
    }
}



static void
xfce_pointers_helper_change_button_mapping (XDeviceInfo *device_info,
                                            XDevice     *device,
                                            Display     *xdisplay,
                                            gint         right_handed,
                                            gint         reverse_scrolling)
{
    XAnyClassPtr  ptr;
    gshort        num_buttons;
    guchar       *buttonmap;
    gint          n;
    gint          right_button;

    /* get the device classes */
    ptr = device_info->inputclassinfo;

    /* search the classes for the number of buttons */
    for (n = 0, num_buttons = 0; n < device_info->num_classes; n++)
    {
        /* find the button class */
        if (ptr->class == ButtonClass)
        {
            /* get the number of buttons */
            num_buttons = ((XButtonInfoPtr) ptr)->num_buttons;

            /* done */
            break;
        }

        /* advance the offset */
        ptr = (XAnyClassPtr) ((gchar *) ptr + ptr->length);
    }

    if (G_LIKELY (num_buttons > 0))
    {
        /* allocate the button map */
        buttonmap = g_new0 (guchar, num_buttons);

        /* get the button mapping */
        gdk_error_trap_push ();
        XGetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);
        if (gdk_error_trap_pop ())
        {
            g_warning ("Failed to get button mapping");
            goto out;
        }

        if (right_handed != -1)
        {
            /* get the right button number */
            right_button = MIN (num_buttons, 3);

            /* check the buttons and swap them if needed */
            xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons, 1, right_button, right_handed);
        }

        if (reverse_scrolling != -1 && num_buttons >= 5)
        {
            /* check the buttons and swap them if needed */
            xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons, 4, 5, !reverse_scrolling);
        }

        /* set the new button mapping */
        gdk_error_trap_push ();
        XSetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);
        if (gdk_error_trap_pop ())
          g_warning ("Failed to set button mapping");

        /* cleanup */
out:    g_free (buttonmap);
    }
}



static gint
xfce_pointers_helper_gcd (gint num,
                          gint denom)
{
    /* calc the greatest common divisor using euclidean's algorithm */
    return (denom != 0 ? xfce_pointers_helper_gcd (denom, num % denom) : num);
}



static void
xfce_pointers_helper_change_feedback (XDevice *device,
                                      Display *xdisplay,
                                      gint     threshold,
                                      gdouble  acceleration)
{
    XFeedbackState      *states;
    gint                 num_feedbacks;
    XPtrFeedbackControl  feedback;
    gint                 n;
    gulong               mask = 0;
    gint                 num = -1, denom = -1, gcd;

    /* get the feedback states for this device */
    states = XGetFeedbackControl (xdisplay, device, &num_feedbacks);

    if (G_LIKELY (states))
    {
        /* get the pointer feedback class */
        for (n = 0; n < num_feedbacks; n++)
        {
            /* find the pointer feedback class */
            if (states->class == PtrFeedbackClass)
            {
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
                    }

                    /* set the mask */
                    mask |= DvAccelNum | DvAccelDenom;
                }

                /* setup the mask for the threshold */
                if (threshold > 0 || threshold == -1)
                    mask |= DvThreshold;

                /* create a new feedback */
                feedback.class      = PtrFeedbackClass;
                feedback.length     = sizeof (XPtrFeedbackControl);
                feedback.id         = states->id;
                feedback.threshold  = threshold;
                feedback.accelNum   = num;
                feedback.accelDenom = denom;

                /* change feedback for this device */
                XChangeFeedbackControl (xdisplay, device, mask, (XFeedbackControl *)(void *)&feedback);

                /* done */
                break;
            }

            /* advance the offset */
            states = (XFeedbackState *) ((gchar *) states + states->length);
        }

        /* cleanup */
        XFreeFeedbackList (states);
    }
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
          g_string_append_c (string, *p);
        else if (*p == ' ')
            string = g_string_append_c (string, '_');
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
    XDevice     *device;
    gint         n, ndevices;
    gchar       *righthanded_str;
    gchar       *threshold_str;
    gchar       *acceleration_str;
    gchar       *device_name;
    gchar       *reverse_scrolling_str;

    /* flush x and trap errors */
    gdk_flush ();
    gdk_error_trap_push ();

    /* get all the registered devices */
    device_list = XListInputDevices (xdisplay, &ndevices);

    for (n = 0; n < ndevices; n++)
    {
        /* get the device info */
        device_info = &device_list[n];

        /* filter out the pointer devices */
        if (device_info->use == IsXExtensionPointer)
        {
            /* filter devices */
            if (xid && device_info->id != *xid)
                continue;

            /* open the device */
            device = XOpenDevice (xdisplay, device_info->id);
            if (G_LIKELY (device))
            {
                /* get a clean device name */
                device_name = xfce_pointers_helper_device_xfconf_name (device_info->name);

                /* create righthanded property string */
                righthanded_str = g_strdup_printf ("/%s/RightHanded", device_name);

                /* check if we have a property for this device, else continue */
                if (xfconf_channel_has_property (helper->channel, righthanded_str))
                {
                    /* create property names */
                    reverse_scrolling_str = g_strdup_printf ("/%s/ReverseScrolling", device_name);
                    threshold_str = g_strdup_printf ("/%s/Threshold", device_name);
                    acceleration_str = g_strdup_printf ("/%s/Acceleration", device_name);

                    /* restore the button mapping */
                    xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay,
                                                                xfconf_channel_get_bool (helper->channel, righthanded_str, TRUE) ? 1 : 0,
                                                                xfconf_channel_get_bool (helper->channel, reverse_scrolling_str, FALSE) ? 1 : 0);

                    /* restore the pointer feedback */
                    xfce_pointers_helper_change_feedback (device, xdisplay,
                                                          xfconf_channel_get_int (helper->channel, threshold_str, -1),
                                                          xfconf_channel_get_double (helper->channel, acceleration_str, -1.00));

                    /* cleanup */
                    g_free (reverse_scrolling_str);
                    g_free (threshold_str);
                    g_free (acceleration_str);
                }

                /* cleanup */
                g_free (righthanded_str);
                g_free (device_name);

                /* close the device */
                XCloseDevice (xdisplay, device);
            }
        }
    }

    /* cleanup */
    XFreeDeviceList (device_list);

    /* flush and remove the x error trap */
    gdk_flush ();
    gdk_error_trap_pop ();
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

    /* flush x and trap errors */
    gdk_flush ();
    gdk_error_trap_push ();

    /* split the property name (+1 so skip the first slash in the name) */
    names = g_strsplit (property_name + 1, "/", -1);

    /* check if splitting worked */
    if (names && g_strv_length (names) == 2)
    {
        /* get all the registered devices */
        device_list = XListInputDevices (xdisplay, &ndevices);

        for (n = 0; n < ndevices; n++)
        {
            /* get the device info */
            device_info = &device_list[n];

            /* find the pointer device */
            if (device_info->use == IsXExtensionPointer)
            {
                /* create a valid xfconf device name */
                device_name = xfce_pointers_helper_device_xfconf_name (device_info->name);

                /* check if this is the device that's been changed */
                if (strcmp (names[0], device_name) == 0)
                {
                    /* open the device */
                    device = XOpenDevice (xdisplay, device_info->id);
                    if (G_LIKELY (device))
                    {
                        /* update the right property */
                        if (strcmp (names[1], "RightHanded") == 0)
                            xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay, !!g_value_get_boolean (value), -1);
                        else if (strcmp (names[1], "ReverseScrolling") == 0)
                            xfce_pointers_helper_change_button_mapping (device_info, device, xdisplay, -1, !!g_value_get_boolean (value));
                        else if (strcmp (names[1], "Threshold") == 0)
                            xfce_pointers_helper_change_feedback (device, xdisplay, g_value_get_int (value), -2.00);
                        else if (strcmp (names[1], "Acceleration") == 0)
                            xfce_pointers_helper_change_feedback (device, xdisplay, -2, g_value_get_double (value));

                        /* close the device */
                        XCloseDevice (xdisplay, device);
                    }

                    /* stop searching */
                    n = ndevices;
                }

                /* cleanup */
                g_free (device_name);
            }
        }

        /* cleanup */
        XFreeDeviceList (device_list);
    }

    /* cleanup */
    g_strfreev (names);

    /* flush and remove the x error trap */
    gdk_flush ();
    gdk_error_trap_pop ();
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
        xfce_pointers_helper_restore_devices (helper, &dpn_event->deviceid);

    return GDK_FILTER_CONTINUE;
}
#endif

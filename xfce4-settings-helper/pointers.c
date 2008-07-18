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
#include <X11/extensions/XInput.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_HAL
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <hal/libhal.h>
#if HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif /* !HAVE_LIBNOTIFY */
#endif /* !HAVE_HAL */

#include "pointers.h"



#define MAX_DENOMINATOR (100.00)



static void      xfce_pointers_helper_class_init                     (XfcePointersHelperClass *klass);
static void      xfce_pointers_helper_init                           (XfcePointersHelper      *helper);
static void      xfce_pointers_helper_finalize                       (GObject                 *object);
static void      xfce_pointers_helper_change_button_mapping_swap     (guchar                  *buttonmap,
                                                                      gshort                   num_buttons,
                                                                      gint                     id_1,
                                                                      gint                     id_2,
                                                                      gboolean                 reverse);
static void      xfce_pointers_helper_change_button_mapping          (XDeviceInfo             *device_info,
                                                                      XDevice                 *device,
                                                                      Display                 *xdisplay,
                                                                      gint                     right_handed,
                                                                      gint                     reverse_scrolling);
static gint      xfce_pointers_helper_gcd                            (gint                     num,
                                                                      gint                     denom);
static void      xfce_pointers_helper_change_feedback                (XDevice                 *device,
                                                                      Display                 *xdisplay,
                                                                      gint                     threshold,
                                                                      gdouble                  acceleration);
static gchar    *xfce_pointers_helper_device_xfconf_name             (const gchar             *name);
static void      xfce_pointers_helper_restore_devices                (XfcePointersHelper      *helper);
static void      xfce_pointers_helper_channel_property_changed       (XfconfChannel           *channel,
                                                                      const gchar             *property_name,
                                                                      const GValue            *value);
#ifdef HAVE_HAL
#if HAVE_LIBNOTIFY
static void      xfce_pointers_helper_notification_closed            (NotifyNotification      *notification,
                                                                      XfcePointersHelper      *helper);
static void      xfce_pointers_helper_notification_clicked           (NotifyNotification      *notification,
                                                                      gchar                   *action,
                                                                      gpointer                 user_data);
#endif /* !HAVE_LIBNOTIFY */
static gboolean  xfce_pointers_helper_device_added_timeout           (gpointer                 user_data);
static void      xfce_pointers_helper_device_added_timeout_destroyed (gpointer                 user_data);
static void      xfce_pointers_helper_device_added                   (LibHalContext           *context,
                                                                      const gchar             *udi);
#endif /* !HAVE_HAL */



struct _XfcePointersHelperClass
{
    GObjectClass __parent__;
};

struct _XfcePointersHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel      *channel;

#ifdef HAVE_HAL
    /* timeout for adding hal devices */
    guint               timeout_id;

    /* dbus connection */
    DBusConnection     *connection;

    /* hal context */
    LibHalContext      *context;

    /* last plugged device name */
    gchar              *last_device;

#ifdef HAVE_LIBNOTIFY
    NotifyNotification *notification;
#endif /* !HAVE_LIBNOTIFY */
#endif /* !HAVE_HAL */
};



G_DEFINE_TYPE (XfcePointersHelper, xfce_pointers_helper, G_TYPE_OBJECT);



static void
xfce_pointers_helper_class_init (XfcePointersHelperClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_pointers_helper_finalize;
}



static void
xfce_pointers_helper_init (XfcePointersHelper *helper)
{
    gint dummy;

#ifdef HAVE_HAL
    DBusError derror;

    /* initialize */
    helper->timeout_id = 0;
    helper->context = NULL;
    helper->connection = NULL;
    helper->last_device = NULL;
#ifdef HAVE_LIBNOTIFY
    helper->notification = NULL;
#endif /* !HAVE_LIBNOTIFY */
#endif /* !HAVE_HAL */

    if (XQueryExtension (GDK_DISPLAY (), "XInputExtension", &dummy, &dummy, &dummy))
    {
        /* open the channel */
        helper->channel = xfconf_channel_new ("pointers");

        /* restore the pointer devices */
        xfce_pointers_helper_restore_devices (helper);

        /* monitor the channel */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_pointers_helper_channel_property_changed), NULL);

#ifdef HAVE_HAL
#ifdef HAVE_LIBNOTIFY
        /* setup a connection with the notification daemon */
        if (!notify_init ("xfce4-settings-helper"))
            g_critical ("Failed to connect to the notification daemon.");
#endif /* !HAVE_LIBNOTIFY */
        /* initialize the dbus error variable */
        dbus_error_init (&derror);

        /* connect to the dbus system bus */
        helper->connection = dbus_bus_get (DBUS_BUS_SYSTEM, &derror);
        if (G_LIKELY (helper->connection))
        {
            /* connect dbus to the main loop */
            dbus_connection_setup_with_g_main (helper->connection, NULL);

            /* create hal context */
            helper->context = libhal_ctx_new ();
            if (G_LIKELY (helper->context))
            {
                /* set user data for the callbacks */
                libhal_ctx_set_user_data (helper->context, helper);

                /* set the dbus connection */
                if (G_LIKELY (libhal_ctx_set_dbus_connection (helper->context, helper->connection)))
                {
                    /* connect to hal */
                    if (G_LIKELY (libhal_ctx_init (helper->context, &derror)))
                    {
                        /* add callbacks for device changes */
                        libhal_ctx_set_device_added (helper->context, xfce_pointers_helper_device_added);
                    }
                    else
                    {
                       /* print warning */
                       g_warning ("Failed to connect to the hal daemon: %s.", derror.message);

                       /* cleanup */
                       LIBHAL_FREE_DBUS_ERROR (&derror);
                    }
                }
            }
        }
        else
        {
            /* print warning */
            g_warning ("Failed to connect to DBus: %s.", derror.message);

            /* cleanup */
            LIBHAL_FREE_DBUS_ERROR (&derror);
        }
#endif /* !HAVE_HAL */
    }
    else
    {
        /* print error */
        g_critical ("Failed to query the XInput extension.");

        /* no channel */
        helper->channel = NULL;
    }
}



static void
xfce_pointers_helper_finalize (GObject *object)
{
    XfcePointersHelper *helper = XFCE_POINTERS_HELPER (object);

#ifdef HAVE_HAL
    if (G_LIKELY (helper->context))
    {
        /* shutdown and free context */
        libhal_ctx_shutdown (helper->context, NULL);
        libhal_ctx_free (helper->context);
    }

    /* release the dbus connection */
    if (G_LIKELY (helper->connection))
        dbus_connection_unref (helper->connection);

    /* cleanup last device name */
    g_free (helper->last_device);

#ifdef HAVE_LIBNOTIFY
    /* close an opened notification */
    if (G_UNLIKELY (helper->notification))
        notify_notification_close (helper->notification, NULL);
#endif /* !HAVE_LIBNOTIFY */
#endif /* !HAVE_HAL */

    /* release the channel */
    if (G_LIKELY (helper->channel))
        g_object_unref (G_OBJECT (helper->channel));

    (*G_OBJECT_CLASS (xfce_pointers_helper_parent_class)->finalize) (object);
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
        XGetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);

        if (right_handed != -1)
        {
            /* get the right button number */
            right_button = num_buttons < 3 ? 2 : 3;

            /* check the buttons and swap them if needed */
            xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons, 1, right_button, !!right_handed);
        }

        if (reverse_scrolling != -1 && num_buttons >= 5)
        {
            /* check the buttons and swap them if needed */
            xfce_pointers_helper_change_button_mapping_swap (buttonmap, num_buttons, 4, 5, !reverse_scrolling);
        }

        /* set the new button mapping */
        XSetDeviceButtonMapping (xdisplay, device, buttonmap, num_buttons);

        /* cleanup */
        g_free (buttonmap);
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
                XChangeFeedbackControl (xdisplay, device, mask, (XFeedbackControl *) &feedback);

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
xfce_pointers_helper_restore_devices (XfcePointersHelper *helper)
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

    /* get all the registered devices */
    device_list = XListInputDevices (xdisplay, &ndevices);

    for (n = 0; n < ndevices; n++)
    {
        /* get the device info */
        device_info = &device_list[n];

        /* filter out the pointer devices */
        if (device_info->use == IsXExtensionPointer)
        {
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
}



#ifdef HAVE_HAL
#if HAVE_LIBNOTIFY
static void
xfce_pointers_helper_notification_closed (NotifyNotification *notification,
                                          XfcePointersHelper *helper)
{
    g_return_if_fail (helper->notification == notification);

    /* set to null */
    helper->notification = NULL;
}



static void
xfce_pointers_helper_notification_clicked (NotifyNotification *notification,
                                           gchar              *action,
                                           gpointer            user_data)
{
    XfcePointersHelper *helper = XFCE_POINTERS_HELPER (user_data);
    GError             *error = NULL;
    gchar              *command;
    const gchar        *path = BINDIR G_DIR_SEPARATOR_S "xfce4-mouse-settings";

    /* build a command */
    if (G_LIKELY (helper->last_device))
        command = g_strdup_printf ("%s -d '%s'", path, helper->last_device);
    else
        command = g_strdup (path);

    /* try to spwn the xfce4-mouse-setting dialog */
    if (!g_spawn_command_line_async (command, &error))
    {
        g_critical ("Failed to spawn the mouse settings dialog: %s", error->message);
        g_error_free (error);
    }

    /* cleanup */
    g_free (command);
}
#endif /* !HAVE_LIBNOTIFY */



static gboolean
xfce_pointers_helper_device_added_timeout (gpointer user_data)
{
    XfcePointersHelper *helper = XFCE_POINTERS_HELPER (user_data);

    GDK_THREADS_ENTER ();

    /* restore the devices */
    xfce_pointers_helper_restore_devices (helper);

#if HAVE_LIBNOTIFY
    /* show a notification */
    if (xfconf_channel_get_bool (helper->channel, "/ShowNotifications", TRUE)
        && notify_is_initted ())
    {
        if (helper->notification == NULL)
        {
            /* create a new notification */
            helper->notification = notify_notification_new (_("New Mouse Device"), _("A new mouse device has been plugged. Click the button "
                                                            "below to configure the new device."), "input-mouse", NULL);
            g_signal_connect (G_OBJECT (helper->notification), "closed", G_CALLBACK (xfce_pointers_helper_notification_closed), helper);
            notify_notification_add_action (helper->notification, "configure", _("Open Mouse Settings Dialog"), xfce_pointers_helper_notification_clicked, helper, NULL);
        }

        /* show the notification for (another) 4 seconds */
        notify_notification_set_timeout (helper->notification, 4000);

        /* show the notification */
        if (!notify_notification_show (helper->notification, NULL))
        {
            /* show warning with the notification information */
            g_warning ("Failed to show notification: %s", _("New Mouse Device"));

            /* failed to show the notification */
            notify_notification_close (helper->notification, NULL);
            helper->notification = NULL;
        }
    }
#endif /* !HAVE_LIBNOTIFY */

    GDK_THREADS_LEAVE ();

    return FALSE;
}



static void
xfce_pointers_helper_device_added_timeout_destroyed (gpointer user_data)
{
    XfcePointersHelper *helper = XFCE_POINTERS_HELPER (user_data);

    /* reset the timeout id */
    helper->timeout_id = 0;
}



static void
xfce_pointers_helper_device_added (LibHalContext *context,
                                   const gchar   *udi)
{
    XfcePointersHelper *helper;

    /* get the helper */
    helper = libhal_ctx_get_user_data (context);

    /* check if an input device has been added and no timeout is running */
    if (libhal_device_query_capability (context, udi, "input.mouse", NULL)
        && helper->timeout_id == 0)
    {
        /* set the device name */
        g_free (helper->last_device);
        helper->last_device = libhal_device_get_property_string (context, udi, "info.product", NULL);

        /* queue a new timeout */
        helper->timeout_id = g_timeout_add_full (G_PRIORITY_LOW, 1000, xfce_pointers_helper_device_added_timeout,
                                                 helper, xfce_pointers_helper_device_added_timeout_destroyed);
    }
}
#endif


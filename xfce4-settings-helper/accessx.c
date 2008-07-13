/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
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
 *
 *  XKB Extension code taken from the original mcs-keyboard-plugin written
 *  by Olivier Fourdan.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif /* !HAVE_LIBNOTIFY */

#include <xfce4-settings-helper/accessx.h>



static void            xfce_accessx_helper_class_init                     (XfceAccessxHelperClass *klass);
static void            xfce_accessx_helper_init                           (XfceAccessxHelper      *helper);
static void            xfce_accessx_helper_finalize                       (GObject                *object);
static void            xfce_accessx_helper_set_xkb                        (XfceAccessxHelper      *helper);
static void            xfce_accessx_helper_channel_property_changed       (XfconfChannel          *channel,
                                                                           const gchar            *property_name,
                                                                           const GValue           *value,
                                                                           XfceAccessxHelper      *helper);
#ifdef HAVE_LIBNOTIFY
static GdkFilterReturn xfce_accessx_helper_event_filter                   (GdkXEvent              *xevent,
                                                                           GdkEvent               *gdk_event,
                                                                           gpointer                user_data);
static void            xfce_accessx_helper_notification_closed            (NotifyNotification     *notification,
                                                                           XfceAccessxHelper      *helper);
static void            xfce_accessx_helper_notification_show              (XfceAccessxHelper      *helper,
                                                                           const gchar            *summary,
                                                                           const gchar            *body);
#endif /* !HAVE_LIBNOTIFY */



struct _XfceAccessxHelperClass
{
    GObjectClass __parent__;
};

struct _XfceAccessxHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel      *channel;

#ifdef HAVE_LIBNOTIFY
    NotifyNotification *notification;
#endif /* !HAVE_LIBNOTIFY */
};



G_DEFINE_TYPE (XfceAccessxHelper, xfce_accessx_helper, G_TYPE_OBJECT);



static void
xfce_accessx_helper_class_init (XfceAccessxHelperClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_accessx_helper_finalize;
}



static void
xfce_accessx_helper_init (XfceAccessxHelper *helper)
{
    gint dummy;

    helper->channel = NULL;    
#ifdef HAVE_LIBNOTIFY
    helper->notification = NULL;
#endif /* !HAVE_LIBNOTIFY */

    if (XkbQueryExtension (GDK_DISPLAY (), &dummy, &dummy, &dummy, &dummy, &dummy))
    {
        /* open the channel */
        helper->channel = xfconf_channel_new ("accessx");

        /* monitor channel changes */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_accessx_helper_channel_property_changed), helper);
        
        /* restore the xbd configuration */
        xfce_accessx_helper_set_xkb (helper);

#ifdef HAVE_LIBNOTIFY
        /* setup a connection with the notification daemon */
        if (!notify_init ("xfce4-settings-helper"))
            g_critical ("Failed to connect to the notification daemon.");

        /* add event filter */
        XkbSelectEvents (GDK_DISPLAY (), XkbUseCoreKbd, XkbControlsNotifyMask, XkbControlsNotifyMask);

        /* monitor all window events */
        gdk_window_add_filter (NULL, xfce_accessx_helper_event_filter, helper);
#endif /* !HAVE_LIBNOTIFY */
    }
    else
    {
        /* warning */
        g_critical ("Failed to initialize the Xkb extension.");
    }
}



static void
xfce_accessx_helper_finalize (GObject *object)
{
    XfceAccessxHelper *helper = XFCE_ACCESSX_HELPER (object);

#ifdef HAVE_LIBNOTIFY
    /* close an opened notification */
    if (G_UNLIKELY (helper->notification))
        notify_notification_close (helper->notification, NULL);
#endif /* !HAVE_LIBNOTIFY */

    /* release the channel */
    if (G_LIKELY (helper->channel))
        g_object_unref (G_OBJECT (helper->channel));

    (*G_OBJECT_CLASS (xfce_accessx_helper_parent_class)->finalize) (object);
}



static void
xfce_accessx_helper_set_xkb (XfceAccessxHelper *helper)
{

    XkbDescPtr xkb;

    /* allocate */
    xkb = XkbAllocKeyboard ();

    if (G_LIKELY (xkb))
    {
        /* flush and avoid crashes on x errors */
        gdk_flush ();
        gdk_error_trap_push ();

        /* load the xkb controls into the structure */
        XkbGetControls (GDK_DISPLAY (), XkbAllControlsMask, xkb);

        /* Mouse keys */
        if (xfconf_channel_get_bool (helper->channel, "/AccessX/MouseKeys", FALSE))
        {
            xkb->ctrls->enabled_ctrls |= XkbMouseKeysMask;
            xkb->ctrls->mk_delay = xfconf_channel_get_int (helper->channel, "/AccessX/MouseKeys/Delay", 100);
            xkb->ctrls->mk_interval = 1000 / xfconf_channel_get_int (helper->channel, "/AccessX/MouseKeys/Interval", 100);
            xkb->ctrls->mk_time_to_max = xfconf_channel_get_int (helper->channel, "/AccessX/MouseKeys/TimeToMax", 100);
            xkb->ctrls->mk_max_speed = xfconf_channel_get_int (helper->channel, "/AccessX/MouseKeys/Speed", 100);
        }
        else
        {
            xkb->ctrls->enabled_ctrls &= ~XkbMouseKeysMask;
        }

        /* Slow keys */
        if (xfconf_channel_get_bool (helper->channel, "/AccessX/SlowKeys", FALSE))
        {
            xkb->ctrls->enabled_ctrls |= XkbSlowKeysMask;
            xkb->ctrls->slow_keys_delay = xfconf_channel_get_int (helper->channel, "/AccessX/SlowKeys/Delay", 100);
        }
        else
        {
            xkb->ctrls->enabled_ctrls &= ~XkbSlowKeysMask;
        }

        /* Bounce keys */
        if (xfconf_channel_get_bool (helper->channel, "/AccessX/BounceKeys", FALSE))
        {
            xkb->ctrls->enabled_ctrls |= XkbBounceKeysMask;
            xkb->ctrls->debounce_delay = xfconf_channel_get_int (helper->channel, "/AccessX/BounceKeys/Delay", 100);
        }
        else
        {
            xkb->ctrls->enabled_ctrls &= ~XkbBounceKeysMask;
        }

        /* Sticky keys */
        if (xfconf_channel_get_bool (helper->channel, "/AccessX/StickyKeys", FALSE))
        {
            xkb->ctrls->enabled_ctrls |= XkbStickyKeysMask;

            if (xfconf_channel_get_bool (helper->channel, "/AccessX/StickyKeys/LatchToLock", FALSE))
                xkb->ctrls->ax_options |= XkbAX_LatchToLockMask;
            else
                xkb->ctrls->ax_options &= ~XkbAX_LatchToLockMask;

            if (xfconf_channel_get_bool (helper->channel, "/AccessX/StickyKeys/TwoKeysDisable", FALSE))
                xkb->ctrls->ax_options |= XkbAX_TwoKeysMask;
            else
                xkb->ctrls->ax_options &= ~XkbAX_TwoKeysMask;
        }
        else
        {
            xkb->ctrls->enabled_ctrls &= ~XkbStickyKeysMask;
        }

        /* If any option is set, enable AccessXKeys, otherwise: don't */
        if ((xkb->ctrls->enabled_ctrls & (XkbStickyKeysMask | XkbBounceKeysMask | XkbSlowKeysMask)) != 0)
            xkb->ctrls->enabled_ctrls |= XkbAccessXKeysMask;
        else
            xkb->ctrls->enabled_ctrls &= ~XkbAccessXKeysMask;

        /* set the new controls */
        XkbSetControls (GDK_DISPLAY (), XkbControlsEnabledMask | XkbStickyKeysMask | XkbBounceKeysMask | XkbSlowKeysMask | XkbMouseKeysMask, xkb);

        /* free the structure */
        XFree (xkb);

        /* flush errors and pop trap */
        gdk_flush ();
        gdk_error_trap_pop ();
    }
    else
    {
        /* warning */
        g_error ("XkbAllocKeyboard() returned a null pointer");
    }
}



static void
xfce_accessx_helper_channel_property_changed (XfconfChannel     *channel,
                                              const gchar       *property_name,
                                              const GValue      *value,
                                              XfceAccessxHelper *helper)
{
    g_return_if_fail (helper->channel == channel);

    /* update the xkb settings */
    xfce_accessx_helper_set_xkb (helper);
}


#ifdef HAVE_LIBNOTIFY
static GdkFilterReturn
xfce_accessx_helper_event_filter (GdkXEvent *xevent,
                                  GdkEvent  *gdk_event,
                                  gpointer   user_data)
{
    XkbEvent          *event = xevent;
    XfceAccessxHelper *helper = XFCE_ACCESSX_HELPER (user_data);
    const gchar       *body;

    switch (event->any.xkb_type)
    {
        case XkbControlsNotify:
            if ((event->ctrls.enabled_ctrl_changes & XkbStickyKeysMask) != 0)
            {
                if ((event->ctrls.enabled_ctrls & XkbStickyKeysMask) != 0)
                    body = _("Sticky keys are enabled");
                else
                    body = _("Sticky keys are disabled");

                xfce_accessx_helper_notification_show (helper, _("Sticky keys"), body);
            }
            else if ((event->ctrls.enabled_ctrl_changes & XkbSlowKeysMask) != 0)
            {
                if ((event->ctrls.enabled_ctrls & XkbSlowKeysMask) != 0)
                    body = _("Slow keys are enabled");
                else
                    body = _("Slow keys are disabled");

                xfce_accessx_helper_notification_show (helper, _("Slow keys"), body);
            }
            else if ((event->ctrls.enabled_ctrl_changes & XkbBounceKeysMask) != 0)
            {
                if ((event->ctrls.enabled_ctrls & XkbBounceKeysMask) != 0)
                    body = _("Bounce keys are enabled");
                else
                    body = _("Bounce keys are disabled");

                xfce_accessx_helper_notification_show (helper, _("Bounce keys"), body);
            }

            break;

        default:
            break;
    }

    return GDK_FILTER_CONTINUE;
}



static void
xfce_accessx_helper_notification_closed (NotifyNotification *notification,
                                         XfceAccessxHelper  *helper)
{
    g_return_if_fail (helper->notification == notification);

    /* set to null */
    helper->notification = NULL;
}



static void
xfce_accessx_helper_notification_show (XfceAccessxHelper *helper,
                                       const gchar       *summary,
                                       const gchar       *body)
{
    /* early leave the avoid dbus errors, we already 
     * told we were unable to connect during init */
    if (notify_is_initted () == FALSE)
        return;

    /* close the running notification */
    if (helper->notification == NULL)
    {
        /* create a new notification */
        helper->notification = notify_notification_new (summary, body, "keyboard", NULL);

        /* close signal */
        g_signal_connect (G_OBJECT (helper->notification), "closed", G_CALLBACK (xfce_accessx_helper_notification_closed), helper);
    }
    else
    {
        /* update the current notification */
        notify_notification_update (helper->notification, summary, body, "keyboard");
    }

    if (G_LIKELY (helper->notification))
    {
        /* show the notification for (another) 2 seconds */
        notify_notification_set_timeout (helper->notification, 2000);

        /* show the notification */
        if (!notify_notification_show (helper->notification, NULL))
        {
            /* show warning with the notification information */
            g_warning ("Failed to show notification: %s (%s).", summary, body);

            /* failed to show the notification */
            notify_notification_close (helper->notification, NULL);
            helper->notification = NULL;
        }
    }
}
#endif /* !HAVE_LIBNOTIFY */

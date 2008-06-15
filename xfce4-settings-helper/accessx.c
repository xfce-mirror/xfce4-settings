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

#ifdef HAVE_XF86MISC
#include <X11/extensions/xf86misc.h>
#endif

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libnotify/notify.h>

static NotifyNotification *accessx_notification = NULL;

static XfconfChannel *accessx_channel;
static gboolean xkbpresent = FALSE;

static gboolean accessx_initialized = FALSE;

static void
toggle_accessx (XfconfChannel *channel);

GdkFilterReturn
accessx_event_filter (GdkXEvent *xevent,
                      GdkEvent *g_event,
                      gpointer data)
{
    XkbEvent *event = xevent;

    switch(event->any.xkb_type)
    {
        case XkbBellNotify:
            break;
        case XkbControlsNotify:
            {
                if(event->ctrls.enabled_ctrl_changes & XkbStickyKeysMask)
                {
                    if(event->ctrls.enabled_ctrls & XkbStickyKeysMask)
                    {
                        notify_notification_update(accessx_notification,
                                            _("Sticky keys"),
                                            _("Sticky keys are enabled"),
                                            "keyboard");
                    }
                    else
                    {
                        notify_notification_update(accessx_notification,
                                            _("Sticky keys"),
                                            _("Sticky keys are disabled"),
                                            "keyboard");
                    }

                     notify_notification_show(accessx_notification, NULL);
                }

                if(event->ctrls.enabled_ctrl_changes & XkbSlowKeysMask)
                {
                    if(event->ctrls.enabled_ctrls & XkbSlowKeysMask)
                    {
                        notify_notification_update(accessx_notification,
                                            _("Slow keys"),
                                            _("Slow keys are enabled"),
                                            "keyboard");
                    }
                    else
                    {
                        notify_notification_update(accessx_notification,
                                            _("Slow keys"),
                                            _("Slow keys are disabled"),
                                            "keyboard");
                    }

                     notify_notification_show(accessx_notification, NULL);
                }

                if(event->ctrls.enabled_ctrl_changes & XkbBounceKeysMask)
                {
                    if(event->ctrls.enabled_ctrls & XkbBounceKeysMask)
                    {
                        notify_notification_update(accessx_notification,
                                            _("Bounce keys"),
                                            _("Bounce keys are enabled"),
                                            "keyboard");
                    }
                    else
                    {
                        notify_notification_update(accessx_notification,
                                            _("Bounce keys"),
                                            _("Bounce keys are disabled"),
                                            "keyboard");
                    }

                     notify_notification_show(accessx_notification, NULL);
                }
            }
            break;
        default:
            break;
    }
    return GDK_FILTER_CONTINUE;
}

static void
cb_accessx_channel_property_changed(XfconfChannel *channel, const gchar *name, const GValue *value, gpointer user_data)
{
    toggle_accessx(channel);
}

static void
toggle_accessx (XfconfChannel *channel)
{
    gboolean slow_keys = xfconf_channel_get_bool (channel, "/AccessX/SlowKeys", FALSE);
    gboolean sticky_keys = xfconf_channel_get_bool (channel, "/AccessX/StickyKeys", FALSE);
    gboolean bounce_keys = xfconf_channel_get_bool (channel, "/AccessX/BounceKeys", FALSE);
    gint slow_keys_delay = xfconf_channel_get_int (channel, "/AccessX/SlowKeysDelay", 100);
    gint debounce_delay = xfconf_channel_get_int (channel, "/AccessX/DeBounceDelay", 100);
    gboolean sticky_keys_ltl = xfconf_channel_get_bool (channel, "/AccessX/StickyKeys/LatchToLock", FALSE);
    gboolean sticky_keys_tk = xfconf_channel_get_bool (channel, "/AccessX/StickyKeys/TwoKeysDisable", FALSE);

    if (xkbpresent)
    {
        XkbDescPtr xkb = XkbAllocKeyboard ();
        if (xkb)
        {
            gdk_error_trap_push ();
            XkbGetControls (GDK_DISPLAY (), XkbAllControlsMask, xkb);

            /* Slow keys */
            if(slow_keys)
            {
                xkb->ctrls->enabled_ctrls |= XkbSlowKeysMask;
                xkb->ctrls->slow_keys_delay = slow_keys_delay;
            }
            else
                xkb->ctrls->enabled_ctrls &= ~XkbSlowKeysMask;

            /* Bounce keys */
            if(bounce_keys)
            {
                xkb->ctrls->enabled_ctrls |= XkbBounceKeysMask;
                xkb->ctrls->debounce_delay = debounce_delay;
            }
            else
                xkb->ctrls->enabled_ctrls &= ~XkbBounceKeysMask;

            /* Sticky keys */
            if(sticky_keys)
                xkb->ctrls->enabled_ctrls |= XkbStickyKeysMask;
            else
                xkb->ctrls->enabled_ctrls &= ~XkbStickyKeysMask;

            if(sticky_keys_ltl)
                xkb->ctrls->ax_options |= XkbAX_LatchToLockMask;
            else
                xkb->ctrls->ax_options &= ~XkbAX_LatchToLockMask;

            if(sticky_keys_tk)
                xkb->ctrls->ax_options |= XkbAX_TwoKeysMask;
            else
                xkb->ctrls->ax_options &= ~XkbAX_TwoKeysMask;

            /* If any option is set, enable AccessXKeys, otherwise: don't */
            if(sticky_keys || bounce_keys || slow_keys)
                xkb->ctrls->enabled_ctrls |= XkbAccessXKeysMask;
            else
                xkb->ctrls->enabled_ctrls &= ~XkbAccessXKeysMask;

            XkbSetControls (GDK_DISPLAY (), XkbControlsEnabledMask | XkbStickyKeysMask | XkbBounceKeysMask | XkbSlowKeysMask, xkb);
            XFree (xkb);
            gdk_flush ();
            gdk_error_trap_pop ();
        }
        else
        {
            g_warning ("XkbAllocKeyboard() returned null pointer");
        }
    }
}

void
accessx_notification_init (XfconfChannel *channel)
{
    g_return_if_fail (accessx_initialized == FALSE);

    int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;
    accessx_channel = channel;

#ifdef DEBUG
    g_message ("Querying Xkb extension");
#endif
    if (XkbQueryExtension (GDK_DISPLAY (), &xkbopcode, &xkbevent, &xkberror, &xkbmajor, &xkbminor))
    {
#ifdef DEBUG
        g_message ("Xkb extension found");
#endif
        xkbpresent = TRUE;
    }
    else
    {
#ifdef DEBUG
        g_message ("Your X server does not support Xkb extension");
#endif
        xkbpresent = FALSE;
    }
#ifdef DEBUG
    g_warning ("This build doesn't include support for Xkb extension");
#endif
    
    g_signal_connect(G_OBJECT(channel), "property-changed", (GCallback)cb_accessx_channel_property_changed, NULL);

    accessx_notification = notify_notification_new(
                               _("Accessibility Notification"),
                               _("Accessibility settings changed"),
                               "preferences-desktop-accessibility",
                               NULL);

    XkbSelectEvents(gdk_display,
                    XkbUseCoreKbd,
                    XkbControlsNotifyMask,
                    XkbControlsNotifyMask);

    gdk_window_add_filter(NULL, accessx_event_filter, NULL);

    toggle_accessx (channel);

    accessx_initialized = TRUE;
}


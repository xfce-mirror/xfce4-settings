/* $Id$ */
/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <X11/Xlib.h>

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <dbus/dbus-glib.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libxfce4kbd-private/xfce-shortcuts-provider.h>
#include <libxfce4kbd-private/xfce-shortcuts-grabber.h>

#include "keyboard-shortcuts.h"



/* Property identifiers */
enum
{
  PROP_0,
};



static void            xfce_keyboard_shortcuts_helper_finalize           (GObject                          *object);
static void            xfce_keyboard_shortcuts_helper_shortcut_added     (XfceShortcutsProvider            *provider,
                                                                          const gchar                      *shortcut,
                                                                          XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_shortcut_removed   (XfceShortcutsProvider            *provider,
                                                                          const gchar                      *shortcut,
                                                                          XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_shortcut_activated (XfceShortcutsGrabber             *grabber,
                                                                          const gchar                      *shortcut,
                                                                          gint                              timestamp,
                                                                          XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_load_shortcuts     (XfceKeyboardShortcutsHelper      *helper);



struct _XfceKeyboardShortcutsHelperClass
{
  GObjectClass __parent__;
};

struct _XfceKeyboardShortcutsHelper
{
  GObject __parent__;

  /* Xfconf channel used for managing the keyboard shortcuts */
  XfconfChannel         *channel;

  XfceShortcutsGrabber  *grabber;
  XfceShortcutsProvider *provider;
};



G_DEFINE_TYPE (XfceKeyboardShortcutsHelper, xfce_keyboard_shortcuts_helper, G_TYPE_OBJECT)



static void
xfce_keyboard_shortcuts_helper_class_init (XfceKeyboardShortcutsHelperClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_keyboard_shortcuts_helper_finalize;
}



static void
xfce_keyboard_shortcuts_helper_init (XfceKeyboardShortcutsHelper *helper)
{
  /* Create shortcuts grabber */
  helper->grabber = xfce_shortcuts_grabber_new ();

  /* Be notified when a shortcut is pressed */
  g_signal_connect (helper->grabber, "shortcut-activated", G_CALLBACK (xfce_keyboard_shortcuts_helper_shortcut_activated), helper);

  /* Create shortcuts provider */
  helper->provider = xfce_shortcuts_provider_new ("commands");

  /* Be notified of property changes */
  g_signal_connect (helper->provider, "shortcut-added", G_CALLBACK (xfce_keyboard_shortcuts_helper_shortcut_added), helper);
  g_signal_connect (helper->provider, "shortcut-removed", G_CALLBACK (xfce_keyboard_shortcuts_helper_shortcut_removed), helper);

  xfce_keyboard_shortcuts_helper_load_shortcuts (helper);
}



static void
xfce_keyboard_shortcuts_helper_finalize (GObject *object)
{
  XfceKeyboardShortcutsHelper *helper = XFCE_KEYBOARD_SHORTCUTS_HELPER (object);

  /* Free shortcuts provider */
  g_object_unref (helper->provider);

  /* Free shortcuts grabber */
  g_object_unref (helper->grabber);

  (*G_OBJECT_CLASS (xfce_keyboard_shortcuts_helper_parent_class)->finalize) (object);
}



static void
xfce_keyboard_shortcuts_helper_shortcut_added (XfceShortcutsProvider       *provider,
                                               const gchar                 *shortcut,
                                               XfceKeyboardShortcutsHelper *helper)
{
  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  DBG ("shortcut = %s", shortcut);
  xfce_shortcuts_grabber_add (helper->grabber, shortcut);
}



static void
xfce_keyboard_shortcuts_helper_shortcut_removed (XfceShortcutsProvider       *provider,
                                                 const gchar                 *shortcut,
                                                 XfceKeyboardShortcutsHelper *helper)
{
  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  DBG ("shortcut = %s", shortcut);
  xfce_shortcuts_grabber_remove (helper->grabber, shortcut);
}



static void
_xfce_keyboard_shortcuts_helper_load_shortcut (XfceShortcut                *shortcut,
                                               XfceKeyboardShortcutsHelper *helper)
{
  g_return_if_fail (shortcut != NULL);
  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));

  DBG ("shortcut = %s", shortcut->shortcut);
  xfce_shortcuts_grabber_add (helper->grabber, shortcut->shortcut);
}



static void
xfce_keyboard_shortcuts_helper_load_shortcuts (XfceKeyboardShortcutsHelper *helper)
{
  GList *shortcuts;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));

  /* Load shortcuts one by one */
  shortcuts = xfce_shortcuts_provider_get_shortcuts (helper->provider);
  g_list_foreach (shortcuts, (GFunc) _xfce_keyboard_shortcuts_helper_load_shortcut, helper);
  xfce_shortcuts_free (shortcuts);
}



static void
xfce_keyboard_shortcuts_helper_shortcut_activated (XfceShortcutsGrabber        *grabber,
                                                   const gchar                 *shortcut,
                                                   gint                         timestamp,
                                                   XfceKeyboardShortcutsHelper *helper)
{
  XfceShortcut  *sc;
  GError        *error = NULL;
  gchar        **argv;
  gboolean       succeed;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  g_return_if_fail (XFCE_IS_SHORTCUTS_PROVIDER (helper->provider));

  /* Ignore empty shortcuts */
  if (shortcut == NULL || g_utf8_strlen (shortcut, -1) == 0)
    return;

  DBG  ("shortcut = %s", shortcut);

  /* Get shortcut from the provider */
  sc = xfce_shortcuts_provider_get_shortcut (helper->provider, shortcut);

  if (G_UNLIKELY (sc == NULL))
    return;

  DBG ("command = %s, snotify = %s (time = %d)",
       sc->command, sc->snotify ? "true" : "false", timestamp);

  /* Handle the argv ourselfs, because xfce_spawn_command_line_on_screen() does
   * not accept a custom timestamp for startup notification */
  succeed = g_shell_parse_argv (sc->command, NULL, &argv, &error);
  if (G_LIKELY (succeed))
    {
      succeed = xfce_spawn_on_screen_with_child_watch (xfce_gdk_screen_get_active (NULL),
                                                       NULL, argv, NULL,
                                                       G_SPAWN_SEARCH_PATH, sc->snotify,
                                                       timestamp, NULL, NULL, &error);

      g_strfreev (argv);
    }

  if (!succeed)
    {
      g_error ("Failed to spawn command \"%s\": %s", sc->command, error->message);
      g_error_free (error);
    }

  xfce_shortcut_free (sc);
}

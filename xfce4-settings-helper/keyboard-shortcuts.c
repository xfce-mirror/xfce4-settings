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

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>

#include "common/xfce-shortcuts-provider.h"
#include "common/xfce-shortcuts-grabber.h"

#include "keyboard-shortcuts.h"



/* Property identifiers */
enum
{
  PROP_0,
};



static void            xfce_keyboard_shortcuts_helper_class_init         (XfceKeyboardShortcutsHelperClass *klass);
static void            xfce_keyboard_shortcuts_helper_init               (XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_finalize           (GObject                          *object);
static void            xfce_keyboard_shortcuts_helper_get_property       (GObject                          *object,
                                                                          guint                             prop_id,
                                                                          GValue                           *value,
                                                                          GParamSpec                       *pspec);
static void            xfce_keyboard_shortcuts_helper_set_property       (GObject                          *object,
                                                                          guint                             prop_id,
                                                                          const GValue                     *value,
                                                                          GParamSpec                       *pspec);
static void            xfce_keyboard_shortcuts_helper_shortcut_added     (XfceShortcutsProvider            *provider,
                                                                          const gchar                      *shortcut,
                                                                          XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_shortcut_removed   (XfceShortcutsProvider            *provider,
                                                                          const gchar                      *shortcut,
                                                                          XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_shortcut_activated (XfceShortcutsGrabber             *grabber,
                                                                          const gchar                      *shortcut,
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



static GObjectClass *xfce_keyboard_shortcuts_helper_parent_class = NULL;



GType
xfce_keyboard_shortcuts_helper_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (XfceKeyboardShortcutsHelperClass),
        NULL,
        NULL,
        (GClassInitFunc) xfce_keyboard_shortcuts_helper_class_init,
        NULL,
        NULL,
        sizeof (XfceKeyboardShortcutsHelper),
        0,
        (GInstanceInitFunc) xfce_keyboard_shortcuts_helper_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "XfceKeyboardShortcutsHelper", &info, 0);
    }

  return type;
}



static void
xfce_keyboard_shortcuts_helper_class_init (XfceKeyboardShortcutsHelperClass *klass)
{
  GObjectClass *gobject_class;

  /* Determine the parent type class */
  xfce_keyboard_shortcuts_helper_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_keyboard_shortcuts_helper_finalize;
  gobject_class->get_property = xfce_keyboard_shortcuts_helper_get_property;
  gobject_class->set_property = xfce_keyboard_shortcuts_helper_set_property;
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
xfce_keyboard_shortcuts_helper_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  /* XfceKeyboardShortcutsHelper *helper = XFCE_KEYBOARD_SHORTCUTS_HELPER (object); */

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_keyboard_shortcuts_helper_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  /* XfceKeyboardShortcutsHelper *helper = XFCE_KEYBOARD_SHORTCUTS_HELPER (object); */

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
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
                                                   XfceKeyboardShortcutsHelper *helper)
{
  XfceShortcut *sc;
  GdkDisplay   *display;
  GdkScreen    *screen;
  GError       *error = NULL;
  gint          monitor;

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

  display = gdk_display_get_default ();

  DBG ("command = %s", sc->command);

  /* Determine active monitor */
  screen = xfce_gdk_display_locate_monitor_with_pointer (display, &monitor);

  /* Spawn command */
  if (!G_UNLIKELY (!xfce_gdk_spawn_command_line_on_screen (screen, sc->command, &error)))
    if (G_LIKELY (error != NULL))
      {
        g_error ("%s", error->message);
        g_error_free (error);
      }

  xfce_shortcut_free (sc);
}

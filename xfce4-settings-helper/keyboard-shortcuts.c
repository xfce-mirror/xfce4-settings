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

#include "keyboard-shortcuts.h"
#include "frap-shortcuts.h"



/* Property identifiers */
enum
{
  PROP_0,
};



static void            xfce_keyboard_shortcuts_helper_class_init       (XfceKeyboardShortcutsHelperClass *klass);
static void            xfce_keyboard_shortcuts_helper_init             (XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_finalize         (GObject                          *object);
static void            xfce_keyboard_shortcuts_helper_get_property     (GObject                          *object,
                                                                        guint                             prop_id,
                                                                        GValue                           *value,
                                                                        GParamSpec                       *pspec);
static void            xfce_keyboard_shortcuts_helper_set_property     (GObject                          *object,
                                                                        guint                             prop_id,
                                                                        const GValue                     *value,
                                                                        GParamSpec                       *pspec);
static void           xfce_keyboard_shortcuts_helper_load_shortcut     (const gchar                      *key,
                                                                        const GValue                     *value,
                                                                        XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_shortcut_callback (const gchar                     *shortcut,
                                                                         XfceKeyboardShortcutsHelper     *helper);
static void            xfce_keyboard_shortcuts_helper_property_changed (XfconfChannel                    *channel,
                                                                        gchar                            *property,
                                                                        GValue                           *value,
                                                                        XfceKeyboardShortcutsHelper      *helper);



struct _XfceKeyboardShortcutsHelperClass
{
  GObjectClass __parent__;
};

struct _XfceKeyboardShortcutsHelper
{
  GObject __parent__;

  /* Xfconf channel used for managing the keyboard shortcuts */
  XfconfChannel *channel;

  /* Hash table for (shortcut -> action) mapping */
  GHashTable    *shortcuts;
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
  GHashTable *properties;

  /* Get Xfconf channel */
  helper->channel = frap_shortcuts_get_channel ();

  /* Create hash table for (shortcut -> command) mapping */
  helper->shortcuts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* Get all properties of the channel */
  properties = xfconf_channel_get_properties (helper->channel, NULL);

  if (G_LIKELY (properties != NULL))
    {
      /* Filter shortcuts */
      g_hash_table_foreach (properties, (GHFunc) xfce_keyboard_shortcuts_helper_load_shortcut, helper);

      /* Destroy properties */
      g_hash_table_destroy (properties);
    }

  /* Set shortcut callback */
  frap_shortcuts_set_shortcut_callback ((FrapShortcutsFunc) xfce_keyboard_shortcuts_helper_shortcut_callback, helper);

  /* Be notified of property changes */
  g_signal_connect (helper->channel, "property-changed", G_CALLBACK (xfce_keyboard_shortcuts_helper_property_changed), helper);
}



static void
xfce_keyboard_shortcuts_helper_finalize (GObject *object)
{
  XfceKeyboardShortcutsHelper *helper = XFCE_KEYBOARD_SHORTCUTS_HELPER (object);

  /* Free shortcuts hash table */
  g_hash_table_destroy (helper->shortcuts);

  /* Free Xfconf channel */
  g_object_unref (helper->channel);

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
xfce_keyboard_shortcuts_helper_load_shortcut (const gchar                 *key,
                                              const GValue                *value,
                                              XfceKeyboardShortcutsHelper *helper)
{
  FrapShortcutsType type;
  gchar            *action;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  g_return_if_fail (G_IS_VALUE (value));

  if (G_LIKELY (frap_shortcuts_parse_value (value, &type, &action)))
    {
      /* Establish passive grab on the shortcut and add it to the hash table */
      if (G_LIKELY (frap_shortcuts_grab_shortcut (key + 1, FALSE)))
        {
          /* Add shortcut -> action pair to the hash table */
          g_hash_table_insert (helper->shortcuts, g_strdup (key + 1), g_strdup (action));
        }
      else
        g_warning ("Failed to load shortcut '%s'", key + 1);

      g_free (action);
    }
}



static void
xfce_keyboard_shortcuts_helper_shortcut_callback (const gchar                 *shortcut,
                                                  XfceKeyboardShortcutsHelper *helper)
{
  GdkDisplay  *display;
  GdkScreen   *screen;
  GError      *error = NULL;
  const gchar *action;
  gint         monitor;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));

  if (shortcut == NULL || g_utf8_strlen (shortcut, -1) == 0)
    return;

  display = gdk_display_get_default ();

  DBG  ("shortcut_callback: shortcut = %s", shortcut);

  if ((action = g_hash_table_lookup (helper->shortcuts, shortcut)) != NULL)
    {
      DBG ("shortcut_callback: action = %s", action);

      /* Determine active monitor */
      screen = xfce_gdk_display_locate_monitor_with_pointer (display, &monitor);

      /* Spawn command */
      if (!G_UNLIKELY (!xfce_gdk_spawn_command_line_on_screen (screen, action, &error)))
        if (G_LIKELY (error != NULL))
          {
            g_warning ("%s", error->message);
            g_error_free (error);
          }
    }
}



static void
xfce_keyboard_shortcuts_helper_property_changed (XfconfChannel               *channel,
                                                 gchar                       *property,
                                                 GValue                      *value,
                                                 XfceKeyboardShortcutsHelper *helper)
{
  FrapShortcutsType type;
  gchar            *action;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  g_return_if_fail (XFCONF_IS_CHANNEL (channel));

  /* Check whether the property was removed */
  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    {
      /* Remove shortcut and ungrab keys if we're monitoring it already */
      if (G_LIKELY (g_hash_table_lookup (helper->shortcuts, property + 1)))
        {
          DBG ("property_changed: removing shortcut = %s", property + 1);

          /* Remove shortcut from the hash table */
          g_hash_table_remove (helper->shortcuts, property + 1);

          /* Ungrab the shortcut */
          frap_shortcuts_grab_shortcut (property + 1, TRUE);
        }
    }
  else
    {
      /* Try to read shortcut information from the GValue. If not, it's probably an Xfwm4 shortcut */
      if (G_LIKELY (frap_shortcuts_parse_value (value, &type, &action)))
        {
          /* Check whether the shortcut already exists */
          if (g_hash_table_lookup (helper->shortcuts, property + 1))
            {
              if (type == FRAP_SHORTCUTS_EXECUTE)
                {
                  DBG ("property_changed: changing action of shortcut = %s to %s", property + 1, action);

                  /* Replace the current action. The key combination hasn't changed so don't ungrab/grab */
                  g_hash_table_replace (helper->shortcuts, property + 1, g_strdup (action));
                }
              else
                {
                  DBG ("property_changed: removing shortcut = %s", property + 1);

                  /* Remove shortcut from the hash table */
                  g_hash_table_remove (helper->shortcuts, property + 1);

                  /* Ungrab the shortcut */
                  frap_shortcuts_grab_shortcut (property + 1, FALSE);
                }
            }
          else
            {
              if (type == FRAP_SHORTCUTS_EXECUTE)
                {
                  DBG ("property_changed: adding shortcut = %s", property + 1);

                  /* Insert shortcut into the hash table */
                  g_hash_table_insert (helper->shortcuts, g_strdup (property + 1), g_strdup (action));

                  /* Establish passive keyboard grab for the new shortcut */
                  frap_shortcuts_grab_shortcut (property + 1, FALSE);
                }
            }

          g_free (action);
        }
    }
}

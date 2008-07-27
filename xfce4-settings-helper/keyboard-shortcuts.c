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

#include <xfconf/xfconf.h>
#include <libxfcegui4/libxfcegui4.h>
#include <dbus/dbus-glib.h>

#include "keyboard-shortcuts.h"



/* Modifiers to be ignored (0x2000 is an Xkb modifier) */
#define IGNORED_MODIFIERS (0x2000 | GDK_LOCK_MASK | GDK_HYPER_MASK)

/* Modifiers to be used */
#define USED_MODIFIERS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_SUPER_MASK | GDK_META_MASK | \
                        GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK)




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
static void            xfce_keyboard_shortcuts_helper_add_filter       (XfceKeyboardShortcutsHelper      *helper);
static GdkFilterReturn xfce_keyboard_shortcuts_helper_filter           (GdkXEvent                        *gdk_xevent,
                                                                        GdkEvent                         *event,
                                                                        XfceKeyboardShortcutsHelper      *helper);
static void            xfce_keyboard_shortcuts_helper_load_shortcut    (const gchar                      *key,
                                                                        const GValue                     *value,
                                                                        XfceKeyboardShortcutsHelper      *helper);
static gboolean        xfce_keyboard_shortcuts_helper_grab_shortcut    (XfceKeyboardShortcutsHelper      *helper,
                                                                        const gchar                      *shortcut,
                                                                        gboolean                          grab);
static gboolean        xfce_keyboard_shortcuts_helper_parse_shortcut   (XfceKeyboardShortcutsHelper      *helper,
                                                                        GdkDisplay                       *display,
                                                                        const gchar                      *shortcut,
                                                                        guint                            *keyval,
                                                                        GdkModifierType                  *modifiers,
                                                                        KeyCode                          *keycode,
                                                                        guint                            *grab_mask);
static gboolean        xfce_keyboard_shortcuts_helper_grab_real        (XfceKeyboardShortcutsHelper      *helper,
                                                                        KeyCode                           keycode,
                                                                        guint                             modifiers,
                                                                        Display                          *display,
                                                                        Window                            window,
                                                                        gboolean                          grab);
static void            xfce_keyboard_shortcuts_helper_handle_key_press (XfceKeyboardShortcutsHelper      *helper,
                                                                        XKeyEvent                        *xevent);
static void            xfce_keyboard_shortcuts_helper_property_changed (XfconfChannel                    *channel,
                                                                        gchar                            *property,
                                                                        GValue                           *value,
                                                                        XfceKeyboardShortcutsHelper      *helper);
static gboolean        xfce_keyboard_shortcuts_helper_extract_values   (XfceKeyboardShortcutsHelper      *helper,
                                                                        const gchar                      *key,
                                                                        const GValue                     *value,
                                                                        gchar                           **shortcut,
                                                                        gchar                           **action);




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

  /* Flag to avoid handling multiple key press events at the same time */
  gboolean       waiting_for_key_release;
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

  helper->waiting_for_key_release = FALSE;

  /* Get Xfconf channel */
  helper->channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

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

  xfce_keyboard_shortcuts_helper_add_filter (helper);

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
xfce_keyboard_shortcuts_helper_add_filter (XfceKeyboardShortcutsHelper *helper)
{
  GdkDisplay *display;
#if 0
  GdkScreen  *screen;
  gint        screens;
  gint        i;
#endif

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));

  display = gdk_display_get_default ();
#if 0
  screens = gdk_display_get_n_screens (display);
#endif

  /* Flush events before adding the event filter */
  XAllowEvents (GDK_DISPLAY_XDISPLAY (display), AsyncBoth, CurrentTime);

  /* Add event filter to the root window of each screen. FIXME: I'm not
   * exactly sure which one of these two options I should use: */
#if 0
  for (i = 0; i < screens; ++i)
    {
      screen = gdk_display_get_screen (display, i);
      gdk_window_add_filter (gdk_screen_get_root_window (screen), (GdkFilterFunc) xfce_keyboard_shortcuts_helper_filter, helper);
    }
#else
  gdk_window_add_filter (NULL, (GdkFilterFunc) xfce_keyboard_shortcuts_helper_filter, helper);
#endif
}



static GdkFilterReturn
xfce_keyboard_shortcuts_helper_filter (GdkXEvent                   *gdk_xevent,
                                       GdkEvent                    *event,
                                       XfceKeyboardShortcutsHelper *helper)
{
  XEvent *xevent = (XEvent *) gdk_xevent;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper), GDK_FILTER_CONTINUE);

  switch (xevent->type)
    {
      case KeyPress:
        xfce_keyboard_shortcuts_helper_handle_key_press (helper, (XKeyEvent *) xevent);
        break;
      case KeyRelease:
        helper->waiting_for_key_release = FALSE;
        break;
      case MappingNotify:
        break;
      default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}



static void
xfce_keyboard_shortcuts_helper_load_shortcut (const gchar                 *key,
                                              const GValue                *value,
                                              XfceKeyboardShortcutsHelper *helper)
{
  gchar *shortcut;
  gchar *action;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  g_return_if_fail (G_IS_VALUE (value));

  /* Only add shortcuts of type 'execute' */
  if (G_LIKELY (xfce_keyboard_shortcuts_helper_extract_values (helper, key, value, &shortcut, &action)))
    {
      /* Establish passive grab on the shortcut and add it to the hash table */
      if (G_LIKELY (xfce_keyboard_shortcuts_helper_grab_shortcut (helper, shortcut, TRUE)))
        {
          /* Add shortcut -> action pair to the hash table */
          g_hash_table_insert (helper->shortcuts, g_strdup (shortcut), g_strdup (action));
        }
      else
        g_warning ("Failed to load shortcut '%s'", key + 1);

      /* Free strings */
      g_free (shortcut);
      g_free (action);
    }
}



static gboolean
xfce_keyboard_shortcuts_helper_grab_shortcut (XfceKeyboardShortcutsHelper *helper,
                                              const char                  *shortcut,
                                              gboolean                     grab)
{
  GdkModifierType modifiers;
  GdkDisplay     *display;
  GdkScreen      *screen;
  Display        *xdisplay;
  KeyCode         keycode;
  Window          xwindow;
  guint           grab_mask;
  guint           keyval;
  gint            bits[32];
  gint            current_mask;
  gint            ignore_mask;
  gint            screens;
  gint            n_bits;
  gint            i;
  gint            j;
  gint            k;

  display = gdk_display_get_default ();
  screens = gdk_display_get_n_screens (display);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  /* Parse the shortcut and abort if that fails */
  if (G_UNLIKELY (!xfce_keyboard_shortcuts_helper_parse_shortcut (helper, display, shortcut, &keyval, &modifiers, &keycode, &grab_mask)))
    {
      g_warning ("Could not parse shortcut '%s', most likely because it is invalid", shortcut);
      return FALSE ;
    }

  /* Create mask containing ignored modifier bits set which are not set in the grab_mask already */
  ignore_mask = IGNORED_MODIFIERS & ~grab_mask & GDK_MODIFIER_MASK;

  /* Store indices of all set bits of the ignore mask in an array */
  for (i = 0, n_bits = 0; ignore_mask != 0; ++i, ignore_mask >>= 1)
    if ((ignore_mask & 0x1) != 0)
      bits[n_bits++] = i;

  for (i = 0; i < (1 << n_bits); ++i)
    {
      /* Map bits in the counter to those in the mask and thereby retrieve all ignored bit
       * mask combinations */
      for (current_mask = 0, j = 0; j < n_bits; ++j)
        if ((i & (1 << j)) != 0)
          current_mask |= (1 << bits[j]);

      /* Grab key on all screens */
      for (k = 0; k < screens; ++k)
        {
          /* Get current screen and root X window */
          screen = gdk_display_get_screen (display, k);
          xwindow = GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen));

          /* Really grab or ungrab the key now */
          if (G_UNLIKELY (!xfce_keyboard_shortcuts_helper_grab_real (helper, keycode, current_mask | grab_mask, xdisplay, xwindow, grab)))
            {
              g_warning ("Could not grab shortcut '%s'. It might be used by another application already.", shortcut);
              return FALSE;
            }
        }
    }

  return TRUE;
}



static gboolean
xfce_keyboard_shortcuts_helper_parse_shortcut (XfceKeyboardShortcutsHelper *helper,
                                               GdkDisplay                  *display,
                                               const gchar                 *shortcut,
                                               guint                       *keyval,
                                               GdkModifierType             *modifiers,
                                               KeyCode                     *keycode,
                                               guint                       *grab_mask)
{
  g_return_val_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper), FALSE);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* Reset keycode and grab mask */
  *keycode = 0;
  *grab_mask = 0;

  /* Parse the GTK+ accelerator string */
  gtk_accelerator_parse (shortcut, keyval, modifiers);

  /* Abort when the accelerator string is invalid */
  if (G_LIKELY (*keyval == 0 && *modifiers == 0))
    return FALSE;

  /* Try to convert the keyval into a X11 keycode */
  if ((*keycode = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY (display), *keyval)) == 0)
    return FALSE;

  /* FIXME: I'm not really sure about this */
  *grab_mask = *modifiers;

  return TRUE;
}



static gboolean
xfce_keyboard_shortcuts_helper_grab_real (XfceKeyboardShortcutsHelper *helper,
                                          KeyCode                      keycode,
                                          guint                        modifiers,
                                          Display                     *display,
                                          Window                       window,
                                          gboolean                     grab)
{
  gdk_error_trap_push ();

  if (G_LIKELY (grab))
    XGrabKey (display, keycode, modifiers, window, FALSE, GrabModeAsync, GrabModeAsync);
  else
    XUngrabKey (display, keycode, modifiers, window);

  gdk_flush ();

  return gdk_error_trap_pop () == 0;
}



static void
xfce_keyboard_shortcuts_helper_handle_key_press (XfceKeyboardShortcutsHelper *helper,
                                                 XKeyEvent                   *xevent)
{
  GdkDisplay  *display;
  GdkScreen   *screen;
  GError      *error = NULL;
  KeySym       keysym;
  gchar       *accelerator_name;
  const gchar *key;
  const gchar *value;
  gint         monitor;
  gint         modifiers;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));

  /* Don't handle multiple key press events in parallel (avoid weird behaviour) */
  if (G_UNLIKELY (helper->waiting_for_key_release))
    return;

  /* Get display information */
  display = gdk_display_get_default ();

  /* Convert event keycode to keysym */
  keysym = XKeycodeToKeysym (GDK_DISPLAY_XDISPLAY (display), xevent->keycode, 0);

  /* Remove ignored modifiers and non-modifier keys from the event state mask */
  modifiers = xevent->state & ~IGNORED_MODIFIERS & GDK_MODIFIER_MASK;

  /* Get accelerator string for the pressed keys */
  accelerator_name = gtk_accelerator_name (keysym, modifiers);

  /* Perform accelerator lookup */
  if (G_LIKELY (g_hash_table_lookup_extended (helper->shortcuts, accelerator_name, (gpointer) &key, (gpointer) &value)))
    {
      /* We have to wait for a release event before handling another key press event */
      helper->waiting_for_key_release = TRUE;

      /* Determine active monitor */
      screen = xfce_gdk_display_locate_monitor_with_pointer (display, &monitor);

      /* Spawn command */
      if (G_UNLIKELY (!xfce_gdk_spawn_command_line_on_screen (screen, value, &error)))
        {
          if (G_LIKELY (error != NULL))
            {
              g_warning ("%s", error->message);
              g_error_free (error);
            }
        }
    }

  /* Free accelerator string */
  g_free (accelerator_name);
}



static void
xfce_keyboard_shortcuts_helper_property_changed (XfconfChannel               *channel,
                                                 gchar                       *property,
                                                 GValue                      *value,
                                                 XfceKeyboardShortcutsHelper *helper)
{
  gchar *shortcut;
  gchar *action;

  g_return_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper));
  g_return_if_fail (XFCONF_IS_CHANNEL (channel));

  /* Check whether the property was removed */
  if (!G_IS_VALUE (value) || G_VALUE_TYPE (value) == G_TYPE_INVALID)
    {
      /* Remove shortcut and ungrab keys if we're monitoring it already */
      if (G_LIKELY (g_hash_table_lookup (helper->shortcuts, property + 1)))
        {
          /* Remove shortcut from the hash table */
          g_hash_table_remove (helper->shortcuts, property + 1);

          /* Ungrab the shortcut */
          xfce_keyboard_shortcuts_helper_grab_shortcut (helper, property + 1, FALSE);
        }
    }
  else
    {
      /* Try to read shortcut information from the GValue. If not, it's probably an Xfwm4 shortcut */
      if (G_LIKELY (xfce_keyboard_shortcuts_helper_extract_values (helper, property, value, &shortcut, &action)))
        {
          /* Check whether the shortcut already exists */
          if (g_hash_table_lookup (helper->shortcuts, shortcut))
            {
              /* Replace the current action. The key combination hasn't changed so don't ungrab/grab */
              g_hash_table_replace (helper->shortcuts, shortcut, g_strdup (action));
            }
          else
            {
              /* Insert shortcut into the hash table */
              g_hash_table_insert (helper->shortcuts, g_strdup (shortcut), g_strdup (action));

              /* Establish passive keyboard grab for the new shortcut */
              xfce_keyboard_shortcuts_helper_grab_shortcut (helper, shortcut, TRUE);
            }

          /* Free strings */
          g_free (shortcut);
          g_free (action);
        }
    }
}



static gboolean
xfce_keyboard_shortcuts_helper_extract_values (XfceKeyboardShortcutsHelper  *helper,
                                               const gchar                  *key,
                                               const GValue                 *value,
                                               gchar                       **shortcut,
                                               gchar                       **action)
{
  const GPtrArray *array;
  const GValue    *type_value;
  const GValue    *action_value;
  gboolean         result = FALSE;

  g_return_val_if_fail (XFCE_IS_KEYBOARD_SHORTCUTS_HELPER (helper), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  /* Non-arrays will not be accepted */
  if (G_UNLIKELY (G_VALUE_TYPE (value) != dbus_g_type_get_collection ("GPtrArray", G_TYPE_VALUE)))
    return FALSE;

  /* Get the pointer array */
  array = g_value_get_boxed (value);

  /* Make sure the array has exactly two members */
  if (G_UNLIKELY (array->len != 2))
    return FALSE;

  /* Get GValues for the array members */
  type_value = g_ptr_array_index (array, 0);
  action_value = g_ptr_array_index (array, 1);

  /* Make sure both are string values */
  if (G_UNLIKELY (G_VALUE_TYPE (type_value) != G_TYPE_STRING || G_VALUE_TYPE (action_value) != G_TYPE_STRING))
    return FALSE;

  /* Check whether the type is 'execute' */
  if (G_LIKELY (g_utf8_collate (g_value_get_string (type_value), "execute") == 0))
    {
      /* Get shortcut and action strings */
      *shortcut = g_strdup (key + 1);
      *action = g_strdup (g_value_get_string (action_value));

      result = TRUE;
    }

  return result;
}

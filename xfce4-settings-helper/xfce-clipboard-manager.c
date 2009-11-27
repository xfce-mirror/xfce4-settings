/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Matthias Clasen
 * Copyright (C) 2007 Anders Carlsson
 * Copyright (C) 2007 Rodrigo Moya
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2009 Mike Massonnet <mmassonnet@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "xfce-clipboard-manager.h"

struct _XfceClipboardManagerClass
{
  GObjectClass __parent__;
};

struct _XfceClipboardManager
{
  GObject __parent__;

  GtkClipboard *default_clipboard;
  GtkClipboard *primary_clipboard;

  GSList       *default_cache;
  gchar        *primary_cache;

  gboolean      internal_change;

  GtkWidget    *window;
};



G_DEFINE_TYPE (XfceClipboardManager, xfce_clipboard_manager, G_TYPE_OBJECT)



static void
xfce_clipboard_manager_class_init (XfceClipboardManagerClass *klass)
{
}



static void
xfce_clipboard_manager_init (XfceClipboardManager *manager)
{
  manager->default_clipboard =
    gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  manager->primary_clipboard =
    gtk_clipboard_get (GDK_SELECTION_PRIMARY);

  manager->default_cache = NULL;
  manager->primary_cache = NULL;
}



Atom XA_CLIPBOARD_MANAGER;
Atom XA_MANAGER;

static void
init_atoms (Display *display)
{
  static int _init_atoms = 0;

  if (_init_atoms > 0)
    return;

  XA_CLIPBOARD_MANAGER = XInternAtom (display, "CLIPBOARD_MANAGER", False);
  XA_MANAGER = XInternAtom (display, "MANAGER", False);

  _init_atoms = 1;
}



static void
xfce_clipboard_manager_default_store (XfceClipboardManager *manager)
{
  GtkSelectionData *selection_data;
  GdkAtom          *atoms;
  gint              n_atoms;
  gint              i;

  g_return_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager));

  if (!gtk_clipboard_wait_for_targets (manager->default_clipboard, &atoms, &n_atoms))
    return;

  if (manager->default_cache != NULL)
    {
      g_slist_foreach (manager->default_cache, (GFunc)gtk_selection_data_free, NULL);
                       g_slist_free (manager->default_cache);
      manager->default_cache = NULL;
    }

  for (i = 0; i < n_atoms; i++)
    {
      if (atoms[i] == gdk_atom_intern_static_string ("TARGETS") ||
          atoms[i] == gdk_atom_intern_static_string ("MULTIPLE") ||
          atoms[i] == gdk_atom_intern_static_string ("DELETE") ||
          atoms[i] == gdk_atom_intern_static_string ("INSERT_PROPERTY") ||
          atoms[i] == gdk_atom_intern_static_string ("INSERT_SELECTION") ||
          atoms[i] == gdk_atom_intern_static_string ("PIXMAP"))
        {
          continue;
        }

      selection_data = gtk_clipboard_wait_for_contents (manager->default_clipboard, atoms[i]);

      if (selection_data == NULL)
        return;

      manager->default_cache = g_slist_prepend (manager->default_cache, selection_data);
    }
}



static void
xfce_clipboard_manager_default_get_func (GtkClipboard     *clipboard,
                                         GtkSelectionData *selection_data,
                                         guint             info,
                                         gpointer          user_data)
{
  XfceClipboardManager *manager = XFCE_CLIPBOARD_MANAGER (user_data);
  GSList               *list;
  GtkSelectionData     *selection_data_cache = NULL;

  g_return_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager));

  list = manager->default_cache;

  for (; list->next != NULL; list = list->next)
    {
      selection_data_cache = list->data;

      if (selection_data->target == selection_data_cache->target)
        break;

      selection_data_cache = NULL;
    }

  if (selection_data_cache == NULL)
    return;

  gtk_selection_data_set (selection_data, selection_data->target,
                          selection_data_cache->format,
                          selection_data_cache->data,
                          selection_data_cache->length);
}



static void
xfce_clipboard_manager_default_clear_func (GtkClipboard *clipboard,
                                           gpointer      user_data)
{
  return;
}



static void
xfce_clipboard_manager_default_restore (XfceClipboardManager *manager)
{
  GtkTargetList    *target_list;
  GtkTargetEntry   *targets;
  gint              n_targets;
  GtkSelectionData *sdata;
  GSList           *list;

  g_return_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager));

  target_list = gtk_target_list_new (NULL, 0);
  list = manager->default_cache;

  for (; list->next != NULL; list = list->next)
    {
      sdata = list->data;
      gtk_target_list_add (target_list, sdata->target, 0, 0);
    }

  targets = gtk_target_table_new_from_list (target_list, &n_targets);

  gtk_clipboard_set_with_data (manager->default_clipboard,
                               targets, n_targets,
                               xfce_clipboard_manager_default_get_func,
                               xfce_clipboard_manager_default_clear_func,
                               manager);
}



static void
xfce_clipboard_manager_default_owner_change (XfceClipboardManager *manager,
                                             GdkEventOwnerChange  *event)
{
  g_return_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager));

  if (event->send_event == TRUE)
    return;

  if (event->owner != 0)
    {
      if (manager->internal_change)
        {
          manager->internal_change = FALSE;
          return;
        }

      xfce_clipboard_manager_default_store (manager);
    }
  else
    {
      /* This 'bug' happens with Mozilla applications, it means that
       * we restored the clipboard (we own it now) but somehow we are
       * being noticed twice about that fact where first the owner is
       * 0 (which is when we must restore) but then again where the
       * owner is ourself (which is what normally only happens and
       * also that means that we have to store the clipboard content
       * e.g. owner is not 0). By the second time we would store
       * ourself back with an empty clipboard... solution is to jump
       * over the first time and don't try to restore empty data. */
      if (manager->internal_change)
        return;

      manager->internal_change = TRUE;
      xfce_clipboard_manager_default_restore (manager);
    }
}



static void
xfce_clipboard_manager_primary_owner_change (XfceClipboardManager *manager,
                                             GdkEventOwnerChange *event)
{
  gchar *text;

  g_return_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager));

  if (event->send_event == TRUE)
    return;

  if (event->owner != 0)
    {
      text = gtk_clipboard_wait_for_text (manager->primary_clipboard);

      if (text != NULL)
        {
          g_free (manager->primary_cache);
          manager->primary_cache = text;
        }
    }
  else
    {
      if (manager->primary_cache != NULL)
        gtk_clipboard_set_text (manager->primary_clipboard,
                                manager->primary_cache,
                                -1);
    }
}



XfceClipboardManager *
xfce_clipboard_manager_new (void)
{
 return XFCE_CLIPBOARD_MANAGER (g_object_new (XFCE_TYPE_CLIPBOARD_MANAGER,
                                              NULL));
}



gboolean
xfce_clipboard_manager_start (XfceClipboardManager *manager)
{
  XClientMessageEvent     xev;
  Display                *display;
  Window                  window;
  Time                    timestamp;

  g_return_val_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager), FALSE);

  display = GDK_DISPLAY ();
  init_atoms (display);

  /* Check if there is a clipboard manager running */
  if (gdk_display_supports_clipboard_persistence (gdk_display_get_default ()))
    {
      g_warning ("A clipboard manager is already running.");
      return FALSE;
    }

  manager->window = gtk_invisible_new ();
  gtk_widget_realize (manager->window);

  window = GDK_WINDOW_XID (manager->window->window);
  timestamp = GDK_CURRENT_TIME;

  XSelectInput (display, window, PropertyChangeMask);
  XSetSelectionOwner (display, XA_CLIPBOARD_MANAGER, window, timestamp);

  g_signal_connect_swapped (manager->default_clipboard, "owner-change",
                            G_CALLBACK (xfce_clipboard_manager_default_owner_change),
                            manager);
  g_signal_connect_swapped (manager->primary_clipboard, "owner-change",
                            G_CALLBACK (xfce_clipboard_manager_primary_owner_change),
                            manager);

  /* Check to see if we managed to claim the selection. If not,
   * we treat it as if we got it then immediately lost it
   */
  if (XGetSelectionOwner (display, XA_CLIPBOARD_MANAGER) == window)
    {
      xev.type = ClientMessage;
      xev.window = DefaultRootWindow (display);
      xev.message_type = XA_MANAGER;
      xev.format = 32;
      xev.data.l[0] = timestamp;
      xev.data.l[1] = XA_CLIPBOARD_MANAGER;
      xev.data.l[2] = window;
      xev.data.l[3] = 0;      /* manager specific data */
      xev.data.l[4] = 0;      /* manager specific data */

      XSendEvent (display, DefaultRootWindow (display), False,
                  StructureNotifyMask, (XEvent *)&xev);
    }
  else
    {
      xfce_clipboard_manager_stop (manager);
      return FALSE;
    }

  return TRUE;
}



void
xfce_clipboard_manager_stop (XfceClipboardManager *manager)
{
  g_return_if_fail (XFCE_IS_CLIPBOARD_MANAGER (manager));

  g_debug ("Stopping clipboard manager");

  g_signal_handlers_disconnect_by_func (manager->default_clipboard,
                                        xfce_clipboard_manager_default_owner_change,
                                        manager);
  g_signal_handlers_disconnect_by_func (manager->primary_clipboard,
                                        xfce_clipboard_manager_primary_owner_change,
                                        manager);
  gtk_widget_destroy (manager->window);

  if (manager->default_cache != NULL)
    {
      g_slist_foreach (manager->default_cache, (GFunc)gtk_selection_data_free, NULL);
      g_slist_free (manager->default_cache);
      manager->default_cache = NULL;
    }

  if (manager->primary_cache != NULL)
    g_free (manager->primary_cache);
}

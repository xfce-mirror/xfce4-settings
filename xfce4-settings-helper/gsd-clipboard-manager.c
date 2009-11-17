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

#include "gsd-clipboard-manager.h"

#define GSD_CLIPBOARD_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_CLIPBOARD_MANAGER, GsdClipboardManagerPrivate))

struct GsdClipboardManagerPrivate
{
        GtkClipboard *default_clipboard;
        GtkClipboard *primary_clipboard;

        GSList       *default_cache;
        gchar        *primary_cache;

        gboolean      internal_change;

        GtkWidget    *window;
};

static void     gsd_clipboard_manager_class_init  (GsdClipboardManagerClass *klass);
static void     gsd_clipboard_manager_init        (GsdClipboardManager      *clipboard_manager);
static void     gsd_clipboard_manager_finalize    (GObject                  *object);

G_DEFINE_TYPE (GsdClipboardManager, gsd_clipboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;


Atom XA_CLIPBOARD_MANAGER;
Atom XA_MANAGER;

static void
init_atoms (Display *display)
{
        static int _init_atoms = 0;

        if (_init_atoms > 0) {
                return;
        }

        XA_CLIPBOARD_MANAGER = XInternAtom (display, "CLIPBOARD_MANAGER", False);
        XA_MANAGER = XInternAtom (display, "MANAGER", False);

        _init_atoms = 1;
}


static void
default_clipboard_store (GsdClipboardManager *manager)
{
        GtkSelectionData *selection_data;
        GdkAtom          *atoms;
        gint              n_atoms;
        gint              i;

        if (!gtk_clipboard_wait_for_targets (manager->priv->default_clipboard, &atoms, &n_atoms)) {
                return;
        }

        if (manager->priv->default_cache != NULL) {
                g_slist_foreach (manager->priv->default_cache, (GFunc)gtk_selection_data_free, NULL);
                g_slist_free (manager->priv->default_cache);
                manager->priv->default_cache = NULL;
        }

        for (i = 0; i < n_atoms; i++) {
                if (atoms[i] == gdk_atom_intern_static_string ("TARGETS")
                    || atoms[i] == gdk_atom_intern_static_string ("MULTIPLE")
                    || atoms[i] == gdk_atom_intern_static_string ("DELETE")
                    || atoms[i] == gdk_atom_intern_static_string ("INSERT_PROPERTY")
                    || atoms[i] == gdk_atom_intern_static_string ("INSERT_SELECTION")
                    || atoms[i] == gdk_atom_intern_static_string ("PIXMAP")) {
                        continue;
                }

                selection_data = gtk_clipboard_wait_for_contents (manager->priv->default_clipboard, atoms[i]);
                if (selection_data == NULL) {
                        continue;
                }

                manager->priv->default_cache = g_slist_prepend (manager->priv->default_cache, selection_data);
        }
}

static void
default_clipboard_get_func (GtkClipboard *clipboard,
                            GtkSelectionData *selection_data,
                            guint info,
                            GsdClipboardManager *manager)
{
        GSList           *list;
        GtkSelectionData *selection_data_cache = NULL;

        list = manager->priv->default_cache;
        for (; list->next != NULL; list = list->next) {
                selection_data_cache = list->data;
                if (selection_data->target == selection_data_cache->target) {
                        break;
                }
                selection_data_cache = NULL;
        }
        if (selection_data_cache == NULL) {
                return;
        }
        gtk_selection_data_set (selection_data, selection_data->target,
                                selection_data_cache->format,
                                selection_data_cache->data,
                                selection_data_cache->length);
}

static void
default_clipboard_clear_func (GtkClipboard *clipboard,
                              GsdClipboardManager *manager)
{
        return;
}


static void
default_clipboard_restore (GsdClipboardManager *manager)
{
        GtkTargetList    *target_list;
        GtkTargetEntry   *targets;
        gint              n_targets;
        GtkSelectionData *sdata;
        GSList           *list;

        target_list = gtk_target_list_new (NULL, 0);
        list = manager->priv->default_cache;
        for (; list->next != NULL; list = list->next) {
                sdata = list->data;
                gtk_target_list_add (target_list, sdata->target, 0, 0);
        }
        targets = gtk_target_table_new_from_list (target_list, &n_targets);

        gtk_clipboard_set_with_data (manager->priv->default_clipboard,
                                     targets, n_targets,
                                     (GtkClipboardGetFunc)default_clipboard_get_func,
                                     (GtkClipboardClearFunc)default_clipboard_clear_func,
                                     manager);
}

static void
default_clipboard_owner_change (GsdClipboardManager *manager,
                                GdkEventOwnerChange *event)
{
        if (event->send_event == TRUE) {
                return;
        }

        if (event->owner != 0) {
                if (manager->priv->internal_change) {
                        manager->priv->internal_change = FALSE;
                        return;
                }
                default_clipboard_store (manager);
        }
        else {
                /* This 'bug' happens with Mozilla applications, it means that
                 * we restored the clipboard (we own it now) but somehow we are
                 * being noticed twice about that fact where first the owner is
                 * 0 (which is when we must restore) but then again where the
                 * owner is ourself (which is what normally only happens and
                 * also that means that we have to store the clipboard content
                 * e.g. owner is not 0). By the second time we would store
                 * ourself back with an empty clipboard... solution is to jump
                 * over the first time and don't try to restore empty data. */
                if (manager->priv->internal_change) {
                        return;
                }

                manager->priv->internal_change = TRUE;
                default_clipboard_restore (manager);
        }
}

static void
primary_clipboard_owner_change (GsdClipboardManager *manager,
                                GdkEventOwnerChange *event)
{
        gchar *text;

        if (event->send_event == TRUE) {
                return;
        }

        if (event->owner != 0) {
                text = gtk_clipboard_wait_for_text (manager->priv->primary_clipboard);
                if (text != NULL) {
                        g_free (manager->priv->primary_cache);
                        manager->priv->primary_cache = text;
                }
        }
        else {
                if (manager->priv->primary_cache != NULL) {
                        gtk_clipboard_set_text (manager->priv->primary_clipboard,
                                                manager->priv->primary_cache,
                                                -1);
                }
        }
}

static gboolean
start_clipboard_idle_cb (GsdClipboardManager *manager)
{
        XClientMessageEvent     xev;
        Display                *display;
        Window                  window;
        Time                    timestamp;

        display = GDK_DISPLAY ();
        init_atoms (display);

        /* Check if there is a clipboard manager running */
        if (gdk_display_supports_clipboard_persistence (gdk_display_get_default ())) {
                g_warning ("Clipboard manager is already running.");
                return FALSE;
        }

        manager->priv->window = gtk_invisible_new ();
        gtk_widget_realize (manager->priv->window);

        window = GDK_WINDOW_XID (manager->priv->window->window);
        timestamp = GDK_CURRENT_TIME;

        XSelectInput (display, window, PropertyChangeMask);
        XSetSelectionOwner (display, XA_CLIPBOARD_MANAGER, window, timestamp);

        g_signal_connect_swapped (manager->priv->default_clipboard, "owner-change",
                                  G_CALLBACK (default_clipboard_owner_change), manager);
        g_signal_connect_swapped (manager->priv->primary_clipboard, "owner-change",
                                  G_CALLBACK (primary_clipboard_owner_change), manager);

        /* Check to see if we managed to claim the selection. If not,
         * we treat it as if we got it then immediately lost it
         */
        if (XGetSelectionOwner (display, XA_CLIPBOARD_MANAGER) == window) {
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
        } else {
                gsd_clipboard_manager_stop (manager);
        }

        return FALSE;
}

gboolean
gsd_clipboard_manager_start (GsdClipboardManager *manager,
                             GError             **error)
{
        g_idle_add ((GSourceFunc) start_clipboard_idle_cb, manager);
        return TRUE;
}

void
gsd_clipboard_manager_stop (GsdClipboardManager *manager)
{
        g_debug ("Stopping clipboard manager");

        g_signal_handlers_disconnect_by_func (manager->priv->default_clipboard,
                                              default_clipboard_owner_change, manager);
        g_signal_handlers_disconnect_by_func (manager->priv->primary_clipboard,
                                              primary_clipboard_owner_change, manager);
        gtk_widget_destroy (manager->priv->window);

        if (manager->priv->default_cache != NULL) {
                g_slist_foreach (manager->priv->default_cache, (GFunc)gtk_selection_data_free, NULL);
                g_slist_free (manager->priv->default_cache);
                manager->priv->default_cache = NULL;
        }
        if (manager->priv->primary_cache != NULL) {
                g_free (manager->priv->primary_cache);
        }
}

static void
gsd_clipboard_manager_set_property (GObject        *object,
                                    guint           prop_id,
                                    const GValue   *value,
                                    GParamSpec     *pspec)
{
        GsdClipboardManager *self;

        self = GSD_CLIPBOARD_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_clipboard_manager_get_property (GObject        *object,
                                    guint           prop_id,
                                    GValue         *value,
                                    GParamSpec     *pspec)
{
        GsdClipboardManager *self;

        self = GSD_CLIPBOARD_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gsd_clipboard_manager_constructor (GType                  type,
                                   guint                  n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
        GsdClipboardManager      *clipboard_manager;
        GsdClipboardManagerClass *klass;

        klass = GSD_CLIPBOARD_MANAGER_CLASS (g_type_class_peek (GSD_TYPE_CLIPBOARD_MANAGER));

        clipboard_manager = GSD_CLIPBOARD_MANAGER (G_OBJECT_CLASS (gsd_clipboard_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (clipboard_manager);
}

static void
gsd_clipboard_manager_dispose (GObject *object)
{
        GsdClipboardManager *clipboard_manager;

        clipboard_manager = GSD_CLIPBOARD_MANAGER (object);

        G_OBJECT_CLASS (gsd_clipboard_manager_parent_class)->dispose (object);
}

static void
gsd_clipboard_manager_class_init (GsdClipboardManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gsd_clipboard_manager_get_property;
        object_class->set_property = gsd_clipboard_manager_set_property;
        object_class->constructor = gsd_clipboard_manager_constructor;
        object_class->dispose = gsd_clipboard_manager_dispose;
        object_class->finalize = gsd_clipboard_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdClipboardManagerPrivate));
}

static void
gsd_clipboard_manager_init (GsdClipboardManager *manager)
{
        manager->priv = GSD_CLIPBOARD_MANAGER_GET_PRIVATE (manager);

        manager->priv->default_clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
        manager->priv->primary_clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);

        manager->priv->default_cache = NULL;
        manager->priv->primary_cache = NULL;
}

static void
gsd_clipboard_manager_finalize (GObject *object)
{
        GsdClipboardManager *clipboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_CLIPBOARD_MANAGER (object));

        clipboard_manager = GSD_CLIPBOARD_MANAGER (object);

        g_return_if_fail (clipboard_manager->priv != NULL);

        G_OBJECT_CLASS (gsd_clipboard_manager_parent_class)->finalize (object);
}

GsdClipboardManager *
gsd_clipboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_CLIPBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_CLIPBOARD_MANAGER (manager_object);
}

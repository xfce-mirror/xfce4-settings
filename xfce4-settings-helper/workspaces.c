/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <dbus/dbus-glib.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libwnck/libwnck.h>

#include "workspaces.h"

#define WORKSPACES_CHANNEL    "xfwm4"
#define WORKSPACE_NAMES_PROP  "/general/workspace_names"
#define WORKSPACE_COUNT_PROP  "/general/workspace_count"

struct _XfceWorkspacesHelper
{
    GObject parent;

    WnckScreen **screens;
    XfconfChannel *channel;
};

typedef struct _XfceWorkspacesHelperClass
{
    GObjectClass parent;
} XfceWorkspacesHelperClass;

static void xfce_workspaces_helper_class_init(XfceWorkspacesHelperClass *klass);
static void xfce_workspaces_helper_init(XfceWorkspacesHelper *helper);
static void xfce_workspaces_helper_finalize(GObject *obj);

static void xfce_workspaces_helper_set_names_prop(XfceWorkspacesHelper *helper,
                                                  GdkScreen *screen,
                                                  gchar **names);
static void xfce_workspaces_helper_update_all_names(XfceWorkspacesHelper *helper);
static void xfce_workspaces_helper_prop_changed(XfconfChannel *channel,
                                                const gchar *property,
                                                const GValue *value,
                                                gpointer user_data);


G_DEFINE_TYPE(XfceWorkspacesHelper, xfce_workspaces_helper, G_TYPE_OBJECT)


static void
xfce_workspaces_helper_class_init(XfceWorkspacesHelperClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass *)klass;

    gobject_class->finalize = xfce_workspaces_helper_finalize;
}

static void
xfce_workspaces_helper_init(XfceWorkspacesHelper *helper)
{
    gint w, s, n_screens, n_workspaces;
    gchar **names;
    GdkDisplay *display;
    GdkScreen *screen;

    helper->channel = xfconf_channel_new(WORKSPACES_CHANNEL);
    names = xfconf_channel_get_string_list(helper->channel,
                                           WORKSPACE_NAMES_PROP);

    /* FIXME: need to do this for all screens? */
    display = gdk_display_get_default();
    n_screens = gdk_display_get_n_screens(display);
    helper->screens = g_new0(WnckScreen *, n_screens + 1);
    for(s = 0; s < n_screens; ++s) {
        helper->screens[s] = wnck_screen_get(s);
        wnck_screen_force_update(helper->screens[s]);

        if(s == 0) {
            /* here we assume that all screens will always have the same
             * number of workspaces, and any changes to this number will
             * occur at the same time.  this may not be a great assumption,
             * but it'll do. */
            g_signal_connect_swapped(G_OBJECT(helper->screens[s]), "workspace-created",
                                     G_CALLBACK(xfce_workspaces_helper_update_all_names),
                                     helper);
            g_signal_connect_swapped(G_OBJECT(helper->screens[s]), "workspace-destroyed",
                                     G_CALLBACK(xfce_workspaces_helper_update_all_names),
                                     helper);
        }

        n_workspaces = wnck_screen_get_workspace_count(helper->screens[s]);
        if(G_UNLIKELY(!names))
            names = g_new0(gchar *, n_workspaces + 1);
        else if(g_strv_length(names) < n_workspaces)
            names = g_renew(gchar *, names, n_workspaces + 1);

        for(w = g_strv_length(names); w < n_workspaces; ++w) {
            /* some of them may not have been set in xfconf */
            names[w] = g_strdup_printf(_("Workspace %d"), w + 1);
        }
        names[w] = NULL;

        screen = gdk_display_get_screen(display, s);
        xfce_workspaces_helper_set_names_prop(helper, screen, names);

        g_strfreev(names);
    }

    g_signal_connect(G_OBJECT(helper->channel),
                     "property-changed::" WORKSPACE_NAMES_PROP,
                     G_CALLBACK(xfce_workspaces_helper_prop_changed), helper);
}

static void
xfce_workspaces_helper_finalize(GObject *obj)
{
    XfceWorkspacesHelper *helper = XFCE_WORKSPACES_HELPER(obj);

    g_signal_handlers_disconnect_by_func(G_OBJECT(helper->channel),
                                         G_CALLBACK(xfce_workspaces_helper_prop_changed),
                                         helper);
    g_object_unref(G_OBJECT(helper->channel));

    g_signal_handlers_disconnect_by_func(G_OBJECT(helper->screens[0]),
                                         G_CALLBACK(xfce_workspaces_helper_update_all_names),
                                         helper);
    g_free(helper->screens);

    G_OBJECT_CLASS(xfce_workspaces_helper_parent_class)->finalize(obj);
}



static void
xfce_workspaces_helper_set_names_prop(XfceWorkspacesHelper *helper,
                                      GdkScreen *screen,
                                      gchar **names)
{
    GString *names_str;
    gint i;

    names_str = g_string_new(NULL);

    for(i = 0; names[i]; ++i)
        g_string_append_len(names_str, names[i], strlen(names[i]) + 1);

    gdk_error_trap_push();

    gdk_property_change(gdk_screen_get_root_window(screen),
                        gdk_atom_intern_static_string("_NET_DESKTOP_NAMES"),
                        gdk_atom_intern_static_string("UTF8_STRING"),
                        8, GDK_PROP_MODE_REPLACE,
                        (guchar *)names_str->str, names_str->len + 1);
    gdk_flush();

    gdk_error_trap_pop();

    g_string_free(names_str, TRUE);
}

static void
xfce_workspaces_helper_update_all_names(XfceWorkspacesHelper *helper)
{
    gint i, n_workspaces = wnck_screen_get_workspace_count(helper->screens[0]);
    gchar const **names;

    names = g_new0(gchar const *, n_workspaces + 1);

    for(i = 0; i < n_workspaces; ++i) {
        WnckWorkspace *space = wnck_screen_get_workspace(helper->screens[0], i);
        names[i] = wnck_workspace_get_name(space);
    }

    xfconf_channel_set_string_list(helper->channel, WORKSPACE_NAMES_PROP,
                                   (const gchar **)names);
    xfconf_channel_set_int(helper->channel, WORKSPACE_COUNT_PROP, n_workspaces);

    g_free(names);
}

static void
xfce_workspaces_helper_prop_changed(XfconfChannel *channel,
                                    const gchar *property,
                                    const GValue *value,
                                    gpointer user_data)
{
    XfceWorkspacesHelper *helper = user_data;
    GPtrArray *names_arr;
    gint s, i, n_workspaces;
    gchar **names;
    GdkDisplay *display;
    GdkScreen *screen;

    if(G_VALUE_TYPE(value) == G_TYPE_INVALID)
        return;

    if(G_VALUE_TYPE(value) != dbus_g_type_get_collection("GPtrArray",
                                                         G_TYPE_VALUE))
    {
        g_warning("(workspace names) Expected boxed GPtrArray property, got %s",
                  G_VALUE_TYPE_NAME(value));
        return;
    }

    names_arr = g_value_get_boxed(value);
    if(!names_arr)
        return;

    display = gdk_display_get_default();

    for(s = 0; helper->screens[s]; ++s) {
        wnck_screen_force_update(helper->screens[s]);
        n_workspaces = wnck_screen_get_workspace_count(helper->screens[s]);
        if(n_workspaces > names_arr->len)
            names = g_new0(gchar *, n_workspaces + 1);
        else {
            names = g_new0(gchar *, names_arr->len + 1);
            n_workspaces = names_arr->len;
        }

        for(i = 0; i < n_workspaces; ++i) {
            if(i < names_arr->len && G_VALUE_HOLDS_STRING(g_ptr_array_index(names_arr, i)))
                names[i] = g_value_dup_string(g_ptr_array_index(names_arr, i));
            else
                names[i] = g_strdup_printf(_("Workspace %d"), i + 1);
        }

        screen = gdk_display_get_screen(display, s);
        xfce_workspaces_helper_set_names_prop(helper, screen, names);
        g_strfreev(names);
    }
}

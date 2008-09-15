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


struct _XfceWorkspacesHelper
{
    GObject parent;

    WnckScreen *screen;
    XfconfChannel *channel;
};

typedef struct _XfceWorkspacesHelperClass
{
    GObjectClass parent;
} XfceWorkspacesHelperClass;

static void xfce_workspaces_helper_class_init(XfceWorkspacesHelperClass *klass);
static void xfce_workspaces_helper_init(XfceWorkspacesHelper *helper);
static void xfce_workspaces_helper_finalize(GObject *obj);

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
    WnckWorkspace *space;
    gint i, n_workspaces;
    gchar **names;

    helper->channel = xfconf_channel_new(WORKSPACES_CHANNEL);
    names = xfconf_channel_get_string_list(helper->channel,
                                           WORKSPACE_NAMES_PROP);

    /* FIXME: need to do this for all screens? */
    helper->screen = wnck_screen_get_default();
    wnck_screen_force_update(helper->screen);
    g_signal_connect_swapped(G_OBJECT(helper->screen), "workspace-created",
                             G_CALLBACK(xfce_workspaces_helper_update_all_names),
                             helper);
    g_signal_connect_swapped(G_OBJECT(helper->screen), "workspace-destroyed",
                             G_CALLBACK(xfce_workspaces_helper_update_all_names),
                             helper);

    n_workspaces = wnck_screen_get_workspace_count(helper->screen);
    i = 0;
    if(names) {
        for(; i < n_workspaces && names[i]; ++i) {
            space = wnck_screen_get_workspace(helper->screen, i);
            wnck_workspace_change_name(space, names[i]);
        }
    }
    if(i != n_workspaces) {
        /* some of them may not have been set in xfconf */
        names = g_realloc(names, sizeof(gchar *) * (n_workspaces + 1));
        for(; i < n_workspaces; ++i) {
            names[i] = g_strdup_printf(_("Workspace %d"), i + 1);
            space = wnck_screen_get_workspace(helper->screen, i);
            wnck_workspace_change_name(space, names[i]);
        }
        names[i] = NULL;
        xfconf_channel_set_string_list(helper->channel, WORKSPACE_NAMES_PROP,
                                       (const gchar **)names);
    }

    g_signal_connect(G_OBJECT(helper->channel),
                     "property-changed::" WORKSPACE_NAMES_PROP,
                     G_CALLBACK(xfce_workspaces_helper_prop_changed), helper);

    g_strfreev(names);
}

static void
xfce_workspaces_helper_finalize(GObject *obj)
{
    XfceWorkspacesHelper *helper = XFCE_WORKSPACES_HELPER(obj);

    g_signal_handlers_disconnect_by_func(G_OBJECT(helper->screen),
                                         G_CALLBACK(xfce_workspaces_helper_update_all_names),
                                         helper);

    g_signal_handlers_disconnect_by_func(G_OBJECT(helper->channel),
                                         G_CALLBACK(xfce_workspaces_helper_prop_changed),
                                         helper);
    g_object_unref(G_OBJECT(helper->channel));

    G_OBJECT_CLASS(xfce_workspaces_helper_parent_class)->finalize(obj);
}



static void
xfce_workspaces_helper_update_all_names(XfceWorkspacesHelper *helper)
{
    gint i, n_workspaces = wnck_screen_get_workspace_count(helper->screen);
    gchar const **names;

    names = g_malloc(sizeof(gchar *) * (n_workspaces + 1));

    for(i = 0; i < n_workspaces; ++i) {
        WnckWorkspace *space = wnck_screen_get_workspace(helper->screen, i);
        names[i] = wnck_workspace_get_name(space);
    }
    names[n_workspaces] = NULL;

    xfconf_channel_set_string_list(helper->channel, WORKSPACE_NAMES_PROP,
                                   (const gchar **)names);

    g_free(names);
}

static void
xfce_workspaces_helper_prop_changed(XfconfChannel *channel,
                                    const gchar *property,
                                    const GValue *value,
                                    gpointer user_data)
{
    XfceWorkspacesHelper *helper = user_data;
    GPtrArray *names;
    gint i, n_workspaces = wnck_screen_get_workspace_count(helper->screen);

    if(G_VALUE_TYPE(value) !=  dbus_g_type_get_collection("GPtrArray",
                                                          G_TYPE_VALUE))
    {
        g_warning("(workspace names) Expected boxed GPtrArray property, got %s",
                  G_VALUE_TYPE_NAME(value));
        return;
    }

    names = g_value_get_boxed(value);
    if(!names)
        return;

    for(i = 0; i < n_workspaces && i < names->len; ++i) {
        WnckWorkspace *space = wnck_screen_get_workspace(helper->screen, i);
        GValue *val = g_ptr_array_index(names, i);
        const gchar *old_name, *new_name;

        if(!G_VALUE_HOLDS_STRING(val)) {
            g_warning("(workspace names) Expected string but got %s for item %d",
                      G_VALUE_TYPE_NAME(val), i);
            continue;
        }

        /* only update the names that have actually changed */
        old_name = wnck_workspace_get_name(space);
        new_name = g_value_get_string(val);
        if(strcmp(old_name, new_name))
            wnck_workspace_change_name(space, new_name);
    }
}

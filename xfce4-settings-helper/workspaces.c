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
static void xfce_workspaces_helper_set_workspace_names(XfceWorkspacesHelper *helper);
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
    helper->channel = xfconf_channel_new(WORKSPACES_CHANNEL);

    xfce_workspaces_helper_set_workspace_names(helper);

    helper->screen = wnck_screen_get_default();
    g_signal_connect_swapped(G_OBJECT(helper->screen), "workspace-created",
                             G_CALLBACK(xfce_workspaces_helper_update_all_names),
                             helper);
    g_signal_connect_swapped(G_OBJECT(helper->screen), "workspace-destroyed",
                             G_CALLBACK(xfce_workspaces_helper_update_all_names),
                             helper);

    g_signal_connect(G_OBJECT(helper->channel),
                     "property-changed::" WORKSPACE_NAMES_PROP,
                     G_CALLBACK(xfce_workspaces_helper_prop_changed), helper);
}

static void
xfce_workspaces_helper_finalize(GObject *obj)
{
    XfceWorkspacesHelper *helper = XFCE_WORKSPACES_HELPER(obj);

    g_signal_handlers_disconnect_by_func(G_OBJECT(helper->screen),
                                         G_CALLBACK(xfce_workspaces_helper_update_all_names),
                                         helper);

    g_object_unref(G_OBJECT(helper->channel));

    G_OBJECT_CLASS(xfce_workspaces_helper_parent_class)->finalize(obj);
}

static void
xfce_workspaces_helper_set_workspace_names(XfceWorkspacesHelper *helper)
{
    WnckWorkspace *workspace;
    WnckScreen *screen;
    GList *li, *workspaces;
    guint i, n_names;
    gchar **names;
    gint n_screens, n;
    const gchar *name, *old_name;
    gchar *tmp_name;
    
    /* get the workspace names */
    names = xfconf_channel_get_string_list(helper->channel, WORKSPACE_NAMES_PROP);
    n_names = names ? g_strv_length(names) : 0;

    /* walk all the displays on this screen */
    /* FIXME? Is this really needed? */
    n_screens = gdk_display_get_n_screens(gdk_display_get_default());
    for(n = 0; n < n_screens; n++) {
        /* get the wnck screen and force an update */
        screen = wnck_screen_get(n);
        wnck_screen_force_update(screen);

        /* walk all the workspaces on this screen */
        workspaces = wnck_screen_get_workspaces(screen);
        for(li = workspaces, i = 0; li != NULL; li = li->next, i++) {
            workspace = WNCK_WORKSPACE(li->data);

            /* check if we have a valid name in the array */
            if(n_names > i && names[i] != NULL && names[i] != '\0') {
                name = names[i];
                tmp_name = NULL;
            } else {
                tmp_name = g_strdup_printf(_("Workspace %d"), i + 1);
                name = tmp_name;
            }

            /* update the workspace name if it has changed */
            old_name = wnck_workspace_get_name(workspace);
            if(old_name == NULL || strcmp(old_name, name) != 0)
                wnck_workspace_change_name(workspace, name);

            /* cleanup */
            g_free(tmp_name);
        }
    }
    
    /* cleanup */
    g_strfreev(names);
}

static void
xfce_workspaces_helper_update_all_names(XfceWorkspacesHelper *helper)
{
    gint i, n_workspaces;
    gchar const **names;
    WnckWorkspace *workspace;

    g_return_if_fail (XFCE_IS_WORKSPACES_HELPER (helper));

    n_workspaces = wnck_screen_get_workspace_count(helper->screen);
    names = g_new0(gchar const *, n_workspaces + 1);

    for(i = 0; i < n_workspaces; ++i) {
        workspace = wnck_screen_get_workspace(helper->screen, i);
        names[i] = wnck_workspace_get_name(workspace);
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
    g_return_if_fail (XFCE_IS_WORKSPACES_HELPER (user_data));
    xfce_workspaces_helper_set_workspace_names(XFCE_WORKSPACES_HELPER(user_data));
}

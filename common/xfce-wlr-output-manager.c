/*
 *  Copyright (c) 2023 Gaël Bonithon <gael@xfce.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "debug.h"
#include "xfce-wlr-output-manager.h"

#include <gdk/gdkwayland.h>



static void
xfce_wlr_output_manager_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec);
static void
xfce_wlr_output_manager_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec);
static void
xfce_wlr_output_manager_constructed (GObject *object);
static void
xfce_wlr_output_manager_finalize (GObject *object);

static void
registry_global (void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
static void
registry_global_remove (void *data, struct wl_registry *registry, uint32_t id);
static void
manager_head (void *data, struct zwlr_output_manager_v1 *wl_manager, struct zwlr_output_head_v1 *head);
static void
manager_done (void *data, struct zwlr_output_manager_v1 *wl_manager, uint32_t serial);
static void
manager_finished (void *data, struct zwlr_output_manager_v1 *wl_manager);
static void
head_name (void *data, struct zwlr_output_head_v1 *head, const char *name);
static void
head_description (void *data, struct zwlr_output_head_v1 *head, const char *description);
static void
head_physical_size (void *data, struct zwlr_output_head_v1 *head, int32_t width, int32_t height);
static void
head_mode (void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *wl_mode);
static void
head_enabled (void *data, struct zwlr_output_head_v1 *head, int32_t enabled);
static void
head_current_mode (void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *wl_mode);
static void
head_position (void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y);
static void
head_transform (void *data, struct zwlr_output_head_v1 *head, int32_t transform);
static void
head_scale (void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale);
static void
head_finished (void *data, struct zwlr_output_head_v1 *head);
static void
head_make (void *data, struct zwlr_output_head_v1 *head, const char *make);
static void
head_model (void *data, struct zwlr_output_head_v1 *head, const char *model);
static void
head_serial_number (void *data, struct zwlr_output_head_v1 *head, const char *serial_number);
static void
head_adaptive_sync (void *data, struct zwlr_output_head_v1 *head, uint32_t state);
static void
mode_size (void *data, struct zwlr_output_mode_v1 *wl_mode, int32_t width, int32_t height);
static void
mode_refresh (void *data, struct zwlr_output_mode_v1 *wl_mode, int32_t refresh);
static void
mode_preferred (void *data, struct zwlr_output_mode_v1 *wl_mode);
static void
mode_finished (void *data, struct zwlr_output_mode_v1 *wl_mode);


enum
{
    PROP_0,
    PROP_LISTENER,
    PROP_LISTENER_DATA,
};

typedef void (*_XfceWlrOutputListener) (void *data,
                                        struct zwlr_output_manager_v1 *wl_manager,
                                        uint32_t serial);

struct _XfceWlrOutputManager
{
    GObject __parent__;

    struct wl_registry *wl_registry;
    struct zwlr_output_manager_v1 *wl_manager;
    struct zwlr_output_manager_v1_listener *wl_listener;

    _XfceWlrOutputListener listener;
    gpointer listener_data;
    GPtrArray *outputs;
    gboolean initialized;
};

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static const struct zwlr_output_head_v1_listener head_listener = {
    .name = head_name,
    .description = head_description,
    .physical_size = head_physical_size,
    .mode = head_mode,
    .enabled = head_enabled,
    .current_mode = head_current_mode,
    .position = head_position,
    .transform = head_transform,
    .scale = head_scale,
    .finished = head_finished,
    .make = head_make,
    .model = head_model,
    .serial_number = head_serial_number,
    .adaptive_sync = head_adaptive_sync,
};

static const struct zwlr_output_mode_v1_listener mode_listener = {
    .size = mode_size,
    .refresh = mode_refresh,
    .preferred = mode_preferred,
    .finished = mode_finished,
};



G_DEFINE_TYPE (XfceWlrOutputManager, xfce_wlr_output_manager, G_TYPE_OBJECT);



static void
xfce_wlr_output_manager_class_init (XfceWlrOutputManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->get_property = xfce_wlr_output_manager_get_property;
    gobject_class->set_property = xfce_wlr_output_manager_set_property;
    gobject_class->constructed = xfce_wlr_output_manager_constructed;
    gobject_class->finalize = xfce_wlr_output_manager_finalize;

    g_object_class_install_property (gobject_class,
                                     PROP_LISTENER,
                                     g_param_spec_pointer ("listener",
                                                           NULL,
                                                           NULL,
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class,
                                     PROP_LISTENER_DATA,
                                     g_param_spec_pointer ("listener-data",
                                                           NULL,
                                                           NULL,
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}



static void
free_mode (gpointer data)
{
    XfceWlrMode *mode = data;
    zwlr_output_mode_v1_destroy (mode->wl_mode);
    g_free (mode);
}



static void
free_output (gpointer data)
{
    XfceWlrOutput *output = data;
    zwlr_output_head_v1_destroy (output->wl_head);
    g_free (output->name);
    g_free (output->description);
    g_list_free_full (output->modes, free_mode);
    g_free (output->manufacturer);
    g_free (output->model);
    g_free (output->serial_number);
    g_free (output->edid);
    g_free (output);
}



static void
xfce_wlr_output_manager_init (XfceWlrOutputManager *manager)
{
}



static void
xfce_wlr_output_manager_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
    XfceWlrOutputManager *manager = XFCE_WLR_OUTPUT_MANAGER (object);

    switch (prop_id)
    {
        case PROP_LISTENER:
            g_value_set_pointer (value, manager->listener);
            break;

        case PROP_LISTENER_DATA:
            g_value_set_pointer (value, manager->listener_data);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
xfce_wlr_output_manager_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
    XfceWlrOutputManager *manager = XFCE_WLR_OUTPUT_MANAGER (object);

    switch (prop_id)
    {
        case PROP_LISTENER:
            manager->listener = g_value_get_pointer (value);
            break;

        case PROP_LISTENER_DATA:
            manager->listener_data = g_value_get_pointer (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}



static void
output_make_edid (gpointer data,
                  gpointer user_data)
{
    XfceWlrOutput *output = data;
    gchar *edid_str = g_strdup_printf ("%s-%s-%s", output->serial_number, output->model, output->manufacturer);
    output->edid = g_compute_checksum_for_string (G_CHECKSUM_SHA1, edid_str, -1);
    g_free (edid_str);
}



static void
xfce_wlr_output_manager_constructed (GObject *object)
{
    XfceWlrOutputManager *manager = XFCE_WLR_OUTPUT_MANAGER (object);
    struct wl_display *wl_display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());

    manager->outputs = g_ptr_array_new_with_free_func (free_output);
    manager->wl_registry = wl_display_get_registry (wl_display);
    wl_registry_add_listener (manager->wl_registry, &registry_listener, manager);
    wl_display_roundtrip (wl_display);
    if (manager->wl_manager != NULL)
    {
        manager->wl_listener = g_new (struct zwlr_output_manager_v1_listener, 1);
        manager->wl_listener->head = manager_head;
        manager->wl_listener->done = manager->listener != NULL ? manager->listener : manager_done;
        manager->wl_listener->finished = manager_finished;
        zwlr_output_manager_v1_add_listener (manager->wl_manager, manager->wl_listener, manager);
        wl_display_roundtrip (wl_display);
        g_ptr_array_foreach (manager->outputs, output_make_edid, NULL);
        manager->initialized = TRUE;
    }
    else
    {
        g_ptr_array_unref (manager->outputs);
        manager->outputs = NULL;
        g_warning ("Your compositor does not seem to support the wlr-output-management protocol:"
                   " display settings won't work");
    }

    G_OBJECT_CLASS (xfce_wlr_output_manager_parent_class)->constructed (object);
}



static void
xfce_wlr_output_manager_finalize (GObject *object)
{
    XfceWlrOutputManager *manager = XFCE_WLR_OUTPUT_MANAGER (object);

    if (manager->wl_manager != NULL)
    {
        zwlr_output_manager_v1_destroy (manager->wl_manager);
        g_free (manager->wl_listener);
        g_ptr_array_unref (manager->outputs);
    }
    wl_registry_destroy (manager->wl_registry);

    G_OBJECT_CLASS (xfce_wlr_output_manager_parent_class)->finalize (object);
}



static void
registry_global (void *data,
                 struct wl_registry *registry,
                 uint32_t id,
                 const char *interface,
                 uint32_t version)
{
    XfceWlrOutputManager *manager = data;

    if (g_strcmp0 (zwlr_output_manager_v1_interface.name, interface) == 0)
        manager->wl_manager = wl_registry_bind (manager->wl_registry, id, &zwlr_output_manager_v1_interface,
                                                MIN ((uint32_t) zwlr_output_manager_v1_interface.version, version));
}



static void
registry_global_remove (void *data,
                        struct wl_registry *registry,
                        uint32_t id)
{
}



static void
manager_head (void *data,
              struct zwlr_output_manager_v1 *wl_manager,
              struct zwlr_output_head_v1 *head)
{
    XfceWlrOutputManager *manager = data;
    XfceWlrOutput *output = g_new0 (XfceWlrOutput, 1);

    output->wl_head = head;
    output->new = TRUE;
    output->scale = wl_fixed_from_double (1.0);
    g_ptr_array_add (manager->outputs, output);
    zwlr_output_head_v1_add_listener (head, &head_listener, output);
    if (manager->initialized)
    {
        wl_display_roundtrip (gdk_wayland_display_get_wl_display (gdk_display_get_default ()));
        output_make_edid (output, NULL);
    }
}



static void
manager_done (void *data,
              struct zwlr_output_manager_v1 *wl_manager,
              uint32_t serial)
{
}



static void
manager_finished (void *data,
                  struct zwlr_output_manager_v1 *wl_manager)
{
    XfceWlrOutputManager *manager = data;
    zwlr_output_manager_v1_destroy (manager->wl_manager);
    manager->wl_manager = NULL;
}



static void
head_name (void *data,
           struct zwlr_output_head_v1 *head,
           const char *name)
{
    XfceWlrOutput *output = data;
    g_free (output->name);
    output->name = g_strdup (name);
}



static void
head_description (void *data,
                  struct zwlr_output_head_v1 *head,
                  const char *description)
{
    XfceWlrOutput *output = data;
    g_free (output->description);
    output->description = g_strdup (description);
}



static void
head_physical_size (void *data,
                    struct zwlr_output_head_v1 *head,
                    int32_t width,
                    int32_t height)
{
    XfceWlrOutput *output = data;
    output->width = width;
    output->height = height;
}



static void
head_mode (void *data,
           struct zwlr_output_head_v1 *head,
           struct zwlr_output_mode_v1 *wl_mode)
{
    XfceWlrOutput *output = data;
    XfceWlrMode *mode = g_new0 (XfceWlrMode, 1);

    mode->wl_mode = wl_mode;
    mode->output = output;
    output->modes = g_list_append (output->modes, mode);
    zwlr_output_mode_v1_add_listener (wl_mode, &mode_listener, mode);
}



static void
head_enabled (void *data,
              struct zwlr_output_head_v1 *head,
              int32_t enabled)
{
    XfceWlrOutput *output = data;
    output->enabled = enabled;
}



static void
head_current_mode (void *data,
                   struct zwlr_output_head_v1 *head,
                   struct zwlr_output_mode_v1 *wl_mode)
{
    XfceWlrOutput *output = data;
    output->wl_mode = wl_mode;
}



static void
head_position (void *data,
               struct zwlr_output_head_v1 *head,
               int32_t x,
               int32_t y)
{
    XfceWlrOutput *output = data;
    output->x = x;
    output->y = y;
}



static void
head_transform (void *data,
                struct zwlr_output_head_v1 *head,
                int32_t transform)
{
    XfceWlrOutput *output = data;
    output->transform = transform;
}



static void
head_scale (void *data,
            struct zwlr_output_head_v1 *head,
            wl_fixed_t scale)
{
    XfceWlrOutput *output = data;
    output->scale = scale;
}



static void
head_finished (void *data,
               struct zwlr_output_head_v1 *head)
{
    XfceWlrOutput *output = data;
    xfsettings_dbg (XFSD_DEBUG_DISPLAYS, "Output disconnected: %s", output->name);
    g_ptr_array_remove (output->manager->outputs, output);
}



static void
head_make (void *data,
           struct zwlr_output_head_v1 *head,
           const char *make)
{
    XfceWlrOutput *output = data;
    g_free (output->manufacturer);
    output->manufacturer = g_strdup (make);
}



static void
head_model (void *data,
            struct zwlr_output_head_v1 *head,
            const char *model)
{
    XfceWlrOutput *output = data;
    g_free (output->model);
    output->model = g_strdup (model);
}



static void
head_serial_number (void *data,
                    struct zwlr_output_head_v1 *head,
                    const char *serial_number)
{
    XfceWlrOutput *output = data;
    g_free (output->serial_number);
    output->serial_number = g_strdup (serial_number);
}



static void
head_adaptive_sync (void *data,
                    struct zwlr_output_head_v1 *head,
                    uint32_t state)
{
    XfceWlrOutput *output = data;
    output->adaptive_sync = state;
}



static void
mode_size (void *data,
           struct zwlr_output_mode_v1 *wl_mode,
           int32_t width,
           int32_t height)
{
    XfceWlrMode *mode = data;
    mode->width = width;
    mode->height = height;
}



static void
mode_refresh (void *data,
              struct zwlr_output_mode_v1 *wl_mode,
              int32_t refresh)
{
    XfceWlrMode *mode = data;
    mode->refresh = refresh;
}



static void
mode_preferred (void *data,
                struct zwlr_output_mode_v1 *wl_mode)
{
    XfceWlrMode *mode = data;
    mode->preferred = TRUE;
}



static void
mode_finished (void *data,
               struct zwlr_output_mode_v1 *wl_mode)
{
    XfceWlrMode *mode = data;
    mode->output->modes = g_list_remove (mode->output->modes, mode);
    free_mode (mode);
}



XfceWlrOutputManager *
xfce_wlr_output_manager_new (XfceWlrOutputListener listener,
                             gpointer listener_data)
{
    return g_object_new (XFCE_TYPE_WLR_OUTPUT_MANAGER,
                         "listener", listener,
                         "listener-data", listener_data,
                         NULL);
}



gpointer
xfce_wlr_output_manager_get_listener_data (XfceWlrOutputManager *manager)
{
    g_return_val_if_fail (XFCE_IS_WLR_OUTPUT_MANAGER (manager), NULL);
    return manager->listener_data;
}



struct zwlr_output_manager_v1 *
xfce_wlr_output_manager_get_wl_manager (XfceWlrOutputManager *manager)
{
    g_return_val_if_fail (XFCE_IS_WLR_OUTPUT_MANAGER (manager), NULL);
    return manager->wl_manager;
}



GPtrArray *
xfce_wlr_output_manager_get_outputs (XfceWlrOutputManager *manager)
{
    g_return_val_if_fail (XFCE_IS_WLR_OUTPUT_MANAGER (manager), NULL);
    return manager->outputs;
}



gchar **
xfce_wlr_output_manager_get_display_infos (XfceWlrOutputManager *manager)
{
    gchar **display_infos;

    g_return_val_if_fail (XFCE_IS_WLR_OUTPUT_MANAGER (manager), NULL);

    display_infos = g_new0 (gchar *, manager->outputs->len + 1);
    for (guint n = 0; n < manager->outputs->len; n++)
    {
        XfceWlrOutput *output = g_ptr_array_index (manager->outputs, n);
        display_infos[n] = g_strdup (output->edid);
    }

    return display_infos;
}

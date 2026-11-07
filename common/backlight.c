/*
 *  Copyright (c) 2026 Grigory Tuboltsev <gtubolcev@gmail.com>
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

#include "backlight.h"

#if defined(__linux__)
#include <gio/gio.h>
#elif defined(HAVE_SYS_BACKLIGHT_H)
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/backlight.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif



#if defined(__linux__)

#define BACKLIGHT_LINUX_SYSFS_DIR "/sys/class/backlight"

static guint64
backlight_read_value (const gchar *device,
                      const gchar *file)
{
    gchar *path = g_build_filename (BACKLIGHT_LINUX_SYSFS_DIR, device, file, NULL);
    gchar *contents = NULL;
    guint64 value = 0;

    if (g_file_get_contents (path, &contents, NULL, NULL))
        value = g_ascii_strtoull (contents, NULL, 10);

    g_free (path);
    g_free (contents);

    return value;
}



gchar *
backlight_find_device (void)
{
    /* A laptop may have several backlight devices for the same screen. The
     * kernel tells their kind in the file "type": "firmware" (ACPI) is the
     * most reliable, then "platform" (vendor drivers), then "raw" (the GPU
     * register). Like GNOME and xfce4-power-manager, take the first kind in
     * this order that has a device; a device of an unknown kind is not used. */
    static const gchar *types[] = { "firmware", "platform", "raw" };
    GDir *dir = g_dir_open (BACKLIGHT_LINUX_SYSFS_DIR, 0, NULL);
    const gchar *entry;
    gchar *best = NULL;
    guint best_index = G_N_ELEMENTS (types); /* index in types[] of the best device so far */

    if (dir == NULL)
        return NULL;

    while ((entry = g_dir_read_name (dir)) != NULL)
    {
        gchar *path = g_build_filename (BACKLIGHT_LINUX_SYSFS_DIR, entry, "type", NULL);
        gchar *type = NULL;
        guint index = G_N_ELEMENTS (types);

        if (g_file_get_contents (path, &type, NULL, NULL))
        {
            g_strstrip (type);
            for (guint i = 0; i < G_N_ELEMENTS (types); i++)
                if (g_strcmp0 (type, types[i]) == 0)
                    index = i;
        }

        if (index < best_index && backlight_read_value (entry, "max_brightness") > 0)
        {
            g_set_str (&best, entry);
            best_index = index;
        }

        g_free (path);
        g_free (type);
    }

    g_dir_close (dir);

    return best;
}



gint
backlight_get_percent (const gchar *device)
{
    guint64 max = backlight_read_value (device, "max_brightness");

    if (max == 0)
        return -1;

    return (backlight_read_value (device, "brightness") * 100 + max / 2) / max;
}



static void
backlight_set_percent_done (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
    GVariant *ret;
    GError *error = NULL;

    ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
    if (ret != NULL)
        g_variant_unref (ret);
    else
    {
        g_warning ("Failed to set the brightness: %s", error->message);
        g_error_free (error);
    }
}



void
backlight_set_percent (const gchar *device,
                       guint percent)
{
    GDBusConnection *bus;
    GError *error = NULL;
    guint64 max, value;

    max = backlight_read_value (device, "max_brightness");
    if (max == 0)
    {
        g_warning ("Failed to set the brightness: cannot read the maximum of %s", device);
        return;
    }

    bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (bus == NULL)
    {
        g_warning ("Failed to set the brightness: %s", error->message);
        g_error_free (error);
        return;
    }

    /* never switch the backlight off completely */
    value = MAX ((max * percent + 50) / 100, 1);
    g_dbus_connection_call (bus, "org.freedesktop.login1", "/org/freedesktop/login1/session/auto",
                            "org.freedesktop.login1.Session", "SetBrightness",
                            g_variant_new ("(ssu)", "backlight", device, (guint32) value),
                            NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                            backlight_set_percent_done, NULL);
    g_object_unref (bus);
}



#elif defined(HAVE_SYS_BACKLIGHT_H)

#define BACKLIGHT_FREEBSD_DEV_DIR "/dev/backlight"

/* opens /dev/backlight/@device, or returns -1 */
static gint
backlight_open (const gchar *device)
{
    gchar *path = g_build_filename (BACKLIGHT_FREEBSD_DEV_DIR, device, NULL);
    gint fd = open (path, O_RDWR | O_CLOEXEC);

    g_free (path);

    return fd;
}



gchar *
backlight_find_device (void)
{
    GDir *dir = g_dir_open (BACKLIGHT_FREEBSD_DEV_DIR, 0, NULL);
    const gchar *entry;
    gchar *best = NULL;

    if (dir == NULL)
        return NULL;

    /* each device appears twice, as "backlightN" and under the name of its
     * driver, e.g. "intel_backlight0"; look only at the former, and take the
     * first screen backlight (not a keyboard one) that we are allowed to open */
    while ((entry = g_dir_read_name (dir)) != NULL)
    {
        struct backlight_info info;
        gint fd;

        if (!g_str_has_prefix (entry, "backlight")
            || (best != NULL && strcmp (entry, best) > 0))
            continue;

        fd = backlight_open (entry);
        if (fd < 0)
            continue;

        memset (&info, 0, sizeof (info));
        if (ioctl (fd, BACKLIGHTGETINFO, &info) == 0 && info.type == BACKLIGHT_TYPE_PANEL)
            g_set_str (&best, entry);

        close (fd);
    }

    g_dir_close (dir);

    return best;
}



gint
backlight_get_percent (const gchar *device)
{
    struct backlight_props props;
    gint fd = backlight_open (device);
    gint percent = -1;

    if (fd < 0)
        return -1;

    /* the kernel reports the brightness in percent already */
    memset (&props, 0, sizeof (props));
    if (ioctl (fd, BACKLIGHTGETSTATUS, &props) == 0)
        percent = MIN (props.brightness, 100);

    close (fd);

    return percent;
}



void
backlight_set_percent (const gchar *device,
                       guint percent)
{
    struct backlight_props props;
    gint fd = backlight_open (device);

    if (fd < 0)
    {
        g_warning ("Failed to set the brightness: cannot open %s: %s", device, g_strerror (errno));
        return;
    }

    /* read the status first: the kernel skips an update to the brightness
     * it saw last, which may be outdated if it was changed by a hotkey */
    memset (&props, 0, sizeof (props));
    if (ioctl (fd, BACKLIGHTGETSTATUS, &props) == 0)
    {
        props.brightness = CLAMP (percent, 1, 100);
        if (ioctl (fd, BACKLIGHTUPDATESTATUS, &props) != 0)
            g_warning ("Failed to set the brightness of %s: %s", device, g_strerror (errno));
    }
    else
        g_warning ("Failed to read the brightness of %s: %s", device, g_strerror (errno));

    close (fd);
}



#else /* neither Linux nor FreeBSD */

gchar *
backlight_find_device (void)
{
    return NULL;
}



gint
backlight_get_percent (const gchar *device)
{
    return -1;
}



void
backlight_set_percent (const gchar *device,
                       guint percent)
{
}

#endif

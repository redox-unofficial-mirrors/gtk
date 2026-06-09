/*
 * Copyright © 2016 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>
#include "gdkprivate-orbital.h"

#include "gdkmonitor-orbital.h"

G_DEFINE_TYPE (GdkOrbitalMonitor, gdk_orbital_monitor, GDK_TYPE_MONITOR)

static void
gdk_orbital_monitor_init (GdkOrbitalMonitor *monitor)
{
}

static void
gdk_orbital_monitor_finalize (GObject *object)
{
  GdkOrbitalMonitor *monitor = (GdkOrbitalMonitor *)object;

  g_free (monitor->name);

  G_OBJECT_CLASS (gdk_orbital_monitor_parent_class)->finalize (object);
}

static void
gdk_orbital_monitor_class_init (GdkOrbitalMonitorClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gdk_orbital_monitor_finalize;
}

/**
 * gdk_orbital_monitor_get_wl_output:
 * @monitor: (type GdkOrbitalMonitor): a #GdkMonitor
 *
 * Returns the Orbital wl_output of a #GdkMonitor.
 *
 * Returns: (transfer none): a Orbital wl_output
 * Since: 3.22
 */
struct wl_output *
gdk_orbital_monitor_get_wl_output (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_ORBITAL_MONITOR (monitor), NULL);

  return GDK_ORBITAL_MONITOR (monitor)->output;
}

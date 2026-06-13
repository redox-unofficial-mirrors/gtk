/*
 * gdkdisplay-orbital.h
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#ifndef __GDK_ORBITAL_DISPLAY__
#define __GDK_ORBITAL_DISPLAY__

#include <config.h>
#include <stdint.h>
#include <orbital.h>

#include <glib.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdk.h>		/* For gdk_get_program_class() */

#include "gdkdisplayprivate.h"

#include <epoxy/egl.h>

G_BEGIN_DECLS

#define GDK_ORBITAL_MAX_THEME_SCALE 4
#define GDK_ORBITAL_THEME_SCALES_COUNT GDK_ORBITAL_MAX_THEME_SCALE

#define GDK_ZWP_POINTER_GESTURES_V1_VERSION 1

typedef struct _GdkOrbitalSelection GdkOrbitalSelection;

typedef enum _GdkOrbitalShellVariant
{
  GDK_ORBITAL_SHELL_VARIANT_XDG_SHELL,
} GdkOrbitalShellVariant;

struct _GdkOrbitalDisplay
{
  GdkDisplay parent_instance;
  GdkScreen *screen;

  /* Startup notification */
  gchar *startup_notification_id;

  /* Most recent serial */
  guint32 serial;

  GdkOrbitalShellVariant shell_variant;

  /* TODO: Orbital egl in mesa uses wl_display as placeholder */
  struct wl_display *orb_display;
  struct gtk_shell1 *gtk_shell;

  /* Keep track of the ID's of the known globals and their corresponding
   * names. This way we can check whether an interface is known, and
   * remove globals given its ID. This table is not expected to be very
   * large, meaning the lookup by interface name time is insignificant. */
  GHashTable *known_globals;

  GHashTable *cursor_cache;

  GSource *event_source;

  int gtk_shell_version;

  struct xkb_context *xkb_context;

  GdkOrbitalSelection *selection;

  GPtrArray *monitors;

  /* egl info */
  EGLDisplay egl_display;
  int egl_major_version;
  int egl_minor_version;

  guint have_egl : 1;
  guint have_egl_khr_create_context : 1;
  guint have_egl_buffer_age : 1;
  guint have_egl_swap_buffers_with_damage : 1;
  guint have_egl_surfaceless_context : 1;
  EGLint egl_min_swap_interval;
};

struct _GdkOrbitalDisplayClass
{
  GdkDisplayClass parent_class;
};

G_END_DECLS

#endif  /* __GDK_ORBITAL_DISPLAY__ */

/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2026 Redox OS Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define GDK_PIXBUF_ENABLE_BACKEND

#include <string.h>

#include "gdkprivate-orbital.h"
#include "gdkcursorprivate.h"
#include "gdkdisplay-orbital.h"
#include "gdkorbital.h"

#include "orbital.h"

#define GDK_TYPE_ORBITAL_CURSOR              (_gdk_orbital_cursor_get_type ())
#define GDK_ORBITAL_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ORBITAL_CURSOR, GdkOrbitalCursor))
#define GDK_ORBITAL_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_ORBITAL_CURSOR, GdkOrbitalCursorClass))
#define GDK_IS_ORBITAL_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ORBITAL_CURSOR))
#define GDK_IS_ORBITAL_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_ORBITAL_CURSOR))
#define GDK_ORBITAL_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_ORBITAL_CURSOR, GdkOrbitalCursorClass))

typedef struct _GdkOrbitalCursor GdkOrbitalCursor;
typedef struct _GdkOrbitalCursorClass GdkOrbitalCursorClass;

struct _GdkOrbitalCursor
{
  GdkCursor cursor;
  gchar *name;
  int scale;
};

struct _GdkOrbitalCursorClass
{
  GdkCursorClass cursor_class;
};

GType _gdk_orbital_cursor_get_type (void);

G_DEFINE_TYPE (GdkOrbitalCursor, _gdk_orbital_cursor, GDK_TYPE_CURSOR)

void
_gdk_orbital_display_init_cursors (GdkOrbitalDisplay *display)
{
  display->cursor_cache = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

void
_gdk_orbital_display_finalize_cursors (GdkOrbitalDisplay *display)
{
  g_hash_table_destroy (display->cursor_cache);
}

void
_gdk_orbital_display_update_cursors (GdkOrbitalDisplay *display)
{
  /* Orbital cursors are drawn by the compositor, nothing to update */
}

static void
gdk_orbital_cursor_finalize (GObject *object)
{
  GdkOrbitalCursor *cursor = GDK_ORBITAL_CURSOR (object);

  g_free (cursor->name);

  G_OBJECT_CLASS (_gdk_orbital_cursor_parent_class)->finalize (object);
}

static cairo_surface_t *
gdk_orbital_cursor_get_surface (GdkCursor *cursor,
                                gdouble   *x_hot,
                                gdouble   *y_hot)
{
  if (x_hot) *x_hot = 0;
  if (y_hot) *y_hot = 0;
  return NULL;
}

void
_gdk_orbital_cursor_set_scale (GdkCursor *cursor,
                               guint      scale)
{
  GdkOrbitalCursor *orbital_cursor = GDK_ORBITAL_CURSOR (cursor);
  orbital_cursor->scale = scale;
}

static void
_gdk_orbital_cursor_class_init (GdkOrbitalCursorClass *orbital_cursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (orbital_cursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (orbital_cursor_class);

  object_class->finalize = gdk_orbital_cursor_finalize;
  cursor_class->get_surface = gdk_orbital_cursor_get_surface;
}

static void
_gdk_orbital_cursor_init (GdkOrbitalCursor *cursor)
{
  cursor->scale = 1;
}

GdkCursor *
_gdk_orbital_display_get_cursor_for_name_with_scale (GdkDisplay  *display,
                                                     const gchar *name,
                                                     guint        scale)
{
  GdkOrbitalCursor *orbital_cursor;
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  orbital_cursor = g_hash_table_lookup (display_orbital->cursor_cache, name);
  if (orbital_cursor && orbital_cursor->scale == scale)
    return GDK_CURSOR (g_object_ref (orbital_cursor));

  orbital_cursor = g_object_new (GDK_TYPE_ORBITAL_CURSOR,
                                 "cursor-type", GDK_CURSOR_IS_PIXMAP,
                                 "display", display,
                                 NULL);

  /* Blank cursor case */
  if (!name || g_str_equal (name, "none") || g_str_equal (name, "blank_cursor"))
    orbital_cursor->name = g_strdup ("none");
  else
    orbital_cursor->name = g_strdup (name);

  orbital_cursor->scale = scale;

  /* Insert into cache. */
  g_hash_table_replace (display_orbital->cursor_cache,
                        orbital_cursor->name,
                        g_object_ref (orbital_cursor));

  return GDK_CURSOR (orbital_cursor);
}

GdkCursor *
_gdk_orbital_display_get_cursor_for_name (GdkDisplay  *display,
                                          const gchar *name)
{
  return _gdk_orbital_display_get_cursor_for_name_with_scale (display, name, 1);
}

GdkCursor *
_gdk_orbital_display_get_cursor_for_type_with_scale (GdkDisplay    *display,
                                                     GdkCursorType  cursor_type,
                                                     guint          scale)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  gchar *cursor_name;
  GdkCursor *result;

  enum_class = g_type_class_ref (GDK_TYPE_CURSOR_TYPE);
  enum_value = g_enum_get_value (enum_class, cursor_type);
  cursor_name = g_strdup (enum_value->value_nick);
  g_strdelimit (cursor_name, "-", '_');
  g_type_class_unref (enum_class);

  result = _gdk_orbital_display_get_cursor_for_name_with_scale (display,
                                                                cursor_name,
                                                                scale);
  g_free (cursor_name);
  return result;
}

GdkCursor *
_gdk_orbital_display_get_cursor_for_type (GdkDisplay    *display,
                                          GdkCursorType  cursor_type)
{
  return _gdk_orbital_display_get_cursor_for_type_with_scale (display,
                                                              cursor_type,
                                                              1);
}

GdkCursor *
_gdk_orbital_display_get_cursor_for_surface (GdkDisplay      *display,
                                             cairo_surface_t *surface,
                                             gdouble          x,
                                             gdouble          y)
{
  GdkOrbitalCursor *cursor;
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);

  cursor = g_object_new (GDK_TYPE_ORBITAL_CURSOR,
                         "cursor-type", GDK_CURSOR_IS_PIXMAP,
                         "display", display_orbital,
                         NULL);
                         
  cursor->name = g_strdup("custom");
  cursor->scale = 1;

  return GDK_CURSOR (cursor);
}

void
_gdk_orbital_display_get_default_cursor_size (GdkDisplay *display,
                                              guint       *width,
                                              guint       *height)
{
  *width = 32;
  *height = 32;
}

void
_gdk_orbital_display_get_maximal_cursor_size (GdkDisplay *display,
                                              guint       *width,
                                              guint       *height)
{
  *width = 256;
  *height = 256;
}

gboolean
_gdk_orbital_display_supports_cursor_alpha (GdkDisplay *display)
{
  return FALSE;
}

gboolean
_gdk_orbital_display_supports_cursor_color (GdkDisplay *display)
{
  return FALSE;
}
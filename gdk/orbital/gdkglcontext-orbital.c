/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-orbital.c: Orbital specific OpenGL wrappers
 *
 * Copyright © 2014  Emmanuele Bassi
 * Copyright © 2014  Alexander Larsson
 * Copyright © 2026  Redox OS Contributors
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

#include "gdkglcontext-orbital.h"
#include "gdkdisplay-orbital.h"

#include "gdkorbitaldisplay.h"
#include "gdkorbitalglcontext.h"
#include "gdkorbitalwindow.h"
#include "gdkprivate-orbital.h"

#include "gdkinternals.h"
#include "gdkintl.h"

#include "orbital.h"

G_DEFINE_TYPE (GdkOrbitalGLContext, gdk_orbital_gl_context, GDK_TYPE_GL_CONTEXT)

static void
gdk_orbital_gl_context_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (gdk_orbital_gl_context_parent_class)->dispose (gobject);
}

void
gdk_orbital_window_invalidate_for_new_frame (GdkWindow      *window,
                                             cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;

  window_rect.x = 0;
  window_rect.y = 0;
  window_rect.width = gdk_window_get_width (window);
  window_rect.height = gdk_window_get_height (window);

  cairo_region_union_rectangle (update_area, &window_rect);
}

static gboolean
gdk_orbital_gl_context_realize (GdkGLContext *context,
                                GError      **error)
{
  g_set_error_literal (error, GDK_GL_ERROR,
                       GDK_GL_ERROR_NOT_AVAILABLE,
                       _("No GL Orbital"));
  return FALSE;
}

static void
gdk_orbital_gl_context_end_frame (GdkGLContext   *context,
                                  cairo_region_t *painted,
                                  cairo_region_t *damage)
{
}

static void
gdk_orbital_gl_context_class_init (GdkOrbitalGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_orbital_gl_context_dispose;

  context_class->realize = gdk_orbital_gl_context_realize;
  context_class->end_frame = gdk_orbital_gl_context_end_frame;
}

static void
gdk_orbital_gl_context_init (GdkOrbitalGLContext *self)
{
}

gboolean
gdk_orbital_display_init_gl (GdkDisplay *display,
                             GdkGLContext *share)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);

  /* TODO */
  display_orbital->have_egl = FALSE;
  return FALSE;
}

GdkGLContext *
gdk_orbital_window_create_gl_context (GdkWindow     *window,
                                      gboolean       attached,
                                      GdkGLContext  *share,
                                      GError       **error)
{
  g_set_error_literal (error, GDK_GL_ERROR,
                       GDK_GL_ERROR_NOT_AVAILABLE,
                       _("TODO EGL Orbital"));
  return NULL;
}

gboolean
gdk_orbital_display_make_gl_context_current (GdkDisplay   *display,
                                             GdkGLContext *context)
{
  return FALSE;
}

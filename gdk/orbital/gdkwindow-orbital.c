/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2026 Redox OS Contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gdk.h"
#include "gdkorbital.h"
#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkdisplay-orbital.h"
#include "gdkframeclockprivate.h"
#include "gdkprivate-orbital.h"
#include "gdkglcontext-orbital.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"

#include "orbital.h"

typedef struct _GdkOrbitalWindow GdkOrbitalWindow;
typedef struct _GdkOrbitalWindowClass GdkOrbitalWindowClass;

#define GDK_TYPE_WINDOW_IMPL_ORBITAL              (_gdk_window_impl_orbital_get_type ())
#define GDK_WINDOW_IMPL_ORBITAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_ORBITAL, GdkWindowImplOrbital))
#define GDK_WINDOW_IMPL_ORBITAL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_ORBITAL, GdkWindowImplOrbitalClass))
#define GDK_IS_WINDOW_IMPL_ORBITAL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_ORBITAL))
#define GDK_IS_WINDOW_IMPL_ORBITAL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_ORBITAL))


typedef struct _GdkWindowImplOrbital GdkWindowImplOrbital;
typedef struct _GdkWindowImplOrbitalClass GdkWindowImplOrbitalClass;

struct _GdkOrbitalWindow
{
  GdkWindow parent;
};

struct _GdkOrbitalWindowClass
{
  GdkWindowClass parent_class;
};

static void gdk_orbital_window_class_init (GdkOrbitalWindowClass *klass) {}
static void gdk_orbital_window_init (GdkOrbitalWindow *orbital_window) {}

G_DEFINE_TYPE (GdkOrbitalWindow, gdk_orbital_window, GDK_TYPE_WINDOW)

struct _GdkWindowImplOrbital
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;
  void *orb_window;

  cairo_surface_t *cairo_surface;

  gchar *title;
  GdkWindowTypeHint hint;
  GdkWindow *transient_for;

  int scale;
  gboolean mapped;
  gboolean suspended;

  guint frame_clock_idle_id;
};

struct _GdkWindowImplOrbitalClass
{
  GdkWindowImplClass parent_class;
};

GType _gdk_window_impl_orbital_get_type (void);

G_DEFINE_TYPE (GdkWindowImplOrbital, _gdk_window_impl_orbital, GDK_TYPE_WINDOW_IMPL)

static void
_gdk_window_impl_orbital_init (GdkWindowImplOrbital *impl)
{
  impl->scale = 1;
  impl->suspended = FALSE;
  impl->mapped = FALSE;
  impl->orb_window = NULL;
  impl->cairo_surface = NULL;
}

static void
gdk_orbital_window_ensure_cairo_surface (GdkWindow *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (impl->cairo_surface != NULL)
    {
      return;
    }

  if (impl->orb_window)
    {
      uint32_t *pixel_data = orb_window_data (impl->orb_window);
      
      int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, window->width);

      impl->cairo_surface = cairo_image_surface_create_for_data ((unsigned char *)pixel_data,
                                                                 CAIRO_FORMAT_ARGB32,
                                                                 window->width,
                                                                 window->height,
                                                                 stride);
      cairo_surface_set_device_scale (impl->cairo_surface, impl->scale, impl->scale);
    }
  else
    {
      /* Software Fallback Patch: Allocates an out-of-screen buffer container so 
       * early sizing metrics or theme evaluations won't throw page faults. */
      impl->cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                        MAX (1, window->width),
                                                        MAX (1, window->height));
      cairo_surface_set_device_scale (impl->cairo_surface, impl->scale, impl->scale);
    }
}

static void
drop_cairo_surfaces (GdkWindow *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (impl->cairo_surface)
    {
      cairo_surface_destroy (impl->cairo_surface);
      impl->cairo_surface = NULL;
    }
}

static void
gdk_orbital_window_update_size (GdkWindow *window,
                                int32_t    width,
                                int32_t    height,
                                int        scale)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  GdkRectangle area;
  cairo_region_t *region;

  if ((window->width == width) &&
      (window->height == height) &&
      (impl->scale == scale))
    {
      return;
    }

  drop_cairo_surfaces (window);

  window->width = MAX (1, width);
  window->height = MAX (1, height);
  impl->scale = scale;

  if (impl->orb_window)
    {
      orb_window_set_size (impl->orb_window, window->width, window->height);
    }

  area.x = 0;
  area.y = 0;
  area.width = window->width;
  area.height = window->height;

  region = cairo_region_create_rectangle (&area);
  _gdk_window_invalidate_for_expose (window, region);
  cairo_region_destroy (region);
}

GdkWindow *
_gdk_orbital_screen_create_root_window (GdkScreen *screen, int width, int height)
{
  GdkOrbitalDisplay *display_orbital =
    GDK_ORBITAL_DISPLAY (gdk_screen_get_display (screen));
  GdkWindow *window;
  GdkWindowImplOrbital *impl;

  window = _gdk_display_create_window (GDK_DISPLAY (display_orbital));
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_ORBITAL, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  impl->wrapper = GDK_WINDOW (window);
  impl->scale = 1;

  window->window_type = GDK_WINDOW_ROOT;
  window->depth = 32;

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  window->width = width;
  window->height = height;
  window->viewable = TRUE;

  /* see init_randr_support() in gdkscreen-orbital.c */
  window->event_mask = GDK_STRUCTURE_MASK;

  return window;
}

static void
gdk_orbital_window_create_surface (GdkWindow *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  uint32_t flags = ORB_WINDOW_ASYNC | ORB_WINDOW_TRANSPARENT;

  if (window->window_type == GDK_WINDOW_TOPLEVEL)
    flags |= ORB_WINDOW_RESIZABLE;
  else if (window->window_type == GDK_WINDOW_TEMP || impl->hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU)
    flags |= ORB_WINDOW_BORDERLESS;

  impl->orb_window = orb_window_new_flags (window->x + window->abs_x, window->y + window->abs_y,
                                           MAX(1, window->width), 
                                           MAX(1, window->height),
                                           impl->title ? impl->title : "",
                                           flags);

  gdk_orbital_window_ensure_cairo_surface (window);
}

static gboolean
frame_clock_pulse (gpointer data)
{
  GdkWindow *window = GDK_WINDOW (data);
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  GdkFrameClock *clock = gdk_window_get_frame_clock (window);
  GdkFrameTimings *timings;

  impl->frame_clock_idle_id = 0;

  if (GDK_WINDOW_DESTROYED (window))
    return G_SOURCE_REMOVE;

  _gdk_frame_clock_thaw (clock);
  
  timings = gdk_frame_clock_get_timings (clock, gdk_frame_clock_get_frame_counter (clock));
  if (timings)
    {
      timings->refresh_interval = 16667; /* 60Hz */
      timings->complete = TRUE;
    }

  return G_SOURCE_REMOVE;
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkWindow     *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (window->update_freeze_count > 0 || !impl->orb_window)
    return;

  _gdk_frame_clock_freeze (clock);

  if (impl->cairo_surface)
    cairo_surface_flush (impl->cairo_surface);

  orb_window_update (impl->orb_window);

  if (impl->frame_clock_idle_id == 0)
    impl->frame_clock_idle_id = g_idle_add (frame_clock_pulse, window);
}

void *
_gdk_orbital_window_get_native (GdkWindow *window)
{
  GdkWindowImplOrbital *impl;

  if (!GDK_IS_WINDOW (window) || !GDK_IS_WINDOW_IMPL_ORBITAL (window->impl))
    return NULL;

  impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  return impl->orb_window;
}

void
_gdk_orbital_display_create_window_impl (GdkDisplay    *display,
                                         GdkWindow     *window,
                                         GdkWindow     *real_parent,
                                         GdkScreen     *screen,
                                         GdkEventMask   event_mask,
                                         GdkWindowAttr *attributes,
                                         gint           attributes_mask)
{
  GdkWindowImplOrbital *impl;
  GdkFrameClock *frame_clock;
  const char *title;

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_ORBITAL, NULL);
  window->impl = GDK_WINDOW_IMPL (impl);
  impl->wrapper = GDK_WINDOW (window);

  g_object_ref (window);
  impl->scale = 1;

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = g_get_application_name () ? g_get_application_name () : "GTK Application";

  impl->title = g_strdup (title);

  gdk_orbital_window_create_surface (window);

  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  frame_clock = gdk_window_get_frame_clock (window);
  g_signal_connect (frame_clock, "after-paint", G_CALLBACK (on_frame_clock_after_paint), window);

  gdk_orbital_event_source_add_window (gdk_window_get_display (window), window);
}

static cairo_surface_t *
gdk_orbital_window_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  gdk_orbital_window_ensure_cairo_surface (window);
  cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static cairo_surface_t *
gdk_orbital_window_create_similar_image_surface (GdkWindow * window,
                                                 cairo_format_t  format,
                                                 int             width,
                                                 int             height)
{
  return cairo_image_surface_create (format, width, height);
}

static gboolean
gdk_window_impl_orbital_begin_paint (GdkWindow *window)
{
  gdk_orbital_window_ensure_cairo_surface (window);
  return FALSE;
}

static void
gdk_window_impl_orbital_end_paint (GdkWindow *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (!GDK_WINDOW_IS_MAPPED (window) || !impl->orb_window)
    return;

  cairo_surface_flush (impl->cairo_surface);
  
  if (window->current_paint.region)
    {
      cairo_rectangle_int_t rect;
      int i, n = cairo_region_num_rectangles (window->current_paint.region);
      
      for (i = 0; i < n; i++)
        {
          cairo_region_get_rectangle (window->current_paint.region, i, &rect);
          int32_t rects[4] = { rect.x, rect.y, rect.width, rect.height };
          orb_window_update_rects (impl->orb_window, rects, 1);
        }
    }
  else
    {
      orb_window_update (impl->orb_window);
    }
}

static void
gdk_orbital_window_hide_surface (GdkWindow *window)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (impl->orb_window)
    {
      drop_cairo_surfaces (window);
      gdk_orbital_event_source_remove_window (gdk_window_get_display (window), window);
      orb_window_destroy (impl->orb_window);
      impl->orb_window = NULL;
    }

  impl->mapped = FALSE;
}

static void
gdk_window_impl_orbital_finalize (GObject *object)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (object);

  g_free (impl->title);
  
  if (impl->frame_clock_idle_id)
    g_source_remove (impl->frame_clock_idle_id);

  G_OBJECT_CLASS (_gdk_window_impl_orbital_parent_class)->finalize (object);
}

static void
gdk_orbital_window_show (GdkWindow *window,
                         gboolean   already_mapped)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (!impl->orb_window)
    gdk_orbital_window_create_surface (window);

  impl->mapped = TRUE;
  _gdk_make_event (window, GDK_MAP, NULL, FALSE);
}

static void
gdk_orbital_window_hide (GdkWindow *window)
{
  gdk_orbital_window_hide_surface (window);
  _gdk_window_clear_update_area (window);
}

static void
gdk_window_orbital_withdraw (GdkWindow *window)
{
  if (!window->destroyed)
    {
      if (GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_WITHDRAWN);

      gdk_orbital_window_hide_surface (window);
    }
}

static void
gdk_window_orbital_move_resize (GdkWindow *window,
                                gboolean   with_move,
                                gint       x,
                                gint       y,
                                gint       width,
                                gint       height)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (with_move)
    {
      window->x = x;
      window->y = y;
      if (impl->orb_window)
        orb_window_set_pos (impl->orb_window, x, y);
    }

  if (width > 0 && height > 0)
    {
      gdk_orbital_window_update_size (window, width, height, impl->scale);
    }
}

static void
gdk_orbital_window_set_title (GdkWindow   *window,
                              const gchar *title)
{
  GdkWindowImplOrbital *impl;
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);

  if (g_strcmp0 (impl->title, title) == 0)
    return;

  g_free (impl->title);
  impl->title = g_strdup (title);

  if (impl->orb_window)
    orb_window_set_title (impl->orb_window, impl->title);
}

static void
gdk_orbital_window_destroy (GdkWindow *window,
                            gboolean   recursing,
                            gboolean   foreign_destroy)
{
  gdk_orbital_window_hide_surface (window);
}


static void
maybe_set_gtk_surface_dbus_properties (GdkWindow *window)
{
}

void
gdk_orbital_window_set_dbus_properties_libgtk_only (GdkWindow  *window,
                                                    const char *application_id,
                                                    const char *app_menu_path,
                                                    const char *menubar_path,
                                                    const char *window_object_path,
                                                    const char *application_object_path,
                                                    const char *unique_bus_name)
{
  maybe_set_gtk_surface_dbus_properties (window);
}

static void gdk_window_orbital_set_events (GdkWindow *window, GdkEventMask event_mask) { GDK_WINDOW (window)->event_mask = event_mask; }
static GdkEventMask gdk_window_orbital_get_events (GdkWindow *window) { return GDK_WINDOW_DESTROYED(window) ? 0 : GDK_WINDOW (window)->event_mask; }
static void gdk_window_orbital_raise (GdkWindow *window) {}
static void gdk_window_orbital_lower (GdkWindow *window) {}
static void gdk_window_orbital_restack_under (GdkWindow *window, GList *native_siblings) {}
static void gdk_window_orbital_restack_toplevel (GdkWindow *window, GdkWindow *sibling, gboolean above) {}
static void gdk_window_orbital_move_to_rect (GdkWindow *w, const GdkRectangle *r, GdkGravity ra, GdkGravity wa, GdkAnchorHints ah, gint dx, gint dy) {}
static void gdk_window_orbital_set_background (GdkWindow *window, cairo_pattern_t *pattern) {}
static gboolean gdk_window_orbital_reparent (GdkWindow *w, GdkWindow *np, gint x, gint y) { return FALSE; }
static void gdk_window_orbital_set_device_cursor (GdkWindow *w, GdkDevice *d, GdkCursor *c) { if (!GDK_WINDOW_DESTROYED(w)) GDK_DEVICE_GET_CLASS(d)->set_window_cursor(d, w, c); }
static void gdk_window_orbital_get_geometry (GdkWindow *w, gint *x, gint *y, gint *width, gint *height) { if (!GDK_WINDOW_DESTROYED(w)) { if (x) *x = w->x; if (y) *y = w->y; if (width) *width = w->width; if (height) *height = w->height; } }
static void gdk_window_orbital_get_root_coords (GdkWindow *w, gint x, gint y, gint *rx, gint *ry) { if (rx) *rx = w->x + x; if (ry) *ry = w->y + y; }
static gboolean gdk_window_orbital_get_device_state (GdkWindow *w, GdkDevice *d, gdouble *x, gdouble *y, GdkModifierType *m) { GdkWindow *child; if (!GDK_WINDOW_DESTROYED(w)) { GDK_DEVICE_GET_CLASS(d)->query_state(d, w, NULL, &child, NULL, NULL, x, y, m); return child != NULL; } return FALSE; }
static void gdk_window_orbital_shape_combine_region (GdkWindow *w, const cairo_region_t *sr, gint ox, gint oy) {}
static void gdk_window_orbital_input_shape_combine_region (GdkWindow *w, const cairo_region_t *sr, gint ox, gint oy) {}
static void gdk_window_orbital_destroy_foreign (GdkWindow *window) {}
static cairo_region_t *gdk_orbital_window_get_shape (GdkWindow *window) { return NULL; }
static cairo_region_t *gdk_orbital_window_get_input_shape (GdkWindow *window) { return NULL; }
static void gdk_orbital_window_focus (GdkWindow *window, guint32 timestamp) {}
static void gdk_orbital_window_set_type_hint (GdkWindow *w, GdkWindowTypeHint h) { GDK_WINDOW_IMPL_ORBITAL(w->impl)->hint = h; }
static GdkWindowTypeHint gdk_orbital_window_get_type_hint (GdkWindow *window) { return GDK_WINDOW_DESTROYED(window) ? GDK_WINDOW_TYPE_HINT_NORMAL : GDK_WINDOW_IMPL_ORBITAL(window->impl)->hint; }
static void gdk_orbital_window_set_modal_hint (GdkWindow *window, gboolean modal) { window->modal_hint = modal; }
static void gdk_orbital_window_set_skip_taskbar_hint (GdkWindow *window, gboolean skips) {}
static void gdk_orbital_window_set_skip_pager_hint (GdkWindow *window, gboolean skips) {}
static void gdk_orbital_window_set_urgency_hint (GdkWindow *window, gboolean urgent) {}
static void gdk_orbital_window_set_geometry_hints (GdkWindow *w, const GdkGeometry *g, GdkWindowHints m) {}
static void gdk_orbital_window_set_role (GdkWindow *window, const gchar *role) {}
static void gdk_orbital_window_set_startup_id (GdkWindow *window, const gchar *startup_id) {}
static void gdk_orbital_window_set_transient_for (GdkWindow *window, GdkWindow *parent) { GDK_WINDOW_IMPL_ORBITAL(window->impl)->transient_for = parent; }
static void gdk_orbital_window_get_frame_extents (GdkWindow *window, GdkRectangle *rect) { *rect = (GdkRectangle) { .x = window->x, .y = window->y, .width = window->width, .height = window->height }; }
static void gdk_orbital_window_set_override_redirect (GdkWindow *window, gboolean override_redirect) {}
static void gdk_orbital_window_set_accept_focus (GdkWindow *window, gboolean accept_focus) {}
static void gdk_orbital_window_set_focus_on_map (GdkWindow *window, gboolean focus_on_map) {}
static void gdk_orbital_window_set_icon_list (GdkWindow *window, GList *pixbufs) {}
static void gdk_orbital_window_set_icon_name (GdkWindow *window, const gchar *name) {}
static void gdk_orbital_window_iconify (GdkWindow *window) {}
static void gdk_orbital_window_deiconify (GdkWindow *window) {}
static void gdk_orbital_window_stick (GdkWindow *window) {}
static void gdk_orbital_window_unstick (GdkWindow *window) {}
static void gdk_orbital_window_maximize (GdkWindow *window) {}
static void gdk_orbital_window_unmaximize (GdkWindow *window) {}
static void gdk_orbital_window_fullscreen (GdkWindow *window) {}
static void gdk_orbital_window_fullscreen_on_monitor (GdkWindow *window, gint monitor) {}
static void gdk_orbital_window_unfullscreen (GdkWindow *window) {}
static void gdk_orbital_window_set_keep_above (GdkWindow *window, gboolean setting) {}
static void gdk_orbital_window_set_keep_below (GdkWindow *window, gboolean setting) {}
static GdkWindow *gdk_orbital_window_get_group (GdkWindow *window) { return NULL; }
static void gdk_orbital_window_set_group (GdkWindow *window, GdkWindow *leader) {}
static void gdk_orbital_window_set_decorations (GdkWindow *window, GdkWMDecoration decorations) {}
static gboolean gdk_orbital_window_get_decorations (GdkWindow *window, GdkWMDecoration *decorations) { return FALSE; }
static void gdk_orbital_window_set_functions (GdkWindow *window, GdkWMFunction functions) {}
static void gdk_orbital_window_begin_resize_drag (GdkWindow *w, GdkWindowEdge e, GdkDevice *d, gint b, gint rx, gint ry, guint32 time) {}
static void gdk_orbital_window_begin_move_drag (GdkWindow *w, GdkDevice *d, gint b, gint rx, gint ry, guint32 time) {}
static void gdk_orbital_window_set_opacity (GdkWindow *window, gdouble opacity) {}
static void gdk_orbital_window_set_composited (GdkWindow *window, gboolean composited) {}
static void gdk_orbital_window_destroy_notify (GdkWindow *window) { if (!GDK_WINDOW_DESTROYED(window)) { _gdk_window_destroy(window, TRUE); } g_object_unref(window); }
static void gdk_orbital_window_sync_rendering (GdkWindow *window) {}
static gboolean gdk_orbital_window_simulate_key (GdkWindow *w, gint x, gint y, guint keyval, GdkModifierType m, GdkEventType pr) { return FALSE; }
static gboolean gdk_orbital_window_simulate_button (GdkWindow *w, gint x, gint y, guint b, GdkModifierType m, GdkEventType pr) { return FALSE; }
static gboolean gdk_orbital_window_get_property (GdkWindow *w, GdkAtom p, GdkAtom t, gulong o, gulong l, gint del, GdkAtom *at, gint *af, gint *al, guchar **d) { return FALSE; }
static void gdk_orbital_window_change_property (GdkWindow *w, GdkAtom p, GdkAtom t, gint f, GdkPropMode m, const guchar *d, gint nel) {}
static void gdk_orbital_window_delete_property (GdkWindow *w, GdkAtom p) {}
static gint gdk_orbital_window_get_scale_factor (GdkWindow *window) { return GDK_WINDOW_DESTROYED(window) ? 1 : GDK_WINDOW_IMPL_ORBITAL(window->impl)->scale; }
static void gdk_orbital_window_set_opaque_region (GdkWindow *window, cairo_region_t *region) {}
static void gdk_orbital_window_set_shadow_width (GdkWindow *window, int left, int right, int top, int bottom) {}
static gboolean gdk_orbital_window_show_window_menu (GdkWindow *window, GdkEvent *event) { return FALSE; }
static gboolean gdk_orbital_window_titlebar_gesture (GdkWindow *window, GdkTitlebarGesture gesture) { return FALSE; }
static gboolean gdk_window_impl_orbital_beep (GdkWindow *window) { return TRUE; }

static void
_gdk_window_impl_orbital_class_init (GdkWindowImplOrbitalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_window_impl_orbital_finalize;

  impl_class->ref_cairo_surface = gdk_orbital_window_ref_cairo_surface;
  impl_class->create_similar_image_surface = gdk_orbital_window_create_similar_image_surface;
  impl_class->show = gdk_orbital_window_show;
  impl_class->hide = gdk_orbital_window_hide;
  impl_class->withdraw = gdk_window_orbital_withdraw;
  impl_class->set_events = gdk_window_orbital_set_events;
  impl_class->get_events = gdk_window_orbital_get_events;
  impl_class->raise = gdk_window_orbital_raise;
  impl_class->lower = gdk_window_orbital_lower;
  impl_class->restack_under = gdk_window_orbital_restack_under;
  impl_class->restack_toplevel = gdk_window_orbital_restack_toplevel;
  impl_class->move_resize = gdk_window_orbital_move_resize;
  impl_class->move_to_rect = gdk_window_orbital_move_to_rect;
  impl_class->set_background = gdk_window_orbital_set_background;
  impl_class->reparent = gdk_window_orbital_reparent;
  impl_class->set_device_cursor = gdk_window_orbital_set_device_cursor;
  impl_class->get_geometry = gdk_window_orbital_get_geometry;
  impl_class->get_root_coords = gdk_window_orbital_get_root_coords;
  impl_class->get_device_state = gdk_window_orbital_get_device_state;
  impl_class->shape_combine_region = gdk_window_orbital_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_orbital_input_shape_combine_region;
  impl_class->destroy = gdk_orbital_window_destroy;
  impl_class->destroy_foreign = gdk_window_orbital_destroy_foreign;
  impl_class->get_shape = gdk_orbital_window_get_shape;
  impl_class->get_input_shape = gdk_orbital_window_get_input_shape;
  impl_class->begin_paint = gdk_window_impl_orbital_begin_paint;
  impl_class->end_paint = gdk_window_impl_orbital_end_paint;
  impl_class->beep = gdk_window_impl_orbital_beep;
  impl_class->focus = gdk_orbital_window_focus;
  impl_class->set_type_hint = gdk_orbital_window_set_type_hint;
  impl_class->get_type_hint = gdk_orbital_window_get_type_hint;
  impl_class->set_modal_hint = gdk_orbital_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_orbital_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_orbital_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_orbital_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_orbital_window_set_geometry_hints;
  impl_class->set_title = gdk_orbital_window_set_title;
  impl_class->set_role = gdk_orbital_window_set_role;
  impl_class->set_startup_id = gdk_orbital_window_set_startup_id;
  impl_class->set_transient_for = gdk_orbital_window_set_transient_for;
  impl_class->get_frame_extents = gdk_orbital_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_orbital_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_orbital_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_orbital_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_orbital_window_set_icon_list;
  impl_class->set_icon_name = gdk_orbital_window_set_icon_name;
  impl_class->iconify = gdk_orbital_window_iconify;
  impl_class->deiconify = gdk_orbital_window_deiconify;
  impl_class->stick = gdk_orbital_window_stick;
  impl_class->unstick = gdk_orbital_window_unstick;
  impl_class->maximize = gdk_orbital_window_maximize;
  impl_class->unmaximize = gdk_orbital_window_unmaximize;
  impl_class->fullscreen = gdk_orbital_window_fullscreen;
  impl_class->fullscreen_on_monitor = gdk_orbital_window_fullscreen_on_monitor;
  impl_class->unfullscreen = gdk_orbital_window_unfullscreen;
  impl_class->set_keep_above = gdk_orbital_window_set_keep_above;
  impl_class->set_keep_below = gdk_orbital_window_set_keep_below;
  impl_class->get_group = gdk_orbital_window_get_group;
  impl_class->set_group = gdk_orbital_window_set_group;
  impl_class->set_decorations = gdk_orbital_window_set_decorations;
  impl_class->get_decorations = gdk_orbital_window_get_decorations;
  impl_class->set_functions = gdk_orbital_window_set_functions;
  impl_class->begin_resize_drag = gdk_orbital_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_orbital_window_begin_move_drag;
  impl_class->set_opacity = gdk_orbital_window_set_opacity;
  impl_class->set_composited = gdk_orbital_window_set_composited;
  impl_class->destroy_notify = gdk_orbital_window_destroy_notify;
  impl_class->get_drag_protocol = _gdk_orbital_window_get_drag_protocol;
  impl_class->register_dnd = _gdk_orbital_window_register_dnd;
  impl_class->drag_begin = _gdk_orbital_window_drag_begin;
  impl_class->sync_rendering = gdk_orbital_window_sync_rendering;
  impl_class->simulate_key = gdk_orbital_window_simulate_key;
  impl_class->simulate_button = gdk_orbital_window_simulate_button;
  impl_class->get_property = gdk_orbital_window_get_property;
  impl_class->change_property = gdk_orbital_window_change_property;
  impl_class->delete_property = gdk_orbital_window_delete_property;
  impl_class->get_scale_factor = gdk_orbital_window_get_scale_factor;
  impl_class->set_opaque_region = gdk_orbital_window_set_opaque_region;
  impl_class->set_shadow_width = gdk_orbital_window_set_shadow_width;
  impl_class->show_window_menu = gdk_orbital_window_show_window_menu;
  impl_class->create_gl_context = gdk_orbital_window_create_gl_context;
  impl_class->invalidate_for_new_frame = gdk_orbital_window_invalidate_for_new_frame;
  impl_class->titlebar_gesture = gdk_orbital_window_titlebar_gesture;
}

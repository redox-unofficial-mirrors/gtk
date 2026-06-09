/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Private uninstalled header defining things local to the Orbital backend
 */

#ifndef __GDK_PRIVATE_ORBITAL_H__
#define __GDK_PRIVATE_ORBITAL_H__

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/orbital/gdkorbital.h>
#include <gdk/orbital/gdkdisplay-orbital.h>

/* Avoid pulling in full wayland/egl headers by using opaque gpointers */
#ifndef EGL_API_TYPES_DEFINED
typedef gpointer EGLSurface;
typedef gpointer EGLConfig;
#endif

#include "gdkinternals.h"
#include "config.h"

#define WL_SURFACE_HAS_BUFFER_SCALE 3
#define WL_POINTER_HAS_FRAME 5

#define GDK_WINDOW_IS_ORBITAL(win)    (GDK_IS_WINDOW_IMPL_ORBITAL (((GdkWindow *)win)->impl))

GdkKeymap *_gdk_orbital_keymap_new (void);
void       _gdk_orbital_keymap_update_from_fd (GdkKeymap *keymap,
                                               uint32_t   format,
                                               uint32_t   fd,
                                               uint32_t   size);
struct xkb_state *_gdk_orbital_keymap_get_xkb_state (GdkKeymap *keymap);
struct xkb_keymap *_gdk_orbital_keymap_get_xkb_keymap (GdkKeymap *keymap);
gboolean           _gdk_orbital_keymap_key_is_modifier (GdkKeymap *keymap,
                                                        guint      keycode);

void       _gdk_orbital_display_init_cursors (GdkOrbitalDisplay *display);
void       _gdk_orbital_display_finalize_cursors (GdkOrbitalDisplay *display);
void       _gdk_orbital_display_update_cursors (GdkOrbitalDisplay *display);

gpointer   _gdk_orbital_display_get_scaled_cursor_theme (GdkOrbitalDisplay *display_orbital,
                                                         guint              scale);

GdkCursor *_gdk_orbital_display_get_cursor_for_type (GdkDisplay    *display,
                                                     GdkCursorType  cursor_type);
GdkCursor *_gdk_orbital_display_get_cursor_for_type_with_scale (GdkDisplay    *display,
                                                                GdkCursorType  cursor_type,
                                                                guint          scale);
GdkCursor *_gdk_orbital_display_get_cursor_for_name (GdkDisplay  *display,
                                                     const gchar *name);
GdkCursor *_gdk_orbital_display_get_cursor_for_name_with_scale (GdkDisplay  *display,
                                                                const gchar *name,
                                                                guint        scale);
GdkCursor *_gdk_orbital_display_get_cursor_for_surface (GdkDisplay *display,
                                                        cairo_surface_t *surface,
                                                        gdouble     x,
                                                        gdouble     y);
void       _gdk_orbital_display_get_default_cursor_size (GdkDisplay *display,
                                                         guint       *width,
                                                         guint       *height);
void       _gdk_orbital_display_get_maximal_cursor_size (GdkDisplay *display,
                                                         guint       *width,
                                                         guint       *height);
gboolean   _gdk_orbital_display_supports_cursor_alpha (GdkDisplay *display);
gboolean   _gdk_orbital_display_supports_cursor_color (GdkDisplay *display);

void       gdk_orbital_display_system_bell (GdkDisplay *display,
                                            GdkWindow  *window);

gpointer   _gdk_orbital_cursor_get_buffer (GdkCursor *cursor,
                                           guint      image_index,
                                           int       *hotspot_x,
                                           int       *hotspot_y,
                                           int       *w,
                                           int       *h,
                                           int       *scale);
guint      _gdk_orbital_cursor_get_next_image_index (GdkCursor *cursor,
                                                     guint      current_image_index,
                                                     guint     *next_image_delay);

void       _gdk_orbital_cursor_set_scale (GdkCursor *cursor,
                                          guint      scale);

GdkDragProtocol _gdk_orbital_window_get_drag_protocol (GdkWindow *window,
                                                       GdkWindow **target);

void            _gdk_orbital_window_register_dnd (GdkWindow *window);
GdkDragContext *_gdk_orbital_window_drag_begin (GdkWindow *window,
                                                GdkDevice *device,
                                                GList     *targets,
                                                gint       x_root,
                                                gint       y_root);
void            _gdk_orbital_window_offset_next_wl_buffer (GdkWindow *window,
                                                           int        x,
                                                           int        y);
GdkDragContext * _gdk_orbital_drop_context_new (GdkDisplay *display,
                                                gpointer    data_device);
void _gdk_orbital_drag_context_set_source_window (GdkDragContext *context,
                                                  GdkWindow      *window);
void _gdk_orbital_drag_context_set_dest_window (GdkDragContext *context,
                                                GdkWindow      *dest_window,
                                                uint32_t        serial);
void _gdk_orbital_drag_context_emit_event (GdkDragContext *context,
                                           GdkEventType    type,
                                           guint32         time_);
void _gdk_orbital_drag_context_set_coords (GdkDragContext *context,
                                           gdouble         x,
                                           gdouble         y);

void gdk_orbital_drag_context_set_action (GdkDragContext *context,
                                          GdkDragAction   action);

GdkDragContext * gdk_orbital_drag_context_lookup_by_data_source   (gpointer source);
GdkDragContext * gdk_orbital_drag_context_lookup_by_source_window (GdkWindow *window);
gpointer         gdk_orbital_drag_context_get_data_source  (GdkDragContext *context);

void gdk_orbital_drop_context_update_targets (GdkDragContext *context);

void _gdk_orbital_display_create_window_impl (GdkDisplay    *display,
                                              GdkWindow     *window,
                                              GdkWindow     *real_parent,
                                              GdkScreen     *screen,
                                              GdkEventMask   event_mask,
                                              GdkWindowAttr *attributes,
                                              gint           attributes_mask);

GdkWindow *_gdk_orbital_display_get_selection_owner (GdkDisplay *display,
                                                     GdkAtom     selection);
gboolean   _gdk_orbital_display_set_selection_owner (GdkDisplay *display,
                                                     GdkWindow  *owner,
                                                     GdkAtom     selection,
                                                     guint32     time,
                                                     gboolean    send_event);
void       _gdk_orbital_display_send_selection_notify (GdkDisplay *display,
                                                       GdkWindow  *requestor,
                                                       GdkAtom     selection,
                                                       GdkAtom     target,
                                                       GdkAtom     property,
                                                       guint32          time);
gint       _gdk_orbital_display_get_selection_property (GdkDisplay  *display,
                                                        GdkWindow   *requestor,
                                                        guchar     **data,
                                                        GdkAtom     *ret_type,
                                                        gint        *ret_format);
void       _gdk_orbital_display_convert_selection (GdkDisplay *display,
                                                   GdkWindow  *requestor,
                                                   GdkAtom     selection,
                                                   GdkAtom     target,
                                                   guint32     time);
gint        _gdk_orbital_display_text_property_to_utf8_list (GdkDisplay    *display,
                                                             GdkAtom        encoding,
                                                             gint           format,
                                                             const guchar  *text,
                                                             gint           length,
                                                             gchar       ***list);
gchar * _gdk_orbital_display_utf8_to_string_target (GdkDisplay  *display,
                                                        const gchar *str);

gboolean    gdk_orbital_display_make_gl_context_current (GdkDisplay        *display,
                                                         GdkGLContext      *context);

GdkDeviceManager *_gdk_orbital_device_manager_new (GdkDisplay *display);
void              _gdk_orbital_device_manager_add_seat (GdkDeviceManager *device_manager,
                                                        guint32           id,
                                                        gpointer          seat);
void              _gdk_orbital_device_manager_remove_seat (GdkDeviceManager *device_manager,
                                                           guint32           id);

GdkKeymap *_gdk_orbital_device_get_keymap (GdkDevice *device);
uint32_t _gdk_orbital_device_get_implicit_grab_serial(GdkOrbitalDevice *device,
                                                      const GdkEvent   *event);
uint32_t _gdk_orbital_seat_get_last_implicit_grab_serial (GdkSeat           *seat,
                                                          GdkEventSequence **sequence);
GdkWindow * gdk_orbital_device_get_focus (GdkDevice *device);
gpointer    gdk_orbital_device_get_data_device (GdkDevice *gdk_device);
void gdk_orbital_seat_set_selection (GdkSeat  *seat,
                                     gpointer  source);

void gdk_orbital_seat_set_primary (GdkSeat  *seat,
                                   gpointer  source);

GdkDragContext * gdk_orbital_device_get_drop_context (GdkDevice *gdk_device);

void     _gdk_orbital_display_deliver_event (GdkDisplay *display, GdkEvent *event);
GSource *_gdk_orbital_display_event_source_new (GdkDisplay *display);
void     _gdk_orbital_display_queue_events (GdkDisplay *display);
guint    _gdk_orbital_get_keysym_from_scancode (uint32_t scancode);

GdkAppLaunchContext *_gdk_orbital_display_get_app_launch_context (GdkDisplay *display);

GdkDisplay *_gdk_orbital_display_open (const gchar *display_name);

GdkWindow *_gdk_orbital_screen_create_root_window (GdkScreen *screen,
                                                   int width,
                                                   int height);

GdkScreen *_gdk_orbital_screen_new (GdkDisplay *display);
void _gdk_orbital_screen_add_output (GdkScreen *screen,
                                     guint32    id,
                                     gpointer   output,
                                     guint32    version);
void _gdk_orbital_screen_remove_output (GdkScreen *screen,
                                        guint32 id);
int _gdk_orbital_screen_get_output_refresh_rate (GdkScreen *screen,
                                                 gpointer   output);
guint32 _gdk_orbital_screen_get_output_scale (GdkScreen *screen,
                                              gpointer   output);
gpointer _gdk_orbital_screen_get_wl_output (GdkScreen *screen,
                                            gint monitor_num);

void _gdk_orbital_screen_set_has_gtk_shell (GdkScreen       *screen);

void _gdk_orbital_screen_init_xdg_output (GdkScreen *screen);

void _gdk_orbital_window_set_grab_seat (GdkWindow      *window,
                                        GdkSeat        *seat);

guint32 _gdk_orbital_display_get_serial (GdkOrbitalDisplay *display_orbital);
void _gdk_orbital_display_update_serial (GdkOrbitalDisplay *display_orbital,
                                         guint32            serial);

cairo_surface_t * _gdk_orbital_display_create_shm_surface (GdkOrbitalDisplay *display,
                                                           int                width,
                                                           int                height,
                                                           guint              scale);
gpointer _gdk_orbital_shm_surface_get_wl_buffer (cairo_surface_t *surface);
gboolean _gdk_orbital_is_shm_surface (cairo_surface_t *surface);

GdkOrbitalSelection * gdk_orbital_display_get_selection (GdkDisplay *display);
GdkOrbitalSelection * gdk_orbital_selection_new (void);
void gdk_orbital_selection_free (GdkOrbitalSelection *selection);

void gdk_orbital_selection_ensure_offer (GdkDisplay *display,
                                         gpointer    wl_offer);
void gdk_orbital_selection_ensure_primary_offer (GdkDisplay *display,
                                                 gpointer    wp_offer);

void gdk_orbital_selection_set_offer (GdkDisplay           *display,
                                      GdkAtom               selection,
                                      gpointer              offer);
gpointer gdk_orbital_selection_get_offer (GdkDisplay *display,
                                          GdkAtom     selection);
GList * gdk_orbital_selection_get_targets (GdkDisplay *display,
                                           GdkAtom     selection);

void     gdk_orbital_selection_store   (GdkWindow    *window,
                                        GdkAtom       type,
                                        GdkPropMode   mode,
                                        const guchar *data,
                                        gint          len);
gpointer gdk_orbital_selection_get_data_source (GdkWindow *owner,
                                                GdkAtom    selection);
void gdk_orbital_selection_unset_data_source (GdkDisplay *display, GdkAtom selection);
gboolean gdk_orbital_selection_set_current_offer_actions (GdkDisplay *display,
                                                          uint32_t    actions);

EGLSurface gdk_orbital_window_get_egl_surface (GdkWindow *window,
                                               EGLConfig config);
EGLSurface gdk_orbital_window_get_dummy_egl_surface (GdkWindow *window,
                                                     EGLConfig config);

gpointer gdk_orbital_window_get_gtk_surface (GdkWindow *window);

void gdk_orbital_seat_set_global_cursor (GdkSeat   *seat,
                                         GdkCursor *cursor); 

gpointer gdk_orbital_window_get_wl_output (GdkWindow *window);

void gdk_orbital_window_inhibit_shortcuts (GdkWindow *window,
                                           GdkSeat   *gdk_seat);
void gdk_orbital_window_restore_shortcuts (GdkWindow *window,
                                           GdkSeat   *gdk_seat);

void * _gdk_orbital_window_get_native (GdkWindow *window);
void gdk_orbital_event_source_add_window    (GdkDisplay *display, GdkWindow *window);
void gdk_orbital_event_source_remove_window (GdkDisplay *display, GdkWindow *window);

#endif /* __GDK_PRIVATE_ORBITAL_H__ */

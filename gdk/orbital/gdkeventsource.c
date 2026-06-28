/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2026 Redox OS Contributors
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

#include "gdkinternals.h"
#include "gdkprivate-orbital.h"

#include <unistd.h>
#include <errno.h>

typedef struct _OrbitalPollItem {
  GdkWindow *window;
  GPollFD pfd;
} OrbitalPollItem;

typedef struct _GdkOrbitalEventSource {
  GSource source;
  GdkDisplay *display;
  GList *poll_items; /* List of OrbitalPollItem* */
} GdkOrbitalEventSource;

static gboolean
gdk_event_source_prepare (GSource *base,
                          gint    *timeout)
{
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) base;

  *timeout = -1;

  return (_gdk_event_queue_find_first (source->display) != NULL);
}

static gboolean
gdk_event_source_check (GSource *base)
{
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) base;
  GList *l;

  if (_gdk_event_queue_find_first (source->display) != NULL)
    return TRUE;

  for (l = source->poll_items; l; l = l->next)
    {
      OrbitalPollItem *item = l->data;
      if (item->pfd.revents & (G_IO_IN | G_IO_ERR | G_IO_HUP))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gdk_event_source_dispatch (GSource     *base,
                           GSourceFunc  callback,
                           gpointer     data)
{
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) base;
  GdkDisplay *display = source->display;
  GdkEvent *event;

  gdk_threads_enter ();

  _gdk_orbital_display_queue_events (display);

  event = gdk_display_get_event (display);

  if (event)
    {
      _gdk_event_emit (event);
      gdk_event_free (event);
    }

  gdk_threads_leave ();

  return TRUE;
}

static void
gdk_event_source_finalize (GSource *base)
{
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) base;
  GList *l;

  for (l = source->poll_items; l; l = l->next)
    {
      OrbitalPollItem *item = l->data;
      g_free (item);
    }
  
  g_list_free (source->poll_items);
  source->poll_items = NULL;
}

static GSourceFuncs orbital_glib_source_funcs = {
  gdk_event_source_prepare,
  gdk_event_source_check,
  gdk_event_source_dispatch,
  gdk_event_source_finalize
};

void
_gdk_orbital_display_deliver_event (GdkDisplay *display,
                                    GdkEvent   *event)
{
  GList *node;

  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event,
                            _gdk_display_get_next_serial (display));
}

GSource *
_gdk_orbital_display_event_source_new (GdkDisplay *display)
{
  GSource *source;
  GdkOrbitalEventSource *orb_source;
  char *name;

  source = g_source_new (&orbital_glib_source_funcs, sizeof (GdkOrbitalEventSource));
  name = g_strdup_printf ("GDK Orbital Event source (%s)", gdk_display_get_name (display));
  g_source_set_name (source, name);
  g_free (name);

  orb_source = (GdkOrbitalEventSource *) source;
  orb_source->display = display;
  orb_source->poll_items = NULL;

  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return source;
}

void
gdk_orbital_event_source_add_window (GdkDisplay *display, GdkWindow *window)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) display_orbital->event_source;
  void *orb_window = _gdk_orbital_window_get_native (window);
  OrbitalPollItem *item;

  if (!orb_window || !source)
    return;

  int fd = orb_window_fd (orb_window);
  
  item = g_new0 (OrbitalPollItem, 1);
  item->window = window;
  item->pfd.fd = fd;
  item->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
  item->pfd.revents = 0;

  source->poll_items = g_list_prepend (source->poll_items, item);
  g_source_add_poll ((GSource *)source, &item->pfd);
}

void
gdk_orbital_event_source_remove_window (GdkDisplay *display, GdkWindow *window)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) display_orbital->event_source;
  GList *l;

  if (!source) return;

  for (l = source->poll_items; l; l = l->next)
    {
      OrbitalPollItem *item = l->data;
      if (item->window == window)
        {
          g_source_remove_poll ((GSource *)source, &item->pfd);
          source->poll_items = g_list_delete_link (source->poll_items, l);
          g_free (item);
          break;
        }
    }
}


static double last_mouse_x = 0.0;
static double last_mouse_y = 0.0;
static gboolean last_btn_left = FALSE;
static gboolean last_btn_middle = FALSE;
static gboolean last_btn_right = FALSE;
static guint current_modifiers = 0;

void
_gdk_orbital_display_queue_events (GdkDisplay *display)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);
  GdkOrbitalEventSource *source = (GdkOrbitalEventSource *) display_orbital->event_source;
  GList *l;

  GdkSeat *seat = gdk_display_get_default_seat (display);
  GdkDevice *core_pointer = seat ? gdk_seat_get_pointer (seat) : NULL;
  GdkDevice *core_keyboard = seat ? gdk_seat_get_keyboard (seat) : NULL;

  
  if (!source) return;

  for (l = source->poll_items; l; l = l->next)
    {
      OrbitalPollItem *item = l->data;
      void *orb_window = _gdk_orbital_window_get_native (item->window);
      if (!orb_window) continue;

      if (item->pfd.revents & (G_IO_IN | G_IO_ERR | G_IO_HUP))
        {
          void *event_iter = orb_window_events (orb_window);
          OrbEventOption oeo = orb_events_next (event_iter);

          while (oeo.tag != OrbEventOption_None)
            {
              GdkEvent *event = NULL;

              switch (oeo.tag)
                {
                  case OrbEventOption_Quit:
                    g_print ("[GTK EVENT]   -> QUIT\n");
                    event = gdk_event_new (GDK_DELETE);
                    event->any.window = g_object_ref (item->window);
                    event->any.send_event = FALSE;
                    break;

                  case OrbEventOption_Resize:
                    g_print ("[GTK EVENT]   -> RESIZE to %dx%d\n", oeo.resize.width, oeo.resize.height);
                    _gdk_orbital_window_update_size_scale(item->window, oeo.resize.width, oeo.resize.height, 0);
                    event = gdk_event_new (GDK_CONFIGURE);
                    event->configure.window = g_object_ref (item->window);
                    event->configure.send_event = FALSE;
                    event->configure.width = oeo.resize.width;
                    event->configure.height = oeo.resize.height;
                    break;

                  case OrbEventOption_Scale:
                    gdouble rscale =  oeo.scale.scale / (gdouble)oeo.scale.baseline;
                    g_print ("[GTK EVENT]   -> SCALE to %d / %d (%fx)\n", oeo.scale.scale, oeo.scale.baseline, rscale);
                    _gdk_orbital_window_update_size_scale(item->window, 0, 0, rscale);
                    break;

                  case OrbEventOption_Focus:
                    g_print ("[GTK EVENT]   -> FOCUS (in: %d)\n", oeo.focus.focused);
                    event = gdk_event_new (GDK_FOCUS_CHANGE);
                    if (core_keyboard) 
                    {
                      gdk_event_set_device (event, core_keyboard);
                      gdk_event_set_source_device (event, core_keyboard);
                    }
                    event->focus_change.window = g_object_ref (item->window);
                    event->focus_change.send_event = FALSE;
                    event->focus_change.in = oeo.focus.focused ? TRUE : FALSE;
                    break;

                  /* 
                    ---- Disabled in favor to XKB keyboard layout

                    case OrbEventOption_TextInput:
                  
                    g_print ("[GTK EVENT]   -> TextInput (character: %u)\n", oeo.text_input.character);
                    
                    event = gdk_event_new (GDK_KEY_PRESS);
                    if (core_keyboard) 
                      {
                        gdk_event_set_device (event, core_keyboard);
                        gdk_event_set_source_device (event, core_keyboard);
                      }
                    event->key.window = g_object_ref (item->window);
                    event->key.send_event = FALSE;
                    event->key.time = GDK_CURRENT_TIME;
                    
                    gchar utf8_buf[7] = { 0 };
                    
                    gint bytes_written = g_unichar_to_utf8 ((gunichar)oeo.text_input.character, utf8_buf);
                    
                    event->key.length = bytes_written;
                    event->key.string = g_strdup (utf8_buf);
                    
                    event->key.keyval = GDK_KEY_VoidSymbol;
                    event->key.hardware_keycode = 0;
                    
                    event->key.state = current_modifiers; 
                    break;
                  */

                  case OrbEventOption_Key:
                    g_print ("[DB_EVENTS] -> KEY (scancode: %d, pressed: %d)\n", 
                             oeo.key.scancode, oeo.key.pressed);
                    
                    event = gdk_event_new (oeo.key.pressed ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
                    
                    if (core_keyboard) 
                      {
                        gdk_event_set_device (event, core_keyboard);
                        gdk_event_set_source_device (event, core_keyboard);
                      }

                    event->key.window = g_object_ref (item->window);
                    event->key.send_event = FALSE;
                    event->key.time = GDK_CURRENT_TIME;
                    event->key.hardware_keycode = _gdk_orbital_get_xkb_keycode(oeo.key.scancode);
                    event->key.keyval = GDK_KEY_VoidSymbol;
                    gdk_event_set_scancode (event, oeo.key.scancode);

                    if (oeo.key.scancode == ORB_KEY_LEFT_SHIFT || oeo.key.scancode == ORB_KEY_RIGHT_SHIFT)
                      {
                        if (oeo.key.pressed) current_modifiers |= GDK_SHIFT_MASK;
                        else current_modifiers &= ~GDK_SHIFT_MASK;
                        event->key.is_modifier = TRUE;
                      }
                    else if (oeo.key.scancode == ORB_KEY_LEFT_CTRL || oeo.key.scancode == ORB_KEY_RIGHT_CTRL)
                      {
                        if (oeo.key.pressed) current_modifiers |= GDK_CONTROL_MASK;
                        else current_modifiers &= ~GDK_CONTROL_MASK;
                        event->key.is_modifier = TRUE;
                      }
                    else if (oeo.key.scancode == ORB_KEY_ALT || oeo.key.scancode == ORB_KEY_ALT_GR)
                      {
                        if (oeo.key.pressed) current_modifiers |= GDK_MOD1_MASK;
                        else current_modifiers &= ~GDK_MOD1_MASK;
                        event->key.is_modifier = TRUE;
                      }
                    else 
                      {
                        event->key.is_modifier = FALSE;
                      }

                    event->key.state = current_modifiers;

                    gdk_keymap_translate_keyboard_state (gdk_keymap_get_for_display (display),
                                event->key.hardware_keycode,
                                event->key.state,
                                event->key.group,
                                &event->key.keyval,
                                NULL, NULL, NULL);
    
                    gunichar c = 0;
                    gchar buf[7];

                    event->key.string = NULL;

                    /* Fill in ->string since apps depend on it, taken from the x11 backend. */
                    if (event->key.keyval != GDK_KEY_VoidSymbol)
                      c = gdk_keyval_to_unicode (event->key.keyval);
                    if (c)
                      {
                        gsize bytes_written;
                        gint len;

                        len = g_unichar_to_utf8 (c, buf);
                        buf[len] = '\0';
                        
                        event->key.string = g_locale_from_utf8 (buf, len,
                                  NULL, &bytes_written,
                                  NULL);
                        if (event->key.string)
                    event->key.length = bytes_written;
                      }
                    else if (event->key.keyval == GDK_KEY_Escape)
                      {
                        event->key.length = 1;
                        event->key.string = g_strdup ("\033");
                      }
                    else if (event->key.keyval == GDK_KEY_Return ||
                      event->key.keyval == GDK_KEY_KP_Enter)
                      {
                        event->key.length = 1;
                        event->key.string = g_strdup ("\r");
                      }

                    if (!event->key.string)
                      {
                        event->key.length = 0;
                        event->key.string = g_strdup ("");
                      }
                    break;

                  case OrbEventOption_Mouse:
                    // g_print ("[GTK EVENT]   -> MOUSE MOTION (x: %d, y: %d)\n", oeo.mouse.x, oeo.mouse.y);
                    
                    last_mouse_x = oeo.mouse.x;
                    last_mouse_y = oeo.mouse.y;

                    event = gdk_event_new (GDK_MOTION_NOTIFY);
                    if (core_pointer) 
                      {
                        gdk_event_set_device (event, core_pointer);
                        gdk_event_set_source_device (event, core_pointer);
                      }
                    event->motion.window = g_object_ref (item->window);
                    event->motion.send_event = FALSE;
                    event->motion.time = GDK_CURRENT_TIME;
                    event->motion.x = last_mouse_x;
                    event->motion.y = last_mouse_y;
                    event->motion.x_root = last_mouse_x;
                    event->motion.y_root = last_mouse_y;
                    
                    event->motion.state = (last_btn_left ? GDK_BUTTON1_MASK : 0);
                    // gdk_event_set_screen (event, display_orbital->screen);
                    // gdk_event_set_seat (event, seat);
                    
                    break;

                  case OrbEventOption_Button:
                    g_print ("[GTK EVENT]   -> BUTTON (L:%d M:%d R:%d)\n", oeo.button.left, oeo.button.middle, oeo.button.right);
                    
                    #define EMIT_BTN(orb_state, tracker, btn_num) \
                      if (orb_state != tracker) { \
                        GdkEvent *b_ev = gdk_event_new (orb_state ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE); \
                        if (core_pointer) { \
                          gdk_event_set_device (b_ev, core_pointer); \
                          gdk_event_set_source_device (b_ev, core_pointer); \
                        } \
                        b_ev->button.window = g_object_ref (item->window); \
                        b_ev->button.send_event = FALSE; \
                        b_ev->button.time = GDK_CURRENT_TIME; \
                        b_ev->button.x = last_mouse_x; \
                        b_ev->button.y = last_mouse_y; \
                        b_ev->button.x_root = last_mouse_x; \
                        b_ev->button.y_root = last_mouse_y; \
                        b_ev->button.button = btn_num; \
                        b_ev->button.state = 0; \
                        ((GdkEventPrivate *)b_ev)->flags |= 1; \
                        _gdk_orbital_display_deliver_event (display, b_ev); \
                        tracker = orb_state; \
                      }

                    EMIT_BTN(oeo.button.left, last_btn_left, 1);
                    EMIT_BTN(oeo.button.middle, last_btn_middle, 2);
                    EMIT_BTN(oeo.button.right, last_btn_right, 3);
                    #undef EMIT_BTN

                    // TODO: Proper grab handling instead of this
                    if (oeo.button.left == 0 && oeo.button.middle == 0 && oeo.button.right == 0)
                      {
                        GdkEvent *enter_event = gdk_event_new (GDK_ENTER_NOTIFY);
                        
                        if (core_pointer)
                          {
                            gdk_event_set_device (enter_event, core_pointer);
                            gdk_event_set_source_device (enter_event, core_pointer);
                          }
                          
                        enter_event->crossing.window = g_object_ref (item->window);
                        enter_event->crossing.send_event = FALSE;
                        enter_event->crossing.time = GDK_CURRENT_TIME;
                        
                        enter_event->crossing.x = last_mouse_x;
                        enter_event->crossing.y = last_mouse_y;
                        enter_event->crossing.x_root = last_mouse_x;
                        enter_event->crossing.y_root = last_mouse_y;
                        
                        enter_event->crossing.mode = GDK_CROSSING_UNGRAB; 
                        enter_event->crossing.detail = GDK_NOTIFY_ANCESTOR;
                        enter_event->crossing.focus = TRUE;
                        enter_event->crossing.state = 0;
                        
                        _gdk_orbital_display_deliver_event (display, enter_event);
                      }
                    event = NULL;
                    break;

                  case OrbEventOption_Scroll:
                    g_print ("[GTK EVENT]   -> SCROLL (y: %d)\n", oeo.scroll.y);
                    event = gdk_event_new (GDK_SCROLL);
                    if (core_pointer) {
                      gdk_event_set_device (event, core_pointer);
                      gdk_event_set_source_device (event, core_pointer);
                    }
                    event->scroll.window = g_object_ref (item->window);
                    event->scroll.send_event = FALSE;
                    event->scroll.direction = GDK_SCROLL_SMOOTH;
                    event->scroll.time = GDK_CURRENT_TIME;
                    // TODO: anyway to adjust sensitivity?
                    event->scroll.delta_x = (gdouble)oeo.scroll.x / 2;
                    event->scroll.delta_y = -(gdouble)oeo.scroll.y / 2;
                    event->scroll.x = last_mouse_x;
                    event->scroll.y = last_mouse_y;
                    event->scroll.x_root = last_mouse_x;
                    event->scroll.y_root = last_mouse_y;
                    event->scroll.state = 0;
                    // gdk_event_set_screen (event, display_orbital->screen);
                    // gdk_event_set_seat (event, seat);
                      
                    break;
                  case OrbEventOption_Hover:
                    {
                      gboolean is_hovered = oeo.hover.entered ? TRUE : FALSE;
                      g_print ("[GTK EVENT]   -> HOVER (in: %d)\n", is_hovered);

                      event = gdk_event_new (is_hovered ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY);

                      if (core_pointer) 
                        {
                          gdk_event_set_device (event, core_pointer);
                          gdk_event_set_source_device (event, core_pointer);
                        }

                      event->crossing.window = g_object_ref (item->window);
                      event->crossing.send_event = FALSE;
                      event->crossing.time = GDK_CURRENT_TIME;

                      event->crossing.x = last_mouse_x;
                      event->crossing.y = last_mouse_y;
                      event->crossing.x_root = last_mouse_x;
                      event->crossing.y_root = last_mouse_y;

                      event->crossing.mode = GDK_CROSSING_NORMAL;
                      event->crossing.detail = GDK_NOTIFY_ANCESTOR;
                      
                      event->crossing.focus = TRUE; 
                      event->crossing.state = 0;
                    }
                    break;
                  case OrbEventOption_Move:
                  case OrbEventOption_Screen:
                  case OrbEventOption_Unknown:
                  default:
                    break;
                }

              if (event)
                {
                  _gdk_orbital_display_deliver_event (display, event);
                }

              oeo = orb_events_next (event_iter);
            }

          orb_events_destroy (event_iter);
          item->pfd.revents = 0;
        }
    }
}

// similar to winit physicalkey_to_scancode
// https://github.com/rust-windowing/winit/blob/master/winit-common/src/xkb/keymap.rs
guint
_gdk_orbital_get_xkb_keycode (uint32_t scancode)
{
  switch (scancode)
    {
      case ORB_KEY_ESC:           return 1 + 8;
      case ORB_KEY_1:             return 2 + 8;
      case ORB_KEY_2:             return 3 + 8;
      case ORB_KEY_3:             return 4 + 8;
      case ORB_KEY_4:             return 5 + 8;
      case ORB_KEY_5:             return 6 + 8;
      case ORB_KEY_6:             return 7 + 8;
      case ORB_KEY_7:             return 8 + 8;
      case ORB_KEY_8:             return 9 + 8;
      case ORB_KEY_9:             return 10 + 8;
      case ORB_KEY_0:             return 11 + 8;
      case ORB_KEY_MINUS:         return 12 + 8;
      case ORB_KEY_EQUALS:        return 13 + 8;
      case ORB_KEY_BKSP:          return 14 + 8;
      case ORB_KEY_TAB:           return 15 + 8;
      case ORB_KEY_Q:             return 16 + 8;
      case ORB_KEY_W:             return 17 + 8;
      case ORB_KEY_E:             return 18 + 8;
      case ORB_KEY_R:             return 19 + 8;
      case ORB_KEY_T:             return 20 + 8;
      case ORB_KEY_Y:             return 21 + 8;
      case ORB_KEY_U:             return 22 + 8;
      case ORB_KEY_I:             return 23 + 8;
      case ORB_KEY_O:             return 24 + 8;
      case ORB_KEY_P:             return 25 + 8;
      case ORB_KEY_BRACE_OPEN:    return 26 + 8;
      case ORB_KEY_BRACE_CLOSE:   return 27 + 8;
      case ORB_KEY_ENTER:         return 28 + 8;
      case ORB_KEY_LEFT_CTRL:     return 29 + 8;
      case ORB_KEY_A:             return 30 + 8;
      case ORB_KEY_S:             return 31 + 8;
      case ORB_KEY_D:             return 32 + 8;
      case ORB_KEY_F:             return 33 + 8;
      case ORB_KEY_G:             return 34 + 8;
      case ORB_KEY_H:             return 35 + 8;
      case ORB_KEY_J:             return 36 + 8;
      case ORB_KEY_K:             return 37 + 8;
      case ORB_KEY_L:             return 38 + 8;
      case ORB_KEY_SEMICOLON:     return 39 + 8;
      case ORB_KEY_QUOTE:         return 40 + 8;
      case ORB_KEY_TICK:          return 41 + 8;
      case ORB_KEY_LEFT_SHIFT:    return 42 + 8;
      case ORB_KEY_BACKSLASH:     return 43 + 8;
      case ORB_KEY_Z:             return 44 + 8;
      case ORB_KEY_X:             return 45 + 8;
      case ORB_KEY_C:             return 46 + 8;
      case ORB_KEY_V:             return 47 + 8;
      case ORB_KEY_B:             return 48 + 8;
      case ORB_KEY_N:             return 49 + 8;
      case ORB_KEY_M:             return 50 + 8;
      case ORB_KEY_COMMA:         return 51 + 8;
      case ORB_KEY_PERIOD:        return 52 + 8;
      case ORB_KEY_SLASH:         return 53 + 8;
      case ORB_KEY_RIGHT_SHIFT:   return 54 + 8;
      case ORB_KEY_NUM_ASTERISK:  return 55 + 8;
      case ORB_KEY_ALT:           return 56 + 8;
      case ORB_KEY_SPACE:         return 57 + 8;
      case ORB_KEY_CAPS:          return 58 + 8;
      case ORB_KEY_F1:            return 59 + 8;
      case ORB_KEY_F2:            return 60 + 8;
      case ORB_KEY_F3:            return 61 + 8;
      case ORB_KEY_F4:            return 62 + 8;
      case ORB_KEY_F5:            return 63 + 8;
      case ORB_KEY_F6:            return 64 + 8;
      case ORB_KEY_F7:            return 65 + 8;
      case ORB_KEY_F8:            return 66 + 8;
      case ORB_KEY_F9:            return 67 + 8;
      case ORB_KEY_F10:           return 68 + 8;
      case ORB_KEY_NUM:           return 69 + 8;
      case ORB_KEY_SCROLL:        return 70 + 8;
      case ORB_KEY_NUM_7:         return 71 + 8;
      case ORB_KEY_NUM_8:         return 72 + 8;
      case ORB_KEY_NUM_9:         return 73 + 8;
      case ORB_KEY_NUM_MINUS:     return 74 + 8;
      case ORB_KEY_NUM_4:         return 75 + 8;
      case ORB_KEY_NUM_5:         return 76 + 8;
      case ORB_KEY_NUM_6:         return 77 + 8;
      case ORB_KEY_NUM_PLUS:      return 78 + 8;
      case ORB_KEY_NUM_1:         return 79 + 8;
      case ORB_KEY_NUM_2:         return 80 + 8;
      case ORB_KEY_NUM_3:         return 81 + 8;
      case ORB_KEY_NUM_0:         return 82 + 8;
      case ORB_KEY_NUM_PERIOD:    return 83 + 8;
      case ORB_KEY_F11:           return 87 + 8;
      case ORB_KEY_F12:           return 88 + 8;
      case ORB_KEY_NUM_ENTER:     return 96 + 8;
      case ORB_KEY_RIGHT_CTRL:    return 97 + 8;
      case ORB_KEY_NUM_SLASH:     return 98 + 8;
      case ORB_KEY_PRTSC:         return 99 + 8;
      case ORB_KEY_HOME:          return 102 + 8;
      case ORB_KEY_UP:            return 103 + 8;
      case ORB_KEY_PGUP:          return 104 + 8;
      case ORB_KEY_LEFT:          return 105 + 8;
      case ORB_KEY_RIGHT:         return 106 + 8;
      case ORB_KEY_END:           return 107 + 8;
      case ORB_KEY_DOWN:          return 108 + 8;
      case ORB_KEY_PGDN:          return 109 + 8;
      case ORB_KEY_INS:           return 110 + 8;
      case ORB_KEY_DEL:           return 111 + 8;
      case ORB_KEY_LEFT_SUPER:    return 125 + 8;
      case ORB_KEY_RIGHT_SUPER:   return 126 + 8;
      case ORB_KEY_APP:           return 127 + 8;
      case ORB_KEY_POWER:         return 116 + 8;
      case ORB_KEY_SLEEP:         return 142 + 8;

      default:
        return scancode + 8;
    }
}
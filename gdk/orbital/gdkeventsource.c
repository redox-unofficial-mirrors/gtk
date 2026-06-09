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
                    event = gdk_event_new (GDK_CONFIGURE);
                    event->configure.window = g_object_ref (item->window);
                    event->configure.send_event = FALSE;
                    event->configure.width = oeo.resize.width;
                    event->configure.height = oeo.resize.height;
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
                    event->key.hardware_keycode = oeo.key.scancode;
                    gdk_event_set_scancode (event, oeo.key.scancode);
                    
                    event->key.keyval = _gdk_orbital_get_keysym_from_scancode (oeo.key.scancode);
                    
                    if (event->key.keyval == GDK_KEY_Shift_L || event->key.keyval == GDK_KEY_Shift_R)
                      {
                        if (oeo.key.pressed) current_modifiers |= GDK_SHIFT_MASK;
                        else current_modifiers &= ~GDK_SHIFT_MASK;
                      }
                    else if (event->key.keyval == GDK_KEY_Control_L || event->key.keyval == GDK_KEY_Control_R)
                      {
                        if (oeo.key.pressed) current_modifiers |= GDK_CONTROL_MASK;
                        else current_modifiers &= ~GDK_CONTROL_MASK;
                      }
                    else if (event->key.keyval == GDK_KEY_Alt_L || event->key.keyval == GDK_KEY_Alt_R)
                      {
                        if (oeo.key.pressed) current_modifiers |= GDK_MOD1_MASK;
                        else current_modifiers &= ~GDK_MOD1_MASK;
                      }

                    event->key.state = current_modifiers;
                    break;

                  case OrbEventOption_Mouse:
                    g_print ("[GTK EVENT]   -> MOUSE MOTION (x: %d, y: %d)\n", oeo.mouse.x, oeo.mouse.y);
                    
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
                    event->scroll.direction = GDK_SCROLL_SMOOTH;
                    event->scroll.time = GDK_CURRENT_TIME;
                    event->scroll.delta_x = (gdouble)oeo.scroll.x;
                    event->scroll.delta_y = (gdouble)oeo.scroll.y;
                    event->scroll.is_stop = FALSE;
                    
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

guint
_gdk_orbital_get_keysym_from_scancode (uint32_t scancode)
{
  switch (scancode)
    {
      case ORB_KEY_ESC:           return GDK_KEY_Escape;
      case ORB_KEY_F1:            return GDK_KEY_F1;
      case ORB_KEY_F2:            return GDK_KEY_F2;
      case ORB_KEY_F3:            return GDK_KEY_F3;
      case ORB_KEY_F4:            return GDK_KEY_F4;
      case ORB_KEY_F5:            return GDK_KEY_F5;
      case ORB_KEY_F6:            return GDK_KEY_F6;
      case ORB_KEY_F7:            return GDK_KEY_F7;
      case ORB_KEY_F8:            return GDK_KEY_F8;
      case ORB_KEY_F9:            return GDK_KEY_F9;
      case ORB_KEY_F10:           return GDK_KEY_F10;
      case ORB_KEY_F11:           return GDK_KEY_F11;
      case ORB_KEY_F12:           return GDK_KEY_F12;
      case ORB_KEY_PRTSC:         return GDK_KEY_Print;
      case ORB_KEY_SCROLL:        return GDK_KEY_Scroll_Lock;
      case ORB_KEY_NUM:           return GDK_KEY_Num_Lock;
      case ORB_KEY_1:             return GDK_KEY_1;
      case ORB_KEY_2:             return GDK_KEY_2;
      case ORB_KEY_3:             return GDK_KEY_3;
      case ORB_KEY_4:             return GDK_KEY_4;
      case ORB_KEY_5:             return GDK_KEY_5;
      case ORB_KEY_6:             return GDK_KEY_6;
      case ORB_KEY_7:             return GDK_KEY_7;
      case ORB_KEY_8:             return GDK_KEY_8;
      case ORB_KEY_9:             return GDK_KEY_9;
      case ORB_KEY_0:             return GDK_KEY_0;
      case ORB_KEY_A:             return GDK_KEY_A;
      case ORB_KEY_B:             return GDK_KEY_B;
      case ORB_KEY_C:             return GDK_KEY_C;
      case ORB_KEY_D:             return GDK_KEY_D;
      case ORB_KEY_E:             return GDK_KEY_E;
      case ORB_KEY_F:             return GDK_KEY_F;
      case ORB_KEY_G:             return GDK_KEY_G;
      case ORB_KEY_H:             return GDK_KEY_H;
      case ORB_KEY_I:             return GDK_KEY_I;
      case ORB_KEY_J:             return GDK_KEY_J;
      case ORB_KEY_K:             return GDK_KEY_K;
      case ORB_KEY_L:             return GDK_KEY_L;
      case ORB_KEY_M:             return GDK_KEY_M;
      case ORB_KEY_N:             return GDK_KEY_N;
      case ORB_KEY_O:             return GDK_KEY_O;
      case ORB_KEY_P:             return GDK_KEY_P;
      case ORB_KEY_Q:             return GDK_KEY_Q;
      case ORB_KEY_R:             return GDK_KEY_R;
      case ORB_KEY_S:             return GDK_KEY_S;
      case ORB_KEY_T:             return GDK_KEY_T;
      case ORB_KEY_U:             return GDK_KEY_U;
      case ORB_KEY_V:             return GDK_KEY_V;
      case ORB_KEY_W:             return GDK_KEY_W;
      case ORB_KEY_X:             return GDK_KEY_X;
      case ORB_KEY_Y:             return GDK_KEY_Y;
      case ORB_KEY_Z:             return GDK_KEY_Z;
      case ORB_KEY_TAB:           return GDK_KEY_Tab;
      case ORB_KEY_CAPS:          return GDK_KEY_Caps_Lock;
      case ORB_KEY_ENTER:         return GDK_KEY_Return;
      case ORB_KEY_BKSP:          return GDK_KEY_BackSpace;
      case ORB_KEY_SPACE:         return GDK_KEY_space;
      case ORB_KEY_MINUS:         return GDK_KEY_minus;
      case ORB_KEY_EQUALS:        return GDK_KEY_equal;
      case ORB_KEY_BRACE_OPEN:    return GDK_KEY_bracketleft;
      case ORB_KEY_BRACE_CLOSE:   return GDK_KEY_bracketright;
      case ORB_KEY_BACKSLASH:     return GDK_KEY_backslash;
      case ORB_KEY_SEMICOLON:     return GDK_KEY_semicolon;
      case ORB_KEY_QUOTE:         return GDK_KEY_apostrophe;
      case ORB_KEY_COMMA:         return GDK_KEY_comma;
      case ORB_KEY_PERIOD:        return GDK_KEY_period;
      case ORB_KEY_SLASH:         return GDK_KEY_slash;
      case ORB_KEY_TICK:          return GDK_KEY_grave;
      case ORB_KEY_INS:           return GDK_KEY_Insert;
      case ORB_KEY_DEL:           return GDK_KEY_Delete;
      case ORB_KEY_HOME:          return GDK_KEY_Home;
      case ORB_KEY_END:           return GDK_KEY_End;
      case ORB_KEY_PGUP:          return GDK_KEY_Page_Up;
      case ORB_KEY_PGDN:          return GDK_KEY_Page_Down;
      case ORB_KEY_UP:            return GDK_KEY_Up;
      case ORB_KEY_DOWN:          return GDK_KEY_Down;
      case ORB_KEY_LEFT:          return GDK_KEY_Left;
      case ORB_KEY_RIGHT:         return GDK_KEY_Right;
      case ORB_KEY_NUM_0:         return GDK_KEY_KP_0;
      case ORB_KEY_NUM_1:         return GDK_KEY_KP_1;
      case ORB_KEY_NUM_2:         return GDK_KEY_KP_2;
      case ORB_KEY_NUM_3:         return GDK_KEY_KP_3;
      case ORB_KEY_NUM_4:         return GDK_KEY_KP_4;
      case ORB_KEY_NUM_5:         return GDK_KEY_KP_5;
      case ORB_KEY_NUM_6:         return GDK_KEY_KP_6;
      case ORB_KEY_NUM_7:         return GDK_KEY_KP_7;
      case ORB_KEY_NUM_8:         return GDK_KEY_KP_8;
      case ORB_KEY_NUM_9:         return GDK_KEY_KP_9;
      case ORB_KEY_NUM_ASTERISK:  return GDK_KEY_KP_Multiply;
      case ORB_KEY_NUM_MINUS:     return GDK_KEY_KP_Subtract;
      case ORB_KEY_NUM_PLUS:      return GDK_KEY_KP_Add;
      case ORB_KEY_NUM_PERIOD:    return GDK_KEY_KP_Decimal;
      case ORB_KEY_NUM_SLASH:     return GDK_KEY_KP_Divide;
      case ORB_KEY_NUM_ENTER:     return GDK_KEY_KP_Enter;
      case ORB_KEY_LEFT_SHIFT:    return GDK_KEY_Shift_L;
      case ORB_KEY_RIGHT_SHIFT:   return GDK_KEY_Shift_R;
      case ORB_KEY_LEFT_CTRL:     return GDK_KEY_Control_L;
      case ORB_KEY_RIGHT_CTRL:    return GDK_KEY_Control_R;
      case ORB_KEY_ALT:           return GDK_KEY_Alt_L;
      case ORB_KEY_LEFT_SUPER:    return GDK_KEY_Super_L;
      case ORB_KEY_RIGHT_SUPER:   return GDK_KEY_Super_R;
      case ORB_KEY_APP:           return GDK_KEY_Menu;
      case ORB_KEY_POWER:         return GDK_KEY_PowerOff;
      case ORB_KEY_SLEEP:         return GDK_KEY_Sleep;

      default:
        break;
    }

  return GDK_KEY_VoidSymbol;
}

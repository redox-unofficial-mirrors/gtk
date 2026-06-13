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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <gdk/gdkwindow.h>
#include <gdk/gdktypes.h>
#include "gdkprivate-orbital.h"
#include "gdkseat-orbital.h"
#include "gdkorbital.h"
#include "gdkkeysyms.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanagerprivate.h"
#include "gdkseatprivate.h"

#include "orbital.h"

typedef struct _GdkOrbitalPointerFrameData GdkOrbitalPointerFrameData;
typedef struct _GdkOrbitalPointerData GdkOrbitalPointerData;

struct _GdkOrbitalPointerData
{
  GdkWindow *focus;

  double surface_x, surface_y;

  GdkModifierType button_modifiers;
  uint32_t time;

  GdkWindow *grab_window;
  uint32_t grab_time;

  GdkCursor *cursor;
  guint current_output_scale;
};

struct _GdkOrbitalSeat
{
  GdkSeat parent_instance;

  guint32 id;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GdkDevice *pointer;
  GdkDevice *keyboard;

  GdkCursor *cursor;
  GdkKeymap *keymap;

  GdkOrbitalPointerData pointer_info;

  GdkModifierType key_modifiers;
  GdkWindow *keyboard_focus;
  GdkWindow *grab_window;
  uint32_t grab_time;

  GdkCursor *grab_cursor;
};

G_DEFINE_TYPE (GdkOrbitalSeat, gdk_orbital_seat, GDK_TYPE_SEAT)

struct _GdkOrbitalDevice
{
  GdkDevice parent_instance;
  GdkOrbitalPointerData *pointer;
};

struct _GdkOrbitalDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkOrbitalDevice, gdk_orbital_device, GDK_TYPE_DEVICE)

#define GDK_TYPE_ORBITAL_DEVICE_MANAGER        (gdk_orbital_device_manager_get_type ())
#define GDK_ORBITAL_DEVICE_MANAGER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_ORBITAL_DEVICE_MANAGER, GdkOrbitalDeviceManager))
#define GDK_ORBITAL_DEVICE_MANAGER_CLASS(c)    (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_ORBITAL_DEVICE_MANAGER, GdkOrbitalDeviceManagerClass))

typedef struct _GdkOrbitalDeviceManager GdkOrbitalDeviceManager;
typedef struct _GdkOrbitalDeviceManagerClass GdkOrbitalDeviceManagerClass;

struct _GdkOrbitalDeviceManager {
  GdkDeviceManager parent_object;
  GList *devices;
};

struct _GdkOrbitalDeviceManagerClass {
  GdkDeviceManagerClass parent_class;
};

GType gdk_orbital_device_manager_get_type (void);

G_DEFINE_TYPE (GdkOrbitalDeviceManager,
               gdk_orbital_device_manager, GDK_TYPE_DEVICE_MANAGER)

static gboolean
gdk_orbital_device_get_history (GdkDevice      *device,
                                GdkWindow      *window,
                                guint32         start,
                                guint32         stop,
                                GdkTimeCoord ***events,
                                gint           *n_events)
{
  return FALSE;
}

static void
gdk_orbital_device_get_state (GdkDevice       *device,
                              GdkWindow       *window,
                              gdouble         *axes,
                              GdkModifierType *mask)
{
  gdouble x, y;

  gdk_window_get_device_position_double (window, device, &x, &y, mask);

  if (axes)
    {
      axes[0] = x;
      axes[1] = y;
    }
}

static gboolean
gdk_orbital_device_update_window_cursor (GdkDevice *device)
{
  return G_SOURCE_REMOVE;
}

static void
gdk_orbital_device_set_window_cursor (GdkDevice *device,
                                      GdkWindow *window,
                                      GdkCursor *cursor)
{
  GdkOrbitalSeat *seat = GDK_ORBITAL_SEAT (gdk_device_get_seat (device));
  GdkOrbitalPointerData *pointer = GDK_ORBITAL_DEVICE (device)->pointer;

  if (seat->grab_cursor)
    cursor = seat->grab_cursor;

  if (cursor == pointer->cursor)
    return;

  if (pointer->cursor)
    g_object_unref (pointer->cursor);

  pointer->cursor = cursor ? g_object_ref (cursor) : NULL;

  gdk_orbital_device_update_window_cursor (device);
}

static void
gdk_orbital_device_warp (GdkDevice *device,
                         GdkScreen *screen,
                         gdouble    x,
                         gdouble    y)
{
}

static void
get_coordinates (GdkDevice *device,
                 double    *x,
                 double    *y,
                 double    *x_root,
                 double    *y_root)
{
  GdkOrbitalPointerData *pointer = GDK_ORBITAL_DEVICE (device)->pointer;
  int root_x, root_y;

  if (x)
    *x = pointer->surface_x;
  if (y)
    *y = pointer->surface_y;

  if (pointer->focus)
    {
      gdk_window_get_root_coords (pointer->focus,
                                  pointer->surface_x,
                                  pointer->surface_y,
                                  &root_x, &root_y);
    }
  else
    {
      root_x = pointer->surface_x;
      root_y = pointer->surface_y;
    }

  if (x_root)
    *x_root = root_x;
  if (y_root)
    *y_root = root_y;
}

static GdkModifierType
device_get_modifiers (GdkDevice *device)
{
  GdkOrbitalSeat *seat = GDK_ORBITAL_SEAT (gdk_device_get_seat (device));
  GdkOrbitalPointerData *pointer = GDK_ORBITAL_DEVICE (device)->pointer;
  GdkModifierType mask = seat->key_modifiers;

  if (pointer)
    mask |= pointer->button_modifiers;

  return mask;
}

static void
gdk_orbital_device_query_state (GdkDevice        *device,
                                GdkWindow        *window,
                                GdkWindow       **root_window,
                                GdkWindow       **child_window,
                                gdouble          *root_x,
                                gdouble          *root_y,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask)
{
  GdkOrbitalSeat *seat;
  GdkOrbitalPointerData *pointer;
  GdkScreen *default_screen;

  seat = GDK_ORBITAL_SEAT (gdk_device_get_seat (device));
  pointer = GDK_ORBITAL_DEVICE (device)->pointer;
  default_screen = gdk_display_get_default_screen (seat->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    *child_window = g_list_find (window->children, pointer->focus) ? pointer->focus : NULL;
  if (mask)
    *mask = device_get_modifiers (device);

  get_coordinates (device, win_x, win_y, root_x, root_y);
}

GdkWindow *
gdk_orbital_device_get_focus (GdkDevice *device)
{
  GdkOrbitalSeat *orbital_seat = GDK_ORBITAL_SEAT (gdk_device_get_seat (device));
  GdkOrbitalPointerData *pointer;

  if (device == orbital_seat->master_keyboard)
    return orbital_seat->keyboard_focus;
  else
    {
      pointer = GDK_ORBITAL_DEVICE (device)->pointer;
      if (pointer)
        return pointer->focus;
    }

  return NULL;
}

static GdkGrabStatus
gdk_orbital_device_grab (GdkDevice    *device,
                         GdkWindow    *window,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkWindow    *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkOrbitalPointerData *pointer = GDK_ORBITAL_DEVICE (device)->pointer;
  void *orb_window = _gdk_orbital_window_get_native (window);

  if (!pointer) {
    g_print("no pointer on gdk_orbital_device_grab!\n");
    return GDK_GRAB_BROKEN;
  }

  if (pointer->grab_window != NULL && time_ != 0 && pointer->grab_time > time_)
    return GDK_GRAB_ALREADY_GRABBED;

  if (orb_window)
    {
      orb_window_set_mouse_grab (orb_window, TRUE);
    }

  pointer->grab_window = window;
  pointer->grab_time = time_ != 0 ? time_ : pointer->time;
  
  if (cursor) 
    {
      gdk_orbital_device_update_window_cursor (device);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_orbital_device_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkOrbitalPointerData *pointer = GDK_ORBITAL_DEVICE (device)->pointer;
  
  if (pointer->grab_window)
    {
      void *orb_window = _gdk_orbital_window_get_native (pointer->grab_window);
      if (orb_window)
        {
          orb_window_set_mouse_grab (orb_window, FALSE);
        }
    }

  pointer->grab_window = NULL;
  gdk_orbital_device_update_window_cursor (device);
}

static GdkWindow *
gdk_orbital_device_window_at_position (GdkDevice       *device,
                                       gdouble         *win_x,
                                       gdouble         *win_y,
                                       GdkModifierType *mask,
                                       gboolean         get_toplevel)
{
  GdkOrbitalPointerData *pointer;

  pointer = GDK_ORBITAL_DEVICE(device)->pointer;

  if (!pointer || !pointer->focus)
    return NULL;

  if (win_x)
    *win_x = pointer->surface_x;
  if (win_y)
    *win_y = pointer->surface_y;
  if (mask)
    *mask = device_get_modifiers (device);

  return pointer->focus;
}

static void
gdk_orbital_device_select_window_events (GdkDevice    *device,
                                         GdkWindow    *window,
                                         GdkEventMask  event_mask)
{
}

static void
gdk_orbital_device_class_init (GdkOrbitalDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_orbital_device_get_history;
  device_class->get_state = gdk_orbital_device_get_state;
  device_class->set_window_cursor = gdk_orbital_device_set_window_cursor;
  device_class->warp = gdk_orbital_device_warp;
  device_class->query_state = gdk_orbital_device_query_state;
  device_class->grab = gdk_orbital_device_grab;
  device_class->ungrab = gdk_orbital_device_ungrab;
  device_class->window_at_position = gdk_orbital_device_window_at_position;
  device_class->select_window_events = gdk_orbital_device_select_window_events;
}

static void
gdk_orbital_device_init (GdkOrbitalDevice *device_core)
{
  GdkDevice *device = GDK_DEVICE (device_core);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

GdkKeymap *
_gdk_orbital_device_get_keymap (GdkDevice *device)
{
  GdkOrbitalSeat *seat = GDK_ORBITAL_SEAT (gdk_device_get_seat (device));
  return seat->keymap;
}

static void
init_devices (GdkOrbitalSeat *seat)
{
  GdkOrbitalDeviceManager *device_manager = GDK_ORBITAL_DEVICE_MANAGER (seat->device_manager);

  /* pointer */
  seat->master_pointer = g_object_new (GDK_TYPE_ORBITAL_DEVICE,
                                       "name", "Orbital Core Pointer",
                                       "type", GDK_DEVICE_TYPE_MASTER,
                                       "input-source", GDK_SOURCE_MOUSE,
                                       "input-mode", GDK_MODE_SCREEN,
                                       "has-cursor", TRUE,
                                       "display", seat->display,
                                       "device-manager", device_manager,
                                       "seat", seat,
                                       NULL);

  GDK_ORBITAL_DEVICE (seat->master_pointer)->pointer = &seat->pointer_info;

  device_manager->devices = g_list_prepend (device_manager->devices, seat->master_pointer);
  g_signal_emit_by_name (device_manager, "device-added", seat->master_pointer);

  /* keyboard */
  seat->master_keyboard = g_object_new (GDK_TYPE_ORBITAL_DEVICE,
                                        "name", "Orbital Core Keyboard",
                                        "type", GDK_DEVICE_TYPE_MASTER,
                                        "input-source", GDK_SOURCE_KEYBOARD,
                                        "input-mode", GDK_MODE_SCREEN,
                                        "has-cursor", FALSE,
                                        "display", seat->display,
                                        "device-manager", device_manager,
                                        "seat", seat,
                                        NULL);
  _gdk_device_reset_axes (seat->master_keyboard);

  device_manager->devices = g_list_prepend (device_manager->devices, seat->master_keyboard);
  g_signal_emit_by_name (device_manager, "device-added", seat->master_keyboard);

  /* link both */
  _gdk_device_set_associated_device (seat->master_pointer, seat->master_keyboard);
  _gdk_device_set_associated_device (seat->master_keyboard, seat->master_pointer);
}

static void
gdk_orbital_seat_finalize (GObject *object)
{
  GdkOrbitalSeat *seat = GDK_ORBITAL_SEAT (object);

  g_clear_object (&seat->keymap);
  g_clear_object (&seat->pointer_info.focus);
  g_clear_object (&seat->pointer_info.cursor);

  G_OBJECT_CLASS (gdk_orbital_seat_parent_class)->finalize (object);
}

static GdkSeatCapabilities
gdk_orbital_seat_get_capabilities (GdkSeat *seat)
{
  return GDK_SEAT_CAPABILITY_POINTER | GDK_SEAT_CAPABILITY_KEYBOARD;
}

static GdkGrabStatus
gdk_orbital_seat_grab (GdkSeat                *seat,
                       GdkWindow              *window,
                       GdkSeatCapabilities     capabilities,
                       gboolean                owner_events,
                       GdkCursor              *cursor,
                       const GdkEvent         *event,
                       GdkSeatGrabPrepareFunc  prepare_func,
                       gpointer                prepare_func_data)
{
  GdkOrbitalSeat *orbital_seat = GDK_ORBITAL_SEAT (seat);
  
  if (prepare_func)
    (prepare_func) (seat, window, prepare_func_data);

  if (!gdk_window_is_visible (window))
    return GDK_GRAB_NOT_VIEWABLE;

  if (capabilities & GDK_SEAT_CAPABILITY_POINTER) {
    gdk_orbital_device_grab (orbital_seat->master_pointer, window, owner_events, 
                             0, NULL, cursor, GDK_CURRENT_TIME);
  }

  if (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD) {
    gdk_orbital_device_grab (orbital_seat->master_keyboard, window, owner_events, 
                             0, NULL, NULL, GDK_CURRENT_TIME);
  }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_orbital_seat_ungrab (GdkSeat *seat)
{
  GdkOrbitalSeat *orbital_seat = GDK_ORBITAL_SEAT (seat);

  g_clear_object (&orbital_seat->grab_cursor);
  
  if (orbital_seat->master_pointer)
    gdk_orbital_device_ungrab (orbital_seat->master_pointer, GDK_CURRENT_TIME);

  if (orbital_seat->master_keyboard)
    gdk_orbital_device_ungrab (orbital_seat->master_keyboard, GDK_CURRENT_TIME);
}

static GdkDevice *
gdk_orbital_seat_get_master (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  GdkOrbitalSeat *orbital_seat = GDK_ORBITAL_SEAT (seat);

  if (capabilities == GDK_SEAT_CAPABILITY_POINTER)
    return orbital_seat->master_pointer;
  else if (capabilities == GDK_SEAT_CAPABILITY_KEYBOARD)
    return orbital_seat->master_keyboard;

  return NULL;
}

static GList *
gdk_orbital_seat_get_slaves (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  return NULL;
}

static void
gdk_orbital_seat_class_init (GdkOrbitalSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->finalize = gdk_orbital_seat_finalize;

  seat_class->get_capabilities = gdk_orbital_seat_get_capabilities;
  seat_class->grab = gdk_orbital_seat_grab;
  seat_class->ungrab = gdk_orbital_seat_ungrab;
  seat_class->get_master = gdk_orbital_seat_get_master;
  seat_class->get_slaves = gdk_orbital_seat_get_slaves;
}

static void
gdk_orbital_seat_init (GdkOrbitalSeat *seat)
{
}

void
_gdk_orbital_device_manager_add_seat (GdkDeviceManager *device_manager,
                                                        guint32           id,
                                                        gpointer          pseat)
{
  GdkDisplay *display = gdk_device_manager_get_display (device_manager);
  GdkOrbitalSeat *seat;

  seat = g_object_new (GDK_TYPE_ORBITAL_SEAT, "display", display, NULL);
  
  seat->keymap = _gdk_orbital_keymap_new ();
  seat->display = display;
  seat->device_manager = device_manager;

  init_devices (seat);
  
  seat->pointer_info.current_output_scale = 1;

  gdk_display_add_seat (display, GDK_SEAT (seat));
}

static void
free_device (gpointer data)
{
  g_object_unref (data);
}

static void
gdk_orbital_device_manager_finalize (GObject *object)
{
  GdkOrbitalDeviceManager *device_manager = GDK_ORBITAL_DEVICE_MANAGER (object);
  g_list_free_full (device_manager->devices, free_device);
  G_OBJECT_CLASS (gdk_orbital_device_manager_parent_class)->finalize (object);
}

static GList *
gdk_orbital_device_manager_list_devices (GdkDeviceManager *device_manager,
                                         GdkDeviceType     type)
{
  GdkOrbitalDeviceManager *orbital_device_manager = GDK_ORBITAL_DEVICE_MANAGER (device_manager);
  GList *devices = NULL, *l;

  for (l = orbital_device_manager->devices; l; l = l->next)
    {
      if (gdk_device_get_device_type (l->data) == type)
        devices = g_list_prepend (devices, l->data);
    }

  return devices;
}

static GdkDevice *
gdk_orbital_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkOrbitalDeviceManager *orbital_device_manager = GDK_ORBITAL_DEVICE_MANAGER (device_manager);
  GdkOrbitalSeat *seat;
  GdkDevice *device;

  device = orbital_device_manager->devices->data;
  seat = GDK_ORBITAL_SEAT (gdk_device_get_seat (device));

  return seat->master_pointer;
}

static void
gdk_orbital_device_manager_class_init (GdkOrbitalDeviceManagerClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_orbital_device_manager_finalize;
  device_manager_class->list_devices = gdk_orbital_device_manager_list_devices;
  device_manager_class->get_client_pointer = gdk_orbital_device_manager_get_client_pointer;
}

static void
gdk_orbital_device_manager_init (GdkOrbitalDeviceManager *device_manager)
{
}

GdkDeviceManager *
_gdk_orbital_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_ORBITAL_DEVICE_MANAGER,
                       "display", display,
                       NULL);
}

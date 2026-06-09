/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2026 Redox OS Contributors
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include "gdkorbital.h"
#include "gdkdisplay.h"
#include "gdkdisplay-orbital.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager.h"
#include "gdkkeysprivate.h"
#include "gdkprivate-orbital.h"
#include "gdk-private.h"

#include "orbital.h" 

G_DEFINE_TYPE (GdkOrbitalDisplay, gdk_orbital_display, GDK_TYPE_DISPLAY)

GdkDisplay *
_gdk_orbital_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkOrbitalDisplay *display_orbital;

  GDK_NOTE (MISC, g_message ("opening Orbital display %s", display_name ? display_name : ""));

  display = g_object_new (GDK_TYPE_ORBITAL_DISPLAY, NULL);
  display->device_manager = _gdk_orbital_device_manager_new (display);

  display_orbital = GDK_ORBITAL_DISPLAY (display);
  
  display_orbital->screen = _gdk_orbital_screen_new (display);
  _gdk_orbital_device_manager_add_seat(display->device_manager, 0, NULL);
  display_orbital->event_source = _gdk_orbital_display_event_source_new (display);
  display_orbital->selection = gdk_orbital_selection_new ();
  _gdk_orbital_display_init_cursors (display_orbital);

  g_signal_emit_by_name (display, "opened");

  return display;
}

static void
gdk_orbital_display_dispose (GObject *object)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (object);

  _gdk_screen_close (display_orbital->screen);

  if (display_orbital->event_source)
    {
      g_source_destroy (display_orbital->event_source);
      g_source_unref (display_orbital->event_source);
      display_orbital->event_source = NULL;
    }

  if (display_orbital->selection)
    {
      gdk_orbital_selection_free (display_orbital->selection);
      display_orbital->selection = NULL;
    }

  G_OBJECT_CLASS (gdk_orbital_display_parent_class)->dispose (object);
}

static void
gdk_orbital_display_finalize (GObject *object)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (object);

  g_object_unref (display_orbital->screen);
  g_ptr_array_free (display_orbital->monitors, TRUE);

  G_OBJECT_CLASS (gdk_orbital_display_parent_class)->finalize (object);
}

static const gchar *
gdk_orbital_display_get_name (GdkDisplay *display)
{
  const gchar *name = g_getenv ("ORBITAL_DISPLAY");
  return name ? name : "orbital-0";
}

static GdkScreen *
gdk_orbital_display_get_default_screen (GdkDisplay *display)
{
  return GDK_ORBITAL_DISPLAY (display)->screen;
}

static void
gdk_orbital_display_sync (GdkDisplay *display)
{
}

static void
gdk_orbital_display_flush (GdkDisplay *display)
{
}

static gulong
gdk_orbital_display_get_next_serial (GdkDisplay *display)
{
  static gulong serial = 0;
  return ++serial;
}

void
gdk_orbital_display_system_bell (GdkDisplay *display,
                                 GdkWindow  *window)
{
}

static void
gdk_orbital_display_beep (GdkDisplay *display)
{
}

static void
gdk_orbital_display_init (GdkOrbitalDisplay *display)
{
  display->monitors = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
gdk_orbital_display_make_default (GdkDisplay *display)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);
  const gchar *startup_id;

  g_free (display_orbital->startup_notification_id);
  display_orbital->startup_notification_id = NULL;

  startup_id = gdk_get_desktop_startup_id ();
  if (startup_id)
    display_orbital->startup_notification_id = g_strdup (startup_id);
}

static gboolean
gdk_orbital_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static GdkWindow *
gdk_orbital_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return NULL;
}

static gboolean
gdk_orbital_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_orbital_display_request_selection_notification (GdkDisplay *display,
                                                    GdkAtom     selection)
{
    return FALSE;
}

static gboolean
gdk_orbital_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_orbital_display_store_clipboard (GdkDisplay    *display,
                                     GdkWindow     *clipboard_window,
                                     guint32        time_,
                                     const GdkAtom *targets,
                                     gint           n_targets)
{
}

static gboolean
gdk_orbital_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_orbital_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_orbital_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_orbital_display_before_process_all_updates (GdkDisplay *display)
{
}

static void
gdk_orbital_display_after_process_all_updates (GdkDisplay *display)
{
}

void
gdk_orbital_display_set_startup_notification_id (GdkDisplay *display,
                                                 const char *startup_id)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);

  g_free (display_orbital->startup_notification_id);
  display_orbital->startup_notification_id = g_strdup (startup_id);
}

static void
gdk_orbital_display_notify_startup_complete (GdkDisplay  *display,
                                             const gchar *startup_id)
{
}

static GdkKeymap *
_gdk_orbital_display_get_keymap (GdkDisplay *display)
{
  GdkDevice *core_keyboard = NULL;
  static GdkKeymap *tmp_keymap = NULL;

  core_keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (display));

  if (core_keyboard && tmp_keymap)
    {
      g_object_unref (tmp_keymap);
      tmp_keymap = NULL;
    }

  if (core_keyboard)
    return _gdk_orbital_device_get_keymap (core_keyboard);

  if (!tmp_keymap)
    tmp_keymap = _gdk_orbital_keymap_new ();

  return tmp_keymap;
}

static void
gdk_orbital_display_push_error_trap (GdkDisplay *display)
{
}

static gint
gdk_orbital_display_pop_error_trap (GdkDisplay *display,
                                    gboolean    ignored)
{
  return 0;
}

static int
gdk_orbital_display_get_n_monitors (GdkDisplay *display)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);
  return display_orbital->monitors->len;
}

static GdkMonitor *
gdk_orbital_display_get_monitor (GdkDisplay *display,
                                 int         monitor_num)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);

  if (monitor_num < 0 || monitor_num >= display_orbital->monitors->len)
    return NULL;

  return (GdkMonitor *)display_orbital->monitors->pdata[monitor_num];
}

static GdkMonitor *
gdk_orbital_display_get_monitor_at_window (GdkDisplay *display,
                                           GdkWindow  *window)
{
  return gdk_orbital_display_get_monitor (display, 0);
}


GdkOrbitalSelection *
gdk_orbital_display_get_selection (GdkDisplay *display)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);

  return display_orbital->selection;
}

gboolean
gdk_orbital_display_query_registry (GdkDisplay  *display,
				    const gchar *global)
{
  GdkOrbitalDisplay *display_orbital = GDK_ORBITAL_DISPLAY (display);
  GHashTableIter iter;
  gchar *value;

  g_return_val_if_fail (GDK_IS_ORBITAL_DISPLAY (display), FALSE);
  g_return_val_if_fail (global != NULL, FALSE);

  g_hash_table_iter_init (&iter, display_orbital->known_globals);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &value))
    {
      if (strcmp (value, global) == 0)
        return TRUE;
    }

  return FALSE;
}


static void
gdk_orbital_display_class_init (GdkOrbitalDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_orbital_display_dispose;
  object_class->finalize = gdk_orbital_display_finalize;

  display_class->window_type = gdk_orbital_window_get_type ();
  display_class->get_name = gdk_orbital_display_get_name;
  display_class->get_default_screen = gdk_orbital_display_get_default_screen;
  display_class->beep = gdk_orbital_display_beep;
  display_class->sync = gdk_orbital_display_sync;
  display_class->flush = gdk_orbital_display_flush;
  display_class->make_default = gdk_orbital_display_make_default;
  display_class->has_pending = gdk_orbital_display_has_pending;
  display_class->queue_events = _gdk_orbital_display_queue_events;
  display_class->get_default_group = gdk_orbital_display_get_default_group;
  display_class->supports_selection_notification = gdk_orbital_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_orbital_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_orbital_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_orbital_display_store_clipboard;
  display_class->supports_shapes = gdk_orbital_display_supports_shapes;
  display_class->supports_input_shapes = gdk_orbital_display_supports_input_shapes;
  display_class->supports_composite = gdk_orbital_display_supports_composite;
  display_class->get_app_launch_context = _gdk_orbital_display_get_app_launch_context;
  display_class->get_default_cursor_size = _gdk_orbital_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_orbital_display_get_maximal_cursor_size;
  display_class->get_cursor_for_type = _gdk_orbital_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_orbital_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = _gdk_orbital_display_get_cursor_for_surface;
  display_class->supports_cursor_alpha = _gdk_orbital_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_orbital_display_supports_cursor_color;
  display_class->before_process_all_updates = gdk_orbital_display_before_process_all_updates;
  display_class->after_process_all_updates = gdk_orbital_display_after_process_all_updates;
  display_class->get_next_serial = gdk_orbital_display_get_next_serial;
  display_class->notify_startup_complete = gdk_orbital_display_notify_startup_complete;
  display_class->create_window_impl = _gdk_orbital_display_create_window_impl;
  display_class->get_keymap = _gdk_orbital_display_get_keymap;
  display_class->push_error_trap = gdk_orbital_display_push_error_trap;
  display_class->pop_error_trap = gdk_orbital_display_pop_error_trap;
  display_class->get_selection_owner = _gdk_orbital_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_orbital_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_orbital_display_send_selection_notify;
  display_class->get_selection_property = _gdk_orbital_display_get_selection_property;
  display_class->convert_selection = _gdk_orbital_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_orbital_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_orbital_display_utf8_to_string_target;

  display_class->make_gl_context_current = gdk_orbital_display_make_gl_context_current;

  display_class->get_n_monitors = gdk_orbital_display_get_n_monitors;
  display_class->get_monitor = gdk_orbital_display_get_monitor;
  display_class->get_monitor_at_window = gdk_orbital_display_get_monitor_at_window;
}

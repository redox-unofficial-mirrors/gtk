/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2026 Redox OS Contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include "gdkscreenprivate.h"
#include "gdkvisualprivate.h"
#include "gdkdisplay.h"
#include "gdkdisplay-orbital.h"
#include "gdkmonitor-orbital.h"
#include "gdkorbital.h"
#include "gdkprivate-orbital.h"
#include "gdk-private.h"

#include "wm-button-layout-translation.h"

#include "orbital.h"

typedef struct _GdkOrbitalScreen      GdkOrbitalScreen;
typedef struct _GdkOrbitalScreenClass GdkOrbitalScreenClass;

#define GDK_TYPE_ORBITAL_SCREEN              (_gdk_orbital_screen_get_type ())
#define GDK_ORBITAL_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ORBITAL_SCREEN, GdkOrbitalScreen))
#define GDK_ORBITAL_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_ORBITAL_SCREEN, GdkOrbitalScreenClass))
#define GDK_IS_ORBITAL_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ORBITAL_SCREEN))
#define GDK_IS_ORBITAL_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_ORBITAL_SCREEN))
#define GDK_ORBITAL_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_ORBITAL_SCREEN, GdkOrbitalScreenClass))

typedef struct {
        gboolean     antialias;
        gboolean     hinting;
        gint         dpi;
        const gchar *rgba;
        const gchar *hintstyle;
} GsdXftSettings;

typedef struct {
  guint  fontconfig_timestamp;
  gchar *modules;
} GsdExtSettings;

struct _GdkOrbitalScreen
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkWindow *root_window;

  int width, height;
  int width_mm, height_mm;

  /* Visual Part */
  GdkVisual *visual;

  GHashTable *settings;
  GsdXftSettings xft_settings;
  GsdExtSettings dbus_settings;
  GDBusProxy *settings_portal;

  GDBusProxy *dbus_proxy;
  GCancellable *dbus_cancellable;
  gulong dbus_setting_change_id;
};

struct _GdkOrbitalScreenClass
{
  GdkScreenClass parent_class;
};

#define GTK_SETTINGS_DBUS_PATH "/org/gtk/Settings"
#define GTK_SETTINGS_DBUS_NAME "org.gtk.Settings"

GType _gdk_orbital_screen_get_type (void);

G_DEFINE_TYPE (GdkOrbitalScreen, _gdk_orbital_screen, GDK_TYPE_SCREEN)

static void
gdk_orbital_screen_dispose (GObject *object)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (object);

  if (screen_orbital->dbus_proxy && screen_orbital->dbus_setting_change_id > 0)
    {
      g_signal_handler_disconnect (screen_orbital->dbus_proxy,
                                   screen_orbital->dbus_setting_change_id);
      screen_orbital->dbus_setting_change_id = 0;
    }

  g_cancellable_cancel (screen_orbital->dbus_cancellable);

  if (screen_orbital->root_window)
    _gdk_window_destroy (screen_orbital->root_window, FALSE);

  G_OBJECT_CLASS (_gdk_orbital_screen_parent_class)->dispose (object);
}

static void
gdk_orbital_screen_finalize (GObject *object)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (object);

  g_clear_object (&screen_orbital->dbus_proxy);
  g_clear_object (&screen_orbital->dbus_cancellable);

  if (screen_orbital->root_window)
    g_object_unref (screen_orbital->root_window);

  g_object_unref (screen_orbital->visual);

  if (screen_orbital->settings)
    g_hash_table_destroy (screen_orbital->settings);

  g_clear_object (&screen_orbital->settings_portal);

  g_free (screen_orbital->dbus_settings.modules);

  G_OBJECT_CLASS (_gdk_orbital_screen_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_orbital_screen_get_display (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->display;
}

static gint
gdk_orbital_screen_get_width (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->width;
}

static gint
gdk_orbital_screen_get_height (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->height;
}

static gint
gdk_orbital_screen_get_width_mm (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->width_mm;
}

static gint
gdk_orbital_screen_get_height_mm (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->height_mm;
}

static gint
gdk_orbital_screen_get_number (GdkScreen *screen)
{
  return 0;
}

static GdkWindow *
gdk_orbital_screen_get_root_window (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->root_window;
}

static GdkVisual *
gdk_orbital_screen_get_system_visual (GdkScreen * screen)
{
  return (GdkVisual *) GDK_ORBITAL_SCREEN (screen)->visual;
}

static GdkVisual *
gdk_orbital_screen_get_rgba_visual (GdkScreen *screen)
{
  return (GdkVisual *) GDK_ORBITAL_SCREEN (screen)->visual;
}

static gboolean
gdk_orbital_screen_is_composited (GdkScreen *screen)
{
  return TRUE;
}

static gchar *
gdk_orbital_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup (gdk_display_get_name (GDK_ORBITAL_SCREEN (screen)->display));
}

static GdkWindow *
gdk_orbital_screen_get_active_window (GdkScreen *screen)
{
  return NULL;
}

static GList *
gdk_orbital_screen_get_window_stack (GdkScreen *screen)
{
  return NULL;
}

static void
gdk_orbital_screen_broadcast_client_message (GdkScreen *screen,
                                             GdkEvent  *event)
{
}

static void
notify_setting (GdkScreen   *screen,
                const gchar *setting)
{
  GdkEvent event;

  event.type = GDK_SETTING;
  event.setting.window = gdk_screen_get_root_window (screen);
  event.setting.send_event = FALSE;
  event.setting.action = GDK_SETTING_ACTION_CHANGED;
  event.setting.name = (gchar *)setting;
  gdk_event_put (&event);
}

typedef enum
{
  GSD_FONT_ANTIALIASING_MODE_NONE,
  GSD_FONT_ANTIALIASING_MODE_GRAYSCALE,
  GSD_FONT_ANTIALIASING_MODE_RGBA
} GsdFontAntialiasingMode;

static int
get_antialiasing (const char *s)
{
  const char *names[] = { "none", "grayscale", "rgba" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_HINTING_NONE,
  GSD_FONT_HINTING_SLIGHT,
  GSD_FONT_HINTING_MEDIUM,
  GSD_FONT_HINTING_FULL
} GsdFontHinting;

static int
get_hinting (const char *s)
{
  const char *names[] = { "none", "slight", "medium", "full" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_RGBA_ORDER_RGBA,
  GSD_FONT_RGBA_ORDER_RGB,
  GSD_FONT_RGBA_ORDER_BGR,
  GSD_FONT_RGBA_ORDER_VRGB,
  GSD_FONT_RGBA_ORDER_VBGR
} GsdFontRgbaOrder;

static int
get_order (const char *s)
{
  const char *names[] = { "rgba", "rgb", "bgr", "vrgb", "vbgr" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

static gdouble
get_dpi_from_gsettings (GdkOrbitalScreen *screen_orbital)
{
  GSettings *settings;
  gdouble factor;

  settings = g_hash_table_lookup (screen_orbital->settings,
                                  "org.gnome.desktop.interface");
  if (settings != NULL)
    factor = g_settings_get_double (settings, "text-scaling-factor");
  else
    factor = 1.0;

  return 96.0 * factor;
}

typedef struct _TranslationEntry TranslationEntry;
struct _TranslationEntry {
  gboolean valid;
  const gchar *schema;
  const gchar *key;
  const gchar *setting;
  GType type;
  union {
    const char *s;
    gint         i;
    gboolean     b;
  } fallback;
};

static TranslationEntry * find_translation_entry_by_schema (const char *schema,
                                                            const char *key);

static void
update_xft_settings (GdkScreen *screen, gboolean notify)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (screen);
  GSettings *settings;
  GsdFontAntialiasingMode antialiasing;
  GsdFontHinting hinting;
  GsdFontRgbaOrder order;
  gboolean use_rgba = FALSE;
  GsdXftSettings xft_settings;
  double dpi;

  if (screen_orbital->settings_portal)
    {
      TranslationEntry *entry;

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");

      if (entry->valid)
        {
          antialiasing = entry->fallback.i;
          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-hinting");
          hinting = entry->fallback.i;
          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-rgba-order");
          order = entry->fallback.i;
        }
      else
        {
          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "antialiasing");
          antialiasing = entry->fallback.i;
          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "hinting");
          hinting = entry->fallback.i;
          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "rgba-order");
          order = entry->fallback.i;
        }

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "text-scaling-factor");
      dpi = 96.0 * entry->fallback.i / 65536.0 * 1024; /* Xft wants 1/1024th of an inch */
    }
  else
    {
      TranslationEntry *entry;
      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");

      if (entry && entry->valid)
        {
          settings = g_hash_table_lookup (screen_orbital->settings, "org.gnome.desktop.interface");
          antialiasing = g_settings_get_enum (settings, "font-antialiasing");
          hinting = g_settings_get_enum (settings, "font-hinting");
          order = g_settings_get_enum (settings, "font-rgba-order");
        }
      else if (g_hash_table_contains (screen_orbital->settings, "org.gnome.settings-daemon.plugins.xsettings"))
        {
          settings = g_hash_table_lookup (screen_orbital->settings, "org.gnome.settings-daemon.plugins.xsettings");
          antialiasing = g_settings_get_enum (settings, "antialiasing");
          hinting = g_settings_get_enum (settings, "hinting");
          order = g_settings_get_enum (settings, "rgba-order");
        }
      else
        {
          antialiasing = GSD_FONT_ANTIALIASING_MODE_GRAYSCALE;
          hinting = GSD_FONT_HINTING_MEDIUM;
          order = GSD_FONT_RGBA_ORDER_RGB;
        }

      dpi = get_dpi_from_gsettings (screen_orbital) * 1024;
    }

  xft_settings.antialias = (antialiasing != GSD_FONT_ANTIALIASING_MODE_NONE);
  xft_settings.hinting = (hinting != GSD_FONT_HINTING_NONE);
  xft_settings.dpi = dpi;
  xft_settings.rgba = "rgb";
  xft_settings.hintstyle = "hintfull";

  switch (hinting)
    {
    case GSD_FONT_HINTING_NONE: xft_settings.hintstyle = "hintnone"; break;
    case GSD_FONT_HINTING_SLIGHT: xft_settings.hintstyle = "hintslight"; break;
    case GSD_FONT_HINTING_MEDIUM: xft_settings.hintstyle = "hintmedium"; break;
    case GSD_FONT_HINTING_FULL: xft_settings.hintstyle = "hintfull"; break;
    }

  switch (order)
    {
    case GSD_FONT_RGBA_ORDER_RGBA: xft_settings.rgba = "rgba"; break;
    case GSD_FONT_RGBA_ORDER_RGB: xft_settings.rgba = "rgb"; break;
    case GSD_FONT_RGBA_ORDER_BGR: xft_settings.rgba = "bgr"; break;
    case GSD_FONT_RGBA_ORDER_VRGB: xft_settings.rgba = "vrgb"; break;
    case GSD_FONT_RGBA_ORDER_VBGR: xft_settings.rgba = "vbgr"; break;
    }

  switch (antialiasing)
   {
   case GSD_FONT_ANTIALIASING_MODE_NONE: xft_settings.antialias = FALSE; break;
   case GSD_FONT_ANTIALIASING_MODE_GRAYSCALE: xft_settings.antialias = TRUE; break;
   case GSD_FONT_ANTIALIASING_MODE_RGBA: xft_settings.antialias = TRUE; use_rgba = TRUE; break;
   }

  if (!use_rgba)
    xft_settings.rgba = "none";

  if (screen_orbital->xft_settings.antialias != xft_settings.antialias)
    {
      screen_orbital->xft_settings.antialias = xft_settings.antialias;
      if (notify) notify_setting (screen, "gtk-xft-antialias");
    }

  if (screen_orbital->xft_settings.hinting != xft_settings.hinting)
    {
      screen_orbital->xft_settings.hinting = xft_settings.hinting;
      if (notify) notify_setting (screen, "gtk-xft-hinting");
    }

  if (screen_orbital->xft_settings.hintstyle != xft_settings.hintstyle)
    {
      screen_orbital->xft_settings.hintstyle = xft_settings.hintstyle;
      if (notify) notify_setting (screen, "gtk-xft-hintstyle");
    }

  if (screen_orbital->xft_settings.rgba != xft_settings.rgba)
    {
      screen_orbital->xft_settings.rgba = xft_settings.rgba;
      if (notify) notify_setting (screen, "gtk-xft-rgba");
    }

  if (screen_orbital->xft_settings.dpi != xft_settings.dpi)
    {
      double dpi = xft_settings.dpi / 1024.;
      const char *scale_env;
      double scale;

      screen_orbital->xft_settings.dpi = xft_settings.dpi;

      scale_env = g_getenv ("GDK_DPI_SCALE");
      if (scale_env)
        {
          scale = g_ascii_strtod (scale_env, NULL);
          if (scale != 0 && dpi > 0)
            dpi *= scale;
        }

      _gdk_screen_set_resolution (screen, dpi);

      if (notify)
        notify_setting (screen, "gtk-xft-dpi");
    }
}

#define WM_SETTINGS_SCHEMA "org.gnome.desktop.wm.preferences"
#define CLASSIC_WM_SETTINGS_SCHEMA "org.gnome.shell.extensions.classic-overrides"

static TranslationEntry translations[] = {
  { FALSE, "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" , G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "gtk-key-theme", "gtk-key-theme-name" , G_TYPE_STRING, { .s = "Default" } },
  { FALSE, "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name", G_TYPE_STRING, { .s = "gnome" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name", G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size", G_TYPE_INT, { .i = 32 } },
  { FALSE, "org.gnome.desktop.interface", "font-name", "gtk-font-name", G_TYPE_STRING, { .s = "Cantarell 11" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink", G_TYPE_BOOLEAN,  { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time", G_TYPE_INT, { .i = 1200 } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout", G_TYPE_INT, { .i = 3600 } },
  { FALSE, "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module", G_TYPE_STRING, { .s = "simple" } },
  { FALSE, "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "gtk-enable-primary-paste", "gtk-enable-primary-paste", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "overlay-scrolling", "gtk-overlay-scrolling", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name", G_TYPE_STRING, { .s = "freedesktop" } },
  { FALSE, "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds", G_TYPE_BOOLEAN, { . b = FALSE } },
  { FALSE, "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age", G_TYPE_INT, { .i = 30 } },
  { FALSE, "org.gnome.desktop.privacy", "remember-recent-files",    "gtk-recent-files-enabled", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, WM_SETTINGS_SCHEMA, "button-layout",    "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, CLASSIC_WM_SETTINGS_SCHEMA, "button-layout",   "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, "org.gnome.desktop.interface", "font-antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.interface", "text-scaling-factor", "gtk-xft-dpi" , G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-double-click-titlebar", "gtk-titlebar-double-click", G_TYPE_STRING, { .s = "toggle-maximize" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-middle-click-titlebar", "gtk-titlebar-middle-click", G_TYPE_STRING, { .s = "none" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-right-click-titlebar", "gtk-titlebar-right-click", G_TYPE_STRING, { .s = "menu" } },
  { FALSE, "org.gnome.desktop.a11y", "always-show-text-caret", "gtk-keynav-use-caret", G_TYPE_BOOLEAN, { .b = FALSE } },
  { FALSE, "org.gnome.desktop.a11y.interface", "high-contrast", "high-contrast", G_TYPE_NONE, { .b = FALSE } },
  { FALSE, "org.gnome.fontconfig", "serial", "gtk-fontconfig-timestamp", G_TYPE_INT, { .i = 0 } }
};

static TranslationEntry *
find_translation_entry_by_schema (const char *schema,
                                  const gchar *key)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (schema, translations[i].schema) &&
          g_str_equal (key, translations[i].key))
        return &translations[i];
    }

  return NULL;
}

static TranslationEntry *
find_translation_entry_by_key (GSettings  *settings,
                               const char *key)
{
  char *schema;
  TranslationEntry *entry;

  g_object_get (settings, "schema", &schema, NULL);
  entry = find_translation_entry_by_schema (schema, key);
  g_free (schema);

  return entry;
}

static TranslationEntry *
find_translation_entry_by_setting (const gchar *setting)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (setting, translations[i].setting))
        return &translations[i];
    }

  return NULL;
}

static void
high_contrast_changed (GdkScreen *screen)
{
  notify_setting (screen, "gtk-theme-name");
  notify_setting (screen, "gtk-icon-theme-name");
}

static void
settings_changed (GSettings   *settings,
                  const gchar *key,
                  GdkScreen   *screen)
{
  TranslationEntry *entry;

  entry = find_translation_entry_by_key (settings, key);

  if (entry != NULL)
    {
      if (entry->type != G_TYPE_NONE)
        notify_setting (screen, entry->setting);
      else if (strcmp (key, "high-contrast") == 0)
        high_contrast_changed (screen);
      else
        update_xft_settings (screen, TRUE);
    }
}

static void
apply_portal_setting (TranslationEntry *entry,
                      GVariant         *value,
                      GdkScreen        *screen)
{
  switch (entry->type)
    {
    case G_TYPE_STRING:
      entry->fallback.s = g_intern_string (g_variant_get_string (value, NULL));
      break;
    case G_TYPE_INT:
      entry->fallback.i = g_variant_get_int32 (value);
      break;
    case G_TYPE_BOOLEAN:
      entry->fallback.b = g_variant_get_boolean (value);
      break;
    case G_TYPE_NONE:
      if (strcmp (entry->key, "antialiasing") == 0 ||
          strcmp (entry->key, "font-antialiasing") == 0)
        entry->fallback.i = get_antialiasing (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "hinting") == 0 ||
               strcmp (entry->key, "font-hinting") == 0)
        entry->fallback.i = get_hinting (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "rgba-order") == 0 ||
               strcmp (entry->key, "font-rgba-order") == 0)
        entry->fallback.i = get_order (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "text-scaling-factor") == 0)
        entry->fallback.i = (int) (g_variant_get_double (value) * 65536.0);
      update_xft_settings (screen, TRUE);
      break;
    default:
      break;
    }
}

static void
settings_portal_changed (GDBusProxy *proxy,
                         const char *sender_name,
                         const char *signal_name,
                         GVariant   *parameters,
                         GdkScreen  *screen)
{
  if (strcmp (signal_name, "SettingChanged") == 0)
    {
      const char *namespace;
      const char *name;
      GVariant *value;
      TranslationEntry *entry;

      g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

      entry = find_translation_entry_by_schema (namespace, name);
      if (entry != NULL)
        {
          char *a = g_variant_print (value, FALSE);
          g_debug ("Using changed portal setting %s %s: %s", namespace, name, a);
          g_free (a);
          apply_portal_setting (entry, value, screen);
          notify_setting (screen, entry->setting);
        }
      else
        g_debug ("Ignoring portal setting %s %s", namespace, name);

      g_variant_unref (value);
    }
}

static void fontconfig_dbus_proxy_open_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data);

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

static void
init_settings (GdkScreen *screen)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (screen);
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  GSettings *settings;
  gint i;

  if (gdk_should_use_portal ())
    {
      GVariant *ret;
      GError *error = NULL;
      const char *schema;
      GVariant *val;
      GVariantIter *iter;
      const char *patterns[] = { "org.gnome.*", NULL };

      screen_orbital->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                                       NULL,
                                                                       PORTAL_BUS_NAME,
                                                                       PORTAL_OBJECT_PATH,
                                                                       PORTAL_SETTINGS_INTERFACE,
                                                                       NULL,
                                                                       &error);
      if (error)
        {
          g_warning ("Settings portal not found: %s", error->message);
          g_error_free (error);
          goto fallback;
        }

      ret = g_dbus_proxy_call_sync (screen_orbital->settings_portal,
                                    "ReadAll",
                                    g_variant_new ("(^as)", patterns),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);
      if (error)
        {
          g_warning ("Failed to read portal settings: %s", error->message);
          g_error_free (error);
          g_clear_object (&screen_orbital->settings_portal);
          goto fallback;
        }

      if (g_variant_n_children (ret) == 0)
        {
          g_debug ("Received no portal settings");
          g_clear_pointer (&ret, g_variant_unref);
          goto fallback;
        }

      g_variant_get (ret, "(a{sa{sv}})", &iter);

      while (g_variant_iter_loop (iter, "{s@a{sv}}", &schema, &val))
        {
          GVariantIter *iter2 = g_variant_iter_new (val);
          const char *key;
          GVariant *v;

          while (g_variant_iter_loop (iter2, "{sv}", &key, &v))
            {
              TranslationEntry *entry = find_translation_entry_by_schema (schema, key);
              if (entry)
                {
                  char *a = g_variant_print (v, FALSE);
                  g_debug ("Using portal setting for %s %s: %s\n", schema, key, a);
                  g_free (a);
                  entry->valid = TRUE;
                  apply_portal_setting (entry, v, screen);
                }
              else
                {
                  g_debug ("Ignoring portal setting for %s %s", schema, key);
                }
            }
          g_variant_iter_free (iter2);
        }
      g_variant_iter_free (iter);
      g_variant_unref (ret);

      g_signal_connect (screen_orbital->settings_portal, "g-signal",
                        G_CALLBACK (settings_portal_changed), screen_orbital);
      return;

fallback:
      g_debug ("Failed to use Settings portal; falling back to gsettings");
    }

  screen_orbital->dbus_cancellable = g_cancellable_new ();
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            GTK_SETTINGS_DBUS_NAME,
                            GTK_SETTINGS_DBUS_PATH,
                            GTK_SETTINGS_DBUS_NAME,
                            screen_orbital->dbus_cancellable,
                            fontconfig_dbus_proxy_open_cb,
                            screen_orbital);

  screen_orbital->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  source = g_settings_schema_source_get_default ();
  if (source == NULL)
    return;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      schema = g_settings_schema_source_lookup (source, translations[i].schema, TRUE);
      if (!schema)
        continue;

      if (g_hash_table_lookup (screen_orbital->settings, (gpointer)translations[i].schema) == NULL)
        {
          settings = g_settings_new_full (schema, NULL, NULL);
          g_signal_connect (settings, "changed", G_CALLBACK (settings_changed), screen);
          g_hash_table_insert (screen_orbital->settings, (gpointer)translations[i].schema, settings);
        }

      if (g_settings_schema_has_key (schema, translations[i].key))
        translations[i].valid = TRUE;

      g_settings_schema_unref (schema);
    }

  update_xft_settings (screen, FALSE);
}

void
_gdk_orbital_screen_set_has_gtk_shell (GdkScreen *screen)
{
}

static void
set_value_from_entry (GdkScreen        *screen,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (screen);
  GSettings *settings;

  if (screen_orbital->settings_portal)
    {
      switch (entry->type)
        {
        case G_TYPE_STRING:
          g_value_set_string (value, entry->fallback.s);
          break;
        case G_TYPE_INT:
          if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
            g_value_set_uint (value, (guint)entry->fallback.i);
          else
            g_value_set_int (value, entry->fallback.i);
          break;
        case G_TYPE_BOOLEAN:
          g_value_set_boolean (value, entry->fallback.b);
          break;
        case G_TYPE_NONE:
          if (g_str_equal (entry->setting, "gtk-xft-antialias"))
            g_value_set_int (value, screen_orbital->xft_settings.antialias);
          else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
            g_value_set_int (value, screen_orbital->xft_settings.hinting);
          else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
            g_value_set_static_string (value, screen_orbital->xft_settings.hintstyle);
          else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
            g_value_set_static_string (value, screen_orbital->xft_settings.rgba);
          else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
            g_value_set_int (value, screen_orbital->xft_settings.dpi);
          else
            g_assert_not_reached ();
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      return;
    }

  settings = (GSettings *)g_hash_table_lookup (screen_orbital->settings, entry->schema);
  switch (entry->type)
    {
    case G_TYPE_STRING:
      if (settings && entry->valid)
        {
          gchar *s;
          s = g_settings_get_string (settings, entry->key);
          g_value_set_string (value, s);
          g_free (s);
        }
      else
        {
          g_value_set_static_string (value, entry->fallback.s);
        }
      break;
    case G_TYPE_INT:
      if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
        g_value_set_uint (value, screen_orbital->dbus_settings.fontconfig_timestamp);
      else
        g_value_set_int (value, settings && entry->valid
                                ? g_settings_get_int (settings, entry->key)
                                : entry->fallback.i);
      break;
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, settings && entry->valid
                                  ? g_settings_get_boolean (settings, entry->key)
                                  : entry->fallback.b);
      break;
    case G_TYPE_NONE:
      if (g_str_equal (entry->setting, "gtk-xft-antialias"))
        g_value_set_int (value, screen_orbital->xft_settings.antialias);
      else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
        g_value_set_int (value, screen_orbital->xft_settings.hinting);
      else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
        g_value_set_static_string (value, screen_orbital->xft_settings.hintstyle);
      else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
        g_value_set_static_string (value, screen_orbital->xft_settings.rgba);
      else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
        g_value_set_int (value, screen_orbital->xft_settings.dpi);
      else
        g_assert_not_reached ();
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
set_decoration_layout_from_entry (GdkScreen        *screen,
                                  TranslationEntry *entry,
                                  GValue           *value)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (screen);
  GSettings *settings = NULL;
  const char *session;

  if (screen_orbital->settings_portal)
    {
      g_value_set_string (value, entry->fallback.s);
      return;
    }

  session = g_getenv ("XDG_CURRENT_DESKTOP");
  if (session && strstr (session, "GNOME-Classic"))
    settings = (GSettings *)g_hash_table_lookup (screen_orbital->settings, CLASSIC_WM_SETTINGS_SCHEMA);

  if (settings == NULL)
    settings = (GSettings *)g_hash_table_lookup (screen_orbital->settings, WM_SETTINGS_SCHEMA);

  if (settings)
    {
      gchar *s = g_settings_get_string (settings, entry->key);

      translate_or_button_layout_to_gtk (s);
      g_value_set_string (value, s);

      g_free (s);
    }
  else
    {
      g_value_set_static_string (value, entry->fallback.s);
    }
}

static void
set_theme_from_entry (GdkScreen        *screen,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkOrbitalScreen *screen_orbital = GDK_ORBITAL_SCREEN (screen);
  GSettings *settings = NULL;
  GSettingsSchema *schema = NULL;
  gboolean hc = FALSE;

  if (screen_orbital->settings_portal == NULL)
    {
      settings = (GSettings *)g_hash_table_lookup (screen_orbital->settings,
                                                   "org.gnome.desktop.a11y.interface");
    }

  if (settings)
    g_object_get (settings, "settings-schema", &schema, NULL);

  if (schema && g_settings_schema_has_key (schema, "high-contrast"))
    hc = g_settings_get_boolean (settings, "high-contrast");

  g_clear_pointer (&schema, g_settings_schema_unref);

  if (hc)
    g_value_set_static_string (value, "HighContrast");
  else
    set_value_from_entry (screen, entry, value);
}

static gboolean
gdk_orbital_screen_get_setting (GdkScreen   *screen,
                                const gchar *name,
                                GValue      *value)
{
  GdkOrbitalScreen *orbital_screen = GDK_ORBITAL_SCREEN (screen);
  TranslationEntry *entry;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  if (orbital_screen->settings != NULL &&
      g_hash_table_size (orbital_screen->settings) == 0)
    return FALSE;

  entry = find_translation_entry_by_setting (name);
  if (entry != NULL)
    {
      if (strcmp (name, "gtk-decoration-layout") == 0)
        set_decoration_layout_from_entry (screen, entry, value);
      else if (strcmp (name, "gtk-theme-name") == 0)
        set_theme_from_entry (screen, entry, value);
      else
        set_value_from_entry (screen, entry, value);
      return TRUE;
   }

  /* Default sane desktop states for Orbital lacking an explicit shell metadata mechanism */
  if (strcmp (name, "gtk-shell-shows-app-menu") == 0)
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  if (strcmp (name, "gtk-shell-shows-menubar") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  if (strcmp (name, "gtk-shell-shows-desktop") == 0)
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  if (strcmp (name, "gtk-dialogs-use-header") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (strcmp (name, "gtk-fontconfig-timestamp") == 0)
    {
      g_value_set_uint (value, orbital_screen->dbus_settings.fontconfig_timestamp);
      return TRUE;
    }

  if (strcmp (name, "gtk-modules") == 0)
    {
      g_value_set_string (value, orbital_screen->dbus_settings.modules);
      return TRUE;
    }

  return FALSE;
}

typedef struct _GdkOrbitalVisual  GdkOrbitalVisual;
typedef struct _GdkOrbitalVisualClass GdkOrbitalVisualClass;

struct _GdkOrbitalVisual
{
  GdkVisual visual;
};

struct _GdkOrbitalVisualClass
{
  GdkVisualClass parent_class;
};

GType _gdk_orbital_visual_get_type (void);

G_DEFINE_TYPE (GdkOrbitalVisual, _gdk_orbital_visual, GDK_TYPE_VISUAL)

static void
_gdk_orbital_visual_class_init (GdkOrbitalVisualClass *klass)
{
}

static void
_gdk_orbital_visual_init (GdkOrbitalVisual *visual)
{
}

static gint
gdk_orbital_screen_visual_get_best_depth (GdkScreen *screen)
{
  return 32;
}

static GdkVisualType
gdk_orbital_screen_visual_get_best_type (GdkScreen *screen)
{
  return GDK_VISUAL_TRUE_COLOR;
}

static GdkVisual*
gdk_orbital_screen_visual_get_best (GdkScreen *screen)
{
  return GDK_ORBITAL_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_orbital_screen_visual_get_best_with_depth (GdkScreen *screen,
                                               gint       depth)
{
  if (depth == 32)
    return GDK_ORBITAL_SCREEN (screen)->visual;
  else
    return NULL;
}

static GdkVisual*
gdk_orbital_screen_visual_get_best_with_type (GdkScreen     *screen,
                                              GdkVisualType  visual_type)
{
  if (visual_type == GDK_VISUAL_TRUE_COLOR)
    return GDK_ORBITAL_SCREEN (screen)->visual;
  else
    return NULL;
}

static GdkVisual*
gdk_orbital_screen_visual_get_best_with_both (GdkScreen     *screen,
                                              gint           depth,
                                              GdkVisualType  visual_type)
{
  if (depth == 32 && visual_type == GDK_VISUAL_TRUE_COLOR)
    return GDK_ORBITAL_SCREEN (screen)->visual;
  else
    return NULL;
}

static void
gdk_orbital_screen_query_depths  (GdkScreen  *screen,
                                  gint      **depths,
                                  gint       *count)
{
  static gint static_depths[] = { 32 };

  *count = G_N_ELEMENTS(static_depths);
  *depths = static_depths;
}

static void
gdk_orbital_screen_query_visual_types (GdkScreen      *screen,
                                       GdkVisualType **visual_types,
                                       gint           *count)
{
  static GdkVisualType static_visual_types[] = { GDK_VISUAL_TRUE_COLOR };

  *count = G_N_ELEMENTS(static_visual_types);
  *visual_types = static_visual_types;
}

static GList *
gdk_orbital_screen_list_visuals (GdkScreen *screen)
{
  GList *list;
  GdkOrbitalScreen *screen_orbital;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_orbital = GDK_ORBITAL_SCREEN (screen);

  list = g_list_append (NULL, screen_orbital->visual);

  return list;
}

#define GDK_TYPE_ORBITAL_VISUAL              (_gdk_orbital_visual_get_type ())
#define GDK_ORBITAL_VISUAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ORBITAL_VISUAL, GdkOrbitalVisual))

/* Currently, the Orbital backend only ever uses ARGB8888.
 */
static GdkVisual *
gdk_orbital_visual_new (GdkScreen *screen)
{
  GdkVisual *visual;

  visual = g_object_new (GDK_TYPE_ORBITAL_VISUAL, NULL);
  visual->screen = GDK_SCREEN (screen);
  visual->type = GDK_VISUAL_TRUE_COLOR;
  visual->depth = 32;
  visual->red_mask = 0xff0000;
  visual->green_mask = 0x00ff00;
  visual->blue_mask = 0x0000ff;
  visual->bits_per_rgb = 8;

  return visual;
}

static void
dbus_properties_change_cb (GDBusProxy         *proxy,
                           GVariant           *changed_properties,
                           const gchar* const *invalidated_properties,
                           gpointer            user_data)
{
  GdkOrbitalScreen *screen_orbital = user_data;
  GVariant *value;
  gint64 timestamp;

  if (g_variant_n_children (changed_properties) <= 0)
    return;

  value = g_variant_lookup_value (changed_properties,
                                  "FontconfigTimestamp",
                                  G_VARIANT_TYPE_INT64);

  if (value != NULL)
    {
      timestamp = g_variant_get_int64 (value);
      timestamp = timestamp / G_TIME_SPAN_SECOND;

      if (timestamp > 0 && timestamp <= G_MAXUINT)
        screen_orbital->dbus_settings.fontconfig_timestamp = (guint)timestamp;
      else if (timestamp > G_MAXUINT)
        g_warning ("Could not handle fontconfig update: timestamp out of bound");

      notify_setting (GDK_SCREEN (screen_orbital), "gtk-fontconfig-timestamp");

      g_variant_unref (value);
    }

  value = g_variant_lookup_value (changed_properties,
                                  "Modules",
                                  G_VARIANT_TYPE_STRING);

  if (value != NULL)
    {
      g_free (screen_orbital->dbus_settings.modules);

      screen_orbital->dbus_settings.modules = g_variant_dup_string (value, NULL);

      notify_setting (GDK_SCREEN (screen_orbital), "gtk-modules");

      g_variant_unref (value);
    }
}

static void
fontconfig_dbus_proxy_open_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GdkOrbitalScreen *screen_orbital = user_data;
  GDBusProxy *proxy;
  GVariant *value;
  gint64 timestamp;

  proxy = g_dbus_proxy_new_for_bus_finish (result, NULL);

  if (proxy == NULL)
    return;

  screen_orbital->dbus_proxy = proxy;
  screen_orbital->dbus_setting_change_id =
    g_signal_connect (screen_orbital->dbus_proxy,
                      "g-properties-changed",
                      G_CALLBACK (dbus_properties_change_cb),
                      screen_orbital);

  value = g_dbus_proxy_get_cached_property (screen_orbital->dbus_proxy,
                                            "FontconfigTimestamp");

  if (value && g_variant_is_of_type (value, G_VARIANT_TYPE_INT64))
    {
      timestamp = g_variant_get_int64 (value);
      timestamp = timestamp / G_TIME_SPAN_SECOND;

      if (timestamp > 0 && timestamp <= G_MAXUINT)
        screen_orbital->dbus_settings.fontconfig_timestamp = (guint)timestamp;
      else if (timestamp > G_MAXUINT)
        g_warning ("Could not handle fontconfig init: timestamp out of bound");
    }

  if (value != NULL)
    g_variant_unref (value);

  value = g_dbus_proxy_get_cached_property (screen_orbital->dbus_proxy,
                                            "Modules");

  if (value && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
    {
      g_free (screen_orbital->dbus_settings.modules);

      screen_orbital->dbus_settings.modules = g_variant_dup_string (value, NULL);
      notify_setting (GDK_SCREEN (screen_orbital), "gtk-modules");
    }

  if (value != NULL)
    g_variant_unref (value);
}

GdkScreen *
_gdk_orbital_screen_new (GdkDisplay *display)
{
  GdkScreen *screen;
  GdkOrbitalScreen *screen_orbital;
  GdkOrbitalMonitor *primary_monitor;

  screen = g_object_new (GDK_TYPE_ORBITAL_SCREEN, NULL);

  screen_orbital = GDK_ORBITAL_SCREEN (screen);
  screen_orbital->display = display;
  
  screen_orbital->width = orb_display_width ();
  screen_orbital->height = orb_display_height ();
  screen_orbital->width_mm = (screen_orbital->width * 25.4) / 96.0;
  screen_orbital->height_mm = (screen_orbital->height * 25.4) / 96.0;

  screen_orbital->visual = gdk_orbital_visual_new (screen);

  screen_orbital->root_window =
    _gdk_orbital_screen_create_root_window (screen,
                                            screen_orbital->width,
                                            screen_orbital->height);

  init_settings (screen);

  /* TODO: Multi monitor API */
  primary_monitor = g_object_new (GDK_TYPE_ORBITAL_MONITOR,
                                  "display", display,
                                  NULL);
                                  
  gdk_monitor_set_position (GDK_MONITOR (primary_monitor), 0, 0);
  gdk_monitor_set_size (GDK_MONITOR (primary_monitor), 
                        screen_orbital->width, 
                        screen_orbital->height);
                        
  gdk_monitor_set_physical_size (GDK_MONITOR (primary_monitor), 
                                 screen_orbital->width_mm, 
                                 screen_orbital->height_mm);
                                 
  primary_monitor->id = 0;

  g_ptr_array_add (GDK_ORBITAL_DISPLAY (display)->monitors, primary_monitor);
  gdk_display_monitor_added (display, GDK_MONITOR (primary_monitor));

  return screen;
}

static void
_gdk_orbital_screen_class_init (GdkOrbitalScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_orbital_screen_dispose;
  object_class->finalize = gdk_orbital_screen_finalize;

  screen_class->get_display = gdk_orbital_screen_get_display;
  screen_class->get_width = gdk_orbital_screen_get_width;
  screen_class->get_height = gdk_orbital_screen_get_height;
  screen_class->get_width_mm = gdk_orbital_screen_get_width_mm;
  screen_class->get_height_mm = gdk_orbital_screen_get_height_mm;
  screen_class->get_number = gdk_orbital_screen_get_number;
  screen_class->get_root_window = gdk_orbital_screen_get_root_window;
  screen_class->get_system_visual = gdk_orbital_screen_get_system_visual;
  screen_class->get_rgba_visual = gdk_orbital_screen_get_rgba_visual;
  screen_class->is_composited = gdk_orbital_screen_is_composited;
  screen_class->make_display_name = gdk_orbital_screen_make_display_name;
  screen_class->get_active_window = gdk_orbital_screen_get_active_window;
  screen_class->get_window_stack = gdk_orbital_screen_get_window_stack;
  screen_class->broadcast_client_message = gdk_orbital_screen_broadcast_client_message;
  screen_class->get_setting = gdk_orbital_screen_get_setting;
  screen_class->visual_get_best_depth = gdk_orbital_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = gdk_orbital_screen_visual_get_best_type;
  screen_class->visual_get_best = gdk_orbital_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = gdk_orbital_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = gdk_orbital_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = gdk_orbital_screen_visual_get_best_with_both;
  screen_class->query_depths = gdk_orbital_screen_query_depths;
  screen_class->query_visual_types = gdk_orbital_screen_query_visual_types;
  screen_class->list_visuals = gdk_orbital_screen_list_visuals;
}

static void
_gdk_orbital_screen_init (GdkOrbitalScreen *screen_orbital)
{
}

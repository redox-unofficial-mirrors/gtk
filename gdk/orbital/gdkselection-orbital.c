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

#include <string.h>

#include "gdkorbital.h"
#include "gdkprivate-orbital.h"
#include "gdkdisplay-orbital.h"
#include "gdkdndprivate.h"
#include "gdkselection.h"
#include "gdkproperty.h"
#include "gdkprivate.h"

#include "orbital.h"

#define STRING_MIMETYPE "text/plain"
#define UTF8_STRING_MIMETYPE "text/plain;charset=utf-8"

enum {
  ATOM_PRIMARY,
  ATOM_CLIPBOARD,
  ATOM_DND,
  N_ATOMS
};

static GdkAtom atoms[N_ATOMS] = { 0 };

struct _GdkOrbitalSelection
{
  GdkWindow *primary_owner;
  GdkWindow *clipboard_owner;
  GdkWindow *dnd_owner;
  
  GArray *source_targets;
};

GdkOrbitalSelection *
gdk_orbital_selection_new (void)
{
  GdkOrbitalSelection *selection;

  if (atoms[ATOM_PRIMARY] == 0)
    {
      atoms[ATOM_PRIMARY] = gdk_atom_intern_static_string ("PRIMARY");
      atoms[ATOM_CLIPBOARD] = gdk_atom_intern_static_string ("CLIPBOARD");
      atoms[ATOM_DND] = gdk_atom_intern_static_string ("GdkOrbitalSelection");
    }

  selection = g_new0 (GdkOrbitalSelection, 1);
  selection->source_targets = g_array_new (FALSE, FALSE, sizeof (GdkAtom));

  return selection;
}

void
gdk_orbital_selection_free (GdkOrbitalSelection *selection)
{
  if (selection->source_targets)
    g_array_unref (selection->source_targets);

  g_free (selection);
}

GdkWindow *
_gdk_orbital_display_get_selection_owner (GdkDisplay *display,
                                          GdkAtom     selection)
{
  GdkOrbitalSelection *orbital_selection = gdk_orbital_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    return orbital_selection->clipboard_owner;
  else if (selection == atoms[ATOM_PRIMARY])
    return orbital_selection->primary_owner;
  else if (selection == atoms[ATOM_DND])
    return orbital_selection->dnd_owner;

  return NULL;
}

gboolean
_gdk_orbital_display_set_selection_owner (GdkDisplay *display,
                                          GdkWindow  *owner,
                                          GdkAtom     selection,
                                          guint32     time,
                                          gboolean    send_event)
{
  GdkOrbitalSelection *orbital_selection = gdk_orbital_display_get_selection (display);

  if (selection == atoms[ATOM_CLIPBOARD])
    {
      orbital_selection->clipboard_owner = owner;
      return TRUE;
    }
  else if (selection == atoms[ATOM_PRIMARY])
    {
      orbital_selection->primary_owner = owner;
      return TRUE;
    }
  else if (selection == atoms[ATOM_DND])
    {
      orbital_selection->dnd_owner = owner;
      return TRUE;
    }

  return FALSE;
}

void
_gdk_orbital_display_send_selection_notify (GdkDisplay *display,
                                            GdkWindow  *requestor,
                                            GdkAtom     selection,
                                            GdkAtom     target,
                                            GdkAtom     property,
                                            guint32     time)
{
}

gint
_gdk_orbital_display_get_selection_property (GdkDisplay  *display,
                                             GdkWindow   *requestor,
                                             guchar     **data,
                                             GdkAtom     *ret_type,
                                             gint        *ret_format)
{
  if (data)
    *data = NULL;
  if (ret_type)
    *ret_type = GDK_NONE;
  if (ret_format)
    *ret_format = 0;

  return 0;
}

static void
emit_empty_selection_notify (GdkWindow *requestor,
                             GdkAtom    selection,
                             GdkAtom    target)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_SELECTION_NOTIFY);
  event->selection.window = g_object_ref (requestor);
  event->selection.send_event = FALSE;
  event->selection.selection = selection;
  event->selection.target = target;
  event->selection.property = GDK_NONE;
  event->selection.time = GDK_CURRENT_TIME;
  event->selection.requestor = g_object_ref (requestor);

  gdk_event_put (event);
  gdk_event_free (event);
}

void
_gdk_orbital_display_convert_selection (GdkDisplay *display,
                                        GdkWindow  *requestor,
                                        GdkAtom     selection,
                                        GdkAtom     target,
                                        guint32     time)
{
  emit_empty_selection_notify (requestor, selection, target);
}

gint
_gdk_orbital_display_text_property_to_utf8_list (GdkDisplay    *display,
                                                 GdkAtom        encoding,
                                                 gint           format,
                                                 const guchar  *text,
                                                 gint           length,
                                                 gchar       ***list)
{
  GPtrArray *array;
  const gchar *ptr;
  gsize chunk_len;
  gchar *copy;
  guint nitems;

  ptr = (const gchar *) text;
  array = g_ptr_array_new ();

  while (ptr < (const gchar *) &text[length])
    {
      chunk_len = strlen (ptr);

      if (g_utf8_validate (ptr, chunk_len, NULL))
        {
          copy = g_strndup (ptr, chunk_len);
          g_ptr_array_add (array, copy);
        }

      ptr = &ptr[chunk_len + 1];
    }

  nitems = array->len;
  g_ptr_array_add (array, NULL);

  if (list)
    *list = (gchar **) g_ptr_array_free (array, FALSE);
  else
    g_ptr_array_free (array, TRUE);

  return nitems;
}

static gchar *
sanitize_utf8 (const gchar *src,
               gboolean return_latin1)
{
  gint len = strlen (src);
  GString *result = g_string_sized_new (len);
  const gchar *p = src;

  while (*p)
    {
      if (*p == '\r')
        {
          p++;
          if (*p == '\n')
            p++;

          g_string_append_c (result, '\n');
        }
      else
        {
          gunichar ch = g_utf8_get_char (p);

          if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
            {
              if (return_latin1)
                {
                  if (ch <= 0xff)
                    g_string_append_c (result, ch);
                  else
                    g_string_append_printf (result,
                                            ch < 0x10000 ? "\\u%04x" : "\\U%08x",
                                            ch);
                }
              else
                {
                  char buf[7];
                  gint buflen;

                  buflen = g_unichar_to_utf8 (ch, buf);
                  g_string_append_len (result, buf, buflen);
                }
            }

          p = g_utf8_next_char (p);
        }
    }

  return g_string_free (result, FALSE);
}

gchar *
_gdk_orbital_display_utf8_to_string_target (GdkDisplay  *display,
                                            const gchar *str)
{
  return sanitize_utf8 (str, TRUE);
}

void
gdk_orbital_selection_add_targets (GdkWindow *window,
                                   GdkAtom    selection,
                                   guint      ntargets,
                                   GdkAtom   *targets)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkOrbitalSelection *orbital_selection = gdk_orbital_display_get_selection (display);

  g_return_if_fail (GDK_IS_WINDOW (window));

  g_array_append_vals (orbital_selection->source_targets, targets, ntargets);
}

void
gdk_orbital_selection_clear_targets (GdkDisplay *display,
                                     GdkAtom     selection)
{
  GdkOrbitalSelection *orbital_selection = gdk_orbital_display_get_selection (display);

  g_array_set_size (orbital_selection->source_targets, 0);
}

gboolean
gdk_orbital_selection_set_current_offer_actions (GdkDisplay *display,
                                                 uint32_t    action)
{
  return FALSE; /* TODO */
}

void
gdk_orbital_selection_ensure_offer (GdkDisplay *display,
                                    gpointer    offer)
{
}

void
gdk_orbital_selection_ensure_primary_offer (GdkDisplay *display,
                                            gpointer    offer)
{
}

void
gdk_orbital_selection_set_offer (GdkDisplay *display,
                                 GdkAtom     selection,
                                 gpointer    offer)
{
}

gpointer
gdk_orbital_selection_get_offer (GdkDisplay *display,
                                 GdkAtom     selection)
{
  return NULL;
}

GList *
gdk_orbital_selection_get_targets (GdkDisplay *display,
                                   GdkAtom     selection)
{
  return NULL;
}

void
gdk_orbital_selection_store (GdkWindow    *window,
                             GdkAtom       type,
                             GdkPropMode   mode,
                             const guchar *data,
                             gint          len)
{
}

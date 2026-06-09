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

#include "gdkdndprivate.h"

#include "gdkmain.h"
#include "gdkinternals.h"
#include "gdkproperty.h"
#include "gdkprivate-orbital.h"
#include "gdkdisplay-orbital.h"
#include "gdkseat-orbital.h"
#include "gdkdeviceprivate.h"

#include <string.h>

#include "orbital.h"

#define GDK_TYPE_ORBITAL_DRAG_CONTEXT              (gdk_orbital_drag_context_get_type ())
#define GDK_ORBITAL_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_ORBITAL_DRAG_CONTEXT, GdkOrbitalDragContext))
#define GDK_ORBITAL_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_ORBITAL_DRAG_CONTEXT, GdkOrbitalDragContextClass))
#define GDK_IS_ORBITAL_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_ORBITAL_DRAG_CONTEXT))
#define GDK_IS_ORBITAL_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_ORBITAL_DRAG_CONTEXT))
#define GDK_ORBITAL_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_ORBITAL_DRAG_CONTEXT, GdkOrbitalDragContextClass))

typedef struct _GdkOrbitalDragContext GdkOrbitalDragContext;
typedef struct _GdkOrbitalDragContextClass GdkOrbitalDragContextClass;

struct _GdkOrbitalDragContext
{
  GdkDragContext context;
  GdkWindow *dnd_window;
  GdkDragAction selected_action;
  uint32_t serial;
  gdouble x;
  gdouble y;
  gint hot_x;
  gint hot_y;
};

struct _GdkOrbitalDragContextClass
{
  GdkDragContextClass parent_class;
};

static GList *contexts;

GType gdk_orbital_drag_context_get_type (void);

G_DEFINE_TYPE (GdkOrbitalDragContext, gdk_orbital_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_orbital_drag_context_finalize (GObject *object)
{
  GdkOrbitalDragContext *orbital_context = GDK_ORBITAL_DRAG_CONTEXT (object);
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkWindow *dnd_window;

  contexts = g_list_remove (contexts, context);

  if (context->is_source)
    gdk_drag_context_set_cursor (context, NULL);

  dnd_window = orbital_context->dnd_window;

  G_OBJECT_CLASS (gdk_orbital_drag_context_parent_class)->finalize (object);

  if (dnd_window)
    gdk_window_destroy (dnd_window);
}

void
_gdk_orbital_drag_context_emit_event (GdkDragContext *context,
                                      GdkEventType    type,
                                      guint32         time_)
{
  GdkWindow *window;
  GdkEvent *event;

  switch (type)
    {
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      break;
    default:
      return;
    }

  if (context->is_source)
    window = gdk_drag_context_get_source_window (context);
  else
    window = gdk_drag_context_get_dest_window (context);

  if (!window)
    return;

  event = gdk_event_new (type);
  event->dnd.window = g_object_ref (window);
  event->dnd.context = g_object_ref (context);
  event->dnd.time = time_;
  event->dnd.x_root = GDK_ORBITAL_DRAG_CONTEXT (context)->x;
  event->dnd.y_root = GDK_ORBITAL_DRAG_CONTEXT (context)->y;
  gdk_event_set_device (event, gdk_drag_context_get_device (context));

  gdk_event_put (event);
  gdk_event_free (event);
}

static GdkWindow *
gdk_orbital_drag_context_find_window (GdkDragContext  *context,
                                      GdkWindow       *drag_window,
                                      GdkScreen       *screen,
                                      gint             x_root,
                                      gint             y_root,
                                      GdkDragProtocol *protocol)
{
  GdkDevice *device;
  GdkWindow *window;

  device = gdk_drag_context_get_device (context);
  window = gdk_device_get_window_at_position (device, NULL, NULL);

  if (window)
    {
      window = gdk_window_get_toplevel (window);
      *protocol = GDK_DRAG_PROTO_ORBITAL;
      return g_object_ref (window);
    }

  return NULL;
}

void
gdk_orbital_drag_context_set_action (GdkDragContext *context,
                                     GdkDragAction   action)
{
  context->suggested_action = context->action = action;
}

static gboolean
gdk_orbital_drag_context_drag_motion (GdkDragContext *context,
                                      GdkWindow      *dest_window,
                                      GdkDragProtocol protocol,
                                      gint            x_root,
                                      gint            y_root,
                                      GdkDragAction   suggested_action,
                                      GdkDragAction   possible_actions,
                                      guint32         time)
{
  if (context->dest_window != dest_window)
    {
      context->dest_window = dest_window ? g_object_ref (dest_window) : NULL;
      _gdk_orbital_drag_context_set_coords (context, x_root, y_root);
      _gdk_orbital_drag_context_emit_event (context, GDK_DRAG_STATUS, time);
    }

  gdk_orbital_drag_context_set_action (context, suggested_action);

  return context->dest_window != NULL;
}

static void
gdk_orbital_drag_context_drag_abort (GdkDragContext *context,
                                     guint32         time)
{
}

static void
gdk_orbital_drag_context_drag_drop (GdkDragContext *context,
                                    guint32         time)
{
}

/* Destination side */

static void
gdk_orbital_drop_context_set_status (GdkDragContext *context,
                                     gboolean        accepted)
{
}

static void
gdk_orbital_drag_context_commit_status (GdkDragContext *context)
{
  GdkOrbitalDragContext *orbital_context = GDK_ORBITAL_DRAG_CONTEXT (context);
  gdk_orbital_drop_context_set_status (context, orbital_context->selected_action != 0);
}

static void
gdk_orbital_drag_context_drag_status (GdkDragContext *context,
                                      GdkDragAction   action,
                                      guint32         time_)
{
  GdkOrbitalDragContext *orbital_context = GDK_ORBITAL_DRAG_CONTEXT (context);
  orbital_context->selected_action = action;
}

static void
gdk_orbital_drag_context_drop_reply (GdkDragContext *context,
                                     gboolean        accepted,
                                     guint32         time_)
{
  if (!accepted)
    gdk_orbital_drop_context_set_status (context, accepted);
}

static void
gdk_orbital_drag_context_drop_finish (GdkDragContext *context,
                                      gboolean        success,
                                      guint32         time)
{
}

static gboolean
gdk_orbital_drag_context_drop_status (GdkDragContext *context)
{
  return FALSE;
}

static GdkAtom
gdk_orbital_drag_context_get_selection (GdkDragContext *context)
{
  return gdk_atom_intern_static_string ("GdkOrbitalSelection");
}

static void
gdk_orbital_drag_context_init (GdkOrbitalDragContext *context_orbital)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (context_orbital);
  
  contexts = g_list_prepend (contexts, context);

  context->action = GDK_ACTION_COPY;
  context->suggested_action = GDK_ACTION_COPY;
  context->actions = GDK_ACTION_COPY | GDK_ACTION_MOVE;
}

static GdkWindow *
gdk_orbital_drag_context_get_drag_window (GdkDragContext *context)
{
  return GDK_ORBITAL_DRAG_CONTEXT (context)->dnd_window;
}

static void
gdk_orbital_drag_context_set_hotspot (GdkDragContext *context,
                                      gint            hot_x,
                                      gint            hot_y)
{
  GdkOrbitalDragContext *context_orbital = GDK_ORBITAL_DRAG_CONTEXT (context);
  gint prev_hot_x = context_orbital->hot_x;
  gint prev_hot_y = context_orbital->hot_y;
  const GdkRectangle damage_rect = { .width = 1, .height = 1 };

  context_orbital->hot_x = hot_x;
  context_orbital->hot_y = hot_y;

  if (prev_hot_x == hot_x && prev_hot_y == hot_y)
    return;

  if (context_orbital->dnd_window)
    gdk_window_invalidate_rect (context_orbital->dnd_window, &damage_rect, FALSE);
}

static gboolean
gdk_orbital_drag_context_manage_dnd (GdkDragContext *context,
                                     GdkWindow      *ipc_window,
                                     GdkDragAction   actions)
{
  GdkDevice *device = gdk_drag_context_get_device (context);

  gdk_seat_ungrab (gdk_device_get_seat (device));
  return FALSE;
}

static void
gdk_orbital_drag_context_set_cursor (GdkDragContext *context,
                                     GdkCursor      *cursor)
{
  GdkDevice *device = gdk_drag_context_get_device (context);
  gdk_orbital_seat_set_global_cursor (gdk_device_get_seat (device), cursor);
}

static void
gdk_orbital_drag_context_action_changed (GdkDragContext *context,
                                         GdkDragAction   action)
{
  GdkCursor *cursor = gdk_drag_get_cursor (context, action);
  gdk_drag_context_set_cursor (context, cursor);
}

static void
gdk_orbital_drag_context_drop_performed (GdkDragContext *context,
                                         guint32         time_)
{
  gdk_drag_context_set_cursor (context, NULL);
}

static void
gdk_orbital_drag_context_cancel (GdkDragContext      *context,
                                 GdkDragCancelReason  reason)
{
  gdk_drag_context_set_cursor (context, NULL);
}

static void
gdk_orbital_drag_context_drop_done (GdkDragContext *context,
                                    gboolean        success)
{
  GdkOrbitalDragContext *context_orbital = GDK_ORBITAL_DRAG_CONTEXT (context);

  if (success)
    {
      if (context_orbital->dnd_window)
        gdk_window_hide (context_orbital->dnd_window);
    }
}

static void
gdk_orbital_drag_context_class_init (GdkOrbitalDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_orbital_drag_context_finalize;

  context_class->find_window = gdk_orbital_drag_context_find_window;
  context_class->drag_status = gdk_orbital_drag_context_drag_status;
  context_class->drag_motion = gdk_orbital_drag_context_drag_motion;
  context_class->drag_abort = gdk_orbital_drag_context_drag_abort;
  context_class->drag_drop = gdk_orbital_drag_context_drag_drop;
  context_class->drop_reply = gdk_orbital_drag_context_drop_reply;
  context_class->drop_finish = gdk_orbital_drag_context_drop_finish;
  context_class->drop_status = gdk_orbital_drag_context_drop_status;
  context_class->get_selection = gdk_orbital_drag_context_get_selection;
  context_class->get_drag_window = gdk_orbital_drag_context_get_drag_window;
  context_class->set_hotspot = gdk_orbital_drag_context_set_hotspot;
  context_class->drop_done = gdk_orbital_drag_context_drop_done;
  context_class->manage_dnd = gdk_orbital_drag_context_manage_dnd;
  context_class->set_cursor = gdk_orbital_drag_context_set_cursor;
  context_class->action_changed = gdk_orbital_drag_context_action_changed;
  context_class->drop_performed = gdk_orbital_drag_context_drop_performed;
  context_class->cancel = gdk_orbital_drag_context_cancel;
  context_class->commit_drag_status = gdk_orbital_drag_context_commit_status;
}

GdkDragProtocol
_gdk_orbital_window_get_drag_protocol (GdkWindow *window, GdkWindow **target)
{
  return GDK_DRAG_PROTO_ORBITAL;
}

void
_gdk_orbital_window_register_dnd (GdkWindow *window)
{
}

static GdkWindow *
create_dnd_window (GdkScreen *screen)
{
  GdkWindowAttr attrs;
  guint mask;

  attrs.x = attrs.y = 0;
  attrs.width = attrs.height = 100;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.window_type = GDK_WINDOW_TEMP;
  attrs.type_hint = GDK_WINDOW_TYPE_HINT_DND;
  attrs.visual = gdk_screen_get_system_visual (screen);

  mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_TYPE_HINT;

  return gdk_window_new (gdk_screen_get_root_window (screen), &attrs, mask);
}

GdkDragContext *
_gdk_orbital_window_drag_begin (GdkWindow *window,
                                GdkDevice *device,
                                GList     *targets,
                                gint       x_root,
                                gint       y_root)
{
  GdkOrbitalDragContext *context_orbital;
  GdkDragContext *context;

  context_orbital = g_object_new (GDK_TYPE_ORBITAL_DRAG_CONTEXT, NULL);
  context = GDK_DRAG_CONTEXT (context_orbital);
  
  context->display = gdk_window_get_display (window);
  context->source_window = g_object_ref (window);
  context->is_source = TRUE;
  context->targets = g_list_copy (targets);

  gdk_drag_context_set_device (context, device);

  context_orbital->dnd_window = create_dnd_window (gdk_window_get_screen (window));

  return context;
}

GdkDragContext * _gdk_orbital_drop_context_new (GdkDisplay *display,
                                                gpointer    data_device)
{
  GdkOrbitalDragContext *context_orbital;
  GdkDragContext *context;

  context_orbital = g_object_new (GDK_TYPE_ORBITAL_DRAG_CONTEXT, NULL);
  context = GDK_DRAG_CONTEXT (context_orbital);
  context->display = display;
  context->is_source = FALSE;

  return context;
}

void
gdk_orbital_drop_context_update_targets (GdkDragContext *context)
{
}

void
gdk_orbital_seat_set_global_cursor (GdkSeat *seat, GdkCursor *cursor)
{
}

void
_gdk_orbital_drag_context_set_coords (GdkDragContext *context,
                                      gdouble         x,
                                      gdouble         y)
{
  GdkOrbitalDragContext *context_orbital = GDK_ORBITAL_DRAG_CONTEXT (context);
  context_orbital->x = x;
  context_orbital->y = y;
}

void
_gdk_orbital_drag_context_set_source_window (GdkDragContext *context,
                                             GdkWindow      *window)
{
  if (context->source_window)
    g_object_unref (context->source_window);

  context->source_window = window ? g_object_ref (window) : NULL;
}

void
_gdk_orbital_drag_context_set_dest_window (GdkDragContext *context,
                                           GdkWindow      *dest_window,
                                           uint32_t        serial)
{
  if (context->dest_window)
    g_object_unref (context->dest_window);

  context->dest_window = dest_window ? g_object_ref (dest_window) : NULL;
  GDK_ORBITAL_DRAG_CONTEXT (context)->serial = serial;
}

GdkDragContext *
gdk_orbital_drag_context_lookup_by_source_window (GdkWindow *window)
{
  GList *l;

  for (l = contexts; l; l = l->next)
    {
      if (window == gdk_drag_context_get_source_window (l->data))
        return l->data;
    }

  return NULL;
}
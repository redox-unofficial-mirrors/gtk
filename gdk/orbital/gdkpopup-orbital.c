/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2026 Redox OS Contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

// these are helpers copied from wayland implementation, not exposed to public header

static void
calculate_popup_rect (GdkWindow    *window,
                      GdkGravity    rect_anchor,
                      GdkGravity    window_anchor,
                      GdkRectangle *out_rect)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  GdkRectangle anchor_rect;
  int x = 0, y = 0;

  anchor_rect = (GdkRectangle) {
    .x = (impl->pending_move_to_rect.rect.x +
          impl->pending_move_to_rect.rect_anchor_dx),
    .y = (impl->pending_move_to_rect.rect.y +
          impl->pending_move_to_rect.rect_anchor_dy),
    .width = impl->pending_move_to_rect.rect.width,
    .height = impl->pending_move_to_rect.rect.height
  };

  switch (rect_anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y;
      break;
    case GDK_GRAVITY_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_CENTER:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + (anchor_rect.height / 2);
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      x = anchor_rect.x;
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH:
      x = anchor_rect.x + (anchor_rect.width / 2);
      y = anchor_rect.y + anchor_rect.height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x = anchor_rect.x + anchor_rect.width;
      y = anchor_rect.y + anchor_rect.height;
      break;
    }

  switch (window_anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      break;
    case GDK_GRAVITY_NORTH:
      x -= window->width / 2;
      break;
    case GDK_GRAVITY_NORTH_EAST:
      x -= window->width;
      break;
    case GDK_GRAVITY_WEST:
      y -= window->height / 2;
      break;
    case GDK_GRAVITY_CENTER:
      x -= window->width / 2;
      y -= window->height / 2;
      break;
    case GDK_GRAVITY_EAST:
      x -= window->width;
      y -= window->height / 2;
      break;
    case GDK_GRAVITY_SOUTH_WEST:
      y -= window->height;
      break;
    case GDK_GRAVITY_SOUTH:
      x -= window->width / 2;
      y -= window->height;
      break;
    case GDK_GRAVITY_SOUTH_EAST:
      x -= window->width;
      y -= window->height;
      break;
    }

  *out_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = window->width,
    .height = window->height
  };
}

static GdkGravity
flip_anchor_horizontally (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_NORTH_EAST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_SOUTH_WEST;
    }

  g_assert_not_reached ();
}

static GdkGravity
flip_anchor_vertically (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return GDK_GRAVITY_SOUTH_WEST;
    case GDK_GRAVITY_NORTH:
      return GDK_GRAVITY_SOUTH;
    case GDK_GRAVITY_NORTH_EAST:
      return GDK_GRAVITY_SOUTH_EAST;
    case GDK_GRAVITY_WEST:
      return GDK_GRAVITY_WEST;
    case GDK_GRAVITY_CENTER:
      return GDK_GRAVITY_CENTER;
    case GDK_GRAVITY_EAST:
      return GDK_GRAVITY_EAST;
    case GDK_GRAVITY_SOUTH_WEST:
      return GDK_GRAVITY_NORTH_WEST;
    case GDK_GRAVITY_SOUTH:
      return GDK_GRAVITY_NORTH;
    case GDK_GRAVITY_SOUTH_EAST:
      return GDK_GRAVITY_NORTH_EAST;
    }

  g_assert_not_reached ();
}

static GdkWindow *
get_real_parent_and_translate (GdkWindow *window,
                               gint      *x,
                               gint      *y)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  GdkWindow *parent = impl->transient_for;

  while (parent)
    {
      GdkWindow *effective_parent = gdk_window_get_effective_parent (parent);

      if (parent == NULL || !effective_parent)
        break;

      *x += parent->x;
      *y += parent->y;

      parent = effective_parent;
    }

  return parent;
}

static void
calculate_moved_to_rect_result (GdkWindow    *window,
                                int           x,
                                int           y,
                                int           width,
                                int           height,
                                GdkRectangle *flipped_rect,
                                GdkRectangle *final_rect,
                                gboolean     *flipped_x,
                                gboolean     *flipped_y)
{
  GdkWindowImplOrbital *impl = GDK_WINDOW_IMPL_ORBITAL (window->impl);
  GdkRectangle best_rect;

  *final_rect = (GdkRectangle) {
    .x = x,
    .y = y,
    .width = width,
    .height = height,
  };

  calculate_popup_rect (window,
                        impl->pending_move_to_rect.rect_anchor,
                        impl->pending_move_to_rect.window_anchor,
                        &best_rect);

  *flipped_rect = best_rect;

  if (x != best_rect.x &&
      impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_X)
    {
      GdkRectangle flipped_x_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_window_anchor;

      flipped_rect_anchor =
        flip_anchor_horizontally (impl->pending_move_to_rect.rect_anchor);
      flipped_window_anchor =
        flip_anchor_horizontally (impl->pending_move_to_rect.window_anchor),
      calculate_popup_rect (window,
                            flipped_rect_anchor,
                            flipped_window_anchor,
                            &flipped_x_rect);

      if (flipped_x_rect.x == x)
        flipped_rect->x = x;
    }

  if (y != best_rect.y &&
      impl->pending_move_to_rect.anchor_hints & GDK_ANCHOR_FLIP_Y)
    {
      GdkRectangle flipped_y_rect;
      GdkGravity flipped_rect_anchor;
      GdkGravity flipped_window_anchor;

      flipped_rect_anchor =
        flip_anchor_vertically (impl->pending_move_to_rect.rect_anchor);
      flipped_window_anchor =
        flip_anchor_vertically (impl->pending_move_to_rect.window_anchor),
      calculate_popup_rect (window,
                            flipped_rect_anchor,
                            flipped_window_anchor,
                            &flipped_y_rect);

      if (flipped_y_rect.y == y)
        flipped_rect->y = y;
    }

  *flipped_x = flipped_rect->x != best_rect.x;
  *flipped_y = flipped_rect->y != best_rect.y;
}

static void
sanitize_anchor_rect (GdkWindow    *window,
                      GdkRectangle *rect)
{
  gint original_width = rect->width;
  gint original_height = rect->height;

  rect->width  = MAX (1, rect->width);
  rect->height = MAX (1, rect->height);
  rect->x = MAX (rect->x + original_width - rect->width, 0);
  rect->y = MAX (rect->y + original_height - rect->height, 0);
}

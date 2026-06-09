/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2013 Jan Arne Petersen
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

#ifndef __GDK_ORBITAL_DEVICE_H__
#define __GDK_ORBITAL_DEVICE_H__

#if !defined (__GDKORBITAL_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkorbital.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include <orbital.h>

G_BEGIN_DECLS

#ifdef GDK_COMPILATION
typedef struct _GdkOrbitalDevice GdkOrbitalDevice;
#else
typedef GdkDevice GdkOrbitalDevice;
#endif
typedef struct _GdkOrbitalDeviceClass GdkOrbitalDeviceClass;

#define GDK_TYPE_ORBITAL_DEVICE         (gdk_orbital_device_get_type ())
#define GDK_ORBITAL_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_ORBITAL_DEVICE, GdkOrbitalDevice))
#define GDK_ORBITAL_DEVICE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_ORBITAL_DEVICE, GdkOrbitalDeviceClass))
#define GDK_IS_ORBITAL_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_ORBITAL_DEVICE))
#define GDK_IS_ORBITAL_DEVICE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_ORBITAL_DEVICE))
#define GDK_ORBITAL_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_ORBITAL_DEVICE, GdkOrbitalDeviceClass))

GDK_AVAILABLE_IN_ALL
GType                gdk_orbital_device_get_type            (void);

GDK_AVAILABLE_IN_ALL
struct wl_seat      *gdk_orbital_device_get_wl_seat         (GdkDevice *device);
GDK_AVAILABLE_IN_ALL
struct wl_pointer   *gdk_orbital_device_get_wl_pointer      (GdkDevice *device);
GDK_AVAILABLE_IN_ALL
struct wl_keyboard  *gdk_orbital_device_get_wl_keyboard     (GdkDevice *device);

GDK_AVAILABLE_IN_3_20
struct wl_seat      *gdk_orbital_seat_get_wl_seat           (GdkSeat   *seat);

GDK_AVAILABLE_IN_3_22
const gchar         *gdk_orbital_device_get_node_path       (GdkDevice *device);

G_END_DECLS

#endif /* __GDK_ORBITAL_DEVICE_H__ */

/* GTK - The GIMP Toolkit
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gtkintl.h"
#include "gtkdevicegroup.h"

typedef struct GtkDeviceGroupPrivate GtkDeviceGroupPrivate;

struct GtkDeviceGroupPrivate
{
  GList *devices;
};

#define GTK_DEVICE_GROUP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_DEVICE_GROUP, GtkDeviceGroupPrivate))

static void gtk_device_group_class_init (GtkDeviceGroupClass *klass);
static void gtk_device_group_init       (GtkDeviceGroup      *group);

static void gtk_device_group_finalize   (GObject             *object);

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkDeviceGroup, gtk_device_group, G_TYPE_OBJECT)


static void
gtk_device_group_class_init (GtkDeviceGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_device_group_finalize;

  signals[DEVICE_ADDED] =
    g_signal_new (I_("device-added"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkDeviceGroupClass, device_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GDK_TYPE_DEVICE);
  signals[DEVICE_REMOVED] =
    g_signal_new (I_("device-removed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkDeviceGroupClass, device_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GDK_TYPE_DEVICE);

  g_type_class_add_private (object_class, sizeof (GtkDeviceGroupPrivate));
}

static void
gtk_device_group_init (GtkDeviceGroup *group)
{
}

static void
gtk_device_group_finalize (GObject *object)
{
  GtkDeviceGroupPrivate *priv;

  priv = GTK_DEVICE_GROUP_GET_PRIVATE (object);

  g_list_foreach (priv->devices, (GFunc) g_object_unref, NULL);
  g_list_free (priv->devices);

  G_OBJECT_CLASS (gtk_device_group_parent_class)->finalize (object);
}

void
gtk_device_group_add_device (GtkDeviceGroup *group,
                             GdkDevice      *device)
{
  GtkDeviceGroupPrivate *priv;

  g_return_if_fail (GTK_IS_DEVICE_GROUP (group));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = GTK_DEVICE_GROUP_GET_PRIVATE (group);

  if (g_list_find (priv->devices, device))
    return;

  priv->devices = g_list_prepend (priv->devices,
                                  g_object_ref (device));

  g_signal_emit (group, signals[DEVICE_ADDED], 0, device);
}

void
gtk_device_group_remove_device (GtkDeviceGroup *group,
                                GdkDevice      *device)
{
  GtkDeviceGroupPrivate *priv;
  GList *dev;

  g_return_if_fail (GTK_IS_DEVICE_GROUP (group));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = GTK_DEVICE_GROUP_GET_PRIVATE (group);

  dev = g_list_find (priv->devices, device);

  if (!dev)
    return;

  priv->devices = g_list_remove_link (priv->devices, dev);

  g_signal_emit (group, signals[DEVICE_REMOVED], 0, device);

  g_object_unref (dev->data);
  g_list_free_1 (dev);
}

GList *
gtk_device_group_get_devices (GtkDeviceGroup *group)
{
  GtkDeviceGroupPrivate *priv;

  g_return_val_if_fail (GTK_IS_DEVICE_GROUP (group), NULL);

  priv = GTK_DEVICE_GROUP_GET_PRIVATE (group);

  return priv->devices;
}

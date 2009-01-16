/*
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gtk/gtk.h>

typedef struct DeviceData DeviceData;
typedef struct Data Data;

struct Data
{
  GtkDeviceGroup *group;

  gdouble x;
  gdouble y;
  gdouble angle;
  gdouble distance;
};

static gboolean
enter_notify_cb (GtkWidget        *widget,
                 GdkEventCrossing *event,
                 gpointer          user_data)
{
  Data *data;
  GList *devices;

  data = (Data *) user_data;
  devices = gtk_device_group_get_devices (data->group);

  if (g_list_length (devices) < 2)
    gtk_device_group_add_device (data->group, event->device);
}

static gboolean
leave_notify_cb (GtkWidget        *widget,
                 GdkEventCrossing *event,
                 gpointer          user_data)
{
  Data *data;

  data = (Data *) user_data;

  gtk_device_group_remove_device (data->group, event->device);
}

static void
multidevice_cb (GtkWidget           *widget,
                GtkDeviceGroup      *group,
                GtkMultiDeviceEvent *event,
                gpointer             user_data)
{
  gint i;
  Data *data;

  data = user_data;

  if (event->n_events > 0)
    {
      data->x = event->events[0]->x;
      data->y = event->events[0]->y;
    }

  if (event->n_events > 1)
    {
      gdk_events_get_distance ((GdkEvent *) event->events[0],
                               (GdkEvent *) event->events[1],
                               &data->distance);
      gdk_events_get_angle ((GdkEvent *) event->events[0],
                            (GdkEvent *) event->events[1],
                            &data->angle);
    }
  else
    {
      data->distance = 0;
      data->angle = 0;
    }

  gtk_widget_queue_draw (widget);
}

static gboolean
expose_cb (GtkWidget      *widget,
           GdkEventExpose *event,
           gpointer        user_data)
{
  Data *data;
  cairo_t *cr;

  data = (Data *) user_data;
  cr = gdk_cairo_create (widget->window);

  cairo_translate (cr, data->x, data->y);

  cairo_save (cr);

  cairo_set_source_rgb (cr, 0., 0., 0.);
  cairo_move_to (cr, 0, 0);
  cairo_rel_line_to (cr, 1000, 0);
  cairo_stroke (cr);

  cairo_restore (cr);

  if (data->angle > 0)
    {
      cairo_save (cr);

      cairo_set_source_rgb (cr, 0, 0, 1);
      cairo_move_to (cr, 0, 0);

      cairo_arc (cr,
                 0.,
                 0.,
                 MAX (10., data->distance),
                 0.,
                 data->angle);

      cairo_close_path (cr);

      cairo_fill (cr);

      cairo_restore (cr);

      cairo_set_source_rgb (cr, 1., 0., 0.);
      cairo_rotate (cr, data->angle);
      cairo_move_to (cr, 0, 0);
      cairo_rel_line_to (cr, data->distance, 0);
      cairo_stroke (cr);
    }

  cairo_destroy (cr);

  return TRUE;
}

int
main (int argc, gchar *argv[])
{
  GtkWidget *window;
  GList *devices;
  Data data = { 0 };

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_app_paintable (window, TRUE);

  data.group = gtk_widget_create_device_group (window);

  gtk_widget_add_events (window,
                         (GDK_POINTER_MOTION_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK));

  gtk_widget_set_support_multidevice (window, TRUE);

  g_signal_connect (window, "enter-notify-event",
                    G_CALLBACK (enter_notify_cb), &data);
  g_signal_connect (window, "leave-notify-event",
                    G_CALLBACK (leave_notify_cb), &data);
  g_signal_connect (window, "multidevice-event",
                    G_CALLBACK (multidevice_cb), &data);
  g_signal_connect (window, "expose-event",
                    G_CALLBACK (expose_cb), &data);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}

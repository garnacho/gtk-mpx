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

#include "testphotoalbumwidget.h"
#include <math.h>

#define TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_PHOTO_ALBUM_WIDGET, TestPhotoAlbumWidgetPrivate))

typedef struct TestPhotoAlbumWidgetPrivate TestPhotoAlbumWidgetPrivate;
typedef struct TestPhoto TestPhoto;

struct TestPhoto
{
  GdkPixbuf *pixbuf;
  GtkDeviceGroup *group;
  gdouble center_x;
  gdouble center_y;
  gdouble x;
  gdouble y;
  gdouble angle;
  gdouble zoom;

  GdkRegion *region;

  gdouble base_zoom;
  gdouble base_angle;
  gdouble initial_distance;
  gdouble initial_angle;
};

struct TestPhotoAlbumWidgetPrivate
{
  GPtrArray *photos;
};

static GQuark quark_group_photo = 0;


static void test_photo_album_widget_class_init (TestPhotoAlbumWidgetClass *klass);
static void test_photo_album_widget_init       (TestPhotoAlbumWidget      *album);

static void test_photo_album_widget_destroy    (GtkObject          *object);

static gboolean test_photo_album_widget_button_press      (GtkWidget           *widget,
                                                           GdkEventButton      *event);
static gboolean test_photo_album_widget_button_release    (GtkWidget           *widget,
                                                           GdkEventButton      *event);
static void     test_photo_album_widget_multidevice_event (GtkWidget           *widget,
                                                           GtkDeviceGroup      *group,
                                                           GtkMultiDeviceEvent *event);
static gboolean test_photo_album_widget_expose            (GtkWidget           *widget,
                                                           GdkEventExpose      *event);

G_DEFINE_TYPE (TestPhotoAlbumWidget, test_photo_album_widget, GTK_TYPE_DRAWING_AREA)


static void
test_photo_album_widget_class_init (TestPhotoAlbumWidgetClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->destroy = test_photo_album_widget_destroy;

  widget_class->button_press_event = test_photo_album_widget_button_press;
  widget_class->button_release_event = test_photo_album_widget_button_release;
  widget_class->expose_event = test_photo_album_widget_expose;

  g_type_class_add_private (klass, sizeof (TestPhotoAlbumWidgetPrivate));

  quark_group_photo = g_quark_from_static_string ("group-photo");
}

static void
test_photo_album_widget_init (TestPhotoAlbumWidget *album)
{
  TestPhotoAlbumWidgetPrivate *priv;
  GtkWidget *widget;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);
  widget = GTK_WIDGET (album);

  priv->photos = g_ptr_array_new ();

  gtk_widget_add_events (widget,
                         (GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK));

  gtk_widget_set_support_multidevice (widget, TRUE);

  /* Multidevice events are not exposed through GtkWidgetClass */
  g_signal_connect (album, "multidevice-event",
                    G_CALLBACK (test_photo_album_widget_multidevice_event), NULL);
}

static void
calculate_rotated_point (gdouble  angle,
                         gdouble  zoom,
                         gdouble  center_x,
                         gdouble  center_y,
                         gdouble  point_x,
                         gdouble  point_y,
                         gdouble *ret_x,
                         gdouble *ret_y)
{
  gdouble distance, xd, yd, ang;

  xd = center_x - point_x;
  yd = center_y - point_y;

  if (xd == 0 && yd == 0)
    {
      *ret_x = center_x;
      *ret_y = center_y;
      return;
    }

  distance = sqrt ((xd * xd) + (yd * yd));
  distance *= zoom;

  ang = atan2 (xd, yd);

  /* Invert angle */
  ang = (2 * G_PI) - ang;

  /* Shift it 270° */
  ang += 3 * (G_PI / 2);

  /* And constraint it to 0°-360° */
  ang = fmod (ang, 2 * G_PI);
  ang += angle;

  *ret_x = center_x + (distance * cos (ang));
  *ret_y = center_y + (distance * sin (ang));
}

static void
allocate_photo_region (TestPhoto *photo)
{
  GdkRegion *region;
  GdkPoint points[4];
  gint width, height, i;

  width = gdk_pixbuf_get_width (photo->pixbuf);
  height = gdk_pixbuf_get_height (photo->pixbuf);

  /* Top/left */
  points[0].x = photo->x - photo->center_x;
  points[0].y = photo->y - photo->center_y;

  /* Top/right */
  points[1].x = photo->x - photo->center_x + width;
  points[1].y = photo->y - photo->center_y;

  /* Bottom/right */
  points[2].x = photo->x - photo->center_x + width;
  points[2].y = photo->y - photo->center_y + height;

  /* Bottom/left */
  points[3].x = photo->x - photo->center_x;
  points[3].y = photo->y - photo->center_y + height;

  for (i = 0; i < 4; i++)
    {
      gdouble ret_x, ret_y;

      calculate_rotated_point (photo->angle,
                               photo->zoom,
                               photo->x,
                               photo->y,
                               (gdouble) points[i].x,
                               (gdouble) points[i].y,
                               &ret_x,
                               &ret_y);

      points[i].x = (gint) ret_x;
      points[i].y = (gint) ret_y;
    }

  if (photo->region)
    gdk_region_destroy (photo->region);

  photo->region = gdk_region_polygon (points, 4, GDK_WINDING_RULE);
}

static TestPhoto *
test_photo_new (TestPhotoAlbumWidget *album,
                GdkPixbuf            *pixbuf)
{
  TestPhoto *photo;
  static gdouble angle = 0;

  photo = g_slice_new0 (TestPhoto);
  photo->pixbuf = g_object_ref (pixbuf);
  photo->group = gtk_widget_create_device_group (GTK_WIDGET (album));
  g_object_set_qdata (G_OBJECT (photo->group), quark_group_photo, photo);

  photo->center_x = 0;
  photo->center_y = 0;
  photo->x = 0;
  photo->y = 0;
  photo->angle = angle;
  photo->zoom = 1.0;

  angle += 0.1;

  allocate_photo_region (photo);

  return photo;
}

static void
test_photo_free (TestPhoto            *photo,
                 TestPhotoAlbumWidget *album)
{
  g_object_unref (photo->pixbuf);
  gtk_widget_remove_device_group (GTK_WIDGET (album), photo->group);

  g_slice_free (TestPhoto, photo);
}

static void
test_photo_raise (TestPhoto            *photo,
                  TestPhotoAlbumWidget *album)
{
  TestPhotoAlbumWidgetPrivate *priv;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);
  g_ptr_array_remove (priv->photos, photo);
  g_ptr_array_add (priv->photos, photo);
}

static void
test_photo_album_widget_destroy (GtkObject *object)
{
  TestPhotoAlbumWidgetPrivate *priv;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (object);

  if (priv->photos)
    {
      g_ptr_array_foreach (priv->photos, (GFunc) test_photo_free, object);
      g_ptr_array_free (priv->photos, TRUE);
      priv->photos = NULL;
    }

  GTK_OBJECT_CLASS (test_photo_album_widget_parent_class)->destroy (object);
}

static TestPhoto *
find_photo_at_position (TestPhotoAlbumWidget *album,
                        gdouble               x,
                        gdouble               y)
{
  TestPhotoAlbumWidgetPrivate *priv;
  TestPhoto *photo;
  gint i;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);

  for (i = priv->photos->len - 1; i >= 0; i--)
    {
      photo = g_ptr_array_index (priv->photos, i);

      if (gdk_region_point_in (photo->region, (gint) x, (gint) y))
        return photo;
    }

  return NULL;
}

static gboolean
test_photo_album_widget_button_press (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  TestPhoto *photo;

  photo = find_photo_at_position (TEST_PHOTO_ALBUM_WIDGET (widget), event->x, event->y);

  if (!photo)
    return FALSE;

  test_photo_raise (photo, TEST_PHOTO_ALBUM_WIDGET (widget));
  gtk_device_group_add_device (photo->group, event->device);

  return TRUE;
}

static gboolean
test_photo_album_widget_button_release (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  GtkDeviceGroup *group;

  group = gtk_widget_get_group_for_device (widget, event->device);

  if (group)
    gtk_device_group_remove_device (group, event->device);
}

static void
test_photo_album_widget_multidevice_event (GtkWidget           *widget,
                                           GtkDeviceGroup      *group,
                                           GtkMultiDeviceEvent *event)
{
  TestPhotoAlbumWidgetPrivate *priv;
  GdkRegion *region;
  TestPhoto *photo;
  gboolean new_center = FALSE;
  gboolean new_position = FALSE;
  gdouble event_x, event_y;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (widget);
  photo = g_object_get_qdata (G_OBJECT (group), quark_group_photo);

  region = gdk_region_copy (photo->region);

  if (event->n_events == 1)
    {
      if (event->type == GTK_EVENT_DEVICE_REMOVED)
        {
          /* Device was just removed, unset zoom/angle info */
          photo->base_zoom = 0;
          photo->base_angle = 0;
          photo->initial_angle = 0;
          photo->initial_distance = 0;
          new_center = TRUE;
        }
      else if (event->type == GTK_EVENT_DEVICE_ADDED)
        new_center = TRUE;

      event_x = event->events[0]->x;
      event_y = event->events[0]->y;
      new_position = TRUE;
    }
  else if (event->n_events == 2)
    {
      gdouble distance, angle;

      gdk_events_get_center ((GdkEvent *) event->events[0],
                             (GdkEvent *) event->events[1],
                             &event_x, &event_y);

      gdk_events_get_distance ((GdkEvent *) event->events[0],
                               (GdkEvent *) event->events[1],
                               &distance);

      gdk_events_get_angle ((GdkEvent *) event->events[0],
                            (GdkEvent *) event->events[1],
                            &angle);

      if (event->type == GTK_EVENT_DEVICE_ADDED)
        {
          photo->base_zoom = photo->zoom;
          photo->base_angle = photo->angle;
          photo->initial_angle = angle;
          photo->initial_distance = distance;
          new_center = TRUE;
        }

      photo->zoom = photo->base_zoom * (distance / photo->initial_distance);
      photo->angle = photo->base_angle + (angle - photo->initial_angle);
      new_position = TRUE;
    }

  if (new_center)
    {
      gdouble origin_x, origin_y;

      origin_x = photo->x - photo->center_x;
      origin_y = photo->y - photo->center_y;

      calculate_rotated_point (- photo->angle,
                               1 / photo->zoom,
                               photo->x - origin_x,
                               photo->y - origin_y,
                               event_x - origin_x,
                               event_y - origin_y,
                               &photo->center_x,
                               &photo->center_y);
    }

  if (new_position)
    {
      photo->x = event_x;
      photo->y = event_y;
    }

  allocate_photo_region (photo);

  gdk_region_union (region, photo->region);
  gdk_region_shrink (region, -4, -4);

  gdk_window_invalidate_region (widget->window, region, FALSE);
}

static gboolean
test_photo_album_widget_expose (GtkWidget      *widget,
                                GdkEventExpose *event)
{
  TestPhotoAlbumWidgetPrivate *priv;
  cairo_t *cr;
  gint i;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (widget);
  cr = gdk_cairo_create (widget->window);

  for (i = 0; i < priv->photos->len; i++)
    {
      TestPhoto *photo = g_ptr_array_index (priv->photos, i);
      GdkRegion *region;
      gint width, height;

      region = gdk_region_copy (photo->region);
      gdk_region_shrink (region, -4, -4);

      gdk_region_intersect (region, event->region);

      if (gdk_region_empty (region))
        {
          gdk_region_destroy (region);
          continue;
        }

      width = gdk_pixbuf_get_width (photo->pixbuf);
      height = gdk_pixbuf_get_height (photo->pixbuf);

      cairo_save (cr);

      gdk_cairo_region (cr, region);
      cairo_clip (cr);

      cairo_translate (cr, photo->x, photo->y);

      cairo_scale (cr, photo->zoom, photo->zoom);
      cairo_rotate (cr, photo->angle);
      gdk_cairo_set_source_pixbuf (cr,
                                   photo->pixbuf,
                                   - photo->center_x,
                                   - photo->center_y);

      cairo_rectangle (cr,
                       - photo->center_x,
                       - photo->center_y,
                       width, height);
      cairo_fill_preserve (cr);

      cairo_set_source_rgb (cr, 0., 0., 0.);
      cairo_stroke (cr);

      cairo_restore (cr);

      gdk_region_destroy (region);
    }

  cairo_destroy (cr);

  return TRUE;
}

GtkWidget *
test_photo_album_widget_new (void)
{
  return g_object_new (TEST_TYPE_PHOTO_ALBUM_WIDGET, NULL);
}

void
test_photo_album_widget_add_image (TestPhotoAlbumWidget *album,
                                   GdkPixbuf            *pixbuf)
{
  TestPhotoAlbumWidgetPrivate *priv;
  TestPhoto *photo;

  g_return_if_fail (TEST_IS_PHOTO_ALBUM_WIDGET (album));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);

  photo = test_photo_new (album, pixbuf);
  g_ptr_array_add (priv->photos, photo);

  if (GTK_WIDGET_REALIZED (album) &&
      GTK_WIDGET_DRAWABLE (album))
    gdk_window_invalidate_region (GTK_WIDGET (album)->window,
                                  photo->region,
                                  FALSE);
}

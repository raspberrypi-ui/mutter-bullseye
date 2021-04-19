#include <stdlib.h>
#include <clutter/clutter.h>
#include <math.h>
#include <gmodule.h>
#include <string.h>

#include "tests/clutter-test-utils.h"

#define N_FIREWORKS 32
/* Units per second per second */
#define GRAVITY -1.5f

#define N_SPARKS (N_FIREWORKS * 32) /* Must be a power of two */
#define TIME_PER_SPARK 0.01f /* in seconds */

#define TEXTURE_SIZE 32

typedef struct _Firework Firework;

struct _Firework
{
  float size;
  float x, y;
  float start_x, start_y;
  ClutterColor color;

  /* Velocities are in units per second */
  float initial_x_velocity;
  float initial_y_velocity;

  GTimer *timer;
};

typedef struct _Spark Spark;

struct _Spark
{
  float x, y;
  ClutterColor color;
  ClutterColor base_color;
};

typedef struct _Data Data;

struct _Data
{
  Firework fireworks[N_FIREWORKS];

  int next_spark_num;
  Spark sparks[N_SPARKS];
  GTimer *last_spark_time;

  CoglMaterial *material;
};

int
test_cogl_point_sprites_main (int argc, char *argv[]);

const char *
test_cogl_point_sprites_describe (void);

static CoglHandle
generate_round_texture (void)
{
  guint8 *p, *data;
  int x, y;
  CoglHandle tex;

  p = data = g_malloc (TEXTURE_SIZE * TEXTURE_SIZE * 4);

  /* Generate a yellow circle which gets transparent towards the edges */
  for (y = 0; y < TEXTURE_SIZE; y++)
    for (x = 0; x < TEXTURE_SIZE; x++)
      {
        int dx = x - TEXTURE_SIZE / 2;
        int dy = y - TEXTURE_SIZE / 2;
        float value = sqrtf (dx * dx + dy * dy) * 255.0 / (TEXTURE_SIZE / 2);
        if (value > 255.0f)
          value = 255.0f;
        value = 255.0f - value;
        *(p++) = value;
        *(p++) = value;
        *(p++) = value;
        *(p++) = value;
      }

  tex = cogl_texture_new_from_data (TEXTURE_SIZE, TEXTURE_SIZE,
                                    COGL_TEXTURE_NO_SLICING,
                                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                    COGL_PIXEL_FORMAT_ANY,
                                    TEXTURE_SIZE * 4,
                                    data);

  g_free (data);

  return tex;
}

static void
paint_cb (ClutterActor        *stage,
          ClutterPaintContext *paint_context,
          Data                *data)
{
  CoglFramebuffer *framebuffer =
    clutter_paint_context_get_framebuffer (paint_context);
  CoglMatrix old_matrix, new_matrix;
  int i;
  float diff_time;

  cogl_framebuffer_get_projection_matrix (framebuffer, &old_matrix);
  /* Use an orthogonal projection from -1 -> 1 in both axes */
  cogl_matrix_init_identity (&new_matrix);
  cogl_framebuffer_set_projection_matrix (framebuffer, &new_matrix);

  cogl_framebuffer_push_matrix (framebuffer);
  cogl_framebuffer_set_modelview_matrix (framebuffer, &new_matrix);

  /* Update all of the firework's positions */
  for (i = 0; i < N_FIREWORKS; i++)
    {
      Firework *firework = data->fireworks + i;

      if ((fabsf (firework->x - firework->start_x) > 2.0f) ||
          firework->y < -1.0f)
        {
          firework->size = g_random_double_range (0.001f, 0.1f);
          firework->start_x = 1.0f + firework->size;
          firework->start_y = -1.0f;
          firework->initial_x_velocity = g_random_double_range (-0.1f, -2.0f);
          firework->initial_y_velocity = g_random_double_range (0.1f, 4.0f);
          g_timer_reset (firework->timer);

          /* Pick a random color out of six */
          if (g_random_boolean ())
            {
              memset (&firework->color, 0, sizeof (ClutterColor));
              ((guint8 *) &firework->color)[g_random_int_range (0, 3)] = 255;
            }
          else
            {
              memset (&firework->color, 255, sizeof (ClutterColor));
              ((guint8 *) &firework->color)[g_random_int_range (0, 3)] = 0;
            }
          firework->color.alpha = 255;

          /* Fire some of the fireworks from the other side */
          if (g_random_boolean ())
            {
              firework->start_x = -firework->start_x;
              firework->initial_x_velocity = -firework->initial_x_velocity;
            }
        }

      diff_time = g_timer_elapsed (firework->timer, NULL);

      firework->x = (firework->start_x +
                     firework->initial_x_velocity * diff_time);

      firework->y = ((firework->initial_y_velocity * diff_time +
                      0.5f * GRAVITY * diff_time * diff_time) +
                     firework->start_y);
    }

  diff_time = g_timer_elapsed (data->last_spark_time, NULL);
  if (diff_time < 0.0f || diff_time >= TIME_PER_SPARK)
    {
      /* Add a new spark for each firework, overwriting the oldest ones */
      for (i = 0; i < N_FIREWORKS; i++)
        {
          Spark *spark = data->sparks + data->next_spark_num;
          Firework *firework = data->fireworks + i;

          spark->x = (firework->x +
                      g_random_double_range (-firework->size / 2.0f,
                                             firework->size / 2.0f));
          spark->y = (firework->y +
                      g_random_double_range (-firework->size / 2.0f,
                                             firework->size / 2.0f));
          spark->base_color = firework->color;

          data->next_spark_num = (data->next_spark_num + 1) & (N_SPARKS - 1);
        }

      /* Update the colour of each spark */
      for (i = 0; i < N_SPARKS; i++)
        {
          float color_value;

          /* First spark is the oldest */
          Spark *spark = data->sparks + ((data->next_spark_num + i)
                                         & (N_SPARKS - 1));

          color_value = i / (N_SPARKS - 1.0f);
          spark->color.red = spark->base_color.red * color_value;
          spark->color.green = spark->base_color.green * color_value;
          spark->color.blue = spark->base_color.blue * color_value;
          spark->color.alpha = 255.0f * color_value;
        }

      g_timer_reset (data->last_spark_time);
    }

  cogl_framebuffer_set_projection_matrix (framebuffer, &old_matrix);
  cogl_framebuffer_pop_matrix (framebuffer);
}

static gboolean
idle_cb (gpointer data)
{
  clutter_actor_queue_redraw (data);

  return G_SOURCE_CONTINUE;
}

G_MODULE_EXPORT int
test_cogl_point_sprites_main (int argc, char *argv[])
{
  ClutterActor *stage;
  CoglHandle tex;
  Data data;
  GError *error = NULL;
  int i;

  clutter_test_init (&argc, &argv);

  data.material = cogl_material_new ();
  data.last_spark_time = g_timer_new ();
  data.next_spark_num = 0;
  cogl_material_set_point_size (data.material, TEXTURE_SIZE);

  tex = generate_round_texture ();
  cogl_material_set_layer (data.material, 0, tex);
  cogl_object_unref (tex);

  if (!cogl_material_set_layer_point_sprite_coords_enabled (data.material,
                                                            0, TRUE,
                                                            &error))
    {
      g_warning ("Failed to enable point sprite coords: %s", error->message);
      g_clear_error (&error);
    }

  for (i = 0; i < N_FIREWORKS; i++)
    {
      data.fireworks[i].x = -FLT_MAX;
      data.fireworks[i].y = FLT_MAX;
      data.fireworks[i].size = 0.0f;
      data.fireworks[i].timer = g_timer_new ();
    }

  for (i = 0; i < N_SPARKS; i++)
    {
      data.sparks[i].x = 2.0f;
      data.sparks[i].y = 2.0f;
    }

  stage = clutter_test_get_stage ();
  clutter_actor_set_background_color (CLUTTER_ACTOR (stage), CLUTTER_COLOR_Black);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cogl Point Sprites");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_test_quit), NULL);
  g_signal_connect_after (stage, "paint", G_CALLBACK (paint_cb), &data);

  clutter_actor_show (stage);

  clutter_threads_add_idle (idle_cb, stage);

  clutter_test_main ();

  cogl_object_unref (data.material);
  g_timer_destroy (data.last_spark_time);

  for (i = 0; i < N_FIREWORKS; i++)
    g_timer_destroy (data.fireworks[i].timer);

  return 0;
}

G_MODULE_EXPORT const char *
test_cogl_point_sprites_describe (void)
{
  return "Point sprites support in Cogl.";
}

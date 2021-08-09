#ifndef _CUSTOM_PAYLOAD_H_
#define _CUSTOM_PAYLOAD_H_

#include <gst/gst.h>

typedef struct 
{
    // Object meta
    gint source_id;
    gint frame_id;
    gint object_id;
    gint count;
    gchar *object_class;
    // Bbox params
    float left;
    float top;
    float width;
    float height;
    // Classifier meta
    gchar *person_gender;
    gchar *person_age;
    gchar *car_color;
    gchar *car_type;

    // Analytics meta
    const gchar *lc_name;
    const gchar *roi_name;
    gint lc_id;
    gint roi_id;

} CustomPayloadMeta;

typedef struct
{
  gint camera_id;
  gint frame_init;
  gint frame_fin;
  gint freq;
  gint analytic;  // 0->flujo   1->aforo
  gint obj_type;  // 0->persona 1->auto  2->??
  gchar *line_id;
  gchar *count;

} LCPayloadMeta;

typedef struct
{
  gint camera_id;
  gint frame_init;
  gint frame_fin;
  gint freq;
  gint analytic; // 0->flujo    1->aforo
  gint obj_type; // 0->persona  1->auto  2->??
  gchar *roi_id;
  gchar *avg;
  gchar *min_count;
  gchar *max_count;

} ROIPayloadMeta;
#endif
